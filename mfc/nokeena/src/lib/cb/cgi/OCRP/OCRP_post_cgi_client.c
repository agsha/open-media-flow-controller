/*
 * OCRP_post_cgi_client.c -- Access interfaces for OCRP cgi input files.
 */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ftw.h>

#include "cb/OCRP_cgi_params.h"
#include "cb/OCRP_post_cgi_client.h"

#define UNUSED_ARGUMENT(x) (void)x

static OCRP_post_flist_t *g_flist;

static int
ftw_callback(const char *fpath, const struct stat *sb, int typeflag)
{
    UNUSED_ARGUMENT(sb);

    char *p;
    int allocsize;
    OCRP_file_type_t ftype = OCRP_FT_UNDEFINED;

    while (1) { // Begin while

    if (typeflag == FTW_F) {
	p = strstr(fpath, (POST_BASE_DATAFILE POST_SCRIPT_NAME_LOAD));
	if (p == fpath) {
	    ftype = OCRP_FT_LOAD;
	    break;
	} 

	p = strstr(fpath, (POST_BASE_DATAFILE POST_SCRIPT_NAME_UPDATE));
	if (p == fpath) {
	    ftype = OCRP_FT_UPDATE;
	    break;
	} 
    }
    break;

    } // End while

    if (ftype != OCRP_FT_UNDEFINED) {
    	if (g_flist->entries >= g_flist->maxentries) {
	   allocsize =  sizeof(OCRP_post_flist_t) + 
		    (((2 * g_flist->maxentries)-1) * sizeof(OCRP_post_file_t));
	   p = realloc(g_flist, allocsize);
	   if (p) {
	   	g_flist = (OCRP_post_flist_t *)p;
	   	g_flist->maxentries *= 2;
	   } else {
	   	return 100; // realloc failed, abort tree walk
	   }
	} 

	g_flist->files[g_flist->entries].type = ftype;
	p = strchr(fpath, '.');
	if (p) {
	    g_flist->files[g_flist->entries].seqno = atol(p+1);
	    g_flist->entries++;
	} else {
	    // Bad filename, no ".<seqno>", ignore entry
	}
    }
    return 0;
}

static  int
qsort_flist_cmp(const void *p1, const void *p2)
{
    OCRP_post_file_t *pf1 = (OCRP_post_file_t *)p1;
    OCRP_post_file_t *pf2 = (OCRP_post_file_t *)p2;

    if (pf1->seqno > pf2->seqno) {
    	return 1;
    } else if (pf1->seqno <  pf2->seqno) {
    	return -1;
    } else {
    	return 0;
    }
}

/*
 * get_OCRP_post_filelist() -- Get OCRP request files ordered in ascending
 *			       "seqno" order.
 * File name format: 
 *	POST-{load,delete}.<seqno>
 *
 * Return:
 *   ==0, Success
 *   !=0, Error
 *
 * 1) Caller frees the returned (OCRP_post_flist_t *) via free().
 * 2) Caller serializes access to this interface (ie. not reentrant).
 */
OCRP_post_flist_t *
get_OCRP_post_filelist(int *status)
{
    int rv;
    OCRP_post_flist_t *flist;
    int allocsize;

    allocsize = sizeof(OCRP_post_flist_t) + 
		((DEFAULT_POST_FLIST_SZ-1) * sizeof(OCRP_post_file_t));
    flist = (OCRP_post_flist_t *)calloc(1, allocsize);
    if (!flist) {
	*status = 1;
    	return 0;
    }
    flist->maxentries = DEFAULT_POST_FLIST_SZ;
    if (g_flist) {
    	free(g_flist);
    }
    g_flist = flist;

    rv = ftw(OCRP_BASE_DIR, ftw_callback, 8);
    if (rv) {
    	free(g_flist);
    	g_flist = 0;
    	*status = rv;
	return 0;
    }

    // Sort flist in ascending "seqno" order.
    qsort(g_flist->files, g_flist->entries, sizeof(g_flist->files[0]), 
    	  qsort_flist_cmp);

    flist = g_flist;
    g_flist = 0;

    *status = 0;
    return flist;
}

/*
 * OCRP_post_file2filename -- Convert OCRP_post_file_t to an absolute pathname
 *			      string.
 * Return:
 *   ==0, Success
 *   !=0, Error
 */
int
OCRP_post_file2filename(const OCRP_post_file_t *file, 
			char *filename, int sizeof_filename)
{
    int rv;
    
    rv = snprintf(filename, sizeof_filename, "%s%s.%ld",
    		  POST_BASE_DATAFILE, 
		  (file->type == OCRP_FT_LOAD ? 
		  	POST_SCRIPT_NAME_LOAD : POST_SCRIPT_NAME_UPDATE),
		  file->seqno);
    if (rv < sizeof_filename) {
    	return 0;
    } else {
    	return 1;
    }
}

/*
 * update_OCRP_state() -- Update OCRP protocol state information.
 *
 * Return:
 *   ==0, Success
 *   !=0, Error
 */
int
update_OCRP_state(const char *data)
{
    char buf[2048];
    size_t sz;
    ssize_t bytes_written;
    int ret = 0;
    int fd = -1;
    int rv;

    if (!data) {
    	return 0;
    }

    while (1) { // Begin while

    rv = snprintf(buf, sizeof(buf), "%s\r\n", data);
    if (rv >= (int)sizeof(buf)) {
    	ret = 1; // buf to small
	break;
    }
    sz = rv;

    fd = open(GET_STATUS_CONTENT_FILE, O_RDWR|O_CREAT);
    if (fd < 0) {
    	ret = 2; // open failed
	break;
    }

    ret = flock(fd, LOCK_EX);
    if (ret < 0) {
    	ret = 3; // flock exclusive failed
	break;
    }

    ret = ftruncate(fd, 0);
    if (ret) {
    	ret = 4; // truncate failed
	break;
    }

    bytes_written = write(fd, buf, sz);
    if (bytes_written != (ssize_t)sz) {
    	ftruncate(fd, 0);
	ret = 5;
	break;
    }
    
    break;

    } // End while

    if (fd >= 0) {
    	close(fd);
    }
    return ret;
}

/*
 * End of OCRP_post_cgi_client.c
 */

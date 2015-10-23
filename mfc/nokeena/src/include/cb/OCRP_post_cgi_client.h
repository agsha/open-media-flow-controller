/*
 * OCRP_post_cgi_client.h -- Access interfaces for OCRP cgi input data.
 */

#ifndef _OCRP_POST_CGI_CLIENT_H
#define _OCRP_POST_CGI_CLIENT_H

#define DEFAULT_POST_FLIST_SZ 1000 // more then enough

typedef enum OCRP_file_type {
    OCRP_FT_UNDEFINED=0,
    OCRP_FT_LOAD,
    OCRP_FT_UPDATE,
} OCRP_file_type_t;

typedef struct OCRP_post_file {
    OCRP_file_type_t type;
    long seqno;
} OCRP_post_file_t;

typedef struct OCRP_post_flist {
    int entries;
    int maxentries;
    OCRP_post_file_t files[1];
} OCRP_post_flist_t;

/*
 * get_OCRP_post_filelist() -- Get OCRP request files ordered in ascending
 *			       "seqno" order.
 * Return:
 *   ==0, Success
 *   !=0, Error
 *
 * 1) Caller frees the returned (OCRP_post_flist_t *) via free().
 * 2) Caller serializes access to this interface (ie. not reentrant).
 */
OCRP_post_flist_t *
get_OCRP_post_filelist(int *status);

/*
 * OCRP_post_file2filename -- Convert OCRP_post_file_t to an absolute pathname
 *			      string.
 * Return:
 *   ==0, Success
 *   !=0, Error
 */
int
OCRP_post_file2filename(const OCRP_post_file_t *file,
                        char *filename, int sizeof_filename);

/*
 * update_OCRP_state() -- Update OCRP protocol state information.
 *
 * Return:
 *   ==0, Success
 *   !=0, Error
 */
int
update_OCRP_state(const char *data);

#endif /* _OCRP_POST_CGI_CLIENT_H */

/*
 * End of OCRP_post_cgi_client.h
 */

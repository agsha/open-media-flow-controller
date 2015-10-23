
/*
 * fqueue.c -- File based queue
 *
 *	Simple file based queue where fixed size queue elements of
 *	FQUEUE_ELEMENTSIZE bytes are used.
 *	
 *	Enqueue:
 *	1) open(queuefile)
 *	2) flock(queuefile, LOCK_EX)
 *
 *	3) lseek(queuefile, 0, SEEK_END)
 *	4) write(queuefile, element, sizeof(element))
 *	5) fsync(queuefile)
 *
 *	6) flock(queuefile, LOCK_UN)
 *	7) close(queuefile)
 *
 *	Dequeue:
 *	1) open(queuefile)
 *	2) flock(queuefile, LOCK_EX)
 *
 *	3) fstat(queuefile, statbuf)
 *	4) read(queuefile, hdr, sizeof(hdr))
 *	5) if ((hdr.u.h.head * FQUEUE_ELEMENTSIZE) >= statbuf.st_size)
 *	       No data, return -1
 *	6) lseek(queuefile, hdr.u.h.head * FQUEUE_ELEMENTSIZE, SEEK_SET)
 *	7) read(queuefile, element, sizeof(element))
 *	8) if (((++hdr.u.h.head * FQUEUE_ELEMENTSIZE) >= statbuf.st_size) &&
 *	       (hdr.u.h.head >= truncate_target))
 *	       truncation_required = 1
 *	       hdr.u.h.head = 1
 *	9) lseek(queuefile, 0, SEEK_SET)
 *	10) write(queuefile, hdr, sizeof(hdr))
 *	11) if (truncation_required) ftruncate(queuefile, sizeof(hdr))
 *	12) fsync(queuefile)
 *
 *	13) flock(queuefile, LOCK_UN)
 *	14) close(queuefile)
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <time.h>

#include "nkn_debug.h"
#include "fqueue.h"

#define DBG(_fmt, ...) DBG_LOG(MSG, MOD_FQUEUE, _fmt, __VA_ARGS__)

#if 0
#define STATIC static
#else
#define STATIC
#endif

#if 0
#define FSYNC(_fd) fsync((_fd))
#else
#define FSYNC(_fd) 0
#endif

fqueue_header_t init_hdr;
fqueue_element_t init_qelement;
static int init_complete = 0;
static unsigned int queue_truncate_target = 1;

STATIC void init_fqueue(void);
STATIC u_int32_t compute_cksum(const char *p, int len);
STATIC int read_qheader(int q_fd, fqueue_header_t *hdr);
STATIC int write_qheader(int q_fd, fqueue_header_t *hdr);
STATIC int open_lock(const char *filename, int passed_fd, int *ret_fd);
STATIC int unlock_close(int fd, int no_close);
STATIC int add_queue_element(const char *queuefilename, int passed_fd,
		const fqueue_element_t *element);
STATIC int int_dequeue_fqueue(const char *queuefilename, int passed_fd,
		const char *outputfilename, fqueue_element_t *qe,
		struct stat *qf_statdata);
STATIC int read_qelement(int q_fd, off_t read_off, fqueue_element_t *ele);
STATIC int write_qelement(int q_fd, off_t write_off, fqueue_element_t *ele, 
		int no_cksum_compute);
STATIC int getfilestatus(const char *queuefilename, int fd, struct stat *filestatus);
STATIC int int_enqueue_fqueue(const char *queuefilename, int fd, 
		const char *inputfilename);
STATIC int int_enqueue_remove(fqueue_element_t *qe, const char *queuefilename, 
		const char *uri, const char * namespace_domain,
		int is_object_fullpath);
STATIC int pre_dequeue_fqueue(const char *queuefilename, int fd, 
		const char *outputfilename, fqueue_element_t *qe, int wait_secs);

STATIC 
void init_fqueue(void)
{
    if (!init_complete) {
    	init_hdr.u.h.magic = FQUEUE_HDR_MAGICNO;
    	init_hdr.u.h.cksum = 0;
    	init_hdr.u.h.max_entries = 0;
    	init_hdr.u.h.head = 1;
    	init_complete = 1;
    }
}

STATIC 
u_int32_t compute_cksum(const char *p, int len) 
{
    int n;
    u_int32_t cksum = 0;

    for (n = 0; n < len; n++) {
    	cksum += p[n];
	if (cksum & 1) {
	    cksum = (cksum >> 1) + 0x8000;
	} else {
	    cksum = (cksum >> 1);
	}
    	cksum += p[n];
	cksum &= 0xffff;
    }
    return cksum;
}

STATIC
int read_qheader(int q_fd, fqueue_header_t *hdr)
{
    int rv = 0;
    off_t off;
    ssize_t bytes;
    u_int32_t computed_cksum;
    u_int32_t given_cksum;

    off = lseek(q_fd, 0, SEEK_SET);
    if (off == -1) {
    	DBG("lseek(fd=%ds) failed errno=%d", q_fd, errno);
	rv = 1;
	goto exit;
    }

    // Read queue file header
    bytes = read(q_fd, (void *)hdr, sizeof(*hdr));
    if (bytes != sizeof(*hdr)) {
    	DBG("read(fd=%d) failed, requested=%lu actual=%lu",
	    q_fd, sizeof(*hdr), bytes);
	rv = 2;
	goto exit;
    }

    if (hdr->u.h.magic != FQUEUE_HDR_MAGICNO) {
    	DBG("corrupted queue header magic, fd=%d read=0x%x expected=0x%x",
	    q_fd, hdr->u.h.magic, FQUEUE_HDR_MAGICNO);
	rv = 3;
	goto exit;
    }

    // Verify checksum
    given_cksum = hdr->u.h.cksum;
    hdr->u.h.cksum = 0;
    computed_cksum = compute_cksum((char *)hdr, sizeof(*hdr));
    if (computed_cksum == given_cksum) {
    	hdr->u.h.cksum = given_cksum;
    } else {
    	DBG("invalid header checksum, fd=%d read=0x%x expected=0x%x",
	    q_fd, hdr->u.h.cksum, computed_cksum);
	rv = 4;
	goto exit;
    }

exit:

    return rv;
}

STATIC
int write_qheader(int q_fd, fqueue_header_t *hdr)
{
    int rv = 0;
    off_t off;
    ssize_t bytes;
    u_int32_t computed_cksum;

    off = lseek(q_fd, 0, SEEK_SET);
    if (off == -1) {
    	DBG("lseek(fd=%ds) failed errno=%d", q_fd, errno);
	rv = 1;
	goto exit;
    }

    // Compute checksum
    hdr->u.h.cksum = 0;
    computed_cksum = compute_cksum((char *)hdr, sizeof(*hdr));
    hdr->u.h.cksum = computed_cksum;

    bytes = write(q_fd, (void *)hdr, sizeof(*hdr));
    if (bytes != sizeof(*hdr)) {
    	DBG("write(fd=%d) failed, written=%lu actual=%lu errno=%d",
	    q_fd, sizeof(*hdr), bytes, errno);
	rv = 2;
	goto exit;
    }

exit:

    return rv;
}

STATIC
int read_qelement(int q_fd, off_t read_off, fqueue_element_t *ele)
{
    int rv = 0;
    off_t off;
    ssize_t bytes;
    u_int32_t given_cksum;
    u_int32_t computed_cksum;

    off = lseek(q_fd, read_off, SEEK_SET);
    if (off == -1) {
    	DBG("lseek(fd=%ds) failed errno=%d", q_fd, errno);
	rv = 1;
	goto exit;
    }

    // Read queue element
    bytes = read(q_fd, (void *)ele, sizeof(*ele));
    if (bytes != sizeof(*ele)) {
    	DBG("read(fd=%d) failed, requested=%lu actual=%lu",
	    q_fd, sizeof(*ele), bytes);
	rv = 2;
	goto exit;
    }

    if (ele->u.e.magic != FQUEUE_ELEMENT_MAGICNO) {
    	DBG("corrupted queue element magic, "
	    "fd=%d read=0x%x expected=0x%x", q_fd, ele->u.e.magic, 
	    FQUEUE_ELEMENT_MAGICNO);
	rv = 3;
	goto exit;
    }

    // Verify checksum
    given_cksum = ele->u.e.cksum;
    ele->u.e.cksum = 0;
    computed_cksum = compute_cksum((char *)ele, sizeof(*ele));
    if (computed_cksum == given_cksum) {
    	ele->u.e.cksum = given_cksum;
    } else {
    	DBG("invalid element checksum, fd=%d read=0x%x expected=0x%x",
	    q_fd, ele->u.e.cksum, computed_cksum);
	rv = 4;
	goto exit;
    }

exit:

    return rv;
}

STATIC
int write_qelement(int q_fd, off_t write_off, fqueue_element_t *ele, 
		int no_cksum_compute)
{
    int rv = 0;
    off_t off;
    ssize_t bytes;
    u_int32_t computed_cksum;

    off = lseek(q_fd, write_off, SEEK_SET);
    if (off == -1) {
    	DBG("lseek(fd=%ds) failed errno=%d", q_fd, errno);
	rv = 1;
	goto exit;
    }

    if (!no_cksum_compute) {
    	ele->u.e.cksum = 0;
    	computed_cksum = compute_cksum((char *)ele, sizeof(*ele));
    	ele->u.e.cksum = computed_cksum;
    }

    // Write queue element
    bytes = write(q_fd, (void *)ele, sizeof(*ele));
    if (bytes != sizeof(*ele)) {
    	DBG("write(fd=%d) failed, requested=%lu actual=%lu",
	    q_fd, sizeof(*ele), bytes);
	rv = 2;
	goto exit;
    }

exit:

    return rv;
}

STATIC 
int open_lock(const char *filename, int passed_fd, int *ret_fd)
{
    int rv;
    int fd;

    fd = passed_fd >= 0 ? passed_fd : open(filename, O_RDWR);
    if (fd >= 0) {
	rv = flock(fd, LOCK_EX);
	if (!rv) {
	    *ret_fd = fd;
	} else {
    	    DBG("flock(fd=%d, f=%s) failed errno=%d", fd, filename, errno);
	    close(fd);
	    rv = 1;
	}
    } else {
    	DBG("open(%s) failed errno=%d", filename, errno);
    	rv = 2;
    }
    return rv;
}

STATIC 
int unlock_close(int fd, int no_close)
{
    int rv;

    rv = flock(fd, LOCK_UN);
    if (rv) {
	DBG("flock(fd=%d) failed errno=%d", fd, errno);
    	rv = 1;
    }
    if (!no_close) {
    	close(fd);
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => Queue limit exceeded
 */
STATIC
int add_queue_element(const char *queuefilename, int passed_fd,
		const fqueue_element_t *element)
{
    int rv;
    int q_fd = -1;
    ssize_t bytes;
    off_t off;
    fqueue_header_t hdr;
    struct stat statdata;
    unsigned int blks_used;

    rv = open_lock(queuefilename, passed_fd, &q_fd);
    if (!rv) {
    	rv = fstat(q_fd, &statdata);
   	if (rv) {
    	    DBG("fstat(fd=%d) failed errno=%d", q_fd, errno);
	    rv = 1;
	    goto exit;
    	}

	rv = read_qheader(q_fd, &hdr);
	if (rv) {
    	    DBG("read_qheader(fd=%d) failed rv=%d", q_fd, rv);
	    rv = 2;
	    goto exit;
    	}

	blks_used = (statdata.st_size/FQUEUE_ELEMENTSIZE) - hdr.u.h.head;
	if (blks_used >= hdr.u.h.max_entries) {
	    rv = -1; // Queue limit exceeded
	    goto exit;
	}

    	off = lseek(q_fd, 0, SEEK_END);
	if (off == -1) {
	    DBG("lseek(fd=%d, f=%s) failed errno=%d", q_fd, queuefilename, 
	    	errno);
	    rv = 3;
	    goto exit;
	}

    	bytes = write(q_fd, (void *)element, sizeof(*element));
	if (bytes == sizeof(*element)) {
	    rv = FSYNC(q_fd);
	    if (rv) {
            	DBG("FSYNC(fd=%d, f=%s) failed errno=%d",
	    	    q_fd, queuefilename, errno);
		rv = 4;
	    }
	} else {
            DBG("write(fd=%d, f=%s) failed sent=%lu actual=%lu errno=%d",
	    	q_fd, queuefilename, sizeof(*element), bytes, errno);
	    rv = 5;
	}
    } else {
        DBG("open_lock(%s) failed rv=%d", queuefilename, rv);
     	rv = 6;
    }

exit:

    if (q_fd >= 0) {
    	unlock_close(q_fd, (passed_fd >= 0));
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => No elements available
 */
STATIC
int int_dequeue_fqueue(const char *queuefilename, int passed_fd,
		const char *outputfilename, fqueue_element_t *qe,
		struct stat *qf_statdata)
{
    int rv;
    int dest_fd = -1;
    int q_fd = -1;
    fqueue_header_t hdr;
    fqueue_element_t element;
    ssize_t bytes;
    struct stat statdata;
    int truncate_qfile = 0;
    int reset_fqueue = 0;

    if (!init_complete) {
    	init_fqueue();
    }

    if (!qe) {
    	// Open destination data file
    	dest_fd = open(outputfilename, O_CREAT|O_RDWR|O_TRUNC, 0666);
    	if (dest_fd < 0) {
    	    DBG("open(%s) failed errno=%d", outputfilename, errno);
    	    rv = 1;
	    goto exit;
    	}
    }

    // Open and lock queue
    rv = open_lock(queuefilename, passed_fd, &q_fd);
    if (rv) {
    	DBG("open_lock(%s) failed rv=%d", queuefilename, rv);
	rv = 2;
	goto exit;
    }

    // Determine queue file size
    rv = fstat(q_fd, &statdata);
    if (rv) {
    	DBG("fstat(fd=%d, f=%s) failed errno=%d", q_fd, queuefilename, errno);
	rv = 3;
	goto exit;
    }
    if (qf_statdata) {
    	*qf_statdata = statdata;
    }

    rv = read_qheader(q_fd, &hdr);
    if (rv) {
    	DBG("read_qheader(fd=%d, f=%s) failed rv=%d", q_fd, queuefilename, rv);
	rv = 4;
	goto exit;
    }

    if ((hdr.u.h.head * FQUEUE_ELEMENTSIZE) >= statdata.st_size) {
    	/* No data available */
	rv = -1;
	goto exit;
    }

    rv = read_qelement(q_fd, hdr.u.h.head * FQUEUE_ELEMENTSIZE, 
    		qe ? qe : &element);
    if (rv) {
    	DBG("read_element(fd=%d, f=%s) failed rv=%d, resetting fqueue", 
	    q_fd, queuefilename, rv);

	// Inconsistent fqueue, reset it.
	reset_fqueue = 1;
	rv = 5;
	goto exit;
    }

    if (!qe) {
    	// Write queue element to destination file
    	bytes = write(dest_fd, (void *)&element, sizeof(element));
    	if (bytes != sizeof(element)) {
    	    DBG("write(fd=%d, f=%s) failed, written=%lu actual=%lu "
	    	"errno=%d", q_fd, 
		outputfilename, sizeof(element), bytes, errno);
	    rv = 6;
	    goto exit;
    	}

    	rv = FSYNC(dest_fd);
    	if (!rv) {
    	    close(dest_fd);
    	    dest_fd = -1;
    	} else {
    	    DBG("FSYNC(fd=%d, f=%s) failed, errno=%d",
	    	dest_fd, outputfilename, errno);
	    rv = 7;
	    goto exit;
    	}
    }

    // Determine if queue truncation is required
    if (((++hdr.u.h.head * FQUEUE_ELEMENTSIZE) >= statdata.st_size) &&
	(+hdr.u.h.head >= queue_truncate_target)) {
	truncate_qfile = 1;
	hdr.u.h.head = 1;
    }

    rv = write_qheader(q_fd, &hdr);
    if (rv) {
    	DBG("write_qheader(fd=%d, f=%s) failed, rv=%d",
	    q_fd, queuefilename, rv);
	rv = 8;
	goto exit;
    }

    if (truncate_qfile) {
    	rv = ftruncate(q_fd, sizeof(hdr));
	if (rv) {
    	    DBG("ftruncate(fd=%d, f=%s) failed errno=%d",
	    	q_fd, queuefilename, errno);
	    rv = 9;
	    goto exit;
	}
    }

    rv = FSYNC(q_fd);
    if (rv) {
    	DBG("FSYNC(fd=%d, f=%s) failed, errno=%d", q_fd, queuefilename, errno);
	rv = 10;
    }

exit:

    if (reset_fqueue) {
    	int ret;
	hdr.u.h.head = 1;
    	ret = write_qheader(q_fd, &hdr);
	if (ret) {
	    DBG("Reset - write_qheader(fd=%d, f=%s) failed, rv=%d",
	    	q_fd, queuefilename, ret);
	}
    	ret = ftruncate(q_fd, sizeof(hdr));
	if (ret) {
    	    DBG("Reset - ftruncate(fd=%d, f=%s) failed errno=%d",
	    	q_fd, queuefilename, errno);
	}
    	ret = FSYNC(q_fd);
	if (ret) {
	    DBG("Reset - FSYNC(fd=%d, f=%s) failed, errno=%d", 
	    	q_fd, queuefilename, errno);
	}
    }

    if (q_fd >= 0) {
    	unlock_close(q_fd, (passed_fd >= 0));
    }

    if (dest_fd >= 0) {
    	close(dest_fd);
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
STATIC
int getfilestatus(const char *queuefilename, int fd, struct stat *filestatus)
{
    int rv;
    struct stat statdata;

    if (fd >= 0) {
       	rv = fstat(fd, &statdata);
    } else {
       	rv = lstat(queuefilename, &statdata);
    }
    if (!rv) {
    	if (filestatus) {
	    *filestatus = statdata;
	}
    } else {
    	DBG("%sstat(f=%s) failed errno=%d",
	    fd >= 0 ? "f" : "l", queuefilename, errno);
	rv = 1;
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => Queue max limit exceeded
 */
STATIC
int int_enqueue_fqueue(const char *queuefilename, int fd, 
		const char *inputfilename)
{
    int rv;
    int src_fd;
    fqueue_element_t element;
    ssize_t bytes;

    if (!init_complete) {
    	init_fqueue();
    }

    src_fd = open(inputfilename, O_RDONLY);
    if (src_fd >= 0) {
        bytes = read(src_fd, (void *)&element, sizeof(element));
	if (bytes == sizeof(element)) {
	    rv = add_queue_element(queuefilename, fd, &element);
	    if (rv) {
		DBG("add_queue_element(%s) failed rv=%d", queuefilename, rv);
		if (rv != -1) {
	    	    rv = 1;
		}
	    }
	} else {
	    rv = 2;
	}
	close(src_fd);
    } else {
    	DBG("open(%s) failed errno=%d", queuefilename, errno);
        rv = 3;
    }
    return rv;
}

/*
 *  Return:
 *      > 0  => Error
 *        0  => Success
 *       -1  => Queue max limit exceeded
 */
STATIC
int int_enqueue_remove(fqueue_element_t *qe, const char *queuefilename, 
		const char *uri, const char * namespace_domain,
		int is_object_fullpath)
{
    int rv;
    fqueue_element_t l_qe;

    if (!init_complete) {
        init_fqueue();
    }

    if (!qe) {
    	qe = &l_qe;

	/* if object_full_path is not null that that takes preference
	 * and when it is not null uri would be NULL as we are not 
	 * sending both at the same time
	 */
    	rv = init_queue_element(qe, 1, "/tmp/junk", uri);
    	if (rv) {
            DBG("init_queue_element(%s) failed rv=%d", queuefilename, rv);
	    rv = 1;
	    return rv;
    	}
    }

    rv = add_attr_queue_element_len(qe, "remove_object", 13, "1", 1);
    if (rv) {
        DBG("add_attr_queue_element_len(%s) failed rv=%d", queuefilename, rv);
	rv = 2;
	return rv;
    }

    /* If object_full_path then that has to be sent */
    if (is_object_fullpath)
    	rv = add_attr_queue_element_len(qe, "object_full_path", 16, 
    				"1", 1);
    else
    	rv = add_attr_queue_element_len(qe, "namespace_domain", 16, 
    				namespace_domain, strlen(namespace_domain));
    if (rv) {
        DBG("add_attr_queue_element_len(%s) failed rv=%d", queuefilename, rv);
	rv = 4;
	return rv;
    }

    /*
     * enqueue the element
     */
    rv = add_queue_element(queuefilename, -1, qe);
    if (rv) {
        DBG("add_queue_element(%s) failed rv=%d", queuefilename, rv);
        if (rv != -1) {
           rv = 3;
        }
    } 
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => No elements available
 */
STATIC
int pre_dequeue_fqueue(const char *queuefilename, int fd, 
		const char *outputfilename, fqueue_element_t *qe, int wait_secs)
{
    int rv, ret;
    struct stat qfile_stat;
    struct stat cur_qfile_stat;
    int delay = wait_secs * 2; // translate to half second intervals
    struct timespec req, rem;
    int terminate = 0;
    int try_dequeue;

    while (1) {
    	try_dequeue = 0;
        rv = int_dequeue_fqueue(queuefilename, fd, outputfilename, qe, 
			&qfile_stat);
	if (rv == -1) {
	    while (delay) {
	    	req.tv_sec = 0;
	    	req.tv_nsec = 500000000; /* half second */
		if (!nanosleep(&req, &rem)) {
		    delay--;
		} else {
		    // Sleep interrrupted
		    terminate = 1;
		    break;
		}
		ret = getfilestatus(queuefilename, fd, &cur_qfile_stat);
		if (!ret) {
		    if ((qfile_stat.st_size != cur_qfile_stat.st_size) ||
		        (qfile_stat.st_mtime != cur_qfile_stat.st_mtime) ||
		        (qfile_stat.st_ctime != cur_qfile_stat.st_ctime)) {
			try_dequeue = 1;
		    	break;
		    }
		} else {
		    terminate = 1;
		    break;
		}
	    }

	    if ((!delay && !try_dequeue) || terminate) {
	    	break;
	    }
	} else {
	    break;
	}
    }
    return rv;
}

/*
 *******************************************************************************
 *              P U B L I C  F U N C T I O N S
 *******************************************************************************
 */

/*
 *  Return:
 *	!=0  => Error
 *	  0  => Success
 */
int create_fqueue(const char *queuefilename, int max_entries)
{
    int fd;
    ssize_t bytes_written;
    int rv = 0;
    fqueue_header_t hdr;

    if (!init_complete) {
    	init_fqueue();
    }
    hdr = init_hdr;
    hdr.u.h.max_entries = max_entries ? 
    	max_entries : FQUEUE_HDR_MAXENTRIES_DEFAULT;
    hdr.u.h.cksum = compute_cksum((char *) &hdr, sizeof(hdr));

    fd = open(queuefilename, O_CREAT|O_RDWR|O_TRUNC, 0666);
    if (fd >= 0) {
	bytes_written = write(fd, (void *)&hdr, sizeof(hdr));
	if (bytes_written == sizeof(hdr)) {
	    rv = 0;
	} else {
	    DBG("write(%s) failed, sent=%lu written=%lu errno=%d", 
	    	queuefilename, sizeof(hdr), bytes_written, errno);
	    rv = 1;
	}
	close(fd);
    } else {
    	DBG("open(%s) failed rv=%d", queuefilename, errno);
        rv = 2;
    }
    return rv;
}

/*
 *  Return:
 *	!=0  => Error
 *	  0  => Success
 */
int create_recover_fqueue(const char *queuefilename, int max_entries, 
			  int delete_old_entries)
{
    int fd = -1;
    fqueue_header_t qh;
    fqueue_element_t qe;
    struct stat statdata;
    int rv = 0;
    int elements_to_remove;
    const char *attrname;
    int attrname_len;
    const char *fname;
    int fname_len;
    char filename[FQUEUE_ELEMENTSIZE+1];

    fd = open(queuefilename, O_RDWR, 0666);
    if (fd >= 0) {
    	rv = get_fqueue_header_fh(fd, &qh);
	if (rv) {
	    DBG("get_fqueue_header_fh() failed, rv=%d", rv);
	    rv = 1;
	    goto exit;
	}
	if (!delete_old_entries) {
	    goto exit; // Success
	}
	// Delete elements
    	rv = fstat(fd, &statdata);
	if (!rv) {
	    elements_to_remove = (statdata.st_size / FQUEUE_ELEMENTSIZE) - 1;

	    // Subsequent elements added after 'elements_to_remove'
	    // will be processed by the current incarnation of nvsd.
	    // Any errors in the delete with the exception of the 
	    // unlink() are assumed to be due to a corrupted queue,
	    // resulting in an abort of the delete action followed by
	    // a truncation of the queue file; data files will persist 
	    // for the unprocessed entries which were lost in the truncate.
	    //       

	    while (elements_to_remove > 0) {
		rv = dequeue_fqueue_fh_el(fd, &qe, 1);
		if (!rv) {
		    elements_to_remove--;
		    if (!qe.u.e.no_delete_datafile) {
		    	rv = get_attr_fqueue_element(&qe, T_ATTR_PATHNAME, 0,
						&attrname, &attrname_len,
						&fname, &fname_len);
			if (!rv && fname_len) {
			    memcpy(filename, fname, fname_len);
			    filename[fname_len] = '\0';
			    unlink(filename);
			    DBG("Deleted: %s", filename);
			}
		    }
		} else if (rv > 0) {
		    DBG("dequeue_fqueue_fh_el() failed, rv=%d", rv);
		    rv = 2;
		    goto exit;
		}
	    }
	    rv = 0; // Success
	}
    } else {
    	// File does not exist
	DBG("File (%s) does not exist", queuefilename);
    	rv = 4;
    }

exit:
    if (fd >= 0) {
    	close(fd);
    }

    if (rv) {
    	return create_fqueue(queuefilename, max_entries);
    }
    return rv;
}

/*
 *  Return:
 *	>=0  => Success
 *	 <0  => Error
 */
fhandle_t open_queue_fqueue(const char *queuefilename)
{
    fhandle_t fd;
    fd = open(queuefilename, O_RDWR);
    if (fd >= 0) {
    	return fd;
    } else {
    	DBG("open(%s) failed rv=%d", queuefilename, errno);
    	return -1;
    }
}

/*
 *  Return:
 *	0  => Success
 *	0  => Error
 */
int close_queue_fqueue(fhandle_t fh)
{
    close(fh);
    return 0;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => Queue limit exceeded
 */
int enqueue_fqueue(const char *queuefilename, const char *inputfilename)
{
    return int_enqueue_fqueue(queuefilename, -1, inputfilename);
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => Queue limit exceeded
 */
int enqueue_fqueue_fh(fhandle_t queuefile_fh, const char *inputfilename)
{
    return int_enqueue_fqueue(0, queuefile_fh, inputfilename);
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => Queue limit exceeded
 */
int enqueue_fqueue_fh_el(fhandle_t queuefile_fh, fqueue_element_t *qe)
{
    int rv;

    rv = add_queue_element(0, queuefile_fh, qe);
    if (rv) {
	DBG("add_queue_element(fd=%d) failed rv=%d", queuefile_fh, rv);
	rv = 1;
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => No elements available
 */
int dequeue_fqueue(const char *queuefilename, const char *outputfilename, 
		int wait_secs)
{
    return pre_dequeue_fqueue(queuefilename, -1, outputfilename, 0, wait_secs);
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => No elements available
 */
int dequeue_fqueue_fh(fhandle_t fd, const char *outputfilename, int wait_secs)
{
    return pre_dequeue_fqueue(0, fd, outputfilename, 0, wait_secs);
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 *	 -1  => No elements available
 */
int dequeue_fqueue_fh_el(fhandle_t fd, fqueue_element_t *el, int wait_secs)
{
    return pre_dequeue_fqueue(0, fd, 0, el, wait_secs);
}

/*
 *  Return:
 *      > 0  => Error
 *        0  => Success
 *       -1  => No elements available
 */
int enqueue_remove(fqueue_element_t *qe, const char *queuefilename, 
		const char *uri, const char * namespace_domain,
		int is_object_fullpath)
{
    return int_enqueue_remove(qe, queuefilename, uri, 
    				namespace_domain, is_object_fullpath);
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int init_queue_element(fqueue_element_t *qe, int no_delete_datafile,
		const char *datafilepathname, const char *uri)
{
    int rv = 0;
    int datafilepathname_len;
    int uri_len;
    int req_bytes;
    attr_record_t *pattrec;

    if (!qe || !datafilepathname || !uri) {
    	DBG("invalid input, qe=%p datafilepathname=%p uri=%p", 
	    qe, datafilepathname, uri);
	return 1;
    }
    *qe = init_qelement;
    datafilepathname_len = strlen(datafilepathname);
    uri_len = strlen(uri);

    req_bytes = (2 * sizeof(pattrec->u.d)) - 2 + datafilepathname_len + uri_len;
    if (req_bytes > FQUEUE_ELEMENT_ATTRSZ) {
    	DBG("request exceeds storage limit, req=%d avail=%d",
    	    req_bytes, FQUEUE_ELEMENT_ATTRSZ);
	return 2;
    }

    qe->u.e.magic = FQUEUE_ELEMENT_MAGICNO;
    qe->u.e.cksum = 0;
    qe->u.e.attr_bytes_used = req_bytes;
    qe->u.e.no_delete_datafile = no_delete_datafile;
    qe->u.e.num_attrs = 2;

    pattrec = (attr_record_t *)qe->u.e.attrs;

    pattrec->u.d.type = T_ATTR_PATHNAME;
    pattrec->u.d.len = datafilepathname_len;
    memcpy((void *)D_DATA(pattrec), (void *)datafilepathname, pattrec->u.d.len);

    pattrec = (attr_record_t *)
    	((char *)pattrec + sizeof(pattrec->u.d) + pattrec->u.d.len - 1);

    pattrec->u.d.type = T_ATTR_URI;
    pattrec->u.d.len = uri_len;
    memcpy((void *)D_DATA(pattrec), (void *)uri, pattrec->u.d.len);

    qe->u.e.cksum = compute_cksum((char *)qe, sizeof(*qe));

    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int init_queue_element_fn(const char *outputfilename, int no_delete_datafile,
		const char *datafilepathname, const char *uri)
{
    int rv;
    int fd;
    fqueue_element_t qe;

    rv = init_queue_element(&qe, no_delete_datafile, datafilepathname, uri);
    if (!rv) {
    	fd = open(outputfilename, O_CREAT|O_RDWR|O_TRUNC, 0666);
    	if (fd >= 0) {
	    rv = write_qelement(fd, 0, &qe, 1);
	    if (rv) {
	    	DBG("write_qelement(f=%s) failed, rv=%d", outputfilename, rv);
	        rv = 1;
	    }
	    close(fd);
    	}
    } else {
    	DBG("init_queue_element() failed file=%s rv=%d", outputfilename, rv);
	rv = 2;
    }

    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int add_attr_queue_element_len(fqueue_element_t *qe, const char *name, 
			int name_len, const char *value, int value_len)
{
    int req_bytes;
    attr_record_t *pattrec;

    if (!qe || !name || !value) {
    	DBG("invalid input, qe=%p name=%p value=%p", qe, name, value);
	return 1;
    }
    req_bytes = sizeof(pattrec->u.nv) - 1 + name_len + value_len;
    if ((qe->u.e.attr_bytes_used + req_bytes) > FQUEUE_ELEMENT_ATTRSZ) {
    	DBG("request exceeds storage limit, req=%d avail=%d",
    	    req_bytes, FQUEUE_ELEMENT_ATTRSZ);
	return 2;
    }

    qe->u.e.cksum = 0;
    qe->u.e.num_attrs++;

    pattrec = (attr_record_t *)&qe->u.e.attrs[qe->u.e.attr_bytes_used];
    pattrec->u.nv.type = T_ATTR_NAMEVALUE;
    pattrec->u.nv.namelen = name_len;
    pattrec->u.nv.valuelen = value_len;
    qe->u.e.attr_bytes_used += req_bytes;

    memcpy((void *)NV_NAME(pattrec), (void *)name, name_len);
    memcpy((void *)NV_VALUE(pattrec), (void *)value, value_len);

    qe->u.e.cksum = compute_cksum((char *)qe, sizeof(*qe));

    return 0;
}

int add_attr_queue_element(fqueue_element_t *qe, const char *name, 
			const char *value)
{
    int name_len;
    int value_len;

    if (!qe || !name || !value) {
    	DBG("invalid input, qe=%p name=%p value=%p", qe, name, value);
	return 1;
    }
    name_len = strlen(name);
    value_len = strlen(value);

    return add_attr_queue_element_len(qe, name, name_len, value, value_len);
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int add_attr_queue_element_fn(const char *inputfilename, const char *name, 
			const char *value)
{
    int rv;
    int fd = -1;
    fqueue_element_t qe;

    if (!inputfilename || !name || !value) {
    	DBG("invalid input, inputfilename=%p name=%p value=%p",
    	    inputfilename, name, value);
	rv = 1;
	goto exit;
    }
    fd = open(inputfilename, O_RDWR);
    if (fd >= 0) {
	rv = read_qelement(fd, 0, &qe);
	if (rv) {
	    DBG("read_qelement(f=%s) failed, rv=%d", inputfilename, rv);
	    rv = 2;
	    goto exit;
	}
	rv = add_attr_queue_element(&qe, name, value);
	if (!rv) {
	    rv = write_qelement(fd, 0, &qe, 1);
	    if (rv) {
	        DBG("write_qelement(f=%s) failed, rv=%d", inputfilename, rv);
	    	rv = 3;
	    	goto exit;
	    }
	} else {
	    DBG("add_attr_queue_element() failed rv=%d, "
	    	"inputfilename=%s name=%s value=%s",
		rv, inputfilename, name, value);
	    rv = 4;
	    goto exit;
	}
    }

exit:

    if (fd >= 0) {
    	close(fd);
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_fqueue_header(const char *inputfilename, fqueue_header_t *qh)
{
    int rv;
    int fd;

    if (!inputfilename) {
    	DBG("invalid input, inputfilename=%p", inputfilename);
	rv = 1;
	goto exit;
    }

    fd = open(inputfilename, O_RDONLY);
    if (fd >= 0) {
	rv = read_qheader(fd, qh);
	if (rv) {
    	    DBG("read_qheader() failed, rv=%d inputfilename=%s",
    	        rv, inputfilename);
	    rv = 2;
	}
	close(fd);
    } else {
    	DBG("open(%s) failed errno=%d", inputfilename, errno);
	rv = 3;
    }

exit:
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_fqueue_header_fh(fhandle_t fh, fqueue_header_t *qh)
{
    int rv = 0;

    if (!qh) {
    	DBG("invalid input, qh=%p", qh);
	rv = 1;
	goto exit;
    }
    rv = read_qheader(fh, qh);
    if (rv) {
	DBG("read_qheader(fd=%d) failed, rv=%d", fh, rv);
	rv = 2;
    }
exit:
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_fqueue_element(const char *inputfilename, fqueue_element_t *el)
{
    int rv;
    int fd;

    if (!inputfilename) {
    	DBG("invalid input, inputfilename=%p", inputfilename);
	rv = 1;
	goto exit;
    }

    fd = open(inputfilename, O_RDONLY);
    if (fd >= 0) {
	rv = read_qelement(fd, 0, el);
	if (rv) {
    	    DBG("read_qelement() failed, rv=%d inputfilename=%s",
    	        rv, inputfilename);
	    rv = 2;
	}
	close(fd);
    } else {
    	DBG("open(%s) failed errno=%d", inputfilename, errno);
	rv = 3;
    }

exit:
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_fqueue_element_fh(fhandle_t fh, off_t off, fqueue_element_t *el)
{
    int rv;

    if (!el) {
    	DBG("invalid input, el=%p", el);
	rv = 1;
	goto exit;
    }

    rv = read_qelement(fh, off, el);
    if (rv) {
	DBG("read_qelement(fd=%d) failed, rv=%d", fh, rv);
	rv = 2;
    }

exit:
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_attr_fqueue_element(const fqueue_element_t *qe, attr_type_t type,
		int nth_attr, /* 0 base, if (type == T_ATTR_NAMEVALUE) */
		const char **name, int *namelen,
		const char **data, int *datalen)
{
    int rv = 0;
    int n;
    attr_record_t *pattrec;

    if (!qe || !data || !datalen) {
    	DBG("invalid input, qe=%p data=%p datalen=%p", qe, data, datalen);
	return 1;
    }

    pattrec = (attr_record_t *)qe->u.e.attrs;

    switch(type) {
    case T_ATTR_PATHNAME:
	*name = "Pathname";
	*namelen = 8;
    	*data = D_DATA(pattrec);
    	*datalen = pattrec->u.d.len;
	break;

    case T_ATTR_URI:
    	pattrec = (attr_record_t *)((char *)pattrec + 
			sizeof(pattrec->u.d) + pattrec->u.d.len - 1);
	*name = "Uri";
	*namelen = 3;
    	*data = D_DATA(pattrec);
    	*datalen = pattrec->u.d.len;
	break;

    case T_ATTR_NAMEVALUE:
	if (nth_attr + 2 >= qe->u.e.num_attrs) {
#if 0
    	    DBG("invalid nth_attr=%d num_attrs=%hhd",
	    	nth_attr, qe->u.e.num_attrs);
#endif
	    rv = 1;
	    break;
	}
    	pattrec = (attr_record_t *)((char *)pattrec + 
			sizeof(pattrec->u.d) + pattrec->u.d.len - 1);
    	pattrec = (attr_record_t *)((char *)pattrec + 
			sizeof(pattrec->u.d) + pattrec->u.d.len - 1);

	for (n = 0; n < nth_attr; n++) {
	    pattrec = (attr_record_t *)((char *)pattrec + 
			    sizeof(pattrec->u.nv) - 1 + 
			    pattrec->u.nv.namelen + pattrec->u.nv.valuelen);
	}
	*name = NV_NAME(pattrec);
	*namelen = pattrec->u.nv.namelen;
	*data = NV_VALUE(pattrec);
	*datalen = pattrec->u.nv.valuelen;
	break;

    default:
	rv = 2;
	break;
    }
    return rv;
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_nvattr_fqueue_element_by_name(const fqueue_element_t *qe,
				const char *name, int namelen,
				const char **data, int *datalen)
{
    int rv;
    int nth;
    const char *t_name;
    int t_namelen;
    const char *t_data;
    int t_datalen;

    nth = 0;
    while ((rv = get_attr_fqueue_element(qe, T_ATTR_NAMEVALUE, nth, 
			&t_name, &t_namelen, &t_data, &t_datalen)) == 0) {
	if ((namelen == t_namelen) && 
		(strncasecmp(name, t_name, namelen) == 0)) {
	    *data = t_data;
	    *datalen = t_datalen;
	    return 0; // Found
	}
	nth++;
    }
    return 1; // Not found
}

/*
 *  Return:
 *	> 0  => Error
 *	  0  => Success
 */
int get_attr_fqueue_element_fn(const char *inputfilename, attr_type_t type,
		int nth_attr, /* 0 base, if (type == T_ATTR_NAMEVALUE) */
		const char **name, int *namelen,
		const char **data, int *datalen)
{
    int rv;
    int fd;
    fqueue_element_t qe;

    if (!inputfilename) {
    	DBG("invalid input, inputfilename=%p", inputfilename);
	rv = 1;
	goto exit;
    }

    fd = open(inputfilename, O_RDONLY);
    if (fd >= 0) {
    	rv = read_qelement(fd, 0, &qe);
	if (rv) {
	    rv = get_attr_fqueue_element(&qe, type, nth_attr, name, namelen,
	    				data, datalen);
	    if (!rv) {
	    	DBG("get_attr_queue_element(f=%s) failed, rv=%d", 
		    inputfilename, rv);
		rv = 2;
	    }
	} else {
	    DBG("read_qelement(f=%s) failed, rv=%d", inputfilename, rv);
	    rv = 3;
	}
	close(fd);
    } else {
	DBG("open(f=%s) failed, errno=%d", inputfilename, errno);
    	rv = 4;
    }
exit:
    return rv;
}

/*
 * End of fqueue.c
 */

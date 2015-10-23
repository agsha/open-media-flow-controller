/*
 * fqueue.h -- File based queue
 */

#ifndef _FQUEUE_H_
#define _FQUEUE_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Incremented the magic by 1 on each byte for 512 bytes URI support */

#define FQUEUE_ELEMENTSIZE		(16*512)
#define FQUEUE_HDR_MAGICNO		0x232538b9
#define FQUEUE_HDR_MAGICNO_V1		0x121427a8
#define FQUEUE_HDR_MAXENTRIES_DEFAULT	2048
#define FQUEUE_ELEMENT_MAGICNO		0x2cbe0bcd
#define FQUEUE_ELEMENT_MAGICNO_V1	0x1badfabc
#define FQUEUE_ELEMENT_ATTRSZ		8180

typedef struct fqueue_header {
    union {
	struct hdr {
	    u_int32_t magic;
	    u_int32_t cksum;
	    u_int32_t max_entries;
	    u_int32_t head;
	} h;
	struct data {
	    char d[FQUEUE_ELEMENTSIZE];
	} d;
    } u;
} fqueue_header_t;

typedef enum {
	T_ATTR_PATHNAME=1,
	T_ATTR_URI,
	T_ATTR_NAMEVALUE
} attr_type_t;

typedef struct attr_record {
    union {
	struct data_len {
	    u_int16_t len;
	    u_int8_t type; /* attr_type_t */
	    char data[1];
#define D_DATA(_p) (char *)((_p)->u.d.data)
	} d;
	struct nameval {
	    u_int8_t type; /* attr_type_t */
	    u_int8_t namelen;
	    u_int16_t valuelen;
	    char data[1];
#define NV_NAME(_p) (char *)((_p)->u.nv.data)
#define NV_VALUE(_p) (char *)&((_p)->u.nv.data[(_p)->u.nv.namelen])
	} nv;
    } u;
} attr_record_t;

typedef struct fqueue_element {
    union {
	struct element {
	    u_int32_t   magic;
	    u_int32_t	cksum;
	    u_int16_t	attr_bytes_used;
	    u_int8_t	no_delete_datafile;
	    u_int8_t	num_attrs;
	    /**
	     ** attr_record_t data
	     **   - 1st attribute T_ATTR_PATHNAME (data file pathname)
	     **   - 2nd attribute T_ATTR_URI (associated URI to retrieve)
	     **   - Other T_ATTR_NAMEVALUE data
	     **/
	    u_int8_t	attrs[FQUEUE_ELEMENT_ATTRSZ];
	} e;
	struct data_fq_elem {
	    char data[FQUEUE_ELEMENTSIZE];
	} d;
    } u;
} fqueue_element_t;

/*
 *******************************************************************************
 *		P U B L I C  F U N C T I O N S
 *******************************************************************************
 */
typedef int fhandle_t;

int create_fqueue(const char *queuefilename, int max_entries);
int create_recover_fqueue(const char *queuefilename, int max_entries,
			  int delete_old_entries);

fhandle_t open_queue_fqueue(const char *queuefilename);
int close_queue_fqueue(fhandle_t fh);

/** inputfile size == FQUEUE_ELEMENTSIZE bytes **/
int enqueue_fqueue(const char *queuefilename, const char *inputfilename);
int enqueue_fqueue_fh(fhandle_t queuefile_fh, const char *inputfilename);
int enqueue_fqueue_fh_el(fhandle_t queuefile_fh, fqueue_element_t *qe);

int dequeue_fqueue(const char *queuefilename, const char *outputfilename,
			int wait_secs);
int dequeue_fqueue_fh(fhandle_t queuefile_fh, const char *outputfilename,
			int wait_secs);
int dequeue_fqueue_fh_el(fhandle_t queuefile_fh, fqueue_element_t *qe,
			int wait_secs);

int enqueue_remove(fqueue_element_t *qe, const char *queuefilename,
			const char *uri, const char * namespace_domain,
			int is_object_fullpath);

int init_queue_element(fqueue_element_t *qe, int no_delete_datafile,
			const char *datafilepathname, const char *uri);

int init_queue_element_fn(const char *outputfilename, int no_delete_datafile,
			const char *datafilepathname, const char *uri);

int add_attr_queue_element(fqueue_element_t *qe, const char *name,
			const char *value);

int add_attr_queue_element_len(fqueue_element_t *qe, const char *name,
			int namelen, const char *value, int valuelen);

int add_attr_queue_element_fn(const char *inputfilename, const char *name,
			const char *value);

int get_fqueue_header(const char *inputfilename, fqueue_header_t *qh);

int get_fqueue_header_fh(fhandle_t fh, fqueue_header_t *qh);

int get_fqueue_element(const char *inputfilename, fqueue_element_t *el);

int get_fqueue_element_fh(fhandle_t fh, off_t off, fqueue_element_t *el);

int get_attr_fqueue_element(const fqueue_element_t *qe, attr_type_t type,
		int nth_attr, /* 0 base, if (type == T_ATTR_NAMEVALUE) */
		const char **name, int *namelen,
		const char **data, int *datalen);

int get_nvattr_fqueue_element_by_name(const fqueue_element_t *qe,
				const char *name, int namelen,
				const char **data, int *datalen);

int get_attr_fqueue_element_fn(const char *inputfilename, attr_type_t type,
		int nth_attr, /* 0 base, if (type == T_ATTR_NAMEVALUE) */
		const char **name, int *namelen,
		const char **data, int *datalen);
#ifdef __cplusplus
}
#endif

#endif /* _FQUEUE_H_ */

/*
 * End of fqueue.h
 */

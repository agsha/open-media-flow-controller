/*
	Nokeena Networks Proprietary
	Author: Vikram Venkataraghavan
*/
#ifndef NKN_MEDIAMGR_API_H
#define NKN_MEDIAMGR_API_H

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>
#include "nkn_defs.h"
#include "nkn_attr.h"
#include "cache_mgr.h"
#include "nkn_namespace.h"
#include "nkn_namespace_stats.h"
#include "nkn_am_api.h"

#define NKN_PHYSID_SZ MAX_URI_SIZE+256
#define NKN_MAX_FILE_NAME_SZ MAX_URI_SIZE+PATH_MAX
#define NKN_MAX_DIR_NAME_SZ MAX_URI_SIZE+256

#define NKN_MM_SAC_SATA_DEF_THRESHOLD 12
#define NKN_MM_SAC_SAS_DEF_THRESHOLD  20
#define NKN_MM_SAC_SSD_DEF_THRESHOLD  1250
#define NKN_MM_MAX_DIR_DEPTH          50

#define NKN_MM_DEBUG_INIT             0
#define NKN_MM_DEBUG_QUEUED           1
#define NKN_MM_DEBUG_INVOK            2
#define NKN_MM_DEBUG_CALLED           4

#define NKN_MM_INGEST_TYPE_PUSH       1
#define NKN_MM_INGEST_TYPE_PULL       2

/*physid string format definition
 ------------------------------------
 | module_  | cod_    | offset + '\0'|
 | 5 bytes +| 8 byte +|    8 byte +  |
 | 1 byte   | 1 byte  | 1 byte       |
 ------------------------------------
 */
#define NKN_PHYSID_MAXLEN   40

/* Ingest flags */
#define INGEST_NO_CLEANUP 0
#define INGEST_CLEANUP_NO_RETRY 1
#define INGEST_CLEANUP_RETRY 2


/*
	The operation enum is specific to media manager and identifies
	whether a particular is for READ or WRITE.
*/
typedef enum {
	OP_NONE,
	MM_OP_READ,
	MM_OP_WRITE,
	MM_OP_STAT,
	MM_OP_DELETE,
	MM_OP_VALIDATE,
	MM_OP_UPDATE_ATTR
} op_t;


struct mm_provider_priv {
	nkn_provider_type_t ptype;
	/* TBD */
};

typedef enum mm_provider_states {
	MM_PROVIDER_STATE_NONE = 0,
	MM_PROVIDER_STATE_ACTIVE,
	MM_PROVIDER_STATE_INACTIVE
} mm_provider_states_t;

enum video_types {
	NKN_VT_NONE = 0,
	NKN_VT_MOVE = 1,
	NKN_VT_OTHER = 2,
};
typedef enum video_types video_types_t;

typedef enum provider_thread_action {
    MM_THREAD_ACTION_ASYNC = 1,
    MM_THREAD_ACTION_IMMEDIATE = 2,
} provider_thread_action_t;

void
mm_send_promote_debug(char *promote_uri, nkn_provider_type_t src,
		      nkn_provider_type_t dst,
                      int flag);
/*
	This structure serves as the response to a DM_stat( ) query.
	- physid: The identity of a block of content. In Release 1, it is the
		  the <inode+blk_id> tuple.  The buffer mgr uses this as an
		  opaque object to handle multiple requests to the same set
		  of blocks.
	- content_len: length of the content that can be served.
	- tot_content_len: Total length of the content.
	- media_blk_len: The block size of this media.
	- The in_ptype of the provider which provides response.
	  - in_ptype is filled by MM but it doesn't hurt for DM* to
	    fill it in since bufmgr calls DM*_stat directly.
*/
#define MM_FLAG_NEW_REQUEST	0x01	// this is the start of a new
					// new request and could
					// be subject to admission control
					// based on resource status
#define MM_FLAG_NO_CACHE	0x02	// do not serve cached data for this
					// request.  must be sent to origin
#define MM_FLAG_TRACE_REQUEST	0x04	// add trace log for this request
#define MM_FLAG_IGNORE_EXPIRY	0x08	// Ok to respond with an expired
					// object to allow revalidation
#define MM_FLAG_CONF_NO_RETRY	0x10	// Do not request retry - e.g. a
					// a retry was previously done
#define MM_FLAG_PREREAD		0x20	// Doing a STAT preread
#define MM_FLAG_ALLOW_PARTIALS	0x40	// Do not serve content if content
					// available is partial
#define MM_FLAG_UNCOND_NO_RETRY	0x80	// dont retry for uncondtional retry
#define MM_FLAG_CACHE_ONLY      0x100   // Fetch only from local cache
#define MM_FLAG_REQ_NO_CACHE	0x200   // client no cache forward to OM
#define MM_FLAG_DNS_DONE	0x400	// dns look up is done
#define MM_FLAG_VALIDATE_TUNNEL	0x800	// sync revalidate if modified tunnel
#define MM_FLAG_CRAWL_REQ       0x1000  // Crawl request
#define MM_FLAG_MM_ING_REQ	0x2000  // Ingest request, return unavail offset

// response flags from provider during stat/get
#define MM_OFLAG_NOAM_HITS      0x01    // provider reports - not to update AM(out_flags)
#define MM_OFLAG_NOBUF		0x02	// The provider does not want
					// any data buffers to be sent down
					// in the GET task.  It will
					// any required allocations itself
#define MM_OFLAG_OBJ_WITH_HOLE  0x04	// Cached object has holes

typedef struct MM_stat_resp {
    char		physid[NKN_PHYSID_MAXLEN];
    int			physid_len;
    uint64_t		physid2;
    nkn_uol_t		in_uol;
    nkn_client_data_t   in_client_data;
    void		*in_proto_data;	// protocol specific data
    uint32_t		in_flags;	// request flags - see above
    uint32_t		out_flags;	// response flags - see above
    uint32_t		media_blk_len;
    off_t		content_len;
    off_t		tot_content_len;
    off_t		unavail_offset;
    nkn_provider_type_t ptype;		// filled by MM
    int			sub_ptype;
    int			mm_stat_ret;
    namespace_token_t	ns_token;
    int                 debug;
    int			attr_size;
} MM_stat_resp_t;

/*
	This structure servers as the response to the an DM_get( ) query.

	The key parameters supplied by the caller are:
	  - The physid that was returned by the STAT task.
	  - Object (byte range) being requested. 
	  - Attribute buffer to be filled in (if required).
	  - Data buffers to be filled in based on the media_blk_len reported
	    in the STAT task.  These are NULL if the STAT task specifies
	    the MM_OFLAG_NOBUF flag.

	The key parameters supplied by the callee are:
	  - Return code
	  - Number of incoming data buffers that were filled.
	  - Data buffers that were allocated by the provider.  These have a
	    reference count of 1 that the caller is responsible for.

	The following errors have special meaning:
		NKN_MM_RETRY_REQUEST - retry the request after specified delay.
			This is used to avoid timing related inefficiencies
			detected by a provider.
*/
#define MM_MAX_OUT_BUFS		64
typedef struct MM_get_resp {
    char		physid[NKN_PHYSID_MAXLEN];
    uint64_t		physid2;
    nkn_uol_t		in_uol;
    nkn_client_data_t	in_client_data;		//client data
    void		*in_proto_data;		 // proto specific data
    out_proto_data_t	out_proto_resp;		 // protocol specific ret data
    void		**in_content_array;	 // array of buffers passed in
    nkn_buffer_t	*in_attr;
    uint32_t		in_num_content_objects;  // # of buffers passed in
    uint32_t		in_flags;		// request flags - see above
    uint32_t		out_flags;		 // response flags from provider
    uint32_t		out_num_content_objects; // # of buffers returned
    nkn_buffer_t	*out_data_buffers[MM_MAX_OUT_BUFS];
						// arrays of buffers allocated
						// by the callee.
    uint32_t		out_num_data_buffers;	// number of buffers allocated
						// by the callee.
    int			err_code;
    int			out_delay_ms;		// Delay in msecs in case
						// error is NKN_MM_RETRY_REQUEST
    nkn_provider_type_t in_ptype;		 // passed as arg
    int			in_sub_ptype;
    namespace_token_t	ns_token;
    nkn_task_id_t       get_task_id;
    uint32_t		provider_flags;
    time_t              start_time;
    time_t              end_time;
    request_context_t	req_ctx;		// Ret on unconditional retry
    						//  passed back in retry
    int			max_retries;
    void *		ocon;
    int                 debug;
    off_t		tot_content_len;
    uint16_t		encoding_type;
} MM_get_resp_t;

typedef enum {
	MM_VALIDATE_NONE,	// unused zero value
	MM_VALIDATE_ERROR,	// transient failure after origin connect
	MM_VALIDATE_INVALID,	// transient failure before origin connect
	MM_VALIDATE_NOT_MODIFIED,
	MM_VALIDATE_MODIFIED,
	MM_VALIDATE_BYPASS,
	MM_VALIDATE_FAIL	// Used to indicate permanent failure,
				// like file not found, redirect, etc.
} MM_validate_ret_codes_t;

#define MM_VALIDATE_EXP_DELIVERY	0x1000

typedef struct MM_validate {
    nkn_uol_t		in_uol;
    nkn_provider_type_t ptype;
    nkn_attr_t		*in_attr;
    nkn_buffer_t	*new_attr;
    namespace_token_t	ns_token;
    void		*in_proto_data;		// proto specific data
    nkn_client_data_t	in_client_data;
    int			ret_code;
    nkn_task_id_t       get_task_id;
    out_proto_data_t	out_proto_resp;		// protocol specific ret data,
						// could be used to return
						//   protocol specific data.
    request_context_t	req_ctx;		// Ret on unconditional retry
    						//  passed back in retry
    void		*cpointer;		// pointer to hold cp
    int			max_retries;
    int                 out_delay_ms;           // Delay in msecs in case
    uint32_t            in_flags;               // request flags 
    int                 debug;
} MM_validate_t;

typedef enum {
	MM_UPDATE_TYPE_SYNC,			//update to sync disk hotness
	MM_UPDATE_TYPE_EXPIRY,
	MM_UPDATE_TYPE_HOTNESS			//update cached hotness in DM2
} MM_update_type_t;

#define MM_UPDATE_FLAGS_EOD 0x01

typedef struct MM_update_attr {
    nkn_uol_t		uol;
    nkn_attr_t		*in_attr;
    nkn_provider_type_t	in_ptype;
    MM_update_type_t	in_utype;
    int			in_uflags;
    int			out_ret_code;
    int                 debug;
} MM_update_attr_t;

/*
	This structure is used by the Origin Manager to populate the diskmgr
	in the offline case.

	Internally, this structure is used between functions in the diskmgr.

	NOTE: The uriname will be hashed to a filename if the objname
	is not set.
*/
#define NKN_MM_PACK_DATA	0x0000001	// Move optimization

/* Flags */
#define MM_FLAG_PRIVATE_BUF_CACHE 0x01  // Indicates chunked encode objects
                                        // from OM, which needs to be put in
                                        // file in tfm.

#define MM_FLAG_DM2_END_PUT	  0x02	// Flag to indicate the end put
					// transaction for an URI. This shall
					// lead to a partial object in DM2
#define MM_FLAG_DM2_ATTR_WRITTEN  0x04  // DM2 flag that indicates the attribute
					// was already written for this PUT
#define MM_FLAG_DM2_PROMOTE	  0x08	// set if object is promoted
					// reset if object is ingested
#define MM_FLAG_CIM_INGEST	  0x10	// Ingest for crawled objects

#define MM_PUT_IGNORE_EXPIRY	  0x20	// Ignore Expiry during put
#define MM_PUT_PUSH_INGEST        0x40  // Push ingest - used when task returned
					// after put.

typedef struct MM_put_data {
    /* These are the new fields */
    nkn_uol_t		uol;		// 24b: This clip
    int		        errcode;	// Should be NKN error code
    uint8_t		iov_data_len;	// Number of vectors in iov_data
    uint8_t		ununsed[3];	// 8 byte alignment
    char		*domain;
    nkn_attr_t		*attr;		// Not required for every put call
    nkn_iovec_t		*iov_data;
    uint64_t		out_content_len;// length that was written to disk
					// for streaming objects (filled by DM2)
    size_t		total_length;	// Length of data portion of object
    uint32_t		flags;
    nkn_provider_type_t ptype;		// Consumes 32bits
    int			sub_ptype;
    nkn_task_module_t   src_manager;

    char		ns_uuid[NKN_MAX_UID_LENGTH];
    uint64_t		namespace_resv_usage;
    uint64_t		namespace_actual_usage;
    uint64_t		cache_pin_resv_usage;
    uint64_t		cache_pin_actual_usage;
    ns_stat_token_t	ns_stoken;	// assigned in dm2_put_active_checks()
    int8_t		cache_pin_enabled;
    int                 debug;
    void                *push_ptr;
    time_t              start_time;
    /* Used by TFM: deprecated */
    //    char		clip_filename[NKN_MAX_FILE_NAME_SZ];
} MM_put_data_t;


/* Used for MM_delete_resp->evict_trans_flag */
#define NKN_EVICT_TRANS_START 1
#define NKN_EVICT_TRANS_STOP 2

/*
 * If we ever decide to send this over the wire, we should compress the flags
 * into a single field.
 */
typedef struct MM_delete_resp {
    nkn_uol_t		in_uol;
    nkn_provider_type_t in_ptype;	// filled by MM
    int			in_sub_ptype;	// filled by MM
    int                 evict_flag;
    int                 evict_trans_flag;
    uint64_t            evict_hotness;
    int			dm2_no_vers_check;// used by DM2 to indicate expired obj
    int			dm2_is_mgmt_locked;// used by DM2 to maintain lock
    int			dm2_local_delete; // flag internal deletes (on PUT)
    int			dm2_del_reason;
    int			out_ret_code;
} MM_delete_resp_t;

typedef struct MM_promote_data {
    nkn_uol_t		in_uol;
    nkn_provider_type_t in_ptype;	// filled by MM
    int			in_sub_ptype;	// filled by MM
    int                 out_ret_code;
    int                 partial_file_write;  // partial promote from TFM
} MM_promote_data_t;

typedef struct MM_provider_stat_s {
    int       ptype;			// IN
    int       sub_ptype;		// IN
    int       percent_full;		// OUT
    int       weight;			// OUT
    nkn_hot_t hotness_threshold;	// OUT
    int       caches_enabled;		// OUT
    int       out_ret_code;		// OUT
    int       high_water_mark;		// OUT
    int       low_water_mark;		// OUT
    uint64_t  cur_io_throughput;
    uint64_t  max_io_throughput;
    int	      max_attr_size;		// OUT
    int       block_size;
} MM_provider_stat_t;

typedef struct MM_delete {
	nkn_uol_t uol;
} MM_delete_t;

/* Session admission control variable */
extern unsigned int nkn_sac_per_thread_limit[NKN_MM_MAX_CACHE_PROVIDERS];

/* Highest max_attr_size of all registered providers */
extern uint32_t glob_mm_largest_provider_attr_size;

/* no. of local provider */
extern uint32_t glob_mm_max_local_providers;

extern uint64_t glob_mm_push_ingest_max_bufs;

/* This function will return information about the URI. */
int MM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp);

/* This function will put content into the disk cache for a URI */
int MM_put(struct MM_put_data *);

/* This function will register each of the provider function ptrs */
int MM_register_provider(nkn_provider_type_t ptype,
			 int sub_ptype,
			 int (*put)(struct MM_put_data *, uint32_t),
			 int (*stat)(nkn_uol_t, MM_stat_resp_t *),
			 int (*get) (MM_get_resp_t *),
			 int (*delete) (MM_delete_resp_t *),
			 int (*shutdown) (void),
			 int (*discover) (struct mm_provider_priv *),
			 int (*evict) (void),
			 int (*provider_stat) (MM_provider_stat_t *),
			 int (*validate) (MM_validate_t *),
			 int (*update_attr) (MM_update_attr_t *),
			 void (*promote_complete)(MM_promote_data_t *),
			 int num_put_threads,
			 int num_get_threads,
			 uint32_t io_q_depth,
			 uint32_t num_disks,
			 int flags);
/* */
int MM_init(void);

int MM_update_thread_count(nkn_provider_type_t ptype, int sub_ptype,
			   int num_put_threads, int num_get_threads,
			   uint32_t num_disks);

/* */
void mm_async_thread_hdl(gpointer data, gpointer user_data);
int MM_get(MM_get_resp_t *in_ptr_resp);

int MM_validate(MM_validate_t *pvld);
int MM_update_attr(MM_update_attr_t *pup);
int MM_delete(MM_delete_resp_t *pdel);
int MM_promote_uri(char *uri, nkn_provider_type_t pt, nkn_provider_type_t hi,
		    nkn_cod_t in_cod, am_object_data_t *in_client_data,
		    time_t update_time);
int MM_push_ingest(AM_obj_t *am_objp, am_xfer_data_t *am_xfer_data,
		    nkn_provider_type_t src, nkn_provider_type_t dst);
int MM_partial_promote_uri(char *uri, nkn_provider_type_t src, nkn_provider_type_t dst, 
		       uint64_t in_offset, uint64_t in_length, 
		       uint64_t rem_length, uint64_t in_total_length,
		       time_t update_time);
void MM_stop_provider_queue(nkn_provider_type_t ptype, uint8_t sptype);
void MM_run_provider_queue(nkn_provider_type_t ptype, uint8_t sptype);
void MM_am_alert_func(void *inptr);
int MM_provider_stat(MM_provider_stat_t *pstat);
int MM_set_provider_state(nkn_provider_type_t ptype, int sub_ptype, int state);
nkn_provider_type_t MM_get_next_faster_ptype(nkn_provider_type_t pt);
nkn_provider_type_t MM_get_fastest_put_ptype(void);
int nkn_mm_validate_dir_depth(const char *uri, int max_depth);
const char*
MM_get_provider_name(nkn_provider_type_t provider);

/* Temporary to remove warning */
int DM_init(void);
int DM2_init(void);
int DM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp);

void mm_push_mgr_cleanup(nkn_task_id_t id);
void mm_push_mgr_input(nkn_task_id_t id);
void mm_push_mgr_output(nkn_task_id_t id);

void move_mgr_cleanup(nkn_task_id_t id);
void move_mgr_input(nkn_task_id_t id);
void move_mgr_output(nkn_task_id_t id);

void nkn_mm_resume_ingest(void *mm_obj, char *uri, off_t client_offset,
			    int flag);
// dm2 specific attribute size needs to be allocated
int nkn_mm_dm2_attr_copy(nkn_attr_t *ap_src, nkn_attr_t **ap_dst,
		  nkn_obj_type_t objtype);
int nkn_mm_create_delete_task(nkn_cod_t cod);

int mm_uridir_to_nsuuid(const char *uri_dir, char *ns_uuid);

void nkn_mm_cim_ingest_failed(char *uri, int errcode);
int mm_update_write_size(nkn_provider_type_t ptype, int size);
#endif	/* NKN_MEDIAMGR_API_H */

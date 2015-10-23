#ifndef __NKN_OM_DEFS__H__
#define __NKN_OM_DEFS__H__


#include <assert.h>
#include <sys/queue.h>
#include "cache_mgr.h" // for nkn_iovec_t definition
#include "nkn_http.h"
#include "server.h"
#include "fqueue.h"
#include "nkn_namespace.h"
#include "chunk_encoding_engine.h"
#include "nkn_lockstat.h"
#include "nkn_hash.h"
#include <zlib.h>
#include <string.h>
#include <limits.h>


/*
 * Defined in om_cfg.c
 */

typedef struct om_cfg {
	struct om_cfg * next;
        char * 	domain;
        char *  ip;
        uint32_t port;
        int32_t prefix_len;
        char * 	prefix;
} om_cfg_t; // deprecated
extern int glob_om_tot_cfg_policy; // deprecated

om_cfg_t * om_lookup_cfg(char * uri); // deprecated

typedef struct origin_server_cfg {
	char *ip;
	uint16_t port; // network byte order
	namespace_token_t ns_token;
	origin_server_policies_t *policies;
	request_policies_t *request_policies;
	const namespace_config_t *nsc;
} origin_server_cfg_t;

/*
 * Max simultaneous chunked objects that can be
 * enqueued to TFM
 */
#define MAX_SIMUL_CHUNK_OBJ_CACHE 200

/* Unconditional retry max time in msecs total=2 x limit */
#define OM_UNCOND_RETRY_MSECS 2500

/*
 * MM_get_resp_t.out_proto_resp.u.OM.flags definitions
 */
#define OM_PROTO_RESP_NO_CONTENT_LEN	(1 << 0)
#define OM_PROTO_RESP_NO_CONTENT_LEN_EOF (1 << 1)
#define OM_PROTO_RESP_CONTENT_LENGTH (1 << 2)
#define OM_PROTO_RESP_CONTENT_LENGTH (1 << 2)

#define OMF_DO_STAT		0x0000000000000001
#define OMF_DO_GET		0x0000000000000002
#define OMF_ENDOF_TRANS		0x0000000000000004
#define OMF_WAIT_COND		0x0000000000000008
#define OMF_INLINE_INGEST	0x0000000000000010
#define OMF_DO_VALIDATE		0x0000000000000020
#define OMF_NO_CACHE		0x0000000000000040
#define OMF_UNCHUNK_CACHE	0x0000000000000080
#define OMF_CHUNK_EOF		0x0000000000000100
#define OMF_VIRUAL_DOMAIN_REQ	0x0000000000000200
#define OMF_UNCHUNK_NOCACHE	0x0000000000000400
#define OMF_NO_CACHE_BM_ONLY	0x0000000000000800
#define OMF_TRANSPARENT_REQ	0x0000000000002000
#define OMF_CONNECT_TIMER     	0x0000000000004000
#define OMF_READ_TIMER     	0x0000000000008000
#define OMF_HTTP_STREAMING_DATA 0x0000000000010000
#define OMF_CON_CLOSE           0x0000000000020000
#define OMF_HAS_SET_TOS		0x0000000000040000
#define OMF_BUFFER_SWITCHED	0x0000000000080000
#define OMF_PE_HOST_HDR		0x0000000000100000
#define OMF_PE_RANGE_HDR	0x0000000000200000
#define OMF_IS_IPV6		0x0000000000400000
#define OMF_OS_FAILOVER_REQ	0x0000000000800000
#define OMF_BUF_CPY_SKIP_OFFSET 0x0000000001000000
#define OMF_COMPRESS_FIRST_TASK 0x0000000002000000
#define OMF_COMPRESS_INIT_DONE  0x0000000004000000
#define OMF_COMPRESS_TASKPOSTED 0x0000000008000000
#define OMF_COMPRESS_TASKRETURN 0x0000000010000000
#define OMF_COMPRESS_COMPLETE   0x0000000020000000
#define OMF_NEGATIVE_CACHING	0x0000000040000000

// Response flag mask
#define OM_RESP_CC_NO_CACHE	0x00000001
#define OM_RESP_CC_MAX_AGE_0	0x00000002
#define OM_RESP_CC_NO_STORE	0x00000004
#define OM_RESP_CC_PRIVATE	0x00000008
#define OM_RESP_CC_PUBLIC	0x00000010
#define OM_RESP_CC_MAX_AGE	0x00000020

/* HASH_URI_CODE(cod, offset)
                ver = cod = 5 bits
                uniq_cod =  25 bits
                offset_val =  34 bits
                HASH_URI_CODE = (ver << 59) | (uniq_cod << 34) | (offset_val)
*/
#define HASH_URI_CODE(cod, offset) \
                (((((uint64_t)cod) >> 32) << 59) | \
                (((uint64_t)(((uint32_t)cod) << 7)) << 27)  | \
                ((((uint64_t)offset) << 30) >> 30))

/* HASH_URI_CODE(cod, offset)
 * 		ver = cod = 8 bits
 * 		uniq_cod =  36 bit
 * 		offset val = 36 bits
*/

#define COD_INDEX_MASK 0xffffffffff
#define COD_REV_SHIFT  40 
#define HASH_URI_CODEI(cod, offset)  \
                (((((uint64_t)cod) >> COD_REV_SHIFT) << 56) | \
                (((uint64_t)(((uint64_t)cod) & COD_INDEX_MASK) << 20)  | \
		((((uint64_t)offset & 0xfffff0000) >> 16))))

#define HASH_URI_CODES(offset)  \
		((uint16_t)((uint64_t)offset & 0xffff))

		

#define OM_PHYSID_LEN	36

typedef struct om_con_cb {
	/*
	 * The fields marked with "XXX:" are accessed from non-network threads
	 * using the following interfaces:
	 *	om_lookup_con_by_physid()
	 *	om_lookup_con_by_uri()
	 *	om_lookup_con_by_fd()
	 *	OM_do_socket_work()
	 *
	 * All updates to the noted fields should be performed under the
	 * the "omccb_mutex" mutex.
	 */
	NKN_CHAIN_ENTRY __urientry;
	NKN_CHAIN_ENTRY __fdentry;
	uint64_t 	flag;
	uint64_t	pe_action;

	/*
         * socket control block
	 */
        int32_t		fd;		// Socket fd
        uint32_t	incarn;		// incarnation always go together with fd
	off_t		content_start;
        off_t 		content_offset;	// XXX: Total content offset including header.
        off_t 		content_data_offset; // content data offset
        off_t 		thistask_datalen;// This task returned data
	int32_t		http_hdr_left;
	void * 		cpcon;		// connection pool con structure
	char *		send_req_buf;	// used to build up HTTP request
	int		send_req_buf_offset;
	int		send_req_buf_totlen;
	int		state;		// debug purpose only

	/*
	 * Match http module socket
	 */
	int32_t		httpfd;		// XXX: HTTP module client socket fd
	char *		httpreq;	// pointing to HTTP module client request buffer
	con_t *		httpreqcon;	// HTTP client con_t
	origin_server_cfg_t oscfg; 	// target origin server data

        /* 
	 * HTTP control block
	 * Debug purpose for saving response to cache after decode
	 */
        nkn_uol_t 	uol;		// XXX:
        http_cb_t * 	phttp;
	off_t		content_length; // XXX:
	off_t		range_offset;
	off_t		range_length;
	char *		dbg_buf;	// store decoded HTTP data.
	time_t		request_send_time;
	time_t		request_response_time;

        /* 
	 * Scheduler control block, all scheduler required fields.
	 */
	MM_get_resp_t * p_mm_task;	// XXX:
	MM_validate_t * p_validate;
	char		physid[NKN_PHYSID_MAXLEN];	// length for OM_cod_offset
	nkn_task_id_t	tfm_taskid;

	chunk_encoding_ctx_t *chunk_encoding_data;
	chunk_encoding_ctx_t *chunk_encoding_data_parser;
	time_t unchunked_obj_expire_time;
	time_t unchunked_obj_corrected_initial_age;
	nkn_objv_t unchunked_obj_objv;
	char *hdr_serialbuf;
	int hdr_serialbufsz;

	/*
	 * Read timer data
	 */
        off_t 		last_thistask_datalen;
	dtois_t 	hash_cod;
	int64_t 	hash_fd;
	int 		hash_cod_flag;
	uint32_t	resp_mask;

	/*
	 * Compression Specific 
	 */
	int comp_status;
	z_stream strm;
	char *comp_buf;
	int comp_offset;
	int comp_type;
	void * comp_task_ptr;
} omcon_t;


#define OM_SET_FLAG(x, mflag) (x)->flag |= (mflag);
#define OM_UNSET_FLAG(x, mflag) (x)->flag &= ~(mflag);
#define OM_CHECK_FLAG(x, mflag) ((x)->flag & (mflag))
#define OM_CON_FD(_ocon) ((_ocon)->fd)
//#define OM_CON_FD(_ocon) ((_ocon)->pfd ? *((_ocon)->pfd) : -1)

#define OM_SET_RESP_MASK(x, flag) (x)->resp_mask |= (flag);
#define OM_UNSET_RESP_MASK(x, flag) (x)->resp_mask &= ~(flag);
#define OM_CHECK_RESP_MASK(x, flag) ((x)->resp_mask & (flag))

LIST_HEAD(om_con_cb_queue, om_con_cb);
TAILQ_HEAD(om_pending_req_queue, pending_req);
extern nkn_mutex_t omccb_mutex;

typedef struct oomgr_request {
	fqueue_element_t fq_element;
	LIST_ENTRY(oomgr_request) list;
} oomgr_request_t;

LIST_HEAD(om_oomgr_req_queue, oomgr_request);
extern pthread_mutex_t om_oomgr_req_queue_mutex;

/*
 * Defined in om_network.c
 */
extern omcon_t * om_lookup_con_by_fd(int fd, nkn_uol_t *uol, int ret_locked,
				int remove_stale_omcon, int *lock_fd);
extern omcon_t *
om_lookup_con_by_uri(nkn_uol_t *uol,
                        int ret_locked, // Return gnm mutex
                        int pass_omcon_locked, // Have omcon_mutex
                        int *uri_hit, off_t *offset_delta,
                        int * ret_val, omcon_t **stale_ocon, int *lock_fd);
extern int OM_do_socket_work(MM_get_resp_t * p_mm_task,  omcon_t **pp_ocon,
				 ip_addr_t* originipv4v6, uint16_t origin_port);
extern int OM_do_socket_validate_work(MM_validate_t * pvalidate, 
					    const mime_header_t *attr_mh,
						omcon_t **pp_ocon);
extern void om_resume_task(omcon_t * ocon);
extern int find_origin_server(con_t * request_con, // IN
			origin_server_cfg_t * oscfg,// IN
			request_context_t * req_ctx, // IN
			const char *client_host, // IN
			int client_host_len,// IN
			int trace_on,// IN
			ip_addr_t* originip, // IN
			uint16_t originport, // IN
			ip_addr_t * ipv4v6, // OUT
			uint16_t * port, // OUT
			int *allow_retry // OUT
			);
/* DNS lookup */
extern uint32_t dns_domain_to_ip(char * domain);
extern int dns_domain_to_ip46(char * domain, int32_t family, ip_addr_t *ip_ptr);

/* Transparent proxy  */
//extern int cp_set_transparent_mode_tosocket(cpcon_t * cpcon);


#endif // __NKN_OM_DEFS__H__


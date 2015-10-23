
#ifndef __SERVER__H__
#define __SERVER__H__

#include <assert.h>
#include <openssl/md5.h>
#include "sys/queue.h"
#include "nkn_sched_api.h"
#include "cache_mgr.h"
#include "nkn_ssp_cb.h"
#include "nkn_http.h"
#include "interface.h"
#include "pe_geodb.h"
#include "pe_ucflt.h"
#include "pe_defs.h"

#define TIMER_INTERVAL	5
#define MAX_ORIG_IP_CNT 4
#define MGRSVR_PORT	7957

/*
 * socket handling
 */

#define SOCKET_CLOSED 		0
#define SOCKET_WAIT_EPOLLOUT_EVENT 1
#define SOCKET_WAIT_EPOLLIN_EVENT 2
#define SOCKET_WAIT_CM_DATA	3
#define SOCKET_DATA_SENTOUT	4
#define SOCKET_TIMER_EVENT	5
#define SOCKET_NO_ACTION	6	// Used by SSP during outbound path (send_to_ssp)
#define SOCKET_CLOSED_DONE_TRUE 7
#define SOCKET_CLOSED_DONE_FALSE 8
#define SOCKET_WAIT_BUFFER_DATA  9
/*
 * connection control block
 */

#define CONF_FIRST_TASK		0x0000000000000001
#define CONF_CANCELD		0x0000000000000004
#define CONF_TASK_POSTED	0x0000000000000008
#define CONF_RECV_DATA		0x0000000000000010
#define CONF_SEND_DATA		0x0000000000000020
#define CONF_BLOCK_RECV		0x0000000000000040
#define CONF_NO_CONTENT_LENGTH	0x0000000000000080
#define CONF_RESP_PARSER_ERROR  0x0000000000000100
#define CONF_TASK_TIMEOUT	0x0000000000000200
#define CONF_HAS_SET_TOS	0x0000000000000800
#define CONF_RETRY_HDL_REQ	0x0000000000001000
#define CONF_REDIRECT_REQ	0x0000000000002000
#define CONF_DNS_DONE		0x0000000000004000
#define CONF_RECV_ERROR         0x0000000000008000
#define CONF_CLIENT_CLOSED      0x0000000000010000
#define CONF_TUNNEL_PROGRESS	0x0000000000020000
#define CONF_RESUME_TASK	0x0000000000040000
#define CONF_INTRL_REQ_SRC	0x0000000000100000
#define CONF_INTRL_HTTPS_REQ	0x0000000000200000
#define CONF_HTTPS_HDR_RECVD	0x0000000000400000
#define CONF_REFETCH_REQ	0x0000000000800000
#define CONF_GEOIP_TASK		0x0000000001000000
#define CONF_PE_HOST_HDR	0x0000000002000000
#define CONF_DNS_HOST_IS_IP	0x0000000004000000
#define CONF_UCFLT_TASK		0x0000000008000000
#define CONF_CI_FETCH_END_OFF   0x0000000010000000
#define CONF_CI_USE_END_OFF   0x0000000020000000

#define CON_MAGIC_FREE          0x123456789deadbee
#define CON_MAGIC_USED          0x1111111111111111

/*
 * con_t NM_func_timer event types
 */
typedef enum {
     TMRQ_EVENT_UNDEFINED=0,
      TMRQ_EVENT_NAMESPACE_WAIT,
      TMRQ_EVENT_NAMESPACE_WAIT_2
} tmrq_event_type_t;

#if 0
typedef struct s_pe_co_list_t {
	char *server;
	char *param_type;
	void *co_resp;
	int32_t status_code;
	struct s_pe_co_list_t *next;
} pe_co_list_t;

typedef struct s_pe_co_t {
	int32_t num_co;
	struct s_pe_co_list_t *cur_co;
	struct s_pe_co_list_t *co_list;
} pe_co_t;
#endif

typedef struct md5_checksum_ {
        /*MD5 checksum calculation for delivery(client) side*/
        MD5_CTX     ctxt;
        uint32_t    in_strlen; /* Length of response data for which MD5
                                    * checksum is already calculated.
                                    */
        uint32_t    temp_seq; /* just for dev debug, has to be removed*/
} http_resp_md5_checksum_t ;

typedef struct struct_con_t{
	uint64_t	magic;
	uint64_t 	con_flag;
	uint64_t	pe_action;

	/* socket control block */
        int32_t 	fd;
        int32_t 	reserved0;
	uint16_t 	reserved1;
	uint16_t 	src_port;
	uint16_t	dest_port;
	uint8_t		ip_family;
        off_t 		sent_len;

	struct {
		uint8_t num_ips;
		uint8_t num_ip_index;
	} os_failover;

	/* Network feature configuration */
	uint64_t max_bandwidth;     	// Session bandwidth: Bytes/sec
	uint64_t min_afr;               // Assured Flow Rate: Bytes/sec
	uint64_t max_client_bw;         // Detected client (ISP) Bandwidth: Bytes/sec
	uint64_t max_allowed_bw;        // Allowed Session Bandwidth (Data Center): Bytes/sec
	uint64_t max_send_size;         // Max Sent Size in this second
	uint64_t bandwidth_send_size;   // AFR, allowed bandwidth send
	uint64_t max_faststart_buf;	// Initial Buffer Size for Fast Start (Bytes)
	time_t   nkn_cur_ts;		// The time to calculate max_send_size

	/* IP TOS setting */
	int32_t		ip_tos;

	/* HTTP control block */
	int32_t		num_of_trans;
	http_cb_t 	http;

	/* Server Side Player control block */
    	char                    session_id[MAX_NKN_SESSION_ID_SIZE];
   	nkn_ssp_params_t	ssp;

	/* server information */
	struct nkn_interface * pns;
    /* VIP pns */
	struct nkn_interface * v_pns;

	/* Scheduler control block */
	nkn_task_id_t	nkn_taskid;

	/* Module type field to indicate whether to copy this or not*/
	nkn_module_t	module_type;

	/* http_timer() state data */
	tmrq_event_type_t timer_event;

	/* ip structures */
	ip_addr_t       src_ipv4v6;
	ip_addr_t       dest_ipv4v6;

	int ns_wait_intervals; // Awaiting namespace init complete intervals

	/* GeoIP info */
	geo_info_t ginf;

	/* URL category */
	ucflt_info_t uc_inf;
	
	struct {
		ip_addr_t ipv4v6[MAX_ORIG_IP_CNT];
		int count;
		ip_addr_t last_ipv4v6;
	}origin_servers;

        /*MD5 checksum calculation for delivery(client) side*/
        http_resp_md5_checksum_t *md5 ;

        /* Transmit rate arguments. max-bw, afr, fast_start buf size */
        pe_transmit_rate_args_u64_t *pe_ssp_args ;

} con_t;

typedef struct http_mgr_output_q {

	struct http_mgr_output_q * next;
	cache_response_t * cptr;
	struct network_mgr * pnm;
	nkn_task_id_t id;
} http_mgr_output_q_t;

	

#define SET_CON_FLAG(x, f) (x)->con_flag |= (f)
#define UNSET_CON_FLAG(x, f) (x)->con_flag &= ~(f)
#define CHECK_CON_FLAG(x, f) ((x)->con_flag & (f))

extern char myhostname[1024];
extern int myhostname_len;


/*
 * http_close_conn() keeps the connection alive while wiping out
 * all HTTP control block data.
 */
void http_close_conn(con_t * con, int sub_errorcode);
int http_try_to_sendout_data(con_t * con);

/*
 * close_conn() closes socket and wipes out all data including socket data 
 * and HTTP control block. 
 */
void close_conn(con_t * con);
void reset_close_conn(con_t * con);

/* perform pre-close routines such as 
 * (A) abortive close, when configured, where we send the pending
 *  bytes and a forced reset to the client if  
 * (B) cleans up the task
 */
int nkn_do_conn_preclose_routine(con_t *con);

int http_handle_request(con_t *con);
int http_send_response(con_t *con);
int http_send_data(con_t * con, char * p, int len);
int reset_http_cb(con_t * con);
int http_apply_ssp(con_t *con, int *val);
int set_ip_policies(int sock_fd, int32_t *old_tos, int new_tos, uint8_t family);
void http_pe_overwrite_ssp_params(con_t *con) ;

/*
 * Internal functions called to post a scheduler task by HTTP module.
 */
int nkn_post_sched_a_task(con_t *con);
int nkn_post_sched_task_again(con_t * con);

void
add_hmoq_item(int num, struct network_mgr *pnm, void *phmoq);

void* remove_hmoq_item(int num);

/*
 * This exported function is used to set up assured flow rate
 * on this session.
 *
 * afr unit is: Bytes/sec
 */
void con_set_afr(con_t *con, uint64_t afr);

/*
 * Local interface IP address
 */
int is_local_ip(const char *ip, int ip_strlen);

/*
 *  Nvsd's heal check info init.
 */
int nkn_init_health_info(void) ;
extern int glob_nvsd_health_shmid ;
/*
 * Any change for these 2 shared memory details has to be
 * replicated in proxyd/pxy_policy.h file.
 */
#define PXY_HEALTH_CHECK_KEY 1234
typedef struct _nvsd_health {
       int good ; /* Just 1 field now */
} nvsd_health_td;

extern nvsd_health_td *nkn_glob_nvsd_health ;

typedef struct pe_con_info_s 
{
	int32_t fd;
	uint32_t incarn;
	con_t *con;
	uint32_t type;
} pe_con_info_t;

#endif // __SERVER__H__


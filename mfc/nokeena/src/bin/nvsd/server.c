#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdint.h>
#include <net/if.h>

#ifndef __USE_MISC
#define __USE_MISC
#endif
#include <netinet/tcp.h>
#include <openssl/md5.h>

#include "nkn_http.h"
#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_sched_api.h"
#include "nkn_stat.h"
#include "nkn_diskmgr_api.h"
#include "nkn_trace.h"
//#include "nkn_accesslog.h"
#include "http_header.h"
#include "ssp.h"
#include "om_defs.h"
#include "nkn_mem_counter_def.h"
#include "nkn_cod.h"
#include "om_fp_defs.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "nvsd_resource_mgr.h"
#include "nkn_encmgr.h"
#include "nkn_tunnel_defs.h"
#include "nkn_nfs_tunnel.h"
#include "nkn_ssl.h"

#ifndef IP_TRANSPARENT
#define IP_TRANSPARENT 19
#endif

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

#ifndef IP_INDEV
#define IP_INDEV 125
#endif

#define F_FILE_ID	LT_SERVER

volatile sig_atomic_t srv_shutdown = 0;
int valgrind_mode = 0;
char myhostname[1024];
int myhostname_len;
int g_dm2_unit_test;
int nkn_http_service_inited = 0;

int	nkn_assert_enable;
static char nkn_build_date[] =           ":Build Date: " __DATE__ " " __TIME__ ;
static char nkn_build_id[] =             ":Build ID: " NKN_BUILD_ID ;
static char nkn_build_prod_release[] =   ":Build Release: " NKN_BUILD_PROD_RELEASE ;
static char nkn_build_number[] =         ":Build Number: " NKN_BUILD_NUMBER ;
static char nkn_build_scm_info_short[] = ":Build SCM Info Short: " NKN_BUILD_SCM_INFO_SHORT ;
static char nkn_build_scm_info[] =       ":Build SCM Info: " NKN_BUILD_SCM_INFO ;
static char nkn_build_extra_cflags[] =   ":Build extra cflags: " EXTRA_CFLAGS_DEF ;

static const char *nvsd_program_name = "mfd";

//char http_listen_intfname[MAX_NKN_INTERFACE][16];
char **http_listen_intfname;
int http_listen_intfcnt = 0;
int max_dns_pend_q = 5000;
int nkn_http_ipv6_enable = 0;
// Local interface IP hash table data
hash_entry_t hash_local_ip[128];
static pthread_mutex_t hash_local_ip_lock[128];
hash_table_def_t ht_local_ip;
hash_entry_t hash_local_ipv6[128];
static pthread_mutex_t hash_local_ipv6_lock[128];
hash_table_def_t ht_local_ipv6;
static const char **ht_local_ipstr;

typedef struct date_str {
	int length;
	char date[128];
} date_str_t;

static date_str_t cur_datestr_array[2];
static volatile int cur_datestr_idx = 1;

extern int glob_tot_get_in_this_second;
extern int nkn_timer_interval;
extern nkn_lockstat_t epolllockstat[MAX_EPOLL_THREADS];
extern int dynamic_uri_enable;

extern int acceptor_init(const char *path);
void nkn_cleanup_a_task(con_t * con);
extern int  read_nkn_cfg(char * configfile);
extern int glob_sched_num_sched_loops;
extern int server_init(void);
extern int server_exit(void);
void cleanup_http_cb(con_t * con);
extern void http_accesslog_write(con_t * con);
extern void nkn_check_cfg(void);
extern void cfg_params_init(void);
extern void check_nknvd(void);
extern void daemonize(void);
extern void config_and_run_counter_update(void);
extern nkn_cod_t http_get_req_cod(con_t *con);
extern URI_REMAP_STATUS nkn_remap_uri(con_t *con, int *resp_idx_offset);
extern int nkn_tokenize_gen_seek_uri(con_t *con);
void httpsvr_threaded_init(void);
static void nkn_check_and_waitfor_preread(void);

extern int is_hex_encoded_uri(char *puri, int len);
extern int del_pns_from_alias_list(nkn_interface_t *, int);


int http_if4_finit(nkn_interface_t *pns, int);
int http_if4_init(nkn_interface_t *pns);
int http_if6_finit(nkn_interface_t *pns);
int http_if6_init(nkn_interface_t *pns);


// preread states
#define NO_PREREAD		(0) // no preread -- present only in exclude disk config
#define PREREAD_START		(1) // monitor preread -- present only in include disk config
#define PREREAD_END		(2) // preread done -- present only in include disk config

extern int service_init_flags;
//preread flags
extern int64_t glob_dm2_sata_preread_done,
	       glob_dm2_sas_preread_done,
	       glob_dm2_ssd_preread_done,
	       glob_dm2_sata_tier_avl,
	       glob_dm2_sas_tier_avl,
	       glob_dm2_ssd_tier_avl;
extern uint64_t glob_dm2_init_done;

int32_t glob_service_preread_init = NO_PREREAD;



extern AO_t glob_tot_hdr_size_from_cache,
    glob_tot_hdr_size_from_sata_disk,
    glob_tot_hdr_size_from_sas_disk,
    glob_tot_hdr_size_from_ssd_disk,
    glob_tot_hdr_size_from_disk,
    glob_tot_hdr_size_from_nfs,
    glob_tot_hdr_size_from_origin,
    glob_tot_hdr_size_from_tfm;

NKNCNT_DEF(vip_parent_if_hit, uint64_t, "", "Successful getsockopt for VIP")
NKNCNT_DEF(vip_parent_if_miss, uint64_t, "", "Failed getsockopt for VIP")
NKNCNT_DEF(tot_vip_hit, uint64_t, "", "Total conns received for VIPs")
NKNCNT_DEF(om_version_expired, uint64_t, "", "version expured")
NKNCNT_DEF(data_not_sentout_when_timeout, uint64_t, "", "nkn_cod_open return err")
NKNCNT_DEF(err_cod_open_ret, uint64_t, "", "nkn_cod_open return err")
NKNCNT_DEF(err_cod_dup_ret, uint64_t, "", "nkn_cod_dup return err")
NKNCNT_DEF(http_task_cancelled, uint64_t, "", "http task cancelled")
NKNCNT_DEF(enlarge_cb_size, uint64_t, "", "enlarge network cb size")
NKNCNT_DEF(cur_open_http_socket, AO_t, "", "Total currently open HTTP sockets")
NKNCNT_DEF(cur_open_http_socket_ipv6, AO_t, "", "Total currently open HTTP sockets for ipv6")
NKNCNT_DEF(tot_svr_sockets, uint16_t, "", "Total server sockets")
NKNCNT_DEF(tot_svr_sockets_ipv6, uint16_t, "", "Total ipv6 server sockets")
NKNCNT_DEF(tot_http_sockets, AO_t, "", "Total HTTP sockets")
NKNCNT_DEF(tot_http_sockets_ipv6, AO_t, "", "Total HTTP sockets ipv6")
NKNCNT_DEF(http_schd_err_get_data, uint64_t, "", "Total GET transaction")
NKNCNT_DEF(http_schd_cur_open_task, AO_t, "", "Total current open tasks")
NKNCNT_DEF(cm_err_errcode, uint64_t, "", "Total CM return error code")
NKNCNT_DEF(cm_err_cancel, uint64_t, "", "Total cancelled task id due to socket closed")
NKNCNT_DEF(http_mgr_output, uint64_t, "", "Total Scheduler callback")
NKNCNT_DEF(tot_bytes_recv, uint64_t, "Bytes", "Total received bytes from socket")
NKNCNT_DEF(tot_bytes_tobesent, AO_t, "Bytes", "Total Bytes received from CM and wait socket for sending")
NKNCNT_DEF(warn_failed_sendout, AO_t, "Bytes", "Total Bytes failed to send out")
NKNCNT_DEF(accept_queue_100, uint64_t, "", "times of accept queue when larger than 100")
NKNCNT_DEF(err_pipeline_req, uint64_t, "", "num of pipeline request")
NKNCNT_DEF(warn_socket_no_recv_data, uint64_t, "", "num of sockets closed without any data received")
NKNCNT_DEF(warn_socket_no_send_data, uint64_t, "", "num of sockets closed without any data sent")
NKNCNT_DEF(overflow_socket_due_afr, uint64_t, "", "num of sockets closed due to AFR limit")
NKNCNT_DEF(err_taskid_not_match, uint64_t, "", "Scheduler returned id does not match")
NKNCNT_DEF(err_cptr_already_freed, uint64_t, "", "Scheduler returned task with already freed cptr")
NKNCNT_DEF(err_timeout_with_task, AO_t, "", "Connection timed out but a task is being hold by scheduler")
NKNCNT_DEF(http_tot_timeout, uint64_t, "", "Connection idled out without activity")
NKNCNT_DEF(tot_event_lost, uint64_t, "", "socket is not full, we lost epollout event")
NKNCNT_DEF(http_tot_send_failed, uint64_t, "", "socket buffer is full, data never get sent out")
NKNCNT_DEF(err_sent_and_with_task, uint64_t, "", "conflict with data sentout called and in scheduler task")
NKNCNT_DEF(tot_video_delivered, AO_t, "", "Total delivered video")
NKNCNT_DEF(tot_video_delivered_with_hit, AO_t, "", "Total delivered video from cache")
NKNCNT_DEF(http_expire_restart, uint64_t, "", "Restarted HTTP request tasks due to object expiration")
NKNCNT_DEF(http_expire_restart_2, uint64_t, "", "Restarted HTTP request tasks due to object expiration")
NKNCNT_DEF(nkn_cur_ts, uint64_t, "", "current time in sec")
NKNCNT_DEF(tot_client_send_from_bm_or_dm, AO_t, "", "")
NKNCNT_DEF(tot_client_send, AO_t, "", "")
NKNCNT_EXTERN(http_tot_200_resp, uint64_t)
NKNCNT_EXTERN(http_tot_206_resp, uint64_t)
NKNCNT_EXTERN(http_tot_302_resp, uint64_t)
NKNCNT_EXTERN(http_tot_304_resp, uint64_t)
NKNCNT_EXTERN(http_tot_400_resp, int)
NKNCNT_EXTERN(http_tot_404_resp, int)
NKNCNT_EXTERN(http_tot_413_resp, int)
NKNCNT_EXTERN(http_tot_414_resp, int)
NKNCNT_EXTERN(http_tot_416_resp, int)
NKNCNT_EXTERN(http_tot_500_resp, int)
NKNCNT_EXTERN(http_tot_501_resp, int)
//NKNCNT_DEF(http_malloc_err_52002, uint64_t, "", "Http malloc error")
//NKNCNT_DEF(http_request_err_52003, uint64_t, "", "Http request error")
NKNCNT_DEF(http_namespace_err_52004, AO_t, "", "Http namespacec error")
//NKNCNT_DEF(http_req_range_1_err_52005, uint64_t, "", "Http request range 1 error")
//NKNCNT_DEF(http_req_range_2_err_52006, uint64_t, "", "Http request range 2 error")
//NKNCNT_DEF(http_req_range_3_err_52007, uint64_t, "", "Http request range 3 error")
//NKNCNT_DEF(http_res_range_1_err_52008, uint64_t, "", "Http responce range 1 error")
//NKNCNT_DEF(http_res_range_2_err_52009, uint64_t, "", "Http responce range 2 error")
//NKNCNT_DEF(http_res_range_3_err_52010, uint64_t, "", "Http responce range 3 error")
//NKNCNT_DEF(http_content_type_err_52011, uint64_t, "", "Http content type error")
NKNCNT_DEF(http_uri_err_52012, AO_t, "", "Http uri error")
//NKNCNT_DEF(http_uri_more_err_52013, uint64_t, "", "Http uri more error")
//NKNCNT_DEF(http_uri_less_err_52014, uint64_t, "", "Http uri less error")
//NKNCNT_DEF(http_res_seek_err_52015, uint64_t, "", "Http responce seek error")
NKNCNT_DEF(http_close_conn_err_52016, AO_t, "", "Http close conn error")
NKNCNT_DEF(http_get_cont_len_err_52017, AO_t, "", "Http get content length error")
NKNCNT_DEF(http_no_host_http11_err_52018, AO_t, "", "Http no host http11 error")
//NKNCNT_DEF(http_parser_err_52019, uint64_t, "", "Http parser error")
//NKNCNT_DEF(http_chunked_req_err_52019, uint64_t, "", "Http chunked request error")
NKNCNT_DEF(http_close_conn_err_52030, AO_t, "", "Http close conn error")


NKNCNT_DEF(om_server_fetch_err_50001, AO_t, "", "OM server fetch error")
NKNCNT_DEF(om_server_down_err_50003, AO_t, "", "OM server down error")
NKNCNT_DEF(om_no_namespace_err_50007, AO_t, "", "OM no namespace error")
NKNCNT_DEF(om_internal_err_50008, AO_t, "", "OM internal error")
NKNCNT_DEF(om_non_cacheable_err_50019, AO_t, "", "OM non cacheable error")
NKNCNT_DEF(om_inv_obj_ver_err_50021, AO_t, "", "OM invalid obj version error")
NKNCNT_DEF(om_service_unavailable_err_50028,
			AO_t, "", "OM service unavailble error")

NKNCNT_DEF(tunnel_non_cacheable, uint64_t, "", "Tunnel non cacheable response")
NKNCNT_DEF(cache_tcp_half_close, uint64_t, "", "Cache TCP Half Close")

NKNCNT_DEF(nkn_server_busy_err_10003, uint64_t, "", "Server busy error")
NKNCNT_DEF(sched_task_create_fail_cnt, uint64_t, "", "Scheduler task creation failed error")
NKNCNT_DEF(client_recv_tot_bytes, AO_t, "", "read data")

NKNCNT_DEF(cl7_proxy_use_om, uint64_t, "", "CL7 proxy to node failed, use OM")
NKNCNT_DEF(cl7_proxy_use_om_failed, uint64_t, "", "CL7 proxy use of OM failed")
NKNCNT_DEF(nfst_cr_init_failed, uint64_t, "", "NFS tunnel cr init failed")
NKNCNT_DEF(nfst_con_task_check_failed, uint64_t, "",
				"NFS tunnel task check failed")
NKNCNT_DEF(nfst_con_task_create_failed, uint64_t, "",
				"NFS tunnel task create failed")
NKNCNT_DEF(nfst_uol_2_uri_failed, uint64_t, "", "NFS tunnel uol to uri failed")
NKNCNT_DEF(nfst_req_2_origin_failed, uint64_t, "",
				"NFS tunnel request to Origin call failed")
NKNCNT_DEF(nfst_find_config_failed, uint64_t, "", "NFS tunnel ns config failed")
NKNCNT_DEF(nfst_get_uri_failed, uint64_t, "", "NFS tunnel uri get failed")

int64_t glob_max_302_redirect_limit = MAX_HTTP_REDIRECT;
NKNCNT_DEF(http_encoding_mismatch, uint64_t, "", "request and response encoding mismatch")

NKNCNT_DEF(tunnel_resp_cache_index_no_uri, uint64_t, "", "No URI found")
NKNCNT_DEF(tunnel_resp_cache_index_exceed_uri_size_1, uint64_t, "", "Cache index uri exceeds max uri size")
NKNCNT_DEF(tunnel_resp_cache_index_exceed_uri_size_2, uint64_t, "", "Cache index uri exceeds max uri size")
NKNCNT_DEF(tunnel_resp_cache_index_dyn_uri_err, uint64_t, "", "Cache index uri exceeds max uri size")
NKNCNT_DEF(resp_cache_index_cod_open_err, uint64_t, "", "Cache index based uri code open error")
NKNCNT_DEF(resp_cache_index_tot_requests, uint64_t, "", "Total number of cache index based requests")
NKNCNT_DEF(resp_cache_index_ssp_return_or_fail, uint64_t, "", "Cache index SSP returned error or sent response")

NKNCNT_DEF(post_sched_task_fail, uint64_t, "", "Post sched task to BM call failure")
NKNCNT_DEF(post_sched_task_again_fail, uint64_t, "", "Post sched task to BM again call failure")
NKNCNT_DEF(normalize_302_redirect, uint64_t, "", "Normalized con for 302 redirect")

NKNCNT_DEF(uf_reject_reset_80001, uint64_t, "", "URL filter - reject with connection reset")
NKNCNT_DEF(uf_reject_close_80002, uint64_t, "", "URL filter - reject with connection close")
NKNCNT_DEF(uf_reject_http_403_80003, uint64_t, "", "URL filter - reject with HTTP 403")
NKNCNT_DEF(uf_reject_http_404_80004, uint64_t, "", "URL filter - reject with HTTP 404")
NKNCNT_DEF(uf_reject_http_301_80005, uint64_t, "", "URL filter - reject with HTTP 301")
NKNCNT_DEF(uf_reject_http_302_80006, uint64_t, "", "URL filter - reject with HTTP 302")
NKNCNT_DEF(uf_reject_http_200_80007, uint64_t, "", "URL filter - reject with HTTP 200")
NKNCNT_DEF(uf_reject_http_custom_80008, uint64_t, "", "URL filter - uri size limit reject with HTTP custom response")
NKNCNT_DEF(uf_uri_size_reject_reset_80010, uint64_t, "", "URL filter - uri size limit reject with connection reset")
NKNCNT_DEF(uf_uri_size_reject_close_80011, uint64_t, "", "URL filter - uri size limit reject with connection close")
NKNCNT_DEF(uf_uri_size_reject_http_403_80012, uint64_t, "", "URL filter - uri size limit reject with HTTP 403")
NKNCNT_DEF(uf_uri_size_reject_http_404_80013, uint64_t, "", "URL filter - uri size limit reject with HTTP 404")
NKNCNT_DEF(uf_uri_size_reject_http_301_80014, uint64_t, "", "URL filter - uri size limit reject with HTTP 301")
NKNCNT_DEF(uf_uri_size_reject_http_302_80015, uint64_t, "", "URL filter - uri size limit reject with HTTP 302")
NKNCNT_DEF(uf_uri_size_reject_http_200_80016, uint64_t, "", "URL filter - uri size limit reject with HTTP 200")
NKNCNT_DEF(uf_uri_size_reject_http_custom_80017, uint64_t, "", "URL filter - uri size limit reject with HTTP custom response")

NKNCNT_EXTERN(normalize_remapped_uri, uint64_t)

NKNCNT_DEF(http_vary_user_agent_mismatch, uint64_t, "", "Restarted HTTP request tasks due to user agent mismatch")
NKNCNT_DEF(http_vary_encoding_mismatch, uint64_t, "", "Restarted HTTP request tasks due to encoding mismatch")
NKNCNT_DEF(tunnel_vary_user_agent_mismatch, uint64_t, "", "Tunnel request due to user agent mismatch")

/*
 * The following queue all the responses from http_mgr_output
 * Each epoll thread will have one queue
 */
extern struct http_mgr_output_q  * hmoq_head[MAX_EPOLL_THREADS];
extern struct http_mgr_output_q  * hmoq_plast[MAX_EPOLL_THREADS];
extern pthread_mutex_t hmoq_mutex[MAX_EPOLL_THREADS];
extern pthread_cond_t hmoq_cond[MAX_EPOLL_THREADS];

/*
 * Process timer events initiated by the con_t NM_func_timer handler
 * on the associated net thread.
 */
NKNCNT_DEF(http_tmrq_ignored, uint64_t, "", "Total HTTP tmrq ignored events")
NKNCNT_DEF(http_tmrq_processed, uint64_t, "", "Total HTTP tmrq processed events")
NKNCNT_DEF(http_tmrq2_processed, uint64_t, "", "Total HTTP tmrq type 2 processed events")
NKNCNT_DEF(http_tmrq_invalid, uint64_t, "", "Total HTTP tmrq invalid events")
NKNCNT_DEF(http_tmrq_events, uint64_t, "", "Total HTTP tmrq add events")
NKNCNT_DEF(http_tmrq_event_fails, uint64_t, "", "Total HTTP tmrq add event failures")

typedef struct http_timer_req_q {
	struct http_timer_req_q *next;
	net_fd_handle_t fhd;
	tmrq_event_type_t type;
} http_timer_req_q_t;

struct http_timer_req_q  * tmrq_head[MAX_EPOLL_THREADS];
struct http_timer_req_q  * tmrq_plast[MAX_EPOLL_THREADS]={NULL};
pthread_mutex_t tmrq_mutex[MAX_EPOLL_THREADS] =
{
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER
};
static int tmrq_add_event(int fd, tmrq_event_type_t type);


int nkn_do_post_sched_a_task(con_t *con, off_t offset_requested, off_t length_requested);
int nkn_do_post_nfst_a_task(con_t *con, off_t offset_requested, off_t length_requested);
void http_mgr_cleanup(nkn_task_id_t id);
void http_mgr_input(nkn_task_id_t id);
void http_mgr_output(nkn_task_id_t id);
int check_out_hmoq(int num_of_epoll);
void check_out_tmrq(int num_of_epoll);
void server_timer_1sec(void);
void init_conn(int svrfd, int sockfd, nkn_interface_t *pns,
	       nkn_interface_t *ppns, struct sockaddr_storage *addr,
	       int thr_num);
static void *httpsvr_init(void *);
//void nkn_setup_interface_parameter(int i);
void nkn_setup_interface_parameter(nkn_interface_t *);
int get_cache_index_uri(const char *uri, int urilen, unsigned char md5bytes[],
			int scheme, int idx_offset, int cksum_len,
			char cache_uri[]);
int cache_index_md5tostr(unsigned char md5[], char md5str[], int len);

static int httpsvrv6_epollin(int sockfd, void * private_data);
static int httpsvrv6_epollout(int sockfd, void * private_data);
static int httpsvrv6_epollerr(int sockfd, void * private_data);
static int httpsvrv6_epollhup(int sockfd, void * private_data);

// Remove the following after eliminating uol.uri
// #define SEND_UOL_URI 1

#define DEFAULT_CKSUM_LEN 	16
#define CKSUM_STR_LEN		64

// Various cache index schemes to position the cache-index string
// within the transformed URI.
enum cache_index_scheme {
	CACHE_INDEX_DEFAULT_SCHEME,	/* Append cache-index string as last part of the URI. */
	CACHE_INDEX_PREFIX,		/* Let cache-index string be the only string in the URI */
	CACHE_INDEX_DYN_URI,		/* Dynamic URI specifier for cache index position in the URI*/
	CACHE_INDEX_MAX = -1,		/* MAX for boundary check */
};

int cache_idx_scheme = CACHE_INDEX_DEFAULT_SCHEME;

vary_hdr_t vary_hdr_cfg;
char usr_agent_cache_index[MAX_USER_AGENT_GROUPS][4] = { ".sp", ".tb", ".pc", ".df" };
char encoding_cache_index[4][4] = { ".uc", ".gz", ".z", ".bz" };
char usr_agent_regex[4][32] = { ".*curl.*", "", "", "" };

static void nkn_record_hit_history (con_t *con, cache_response_t *cptr, uint64_t size);
static void
nkn_record_hit_history (con_t *con, cache_response_t *cptr, uint64_t size)
{
	const char *ptmp;
	int len;
	int al_record_cache_history = 0;
	int this_provider;
	int provider_is_changed = 0;
	long int prev_provider_size = 0;
	int hit_history_init = 0;

	if  (cptr && (size == 0)) {
		return;
	}

	if (cptr) {
		if (con->http.last_provider != cptr->provider) {
			provider_is_changed = 1;
			prev_provider_size = con->http.last_provider_size_from;
			con->http.last_provider = cptr->provider;
			con->http.last_provider_size_from = size;
		} else {
			con->http.last_provider_size_from += size;
		}
		this_provider = con->http.last_provider;

		/* Update counters */
		switch (this_provider) {
		case SolidStateMgr_provider:
		case BufferCache_provider:
		case SAS2DiskMgr_provider:
		case SATADiskMgr_provider:
		case NFSMgr_provider:
			AO_fetch_and_add(&glob_tot_client_send_from_bm_or_dm, size);
			AO_fetch_and_add(&glob_tot_client_send, size);
			break;
		case OriginMgr_provider:
		case Tunnel_provider:
		default:
			AO_fetch_and_add(&glob_tot_client_send, size);
			break;
		}
	}

	/*
	 * Special case that neither "%d" nor %e" is configured, but "%c" is configured.
	 */
	if (con->http.nsconf != NULL) {
		al_record_cache_history =
		    con->http.nsconf->acclog_config->al_record_cache_history;
	}

	if (al_record_cache_history == 0) {
		if (con->http.hit_history == NULL) return;
	}

	if (con->http.hit_history == NULL) {
		if (cptr == NULL || con->http.nfs_tunnel) return; // Not valid.
		con->http.hit_history = (char *)nkn_malloc_type(
	    		HIT_HISTORY_SIZE, mod_http_hit_history_t);
		con->http.hit_history[0] = 0;
		hit_history_init = 1;
	}
	/* record last provider information */
	if (provider_is_changed) {
		switch (cptr->provider) {
		case OriginMgr_provider: {
			char origin_info[64];
			char ip[INET6_ADDRSTRLEN];
			int log_ip = 0;
			const char *host = NULL;
			int hostlen = 0;
			int ret = 0;
			int hdrcnt = 0;
			u_int32_t attrs;

			if (con->origin_servers.last_ipv4v6.family == AF_INET) {
                               if(inet_ntop(AF_INET, &IPv4(con->origin_servers.last_ipv4v6),
							&ip[0], INET6_ADDRSTRLEN) != NULL)
					log_ip = 1;
			} else if (con->origin_servers.last_ipv4v6.family == AF_INET6) {
                               if(inet_ntop(AF_INET6, &IPv6(con->origin_servers.last_ipv4v6),
							&ip[0], INET6_ADDRSTRLEN) != NULL)
					log_ip = 1;
			}
			if (!log_ip) {
				mime_header_t response_hdr;
				if (con->http.attr) {
			                init_http_header(&response_hdr, 0, 0);
                			ret = nkn_attr2http_header(con->http.attr, 0, &response_hdr);
			       		if (!ret) {
						if (is_known_header_present(&response_hdr, MIME_HDR_X_NKN_ORIGIN_IP)) {
							ret = get_known_header(&response_hdr, MIME_HDR_X_NKN_ORIGIN_IP,
									    &host, &hostlen, &attrs, &hdrcnt);
							if (!ret)
								log_ip = 1;
						}
						shutdown_http_header(&response_hdr);
					}

				}
			}
			if (log_ip) {
				snprintf(origin_info, 64, "%s:%s", "Origin", host? host:ip);
				ptmp = origin_info;
			} else {
				ptmp = "Origin";
			}
		break;
		}
		case SolidStateMgr_provider: ptmp="SSD"; break;
		case BufferCache_provider: ptmp="Buffer"; break;
		case SAS2DiskMgr_provider: ptmp="SAS"; break;
		case SATADiskMgr_provider: ptmp="SATA"; break;
		case NFSMgr_provider: ptmp="NFS"; break;
		case Tunnel_provider: ptmp="Tunnel"; break;
		default: ptmp="internal"; break;
		}

		if (hit_history_init) {
			len = snprintf(&con->http.hit_history[con->http.hit_history_used],
					HIT_HISTORY_SIZE - con->http.hit_history_used - 1,
					"%s", ptmp);
		} else {
			len = snprintf(&con->http.hit_history[con->http.hit_history_used],
					HIT_HISTORY_SIZE - con->http.hit_history_used - 1,
					"_%ld_%s", prev_provider_size, ptmp);
		}
		len  = MIN(len, HIT_HISTORY_SIZE - con->http.hit_history_used - 1);
		con->http.hit_history_used += len;
	} else {
		if (cptr == NULL && size == 0) {
			len = snprintf(&con->http.hit_history[con->http.hit_history_used],
					HIT_HISTORY_SIZE - con->http.hit_history_used - 1,
					"_%ld", con->http.last_provider_size_from);
			len  = MIN(len, HIT_HISTORY_SIZE - con->http.hit_history_used - 1);
			con->http.hit_history_used += len;
		}
	}

	return;
}

/*
 * ==================> scheduler related functions <=============================
 * internal function for create a new task or repost second task on the same HTTP request.
 */
static nfst_response_t *
nfst_cr_init(void)
{
        nfst_response_t *nfst_cr = nkn_calloc_type(1, sizeof(nfst_response_t),
				mod_nfst_response_t);
        return nfst_cr;
}

static void
nfst_cr_free(nfst_response_t *nfst_cr)
{
	if (nfst_cr->uri)
		free(nfst_cr->uri);
	free(nfst_cr);
}

static cache_response_t *
cr_init(void)
{
        cache_response_t *cr = nkn_calloc_type(1, sizeof(*cr),
					       mod_http_cache_response_t);
        return cr;
}

static void
cr_free(cache_response_t *cr)
{
        if (cr) {
		if (cr->in_rflags & CR_RFLAG_NFS_BYPASS) {
			nfst_cr_free((nfst_response_t *)cr);
			return;
		}
		if (cr->uol.cod != NKN_COD_NULL) {
			nkn_cod_close(cr->uol.cod, NKN_COD_NW_MGR);
			cr->uol.cod = NKN_COD_NULL;
		}
		free(cr);
	}
}


/*
 * This function posts a task to scheduler.
 * Scheduler should call Buffer Manager input function.
 *
 * If error happens, we should close the connection.
 * return FALSE: con is freed.
 * return TRUE: con is still valid.
 *
 */
int
nkn_do_post_sched_a_task(con_t *con, off_t offset_requested,
			 off_t length_requested)
{
        cache_response_t *cr;
        uint32_t deadline_in_msec;
	const char *data;
	int len, hostlen;
	u_int32_t attrs;
	int hdrcnt;
	char *p;
	const namespace_config_t * ns_cfg;
	char cache_uri [MAX_URI_HDR_HOST_SIZE + MAX_URI_SIZE + 1024];
	int rv;
	char host[2048];
	namespace_token_t token;
	char *p_host = host;
	uint16_t port;
	size_t cachelen;
	http_config_t *httpcfg;

        // The fd should always be in the epoll list
        // for monitoring socket reset by peer
        if (NM_add_event_epollin(con->fd) == FALSE) {
		close_conn(con);
		return FALSE;
	}

	if (con->http.nfs_tunnel) {
		if (nkn_do_post_nfst_a_task(con, offset_requested,
					    length_requested)) {
			close_conn(con);
			return FALSE;
		}
		goto end;
	}

        // build up the cache response structure
        cr = cr_init();
        if (!cr) {
		close_conn(con);
		return FALSE;
	}
        cr->magic = CACHE_RESP_REQ_MAGIC;

        if (mime_hdr_get_cache_index(&con->http.hdr,
                &data, &len, &attrs, &hdrcnt)) {

                cr_free(cr);
                close_conn(con);
                return FALSE; // Should never happen
        }

	if (CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST)) {
		L7_SWT_NODE_NS(&con->http);
	}

	ns_cfg = con->http.nsconf;
	if (ns_cfg == NULL) {
		cr_free(cr);
		close_conn(con);
		return FALSE;
	}
	/* Default to strip query if ssp config is present
	 * (or) Check strip_query knob B1750
	 * */
	if ((ns_cfg->ssp_config && ns_cfg->ssp_config->player_type != -1) ||
		(ns_cfg->http_config && ns_cfg->http_config->request_policies.strip_query == NKN_TRUE)) {
		if (!CHECK_CON_FLAG(con, CONF_REFETCH_REQ) &&
			is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_QUERY)) {
			p = memchr(data, '?', len);
			if (p) {
				len = p-data;
                                add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                                                 data, len);
			}
		} else {
			DBG_LOG(MSG, MOD_HTTP, "RBCI Probe request and/or no query header present."
				" Strip query string from URI action skipped, con=%p", con);
		}
	}

	if ((httpcfg = ns_cfg->http_config) != NULL) {
		if (ns_cfg->http_config->policies.eod_on_origin_close == NKN_TRUE) {
		    cr->in_rflags |= CR_RFLAG_EOD_CLOSE;
		}
		if (ns_cfg->http_config->policies.ingest_only_in_ram) {
		    cr->in_rflags |= CR_RFLAG_ING_ONLYIN_RAM;
		}
		if (ns_cfg->http_config->policies.delivery_expired == NKN_TRUE) {
		    cr->in_rflags |= CR_RFLAG_DELIVER_EXP;
		}
		if (ns_cfg->http_config->policies.reval_type == NKN_TRUE) {
		    cr->in_rflags |= CR_RFLAG_POLICY_REVAL;
		}
		cr->nscfg_contlen =
		    ns_cfg->http_config->policies.store_cache_min_size;
	}

        /*get_known_header and add_known_header for the same heap varaible makes that variable unusable
        so getting 'data' again here*/
        if (mime_hdr_get_cache_index(&con->http.hdr,
                &data, &len, &attrs, &hdrcnt)) {
                cr_free(cr);
                close_conn(con);
                return FALSE; // Should never happen
        }

	if (con->http.req_cod != NKN_COD_NULL) {
		if (!ns_cfg->ssp_config || ns_cfg->ssp_config->player_type == -1) {
			// No SSP so cache_uri will remain the same.
			goto valid_cod;
		}

		// Have COD, insure associated URI is the same
		char *cod_uri = 0;
		int cod_urilen;

		rv = nkn_cod_get_cn(con->http.req_cod, &cod_uri, &cod_urilen);
		if (cod_uri) {
			hostlen = 0;
			rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &con->http.hdr,
						    ns_cfg, 0, 0, &p_host, sizeof(host),
						    &hostlen, &port, 0, 1);
			if (rv) {
				DBG_LOG(MSG, MOD_HTTP,
					"request_to_origin_server() failed, rv=%d", rv);
				cr_free(cr);
				close_conn(con);
				return FALSE;
			}

			token.u.token = 0;
			rv = nstoken_to_uol_uri_stack(token, data,
					    len, host, hostlen,
					    cache_uri, sizeof(cache_uri),
					    ns_cfg, &cachelen);
			if (!rv) {
				DBG_LOG(MSG, MOD_HTTP, "nstoken_to_uol_uri(token=0x%lx) failed",
							con->http.ns_token.u.token);
				cr_free(cr);
				close_conn(con);
				return FALSE;
			}

			if (strcmp(cache_uri, cod_uri)) {
				// URI is different
				nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
				con->http.req_cod = nkn_cod_open(cache_uri,
							    cachelen, NKN_COD_NW_MGR);
				if (con->http.req_cod == NKN_COD_NULL) {
					DBG_LOG(MSG, MOD_HTTP,
						"No COD for cache_uri=%s, request dropped", cache_uri);
					cr_free(cr);
					close_conn(con);
					glob_err_cod_open_ret ++;
					return FALSE;
				}
			}
			goto valid_cod;
		} else {
			DBG_LOG(MSG, MOD_HTTP, "nkn_cod_get_cn(cod=0x%lx) failed, rv=%d",
						con->http.req_cod, rv);
			nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
			con->http.req_cod = NKN_COD_NULL;
			goto valid_cod;
		}
	}

	hostlen = 0;
	rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &con->http.hdr,
				      ns_cfg, 0, 0, &p_host, sizeof(host),
				      &hostlen, &port, 0, 1);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP,
			    "request_to_origin_server() failed, rv=%d", rv);
		cr_free(cr);
		close_conn(con);
		return FALSE;
	}

	token.u.token = 0;
	rv = nstoken_to_uol_uri_stack(token, data,
				     len, host, hostlen,
				     cache_uri, sizeof(cache_uri),
				     ns_cfg, &cachelen);
	if (!rv) {
		DBG_LOG(MSG, MOD_HTTP, "nstoken_to_uol_uri(token=0x%lx) failed",
					con->http.ns_token.u.token);
		cr_free(cr);
		close_conn(con);
		return FALSE;
	}

	con->http.req_cod = nkn_cod_open(cache_uri, cachelen, NKN_COD_NW_MGR);
	if (con->http.req_cod == NKN_COD_NULL) {
		DBG_LOG(MSG, MOD_HTTP,
			"No COD for cache_uri=%s, request dropped", cache_uri);
		cr_free(cr);
		close_conn(con);
		glob_err_cod_open_ret ++;
		return FALSE;
	}

valid_cod:
	cr->uol.cod = nkn_cod_dup(con->http.req_cod, NKN_COD_NW_MGR);
	if (!cr->uol.cod) {
		DBG_LOG(MSG, MOD_HTTP,
			    "No COD for cache_uri=%s, request dropped", cache_uri);
		cr_free(cr);
		close_conn(con);
		glob_err_cod_dup_ret ++;
		return FALSE;
	}
#ifdef SEND_UOL_URI
	cr->uol.uri = (char *)nkn_cod_get_cnp(cr->uol.cod);
#endif

	cr->uol.offset = offset_requested;
	cr->uol.length = length_requested;
	cr->in_client_data.proto = NKN_PROTO_HTTP;
	if (CHECK_HTTP_FLAG(&con->http, HRF_LOCAL_GET_REQUEST)) {
		//cr->in_client_data.ip_address = -1;
		memset(&(cr->in_client_data.ipv4v6_address), 0, sizeof(ip_addr_t));
		cr->in_client_data.port = -1;
		cr->in_client_data.client_ip_family = AF_INET;
		cr->in_proto_data = NULL;
	} else {
		memcpy(&(cr->in_client_data.ipv4v6_address), &(con->src_ipv4v6), sizeof(ip_addr_t));
		cr->in_client_data.port = con->src_port;
		cr->in_client_data.client_ip_family = con->ip_family;
		cr->in_proto_data = con;
	}


	if (con->sent_len == 0) {
		cr->in_rflags |= (CR_RFLAG_GETATTR | CR_RFLAG_FIRST_REQUEST);
		SET_CON_FLAG(con, CONF_FIRST_TASK);
		/* BZ 1907: set flag CR_RFLAG_NO_DATA if AM count is not to be
		 * updated the task will return with no data to http_output_mgr
		 * which will call back this function with the task id reset
		 * for a new task
		 */
		if (length_requested == 0) {
			cr->in_rflags |=
			    (CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) &&
		    	     httpcfg &&
			     !httpcfg->request_policies.passthrough_http_head) ?
		       	CR_RFLAG_NO_DATA : 0;
		}
	}
	else {
		UNSET_CON_FLAG(con, CONF_FIRST_TASK);
	}

	// Fix for BZ 10300 & BZ 10481
	if (CHECK_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED)) {
	    if (con->ssp.ssp_callback_status > 0) {
		/* For all internal fetches, post the first fetch we need to
		 * clear the following flags. This will prevent revalidation
		 * from occuring for every internal SSP fetch. Only the first
		 * fetch will undergo revalidation.
		 */
		cr->in_rflags &= ~CR_RFLAG_FIRST_REQUEST;
		CLEAR_HTTP_FLAG(&con->http, HRF_REVALIDATE_CACHE);
	    }

	    if (CHECK_HTTP_FLAG(&con->http, HRF_SSP_INTERNAL_REQ)) {
		cr->in_rflags |= CR_RFLAG_IGNORE_COUNTERSTAT;
	    }
	    else {
		cr->in_rflags &= ~CR_RFLAG_IGNORE_COUNTERSTAT;
	    }
	}

        if (CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ)) {
		cr->in_rflags |= CR_RFLAG_MFC_PROBE_REQ;
        }

	if ( CHECK_HTTP_FLAG(&con->http, HRF_REVALIDATE_CACHE) ) {
		cr->req_max_age = con->http.req_max_age;
		cr->in_rflags |= CR_RFLAG_REVAL;
	}

	if (CHECK_HTTP_FLAG(&con->http, HRF_NO_CACHE) ||  CHECK_CON_FLAG(con, CONF_REFETCH_REQ)) {
		cr->in_rflags |= (CR_RFLAG_NO_CACHE | CR_RFLAG_SEP_REQUEST);
	}

	// Set trace flag if requested
	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST) ? CR_RFLAG_TRACE : 0;

	// Set the cache only request flag if enforced by SSP
	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_SSP_CACHE_ONLY_FETCH) ? CR_RFLAG_CACHE_ONLY : 0;
	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_SSP_NO_AM_HITS) ? (CR_RFLAG_NO_AMHITS) : 0;

	if (CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD)) {
		if (httpcfg && !(httpcfg->request_policies.passthrough_http_head ||
					    httpcfg->request_policies.http_head_to_get)) {
			cr->in_rflags |= CR_RFLAG_CACHE_ONLY;
		}
	}

	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_SEP_REQUEST) ? CR_RFLAG_SEP_REQUEST : 0;
	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_DYNAMIC_URI) ? CR_RFLAG_SYNC_REVALWAYS : 0;
	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_CLIENT_INTERNAL) ? CR_RFLAG_PREFETCH_REQ : 0;
	cr->in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_CLIENT_INTERNAL) ? CR_RFLAG_CLIENT_INTERNAL : 0;

	cr->ns_token = con->http.ns_token;

	if (cr->in_rflags & CR_RFLAG_DELIVER_EXP) {
		//unset max-age, nocache, proxyreval config
		cr->in_rflags &= ~(CR_RFLAG_NO_CACHE | CR_RFLAG_REVAL | CR_RFLAG_POLICY_REVAL);
	}

        //
        // calculate deadline
        //
        // By this time, I do not have any buffered data in my socket hand.
        // Assume each task call will return 1MBytes data
        // So I can set deadline by this format:
        //
	if ( con->min_afr ) {
		/*
		 * ABR Feature is enabled, then we need to calculate the deadline_in_msec
		 *
		 * Assuming that every task will get one buffer page 32 KBytes
		 * con->min_afr unit is Bytes/sec.
		 * deadline_in_msec unit is msec.
		 * So the equation is
	 	 *
		 *  deadline_in_msec = Bytes in a Buffer Page / scon->min_afr
		 */
        	deadline_in_msec = 1000 * (32 * KBYTES) / con->min_afr; // in msec
	} else {
		// feature disabled
		deadline_in_msec = 0;
	}

	if (con->nkn_taskid != -1) {
		// BUG: TBD:
		//
		// Something is not right.
		// Most likely, it is because HTTP pipeline request.
		// last task is not finished yet, the second GET request has already come.
		// We will close this kind of connection so far.
		// We should do assert here.
		// assert (con->nkn_taskid == -1)
		glob_err_pipeline_req ++;

		close_conn(con);
		cr_free(cr); /* Decrement refcount for COD entry and release cache response*/
		return FALSE;
	}

        // Create a new task
        con->nkn_taskid = nkn_task_create_new_task(
                                        TASK_TYPE_CHUNK_MANAGER,
                                        TASK_TYPE_HTTP_SRV,
                                        TASK_ACTION_INPUT,
                                        0, /* NOTE: Put callee operation if any*/
                                        cr,
                                        sizeof(cache_response_t),
                                        deadline_in_msec);
	if (con->nkn_taskid == -1) {
                DBG_LOG(MSG, MOD_HTTP,
                            "task creation failed.sock %d offset=%ld, length is %ld.",
                            con->fd, offset_requested, length_requested);
                glob_sched_task_create_fail_cnt++;
                if (CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT)) {
                        close_conn(con);
                } else {
                        http_build_res_504(&con->http, NKN_RESOURCE_TASK_FAILURE);
                        http_close_conn(con, NKN_RESOURCE_TASK_FAILURE);
                }
		cr_free(cr); /* Decrement refcount for COD entry and release cache response*/
                return FALSE;
	}
	DBG_LOG(MSG, MOD_HTTP,
		"sock %d create a task %ld. offset=%ld, length is %ld.",
		con->fd, con->nkn_taskid, offset_requested, length_requested);

	nkn_task_set_private(TASK_TYPE_HTTP_SRV, con->nkn_taskid, (void *)&gnm[con->fd]);
        nkn_task_set_state(con->nkn_taskid, TASK_STATE_RUNNABLE);
	AO_fetch_and_add1(&glob_http_schd_cur_open_task);

end:
	// We cannot use nkn_taskid to judge if a task is in progress or in our hand.
	// (nkn_taskid != -1) does not mean the task is in the hand of scheduler
	// because it could happen for (nkn_taskid != -1) when we are sending data out.
	// So this flag is introduced.
	SET_CON_FLAG(con, CONF_TASK_POSTED);
	NM_EVENT_TRACE(con->fd, FT_TASK_NEW);
        return TRUE;
}

/*
 * Scheduler interface function for posting task to buffer manager module
 *
 * return FALSE: con is freed.
 * return TRUE: con is still valid.
 */
int
nkn_post_sched_a_task(con_t *con)
{
	off_t offset_requested;
	off_t length_requested;
	int ret;

	if ( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE) ) {
		offset_requested = con->http.brstart;
		if ( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE_START_STOP) || con->http.brstop ) {
			length_requested = con->http.brstop - con->http.brstart + 1;
		} else {
			length_requested = 0;
		}
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_SEEK) ) {
		offset_requested = con->http.seekstart;
		if ( con->http.seekstop ) length_requested = con->http.seekstop - con->http.seekstart + 1;
		else length_requested = 0;
	} else {
		offset_requested = 0;
		length_requested = con->http.content_length;
	}

	ret = nkn_do_post_sched_a_task(con, offset_requested, length_requested);

	if (ret != TRUE) {
		glob_post_sched_task_fail++;
    }
        return ret;
}

/*
 * Scheduler interface function for posting task once again to
 * buffer manager module
 *
 * return FALSE: con is freed.
 * return TRUE: con is still valid.
 */
int nkn_post_sched_task_again(con_t * con)
{
	off_t offset_requested;
	off_t length_requested;
	off_t extra_data;
	int ret = TRUE;

	if (CHECK_CON_FLAG(con, CONF_TUNNEL_PROGRESS)) {
		NKN_ASSERT(0);
		return ret;
	}

	if ( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE) ) {
		offset_requested = con->http.brstart + con->sent_len;
		if ( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE_START_STOP) || con->http.brstop ) {
			length_requested = con->http.brstop - con->http.brstart - con->sent_len + 1;
		} else {
			length_requested = 0;
		}
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_SEEK) ) {
		extra_data = con->http.prepend_content_size +
			con->http.append_content_size;
		offset_requested = con->http.seekstart +
			(con->sent_len - extra_data);
		if ( con->http.seekstop ) {
			length_requested = (con->http.seekstop -
				con->http.seekstart + 1) -
				(con->sent_len - extra_data);
		} else {
			length_requested = 0;
		}
	} else {
		offset_requested = con->sent_len;
		if (con->http.content_length) {
			length_requested = con->http.content_length - con->sent_len;
		} else {
			length_requested = 0;
		}
	}

	if (con->sent_len > 0) {
		SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE_HM);
	}

	ret = nkn_do_post_sched_a_task(con, offset_requested, length_requested);

	if (ret != TRUE) {
		glob_post_sched_task_again_fail++;
	}
        return ret;
}

void
nkn_cleanup_a_task(con_t *con)
{
	if ( con->nkn_taskid == -1) {
		return;
	}

	NM_EVENT_TRACE(con->fd, FT_TASK_CLEANUP);
	if (CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
		DBG_LOG(SEVERE, MOD_HTTP,
			"Need to close a socket but scheduler has not "
			"returned the task yet. socket id=%d task id=%ld",
			con->fd, con->nkn_taskid);
		DBG_ERR(SEVERE,
                        "Need to close a socket but scheduler has not "
                        "returned the task yet. socket id=%d task id=%ld",
                        con->fd, con->nkn_taskid);
	  	assert(0);
	}

	//nkn_task_set_private(TASK_TYPE_HTTP_SRV, con->nkn_taskid, NULL);

        nkn_task_set_action_and_state(con->nkn_taskid, TASK_ACTION_CLEANUP,
				      TASK_STATE_RUNNABLE);

	con->nkn_taskid = -1;
	con->http.attr = NULL;
	AO_fetch_and_sub1(&glob_http_schd_cur_open_task);
}


/*
 * scheduler interface functions for talking to buffer manager module
 *
 * return FALSE: con is freed.
 * return TRUE: con is still valid.
 */
int
http_try_to_sendout_data(con_t *con)
{
        int ret;

	NM_EVENT_TRACE(con->fd, FT_HTTP_TRY_TO_SENDOUT_DATA);
	if (NM_CHECK_FLAG(&gnm[con->fd], NMF_RST_WAIT)) {
		NM_del_event_epoll_inout(con->fd);
		NM_add_event_epollin(con->fd);
		return TRUE;
	}

	//printf("http_try_to_sendout_data: fd=%d\n", con->fd);
	if (CHECK_CON_FLAG(con, (CONF_TASK_POSTED|CONF_CANCELD))) {
		NM_del_event_epoll(con->fd);
		DBG_LOG(WARNING, MOD_HTTP,
			"Call to send out data while scheduler has not "
			"returned the task yet. socket id=%d task id=%ld",
			con->fd, con->nkn_taskid);
	  	//assert(0);
		glob_err_sent_and_with_task++;
		/* change the code to wait scheduler returns */
		return TRUE;
	}

	// It could be called by server_timer()
	// Under that case, we may not have data in the server module.
	// Then we need to post a scheduler task.
	// BZ 1907: the only exception here is when we post a BM task for HEAD request with CR_RFLAG_NO_DATA
	// In this case we do not expect the data to come back and we can proceed to sending the response via
	// http_send_response
	if ( (con->http.num_nkn_iovecs == 0) &&
	    !CHECK_HTTP_FLAG(&con->http, HRF_ZERO_DATA_ERR_RETURN) &&
	    !CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) &&
	    !(con->http.attr && (con->http.attr->na_flags2 & NKN_OBJ_NEGCACHE) &&
		    (con->http.attr->content_length == 0)) ) {
		ret = nkn_post_sched_task_again(con);
		return ret;
	}

	NM_set_socket_active(&gnm[con->fd]);

        // Time to send data out
        ret = http_send_response(con);
	//printf("bug: ====>   ret=%d\n", ret);

        switch( ret ) {
        case SOCKET_WAIT_EPOLLOUT_EVENT:
		/*
		 * Data has not been completely sent out
		 * Wait for next epoll event to send out more data.
		 */
		NM_add_event_epollout(con->fd);
                break;
        case SOCKET_WAIT_EPOLLIN_EVENT:
		/*
		 * This HTTP transaction is completed.
		 * HTTP structure has been reset.
		 * It is keep-alive connection and wait for next HTTP transaction.
		 */
		if (NM_add_event_epollin(con->fd) == FALSE) {
			/*
			 * BUG 6400:
			 * Since NM_add_event_epollin returns FALSE,
			 * in pipeline case, con should have already been freed.
			 * so we do not need to call close_conn(con) here.
			 * for safety purpose, I still call close_conn here.
			 */
			if (gnm[con->fd].private_data == con) {
				close_conn(con);
			}
			return FALSE;
		}
                break;
	    case SOCKET_WAIT_CM_DATA:
		/*
                 * all data have been sent, call CM to release buffer.
		 * Ask scheduler to send more data.
		 */
		nkn_cleanup_a_task(con);
		// No checks for MBR in this scenario. BZ 10327
		NM_del_event_epoll(con->fd);
		if (nkn_post_sched_task_again(con) == FALSE) {
		    DBG_LOG(MSG, MOD_HTTP, "Post sched task again call to BM failed");
		    return FALSE;
		}
		break;

            case SOCKET_WAIT_BUFFER_DATA:
                /*
                 * all data have been sent, call CM to release buffer.
                 * Ask scheduler to send more data.
                 */
                nkn_cleanup_a_task(con);
                // Due to Bandwidth limitation
                /*
                * Due to Bandwidth limitation
                 * Additional check to see, if we used the timeslot wisely.
                 * We should be sending atleast MBR for a single timeslot, if not
                 * fetch more data and send it, as below
                */
                if ( con->max_bandwidth == 0 ||
			(con->max_send_size < con->max_bandwidth) || (con->max_faststart_buf > 0)) {
                    // Otherwise session bandwidth feature is not enabled
                    NM_del_event_epoll(con->fd);
                    if (nkn_post_sched_task_again(con) == FALSE) {
                        DBG_LOG(MSG, MOD_HTTP, "Post sched task again call to BM failed");
                        return FALSE;
                    }
                    break;
                }
		// Otherwise when session bandwidth is configured, follow through the next case
	case SOCKET_TIMER_EVENT:
		/*
		 * AFR case, wait for next second time slot to send next data.
		 */
		NM_del_event_epoll(con->fd);
		{
			// put it into time wait queue
			struct network_mgr * pnm;

			// If session bandwidth feature is enabled,
			// we will count on timer to post next sched task
			pnm=&gnm[con->fd];
			NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
			if (NM_CHECK_FLAG(pnm, NMF_IN_SBQ) ) {
				LIST_REMOVE(pnm, sbq_entries);
			}
			LIST_INSERT_HEAD( &(pnm->pthr->sbq_head[pnm->pthr->cur_sbq]), pnm, sbq_entries);
			NM_SET_FLAG(pnm, NMF_IN_SBQ);
			NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
		}
		break;
        case SOCKET_CLOSED:
		/*
		 * Identify partial data response due to client side connection closures.
		 */
		if ((con->http.respcode && !con->http.subcode) &&
			(!CHECK_HTTP_FLAG(&con->http, HRF_TRANSCODE_CHUNKED) &&
			(con->sent_len < con->http.content_length))) {
			con->http.subcode = NKN_HTTP_CLIENT_CLOSED;
			AO_fetch_and_add1(&glob_http_close_conn_err_52030);
			DBG_LOG(MSG, MOD_HTTP,
				"Client connection closed after partial data transfer "
				"resp_code=%d, subcode=%d, partial data size=%ld",
				con->http.respcode, con->http.subcode, con->sent_len);
		}

		/*
		 * Bug 5716 - If close_socket_by_RST is enabled and there is pending
		 * data in the send queue, give some more time to data go out.
		 * otherwise RST will discard all the data which is in the tcp queues
		 */
    	        if (nkn_do_conn_preclose_routine(con) == TRUE) {
		    /* no need to close the connection; returning TRUE
		     * to the EPOLL loop will force close the fd
		     */
		    return TRUE;
		} else {
		    // No action; close the connection, cleanup fd etc
		    close_conn(con);
		    return FALSE;
		}
	case SOCKET_CLOSED_DONE_FALSE:
                /* socket already closed via http_close_conn; nothing
		 * to do
		 */
		return FALSE;
	case SOCKET_CLOSED_DONE_TRUE:
	        /* indicate to EPOLL loop that force reset was done and
		 * the fd needs to be closed
		 */
	        return TRUE;
	case SOCKET_NO_ACTION:
	        /*
		 * This is set by SSP when exiting from send_to_ssp
		 * If SSP determines that request has to be tunneled based on analysing the response data, then
		 * this return code is set, since the tunnel is already make prior to this return
		 */
	        break;

        case SOCKET_DATA_SENTOUT:
        default:
		assert(0); // crash here, should never happen
		break;
	}

	return TRUE;
}

/* Match given user agent with a list of regex and return the index
 * If there is no match return -1
 */
int get_user_agent_group(con_t *con, const char *usr_agnt, int len);

int get_user_agent_group(con_t *con, const char *usr_agnt, int len) {
	int i;
	int rv;
	char errorbuf[1024];
	char *str;

#if 1
	const namespace_config_t *nsconf;
	
	// For the no. of regex check if the user agent matches with the regex
	nsconf = con->http.nsconf;
	if (nsconf->http_config->vary_hdr_cfg.enable == 0) {
		return( -1 );
	}
	
	str = alloca(len+1);
	strncpy(str, usr_agnt, len);
	str[len] = 0;
	for (i = 0; i < MAX_USER_AGENT_GROUPS; i++) {
		if (nsconf->http_config->vary_hdr_cfg.user_agent_regex_ctx[i]) {
			rv = nkn_regexec(nsconf->http_config->vary_hdr_cfg.user_agent_regex_ctx[i], str,
					 errorbuf, sizeof(errorbuf));
			if (!rv) {
				return(i);
			}
		}
	}
	return(-1);
#endif

#if 0
	if (vary_hdr_cfg.user_agent_regex_ctx[0] == NULL) {
		for (i = 0; i < MAX_USER_AGENT_GROUPS; i++) {
			if (usr_agent_regex[i][0]) {
				vary_hdr_cfg.user_agent_regex_ctx[i] = nkn_regex_ctx_alloc();
				rv = nkn_regcomp(vary_hdr_cfg.user_agent_regex_ctx[i],
					     //vary_hdr_cfg.user_agent_regex[i],
					     usr_agent_regex[i],
					     errorbuf, sizeof(errorbuf));
				if (rv) {
					errorbuf[sizeof(errorbuf)-1] = '\0';
					DBG_LOG(MSG, MOD_HTTP, "nkn_regcomp() failed, rv=%d, "
						"regex=[\"%s\"] error=[%s]", 
						rv, vary_hdr_cfg.user_agent_regex[i], errorbuf);
				}
			}
		}
	}

	str = alloca(len+1);
	strncpy(str, usr_agnt, len);
	str[len] = 0;
	for (i = 0; i < MAX_USER_AGENT_GROUPS; i++) {
		if (vary_hdr_cfg.user_agent_regex_ctx[i]) {
			rv = nkn_regexec(vary_hdr_cfg.user_agent_regex_ctx[i], str,
					 errorbuf, sizeof(errorbuf));
			if (!rv) {
				return(i);
			}
		}
	}
	return(-1);
#endif
}

void
http_mgr_cleanup(nkn_task_id_t id) {
	UNUSED_ARGUMENT(id);
        return ;
}

void
http_mgr_input(nkn_task_id_t id) {
	UNUSED_ARGUMENT(id);
        return ;
}

void
http_mgr_output(nkn_task_id_t id)
{
	http_mgr_output_q_t * phmoq;
        cache_response_t * cptr;
        con_t *con;
	int num;
	struct network_mgr * pnm;
	int task_canceled;
	int ret = TRUE;
	const namespace_config_t *nsconf;
	namespace_token_t ns_token;
	const char *uri;
	int urilen;
	int rv;
	u_int32_t attr;
	int hdrcnt;

	glob_http_mgr_output ++;

        // Get cache_response_t structure
        cptr = (cache_response_t *) nkn_task_get_data(id);
        if (!cptr) {
		glob_http_schd_err_get_data ++;
                //printf("ERROR: nkn_task_get_data returns NULL\n");
                return;
        }
 //     assert(cptr->magic == CACHE_RESP_RESP_MAGIC);

        // Get con_t structure
	pnm = (struct network_mgr *)nkn_task_get_private(TASK_TYPE_HTTP_SRV, id);

	/*
	 * Do a sanity check of this task id.
	 */
	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_SERVER);

	if (cptr->magic != CACHE_RESP_RESP_MAGIC) {
                glob_err_cptr_already_freed++;
                NM_TRACE_UNLOCK(pnm, LT_SERVER);
                pthread_mutex_unlock(&pnm->mutex);
                return;
	}

	if ( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
		// BUG: memory leak
		glob_err_taskid_not_match++;
                cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
	}
	con = (con_t *)pnm->private_data;
	if (!con || con->nkn_taskid != id) {
		/*
		 * TBD: This case should never happen.
		 * We need to have a valid test case to see why this case
		 * happened.
		 */
		glob_err_taskid_not_match++;
		DBG_LOG(MSG, MOD_HTTP, "scheduler returned not matched id=%ld", id);
                cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
	}

	if (CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST)) {
		L7_SWT_PARENT_NS(&con->http);
	}

	/*
	 * socket closed by client.
	 */
	NM_EVENT_TRACE(con->fd, FT_HTTP_MGR_OUTPUT);
	task_canceled = CHECK_CON_FLAG(con, CONF_CANCELD);
        if (task_canceled)  {
		int32_t cp_sockfd = -1;
		uint32_t cp_incarn = 0;
		glob_http_task_cancelled++;
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		if (cptr->errcode == NKN_OM_NON_CACHEABLE) {
			cp_sockfd = cptr->out_proto_resp.u.OM.cp_sockfd;
			cp_incarn = cptr->out_proto_resp.u.OM.incarn;
			if ((cp_sockfd >= 0 && cp_sockfd < MAX_GNM) &&
				(gnm[cp_sockfd].incarn == cp_incarn) &&
				(con->fd == cptr->out_proto_resp.u.OM.httpcon_sockfd)) {
					cp_add_event(cptr->out_proto_resp.u.OM.cp_sockfd, OP_CLOSE, NULL);
			}
		}
		if (CHECK_CON_FLAG(con, CONF_TASK_TIMEOUT)) {
			UNSET_CON_FLAG(con, CONF_TASK_TIMEOUT);
			AO_fetch_and_sub1(&glob_err_timeout_with_task);
			cptr->errcode = NKN_HTTP_CANCELED_SOCKET;
		}
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TPROXY_MODE)) {
			SET_CON_FLAG(con, CONF_CLIENT_CLOSED);
			close_conn(con);
		} else {
			if (!(CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT) ||
			    CHECK_HTTP_FLAG(&con->http, HRF_ONE_PACKET_SENT))) {
				if (cptr->errcode == NKN_OM_SERVER_FETCH_ERROR) {
					http_build_res_504(&con->http, NKN_OM_SVR_ERR_CLT_CANCEL);
				}
			} else {
				SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);
			}
			http_close_conn(con, cptr->errcode);
		}
                cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
                pthread_mutex_unlock(&pnm->mutex);
		return;
	}

	UNSET_CON_FLAG(con, CONF_RESUME_TASK);
	/*
	 * check out the error code.
	 */
	if (cptr->errcode) {

	glob_cm_err_errcode ++;
	switch(cptr->errcode) {

	case NKN_BUF_VERSION_EXPIRED:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		if (!con->sent_len) {
			/*
			 * Object expired, restart the operation since no
			 * bytes have been sent to the client.
			 */
			nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
			con->http.req_cod = NKN_COD_NULL;

			/*
			 * Check if we have already restarted, to avoid
			 * infinite loop. Close the connection second time.
			 * BZ 3330.
			 * Increase the retries by one to cater for BZ 3717 and 3733
			 */
			if ( !CHECK_HTTP_FLAG(&con->http, HRF_EXPIRE_RESTART_DONE)) {
				glob_http_expire_restart++;
				ret = nkn_post_sched_a_task(con);
				if (ret != FALSE) {
					SET_HTTP_FLAG(&con->http,
						      HRF_EXPIRE_RESTART_DONE);
				} else {
					DBG_LOG(MSG, MOD_HTTP,
						"Post sched task to BM failed");
				}
			} else if ( !CHECK_HTTP_FLAG(&con->http,
						   HRF_EXPIRE_RESTART_2_DONE)) {
				glob_http_expire_restart_2++;
				ret = nkn_post_sched_a_task(con);
				if (ret != FALSE) {
					SET_HTTP_FLAG(&con->http,
						     HRF_EXPIRE_RESTART_2_DONE);
				} else {
					 DBG_LOG(MSG, MOD_HTTP,
						"Post sched task to BM failed");
				}
			} else {
				glob_om_version_expired++;
				goto next;
				//CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
				//http_close_conn(con, cptr->errcode);
			}
		} else {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_close_conn(con, cptr->errcode);
		}
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

	case NKN_BUF_INVALID_OFFSET:
		if ((con->sent_len == 0) && cptr->attr) {
                        con->http.cache_encoding_type = cptr->attr->encoding_type;
                        /*
			 * cptr->attr->encoding_type = 0 means data is
			 * uncompressed type. It can be served to any client.
			 * con->http.http_encoding_type = 0 means client can
			 * accept only uncompressed data. so if the data
			 * is compressed type then it should be checked with
			 * client's request type. If there is a mismatch, it
			 * should be tunnelled.
			 */
                        if (cptr->attr->encoding_type) {
                                if (con->http.http_encoding_type &&
                                        (cptr->attr->encoding_type & con->http.http_encoding_type)) {
                                } else {
                                        UNSET_CON_FLAG(con, CONF_TASK_POSTED);
                                        nkn_cleanup_a_task(con);
                                        con->http.tunnel_reason_code = NKN_TR_REQ_NOT_SUPPORTED_ENCODING;
                                        glob_http_encoding_mismatch++;
                                        goto next;
                                }
                        }
                }
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_416(&con->http, cptr->errcode);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

	case NKN_NFS_STAT_INV_OFFSET:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		con->http.provider = NFSMgr_provider;
		con->http.first_provider = NFSMgr_provider;
		con->http.last_provider = NFSMgr_provider;
		http_build_res_416(&con->http, cptr->errcode);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
                return;

	case NKN_BUF_INV_EOD_OFFSET:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

#if 0
	case NKN_SERVER_BUSY:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_503(&con->http, cptr->errcode);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
#endif // 0

	case NKN_NFS_STAT_URI_ERR:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		/*
		 * when NFS_stat fails, cache provider is shown as unknown
		 */
		con->http.provider = NFSMgr_provider;
		con->http.first_provider = NFSMgr_provider;
		con->http.last_provider = NFSMgr_provider;
		http_build_res_404(&con->http, cptr->errcode);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
#if 0
		/* BZ 3639
		 * VALGRIND: throws invalid read on freed buffer
		 * in this piece of code. Disable this case, till we
		 * fix it properly.
		 */
	case NKN_BUF_VALIDATE_ORIGIN_FAILURE:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		if (cptr->out_proto_resp.u.OM.data_len &&
				    cptr->out_proto_resp.u.OM.data) {
		    int avai_len = con->http.cb_max_buf_size -
					    con->http.cb_totlen - 1;
		    if (avai_len >= cptr->out_proto_resp.u.OM.data_len) {
			con->http.resp_buf =
				&con->http.cb_buf[con->http.cb_totlen + 1];
			con->http.resp_buflen = avai_len;
		    } else {
			con->http.resp_buf = (char *)nkn_malloc_type(
				cptr->out_proto_resp.u.OM.data_len, mod_http_respbuf);
			if (con->http.resp_buf != NULL) {
			    DBG_LOG(MSG, MOD_HTTP, "malloc failed");
			    free(cptr->out_proto_resp.u.OM.data);
			    cptr->out_proto_resp.u.OM.data_len = 0;
			    goto next;
			}
			con->http.resp_buflen =
				cptr->out_proto_resp.u.OM.data_len;
			SET_HTTP_FLAG(&con->http, HRF_MALLOCED_RESP_BUF);
		    }
		    memmove(con->http.resp_buf,
			    cptr->out_proto_resp.u.OM.data,
			    cptr->out_proto_resp.u.OM.data_len);
		    con->http.res_hdlen = cptr->out_proto_resp.u.OM.data_len;
		    free(cptr->out_proto_resp.u.OM.data);
		} else {
		    goto next;
		}
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		pthread_mutex_unlock(&pnm->mutex);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		return;
#endif

	case NKN_OM_SERVER_DOWN:
	case NKN_OM_SERVICE_UNAVAILABLE:
	{
		const char *hostport;
		int hostportlen;
		char *pbuf;
		char abuf[1024];

		if (CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST)) {
		    if (!CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_LOCAL)) {

			// Proxy node connect failed, retry request to origin
		    	SET_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_LOCAL);
			ret = get_known_header(&con->http.hdr,
					MIME_HDR_X_NKN_CL7_ORIGIN_HOST,
					&hostport, &hostportlen,
					&attr, &hdrcnt);
			if (!ret && !(*hostport) && (hostportlen > 1)) {
				/*
				 * Remove NULL to force OM to use it
				 */
				pbuf = alloca(hostportlen);
				memcpy(pbuf, hostport+1, hostportlen-1);
				pbuf[hostportlen-1] = '\0';
				rv = add_known_header(&con->http.hdr,
					MIME_HDR_X_NKN_CL7_ORIGIN_HOST,
					pbuf, hostportlen-1);

				/* Access log status header */
				rv = snprintf(abuf, sizeof(abuf), "Retry=%s",
					      pbuf);
				update_known_header(&con->http.hdr,
					MIME_HDR_X_NKN_CL7_STATUS, abuf, rv, 0);
			}
			delete_known_header(&con->http.hdr,
						MIME_HDR_X_NKN_CL7_PROXY);
			glob_cl7_proxy_use_om++;

			UNSET_CON_FLAG(con, CONF_TASK_POSTED);
			nkn_cleanup_a_task(con);
			nkn_post_sched_a_task(con);
			cr_free(cptr);
			NM_TRACE_UNLOCK(pnm, LT_SERVER);
			pthread_mutex_unlock(&pnm->mutex);
			return;
		    } else {
			glob_cl7_proxy_use_om_failed++;
		    }
		}
		// Fall through
	}
	case NKN_OM_SERVER_FETCH_ERROR:
	case NKN_OM_LEAK_DETECT_ERROR:
	case NKN_OM_ORIGIN_FAILOVER_ERROR:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		if (cptr->errcode == NKN_OM_SERVICE_UNAVAILABLE) {
			http_build_res_503(&con->http, cptr->errcode);
		} else {
			http_build_res_504(&con->http, cptr->errcode);
		}
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

	case NKN_OM_REDIRECT:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		//SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);

		/* Create a new header and add loction header only.
		 * This avoids function http_build_res_302 from returning
		 * all headers in the request.
		 */
		{
			mime_header_t response_hdr;
			const char *data1;
			int datalen1;
			int hdrcnt1;
			u_int32_t attr1;

			init_http_header(&response_hdr, 0, 0);
			rv = get_known_header(&con->http.hdr, MIME_HDR_LOCATION, &data1, &datalen1, &attr1, &hdrcnt1);
			if (!rv) {
				rv = add_known_header(&response_hdr, MIME_HDR_LOCATION, data1, datalen1);
			}

			if (!rv) {
				http_build_res_302(&con->http, 0, &response_hdr);
			} else {
				DBG_LOG(MSG, MOD_HTTP,
					"add_known_header(MIME_HDR_CONTENT_LOCATION) "
					"failed, rv=%d", rv);
			}
                	http_close_conn(con, 0);
			shutdown_http_header(&response_hdr);
		}

		//http_build_res_302(&con->http, 0, &con->http.hdr);
		//http_close_conn(con, 0);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

	case NKN_OM_SSP_RETURN:
		if ((CHECK_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP) ||
	   	     CHECK_HTTP_FLAG(&con->http, HRF_MULTIPART_RESPONSE))) {
			SET_HTTP_FLAG(&con->http, HRF_ZERO_DATA_ERR_RETURN);
			break;
		}
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_close_conn(con, NKN_SSP_BAD_REMAPPED_URL);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

	case NKN_FOUND_BUT_NOT_VALID_YET:
		/* object found, presumably pinned, but cant be served yet */
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_403(&con->http, cptr->errcode);
		http_close_conn(con, cptr->errcode);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;

	case NKN_MM_REDIRECT_REQUEST:
		/* OM recieved a 302 redirect. Here we make sure we get the
		 * Location header, set the HTTP flag to indicate a REDIRECT
		 * response and re-schedule the task again so that we can reach
		 * out to the new origin and fetch the object.
		 */
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		/* Cant have more than MAX_HTTP_REDIRECT responses, otherwise
		 * we will end up in a loop. If hit the MAX, simply fall to
		 * the default case and let client handle in whatever way it wants.
		 * Note: "glob_max_302_redirect_limit" is configurable in case
		 * we want to raise the limit from a default of 5.
		 */
		if (con->http.redirect_count >= glob_max_302_redirect_limit) {
			goto next; /* jump out of NKN_MM_REDIRECT_REQUEST */
		}

		nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
		con->http.req_cod = NKN_COD_NULL;
		/* Counter to track number of redirect responses for this
		 * request.
		 */
		con->http.redirect_count++;

		/* Try to get the Location header.. if None was present.. where do
		 * we reach out for the new content??
		 */
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_LOCATION, &uri, &urilen,
				&attr, &hdrcnt);
		if (!rv) {
			SET_HTTP_FLAG(&con->http, HRF_302_REDIRECT);
			con->module_type = NORMALIZER;
			glob_normalize_302_redirect++;
			nkn_post_sched_a_task(con);
		}
		else {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_404(&con->http, cptr->errcode);
			http_close_conn(con, cptr->errcode);
		}

		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return; /* end of NKN_MM_REDIRECT_REQUEST */

	case NKN_OM_CI_END_OFFSET_RESPONSE:
		/* Post a task again for the actual cache index calculation */
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);

		nsconf = namespace_to_config(con->http.ns_token);
		if (nsconf == NULL) {
			http_close_conn(con, NKN_HTTP_CLOSE_CONN_RBCI_REFETCH);
			cr_free(cptr);
			NM_TRACE_UNLOCK(pnm, LT_SERVER);
			pthread_mutex_unlock(&pnm->mutex);
			return;
		}

		SET_CON_FLAG(con, CONF_CI_USE_END_OFF);

		con->http.content_length = cptr->length_response;

		if (nsconf->http_config->cache_index.checksum_from_end) {
			if (con->http.content_length <
				(2 * nsconf->http_config->cache_index.checksum_bytes)) {
				UNSET_CON_FLAG(con, CONF_REFETCH_REQ);
				goto next;
			}
		} else {
			UNSET_CON_FLAG(con, CONF_REFETCH_REQ);
			goto next;
		}

		ret = nkn_post_sched_a_task(con);
		if (ret != FALSE) {
			glob_resp_cache_index_tot_requests++;
			if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
				DBG_TRACE("Response cache-index - second request: URI:%s is ",
					  uri);
			}
		} else {
			UNSET_CON_FLAG(con, CONF_REFETCH_REQ);
			DBG_LOG(MSG, MOD_HTTP, "Post sched task to BM failed");
			goto next;
		}

		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return; /* end of NKN_OM_CONF_REFETCH_REQUEST */
	case NKN_OM_CONF_REFETCH_REQUEST:
	{
		int exceed_uri_size, tunnel_path;
		int md5_len;
		char new_uri[MAX_URI_SIZE];
		const char *org_uri;
		int org_urilen, ssp_ret_val;
		uint32_t attrs;
		int org_hdrcnt, resp_idx_offset;

		exceed_uri_size = tunnel_path = 0;
		md5_len = DEFAULT_CKSUM_LEN;
		resp_idx_offset = -1;
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);

		nsconf = namespace_to_config(con->http.ns_token);
		if (nsconf == NULL) {
			http_close_conn(con, NKN_HTTP_CLOSE_CONN_RBCI_REFETCH);
			NM_TRACE_UNLOCK(pnm, LT_SERVER);
			pthread_mutex_unlock(&pnm->mutex);
			cr_free(cptr);
			return;
		}

		nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
		con->http.req_cod = NKN_COD_NULL;

		if (dynamic_uri_enable) {
			ret = nkn_remap_uri(con, &resp_idx_offset);
			nsconf = con->http.nsconf;
			if ((ret && nsconf->http_config->request_policies.dynamic_uri.tunnel_no_match) ||
				(ret == URI_REMAP_TOKENIZE_ERR)) {
				if (ret != URI_REMAP_TOKENIZE_ERR) {
					con->http.tunnel_reason_code = NKN_TR_REQ_DYNAMIC_URI_REGEX_FAIL;
				}
				DBG_LOG(MSG, MOD_HTTP,
					"Dynamic URI failed in response cache index path."
					"Setup tunnel path");
				glob_tunnel_resp_cache_index_dyn_uri_err++;
				release_namespace_token_t(con->http.ns_token);
				goto next;
            }
			if (nsconf->http_config->request_policies.seek_uri.seek_uri_enable) {
				ret = nkn_tokenize_gen_seek_uri(con);
			}
			if (resp_idx_offset != -1) {
				cache_idx_scheme = CACHE_INDEX_DYN_URI;
			} else {
				cache_idx_scheme = CACHE_INDEX_DEFAULT_SCHEME;
			}
        }

		/* Extend MD5 bytes length to concatenate two
		 * checksum(header+data) values.
		 */
		if (nsconf->http_config->cache_index.include_header &&
			nsconf->http_config->cache_index.include_object &&
			(is_known_header_present(&con->http.hdr,
					  MIME_HDR_X_NKN_CLUSTER_TPROXY)==0)) {
			md5_len *= 2;
		}

		// Fetch the uri from mime header.
		if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			&org_uri, &org_urilen, &attrs, &org_hdrcnt)) {
			if (CHECK_HTTP_FLAG(&con->http, HRF_HAS_HEX_ENCODE) ||
			    get_known_header(&con->http.hdr, MIME_HDR_X_NKN_DECODED_URI,
				&org_uri, &org_urilen, &attrs, &org_hdrcnt)) {
				if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
					&org_uri, &org_urilen, &attrs, &org_hdrcnt)) {
					DBG_LOG(MSG, MOD_HTTP,
						"No URI found, uri=%p", org_uri);
					glob_tunnel_resp_cache_index_no_uri++;
					release_namespace_token_t(
						con->http.ns_token);
					goto next;
				}
			}
		}

		// If default scheme is set, apply sanity check on uri length
		// against MAX_URI_SIZE.
		if ((CACHE_INDEX_DEFAULT_SCHEME == cache_idx_scheme) ||
			(CACHE_INDEX_DYN_URI == cache_idx_scheme)) {
			if ((nsconf->http_config->cache_index.include_header &&
				nsconf->http_config->cache_index.include_object) &&
				(is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY)==0) &&
				((org_urilen+64) > (MAX_URI_SIZE - MAX_URI_HDR_SIZE - 1))) {
				glob_tunnel_resp_cache_index_exceed_uri_size_2++;
				exceed_uri_size = 1;
				DBG_LOG(MSG, MOD_HTTP,
					"Cache index URI size(%d) exceeds the maximum uri size(%d)"
					"for header and object based cache indexing",
					(org_urilen+64), MAX_URI_SIZE);
			} else if ((nsconf->http_config->cache_index.include_header ||
				nsconf->http_config->cache_index.include_object) &&
				(is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY)==0) &&
				((org_urilen+32) > (MAX_URI_SIZE - MAX_URI_HDR_SIZE - 1))) {
				glob_tunnel_resp_cache_index_exceed_uri_size_1++;
				exceed_uri_size = 1;
				DBG_LOG(MSG, MOD_HTTP,
					"Cache index URI size(%d) exceeds the maximum uri size(%d)"
					"for header or object based cache indexing",
					(org_urilen+32), MAX_URI_SIZE);
			}
		}
		release_namespace_token_t(con->http.ns_token);
		nsconf = NULL;

		if (!exceed_uri_size) {
			// Perform the actual cache index URI generation.
			tunnel_path = get_cache_index_uri(org_uri, org_urilen,
							&cptr->out_proto_resp.u.OM_cache_idx_data.cksum[0],
							cache_idx_scheme, resp_idx_offset, md5_len, &new_uri[0]);
		} else {
			con->http.tunnel_reason_code = NKN_TR_RES_CACHE_IDX_URI_MAX_SIZE_LIMIT;
		}

		// cache uri length exceeds max uri size or cache index construction error.
		// Tunnel the requests.
		if (tunnel_path || exceed_uri_size) {
			goto next;
		}

		DBG_LOG(MSG, MOD_HTTP,
			"Original URI:%s is remapped to cache-index URI:%s",
			uri, new_uri);
		org_urilen = strlen(new_uri);
		if (!is_hex_encoded_uri(new_uri, org_urilen)) {
			SET_HTTP_FLAG2(&con->http, HRF2_RBCI_URI);
		} else {
			con->http.tunnel_reason_code = NKN_TR_RES_HEX_ENCODED;
			goto next;
		}
		add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				 new_uri, org_urilen);
		con->module_type = NORMALIZER;
		glob_normalize_remapped_uri++;

		// Apply SSP
		ret = http_apply_ssp(con, &ssp_ret_val);
		if ((ssp_ret_val < 0) || (ssp_ret_val == 3)) {
			DBG_LOG(MSG, MOD_HTTP,
				"SSP returns error. Tunnel setup/response sent to client, ret=%d, "
				"ssp_ret_val=%d",
				ret, ssp_ret_val);
			glob_resp_cache_index_ssp_return_or_fail++;
			cr_free(cptr);
			NM_TRACE_UNLOCK(pnm, LT_SERVER);
			pthread_mutex_unlock(&pnm->mutex);
			return;
		}

		con->http.req_cod = http_get_req_cod(con);

		if (con->http.req_cod == NKN_COD_NULL) {
			// COD open failure. Take up tunnel path.
			DBG_LOG(MSG, MOD_HTTP,
				"Response based cache index uri COD open error");
			glob_resp_cache_index_cod_open_err++;
			goto next;
		}
		ret = nkn_post_sched_a_task(con);
		if (ret != FALSE) {
			glob_resp_cache_index_tot_requests++;
			if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
				DBG_TRACE("Response cache-index - second request: URI:%s is "
					  "remapped to cache-index URI:%s",
					  uri, new_uri);
			}
		} else {
			 DBG_LOG(MSG, MOD_HTTP, "Post sched task to BM failed");
			 goto next;
		}
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return; /* end of NKN_OM_CONF_REFETCH_REQUEST */
	}
	case NKN_OM_WWW_AUTHENTICATE_NTLM:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		con->http.tunnel_reason_code = NKN_TR_RES_WWW_AUTHENTICATE_NTLM;
		goto next;

	case NKN_OM_CONNECTION_UPGRADE:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
		con->http.tunnel_reason_code = NKN_TR_RES_CONNECTION_UPGRADE;
		goto next;

 	case NKN_MM_OBJ_NOT_FOUND_IN_CACHE:
		con->http.tunnel_reason_code = NKN_TR_RES_NOT_IN_CACHE;
	    	if ( CHECK_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP) &&
		     !CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) ) {
			SET_HTTP_FLAG(&con->http, HRF_ZERO_DATA_ERR_RETURN);
			break;
		}
		// This will fall through

	case NKN_OM_NON_CACHEABLE:
	case NKN_SERVER_BUSY:
	default:
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		nkn_cleanup_a_task(con);
next:
		nsconf = namespace_to_config(con->http.ns_token);
		if (nsconf == NULL) {
                    cr_free(cptr);
		    NM_TRACE_UNLOCK(pnm, LT_SERVER);
                    pthread_mutex_unlock(&pnm->mutex);

		    http_close_conn(con, NKN_HTTP_CLOSE_CONN);
		    return;
		}

		ret = cptr->errcode;
		if ((cptr->in_rflags & CR_RFLAG_FIRST_REQUEST) &&
		  (nsconf->http_config->origin.select_method != OSS_RTSP_CONFIG) &&
		  (nsconf->http_config->origin.select_method != OSS_RTSP_DEST_IP)) {

                        int sockfd = cptr->out_proto_resp.u.OM.cp_sockfd;
                        unsigned int incarn = cptr->out_proto_resp.u.OM.incarn;


                        if ((con->fd == cptr->out_proto_resp.u.OM.httpcon_sockfd)
                            && (gnm[sockfd].incarn == incarn)) {
                                /* Already copied OM's cp sockfd and incarn. */
			}
			else {
				sockfd = -1;
				incarn = 0;
			}

			/*
			 * most likely BM will change request byte-range, don't
			 * pick up existing OM socket connection.
			 *
			 * Notes for BZ 5287: In case of requests that have a
			 * byte range, is it possible that BM modifies the
			 * request range so as to align to a page boundary.
			 * This modified request, if sent to Origin, may have
			 * returned with a response that is marked as
			 * NON_CACHEABLE - in this case, the file was not found
			 * and OM tries to tunnel it to the client. Since the
			 * original request may have been already modified by
			 * BM, we cannot tunnel back the response as is. Hence
			 * we need to tunnel the original request received
			 * from the client to the origin server.
			 *
			 * This next line of code closes the cp_sockfd (as we
			 * no longer) need it. And also resets the request
			 * offset to 0, so that when a tunnel is made, the
			 * original request is sent as is.
			 */
			if ((sockfd!= -1) && CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE)) {
				cp_add_event(sockfd, OP_CLOSE, NULL);
				sockfd = -1;
				con->http.cb_offsetlen = 0;
			}
			/*
			 * for any internal error code,
			 * we reuse tunnel code to forward the data to origin server.
			 */
		  	switch(nsconf->http_config->origin.select_method) {
			case OSS_HTTP_NODE_MAP:
			    if ((cptr->errcode != NKN_OM_NON_CACHEABLE) &&
			    	(cptr->errcode != NKN_BUF_VERSION_EXPIRED) &&
			    	(cptr->errcode != NKN_MM_OBJ_NOT_FOUND_IN_CACHE) &&
				(cptr->errcode != NKN_OM_CONF_REFETCH_REQUEST)) {
			    	break;
			    }
			    // Fall through
			default:
				if (con->http.tunnel_reason_code == 0) {
					con->http.tunnel_reason_code = NKN_MAX_SUB_ERROR_CODE + cptr->errcode;
				}
			    /* Tunneling.. so reduce the session count for this
			     * namespace
			     */
				if (CHECK_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED)) {
                                    cleanSSP_force_tunnel(con);
                                }

			    if (!CHECK_HTTP_FLAG(&con->http,
						 HRF_CL_L7_PROXY_REQUEST)) {
			    	ret = fp_make_tunnel(con, sockfd, incarn, 0);
			    } else {
			    	if (sockfd < 0) {
					delete_known_header(&con->http.hdr,
						MIME_HDR_X_NKN_CL7_PROXY);

				}
#ifndef CL7_USE_NODE_NS
			    	ret = fp_make_tunnel(con, sockfd, incarn, 0);
#else
			    	L7_SWT_NODE_NS(&con->http);
			    	ret = fp_make_tunnel(con, sockfd, incarn, 0);
				if (ret) { // Tunnel failed
			    		L7_SWT_PARENT_NS(&con->http);
				}
#endif
			    }
			    if (ret == 0) {
				glob_tunnel_non_cacheable++;
			    }
			}
		}
		ns_token = con->http.ns_token;
		if (ret != 0) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		  	if ((nsconf->http_config->origin.select_method == OSS_RTSP_CONFIG) ||
			    (nsconf->http_config->origin.select_method == OSS_RTSP_DEST_IP)) {
			    http_build_res_501(&con->http, ret);
			}
			http_close_conn(con, ret);
		}
		release_namespace_token_t(ns_token);
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
	}
	}

	if (cptr->attr) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE) || (CHECK_HTTP_FLAG(&con->http, HRF_BYTE_SEEK))) {
			if (cptr->attr->na_flags2 & NKN_OBJ_NEGCACHE) {
				UNSET_CON_FLAG(con, CONF_TASK_POSTED);
				nkn_cleanup_a_task(con);
				CLEAR_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
				CLEAR_HTTP_FLAG(&con->http, HRF_BYTE_SEEK);
				nkn_post_sched_a_task(con);
				cr_free(cptr);
				NM_TRACE_UNLOCK(pnm, LT_SERVER);
				pthread_mutex_unlock(&pnm->mutex);
				return;
			}
		}
		if (cptr->in_rflags & CR_RFLAG_DELIVER_EXP &&
			cptr->attr->cache_expiry <= nkn_cur_ts) {
			SET_HTTP_FLAG2(&con->http, HRF2_EXPIRED_OBJ_DEL);
		}

		// Vary header processing
		// Check if Vary header attribute is set and if this not a vary refetch request
		if (nkn_attr_is_vary(cptr->attr) && !CHECK_HTTP_FLAG2(&con->http, HRF2_VARY_REQUEST)) {
			mime_header_t response_hdr;
			const char *ua_str, *req_ua;
			int ua_len, req_ua_len;

			// load headers from attribute buffer
			init_http_header(&response_hdr, 0, 0);
			ret = nkn_attr2http_header(cptr->attr, 0, &response_hdr);
			if (!ret) {
				// Get Vary header
				//if (is_known_header_present(&response_hdr, MIME_HDR_VARY)) {
				//	ret = get_known_header(&response_hdr,
				//		MIME_HDR_VARY,
				//		&ua_str, &ua_len, &attr, &hdrcnt);
				//}
				// If X_NKN_USER_AGENT was present it is User Agent based else
				//   based on Accept encoding. Only these two are supported.
				if (is_known_header_present(&response_hdr, MIME_HDR_X_NKN_USER_AGENT)) {
					int cache_ua_grp = -1;
					int req_ua_grp = -1;
					
					// Get Stored user agent and get its group
					ret = get_known_header(&response_hdr,
						MIME_HDR_X_NKN_USER_AGENT,
						&ua_str, &ua_len, &attr, &hdrcnt);
					if (!ret)
						cache_ua_grp = get_user_agent_group(con, ua_str, ua_len);

					// Get request user agent and get its group
					ret = get_known_header(&con->http.hdr, 
						MIME_HDR_USER_AGENT,
						&req_ua, &req_ua_len, &attr, &hdrcnt);
					if (!ret)
						req_ua_grp = get_user_agent_group(con, req_ua, req_ua_len);

					/* Check if both groups match
					 * If they match serve the current object
					 * Else change the cache index for the group
					 * and fetch the object from origin server
					 */
#if 0
					ret = get_known_header(&con->http.hdr, 
						MIME_HDR_X_NKN_REMAPPED_URI,
						&req_ua, &req_ua_len, &attr, &hdrcnt);
					ret = get_known_header(&con->http.hdr, 
						MIME_HDR_X_NKN_DECODED_URI,
						&req_ua, &req_ua_len, &attr, &hdrcnt);
					ret = get_known_header(&con->http.hdr, 
						MIME_HDR_X_NKN_URI,
						&req_ua, &req_ua_len, &attr, &hdrcnt);
#endif
					// Change cache index and start a new request
					if ((cache_ua_grp != req_ua_grp) && 
							!mime_hdr_get_cache_index(&con->http.hdr,
							&req_ua, &req_ua_len, &attr, &hdrcnt)) {
						char   new_uri[2 * MAX_URI_SIZE];
						int    new_uri_len = 0 ;

						strncpy(new_uri, req_ua, req_ua_len);
						new_uri[req_ua_len] = 0;
						if (req_ua_grp == -1) {
							// Object in cache index is having some 
							//   other user agent
							// Tunnel the request.
							UNSET_CON_FLAG(con, CONF_TASK_POSTED);
							nkn_cleanup_a_task(con);
							con->http.tunnel_reason_code = NKN_TR_REQ_VARY_USER_AGENT_MISMATCH;
							glob_tunnel_vary_user_agent_mismatch++;
							goto next;
						}
						else {
							// Append the cache index with user agent group
							strcat(new_uri, usr_agent_cache_index[req_ua_grp] );
						}
						new_uri_len = req_ua_len + 3;
						add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
								 new_uri, new_uri_len);
						SET_HTTP_FLAG2(&con->http, HRF2_VARY_REQUEST);
						shutdown_http_header(&response_hdr);

						UNSET_CON_FLAG(con, CONF_TASK_POSTED);
						nkn_cleanup_a_task(con);
						/*
						 * This is not the required Object, restart the 
						 * operation with new cache index.
						 */
						nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
						con->http.req_cod = NKN_COD_NULL;
						
						glob_http_vary_user_agent_mismatch++;
						ret = nkn_post_sched_a_task(con);
						cr_free(cptr);
						NM_TRACE_UNLOCK(pnm, LT_SERVER);
						pthread_mutex_unlock(&pnm->mutex);
						return;
					}
					else if ((req_ua_grp == -1) && (cache_ua_grp == req_ua_grp)) {
						if (ua_len != req_ua_len || strncasecmp(ua_str, req_ua, ua_len) != 0) {
							// Object in cache is having some 
							//   other user agent
							// Tunnel the request.
							UNSET_CON_FLAG(con, CONF_TASK_POSTED);
							nkn_cleanup_a_task(con);
							con->http.tunnel_reason_code = NKN_TR_REQ_VARY_USER_AGENT_MISMATCH;
							glob_tunnel_vary_user_agent_mismatch++;
							goto next;
						}
					}
				}
				else {
					// Vary on Accept encoding
					if (con->http.http_encoding_type) {
					if (con->http.http_encoding_type & (HRF_ENCODING_GZIP 
							| HRF_ENCODING_DEFLATE)) {
						con->http.http_encoding_type |= (HRF_ENCODING_GZIP 
							| HRF_ENCODING_DEFLATE);
					}
					if (!(cptr->attr->encoding_type & con->http.http_encoding_type)) {
						char   new_uri[2 * MAX_URI_SIZE];
						int    new_uri_len = 0 ;

						mime_hdr_get_cache_index(&con->http.hdr,
							&req_ua, &req_ua_len, &attr, &hdrcnt);

						strncpy(new_uri, req_ua, req_ua_len);
						new_uri[req_ua_len] = 0;
						new_uri_len = req_ua_len + 3;

						//Check request encoding type
						if (con->http.http_encoding_type &(HRF_ENCODING_GZIP 
							| HRF_ENCODING_DEFLATE)) {
							strcat(new_uri, encoding_cache_index[1] );
						}
						else if (con->http.http_encoding_type & HRF_ENCODING_COMPRESS) {
							strcat(new_uri, encoding_cache_index[2] );
						}
						else if (con->http.http_encoding_type & HRF_ENCODING_BZIP2) {
							strcat(new_uri, encoding_cache_index[3] );
						}
						else {
							strcat(new_uri, encoding_cache_index[0] );
						}
						add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
								 new_uri, new_uri_len);
						SET_HTTP_FLAG2(&con->http, HRF2_VARY_REQUEST);
						shutdown_http_header(&response_hdr);

						UNSET_CON_FLAG(con, CONF_TASK_POSTED);
						nkn_cleanup_a_task(con);
						/*
						 * This is not the required Object, restart the 
						 * operation with new cache index.
						 */
						nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
						con->http.req_cod = NKN_COD_NULL;
						
						glob_http_vary_encoding_mismatch++;
						ret = nkn_post_sched_a_task(con);
						cr_free(cptr);
						NM_TRACE_UNLOCK(pnm, LT_SERVER);
						pthread_mutex_unlock(&pnm->mutex);
						return;
					}
					}
				}
			}
		}

		con->http.cache_encoding_type = cptr->attr->encoding_type;
		/*
		 * cptr->attr->encoding_type = 0 means data is uncompressed type
		 * It can be served to any client. con->http.http_encoding_type = 0
		 * means client can accept only uncompressed data. so if the data
		 * is compressed type then it should be checked with client's
		 * request type. If there is a mismatch, it should be tunnelled.
		 */
		if (cptr->attr->encoding_type) {
			if (con->http.http_encoding_type &&
				(cptr->attr->encoding_type & con->http.http_encoding_type)) {
				if (cptr->attr->na_flags & (NKN_OBJ_COMPRESSED)) {
					NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, compress_cnt);
					mime_header_t response_hdr;
					const char *org_len_str;
		                        int org_len;

					init_http_header(&response_hdr, 0, 0);
				        ret = nkn_attr2http_header(cptr->attr, 0, &response_hdr);
		                        if (!ret) {
				          if (is_known_header_present(&response_hdr, MIME_HDR_X_NKN_UNCOMPRESSED_LENGTH)) {
		                              ret = get_known_header(&response_hdr,
			                            MIME_HDR_X_NKN_UNCOMPRESSED_LENGTH,
				                    &org_len_str, &org_len,
					            &attr, &hdrcnt);
		                              if (!ret) {
						int content_len = atoi(org_len_str);
						if(content_len > (int)cptr->attr->content_length) {
							con->http.nsconf->stat->client[PROTO_HTTP].compress_bytes_save += (
									content_len-(int)cptr->attr->content_length);
						} else {
							con->http.nsconf->stat->client[PROTO_HTTP].compress_bytes_overrun += (
									(int)cptr->attr->content_length)-content_len;
						}
			                      }
					}

					shutdown_http_header(&response_hdr);
				      }
				}
			} else {
				UNSET_CON_FLAG(con, CONF_TASK_POSTED);
				nkn_cleanup_a_task(con);
				con->http.tunnel_reason_code = NKN_TR_REQ_NOT_SUPPORTED_ENCODING;
				glob_http_encoding_mismatch++;
				goto next;
			}
		}
	}

	NM_TRACE_UNLOCK(pnm, LT_SERVER);
	pthread_mutex_unlock(&pnm->mutex);

	if (CHECK_CON_FLAG(con, CONF_TASK_TIMEOUT)) {
		UNSET_CON_FLAG(con, CONF_TASK_TIMEOUT);
		AO_fetch_and_sub1(&glob_err_timeout_with_task);
		SET_HTTP_FLAG(&con->http, HRF_SEP_REQUEST);
	}


	// New design:
	// avoid to get the mutex lock in the scheduler thread
	// This new design is only push the task into http_mgr_output_q
	// then immediately returns.

	// Insert the task into a queue
	phmoq = (http_mgr_output_q_t *)nkn_malloc_type(sizeof(http_mgr_output_q_t), mod_http_mgr_output_q_t);
	if ( !phmoq ) {

		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_SERVER);
		if ( NM_CHECK_FLAG(pnm, NMF_IN_USE) && (id == con->nkn_taskid) ) {
			con = (con_t *)pnm->private_data;
			UNSET_CON_FLAG(con, CONF_TASK_POSTED);
			nkn_cleanup_a_task(con);
			http_close_conn(con, NKN_HTTP_MALLOC);
		}
		cr_free(cptr);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
	}

	phmoq->cptr=cptr;
	phmoq->pnm=pnm;
	phmoq->id=id;
	phmoq->next = NULL;

	/*
	 * Update the counter in single thread
	 * Otherwise we will have to introduce a separate mutex for counters
	 * among multiple network threads.
	 */
	AO_fetch_and_add(&glob_tot_bytes_tobesent, cptr->length_response);
	DBG_LOG(MSG, MOD_HTTP,
		"scheduler returns socket %d a task %ld with total length %d.",
		con->fd, id, cptr->length_response);

	/*
	 * append phmoq into the end of hmoq.
	 * Each queue is protected by mutex
	 * Each queue should match to the epoll thread
	 */
        num = pnm->pthr->num;
        if (nm_hdl_send_and_receive) {
		add_hmoq_item(num, pnm, phmoq);
        } else {
                pthread_mutex_lock(&hmoq_mutex[num]);
                if (hmoq_plast[num] == NULL) {
                        hmoq_head[num] = phmoq;
                }
                else {
                        hmoq_plast[num]->next = phmoq;
                }
                hmoq_plast[num] = phmoq;
                pthread_cond_signal(&hmoq_cond[num]);
                pthread_mutex_unlock(&hmoq_mutex[num]);
        }
}

#define MAX_HMOQ_PROCESSING_ENTIRES (500)
int check_out_hmoq(int num_of_epoll)
{
	http_mgr_output_q_t * phmoq;
	struct network_mgr * pnm;
	cache_response_t * cptr;
	con_t *con;
        int i;
	int32_t len;
	int processed_entries = 0;
	nkn_task_id_t id;
	off_t total_bytes_received;

again:

	total_bytes_received = 0;
        if (processed_entries >= MAX_HMOQ_PROCESSING_ENTIRES) {
                return processed_entries;
        }

	phmoq = remove_hmoq_item (num_of_epoll);
	if (phmoq == NULL)
		return processed_entries;
	cptr = phmoq->cptr;
	pnm = phmoq->pnm;
	id = phmoq->id;
	free(phmoq);
	processed_entries++;

	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_SERVER);
	if ( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		cr_free(cptr);
		goto again;
	}

	// Process this task
	con = (con_t *)pnm->private_data;
	UNSET_CON_FLAG(con, CONF_TASK_POSTED);

	/*
	 * avoid the same pnm structure has been reused by different connection
	 * while we are blocked in getting above mutex lock.
	 */
	if (id != con->nkn_taskid) {
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		cr_free(cptr);
		goto again;
	}

	/*
	 * When we got a singal that socket has been closed/reset by peer
	 * We still let the task complete.
	 * Upon the return of the task, we close the socket.
	 * Right now it is good time to close the socket.
	 */
	if (CHECK_CON_FLAG(con, CONF_CANCELD)) {
		glob_cm_err_cancel ++;
		cr_free(cptr);
		nkn_cleanup_a_task(con);
		close_conn(con);
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		goto again;
	}

	/*
	 * Process OM out protocol response data.
	 * Note: Data is null if request resolved by BM.
	 */
	if (cptr->in_rflags & CR_RFLAG_FIRST_REQUEST) {
		con->http.respcode = cptr->out_proto_resp.u.OM.respcode;
		//comment this out Bug#4966
		//if (cptr->provider == OriginMgr_provider){
		//	con->http.remote_ip = cptr->out_proto_resp.u.OM.remote_ip;
		//	con->http.remote_port = cptr->out_proto_resp.u.OM.remote_port;
		//}
		//Get the first tick count here
		gettimeofday(&(con->http.ttfb_ts), NULL);
	}
	if (cptr->in_rflags & CR_RFLAG_RET_REVAL) {
		SET_HTTP_FLAG(&con->http, HRF_RETURN_REVAL_CACHE);
	}
	if (con->http.first_provider == Unknown_provider) {
		con->http.first_provider = cptr->provider;
	}
	con->http.provider = cptr->provider;
	if (cptr->out_proto_resp.u.OM.flags & OM_PROTO_RESP_NO_CONTENT_LEN) {
		SET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
		SET_HTTP_FLAG(&con->http, HRF_NO_CACHE);
	}

	/*
	 * For Accesslog purpose only. Backup all information from cptr to con
	 */
        for (i=0; (i<cptr->num_nkn_iovecs); i++) {

		len = cptr->content[i].iov_len;
		total_bytes_received += len;

		con->http.tot_reslen = total_bytes_received;
		// normal work to adjust the pointers
		con->http.content[i].iov_base=cptr->content[i].iov_base;
		con->http.content[i].iov_len=len;
        }

	/*
	 * status update
	 */
	if (cptr->provider == OriginMgr_provider) {
		con->http.size_from_origin += total_bytes_received;
	}
#if 0
	/*
	 * Bug 1576: For AFR calculation, treat Origin Manager response same
	 * as others.  So comment out the next lines.
	 */
	if (cptr->provider == OriginMgr_provider) {
		/*
		 * Ideal design is: we should reserve the bandwidth for OM
		 * response transaction until connection is closed.
		 * The code here is: we make no reservation for bandwidth
		 * as soon as we detect it is the response from OriginMgr.
		 */

		// We disabled ABR/Session Bandwidth if configured for this case.
		con->max_bandwidth = 0;
		con->min_afr = 0;
	}
#endif // 0
	con->http.num_nkn_iovecs=cptr->num_nkn_iovecs;

	if (CHECK_CON_FLAG(con, CONF_NO_CONTENT_LENGTH)) {
	    // Temporary fix for Bug 1587
	    // assert(cptr->provider == OriginMgr_provider);
	    	if (cptr->out_proto_resp.u.OM.flags &
			OM_PROTO_RESP_NO_CONTENT_LEN_EOF) {
			con->http.content_length =
				con->sent_len + total_bytes_received;
		} else {
			con->http.content_length = OM_STAT_SIG_TOT_CONTENT_LEN;
		}
		SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);
		/* Update counter */
		if (cptr->in_rflags & CR_RFLAG_FIRST_REQUEST) {
			switch(con->http.respcode) {
			case 200:
				if (!(CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ))) {
					glob_http_tot_200_resp++;
					NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_2xx);
					NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_200);
				}
				break;
			case 206:
				AO_fetch_and_add1(&glob_http_tot_206_resp);
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_2xx);
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_206);
				break;
			case 302:
				AO_fetch_and_add1(&glob_http_tot_302_resp);
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_3xx);
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_302);
				break;
			case 304:
				glob_http_tot_304_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_3xx);
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_304);
				break;
			case 400:
				glob_http_tot_400_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_4xx);
				break;
			case 404:
				glob_http_tot_404_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_4xx);
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_404);
				break;
			case 413:
				glob_http_tot_413_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_4xx);
				break;
			case 414:
				glob_http_tot_414_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_4xx);
				break;
			case 416:
				glob_http_tot_416_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_4xx);
				break;
			case 500:
				glob_http_tot_500_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_5xx);
				break;
			case 501:
				glob_http_tot_501_resp++;
				NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, resp_5xx);
				break;
			default:
				break;
			}
		}

	}
	else if (cptr->attr) {
		con->http.attr = cptr->attr;
		con->http.object_hotval = cptr->attr->hotval;
		con->http.content_length = cptr->attr->content_length;
		con->http.obj_create = cptr->attr->origin_cache_create;
		con->http.obj_expiry = cptr->attr->cache_expiry;
	}

	nkn_record_hit_history (con, cptr, total_bytes_received);

	if (cptr->out_proto_resp.u.OM.flags & OM_PROTO_RESP_CONTENT_LENGTH) {
		// Tunnel response case
		con->http.content_length =
			cptr->out_proto_resp.u.OM.content_length;
		SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);
	}

	cr_free(cptr);
	//nkn_cleanup_a_task(con);

	// Time to send data out
	//printf("http_mgr_output: fd=%d\n", con->fd);
	http_try_to_sendout_data(con);
	//NM_add_event_epollout(con->fd);

	NM_TRACE_UNLOCK(pnm, LT_SERVER);
	pthread_mutex_unlock(&pnm->mutex);
	goto again;
}

/*
 * check_out_tmrq() - Perform NM_func_timer initiated actions on the net thread.
 */
void check_out_tmrq(int num_of_epoll)
{
	http_timer_req_q_t *ptmr;
	network_mgr_t *pnm;
	con_t *con;
	const namespace_config_t *nsconf;
	int init_in_progress;
	int retries;
	int rv;

	while (1) { /* Begin while */

	pthread_mutex_lock(&tmrq_mutex[num_of_epoll]);
	if (tmrq_head[num_of_epoll] == NULL) {
		tmrq_plast[num_of_epoll] = NULL;
		pthread_mutex_unlock(&tmrq_mutex[num_of_epoll]);
		break;
	}
	ptmr = tmrq_head[num_of_epoll];
	tmrq_head[num_of_epoll] = tmrq_head[num_of_epoll]->next;
	if (tmrq_head[num_of_epoll] == NULL) {
		tmrq_plast[num_of_epoll] = NULL;
	}
	pthread_mutex_unlock(&tmrq_mutex[num_of_epoll]);

	pnm = &gnm[ptmr->fhd.fd];
	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_SERVER);
	if (!NM_CHECK_FLAG(pnm, NMF_IN_USE) ||
	    (pnm->incarn != ptmr->fhd.incarn)) {
		NM_TRACE_UNLOCK(pnm, LT_SERVER);
		pthread_mutex_unlock(&pnm->mutex);
		free(ptmr);
		glob_http_tmrq_ignored++;
		continue;
	}

	switch(ptmr->type) {
	case TMRQ_EVENT_NAMESPACE_WAIT:
		glob_http_tmrq_processed++;
		init_in_progress = 0;
		con = (con_t*)pnm->private_data;
		nsconf = namespace_to_config(con->http.ns_token);
		if (nsconf) {
		    init_in_progress = nsconf->init_in_progress;
		}
		release_namespace_token_t(con->http.ns_token);

		if (init_in_progress) {
		    con->ns_wait_intervals++;
		    if (con->ns_wait_intervals < NS_INIT_WAIT_MAX_INTERVALS) {
			/* Rearm timer */
			retries = 1;
			con->timer_event = TMRQ_EVENT_NAMESPACE_WAIT;
			while ((rv = net_set_timer(ptmr->fhd,
						  NS_INIT_WAIT_INTERVAL,
						  TT_ONESHOT)) < 0) {
			    if ((retries--) == 0) {
				break;
			    }
			}
			if (!rv) {
			    break; // Success
			} else {
			    con->timer_event = TMRQ_EVENT_UNDEFINED;
			}
		    }
		}
		nkn_post_sched_a_task(con);
		break;

	case TMRQ_EVENT_NAMESPACE_WAIT_2:
		glob_http_tmrq2_processed++;
		init_in_progress = 0;
		con = (con_t*)pnm->private_data;
		nsconf = namespace_to_config(con->http.ns_token);
		if (nsconf) {
		    init_in_progress = nsconf->init_in_progress;
		}
		release_namespace_token_t(con->http.ns_token);

		if (init_in_progress) {
		    con->ns_wait_intervals++;
		    if (con->ns_wait_intervals < NS_INIT_WAIT_MAX_INTERVALS) {
			/* Rearm timer */
			retries = 1;
			con->timer_event = TMRQ_EVENT_NAMESPACE_WAIT_2;
			while ((rv = net_set_timer(ptmr->fhd,
						  NS_INIT_WAIT_INTERVAL,
						  TT_ONESHOT)) < 0) {
			    if ((retries--) == 0) {
				break;
			    }
			}
			if (!rv) {
			    break; // Success
			} else {
			    con->timer_event = TMRQ_EVENT_UNDEFINED;
			}
		    }
		}
		release_namespace_token_t(con->http.ns_token); // rel final ref
		con->http.ns_token = NULL_NS_TOKEN;

		// Retry request
		SET_CON_FLAG(con, CONF_RETRY_HDL_REQ);
		http_handle_request(con);
		break;
	default:
		glob_http_tmrq_invalid++;
	   	break;
	}
	NM_TRACE_UNLOCK(pnm, LT_SERVER);
	pthread_mutex_unlock(&pnm->mutex);
	free(ptmr);

	} /* End while */
}

/*
 * tmrq_add_event() -  Add event to timeout request queue on behalf of
 *		       the associated con_t NM_func_timer handler.
 *
 * Assumptions:
 *  1) Caller holds the associated gnm[] mutex.
 */
static int
tmrq_add_event(int fd, tmrq_event_type_t type)
{
	/* Caller holds the associated gnm[] mutex */
	http_timer_req_q_t *ptmr;
	int num;

	ptmr = (http_timer_req_q_t *)
	    nkn_malloc_type(sizeof(http_timer_req_q_t), mod_http_timer_req_q_t);
	if (!ptmr) {
		glob_http_tmrq_event_fails++;
		return 1; // malloc failed
	}
	ptmr->next = NULL;
	ptmr->fhd = NM_fd_to_fhandle(fd);
	ptmr->type = type;

	num = gnm[fd].pthr->num;
	pthread_mutex_lock(&tmrq_mutex[num]);
	if (tmrq_plast[num] == NULL) {
		tmrq_head[num] = ptmr;
	} else {
		tmrq_plast[num]->next = ptmr;
	}
	tmrq_plast[num] = ptmr;
	pthread_mutex_unlock(&tmrq_mutex[num]);

	glob_http_tmrq_events++;
	return 0;
}

void server_timer_1sec(void)
{
	int next_datestr_idx;
	date_str_t *next;

	if (cur_datestr_idx) {
		next_datestr_idx = 0;
	} else {
		next_datestr_idx = 1;
	}
	next = &cur_datestr_array[next_datestr_idx];
	time(&nkn_cur_ts);
	glob_nkn_cur_ts = nkn_cur_ts;
	mk_rfc1123_time(&nkn_cur_ts, next->date, sizeof(cur_datestr_array[0].date), &next->length);
	cur_datestr_idx = next_datestr_idx;

	// For HTTP GET attack counters
	glob_tot_get_in_this_second=0;
}

inline char *nkn_get_datestr(int *datestr_len)
{
	date_str_t *cur = &cur_datestr_array[cur_datestr_idx];
	if (datestr_len) {
		*datestr_len = cur->length;
	}

	return cur->date;
}

/*
 * return 0 is a valid possible case.
 * only return -1 is error.
 */
int nkn_check_http_cb_buf(http_cb_t * phttp)
{
	int rlen;
	char * p;
	int size;

	rlen = phttp->cb_max_buf_size - phttp->cb_totlen;
	if (rlen <= 10) {
		glob_enlarge_cb_size++;
		if (phttp->cb_max_buf_size < TYPICAL_HTTP_CB_BUF) {
			assert(0);
		}
		size = phttp->cb_max_buf_size * 2;
		if (size > max_http_req_size) {
			return rlen;
		}
		p = (char *)nkn_realloc_type(phttp->cb_buf, size + 1, mod_network_cb);
		if (p == NULL) {
			return -1;
		}
		phttp->cb_max_buf_size = size;
		phttp->cb_buf = p;
		/*
		 * Bug 3700 we have changed http hdrs
		 * not to init to cb_buf memoryi
		 * */
		reset_http_header(&phttp->hdr, NULL, 0);
		rlen = phttp->cb_max_buf_size - phttp->cb_totlen;
	}
	return rlen;
}

/* Bug 6558:
 * Enforce RFC 2616, Section 10.4.14
 * 413 Request Entity Too Large
 *
 * The server is refusing to process a request because the request entity is
 * larger than the server is willing or able to process. The server MAY
 * close the connection to prevent the client from continuing the request.
 *
 * If the condition is temporary, the server SHOULD include a Retry-After
 * header field to indicate that it is temporary and after what time the
 * client MAY try again.
 */
static inline int send_err_on_large_request(con_t* con)
{
    http_build_res_413(&con->http, 0);
    if (con->http.res_hdlen) {
	http_send_data(con, con->http.resp_buf,
			 con->http.res_hdlen);
	con->http.res_hdlen = 0;
    }

    return 0;
}

/*
 * ===============> http socket <===============================
 * The following functions are designed for normal http socket.
 * we need to provde epollin/epollout/epollerr/epollhup functions
 */

static int http_epollin(int sockfd, void * private_data)
{
	con_t * con = (con_t *)private_data;
	int rlen;
	int n;
	int err = 0;
	int ret = -1;
	struct tcp_info tcp_con_info;

	NM_EVENT_TRACE(sockfd, FT_HTTP_EPOLLIN);
	/*
	 * We do not support pipeline request and POST method for now
	 */
	if ( CHECK_CON_FLAG(con, CONF_BLOCK_RECV) ||
			CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
		/*
		 * BUG 5381: Don't read any data from socket for now.
		 * When this transaction is finished and it is keep-alive
		 * request, another event will trig to call http_epollin()
		 * when this socket is added back to epoll loop.
		 */
		goto close_socket;
	}

	if ( CHECK_CON_FLAG(con, CONF_TUNNEL_PROGRESS) ) {
		return TRUE;
	}

	rlen = nkn_check_http_cb_buf(&con->http);
	/* BZ 6558:
	 * In some cases, rlen comes back as 0 because the
	 * buffer exhausted or is packed exactly to its limit.
	 * In such cases, we never respond to the client with
	 * an error and attempt to read from the socket with
	 * length 0. This will cause the socket to close,
	 * without us responding with a 413 to the client.
	 */
	if ((rlen <= 0)) {
		// if request entity too large.. we would have responded
		// with a 413
		(void) send_err_on_large_request(con);
		err = 1;
		goto close_socket;
	}

	n = recv(sockfd, &con->http.cb_buf[con->http.cb_totlen], rlen, 0);
	//printf("n=%d\n", n);
	if (n <= 0) {
		err = 2;
		SET_CON_FLAG(con, CONF_RECV_ERROR);
		goto close_socket;
	}

	glob_tot_bytes_recv += n;
	AO_fetch_and_add(&glob_client_recv_tot_bytes, n);

	// valid data
	con->http.cb_totlen += n;

	if (CHECK_CON_FLAG(con, CONF_INTRL_REQ_SRC) && ((uint64_t)con->http.cb_totlen  >= sizeof(uint64_t)) ) {
		ssl_con_hdr_t *hdr = (ssl_con_hdr_t *)&con->http.cb_buf[0];
		/* For https request , a header will be inserted by ssld before the data */
		if (hdr->magic == HTTPS_REQ_IDENTIFIER ) {
			SET_CON_FLAG(con, CONF_INTRL_HTTPS_REQ);
			if ((uint64_t)(con->http.cb_totlen ) >= sizeof(ssl_con_hdr_t)) {
				UNSET_CON_FLAG(con, CONF_INTRL_REQ_SRC);
				memcpy(&con->src_ipv4v6, &hdr->src_ipv4v6, sizeof(ip_addr_t));
				con->src_port = hdr->src_port;
				memcpy(&con->dest_ipv4v6, &hdr->dest_ipv4v6, sizeof(ip_addr_t));
				con->dest_port = hdr->dest_port;
				memcpy(&con->http.remote_ipv4v6, &hdr->dest_ipv4v6, sizeof(ip_addr_t));
				con->http.remote_port = ntohs(hdr->dest_port);
				con->ip_family = hdr->src_ipv4v6.family;
				con->http.cb_totlen -= sizeof(ssl_con_hdr_t);
				memcpy(con->http.cb_buf, &con->http.cb_buf[sizeof(ssl_con_hdr_t)],
						con->http.cb_totlen);
				UNSET_CON_FLAG(con, CONF_INTRL_HTTPS_REQ);
				SET_CON_FLAG(con, CONF_HTTPS_HDR_RECVD);
				SET_HTTP_FLAG(&con->http, HRF_HTTPS_REQ);
			} else {
				// Wait till , ssl_con_hdr_t is received
				return TRUE;
			}
		} else {
			UNSET_CON_FLAG(con, CONF_INTRL_REQ_SRC);
		}
	}

	if ( n > max_http_req_size) {
		(void) send_err_on_large_request(con);
		err = 3;
		goto close_socket;
	}
#if 0
	else if (n == 0) {
		/*
		 * BUG TBD:
		 * Don't understand what root cause it is.
		 * errno returns ENOENT (2), which is un-documentated in the man recv page.
		 * On google, someone suggests this errno could be because of
		 * the code is not compiled with -D_REENTRANT
	 	 *
		 * Workaround: do nothing and skip this epollin signal.
		 */
		return TRUE;
	}
#endif // 0

	SET_CON_FLAG(con, CONF_RECV_DATA);


	// Normal HTTP Transaction request

	ret = http_handle_request(con);

	if ((ret == TRUE) && CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_TPROXY_MODE)) {
			ret = NM_get_tcp_info(sockfd, &tcp_con_info);
			if ((ret == 0) &&
			    (tcp_con_info.tcpi_state == TCP_CLOSE_WAIT)) {
				glob_cache_tcp_half_close++;
				shutdown(sockfd, SHUT_RD);
				NM_EVENT_TRACE(sockfd, FT_TASK_RESUME);
				SET_CON_FLAG(con, CONF_RESUME_TASK);
			}
		}
	}

	return TRUE;

close_socket:
	/*
	 * socket needs to be closed.
	 */
	if (CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_TPROXY_MODE)) {
			ret = NM_get_tcp_info(sockfd, &tcp_con_info);
			if ((ret == 0) && (tcp_con_info.tcpi_state == TCP_CLOSE_WAIT)) {
				glob_cache_tcp_half_close++;
				shutdown(sockfd, SHUT_RD);
				NM_EVENT_TRACE(sockfd, FT_TASK_RESUME);
				SET_CON_FLAG(con, CONF_RESUME_TASK);
			}
		}
		NM_del_event_epoll(sockfd);
	} else {
		close_conn(con);
	}
	return TRUE;
}

static int http_epollout(int sockfd, void * private_data)
{
	con_t * con = (con_t *)private_data;

	UNUSED_ARGUMENT(sockfd);

	NM_EVENT_TRACE(sockfd, FT_HTTP_EPOLLOUT);
	http_try_to_sendout_data(con);

	return TRUE;
}

static int http_epollerr(int sockfd, void * private_data)
{
	con_t * con = (con_t *)private_data;

	UNUSED_ARGUMENT(sockfd);
	NM_EVENT_TRACE(sockfd, FT_HTTP_EPOLLERR);

	if (CHECK_CON_FLAG(con, CONF_TUNNEL_PROGRESS)) {
		return TRUE;
	}

	if (CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
		//printf("CHECK_CON_FLAG(con, CONF_TASK_POSTED\n");
		SET_CON_FLAG(con, CONF_CANCELD);
		NM_del_event_epoll(con->fd);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TPROXY_MODE)) {
			shutdown(sockfd, SHUT_RD);
			NM_EVENT_TRACE(sockfd, FT_TASK_RESUME);
			SET_CON_FLAG(con, CONF_RESUME_TASK);
		}
	} else {
		close_conn(con);
	}
	//ERROR("%s: socket error, close socket", __FUNCTION__);
	return TRUE;
}

static int http_epollhup(int sockfd, void * private_data)
{
	// Same as http_epollerr
	NM_EVENT_TRACE(sockfd, FT_HTTP_EPOLLHUP);
	return http_epollerr(sockfd, private_data);
}

static int http_timer(int sockfd, void *private_data,
		      net_timer_type_t timer_type)
{
	UNUSED_ARGUMENT(timer_type);
	con_t *con;

	NM_EVENT_TRACE(sockfd, FT_HTTP_TIMER);
	con = (con_t*)private_data;
	int rv;

	switch(con->timer_event) {
	case TMRQ_EVENT_NAMESPACE_WAIT:
	case TMRQ_EVENT_NAMESPACE_WAIT_2:
		rv = tmrq_add_event(sockfd, con->timer_event);
		if (rv) {
			DBG_LOG(MSG, MOD_HTTP,
				"tmrq_add_event() failed, fd=%d event_type=%d",
				sockfd, con->timer_event);
		}
		break;
	default:
		DBG_LOG(MSG, MOD_HTTP,
			"tmrq_add_event() invalid event_type, "
			"fd=%d event_type=%d", sockfd, con->timer_event);
		break;
	}
	return 0;
}

static int http_timeout(int sockfd, void * private_data, double timeout)
{
	con_t * con = (con_t *)private_data;

	UNUSED_ARGUMENT(sockfd);
	NM_EVENT_TRACE(sockfd, FT_HTTP_TIMEOUT);

	if (gnm[sockfd].private_data != (void *)con) return TRUE;
	if (con->magic != CON_MAGIC_USED) return TRUE;
	if ((gnm[sockfd].fd != sockfd) || (con->fd != sockfd)) return TRUE;

	if (NM_CHECK_FLAG(&gnm[con->fd], NMF_RST_WAIT)) {
		close_conn(con);
		return TRUE;
	}

	if (CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
		if (CHECK_CON_FLAG(con, CONF_TASK_TIMEOUT)) {
			return FALSE;
		}

		NM_EVENT_TRACE(sockfd, FT_TASK_TIMEOUT);
		SET_CON_FLAG(con, CONF_TASK_TIMEOUT);
		AO_fetch_and_add1(&glob_err_timeout_with_task);
		DBG_LOG(MSG, MOD_HTTP, "timeout: flag=0x%lx  timeout=%f fd=%d", con->con_flag, timeout, con->fd);
		/*
		 * BIG BUG: Too much complain of this crash.
		 * This crash is due to scheduler does not return task for a
		 * very long time. Now I just skip this task assuming this
		 * task does not posted before at all.
		 * as a result: it could lead to memory leak or crash somewhere
		 * else.
		 *
		 * Right fix: scheduler should allow to cancel a task.
		 */
		if (CHECK_HTTP_FLAG(&con->http, HRF_TPROXY_MODE)) {
			NM_EVENT_TRACE(sockfd, FT_TASK_RESUME);
			SET_CON_FLAG(con, CONF_RESUME_TASK);
		}
		NM_del_event_epoll(con->fd);
		/*
		 * return FALSE: means we do not close socket at this time.
		 * we will close socket at the time when task is completed.
		 *
		 * if task never returns (why?), we leak this socket and con
		 * structure.
		 */
		return FALSE;
	} else if ((con->http.num_nkn_iovecs != 0) && (con->http.timeout_called <= 2)){

		glob_data_not_sentout_when_timeout ++;
		network_mgr_t * pnm;
		off_t        sent_len;
		int ret;

		/*
		 * We do not want to be infinitely wait.
		 * Infinite wait issue could happen for disappearing client.
		 */
		con->http.timeout_called ++;
		pnm=&gnm[con->fd];

		/* We have data in hand. let's try to send data out if we could. */
		sent_len = con->sent_len;
		ret = http_try_to_sendout_data(con);
		if ( ret == FALSE ) {
			/*
			 * socket has been closed and con is not valid pointer any more.
			 */
			return TRUE;
		}
		if (sent_len == con->sent_len) {
			glob_http_tot_send_failed++;
			DBG_LOG(MSG, MOD_HTTP,
				"Socket buffer is still full, data never get sent out %d",
				con->fd);
		}
		else {
			glob_tot_event_lost++;
			DBG_LOG(MSG, MOD_HTTP,
				"Socket buffer available, but does not get epollout event %d",
				con->fd);
		}

        if (con->http.req_cod != NKN_COD_NULL) {
			nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
			con->http.req_cod = NKN_COD_NULL;
		}
		return FALSE;   // Don't close the connection
	} else {
		/*
		 * There is no activity on socket at all.
		 * socket has been idled out for so long time
		 */
		glob_http_tot_timeout++;
	}

        // Same as http_epollerr
	close_conn(con);

        return TRUE;
}

/*
 * This function is called when HTTP connection is closed
 */
void cleanup_http_cb(con_t *con)
{
        int i;
        char md5_checksum_str[CKSUM_STR_LEN+1] = {0,};

	// Cleanup Scheduler
	if (con->nkn_taskid != -1) {
		nkn_cleanup_a_task(con);
	}

	gettimeofday(&(con->http.end_ts), NULL);

	// update the error subcode counters
	switch (con->http.subcode){
	case NKN_SERVER_BUSY:			//10003
		glob_nkn_server_busy_err_10003++;
		break;
	case NKN_OM_SERVER_FETCH_ERROR: 	//50001
		AO_fetch_and_add1(&glob_om_server_fetch_err_50001);
		break;
	case NKN_OM_SERVER_DOWN:		//50003
		AO_fetch_and_add1(&glob_om_server_down_err_50003);
		break;
	case NKN_OM_NO_NAMESPACE:		//50007
		AO_fetch_and_add1(&glob_om_no_namespace_err_50007);
		break;
	case NKN_OM_INTERNAL_ERR:		//50008
		AO_fetch_and_add1(&glob_om_internal_err_50008);
		break;
	case NKN_OM_NON_CACHEABLE:		//50019
		AO_fetch_and_add1(&glob_om_non_cacheable_err_50019);
		break;
	case NKN_OM_INV_OBJ_VERSION:		//50021
		AO_fetch_and_add1(&glob_om_inv_obj_ver_err_50021);
		break;
	case NKN_OM_SERVICE_UNAVAILABLE:
		AO_fetch_and_add1(&glob_om_service_unavailable_err_50028);
		break;
	case NKN_HTTP_MALLOC:			//52002
	case NKN_HTTP_REQUEST:			//52003
		break;
	case NKN_HTTP_NAMESPACE:		//52004
		AO_fetch_and_add1(&glob_http_namespace_err_52004);
		break;
	case NKN_HTTP_REQ_RANGE_1:		//52005
	case NKN_HTTP_REQ_RANGE_2:		//52006
	case NKN_HTTP_REQ_RANGE_3:		//52007
	case NKN_HTTP_RES_RANGE_1:		//52008
	case NKN_HTTP_RES_RANGE_2:            	//52009
	case NKN_HTTP_RES_RANGE_3:            	//52010
	case NKN_HTTP_CONTENT_TYPE:           	//52011
		break;
	case NKN_HTTP_URI:                    	//52012
		AO_fetch_and_add1(&glob_http_uri_err_52012);
		break;
	case NKN_HTTP_URI_MORE:               	//52013
	case NKN_HTTP_URI_LESS:               	//52014
	case NKN_HTTP_RES_SEEK:               	//52015
	case NKN_HTTP_CLOSE_CONN:             	//52016
		AO_fetch_and_add1(&glob_http_close_conn_err_52016);
		break;
	case NKN_HTTP_GET_CONT_LENGTH:        	//52017
		AO_fetch_and_add1(&glob_http_get_cont_len_err_52017);
		break;
	case NKN_HTTP_NO_HOST_HTTP11:         	//52018
		AO_fetch_and_add1(&glob_http_no_host_http11_err_52018);
		break;
	case NKN_HTTP_PARSER:                 	//52019
	case NKN_HTTP_ERR_CHUNKED_REQ:        	//52020
		break;
	case NKN_UF_CONN_RESET:
		AO_fetch_and_add1(&glob_uf_reject_reset_80001);
		break;
	case NKN_UF_CONN_CLOSE:
		AO_fetch_and_add1(&glob_uf_reject_close_80002);
		break;
	case NKN_UF_HTTP_403_FORBIDDEN:
		AO_fetch_and_add1(&glob_uf_reject_http_403_80003);
		break;
	case NKN_UF_HTTP_404_NOTFOUND:
		AO_fetch_and_add1(&glob_uf_reject_http_404_80004);
		break;
	case NKN_UF_HTTP_301_REDIRECT_PERM:
		AO_fetch_and_add1(&glob_uf_reject_http_301_80005);
		break;
	case NKN_UF_HTTP_302_REDIRECT:
		AO_fetch_and_add1(&glob_uf_reject_http_302_80006);
		break;
	case NKN_UF_HTTP_200_WITH_BODY:
		AO_fetch_and_add1(&glob_uf_reject_http_200_80007);
		break;
	case NKN_UF_CUSTOM_HTTP_RESPONSE:
		AO_fetch_and_add1(&glob_uf_reject_http_custom_80008);
		break;
	case NKN_UF_URI_SIZE_CONN_RESET:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_reset_80010);
		break;
	case NKN_UF_URI_SIZE_CONN_CLOSE:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_close_80011);
		break;
	case NKN_UF_URI_SIZE_HTTP_403_FORBIDDEN:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_http_403_80012);
		break;
	case NKN_UF_URI_SIZE_HTTP_404_NOTFOUND:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_http_404_80013);
		break;
	case NKN_UF_URI_SIZE_HTTP_301_REDIRECT_PERM:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_http_301_80014);
		break;
	case NKN_UF_URI_SIZE_HTTP_302_REDIRECT:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_http_302_80015);
		break;
	case NKN_UF_URI_SIZE_HTTP_200_WITH_BODY:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_http_200_80016);
		break;
	case NKN_UF_URI_SIZE_CUSTOM_HTTP_RESPONSE:
		AO_fetch_and_add1(&glob_uf_uri_size_reject_http_custom_80017);
		break;
	default:
		break;
	}

	if (con->http.respcode == 0 /*|| con->http.content_length == 0 */) {
		DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d r=%d, s=%d, l=%ld", con, con->fd,
			con->http.respcode, con->http.subcode, con->http.content_length);
	}
	NKN_ASSERT(con->sent_len >= 0);

        // Cleanup HTTP structure
	if ( CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA) ) {
		con->sent_len = 0; // Note zero xfer for accesslog
	}

	// We do HTTP accesslog.
	/*
	 * In case of error or timeout and no response was sent,
	 * set sub error code.
	 */
	if ( (CHECK_CON_FLAG(con, CONF_RECV_ERROR) || CHECK_CON_FLAG(con, CONF_CLIENT_CLOSED))
		&& (con->http.subcode == 0)) {
		con->http.subcode = NKN_HTTP_CLIENT_CLOSED;
	}
	nkn_record_hit_history(con, NULL, 0); // complete hit history

        // Release MD5 checksum context
        if (con->http.nsconf 
            && con->http.nsconf->acclog_config->al_resp_hdr_md5_chksum_configured 
            && con->http.nsconf->md5_checksum_len 
            && con->md5) {

            unsigned char md5_digest_str[MD5_DIGEST_LENGTH] = {0,};
            int hdr_added = 0;

            //Calculate the final MD5 checksum value.
            MD5_Final(md5_digest_str, &con->md5->ctxt);

            // When it is a byte-range or seek request, take MD5 checksum only when
            // offset starts from 0.
            if (   (!CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE) || 
                      /* Allow range requests like 0-NNN, where NNN is >= configured chksum data len
                       * eg: 0-1023 should allowed where configured is 1024 chars. */
                       ((con->http.brstart == 0) && (con->http.brstop >= con->http.nsconf->md5_checksum_len-1))) 
                && (!CHECK_HTTP_FLAG(&con->http, HRF_BYTE_SEEK)  || 
                      /* Allow seek requests like 0-NNN, where NNN is >= configured chksum data len*/
                       ((con->http.seekstart == 0) && (con->http.seekstop >= con->http.nsconf->md5_checksum_len-1))) 
                && ((con->http.respcode == 206) || (con->http.respcode == 200)) ) {

                cache_index_md5tostr(md5_digest_str, md5_checksum_str, DEFAULT_CKSUM_LEN);
                add_known_header(con->http.p_resp_hdr, MIME_HDR_X_NKN_MD5_CHECKSUM, 
                                  md5_checksum_str, MD5_DIGEST_LENGTH*2);
                hdr_added = 1;
            }

            DBG_LOG(MSG, MOD_HTTP,  "MD5_CHECKSUM(Final[%d]):[totlen:%d]:digest:'%s',"
                                    " con=%p fd=%d r=%d, s=%d, l=%ld, header %s added", 
                        con->md5->temp_seq,
                        con->md5->in_strlen,
                        md5_checksum_str, con, con->fd,
                        con->http.respcode, con->http.subcode, 
                        con->http.content_length,
                        (hdr_added ? "": "NOT" ));
        } 

        if (!CHECK_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE) /* clear md5 ctxt, when conn gets closed */
                  && con->md5) { /* when MD5 ctxt is present */
            DBG_LOG(MSG, MOD_HTTP,  "MD5_CHECKSUM(Close conn: %p). Free md5 ctxt: %p", con, con->md5) ;
            free(con->md5);
            con->md5 = NULL;
        }

	http_accesslog_write(con);
	if (con && 
	    is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_URI) &&
	    !(CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ)) ) {
		/*
         	* Update global counters.
         	*/
        	for (i=0;i<con->http.num_nkn_iovecs;i++) {
			AO_fetch_and_add(&glob_tot_bytes_tobesent, -con->http.content[i].iov_len);
			AO_fetch_and_add(&glob_warn_failed_sendout, con->http.content[i].iov_len);

			/* Since we haven't sent data out, we need to do "minus". */

			switch( con->http.provider ) {
			case BufferCache_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_cache, -con->http.content[i].iov_len);
			break;

			case SolidStateMgr_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_ssd_disk, -con->http.content[i].iov_len);
			AO_fetch_and_add(&glob_tot_hdr_size_from_disk, -con->http.content[i].iov_len);
			break;

			case SAS2DiskMgr_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_sas_disk, -con->http.content[i].iov_len);
			AO_fetch_and_add(&glob_tot_hdr_size_from_disk, -con->http.content[i].iov_len);
			break;

			case SATADiskMgr_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_sata_disk, -con->http.content[i].iov_len);
			AO_fetch_and_add(&glob_tot_hdr_size_from_disk, -con->http.content[i].iov_len);
			break;

			case TFMMgr_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_tfm, -con->http.content[i].iov_len);
			break;

			case NFSMgr_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_nfs, -con->http.content[i].iov_len);
			break;

			case OriginMgr_provider:
			AO_fetch_and_add(&glob_tot_hdr_size_from_origin, -con->http.content[i].iov_len);
			break;

			default: break;
			}
        	}
        	AO_fetch_and_add1(&glob_tot_video_delivered);
        	switch( con->http.provider ) {
        	case SolidStateMgr_provider: 
        	case BufferCache_provider: 
        	case SAS2DiskMgr_provider: 
        	case SATADiskMgr_provider: 
        	case TFMMgr_provider: 
			AO_fetch_and_add1(&glob_tot_video_delivered_with_hit); 
			break;
        	case NFSMgr_provider: break;
        	case OriginMgr_provider: break;
        	default: break;
        	}
	}

	if (con->http.req_cod != NKN_COD_NULL) {
		nkn_cod_close(con->http.req_cod, NKN_COD_NW_MGR);
		con->http.req_cod = NKN_COD_NULL;
	}

	if ( CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA) ) {
		con->sent_len = 0; // Note zero xfer for accesslog
	}
	/* Bug 3700 we have changed http hdrs not to init to cb_buf */
	reset_http_header(&con->http.hdr, NULL, 0);
	if (con->http.p_resp_hdr) {
                shutdown_http_header(con->http.p_resp_hdr);
                free(con->http.p_resp_hdr);
		con->http.p_resp_hdr = NULL;
        }

	if (con->http.resp_buf) {
		free(con->http.resp_buf);
		con->http.resp_buf = 0;
		con->http.resp_buflen = 0;
	}
	if (con->http.resp_body_buf) {
		if (con->http.resp_body_buflen > 0) {
		    free((void *)con->http.resp_body_buf);
		}
		con->http.resp_body_buf = 0;
		con->http.resp_body_buflen = 0;
	}
	if (con->http.prepend_content_data) {
		free(con->http.prepend_content_data);
		con->http.prepend_content_data = 0;
	}
	if (con->http.append_content_data) {
		free(con->http.append_content_data);
		con->http.append_content_data = 0;
	}
	if (con->http.uri_token) {
		free(con->http.uri_token);
		con->http.uri_token = 0;
	}

	release_namespace_token_t(con->http.ns_token);
	con->http.nsconf = NULL;
	con->http.ns_token = NULL_NS_TOKEN;

	release_namespace_token_t(con->http.cl_l7_ns_token);
	con->http.cl_l7_ns_token = NULL_NS_TOKEN;
	release_namespace_token_t(con->http.cl_l7_node_ns_token);
	con->http.cl_l7_node_ns_token = NULL_NS_TOKEN;

	// reset counters
	con->http.size_from_origin = 0;
	if (con->http.hit_history) {
		free(con->http.hit_history);
		con->http.hit_history = NULL;
	}
	con->http.hit_history_used = 0;

	// reset GeoIP info
	memset(&con->ginf, 0, sizeof(geo_info_t));
        memset(&con->uc_inf, 0, sizeof(ucflt_info_t));

}


/*
 * This function is called when HTTP connection keep-alive feature is enabled.
 */
int reset_http_cb(con_t * con)
{
	struct network_mgr * pnm;
	char * cb_buf;
	int cb_max_buf_size;
	int cb_buf_bak_len;
	uint32_t remote_ip;
	uint16_t remote_port;
	uint16_t remote_sock_port;
	int remote_family;
	int ret = TRUE;

	if (CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST)) {
		L7_SWT_PARENT_NS(&con->http);
	}

	/*
	 * The following code was in the NM_close_socket()
	 * Since this routine does not call NM_close_socket()
	 * So I move to here.
	 */
	pnm = &gnm[con->fd];
	NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
	if (NM_CHECK_FLAG(pnm, NMF_IN_SBQ) ) {
               	LIST_REMOVE(pnm, sbq_entries);
		NM_UNSET_FLAG(pnm, NMF_IN_SBQ);
	}
	NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

	/* Requirement 2.1 - 34. 
	 * Reduce session count for this namespace.
	 *
	 * We dont expect nsconf to be NULL unless it was
	 * a bad request, and we are responding without a
	 * namespace resolved.
	 */
	if (CHECK_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED) && con->http.nsconf != NULL) {
		nvsd_rp_free_resource(RESOURCE_POOL_TYPE_CLIENT_SESSION, con->http.nsconf->rp_index, 1);
		AO_fetch_and_sub1((volatile AO_t *) &(con->http.nsconf->stat->g_http_sessions));
		NS_DECR_STATS(con->http.nsconf, PROTO_HTTP, client, active_conns);
		NS_DECR_STATS(con->http.nsconf, PROTO_HTTP, client, conns);
		CLEAR_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED);
	}

        cleanup_http_cb(con);


        // Fields for MD5 checksum calculation for response. Re-init MD5 context here.
        if (con->md5) {
            DBG_LOG(MSG, MOD_HTTP, "MD5_CHECKSUM(Re-Init, ctxt not freed): con=%p, md5_ctxt:%p",
                                   con, con->md5);
            MD5_Init(&con->md5->ctxt) ;
            con->md5->in_strlen = 0;
            con->md5->temp_seq = 0;
        }

	/*
	 * we only need to reset this fields.
	 */
	if (CHECK_CON_FLAG(con, CONF_HTTPS_HDR_RECVD)) {
		con->con_flag = CONF_HTTPS_HDR_RECVD;
        } else { 
		con->con_flag = 0;
        }
	con->sent_len = 0;
        con->origin_servers.count = 0;
 
	/*
	 * Comment out the next two lines so we make AFR calculation
	 * is connection-based, not HTTP transaction-based.
	 */
	//con->max_send_size = 0;
	//con->max_faststart_buf = 0;

	/*
	 * For pipeline request, we should save already loaded data.
	 */
	cb_buf = con->http.cb_buf;
	cb_max_buf_size = con->http.cb_max_buf_size;
	cb_buf_bak_len = con->http.cb_totlen - con->http.cb_offsetlen;
	if (cb_buf_bak_len>0 && cb_buf_bak_len <= con->http.cb_max_buf_size) {
		memcpy(cb_buf, &cb_buf[con->http.cb_offsetlen], cb_buf_bak_len);
	}
	else {
		cb_buf_bak_len = 0;
	}
	remote_ip = IPv4(con->http.remote_ipv4v6);
	remote_port = con->http.remote_port;
	remote_sock_port = con->http.remote_sock_port;
	remote_family = con->http.remote_ipv4v6.family;
	memset(&con->http, 0, sizeof(http_cb_t));
	IPv4(con->http.remote_ipv4v6) = remote_ip;
	con->http.remote_port = remote_port;
	con->http.remote_sock_port = remote_sock_port;
	con->http.remote_ipv4v6.family = remote_family;
	con->http.cb_buf = cb_buf;
	con->http.cb_max_buf_size = cb_max_buf_size;
	con->nkn_taskid = -1;
	con->http.cache_encoding_type = -1;
	con->http.object_hotval = 0xaaaaaaaa;
	con->http.last_provider = -1;
	con->os_failover.num_ips = 0;
	con->os_failover.num_ip_index = 0;

	// HTTP data initialization
	init_http_header(&con->http.hdr, NULL, 0);

	NM_add_event_epollin(con->fd);

	/*
	 * has more data, it could be pipeline request.
	 */
	if (cb_buf_bak_len) {
		con->http.cb_totlen = cb_buf_bak_len;
		ret = http_handle_request(con);
	}
	return ret;
}

void re_register_http_socket(int sockfd, void * con);
void re_register_http_socket(int sockfd, void * con)
{
	/* Caller should hold gnm[sockfd].mutex locker */
	if (register_NM_socket(sockfd,
                        (void *)con,
                        http_epollin,
                        http_epollout,
                        http_epollerr,
                        http_epollhup,
			http_timer,
                        http_timeout,
                        http_idle_timeout,
                        USE_LICENSE_ALWAYS_TRUE,
			TRUE) == FALSE)
	{
		NKN_ASSERT(0);
		/* 
		 * Ideally, this should never happen. 
		 * Maybe we should call assert(0) here.
		 *
		 * Here it is transferring keep-alive tunnel client socket back to 
		 * normal client socket.  It should have already hold a license.
		 */
		nkn_close_fd(sockfd, NETWORK_FD);
		free(con);
	}
}


/*
 * This function cleans up HTTP structure and con structure
 * It should be called only once for each socket at the time of socket close time
 */
static
void int_close_conn(con_t * con, int reset_close)
{
	DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d", con, con->fd);

	if (CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST)) {
		L7_SWT_PARENT_NS(&con->http);
	}

	NM_EVENT_TRACE(con->fd, FT_CLOSE_CONN);
	if (CHECK_CON_FLAG(con, CONF_RESP_PARSER_ERROR)) {
		http_build_res_404(&con->http, NKN_OM_PARSE_ERR_1);
		 if (con->http.res_hdlen) {
			 http_send_data(con, con->http.resp_buf, 
					 con->http.res_hdlen);
			 con->http.res_hdlen = 0;

		 }
	}
	if (reset_close) {
		NM_reset_close_socket(con->fd);
	} else {
		NM_close_socket(con->fd);
	}

	if ( !CHECK_CON_FLAG(con, CONF_RECV_DATA) )
	{
		glob_warn_socket_no_recv_data++;
		//printf("close_conn: fd=%d does not receive any data\n", con->fd);
	}
	if ( !CHECK_CON_FLAG(con, CONF_SEND_DATA) )
	{
		glob_warn_socket_no_send_data++;
		//printf("close_conn: fd=%d has not sent any data yet\n", con->fd);
	}
	AO_fetch_and_sub1(&con->pns->tot_sessions);
	if (con->ip_family == AF_INET6) {
		AO_fetch_and_sub1(&glob_cur_open_http_socket_ipv6);
	} else {
		AO_fetch_and_sub1(&glob_cur_open_http_socket);
	}
	/* add back the bandwidth */
	AO_fetch_and_add(&con->pns->max_allowed_bw, con->max_bandwidth);

    if (con->v_pns) {
        AO_fetch_and_sub1(&con->v_pns->tot_sessions);

        if (AO_load(&con->v_pns->if_refcnt) > 0) {
            AO_fetch_and_sub1(&con->v_pns->if_refcnt);
        }
        if ((0 == AO_load(&con->v_pns->if_refcnt)) && AO_load(&con->v_pns->if_status) == 0) {
            del_pns_from_alias_list(con->v_pns, 0 /* Take lock and delete*/);
        }
        con->v_pns = NULL;
    }

	if (CHECK_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED) && con->http.nsconf != NULL) {
                nvsd_rp_free_resource(RESOURCE_POOL_TYPE_CLIENT_SESSION, con->http.nsconf->rp_index, 1);
                AO_fetch_and_sub1((volatile AO_t *) &(con->http.nsconf->stat->g_http_sessions));
                NS_DECR_STATS(con->http.nsconf, PROTO_HTTP, client, active_conns);
                NS_DECR_STATS(con->http.nsconf, PROTO_HTTP, client, conns);
                CLEAR_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED);
        }

	close_conn_ssp(con);
        cleanup_http_cb(con);
	free(con->http.cb_buf);

        if (con->pe_ssp_args) {
		free(con->pe_ssp_args) ;
		con->pe_ssp_args = NULL;
	}

	assert(con->magic == CON_MAGIC_USED);
	con->magic = CON_MAGIC_FREE;
        free(con);
}

void close_conn(con_t * con)
{
	return int_close_conn(con, 0);
}

void reset_close_conn(con_t * con)
{
	return int_close_conn(con, 1);
}


/*
 * This exported function is used to set up assured flow rate 
 * on this session.
 *
 * afr unit is: Bytes/sec
 */
void con_set_afr(con_t *con, uint64_t afr)
{
	uint64_t max_bandwidth;

	max_bandwidth = (uint64_t) ((float)afr * 1.2);


	/* make this bandwidth available */

	/*
	 * in SSP calculation of afr,
	 * SSP pick up afr =  max( min(client_bw, max_allowed_bw), profile)
	 * client_bw = min(detected client bw, max session bw)
	 * so in this function , we can pick up the SSP afr value as final value;
	 */
	AO_fetch_and_add(&con->pns->max_allowed_bw, 
				(con->max_bandwidth - max_bandwidth));
	con->max_bandwidth = max_bandwidth;
	con->min_afr = afr;

	/* calculate accumulated max_allowed_bw */
}

void init_conn(int svrfd, int sockfd, nkn_interface_t * pns, nkn_interface_t * ppns, struct sockaddr_storage * addr, int thr_num)
{
    con_t *con;
    int j, k;
	struct sockaddr_in *addrptr;
	struct sockaddr_in6 *addrptrv6;
	socklen_t dlen;
        struct sockaddr_in dip;
	struct sockaddr_in6 dip6;

	
	if (addr->ss_family == AF_INET6) {
		addrptrv6 = (struct sockaddr_in6*)addr;
	} else {
		addrptr = (struct sockaddr_in*)addr;
	}

	con = nkn_calloc_type(1, sizeof(con_t), mod_http_con_t);
	if (!con) {
		nkn_close_fd(sockfd, NETWORK_FD);
		return;
	}
	NM_EVENT_TRACE(sockfd, FT_INIT_CONN);

    /* PNS choosing logic:
    The interface could be ALIAS or non-alias. Alias interfaces are reached only through a physical or bond interface.
    This means, alias (VIP) can be reached through any of the physical interfaces. Thus the physcial/bond interface
    through which the alias is reached should be accounted for bw, sessions, afr computations. 
    1. If the connection request received is for physical interface,
        pns = physical interface, parent pns - ppns = NULL
        Simply use 'pns' for computations.
    2. If the request received is for alias interface, then
        pns = alias interface, ppns = parent physcial interface
        Now, we should use ppns for the aforementioned computation.
        So, set pns = ppns and v_pns = pns in con.
        However, make sure FDs, IP addresses are all that belong to the alias interface and not that of parent's
    */


    if (ppns) {
        con->pns = ppns;
        con->v_pns = pns;
        AO_fetch_and_add1(&pns->if_refcnt);
    }
    else {
        con->pns = pns;
        con->v_pns = NULL;
    }
		
    con->magic = CON_MAGIC_USED;
    con->fd = sockfd;
    con->nkn_taskid = -1;
    con->ip_tos = -1;
    con->src_ipv4v6.family = addr->ss_family;
    con->dest_ipv4v6.family = addr->ss_family;	
    con->http.remote_ipv4v6.family = addr->ss_family;
    if (addr->ss_family == AF_INET) {
        con->src_port = addrptr->sin_port;
        IPv4(con->src_ipv4v6) = addrptr->sin_addr.s_addr;
        IPv4(con->dest_ipv4v6) = pns->if_addrv4;
	//For tproxy case, the destination address needs to be the proper origin address and not the intarface address
	if (dip.sin_addr.s_addr != pns->if_addrv4) {
		dlen = sizeof(struct sockaddr_in);
		memset(&dip, 0, dlen);
		if (getsockname(con->fd, &dip, &dlen) != -1) {
			IPv4(con->http.remote_ipv4v6) = dip.sin_addr.s_addr;
			con->http.remote_sock_port = dip.sin_port;
		}
	} else {
        	IPv4(con->http.remote_ipv4v6) = pns->if_addrv4;
	}

	} else {
		/* if (inet_ntop(AF_INET6, &(addrptrv6->sin6_addr), &(IPv6(con->src_ipv4v6)),
				sizeof(struct in6_addr)) == NULL) {
	                free(con->http.cb_buf);
        	        nkn_close_fd(sockfd, NETWORK_FD);
                	free(con);
                	return;
		} */
		con->src_port = addrptrv6->sin6_port;
		memcpy(&IPv6(con->src_ipv4v6), &addrptrv6->sin6_addr, sizeof(struct in6_addr));
	}
	
	for (j=0; j<MAX_PORTLIST; j++) {
		if (addr->ss_family == AF_INET) {
			for (k=0; k< NM_tot_threads; k++) {
				if (pns->listenfd[j][k] == svrfd) {
					con->http.remote_port = pns->port[j];
					con->dest_port = htons(pns->port[j]);
					break;
				}
			}
		} else {
            uint32_t ifv6_cnt = 0, found = 0;
            for (ifv6_cnt = 0; ifv6_cnt < pns->ifv6_ip_cnt; ifv6_cnt++) {
                if (pns->if_addrv6[ifv6_cnt].listenfdv6[j] && (pns->if_addrv6[ifv6_cnt].listenfdv6[j] == svrfd)) {
                    con->http.remote_port = pns->if_addrv6[ifv6_cnt].port[j];
                    con->dest_port = htons(pns->if_addrv6[ifv6_cnt].port[j]);
                    found = 1;
                    break;
                }
            }
            if (found) {
                memcpy(&IPv6(con->dest_ipv4v6), &pns->if_addrv6[ifv6_cnt].if_addrv6, sizeof(struct in6_addr));
		dlen = sizeof(struct sockaddr_in6);
		memset(&dip6, 0, dlen);
		if (getsockname(con->fd, &dip6, &dlen) != -1) {
		    memcpy(IPv6(con->http.remote_ipv4v6), dip6.sin6_addr.s6_addr, sizeof(struct in6_addr));
		    con->http.remote_sock_port = dip6.sin6_port;
		}
                break;
            }
		}
	}
	con->http.cb_max_buf_size = TYPICAL_HTTP_CB_BUF;
	con->http.cb_buf = (char *)nkn_malloc_type(con->http.cb_max_buf_size * sizeof(char) + 1,
						mod_network_cb);

	// HTTP data initialization
	init_http_header(&con->http.hdr, NULL, 0);
	con->http.cache_encoding_type = -1;
	con->http.last_provider = -1;
	con->http.object_hotval = 0xaaaaaaaa;

	// Setup session bandwidth and ABR
	con->max_bandwidth = sess_bandwidth_limit;
	AO_fetch_and_add(&con->pns->max_allowed_bw, -con->max_bandwidth);
	con->min_afr = sess_assured_flow_rate;
	con->max_client_bw = 100 << 20;		// 100 MBytes/sec.  hard-coded by now.
	con->max_allowed_bw =  AO_load(&con->pns->max_allowed_bw);
	con->max_send_size = 0;
	con->max_faststart_buf = 0;

	DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d", con, sockfd);

	if ( con->min_afr ) {
		/* 
		 * If assured_bit_rate for this session is enabled
		 * We set up the max session bandwidth for this session.
		 * The extra .2 is the insurance co-efficiency factor.
		 */
		con_set_afr(con, con->min_afr);
	}
	if ( sess_bandwidth_limit && (con->max_client_bw > (uint64_t)sess_bandwidth_limit) ) {
		/*
		 * if max session bandwidth is configured,
		 * we pick up the min(sess_bandwidth_limit, max_client_bw)
		 * as max client bw.
		 */
		con->max_client_bw = sess_bandwidth_limit;
	}


#ifdef SOCKFD_TRACE
	if (debug_fd_trace && gnm[sockfd].p_trace == NULL) {
		gnm[sockfd].p_trace = nkn_calloc_type(1, sizeof(nm_trace_t), mod_nm_trace_t);
#ifdef DBG_EVENT_TRACE
		pthread_mutex_init(&gnm[sockfd].p_trace->mutex, NULL);
#endif
	}
#endif
	pthread_mutex_lock(&gnm[sockfd].mutex);
	NM_TRACE_LOCK(&gnm[sockfd], LT_SERVER);
	gnm[sockfd].accepted_thr_num = thr_num;
	if (addr->ss_family == AF_INET6) {
		//The flags will be overwritten in register_nm_socket
		NM_SET_FLAG(&gnm[sockfd], NMF_IS_IPV6);
	}
	if (register_NM_socket(sockfd,
                        (void *)con,
                        http_epollin,
                        http_epollout,
                        http_epollerr,
                        http_epollhup,
			http_timer,
                        http_timeout,
                        http_idle_timeout,
                        USE_LICENSE_TRUE,
			TRUE) == FALSE)
	{
		free(con->http.cb_buf);
		nkn_close_fd(sockfd, NETWORK_FD);
		free(con);
		NM_TRACE_UNLOCK(&gnm[sockfd], LT_SERVER);
		pthread_mutex_unlock(&gnm[sockfd].mutex);
		return;
	}

	/* 
	 * We need to increase counter first before calling NM_add_event_epollin().
	 * otherwise pns->tot_sessions could be zero if NM_add_event_epollin returns FALSE.
	 */
	AO_fetch_and_add1(&con->pns->tot_sessions);
    if (con->v_pns) {
        AO_fetch_and_add1(&con->v_pns->tot_sessions);
        AO_fetch_and_add1(&glob_tot_vip_hit);
    }
	if (addr->ss_family == AF_INET6) {
		AO_fetch_and_add1(&glob_tot_http_sockets_ipv6);
		AO_fetch_and_add1(&glob_cur_open_http_socket_ipv6);
	} else {
		AO_fetch_and_add1(&glob_tot_http_sockets);
		AO_fetch_and_add1(&glob_cur_open_http_socket);
	}

	con->ip_family = addr->ss_family;
        if ( NM_CHECK_FLAG(&gnm[sockfd], NMF_IN_USE) ) {
		/*
		 * To fix BUG 1786, 429.
		 * Inside successful register_NM_socket() function call,
		 * register_NM_socket() puts this socket into TIMEOUT QUEUE.
		 * Then this thread is swapped out.
		 * Then timeout function called first which calls close_conn() and free the con memory.
		 * Then swap thread back to here.
		 * If this is the case,  NM_CHECK_FLAG(pnm, NMF_IN_USE) should not be set.
		 * Otherwise, it means timeout function has not been called.
		 *
		 * Why timeout is called first, I have no idea so far.
		 *
		 * -- Max
		 */
		if ( NM_add_event_epollin(sockfd) == FALSE ) {
			close_conn(con);
			NM_TRACE_UNLOCK(&gnm[sockfd], LT_SERVER);
			pthread_mutex_unlock(&gnm[sockfd].mutex);
			return;
		}
	}

	if ((addr->ss_family == AF_INET) && IPv4(con->src_ipv4v6) == 0x0100007F) {
		SET_CON_FLAG(con, CONF_INTRL_REQ_SRC);
	}
	NM_TRACE_UNLOCK(&gnm[sockfd], LT_SERVER);
	pthread_mutex_unlock(&gnm[sockfd].mutex);
}

/* Export this function.
 * Construct cache index URI string based on given scheme.
 *
 * IN  uri		original URI string
 * IN  urilen		original URI length
 * IN  scheme		scheme to specify the format for cache-index
 * IN  cksum_len	MD5 string length
 * IN  idx_offset	Cache index offset within URI
 * OUT cache_uri	transformed URI containing cache-index string
 *
 * returns  0:  success
 * returns -1:  error
 */
int get_cache_index_uri(const char *uri, int urilen, unsigned char md5bytes[], 
	int scheme, int idx_offset, int cksum_len, char cache_uri[])
{
	int ret = 0, idx;
	char md5str[CKSUM_STR_LEN+1];
	char *ptr;

	idx = DEFAULT_CKSUM_LEN;

	cache_index_md5tostr(md5bytes, md5str, cksum_len);
	switch (scheme) {
		case CACHE_INDEX_DEFAULT_SCHEME:
		{
			ret = -1;
			// Append cache index md5 as last part of the URI following 
			// '/' from the end.
			strncpy(cache_uri, uri, urilen);
			cache_uri[urilen] = 0;
			ptr = strrchr(cache_uri, '/');
			if (ptr) {
				*(ptr+1) = 0;
				strncat(cache_uri, md5str, sizeof(md5str));
				ret = 0;
			}
			break;
		}
		case CACHE_INDEX_PREFIX:
		{
			// Replace cache index md5 string as the only part of the URI 
			// and overwriting all other original contents.
			snprintf(cache_uri, MAX_URI_SIZE, "/%s", md5str);	
			break;
		}
		case CACHE_INDEX_DYN_URI:
		{
			// Position the cache-index at specified location in dynamic URI
			// <map-to> regex string.
			strncpy(cache_uri, uri, idx_offset);
			cache_uri[idx_offset] = 0;
			strncat(cache_uri, md5str, sizeof(md5str));
			strncat(cache_uri, &uri[idx_offset], MAX_URI_SIZE);
			break;
		}
		default:
		{
			ret = -1;
			break;	
		}
	}
	return ret;
}

/* Export this function.
 * Utility function to convert cache-index MD5 into its string format
 *
 * IN  md5		MD5 in bytes
 * IN  len		length of md5 string
 * OUT md5str	MD5 string
 *
 * returns  0:  success
 * returns -1:  error
 */
int cache_index_md5tostr(unsigned char md5[], char md5str[], int len) {
	int i;

	memset(&md5str[0], 0, 64);
	for (i=0; i<=(len-1); i++) {
		snprintf(&md5str[i*2], (64 - (i*2)+1), "%02x", 
				md5[i]);
	}

	return 0;
}

/*
 * ===============> server socket <===============================
 * The following functions are designed for server socket.
 * we need to provde epollin/epollout/epollerr/epollhup functions
 */

static int httpsvr_epollin(int sockfd, void * private_data)
{
	int clifd;
	int cnt;
	struct sockaddr_in addr;
	socklen_t addrlen;
	nkn_interface_t * pns = (nkn_interface_t *)private_data;
	nkn_interface_t * ppns = NULL;
    int i32index = 0;
    unsigned if_len = IFNAMSIZ;
    char if_name[IFNAMSIZ] = {0};

	/* always returns TRUE for this case */

	/* BUG 9645: we are in the infinite loop, and watch-dog killed nvsd
	 * Solution: set a limit 5000 for max one time accept().
	 */
    for (cnt=0; cnt<5000; cnt++) {

	//printf("cnt=%d\n", cnt);
	addrlen=sizeof(struct sockaddr_in);
	clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
	if (clifd == -1) {
		if (cnt>100) {
			glob_accept_queue_100++;
			//if (nkn_server_verbose) printf("total %d sockets in accept queue.\n", cnt);
		}
		return TRUE;
	}
	nkn_mark_fd(clifd, NETWORK_FD);

    if (NKN_IF_ALIAS == pns->if_type) {
        if (getsockopt(clifd, SOL_IP, IP_INDEV, if_name, &if_len) < 0) {
            DBG_LOG(MSG, MOD_HTTP, "getsockopt failed to get indev. Error: %d", errno);
        }
        else {
            DBG_LOG(MSG, MOD_HTTP, "getsockopt[indev] if_name: %s, pns_if: %s", if_name, pns->if_name);
        }

        if ('\0' == if_name[0]) {
            AO_fetch_and_add1(&glob_vip_parent_if_miss);
        }
        else {
            AO_fetch_and_add1(&glob_vip_parent_if_hit);
        }

        for (i32index = 0; i32index < MAX_NKN_INTERFACE; i32index++) {
            if (0 == strcmp(interface[i32index].if_name, if_name)) {
                DBG_LOG(MSG, MOD_HTTP, "getsockopt[indev] if_name: %s, pns_if: %s", if_name, pns->if_name);
                break;
            }
        }
        assert(i32index < MAX_NKN_INTERFACE);
        ppns = &interface[i32index];
    }


    if (NKN_IF_ALIAS == pns->if_type) {
        // max_allowed_sessions is calculated based on ABR
        if (AO_load(&ppns->tot_sessions) > ppns->max_allowed_sessions) {
            glob_overflow_socket_due_afr++;
            nkn_close_fd(clifd, NETWORK_FD);
            return TRUE;
        }
    }
    else {
        if (AO_load(&pns->tot_sessions) > pns->max_allowed_sessions) {
            glob_overflow_socket_due_afr++;
            nkn_close_fd(clifd, NETWORK_FD);
            return TRUE;
        }
    }

	if (NM_set_socket_nonblock(clifd) == FALSE) {
		DBG_LOG(MSG, MOD_HTTP,
		    "Failed to set socket %d to be nonblocking socket.",
		    clifd);
		nkn_close_fd(clifd, NETWORK_FD);
		return TRUE;
	}

	// Bind this socket into this accepted interface 
	if (bind_socket_with_interface) {
		NM_bind_socket_to_interface(clifd, pns->if_name);
	}

	init_conn(sockfd, clifd, pns, ppns, (struct sockaddr_storage*)&addr, gnm[sockfd].pthr->num);
    }
	return TRUE;
}

static int httpsvr_epollout(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);

	DBG_LOG(SEVERE, MOD_HTTP, "epollout should never called. sockid=%d", sockfd);
	DBG_ERR(SEVERE, "epollout should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}

static int httpsvr_epollerr(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);

	DBG_LOG(SEVERE, MOD_HTTP, "epollerr should never called. sockid=%d", sockfd);
	DBG_ERR(SEVERE, "epollerr should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}

static int httpsvr_epollhup(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);

	DBG_LOG(SEVERE, MOD_HTTP, "epollhup should never called. sockid=%d", sockfd);
	DBG_ERR(SEVERE, "epollhup should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}

void httpsvr_threaded_init(void)
{
	pthread_attr_t attr;
	int stacksize = 512 * KiBYTES, rv; // 512MB for libketama
	pthread_t pid;
    int i;

	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(SEVERE, MOD_NETWORK, 
			"pthread_attr_init() failed, rv=%d", rv);
		DBG_ERR(SEVERE,
                        "pthread_attr_init() failed, rv=%d", rv);
		return;
	}
	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(SEVERE, MOD_NETWORK, 
			"pthread_attr_setstacksize() stacksize=%d failed, "
			"rv=%d", stacksize, rv);
		return;
	}

	/*
	 * Local interface IP hash table initialization
	 */
	ht_local_ip = ht_base_nocase_hash;
	ht_local_ip.ht = hash_local_ip;
    ht_local_ip.ht_lock = hash_local_ip_lock;
	ht_local_ip.size = sizeof(hash_local_ip)/sizeof(hash_entry_t);
    for (i = 0; i < ht_local_ip.size; i++) {
        pthread_mutex_init(&ht_local_ip.ht_lock[i], NULL);
    }

    // Local interface IPv6 hash table initialization

	ht_local_ipv6 = ht_base_nocase_hash;
	ht_local_ipv6.ht = hash_local_ipv6;
    ht_local_ipv6.ht_lock = hash_local_ipv6_lock;
	ht_local_ipv6.size = sizeof(hash_local_ipv6)/sizeof(hash_entry_t);

    for (i = 0; i < ht_local_ipv6.size; i++) {
        pthread_mutex_init(&ht_local_ipv6.ht_lock[i], NULL);
    }



	pthread_create(&pid, &attr, httpsvr_init, NULL);
}

static void nkn_check_and_waitfor_preread(void)
{
	// include disk config check
	if (service_init_flags & PREREAD_INIT) {
		// make watch-dog not to restart until this set to PREREAD_END..
		glob_service_preread_init = PREREAD_START;
		// monitor preread counters
		while(!glob_dm2_init_done);

		while ((glob_dm2_sata_tier_avl && glob_dm2_sata_preread_done != 1) ||
	       	      (glob_dm2_sas_tier_avl && glob_dm2_sas_preread_done != 1) ||
	       	      (glob_dm2_ssd_tier_avl &&	glob_dm2_ssd_preread_done != 1));
		glob_service_preread_init = PREREAD_END;
    }	
}

int http_if4_finit(nkn_interface_t *pns, int alias /* 0 = alias unlocked, 1 = locked*/) {
    int j, k;

    if (!pns) {
        DBG_LOG(SEVERE, MOD_HTTP, "Invalid pns received for finit");
        return -1;
    }

    if (AO_load(&pns->if_listening) == 1) {
        for (j = 0; j < MAX_PORTLIST; j++) {
	    for (k = 0; k < NM_tot_threads; k++) {
		if (pns->listenfd[j][k]) {
		    pthread_mutex_lock(&gnm[pns->listenfd[j][k]].mutex);
		    unregister_NM_socket(pns->listenfd[j][k], FALSE);
		    NM_close_socket(pns->listenfd[j][k]);
		    pthread_mutex_unlock(&gnm[pns->listenfd[j][k]].mutex);
		    pns->listenfd[j][k] = 0;
		    glob_tot_svr_sockets--;
		}
	    }
        }
    }

    AO_store(&pns->if_listening, 0);
    if (NKN_IF_ALIAS != pns->if_type) {
        return 0;
    }

    if ((AO_load(&pns->if_refcnt) == 0) && (AO_load(&pns->if_status) == 0)) {
        del_pns_from_alias_list(pns, alias);
    }

    return 0;
}

int http_if4_init(nkn_interface_t *pns) {
    struct sockaddr_in srv;
    int ret, val, j, k;
    int listenfd;
    network_mgr_t *pnm = NULL;
    char name[64];

    if (!pns) {
        DBG_LOG(SEVERE, MOD_HTTP, "Invalid pns received for init");
        return -1;
    }

    AO_store(&pns->if_status, 1);

    if (!pns->if_addrv4) {
        goto set_if_params;
    }

    for (j = 0; j < MAX_PORTLIST; j++) {
        if (0 == nkn_http_serverport[j]) {
            continue;
        }
	for (k = 0; k < NM_tot_threads; k++) {
	    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		DBG_LOG(SEVERE, MOD_HTTP, "Failed to create IPv4 socket. errno = %d", errno);
		continue;
	    }
	    nkn_mark_fd(listenfd, NETWORK_FD);
	    val = 1;
	    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	    if (-1 == ret) {
		DBG_LOG(SEVERE, MOD_HTTP, "Failed to set socket option. errno = %d", errno);
		nkn_close_fd(listenfd, NETWORK_FD);
		continue;
	    }

	    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
	    if (-1 == ret) {
		DBG_LOG(SEVERE, MOD_HTTP, "Failed to set socket option. errno = %d", errno);
		nkn_close_fd(listenfd, NETWORK_FD);
		continue;
	    }

	    ret = setsockopt(listenfd, SOL_IP, IP_TRANSPARENT, &val, sizeof(val));
	    if (-1 == ret) {
		DBG_LOG(SEVERE, MOD_HTTP, "Failed to se socket option (IP_TRANSPARENT). errno = %d", errno);
		nkn_close_fd(listenfd, NETWORK_FD);
		continue;
	    }

	    memset(&srv, 0, sizeof(srv));
	    srv.sin_family = AF_INET;
	    srv.sin_addr.s_addr = pns->if_addrv4;
	    srv.sin_port = htons(nkn_http_serverport[j]);

	    if (bind(listenfd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
		DBG_LOG(SEVERE, MOD_HTTP, "Failed to bind server socket to port %d. errno = %d", nkn_http_serverport[j],
			errno);
		nkn_close_fd(listenfd, NETWORK_FD);
		continue;
	    }
	    DBG_LOG(MSG, MOD_HTTP, "NVSD:[port_cnt: %d] bind to server socket succ. to port %d, listenfd: %d", j,
		    ntohs(srv.sin_port), listenfd);

	    // make listen socket non-blocking
	    NM_set_socket_nonblock(listenfd);

	    // bind this socket into this accepted interface
	    NM_bind_socket_to_interface(listenfd, pns->if_name);

	    listen(listenfd, 10000);
	    pns->port[j] = nkn_http_serverport[j];
	    pns->listenfd[j][k] = listenfd;

	    if (register_NM_socket(listenfd, 
		pns,
		httpsvr_epollin,
		httpsvr_epollout,
		httpsvr_epollerr,
		httpsvr_epollhup,
		NULL,
		NULL,
		0,
		USE_LICENSE_FALSE,
		FALSE) == FALSE) {
		nkn_close_fd(listenfd, NETWORK_FD);
		continue;
	    }

	    NM_add_event_epollin(listenfd);
	    pnm = &gnm[listenfd];
	    gnm[listenfd].accepted_thr_num = gnm[listenfd].pthr->num;
	    glob_tot_svr_sockets++;
	    AO_store(&pns->if_listening, 1);
	}
    }

set_if_params:

    if (NKN_IF_ALIAS != pns->if_type) {
        /* Setup max_allowed_sessions and max_allowed_bw */
        nkn_setup_interface_parameter(pns);
        /* add monitor counters */
        // multiple addition to the same counter name do not harm
        snprintf(name, 64, "net_port_%s_tot_sessions", pns->if_name);
        nkn_mon_add(name, NULL, &pns->tot_sessions, sizeof(pns->tot_sessions));
        snprintf(name, 64, "net_port_%s_available_bw", pns->if_name);
        nkn_mon_add(name, NULL, &pns->max_allowed_bw, sizeof(pns->max_allowed_bw));
        snprintf(name, 64, "net_port_%s_credit", pns->if_name);
        nkn_mon_add(name, NULL, &pns->if_credit, sizeof(pns->if_credit));
    } 

    return 0;
}

int http_if6_finit(nkn_interface_t *pns) {
    uint32_t ipv6_cnt;
    int j;

    if (!pns || (NKN_IF_ALIAS == pns->if_type)) {
        DBG_LOG(SEVERE, MOD_HTTP, "Invalid pns received or IPv6 is not supported on alias");
        return -1;
    }

    for (ipv6_cnt = 0; ipv6_cnt < pns->ifv6_ip_cnt; ipv6_cnt++) {
        if (AO_load(&pns->if_addrv6[ipv6_cnt].if_listening) != 1) {
            continue;
        }

        for (j = 0; j < MAX_PORTLIST; j++) {
            if (pns->if_addrv6[ipv6_cnt].listenfdv6[j]) {
                pthread_mutex_lock(&gnm[pns->if_addrv6[ipv6_cnt].listenfdv6[j]].mutex);
                unregister_NM_socket(pns->if_addrv6[ipv6_cnt].listenfdv6[j], FALSE);
                NM_close_socket(pns->if_addrv6[ipv6_cnt].listenfdv6[j]);
                pthread_mutex_unlock(&gnm[pns->if_addrv6[ipv6_cnt].listenfdv6[j]].mutex);
                pns->if_addrv6[ipv6_cnt].listenfdv6[j] = 0;
                glob_tot_svr_sockets_ipv6--;
            }
        }
        AO_store(&pns->if_addrv6[ipv6_cnt].if_listening, 0);
    }

    return 0;
}

int http_if6_init(nkn_interface_t *pns) {
    struct sockaddr_in6 srv_v6;
    int j;
    uint32_t ipv6_cnt;
    int val, ret;
    int listenfdv6;
    network_mgr_t *pnm = NULL;

    if (!pns) {
        DBG_LOG(SEVERE, MOD_HTTP, "Invalid pns received");
        return -1;
    }

    for (ipv6_cnt = 0; ipv6_cnt < pns->ifv6_ip_cnt; ipv6_cnt++) {

        if (1 == AO_load(&pns->if_addrv6[ipv6_cnt].if_listening)) {
            continue;
        }
        for (j = 0; j < MAX_PORTLIST; j++) {
            if (0 == nkn_http_serverport[j]) {
                continue;
            }


            if ((listenfdv6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
                DBG_LOG(SEVERE, MOD_HTTP, "Failed to create IPv6 socket. errno = %d", errno);
                continue;
            }
            nkn_mark_fd(listenfdv6, NETWORK_FD);
            val = 1;
            ret = setsockopt(listenfdv6, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
            if (-1 == ret) {
                DBG_LOG(SEVERE, MOD_HTTP, "Failed to set ipv6 socket options (SO_REUSEADDR). errno = %d", errno);
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }
            val = 1;
            ret = setsockopt(listenfdv6, SOL_IP, IP_TRANSPARENT, &val, sizeof(val));
            if (-1 == ret) {
                DBG_LOG(SEVERE, MOD_HTTP, "Failed to set ipv6 socket option (IP_TRANSPARENT). errno = %d", errno);
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }

            memset(&srv_v6, 0, sizeof(srv_v6));
            srv_v6.sin6_family = AF_INET6;
            memcpy(&srv_v6.sin6_addr, &pns->if_addrv6[ipv6_cnt].if_addrv6, sizeof(struct in6_addr));
            srv_v6.sin6_port = htons(nkn_http_serverport[j]);

            if (bind(listenfdv6, (struct sockaddr *)&srv_v6, sizeof(srv_v6)) < 0) {
                DBG_LOG(SEVERE, MOD_HTTP, "Failed to bind server ipv6 socket to port %d. errno = %d",
                        nkn_http_serverport[j], errno);
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }
            DBG_LOG(MSG, MOD_HTTP, "NVSD: [ifcnt: %d, port_cnt: %d] bind to server socket succ. to port %d. listenfdv6: %d", 
                ipv6_cnt, j, ntohs(srv_v6.sin6_port), listenfdv6);

            // Make socket non-blocking
            NM_set_socket_nonblock(listenfdv6);

            NM_bind_socket_to_interface(listenfdv6, pns->if_name);

            listen(listenfdv6, 10000);
            pns->if_addrv6[ipv6_cnt].port[j] = nkn_http_serverport[j];
            pns->if_addrv6[ipv6_cnt].listenfdv6[j] = listenfdv6;
            // It is enuf to set it once for one port.
            // But it is fine to do it repeatedly for all configured ports during 
            // startup
            AO_store(&pns->if_addrv6[ipv6_cnt].if_status, 1);
            AO_store(&pns->if_addrv6[ipv6_cnt].if_listening, 1);

            if (NKN_IF_ALIAS != pns->if_type) {
                // Setup max_allowed_sessions and max_allowed_bw
                nkn_setup_interface_parameter(pns);
            }
            pnm = &gnm[listenfdv6];
            NM_SET_FLAG(pnm, NMF_IS_IPV6);

            if (register_NM_socket(listenfdv6,
                pns,
                httpsvrv6_epollin,
                httpsvrv6_epollout,
                httpsvrv6_epollerr,
                httpsvrv6_epollhup,
                NULL,
                NULL,
                0,
                USE_LICENSE_FALSE,
                FALSE) == FALSE) {
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }
            NM_add_event_epollin(listenfdv6);
            gnm[listenfdv6].accepted_thr_num = gnm[listenfdv6].pthr->num;
            glob_tot_svr_sockets_ipv6++;
        }
    }

    return 0;

}

static void * httpsvr_init(void *arg)
{
	UNUSED_ARGUMENT(arg);
	int ret;
	int val;
	int i, j;
	unsigned int ipv6_cnt = 0;
	int listen_all = 1;
	const char *ipaddr_str;
	int ipaddr_strlen;
	struct in_addr inaddr;
    nkn_interface_t *pns = NULL;
    char ipv6host[NI_MAXHOST];

	// blocking call
	nkn_check_and_waitfor_preread();

	nkn_http_init();

	gethostname(myhostname, sizeof(myhostname)-1);
	myhostname_len = strlen(myhostname);

	ht_local_ipstr = nkn_calloc_type(1, 
				(sizeof(char *) * (glob_tot_interface+1)),
				mod_http_ipstr_table);
	if (!ht_local_ipstr) {
		DBG_LOG(SEVERE, MOD_HTTP, "memory alloc for ht_local_ipstr failed");
	}


	/* Check if any interface is enabled else listen on all
	 * interface
	 */

	listen_all = !http_listen_intfcnt;
    for (i = 0; i < glob_tot_interface; i++) {
        if (listen_all) {
            interface[i].enabled = 1;
            continue;
        }
        for (j = 0; j < http_listen_intfcnt; j++) {
            if (strcmp(interface[i].if_name, http_listen_intfname[j]) == 0) {
                interface[i].enabled = 1;
                break;
            }
        }
    }

	for (i=0; i<glob_tot_interface; i++)
	{
		if (l4proxy_enabled && strcmp(interface[i].if_name, "lo")) {
			/* Setup max_allowed_sessions and max_allowed_bw */
			//nkn_setup_interface_parameter(i);
			nkn_setup_interface_parameter(&interface[i]);
			/* When l4proxy is enabled, bind to loopback only for mfc_probe */
			continue;
		}

		if ((listen_all == 0) &&
		    (interface[i].enabled != 1) &&
		    (strcmp(interface[i].if_name, "lo") !=0 ))
		    continue;

#define M_ADD_IP_TO_LOCALHASH(x) \
		inaddr.s_addr = (x)->if_addrv4;\
		ipaddr_str = inet_ntoa(inaddr);\
		if (ipaddr_str) {\
			ipaddr_str = nkn_strdup_type(ipaddr_str, mod_http_ipstr);\
			ipaddr_strlen = strlen(ipaddr_str);\
			ht_local_ipstr[i] = ipaddr_str;\
			ret = (*ht_local_ip.lookup_func)(&ht_local_ip,\
					ipaddr_str, ipaddr_strlen, &val);\
			if (ret) {\
                val = 1;\
				ret = (*ht_local_ip.add_func)(&ht_local_ip,\
					ipaddr_str, ipaddr_strlen, val);\
				if (ret) {\
					DBG_LOG(SEVERE, MOD_HTTP,\
					    "ht_local_ip.add_func() failed, rv=%d key=%s",\
					    ret, ipaddr_str);\
					DBG_ERR(SEVERE,\
					    "ht_local_ip.add_func() failed, rv=%d key=%s",\
					    ret, ipaddr_str);\
				}\
			}\
		}

		// Add interface IP addr to local interface IP hash table
        M_ADD_IP_TO_LOCALHASH(&interface[i]);

        http_if4_init(&interface[i]);

		if (nkn_http_ipv6_enable == 0) 	continue; //continue with the interfaces

		for (ipv6_cnt = 0; ipv6_cnt < interface[i].ifv6_ip_cnt; ipv6_cnt++) {
            if (inet_ntop(AF_INET6, &interface[i].if_addrv6[ipv6_cnt].if_addrv6, ipv6host, NI_MAXHOST)) {
                ret = ht_local_ipv6.lookup_func(&ht_local_ipv6, ipv6host, strlen(ipv6host), &val);
                if (ret) {
                    val = 1; 
                    ipaddr_str = nkn_strdup_type(ipv6host, mod_http_ipstr);
                    ipaddr_strlen = strlen(ipaddr_str);
                    ret = ht_local_ipv6.add_func(&ht_local_ipv6, ipaddr_str, ipaddr_strlen, val);
                    if (ret) {
                        DBG_LOG(SEVERE, MOD_HTTP, "ht_local_ipv6.add_func() failed, rv=%d, key=%s", ret, ipaddr_str);
                        free((void *)ipaddr_str);
                    }
                }
            }
        }

        http_if6_init(&interface[i]);

	} //end for loop interfaces

    for (pns = if_alias_list; pns; pns = pns->next) {
        if (listen_all) {
            pns->enabled = 1;
            continue;
        }
        for (i = 0; i < http_listen_intfcnt; i++) {
            if (strcmp(pns->if_name, http_listen_intfname[i]) == 0) {
                pns->enabled = 1;
                break;
            }
        }
    }

    for (pns = if_alias_list; pns; pns = pns->next) {

        if (!listen_all && !pns->enabled) {
            continue;
        }
		// Add interface IP addr to local interface IP hash table
        M_ADD_IP_TO_LOCALHASH(pns);
        // Aliases are only for IPv4
        http_if4_init(pns);
        //if (nkn_http_ipv6_enable == 0)  continue;
        //http_if6_init(pns);
    }

	DBG_LOG(SEVERE, MOD_SYSTEM, "HTTP service ready");
	DBG_LOG(ALARM, MOD_SYSTEM, "HTTP service ready");

	/*
	 * Start the thread to accept client connection socket fds from
	 * proxyd process, only when this is configured from CLI.
	 * Else initialize the listen sockets in nvsd itself.
	 */
	if (l4proxy_enabled && ((acceptor_init("/config/nkn/.proxyd") == -1))) {
		DBG_LOG(SEVERE, MOD_NETWORK, "unable to init acceptor()");
		exit(1);
	}

	nkn_http_service_inited = 1;
	return NULL;
}

#include <atomic_ops.h>
//void nkn_setup_interface_parameter(int i)
void nkn_setup_interface_parameter(nkn_interface_t *pns)
{
	/* it should be ok to use this counter as mutex */
	if ( sess_assured_flow_rate ) {
		/*
		 * If assured_bit_rate is enabled
		 * We set up the max allowed sessions for this port.
		 */
		AO_store(&pns->max_allowed_sessions, 
			(pns->if_bandwidth / sess_assured_flow_rate));		
		AO_store(&pns->max_allowed_bw,
				pns->if_bandwidth);
	}
	else {
		AO_store(&pns->max_allowed_sessions,
				nkn_max_client);
		AO_store(&pns->max_allowed_bw,
				pns->if_bandwidth);
	}
        DBG_LOG(SEVERE, MOD_HTTP, 
                "NVSD:interface[%s, %p], max_allowd_sessions: %ld, bw:%lu ",
                pns->if_name, pns, 
                pns->max_allowed_sessions, 
                pns->if_bandwidth) ;
}
/* 
 * ==================> main functions <======================
 * initialization supporting functions
 */

static void usage(void)
{
	printf("\n");
	printf("nvsd - Nokeena Very Scalable Delivery system\n");
	printf("Usage:\n");
	printf("-f <name> : filename of the config-file\n");
	printf("-s        : skip process check to allow multiple running nvsd process\n");
	printf("-D        : don't go to background (default: go to background)\n");
	printf("-v        : show version\n");
	printf("-V        : valgrind mode\n");
	printf("-T	  : DM2 test mode\n");
	printf("-h        : show this help\n");
	exit(1);
}

static void version(void)
{
	printf("\n");
	printf("nvsd: Nokeena Video Server Daemon\n");
	printf("%s\n", nkn_build_date);
	printf("%s\n", nkn_build_id);
	printf("%s\n", nkn_build_prod_release);
	printf("%s\n", nkn_build_number);
	printf("%s\n", nkn_build_scm_info_short);
	printf("%s\n", nkn_build_scm_info);
	printf("%s\n", nkn_build_extra_cflags);
	printf("\n");
	exit(1);

}


int main(int argc, char **argv)
{
	int ret;
	char *configfile=NULL;
	int dont_daemonize = 0;
	int skip_proc_chk = 0;
	int err = 0;

	/* TBD:
	 * the following line should be removed in the future ,
	 * it is caused by gcc lazy binding feature.
	 */
	void * p=nkn_malloc_type(10,mod_main); free(p); p=nkn_calloc_type(1,10,mod_main); p=nkn_realloc_type(p,20,mod_main); free(p); p=nkn_strdup_type("abc",mod_main); free(p);

	// init global mempool after gcc lazy binding call
	nkn_init_pool_mem();

	while ((ret = getopt(argc, argv, "f:hsvDVT")) != -1) {
                switch (ret) {
                case 'f':
			configfile = optarg;
                        break;
		case 'h':
                        usage();
			break;
		case 's':
			skip_proc_chk = 1;
			break;
		case 'v':
                        version();
			break;
		case 'D':
			dont_daemonize = 1;
			break;
		case 'T':
			g_dm2_unit_test = 1;
			break;
		case 'V':
			valgrind_mode = 1;
			break;
		default:
			usage();
			break;
                }
        }

	/* Disable glib slice allocator */
	setenv("G_SLICE", "always-malloc", 1);


	/* Set up the GMemVTable for tracking glib allocations */
#if 0
	g_mem_set_vtable (glib_mem_profiler_table);
#endif
	if (skip_proc_chk == 0) check_nknvd();
	if (dont_daemonize == 0) daemonize();

	/* Get a real time priority */
	setpriority(PRIO_PROCESS, 0, -1);	

	/* Initialize the counters */
	config_and_run_counter_update(); //Initialize and run copycounter
       /*
	* Load the configuration from file nkn.conf.default first, then
	* load the configuration from TallMaple DB which will overwrite the
	* configuration file.  Finally, check the integration of 
	* configuration parameters.
	*/
	cfg_params_init();

	/* Read and override configuration from nkn.conf.default file */
	read_nkn_cfg(configfile);

	/*  ---- mgmtd init */
	/* BZ 7803 -
	 * if we fail to connect to mgmtd, we dont have any
	 * configuration to continue with.. we simply jump
	 * to exit and die!!
	 */

	/* Re-Org NVSD mgmt threads
	 * Moved the gcl context creation inside the corresponding
	 * mgmt threads
	 */

	err = lc_init();
	bail_error(err);

	err = lc_log_init(nvsd_program_name, NULL, LCO_none,
                      LC_COMPONENT_ID(LCI_NVSD),
                      LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
	bail_error(err);

	/* Config Check is internal AND MUST BE THE LAST CALL before
	 * server_init() is called.
	 */
	nkn_check_cfg();

	// All initialization functions should be called within server_init().
	if (server_init() != 1) {
		return 0;
	}

	/*
	 * ============> infinite loop <=============
	 * NM_main() will never exit unless people use Ctrl-C or Kill
	 */
	NM_main();

	// Done. We exit
	server_exit();

bail:
    return err;
}

int is_local_ip(const char *ip, int ip_strlen)
{
	int ret;
	int val;

      	ret = (*ht_local_ip.lookup_func)(&ht_local_ip, ip, ip_strlen, &val);
	return (ret ? 0 : 1);
}

/* does the standard pre close routine of (a) cleaning up the task
   (b)supporting abortive close if the flag is enabled to avoid
   TIME_WAIT state
   @param con - the connection context
   @return - returns TRUE if abortive close is done and FALSE
   otherwise
*/
int nkn_do_conn_preclose_routine(con_t *con)
{
        int32_t ret = 0;

	if (close_socket_by_RST_enable) {
	    int pending_bytes;
	    ret = NM_get_tcp_sendq_pending_bytes(con->fd, &pending_bytes);
	    if (ret == 0 && pending_bytes > 0) {
		NM_SET_FLAG(&gnm[con->fd], NMF_RST_WAIT);
		NM_change_socket_timeout_time(&gnm[con->fd], 1);
		nkn_cleanup_a_task(con);
		UNSET_CON_FLAG(con, CONF_TASK_POSTED);
		return TRUE;
	    }
	}

	/*
	 * Socket closed.
	 */
	//NM_del_event_epoll(con->fd);
	nkn_cleanup_a_task(con);
	return FALSE;
}

int
nkn_do_post_nfst_a_task(con_t *con, off_t offset_requested,
			off_t length_requested)
{
	nfst_response_t *nfst_cr;
        uint32_t deadline_in_msec;
	const char *data;
	int len, hostlen;
	u_int32_t attrs;
	int hdrcnt;
	const namespace_config_t * ns_cfg;
	char *cache_uri;
	int rv;
	char host[2048];
	char *p_host = host;
	uint16_t port;
	size_t cacheurilen;

        // build up the cache response structure
        nfst_cr = nfst_cr_init();
        if (!nfst_cr) {
		glob_nfst_cr_init_failed++;
		return -1;
	}

	/* Set this flag, so on return this can be detected and
	 * cache uri can be freed up
	 */
	nfst_cr->cr.in_rflags |= CR_RFLAG_NFS_BYPASS;

        nfst_cr->cr.magic = CACHE_RESP_REQ_MAGIC;

	if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			&data, &len, &attrs, &hdrcnt)) {
		if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_DECODED_URI,
				&data, &len, &attrs, &hdrcnt)) {
			if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
				&data, &len, &attrs, &hdrcnt)) {
				cr_free(&nfst_cr->cr);
				glob_nfst_get_uri_failed++;
				return -1; // Should never happen
			}
		}
	}

	ns_cfg = con->http.nsconf;
	if (ns_cfg == NULL) {
		cr_free(&nfst_cr->cr);
		glob_nfst_find_config_failed++;
		return -1;
	}

	hostlen = 0;
	rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &con->http.hdr,
				      ns_cfg, 0, 0, &p_host, sizeof(host),
				      &hostlen, &port, 0, 1);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP,
			"request_to_origin_server() failed, rv=%d", rv);
		cr_free(&nfst_cr->cr);
		glob_nfst_req_2_origin_failed++;
		return -1;
	}

	cache_uri =
	    nstoken_to_uol_uri(con->http.ns_token, data,
			       len, host, hostlen, &cacheurilen);
	if (!cache_uri) {
		DBG_LOG(MSG, MOD_HTTP, "nstoken_to_uol_uri(token=0x%lx) failed",
			con->http.ns_token.u.token);
		cr_free(&nfst_cr->cr);
		glob_nfst_uol_2_uri_failed++;
		return -1;
	}

	nfst_cr->uri = cache_uri;

	nfst_cr->cr.uol.offset = offset_requested;
	nfst_cr->cr.uol.length = length_requested;

	if (con->sent_len == 0) {
		nfst_cr->cr.in_rflags |= (CR_RFLAG_GETATTR | CR_RFLAG_FIRST_REQUEST);
		SET_CON_FLAG(con, CONF_FIRST_TASK);
		/* BZ 1907: set flag CR_RFLAG_NO_DATA if AM count is not to be
		 * updated the task will return with no data to http_output_mgr
		 * which will call back this
		 * function with the task id reset for a new task
		 */
		if (length_requested == 0) {
			nfst_cr->cr.in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) ?
		               CR_RFLAG_NO_DATA : 0;
		}
	} else {
		UNSET_CON_FLAG(con, CONF_FIRST_TASK);
	}

	nfst_cr->cr.in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_NO_CACHE) ?
				CR_RFLAG_NO_CACHE : 0;

	// Set trace flag if requested
	nfst_cr->cr.in_rflags |= CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST) ?
                                CR_RFLAG_TRACE : 0;

	nfst_cr->cr.ns_token = con->http.ns_token;

        //
        // calculate deadline
        //
        // By this time, I do not have any buffered data in my socket hand.
        // Assume each task call will return 1MBytes data
        // So I can set deadline by this format:
        //
	if ( con->min_afr ) {
		/*
		 * ABR Feature is enabled, then we need to calculate the deadline_in_msec
		 *
		 * Assuming that every task will get one buffer page 32 KBytes
		 * con->min_afr unit is Bytes/sec.
		 * deadline_in_msec unit is msec.
		 * So the equation is
	 	 *
		 * 	deadline_in_msec = Bytes in a Buffer Page / scon->min_afr
		 */
        	deadline_in_msec = 1000 * (32 * KBYTES) / con->min_afr; // in msec
	}
	else {
		// feature disabled
		deadline_in_msec = 0;
	}

	if (con->nkn_taskid != -1) {
		// BUG: TBD:
		//
		// Something is not right.
		// Most likely, it is because HTTP pipeline request.
		// last task is not finished yet, the second GET request
		// has already come.
		// We will close this kind of connection so far.
		// We should do assert here.
		// assert (con->nkn_taskid == -1)
		glob_err_pipeline_req ++;

		cr_free(&nfst_cr->cr);
		glob_nfst_con_task_check_failed++;
		return -1;
	}

        // Create a new task
        con->nkn_taskid = nkn_task_create_new_task(
                                    TASK_TYPE_NFS_TUNNEL_MANAGER,
                                    TASK_TYPE_HTTP_SRV,
                                    TASK_ACTION_INPUT,
                                    0, /* NOTE: Put callee operation if any*/
                                    nfst_cr,
                                    sizeof(nfst_response_t),
                                    deadline_in_msec);
	if (con->nkn_taskid == -1) {
                DBG_LOG(MSG, MOD_HTTP,
			"task creation failed.sock %d offset=%ld,"
			"length is %ld.", con->fd, offset_requested,
			length_requested);
                glob_sched_task_create_fail_cnt++;
                if (!CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT)) {
			http_build_res_504(&con->http,
				NKN_RESOURCE_TASK_FAILURE);
			/* TODO Need to check this */
			http_close_conn(con, NKN_RESOURCE_TASK_FAILURE);
                }
		cr_free(&nfst_cr->cr);
		glob_nfst_con_task_create_failed++;
		return -1;
	}

	DBG_LOG(MSG, MOD_HTTP,
		"sock %d create a task %ld. offset=%ld, length is %ld.",
		con->fd, con->nkn_taskid, offset_requested, length_requested);

	nkn_task_set_private(TASK_TYPE_HTTP_SRV, con->nkn_taskid,
				(void *)&gnm[con->fd]);
        nkn_task_set_state(con->nkn_taskid, TASK_STATE_RUNNABLE);
	AO_fetch_and_add1(&glob_http_schd_cur_open_task);

	return 0;
}

static int httpsvrv6_epollin(int sockfd, void * private_data)
{
    int clifd;
    int cnt;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    nkn_interface_t * pns = (nkn_interface_t *)private_data;
    nkn_interface_t * ppns = NULL;
    int i32index = 0;
    unsigned if_len = IFNAMSIZ;
    char if_name[IFNAMSIZ] = {0};

    /* always returns TRUE for this case */

    for (cnt=0; ; cnt++) {

        //printf("cnt=%d\n", cnt);
        addrlen=sizeof(struct sockaddr_in6);
        clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
        if (clifd == -1) {
            if (cnt>100) {
                glob_accept_queue_100++;
                //if (nkn_server_verbose) printf("total %d sockets in accept queue.\n", cnt);
            }
            return TRUE;
        }
        nkn_mark_fd(clifd, NETWORK_FD);

        // Well. Alias is not applicable for IPv6.
        if (NKN_IF_ALIAS == pns->if_type) {
            if (getsockopt(clifd, SOL_IP, IP_INDEV, if_name, &if_len) < 0) {
                DBG_LOG(MSG, MOD_HTTP, "getsockopt failed to get indev. Error: %d", errno);
            }
            else {
                DBG_LOG(MSG, MOD_HTTP, "getsockopt[indev] if_name: %s, pns_if: %s", if_name, pns->if_name);
            }

            if ('\0' == if_name[0]) {
                AO_fetch_and_add1(&glob_vip_parent_if_miss);
            }
            else {
                AO_fetch_and_add1(&glob_vip_parent_if_hit);
            }

            for (i32index = 0; i32index < MAX_NKN_INTERFACE; i32index++) {
                if (0 == strcmp(interface[i32index].if_name, if_name)) {
                    break;
                }
            }
            assert(i32index < MAX_NKN_INTERFACE);
            ppns = &interface[i32index];
        }


        // max_allowed_sessions is calculated based on ABR
        if (NKN_IF_ALIAS == pns->if_type) {
            if (AO_load(&ppns->tot_sessions) > ppns->max_allowed_sessions) {
                glob_overflow_socket_due_afr++;
                nkn_close_fd(clifd, NETWORK_FD);
                return TRUE;
            }
        }
        else {
            if (AO_load(&pns->tot_sessions) > pns->max_allowed_sessions) {
                glob_overflow_socket_due_afr++;
                nkn_close_fd(clifd, NETWORK_FD);
                return TRUE;
            }
        }

        // valid socket
        if (NM_set_socket_nonblock(clifd) == FALSE) {
            DBG_LOG(MSG, MOD_HTTP,
                    "Failed to set socket %d to be nonblocking socket.",
                    clifd);
            nkn_close_fd(clifd, NETWORK_FD);
            return TRUE;
        }

        // Bind this socket into this accepted interface
        if (bind_socket_with_interface) {
            NM_bind_socket_to_interface(clifd, pns->if_name);
        }
        addr.ss_family = AF_INET6;
        init_conn(sockfd, clifd, pns, ppns, &addr, gnm[sockfd].pthr->num);
    }
    return TRUE;
}

static int httpsvrv6_epollout(int sockfd, void * private_data)
{
        UNUSED_ARGUMENT(private_data);

        DBG_LOG(SEVERE, MOD_HTTP, "epollout_v6 should never called. sockid=%d", sockfd);
        DBG_ERR(SEVERE, "epollout_v6 should never called. sockid=%d", sockfd);
        assert(0); // should never called
        return FALSE;
}

static int httpsvrv6_epollerr(int sockfd, void * private_data)
{
        UNUSED_ARGUMENT(private_data);

        DBG_LOG(SEVERE, MOD_HTTP, "epollerr_v6 should never called. sockid=%d", sockfd);
        DBG_ERR(SEVERE, "epollerr_v6 should never called. sockid=%d", sockfd);
        assert(0); // should never called
        return FALSE;
}

static int httpsvrv6_epollhup(int sockfd, void * private_data)
{
        UNUSED_ARGUMENT(private_data);

        DBG_LOG(SEVERE, MOD_HTTP, "epollhup_v6 should never called. sockid=%d", sockfd);
        DBG_ERR(SEVERE, "epollhup_v6 should never called. sockid=%d", sockfd);
        assert(0); // should never called
        return FALSE;
}


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <linux/netfilter_ipv4.h>
#include <sys/prctl.h>
#include <arpa/inet.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>

#include <libgen.h>
#include <netdb.h>

#include "nkn_assert.h"
#include "nkn_debug.h"
#include "nkn_om.h"
#include "om_defs.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "network.h"
#include "server.h"
#include "nkn_cfg_params.h"
#include "fqueue.h"
#include "nkn_am_api.h"
#include "nkn_namespace.h"
#include "nkn_cod.h"
#include "om_fp_defs.h"
#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_pseudo_con.h"
#include "pe_defs.h"
#include "nkn_hash.h"
#include "nkn_tunnel_defs.h"
#include "nkn_defs.h"
#include "nkn_ssl.h"
#include "nkn_cim.h"

#define F_FILE_ID	LT_OM_NETWORK

NKNCNT_DEF(om_lookup_by_uri_offset_mismatch, int32_t, "", "")
NKNCNT_DEF(om_lookup_by_uri_no_cache_flag_set, int32_t, "", "")
NKNCNT_DEF(om_lookup_by_fd_stale_omcon, int32_t, "", "")
NKNCNT_DEF(om_lookup_by_uri_ocon_freed, int32_t, "", "")
NKNCNT_DEF(om_lookup_by_fd_ocon_freed, int32_t, "", "")
NKNCNT_DEF(tot_size_to_os_by_origin, int32_t, "", "to origin server by origin")
NKNCNT_DEF(om_open_server_conn, AO_t, "", "current open server connections")
NKNCNT_DEF(om_tot_completed_http_trans, int64_t, "", "total completed HTTP transaction")
NKNCNT_DEF(om_err_conn_failed, int32_t, "", "total failed socket connect")
NKNCNT_DEF(om_err_cp_activate_socket, int32_t, "", "total failed socket activate")
NKNCNT_DEF(om_err_cp_activate_2_socket, int32_t, "", "validate socket failed")
NKNCNT_DEF(om_err_get, int32_t, "", "OM_GET format is not right")
NKNCNT_DEF(oom_tot_reqs, int32_t, "", "offline OM total requests")
NKNCNT_DEF(oom_err_dropped, int32_t, "", "dropped offline OM requests")
NKNCNT_DEF(om_fetch_error, int32_t, "", "transfer from origin server error")
NKNCNT_DEF(om_server_down, int32_t, "", "server connect error")
NKNCNT_DEF(om_no_cache_unsup_resp, int32_t, "", "no-cache unsupported response")
NKNCNT_DEF(om_no_cache_chunked, int32_t, "", "no-cache chunked response")
NKNCNT_DEF(om_no_cache_expired, int32_t, "", "expired response")
NKNCNT_DEF(om_no_cache_am_no_ingest, int32_t, "", "AM no injest")
NKNCNT_DEF(om_no_cache_tran_enc, int32_t, "", "no-cache transfer encoding")
NKNCNT_DEF(om_no_cache_no_content_len, int32_t, "","no-cache no content length")
NKNCNT_DEF(om_no_cache_small_object, int32_t, "","no-cache small object")
NKNCNT_DEF(om_unchunk_cache, int32_t, "", "unchunk and cache object")
NKNCNT_DEF(om_stale_conn_close, int32_t, "", "stale OS connection close")
NKNCNT_DEF(om_con_steal, int32_t, "", "OS connection steal")
NKNCNT_DEF(om_nocache_set_cookie, int32_t, "", "no-cache set_cookie")
NKNCNT_DEF(om_nocache_already_expired, int32_t, "", "no-cache already expired")
NKNCNT_DEF(om_no_cache_large_header_resp, int32_t, "", "no-cache large http response")
NKNCNT_DEF(om_unchunk_simul_objs_cnt, int32_t, "", "unchunk exceeded parallel limit")
NKNCNT_DEF(om_set_connect_timer_retries, int32_t, "", "set connect timer retries")
NKNCNT_DEF(om_set_read_timer_retries, int32_t, "", "set read timer retries")
NKNCNT_DEF(om_delay_collision, int32_t, "", "Request collision delay")
NKNCNT_DEF(om_delay_behind, int32_t, "", "Request behind delay")
NKNCNT_DEF(om_delay_ahead, int32_t, "", "Request ahead delay")
NKNCNT_DEF(om_err_fd_inuse, int32_t, "", "FD already in use")
NKNCNT_DEF(om_connect_timeouts, int64_t, "", "Connect timeouts")
NKNCNT_DEF(om_read_timeouts, int64_t, "", "Read timeouts")
NKNCNT_DEF(om_read_tmout_partial_data, int64_t, "", "Read timeout resume with partial data")
NKNCNT_DEF(om_read_tmout_close, int64_t, "", "Read timeout close")
NKNCNT_DEF(om_read_val_tmout_close, int64_t, "", "Read timeout validate close")
NKNCNT_DEF(om_validators_all_ignore, int64_t, "", "Skipping COD version checks")
NKNCNT_DEF(tunnel_res_incapable_byte_range_origin, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_cod_expire, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_404_code, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_unsup_resp, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_query_str, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_byte_range_and_chunked, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_chunked_with_sub_encoding, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_no_content_len, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_hex_encode, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_no_conlen_or_no_chunk, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_conlen_zero, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_no_cache, int64_t, "", "tunnel")
NKNCNT_DEF(tunnel_res_pe, int64_t, "", "pe")
NKNCNT_DEF(tunnel_noetag_lmh, int64_t, "", "tunnel no etag/lmh")
NKNCNT_DEF(tunnel_res_parse_err_2, int64_t, "", "parse error 2")
NKNCNT_DEF(tunnel_res_parse_err_3, int64_t, "", "parse error 3")
NKNCNT_DEF(tunnel_res_parse_err_5, int64_t, "", "parse error 5")
NKNCNT_DEF(tunnel_res_parse_err_6, int64_t, "", "parse error 6")
NKNCNT_DEF(tunnel_res_parse_err_8, int64_t, "", "parse error 8")
NKNCNT_DEF(tunnel_res_parse_err_9, int64_t, "", "parse error 9")
NKNCNT_DEF(tunnel_res_parse_err_10, int64_t, "", "parse error 10")
NKNCNT_DEF(tunnel_res_parse_internal, int64_t, "", "parse internal")
NKNCNT_DEF(tunnel_res_os_no_support_byte_range, int64_t, "", "os does not support byte-range req")
NKNCNT_DEF(tunnel_object_chunked, int64_t, "", "Object is not cacheable")
NKNCNT_DEF(tunnel_http_byte_range_streaming, int64_t, "", "Object is not cacheable")
NKNCNT_DEF(tunnel_resp_cache_index_no_header, int64_t, "", "Object does not have required response header for cache index")
NKNCNT_DEF(tunnel_resp_cache_index_chunked_no_uri, int64_t, "", "Cache index response header based chunked object has no uri")
NKNCNT_DEF(tunnel_resp_cache_index_data_chunked, int64_t, "", "Cache index response data configured for chunked object")
NKNCNT_DEF(tunnel_resp_cache_index_out_of_range_data, int64_t, "", "Cache index response object has out of range data")
NKNCNT_DEF(tunnel_resp_cache_index_get_known_hdr_err, int64_t, "", "Cache index response header failed to get known header")
NKNCNT_DEF(tunnel_resp_cache_index_get_unknown_hdr_err, int64_t, "", "Cache index response header failed to get unknown header")
NKNCNT_DEF(tunnel_resp_cache_index_no_known_hdr, int64_t, "", "Cache index response header does not have configured known header")
NKNCNT_DEF(tunnel_resp_cache_index_get_cl_header_err, int64_t, "", "Cache index response header failed to get content length value from header")
NKNCNT_DEF(tunnel_resp_vary_field_not_supported, int64_t, "", "Vary header contains unsupported fields")
NKNCNT_DEF(tunnel_resp_vary_handling_disabled, int64_t, "", "Vary header handling not enabled ")
NKNCNT_DEF(om_connect_timer_expired_1, int64_t, "", "Connect timer expired")
NKNCNT_DEF(om_connect_timer_expired_2, int64_t, "", "Connect timer expired")
NKNCNT_DEF(om_connect_timer_expired_3, int64_t, "", "Connect timer expired")
NKNCNT_DEF(om_connect_timer_expired_4, int64_t, "", "Connect timer expired")
NKNCNT_DEF(om_connect_get_failover_retry, int64_t, "", "Connect timer expired in get path and failover retry done")
NKNCNT_DEF(om_connect_validate_failover_retry, int64_t, "", "Connect timer expired in validate path and failover retry done")
NKNCNT_DEF(om_read_timer_expired_1, int64_t, "", "Read timer expired")
NKNCNT_DEF(om_read_timer_expired_2, int64_t, "", "Read timer expired")
NKNCNT_DEF(om_err_init_client, int64_t, "", "Client init failed")
NKNCNT_DEF(om_err_init_client_retry, int64_t, "", "Client init fail retry")
NKNCNT_DEF(om_err_val_init_client, int64_t, "", "Client init failed in validate")
NKNCNT_DEF(om_err_val_init_client_retry, int64_t, "", "Client init fail retry in validate")
NKNCNT_DEF(om_err_add_event_epollin, int64_t, "", "add event epollin failed")
NKNCNT_DEF(om_init_client_no_ns, int64_t, "", "Client init, no namespace")
NKNCNT_DEF(om_init_client_no_config, int64_t, "", "Client init, no config")
NKNCNT_DEF(om_init_client_inv_os_select, int64_t, "", "Client init, invalid os select method")
NKNCNT_DEF(om_init_client_malloc_failed, int64_t, "", "Client init, malloc failed")
NKNCNT_DEF(om_init_client_find_os_failed, int64_t, "", "Client init, find origin server failed")
NKNCNT_DEF(om_init_client_open_socket_failed, int64_t, "", "Client init, open socket failed")
NKNCNT_DEF(om_err_leak_task_detected, int64_t, "", "found leaking task")
NKNCNT_DEF(om_err_leak_task_detected_returned, int64_t, "", "found leaked and returned task")
NKNCNT_DEF(om_err_leak_task_server_fetch_error, int64_t, "", "found leaked set error")
NKNCNT_DEF(om_redirect_302, AO_t, "", "Number of 302 redirects serviced")
NKNCNT_DEF(om_in_uri_hashtable, int64_t, "", "Already in URI hashtable")
NKNCNT_DEF(origin_send_tot_bytes, AO_t, "", "Total bytes sent to origin")
NKNCNT_DEF(tot_size_from_internal_and_origin, unsigned long long, "", "total size")
NKNCNT_DEF(cache_encoding_mismatch, uint64_t, "", "cache encoding mismatch")
NKNCNT_DEF(ssl_fid_index, AO_t, "", "ssl connection shm fd index")
NKNCNT_DEF(om_dns_mismatch, AO_t, "", "dns mismatch between client and mfc")
NKNCNT_DEF(om_vary_hdr_user_agent, uint64_t, "", "vary user agent")
NKNCNT_DEF(om_vary_hdr_accept_encoding, uint64_t, "", "vary accept encoding")
NKNCNT_DEF(om_aws_auth_cnt, uint64_t, "", "aws authroization count")
NKNCNT_DEF(om_validate_aws_auth_cnt, uint64_t, "", "aws authroization count")

/* config to set the revalidation method type to GET+IMS (default is HEAD) */
int nkn_http_reval_meth_get = 0;

extern NCHashTable *om_con_by_uri;
extern NCHashTable *om_con_by_fd;
extern int nkn_max_domain_ips;

NKNCNT_EXTERN(cp_tot_cur_open_sockets, AO_t)
NKNCNT_EXTERN(cp_tot_cur_open_sockets_ipv6, AO_t)
NKNCNT_EXTERN(cp_tot_closed_sockets, AO_t)
NKNCNT_EXTERN(cp_tot_closed_sockets_ipv6, AO_t)

extern char *get_ip_str(ip_addr_t *ip_ptr);
extern int om_http_parse_resp(omcon_t * ocon);
extern int OM_get_ret_task(omcon_t * ocon);
extern int OM_get_ret_error_to_task(omcon_t * ocon);
extern int dns_hash_and_mark(char *domain, ip_addr_t ip, int *found);
int32_t is_dns_mismatch(char *host_name, ip_addr_t client_ip, int32_t family);
extern int nkn_strcmp_incase(const char * p1, const char * p2, int n);
extern hash_table_def_t ht_local_ip;
extern int http_create_header_cluster(con_t *con, int hdr_token, char *header_cluster, int *header_cluster_len_p);

/* mutex to protect om connection data */
nkn_mutex_t omcon_mutex;
AO_t glob_om_switch_to_4k_page_failed;
uint64_t glob_retry_om_by_fd;
uint64_t glob_retry_om_by_uri;
uint64_t glob_retry_om_close_fd;
extern unsigned long long glob_total_bytes_from_internal_and_origin;
pthread_mutex_t om_oomgr_req_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t om_oomgr_req_queue_cv;

static int om_close_conn(omcon_t * con);
void omq_head_leak_detection(void);
void call_om_close_conn(void * ocon);


#define DBG(_fmt, ...) DBG_LOG(MSG, MOD_OM, _fmt, __VA_ARGS__)

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

static int om_makecopy_to_bufmgr(omcon_t * ocon, int * copylen);
static int update_cache_control_header (mime_header_t *hdr, const char *policy_max_age, 
        int policy_max_age_strlen, int fd);
int apply_cc_max_age_policies(origin_server_policies_t *origin_policies,
				     mime_header_t *hdr, int fd);
static int get_cache_age_policies(origin_server_policies_t *origin_policy,
				     mime_header_t *hdr, int fd, int *);
static int parse_cc_headers_to_mask(omcon_t *ocon);
static int cp_callback_http_body_received(void * private_data, char * p, int * len, int * shouldexit);
static char om_used_fds[roundup((MAX_GNM), NBBY)/NBBY] = {0};

static int setup_connect_timer(omcon_t *ocon);
static int setup_read_timer(omcon_t *ocon);
static int setup_idle_timeout(omcon_t *ocon);
static int cancel_timer(omcon_t *ocon);
static int set_uncond_connect_retry(omcon_t *ocon);
static int set_uncond_read_retry(omcon_t *ocon);
static int set_uncond_escal_retry(omcon_t *ocon);
static int set_task_uncond_taskstart_retry(void *taskptr, int is_get_task);
static int set_failover_connect_retry(omcon_t *ocon);
static int set_failover_delay_retry(omcon_t *ocon) __attribute__ ((unused));
static int resp_header_cache_index(omcon_t *ocon);
static int resp_data_cache_index(omcon_t *ocon);

int
parse_location_header(const char *loc, int loclen, char **host, int *hostlen,
		char **port, int *portlen, char **uri, int *urilen);

static int parse_uri_2(const char *uri, int urilen,
		char **dir, int *dirlen,
		char **file, int *filelen);

static int
process_302_redirect_response(omcon_t *ocon);

static inline int is_nvsd_listen_server(ip_addr_t* ipv4v6, uint16_t port)
{
        int i, j, k;
        uint32_t ip_h;
        //uint32_t ipv4;
        int rv;
        char ip_host[NI_MAXHOST];

        if (ipv4v6->family == AF_INET) {
            struct in_addr inaddr4;
                inaddr4.s_addr = IPv4((*ipv4v6));
                ip_h = ntohl(inaddr4.s_addr);
                /*************************************************************
                * Avoid 0.0.0.0, or any local IPs all together
                * If ip is between 127.0.0.0 and 127.255.255.255, then
                * we should not connect
                ************************************************************/
                if (0 == ip_h) {
                    return TRUE;
                }

                if (!inet_ntop(AF_INET, &inaddr4, ip_host, NI_MAXHOST)) {
                    return TRUE;
                }


                rv = ht_local_ip.lookup_func(&ht_local_ip, ip_host, strlen(ip_host), &i);
                DBG_LOG(WARNING, MOD_OM, "IP to check is %s, rv=%d", ip_host, rv);

                if (!rv) {
                    for(j=0; j<MAX_PORTLIST; j++) {
                        if(port==htons(nkn_http_serverport[j])) {
                            return TRUE;
                        }
                    }
                }

                return FALSE;

        } else {//ipv6, todo, implement ipv6 zero compare
                if (IN6_IS_ADDR_UNSPECIFIED(&(IPv6((*ipv4v6))))) {
                        return TRUE;
                }
                for (i = 0; i < glob_tot_interface; i++) {
                        for (j = 0; j < (int)interface[i].ifv6_ip_cnt; j++) {
                                if (memcmp(&interface[i].if_addrv6[j].if_addrv6, &IPv6((*ipv4v6)),
                                                 sizeof (struct in6_addr)) == 0) {
                                        for(k=0; k<MAX_PORTLIST; k++) {
                                                if(port==htons(interface[i].if_addrv6[j].port[k])) {
                                                        return TRUE;
                                                }
                                        }
                                }
                        }
                }
        }
        return FALSE;
}

static void nhash_delete_omcon_by_fd(omcon_t * ocon)
{
        uint64_t keyhash1;

        if (ocon->hash_fd != -1) {
                keyhash1 = n_uint64_hash(&ocon->hash_fd);
                nchash_remove(om_con_by_fd, keyhash1, &ocon->hash_fd);
                ocon->hash_fd = -1;
        }
}

static void nhash_delete_omcon_by_uri(omcon_t * ocon)
{
        uint64_t keyhash2;

        if (ocon->hash_cod_flag != 1) {
                keyhash2 = n_dtois_hash(&ocon->hash_cod);
                nchash_remove_off0(om_con_by_uri, keyhash2, &ocon->hash_cod);
                ocon->hash_cod_flag = 1;
        }
}

#define RECORD_OCON_IN_TASK 1

static void free_omcon_t(omcon_t *om)
{
	if (!om) {
		return;
	}
	if (om->uol.cod) {
		nkn_cod_close(om->uol.cod, NKN_COD_ORIGIN_MGR);
		om->uol.cod = NKN_COD_NULL;
	}
	om->physid[0] = '\0';
        om->fd = 0;
        om->incarn = 0;

	if(om->oscfg.ip) free(om->oscfg.ip);

	if (om->chunk_encoding_data) {
		ce_free_chunk_encoding_ctx_t(om->chunk_encoding_data);
		om->chunk_encoding_data = 0;
	}

	if (om->chunk_encoding_data_parser) {
		ce_free_chunk_encoding_ctx_t(om->chunk_encoding_data_parser);
		om->chunk_encoding_data_parser = 0;
	}

	if (om->hdr_serialbuf) {
		free(om->hdr_serialbuf);
		om->hdr_serialbuf = 0;
	}

	if (OM_CHECK_FLAG(om, OMF_DO_VALIDATE) && om->httpreqcon) {
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)om->httpreqcon);
		om->httpreqcon = 0;
	}
	om->resp_mask = 0;
	DBG_LOG(MSG, MOD_OM, "free_omcon_t: free om=%p", om);
	if ((om->comp_status==1) && om->comp_task_ptr) {
	    free(om->comp_task_ptr);
	}

	free(om);
	AO_fetch_and_sub1(&glob_om_open_server_conn);
}

int
om_delete_check(void *key, void *value, void *user_data);
int
om_delete_check(void *key, void *value, void *user_data)
{
    UNUSED_ARGUMENT(key);
    UNUSED_ARGUMENT(user_data);
    int32_t fd;
    omcon_t *ocon = (omcon_t *)value;
    fd = OM_CON_FD(ocon);
    if(nkn_cur_ts - gnm[fd].last_active > http_idle_timeout * 2) {
	return 1;
    }
    return 0;
}


void omq_head_leak_detection()
{
        omcon_t *ocon;
        int32_t fd;
        cpcon_t *cpcon;
        static time_t nkn_last_ts = 0;
	void * key = NULL;
	struct network_mgr * pnm = NULL;

        // Try to detect leaking task in OM once every 300 sec (5 min)
        if(nkn_cur_ts - nkn_last_ts < 300) {
                return;
        }
        nkn_last_ts = nkn_cur_ts;
again:
	NKN_MUTEX_LOCK(&omcon_mutex);
	ocon = nchash_find_match(om_con_by_fd, om_delete_check,
				     NULL, &key);
	if (ocon)
	    goto do_delete;
	NKN_MUTEX_UNLOCK(&omcon_mutex);
	return;

do_delete:
	/*
	 * TODO: At code re-org time, we should add code to acquire
         *       gnm[fd].mutex before operate on cpcon.
         */
        fd = OM_CON_FD(ocon);
        if ((fd<0) || (fd>MAX_GNM)) { assert(0); }
        pnm=&gnm[fd];
        pthread_mutex_lock(&pnm->mutex);
        NM_TRACE_LOCK(pnm, LT_OM_NETWORK);
        cpcon = pnm->private_data; // valid cpcon pointer
        if( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ||
            (cpcon == NULL) || // cpcon has been freed
            (pnm->fd != fd) || // wrong fd
            (pnm->incarn != ocon->incarn) || // wrong incarn
            (cpcon->type != CP_APP_TYPE_OM) || // not right type
            (cpcon->private_data != (void *)ocon) // not right ocon
             ) {
                glob_om_err_leak_task_detected ++;
                DBG_ERR(MSG, "Detected one leaking task (omcon->fd=%d)", fd);

                nhash_delete_omcon_by_fd(ocon);
                nhash_delete_omcon_by_uri(ocon);
                clrbit(om_used_fds, fd);
                NKN_MUTEX_UNLOCK(&omcon_mutex);
                if (ocon->p_mm_task && OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
                        /*
                         * Detected a leaking task in OM
                         * but also MM said that this task has been returned
                         * Then we do nothing except clean up ocon memory.
                         */
                        if (ocon->p_mm_task->ocon != ocon) {
                                glob_om_err_leak_task_detected_returned ++;
                                ocon->p_mm_task = NULL;
                        }
                        else {
                                glob_om_err_leak_task_server_fetch_error ++;
                                ocon->p_mm_task->err_code = NKN_OM_LEAK_DETECT_ERROR;
                        }
                }
                else if (ocon->p_validate && OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
                        ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
                }

                om_close_conn(ocon);
                NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
                pthread_mutex_unlock(&pnm->mutex);

                goto again; // Try again
        }
        NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
        pthread_mutex_unlock(&pnm->mutex);

        /*
         * Although this task has been more than http_idle_timeout,
         * But it has not been leaked yet.
         * It is still linked in cpcon and it is valid cpcon.
         */
        NKN_MUTEX_UNLOCK(&omcon_mutex);
        return;
}

static void check_dns_mismatch(con_t *httpcon, ip_addr_t *ipv4v6)
{
	int rv;
	int hdrcnt;
	int datalen;
	u_int32_t attrs;
	uint32_t destip;
	const char *data;
	struct in_addr inaddr4;

	if (httpcon == NULL) return;
	if (ipv4v6->family != AF_INET) return;
	rv = get_known_header(&httpcon->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
						    &data, &datalen, &attrs, &hdrcnt);
	if (rv == 0) {
		destip = inet_addr(data);
		inaddr4.s_addr = IPv4((*ipv4v6));

		if (destip != inaddr4.s_addr) {
			AO_fetch_and_add1(&glob_om_dns_mismatch);
		}
	}

	return;
}

static int have_chunk_encoded_eof(omcon_t * ocon, int copylen)
{
	int rv;
	int chunk_encoding_eof = 0;
	chunk_encoding_input_args_t ce_args;
	nkn_iovec_t input_iovec;
	nkn_iovec_t dummy_iovecs[MAX_CM_IOVECS];
	nkn_buffer_t *bufs[MAX_CM_IOVECS];
	int used_out_iovecs;
	off_t out_logical_data_offset;
	off_t out_data_length;
	mime_header_t *trailer_hdrs = 0;
    
	if (!ocon->chunk_encoding_data_parser) {
		ocon->chunk_encoding_data_parser = 
			    ce_alloc_chunk_encoding_ctx_t(-1, 0, 0, 0);
		    if (!ocon->chunk_encoding_data_parser) {
			    DBG_LOG(MSG, MOD_OM,
			    "ce_alloc_chunk_encoding_ctx_t() failed "
			    "unchunking disabled");
			    OM_UNSET_FLAG(ocon, OMF_UNCHUNK_CACHE);

			    return 0;
		    }
	}

    	// Parse current segment

    	ce_args.in_logical_data_offset = ocon->uol.offset + 
		ocon->thistask_datalen;
	if (!ce_args.in_logical_data_offset) {
		ce_args.in_data_offset = ocon->phttp->cb_bodyoffset;
	} else {
    		ce_args.in_data_offset = 0;
	}

    	input_iovec.iov_base = &ocon->phttp->cb_buf[ocon->phttp->cb_offsetlen];
    	input_iovec.iov_len = copylen; //ocon->phttp->cb_totlen;

    	ce_args.in_iovec = &input_iovec;
    	ce_args.in_iovec_len = 1;

    	ce_args.out_iovec = dummy_iovecs;
    	ce_args.out_iovec_len = MAX_CM_IOVECS;

    	ce_args.out_iovec_token = bufs;
    	ce_args.out_iovec_token_len = MAX_CM_IOVECS;

    	ce_args.used_out_iovecs = &used_out_iovecs;
    	ce_args.out_logical_data_offset = &out_logical_data_offset;
    	ce_args.out_data_length = &out_data_length;
    	ce_args.trailer_hdrs = &trailer_hdrs;

  	rv = ce_chunk2data(ocon->chunk_encoding_data_parser, &ce_args);
	if (rv < 0) {
		chunk_encoding_eof = 1;
	}

    	if (trailer_hdrs) {
    		shutdown_http_header(trailer_hdrs);
	    	free(trailer_hdrs);
    	}
	return chunk_encoding_eof;
}

/* ==================================================================== */
/*
 * Copy data from network buffer to buffer manager
 * the data copy logic is chunk-aware.
 *
 * Input:
 *   ocon:     omcon_t structure
 *   *copylen: total these bytes data in the cb_buf. 
 *             copylen includes chunked header size too.
 *             data is saved in the buffer &cb_buf[phttp->cb_offsetlen].
 *	       *copylen may or maynot be equal to cb_totlen. Most time they are equal.
 * Return value:
 *   *copylen: these bytes data left, which has not been copied yet.
 *   return one of the following MACRO
 */
#define MAKECOPY_NO_TASK	1
#define MAKECOPY_NO_DATA	2
#define MAKECOPY_NO_IOVECS	3
#define MAKECOPY_IOVECS_FULL	4
#define MAKECOPY_CL_EOT		5
#define MAKECOPY_CHUNKED_EOT	6
#define MAKECOPY_HEAD_EOT	7
#define MAKECOPY_MORE_DATA	8
#define MAKECOPY_COMPRESS_TASK_POSTED 9

static int om_makecopy_to_bufmgr(omcon_t * ocon, int * copylen)
{
	http_cb_t * phttp = ocon->phttp;
	int num;
	off_t offset, cplen;
	off_t unaligned_offset;
	int nocache_head_req;
	char * p;
	con_t *httpreqcon = NULL;

	if (!ocon->p_mm_task) {
		return MAKECOPY_NO_TASK;
	}
	// No more data there
	if(copylen==NULL || *copylen==0) {

		return MAKECOPY_NO_DATA;
	}

	/*
	 * unknown reason why this value could be zero
	 * Maybe we should call assert here
	 */
	if(!phttp || phttp->num_nkn_iovecs == 0) {
		return MAKECOPY_NO_IOVECS;
	}

	/*
	 * if RBCI is configured, start location (for data copy) should
	 * be offset zero in iov[0] since the RBCI probe request is for
	 * first n(configured) bytes.
	 */
	httpreqcon = (con_t *)(ocon->p_mm_task->in_proto_data);
	if (httpreqcon && CHECK_CON_FLAG(httpreqcon, CONF_REFETCH_REQ)) {
		// finding where to copy
		offset = ocon->thistask_datalen;
		for(num=0; num<phttp->num_nkn_iovecs; num++) {
			if(offset >= phttp->content[num].iov_len) {
				offset -= phttp->content[num].iov_len;
			}
			else {
				break;
			}
		}
		nocache_head_req = 0;
		// For RBCI probe request skip offset calculations so that the
		// out_num_content_objects is set to 1 always, PR 785691.
		OM_SET_FLAG(ocon, OMF_BUF_CPY_SKIP_OFFSET);
		goto iov_copy;
	}

	// non-cacheable no content length HEAD request
	if (OM_CHECK_FLAG(ocon, OMF_NO_CACHE) &&
	    (ocon->content_length == OM_STAT_SIG_TOT_CONTENT_LEN) &&
	    CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_METHOD_HEAD)) {
	    	// Only send headers
	    	DBG_LOG(MSG, MOD_OM, 
			"no-cache HEAD request, "
			"fd=%d cb_totlen=%d cb_bodyoffset=%ld",
			OM_CON_FD(ocon), phttp->cb_totlen, phttp->cb_bodyoffset);
		//phttp->cb_totlen = phttp->cb_bodyoffset;
		OM_SET_FLAG(ocon, OMF_ENDOF_TRANS);
		nocache_head_req = 1;
	} else {
		nocache_head_req = 0;
	}

	if (!OM_CHECK_FLAG(ocon, OMF_CHUNK_EOF) &&
	    OM_CHECK_FLAG(ocon, (OMF_UNCHUNK_CACHE | OMF_UNCHUNK_NOCACHE))) {
	    if(!IS_PSEUDO_FD(ocon->httpfd)) {
		if (have_chunk_encoded_eof(ocon, *copylen)) {
			OM_SET_FLAG(ocon, OMF_CHUNK_EOF);
		}
	    }
	}

	// Adjustments for unaligned start offset

	if (!ocon->thistask_datalen) {
	    offset = ocon->content_start + ocon->content_offset;
	    unaligned_offset = offset % CM_MEM_PAGE_SIZE;

	    if (unaligned_offset) {
	    	phttp->content[0].iov_base += unaligned_offset;
	    	phttp->content[0].iov_len = CM_MEM_PAGE_SIZE - unaligned_offset;

	    	DBG_LOG(MSG, MOD_OM, "unaligned copy fd=%d offset=0x%lx(%ld)", 
		    	OM_CON_FD(ocon), offset, offset);
	    } else {
	    	DBG_LOG(MSG, MOD_OM, "copy fd=%d offset=0x%lx(%ld)", 
	    		OM_CON_FD(ocon), offset, offset);
	    }
	}

	// finding where to copy
	offset = ocon->thistask_datalen;
	for(num=0; num<phttp->num_nkn_iovecs; num++) {
		if(offset >= phttp->content[num].iov_len) {
			offset -= phttp->content[num].iov_len;
		}
		else {
			break;
		}
	}
	if(num==phttp->num_nkn_iovecs) {
		// All space has been used. Not enough space in iovec.
#if 1
		DBG_LOG(MSG, MOD_OM, 
			"No space fd=%d end_offset=0x%lx(%ld) thistask_datalen=0x%lx(%ld)",
			OM_CON_FD(ocon),
			ocon->content_data_offset, ocon->content_data_offset,
			ocon->thistask_datalen, ocon->thistask_datalen);
#endif
		return MAKECOPY_IOVECS_FULL;
	}

        if ((ocon->content_length <= (CM_MEM_MIN_PAGE_SIZE * 4)) &&
                         !OM_CHECK_FLAG(ocon, OMF_BUFFER_SWITCHED)) {
                int ret = nkn_buffer_set_bufsize(ocon->p_mm_task->in_content_array[0],
                                                (CM_MEM_MIN_PAGE_SIZE * 4), BM_CACHE_ALLOC);
                if (!ret) {
                        phttp->content[0].iov_base =
                            nkn_buffer_getcontent(ocon->p_mm_task->in_content_array[0]);
                        phttp->content[0].iov_len =
                            nkn_buffer_get_bufsize(ocon->p_mm_task->in_content_array[0]);
			if (unaligned_offset) {
				phttp->content[0].iov_base += unaligned_offset;
				phttp->content[0].iov_len = (4 * CM_MEM_MIN_PAGE_SIZE) - unaligned_offset;
			}
                } else {
			AO_fetch_and_add1(&glob_om_switch_to_4k_page_failed);
		}
                OM_SET_FLAG(ocon, OMF_BUFFER_SWITCHED);
	}

iov_copy:
	// Let's do the real copy
	p = &phttp->cb_buf[phttp->cb_offsetlen];
	for( ; num<phttp->num_nkn_iovecs; num++) {

		cplen = (phttp->content[num].iov_len - offset >= *copylen) ?
			*copylen : phttp->content[num].iov_len - offset;

		memcpy( phttp->content[num].iov_base + offset,
			p,
			cplen);
		offset = 0;
		phttp->tot_reslen += cplen;
		ocon->thistask_datalen += cplen;

		glob_tot_size_from_internal_and_origin += cplen;
		ocon->content_data_offset += cplen;
		p += cplen;

		*copylen -= cplen;
		if(*copylen ==  0) {
			break;
		}
	}

	if (is_known_header_present(&phttp->hdr, MIME_HDR_CONTENT_LENGTH)) {
		int http_hdr_len;
		if (OM_CHECK_FLAG(ocon, OMF_NO_CACHE_BM_ONLY)) {
			http_hdr_len = 0;
		} else {
			http_hdr_len = OM_CHECK_FLAG(ocon, OMF_NO_CACHE) ?  
					phttp->http_hdr_len : 0;
		}
	   	if (phttp->tot_reslen + ocon->range_offset >= 
			http_hdr_len + ocon->content_length) {
			/* EOF, received content_length bytes */

			OM_SET_FLAG(ocon, OMF_ENDOF_TRANS);
			return MAKECOPY_CL_EOT;
		}

		
	}

#if 1
	if (num == phttp->num_nkn_iovecs) {
		DBG_LOG(MSG, MOD_OM, 
			"Full fd=%d end_offset=0x%lx(%ld) thistask_datalen=0x%lx(%ld)",
			OM_CON_FD(ocon),
			ocon->content_data_offset, ocon->content_data_offset,
			ocon->thistask_datalen, ocon->thistask_datalen);
	}
#endif

	if (OM_CHECK_FLAG(ocon, OMF_CHUNK_EOF) &&
	    (*copylen == 0)) {
	    //(phttp->cb_totlen == 0)) {
		ocon->p_mm_task->out_proto_resp.u.OM.flags |= 
			OM_PROTO_RESP_NO_CONTENT_LEN_EOF;
		OM_SET_FLAG(ocon, OMF_ENDOF_TRANS);
		return MAKECOPY_CHUNKED_EOT;
	} 

	if (num==phttp->num_nkn_iovecs) {
		return MAKECOPY_IOVECS_FULL;
	}

	if (nocache_head_req) {
		return MAKECOPY_HEAD_EOT;
	}

	return MAKECOPY_MORE_DATA;
}

 
/*
 * socket functions
 */


/* ==================================================================== */
/*
 * Connection Pool module callback functions
 */
static int cp_callback_connected(void * private_data)
{
	int i = 0 ;
	int match_found = 0;
	/* do nothing at this time. */
	omcon_t * ocon = (omcon_t *)private_data;
	if (ocon->cpcon) ocon->state = ((cpcon_t *) ocon->cpcon)->state;
	((cpcon_t *) ocon->cpcon)->nsconf = ocon->oscfg.nsc;

	cancel_timer(ocon);

	if(OM_CHECK_FLAG(ocon, OMF_DO_GET) && 
			ocon->httpreqcon->origin_servers.count < MAX_ORIG_IP_CNT) {
		for(i = 0 ; i <  ocon->httpreqcon->origin_servers.count; i++) {
			if(ocon->httpreqcon->origin_servers.ipv4v6[i].family == AF_INET) {
			    if(IPv4(ocon->httpreqcon->origin_servers.ipv4v6[i]) == 
					IPv4((((cpcon_t *) ocon->cpcon)->remote_ipv4v6))) {
				match_found = 1; 
				break;
			    }
			} else if(ocon->httpreqcon->origin_servers.ipv4v6[i].family == AF_INET6) {
			    if(memcmp(&IPv6(ocon->httpreqcon->origin_servers.ipv4v6[i]), 
					&IPv6((((cpcon_t *) ocon->cpcon)->remote_ipv4v6)), 
					sizeof(struct in6_addr)) == 0) {
				match_found = 1;
				break;
			    }
			}
		}
		if(!match_found) {
	               memcpy(&ocon->httpreqcon->origin_servers.ipv4v6[ocon->httpreqcon->origin_servers.count], 
				&((cpcon_t *) ocon->cpcon)->remote_ipv4v6, sizeof(ip_addr_t));
		
	               ocon->httpreqcon->origin_servers.count++;
		}

		memcpy(&ocon->httpreqcon->origin_servers.last_ipv4v6,
			 &((cpcon_t *) ocon->cpcon)->remote_ipv4v6, sizeof(ip_addr_t));

        }

	// For Origin failover case, we've to setup the socket idle timeout
	// value.
	if (http_cfg_fo_use_dns) {
		setup_idle_timeout(ocon);
	}

	DBG_LOG(MSG, MOD_OM, "connect(fd=%d) succeeded", OM_CON_FD(ocon));
	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_CONNECTED);
	return 0;
}

static void cp_callback_connect_failed(void * private_data, int called_from)
{
	int error = 0;
	int len = 0, found = 0;
	int err_code = NKN_OM_SERVER_DOWN;
	ip_addr_t remote_ip;

	UNUSED_ARGUMENT(called_from);

	omcon_t * ocon = (omcon_t *)private_data;
	if (ocon->cpcon) {
		ocon->state = ((cpcon_t *) ocon->cpcon)->state;
		remote_ip = ((cpcon_t *) ocon->cpcon)->remote_ipv4v6;
	}

	DBG_LOG(MSG, MOD_OM, "connect(fd=%d) failed", OM_CON_FD(ocon));
	// Socket connection failed
	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_CONNECT_FAILED);

	cancel_timer(ocon);

	len = sizeof(error);
	if (getsockopt(OM_CON_FD(ocon), SOL_SOCKET, SO_ERROR, 
				(void *)&error, (socklen_t *)&len)) {
		DBG_LOG(MSG, MOD_OM, "getsockopt(fd=%d) failed, errno=%d", 
				OM_CON_FD(ocon), errno);
	} else {
		if (error == ECONNREFUSED) {
			err_code = NKN_OM_SERVICE_UNAVAILABLE;
		}
	}

	if (OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		if(ocon->p_mm_task) {
			if (ocon->p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
				DBG_TRACE("OM connect failed %s:%hu for object %s",
					get_ip_str(&(ocon->phttp->remote_ipv4v6)),
					ocon->phttp->remote_port,
				nkn_cod_get_cnp(ocon->uol.cod));
			}

			if (ocon->oscfg.request_policies->connect_timer_msecs) {
				// Return uncondtional retry indication
				if (set_uncond_connect_retry(ocon)) {
					// Retries exceeded
					glob_om_connect_timer_expired_1++;
					ocon->p_mm_task->err_code = err_code;
				}
			} else {
				con_t *httpreqcon = ocon->httpreqcon;
				if (http_cfg_fo_use_dns && httpreqcon &&
				   (!CHECK_CON_FLAG(httpreqcon, CONF_DNS_HOST_IS_IP) &&
				    !CHECK_HTTP_FLAG(&httpreqcon->http, HRF_MFC_PROBE_REQ))) {
					const char  *loc_cnp = NULL;

					loc_cnp = nkn_cod_get_cnp(ocon->uol.cod) ;
					DBG_LOG(MSG, MOD_OM,
						"Origin Server connect failed in GET path for %s:%hu for object %s."
						"Trying to connect to next server...",
						get_ip_str(&remote_ip), ocon->phttp->remote_port,
						loc_cnp) ;

					if (ocon->p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
						DBG_TRACE( "DNS_MULTI_IP_FO: Origin Server connect failed in GET path for %s:%hu for object %s."
							"Trying to connect to next server...",
							get_ip_str(&remote_ip), ocon->phttp->remote_port,
							loc_cnp);
					}

					dns_hash_and_mark(ocon->oscfg.ip, remote_ip, &found);

					switch (httpreqcon->os_failover.num_ip_index) {
					case 1:
						set_failover_connect_retry(ocon);
						OM_SET_FLAG(ocon, OMF_OS_FAILOVER_REQ);
						glob_om_connect_get_failover_retry++;
						break;
					case 2:
					case 3:
						ocon->p_mm_task->err_code = NKN_MM_UNCOND_RETRY_REQUEST;
						glob_om_connect_get_failover_retry++;
						break;
					default:
						ocon->p_mm_task->err_code = NKN_OM_ORIGIN_FAILOVER_ERROR;
						break;
					}
				} else {
					ocon->p_mm_task->err_code = err_code;
				}
			}
		}
	}
	else { //OMF_DO_VALIDATE
		if(ocon->p_validate) {
			if (ocon->oscfg.request_policies->connect_timer_msecs) {
				// Return uncondtional retry indication
				if (set_uncond_connect_retry(ocon)) {
					// Retries exceeded
					glob_om_connect_timer_expired_2++;
					ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				}
			} else {
				con_t *org_httpreqcon = ocon->httpreqcon;
				con_t *request_con = (con_t *)ocon->p_validate->in_proto_data;
				if (http_cfg_fo_use_dns &&
				   (!CHECK_CON_FLAG(request_con, CONF_DNS_HOST_IS_IP)) &&
				   (VALID_NS_TOKEN(ocon->p_validate->ns_token) &&
				    strcmp(ocon->oscfg.nsc->namespace, "mfc_probe") != 0)) {
					const char  *loc_cnp = NULL;

					loc_cnp = nkn_cod_get_cnp(ocon->uol.cod) ;

					DBG_LOG(MSG, MOD_OM,
						"Origin Server connect failed in VALIDATION path for %s:%hu for object %s."
						"Trying to connect to next server...",
						get_ip_str(&remote_ip), ocon->phttp->remote_port,
						loc_cnp);
					if (CHECK_HTTP_FLAG(&org_httpreqcon->http, HRF_TRACE_REQUEST)) {
						DBG_TRACE("Origin Server connect failed in VALIDATION path for %s:%hu for object %s."
							  "Trying to connect to next server...",
							  get_ip_str(&remote_ip), ocon->phttp->remote_port, loc_cnp);
					}

					dns_hash_and_mark(ocon->oscfg.ip, remote_ip, &found);

					switch (org_httpreqcon->os_failover.num_ip_index) {
					case 1:
						set_failover_connect_retry(ocon);
						OM_SET_FLAG(ocon, OMF_OS_FAILOVER_REQ);
						glob_om_connect_validate_failover_retry++;
						break;
					case 2:
					case 3:
						ocon->p_validate->ret_code = NKN_MM_UNCOND_RETRY_REQUEST;
						glob_om_connect_validate_failover_retry++;
						break;
					default:
						ocon->p_validate->ret_code = NKN_OM_ORIGIN_FAILOVER_ERROR;
						break;
					}
				} else {
					ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				}
			}
		}
	}

	glob_om_server_down++;
	glob_om_err_conn_failed++;
}

#define OUTBUF(_data, _datalen) { \
        if(om_write_outbuf((_data), (_datalen), outbuf_p,outbufsz_p,&bytesused) \
                 == NULL) { return NULL; \
        }\
}

static inline char *
om_write_outbuf(const char *data, int datalen  /* Input */,
       char **outbuf_p, int *outbufsz_p, int *bytesused_p)
{
	int   bytesused = *bytesused_p;
	char *outbuf    = *outbuf_p;
	int   outbufsz  = *outbufsz_p;

	if (datalen + bytesused >= outbufsz ) {
		if (outbufsz < 16*1024) {
                       /* Double output buf size */
                        outbufsz  *= 2 ;
			*outbufsz_p = outbufsz ;

			outbuf = (char *)nkn_realloc_type(*outbuf_p, outbufsz, mod_om_ocon_t);
                        if (!outbuf) {
			    DBG_LOG(MSG, MOD_OM, "Failed to reallocate output buf. size: %d", 
                                                 outbufsz) ;
                            return NULL;
                        }
			*outbuf_p = outbuf;
			DBG_LOG(MSG, MOD_OM, "Output buf size doubled. datalen(%d)+bytesused(%d) >= outbufsz(%d)",
			                    datalen, bytesused, outbufsz);
                } else {
			DBG_LOG(MSG, MOD_OM, "Buf size > 16K, truncate buf. datalen(%d)+bytesused(%d) >= outbufsz(%d)",
			                     datalen, bytesused, outbufsz);
			outbuf[outbufsz-1] = '\0';
			return outbuf; 
		} 
	} 

        memcpy((void*)(outbuf + bytesused), data, datalen);
        *bytesused_p = bytesused + datalen ;
        return outbuf ;
}

static char *om_build_http_request(omcon_t *ocon, char **outbuf_p, int *outbufsz_p);

static char *om_build_http_request(omcon_t *ocon, char **outbuf_p, int *outbufsz_p)
{
	int rv;
	con_t *httpreqcon = ocon->httpreqcon;
    	int token;
    	int nth;
    	const char *name;
    	int namelen;
	const char *data;
	int datalen;
    	u_int32_t attrs;
    	int hdrcnt;
	int bytesused = 0;
	char range_value[128];
        int  range_value_set = 0;
	char *querystr;
	u_int8_t user_attr = 0;
	header_descriptor_t *hd;
	char *outbuf = *outbuf_p;
	int x_forward_hdr_no = -1;
	const char *location;
	int locationlen;
	int add_host_302 =0 ;
	int16_t encoding_type = -1;
	off_t rbci_offset, rbci_size, content_len;
	off_t rbci_offset_start, rbci_offset_end;
	char buf[32];
        char header_cluster[MAX_HTTP_HEADER_SIZE + 1];
        int header_cluster_len;
	const char head_method[] = "HEAD";
	int loc_has_query = 0;
	const char get_method[] = "GET";
	const char *uri;
	int urilen;
	const char *method;
	int methodlen;

	if (!httpreqcon) {
		DBG_LOG(MSG, MOD_OM, 
			"Invalid input, fd=%d httpreqcon=%p",
			OM_CON_FD(ocon), httpreqcon);
		return 0;
	}
    	rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_METHOD, 
			&data, &datalen, &attrs, &hdrcnt);
    	if (rv) {
		DBG_LOG(MSG, MOD_OM, "No HTTP method fd=%d", OM_CON_FD(ocon));
		return 0;
    	}

	if (CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
		OUTBUF(head_method, 4);
		method = head_method;
		methodlen = 4;
	} else {
		if (!httpreqcon->http.nsconf->http_config->request_policies.http_head_to_get) {
			OUTBUF(data, datalen);
			method = data;
			methodlen = datalen;
		} else {
			OUTBUF("GET", 3);
			method = get_method;
			methodlen = 3;
		}
	}
    	OUTBUF(" ", 1);
    	if ((rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_SEEK_URI, 
				&data, &datalen, &attrs, &hdrcnt))) {
		// BUG 3909, 5298, 4218, 5990, 6150:
		// F-pro0xy, R-proxy: change absolute uri to relative uri
		// T-Proxy & Cluster L7 Proxy: keep as is
		if ( ((ocon->oscfg.nsc->http_config->origin.select_method ==
			OSS_HTTP_DEST_IP) ||
		      (ocon->oscfg.nsc->http_config->origin.select_method ==
		     	OSS_HTTP_PROXY_CONFIG))
				&& !(rv = get_known_header(&httpreqcon->http.hdr, 
						MIME_HDR_X_NKN_ABS_URL_HOST,
						&data, &datalen, &attrs, 
						&hdrcnt))) {
			OUTBUF("http://", 7);
			OUTBUF(data, datalen);
		}
		if (CHECK_HTTP_FLAG(&httpreqcon->http, HRF_302_REDIRECT)) {
			char *dir, *file, *r_dir, *r_file;
			int dirlen, filelen, r_dirlen, r_filelen, querylen = 0;
			rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_REDIRECT_URI,
				    &data, &datalen, &attrs, &hdrcnt);
			if (rv) {
				DBG_LOG(MSG, MOD_OM, "No redirect URI, fd = %d", OM_CON_FD(ocon));
				return 0;
			}
			rv = parse_uri_2(data, datalen, &r_dir, &r_dirlen, &r_file, &r_filelen);
			if (rv) {
				DBG_LOG(MSG, MOD_OM, "failed in parse_uri_2, fd = %d", OM_CON_FD(ocon));
				return 0;
			}
			// Check if redirect URI has query
			if (memchr(data, '?', datalen) != 0) {
				loc_has_query = 1;
			}

			// Get the original URI also
			rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_URI,
					&data, &datalen, &attrs, &hdrcnt);
			if (rv) {
				DBG_LOG(MSG, MOD_OM, "No NKN_URI found, fd = %d", OM_CON_FD(ocon));
				return 0;
			}

			rv = parse_uri_2(data, datalen, &dir, &dirlen, &file, &filelen);
			if (rv) {
				DBG_LOG(MSG, MOD_OM, "failed in parse_uri_2, fd = %d", OM_CON_FD(ocon));
				return 0;
			}
			// We have the original URI and the redirect URI now..
			// Check how to construct the new URI
			// First put the directory.. if any
			// If the redirected uri is a relative URI, then
			// append it to the original directory name
			if((querystr = memchr(data, '?', datalen)) != 0) {
				querylen = datalen - (int)(querystr-data);
			}
			if (r_dirlen > 0 && r_dir[0] != '/') {
				OUTBUF(dir, dirlen);
				OUTBUF("/", 1);
			}
			if ( r_dir ) {
				OUTBUF(r_dir, r_dirlen);
				OUTBUF("/", 1);
			}
			// Now check if we need to append the file also
			if (r_filelen > 0 ){
				OUTBUF(r_file, r_filelen);
				// Append query string
				// Only if redirect URI did not have a query
				// PR 764538
				if((loc_has_query == 0) && (querylen > 0)) {
				    OUTBUF(querystr, querylen);
				}
			} else {
				OUTBUF(file, filelen);
			}
			OUTBUF(" ", 1);
			goto skip_on_redirect_uri;

		} else {
		    rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		    if (rv) {
			DBG_LOG(MSG, MOD_OM, "No URI, fd=%d", OM_CON_FD(ocon));
			return 0;
		    }
		}
	}

	// URI
	uri = data;
	urilen = datalen;
    	OUTBUF(data, datalen);
    	OUTBUF(" ", 1);


skip_on_redirect_uri:
	if ((querystr = memchr(data, '?', datalen)) != 0) {
		//
		// Request requires query string, note it for now 
		// in the OM hdrs NV list and place it in the attributes 
		// if the request is successful.
		//
		rv = add_namevalue_header(&ocon->phttp->hdr, 
				http_known_headers[MIME_HDR_X_NKN_QUERY].name,
				http_known_headers[MIME_HDR_X_NKN_QUERY].namelen,
				querystr, datalen - (int)(querystr-data),
				user_attr);
    		if (rv) {
			DBG_LOG(MSG, MOD_OM, 
				"add_namevalue_header() failed, rv=%d fd=%d", 
				rv, OM_CON_FD(ocon));
			return 0;
    		}
	}

	// HTTP version
	if ((httpreqcon->http.flag & HRF_HTTP_11)
               /*
                * In case of reverse-proxy, send the highest http version
                * supported by MFC (for RFC-2616 compliance).
                * In case of T-proxy & Cluster L7 Proxy, send the same 
		* version of request from client 
                * to origin. (ie, 1.0 -> 1.0, 1.1 -> 1.1)
                */
            || ((!nkn_namespace_use_client_ip(ocon->oscfg.nsc)) &&
	        (ocon->oscfg.nsc->http_config->origin.select_method !=
		     	OSS_HTTP_PROXY_CONFIG))) {
    		OUTBUF("HTTP/1.1\r\n", 10);
	} else if (httpreqcon->http.flag & HRF_HTTP_10) {
    		OUTBUF("HTTP/1.0\r\n", 10);
	} else {
    		OUTBUF("\r\n", 2);
	}

	// Compute Range: header value

	if (CHECK_HTTP_FLAG(&httpreqcon->http, HRF_BYTE_RANGE) ||
	    CHECK_HTTP_FLAG(&httpreqcon->http, HRF_BYTE_SEEK)) {
	   	// Client BR/Seek request
		if ((CHECK_HTTP_FLAG(&httpreqcon->http, HRF_BYTE_RANGE) && 
			(!CHECK_HTTP_FLAG(&httpreqcon->http, HRF_BYTE_RANGE_START_STOP) && !httpreqcon->http.brstop)) || 
		    (CHECK_HTTP_FLAG(&httpreqcon->http, HRF_BYTE_SEEK) && 
			!httpreqcon->http.seekstop)) {
			// Range start to EOF
			if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
				!CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {

				content_len = ocon->httpreqcon->http.content_length;

				rbci_offset = ocon->oscfg.nsc->http_config->cache_index.checksum_offset;
				rbci_size = ocon->oscfg.nsc->http_config->cache_index.checksum_bytes;

				if (ocon->oscfg.nsc->http_config->cache_index.checksum_from_end) {
					if (rbci_offset > rbci_size) {
						rbci_offset_start = content_len - rbci_offset;
						rbci_offset_end = rbci_offset_start + rbci_size - 1;
					} else {
						rbci_offset_start = content_len - rbci_size;
						rbci_offset_end = content_len - 1;
					}
				} else {
					rbci_offset_start = rbci_offset;
					rbci_offset_end = rbci_offset_start + rbci_size - 1;
				}
				snprintf(range_value, sizeof(range_value),
					"bytes=%ld-%ld", rbci_offset_start,
					rbci_offset_end);
			} else if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
				range_value[0] = '\0';
				/* Dont send byte range */
			} else {
				snprintf(range_value, sizeof(range_value), 
					"bytes=%ld-", ocon->uol.offset);
			}
		} else {
			if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
				!CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
				content_len = ocon->httpreqcon->http.content_length;

				rbci_offset = ocon->oscfg.nsc->http_config->cache_index.checksum_offset;
				rbci_size = ocon->oscfg.nsc->http_config->cache_index.checksum_bytes;

				if (ocon->oscfg.nsc->http_config->cache_index.checksum_from_end) {
					if (rbci_offset > rbci_size) {
						rbci_offset_start = content_len - rbci_offset;
						rbci_offset_end = rbci_offset_start + rbci_size - 1;
					} else {
						rbci_offset_start = content_len - rbci_size;
						rbci_offset_end = content_len - 1;
					}
				} else {
					rbci_offset_start = rbci_offset;
					rbci_offset_end = rbci_offset_start + rbci_size - 1;
				}
				snprintf(range_value, sizeof(range_value),
					"bytes=%ld-%ld", rbci_offset_start,
					rbci_offset_end);
			} else if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
				range_value[0] = '\0';
				/* Dont send byte range */
                        } else {
                                snprintf(range_value, sizeof(range_value),
                                        "bytes=%ld-%ld",
                                        ocon->uol.offset,
                                        ocon->uol.offset + ocon->uol.length-1);
                        }
		}
	} else {
		/* If its a RBCI request do a byte range fetch */
		if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
			CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
			    !CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
			content_len = ocon->httpreqcon->http.content_length;

			rbci_offset = ocon->oscfg.nsc->http_config->cache_index.checksum_offset;
			rbci_size = ocon->oscfg.nsc->http_config->cache_index.checksum_bytes;

			if (ocon->oscfg.nsc->http_config->cache_index.checksum_from_end) {
				if (rbci_offset > rbci_size) {
					rbci_offset_start = content_len - rbci_offset;
					rbci_offset_end = rbci_offset_start + rbci_size - 1;
				} else {
					rbci_offset_start = content_len - rbci_size;
					rbci_offset_end = content_len - 1;
				}
			} else {
				rbci_offset_start = rbci_offset;
				rbci_offset_end = rbci_offset_start + rbci_size - 1;
			}
			snprintf(range_value, sizeof(range_value),
				"bytes=%ld-%ld", rbci_offset_start,
				rbci_offset_end);
		} else if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
			CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
			CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
			range_value[0] = '\0';
				/* Dont send byte range */
		} else {
			// Client non BR request
			if (!ocon->uol.offset) {
				range_value[0] = '\0';
			} else {
				snprintf(range_value, sizeof(range_value), 
					"bytes=%ld-%ld", 
					ocon->uol.offset, 
					ocon->uol.offset + ocon->uol.length-1);
				/* Byte Range Request from BM */
				if (IS_PSEUDO_FD(ocon->httpfd)) {
					SET_HTTP_FLAG(&httpreqcon->http, HRF_BYTE_RANGE_BM);
				}
			}
		}
	}
	if (is_known_header_present(&httpreqcon->http.hdr, MIME_HDR_X_NKN_PE_HOST_HDR)) {
		OM_SET_FLAG(ocon, OMF_PE_HOST_HDR);
	}

	// Add known headers
    	for (token = 0; token < MIME_HDR_MAX_DEFS; token++) {
		if (!http_end2end_header[token]) {
			continue;
		}
		switch(token) {
		case MIME_HDR_HOST:
			if (CHECK_HTTP_FLAG(&httpreqcon->http, HRF_302_REDIRECT)) {
				rv = get_known_header(&httpreqcon->http.hdr,
					MIME_HDR_X_LOCATION, &location, &locationlen,
					&attrs, &hdrcnt);
				if (!rv && strncmp(location, "http://", 7) == 0) {
					add_host_302 = 1;
					continue;
				}
			}
                        // If the Host header is configured on MFC, that will override 
                        // the Host header from client when a request is sent to origin.
			if (!ocon->oscfg.policies->host_hdr_value && 
                              (ocon->oscfg.policies->is_host_header_inheritable || 
					CHECK_CON_FLAG(httpreqcon, CONF_PE_HOST_HDR) ||
					OM_CHECK_FLAG(ocon, OMF_PE_HOST_HDR))) {
				break; // Pass through
			} else {
				continue; // Replace with our value
			}
		case MIME_HDR_RANGE: // Replace with our value
			if ((*range_value == 0) && OM_CHECK_FLAG(ocon, OMF_PE_RANGE_HDR)) {
				range_value_set = 1;
				break;
			}
		case MIME_HDR_IF_RANGE:
		case MIME_HDR_X_NKN_URI:
		case MIME_HDR_X_NKN_METHOD:
		case MIME_HDR_X_NKN_QUERY:
		case MIME_HDR_X_NKN_REMAPPED_URI:
		case MIME_HDR_X_NKN_DECODED_URI:
		case MIME_HDR_X_NKN_SEEK_URI:
                case MIME_HDR_X_NKN_CLUSTER_TPROXY:
			continue;

		case MIME_HDR_AUTHORIZATION:
			if (ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.active) continue;

		case MIME_HDR_DATE:
			if (ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.active) continue;

		case MIME_HDR_IF_MODIFIED_SINCE:
		case MIME_HDR_IF_NONE_MATCH:
			if (httpreqcon->http.nsconf->http_config->request_policies.http_head_to_get) continue;

			break;

                case MIME_HDR_X_NKN_CL7_PROXY:
                        if (ocon->oscfg.nsc->http_config->origin.select_method != 
                            OSS_HTTP_PROXY_CONFIG) {
                                /* tier 2 node, do not include the header */
                                continue;
                        }
                        break;

		case MIME_HDR_ACCEPT_ENCODING:
                        if ((int16_t)ocon->p_mm_task->encoding_type != -1) {
                                encoding_type = ocon->p_mm_task->encoding_type;
                        } else {
                                encoding_type = ocon->httpreqcon->http.cache_encoding_type;
                        }
                        if ((int16_t)encoding_type == -1) break;
                        switch(encoding_type) {
                        case HRF_ENCODING_ALL:
                                datalen = 1; //"*"
                                buf[0] = '*';
                                break;
                        case HRF_ENCODING_GZIP:
                                datalen = 4; //"gzip"
                                strcpy(buf, "gzip");
                                break;
                        case HRF_ENCODING_DEFLATE:
                                datalen = 8; //"deflate"
                                strcpy(buf, "deflate");
                                break;
                        case HRF_ENCODING_COMPRESS:
                                datalen = 8; //"compress"
                                strcpy(buf, "compress");
                                break;
                        case HRF_ENCODING_EXI:
                                datalen = 3; //"exi"
                                strcpy(buf, "exi");
                                break;
                        case HRF_ENCODING_IDENTITY:
                                datalen = 8; //"identity"
                                strcpy(buf, "identity");
                                break;
                        case HRF_ENCODING_PACK200_GZIP:
                                datalen = 12; //"pack200_gzip"
                                strcpy(buf, "pack200_gzip");
                                break;
                        case HRF_ENCODING_SDCH:
                                datalen = 4; //"sdch"
                                strcpy(buf, "sdch");
                                break;
                        case HRF_ENCODING_BZIP2:
                                datalen = 5; //"bzip2"
                                strcpy(buf, "bzip2");
                                break;
                        case HRF_ENCODING_PEERDIST:
                                datalen = 8; //"peerdist"
                                strcpy(buf, "peerdist");
                                break;
                        default:
                                continue;
                        }
                        OUTBUF(http_known_headers[token].name,
                                                http_known_headers[token].namelen);
                        OUTBUF(": ", 2);
                        OUTBUF(buf, datalen);
                        OUTBUF("\r\n", 2);
                        continue;
		default:
			break;
		}

		rv = get_known_header(&httpreqcon->http.hdr, token, &data, 
				&datalen, &attrs, &hdrcnt);
		if (!rv) {
	    		OUTBUF(http_known_headers[token].name, 
	    			http_known_headers[token].namelen);
	    		OUTBUF(": ", 2);
	    		OUTBUF(data, datalen);

		} else {
	    		continue;
		}
        	for (nth = 1; nth < hdrcnt; nth++) {
	    		rv = get_nth_known_header(&httpreqcon->http.hdr, 
				token, nth, &data, &datalen, &attrs);
	    		if (!rv) {
	    			OUTBUF(",", 1);
	    			OUTBUF(data, datalen);
	    		}
		}
		OUTBUF("\r\n", 2);
    	}

        if (!CHECK_CON_FLAG(httpreqcon, CONF_REFETCH_REQ) &&
            nkn_http_is_tproxy_cluster(ocon->oscfg.nsc->http_config)) {

                if (http_create_header_cluster(httpreqcon, 
                        MIME_HDR_X_NKN_CLUSTER_TPROXY, header_cluster, 
                        &header_cluster_len) == FALSE) {
                        return 0;
                }

                OUTBUF(http_known_headers[MIME_HDR_X_NKN_CLUSTER_TPROXY].name,
                       http_known_headers[MIME_HDR_X_NKN_CLUSTER_TPROXY].namelen);
                OUTBUF(": ", 2);
                OUTBUF(header_cluster, header_cluster_len);
                OUTBUF("\r\n", 2);
        }

	// Add Range: header
	if (*range_value) {
		OUTBUF(http_known_headers[MIME_HDR_RANGE].name, 
	    			http_known_headers[MIME_HDR_RANGE].namelen);
		OUTBUF(": ", 2);
		OUTBUF(range_value, (int)strlen(range_value));
		OUTBUF("\r\n", 2);
                range_value_set = 1;
	}

	if (!(ocon->oscfg.policies->is_host_header_inheritable ||
                        // If the Host header is configured in RProxy case, go to next else block
                        // and override Host header with the configured value.
                        (ocon->oscfg.policies->host_hdr_value &&
                               (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_DEST_IP) &&
                               (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_FOLLOW_HEADER)
                        )  ||
			CHECK_CON_FLAG(httpreqcon, CONF_PE_HOST_HDR) ||
			OM_CHECK_FLAG(ocon, OMF_PE_HOST_HDR)) ||
			add_host_302) {
		// Add Host: header
		OUTBUF(http_known_headers[MIME_HDR_HOST].name, 
		    http_known_headers[MIME_HDR_HOST].namelen);
		OUTBUF(": ", 2);

		OUTBUF(ocon->oscfg.ip, (int)strlen(ocon->oscfg.ip));
		if (ntohs(ocon->oscfg.port) != 80) {
			char port[16];
			snprintf(port, sizeof(port), ":%d", 
				 ntohs(ocon->oscfg.port));
			OUTBUF(port, (int)strlen(port));
		}
		OUTBUF("\r\n", 2);
        } else if (ocon->oscfg.policies->host_hdr_value && // Send configured Host header to origin.
                (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_DEST_IP) &&
                (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_FOLLOW_HEADER)) {
                int host_hdr_len = 0 ;

                OUTBUF(http_known_headers[MIME_HDR_HOST].name,
                       http_known_headers[MIME_HDR_HOST].namelen);
                OUTBUF(": ", 2);

                host_hdr_len = strlen(ocon->oscfg.policies->host_hdr_value) ;
                OUTBUF(ocon->oscfg.policies->host_hdr_value, host_hdr_len);
                OUTBUF("\r\n", 2);
                DBG_LOG(MSG, MOD_OM, "GET path: Overriding Host header with user configured value: '%s'",
                                     ocon->oscfg.policies->host_hdr_value) ;
        }


	// Add Connection: header
	OUTBUF(http_known_headers[MIME_HDR_CONNECTION].name, 
		    http_known_headers[MIME_HDR_CONNECTION].namelen);
	OUTBUF(": ", 2);
	if(cp_enable) {
		OUTBUF("Keep-Alive\r\n", 12);
	} else {
		OUTBUF("Close\r\n", 7);
	}

	// Add unknown headers
    	nth = 0;
    	while ((rv = get_nth_unknown_header(&httpreqcon->http.hdr, nth, &name, 
			&namelen, &data, &datalen, &attrs)) == 0) {
		nth++;
		if (ocon->oscfg.nsc->http_config->origin.u.http.send_x_forward_header) {
			if ( strncasecmp(name, "X-Forwarded-For", namelen) == 0 ) {
				x_forward_hdr_no = nth - 1;
				continue;
			}
		}
		OUTBUF(name, namelen);
		OUTBUF(": ", 2);
		OUTBUF(data, datalen);
		OUTBUF("\r\n", 2);
	}

	/* Add configured headers
	 * 
	 * For internal genertated requests src_ip is not known.
	 * So do not send X-Forwarded-For header with 0.0.0.0 IP.
	 * BZ 3706
	 *
	 * If X-Forwarded-For: is present in request, append IP
	 * BZ 3691
	 */
	if (ocon->oscfg.nsc->http_config->origin.u.http.send_x_forward_header) {  
		char *src_ip_str = alloca(INET6_ADDRSTRLEN+1);
		int is_ipv6 = httpreqcon->ip_family == AF_INET?0:1;
		datalen = 0;
		const char *retptr = NULL;
		if (x_forward_hdr_no >= 0) {
			get_nth_unknown_header(&httpreqcon->http.hdr, x_forward_hdr_no, 
				&name, &namelen, &data, &datalen, &attrs);
		}
		src_ip_str = get_ip_str(&(httpreqcon->src_ipv4v6));
		/*
                if (is_ipv6 && IPv6(httpreqcon->src_ipv4v6)) {
                        retptr = inet_ntop(httpreqcon->ip_family, &IPv6(httpreqcon->src_ipv4v6), src_ip_str,
                                INET6_ADDRSTRLEN+1);
                } else if (IPv4(httpreqcon->src_ipv4v6)) {
                        retptr = inet_ntop(httpreqcon->ip_family, &IPv4(httpreqcon->src_ipv4v6), src_ip_str,
                                INET_ADDRSTRLEN+1);
                }
		*/
                if (is_ipv6 && IPv6(httpreqcon->src_ipv4v6)) {
                        retptr = inet_ntop(httpreqcon->ip_family, &IPv6(httpreqcon->src_ipv4v6), src_ip_str,
                                INET6_ADDRSTRLEN+1);
                } else if (IPv4(httpreqcon->src_ipv4v6)) {
                        retptr = inet_ntop(httpreqcon->ip_family, &IPv4(httpreqcon->src_ipv4v6), src_ip_str,
                                INET_ADDRSTRLEN+1);
                }

                if(retptr) {
		    OUTBUF("X-Forwarded-For: ", 17);
		    if (datalen) {
			OUTBUF(data, datalen);
			OUTBUF(", ", 2);
		    }
		    OUTBUF(src_ip_str, (int)strlen(src_ip_str));
		    OUTBUF("\r\n", 2);
		}
	}
	for (nth = 0; 
	     nth < ocon->oscfg.nsc->http_config->origin.u.http.num_headers; 
	     nth++) {
		hd = &ocon->oscfg.nsc->http_config->origin.u.http.header[nth];
		if ((hd->action == ACT_ADD) && hd->name) {
                    if (range_value_set
                        && (strncmp(hd->name, "Range", hd->name_strlen) == 0)) {
                         /* Add Range header only if not present.*/
                    } else {
		    OUTBUF(hd->name, hd->name_strlen);
		    OUTBUF(": ", 2);
		    if (hd->value) {
		    	OUTBUF(hd->value, hd->value_strlen);
		    }
		    OUTBUF("\r\n", 2);
                    }
		}
	}

	if (ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.active) {
		int date_len = 0;
		int len = 0;
		char *bucket_name;
		char *encoded_sign;
		char *p;
		char time_str[1024];
		char signature[4096] = { 0 };
		char final_uri[4096] = { 0 };
		unsigned char hmac[1024] = { 0 };
		char content_md5[4] = "";
		char content_type[4] = "";
		char cananionicalized_amz_hdrs[4] = "";
		const char *aws_access_key;
		int aws_access_key_len;
		const char *aws_secret_key;
		int aws_secret_key_len;
		unsigned int hmac_len = 0;
		struct tm *local;
		time_t t;

		aws_access_key = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.access_key;
		aws_access_key_len = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.access_key_len;
		aws_secret_key = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.secret_key;
		aws_secret_key_len = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.secret_key_len;

		t = time(NULL);
		local = gmtime(&t);
		date_len = strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S GMT", local);

		strcat(final_uri, "/");
		bucket_name = strtok(ocon->oscfg.ip, ".");
		if (bucket_name) strcat(final_uri, bucket_name);
		if (uri[1] == '?' && ((strncmp(uri+2, "prefix", 6) == 0) || (strncmp(uri+2, "delimiter", 9) == 0))) {
			strncat(final_uri, "/", 1);
		} else {
			strncat(final_uri, uri, urilen);
		}

		strncat(signature, method, methodlen);
		strcat(signature, "\n");
		strcat(signature, content_md5);
		strcat(signature, "\n");
		strcat(signature, content_type);
		strcat(signature, "\n");
		strncat(signature, time_str, date_len);
		strcat(signature, "\n");
		strcat(signature, cananionicalized_amz_hdrs);
		strcat(signature, final_uri);

		p = (char *)HMAC(EVP_sha1(), aws_secret_key, aws_secret_key_len,
				(const unsigned char *)signature, strlen(signature), hmac, &hmac_len);
		if (hmac_len == 0) goto end;
		encoded_sign = g_base64_encode(hmac, hmac_len);
		if (encoded_sign == NULL) goto end;

		OUTBUF("Date", 4);
		OUTBUF(": ",2);
		OUTBUF(time_str, date_len);
		OUTBUF("\r\n", 2);

		strcpy(signature, "AWS ");
		strncat(signature, aws_access_key, aws_access_key_len);
		strcat(signature, ":");
		strcat(signature, encoded_sign);
		g_free(encoded_sign);

		len = strlen(signature);
		OUTBUF("Authorization", 13);
		OUTBUF(": ", 2);
		OUTBUF(signature, len);
		OUTBUF("\r\n", 2);
		glob_om_aws_auth_cnt++;
	}

end:
	OUTBUF("\r\n", 2);
	OUTBUF("", 1);
	return outbuf;
}

static char *om_build_http_validate_request(omcon_t *ocon, char **outbuf_p, 
					int *outbufsz_p)
{
	int rv;
	int bytesused = 0;
	mime_header_t hdrs;
	const char *data;
	int datalen;
    	const char *lastmodstr;
    	int lastmodstr_len;
	const char *etagstr;
	int etagstr_len;
    	u_int32_t attrs;
    	int hdrcnt;
	const char *p_uri;
	int urilen;
	const char *hoststr;
	int hoststr_len;
	const char *cookiestr;
	int cookie_len;
	int nth;
	con_t *httpreqcon = ocon->httpreqcon;
	con_t *org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);
	header_descriptor_t *hd;
        int range_header_set = 0;
        const char *rangestr = NULL;
        int rangestr_len = 0;
	int rv_etag;
        char *outbuf = *outbuf_p;
	const char *location;
	int locationlen;
	int add_host_302 =0 ;
	nkn_client_data_t * client_data = NULL;
        char header_cluster[MAX_HTTP_HEADER_SIZE + 1];
        int header_cluster_len;
	const char *head_method = "HEAD";
	const char *get_method = "GET";
	const char *method;
	int methodlen;
    	const char *name;
    	int namelen;
    	int token;
	const namespace_config_t *ns_cfg = ocon->oscfg.nsc;
	int use_client_headers;

	init_http_header(&hdrs, 0, 0);
	rv = nkn_attr2http_header(ocon->p_validate->in_attr, 0, &hdrs);
	if (rv) {
		DBG_LOG(MSG, MOD_OM, 
			"nkn_attr2http_header() failed, rv=%d", rv);
		goto err_exit;
	}

	etagstr = 0;
	lastmodstr = 0;

    	rv_etag = get_known_header(&hdrs, MIME_HDR_ETAG, &etagstr, &etagstr_len, 
			      &attrs, &hdrcnt);

    	if ((rv = get_known_header(&hdrs, MIME_HDR_LAST_MODIFIED, 
				&lastmodstr, &lastmodstr_len, 
				&attrs, &hdrcnt))) {
		if (!((ns_cfg && ns_cfg->http_config 
		    && (NKN_TRUE == ns_cfg->http_config->policies.validate_with_date_hdr))
		    && 0 == (rv = get_known_header(&hdrs, MIME_HDR_DATE,
		    &lastmodstr, &lastmodstr_len, &attrs, &hdrcnt)))) {
			/* If ETag header is avaiable, use it to validate
			 * BZ 3716
			 */
			if (!(0 == rv_etag)) {
				DBG_LOG(MSG, MOD_OM, 
					"get_known_header(MIME_HDR_LAST_MODIFIED) failed, rv=%d",
					rv);
				goto err_exit;
			}
		}
	}

       /* BZ 7325: When "delivery protocol http revalidate-get" is enabled, 
        * use GET+IMS request 
        */
        if (ocon->oscfg.nsc->http_config->policies.cache_revalidate.method == METHOD_HEAD) {
	    OUTBUF("HEAD ", 5);
	    method = head_method;
	    methodlen = 4;
        } else { 
	    OUTBUF("GET ", 4);
	    method = get_method;
	    methodlen = 3;
        }

    	if ((rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_SEEK_URI,
				&p_uri, &urilen, &attrs, &hdrcnt))) {
		/* BZ 3909: for tproxy case, we should give preference to
		 * X_NKN_ABS_URL_HOST over X_NKN_URI, if the former exists
		 */
		if ( (nkn_namespace_use_client_ip(ocon->oscfg.nsc))
				&& !(rv = get_known_header(&httpreqcon->http.hdr,
						MIME_HDR_X_NKN_ABS_URL_HOST,
						&p_uri, &urilen, &attrs,
						&hdrcnt))) {
			OUTBUF("http://", 7);
			OUTBUF(p_uri, urilen);
		}
    		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_URI,
				&p_uri, &urilen, &attrs, &hdrcnt);
    		if (rv) {
			DBG_LOG(MSG, MOD_OM, "No URI, fd=%d", OM_CON_FD(ocon));
			return 0;
    		}
	}

	OUTBUF(p_uri, urilen);

	OUTBUF(" HTTP/1.1\r\n", 11);

	// Add Host: header
	OUTBUF(http_known_headers[MIME_HDR_HOST].name, 
		    http_known_headers[MIME_HDR_HOST].namelen);
	OUTBUF(": ", 2);

	if (CHECK_HTTP_FLAG(&httpreqcon->http, HRF_302_REDIRECT)) {
		// Get the LOCATION header
		rv = get_known_header(&httpreqcon->http.hdr,
				MIME_HDR_X_LOCATION, &location, &locationlen,
				&attrs, &hdrcnt);
		if (!rv && strncmp(location, "http://", 7) == 0) {
			add_host_302 = 1;
		}
	}

	if (is_known_header_present(&hdrs, MIME_HDR_X_NKN_PE_HOST_HDR)) {
		OM_SET_FLAG(ocon, OMF_PE_HOST_HDR);
	}

	if (!(ocon->oscfg.policies->is_host_header_inheritable ||
                        (ocon->oscfg.policies->host_hdr_value &&
                               (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_DEST_IP) &&
                               (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_FOLLOW_HEADER)
                        )  ||
			CHECK_CON_FLAG(httpreqcon, CONF_PE_HOST_HDR) ||
			OM_CHECK_FLAG(ocon, OMF_PE_HOST_HDR)) ||
			add_host_302) {
		OUTBUF(ocon->oscfg.ip, (int)strlen(ocon->oscfg.ip));
		if (ntohs(ocon->oscfg.port) != 80) {
			char port[16];
			snprintf(port, sizeof(port), ":%d", 
				 ntohs(ocon->oscfg.port));
			OUTBUF(port, (int)strlen(port));
		}
	} else if (ocon->oscfg.policies->host_hdr_value &&
                (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_DEST_IP) &&
                (ocon->oscfg.nsc->http_config->origin.select_method != OSS_HTTP_FOLLOW_HEADER)) {
                int host_hdr_len = 0 ;

                host_hdr_len = strlen(ocon->oscfg.policies->host_hdr_value) ;
                OUTBUF(ocon->oscfg.policies->host_hdr_value, host_hdr_len);
                DBG_LOG(MSG, MOD_OM, "Validate path: Overriding Host header with user configured value: '%s'",
                                     ocon->oscfg.policies->host_hdr_value) ;
        } else {
		/* Check first if PE has set a Host header */
		rv = get_known_header(&hdrs, MIME_HDR_X_NKN_PE_HOST_HDR,
			&hoststr, &hoststr_len, &attrs, &hdrcnt);
		if (rv) {
			//bug 8143
			if (org_httpreqcon) {
				rv = get_known_header(&org_httpreqcon->http.hdr, MIME_HDR_HOST, &hoststr, &hoststr_len,
					&attrs, &hdrcnt);
			}
			else {
				rv = get_known_header(&hdrs, MIME_HDR_X_NKN_CLIENT_HOST,
					&hoststr, &hoststr_len, &attrs, &hdrcnt);
			}
		}
		if(!rv && (hoststr[0]=='.')) {
		/*
		 * BUG 4455: special case for T-Proxy
		 * This is not good code. needs to be optimized.
		 * Internal format: .real_IP:port:domain.
		 */
		char * p;
		int len;
		int num_hit;

		num_hit = 0;
		len = 0;
                p = (char *)hoststr;
                while(len < hoststr_len) {
			if (*p == ':') {
				if (num_hit == 1) {
					p++; len++;
                			hoststr = (const char *)p;
			                hoststr_len -= len;
					break;
                                }
                                num_hit ++;
                        }
                        p++;
                        len ++;
		}
		}

		if (hoststr_len) {
			OUTBUF(hoststr, hoststr_len);
		}
	}
	OUTBUF("\r\n", 2);

	// Add If-Modified-Since: header
	if (lastmodstr) {
		OUTBUF(http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].name, 
			    http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].namelen);
		OUTBUF(": ", 2);
		OUTBUF(lastmodstr, lastmodstr_len);
		OUTBUF("\r\n", 2);
	}
	
	// Add If-None-Match: header
	if (etagstr) {
		OUTBUF(http_known_headers[MIME_HDR_IF_NONE_MATCH].name, 
		       http_known_headers[MIME_HDR_IF_NONE_MATCH].namelen);
		OUTBUF(": ", 2);
		OUTBUF(etagstr, etagstr_len);
		OUTBUF("\r\n", 2);
	}

	// Add "Cache-control: max-age=0" header
	if(org_httpreqcon) {
		const cooked_data_types_t *ckdp;
		int ckd_len;
		mime_hdr_datatype_t dtype;

		// BZ 5569: force a upstream cache flush if
		// clients havent requested so.
		// Check if client request already adds a max-age=0
		rv = get_cooked_known_header_data(&org_httpreqcon->http.hdr, MIME_HDR_CACHE_CONTROL,
				&ckdp, &ckd_len, &dtype);
		if (!rv && (dtype == DT_CACHECONTROL_ENUM) &&
		    (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE)) {
			/* Do nothing.. client request already has a
			 * Cache-Control Header which we will use as is.
			 */

		} else {
			// Client doesnt give a Cache-Control header.
			// So, add one to force revalidation if the
			// namespace says so.
			if (ocon->oscfg.policies->flush_ic_cache == NKN_TRUE) {
				OUTBUF(http_known_headers[MIME_HDR_CACHE_CONTROL].name,
				       http_known_headers[MIME_HDR_CACHE_CONTROL].namelen);
				OUTBUF(": ", 2);
				OUTBUF("max-age=0\r\n", 11);
			}
		}

		// Add the client's Cache-Control directives
    		rv = get_known_header(&org_httpreqcon->http.hdr, MIME_HDR_CACHE_CONTROL,
				&hoststr, &hoststr_len, &attrs, &hdrcnt);
		if(!rv) {
			OUTBUF(http_known_headers[MIME_HDR_CACHE_CONTROL].name,
			       http_known_headers[MIME_HDR_CACHE_CONTROL].namelen);
			OUTBUF(": ", 2);
			OUTBUF(hoststr, hoststr_len);
			OUTBUF("\r\n", 2);
		}
	}

	/* Insert Cookie header from client in revalidation request to origin */
	if (org_httpreqcon) {
		if (is_known_header_present(&org_httpreqcon->http.hdr, MIME_HDR_COOKIE)) {
			rv = get_known_header(&org_httpreqcon->http.hdr, MIME_HDR_COOKIE,
					&cookiestr, &cookie_len, &attrs, &hdrcnt);
			if(!rv) {
				OUTBUF(http_known_headers[MIME_HDR_COOKIE].name,
					http_known_headers[MIME_HDR_COOKIE].namelen);
				OUTBUF(": ", 2);
				OUTBUF(cookiestr, cookie_len);
				OUTBUF("\r\n", 2);
				for(nth = 1; nth < hdrcnt; nth++) {
					rv = get_nth_known_header(&org_httpreqcon->http.hdr,
							MIME_HDR_COOKIE, nth,
							&cookiestr, &cookie_len, &attrs);
					if(rv) {
						DBG_LOG(MSG, MOD_HTTP,
						"get_nth_known_header(n=%d) failed, " "nth=%d rv=%d",
						MIME_HDR_COOKIE, nth, rv);
						continue;
					}

					OUTBUF(http_known_headers[MIME_HDR_COOKIE].name,
						http_known_headers[MIME_HDR_COOKIE].namelen);
					OUTBUF(": ", 2);
					OUTBUF(cookiestr, cookie_len);
					OUTBUF("\r\n", 2);
				}
			}
		}

		if (is_known_header_present(&org_httpreqcon->http.hdr, MIME_HDR_COOKIE2)) {
			rv = get_known_header(&org_httpreqcon->http.hdr, MIME_HDR_COOKIE2,
					&cookiestr, &cookie_len, &attrs, &hdrcnt);
			if(!rv) {
				OUTBUF(http_known_headers[MIME_HDR_COOKIE2].name,
					http_known_headers[MIME_HDR_COOKIE2].namelen);
				OUTBUF(": ", 2);
				OUTBUF(cookiestr, cookie_len);
				OUTBUF("\r\n", 2);
				for(nth = 1; nth < hdrcnt; nth++) {
					rv = get_nth_known_header(&org_httpreqcon->http.hdr,
							MIME_HDR_COOKIE2, nth,
							&cookiestr, &cookie_len, &attrs);
					if(rv) {
						DBG_LOG(MSG, MOD_HTTP,
						"get_nth_known_header(n=%d) failed, " "nth=%d rv=%d",
						MIME_HDR_COOKIE2, nth, rv);
						continue;
					}

					OUTBUF(http_known_headers[MIME_HDR_COOKIE2].name,
						http_known_headers[MIME_HDR_COOKIE2].namelen);
					OUTBUF(": ", 2);
					OUTBUF(cookiestr, cookie_len);
					OUTBUF("\r\n", 2);
				}
			}
		}
	}

	/* Add configured headers
	 * 
	 * For internal genertated requests src_ip is not known.
	 * So do not send X-Forwarded-For header with 0.0.0.0 IP.
	 * BZ 3706
	 */
	if (ocon->oscfg.nsc->http_config->origin.u.http.send_x_forward_header) {
		char *src_ip_str = alloca(INET6_ADDRSTRLEN+1);
                int src_ip_str_len = 0;
		int ret = 0;
		client_data = (nkn_client_data_t *)(&ocon->p_validate->in_client_data);
		if (httpreqcon && client_data) {
		    if ((client_data->client_ip_family == AF_INET6) && IPv6(httpreqcon->src_ipv4v6)) {
			if(inet_ntop(client_data->client_ip_family, &IPv6(httpreqcon->src_ipv4v6), src_ip_str,
				INET6_ADDRSTRLEN+1)) {
				ret = 1;
			}
		    } else if(IPv4(httpreqcon->src_ipv4v6)) {
			if( inet_ntop(client_data->client_ip_family, &IPv4(httpreqcon->src_ipv4v6), src_ip_str,
				INET_ADDRSTRLEN+1)) {
				ret = 1;
			}
		    }
		    if (ret) {
          	    	src_ip_str_len = (int)strlen(src_ip_str) ;
		    	OUTBUF("X-Forwarded-For: ", 17);
		    	OUTBUF(src_ip_str, src_ip_str_len) ;
		    	OUTBUF("\r\n", 2);
		    }
		}
	}

        if (nkn_http_is_tproxy_cluster(ocon->oscfg.nsc->http_config)) {
                if (http_create_header_cluster(org_httpreqcon,
                        MIME_HDR_X_NKN_CLUSTER_TPROXY,
                        header_cluster, &header_cluster_len) == FALSE) {
                        return 0;
                }

                OUTBUF(http_known_headers[MIME_HDR_X_NKN_CLUSTER_TPROXY].name,
                       http_known_headers[MIME_HDR_X_NKN_CLUSTER_TPROXY].namelen);
                OUTBUF(": ", 2);
                OUTBUF(header_cluster, header_cluster_len);
                OUTBUF("\r\n", 2);
        }

	if (!ns_cfg->http_config->policies.cache_revalidate.use_client_headers)
		goto skip_client_header_copy;

	// Copy other known headers
	for (token = 0; token < MIME_HDR_MAX_DEFS; token++) {
		if (!http_end2end_header[token]) {
			continue;
		}
		switch(token) {
		    case MIME_HDR_AUTHORIZATION:
		    case MIME_HDR_PROXY_AUTHENTICATE:
		    case MIME_HDR_PROXY_AUTHENTICATION_INFO:
		    case MIME_HDR_PROXY_AUTHORIZATION:
		    case MIME_HDR_PROXY_CONNECTION:
		    case MIME_HDR_REFERER:
		    case MIME_HDR_WWW_AUTHENTICATE:
		    case MIME_HDR_AUTHENTICATION_INFO:
			break;
		    default:
			continue;
		}

		rv = get_known_header(&org_httpreqcon->http.hdr, token, &data,
				&datalen, &attrs, &hdrcnt);
		if (!rv) {
			OUTBUF(http_known_headers[token].name,
				http_known_headers[token].namelen);
			OUTBUF(": ", 2);
			OUTBUF(data, datalen);

		} else {
			continue;
		}
		for (nth = 1; nth < hdrcnt; nth++) {
			rv = get_nth_known_header(&org_httpreqcon->http.hdr,
				token, nth, &data, &datalen, &attrs);
			if (!rv) {
				OUTBUF(",", 1);
				OUTBUF(data, datalen);
			}
		}
		OUTBUF("\r\n", 2);
	}

	// Copy unknown headers
	nth = 0;
	while ((rv = get_nth_unknown_header(&org_httpreqcon->http.hdr, nth, &name,
			&namelen, &data, &datalen, &attrs)) == 0) {
		nth++;
		if ( strncasecmp(name, "X-Forwarded-For", namelen) == 0 ) {
			continue;
		}
		OUTBUF(name, namelen);
		OUTBUF(": ", 2);
		OUTBUF(data, datalen);
		OUTBUF("\r\n", 2);
	}

skip_client_header_copy:;

	for (nth = 0; 
	     nth < ocon->oscfg.nsc->http_config->origin.u.http.num_headers; 
	     nth++) {
		hd = &ocon->oscfg.nsc->http_config->origin.u.http.header[nth];
		if ((hd->action == ACT_ADD) && hd->name) {
                   /*
                    * If Range header configured and Range header already present,
                    * do not append the configured one.
                    */
                    if (strncmp(hd->name, "Range", hd->name_strlen) == 0) {
                        rv = get_known_header(&hdrs, MIME_HDR_RANGE,
                                              &rangestr, &rangestr_len, &attrs, &hdrcnt);
                        if(!rv) {
                             range_header_set = 1;
                        }
                    }

                    if (!range_header_set) {
		    OUTBUF(hd->name, hd->name_strlen);
		    OUTBUF(": ", 2);
		    if (hd->value) {
		    	OUTBUF(hd->value, hd->value_strlen);
		    }
		    OUTBUF("\r\n", 2);
                        range_header_set = 0;
                    }
		}
	}

	if (ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.active) {
		int date_len = 0;
		int len = 0;
		char *bucket_name;
		char *encoded_sign;
		char *p;
		char time_str[1024];
		char signature[4096] = { 0 };
		char final_uri[4096] = { 0 };
		unsigned char hmac[1024] = { 0 };
		char content_md5[4] = "";
		char content_type[4] = "";
		char cananionicalized_amz_hdrs[4] = "";
		const char *aws_access_key;
		int aws_access_key_len;
		const char *aws_secret_key;
		int aws_secret_key_len;
		unsigned int hmac_len = 0;
		struct tm *local;
		time_t t;

		aws_access_key = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.access_key;
		aws_access_key_len = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.access_key_len;
		aws_secret_key = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.secret_key;
		aws_secret_key_len = ocon->oscfg.nsc->http_config->origin.u.http.server[0]->aws.secret_key_len;

		t = time(NULL);
		local = gmtime(&t);
		date_len = strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S GMT", local);

		strcat(final_uri, "/");
		bucket_name = strtok(ocon->oscfg.ip, ".");
		if (bucket_name) strcat(final_uri, bucket_name);
		if (p_uri[1] == '?' && ((strncmp(p_uri+2, "prefix", 6) == 0) || (strncmp(p_uri+2, "delimiter", 9) == 0))) {
			strncat(final_uri, "/", 1);
		} else {
			strncat(final_uri, p_uri, urilen);
		}

		strncat(signature, method, methodlen);
		strcat(signature, "\n");
		strcat(signature, content_md5);
		strcat(signature, "\n");
		strcat(signature, content_type);
		strcat(signature, "\n");
		strncat(signature, time_str, date_len);
		strcat(signature, "\n");
		strcat(signature, cananionicalized_amz_hdrs);
		strcat(signature, final_uri);

		p = (char *)HMAC(EVP_sha1(), aws_secret_key, aws_secret_key_len,
				(const unsigned char *)signature, strlen(signature), hmac, &hmac_len);
		if (hmac_len == 0) goto end;
		encoded_sign = g_base64_encode(hmac, hmac_len);
		if (encoded_sign == NULL) goto end;

		strcpy(signature, "AWS ");
		strncat(signature, aws_access_key, aws_access_key_len);
		strcat(signature, ":");
		strcat(signature, encoded_sign);
		g_free(encoded_sign);

		OUTBUF("Date", 4);
		OUTBUF(": ",2);
		OUTBUF(time_str, date_len);
		OUTBUF("\r\n", 2);

		len = strlen(signature);
		OUTBUF("Authorization", 13);
		OUTBUF(": ", 2);
		OUTBUF(signature, len);
		OUTBUF("\r\n", 2);
		glob_om_validate_aws_auth_cnt++;
	}

end:
	// End of request
	OUTBUF("\r\n", 2);
	OUTBUF("", 1);

	shutdown_http_header(&hdrs);
	return outbuf;

err_exit:

	shutdown_http_header(&hdrs);
	return 0;
}
#undef OUTBUF

/* =================================================== */
/* 
 * return >0: means so much data has been sent out
 * return -1: means EWOULDBLOCK
 * return -2: unexpected error, socket should close.
 */
static int om_send(omcon_t * ocon);
static int om_send(omcon_t * ocon)
{
        int sockfd=OM_CON_FD(ocon);
        int ret;
	char * p;
	int len;
	u_int32_t attrs;
	int hdrcnt;
	const char *host_hdr = NULL;
	int host_hdr_len = 0;
	int rv;
	cpcon_t *cpcon = ocon->cpcon;

	con_t *httpreqcon = ocon->httpreqcon;
	if(cpcon && CHECK_CPCON_FLAG(cpcon, CPF_HTTPS_CONNECT) && !CHECK_CPCON_FLAG(cpcon, CPF_HTTPS_HDR_SENT)) {
		ssl_con_hdr_t hdr ;
		memset(&hdr, 0, sizeof(hdr));
		hdr.magic = HTTPS_REQ_IDENTIFIER;
		cpcon->ssl_fid = (AO_fetch_and_add1(&glob_ssl_fid_index) % MAX_SHM_SSL_ID_SIZE);
		hdr.ssl_fid = cpcon->ssl_fid;
		if(ocon->oscfg.policies->host_hdr_value) {

			/* Use the Host Header value set from CLI */
			host_hdr_len = strlen(ocon->oscfg.policies->host_hdr_value);
			host_hdr = ocon->oscfg.policies->host_hdr_value;

		} else if (ocon->oscfg.policies->is_host_header_inheritable || 
				CHECK_CON_FLAG(httpreqcon, CONF_PE_HOST_HDR) ||  
				OM_CHECK_FLAG(ocon, OMF_PE_HOST_HDR)) {

			/* In revalidation path , inherit the host header from PE if available or
			   inherit from client request 
			*/
			if (OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
				con_t *org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);
				mime_header_t hdrs;
				init_http_header(&hdrs, 0, 0);
				
				rv = nkn_attr2http_header(ocon->p_validate->in_attr, 0, &hdrs);
				if (!rv) {

		  		  /* Check first if PE has set a Host header */
				  rv = get_known_header(&hdrs, MIME_HDR_X_NKN_PE_HOST_HDR,
				  	  &host_hdr, &host_hdr_len, &attrs, &hdrcnt);
				  if (rv) {
				    //bug 8143
		  	  	    if (org_httpreqcon) {
				 	rv = get_known_header(&org_httpreqcon->http.hdr, MIME_HDR_HOST, 
						&host_hdr, &host_hdr_len, &attrs, &hdrcnt);
				    }  
  				    else {
					rv = get_known_header(&hdrs, MIME_HDR_X_NKN_CLIENT_HOST,
						&host_hdr, &host_hdr_len, &attrs, &hdrcnt);
				    }
				  }
				}

                	        shutdown_http_header(&hdrs);
			} else {

				/* regular path , inherit host header from client request */
				rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_HOST, 
				    	  &host_hdr, &host_hdr_len, &attrs, &hdrcnt);
			}
		}

		
		if(host_hdr_len > 0 && host_hdr) {
			memcpy(hdr.virtual_domain, host_hdr, host_hdr_len);
		} else if (ocon->oscfg.ip) {
			memcpy(hdr.virtual_domain, ocon->oscfg.ip, strlen(ocon->oscfg.ip));
		}
                if (httpreqcon->ip_family == AF_INET6 && 
			IPv6(httpreqcon->src_ipv4v6)) {
			hdr.src_ipv4v6.family = AF_INET6;
			memcpy(&(IPv6(hdr.src_ipv4v6)), &(IPv6(httpreqcon->src_ipv4v6)), sizeof(struct in6_addr));
		} else {
			IPv4(hdr.src_ipv4v6) = IPv4(httpreqcon->src_ipv4v6)  ;
			hdr.src_ipv4v6.family = AF_INET;
		}
		hdr.src_port = httpreqcon->src_port;

		if(ocon->phttp->remote_ipv4v6.family == AF_INET) {
			IPv4(hdr.dest_ipv4v6) = IPv4(ocon->phttp->remote_ipv4v6);
			hdr.dest_ipv4v6.family = AF_INET;
		} else {
			hdr.dest_ipv4v6.family = AF_INET6;
			memcpy(&(IPv6(hdr.dest_ipv4v6)), &(IPv6(ocon->phttp->remote_ipv4v6)), sizeof(struct in6_addr));
		}
		hdr.dest_port = htons(ocon->phttp->remote_port); 

        	ret = send(sockfd, (char *)&hdr, sizeof(hdr), 0);
        	if (ret == -1) {
                	if (errno == EAGAIN) {
				NM_add_event_epollout(sockfd);
                        	return -1;
			}
                	return -2;
        	}
		SET_CPCON_FLAG(cpcon, CPF_HTTPS_HDR_SENT);
	}
	p = &ocon->send_req_buf[ocon->send_req_buf_offset];
	len = ocon->send_req_buf_totlen - ocon->send_req_buf_offset;
	while(len) {
        	ret = send(sockfd, p, len, 0);
        	if (ret == -1) {
                	if (errno == EAGAIN) {
				NM_add_event_epollout(sockfd);
                        	return -1;
			}

                //
                // The most popular error are EPIPE and ECONNRESET
                // EPIPE: data sent when socket has been closed.
                // ECONNRESET: data sent wehn socket has been reset
                //
                // We have called signal(EPIPE, SIG_IGN) to ignore the EPIPE signal
                //

                //ERROR("%s: send failed", __FUNCTION__);
                	return -2;
        	}
		p += ret;
		ocon->send_req_buf_offset += ret;
		len -= ret;
		glob_tot_size_to_os_by_origin += ret;
		AO_fetch_and_add(&glob_origin_send_tot_bytes, ret);
		if (NULL != ocon->httpreqcon) {
			NS_UPDATE_STATS((ocon->httpreqcon->http.nsconf), PROTO_HTTP, server, out_bytes, ret);
		}
	}
	ocon->send_req_buf_offset = 0;
	ocon->send_req_buf_totlen = 0;

        return 0;
}

/*
 * Socket has been established, at this time we need to send out GET request
 */
static int cp_callback_send_httpreq(void * private_data)
{
	omcon_t * ocon = (omcon_t *)private_data;
	int len;
	int ret;

	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_SEND_HTTPREQ);
	if (ocon->cpcon) ocon->state = ((cpcon_t *) ocon->cpcon)->state;
	if(ocon->send_req_buf && 
		(ocon->send_req_buf_offset != ocon->send_req_buf_totlen)) {
		// Left over in last call. Most likely because of EAGAIN.
		if (NM_add_event_epollin(OM_CON_FD(ocon))==FALSE) { // to remove epollout
			return -1;
		}
		goto next;
	}

	len = MAX(TYPICAL_HTTP_CB_BUF, 
				(ocon->httpreqcon ? 
				 ocon->httpreqcon->http.cb_max_buf_size : 0));
	ocon->send_req_buf = (char *)nkn_malloc_type(len, mod_om_send_req_buf);
	if (!ocon->send_req_buf) {
		DBG_LOG(MSG, MOD_OM, "(fd=%d) malloc failed", OM_CON_FD(ocon));
		return -1;
	}

	/* This is a good place to call policy engine handler for OM request
	 */
	if (ocon->oscfg.nsc->http_config->policy_engine_config.policy_file) {
		pe_ilist_t * p_list = NULL;
		pe_rules_t * p_perule;

		p_perule = pe_create_rule(&ocon->oscfg.nsc->http_config->policy_engine_config, &p_list);
		if (p_perule) {
			uint64_t action;
			action = pe_eval_om_send_request(p_perule, ocon);
			pe_free_rule(p_perule, p_list);
		}
	}

	if (OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		assert(ocon->httpreq != NULL);
		if (!om_build_http_request(ocon, &ocon->send_req_buf, &len)) {
			DBG_LOG(MSG, MOD_OM, 
				"om_build_http_request(fd=%d) failed", 
				OM_CON_FD(ocon));
                        if(ocon->p_mm_task) {
                           ocon->p_mm_task->err_code = NKN_OM_BUILD_REQ_ERR;
                        }

			return -1;
		}

	} else if (OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
		if (!om_build_http_validate_request(ocon, &ocon->send_req_buf, &len)) {
			DBG_LOG(MSG, MOD_OM, 
				"om_build_http_validate_request(fd=%d) failed", 
				OM_CON_FD(ocon));
			return -1;
		}
	} else {
		assert(!"Invalid request");
	}
	ocon->send_req_buf_offset = 0;
	ocon->send_req_buf_totlen = strlen(ocon->send_req_buf);

	if (ocon->httpreqcon) {
		NS_INCR_STATS((ocon->httpreqcon->http.nsconf), PROTO_HTTP, server, requests);
	}

next:
	ocon->request_send_time = nkn_cur_ts;
	ret = om_send(ocon);
	if( ret==-1 ) {
		DBG_LOG(MSG, MOD_OM, "om_send(fd=%d) EAGAIN, ret=%d", 
			OM_CON_FD(ocon), ret);
		return 1;
	} else if(ret==-2) {
		DBG_LOG(MSG, MOD_OM, "om_send(fd=%d) failed, ret=%d", 
			OM_CON_FD(ocon), ret);
		// close socket
		return -1;
	}
	DBG_LOG(MSG, MOD_OM, "om_send(fd=%d) succeeded [%s]",
		OM_CON_FD(ocon), ocon->send_req_buf);
	free(ocon->send_req_buf);
	ocon->send_req_buf = NULL;
	setup_read_timer(ocon);

	return 0;
}

static int process_validate_response(omcon_t *ocon)
{
	int rv;
	int token;
	mime_header_t hdrs;
	const char *inhand_data, *curr_data, *p, *data;
	int inhand_datalen, curr_datalen, header_enum, datalen;
    	u_int32_t attrs;
    	int hdrcnt;
	nkn_attr_t *pnew_attr;
	time_t corrected_initial_age;
	const char *cnp;
	nkn_objv_t obj_vers;
	const cooked_data_types_t *ckdp;
	int ckd_len;
	mime_hdr_datatype_t dtype;
	int set_cookie = 0, set_cookie2 = 0;
	int version_check = 1;
	int cache_object = 0;
	int http_encoding_type = 0;
	time_t expiry = 0;
	int cache_pin = 0;
	int cim_flags = 0;
	int max_age_tunnel = 0;
	int s_maxage_ignore = 0;
	int vary_hdr = 0;
	int vary_encoding = 0;
	int nth;
	const char *name;
	int namelen;

	init_http_header(&hdrs, 0, 0);

	/*
	 * Update out_proto_resp in validate struct with relevent data.
	 */
	ocon->p_validate->out_proto_resp.u.OM.respcode = ocon->phttp->respcode;
	memcpy(&ocon->p_validate->out_proto_resp.u.OM.remote_ipv4v6, 
			&ocon->phttp->remote_ipv4v6, sizeof(ip_addr_t));
	ocon->p_validate->out_proto_resp.u.OM.remote_port = 
		ocon->phttp->remote_port;

	/*
	 * Apply Policy Engine
	 */
	con_t *org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);

	if (ocon->pe_action & PE_ACTION_RES_NO_CACHE_OBJECT) {
		//glob_tunnel_res_pe++;
		// Change from FAIL to BYPASS for setaction no_cache
		ocon->p_validate->ret_code = MM_VALIDATE_BYPASS;
		DBG_LOG(MSG, MOD_OM, "Policy: no_cache_object, resp=%d, fd=%d", 
			ocon->phttp->respcode, OM_CON_FD(ocon));
		goto exit;
	}
	if ((ocon->pe_action & PE_ACTION_CACHE_OBJECT) ||
	    (org_httpreqcon && CHECK_ACTION(org_httpreqcon, PE_ACTION_REQ_CACHE_OBJECT))) {
		cache_object = 1;
	}

        if (ocon->phttp->respcode == 200) { // Modified
                ocon->p_validate->ret_code = MM_VALIDATE_MODIFIED;
                if (ocon->oscfg.nsc->http_config->policies.cache_revalidate.validate_header ||
		    ocon->oscfg.policies->object_correlation_validatorsall_ignore) {
                        // step1: convert inhand attribute to header for comparison
                        rv = nkn_attr2http_header(ocon->p_validate->in_attr, 0, &hdrs);
                        if (rv) {
                                shutdown_http_header(&hdrs);
                                return TRUE;
                        }
			//If we are told to ignore all validators, just move on
			if (ocon->oscfg.policies->object_correlation_validatorsall_ignore) {
				version_check = 0;//avoid additional check below
				glob_om_validators_all_ignore++;
				DBG_LOG(MSG, MOD_OM, "Policy set to ignore all validators. Proceeding without version checks");
				goto not_modified;
			}
                        rv = ocon->oscfg.nsc->http_config->policies.cache_revalidate.header_len;
                        p = ocon->oscfg.nsc->http_config->policies.cache_revalidate.header_name;
                        // step2: first validate the given name
                        rv = mime_hdr_strtohdr(MIME_PROT_HTTP, p, rv, &header_enum);
                        if (rv) {
                                shutdown_http_header(&hdrs);
                                return TRUE;
                        }
                        // step3: get the inhand data/datalen
                        rv = get_known_header(&hdrs, header_enum,
                                &inhand_data, &inhand_datalen, &attrs, &hdrcnt);
                        if  (rv) {
                                shutdown_http_header(&hdrs);
                                return TRUE;
                        }
                        // step4: get the current data/datalen
                        if (is_known_header_present(&ocon->phttp->hdr, header_enum)) {
                                rv = get_known_header(&ocon->phttp->hdr, header_enum,
                                                            &curr_data, &curr_datalen, &attrs, &hdrcnt);
                                if (rv) {
                                        shutdown_http_header(&hdrs);
                                        return TRUE;
                                }
                        }
                        // step5: compare the inhand and current data
                        if ((inhand_datalen == curr_datalen) &&
                                (strncmp(inhand_data, curr_data, curr_datalen)) == 0) {
                                ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
                                //No need to check versions again. We are sure its not modified. bug 8596
                                version_check = 0;
                                goto not_modified;
                        }
                }
                DBG_LOG(MSG, MOD_OM, "Object modified, fd=%d", OM_CON_FD(ocon));
                shutdown_http_header(&hdrs);
                return TRUE;

        }
	if (ocon->phttp->respcode == 304) { // Not modified
		ocon->p_validate->ret_code = MM_VALIDATE_ERROR;

		// Update attributes
		rv = nkn_attr2http_header(ocon->p_validate->in_attr, 0, &hdrs);
		if (rv) {
			DBG_LOG(MSG, MOD_OM, 
				"nkn_attr2http_header() failed, rv=%d, fd=%d", 
				rv, OM_CON_FD(ocon));
			goto exit;
		}

not_modified:
		http_encoding_type = ocon->p_validate->in_attr->encoding_type;

		if((ocon->p_validate->in_attr->na_flags & (NKN_OBJ_COMPRESSED))) {
		    http_encoding_type = ocon->phttp->http_encoding_type;
		}

    		for (token = 0; token < MIME_HDR_MAX_DEFS; token++) {
			if (!http_end2end_header[token] || 
			    !http_allowed_304_header[token]) {
				continue;
			}

    			if ((rv = get_known_header(&ocon->phttp->hdr, token,
					&data, &datalen, 
					&attrs, &hdrcnt)) == 0) {
				int nth = 0;
				if (http_removable_304_header[token] && hdrcnt == 0) {
					delete_known_header(&hdrs, token);
					continue;
				}
				rv = update_known_header(&hdrs, token, data, 
						datalen, 1 /* Replace */);
				if (rv) {
					TSTR(data, datalen, p_value);
					DBG_LOG(MSG, MOD_OM, 
						"add_known_header(%s, %s) "
						"failed, rv=%d, fd=%d",
						http_known_headers[token].name,
						p_value, rv, OM_CON_FD(ocon));
					goto exit;
				}
				// Add Multi valued headers
				for (nth = 1; nth < hdrcnt; nth++) {
					rv = get_nth_known_header(
						&ocon->phttp->hdr, token, nth, 
						&data, &datalen, &attrs);
					if (rv) {
						DBG_LOG(MSG, MOD_OM, 
						"get_nth_known_header(n=%d) "
						"failed, "
						"nth=%d rv=%d", token, nth, rv);
						    continue;
					}
					add_known_header(&hdrs, token, 
							 data, datalen);
				}
			} else if (http_removable_304_header[token]) {
				delete_known_header(&hdrs, token);
			}
		}

		// Update unknown headers
		// This would add any custom headers inserted by PE
		nth = 0;
		while ((rv = get_nth_unknown_header(&ocon->phttp->hdr, nth, &name, 
				&namelen, &data, &datalen, &attrs)) == 0) {
			nth++;
			if (is_unknown_header_present(&hdrs, name, namelen)) {
				rv = delete_unknown_header(&hdrs, name, namelen);
			}
			rv = add_unknown_header(&hdrs, name, namelen, data, datalen);
		}

                if (ocon->oscfg.policies->object_correlation_validatorsall_ignore) {
                        version_check = 0;//avoid additional check below
                        glob_om_validators_all_ignore++;
		}

		// Insure the object version is still the same otherwise 
		// consider it modified
		if (version_check ) {
			cnp = nkn_cod_get_cnp(ocon->p_validate->in_uol.cod);
			if (cnp && *cnp) {
			    int dummy;
			    if (ocon->oscfg.policies->object_correlation_etag_ignore) {
				    rv = compute_object_version_use_lm(&hdrs, cnp, strlen(cnp),
								    http_encoding_type, &obj_vers,
								    &dummy);
			    } else {
				    rv = compute_object_version(&hdrs, cnp, strlen(cnp),
								    http_encoding_type, &obj_vers,
								    &dummy);
			    }
			    if (!rv) {
				if (!NKN_OBJV_EQ(&obj_vers, 
					&ocon->p_validate->in_attr->obj_version)) {

				    /*! Bug 3890 , 
				     * If ETAG and LAST_MODIFIED Headers are not present, 
				     * then object version will not match , Treat this case as valid
				     */
				    if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_ETAG) ||
					is_known_header_present(&ocon->phttp->hdr, MIME_HDR_LAST_MODIFIED) ) { 

				    	ocon->p_validate->ret_code = MM_VALIDATE_MODIFIED;
				    	DBG_LOG(MSG, MOD_OM, "objv changed cod=0x%lx "
						    "cnp=%s marking as Modified", 
				  		  ocon->p_validate->in_uol.cod, cnp);
				    	goto exit;
				    }
				}
			    } else {
				ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				DBG_LOG(MSG, MOD_OM, 
					"compute_object_version() failed, "
					"rv=%d", rv);
				goto exit;
			    }
			} else {
			    	ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				DBG_LOG(MSG, MOD_OM, "nkn_cod_get_cnp() failed, "
					"cod=0x%lx", ocon->p_validate->in_uol.cod);
				goto exit;
			}
		}

		/*
		 * BUG 5907: If origin server returns "Cache-Control: no-cache"
		 * We should expire cached object.
		 */
		if (is_known_header_present(&hdrs, MIME_HDR_X_ACCEL_CACHE_CONTROL)) {
			rv = get_cooked_known_header_data(&hdrs,
					MIME_HDR_X_ACCEL_CACHE_CONTROL,
					&ckdp, &ckd_len, &dtype);
		} else {
			rv = get_cooked_known_header_data(&hdrs,
					MIME_HDR_CACHE_CONTROL,
					&ckdp, &ckd_len, &dtype);
		}
		if (!rv && (dtype == DT_CACHECONTROL_ENUM) && (cache_object == 0)) {
			int is_maxage_zero, is_no_cache;
			is_maxage_zero = is_no_cache = 0;
			/**
			// cache-override follow config + no-cache + max-age=0 case
			if ((ocon->oscfg.policies->cache_no_cache == NO_CACHE_FOLLOW) &&
			    (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_CACHE) &&
			    ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) && 
			     (ckdp->u.dt_cachecontrol_enum.max_age == 0))) {
				// when "no-cache" is present.
				ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				goto exit;
			}
			**/
			is_maxage_zero = ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) && 
					(ckdp->u.dt_cachecontrol_enum.max_age == 0)) ? 1 : 0;
			is_no_cache = (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_CACHE) ? 1 : 0;
			if ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_STORE) &&
			    !(is_no_cache || is_maxage_zero) &&
			    (ocon->oscfg.policies->cache_no_cache == NO_CACHE_FOLLOW)){
				// when "no-store" is present.
				ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				goto exit;
			}
		}

		/*
		 * Apply CLI configured cache-control policies.
		 */
		if ((ocon->oscfg.policies->cache_no_cache == NO_CACHE_TUNNEL) &&
		     ocon->resp_mask == OM_RESP_CC_MAX_AGE) {
			max_age_tunnel = 1;
		}

		if ((ocon->oscfg.policies->cache_no_cache != NO_CACHE_TUNNEL) ||
		    max_age_tunnel) {
			rv = apply_cc_max_age_policies(
					ocon->oscfg.policies,
					&hdrs, OM_CON_FD(ocon));
			if (rv) {
				DBG_LOG(MSG, MOD_OM, "apply_cc_max_age_policies() "
					"failed rv=%d, fd=%d", rv, OM_CON_FD(ocon));
			}
		}

		/*
		 * Apply Policy Engine Script returned cache-control result.
		 */

		/*
		 * Special case for cookie handling. If set cookie cacheable, then we've to strip 
		 * the respective cookie headers from the origin 304 response. This is done here 
		 * in order to maintain the already cached object to be cookie agnostic.
		 */

		set_cookie = is_known_header_present(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE);
		set_cookie2 = is_known_header_present(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2);

		if (set_cookie || set_cookie2) {
			if ((cache_object == 1) ||
			    ocon->oscfg.policies->is_set_cookie_cacheable == 1) {
				const char *cookie_str;
				int cookie_len, rv1;
				u_int32_t attr;
				int hdrcnt1, nth;
				con_t *org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);

				// Special case for handling 304 with Set-Cookie/Set-Cookie2 headers but the original
				// object is a normal cached object i.e, NKN_PROXY_REVAL not set in cache attribute.
				if (!(ocon->p_validate->in_attr->na_flags & NKN_PROXY_REVAL) &&
				    !(ocon->oscfg.policies->cache_cookie_noreval)) {
					ocon->p_validate->ret_code = MM_VALIDATE_MODIFIED;
					DBG_LOG(MSG, MOD_OM, "304 response with Set-Cookie/Set-Cookie2 headers but proxy reval flag "
							"unset in cache attribute. Force expiration to update cache, fd=%d",
							OM_CON_FD(ocon));
					shutdown_http_header(&hdrs);
					return TRUE;
				}

				delete_known_header(&org_httpreqcon->http.hdr, MIME_HDR_SET_COOKIE);
				delete_known_header(&org_httpreqcon->http.hdr, MIME_HDR_SET_COOKIE2);

				if (set_cookie) {
					rv1 = mime_hdr_get_known(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE, &cookie_str, &cookie_len, &attr, &hdrcnt1);
					if (!rv1) {
						delete_known_header(&hdrs, MIME_HDR_SET_COOKIE);
					} else {
						DBG_LOG(MSG, MOD_OM, "mime_hdr_get_known(MIME_HDR_SET_COOKIE) failed ");
					}
					for(nth = 0; nth < hdrcnt1; nth++) {
						rv1 = get_nth_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE, nth, &cookie_str, &cookie_len, &attrs);
						if (!rv1) {
							add_known_header(&org_httpreqcon->http.hdr, MIME_HDR_SET_COOKIE, cookie_str, cookie_len);
						} else {
							DBG_LOG(MSG, MOD_OM,
									"add_known_header() failed rv=%d nth=%d, MIME_HDR_SET_COOKIE ",
									rv1, nth);
						}
					}
				}

				if (set_cookie2) {
					rv1 = mime_hdr_get_known(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2, &cookie_str, &cookie_len, &attr, &hdrcnt1);
					if (!rv1) {
						delete_known_header(&hdrs, MIME_HDR_SET_COOKIE2);
					} else {
						DBG_LOG(MSG, MOD_OM, "mime_hdr_get_known(MIME_HDR_SET_COOKIE2) failed ");
					}
					for(nth = 0; nth < hdrcnt1; nth++) {
						rv1 = get_nth_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2, nth, &cookie_str, &cookie_len, &attrs);
						if (!rv1) {
							add_known_header(&org_httpreqcon->http.hdr, MIME_HDR_SET_COOKIE2, cookie_str, cookie_len);
						} else {
							DBG_LOG(MSG, MOD_OM,
									"add_known_header() failed rv=%d nth=%d, MIME_HDR_SET_COOKIE2 ",
									rv1, nth);
						}
					}
				}
				SET_HTTP_FLAG(&org_httpreqcon->http, HRF_CACHE_COOKIE);
			} else {
				goto exit;
			}
		}

		// Update Vary header
		// Take care of case when vary header is changed but origin
		//	returns 304. Will this happen ?
		if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY)) {
			const char *hdr_value = 0;
			int hdr_len;
			//u_int32_t attrs;
			//int hdrcnt;

			// if Vary header handeling is not enabled, tunnel response
			if (ocon->httpreqcon->http.nsconf->http_config->vary_hdr_cfg.enable == 0) { 
				//glob_tunnel_resp_vary_handling_disabled++;
				DBG_LOG(MSG, MOD_OM,
					"Vary header handling not enabled");
				ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				goto exit;
			}
			// If other headers are indicated in vary header, tunnel the response
			if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY_OTHERS)) { 
				DBG_LOG(MSG, MOD_OM,
					"Un Supported vary header field value");
				//glob_tunnel_resp_vary_field_not_supported++;
				ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
				goto exit;
			}
			// Check if User Agent is indicated in the VARY header.
			if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY_USR_AGNT)) { 
				// If so get the User Agent header from the request and
				// store it in the response header
				if (!is_known_header_present(&hdrs, MIME_HDR_X_NKN_USER_AGENT)) {
					con_t *org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);
					if (!get_known_header(&org_httpreqcon->http.hdr, MIME_HDR_USER_AGENT,
								&hdr_value, &hdr_len, &attrs, &hdrcnt)) {
						add_known_header(&hdrs, MIME_HDR_X_NKN_USER_AGENT,
									hdr_value, hdr_len);
					}
				}
				// Set VARY flag in attributes
				vary_hdr = 1;
				//glob_om_vary_hdr_user_agent++;
			}
			else {
				if (is_known_header_present(&hdrs, MIME_HDR_X_NKN_USER_AGENT)) {
					delete_known_header(&hdrs, MIME_HDR_X_NKN_USER_AGENT);
				}
			}

			// Check if Accept-Encoding is indicated in the VARY header.
			if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY_ACCPT_ENCD)) { 
				if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_CONTENT_ENCODING)
						&& (ocon->phttp->http_encoding_type != HRF_ENCODING_UNKNOWN)) {
					vary_encoding = 1;
				}
				// Set VARY flag in attributes
				//glob_om_vary_hdr_accept_encoding++;
				vary_hdr = 1;
			}
		}
		else {
			if (is_known_header_present(&hdrs, MIME_HDR_X_NKN_USER_AGENT)) {
				delete_known_header(&hdrs, MIME_HDR_X_NKN_USER_AGENT);
			}
		}

		rv = mime_header2nkn_buf_attr(&hdrs, 0, 
					      ocon->p_validate->new_attr,
					      nkn_buffer_set_attrbufsize,
					      nkn_buffer_getcontent);
		if (rv) {
			DBG_LOG(MSG, MOD_OM, 
				"mime_header2nkn_buf_attr() failed, rv=%d, "
				"fd=%d", rv, OM_CON_FD(ocon));
			goto exit;
		}
		pnew_attr = (nkn_attr_t *) 
			nkn_buffer_getcontent(ocon->p_validate->new_attr);
	        if (ocon->oscfg.policies->ignore_s_maxage) {
        	        s_maxage_ignore = 1;
        	}
		compute_object_expiration(&hdrs, 
			s_maxage_ignore,
			ocon->request_send_time, 
			ocon->request_response_time,  
			nkn_cur_ts,
			&corrected_initial_age,
			&pnew_attr->cache_expiry);

		/* Apply crawler expiry if the object is crawled */
		if(ocon->p_validate->in_client_data.flags & NKN_CLIENT_CRAWL_REQ) {
		    rv = cim_get_params(ocon->uol.cod, &expiry, &cache_pin,
					&cim_flags);
		    if(!rv) {
			if (expiry != 0)
			    pnew_attr->cache_expiry = expiry;
		    }
		    pnew_attr->origin_cache_create = ocon->request_response_time;
		} else {
		    pnew_attr->origin_cache_create = 
			    ocon->request_response_time - corrected_initial_age;
		    pnew_attr->cache_correctedage = corrected_initial_age;
		}
		if (pnew_attr->cache_expiry == NKN_EXPIRY_FOREVER) {
		    pnew_attr->cache_expiry = nkn_cur_ts +
			    ocon->oscfg.policies->cache_age_default;
		}

		if (vary_hdr) {
			nkn_attr_set_vary(pnew_attr);
		}
		else {
			if (nkn_attr_is_vary(pnew_attr)) {
				nkn_attr_reset_vary(pnew_attr);
			}
		}
		if (vary_encoding) {
			//pnknattr->na_flags |= NKN_OBJ_COMPRESSED;
			pnew_attr->encoding_type = ocon->phttp->http_encoding_type; 
		}

		ocon->p_validate->ret_code = MM_VALIDATE_NOT_MODIFIED;
		DBG_LOG(MSG, MOD_OM, "Object not modified, fd=%d", 
			OM_CON_FD(ocon));
		/* BZ 3442 , Return TRUE for 304 response from origin and
		 * do not close the connection */
		shutdown_http_header(&hdrs);
		return TRUE; 

	}
	if ((ocon->phttp->respcode >= 500) && (ocon->phttp->respcode <= 505)) {
		//5xx error are treated as validate errors 
		ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
	}

	if (ocon->phttp->respcode == 401) {
		ocon->p_validate->ret_code = MM_VALIDATE_BYPASS;
		goto exit;
	}
 
	if ((ocon->phttp->respcode == 404) ||
	    (ocon->phttp->respcode == 302) ||
	    (ocon->phttp->respcode == 301) ||
	    (ocon->phttp->respcode == 305) ||
	    (ocon->phttp->respcode == 300) ||
	    (ocon->phttp->respcode == 307)) { 
                /* BZ 3525: added handling for 3xx codes (except 304) 
                 */
 		/*
		 * Permanent failure cases - return FAIL instead of ERROR. 
		 * If FAIL is returned no further action, like retry would 
		 * be taken.
		 */
#if 0
		/* BZ 3639 - Disable this code for now, due to
		 * VALGRIND errors
		 */
		out_proto_data_t *pout_proto_resp = 
			    &ocon->p_validate->out_proto_resp;
#endif
		ocon->p_validate->ret_code = MM_VALIDATE_FAIL;
		DBG_LOG(MSG, MOD_OM, "Object validate failed, resp=%d, fd=%d", 
			ocon->phttp->respcode, OM_CON_FD(ocon));
		goto exit;
#if 0
		/* BZ 3639 - Disable this code for now, due to
		 * VALGRIND errors
		 */
		pout_proto_resp->u.OM.data = 
			(char *)nkn_malloc_type(
				ocon->phttp->cb_totlen, mod_http_om_respbuf);
		if (pout_proto_resp->u.OM.data != NULL) {
		    memmove(pout_proto_resp->u.OM.data, 
				ocon->phttp->cb_buf, ocon->phttp->cb_totlen);
		    pout_proto_resp->u.OM.data_len = ocon->phttp->cb_totlen;
		} else {
		    DBG_LOG(MSG, MOD_OM, "(fd=%d) malloc failed",
							 OM_CON_FD(ocon));
		    pout_proto_resp->u.OM.data_len = 0;
		}
#endif
	}
 
	ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
	DBG_LOG(MSG, MOD_OM, "Object validate error, fd=%d", OM_CON_FD(ocon));
exit:
	shutdown_http_header(&hdrs);
	return FALSE; // Close connection
}

static int get_cache_age_policies(origin_server_policies_t *origin_policy,
				     mime_header_t *hdr, int fd, int *age_value)
{
    const char *content_type;
    int content_type_len;
    u_int32_t attrs;
    int hdrcnt;
    int rv = 0;
    int n;
    const char *policy_content_type;
    int policy_content_type_strlen;
    const char *policy_max_age;
    //int policy_max_age_strlen;
    cache_age_policies_t *policy ;

    const cooked_data_types_t *ckdp;
    int ckd_len;
    mime_hdr_datatype_t dtype;

    if(origin_policy)
	    policy = &origin_policy->cache_age_policies;

    if (!policy || !hdr) {
	DBG_LOG(MSG, MOD_OM, "Required args missing, policy=%p hdr=%p fd=%d",
		policy, hdr, fd);
    	return 1; 
    }

    if (!policy->entries) {
    	// Nothing to do
	return 0;
    }

    rv = get_cooked_known_header_data(hdr, MIME_HDR_CACHE_CONTROL, &ckdp, &ckd_len, 
				      &dtype);
    if (!rv && (dtype == DT_CACHECONTROL_ENUM) &&
	(origin_policy->cache_no_cache == NO_CACHE_FOLLOW)) {
	// no-cache + max-age=0 case
	if ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_CACHE) &&
	    ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) &&
	     (ckdp->u.dt_cachecontrol_enum.max_age == 0))) {
	    DBG_LOG(MSG, MOD_OM, 
		    "no-cache present, max-age not allowed, fd=%d", fd);
	   // "Max-age=xxx" not allowed if "no-cache" is present.
	    return 0;
	}
	if (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_STORE) {
	    DBG_LOG(MSG, MOD_OM, 
		    "no-store present, max-age not allowed, fd=%d", fd);
	   // "Max-age=xxx" not allowed if "no-store" is present.
	    return 0;
	}
    }

    rv = get_known_header(hdr, MIME_HDR_CONTENT_TYPE, 
		    	  &content_type, &content_type_len, &attrs, &hdrcnt);
    if (rv) {
        content_type = "";
        content_type_len = 1;
    }

    // Look for Content-Type match

    policy_max_age = 0;
    for (n = 0; n < policy->entries; n++) {
    	policy_content_type = policy->content_type_to_max_age[n]->content_type;
    	policy_content_type_strlen = 
	    policy->content_type_to_max_age[n]->content_type_strlen;

	if (!policy_content_type /* "any" case */ ||
	    ((policy_content_type_strlen == content_type_len) && 
	     !memcmp(policy_content_type, content_type, content_type_len))) {
                *age_value = MAX(0, ckdp->u.dt_cachecontrol_enum.max_age);
                DBG_LOG(MSG, MOD_OM, 
                        "Update fd=%d Override age: %d", fd, *age_value);
                rv = 2;
                break;
            }
	}

    return rv;
}

static int update_cache_control_header (mime_header_t *hdr, const char *policy_max_age, 
        int policy_max_age_strlen, int fd)
{
    int rv;

    if (is_known_header_present(hdr, MIME_HDR_CACHE_CONTROL)) {
        rv = update_known_header(hdr, MIME_HDR_CACHE_CONTROL, 
                policy_max_age, policy_max_age_strlen, 0);
        if (!rv) {
            DBG_LOG(MSG, MOD_OM, 
                    "Update fd=%d Cache-Control: %s", fd, policy_max_age);
        } else {
            DBG_LOG(MSG, MOD_OM, 
                    "update_known_header() failed, rv=%d fd=%d", rv, fd);
            rv = 2;
        }
    } else {
        rv = add_known_header(hdr, MIME_HDR_CACHE_CONTROL, 
                policy_max_age, policy_max_age_strlen);
        if (!rv) {
            DBG_LOG(MSG, MOD_OM, 
                    "Add fd=%d Cache-Control: %s", fd, policy_max_age);
        } else {
            DBG_LOG(MSG, MOD_OM, 
                    "add_known_header() failed, rv=%d fd=%d", rv, fd);
            rv = 3;
        }
    }
    if (!rv) {
        DBG_LOG(MSG, MOD_OM,
                "Max-age content type policy update is successful. Deleting Age header, if present.");
        // Worry not if it succeeds or fails.
        delete_known_header (hdr, MIME_HDR_AGE);
    }

    return rv;

}


/*
 * Apply cli "namespace <name> origin-fetch cache-age" policies
 */
int apply_cc_max_age_policies(origin_server_policies_t *origin_policy,
				     mime_header_t *hdr, int fd)
{
    const char *content_type;
    char resp_cont_type[MAX_HTTP_HEADER_SIZE];
    int content_type_len, content_type_offset = 0;
    u_int32_t attrs;
    int hdrcnt;
    int rv;
    int n;
    const char *policy_content_type;
    int policy_content_type_strlen;
    const char *policy_max_age;
    int policy_max_age_strlen;
    cache_age_policies_t *policy ;
    content_type_max_age_t *pContent_type_any = NULL;

    const cooked_data_types_t *ckdp;
    int ckd_len;
    mime_hdr_datatype_t dtype;

    if(origin_policy)
	    policy = &origin_policy->cache_age_policies;

    if (!policy || !hdr) {
	DBG_LOG(MSG, MOD_OM, "Required args missing, policy=%p hdr=%p fd=%d",
		policy, hdr, fd);
    	return 1; 
    }

    if (!policy->entries) {
    	// Nothing to do
	return 0;
    }

    rv = get_cooked_known_header_data(hdr, MIME_HDR_CACHE_CONTROL, &ckdp, 
				&ckd_len, &dtype);
    if (!rv && (dtype == DT_CACHECONTROL_ENUM) && 
	(origin_policy->cache_no_cache == NO_CACHE_FOLLOW)) {
	// no-cache + max-age=0 case
	if ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_CACHE) &&
	    ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) &&
	     (ckdp->u.dt_cachecontrol_enum.max_age == 0))) {
	    DBG_LOG(MSG, MOD_OM, 
		    "no-cache present, max-age not allowed, fd=%d", fd);
	   // "Max-age=xxx" not allowed if "no-cache" is present.
	    return 0;
	}
	if (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_STORE) {
	    DBG_LOG(MSG, MOD_OM, 
		    "no-store present, max-age not allowed, fd=%d", fd);
	   // "Max-age=xxx" not allowed if "no-store" is present.
	    return 0;
	}
    }

    rv = get_known_header(hdr, MIME_HDR_CONTENT_TYPE, 
		    	  &content_type, &content_type_len, &attrs, &hdrcnt);
    memset(resp_cont_type, 0, MAX_HTTP_HEADER_SIZE);
    if (rv) {
        content_type = "";
        content_type_len = 1;
	strncpy(&resp_cont_type[0], content_type, 1);
    } else {
	strncpy(resp_cont_type, content_type, content_type_len);
    }

    // Look for Content-Type match

    policy_max_age = 0;
    for (n = 0; n < policy->entries; n++) {
    	policy_content_type = policy->content_type_to_max_age[n]->content_type;
    	policy_content_type_strlen = 
	    policy->content_type_to_max_age[n]->content_type_strlen;

        // BZ-10326 change: Hold content-type-any and apply if no specific 
	// content-type is hit
        if (!policy_content_type) // content-type-any case
        {
            pContent_type_any = policy->content_type_to_max_age[n];
            continue;
        }

	// Derive the "type/subtype" specification from the content_type response.
	content_type_offset = strcspn(resp_cont_type, ";");
	if (content_type_len != content_type_offset) //Parameter list with sep ";"
		content_type_len = content_type_offset;

        if ((policy_content_type_strlen == content_type_len) &&
	    (nkn_strcmp_incase(policy_content_type, content_type, 
				content_type_len) == 0)) {
            policy_max_age = policy->content_type_to_max_age[n]->max_age;
            policy_max_age_strlen = 
                policy->content_type_to_max_age[n]->max_age_strlen;
            break;
        }
    }

    if (policy_max_age) 
    {
        rv = update_cache_control_header (hdr, policy_max_age, 
					policy_max_age_strlen, fd);
    } 
    // BZ 10326: Letz handle 'content-type-any' case now
    else if (pContent_type_any)
    {
        rv = update_cache_control_header (hdr, pContent_type_any->max_age, 
					pContent_type_any->max_age_strlen, fd);
    }
    else {
    	rv = 0;
    }

    return rv;
}

static int save_mime_header_t(omcon_t *ocon)
{
    int rv = 0;

    // Since ocon->http.hdr is only valid in the current context,
    // copy it in serialized form for use in generating the 
    // attributes after EOF which may occur in a different task
    // context.
    ocon->hdr_serialbufsz = serialize_datasize(&ocon->phttp->hdr);
    if (ocon->hdr_serialbufsz) {
	ocon->hdr_serialbuf = 
	    nkn_malloc_type(ocon->hdr_serialbufsz, mod_om_ocon_serialbuf);
	if (ocon->hdr_serialbuf) {
	    rv = serialize(&ocon->phttp->hdr, ocon->hdr_serialbuf, 
			   ocon->hdr_serialbufsz);
	    if (rv) {
		DBG_LOG(MSG, MOD_OM, 
			"serialize() failed, rv=%d", rv);
		rv = 1;
		goto err_exit;
	    }
	} else {
	    DBG_LOG(MSG, MOD_OM, "nkn_malloc_type() failed");
	    rv = 2;
	    goto err_exit;
	}
    } else {
	DBG_LOG(MSG, MOD_OM, "serialize_datasize() failed");
	rv = 3;
	goto err_exit;
    }

err_exit:

    return rv;
}

/*
 * This function should be  de-preciated.
 * any non-cacheable response from origin server will be forwarded to client
 * through forward proxy tunnel code path.
 *
 * The code still here is because of chunked-encoding response.
 */
static int setup_tunnel_response(omcon_t *ocon)
{
    int ret;
    nkn_attr_t *pnknattr;
    const cooked_data_types_t *pckd;
    int datalen;
    mime_hdr_datatype_t dtype;
    off_t content_length;

    const char *chunk_encoding;
    int chunk_encoding_len;
    u_int32_t attrs;
    int hdrcnt;

    // Initialize attribute buffer
    if (ocon->p_mm_task->in_attr) {
	    pnknattr = (nkn_attr_t *) 
		    nkn_buffer_getcontent(ocon->p_mm_task->in_attr);
	    ocon->range_offset = ocon->uol.offset; // Force setid on attr
	    ocon->content_data_offset = ocon->range_offset;
    }

    // Determine content length
    ret = get_cooked_known_header_data(&ocon->phttp->hdr, MIME_HDR_CONTENT_LENGTH, 
    				       &pckd, &datalen, &dtype);
    content_length = OM_STAT_SIG_TOT_CONTENT_LEN;
    if (!ret && (dtype == DT_SIZE)) {
    	content_length = pckd->u.dt_size.ll + ocon->phttp->http_hdr_len;

	// Note: content_length = (header length + body length)
	//       To allow proper EOF action on client.  
	//
    	ocon->p_mm_task->out_proto_resp.u.OM.content_length = content_length;
    	ocon->p_mm_task->out_proto_resp.u.OM.flags |= 
		OM_PROTO_RESP_CONTENT_LENGTH;
    } else {
	if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_TRANSFER_ENCODING)) {
	    ret = get_known_header(&ocon->phttp->hdr, MIME_HDR_TRANSFER_ENCODING, 
			    &chunk_encoding, &chunk_encoding_len, 
			    &attrs, &hdrcnt);
	    if ((ret == 0) && (hdrcnt == 1) && 
		(strncasecmp(chunk_encoding, "chunked", 
			     chunk_encoding_len) == 0)) {
		OM_SET_FLAG(ocon, OMF_UNCHUNK_NOCACHE);
	    }
    	}
    	ocon->p_mm_task->out_proto_resp.u.OM.flags |= 
	    OM_PROTO_RESP_NO_CONTENT_LEN;
    }

    OM_SET_FLAG(ocon, OMF_NO_CACHE);
    NKN_MUTEX_LOCK(&omcon_mutex);
    nhash_delete_omcon_by_uri(ocon);
    NKN_MUTEX_UNLOCK(&omcon_mutex);
    ocon->content_length = content_length;

    //ocon->phttp->cb_offsetlen = 0; // copy http header too
    return 0;
}

/*
 * return TRUE: cacheable
 * return FALSE: tunnel_response
 */
static int check_mfd_limitation_for_response(omcon_t *ocon)
{

	/* B1750 
	 * If ssp config is not present we check for 'disable_cache_on_query'
	 */
	if (((ocon->oscfg.nsc->ssp_config == 0) ||
	     (ocon->oscfg.nsc->ssp_config->player_type == -1)) &&
	     (ocon->oscfg.policies && 
	     (ocon->oscfg.policies->disable_cache_on_query == NKN_TRUE)) &&
	      CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_HAS_QUERY) &&
	      (CHECK_ACTION(ocon, PE_ACTION_CACHE_OBJECT) == 0) &&
	      (!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_TIER2_MODE))) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_QUERY_STR;
		glob_tunnel_res_query_str++;
		return FALSE;
	}
	/*
	 * BUG 2696
	 * Byte-range request and chunked encoding response.
	 * we don't know how to handle it, then tunnel the response.
	 */
	if(CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE) &&
	   CHECK_HTTP_FLAG(ocon->phttp, HRF_TRANSCODE_CHUNKED)) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_RANGE_CHUNKED;
		glob_tunnel_res_byte_range_and_chunked++;
		return FALSE;
	}

	/*
	 * PR: 765070
	 * Any chunked encoded data with sub-encoding(for eg: 'Transfer-Encoding: zip,chunked')
         * will be just tunnelled.
	 * Only cacheable data with 'Transfer-Encoding: chunked' will be cached.
         */
	if(CHECK_HTTP_FLAG2(ocon->phttp, HRF2_TRANSCODE_CHUNKED_WITH_SUB)) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SUB_ENCODED_CHUNKED;
		glob_tunnel_res_chunked_with_sub_encoding++;
		return FALSE;
	}

	/*
	 * BUG 1662
	 * Allow tunnel path for those response without content length or content length set to 0.
	 */
	if(!CHECK_HTTP_FLAG(ocon->phttp, HRF_CONTENT_LENGTH) &&
	   !CHECK_HTTP_FLAG(ocon->phttp, HRF_TRANSCODE_CHUNKED)) {
	   if((ocon->phttp->respcode != 302) &&
		    (!OM_CHECK_FLAG(ocon, OMF_NEGATIVE_CACHING))) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NO_CONTLEN_NO_CHUNK;
		glob_tunnel_res_no_conlen_or_no_chunk++;
		return FALSE;
	   }
	}

	/*
	 * Bug 2996
 	 * MFD returns response code 404, if the content length is zero.
	 *
	 * BUG 6481:
	 * Do not tunnel content_lenth=0 when redirect time.
	 */
	if(CHECK_HTTP_FLAG(ocon->phttp, HRF_CONTENT_LENGTH) &&
			ocon->phttp->content_length == 0) {
	   if((ocon->phttp->respcode != 302) &&
		(!OM_CHECK_FLAG(ocon, OMF_NEGATIVE_CACHING))) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CONTLEN_ZERO;
		glob_tunnel_res_conlen_zero++;
		return FALSE;
	   }
	}

	/* If the URI had hex encoding, process the request through MFD,
	 * but tunnel the response, so that the request headers could be modified
	 * by MFD.
	 * BZ 3731
	 */
	if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_HAS_HEX_ENCODE)) {
		if ((ocon->oscfg.nsc->ssp_config == 0 ||  
		    ocon->oscfg.nsc->ssp_config->player_type == -1) && 
		    !CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_DYNAMIC_URI) &&
		    !CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
		    !CHECK_HTTP_FLAG2(&ocon->httpreqcon->http, HRF2_RBCI_URI) &&
		    !CHECK_HTTP_FLAG2(&ocon->httpreqcon->http, HRF2_QUERY_STRING_REMOVED_URI) &&
		    (!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_TIER2_MODE))) {
			glob_tunnel_res_hex_encode++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_HEX_ENCODED;
			return FALSE;
		}
	}

	/* Dont check for size if its a byte range request or
	 * a request for the rbci 
	 * TODO: Idealy the length should have been taken from the content-range
	 * and the check can be avoided.
	 */
	if(CHECK_HTTP_FLAG(ocon->phttp, HRF_CONTENT_LENGTH) &&
		!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE) &&
		!CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
		!CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF))
	{
		if(ocon->oscfg.policies &&
		   ((ocon->oscfg.policies->cache_max_size &&
			(ocon->phttp->content_length >
			    ocon->oscfg.policies->cache_max_size)) ||
			(ocon->oscfg.policies->cache_min_size &&
			(ocon->phttp->content_length <
			    ocon->oscfg.policies->cache_min_size)))) {
			ocon->httpreqcon->http.tunnel_reason_code =
						    NKN_TR_RES_CACHE_SIZE_LIMIT;
			return FALSE;
		}
	}

#if 0
	/*
	 * The HTTP response header is > 512 bytes, we tunnel the reponse.
	 */
	if (ocon->phttp->http_hdr_len > NKN_MAX_ATTR_SIZE) {
		DBG_LOG(MSG, MOD_OM, "fd=%d large http response header=%d",
			OM_CON_FD(ocon), ocon->phttp->http_hdr_len);
		glob_om_no_cache_large_header_resp++;
		return 0;
	}
#endif

	if (ocon->phttp->http_encoding_type == HRF_ENCODING_UNKNOWN) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NOT_SUPPORTED_ENCODING;
		return FALSE;
	}

	// Parse CC headers.
	parse_cc_headers_to_mask(ocon);

	/* cacheable response so far */
	return TRUE;
}

// Parse CC headers to derive response mask based on it
static int parse_cc_headers_to_mask(omcon_t *ocon) {
	int ret;
	const cooked_data_types_t *pckd;
	int datalen;
	mime_hdr_datatype_t dtype;
	if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_CACHE_CONTROL)) {
		ret = get_cooked_known_header_data(&ocon->phttp->hdr,
					MIME_HDR_CACHE_CONTROL,
					&pckd, &datalen, &dtype);
		if (!ret && (dtype == DT_CACHECONTROL_ENUM)) {
			if (pckd->u.dt_cachecontrol_enum.mask & (1 << CT_MAX_AGE)) {
				if (pckd->u.dt_cachecontrol_enum.max_age == 0)  {
					OM_SET_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0);
				} else {
					OM_SET_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE);
				}
			}
			if (pckd->u.dt_cachecontrol_enum.mask & (1 << CT_NO_CACHE)) {
				OM_SET_RESP_MASK(ocon, OM_RESP_CC_NO_CACHE);
			}
			if (pckd->u.dt_cachecontrol_enum.mask & (1 << CT_NO_STORE)) {
				OM_SET_RESP_MASK(ocon, OM_RESP_CC_NO_STORE);
			}
			if (pckd->u.dt_cachecontrol_enum.mask & (1 << CT_PRIVATE)) {
				OM_SET_RESP_MASK(ocon, OM_RESP_CC_PRIVATE);
			}
		}
	}
	return ret;
}

//
// connection pool calls back when receives the HTTP response.
//
static int cp_callback_http_header_parse_error(void * private_data)
{
	omcon_t * ocon = (omcon_t *)private_data;

	cancel_timer(ocon);

	if (ocon->cpcon) ocon->state = ((cpcon_t *) ocon->cpcon)->state;

	if (OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		if (ocon->p_mm_task) {
			if (!ocon->p_mm_task->err_code) {
				ocon->p_mm_task->err_code = NKN_OM_PARSE_ERR_1;
			}
			memcpy(&ocon->p_mm_task->out_proto_resp.u.OM.remote_ipv4v6,
				&ocon->phttp->remote_ipv4v6, sizeof(ip_addr_t));
			memcpy(&(ocon->p_mm_task->out_proto_resp.u.OM.remote_ipv4v6), &(ocon->phttp->remote_ipv4v6),
				sizeof(ip_addr_t));
			ocon->p_mm_task->
				out_proto_resp.u.OM.remote_port =
						    ocon->phttp->remote_port;
			ocon->p_mm_task->
				out_proto_resp.u.OM.cp_sockfd =
						    OM_CON_FD(ocon);
			ocon->p_mm_task->
				out_proto_resp.u.OM.httpcon_sockfd =
						    ocon->httpfd;
			ocon->p_mm_task->
				out_proto_resp.u.OM.incarn =
						    gnm[OM_CON_FD(ocon)].incarn;
			if(!IS_PSEUDO_FD(ocon->httpfd)) {
				OM_SET_FLAG(ocon, OMF_NO_CACHE);
    				NKN_MUTEX_LOCK(&omcon_mutex);
				nhash_delete_omcon_by_uri(ocon);
    				NKN_MUTEX_UNLOCK(&omcon_mutex);
			}
			NM_del_event_epoll(OM_CON_FD(ocon));
			return 1;
		}
	}
	else { // OMF_DO_VALIDATE
		if(ocon->p_validate) {
			ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
		}
	}

	return -1;
}

static int data_is_chunked(omcon_t * ocon)
{
	const char *chunk_encoding;
	int        chunk_encoding_len;
	u_int32_t  attrs;
	int        hdrcnt;
	int        ret;

	if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_METHOD_GET) &&
	    is_known_header_present(&ocon->phttp->hdr, MIME_HDR_TRANSFER_ENCODING)) {
		ret = get_known_header(&ocon->phttp->hdr, MIME_HDR_TRANSFER_ENCODING,
				&chunk_encoding, &chunk_encoding_len,
				&attrs, &hdrcnt);
		if ((ret == 0) && (hdrcnt == 1) &&
		    (strncasecmp(chunk_encoding, "chunked",
			     chunk_encoding_len) == 0)) {
			return 1;
		}
	}
	return 0;
}

static int data_is_offline_compress_worth(omcon_t * ocon)
{
	int        ret;
	const cooked_data_types_t *pckd;
	int datalen;
	mime_hdr_datatype_t dtype;
	uint64_t content_length;
	int rv = 0;
	char *dir, *file;
	int dirlen, filelen;
	const char *data;
    	u_int32_t attrs;
    	int hdrcnt;
	int valid_file_type = 0;
	int i =0;
        
	OM_UNSET_FLAG(ocon, (OMF_COMPRESS_FIRST_TASK|OMF_COMPRESS_INIT_DONE|OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN|OMF_COMPRESS_COMPLETE));	

	if(ocon->oscfg.nsc  && (ocon->oscfg.nsc->compress_config->enable == 1) &&
		    CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_METHOD_GET) &&
		    !CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ)) {
	    if(ocon->phttp->http_encoding_type != 0) {
		// Data is already in compressed format , so skip to compress 
		return 0;
	    }

	    if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_CONTENT_LENGTH)) {

		ret = get_cooked_known_header_data(&ocon->phttp->hdr, MIME_HDR_CONTENT_LENGTH,
                                      &pckd, &datalen, &dtype);
    		content_length = OM_STAT_SIG_TOT_CONTENT_LEN;
		if (!ret && (dtype == DT_SIZE)) {
			 content_length = pckd->u.dt_size.ll;
		}

		if(content_length < ocon->oscfg.nsc->compress_config->min_obj_size || content_length > ocon->oscfg.nsc->compress_config->max_obj_size) {
			return 0;
		}
	   } else if(data_is_chunked(ocon) &&  IS_PSEUDO_FD(ocon->httpfd)) {
    		content_length = OM_STAT_SIG_TOT_CONTENT_LEN;
	   } else {
		return 0;
	   } 

		if(ocon->oscfg.nsc->compress_config->algorithm == COMPRESS_DEFLATE) {
			ocon->comp_type = HRF_ENCODING_DEFLATE;
		} else if(ocon->oscfg.nsc->compress_config->algorithm == COMPRESS_GZIP) {
			ocon->comp_type = HRF_ENCODING_GZIP;
		} else {
			ocon->comp_type = HRF_ENCODING_UNKNOWN;
		}
#if 1
			// Get the original URI also
		rv = get_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_X_NKN_URI,
				&data, &datalen, &attrs, &hdrcnt);
		if (rv) {
			DBG_LOG(MSG, MOD_OM, "No NKN_URI found, fd = %d", OM_CON_FD(ocon));
			return 0;
		}

		rv = parse_uri_2(data, datalen, &dir, &dirlen, &file, &filelen);
		if (rv) {
			DBG_LOG(MSG, MOD_OM, "failed in parse_uri_2, fd = %d", OM_CON_FD(ocon));
			return 0;
		}
#endif

		
		for(i =0 ;i<MAX_NS_COMPRESS_FILETYPES; i++) {
			if((ocon->oscfg.nsc->compress_config->file_types[i].allowed == 1 ) && 
			ocon->oscfg.nsc->compress_config->file_types[i].type && 
			(ocon->oscfg.nsc->compress_config->file_types[i].type_len > 0) &&
			file && filelen > 0) {
				char *p = file + (filelen - ocon->oscfg.nsc->compress_config->file_types[i].type_len) ;
				if(strncasecmp(p, ocon->oscfg.nsc->compress_config->file_types[i].type, ocon->oscfg.nsc->compress_config->file_types[i].type_len)== 0) {
					valid_file_type = 1;
					break;
				}
			}
		
		}
			

		if(valid_file_type == 0) 
			return 0;

		
		/* Do  comparison for file-types based compression */

		if(content_length > CM_MEM_PAGE_SIZE && !IS_PSEUDO_FD(ocon->httpfd)) {
			/* Mark for offline compression */
			return 1;
		} else {
			/* Mark for inline compression */
			ocon->comp_status = 1;
			OM_SET_FLAG(ocon, OMF_COMPRESS_FIRST_TASK);
			return 0;
		}

	} 
	return 0;
}
static int swap_to_negative_cache(omcon_t *ocon)
{

    int ret = 0;
    http_cb_t * phttp = ocon->phttp;

    // swap attribute page to negative attribute page
    ret = nkn_buffer_set_bufsize(ocon->p_mm_task->in_attr,
			nkn_buffer_get_bufsize(ocon->p_mm_task->in_attr),
                        BM_NEGCACHE_ALLOC);
    if (ret) {
	DBG_LOG(MSG, MOD_OM, "Negative attribute page convert\
				failed http_response=%d",
                                ocon->phttp->respcode);
	return 0;
    }

    // swap data page to negative data page.
    ret = nkn_buffer_set_bufsize(ocon->p_mm_task->in_content_array[0],
                                 ocon->phttp->content_length,
                                 BM_NEGCACHE_ALLOC);
    if (ret) {
	DBG_LOG(MSG, MOD_OM, "Negative data page convert\
                              failed http_response=%d",
                              ocon->phttp->respcode);
	return 0;

    }

    phttp->content[0].iov_base = 
		nkn_buffer_getcontent(ocon->p_mm_task->in_content_array[0]);
    phttp->content[0].iov_len =
	        nkn_buffer_get_bufsize(ocon->p_mm_task->in_content_array[0]);

    OM_SET_FLAG(ocon, OMF_BUFFER_SWITCHED);
    OM_SET_FLAG(ocon, OMF_NEGATIVE_CACHING);
    return 1;
}

static int unsuccess_response_cache_worthy(omcon_t * ocon, int *age)
{
	int i =0;
      
	if((ocon->phttp->respcode == 200) || (ocon->phttp->respcode == 206))
	    return 0;
 
	/* IF content-length is more than 32KB then do not cache it */
	if( ocon->phttp->content_length > CM_MEM_PAGE_SIZE) {
	    return 0;
	}

	if(data_is_chunked(ocon)) {
	    return 0;
	}

	if(ocon->oscfg.nsc  && (ocon->oscfg.nsc->non2xx_cache_cfg)) {
	    for(i=0; i < MAX_UNSUCCESSFUL_RESPONSE_CODES; i++) {
		    if((ocon->oscfg.nsc->non2xx_cache_cfg[i].status == 1) &&
			(ocon->oscfg.nsc->non2xx_cache_cfg[i].code == (unsigned)ocon->phttp->respcode)) {
			*age = ocon->oscfg.nsc->non2xx_cache_cfg[i].age;
			return swap_to_negative_cache(ocon);
		    }
	    }
	}

	return 0;
}
static int cp_callback_http_header_received(void *private_data,
					char *p __attribute((unused)),
					int len,
					int content_length __attribute((unused)))
{
	omcon_t * ocon = (omcon_t *)private_data;
        int ret;
	char *outbuf;
	int outbuf_size;
	int do_ingest = 0;
	const cooked_data_types_t *pckd;
	int datalen;
	mime_hdr_datatype_t dtype;
	int cache_object;
	time_t object_expire_time;
	time_t object_corrected_initial_age;
	nkn_attr_t *pnknattr = NULL;
	mime_header_t *reqhdr;
	const char *uri = 0;
	int urilen;
	u_int32_t attrs;
	int hdrcnt;
	AM_pk_t am_pk;
	const char *querystr;
	int object_has_min_expire_time;
	int object_size_cacheable;
	const char *chunk_encoding;
	int chunk_encoding_len;
	char *cod_uri;
	int cod_urilen;
	nkn_objv_t objv;
	int streaming = 0;
	node_map_context_t *nm_ctx;
	request_context_t *req_ctx;
	mime_header_t *httpreq;
	am_object_data_t am_object_data;
	int http_byte_range_streaming = 0;
	cpcon_t *cpcon = NULL;
	con_t *httpreqcon = NULL;
	int is_cookie_tunnel = 0, set_cookie = 0, set_cookie2 = 0;
	int16_t encoding_type = -1;
	time_t expiry = 0;
	int cache_pin = 0;
	int cim_flags = 0;
	int max_age_tunnel = 0;
	int compress_streaming = 0;
	int non_206_origin_streaming = 0;
	int unsuccessful_cache_age = 0;
	int unsuccess_cache_response = 0;
	int s_maxage_ignore = 0;
	int tunnel_no_etag_lmh = 1;
	int nonmatch_type = 0;
	int vary_hdr = 0;
	int vary_encoding = 0;

	cancel_timer(ocon);
	if (ocon->cpcon) {
		cpcon = ocon->cpcon;
		ocon->state = cpcon->state;
	}
	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_HTTP_HEADER_RECEIVED);

	ocon->request_response_time = nkn_cur_ts;
	reqhdr = &ocon->httpreqcon->http.hdr;

	outbuf_size = MAX(TYPICAL_HTTP_CB_BUF, ocon->phttp->cb_max_buf_size);
	outbuf = alloca(outbuf_size);


        /*
         * Before exing from this function, save the flags set
         * in the response path to the 'http' structure from the
         * request.
         */
	ocon->httpreqcon->http.response_flags = ocon->phttp->flag  ;


	/*
	 * Apply Policy Engine
	 */
	ocon->pe_action = PE_ACTION_NO_ACTION;
	if (ocon->oscfg.nsc->http_config->policy_engine_config.policy_file) {
		pe_ilist_t * p_list = NULL;
		pe_rules_t * p_perule;

		p_perule = pe_create_rule(&ocon->oscfg.nsc->http_config->policy_engine_config, &p_list);
		if(p_perule) {
			pe_eval_om_recv_response(p_perule, ocon);
			pe_free_rule(p_perule, p_list);
		}
	}

	if (!OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE) &&
		    CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_METHOD_HEAD) &&
			!ocon->httpreqcon->http.nsconf->http_config->request_policies.http_head_to_get) {
		goto tunnel_response;
	}

	/*
	 * Node map origin response action handler
	 */
	if (ocon->oscfg.nsc->http_config->origin.select_method == 
			OSS_HTTP_NODE_MAP) {
	    if (OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
	    	req_ctx = &ocon->p_validate->req_ctx;
		nm_ctx = 
		    REQUEST_CTX_TO_NODE_MAP_CTX(&ocon->p_validate->req_ctx);
		httpreq = 
			&((con_t *)(ocon->p_validate->in_proto_data))->http.hdr;
	    } else {
	    	req_ctx = &ocon->p_mm_task->req_ctx;
		nm_ctx = REQUEST_CTX_TO_NODE_MAP_CTX(&ocon->p_mm_task->req_ctx);
		httpreq = 
			&((con_t *)(ocon->p_mm_task->in_proto_data))->http.hdr;
	    }
	    if (nm_ctx) {
		DBG_LOG(MSG, MOD_OM, "NodeMap request, fd=%d req_ctx=%p",
			OM_CON_FD(ocon), req_ctx);
	    	ret = (*nm_ctx->my_node_map->origin_reply_action)(
						&nm_ctx->my_node_map->ctx,
						req_ctx,
						httpreq,
						&ocon->phttp->hdr,
						ocon->phttp->respcode);
		if (ret) {
		    char dummy_client_req_ip_port[16];
		    char res_origin_ip[128];
		    char *p_res_origin_ip;
		    int origin_ip_len;
		    uint16_t res_origin_port;

		    dummy_client_req_ip_port[0] = 0;
		    res_origin_ip[0] = 0;
		    p_res_origin_ip = res_origin_ip;

		    ret = request_to_origin_server(
		    			REQ2OS_SET_ORIGIN_SERVER, req_ctx,
				      	reqhdr, ocon->httpreqcon->http.nsconf, 
				      	dummy_client_req_ip_port, 
				      	sizeof(dummy_client_req_ip_port),
					&p_res_origin_ip,
					sizeof(res_origin_ip),
					&origin_ip_len,
					&res_origin_port, 0, 0);
		   if (!ret) { 
		       /* 
		     	* Dismiss with unconditional reply
		     	*/
		    	set_uncond_escal_retry(ocon);
		    	return -1; // Close socket
		    } else {
			if (OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
				ocon->p_mm_task->err_code = 
					NKN_OM_GET_START_ERR;
			} else {
				ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
			}
		    	return -1; // Close socket
		    }
		}
	    }
	}

	if (OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
		NM_FLOW_TRACE(OM_CON_FD(ocon));
		if( ocon->p_validate && (process_validate_response(ocon) == FALSE) ) {
			NM_FLOW_TRACE(OM_CON_FD(ocon));
			return -1;
		}
		return 2;
	}

	/*
	 * BUG 3375: I have no idea why this time task could be lost.
	 */
	if (!ocon->p_mm_task) {
		NM_FLOW_TRACE(OM_CON_FD(ocon));
		return -1;
	}

	if (ocon->p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
	        char c = ocon->phttp->cb_buf[ocon->phttp->cb_offsetlen];
	        ocon->phttp->cb_buf[ocon->phttp->cb_offsetlen] = '\0';;
	    	DBG_TRACE("OM http_response object %s\n%s",
			  nkn_cod_get_cnp(ocon->uol.cod), ocon->phttp->cb_buf);
	        ocon->phttp->cb_buf[ocon->phttp->cb_offsetlen] = c;
	}

	/* 
	 * Copy accesslog data.
	 * We copy certain fields from ocon->phttp to the task output
	 * protocol response data so that these can be used by the http module.
	 * We cannot just copy this data to the con_t since multiple
	 * waiters on a physid will not receive the data; out_proto_resp 
	 * is copied to all waiters by BM.
	 */
	ocon->p_mm_task->out_proto_resp.u.OM.respcode = ocon->phttp->respcode;
	memcpy(&(ocon->p_mm_task->out_proto_resp.u.OM.remote_ipv4v6), &(ocon->phttp->remote_ipv4v6),
		sizeof(ip_addr_t));
	ocon->p_mm_task->out_proto_resp.u.OM.remote_port = ocon->phttp->remote_port;

	if (cpcon) {
		httpreqcon = (con_t *)(ocon->p_mm_task->in_proto_data);
		if (httpreqcon && CHECK_CON_FLAG(httpreqcon, CONF_RESUME_TASK)) {
			SET_CPCON_FLAG(cpcon, CPF_RESUME_TASK);
		}
	}

	if ((ocon->oscfg.nsc->http_config->policies.cache_ingest.incapable_byte_ran_origin == NKN_TRUE)
		&& (ocon->phttp->respcode == 200)) {
		ocon->p_mm_task->err_code = 0;
		if (!IS_PSEUDO_FD(ocon->httpfd)) {
			non_206_origin_streaming = 1;
			glob_tunnel_res_incapable_byte_range_origin++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_INCAPABLE_BR_ORIGIN;
			if ((CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE_BM) ||
				CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE_HM))) {
				nkn_cod_set_expired(ocon->uol.cod);
				nkn_cod_set_new_version(ocon->uol.cod, objv);
			}
			goto create_am_entry;			
		}
	} else if
	(CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE_BM) || 
		CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE_HM)) {
		/* 
		 * If Byte Range Request from BM or HM and the response is not equal to 206, 
		 * then we have to delete the partial cache. 
		 */
		if (ocon->phttp->respcode != 206) {
			nkn_cod_set_expired(ocon->uol.cod);
			nkn_cod_set_new_version(ocon->uol.cod, objv);
			ocon->p_mm_task->err_code = NKN_OM_INV_PARTIAL_OBJ;
			NS_INCR_STATS(ocon->oscfg.nsc, PROTO_HTTP, server, nonbyterange_cnt);
			glob_tunnel_res_os_no_support_byte_range ++;
			return -1;
		}
	}

	/*
	 * Apply Policy Engine
	 */
	if (ocon->pe_action & PE_ACTION_REDIRECT) {
		ocon->p_mm_task->err_code = NKN_OM_REDIRECT;
		return -1;
	}
	if (ocon->pe_action & PE_ACTION_RES_NO_CACHE_OBJECT) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_POLICY_NO_CACHE;
		glob_tunnel_res_pe++;
		goto tunnel_response;
	}
	if (CHECK_ACTION(ocon->httpreqcon, PE_ACTION_REQ_CACHE_OBJECT)) {
		SET_ACTION(ocon, PE_ACTION_REQ_CACHE_OBJECT);
		//if (!CHECK_ACTION(ocon, PE_ACTION_RES_CACHE_OBJECT))
		//	add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_CACHE_POLICY, "cache", 5);
	}

	set_cookie = is_known_header_present(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE);
	set_cookie2 = is_known_header_present(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2);

#if 0
	if (CHECK_CON_FLAG(httpreqcon, CONF_PE_HOST_HDR) ||
			OM_CHECK_FLAG(ocon, OMF_PE_HOST_HDR)) {
		const char *host_hdr;
		int host_hdr_len;
		int rv;

		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_PE_HOST_HDR, 
				&host_hdr, &host_hdr_len, &attrs, &hdrcnt);
		if (rv) {
			rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_HOST, 
					&host_hdr, &host_hdr_len, &attrs, &hdrcnt);
			if (!rv) {
				add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_PE_HOST_HDR, 
					host_hdr, host_hdr_len);
			}
		}
	}
#endif

	/*
	 * If it is neither 200 nor 206 response, we do not cache. And if 200 OK
	 * comes for byte range  request, then the response will be 
	 * tunnelled. And if the caching is enabled in cli, OM will send a
	 * signal to AM to cache the response in streaming mode.
	 */


	if((unsuccess_cache_response = unsuccess_response_cache_worthy(ocon, &unsuccessful_cache_age))) {

	    goto cache_path;
	}

	/* This is the case where the cache index needs to be calculated from
	 * an offset from the end of the file. 
	 * This is the response to the head request sent to find the content
	 * length of the object. The content length is required to calculate 
	 * the offset for the next fetch for the cache-index 
	 * If the content length is not present return NKN_OM_NON_CACHEABLE,
	 * so that we do a tunnel from server.c
	 */
	if (CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF)) {
		UNSET_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF);
		if (ocon->phttp->respcode == 200) {
			ret = get_cooked_known_header_data(&ocon->phttp->hdr,
							MIME_HDR_CONTENT_LENGTH, &pckd,
							&datalen, &dtype);
			if (!ret && (dtype == DT_SIZE)) {
				ocon->p_mm_task->tot_content_len = pckd->u.dt_size.ll;
				ocon->p_mm_task->err_code = NKN_OM_CI_END_OFFSET_RESPONSE;
			} else {
				UNSET_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ);
				ocon->p_mm_task->err_code = NKN_OM_NON_CACHEABLE;
			}
		} else {
			UNSET_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ);
			ocon->p_mm_task->err_code = NKN_OM_NON_CACHEABLE;
		}
		return -1; //Close the socket
	}

	switch(ocon->phttp->respcode) {
	case 200:
		if (!data_is_chunked(ocon)) {
			if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE)) {
				http_byte_range_streaming = 1;
				if(IS_PSEUDO_FD(ocon->httpfd)) {
					streaming = NKN_COD_SET_STREAMING;
					goto streaming_request;
				} else {
					goto create_am_entry;
				}
			}

			if(data_is_offline_compress_worth(ocon)) {
				// Add code to handle compression 
				compress_streaming = 1;
				goto create_am_entry;
			}

			if(IS_PSEUDO_FD(ocon->httpfd) && (ocon->comp_status == 1)) {
				streaming = NKN_COD_SET_STREAMING;
				goto streaming_request;
			}

			if (IS_PSEUDO_FD(ocon->httpfd) && 
			    CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_STREAMING_DATA)) {
				OM_SET_FLAG(ocon, OMF_HTTP_STREAMING_DATA);
				http_byte_range_streaming = 1;
				streaming = NKN_COD_SET_STREAMING;
				goto streaming_request;
			}
		} else if(IS_PSEUDO_FD(ocon->httpfd)) {
			// Chunked response for internally triggered request  
			int log_ip =1;
			char ip[INET6_ADDRSTRLEN];

			if(ocon->httpreqcon->origin_servers.last_ipv4v6.family == AF_INET) {
                               if(inet_ntop(AF_INET, &IPv4(ocon->httpreqcon->origin_servers.last_ipv4v6),
                                                        &ip[0], INET6_ADDRSTRLEN) == NULL)
                                        log_ip = 0;

                       } else if (ocon->httpreqcon->origin_servers.last_ipv4v6.family == AF_INET6) {
                               if(inet_ntop(AF_INET6, &IPv6(ocon->httpreqcon->origin_servers.last_ipv4v6),
                                                        &ip[0], INET6_ADDRSTRLEN) == NULL)
                               		log_ip = 0;
                       } else {
                               log_ip = 0;
                       }

			if(log_ip) {
				add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_ORIGIN_IP, 
					 ip, strlen(ip));
				
			}

			data_is_offline_compress_worth(ocon);
			streaming = NKN_COD_SET_STREAMING;
			tunnel_no_etag_lmh = 0;
			goto streaming_request;
		} else {
			/* Check MFD limitations */
			if (check_mfd_limitation_for_response(ocon) == FALSE) {
				goto tunnel_response;
			}
			goto skip_cod_set;
		}


	case 206:
		break;
	case 404:
		if (((ocon->oscfg.nsc->ssp_config == 0) ||
		      (ocon->oscfg.nsc->ssp_config->player_type == -1))) {
			glob_tunnel_res_404_code++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SSP_CONFIG_ERR;
			goto tunnel_response;
		} else {
	    		ocon->p_mm_task->err_code = NKN_OM_SSP_RETURN;
			return -1;
		}
	case 302:
		break;
	case 100:
		UNSET_CON_FLAG(ocon->httpreqcon, CONF_BLOCK_RECV);
		/* It has to hit the default case */
	default:
		if (CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
		    (ocon->oscfg.nsc->http_config->cache_index.include_object)) {
			ocon->p_mm_task->err_code = NKN_OM_NON_CACHEABLE;
			UNSET_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ);
			UNSET_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF);
			return -1; //Close the socket
		}
		if (DO_DBG_LOG(MSG, MOD_OM)) {
			DBG_LOG(MSG, MOD_OM, 
				"fd=%d tunnel unexpected http_response "
				"http_response=%d \n%s",
				OM_CON_FD(ocon), ocon->phttp->respcode, 
				dump_http_header(&ocon->phttp->hdr, outbuf, 
					 outbuf_size, 1));
		}
		glob_tunnel_res_unsup_resp++;
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_UNSUPP_RESP;
		goto tunnel_response;
	}

cache_path:
	if ((int16_t)ocon->p_mm_task->encoding_type != -1) {
                encoding_type = ocon->p_mm_task->encoding_type;
        } else {
                encoding_type = ocon->httpreqcon->http.cache_encoding_type;
        }
        if (encoding_type != -1) {
                if (encoding_type == (int16_t)ocon->phttp->http_encoding_type) {
                } else {
                        nkn_cod_set_expired(ocon->uol.cod);
                        nkn_cod_set_new_version(ocon->uol.cod, objv);
                        ocon->p_mm_task->err_code = NKN_OM_INV_PARTIAL_OBJ;
                        glob_cache_encoding_mismatch++;
                        return -1;
                }
        }


streaming_request:
	/* Check MFD limitations */
	if (check_mfd_limitation_for_response(ocon) == FALSE) {
		goto tunnel_response;
	}

	if ((ocon->phttp->respcode == 302) && (unsuccess_cache_response==0)) {
		/* 302 - Redirect */
		DBG_LOG(MSG, MOD_OM, "Setting up 302 redirect");
		ret = process_302_redirect_response(ocon);
		if (ret == -1) {
			goto tunnel_response;
		} else if (ret == -2) {
                        return -1;
		}
	}

	/* Check if the response is cacheable 
	 * If not, tunnel it after adding an entry to AM. 
	 * This is done to support ingestion of chunked objects without TFM.
	 */
	/* In case the response is for internal request, set the 
	 * streaming flag in COD
	 */

	ret = nkn_cod_get_cn(ocon->uol.cod, &cod_uri, &cod_urilen);
	if (ret) {
	    	DBG_LOG(MSG, MOD_OM, 
		    "nkn_cod_get_cn() failed, rv=%d", ret);
	    	ocon->p_mm_task->err_code = NKN_OM_INV_OBJ_VERSION;
	    	return -1;
	}

	if (ocon->oscfg.policies->object_correlation_etag_ignore) {
		ret = compute_object_version_use_lm(&ocon->phttp->hdr, cod_uri, 
				cod_urilen, ocon->phttp->http_encoding_type, &objv,
				&nonmatch_type);
	} else {
		ret = compute_object_version(&ocon->phttp->hdr, cod_uri, 
				cod_urilen, ocon->phttp->http_encoding_type, &objv,
				&nonmatch_type);
	}
	if (ret) {
	    	DBG_LOG(MSG, MOD_OM, 
		    "compute_object_version() failed, rv=%d cod=0x%lx uri=%s encoding_type=%d",
				    ret, ocon->uol.cod, cod_uri, ocon->phttp->http_encoding_type);
	    	ocon->p_mm_task->err_code = NKN_OM_INV_OBJ_VERSION;
	    	return -1;
	}

	// if no etag/lmh then compute object version sets nonmatch type
	// also check if validatorsall ignore not set then tunnel
	// this will not be done for am chunked case.
	if (tunnel_no_etag_lmh && nonmatch_type && !(unsuccess_cache_response) && 
			!(ocon->oscfg.policies->object_correlation_validatorsall_ignore)) {
		glob_tunnel_noetag_lmh++;
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NO_ETAG_LMH;
		goto tunnel_response;
	}
	ret = nkn_cod_test_and_set_version(ocon->uol.cod, objv,
					    NKN_COD_SET_ORIGIN | streaming);
	if (ret) {
		//If we are told to ignore all validators, just move on
                if (ret == NKN_COD_VERSION_MISMATCH &&
                    ocon->oscfg.policies->object_correlation_validatorsall_ignore) {
                        glob_om_validators_all_ignore++;
                        DBG_LOG(MSG, MOD_OM, "Policy set to ignore all validators. Proceeding without version checks");
                        goto skip_cod_set;//as good as fall through
                }
		/* BZ 3330
		 * If we are trying second time and origin gives new content,
		 * tunnel the response.
		 * Increase the retries by one to cater for BZ 3717 and 3733
		 */
		if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_EXPIRE_RESTART_2_DONE)) {
			glob_tunnel_res_cod_expire++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_COD_EXPIRED;
			goto tunnel_response;
		}
		else {
			DBG_LOG(MSG, MOD_OM, 
				"nkn_cod_test_and_set_version() failed, rv=%d "
				"cod=0x%lx uri=%s", 
				ret, ocon->uol.cod, nkn_cod_get_cnp(ocon->uol.cod));
			nkn_cod_set_expired(ocon->uol.cod);
			nkn_cod_set_new_version(ocon->uol.cod, objv);
			ocon->p_mm_task->err_code = NKN_OM_INV_OBJ_VERSION;
			return -1;
		}
	}


skip_cod_set: 
	/*
	 * Apply Cache-Control: max-age policies
	 */
	if ((ocon->oscfg.policies->cache_no_cache == NO_CACHE_TUNNEL) &&
	    (ocon->resp_mask == OM_RESP_CC_MAX_AGE)) {
		max_age_tunnel = 1;
	}

	if ((ocon->oscfg.policies->cache_no_cache != NO_CACHE_TUNNEL) ||
	     max_age_tunnel) {
		ret = apply_cc_max_age_policies(
			ocon->oscfg.policies,
			&ocon->phttp->hdr, OM_CON_FD(ocon));
		if (ret) {
		    DBG_LOG(MSG, MOD_OM, 
	    		    "apply_cc_max_age_policies() failed, rv=%d fd=%d",
		   	 ret, OM_CON_FD(ocon));
		}
	}

#if 0
	/*
	 * delete_invalid_warning_values() removes invalid
	 * warning values. It is decided to remove this
	 * functionality from 11.a.1. If it is needed, it
	 * can be enabled in the future.
	 */
	/* Delete Invalid Warning Values */
	ret = delete_invalid_warning_values(&ocon->phttp->hdr);
	if (ret) {
	    DBG_LOG(MSG, MOD_OM, 
		    "delete_invalid_warning_values() failed, rv=%d \
		    fd=%d", ret, OM_CON_FD(ocon));
	}
#endif

	/*
	 * If possible, cache Transfer-Encoding: Chunked as unchunked.
	 */
	if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_METHOD_GET) && 
	    is_known_header_present(&ocon->phttp->hdr, MIME_HDR_TRANSFER_ENCODING)) {
		ret = get_known_header(&ocon->phttp->hdr, MIME_HDR_TRANSFER_ENCODING, 
				&chunk_encoding, &chunk_encoding_len, 
				&attrs, &hdrcnt);
		if ((ret == 0) && (hdrcnt == 1) && 
		    (strncasecmp(chunk_encoding, "chunked", 
		    		 chunk_encoding_len) == 0)) {
			if (ocon->oscfg.policies->ignore_s_maxage) {
				s_maxage_ignore = 1;
			}
			cache_object = 
				compute_object_expiration(
					&ocon->phttp->hdr, 
				        s_maxage_ignore,	
					ocon->request_send_time,
					ocon->request_response_time,
					nkn_cur_ts,
					&object_corrected_initial_age,
				      	&object_expire_time);
			// By default, cache responses with CC max-age=0/no-cache headers.
			if(!cache_object && 
			  (OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_NO_CACHE) || 
			   OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0))) {
				switch (ocon->oscfg.policies->cache_no_cache) {
				case NO_CACHE_FOLLOW:
					{
						if (!(OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_NO_STORE) ||
						      OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_PRIVATE))) {
							//Setup attr in order to always revalidate before serving to client.
							if(ocon->p_mm_task->in_attr) {
								pnknattr = (nkn_attr_t *)
									nkn_buffer_getcontent(ocon->p_mm_task->in_attr);
								pnknattr->na_flags |= NKN_PROXY_REVAL;
							}

							cache_object = 1;
						}
						break;
					}
				case NO_CACHE_OVERRIDE:
					{
						// max-age=0 + no-cache override case without any cache-age policies.
						if (OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0) &&
						   (object_expire_time <= nkn_cur_ts)) {
						    if(unsuccess_cache_response) {
							object_expire_time = nkn_cur_ts +
								unsuccessful_cache_age;
						    } else {
							object_expire_time = nkn_cur_ts +
								ocon->oscfg.policies->cache_age_default;
						    }
						}
						cache_object = 1;
						break;
					}
				case NO_CACHE_TUNNEL:
				default:
					break;
				}
			}

			if (cache_object == 0) {
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NON_CACHEABLE;
			}

			/* Apply crawler expiry if the object is crawled */
			if(ocon->p_mm_task->in_client_data.flags &
				NKN_CLIENT_CRAWL_REQ) {
			    ret = cim_get_params(ocon->uol.cod, &expiry,
						&cache_pin, &cim_flags);
			    if(!ret) {
				if (expiry != 0)
				    object_expire_time = expiry;
			    }
			}

			/* Apply unsuccessful cache response expiry */
			
			if (object_expire_time == NKN_EXPIRY_FOREVER) {
				if(unsuccess_cache_response) {
					object_expire_time = nkn_cur_ts +
						unsuccessful_cache_age;
				} else {
					object_expire_time = nkn_cur_ts +
						ocon->oscfg.policies->cache_age_default;
				}
			}

                        if (cache_object == 0) {
                                if (!CHECK_TR_RES_OVERRIDE(ocon->httpreqcon->http.nsconf->tunnel_res_override,
                                        NKN_TR_RES_OBJ_EXPIRED)) {
                                        cache_object = 0; //tunnel override has not been set, so tunnel it
                                        ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CHUNKED_EXP;
                                } else {
                                        int age_value = 0;
                                        cache_object = 1;//tunnel override set, cache it.
                                        // BZ 9566 - S
                                        if (2 == get_cache_age_policies (ocon->oscfg.policies, &ocon->phttp->hdr,
                                                        OM_CON_FD(ocon), &age_value))
                                        {
                                                DBG_LOG(MSG, MOD_OM,
                                                        "Tunnel override set to cache expired chunked object. Updating calculated "
                                                        "object expiry time[%ld] with default cache age policy value[%d]",
                                                        object_expire_time, age_value);
                                                        object_expire_time = nkn_cur_ts + age_value;
                                        }
                                        else
                                        // BZ 9566 - E
                                        {
                                                DBG_LOG(MSG, MOD_OM,
                                                        "Tunnel override set to cache expired chunked object. Updating calculated "
                                                        "object expiry time[%ld] with default cache age value[%d]",
                                                        object_expire_time, ocon->oscfg.policies->cache_age_default);
                                                        object_expire_time = nkn_cur_ts + ocon->oscfg.policies->cache_age_default;
                                        }
                                }
                        }

                        if (cache_object &&
                                (object_expire_time <= (nkn_cur_ts + ocon->oscfg.nsc->http_config->policies.store_cache_min_age))) {
                                        cache_object = 0; //tunnel override has not been set, so tunnel it
                                        ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CHUNKED_EXP;
                        }

			if (cache_object) {
				ret = get_cooked_known_header_data(&ocon->phttp->hdr,
							    MIME_HDR_CACHE_CONTROL,
							    &pckd, &datalen, 
							    &dtype);
			if (!ret && (dtype == DT_CACHECONTROL_ENUM) ) {

			    if (pckd->u.dt_cachecontrol_enum.mask & CT_MASK_NO_TRANSFORM &&
			    	!CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT)) {
				DBG_LOG(MSG, MOD_OM, 
					"Unchunk and cache disabled for uri=%s "
					"cache_control: 0x%x", 
					nkn_cod_get_cnp(ocon->uol.cod),
					pckd->u.dt_cachecontrol_enum.mask);
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CC_NO_TRANSFORM;
				if (!CHECK_TR_RES_OVERRIDE(ocon->httpreqcon->http.nsconf->tunnel_res_override, NKN_TR_RES_CC_NO_TRANSFORM)) {
					cache_object = 0; //tunnel override has not been set, so tunnel it
				}
				else {
					cache_object = 1; //tunnel override set, cache it
				}
			    }
			    /* If origin sends Cache-Control: private and
			     * Set-Cookie caching is enabled , object will
			     * not be cached 
			     */

		    	    if (cache_object && ocon->oscfg.policies->is_set_cookie_cacheable &&
					pckd->u.dt_cachecontrol_enum.mask & CT_MASK_PRIVATE &&
					!CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT)) {
	
			        	DBG_LOG(MSG, MOD_OM,
					"Cache-Control: private present, caching disbales"
					"uri=%s fd=%d",
					nkn_cod_get_cnp(ocon->uol.cod),
					OM_CON_FD(ocon));
					ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CC_PRIVATE;
                                        cache_object = 0;

			    }

			}
			}

			if (cache_object && is_cookie_tunnel) {
				DBG_LOG(MSG, MOD_OM, 
					"Set-Cookie present, caching disabled "
					"uri=%s fd=%d", 
					nkn_cod_get_cnp(ocon->uol.cod), 
					OM_CON_FD(ocon));
				glob_om_nocache_set_cookie++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SET_COOKIE;
                                cache_object = 0;
			}

			if (!IS_PSEUDO_FD(ocon->httpfd) && 
				(cache_object || 
				(ocon->oscfg.policies->cache_no_cache == NO_CACHE_OVERRIDE)) &&
				CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ))
			{
				// For chunked response cases, object based cache-index is not applicable
				// and under such scenarios we tunnel the response.
				if (ocon->oscfg.nsc->http_config->cache_index.include_object) {
					DBG_LOG(MSG, MOD_OM,
						"Chunked response received and response data based cache-index set. "
						"Tunnelling chunked repsonse, uri=%s, fd=%d",
						nkn_cod_get_cnp(ocon->uol.cod), OM_CON_FD(ocon));
					glob_tunnel_resp_cache_index_chunked_no_uri++;
					ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CACHE_IDX_OBJ_CKSUM_CHUNKED;
					goto tunnel_response;
				}

				// Construct the response header based cache index.
				// Sanity check on the URI length.

				if (get_known_header(reqhdr, MIME_HDR_X_NKN_DECODED_URI,
					&uri, &urilen, &attrs, &hdrcnt)) {
					if (get_known_header(reqhdr, MIME_HDR_X_NKN_URI,
							&uri, &urilen, &attrs, &hdrcnt)) {
						DBG_LOG(MSG, MOD_OM,
							"No URI found, uri=%p", uri);
						glob_tunnel_resp_cache_index_chunked_no_uri++;
						goto tunnel_response;
					}
				}

				// Derive the response header based cache index.
				ret = resp_header_cache_index(ocon);

				// Response header not present or unable to fetch response header value.
				// In such cases, tunnel the response.
				if (ret != 0) {
					glob_tunnel_resp_cache_index_no_header++;
					ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CACHE_IDX_NO_HEADER;
					goto tunnel_response;
				}
				SET_HTTP_FLAG(&ocon->httpreqcon->http, HRF_RESP_INDEX_CHUNK_REFETCH);
				ocon->p_mm_task->err_code = NKN_OM_CONF_REFETCH_REQUEST;
				UNSET_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ);
				return -1; //Close the socket	
			}

			if(!IS_PSEUDO_FD(ocon->httpfd)) {
				if ((cache_object) ||
					((ocon->oscfg.policies->cache_no_cache == NO_CACHE_OVERRIDE) && 
						!is_cookie_tunnel) ||
					CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT)) {
					goto create_am_entry;
				} else {
					goto tunnel_response;
				}
			}

			/* Expiry checks and size based checks should be done
			 * only in MM, so that the object can be made
			 * available in RAM cache in case if the checks fail
			 */

			if ((cache_object) ||
			    (ocon->oscfg.policies->cache_no_cache == NO_CACHE_OVERRIDE) ||
			    CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT)) {
				ocon->unchunked_obj_expire_time =
					object_expire_time;
				ocon->unchunked_obj_corrected_initial_age =
					object_corrected_initial_age;
				if(IS_PSEUDO_FD(ocon->httpfd)) {
				    OM_SET_FLAG(ocon, OMF_UNCHUNK_CACHE);
				    DBG_LOG(MSG, MOD_OM,
					"Cache unchunked data for uri=%s "
					"fd=%d",
					nkn_cod_get_cnp(ocon->uol.cod),
					OM_CON_FD(ocon));
				    glob_om_unchunk_cache++;
				} else {
				    DBG_LOG(MSG, MOD_OM, 
					    "Simultaneous chunk object limit "
					    "exceeded for uri=%s "
					    "fd=%d",
					    nkn_cod_get_cnp(ocon->uol.cod), 
					    OM_CON_FD(ocon));
				    glob_om_unchunk_simul_objs_cnt++;
				}
			}

			// Save state for validate
			ret = save_request_validate_state(reqhdr, 
							  &ocon->phttp->hdr,
							  ocon->oscfg.nsc);
			if (ret) {
				DBG_LOG(MSG, MOD_OM, 
					"save_request_validate_state() failed, "
					"ret=%d", ret);
				glob_tunnel_res_parse_err_10++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SAVE_VAL_ST_ERR;
				goto tunnel_response;
			}

			if (IS_PSEUDO_FD(ocon->httpfd)) {
				if ((set_cookie || set_cookie2) &&
					(ocon->oscfg.policies->is_set_cookie_cacheable == 1)) {
					if (set_cookie) {
						delete_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE);
					}
					if (set_cookie2) {
						delete_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2);
					}
					SET_HTTP_FLAG(&ocon->httpreqcon->http, HRF_CACHE_COOKIE);
				}
			}

			ret = save_mime_header_t(ocon);
			if (ret) {
				DBG_LOG(MSG, MOD_OM, 
				    "save_mime_header_t() failed, rv=%d", ret);
				glob_tunnel_res_parse_err_9++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SAVE_MIME_HDR_ERR;
                                goto tunnel_response;
			}

			ocon->unchunked_obj_objv = objv;
			if(IS_PSEUDO_FD(ocon->httpfd)) {
			// Serialize ocon mime_header_t into attribute buffer
			    ret = mime_header2nkn_buf_attr(&ocon->phttp->hdr, 0,
						    ocon->p_mm_task->in_attr,
						    nkn_buffer_set_attrbufsize,
						    nkn_buffer_getcontent);
			    if (ret) {
				    DBG_LOG(MSG, MOD_OM,
					    "mime_header2nkn_buf_attr(mime_header_t) "
					    "failed rv=%d", ret);
				    glob_tunnel_res_parse_err_8++;
				    ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_MIME_HDR_TO_BUF_ERR;
                                    goto tunnel_response;
			    }

			    pnknattr = (nkn_attr_t *)
				    nkn_buffer_getcontent(
					    ocon->p_mm_task->in_attr);

			    if(ocon->p_mm_task->in_client_data.flags &
						    NKN_CLIENT_CRAWL_REQ) {
				pnknattr->origin_cache_create =
				    ocon->request_response_time;
			    } else {
				/* response time - corrected age provides
				 * origin cache create time
				 */
				pnknattr->origin_cache_create =
					ocon->request_response_time -
					object_corrected_initial_age;
				pnknattr->cache_correctedage =
					object_corrected_initial_age;
			    }
			    pnknattr->cache_expiry = object_expire_time;
			    pnknattr->content_length = ocon->content_length;
			    pnknattr->obj_version = objv;
			    pnknattr->encoding_type = ocon->phttp->http_encoding_type;
			    /* back up for continuous request */
			    ocon->httpreqcon->http.cache_encoding_type = pnknattr->encoding_type;
                            /* Get the pinned header from the namespace config
                             * and check if it is present in the response header
                             */
                            ret = mime_hdr_is_unknown_present(&ocon->phttp->hdr,
                                                 ocon->oscfg.nsc->cache_pin_config->pin_header,
                                                 ocon->oscfg.nsc->cache_pin_config->pin_header_len);
                            if (ret && ocon->oscfg.nsc->cache_pin_config->enable) {
                                 /* Object can be pinned */
                                 nkn_attr_set_cache_pin(pnknattr);
                            }
			} else {
			    pnknattr = (nkn_attr_t *)
				nkn_buffer_getcontent(ocon->p_mm_task->in_attr);
			    nkn_attr_set_streaming(pnknattr);
			}
			glob_om_no_cache_chunked++;
		} else {
			glob_om_no_cache_tran_enc++;
		}

		if (!ocon->oscfg.policies->cache_cookie_noreval &&
			CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_CACHE_COOKIE) &&
			pnknattr) {
			pnknattr->na_flags |= NKN_PROXY_REVAL;
		}

		DBG_LOG(MSG, MOD_OM, "Tunnel chunk response, fd=%d", 
			OM_CON_FD(ocon));
		//printf("httpres: chunked encoding: len=%d\n", len);
		setup_tunnel_response(ocon);
		/*
		 * BUG: assume that all HTTP response header has been copied.
		 * We should check how many bytes have been copied.
		 */
		switch(om_makecopy_to_bufmgr(ocon, &len)) {
		case MAKECOPY_NO_TASK:
		case MAKECOPY_NO_DATA:
		case MAKECOPY_NO_IOVECS:
			NKN_ASSERT(0);
			return -1;
		case MAKECOPY_IOVECS_FULL:
			DBG_LOG(MSG, MOD_OM, 
				"Too big HTTP response header fd=%d", OM_CON_FD(ocon));
			return -1;
		case MAKECOPY_CL_EOT:
		case MAKECOPY_CHUNKED_EOT:
		case MAKECOPY_HEAD_EOT:
			return -1;
		case MAKECOPY_MORE_DATA:
			setup_read_timer(ocon);
			return 0;
		default:
			assert(0);
		}
	}
	
	// Determine entity length

	if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_CONTENT_RANGE)) {
		ret = get_cooked_known_header_data(&ocon->phttp->hdr, 
						MIME_HDR_CONTENT_RANGE, &pckd,
						&datalen, &dtype);
		if (ret || dtype != DT_CONTENTRANGE) {
			DBG_LOG(MSG, MOD_OM, 
				"get_cooked_known_header_data(CONTENT_RANGE) "
				"failed, ret=%d dtype=%d", ret, dtype);
			glob_tunnel_res_parse_err_2++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CONT_RANGE_ERR;
			goto tunnel_response;
		}

		if (!pckd->u.dt_contentrange.entity_length ||
		    !pckd->u.dt_contentrange.length ||
		    (pckd->u.dt_contentrange.length > 
		    	pckd->u.dt_contentrange.entity_length)) {
			DBG_LOG(MSG, MOD_OM, 
				"Invalid Content-Range data "
				"range_off=%ld range_len=%ld "
				"content_length=%ld",
				pckd->u.dt_contentrange.offset,
				pckd->u.dt_contentrange.length,
				pckd->u.dt_contentrange.entity_length);
			glob_tunnel_res_parse_err_3++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_INVALID_CONT_RANGE;
			goto tunnel_response;
		}

		if(ocon->oscfg.policies && 
		   ((ocon->oscfg.policies->cache_max_size && 
			(pckd->u.dt_contentrange.entity_length > ocon->oscfg.policies->cache_max_size)) ||
			(ocon->oscfg.policies->cache_min_size &&
			(pckd->u.dt_contentrange.entity_length < ocon->oscfg.policies->cache_min_size)))) {
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CACHE_SIZE_LIMIT;
			goto tunnel_response;
		}
		
		ocon->range_offset = pckd->u.dt_contentrange.offset;
		ocon->range_length = pckd->u.dt_contentrange.length;

		ocon->content_length = pckd->u.dt_contentrange.entity_length;
	} else {
		ret = get_cooked_known_header_data(&ocon->phttp->hdr, 
						MIME_HDR_CONTENT_LENGTH, &pckd,
						&datalen, &dtype);
		if (ret || (dtype != DT_SIZE)) {
			DBG_LOG(MSG, MOD_OM, 
				"get_cooked_known_header_data(CONTENT_LENGTH) "
				"failed, ret=%d dtype=%d, tunnel response", 
				ret, dtype);
			/* If Content-length not present, we do not cache. */
			glob_om_no_cache_no_content_len++;
			glob_tunnel_res_no_content_len++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NO_CONT_LEN;
			goto tunnel_response;
		}
		ocon->content_length = pckd->u.dt_size.ll;

		ocon->range_offset = 0;
		ocon->range_length = 0;
	}

	// Make sure received OS data offset agrees with the 
	// request offset (uol.offset), if not terminate and close
	// user connnection as this should never happen.

	if (!CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) && ocon->range_offset != ocon->uol.offset) {
	    DBG_LOG(MSG, MOD_OM, 
	    	    "ocon->range_offset(%ld) != ocon->uol.offset(%ld)",
		    ocon->range_offset, ocon->uol.offset);
		glob_tunnel_res_parse_internal++;
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_RANGE_OFFSET_ERR;
		goto tunnel_response;
	}

	// Determine if object can be cached

	if (get_known_header(reqhdr, MIME_HDR_X_NKN_DECODED_URI,
			&uri, &urilen, &attrs, &hdrcnt)) {
		if (get_known_header(reqhdr, MIME_HDR_X_NKN_URI, 
			&uri, &urilen, &attrs, &hdrcnt)) {
			DBG_LOG(MSG, MOD_OM, 
				"No URI found, uri=%p", uri);
		}
	}
        if (ocon->oscfg.policies->ignore_s_maxage) {
        	s_maxage_ignore = 1;
        }
	cache_object = compute_object_expiration(&ocon->phttp->hdr, 
				s_maxage_ignore,
				ocon->request_send_time,
				ocon->request_response_time,
				nkn_cur_ts,
				&object_corrected_initial_age,
				&object_expire_time);

	// By default, cache responses with CC max-age=0/no-cache headers.
	if(!cache_object &&
	  (OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_NO_CACHE) ||
	   OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0))) {
		switch (ocon->oscfg.policies->cache_no_cache) {
		case NO_CACHE_FOLLOW:
			{
				if (!(OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_NO_STORE) ||
				      OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_PRIVATE))) {
					//Setup attr in order to always revalidate before serving to client.
					if(ocon->p_mm_task->in_attr) {
						pnknattr = (nkn_attr_t *)
								nkn_buffer_getcontent(ocon->p_mm_task->in_attr);
						pnknattr->na_flags |= NKN_PROXY_REVAL;
					}

					cache_object = 1;
				}
				break;
			}
		case NO_CACHE_OVERRIDE:
			{
				// max-age=0 + no-cache override case without any cache-age policies.
				if (OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0) && 
				   (object_expire_time <= nkn_cur_ts)) {
				    if(unsuccess_cache_response) {
		    			object_expire_time = nkn_cur_ts +
						unsuccessful_cache_age;
				    } else {
					object_expire_time = nkn_cur_ts + 
						ocon->oscfg.policies->cache_age_default;
				    }
				}
				cache_object = 1;
				break;
			}
		case NO_CACHE_TUNNEL:
		default:
			break;
		}
	}

	if (cache_object == 0) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NON_CACHEABLE;
	}

	/* Apply crawler expiry if the object is crawled */
	if(ocon->p_mm_task->in_client_data.flags & NKN_CLIENT_CRAWL_REQ) {
	    ret = cim_get_params(ocon->uol.cod, &expiry, &cache_pin,
				&cim_flags);
	    if(!ret) {
		if (expiry != 0)
		    object_expire_time = expiry;
	    }
	}

	if (object_expire_time == NKN_EXPIRY_FOREVER) {
		/* If there is query string, and no expiry/max-age headers
		 * set expire time to current so that the object won't be cached.
		 * Temp. fix for now. This object needs to be marked for revalidation
		 * for every request.
		 * BZ 3456.
		 * Bypass this fix if vitual player is configured. BZ 3963.
		 */
		if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_HAS_QUERY) &&
				!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_SSP_CONFIGURED) &&
				(CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT) == 0) && 
				(ocon->oscfg.policies->disable_cache_on_query == NKN_TRUE)) {
			object_expire_time = nkn_cur_ts;
		}
		else {
		    if(unsuccess_cache_response) {
			object_expire_time = nkn_cur_ts +
				unsuccessful_cache_age;
		    } else {
			object_expire_time = nkn_cur_ts +
				ocon->oscfg.policies->cache_age_default;
		    }
		}
	}

	if (object_expire_time <= nkn_cur_ts) {
		/* Cache objects with Set-Cookie + max-age=0 and cache cookie policy set.
		 * Revalidate the cached object before returning to client 
		 */
		if (ocon->oscfg.policies->is_set_cookie_cacheable &&
			(set_cookie || set_cookie2)) {
			if (is_known_header_present(&ocon->phttp->hdr, 
					    MIME_HDR_X_ACCEL_CACHE_CONTROL)) {
				ret = get_cooked_known_header_data(&ocon->phttp->hdr,
								MIME_HDR_X_ACCEL_CACHE_CONTROL,
								&pckd, &datalen, 
								&dtype);
			} else {
				ret = get_cooked_known_header_data(&ocon->phttp->hdr,
								MIME_HDR_CACHE_CONTROL,
								&pckd, &datalen, 
								&dtype);
			}
			if (!ret && (dtype == DT_CACHECONTROL_ENUM) && 
				(pckd->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) && 
				     (pckd->u.dt_cachecontrol_enum.max_age == 0))  {
					if (ocon->p_mm_task->in_attr) {
						pnknattr = (nkn_attr_t *)
								nkn_buffer_getcontent(ocon->p_mm_task->in_attr);
						pnknattr->na_flags |= NKN_PROXY_REVAL;
					}
					cache_object =1;
					goto skip_exp_check;
			}
		}
		DBG_LOG(MSG, MOD_OM, 
			"Object already expired, caching disabled "
			"uri=%s fd=%d", 
			nkn_cod_get_cnp(ocon->uol.cod), OM_CON_FD(ocon));

		if ((ocon->oscfg.policies->cache_no_cache == NO_CACHE_TUNNEL) ||
		    !(OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0) ||
			OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_NO_CACHE))) {
			DBG_LOG(MSG, MOD_OM,
				"Object already expired, caching disabled "
				"uri=%s fd=%d",
				nkn_cod_get_cnp(ocon->uol.cod), OM_CON_FD(ocon));
			glob_om_nocache_already_expired++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_OBJ_EXPIRED;

			if (!CHECK_TR_RES_OVERRIDE(ocon->httpreqcon->http.nsconf->tunnel_res_override, 
				NKN_TR_RES_OBJ_EXPIRED)) {
				cache_object = 0; //tunnel override has not been set, so tunnel it
			} else {
				int age_value = 0;
				cache_object = 1;//tunnel override set, cache it.
				// BZ 9566 - S
				if (2 == get_cache_age_policies (ocon->oscfg.policies, &ocon->phttp->hdr, 
					OM_CON_FD(ocon), &age_value))
				{
					DBG_LOG(MSG, MOD_OM, 
						"Tunnel override set to cache expired object. Updating calculated "
						"object expiry time[%ld] with default cache age policy value[%d]",
						object_expire_time, age_value); 
					object_expire_time = nkn_cur_ts + age_value;
				}
				else
				// BZ 9566 - E
				{
					DBG_LOG(MSG, MOD_OM,
						"Tunnel override set to cache expired object. Updating calculated "
						"object expiry time[%ld] with default cache age value[%d]",
						object_expire_time, ocon->oscfg.policies->cache_age_default); 
					object_expire_time = nkn_cur_ts + ocon->oscfg.policies->cache_age_default;
				}
			}
		} else {
			    DBG_LOG(MSG, MOD_OM,
				"CC: max-age=0 and/or no-cache header present. By default, cache the object although "
				"it's expired object - expiry time[%ld], fd=%d, ocon=%p",
				object_expire_time, ocon->fd, ocon);
		}

skip_exp_check:
 		if ((ocon->oscfg.policies->cache_no_cache == NO_CACHE_TUNNEL) ||
		    !(OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_MAX_AGE_0) ||
		      OM_CHECK_RESP_MASK(ocon, OM_RESP_CC_NO_CACHE))) {
			DBG_LOG(MSG, MOD_OM, 
				"Object already expired, caching disabled "
				"uri=%s fd=%d", 
				nkn_cod_get_cnp(ocon->uol.cod), OM_CON_FD(ocon));
			glob_om_nocache_already_expired++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_OBJ_EXPIRED;
			if (!CHECK_TR_RES_OVERRIDE(ocon->httpreqcon->http.nsconf->tunnel_res_override, NKN_TR_RES_OBJ_EXPIRED)) {
				cache_object = 0; //tunnel override has not been set, so tunnel it
			}
		}
	}

	if ((set_cookie || set_cookie2) &&
	    !ocon->oscfg.policies->is_set_cookie_cacheable &&
	    !CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT)) {
		is_cookie_tunnel = 1;
	}

	if (cache_object && is_cookie_tunnel) {
		DBG_LOG(MSG, MOD_OM, 
			"Set-Cookie present, caching disabled "
			"uri=%s fd=%d", 
			nkn_cod_get_cnp(ocon->uol.cod), OM_CON_FD(ocon));
		glob_om_nocache_set_cookie++;
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SET_COOKIE;
		cache_object = 0;
	}

	if ((ocon->oscfg.policies->cache_no_cache == NO_CACHE_OVERRIDE) && 
	    !is_cookie_tunnel) {
		cache_object = 1;
	}

	if (cache_object) {
		do_ingest = 1;
	}

        if(unsuccess_cache_response &&
                (object_expire_time > (nkn_cur_ts + unsuccessful_cache_age))) {
            object_expire_time = nkn_cur_ts + unsuccessful_cache_age;
        }

	object_has_min_expire_time  = ((object_expire_time - nkn_cur_ts) > 
	    		    ocon->oscfg.policies->store_cache_min_age);

	object_size_cacheable = (ocon->content_length >= 
	    		ocon->oscfg.policies->store_cache_min_size);

	/* 
	 * Policy Engine set action "cache_object" overrides other logic
	 */
	if (CHECK_ACTION(ocon,PE_ACTION_CACHE_OBJECT)) {
		cache_object = 1;
		do_ingest = 1;
		object_has_min_expire_time = 1;
		object_size_cacheable = 1;
	}

	/* Special case to cache expired object */
	if (cache_object &&
	    !ocon->oscfg.policies->store_cache_min_age) {
		do_ingest = 1;
		object_has_min_expire_time = 1;
	}

	if (do_ingest && 
	    object_has_min_expire_time && object_size_cacheable) {

#if 0
	     	/* Inline vs offline ingest */
		if (ocon->oscfg.policies->offline_om_fetch_filesize && 
		    (ocon->content_length > 
		    	ocon->oscfg.policies->offline_om_fetch_filesize)) {
	    		if (!IS_PSEUDO_FD(ocon->httpreqcon->fd) && 
			    ((CHECK_HTTP_FLAG(&ocon->httpreqcon->http, 
						HRF_BYTE_RANGE) &&
			    !ocon->httpreqcon->http.brstart) ||
	    		    (!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, 
			    			HRF_BYTE_RANGE) &&
			    !ocon->uol.offset))) {
	    			submit_oomgr_request(ocon);
	    		}
		} else {
			OM_SET_FLAG(ocon, OMF_INLINE_INGEST);
		}
#endif
	}
	else {
		if (do_ingest) { 
			if (!object_has_min_expire_time) {
				// Only add to BM due to low expiry
				glob_om_no_cache_expired++; 
				ocon->p_mm_task->out_flags |= MM_OFLAG_NOAM_HITS;
				nkn_am_log_error(nkn_cod_get_cnp(ocon->uol.cod),
						 NKN_MM_CONF_OBJECT_EXPIRED);
			} else if (!object_size_cacheable) {
				// Only add to BM due to size
				glob_om_no_cache_small_object++; 
                                ocon->p_mm_task->out_flags |= MM_OFLAG_NOAM_HITS;
				nkn_am_log_error(nkn_cod_get_cnp(ocon->uol.cod),
						 NKN_MM_CONF_SMALL_OBJECT_SIZE);
			}
		}
		else { 
			glob_om_no_cache_am_no_ingest++; 
		}
		if (!cache_object) {
			//flag OMF_NO_CACHE_BM_ONLY: deprecated
			//OM_SET_FLAG(ocon, OMF_NO_CACHE|OMF_NO_CACHE_BM_ONLY);
			glob_tunnel_res_no_cache++;
			goto tunnel_response;
		}
	}

	if (CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ)) {

		// Check for presence of range header. If so it should be cover
		// first n configured bytes for cache index calculation. If not
		// tunnel the request here itself.
		/*
		if (ocon->oscfg.nsc->http_config->cache_index.include_object &&
				CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE)) {
			uint32_t range_start, range_stop;
			range_start = ocon->httpreqcon->http.brstart;
			range_stop = ocon->httpreqcon->http.brstop;

			if ((range_start) || 
				(range_stop < ocon->oscfg.nsc->http_config->cache_index.checksum_bytes)) {
				DBG_LOG(MSG, MOD_OM,
					"Response based cache index has range outside first configured %d bytes",
					ocon->oscfg.nsc->http_config->cache_index.checksum_bytes);
				glob_tunnel_resp_cache_index_out_of_range_data++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CACHE_IDX_OUT_OF_RANGE_DATA;	
				goto tunnel_response;
			}
		}
		*/
		if (ocon->oscfg.nsc->http_config->cache_index.include_header &&
			(!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_TIER2_MODE))) {

			// Derive the response header based cache index.
			ret = resp_header_cache_index(ocon);

			// Response header not present or unable to fetch response header value.
			// In such cases, tunnel the response.
			if (ret != 0) {
				glob_tunnel_resp_cache_index_no_header++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_CACHE_IDX_NO_HEADER;	
				goto tunnel_response;
			}

			// Check if response object based cache indexing also needs to be done.
			if (ocon->oscfg.nsc->http_config->cache_index.include_object) {
				goto finish;
			}
			ocon->p_mm_task->err_code = NKN_OM_CONF_REFETCH_REQUEST;
			UNSET_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ);
			return -1; //Close the socket
		} else if (ocon->oscfg.nsc->http_config->cache_index.include_object) {
			goto finish;
		}
	}

	// Build cache attribute data

	if (ocon->p_mm_task->in_attr) {
		const char *cookie_str;
		int cookie_len, rv;
		u_int32_t attr;
		int hdrcnt1, nth;

		if (ocon->range_length) {
			char numbuf[64];
			delete_known_header(&ocon->phttp->hdr, 
					    MIME_HDR_CONTENT_RANGE);
			snprintf(numbuf, sizeof(numbuf), "%ld", 
				ocon->content_length);

			// Replace with entity content length
			add_known_header(&ocon->phttp->hdr, MIME_HDR_CONTENT_LENGTH, 
					numbuf, strlen(numbuf));
		}
		mime_hdr_remove_hop2hophdrs(&ocon->phttp->hdr);


		if (!get_known_header(reqhdr, MIME_HDR_X_NKN_REMAPPED_URI,
			&uri, &urilen, &attrs, &hdrcnt)) {
		    if (!get_known_header(reqhdr, MIME_HDR_X_NKN_URI,
			    &uri, &urilen, &attrs, &hdrcnt)) {
			add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_URI,
						uri, urilen);
		    }
		}

		// Note query string
		ret = get_namevalue_header(&ocon->phttp->hdr, 
				http_known_headers[MIME_HDR_X_NKN_QUERY].name,
				http_known_headers[MIME_HDR_X_NKN_QUERY].namelen,
				&querystr, &datalen, &attrs);
		if (!ret) {
			//
			// Request requires the query string, note it in the
			// attributes for future use in case we need to
			// do a validate (i.e. If-Modified-Since)
			//
    	    		ret = add_known_header(&ocon->phttp->hdr, 
					       MIME_HDR_X_NKN_QUERY,
					       querystr, datalen);
			if (ret) {
				DBG_LOG(MSG, MOD_OM, 
					"add_known_header() failed rv=%d, "
					"MIME_HDR_X_NKN_QUERY datalen=%d", 
					ret, datalen);
				glob_tunnel_res_parse_err_5++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_ADD_NKN_QUERY_ERR;
				goto tunnel_response;
			}

			delete_namevalue_header(&ocon->phttp->hdr,
				http_known_headers[MIME_HDR_X_NKN_QUERY].name,
				http_known_headers[MIME_HDR_X_NKN_QUERY].namelen);
		}

		// Vary header handling
		// Check if VARY header handling is enabled
		// Check if VARY header is present
		// Set Vary attribute only for User-Agent and Accept-Encoding
		//   for other headers, tunnel the request.
		if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY)) {
			const char *hdr_value = 0;
			int hdr_len;
			//u_int32_t attrs;
			//int hdrcnt;

			// if Vary header handeling is not enabled, tunnel response
#if 1
			if (ocon->httpreqcon->http.nsconf->http_config->vary_hdr_cfg.enable == 0) { 
				DBG_LOG(MSG, MOD_OM,
					"Vary header handling not enabled");
				glob_tunnel_resp_vary_handling_disabled++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_VARY_DISABLE;
				goto tunnel_response;
			}
#endif			
			// If other headers are indicated in vary header, tunnel the response
			if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY_OTHERS)) { 
				DBG_LOG(MSG, MOD_OM,
					"Un Supported vary header field value");
				glob_tunnel_resp_vary_field_not_supported++;
				ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NOT_SUPPORTED_VARY;
				goto tunnel_response;
			}
			// Check if User Agent is indicated in the VARY header.
			if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY_USR_AGNT)) { 
				// If so get the User Agent header from the request and
				// store it in the response header
				if (!get_known_header(reqhdr, MIME_HDR_USER_AGENT,
							&hdr_value, &hdr_len, &attrs, &hdrcnt)) {
					add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_USER_AGENT,
								hdr_value, hdr_len);
				}
				
				// Set VARY flag in attributes
				vary_hdr = 1;
				glob_om_vary_hdr_user_agent++;
			}
			// Check if Accept-Encoding is indicated in the VARY header.
			if (CHECK_HTTP_FLAG2(ocon->phttp, HRF2_HAS_VARY_ACCPT_ENCD)) { 
				if (is_known_header_present(&ocon->phttp->hdr, MIME_HDR_CONTENT_ENCODING)
						&& (ocon->phttp->http_encoding_type != HRF_ENCODING_UNKNOWN)) {
					vary_encoding = 1;
				}
				// Set VARY flag in attributes
				glob_om_vary_hdr_accept_encoding++;
				vary_hdr = 1;
			}
		}
		
		// Save state for validate
		ret = save_request_validate_state(reqhdr, &ocon->phttp->hdr,
						  ocon->oscfg.nsc);
		if (ret) {
			DBG_LOG(MSG, MOD_OM,
				"save_request_validate_state() failed, "
				"ret=%d", ret);
			glob_tunnel_res_parse_err_6++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SAVE_VAL_ST_ERR;
			goto tunnel_response;
		}

		/*
		 * Special case for cookie handling. If set cookie cacheable, then we've to make a copy of 
		 * cookie headers into client side response hdrs followed by stripping the respective cookie 
		 * headers from the origin response. This is done here in order to make the cached object 
		 * to be cookie agnostic.
		 */

		if ((set_cookie || set_cookie2) && 
			(ocon->oscfg.policies->is_set_cookie_cacheable == 1)) {
            delete_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_SET_COOKIE);
            delete_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_SET_COOKIE2);

			if (set_cookie) {
				mime_hdr_get_known(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE, &cookie_str, &cookie_len, &attr, &hdrcnt1);
				for(nth = 0; nth < hdrcnt1; nth++) {
					rv = get_nth_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE, nth, &cookie_str, &cookie_len, &attrs);
					if (!rv) {
						add_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_SET_COOKIE, cookie_str, cookie_len);
					} else {
						DBG_LOG(MSG, MOD_OM, 
								"get_nth_known_header() failed rv=%d nth=%d,MIME_HDR_SET_COOKIE ", 
								rv, nth); 
					}
				}
				delete_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE);
			} 
			if (set_cookie2) {
				mime_hdr_get_known(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2, &cookie_str, &cookie_len, &attr, &hdrcnt1);
				for(nth = 0; nth < hdrcnt1; nth++) {
					rv = get_nth_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2, nth, &cookie_str, &cookie_len, &attrs);
					if (!rv) {
						add_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_SET_COOKIE2, cookie_str, cookie_len);
					} else {
						DBG_LOG(MSG, MOD_OM,
								"get_nth_known_header() failed rv=%d nth=%d,MIME_HDR_SET_COOKIE ",
								rv, nth);
					}
				}
				delete_known_header(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2);
			}
			SET_HTTP_FLAG(&ocon->httpreqcon->http, HRF_CACHE_COOKIE);
		}

		// Serialize ocon mime_header_t into attribute buffer
		ret = mime_header2nkn_buf_attr(&ocon->phttp->hdr, 0,
					       ocon->p_mm_task->in_attr,
					       nkn_buffer_set_attrbufsize,
					       nkn_buffer_getcontent);
		if (ret) {
			DBG_LOG(MSG, MOD_OM,
				"mime_header2nkn_buf_attr(mime_header_t) "
				"failed rv=%d", ret);
			glob_tunnel_res_parse_err_8++;
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_MIME_HDR_TO_BUF_ERR;
			goto tunnel_response;
		}
		pnknattr = (nkn_attr_t *)
			nkn_buffer_getcontent(ocon->p_mm_task->in_attr);
	
		if(ocon->p_mm_task->in_client_data.flags & NKN_CLIENT_CRAWL_REQ) {
		    pnknattr->origin_cache_create = ocon->request_response_time;
		} else {
		    pnknattr->origin_cache_create =
			    ocon->request_response_time -
			    object_corrected_initial_age;
		    pnknattr->cache_correctedage = object_corrected_initial_age;
		}
		pnknattr->cache_expiry = object_expire_time;
		if (http_byte_range_streaming) {
			nkn_attr_set_streaming(pnknattr);
			ocon->content_length = OM_STAT_SIG_TOT_CONTENT_LEN;
		} else {
			pnknattr->content_length = ocon->content_length;
		}

		if (unsuccess_cache_response) {
			pnknattr->na_respcode = ocon->phttp->respcode;
                        ocon->p_mm_task->out_flags |= MM_OFLAG_NOAM_HITS;
                        nkn_attr_set_negcache(pnknattr);
                }

		if(ocon->comp_status == 1) {
			ocon->content_length = OM_STAT_SIG_TOT_CONTENT_LEN;
			nkn_attr_set_streaming(pnknattr);
		}

		pnknattr->obj_version = objv;
		pnknattr->encoding_type = ocon->phttp->http_encoding_type;
		ocon->httpreqcon->http.cache_encoding_type = pnknattr->encoding_type;

		// If HRF_CACHE_COOKIE flag is set, mark the object for forced revalidation.
		if (!ocon->oscfg.policies->cache_cookie_noreval && 
			CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_CACHE_COOKIE)) {
			pnknattr->na_flags |= NKN_PROXY_REVAL;
		}

		if (vary_hdr) {
			nkn_attr_set_vary(pnknattr);
		}
		if (vary_encoding) {
			//pnknattr->na_flags |= NKN_OBJ_COMPRESSED;
			pnknattr->encoding_type = ocon->phttp->http_encoding_type; 
		}

		/* Pin this object if required */
		// TODO: Cache Pinning

		/* Get the pinned header from the namespace config
		 * and check if it is present in the response header
		 */
		ret = mime_hdr_is_unknown_present(&ocon->phttp->hdr,
			ocon->oscfg.nsc->cache_pin_config->pin_header,
			ocon->oscfg.nsc->cache_pin_config->pin_header_len);
		if (ret && ocon->oscfg.nsc->cache_pin_config->enable) {
			/* Object can be pinned */
			nkn_attr_set_cache_pin(pnknattr);
		}

		/* Get the start validity time header value, convert to a
		 * time_t and set in the attributes
		 */
		if (ocon->oscfg.nsc->cache_pin_config->start_header_len) {
			const char *hdr_val = NULL;
			int hdr_val_len = 0;
			ret = mime_hdr_get_unknown(&ocon->phttp->hdr,
					ocon->oscfg.nsc->cache_pin_config->start_header,
					ocon->oscfg.nsc->cache_pin_config->start_header_len,
					&hdr_val, &hdr_val_len, &attrs);
			if (!ret) {
				/*
				 * Header found. convert the value to a time_t
				 * and set in the attributes
				 */
				time_t start_time = parse_date(hdr_val, (hdr_val + hdr_val_len));
				if (start_time) {
					pnknattr->cache_time0 = start_time;
				}
			}
		}

		/* If Set-Cookie caching is enabled from CLI 
		 * and origin-server sends "Cache-Control:
		 * proxy-revalidate, max-age=0", object will be cached
		 * but revalidated with IMS request before returning to
		 * client.
		 */
		if(ocon->oscfg.policies->is_set_cookie_cacheable && (set_cookie || set_cookie2)) {
			if (is_known_header_present(&ocon->phttp->hdr, 
					    MIME_HDR_X_ACCEL_CACHE_CONTROL)) {
				ret = get_cooked_known_header_data(&ocon->phttp->hdr,
								MIME_HDR_X_ACCEL_CACHE_CONTROL,
								&pckd, &datalen, 
								&dtype);
			} else {
				ret = get_cooked_known_header_data(&ocon->phttp->hdr,
								MIME_HDR_CACHE_CONTROL,
								&pckd, &datalen, 
								&dtype);
			}
			if (!ret && (dtype == DT_CACHECONTROL_ENUM)) {
				if ((pckd->u.dt_cachecontrol_enum.mask & CT_MASK_PROXY_REVALIDATE)
				 || (pckd->u.dt_cachecontrol_enum.mask & CT_MASK_MUST_REVALIDATE)
				 || ((pckd->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) && 
				     (pckd->u.dt_cachecontrol_enum.max_age == 0) )) {
					pnknattr->na_flags |= NKN_PROXY_REVAL;
				}
			}
		}

	}

finish:
	ocon->content_data_offset = ocon->range_offset;
	setup_read_timer(ocon);
	return 0;

create_am_entry:
	//uri = nkn_cod_get_cnp(ocon->uol.cod);
	//am_pk.name = (char *)uri;
	am_pk.provider_id = OriginMgr_provider;
	am_pk.key_len = 0;
	memset(&am_object_data, 0, sizeof(am_object_data));
	memcpy(&(am_object_data.client_ipv4v6), &(ocon->httpreqcon->src_ipv4v6),
		sizeof(ip_addr_t));
	am_object_data.client_port = ocon->httpreqcon->src_port;
	am_object_data.client_ip_family = ocon->httpreqcon->ip_family;
	memcpy(&(am_object_data.origin_ipv4v6), &(ocon->phttp->remote_ipv4v6),
		sizeof(ip_addr_t));
	am_object_data.origin_port = ocon->phttp->remote_port;
	am_object_data.proto_data = ocon->httpreqcon;
	am_object_data.encoding_type = ocon->httpreqcon->http.http_encoding_type;
	if (CHECK_ACTION(ocon->httpreqcon, PE_ACTION_SET_ORIGIN_SERVER) ||
			CHECK_CON_FLAG(ocon->httpreqcon, CONF_PE_HOST_HDR)) {
		ocon->httpreqcon->module_type = NORMALIZER;
	}
	am_object_data.flags = AM_OBJ_TYPE_STREAMING;
	am_object_data.ns_token = ocon->httpreqcon->http.ns_token;
	am_object_data.host_hdr = NULL;
	am_object_data.in_cod = ocon->uol.cod;

	if (http_byte_range_streaming) {
		glob_tunnel_http_byte_range_streaming++;
		am_object_data.flags |=  AM_NON_CHUNKED_STREAMING;
	} else if(compress_streaming == 1 || non_206_origin_streaming == 1) {
		am_object_data.flags |=  AM_NON_CHUNKED_STREAMING;
	} else {
		glob_tunnel_object_chunked++;
	}
	/* AM hit is not done for reval always */
	if (((ocon->oscfg.nsc->http_config->policies.reval_type != 1) ||
		(ocon->oscfg.nsc->http_config->policies.reval_barrier !=
			NS_REVAL_ALWAYS_TIME_BARRIER)) && 
			!(ocon->p_mm_task->in_flags & MM_FLAG_REQ_NO_CACHE)) {
		/* Do a hit count for AM */
		AM_inc_num_hits(&am_pk, 1, 0, NULL, &am_object_data, NULL);
	}
	
tunnel_response:
	if (CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ)) {
		ocon->p_mm_task->err_code = NKN_OM_NON_CACHEABLE;
		UNSET_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ);
		return -1; //Close the socket
	}
	
	OM_UNSET_FLAG(ocon, OMF_NEGATIVE_CACHING);
	ocon->p_mm_task->out_proto_resp.u.OM.incarn = gnm[OM_CON_FD(ocon)].incarn;
	ocon->p_mm_task->out_proto_resp.u.OM.cp_sockfd = OM_CON_FD(ocon);
	ocon->p_mm_task->out_proto_resp.u.OM.httpcon_sockfd = ocon->httpfd;
	if(!IS_PSEUDO_FD(ocon->httpfd)) {
	    OM_SET_FLAG(ocon, OMF_NO_CACHE);
	    NKN_MUTEX_LOCK(&omcon_mutex);
	    nhash_delete_omcon_by_uri(ocon);
	    NKN_MUTEX_UNLOCK(&omcon_mutex);
	}
	NM_del_event_epoll(OM_CON_FD(ocon));
	ocon->p_mm_task->err_code = NKN_OM_NON_CACHEABLE;
	//om_close_conn(ocon);
	return 1;
}

/*
 * After receive complete HTTP response, we are getting content data
 * We stay in this state until all content data has been received
 */
static int cp_callback_http_body_received(void *private_data,
					 char *p __attribute((unused)),
					 int * len,
					 int *shouldexit)
{
	omcon_t * ocon = (omcon_t *) private_data;
	cpcon_t * cpcon = NULL;
	con_t *httpreqcon = NULL;
	int ret;
	int iovec_full = 0;
	uint32_t resp_data_bytes;

	if (ocon->cpcon) {
		cpcon = ocon->cpcon;
		ocon->state = cpcon->state;
	}
	http_cb_t * phttp = ocon->phttp;

	if (!CHECK_HTTP_FLAG(phttp, HRF_PARSE_DONE)) {
		return 0;
	}
	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_HTTP_BODY_RECEIVED);

	/* OM continuously send data to BM */
	switch(ret = om_makecopy_to_bufmgr(ocon, len)) {
	case MAKECOPY_NO_TASK:
	case MAKECOPY_NO_IOVECS:
		break;
	case MAKECOPY_NO_DATA:
		assert(0);
	case MAKECOPY_IOVECS_FULL:
		// For MAKECOPY_IOVECS_FULL case, we need to fall through inorder to allow
		// to apply RBCI for more than 24KB data checksum cases.
		iovec_full = 1;
		break;
	case MAKECOPY_CL_EOT:
	case MAKECOPY_CHUNKED_EOT:
	case MAKECOPY_HEAD_EOT:
		ocon->p_mm_task->err_code = 0;
		*shouldexit = 1;
		break;
	case MAKECOPY_MORE_DATA:
		/* BUG 3471: comment out the next line since I got crash */
		//OM_get_ret_task(ocon);
		break;
	default:
		assert(0);
	}

	if (ocon->p_mm_task && cpcon) {
		httpreqcon = (con_t *)(ocon->p_mm_task->in_proto_data);
		if (httpreqcon) {
			if (CHECK_CON_FLAG(httpreqcon, CONF_REFETCH_REQ) &&
			    ocon->oscfg.nsc->http_config->cache_index.include_object &&
			    (!CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_TIER2_MODE))) {
				unsigned int cksum_bytes = ocon->oscfg.nsc->http_config->
								    cache_index.checksum_bytes;
				/*
				 * Wait till we receive configured data
				 * bytes for cache index calculation.
				 */
				resp_data_bytes = cpcon->http.tot_reslen;
				if (((ocon->content_length < cksum_bytes) &&
				     (resp_data_bytes >= ocon->content_length)) ||
				    (resp_data_bytes >= cksum_bytes)) {
					// Derive the response data based cache index.
					ret = resp_data_cache_index(ocon);
					ocon->p_mm_task->err_code = NKN_OM_CONF_REFETCH_REQUEST;
					UNSET_CON_FLAG(httpreqcon, CONF_REFETCH_REQ);
					SET_CPCON_FLAG(cpcon, CPF_REFETCH_CLOSE);
					*shouldexit = 1;
					return -1;
				}
			}
			if (CHECK_CON_FLAG(httpreqcon, CONF_RESUME_TASK)) {
				SET_CPCON_FLAG(cpcon, CPF_RESUME_TASK);
			}
		}
	}
	if (iovec_full) {
		cancel_timer(ocon);
		if((ocon->comp_status == 1) && 
		    (OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN))) {
		cp_add_event(OM_CON_FD(ocon), OP_EPOLLOUT, NULL);
		return 0;
	}

		OM_get_ret_task(ocon);
	}

	return 0;
}

/*
 * The transaction is completely done.
 */
static int cp_callback_close_socket(void * private_data, int close_httpconn);
static int cp_callback_close_socket(void *private_data, int close_httpconn)
{
	UNUSED_ARGUMENT(close_httpconn);
	omcon_t * ocon = (omcon_t *)private_data;
	int ret, orglen, errcode;
	int copylen = ocon->phttp->cb_totlen - ocon->phttp->cb_offsetlen;

	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_CLOSE_SOCKET);
	if (ocon->cpcon) ocon->state = ((cpcon_t *) ocon->cpcon)->state;

	/*
	 * To Be Done:
	 * not sure why we need to call om_makecopy_to_bufmgr at this time.
	 * cp_internal_close_conn() will always
	 * call cp_callback_http_body_received()
	 * before calling cp_callback_close_socket()
	 */
	if (OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
	    orglen = copylen;
	    if (ocon->p_mm_task) {
		errcode = ocon->p_mm_task->err_code;
		if (errcode == NKN_OM_CONF_REFETCH_REQUEST) {
		    goto skip_buf_copy;
		}
	    }
	    ret = om_makecopy_to_bufmgr(ocon, &copylen);
	    switch( ret ) {
	    case MAKECOPY_NO_TASK:
	    case MAKECOPY_NO_IOVECS:
		DBG_LOG(MSG, MOD_OM, 
			"om_makecopy_to_bufmgr() ret=%d fd=%d "
			"closed with data %d bytes", 
			ret,  OM_CON_FD(ocon), copylen);
		break;
	    case MAKECOPY_NO_DATA:
	    case MAKECOPY_IOVECS_FULL:
	    case MAKECOPY_CL_EOT:
	    case MAKECOPY_CHUNKED_EOT:
	    case MAKECOPY_HEAD_EOT:
	    case MAKECOPY_MORE_DATA:
	    	ocon->phttp->cb_offsetlen += orglen - copylen;
		if (ocon->thistask_datalen  && ocon->phttp->respcode) {
                       ocon->p_mm_task->err_code = 0;
		} else {
			if (OM_CHECK_FLAG(ocon, OMF_NEGATIVE_CACHING) &&
				(ocon->phttp->content_length == 0)) {
			    ocon->p_mm_task->err_code = 0;
			} else {
			    glob_om_fetch_error++;
			    if(ocon->p_mm_task->err_code == 0) {
				ocon->p_mm_task->err_code = 
					NKN_OM_SERVER_FETCH_ERROR;
			    }
			}
		}
		break;
	    default:
		assert(0);
	    }
	} else if (OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
	    if (ocon->p_validate->ret_code == MM_VALIDATE_NONE) {
		ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
	    }
	}

skip_buf_copy:
	// Reset the error code back to original value in the case of 
	// response cache-index refetch. This is because the first probe
	// request would close connection after using configured num-bytes
	// for response body cache index cases.
	if (ocon->p_mm_task && (errcode == NKN_OM_CONF_REFETCH_REQUEST)) {
		ocon->p_mm_task->err_code = errcode;
	}

	DBG_LOG(MSG, MOD_OM, "om_close_conn(fd=%d)", OM_CON_FD(ocon));

	if((ocon->comp_status==1) && OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN)) {
	    return FALSE;
	}

	if (om_close_conn(ocon) == 0) return TRUE;

	if((ocon->comp_status==1) && OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN)) {
	    return FALSE;
	}

	return TRUE;
}

static int cp_callback_timer(void * private_data, net_timer_type_t type);
static int cp_callback_timer(void * private_data, net_timer_type_t type)
{
	UNUSED_ARGUMENT(type);
	omcon_t * ocon = (omcon_t *)private_data;
	int retval = 0;

	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_CP_CALLBACK_TIMER);

	if (OM_CHECK_FLAG(ocon, OMF_CONNECT_TIMER)) {
	    // Abort connect sequence, and send the caller a
	    // an unconditional retry indication.

	    if (ocon->p_mm_task && OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
	    	if (set_uncond_connect_retry(ocon)) { // Retries exceeded
			glob_om_connect_timer_expired_3++;
			ocon->p_mm_task->err_code = NKN_OM_SERVER_DOWN;
		}
		retval = 1; // Close connection

	    } else if (ocon->p_validate && 
		       OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
	    	if (set_uncond_connect_retry(ocon)) { // Retries exceeded
			glob_om_connect_timer_expired_4++;
			ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
		}
		retval = 1; // Close connection
	    }
	    glob_om_connect_timeouts++;

	} else if (OM_CHECK_FLAG(ocon, OMF_READ_TIMER)) {
	    if (ocon->thistask_datalen == ocon->last_thistask_datalen) {
	    	// Abort the read sequence, if no data received send
	    	// an unconditional retry indication otherwise return the
	    	// partial data.

	    	if (ocon->p_mm_task && OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		    if (ocon->thistask_datalen) {
		    	// Send partial data
                	DBG_LOG(MSG, MOD_OM, 
				"Read timeout, resume task with "
				"partial data fd=%d bytes=%ld",
				OM_CON_FD(ocon), ocon->thistask_datalen);
			OM_get_ret_task(ocon);
	    		glob_om_read_tmout_partial_data++;
		    } else {
		    	// Return unconditional retry indication
			if (set_uncond_read_retry(ocon)) { // Retries exceeded
				glob_om_read_timer_expired_1++;
				ocon->p_mm_task->err_code = NKN_OM_SERVER_DOWN;
			}
			retval = 1; // Close connection
	    		glob_om_read_tmout_close++;
		    }
	    	} else if (ocon->p_validate && 
			   OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
		    if (set_uncond_read_retry(ocon)) { // Retries exceeded
			glob_om_read_timer_expired_2++;
			ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
		    }
		    retval = 1; // Close connection
		    glob_om_read_val_tmout_close++;
	    	}
	    	glob_om_read_timeouts++;

	    } else {
	    	// Connection is active, rearm timer
		setup_read_timer(ocon);
	    }
	}
	return retval;
}

/*
 * This function should be called only from CP thread.
 */
int om_close_conn(omcon_t * ocon)
{
	int close_fd;

	cancel_timer(ocon);
	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_OM_CLOSE_CONN);
	OM_SET_FLAG(ocon, OMF_CON_CLOSE);
	if (ocon->p_mm_task && OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		((ocon->p_mm_task->err_code == 0) || 
		    ((OM_CHECK_FLAG(ocon, OMF_NEGATIVE_CACHING))&& (ocon->phttp->content_length == 0))) 
			?  OM_get_ret_task(ocon)
			: OM_get_ret_error_to_task(ocon);



	    if((ocon->comp_status == 1) && OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED)) {
		return 1;
	    }
	} else if (ocon->p_validate && OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
		// When validate, we never call OM_get_ret_task().
		NM_FLOW_TRACE(OM_CON_FD(ocon));
		om_resume_task(ocon);
        } else {
		NM_FLOW_TRACE(OM_CON_FD(ocon));
                DBG_LOG(MSG, MOD_OM, "No task pending, ignore request fd=%d "
                        "p_mm_task=%p p_validate=%p",
                        OM_CON_FD(ocon), ocon->p_mm_task, ocon->p_validate);
        }

	if(OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE) || ocon->content_offset != 0) {
		glob_om_tot_completed_http_trans++;
	}

	if(ocon->send_req_buf) {
		free(ocon->send_req_buf);
		ocon->send_req_buf = NULL;
	}
	NKN_MUTEX_LOCK(&omcon_mutex);
	close_fd = OM_CON_FD(ocon);
	nhash_delete_omcon_by_fd(ocon);
	nhash_delete_omcon_by_uri(ocon);
	clrbit(om_used_fds, close_fd);
	NKN_MUTEX_UNLOCK(&omcon_mutex);

	// Since we are dynamically allocate/free ocon
	// We do not need to reset the value.
	free_omcon_t(ocon);

	return 0;
}

void call_om_close_conn(void * ocon)
{
	om_close_conn((omcon_t *)ocon);
}

static void retry_close(omcon_t *ocon, cpcon_t * cpcon, int tobefreed)
{
	namespace_token_t ns_token;
	if (tobefreed) {
		glob_retry_om_close_fd++;
                nkn_close_fd(OM_CON_FD(ocon) , NETWORK_FD);
        }
	if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
		AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets_ipv6);
		AO_fetch_and_add1(&glob_cp_tot_closed_sockets_ipv6);
	} else {
		AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets);
		AO_fetch_and_add1(&glob_cp_tot_closed_sockets);
	}

	if (cpcon->nsconf) {
		NS_DECR_STATS(cpcon->nsconf, PROTO_HTTP, server, active_conns);
		NS_DECR_STATS(cpcon->nsconf, PROTO_HTTP, server, conns);
	}

	/* we can directly free the memory. */
	shutdown_http_header(&cpcon->http.hdr);
	if (cpcon->http.resp_buf)
		free(cpcon->http.resp_buf);
	free(cpcon->http.cb_buf);
	cpcon->magic = CPMAGIC_DEAD;
	if (cpcon->oscfg.ip)
		free(cpcon->oscfg.ip);
	ns_token = cpcon->ns_token;
	free(cpcon);
        free_omcon_t(ocon);
	release_namespace_token_t(ns_token);

	return;
}

/*
 * end of finite state machine
 */


/* ==================================================================== */
/*
 * support functions
 */

/*
 * hold gnm->mutex lock only when ret_locked==1 and returned ocon!=NULL
 *
 * ret_val = 0: not in om_con_by_uri hashtable.
 * ret_val = 1 && 2: should be retried
 * ret_val = 3: shoule be replaced in om_con_by_uri hashtable.
 */
omcon_t * 
om_lookup_con_by_uri(nkn_uol_t *uol,
			int ret_locked, // Return gnm mutex
			int pass_omcon_locked, // Have omcon_mutex
			int *uri_hit, off_t *offset_delta,
			int * ret_val, omcon_t **stale_ocon, int *lock_fd)
{
	omcon_t * ocon = NULL;
	cpcon_t * cpcon;
	off_t current_offset;
	uint64_t keyhash;
	struct network_mgr * pnm;
	int32_t os_fd;
	uint32_t os_incarn;
	dtois_t uri_code;

	/*
	 * lookup uri hash table.
	 */
	*uri_hit = 0;
	*ret_val = 0;
        uri_code.ui = HASH_URI_CODEI(uol->cod, uol->offset);
	uri_code.us = HASH_URI_CODES(uol->offset);
	keyhash = n_dtois_hash(&uri_code);
	if (!pass_omcon_locked) { 
		NKN_MUTEX_LOCK(&omcon_mutex); 
	}
	ocon = nchash_lookup_off0(om_con_by_uri, keyhash, &uri_code);
	if (ocon == NULL) {
		/* simple, not found. */
		if (!pass_omcon_locked) { 
			NKN_MUTEX_UNLOCK(&omcon_mutex); 
		}
		// new insert
		return NULL;
	}

	os_fd = ocon->fd;
	os_incarn = ocon->incarn;
	if (!pass_omcon_locked) { 
		NKN_MUTEX_UNLOCK(&omcon_mutex); 
	}
	if(os_fd<0 || os_fd>=MAX_GNM) {
		// new insert
		return NULL;
	}

	/*
	 * release omcon_mutex and acquire gnm[ocon->fd].mutex lock
	 */
	pnm = &gnm[os_fd];
	if (pass_omcon_locked) {
		/*
		 * avoid deadlock, using trylock only for this case.
		 */
		if (pthread_mutex_trylock(&pnm->mutex) != 0) {
			*ret_val = 1;
			// need to check
			return NULL; // Could not acquire mutex lock
		}
		NM_TRACE_LOCK(pnm, LT_OM_NETWORK);
	} else {
		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_OM_NETWORK);
	}
	cpcon = gnm[os_fd].private_data;
	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) &&
	    (pnm->fd == os_fd) && 
	    (pnm->incarn == os_incarn) &&
	    (cpcon->type == CP_APP_TYPE_OM) &&
	    (cpcon->private_data == (void *)ocon) &&
	    (ocon->fd == os_fd) &&		/* BZ 7408 fd and incarn is changed */
	    (ocon->incarn == os_incarn))	/* by the time gnm lock is acquired */
	{
		if ((ocon->httpfd >= 0) && !OM_CHECK_FLAG(ocon, OMF_NO_CACHE)) {
			current_offset = ocon->content_offset + ocon->content_start;
			if ((ocon->uol.offset == uol->offset) ||
			((current_offset == uol->offset) &&
			((ocon->content_length - current_offset) >=
			uol->length))) {
				/* valid ocon and return this ocon */
				if(!ret_locked) {
					NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
					pthread_mutex_unlock(&pnm->mutex);
				}
				if (lock_fd) *lock_fd = (int) os_fd;
				// no check here
				return ocon;
			} else {
				glob_om_lookup_by_uri_offset_mismatch ++;
				*uri_hit = 1;
				*offset_delta = uol->offset - current_offset;
				NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);
				// delayed retry to BM
				*ret_val = 2;
				return NULL;
			}
		} else {
			glob_om_lookup_by_uri_no_cache_flag_set ++;
			*stale_ocon = ocon;
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
			// replace ocon
			*ret_val = 3;
			return NULL;
		}
	}
	else {
		glob_om_lookup_by_uri_ocon_freed ++;
		*stale_ocon = ocon;
		NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
		pthread_mutex_unlock(&pnm->mutex);
		// replace ocon
		*ret_val = 3;
		return NULL;
	}
}

/*
 * hold gnm->mutex lock only when 
 *      1. ret_locked==1 and returned ocon!=NULL
 * all other cases we will not hold the gnm->mutex lock.
 *
 * Notice:
 *      Only either returned ocon!=NULL or returned stale_ocon!=NULL
 *      is true, but not both are true in the same time.
 */
omcon_t * om_lookup_con_by_fd(int fd, nkn_uol_t *uol, int ret_locked,
				int remove_stale_omcon, int *lock_fd)
{
        omcon_t * ocon = NULL;
	cpcon_t * cpcon;
	off_t current_offset;
	uint64_t loc_fd = fd, keyhash;
	struct network_mgr * pnm;
	int32_t os_fd;
	uint32_t os_incarn;

	/*
	 * omcon_mutex is used to protect hash table only,
	 * not ocon structure.
	 */
	keyhash = n_uint64_hash(&loc_fd);
	NKN_MUTEX_LOCK(&omcon_mutex);
	ocon = nchash_lookup(om_con_by_fd, keyhash, &loc_fd);
	if (ocon == NULL) {
		/* simple, not found. */
		NKN_MUTEX_UNLOCK(&omcon_mutex);
		return NULL;
	}

	os_fd = ocon->fd;
	os_incarn = ocon->incarn;
	NKN_MUTEX_UNLOCK(&omcon_mutex);
	if(os_fd<0 || os_fd>=MAX_GNM) {
		return NULL;
	}
	pnm = &gnm[os_fd];
	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_OM_NETWORK);
	cpcon = gnm[os_fd].private_data;
	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) &&
	    (pnm->fd == os_fd) && 
	    (pnm->incarn == os_incarn) &&
	    (cpcon->type == CP_APP_TYPE_OM) &&
	    (cpcon->private_data == (void *)ocon) &&
	    (ocon->fd == os_fd) &&		/* BZ 7408 fd and incarn is changed */
	    (ocon->incarn == os_incarn))	/* by the time gnm lock is acquired */
	{
		/*
		 * It is impossible for other thread to free ocon at this time.
		 * because we own the gnm[ocon->fd].mutex lock here.
		 */
		current_offset = ocon->content_offset + ocon->content_start;
		if ((ocon->uol.cod == uol->cod) &&
			((ocon->uol.offset == uol->offset) ||
			((current_offset == uol->offset) &&
			((ocon->content_length - current_offset) >=
                                uol->length)) ||
			IS_PSEUDO_FD(fd))) {
			/* find a good ocon and return this ocon*/
			if(!ret_locked) {
				NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);
			}
			if (lock_fd) *lock_fd = (int) os_fd;
        		return ocon;
		} 

		if (remove_stale_omcon) {
			// close the stale_omcon right away here
			NKN_MUTEX_LOCK(&omcon_mutex);

			ocon->httpfd = -1;
			nhash_delete_omcon_by_fd(ocon);
			nhash_delete_omcon_by_uri(ocon);
			clrbit(om_used_fds, os_fd);
			NKN_MUTEX_UNLOCK(&omcon_mutex);
			cp_add_event(os_fd, OP_CLOSE_CALLBACK, NULL);
			glob_om_stale_conn_close++;
		}

		glob_om_lookup_by_fd_stale_omcon ++;
		// Fall through common code
	}
	else {
		glob_om_lookup_by_fd_ocon_freed ++;
		// Fall through common code
	}

	NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
	pthread_mutex_unlock(&pnm->mutex);
	return NULL;
}

/* ==================================================================== */
/*
 * Main entry loop of in-line OM
 */

/* 
 * Finds the origin server (ip, port) from the namespace configuration
 * Follows om code present in om_network.c 
 */
int find_origin_server(con_t * request_con, // IN
			origin_server_cfg_t * oscfg,// IN
			request_context_t * req_ctx, // IN
			const char *client_host, // IN
			int client_host_len,// IN
			int trace_on,// IN
			ip_addr_t* originipv4v6, // IN
			uint16_t originport, // IN
			ip_addr_t *ipv4v6, // OUT
			uint16_t * port, // OUT
			int *allow_retry // OUT
)
{
	const namespace_config_t *nsc;
	int rv;
	int ip_len;
	//char *host;
	u_int32_t attr;
	int hdrcnt;
	const char *data;
	int datalen;
	const char *location;
	int locationlen;
	int lookup_method;
	//int hostlen, portlen;

	nsc = oscfg->nsc;
	if (oscfg->ip) { free(oscfg->ip); oscfg->ip = NULL; }


	/*
	 * find the origin server based on configuration.
	 * No need to check, ip, if port not there, as good
	 * as IP not there
	 */
	if (!originport) {
		if (CHECK_CON_FLAG(request_con, CONF_REFETCH_REQ) && nsc && nsc->http_config &&
                                nkn_http_is_tproxy_cluster(nsc->http_config)) {
			lookup_method = REQ2OS_TPROXY_ORIGIN_SERVER_LOOKUP;
		} else {
			lookup_method = REQ2OS_ORIGIN_SERVER;
		}	
		rv = request_to_origin_server(lookup_method, req_ctx,
				      request_con ? &request_con->http.hdr : 0, 
				      nsc, client_host, client_host_len,
				      &oscfg->ip, 0, &ip_len,
				      &oscfg->port, 1, 0);
	} else {
		/* char ipdotaddr[INET_ADDRSTRLEN+6];
		inet_ntop(AF_INET, (const void *)&originip,
			  ipdotaddr, INET_ADDRSTRLEN);
			ipdotaddr[INET_ADDRSTRLEN] = '\0';*/
		oscfg->ip = nkn_strdup_type(get_ip_str(originipv4v6), mod_om_strdup2);
		oscfg->port = originport;
		rv = 0;
	}

	if (rv) {
		if (rv < 0) {
			*allow_retry = 1;
		} else {
			*allow_retry = 0;
		}
		DBG_LOG(WARNING, MOD_OM, 
			"request_to_origin_server() failed, rv=%d", rv);
		return FALSE;
	}

	/* 302 Redirect.
	 * If the host/origin was a different one, then connect
	 * to a server other than what the namespace tells us to.
	 */
	if (CHECK_HTTP_FLAG(&request_con->http, HRF_302_REDIRECT)) {
		// Get the LOCATION header
		rv = get_known_header(&request_con->http.hdr,
				MIME_HDR_X_LOCATION, &location, &locationlen,
				&attr, &hdrcnt);
		if (!rv) {
			// Check if a host header exisits.. if so, swap out
			// the hold destinatio ip with the new one.
			rv = get_known_header(&request_con->http.hdr,
					MIME_HDR_X_REDIRECT_HOST, &data, &datalen,
					&attr, &hdrcnt);
			if (!rv) {
				if (oscfg->ip) {
				    free(oscfg->ip);
				}
				oscfg->ip = nkn_calloc_type(1, sizeof(char)*(datalen + 1), mod_om_strdup1);
				snprintf(oscfg->ip, datalen+1, "%s", data);
			}
			// Check if a X_Port header exisits.. if so, swap
			// out the old destination port with the new one.
			rv = get_known_header(&request_con->http.hdr,
					MIME_HDR_X_REDIRECT_PORT, &data, &datalen,
					&attr, &hdrcnt);
			if (!rv) {
				char ctmp[32];
				strncpy(ctmp, data, datalen);
				ctmp[datalen+1] = '\0';
				oscfg->port = atoi(ctmp);
				oscfg->port = htons(oscfg->port);
			} else 	if(strncmp(location, "http://", 7) == 0) {
				oscfg->port = htons(80);
			}
		}
	}

	*port = oscfg->port;
	if  (dns_domain_to_ip46(oscfg->ip, ipv4v6->family, ipv4v6) == 0) {
		DBG_LOG(WARNING, MOD_OM, "dns_domain_to_ip(%s) failed, family(%d)", oscfg->ip,
				 ipv4v6->family);
		if (trace_on) {
			DBG_TRACE("OM dns_domain_to_ip(%s) failed, family(%d)", oscfg->ip, ipv4v6->family);
		}
		return FALSE;
	}
	/*
	 * We don't want to make a socket to connect back to nvsd itself
	 * it will become an infinite loop, unless its an internal request
	 */
	if (!CHECK_HTTP_FLAG(&request_con->http, HRF_CLIENT_INTERNAL)) {
		if (is_nvsd_listen_server(ipv4v6, *port)) {
			DBG_LOG(WARNING, MOD_OM, "origin_ip(%s) is matching to local_ip", oscfg->ip);
			return FALSE;
		}
	}
	
	return TRUE;
}

static omcon_t * om_init_client(const char * uri, char * physid, 
				con_t *request_con, 
				namespace_token_t ns_token,
				int return_locked, int trace_on, 
				const char *client_host, int client_host_len, 
				void * private_data,
				request_context_t *req_ctx, int *allow_retry,
				ip_addr_t* originipv4v6, uint16_t originport)
{
        omcon_t * ocon;
	cpcon_t * cpcon;
	const namespace_config_t *nsc;
	ip_addr_t  src_ipv4v6;
	uint16_t port;
	ip_addr_t ipv4v6;
	int is_v6origin =0;

	memset(&src_ipv4v6, 0, sizeof(ip_addr_t));
	memset(&ipv4v6, 0, sizeof(ip_addr_t));


	nkn_client_data_t *client_data = (nkn_client_data_t *) (private_data);
	int ret_allow_retry;

	UNUSED_ARGUMENT(uri);

	*allow_retry = 0;

	nsc = namespace_to_config(ns_token);
	if(!nsc) {
		DBG_LOG(WARNING, MOD_OM, "No namespace data.");
		glob_om_init_client_no_ns++;
		return NULL; // No namespace data
	}

	if(!nsc->http_config) {
                DBG_LOG(WARNING, MOD_OM, "No origin server config data.");
		release_namespace_token_t(ns_token);
		glob_om_init_client_no_config++;
		return NULL;
	}

	switch(nsc->http_config->origin.select_method) {
	case OSS_HTTP_CONFIG:
	case OSS_HTTP_PROXY_CONFIG:
	case OSS_HTTP_ABS_URL:
	case OSS_HTTP_FOLLOW_HEADER:
	case OSS_HTTP_SERVER_MAP:
	case OSS_HTTP_NODE_MAP:
		break;
	case OSS_HTTP_DEST_IP:
		if (!client_host || (client_host[0]!='.')) break; // sanity check
		// BUG 4455: special case for T-Proxy
		// Internal format: .real_IP:port:domain.
		client_host ++;
		client_host_len --;
		break;
	default:
                DBG_LOG(WARNING, MOD_OM, "No origin server HTTP config data.");
		release_namespace_token_t(ns_token);
		glob_om_init_client_inv_os_select++;
		return NULL;
	}

	// fill in the slot
	ocon = (omcon_t *)nkn_malloc_type(sizeof(omcon_t), mod_om_ocon_t);
	if(!ocon) {
                DBG_LOG(WARNING, MOD_OM, "Run out of memory.");
		release_namespace_token_t(ns_token);
		*allow_retry = 1;
		glob_om_init_client_malloc_failed++;
		return NULL;
	}
	memset(ocon, 0, sizeof(omcon_t));
	AO_fetch_and_add1(&glob_om_open_server_conn);

	ocon->httpreqcon = request_con;
	strcpy(ocon->physid, physid);
	// Note: release_namespace_token_t(ns_token) invoked when 
	//       omcon_t is destructed
	ocon->oscfg.ns_token = ns_token;
	ocon->oscfg.policies = &nsc->http_config->policies;
	ocon->oscfg.request_policies = &nsc->http_config->request_policies;
	ocon->oscfg.nsc = nsc;
        if (nsc->http_config->origin.u.http.ip_version == FOLLOW_CLIENT) {
		ipv4v6.family = client_data->client_ip_family;
                if (client_data->client_ip_family== AF_INET6) {
                        is_v6origin = 1;
                }
        } else if (nsc->http_config->origin.u.http.ip_version == FORCE_IPV6) {
                is_v6origin = 1;
		ipv4v6.family = AF_INET6;
        } else {
		ipv4v6.family = AF_INET;
	}

	if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_TPROXY_MODE)) {
		ipv4v6.family = client_data->client_ip_family;
	}

	if (find_origin_server(ocon->httpreqcon, &ocon->oscfg, req_ctx,
				   client_host, client_host_len, trace_on, 
				   originipv4v6, originport,
				   &ipv4v6, &port, &ret_allow_retry) == FALSE) 
	{
		free_omcon_t(ocon);
		if (ret_allow_retry) {
			*allow_retry = 1;
		}
		glob_om_init_client_find_os_failed++;
		release_namespace_token_t(ns_token);
                return NULL;
        }

        if (CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_TPROXY_MODE) &&
                        nsc->http_config->request_policies.dns_resp_mismatch) {
                int dns_mismatch = 1;

                if (request_con->ip_family == AF_INET) {
                        dns_mismatch = 0;
                        if (IPv4(request_con->http.remote_ipv4v6) != IPv4(request_con->dest_ipv4v6)) {
                                dns_mismatch = is_dns_mismatch(ocon->oscfg.ip, request_con->http.remote_ipv4v6, ipv4v6.family);
                        }
                } else {
                        dns_mismatch = 0;
                        if (memcmp(IPv6(request_con->http.remote_ipv4v6), IPv6(request_con->dest_ipv4v6), sizeof(struct in6_addr))) {
                                dns_mismatch = is_dns_mismatch(ocon->oscfg.ip, request_con->http.remote_ipv4v6, ipv4v6.family);
                        }
                }

                if (dns_mismatch) {
                        AO_fetch_and_add1(&glob_om_dns_mismatch);
                        release_namespace_token_t(ns_token);
                        return NULL;
                }
        }

	/*
	 * default: src_ip = 0.
	 * in case client ip address is missing, we use interface IP address.
	 *
	 * Bug 5473 :
	 * We should check only for use_client_ip being set, since this can be 
	 * set for both OSS_HTTP_DEST_IP and OSS_HTTP_FOLLOW_HEADER.
	 */
	if (nsc->http_config->origin.u.http.use_client_ip == NKN_TRUE || 
            (CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) && 
             nkn_http_is_tproxy_cluster(nsc->http_config))) {
		if (client_data != NULL) {
			memcpy(&src_ipv4v6, &(client_data->ipv4v6_address), sizeof(ip_addr_t));
		}
	}

        cpcon = cp_open_socket_handler(&ocon->fd, 
		&src_ipv4v6, &ipv4v6, port,
		CP_APP_TYPE_OM,
		(void *)ocon,
		cp_callback_connected,
		cp_callback_connect_failed,
		cp_callback_send_httpreq,
		cp_callback_http_header_received,
		cp_callback_http_header_parse_error,
		cp_callback_http_body_received,
		cp_callback_close_socket,
		cp_callback_timer);
        if(!cpcon) {
                free_omcon_t(ocon);
		*allow_retry = 1;
		glob_om_init_client_open_socket_failed++;
		release_namespace_token_t(ns_token);
                return NULL;
	    }
	if (CHECK_HTTP_FLAG(&request_con->http, HRF_METHOD_HEAD) ||
		(CHECK_CON_FLAG(ocon->httpreqcon, CONF_CI_FETCH_END_OFF))) {
		SET_CPCON_FLAG(cpcon, CPF_METHOD_HEAD);
	}
	if (nsc->http_config->origin.u.http.use_client_ip == NKN_TRUE || 
            (CHECK_CON_FLAG(ocon->httpreqcon, CONF_REFETCH_REQ) &&
             nkn_http_is_tproxy_cluster(nsc->http_config))) {
		SET_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ);
		//cpcon->src_ip = request_con->src_ip;
		//cpcon->src_port = request_con->src_port;
        }
	if (is_v6origin) {
		SET_CPCON_FLAG(cpcon, CPF_IS_IPV6);
		OM_SET_FLAG(ocon, OMF_IS_IPV6);
	}

	ocon->incarn = gnm[ocon->fd].incarn;
	ocon->cpcon = (void *)cpcon;
	ocon->phttp = &cpcon->http;
	cpcon->http.nsconf = ocon->oscfg.nsc;
	cpcon->ns_token = ns_token;

	NKN_MUTEX_LOCK(&omcon_mutex);
	if (isset(om_used_fds, OM_CON_FD(ocon))) {
		struct network_mgr * pnm;

		/*
		 * BUG 3665
		 * To be done: Cannot find a code path which can insert 
		 * the same cpcon (same cp_sockfd) into omq_head twice.
		 * add bitmap om_used_fds[] to work around the issue
		 * by aborting the second request.
		 * Note: we do not need to free cpcon at this time
		 * because it was used by first request.
		 */
		glob_om_err_fd_inuse++;
		pnm = &gnm[ocon->fd];
		NKN_MUTEX_UNLOCK(&omcon_mutex);
		free_omcon_t(ocon);
		release_namespace_token_t(ns_token);
		/* This lock was acquired inside function 
		 * cp_open_socket_handler() */
		NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
		pthread_mutex_unlock(&pnm->mutex);
		return NULL;
	}
	setbit(om_used_fds, OM_CON_FD(ocon));
	if (!return_locked) {
		NKN_MUTEX_UNLOCK(&omcon_mutex);
	}
	NM_EVENT_TRACE(OM_CON_FD(ocon), FT_OM_INIT_CLIENT);
        return ocon;
}

/*
 * Main loop and socket functions
 * Return:
 *   ret == 0 => Success
 *   ret != 0 => Error
 */
int OM_do_socket_work(MM_get_resp_t * p_mm_task, omcon_t **pp_ocon,
		      ip_addr_t* originipv4v6, uint16_t originport)
{
	omcon_t * ocon = NULL;
	cpcon_t * cpcon ;
	uint32_t incarn;
	omcon_t * stale_ocon = NULL;
	int32_t ocon_fd;
	con_t * httpcon;
	struct network_mgr * pnm;
        int i;
	uint64_t httpcon_fd;
	uint64_t keyhash1, keyhash2;
	dtois_t uri_code;
	int retval = 0;
	int fd;
	int lock_fd = -1;
	off_t current_offset;
	int ret_cp_lock = 0;
	int continuation_request = 0;
	int uri_hit;
	off_t offset_delta = 0;
	int request_delay_msecs;
	int restarted = 0;
	int allow_retry;
	int ret_val;
	namespace_token_t ns_token;

	httpcon = (con_t *)p_mm_task->in_proto_data;
	if (httpcon == NULL) {
		/* Sanity chck */
		glob_om_err_get ++;
		retval = 1;
		goto error;
	}
	if(httpcon->fd==-1 || (httpcon->http.cb_totlen==0)) {
		/* Sanity chck */
		glob_om_err_get ++;
		retval = 3;
		goto error;
	}
	if ((httpcon->fd>0 && httpcon->fd<MAX_GNM)) {
		NM_EVENT_TRACE(httpcon->fd, FT_OM_DO_SOCKET_WORK);
	}

restart_fd:
	ocon=om_lookup_con_by_fd(httpcon->fd, &p_mm_task->in_uol, 
		1 /* ret_locked */, TRUE, &lock_fd);
restart:
	if (restarted >= 2) {
		/* Break infinite loop when MM says no RETRY */
		ocon = NULL;
		retval = 4;
		goto error;
	}

	if (!ocon) {
		ocon=om_lookup_con_by_uri(&p_mm_task->in_uol,
				1 /* ret_locked */, 0/* passin_lock */,
				&uri_hit, &offset_delta, &ret_val, &stale_ocon, &lock_fd);
		if (ocon && restarted && !(p_mm_task->in_flags & MM_FLAG_CONF_NO_RETRY)) {
			/*
			 * Delay to allow the other task to run ahead
			 * so this request is satisfied from BM.
			 */
			ocon_fd = lock_fd;
			lock_fd = -1;
			pnm = &gnm[ocon_fd];
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
			request_delay_msecs = om_req_collison_delay_msecs;
			ocon = NULL;
			retval = 101;
			// printf("delay collision %d, %ld\n", httpcon->fd, p_mm_task->in_uol.offset);
			glob_om_delay_collision++;
			goto delay_request;

		} else if (!ocon && uri_hit && !(p_mm_task->in_flags & MM_FLAG_CONF_NO_RETRY)) {
			if ((offset_delta < 0) && 
			    (-offset_delta <= (2*MiBYTES))) {
				/*
			    	 * We are behind and within the 2MB window
			    	 * Perform immediate retry to hit in BM
				 */
				request_delay_msecs = om_req_behind_delay_msecs;
				retval = 102;
				// printf("delay behind %d, %ld\n", httpcon->fd, p_mm_task->in_uol.offset);
				glob_om_delay_behind++;
				goto delay_request;

			} else if ((offset_delta > 0) && 
				    (offset_delta <= (2*MiBYTES))) {
				/*
			    	 * We are ahead and within 2MB window
			 	 * Delay to allow for a 2MB window ahead
				 * of our current position
				 */
				request_delay_msecs = om_req_ahead_delay_msecs;
				retval = 103;
				// printf("delay ahead %d, %ld\n", httpcon->fd, p_mm_task->in_uol.offset);
				glob_om_delay_ahead++;
				goto delay_request;
			}
		}
	}

	if((ocon == NULL) || restarted) {
		/*
		 * Case: This is a new physid with new connection
		 */
                if (ocon) {
			ocon_fd = lock_fd;
			lock_fd = -1;
			pnm = &gnm[ocon_fd];
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
                }

		httpcon_fd = (uint64_t)httpcon->fd;
		keyhash1 = n_uint64_hash(&httpcon_fd);
        	uri_code.ui = HASH_URI_CODEI(p_mm_task->in_uol.cod ,
					p_mm_task->in_uol.offset);
		uri_code.us = HASH_URI_CODES(p_mm_task->in_uol.offset); 
		// printf("init %d, %ld, %d\n", httpcon->fd, p_mm_task->in_uol.offset, p_mm_task->in_flags);
       		ocon = om_init_client(nkn_cod_get_cnp(p_mm_task->in_uol.cod), 
				p_mm_task->physid, httpcon, 
				p_mm_task->ns_token, 1, 
				p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST, 
				0, 0, &p_mm_task->in_client_data,
				&p_mm_task->req_ctx, &allow_retry,
				originipv4v6, originport);
		if( ocon == NULL ) {
			/* pnm[cp_sockerfd].mutex  has been released in om_init_client */
			retval = 2;
			glob_om_err_init_client++;
			if (allow_retry) {
				glob_om_err_init_client_retry++;
				set_task_uncond_taskstart_retry(p_mm_task, 1);
			}
			goto error;
		}
		ocon->uol.offset = p_mm_task->in_uol.offset;
		ocon->hash_cod = uri_code;
		ocon->hash_fd = ocon->httpfd	= httpcon->fd;
		omcon_t *temp;
		if (!(temp = nchash_lookup(om_con_by_fd , keyhash1, &ocon->hash_fd))){
			nchash_insert_new(om_con_by_fd, keyhash1, &ocon->hash_fd, ocon);
		} else {
			/* 
			 * This case should NEVER happen.
			 * If it does happen, we need to debug.
			 */
			NKN_ASSERT(0);

			pnm = &gnm[ocon->fd];
			cpcon = (cpcon_t *)ocon->cpcon;
			clrbit(om_used_fds, ocon->fd);
			NKN_MUTEX_UNLOCK(&omcon_mutex);
			int tobefreed = 0;
                        if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
                                NM_close_socket(ocon->fd);
                        } else
                                tobefreed= 1;
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
			retry_close(ocon, cpcon, tobefreed);
			glob_retry_om_by_fd++;
			ocon = NULL;
			goto restart_fd;
		}

		/*
		 * Check if this "p_mm_task->in_uol" is in hashtable or not.
		 */
		omcon_t *temp2;
		stale_ocon = NULL;
		temp2=om_lookup_con_by_uri(&p_mm_task->in_uol,
				0 /* ret_locked */, 1/* passin_lock */,
				&uri_hit, &offset_delta, &ret_val, &stale_ocon, &lock_fd);
		if ((temp2 == NULL) && (ret_val == 0 || ret_val == 3)) {
			keyhash2 = n_dtois_hash(&uri_code);
			if (ret_val == 0) {
				nchash_insert_off0(om_con_by_uri, 
				    	keyhash2, &ocon->hash_cod, ocon);
			} else if (ret_val == 3) {
				nhash_replace_off0(om_con_by_uri,
					keyhash2, &ocon->hash_cod, ocon);	
				if (stale_ocon) {
					stale_ocon->hash_cod_flag = 1;
				}
				glob_om_in_uri_hashtable++;
			}
			ocon->hash_cod_flag = 0;
			stale_ocon = NULL;
		} else {
                        pnm = &gnm[ocon->fd];
			cpcon = (cpcon_t *)ocon->cpcon;
			if (ocon == nchash_lookup(om_con_by_fd , keyhash1,
					         &ocon->hash_fd)){
				nchash_remove(om_con_by_fd, keyhash1,
					     &ocon->hash_fd);
			}
			clrbit(om_used_fds, ocon->fd);
                        NKN_MUTEX_UNLOCK(&omcon_mutex);
			int tobefreed = 0;
                        if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
                                NM_close_socket(ocon->fd);
                        } else
                                tobefreed= 1;
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
                        pthread_mutex_unlock(&pnm->mutex);
			retry_close(ocon, cpcon, tobefreed);
			glob_retry_om_by_uri++;
			ocon = NULL;
			stale_ocon = NULL;
			restarted ++;
                        goto restart;
		}
		ret_cp_lock = 1;
		// Release lock acquired in om_init_client()
		ocon->content_start = ocon->uol.offset;
		NKN_MUTEX_UNLOCK(&omcon_mutex);
		ocon->uol.cod = nkn_cod_dup(p_mm_task->in_uol.cod, NKN_COD_ORIGIN_MGR);

		ocon->uol.length = p_mm_task->in_uol.length;
		ocon->thistask_datalen = 0;
		ocon->httpreq	= httpcon->http.cb_buf;
		ocon->p_mm_task  = p_mm_task;
		p_mm_task->ocon  = (void *)ocon; // debug support

		if (p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
			char ipdotaddr[INET_ADDRSTRLEN+1];
			inet_ntop(AF_INET, (const void *)&IPv4(ocon->phttp->remote_ipv4v6),
				  ipdotaddr, INET_ADDRSTRLEN);
			ipdotaddr[INET_ADDRSTRLEN] = '\0';
			DBG_TRACE("OM connecting %s:%hu for object %s",
				  ipdotaddr, ocon->phttp->remote_port,
				  nkn_cod_get_cnp(ocon->uol.cod));
		}

        	/* Time to make socket to origin server */
		cpcon = (cpcon_t *)ocon->cpcon;
               	OM_SET_FLAG(ocon, OMF_DO_GET|OMF_WAIT_COND);
#ifdef RECORD_OCON_IN_TASK
		nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
				     p_mm_task->get_task_id, ocon);
#endif
       		ocon->phttp->num_nkn_iovecs = p_mm_task->in_num_content_objects;
		assert(ocon->phttp->num_nkn_iovecs <= MAX_CM_IOVECS);
       		for(i=0; i < (int)p_mm_task->in_num_content_objects; i++) {
			ocon->phttp->content[i].iov_len  = CM_MEM_PAGE_SIZE;
			ocon->phttp->content[i].iov_base = 
			nkn_buffer_getcontent(p_mm_task->in_content_array[i]);
		}
        	if (cp_activate_socket_handler(OM_CON_FD(ocon), cpcon) == FALSE) {
			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
			glob_om_err_cp_activate_socket++;
	        	if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
        	        	AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets_ipv6);
                		AO_fetch_and_add1(&glob_cp_tot_closed_sockets_ipv6);
        		} else {
                		AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets);
                		AO_fetch_and_add1(&glob_cp_tot_closed_sockets);
        		}
			ret_cp_lock = 0;
			fd = OM_CON_FD(ocon);

			DBG_LOG(MSG, MOD_OM, "cp_activate_socket_handler failed for fd=%d",
					OM_CON_FD(ocon));

			/* we can directly free the memory. */
			shutdown_http_header(&cpcon->http.hdr);
			free(cpcon->http.cb_buf);

			/* BZ 8532: port leaks here in case ot tproxy */
			if (CHECK_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ)) {
			    /* Free the port */
			    cp_release_transparent_port(cpcon);
			}
			cpcon->magic = CPMAGIC_DEAD;
			if (cpcon->oscfg.ip) free(cpcon->oscfg.ip);
			ns_token = cpcon->ns_token;
			free(cpcon);
			ocon->p_mm_task->err_code = NKN_OM_SERVER_DOWN;
#ifdef RECORD_OCON_IN_TASK
			nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
						p_mm_task->get_task_id, 0);
#endif
			// close the task without data back
			p_mm_task->out_num_content_objects = 0;
			retval = 0;
			DBG_LOG(MSG, MOD_OM, "socket error retval = %d\n", retval);
			om_close_conn(ocon);
			nkn_close_fd(fd, NETWORK_FD);
			release_namespace_token_t(ns_token);
			ocon = NULL;
			*pp_ocon = NULL;
			return retval;
        	}
		setup_connect_timer(ocon);

	} else {
		/*
		 * Continuation request.
		 * Requestor may not be the original initiator if
		 * content is cacheable.
		 * 
		 */
		continuation_request = 1;
		//fd = OM_CON_FD(ocon);
		fd = lock_fd;
		lock_fd = -1;
		incarn = ocon->incarn;
		cpcon = (cpcon_t *)ocon->cpcon;

		/* Note: ocon my be freed at this point */

		pnm=&gnm[fd];

		// Synchronize between epoll thread and this thread
		// This lock should get before NM_add_event_epollin
		ret_cp_lock = 1;
		if( !NM_CHECK_FLAG(pnm, NMF_IN_USE) || 
		    (pnm->incarn != incarn) || 
		    (pnm->private_data != cpcon)) {
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
			ret_cp_lock = 0;
			ocon = NULL;
			restarted++;
			goto restart;
		}

		NKN_MUTEX_LOCK(&omcon_mutex);
		current_offset = ocon->content_offset + ocon->content_start;
		if (ocon->p_mm_task || 
		    ((ocon->httpfd != httpcon->fd) && OM_CHECK_FLAG(ocon, OMF_NO_CACHE))||
		    (cpcon->state == CP_FSM_SYN_SENT) ||
		    (ocon->uol.cod != p_mm_task->in_uol.cod) ||
		    (current_offset != p_mm_task->in_uol.offset) ||
		    (p_mm_task->in_uol.length > (ocon->content_length - current_offset))) {
			if (!restarted && isset(om_used_fds, fd)) {
				nhash_delete_omcon_by_fd(ocon);
				nhash_delete_omcon_by_uri(ocon);
			}
			NKN_MUTEX_UNLOCK(&omcon_mutex);
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
			ret_cp_lock = 0;
			ocon = NULL;
			restarted++;
			goto restart;
		}
		if (ocon->httpfd != httpcon->fd) {
			nhash_delete_omcon_by_fd(ocon);
			ocon->hash_fd = ocon->httpfd = httpcon->fd;
			keyhash1 = n_uint64_hash(&ocon->hash_fd);
			nchash_insert(om_con_by_fd, keyhash1, &ocon->hash_fd, ocon);
			glob_om_con_steal++;
		}
		NKN_MUTEX_UNLOCK(&omcon_mutex);

       		ocon->uol.offset = p_mm_task->in_uol.offset;
       		ocon->uol.length = p_mm_task->in_uol.length;
       		ocon->p_mm_task  = p_mm_task;
		ocon->httpreqcon = httpcon;
       		p_mm_task->ocon = (void *)ocon; // debug support

		ocon->thistask_datalen = 0;
		ocon->httpreq	= 0;
#ifdef RECORD_OCON_IN_TASK
		nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
				     p_mm_task->get_task_id, ocon);
#endif
		OM_SET_FLAG(ocon, OMF_WAIT_COND);

       		ocon->phttp->num_nkn_iovecs = p_mm_task->in_num_content_objects;
		assert(ocon->phttp->num_nkn_iovecs <= MAX_CM_IOVECS);
       		for(i=0; i < (int)p_mm_task->in_num_content_objects; i++) {
			ocon->phttp->content[i].iov_len  = CM_MEM_PAGE_SIZE;
			ocon->phttp->content[i].iov_base = 
			nkn_buffer_getcontent(p_mm_task->in_content_array[i]);
		}

		if(p_mm_task->in_uol.offset && (ocon->phttp->cb_totlen-ocon->phttp->cb_offsetlen > 0) ) {
			// copy data if left over in last read
			cp_add_event(OM_CON_FD(ocon), OP_EPOLLOUT, NULL);
		}
		else {
			if (NM_add_event_epollin(OM_CON_FD(ocon))==FALSE) {
				NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);
				ret_cp_lock = 0;
				retval = 9;
				glob_om_err_add_event_epollin++;

				DBG_LOG(MSG, MOD_OM, "NM_add_event_epollin failed for fd=%d",
					OM_CON_FD(ocon));

				/* let network thread free the memory */
				pnm = &gnm[OM_CON_FD(ocon)];
				pthread_mutex_lock(&pnm->mutex);
				NM_TRACE_LOCK(pnm, LT_OM_NETWORK);
				cp_add_event(OM_CON_FD(ocon), OP_CLOSE_CALLBACK, NULL);
				NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);

				ocon = NULL;
				goto error;
			}
		}
		setup_read_timer(ocon);
	}

	if(ret_cp_lock) {
		pnm = &gnm[OM_CON_FD(ocon)];
		NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
		pthread_mutex_unlock(&pnm->mutex);
	}
	*pp_ocon = ocon;
	//NM_EVENT_TRACE(OM_CON_FD(ocon), FT_OM_DO_SOCKET_WORK);

	assert(retval == 0);
	return retval;


delay_request:
	p_mm_task->out_delay_ms = request_delay_msecs;
	p_mm_task->err_code = NKN_MM_CONF_RETRY_REQUEST;

	nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
			     p_mm_task->get_task_id, 0);
	p_mm_task->out_num_content_objects = 0;
	assert(ocon == NULL);
	*pp_ocon = NULL;
	return retval;

error:
	nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
			     p_mm_task->get_task_id, 0);
	// close the task without data back
	p_mm_task->out_num_content_objects = 0;
	assert(ocon == NULL);
	DBG_LOG(MSG, MOD_OM, "socket error retval = %d\n", retval);
	*pp_ocon = NULL;
	return retval;
}

/*
 * Initiate object validate
 */
int OM_do_socket_validate_work(MM_validate_t * pvalidate,
				     const mime_header_t *attr_mh,
				     omcon_t **pp_ocon)
{
	omcon_t *ocon = NULL;
	cpcon_t * cpcon;
	struct network_mgr *pnm;
	const char *client_host = 0;
	const char *remapped_uri;
	int remapped_uri_len;
	int client_host_len;
	int urilen;
	u_int32_t hattr;
	int hdrcnt;
	int retval;
	nkn_pseudo_con_t *pcon = 0;
	const char *cache_key;
	const char *uri;
	int rv;
	int fd;
	int allow_retry;
        const namespace_config_t *nsc;
	namespace_token_t ns_token;
#if 1
	const char *origin_ip = 0;
	int origin_ip_len = 0;
#endif
	con_t *org_httpreqcon = NULL;
	cache_key = nkn_cod_get_cnp(pvalidate->in_uol.cod);

	/*
	 * Create a pseudo_con_t for this request since con_t does not exist
	 */
	pcon = nkn_alloc_pseudo_con_t();
	if (!pcon) {
		retval = 1;
		goto error;
	}
	if (pvalidate->in_proto_data) {
		rv = get_known_header(&((con_t *)(pvalidate->in_proto_data))->http.hdr,
					MIME_HDR_X_NKN_URI, &uri, &urilen, &hattr,
					&hdrcnt);
		if (!rv) {
			TSTR(uri, urilen, uri_name);
			rv = nkn_pseudo_con_t_init(pcon, NULL, HRF_METHOD_GET, 
				(char *)uri_name,
				pvalidate->ns_token);
		}
		else {
			uri = ns_uri2uri(pvalidate->ns_token, (char *)cache_key, 0);
			rv = nkn_pseudo_con_t_init(pcon, NULL, HRF_METHOD_GET, (char *)uri,
				   pvalidate->ns_token);
		}
	}
	else {
		uri = ns_uri2uri(pvalidate->ns_token, (char *)cache_key, 0);
		rv = nkn_pseudo_con_t_init(pcon, NULL, HRF_METHOD_GET, (char *)uri,
				   pvalidate->ns_token);
	}
	if (pvalidate->in_proto_data) {
                int hdr_token;

		org_httpreqcon = (con_t *)(pvalidate->in_proto_data);
                nsc = namespace_to_config(pvalidate->ns_token) ;

                if (nsc && (nsc->http_config->origin.select_method == OSS_HTTP_DEST_IP)) { 
                    hdr_token = MIME_HDR_X_NKN_REQ_REAL_DEST_IP ;
                } else {
                    hdr_token = MIME_HDR_HOST ;
                }
		if (nsc) {
			release_namespace_token_t(pvalidate->ns_token);
		}

		get_known_header(&org_httpreqcon->http.hdr, hdr_token, &client_host,
			&client_host_len, &hattr, &hdrcnt);

		if (nkn_http_is_tproxy_cluster(nsc->http_config)) {
			hdr_token = MIME_HDR_X_NKN_REMAPPED_URI;
			rv = get_known_header(&org_httpreqcon->http.hdr, hdr_token,
				    &remapped_uri, &remapped_uri_len, &hattr, &hdrcnt);
			if (!rv) {
				add_known_header(&pcon->con.http.hdr, hdr_token, remapped_uri, remapped_uri_len);
			}
		}
	}
	else {
		get_known_header(attr_mh, MIME_HDR_X_NKN_CLIENT_HOST, &client_host, 
			 &client_host_len, &hattr, &hdrcnt);
	}
	if (!client_host) {
		client_host_len = 0;
	}
#if 1
	/* If origin server changed by policy, it is stored in mime header
	 * MIME_HDR_X_NKN_ORIGIN_SVR. If this value exists read it and set it 
	 * to nre header also, so that the same origin server is used for
	 * validate request also.
	 */
	rv = get_known_header(attr_mh, MIME_HDR_X_NKN_ORIGIN_SVR, &origin_ip, 
			 &origin_ip_len, &hattr, &hdrcnt);
	if (!rv) {
		add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_ORIGIN_SVR,
			     origin_ip, origin_ip_len);
	}
#endif
	pvalidate->ret_code = MM_VALIDATE_INVALID;
	ocon = om_init_client(cache_key, (char *)"OM_0_0", (con_t *)pcon, 
			      pvalidate->ns_token, 1, 0, 
			      client_host, client_host_len, 
			      &pvalidate->in_client_data, &pvalidate->req_ctx,
			      &allow_retry, 0, 0);
	if( ocon == NULL ) {
		retval = 2;
		glob_om_err_val_init_client++;
		if (allow_retry) {
			glob_om_err_val_init_client_retry++;
			set_task_uncond_taskstart_retry(pvalidate, 0);
		}
		goto error;
	}
	ocon->uol.offset = 0;
	ocon->uol.length = 0;
	ocon->hash_cod_flag = 1;
	ocon->hash_fd = ocon->httpfd = -1;
	NM_FLOW_TRACE(OM_CON_FD(ocon));
	NKN_MUTEX_UNLOCK(&omcon_mutex);
	ocon->uol.cod =
		nkn_cod_dup(pvalidate->in_uol.cod, NKN_COD_ORIGIN_MGR);
	ocon->thistask_datalen = 0;
	ocon->httpreq	= 0;

	pnm=&gnm[OM_CON_FD(ocon)];

        /* Time to make socket to origin server */
	cpcon = (cpcon_t *)ocon->cpcon;
	OM_SET_FLAG(ocon, OMF_DO_VALIDATE);
	ocon->p_validate = pvalidate;
#ifdef RECORD_OCON_IN_TASK
	nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
			     pvalidate->get_task_id, ocon);
#endif
        if(cp_activate_socket_handler(OM_CON_FD(ocon), cpcon) == FALSE) {
		glob_om_err_cp_activate_2_socket++;
	        if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
        	        AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets_ipv6);
                	AO_fetch_and_add1(&glob_cp_tot_closed_sockets_ipv6);
        	} else {
                	AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets);
                	AO_fetch_and_add1(&glob_cp_tot_closed_sockets);
        	}
		retval = 3;
		fd = OM_CON_FD(ocon);
		NM_FLOW_TRACE(fd);
		DBG_LOG(SEVERE, MOD_OM, "cp_activate_socket_handler failed for fd=%d",
				OM_CON_FD(ocon));

		/* we can directly free the memory. */
		shutdown_http_header(&cpcon->http.hdr);
		free(cpcon->http.cb_buf);
		/* BZ 8532: port leaks here in case ot tproxy */
		if (CHECK_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ)) {
		    /* Free the port */
		    cp_release_transparent_port(cpcon);
		}
		cpcon->magic = CPMAGIC_DEAD;
		if (cpcon->oscfg.ip) free(cpcon->oscfg.ip);
		ns_token = cpcon->ns_token;
		free(cpcon);
		ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
#ifdef RECORD_OCON_IN_TASK
		nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
					pvalidate->get_task_id, 0);
#endif
		DBG_LOG(MSG, MOD_OM, "socket error retval = %d\n", retval);
		om_close_conn(ocon);
		nkn_close_fd(fd, NETWORK_FD);
		release_namespace_token_t(ns_token);
		ocon = NULL;
		*pp_ocon = NULL;
		return retval;
        }
	setup_connect_timer(ocon);

	NM_FLOW_TRACE(OM_CON_FD(ocon));
	OM_SET_FLAG(ocon, OMF_WAIT_COND);
	NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
	pthread_mutex_unlock(&pnm->mutex);

	*pp_ocon = ocon;
	return 0;

error:

	if (pcon) {
		nkn_free_pseudo_con_t(pcon);
		pcon = NULL;
	}
#ifdef RECORD_OCON_IN_TASK
	nkn_task_set_private(TASK_TYPE_MEDIA_MANAGER, 
			     pvalidate->get_task_id, 0);
#endif
	assert(ocon == NULL);
	DBG_LOG(MSG, MOD_OM, "socket error retval = %d\n", retval);
	*pp_ocon = NULL;
	return retval;
}

static int setup_connect_timer(omcon_t *ocon)
{
	int rv;
	net_fd_handle_t fhd;
	int retries;

	if (!ocon->oscfg.request_policies->connect_timer_msecs) {
		return 0;
	}

	retries = 1;
	fhd = NM_fd_to_fhandle(OM_CON_FD(ocon));
	while ((rv = net_set_timer(fhd, 
	     		ocon->oscfg.request_policies->connect_timer_msecs, 
		     	TT_ONESHOT)) < 0) {
		if (retries--) {
			glob_om_set_connect_timer_retries++;
		} else {
			break;
		}
	}

	if (!rv) {
	    	OM_SET_FLAG(ocon, OMF_CONNECT_TIMER);
	} else {
	    	DBG_LOG(WARNING, MOD_OM, 
		    	"net_set_timer() failed, rv=%d interval=%d",
		    	rv, 
		    	ocon->oscfg.request_policies->connect_timer_msecs);
		return 1;
	}
	return 0;
}

static int setup_read_timer(omcon_t *ocon)
{
	int rv;
	net_fd_handle_t fhd;
	int retries;

	if (!ocon->oscfg.request_policies->read_timer_msecs) {
		return 0;
	}

	retries = 1;
	fhd = NM_fd_to_fhandle(OM_CON_FD(ocon));
	while ((rv = net_set_timer(fhd, 
			ocon->oscfg.request_policies->read_timer_msecs, 
		     	TT_ONESHOT)) < 0) {
		if (retries--) {
			glob_om_set_read_timer_retries++;
		} else {
			break;
		}
	}

	if (!rv) {
	    	OM_SET_FLAG(ocon, OMF_READ_TIMER);
	} else {
	    	DBG_LOG(WARNING, MOD_OM, 
		    	"net_set_timer() failed, rv=%d interval=%d",
		    	rv, 
		    	ocon->oscfg.request_policies->read_timer_msecs);
		return 1;
	}
	ocon->last_thistask_datalen = ocon->thistask_datalen;
	return 0;
}

static int setup_idle_timeout(omcon_t *ocon)
{

	NM_change_socket_timeout_time(&gnm[OM_CON_FD(ocon)], (http_idle_timeout/2)+1);

	DBG_LOG(WARNING, MOD_OM,
		"setup_idle_timeout(): change idle timeout value for %d from %d to %d seconds"
		" ocon=%p",
		OM_CON_FD(ocon),
		http_cfg_origin_conn_timeout,
		http_idle_timeout,
		ocon);

	return 0;
}

static int cancel_timer(omcon_t *ocon)
{
	int rv;
	int fd;
	net_fd_handle_t fhd;

	fd = OM_CON_FD(ocon);
	if ((fd >= 0) && 
	    OM_CHECK_FLAG(ocon, (OMF_CONNECT_TIMER|OMF_READ_TIMER))) {
		OM_UNSET_FLAG(ocon, (OMF_CONNECT_TIMER|OMF_READ_TIMER));
		fhd = NM_fd_to_fhandle(OM_CON_FD(ocon));
		rv = net_cancel_timer(fhd, TT_ONESHOT);
	} else {
		rv = 0;
	}
	return rv;
}

// Returns: !=0 => Retry not allowed
static int set_unconditional_retry(omcon_t *ocon, int retry_type)
{
#define UNCOND_RETRY_CONNECT    1
#define UNCOND_RETRY_READ       2
#define UNCOND_RETRY_ESCAL      3
#define UNCOND_RETRY_TASKSTART  4

	int retval = 0;
	int timeout = 0;
	int node_map_case;
	int out_delay_ms;
	int max_retries;
	int http_reply_code;
	request_context_t *req_ctx = NULL;
	node_map_context_t *nm_ctx;
	int rv;

	node_map_case = (ocon->oscfg.nsc->http_config->origin.select_method == 
				OSS_HTTP_NODE_MAP);
	if (ocon->p_mm_task &&
	    (ocon->p_mm_task->in_flags & MM_FLAG_UNCOND_NO_RETRY)) {
		retval = 1;
		goto exit;
	} else if (ocon->p_validate &&
		   (ocon->p_validate->in_flags & MM_FLAG_UNCOND_NO_RETRY)) {
		retval = 1;
		goto exit;
	}

	switch(retry_type) {
	case UNCOND_RETRY_CONNECT:
		out_delay_ms = 
			ocon->oscfg.request_policies->connect_retry_delay_msecs;
		http_reply_code = HTTP_REPLY_CODE_CONNECT_TIMEOUT;
		timeout = 1; // timer event
		break;
	case UNCOND_RETRY_READ:
		out_delay_ms =
			ocon->oscfg.request_policies->read_retry_delay_msecs;
		http_reply_code = HTTP_REPLY_CODE_READ_TIMEOUT;
		timeout = 1; // timer event
		break;
	case UNCOND_RETRY_ESCAL:
		out_delay_ms = 10;
		break;
	case UNCOND_RETRY_TASKSTART:
		out_delay_ms = 
			ocon->oscfg.request_policies->connect_retry_delay_msecs;
		break;
	default:
		assert(!"Invalid case");
	}

	if (out_delay_ms) {
		max_retries = OM_UNCOND_RETRY_MSECS / out_delay_ms;
	} else {
		max_retries = OM_UNCOND_RETRY_MSECS;
	}

	if (ocon->p_mm_task) {
		ocon->p_mm_task->err_code = NKN_MM_UNCOND_RETRY_REQUEST;
		ocon->p_mm_task->out_delay_ms =  out_delay_ms;
		ocon->p_mm_task->max_retries = max_retries;

		req_ctx = &ocon->p_mm_task->req_ctx;
	} else if (ocon->p_validate) {
		ocon->p_validate->ret_code = NKN_MM_UNCOND_RETRY_REQUEST;
		ocon->p_validate->out_delay_ms = out_delay_ms;
		ocon->p_validate->max_retries = max_retries;

		req_ctx = &ocon->p_validate->req_ctx;
	}

	if (node_map_case && timeout) {
		// Reflect timeout initiated retry action in request context
		nm_ctx = REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx);
		if (nm_ctx) {
		    rv = (*nm_ctx->my_node_map->origin_reply_action)(
		    		&nm_ctx->my_node_map->ctx,
				req_ctx, NULL, NULL, http_reply_code);
		}
	}
exit:
	return retval;
}

// Setup Origin Failover retry parameters.
static int set_failover_retry(omcon_t *ocon, int retry_type) {
#define OS_FAILOVER_RETRY_CONNECT    1
#define OS_FAILOVER_RETRY_DELAY      2

	int retval = 0;
	int out_delay_ms;
	int max_retries;

	switch(retry_type) {
	case OS_FAILOVER_RETRY_CONNECT:
		out_delay_ms = 1; // 1 msec delay
		max_retries = nkn_max_domain_ips - 1;
		break;
	case OS_FAILOVER_RETRY_DELAY:
		// For future enhancments to handle delayed retries.
	default:
		break;
	}

	if (ocon->p_mm_task) {
		ocon->p_mm_task->err_code = NKN_MM_UNCOND_RETRY_REQUEST;
		ocon->p_mm_task->out_delay_ms =  out_delay_ms;
		ocon->p_mm_task->max_retries = max_retries;
	} else if (ocon->p_validate) {
		ocon->p_validate->ret_code = NKN_MM_UNCOND_RETRY_REQUEST;
		ocon->p_validate->out_delay_ms = out_delay_ms;
		ocon->p_validate->max_retries = max_retries;
	}

	DBG_LOG(MSG, MOD_HTTP,
		"set_failover_retry: setup Origin Failover retry parameters for ocon:%p, fd=%d"
		"out_delay_ms=%d, max_retries=%d",
		ocon, OM_CON_FD(ocon), out_delay_ms, max_retries);

	return retval;
}

// Construct the cache index based on response header values.
// Returns: 0  => Success
//			-1 => Failure
static int 
resp_header_cache_index(omcon_t *ocon) {
	int ret = 0;
	MD5_CTX ctx;
	unsigned char hdr_cksum[MD5_DIGEST_LENGTH];
	const char *header_value, *in_header;
	int i, rv, header_len, cl_len, hdrcnt, in_header_len;
	uint32_t attr;
	const cooked_data_types_t *pckd;
	mime_hdr_datatype_t dtype;
	int header_token;
	uint64_t content_length;

	// Initialize MD5 context.
	MD5_Init(&ctx);

	// Fetch configured response header values from the reply.
	for (i=0;i<ocon->oscfg.nsc->http_config->cache_index.headers_count;i++) {
		if (!ocon->oscfg.nsc->http_config->cache_index.include_header_names[i]) {
			continue;
		}

		in_header = ocon->oscfg.nsc->http_config->cache_index.include_header_names[i];
		in_header_len = strlen(ocon->oscfg.nsc->http_config->cache_index.include_header_names[i]);

		rv = mime_hdr_strtohdr(MIME_PROT_HTTP, 
				in_header, 
				in_header_len, 
				&header_token);

		if (rv) {
			rv = get_unknown_header(&ocon->phttp->hdr,
					in_header,
					in_header_len,
					&header_value, &header_len,
					&attr);
			if (rv) {
				DBG_LOG(MSG, MOD_HTTP,
					"get_unknown_header "
					"failed, header=\"%s\", rv=%d, ocon=%p",
					in_header, rv, ocon);
				glob_tunnel_resp_cache_index_get_unknown_hdr_err++;
				ret = -1;
				goto done;
			}
			MD5_Update(&ctx, header_value, header_len);
			continue;
		}

		// Quick sanity to determine if the configured header is present 
		// in the response.
		if (is_known_header_present(&ocon->phttp->hdr, header_token)) {
			switch (header_token)
			{
				case MIME_HDR_CONTENT_LENGTH:
				{
					rv = get_cooked_known_header_data(&ocon->phttp->hdr,
							MIME_HDR_CONTENT_LENGTH,
							&pckd, &cl_len, &dtype);
					if (!rv && (dtype == DT_SIZE)) {
						content_length = pckd->u.dt_size.ll;
					} else {
						glob_tunnel_resp_cache_index_get_cl_header_err++;
						ret = -1;
						goto done;
					}
					MD5_Update(&ctx, &content_length, sizeof(content_length));
					break;
				} 

				default:
				{
					rv = get_known_header(&ocon->phttp->hdr, header_token,
							&header_value,
							&header_len,
							&attr, &hdrcnt);
					if (rv) {
						DBG_LOG(MSG, MOD_HTTP,
							"get_known_header "
							"failed, header=\"%s\", rv=%d, ocon=%p",
							in_header, rv, ocon);
						glob_tunnel_resp_cache_index_get_known_hdr_err++;
						ret = -1;
						goto done;
					}
					MD5_Update(&ctx, header_value, header_len);
					break;
				}
			} /* end switch */
			continue;
		} else {
			DBG_LOG(MSG, MOD_HTTP,
				"The known header token(%d) is not present in the response",
				header_token);
			glob_tunnel_resp_cache_index_no_known_hdr++;	
			ret = -1;
			goto done;
		}
	} /* end for */

	MD5_Final(hdr_cksum, &ctx);
	memcpy(&ocon->p_mm_task->out_proto_resp.u.OM_cache_idx_data.cksum[0], &hdr_cksum[0], MD5_DIGEST_LENGTH);
done:
	return ret;
}

// Construct the cache index based on response header values.
// Returns: 0  => Success
//         -1  => Failure
static int
resp_data_cache_index(omcon_t *ocon) {

#define RESP_IDX_OBJ_MAX_LEN (32768)

	int ret = 0;
	unsigned char resp_body_data[RESP_IDX_OBJ_MAX_LEN];
	unsigned char data_cksum[MD5_DIGEST_LENGTH];
	unsigned char total_cksum[MD5_DIGEST_LENGTH*2];
	nkn_iovec_t *iov;
	cpcon_t *cpcon;

	MD5_CTX ctx;
	int offset = 0;
	int copylen = 0;
	cpcon = ocon->cpcon;

	// Get the offset within repsonse buffer to the start of response body.
	offset = cpcon->http.cb_bodyoffset;

	// Set copylen to the right size.
	if (ocon->content_length < ocon->oscfg.nsc->http_config->cache_index.checksum_bytes) {
		copylen = ocon->content_length;
	} else {
		copylen = ocon->oscfg.nsc->http_config->cache_index.checksum_bytes;
	}

	// Initialize response data. This would also take care of padding the
	// response data to configured object <num-bytes>.
	memset(&resp_body_data[0], 0, ocon->oscfg.nsc->http_config->cache_index.checksum_bytes);

	// Binary data copy of response data from first iov vector index.
	iov = &ocon->phttp->content[0];
	memcpy(&resp_body_data[0], iov->iov_base, copylen);

	// Calculate the MD5 checksum for the configured object <num-bytes> size.
	MD5_Init(&ctx);
	MD5_Update(&ctx, &resp_body_data[0], ocon->oscfg.nsc->http_config->cache_index.checksum_bytes);
	MD5_Final(data_cksum, &ctx);

	// If both repsonse header + response data checksum is configured then 
	// append both the checksum values (16 + 16 = 32 bytes).
	// i.e, checksum string length (after hex value to char conversion) is
	// 32 + 32 = 64 chars.
	if (ocon->oscfg.nsc->http_config->cache_index.include_header && 
		ocon->oscfg.nsc->http_config->cache_index.include_object) {
		memcpy(&total_cksum[0], &ocon->p_mm_task->out_proto_resp.u.OM_cache_idx_data.cksum[0], MD5_DIGEST_LENGTH);
		memcpy(&total_cksum[MD5_DIGEST_LENGTH], &data_cksum[0], MD5_DIGEST_LENGTH);
		memcpy(&ocon->p_mm_task->out_proto_resp.u.OM_cache_idx_data.cksum[0], total_cksum, MD5_DIGEST_LENGTH*2);
	} else {
		memcpy(&ocon->p_mm_task->out_proto_resp.u.OM_cache_idx_data.cksum[0], data_cksum, MD5_DIGEST_LENGTH);
	}
	return ret;
}

// Returns: 0 => Retry allowed
static int set_uncond_connect_retry(omcon_t *ocon)
{
	return set_unconditional_retry(ocon, UNCOND_RETRY_CONNECT);
}

// Returns: 0 => Retry allowed
static int set_uncond_read_retry(omcon_t *ocon)
{
	return set_unconditional_retry(ocon, UNCOND_RETRY_READ);
}

// Returns: 0 => Retry allowed
static int set_uncond_escal_retry(omcon_t *ocon)
{
    return set_unconditional_retry(ocon, UNCOND_RETRY_ESCAL);
}

// set_task_uncond_connect_retry() - Set unconditional connect retry on 
//				     get/validate task.
// Returns: 0 => Retry allowed
//
static int set_task_uncond_taskstart_retry(void *taskptr, int is_get_task)
{
	omcon_t dummy_ocon;
	http_config_t *phttp_config;
	int retval;
	int rv;

	memset(&dummy_ocon, 0, sizeof(dummy_ocon));

	if (is_get_task) {
	    dummy_ocon.p_mm_task = (MM_get_resp_t *)taskptr;
	    dummy_ocon.oscfg.ns_token = dummy_ocon.p_mm_task->ns_token;
	} else {
	    dummy_ocon.p_validate = (MM_validate_t *)taskptr;
	    dummy_ocon.oscfg.ns_token = dummy_ocon.p_validate->ns_token;
	}

	dummy_ocon.oscfg.nsc = namespace_to_config(dummy_ocon.oscfg.ns_token);
	if (dummy_ocon.oscfg.nsc) {
	    phttp_config = dummy_ocon.oscfg.nsc->http_config;

	    if (phttp_config) {
		dummy_ocon.oscfg.policies = &phttp_config->policies;
		dummy_ocon.oscfg.request_policies = 
		    &phttp_config->request_policies;

	    	if (phttp_config->request_policies.connect_timer_msecs) {
		    rv = set_unconditional_retry(&dummy_ocon, 
		    				 UNCOND_RETRY_TASKSTART);
		    if (rv) {
	    		retval = 1; // Not initiated
		    }
		} else {
		    retval = 2; // Not initiated
		}
	    } else {
	    	retval = 3; // Not initiated
	    }
	    release_namespace_token_t(dummy_ocon.oscfg.ns_token);
	} else {
	    retval = 4; // Not initiated
	}
	return retval;
}

static int set_failover_connect_retry(omcon_t *ocon)
{

	return set_failover_retry(ocon, OS_FAILOVER_RETRY_CONNECT);
}

static int set_failover_delay_retry(omcon_t *ocon)
{

	return set_failover_retry(ocon, OS_FAILOVER_RETRY_DELAY);
}

#undef UNCOND_RETRY_CONNECT
#undef UNCOND_RETRY_READ
#undef UNCOND_RETRY_ESCAL
#undef UNCOND_RETRY_TASKSTART
#undef RESP_IDX_OBJ_MAX_LEN
#undef OS_FAILOVER_RETRY_CONNECT
#undef OS_FAILOVER_RETRY_DELAY

int
parse_location_header(const char *uri, int urilen,
		char **host, int *hostlen,
		char **port, int *portlen,
		char **_uri, int *_urilen)
{
    char *p = NULL;
    char *p_start = NULL;
    int len = urilen;

    if (uri == NULL || urilen <= 0) {
	return -1; // tunnel the response
    }
    *host = NULL;
    *port = NULL;
    *hostlen = 0;
    *portlen = 0;

    p_start = (char *) uri;
    p = p_start;
    /* Check if http */

    // form-1: http://<domain>:<port>/<url>
    // form-2: /<url>
    // form-3: <scheme>//<domain>:<port>/<url>
    // form-4: <scheme>//<domain>/<url>

    if ((p[0] != '/') && !(strncmp(p, "http://", 7) == 0)) {
	return -2; // unknown scheme. tunnel response
    }

    if (p[0] != '/') {
	p += 7; // skip scheme
	len -=7;
    }	
    /* grab hostname */
    p_start = p;

    /* Check if the host header is in ipv6 format. */
    /*
     * Ref: RFC: 2732
     *   host          = hostname | IPv4address | IPv6reference
     *   ipv6reference = "[" IPv6address "]"
     */
    if (*p == '[') {
	p++ ; len--; // skip starting '[' char.
	p_start = p ;
        while ((*p != ']') && (len > 0)) {p++; len--;}
	*hostlen = (p - p_start) ;
	p++ ; len--; // skip ending ']' char.
    } else {
        while ((*p != '/') && (*p != ':') && (len > 0)) {p++; len--;}
        *hostlen = (p - p_start);
    }
    *host = (char *) p_start;

    /* possibly port is available */
    if (*p == ':') {
	p++;
	len--;
	p_start = p;
	while ((*p != '/') && (*p != '\r') && (*p != '\n') && (len > 0)) {
		p++; len--;
	}
	*port = (char *) p_start;
	*portlen = (p - p_start);
    }

    /* grab uri - dirname */
    if (uri != NULL) {
	    p_start = p;
	    *_uri = p_start;
	    while((*p != '\r') && (*p != '\n') && len > 0) {p++; len--;}
	    *_urilen = (p - p_start);
    }

    return 0;
}

static int parse_uri_2(const char *uri, int urilen,
		char **dir, int *dirlen,
		char **file, int *filelen)
{
    char *p_start = (char *) uri;
    char *pend;

    *dir = NULL;
    *file = NULL;
    *dirlen = 0;
    *filelen = 0;
    if (uri == NULL)
	return -1;
    // scan for dirname and then filename
    pend = p_start + urilen; //move to end of the string.
    while (*pend != '/' && pend != p_start) {
	(*filelen)++;
	pend--;
    }
    *file = pend + 1;
    (*filelen) --;
    if (*filelen < 0) *filelen = 0, *file = NULL;

    // get dir
    *dir = (char*) uri;
    *dirlen = (pend - uri);

    return 0;
}


/* Handle a 302 redirect response
 * - parse the Location header and setup some internal headers
 * - Sets up the REDIRECT_302 flag in the http.flags.
 *
 * Returns:
 * 0 - Success	// NEVER RETURNED - PLACEHOLDER ONLY
 * -1 - tunnel
 * -2 - close connection
 */
static int
process_302_redirect_response(omcon_t *ocon)
{
	int rv;
	const char *data;
	char *host, *port;
	int hostlen, portlen;
	char *_uri;
	u_int32_t attrs;
	int hdrcnt;
	int datalen;
	int urilen;

	/* Tunnel response if !reverse_proxy or if CLI not set
	 * to handle it.
	 */
	if ((ocon->oscfg.nsc->http_config->policies.redirect_302 == NKN_FALSE)||
	    ((ocon->oscfg.nsc->http_config->origin.select_method != 
	    				OSS_HTTP_CONFIG) &&
	     (ocon->oscfg.nsc->http_config->origin.select_method != 
	     				OSS_HTTP_PROXY_CONFIG)) ) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NO_302_NOT_RPROXY;
		return -1;
	}

	/* Check whether any cookies are present in this response.
	 * If so, tunnel it since client may want the cookie
	 * in subsequent requests */
	if (mime_hdr_is_known_present(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE) ||
	    mime_hdr_is_known_present(&ocon->phttp->hdr, MIME_HDR_SET_COOKIE2)) {
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_SET_COOKIE;
		return -1;
	}

	rv = get_known_header(&ocon->phttp->hdr, MIME_HDR_LOCATION,
			&data, &datalen,
			&attrs, &hdrcnt);
	if (rv) {
		/* 302 without Location header.. tunnel */
		ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_NO_LOC_HDR;
		return -1;
	} else {

		ocon->p_mm_task->err_code = NKN_MM_REDIRECT_REQUEST;

		rv = parse_location_header(data, datalen,
				&host, &hostlen,
				&port, &portlen,
				&_uri, &urilen);
		if (rv) {
			ocon->httpreqcon->http.tunnel_reason_code = NKN_TR_RES_LOC_HDR_ERR;
			return -1;
		}

		if (host && hostlen > 0) {
			add_known_header(&ocon->httpreqcon->http.hdr,
					MIME_HDR_X_REDIRECT_HOST, host, hostlen);
		} 
		if((strncmp(data, "http://", 7) == 0) && 
			is_known_header_present(&ocon->httpreqcon->http.hdr,
					MIME_HDR_X_REDIRECT_PORT)) {
			delete_known_header(&ocon->httpreqcon->http.hdr,
					MIME_HDR_X_REDIRECT_PORT);
		}
		if (port && portlen > 0) {
			add_known_header(&ocon->httpreqcon->http.hdr,
					MIME_HDR_X_REDIRECT_PORT, port, portlen);
		} 
		add_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_X_REDIRECT_URI, _uri, urilen);
		add_known_header(&ocon->httpreqcon->http.hdr, MIME_HDR_X_LOCATION, data, datalen);
		// Resend request to new location!!
		AO_fetch_and_add1(&(glob_om_redirect_302));
		return -2;
	}

	/* ----- NO RETURN STATEMENT HERE. HANDLE ALL CASES IN THE IF-ELSE ---- */

}

/*
 * End of om_network.c
 */

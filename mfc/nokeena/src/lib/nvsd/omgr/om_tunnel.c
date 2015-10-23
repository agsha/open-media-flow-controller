#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <netinet/tcp.h>
#include <openssl/hmac.h>

#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "nkn_util.h"
#include "network.h"
#include "server.h"
#include "nkn_cfg_params.h"
#include "nkn_lockstat.h"
#include "nkn_om.h"
#include "om_defs.h"
#include "om_fp_defs.h"
#include "nkn_tunnel_defs.h"
#include "nkn_ssl.h"

#define F_FILE_ID	LT_OM_TUNNEL

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

NKNCNT_DEF(fp_event_replacement, int32_t, "", "event replacement")
NKNCNT_DEF(fp_event_insert, int32_t, "", "event insert")
NKNCNT_DEF(fp_event_replace_ignored_2, int32_t, "", "event replace ignored 2")
NKNCNT_DEF(fp_event_replace_ignored_3, int32_t, "", "event replace ignored 3")
NKNCNT_DEF(tot_size_to_os_by_tunnel, int64_t, "", "size to os by tunnel")
NKNCNT_DEF(tot_size_from_tunnel, AO_t, "", "size from forward proxy")
NKNCNT_DEF(fp_tot_tunnel, int64_t, "", "Tot CP connection")
NKNCNT_DEF(fp_cur_open_tunnel, AO_t, "", "Cur CP connection")
NKNCNT_DEF(fp_err_make_tunnel, int64_t, "", "Failed to make tunnel")
NKNCNT_DEF(fp_err_activate_cp, int64_t, "", "Failed to activate tunnel")
NKNCNT_DEF(fp_err_internal_closed, int64_t, "", "internal closed")
NKNCNT_DEF(fp_err_internal_closed_1, int64_t, "", "internal closed 1");
NKNCNT_DEF(fp_err_internal_closed_2, int64_t, "", "internal closed 2")
NKNCNT_DEF(fp_tunnel_keep_alive, int64_t, "", "tunnel keep alive");
NKNCNT_DEF(fp_err_activate_1, int64_t, "", "Bad error on activate, fd leak")
NKNCNT_DEF(fp_close_proxy_trylock_err, int64_t, "", "error leak socket")
NKNCNT_DEF(fp_client_socket_err, int64_t, "", "client socket err")
NKNCNT_DEF(fp_tunnel_os_failover_retry, uint64_t, "", "Tunnel path Origin Failover retries")
NKNCNT_DEF(fp_tcp_half_close, uint64_t, "", "Tunnel TCP Half Close")
NKNCNT_DEF(fp_err_epollerr_case1, uint64_t, "", "Tunnel epoll err case 1")
NKNCNT_DEF(fp_err_epollerr_case2, uint64_t, "", "Tunnel epoll err case 2")
NKNCNT_DEF(fp_aws_auth_cnt, uint64_t, "", "aws authroization count")

NKNCNT_EXTERN(ssl_fid_index, AO_t)
NKNCNT_EXTERN(tot_client_send, AO_t)
NKNCNT_EXTERN(cp_tot_cur_open_sockets, AO_t)
NKNCNT_EXTERN(cp_tot_closed_sockets, AO_t)
NKNCNT_EXTERN(cp_tot_cur_open_sockets_ipv6, AO_t)
NKNCNT_EXTERN(cp_tot_closed_sockets_ipv6, AO_t)
extern AO_t glob_http_tot_well_finished;
NKNCNT_EXTERN(client_recv_tot_bytes, AO_t)
NKNCNT_EXTERN(origin_send_tot_bytes, AO_t)
NKNCNT_EXTERN(client_send_tot_bytes, AO_t)
NKNCNT_EXTERN(reset_dscp_responses, AO_t)
NKNCNT_EXTERN(aws_auth_hdr_mismatch, uint64_t);

extern int nkn_max_domain_ips;
long long glob_http_mutex_trylock_ser_to_cli = 0;
long long glob_http_mutex_trylock_cli_to_ser = 0;
long long glob_http_mutex_trylock_header_rcv = 0;
long long glob_http_mutex_trylock_body_rcv = 0;
long long glob_http_mutex_trylock_close_proxy = 0;
long long glob_cp_mutex_trylock_epollin = 0,
	  glob_cp_mutex_trylock_epollout = 0,
	  glob_cp_mutex_trylock_epollerr = 0;

#define CALLED_FROM_CP	1
#define CALLED_FROM_EPOLL	2

/* strcasestr declared in /usr/include/string.h */
/* extern char *strcasestr(const char *haystack, const char *needle); */

/* DNS lookup */
extern int auth_helper_get_dns(con_t*, uint32_t,uint32_t,char*,int);
extern void cleanup_http_cb(con_t * con);
extern void re_register_http_socket(int fd, void * con);
extern int apply_cc_max_age_policies(origin_server_policies_t *origin_policies,
                                     mime_header_t *hdr, int fd);
static int apply_policy_and_forward_client_request(fpcon_t *fpcon);
static int forward_client_request(fpcon_t *fpcon);

static int fp_forward_client_to_server(fpcon_t * fpcon, char * p, off_t * totlen);
static int fp_forward_server_to_client(fpcon_t * fpcon, char * p, int * totlen);
static void fp_internal_closed(fpcon_t * fpcon, int close_httpconn, int called_from);
static int fp_create_new_cpcon(fpcon_t * fpcon, con_t * httpcon);
static int fp_close_proxy_socket(void * fpcon, int close_httpconn);
static int fphttp_epollin(int fd, void * private_data);
static int fphttp_epollout(int fd, void * private_data);
static int fphttp_epollerr(int fd, void * private_data);
static int fphttp_epollhup(int fd, void * private_data);

int fp_validate_client_request(con_t *con);
int fp_validate_auth_header(con_t *con);
int fp_get_aws_authorization_hdr_value(con_t *con, char *hdr_value);

extern int http_build_res_common(http_cb_t * phttp, int status_code, 
				int errcode, mime_header_t * presponse_hdr);
extern int dns_hash_and_mark(char *domain, ip_addr_t ip, int *found);

/* ==================================================================== */
/*
 * Forward Proxy Socket Close Event Queue
 * The reason to introduce this queue is to avoid cp_socket to call fp_internal_closed() directly.
 */
static struct fp_event_q_t {
        struct fp_event_q_t * next;
        int fd;
        op_type op;
        int close_httpconn;
} ** g_fpeventq;
static pthread_mutex_t fsq_mutex[MAX_EPOLL_THREADS] =
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
static void fp_add_event(int fd, op_type op, int close_httpconn);
void check_out_fp_queue(int num_of_epoll);

void fp_init(void);
void fp_init(void)
{
        int i;

        g_fpeventq = (struct fp_event_q_t **)(nkn_malloc_type(
			sizeof(struct fp_event_q_t *) * MAX_EPOLL_THREADS, mod_om_fpevent_q));
        for (i=0; i<MAX_EPOLL_THREADS; i++) {
            g_fpeventq[i] = NULL;
        }

        return;
}

/*
 * socket_close_event_queue management function
 * fd is the httpcon->fd
 *
 * caller should own gnm[httpcon->fd].mutex lock
 */
static void fp_add_event(int fd, op_type op, int close_httpconn) {
	fpcon_t * fpcon;
	struct fp_event_q_t * psceq;
	int num;
	struct network_mgr * pnm;

	//printf("fp_add_event: fd=%d op_type=%d\n", fd, op);

	if(fd==-1) return;
	NM_EVENT_TRACE(fd, FT_FP_ADD_EVENT + op - OP_CLOSE);
	pnm = &gnm[fd];
	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {

		fpcon = (fpcon_t *)(gnm[fd].private_data);

		/*
		 * event replacement
		*/
		if(CHECK_FPCON_FLAG(fpcon, FPF_IN_EVENT_QUEUE)) {
			NM_EVENT_TRACE(fd, FT_FP_ADD_EVENT_REPLACE);
			glob_fp_event_replacement++;
			/*
			 * if we are already in the event queue and
			 * this op==OP_CLOSE
			 * we should replace old op by OP_CLOSE
			 */
			if (op==OP_CLOSE) {
				num = pnm->pthr->num;
				pthread_mutex_lock(&fsq_mutex[num]);
				psceq = g_fpeventq[num];
				while(psceq) {
					if(psceq->fd == fd) {
						psceq->op = op;
						psceq->close_httpconn = close_httpconn;
						NM_EVENT_TRACE(fd, FT_FP_ADD_REPLACE_WITH_CLOSE);
						/* 
						 * return only if the event was replaced.
						 */
						pthread_mutex_unlock(&fsq_mutex[num]);
						return;
					}
					psceq = psceq->next;
				}
				pthread_mutex_unlock(&fsq_mutex[num]);
				glob_fp_event_insert++;
			}
			else {
				glob_fp_event_replace_ignored_2++;
				return;
			}
		}

		/*
	 	 * add a new event
		 */
		psceq = (struct fp_event_q_t *)nkn_malloc_type(
				sizeof(struct fp_event_q_t), mod_fp_fpevent_q);
		if(!psceq) {
			glob_fp_event_replace_ignored_3++;
			return;
		}

		SET_FPCON_FLAG(fpcon, FPF_IN_EVENT_QUEUE);

		/* add into which network thread */
		num = pnm->pthr->num;
		pthread_mutex_lock(&fsq_mutex[num]);

		psceq->next = g_fpeventq[num];
		psceq->fd = fd;
		psceq->close_httpconn = close_httpconn;
		psceq->op = op;
		g_fpeventq[num] = psceq;
		pthread_mutex_unlock(&fsq_mutex[num]);
	}
	return;
}

/* 
 * this function should be called only from fp_internal_closed() 
 * fd is httpcon->fd
 */
static void fp_remove_event(int fd);
static void fp_remove_event(int fd)
{
	struct fp_event_q_t * psceq, * psceq_del;
	int num;
	struct network_mgr * pnm;

	NM_EVENT_TRACE(fd, FT_FP_REMOVE_EVENT);
	pnm = &gnm[fd];
	num = pnm->pthr->num;
	pthread_mutex_lock(&fsq_mutex[num]);

	psceq = g_fpeventq[num];
	if(psceq == NULL) {
                /*
                 * BUG 5009 (See BUG 3664)
                 * This could happen.
                 */
		pthread_mutex_unlock(&fsq_mutex[num]);
		return;
	}
	if(psceq->fd == fd) {
		g_fpeventq[num] = psceq->next;
		free(psceq);
		pthread_mutex_unlock(&fsq_mutex[num]);
		return;
	}

	while(psceq->next) {
		if(psceq->next->fd == fd) {
			psceq_del = psceq->next;
			psceq->next = psceq_del->next;
			free(psceq_del);
			break;
		}
		psceq = psceq->next;
	}
	pthread_mutex_unlock(&fsq_mutex[num]);
	return;
}

/*
 * This function is running on network thread.
 */
void check_out_fp_queue(int num_of_epoll) {
	struct fp_event_q_t * psceq;
	struct network_mgr * pnm;
	fpcon_t * fpcon;

again:
	pthread_mutex_lock(&fsq_mutex[num_of_epoll]);
	while (g_fpeventq[num_of_epoll]) {
		psceq = g_fpeventq[num_of_epoll];
		g_fpeventq[num_of_epoll] = g_fpeventq[num_of_epoll]->next;
		pthread_mutex_unlock(&fsq_mutex[num_of_epoll]);// Need release now

		pnm = &gnm[psceq->fd];
		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);
		if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
			NM_EVENT_TRACE(psceq->fd, FT_CHECK_OUT_FP_QUEUE + psceq->op - OP_CLOSE);
			fpcon = (fpcon_t *)(gnm[psceq->fd].private_data);
			if (fpcon && (fpcon->httpcon_fd_incarn == gnm[psceq->fd].incarn) && 
				(fpcon->magic == FPMAGIC_ALIVE)) {
			UNSET_FPCON_FLAG(fpcon, FPF_IN_EVENT_QUEUE);
			switch(psceq->op) {
			case OP_CLOSE:
				fp_internal_closed(fpcon, psceq->close_httpconn, CALLED_FROM_CP);
				break;
			case OP_EPOLLIN:
				fphttp_epollin(psceq->fd, gnm[psceq->fd].private_data);
				break;
			case OP_EPOLLOUT:
				fphttp_epollout(psceq->fd, gnm[psceq->fd].private_data);
				break;
			case OP_EPOLLERR:
				fphttp_epollerr(psceq->fd, gnm[psceq->fd].private_data);
				break;
			default:
				assert(0);
			}
			}
			//else {
			//	assert(0);
			//}
		}
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);

		free(psceq);
		goto again;
	}
	pthread_mutex_unlock(&fsq_mutex[num_of_epoll]);
}


/* ==================================================================== */
/*
 * We are in the tunnel code to forward the data from client to server.
 * It is very important to acquire both lockers first.
 */

static int fphttp_epollin(int fd, void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	char * p;
	int ret, rlen;
	off_t len, orglen, content_len;
	struct network_mgr * pnm_cp;
	struct tcp_info tcp_con_info;

	NM_EVENT_TRACE(fd, FT_FPHTTP_EPOLLIN);
	//printf("fphttp_epollin: fd=%d cp_sockfd=%d\n", fd, fpcon->cp_sockfd);
	pnm_cp = &gnm[fpcon->cp_sockfd];
	if(pthread_mutex_trylock(&pnm_cp->mutex) != 0) {
		/* 
		 * if we could not get peer mutex lock.
		 * add an event. and callback later on.
		 */
		glob_cp_mutex_trylock_epollin++;
		fp_add_event(fd, OP_EPOLLIN, 0);
		return TRUE;
	}
	NM_TRACE_LOCK(pnm_cp, LT_OM_TUNNEL);
	if( !NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) ||
	    (pnm_cp->incarn != fpcon->cp_sockfd_incarn) ) {
		/* peer socket is not in USE. why? */
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		fp_internal_closed(fpcon, TRUE, CALLED_FROM_CP);
		return TRUE;// We already closed
	}
	if (CHECK_CON_FLAG(httpcon, CONF_CANCELD)) {
		fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		return TRUE;
	}
	if (CHECK_CON_FLAG(httpcon, CONF_BLOCK_RECV)) {
		/*
		 * BUG 6290: for half close connection,
		 * we should continue to send more data to client
		 */
		//fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
		NM_del_event_epoll(fd);
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		return TRUE;
	}
	/*
	 * Got both socket mutex lock.
	 */
	rlen = nkn_check_http_cb_buf(&httpcon->http);
	len = httpcon->http.cb_totlen - httpcon->http.cb_offsetlen;
	if (rlen == -1) {
		fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		return TRUE;
	} else if (rlen > 0) {
		p = &httpcon->http.cb_buf[httpcon->http.cb_totlen];
		ret = recv(fd, p, rlen, MSG_DONTWAIT);
		if (len == 0 && ret <= 0) {
			DBG_LOG(MSG, MOD_TUNNEL, "recv(fd=%d) ret=%d errno=%d ", fd, ret, errno);
			if (ret==-1 && errno==EAGAIN) {
				NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
				pthread_mutex_unlock(&pnm_cp->mutex);
				return TRUE;
			}
			if (ret == 0 &&
			    CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE) &&
			    CHECK_HTTP_FLAG(&httpcon->http, HRF_TPROXY_MODE)) {
				ret = NM_get_tcp_info(fd, &tcp_con_info);
				if (ret == 0 && tcp_con_info.tcpi_state == TCP_CLOSE_WAIT) {
					glob_fp_tcp_half_close++;
					NM_del_event_epoll(fd);
					shutdown(fd, SHUT_RD);
					NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
					pthread_mutex_unlock(&pnm_cp->mutex);
					return TRUE;
				}
			}
			SET_CON_FLAG(httpcon, CONF_RECV_ERROR);
			CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
			fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
			NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm_cp->mutex);
			return TRUE;
		}
		if (ret > 0) {
			httpcon->http.cb_totlen += ret;
			AO_fetch_and_add(&glob_client_recv_tot_bytes, ret);
		}
	}

	p = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
	len = httpcon->http.cb_totlen - httpcon->http.cb_offsetlen;
	if (CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE)) {
		content_len = httpcon->http.content_length + 
				    httpcon->http.http_hdr_len - 
					    httpcon->http.cb_bodyoffset;
		if (content_len == 0) {
			NM_del_event_epoll(fd);
			NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm_cp->mutex);
			return TRUE;
		}
		if (len > content_len) len = content_len;
	}
	orglen = len;
	ret = fp_forward_client_to_server(fpcon, p, &len);
	httpcon->http.cb_offsetlen += orglen - len;
	httpcon->http.cb_bodyoffset += orglen - len;
	if( ret == FALSE ) {
		CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
		fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		return TRUE;
	}
	if(httpcon->http.cb_offsetlen == httpcon->http.cb_totlen) {
		/* 
		 * All data has been sent out 
		 * request size is too big, most likely it is a POST request
		 * reset the offsetlen and totlen.
		 */
		httpcon->http.cb_offsetlen = 0;
		httpcon->http.cb_totlen = 0;
	}

	NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm_cp->mutex);

	/* No need to call any function here */
	return TRUE;
}

static int fphttp_epollout(int fd, void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm_cp;

	NM_EVENT_TRACE(fd, FT_FPHTTP_EPOLLOUT);
	pnm_cp = &gnm[fpcon->cp_sockfd];
	if(pthread_mutex_trylock(&pnm_cp->mutex) != 0) {
		/* 
		 * if we could not get peer mutex lock.
		 * add an event. and callback later on.
		 */
		glob_cp_mutex_trylock_epollout++;
		fp_add_event(fd, OP_EPOLLOUT, 0);
		return TRUE;
	}
	NM_TRACE_LOCK(pnm_cp, LT_OM_TUNNEL);
	if( !NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) ||
	    (pnm_cp->incarn != fpcon->cp_sockfd_incarn) ) {
		/* peer socket is not in USE. why? */
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		fp_internal_closed(fpcon, TRUE, CALLED_FROM_CP);
		return TRUE;
	}

	/*
	 * Got both socket mutex lock.
	 */
	NM_add_event_epollin(httpcon->fd);
	cp_add_event(fpcon->cp_sockfd, OP_EPOLLOUT, NULL);

	NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm_cp->mutex);
	return TRUE;
}

static int fphttp_epollerr(int fd, void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t *httpcon;
	struct network_mgr * pnm_cp;

	NM_EVENT_TRACE(fd, FT_FPHTTP_EPOLLERR);

	// protection code for PR 904725. 
	// Not sure how this can happen.. Not able to recreate 
	// so protection code added for now.
	if ((fpcon == NULL) || 
		(fpcon->magic != FPMAGIC_ALIVE)) {
		// already fpcon freed 
		// remove fd from epoll
		NM_del_event_epoll(fd);
		glob_fp_err_epollerr_case1 ++;
		return TRUE;	
	}
	httpcon = fpcon->httpcon;
	pnm_cp = &gnm[fpcon->cp_sockfd];
	if(pthread_mutex_trylock(&pnm_cp->mutex) != 0) {
		/* 
		 * if we could not get peer mutex lock.
		 * add an event. and callback later on.
		 */
		glob_cp_mutex_trylock_epollerr++;
		fp_add_event(fd, OP_EPOLLERR, 0);
		return TRUE;
	}

	// protection code for PR 904725. 
	// Not sure how this can happen.. Not able to recreate 
	// so protection code added for now.
	if (httpcon != NULL && 
		httpcon->magic == CON_MAGIC_USED) {
		CLEAR_HTTP_FLAG(&fpcon->httpcon->http, 
				HRF_CONNECTION_KEEP_ALIVE);
	} else {
		NM_del_event_epoll(fd);
		glob_fp_err_epollerr_case2 ++;
	}

	NM_TRACE_LOCK(pnm_cp, LT_OM_TUNNEL);
	if( !NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) ||
	    (pnm_cp->incarn != fpcon->cp_sockfd_incarn) ) {
		/* peer socket is not in USE. why? */
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		fp_internal_closed(fpcon, TRUE, CALLED_FROM_CP);
		return TRUE;
	}

	DBG_LOG(MSG, MOD_TUNNEL, "fphttp_epollerr: fd=%d", fd);

	fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
	NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm_cp->mutex);
	return TRUE;
}


static int fphttp_epollhup(int fd, void * private_data)
{
	NM_EVENT_TRACE(fd, FT_FPHTTP_EPOLLHUP);
	return fphttp_epollerr(fd, private_data);
}

#if 0
static int fphttp_timeout(int fd, void * private_data, double timeout);
static int fphttp_timeout(int fd, void * private_data, double timeout)
{
	fpcon_t *fpcon = (fpcon_t *)private_data;

	UNUSED_ARGUMENT(fd);
	UNUSED_ARGUMENT(timeout);

	fp_internal_closed(fpcon, TRUE, CALLED_FROM_EPOLL);
	return TRUE;
}
#endif

/*
 * This function is called by connection poll function.
 * when calling this API, gnm[cpsockfd].mutex has already locked
 */
int cp_tunnel_client_data_to_server(void * private_data);
int cp_tunnel_client_data_to_server(void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm;
	char * p;
	int ret;
	off_t len, orglen, content_len;

	if (fpcon->magic != FPMAGIC_ALIVE) {return -1;}
	if (httpcon == NULL) {return -1;}
	if (httpcon->magic != CON_MAGIC_USED) {return -1;}

	NM_EVENT_TRACE(httpcon->fd, FT_CP_TUNNEL_CLIENT_DATA_TO_SERVER);
	pnm = &gnm[httpcon->fd];
        DBG_LOG(MSG, MOD_TUNNEL, "fd=%d", httpcon->fd);
        if(pthread_mutex_trylock(&pnm->mutex) != 0) {
		/* 
		 * Cannot get lock ?
		 * I have no idea why this function returns 16 EBUSY 
		 */
		glob_http_mutex_trylock_cli_to_ser++;
                return -2;
        }
	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);

	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) &&
	    (pnm->incarn == fpcon->httpcon_fd_incarn) ) {
		p = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
		len = httpcon->http.cb_totlen - httpcon->http.cb_offsetlen;
		if(CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE)) {
			content_len = httpcon->http.content_length + 
					    httpcon->http.http_hdr_len - 
						    httpcon->http.cb_bodyoffset;
			if (len > content_len) len = content_len;
		}
		orglen = len;
		ret = fp_forward_client_to_server(fpcon, p, &len);
		httpcon->http.cb_offsetlen += orglen - len;
		httpcon->http.cb_bodyoffset += orglen - len;
		if( ret == FALSE ) {
			NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm->mutex);
			return -1;
		}
		if (httpcon->http.cb_offsetlen == httpcon->http.cb_totlen){
			httpcon->http.cb_offsetlen = 0;
			httpcon->http.cb_totlen = 0;
		}
	}
	NM_add_event_epollin(httpcon->fd);

	NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm->mutex);
	return 0;
}

/*
 * We receive data from client socket and forward to server socket
 *
 * caller requires to get both httpcon and cpcon mutex locker.
 */
static int fp_forward_client_to_server(fpcon_t * fpcon, char * p, off_t *len)
{
	con_t * httpcon = (con_t *)fpcon->httpcon;
	int ret;
        const namespace_config_t *nsc ;
	const char *host_hdr = NULL;
	int host_hdr_len = 0;
    	u_int32_t attrs;
    	int hdrcnt;

	nsc = httpcon->http.nsconf; //namespace_to_config(httpcon->http.ns_token);
	NM_EVENT_TRACE(httpcon->fd, FT_FP_FORWARD_CLIENT_TO_SERVER);
	NM_set_socket_active(&gnm[fpcon->cp_sockfd]);

	con_t *httpreqcon = (con_t *)fpcon->httpcon;
	if(fpcon->cpcon && CHECK_CPCON_FLAG(fpcon->cpcon, CPF_HTTPS_CONNECT) 
			&& !(CHECK_CPCON_FLAG(fpcon->cpcon, CPF_HTTPS_HDR_SENT))) {
		ssl_con_hdr_t hdr ;
		memset(&hdr, 0, sizeof(hdr));

        	if (nsc->http_config->policies.host_hdr_value ) {
			host_hdr = nsc->http_config->policies.host_hdr_value;
			host_hdr_len = strlen(nsc->http_config->policies.host_hdr_value);
		} else 	if(nsc->http_config->policies.is_host_header_inheritable) {
    			get_known_header(&httpcon->http.hdr, MIME_HDR_HOST,
				&host_hdr, &host_hdr_len, &attrs, &hdrcnt);
		} 

		if(host_hdr && host_hdr_len > 0 ) {
			memcpy(hdr.virtual_domain, host_hdr, host_hdr_len);
		} else if(fpcon->cpcon && fpcon->cpcon->oscfg.ip) {
                        memcpy(hdr.virtual_domain, fpcon->cpcon->oscfg.ip, strlen(fpcon->cpcon->oscfg.ip));
                }

		hdr.magic = HTTPS_REQ_IDENTIFIER;
		fpcon->cpcon->ssl_fid = (AO_fetch_and_add1(&glob_ssl_fid_index) % MAX_SHM_SSL_ID_SIZE);
		hdr.ssl_fid = fpcon->cpcon->ssl_fid; 
		

		hdr.src_ipv4v6.family = httpreqcon->src_ipv4v6.family;
		if(httpreqcon->src_ipv4v6.family == AF_INET) {
			IPv4(hdr.src_ipv4v6) = IPv4(httpreqcon->src_ipv4v6)  ;
		} else {
			memcpy(&(IPv6(hdr.src_ipv4v6)), &(IPv6(httpreqcon->src_ipv4v6)), sizeof(struct in6_addr));
		}
		hdr.src_port = httpreqcon->src_port;

		hdr.dest_ipv4v6.family = fpcon->cpcon->remote_ipv4v6.family;
		if(fpcon->cpcon->remote_ipv4v6.family == AF_INET) {
			IPv4(hdr.dest_ipv4v6) = IPv4(fpcon->cpcon->remote_ipv4v6);
		} else {
			memcpy(&(IPv6(hdr.dest_ipv4v6)), &(IPv6(fpcon->cpcon->remote_ipv4v6)), sizeof(struct in6_addr));
		}

		hdr.dest_port = fpcon->cpcon->remote_port;
		ret = send(fpcon->cp_sockfd, (char *)&hdr, sizeof(hdr), 0);
		//DBG_LOG(MSG, MOD_TUNNEL, "send: fd=%d ret=%d", fpcon->cp_sockfd, ret);
		if( ret==-1 ) {
			if(errno == EAGAIN) {
				NM_add_event_epollout(fpcon->cp_sockfd);
				NM_del_event_epoll(httpcon->fd);
				return TRUE;
			}
			return FALSE;
		} 
		SET_CPCON_FLAG(fpcon->cpcon, CPF_HTTPS_HDR_SENT);
		
	}

	while(*len) {

		//printf("offset=%ld http.cb_totlen=%d\n", fpcon->offset, fpcon->http.cb_totlen);
		ret = send(fpcon->cp_sockfd, p, *len, 0);
		//DBG_LOG(MSG, MOD_TUNNEL, "send: fd=%d ret=%d", fpcon->cp_sockfd, ret);
		if( ret==-1 ) {
			if(errno == EAGAIN) {
				NM_add_event_epollout(fpcon->cp_sockfd);
				NM_del_event_epoll(httpcon->fd);
				return TRUE;
			}
			DBG_LOG(MSG, MOD_TUNNEL, "send: fd=%d ret=%d errno=%d", 
				fpcon->cp_sockfd, ret, errno);
			return FALSE;
		} 
		p += ret;
		*len -= ret;
		glob_tot_size_to_os_by_tunnel += ret;
		AO_fetch_and_add(&glob_origin_send_tot_bytes, ret);

		/* forwarding request to origin.. so update server 
		 * side stats 
		 */
		NS_UPDATE_STATS(httpcon->http.nsconf, PROTO_HTTP, 
				server, out_bytes, ret);
	}
	NM_add_event_epollin(httpcon->fd);
	//printf("EOF: fpcon->offset=%ld\n", fpcon->offset);
	return TRUE;
}


int cp_tunnel_server_data_to_client(void * private_data);
int cp_tunnel_server_data_to_client(void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm;
	char * p;
	int ret = 0;
	int len;
	http_cb_t * phttp;

	if (fpcon->magic != FPMAGIC_ALIVE) {return -1;}
	if (httpcon == NULL) {return -1;}
	if (httpcon->magic != CON_MAGIC_USED) {return -1;}

	NM_EVENT_TRACE(httpcon->fd, FT_CP_TUNNEL_CLIENT_DATA_TO_SERVER);
	pnm = &gnm[httpcon->fd];
        DBG_LOG(MSG, MOD_TUNNEL, "fd=%d", httpcon->fd);
        if(pthread_mutex_trylock(&pnm->mutex) != 0) {
		// this may be because of fp epoll handler 
		glob_http_mutex_trylock_ser_to_cli++;
                fp_add_event(httpcon->fd, OP_EPOLLOUT, 0);
                return -2;
        }
	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);

	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) &&
	    (pnm->incarn == fpcon->httpcon_fd_incarn) ) {
		phttp = &fpcon->httpcon->http;
		// egain header sent offset can be less or equal
		// if less then send the remaining header bytes
		// else start sending body and move to body state
		// phttp->resp_buflen used for egain partial header write
		if (phttp->resp_buflen < phttp->res_hdlen) {
			p = &phttp->resp_buf[phttp->resp_buflen];
                	len = phttp->res_hdlen - phttp->resp_buflen;
			ret = fp_forward_server_to_client(fpcon, p, &len);
			if (ret == -2) {
				// retry fp epoll out added so just 
				phttp->resp_buflen += (phttp->res_hdlen - 
							     phttp->resp_buflen -
							     len);
			} 
		} else {
			// all sent so start sending body...
			ret = 0;
		}
	}
        NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
        pthread_mutex_unlock(&pnm->mutex);
	return ret;
}

/*
 * caller should require both httpcon and cpcon network gnm mutex locker.
 * INPUT: 
 *    *totlen: size of data to be sent.
 * RETURN:
 *    *totlen: size of data left
 *    return 0: success. All data sent out
 *    return -1: error
 *    return -2: EAGAIN
 */
static int fp_forward_server_to_client(fpcon_t * fpcon, char * p, int * totlen)
{
	con_t * httpcon = (con_t *)fpcon->httpcon;
	int ret;
	int send_len = 0;
	http_cb_t *phttp;
	const namespace_config_t *nsc;

	nsc = fpcon->httpcon->http.nsconf;
	phttp = &fpcon->httpcon->http;

	NM_EVENT_TRACE(httpcon->fd, FT_FP_FORWARD_SERVER_TO_CLIENT);
	if (CHECK_HTTP_FLAG(&httpcon->http, HRF_SUPPRESS_SEND_DATA)) {
		p += (*totlen);
		*totlen = 0;
		return 0;
	}

	/* Set TOS if set by policy engine or NS configuration*/
	if ((nsc->http_config->response_policies.dscp >= 0) || 
	    CHECK_HTTP_FLAG(phttp, HRF_PE_SET_IP_TOS)) {
		if (!CHECK_HTTP_FLAG(phttp, HRF_PE_SET_IP_TOS)) {
			phttp->tos_opt = nsc->http_config->response_policies.dscp << 2;
		}

		if (!CHECK_HTTP_FLAG(phttp, HRF_HTTP_SET_IP_TOS) ||
		    CHECK_HTTP_FLAG(phttp, HRF_PE_SET_IP_TOS)) {
			if (set_ip_policies(httpcon->fd, &httpcon->ip_tos, phttp->tos_opt, 
					httpcon->src_ipv4v6.family)) {
				DBG_LOG(MSG, MOD_TUNNEL,
					"IP TOS setting failed for httpcon=%p, fpcon=%p",
					httpcon, fpcon);
			} else {
				SET_HTTP_FLAG(phttp, HRF_HTTP_SET_IP_TOS);
				CHECK_HTTP_FLAG(&httpcon->http, HRF_PE_SET_IP_TOS) ? CLEAR_HTTP_FLAG(&httpcon->http, HRF_PE_SET_IP_TOS):0;
			}
		}
	} else {
		//Restore back the old IP TOS setting if needed.
		if (httpcon->ip_tos >= 0) {
			setsockopt(httpcon->fd, SOL_IP, IP_TOS, &httpcon->ip_tos,
				sizeof(httpcon->ip_tos));
			AO_fetch_and_add1(&glob_reset_dscp_responses);
			DBG_LOG(MSG, MOD_TUNNEL,
				"Restored old IP TOS settings for client socket=%d, tos=%x and con=%p",
				httpcon->fd, httpcon->ip_tos, httpcon);
		}
	}

	while(*totlen) {

		ret = send(httpcon->fd, p, *totlen, 0);
		if( ret==-1 ) {
		    if(errno == EAGAIN) {
			NM_add_event_epollout(httpcon->fd);
			return -2;
		    }
		    DBG_LOG(MSG, MOD_TUNNEL, "send: fd=%d ret=%d errno=%d", 
			    httpcon->fd, ret, errno);
		    return -1;
		} 

		p += ret;
		*totlen -= ret;
		send_len += ret;

		fpcon->httpcon->sent_len += ret; /* for accesslog */
		AO_fetch_and_add(&glob_client_send_tot_bytes, ret);
		AO_fetch_and_add(&glob_tot_client_send, ret);
		AO_fetch_and_add(&glob_tot_size_from_tunnel, ret);
		NS_UPDATE_STATS((httpcon->http.nsconf), PROTO_HTTP, 
			client, out_bytes_tunnel, ret);
		NS_UPDATE_STATS((httpcon->http.nsconf), PROTO_HTTP, 
			client, out_bytes, ret);
	}

	/* Bug fix 4207 , This check is added to increment http well
	 * finished count for tunnel responses also for now.
	 * This can be moved to someother place at later point of time
	 */
	if (fpcon->cpcon && (fpcon->cpcon->expected_len - send_len <= 0) &&
			!CHECK_CPCON_FLAG(fpcon->cpcon, CPF_TE_CHUNKED) ) {
		AO_fetch_and_add1(&glob_http_tot_well_finished);

	}
	return 0;
}

/* ==================================================================== */
/*
 * Callback functions from connection poll functions
 *
 * connection pooling function will get the 
 * gnm[cpcon->fd].mutex before calling this function.
 */
static int fp_connected(void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm;
	const namespace_config_t *nsc;
	int i = 0 ;
	int match_found = 0;

	if (fpcon->magic != FPMAGIC_ALIVE) {return -1;}
	if (httpcon == NULL) {return -1;}
	if (httpcon->magic != CON_MAGIC_USED) {return -1;}

	NM_EVENT_TRACE(httpcon->fd, FT_FP_CONNECTED);
        /*
         * Server side connection has been established, Then
         * 1. return 200 OK to client side.
         * 2. register this socket in network module and ready for forwarding data.
         */
        DBG_LOG(MSG, MOD_TUNNEL, "connect(fd=%d) succeeded", httpcon->fd);
	pnm = &gnm[httpcon->fd];
	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);
	if (!NM_CHECK_FLAG(pnm, NMF_IN_USE) || 
	    (pnm->incarn != fpcon->httpcon_fd_incarn)) {
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);
		return -1;
	}
       if(httpcon->origin_servers.count < MAX_ORIG_IP_CNT) {
               for(i = 0 ; i <  httpcon->origin_servers.count; i++) {
                       if(httpcon->origin_servers.ipv4v6[i].family == AF_INET) {
                           if(IPv4(httpcon->origin_servers.ipv4v6[i]) == 
					IPv4((((cpcon_t *) fpcon->cpcon)->remote_ipv4v6))) {
                                       match_found = 1;
                                       break;
                           }
                       } else if(httpcon->origin_servers.ipv4v6[i].family == AF_INET6) {
                           if(memcmp(&IPv6(httpcon->origin_servers.ipv4v6[i]), 
				&IPv6((((cpcon_t *) fpcon->cpcon)->remote_ipv4v6)), 
				sizeof(struct in6_addr)) == 0) {
                               match_found = 1;
			       break;
                           }
                       } 
               }
               if(!match_found) {
                      memcpy(&httpcon->origin_servers.ipv4v6[httpcon->origin_servers.count], 
			&((cpcon_t *) fpcon->cpcon)->remote_ipv4v6, 
			sizeof(ip_addr_t));

                      httpcon->origin_servers.count++;
               }

	       memcpy(&httpcon->origin_servers.last_ipv4v6,
                        &((cpcon_t *) fpcon->cpcon)->remote_ipv4v6,
                        sizeof(ip_addr_t));


        }


	// For Origin failover case, we've to setup the socket idle timeout
	// value.
	if (http_cfg_fo_use_dns) {
		NM_change_socket_timeout_time(&gnm[fpcon->cp_sockfd], (http_idle_timeout/2)+1);
		DBG_LOG(MSG, MOD_TUNNEL,
			"setup_idle_timeout(): change idle timeout value for %d from %d to %d seconds"
			" fpcon=%p",
			fpcon->cp_sockfd, http_cfg_origin_conn_timeout,
			http_idle_timeout, fpcon);
		if (CHECK_HTTP_FLAG(&httpcon->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE( "DNS_MULTI_IP_FO: setup_idle_timeout(): change idle timeout"
				   " value for %d from %d to %d seconds"
				   " fpcon=%p",
				   fpcon->cp_sockfd, http_cfg_origin_conn_timeout,
				   http_idle_timeout, fpcon);
		}
	}

        nsc = httpcon->http.nsconf; //namespace_to_config(httpcon->http.ns_token);
	((cpcon_t *)fpcon->cpcon)->nsconf = nsc;
        if (nsc->http_config->policies.client_req_connect_handle
                         && CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_CONNECT))
 {
                const char * res = "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\n\r\n";
                send(httpcon->fd, res, strlen(res), 0);
                httpcon->http.cb_totlen = 0;
                httpcon->http.cb_offsetlen = 0;
                httpcon->http.cb_bodyoffset = 0;
                httpcon->http.http_hdr_len = 0;
                UNSET_CON_FLAG(httpcon, CONF_BLOCK_RECV);
                CLEAR_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE);

                /* For Accesslog purpose */
                httpcon->http.respcode = 200;
		httpcon->http.res_hdlen = strlen(res);
		SET_HTTP_FLAG(&httpcon->http, HRF_RES_HEADER_SENT);
        }
        //release_namespace_token_t(httpcon->http.ns_token);
	NM_add_event_epollin(fpcon->httpcon->fd);
	NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm->mutex);

        return 0;
}

/* ==================================================================== */
/*
 * Callback functions from connection poll functions
 *
 * connection pooling function will get the 
 * gnm[cpcon->fd].mutex before calling this function.
 */
static void fp_connect_failed(void * private_data, int called_from)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	http_cb_t *phttp = &fpcon->httpcon->http;
	struct network_mgr * pnm;
	int len = 0, found;
	int error = 0;
	int err_code = NKN_OM_SERVER_DOWN;
	int resp_code = 504;
	ip_addr_t remote_ip;

	if (fpcon->magic != FPMAGIC_ALIVE) {return;}
	if (httpcon == NULL) {return;}
	if (httpcon->magic != CON_MAGIC_USED) {return;}

	NM_EVENT_TRACE(httpcon->fd, FT_FP_CONNECT_FAILED);
	DBG_LOG(MSG, MOD_TUNNEL, "connect(fd=%d) failed", httpcon->fd);
	pnm = &gnm[httpcon->fd];

	if (called_from == OM_EPOLL_EVENT) {
		/*
		 * This function is called from OM epoll event.
		 * We do not have client socket mutex lock.
		 */
		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);
		if (!NM_CHECK_FLAG(pnm, NMF_IN_USE) ||
		    (pnm->incarn != fpcon->httpcon_fd_incarn)) {
			NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm->mutex);
			return;
		}
	}

	if (!NM_CHECK_FLAG(pnm, NMF_IN_USE) ||
	    (pnm->incarn != fpcon->httpcon_fd_incarn)) {
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);
		return;
	}

	if (http_cfg_fo_use_dns &&
	   (!CHECK_CON_FLAG(httpcon, CONF_CLIENT_CLOSED)) &&
	   (!CHECK_CON_FLAG(httpcon, CONF_DNS_HOST_IS_IP)) &&
	   (fpcon->cpcon->state == CP_FSM_SYN_SENT) &&
	   (!CHECK_HTTP_FLAG(phttp, HRF_MFC_PROBE_REQ))) {
		/* Restart tunnel using next OS */
		if (httpcon->os_failover.num_ip_index < nkn_max_domain_ips) {
			SET_FPCON_FLAG(fpcon, FPF_RESTART_TUNNEL);
		}
		UNSET_CON_FLAG(httpcon, CONF_DNS_DONE);
		remote_ip = fpcon->cpcon->remote_ipv4v6;
		dns_hash_and_mark(fpcon->cpcon->oscfg.ip, remote_ip, &found);
		glob_fp_tunnel_os_failover_retry++;
		if (CHECK_HTTP_FLAG(&httpcon->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE( "DNS_MULTI_IP_FO: connect-failed: "
				   " cp sockfd:%d, retry: %d"
				   " fpcon=%p",
				   fpcon->cp_sockfd,
				   httpcon->os_failover.num_ip_index,
				   fpcon);
		}
		if (called_from == OM_EPOLL_EVENT) {
			NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm->mutex);
		}
		return;
	}

	len = sizeof(error);
	if (!getsockopt(fpcon->cp_sockfd, SOL_SOCKET, 
		    SO_ERROR, (void *)&error, (socklen_t *)&len)) {
		if (error == ECONNREFUSED) {
			err_code = NKN_OM_SERVICE_UNAVAILABLE;
			resp_code = 503;
		}
	}

	CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	http_build_res_common(phttp, resp_code, err_code, NULL); 

	len = phttp->res_hdlen;

	fp_forward_server_to_client(fpcon, &phttp->resp_buf[0], &len);

	SET_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT);
	SET_CPCON_FLAG(fpcon->cpcon, CPF_CONN_CLOSE);

	fpcon->cpcon->http.cb_totlen = 0;

	if (called_from == OM_EPOLL_EVENT) {
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);
	}

	return;
}

#define OUTBUF(_data, _datalen) { \
    if (((_datalen)+bytesused) >= outbufsz) { \
   	DBG_LOG(MSG, MOD_OM, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
		(_datalen), bytesused, outbufsz); \
	outbuf[outbufsz-1] = '\0'; \
	return outbuf; \
    } \
    memcpy((void *)&outbuf[bytesused], (_data), (_datalen)); \
    bytesused += (_datalen); \
}

#define OUTBUF_PTR (&outbuf[bytesused])
#define OUTBUF_REM_LEN (outbufsz-bytesused)
#define INC_OUTBUF_LEN(_datalen) { \
    if (((_datalen)+bytesused) >= outbufsz) { \
   	DBG_LOG(MSG, MOD_OM, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
		(_datalen), bytesused, outbufsz); \
	outbuf[outbufsz-1] = '\0'; \
	return outbuf; \
    } \
    bytesused += (_datalen); \
}

#define INC_OUTBUF(_datalen) { \
	if (((_datalen)+bytesused) >= outbufsz ) { \
		if (outbufsz < 16*1024) { \
			*outbufsz_p = outbufsz * 2; \
			outbufsz = *outbufsz_p; \
			*outbuf_p = (char *)nkn_realloc_type(*outbuf_p, outbufsz, mod_om_ocon_t); \
			outbuf = *outbuf_p; \
			DBG_LOG(MSG, MOD_OM, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
			(_datalen), bytesused, outbufsz); \
			} \
		else { \
			DBG_LOG(MSG, MOD_OM, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
			(_datalen), bytesused, outbufsz); \
			outbuf[outbufsz-1] = '\0'; \
			return outbuf; \
		} \
	} \
}

static char *fp_build_http_request(fpcon_t * fpcon, char **outbuf_p, int *outbufsz_p);
static char *fp_build_http_request(fpcon_t * fpcon, char **outbuf_p, int *outbufsz_p)
{
	int rv;
	con_t * httpreqcon = (con_t *)fpcon->httpcon;
    	int token;
    	int nth;
    	const char *name;
    	int namelen;
	const char *data;
	int datalen;
    	u_int32_t attrs;
    	int hdrcnt;
	int bytesused = 0;
	int range_value_set = 0;
	char *querystr;
	u_int8_t user_attr = 0;
	header_descriptor_t *hd;
	char *outbuf = *outbuf_p;
	int outbufsz = *outbufsz_p;
	int x_forward_hdr_no = -1;
	const namespace_config_t *nsc;
	const char *method;
	int methodlen;
	const char *uri;
	int urilen;


	if (!httpreqcon) {
		DBG_LOG(MSG, MOD_OM, "Invalid input, httpreqcon=%p", httpreqcon);
		return 0;
	}
	nsc=httpreqcon->http.nsconf; //namespace_to_config(httpreqcon->http.ns_token);
    	rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_METHOD, 
			&data, &datalen, &attrs, &hdrcnt);
    	if (rv) {
		DBG_LOG(MSG, MOD_OM, "No HTTP method");
		//release_namespace_token_t(httpreqcon->http.ns_token);
		return 0;
    	}
	method = data;
	methodlen = datalen;

	/* BZ 5400: Do NOT convert HEAD to GET. Use the method as is. */
	OUTBUF(data, datalen);
    	OUTBUF(" ", 1);
    	if ((rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_SEEK_URI, 
				&data, &datalen, &attrs, &hdrcnt))) {
		// BUG 3909, 5298, 4218, 5990, 6150:
		// F-pro0xy, R-proxy: change absolute uri to relative uri
		// T-Proxy: keep asis
		if ( (nsc->http_config->origin.select_method==OSS_HTTP_DEST_IP)
				&& !(rv = get_known_header(&httpreqcon->http.hdr, 
						MIME_HDR_X_NKN_ABS_URL_HOST,
						&data, &datalen, &attrs, 
						&hdrcnt))) {
			OUTBUF("http://", 7);
			OUTBUF(data, datalen);
		}
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		if (rv) {
			DBG_LOG(MSG, MOD_OM, "No URI, fd=%d", httpreqcon->fd);
			//release_namespace_token_t(httpreqcon->http.ns_token);
			return 0;
		}
	}

	// URI
	uri = data;
	urilen = datalen;
    	OUTBUF(data, datalen);
    	OUTBUF(" ", 1);

	if ((querystr = memchr(data, '?', datalen)) != 0) {
		//
		// Request requires query string, note it for now 
		// in the OM hdrs NV list and place it in the attributes 
		// if the request is successful.
		//
		rv = add_namevalue_header(&httpreqcon->http.hdr, 
				http_known_headers[MIME_HDR_X_NKN_QUERY].name,
				http_known_headers[MIME_HDR_X_NKN_QUERY].namelen,
				querystr, datalen - (int)(querystr-data),
				user_attr);
    		if (rv) {
			DBG_LOG(MSG, MOD_OM, "add_namevalue_header() failed, rv=%d", rv);
			//release_namespace_token_t(httpreqcon->http.ns_token);
			return 0;
    		}
	}

	// HTTP version
	if ((httpreqcon->http.flag & HRF_HTTP_11)
               /*
                * In case of reverse-proxy, send the highest http version
                * supported by MFC (for RFC-2616 compliance).
                * In case of T-proxy, send the same version of request from client 
                * to origin. (ie, 1.0 -> 1.0, 1.1 -> 1.1)
                */
            || (!nkn_namespace_use_client_ip(nsc))) {
    		OUTBUF("HTTP/1.1\r\n", 10);
	} else if (httpreqcon->http.flag & HRF_HTTP_10) {
    		OUTBUF("HTTP/1.0\r\n", 10);
	} else {
    		OUTBUF("\r\n", 2);
	}

	if (CHECK_HTTP_FLAG(&httpreqcon->http, HRF_SSP_SEEK_TUNNEL)) {
		range_value_set = 1;
	}

	// Add known headers
    	for (token = 0; token < MIME_HDR_MAX_DEFS; token++) {
		if (!http_end2end_header[token]) {
			continue;
		}
		switch(token) {
		case MIME_HDR_HOST:
			if ((!nsc->http_config->policies.host_hdr_value && 
                              nsc->http_config->policies.is_host_header_inheritable) ||
			   (nsc->http_config->origin.select_method == OSS_HTTP_DEST_IP) ||
			   (nsc->http_config->origin.select_method == OSS_HTTP_FOLLOW_HEADER)) {
				break; // Pass through
			} else {
				continue; // Replace with our value
			}
		//case MIME_HDR_RANGE: // Replace with our value
		case MIME_HDR_IF_RANGE:
		case MIME_HDR_X_NKN_URI:
		case MIME_HDR_X_NKN_METHOD:
		case MIME_HDR_X_NKN_QUERY:
		case MIME_HDR_X_NKN_REMAPPED_URI:
		case MIME_HDR_X_NKN_DECODED_URI:
		case MIME_HDR_X_NKN_SEEK_URI:
                case MIME_HDR_X_NKN_CLUSTER_TPROXY:
                case MIME_HDR_X_NKN_CL7_PROXY:
			continue;
		case MIME_HDR_AUTHORIZATION:
			if (nsc->http_config->origin.u.http.server[0]->aws.active) continue;
		case MIME_HDR_DATE:
			if (nsc->http_config->origin.u.http.server[0]->aws.active) continue;
		default:
			break;
		}

        	rv = get_known_header(&httpreqcon->http.hdr, token, &data, 
				&datalen, &attrs, &hdrcnt);
		if (!rv) {
			INC_OUTBUF(http_known_headers[token].namelen+datalen+1);
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
				INC_OUTBUF(datalen+3);
	    			OUTBUF(",", 1);
	    			OUTBUF(data, datalen);
	    		}
		}
		OUTBUF("\r\n", 2);
    	}

        if (nsc->http_config->policies.host_hdr_value && 
                (nsc->http_config->origin.select_method != OSS_HTTP_DEST_IP) &&
                (nsc->http_config->origin.select_method != OSS_HTTP_FOLLOW_HEADER)) {
                int host_hdr_len = 0;

                host_hdr_len = strlen(nsc->http_config->policies.host_hdr_value) ;
		INC_OUTBUF(http_known_headers[MIME_HDR_HOST].namelen+
			(int)host_hdr_len+20);
		OUTBUF(http_known_headers[MIME_HDR_HOST].name,
		    http_known_headers[MIME_HDR_HOST].namelen);
		OUTBUF(": ", 2);

                OUTBUF(nsc->http_config->policies.host_hdr_value, host_hdr_len);
                OUTBUF("\r\n", 2);
                DBG_LOG(MSG, MOD_OM, "Tunnel: Overriding Host header with user configured value: '%s'",
                                     nsc->http_config->policies.host_hdr_value) ;

        } else if (!nsc->http_config->policies.is_host_header_inheritable &&
		(nsc->http_config->origin.select_method != OSS_HTTP_DEST_IP) &&
		(nsc->http_config->origin.select_method != OSS_HTTP_FOLLOW_HEADER)) {
		// Add own host header if its not inheritable and if its not tproxy
		// Add Host: header
		INC_OUTBUF(http_known_headers[MIME_HDR_HOST].namelen+
			(int)strlen(fpcon->cpcon->oscfg.ip)+20);
		OUTBUF(http_known_headers[MIME_HDR_HOST].name,
		    http_known_headers[MIME_HDR_HOST].namelen);
		OUTBUF(": ", 2);

		OUTBUF(fpcon->cpcon->oscfg.ip, (int)strlen(fpcon->cpcon->oscfg.ip));
		if (ntohs(fpcon->cpcon->oscfg.port) != 80) {
			char port[16];
			snprintf(port, sizeof(port), ":%d", 
				 ntohs(fpcon->cpcon->oscfg.port));
			OUTBUF(port, (int)strlen(port));
		}
		OUTBUF("\r\n", 2);
	}

	// Add Connection: header
	INC_OUTBUF(http_known_headers[MIME_HDR_CONNECTION].namelen+20);
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
		if (nsc->http_config->origin.u.http.send_x_forward_header) {
			if ( strncasecmp(name, "X-Forwarded-For", namelen) == 0 ) {
				x_forward_hdr_no = nth - 1;
				continue;
			}
		}
		INC_OUTBUF(namelen+datalen+3);
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
	if (nsc->http_config->origin.u.http.send_x_forward_header && 
			IPv4(httpreqcon->src_ipv4v6)) {
		char *src_ip_str = alloca(INET_ADDRSTRLEN+1);
		datalen = 0;
		if (x_forward_hdr_no >= 0) {
			get_nth_unknown_header(&httpreqcon->http.hdr, x_forward_hdr_no, 
				&name, &namelen, &data, &datalen, &attrs);
		}
		if (inet_ntop(AF_INET, &IPv4(httpreqcon->src_ipv4v6), src_ip_str, 
			      INET_ADDRSTRLEN+1)) {
		    INC_OUTBUF((int)strlen(src_ip_str)+datalen+20);
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
	     nth < nsc->http_config->origin.u.http.num_headers; 
	     nth++) {
		hd = &nsc->http_config->origin.u.http.header[nth];
		if ((hd->action == ACT_ADD) && hd->name) {
		    if (range_value_set &&
			(strncmp(hd->name, "Range", hd->name_strlen) == 0)) {
		    } else {
			INC_OUTBUF(hd->name_strlen+hd->value_strlen+3);
			OUTBUF(hd->name, hd->name_strlen);
			OUTBUF(": ", 2);
			if (hd->value) {
			    OUTBUF(hd->value, hd->value_strlen);
			}
			OUTBUF("\r\n", 2);
		    }
		}
	}

	if (nsc->http_config->origin.u.http.server[0]->aws.active) {
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
		unsigned int hmac_len;
		struct tm *local;
		time_t t;

		INC_OUTBUF(13+2+len+3+4+2+len+3);

		aws_access_key = nsc->http_config->origin.u.http.server[0]->aws.access_key;
		aws_access_key_len = nsc->http_config->origin.u.http.server[0]->aws.access_key_len;
		aws_secret_key = nsc->http_config->origin.u.http.server[0]->aws.secret_key;
		aws_secret_key_len = nsc->http_config->origin.u.http.server[0]->aws.secret_key_len;

		t = time(NULL);
		local = gmtime(&t);
		date_len = strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S GMT", local);

		strcat(final_uri, "/");
		bucket_name = strtok(fpcon->cpcon->oscfg.ip, ".");
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
		glob_fp_aws_auth_cnt++;
	}

end:
	OUTBUF("\r\n", 2);
	OUTBUF("", 1);
	//release_namespace_token_t(httpreqcon->http.ns_token);
	return outbuf;
}

static int fp_send_request(fpcon_t * fpcon, char * p, int len);
static int fp_send_request(fpcon_t * fpcon, char * p, int len)
{
	con_t * httpcon = (con_t *)fpcon->httpcon;
	int ret;
	const char *host_hdr = NULL;
	int host_hdr_len = 0;
    	u_int32_t attrs;
    	int hdrcnt;
	const namespace_config_t *nsc;
	nsc = httpcon->http.nsconf; //namespace_to_config(httpcon->http.ns_token);

	NM_set_socket_active(&gnm[fpcon->cp_sockfd]);

	//if(httpcon->http.nsconf && (httpcon->http.nsconf->http_config->origin_request_type & DP_ORIGIN_REQ_SECURE) && !httpcon->hdr_sent) {
	if(fpcon->cpcon && CHECK_CPCON_FLAG(fpcon->cpcon, CPF_HTTPS_CONNECT) 
			&& !(CHECK_CPCON_FLAG(fpcon->cpcon, CPF_HTTPS_HDR_SENT))) {
		ssl_con_hdr_t hdr ;
		memset(&hdr, 0 , sizeof(hdr));
		/* REad the CLI set host heade value*/
        	if (nsc->http_config->policies.host_hdr_value ) {
			host_hdr = nsc->http_config->policies.host_hdr_value;
			host_hdr_len = strlen(nsc->http_config->policies.host_hdr_value);
		} else 	if(nsc->http_config->policies.is_host_header_inheritable) {
			/* Get host header value from client request */
    			get_known_header(&httpcon->http.hdr, MIME_HDR_HOST,
				&host_hdr, &host_hdr_len, &attrs, &hdrcnt);
		} 

		if(host_hdr && host_hdr_len > 0 ) {
			memcpy(hdr.virtual_domain, host_hdr, host_hdr_len);
		} else if(fpcon->cpcon && fpcon->cpcon->oscfg.ip) {
                        memcpy(hdr.virtual_domain, fpcon->cpcon->oscfg.ip, strlen(fpcon->cpcon->oscfg.ip));
                }

		hdr.magic = HTTPS_REQ_IDENTIFIER;
		fpcon->cpcon->ssl_fid = (AO_fetch_and_add1(&glob_ssl_fid_index) % MAX_SHM_SSL_ID_SIZE);
		hdr.ssl_fid = fpcon->cpcon->ssl_fid; 

		hdr.src_ipv4v6.family = httpcon->src_ipv4v6.family;
		if(httpcon->src_ipv4v6.family == AF_INET) {
			IPv4(hdr.src_ipv4v6) = IPv4(httpcon->src_ipv4v6)  ;
		} else {
			memcpy(&(IPv6(hdr.src_ipv4v6)), &(IPv6(httpcon->src_ipv4v6)), sizeof(struct in6_addr));
		}
		hdr.src_port = httpcon->src_port;

		hdr.dest_ipv4v6.family = fpcon->cpcon->remote_ipv4v6.family;
		if(fpcon->cpcon->remote_ipv4v6.family == AF_INET) {
			IPv4(hdr.dest_ipv4v6) = IPv4(fpcon->cpcon->remote_ipv4v6);
		} else {
			memcpy(&(IPv6(hdr.dest_ipv4v6)), &(IPv6((fpcon->cpcon->remote_ipv4v6))), sizeof(struct in6_addr));
		}

		hdr.dest_port = fpcon->cpcon->remote_port;
		ret = send(fpcon->cp_sockfd, (char *)&hdr, sizeof(hdr), 0);
		//DBG_LOG(MSG, MOD_TUNNEL, "send: fd=%d ret=%d", fpcon->cp_sockfd, ret);
		if( ret==-1 ) {
			if(errno == EAGAIN) {
				NM_add_event_epollout(fpcon->cp_sockfd);
				NM_del_event_epoll(httpcon->fd);
				return TRUE;
			}
			return FALSE;
		}
		SET_CPCON_FLAG(fpcon->cpcon, CPF_HTTPS_HDR_SENT); 
	}
	while(len) {

		//printf("offset=%ld http.cb_totlen=%d\n", fpcon->offset, fpcon->http.cb_totlen);
		ret = send(fpcon->cp_sockfd, p, len, 0);
		//DBG_LOG(MSG, MOD_TUNNEL, "send: fd=%d ret=%d", fpcon->cp_sockfd, ret);
		if( ret==-1 ) {
			if(errno == EAGAIN) {
				NM_add_event_epollout(fpcon->cp_sockfd);
				NM_del_event_epoll(httpcon->fd);
				return TRUE;
			}
			return FALSE;
		} 
		p += ret;
		len -= ret;
		glob_tot_size_to_os_by_tunnel += ret;
		AO_fetch_and_add(&glob_origin_send_tot_bytes, ret);

		/* forwarding request to origin.. so update server 
		 * side stats 
		 */
		NS_UPDATE_STATS(httpcon->http.nsconf, PROTO_HTTP, 
				server, out_bytes, ret);
	}
	NM_add_event_epollin(httpcon->fd);
	//printf("EOF: fpcon->offset=%ld\n", fpcon->offset);
	return TRUE;
}

static int apply_policy_and_forward_client_request(fpcon_t *fpcon)
{
	con_t *httpcon = (con_t *)fpcon->httpcon;
	char * send_req_buf;
	int send_len;

        DBG_LOG(MSG, MOD_TUNNEL, "fp_send_httpreq: fd=%d", httpcon->fd);

        send_len = MAX(TYPICAL_HTTP_CB_BUF,
                                (httpcon ?
                                 httpcon->http.cb_max_buf_size : 0));
        send_req_buf = (char *)nkn_malloc_type(send_len, mod_fp_send_req_buf);
        if (!send_req_buf) {
                DBG_LOG(MSG, MOD_OM, "malloc failed");
                return -1;
        }

	if (!fp_build_http_request(fpcon, &send_req_buf, &send_len)) {
		DBG_LOG(MSG, MOD_OM, "fp_build_http_request failed");
		free(send_req_buf);
		return -1;
	}

	send_len = strlen(send_req_buf);

	if (fp_send_request(fpcon, send_req_buf, send_len) == FALSE) {
		return -1;
	}

	free(send_req_buf);

	httpcon->http.cb_offsetlen = httpcon->http.http_hdr_len;
	/*
	 * Request with Content-Length.
	 */
	if(CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE)) {
		char *p;
		const char *data;
		int ret, datalen, hdrcnt;
		u_int32_t attrs;
		off_t len, orglen, content_len;

		if (is_known_header_present(&httpcon->http.hdr, MIME_HDR_EXPECT)) {
			ret = get_known_header(&httpcon->http.hdr,
						    MIME_HDR_EXPECT, &data,
							&datalen, &attrs, &hdrcnt);
			if (ret) {
				return -1;
			}
			if (datalen == 12 &&
			    strncasecmp(data, "100-Continue", 12) == 0) {
				SET_HTTP_FLAG(&httpcon->http, HRF_100_CONTINUE);
				goto end;
			}
		}

		p = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
		content_len = httpcon->http.content_length +
				    httpcon->http.http_hdr_len -
					    httpcon->http.cb_bodyoffset;
		len = httpcon->http.cb_totlen - httpcon->http.cb_offsetlen;
		if (len > content_len) len = content_len;
		orglen = len;
		ret = fp_forward_client_to_server(fpcon, p, &len);
		httpcon->http.cb_offsetlen += orglen - len;
		httpcon->http.cb_bodyoffset += orglen - len;
		if(ret == FALSE) {
			return -1;
		}
	}
end:
	if (httpcon->http.cb_offsetlen == httpcon->http.cb_totlen) {
		httpcon->http.cb_offsetlen = 0;
		httpcon->http.cb_totlen = 0;
	}

	UNSET_CON_FLAG(httpcon, CONF_BLOCK_RECV);
	NM_add_event_epollin(httpcon->fd);

	return 0;
}

static int forward_client_request(fpcon_t *fpcon)
{
	con_t *httpcon = (con_t *)fpcon->httpcon;
        char *p, *pmethod, *phost, *start_pos;
        char backup[256];
        int ret = 0;
        int bytes_left = 0;
        int backuplen = 0, methodlen = 0;
        off_t len, orglen, content_len;
        const namespace_config_t *nsc;

	httpcon->http.cb_offsetlen = 0;
        /*
         * the following code can be replaced by calling get_known_header().
         * the reason I parse myself here is for high performance.
         *
         * We backup the original domain string and copy back later on
         * for accesslog.
         */

        DBG_LOG(MSG, MOD_TUNNEL, "fp_send_httpreq: fd=%d", httpcon->fd);
        // find out method length
        // p = strchr(pmethod, ' '); // parse error?
        // if(!p) goto done;
        start_pos = p = pmethod = &httpcon->http.cb_buf[0];
        /* bug 5017 - since cb_offsetlen is reset during
        fp_make_tunnel call, so decided to use cb_bodyoffset */
        len = httpcon->http.cb_bodyoffset;
        while (len && p[0] != ' ') { p++; len--; }
        if (len <= 0) goto done;
        p++;
        len--;
        methodlen = p - pmethod;

        // find out backup string length
        if(strncmp(p, "http://", 7)!=0) {
                /* BZ 4094. For cases where no Host header
                 * and no abs_url is seen, we tunnel the data.
                 * we cant jump to done directly.
                 */
                goto done1;
        }

        // Otherwise forward proxy
        // find out backup string length
        p+=7;
        len -= 7;
        while(len && *p!='/' && *p!=' ' && *p!='\n') {
                p++;
                len--;
        }
        if (*p!='/') {
                /* e.g. OPTIONS http://www.google.com HTTP/1.1 */
                goto done;
        }
        backuplen = p - pmethod;
        if(backuplen > 256) {
                backuplen = 0;
                goto done;
        }
        memcpy(backup, pmethod, backuplen); // backup domain part
done1:
        // Copy the method
        memcpy(p-methodlen, pmethod, methodlen);        // Copy HTTP method
        httpcon->http.cb_offsetlen = p - pmethod - methodlen;
done:
        // TODO : fix correct Host header here
        nsc = fpcon->httpcon->http.nsconf;
        if (nsc && (!nsc->http_config->policies.is_host_header_inheritable)
                && (!CHECK_FPCON_FLAG(fpcon, FPF_USE_DEST_IP)) )
        {
                char *pstart = NULL;
do_again:
                /* store the current parser
                 * state to reset later */
                bytes_left = len;

                phost = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
                //p = strcasestr(phost, "\r\nHost:"); // parse error?
                //if (p == NULL) p = strstr(phost, "\r\nhost:"); // lower case?
                while (len >= 7) {
                        if (p[0] == '\r' && p[1] == '\n' &&
                            (p[2] == 'H' || p[2] == 'h') &&
                            (p[3] == 'O' || p[3] == 'o') &&
                            (p[4] == 'S' || p[4] == 's') &&
                            (p[5] == 'T' || p[5] == 't') &&
                            (p[6] == ':')) {
                                break;
                        }
                        p++; len--;
                }
                if (len < 7) {
                        p = NULL;
                }
                if (p && (p != phost)) {
                        char *pend;

                        // check if we are reverse proxying
                        /* found! */
                        p += 7;
                        len -= 7;
                        while (len && *p == ' ') { p++; len--; } // skip spaces..
                        // find length of the host header
                        //pend = strstr(p, "\r\n"); // parse error?
                        pstart = p;
                        while (len >=2) {
                                if (p[0] == '\r' && p[1] == '\n') {
                                        break;
                                }
                                p++, len--;
                        }
                        if (len < 2) {
                                p++;
                                len--;
                        }
                        pend = p;
                        p = pstart;
                        if (pend) {
                                char portbuf[16];
                                // end of header found
                                int hostlen = pend - p; // use the /r/n too
                                // get the size of the new string
                                int newhostlen = strlen(fpcon->cpcon->oscfg.ip);
                                // get offset to move from
                                int move_bytes =
                                        &httpcon->http.cb_buf[httpcon->http.cb_totlen] - pend;
                                int portlen = 0;
                                int port = ntohs(fpcon->cpcon->oscfg.port);
                                if (port != 80) {
                                        portlen = snprintf(portbuf, sizeof(portbuf), ":%d", port);
                                        if (portlen > 0) {
                                                newhostlen += portlen;
                                        }
                                }
                                int len_diff = 0;
                                if (newhostlen > hostlen) {
                                        len_diff = newhostlen - hostlen;
                                } else {
                                        len_diff = hostlen - newhostlen;
                                }
                                // move up or down
                                //int move_dir = (hostlen > newhostlen) ? -1 : 1;

                                /* BZ 4574: Before editting this buffer, check whether
                                 * we are likely to spillover the allocated buffer and cause
                                 * a memory corruption else where.
                                 * If we spill over, then reallocate the buffer before editing
                                 */
                                if ((newhostlen > hostlen) &&
                                    ((httpcon->http.cb_totlen + (len_diff)) >
                                     (httpcon->http.cb_max_buf_size + 1))) {
                                        /* Realloc buffer here */
                                        char *tmp = (char *) nkn_realloc_type(httpcon->http.cb_buf,
                                                                              httpcon->http.cb_totlen + \
                                                                              len_diff + 1,
                                                                              mod_network_cb);
                                        if (tmp == NULL) {
                                                return -1;
                                        }
                                        httpcon->http.cb_max_buf_size =\
                                                httpcon->http.cb_totlen + len_diff;
                                        httpcon->http.cb_buf = tmp;
                                        pmethod = tmp;
                                        p = tmp +\
                                            httpcon->http.cb_offsetlen+\
                                            methodlen;
                                        len = bytes_left;
                                        reset_http_header(&httpcon->http.hdr, NULL, 0);
                                        goto do_again;
                                }

                                memmove(p + newhostlen, pend, move_bytes);
                                if (portlen > 0) {
                                    memcpy(p, fpcon->cpcon->oscfg.ip, newhostlen - portlen);
                                    memcpy(p + newhostlen - portlen, portbuf, portlen);
                                } else {
                                    memcpy(p, fpcon->cpcon->oscfg.ip, newhostlen);
                                }

                                //httpcon->http.cb_totlen +=  (move_dir * (hostlen - newhostlen));
                                if (hostlen > newhostlen) {
                                        httpcon->http.cb_totlen -= len_diff;
                                        httpcon->http.cb_bodyoffset -= len_diff;
                                        if (CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE)) {
                                                httpcon->http.http_hdr_len -= len_diff;
                                        }
                                } else if (hostlen < newhostlen) {
                                        // move buffer down
                                        httpcon->http.cb_totlen += len_diff;
                                        httpcon->http.cb_bodyoffset += len_diff;
                                        if (CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE)) {
                                                httpcon->http.http_hdr_len += len_diff;
                                        }
                                } else {
                                        // both are same size
                                }
                        }
                        else {
                                // This is strange.. we didnt find end of header
                                assert(!"No Header end for tunnel request");
                        }
                }
        }

       /*
         * Request with Content-Length.
         */
        if(CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE)) {
                const char *data;
                int datalen, hdrcnt;
                u_int32_t attrs;

                len = httpcon->http.http_hdr_len - httpcon->http.cb_offsetlen;
                p = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
                ret = fp_forward_client_to_server(fpcon, p, &len);
                if (ret == FALSE) {
                        return -1;
                }
                httpcon->http.cb_offsetlen = httpcon->http.http_hdr_len;

                if (is_known_header_present(&httpcon->http.hdr, MIME_HDR_EXPECT)) {
                        ret = get_known_header(&httpcon->http.hdr,
                                                    MIME_HDR_EXPECT, &data,
                                                        &datalen, &attrs, &hdrcnt);
                        if (ret) {
                                return -1;
                        }
                        if (datalen == 12 &&
                            strncasecmp(data, "100-Continue", 12) == 0) {
                                SET_HTTP_FLAG(&httpcon->http, HRF_100_CONTINUE);
                                goto end;
                        }
                }

                p = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
                content_len = httpcon->http.content_length +
                                    httpcon->http.http_hdr_len -
                                            httpcon->http.cb_bodyoffset;
                len = httpcon->http.cb_totlen - httpcon->http.cb_offsetlen;
                if (len > content_len) len = content_len;
                orglen = len;
        } else {
                p = &httpcon->http.cb_buf[httpcon->http.cb_offsetlen];
                len = httpcon->http.cb_totlen - httpcon->http.cb_offsetlen;
                orglen = len;
        }

        if (len > 0) {
                ret = fp_forward_client_to_server(fpcon, p, &len);
                if(ret == FALSE) {
                        return -1;
                }
                httpcon->http.cb_offsetlen += orglen - len;
                httpcon->http.cb_bodyoffset += orglen - len;
        }
end:
        if (httpcon->http.cb_offsetlen == httpcon->http.cb_totlen) {
                httpcon->http.cb_offsetlen = 0;
                httpcon->http.cb_totlen = 0;
        }

        if(backuplen) {
                memcpy(pmethod, backup, backuplen); // recover for access log
        }

        UNSET_CON_FLAG(httpcon, CONF_BLOCK_RECV);
        NM_add_event_epollin(httpcon->fd);

        return 0;
}

/*
 * connection pooling function will get the
 * gnm[cpcon->fd].mutex before calling this function.
 */
static int fp_send_httpreq(void * private_data)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm;
	const namespace_config_t *nsc;
	int ret;

	if (fpcon->magic != FPMAGIC_ALIVE) {return -1;}
	if (httpcon == NULL) {return -1;}
	if (httpcon->magic != CON_MAGIC_USED) {return -1;}

	NM_EVENT_TRACE(httpcon->fd, FT_FP_SEND_HTTPREQ);
	pnm = &gnm[httpcon->fd];
	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);
	if (!NM_CHECK_FLAG(pnm, NMF_IN_USE) ||
	    (pnm->incarn != fpcon->httpcon_fd_incarn)) {
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);
		return -1;
	}

	if (httpcon->http.cb_totlen == 0) {
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);
		return -1;
	}

	nsc = fpcon->httpcon->http.nsconf;

	//For HTTP 0.9 requests copy HRF_HTTP_09 flag from request 
	//to response hdr.
	if (CHECK_HTTP_FLAG(&httpcon->http, HRF_HTTP_09)) {
		SET_HTTP_FLAG(&fpcon->cpcon->http, HRF_HTTP_09);
	}

	if (!CHECK_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE) ||
	    (!is_known_header_present(&httpcon->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY) &&
             !is_known_header_present(&httpcon->http.hdr, MIME_HDR_X_NKN_CL7_PROXY) &&   
             (nsc->http_config->origin.select_method == OSS_HTTP_DEST_IP ||
	     ((nsc->http_config->origin.select_method == OSS_HTTP_FOLLOW_HEADER) &&
	     (nsc->http_config->origin.u.http.server[0]->aws.active == 0))))) {
		ret = forward_client_request(fpcon);
	} else {
		ret = apply_policy_and_forward_client_request(fpcon);
	}

	if (nsc)
		NS_INCR_STATS(nsc, PROTO_HTTP, server, requests);

	NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm->mutex);

        return ret;
}

/*
 * connection pooling function will get the
 * gnm[cpcon->fd].mutex before calling this function.
 */

static int fp_http_header_received(void * private_data, char * p, int len, int content_length)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm;
	int ret = -1;
	uint64_t client_content_len = 0;
	http_cb_t * phttp;
	const namespace_config_t *nsc;

	if (fpcon->magic != FPMAGIC_ALIVE) {return -3;}
	if (httpcon == NULL) {return -3;}
	if (httpcon->magic != CON_MAGIC_USED) {return -3;}

	//Get the firstbyteout time here. 
	gettimeofday(&(fpcon->httpcon->http.ttfb_ts), NULL);

	NM_EVENT_TRACE(fpcon->httpcon->fd, FT_FP_HTTP_HEADER_RECEIVED);
	//printf("fp_http_header_received: fd=%d cp_sockfd=%d\n", fpcon->httpcon->fd, fpcon->cp_sockfd);
	/*
	 * For Accesslog purpose, save the information into fpcon->httpcon
	 */
	pnm = &gnm[httpcon->fd];
	DBG_LOG(MSG, MOD_TUNNEL, "fp_http_header_received: fd=%d", httpcon->fd);
        if(pthread_mutex_trylock(&pnm->mutex) != 0) {
		/* 
		 * BUG 3745
		 * Cannot get lock ?
		 * I have no idea why this function returns 16 EBUSY 
		 */
		glob_http_mutex_trylock_header_rcv++;
                cp_add_event(fpcon->cp_sockfd, OP_TUNNEL_READY, NULL);
                return -3;
        }
	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);

	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) &&
	    (pnm->incarn == fpcon->httpcon_fd_incarn) ) {
		cpcon_t * cpcon;
		node_map_context_t *nm_ctx;
		int rv;

		assert(fpcon->magic == FPMAGIC_ALIVE);
		/*
		 * Node map origin response action handler
		 */
		nsc = fpcon->httpcon->http.nsconf; //namespace_to_config(fpcon->httpcon->http.ns_token);
		if (nsc && (nsc->http_config->origin.select_method ==  
			    OSS_HTTP_NODE_MAP)) {
			nm_ctx = REQUEST_CTX_TO_NODE_MAP_CTX(
							&fpcon->request_ctx);
			if (nm_ctx) {
				rv =(*nm_ctx->my_node_map->origin_reply_action)
					(&nm_ctx->my_node_map->ctx,
					&fpcon->request_ctx,
					&fpcon->httpcon->http.hdr,
					&fpcon->cpcon->http.hdr,
					fpcon->cpcon->http.respcode);
				if (rv) {
		    			char dummy_clnt_req_ip_port[16];
		    			char res_origin_ip[128];
		    			char *p_res_origin_ip;
		    			int origin_ip_len;
		    			uint16_t res_origin_port;

		    			dummy_clnt_req_ip_port[0] = 0;
		    			res_origin_ip[0] = 0;
		    			p_res_origin_ip = res_origin_ip;

		    			rv = request_to_origin_server(
		    				REQ2OS_SET_ORIGIN_SERVER, 
						&fpcon->request_ctx,
				      		&fpcon->httpcon->http.hdr, nsc,
				      		dummy_clnt_req_ip_port, 
				      		sizeof(dummy_clnt_req_ip_port),
						&p_res_origin_ip,
						sizeof(res_origin_ip),
						&origin_ip_len,
						&res_origin_port, 0, 0);
					if (!rv) {
					    fpcon->httpcon->http.cb_totlen =
					      fpcon->httpcon->http.http_hdr_len;
					    //release_namespace_token_t(
					    //	fpcon->httpcon->http.ns_token);
					    /* Restart tunnel using next OS */
					    SET_FPCON_FLAG(fpcon, 
							FPF_RESTART_TUNNEL);
					    //We are basically restarting the tunnel process,
					    //with a new OS, so need to do DNS again
					    UNSET_CON_FLAG(httpcon, CONF_DNS_DONE);
					    NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
					    pthread_mutex_unlock(&pnm->mutex);
					    return -1;
					}
				}
			}
		}
		//release_namespace_token_t(fpcon->httpcon->http.ns_token);

		/*
		 * copy necessary fields for building up response header.
		 * these fields are needed by setup_http_build_200/206().
		 */
		phttp = &fpcon->httpcon->http;
		cpcon = fpcon->cpcon;
		client_content_len = phttp->content_length;
		phttp->content_length = cpcon->http.content_length;
		phttp->obj_create = nkn_cur_ts;
		if(!CHECK_HTTP_FLAG(phttp,HRF_BYTE_RANGE_START_STOP) && (phttp->brstop == 0)) {
			phttp->brstop=phttp->content_length + phttp->brstart - 1;
		} else if(phttp->brstop >= phttp->content_length) {
			phttp->brstop=phttp->content_length + phttp->brstart - 1;
		}
		/* BUG 5549 
		 * If we can parse Content-Range successfully, we should set brstart, brstop
		 * to the value returned from Origin server.
		 */
		if (is_known_header_present(&cpcon->http.hdr, MIME_HDR_CONTENT_RANGE)) {
			const char *range = 0;
			int rangelen;
			u_int32_t attrs;
			int hdrcnt;

			get_known_header(&cpcon->http.hdr, MIME_HDR_CONTENT_RANGE, &range, &rangelen,
					&attrs, &hdrcnt);
			range += 6; // skip "bytes "
			phttp->brstart = atol(range);
			range = strchr(range, '-');
			if (range) {
				range++;
				phttp->brstop = atol(range);
			}
			SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		}

		if (CHECK_HTTP_FLAG(&cpcon->http, HRF_TRANSCODE_CHUNKED)) {
			SET_HTTP_FLAG(phttp, HRF_TRANSCODE_CHUNKED);
		}

		if (!CHECK_HTTP_FLAG(&cpcon->http, HRF_CONNECTION_KEEP_ALIVE) && 
		    !CHECK_CPCON_FLAG(cpcon, CPF_CONN_KEEP_ALIVE)) {
			CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
		}

		nsc = fpcon->httpcon->http.nsconf; //namespace_to_config(fpcon->httpcon->http.ns_token);
		cpcon->oscfg.ns_token = fpcon->httpcon->http.ns_token;
		cpcon->oscfg.policies = &nsc->http_config->policies;
		cpcon->oscfg.request_policies = &nsc->http_config->request_policies;
		cpcon->oscfg.nsc = nsc;
		//release_namespace_token_t(fpcon->httpcon->http.ns_token);

		ret = apply_cc_max_age_policies(
                                cpcon->oscfg.policies,
                                &cpcon->http.hdr, fpcon->cp_sockfd);
		if (ret) {
			DBG_LOG(MSG, MOD_TUNNEL, "apply_cc_max_age_policies() "
				"failed ret=%d, fd=%d", ret, fpcon->cp_sockfd);
		}

		/* update counters */
		http_build_res_common(phttp, 
					cpcon->http.respcode, 
					0,
					&cpcon->http.hdr);
		p = &phttp->resp_buf[0];
		len = phttp->res_hdlen;

		NS_INCR_STATS((phttp->nsconf), PROTO_HTTP, client, responses);
		/* 
		 * tunnel response as-is to client 
		 * for small object, we sent out later 
		 */
		if ((content_length==0) ||
		    (len + content_length > MAX_ONE_PACKET_SIZE) ||
		    (CHECK_HTTP_FLAG(&cpcon->http, HRF_TRANSCODE_CHUNKED)) ||
		    (CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD))) {

			/* fp_forward_server_to_client will return: 0, -1, -2 */
			ret = fp_forward_server_to_client(fpcon, p, &len);
			if (ret == -2) {
				// resp_buflen is used to track partial write here...
				// phttp->resp_buf is set so it is fine to use resp_buflen
				// for this case.
				phttp->resp_buflen = (phttp->res_hdlen - len);	
			}	
			if (CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) {
			    SET_HTTP_FLAG(phttp, HRF_SUPPRESS_SEND_DATA);
			}
		}
		else {
			/* small object transaction. */
			SET_HTTP_FLAG(phttp, HRF_ONE_PACKET_SENT);
			ret = 3;
		}
		phttp->content_length = client_content_len;
		if (CHECK_CPCON_FLAG(cpcon, CPF_SPECIAL_TUNNEL)) {
                        UNSET_CON_FLAG(httpcon, CONF_BLOCK_RECV);
                        CLEAR_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE);
                        NM_add_event_epollin(fpcon->httpcon->fd);
		} else if (cpcon->http.respcode == 100) {
			UNSET_CON_FLAG(fpcon->httpcon, CONF_BLOCK_RECV);
			if (fpcon->httpcon->http.cb_totlen -
				fpcon->httpcon->http.cb_offsetlen > 0) {
				fp_add_event(fpcon->httpcon->fd, OP_EPOLLIN, 0);
			} else {
				NM_add_event_epollin(fpcon->httpcon->fd);
			}
		} else if (!CHECK_HTTP_FLAG(&cpcon->http, HRF_PARSE_DONE)) {
			NM_EVENT_TRACE(fpcon->httpcon->fd, FT_TASK_CANCEL);
			NM_add_event_epollin(fpcon->httpcon->fd);
			SET_CON_FLAG(fpcon->httpcon, CONF_CANCELD);
                } else if (CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_CONNECT)) {
                        UNSET_CON_FLAG(httpcon, CONF_BLOCK_RECV);
                        CLEAR_HTTP_FLAG(&httpcon->http, HRF_PARSE_DONE);
                        NM_add_event_epollin(fpcon->httpcon->fd);

		} else {
			if (ret != -2) {   // EAGAIN -- epollout event should not be deleted
				NM_del_event_epoll(fpcon->httpcon->fd);
			}
		}
	} else {
		ret = -1;
		glob_fp_client_socket_err++;
	}
	NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm->mutex);
	return ret;
}

static void fp_http_md5_checksum_update (con_t *con, char *p, int len)
{

   /*
    * If the log format has %{X-NKN-MD5-Checksum}o & the MD5 checksum
    * string length configured, calculate md5 chksum for 'len' characters
    * and update the md5-context in http connection 'con'.
    */
    if (con->http.nsconf &&
        con->http.nsconf->acclog_config->al_resp_hdr_md5_chksum_configured &&
        con->http.nsconf->md5_checksum_len && 
        con->md5 &&
        (con->http.respcode == 200 || con->http.respcode == 206)) {
        int md5_total_len = con->md5->in_strlen + len ;

        // Update the MD5 context with 'len' response data string.
        if(md5_total_len > con->http.nsconf->md5_checksum_len) {
            len = con->http.nsconf->md5_checksum_len - con->md5->in_strlen ;
        }

        if (len > 0) {
            con->md5->temp_seq ++ ;

            // Do MD5 update only when the data length is less than the
            // configured number of bytes.
            con->md5->in_strlen += len ;

            MD5_Update(&con->md5->ctxt, p, len);
            DBG_LOG(MSG, MOD_HTTP,  "MD5_CHECKSUM(Tunnel-Update[%d]):[Len:%d], con=%p fd=%d r=%d, s=%d, l=%ld",
                    con->md5->temp_seq,
                    len, con, con->fd,
                    con->http.respcode, con->http.subcode,
                    con->http.content_length);
        }
    }

}
/*
 * connection pooling function will get the 
 * gnm[cpcon->fd].mutex before calling this function.
 */
static int fp_http_body_received(void * private_data, char * p, int * plen, int * shouldexit)
{
	fpcon_t * fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	struct network_mgr * pnm;
	int ret;
	char buf[MAX_ONE_PACKET_SIZE];
	http_cb_t * phttp;
	int cplen, newlen;
	int origlen =0;

	if (fpcon->magic != FPMAGIC_ALIVE) {return -1;}
	if (httpcon == NULL) {return -1;}
	if (httpcon->magic != CON_MAGIC_USED) {return -1;}

	//printf("fp_http_body_received: fd=%d cp_sockfd=%d\n", fpcon->httpcon->fd, fpcon->cp_sockfd);
	UNUSED_ARGUMENT(shouldexit);

	NM_EVENT_TRACE(httpcon->fd, FT_FP_HTTP_BODY_RECEIVED);
	pnm = &gnm[httpcon->fd];
        if(pthread_mutex_trylock(&pnm->mutex) != 0) {
                /*
                 * if we could not get peer mutex lock.
                 * add an event. and callback later on.
                 */
		glob_http_mutex_trylock_body_rcv++;
                fp_add_event(httpcon->fd, OP_EPOLLOUT, 0);
                return -2;
        }

	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);
	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) &&
	    (pnm->incarn == fpcon->httpcon_fd_incarn) ) {
		phttp = &fpcon->httpcon->http;

		if (CHECK_HTTP_FLAG(phttp, HRF_ONE_PACKET_SENT)) {
			int excess_bytes = 0;
			CLEAR_HTTP_FLAG(phttp, HRF_ONE_PACKET_SENT);
			memcpy(buf, phttp->resp_buf, phttp->res_hdlen);
			cplen = MAX_ONE_PACKET_SIZE - phttp->res_hdlen;
			/*
			 * when HRF_ONE_PACKET_SENT is set, the maximum
			 * send size is MAX_ONE_PACKET_SIZE. If *plen
			 * is greater than the MAX_ONE_PACKET_SIEZ, then
			 * the excess_bytes has to be added in the unsent
			 * bytes count
			 */
			if (cplen > *plen) {
				cplen = *plen;
			} else {
				excess_bytes = *plen - cplen;
			}
			memcpy(&buf[phttp->res_hdlen], p, cplen);
			newlen = phttp->res_hdlen + cplen;
			origlen = newlen;
			ret = fp_forward_server_to_client(fpcon, &buf[0], &newlen);
			if (newlen > cplen) {
				// Even header has not been sent out.
				ret = -1;
			}
			else {
                                fp_http_md5_checksum_update(fpcon->httpcon, &buf[phttp->res_hdlen],
                                                            origlen - newlen - phttp->res_hdlen) ;
				*plen = (newlen + excess_bytes);
			}
                }
                else {
                        char *p_orig = p;

			origlen = *plen;
			ret = fp_forward_server_to_client(fpcon, p, plen);
                        fp_http_md5_checksum_update(fpcon->httpcon, p_orig, 
                                                    origlen - *plen) ;
                }
	}
	else {
		ret = -1;
		glob_fp_client_socket_err++;
	}
	NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm->mutex);

	return ret;
}

static int fp_close_proxy_socket(void * pfpcon, int close_httpconn) {
	fpcon_t * fpcon = (fpcon_t *)pfpcon;
	struct network_mgr * pnm;
	http_cb_t *phttp;
	int len = 0;

	if (fpcon->magic != FPMAGIC_ALIVE) {return TRUE;}
	if (fpcon->httpcon == NULL) {return TRUE;}
	if (fpcon->httpcon->magic != CON_MAGIC_USED) { return TRUE;}

	DBG_LOG(MSG, MOD_TUNNEL, "fd=%d, data sent= %ld", fpcon->httpcon->fd, fpcon->httpcon->sent_len);
	NM_EVENT_TRACE(fpcon->httpcon->fd, FT_FP_CLOSE_PROXY_SOCKET);
	pnm = &gnm[fpcon->httpcon->fd];
        if(pthread_mutex_trylock(&pnm->mutex) != 0) {
		glob_http_mutex_trylock_close_proxy++;
		glob_fp_close_proxy_trylock_err ++;
		return FALSE;
	}

	NM_TRACE_LOCK(pnm, LT_OM_TUNNEL);
	if (!NM_CHECK_FLAG(pnm, NMF_IN_USE) || 
	    (pnm->incarn != fpcon->httpcon_fd_incarn)) {
		NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm->mutex);
		return TRUE;
	}

	if (!CHECK_FPCON_FLAG(fpcon, FPF_RESTART_TUNNEL) && 
	    (fpcon->httpcon->sent_len == 0)) {
 		phttp = &fpcon->httpcon->http;
		CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_504(phttp, NKN_TUNNEL_FETCH_ERROR);
		len = phttp->res_hdlen;
		fp_forward_server_to_client(fpcon, &phttp->resp_buf[0], &len);
		SET_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT);
	}
	//fp_add_event(fpcon->httpcon->fd, OP_CLOSE, close_httpconn);
	fp_internal_closed(fpcon, close_httpconn, CALLED_FROM_CP);
	NM_TRACE_UNLOCK(pnm, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm->mutex);

	return TRUE;
}

static int fp_timer(void *private_data, net_timer_type_t type)
{
	UNUSED_ARGUMENT(type);
	fpcon_t *fpcon = (fpcon_t *)private_data;
	con_t * httpcon = (con_t *)fpcon->httpcon;
	http_cb_t *phttp = &fpcon->httpcon->http;
	int len = 0;
	int err_code = NKN_OM_SERVER_DOWN;
	int resp_code = 504;

	if ((fpcon->magic != FPMAGIC_ALIVE) || (httpcon == NULL)) { return -1;}

	http_build_res_common(phttp, resp_code, err_code, NULL); 

	len = phttp->res_hdlen;

	fp_forward_server_to_client(fpcon, &phttp->resp_buf[0], &len);

	CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
	SET_CPCON_FLAG(fpcon->cpcon, CPF_CONN_CLOSE);
	SET_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT);

	return 1;
}


/*
 * fp_internal_closed() caller needs to hold gnm[httpcon->fd].mutex lock,
 *
 * if(called_from == CALLED_FROM_CP) then 
 *	caller does not matter if gnm[cpcon->fd].mutex lock has been hold or not.
 *
 * if(called_from == CALLED_FROM_EPOLL) then 
 *      caller needs to hold gnm[cpcon->fd].mutex lock.
 */
static void fp_internal_closed(fpcon_t * fpcon, int close_httpconn, int called_from)
{
	con_t * httpcon;
	struct network_mgr * pnm_cp;
	int restart_tunnel;
	int rv;

	//printf("=====>fp_internal_closed: called\n");
        if(fpcon == NULL) return;
	if(fpcon->magic != FPMAGIC_ALIVE) return;
	httpcon = (con_t *)fpcon->httpcon;
	if (httpcon == NULL) {return;}
	if (httpcon->magic != CON_MAGIC_USED) return;

	/* If we are in the event queue, we should delete from there */
	if(CHECK_FPCON_FLAG(fpcon, FPF_IN_EVENT_QUEUE)) {
		fp_remove_event(httpcon->fd);
		UNSET_FPCON_FLAG(fpcon, FPF_IN_EVENT_QUEUE);
	}

	if((called_from == CALLED_FROM_EPOLL) && fpcon->cpcon) {
		pnm_cp = &gnm[fpcon->cp_sockfd];
		if( NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) &&
	   	    (pnm_cp->incarn == fpcon->cp_sockfd_incarn) ) {
			glob_fp_err_internal_closed_1++;
			SET_CON_FLAG(httpcon, CONF_CLIENT_CLOSED);
			NM_del_event_epoll(httpcon->fd);
			cp_add_event(fpcon->cp_sockfd, OP_CLOSE_CALLBACK, NULL);
			return;
		}
	}

	NM_EVENT_TRACE(httpcon->fd, FT_FP_INTERNAL_CLOSED);
	glob_fp_err_internal_closed++;
	DBG_LOG(MSG, MOD_TUNNEL, "Tunnel %d <--> %d break up",
				    httpcon->fd, fpcon->cp_sockfd);

	/* Everthing will be treated as data for non-processable http responses */
	if (!CHECK_HTTP_FLAG(&fpcon->cpcon->http, HRF_PARSE_DONE)) {
		httpcon->http.res_hdlen = 0;
	}

	/* To Identify the data length, header length will be subtracted from total length */
	if (httpcon->sent_len >=  httpcon->http.res_hdlen) {
		httpcon->sent_len -= httpcon->http.res_hdlen;
	}

	restart_tunnel = CHECK_FPCON_FLAG(fpcon, FPF_RESTART_TUNNEL);
	fpcon->cpcon = NULL;
	fpcon->magic = FPMAGIC_DEAD;
	fpcon->httpcon_fd_incarn = 0;
	AO_fetch_and_sub1(&glob_fp_cur_open_tunnel);

	if (!CHECK_HTTP_FLAG(&httpcon->http, HRF_RP_SESSION_ALLOCATED)) {
		AO_fetch_and_sub1((volatile AO_t *) &(httpcon->http.nsconf->stat->g_http_sessions));
                NS_DECR_STATS(httpcon->http.nsconf, PROTO_HTTP, client, active_conns);
                NS_DECR_STATS(httpcon->http.nsconf, PROTO_HTTP, client, conns);
        }

	if( !restart_tunnel && (close_httpconn == FALSE ) &&
	    (CHECK_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE)) ) {
		/* if proxy-connection: keep-alive*/
		DBG_LOG(MSG, MOD_TUNNEL, "Connection is kept alive fd=%d", httpcon->fd);
		unregister_NM_socket(httpcon->fd, FALSE);
		glob_fp_tunnel_keep_alive++;
		re_register_http_socket(httpcon->fd, (void *)httpcon);
		reset_http_cb(httpcon);
		free(fpcon);	/* Time to free fpcon */
		return;
	}

	if (!restart_tunnel) {
		/* close the client connection */
		DBG_LOG(MSG, MOD_TUNNEL, 
			"Connection is closed fd=%d\n", httpcon->fd);
		close_conn(httpcon); // should be closed when state machine rets
	} else {
		rv = fp_make_tunnel(httpcon, -1, 0, &fpcon->request_ctx);
		if (!rv) {
			DBG_LOG(MSG, MOD_OM, 
				"Restarted tunnel fd=%d", httpcon->fd);
		} else {
			DBG_LOG(MSG, MOD_OM, 
				"Restart tunnel fd=%d, failed, rv=%d", 
				httpcon->fd, rv);
			CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
			http_close_conn(httpcon, rv);
		}
	}
	free(fpcon);	/* Time to free fpcon */
}


/* ==================================================================== */
static int fp_use_existing_cpcon(fpcon_t * fpcon, cpcon_t * cpcon)
{
	/* To Do List: we should remove this function in the future */
	SET_CPCON_FLAG(cpcon, CPF_USED_IN_FPCON);
        fpcon->cpcon = cpcon;

        cpcon->private_data = (void *)fpcon;
        cpcon->cp_connected = fp_connected;
        cpcon->cp_connect_failed = fp_connect_failed;
        cpcon->cp_send_httpreq = fp_send_httpreq;
        cpcon->cp_http_header_received = fp_http_header_received;
        cpcon->cp_http_header_parse_error = NULL;
        cpcon->cp_httpbody_received = fp_http_body_received;
        cpcon->cp_closed = fp_close_proxy_socket;
	cpcon->cp_timer = fp_timer;
	cpcon->type = CP_APP_TYPE_TUNNEL;

	return 0;
}

/*
 * fp_create_new_cpcon() should free cpcon if return errcode.
 */
static int fp_create_new_cpcon(fpcon_t * fpcon, con_t * httpcon)
{
	struct network_mgr * pnm_cp = NULL;
        cpcon_t * cpcon;
	const namespace_config_t *nsc;
	origin_server_cfg_t oscfg;
	ip_addr_t src_ipv4v6;
	ip_addr_t ipv4v6;
	uint16_t port;
	int ret;
	int allow_retry;

	nsc = httpcon->http.nsconf; //namespace_to_config(httpcon->http.ns_token);
	if (!nsc) {
		DBG_LOG(WARNING, MOD_TUNNEL, "No namespace data.");
                return NKN_FP_SOCKET;
	}

	if(nsc->http_config->origin.select_method == OSS_NFS_CONFIG ||
	    nsc->http_config->origin.select_method == OSS_NFS_SERVER_MAP) {
		DBG_LOG(WARNING, MOD_TUNNEL, "Not a http origin server.");
		//release_namespace_token_t(httpcon->http.ns_token);
                return NKN_FP_NOT_HTTP_ORIGIN;
	}

	if (!nsc->http_config) {
		DBG_LOG(WARNING, MOD_TUNNEL, "No origin server config data.");
		//release_namespace_token_t(httpcon->http.ns_token);
                return NKN_FP_SOCKET;
	}
	oscfg.nsc = nsc;
	oscfg.ip = NULL;
        if (nsc->http_config->origin.u.http.ip_version == FOLLOW_CLIENT) {
                ipv4v6.family = httpcon->ip_family;
        } else if (nsc->http_config->origin.u.http.ip_version == FORCE_IPV6) {
                ipv4v6.family = AF_INET6;
        } else {
                ipv4v6.family = AF_INET;
        }

	if (CHECK_FPCON_FLAG(fpcon, FPF_USE_DEST_IP)) {
		int rv;
		u_int32_t attrs;
		int hdrcnt;
		const char *data;
		int datalen;

		ret = FALSE;
		rv = get_known_header(&httpcon->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
					&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			rv = inet_pton(ipv4v6.family, data, &ipv4v6.addr);
           		if (rv <= 0) {
               			if (rv == 0) {
					DBG_LOG(WARNING, MOD_TUNNEL, "inet_pton, Not in presentation format: %s", data);
				} else {
					DBG_LOG(WARNING, MOD_TUNNEL, "inet_pton error: %s errno:%d", data, errno);
				}
           		}
			oscfg.ip = nkn_strdup_type(data, mod_ns_origin_ip_buf);
			rv = get_known_header(&httpcon->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
					&data, &datalen, &attrs, &hdrcnt);
			if (!rv) {
				oscfg.port = htons(atoi(data));
				port = oscfg.port;
				ret = TRUE;
			}
                }
	}
	else {
		ret = find_origin_server(httpcon, &oscfg, &fpcon->request_ctx, 0, 0, 0, 0, 0, &ipv4v6, &port, 
				 &allow_retry);
	}

	if (nkn_namespace_use_client_ip(nsc)) {
		memcpy(&src_ipv4v6, &(httpcon->src_ipv4v6), sizeof(ip_addr_t)); // T-Proxy: use client IP address
	}
	else {
		if (ipv4v6.family == AF_INET6) {
			src_ipv4v6.family = AF_INET6;
			memcpy(&(IPv6(src_ipv4v6)), &(httpcon->pns->if_addrv6[0].if_addrv6), sizeof(struct in6_addr)); // Use MFD interface IP address
		} else {
			src_ipv4v6.family = AF_INET;
			IPv4(src_ipv4v6) = httpcon->pns->if_addrv4;
		}
	}
	//release_namespace_token_t(httpcon->http.ns_token);
	if (ret == FALSE) {
		if(oscfg.ip) { free(oscfg.ip); };
		return NKN_FP_SOCKET;
	}

	if ((!CHECK_CRAWL_REQ(&httpcon->http)) &&
		nsc->http_config->request_policies.num_accept_headers) {
		ret = fp_validate_client_request(httpcon);
		if (ret) {
			if(oscfg.ip) { free(oscfg.ip); };
			http_build_res_403(&httpcon->http, ret);
			return ret;
		}
	}

	/* Open a cpcon socket - origin facing. 
	 * This call only grabs the socket fd, sets it in cpcon
	 * and returns a (cpcon_t *) back to us. 
	 *
	 * Note: if call fails, socket fd is released by 
	 * cp_open_socket_handler() itself.
	 */
	cpcon = cp_open_socket_handler(&fpcon->cp_sockfd, 
		&src_ipv4v6, &ipv4v6, port,
		CP_APP_TYPE_TUNNEL,
		(void *)fpcon,
		fp_connected,
		fp_connect_failed,
		fp_send_httpreq,
		fp_http_header_received,
		NULL,
		fp_http_body_received,
		fp_close_proxy_socket,
		fp_timer);
	if(!cpcon) {
		if(oscfg.ip) { free(oscfg.ip); };
		return NKN_FP_SOCKET;
        }

	if(cpcon->oscfg.ip) { free(cpcon->oscfg.ip); cpcon->oscfg.ip = NULL;};
	cpcon->oscfg.ip = nkn_strdup_type(oscfg.ip, mod_ns_origin_ip_buf);
	cpcon->oscfg.port = oscfg.port;
	if(oscfg.ip) { free(oscfg.ip); };

	//cpcon->src_ip = httpcon->src_ip;
	//cpcon->src_port = httpcon->src_port;
	if (nkn_namespace_use_client_ip(nsc)) {
		SET_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ);
	}
	pnm_cp = &gnm[fpcon->cp_sockfd];
	DBG_LOG(MSG, MOD_TUNNEL, "cp_open_socket_handler returns %p", cpcon);

        SET_CPCON_FLAG(cpcon, CPF_USED_IN_FPCON);
	fpcon->cp_sockfd_incarn = gnm[fpcon->cp_sockfd].incarn;
        fpcon->cpcon = cpcon;
        if (!CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_GET) &&
	    !CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_HEAD)) {
		SET_CPCON_FLAG(cpcon, CPF_METHOD_NOT_GET);
		if (!CHECK_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE)) {
			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
		}
	}
	/* Don't close https socket */
        if (CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_CONNECT)) {
                SET_CPCON_FLAG(cpcon, CPF_METHOD_CONNECT);
                UNSET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
                if (nsc->http_config->policies.client_req_connect_handle) {
                        SET_CPCON_FLAG(cpcon, CPF_CONNECT_HANDLE);
                }
        }

        if (CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_HEAD)) {
		SET_CPCON_FLAG(cpcon, CPF_METHOD_HEAD);
	}
	cpcon->http.nsconf = nsc;

        /*
	 * Time to make socket to origin server
	 */
        if(cp_activate_socket_handler(fpcon->cp_sockfd, cpcon) == FALSE) {
		glob_fp_err_activate_cp ++;

		pthread_mutex_lock(&pnm_cp->mutex);
		NM_TRACE_LOCK(pnm_cp, LT_OM_TUNNEL);
		if( NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) &&
	   	    (pnm_cp->incarn == fpcon->cp_sockfd_incarn) ) {
			// if we couldnt bind.. set the cpcon to close.
			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
			cp_add_event(fpcon->cp_sockfd, OP_CLOSE, NULL);
		} else {
			/* Bug 5164/5988: fd leak.
			 * activate fails, we unregister the cp_sockfd
			 * from the epollfd, and clear the gnm[fd].flags.
			 * As a result, the socket with never get closed
			 * anywhere, since we expect the socket to come
			 * through either a cp event (in the cpqueue) or
			 * an epoll event. The close event will never
			 * come in the cp-queue, since the flags on the
			 * gnm[fd].flag is set to 0!
			 *
			 * Note: this piece code gets called on the network
			 * thread.
			 */
			// decrement the count. The above (if) case
			// does a decrement if the cpcon is in the queue.
			// so avoid a double-decrement.
			if (ipv4v6.family == AF_INET6) {			
				AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets_ipv6);
				AO_fetch_and_add1(&glob_cp_tot_closed_sockets_ipv6);
			} else {
				AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets);
				AO_fetch_and_add1(&glob_cp_tot_closed_sockets);
			}
			// Close the http header holder for this cpcon
			shutdown_http_header(&cpcon->http.hdr);
			// free response buffer for this cpcon
			free(cpcon->http.cb_buf);

			/* Bug 8532:
			 * port leaks here since we dont release the port,
			 * but close the fd.
			 */
			if (CHECK_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ)) {
			    /* Free the port */
			    cp_release_transparent_port(cpcon);
			}
			// Declare cpcon as dead.
			cpcon->magic = CPMAGIC_DEAD;
			if (cpcon->oscfg.ip)
				free(cpcon->oscfg.ip);
			free(cpcon);
			// finally close the socket!
			nkn_close_fd(fpcon->cp_sockfd, NETWORK_FD);
			glob_fp_err_activate_1++;
		}
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		return NKN_FP_ACTIVATE_SOCKET;
        }
	DBG_LOG(MSG, MOD_TUNNEL, "cp_activate_socket_handler returns TRUE");

	/* release mutex locker at this time */
	NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
	pthread_mutex_unlock(&pnm_cp->mutex);
	return 0;
}


/*
 * Caller should hold the &gnm[fd].mutex lock
 * This function does not run on network thread.
 * Can be called even it is "CONNECT domain:port HTTP/1.0" request
 * 
 *
 * INPUT: 
 *	1. cp_sockfd != -1: pick up an existing cpcon.
 *	   incarn: verify the cp_sockfd.
 *	2  cp_sockfd == -1: create a new cpcon.
 *	   incarn: set to 0
 * OUTPUT:
 *	return 0: success
 * 	return != 0: failure with this errcode.
 */
int fp_make_tunnel(con_t * httpcon, int cp_sockfd, uint32_t incarn, 
		   request_context_t *caller_req_ctx)
{
        fpcon_t * fpcon = NULL;
	cpcon_t * cpcon = NULL;
	struct network_mgr * pnm = NULL;
	struct network_mgr * pnm_cp = NULL;
	int ret = -1;
	int set_flag = 0;
	request_context_t *req_ctx;
	request_context_t req_ctx_data;

	if (caller_req_ctx) {
		req_ctx = caller_req_ctx;
	} else {
		memset((char *)&req_ctx_data, 0, sizeof(req_ctx_data));
		req_ctx = &req_ctx_data;
	}

	NM_EVENT_TRACE(httpcon->fd, FT_FP_MAKE_TUNNEL);
	SET_CON_FLAG(httpcon, CONF_TUNNEL_PROGRESS);

       /* Set MD5 context for request tunnel case. */
        if (httpcon->http.nsconf
           && httpcon->http.nsconf->acclog_config->al_resp_hdr_md5_chksum_configured
           && httpcon->http.nsconf->md5_checksum_len
           && !httpcon->md5) {
            httpcon->md5 = (http_resp_md5_checksum_t *)nkn_malloc_type(
                                                           sizeof(http_resp_md5_checksum_t),
                                                           mod_om_md5_context);
            if (httpcon->md5) {
                MD5_Init(&httpcon->md5->ctxt) ;
                httpcon->md5->temp_seq = 0;
                httpcon->md5->in_strlen = 0;
                DBG_LOG(MSG, MOD_HTTP, "MD5_CHECKSUM init(tunnel) con:%p, md5_ctxt:%p", 
                                       httpcon, httpcon->md5);
            }
        }

        /*adns changes. Before it starts its routine making sure we do
        a DNS lookup first and then come back and kickstart this. Use the
        Auth helper to post a task and recv it*/
        if(adns_enabled && cp_sockfd==-1 && !(CHECK_CON_FLAG(httpcon,CONF_DNS_DONE)))
        {
                uint16_t port;
		char ip[1024];
                char *p_ip=ip;
                int ip_len;
		int origin_ip_family = AF_INET;
                const namespace_config_t *nsc;
		int rv;
		u_int32_t attrs;
		int hdrcnt;
		const char *data;
		int datalen;
		nsc = httpcon->http.nsconf; //namespace_to_config(httpcon->http.ns_token);
                if (nsc->http_config->origin.u.http.ip_version == FOLLOW_CLIENT) {
                        origin_ip_family = httpcon->ip_family;
                } else if (nsc->http_config->origin.u.http.ip_version == FORCE_IPV6) {
                        origin_ip_family = AF_INET6;
                } else {
                        origin_ip_family = AF_INET;
                }

		/* In the T-proxy environment we should blindly follow the client */
		if (CHECK_HTTP_FLAG(&httpcon->http, HRF_TPROXY_MODE)) {
                        origin_ip_family = httpcon->ip_family;
		}

		rv = get_known_header(&httpcon->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
					&data, &datalen, &attrs, &hdrcnt);

		if (!rv && (origin_ip_family == httpcon->ip_family)) {
                        if (origin_ip_family == AF_INET) {
                                if (IPv4(httpcon->http.remote_ipv4v6) != IPv4(httpcon->dest_ipv4v6)) {
                                        /* If its a CONNECT method and handle connect is enabled,
					 * we need to get the SVRHOST to make the tunnel */
                                        if (nsc->http_config->policies.client_req_connect_handle &&
						CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_CONNECT)) {
                                                //I need to lookup the origin, so dont do anything here
                                        } else {
                                                set_flag = 1;
                                                goto make_tunnel;
                                        }
                                }
                        } else {
                                if (memcmp(IPv6(httpcon->http.remote_ipv4v6), IPv6(httpcon->dest_ipv4v6), sizeof(struct in6_addr))) {
                                        /* If its a CONNECT method and handle connect is enabled,
					 * we need to get the SVRHOST to make the tunnel */
                                        if (nsc->http_config->policies.client_req_connect_handle &&
                                                CHECK_HTTP_FLAG(&httpcon->http, HRF_METHOD_CONNECT)) {
                                                //I need to lookup the origin, so dont do anything here
                                        } else {
                                                set_flag = 1;
                                                goto make_tunnel;
                                        }
                                }
                        }
                }

                ret = request_to_origin_server(REQ2OS_ORIGIN_SERVER_LOOKUP, 
			 req_ctx, httpcon?&httpcon->http.hdr : 0, nsc, 0, 0,
                         &p_ip, sizeof(ip), &ip_len, &port, 1, 0);
		//release_namespace_token_t(httpcon->http.ns_token);
		if (ret) {
			DBG_LOG(ERROR, MOD_TUNNEL, 
				"failed request_to_origin_server (errcode = %d)", ret);
			// Should NKN_HTTP_NO_HOST_HTTP11 be returned ?
			return NKN_FP_DNS_ERR; 	
		}
                ret = auth_helper_get_dns(httpcon, cp_sockfd, incarn, p_ip, origin_ip_family);
		/*If returned 1, means it posted a task and if 0  there is no need
		 *to post a task, its ip or is in cache
		 *so just continue the regular path*/

		if (ret==1) {
			ret = suspend_NM_socket(httpcon->fd);
			if (ret == FALSE) {
				DBG_LOG(ERROR, MOD_TUNNEL, 
					"failed to suspend_NM_socket()");
				return NKN_FP_DNS_ERR;
			}
			return 0;//we posted a task
		}
		if (ret == 2) {
			SET_CON_FLAG(httpcon, CONF_DNS_HOST_IS_IP);
		}
		if (ret == -1) {
			DBG_LOG(ERROR, MOD_TUNNEL, 
				"failed to do dns lookup (errcode = %d)", ret);
			if (http_cfg_fo_use_dns &&
			   (httpcon->os_failover.num_ip_index)) {
				http_build_res_504(&httpcon->http, NKN_OM_ORIGIN_FAILOVER_ERROR);
				if (CHECK_HTTP_FLAG(&httpcon->http, HRF_TRACE_REQUEST)) {
					DBG_TRACE( "DNS_MULTI_IP_FO: Make-tunnel. DNS lookup failure(FO case), return 504."
						   " http sockfd:%d, httpcon=%p",
						   httpcon->fd, httpcon);
				}
			} else {
				http_build_res_503(&httpcon->http, NKN_FP_DNS_ERR);
				if (CHECK_HTTP_FLAG(&httpcon->http, HRF_TRACE_REQUEST)) {
					DBG_TRACE( "DNS_MULTI_IP_FO: Make-tunnel. DNS lookup failure(Use DNS case), return 503."
						   " http sockfd:%d, httpcon=%p",
						   httpcon->fd, httpcon);
				}
			}
			return NKN_FP_DNS_ERR; 	
		}
		if (ret == -2) {
			http_build_res_504(&httpcon->http, NKN_RESOURCE_TASK_FAILURE);
			return NKN_RESOURCE_TASK_FAILURE;
		}
		if (ret == -3) {
			DBG_LOG(ERROR, MOD_TUNNEL,
				"dns lookup queue is full");
			http_build_res_503(&httpcon->http, NKN_FP_DNS_QUEUE_FULL);
			return NKN_FP_DNS_QUEUE_FULL;
		}
        }
        /*end adns changes*/

make_tunnel:
        pnm = &gnm[httpcon->fd];

	//printf("=====>fp_make_tunnel: called\n");
	/*
	 * Step 1: allocate the fpcon memory structure.
	 */
        fpcon = (fpcon_t *)nkn_malloc_type(sizeof(fpcon_t), mod_om_fpcon_t);
        if(!fpcon) { 
		CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
		return NKN_FP_MALLOC; 
	}
        memset(fpcon, 0, sizeof(fpcon_t));
	fpcon->magic = FPMAGIC_ALIVE;
        fpcon->httpcon = httpcon;
	fpcon->cp_sockfd = cp_sockfd;
	fpcon->cp_sockfd_incarn = incarn;
	fpcon->httpcon_fd_incarn = gnm[httpcon->fd].incarn;
	if (req_ctx) {
		fpcon->request_ctx = *req_ctx;
	}
	if (set_flag == 1) {
		SET_FPCON_FLAG(fpcon, FPF_USE_DEST_IP);
	}

	CLEAR_HTTP_FLAG(&httpcon->http, HRF_302_REDIRECT);
	/*
	 * Step 2: allocate the cpcon memory structure.
	 */
	ret = -1;
	if (cp_sockfd != -1) {
		/* Get the gnm mutex and release in the last */
		pnm_cp = &gnm[cp_sockfd];
		pthread_mutex_lock(&pnm_cp->mutex);
		NM_TRACE_LOCK(pnm_cp, LT_OM_TUNNEL);
		if( NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) &&
	   	    (pnm_cp->incarn == fpcon->cp_sockfd_incarn) ) {
			cpcon = (cpcon_t *)(gnm[cp_sockfd].private_data);
			/* Sanity check */
			if (CHECK_CPCON_FLAG(cpcon, CPF_WAIT_FOR_TUNNEL) &&
			    !CHECK_CPCON_FLAG(cpcon, CPF_USED_IN_FPCON)) {
				ret = fp_use_existing_cpcon(fpcon, cpcon);
				UNSET_CPCON_FLAG(cpcon, CPF_WAIT_FOR_TUNNEL);
			}
		}
	}
	else {
		ret = fp_create_new_cpcon(fpcon, httpcon);
	}
	if(ret != 0) {
		/* we didn't get cpcon */
		DBG_LOG(ERROR, MOD_TUNNEL, "failed to open a tunnel (errcode = %d)", ret);
		glob_fp_err_make_tunnel++;
        	free(fpcon);
		if (cp_sockfd != -1) {
			NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm_cp->mutex);
		}

		CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
		return ret;
	}
	DBG_LOG(MSG, MOD_TUNNEL, "Tunnel %d <--> %d established",
			fpcon->httpcon->fd, fpcon->cp_sockfd);

	/*
	 * Step 3: Change the epoll register functions.
	 */
	unregister_NM_socket(httpcon->fd, FALSE);
        if(register_NM_socket(httpcon->fd,
                (void *)fpcon,
                fphttp_epollin,
                fphttp_epollout,
                fphttp_epollerr,
                fphttp_epollhup,
		NULL,
                NULL,	// When downloading large video, we cannot timeout the session.
                0,
                USE_LICENSE_ALWAYS_TRUE,
		TRUE) == FALSE)
        {
		/*
		 * Ideally, this should never happen.
		 * Maybe we should call assert(0) here.
		 *
		 * Here it is transferring normal client socket to tunnel client socket.
		 * It should have already hold a license.
		 */
		if( NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) &&
	   	    (pnm_cp->incarn == fpcon->cp_sockfd_incarn) ) {
			if (cp_sockfd != -1) {
				cp_add_event(fpcon->cp_sockfd, OP_CLOSE, NULL);
			}
		}
		CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
		if (cp_sockfd != -1) {
			NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
			pthread_mutex_unlock(&pnm_cp->mutex);
		}
	        return NKN_FP_NETWORK;
        }

	/*
	 * Step 4: forwarding existing origin server response data to client.
	 */
	if (cp_sockfd != -1) { /* existing cpcon may have data in the cb_buf */
		NM_add_event_epollin(httpcon->fd);
		//NM_add_event_epollin(cp_sockfd);
		assert(fpcon->cpcon->http.cb_totlen > 0);
		cp_add_event(cp_sockfd, OP_TUNNEL_READY, NULL);
	}

	/* For accesslog only */
	httpcon->http.provider = Tunnel_provider;
	httpcon->http.first_provider = Tunnel_provider;
	httpcon->http.last_provider = Tunnel_provider;

	AO_fetch_and_add1(&glob_fp_cur_open_tunnel);
	AO_fetch_and_add1((volatile AO_t *) &(httpcon->http.nsconf->stat->g_tunnels));
	if (!CHECK_HTTP_FLAG(&httpcon->http, HRF_RP_SESSION_ALLOCATED)) {
		AO_fetch_and_add1((volatile AO_t *) &(httpcon->http.nsconf->stat->g_http_sessions));
		NS_INCR_STATS(httpcon->http.nsconf, PROTO_HTTP, client, active_conns);
		NS_INCR_STATS(httpcon->http.nsconf, PROTO_HTTP, client, conns);
	}
        glob_fp_tot_tunnel ++;

	fpcon->cpcon->client_fd = httpcon->fd;

	if (cp_sockfd != -1) {
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_TUNNEL);
		pthread_mutex_unlock(&pnm_cp->mutex);
	}

        return 0;
}

int fp_get_aws_authorization_hdr_value(con_t *con, char *hdr_value)
{
	int methodlen;
	int urilen;
	int hostlen;
	int datelen;
	unsigned int hmaclen;
	const char *method;
	const char *uri;
	const char *host;
	const char *date;
	u_int32_t attr;
	int hdrcnt;
	int rv;

	char *bucket_name;
	char *encoded_sign;
	char *p;
	char *signature;
	char final_uri[4096] = { 0 };
	unsigned char hmac[1024] = { 0 };
	char content_md5[4] = "";
	char content_type[4] = "";
	char cananionicalized_amz_hdrs[4] = "";
	char domain[1024];

	const char *aws_access_key;
	int aws_access_key_len;
	const char *aws_secret_key;
	int aws_secret_key_len;

	rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_METHOD,
				&method, &methodlen, &attr, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "method is not found\n");
		return 1;
	}

	rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
				&uri, &urilen, &attr, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "uri is not found\n");
		return 1;
	}

	rv = get_known_header(&con->http.hdr, MIME_HDR_DATE,
				&date, &datelen, &attr, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "date is not found\n");
		return 1;
	}

	if (con->http.nsconf->http_config->origin.select_method == OSS_HTTP_FOLLOW_HEADER) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_HOST,
				 &host, &hostlen, &attr, &hdrcnt);
		if (rv) {
			DBG_LOG(MSG, MOD_HTTP, "host is not found\n");
			return 1;
		}
		memcpy(domain, host, hostlen);
		domain[hostlen] = '\0';
	} else if (con->http.nsconf->http_config->origin.select_method == OSS_HTTP_CONFIG) {
		memcpy(domain, con->http.nsconf->http_config->origin.u.http.server[0]->hostname,
			    con->http.nsconf->http_config->origin.u.http.server[0]->hostname_strlen);
		domain[con->http.nsconf->http_config->origin.u.http.server[0]->hostname_strlen] = '\0';
	} else {
		DBG_LOG(MSG, MOD_HTTP, "it not follow header host and it is not http host\n");
		return 1;
	}

	signature = hdr_value;
	aws_access_key = con->http.nsconf->http_config->origin.u.http.server[0]->aws.access_key;
	aws_access_key_len = con->http.nsconf->http_config->origin.u.http.server[0]->aws.access_key_len;
	aws_secret_key = con->http.nsconf->http_config->origin.u.http.server[0]->aws.secret_key;
	aws_secret_key_len = con->http.nsconf->http_config->origin.u.http.server[0]->aws.secret_key_len;

	bucket_name = strtok(domain, ".");
	strcat(final_uri, "/");
	strcat(final_uri, bucket_name);
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
	strncat(signature, date, datelen);
	strcat(signature, "\n");
	strcat(signature, cananionicalized_amz_hdrs);
	strcat(signature, final_uri);

	p = (char *)HMAC(EVP_sha1(), aws_secret_key, aws_secret_key_len,
		    (const unsigned char *)signature, strlen(signature), hmac, &hmaclen);
	if (hmaclen == 0) return 1;
	encoded_sign = g_base64_encode(hmac, hmaclen);
	if (encoded_sign == NULL) return 1;

	strcpy(signature, "AWS ");
	strncat(signature, aws_access_key, aws_access_key_len);
	strcat(signature, ":");
	strcat(signature, encoded_sign);
	g_free(encoded_sign);

	return 0;
}

int fp_validate_auth_header(con_t *con)
{
	int ret;
	int datalen;
	const char *data;
	uint32_t attr;
	int header_cnt;
	char hdr_value[4096] = { 0 };
	int len;

	if (con->http.nsconf->http_config->origin.u.http.server[0]->aws.active) {
		ret = get_known_header(&con->http.hdr, MIME_HDR_AUTHORIZATION,
			    &data, &datalen, &attr, &header_cnt);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, "Authorization header is not found\n")
			return NKN_HTTP_AUTHORIZATION_FAIL;
		}
		ret = fp_get_aws_authorization_hdr_value(con, hdr_value);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, "get_aws_authorization_hdr_value is failed\n")
			return NKN_HTTP_AUTHORIZATION_FAIL;
		}
		len = strlen(hdr_value);
		if ((len != datalen) || memcmp(hdr_value, data, datalen)) {
			DBG_LOG(MSG, MOD_HTTP, "mismatch in the aws authorization header value\n");
			glob_aws_auth_hdr_mismatch++;
			return NKN_HTTP_AUTHORIZATION_FAIL;
		}
	}

	return 0;
}

int fp_validate_client_request(con_t *con)
{
	int n;
	const namespace_config_t *nsc;
	header_descriptor_t *hd;
	int ret;

	nsc = con->http.nsconf;
	for (n = 0; n <  nsc->http_config->request_policies.num_accept_headers; n++) {
		hd = &nsc->http_config->request_policies.accept_headers[n];
		if (hd && hd->name) {
			if (strcasecmp(hd->name, "Authorization") == 0) {
				ret = fp_validate_auth_header(con);
				if (ret) return ret;
			}
		}
	}

	return 0;
}

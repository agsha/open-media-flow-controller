/**
 * @file   dns_request_proc.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Mar 27 10:58:21 2012
 * 
 * @brief  Implements data transformation of a DNS query. a tokenized
 * query described by parsed_dns_msg_t is noarmalized into a data
 * store URI and sent to the data store for lookup. the data store
 * responds with a A record or error. If there are more than one
 * questions in the query, then the questions are sent to the store
 * sequentially and the responses are collected. 
 * Once all the questions are processed, a DNS response is built and
 * sent to the network context by setting the EPOLLOUT event
 * 
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

#include "dns_request_proc.h"
#include "dns_parser.h"
#include "dns_builder.h"
#include "dns_con.h"
#include "common/nkn_free_list.h"
#include "common/cr_utils.h"
#include "common/rrecord_msg_utils.h"
#include "de_intf.h"
#include "stored/ds_tables.h"
#include "mfp/thread_pool/mfp_thread_pool.h"
#include "cont_pool.h"

/* extern */
extern obj_list_ctx_t* dns_con_ctx_list;
extern mfp_thread_pool_t* gl_th_pool;
extern cr_dns_cfg_t g_cfg;
extern cont_pool_t* gl_cp;
extern dns_disp_mgr_t gl_disp_mgr;

/* counters */
uint64_t glob_dns_store_read_timeo_cnt = 0;

typedef struct tag_task_in_data {
    parsed_dns_msg_t *pr_ctx;
    ref_count_mem_t *ref_con;
} task_in_data_t;

#define DNS_DATA_TFORM_LOOKUP_START (0x1)
#define DNS_DATA_TFORM_LOOKUP_COMPLETE (0x2)
#define DNS_DATA_TFORM_LOOKUP_PROGRESS (0x4)
#define DNS_DATA_TFORM_LOOKUP_ERR (0x8)
#define DNS_DATA_TFORM_CLEANUP (0x16)

typedef struct tag_task_out_data {
    uint8_t state;
    char *ce[MAX_QUERY_COUNT];
    uint32_t ce_resp_size[MAX_QUERY_COUNT];
    int32_t lookup_status_map[MAX_QUERY_COUNT]; // 1: complete
					     // <=0: err
} task_out_data_t;

struct tag_dns_data_tform_task;
typedef void (*dns_data_tform_task_delete_fnp)
(struct tag_dns_data_tform_task*);

typedef struct tag_dns_data_tform_task {
    uint8_t state;
    task_in_data_t in;
    task_out_data_t out;
    uint32_t num_q;
    uint32_t curr_q_num;
    uint32_t num_q_valid;
    dns_data_tform_task_delete_fnp delete;
}dns_data_tform_task_t;

static int32_t createDataTransformTask(const parsed_dns_msg_t *pr_ctx,
	       const ref_count_mem_t *con, uint32_t num_q, 
	       dns_data_tform_task_t **out);

static void  deleteDataTransformTask(dns_data_tform_task_t *t);

static int32_t scheduleLookupTask(dns_data_tform_task_t *t);

static void processDataTformTask(void *arg);

void lookupTaskHandler(void *arg);

static int32_t buildAllResponses(dns_data_tform_task_t *t);

static inline int32_t lookupDomainIP(
	     char const* dname, char const* src_ip, 
	     cache_entity_addr_type_t qtype, char** result,
	     uint32_t *result_buf_size);

static int32_t convertStrToIP_Num(char const* result, uint8_t* res_ip_num);

static uint8_t getDNSRcodeFromErr(int32_t err);

static inline int32_t setupResponseSend(ref_count_mem_t *ref_con); 

static int32_t buildErrorResponse(dns_data_tform_task_t *t, 
				  int32_t err);

static inline int32_t getDNSHeaderParms(dns_data_tform_task_t *t, 
					uint32_t *tot_a_cnt,
					uint32_t *tot_auth_cnt,
					uint32_t *valid_q_cnt,
					uint8_t *aa_flag,
					uint8_t *rcode);

cache_entity_addr_type_t convertToCacheEntityAddr(
				  uint32_t dns_type);

static void deleteDataTransformTask(dns_data_tform_task_t *t) {

    if (t->state & DNS_DATA_TFORM_CLEANUP) {
	int32_t i;
	for (i = 0; i < MAX_QUERY_COUNT; i++) {
	    free(t->out.ce[i]);
	}
	free(t);
    }
}

static int32_t createDataTransformTask(const parsed_dns_msg_t *pr_ctx,
	       const ref_count_mem_t *con, uint32_t num_q, 
	       dns_data_tform_task_t **out) {

    dns_data_tform_task_t *t = NULL;
    int32_t err = 0;

    assert(pr_ctx);
    assert(con);
    assert(out);

    *out = NULL;    
    t = (dns_data_tform_task_t*)
	nkn_calloc_type(1, sizeof(dns_data_tform_task_t), 100);
    if (!t) {
	err = -ENOMEM;
	goto error;
    }
    t->in.pr_ctx = (parsed_dns_msg_t *)pr_ctx;
    t->in.ref_con = (ref_count_mem_t *)con;
    t->num_q = num_q;
    t->curr_q_num = 0;
    t->delete = deleteDataTransformTask;
    *out = t;
    return err;
    
 error:
    if (t) {
	if (t->delete) {
	    t->delete(t);
	} else {
	    free(t);
	}
    }
    return err;
}

/** 
 * handles a lookup error by setting up the error states correctly
 * 
 * @param t [in/out] - data transform task descriptor
 */
static inline void handleLookupErr(dns_data_tform_task_t *t, 
				   int32_t err_code) {

    t->out.lookup_status_map[t->curr_q_num] = err_code;
    t->curr_q_num++;
    t->out.state |= DNS_DATA_TFORM_LOOKUP_ERR;    
    return;
}

void lookupTaskHandler(void *arg) {

    dns_data_tform_task_t *t = NULL;
    thread_pool_task_t *tp_task = NULL;
    parsed_dns_msg_t *pr_ctx = NULL;
    int32_t err = 0;
    char* dname;
    char result[300], src_ip[32] = {0};
    ref_count_mem_t *ref_con = NULL;
    dns_con_ctx_t* con = NULL;
    nw_ctx_t *nw = NULL;
    uint32_t opcode; 
    cache_entity_addr_type_t qtype;

    memset(result, 0, 16);

    assert(arg);
    t = (dns_data_tform_task_t*)arg;
    pr_ctx = t->in.pr_ctx;
    ref_con = t->in.ref_con;
    con = (dns_con_ctx_t*)ref_con->mem;

    err = pr_ctx->get_qname_hdlr(pr_ctx, t->curr_q_num, 
	    (char const **)&dname);
    if (err) {
	DBG_LOG(ERROR, MOD_NETWORK, "error reading domain name from "
		"parsed data, [err=%d]", err);
	goto error;
    }
    
    opcode = pr_ctx->get_opcode_hdlr(pr_ctx);
    if (opcode != 0) {
	DBG_LOG(ERROR, MOD_NETWORK, "error unsupported query type "
		"OPCODE: %d", opcode);
	err = -E_DNS_UNSUPPORTED_OPCODE;
	goto error;
    }
	
    /* QTYPE validation already done in the caller */
    qtype = convertToCacheEntityAddr(
     pr_ctx->get_qtype_hdlr(pr_ctx, t->curr_q_num));
    
    nw = dns_con_get_nw_ctx(con);
    inet_ntop(AF_INET, &(nw->r_addr.sin_addr.s_addr), 
	      src_ip, 32);
    err = lookupDomainIP(dname, src_ip, qtype, 
		 &t->out.ce[t->curr_q_num], 
		 &t->out.ce_resp_size[t->curr_q_num]);
    if (err < 0) {
	DBG_LOG(ERROR, MOD_NETWORK, 
		"error looking up domain name [err=%d]", err);
	goto error;
    }

#if 0
    char res_ip_num[4];
    err = convertStrToIP_Num(result, (uint8_t*)&res_ip_num[0]);
    if (err < 0) {
	DBG_LOG(ERROR, MOD_NETWORK, 
		"error converting domain name to IP number [err=%d]", err);
	goto error;
    }

    t->out.ce[t->curr_q_num][0] = res_ip_num[0];
    t->out.ce[t->curr_q_num][1] = res_ip_num[1];
    t->out.ce[t->curr_q_num][2] = res_ip_num[2];
    t->out.ce[t->curr_q_num][3] = res_ip_num[3];
#endif

    t->out.state &= ~(DNS_DATA_TFORM_LOOKUP_ERR);

    t->out.lookup_status_map[t->curr_q_num] = 1;
    t->curr_q_num++;
    t->num_q_valid++;

    t->out.state |= DNS_DATA_TFORM_LOOKUP_COMPLETE;

    tp_task = newThreadPoolTask(processDataTformTask, t, 
	    (deleteTaskArg)t->delete);
    if (!tp_task) {
	err = -E_DNS_DATA_TFORM_LOOKUP_ERR;
	goto error;
    }
    gl_th_pool->add_work_item(gl_th_pool, tp_task);

    return;

error:
    handleLookupErr(t, err);
    tp_task = newThreadPoolTask(processDataTformTask, t, 
	    (deleteTaskArg)t->delete);
    if (!tp_task) {
	err = -E_DNS_DATA_TFORM_LOOKUP_ERR;
	goto error;
    }
    gl_th_pool->add_work_item(gl_th_pool, tp_task);

    return;

}

static int32_t scheduleLookupTask(dns_data_tform_task_t *t) {
    
    parsed_dns_msg_t *pr_ctx = t->in.pr_ctx;
    uint32_t i = t->curr_q_num;
    uint16_t qtype = pr_ctx->get_qtype_hdlr(pr_ctx, i);
    uint16_t qclass = pr_ctx->get_qclass_hdlr(pr_ctx, i);
    ref_count_mem_t *ref_con = t->in.ref_con;
    thread_pool_task_t *tp_task = NULL;
    int32_t err = 0;

    if ((qtype == DNS_A || qtype == DNS_AAAA || DNS_CNAME) 
	&& (qclass == DNS_CLASS_INT)) {
	tp_task = newThreadPoolTask(lookupTaskHandler, t, 
				    (deleteTaskArg)t->delete);
	if (!tp_task) {
	    err = -E_DNS_TPOOL_TASK_CREATE;
	    goto error;
	}
	gl_th_pool->add_work_item(gl_th_pool, tp_task);
    } else {
	err = -E_DNS_UNSUPPORTED_RR_TYPE;
	handleLookupErr(t, err);
	tp_task = newThreadPoolTask(processDataTformTask, t, 
				    (deleteTaskArg)t->delete);
	if (!tp_task) {
	    goto error;
	}
	gl_th_pool->add_work_item(gl_th_pool, tp_task);
	goto error;
    }
    
 error:
    return err;
}

static void processDataTformTask(void *arg) {

    assert(arg);
    
    dns_data_tform_task_t *t = (dns_data_tform_task_t *)arg;
    parsed_dns_msg_t *pr_ctx = t->in.pr_ctx;
    ref_count_mem_t *ref_con = t->in.ref_con;
    dns_con_ctx_t* con = (dns_con_ctx_t*)ref_con->mem;
    uint8_t proc_err = 0;
    int32_t err = 0;
    
    if (t->out.state == DNS_DATA_TFORM_LOOKUP_ERR) {
	proc_err = 1;
	err = -E_DNS_DATA_TFORM_LOOKUP_ERR;
    }
    if (t->curr_q_num < t->num_q && !proc_err) {
	
	err = scheduleLookupTask(t);
	if (err) {
	    goto error;
	}
    } else if (t->curr_q_num == t->num_q || proc_err) {
	nw_ctx_t *nw = NULL;
	err = buildAllResponses(t);
	if (err) {
	    goto error;
	}
	/* accessing the nw context in the request processing thread
	 * is safe since the connection context is reference counted
	 * which ensures that the objects in the nw context will
	 * always be 'active'
	 */
	nw = dns_con_get_nw_ctx(con);
	if (nw->ptype == DNS_TCP) {
	    nw->disp->set_write_hdlr((event_disp_t*)nw->disp, 
				    (entity_data_t*)nw->ed);
	} else {
	    err = dns_con_send_data(con);
	    ref_con->release_ref_cont(ref_con);
	    if (err) {
		goto error;
	    }
	}
	t->state |= DNS_DATA_TFORM_CLEANUP;
    }

    return;

 error:
    /* set data transform error with error code */
    t->state = DNS_DATA_TFORM_CLEANUP;
    return;
}

static inline int32_t getDNSHeaderParms(dns_data_tform_task_t *t, 
					uint32_t *valid_a_cnt,
					uint32_t *valid_auth_cnt,
					uint32_t *valid_q_cnt,
					uint8_t *aa_flag,
					uint8_t *rcode) {

    int32_t *valid_q_idx = t->out.lookup_status_map;
    uint32_t q_cnt = t->num_q, i = 0;

    for (i = 0; i < q_cnt; i++) {
	if (valid_q_idx[i] <= 0) {
	    (*rcode) = getDNSRcodeFromErr(valid_q_idx[i]);
	    (*valid_q_cnt)++;
	    //(*valid_a_cnt)= 0;
	    // break;
	} else {
	    uint8_t *buf = (uint8_t*)t->out.ce[i];
	    uint32_t buf_size = t->out.ce_resp_size[i];
	    uint8_t *rdata = NULL;
	    
	    rrecord_msg_hdr_t *hdr;
	    rrecord_msg_data_preamble_t *rrp;

	    rrecord_msg_fmt_reader_fill_hdr(buf, 
		    buf_size, (const rrecord_msg_hdr_t **)&hdr);
	    rrecord_msg_fmt_reader_get_record_at(buf, buf_size, i,
			 (const rrecord_msg_data_preamble_t **)&rrp, 
			 (const uint8_t **)&rdata);
	    if (rrp->type == ce_addr_type_ns) {
		(*aa_flag) = 1;
		(*valid_auth_cnt)++;
	    } else {
		(*valid_a_cnt) += hdr->num_records;
	    }

	}	    
    }

    return 0;
}

int32_t buildAllResponses(dns_data_tform_task_t *t) {

    uint32_t valid_q_cnt = t->num_q_valid,
	valid_a_cnt = 0,
	valid_auth_cnt = 0,
	q_cnt = t->num_q, i = 0;
    int32_t *valid_q_idx = t->out.lookup_status_map;
    parsed_dns_msg_t *pr_ctx = NULL;
    ref_count_mem_t *ref_con = t->in.ref_con;
    int32_t rc = 0;
    uint8_t rcode = 0;
    uint8_t aa_flag = 1;

    assert(valid_q_cnt <= MAX_QUERY_COUNT);
 
    getDNSHeaderParms(t, &valid_a_cnt, &valid_auth_cnt, 
		      &valid_q_cnt, &aa_flag, &rcode);
    DBG_LOG(MSG, MOD_NETWORK, "Resp Builder: num valid answers "
	    "%d, num auth count %d, RCODE %d, tot question %d "
	    ",num valid questions %d", valid_a_cnt, valid_auth_cnt, 
	    rcode, q_cnt, valid_q_cnt);

    pr_ctx = t->in.pr_ctx;
    dns_builder_t* bldr = createDNS_Builder(2048);

    /* build DNS header */
    if (valid_q_cnt > 0) {
	if (bldr == NULL) {
	    rc = -1;
	    goto error_return;
	}
	if (bldr->set_msg_hdr_hdlr(bldr,
	   pr_ctx->get_query_id_hdlr(pr_ctx), 1, 0, aa_flag, 0, 0,
	   0,  rcode) < 0) {
	    rc = -2;
	    goto error_return;
	}
	if (bldr->set_cnt_parm_hdlr(bldr, valid_q_cnt, 
		    valid_a_cnt, valid_auth_cnt, 0) < 0) {
	    rc = -3;
	    goto error_return;
	}

	/* build questions */
	for (i = 0; i < q_cnt; i++) {

	    if (1) {//valid_q_idx[i] == 1) {

		char const* name;
		int32_t qtype;
		pr_ctx->get_qname_hdlr(pr_ctx, i, &name);
		qtype = pr_ctx->get_qtype_hdlr(pr_ctx, i);
		if (bldr->add_query_hdlr(bldr, name, qtype, DNS_CLASS_INT)
			< 0) {
		    rc = -4;
		    goto error_return;
		}
	    }
	}

	/* build answers */
	uint32_t pos = 0;
	for (i = 0; i < q_cnt; i++) {

	    if (valid_q_idx[i] == 1) {
		pos = i;
		char const* name;
		uint8_t *resp = (uint8_t *)t->out.ce[pos], *ptr, empty[1] = {'\0'};
		uint32_t resp_size = t->out.ce_resp_size[pos], num_rr, j;
		uint8_t res_ip_num[4];
		uint32_t type;
		uint32_t rr_type;
		uint16_t rr_len = 0;
		uint8_t *rdata = NULL;
		struct in6_addr ipv6_addr;
		rrecord_msg_hdr_t *hdr;
		rrecord_msg_data_preamble_t *rrp;
		
		rrecord_msg_fmt_reader_fill_hdr(resp, 
		    resp_size, (const rrecord_msg_hdr_t **)&hdr);
		num_rr = hdr->num_records;
		for (j = 0; j < num_rr; j++) {
		    rrecord_msg_fmt_reader_get_record_at(resp, resp_size, j,
			 (const rrecord_msg_data_preamble_t **)&rrp, 
			 (const uint8_t **)&rdata);

		    type = rrp->type;
		    if (type == ce_addr_type_ipv4) {
			rr_type = DNS_A;
			rc = convertStrToIP_Num((char *)rdata, res_ip_num);
			if (rc < 0) {
			    rc = -11;
			    DBG_LOG(ERROR, MOD_NETWORK, 
				"error converting domain name to IP "
				"number [err=%d]", rc);
			    goto error_return;
			}
			ptr = res_ip_num;
			rr_len = 4;
		    } else if (type == ce_addr_type_ipv6) {
			inet_pton(AF_INET6, (const char *)rdata, 
				  &ipv6_addr);
			rr_type = DNS_AAAA;
			ptr = (uint8_t*)&ipv6_addr;
			rr_len = sizeof(struct in6_addr);
		    } else if (type == ce_addr_type_cname) {
			rr_type = DNS_CNAME;
			ptr = rdata;
			rr_len = rrp->len;
		    } else if (type == ce_addr_type_ns) {
			rr_type = DNS_NS;
			ptr = rdata;
			rr_len = rrp->len;
			goto skip_ans;
		    }
		    pr_ctx->get_qname_hdlr(pr_ctx, i, &name);
		    if (bldr->add_ans_hdlr(bldr, name, rr_type, 
					   DNS_CLASS_INT, 
					   hdr->ttl, rr_len,
					   (uint8_t*)(ptr)) < 0) {
			rc = -5;
			goto error_return;
		    }
	    skip_ans:
		    pr_ctx->get_qname_hdlr(pr_ctx, i, &name);
		    if (type == ce_addr_type_ns) {
			ptr = rdata;
			rr_len = rrp->len;
			if (bldr->add_auth_hdlr(bldr, name, 
						DNS_NS, DNS_CLASS_INT,
						hdr->ttl, rr_len,
						(uint8_t*)ptr) < 0) {
			    rc = -5;
			    goto error_return;
			}
		    }
		}
		pos++;
	    }
	}
	dns_con_ctx_t* con = (dns_con_ctx_t*)ref_con->mem;
	nw_ctx_t* nw = dns_con_get_nw_ctx(con);
	proto_data_t *pd = dns_con_get_proto_data_safe(con);
	uint8_t const* resp_data = NULL;
	uint8_t *buf = NULL;
	uint32_t resp_data_len = 0, rw_pos = 0, buf_avail = 0;
	uint16_t msg_size = 0, preamble_size = 0;

	if (bldr->get_proto_data_hdlr(bldr, &resp_data, 
				      &resp_data_len) < 0) {
	    dns_con_rel_proto_data(con);
	    rc = -6;
	    goto error_return;
	}

       	pd->get_curr_buf(pd, (uint8_t const**)&buf, 
			 &rw_pos, &buf_avail);
	pd->set_curr_buf_type(pd, PROTO_DATA_BUF_TYPE_SEND);
	msg_size = htons(resp_data_len);
	if (nw->ptype == DNS_TCP) {
	    memcpy(buf + rw_pos, &msg_size, 2);
	    preamble_size = 2;
	} else {
	    /* check for UDP truncation; if message needs to be
	     * truncated then set the truncation bit and proceed
	     * to send the data.
	     * NOTE: the builder does not have API's to modify the
	     * DNS message headers as it is stateful and at this
	     * point we the builder's state would be complete and
	     * will not allow modifications to the header.
	     * the other option would be to discard the build OTW
	     * data and reconstruct it again but that is a tad
	     * expensive and for the moment we simply manipulate
	     * the TC bit inline
	     */
	    if (resp_data_len > 512) {
		uint8_t *b = (uint8_t *)(&resp_data[2]);
		(*b) |= (1 << 1);
		resp_data_len = 512;
	    }
	}
	memcpy(buf + rw_pos + preamble_size, resp_data, 
	       resp_data_len);
	pd->upd_curr_buf_rw_pos(pd, resp_data_len + preamble_size);
	pd->mark_curr_buf(pd);
	dns_con_rel_proto_data(con);
	bldr->delete_hdlr(bldr);
	pr_ctx->delete_hdlr(pr_ctx);
    }
    
    
    return rc;

error_return:
    if (bldr != NULL)
	bldr->delete_hdlr(bldr);
    if (pr_ctx) 
	pr_ctx->delete_hdlr(pr_ctx);
    return rc;

    }

static int32_t buildErrorResponse(dns_data_tform_task_t *t, 
				  int32_t err) {

    parsed_dns_msg_t *pr_ctx = NULL;
    ref_count_mem_t *ref_con = t->in.ref_con;
    int32_t rc = 0;
    uint8_t rcode = 0;

    pr_ctx = t->in.pr_ctx;
    dns_builder_t* bldr = createDNS_Builder(2048);
    rcode = getDNSRcodeFromErr(err);
    
    if (bldr->set_msg_hdr_hdlr(bldr,
       pr_ctx->get_query_id_hdlr(pr_ctx), 1, 0, 1, 0, 0, 0, rcode) < 0) {
	rc = -2;
	goto error_return;
    }
    if (bldr->set_cnt_parm_hdlr(bldr, 0,
				0, 0, 0) < 0) {
	rc = -3;
	goto error_return;
    }
    dns_con_ctx_t* con = (dns_con_ctx_t*)ref_con->mem;
    nw_ctx_t* nw = dns_con_get_nw_ctx(con);
    proto_data_t *pd = dns_con_get_proto_data_safe(con);
    uint8_t const* resp_data = NULL;
    uint8_t *buf = NULL;
    uint32_t resp_data_len = 0, rw_pos = 0;
    uint16_t msg_size = 0, preamble_size = 0;

    pd->get_curr_buf(pd, (uint8_t const**)&buf, 
		     &rw_pos, &resp_data_len);
    pd->set_curr_buf_type(pd, PROTO_DATA_BUF_TYPE_SEND);
    if (bldr->get_proto_data_hdlr(bldr, &resp_data, 
				  &resp_data_len) < 0) {
	dns_con_rel_proto_data(con);
	rc = -6;
	goto error_return;
    }
    msg_size = htons(resp_data_len);
    if (nw->ptype == DNS_TCP) {
	memcpy(buf + rw_pos, &msg_size, 2);
	preamble_size = 2;
    }
    memcpy(buf + rw_pos + preamble_size, resp_data, 
	   resp_data_len);
    pd->upd_curr_buf_rw_pos(pd, resp_data_len + preamble_size);
    pd->mark_curr_buf(pd);
    dns_con_rel_proto_data(con);
    bldr->delete_hdlr(bldr);
    pr_ctx->delete_hdlr(pr_ctx);

    return rc;

 error_return:
    if (bldr != NULL)
	bldr->delete_hdlr(bldr);
    if (pr_ctx) 
	pr_ctx->delete_hdlr(pr_ctx);
    return rc;
}

static inline int32_t
setupResponseSend(ref_count_mem_t *ref_con) {
    int32_t err = 0;
    dns_con_ctx_t *con = NULL;
    nw_ctx_t *nw = NULL;

    con = (dns_con_ctx_t *)ref_con->mem;

    nw = dns_con_get_nw_ctx(con);
    if (nw->ptype == DNS_TCP) {
	nw->disp->set_write_hdlr((event_disp_t*)nw->disp, 
				(entity_data_t*)nw->ed);
    } else {
	err = dns_con_send_data(con);
	ref_con->release_ref_cont(ref_con);
	if (err) {
	    return err;
	}
    }
    
    return err;
}

static uint8_t getDNSRcodeFromErr(int32_t err) {
 
    uint8_t rv = 0;

    switch(-err) {
	case E_DNS_UNSUPPORTED_RR_TYPE:
	    rv = 1;
	    break;
	case E_DNS_QUERY_FAIL:
	    rv = 3;
	    break;
	case E_DNS_UNSUPPORTED_OPCODE:
	    rv = 4;
	    break;
	default:
	    rv = 1;
	    break;
    }

    return rv;
}

int32_t parseCompleteHandler(parsed_dns_msg_t* pr_ctx,
	void* ctx) {

    // printf("Received DNS message.\n");

    int32_t rc = 0;
    uint32_t i = 0;
    uint32_t q_cnt = pr_ctx->get_question_cnt_hdlr(pr_ctx);
    ref_count_mem_t *ref_con = (ref_count_mem_t*)ctx;
    dns_data_tform_task_t *t = NULL;
    dns_con_ctx_t *con = ref_con->mem;
    proto_data_t *pd = dns_con_get_proto_data_safe(con);
    pd->mark_curr_buf(pd);
    dns_con_rel_proto_data(con);

    rc = createDataTransformTask(pr_ctx, ref_con, q_cnt, &t);
    if (rc) {
	goto error;
    }
    ref_con->hold_ref_cont(ref_con);
    processDataTformTask(t);

 error:
    return rc;
}


static inline int32_t
lookupDomainIP(char const* dname, char const* src_ip, 
	       cache_entity_addr_type_t qtype, char** result,
	       uint32_t *result_buf_size) {

    struct sockaddr_un addr;
    const char *store_read_fmt = "SEARCH /DN/%s/CE/?src_ip=%s?qtype=%d\n";
    char buf[1024];
    uint32_t buf_len, ttc = 0, tts = 0, ttr = 0;
    int32_t err = 0, rv = 0;

    if (gl_cp == NULL) {

	DBG_LOG(ERROR, MOD_NETWORK, "Store connections not initialized");
	return -1;
    }
    void* fd = gl_cp->get_cont_hdlr(gl_cp);
    int32_t store_fd = *(int32_t*)(fd);
    if (store_fd < 0) {
	store_fd = makeUnixConnect(STORE_SOCK_FILE);
	if (store_fd < 0) {

	    DBG_LOG(ERROR, MOD_NETWORK, "Connecting Store failed.\n");
	    err = -1;
	    goto error;
	}
    }
    /* read store */

    buf_len = snprintf(buf, 1024, store_read_fmt, 
		       dname, src_ip, qtype);
    uint32_t attempt = 0;
try_again:
    rv = send(store_fd, buf, buf_len, MSG_NOSIGNAL);
    if (rv < 0) {
	close(store_fd);
	if (errno == EPIPE) {
	    DBG_LOG(ERROR, MOD_NETWORK, "error sending store request "
		    "broken pipe [err=%d], retrying attempt %d", 
		    errno, attempt);

	    if (attempt ==0) {
		store_fd = makeUnixConnect(STORE_SOCK_FILE);
		attempt++;
		if (store_fd > 0)
		    goto try_again;
	    }
	}
	err = -errno;
	DBG_LOG(ERROR, MOD_NETWORK, "error sending store request "
		"[err=%d]", err);
	goto error;
    }
    buf_len = 1024;
    rv = timed_nw_read(store_fd, buf, buf_len, 10000);
    if (rv <= 0) {
	err = rv;
	glob_dns_store_read_timeo_cnt++;
	DBG_LOG(ERROR, MOD_NETWORK, "error receiving store response "
		"[err=%d]", err);
	goto error;
    }
    if (rv > 6) {
	if (strncmp(buf, "200 OK", 6) == 0) {
	    char* cont = memchr(buf, '\n', rv);
	    if (cont == NULL) {

		DBG_LOG(ERROR, MOD_NETWORK, "Invalid response from store");
		err = -1;
	    } else {
		cont++;
		uint32_t cont_len = (buf + rv) - cont;
		*result = (char *)nkn_malloc_type(cont_len, 102);
		if (!*result) {
		    err = -ENOMEM;
		    goto error;
		}
		memcpy(*result, cont, cont_len);
		*result_buf_size = cont_len;
		err = cont_len;
	    }
	} else {
	    DBG_LOG(ERROR, MOD_NETWORK, "Invalid response/No result found\
		    in store");
	    err = -E_DNS_QUERY_FAIL;
	}
    } else {
	DBG_LOG(ERROR, MOD_NETWORK, "Invalid response/No result found\
		in store");
	err = -E_DNS_QUERY_FAIL;
    }

error:
    *((int32_t*)fd) = store_fd;
    gl_cp->put_cont_hdlr(gl_cp, fd);

    return err;

}


static int32_t convertStrToIP_Num(char const* result, uint8_t* res_ip_num) {

    uint32_t word_cnt = 0;
    uint32_t in_len = strlen(result);
    char const* pos1 = result, *pos2 = NULL;
    int32_t rc = 0;
    char* tailptr = alloca(4);
    while (word_cnt <= 3) {

	pos2 = strchr(pos1, '.');
	if ((pos2 == NULL) && (word_cnt == 3)) {
	    pos2 = result + in_len; 
	} else if (pos2 == NULL) {
	    rc = -2;
	    break;
	}
	char tmp[4] = {'\0', '\0', '\0', '\0'};
	memset(tailptr, '\0', 4);
	if ((pos2 - pos1) > 3) {
	    rc = -4;
	    break;
	}
	strncpy(&tmp[0], pos1, pos2 - pos1);
	uint64_t value = strtol(&tmp[0], &tailptr, 0);
	if (tailptr[0] != '\0') {
	    rc = -1;
	    break;
	}
	if (value > 255) {
	    rc = -3;
	    break;
	}
	res_ip_num[word_cnt++] = (uint8_t)value;
	if (word_cnt <= 3)
	    pos1 = pos2 + 1;
    }
    return rc;
}

cache_entity_addr_type_t convertToCacheEntityAddr(
			  uint32_t dns_type) {
    
    cache_entity_addr_type_t rv;
    switch(dns_type) {
	case DNS_A:
	    rv = ce_addr_type_ipv4;
	    break;
	case DNS_AAAA:
	    rv = ce_addr_type_ipv6;
	    break;
	case DNS_CNAME:
	    rv = ce_addr_type_cname;
	    break;
	case DNS_ALL:
	    rv = ce_addr_type_max;
	    break;
	default:
	    rv = ce_addr_type_none;
	    break;
    }
    
    return rv;
}
	

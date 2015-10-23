#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "dns_parser.h"
#include "nkn_memalloc.h"


typedef struct dns_hdr {

    uint16_t query_id;
    uint8_t qr;
    uint8_t op_code;
    uint8_t aa;
    uint8_t tc;
    uint8_t rd;
    uint8_t ra;
    uint8_t res;
    uint8_t rcode;
    uint16_t q_count;
    uint16_t ans_count;
    uint16_t auth_count;
    uint16_t addit_count;
} dns_hdr_t;


typedef struct dns_query_msg {

    char name[MAX_DNS_QNAME_LEN];
    uint16_t type;
    uint16_t cl;
} dns_query_msg_t;


typedef struct dns_resp_msg {

    char qname[MAX_DNS_QNAME_LEN];
    uint16_t type;
    uint16_t cl;
    uint32_t ttl;
    uint16_t rdata_len;
    uint8_t rdata[MAX_DNS_RESP_LEN];
} dns_resp_msg_t;


typedef struct local_parse_ctx {

    dns_hdr_t hdr;
    dns_query_msg_t* query_msg;
    dns_resp_msg_t* ans_msg;
    dns_resp_msg_t* auth_msg;
    dns_resp_msg_t* addit_msg;
} local_parse_ctx_t;


typedef enum {

    TCP_PREFIX,
    DNS_HDR,
    QUERY_MSG,
    ANS_MSG,
    AUTH_MSG,
    ADDIT_MSG,
    COMPLETE,
    ERROR
} STATE;


typedef struct local_ctx {

    uint8_t* rbuf;
    uint32_t max_rbuf_len;
    uint32_t rbuf_read_pos;
    uint32_t rbuf_write_pos;

    uint32_t track_q_cnt;
    uint32_t track_ans_cnt;
    uint32_t track_auth_cnt;
    uint32_t track_addit_cnt;
    uint32_t over_tcp_flag;
    uint16_t tcp_len_prefix;
    STATE rstate;
    local_parse_ctx_t* pctx;
    registered_dns_msg_hdlr_fptr reg_msg_hdlr;
    void* reg_ctx;

}local_ctx_t;


static int32_t parse(dns_pr_t* pr, uint8_t const* data, uint32_t len);
static int32_t delete(dns_pr_t* pr);
static int32_t set_transport_type(dns_pr_t *pr, uint32_t over_tcp_flag);

static int32_t parseDNS_Hdr(uint8_t const* data, uint32_t len, dns_hdr_t* hdr); 
static int32_t parseQueryMsg(uint8_t const* data, uint32_t r_offset,
			     uint32_t len, dns_query_msg_t* query_msg); 
static int32_t parseResponseMsg(uint8_t const* data, uint32_t r_off,
				uint32_t len, dns_resp_msg_t* resp_msg); 
static int32_t decodeNameLabel(uint8_t const* data, uint32_t r_offset, 
			       uint32_t len, char* name);
static void set_parse_complete_hdlr(dns_pr_t *pr,
				    registered_dns_msg_hdlr_fptr reg_msg_hdlr,
				    void *reg_ctx);

static parsed_dns_msg_t* createDNS_ParseResultCont(local_parse_ctx_t* ctx); 

dns_pr_t* createDNS_Handler(uint32_t max_rbuf_len) {
    
    uint32_t over_tcp_flag = 0;
  
    if (max_rbuf_len == 0) 
	return NULL;
    dns_pr_t* pr = calloc(1, sizeof(dns_pr_t));
    if (pr == NULL)
	return NULL;
    local_ctx_t* lctx = calloc(1, sizeof(local_ctx_t));
    if (lctx == NULL) {
	free(pr);
	return NULL;
    }
    lctx->rbuf = nkn_malloc_type(max_rbuf_len, mod_dns_parser_buf);
    if (lctx->rbuf == NULL) {
	free(lctx);
	free(pr);
	return NULL;
    }
    lctx->pctx = calloc(1, sizeof(local_parse_ctx_t));
    if (lctx->pctx == NULL) {
	free(lctx->rbuf);
	free(lctx);
	free(pr);
	return NULL;
    }
    lctx->over_tcp_flag = over_tcp_flag;
    lctx->max_rbuf_len = max_rbuf_len;
    lctx->rbuf_read_pos = lctx->rbuf_write_pos = 0;
//  lctx->reg_ctx = reg_ctx;
    lctx->track_q_cnt = lctx->track_ans_cnt = lctx->track_auth_cnt =
	lctx->track_addit_cnt = 0;
    if (lctx->over_tcp_flag == 1)
	lctx->rstate = TCP_PREFIX;
    else
	lctx->rstate = DNS_HDR;
//   lctx->reg_msg_hdlr = reg_msg_hdlr;
    pr->__internal = lctx;

    pr->set_transport_hdlr = set_transport_type;
    pr->set_parse_complete_hdlr = set_parse_complete_hdlr;
    pr->parse_hdlr = parse;
    pr->delete_hdlr = delete;
    return pr;
}

static void set_parse_complete_hdlr(dns_pr_t *pr,
	    registered_dns_msg_hdlr_fptr reg_msg_hdlr,
	    void *reg_ctx) {

    assert(pr);
    local_ctx_t *lctx = NULL;
    
    lctx = pr->__internal;
    lctx->reg_msg_hdlr = reg_msg_hdlr;
    lctx->reg_ctx = reg_ctx;

    return;
}

static int32_t set_transport_type(dns_pr_t *pr, 
				  uint32_t over_tcp_flag) {
    
    local_ctx_t *lctx = NULL;
    assert(pr);
    
    lctx = (local_ctx_t *)pr->__internal;
    if ((over_tcp_flag != 0) && (over_tcp_flag != 1))
	return -1;
    lctx->over_tcp_flag = over_tcp_flag;
    if (lctx->over_tcp_flag == 1)
	lctx->rstate = TCP_PREFIX;
    else
	lctx->rstate = DNS_HDR;
    return 0;
}
static int32_t delete(dns_pr_t* pr) {

    local_ctx_t* lctx = (local_ctx_t*)pr->__internal;
    free(lctx->rbuf);
    if (lctx->pctx->query_msg != NULL)
	free(lctx->pctx->query_msg);
    if (lctx->pctx->ans_msg != NULL)
	free(lctx->pctx->ans_msg);
    if (lctx->pctx->auth_msg != NULL)
	free(lctx->pctx->auth_msg);
    if (lctx->pctx->addit_msg != NULL)
	free(lctx->pctx->addit_msg);
    free(lctx->pctx);
    free(lctx);
    free(pr);
    return 0;
}


static int32_t parse(dns_pr_t* pr, uint8_t const* data, uint32_t len) {

    local_ctx_t* lctx = (local_ctx_t*)pr->__internal;
    if (lctx->max_rbuf_len - lctx->rbuf_write_pos < len) {
	lctx->rstate = ERROR;
	return -1;
    }
    memcpy(lctx->rbuf + lctx->rbuf_write_pos, data, len);
    lctx->rbuf_write_pos += len;

    uint32_t rc = 0;
    while (rc == 0) {

	switch (lctx->rstate) {
	    case TCP_PREFIX:
		{
		    if ((lctx->rbuf_write_pos - lctx->rbuf_read_pos) < 2)
			return 0;
		    uint16_t plen = *(uint16_t*)(lctx->rbuf);
		    lctx->tcp_len_prefix = ntohs(plen);
		    lctx->rbuf_read_pos += 2;
		    lctx->rstate = DNS_HDR;
		    break;
		}
	    case DNS_HDR: 
		{
		    int32_t read_cnt = parseDNS_Hdr(lctx->rbuf
						    + lctx->rbuf_read_pos, 
						    lctx->rbuf_write_pos - lctx->rbuf_read_pos,
						    &lctx->pctx->hdr);
		    if (read_cnt > 0) { 

			if ((lctx->pctx->hdr.q_count > MAX_QUERY_COUNT)
			    || (lctx->pctx->hdr.ans_count > MAX_RESP_COUNT)
			    || (lctx->pctx->hdr.auth_count > MAX_RESP_COUNT)
			    || (lctx->pctx->hdr.addit_count >
				MAX_RESP_COUNT)) {

			    lctx->rstate = ERROR;
			    break;
			}
			lctx->pctx->query_msg =
			    nkn_calloc_type(lctx->pctx->hdr.q_count, 
					    sizeof(dns_query_msg_t),
					    mod_dns_parser_token_data);
			lctx->pctx->ans_msg =
			    nkn_calloc_type(lctx->pctx->hdr.ans_count, 
					    sizeof(dns_resp_msg_t),
					    mod_dns_parser_token_data);
			lctx->pctx->auth_msg = 
			    nkn_calloc_type(lctx->pctx->hdr.auth_count,
					    sizeof(dns_resp_msg_t),
					    mod_dns_parser_token_data);
			lctx->pctx->addit_msg =
			    nkn_calloc_type(lctx->pctx->hdr.addit_count, 
					    sizeof(dns_resp_msg_t),
					    mod_dns_parser_token_data);
			if ((lctx->pctx->query_msg == NULL)
			    || (lctx->pctx->ans_msg == NULL)
			    || (lctx->pctx->auth_msg == NULL)
			    || (lctx->pctx->addit_msg == NULL)) {
			    if (lctx->pctx->query_msg != NULL)
				free(lctx->pctx->query_msg);
			    if (lctx->pctx->ans_msg != NULL)
				free(lctx->pctx->ans_msg);
			    if (lctx->pctx->auth_msg != NULL)
				free(lctx->pctx->auth_msg);
			    if (lctx->pctx->addit_msg != NULL)
				free(lctx->pctx->addit_msg);
			    lctx->rstate = ERROR;
			} else {
			    lctx->rbuf_read_pos += read_cnt;
			    lctx->rstate = QUERY_MSG;
			}
		    } else
			return 0;
		    break;
		}
	    case QUERY_MSG:
		{
		    int32_t read_cnt = parseQueryMsg(lctx->rbuf, 
						     lctx->rbuf_read_pos, 
						     lctx->rbuf_write_pos - lctx->rbuf_read_pos,
						     &lctx->pctx->query_msg[lctx->track_q_cnt]);
		    if (read_cnt > 0) {
			lctx->rbuf_read_pos += read_cnt;
			lctx->track_q_cnt++;
			if (lctx->track_q_cnt < lctx->pctx->hdr.q_count)
			    continue;
			if (lctx->pctx->hdr.ans_count > 0)
			    lctx->rstate = ANS_MSG;
			else if (lctx->pctx->hdr.auth_count > 0)
			    lctx->rstate = AUTH_MSG;
			else if (lctx->pctx->hdr.addit_count > 0)
			    lctx->rstate = ADDIT_MSG;
			else
			    lctx->rstate = COMPLETE;
		    } else if (read_cnt < 0) {
			lctx->rstate = ERROR;
		    } else
			return 0;
		    break;
		}
	    case ANS_MSG:
		{
		    int32_t read_cnt = parseResponseMsg(lctx->rbuf, 
							lctx->rbuf_read_pos,
							lctx->rbuf_write_pos - lctx->rbuf_read_pos,
							&lctx->pctx->ans_msg[lctx->track_ans_cnt]);
		    if (read_cnt > 0) { 
			lctx->rbuf_read_pos += read_cnt;
			lctx->track_ans_cnt++;
			if (lctx->track_ans_cnt < lctx->pctx->hdr.ans_count)
			    continue;
			if (lctx->pctx->hdr.auth_count > 0)
			    lctx->rstate = AUTH_MSG;
			else if (lctx->pctx->hdr.addit_count > 0)
			    lctx->rstate = ADDIT_MSG;
			else
			    lctx->rstate = COMPLETE;
		    } else if (read_cnt < 0) {
			lctx->rstate = ERROR;
		    } else
			return 0;
		    break;
		}
	    case AUTH_MSG:
		{
		    int32_t read_cnt = parseResponseMsg(lctx->rbuf, 
							lctx->rbuf_read_pos,
							lctx->rbuf_write_pos - lctx->rbuf_read_pos,
							&lctx->pctx->auth_msg[lctx->track_auth_cnt]);
		    if (read_cnt > 0) {
			lctx->rbuf_read_pos += read_cnt;
			lctx->track_auth_cnt++;
			if (lctx->track_auth_cnt < lctx->pctx->hdr.auth_count)
			    continue;
			if (lctx->pctx->hdr.addit_count > 0)
			    lctx->rstate = ADDIT_MSG;
			else
			    lctx->rstate = COMPLETE;
		    } else if (read_cnt < 0) {
			lctx->rstate = ERROR;
		    } else
			return 0;
		    break;
		}
	    case ADDIT_MSG:
		{
		    int32_t read_cnt = parseResponseMsg(lctx->rbuf, 
							lctx->rbuf_read_pos,
							lctx->rbuf_write_pos - lctx->rbuf_read_pos,
							&lctx->pctx->addit_msg[lctx->track_addit_cnt]);
		    if (read_cnt > 0) { 
			lctx->rbuf_read_pos += read_cnt;
			lctx->track_addit_cnt++;
			if (lctx->track_addit_cnt< lctx->pctx->hdr.addit_count)
			    continue;
			lctx->rstate = COMPLETE;

		    } else if (read_cnt < 0) {
			lctx->rstate = ERROR;
		    } else
			return 0;
		    break;
		}
	    case COMPLETE:
		{
		    local_parse_ctx_t* ctx =calloc(1,sizeof(local_parse_ctx_t));
		    if (ctx == NULL) {
			lctx->rstate = ERROR;
			break;
		    }
		    parsed_dns_msg_t* prctx = 
			createDNS_ParseResultCont(lctx->pctx);
		    if (prctx == NULL) {
			lctx->rstate = ERROR;
			break;
		    }
		    lctx->pctx = ctx; 
		    uint32_t rem_len = lctx->rbuf_write_pos -
			lctx->rbuf_read_pos;
		    memcpy(lctx->rbuf, lctx->rbuf+lctx->rbuf_read_pos, rem_len);
		    lctx->rbuf_read_pos = 0;
		    lctx->rbuf_write_pos = rem_len;
		    lctx->track_q_cnt = lctx->track_ans_cnt = 
			lctx->track_auth_cnt = lctx->track_addit_cnt = 0;
		    if (lctx->over_tcp_flag == 0)
			lctx->rstate = DNS_HDR;
		    else
			lctx->rstate = TCP_PREFIX;
		    lctx->reg_msg_hdlr(prctx, lctx->reg_ctx);
		    break;
		}
	    case ERROR:
		{
		    rc = -1;
		    break;
		}
	}
    }
    return rc;
}


static int32_t parseDNS_Hdr(uint8_t const* data, uint32_t len, dns_hdr_t* hdr) {

    if (len < 12)
	return 0;
    uint16_t qid = *(uint16_t*)data;
    hdr->query_id = ntohs(qid);
    uint8_t b3 = data[2];
    uint8_t b4 = data[3];
    hdr->qr = (b3 & 0x80) >> 7;
    hdr->op_code = (b3 & 0x78) >> 3;;
    hdr->aa = (b3 & 0x04) >> 2;
    hdr->tc = (b3 & 0x02) >> 1;
    hdr->rd = b3 & 0x01;
    hdr->ra = (b4 & 0x80) >> 7;
    hdr->res = (b4 & 0x70) >> 4;
    hdr->rcode = b4 & 0x0F;
    uint16_t cnt = *(uint16_t*)(data + 4);
    hdr->q_count = ntohs(cnt);
    cnt = *(uint16_t*)(data + 6);
    hdr->ans_count = ntohs(cnt);
    cnt = *(uint16_t*)(data + 8);
    hdr->auth_count = ntohs(cnt);
    cnt = *(uint16_t*)(data + 10);
    hdr->addit_count = ntohs(cnt);
    return 12;
}


static int32_t parseQueryMsg(uint8_t const* base, uint32_t r_offset, 
			     uint32_t len, dns_query_msg_t* query_msg) {

    uint8_t const* data = base + r_offset;
    int32_t rlen = decodeNameLabel(base, r_offset, len, &query_msg->name[0]);
    if (rlen > 0) {

	if ((len - rlen) < 4)
	    return 0;
	query_msg->type = *(uint16_t*)(data + rlen);
	query_msg->type = ntohs(query_msg->type);
	rlen += 2;
	query_msg->cl = *(uint16_t*)(data + rlen);
	query_msg->cl = ntohs(query_msg->cl);
	rlen += 2;
    }
    return rlen;
}


static int32_t parseResponseMsg(uint8_t const* base, uint32_t r_offset,
				uint32_t len, dns_resp_msg_t* resp_msg) {

    uint8_t const* data = base + r_offset;
    int32_t rlen = decodeNameLabel(base, r_offset, len, &resp_msg->qname[0]);
    if (rlen > 0) {

	if ((len - rlen) < 10)
	    return 0;
	resp_msg->type = *(uint16_t*)(data + rlen);
	resp_msg->type = ntohs(resp_msg->type);
	rlen += 2;
	resp_msg->cl = *(uint16_t*)(data + rlen);
	resp_msg->cl = ntohs(resp_msg->cl);
	rlen += 2;
	resp_msg->ttl = *(uint32_t*)(data + rlen);
	resp_msg->ttl = ntohl(resp_msg->ttl);
	rlen += 4;
	resp_msg->rdata_len = *(uint16_t*)(data + rlen);
	resp_msg->rdata_len = ntohs(resp_msg->rdata_len);
	rlen += 2;
	if ((len - rlen) < resp_msg->rdata_len)
	    return 0;
	if (resp_msg->rdata_len > MAX_DNS_RESP_LEN) {
	    assert(0);
	    return -1;
	}
	memcpy(&resp_msg->rdata[0], data + rlen, resp_msg->rdata_len);
	rlen += resp_msg->rdata_len;
    }
    return rlen;
}


static int32_t decodeNameLabel(uint8_t const* base, uint32_t r_offset, 
			       uint32_t len, char* name) {

    uint8_t const* data = base + r_offset;
    uint32_t cflag = 0;
    uint16_t offset = 0;
    if (data[0]  >= 64) {

	offset = data[0];
	offset <<= 10;
	offset += data[1];
	data = base + offset;
	cflag = 1;
	if (len < 2)
	    return 0;
	if (offset >= r_offset)
	    return -1;
    }
    uint32_t pos = 0, l_len = 0;
    while (1) {

	if (data[pos] == '\0') {
	    name[pos] = '\0';
	    if (cflag == 0) {
		if ((pos + 1) >= len)
		    return 0;
		if ((pos + 1) >= MAX_DNS_RESP_LEN)
		    return -1;
	    } else if ((offset + pos + 1) >= r_offset)
		return -1;
	    pos++;
	    break;
	}
	l_len = data[pos];
	name[pos] = '.';
	if (cflag == 0) {
	    if ((pos + 1) >= len)
		return 0;
	    if ((pos + 1) >= MAX_DNS_RESP_LEN)
		return -1;
	} else if ((offset + pos + 1) >= r_offset)
	    return -1;
	pos++;
	if (cflag == 0) {
	    if ((pos + l_len) >= len)
		return 0;
	    if ((pos + l_len) >= MAX_DNS_RESP_LEN)
		return -1;
	} else if ((offset + pos + l_len) >= r_offset)
	    return -1;
	memcpy(name + pos, data + pos, l_len);
	pos += l_len;
    }
    if (cflag == 1)
	return 2;
    return pos;
}


static uint32_t getQueryID(parsed_dns_msg_t const* pctx);
static uint32_t getQR(parsed_dns_msg_t const* pctx);
static uint32_t getOpCode(parsed_dns_msg_t const* pctx);
static uint32_t getAA(parsed_dns_msg_t const* pctx);
static uint32_t getTC(parsed_dns_msg_t const* pctx);
static uint32_t getRD(parsed_dns_msg_t const* pctx);
static uint32_t getRA(parsed_dns_msg_t const* pctx);
static uint32_t getRCode(parsed_dns_msg_t const* pctx);
static uint32_t getQnCount(parsed_dns_msg_t const* pctx);
static uint32_t getArCount(parsed_dns_msg_t const* pctx);
static uint32_t getAuCount(parsed_dns_msg_t const* pctx);
static uint32_t getAdCount(parsed_dns_msg_t const* pctx);

static int32_t getQueryName(parsed_dns_msg_t const* pctx, uint32_t idx,
			    char const** addr);
static int32_t getQueryType(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getQueryClass(parsed_dns_msg_t const* pctx, uint32_t idx);

static int32_t getAnsQName(parsed_dns_msg_t const* pctx, uint32_t idx,
			   char const** addr);
static int32_t getAnsType(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAnsClass(parsed_dns_msg_t const* pctx, uint32_t idx);
static int64_t getAnsTTL(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAnsRD_Length(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAnsRD_Data(parsed_dns_msg_t const* pctx, uint32_t idx,
			     uint8_t const** data, uint32_t* data_len);

static int32_t getAuthQName(parsed_dns_msg_t const* pctx, uint32_t idx,
			    char const** addr);
static int32_t getAuthType(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAuthClass(parsed_dns_msg_t const* pctx, uint32_t idx);
static int64_t getAuthTTL(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAuthRD_Length(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAuthRD_Data(parsed_dns_msg_t const* pctx, uint32_t idx,
			      uint8_t const** data, uint32_t* data_len);

static int32_t getAdditQName(parsed_dns_msg_t const* pctx, uint32_t idx,
			     char const** addr);
static int32_t getAdditType(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAdditClass(parsed_dns_msg_t const* pctx, uint32_t idx);
static int64_t getAdditTTL(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAdditRD_Length(parsed_dns_msg_t const* pctx, uint32_t idx);
static int32_t getAdditRD_Data(parsed_dns_msg_t const* pctx, uint32_t idx,
			       uint8_t const** data, uint32_t* data_len);

static void printVal(parsed_dns_msg_t const* pctx);

static void deleteParsedResult(parsed_dns_msg_t* pctx);

static parsed_dns_msg_t* createDNS_ParseResultCont(local_parse_ctx_t* ctx) {

    if (ctx == NULL)
	return NULL;
    parsed_dns_msg_t* pctx = calloc(1, sizeof(parsed_dns_msg_t));
    if (pctx == NULL)
	return NULL;
    pctx->__internal = ctx;
    pctx->get_query_id_hdlr = getQueryID;
    pctx->get_qr_hdlr = getQR;
    pctx->get_opcode_hdlr = getOpCode;
    pctx->get_aa_hdlr = getAA;
    pctx->get_tc_hdlr = getTC;
    pctx->get_rd_hdlr = getRD;
    pctx->get_ra_hdlr = getRA;
    pctx->get_rcode_hdlr = getRCode;
    pctx->get_question_cnt_hdlr = getQnCount;
    pctx->get_answer_cnt_hdlr = getArCount;
    pctx->get_authority_cnt_hdlr = getAuCount;
    pctx->get_addit_rc_cnt_hdlr = getAdCount;

    pctx->get_qname_hdlr = getQueryName;
    pctx->get_qtype_hdlr = getQueryType;
    pctx->get_qclass_hdlr = getQueryClass;

    pctx->get_ans_qname_hdlr = getAnsQName;
    pctx->get_ans_type_hdlr = getAnsType;
    pctx->get_ans_class_hdlr = getAnsClass;
    pctx->get_ans_ttl_hdlr = getAnsTTL;
    pctx->get_ans_rdlen_hdlr = getAnsRD_Length;
    pctx->get_ans_rdata_hdlr = getAnsRD_Data;

    pctx->get_auth_qname_hdlr = getAuthQName;
    pctx->get_auth_type_hdlr = getAuthType;
    pctx->get_auth_class_hdlr = getAuthClass;
    pctx->get_auth_ttl_hdlr = getAuthTTL;
    pctx->get_auth_rdlen_hdlr = getAuthRD_Length;
    pctx->get_auth_rdata_hdlr = getAuthRD_Data;

    pctx->get_addit_qname_hdlr = getAdditQName;
    pctx->get_addit_type_hdlr = getAdditType;
    pctx->get_addit_class_hdlr = getAdditClass;
    pctx->get_addit_ttl_hdlr = getAdditTTL;
    pctx->get_addit_rdlen_hdlr = getAdditRD_Length;
    pctx->get_addit_rdata_hdlr = getAdditRD_Data;

    pctx->print_msgval_hdlr = printVal;

    pctx->delete_hdlr = deleteParsedResult;

    return pctx;
}


static uint32_t getQueryID(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.query_id;
}

static uint32_t getQR(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.qr;
}

static uint32_t getOpCode(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.op_code;
}

static uint32_t getAA(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.aa;
}

static uint32_t getTC(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.tc;
}

static uint32_t getRD(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.rd;
}

static uint32_t getRA(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.ra;
}

static uint32_t getRCode(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.rcode;
}

static uint32_t getQnCount(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.q_count;
}

static uint32_t getArCount(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.ans_count;
}

static uint32_t getAuCount(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.auth_count;
}

static uint32_t getAdCount(parsed_dns_msg_t const* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    return pctx->hdr.addit_count;
}

static int32_t getQueryName(parsed_dns_msg_t const* ctx, uint32_t idx,
			    char const** addr) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.q_count)
	return -1;
    *addr = &pctx->query_msg[idx].name[0];
    if (*addr == NULL)
	return -1;
    return 0;
}

static int32_t getQueryType(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.q_count)
	return -1;
    return pctx->query_msg[idx].type;
}

static int32_t getQueryClass(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.q_count)
	return -1;
    return pctx->query_msg[idx].cl;
}

static int32_t getAnsQName(parsed_dns_msg_t const* ctx, uint32_t idx,
			   char const** data) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.ans_count)
	return -1;
    *data = &pctx->ans_msg[idx].qname[0];
    if (*data == NULL)
	return -1;
    return 0;
}

static int32_t getAnsType(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.ans_count)
	return -1;
    return pctx->ans_msg[idx].type;
}

static int32_t getAnsClass(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.ans_count)
	return -1;
    return pctx->ans_msg[idx].cl;
}

static int64_t getAnsTTL(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.ans_count)
	return -1;
    return pctx->ans_msg[idx].ttl;
}

static int32_t getAnsRD_Length(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.ans_count)
	return -1;
    return pctx->ans_msg[idx].rdata_len;
}

static int32_t getAnsRD_Data(parsed_dns_msg_t const* ctx, uint32_t idx,
			     uint8_t const** data, uint32_t* data_len) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.ans_count)
	return -1;
    *data_len = pctx->ans_msg[idx].rdata_len;
    *data = pctx->ans_msg[idx].rdata; 
    return 0;
}


static int32_t getAuthQName(parsed_dns_msg_t const* ctx, uint32_t idx,
			    char const** data) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.auth_count)
	return -1;
    *data = &pctx->auth_msg[idx].qname[0];
    if (*data == NULL)
	return -1;
    return 0;
}

static int32_t getAuthType(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.auth_count)
	return -1;
    return pctx->auth_msg[idx].type;
}

static int32_t getAuthClass(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.auth_count)
	return -1;
    return pctx->auth_msg[idx].cl;
}

static int64_t getAuthTTL(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.auth_count)
	return -1;
    return pctx->auth_msg[idx].ttl;
}

static int32_t getAuthRD_Length(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.auth_count)
	return -1;
    return pctx->auth_msg[idx].rdata_len;
}

static int32_t getAuthRD_Data(parsed_dns_msg_t const* ctx, uint32_t idx,
			      uint8_t const** data, uint32_t* data_len) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.auth_count)
	return -1;
    *data_len = pctx->auth_msg[idx].rdata_len;
    *data = pctx->auth_msg[idx].rdata; 
    return 0;

}

static int32_t getAdditQName(parsed_dns_msg_t const* ctx, uint32_t idx,
			     char const** data) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.addit_count)
	return -1;
    *data = &pctx->addit_msg[idx].qname[0];
    if (*data == NULL)
	return -1;
    return 0;
}

static int32_t getAdditType(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.addit_count)
	return -1;
    return pctx->addit_msg[idx].type;
}

static int32_t getAdditClass(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.addit_count)
	return -1;
    return pctx->addit_msg[idx].cl;
}

static int64_t getAdditTTL(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.addit_count)
	return -1;
    return pctx->addit_msg[idx].ttl;
}

static int32_t getAdditRD_Length(parsed_dns_msg_t const* ctx, uint32_t idx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.addit_count)
	return -1;
    return pctx->addit_msg[idx].rdata_len;
}

static int32_t getAdditRD_Data(parsed_dns_msg_t const* ctx, uint32_t idx,
			       uint8_t const** data, uint32_t* data_len) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    if (idx >= pctx->hdr.addit_count)
	return -1;
    *data_len = pctx->addit_msg[idx].rdata_len;
    *data = pctx->addit_msg[idx].rdata; 
    return 0;
}

static void printVal(parsed_dns_msg_t const* pctx) {

    printf("############# DNS Header ##############\n");
    printf("Query id    : %u\n", pctx->get_query_id_hdlr(pctx));
    printf("QR          : %u\n", pctx->get_qr_hdlr(pctx));
    printf("OP Code     : %u\n", pctx->get_opcode_hdlr(pctx));
    printf("AA          : %u\n", pctx->get_aa_hdlr(pctx));
    printf("TC          : %u\n", pctx->get_tc_hdlr(pctx));
    printf("RD          : %u\n", pctx->get_rd_hdlr(pctx));
    printf("RA          : %u\n", pctx->get_ra_hdlr(pctx));
    printf("R Code      : %u\n", pctx->get_rcode_hdlr(pctx));
    printf("Qn Count    : %u\n", pctx->get_question_cnt_hdlr(pctx));
    printf("Ans Count   : %u\n", pctx->get_answer_cnt_hdlr(pctx));
    printf("Auth Count  : %u\n", pctx->get_authority_cnt_hdlr(pctx));
    printf("Addit Count : %u\n", pctx->get_addit_rc_cnt_hdlr(pctx));
    printf("############# Query part ##############\n");
    uint32_t i;
    for (i = 0;i < pctx->get_question_cnt_hdlr(pctx); i++) {

	char const*name;
	pctx->get_qname_hdlr(pctx, i, &name);
	printf("Query Name  : %s\n", name); 
	printf("Query Type  : %u\n", pctx->get_qtype_hdlr(pctx, i));
	printf("Query Class : %u\n", pctx->get_qclass_hdlr(pctx, i));
	printf("---------------------------------------\n");
    }
    printf("############# Answer part #############\n");
    for (i = 0;i < pctx->get_answer_cnt_hdlr(pctx); i++) {

	char const* name;
	pctx->get_ans_qname_hdlr(pctx, i, &name);
	printf("Query Name  : %s\n", name); 
	printf("Type        : %u\n", pctx->get_ans_type_hdlr(pctx, i));
	printf("Class       : %u\n", pctx->get_ans_class_hdlr(pctx, i));
	printf("TTL         : %u\n", (uint32_t)pctx->get_ans_ttl_hdlr(pctx, i));
	printf("RD length   : %u\n", pctx->get_ans_rdlen_hdlr(pctx, i));
	printf("RDATA       : \n");
	uint8_t const* data;
	uint32_t rdlen = 0;
	pctx->get_ans_rdata_hdlr(pctx, i, &data, &rdlen);
	uint32_t j = 0;
	for (j = 0; j <  rdlen; j++) {

	    if ((j != 0) && (j % 16 == 0))
		printf("\n");
	    printf("%02X", data[j]);
	}
	printf("\n---------------------------------------\n");
    }
    printf("############# Auth part ###############\n");
    for (i = 0;i < pctx->get_authority_cnt_hdlr(pctx); i++) {

	char const* name;
	pctx->get_auth_qname_hdlr(pctx, i, &name);
	printf("Query Name  : %s\n", name); 
	printf("Type        : %u\n", pctx->get_auth_type_hdlr(pctx, i));
	printf("Class       : %u\n", pctx->get_auth_class_hdlr(pctx, i));
	printf("TTL         : %ld\n", pctx->get_auth_ttl_hdlr(pctx, i));
	printf("RD length   : %u\n", pctx->get_auth_rdlen_hdlr(pctx, i));
	printf("RDATA       : \n");
	uint8_t const* data;
	uint32_t rdlen = 0;
	pctx->get_auth_rdata_hdlr(pctx, i, &data, &rdlen);
	uint32_t j = 0;
	for (j = 0; j <  rdlen; j++) {

	    if ((j != 0) && (j % 16 == 0))
		printf("\n");
	    printf("%02X", data[j]);
	}
	printf("\n---------------------------------------\n");
    }
    printf("############# Addit part ##############\n");
    for (i = 0;i < pctx->get_addit_rc_cnt_hdlr(pctx) ; i++) {

	char const* name;
	pctx->get_addit_qname_hdlr(pctx, i, &name);
	printf("Query Name  : %s\n", name); 
	printf("Type        : %u\n", pctx->get_addit_type_hdlr(pctx, i));
	printf("Class       : %u\n", pctx->get_addit_class_hdlr(pctx, i));
	printf("TTL         : %ld\n", pctx->get_addit_ttl_hdlr(pctx, i));
	printf("RD length   : %u\n", pctx->get_addit_rdlen_hdlr(pctx, i));
	printf("RDATA       : \n");
	uint8_t const* data;
	uint32_t rdlen = 0;
	pctx->get_addit_rdata_hdlr(pctx, i, &data, &rdlen);
	uint32_t j = 0;
	for (j = 0; j < rdlen; j++) {

	    if ((j != 0) && (j % 16 == 0))
		printf("\n");
	    printf("%02X", data[j]);
	}
	printf("\n---------------------------------------\n");
    }
    printf("\n--------------- END -------------------\n");
}

static void deleteParsedResult(parsed_dns_msg_t* ctx) {

    local_parse_ctx_t* pctx = (local_parse_ctx_t*)ctx->__internal;
    free(pctx->query_msg);
    if (pctx->ans_msg != NULL)
	free(pctx->ans_msg);
    if (pctx->auth_msg != NULL)
	free(pctx->auth_msg);
    if (pctx->addit_msg != NULL)
	free(pctx->addit_msg);
    free(pctx);
    free(ctx);
}


#ifdef DNS_PARSER_UT
#include "nkn_mem_counter_def.h"

int nkn_mon_add(const char *name, const char *instance, void *obj,
		uint32_t size)
{   
    return 0;
}

int32_t parse_complete(parsed_dns_msg_t *parsed_data, 
		       void *ctx) {

    int32_t *c = (int32_t *)ctx;

    if (c) {
	printf("Query Number: %d\n", *c);
	free(c);
    }
    parsed_data->print_msgval_hdlr(parsed_data);
    
    return 0;
}

/** 
 * parse a single semantically correct A record query
 * 
 * 
 * @return 
 */
int32_t UT_DP_TEST01(void) {

    FILE *f = NULL;
    size_t rsize = 0, size = 0;
    uint8_t *buf;

    dns_pr_t *dp = createDNS_Handler(2048);
    if (!dp) {
	return -1;
    }
    
    f = fopen("dns_ut_test01", "rb");
    if (!f) {
	return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);

    buf = (uint8_t*)malloc(size);
    rsize = fread(buf, 1, size, f);
    if (rsize != size) {
	return -1;
    }
        
    dp->set_transport_hdlr(dp, 0);
    dp->set_parse_complete_hdlr(dp, parse_complete, NULL);
    dp->parse_hdlr(dp, buf, size);
        
    return 0;
}


/** 
 * parse a multiple semantically correct A record query in
 * one parser instance
 * 
 * 
 * @return 
 */
int32_t UT_DP_TEST02(void) {

    FILE *f = NULL;
    int i = 0, *c = NULL;
    const int num_q = 2;
    size_t rsize = 0, size = 0;
    uint8_t *buf;

    dns_pr_t *dp = createDNS_Handler(2048);
    if (!dp) {
	return -1;
    }
    
    f = fopen("dns_ut_test01", "rb");
    if (!f) {
	return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);

    buf = (uint8_t*)malloc(size);
    rsize = fread(buf, 1, size, f);
    if (rsize != size) {
	return -1;
    }


    for (i = 0; i < num_q; i++) {
	dp->set_transport_hdlr(dp, 0);
	c = (int32_t *)malloc(sizeof(int32_t));
	if (!c) {
	    return -1;
	}
	*c = i;
	dp->set_parse_complete_hdlr(dp, parse_complete, c);
	dp->parse_hdlr(dp, buf, size);
    }
        
    return 0;
}

int32_t main(int argc, char *argv[]) {

    int32_t rv = 0;

    rv = UT_DP_TEST01();
    assert(rv == 0);
    
    rv = UT_DP_TEST02();
    assert(rv == 0);

    return 0;

}
    
#endif

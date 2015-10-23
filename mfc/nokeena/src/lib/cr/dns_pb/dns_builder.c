#include "dns_builder.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "nkn_memalloc.h"

typedef enum {

    HDR,
    CNT_PARM,
    QUERY,
    ANS,
    AUTH,
    ADDIT,
    COMPLETE,
    ERROR
} build_state_t;


typedef struct local_ctx {

    uint8_t* wbuf;
    uint32_t max_wbuf_len;
    uint32_t wbuf_write_pos;
    build_state_t state;
    uint16_t q_cnt;
    uint16_t ans_cnt;
    uint16_t auth_cnt;
    uint16_t addit_cnt;
    uint16_t q_track_cnt;
    uint16_t ans_track_cnt;
    uint16_t auth_track_cnt;
    uint16_t addit_track_cnt;
    char qname_track[MAX_QNAME_CNT][256];
    uint16_t qname_offset[MAX_QNAME_CNT];
} local_ctx_t;


static int32_t setMsgHdr(dns_builder_t* bldr, uint16_t query_id, uint8_t qr,
			 uint8_t op_code, uint8_t aa, uint8_t tc, uint8_t rd, uint8_t ra,
			 uint8_t rcode);

static int32_t setCntParm(dns_builder_t* bldr, uint16_t q_cnt,uint16_t ans_cnt,
			  uint16_t auth_cnt, uint16_t addit_cnt);

static int32_t addQuery(dns_builder_t* bldr, char const* qname, uint16_t type,
			uint16_t cl);

static int32_t addAnswer(dns_builder_t* bldr, char const* qname, uint16_t type, 
			 uint16_t cl, uint32_t ttl, uint16_t rd_len, uint8_t const* rdata);

static int32_t addAuth(dns_builder_t* bldr, char const* qname, uint16_t type, 
		       uint16_t cl, uint32_t ttl, uint16_t rd_len, uint8_t const* rdata);

static int32_t addAddit(dns_builder_t* bldr, char const* qname, uint16_t type, 
			uint16_t cl, uint32_t ttl, uint16_t rd_len, uint8_t const* rdata);

static int32_t getProtoData(dns_builder_t const* bldr, 
			    uint8_t const**proto_data, uint32_t* pd_len);

static void delete(dns_builder_t* bldr);

static int32_t addResponse(dns_builder_t* bldr, char const* qname,
			   uint16_t type, uint16_t cl, uint32_t ttl, uint16_t rd_len, 
			   uint8_t const* rdata);

static int32_t encodeQName(char const* qname, char* enc_qname);

static uint16_t findQnameOffset(dns_builder_t const* bldr, char const* qname);

static int32_t strrstr_offset(char const* str, char const* f_str);

dns_builder_t* createDNS_Builder(uint32_t max_wbuf_len) {

    if (max_wbuf_len == 0)
	return 0;
    dns_builder_t* bldr = calloc(1, sizeof(dns_builder_t));
    if (bldr == NULL)
	return NULL;
    local_ctx_t* lctx = calloc(1, sizeof(local_ctx_t));
    if (lctx == NULL) {
	free(bldr);
	return NULL;
    }
    lctx->wbuf = nkn_malloc_type(max_wbuf_len, 
				 mod_dns_builder_buf);
    if (lctx->wbuf == NULL) {
	free(lctx);
	free(bldr);
    }
    lctx->max_wbuf_len = max_wbuf_len;
    lctx->wbuf_write_pos = 0;
    lctx->state = HDR;
    lctx->q_track_cnt = 0;
    lctx->ans_track_cnt = 0;
    lctx->auth_track_cnt = 0;
    lctx->addit_track_cnt = 0;
    bldr->__internal = (void*)lctx;

    bldr->set_msg_hdr_hdlr = setMsgHdr;
    bldr->set_cnt_parm_hdlr = setCntParm;
    bldr->add_query_hdlr = addQuery;
    bldr->add_ans_hdlr = addAnswer;
    bldr->add_auth_hdlr = addAuth;
    bldr->add_addit_hdlr = addAddit;
    bldr->get_proto_data_hdlr = getProtoData;

    bldr->delete_hdlr = delete;

    return bldr;
}


static void delete(dns_builder_t* bldr) {

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    free(lctx->wbuf);
    free(lctx);
    free(bldr);
}

static int32_t setMsgHdr(dns_builder_t* bldr, uint16_t query_id, uint8_t qr,
	 uint8_t op_code, uint8_t aa, uint8_t tc, uint8_t rd, uint8_t ra,
	 uint8_t rcode) {

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->max_wbuf_len < 4)
	return -1;
    if (lctx->state == COMPLETE)
	lctx->state = HDR;
    if (lctx->state != HDR)
	return -1;
    query_id = htons(query_id);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &query_id, 2);
    lctx->wbuf_write_pos += 2;
    uint8_t b[2];
    b[0] = qr << 7;
    b[0] |= (op_code << 3);
    b[0] |= (aa << 2);
    b[0] |= (tc << 1);
    b[0] |= rd;
    b[1] = 0;
    b[1] |= (ra << 7);
    b[1] |= rcode;
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &b[0], 2);
    lctx->wbuf_write_pos += 2;
    lctx->state = CNT_PARM;
    return 0;
}

static int32_t setCntParm(dns_builder_t* bldr, uint16_t q_cnt,
			  uint16_t ans_cnt, uint16_t auth_cnt, 
			  uint16_t addit_cnt) { 

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + 8))
	return -1;
    if (lctx->state != CNT_PARM)
	return -1;
    if ((q_cnt == 0) || (q_cnt > MAX_QNAME_CNT))
	return -2;
    lctx->q_cnt = q_cnt;
    lctx->ans_cnt = ans_cnt;
    lctx->auth_cnt = auth_cnt;
    lctx->addit_cnt = addit_cnt;
    uint16_t cnt = htons(q_cnt);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &cnt, 2);
    lctx->wbuf_write_pos += 2;
    cnt = htons(ans_cnt);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &cnt, 2);
    lctx->wbuf_write_pos += 2;
    cnt = htons(auth_cnt);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &cnt, 2);
    lctx->wbuf_write_pos += 2;
    cnt = htons(addit_cnt);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &cnt, 2);
    lctx->wbuf_write_pos += 2;
    lctx->state = QUERY;
    return 0;
}


static int32_t addQuery(dns_builder_t* bldr, char const* qname, 
			uint16_t type, uint16_t cl) {

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->state != QUERY)
	return -1;
    if (lctx->q_track_cnt == lctx->q_cnt)
	return -2;
    if (strlen(qname) > 255)
	return -3;
    char en_q[256];
    int32_t rc = encodeQName(qname, &en_q[0]);
    if (rc < 0)
	return -4;

    if (lctx->wbuf_write_pos < 16384) {
	strncpy(&lctx->qname_track[lctx->q_track_cnt][0], qname, 256);
	lctx->qname_offset[lctx->q_track_cnt] = lctx->wbuf_write_pos;
    } else
	lctx->qname_offset[lctx->q_track_cnt] = 0;

    if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + rc))
	return -5;
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &en_q[0], rc);
    lctx->wbuf_write_pos += rc;

    if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + 4)) {
	lctx->state = ERROR;
	return -6;
    }
    type = htons(type);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &type, 2);
    lctx->wbuf_write_pos += 2;
    cl = htons(cl);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &cl, 2);
    lctx->wbuf_write_pos += 2;
    lctx->q_track_cnt++;
    if (lctx->q_track_cnt == lctx->q_cnt) {

	if (lctx->ans_cnt > 0)
	    lctx->state = ANS;
	else if (lctx->auth_cnt > 0)
	    lctx->state = AUTH;
	else if (lctx->addit_cnt > 0)
	    lctx->state = ADDIT;
	else
	    lctx->state = COMPLETE;
    }
    return 0;
}


static int32_t addAnswer(dns_builder_t* bldr, char const* qname, 
	 uint16_t type, uint16_t cl, uint32_t ttl, uint16_t rd_len, 
	 uint8_t const* rdata) {

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->state != ANS)
	return -1;
    if (lctx->ans_track_cnt == lctx->ans_cnt)
	return -2;
    if (strlen(qname) > 255)
	return -3;
    int32_t rc = addResponse(bldr, qname, type, cl, ttl, rd_len, rdata);
    if (rc < 0) {
	lctx->state = ERROR;
	return rc;
    }
    lctx->ans_track_cnt++;
    if (lctx->ans_track_cnt == lctx->ans_cnt) {

	if (lctx->auth_cnt > 0)
	    lctx->state = AUTH;
	else if (lctx->addit_cnt > 0)
	    lctx->state = ADDIT;
	else
	    lctx->state = COMPLETE;
    }
    return 0;
}


static int32_t addAuth(dns_builder_t* bldr, char const* qname, uint16_t type,
       uint16_t cl, uint32_t ttl, uint16_t rd_len, uint8_t const* rdata) {

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->state != AUTH)
	return -1;
    if (lctx->auth_track_cnt == lctx->auth_cnt)
	return -2;
    if (strlen(qname) > 255)
	return -3;
    int32_t rc = addResponse(bldr, qname, type, cl, ttl, rd_len, rdata);
    if (rc < 0) {
	lctx->state = ERROR;
	return rc;
    }
    lctx->auth_track_cnt++;
    if (lctx->auth_track_cnt == lctx->auth_cnt) {

	if (lctx->addit_cnt > 0)
	    lctx->state = ADDIT;
	else
	    lctx->state = COMPLETE;
    }
    return 0;
}

static int32_t addAddit(dns_builder_t* bldr, char const* qname, uint16_t type,
			uint16_t cl, uint32_t ttl, uint16_t rd_len, uint8_t const* rdata) {


    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->state != ADDIT)
	return -1;
    if (lctx->addit_track_cnt == lctx->addit_cnt)
	return -2;
    if (strlen(qname) > 255)
	return -3;
    int32_t rc = addResponse(bldr, qname, type, cl, ttl, rd_len, rdata);
    if (rc < 0) {
	lctx->state = ERROR;
	return rc;
    }
    lctx->addit_track_cnt++;
    if (lctx->addit_track_cnt == lctx->addit_cnt)
	lctx->state = COMPLETE;
    return 0;
}


static int32_t getProtoData(dns_builder_t const* bldr, 
			    uint8_t const**proto_data, uint32_t* pd_len) {


    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    if (lctx->state != COMPLETE)
	return -1;
    *proto_data = lctx->wbuf;
    *pd_len = lctx->wbuf_write_pos;
    return 0;
}


static int32_t addResponse(dns_builder_t* bldr, char const* qname,
			   uint16_t type, uint16_t cl, uint32_t ttl, uint16_t rd_len, 
			   uint8_t const* rdata) {

    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    uint16_t qname_off = findQnameOffset(bldr, qname);
    if (qname_off > 0) {

	qname_off |= (0xC0 << 8);
	if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + 2))
	    return -4;
	qname_off = htons(qname_off);
	memcpy(lctx->wbuf + lctx->wbuf_write_pos, &qname_off, 2);
	lctx->wbuf_write_pos += 2;
    } else {
	char en_q[256];
	int32_t rc = encodeQName(qname, &en_q[0]);
	if (rc < 0)
	    return -5;
	if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + rc))
	    return -6;
	memcpy(lctx->wbuf + lctx->wbuf_write_pos, &en_q[0], rc);
	lctx->wbuf_write_pos += rc;
    }
    if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + 10))
	return -7;
    type = htons(type);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &type, 2);
    lctx->wbuf_write_pos += 2;
    cl = htons(cl);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &cl, 2);
    lctx->wbuf_write_pos += 2;
    ttl = htonl(ttl);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &ttl, 4);
    lctx->wbuf_write_pos += 4;

    char *enc_dom_name = NULL;
    if (type == 512 || type == 1280) {	  /* NS, CNAME RR's
					     in network byte order*/
	enc_dom_name = calloc(1, rd_len + 2);
	encodeQName((char *)rdata, enc_dom_name);
	rdata = (uint8_t*)enc_dom_name;
	rd_len = strlen(enc_dom_name) + 1;
    }

    uint16_t rd_len_nb = htons(rd_len);
    memcpy(lctx->wbuf + lctx->wbuf_write_pos, &rd_len_nb, 2);
    lctx->wbuf_write_pos += 2;

    if (lctx->max_wbuf_len < (lctx->wbuf_write_pos + rd_len))
	return -8;

    memcpy(lctx->wbuf + lctx->wbuf_write_pos, rdata, rd_len);
    lctx->wbuf_write_pos += rd_len;

    if (enc_dom_name) {
	free(enc_dom_name);
    }
    return 0;
}


static int32_t encodeQName(char const* qname, char* enc_qname) {


    uint32_t ref_pos = 0;
    uint32_t ref_cnt = 0;
    uint32_t pos = 0;
    while (qname[pos] != '\0') {

	if (qname[pos] == '.') {
	    ref_cnt++;
	    if (ref_cnt == 1)
		ref_pos = pos;
	    else {
		uint8_t len = pos - ref_pos - 1;
		enc_qname[ref_pos] = len;
		ref_pos = pos;
		ref_cnt = 1;
	    }
	} else {
	    enc_qname[pos] = qname[pos];
	}
	pos++;
    }
    if (ref_cnt == 1) {
	uint8_t len = pos - ref_pos - 1;
	enc_qname[ref_pos] = len;
	enc_qname[pos] = 0;
	return pos + 1;
    } else
	return -1;
}


static uint16_t findQnameOffset(dns_builder_t const* bldr, 
				char const* qname) {


    local_ctx_t* lctx = (local_ctx_t*)bldr->__internal;
    uint32_t i = 0;
    for (; i < lctx->q_cnt; i++) {

	if (lctx->qname_offset[i] > 0) {
	    int32_t rc = strrstr_offset(lctx->qname_track[i], qname);
	    if (rc >= 0)
		return lctx->qname_offset[i] + rc;
	}
    }
    return 0;
}


static int32_t strrstr_offset(char const* str, char const* f_str) {

    uint32_t f_str_len = strlen(f_str);
    uint32_t str_len = strlen(str);
    uint32_t start_offset = 0;
    if (f_str_len > str_len)
	return -1;
    else if (str_len > f_str_len)
	start_offset = str_len - f_str_len - 1;
    if (strcmp(str + start_offset, f_str) == 0)
	return start_offset;
    return -2;
}


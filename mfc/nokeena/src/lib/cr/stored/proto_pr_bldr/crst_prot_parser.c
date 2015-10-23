#include "crst_prot_parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {

    MSG,
    CONTENT,
    COMPLETE,
    ERROR
} pr_state_t;

typedef struct local_pr_ctx {

    pr_state_t state;
    uint8_t* buf;
    uint32_t max_buf_len;
    uint32_t buf_rpos;
    uint32_t buf_wpos;
    crst_prot_method_t method;
    char uri[512];
    uint32_t clen;
    uint8_t const* content;
    uint32_t clen_recvd;
    crst_msg_hdlr_fptr msg_hdlr;
    void* msg_hdlr_ctx;
} local_pr_ctx_t;


static int32_t parse(crst_prot_parser_t* pr, uint8_t const* buf,
	uint32_t buf_len);

static void delete(crst_prot_parser_t* pr);

struct crst_prot_msg;
static struct crst_prot_msg* createCRST_ProtMsg(crst_prot_method_t method, 
	char const* uri, uint32_t clen, uint8_t const* content); 
static crst_prot_method_t mapMethod(uint8_t const* buf, uint32_t len); 


crst_prot_parser_t* createCRST_Prot_Parser(uint32_t max_buf_len, 
	crst_msg_hdlr_fptr msg_hdlr, void* msg_hdlr_ctx) {

    if (max_buf_len == 0)
	return NULL;
    crst_prot_parser_t* pr = calloc(1, sizeof(crst_prot_parser_t));
    if (pr == NULL)
	return NULL;
    local_pr_ctx_t* pr_ctx = calloc(1, sizeof(local_pr_ctx_t));
    if (pr_ctx == NULL) {

	free(pr);
	return NULL;
    }
    pr_ctx->buf = malloc(max_buf_len);
    if (pr_ctx->buf == NULL) {

	free(pr_ctx);
	free(pr);
	return NULL;
    }
    pr_ctx->state = MSG;
    pr_ctx->max_buf_len = max_buf_len;
    pr_ctx->buf_rpos = 0;
    pr_ctx->buf_wpos = 0;
    pr_ctx->msg_hdlr = msg_hdlr;
    pr_ctx->msg_hdlr_ctx = msg_hdlr_ctx;
    pr_ctx->method = UNDEFINED;
    pr_ctx->content = NULL;
    pr_ctx->clen = 0;
    pr_ctx->clen_recvd = 0;

    pr->__internal = (void*)pr_ctx;

    pr->parse_hdlr = parse;
    pr->delete_hdlr = delete;
    return pr;
}


static void delete(crst_prot_parser_t* pr) {

    local_pr_ctx_t* pr_ctx = (local_pr_ctx_t*)pr->__internal;
    free(pr_ctx->buf);
    free(pr_ctx);
    free(pr);
}


static int32_t parse(crst_prot_parser_t* pr, uint8_t const* buf, 
	uint32_t buf_len) {

    local_pr_ctx_t* pr_ctx = (local_pr_ctx_t*)pr->__internal;
    uint32_t avl_buf_len = pr_ctx->max_buf_len -  pr_ctx->buf_wpos;
    if (avl_buf_len < buf_len) {

	pr_ctx->state = ERROR;
	return -1;
    }
    memcpy(pr_ctx->buf + pr_ctx->buf_wpos, buf, buf_len);
    pr_ctx->buf_wpos += buf_len;
    int32_t rc = 0;
    while (1) {

	uint32_t byte_avl = pr_ctx->buf_wpos - pr_ctx->buf_rpos;
	if (pr_ctx->state == MSG) {

	    char end_chr = '\n';
	    uint8_t* end_ptr = memchr(pr_ctx->buf + pr_ctx->buf_rpos, end_chr,
		    byte_avl);
	    if (end_ptr != NULL) {

		uint32_t fl_len = end_ptr - (pr_ctx->buf + pr_ctx->buf_rpos);
		char sp_chr = ' ';
		uint8_t* fs_ptr = memchr(pr_ctx->buf + pr_ctx->buf_rpos, sp_chr,
			fl_len);
		if (fs_ptr != NULL) {

		    uint32_t fw_len = fs_ptr - (pr_ctx->buf + pr_ctx->buf_rpos);
		    pr_ctx->method = mapMethod(pr_ctx->buf + pr_ctx->buf_rpos, 
			    fw_len);
		    uint8_t* ss_ptr = memchr(pr_ctx->buf + pr_ctx->buf_rpos 
			    + fw_len + 1, sp_chr, fl_len - fw_len - 1);
		    uint32_t sw_len = 0;
		    if (ss_ptr == NULL) {
			sw_len = fl_len - fw_len - 1; 
		    } else {
			sw_len = ss_ptr - (pr_ctx->buf + pr_ctx->buf_rpos
				+ fw_len + 1);
		    }
		    if (sw_len >= 512) {

			pr_ctx->state = ERROR;
			rc = -2;
			break;
		    } else {

			memcpy(&pr_ctx->uri[0],
				pr_ctx->buf + pr_ctx->buf_rpos + fw_len +1,
				sw_len);
			pr_ctx->uri[sw_len] = '\0';
		    }
		    if (ss_ptr != NULL) {

			uint32_t tw_len = end_ptr - (ss_ptr + 1);
			char clen_str[10];
			if (tw_len >= 10) {

			    pr_ctx->state = ERROR;
			    rc = -3;
			    break;
			}
			memcpy(&clen_str[0], ss_ptr + 1, tw_len);
			clen_str[tw_len] = '\0';
			char* sl_end_ptr;
			int64_t val = strtol(&clen_str[0], &sl_end_ptr, 0);
			if (sl_end_ptr[0] == '\0') {

			    pr_ctx->clen = val;
			} else {

			    pr_ctx->state = ERROR;
			    rc = -4;
			    break;
			}
		    }
		    pr_ctx->buf_rpos += fl_len + 1;
		    pr_ctx->state = CONTENT;
		    if (pr_ctx->clen > 0)
			pr_ctx->content = pr_ctx->buf + pr_ctx->buf_rpos;
		} else {

		    pr_ctx->state = ERROR;
		    rc = -1;
		    break;
		}
	    } else {
		break;
	    }
	} else if (pr_ctx->state == CONTENT) {

	    uint32_t req_bytes = pr_ctx->clen - pr_ctx->clen_recvd;
	    if (byte_avl >= req_bytes) {

		pr_ctx->buf_rpos += req_bytes; 
		pr_ctx->clen_recvd += req_bytes;
		pr_ctx->state = COMPLETE;
	    } else {

		if (byte_avl == 0)
		    break;
		pr_ctx->buf_rpos += byte_avl; 
		pr_ctx->clen_recvd += byte_avl;
	    }
	} else if (pr_ctx->state == COMPLETE) {

	    crst_prot_msg_t* msg = createCRST_ProtMsg(pr_ctx->method,
		    &pr_ctx->uri[0], pr_ctx->clen, pr_ctx->content);
	    if (msg == NULL) {
		rc = -5;
		break;
	    }
	    pr_ctx->msg_hdlr(msg, pr_ctx->msg_hdlr_ctx);
	    pr_ctx->state = MSG;
	    uint32_t bytes_left = pr_ctx->buf_wpos - pr_ctx->buf_rpos;
	    memcpy(pr_ctx->buf, pr_ctx->buf + pr_ctx->buf_rpos, bytes_left);
	    pr_ctx->buf_rpos = 0;
	    pr_ctx->buf_wpos = bytes_left;
	    pr_ctx->method = UNDEFINED;
	    pr_ctx->clen = 0;
	    pr_ctx->content = NULL;
	    pr_ctx->clen_recvd = 0;
	} else {
	    rc = -6;
	    break;
	}
    }
    return rc;
}



typedef struct local_ctx {

    crst_prot_method_t method;
    char uri[512];
    uint32_t clen;
    uint8_t* content;
} local_ctx_t;


static crst_prot_method_t getMethod(crst_prot_msg_t const* msg);

static void getURI(crst_prot_msg_t const* msg, char const**uri);

static uint32_t getClen(crst_prot_msg_t const* msg);

static void getContent(crst_prot_msg_t const* msg, uint8_t const** buf);

static void print(crst_prot_msg_t const* msg);

static void deleteMsg(crst_prot_msg_t* msg); 

static crst_prot_msg_t* createCRST_ProtMsg(crst_prot_method_t method, 
	char const* uri, uint32_t clen, uint8_t const* content) {

    crst_prot_msg_t* msg = calloc(1, sizeof(crst_prot_msg_t));
    if (msg == NULL)
	return NULL;
    local_ctx_t* ctx = calloc(1, sizeof(local_ctx_t));
    if (ctx == NULL) {

	free(msg);
	return NULL;
    }
    if (clen > 0) {

	ctx->content = malloc(clen);
	if (ctx->content == NULL) {

	    free(ctx);
	    free(msg);
	    return NULL;
	}
	memcpy(ctx->content, content, clen);
    } else
	ctx->content = NULL;
    ctx->method = method;
    strcpy(&ctx->uri[0], uri);
    ctx->clen = clen;

    msg->__internal = ctx;
    msg->get_method_hdlr = getMethod;
    msg->get_uri_hdlr = getURI;
    msg->get_clen_hdlr = getClen;
    msg->get_content_hdlr = getContent;
    msg->print_hdlr = print;

    msg->delete_hdlr = deleteMsg;
    return msg;
}


static void deleteMsg(crst_prot_msg_t* msg) {

    local_ctx_t* ctx = (local_ctx_t*)msg->__internal;
    if (ctx->content != NULL)
	free(ctx->content);
    free(ctx);
    free(msg);
}


static crst_prot_method_t getMethod(crst_prot_msg_t const* msg) {

    local_ctx_t* ctx = (local_ctx_t*)msg->__internal;
    return ctx->method;
}


static void getURI(crst_prot_msg_t const* msg, char const**uri) {

    local_ctx_t* ctx = (local_ctx_t*)msg->__internal;
    *uri = &ctx->uri[0];
}


static uint32_t getClen(crst_prot_msg_t const* msg) {

    local_ctx_t* ctx = (local_ctx_t*)msg->__internal;
    return ctx->clen;
}


static void getContent(crst_prot_msg_t const* msg, uint8_t const** buf) {

    local_ctx_t* ctx = (local_ctx_t*)msg->__internal;
    *buf = ctx->content;
}


static void print(crst_prot_msg_t const* msg) {

    printf("Method  : %u\n", msg->get_method_hdlr(msg));
    char const* uri;
    msg->get_uri_hdlr(msg, &uri);
    printf("URI     : %s\n", uri);
    printf("CLEN    : %u\n\n", msg->get_clen_hdlr(msg));
    uint8_t const* buf;
    msg->get_content_hdlr(msg, &buf);
    uint32_t j = 0;
    for (; j < msg->get_clen_hdlr(msg); j++) {
	printf("%02X ", buf[j]);
	if ((j > 0) && (j % 16 == 0))
	    printf("\n");
    }
    printf("\n\n");
}


static crst_prot_method_t mapMethod(uint8_t const* buf, uint32_t len) {

    crst_prot_method_t method = UNDEFINED;
    if (strncmp((char*)buf, "CREATE", len) == 0)
	method = CREATE;
    else if (strncmp((char*)buf, "ADD", len) == 0)
	method = ADD;
    else if (strncmp((char*)buf, "REMOVE", len) == 0)
	method = REMOVE;
    else if (strncmp((char*)buf, "UPDATE", len) == 0)
	method = UPDATE;
    else if (strncmp((char*)buf, "GET", len) == 0)
	method = GET;
    else if (strncmp((char*)buf, "DELETE", len) == 0)
	method = DELETE;
    else if (strncmp((char*)buf, "SEARCH", len) == 0)
	method = SEARCH;
    return method;
}


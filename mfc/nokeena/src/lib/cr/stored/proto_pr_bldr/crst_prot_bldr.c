#include "crst_prot_bldr.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
    BUILD,
    FW_SET,
    DESC_SET,
    COMPLETE,
    ERROR
}bldr_state_t;


typedef struct local_ctx {

    bldr_state_t state;
    uint8_t* buf;
    uint32_t buf_wpos;
    uint32_t buf_max_len;
    uint32_t buf_bytes_read;
} local_ctx_t;


static int32_t retrieveProtData(crst_prot_bldr_t* bldr, 
	uint8_t* prot_data, int32_t prot_data_len);

static int32_t setCode(crst_prot_bldr_t* bldr, uint32_t resp_code);

static int32_t setMessageName(crst_prot_bldr_t* bldr, char const* name); 

static int32_t setDesc(crst_prot_bldr_t* bldr, char const* desc);

static int32_t setClen(crst_prot_bldr_t* bldr, uint32_t clen);

static int32_t addContent(crst_prot_bldr_t* bldr, uint8_t const* cont,
	uint32_t cont_len);

static int32_t resizeBufferIfNeeded(local_ctx_t* ctx, uint32_t cont_len); 

static void delete(crst_prot_bldr_t* bldr);


crst_prot_bldr_t* createCRST_ProtocolBuilder(uint32_t approx_buf_len) {

    if (approx_buf_len == 0)
	return NULL;
    crst_prot_bldr_t* bldr = calloc(1, sizeof(crst_prot_bldr_t));
    if (bldr == NULL)
	return NULL;
    local_ctx_t* ctx = calloc(1, sizeof(local_ctx_t));
    if (ctx == NULL) {

	free(bldr);
	return NULL;
    }
    ctx->buf = malloc(approx_buf_len);
    if (ctx->buf == NULL) {

	free(ctx);
	free(bldr);
	return NULL;
    }
    ctx->state = BUILD;
    ctx->buf_wpos = 0;
    ctx->buf_max_len = approx_buf_len;
    ctx->buf_bytes_read = 0;
    bldr->__internal = (void*)ctx;

    bldr->get_prot_data_hdlr = retrieveProtData; 
    bldr->set_resp_code_hdlr = setCode;
    bldr->set_msg_name_hdlr = setMessageName;
    bldr->set_desc_hdlr = setDesc;
    bldr->add_content_hdlr = addContent;
    bldr->delete_hdlr = delete;
    return bldr;
}


static void delete(crst_prot_bldr_t* bldr) {

    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    free(ctx->buf);
    free(ctx);
    free(bldr);
}


static int32_t retrieveProtData(crst_prot_bldr_t* bldr, 
	uint8_t* prot_data, int32_t prot_data_len) {

    if (prot_data_len <= 0)
	return -1;
    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    if (ctx->state == DESC_SET) {

	char line_end = '\n';
	if (resizeBufferIfNeeded(ctx, 1) < 0) {
	    ctx->state = ERROR;
	    return -2;
	}
	memcpy(ctx->buf + ctx->buf_wpos, &line_end, 1);
	ctx->buf_wpos += 1;
	ctx->state = COMPLETE;
    }
    if (ctx->state != COMPLETE)
	return -3;
    if ((uint32_t)prot_data_len > (ctx->buf_wpos - ctx->buf_bytes_read))
	prot_data_len = (ctx->buf_wpos - ctx->buf_bytes_read);
    memcpy(prot_data, ctx->buf + ctx->buf_bytes_read, prot_data_len);
    ctx->buf_bytes_read += prot_data_len;
    if (ctx->buf_bytes_read == ctx->buf_wpos) {

	ctx->state = BUILD;
	ctx->buf_wpos = 0;
	ctx->buf_bytes_read = 0;
    }
    return prot_data_len;
}


static int32_t setCode(crst_prot_bldr_t* bldr, uint32_t resp_code) {

    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    if (ctx->state != BUILD)
	return -1;
    char rc[10];
    snprintf(&rc[0], 10, "%u ", resp_code);
    uint32_t str_len = strlen(&rc[0]);
    if (resizeBufferIfNeeded(ctx, str_len) < 0) {
	ctx->state = ERROR;
	return -2;
    }
    memcpy(ctx->buf, &rc[0], str_len);
    ctx->buf_wpos += str_len;
    ctx->state = FW_SET;
    return 0;
}


static int32_t setMessageName(crst_prot_bldr_t* bldr, char const* name) {

    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    if (ctx->state != BUILD)
	return -1;
    uint32_t str_len = strlen(name);
    if (resizeBufferIfNeeded(ctx, str_len) < 0) {
	ctx->state = ERROR;
	return -2;
    }
    memcpy(ctx->buf, name, str_len);
    ctx->buf_wpos += str_len;
    ctx->state = FW_SET;
    return 0;
}


static int32_t setDesc(crst_prot_bldr_t* bldr, char const* desc) {

    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    if (ctx->state != FW_SET)
	return -1;
    uint32_t str_len = strlen(desc);
    if (resizeBufferIfNeeded(ctx, str_len) < 0) {
	ctx->state = ERROR;
	return -2;
    }
    memcpy(ctx->buf + ctx->buf_wpos, desc, str_len);
    ctx->buf_wpos += str_len;
    ctx->state = DESC_SET;
    return 0;
}


static int32_t setClen(crst_prot_bldr_t* bldr, uint32_t clen) {

    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    if (ctx->state != DESC_SET)
	return -1;
    char len_str[10];
    snprintf(&len_str[0], 10, " %u\n", clen);
    uint32_t str_len = strlen(&len_str[0]);
    if (resizeBufferIfNeeded(ctx, str_len) < 0) {
	ctx->state = ERROR;
	return -2;
    }
    memcpy(ctx->buf + ctx->buf_wpos, &len_str[0], str_len);
    ctx->buf_wpos += str_len;
    return 0;
}


static int32_t addContent(crst_prot_bldr_t* bldr, uint8_t const* cont,
	uint32_t cont_len) {

    local_ctx_t* ctx = (local_ctx_t*)bldr->__internal;
    if (ctx->state != DESC_SET)
	return -1;
    if (setClen(bldr, cont_len) < 0) {
	ctx->state = ERROR;
	return -2;
    }
    if (resizeBufferIfNeeded(ctx, cont_len) < 0) {
	ctx->state = ERROR;
	return -3;
    }
    memcpy(ctx->buf + ctx->buf_wpos, cont, cont_len);
    ctx->buf_wpos += cont_len;
    ctx->state = COMPLETE;
    return 0;
}


static int32_t resizeBufferIfNeeded(local_ctx_t* ctx, uint32_t cont_len) {

    uint32_t bytes_avl = ctx->buf_max_len - ctx->buf_wpos;
    if (cont_len <= bytes_avl)
	return 0;
    uint32_t new_len = ctx->buf_max_len + (cont_len - bytes_avl);
    uint8_t* tmp = realloc(ctx->buf, new_len);
    if (tmp != NULL) {
	ctx->buf = tmp;
	ctx->buf_max_len = new_len;
    } else {
	return -1;
    }
    return 0;
}


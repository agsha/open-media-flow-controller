#include "crst_con_ctx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CRST_PROT_REQ_LEN 1024

#define APPROX_CRST_PROT_RES_LEN 4096

static void delete(crst_con_ctx_t* con_ctx);

crst_con_ctx_t* createStoreConnectionContext(crst_msg_hdlr_fptr
	msg_hdlr, void* msg_hdlr_ctx, 
	crst_con_void_del_fptr msg_ctx_delete_hdlr ) {

    if (msg_hdlr == NULL)
	return NULL;
    crst_con_ctx_t* con_ctx = calloc(1, sizeof(crst_con_ctx_t));
    if (con_ctx == NULL)
	return NULL;
    pthread_mutex_init(&con_ctx->lock, NULL);
    con_ctx->pr = createCRST_Prot_Parser(MAX_CRST_PROT_REQ_LEN, msg_hdlr,
	    msg_hdlr_ctx);
    if (con_ctx->pr == NULL) {

	free(con_ctx);
	return NULL;
    }
    con_ctx->bldr = createCRST_ProtocolBuilder(APPROX_CRST_PROT_RES_LEN);
    if (con_ctx->bldr == NULL) {

	con_ctx->pr->delete_hdlr(con_ctx->pr);
	free(con_ctx);
	return NULL;
    }
    con_ctx->msg_hdlr = msg_hdlr;
    con_ctx->msg_hdlr_ctx = msg_hdlr_ctx;
    con_ctx->msg_ctx_delete_hdlr = msg_ctx_delete_hdlr;
    con_ctx->delete_hdlr = delete;
    return con_ctx;
}


static void delete(crst_con_ctx_t* con_ctx) {

    if ((con_ctx->msg_hdlr_ctx != NULL) 
	    && (con_ctx->msg_ctx_delete_hdlr != NULL))
	con_ctx->msg_ctx_delete_hdlr(con_ctx->msg_hdlr_ctx);
    con_ctx->pr->delete_hdlr(con_ctx->pr);
    con_ctx->bldr->delete_hdlr(con_ctx->bldr);
    pthread_mutex_destroy(&con_ctx->lock);
    free(con_ctx);
}


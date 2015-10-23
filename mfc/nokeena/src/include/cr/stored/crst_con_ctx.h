#ifndef CRST_CON_CTX_H
#define CRST_CON_CTX_H

#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>

#include "crst_prot_parser.h"
#include "crst_prot_bldr.h"

struct crst_con_ctx;

typedef void (*delete_crst_con_ctx_fptr)(struct crst_con_ctx*);

typedef void (*crst_con_void_del_fptr)(void* ctx);

typedef struct crst_con_ctx {

    pthread_mutex_t lock;
    crst_prot_parser_t* pr;
    crst_prot_bldr_t* bldr;
    struct timeval last_activity;
    crst_msg_hdlr_fptr msg_hdlr;
    void* msg_hdlr_ctx;
    crst_con_void_del_fptr msg_ctx_delete_hdlr;
    delete_crst_con_ctx_fptr delete_hdlr;
} crst_con_ctx_t;


crst_con_ctx_t* createStoreConnectionContext(crst_msg_hdlr_fptr msg_hdlr,
	void* ctx, crst_con_void_del_fptr msg_ctx_delete_hdlr);


#endif


#ifndef CRST_NW_HANDLER_H
#define CRST_NW_HANDLER_H

#include <stdint.h>
#include "event_dispatcher.h"
#include "entity_data.h"

#include "nkn_ref_count_mem.h"

int8_t crst_listen_handler(entity_data_t* ctx, void* caller_id,
		delete_caller_ctx_fptr caller_id_delete_hdlr);

int8_t crst_epollin_handler(entity_data_t* ctx, void* caller_id,
		delete_caller_ctx_fptr caller_id_delete_hdlr); 

int8_t crst_epollout_handler(entity_data_t* ctx, void* caller_id,
		delete_caller_ctx_fptr caller_id_delete_hdlr);

int8_t crst_epollerr_handler(entity_data_t* ctx, void* caller_id,
		delete_caller_ctx_fptr caller_id_delete_hdlr); 

int8_t crst_epollhup_handler(entity_data_t* ctx, void* caller_id,
		delete_caller_ctx_fptr caller_id_delete_hdlr); 

int8_t crst_timeout_handler(entity_data_t* ctx, void* caller_id,
		delete_caller_ctx_fptr caller_id_delete_hdlr); 


typedef struct msg_hdlr_ctx {

    event_disp_t* disp;
    entity_data_t* entity_ctx;
} msg_hdlr_ctx_t;

#endif

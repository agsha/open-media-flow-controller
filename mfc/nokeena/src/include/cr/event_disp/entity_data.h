#ifndef ENTITY_DATA_H
#define ENTITY_DATA_H

#include <stdlib.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

    struct entity_data;

    typedef void (*delete_caller_ctx_fptr)(void* caller_ctx);

    typedef int8_t (*handle_event_fptr)(struct entity_data*,
	    void* caller_ctx, delete_caller_ctx_fptr delete_caller_ctx_hdlr);

    typedef void (*delete_context_fptr)(void*);

    typedef void (*delete_fptr)(struct entity_data*);

    typedef struct entity_data {

	int32_t fd;
	int32_t event_flags;
	struct timeval scheduled;
	struct timeval to_be_scheduled;
	struct timeval event_time;
	void* context;

	handle_event_fptr read_hdlr;
	handle_event_fptr write_hdlr;
	handle_event_fptr error_hdlr;
	handle_event_fptr hup_hdlr;
	handle_event_fptr timeout_hdlr;
	delete_context_fptr delete_context_hdlr;
	delete_fptr delete_hdlr;
    }entity_data_t;


    entity_data_t* newEntityData(int32_t fd, void* context,
	    delete_context_fptr delete_context_hdlr,
	    handle_event_fptr read_hdlr, handle_event_fptr write_hdlr,
	    handle_event_fptr error_hdlr, handle_event_fptr hup_hdlr,
	    handle_event_fptr timeout_hdlr);

#ifdef __cplusplus
}
#endif

#endif


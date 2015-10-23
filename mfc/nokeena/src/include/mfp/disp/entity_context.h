#ifndef ENTITY_CONTEXT_HH
#define ENTITY_CONTEXT_HH

#include <stdlib.h>
#include <sys/time.h>
#include "displib_conf.h"

#ifdef __cplusplus
extern "C" {
#endif


struct dispatcher_mngr;

typedef struct entity_context entity_context_t;

// Defining the entity_context struct used for register/unregister

typedef int8_t (*handleEvent)(entity_context_t*);
typedef void (*freeContextData)(void*);
typedef void (*deleteEntity)(entity_context_t*);

/* Implementation for the handlers and the contextFree will be
depend on the application requirements. Every application can
define their own handlers and assign the handler ptrs accordingly */

struct entity_context {

        int32_t fd;
        int32_t event_flags;//events to reg. when calling register at disp
        struct dispatcher_mngr* disp_mngr;
        pthread_mutex_t context_state;
        struct timeval scheduled;
        struct timeval to_be_scheduled;
	struct timeval event_time;

        // Application specific data
        void* context_data;

        // Application specific Handlers
        handleEvent event_read;
        handleEvent event_write;
        handleEvent event_error;
        handleEvent event_hup;
        handleEvent event_timeout;
        freeContextData free_data_handler;
	deleteEntity delete_entity;

};


entity_context_t* newEntityContext(int32_t fd,
        void* context_data, freeContextData free_data_handler,
        handleEvent read_handler, handleEvent write_handler,
        handleEvent error_handler, handleEvent hup_handler,
        handleEvent timeout_handler, struct dispatcher_mngr* disp);


#ifdef __cplusplus
}
#endif

#endif


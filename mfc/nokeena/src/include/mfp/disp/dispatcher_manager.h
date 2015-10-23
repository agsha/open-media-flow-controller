#ifndef DISPATCHER_MANAGER_HH
#define DISPATCHER_MANAGER_HH

/*
 Purpose: A common interface to register/unregister events to a set(n) of 
	dispatchers. (where 'n' is configurable by user)
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "dispatcher.h"
#include "displib_conf.h"

#define MAX_DISP_WAIT 3 // In milliseconds

struct dispatcher_mngr;

typedef void* (*startIntf)(void*);
typedef void (*endIntf)(struct dispatcher_mngr*);
typedef int8_t (*dispManagerIntf)(entity_context_t*);

typedef struct dispatcher_mngr {

	int32_t num_disps;
	int32_t max_fds;
	int32_t next_disp_to_use;
	dispatcher_t** disps;

	int32_t* fd_flag;
	fd_lock_t* fd_lk;

	pthread_mutex_t state;
	pthread_t *disp_thrds;
	entity_context_t** mngr_cnxt; /* To register timeout handlers 
	   with all disps. So that they never wait indefinitly for events.
	   If allowed to wait indefinitely: disp locks will also be held 
	   indefinitely (which SHOULD NOT be allowed)*/

	dispManagerIntf register_entity;
	dispManagerIntf self_unregister;
	dispManagerIntf unregister_entity;
		//To unreg an entity outside its handler
	
	startIntf start;
	endIntf end;

        // Interface for add/del events/timeout
        dispManagerIntf self_set_read;
        dispManagerIntf self_unset_read;
        dispManagerIntf self_set_write;
        dispManagerIntf self_unset_write;
        dispManagerIntf self_set_error;
        dispManagerIntf self_unset_error;
        dispManagerIntf self_set_hup;
        dispManagerIntf self_unset_hup;

        dispManagerIntf self_del_all_event;

        dispManagerIntf self_schedule_timeout;

}dispatcher_mngr_t;

void initDispLib(disp_malloc_fptr malloc_hdlr, disp_calloc_fptr calloc_hdlr,
		        disp_thread_conf_fptr thread_conf,
				disp_log_hdlr_fptr log_hdlr, uint64_t log_id);

dispatcher_mngr_t* newDispManager(int32_t num_disps, int32_t max_fds);


#ifdef __cplusplus
}
#endif


#endif


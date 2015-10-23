#ifndef DISPATCHER_HH
#define DISPATCHER_HH

/* Purpose: To monitor a list of entities for registered events */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>

#include <glib.h>

#include "entity_context.h"
#include "fd_state.h"
#include "displib_conf.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct dispatcher dispatcher_t;

typedef int8_t (*dispatcherIntf)(dispatcher_t* disp, entity_context_t*);
typedef void* (*start)(void*); 
typedef void (*end_disp)(dispatcher_t*); 


struct dispatcher {
	int32_t poll_fds;
	fd_lock_t* fd_lk;	
	uint32_t disp_id;

	int32_t* fd_flag;//Array used for lookup if fd is alive
	entity_context_t** con_ptr;
	pthread_mutex_t disp_state;	
	
	//Why Tree? we need the ctx to be sorted on the timeouts scheduled by them
	GTree* registered_timeouts;
	struct timeval scheduled_timeout;
	int32_t epoll_timeout; //In ms: that will be fed to epoll_wait
	int8_t stop_flag;

	start start_dispatcher;
	end_disp end_dispatcher;
	end_disp delete_dispatcher;

    dispatcherIntf register_entity;
    dispatcherIntf unregister_entity;

	// Interface for add/del events/timeout
	dispatcherIntf set_read_event;
	dispatcherIntf unset_read_event;
	dispatcherIntf set_write_event;
	dispatcherIntf unset_write_event;
	dispatcherIntf set_error_event;
	dispatcherIntf unset_error_event;
	dispatcherIntf set_hup_event;
	dispatcherIntf unset_hup_event;
	dispatcherIntf del_all_event;

	dispatcherIntf sched_timeout_event;
};


dispatcher_t* newDispatcher(uint32_t id, fd_lock_t* fd_lk);


#ifdef __cplusplus
}
#endif

#endif

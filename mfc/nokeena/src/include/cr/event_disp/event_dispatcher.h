#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

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
#include <sys/prctl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>

#include <glib.h>

#include "entity_data.h"

#ifdef __cplusplus
extern "C" {
#endif

    struct event_dispatcher; 

    typedef int32_t (*event_disp_intf_fptr)(struct event_dispatcher* disp,
	    entity_data_t* ent_data);
    typedef void* (*start_event_disp_fptr)(void*); 
    typedef void (*end_event_disp_fptr)(struct event_dispatcher*); 


    typedef struct event_dispatcher {

	void* __internal;

	start_event_disp_fptr start_hdlr;
	end_event_disp_fptr end_hdlr;
	end_event_disp_fptr delete_hdlr;

	event_disp_intf_fptr reg_entity_hdlr;
	event_disp_intf_fptr unreg_entity_hdlr;

	event_disp_intf_fptr set_read_hdlr;
	event_disp_intf_fptr unset_read_hdlr;
	event_disp_intf_fptr set_write_hdlr;
	event_disp_intf_fptr unset_write_hdlr;
	event_disp_intf_fptr set_error_hdlr;
	event_disp_intf_fptr unset_error_hdlr;
	event_disp_intf_fptr set_hup_hdlr;
	event_disp_intf_fptr unset_hup_hdlr;

	event_disp_intf_fptr sched_timeout_hdlr;
    }event_disp_t;


    event_disp_t* newEventDispatcher(uint32_t id, uint32_t max_descs, 
	    uint32_t tmout_feature);


#ifdef __cplusplus
}
#endif

#endif

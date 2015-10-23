#include <stdio.h>

#include "event_dispatcher.h"
#include "entity_data.h"


#define START_PORT 5555
#define END_PORT 5564

#define MAX_FDS 20000

pthread_t disp_thrd;
event_disp_t* disp; 

static void exitClean(int signal_num);
static void exitClean(int signal_num) {
    printf("Caught CTRL-C\n");
    exit(0);
}


static int8_t writeHandler(entity_data_t* ent_ctx, void* caller_ctx,
	delete_caller_ctx_fptr delete_caller_ctx_hdlr);

static int8_t timeoutHandler(entity_data_t* ent_ctx, void* caller_ctx,
	delete_caller_ctx_fptr delete_caller_ctx_hdlr); 

static void* unregEntity(void* ctx);

int main(int argc, char *argv[]) {

    int32_t num_timeouts = END_PORT - START_PORT + 1;
    event_disp_t* disp = newEventDispatcher(0, MAX_FDS, 1);
    pthread_create(&disp_thrd, NULL, disp->start_hdlr, disp);

    struct sigaction action_cleanup;
    memset(&action_cleanup, 0, sizeof(struct sigaction));
    action_cleanup.sa_handler = exitClean; 
    action_cleanup.sa_flags = 0;
    sigaction(SIGINT, &action_cleanup, NULL);
    sigaction(SIGTERM, &action_cleanup, NULL);

    uint32_t i = 0;
    for(; i< num_timeouts; i++) {

	// creating sockets
	int32_t fd = -1;	
	struct sockaddr_in addr;			
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
	    perror("socket() failed: ");
	    exit(-1);
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(START_PORT + i);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr*)&addr,
		    sizeof(struct sockaddr)) < 0) {
	    perror("bind() failed: ");
	    close(fd);
	    exit(-1);
	}

	// register for timeout event and attach appropriate hndlr
	handle_event_fptr timeout_handler = timeoutHandler;

	entity_data_t* entity_ctx = newEntityData(fd, disp, NULL, 
		NULL, writeHandler, NULL, NULL, timeout_handler);

	uint32_t tmout_in_secs;
	// /* TEST 1:*/ tmout_in_secs = 0;
	// /* TEST 2:*/ tmout_in_secs = (i*5); 
	/* TEST 3:*/ tmout_in_secs = (num_timeouts*5) - (i*5); 
	/* // TEST 4:
	   if (i %2 == 0)
	   tmout_in_secs = (i*8) - (i*5);
	   else
	   tmout_in_secs = (i*8) + (i*5);
	 */
	gettimeofday(&(entity_ctx->to_be_scheduled), NULL);
	entity_ctx->to_be_scheduled.tv_sec += (tmout_in_secs);
	entity_ctx->event_flags |= EPOLLIN;
	disp->reg_entity_hdlr(disp, entity_ctx);

	printf("Registered fd : %d Timeout : %d\n", fd, tmout_in_secs);
    }

    pthread_join(disp_thrd, NULL);
    return 0;
}


static int8_t timeoutHandler(entity_data_t* ent_ctx, void* caller_ctx,
	delete_caller_ctx_fptr delete_caller_ctx_hdlr) {

    printf("Timeed out fd: %d\n", ent_ctx->fd);
    event_disp_t *disp = (event_disp_t*)caller_ctx;
    if (ent_ctx->fd % 2 == 0) {

	gettimeofday(&(ent_ctx->to_be_scheduled), NULL);
	ent_ctx->to_be_scheduled.tv_sec +=  2;
	disp->sched_timeout_hdlr(disp, ent_ctx);
	pthread_t unreg_thrd;
	pthread_create(&unreg_thrd, NULL, unregEntity, ent_ctx);
    } else {

	ent_ctx->event_flags |= EPOLLOUT;
	disp->set_write_hdlr(disp, ent_ctx);
    }
    return 1;
}


static int8_t writeHandler(entity_data_t* ent_ctx, void* caller_ctx,
	delete_caller_ctx_fptr delete_caller_ctx_hdlr) {

    printf("Unregistering fd : %d\n", ent_ctx->fd);
    event_disp_t *disp = (event_disp_t*)caller_ctx;
    disp->unreg_entity_hdlr(disp, ent_ctx);
    return 1;
}


static void* unregEntity(void* ctx) {

    entity_data_t* ent_ctx = (entity_data_t*)ctx;
    event_disp_t* disp = ent_ctx->context;
    printf("Unregistering fd (different thread): %d\n", ent_ctx->fd);
    disp->unreg_entity_hdlr(disp, ent_ctx);
    return NULL;
}


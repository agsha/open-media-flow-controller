#include <stdio.h>

#include "dispatcher_manager.h"
#include "entity_context.h"


#define START_PORT 5555
#define END_PORT 5564

#define NUM_DISPS 1
#define MAX_FDS 20000

pthread_t disp_thrd;
dispatcher_mngr_t* disp_mngr; 

static void exitClean(int signal_num);
static void exitClean(int signal_num) {
	printf(" Caught CTRL-C\n");
	disp_mngr->end(disp_mngr);
}
static int8_t timeoutHandler(entity_context_t* ent_ctx);

int main(int argc, char *argv[]) {

	int32_t num_timeouts = END_PORT - START_PORT + 1;
	disp_mngr = newDispManager(NUM_DISPS, MAX_FDS);
	int32_t i =0;
	int32_t* fd_array = calloc(num_timeouts, sizeof(int32_t));

	// start dispatcher
	for(; i < NUM_DISPS; i++)
		pthread_create(&disp_thrd, NULL, disp_mngr->start, 
				disp_mngr);

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = exitClean; 
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);

	for(i = 0; i< num_timeouts; i++) {

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
		fd_array[i] = fd;
		handleEvent timeout_handler = timeoutHandler;

		entity_context_t* entity_ctx = newEntityContext(fd,
				fd_array + i, NULL, NULL, NULL, NULL, NULL,
				timeout_handler, disp_mngr);

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
		disp_mngr->register_entity(entity_ctx);

		printf("Registered fd : %d Timeout : %d\n",
				fd_array[i],tmout_in_secs);
	}

	pthread_join(disp_thrd, NULL);
	free(fd_array);
	return 0;
}


static int8_t timeoutHandler(entity_context_t* ent_ctx) {

	int32_t* ctx = (int32_t*)ent_ctx->context_data;
	printf("Timeed out fd: %d\n", *ctx);
	return 1;
}


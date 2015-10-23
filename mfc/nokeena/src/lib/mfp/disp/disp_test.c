#include "dispatcher_manager.h"


dispatcher_mngr_t* disp_mngr = NULL;
int ctr = 0;
entity_context_t *ent_context1, *ent_context2;
pthread_t disp_mngr_thrd;

int8_t printStr(entity_context_t* ent_context);
int8_t timeout(entity_context_t* ent_context);


void exitClean(int signal_num) {

	disp_mngr->end(disp_mngr);
}

void* disp_malloc_custom(int32_t size) { return malloc(size); }
void* disp_calloc_custom(int32_t num, int32_t size) { return calloc(num, size); }


int main() {

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = exitClean; 
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);

	int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr1;
	addr1.sin_family = AF_INET;
	addr1.sin_addr.s_addr = INADDR_ANY;
	addr1.sin_port = htons(1234);
	if (bind(fd1, (struct sockaddr *) &addr1, sizeof(addr1)) == -1) {
		perror("bind");
		exit(1);
	}

        int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr2;
        addr2.sin_family = AF_INET;
        addr2.sin_addr.s_addr = INADDR_ANY;
        addr2.sin_port = htons(1235);
        if (bind(fd2, (struct sockaddr *) &addr2, sizeof(addr2)) == -1) {
                perror("bind");
                exit(1);
        }

	disp_mngr = newDispManager(1, 100);

	handleEvent print_str = printStr;
	handleEvent timeout_check = timeout;
	ent_context1 = newEntityContext(fd1, NULL, NULL, 
			print_str, NULL,
			NULL, NULL, timeout_check, disp_mngr);


	ent_context2 = newEntityContext(fd2, NULL, NULL, 
			print_str, NULL,
			NULL, NULL, timeout_check, disp_mngr);

	ent_context1->init_event_flags |= EPOLLIN;
	ent_context2->init_event_flags |= EPOLLIN;

	printf("ent_context->fd1: %d\n", ent_context1->fd);
	printf("ent_context->fd2: %d\n", ent_context2->fd);

	disp_mngr->register_entity(ent_context1);
	disp_mngr->register_entity(ent_context2);

	gettimeofday(&(ent_context1->to_be_scheduled), NULL);
	gettimeofday(&(ent_context2->to_be_scheduled), NULL);
	ent_context1->to_be_scheduled.tv_sec += 30;
	ent_context2->to_be_scheduled.tv_sec += 30;
	disp_mngr->self_schedule_timeout(ent_context1);
	disp_mngr->self_schedule_timeout(ent_context2);

	pthread_create(&(disp_mngr_thrd), NULL,
                                disp_mngr->start,
                                disp_mngr);
	pthread_join(disp_mngr_thrd, NULL);
	return 0;
}


int8_t printStr(entity_context_t* ent_context) {

	char buf[5000];
	struct sockaddr addr;
	socklen_t len = 0;
	memset(&buf[0], 0, 5000);
	memset(&addr, 0, sizeof(addr));
	int32_t ret;
	ret = recvfrom(ent_context->fd, &buf[0], 5000, 0, &addr, &len);
	if (ret >=0)
		printf("Bytes Received: %d\n", ret);
	ctr++;/*
	if (ctr == 5) {
		ent_context->disp_mngr->end(ent_context->disp_mngr);
		//ent_context1->disp_mngr->self_unregister(ent_context1);
		//ent_context2->disp_mngr->unregister_entity(ent_context2);
	}
	*/
	return 1;
}


int8_t timeout(entity_context_t* ent_context) {

	//printf("timeout\n");
	gettimeofday(&(ent_context->to_be_scheduled), NULL);
	ent_context->to_be_scheduled.tv_sec += 30;
	ent_context->disp_mngr->self_schedule_timeout(ent_context);
	return 1;
}


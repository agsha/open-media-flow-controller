#include "cont_pool.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>


static void* get(void* arg); 
static void* put(void* arg); 

cont_pool_t* cp = NULL;

int32_t main() {


    cp = createContainerPool(18,1);
    if (cp == NULL) {

	printf("cp create failed.\n");
	return -1;
    }
    pthread_t pt;
    pthread_t gt;
    pthread_create(&pt, NULL, put, NULL);
    pthread_create(&gt, NULL, get, NULL);
    pthread_join(pt, NULL);
    pthread_join(gt, NULL);
    cp->delete_hdlr(cp);

    return 0;
}

static void* put(void* arg) {

    uint32_t i = 0;
    for (; i < 18; i ++) {

	uint32_t* cont = malloc(sizeof(uint32_t));
	*cont = i;
	cp->put_cont_hdlr(cp, cont);
	printf("Put value: %u\n", *cont);
    }
    return NULL;
}

static void* get(void* arg) {

    uint32_t i = 0;
    for (; i < 18; i ++) {

	uint32_t* cont = cp->get_cont_hdlr(cp);
	printf("Get value: %u\n", *cont);
	free(cont);
    }
    return NULL;
}


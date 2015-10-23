#ifndef _NKN_EM_API_H
#define _NKN_EM_API_H


#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>

#include "nkn_sched_api.h"
#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

#define UNUSED_ARGUMENT(x) 	(void)x
#define MAX_ENC_THREADS 	8
#define MAX_ENC_TASK_ARR 	256001
#define ENCODING_MGR_MAGIC 	0xbabababa
#define ENCODING_MGR_MAGIC_DEAD 0xdeadbaba

typedef enum 
{
	AES_128_CBC,
	AES_256_CBC
}ENCTYPE;

typedef struct enc_msg
{
    int32_t magic;
    uint8_t *in_enc_data;			//caller allocates memory
    uint8_t *out_enc_data;			//caller allocates memory
    uint8_t iv[256]; 
    uint8_t key[256]; 
    uint8_t key_uri[512];
    ENCTYPE enctype;
    int32_t in_len;				// size of input
    int32_t out_len;				// size of output
    int32_t key_len;				//key length in bytes	
    int32_t iv_len;				//iv length in bytes	
    int32_t errcode;				// Error code
} enc_msg_t;

void enc_mgr_input(nkn_task_id_t id);		//puts into taskqueue
void enc_mgr_output(nkn_task_id_t id);		//dummy
void enc_mgr_cleanup(nkn_task_id_t id);		//dummy
void *enc_input_handler_thread(void *arg);	//picks tasks from queue
void enc_mgr_thread_input(nkn_task_id_t id);	//does actual processing 
void enc_mgr_init(void);			//initializes the worker threads
int enc_mgr_engine(enc_msg_t*);

#endif /*_NKN_EM_API_H*/

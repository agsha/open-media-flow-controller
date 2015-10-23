/*********************************************************************
This module does encryption of packets based on the ENCTYPE chosen
It could be invoked  as a scheduler task or directly as an API.
enc_mgr_init()
 	:initializes the task queue and starts the enc threads
enc_mgr_thread_input()
	:function called by the worker threads
enc_mgr_engine()
	:function which initializes the enc context and 
	 calls do_encrypt()
do_encrypt()
	:actual encryption function
  
*********************************************************************/
#include <openssl/evp.h>

#include "nkn_stat.h"
#include "nkn_encmgr.h"

NKNCNT_DEF(em_tot_input, uint64_t, "", "em total called");
NKNCNT_DEF(em_tot_output, uint64_t, "", "em total called");
NKNCNT_DEF(em_tot_cleanup, uint64_t, "", "em cleanup total called");
NKNCNT_DEF(em_err_encryptupdate, uint64_t, "", "");
NKNCNT_DEF(em_err_encryptfinal, uint64_t, "", "");
NKNCNT_DEF(em_do_encrypt, uint64_t, "", "");
NKNCNT_DEF(em_enctype_tot_aes_128_cbc, uint64_t, "", "");
NKNCNT_DEF(em_enctype_tot_aes_256_cbc, uint64_t, "", "");

int nkn_enc_mgr_enable = 0;

static nkn_mutex_t encreq_mutex;
static pthread_cond_t encreq_cond = PTHREAD_COND_INITIALIZER;
static nkn_task_id_t enc_task_arr[MAX_ENC_TASK_ARR];

volatile int enc_task_arr_tail;
volatile int enc_task_arr_head;

pthread_t encthr[MAX_ENC_THREADS];

int do_encrypt(EVP_CIPHER_CTX *e, unsigned char *pt, unsigned char *ct,
			 int *inlen, int *olen);

static int isenc_task_arrFull( void )
{
	if ((enc_task_arr_tail < enc_task_arr_head)
        	&& (enc_task_arr_head - enc_task_arr_tail) == 1)
        	return 1;
    	if ((enc_task_arr_tail == (MAX_ENC_TASK_ARR - 1))
        	&& (enc_task_arr_head == 0))
	{
        	return 1;
	}
    	return 0;
}

static int get_enc_task_arrCnt(void )
{
	if (enc_task_arr_head < enc_task_arr_tail)
        	return enc_task_arr_tail - enc_task_arr_head;
    	else
	{
        	if (enc_task_arr_head == enc_task_arr_tail)
		{
            		return 0;
		}
        	else
		{
            		return (enc_task_arr_tail + MAX_ENC_TASK_ARR -
                    enc_task_arr_head);
		}
    	}
}

void enc_mgr_input(nkn_task_id_t id)
{
	glob_em_tot_input ++;

        nkn_task_set_state(id, TASK_STATE_EVENT_WAIT);

        NKN_MUTEX_LOCK(&encreq_mutex);
        if (isenc_task_arrFull())
	{
                nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
                                        TASK_STATE_RUNNABLE);
		NKN_MUTEX_UNLOCK(&encreq_mutex);
                return;
        }
        enc_task_arr[enc_task_arr_tail] = id;
        if (enc_task_arr_tail == (MAX_ENC_TASK_ARR - 1)) 
	{
                enc_task_arr_tail = 0;
        } else {
                enc_task_arr_tail++;
	}
        pthread_cond_signal(&encreq_cond);
        NKN_MUTEX_UNLOCK(&encreq_mutex);
}

void enc_mgr_output(nkn_task_id_t id)
{
	glob_em_tot_output ++;
	UNUSED_ARGUMENT(id);
	return;
}

void enc_mgr_cleanup(nkn_task_id_t id)
{
	glob_em_tot_cleanup ++;
	UNUSED_ARGUMENT(id);
	return;
}

void *enc_input_handler_thread(void *arg)
{
     	nkn_task_id_t taskid;

	UNUSED_ARGUMENT(arg);
     	prctl(PR_SET_NAME, "nvsd-enc-input-work", 0, 0, 0);

     	while (1)
     	{
        	NKN_MUTEX_LOCK(&encreq_mutex);
                if (get_enc_task_arrCnt() < 1) 
		{
                        pthread_cond_wait(&encreq_cond, &encreq_mutex.lock);
                        NKN_MUTEX_UNLOCK(&encreq_mutex);
                }
 		else
		{
                        taskid = enc_task_arr[enc_task_arr_head];
                        if ( enc_task_arr_head == (MAX_ENC_TASK_ARR - 1))
			{
                                enc_task_arr_head = 0;
                        }
			else
			{
                                enc_task_arr_head++ ;
			}
                        NKN_MUTEX_UNLOCK(&encreq_mutex);

                        enc_mgr_thread_input(taskid);
                }
     	}
}


void enc_mgr_thread_input(nkn_task_id_t id)
{
	int ret=0;
    	enc_msg_t * data;
	struct nkn_task     *ntask = nkn_task_get_task(id);

    	if(!ntask)
    	{
		DBG_LOG(ERROR, MOD_EM, "Unable to get task from scheduler");
	        nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
                                                        TASK_STATE_RUNNABLE);

		return;
	}

    	data = (enc_msg_t*)nkn_task_get_data(id);
   	ret=enc_mgr_engine(data);
	data->errcode=ret;
    	nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
                                                        TASK_STATE_RUNNABLE);

}

void enc_mgr_init(void)
{
        long i;

	if (nkn_enc_mgr_enable == 0) {
		return;
	}

	NKN_MUTEX_INIT(&encreq_mutex, NULL, "encreq_mutex");
        for(i=0; i<MAX_ENC_THREADS; i++)
	{
        	DBG_LOG(MSG, MOD_EM,
                                "Thread %ld created for Encoding Manager",i);
                if(pthread_create(&encthr[i], NULL,
			 enc_input_handler_thread, (void *)i) != 0)
		{
                	DBG_LOG(SEVERE, MOD_EM,
				 "Failed to create thread %ld",i);
                }
        }

}

int enc_mgr_engine(enc_msg_t *data)
{
	int ret=0;
	EVP_CIPHER_CTX en_ctx;
	EVP_CIPHER_CTX_init(&en_ctx);

	switch(data->enctype)
	{
		case AES_128_CBC:
		glob_em_enctype_tot_aes_128_cbc++;
		ret=EVP_EncryptInit_ex(&en_ctx, EVP_aes_128_cbc(),
				 NULL, data->key, data->iv);

#if 0		
		int32_t i;
		printf("Key: ");
		for (i =0; i < data->key_len; i++)
		    printf("%02X", data->key[i]);
		printf("\nIV: ");
		for (i =0; i < data->iv_len; i++)
		    printf("%02X", data->iv[i]);
		printf("\nkey: %d\niv: %d\n", data->key_len, data->iv_len);
#endif 		
	        if(!ret)
        	{
                	DBG_LOG(ERROR, MOD_EM,
                          "Encryption context failed \
                          initializing");
			EVP_CIPHER_CTX_cleanup(&en_ctx);
                	return -1;
		}
        
                DBG_LOG(MSG, MOD_EM,
                                "initializing AES_128_CBC \
				with key:%s and iv:%s",
				data->key, data->iv);

		break;

		case AES_256_CBC:
		glob_em_enctype_tot_aes_256_cbc++;
		ret=EVP_EncryptInit_ex(&en_ctx, EVP_aes_256_cbc(),
				 NULL, data->key, data->iv);
                if(!ret)
                {
                        DBG_LOG(ERROR, MOD_EM,
                          "Encryption context failed \
                          initializing");
			EVP_CIPHER_CTX_cleanup(&en_ctx);
                        return -1;
                }

                DBG_LOG(MSG, MOD_EM,
                                "initializing AES_256_CBC \
				 with key:%s and iv:%s",
				 data->key, data->iv); 
		break;
	}
	ret=do_encrypt(&en_ctx, data->in_enc_data,data->out_enc_data,
			&(data->in_len),&(data->out_len));
        DBG_LOG(MSG, MOD_EM,
                          "Encryption complete with \
			   inlen=%d and outlen=%d",
			     (data->in_len),(data->out_len));
	EVP_CIPHER_CTX_cleanup(&en_ctx);
	return ret;

}


int do_encrypt(EVP_CIPHER_CTX *en_ctx, unsigned char *pt, unsigned char *ct,
		 int *inlen, int *olen)
{
	int ret=0;
	/* 
	 * max ciphertext len for a n bytes of plaintext
	 * is n + EVP_CIPHER_CTX_block_size(e) -1 bytes 
	 */
  	int c_len=0, f_len=0;

	glob_em_do_encrypt++;
  	/*
	 * return 1 success, 0 for failure
  	 * update ciphertext, c_len is filled with the length of ciphertext
	 * generated,len is the size of plaintext in bytes 
	 */
  	ret=EVP_EncryptUpdate(en_ctx, ct, &c_len, pt, *inlen);
        if(!ret)
        {
		glob_em_err_encryptupdate++;
                DBG_LOG(ERROR, MOD_EM,
                          "Encryption operation failed"); 
                return -1;
        }
	
	

  	/* update ciphertext with the final remaining bytes */
  	ret=EVP_EncryptFinal_ex(en_ctx, ct+c_len, &f_len);
        if(!ret)
        {
		glob_em_err_encryptfinal++;
                DBG_LOG(ERROR, MOD_EM,
                          "Final Encryption operation failed");
                return -1;
        }

  	*olen = c_len + f_len;

	return 0;
}




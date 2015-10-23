#ifndef CONT_POOL_H
#define CONT_POOL_H

#include <stdint.h>

struct cont_pool;

typedef void* (*get_from_cont_pool_fptr)(struct cont_pool* cp);

typedef int32_t (*put_to_cont_pool_fptr)(struct cont_pool* cp, void* cont);

typedef void (*delete_cont_pool_fptr)(struct cont_pool* cp);


typedef struct cont_pool {

    get_from_cont_pool_fptr get_cont_hdlr;
    put_to_cont_pool_fptr put_cont_hdlr;
    delete_cont_pool_fptr delete_hdlr;
    void* __internal;
}cont_pool_t;


cont_pool_t* createContainerPool(uint32_t cont_cnt_perq, uint32_t queue_cnt);

#endif

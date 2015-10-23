/**
 * @file   cr_common_intf.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Mon Feb 27 16:55:29 2012
 * 
 * @brief  describes the interfaces for the source-sink model for the
 * Content Router (CR) Producs.
 *
 * Objects:
 * obj_protocol_desc - implements the protocol descriptor object that is capable
 * of transforming raw on the wire (OTW) protocol data into a tokenized format
 * and vice versa. the object implements, init, shutdown and the conversion
 * methods
 *
 * Interface I/O data types:
 * proto_token_t - tokenized version of the raw data on the wire (OTW)
 * proto_data_t - raw OTW data
 * 
 */

#ifndef _CR_COMMON_INTF_
#define _CR_COMMON_INTF_

#include <stdio.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

typedef struct tag_proto_token_t proto_token_t;

typedef enum tag_proto_data_buf_type {
    PROTO_DATA_BUF_TYPE_RECV,
    PROTO_DATA_BUF_TYPE_SEND
} proto_data_buf_type_t;

struct tag_proto_data;
typedef int32_t (*proto_data_get_curr_buf_fnp)(struct tag_proto_data*, 
					       const uint8_t **,
					       uint32_t *, uint32_t *);
typedef int32_t (*proto_data_get_buf_fnp)(struct tag_proto_data*, 
					  uint32_t, 
					  const uint8_t **, 
					  uint32_t *, uint32_t *);
typedef void (*proto_data_mark_curr_buf_fnp)(struct tag_proto_data*);
typedef void (*proto_data_set_curr_buf_type_fnp)
(struct tag_proto_data*, proto_data_buf_type_t);
typedef proto_data_buf_type_t (*proto_data_get_buf_type_fnp)
(struct tag_proto_data*, uint32_t);
typedef int32_t (*proto_data_update_curr_buf_rw_pos_fnp)
(struct tag_proto_data *, uint32_t );

typedef struct tag_proto_data {
    pthread_mutex_t lock;
    uint32_t num_iovecs;
    uint32_t alloc_size_per_vec;
    uint32_t exp_data_size;
    uint32_t curr_vec_id;
    struct iovec *otw_data;
    proto_data_buf_type_t *buf_type;

    proto_data_get_curr_buf_fnp get_curr_buf;
    proto_data_mark_curr_buf_fnp mark_curr_buf;
    proto_data_set_curr_buf_type_fnp set_curr_buf_type;
    proto_data_update_curr_buf_rw_pos_fnp upd_curr_buf_rw_pos;

    proto_data_get_buf_fnp get_buf;
    proto_data_get_buf_type_fnp get_buf_type;
} proto_data_t;

typedef int32_t (*proto_desc_init_fnp)(void *state_data);
typedef int32_t (*proto_desc_init_shutdown_fnp)(void *state_data);
typedef int32_t (*proto_data_to_token_data_fnp)(void *state_data, 
						proto_data_t *buf,
						void **proto_token);
typedef int32_t (*token_data_to_proto_data_fnp)(void *state_data,
						proto_token_t *token_data,
						proto_data_t **buf);

typedef struct tag_obj_proto_desc {
    pthread_mutex_t lock;
    void *state_data;
    proto_desc_init_fnp init;
    proto_desc_init_shutdown_fnp shutdown;
    proto_data_to_token_data_fnp proto_data_to_token_data;
    token_data_to_proto_data_fnp token_data_to_proto_data;
} obj_proto_desc_t;

struct tag_obj_store_t;
typedef int32_t (*obj_store_init_fnp)(struct tag_obj_store_t *, void *);
typedef int32_t (*obj_store_shutdown_fnp)(struct tag_obj_store_t *);
typedef int32_t (*obj_store_read_fnp)(struct tag_obj_store_t *, 
      char *key, uint32_t key_len, char *buf, uint32_t *buf_len);
typedef int32_t (*obj_store_write_fnp)(struct tag_obj_store_t *, 
       char *key, uint32_t key_len, char *buf, uint32_t buf_len);

typedef struct tag_obj_store_t {
    pthread_mutex_t lock;
    void *state_data;
    obj_store_init_fnp init;
    obj_store_shutdown_fnp shutdown;
    obj_store_read_fnp read;
    obj_store_write_fnp write;
} obj_store_t;

#ifdef __cplusplus
}
#endif

#endif //_CR_COMMON_INTF_

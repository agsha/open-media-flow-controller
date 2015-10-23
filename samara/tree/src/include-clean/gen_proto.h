/*
 *
 * gen_proto.h
 *
 *
 *
 */

#ifndef __GEN_PROTO_H_
#define __GEN_PROTO_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "gcl.h"

typedef struct gen_header gen_header;
typedef struct gen_proto_msg gen_proto_msg;
typedef struct gen_request gen_request;
typedef struct gen_response gen_response;

typedef enum
{
    gen_none =          0,
    gen_type_request =  1,
    gen_type_response = 2,
    gen_LAST
} gen_msg_type;

static const lc_enum_string_map gen_msg_type_map[] = {
    {gen_type_request, "gen_type_request"},
    {gen_type_response, "gen_type_response"},
    {0, NULL}
};

int gen_msg_create(void **ret_req_msg, gen_msg_type msg_type, uint32 msg_len);

int gen_msg_free(void **ret_msg);

int gen_msg_send(gcl_session *sess, const void *msg);

int gen_msg_dump(const gcl_session *sess,
                 const gen_proto_msg *gen_msg, 
                 int log_level,  /* like LOG_INFO, or -1 for no logging */
                 const char *prefix);

int gen_init(gcl_handle *gclh);

int gen_deinit(gcl_handle *gclh);

typedef int (*gen_request_callback_func)
  (gcl_session *sess, void *request, uint32 len, void *arg);

typedef int (*gen_response_callback_func)
  (gcl_session *sess, void *response, uint32 len, void *arg);

typedef int (*gen_accept_callback_func)
  (gcl_session *sess, void *arg);

int
gen_set_request_callback(gcl_handle *gclh,
                         gen_request_callback_func callback_func,
                         void *callback_arg);
int
gen_set_response_callback(gcl_handle *gclh,
                          gen_response_callback_func callback_func,
                          void *callback_arg);

int
gen_set_accept_callback(gcl_handle *gclh,
                        gen_accept_callback_func callback_func,
                        void *callback_arg);

#ifdef __cplusplus
}
#endif

#endif /* __GEN_PROTO_H_ */

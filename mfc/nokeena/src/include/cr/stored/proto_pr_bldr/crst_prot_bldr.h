#ifndef CRST_PROT_BLDR_H
#define CRST_PROT_BLDR_H

#include <stdint.h>

struct crst_prot_bldr;

typedef int32_t (*crst_pb_rem_prot_data_fptr)(struct crst_prot_bldr* bldr,
	uint8_t* prot_data, int32_t prot_data_len);

typedef int32_t (*crst_pb_set_code_fptr)(struct crst_prot_bldr* bldr, 
	uint32_t resp_code);

typedef int32_t (*crst_pb_set_msg_name_fptr)(struct crst_prot_bldr* bldr,
	char const* name);

typedef int32_t (*crst_pb_set_desc_fptr)(struct crst_prot_bldr* bldr, 
	char const* resp_desc);

typedef int32_t (*crst_pb_add_cont_fptr)(struct crst_prot_bldr* bldr, 
	uint8_t const* cont, uint32_t cont_len);

typedef void (*crst_bld_delete_fptr)(struct crst_prot_bldr* bldr);

typedef struct crst_prot_bldr {

    crst_pb_rem_prot_data_fptr get_prot_data_hdlr;
    crst_pb_set_code_fptr set_resp_code_hdlr;
    crst_pb_set_msg_name_fptr set_msg_name_hdlr;
    crst_pb_set_desc_fptr set_desc_hdlr;
    crst_pb_add_cont_fptr add_content_hdlr;
    crst_bld_delete_fptr delete_hdlr;
    void* __internal;
} crst_prot_bldr_t;


crst_prot_bldr_t* createCRST_ProtocolBuilder(uint32_t approx_buf_len);

#endif

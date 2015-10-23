#ifndef CRST_PROT_PARSER_H
#define CRST_PROT_PARSER_H

#include <stdint.h>

struct crst_prot_parser;
struct crst_prot_msg;

typedef int32_t (*crst_msg_hdlr_fptr)(struct crst_prot_msg* msg,
	void* con_ctx);

typedef int32_t (*crst_prot_parse_fptr)(struct crst_prot_parser* pr,
	uint8_t const* buf, uint32_t buf_len);

typedef void (*crst_prot_parser_delete_fptr)(struct crst_prot_parser* pr);

typedef struct crst_prot_parser {

   crst_prot_parse_fptr parse_hdlr;
   crst_prot_parser_delete_fptr delete_hdlr;
    void*__internal;
} crst_prot_parser_t;


crst_prot_parser_t* createCRST_Prot_Parser(uint32_t max_buf_len, 
	crst_msg_hdlr_fptr msg_hdlr, void* msg_hdlr_ctx);


typedef enum {

    CREATE,
    ADD,
    REMOVE,
    UPDATE,
    GET,
    DELETE,
    SEARCH,
    UNDEFINED
} crst_prot_method_t;

typedef crst_prot_method_t (*crst_get_prot_method_fptr)(
	struct crst_prot_msg const* msg);

typedef void (*crst_get_prot_uri_fptr)(struct crst_prot_msg const* msg,
	char const**uri);

typedef uint32_t (*crst_get_prot_cont_len_fptr)(struct crst_prot_msg const*msg);

typedef void (*crst_get_prot_content_fptr)(struct crst_prot_msg const* msg, 
	uint8_t const** buf);

typedef void (*crst_prot_msg_print_fptr)(struct crst_prot_msg const* msg);

typedef void (*crst_prot_msg_delete_fptr)(struct crst_prot_msg* msg);

typedef struct crst_prot_msg {

    crst_get_prot_method_fptr get_method_hdlr;
    crst_get_prot_uri_fptr get_uri_hdlr;
    crst_get_prot_cont_len_fptr get_clen_hdlr;
    crst_get_prot_content_fptr get_content_hdlr;
    crst_prot_msg_print_fptr print_hdlr;
    crst_prot_msg_delete_fptr delete_hdlr;
    void* __internal;
 
} crst_prot_msg_t;

#endif


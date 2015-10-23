/*
 *
 * src/bin/web/xml_gw/xg_main.h
 *
 *
 *
 */

#ifndef __XG_MAIN_H_
#define __XG_MAIN_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "md_client.h"
#include "gcl.h"
#include "bnode_proto.h"

typedef enum {
    xf_string = 1, /* "normal" format */
    xf_base64 = 2, /* base64Binary format, [A-Za-z0-9+/=] */
    xf_hex = 3,    /* hexBinary format, pairs of [0-9a-fA-F] */
    xf_LAST,
} xg_value_format;

static const lc_enum_string_map xg_value_format_map[] = {
    { xf_string, "string" },
    { xf_base64, "base64binary" },
    { xf_hex, "hexbinary" },
    { 0, NULL}
};

typedef struct xg_request_state {
    tbool xrs_is_web;
    char xrs_request_input_filename[256];
    char xrs_request_output_filename[256];
    int xrs_request_input_fd;
    int xrs_request_output_fd;

    /* The incoming request */
    tbuf *xrs_request;


    char xrs_provider_name[32];
    gcl_body_proto xrs_body_proto;
    bap_service_id xrs_service;
    char xrs_lang_tag[32];

    /* How we'll talk to the mgmt provider */
    md_client_context *xrs_mcc;

    /* Set as soon as we know, maybe before xrs_mgmt_request */
    bn_msg_type xrs_mgmt_request_type;

    /* What we'll send to mgmtd */
    bn_request *xrs_mgmt_request;
    tbool xrs_mgmt_request_sent;

    uint32 xrs_request_code;
    tstring *xrs_request_msg;

    /* Set to 'none' if we don't have a response from mgmtd */
    bn_msg_type xrs_mgmt_response_type;

    /* What we received from mgmtd */
    bn_response *xrs_mgmt_response;

    /* Options about response */
    tbool xrs_response_opt_include_header;
    tbool xrs_response_opt_nodes_tree;
    tbool xrs_response_opt_name_parts;
    tbool xrs_response_opt_include_namespace;
    tbool xrs_response_opt_use_stylesheet;
    xg_value_format xrs_response_opt_binary_format;

    /* The response to send back to the original requestor */
    tbuf *xrs_response;
} xg_request_state;

extern tstring *xg_libxml_errors;


#ifdef __cplusplus
}
#endif

#endif /* __XG_MAIN_H_ */

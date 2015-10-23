#include <stdio.h>

#include "nkn_debug.h"

#include "common/cr_utils.h"
#include "crst_request_proc.h"
#include "crst_nw_handler.h"
#include "crst_con_ctx.h"

#include "ds_api.h"
#include "de_intf.h"
#include "ds_tables.h"


/* EXTERN */
extern de_intf_t gl_obj_de_rr;
extern de_intf_t gl_obj_de_geo_lf;
/* GLOBALS */
uint32_t resp_code[] = {200, 400, 404, 501};
char const* resp_str[] = {"OK", "Bad Request", "Not Found", "Not Supported"};

static int32_t handleDomainLookup(char const* uri, char**response,
				  uint32_t *response_len); 

int32_t crstMsgHandler(crst_prot_msg_t* msg, void* ctx) {

    
    msg_hdlr_ctx_t* msg_ctx = (msg_hdlr_ctx_t*)ctx;
    ref_count_mem_t* ref_cont = msg_ctx->entity_ctx->context;
    uint32_t response_len = 0;
    ref_cont->hold_ref_cont(ref_cont);

    int32_t rc = 3; 
    char const* uri;
    char* response = NULL;;
    msg->get_uri_hdlr(msg, &uri);
    if (msg->get_method_hdlr(msg) == SEARCH) {
	DBG_LOG(MSG, MOD_CRST, "SEARCH: %s", uri);
	rc = handleDomainLookup(uri, &response, &response_len);
    } else {
	DBG_LOG(ERROR, MOD_CRST, "Unsupported store request "
		"type %s", uri);
    }
    crst_con_ctx_t* con_ctx = (crst_con_ctx_t*)ref_cont->mem;
    crst_prot_bldr_t* bldr = con_ctx->bldr;
    bldr->set_resp_code_hdlr(bldr, resp_code[rc]);
    bldr->set_desc_hdlr(bldr, resp_str[rc]);
	
    if (response != NULL) {
	DBG_LOG(MSG, MOD_CRST, "resp code: %u\n response: %s", 
		resp_code[rc], resp_str[rc]);
	uint32_t result_len = response_len;
	bldr->add_content_hdlr(bldr, (uint8_t*)response, result_len);
	free(response);
    }  
    
    msg_ctx->disp->set_write_hdlr(msg_ctx->disp, msg_ctx->entity_ctx);
    msg->delete_hdlr(msg);
    ref_cont->release_ref_cont(ref_cont);

    return 0;
}


static int32_t handleDomainLookup(char const* uri, char**response,
				  uint32_t *response_len) {

    *response = NULL;
    if (strncmp(uri, "/DN/", 4) != 0) {
	return 3;
    }
    uri += 4;
    char const* pos = strchr(uri, '/');
    if (pos == NULL) {
	return 1;
    }
    uint32_t dn_len = pos - uri;
    if (dn_len >= 512)
	return 1;
    char dn[dn_len+1];
    strncpy(&dn[0], uri, dn_len);
    dn[dn_len] = '\0';

    pos+=12;
    char const* pos1 = strchr(pos, '?');
    if (pos1 == NULL) {
	return 1;
    }
    uint32_t src_ip_len = pos1-pos;
    char src_ip[src_ip_len+1];
    strncpy(src_ip, pos, pos1-pos+1);
    src_ip[src_ip_len] = '\0';
    pos = pos1;

    char *qtype_str = strdup(pos + 7); 
    if (qtype_str == NULL) {
	return 1;
    }
    int32_t rv = get_domain_lookup_resp(&dn[1], qtype_str, &src_ip[0],
	    response, response_len);
    free(qtype_str);
    return rv;
}


#if 0
    if (de_res) { 
	glob_crst_de_fail++;
	uint32_t rlen, ttl = 0;

	if (domain_ctx->cname_last_resort[0] == '\0') {
	    result = NULL;
	    result_size = 0;
	    rc = -1;
	    DBG_LOG(ERROR, MOD_CRST, "DE [type=%d] failed, "
		    "and CNAME of last resort is absent", 
		    domain_ctx->rp.type);
	    goto unlock_return;
	}
	rlen = rrecord_msg_fmt_builder_compute_record_size(
			   domain_ctx->cname_last_resort_len);

	rc = rrecord_msg_fmt_builder_create(rlen, &rrb);
	if (rc) {
	    goto unlock_return;
	}

	rc = rrb->add_hdr(rrb, 1, ttl);
	if (rc) {
	    goto unlock_return;
	}

	rc = rrb->add_record(rrb, ce_addr_type_ns,
	      (uint8_t *)domain_ctx->cname_last_resort,
	      domain_ctx->cname_last_resort_len);
	if (rc) {
	    goto unlock_return;
	}
	
	rc = rrb->get_buf(rrb, (uint8_t **) result, 
			   result_size);
	if (rc) {
	    goto unlock_return;
	}

    } else {
	domain_ctx->stats.num_hits++;
    }
#endif

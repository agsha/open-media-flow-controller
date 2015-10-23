/*
 * nkn_pseudo_con.h
 *      Interfaces to create a psuedo con_t for use in OM requests
 *      which do _not_ originate from a client.
 *      The created object is passed in the 'in_proto_data' field of
 *      the MM_stat_resp_t and MM_get_resp_t structures.
 */
#ifndef _NKN_PSEUDO_CON_H_
#define _NKN_PSEUDO_CON_H_

#include "server.h"
#include "nkn_namespace.h"
#include "nkn_am_api.h"

typedef struct nkn_pseudo_con { // derived class
    con_t con; // Always first
    char dummy_buf[8];
} nkn_pseudo_con_t;

nkn_pseudo_con_t *nkn_alloc_pseudo_con_t(void);
nkn_pseudo_con_t *nkn_alloc_pseudo_con_t_custom(int size);
int nkn_pseudo_con_t_init(nkn_pseudo_con_t *pcon, const nkn_attr_t *attr,
			  int method, char *uri, namespace_token_t token);
void nkn_free_pseudo_con_t(nkn_pseudo_con_t *pcon);
int nkn_copy_con_client_data(int nomodule_check, void *in_proto_data,
			     void **out_proto_data);
void nkn_free_con_client_data(void *proto_data);
int nkn_copy_host_hdr(char *host_hdr,
			     void **out_host_hdr);
int nkn_copy_con_host_hdr(void *in_proto_data,
			     void **out_host_hdr);
void nkn_free_con_host_hdr(void *out_host_hdr);
void nkn_populate_proto_data(void **in_proto_data,
			nkn_client_data_t * out_client_data,
			am_object_data_t * in_client_data,
			char * uri,
			namespace_token_t token, nkn_provider_type_t src_ptype);
void nkn_populate_attr_data(nkn_attr_t *nknattr, nkn_pseudo_con_t *pcon, 
			    nkn_provider_type_t src_ptype);
void nkn_free_proto_data(void *in_proto_data, nkn_provider_type_t src_ptype);

nkn_pseudo_con_t *nkn_create_pcon(void);

void nkn_populate_encoding_hdr(uint16_t encoding_type,
				    nkn_pseudo_con_t *pseudo_con);

#endif /* _NKN_PSEUDO_CON_H_ */

/*
 * End of nkn_pseudo_con.h
 */

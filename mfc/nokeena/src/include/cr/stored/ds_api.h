#ifndef _DS_API
#define _DS_API

#include <stdio.h>
#include <inttypes.h>
#include "store_errno.h"
#include "crst_common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

int32_t create_new_CG(char* name);
int32_t add_CE_to_CG(char* cg_name, char* ce_name);
int32_t del_CE_from_CG(char* cg_name, char* ce_name);
int32_t add_domain_to_CG(char* cg_name, char* domain);
int32_t set_attr_CG(char* cg_name, char* attr, void* val);
int32_t del_domain_from_CG(char* cg_name, char* domain);
int32_t del_CG(char* cg_name);
int32_t create_new_CE(const char* ce_name, 
	      const char *ce_address, 
	      cache_entity_addr_type_t ce_addr_type, 
	      uint8_t lf_method, 
	      uint32_t lf_poll_freq, 
	      uint32_t lf_port,
	      uint32_t ce_load_watermark);
int32_t set_attr_CE(char* ce_name, char* attr, void* val);
int32_t del_CE(char* ce_name);
int32_t create_new_POP(char* pop_name, char*, char*);
int32_t map_CE_to_POP(char* pop_name, char* ce_name);
int32_t del_CE_from_POP(char* pop_name, char* ce);
int32_t set_attr_POP(char* pop_name, char* attr, void* val);
int32_t del_POP(char*);

int32_t get_domain_list(void** domain_list);
int32_t get_cg_list_for_domain(char* domain_name, void* cg_list);
int32_t get_attr_names_domain(char* domain_name);
int32_t get_attr_domain(char* domain_name, char* attr, void*val);
int32_t set_attr_domain(char* domain_name, char* attr, void*val);
int32_t del_cg_from_domain(char*domain, char* cg_name);
int32_t add_cg_to_domain(char* domain, char* cg_name);
int32_t update_attr_domain(char* domain_name, 
			   const char *const last_resort,
			   uint8_t routing_type,
			   uint32_t static_rid,
			   const char *const static_ip,
			   const char *const static_ip_end,
			   double geo_weight, 
			   double load_weight,
			   uint32_t ttl);
int32_t del_domain(char *domain_name);

int32_t get_domain_lookup_resp(char const* dname, char const* qtype_str, 
	char const* src_ip, char** response, uint32_t* response_len); 

#ifdef __cplusplus
}
#endif

#endif //_DS_API

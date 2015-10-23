#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include "tstring.h"

#include "stdint.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_mgmt_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_common_config.h"
#include "nvsd_mgmt_lib.h"

#include "crst_mgmt.h"
#include "de_intf.h"
#include "ds_tables.h"
#include "ds_api.h"

/* extern  */
extern const char *config_sequence[];
extern uint32_t num_config_sequence; 
extern cr_dns_store_cfg_t cr_dns_store_cfg;

/* Function Prototypes */


/* Local Variables */
const char nkn_cr_stored_config_prefix[] = "/nkn/cr_dns_store/config";

static const char nkn_cr_stored_max_domains_binding[] = "/nkn/cr_dns_store/config/global/max_domains"; //bt_uint32;
static const char nkn_cr_stored_max_cache_entities_binding[] = "/nkn/cr_dns_store/config/global/max_cache_entities"; //bt_uint32;
static const char nkn_cr_stored_listen_path_binding[] = "/nkn/cr_dns_store/config/global/listen_path"; //bt_string
static const char nkn_cr_stored_listen_port_binding[] = "/nkn/cr_dns_store/config/global/listen_port";//bt_uint32
static const char nkn_cr_stored_debug_assert_binding[] = "/nkn/cr_dns_store/config/global/debug_assert";

const char nkn_cr_stored_domain_config_prefix[] = "/nkn/cr_dns_store/config/cr_domain/**";
static const char nkn_cr_stored_domain_binding[] = "/nkn/cr_dns_store/config/cr_domain/*";
static const char nkn_cr_stored_domain_ttl_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/ttl";
static const char nkn_cr_stored_domain_last_resort_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/last_resort";
#if 0
static const char nkn_cr_stored_domain_static_ip_start_range_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/static_ip_range/*/start";
static const char nkn_cr_stored_domain_static_ip_end_range_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/static_ip_range/*/end";
static const char nkn_cr_stored_domain_static_cg_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/static_ip_range/*/cg";
#endif
static const char nkn_cr_stored_domain_static_ip_start_range_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/static_ip_range/start";
static const char nkn_cr_stored_domain_static_ip_end_range_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/static_ip_range/end";

static const char nkn_cr_stored_domain_cache_group_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/cache_group";
static const char nkn_cr_stored_domain_routing_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/routing"; //bt_uint8
static const char nkn_cr_stored_domain_geo_weight_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/geo_weight";
static const char nkn_cr_stored_domain_load_weight_binding[] = "/nkn/cr_dns_store/config/cr_domain/*/load_weight";

const char nkn_cr_stored_pop_config_prefix[] = "/nkn/cr_dns_store/config/pop/**";
static const char nkn_cr_stored_pop_binding[] = "/nkn/cr_dns_store/config/pop/*";
static const char nkn_cr_stored_pop_latitude_binding[] = "/nkn/cr_dns_store/config/pop/*/location/latitude";
static const char nkn_cr_stored_pop_longitude_binding[] = "/nkn/cr_dns_store/config/pop/*/location/longitude";

const char nkn_cr_stored_cache_group_name_binding[] = "/nkn/cr_dns_store/config/cache-group/*";

const char nkn_cr_stored_cache_entity_config_prefix[] = "/nkn/cr_dns_store/config/cache-entity/**";
static const char nkn_cr_stored_cache_entity_binding[] = "/nkn/cr_dns_store/config/cache-entity/*";
static const char nkn_cr_stored_cache_entity_ipv4address_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/ipv4address";
static const char nkn_cr_stored_cache_entity_ipv6address_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/ipv6address";
static const char nkn_cr_stored_cache_entity_cname_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/cname";
static const char nkn_cr_stored_cache_entity_group_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/cache-group/*";
static const char nkn_cr_stored_cache_entity_pop_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/pop/*";
static const char nkn_cr_stored_cache_entity_watermark_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/water_mark";
static const char nkn_cr_stored_cache_entity_lf_method_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/lf_method";
static const char nkn_cr_stored_cache_entity_poll_freq_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/poll_freq"; //bt_uint32
static const char nkn_cr_stored_cache_entity_lf_port_binding[] = "/nkn/cr_dns_store/config/cache-entity/*/port"; //bt_uint32
/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cr_stored_cfg_query()
 */
int
nkn_cr_stored_cfg_query(const bn_binding_array *bindings, uint32_t i)
{
    int err = 0;
    tbool rechecked_licenses = false;

    err = cr_mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		     config_sequence[i], NULL, 
		     nkn_cr_stored_cfg_handle_change,
		     &rechecked_licenses);
    bail_error(err);

bail:
    return err;

}

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cr_stored_module_cfg_handle_change()
 *	purpose : handler for config changes for nkn 'content sync engine' config parameters module
 */
int
nkn_cr_stored_module_cfg_handle_change (bn_binding_array *old_bindings,
					 bn_binding_array *new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;
    
    err = cr_mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
    		nkn_cr_stored_config_prefix,
			NULL,
			nkn_cr_stored_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);

    err = cr_mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
    		nkn_cr_stored_config_prefix,
			NULL,
			nkn_cr_stored_delete_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);

bail:
	return err;

} /* end of nkn_cr_stored_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cr_stored_cfg_handle_change()
 *	purpose : handler for config changes for cr_stored module
 */

int nkn_cr_stored_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
		const bn_binding *binding,
		const tstring *bndgs_name,
		const tstr_array *bndgs_name_components,
		const tstring *bndgs_name_last_part,
		const tstring *b_value,
		void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_domain_name = NULL;
    const char *t_pop_name = NULL;
    const char *t_ce_name = NULL;
    tstr_array *domain_name_parts = NULL;
    tstr_array *pop_name_parts = NULL;
    tstr_array *ce_name_parts = NULL;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is our node */
    if (! bn_binding_name_pattern_match (ts_str(name), "/nkn/cr_dns_store/config/**"))
    {
    	goto bail;
    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_max_domains_binding)) {
	uint32 t_max_domains = 0;
	
	err = bn_binding_get_uint32(binding,
				    ba_value, NULL,
				    &t_max_domains);
	bail_error(err);
	
	pthread_mutex_lock(&cr_dns_store_cfg.lock);
	if (t_max_domains) {
	    cr_dns_store_cfg.max_domains = t_max_domains;
	}
	pthread_mutex_unlock(&cr_dns_store_cfg.lock);
	
    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_max_cache_entities_binding)) {
	uint32 t_max_cache_entities = 0;
		
	err = bn_binding_get_uint32(binding,
				ba_value, NULL,
				&t_max_cache_entities);
	bail_error(err);
	
	pthread_mutex_lock(&cr_dns_store_cfg.lock);
	if (t_max_cache_entities) {
	    cr_dns_store_cfg.max_cache_entities =
		t_max_cache_entities;
	}
	pthread_mutex_unlock(&cr_dns_store_cfg.lock);
		
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_listen_path_binding)) {

    	tstring *t_listen_path= NULL;

    	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_listen_path);
    	bail_error(err);

		pthread_mutex_lock(&cr_dns_store_cfg.lock);

		//cr_dns_store_cfg.listen_path = t_listen_path ? 1:0;
		strlcpy(cr_dns_store_cfg.listen_path, ts_str(t_listen_path), 256);
		pthread_mutex_unlock(&cr_dns_store_cfg.lock);

     }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_listen_port_binding)) {
		uint32 t_listen_port = 0;

		err = bn_binding_get_uint32(binding,
				ba_value, NULL,
				&t_listen_port);
		bail_error(err);

		pthread_mutex_lock(&cr_dns_store_cfg.lock);
		cr_dns_store_cfg.listen_port = t_listen_port;
		pthread_mutex_unlock(&cr_dns_store_cfg.lock);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_debug_assert_binding)) {
  		tbool t_debug_assert = false;

   		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_debug_assert);
		bail_error(err);

  		pthread_mutex_lock(&cr_dns_store_cfg.lock);
  		cr_dns_store_cfg.debug_assert = t_debug_assert ? 1 :0;
		pthread_mutex_unlock(&cr_dns_store_cfg.lock);

    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_config_prefix)) {

    	bn_binding_get_name_parts (binding, &domain_name_parts);
    	bail_error_null(err, domain_name_parts);

    	t_domain_name = tstr_array_get_str_quick (domain_name_parts, 4);
    	bail_error(err);

	//   	create_domain((char *) t_domain_name);
	update_attr_domain((char *) t_domain_name, NULL, 0, 0, 
			   NULL, NULL, 0.0, 0.0, 0); 
    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_last_resort_binding)) {

    	tstring *t_last_resort= NULL;
       	char t_attrib[] = "last-resort";

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_last_resort);
	bail_error(err);
	
	update_attr_domain((char *) t_domain_name, 
			   (const char *)ts_str(t_last_resort),
			   0, 0, NULL, NULL, 0.0, 0.0, 0);
	//   		set_attr_domain((char *) t_domain_name, t_attrib, (char *)ts_str(t_last_resort));
    }
#if 0
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_static_ip_start_range_binding)) {

    	tstring *t_static_ip_start_range= NULL;
       	char t_attrib[] = "static-ip-start-range";
       	const char *iprange_index = NULL;

       	iprange_index = tstr_array_get_str_quick (domain_name_parts, 6);
    	bail_error(err);

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_static_ip_start_range);
	bail_error(err);

	//	set_attr_domain((char *) t_domain_name, t_attrib,
	//	(char *)ts_str(t_static_ip));
	update_attr_domain((char *) t_domain_name, NULL, 0, 0,
		   (const char*)ts_str(t_static_ip_start_range), 
		   NULL, 0.0, 0.0, 0);

    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_static_ip_end_range_binding)) {

    	tstring *t_static_ip_end_range= NULL;
       	char t_attrib[] = "static-ip-end-range";
       	const char *iprange_index = NULL;

       	iprange_index = tstr_array_get_str_quick (domain_name_parts, 6);
    	bail_error(err);

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_static_ip_end_range);
	bail_error(err);

	//	set_attr_domain((char *) t_domain_name, t_attrib,
	//	(char *)ts_str(t_static_ip));
	update_attr_domain((char *) t_domain_name, NULL, 0, 0, 
		   NULL, (const char *)ts_str(t_static_ip_end_range),
		   0.0, 0.0, 0);

    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_static_cg_binding)) {

    	tstring *t_ip_range_cg= NULL;
       	char t_attrib[] = "static-ip-cg";
       	const char *iprange_index = NULL;

       	iprange_index = tstr_array_get_str_quick (domain_name_parts, 6);
    	bail_error(err);

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ip_range_cg);
	bail_error(err);

	//	set_attr_domain((char *) t_domain_name, t_attrib,
	//	(char *)ts_str(t_static_ip));
	update_attr_domain((char *) t_domain_name, NULL, 0,
	   (const char *)ts_str(t_ip_range_cg), 0.0, 0.0, 0);

    }
#endif
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_static_ip_start_range_binding)) {

    	tstring *t_static_ip_start_range= NULL;
       	char t_attrib[] = "static-ip-start-range";

       	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_static_ip_start_range);
       	bail_error(err);

     	update_attr_domain((char *) t_domain_name,
       										 NULL,
       										    0,
       										    0,
  		   (const char*)ts_str(t_static_ip_start_range),
		   	   	   	   	   	   	   	   	   	   NULL,
		   	   	   	   	   	   	   	   	   	   0.0,
		   	   	   	   	   	   	   	   	   	   0.0,
		   	   	   	   	   	   	   	   	   	   0);

    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_static_ip_end_range_binding)) {

    	tstring *t_static_ip_end_range= NULL;
       	char t_attrib[] = "static-ip-end-range";

       	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_static_ip_end_range);
       	bail_error(err);

      	update_attr_domain((char *) t_domain_name,
       										 NULL,
       										    0,
       										    0,
		   	   	   	   	   	   	   	   	   	   NULL,
		   (const char*)ts_str(t_static_ip_end_range),
		   	   	   	   	   	   	   	   	   	   0.0,
		   	   	   	   	   	   	   	   	   	   0.0,
		   	   	   	   	   	   	   	   	   	   0);

    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_cache_group_binding)) {

    	tstring *t_domain_cg= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_domain_cg);
	bail_error(err);

	add_cg_to_domain((char *) t_domain_name, 
			 (char *)ts_str(t_domain_cg));
	
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_routing_binding)) {
	uint8 t_domain_routing = 0;
      	char t_attrib[] = "routing";
	
	err = bn_binding_get_uint8(binding,
				   ba_value, NULL,
				   &t_domain_routing);
	bail_error(err);
	
	//set_attr_domain((char *)t_domain_name, t_attrib, (void *)&t_domain_routing);
	update_attr_domain((char *) t_domain_name, NULL, t_domain_routing,
			   0, NULL, NULL, 0.0, 0.0, 0);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_ttl_binding)) {
	uint32 t_domain_ttl = 0;
      	char t_attrib[] = "ttl";

	err = bn_binding_get_uint32(binding,
				   ba_value, NULL,
				   &t_domain_ttl);
	bail_error(err);
	//set_attr_domain((char *)t_domain_name, t_attrib, (void *)&t_domain_ttl);
	update_attr_domain((char *) t_domain_name, NULL, 0, 0,
			   NULL, NULL, 0.0, 0.0, t_domain_ttl);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_geo_weight_binding)) {

    	tstring *t_geo_wt= NULL;
      	char t_attrib[] = "geo-weight";

      	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_geo_wt);
      	bail_error(err);

	//	set_attr_domain((char *) t_domain_name, t_attrib,
	//	(void *)&t_geo_wt);
	if (t_geo_wt) {
	    update_attr_domain((char *) t_domain_name, NULL, 0, 0,
			   NULL, NULL, atof(ts_str(t_geo_wt)), 
			   0.0, 0);
	}
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_load_weight_binding)) {

    	tstring *t_load_wt = NULL;
      	char t_attrib[] = "load-weight";
	
      	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_load_wt);
      	bail_error(err);

	//	set_attr_domain((char *) t_domain_name, t_attrib,
	//	(void *)&t_geo_wt);

	if (t_load_wt) {
	    update_attr_domain((char *) t_domain_name, NULL, 0, 0,
		   NULL, NULL, 0.0, atof(ts_str(t_load_wt)), 0);
	}
    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_pop_config_prefix)) {

    	bn_binding_get_name_parts (binding, &pop_name_parts);
    	bail_error_null(err, pop_name_parts);

    	t_pop_name = tstr_array_get_str_quick (pop_name_parts, 4);
    	bail_error(err);

    	create_new_POP((char *)t_pop_name, NULL, NULL);

    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_pop_latitude_binding)) {

    	tstring *t_pop_latitude= NULL;

    	char t_attrib[] = "latitude";

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_pop_latitude);
	bail_error(err);
	
	create_new_POP((char *)t_pop_name, 
		       (char *)ts_str(t_pop_latitude), NULL); 
		//   	    set_attr_POP((char *)t_pop_name, t_attrib, (void *)ts_str(t_pop_latitude));

    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_pop_longitude_binding)) {

    	tstring *t_pop_longitude= NULL;
    	char t_attrib[] = "longitude";

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_pop_longitude);
	bail_error(err);
	
	create_new_POP((char *)t_pop_name, NULL,
		       (char *)ts_str(t_pop_longitude)); 
	//	set_attr_POP((char *)t_pop_name, t_attrib, (void *)ts_str(t_pop_longitude));

   }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_group_name_binding)) {

    	tstring *t_cg_name= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_cg_name);
	bail_error(err);
	
	create_new_CG((char *)ts_str(t_cg_name));
	
    }


    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_config_prefix)) {

    	bn_binding_get_name_parts (binding, &ce_name_parts);
    	bail_error_null(err, ce_name_parts);

    	t_ce_name = tstr_array_get_str_quick (ce_name_parts, 4);
    	bail_error(err);

    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_ipv4address_binding)) {

    	tstring *t_ipv4address= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ipv4address);
	bail_error(err);
	
	create_new_CE((char *)t_ce_name, 
		      (char *)ts_str(t_ipv4address),
		      ce_addr_type_ipv4, 
		      0, 0, 0, 0);

    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_lf_method_binding)) {
	uint8 t_domain_lfm = 0;

	err = bn_binding_get_uint8(binding,
				   ba_value, NULL,
				   &t_domain_lfm);
	create_new_CE(t_ce_name, NULL, 0, t_domain_lfm, 0, 0, 0);

	bail_error(err);
	
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_poll_freq_binding)) {
	uint32 t_poll_freq = 0;
	
	err = bn_binding_get_uint32(binding,
				    ba_value, NULL,
				    &t_poll_freq);
	bail_error(err);
	create_new_CE(t_ce_name, NULL, 0, 0, t_poll_freq, 0, 0);
	
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_lf_port_binding)) {
	uint32 t_port = 0;
	
	err = bn_binding_get_uint32(binding,
				    ba_value, NULL,
		 		    &t_port);
	bail_error(err);
	create_new_CE(t_ce_name, NULL, 0, 0, 0, t_port, 0);
	
    }

    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_ipv6address_binding)) {

    	tstring *t_ipv6address= NULL;

   		err = bn_binding_get_tstr(binding,
   				ba_value, bt_string, NULL,
   				&t_ipv6address);
   		bail_error(err);

	create_new_CE((char *)t_ce_name, 
		      (char *)ts_str(t_ipv6address),
		      ce_addr_type_ipv6, 
		      0, 0, 0, 0);
	
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_cname_binding)) {

    	tstring *t_cname= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_cname);
	bail_error(err);
	
	create_new_CE((char *)t_ce_name, 
		      (char*)ts_str(t_cname),
		      ce_addr_type_cname, 
		      0, 0, 0, 0);
	
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_group_binding)) {

    	tstring *t_ce_cg= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ce_cg);
	bail_error(err);

	add_CE_to_CG((char *)ts_str(t_ce_cg), (char *)t_ce_name);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_pop_binding)) {

    	tstring *t_ce_pop= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ce_pop);
	bail_error(err);
	map_CE_to_POP((char *)ts_str(t_ce_pop), (char *)t_ce_name);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_watermark_binding)) {
	uint32 t_watermark = 0;
    	char t_attrib[] = "watermark";

	err = bn_binding_get_uint32(binding,
				    ba_value, NULL,
				    &t_watermark);
	bail_error(err);
	
	//	set_attr_CE((char *)t_ce_name, t_attrib, (void *)
	//	&t_watermark);
	create_new_CE((char *)t_ce_name, 
		      NULL, 0,
		      0, 0, 0, t_watermark);

    }

bail:
    return err;
}


/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cr_stored_delete_cfg_handle_change()
 *	purpose : handler for config changes for cr_stored module
 */

int nkn_cr_stored_delete_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
		const bn_binding *binding,
		const tstring *bndgs_name,
		const tstr_array *bndgs_name_components,
		const tstring *bndgs_name_last_part,
		const tstring *b_value,
		void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

   	/* Check if this is our node */
   	if (!(bn_binding_name_pattern_match (ts_str(name), "/nkn/cr_dns_store/config/**")))
   	{
      	goto bail;
    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_group_name_binding)) {

     	tstring *t_cg_name= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_cg_name);
	bail_error(err);
	
	del_CG((char *)ts_str(t_cg_name));
    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_binding)) {

       	tstring *t_domain= NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_domain);
	bail_error(err);
	del_domain((char *)ts_str(t_domain));
    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_pop_binding)) {

       	tstring *t_pop = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_pop);
	bail_error(err);
	
	del_POP((char*)ts_str(t_pop));

    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_binding)) {

       	tstring *t_ce = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ce);
	bail_error(err);
	
	del_CE((char*) ts_str(t_ce));
    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_pop_binding)) {

       	tstring *t_ce_pop= NULL;
       	const char *t_ce = NULL;

       	bn_binding_get_name_parts (binding, &name_parts);
       	bail_error_null(err, name_parts);

       	t_ce = tstr_array_get_str_quick (name_parts, 4);
       	bail_error(err);

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ce_pop);
	bail_error(err);

	del_CE_from_POP((char*) ts_str(t_ce_pop), (char*) t_ce);


    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_cache_entity_group_binding)) {

	tstring *t_ce_cg= NULL;
	const char *t_ce = NULL;

	bn_binding_get_name_parts (binding, &name_parts);
	bail_error_null(err, name_parts);

	t_ce = tstr_array_get_str_quick (name_parts, 4);
	bail_error(err);

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_ce_cg);
	bail_error(err);

	del_CE_from_CG((char*) ts_str(t_ce_cg), (char*) t_ce);
    }
    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_stored_domain_cache_group_binding)) {

	tstring *t_domain_cg= NULL;
	const char *t_domain = NULL;

	bn_binding_get_name_parts (binding, &name_parts);
	bail_error_null(err, name_parts);

	t_domain = tstr_array_get_str_quick (name_parts, 4);
	bail_error(err);

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_domain_cg);
	bail_error(err);

	del_cg_from_domain((char *) t_domain, 
			   (char*) ts_str(t_domain_cg));
    }

bail:
	tstr_array_free(&name_parts);
    return err;
}


int cr_mgmt_lib_mdc_foreach_binding_prequeried(const bn_binding_array *bindings,
					    const char *binding_spec, const char *db_name,
					    mdc_foreach_binding_func callback, void *callback_data)
{
	int err = 0;
	node_name_t binding_spec_pattern = {0};
	uint32 ret_num_matches = 0;

	snprintf(binding_spec_pattern , sizeof(binding_spec_pattern), "%s/**", binding_spec);
	err = mdc_foreach_binding_prequeried(bindings, binding_spec_pattern, db_name,
			callback,
			callback_data, &ret_num_matches);
	bail_error(err);

bail:
    return err;
}


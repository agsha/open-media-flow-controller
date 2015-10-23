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

#include "dns_mgmt.h"
#include "dns_common_defs.h"


/* extern */
extern const char *config_sequence[];
uint32_t num_config_sequence;
cr_dns_cfg_t g_cfg;

/* globals */

static const char nkn_cr_dnsd_config_prefix[] = "/nkn/cr_dns/config";
static const char nkn_cr_dnsd_listen_port[] = "/nkn/cr_dns/config/listen/port";
static const char nkn_cr_dnsd_tcp_timeout[] = "/nkn/cr_dns/config/tcp/timeout";
static const char nkn_cr_dnsd_tx_threads[] = "/nkn/cr_dns/config/tx/threads";
static const char nkn_cr_dnsd_rx_threads[] = "/nkn/cr_dns/config/rx/threads";
static const char nkn_cr_dnsd_debug_assert[] = "/nkn/cr_dns/config/debug_assert";

int
nkn_cr_dnsd_cfg_query(const bn_binding_array *bindings, uint32_t i)
{
    int err = 0;
    tbool rechecked_licenses = false;

    err = nkn_cr_dnsd_mgmt_lib_mdc_foreach_binding_prequeried(
		      bindings, config_sequence[i], NULL, 
		      nkn_cr_dnsd_cfg_handle_change,
		      &rechecked_licenses);
    bail_error(err);

bail:
    return err;
}

int
nkn_cr_dnsd_module_cfg_handle_change (bn_binding_array *old_bindings,
				      bn_binding_array *new_bindings)
{
	int err = 0;
	tbool rechecked_licenses = false;

   err = nkn_cr_dnsd_mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
    		nkn_cr_dnsd_config_prefix,
			NULL,
			nkn_cr_dnsd_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);

bail:
	return err;

}

int 
nkn_cr_dnsd_cfg_handle_change(
			const bn_binding_array *arr, uint32 idx,
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
    if (! bn_binding_name_pattern_match (ts_str(name), "/nkn/cr_dns/config/**"))
    {
    	goto bail;
    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_dnsd_listen_port)) {
		uint32 t_listen_port = 0;

		err = bn_binding_get_uint32(binding,
				ba_value, NULL,
				&t_listen_port);
		bail_error(err);
		
		HOLD_CFG_OBJ(&g_cfg);
		if (t_listen_port) {
		    g_cfg.dns_listen_port = t_listen_port;
		}
		REL_CFG_OBJ(&g_cfg);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_dnsd_tcp_timeout)) {
  		uint32 t_tcp_timeout = 0;

  		err = bn_binding_get_uint32(binding,
  				ba_value, NULL,
  				&t_tcp_timeout);
  		bail_error(err);

		HOLD_CFG_OBJ(&g_cfg);
		if (t_tcp_timeout) {
		    g_cfg.tcp_timeout = t_tcp_timeout;
		}
		REL_CFG_OBJ(&g_cfg);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_dnsd_tx_threads)) {
  		uint32 t_tx_threads = 0;

  		err = bn_binding_get_uint32(binding,
  				ba_value, NULL,
  				&t_tx_threads);
  		bail_error(err);

		HOLD_CFG_OBJ(&g_cfg);
		if (t_tx_threads) {
		    g_cfg.tx_threads = t_tx_threads;
		}
		REL_CFG_OBJ(&g_cfg);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_dnsd_rx_threads)) {
  		uint32 t_rx_threads = 0;

  		err = bn_binding_get_uint32(binding,
  				ba_value, NULL,
  				&t_rx_threads);
  		bail_error(err);

		HOLD_CFG_OBJ(&g_cfg);
		if (t_rx_threads) {
		    g_cfg.rx_threads = t_rx_threads;
		}
		REL_CFG_OBJ(&g_cfg);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cr_dnsd_debug_assert)) {
  		tbool t_debug_assert = false;

   		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_debug_assert);
		bail_error(err);

		HOLD_CFG_OBJ(&g_cfg);
		g_cfg.debug_assert = t_debug_assert ? 1 :0;
		REL_CFG_OBJ(&g_cfg);
      }

bail:
    return err;
}

int 
nkn_cr_dnsd_delete_cfg_handle_change(
		       const bn_binding_array *arr, uint32 idx,
		       const bn_binding *binding,
		       const tstring *bndgs_name,
		       const tstr_array *bndgs_name_components,
		       const tstring *bndgs_name_last_part,
		       const tstring *b_value,
		       void *data)
{
    int err = 0;

    return err;
}

int nkn_cr_dnsd_mgmt_lib_mdc_foreach_binding_prequeried(
			const bn_binding_array *bindings,
			const char *binding_spec, 
			const char *db_name,
			mdc_foreach_binding_func callback, 
			void *callback_data)
{
    int err = 0;
    node_name_t binding_spec_pattern = {0};
    uint32 ret_num_matches = 0;
    
    snprintf(binding_spec_pattern, 
	     sizeof(binding_spec_pattern), "%s/**", binding_spec);
    err = mdc_foreach_binding_prequeried(bindings, 
			 binding_spec_pattern, db_name,
			 callback, callback_data, 
			 &ret_num_matches);
    bail_error(err);
    
bail:
    return err;
}


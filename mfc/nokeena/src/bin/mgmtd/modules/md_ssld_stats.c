/*
 *
 * Filename:  md_ssld_stats.c
 * Date:      2011/05/23
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#include "file_utils.h"
#include "ssld_mgmt.h"
#include "ssl_cntr_api.h"
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

int md_ssld_stats_init(const lc_dso_info *info, void *data);

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_ssld_vh_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static md_upgrade_rule_array *md_ssld_stats_ug_rules = NULL;

static int
md_iterate_vh(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg);
static int
md_ssld_stats_add_initial(md_commit *commit, mdb_db *db, void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
int
md_ssld_stats_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("ssld_stats", 1,
	    "/stat/nkn/ssld", NULL,
	    modrf_all_changes,
	    NULL, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    0,
	    0,
	    md_generic_upgrade_downgrade,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);


    /* Monitoring Nodes */
    /*! Namespace get counters */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/client/tot_open_sock";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_tot_ssl_sockets";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/client/tot_size_from";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_tot_size_from_ssl";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/client/tot_size_sent";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_tot_size_sent_ssl";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/client/cur_ssl_sock";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_cur_open_ssl_sockets";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/client/handshake_fail";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_tot_handshake_failure";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/client/handshake_success";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_tot_handshake_done";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/tot_sock";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_tot_ssl_sockets";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/cur_sock";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_cur_open_ssl_sockets";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/data_recv";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_tot_size_from_ssl";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/data_sent";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_tot_size_sent_ssl";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/handshake_failure";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_tot_handshake_failure";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/handshake_success";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_tot_handshake_success";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/ssld/origin/server_auth_fail";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_ssld_vh_stats_get_uint64;
    node->mrn_mon_get_arg =         (void *) "glob_origin_tot_server_auth_failures";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    /*! Added in module version 2
     */

bail:
    return(err);
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

static int
md_ssld_vh_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    const char *var_name = (char *)arg;;

    uint64_t n = sslcnt_get_uint64((char*) var_name);

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, n);
    bail_error(err);

bail:
    return err;
}

static int
md_iterate_vh(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)

{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    char *name = NULL;
    tstr_array *names_array = NULL;
    const char *namespaceid_str = NULL;
    const tstring *namespaceid = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/ssld/config/virtual_host",
	    0, &binding_arrays);
    bail_error(err);

    err = bn_binding_array_length(binding_arrays, &num_bindings);
    bail_error(err);

    for ( i = 0 ; i < num_bindings ; ++i) {
	err = bn_binding_array_get(binding_arrays, i, &binding);
	bail_error(err);

	safe_free(name);

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
		&name);

	err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
	bail_error(err);
	safe_free(name);
    }


bail:
    bn_binding_array_free(&binding_arrays);
    tstr_array_free(&names_array);
    return(err);
}

static int
md_ssld_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

bail:
    bn_binding_free(&binding);
    return(err);
}

/* ---------------------------------END OF FILE---------------------------------------- */

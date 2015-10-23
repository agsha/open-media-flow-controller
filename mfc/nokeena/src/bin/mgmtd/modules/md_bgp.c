/*
 *
 * Filename:  md_bgp.c
 *
 * (C) Copyright 2011 Juniper Networks
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
#include "tpaths.h"
#include <arpa/inet.h>

#define VTYSH_PATH       "/usr/local/bin/vtysh"
#define BGPD_CONF_FILE      "/config/usrlocaletc/bgpd.conf"
#define ZEBRA_CONF_FILE      "/config/usrlocaletc/zebra.conf"
#define MAX_VALUE_LEN       256
#define MAX_VTYSH_LEN       200
#define MAX_AS_NUM          4294967295
#define MAX_TIMER_VALUE     65535

static const char *vnet_prefix_node = "/virt/config/vnet/host-only/ip/address";
static const char *vnet_mask_node = "/virt/config/vnet/host-only/ip/mask_len";

const char bgpd_config_path[] = "/usr/local/bin/bgpd-conf.sh";

typedef struct filter_info_
{
    char vnet_prefix[MAX_VALUE_LEN];
    char vnet_mask[MAX_VALUE_LEN];
} filter_info_t;

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_bgp_init(const lc_dso_info *info, void *data);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_bgp_ug_rules = NULL;

static int md_bgp_clear_bgp (md_commit *commit, mdb_db **db,
                const char *action_name, bn_binding_array *params,
                void *cb_arg);

static int
md_bgp_commit_side_effects (md_commit *commit,
                            const mdb_db *old_db, mdb_db *new_db,
                            mdb_db_change_array *change_list, void *arg);

static int
md_bgp_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg);

static int
md_bgp_add_config_filter(filter_info_t *bgp_filter_info_p, tstring *conf_buf);

static int
md_bgp_add_config_bgp(md_commit *commit, const mdb_db *new_db,
    tstring *bgpd_conf_buf);

static int
md_bgp_get_filter_info (md_commit *commit, const mdb_db *new_db,
    filter_info_t *bgp_filter_info_p);

static int
md_bgp_create_zebra_config (md_commit *commit, const mdb_db *new_db,
    filter_info_t *bgp_filter_info_p);

static int
md_bgp_create_bgpd_config(md_commit *commit, const mdb_db *new_db,
    filter_info_t *bgp_filter_info_p);

static int
md_bgp_config_bgpd (md_commit *commit, const mdb_db *new_db,
        mdb_db_change_array *change_list);

static int
md_bgp_config_files (md_commit *commit, const mdb_db *new_db);

static int
md_bgp_config_files_restart (md_commit *commit, const mdb_db *new_db);

static int
md_bgp_validate_as (md_commit *commit, const char *as_str, tbool is_local);

static int
md_bgp_local_as_exists (md_commit *commit,
                     const mdb_db *new_db);

int64_t
md_bgp_get_timer_value (md_commit *commit, const char *timer_str);

static int
md_bgp_check_local_as(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg);

static int
md_bgp_check_neighbor_remote(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg);

static int
md_bgp_check_network_mask(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg);

static int
md_bgp_check_router_id(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg);

static int
md_bgp_get_timers(md_commit *commit, const mdb_db *new_db,
                  const char *local_as, tstring **t_timer_ka_p,
                  tstring **t_timer_hold_p);

static int
md_bgp_check_timers(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

int
md_bgp_init (const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating BGP module
     */
    err = mdm_add_module("bgp", /*module_name*/
                        2,      /*module_version*/
                        "/nkn/bgp",     /*root_node_name*/
                        "/nkn/bgp/config",      /*root_config_name*/
                        0,      /*md_mod_flags*/
	                md_bgp_commit_side_effects,     /*side_effects_func*/
                        NULL,   /*side_effects_arg*/
                        NULL,   /*check_func*/
                        NULL,   /*check_arg*/
                        md_bgp_commit_apply,    /*apply_func*/
                        NULL,   /*apply_arg*/
                        0,      /*commit_order*/
                        0,      /*apply_order*/
                        md_generic_upgrade_downgrade,   /*updown_func*/
                        &md_bgp_ug_rules,   /*updown_arg*/
                        NULL,   /*initial_func*/
                        NULL,   /*initial_arg*/
                        NULL,   /*mon_get_func*/
                        NULL,   /*mon_get_arg*/
                        NULL,   /*mon_iterate_func*/
                        NULL,   /*mon_iterate_arg*/
                        &module);       /*ret_module*/
    bail_error(err);

    err = mdm_set_interest_subtrees(module, "/virt/config");
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_check_node_func       = md_bgp_check_local_as;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP local AS node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/neighbor/*";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP neighbor node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/neighbor/*/remote_as";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "false";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_check_node_func       = md_bgp_check_neighbor_remote;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP neighbor remote node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/neighbor/*/shutdown";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "false";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP neighbor shutdown node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/network/*";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP network node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/network/*/*";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_check_node_func       = md_bgp_check_network_mask;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP network mask node ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/redist_conn";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "init";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP redistribute connected node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/log_neighbor";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "true";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP log neighbor changes node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/router_id";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "init";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_check_node_func       = md_bgp_check_router_id;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP router ID node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/timer_ka";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "init";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_check_node_func       = md_bgp_check_timers;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP keep-alive time node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/config/local_as/*/timer_hold";
    node->mrn_value_type            = bt_string;
    node->mrn_initial_value         = "init";
    node->mrn_limit_str_max_chars   = 256;
    node->mrn_check_node_func       = md_bgp_check_timers;
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    node->mrn_description           = "BGP hold timer node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name                  = "/nkn/bgp/actions/clear_bgp";
    node->mrn_node_reg_flags        = mrf_flags_reg_action;
    node->mrn_cap_mask              = mcf_cap_action_privileged;
    node->mrn_action_config_func    = md_bgp_clear_bgp;
    node->mrn_action_arg            = (void *)NULL;
    node->mrn_description           = "action node for clear bgp";
    err = mdm_add_node(module, &node);
    bail_error(err);

	err = md_upgrade_rule_array_new(&md_bgp_ug_rules);
	bail_error(err);
	ra = md_bgp_ug_rules;


    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/bgp/config/local_as/*/neighbor/*/shutdown" ;
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/bgp/config/local_as/*/log_neighbor";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "true";
    MD_ADD_RULE(ra);

bail:
	return(err);
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

static int md_bgp_clear_bgp (md_commit *commit, mdb_db **db,
                const char *action_name, bn_binding_array *params,
                void *cb_arg)
{
    int err = 0;

    bail_require_quiet(!strcmp(action_name, "/nkn/bgp/actions/clear_bgp"));

    err = lc_launch_quick_status(NULL, NULL, false, 3,
            VTYSH_PATH, "-c", "clear bgp *");
    bail_error(err);

bail:
    return(err);
}


/* write filter configurations to a buffer conf_buf provided by the caller */
static int
md_bgp_add_config_filter (filter_info_t *bgp_filter_info_p, tstring *conf_buf)
{
    int err = 0;

    err = ts_append_sprintf(conf_buf,
                            "!\naccess-list host-only permit %s/%s\n!\n",
                            bgp_filter_info_p->vnet_prefix,
                            bgp_filter_info_p->vnet_mask);
    bail_error(err);

    err = ts_append_sprintf(conf_buf,
                            "route-map filter-host-only deny 10\n"
                            " match ip address host-only\n!\n"
                            "route-map filter-host-only permit 20\n!\n");
    bail_error(err);

bail:
    return(err);
}

/* write BGP configurations to a buffer bgpd_conf_buf provided by the caller */
static int
md_bgp_add_config_bgp (md_commit *commit, const mdb_db *new_db,
                       tstring *bgpd_conf_buf)
{
    int err = 0;
    int i = 0, num_bindings = 0;
    bn_binding_array *binding_arrays = NULL;
    tstr_array *bname_parts = NULL;
    const bn_binding *binding = NULL;
    tstring    *t_value = NULL;
    char        timer_hold[MAX_VALUE_LEN];
    const char *local_as = NULL, *neighbor = NULL, *network = NULL,
        *mask = NULL;

    /* generate router bgp configurations */
    err = mdb_iterate_binding(commit, new_db, "/nkn/bgp/config/local_as",
            mdqf_sel_iterate_subtree, &binding_arrays);
    bail_error(err);

    num_bindings = bn_binding_array_length_quick(binding_arrays);
    lc_log_basic(LOG_DEBUG, "num_bindings for bgp is %d", num_bindings);

    for (i = 0; i < num_bindings; i++) {

        binding = bn_binding_array_get_quick(binding_arrays, i);
        bail_null(binding);
        err = bn_binding_get_name_parts(binding, &bname_parts);
        bail_error(err);
        err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
                                  &t_value);
        bail_error_null(err, t_value);

        if (bn_binding_name_parts_pattern_match(bname_parts, true,
                "/nkn/bgp/config/local_as/*")) {
            local_as = tstr_array_get_str_quick(bname_parts, 4);
            bail_null(local_as);

            err = ts_append_sprintf(bgpd_conf_buf,
                                    "router bgp %s\n", local_as);
            bail_error(err);

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/neighbor/*/remote_as")) {
            neighbor = tstr_array_get_str_quick(bname_parts, 6);
            bail_null(neighbor);

            err = ts_append_sprintf(bgpd_conf_buf,
                            " neighbor %s remote-as %s\n", neighbor,
                            ts_str(t_value));
            bail_error(err);

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/neighbor/*/shutdown")) {
            if (strcmp(ts_str(t_value), "true") == 0)  {
                neighbor = tstr_array_get_str_quick(bname_parts, 6);
                bail_null(neighbor);

                err = ts_append_sprintf(bgpd_conf_buf,
                      " neighbor %s shutdown\n", neighbor);
                bail_error(err);
            }

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/network/*/*")) {
            network = tstr_array_get_str_quick(bname_parts, 6);
            bail_null(network);

            mask = tstr_array_get_str_quick(bname_parts, 7);
            bail_null(mask);

            err = ts_append_sprintf(bgpd_conf_buf,
                    " network %s/%s\n", network, mask);
            bail_error(err);

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/redist_conn")) {
            if (strcmp(ts_str(t_value), "false") != 0 &&
                strcmp(ts_str(t_value), "init") != 0)  {
                err = ts_append_sprintf(bgpd_conf_buf,
                      " redistribute connected route-map filter-host-only\n");
                bail_error(err);
            }

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/log_neighbor")) {
            if (strcmp(ts_str(t_value), "true") == 0)  {
                err = ts_append_sprintf(bgpd_conf_buf,
                      " bgp log-neighbor-changes\n");
                bail_error(err);
            }

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/router_id")) {
            if (strcmp(ts_str(t_value), "false") != 0 &&
                strcmp(ts_str(t_value), "init") != 0) {
                err = ts_append_sprintf(bgpd_conf_buf,
                        " bgp router-id %s\n", ts_str(t_value));
                bail_error(err);
            }

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/timer_hold")) {
            safe_strlcpy(timer_hold, ts_str(t_value));

        } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
            "/nkn/bgp/config/local_as/*/timer_ka")) {
            /* hold timer should have been retrieved */
            if (timer_hold[0] != 0) {
                if (strcmp(timer_hold, "false") != 0 &&
                    strcmp(timer_hold, "init") != 0 &&
                    strcmp(ts_str(t_value), "false") != 0 &&
                    strcmp(ts_str(t_value), "init") != 0) {
                    err = ts_append_sprintf(bgpd_conf_buf,
                                " timers bgp %s %s\n",
                                ts_str(t_value), timer_hold);
                    bail_error(err);
                }
            }
        }

        tstr_array_free(&bname_parts);
        ts_free(&t_value);
    }

bail:
    bn_binding_array_free(&binding_arrays);
    return(err);
}

/* get vnet host-only prefix and mask */
static int
md_bgp_get_filter_info (md_commit *commit, const mdb_db *new_db,
    filter_info_t *bgp_filter_info_p)
{
    int err = 0;
    tstring *t_vnet_prefix = NULL, *t_vnet_mask = NULL;
    tbool found = false;

    safe_strlcpy(bgp_filter_info_p->vnet_prefix, "0.0.0.0");
    safe_strlcpy(bgp_filter_info_p->vnet_mask, "32");

    /* generate access-list to filter host-only vnet */
    err = mdb_get_node_value_tstr(commit, new_db, vnet_prefix_node,
                                  0, &found, &t_vnet_prefix);
    if (found == false) {
        lc_log_debug(LOG_DEBUG, "host-only vnet not present");
        err = 0;
        goto bail;
    }
    bail_error_null(err, t_vnet_prefix);

    /* vnet_prefix_node doesn't exist */
    if (ts_equal_str(t_vnet_prefix, "0.0.0.0", false)) {
        lc_log_debug(LOG_DEBUG, "host-only vnet is 0.0.0.0");
        goto bail;
    }

    err = mdb_get_node_value_tstr(commit, new_db, vnet_mask_node,
                                  0, NULL, &t_vnet_mask);
    bail_error_null(err, t_vnet_mask);

    bgp_filter_info_p->vnet_prefix[MAX_VALUE_LEN - 1] = 0;
    bgp_filter_info_p->vnet_mask[MAX_VALUE_LEN - 1] = 0;
    safe_strlcpy(bgp_filter_info_p->vnet_prefix, ts_str(t_vnet_prefix));
    safe_strlcpy(bgp_filter_info_p->vnet_mask, ts_str(t_vnet_mask));

bail:
    ts_free(&t_vnet_prefix);
    ts_free(&t_vnet_mask);
    return(err);
}

/* create zebra.conf */
static int
md_bgp_create_zebra_config (md_commit *commit, const mdb_db *new_db,
    filter_info_t *bgp_filter_info_p)
{
    int err = 0, status = 0;
    char *zebra_conf_temp = NULL;
    int zebra_conf_fd = -1;
    int32 bytes_written = 0;
    tstring *zebra_conf_buf = NULL;

    err = lf_temp_file_get_fd(ZEBRA_CONF_FILE,
                              &zebra_conf_temp, &zebra_conf_fd);
    bail_error(err);

    err = ts_new(&zebra_conf_buf);
    bail_error(err);

    err = ts_append_sprintf(zebra_conf_buf,
          "password n1keenAZebra\nlog syslog\ndebug zebra events\ndebug zebra packet\ndebug zebra kernel\ndebug zebra rib\n!\n");
    bail_error(err);

    err = md_bgp_add_config_filter(bgp_filter_info_p, zebra_conf_buf);
    bail_error(err);

    /* Write the file now. */
    err = lf_write_bytes_tstr(zebra_conf_fd, zebra_conf_buf, &bytes_written);
    bail_error(err);

    close(zebra_conf_fd);

    err = lf_temp_file_activate(ZEBRA_CONF_FILE, zebra_conf_temp,
                                md_gen_conf_file_owner,
                                md_gen_conf_file_group,
                                md_gen_conf_file_mode, true);
    bail_error(err);

bail:
    ts_free(&zebra_conf_buf);
    safe_free(zebra_conf_temp);

    return(err);
}

/* create bgpd.conf */
static int
md_bgp_create_bgpd_config (md_commit *commit, const mdb_db *new_db,
    filter_info_t *bgp_filter_info_p)
{
    int err = 0, status = 0;
    char *bgpd_conf_temp = NULL;
    int bgpd_conf_fd = -1;
    int32 bytes_written = 0;
    tstring *bgpd_conf_buf = NULL;

    err = lf_temp_file_get_fd(BGPD_CONF_FILE,
                              &bgpd_conf_temp, &bgpd_conf_fd);
    bail_error(err);

    err = ts_new(&bgpd_conf_buf);
    bail_error(err);

    err = ts_append_sprintf(bgpd_conf_buf,
          "password n1keenABgpd\nlog syslog\ndebug bgp\ndebug bgp as4\ndebug bgp events\ndebug bgp keepalives\ndebug bgp updates\ndebug bgp fsm\ndebug bgp filters\ndebug bgp zebra\n!\n");
    bail_error(err);

    err = md_bgp_add_config_bgp(commit, new_db, bgpd_conf_buf);
    bail_error(err);

    err = md_bgp_add_config_filter(bgp_filter_info_p, bgpd_conf_buf);
    bail_error(err);

    /* Write the file now. */
    err = lf_write_bytes_tstr(bgpd_conf_fd, bgpd_conf_buf, &bytes_written);
    bail_error(err);

    close(bgpd_conf_fd);

    err = lf_temp_file_activate(BGPD_CONF_FILE, bgpd_conf_temp,
                                md_gen_conf_file_owner,
                                md_gen_conf_file_group,
                                md_gen_conf_file_mode, true);
    bail_error(err);

bail:
    ts_free(&bgpd_conf_buf);
    safe_free(bgpd_conf_temp);

    return(err);
}

/* create bgpd.conf and zebra.conf */
static int
md_bgp_config_files (md_commit *commit, const mdb_db *new_db)
{
    int err = 0;
    filter_info_t bgp_filter_info;

    err = md_bgp_get_filter_info(commit, new_db, &bgp_filter_info);
    bail_error(err);

    err = md_bgp_create_zebra_config(commit, new_db, &bgp_filter_info);
    bail_error(err);

    err = md_bgp_create_bgpd_config(commit, new_db, &bgp_filter_info);
    bail_error(err);

bail:
    return(err);
}

/* config bgpd through bgpd-conf.sh script */
static int
md_bgp_apply_config(md_commit *commit, tbool sense, const char *local_as, char *cmd_str)
{
    int     err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result lr;
    char    router_bgp_str[MAX_VTYSH_LEN];
    char    full_sub_cmd_str[MAX_VTYSH_LEN];

    lc_log_basic(LOG_DEBUG, "bgp config: sense: %d local_as: %s cmd_str: %s",
                 sense, local_as, cmd_str == NULL ? "(NULL)" : cmd_str);

    memset(router_bgp_str, 0, MAX_VTYSH_LEN);
    memset(full_sub_cmd_str, 0, MAX_VTYSH_LEN);

    memset(&lr, 0, sizeof(lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), bgpd_config_path);
    bail_error(err);

    if (cmd_str == NULL) {
        /* "router bgp <as-num>" CLI, no sub-command */
        snprintf(router_bgp_str, MAX_VTYSH_LEN - 1,
                 "%srouter bgp %s", sense ? "" : "no ", local_as);

        err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 2,
                bgpd_config_path, router_bgp_str);
        bail_error(err);

    } else {
        snprintf(router_bgp_str, MAX_VTYSH_LEN - 1,
                 "router bgp %s", local_as);
        snprintf(full_sub_cmd_str, MAX_VTYSH_LEN - 1,
                 "%s%s", sense ? "" : "no ", cmd_str);

        err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 3,
                bgpd_config_path, router_bgp_str, full_sub_cmd_str);
        bail_error(err);
    }

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    err = lc_launch(lp, &lr);
    bail_error(err);

 bail:
    lc_launch_params_free(&lp);
    return(err);
}

/* config bgpd process */
static int
md_bgp_config_bgpd (md_commit *commit, const mdb_db *new_db,
        mdb_db_change_array *change_list)
{
    int err = 0;
    uint32  num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    const char *local_as = NULL, *neighbor = NULL,
               *prefix_str = NULL, *mask_str = NULL;
    tbool bgpd_config_file = false;
    tstring *t_remote_as = NULL, *t_shutdown = NULL, *t_redist_conn = NULL,
        *t_router_id = NULL, *t_timer_hold = NULL, *t_timer_ka = NULL,
        *t_log_neighbor = NULL;
    char sub_command_str[MAX_VTYSH_LEN];
    tbool bgpd_config = false, timer_config = false;

    num_changes = mdb_db_change_array_length_quick(change_list);

    /* go through each of the new BGP configurations, "init" value is to
       prevent automatically created nodes to trigger BGP config, which
       can result in race condition and error messages */
    for(i = 0; i < num_changes; i++) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (ts_has_prefix_str(change->mdc_name, "/nkn/bgp/config", false)==0) {
            continue;
        }

        /* create bgpd config files after seeing the 1st bgp CLU */
        if (bgpd_config == false) {
            bgpd_config = true;

            err = md_bgp_config_files(commit, new_db);
            bail_error(err);
        }

        memset(sub_command_str, 0, MAX_VTYSH_LEN);

        lc_log_basic(LOG_DEBUG,
                     "md_bgp_commit_apply node name %s change type %d",
                     ts_str(change->mdc_name), change->mdc_change_type);

        local_as = tstr_array_get_str_quick(change->mdc_name_parts, 4);
        bail_null(local_as);

        if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,1,"*")) {
            if (mdct_add == change->mdc_change_type ||
                mdct_modify == change->mdc_change_type) {

                err = md_bgp_apply_config(commit, true, local_as, NULL);

            } else if (mdct_delete == change->mdc_change_type) {

                err = md_bgp_apply_config(commit, false, local_as, NULL);
                /* if a BGP router is removed, all sub-config would
                   also be removed. no need for further config */
                break;
            }

        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         3, "*", "neighbor", "*")) {
            if (mdct_delete != change->mdc_change_type) {
                continue;
            }

            neighbor = tstr_array_get_str_quick(change->mdc_name_parts, 6);
            bail_null(neighbor);

            snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                     "neighbor %s", neighbor);
            err = md_bgp_apply_config(commit, false, local_as, sub_command_str);

        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         4, "*", "neighbor", "*", "remote_as")) {
            if (mdct_add != change->mdc_change_type &&
                mdct_modify != change->mdc_change_type) {
                continue;
            }

            neighbor = tstr_array_get_str_quick(change->mdc_name_parts, 6);
            bail_null(neighbor);

            err = mdb_get_node_value_tstr(commit, new_db,
                      ts_str(change->mdc_name), 0, NULL, &t_remote_as);
            bail_error_null(err, t_remote_as);

            snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                     "neighbor %s remote-as %s",
                      neighbor, ts_str(t_remote_as));

            err = md_bgp_apply_config(commit, true, local_as, sub_command_str);

            ts_free(&t_remote_as);

        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         4, "*", "neighbor", "*", "shutdown")) {
            if (mdct_add != change->mdc_change_type &&
                mdct_modify != change->mdc_change_type) {
                continue;
            }

            neighbor = tstr_array_get_str_quick(change->mdc_name_parts, 6);
            bail_null(neighbor);

            err = mdb_get_node_value_tstr(commit, new_db,
                    ts_str(change->mdc_name), 0, NULL, &t_shutdown);
            bail_error_null(err, t_shutdown);

            snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                     "neighbor %s shutdown", neighbor);

            if (strcmp(ts_str(t_shutdown), "false") == 0) {
                err = md_bgp_apply_config(commit, false, local_as, sub_command_str);
            } else if (strcmp(ts_str(t_shutdown), "true") == 0) {
                err = md_bgp_apply_config(commit, true, local_as, sub_command_str);
            }

            ts_free(&t_shutdown);

        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         4, "*", "network", "*", "*")) {
            prefix_str = tstr_array_get_str_quick(change->mdc_name_parts, 6);
            bail_null(prefix_str);

            mask_str = tstr_array_get_str_quick(change->mdc_name_parts, 7);
            bail_null(mask_str);

            snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                     "network %s/%s", prefix_str, mask_str);

            if (mdct_add == change->mdc_change_type ||
                mdct_modify == change->mdc_change_type) {

                err = md_bgp_apply_config(commit, true, local_as, sub_command_str);
            } else if (mdct_delete == change->mdc_change_type) {

                err = md_bgp_apply_config(commit, false, local_as, sub_command_str);
            }
        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         2, "*", "redist_conn")) {
            if (mdct_add != change->mdc_change_type &&
                mdct_modify != change->mdc_change_type) {
                continue;
            }

            err = mdb_get_node_value_tstr(commit, new_db,
                                          ts_str(change->mdc_name),
                                          0, NULL, &t_redist_conn);
            bail_error_null(err, t_redist_conn);

            if (strcmp(ts_str(t_redist_conn), "false") == 0) {
                snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                         "redistribute connected");
                err = md_bgp_apply_config(commit, false, local_as, sub_command_str);
            } else if (strcmp(ts_str(t_redist_conn), "init") != 0) {

                snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                         "redistribute connected route-map filter-host-only");
                err = md_bgp_apply_config(commit, true, local_as, sub_command_str);
            }

            ts_free(&t_redist_conn);

        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         2, "*", "log_neighbor")) {
            if (mdct_add != change->mdc_change_type &&
                mdct_modify != change->mdc_change_type) {
                continue;
            }

            err = mdb_get_node_value_tstr(commit, new_db,
                                          ts_str(change->mdc_name),
                                          0, NULL, &t_log_neighbor);
            bail_error_null(err, t_log_neighbor);

            snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                     "bgp log-neighbor-changes");
            err = md_bgp_apply_config(commit,
                                strcmp(ts_str(t_log_neighbor), "true") ? false : true,
                                local_as, sub_command_str);
            ts_free(&t_log_neighbor);

        } else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                         2, "*", "router_id")) {
            if (mdct_add != change->mdc_change_type &&
                mdct_modify != change->mdc_change_type) {
                continue;
            }

            err = mdb_get_node_value_tstr(commit, new_db,
                                          ts_str(change->mdc_name),
                                          0, NULL, &t_router_id);
            bail_error_null(err, t_router_id);

            if (strcmp(ts_str(t_router_id), "false") == 0) {
                snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                         "bgp router-id");
                err = md_bgp_apply_config(commit, false, local_as, sub_command_str);
            } else if (strcmp(ts_str(t_router_id), "init") != 0) {
                snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                         "bgp router-id %s", ts_str(t_router_id));
                err = md_bgp_apply_config(commit, true, local_as, sub_command_str);
            }

            ts_free(&t_router_id);

        } else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                          2, "*", "timer_ka")
                    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                          2, "*", "timer_hold"))
                    && timer_config == false) {


            if (mdct_add != change->mdc_change_type &&
                mdct_modify != change->mdc_change_type) {
                continue;
            }

            /* only config timers once */
            timer_config = true;

            err = md_bgp_get_timers(commit, new_db, local_as,
                                    &t_timer_ka, &t_timer_hold);
            bail_error_null(err, t_timer_ka);
            bail_null(t_timer_hold);

            if (strcmp(ts_str(t_timer_ka), "false") == 0) {
                snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                         "timers bgp");
                err = md_bgp_apply_config(commit, false, local_as, sub_command_str);
            } else if (strcmp(ts_str(t_timer_ka), "init") != 0) {
                snprintf(sub_command_str, MAX_VTYSH_LEN - 1,
                         "timers bgp %s %s",
                         ts_str(t_timer_ka), ts_str(t_timer_hold));
                err = md_bgp_apply_config(commit, true, local_as, sub_command_str);
            }

            ts_free(&t_timer_ka);
            ts_free(&t_timer_hold);
        }
    }

bail:
    return(err);
}

/* create bgpd.conf and zebra.conf, and restart both processes */
static int
md_bgp_config_files_restart (md_commit *commit, const mdb_db *new_db)
{
    int err = 0, status = 0;
    tstring *output1 = NULL, *output2 = NULL;

    err = md_bgp_config_files(commit, new_db);
    bail_error(err);

    /* send SIGTERM to zebra and bgpd. SIGHUP doesn't clean up access-list
       for zebra. pm will restart zebra and bgpd */
    err = lc_launch_quick_status(&status, &output1, false, 3,
            "/usr/bin/killall", "-15", "zebra");
    bail_error(err);

    err = lc_launch_quick_status(&status, &output2, false, 3,
            "/usr/bin/killall", "-15", "bgpd");
    bail_error(err);

bail:
    ts_free(&output1);
    ts_free(&output2);

    return(err);
}

static int64_t
md_bgp_str_to_number(const char *str)
{
    int64_t number;

    /* check whether str includes only numbers */
    if (strspn(str, "0123456789") != strlen(str)) {
        return -1;
    }

    number = atol(str);
    return number;
}

static int
md_bgp_validate_as (md_commit *commit, const char *as_str, tbool is_local)
{
    int err = 0;
    int str_len;
    int64_t as_number;

    if (*as_str == '0') {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
                _("AS number cannot have leading 0"));
        bail_error(err);
        return 1;
    }

    as_number = md_bgp_str_to_number(as_str);

    if (as_number < 1 || as_number > MAX_AS_NUM) {
        if (is_local) {
            err = md_commit_set_return_status_msg_fmt(commit, 1,
                    _("Invalid local AS number"));
        } else {
            err = md_commit_set_return_status_msg_fmt(commit, 1,
                    _("Invalid remote AS number. Remote AS should be specified before other neighbor configuration commands"));
        }
        bail_error(err);
        return 1;
    }

bail:
    return err;
}

static int
md_bgp_local_as_exists (md_commit *commit,
                     const mdb_db *new_db)
{
    int err = 0;
    uint32 num_bindings = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    char *local_as = NULL;

    err = mdb_iterate_binding(commit, new_db, "/nkn/bgp/config/local_as",
            0, &binding_arrays);
    bail_error(err);

    err = bn_binding_array_length(binding_arrays, &num_bindings);
    bail_error(err);

    if (num_bindings > 1) {
        err = 1;
        goto bail;
    }

bail:
    bn_binding_array_free(&binding_arrays);
    return(err);
}

static int
md_bgp_check_local_as(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const char *local_as = NULL;

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
        goto bail;
    }

    local_as = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(local_as);

    err = md_bgp_validate_as(commit, local_as, true);
    if (err) {
        goto bail;
    }

    if (md_bgp_local_as_exists(commit, new_db)) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
              _("BGP is already running. Run show bgp to find out the local AS"));
        bail_error(err);
        goto bail;
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    return(err);
}

static int
md_bgp_check_neighbor_remote(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const char *neighbor = NULL;
    struct in_addr neighbor_addr;
    tstring *t_remote_as = NULL;

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
        goto bail;
    }

    /* check neighbor IP */
    neighbor = tstr_array_get_str_quick(node_name_parts, 6);
    bail_null(neighbor);

    if (inet_pton(AF_INET, neighbor, &neighbor_addr) <= 0) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
              _("Invalid neighbor"));
        bail_error(err);
        goto bail;
    }

    /* check remote AS */
    err = mdb_get_node_value_tstr(commit, new_db, ts_str(node_name),
                                  0, NULL, &t_remote_as);
    bail_error_null(err, t_remote_as);

    err = md_bgp_validate_as(commit, ts_str(t_remote_as), false);
    if (err) {
        goto bail;
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_remote_as);
    return(err);
}

static int
md_bgp_check_network_mask(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const char *prefix_str = NULL, *mask_str = NULL;
    struct in_addr prefix_addr;
    uint32_t prefix;
    uint32_t mask = 0xffffffff;
    int64_t mask_number;

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
        goto bail;
    }

    prefix_str = tstr_array_get_str_quick(node_name_parts, 6);
    bail_null(prefix_str);

    if (inet_pton(AF_INET, prefix_str, &prefix_addr) <= 0) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
              _("invalid prefix"));
        bail_error(err);
        goto bail;
    }

    mask_str = tstr_array_get_str_quick(node_name_parts, 7);
    bail_null(mask_str);

    if (mask_str[0] == '0' && mask_str[1] != 0) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
                _("Mask cannot have leading 0"));
        bail_error(err);
        goto bail;
    }

    mask_number = md_bgp_str_to_number(mask_str);
    if (mask_number < 0 || mask_number > 32) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
                _("Invalid mask"));
        bail_error(err);
        goto bail;
    }

    if (mask_number == 0) {
        mask = 0;
    } else {
        mask = mask << (32 - mask_number);
    }

    prefix = ntohl(prefix_addr.s_addr);

    if (prefix != (prefix & mask)) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
                _("Prefix doesn't match mask length"));
        bail_error(err);
        goto bail;
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    return(err);
}

static int
md_bgp_check_router_id(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    struct in_addr router_id_addr;
    tstring *t_router_id = NULL;

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
        goto bail;
    }

    /* check router ID */
    err = mdb_get_node_value_tstr(commit, new_db, ts_str(node_name),
                                  0, NULL, &t_router_id);
    bail_error_null(err, t_router_id);

    if (strcmp(ts_str(t_router_id), "false") != 0 &&
        strcmp(ts_str(t_router_id), "init") != 0 &&
        inet_pton(AF_INET, ts_str(t_router_id), &router_id_addr) <= 0) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
              _("Invalid router ID"));
        bail_error(err);
        goto bail;
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_router_id);
    return(err);
}

/*
 * ret = -1: "no timer" is configured
 * ret = -2: invalid number
 */
int64_t
md_bgp_get_timer_value (md_commit *commit, const char *timer_str)
{
    int64_t timer_value;
    int err = 0;

    if (strcmp(timer_str, "false") == 0 ||
        strcmp(timer_str, "init") == 0) {
        /* "no timer" is configured */
        return -1;
    }

    if (timer_str[0] == '0' && timer_str[1] != 0) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
                _("timer string cannot have leading 0"));
        bail_error(err);
        goto bail;
    }

    timer_value = md_bgp_str_to_number(timer_str);
    if (timer_value < 0 || timer_value > MAX_TIMER_VALUE) {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
                _("Invalid timer value"));
        bail_error(err);
        goto bail;
    }

    return timer_value;

bail:
   return -2;
}

/* given local_as, get timer_ka and timer_hold string value */
static int
md_bgp_get_timers(md_commit *commit, const mdb_db *new_db,
                  const char *local_as, tstring **t_timer_ka_p,
                  tstring **t_timer_hold_p)
{
    int err = 0;
    node_name_t timer_ka_nd = {0}, timer_hold_nd = {0};

    snprintf(timer_ka_nd, sizeof(timer_ka_nd),
             "/nkn/bgp/config/local_as/%s/timer_ka", local_as);
    snprintf(timer_hold_nd, sizeof(timer_hold_nd),
             "/nkn/bgp/config/local_as/%s/timer_hold", local_as);

    err = mdb_get_node_value_tstr(commit, new_db, timer_hold_nd,
                                  0, NULL, t_timer_hold_p);
    bail_error_null(err, *t_timer_hold_p);

    err = mdb_get_node_value_tstr(commit, new_db, timer_ka_nd,
                                  0, NULL, t_timer_ka_p);
    bail_error_null(err, *t_timer_ka_p);

bail:
    return(err);
}

static int
md_bgp_check_timers(md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      const mdb_db_change_array *change_list,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const char *local_as = NULL;
    tstring *t_timer_ka = NULL, *t_timer_hold = NULL;
    int64_t timer_hold_value = 0, timer_ka_value = 0;

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
        goto bail;
    }

    local_as = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(local_as);

    err = md_bgp_get_timers(commit, new_db, local_as, &t_timer_ka, &t_timer_hold);
    bail_error_null(err, t_timer_ka);
    bail_null(t_timer_hold);

    timer_hold_value = md_bgp_get_timer_value(commit, ts_str(t_timer_hold));
    timer_ka_value = md_bgp_get_timer_value(commit, ts_str(t_timer_ka));

    if (timer_hold_value < -1 || timer_ka_value < -1) {
        goto bail;
    }

    if (timer_ka_value == -1 && timer_hold_value == -1) {
        /* both timers are false when "no timer" is issued */
        err = md_commit_set_return_status(commit, 0);
        bail_error(err);
        goto bail;
    }

    if (timer_ka_value >= 0 && timer_hold_value >= 0) {

        if (timer_hold_value < timer_ka_value * 3) {
            err = md_commit_set_return_status_msg_fmt(commit, 1,
                  _("Hold time must be at least 3 times keep-alive interval"));
            bail_error(err);
            goto bail;
        }
    } else {
        err = md_commit_set_return_status_msg_fmt(commit, 1,
              _("Invalid timer config"));
        bail_error(err);
        goto bail;
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_timer_hold);
    ts_free(&t_timer_ka);

    return(err);
}

static int
md_bgp_commit_side_effects (md_commit *commit,
                            const mdb_db *old_db, mdb_db *new_db,
                            mdb_db_change_array *change_list, void *arg)
{
    tbool initial = false;
    int     err = 0;
    uint32  num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    const char *local_as = NULL, *prefix_str = NULL, *mask_str = NULL;
    node_name_t network_nd = {0};
    int num_bindings = 0;
    bn_binding_array *binding_arrays = NULL;

    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);

    /* if mgmtd just restarted */
    if (initial) {
        return 0;
    }

    num_changes = mdb_db_change_array_length_quick(change_list);

    /* go through each of the new BGP configurations */
    for(i = 0; i < num_changes; i++) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (ts_has_prefix_str(change->mdc_name, "/nkn/bgp/config", false)==0) {
            continue;
        }

        local_as = tstr_array_get_str_quick(change->mdc_name_parts, 4);
        bail_null(local_as);

       if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,4,
                                                   4, "*", "network", "*", "*")) {

            prefix_str = tstr_array_get_str_quick(change->mdc_name_parts, 6);
            bail_null(prefix_str);

            mask_str = tstr_array_get_str_quick(change->mdc_name_parts, 7);
            bail_null(mask_str);

            if (mdct_delete == change->mdc_change_type) {
                /* delete parent network node if no children under it */
                snprintf(network_nd, sizeof(network_nd),
                        "/nkn/bgp/config/local_as/%s/network/%s",
                        local_as, prefix_str);

                err = mdb_iterate_binding(commit, new_db, network_nd,
                                mdqf_sel_iterate_subtree, &binding_arrays);
                bail_error(err);

                num_bindings = bn_binding_array_length_quick(binding_arrays);
                if (num_bindings == 0) {
                    err = mdb_set_node_str(commit, (mdb_db *)new_db,
                                           bsso_delete,
                                           0, bt_string, "", "%s", network_nd);
                    bail_error(err);
                }

                bn_binding_array_free(&binding_arrays);
            }
        }
    }

bail:
    return(err);
}

static int
md_bgp_commit_apply (md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      mdb_db_change_array *change_list, void *arg)
{
    tbool initial = false;
    int     err = 0;
    uint32  num_changes = 0, i = 0;
    mdb_db_change *change = NULL;

    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);

    /* if mgmtd just restarted */
    if (initial) {
        err = md_bgp_config_files_restart(commit, new_db);
        bail_error(err);
        goto bail;
    }

    num_changes = mdb_db_change_array_length_quick(change_list);
    lc_log_basic(LOG_DEBUG,
                 "%s: num_changes = %d", __FUNCTION__, num_changes);

    /* check if vnet host-only node has changes */
    for(i = 0; i < num_changes; i++) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (ts_has_prefix_str(change->mdc_name, "/virt/", false) &&
            (ts_equal_str(change->mdc_name, vnet_prefix_node, false) ||
                ts_equal_str(change->mdc_name, vnet_mask_node, false))) {

            err = md_bgp_config_files_restart(commit, new_db);
            bail_error(err);
            goto bail;
        }
    }

    err = md_bgp_config_bgpd(commit, new_db, change_list);
    bail_error(err);

bail:
    return(err);
}

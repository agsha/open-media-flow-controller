/*
 *
 * Filename:  md_watch_dog.c
 * Date:      2011/07/06
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2011 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */


#include <ctype.h>
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_defs.h"
#include "watchdog_defs.h"

#define wd "/nkn/watch_dog/config/nvsd/alarms/"

#define MAX_WD_MOD_NDS   sizeof(md_wd_mod_nds)/sizeof( md_wd_mod_nds_t)
#define MAX_WD_DEL_NDS   sizeof(md_wd_delete_nds)/sizeof(md_wd_delete_nds_t)


#define PROBE_DELETE_CHECK(ver, name)       \
{ver, "/nkn/watch_dog/config/nvsd/alarms/"name"/enable"}, \
{ver, "/nkn/watch_dog/config/nvsd/alarms/"name"/threshold"}, \
{ver, "/nkn/watch_dog/config/nvsd/alarms/"name"/poll_freq"}, \
{ver, "/nkn/watch_dog/config/nvsd/alarms/"name"/action"}, \


#define PROBE_MODIFY_CHECK(ver,name, attribute, data_type, value)       \
{ ver, {{"/nkn/watch_dog/config/nvsd/alarms/"name"/"attribute"", bt_##data_type, #value}}},


#define PROBE_CHECK(name, func_name, threshold, freq, action, enabled, gdb_str)        \
    err = mdm_new_node(module, &node);  \
bail_error(err);  \
node->mrn_name              = "/nkn/watch_dog/config/nvsd/alarms/"name"/enable"; \
node->mrn_value_type        = bt_bool; \
node->mrn_initial_value     = #enabled;  \
node->mrn_node_reg_flags    = mrf_flags_reg_config_literal; \
node->mrn_cap_mask          = mcf_cap_node_restricted; \
node->mrn_description       = ""; \
err = mdm_add_node(module, &node); \
bail_error(err); \
\
err = mdm_new_node(module, &node); \
bail_error(err); \
node->mrn_name              = "/nkn/watch_dog/config/nvsd/alarms/"name"/threshold"; \
node->mrn_value_type        = bt_uint32; \
node->mrn_initial_value     = #threshold; \
node->mrn_node_reg_flags    = mrf_flags_reg_config_literal; \
node->mrn_cap_mask          = mcf_cap_node_restricted; \
node->mrn_description       = ""; \
err = mdm_add_node(module, &node); \
bail_error(err); \
\
err = mdm_new_node(module, &node); \
bail_error(err); \
node->mrn_name              = "/nkn/watch_dog/config/nvsd/alarms/"name"/poll_freq"; \
node->mrn_value_type        = bt_uint32; \
node->mrn_initial_value     = #freq; \
node->mrn_node_reg_flags    = mrf_flags_reg_config_literal; \
node->mrn_cap_mask          = mcf_cap_node_restricted; \
node->mrn_description       = ""; \
err = mdm_add_node(module, &node); \
bail_error(err); \
\
err = mdm_new_node(module, &node); \
bail_error(err); \
node->mrn_name               = "/nkn/watch_dog/config/nvsd/alarms/"name"/action"; \
node->mrn_value_type        = bt_uint64; \
node->mrn_initial_value_uint64    = action; \
node->mrn_node_reg_flags    = mrf_flags_reg_config_literal; \
node->mrn_cap_mask          = mcf_cap_node_restricted; \
node->mrn_description       = ""; \
err = mdm_add_node(module, &node); \
bail_error(err); \
\
MD_NEW_RULE(ra, md_ver -1, md_ver); \
rule->mur_upgrade_type =   MUTT_ADD; \
rule->mur_name =           "/nkn/watch_dog/config/nvsd/alarms/"name"/enable"; \
rule->mur_new_value_type = bt_bool; \
rule->mur_new_value_str =  #enabled; \
MD_ADD_RULE(ra); \
\
MD_NEW_RULE(ra, md_ver -1, md_ver); \
rule->mur_upgrade_type =   MUTT_ADD; \
rule->mur_name =           "/nkn/watch_dog/config/nvsd/alarms/"name"/threshold"; \
rule->mur_new_value_type = bt_uint32; \
rule->mur_new_value_str =  #threshold; \
MD_ADD_RULE(ra); \
\
MD_NEW_RULE(ra, md_ver -1, md_ver); \
rule->mur_upgrade_type =   MUTT_ADD; \
rule->mur_name =           "/nkn/watch_dog/config/nvsd/alarms/"name"/poll_freq"; \
rule->mur_new_value_type = bt_uint32; \
rule->mur_new_value_str =  #freq; \
MD_ADD_RULE(ra); \
\
MD_NEW_RULE(ra, md_ver -1, md_ver); \
rule->mur_upgrade_type =   MUTT_ADD; \
rule->mur_name =           "/nkn/watch_dog/config/nvsd/alarms/"name"/action"; \
rule->mur_new_value_type = bt_uint64; \
rule->mur_new_value_uint64 =  action; \
MD_ADD_RULE(ra);

typedef struct md_wd_mod_nds_st {
    int ver;
    bn_str_value nd_vals[1];
} md_wd_mod_nds_t;

#if 1
const md_wd_mod_nds_t md_wd_mod_nds[ ] = {
#define PROBE_MODIFY_CHECKS
#include "watchdog.inc"
#undef PROBE_MODIFY_CHECKS
    { 0, {{NULL, 0, 0}}},
};

typedef struct md_wd_delete_nds_st {
    int ver;
    const char *nd_name;
}md_wd_delete_nds_t;

const md_wd_delete_nds_t md_wd_delete_nds[] = {
#define PROBE_DELETE_CHECKS
#include "watchdog.inc"
#undef PROBE_DELETE_CHECKS
    {0, NULL},
};

#endif
/*=======================================================================================================
 *
 *Public declarations
 *
 *=====================================================================================================*/


int md_watchdog_init(const lc_dso_info *info, void *data);


/*=======================================================================================================
 *
 *Local declarations
 *
 *=====================================================================================================*/
static int
md_wathcdog_commit_side_effects(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg);

static int
md_watchdog_add_initial(
	md_commit *commit,
	mdb_db    *db,
	void *arg);

static int
md_watchdog_add_nodes(
	md_commit *commit,
	mdb_db    *db, int from, int to);

static int
md_watchdog_delete_check_nodes(
	md_commit *commit,
	mdb_db    *db, int from , int to);

static md_upgrade_rule_array *md_watchdog_ug_rules = NULL;


static int
md_watchdog_upgrade_downgrade(const mdb_db *old_db,
	mdb_db *inout_new_db,
	uint32 from_module_version,
	uint32 to_module_version, void *arg);

/*=======================================================================================================
 *
 *Public definitions
 *
 *=====================================================================================================*/
#define MD_WD_VER(ver)    md_ver = ver;

static int md_ver = 0;

    int
md_watchdog_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
#define PROBE_VERSION
#include "watchdog.inc"
#undef PROBE_VERSION

    err = mdm_add_module("watchdog", md_ver,
	    "/nkn/watch_dog", NULL,
	    modrf_all_changes,//IS this falg needed?
	    NULL, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    0, 0,
	    md_watchdog_upgrade_downgrade,
	    &md_watchdog_ug_rules,
	    md_watchdog_add_initial, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_watchdog_ug_rules);
    bail_error(err);
    ra = md_watchdog_ug_rules;

    /*TODO:Reparent this node */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/watch_dog/config/nvsd/restart";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/watch_dog/config/debug/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/watch_dog/config/debug/gdb_ena";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/watch_dog/config/nvsd/mem/softlimit";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/watch_dog/config/nvsd/mem/hardlimit";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/watch_dog/config/nvsd/mem/softlimit";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/watch_dog/config/nvsd/mem/hardlimit";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);


#define PROBE_CHECKS
#include "watchdog.inc"
#undef PROBE_CHECKS


bail:
    return(err);
}

/*=======================================================================================================
 *
 *Local definitions
 *
 *=====================================================================================================*/
static int
md_watchdog_add_initial(
	md_commit *commit,
	mdb_db    *db,
	void *arg)
{
    int err = 0;


bail:
    return err;

}


/* Add check function nodes on a manufacture and upgrade scenario
 * Maufacture would create all nodes(Typicall no nodes will exist),
 * so the if(!check_func_found) will be true for all iterations and
 * nodes will be created.
 * for upgrade we check if the parent node is existing,if so
 * dont create the child nodes
 */
static int
md_watchdog_add_nodes(
	md_commit *commit,
	mdb_db    *db, int from,
	int to)
{
    int err = 0;
    uint32_t i = 0;


    for(i = 0; i < MAX_WD_MOD_NDS ; i ++) {

	if(md_wd_mod_nds[i].ver == to && md_wd_mod_nds[i].nd_vals[0].bv_name) {

	    err = mdb_create_node_bindings(commit, db,
		    md_wd_mod_nds[i].nd_vals,
		    1);
	    bail_error(err);

	}
    }
bail:
    return err;


}

static int
md_watchdog_delete_check_nodes(
	md_commit *commit,
	mdb_db    *db, int from,
	int to)
{
    int err = 0;
    uint32_t i = 0;
    bn_binding *binding = NULL;

    for(i = 0; i < MAX_WD_DEL_NDS ; i++) {

	if(md_wd_delete_nds[i].ver == to && md_wd_delete_nds[i].nd_name) {

	    err = mdb_get_node_binding(commit, db, md_wd_delete_nds[i].nd_name,
		    0, &binding);
	    bail_error(err);

	    if(binding) {
		bn_binding_free(&binding);

		err = bn_binding_new_named(&binding, md_wd_delete_nds[i].nd_name);
		bail_error(err);

		err = mdb_set_node_binding(commit, db, bsso_delete, 0,
			binding);
		bail_error(err);
		bn_binding_free(&binding);
	    }
	}
    }
bail:
    bn_binding_free(&binding);
    return err;

}

    static int
md_watchdog_upgrade_downgrade(const mdb_db *old_db,
	mdb_db *inout_new_db,
	uint32 from_module_version,
	uint32 to_module_version, void *arg)
{
    int err = 0;

    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);

    err = md_watchdog_add_nodes(NULL,
	    inout_new_db, from_module_version,
	    to_module_version);
    bail_error(err);



    /*Handle delete of alarms */
    /* Look at the diff of new and old db
     * determine the deleted alarms
     * remove them
     */
    err = md_watchdog_delete_check_nodes(
	    NULL,
	    inout_new_db, from_module_version,
	    to_module_version);
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */

/*
 *
 * Filename:  md_nkn_monitor.c
 * Date:      2008/11/17b
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "nkn_mgmt_defs.h"
#include <sys/mount.h>
/* ------------------------------------------------------------------------- */

static md_upgrade_rule_array *md_nkn_monitor_ug_rules = NULL;

int md_nkn_monitor_init(const lc_dso_info *info, void *data);
static int md_nkn_monitor_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int
md_get_clusterinstance(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);

static int
md_get_temp_values(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);

static int
md_get_node(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);

static int
md_get_node_uint32(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);
/* ------------------------------------------------------------------------- */
int
md_nkn_monitor_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("juniper-monitor", 1,
                         "/nkn/monitor", NULL,
                         0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
                         &md_nkn_monitor_ug_rules,
                         md_nkn_monitor_add_initial, NULL,
                         NULL, NULL, NULL, NULL,
                         &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/diskname";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/serial";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/state";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/type";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/model";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/blocksize";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/totalsize";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/usedblock";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/freespace";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/capacity";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/util";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/iorate";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/read";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/write";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/ioerr";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/ioclr";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/spaceerr";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/spaceclr";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/diskbw";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/bwerr";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/disk/*/bwclr";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sas/disk/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sata/disk/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/ssd/disk/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sas/disk/*/freespace";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sata/disk/*/freespace";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/ssd/disk/*/freespace";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sas/disk/*/read";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sas/disk/*/write";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/ssd/disk/*/read";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/ssd/disk/*/write";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sata/disk/*/read";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/sata/disk/*/write";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/root/disk/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/root/disk/*/write";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/root/disk/*/read";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/root/disk/*/freespace";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*LF deamon monitoring node */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/lfd/appmaxutil";
    node->mrn_value_type 	= bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description 	= "Application max utilisation for nvsd given by LFD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/lfd/maxthroughput";
    node->mrn_value_type        = bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description       = "Max throughput possible in the MFC device";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/lfd/maxopenconnections";
    node->mrn_value_type        = bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description       = "Max no. of open connections possible in the MFC device";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/lfd/sasdiskutil";
    node->mrn_value_type        = bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description       = "Percentage of SAS disk utilization";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/lfd/satadiskutil";
    node->mrn_value_type        = bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description       = "Percentage of SATA disk utilization";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/lfd/ssddiskutil";
    node->mrn_value_type        = bt_uint64;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_gmgmthd;
    node->mrn_description       = "Percentage of SSD disk utilization";
    err = mdm_add_node(module, &node);
    bail_error(err);

/* The next 3 are dummy nodes registered for the purpose of hidden scalars
used for OriginUp/Down SNMP Notification */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/cluster-instance";
    node->mrn_value_type        = bt_int32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_clusterinstance;
    node->mrn_description       = "Cluster-instance";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/nodename";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "Node name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/cluster-namespace";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "Namespace associated with cluster";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/mirror-complete";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "Root disk re-mirroring done message";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/mirror-broken-error";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "Root disk mirror broken error message";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/jbod-shelf-error";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "JBODShelf unreachable error message";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/temp-curr";
    node->mrn_value_type        = bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node_uint32;
    node->mrn_description       = "Current temperature value";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/temp-thresh";
    node->mrn_value_type        = bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node_uint32;
    node->mrn_description       = "Hard-coded temperature threshold value";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/temp-sensor";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "Sensor name that triggered the event";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/monitor/filename";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_node;
    node->mrn_description       = "Accesslog filename that failed upload";
    err = mdm_add_node(module, &node);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
const bn_str_value md_nkn_monitor_initial_values[] = {
};


/* ------------------------------------------------------------------------- */
static int
md_nkn_monitor_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_nkn_monitor_initial_values,
         sizeof(md_nkn_monitor_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    bn_binding_free(&binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* It returns a dummy value */
static int
md_get_clusterinstance(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    err = bn_binding_new_int32(ret_binding, node_name, ba_value,
                             0, 1);
    bail_error_null(err, *ret_binding);

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
/* It returns a dummy value */
static int
md_get_temp_values(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    err = bn_binding_new_uint32(ret_binding, node_name, ba_value,
                             0, 1);
    bail_error_null(err, *ret_binding);

bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/*It returns a dummy value */
static int
md_get_node(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
	    0, "dummy");
    bail_error_null(err, *ret_binding);

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_get_node_uint32(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    err = bn_binding_new_uint32(ret_binding, node_name, ba_value,
                             0, 1);
    bail_error_null(err, *ret_binding);

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */

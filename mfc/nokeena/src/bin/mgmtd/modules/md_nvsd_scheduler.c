/*
 *
 * Filename:  md_nvsd_scheduler.c
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



/* ------------------------------------------------------------------------- */

int md_nvsd_scheduler_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_scheduler_ug_rules = NULL;


/* ------------------------------------------------------------------------- */
int
md_nvsd_scheduler_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-scheduler", 1,
                         "/nkn/nvsd/scheduler", "/nkn/nvsd/scheduler/config",
			 0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_nvsd_scheduler_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

 bail:
    return(err);
}


/*
 *
 * Filename:  md_nkn.c
 * Date:      2008/10/21
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

int md_nkn_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nkn_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
int
md_nkn_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nokeena", 1,
                         "/nkn", NULL,
			 modrf_all_changes,
                         NULL, NULL,
                         md_nkn_commit_check, NULL,
                         md_nkn_commit_apply, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade, &md_nkn_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL, &module);
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    /* Init the various NKN modules */
    md_nkn_http_init (module);

 bail:
    return(err);
}


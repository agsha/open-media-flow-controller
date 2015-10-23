/*
 * @file md_jpsd_tdf.c
 * @brief
 * md_jpsd_tdf.c - definations for jpsd-tdf mgmt functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 */

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

#define MAX_TDF_DOMAIN_STR (63)
#define MAX_TDF_VRF_STR (128)
#define MAX_TDF_DOMAIN	(16000)

static md_upgrade_rule_array *md_jpsd_tdf_ug_rules = NULL;

int md_jpsd_tdf_init(const lc_dso_info *info, void *data);

const bn_str_value md_jpsd_tdf_initial_values[] = {
};

static int md_jpsd_tdf_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
	int err = 0;
	bn_binding *binding = NULL;

	err = mdb_create_node_bindings (commit, db,
			md_jpsd_tdf_initial_values,
			sizeof(md_jpsd_tdf_initial_values)/sizeof(bn_str_value));
	bail_error(err);

bail:
	bn_binding_free(&binding);
	return(err);
}

static int md_jpsd_tdf_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg)
{
	return 0;
}

static int md_jpsd_tdf_commit_side_effects(
		md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		mdb_db_change_array *change_list,
		void *arg)
{
	return 0;
}

int md_jpsd_tdf_init(const lc_dso_info *info, void *data)
{
	int err = 0;
	md_module *module = NULL;
	md_reg_node *node = NULL;
	md_upgrade_rule *rule = NULL;
	md_upgrade_rule_array *ra = NULL;

	/* Creating the Nokeena specific module */
	err = mdm_add_module("jpsd", 1,
		"/nkn/jpsd", "/nkn/jpsd/tdf",
		0,
		NULL, NULL,
		NULL, NULL,
		NULL, NULL,
		0, 0,
		md_generic_upgrade_downgrade,
		&md_jpsd_tdf_ug_rules,
		NULL, NULL,
		NULL, NULL,
		NULL, NULL,
		&module);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain/*";
	node->mrn_value_type = bt_string;
	node->mrn_limit_str_min_chars = 1;
	node->mrn_limit_str_max_chars = MAX_TDF_DOMAIN_STR;
	//node->mrn_limit_str_no_charlist = TDF_DOMAIN_ALLOWED_CHARS;
	//node->mrn_limit_wc_count_max = MAX_TDF_DOMAIN;
	node->mrn_bad_value_msg = "Error creating domain";
	node->mrn_node_reg_flags = mrf_flags_reg_config_wildcard;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain/*/active";
	node->mrn_value_type = bt_string;
	node->mrn_initial_value = "No";
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Status";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain/*/rule";
	node->mrn_value_type = bt_string;
	node->mrn_initial_value = "-";
	node->mrn_limit_str_max_chars = MAX_TDF_DOMAIN_STR;
	//node->mrn_limit_wc_count_max = MAX_TDF_DOMAIN;
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Rule Name";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain-rule/*";
	node->mrn_value_type = bt_string;
	node->mrn_limit_str_min_chars = 1;
	node->mrn_limit_str_max_chars = MAX_TDF_DOMAIN_STR;
	//node->mrn_limit_str_no_charlist = TDF_DOMAIN_ALLOWED_CHARS;
	//node->mrn_limit_wc_count_max = MAX_TDF_DOMAIN;
	node->mrn_bad_value_msg = "Error creating domain-rule";
	node->mrn_node_reg_flags = mrf_flags_reg_config_wildcard;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Rule";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain-rule/*/precedence";
	node->mrn_value_type = bt_uint32;
	node->mrn_initial_value = "0";
	node->mrn_limit_num_min_int = 0;
	node->mrn_limit_num_max_int = 4294967295;
	node->mrn_bad_value_msg = "Error creating precedence";
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Rule Precedence";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain-rule/*/uplink-vrf";
	node->mrn_value_type = bt_string;
	node->mrn_initial_value = "-";
	node->mrn_limit_str_max_chars = MAX_TDF_VRF_STR;
	//node->mrn_limit_str_no_charlist = TDF_DOMAIN_ALLOWED_CHARS;
	node->mrn_bad_value_msg = "Error creating uplink-vrf";
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Rule Uplink-Vrf";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain-rule/*/downlink-vrf";
	node->mrn_value_type = bt_string;
	node->mrn_initial_value = "-";
	node->mrn_limit_str_max_chars = MAX_TDF_VRF_STR;
	//node->mrn_limit_str_no_charlist = TDF_DOMAIN_ALLOWED_CHARS;
	node->mrn_bad_value_msg = "Error creating downink-vrf";
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Rule Downlink-Vrf";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = "/nkn/jpsd/tdf/domain-rule/*/refcount";
	node->mrn_value_type = bt_uint32;
	node->mrn_initial_value = "0";
	node->mrn_limit_num_min_int = 0;
	node->mrn_limit_num_max_int = 1024;
	node->mrn_bad_value_msg = "Error creating refcount";
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_restricted;
	node->mrn_description = "Tdf Domain Refcount";
	err = mdm_add_node(module, &node);
	bail_error(err);

bail:
	return (err);
}

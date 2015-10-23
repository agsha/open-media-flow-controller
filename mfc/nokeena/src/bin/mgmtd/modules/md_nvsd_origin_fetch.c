/*
 *
 * Filename:    md_nvsd_origin_fetch.c
 * Date:        2009/01/11
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include <ctype.h>
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#include "jmd_common.h"


#define NKN_MAX_HOST_HEADER_STR 128

static md_upgrade_rule_array *md_nvsd_origin_fetch_ug_rules = NULL;

int md_nvsd_origin_fetch_init(
        const lc_dso_info *info,
        void *data);

static int md_nvsd_origin_fetch_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_origin_fetch_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_nvsd_origin_fetch_upgrade_downgrade(const mdb_db *old_db,
					mdb_db *inout_new_db,
					uint32 from_module_version,
					uint32 to_module_version,
					void *arg);

int
md_nvsd_origin_fetch_init(
        const lc_dso_info *info,
        void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("nvsd-origin-fetch", 41,
	    "/nkn/nvsd/origin_fetch/", "/nkn/nvsd/origin_fetch/config",
	    modrf_all_changes,
	    md_nvsd_origin_fetch_commit_side_effects, NULL,
	    md_nvsd_origin_fetch_commit_check, NULL,
	    NULL, NULL,
	    0, 0,
	    md_nvsd_origin_fetch_upgrade_downgrade,
	    &md_nvsd_origin_fetch_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*";
    node->mrn_value_type =          bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_NAMESPACE_LENGTH;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?";
    node->mrn_description =         "Namespace configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-fill";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "2";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache-fill";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-directive/no-cache";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "follow";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache directive";
    err = mdm_add_node(module, &node);
    bail_error(err);

    // BZ 2375
    // Changed "/nkn/nvsd/origin_fetch/config/*/cache-age" to "/nkn/nvsd/origin_fetch/config/*/cache-age/default"
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-age/default";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   94672800; // 3 years
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache age default. If data received from origin has no cache-age at all";
    err = mdm_add_node(module, &node);
    bail_error(err);

    // BZ 2375
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type/*";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_limit_wc_count_max =  5;
    node->mrn_bad_value_msg = 	    "error: max of 5 Content-Type TTLs allowed (inclusive of content-type-any)";
    node->mrn_description =         "Cache age Per Content-Type";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type/*/type";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache age Per Content-Type - MIME Type for ex. application/qmx";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type/*/value";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   28800;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   94672800; // 3 years
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache age Per Content-Type - Timeout. default 28800 seconds";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-control/custom-header";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Custom Header Value for cache control";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-control/s-maxage-ignore";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Ignre s-maxage header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/partial-content";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Partial content cache";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/date-header/modify";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Modify date header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache revalidation flag";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* BZ-XXXX
     * Add CLI to configure cache-revalidation method and validate-header name
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/method";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "head";
    node->mrn_limit_str_choices_str = ",head,get";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_bad_value_msg =	    "error: only HEAD or GET methods are supported at present";
    node->mrn_description =         "Use a HEAD or GET method for revalidation";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/validate-header";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Use a validator function, for example Etag";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/validate-header/header-name";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "etag";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "A header whose value is used to determine if the content changed or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/use-date-header";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Use Date header when LM is not present in the HTTP headers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/flush-intermediate-cache";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Add a Cache-Control: max-age = 0 in the revalidation request";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/content-store/media/cache-age-threshold";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   60;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   2592000; // 30 days
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache Age threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/content-store/media/uri-depth-threshold";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   10;
    node->mrn_limit_num_min_int =   1;
    node->mrn_limit_num_max_int =   20;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Directory depth of URIs";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =
	"/nkn/nvsd/origin_fetch/config/*/content-store/media/object-size-threshold";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   4096;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Size threshold at which "
	"Origin Manager will start caching "
	"data into disks";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/offline/fetch/size";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   10240;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Offline fetch file-size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/offline/fetch/smooth-flow";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Offline fetch smooth flow option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "set-cookie";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Header name - this mimics a wildcard node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie/value";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "any";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Header Value. default is any - i.e any value";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie/cache";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Can we cache this header?";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie/cache/no-revalidation";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Can we cache this header?";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/convert/head";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Allow/Disallow converting HEAD to GET";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/host-header/inherit";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "HOST: header in the request from MFD to origin be set"
	"with the FQDN or IP address of the HOST: header received "
	"in the incoming URL to  MFD.";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/query/cache/enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Allow object to be cached if query string is present";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/query/strip";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Strip the query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cookie/cache/enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Allow object to be cached if Cookie header string is present";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-directive/no-cache";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "follow";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache directive ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-age/default";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   94672800; // 3 years
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache age default. If data received from origin has no cache-age at all";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/cache-age-threshold";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   60;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   2592000; // 30 days
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Cache Age threshold ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =
	"/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/object-size-threshold";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Size threshold at which "
	"Origin Manager will start caching "
	"data into disks";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/cache-control/max_age";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "set the max-age for cache-control header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/cache-control/action";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "serve-from-origin";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "set the action for cache-control header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/byte-range/preserve";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Set byte-range value";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/byte-range/align";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Set byte-range preserve option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/origin_fetch/config/*/object/revalidate/time_barrier";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"revalidate  bstrtime";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/origin_fetch/config/*/object/revalidate/reval_type";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       " ";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"revalidate  bstrtime";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/ingest-policy";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "1";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Ingest-policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/detect_eod_on_close";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "End of data detection";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/tunnel-all";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Tunnel all requests to origin, for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Cache-Pinning */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =			"/nkn/nvsd/origin_fetch/config/*/object/cache_pin/cache_size";
    node->mrn_value_type = 	  	bt_uint32;
    node->mrn_initial_value_int = 	0;
    node->mrn_node_reg_flags =      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 		mcf_cap_node_basic;
    node->mrn_description = 		"pinned object cache-size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/origin_fetch/config/*/object/cache_pin/max_obj_size";
    node->mrn_value_type =      	bt_uint32;
    node->mrn_initial_value_int = 	0;
    node->mrn_node_reg_flags =      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =  		mcf_cap_node_basic;
    node->mrn_description = 		"pinned object cache-size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =        	        "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/enable";
    node->mrn_value_type =       	bt_bool;
    node->mrn_initial_value =    	"false";
    node->mrn_node_reg_flags =     	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =         	mcf_cap_node_basic;
    node->mrn_description =      	"Cache-Pinning";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =			"/nkn/nvsd/origin_fetch/config/*/object/cache_pin/pin_header_name";
    node->mrn_value_type =      	bt_string;
    node->mrn_initial_value =       	"";
    node->mrn_node_reg_flags =      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =  		mcf_cap_node_basic;
    node->mrn_description = 		"pin-header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =			"/nkn/nvsd/origin_fetch/config/*/object/validity/start_header_name";
    node->mrn_value_type =      	bt_string;
    node->mrn_initial_value =       	"";
    node->mrn_node_reg_flags =   	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description = 		"validity-start-header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =			"/nkn/nvsd/origin_fetch/config/*/object/cache_pin/validity/end_header_name";
    node->mrn_value_type =      	bt_string;
    node->mrn_initial_value =       	"";
    node->mrn_node_reg_flags =      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =  		mcf_cap_node_basic;
    node->mrn_description = 		"validity-stop-header";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =			"/nkn/nvsd/origin_fetch/config/*/redirect_302";
    node->mrn_value_type =      	bt_bool;
    node->mrn_initial_value =       	"false";
    node->mrn_node_reg_flags =      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =  		mcf_cap_node_basic;
    node->mrn_description = 		"False = tunnel 302 response from origin; "
	"true = handle 302 and reach out to new "
	"origin server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* BZ 7243 - Domain name exclusion from MFC cache index */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/origin_fetch/config/*/cache-index/domain-name";
    node->mrn_value_type =	bt_string;
    node->mrn_initial_value =	"include";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =	mcf_cap_node_basic;
    node->mrn_description =	"When CLI is configured, the behavior would be to "
	"exclude the configured origin server domain-name "
	"or the one provided by the incoming request as "
	"part of host header in the cache name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/origin_fetch/config/*/rtsp/cache-index/domain-name";
    node->mrn_value_type =	bt_string;
    node->mrn_initial_value =	"include";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =	mcf_cap_node_basic;
    node->mrn_description =	"When CLI is configured, the behavior would be to "
	"exclude the configured origin server domain-name "
	"or the one provided by the incoming request as "
	"part of host header in the cache name";
    err = mdm_add_node(module, &node);

    bail_error(err);
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/obj-thresh-min";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Minimum object size threshold for Caching";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/obj-thresh-max";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Maximum object size threshold for Caching";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/auth-header";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "MFC will try to cache request containing AUTH header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/cookie-header";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "MFC will try to cache request containing COOKIE header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/query-string";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "MFC will try to cache request URI containing query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/cache-control";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "MFC will try to cache requests containing either or both "
	"cache-control: no-cache , PRAGMA: no-cache";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/tunnel-override/response/cc-notrans";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "MFC will try to cache response with Cache control set to no-transform";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/tunnel-override/response/obj-expired";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "MFC will try to cache response with that indicates expired "
	"objects with new expiry date set to current + cache_age deafult configured";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/object/correlation/etag/ignore";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Etag ignore configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/object/correlation/validatorsall/ignore";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "COD version check  ignore configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-fill/client-driven/aggressive_threshold";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   9;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   100; // 30 days
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "aggressive threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 				= "/nkn/nvsd/origin_fetch/config/*/cache-ingest/hotness-threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int =  3;
    node->mrn_limit_num_min_int =  3;
    node->mrn_limit_num_max_int =  65535;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Cache Ingest : Hotness threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/cache-ingest/byte-range-incapable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "cache ingest byte-range-incapable origin node";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/origin_fetch/config/*/cache-ingest/size-threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Analytics Manager : Size in bytes below which cache ingest should happen";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/method/connect/handle";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Tunnel the connect method";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name
	="/nkn/nvsd/origin_fetch/config/*/cache-index/include-header";
    node->mrn_value_type	    =	    bt_bool;
    node->mrn_initial_value	    =       "false";
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask	   		=	mcf_cap_node_basic;
    node->mrn_description		=   "Whether Header names to be included in the cache index or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name
	="/nkn/nvsd/origin_fetch/config/*/cache-index/include-object";
    node->mrn_value_type	    =	    bt_bool;
    node->mrn_initial_value	    =       "false";
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask	   		=	mcf_cap_node_basic;
    node->mrn_description		=   "Whether Object info to be included in the cache index or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name	    ="/nkn/nvsd/origin_fetch/config/*/cache-index/include-headers/*";
    node->mrn_value_type	=	bt_uint32;
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask	    =	mcf_cap_node_basic;
    node->mrn_limit_wc_count_max	=   8;
    node->mrn_bad_value_msg	    =	"Only 8 Header names are allowed";
    node->mrn_description		=   "No.of Header names to be included in the cache index";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name
	="/nkn/nvsd/origin_fetch/config/*/cache-index/include-headers/*/header-name";
    node->mrn_value_type	    =	    bt_string;
    node->mrn_initial_value	    =       "";
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask	   		=	mcf_cap_node_basic;
    node->mrn_description		=   "Header names to be included in the cache index";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name	    ="/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-bytes";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  32768;
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask	   		=	mcf_cap_node_basic;
    node->mrn_description		=   "No.of bytes to be included for checksum calculation to use in the cache-index";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-offset";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 1073741823;
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_description	= "Offset of the object for checksum calculation to use in the cache-index";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/from-end";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value	= "false";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_description	= "Offset is from the end of the object";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* http secure option */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/secure";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "HTTPs support enable";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* http plain-text option */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/plain-text";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                    "/nkn/nvsd/origin_fetch/config/*/host-header/header-value";
    node->mrn_limit_str_max_chars =     NKN_MAX_HOST_HEADER_STR;
    node->mrn_value_type =              bt_string;
    node->mrn_initial_value =           "";
    node->mrn_node_reg_flags =          mrf_flags_reg_config_literal;
    node->mrn_cap_mask =                mcf_cap_node_basic;
    node->mrn_check_node_func   =       md_rfc952_domain_validate;
    node->mrn_description =             "host-header-value";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =			"/nkn/nvsd/origin_fetch/config/*/object/compression/enable";
    node->mrn_value_type =              bt_bool;
    node->mrn_initial_value =           "false";
    node->mrn_node_reg_flags =          mrf_flags_reg_config_literal;
    node->mrn_cap_mask =                mcf_cap_node_basic;
    node->mrn_description =             "compression-status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/origin_fetch/config/*/object/compression/obj_range_min";
    node->mrn_value_type =          bt_uint64;
    node->mrn_initial_value =       "128";
    node->mrn_limit_num_min_int =   128;
    node->mrn_limit_num_max_int =   10485760; // 10MB
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Min object size for compression";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/origin_fetch/config/*/object/compression/obj_range_max";
    node->mrn_value_type =          bt_uint64;
    node->mrn_initial_value =       "10485760";
    node->mrn_limit_num_min_int =   128;
    node->mrn_limit_num_max_int =   10485760; // 10MB
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Max object size for compression";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/origin_fetch/config/*/object/compression/algorithm";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "gzip";
    node->mrn_limit_str_choices_str = ",gzip,deflate";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Type of compression algorithm";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/origin_fetch/config/*/object/compression/file_types/*";
    node->mrn_value_type =          bt_string;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Type of files to compress";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/origin_fetch/config/*/cache/response/*";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "cache response code";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/origin_fetch/config/*/cache/response/*/age";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "cache age for response code";
    node->mrn_initial_value =       "180";
    node->mrn_limit_num_min_int =   60;
    node->mrn_limit_num_max_int =   28800; // in secs
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/origin_fetch/config/*/header/vary";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Enable/Disable setting of vary-header when a request is made to origin server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                      =       "/nkn/nvsd/origin_fetch/config/*/vary-header/pc";
    node->mrn_value_type                =       bt_string;
    node->mrn_initial_value             =       "";
    node->mrn_node_reg_flags            =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                  =       mcf_cap_node_basic;
    node->mrn_description               =       "specify regex to match user agents from PC based browsers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                      =       "/nkn/nvsd/origin_fetch/config/*/vary-header/tablet";
    node->mrn_value_type                =       bt_string;
    node->mrn_initial_value             =       "";
    node->mrn_node_reg_flags            =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                  =       mcf_cap_node_basic;
    node->mrn_description               =       "specify regex to match user agents from Tablet based browsers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                      =       "/nkn/nvsd/origin_fetch/config/*/vary-header/phone";
    node->mrn_value_type                =       bt_string;
    node->mrn_initial_value             =       "";
    node->mrn_node_reg_flags            =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                  =       mcf_cap_node_basic;
    node->mrn_description               =       "specify regex to match user agents from phone based browsers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/client-header-inherit";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value	= "false";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_description	= "Inherit client headers for revalidation";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/origin_fetch/config/*/fetch-page";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value	= "false";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_description	= "Enable/Disable to user higher number of pages for fetching objects";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/origin_fetch/config/*/fetch-page-count";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value	= "1";
    node->mrn_limit_num_min_int =   1;
    node->mrn_limit_num_max_int =   4; // number of pages
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_description	= "Number of pages to be used for fetching objects";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* ------------------------------------------------------------------------
     *                      UPGRADE PROCESSING NODES
     * ------------------------------------------------------------------------
     */
    err = md_upgrade_rule_array_new(&md_nvsd_origin_fetch_ug_rules);
    bail_error(err);

    ra = md_nvsd_origin_fetch_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/content-store/media/object-size-threshold";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie";
    rule->mur_new_value_str =       "set_cookie";
    rule->mur_new_value_type =      bt_string;
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie/cache";
    rule->mur_new_value_str =       "true";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/convert/head";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type-any";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    /* Removing the above node as we are using content-type/type with ""
     * for this case */
    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =        MUTT_DELETE;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type-any";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type = 		MUTT_CLAMP;
    rule->mur_name = 			"/nkn/nvsd/origin_fetch/config/*/cache-age/default";
    rule->mur_lowerbound = 		0;
    rule->mur_upperbound = 		94672800; // 3 years
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type = 		MUTT_CLAMP;
    rule->mur_name = 			"/nkn/nvsd/origin_fetch/config/*/cache-age/content-type/*/value";
    rule->mur_lowerbound = 		0;
    rule->mur_upperbound = 		94672800; // 3 years
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/host-header/inherit";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/query/cache/enable";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/query/strip";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type = 		MUTT_CLAMP;
    rule->mur_name = 			"/nkn/nvsd/origin_fetch/config/*/content-store/media/cache-age-threshold";
    rule->mur_lowerbound = 		1;
    rule->mur_upperbound = 		2592000; // 30 days
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/use-date-header";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cookie/cache/enable";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/ingest-policy";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "1";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/detect_eod_on_close";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    /* Bug 5843 - show run displays the internal error for 1.1.0 to 2.0.3 upgrade */
    /* Aligned the nodes with baffing ,appalachian, chirripo */
    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/object-size-threshold";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-directive/no-cache";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "follow";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-age/default";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "28800";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/cache-age-threshold";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "60";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-fill";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "1";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/byte-range/align";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/byte-range/preserve";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =    	    "/nkn/nvsd/origin_fetch/config/*/object/revalidate/time_barrier";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =    	    "/nkn/nvsd/origin_fetch/config/*/object/revalidate/reval_type";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 18, 19);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/cache-control/max_age";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 18, 19);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/cache-control/action";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "serve-from-origin";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/client-request/tunnel-all";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/validity/start_header_name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/validity/end_header_name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-control/custom-header";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =	    MUTT_ADD;
    rule->mur_name =		    "/nkn/nvsd/origin_fetch/config/*/redirect_302";
    rule->mur_new_value_type = 	    bt_bool;
    rule->mur_new_value_str =	    "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/header/set-cookie/cache";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_modify_cond_value_str = "true";
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/flush-intermediate-cache";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-index/domain-name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "include";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-index/domain-name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "include";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/obj-thresh-max";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/obj-thresh-min";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/auth-header";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/cookie-header";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/query-string";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/tunnel-override/request/cache-control";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/tunnel-override/response/cc-notrans";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/tunnel-override/response/obj-expired";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/method";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "head";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/validate-header";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/validate-header/header-name";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "etag";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/object/correlation/etag/ignore";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-fill/client-driven/aggressive_threshold";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "9";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-ingest/hotness-threshold";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "3";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-ingest/size-threshold";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 26, 27);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/client-request/method/connect/handle";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 26, 27);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/header/set-cookie/cache/no-revalidation";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-header";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-bytes";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-headers/*";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-headers/*/header-name";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    /* upgrade fails from 2.0.9 to 2.3 for this node . since the version in 2.0.9 is 28.
     * This node is not created for an upgrade
     * Bumping revision number 22,23 to 28,29
     */

    /* Cache - Pinning */
    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/cache_size";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/max_obj_size";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/enable";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/pin_header_name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/validity/start_header_name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/cache_pin/validity/end_header_name";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/content-store/media/uri-depth-threshold";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "10";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/secure";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/plain-text";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 30, 31);
    rule->mur_upgrade_type =	    MUTT_CLAMP;
    rule->mur_name =		    "/nkn/nvsd/origin_fetch/config/*/cache-ingest/hotness-threshold";
    rule->mur_lowerbound =	    3;
    rule->mur_upperbound =	    65535;
    MD_ADD_RULE(ra);

    /* adding node in 31-> 32 to handle cases of upgrade from images where this node doesn't  exists */
    MD_NEW_RULE(ra, 31, 32);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/object/correlation/validatorsall/ignore";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 31, 32);
    rule->mur_upgrade_type =        MUTT_REPARENT;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/object/correlation/versioncheck";
    rule->mur_reparent_self_value_follows_name = true;
    rule->mur_reparent_graft_under_name = NULL;
    rule->mur_reparent_new_self_name = "validatorsall";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 32, 33);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/host-header/header-value";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 33, 34);
    rule->mur_upgrade_type = 	MUTT_CLAMP;
    rule->mur_name = 		"/nkn/nvsd/origin_fetch/config/*/content-store/media/cache-age-threshold";
    rule->mur_lowerbound = 	0;
    rule->mur_upperbound = 	2592000; // 30 days
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 33, 34);
    rule->mur_upgrade_type = 	MUTT_CLAMP;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/cache-age-threshold";
    rule->mur_lowerbound = 	0;
    rule->mur_upperbound = 	2592000; // 30 days
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 34, 35);
    rule->mur_upgrade_type =        MUTT_CLAMP;
    rule->mur_name =                "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-bytes";
    rule->mur_lowerbound =          0;
    rule->mur_upperbound =          32768;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/object/compression/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/object/compression/algorithm";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "gzip";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/object/compression/obj_range_min";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "128";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/object/compression/obj_range_max";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "10485760";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/object/compression/file_types/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 36, 37);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/cache-control/s-maxage-ignore";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 36, 37);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/cache/response/*";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 36, 37);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =		"/nkn/nvsd/origin_fetch/config/*/cache/response/*/age";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "180";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 37, 38);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-ingest/byte-range-incapable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 38, 39);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-offset";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 38, 39);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/from-end";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 39, 40);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/origin_fetch/config/*/vary-header/pc";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 39, 40);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/origin_fetch/config/*/vary-header/tablet";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 39, 40);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/origin_fetch/config/*/vary-header/phone";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 39, 40);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/header/vary";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 39, 40);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/client-header-inherit";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 40, 41);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/fetch-page";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 40, 41);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/*/fetch-page-count";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "1";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);
bail:
    return err;
}



/* ------------------------------------------------------------------------- */
static int
md_nvsd_origin_fetch_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes=0;
    const mdb_db_change *change = NULL;
    const char *namespace = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);

    for (int i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i );
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "nvsd", "origin_fetch", "config", "*", "obj-thresh-max"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint32 t_obj_thresh_max = 0;
	    uint32 t_obj_thresh_min = 0;
	    char *thresh_min_node_name = NULL;

	    namespace = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    thresh_min_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/obj-thresh-min", namespace);
	    bail_null(thresh_min_node_name);

	    err = mdb_get_node_value_uint32(commit, new_db,
		    thresh_min_node_name, 0, NULL, &t_obj_thresh_min);
	    bail_error(err);

	    err = mdb_get_node_value_uint32(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &t_obj_thresh_max);
	    bail_error(err);

	    if ( (t_obj_thresh_min != 0) && (t_obj_thresh_max != 0) ) {
		if ( t_obj_thresh_min > t_obj_thresh_max ) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("Bad size values\n"));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "nvsd", "origin_fetch", "config", "*", "obj-thresh-min"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint32 t_obj_thresh_max = 0;
	    uint32 t_obj_thresh_min = 0;
	    char *thresh_max_node_name = NULL;

	    namespace = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    thresh_max_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/obj-thresh-max", namespace);
	    bail_null(thresh_max_node_name);

	    err = mdb_get_node_value_uint32(commit, new_db, thresh_max_node_name, 0,
		    NULL, &t_obj_thresh_max);
	    bail_error(err);

	    err = mdb_get_node_value_uint32(commit, new_db, ts_str(change->mdc_name), 0,
		    NULL, &t_obj_thresh_min);
	    bail_error(err);

	    if ( (t_obj_thresh_min != 0) && (t_obj_thresh_max != 0) ) {
		if ( t_obj_thresh_min > t_obj_thresh_max ) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("Bad size values\n"));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
			"nkn", "nvsd", "origin_fetch", "config", "*", "object", "compression", "obj_range_max"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint64 t_obj_thresh_max = 0;
	    uint64 t_obj_thresh_min = 0;
	    char *thresh_min_node_name = NULL;

	    namespace = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    thresh_min_node_name =
		smprintf("/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_min", namespace);
	    bail_null(thresh_min_node_name);

	    err = mdb_get_node_value_uint64(commit, new_db,
		    thresh_min_node_name, 0, NULL, &t_obj_thresh_min);
	    bail_error(err);

	    err = mdb_get_node_value_uint64(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &t_obj_thresh_max);
	    bail_error(err);

	    if ( (t_obj_thresh_min != 0) && (t_obj_thresh_max != 0) ) {
		if ( t_obj_thresh_min > t_obj_thresh_max ) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("Bad size values\n"));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
			"nkn", "nvsd", "origin_fetch", "config", "*", "object", "compression", "obj_range_min"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint64 t_obj_thresh_max = 0;
	    uint64 t_obj_thresh_min = 0;
	    char *thresh_max_node_name = NULL;

	    namespace = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    thresh_max_node_name =
		smprintf("/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_max", namespace);
	    bail_null(thresh_max_node_name);

	    err = mdb_get_node_value_uint64(commit, new_db, thresh_max_node_name, 0,
		    NULL, &t_obj_thresh_max);
	    bail_error(err);

	    err = mdb_get_node_value_uint64(commit, new_db, ts_str(change->mdc_name), 0,
		    NULL, &t_obj_thresh_min);
	    bail_error(err);

	    if ( (t_obj_thresh_min != 0) && (t_obj_thresh_max != 0) ) {
		if ( t_obj_thresh_min > t_obj_thresh_max ) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("Bad size values\n"));
		    bail_error(err);
		    goto bail;
		}
	    }
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 9,
			"nkn", "nvsd", "origin_fetch", "config", "*", "object", "compression", "file_types", "*"))
		&& (mdct_delete != change->mdc_change_type)) {
	    node_name_t name = {0};

	    const char *ns_name =	tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    const char *file_extn_name = tstr_array_get_str_quick(change->mdc_name_parts, 8);
	    tstr_array *t_compression_file_extns = NULL;
	    tstring *t_file_extn_name = NULL;


	    if((!(lc_is_prefix(".", file_extn_name, true))) ||
		    (strlen(file_extn_name) <= 1))
	    {
		err = md_commit_set_return_status_msg(commit, 1,
			"File extension name should starts with '.'");
		bail_error(err);
		goto bail;
	    }

	    snprintf(name, sizeof(name), "/nkn/nvsd/origin_fetch/config/%s/object/compression/file_types/*",
		    ns_name);

	    err = mdb_get_matching_tstr_array(commit,
		    old_db,
		    name,
		    0,
		    &t_compression_file_extns);
	    bail_error(err);
	    if (t_compression_file_extns != NULL) {
		for(uint32 j = 0; j < tstr_array_length_quick(t_compression_file_extns); j++) {

		    err = mdb_get_node_value_tstr(commit,
			    old_db,
			    tstr_array_get_str_quick(t_compression_file_extns,
				j),                                         0, NULL,
			    &t_file_extn_name);
		    bail_error(err);

		    if (t_file_extn_name && ts_equal_str(t_file_extn_name, file_extn_name, true)) {
			/* Name matched... REJECT the commit */
			err = md_commit_set_return_status_msg(commit, 1,
				"File extension already exists");
			bail_error(err);
			goto bail;
		    }
		}
	    }

	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
			"nkn", "nvsd", "origin_fetch", "config", "*", "cache", "response", "*"))
		&& (mdct_delete != change->mdc_change_type)) {
	    node_name_t name = {0};

	    const char *ns_name =	tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    const char *res_code = tstr_array_get_str_quick(change->mdc_name_parts, 7);
	    int response = 0;
	    if(res_code)
		response = atoi(res_code);

	    if(response != 301 && response != 302 &&  response != 303 &&
		    response != 403 && response != 404 && response != 406 &&
		    response != 410 && response != 414)
	    {
		err = md_commit_set_return_status_msg(commit, 1,
			"Given response code is not supported");
		bail_error(err);
		goto bail;
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    return(err);
}


static int
md_nvsd_origin_fetch_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    int	i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    char *bn_ns_node = NULL;
    tbool found = false;
    tstring *t_namespace = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "origin_fetch", "config", "*"))
		&& (mdct_add == change->mdc_change_type))
	{
	    /* Check if the namespace parent node was created..
	     * If not create an empty one
	     */
	    bn_ns_node = smprintf("/nkn/nvsd/namespace/%s",
		    tstr_array_get_str_quick(change->mdc_name_parts, 4));
	    bail_null(bn_ns_node);

	    err = mdb_get_node_value_tstr(NULL, old_db,
		    bn_ns_node, 0, &found,
		    &t_namespace);
	    bail_error(err);

	    if (!found) {
		/* Namespace node doesnt exist, create one */
		bn_str_value binding_value;

		binding_value.bv_name = bn_ns_node;
		binding_value.bv_type = bt_string;
		binding_value.bv_value = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		err = mdb_create_node_bindings(commit, inout_new_db, &binding_value, 1);
		bail_error(err);
	    }
	    safe_free(bn_ns_node);
	    ts_free(&t_namespace);
	}
    }

bail:
    safe_free(bn_ns_node);
    ts_free(&t_namespace);
    return err;
}


static int
md_nvsd_origin_fetch_upgrade_downgrade(const mdb_db *old_db,
					mdb_db *inout_new_db,
					uint32 from_module_version,
					uint32 to_module_version,
					void *arg)
{
    int err = 0;
    tbool found = false;
    bn_binding *new_binding = NULL;
    char *new_name = NULL;
    uint32 i = 0;
    uint32 num_ns_names = 0;
    tstr_array *ts_ns_names = NULL;
    uint32 cache_default_val = 0;
    tstring *t_ns_name = NULL;

    err - md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);

    if (from_module_version == 3 && to_module_version == 4) {
	err = mdb_get_matching_tstr_array(NULL, inout_new_db,
		"/nkn/nvsd/origin_fetch/config/*/cache-age",
		0,
		&ts_ns_names);
	bail_error(err);

	num_ns_names = tstr_array_length_quick(ts_ns_names);
	for(i = 0; i < num_ns_names; ++i) {
	    // get the origin_fetch names (per NS)
	    t_ns_name = tstr_array_get_quick(ts_ns_names, i);

	    // Get the node value
	    err = mdb_get_node_value_uint32(NULL, inout_new_db,
		    ts_str(t_ns_name), 0, &found, &cache_default_val);
	    bail_error(err);

	    if (!found)
		goto bail;

	    // Now delete the old node now what we have the value
	    // /nkn/nvsd/origin_fetch/config/*/cache-age/default

	    // Create the old nodes binding name
	    new_name = smprintf("%s", ts_str(t_ns_name));
	    bail_null(new_name);

	    // Create a binding node and set its value
	    err = bn_binding_new_uint32(
		    &new_binding, new_name, ba_value,
		    0, 0);
	    bail_error(err);

	    err = mdb_set_node_binding(NULL, inout_new_db, bsso_delete,
		    0, new_binding);
	    bail_error(err);
	    safe_free(new_name);

	    // Create the new binding name
	    new_name = smprintf("%s/default", ts_str(t_ns_name));
	    bail_null(new_name);

	    // Create a binding node and set its value for
	    // the new node
	    // /nkn/nvsd/origin_fetch/config/*/cache-age/default
	    err = bn_binding_new_uint32(
		    &new_binding, new_name, ba_value,
		    0, cache_default_val);
	    bail_error(err);

	    // fire of message to create and modify the node.
	    err = mdb_set_node_binding(NULL, inout_new_db, bsso_modify,
		    0, new_binding);
	    bail_error(err);

	    bn_binding_free(&new_binding);
	    safe_free(new_name);

	}
    }

bail:
    bn_binding_free(&new_binding);
    safe_free(new_name);
    tstr_array_free(&ts_ns_names);
    return err;
}


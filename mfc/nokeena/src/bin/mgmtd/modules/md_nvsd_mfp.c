/*
 *
 * Filename:  md_nvsd_mfp.c
 * Date:      2010/09/02
 * Author:    Varsha Rajagopalan
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
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
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#define CLIST_STREAM_NAME CLIST_ALPHANUM "_"  "-"
#define NKNCNT_BINARY_PATH     "/opt/nkn/bin/nkncnt"
#define FILE_PUBLISHER    "/opt/nkn/sbin/file_mfpd"
#define LIVE_PUBLISHER    "/opt/nkn/sbin/live_mfpd"
#define MAX_NODE_SIZE 512
/* The global variable has been added as flag to indicate when the
 * mount point check for profile source has to be done.
 * BG: Bug 9070. A session with a profile having a valid mount point  is stored.
 * The mount point is removed at a later point and the machine is upgraded/reloaded,
 * the mgmtd exits in the init function since the check for the session profile
 * nfs mount point fails./
 * This flag will enable checking of mount point only after the mgmtd has come up.The
 * mount point check will not be done when mgmtd is coming up.
 */
int mfp_mod_init_done = 0;

/* ------------------------------------------------------------------------- */

int md_nvsd_mfp_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_mfp_ug_rules = NULL;

static int md_nvsd_mfp_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_nvsd_mfp_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_mfp_upgrade_downgrade(const mdb_db *old_db,
                              mdb_db *inout_new_db,
                              uint32 from_module_version,
                              uint32 to_module_version,
                              void *arg);
static int md_nvsd_mfp_commit_side_effects(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		mdb_db_change_array *change_list, void *arg);

static int
md_mfp_check_flash_segment_duration(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_check_pid_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_check_port_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_check_sliding_window_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_check_mobile_delivery_url(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_check_profile_source_duplicate(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);
static int
md_mfp_check_ssp_nfs_mount_exists(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);
static int
md_mfp_check_ipsource_mandatory(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_check_flash_encrypt_extension(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_mfp_get_counters_details(md_commit *commit,
	const mdb_db *db, const char *action_name,
	bn_binding_array *params, void *cb_arg);
/* ------------------------------------------------------------------------- */

md_commit_forward_action_args md_mfp_gmgmthd_fwd_args =
{
    GCL_CLIENT_MFP, true, N_("Request failed; MFD subsystem 'mfp-gmgmthd' did not respond"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};
/* ------------------------------------------------------------------------- */
int
md_nvsd_mfp_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Media flow publisher specific module
     */
    err = mdm_add_module("nvsd-mfp", 4,
	    "/nkn/nvsd/mfp", NULL,
	    modrf_namespace_unrestricted,
	    md_nvsd_mfp_commit_side_effects,
	    NULL,
	    NULL, NULL,
	    md_nvsd_mfp_commit_apply, NULL,
	    0, 0,
	    md_nvsd_mfp_upgrade_downgrade,
	    &md_nvsd_mfp_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_limit_str_max_chars 	= 	40;
    node->mrn_limit_str_charlist = CLIST_STREAM_NAME;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Set of MFP stream configs";
    node->mrn_bad_value_msg =      N_("Error: Either the Session name has more than 40 characters or  Invalid characters in it.\n"
	    "Valid character set: a-z, A-Z, 0-9, '_', '-'\n");
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * To allow restart of a session, we will change the internal name
     * used for MFP.The node value will be updated when we get restart
     * action message.
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/name";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "MFP Stream name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/status";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Published status - new/publish ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-type";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Media type -file /live";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Stream media source & encapsulation type-file_ssp_media_sbr/file_ssp_media_mbr/file_spts_media_profile/live_spts_media_profile";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * BUG :6877 - There is no difference between SSP - MBR and SBR.
     * So by default for SSP , the file_ssp_media_sbr nodes will be used.
     * Commenting out the nodes that were getting created for MBR, as they are not needed
     * anymore.
     */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/svr-manifest-url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =	   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_ssp_nfs_mount_exists;
    node->mrn_description =        "SSP-Media-SBR server-manifest source address";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/svr-manifest-bitrate";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  1;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  40000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "SSP-Media-SBR server-manifest bit rate";
    err = mdm_add_node(module, &node);
    bail_error(err);
    /*
     * BUG: 6874
     * Add the Protocol node , to set the protocol used to fetch the file.
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/protocol";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =	   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "SSP-Media protocol used to fetch the file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/client-manifest-url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =	   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "SSP-Media-SBR client-manifest source address";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/client-manifest-bitrate";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  1;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  40000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "SSP-Media-SBR client-manifest bit rate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/type";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =	   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Type of the selected source in SSP-Media-SBR either mux or video or audio";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "URL of the selected source in SSP-Media_SBR";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_ssp_media_sbr/bitrate";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  1;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  40000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Bitrate of the selected source in SSP-Media_SBR";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  20;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 20;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 20 profiles");
    node->mrn_description =        "Each File Media source SPTS-Media Profile";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*/source";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =	   md_mfp_check_profile_source_duplicate;
    node->mrn_description =        "Each File media source SPTS-Media profile source ip";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*/dest";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each File media source SPTS-Media profile dest ip";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*BUG FIX: 6694
     * Removing the default value for bitrate as 1.
     * Setting default value as 0 which indicates  that
     * the MFP will generate the bitrate value by itself.
     * Will be adding a check in the gmgthd to send the
     * BitRate tag if the value is > 0
     */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*/bitrate";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  40000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each File media source SPTS-Media Profile bitrate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*/vpid";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_charlist = "0123456789abcdefABCDEFx";
    node->mrn_check_node_func =       md_mfp_check_pid_range;
    node->mrn_bad_value_msg =      N_("error:illegal characters present. Enter either an decimal or hexadecimal value");
    node->mrn_description =        "Each File media source SPTS-Media Profile VPID number(16 to 8190)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*/apid";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_charlist = "0123456789abcdefABCDEFx";
    node->mrn_check_node_func =       md_mfp_check_pid_range;
    node->mrn_bad_value_msg =      N_("error:illegal characters present. Enter either an decimal or hexadecimal value");
    node->mrn_description =        "Each File media source SPTS-Media Profile APID number(16 to 8190";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*BUG FIX: 6656
     * Adding a node to take the protocol that will be used
     * to fetch the input source file for file spts.
     * TODO: Currently only NFS can be given. This is restricted in the
     * web-gui
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_spts_media_profile/*/protocol";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each File media source SPTS-Media profile input file fetch protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  20;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_bad_value_msg =      N_("error:Max profiles already configured.");
    node->mrn_description =        "Each Live Media source SPTS-Media Profile";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/source";
    node->mrn_value_type =         bt_ipv4addr;
    node->mrn_initial_value =      "0.0.0.0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_ipsource_mandatory;
    node->mrn_description =        "Each Live media source SPTS-Media profile source ip";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/dest";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each Live media source SPTS-Media profile dest ip";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/bitrate";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  1;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  40000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each Live media source SPTS-Media Profile bitrate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/vpid";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_charlist = "0123456789abcdefABCDEFx";
    node->mrn_check_node_func =       md_mfp_check_pid_range;
    node->mrn_bad_value_msg =      N_("error:illegal characters present. Enter either an decimal or hexadecimal value");
    node->mrn_description =        "Each Live media source SPTS-Media Profile VPID number(16 to 8190";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/apid";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_charlist = "0123456789abcdefABCDEFx";
    node->mrn_check_node_func =       md_mfp_check_pid_range;
    node->mrn_bad_value_msg =      N_("error:illegal characters present. Enter either an decimal or hexadecimal value");
    node->mrn_description =        "Each Live media source SPTS-Media Profile APID number(16 to 8190";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/srcport";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  0;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_port_range;
    node->mrn_description =        "Each Live media source SPTS-Media Profile Source port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*Adding a new node for source address */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/ipsource";
    node->mrn_value_type =         bt_ipv4addr;
    node->mrn_initial_value =	   "0.0.0.0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each Live media source SPTS-Media profile source ip address";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/status";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Publishing Scheme-Mobile-usage status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/keyframe";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  2000;
    node->mrn_limit_num_min_int =  2000;
    node->mrn_limit_num_max_int =  30000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Key frame rate for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/segment-start";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  1;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Segment start index for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/segment-precision";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  3;
    node->mrn_limit_num_min_int =  2;
    node->mrn_limit_num_max_int =  10;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "segment index precision for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*TODO
     *  The segment rollover check , do we need a limit checker here
     *  since the limit meant in MFP is different from the limits in mgmtd.
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/segment-rollover";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  20;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_sliding_window_range;
    node->mrn_description =        "Segment start index for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/minsegsinchildplay";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  10;
    node->mrn_limit_num_min_int =  3;
    node->mrn_limit_num_max_int =  30;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Minimum segments in child playlist for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/dvr";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "DVR enable/disable for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/storage_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Storage URL for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/delivery_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_mobile_delivery_url;
    node->mrn_description =        "Delivery URL for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Release 11.A Encryption  related nodes*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Enable / disable encryption for Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/unique_key";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Enable / disable unique key/profile for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/key_rotation";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  65535;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Key rotation interval for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_native";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "KMS External agent for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_external";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "KMS External agent for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_srvr";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "KMS External agent server address for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_port";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  12684;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  65535;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "KMS External agent server port for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_native_prefix";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "KMS Native file type prefix for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_storage_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "KMS Native key file storage url used for encryption in Mobile Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/smooth/status";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Publishing Scheme-Smoothstreaming-usage status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/smooth/keyframe";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  2000;
    node->mrn_limit_num_min_int =  2000;
    node->mrn_limit_num_max_int =  30000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Key frame rate for Smoothstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/smooth/dvrlength";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  30;
    node->mrn_limit_num_min_int =  5;
    node->mrn_limit_num_max_int =  180;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "DVR window length for Live in minutes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/smooth/dvr";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "DVR enable/disable for Smoothstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/smooth/storage_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Storage URL for Smoothstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/smooth/delivery_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Delivery URL for Smoothstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/status";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Publishing Scheme-Flashstreaming-usage status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/keyframe";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  2000;
    node->mrn_limit_num_min_int =  2000;
    node->mrn_limit_num_max_int =  30000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Key frame rate for Flashstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/storage_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Storage URL for Flashstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/delivery_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Delivery URL for Flashstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/segment";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  2000;
    node->mrn_limit_num_min_int =  2000;
    node->mrn_limit_num_max_int =  30000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func = md_mfp_check_flash_segment_duration;
    node->mrn_description =        "Segment duration for Flashstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/stream_type";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "vode";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "DVR/vod/live  for Flashstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/dvrsec";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  30;
    node->mrn_limit_num_min_int =  5;
    node->mrn_limit_num_max_int =  180;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "DVR seconds for Flashstreaming Pub scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Encryption enable /disable for Flashstreaming";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/common_key";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Common key for encryption for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/lic_srvr_url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "License server url for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/lic_srvr_cert";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_flash_encrypt_extension;
    node->mrn_description =        "License server certificate for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/trans_cert";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_flash_encrypt_extension;
    node->mrn_description =        "Transport certificate for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/pkg_cred";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_flash_encrypt_extension;
    node->mrn_description =        "Package credentials for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/cred_pswd";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "credential password for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/policy_file";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_flash_encrypt_extension;
    node->mrn_description =        "Policy file for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/encrypt/contentid";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =    md_mfp_check_flash_encrypt_extension;
    node->mrn_description =        "Content Id for Flash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/flash/http_origin_reqd";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP Origin module required for Flashstreaming";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/streamid";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Auto generated stream id for each stream when created or modified";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mfp/config/*/stop-time";
    node->mrn_initial_value_int =   0;
    node->mrn_value_type        = bt_time_sec;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Stop time value to be stored";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*MP4 -Package*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  20;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 20;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 20 profiles");
    node->mrn_description =        "Each File Media source MP4 Profile";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*/source";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =	   md_mfp_check_profile_source_duplicate;
    node->mrn_description =        "Each File media source MP4 file path";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*/bitrate";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  40000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each File media source MP4 profile bitrate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*/protocol";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Each File media source MP4 profile input file fetch protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/mfp/status-time";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  5;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  60;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Refresh duration for status update in minutes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     *   Action nodes
     */

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mfp/actions/publish";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_mfp_gmgmthd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "stream-name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mfp/actions/stop";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_mfp_gmgmthd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "stream-name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mfp/actions/status";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_mfp_gmgmthd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "stream-name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mfp/actions/remove";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_mfp_gmgmthd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "stream-name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mfp/actions/restart";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_mfp_gmgmthd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "stream-name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 5);
    bail_error(err);
    node->mrn_name               = "/nkn/nvsd/mfp/actions/getcounter";
    node->mrn_node_reg_flags     = mrf_flags_reg_action;
    node->mrn_cap_mask           = mcf_cap_action_privileged;
    node->mrn_action_func        = md_mfp_get_counters_details;
    node->mrn_actions[0]->mra_param_name = "process";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "frequency";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "duration";
    node->mrn_actions[2]->mra_param_type = bt_string;
    node->mrn_actions[3]->mra_param_name = "filename";
    node->mrn_actions[3]->mra_param_type = bt_string;
    node->mrn_actions[4]->mra_param_name = "pattern";
    node->mrn_actions[4]->mra_param_type = bt_string;
    node->mrn_description        = "Live/File publisher counter details";
    err = mdm_add_node(module, &node);

    /*Upgrade rules */
    err = md_upgrade_rule_array_new(&md_nvsd_mfp_ug_rules);
    bail_error(err);
    ra = md_nvsd_mfp_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/media-encap/live_spts_media_profile/*/ipsource";
    rule->mur_new_value_type =  bt_ipv4addr;
    rule->mur_new_value_str  =  "0.0.0.0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*/source";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*/protocol";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/media-encap/file_mp4/*/bitrate";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/dvrsec";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "30";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/segment";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "2000";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/stream_type";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "vod";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/common_key";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/lic_srvr_url";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/lic_srvr_cert";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/trans_cert";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/pkg_cred";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/cred_pswd";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/policy_file";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/flash/encrypt/contentid";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/mfp/config/*/flash/http_origin_reqd";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/mfp/config/*/flash/encrypt/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/mobile/encrypt/kms_port";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "12684";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/mobile/segment-precision";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "3";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/mfp/config/*/mobile/keyframe";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "3";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =		"/nkn/nvsd/mfp/config/*/flash/dvr";
    MD_ADD_RULE(ra);

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_mfp_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

 bail:
    bn_binding_free(&binding);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_mfp_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;

    /*
     * Nothing to do.
     */
    /*The commit apply is the final step in mgmtd coming up process
     *  of side-effects, commit-check , commit apply.
     *  The flag is set indicating that the mount point check needs to be done
     *  any new profile that is going to be added.
     */
    mfp_mod_init_done  = 1;

 bail:
    return(err);
}

static int
md_nvsd_mfp_upgrade_downgrade(const mdb_db *old_db,
                              mdb_db *inout_new_db,
                              uint32 from_module_version,
                              uint32 to_module_version,
                              void *arg)
{
	int err = 0;
	const char *old_binding_name = NULL;

	err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
		to_module_version, arg);
	bail_error(err);

bail:
	return err;
}

static int
md_nvsd_mfp_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    tbool	found = false;
    int	i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    node_name_t node_name = {0};
    bn_binding *binding = NULL;
    tbool ret_is_abs;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);
	if((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "mfp", "config", "*", "mobile", "segment-rollover") ||
	    bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "mfp", "config", "*", "mobile", "minsegsinchildplay")) &&
	   (mdct_modify == change->mdc_change_type)) {
	    const char *streamname = NULL;
	    uint32_t minsegs = 0, sliding = 0;

	    streamname = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    bail_null(streamname);

	    snprintf(node_name, 256, "/nkn/nvsd/mfp/config/%s/mobile/minsegsinchildplay", streamname);
	    bail_null(node_name);
	    err = mdb_get_node_binding (commit, inout_new_db, node_name, 0, &binding);
	    bail_null(binding);
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &minsegs);
	    bail_error(err);

	    snprintf(node_name, 256, "/nkn/nvsd/mfp/config/%s/mobile/segment-rollover", streamname);
	    bail_null(node_name);
	    err = mdb_get_node_binding (commit, inout_new_db, node_name, 0, &binding);
	    bail_null(binding);
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &sliding);
	    bail_error(err);

	    if ((sliding <5) ||(sliding >32) || (sliding < (minsegs+ 2))){
		err = md_commit_set_return_status_msg_fmt (commit, 1,
		_("Invalid sliding window number '%u'.Enter value between 5 to 32 and atleast 2 greater than Minimum segs in child playlist\n"), (unsigned int)sliding);
		bail_error(err);
	    }

	}
    }

bail:
    return err;

}


static int
md_mfp_check_pid_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    char *pidrange = NULL;
    int val = 0;

    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    if (change_type != mdct_add && change_type != mdct_modify) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &pidrange);
    bail_error_null(err, pidrange);
    if (strlen(pidrange) == 0 ){
	goto bail;
    }
    /* if it has prefix 0x - treat as hexadecimal*/
    else if ( lc_is_prefix( "0x", pidrange, false)){
	uint32 i = 2; // skip 0x
	for(; i< strlen(pidrange); i++){
	    if(!isxdigit(pidrange[i])) {
		/*Invalid hex digit*/
		err = md_commit_set_return_status_msg_fmt
		    (commit, 1, _("Invalid nex digit '%c' for the pid value\n"), pidrange[i]);
		bail_error(err);
	    }
	    if (('0' <= pidrange[i]) && (pidrange[i] <= '9')) {
		val = val * 16 + (pidrange[i] - '0');
	    }
	    else if (('a' <= pidrange[i]) && (pidrange[i] <= 'f')) {
		val = val * 16 + (10 + (pidrange[i] - 'a'));
	    } else if (('A' <= pidrange[i]) && (pidrange[i] <= 'F')) {
		val = val * 16 + (10 + (pidrange[i] - 'A'));
	    }
	}
	if( val<16 || val > 8190) {
	    err = md_commit_set_return_status_msg_fmt(commit, 1, _("Out of range value: '%d' for the pid value\n."
			"Valid range: 16 to 8190\n"), val);
	    bail_error(err);
	}
    }else {
	uint32 i = 0;
	for( ; i< strlen(pidrange); i++) {
	    if(!isdigit(pidrange[i])){
		/*error invalid digit*/
		err = md_commit_set_return_status_msg_fmt(commit, 1, _("Invalid digit specified: '%c' for the pid value.\n"), val);
		bail_error(err);
	    }
	}
	val = atoi(pidrange);
	if (val < 16 || val > 8190) {
	    /*Error - out of range*/
	    err = md_commit_set_return_status_msg_fmt(commit, 1, _("Out of range value: '%d' for the pid value\n."
			"Valid range: 16 to 8190\n"), val);
	    bail_error(err);
	}
    }




bail:
    safe_free(pidrange);
    return err;
}
/*
 * BUG FIX:
 * The port value accepted for live spts profile
 * should be between 1024 and 65535 and
 * should not have the value 8080.
 */
static int
md_mfp_check_port_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    uint64 port;
    int val = 0;

    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_uint64(attrib, NULL, NULL, &port);
    bail_error(err);
    if ((port ==0) ||(port ==8080) || (port < 1024 ) || (port > 65535)){
	err = md_commit_set_return_status_msg_fmt
	    (commit, 1, _("Invalid port number '%u'.Enter value between 1024 and 65535 excluding 8080\n"), (unsigned int)port);
	bail_error(err);
    }

bail:
    return err;
}
/*
 * BUG FIX: The sliding window size should be
 * atleast 2 greater than the minsegments to play
 * value. The range check for sliding window is done
 * here.
 */
static int
md_mfp_check_sliding_window_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    uint32 sliding = 0;
    int val = 0;
    char *minsegs_node_name = NULL;
    bn_binding *binding = NULL;
    uint32 minsegs = 0;

    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
	goto bail;
    }

    bail_null(new_attribs);
    if ( (change_type != mdct_add)&& (change_type != mdct_modify) ) {
	goto bail;
    }
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    minsegs_node_name = smprintf("/nkn/nvsd/mfp/config/%s/mobile/minsegsinchildplay", streamname);
    bail_null(node_name);
    err = mdb_get_node_binding (commit, new_db, minsegs_node_name, 0, &binding);
    bail_null(binding);
    err = bn_binding_get_uint32(binding, ba_value, NULL, &minsegs);
    bail_error(err);
    err = bn_attrib_get_uint32(attrib, NULL, NULL, &sliding);
    bail_error(err);
    if ((sliding <5) ||(sliding >32) || (sliding < (minsegs+ 2))){
	err = md_commit_set_return_status_msg_fmt (commit, 1, _("Invalid sliding window number '%u'.Enter value between 5 to 32 and atleast 2 greater than Minimum segs in child playlist\n"), (unsigned int)sliding);
	bail_error(err);
    }

bail:
    safe_free(minsegs_node_name);
    bn_binding_free(&binding);
    return err;
}
/*
 * BUG FIX: 6304
 * Check if the delivery url for mobile streaming
 * ends with the extension m3u8. Else report error.
 */
static int
md_mfp_check_mobile_delivery_url(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    uint32 sliding = 0;
    int val = 0;
    char *delivery_url = NULL;

    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    if ( (change_type != mdct_modify) ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &delivery_url);
    bail_error(err);
    if ( !(lc_is_suffix(".m3u8", delivery_url, false))){
	err = md_commit_set_return_status_msg_fmt (commit, 1, _("Delivery url must have the .m3u8 extension\n"));
	bail_error(err);
    }
    /*BUG 7713:
     *Check if any other character is present apart from the extension. Else report
     *error to give a valid url name before extension.
     */
    if (strlen(delivery_url) <=5){
	err = md_commit_set_return_status_msg_fmt (commit, 1, _("Delivery url must have the valid name before .m3u8 extension\n"));
	bail_error(err);
    }

bail:
    safe_free(delivery_url);
    return err;
}

/*
 * BUG FIX: 6689
 *  Sample  source file  cant be given across multiple profiles
 *  within the same stream session name  of type File Multirate MPEG-2 TS.
 */
static int
md_mfp_check_profile_source_duplicate(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    const char *profile_id = NULL;
    const char *media_encap = NULL;
    uint32 sliding = 0;
    int val = 0;
    bn_binding *binding = NULL;
    char *source_url = NULL;
    tstr_array *source_tstr_bindings = NULL;
    unsigned int arr_index = 0;
    const char *old_binding_name = NULL;
    const char *s_node_name = NULL;
    const char *protocol_node_name = NULL;
    const char *mount_name = NULL;
    tstr_array *mount_names = NULL;
    tstring *t_protocol = NULL;
    int32 code = 0;

    /*BUG FIX:9070:Do the mount point check only after mgmtd has come up*/
    if(mfp_mod_init_done) {
    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    profile_id = tstr_array_get_str_quick(node_name_parts, 7);
    bail_null(profile_id);

    /*Get the media-encap to choose the node structure we have to use*/
    media_encap = tstr_array_get_str_quick(node_name_parts, 6);
    bail_null(profile_id);

    if ( (change_type != mdct_add)  ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &source_url);
    bail_error(err);

    /*BUG 6656
     * Check if the mount point exists for this source if the
     * protocol selected is NFS
     */
    protocol_node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/%s/%s/protocol", streamname, media_encap, profile_id);
    bail_null(protocol_node_name);

    err = mdb_get_node_binding (commit, new_db, protocol_node_name, 0, &binding);
    bail_null(binding);
    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_protocol);
    bail_error(err);

    if(ts_equal_str(t_protocol, "nfs", false)) {
	err = ts_tokenize_str( source_url, '/', 0, 0,
		ttf_ignore_leading_separator |
		ttf_ignore_trailing_separator |
		ttf_single_empty_token_null, &mount_names);
	bail_null(mount_names);
	mount_name = tstr_array_get_str_quick(mount_names, 0);
	bail_null(mount_name);
	err = lc_launch_quick_status(&code, NULL, false, 2,
		"/opt/nkn/bin/mgmt_nfs_mountstat.sh",
		mount_name);
	if(code) {
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("Profile source %s mount point doesn't exists or has an mount error.\n"), source_url);
	    bail_error(err);
	    goto bail;
	}
    }

    s_node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/%s/*/source", streamname, media_encap);
    bail_null(s_node_name);

    err = mdb_get_matching_tstr_array( NULL,
	    old_db,
            s_node_name,
	    0,
	    &source_tstr_bindings);
    bail_error(err);
    bail_null(source_tstr_bindings);
    /* Check all the existing profiles to see if
     * the given source file already exists
     */
    for ( arr_index = 0; arr_index < tstr_array_length_quick(source_tstr_bindings); ++arr_index)
    {
	tstring *t_source = NULL;
	old_binding_name = tstr_array_get_str_quick(
		source_tstr_bindings, arr_index);
	err = mdb_get_node_binding (commit, new_db, old_binding_name, 0, &binding);
	bail_null(binding);
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_source);
	bail_error(err);
	if((ts_equal_str(t_source, source_url, false))){
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("Profile source %s already exists.\n"), source_url);
	    bail_error(err);
	    break;
	}
	ts_free(&t_source);
    }
    }
bail:
    tstr_array_free(&source_tstr_bindings);
    tstr_array_free(&mount_names);
    safe_free(source_url);
    bn_binding_free(&binding);
    ts_free(&t_protocol);
    return err;
}
/* Function call to check if the NFS mountpoint exists for
 * File SSP
 */
static int
md_mfp_check_ssp_nfs_mount_exists(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    const char *profile_id = NULL;
    uint32 sliding = 0;
    int val = 0;
    bn_binding *binding = NULL;
    char *source_url = NULL;
    const char *protocol_node_name = NULL;
    const char *mount_name = NULL;
    tstr_array *mount_names = NULL;
    tstring *t_protocol = NULL;
    int32 code = 0;
	
    /*BUG FIX:9070:Do the mount point check only after mgmtd has come up*/
    if(mfp_mod_init_done) {
    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    if ( change_type != mdct_modify ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &source_url);
    bail_error(err);

    /*BUG 6874
     * Check if the mount point exists for this source if the
     * protocol selected is NFS
     * SBR nodes are used by default for SSP.
     */
    protocol_node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/protocol", streamname);
    bail_null(protocol_node_name);

    err = mdb_get_node_binding (commit, new_db, protocol_node_name, 0, &binding);
    bail_null(binding);
    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_protocol);
    bail_error(err);

    if(ts_equal_str(t_protocol, "nfs", false)) {
	err = ts_tokenize_str( source_url, '/', 0, 0,
		ttf_ignore_leading_separator |
		ttf_ignore_trailing_separator |
		ttf_single_empty_token_null, &mount_names);
	bail_null(mount_names);
	mount_name = tstr_array_get_str_quick(mount_names, 0);
	bail_null(mount_name);
	err = lc_launch_quick_status(&code, NULL, false, 2,
		"/opt/nkn/bin/mgmt_nfs_mountstat.sh",
		mount_name);
	if(code) {
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _(" %s mount point doesn't exists or has an mount error.\n"), source_url);
	    bail_error(err);
	    goto bail;
	}
    }
    }
bail:
    tstr_array_free(&mount_names);
    safe_free(source_url);
    bn_binding_free(&binding);
    ts_free(&t_protocol);
    return err;
}
static int
md_mfp_get_counters_details(md_commit *commit,
	const mdb_db *db, const char *action_name,
	bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    uint32_t i;
    uint32_t ret_err = 0;
    bn_request *req = NULL;
    tstring *ret_msg = NULL;
    tstring *stdout_output = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result lr;
    char *frequency = NULL;
    char *duration = NULL;
    char *pattern = NULL;
    char *filename = NULL;
    char *process = NULL;
    const bn_binding *binding = NULL;
    /* Initialize launch params */
    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, &lp);
    err = ts_new_str(&(lp->lp_path), NKNCNT_BINARY_PATH);
    bail_error(err);
    err = tstr_array_new(&(lp->lp_argv), 0);
    bail_error(err);

    err = tstr_array_insert_str(lp->lp_argv, 0,  NKNCNT_BINARY_PATH);
    bail_error(err);
    /* The process  whose shared memory has to be attached*/
    err = bn_binding_array_get_binding_by_name(params, "process", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &process);
    err = tstr_array_insert_str(lp->lp_argv, 1, "-P");
    bail_error(err);
    if(process) {
	    err = tstr_array_insert_str(lp->lp_argv, 2, process);
	    bail_error(err);
    }else {
	/*Unknown process throw error*/
	err = 1;
	goto bail;

    }
    /* Frequency value*/
    err = bn_binding_array_get_binding_by_name(params, "frequency", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &frequency);
    err = tstr_array_insert_str(lp->lp_argv, 3, "-t");
    bail_error (err);
    if(frequency) {
	err = tstr_array_insert_str(lp->lp_argv, 4, frequency);
	bail_error(err);
    }else {//Defualt value is set to 0.
	err = tstr_array_insert_str(lp->lp_argv, 4, "0");
	bail_error(err);
    }
    /* Duration value */
    err = bn_binding_array_get_binding_by_name(params, "duration", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &duration);
    err = tstr_array_insert_str(lp->lp_argv, 5, "-l");
    bail_error (err);
    if(duration) {
	err = tstr_array_insert_str(lp->lp_argv, 6, duration);
    }else {//Default value is set to 0.
	err = tstr_array_insert_str(lp->lp_argv, 6, "0");
    }
    bail_error(err);
    /* Search pattern */
    err = bn_binding_array_get_binding_by_name(params, "pattern", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &pattern);
    err = tstr_array_insert_str(lp->lp_argv, 7, "-s");
    bail_error (err);
    //The default pattern "-" is changed to" _" in the CLI command itself.
    //Since the pattern string is already set, NULL check is skipped.
    err = tstr_array_insert_str(lp->lp_argv, 8, pattern);
    bail_error(err);
    /* Filename */
    err = bn_binding_array_get_binding_by_name(params, "filename", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &filename);
    err = tstr_array_insert_str(lp->lp_argv, 9, "-S");
    bail_error (err);
    //The filename will never be NULL , hence skipping the NULL check.
    err = tstr_array_insert_str(lp->lp_argv, 10, filename);
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;
    err = lc_launch(lp, &lr);
    bail_error(err);
    //stdout_output = lr.lr_captured_output;
bail:
    safe_free(frequency);
    safe_free(pattern);
    safe_free(duration);
    safe_free(filename);
    safe_free(process);
    ts_free(&stdout_output);
    lc_launch_params_free(&lp);
    return err;
}

static int
md_mfp_check_flash_encrypt_extension(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *filename = NULL;
    char *file = NULL;

    /*Get the file name and then check for extension*/
    filename = tstr_array_get_str_quick(node_name_parts, 7);
    if ( (change_type != mdct_modify) ) {
	goto bail;
    }
    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &file);
    bail_error(err);
    if( (!(strcmp(filename, "lic_srvr_cert")))||(!(strcmp(filename, "trans_cert"))) ){
	if(!(lc_is_suffix(".der", file, false))) {
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("%s must have the .der extension\n"), file);
	    bail_error(err);
	}
    }
    else if( !(strcmp(filename, "pkg_cred")) ){
	if(!(lc_is_suffix(".pfx", file, false))) {
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("%s must have the .pfx extension\n"), file);
	    bail_error(err);
	}
    }
    else if( !(strcmp(filename, "policy_file")) ){
	if(!(lc_is_suffix(".pol", file, false))) {
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("%s must have the .pol extension\n"), file);
	    bail_error(err);
	}
    }
    if (strlen(file) <=4){
	err = md_commit_set_return_status_msg_fmt (commit, 1, _("Filename must have the valid name before file extension\n"));
	bail_error(err);
    }

bail:
    safe_free(file);
    return err;
}

static int
md_mfp_check_flash_segment_duration(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    uint32 segment_value = 0;
    int val = 0;
    bn_binding *binding = NULL;
    uint32 fragment_value = 0;
    node_name_t  fragment = {0};

    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    snprintf(fragment, sizeof(fragment), "/nkn/nvsd/mfp/config/%s/flash/keyframe", streamname);
    err = mdb_get_node_binding (commit, new_db, fragment, 0, &binding);
    bail_null(binding);
    err = bn_binding_get_uint32(binding, ba_value, NULL, &fragment_value);
    bail_error(err);
    err = bn_attrib_get_uint32(attrib, NULL, NULL, &segment_value);
    bail_error(err);
    if (segment_value < fragment_value){
	err = md_commit_set_return_status_msg_fmt (commit, 1, _("The segment duration value should be greater than or equal to the fragment duration value for Zeri\n"));
	bail_error(err);
    }

bail:
    bn_binding_free(&binding);
    return err;
}

/*
 * BUG FIX: 6689
 *  Sample  source file  cant be given across multiple profiles
 *  within the same stream session name  of type File Multirate MPEG-2 TS.
 *  Avoid bringing in dependency between 2 different nodes.
 */
static int
md_mfp_check_ipsource_mandatory(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *attrib =  NULL;
    const char *streamname = NULL;
    const char *profile_id = NULL;
    int val = 0;
    bn_binding *binding = NULL;
    char *ip_addr = NULL;
    unsigned int arr_index = 0;
    const char *old_binding_name = NULL;
    char src_ip_node[MAX_NODE_SIZE];
    tstring *t_srcip = NULL;
    int32 code = 0;

    if ( (change_type != mdct_add)&& (change_type != mdct_modify) ) {
	goto bail;
    }
    streamname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(streamname);

    profile_id = tstr_array_get_str_quick(node_name_parts, 7);
    bail_null(profile_id);


    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_str(attrib, NULL, bt_ipv4addr, NULL, &ip_addr);
    bail_error(err);

    /*BUG 8793
     * Check if the multicast reception address begins with 232.,
     * if true then the source ip address should be filled. else it can be ignored.
     */

    if(lc_is_prefix("232.", ip_addr, false)) {
	snprintf(src_ip_node, sizeof(src_ip_node), "/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/ipsource", streamname, profile_id);
	err = mdb_get_node_binding (commit, new_db, src_ip_node, 0, &binding);
	if(!(binding)){
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("Profile source IP address is mandatory for given Multicast reception IP address %s.\n"), ip_addr);
	    bail_error(err);
	    goto bail;
	}
	err = bn_binding_get_tstr(binding, ba_value, bt_ipv4addr, NULL, &t_srcip);
	bail_error(err);

	if((!t_srcip) || ((ts_length(t_srcip)<=0 ))) {
	    err = md_commit_set_return_status_msg_fmt (commit, 1, _("Profile source IP address is mandatory for given Multicast reception IP address %s.\n"), ip_addr);
	    bail_error(err);
	    goto bail;
	}
    }

bail:
    safe_free(ip_addr);
    bn_binding_free(&binding);
    ts_free(&t_srcip);
    return err;
}

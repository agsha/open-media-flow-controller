/*
 *
 * Filename:  md_nvsd_virtual_player.c
 * Date:      2008/11/17b
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */
#define USE_SPRITNF

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#define  VALID_BURST CLIST_NUMBERS "."
#define INVALID_LICENSE_MAXSESSIONS	"0"

md_commit_forward_action_args md_virtual_player_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD subsystem did not respond"),
    mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};

int vp_log_level = LOG_INFO;
//Rate control scheme
#define RC_MBR "1"
#define RC_ABR  "2"

//Bandwidth opt - transcode ratio
#define md_bw_opt_transcode_high  "high"
#define md_bw_opt_transcode_low "low"
#define md_bw_opt_transcode_med "med"

#define md_bw_opt_transcode_option ","  md_bw_opt_transcode_high "," \
                                        md_bw_opt_transcode_low "," \
                                    md_bw_opt_transcode_med"," \

//Bandwidth opt - switch rate options
#define md_bw_opt_switch_rate_higher  "higher"
#define md_bw_opt_switch_rate_lower "lower"

#define md_bw_opt_switch_rate_option ","  md_bw_opt_switch_rate_higher "," \
                                        md_bw_opt_switch_rate_lower "," \

//Valid rate control qeury rate options.
#define md_rc_query_ru_kbps "Kbps"
#define md_rc_query_ru_KBps "KBps"
#define md_rc_query_ru_Mbps "Mbps"
#define md_rc_query_ru_MBps "MBps"

#define md_rc_query_units "," md_rc_query_ru_kbps ","\
                              md_rc_query_ru_KBps "," \
                              md_rc_query_ru_Mbps "," \
                              md_rc_query_ru_MBps ","\

enum   {
    VP_GENERIC = 0,
    VP_BREAK, //deprecated
    VP_QSS,//deprecated
    VP_YAHOO,
    VP_SMOOTHFLOW,//deprecated
    VP_YOUTUBE,
    VP_SSPUB,
    VP_FLASHPUB
} player_type;

#ifdef USE_MFD_LICENSE
static const char *nkn_mfd_lic_binding = "/nkn/mfd/licensing/config/mfd_licensed";
#endif /* USE_MFD_LICENSE */

const char *match_type_choices = ",uri_query,uri_hdr,uol,uri_regex";
const char *faststart_type_choices = ",uri_query,uri_hdr,uol,uri_regex,byte_offset";
const char *max_session_rate_choices = ",uri_query,uri_hdr,uol,uri_regex,rate";
const char *seek_choices = ",uri_query,uri_hdr,uol,uri_regex";
const char *full_download_choices = ",uri_query,uri_hdr,uol,uri_regex";
const char *assured_flow_choices = ",uri_query,uri_hdr,uol,uri_regex,rate";
const char *smooth_flow_choices = ",uri_query,uri_hdr,uol,uri_regex";
const char *profile_rate_choices = ",uri_query,uri_hdr,uol,uri_regex";


/* ------------------------------------------------------------------------- */
static md_upgrade_rule_array *md_nvsd_virtual_player_ug_rules = NULL;

int md_nvsd_virtual_player_init(const lc_dso_info *info, void *data);

static int md_nvsd_virtual_player_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_nvsd_virtual_player_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_virtual_player_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);


static int
md_nvsd_virtual_player_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg);

static int
md_nvsd_vp_af_to_rc_map(const mdb_db *old_db,
	mdb_db *inout_new_db, void *arg);
static int
md_nvsd_vp_deprecate_player(const mdb_db *old_db,
	mdb_db *inout_new_db, uint32 type);

static int
md_nvsd_vp_set_default_type_1(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg,
        const mdb_db_change *change);
static int
md_nvsd_vp_set_default_type_2(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg,
        const mdb_db_change *change);
static int
md_nvsd_vp_set_default_type_4(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg,
        const mdb_db_change *change);

static int
md_bw_opt_ftype_check(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

int md_vp_set_assured_flow_auto_config(md_commit *commit,
			mdb_db **db,
			const char *action_name,
			bn_binding_array *params,
			void *cb_arg);
static int
md_check_af_delivery_overhead_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

static int
md_check_rc_burst_range(md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);

int md_vp_set_default_config(md_commit *commit,
			mdb_db **db,
			const char *action_name,
			bn_binding_array *params,
			void *cb_arg);
int
md_virtual_player_default_type_0(md_commit *commit,
				 mdb_db **db,
				 const char *name);

int
md_virtual_player_default_type_1(md_commit *commit,
				 mdb_db **db,
				 const char *name);

int
md_virtual_player_default_type_2(md_commit *commit,
				 mdb_db **db,
				 const char *name);

int
md_virtual_player_default_type_3(md_commit *commit,
				 mdb_db **db,
				 const char *name);
int
md_virtual_player_default_type_4(md_commit *commit,
				 mdb_db **db,
				 const char *name);
int
md_virtual_player_default_type_5(md_commit *commit,
				 mdb_db **db,
				 const char *name);


int
md_virtual_player_default_type_7(md_commit *commit,
				 mdb_db **db,
				 const char *name);
/* ------------------------------------------------------------------------- */
int
md_nvsd_virtual_player_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-virtual-player", 17,
	    "/nkn/nvsd/virtual_player", "/nkn/nvsd/virtual_player/config",
	    modrf_all_changes,
	    md_nvsd_virtual_player_commit_side_effects, NULL,
	    md_nvsd_virtual_player_commit_check, NULL,
	    NULL, NULL,
	    0, 0,
	    md_nvsd_virtual_player_upgrade_downgrade, // adding new upgrade function to handle type change for break.
	    &md_nvsd_virtual_player_ug_rules,
	    md_nvsd_virtual_player_add_initial, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    /*------------------------------------------------------------------------
     *      NODE DEFINITIONS FOR COMMON AS WELL AS CUSTOM VPE
     *----------------------------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*";
    node->mrn_value_type =      bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_VPLAYER_LENGTH;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Virtual player name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/type";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = (uint32)(-1);
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Virtual player type";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/max_session_rate";
    node->mrn_value_type =      bt_uint32;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_initial_value_int =   0;
    node->mrn_description =     "Max Session Rate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/digest";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "md-5";
    node->mrn_limit_str_choices_str = ",md-5,sha-1,crc,checksum";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Hash verify digest scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/data/string";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Data string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/data/uri_offset";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI offset";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/data/uri_len";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI Length";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/secret/value";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Secret word/string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/secret/type";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "append";
    node->mrn_limit_str_choices_str = ",append,prefix";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Secret type??";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*-----------------------------------------------------------------------*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/match/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI query-parm-string";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*Adding expiry time  verify and url type */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/expiry_time/epochtime";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Expiry time query param name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/hash_verify/url_type";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "absolute-url";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URL type - relative-uri, absolute-url, object-name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/fast_start/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/fast_start/valid";
    node->mrn_value_type =      bt_link;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Link to node that was last set/edited. "
	"Contains the node name.";
    err = mdm_add_node(module, &node);
    bail_error(err);
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/fast_start/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Query paramater string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/fast_start/size";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * This nodehas been deleted as per  Bug 5919
     * Bug:10940: Feature is needed, adding this back.
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/fast_start/time";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "fast start time";
    err = mdm_add_node(module, &node);
    bail_error(err);
    /*----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Query parameter string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/length/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Seek length parameter string";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/enable_tunnel";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable Tunnel";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/flv-type";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Seek flv type string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/mp4-type";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Seek MP4 type string";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/full_download/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/full_download/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =        "/nkn/nvsd/virtual_player/config/*/full_download/always";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Flag to indicate whether always to do a full"
	" download or not. true - always, "
	"false - check \"match\" node";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =        "/nkn/nvsd/virtual_player/config/*/full_download/match";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Flag to indicate whether always to do a full"
	" download or not. true - always, "
	"false - check \"match\" node";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/full_download/uri_hdr";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 256;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Header to match";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     *Rate-control options
     *Will override the assured-flow option
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/valid";
    node->mrn_value_type =      bt_link;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Link to node that was last set/edited. "
	"Contains the node name.";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/query/str";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/query/rate";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_choices_str = md_rc_query_units;
    node->mrn_limit_str_max_chars = 8;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Rate value for the Query string value";
    node->mrn_bad_value_msg =      N_("error: Invalid rate units. value can be Kbps, KBps, Mbps, MBps.");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/static/rate";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Static rate in kbps";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/auto_detect";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "auto rate detection";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/scheme";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Scheme max-bit-rate/assured-flow-rate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_control/burst";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "1.1";
    node->mrn_limit_str_charlist = VALID_BURST;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "burst-factor for the flow";
    node->mrn_check_node_func = md_check_rc_burst_range;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/smooth_flow/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/smooth_flow/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*-----------------------------------------------------------------------*/



    /*-----------------------------------------------------------------------*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_map/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "true";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_map/match/*";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 256;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Match string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_map/match/*/rate";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "rate in kbps";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_map/match/*/query_string";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Query String";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_map/match/*/uol/offset";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI Offset";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/rate_map/match/*/uol/length";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI Length";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/virtual_player/actions/fast_start/uri_query";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_virtual_player_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "value";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Bug 5919 - removed this option
     * Bug 10490:Feature needed in 12.1. Uncommenting code.
     */
    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/virtual_player/actions/fast_start/time";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_virtual_player_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "value";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/virtual_player/actions/fast_start/size";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_virtual_player_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "value";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*-----------------------------------------------------------------------
     * TYPE 3/4 ADDITIONS
     * MODULE VERSION: 2
     *----------------------------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/health_probe/uri_query";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Health probe query string";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/health_probe/match";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Health probe match string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/health_probe/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "inidcates whether this node is configured or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/req_auth/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "inidicates whether this parameter node is configured or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/req_auth/digest";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "md-5";
    node->mrn_limit_str_choices_str = ",md-5,sha-1,crc,checksum";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "digest scheme";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =        "/nkn/nvsd/virtual_player/config/*/req_auth/stream_id/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Data string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =        "/nkn/nvsd/virtual_player/config/*/req_auth/auth_id/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Data string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/req_auth/secret/value";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Shared Secret word/string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/req_auth/time_interval";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Time interval ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/req_auth/match/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI query-parm-string";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/control_point";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    //node->mrn_limit_str_choices_str = ",player,server";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Control Point string for Type 4";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/signals/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "flag to indicate if this node is configured";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/signals/session_id/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI query for signals-sessionid";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/signals/state/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI Query for Signals-state";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/signals/profile/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "URI query for signals-profile";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/prestage/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Flag to indicate whther prestage config is used or not";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/prestage/indicator/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Prestage indicator query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/prestage/namespace/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Prestage name query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/quality-tag";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "QualityLevels";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Quality-Tag query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/fragment-tag";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "Fragments";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Fragments-Tag query string";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/segment-tag";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Segment tag";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/segment-delimiter-tag";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "segment tag delimiter";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/flash-fragment-tag";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Flashstream-pub fragment tag";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*-----------------------------------------------------------------------*/

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/virtual_player/actions/set_default";
    node->mrn_node_reg_flags = 	mrf_flags_reg_action;
    node->mrn_cap_mask = 	mcf_cap_action_basic;
    node->mrn_action_config_func = md_vp_set_default_config;
    node->mrn_action_arg = 	NULL;
    node->mrn_description = 	"Set default for a player type";
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "type";
    node->mrn_actions[1]->mra_param_type = bt_uint32;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Added in module version 4
     * BZ 2728
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/video_id/uri_query";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Type 5: video-id";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/video_id/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Type 5: video-id";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Added in module version 5
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/video_id/format_tag";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Type 5: video-id format-tag";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/bw_opt/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Enable State";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 16;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_check_node_func = md_bw_opt_ftype_check;
    node->mrn_limit_wc_count_max = 2;
    node->mrn_bad_value_msg =      N_("error:Two file types are already configured.");
    node->mrn_description =     "Bandwidth optimization for File type MP4/FLV";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*/transcode_ratio";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 16;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_limit_str_choices_str = md_bw_opt_transcode_option;
    node->mrn_bad_value_msg =      N_("error: Invalid transcode ratio.Transcode ratio value can be low, med, high.");
    //    node->mrn_check_node_func = md_bw_opt_transcode_check;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Transcode compression for the file-type";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*/hotness_threshold";
    node->mrn_value_type =      bt_int32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Hotness threshold afte which transcode is activated";
    node->mrn_bad_value_msg =      N_("error: Hotness threshold value must be between 0 and 100");
    err = mdm_add_node(module, &node);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*/switch_rate";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_limit_str_max_chars = 16;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_limit_str_choices_str = md_bw_opt_switch_rate_option;
    node->mrn_bad_value_msg =      N_("error: Invalid switch rate option.Switch rate value can be lower, higher.");
    //   node->mrn_check_node_func = md_bw_opt_switch_check;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Switching rate  for a file type";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*/switch_limit";
    node->mrn_value_type =      bt_int32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  16000;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Limit bitrate value for the switching rate";
    node->mrn_bad_value_msg =      N_("error: Bit-rate cap value range must be between 0 and 16000kbps");
    err = mdm_add_node(module, &node);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/virtual_player/config/*/seek/use_9byte_hdr";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "byte offset option for seek-flv-type";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = md_upgrade_rule_array_new(&md_nvsd_virtual_player_ug_rules);
    bail_error(err);
    ra = md_nvsd_virtual_player_ug_rules;

    /*! Added in module version 3
     */
    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/virtual_player/config/*/seek/length/uri_query";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/video_id/uri_query";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/video_id/active";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

#if 1
    /*! Deleting the chunk string param in module version 4
     */
    MD_NEW_RULE(ra,3,4);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =		"/nkn/nvsd/virtual_player/config/*/signals/chunk/uri_query";
    MD_ADD_RULE(ra);
#endif

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/video_id/format_tag";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/seek/enable_tunnel";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/quality-tag";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"QualityLevels";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/fragment-tag";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"Fragments";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/hash_verify/expiry_time/epochtime";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/hash_verify/url_type";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"absolute-url";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/seek/mp4-type";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/seek/flv-type";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =		"/nkn/nvsd/virtual_player/config/*/fast_start/time";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/segment-tag";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/segment-delimiter-tag";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/flash-fragment-tag";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/bw_opt/active";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*/transcode_ratio";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/bw_type/ftype/*/hotness_threshold";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str = 	"0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/bw_opt/ftype/*/switch_rate";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/bw_type/ftype/*/switch_limit";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str = 	"0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/seek/use_9byte_hdr";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/assured_flow/use-mbr";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/assured_flow/overhead";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"1.25";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    /*Adding nodes for  rate-control-video-pacing */
    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/active";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/burst";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"1.1";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/valid";
    rule->mur_new_value_type =  bt_link;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/auto_detect";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/scheme";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str = 	"0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/query/str";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/query/rate";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/rate_control/static/rate";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str = 	"0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    /*Adding the fast-start time option*/

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/virtual_player/config/*/fast_start/time";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str = 	"0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
const bn_str_value md_nvsd_virtual_player_initial_values[] = {

};


/* ------------------------------------------------------------------------- */
static int
md_nvsd_virtual_player_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_nvsd_virtual_player_initial_values,
         sizeof(md_nvsd_virtual_player_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    bn_binding_free(&binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_virtual_player_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 i = 0;
    uint32 idx = 0;
    uint32 num_ns = 0;
    tbool found = false;
    uint32 num_changes = 0;
    char *node_name = NULL;
    tbool t_license = false;
    const char *vp_name = NULL;
    tstring *ns_vp_name = NULL;
    tstring *t_max_session_rate = NULL;
    tstr_array *t_namespace = NULL;
    tstr_array *t_name_parts= NULL;
    const mdb_db_change *change = NULL;
    uint32 uiType = 0;


    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (bn_binding_name_parts_pattern_match_va
                    (change->mdc_name_parts, 0, 5,
                     "nkn", "nvsd", "virtual_player", "config", "*"))
        {
            if (change->mdc_change_type == mdct_delete) {
                vp_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);

                err = mdb_get_tstr_array(commit,
                        new_db, "/nkn/nvsd/namespace",
                        NULL, false, 0, &t_namespace);
                bail_error_null(err, t_namespace);


                if (t_namespace == NULL) {
                    /* No namespaces found (or configured).
                     * Good to delete this virtual player
                     */
                    err = md_commit_set_return_status(commit, 0);
                    bail_error(err);
                }

                num_ns = tstr_array_length_quick(t_namespace);
                for(idx = 0; idx < num_ns; idx++) {
		    node_name = smprintf ("/nkn/nvsd/namespace/%s/virtual_player",
                            		tstr_array_get_str_quick(t_namespace, idx));
                    err = mdb_get_node_value_tstr(commit,
                            new_db,
                            node_name,
                            mdqf_sel_class_enter_passthrough,
                            &found,
                            &ns_vp_name);
                    bail_error(err);

                    if (found && (strcmp(vp_name, ts_str(ns_vp_name)) == 0)) {
                        /* Prepare to get namespace name for pretty print!
                         */
                        err = ts_tokenize_str(
                                tstr_array_get_str_quick(t_namespace, idx),
                                '/', '\0', '\0', 0, &t_name_parts);
                        bail_error(err);

                        err = md_commit_set_return_status_msg_fmt(
                                commit, 1,
                                _("Virtual player \"%s\" is associated with "
                                  "namespace \"%s\". Cannot delete this player"),
                                vp_name,  tstr_array_get_str_quick(t_namespace, idx));
                        bail_error(err);
                        goto bail;
                    }
                    found = false;
                    ts_free(&ns_vp_name);
                    ns_vp_name = NULL;
                }
            }
        }
#ifdef USE_MFD_LICENSE
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "virtual_player", "config", "*", "max_session_rate"))
			&& (mdct_modify == change->mdc_change_type))
        {

	    /*
	    *  Add the license check
	    *  as of March 16th, 2009 license is only controlling
	    *  number of concurrent sessions and max bandwidth per session
	    *  If no license then sessions is set at 200 and max bandwidth
	    *  is set to 200kbps
	    */

	    /* First get the license node */
	    err = mdb_get_node_value_tbool(commit, new_db, nkn_mfd_lic_binding,
					    0, &found, &t_license);
	    bail_error(err);
	
	    if (t_license)
	    {
		    /* License exists hence nothing to check */
		    err = md_commit_set_return_status(commit, 0);
		    goto bail;
	    }
	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
				0, &found, &t_max_session_rate);
	    bail_error(err);
	    bail_null(t_max_session_rate);
			
	    /* Check if the value is default for no license */
	    if (strcmp (ts_str(t_max_session_rate), INVALID_LICENSE_MAXSESSIONS))
	    {
		err = md_commit_set_return_status_msg(commit, 1,
			"error: license does not exist to change this parameter");
		bail_error(err);
		goto bail;
	    }
        }
#endif /* USE_MFD_LICENSE */
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

 bail:
    ts_free(&t_max_session_rate);
    tstr_array_free(&t_name_parts);
    tstr_array_free(&t_namespace);
    ts_free(&ns_vp_name);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_virtual_player_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    uint32 i = 0;
    uint32 num_changes = 0;
    const mdb_db_change *change = NULL;
    const char *child = NULL;
    tstr_array *t_namespace = NULL;
    uint32 num_ns = 0;
    uint32 idx = 0;
    const bn_attrib *new_val = NULL;
    uint32 type = (uint32) -1;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	/* Check if its a virtual player node change */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
			"nkn", "nvsd", "virtual_player", "config", "*", "assured_flow", "*"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    /* we arent intereseted in "active" node - since its not a config
	     * node
	     */
	    child = tstr_array_get_str_quick(change->mdc_name_parts, 6);
	    if ( (strcmp(child, "active") != 0)
		    && (strcmp(child, "valid") != 0))
	    {
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
			bt_link, ts_str(change->mdc_name),
			"/nkn/nvsd/virtual_player/config/%s/assured_flow/valid",
			tstr_array_get_str_quick(change->mdc_name_parts, 4));
	    }
	}
    }
bail:
    return err;
}

int md_vp_set_default_config(md_commit *commit,
			mdb_db **db,
			const char *action_name,
			bn_binding_array *params,
			void *cb_arg)
{
    int err = 0;
    const bn_binding *binding = NULL;
    uint32 type = (uint32) (-1);
    char *name = NULL;

    err = bn_binding_array_get_binding_by_name(params, "name", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &name);
    bail_error_null(err, name);

    err = bn_binding_array_get_binding_by_name(params, "type", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_uint32(binding, ba_value, NULL, &type);
    bail_error(err);
    if (type == (uint32)(-1)) {
	goto bail;
    }

    /* All required variables retrieved */
    switch(type)
    {
	case 0:
	    err = md_virtual_player_default_type_0(commit, db, name);
	    bail_error(err);
	    break;
	case 1:
	    err = md_virtual_player_default_type_1(commit, db, name);
	    bail_error(err);
	    break;
	case 2:
	    err = md_virtual_player_default_type_2(commit, db, name);
	    bail_error(err);
	    break;
	case 3:
	    err = md_virtual_player_default_type_3(commit, db, name);
	    bail_error(err);
	    break;
	case 4:
	    err = md_virtual_player_default_type_4(commit, db, name);
	    bail_error(err);
	    break;
	case 5: /*youtube default settings*/
	    err = md_virtual_player_default_type_5(commit, db, name);
	    bail_error(err);
	    break;
	case 7: /* flashstream-pub default settings*/
	    err = md_virtual_player_default_type_7(commit, db, name);
	    bail_error(err);
	    break;
	default:
	    break;
    }

bail:
    safe_free(name);
    return err;

}


int md_virtual_player_default_type_0(md_commit *commit,
		mdb_db **db,
		const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);

    s = smprintf("%s/seek/flv-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "byte-offset");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/mp4-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "time-msec");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);
bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int md_virtual_player_default_type_1(md_commit *commit,
		mdb_db **db,
		const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *q = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);

    s = smprintf("%s/seek/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "ec_seek");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/flv-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "byte-offset");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/mp4-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "time-msec");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);
bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}


int md_virtual_player_default_type_2(md_commit *commit,
		mdb_db **db,
		const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *q = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    uint32 i = 0;
    tstring *ret_msg = NULL;
    struct __map__ {
	const char *match;
	uint32	   rate;
    } ratemap [] = {
	{"00", 150}, {"01", 180}, {"02", 270},
	{"03", 330}, {"04", 420}, {"05", 470},
	{"06", 520}, {"07", 575}, {"08", 700},
	{"09", 800}, {"0A", 900}, {"0B", 1300},
	{"0C", 1750}, {"0D", 1920},
    };

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);

    for(i = 0; i < sizeof(ratemap)/sizeof(struct __map__); i++) {
	s = smprintf("%s/rate_map/match/%s", p, ratemap[i].match);
	bail_null(s);
	err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, ratemap[i].match);
	bail_error_null(err, binding);
	err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
	bail_error(err);
	bn_binding_free(&binding);
	safe_free(s);

	s = smprintf("%s/rate_map/match/%s/rate", p, ratemap[i].match);
	bail_null(s);
	err = bn_binding_new_uint32(&binding, s, ba_value, 0, ratemap[i].rate);
	bail_error_null(err, binding);
	err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
	bail_error(err);
	bn_binding_free(&binding);
	safe_free(s);
    }

    s = smprintf("%s/rate_map/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);


bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;

}


int
md_virtual_player_default_type_4(md_commit *commit,
				 mdb_db **db,
				 const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *q = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);


    s = smprintf("%s/control_point", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "player");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/signals/session_id/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "sid");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/signals/state/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "sf");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/signals/profile/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "pf");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/signals/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);


    s = smprintf("%s/seek/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "toff");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/flv-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "byte-offset");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/mp4-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "time-msec");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/hash_verify/secret/type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "prefix");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/hash_verify/match/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "h");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);

bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int
md_virtual_player_default_type_3(md_commit *commit,
				 mdb_db **db,
				 const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *q = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);

    s = smprintf("%s/req_auth/digest", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "md-5");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/req_auth/stream_id/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "streamid");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/req_auth/auth_id/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "authid");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/req_auth/secret/value", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "ysecret");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/req_auth/time_interval", p);
    bail_null(s);
    err = bn_binding_new_uint32(&binding, s, ba_value, 0, 15);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/req_auth/match/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "ticket");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/req_auth/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/health_probe/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "no-cache");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/health_probe/match", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "1");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/health_probe/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);


bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int
md_virtual_player_default_type_5(md_commit *commit,
				 mdb_db **db,
				 const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);

    s = smprintf("%s/seek/uri_query", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "begin");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/flv-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "time-msec");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/mp4-type", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "time-msec");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/seek/active", p);
    bail_null(s);
    err = bn_binding_new_tbool(&binding, s, ba_value, 0, true);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);


bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

/*
 * Setting the default values for flash-stream pub
 */
int md_virtual_player_default_type_7(md_commit *commit,
		mdb_db **db,
		const char *name)
{
    int err = 0;
    bn_request *req = NULL;
    char *p = NULL;
    char *s = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    p = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(p);

    s = smprintf("%s/flash-fragment-tag", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "Frag");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/segment-tag", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "Seg");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);
    safe_free(s);

    s = smprintf("%s/segment-delimiter-tag", p);
    bail_null(s);
    err = bn_binding_new_str(&binding, s, ba_value, bt_string, 0, "-");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);


bail:
    safe_free(p);
    safe_free(s);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

static int
md_nvsd_virtual_player_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg)
{
    int err = 0;
    tstr_array *vp_tstr_bindings = NULL;
    const char *old_binding_name = NULL;
    unsigned int arr_index ;
    tbool found = false;
    tstring *vptype = NULL;

    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);
    /*
     * New upgrade rule to convert all the break  type virtual players
     * to generic virtual player
     */
    if ((to_module_version == 11)) {
	lc_log_debug(vp_log_level, _("Type change for  break players\n"));
	err = mdb_get_matching_tstr_array( NULL,
		inout_new_db,
		"/nkn/nvsd/virtual_player/config/*/type",
		0,
		&vp_tstr_bindings);
	bail_error(err);
	bail_null(vp_tstr_bindings);
	lc_log_debug(vp_log_level, "total vp_str_bindings %d", tstr_array_length_quick(vp_tstr_bindings)  );

	for ( arr_index = 0; arr_index < tstr_array_length_quick(vp_tstr_bindings); ++arr_index)
	{
	    tstring *type = NULL;
	    old_binding_name = tstr_array_get_str_quick(
		    vp_tstr_bindings, arr_index);
	    err = mdb_get_node_value_tstr(NULL, inout_new_db, old_binding_name,
		    0, &found, &type);
	    bail_error(err);
	    if (found && (ts_length(type) > 0)){
		int vp_type = atoi(ts_str(type));
		if(vp_type == 1) { // If the  virtual player type was break then set it to generic in upgrade
		    //generic is type 0, break is type 1
		    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			    0, bt_uint32,
			    "0",
			    "%s",
			    old_binding_name);
		    bail_error(err);
		    lc_log_debug(vp_log_level, "changed type for:%s", old_binding_name);
		}
	    }
	    ts_free(&type);
	    bail_error(err);
	}
    }
    /*
     *  New upgrade rule to convert all the smoothflow  type virtual players
     * to generic virtual player
     */
    if ((to_module_version == 13)) {
	lc_log_debug(vp_log_level, _("Type change for  smoothflow players\n"));
	err = mdb_get_matching_tstr_array( NULL,
		inout_new_db,
		"/nkn/nvsd/virtual_player/config/*/type",
		0,
		&vp_tstr_bindings);
	bail_error(err);
	bail_null(vp_tstr_bindings);
	lc_log_debug(vp_log_level, "total vp_str_bindings %d", tstr_array_length_quick(vp_tstr_bindings)  );

	for ( arr_index = 0; arr_index < tstr_array_length_quick(vp_tstr_bindings); ++arr_index)
	{
	    old_binding_name = tstr_array_get_str_quick(
		    vp_tstr_bindings, arr_index);
	    err = mdb_get_node_value_tstr(NULL, inout_new_db, old_binding_name,
		    0, &found, &vptype);
	    bail_null(vptype);
	    bail_error(err);
	    if (found && (ts_length(vptype) > 0)){
		int vp_type = atoi(ts_str(vptype));
		if(vp_type == 4) { // If the  virtual player type was smoothflow then set it to generic in upgrade
		    //generic is type 0, smoothflow is type 4
		    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			    0, bt_uint32,
			    "0",
			    "%s",
			    old_binding_name);
		    bail_error(err);
		    lc_log_debug(vp_log_level, "changed type for:%s", old_binding_name);
		}
	    }
	}
    }

    /*
     *  New upgrade rule to map the assured flow nodes to rate-control node
     *  values.
     *  The mapping for the old fields to the new ones is as follows
     *  -	delivery-overhead maps to burst-factor
     *  -	rate maps to static
     *  -	use-mbr maps to max-bit-rate option
     *  -	auto maps to auto-detect
     *  -	assured-flow maps to rate-control
     *
     */
    if ((to_module_version == 17)) {
	lc_log_debug(vp_log_level, _("Converting assured flow nodes to rate-control\n"));
	err = md_nvsd_vp_af_to_rc_map(old_db, inout_new_db, arg);
	bail_error(err);

	err = md_nvsd_vp_deprecate_player(old_db, inout_new_db, VP_QSS);
	bail_error(err);
    }

bail:
    ts_free(&vptype);
    tstr_array_free(&vp_tstr_bindings);
    return err;
}

static int
md_bw_opt_ftype_check(md_commit *commit,
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
    const char *vpname = NULL;
    tstring *ftype = NULL;
    int val = 0;

    vpname = tstr_array_get_str_quick(node_name_parts, 4);
    bail_null(vpname);

    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_tstr(attrib, NULL, bt_string, NULL, &ftype);
    bail_error(err);
    if ( !(ts_equal_str(ftype, "flv", false))&& !(ts_equal_str(ftype, "mp4", false))) {
	err = md_commit_set_return_status_msg_fmt
	    (commit, 1, _("Invalid file type specified '%s'.Bandwidth optimization is available"
			  "for only FLV and MP4 file type\n"), ts_str(ftype));
	bail_error(err);
    }

bail:
    ts_free(&ftype);
    return err;
}

static int
md_check_rc_burst_range(md_commit *commit,
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
    tstring *value = NULL;
    int val = 0;
    float f_burst = 0;
    char *end = NULL;


    if ( (change_type != mdct_add) && (change_type != mdct_modify) ) {
	goto bail;
    }

    bail_null(new_attribs);
    attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
    bail_null(attrib);

    err = bn_attrib_get_tstr(attrib, NULL, bt_string, NULL, &value);
    bail_error(err);

    f_burst = strtof(ts_str(value), &end);

    if ( f_burst < 1.0 || f_burst > 3.0 ) {
	err = md_commit_set_return_status_msg_fmt
	    (commit, 1, _("Invalid burst factor. Valid range is 1.0 - 3.0\n"));
	bail_error(err);
    }

bail:
    ts_free(&value);
    return err;
}


static int
md_nvsd_vp_af_to_rc_map(const mdb_db *old_db,
	mdb_db *inout_new_db, void *arg)
{
    int err = 0;
    tstr_array *vp_tstr_bindings = NULL;
    tstr_array *binding_parts= NULL;
    const char *old_binding_name = NULL;
    unsigned int i ;
    tbool found = false;
    tbool is_detect = false;
    tbool is_rate = false;
    tbool is_query = false;
    tbool is_auto = false;
    tbool is_active = false;
    const char *vpname = NULL;
    tstring *rate =NULL;
    tstring *doverhead = NULL;
    tstring *query = NULL;
    tstring *vlink = NULL;
    tbool is_mbr = false;
    node_name_t src = {0};
    node_name_t dest = {0};
    node_name_t vnode = {0};

    err = mdb_get_matching_tstr_array( NULL,
	    inout_new_db,
	    "/nkn/nvsd/virtual_player/config/*/assured_flow/auto",
	    0,
	    &vp_tstr_bindings);
    bail_error(err);
    bail_null(vp_tstr_bindings);

    for ( i = 0; i < tstr_array_length_quick(vp_tstr_bindings); i++ ) {
	tstr_array_free(&binding_parts);
	old_binding_name = tstr_array_get_str_quick(
		vp_tstr_bindings, i);
	err = bn_binding_name_to_parts (old_binding_name,
		&binding_parts, NULL);
	bail_error(err);
	vpname = tstr_array_get_str_quick (binding_parts, 4);
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/auto", vpname);
	snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect", vpname);
	err = mdb_get_node_value_tbool(NULL, inout_new_db, src, 0, &found, &is_auto);
	bail_error(err);
	if(found) {
	    if(is_auto) {
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_bool, "true", "%s", dest);
		bail_error(err);
	    }
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_bool, "false", "%s", src);
	    bail_error(err);
	}

	/*Set the active nodes*/
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/active", vpname);
	snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/active", vpname);
	err = mdb_get_node_value_tbool(NULL, inout_new_db, src, 0, &found, &is_active);
	bail_error(err);
	if(found) {
	    if(is_active) {
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_bool, "true", "%s", dest);
		bail_error(err);
	    }
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_bool, "false", "%s", src);
	    bail_error(err);
	}

	/*Setting the scheme*/
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/use-mbr", vpname);
	snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", vpname);
	err = mdb_get_node_value_tbool(NULL, inout_new_db, src, 0, &found, &is_mbr);
	bail_error(err);
	if(found) {
	    if(is_mbr) {
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_uint32, RC_MBR, "%s", dest);
		bail_error(err);
	    }else {
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_uint32, RC_ABR, "%s", dest);
		bail_error(err);
	    }
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_bool, "false", "%s", src);
	    bail_error(err);
	}

	/*Setting static rate*/
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/rate", vpname);
	snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", vpname);
	ts_free(&rate);
	err = mdb_get_node_value_tstr(NULL, inout_new_db, src, 0, &found, &rate);
	bail_error(err);
	if(found) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_uint32, ts_str(rate), "%s", dest);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_uint32, "0", "%s", src);
	    bail_error(err);
	}

	/*Setting the query value*/
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/uri_query", vpname);
	snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str", vpname);
	ts_free(&query);
	err = mdb_get_node_value_tstr(NULL, inout_new_db, src, 0, &found, &query);
	bail_error(err);
	if(found) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_string, ts_str(query), "%s", dest);
	    bail_error(err);
	    /*Assured flow worked with Kilo bytes so setting value to KBps*/
	    snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate", vpname);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_string, "KBps", "%s", dest);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_string, "", "%s", src);
	    bail_error(err);
	}

	/*setting the delivery overhead to burst-factor*/
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/overhead", vpname);
	snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/burst", vpname);
	ts_free(&doverhead);
	err = mdb_get_node_value_tstr(NULL, inout_new_db, src, 0, &found, &doverhead);
	bail_error(err);
	if(found) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_string, ts_str(doverhead), "%s", dest);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_string, "", "%s", src);
	    bail_error(err);
	}

	/*Setting valid link*/
	snprintf(src, sizeof(src), "/nkn/nvsd/virtual_player/config/%s/assured_flow/valid", vpname);
	ts_free(&vlink);
	err = mdb_get_node_value_tstr(NULL, inout_new_db, src, 0, &found, &vlink);
	bail_error(err);
	if(found && vlink){
	    is_detect = ts_has_suffix_str(vlink, "auto",false);
	    is_rate = ts_has_suffix_str(vlink, "rate",false);
	    is_query = ts_has_suffix_str(vlink, "uri_query",false);
	    snprintf(dest, sizeof(dest), "/nkn/nvsd/virtual_player/config/%s/rate_control/valid", vpname);
	    if(is_detect) {
		snprintf(vnode, sizeof(vnode),"/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect", vpname);
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_link, vnode, "%s", dest);
		bail_error(err);
	    }else if (is_rate) {
		snprintf(vnode, sizeof(vnode),"/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", vpname);
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_link, vnode, "%s", dest);
		bail_error(err);
	    }else if (is_query) {
		snprintf(vnode, sizeof(vnode),"/nkn/nvsd/virtual_player/config/%s/rate_control/query/str", vpname);
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_link, vnode, "%s", dest);
		bail_error(err);
	    }else {
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_link, "", "%s", dest);
		bail_error(err);
	    }
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_link, "", "%s", src);
	    bail_error(err);
	}
    }


bail:
    ts_free(&doverhead);
    ts_free(&query);
    ts_free(&rate);
    ts_free(&vlink);
    tstr_array_free(&vp_tstr_bindings);
    tstr_array_free(&binding_parts);
    return err;
}
static int
md_nvsd_vp_deprecate_player(const mdb_db *old_db,
	mdb_db *inout_new_db, uint32 type)
{
    int err = 0;
    tstr_array *vp_tstr_bindings = NULL;
    const char *old_binding_name = NULL;
    uint32 arr_index = 0;
    tbool found = false;


    lc_log_debug(vp_log_level, _("Deprecating players\n"));
    err = mdb_get_matching_tstr_array( NULL,
	    inout_new_db,
	    "/nkn/nvsd/virtual_player/config/*/type",
	    0,
	    &vp_tstr_bindings);
    bail_error(err);
    if(!vp_tstr_bindings) {
	goto bail;
    }
    lc_log_debug(vp_log_level, "total vp_str_bindings %d", tstr_array_length_quick(vp_tstr_bindings)  );
    for ( arr_index = 0; arr_index < tstr_array_length_quick(vp_tstr_bindings); ++arr_index)
    {
	uint32 vptype = 0;
	old_binding_name = tstr_array_get_str_quick(
		vp_tstr_bindings, arr_index);
	found = false;
	err = mdb_get_node_value_uint32(NULL, inout_new_db, old_binding_name,
		0, &found, &vptype);
	bail_error(err);
	if (found ){
	    if(vptype == type) { // If the  virtual player type was qss-streamlet then set it to generic in upgrade
		//generic is type 0, qss-streamlet is type 2
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			0, bt_uint32,
			"0",
			"%s",
			old_binding_name);
		bail_error(err);
		lc_log_debug(vp_log_level, "changed type for:%s", old_binding_name);
	    }
	}
    }
bail:
    tstr_array_free(&vp_tstr_bindings);

    return err;
}

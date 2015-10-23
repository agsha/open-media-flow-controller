/*
 * Filename:    cli_nvsd_virtual_player_cmds.c
 * Date:        2009/01/10
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009, Nokeena Networks Inc.
 *  All rights reserved.
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "cli_module.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

#define RC_MBR "1"
#define RC_AFR "2"
#define RC_DEFAULT_BURST "1.1"

#define MAX_NODE_LENGTH 512
cli_expansion_option cli_digest_opts[] = {
    {"md-5", N_("MD-5"), (void*) "md-5"},
    //{"sha-1", N_("SHA-1"), (void*) "sha-1"},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_shared_secret_opt[] = {
    {"append", N_("Append string to URI and verify"), (void*) "append"},
    {"prefix", N_("Prefix string to URI and verify"), (void*) "prefix"},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_control_point_opts[] = {
    {"player", N_("Configure control point as player"), (void *) "player"},
    {"server", N_("Configure control point as server"), (void *) "server"},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_vp_type_opts[] = {
    {"generic", N_("Create a Type 0 Player: Supports AssuredFlow, Max-Bandwidth, Fast-start, Full-download,"
            " Authentication via Hash, and Seek/Scrub"), (void *) "0"},
//    {"break", N_("Create a Type 1 Player: Supports AssuredFlow, Max-Bandwidth, Fast-start, Authentication via"
  //         " Hash, and Seek/Scrub"), (void *) "1"},
 //   {"qss-streamlet", N_("Create a Type 2 Player: Supports AssuredFlow via Rate-maps, and Max-bandwidth"), (void *) "2"},
    {"yahoo", N_("Create a Type 3 Player: Supports AssuredFlow, Max-Bandwidth, Health probes, Authentication via"
            " stream and auth-id, and Seek/Scrub"), (void *) "3"},
/*    {"smoothflow", N_("Create a Type 4 Player: Supports SmoothFlow; required for SmoothFlow functionality"), (void *) "4"},*/
    {"youtube", N_("Create a Type 5 Player: Supports Video-Id, Seek/Scrub, AssuredFlow, Fast-Start, Max-Bandwidth"), (void *) "5"},
    {"smoothstream-pub", N_("Create a Type 6 Player: Supports Smoothstream-pub; required for Smoothstream functionality"), (void *) "6"},
    {"flashstream-pub", N_("Create a Type 7 Player: Supports Flashstream-pub; required for Flashstream functionality"), (void *) "7"},

    {NULL, NULL, NULL},
};


cli_expansion_option cli_vp_mp4_type[] = {
    {"time-secs", N_("time in seconds"), NULL},
    {"time-msec", N_("time in msec"), NULL},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_vp_flv_type[] = {
    {"byte-offset", N_("byte-offset"), NULL},
    {"time-secs", N_("time-secs"), NULL},
    {"time-msec", N_("time-msec"), NULL},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_expire_time_verify_url_opt[] = {
    {"absolute-url", N_("Default mode. Includes the entire REQ for hash calculation(including query strings up to match query string"), (void*) "absolute-url"},
    {"relative-uri", N_("Only the URI-no domain, no access method used for hash calculation(including query strings up to match query string)"), (void*) "relative-uri"},
    {"object-name", N_("Only the Object name used for hash calculation(including query strings up to match query string"), (void*) "object-name"},
    {NULL, NULL, NULL},
};
cli_expansion_option cli_vp_bw_ftype[] = {
    {"mp4" , N_("MP4 file format with H.264/AAC codec"), NULL},
    {"flv" , N_("FLV file format with H.264/AAC or VP6/MP3 codec"), NULL},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_vp_bw_transcode[] = {
    {"low", N_("Configure this option to approximately get a 10-15%% savings in file size"), NULL},
    {"med", N_("Configure this option to approximately get a 30-35%% savings in file size"), NULL},
    {"high", N_("Configure this option to approximately get a 50-60%% savings in file size"), NULL},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_vp_bw_switch_rate[] = {
    {"lower", N_("Switch to lower rate"), NULL},
    {"higher", N_("Switch to higher rate"), NULL},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_vp_rc_query_ru[] = {
    {"Kbps", N_("Kilo bits/sec"), NULL},
    {"KBps", N_("Kilo Bytes/sec"), NULL},
    {"Mbps", N_("Mega bits/sec"), NULL},
    {"MBps", N_("Mega Bytes/sec"), NULL},
    {NULL, NULL, NULL},
};
const char *vp_type_names[] = {
	"generic",
	"generic", // commenting out the break player "break",
	"generic", // commenting out the qss-streamlet player
	"yahoo",
	"generic", // commenting out the smoothflow player,
	"youtube",
	"smoothstream-pub",
	"flashstream-pub"
};

enum {

    /* Maximum of 31 player types are allowed. */
    /* When a new type is defined, Define its enumerate here!
     */
    vp_type_0 =  (1 << 0),
    vp_type_1 =  (1 << 1),
    vp_type_2 =  (1 << 2),
    vp_type_3 =  (1 << 3),
    vp_type_4 =  (1 << 4),
    vp_type_5 =  (1 << 5),
    vp_type_6 =  (1 << 6),
    vp_type_7 =  (1 << 7),
    /* When new type is added.. include the bit value here.
     */
    vp_type_all = ( vp_type_0 |
                    vp_type_1 |
                    vp_type_2 |
                    vp_type_3 |
                    vp_type_4 |
                    vp_type_5 |
                    vp_type_6 |
		    vp_type_7),
    /* Never touch below this line */
    vp_type_none = (uint32)(-1),
};


int
cli_nvsd_virtual_player_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_vp_set_type(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);



int
cli_nvsd_vp_type_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_hash_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_connection_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_fast_start_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_seek_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int
cli_nvsd_vp_assured_flow_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_smooth_flow_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_rate_map_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_full_download_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_health_probe_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_control_point_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_req_auth_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_vp_signals_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_video_id_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static void
cli_nvsd_print_bw_opt_config (const char *name);

static void
cli_nvsd_print_rate_control_config (
	const char *name, const char *rc);
int 
cli_nvsd_bw_opt_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nvsd_quality_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_fragment_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_segment_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_seg_frag_delimiter_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_bw_opt_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int 
cli_nvsd_vp_rate_control_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
/*--------------------------------------------------------------------------*/
static int
cli_nvsd_vp_hash_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_connection_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_fast_start_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_seek_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_seek_flv_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_assured_flow_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_rate_map_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_full_download_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_vp_health_probe_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_vp_req_auth_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_vp_prestage_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_nvsd_bw_opt_get_file_type(
	tstr_array **ret_names,
	const char *vpname);

int
cli_nvsd_quality_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nvsd_fragment_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_segment_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_seg_frag_delimiter_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_nvsd_vp_rate_control_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
/*--------------------------------------------------------------------------*/
static int
cli_nvsd_vp_is_cmd_allowed(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params, 
        tbool *ret_allowed);


static int
cli_nvsd_vp_get_names(
        tstr_array **ret_names,
        tbool hide_display);

static int 
cli_nvsd_vp_validate(
        const char *name,
        tbool *ret_valid,
        int *ret_error);

int 
cli_vp_hash_verify_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_seek_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);
int 
cli_vp_bw_opt_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_fast_start_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);


int 
cli_vp_assured_flow_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_connection_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_health_probe_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_req_auth_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_signals_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_smooth_flow_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_prestage_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);
int 
cli_vp_full_download_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_control_point_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_rate_map_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_video_id_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int 
cli_vp_smoothstream_pub_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

int cli_vp_flashstream_pub_revmap( void *data,
	         const cli_command *cmd, const bn_binding_array *bindings,
		 const char *name, const tstr_array *name_parts,
		 const char *value, const bn_binding *binding,
		 cli_revmap_reply *ret_reply, const tstring *type,
		 const char *vp_name);


int
cli_vp_rate_control_revmap(
	void *data,
        const cli_command *cmd,
        const bn_binding_array *bindings,
        const char *name,
        const tstr_array *name_parts,
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);



/*
 * Helper function to get a player's type field 
 * and resolve it into an enum.
 */
static int
cli_nvsd_vp_type_to_mask(
        const char *player,
        uint32  *ret_mask);


int
cli_append_req_msg(bn_request *req,const char *node,
	bn_set_subop subop, bn_type type, const char *value);
/*--------------------------------------------------------------------------*/
int
cli_nvsd_virtual_player_rate_map_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_virtual_player_full_download_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_virtual_player_name_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);
int
cli_nvsd_virtual_player_seek_param_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int
cli_virtual_player_name_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);

static int 
cli_nvsd_virtual_player_get_match_strings(
        tstr_array **ret_strings,
        tbool hide_display,
        const char *name);

static int
cli_nvsd_virtual_player_validate(
        const char *name,
        const char *type,
        tbool *valid,
        int  *err);

static int
cli_nvsd_virtual_player_get_player_names(
        tstr_array **ret_names,
        tbool hide_display);

int 
cli_nvsd_virtual_player_match_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);
int
cli_nvsd_virtual_player_match_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);

static int 
cli_nvsd_virtual_player_rate_map_get_match_strings(
        tstr_array **ret_strings,
        tbool hide_display,
        const char *name);

int 
cli_nvsd_virtual_player_rate_map_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);
int
cli_nvsd_virtual_player_rate_map_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);

static int
cli_nvsd_vp_set_state(
        char *binding,
        tbool state);

int 
cli_nvsd_virtual_player_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nvsd_virtual_player_show_player(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_virtual_player_show_rate_map_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

static int
cli_nvsd_vp_show_type_2(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_1(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_0(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_3(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_4(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_5(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_6(
        const char *name,
        const bn_binding_array *bindings);

static int
cli_nvsd_vp_show_type_7(
        const char *name,
	const bn_binding_array *bindings);

static int cli_virtual_player_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_virtual_player_check_and_create(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        tbool *valid);

int 
 cli_nvsd_vp_seek_length_config(
         void *data,
         const cli_command *cmd,
         const tstr_array *cmd_line,
         const tstr_array *params);
int 
cli_nvsd_vp_seek_flv_type_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_vp_seek_mp4_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_vp_seek_tunnel_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static void
cli_nvsd_print_use_9byte_hdr (	const char *name);
/*---------------------------------------------------------------------------*/
int 
cli_nvsd_virtual_player_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
        /* This is pretty much redundant and can be skipped in your
         * modules, unless you want internationalization support.
         */
        err = cli_set_gettext_domain(GETTEXT_DOMAIN);
        bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }


    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player";
    cmd->cc_help_descr =        N_("Configure virtual player options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset virtual player options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player *";
    cmd->cc_help_exp =          N_("<player name>");
    cmd->cc_help_exp_hint =     
        N_("Choose from following list or enter new name for a player");
    cmd->cc_help_term_prefix =  
        N_("Create a new player or change configuration of existing player");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/*";
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_enter_prefix_mode;
    cmd->cc_revmap_order =      cro_virtual_player;
    cmd->cc_revmap_names =      "/nkn/nvsd/virtual_player/config/*";
    cmd->cc_revmap_callback =   cli_virtual_player_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player *";
    cmd->cc_help_exp =          N_("<player name>");
    cmd->cc_help_exp_hint =     N_("Choose from following list");
    cmd->cc_help_term =         N_("Delete virtual player from configured database");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * no";
    cmd->cc_help_descr =        
        N_("Clear/reset virtual player configuration parameters");
    cmd->cc_req_prefix_count =  2;
    CLI_CMD_REGISTER;


#if defined(VP_CMD_PREFIX)
#undef VP_CMD_PREFIX 
#endif

#define VP_CMD_PREFIX   "virtual-player * "
    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "type";
    cmd->cc_help_descr =        N_("Configure virtual player type.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "type *";
    cmd->cc_help_exp =          N_("<player type>");
    cmd->cc_help_exp_hint =     N_("Choose a player type from following list");
    cmd->cc_help_options =      cli_vp_type_opts;
    cmd->cc_help_num_options =  sizeof(cli_vp_type_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_nvsd_vp_set_type; 
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "type";
    cmd->cc_help_descr =        N_("dsdkls");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "type *";
    cmd->cc_help_exp =          N_("<player type>");
    cmd->cc_help_exp_hint =     N_("Choose a player type from following list");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    //cmd->cc_exec_callback =     cli_nvsd_vp_set_type; 
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "type * no";
    cmd->cc_help_descr =        N_("Clear/reset");
    cmd->cc_req_prefix_count =  4;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX " type * hidden";
    cmd->cc_help_descr =        N_("A hidden command that does nothing");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/


    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify"; 
    cmd->cc_help_descr =        N_("Configure hash options. Applies to generic type.");
    //cmd->cc_help_descr =        N_("Configure hash options. Applies to generic, break,and smoothflow types.");
    cmd->cc_flags =             ccf_terminal | ccf_var_args;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_hash_config; 
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | digest";
    cmd->cc_help_descr =        N_("Configure hash digest type");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | digest *"; 
    cmd->cc_help_exp =          N_("<digest type>");
    cmd->cc_help_options =      cli_digest_opts;
    cmd->cc_help_num_options =  sizeof(cli_digest_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    CLI_CMD_REGISTER;
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | data";
    cmd->cc_help_descr =        N_("Configure data options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | data *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("String");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    cmd->cc_exec_callback =     cli_nvsd_vp_hash_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | data uol";
    cmd->cc_help_descr =        N_("Configure URI offset/length");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | data uol *";
    cmd->cc_help_exp =          N_("<offset>");
    cmd->cc_help_exp_hint =     N_("URI offset");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | data uol * *";
    cmd->cc_help_exp =          N_("<length>");
    cmd->cc_help_exp_hint =     N_("URI length");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    cmd->cc_exec_callback =     cli_nvsd_vp_hash_config;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | match";
    cmd->cc_help_descr =        N_("Set match string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | match query-string-parm";
    cmd->cc_help_descr =        N_("Set match string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | match query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | shared-secret";
    cmd->cc_help_descr =        N_("Configure shared secret options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | shared-secret *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Shared secret string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | shared-secret * *";
    cmd->cc_help_options =      cli_shared_secret_opt;
    cmd->cc_help_num_options =  sizeof(cli_shared_secret_opt)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    CLI_CMD_REGISTER;
    /*
     * 0ct4-2010 :Adding expiry-time-verify parameter 
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | expiry-time-verify";
    cmd->cc_help_descr =        N_("Set expiry time since epoch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | expiry-time-verify query-string-parm";
    cmd->cc_help_descr =        N_("Set expiry time value since epoch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | expiry-time-verify query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Query string parameter, whose value is a timestamp in Standard POSIX format.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | url-type";
    cmd->cc_help_descr =        N_("Set url-type");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "hash-verify | url-type *";
    cmd->cc_help_options =      cli_expire_time_verify_url_opt;
    cmd->cc_help_num_options =  sizeof(cli_expire_time_verify_url_opt)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "hash-verify-digest";
    cmd->cc_help_descr =        N_("Disable hash-verify configuration option");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_hash_config_reset;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_4;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "connection";
    cmd->cc_help_descr =        N_("Configure connection parameters. Applies to All types.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "connection max-bandwidth";
    cmd->cc_help_descr =        N_("Configure Maximum session rate");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "connection max-bandwidth *";
    cmd->cc_help_exp =          N_("<Kbps>");
    cmd->cc_help_exp_hint =     N_("Maximum session rate, in kilo-bits-per-second");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_2 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_connection_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "connection";
    cmd->cc_help_descr =        
        N_("Clear connection parameters to their default (0 = unlimited)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_2 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_connection_config_reset;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start";
    cmd->cc_help_descr =        N_("Configure fast-start options. Applies to generic, and youtube types.");
    //cmd->cc_help_descr =        N_("Configure fast-start options. Applies to generic, break, and youtube types.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no "VP_CMD_PREFIX "fast-start";
    cmd->cc_help_descr =        N_("Disable fast-start options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_fast_start_config_reset;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start query-string-parm";
    cmd->cc_help_descr =        N_("Configure Query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_fast_start_config;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start size";
    cmd->cc_help_descr =        N_("Configure size option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start size *";
    cmd->cc_help_exp =          N_("<size-kB>");
    cmd->cc_help_exp_hint =     N_("Size in kilobytes");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_fast_start_config;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start time";
    cmd->cc_help_descr =        N_("Configure time option");
    //    cmd->cc_flags =		ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "fast-start time *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Time in seconds to start from");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_fast_start_config;
    cmd->cc_magic =             vp_type_0 | vp_type_1| vp_type_5;
    CLI_CMD_REGISTER;

    /*--------------------------------------------------------------------*/


    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config";
    cmd->cc_help_descr =        N_("Configure seek options. Applies to generic, and youtube types.");
    //cmd->cc_help_descr =        N_("Configure seek options. Applies to generic,break,smoothflow, and youtube types.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config";
    cmd->cc_help_descr =        N_("Clear seek options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config  query-string-parm";
    cmd->cc_help_descr =     N_("Clear query-string parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config  query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Query string option for seek");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_comp_callback =     cli_nvsd_virtual_player_seek_param_comp;
    cmd->cc_comp_data =         (void *)"/nkn/nvsd/virtual_player/config/%s/seek/uri_query";
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/seek/uri_query";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_callback = 	cli_nvsd_vp_seek_config_reset;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =          VP_CMD_PREFIX "seek-config query-string-parm * no";
    cmd->cc_help_descr =        N_("Clear/negate seek-config parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-length";
    cmd->cc_help_descr =     N_("Clear seek-length query-string parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-length query-string-parm";
    cmd->cc_help_descr =     N_("Clear seek-length query-string parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-length query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Clear seek-length option for seek");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_comp_callback =     cli_nvsd_virtual_player_seek_param_comp;
    cmd->cc_comp_data =         (void *)"/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query";
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/seek/length/uri_query";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_operation =    cxo_reset;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type ";
    cmd->cc_help_descr =        N_("Reset the seek-flv-type");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_name =	        "/nkn/nvsd/virtual_player/config/$1$/seek/flv-type";
    //cmd->cc_exec_value =	"byte-offset";
    //cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 =        "/nkn/nvsd/virtual_player/config/$1$/seek/use_9byte_hdr";
    //cmd->cc_exec_value2 =	"false";
    cmd->cc_exec_type2 =	bt_bool;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback = 	cli_nvsd_vp_seek_flv_config_reset;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type byte-offset";
    cmd->cc_help_descr =        N_("Reset the byte-offset use-9byte-hdr option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type byte-offset use-9byte-hdr";
    cmd->cc_help_descr =        N_("Reset the byte-offset use-9byte-hdr option");
    cmd->cc_flags =             ccf_terminal | ccf_unavailable;
    cmd->cc_exec_name =        "/nkn/nvsd/virtual_player/config/$1$/seek/use_9byte_hdr";
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek-config query-string-parm * seek-mp4-type ";
    cmd->cc_help_descr =     N_("Reset seek-mp4 type");
    //cmd->cc_help_exp =          N_("<string>");
    //cmd->cc_help_exp_hint =     N_("Clear seek-length option for seek");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_name =	        "/nkn/nvsd/virtual_player/config/$1$/seek/mp4-type";
    cmd->cc_exec_value =	"time-msec";
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_type =		bt_string;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;
#if 0

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek query-string-parm * enable-tunnel";
    cmd->cc_help_descr =        N_("Clear tunnel options under seek");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_name =	        "/nkn/nvsd/virtual_player/config/$1$/seek/enable_tunnel";
    cmd->cc_exec_value =	"false";
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek query-string-parm * seek-length";
    cmd->cc_help_descr =        N_("Clear tunnel options under seek");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek query-string-parm * seek-length query-string-parm";
    cmd->cc_help_descr =        N_("Clear tunnel options under seek");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek query-string-parm * seek-length query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Clear tunnel options under seek");
    cmd->cc_comp_callback =     cli_nvsd_virtual_player_seek_param_comp;
    cmd->cc_comp_data =         (void *)"/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query";
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "seek query-string-parm * seek-length query-string-parm * enable-tunnel";
    cmd->cc_help_descr =        N_("Clear seek options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_name =	        "/nkn/nvsd/virtual_player/config/$1$/seek/enable_tunnel";
    cmd->cc_exec_value =	"false";
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    cmd->cc_comp_callback =     cli_nvsd_virtual_player_seek_param_comp;
    cmd->cc_comp_data =         (void *)"/nkn/nvsd/virtual_player/config/%s/seek/uri_query";
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * tunnel-mode";
    cmd->cc_help_descr =        N_("Enable/Disable tunnel");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * tunnel-mode enable";
    cmd->cc_help_descr =        N_("Enable tunnel");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_name = 	"/nkn/nvsd/virtual_player/config/$1$/seek/enable_tunnel";
    cmd->cc_exec_value =	"true";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_tunnel_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * tunnel-mode disable";
    cmd->cc_help_descr =        N_("Disable tunnel");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_name = 	"/nkn/nvsd/virtual_player/config/$1$/seek/enable_tunnel";
    cmd->cc_exec_value =	"false";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_tunnel_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-length";
    cmd->cc_help_descr =        N_("Configure Seek-length query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-length query-string-parm";
    cmd->cc_help_descr =        N_("Configure Seek-length query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-length query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure seek-length query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_length_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type";
    cmd->cc_help_descr =        N_("Configure Seek-flv type");
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure seek flv type");
    cmd->cc_help_options 	=	cli_vp_flv_type;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_flv_type) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_flv_type_config;
    CLI_CMD_REGISTER;
#endif
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type byte-offset";
    cmd->cc_help_descr =        N_("offset in terms of bytes");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_flv_type_config;
    cmd->cc_exec_name = 	"/nkn/nvsd/virtual_player/config/$1$/seek/flv-type";
    cmd->cc_exec_value =	"byte-offset";
    cmd->cc_exec_type =		bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type time-msec";
    cmd->cc_help_descr =        N_("time in msec");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_flv_type_config;
    cmd->cc_exec_name = 	"/nkn/nvsd/virtual_player/config/$1$/seek/flv-type";
    cmd->cc_exec_value =	"time-msec";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 = 	"/nkn/nvsd/virtual_player/config/$1$/seek/use_9byte_hdr";
    cmd->cc_exec_value2 =	"false";
    cmd->cc_exec_type2 =   	bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type time-secs";
    cmd->cc_help_descr =        N_("time in seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_flv_type_config;
    cmd->cc_exec_name = 	"/nkn/nvsd/virtual_player/config/$1$/seek/flv-type";
    cmd->cc_exec_value =	"time-secs";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 = 	"/nkn/nvsd/virtual_player/config/$1$/seek/use_9byte_hdr";
    cmd->cc_exec_value2 =	"false";
    cmd->cc_exec_type2 =   	bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-flv-type byte-offset use-9byte-hdr";
    cmd->cc_help_descr =        N_("Prefix 9byte FLV header in place of the standard 13byte FLV header");
    cmd->cc_flags =             ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_flv_type_config;
    cmd->cc_exec_name = 	"/nkn/nvsd/virtual_player/config/$1$/seek/flv-type";
    cmd->cc_exec_value =	"byte-offset";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 = 	"/nkn/nvsd/virtual_player/config/$1$/seek/use_9byte_hdr";
    cmd->cc_exec_value2 =	"true";
    cmd->cc_exec_type2 =   	bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-mp4-type";
    cmd->cc_help_descr =        N_("Configure MP4 type");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "seek-config query-string-parm * seek-mp4-type *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure seek-mp4 type");
    cmd->cc_help_options 	=	cli_vp_mp4_type;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_mp4_type) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_4 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_seek_mp4_config;
    CLI_CMD_REGISTER;


    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow";
    cmd->cc_help_descr =        N_("Configure assured flow options. Applies to generic and youtube types.");
    cmd->cc_flags =             ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX  "assured-flow";
    cmd->cc_help_descr =        N_("Clear assured flow options.");
    cmd->cc_flags =             ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_assured_flow_config_reset;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_assured_flow_config;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow auto";
    cmd->cc_help_descr =        N_("Automatically determine rate for session");
    cmd->cc_flags =             ccf_terminal | ccf_var_args;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_assured_flow_config;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow auto | use-mbr";
    cmd->cc_help_descr =        N_("MBR (max bit rate) instead of AFR (assured flow rate");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow auto | delivery-overhead";
    cmd->cc_help_descr =        N_("Configure a factor to account for delivery overheads.");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow auto | delivery-overhead *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.25-2.0) to account for delivery overheads, e.g., TCP/IP (default is 1.25)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow rate";
    cmd->cc_help_descr =        N_("Configure rate");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "assured-flow rate *";
    cmd->cc_help_exp =          N_("<Kbps>");
    cmd->cc_help_exp_hint =     N_("Rate");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_assured_flow_config;
    cmd->cc_magic =             vp_type_0 | vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;
    /*--------------------------------------------------------------------*/
    /*
     * Rate control CLI . will replace the assured-flow option.
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control";
    cmd->cc_help_descr =        N_("Configure rate-control options. Applies to generic and youtube types.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX  "rate-control";
    cmd->cc_help_descr =        N_("Clear rate-control options.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config_reset;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate";
    cmd->cc_help_descr =        N_("Best effort rate control mechanism using MBR mechanism."
	                           "System provisioning is a pre-requisite to guarantee delivery rate. (Recommended option)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate";
    cmd->cc_help_descr =        N_("Guaranteed rate control mechanism using AFR mechanism."
	                           "Employs a much stricter control over delivery rate.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate auto-detect";
    cmd->cc_help_descr =        N_("Enables the auto detection of encoding bit-rates for supported"
	                           "media file format for pacing/rate throttling enforcement.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate auto-detect burst-factor";
    cmd->cc_help_descr =        N_("Enables the appliance to deliver data at a rate greater than the configured/detected rate."
	                           "Default is 10% faster (1.1)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate auto-detect burst-factor *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.0-3.0).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"1";

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate static";
    cmd->cc_help_descr =        N_("Enforce a statically configured fixed value for throttling the"
	                           "delivery rate for all objects. Useful for non-media objects");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate static *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure the static rate in Kbps");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate static * burst-factor";
    cmd->cc_help_descr =        N_("Enables the appliance to deliver data at a rate greater than the configured/detected rate."
	                           "Default is 10% faster (1.1)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate static * burst-factor *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.0-3.0).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"1";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate query-string-parm";
    cmd->cc_help_descr =        N_("Enforces rate control using the value provided by a client via"
	                           " an incoming request through the configured query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure the query string value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate query-string-parm * *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure the rate units");
    cmd->cc_help_options 	=	cli_vp_rc_query_ru;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_rc_query_ru) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate query-string-parm * * burst-factor";
    cmd->cc_help_descr =        N_("Enables the appliance to deliver data at a rate greater than the configured/detected rate."
	                           "Default is 10% faster (1.1)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control max-bit-rate query-string-parm * * burst-factor *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.0-3.0).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"1";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate auto-detect";
    cmd->cc_help_descr =        N_("Enables the auto detection of encoding bit-rates for supported"
	                           "media file format for pacing/rate throttling enforcement.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate auto-detect burst-factor";
    cmd->cc_help_descr =        N_("Enables the appliance to deliver data at a rate greater than the configured/detected rate."
	                           "Default is 10% faster (1.1)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate auto-detect burst-factor *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.0-3.0).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"1";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate static";
    cmd->cc_help_descr =        N_("Enforce a statically configured fixed value for throttling the"
	                           "delivery rate for all objects. Useful for non-media objects");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate static *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure the static rate in Kbps");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate static * burst-factor";
    cmd->cc_help_descr =        N_("Enables the appliance to deliver data at a rate greater than the configured/detected rate."
	                           "Default is 10% faster (1.1)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate static * burst-factor *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.0-3.0).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"1";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate query-string-parm";
    cmd->cc_help_descr =        N_("Enforces rate control using the value provided by a client via"
	                           " an incoming request through the configured query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure the query-string value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate query-string-parm * *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure the rate units");
    cmd->cc_help_options 	=	cli_vp_rc_query_ru;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_rc_query_ru) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate query-string-parm * * burst-factor";
    cmd->cc_help_descr =        N_("Best effort rate control mechanism using MBR mechanism."
	                           "System provisioning is a pre-requisite to guarantee delivery rate. (Recommended option)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-control assured-flow-rate query-string-parm * * burst-factor *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Configure a factor in the range(1.0-3.0).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             vp_type_0 |vp_type_1 | vp_type_3 | vp_type_5;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_control_config;
    cmd->cc_exec_data =         (void *)"1";
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "smooth-flow";
    cmd->cc_help_descr =        N_("Configure smooth flow options");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "smooth-flow";
    cmd->cc_help_descr =        N_("Clear smooth flow options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_smooth_flow_config_reset;
    cmd->cc_magic =             vp_type_0;
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "smooth-flow query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "smooth-flow query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_smooth_flow_config;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;
#endif
    /*--------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*/
    /*Rate map option is only for qss-streamlet. Since the player has been deprecated.
     * This cli is not needed anymore. 
     * The cli is not to be used  for generice player.
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map";
    cmd->cc_help_descr =        N_("Configure rate map options. Applies to generic type only.");
    cmd->cc_flags =             ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match";
    cmd->cc_help_descr =        N_("Configure match string to match in URI");
    CLI_CMD_REGISTER;
   
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Match String");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/$1$/rate_map/match/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate";
    cmd->cc_help_descr =        N_("Configure rate options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate *";
    cmd->cc_help_exp =          N_("<Kbps>");
    cmd->cc_help_exp_hint =     N_("Rate");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_map_config;
    cmd->cc_magic =             vp_type_0 | vp_type_2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "rate-map";
    cmd->cc_help_descr =        N_("Clear a rate-map entry from the list");
    cmd->cc_flags =             ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "rate-map match";
    cmd->cc_help_descr =        N_("Choose a match string to remove from the rate-map");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "rate-map match *";
    cmd->cc_help_exp =          N_("<match string>");
    cmd->cc_help_exp_hint =     N_("Match string");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/$1$/rate_map/match/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_map_config_reset;
    cmd->cc_magic =             vp_type_0 | vp_type_2;
    CLI_CMD_REGISTER;

    
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate * query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    cmd->cc_flags =             ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate * query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    cmd->cc_flags =             ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_map_config;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate * uol";
    cmd->cc_help_descr =        N_("Configure URI Offset and length");
    cmd->cc_flags =             ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate * uol *";
    cmd->cc_help_exp =          N_("<offset>");
    cmd->cc_help_exp_hint =     N_("URI offset");
    cmd->cc_flags =             ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "rate-map match * rate * uol * *";
    cmd->cc_help_exp =          N_("<length>");
    cmd->cc_help_exp_hint =     N_("URI length");
    cmd->cc_flags =             ccf_terminal | ccf_unavailable;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_rate_map_config;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/


    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download";
    cmd->cc_help_descr =        N_("Configure full download options.  Applies to generic type only.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "full-download";
    cmd->cc_help_descr =        N_("Disable full-download ");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_full_download_config_reset;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download always";
    cmd->cc_help_descr =        N_("Configure full download options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_full_download_config;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download match";
    cmd->cc_help_descr =        N_("Configure match options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download match *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Match string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download match * query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download match * query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_full_download_config;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download match * header";
    cmd->cc_help_descr =        N_("Configure Header options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "full-download match * header *";
    cmd->cc_help_exp =          N_("<header name>");
    cmd->cc_help_exp_hint =     N_("Header Name");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_full_download_config;
    cmd->cc_magic =             vp_type_0;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "health-probe";
    cmd->cc_help_descr =        
        N_("Configure query string and match string for health probe. Applies to yahoo type only.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "health-probe query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string for health probe");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "health-probe query-string-parm *";
    cmd->cc_help_exp =          N_("<query string>");
    cmd->cc_help_exp_hint =     N_("URI query string parameter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "health-probe query-string-parm * match";
    cmd->cc_help_descr =        N_("Configure match string for health probe");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "health-probe query-string-parm * match *";
    cmd->cc_help_exp =          N_("<query string>");
    cmd->cc_help_exp_hint =     N_("URI query string parameter");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_health_probe_config;
    cmd->cc_magic =             vp_type_3;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "health-probe";
    cmd->cc_help_descr =        N_("Disable health-probe configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_health_probe_config_reset;
    cmd->cc_magic =             vp_type_3;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/


    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "control-point";
    cmd->cc_help_descr =        N_("Configure a control point string for this player. Applies to smoothflow type only.");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "control-point *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("");
    cmd->cc_help_options =      cli_control_point_opts;
    cmd->cc_help_num_options =  sizeof(cli_control_point_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_control_point_config;
    cmd->cc_magic =             vp_type_4;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth"; 
    cmd->cc_help_descr =        "Configure md-5 hash options. Applies to yahoo type only.";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest";
    cmd->cc_help_descr =        "";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5";
    cmd->cc_help_descr =        "";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id ";
    cmd->cc_help_descr =        N_("Configure stream-id parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 " 
                                    "stream-id query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string parameter that represents stream-id");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("query string parameter that represents stream-id");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id";
    cmd->cc_help_descr =        N_("Configure auth-id parameter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string parameter that represents auth-id");
    CLI_CMD_REGISTER;
        
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("query string parameter that represents auth-id");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm * "
                                    "shared-secret";
    cmd->cc_help_descr =        N_("Configure shared secret string");
    CLI_CMD_REGISTER;
        
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm  * "
                                    "auth-id query-string-parm * "
                                    "shared-secret *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("A shared secret string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm * "
                                    "shared-secret * "
                                    "time-interval";
    cmd->cc_help_descr =        N_("Configure a time-interval");
    CLI_CMD_REGISTER;
        
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm  * "
                                    "auth-id query-string-parm * "
                                    "shared-secret * "
                                    "time-interval *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Time interval, specified in seconds");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm * "
                                    "shared-secret * "
                                    "time-interval * "
                                    "match";
    cmd->cc_help_descr =        N_("Configure match string to validate the "
                                    "computed hash value against this");
    CLI_CMD_REGISTER;
        
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm * "
                                    "shared-secret * "
                                    "time-interval * "
                                    "match query-string-parm";
    cmd->cc_help_descr =        N_("Configure the query string");
    CLI_CMD_REGISTER;
        
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "req-auth digest md-5 "
                                    "stream-id query-string-parm * "
                                    "auth-id query-string-parm * "
                                    "shared-secret * "
                                    "time-interval * "
                                    "match query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("This is used to compare the computed hash value."
                                    "If this value matches, the session is processed, "
                                    "otherwise the session is rejected");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_req_auth_config;
    cmd->cc_magic =             vp_type_3;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " VP_CMD_PREFIX "req-auth";
    cmd->cc_help_descr =        N_("Disable req-auth configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_req_auth_config_reset;
    cmd->cc_magic =             vp_type_3;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/


    /*-----------------------------------------------------------------------*/
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals";
    cmd->cc_help_descr =        N_("Configure signals for this player. Applies to smoothflow type only.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id";
    cmd->cc_help_descr =        N_("Configure session-id string identifer");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm";
    cmd->cc_help_descr =        N_("Configure a query string parameter to "
                                    "identify the session id");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("A string that identifies the session");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state";
    cmd->cc_help_descr =        N_("Configure state options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm";
    cmd->cc_help_descr =        N_("Configure string that identifies the "
                                    "state of the session");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("A string that identifies the session state");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm * "
                                "profile";
    cmd->cc_help_descr =        N_("Configure profile options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm * "
                                "profile query-string-parm";
    cmd->cc_help_descr =        N_("Configure string that indentifies the "
                                    "profile that the session uses");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm * "
                                "profile query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("String that identifies the profile to "
                                    "use");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_signals_config;
    cmd->cc_magic =             vp_type_4;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm * "
                                "profile query-string-parm * "
                                "chunk";
    cmd->cc_help_descr =        N_("Configure chunk options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm * "
                                "profile query-string-parm * "
                                "chunk query-string-parm";
    cmd->cc_help_descr =        N_("Configure string that identifies the "
                                    "chunk used by the session");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "signals "
                                "session-id query-string-parm * "
                                "state query-string-parm * "
                                "profile query-string-parm * "
                                "chunk query-string-parm *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     N_("String that identifies the chunk used "
                                    "by the session");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_vp_signals_config;
    cmd->cc_magic =             vp_type_4;
    CLI_CMD_REGISTER;
#endif
    /*-----------------------------------------------------------------------*/
    
    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name";
    cmd->cc_help_descr = 	N_("Configure video-id parameters such as query string, etc. Applies to youtube only");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name video-id ";
    cmd->cc_help_descr = 	N_("Configure video-id parameters such as query string, etc.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name "
	    			"video-id query-string-parm";
    cmd->cc_help_descr = 	N_("Configure a query string parameter for video-id");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name "
	    			"video-id query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter whose value MFD will use to recognise the video id");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name "
	    			"video-id query-string-parm * "
				"format-tag";
    cmd->cc_help_descr = 	N_("Configure a query string parameter for video-id");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name "
	    			"video-id query-string-parm * "
	    			"format-tag query-string-parm";
    cmd->cc_help_descr = 	N_("Configure a query string parameter for video-id");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "cache-name "
	    			"video-id query-string-parm * "
	    			"format-tag query-string-parm *";
    cmd->cc_help_exp = 		N_("<string>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter whose value MFD will use to recognise the video id");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_video_id_config;
    cmd->cc_magic = 		vp_type_5;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "quality-tag";
    cmd->cc_help_descr = 	N_("Configure smoothstream-pub quality-tag parameter. Applies to smoothstream-pub type only");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "quality-tag *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter used by smoothstream-pub for determining the quality level");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_quality_config;
    cmd->cc_magic = 		vp_type_6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no "VP_CMD_PREFIX "quality-tag";
    cmd->cc_help_descr = 	N_("Reset smoothstream-pub quality-tag parameter");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_quality_config_reset;
    cmd->cc_magic = 		vp_type_6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "fragment-tag";
    cmd->cc_help_descr = 	N_("Configure smoothstream/flashstream-pub fragment-tag parameter.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "fragment-tag *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter used by smoothstream/flashstream-pub for determining the fragment to be requested");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_fragment_config;
    cmd->cc_magic = 		vp_type_6| vp_type_7;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str = 	"no "VP_CMD_PREFIX "fragment-tag";
    cmd->cc_help_descr = 	N_("Reset smoothstream-pub fragment-tag parameter");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_fragment_config_reset;
    cmd->cc_magic = 		vp_type_6| vp_type_7;
    CLI_CMD_REGISTER;

    /* Flash stream pub relatet cli commands */

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "segment-tag";
    cmd->cc_help_descr = 	N_("Configure flashstream-pub segment-tag parameter.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "segment-tag *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter used by flash-pub for determining the segment to be requested");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_segment_config;
    cmd->cc_magic = 		vp_type_7;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str = 	"no "VP_CMD_PREFIX "segment-tag";
    cmd->cc_help_descr = 	N_("Reset flashstream-pub segment-tag parameter");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_segment_config_reset;
    cmd->cc_magic = 		vp_type_7;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "seg-frag-delimiter";
    cmd->cc_help_descr = 	N_("Configure flashstream-pub segment fragment-delimiter tag parameter.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "seg-frag-delimiter *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter used by flashstream-pub for determining the segment-fragment delimiter");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_seg_frag_delimiter_config;
    cmd->cc_magic = 		vp_type_7;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str = 	"no "VP_CMD_PREFIX "seg-frag-delimiter" ;
    cmd->cc_help_descr = 	N_("Reset segment-fragment delimiter parameter");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_seg_frag_delimiter_config_reset;
    cmd->cc_magic = 		vp_type_7;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt";
    cmd->cc_help_descr = 	N_("Configure bandwidth optimization for MP4/FLV video files by enabling offline"
	    				"file transcoding.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         VP_CMD_PREFIX "bandwidth-opt file-type";
    cmd->cc_help_descr =        N_("Configure bandwidth optimization for MP4/FLV video files by enabling offline"
	    "file transcoding.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type *";
    cmd->cc_help_options 	=	cli_vp_bw_ftype;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_bw_ftype) / 
	    				sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * transcode-comp-ratio";
    cmd->cc_help_descr = 	N_("Configure bandwidth optimization for MP4/FLV video files by enabling offline"
	    				"file transcoding.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * transcode-comp-ratio *";
    cmd->cc_help_options 	=	cli_vp_bw_transcode;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_bw_transcode) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_bw_opt_config;
    cmd->cc_magic = 		vp_type_0  | vp_type_5 ;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * transcode-comp-ratio * activate-on";
    cmd->cc_help_descr = 	N_("Configure the hotness trigger threshold  for enabling offline"
	    				"file transcoding.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * transcode-comp-ratio * activate-on *";
    cmd->cc_help_exp =          N_("<hotness trigger threshold>");
    cmd->cc_help_exp_hint = 	N_("Integer [0-100]");
    cmd->cc_flags = 		ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_bw_opt_config;
    cmd->cc_magic = 		vp_type_0 | vp_type_5 ;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " VP_CMD_PREFIX "bandwidth-opt";
    cmd->cc_help_descr = 	N_("Reset the bandwidth optimization for MP4/FLV video files ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " VP_CMD_PREFIX "bandwidth-opt file-type";
    cmd->cc_help_descr = 	N_("Reset bandwidth optimization for MP4/FLV video files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " VP_CMD_PREFIX "bandwidth-opt file-type *";
    cmd->cc_help_options 	=	cli_vp_bw_ftype;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_bw_ftype) / 
	    				sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * switch-rate";
    cmd->cc_help_descr = 	N_("Allows to switch a video to a lower/higher bit rate version if available, before delivery.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * switch-rate *";
    cmd->cc_help_options 	=	cli_vp_bw_switch_rate;
    cmd->cc_help_num_options 	=       sizeof(cli_vp_bw_switch_rate) / 
	    				sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * switch-rate * limit-rate-to";
    cmd->cc_help_descr = 	N_("Specifies a min or max bit-rate cap in Kbps, where a switch is limited by this rate.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	VP_CMD_PREFIX "bandwidth-opt file-type * switch-rate * limit-rate-to *";
    cmd->cc_help_exp =          N_("<Kbps>");
    cmd->cc_help_exp_hint = 	N_("Integer [0-16000]");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_bw_opt_config;
    cmd->cc_magic = 		vp_type_5 ;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " VP_CMD_PREFIX "bandwidth-opt file-type * switch-rate";
    cmd->cc_help_descr = 	N_("Reset the switch-rate option for the file-type.");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_bw_opt_reset;
    cmd->cc_magic = 		vp_type_5 ;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " VP_CMD_PREFIX "bandwidth-opt file-type * transcode-comp-ratio";
    cmd->cc_help_descr = 	N_("Reset the transcode-comp-ratio values for the file type.");
    cmd->cc_flags = 		ccf_terminal; 
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_bw_opt_reset;
    cmd->cc_magic = 		vp_type_0 | vp_type_5 ;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show virtual-player";
    cmd->cc_help_descr =        N_("Display virtual player options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show virtual-player list";
    cmd->cc_help_descr =        N_("Display a list of available virtual players");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_show_list;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show virtual-player *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     N_("Virtual player name");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/*";
    cmd->cc_comp_type =         cct_matching_names;
    //cmd->cc_help_callback =     cli_virtual_player_name_help;
    //cmd->cc_comp_callback =     cli_nvsd_virtual_player_name_comp;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_show_player;
    CLI_CMD_REGISTER;
    /*-----------------------------------------------------------------------*/
    
bail:
    return err;
}




int 
cli_nvsd_virtual_player_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tstr_array *ret_names = NULL;
    uint32 num_names = 0, i = 0;
    bn_binding *bn_player_type  = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_nvsd_virtual_player_get_player_names(&ret_names, false);
    bail_error(err);

    cli_printf(_("Virtual Players :\n"));
    num_names = tstr_array_length_quick(ret_names);

    if (num_names <= 0) {
        err = cli_printf(_("  No Player configured."));
        bail_error(err);
    } 
    else {
        for(i = 0; i < num_names; ++i) {
		uint32 type = 0;
		const char *name = tstr_array_get_str_quick(ret_names, i);
        	err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_player_type, 
                	NULL, "/nkn/nvsd/virtual_player/config/%s/type", name);
	        bail_error(err);

        	err = bn_binding_get_uint32(bn_player_type, ba_value, NULL, &type);
	        bail_error(err);
	 	
        	err = cli_printf_query(_("  %s\t\t(type %s)\n"),
                    	name, vp_type_names[type]);
	        bail_error(err);
        }
    }

bail:
    bn_binding_free(&bn_player_type);
    tstr_array_free(&ret_names);
    return err;
}

int 
cli_nvsd_virtual_player_show_player(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *name = NULL;
    tstr_array *names = NULL;
    char *bn_player = NULL;
    bn_binding_array *binding = NULL;
    uint32 type = (uint32) (-1);
    uint32 idx = 0;
    bn_binding *bn_player_type = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    // Validate the name first
    name = tstr_array_get_str_quick(params, 0);
    bail_null(name);

    err = cli_nvsd_virtual_player_get_player_names(&names, false);
    bail_error(err);

    err = tstr_array_linear_search_str(names, name, 0, &idx);
    if (err == lc_err_not_found) {
        cli_printf_error(_("Virtual-player \"%s\" not found\n"), name);
        err = 0;
        bail_error(err);
    }
    else {
        bn_player = smprintf("/nkn/nvsd/virtual_player/config/%s/*", name);
        bail_null(bn_player);

        // Get type 
        err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_player_type, 
                NULL, "/nkn/nvsd/virtual_player/config/%s/type", name);
        bail_error(err);


        err = bn_binding_get_uint32(bn_player_type, ba_value, NULL, &type);
        bail_error(err);

        err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, 
                &binding, true, true, bn_player);

        cli_printf(_("Virtual Player : %s\n"), name);
        cli_printf(_("  Type : %s\n"), vp_type_names[type]);

        switch(type)
        {
        case 0:
            // Player of type 0
            err = cli_nvsd_vp_show_type_0(name, binding);
            bail_error(err);
            break;
        case 1:
            // Player of type 1
            err = cli_nvsd_vp_show_type_1(name, binding);
            bail_error(err);
            break;
        case 2:
            // Player of type 2
            err = cli_nvsd_vp_show_type_2(name, binding);
            bail_error(err);
            break;
        case 3:
            err = cli_nvsd_vp_show_type_3(name, binding);
            bail_error(err);
            break;
        case 4:
            err = cli_nvsd_vp_show_type_4(name, binding);
            bail_error(err);
            break;
        case 5:
            err = cli_nvsd_vp_show_type_5(name, binding);
	    bail_error(err);
	    break;
	case 6:
	    err = cli_nvsd_vp_show_type_6(name, binding);
	    bail_error(err);
	    break;
	case 7:
	    err = cli_nvsd_vp_show_type_7(name, binding);
	    bail_error(err);
	    break;
        default:
            cli_printf_error(_("Unknown player type\n"));
            break;
        }

    }

bail:
    bn_binding_array_free(&binding);
    bn_binding_free(&bn_player_type);
    safe_free(bn_player);
    tstr_array_free(&names);
    return err;
}
static void
cli_nvsd_print_seek_enable_tunnel (
	const char *name)
{
    int err = 0;
    tstring *t_enable_tunnel = NULL;

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, false, &t_enable_tunnel, 
	    NULL, "/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel", name);
    bail_error(err);


    if(!strcmp(ts_str(t_enable_tunnel),"true")) {
	cli_printf(_(
		    "  Tunnel Enabled : Yes\n"));
    } else {
	cli_printf(_(
		    "  Tunnel Enabled : No\n"));
    }

    bail:
	ts_free(&t_enable_tunnel);
	return;
}
static void
cli_nvsd_print_assured_flow_config (
	const char *name, 
	const char *afr)
{
    int err = 0;
    char *t_valid = NULL;
    uint32 num_parts = 0;
    bn_binding *bn_valid = NULL;
    tstr_array *name_parts = NULL;
    const tstring *last_name_part = NULL;
    bn_binding *binding = NULL;
    tbool t_mbr = false;



    /* Sanity Test */
    if (!name || !afr) return ;

    // Display Fast start config
    //
    // Get type 
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_valid, 
                NULL, "/nkn/nvsd/virtual_player/config/%s/assured_flow/valid", name);
    bail_error(err);


    err = bn_binding_get_str(bn_valid, ba_value, bt_link, NULL, &t_valid);
    bail_error(err);

    /* Now tokenize the node name */
    err = ts_tokenize_str(t_valid, '/', '\0', '\0', 0, &name_parts);
    bail_error(err);

    num_parts = tstr_array_length_quick(name_parts);
    last_name_part = tstr_array_get_quick (name_parts, num_parts - 1);

    cli_printf(_(
                "Assured Flow Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), afr);
    if (!strcmp (ts_str(last_name_part), "rate"))
        cli_printf_query(_(
                "  Rate: #%s/rate# Kbps (ACTIVE)\n"), afr);
    else
        cli_printf_query(_(
                "  Rate: #%s/rate# Kbps\n"), afr);
    if (!strcmp (ts_str(last_name_part), "uri_query"))
    	cli_printf_query(_(
                "  URI query: #%s/uri_query# (ACTIVE) \n"), afr);
    else
    	cli_printf_query(_(
                "  URI query: #%s/uri_query# \n"), afr);
    if (!strcmp (ts_str(last_name_part), "auto"))
        cli_printf_query(_(
                "  Auto: #%s/auto# (ACTIVE) "), afr);
    else
        cli_printf_query(_(
                "  Auto: #%s/auto# "), afr);
    cli_printf_query(_("  delivery-overhead: #%s/overhead#"), afr);
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding, 
                NULL, "/nkn/nvsd/virtual_player/config/%s/assured_flow/use-mbr", name);
    bail_error(err);
    err = bn_binding_get_tbool(binding, ba_value, NULL, &t_mbr);
    bail_error(err);
    if(t_mbr) {
	cli_printf(_("using MBR"));
    }
    cli_printf(_("\n"));


    cli_printf(_("\n"));

bail:
    safe_free(t_valid);
    bn_binding_free(&bn_valid);
    bn_binding_free(&binding);
    tstr_array_free(&name_parts);
    return;
}

static void
cli_nvsd_print_fast_start_config (
	const char *name, 
	const char *fast)
{
    int err = 0;
    char *t_valid = NULL;
    uint32 num_parts = 0;
    bn_binding *bn_valid = NULL;
    tstr_array *name_parts = NULL;
    const tstring *last_name_part = NULL;
    uint32 type = 0;	

    /* Sanity Test */
    if (!name || !fast) return ;

    // Display Fast start config
    //
    // Get type 
    //
    err = cli_nvsd_vp_type_to_mask(name, &type);
    bail_error(err);

    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_valid, 
                NULL, "/nkn/nvsd/virtual_player/config/%s/fast_start/valid", name);
    bail_error(err);


    err = bn_binding_get_str(bn_valid, ba_value, bt_link, NULL, &t_valid);
    bail_error(err);

    /* Now tokenize the node name */
    err = ts_tokenize_str(t_valid, '/', '\0', '\0', 0, &name_parts);
    bail_error(err);

    num_parts = tstr_array_length_quick(name_parts);
    last_name_part = tstr_array_get_quick (name_parts, num_parts - 1);

    cli_printf(_(
                "Fast Start Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), fast);
    if (!strcmp (ts_str(last_name_part), "size"))
    	cli_printf_query(_(
                "  Size: #%s/size# kBytes (ACTIVE)\n"), fast);
    else
    	cli_printf_query(_(
                "  Size: #%s/size# kBytes\n"), fast);
#if 1
    if (!strcmp (ts_str(last_name_part), "time"))
    	cli_printf_query(_(
                "  time: #%s/time# seconds (ACTIVE)\n"), fast);
    else
    	cli_printf_query(_(
                "  time: #%s/time# seconds\n"), fast);
#endif
    if (!strcmp (ts_str(last_name_part), "uri_query"))
    	cli_printf_query(_(
                "  URI query: #%s/uri_query# (ACTIVE) \n"), fast);
    else
    	cli_printf_query(_(
                "  URI query: #%s/uri_query# \n"), fast);

    cli_printf(_("\n"));

bail:
    safe_free(t_valid);
    bn_binding_free(&bn_valid);
    tstr_array_free(&name_parts);
    return;
}

static int
cli_nvsd_vp_show_type_0(
        const char *name,
        const bn_binding_array *parent_bindings)
{
    int err = 0;
    char *r = NULL;
    char *h = NULL;
    char *afr = NULL;
    char *fast = NULL;
    char *f = NULL;
    char *seek = NULL;
    char *s = NULL;

    UNUSED_ARGUMENT(parent_bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
    h = smprintf("%s/hash_verify", r);
    bail_null(h);
    //s = smprintf("%s/smooth_flow", r);
    //bail_null(s);
    f = smprintf("%s/full_download", r);
    bail_null(f);
    fast = smprintf("%s/fast_start", r);
    bail_null(fast);
    afr = smprintf("%s/assured_flow", r);
    bail_null(afr);
    seek = smprintf("%s/seek", r);
    bail_null(seek);
    

    cli_printf(_("\n"));
    // Display hash-verify config
    //
    cli_printf(_(
                "Hash Verify Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), h); 
    cli_printf_query(_(
                "  Digest: #%s/digest#\n"), h); 
#if 0
    cli_printf_query(_(
                "  Data String: #%s/data/string#\n"), h);
    cli_printf_query(_(
                "  Data UOL Offset: #%s/data/uri_offset#\n"), h);
    cli_printf_query(_(
                "  Data UOL Length: #%s/data/uri_len#\n"), h);
#endif
    cli_printf_query(_(
                "  Match query string: #%s/match/uri_query#\n"), h);
    cli_printf(_(
                "  Shared secret: ****\n") );
    cli_printf_query(_(
                "  Expiry time : #%s/expiry_time/epochtime# (POSIX timestamp format)\n"), h);
    cli_printf_query(_(
                "  URL format : #%s/url_type# \n"), h);


    cli_printf(_("\n"));
    // Display Connection parameters
    //
    cli_printf(_(
                "Connection Configuration\n"));
    cli_printf_query(_(
                "  Max Bandwidth: #%s/max_session_rate# Kbps\n"), r);

    cli_printf(_("\n"));
    // Display seek config
    cli_printf(_(
                "Seek Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), seek);
    cli_printf_query(_(
                "  URI Query: #%s/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek Length URI Query : #%s/length/uri_query#\n"), seek);

    cli_printf_query(_(
                "  Seek FLV type : #%s/flv-type#\n"), seek);
    cli_nvsd_print_use_9byte_hdr(name);
    cli_printf_query(_(
                "  Seek MP4 type : #%s/mp4-type#\n"), seek);
    cli_nvsd_print_seek_enable_tunnel(name);

    cli_printf(_("\n"));
#if 0
    // Display smooth-flow config
    //
    cli_printf(_(
                "Smooth Flow Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), s);
    cli_printf_query(_(
                "  URI Query: #%s/uri_query#\n"), s);
#endif
    cli_printf(_("\n"));
    // Display AFR config
    //cli_nvsd_print_assured_flow_config (name, afr);
    cli_nvsd_print_rate_control_config (name, afr);

    // Display Fast Start config
    cli_nvsd_print_fast_start_config (name, fast);

    // Display Full download config
    cli_printf(_(
                "Full Download Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), f);
    cli_printf_query(_(
                "  Always: #%s/always#\n"), f);
    cli_printf_query(_(
                "  Match string: #%s/match#\n"), f);
    cli_printf_query(_(
                "  URI query string: #%s/uri_query#\n"), f);
    cli_printf_query(_(
                "  Header: #%s/uri_hdr#\n"), f);

    // Display Bandwidth optimization values
     cli_nvsd_print_bw_opt_config(name);

    cli_printf(_("\n"));
#if 0
    // Display Rate Map
    err = cli_nvsd_vp_show_type_2(name, parent_bindings);
    bail_error(err);
#endif 

bail:
    safe_free(r);
    safe_free(h);
    safe_free(afr);
    safe_free(fast);
    safe_free(f);
    safe_free(seek);
    safe_free(s);
    return err;
}


static int
cli_nvsd_vp_show_type_1(
        const char *name,
        const bn_binding_array *parent_bindings)
{
    int err = 0;
    char *r = NULL;
    char *h = NULL;
    char *afr = NULL;
    char *fast = NULL;
    char *f = NULL;
    char *seek = NULL;
    char *s = NULL;
    char *secret_value = NULL;
    bn_binding *binding = NULL;

    UNUSED_ARGUMENT(parent_bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
    h = smprintf("%s/hash_verify", r);
    bail_null(h);
    s = smprintf("%s/smooth_flow", r);
    bail_null(s);
    f = smprintf("%s/full_download", r);
    bail_null(f);
    fast = smprintf("%s/fast_start", r);
    bail_null(fast);
    afr = smprintf("%s/assured_flow", r);
    bail_null(afr);
    seek = smprintf("%s/seek", r);
    bail_null(seek);
    

    cli_printf(_("\n"));
    // Display hash-verify config
    //
    cli_printf(_(
                "Hash Verify Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), h); 
    cli_printf_query(_(
                "  Digest: #%s/digest#\n"), h); 
#if 0
    cli_printf_query(_(
                "  Data String: #%s/data/string#\n"), h);
    cli_printf_query(_(
                "  Data UOL Offset: #%s/data/uri_offset#\n"), h);
    cli_printf_query(_(
                "  Data UOL Length: #%s/data/uri_len#\n"), h);
#endif
    cli_printf_query(_(
                "  Match query string: #%s/match/uri_query#\n"), h);
    /* Query the DB for the shared secret .. if empty then print nothing
     */
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding, NULL, 
            "%s/secret/value", h);
    bail_error(err);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &secret_value);
    bail_error(err);
    if (strlen(secret_value) > 0) {
        cli_printf_query(_(
                    "  Shared secret: **** (#%s/secret/type#)\n"), h );
    } else {
        cli_printf(_(
                    "  Shared secret: \n") );
    }
    cli_printf_query(_(
                "  Expiry time : #%s/expiry_time/epochtime# (POSIX timestamp format)\n"), h);
    cli_printf_query(_(
                "  URL format : #%s/url_type# \n"), h);
    cli_printf(_("\n"));


    // Display seek config
    cli_printf(_(
                "Seek Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), seek);
    cli_printf_query(_(
                "  URI Query: #%s/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek Length URI Query : #%s/length/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek FLV type : #%s/flv-type#\n"), seek);
    cli_nvsd_print_use_9byte_hdr(name);
    cli_printf_query(_(
                "  Seek MP4 type : #%s/mp4-type#\n"), seek);

    cli_nvsd_print_seek_enable_tunnel(name);


    cli_printf(_("\n"));
    // Display AFR config
    cli_nvsd_print_rate_control_config (name, afr);

    // Display Fast Start config
    //
    cli_nvsd_print_fast_start_config (name, fast);

    cli_printf(_("\n"));
    // Display Connection parameters
    //
    cli_printf(_(
                "Connection Configuration\n"));
    cli_printf_query(_(
                "  Max Bandwidth: #%s/max_session_rate# Kbps\n"), r);

    cli_printf(_("\n"));
bail:
    safe_free(r);
    safe_free(h);
    safe_free(afr);
    safe_free(fast);
    safe_free(f);
    safe_free(seek);
    safe_free(s);
    bn_binding_free(&binding);
    return err;
}



static int
cli_nvsd_vp_show_type_2(
        const char *name,
        const bn_binding_array *parent_bindings)
{
    int err = 0;
    char *pattern = NULL;
    uint32 num_matches = 0;
    bn_binding_array *bindings = NULL;

    UNUSED_ARGUMENT(parent_bindings);

    // Display rate map
    pattern = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/match/*", name);
    bail_null(pattern);

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, 
            &bindings, true, true, pattern);
    bail_error(err);

    err = bn_binding_array_sort(bindings);
    bail_error(err);

    cli_printf(_("Rate-Map configuration :\n"));
    cli_printf_query(_(" Enabled: #/nkn/nvsd/virtual_player/config/%s/rate_map/active#\n"),
            name);


    err = mdc_foreach_binding_prequeried(bindings, pattern, NULL,
            cli_nvsd_virtual_player_show_rate_map_one, NULL, &num_matches);
    bail_error(err);

    cli_printf(_("\n"));
    // Display Connection parameters
    //
    cli_printf(_(
                "Connection Configuration\n"));
    cli_printf_query(_(
                "  Max Bandwidth: #/nkn/nvsd/virtual_player/config/%s/max_session_rate# Kbps\n"), name);

    cli_printf(_("\n"));

bail:
    bn_binding_array_free(&bindings);
    safe_free(pattern);
    return err;
}

int
cli_nvsd_virtual_player_show_rate_map_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data)
{
    int err = 0;
    const char *r = NULL;

    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    r = ts_str(name);

    cli_printf(_("\n"));
    cli_printf_prequeried(bindings, _(
                " Match: #%s# \n"), r);
    cli_printf_prequeried(bindings, _(
                "   Rate: #%s/rate# Kbps \n"), r);
#if 0
    cli_printf_prequeried(bindings, _(
                "   Query string: #%s/query_string#\n"), r);
    cli_printf_prequeried(bindings, _(
                "   URI Offset: #%s/uol/offset#\n"), r);
    cli_printf_prequeried(bindings, _(
                "   URI Length: #%s/uol/length#\n"), r);
#endif

    return err;
}


int
cli_nvsd_vp_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{

    int err = 0;
    int ret_err = 0;
    tbool valid = true;
    char *bn_name = NULL;
    //const char *type = NULL;
    const char *c_player = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    if (cli_prefix_modes_is_enabled()) {
        err = cli_nvsd_vp_validate(c_player, &valid, &ret_err);
        bail_error(err);

        if (!valid & (ret_err == lc_err_not_found)) {

            /* mandate the "type" when the Virtual Player is created. 
             * That is, if a virtual player is created for the 1st time 
             * the command fails if no type is specified.
             * You can check this by looking at whether it's an existing 
             * player or a new one.
             */
            cli_printf_error(_("Player '%s' doesn't exist. Create it first with the "
                        "'virtual-player <name> type <type>'  command."), c_player);
            goto bail;
            /* Player doesnt exisit. Create it 
             */
            //bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s", c_player);
            //bail_null(bn_name);

            //err = mdc_create_binding(cli_mcc, NULL, NULL, 
                    //bt_string, 
                    //c_player, 
                    //bn_name);
            //bail_error(err);

            //valid = true;
        }

        if (valid) {
            err = cli_prefix_enter_mode(cmd, cmd_line);
            bail_error(err);
        }
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    safe_free(bn_name);
    return err;
}


static int
cli_nvsd_vp_hash_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    char *bn_name = NULL;
    tbool is_allowed = false;
    const char *vpname = NULL;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    vpname = tstr_array_get_str_quick(params, 0);
    bail_null(vpname);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/active", vpname);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_bool,
            "false",
            bn_name);
    bail_error(err);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/url_type", vpname);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
	    "absolute-url",
	    bn_name);
    bail_error(err);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/expiry_time/epochtime", vpname);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_reset, bt_string,
	    "0",
	    bn_name);
    bail_error(err);
    
bail:
    safe_free(bn_name);
    return err;
}

int
cli_nvsd_vp_hash_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *player = NULL;
    uint32 idx = 0;
    char *bn_digest = NULL, 
         *bn_data = NULL, 
         *bn_uol_len = NULL, 
         *bn_uol_off = NULL, 
         *bn_secret = NULL, 
         *bn_secret_type = NULL, 
         *bn_uri_query = NULL,
	 *bn_expire_time = NULL,
	 *bn_url_type = NULL,
	 *bn_active = NULL;

    tbool valid = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &valid);
    bail_error(err);

    if (!valid) {
        goto bail;
    }

    // First get all mandatory arguments
    player = tstr_array_get_str_quick(params, 0);
    bail_null(player);

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/active", player);
    bail_null(bn_active);
    bn_digest = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/digest", player);
    bail_null(bn_digest);
    bn_data = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/data/string", player);
    bail_null(bn_data);
    bn_uol_off = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/data/uri_offset", player);
    bail_null(bn_uol_off);
    bn_uol_len = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/data/uri_len", player);
    bail_null(bn_uol_len);
    bn_secret = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/secret/value", player);
    bail_null(bn_secret);
    bn_secret_type = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/secret/type", player);
    bail_null(bn_secret_type);
    bn_uri_query = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/match/uri_query", player);
    bail_null(bn_uri_query);
    bn_url_type = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/url_type", player);
    bail_null(bn_url_type);
    bn_expire_time = smprintf("/nkn/nvsd/virtual_player/config/%s/hash_verify/expiry_time/epochtime", player);
    bail_null(bn_url_type);


    // Set the digest method
    err = tstr_array_linear_search_str(cmd_line, "digest", 0, &idx);
    if (err != lc_err_not_found) {
        err = mdc_set_binding(cli_mcc, NULL, NULL,
                bsso_modify, bt_string, 
                tstr_array_get_str_quick(cmd_line, idx + 1),
                bn_digest);
        bail_error(err);

        err = cli_nvsd_vp_set_state(bn_active, true);
        bail_error(err);
    }

    err = tstr_array_linear_search_str(cmd_line, "match", 0, &idx);
    if (err != lc_err_not_found) {
        //uri_query = tstr_array_get_str_quick(cmd_line, idx + 2);
        //idx = 0;
        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 2), bn_uri_query);
        bail_error(err);
        err = cli_nvsd_vp_set_state(bn_active, true);
        bail_error(err);
    }
    err = 0;

    err = tstr_array_linear_search_str(cmd_line, "uol", 0, &idx);
    if (err == lc_err_not_found) {
        err = tstr_array_linear_search_str(cmd_line, "data", 0, &idx);
        //data = tstr_array_get_str_quick(cmd_line, idx + 1);
        //idx = 0;
        if (err != lc_err_not_found) {
            err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                    tstr_array_get_str_quick(cmd_line, idx + 1), bn_data);
            bail_error(err);
            err = cli_nvsd_vp_set_state(bn_active, true);
            bail_error(err);
        }
    }
    else {
        // uol was found - "data"
        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_uol_off);
        bail_error(err);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
                tstr_array_get_str_quick(cmd_line, idx + 2), bn_uol_len);
        bail_error(err);
        err = cli_nvsd_vp_set_state(bn_active, true);
        bail_error(err);
    }
    err = 0;

    err = tstr_array_linear_search_str(cmd_line, "shared-secret", 0, &idx);
    if (err != lc_err_not_found) {
        //secret_string = tstr_array_get_str_quick(cmd_line, idx + 1);
        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_secret);
        bail_error(err);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 2), bn_secret_type);
        bail_error(err);
        err = cli_nvsd_vp_set_state(bn_active, true);
        bail_error(err);
    }
    err = 0;
    err = tstr_array_linear_search_str(cmd_line, "expiry-time-verify", 0, &idx);
    if (err != lc_err_not_found) {
        //expiry time verify value =  tstr_array_get_str_quick(cmd_line, idx + 2);
        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 2), bn_expire_time);
        bail_error(err);
        err = cli_nvsd_vp_set_state(bn_active, true);
        bail_error(err);
    }
    err = 0;
    err = tstr_array_linear_search_str(cmd_line, "url-type", 0, &idx);
    if (err != lc_err_not_found) {
        //url type value =  tstr_array_get_str_quick(cmd_line, idx + 1);
        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_url_type);
        bail_error(err);
        err = cli_nvsd_vp_set_state(bn_active, true);
        bail_error(err);
    }
    err = 0;
bail:
    safe_free(bn_digest); 
    safe_free(bn_data); 
    safe_free(bn_uol_len); 
    safe_free(bn_uol_off); 
    safe_free(bn_secret); 
    safe_free(bn_secret_type); 
    safe_free(bn_url_type);
    safe_free(bn_expire_time);
    safe_free(bn_uri_query);
    safe_free(bn_active);
    return err;
}


static int 
cli_nvsd_vp_connection_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    char *bn_name = NULL;
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    
    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/max_session_rate",
            c_player);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_reset, bt_uint32,
            "0",
            bn_name);
    bail_error(err);

bail:
    safe_free(bn_name);
    return err;
}


int 
cli_nvsd_vp_connection_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_bandwidth = NULL;
    char *bn_name = NULL;
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_bandwidth = tstr_array_get_str_quick(params, 1);
    bail_null(c_bandwidth);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/max_session_rate", 
            c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
            c_bandwidth, 
            bn_name);
    bail_error(err);

bail:
    safe_free(bn_name);
    return err;
}



static int 
cli_nvsd_vp_fast_start_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    char *bn_name = NULL;
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    
    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
            c_player);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_bool,
            "false",
            bn_name);
    bail_error(err);

bail:
    safe_free(bn_name);
    return err;
}



int 
cli_nvsd_vp_fast_start_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_keyword = NULL;
    const char *c_value = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }


    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_value = tstr_array_get_str_quick(params, 1);

    c_keyword = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(c_keyword);

    /* Handle "query-string-parm" */
    if (safe_strcmp(c_keyword, "query-string-parm") == 0) {
        err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
                "/nkn/nvsd/virtual_player/actions/fast_start/uri_query", 2,
                "name", bt_string, c_player,
                "value", bt_string, c_value);
        bail_error(err);

        goto bail;
    }

    /* Handle "size" */
    if (safe_strcmp(c_keyword, "size") == 0) {
        err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
                "/nkn/nvsd/virtual_player/actions/fast_start/size", 2,
                "name", bt_string, c_player,
                "value", bt_string, c_value);
        bail_error(err);

        goto bail;
    }

    /* Handle "time" */
    if (safe_strcmp(c_keyword, "time") == 0) {
        err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
                "/nkn/nvsd/virtual_player/actions/fast_start/time", 2,
                "name", bt_string, c_player,
                "value", bt_string, c_value);
        bail_error(err);

        goto bail;
    }

    if (ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

bail:
    ts_free(&ret_msg);
    return err;
}



static int 
cli_nvsd_vp_seek_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    char *bn_name = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }


    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/active",
            c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "false", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            "", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            "", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "false", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    /* Get the type of the player. If it is  youtube , set defualt as time-msec, else 
     * byte-offset
     */
    err = cli_nvsd_vp_seek_flv_config_reset(data, cmd, cmd_line, params);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            "time-msec", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }


bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_vp_seek_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_query = NULL;
    char *bn_name = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tstring *existing_value = NULL;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_query = tstr_array_get_str_quick(params, 1);
    bail_null(c_query);



    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",
            c_player);
    bail_null(bn_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
	    &existing_value, bn_name, NULL );
    bail_error(err);

	if(existing_value && ts_length(existing_value) > 0){
		if(!(ts_equal_str(existing_value, c_query, false))) {
			cli_printf(_("Error:seek uri query string is already set to (%s)\n"
						"Reset the value using \"no seek-config query-string-parm *\" \n"),
					ts_str(existing_value));
			goto bail;
		}
	}

    if(cli_prefix_modes_is_enabled()) {
	    err = cli_prefix_enter_mode(cmd, cmd_line);
	    bail_error(err);
    }

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_query, bn_name);
    bail_error(err);

    if ( err == 0 ) {
        ts_free(&ret_msg);
        safe_free(bn_name);
        bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/active",
                c_player);
        bail_null(bn_name);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
                "true", bn_name);
        bail_error(err);
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            "", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "false", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/flv-type",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            "time-msec", bn_name);
    bail_error(err);

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",
		    c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            "time-msec", bn_name);
    bail_error(err);

    if (ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }
#if 0
    idx = tstr_array_length_quick(cmd_line);
    if ( idx == 6 ) {
	ts_free(&ret_msg);  //do we need this
	safe_free(bn_name);

	bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		c_player);
	bail_null(bn_name);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
		"true", bn_name);
	bail_error(err);


    }
    if (idx > 6) {
	c_seek_length =  tstr_array_get_str_quick(params, 2); //, idx + 2);
	bail_null(c_seek_length);

	seek_len_query = smprintf(
		"/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",
		c_player);
	bail_null(seek_len_query);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
		c_seek_length, seek_len_query);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));
    } 
    err = 0;
    if ( idx == 9  )  {
	bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		c_player);
	bail_null(bn_name);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
		"true", bn_name);
	bail_error(err);


    }
#endif
bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    ts_free(&existing_value);
    return err;
}


static int 
cli_nvsd_vp_assured_flow_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    node_name_t bn_name = {0};
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/virtual_player/config/%s/assured_flow/active", c_player);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "false", bn_name);
    bail_error(err);

    if (ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/virtual_player/config/%s/assured_flow/overhead", c_player);
    err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, bn_name);
    bail_error(err);
    if (ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/virtual_player/config/%s/assured_flow/use-mbr", c_player);
    err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, bn_name);
    bail_error(err);
    if (ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }

bail:
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_vp_assured_flow_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_keyword = NULL;
    const char *c_value = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    const char *tmbr = "false";
    const char *c_overhead = NULL;
    uint32 idx = 0;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_keyword = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(c_keyword);

    if (safe_strcmp(c_keyword, "query-string-parm") == 0) {
        c_value = tstr_array_get_str_quick(params, 1);
        bail_null(c_value);

        err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
                "/nkn/nvsd/virtual_player/actions/assured_flow/uri_query", 2,
                "name", bt_string, c_player,
                "value", bt_string, c_value);
        bail_error(err);
        
    }

    /*Updating this CLI for video-pacing support
     * the auto option can have values for delivery-overhead
     * and the max-bit rate option use-mbr
     */
    if (safe_strcmp(c_keyword, "auto") == 0) {
	/* use - mbr option is present*/
	err = tstr_array_linear_search_str(cmd_line, "use-mbr", 0, &idx);
	if (err != lc_err_not_found) {
	    tmbr = "true";
	}
	/* overhead delivery option is present*/
	err = tstr_array_linear_search_str(cmd_line, "delivery-overhead", 0, &idx);
	if (err != lc_err_not_found) {
	    c_overhead = tstr_array_get_str_quick(params, 1);
	    bail_null(c_overhead);
	}
	/*If overhead value is present then we send it with action
	 * else it is not sent to the action handler
	 */
	if(c_overhead){
	    err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
		    "/nkn/nvsd/virtual_player/actions/assured_flow/auto", 4,
		    "name", bt_string, c_player,
		    "value", bt_string, "",
		    "use-mbr", bt_bool, tmbr,
		    "overhead", bt_string, c_overhead);
	    bail_error(err);
	}else {
	    err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
		    "/nkn/nvsd/virtual_player/actions/assured_flow/auto", 3,
		    "name", bt_string, c_player,
		    "value", bt_string, "",
		    "use-mbr", bt_bool, tmbr);
	    bail_error(err);
	}
    }

    if (safe_strcmp(c_keyword, "rate") == 0) {
        c_value = tstr_array_get_str_quick(params, 1);
        bail_null(c_value);

        err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg, 
                "/nkn/nvsd/virtual_player/actions/assured_flow/rate", 2,
                "name", bt_string, c_player,
                "value", bt_string, c_value);
        bail_error(err);
    }
bail:
    ts_free(&ret_msg);
    return err;
}


int 
cli_nvsd_vp_smooth_flow_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_keyword = NULL;
    const char *c_value = NULL;
    tbool is_allowed = false;
    char *bn_name = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_keyword = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(c_keyword);

    c_value = tstr_array_get_str_quick(params, 1);
    bail_null(c_value);

    if (safe_strcmp(c_keyword, "query-string-parm") == 0) {
        bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/smooth_flow/uri_query",
                c_player);
        bail_null(bn_name);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
                c_value, bn_name);
        bail_error(err);

        if (ret_err != 0) {
            cli_printf_error(_("%s"),  ts_str(ret_msg));
            goto bail;
        }

        safe_free(bn_name);
        bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/smooth_flow/active",
                c_player);
        bail_null(bn_name);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
                "true", bn_name);
        bail_error(err);

        if (ret_err != 0) {
            cli_printf_error(_("%s"),  ts_str(ret_msg));
            goto bail;
        }


    }
bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}

static int 
cli_nvsd_vp_rate_map_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_profile = NULL;
    char *bn_name = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_profile = tstr_array_get_str_quick(params, 1);
    bail_null(c_profile);


    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s",
            c_player, c_profile);
    bail_null(bn_name);

    err = mdc_delete_binding(cli_mcc, &ret_err, &ret_msg, bn_name);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}


int 
cli_nvsd_vp_rate_map_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    tbool is_allowed = false;
    char *bn_name = NULL;
    char *bn_rate = NULL;
    char *bn_active = NULL;
    char *bn_uol_l = NULL;
    char *bn_uol_o = NULL;
    char *bn_uri = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    const char *c_profile = NULL;
    const char *c_rate = NULL;
    const char *c_query = NULL;
    const char *c_length = NULL;
    const char *c_offset = NULL;
    uint32 idx = 0;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_profile = tstr_array_get_str_quick(params, 1);
    bail_null(c_profile);

    c_rate = tstr_array_get_str_quick(params, 2);
    bail_null(c_rate);

    /* Create the binding and set node as active */
    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s", 
            c_player, c_profile);
    bail_null(c_player);

    // "active" node
    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/active",
            c_player);
    bail_null(bn_active);

    // "rate" node
    bn_rate = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/rate",
            c_player, c_profile);
    bail_null(bn_rate);


    err = mdc_create_binding(cli_mcc, &ret_err, &ret_msg, bt_string,
            c_profile, bn_name);
    bail_error(err);
    bail_error_msg(ret_err, "Error: %s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "true", bn_active);
    bail_error(err);
    bail_error_msg(ret_err, "Error: %s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_uint32,
            c_rate, bn_rate);
    bail_error(err);
    bail_error_msg(ret_err, "Error: %s", ts_str(ret_msg));


    // These params are optional
    err = tstr_array_linear_search_str(cmd_line, "uol", 0, &idx);
    if (err != lc_err_not_found) {

        c_offset = tstr_array_get_str_quick(cmd_line, idx + 1);
        bail_null(c_offset);

        c_length =  tstr_array_get_str_quick(cmd_line, idx + 2);
        bail_null(c_length);

        bn_uol_o = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/uol/offset",
                c_player, c_profile);
        bail_null(bn_uol_o);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_uint32,
                c_offset, bn_uol_o);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));

        bn_uol_l = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/uol/length",
                c_player,  c_profile);
        bail_null(bn_uol_l);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_uint32,
                c_length, bn_uol_l);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));
    }

    // This is the case when user gives Query string
    err = tstr_array_linear_search_str(cmd_line, "query-string-parm", 0, &idx);
    if (err != lc_err_not_found) {

        c_query = tstr_array_get_str_quick(cmd_line, idx + 1);
        bail_null(c_query);

        bn_uri = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/query_string",
                c_player, c_profile);
        bail_null(bn_uri);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
                c_query, bn_uri);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));
    }

    err = 0;

bail:
    safe_free(bn_name);
    safe_free(bn_active);
    safe_free(bn_rate);
    safe_free(bn_uri);
    safe_free(bn_uol_o);
    safe_free(bn_uol_l);
    ts_free(&ret_msg);
    return err;
}

static int 
cli_nvsd_vp_full_download_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    char *bn_name = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/full_download/active",
            c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "false", bn_name);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));
    

bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_vp_full_download_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_match = NULL;
    const char *c_query = NULL;
    const char *c_header = NULL;
    const char *c_keyword = NULL;
    const char *c_keyword2 = NULL;
    char *bn_name = NULL;
    char *bn_active = NULL;
    char *bn_query = NULL;
    char *bn_header = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_keyword = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(c_keyword);

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/full_download/active",
            c_player);
    bail_null(bn_active);

    if (safe_strcmp(c_keyword, "always") == 0) {
        bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/full_download/always",
                c_player);
        bail_null(bn_name);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool, 
                "true", bn_name);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
                "true", bn_active);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));

        goto bail;
    }

    if (safe_strcmp(c_keyword, "match") == 0) {
        bn_name = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/full_download/match",
                c_player);
        bail_null(bn_name);

        c_match = tstr_array_get_str_quick(params, 1);
        bail_null(c_match);

        c_keyword2 = tstr_array_get_str_quick(cmd_line, 5);
        bail_null(c_keyword2);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, 
                bt_string, c_match, bn_name);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
                "true", bn_active);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));

        if (safe_strcmp(c_keyword2, "query-string-parm") == 0) {
            c_query = tstr_array_get_str_quick(params, 2);
            bail_null(c_query);

            bn_query = smprintf(
                    "/nkn/nvsd/virtual_player/config/%s/full_download/uri_query",
                    c_player);
            bail_null(bn_query);

            err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify,
                    bt_string, c_query, bn_query);
            bail_error(err);
            bail_error_msg(ret_err, "%s", ts_str(ret_msg));
        }

        if (safe_strcmp(c_keyword2, "header") == 0) {
            c_header = tstr_array_get_str_quick(params, 2);
            bail_null(c_header);

            bn_header = smprintf(
                    "/nkn/nvsd/virtual_player/config/%s/full_download/uri_hdr",
                    c_player);
            bail_null(c_header);

            err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify,
                    bt_string, c_header, bn_header);
            bail_error(err);
            bail_error_msg(ret_err, "%s", ts_str(ret_msg));
        }
    }

bail:
    safe_free(bn_name);
    safe_free(bn_active);
    safe_free(bn_query);
    safe_free(bn_header);
    ts_free(&ret_msg);
    return err;
}

static int 
cli_nvsd_vp_health_probe_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    char *bn_name = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/health_probe/active",
            c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool, 
            "false", bn_name);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_vp_health_probe_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_query = NULL;
    const char *c_match = NULL;
    tbool is_allowed = false;
    char *bn_name = NULL;
    char *bn_match = NULL;
    char *bn_active = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_query = tstr_array_get_str_quick(params, 1);
    bail_null(c_query);

    c_match = tstr_array_get_str_quick(params, 2);
    bail_null(c_match);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/health_probe/uri_query",
            c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_query, bn_name);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    bn_match = smprintf("/nkn/nvsd/virtual_player/config/%s/health_probe/match",
            c_player);
    bail_null(bn_match);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_match, bn_match);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/health_probe/active",
            c_player);
    bail_null(bn_active);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool, 
            "true", bn_active);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));


bail:
    safe_free(bn_match);
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}


int 
cli_nvsd_vp_control_point_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_control = NULL;
    char *bn_name = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_control = tstr_array_get_str_quick(params, 1);
    bail_null(c_control);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/control_point",
            c_player);
    bail_null(bn_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_control, bn_name);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
    safe_free(bn_name);
    ts_free(&ret_msg);
    return err;
}


int 
cli_nvsd_vp_req_auth_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    tbool is_allowed = false;
    char *bn_active = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);
    
    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/active",
            c_player);
    bail_null(bn_active);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool, 
            "false", bn_active);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
    safe_free(bn_active);
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_vp_req_auth_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_streamid = NULL;
    const char *c_authid = NULL;
    const char *c_secret = NULL;
    const char *c_time = NULL;
    const char *c_match = NULL;
    char *bn_streamid = NULL;
    char *bn_authid = NULL;
    char *bn_secret = NULL;
    char *bn_time = NULL;
    char *bn_match = NULL;
    char *bn_active = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_streamid = tstr_array_get_str_quick(params, 1);
    bail_null(c_streamid);

    c_authid = tstr_array_get_str_quick(params, 2);
    bail_null(c_authid);

    c_secret = tstr_array_get_str_quick(params, 3);
    bail_null(c_secret);

    c_time = tstr_array_get_str_quick(params, 4);
    bail_null(c_time);

    c_match = tstr_array_get_str_quick(params, 5);
    bail_null(c_match);

    bn_streamid = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/stream_id/uri_query",
            c_player);
    bail_null(bn_streamid);

    bn_authid = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/auth_id/uri_query",
            c_player);
    bail_null(bn_authid);

    bn_secret = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/secret/value",
            c_player);
    bail_null(bn_secret);

    bn_time = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/time_interval",
            c_player);
    bail_null(bn_time);

    bn_match = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/match/uri_query",
            c_player);
    bail_null(bn_match);

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/req_auth/active",
            c_player);
    bail_null(bn_active);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string, 
            c_streamid, bn_streamid);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string, 
            c_authid, bn_authid);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string, 
            c_secret, bn_secret);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_uint32, 
            c_time, bn_time);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string, 
            c_match, bn_match);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool, 
            "true", bn_active);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));


bail:
    safe_free(bn_streamid);
    safe_free(bn_authid);
    safe_free(bn_secret);
    safe_free(bn_time);
    safe_free(bn_match);
    safe_free(bn_active);
    ts_free(&ret_msg);
    return err;
}


int 
cli_nvsd_vp_prestage_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    char *bn_active = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/prestage/active",
            c_player);
    bail_null(bn_active);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "false", bn_active);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
    safe_free(bn_active);
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_quality_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_query = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}
	
	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);

#if 0
	/* get the command */
	c_query_tag = tstr_array_get_str_quick(cmd_line, 3);
	bail_null(c_query_tag);
#endif

	//lc_log_basic(LOG_DEBUG, "c_query_tag is %s", c_query_tag);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	node = smprintf("/nkn/nvsd/virtual_player/config/%s/quality-tag",c_vp_name);
	bail_null(node);
	
	c_query = "QualityLevels";

	lc_log_basic(LOG_DEBUG, "c_query is %s", c_query);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_reset, bt_string,
			c_query, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	ts_free(&ret_msg);
	return err;

}

int 
cli_nvsd_vp_signals_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_session = NULL;
    const char *c_state = NULL;
    const char *c_profile = NULL;
    char *bn_session = NULL;
    char *bn_state_query = NULL;
    char *bn_profile = NULL;
    char *bn_chunk = NULL;
    char *bn_active = NULL;
    tbool is_allowed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_session = tstr_array_get_str_quick(params, 1);
    bail_null(c_session);

    c_state = tstr_array_get_str_quick(params, 2);
    bail_null(c_state);

    c_profile = tstr_array_get_str_quick(params, 3);
    bail_null(c_profile);

    bn_session = smprintf("/nkn/nvsd/virtual_player/config/%s/signals/session_id/uri_query",
            c_player);
    bail_null(bn_session);

    bn_state_query = smprintf("/nkn/nvsd/virtual_player/config/%s/signals/state/uri_query",
            c_player);
    bail_null(bn_state_query);

    bn_profile = smprintf("/nkn/nvsd/virtual_player/config/%s/signals/profile/uri_query",
            c_player);
    bail_null(bn_profile);

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/signals/active",
            c_player);
    bail_null(bn_active);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_session, bn_session);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_state, bn_state_query);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
            c_profile, bn_profile);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
            "true", bn_active);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));
#if 0
    err = tstr_array_linear_search_str(cmd_line, "chunk", 0, &idx);
    if (err != lc_err_not_found) {
        c_chunk = tstr_array_get_str_quick(params, 4);
        bail_null(c_chunk);

        bn_chunk = smprintf("/nkn/nvsd/virtual_player/config/%s/signals/chunk/uri_query",
                c_player);
        bail_null(bn_chunk);

        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
                c_chunk, bn_chunk);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));

    }
#endif
    err = 0;
bail:
    safe_free(bn_session);
    safe_free(bn_state_query);
    safe_free(bn_profile);
    safe_free(bn_chunk);
    safe_free(bn_active);
    ts_free(&ret_msg);
    return err;
}


int 
cli_nvsd_video_id_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
    const char *c_player = NULL;
    const char *c_videoid = NULL;
    const char *c_formattag = NULL;
	tbool is_allowed = false;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_videoid = NULL;
	char *bn_active = NULL;
	char *bn_formattag = NULL;


	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}

	c_player = tstr_array_get_str_quick(params, 0);
	bail_null(c_player);

	c_videoid = tstr_array_get_str_quick(params, 1);
	bail_null(c_videoid);

	c_formattag = tstr_array_get_str_quick(params, 2);
	bail_null(c_formattag);

	bn_videoid = smprintf("/nkn/nvsd/virtual_player/config/%s/video_id/uri_query", c_player);
	bail_null(bn_videoid);

	bn_formattag = smprintf("/nkn/nvsd/virtual_player/config/%s/video_id/format_tag", c_player);
	bail_null(bn_formattag);

	bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/video_id/active", c_player);
	bail_null(bn_active);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_videoid, bn_videoid);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_formattag, bn_formattag);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
		"true", bn_active);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(bn_videoid);
	safe_free(bn_active);
	safe_free(bn_formattag);
	ts_free(&ret_msg);
	return err;
}

int 
cli_nvsd_quality_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_query = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	char *bn_active = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}
	
	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);
#if 0
	/* get the command */
	c_query_tag = tstr_array_get_str_quick(cmd_line, 2);
	bail_null(c_query_tag);
#endif
	/* get the query-param */
	c_query = tstr_array_get_str_quick(params, 1);
	bail_null(c_query);


	lc_log_basic(LOG_DEBUG, "c_query is %s", c_query);
	//lc_log_basic(LOG_DEBUG, "c_query_tag is %s", c_query_tag);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	node = smprintf("/nkn/nvsd/virtual_player/config/%s/quality-tag",c_vp_name);
	bail_null(node);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	safe_free(bn_active);
	ts_free(&ret_msg);
	return err;
}




#if 1

int
cli_nvsd_virtual_player_rate_map_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *player = NULL;
    const char *profile = NULL;
    const char *rate =  NULL;
    char *bn_active = NULL;
    char *bn_name = NULL;
    char *bn_match = NULL;
    char *bn_rate = NULL;
    char *bn_uol_offset = NULL;
    char *bn_uol_len = NULL;
    char *bn_uri_query = NULL;
    uint32 idx = 0;

    tbool valid = false;
    err = cli_virtual_player_check_and_create(data, cmd, cmd_line, params, &valid);
    bail_error(err);

    player = tstr_array_get_str_quick(params, 0);
    bail_null(player);
    profile = tstr_array_get_str_quick(params, 1);
    bail_null(profile);
    rate = tstr_array_get_str_quick(params, 2);
    bail_null(rate);

    bn_active = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/active", player);
    bail_null(bn_active);

    cli_nvsd_vp_set_state(bn_active, true);

    bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s",
            player, profile);
    bail_null(bn_name);

    err = mdc_create_binding(cli_mcc, NULL, NULL, bt_string, 
            profile,
            bn_name);
    bail_error(err);

    // Setup parameters for this profile
    bn_rate = smprintf("/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/rate",
            player, profile);
    bail_null(bn_rate);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
            rate,
            bn_rate);
    bail_error(err);

    // These params are optional
    err = tstr_array_linear_search_str(cmd_line, "uol", 0, &idx);
    if (err != lc_err_not_found) {

        bn_uol_offset = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/uol/offset",
                player, profile);
        bail_null(bn_uol_offset);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_uol_offset);
        bail_error(err);

        bn_uol_len = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/uol/length",
                player,  profile);
        bail_null(bn_uol_len);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
                tstr_array_get_str_quick(cmd_line, idx + 2), bn_uol_len);
        bail_error(err);
    }
    // This is the case when user gives Query string
    err = tstr_array_linear_search_str(cmd_line, "query-string-parm", 0, &idx);
    if (err != lc_err_not_found) {
        bn_uri_query = smprintf(
                "/nkn/nvsd/virtual_player/config/%s/rate_map/match/%s/query_string",
                player, profile);
        bail_null(bn_uri_query);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_uri_query);
        bail_error(err);
    }
    err = 0;

bail:
    safe_free(bn_name);
    safe_free(bn_match);
    safe_free(bn_rate);
    safe_free(bn_uol_len);
    safe_free(bn_uol_offset);
    safe_free(bn_uri_query);
    return err;
}

int
cli_nvsd_virtual_player_full_download_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *player = NULL;
    char *bn_uri_query = NULL,
         *bn_header = NULL,
         *bn_match = NULL;
    uint32 idx = 0;

    tbool valid = false;
    err = cli_virtual_player_check_and_create(data, cmd, cmd_line, params, &valid);
    bail_error(err);

    //tstr_array_print(cmd_line, "cmds");
    //tstr_array_print(params, "params");

    player = tstr_array_get_str_quick(params, 0);
    bail_null(player);

    bn_match = smprintf("/nkn/nvsd/virtual_player/config/%s/full_download/match", player);
    bail_null(bn_match);

    err = tstr_array_linear_search_str(cmd_line, "header", 0, &idx);
    if (err != lc_err_not_found) {
        bn_header = smprintf("/nkn/nvsd/virtual_player/config/%s/full_download/uri_hdr", player);
        bail_null(bn_header);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_header);
        bail_error(err);
    }

    err = tstr_array_linear_search_str(cmd_line, "query-string-parm", 0, &idx);
    if (err != lc_err_not_found) {
        bn_uri_query = smprintf("/nkn/nvsd/virtual_player/config/%s/full_download/uri_query", player);
        bail_null(bn_uri_query);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_uri_query);
        bail_error(err);
    }

    err = tstr_array_linear_search_str(cmd_line, "match", 0, &idx);
    if (err != lc_err_not_found) {
        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
                tstr_array_get_str_quick(cmd_line, idx + 1), bn_match);
        bail_error(err);
    }
    //err = 0;

bail:
    safe_free(bn_uri_query);
    safe_free(bn_header);
    safe_free(bn_match);
    return err;
}


int
cli_virtual_player_name_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *name = NULL;
    int i = 0, num_names = 0;

    switch(help_type)
    {
    case cht_termination:
        if (cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
        }
        break;

    case cht_expansion:
        if (cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&names, NULL);
        bail_error(err);

        err = cli_nvsd_virtual_player_name_comp(data, cmd, cmd_line, params, 
                curr_word, names);
        bail_error(err);

        num_names = tstr_array_length_quick(names);
        for(i = 0; i < num_names; ++i) {
            name  = tstr_array_get_str_quick(names, i);
            bail_null(name);

            err = cli_add_expansion_help(context, name, NULL);
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

bail:
    tstr_array_free(&names);
    return err;
}

int
cli_nvsd_virtual_player_name_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *names = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_nvsd_virtual_player_get_player_names(&names, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    return err;
}

int
cli_nvsd_virtual_player_seek_param_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    char *node_name = NULL;
    const char *param_value = NULL;
    tstring *ret_value = NULL;
    tbool is_allowed = false;

    UNUSED_ARGUMENT(curr_word);

    param_value = tstr_array_get_str_quick(params, 0);

    bail_null(data);

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);
 
    if (!is_allowed) {
	    goto bail;
    }  

    node_name  = smprintf((const char*)data, tstr_array_get_str_quick(params, 0));
    bail_null(node_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
	    &ret_value,node_name,NULL );
    bail_error(err);

    err = tstr_array_append_str(ret_completions,ts_str(ret_value));
    bail_error(err);


bail:
    ts_free(&ret_value);
    return err;
}


/*
 * Check whether player name exists or not.
 */
static int
cli_nvsd_virtual_player_validate(
        const char *name,
        const char *type, 
        tbool *valid, 
        int *ret_err)
{
    int err = 0;
    tstr_array *names = NULL;
    uint32 idx = 0;
    bn_binding *binding = NULL;
    uint32 vp_type;
    uint32 t = atoi(type);


    bail_null(valid);
    *valid = false;

    bail_null(ret_err);
    *ret_err = 0;

    err = cli_nvsd_virtual_player_get_player_names(&names, false);
    bail_error(err);

    err = tstr_array_linear_search_str(names, name, 0, &idx);
    if (err != lc_err_not_found) {
        // Player already exists. Check whether the user gave the correct type 
        // or not.
        
        err = mdc_get_binding_fmt(
                cli_mcc, NULL, NULL, false, &binding, NULL, 
                "/nkn/nvsd/virtual_player/config/%s/type", name);
        bail_error(err);

        if (binding) {
            err = bn_binding_get_uint32(binding, ba_value, NULL, &vp_type);
            bail_error(err);

            if (t == vp_type) {
                // This player is correctly registered against the type requested
                *valid = true;
            }
            else {
                //cli_printf_error(_("player %s is inconsistent with its type definitions\n"), name);
                *ret_err = lc_err_generic_error;
                bail_error(err);
            }
        }
    }
    else {
    }

bail:
    tstr_array_free(&names);
    bn_binding_free(&binding);
    return err;
}


static int
cli_nvsd_virtual_player_get_player_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &names_config,
            "/nkn/nvsd/virtual_player/config", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;

bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    return err;
}



/*----------------------------------------------------------------------------
 * Helper APIs to get match strings configure under a node.
 *--------------------------------------------------------------------------*/
static int 
cli_nvsd_virtual_player_get_match_strings(
        tstr_array **ret_strings,
        tbool hide_display,
        const char *name)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    tstring *value = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_strings);
    *ret_strings = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL,
            NULL, &value, name, NULL);
    bail_error_null(err, value);
//
    //err = mdc_get_binding_children_tstr_array(cli_mcc,
            //NULL, NULL, &names_config,
            //name, NULL);
    //bail_error_null(err, names_config);

    err = tstr_array_append_str(names, ts_str(value));
    //err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_strings = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    ts_free(&value);
    return err;
}



/*
 * "data" is passed with the node leading to uri_query
 */
int 
cli_nvsd_virtual_player_match_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *names = NULL;
    char *pnode = NULL; //(const char*) (data);

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    bail_null(data);

    // Create the node to look for
    pnode = smprintf((const char*) data, tstr_array_get_str_quick(params, 0));
    bail_null(pnode);

    //cli_printf(_("Node: %s\n"), pnode);

    err = cli_nvsd_virtual_player_get_match_strings(&names, true, pnode);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    safe_free(pnode);
    return err;
}


int
cli_nvsd_virtual_player_match_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *name = NULL;
    int i = 0, num_names = 0;

    switch(help_type)
    {
    case cht_termination:
        if (cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
        }
        break;

    case cht_expansion:
        if (cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&names, NULL);
        bail_error(err);

        err = cli_nvsd_virtual_player_match_comp(data, cmd, cmd_line, params, 
                curr_word, names);
        bail_error(err);

        num_names = tstr_array_length_quick(names);
        for(i = 0; i < num_names; ++i) {
            name  = tstr_array_get_str_quick(names, i);
            bail_null(name);

            err = cli_add_expansion_help(context, name, NULL);
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

bail:
    tstr_array_free(&names);
    return err;
}

/*---------------------------------------------------------------------------
 * Specific to rate_map
 *-------------------------------------------------------------------------*/
static int 
cli_nvsd_virtual_player_rate_map_get_match_strings(
        tstr_array **ret_strings,
        tbool hide_display,
        const char *name)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_strings);
    *ret_strings = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc, NULL, NULL,
            &names_config, name, NULL);
    bail_error_null(err, names_config);
    
    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_strings = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    //ts_free(&value);
    return err;
}



/*
 * "data" is passed with the node leading to uri_query
 */
int 
cli_nvsd_virtual_player_rate_map_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *names = NULL;
    char *pnode = NULL; //(const char*) (data);

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    bail_null(data);

    // Create the node to look for
    pnode = smprintf((const char*) data, tstr_array_get_str_quick(params, 0));
    bail_null(pnode);

    err = cli_nvsd_virtual_player_rate_map_get_match_strings(&names, true, pnode);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    safe_free(pnode);
    return err;
}


int
cli_nvsd_virtual_player_rate_map_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *name = NULL;
    int i = 0, num_names = 0;

    switch(help_type)
    {
    case cht_termination:
        if (cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
        }
        break;

    case cht_expansion:
        if (cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&names, NULL);
        bail_error(err);

        err = cli_nvsd_virtual_player_rate_map_comp(data, cmd, cmd_line, params, 
                curr_word, names);
        bail_error(err);

        num_names = tstr_array_length_quick(names);
        for(i = 0; i < num_names; ++i) {
            name  = tstr_array_get_str_quick(names, i);
            bail_null(name);

            err = cli_add_expansion_help(context, name, NULL);
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

bail:
    tstr_array_free(&names);
    return err;
}

#endif

static int
cli_nvsd_vp_set_state(
        char *binding,
        tbool state)
{
    int err = 0;
    
    bail_null(binding);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_bool, 
            (state ? "true" : "false"),
            binding);
    bail_error(err);

bail:
    return err;
}

/*---------------------------------------------------------------------------
 *-------------------------------------------------------------------------*/
#define CONSUME_REVMAP_NODES(c) \
    {\
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name);\
        bail_error(err);\
    }
/*---------------------------------------------------------------------------
 *-------------------------------------------------------------------------*/
typedef int (*cli_vp_revmap_callback)(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name);

cli_vp_revmap_callback revmap_exec[] = {
    cli_vp_hash_verify_revmap,
    cli_vp_full_download_revmap,
    cli_vp_fast_start_revmap,
    cli_vp_seek_revmap,
    cli_vp_req_auth_revmap,
    cli_vp_smooth_flow_revmap,
    //cli_vp_assured_flow_revmap,
    cli_vp_health_probe_revmap,
    cli_vp_signals_revmap,
    cli_vp_connection_revmap,
    cli_vp_prestage_revmap,
    cli_vp_control_point_revmap,
    cli_vp_rate_map_revmap,
    cli_vp_video_id_revmap,
    cli_vp_smoothstream_pub_revmap,
    cli_vp_flashstream_pub_revmap,
    cli_vp_bw_opt_revmap,
    cli_vp_rate_control_revmap,
    NULL
};

/* ------------------------------------------------------------------------- */
int 
cli_vp_hash_verify_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_hv_active = NULL;
    tstring *hv_data_string = NULL;
    tstring *hv_digest = NULL;
    tstring *hv_match_uri = NULL;
    tstring *hv_secret_type = NULL;
    tstring *hv_secret_value = NULL;
    tstring *hv_data_uol_off = NULL;
    tstring *hv_data_uol_len = NULL;
    tstring *hv_expiry_time = NULL;
    tstring *hv_url_type = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    // process hash verify nodes 
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_hv_active, "%s/hash_verify/active", name);
    bail_error(err);

    if (ts_equal_str(is_hv_active, "true", false)) {

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &hv_digest, "%s/hash_verify/digest", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &hv_data_string, "%s/hash_verify/data/string", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_data_uol_off, "%s/hash_verify/data/uri_offset", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_data_uol_len, "%s/hash_verify/data/uri_len", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_match_uri, "%s/hash_verify/match/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_secret_type, "%s/hash_verify/secret/type", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_secret_value, "%s/hash_verify/secret/value", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_expiry_time, "%s/hash_verify/expiry_time/epochtime", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &hv_url_type, "%s/hash_verify/url_type", name);
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, "     hash-verify digest %s\n", ts_str(hv_digest));
        bail_error(err);


        if ( ts_length(hv_data_string) != 0) {
            err = ts_append_sprintf(rev_cmd, " data %s", ts_str(hv_data_string));
            bail_error(err);
        }
        else {
            if (ts_equal_str(hv_data_uol_off, "0", true) && ts_equal_str(hv_data_uol_len, "0", false)) {
                // Default case.. dont print any command
            } else {
                err = ts_append_sprintf(rev_cmd, " data uol %s %s", 
                        ts_str(hv_data_uol_off), ts_str(hv_data_uol_len));
                bail_error(err);
            }
        }

	err = ts_append_sprintf(rev_cmd, "     hash-verify shared-secret \"%s\" %s\n", 
		ts_str_maybe_empty(hv_secret_value), ts_str(hv_secret_type));
	bail_error(err);

	err = ts_append_sprintf(rev_cmd, "     hash-verify match query-string-parm \"%s\" \n", 
		ts_str_maybe_empty(hv_match_uri));
	bail_error(err);

	err = ts_append_sprintf(rev_cmd, "     hash-verify expiry-time-verify query-string-parm \"%s\" \n", 
		ts_str_maybe_empty(hv_expiry_time));
	bail_error(err);

	err = ts_append_sprintf(rev_cmd, "     hash-verify url-type \"%s\" \n", 
		ts_str_maybe_empty(hv_url_type));
	bail_error(err);

#if 0
	err = ts_append_sprintf(rev_cmd, "   exit\n");
	bail_error(err);
#endif
    }

    CONSUME_REVMAP_NODES("%s/type");
    CONSUME_REVMAP_NODES("%s/hash_verify/secret/value");
    CONSUME_REVMAP_NODES("%s/hash_verify/secret/type");
    CONSUME_REVMAP_NODES("%s/hash_verify/data/string");
    CONSUME_REVMAP_NODES("%s/hash_verify/data/uri_offset");
    CONSUME_REVMAP_NODES("%s/hash_verify/data/uri_len");
    CONSUME_REVMAP_NODES("%s/hash_verify/match/uri_query");
    CONSUME_REVMAP_NODES("%s/hash_verify/digest");
    CONSUME_REVMAP_NODES("%s/hash_verify/active");
    CONSUME_REVMAP_NODES("%s/hash_verify/expiry_time/epochtime");
    CONSUME_REVMAP_NODES("%s/hash_verify/url_type");

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_seek_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *seek_uri = NULL;
    tstring *seek_length_uri = NULL;
    tstring *is_seek_active = NULL;
    tstring *rev_cmd = (tstring *) data;
    tstring *enable_tunnel = NULL;
    tstring *flv_type = NULL;
    tstring *mp4_type = NULL;
    tstring *use_9byte_hdr = NULL;


    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    // Process seek nodes
    //
    // /nkn/nvsd/virtual_player/config/breakplayer/seek/active = true (bool)
    // /nkn/nvsd/virtual_player/config/breakplayer/seek/uri_query = fs (string)

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_seek_active, "%s/seek/active", name);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
	    NULL, &enable_tunnel, "%s/seek/enable_tunnel", name);
    bail_error(err);

    if (ts_equal_str(is_seek_active, "true", false)) {
        /* Create reverse map for seek uri-query */
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &seek_uri, "%s/seek/uri_query", name);
        bail_error(err);
		
            err = ts_append_sprintf(rev_cmd, "    seek-config query-string-parm \"%s\"\n", 
                    ts_str_maybe_empty(seek_uri));
            bail_error(err);

        /* Create reverse map for seek length uri-query */
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &seek_length_uri, "%s/seek/length/uri_query", name);
        bail_error(err);

	err = ts_append_sprintf(rev_cmd,
		"     seek-length query-string-parm \"%s\"\n", ts_str_maybe_empty(seek_length_uri));
	bail_error(err);
	/* Create reverse map for seek enable tunnel*/
	if (ts_equal_str(enable_tunnel, "true", false)) {
		err = ts_append_sprintf(rev_cmd,
			"     tunnel-mode enable\n");
		bail_error(err);
	}

	/*Create reverse map for seek flv-type*/
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &flv_type, "%s/seek/flv-type", name);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
		NULL, &use_9byte_hdr, "%s/seek/use_9byte_hdr", name);
	bail_error(err);
	
	if((ts_equal_str(use_9byte_hdr, "true", false)) &&
		(ts_equal_str(flv_type, "byte-offset", false)) ){
		err = ts_append_sprintf(rev_cmd,
			"     seek-flv-type \"%s\" use-9byte-hdr\n", ts_str_maybe_empty(flv_type));
		bail_error(err);
	}else {
	    err = ts_append_sprintf(rev_cmd,
		    "     seek-flv-type \"%s\"\n", ts_str_maybe_empty(flv_type));
	    bail_error(err);
	}
	/*Create reverse map for seek mp4-type*/
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &mp4_type, "%s/seek/mp4-type", name);
	bail_error(err);

	err = ts_append_sprintf(rev_cmd,
			"     seek-mp4-type \"%s\"\n", ts_str_maybe_empty(mp4_type));
	bail_error(err);
    	err = ts_append_sprintf(rev_cmd, "   exit\n");
	bail_error(err);
    }

    CONSUME_REVMAP_NODES("%s/seek/uri_query");
    CONSUME_REVMAP_NODES("%s/seek/length/uri_query");
    CONSUME_REVMAP_NODES("%s/seek/active");
    CONSUME_REVMAP_NODES("%s/seek/enable_tunnel");
    CONSUME_REVMAP_NODES("%s/seek/flv-type");
    CONSUME_REVMAP_NODES("%s/seek/mp4-type");
    CONSUME_REVMAP_NODES("%s/seek/use_9byte_hdr");
bail:
    ts_free(&flv_type);
    ts_free(&use_9byte_hdr);
    ts_free(&mp4_type);
    ts_free(&enable_tunnel);
    ts_free(&seek_length_uri);
    ts_free(&seek_uri);
    ts_free(&is_seek_active);
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_fast_start_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *fs_time = NULL;
    tstring *fs_size = NULL;
    tstring *fs_uri = NULL;
    tstring *fs_valid = NULL;
    tstring *is_fstart_active = NULL;
    tstr_array *fs_name_parts = NULL;
    const char *valid_link = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_fstart_active, "%s/fast_start/active", name);
    bail_error(err);

    if ( ts_equal_str(is_fstart_active, "true", false)) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &fs_valid, "%s/fast_start/valid", name);
        bail_error(err);

        /// Check which node is configured
        if (ts_length(fs_valid) != 0) {
            err = bn_binding_name_to_parts(ts_str(fs_valid), &fs_name_parts, NULL);
            bail_error(err);

            valid_link = tstr_array_get_str_quick(
                    fs_name_parts, tstr_array_length_quick(fs_name_parts) - 1);

            if (strcmp(valid_link, "size") == 0) {

                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &fs_size, "%s/fast_start/size", name);
                bail_error(err);

                err = ts_append_sprintf(rev_cmd, "     fast-start size \"%s\"\n", ts_str_maybe_empty(fs_size));
                bail_error(err);
            }
#if 1
            else if (strcmp(valid_link, "time") == 0) {

                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &fs_time, "%s/fast_start/time", name);
                bail_error(err);
                
                err = ts_append_sprintf(rev_cmd, "     fast-start time \"%s\" \n", ts_str_maybe_empty(fs_time));
                bail_error(err);
            }
#endif
            else if (strcmp(valid_link, "uri_query") == 0) {

                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &fs_uri, "%s/fast_start/uri_query", name);
                bail_error(err);
                
                err = ts_append_sprintf(rev_cmd, "     fast-start query-string-parm \"%s\" \n", ts_str_maybe_empty(fs_uri));
                bail_error(err);
            }
            else {
                /* Nothing to show */
            }
        }
    }

    CONSUME_REVMAP_NODES("%s/fast_start/active");
    CONSUME_REVMAP_NODES("%s/fast_start/size");
    CONSUME_REVMAP_NODES("%s/fast_start/time");
    CONSUME_REVMAP_NODES("%s/fast_start/valid");
    CONSUME_REVMAP_NODES("%s/fast_start/uri_query");

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_assured_flow_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *af_auto = NULL;
    tstring *af_auto_mbr = NULL;
    tstring *af_auto_overhead = NULL;
    tstring *af_rate = NULL;
    tstring *af_uri = NULL;
    tstring *af_valid = NULL;
    tstring *is_aflow_active = NULL;
    const char *valid_link = NULL;
    tstr_array *af_name_parts = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);


    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_aflow_active, "%s/assured_flow/active", name);
    bail_error(err);
   
    if ( ts_equal_str(is_aflow_active, "true", false)) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &af_valid, "%s/assured_flow/valid", name);
        bail_error(err);
        
        /// Check which node is configured
        if (ts_length(af_valid) != 0) {

            err = bn_binding_name_to_parts(ts_str(af_valid), &af_name_parts, NULL);
            bail_error(err);

            valid_link = tstr_array_get_str_quick(
                    af_name_parts, tstr_array_length_quick(af_name_parts) - 1);

            if (strcmp(valid_link, "rate") == 0) {
                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &af_rate, "%s/assured_flow/rate", name);
                bail_error(err);
                
                err = ts_append_sprintf(rev_cmd, "     assured-flow rate \"%s\"\n", 
                        ts_str_maybe_empty(af_rate));
                bail_error(err);
            }
            else if (strcmp(valid_link, "auto") == 0) {
                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &af_auto, "%s/assured_flow/auto", name);
                bail_error(err);

                err = ts_append_str(rev_cmd, "     assured-flow auto");
                bail_error(err);
		/* Get the use-mbr flag*/
                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &af_auto_mbr, "%s/assured_flow/use-mbr", name);
                bail_error(err);
		if (ts_equal_str(af_auto_mbr, "true", false)) {
		    err = ts_append_str(rev_cmd, "     use-mbr ");
		    bail_error(err);
		}

		
		/* Get the delivery-overhead flag*/
                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &af_auto_overhead, "%s/assured_flow/overhead", name);
                bail_error(err);
		if (!(ts_equal_str(af_auto_overhead, "1.25", false))) {
		    err = ts_append_sprintf(rev_cmd, " delivery-overhead %s", ts_str(af_auto_overhead));
		    bail_error(err);
		}
                err = ts_append_str(rev_cmd, "\n");
                bail_error(err);

            }
            else if (strcmp(valid_link, "uri_query") == 0) {
                err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &af_uri, "%s/assured_flow/uri_query", name);
                bail_error(err);

                err = ts_append_sprintf(
                        rev_cmd, "     assured-flow query-string-parm \"%s\"\n", 
                        ts_str_maybe_empty(af_uri));
                bail_error(err);
            }
            else {
                /* Do nothing here */
            }
        }
    }

    CONSUME_REVMAP_NODES("%s/assured_flow/valid");
    CONSUME_REVMAP_NODES("%s/assured_flow/auto");
    CONSUME_REVMAP_NODES("%s/assured_flow/rate");
    CONSUME_REVMAP_NODES("%s/assured_flow/uri_query");
    CONSUME_REVMAP_NODES("%s/assured_flow/active");
    CONSUME_REVMAP_NODES("%s/assured_flow/use-mbr");
    CONSUME_REVMAP_NODES("%s/assured_flow/overhead");
    
bail:
    ts_free(&af_auto);
    ts_free(&af_auto_mbr);
    ts_free(&af_auto_overhead);
    ts_free(&af_rate);
    ts_free(&af_uri);
    ts_free(&af_valid);
    ts_free(&is_aflow_active);


    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_connection_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *t_bwidth = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_bwidth, "%s/max_session_rate", name);
    bail_error(err);

    if(t_bwidth && strcmp(ts_str(t_bwidth), "0")) {
	    err = ts_append_sprintf(rev_cmd,
			    "     connection max-bandwidth %s\n", ts_str(t_bwidth));
	    bail_error(err);
    }
    CONSUME_REVMAP_NODES("%s/max_session_rate");

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_health_probe_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_active = NULL;
    tstring *t_query = NULL;
    tstring *t_match = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
            NULL, &is_active, "%s/health_probe/active", name);
    bail_error(err);

    if (ts_equal_str(is_active, "true", false)) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_match, "%s/health_probe/match", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_query, "%s/health_probe/uri_query", name);
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, 
                "     health-probe query-string-parm \"%s\" match \"%s\" \n",
                ts_str_maybe_empty(t_query), ts_str_maybe_empty(t_match));
        bail_error(err);
    }

    CONSUME_REVMAP_NODES("%s/health_probe/active");
    CONSUME_REVMAP_NODES("%s/health_probe/match");
    CONSUME_REVMAP_NODES("%s/health_probe/uri_query");

bail:
    return err;
}


/* ------------------------------------------------------------------------- */
int 
cli_vp_signals_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_active = NULL;
    tstring *t_profile = NULL,
            *t_sessionid = NULL,
            *t_state = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_active, "%s/signals/active", name);
    bail_error(err);

    if ( ts_equal_str(is_active, "true", false) ) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_sessionid, "%s/signals/session_id/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_state, "%s/signals/state/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_profile, "%s/signals/profile/uri_query", name);
        bail_error(err);
#if 0
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_chunk, "%s/signals/chunk/uri_query", name);
        bail_error(err);
#endif
        err = ts_append_sprintf(rev_cmd,
                "     signals session-id query-string-parm \"%s\" "
                "state query-string-parm \"%s\" "
                "profile query-string-parm \"%s\" \n",
                ts_str_maybe_empty(t_sessionid), ts_str_maybe_empty(t_state), ts_str_maybe_empty(t_profile));
        bail_error(err);
#if 0
        if ( ts_length(t_chunk) > 0) {
            err = ts_append_sprintf(rev_cmd, "chunk query-string-parm %s\n",
                    ts_str(t_chunk));
            bail_error(err);
        } else {
            err = ts_append_str(rev_cmd, "\n");
            bail_error(err);
        }
#endif
    }
    CONSUME_REVMAP_NODES("%s/signals/active");
//    CONSUME_REVMAP_NODES("%s/signals/chunk/uri_query");
    CONSUME_REVMAP_NODES("%s/signals/profile/uri_query");
    CONSUME_REVMAP_NODES("%s/signals/session_id/uri_query");
    CONSUME_REVMAP_NODES("%s/signals/state/uri_query");

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_req_auth_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_active = NULL;
    tstring *t_streamid = NULL,
            *t_authid = NULL,
            *t_secret = NULL,
            *t_time = NULL,
            *t_match = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_active, "%s/req_auth/active", name);
    bail_error(err);

    if ( ts_equal_str(is_active, "true", false) ) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_streamid, "%s/req_auth/stream_id/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_authid, "%s/req_auth/auth_id/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_secret, "%s/req_auth/secret/value", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_match, "%s/req_auth/match/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_time, "%s/req_auth/time_interval", name);
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, 
                "     req-auth digest md-5 "
                "stream-id query-string-parm \"%s\" "
                "auth-id query-string-parm \"%s\" "
                "shared-secret \"%s\" time-interval \"%s\"  "
                "match query-string-parm \"%s\" \n",
                ts_str_maybe_empty(t_streamid), ts_str_maybe_empty(t_authid), 
                ts_str_maybe_empty(t_secret), ts_str_maybe_empty(t_time), 
                ts_str_maybe_empty(t_match));
        bail_error(err);
    }

    CONSUME_REVMAP_NODES("%s/req_auth/active");
    CONSUME_REVMAP_NODES("%s/req_auth/stream_id/uri_query");
    CONSUME_REVMAP_NODES("%s/req_auth/auth_id/uri_query");
    CONSUME_REVMAP_NODES("%s/req_auth/secret/value");
    CONSUME_REVMAP_NODES("%s/req_auth/match/uri_query");
    CONSUME_REVMAP_NODES("%s/req_auth/time_interval");
    CONSUME_REVMAP_NODES("%s/req_auth/digest");

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_prestage_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_prestage_active = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_prestage_active, "%s/prestage/active", name);
    bail_error(err);

#if 0
    if (ts_equal_str(is_prestage_active, "true", false)) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &c_indicator, "%s/prestage/indicator/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &c_namespace, "%s/prestage/namespace/uri_query", name);
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, "  prestage indicator query-string-parm %s "
                "namespace query-string-parm %s\n", ts_str(c_indicator), ts_str(c_namespace));
        bail_error(err);
    }
#endif

    CONSUME_REVMAP_NODES("%s/prestage/active");
    CONSUME_REVMAP_NODES("%s/prestage/indicator/uri_query");
    CONSUME_REVMAP_NODES("%s/prestage/namespace/uri_query");

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_smooth_flow_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_smoothflow_active = NULL;
    tstring *c_query = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_smoothflow_active, "%s/smooth_flow/active", name);
    bail_error(err);

    if (ts_equal_str(is_smoothflow_active, "true", false)) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &c_query, "%s/smooth_flow/uri_query", name);
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, "  smooth-flow query-string-parm \"%s\" \n",
                ts_str_maybe_empty(c_query));
        bail_error(err);
    }
    
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/smooth_flow/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/smooth_flow/uri_query", name);
    bail_error(err);

bail:
    return err;
}



/* ------------------------------------------------------------------------- */
int 
cli_vp_full_download_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_fulldownload_active = NULL;
    tstring *t_always = NULL;
    tstring *t_match = NULL;
    tstring *t_query = NULL;
    tstring *t_header = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_fulldownload_active, "%s/full_download/active", name);
    bail_error(err);

    if ( ts_equal_str(is_fulldownload_active, "true", false) ) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &t_always, "%s/full_download/always", name);
        bail_error(err);

        if ( ts_equal_str(t_always, "true", false) ) {
            err = ts_append_sprintf(rev_cmd, 
                    "     full-download always\n");
            bail_error(err);
        } else {
            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_match, "%s/full_download/match", name);
            bail_error(err);

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_query, "%s/full_download/uri_query", name);
            bail_error(err);

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_header, "%s/full_download/uri_hdr", name);
            bail_error(err);

            err = ts_append_sprintf(rev_cmd, "     full-download match \"%s\" ",
                    ts_str_maybe_empty(t_match));
            bail_error(err);

            if ( ts_length(t_query) != 0 ) {
                err = ts_append_sprintf(rev_cmd, " query-string-parm \"%s\"\n",
                        ts_str(t_query));
                bail_error(err);
            } else {
                err = ts_append_sprintf(rev_cmd, " header \"%s\" \n",
                        ts_str_maybe_empty(t_header));
                bail_error(err);
            }
        }
    }


    CONSUME_REVMAP_NODES("%s/full_download/active");
    CONSUME_REVMAP_NODES("%s/full_download/always");
    CONSUME_REVMAP_NODES("%s/full_download/match");
    CONSUME_REVMAP_NODES("%s/full_download/uri_query");
    CONSUME_REVMAP_NODES("%s/full_download/uri_hdr");

bail:
    return err;
}


int 
cli_vp_control_point_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *t_control  = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_control, "%s/control_point", name);
    bail_error(err);

    if ( ts_length(t_control) > 0 ) {
        err = ts_append_sprintf(rev_cmd, 
                "     control-point %s\n",
                ts_str(t_control));
        bail_error(err);
    }



    CONSUME_REVMAP_NODES("%s/control_point");


bail:
    return  err;
}

int 
cli_vp_rate_map_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_active = NULL;
    const char *c_match_name = NULL;
    char *c_name = NULL;
    tstr_array *match_name_parts = NULL;
    uint32 num_matches = 0;
    uint32 idx = 0;
    tstring *t_rate = NULL;
    tstring *t_uol_length = NULL;
    tstring *t_uol_offset = NULL;
    tstring *t_query = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_active, "%s/rate_map/active", name);
    bail_error(err);

    if (ts_equal_str(is_active, "true", false)) {
        c_name = smprintf("%s/rate_map/match/*", name);

	err = bn_binding_array_get_name_part_tstr_array(bindings, c_name, 7, &match_name_parts);
	bail_error(err);

	if (!match_name_parts) 
		goto bail;

	num_matches = tstr_array_length_quick(match_name_parts);
        for (idx = 0; idx < num_matches; idx++) {
            c_match_name = tstr_array_get_str_quick(match_name_parts, idx);
            bail_null(c_match_name);

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_rate, "%s/rate_map/match/%s/rate", 
                    name, c_match_name);
            bail_error(err);

            err = ts_append_sprintf(rev_cmd, "     rate-map match \"%s\" rate \"%s\"",
                    c_match_name, ts_str_maybe_empty(t_rate));
            bail_error(err);

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_query, "%s/rate_map/match/%s/query_string", 
                    name, c_match_name);
            bail_error(err);

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_uol_length, "%s/rate_map/match/%s/uol/length", name,
                    c_match_name);
            bail_error(err);
            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &t_uol_offset, "%s/rate_map/match/%s/uol/offset", name,
                    c_match_name);
            bail_error(err);
#if 0
            if (ts_length(t_query) != 0) {
                err = ts_append_sprintf(rev_cmd, " query-string-parm %s\n",
                        ts_str(t_query));
                bail_error(err);
            } else if ( !ts_equal_str(t_uol_length, "0", false)) {
                err = ts_append_sprintf(rev_cmd, " uol %s %s\n", 
                        ts_str(t_uol_offset), ts_str(t_uol_length));
                bail_error(err);
            } else {
                err = ts_append_sprintf(rev_cmd, "\n");
                bail_error(err);
            }
#endif
	    err = ts_append_sprintf(rev_cmd, "\n");
	    bail_error(err);

#define CONSUME_RATEMAP_NODES(c) \
            {\
                err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name, c_match_name);\
                bail_error(err);\
            }
            CONSUME_RATEMAP_NODES("%s/rate_map/match/%s");
            CONSUME_RATEMAP_NODES("%s/rate_map/match/%s/rate");
            CONSUME_RATEMAP_NODES("%s/rate_map/match/%s/query_string");
            CONSUME_RATEMAP_NODES("%s/rate_map/match/%s/uol/offset");
            CONSUME_RATEMAP_NODES("%s/rate_map/match/%s/uol/length");


#undef CONSUME_RATEMAP_NODES
        }
    }

    CONSUME_REVMAP_NODES("%s/rate_map/active");

bail:
    safe_free(c_name);
    return err;
}

/* ------------------------------------------------------------------------- */
int 
cli_vp_video_id_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *is_active = NULL;
    tstring *t_videoid_query = NULL;
    tstring *t_format_tag = NULL;
    tstring *rev_cmd = (tstring *) data;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_active, "%s/video_id/active", name);
    bail_error(err);

    if (ts_equal_str(is_active, "true", false)) {
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &t_videoid_query, "%s/video_id/uri_query", name);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
                NULL, &t_format_tag, "%s/video_id/format_tag", name);
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, "     cache-name video-id query-string-parm \"%s\" format-tag query-string-parm \"%s\" \n",
                ts_str_maybe_empty(t_videoid_query), ts_str_maybe_empty(t_format_tag));
        bail_error(err);
    }

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/video_id/uri_query", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/video_id/format_tag", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/video_id/active", name);
    bail_error(err);

bail:
    return err;
}

int cli_vp_smoothstream_pub_revmap
(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
	int err = 0;
	tstring *t_fragment_tag = NULL;
	tstring *t_quality_tag = NULL;
	tstring *rev_cmd = (tstring *) data;

	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name_parts);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(vp_name);


        if((ts_equal_str(type, "6", false))) {
	    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		    NULL, &t_quality_tag, "%s/quality-tag", name);
	    bail_error_null(err, t_quality_tag);
	    
	    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &t_fragment_tag, "%s/fragment-tag", name);
	    bail_error_null(err, t_fragment_tag);

	/* show the config in show run only when they are not defaults */
	if (strcmp(ts_str(t_quality_tag), "QualityLevels" )) {
		err = ts_append_sprintf(rev_cmd, 
				"     quality-tag  \"%s\" \n",
				ts_str_maybe_empty(t_quality_tag));
		bail_error(err);
	}
	if (strcmp(ts_str(t_fragment_tag), "Fragments" )) {
		err = ts_append_sprintf(rev_cmd, 
				"     fragment-tag  \"%s\" \n",
				ts_str_maybe_empty(t_fragment_tag));
		bail_error(err);
	}

	}
	CONSUME_REVMAP_NODES("%s/quality-tag");
	CONSUME_REVMAP_NODES("%s/fragment-tag");

bail:
    return err;
}


#if 0
#define	APPEND_VP_CLI_CMD(node_prefix, node_name_fmt, cli_cmd_fmt)	\
	{	\
        node_name = smprintf(node_name_fmt, node_prefix);	\
        bail_null(node_name);	\
	\
        err = bn_binding_array_get_value_tstr_by_name	\
            (bindings, node_name, NULL, &t_node_value);	\
        bail_error_null(err, t_node_value);	\
	\
        err = ts_new_sprintf(&rev_append, cli_cmd_fmt,	\
                             ts_str(t_node_value));	\
        bail_error(err);	\
	\
	err = ts_append(rev_cmd, rev_append);	\
	bail_error(err);	\
        ts_free(&rev_append);	\
        safe_free(node_name);	\
        ts_free(&t_node_value);	\
	}	\

/* ------------------------------------------------------------------------- */
static int 
cli_vp_revmap_type_0(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *af_auto = NULL;
    tstring *af_rate = NULL;
    tstring *af_uri = NULL;
    tstring *af_valid = NULL;
    tstring *is_aflow_active = NULL;
    const char *valid_link = NULL;
    tstr_array *af_name_parts = NULL;
    tstring *rev_cmd = NULL;
    int i = 0;


    // Create the prefix command
    err = ts_new_sprintf(&rev_cmd, "virtual-player %s type 0\n", vp_name);
    bail_error(err);

    for (i = 0; ; i++) {
        if ( revmap_exec[i] == NULL ) {
            break;
	}
        }

        err = (*revmap_exec[i])( (void *) rev_cmd, 
                cmd, bindings, name, name_parts, value, 
                binding, ret_reply, type, vp_name);
        bail_error(err);

    }

#if 0
    err = cli_vp_hash_verify_revmap( (void*) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);

    err = cli_vp_seek_revmap((void*) rev_cmd,
            cmd, bindings,  name, name_parts,
            value, binding, ret_reply, type,
            vp_name);
    bail_error(err);

    err = cli_vp_fast_start_revmap((void*) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);

    err = cli_vp_assured_flow_revmap( (void*) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);

    err = cli_vp_full_download_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);

    err = cli_vp_smooth_flow_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);

    err = cli_vp_connection_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);

    /* Other types that MUST not yield any config */
    err = cli_vp_req_auth_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);
   
    err = cli_vp_health_probe_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);
   
    err = cli_vp_signals_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);
    
    err = cli_vp_prestage_revmap( (void *) rev_cmd,
            cmd, bindings,  name, name_parts, value,
            binding, ret_reply, type, vp_name);
    bail_error(err);
#endif

    err = ts_append_sprintf(rev_cmd, "   exit\n");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

    // Now account for all of the bindings
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);
#if 0
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/always", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/match", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/uri_query", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/uri_hdr", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/max_session_rate", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/smooth_flow/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/smooth_flow/uri_query", name);
    bail_error(err);
 #endif            


bail:
    return err;
}


/* ------------------------------------------------------------------------- */
static int 
cli_vp_revmap_type_2(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    uint32_t idx = 0;
    uint32_t num_matchs = 0;
    char *t_name = NULL;
    tstring *rev_cmd = NULL;
    tstring *t_rate = NULL;
    tstring *t_uol_length = NULL;
    tstring *t_uol_offset = NULL;
    tstring *t_query_string = NULL;
    tstr_array *match_name_parts = NULL;
    int i = 0;


    // Create the prefix command
    err = ts_new_sprintf(&rev_cmd, "virtual-player %s type 2\n", vp_name);
    bail_error(err);
    
    for (i = 0; ; i++) {
        if ( revmap_exec[i] == NULL ) {
            break;
        }

        err = (*revmap_exec[i])( (void *) rev_cmd, 
                cmd, bindings, name, name_parts, value, 
                binding, ret_reply, type, vp_name);
        bail_error(err);

    }
#if 0
    /*

       //// Unused nodes for Type 2 player
       //
       //
     /nkn/nvsd/virtual_player/config/breakplayer/full_download/active = false (bool)
     /nkn/nvsd/virtual_player/config/breakplayer/full_download/always = false (bool)
     /nkn/nvsd/virtual_player/config/breakplayer/full_download/match =  (string)
     /nkn/nvsd/virtual_player/config/breakplayer/full_download/uri_hdr =  (string)
     /nkn/nvsd/virtual_player/config/breakplayer/full_download/uri_query =  (string)
     /nkn/nvsd/virtual_player/config/breakplayer/max_session_rate = 0 (uint32)
     /nkn/nvsd/virtual_player/config/breakplayer/rate_map/active = true (bool)
     /nkn/nvsd/virtual_player/config/breakplayer/smooth_flow/active = false (bool)
     /nkn/nvsd/virtual_player/config/breakplayer/smooth_flow/uri_query =  (string)
     */

    // process hash verify nodes 
    // Process seek nodes
    // Process fast start
    // Process assured flow

    t_name = smprintf("%s/rate_map/match", name);

    err = mdc_get_binding_children_tstr_array(cli_mcc, NULL, NULL,
            &match_name_parts, t_name, NULL);
    bail_error(err);

    if (!match_name_parts) goto bail;

    num_matchs = tstr_array_length_quick(match_name_parts);

    for (idx = 0; idx < num_matchs; idx++)
    {
          err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &t_rate, "%s/rate_map/match/%s/rate", name,
	                tstr_array_get_str_quick(match_name_parts, idx));
          bail_error(err);
          err = ts_append_sprintf(rev_cmd, "     rate-map match %s rate %s",
	                tstr_array_get_str_quick(match_name_parts, idx),
	                ts_str(t_rate));
          bail_error(err);

          err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &t_query_string, "%s/rate_map/match/%s/query_string", name,
	                tstr_array_get_str_quick(match_name_parts, idx));
          bail_error(err);

          err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &t_uol_length, "%s/rate_map/match/%s/uol/length", name,
	                tstr_array_get_str_quick(match_name_parts, idx));
          bail_error(err);
          err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &t_uol_offset, "%s/rate_map/match/%s/uol/offset", name,
	                tstr_array_get_str_quick(match_name_parts, idx));
          bail_error(err);

          if (strcmp(ts_str(t_query_string), "") != 0) {
               err = ts_append_sprintf(rev_cmd, " query-string-parm %s\n",
	                ts_str(t_query_string));
	  }
	  else if (strcmp(ts_str(t_uol_length), "0") != 0) {
               err = ts_append_sprintf(rev_cmd, " uol %s %s\n",
	                ts_str(t_uol_offset), ts_str(t_uol_length));
	  }
	  else err = ts_append_sprintf(rev_cmd, "\n");
          bail_error(err);
    }
#endif

    err = ts_append_sprintf(rev_cmd, "   exit\n");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

    // Now account for all of the bindings
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/type", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/secret/value", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/secret/type", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/data/string", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/data/uri_offset", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/data/uri_len", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/match/uri_query", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/digest", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/hash_verify/active", name);
    bail_error(err);
    
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/seek/uri_query", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/seek/active", name);
    bail_error(err);
            
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/fast_start/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/fast_start/size", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/fast_start/time", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/fast_start/valid", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/fast_start/uri_query", name);
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/assured_flow/valid", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/assured_flow/auto", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/assured_flow/rate", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/assured_flow/uri_query", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/assured_flow/active", name);
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/always", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/match", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/uri_query", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/full_download/uri_hdr", name);
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/max_session_rate", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/smooth_flow/active", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/smooth_flow/uri_query", name);
    bail_error(err);

    for (idx = 0; idx < num_matchs; idx++)
    {
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/match/%s",
	                name, tstr_array_get_str_quick(match_name_parts, idx));
        bail_error(err);
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/match/%s/query_string",
	                name, tstr_array_get_str_quick(match_name_parts, idx));
        bail_error(err);
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/match/%s/rate",
	                name, tstr_array_get_str_quick(match_name_parts, idx));
        bail_error(err);
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/match/%s/uol/length",
	                name, tstr_array_get_str_quick(match_name_parts, idx));
        bail_error(err);
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/rate_map/match/%s/uol/offset",
	                name, tstr_array_get_str_quick(match_name_parts, idx));
        bail_error(err);
    }

bail:
    safe_free(t_name);
    tstr_array_free(&match_name_parts);
    return err;
}

static int 
cli_vp_revmap_type_3(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    int i = 0;
    tstring *rev_cmd = NULL;
    
    // Create the prefix command
    err = ts_new_sprintf(&rev_cmd, "virtual-player %s type 3\n", vp_name);
    bail_error(err);

    for (i = 0; ; i++) {
        if ( revmap_exec[i] == NULL ) {
            break;
        }

        err = (*revmap_exec[i])( (void *) rev_cmd, 
                cmd, bindings, name, name_parts, value, 
                binding, ret_reply, type, vp_name);
        bail_error(err);

    }

    err = ts_append_sprintf(rev_cmd, "   exit\n");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
    return err;
}

static int 
cli_vp_revmap_type_4(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    int i = 0;
    tstring *rev_cmd = NULL;
    
    // Create the prefix command
    err = ts_new_sprintf(&rev_cmd, "virtual-player %s type 4\n", vp_name);
    bail_error(err);

    for (i = 0; ; i++) {
        if ( revmap_exec[i] == NULL ) {
            break;
        }

        err = (*revmap_exec[i])( (void *) rev_cmd, 
                cmd, bindings, name, name_parts, value, 
                binding, ret_reply, type, vp_name);
        bail_error(err);

    }

    err = ts_append_sprintf(rev_cmd, "   exit\n");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
    return err;
}

#endif


/*--------------------------------------------------------------------------
 * All revmap calls fall here.. 
 *  We simply run through individual callbacks and generate the commands.
 *------------------------------------------------------------------------*/
static int 
cli_vp_revmap_type(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    int i = 0;
    tstring *rev_cmd = NULL;
    int iType = atoi(ts_str(type));

    UNUSED_ARGUMENT(data);

    // Create the prefix command
    err = ts_new_sprintf(&rev_cmd, "virtual-player %s type %s\n", vp_name,vp_type_names[iType]);
    bail_error(err);

    for (i = 0; ; i++) {
        if ( revmap_exec[i] == NULL ) {
            break;
        }

        err = (*revmap_exec[i])( (void *) rev_cmd, 
                cmd, bindings, name, name_parts, value, 
                binding, ret_reply, type, vp_name);
        bail_error(err);

    }

    err = ts_append_sprintf(rev_cmd, "   exit\n");
    bail_error(err);

    CONSUME_REVMAP_NODES("%s");
    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
    return err;
}

static int
cli_virtual_player_revmap(void *data, const cli_command *cmd,
               const bn_binding_array *bindings, const char *name,
               const tstr_array *name_parts, const char *value,
               const bn_binding *binding,
               cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *t_vp_name = NULL;
    char *type_name = NULL;
    char *node_name = NULL;
    tstring *t_type = NULL;
    tstring *t_node_value = NULL;
    char *bname_type = NULL;

    // Get the player type
    bname_type = smprintf("%s/type", name);
    bail_null(bname_type);

    // Get the type value
    err = bn_binding_array_get_value_tstr_by_name(
            bindings, bname_type, NULL, &t_type);
    bail_error_null(err, t_type);

    err = tstr_array_get_str (name_parts, 4, &t_vp_name);
    bail_null(t_vp_name);
        
    err = cli_vp_revmap_type(
            data, cmd, bindings, name, name_parts, 
            value, binding, ret_reply,
            t_type, t_vp_name);
    bail_error(err);

#if 0
    if (ts_equal_str(t_type, "1", false)) {
        // Type 1 player
        err = cli_vp_revmap_type_1(
                data, cmd, bindings, name, name_parts, 
                value, binding, ret_reply,
                t_type, t_vp_name);
        bail_error(err);
    }
    else if (ts_equal_str(t_type, "0", false)) {
        // Type 0 player
        err = cli_vp_revmap_type(
                data, cmd, bindings, name, name_parts, 
                value, binding, ret_reply,
                t_type, t_vp_name);
        bail_error(err);
    }
    else if (ts_equal_str(t_type, "2", false)) {
        // Type 2 player
        err = cli_vp_revmap_type_2(
                data, cmd, bindings, name, name_parts, 
                value, binding, ret_reply,
                t_type, t_vp_name);
        bail_error(err);
    }
    else if (ts_equal_str(t_type, "3", false)) {
        // Type 3 player
        err = cli_vp_revmap_type(
                data, cmd, bindings, name, name_parts, 
                value, binding, ret_reply,
                t_type, t_vp_name);
        bail_error(err);
    }
    else if (ts_equal_str(t_type, "4", false)) {
        // Type 4 player
        err = cli_vp_revmap_type_4(
                data, cmd, bindings, name, name_parts, 
                value, binding, ret_reply,
                t_type, t_vp_name);
        bail_error(err);
    }




    type_name = smprintf("%s/type", name);
    bail_null(type_name);


    err = ts_new_sprintf(&rev_cmd, 
            "\n"
            "##\n"
            "## Virtual-Player configuration\n"
            "##\n");
    bail_error(err);


    if (!strcmp (ts_str(t_type), "0"))
    {
        APPEND_VP_CLI_CMD(name, "%s/max_session_rate", "\tconnection max-bandwidth %s\n");
    }

    if ( ts_equal_str(t_type, "1", false) ) {
    }

    err = ts_append_str(rev_cmd, "   exit");
    bail_error(err);

    err = tstr_array_append_takeover(ret_reply->crr_cmds, &rev_cmd);
    bail_error(err);

    /* Now account for all of the bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

    num_nodes = sizeof(vp_nodes_last_part_list)/sizeof(char*);
    for (i = 0; i < num_nodes; i++)
    {
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/%s", name, vp_nodes_last_part_list[i]);
        bail_error(err);
    }
#endif

 bail:
    safe_free(type_name);
    safe_free(node_name);
    ts_free(&rev_cmd);
    ts_free(&t_type);
    ts_free(&t_node_value);
    return(err);
}
#if 1
static int
cli_virtual_player_check_and_create(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        tbool *valid)
{
    int err = 0, ret_err = 0;
    //tbool valid = false;
    char *bn_name = NULL;
    char *bn_vp_type = NULL;
    const char *type = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    bail_null(valid);

    *valid = false;

    err = cli_nvsd_virtual_player_validate(
            tstr_array_get_str_quick(params, 0),
            tstr_array_get_str_quick(cmd_line, 3),
            valid, &ret_err);
    
    if (!(*valid) && !ret_err) {
        // Node doesnt exist.. create it

        type = "0"; //tstr_array_get_str_quick(cmd_line, 3);
        bail_null(type);

        bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s",
                tstr_array_get_str_quick(params, 0));
        bail_null(bn_name);

        err = mdc_create_binding(cli_mcc, 
                NULL, NULL,
                bt_string,
                tstr_array_get_str_quick(params, 0),
                bn_name);
        bail_error(err);

        bn_vp_type = smprintf("/nkn/nvsd/virtual_player/config/%s/type", 
                tstr_array_get_str_quick(params, 0));
        bail_null(bn_vp_type);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
                type, bn_vp_type);
        bail_error(err);

        *valid = true;
    }

bail:
    return err;
}
#endif


/*
 * Check that type is being set from default to specialized type.
 * Never allow setting of type from specialized to default.
 */
int
cli_nvsd_vp_set_type(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    const char *c_type = NULL;
    char *bn_vp_type = NULL;
    tstring *ret_prefixes = NULL;
    tstring *ret_value = NULL;
    tbool type_3_licensed = false;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    bn_type ret_type = bt_NONE;
    uint32 type = (uint32) (-1);
    const char *c_vptype = NULL;

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    c_type = tstr_array_get_str_quick(params, 1);
    bail_null(c_type);

    bn_vp_type = smprintf("/nkn/nvsd/virtual_player/config/%s/type", c_player);
    bail_null(bn_vp_type);

    /* check whether type is within valid id's
     */
    //type = atoi(c_type);
    if( (strcmp( c_type, "generic")) == 0) {
	    type = 0;
    }
#if 0
    else if ( (strcmp( c_type, "break")) == 0) {
	    type = 1;
    }
    else if ( (strcmp( c_type, "qss-streamlet")) == 0) {
	    type = 2;
    }
#endif
    else if ( (strcmp( c_type, "yahoo")) == 0) {
	    type = 3;
    }
#if 0
    else if ( (strcmp( c_type, "smoothflow")) == 0) {
	    type = 4;
    }
#endif
    else if ( (strcmp( c_type, "youtube")) == 0) {
	    type = 5;
    } else if ( (strcmp( c_type, "smoothstream-pub")) == 0) {
	    type = 6;
    } else if ( (strcmp( c_type, "flashstream-pub")) == 0) {
	    type = 7;
    }
    c_vptype = smprintf("%u", type); 
    if (type > 7) {
        cli_printf_error(_("Invalid type assignment for virtual-player. "
                    "Valid type are generic," 
		    "yahoo, youtube , flashstream-pub,"
		    "and smoothstream-pub")); //Removed smoothflow
        goto bail;
    }
    else if (type == 5) // YouTube Player
    {
        cli_printf(_("Note: Please set cache name parameters video-id and format-type for this virtual-player"));
    }
    else if (type == 3) // Yahoo Player
    {
    	/* This is allowed only if valid license exists */
    	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
            &type_3_licensed, "/nkn/mfd/licensing/config/vp_type03_licensed", NULL);
    	bail_error(err);

	if (!type_3_licensed)
	{
            cli_printf_error(_("Type Yahoo virtual-player is a licensed feature\n"
	    		"  Failed to set type to Yahoo as license does not exist"));
            goto bail;
	}
    }
    
    /* check whether this player already exists.. If so, bail with error
     */
    err = mdc_get_binding_tstr_fmt(cli_mcc, &ret_err, &ret_msg, &ret_type,
            &ret_value, NULL, "/nkn/nvsd/virtual_player/config/%s/type", c_player);
    bail_error(err);

    if ( ret_value && (strcmp(c_vptype, ts_str(ret_value)) != 0) ) {
        cli_printf_error(_("Bad command. Trying to re-assign player type "
                    "for player '%s'"), c_player);
        goto bail;
    }


    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uint32,
            c_vptype, bn_vp_type);
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va(cli_mcc, NULL, NULL,
            "/nkn/nvsd/virtual_player/actions/set_default", 2, 
            "name", bt_string, c_player,
            "type", bt_uint32, c_vptype);
    bail_error(err);

    err = cli_get_prefix_summary(false, &ret_prefixes);
    bail_error(err);
    if ( ret_prefixes == NULL ) {
        err = cli_nvsd_vp_enter_prefix_mode(data, cmd, cmd_line, params);
        bail_error(err);

        err = cli_prefix_pop();
        bail_error(err);

        err = cli_prefix_push("virtual-player", true, false);
        bail_error(err);

        err = cli_prefix_push(c_player, false, true);
        bail_error(err);

        err = clish_prompt_update(false);
        bail_error(err);
    }

bail:
    safe_free(bn_vp_type);
    return err;
}


/*
 * Get list of virtual players that have been configured in the
 * database
 */
static int
cli_nvsd_vp_get_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *t_names = NULL;
    tstr_array_options opts;
    tstr_array *t_names_config = NULL;
    uint32 ret_err = 0;
    tstring *t_ret_msg = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&t_names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            &ret_err, &t_ret_msg, 
            &t_names_config,
            "/nkn/nvsd/virtual_player/config", NULL);
    bail_error_null(err, t_names_config);

    err = tstr_array_concat(t_names, &t_names_config);
    bail_error(err);

    *ret_names = t_names;
    t_names = NULL;

bail:
    ts_free(&t_ret_msg);
    tstr_array_free(&t_names);
    tstr_array_free(&t_names_config);
    return err;
}


/*
 * This function does 3 things.
 *  1. If the player is a new player, then do nothing.
 *  2. If the player exists, validate the type is set - or throw error
 *  3. If the player exists & type is set, check if the command is allowed.
 */
static int
cli_nvsd_vp_is_cmd_allowed(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params, 
        tbool *ret_allowed)
{
    int err = 0;
    const char *c_player = NULL;
    uint32 type = (uint32)(-1);
    bn_binding *binding = NULL;
    tbool is_valid = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    bail_null(ret_allowed);
    *ret_allowed = false;

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    /* Stage 1 - Check if this player exists
     */
    err = cli_nvsd_vp_validate(c_player, &is_valid, NULL);
    bail_error(err);

    if (is_valid) {
        /* Stage 2 - Player exists.. Check if the type is set.
         */
        err = cli_nvsd_vp_type_to_mask(c_player, &type);
        bail_error(err);

        if (type == vp_type_none) {
            /* Type was NEVER set for this player. BAIL !!!
             */
            cli_printf_error(_("This player doesnt have a type associated with it."
                        " Set a type before configuring the player."));
            *ret_allowed = false;
            goto bail;
        }

        /* Stage 3 - Check if this command word is allowed in this 
         * player type or not.
         */
        if ( cmd->cc_magic & type ) {
            *ret_allowed = true;
            goto bail;
        }
        /* if we hit this line, then we scanned through our cmd list
         * and didnt see the correct mask
         */
        cli_printf_error(_("This command cannot be used for a virtual-player of this type."));
    }


bail:
    bn_binding_free(&binding);
    return err;
}


/*
 * Check whether the player already exists or not.
 * Only validates the player name.
 */
static int 
cli_nvsd_vp_validate(
        const char *name,
        tbool *ret_valid,
        int *ret_error)
{
    int err = 0;
    tstr_array *t_names = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_nvsd_vp_get_names(&t_names, false);
    bail_error(err);

    err = tstr_array_linear_search_str(t_names, name, 0, &idx);
    if (err != lc_err_not_found) {
        *ret_valid = true;
    }

    if (NULL != ret_error)
        *ret_error = err;

    err = 0;
bail:
    tstr_array_free(&t_names);
    return err;
}


/*
 * Helper function to get a player's type field 
 * and resolve it into an enum.
 */
static int
cli_nvsd_vp_type_to_mask(
        const char *player,
        uint32  *ret_mask)
{
    int err = 0;
    uint32 type = 0;
    bn_binding *binding = NULL;

    bail_null(player);

    bail_null(ret_mask);
    *ret_mask = 0;

    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding, NULL, 
            "/nkn/nvsd/virtual_player/config/%s/type", player);
    bail_error(err);

    err = bn_binding_get_uint32(binding, ba_value, NULL, &type);
    bail_error(err);

    if (type == (uint32) (-1))
        *ret_mask = vp_type_none;
    else
        *ret_mask = (1 << type);
    
bail:
    bn_binding_free(&binding);
    return err;
}

static int
cli_nvsd_vp_show_type_3(
        const char *name,
        const bn_binding_array *bindings)
{
    int err = 0;
    char *r = NULL;
    char *afr = NULL;
    char *seek = NULL;
    char *h = NULL;
    char *ra = NULL;

    UNUSED_ARGUMENT(bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
    afr = smprintf("%s/assured_flow", r);
    bail_null(afr);
    seek = smprintf("%s/seek", r);
    bail_null(seek);
    h = smprintf("%s/health_probe", r);
    bail_null(h);
    ra = smprintf("%s/req_auth", r);
    bail_null(ra);
   
    cli_printf(_("\n"));
    cli_printf_query(_(
                "Request Auth Configuration:\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), ra);
    cli_printf_query(_(
                "  Digest Type: #%s/digest#\n"), ra);
    cli_printf_query(_(
                "  Stream-Id Query String: #%s/stream_id/uri_query#\n"), ra);
    cli_printf_query(_(
                "  Auth-Id Query String: #%s/auth_id/uri_query#\n"), ra);
    cli_printf_query(_(
                "  Shared-secret String: #%s/secret/value#\n"), ra);
    cli_printf_query(_(
                "  Time interval: #%s/time_interval# second(s)\n"), ra);
    cli_printf_query(_(
                "  Match Query String: #%s/match/uri_query#\n"), ra);

    cli_printf(_("\n"));

    cli_printf_query(_(
                "Health Probe Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), h);
    cli_printf_query(_(
                "  Query string: #%s/uri_query#\n"), h);
    cli_printf_query(_(
                "  Match string: #%s/match#\n"), h);
    

    cli_printf(_("\n"));
    // Display seek config
    //
    cli_printf(_(
                "Seek Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), seek);
    cli_printf_query(_(
                "  URI Query: #%s/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek Length URI Query : #%s/length/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek FLV type : #%s/flv-type#\n"), seek);
    cli_nvsd_print_use_9byte_hdr(name);
    cli_printf_query(_(
                "  Seek MP4 type : #%s/mp4-type#\n"), seek);
    cli_nvsd_print_seek_enable_tunnel(name);

    cli_printf(_("\n"));
    // Display AFR config
    cli_nvsd_print_rate_control_config (name, afr);

    cli_printf(_("\n"));
    // Display Connection parameters
    //
    cli_printf(_(
                "Connection Configuration\n"));
    cli_printf_query(_(
                "  Max Bandwidth: #%s/max_session_rate# Kbps\n"), r);

    cli_printf(_("\n"));

bail:
    safe_free(r);
    safe_free(afr);
    safe_free(seek);
    safe_free(h);
    safe_free(ra);
    return err;
}

static int
cli_nvsd_vp_show_type_4(
        const char *name,
        const bn_binding_array *bindings)
{
    int err = 0;
    char *r = NULL;
    char *h = NULL;
    char *seek = NULL;
    char *p = NULL;
    bn_binding *binding = NULL;
    char *secret_value = NULL;
    char *seek_query = NULL;

    UNUSED_ARGUMENT(bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
    h = smprintf("%s/hash_verify", r);
    bail_null(h);
    seek = smprintf("%s/seek", r);
    bail_null(seek);
    p = smprintf("%s/prestage", r);
    bail_null(p);


    cli_printf(_("\n"));
    cli_printf_query(_(
                "Control Point : #%s/control_point#\n"), r);
    
    
    cli_printf(_("\n"));
    cli_printf(_(
                "Signals Configuration:\n"));
    cli_printf_query(_(
                "  Enabled: #%s/signals/active#\n"), r);
    cli_printf_query(_(
                "  Session-Id string: #%s/signals/session_id/uri_query#\n"), r);
    cli_printf_query(_(
                "  State string: #%s/signals/state/uri_query#\n"), r);
    cli_printf_query(_(
                "  Profile string: #%s/signals/profile/uri_query#\n"), r);
#if 0
    cli_printf_query(_(
                "  Chunk string: #%s/signals/chunk/uri_query#\n"), r);
    cli_printf(_("\n"));
    cli_printf(_(
                "Prestage Configuration:\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), p);
    cli_printf_query(_(
                "  Indicator string: #%s/indicator/uri_query#\n"), p);
    cli_printf_query(_(
                "  Namespace string: #%s/namespace/uri_query#\n"), p);
#endif            
    // Display hash-verify config
    cli_printf(_("\n"));
    cli_printf(_(
                "Hash Verify Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), h); 
    cli_printf_query(_(
                "  Digest: #%s/digest#\n"), h); 
#if 0
    cli_printf_query(_(
                "  Data String: #%s/data/string#\n"), h);
    cli_printf_query(_(
                "  Data UOL Offset: #%s/data/uri_offset#\n"), h);
    cli_printf_query(_(
                "  Data UOL Length: #%s/data/uri_len#\n"), h);
#endif
    cli_printf_query(_(
                "  Match query string: #%s/match/uri_query#\n"), h);

    /* Query the DB for the shared secret .. if empty then print nothing
     */
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding, NULL, 
            "%s/secret/value", h);
    bail_error(err);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &secret_value);
    bail_error(err);
    if (strlen(secret_value) > 0) {
        cli_printf_query(_(
                    "  Shared secret: **** (#%s/secret/type#)\n"), h );
    } else {
        cli_printf(_(
                    "  Shared secret: \n") );
    }
    cli_printf_query(_(
                "  Expiry time : #%s/expiry_time/epochtime# (POSIX timestamp format)\n"), h);
    cli_printf_query(_(
                "  URL format : #%s/url_type# \n"), h);
    cli_printf(_("\n"));
    // Display Connection parameters
    //
    cli_printf(_(
                "Connection Configuration\n"));
    cli_printf_query(_(
                "  Max Bandwidth: #%s/max_session_rate# Kbps\n"), r);

    cli_printf(_("\n"));
    // Display seek config
    //
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding, NULL,
		    "%s/uri_query", seek);
    bail_error(err);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &seek_query);
    bail_error(err);
    if( strlen(seek_query) > 0) {
    cli_printf(_(
                "Seek Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), seek);
    cli_printf_query(_(
                "  URI Query: #%s/uri_query#\n"), seek);

    cli_printf_query(_(
                "  Seek Length URI Query : #%s/length/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek FLV type : #%s/flv-type#\n"), seek);
    cli_nvsd_print_use_9byte_hdr(name);
    cli_printf_query(_(
                "  Seek MP4 type : #%s/mp4-type#\n"), seek);

    cli_nvsd_print_seek_enable_tunnel(name);
    }


bail:
    safe_free(r);
    safe_free(seek);
    safe_free(h);
    safe_free(p);
    bn_binding_free(&binding);
    safe_free(seek_query);
    return err;
}

static int
cli_nvsd_vp_show_type_5(
        const char *name,
        const bn_binding_array *bindings)
{
    int err = 0;
    char *r = NULL;
    char *seek = NULL;
    char *afr = NULL;
    char *fast = NULL;

    UNUSED_ARGUMENT(bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
    fast = smprintf("%s/fast_start", r);
    bail_null(fast);
    afr = smprintf("%s/assured_flow", r);
    bail_null(afr);
    seek = smprintf("%s/seek", r);
    bail_null(seek);

    cli_printf(_("\n"));
    // Display video-id config
    //
    cli_printf(_(
                "Cache Name Configuration\n"));
    //cli_printf_query(_(
                //"  Enabled: #%s/video_id/active#\n"), r);
    cli_printf_query(_(
                "  Video-Id : #%s/video_id/uri_query#\n"), r);
    cli_printf_query(_(
                "  Format Tag : #%s/video_id/format_tag#\n"), r);
    
    cli_printf(_("\n"));
    // Display seek config
    cli_printf(_(
                "Seek Configuration\n"));
    cli_printf_query(_(
                "  Enabled: #%s/active#\n"), seek);
    cli_printf_query(_(
                "  URI Query: #%s/uri_query#\n"), seek);
    cli_printf_query(_(
                "  Seek Length URI Query : #%s/length/uri_query#\n"), seek);
   
    cli_printf_query(_(
                "  Seek FLV type : #%s/flv-type#\n"), seek);
    cli_nvsd_print_use_9byte_hdr(name);
    cli_printf_query(_(
                "  Seek MP4 type : #%s/mp4-type#\n"), seek);
    cli_nvsd_print_seek_enable_tunnel(name);

    cli_printf(_("\n"));
    // Display AFR config
    cli_nvsd_print_rate_control_config (name, afr);

    // Display Fast Start config
    //
    cli_nvsd_print_fast_start_config (name, fast);

    cli_printf(_("\n"));
    // Display Connection parameters
    //
    cli_printf(_(
                "Connection Configuration\n"));
    cli_printf_query(_(
                "  Max Bandwidth: #%s/max_session_rate# Kbps\n"), r);

    cli_nvsd_print_bw_opt_config(name);

    cli_printf(_("\n"));
bail:
    safe_free(r);
    safe_free(afr);
    safe_free(fast);
    safe_free(seek);
    return err;
}
static int
cli_nvsd_vp_show_type_6(
        const char *name,
	const bn_binding_array *bindings)
{
    int err = 0;
    char *r = NULL;
 
    UNUSED_ARGUMENT(bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
   cli_printf(_("\n"));

   // Display smooth-stream config
   //
   cli_printf(_(
                "smoothstream-pub Configuration\n"));
 
    cli_printf_query(_(
                "  Quality-Tag : #%s/quality-tag#\n"), r);
    cli_printf_query(_(
                "  Fragment-Tag : #%s/fragment-tag#\n"), r);
    
    cli_printf(_("\n"));
bail:
    safe_free(r);
    return err;
}

int 
 cli_nvsd_vp_seek_length_config(
         void *data,
         const cli_command *cmd,
         const tstr_array *cmd_line,
         const tstr_array *params)
 {
     int err = 0;
     const char *c_player = NULL;
     const char * c_seek_length = NULL;
     const char * c_query = NULL;
     tbool is_allowed = false;
     uint32 ret_err = 0;
     tstring *ret_msg = NULL;
     tstring *seek_query = NULL;
     char *seek_len_query = NULL;
     char *bn_name = NULL;
 
     err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
     bail_error(err);
 
     if (!is_allowed) {
         goto bail;
     }  
     	
   	c_player = tstr_array_get_str_quick(params, 0);
        bail_null(c_player);

	c_query =  tstr_array_get_str_quick(params, 1); //, idx + 2);
   	bail_null(c_query);

	c_seek_length =  tstr_array_get_str_quick(params, 2); //, idx + 2);
   	bail_null(c_seek_length);

    	bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",
			c_player);
	bail_null(bn_name);

	/*
	 * Check whether seek query string is set 
	 * If set, ignore the param value
	 * else set the param value and then set the seek-length.
	 */
    	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		&seek_query, bn_name, NULL );
	bail_error(err);

	if(seek_query && ts_length(seek_query) > 0){
		if(!(ts_equal_str(seek_query, c_query, false))) {
			cli_printf(_("ERROR:seek uri query string is already set to (%s)\n"
						"Reset the value using \"no seek-config query-string-parm *\" \n"),
					ts_str(seek_query));
			goto bail;
		}
	}

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, bn_name);
	bail_error(err);

	seek_len_query = smprintf(
		     "/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",
		     c_player);
	bail_null(seek_len_query);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
		c_seek_length, seek_len_query);
     if ( err == 0 ) {
         ts_free(&ret_msg);
         safe_free(bn_name);
         bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/active",
                 c_player);
         bail_null(bn_name);
 
         err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
                 "true", bn_name);
         bail_error(err);
     }
     
     if (ret_err != 0) {
         cli_printf_error(_("%s"), ts_str(ret_msg));
     }
bail:
    safe_free(seek_len_query);
    safe_free(bn_name);
    ts_free(&ret_msg);
    ts_free(&seek_query);
    return err;
}
int 
cli_nvsd_vp_seek_tunnel_config(
          void *data,
          const cli_command *cmd,
          const tstr_array *cmd_line,
          const tstr_array *params)
{
	int err = 0;
	const char *c_player = NULL;
	const char * c_tunnel = NULL;
	const char * c_query = NULL;
	tbool is_allowed = false;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	tstring *seek_query = NULL;
	char *seek_tunnel = NULL;
        char *bn_name = NULL;

	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}  
  
    	c_player = tstr_array_get_str_quick(params, 0);
        bail_null(c_player);

    	c_query = tstr_array_get_str_quick(params, 1);
        bail_null(c_query);

    	bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",
			c_player);
	bail_null(bn_name);

	/*
	 * Check whether seek query string is set 
	 * If set, ignore the param value
	 * else set the param value and then set the seek-length.
	 */
    	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		&seek_query, bn_name, NULL );
	bail_error(err);

	if(seek_query && ts_length(seek_query) > 0){
		if(!(ts_equal_str(seek_query, c_query, false))) {
			cli_printf(_("ERROR:seek uri query string is already set to (%s)\n"
						"Reset the value using \"no seek-config query-string-parm *\" \n"),
					ts_str(seek_query));
			goto bail;
		}
	}

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, bn_name);
	bail_error(err);

  	seek_tunnel = smprintf(
			"/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
			c_player);
  	bail_null(seek_tunnel);
	c_tunnel = cmd->cc_exec_value;

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
			c_tunnel, seek_tunnel);
	bail_error(err);

	if ( err == 0 ) {
		ts_free(&ret_msg);
		safe_free(bn_name);
		bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/active",
				c_player);
		bail_null(bn_name);
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
				"true", bn_name);
		bail_error(err);
	}
	if (ret_err != 0) {
		cli_printf_error(_("%s"), ts_str(ret_msg));
	}

bail:
	safe_free(seek_tunnel);
	safe_free(bn_name);
	ts_free(&ret_msg);
        ts_free(&seek_query);
	return err;
}

int 
cli_nvsd_vp_seek_flv_type_config(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
       	const char *c_player = NULL;
       	const char * c_seek_flv_type = NULL;
       	tbool is_allowed = false;
       	uint32 ret_err = 0;
       	tstring *ret_msg = NULL;
       	char *seek_flv_node = NULL;
	const char * c_query = NULL;
       	tstring *seek_query = NULL;
       	char *bn_name = NULL;
	const char *t_use_9byte_hdr = NULL;
	char t_node_name[MAX_NODE_LENGTH];

	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

        if (!is_allowed) {
		goto bail;
        }  
   
     	c_player = tstr_array_get_str_quick(params, 0);
        bail_null(c_player);

	c_query = tstr_array_get_str_quick(params, 1);
        bail_null(c_query);
   
	/* 
	 * The param 1 is for seek_query
	 */

    	bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",
			c_player);
	bail_null(bn_name);

	/*
	 * Check whether seek query string is set 
	 * If set, ignore the param value
	 * else set the param value and then set the seek-length.
	 */
    	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		&seek_query, bn_name, NULL );
	bail_error(err);

	if(seek_query && ts_length(seek_query) > 0){
		if(!(ts_equal_str(seek_query, c_query, false))) {
			cli_printf(_("ERROR:seek uri query string is already set to (%s)\n"
						"Reset the value using \"no seek-config query-string-parm *\" \n"),
					ts_str(seek_query));
			goto bail;
		}
	}

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, bn_name);
	bail_error(err);

  	c_seek_flv_type =  cmd->cc_exec_value; 
     	bail_null(c_seek_flv_type);

  
  	seek_flv_node = smprintf(
  		     "/nkn/nvsd/virtual_player/config/%s/seek/flv-type",
  		     c_player);
  	bail_null(seek_flv_node);
  
  	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
  		c_seek_flv_type, seek_flv_node);
       if ( err == 0 ) {
           ts_free(&ret_msg);
           safe_free(bn_name);
           bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/active",
                   c_player);
           bail_null(bn_name);
   
           err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
                   "true", bn_name);
           bail_error(err);
       }
       /*use-9byte-hdr*/
	t_use_9byte_hdr = cmd->cc_exec_value2;
	if(t_use_9byte_hdr) {
	    snprintf(t_node_name, sizeof(t_node_name), "/nkn/nvsd/virtual_player/config/%s/seek/use_9byte_hdr",
		    c_player);
	   err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
		   t_use_9byte_hdr, t_node_name);
	}
       
       if (ret_err != 0) {
           cli_printf_error(_("%s"), ts_str(ret_msg));
       }
  bail:
      safe_free(seek_flv_node);
      safe_free(bn_name);
      ts_free(&ret_msg);
      ts_free(&seek_query);
      return err;
}

int 
cli_nvsd_vp_seek_mp4_config(
		void *data,
   		const cli_command *cmd,
   		const tstr_array *cmd_line,
   		const tstr_array *params)
{
	int err = 0;
        const char *c_player = NULL;
        const char *c_query = NULL;
        const char * c_seek_mp4_type = NULL;
        tbool is_allowed = false;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	tstring *seek_query = NULL;
	char *seek_mp4_node = NULL;
	char *bn_name = NULL;

   	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
   	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}

	c_player = tstr_array_get_str_quick(params, 0);
	bail_null(c_player);

	c_query = tstr_array_get_str_quick(params, 1);
        bail_null(c_query);

    	bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",
			c_player);
	bail_null(bn_name);

	/*
	 * Check whether seek query string is set 
	 * If set, ignore the param value
	 * else set the param value and then set the seek-length.
	 */
    	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		&seek_query, bn_name, NULL );
	bail_error(err);

	if(seek_query && ts_length(seek_query) > 0){
		if(!(ts_equal_str(seek_query, c_query, false))) {
			cli_printf(_("ERROR:seek uri query string is already set to (%s)\n"
						"Reset the value using \"no seek-config query-string-parm *\" \n"),
					ts_str(seek_query));
			goto bail;
		}
	}

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, bn_name);
	bail_error(err);
      
     	c_seek_mp4_type =  tstr_array_get_str_quick(params, 2); //, idx + 2);
       	bail_null(c_seek_mp4_type);
     
     	seek_mp4_node = smprintf(
			"/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",
			c_player);
     	bail_null(seek_mp4_node);
     
     	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_seek_mp4_type, seek_mp4_node);
	if ( err == 0 ) {
		ts_free(&ret_msg);
		safe_free(bn_name);
		bn_name = smprintf("/nkn/nvsd/virtual_player/config/%s/seek/active",
				c_player);
		bail_null(bn_name);
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_bool,
				"true", bn_name);
		bail_error(err);
	}
	if (ret_err != 0) {
		cli_printf_error(_("%s"), ts_str(ret_msg));
	}
bail:
	safe_free(seek_mp4_node);
        safe_free(bn_name);
        ts_free(&ret_msg);
        ts_free(&seek_query);
        return err;
}

static int
cli_nvsd_vp_show_type_7(
        const char *name,
	const bn_binding_array *bindings)
{
    int err = 0;
    char *r = NULL;

    UNUSED_ARGUMENT(bindings);

    r = smprintf("/nkn/nvsd/virtual_player/config/%s", name);
    bail_null(r);
   cli_printf(_("\n"));

   // Display flash-stream config
   cli_printf(_(
                "Flashstream-pub Configuration\n"));

    cli_printf_query(_(
                "  Fragment-Tag : #%s/flash-fragment-tag#\n"), r);
    cli_printf_query(_(
                "  Segment-Tag : #%s/segment-tag#\n"), r);
    cli_printf_query(_(
                "  Segment-Fragment-Delimiter : #%s/segment-delimiter-tag#\n"), r);

    cli_printf(_("\n"));


bail:
    return err;
}

int
cli_nvsd_fragment_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_query = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	char *bn_active = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	uint32 type = 0;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}
	
	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);

#if 0
	/* get the command */
	const char *c_query_tag = NULL;
	c_query_tag = tstr_array_get_str_quick(cmd_line, 2);
	bail_null(c_query_tag);
	lc_log_basic(LOG_DEBUG, "c_query_tag is %s", c_query_tag);
#endif
	err = cli_nvsd_vp_type_to_mask(c_vp_name, &type);
	bail_error(err);

	/* get the query-param */
	c_query = tstr_array_get_str_quick(params, 1);
	bail_null(c_query);


	lc_log_basic(LOG_DEBUG, "c_query is %s", c_query);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	if( type  == vp_type_6) {
	    node = smprintf("/nkn/nvsd/virtual_player/config/%s/fragment-tag",c_vp_name);
	    bail_null(node);
	} else {
	    node = smprintf("/nkn/nvsd/virtual_player/config/%s/flash-fragment-tag",c_vp_name);
	    bail_null(node);
	}

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	safe_free(bn_active);
	ts_free(&ret_msg);
	return err;
}

int 
cli_nvsd_fragment_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_query = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	uint32 type = 0;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}

	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);
#if 0
	/* get the command */
	c_query_tag = tstr_array_get_str_quick(cmd_line, 3);
	bail_null(c_query_tag);
	lc_log_basic(LOG_DEBUG, "c_query_tag is %s", c_query_tag);
#endif
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	err = cli_nvsd_vp_type_to_mask(c_vp_name, &type);
	bail_error(err);

	if(type == vp_type_6){
	    c_query = "Fragments";
	    node = smprintf("/nkn/nvsd/virtual_player/config/%s/fragment-tag",c_vp_name);
	    bail_null(node);
	}else if (type == vp_type_7){
	    c_query = "";
	    node = smprintf("/nkn/nvsd/virtual_player/config/%s/flash-fragment-tag",c_vp_name);
	    bail_null(node);
	}else {
	    goto bail;
	}



	lc_log_basic(LOG_DEBUG, "c_query is %s", c_query);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_query, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	ts_free(&ret_msg);
	return err;

}

int
cli_nvsd_segment_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_segment = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	char *bn_active = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}
	
	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);

#if 0
	/* get the command */
	const char *c_query_tag = NULL;
	c_query_tag = tstr_array_get_str_quick(cmd_line, 2);
	bail_null(c_query_tag);
	lc_log_basic(LOG_DEBUG, "c_query_tag is %s", c_query_tag);
#endif

	/* get the query-param */
	c_segment = tstr_array_get_str_quick(params, 1);
	bail_null(c_segment);


	lc_log_basic(LOG_DEBUG, "c_segment is %s", c_segment);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	node = smprintf("/nkn/nvsd/virtual_player/config/%s/segment-tag",c_vp_name);
	bail_null(node);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_segment, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	safe_free(bn_active);
	ts_free(&ret_msg);
	return err;
}

int 
cli_nvsd_segment_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_segment = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}

	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);
#if 0
	/* get the command */
	c_query_tag = tstr_array_get_str_quick(cmd_line, 3);
	bail_null(c_query_tag);
#endif
	c_segment = "Seg";
	lc_log_basic(LOG_DEBUG, "c_segment is %s", c_segment);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	node = smprintf("/nkn/nvsd/virtual_player/config/%s/segment-tag",c_vp_name);
	bail_null(node);


	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_segment, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	ts_free(&ret_msg);
	return err;

}

int
cli_nvsd_seg_frag_delimiter_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_segment = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	char *bn_active = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}
	
	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);

#if 0
	/* get the command */
	const char *c_query_tag = NULL;
	c_query_tag = tstr_array_get_str_quick(cmd_line, 2);
	bail_null(c_query_tag);
	lc_log_basic(LOG_DEBUG, "c_query_tag is %s", c_query_tag);
#endif

	/* get the query-param */
	c_segment = tstr_array_get_str_quick(params, 1);
	bail_null(c_segment);


	lc_log_basic(LOG_DEBUG, "c_seg_frag_delimiter is %s", c_segment);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );

	node = smprintf("/nkn/nvsd/virtual_player/config/%s/segment-delimiter-tag",c_vp_name);
	bail_null(node);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_segment, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	safe_free(bn_active);
	ts_free(&ret_msg);
	return err;
}

int 
cli_nvsd_seg_frag_delimiter_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *c_segment = NULL;
	const char *c_vp_name = NULL;
	tbool is_allowed = false;
	char *node = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	/* Check if command is allowed */
	err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
	bail_error(err);

	if (!is_allowed) {
		goto bail;
	}

	/* get the VP name */
	c_vp_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_vp_name);
#if 0
	/* get the command */
	c_query_tag = tstr_array_get_str_quick(cmd_line, 3);
	bail_null(c_query_tag);
#endif
	c_segment = "-";

	lc_log_basic(LOG_DEBUG, "c_segment is %s", c_segment);
	lc_log_basic(LOG_DEBUG, "c_vp_name is %s",c_vp_name );


	node = smprintf("/nkn/nvsd/virtual_player/config/%s/segment-delimiter-tag", c_vp_name);
	bail_null(node);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
			c_segment, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));

bail:
	safe_free(node);
	ts_free(&ret_msg);
	return err;

}

int cli_vp_flashstream_pub_revmap( void *data,
	         const cli_command *cmd, const bn_binding_array *bindings,
		 const char *name, const tstr_array *name_parts,
		 const char *value, const bn_binding *binding,
		 cli_revmap_reply *ret_reply, const tstring *type,
		 const char *vp_name)
{
     int err = 0;
     tstring *t_fragment_tag = NULL;
     tstring *t_segment_tag = NULL;
     tstring *t_seg_frag_delimiter = NULL;
     tstring *rev_cmd = (tstring *) data;
 
     UNUSED_ARGUMENT(cmd);
     UNUSED_ARGUMENT(binding);
     UNUSED_ARGUMENT(name_parts);
     UNUSED_ARGUMENT(value);
     UNUSED_ARGUMENT(vp_name);

     if((ts_equal_str(type, "7", false))) {
     err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	     NULL, &t_segment_tag, "%s/segment-tag", name);
     bail_error_null(err, t_segment_tag);

     err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	     NULL, &t_fragment_tag, "%s/flash-fragment-tag", name);
     bail_error_null(err, t_fragment_tag);

     err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	     NULL, &t_seg_frag_delimiter, "%s/segment-delimiter-tag", name);
     bail_error_null(err, t_seg_frag_delimiter);

     /* show the config in show run only when they are not defaults */
     if (strcmp(ts_str(t_segment_tag), "Seg" )) {
	 err = ts_append_sprintf(rev_cmd,
		 "     segment-tag  %s\n",
		 ts_str(t_segment_tag));
	 bail_error(err);
     }
     if (strcmp(ts_str(t_fragment_tag), "Frag" )) {
	 err = ts_append_sprintf(rev_cmd,
		 "     fragment-tag  %s\n",
		 ts_str(t_fragment_tag));
	 bail_error(err);
     }
     if (strcmp(ts_str(t_seg_frag_delimiter), "-" )) {
	 err = ts_append_sprintf(rev_cmd,
		 "     seg-frag-delimiter  %s\n",
		 ts_str(t_seg_frag_delimiter));
	 bail_error(err);
     }
     }

     CONSUME_REVMAP_NODES("%s/flash-fragment-tag");
     CONSUME_REVMAP_NODES("%s/segment-tag");
     CONSUME_REVMAP_NODES("%s/segment-delimiter-tag");

bail:
     ts_free(&t_segment_tag);
     ts_free(&t_fragment_tag);
     ts_free(&t_seg_frag_delimiter);
     return err;
}

int
cli_nvsd_bw_opt_config(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_ftype = NULL;
    const char *c_vp_name = NULL;
    const char *c_transcode = NULL;
    const char *c_hot_thres = NULL;
    const char *c_switch_rate = NULL;
    const char *c_switch_limit = NULL;
    const char *c_cmd_name = NULL;
    tbool is_allowed = false;
    char  node[MAX_NODE_LENGTH];
    char  n_transcode[MAX_NODE_LENGTH];
    char  n_trigger[MAX_NODE_LENGTH];
    char  active_node[MAX_NODE_LENGTH];
    char *bn_active = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    int num_params = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(data);



    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
	goto bail;
    }

    num_params = tstr_array_length_quick(params);

    /* get the VP name */
    c_vp_name = tstr_array_get_str_quick(params, 0);
    bail_null(c_vp_name);

    c_cmd_name = tstr_array_get_str_quick(cmd_line, 5);


    /* get the file-type */
    c_ftype = tstr_array_get_str_quick(params, 1);
    bail_null(c_ftype);



    snprintf( node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s", c_vp_name, c_ftype);
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
	    node, bt_string, 0, c_ftype, NULL);
    bail_error(err);


    if(!(strcmp(c_cmd_name, "transcode-comp-ratio"))) {
	/*get the transcode compression ratio*/
	c_transcode = tstr_array_get_str_quick(params, 2);
	bail_null(c_transcode);
	snprintf( n_transcode, sizeof(n_transcode), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/transcode_ratio", c_vp_name, c_ftype);
	err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
		n_transcode, bt_string, 0, c_transcode, NULL);
	bail_error(err);
	/* Get the  hotness -threshold value*/
	if( num_params == 4) {
	    c_hot_thres = tstr_array_get_str_quick(params, 3);
	    bail_null(c_hot_thres);
	    snprintf( n_trigger, sizeof(n_trigger), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/hotness_threshold", c_vp_name, c_ftype);
	    err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
		    n_trigger, bt_int32, 0, c_hot_thres, NULL);
	    bail_error(err);
	}
    } else {
	/*get the switch-rate option*/
	c_switch_rate = tstr_array_get_str_quick(params, 2);
	bail_null(c_switch_rate);
	snprintf( n_transcode, sizeof(n_transcode), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_rate", c_vp_name, c_ftype);
	err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
		n_transcode, bt_string, 0, c_switch_rate, NULL);
	bail_error(err);

	c_switch_limit = tstr_array_get_str_quick(params, 3);
	bail_null(c_switch_limit);
	snprintf( n_trigger, sizeof(n_trigger), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_limit", c_vp_name, c_ftype);
	err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
		n_trigger, bt_int32, 0, c_switch_limit, NULL);
	bail_error(err);
    }
    /*Set the active node for bandwidth optimization*/
    snprintf(active_node, sizeof(active_node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/active", c_vp_name);
    err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
	    active_node, bt_bool, 0, "true", NULL);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_err, &ret_msg);
    bail_error(err);
    bail_error_msg(ret_err, "%s", ts_str(ret_msg));
bail:
    safe_free(bn_active);
    ts_free(&ret_msg);
    bn_request_msg_free(&req);
    return err;
}


int 
cli_nvsd_bw_opt_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ftype = NULL;
    const char *c_vp_name = NULL;
    const char *c_cmd_name = NULL;
    tbool is_allowed = false;
    char node[MAX_NODE_LENGTH];
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tstr_array *file_type_arr = NULL;
    uint32 num_names = 0;
    tstring *transcode = NULL, *switch_rate = NULL;
    bn_binding *binding = NULL;

    /* Check if command is allowed */
    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);
    
    if (!is_allowed) {
	goto bail;
    }

    /* get the VP name */
    c_vp_name = tstr_array_get_str_quick(params, 0);
    bail_null(c_vp_name);

    /* get the command name */
    c_cmd_name = tstr_array_get_str_quick(cmd_line, 6);
    bail_null(c_cmd_name);

    /* get the file type  to be reset/deleted*/
    ftype = tstr_array_get_str_quick(params, 1);
    bail_null(ftype);

    if(!(strcmp(c_cmd_name, "transcode-comp-ratio"))) {
	snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/transcode_ratio", c_vp_name, ftype);
	err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, node);
	bail_error(err);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/hotness_threshold", c_vp_name, ftype);
	err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, node);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	/* Check if the switch rate value is set for this ftype
	 * If present , dont' delete the ftype node
	 * else delete the ftype
	 */
	err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding,
		NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_rate", c_vp_name, ftype);
	bail_error(err);
	if(!binding) {
	    snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s", c_vp_name, ftype);
	    err = mdc_delete_binding(cli_mcc, &ret_err, &ret_msg, node);
	    bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	}else {
	    err = bn_binding_get_tstr(binding, ba_value,  bt_string, NULL, &switch_rate);
	    bail_error(err);
	    if(ts_length(switch_rate) <= 0 ) {
		snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s", c_vp_name, ftype);
		err = mdc_delete_binding(cli_mcc, &ret_err, &ret_msg, node);
		bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	    }
	}


    }else {
	snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_rate", c_vp_name, ftype);
	err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, node);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_limit", c_vp_name, ftype);
	err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, node);
	bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	/* Check if the transcode value is set for this ftype
	 * If present , dont' delete the ftype node
	 * else delete the ftype
	 */
	err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding,
		NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/transcode_ratio", c_vp_name, ftype);
	bail_error(err);
	if(!binding) {
	    snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s", c_vp_name, ftype);
	    err = mdc_delete_binding(cli_mcc, &ret_err, &ret_msg, node);
	    bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	}else {
	    err = bn_binding_get_tstr(binding, ba_value,  bt_string, NULL, &transcode);
	    bail_error(err);
	    if(ts_length(transcode) <= 0 ) {
		snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s", c_vp_name, ftype);
		err = mdc_delete_binding(cli_mcc, &ret_err, &ret_msg, node);
		bail_error_msg(ret_err, "%s", ts_str(ret_msg));
	    }
	}
    }
    /* If the number of file type goes to zero disable bandwidth optimization*/
    snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype", c_vp_name);
    err = cli_nvsd_bw_opt_get_file_type(&file_type_arr, node);
    num_names = tstr_array_length_quick(file_type_arr);
    if (num_names <= 0) {
	/*Set the active node for bandwidth optimization*/
	snprintf(node, sizeof(node), "/nkn/nvsd/virtual_player/config/%s/bw_opt/active", c_vp_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_reset, bt_string,
		"false", node);

    }

bail:
    ts_free(&ret_msg);
    bn_binding_free(&binding);
    ts_free(&transcode);
    ts_free(&switch_rate);

    tstr_array_free(&file_type_arr);
    return err;

}

static void
cli_nvsd_print_bw_opt_config (const char *name)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint32 type = 0;	
    tbool t_active = false;
    char bw_opt[MAX_NODE_LENGTH];
    tstr_array *file_type_arr = NULL;
    uint32 num_names = 0, i = 0;
    tstring *transcode = NULL;
    tstring *switch_rate = NULL;
    

    /* Sanity Test */
    if (!name) return ;

    // Get type 
    err = cli_nvsd_vp_type_to_mask(name, &type);
    bail_error(err);

    snprintf(bw_opt, sizeof(bw_opt), "/nkn/nvsd/virtual_player/config/%s/bw_opt", name);
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding, 
                NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/active", name);
    bail_error(err);
    
    err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
    bail_error(err);

    if(t_active) {
		cli_printf(_("\nBandwidth Optimization Configuration\n"));
		cli_printf_query(_("  Enabled: #%s/active#\n"), bw_opt);
		snprintf(bw_opt, sizeof(bw_opt), "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype", name);
		err = cli_nvsd_bw_opt_get_file_type(&file_type_arr, bw_opt);
		num_names = tstr_array_length_quick(file_type_arr);
		if (num_names <= 0) {
			err = cli_printf(_("  No File type configured."));
			bail_error(err);
		}else {
			for(i = 0; i < num_names; ++i) {
				int32 hotness = 0;
				int32 switch_limit = 0;
				const char *ftype = tstr_array_get_str_quick(file_type_arr, i);
				err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding,
						NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/transcode_ratio", name, ftype);
				bail_error(err);

				err = bn_binding_get_tstr(binding, ba_value,  bt_string, NULL, &transcode);
				bail_error(err);

				err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding,
						NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/hotness_threshold", name, ftype);
				bail_error(err);

				err = bn_binding_get_int32(binding, ba_value, NULL, &hotness);
				bail_error(err);

				err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding,
						NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_rate", name, ftype);
				bail_error(err);

				err = bn_binding_get_tstr(binding, ba_value,  bt_string, NULL, &switch_rate);
				bail_error(err);

				err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &binding,
						NULL, "/nkn/nvsd/virtual_player/config/%s/bw_opt/ftype/%s/switch_limit", name, ftype);
				bail_error(err);

				err = bn_binding_get_int32(binding, ba_value, NULL, &switch_limit);
				bail_error(err);

				//file type 
				cli_printf( _("  File Type:%s \n"), ftype);
				if(ts_length(transcode) > 0 ){
				    // transcode ration
				    cli_printf( _("    Transcode Compression Ratio:%s\n"), ts_str(transcode));
				    // hotness threshold
				    cli_printf( _("    Hotness Trigger Threshold:%d\n"), hotness);
				}
				if(ts_length(switch_rate) > 0) {
				    // switch rate option
				    cli_printf( _("    Switch-rate option:%s\n"), ts_str(switch_rate));
				    // limit rate to
				    cli_printf( _("    Switch limit rate to:%d (Kbps)\n"), switch_limit);
				}
			}
		}

    }



bail:
    ts_free(&transcode);
    ts_free(&switch_rate);
    bn_binding_free(&binding);
    tstr_array_free(&file_type_arr);
    return;
}

static int
cli_nvsd_bw_opt_get_file_type(
        tstr_array **ret_names,
        const char *vpname)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &names_config,
            vpname, NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;

bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    return err;
}

int 
cli_vp_bw_opt_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *transcode = NULL;
    tstring *trigger_thres = NULL;
    tstring *is_active = NULL;
    tstring *rev_cmd = (tstring *) data;
    char node[MAX_NODE_LENGTH];
    const char *c_match_name = NULL;
    tstr_array *match_name_parts = NULL;
    uint32 num_matches = 0, idx;
    tstring *switch_rate = NULL;
    tstring *switch_limit = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);
    UNUSED_ARGUMENT(vp_name);

    // Process bandwidth optimization nodes

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &is_active, "%s/bw_opt/active", name);
    bail_error(err);

    if (ts_equal_str(is_active, "true", false)) {
        /* Get the list of file type and their node value*/
	snprintf(node, sizeof(node), "%s/bw_opt/ftype/*", name);
	err = bn_binding_array_get_name_part_tstr_array(bindings, node, 7, &match_name_parts);
	bail_error(err);
	if (!match_name_parts) 
	    goto bail;

	num_matches = tstr_array_length_quick(match_name_parts);
        for (idx = 0; idx < num_matches; idx++) {
            c_match_name = tstr_array_get_str_quick(match_name_parts, idx);
            bail_null(c_match_name);

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &transcode, "%s/bw_opt/ftype/%s/transcode_ratio", 
                    name, c_match_name);
            bail_error(err);

	    if(transcode && (ts_length(transcode) > 0)) {
		err = ts_append_sprintf(rev_cmd, "     bandwidth-opt file-type \"%s\" transcode-comp-ratio \"%s\"",
			c_match_name, ts_str_maybe_empty(transcode));
		bail_error(err);
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &trigger_thres, "%s/bw_opt/ftype/%s/hotness_threshold",
			name, c_match_name);
		bail_error(err);
		if(!(ts_equal_str(trigger_thres, "0", false))) {
		    err = ts_append_sprintf(rev_cmd, " activate-on %s", ts_str(trigger_thres));
		    bail_error(err);
		}
		err = ts_append_sprintf(rev_cmd, "\n");
		bail_error(err);
	    }

            err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                    NULL, &switch_rate, "%s/bw_opt/ftype/%s/switch_rate", 
                    name, c_match_name);
            bail_error(err);
	    if(switch_rate && (ts_length(switch_rate) > 0 )) {
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &switch_limit, "%s/bw_opt/ftype/%s/switch_limit",
			name, c_match_name);
		bail_error(err);
		err = ts_append_sprintf(rev_cmd, "     bandwidth-opt file-type \"%s\" switch-rate \"%s\" limit-rate-to %s",
			c_match_name, ts_str_maybe_empty(switch_rate), ts_str(switch_limit));
		bail_error(err);
		err = ts_append_sprintf(rev_cmd, "\n");
		bail_error(err);
	    }

#define CONSUME_BW_OPT_NODES(c) \
            {\
                err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name, c_match_name);\
                bail_error(err);\
            }
            CONSUME_BW_OPT_NODES("%s/bw_opt/ftype/%s");
            CONSUME_BW_OPT_NODES("%s/bw_opt/ftype/%s/transcode_ratio");
            CONSUME_BW_OPT_NODES("%s/bw_opt/ftype/%s/hotness_threshold");
            CONSUME_BW_OPT_NODES("%s/bw_opt/ftype/%s/switch_rate");
            CONSUME_BW_OPT_NODES("%s/bw_opt/ftype/%s/switch_limit");
#undef CONSUME_BW_OPT_NODES
        }
    }

    CONSUME_REVMAP_NODES("%s/bw_opt/active");
bail:
    tstr_array_free(&match_name_parts);
    return err;
}
static void
cli_nvsd_print_use_9byte_hdr (	const char *name)
{
    int err = 0;
    tstring *t_9byte = NULL;

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, false, &t_9byte, 
	    NULL, "/nkn/nvsd/virtual_player/config/%s/seek/use_9byte_hdr", name);
    bail_error(err);


    if(!strcmp(ts_str(t_9byte),"true")) {
	cli_printf(_(
		    "  Use 9byte hdr : Yes\n"));
    }

    bail:
	ts_free(&t_9byte);
	return;
}

static int 
cli_nvsd_vp_seek_flv_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    node_name_t  bn_name = {0};
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tbool is_allowed = false;
    uint32 type = 0;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }


    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);
    
    err = cli_nvsd_vp_type_to_mask(c_player, &type);
    bail_error(err);

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/virtual_player/config/%s/seek/flv-type",  c_player);
    /* Get the type of the player. If it is  youtube , set defualt as time-msec, else 
     * byte-offset
     */

    if(type == vp_type_5) {
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
		"time-msec", bn_name);
	bail_error(err);
    }else {
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg, bsso_modify, bt_string,
		"byte-offset", bn_name);
	bail_error(err);
    }
    /* the use-9byte-header is associated with seek-flv-type.
     * so when flv-type is reset, this value is also reset.
     */
    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/virtual_player/config/%s/seek/use_9byte_hdr",  c_player);
    err = mdc_reset_binding(cli_mcc, &ret_err, &ret_msg, bn_name);
    bail_error(err);
    

    if(ret_err != 0) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
    }


bail:
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_vp_rate_control_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tstring *ret_msg = NULL;
    bn_request *req = NULL;
    node_name_t vp_node = {0};
    tbool is_allowed = false;
    UNUSED_ARGUMENT(cmd);

    const char *c_player = NULL;
    const char *c_scheme = NULL;
    const char *c_option = NULL;
    const char *c_str = NULL;
    const char *c_rate = NULL;
    const char *c_valid = NULL;
    const char *c_burst = NULL;
    node_name_t valid = {0};
    uint32 code = 0;
    tbool is_burst = false;

    const char *burst = (char *)cmd->cc_exec_data;
    if(!(strcmp(burst, "1"))) {
	is_burst = true;
    }

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    /*Get the scheme */
    c_scheme = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(c_scheme);

    /*Get the sub-option*/
    c_option = tstr_array_get_str_quick(cmd_line, 4);
    bail_null(c_option);

    /*create the request message to set the config nodes*/
    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);



    snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", c_player);
    if(lc_is_prefix(c_scheme, "max-bit-rate", false)) {
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_uint32, RC_MBR);
	bail_error(err);
    }else {
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_uint32, RC_AFR);
	bail_error(err);
    }
    /*Get the sub-options*/
    if(lc_is_prefix(c_option, "auto-detect",false)) {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_bool, "true");
	bail_error(err);
	if(is_burst) {
	    c_burst = tstr_array_get_str_quick(params, 1); // second parameter is the burst-factor
	    bail_null(c_burst);
	}
	c_valid = "auto_detect";
    }else {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_reset, bt_bool, "false");
	bail_error(err);
    }

    if(lc_is_prefix(c_option, "static",false)) {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", c_player);
	c_rate = tstr_array_get_str_quick(params, 1); // second parameter is the static rate value.
	bail_null(c_rate);
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_uint32, c_rate);
	bail_error(err);
	if(is_burst) {
	    c_burst = tstr_array_get_str_quick(params, 2); // third parameter is the burst-factor
	    bail_null(c_burst);
	}
	c_valid = "static/rate";
    }else {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_reset, bt_uint32, "0");
	bail_error(err);
    }

    if(lc_is_prefix(c_option, "query-string-parm",false)) {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/query/str", c_player);
	c_str = tstr_array_get_str_quick(params, 1); // second parameter is the query_string value.
	bail_null(c_str);
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_string, c_str);
	bail_error(err);
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate", c_player);
	c_str = tstr_array_get_str_quick(params, 2); // third parameter is units.
	bail_null(c_str);
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_string, c_str);
	bail_error(err);
	if(is_burst) {
	    c_burst = tstr_array_get_str_quick(params, 3); // fourth parameter is the burst-factor
	    bail_null(c_burst);
	}
	c_valid = "query/str";
    }else {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/query/str", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_reset, bt_string, "");
	bail_error(err);
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_reset, bt_string, "");
	bail_error(err);
    }

    /*Set the burst factor*/
    if(is_burst) {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/burst", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_modify, bt_string, c_burst);
	bail_error(err);
    }else {
	snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/burst", c_player);
	err = cli_append_req_msg(req, vp_node, bsso_reset, bt_string, "");
	bail_error(err);
    }
    

    /*Set the feature as enabled*/
    snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/active", c_player);
    err = cli_append_req_msg(req, vp_node, bsso_modify, bt_bool, "true");
    bail_error(err);

    /*Set the valid link */
    snprintf(valid, sizeof(valid),"/nkn/nvsd/virtual_player/config/%s/rate_control/%s", c_player, c_valid);
    snprintf(vp_node, sizeof(vp_node),"/nkn/nvsd/virtual_player/config/%s/rate_control/valid", c_player);
    err = cli_append_req_msg(req, vp_node, bsso_modify, bt_link, valid);
    bail_error(err);

    /*Send it to mgmtd*/
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &code, &ret_msg);
    bail_error(err);
    if (code) {
	/* message has failed, goto bail */
	goto bail;
    }


bail:
    ts_free(&ret_msg);
    bn_request_msg_free(&req);
    return err;
}


int
cli_append_req_msg(bn_request *req,const char *node,
	bn_set_subop subop, bn_type type, const char *value)
{
    int err = 0;
    bn_binding *binding = NULL;
    bail_null(req);
    err = bn_binding_new_str_autoinval(&binding, node, ba_value,
	    type, 0, value);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append_binding_takeover(req, subop, 0, &binding, NULL);
    bail_error(err);
bail:
    return err;
}

static void
cli_nvsd_print_rate_control_config (
	const char *name, 
	const char *rc)
{
    int err = 0;
    node_name_t pattern = {0} ;
    bn_binding_array  *bindings = NULL;
    tstring *scheme = NULL;
    tstring *srate = NULL;
    tstring *qstr = NULL;
    tstring *qrate = NULL;
    tstring *burst = NULL;
    tstring *valid = NULL;
    tbool is_active = false;
    tbool is_auto = false;
    tbool is_found = false;
    tbool is_detect = false;
    tbool is_rate = false;
    tbool is_query = false;



    /* Sanity Test */
    if (!name || !rc) return ;
    
    snprintf(pattern, sizeof(pattern), "/nkn/nvsd/virtual_player/config/%s/rate_control", name);
    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
	    &bindings, true, true, pattern);
    bail_error(err);
//    bn_binding_array_dump("*********", bindings, LOG_NOTICE);
    snprintf(pattern, sizeof(pattern), "/nkn/nvsd/virtual_player/config/%s/rate_control/active", name);
    err = bn_binding_array_get_value_tbool_by_name  ( bindings, pattern, &is_found, &is_active);
    cli_printf(_("Rate control Configuration\n  Enabled: %s \n"), is_active?"Yes":"No");
    /*Scheme*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    NULL, &scheme, "/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", name);
    bail_error(err);
    if(ts_equal_str(scheme, RC_MBR, false)) {
	cli_printf(_("  Scheme : Max Bit Rate\n"));
    }else if(ts_equal_str(scheme, RC_AFR, false)){
	cli_printf(_("  Scheme : Assured Flow Rate\n"));
    }else {
	cli_printf(_("  Scheme : None\n"));
    }
    /*Sub option*/
    /*Get the valid link*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    NULL, &valid, "/nkn/nvsd/virtual_player/config/%s/rate_control/valid",name);
    bail_error(err);
    if(valid  && (ts_length(valid) >0)) {
	is_detect = ts_has_suffix_str(valid, "auto_detect",false);
	is_rate = ts_has_suffix_str(valid, "static/rate",false);
	is_query = ts_has_suffix_str(valid, "query/str",false);
    }
    /*Auto-detect*/
    snprintf(pattern, sizeof(pattern), "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect", name);
    err = bn_binding_array_get_value_tbool_by_name  ( bindings, pattern, &is_found, &is_auto);
    /*Static rate*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    NULL, &srate, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", name);
    bail_error(err);
    /*Query-string*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    NULL, &qstr, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str", name);
    bail_error(err);
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    NULL, &qrate, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate", name);
    bail_error(err);
    cli_printf(_("    Auto-detect:%s %s\n"), is_auto?"Enabled":"Disabled", is_detect?"(ENABLED)":"");
    cli_printf(_("    Static-rate:%s Kbps %s\n"), ts_str(srate), is_rate?"(ENABLED)":"");
    cli_printf(_("    URI-Query:%s %s %s\n"), ts_str(qstr), ts_str(qrate), is_query?"(ENABLED)":"");
    /*Burst-factor*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    NULL, &burst, "/nkn/nvsd/virtual_player/config/%s/rate_control/burst", name);
    bail_error(err);
    if(burst) {
	cli_printf(_("  Burst-Factor: %s\n"), ts_str(burst));
    }

    cli_printf(_("\n"));

bail:
    bn_binding_array_free(&bindings);
    ts_free(&scheme);
    ts_free(&srate);
    ts_free(&qstr);
    ts_free(&qrate);
    ts_free(&burst);
    ts_free(&valid);
    return;
}

int 
cli_vp_rate_control_revmap(
        void *data, 
        const cli_command *cmd, 
        const bn_binding_array *bindings, 
        const char *name,
        const tstr_array *name_parts, 
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply,
        const tstring *type,
        const char *vp_name)
{
    int err = 0;
    tstring *valid = NULL;
    tstring *rev_cmd = (tstring *) data;
    tstring *scheme = NULL;
    tstring *srate = NULL;
    tstring *qstr = NULL;
    tstring *qrate = NULL;
    tstring *burst = NULL;
    node_name_t  pattern = {0};
    tbool is_active = false;
    tbool is_found = false;


    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(type);

    snprintf(pattern, sizeof(pattern), "/nkn/nvsd/virtual_player/config/%s/rate_control/active", vp_name);
    err = bn_binding_array_get_value_tbool_by_name  ( bindings, pattern, &is_found, &is_active);

    if (is_active) {
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &valid, "/nkn/nvsd/virtual_player/config/%s/rate_control/valid", vp_name);
	bail_error(err);
	/*Scheme value*/
        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &scheme, "/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", vp_name);
        bail_error(err);
	if(ts_equal_str(scheme, RC_MBR, false)) {
                err = ts_append_sprintf(rev_cmd, "     rate-control max-bit-rate");
                bail_error(err);
	}else {
                err = ts_append_sprintf(rev_cmd, "     rate-control assured-flow-rate");
                bail_error(err);
	}


	/*Sub-options  Check which node is configured*/
        if (ts_length(valid) != 0) {

	    if(ts_has_suffix_str(valid, "auto_detect",false) ){
                err = ts_append_sprintf(rev_cmd, " auto-detect");
                bail_error(err);
            }
            else if ( ts_has_suffix_str(valid, "static/rate",false)) {
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &srate, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", vp_name);
		bail_error(err);
                err = ts_append_sprintf(rev_cmd, " static-rate \"%s\"", ts_str_maybe_empty(srate));
                bail_error(err);
            }
            else if ( ts_has_suffix_str(valid, "query/str",false)) {
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &qstr, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str", vp_name);
		bail_error(err);
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
			NULL, &qrate, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate", vp_name);
		bail_error(err);
		err = ts_append_sprintf(
			rev_cmd, " query-string-parm \"%s\" %s",
			ts_str_maybe_empty(qstr), ts_str_maybe_empty(qrate));
                bail_error(err);
            }
            else {
                /* Do nothing here */
            }
	    /*Burst factor*/
	    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		    NULL, &burst, "/nkn/nvsd/virtual_player/config/%s/rate_control/burst", vp_name);
	    bail_error(err);
	    if(burst && (!ts_equal_str(burst, RC_DEFAULT_BURST, false)) ){
                err = ts_append_sprintf(rev_cmd, " burst-factor \"%s\"", ts_str_maybe_empty(burst));
                bail_error(err);

	    }
	    err = ts_append_str(rev_cmd, "\n");
	    bail_error(err);
        }

    }

    CONSUME_REVMAP_NODES("%s/rate_control/valid");
    CONSUME_REVMAP_NODES("%s/rate_control/active");
    CONSUME_REVMAP_NODES("%s/rate_control/query/str");
    CONSUME_REVMAP_NODES("%s/rate_control/query/rate");
    CONSUME_REVMAP_NODES("%s/rate_control/burst");
    CONSUME_REVMAP_NODES("%s/rate_control/static/rate");
    CONSUME_REVMAP_NODES("%s/rate_control/scheme");
    CONSUME_REVMAP_NODES("%s/rate_control/auto_detect");
    
bail:
    ts_free(&scheme);
    ts_free(&srate);
    ts_free(&qstr);
    ts_free(&qrate);
    ts_free(&burst);
    ts_free(&valid);
    return err;
}

static int
cli_nvsd_vp_rate_control_config_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *c_player = NULL;
    node_name_t bn_name = {0};
    tbool is_allowed = false;

    err = cli_nvsd_vp_is_cmd_allowed(data, cmd, cmd_line, params, &is_allowed);
    bail_error(err);

    if (!is_allowed) {
        goto bail;
    }

    c_player = tstr_array_get_str_quick(params, 0);
    bail_null(c_player);

    snprintf(bn_name, sizeof(bn_name),"/nkn/nvsd/virtual_player/config/%s/rate_control/active", c_player);

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_reset, bt_bool,
            "false",
            bn_name);
    bail_error(err);

bail:
    return err;
}

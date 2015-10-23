/*
 * Filename :   cli_nvsd_namespace_cmds.c
 * Date :       2008/12/16
 * Author :     Dhruva Narasimhan
 *
 * (C) Copyright 2008, Nokeena Networks Inc.
 * All rights reserved.
 *
 */


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include <openssl/md5.h>

#include "url_utils.h"
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "nkn_mgmt_defs.h"

#include "libnkncli.h"
#include "nkn_defs.h"


#define	CACHE_INHERIT_SET_STR	"set"
#define	NS_DEFAULT_PORT		"80"
#define	ALL_STR			"all"

#define cli_nvsd_ns_precedence_help_msg		N_("A precedence number from 0-10, " 	\
						"with 0 being highest precedence "	\
						"and 10 being least")

#define MAX_PORT		65535
#define NS_HTTP_CLIENT_REQ  "namespace * delivery protocol http client-request "
#define NS_HTTP_ORIGIN_REQ  "namespace * delivery protocol http origin-request "
cli_expansion_option cli_namespace_cluster_hash_opts[] = {
    {"base-url", N_("Use base url"), (void*) "1"},
    {"complete-url", N_("Use complete url"), (void*) "2"},
    {NULL, NULL, NULL}
};

typedef enum en_proxy_type
{
	REVERSE_PROXY = 0,
	MID_TIER_PROXY = 1,
	VIRTUAL_PROXY = 2
} en_proxy_type;

/* Copied from mgmtd/modules/md_nvsd_namespace.c */
typedef enum {
	mt_none = 0,
	mt_uri_name,
	mt_uri_regex,
	mt_header_name,
	mt_header_regex,
	mt_query_string_name,
	mt_query_string_regex,
	mt_vhost,
	mt_rtsp_uri,
	mt_rtsp_uri_regex,
	mt_rtsp_vhost,
} match_type_t;

/* Copied from mgmtd/modules/md_nvsd_namespace.c */
typedef enum {
	osvr_none = 0,
	osvr_http_host,
	osvr_http_abs_url,
	osvr_http_follow_header,
	osvr_http_dest_ip,
	osvr_http_server_map,
	osvr_nfs_host,
	osvr_nfs_server_map,
	osvr_rtsp_host,
	osvr_rtsp_dest_ip,
	osvr_http_node_map,
} origin_server_type_t;



/* This enum clubs together the config. 
 * So when multiple namespaces are configured, the
 * o/p looks wierd. If anyone requires this 
 * desperately, revist this logic
 */
enum {
	crso_name = 1,
	crso_origin_server = 1,
	crso_domain = 1,
	crso_domain_regex = 1,
	crso_delivery_proto = 1,
	crso_match = 1,
	crso_origin_fetch = 1,
	crso_origin_request = 1,
	crso_client_request = 1,
	crso_client_request_geo = 1,
	crso_client_response = 1,
	crso_virtual_player = 1,
	crso_policy_map = 1,
	crso_proxy_mode = 1,
	crso_pub_point = 1,
	crso_precedence = 1,
	crso_status = 1,
	crso_cmm = 1,
	crso_pin = 1,
	crso_policy = 1,
	crso_media_cache = 1,
	crso_accesslog = 1,
	crso_compression = 1,
	crso_url_filter = 1,
};

typedef int (*cli_show_cl_suffix_t)(const char *ns_name, const char *cl_name, const char *suffix_name, tbool print_msg);

static cli_show_cl_suffix_t cli_show_cl_suffix  = NULL;
static cli_help_callback cli_virtual_player_name_help = NULL;
static cli_completion_callback cli_nvsd_virtual_player_name_comp = NULL;

enum {
    cid_nvsd_origin_fetch_cache_age_add_one = 1,
    cid_nvsd_origin_fetch_cache_age_delete_one,
};

enum {
	cc_oreq_header_add_one = 1,
	cc_oreq_header_delete_one,
};

enum {
	cc_clreq_header_accept_add_one = 1,
	cc_clreq_header_accept_delete_one,
};

enum {
	cc_clreq_header_add_one = 1,
	cc_clreq_header_delete_one,
};

enum {
	cc_clreq_reval_set 	= 1,
	cc_clreq_reval_unset 	= 2,
	cc_clreq_reval_inline 	= 4,
	cc_clreq_reval_offline 	= 8,
};

enum {
	cc_oreq_rtsp_rtp_udp = 1,
	cc_oreq_rtsp_rtp_rtsp,
	cc_oreq_rtsp_rtp_client,
};

enum {
	cc_magic_delivery_http =1,
	cc_magic_delivery_rtsp,
};

enum {
	client_request_reval_always_inline_enabled = 1,
	client_request_reval_always_offline_enabled,
	client_request_reval_always_disabled,
};

enum {
	cc_magic_ns_follow_dest_ip,
	cc_magic_ns_follow_host_hdr,
};

enum {
	cc_magic_ns_dyn_uri_regex_add = 1,
	cc_magic_ns_dyn_uri_all_add,
	cc_magic_ns_dyn_uri_str_delete,
	cc_magic_ns_dyn_uri_all_delete,
	cc_magic_ns_dyn_uri_tun_unset,
	cc_magic_ns_dyn_uri_regex_tokenize_add,
	cc_magic_ns_dyn_uri_regex_tokenize_delete,
};

enum {
	cc_magic_ns_seek_uri_regex_add = 1,
	cc_magic_ns_seek_uri_str_delete,
	cc_magic_ns_seek_uri_all_delete,
};

enum {
    cid_compress_filetype_delete = 1,
    cid_compress_filetype_add,
};

enum {
	cc_magic_ns_black = 1,
	cc_magic_ns_white ,
	cc_magic_ns_filter_close,
	cc_magic_ns_filter_reset,
	cc_magic_ns_filter_200,
	cc_magic_ns_filter_301,
	cc_magic_ns_filter_301_uri,
	cc_magic_ns_filter_302,
	cc_magic_ns_filter_302_uri,
	cc_magic_ns_filter_403,
	cc_magic_ns_filter_404
};

cli_expansion_option cli_namespace_compression_types[] = {
	{"deflate", N_("Deflate compression"), (void *) "deflate"},
	{"gzip", N_("Gzip Compression"), (void *)"gzip"},
	{NULL, NULL, NULL},
};
cli_expansion_option cli_unsuccessful_response_opts[] = {
        {"301", N_("Specify 301 response for caching"), (void *) "301"},
        {"302", N_("Specify 302 response for caching"), (void *) "302"},
        {"303", N_("Specify 303 response for caching"), (void *) "303"},
        {"403", N_("Specify 403 response for caching"), (void *) "403"},
        {"404", N_("Specify 404 response for caching"), (void *) "404"},
        {"406", N_("Specify 406 response for caching"), (void *) "406"},
        {"410", N_("Specify 410 response for caching"), (void *) "410"},
        {"414", N_("Specify 414 response for caching"), (void *) "414"},
        {NULL, NULL, NULL},
};


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

int check_smap_refresh_status(const char *smap_name, int *refresh_status);
/*---------------------------------------------------------------------------
 * Command Initializers
 *--------------------------------------------------------------------------*/
static int
cli_ns_filter_map_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);
int 
cli_nvsd_ns_filter_action(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int 
cli_nvsd_ns_filter_uri_size_action(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nvsd_namespace_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_namespace_object_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_namespace_http_origin_fetch_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_origin_request_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_filter_init(
        const lc_dso_info *info,
        const cli_module_context *context);
int 
cli_nvsd_namespace_http_client_request_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_client_response_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_cache_index_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_domain_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_match_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_match_uri_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_match_header_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_match_query_string_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_http_match_vhost_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int 
cli_nvsd_namespace_rtsp_match_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_rtsp_match_uri_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_rtsp_match_vhost_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int 
cli_nvsd_namespace_delivery_proto_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_namespace_origin_server_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_namespace_origin_server_http_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_origin_server_http_follow_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_origin_server_http_ipversion_cmds_init(
                const lc_dso_info *info,
                const cli_module_context *context);

int
cli_nvsd_namespace_origin_server_nfs_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_origin_server_rtsp_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_rtsp_origin_fetch_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_namespace_rtsp_cache_index_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_namespace_prestage_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_policy_map_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int 
cli_nvsd_namespace_http_origin_request_cache_reval_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_namespace_pub_point_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_policy_engine_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_namespace_smap_nfs_print_msg(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Configuration Callback functions - namespace * match <...>
 *--------------------------------------------------------------------------*/

int
cli_config_domain_fqdn(void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);

typedef enum {
    ns_domain_invalid = 0,
    ns_domain_fqdn_add,
    ns_domain_fqdn_delete,
    ns_domain_regex
}ns_domain_cmd_magic_t;

int cli_nvsd_ns_origin_server_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_ns_origin_server_follow_hdr_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_namespace_pe_hdlr(
                void *data,
                const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params);

int cli_nvsd_namespace_match_header_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_match_query_string_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_match_vhost_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * Configuration Callback functions - namespace * origin-server <...>
 *--------------------------------------------------------------------------*/
int cli_nvsd_namespace_origin_server_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_ns_host_header_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_ns_dscp_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_ns_obj_checksum_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_nvsd_http_server_map_show(const bn_binding_array *bindings, uint32 idx,
		const bn_binding *binding, const tstring *name,
		const tstr_array *name_components, const tstring *name_last_part,
		const tstring *value, void *callback_data);

int cli_nvsd_ns_cluster_show(const bn_binding_array *bindings, uint32 idx,
		const bn_binding *binding, const tstring *name,
		const tstr_array *name_components, const tstring *name_last_part,
		const tstring *value, void *callback_data);

int cli_nvsd_ns_vip_show(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data);

int cli_nvsd_namespace_origin_server_http_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_origin_server_nfs_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_origin_server_show_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_rtsp_pub_point_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_policy_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int cli_nvsd_namespace_origin_server_rtsp(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int cli_nvsd_ns_use_clip_cb( void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_delete(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_ns_name_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);

int
cli_ns_fqdn_completion( void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);
int
cli_ns_name_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int
cli_ns_uid_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int 
cli_ns_name_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_ns_uri_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);

int
cli_ns_uri_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int
cli_ns_cache_inherit_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

//int 
//cli_nvsd_config_uri_proto(
        //void *data,
        //const cli_command *cmd,
        //const tstr_array *cmd_line,
        //const tstr_array *params);

//int 
//cli_nvsd_uri_delivery_proto(
        //void *data,
        //const cli_command *cmd,
        //const tstr_array *cmd_line,
        //const tstr_array *params);

//int 
//cli_ns_uri_enter_mode(
        //void *data,
        //const cli_command *cmd,
        //const tstr_array *cmd_line,
        //const tstr_array *params);
//
//int
//cli_nvsd_config_uri_delivery_proto(
        //void *data,
        //const cli_command *cmd,
        //const tstr_array *cmd_line,
        //const tstr_array *params);

tbool
uid_prefix_match (
	const char *t_lost_uid,
	tstr_array *names);

int 
cli_nvsd_namespace_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_namespace_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_no_origin_fetch(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_no_bind_policy(
	void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_no_origin_reset_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int 
cli_nvsd_no_origin_delete_cache_age(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int
cli_nvsd_config_uri_delivery_proto_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

char *
get_extended_uid (
	const char *ns_uid,
	const char *proxy_domain,
	const char *proxy_domain_port);

int
cli_ns_object_list_delete_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

#if 0
/* PR 789822
 * Launch object delete script from CLI,
 * Untill we device an optimum method
 */
int
cli_ns_object_delete_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
#endif

int
cli_ns_validity_header_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int
cli_ns_object_reval_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int 
cli_nvsd_origin_fetch_cache_age_content_type(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_origin_fetch_cache_age_content_type_any(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_ns_compression_show_config(
	void *data,
	const cli_command 	*cmd,
	const tstr_array	*cmd_line,
	const tstr_array	*params);
int
cli_nvsd_ns_compression_file_types_show(
	const bn_binding_array *bindings,
        uint32 idx, 
	const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value, 
	void *callback_data);

static int
cli_nvsd_cache_age_entry_create(const char *name, 
                                int32 db_dev, 
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                uint32 *ret_code);

static int
cli_nvsd_cache_age_entry_modify(const char *name, 
                                int32 cache_age_index, 
                                int32 db_rev,
                                const tstr_array *cmd_line, 
				const char *cword,
				const char *param);

int
cli_compression_filetypes(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

cli_help_callback  cli_nvsd_pe_generic_help = NULL;

typedef int (*cli_nvsd_pe_is_valid_item_t)(const char *node, const char *match_val,
		tbool *is_valid);

cli_nvsd_pe_is_valid_item_t cli_nvsd_pe_is_valid_item = NULL;

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
//static int 
//cli_nvsd_namespace_list(
        //tstr_array **ret_completions,
        //tbool hide_display);

static int 
cli_ns_name_validate(
        const char *namespace,
        tbool *ret_valid);


static int 
cli_namespace_get_names(
        tstr_array **ret_names,
        tbool hide_display);

static int 
cli_namespace_get_pub_points(
        tstr_array **ret_names,
        tbool hide_display,
	const char *namespace);

static int 
cli_namespace_get_uids(
        tstr_array **ret_uids,
        tbool hide_display,
        const char *ns_name);

static int 
cli_nvsd_ns_is_reval_always_enabled(
	const char* ns, 
	int *retval,
	const bn_binding_array *bindings);

int 
cli_namespace_generic_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


static int
cli_nvsd_uri_escaped_name(
        const char *str,
        char **ret_str);

int 
cli_nvsd_namespace_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int 
cli_nvsd_origin_fetch_cache_age_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);
int 
cli_nvsd_origin_fetch_unsuccessful_cache_age_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);
int 
cli_nvsd_origin_http_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);
int
cli_nvsd_ns_servermap_add(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_nvsd_ns_servermap_delete(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_nvsd_ns_cache_index_header_add(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_nvsd_ns_cache_index_header_delete(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_nvsd_ns_cache_index_header_delete_all(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_ns_object_show_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_origin_http_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int
cli_nvsd_origin_http_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);

int
cli_nvsd_origin_server_disable(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_config_uri_delivery_proto_reset_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int
cli_nvsd_namespace_prestage_user_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int
cli_nvsd_http_smap_dupchk(
		const bn_binding_array *bindings,
		uint32 idx,
		const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value,
		void *callback_data);
int
cli_nvsd_ns_cache_index_header_dupchk(
		const bn_binding_array *bindings,
		uint32 idx,
		const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value,
		void *callback_data);
int
cli_nvsd_namespace_show_http_proto_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);
int
cli_nvsd_namespace_show_rtsp_proto_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

static int cli_nvsd_origin_fetch_cache_age_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_client_response_dscp_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_obj_checksum_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_origin_fetch_agg_thres_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_namespace_pe_revmap (void *data, const cli_command *cmd,
		const bn_binding_array *bindings,
		const char *name,
		const tstr_array *name_parts,
		const char *value, const bn_binding *binding,
		cli_revmap_reply *ret_reply);

int
cli_namespace_cache_inherit_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int 
cli_nvsd_namespace_cluster_hash_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_geo_service_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_cache_index_header_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int cli_nvsd_http_cache_index_include_headers_show(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data);

static int
cli_nvsd_origin_fetch_cache_revalidation_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
int 
cli_ns_is_valid_name(const char *name, tstring **ret_msg);


int 
cli_ns_get_disknames(
        tstr_array **ret_labels,
        tbool hide_display);


static int
cli_get_match_type (const char *namespace, match_type_t *mt, const bn_binding_array *bindings);

static int
cli_get_origin_type (const char *namespace, origin_server_type_t *ot, const bn_binding_array *bindings);

int
cli_nvsd_ns_dynamic_uri_exec_cb(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_nvsd_domain_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_uri_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
#if 0
static int
cli_nvsd_rtsp_uri_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
#endif
static int
cli_nvsd_uri_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
#if 0
static int
cli_nvsd_rtsp_uri_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
#endif
static int
cli_nvsd_header_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_header_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_query_string_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_query_string_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_vhost_name_port_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);


static int
cli_nvsd_ns_http_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_http_server_map_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_nfs_server_map_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_nfs_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_http_abs_url_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_http_follow_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_origin_req_cache_revalidation_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int 
cli_nvsd_ns_origin_request_header_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int 
cli_nvsd_ns_client_request_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int 
cli_nvsd_ns_client_request_cookie_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
int
cli_nvsd_ns_client_request_reval_always_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_ns_cache_control_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int 
cli_nvsd_origin_fetch_cache_fill_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
int 
cli_nvsd_origin_fetch_compression_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
static int
cli_nvsd_ns_origin_request_read_retry_delay_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_origin_request_read_timeout_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_origin_request_connect_timeout_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_origin_request_host_header_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);


static int
cli_nvsd_ns_origin_request_connect_retry_delay_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_ns_seek_uri_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_origin_fetch_cache_index_config(void *data, const cli_command *cmd,
					 const tstr_array *cmd_line,
					 const tstr_array *params);
int
cli_nvsd_origin_fetch_cache_index_offset_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
int 
cli_nvsd_namespace_generic_callback(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_origin_req_cache_revalidate_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_nvsd_origin_fetch_reval_always(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);


static int
cli_nvsd_origin_fetch_obj_thres_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

int
cli_nvsd_ns_origin_request_header_add(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_nvsd_origin_request_header_add(const char *name, 
                                int32 db_dev, 
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                uint32 *ret_code);

#if 0
static int
cli_nvsd_client_request_header_accept_add(const char *name,
                                int32 db_dev,
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                uint32 *ret_code);
#endif

static int
cli_nvsd_ns_client_request_header_accept_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);


int cli_nvsd_ns_client_request_header_accept_hdlr(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_ns_client_response_header_delete(void *data,
		const cli_command 	*cmd,
		const tstr_array 	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_client_response_header_add(void *data,
		const cli_command 	*cmd,
		const tstr_array 	*cmd_line,
		const tstr_array	*params);


int
cli_nvsd_ns_seek_uri_exec_cb(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int 
cli_nvsd_ns_origin_fetch_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_origin_fetch_rtsp_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_origin_request_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_client_request_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_live_pub_point_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_cache_pin_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int 
cli_nvsd_ns_media_cache_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

int
cli_nvsd_ns_client_request_header_accept_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int
cli_nvsd_ns_client_response_header_delete_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int 
cli_nvsd_ns_client_response_header_add_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

int 
cli_nvsd_ns_client_response_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params);

static int
cli_nvsd_ns_client_response_header_delete_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_ns_client_response_header_add_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_ns_dynamic_uri_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int 
cli_nvsd_origin_request_header_add_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);

tbool
cli_nvsd_ns_is_active(const char *name);

static int
cli_nvsd_ns_mdc_array_get_matching_indices(
		md_client_context *mcc, 
		const char *root_name, 
		const char *db_name, 
		const char *value,
		//const bn_binding_array *children, 
		int32 rev_before, 
		int32 *ret_rev_after, 
		uint32_array **ret_indices, 
		uint32 *ret_code, 
		tstring **ret_msg);

static int 
cli_nvsd_ns_warn214_header_mods(const char *header);

int cli_nvsd_namespace_delivery_proto_match_details(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params,
		const char *c_proto_name);

static int
cli_nvsd_ns_rtsp_follow_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int cli_nvsd_ns_check_if_consumed(const tstr_array *crr_nodes, 
		const char *format, ...) 
	__attribute__ ((format (printf, 2, 3)));
#if 0
static int
cli_nvsd_rtsp_vhost_name_port_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);
#endif
int cli_nvsd_namespace_rtsp_match_vhost_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_nvsd_ns_rtsp_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_namespace_rtsp_pub_point_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_namespace_object_revmap (void *data, const cli_command *cmd,
		const bn_binding_array *bindings,
		const char *name,
		const tstr_array *name_parts,
		const char *value, const bn_binding *binding,
		cli_revmap_reply *ret_reply);

int 
cli_nvsd_origin_fetch_byte_range_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int cli_nvsd_origin_fetch_rtsp_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_ns_show_counters(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_ns_show_resource_monitoring_counters(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_ns_rtsp_cache_index_show_config(void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);

int
cli_nvsd_ns_cache_index_show_config(void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);

const int32 cli_nvsd_namespace_cmds_init_order = INT32_MAX - 2;
int header_dup = 0;

int
cli_nvsd_namespace_disk_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nvsd_namespace_vip_bind_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *callback = NULL;


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


    /* Must get the help and completion callbacks from
     * cli_nvsd_virtual_player_cmds 
     */

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_nvsd_virtual_player_cmds", "cli_virtual_player_name_help", 
            &callback);
    bail_error_null(err, callback);

    cli_virtual_player_name_help = (cli_help_callback)(callback);

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_nvsd_virtual_player_cmds", "cli_nvsd_virtual_player_name_comp", 
            &callback);
    bail_error_null(err, callback);
    cli_nvsd_virtual_player_name_comp = (cli_completion_callback)(callback);

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_nvsd_cluster_cmds", "cli_show_cl_suffix", 
            &callback);
    bail_error_null(err, callback);

    cli_show_cl_suffix = ( cli_show_cl_suffix_t)(callback);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace";
    cmd->cc_help_descr =        N_("Configure Namespace options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace";
    cmd->cc_help_descr =        N_("Disable namespace options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     N_("New namespace name");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_callback =     cli_ns_name_help;
    cmd->cc_comp_callback =     cli_ns_name_completion;
//    cmd->cc_comp_type =         cct_matching_names;
//    cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/*";
    cmd->cc_exec_callback =     cli_ns_name_enter_mode;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    //cmd->cc_revmap_order =      cro_namespace;
    //cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     N_("Namespace name");
    cmd->cc_help_callback =     cli_ns_name_help;
    cmd->cc_comp_callback =     cli_ns_name_completion;
//    cmd->cc_comp_type =         cct_matching_names;
//    cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/*";
    cmd->cc_flags =             ccf_terminal; 
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_namespace_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * no";
    cmd->cc_help_descr =        N_("Clear/remove existing namespace configuration options");
    cmd->cc_req_prefix_count =  2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * precedence";
    cmd->cc_help_descr =        "Set precedence for namespace";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * precedence *";
    cmd->cc_help_exp =          "<11-65535>";
    cmd->cc_help_exp_hint =	"namespace precedence value";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/ns-precedence";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   crso_precedence;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * status";
    cmd->cc_help_descr =        N_("Set Status of this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * status inactive";
    cmd->cc_help_descr =        N_("Disable this namespace");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/status/active";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   crso_status;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * status active";
    cmd->cc_help_descr =        N_("Enable this namespace");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/status/active";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   crso_status;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cache-inherit";
    cmd->cc_help_descr =        N_("Inherit cache parameters from another namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cache-inherit *";
    cmd->cc_help_exp =          N_("<uid>");
    cmd->cc_help_exp_hint =     N_("Namespace's UID");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_callback =     cli_ns_cache_inherit_comp;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/cache_inherit";
    cmd->cc_exec_value =        CACHE_INHERIT_SET_STR;
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_name2 =        "/nkn/nvsd/namespace/$1$/uid";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_string;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names =      "/nkn/nvsd/namespace/*/cache_inherit";
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_namespace_cache_inherit_revmap;
    cmd->cc_revmap_suborder =	crso_origin_fetch;
    CLI_CMD_REGISTER;

    /*------------------------------------------------------------------------
     * ACCESSLOG BINDINGS
     *----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * accesslog";
    cmd->cc_help_descr =        N_("Bind to an access log profile");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * accesslog";
    cmd->cc_help_descr =        N_("Reset to use default access log profile");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/accesslog";
    cmd->cc_exec_value =	"default";
    cmd->cc_exec_type =		bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * accesslog *";
    cmd->cc_help_exp =		N_("<log-template-name>");
    cmd->cc_help_exp_hint =	N_("Accesslog profile name");
    cmd->cc_help_use_comp =	true;
    cmd->cc_comp_type =		cct_matching_values;
    cmd->cc_comp_pattern =	"/nkn/accesslog/config/profile/*";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/accesslog";
    cmd->cc_exec_value =	"$2$";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_revmap_suborder =	crso_accesslog;
    cmd->cc_revmap_names =	"/nkn/nvsd/namespace/*/accesslog";
    cmd->cc_revmap_cmds =	"namespace $4$ accesslog $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster-hash";
    cmd->cc_help_descr =        N_("Cluster Hash Parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster-hash base-url";
    cmd->cc_help_descr =        N_("Cluster Hash base-url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/cluster-hash";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_suborder =   crso_cmm;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster-hash complete-url";
    cmd->cc_help_descr =        N_("Cluster Hash complete-url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/cluster-hash";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names = 	"/nkn/nvsd/namespace/*/cluster-hash";
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_namespace_cluster_hash_revmap;
    cmd->cc_revmap_suborder =   crso_cmm;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object";
    cmd->cc_help_descr =        N_("Every object ingested to this namespace will be cache-pinned");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object auto-pin";
    cmd->cc_help_descr =        N_("Every object ingested to this namespace will be cache-pinned");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/pinned_object/auto_pin";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_pin;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pinned-object";
    cmd->cc_help_descr =        N_("Remove the cache pinning config for this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pinned-object auto-pin";
    cmd->cc_help_descr =        N_("Remove the cache pinning config for this namespace");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/pinned_object/auto_pin";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit";
    cmd->cc_help_descr =        N_("Resource limitation at namespace level");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier";
    cmd->cc_help_descr =        N_("Namespace level resource limitation for cache-tier ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier sas";
    cmd->cc_help_descr =        N_("Namespace level resource limitation for sas");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier sata";
    cmd->cc_help_descr =        N_("Namespace level resource limitation for sata");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier ssd";
    cmd->cc_help_descr =        N_("Namespace level resource limitation for ssd");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier sas cache-capacity";
    cmd->cc_help_descr =        N_("Set namespace level cache-capaciy for sas");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier sata cache-capacity";
    cmd->cc_help_descr =        N_("Set namespace level cache-capaciy for sas");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier ssd cache-capacity";
    cmd->cc_help_descr =        N_("Set namespace level cache-capaciy for sas");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier sas cache-capacity *";
    cmd->cc_help_exp =          N_("<Cache-capacity>");
    cmd->cc_help_exp_hint =	N_("Cache-capacity in MB");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/resource/cache/sas/capacity";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint64;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder = 	crso_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier sata cache-capacity *";
    cmd->cc_help_exp =          N_("<Cache-capacity>");
    cmd->cc_help_exp_hint =     N_("Cache-capacity in MB");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/resource/cache/sata/capacity";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint64;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder = 	crso_media_cache;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * resource-limit cache-tier ssd cache-capacity *";
    cmd->cc_help_exp =          N_("<Cache-capacity>");
    cmd->cc_help_exp_hint =     N_("Cache-capacity in MB");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/resource/cache/ssd/capacity";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint64;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder = 	crso_media_cache;
    CLI_CMD_REGISTER;

    /*                      object management                              */ 
    err = cli_nvsd_namespace_object_cmds_init(info, context);
    bail_error(err);

    /*---------------------------------------------------------------------*/
    /*                      URI <URI-PREFIX>                               */ 
    err = cli_nvsd_namespace_domain_cmds_init(info, context);
    bail_error(err);

#if 1 /* Commented out for Appalacian as CLI structure would change
       * for Baffin and Cayoosh - Ramanand 15th Jan 2010
       */
    err = cli_nvsd_namespace_delivery_proto_cmds_init(info, context);
    bail_error(err);
#endif /* 0 */

    err = cli_nvsd_namespace_origin_server_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_match_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_prestage_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_origin_fetch_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_origin_request_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_client_request_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_filter_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_client_response_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_cache_index_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_policy_map_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_pub_point_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_policy_engine_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_rtsp_match_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_rtsp_origin_fetch_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_rtsp_cache_index_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_disk_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_vip_bind_cmds_init(info, context);
    bail_error(err);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * virtual-player";
    cmd->cc_help_descr =        N_("Add a virtual player");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * virtual-player *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     ""; //N_("Virtual player name");
    cmd->cc_help_callback =     cli_virtual_player_name_help;
    cmd->cc_comp_callback =     cli_nvsd_virtual_player_name_comp;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/virtual_player";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder = 	crso_virtual_player;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * virtual-player";
    cmd->cc_help_descr =        N_("Add a virtual player");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * virtual-player *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     N_("Virtual player name");
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/*/virtual_player";
    //cmd->cc_help_callback =     cli_virtual_player_name_help;
    //cmd->cc_comp_callback =     cli_nvsd_virtual_player_name_comp;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/virtual_player";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace";
    cmd->cc_help_descr =        N_("Display namespace configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace list";
    cmd->cc_help_descr =        N_("Display a list of namespaces configured");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_namespace_show_list;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace *";
    cmd->cc_help_exp =          N_("<namespace name>");
    cmd->cc_help_exp_hint =     "";
    cmd->cc_help_callback =     cli_ns_name_help;
    cmd->cc_comp_callback =     cli_ns_name_completion;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_namespace_show_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object ";
    cmd->cc_help_descr =        N_("Check if URI exists");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list";
    cmd->cc_help_descr =        N_("Display the object list in namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list all";
    cmd->cc_help_descr =        N_("Display the object list in namespace");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =		cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list all export";
    cmd->cc_help_descr =        N_("Upload the generated objects list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list all export *";
    cmd->cc_help_exp =          N_("Remote URL");
    cmd->cc_help_exp_hint =     N_("<scp://username:password@hostname/path>");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =		cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list *";
    cmd->cc_help_exp =          N_("<URI> | <pattern>");
    cmd->cc_help_exp_hint =     N_("Listing object(s) in the cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list * export";
    cmd->cc_help_descr =        N_("Upload the generated objects list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list * export *";
    cmd->cc_help_exp =          N_("Remote URL");
    cmd->cc_help_exp_hint =     N_("<scp://username:password@hostname/path>");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =		cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list * *";
    cmd->cc_help_exp   =        N_("<domain name>");
    cmd->cc_help_exp_hint =     N_("Valid only in the case of transparent & mid-tier proxy");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =	cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list * * *";
    cmd->cc_help_exp =          N_("<port>");
    cmd->cc_help_exp_hint =     N_("Defaults  to 80");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =	cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list * * * export";
    cmd->cc_help_descr =        N_("Upload the generated objects list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show namespace * object list * * * export *";
    cmd->cc_help_exp =          N_("Remote URL");
    cmd->cc_help_exp_hint =     N_("<scp://username:password@hostname/path>");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =		cli_ns_object_show_cmd_hdlr;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"show namespace * counters";
    cmd->cc_help_descr =	N_("Show namespace specific counters");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_query_basic_curr;
    cmd->cc_exec_callback = 	cli_ns_show_counters;
    CLI_CMD_REGISTER;
#if 0
    //Comenting out the s separate CLI command for showing resource monitoring counters.
    //Thilak - Oct 18,2010
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"show namespace * counters resoure-monitoring";
    cmd->cc_help_descr =	N_("Show namespace specific counters");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_query_basic_curr;
    cmd->cc_exec_callback = 	cli_ns_show_resource_monitoring_counters;
    CLI_CMD_REGISTER;
#endif
    err = cli_revmap_exclude_bindings("/nkn/nvsd/namespace/*/prestage/user/*");
    bail_error(err);

    err = cli_revmap_ignore_bindings_va(26,
		"/nkn/nvsd/namespace/*",
		"/nkn/nvsd/namespace/*/match/precedence",
		"/nkn/nvsd/namespace/*/match/type",
		"/nkn/nvsd/namespace/*/origin-server/type",
		"/nkn/nvsd/namespace/*/delivery/proto/*",
                "/nkn/nvsd/originmgr/config/**",
                "/nkn/oomgr/config/**",
                "/nkn/cmm/config/**",
                "/nkn/nvsd/origin_fetch/config/*/header/**",
		"/nkn/nvsd/origin_fetch/config/*/client-request/cache-control/max_age",
		"/nkn/nvsd/origin_fetch/config/*/object/**",
		"/nkn/nvsd/namespace/*/proxy-mode",
                "/nkn/nvsd/uri/config/*/uri-prefix/*/domain/regex/map",
                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/server-map",
                "/nkn/nvsd/namespace/*/rtsp/match/precedence",
                "/nkn/nvsd/namespace/*/rtsp/match/type",
		"/nkn/nvsd/namespace/mfc_probe/prestage/user/mfc_probe_ftpuser",
		"/nkn/nvsd/namespace/mfc_probe/prestage/user/mfc_probe_ftpuser/password",
		"/nkn/nvsd/origin_fetch/config/*/cache-index/include-header",
		"/nkn/nvsd/origin_fetch/config/*/cache-index/include-object",
		"/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-bytes",
		"/nkn/nvsd/origin_fetch/config/*/cache-index/include-headers/*",
		"/nkn/nvsd/namespace/*/client-request/seek-uri/enable",
		"/nkn/nvsd/origin_fetch/config/*/tunnel-override/**",
		"/nkn/nvsd/namespace/*/pinned_object/auto_pin",
		"nkn/nvsd/namespace/*/distrib-id");

    bail_error(err);

bail:
    return err;
}

int cli_nvsd_http_smap_dupchk(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *server_map = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);

    if (idx != 0)
	cli_printf(_("\n"));

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &server_map);
    bail_error(err);

    if (safe_strcmp (ts_str(server_map) , (char *)callback_data) == 0)
    	{
			 cli_printf(_("Server-map %s already exists. Blocking the duplicate entry\n"), ts_str(server_map));
    	}

    bail:
	ts_free(&server_map);
	return(err);
}
int
cli_nvsd_ns_servermap_add(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    uint32 code = 0;
    tbool dup_deleted = false;
    tstring *msg = NULL;
    const char *map_name = NULL;
    char *node_name = NULL;
    const char *ns_name = NULL;
    char *smap_binding = NULL;
    int refresh_status = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    map_name = tstr_array_get_str_quick(params, 1);
    bail_null(map_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(map_name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

#if 0
    err = check_smap_refresh_status(map_name, &refresh_status);
    bail_error(err);

    if (refresh_status == 0) {
	cli_printf_error("server-map %s:  map file refresh status should be true", map_name);
	goto bail;
    }
#endif
    smap_binding = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name", ns_name);
    bail_null(smap_binding);

    node_name = smprintf ("/nkn/nvsd/namespace/%s/origin-server/http/server-map", ns_name);

    err = mdc_array_append_single(cli_mcc,
			node_name,
			"map-name",
			bt_string,
			map_name,
			false,
			&dup_deleted,
			&code,
			&msg);
    bail_error(err);

bail:
    ts_free(&msg);
    bn_binding_array_free(&barr);

  return(err);
}

int
check_smap_refresh_status(const char *smap_name, int *refresh_status)
{
    node_name_t node_name = {0};
    bn_binding *binding = NULL;
    int err = 0;
    tbool map_status = false;

    bail_require(smap_name);
    bail_require(refresh_status);

    *refresh_status = 0;

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/server-map/config/%s/map-status", smap_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, node_name, NULL);
    bail_error(err);

    if (binding) {
	err = bn_binding_get_tbool(binding, ba_value, NULL, &map_status);
	bail_error(err);
	if (map_status == true) {
	    *refresh_status = 1;
	}
    }

bail:
    bn_binding_free(&binding);
    return err;
}
int
cli_nvsd_ns_servermap_delete(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    const char *map_name = NULL;
    char *node_name = NULL;
    const char *ns_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    map_name = tstr_array_get_str_quick(params, 1);
    bail_null(map_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(map_name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    node_name = smprintf ("/nkn/nvsd/namespace/%s/origin-server/http/server-map", ns_name);

    err = mdc_array_delete_by_value_single(cli_mcc,
                node_name,
                true, "map-name", bt_string,
                map_name, NULL, NULL, NULL);
    bail_error(err);

bail:

    bn_binding_array_free(&barr);

  return(err);
}

int 
cli_ns_is_valid_name(const char *name, tstring **ret_msg)
{
    int err = 0;
    int i = 0;
    const char *p = " /\\*~=.-+:|`\"?";
    int j = 0;
    int k = strlen(name);
    int l = strlen(p);

    if (name[0] == '_') {
        if ( ret_msg != NULL )
            err = ts_new_str(ret_msg, "Bad namespace name. Name cannot have '_' as prefix");
        err = lc_err_not_found;
        goto bail;
    }

    for(i = 0; i < l; i++) {
        for(j = 0; j < k; j++) {
            if ( p[i] == name[j] ) {
                if ( ret_msg != NULL )
                    err = ts_new_sprintf(ret_msg, "Value '%s' contains illegal characters", name);
                err = lc_err_not_found;
                goto bail;
            }
        }
    }

bail:
    return err;
}

int 
cli_ns_name_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_err = 0;
    tbool valid = false;
    char *binding_name = NULL;
    char *origin_fetch_binding = NULL;
    tstring *ret_msg = NULL;

    UNUSED_ARGUMENT(data);

    err = cli_ns_is_valid_name(tstr_array_get_str_quick(params, 0), &ret_msg);
    if ( err != 0 ) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
        err = 0;
        goto bail;
    }
    err = cli_ns_name_validate(tstr_array_get_str_quick(params, 0),
            &valid);
    //bail_error(err);
    //

    // If we are running in EXEC mode, then we need to check
    // that the user didnt give a bigus namespace name.
    //
    if ( !cli_have_capabilities(ccp_set_rstr_curr) && !valid ) {
        cli_printf_error(_("Unknown namespace '%s'"), 
                tstr_array_get_str_quick(params, 0));
        err = 0;
        goto bail;
    }
    // If we are running in EXEC mode, and namespace name is valid
    // DO NOT allow to enter into prefix mode.
    else if ( !cli_have_capabilities(ccp_set_rstr_curr) && valid ) {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);

        goto bail;
    }

    if(!valid) {
        // Node doesnt exist.. create it
            binding_name = smprintf("/nkn/nvsd/namespace/%s", 
                    tstr_array_get_str_quick(params, 0));
            bail_null(binding_name);

            err = mdc_create_binding(cli_mcc,
                    &ret_err,
                    &ret_msg,
                    bt_string,
                    tstr_array_get_str_quick(params, 0),
                    binding_name);
            bail_error(err);
            bail_error_msg(ret_err, "%s", ts_str(ret_msg));


        valid = true;
    }

    if (cli_prefix_modes_is_enabled()) {
        err = cli_ns_name_validate(tstr_array_get_str_quick(params, 0),
                &valid);
        bail_error(err);

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
    safe_free(binding_name);
    safe_free(origin_fetch_binding);
    return err;
}

static int 
cli_ns_name_validate(
        const char *namespace,
        tbool *ret_valid)
{
    int err = 0;
    tstr_array *ns = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_namespace_get_names(&ns, false);
    bail_error(err);


    err = tstr_array_linear_search_str(ns, namespace, 0, &idx);
    if (lc_err_not_found != err) {
        bail_error(err);
        *ret_valid = true;
    }
    else {
        err = 0;
        // This condition doesnt exist since
        //  - if name does not exist, we create it
        //  - if name exists then this is never hit
        //err = cli_printf_error(_("Unknown namespace - %s\n"), namespace);
        //bail_error(err);
    }
bail:
    tstr_array_free(&ns);
    return err;
}

int 
cli_namespace_get_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    char *bn_name = NULL;
    bn_binding *display_binding = NULL;

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
            "/nkn/nvsd/namespace", NULL);
    bail_error_null(err, names_config);

    /* Remove mfc_probe */

    err = tstr_array_delete_str(names_config, "mfc_probe");
    bail_error(err);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    safe_free(bn_name);
    bn_binding_free(&display_binding);
    return err;
}


int
cli_ns_fqdn_completion( void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *fqdns = NULL;
    node_name_t fqdn_node = {0};
    const char *ns_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    snprintf(fqdn_node, sizeof(fqdn_node),
	    "/nkn/nvsd/namespace/%s/fqdn-list", ns_name);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &fqdns, fqdn_node, NULL);
    bail_error_null(err, fqdns);

    err = tstr_array_concat(ret_completions, &fqdns);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    return err;
}


int
cli_ns_name_completion(
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

    err = cli_namespace_get_names(&names, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    return err;
}
/*--------------------------------------------------------------------------*/

static int 
cli_namespace_get_uids(
        tstr_array **ret_uids,
        tbool hide_display,
        const char *ns_name)
{
    int err = 0;
    tstr_array *uids = NULL;
    tstr_array_options opts;
    tstr_array *uids_config = NULL;
    char *bn_uri = NULL;
    bn_binding *display_binding = NULL;
    char *binding_name = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_uids);
    *ret_uids = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&uids, &opts);
    bail_error(err);

    binding_name = smprintf("/nkn/nvsd/uri/config/%s/uid", ns_name);
    bail_null(binding_name);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &uids_config, 
            binding_name,
            NULL);
    bail_error_null(err, uids_config);

    err = tstr_array_concat(uids, &uids_config);
    bail_error(err);

    *ret_uids = uids;
    uids = NULL;
bail:
    tstr_array_free(&uids);
    tstr_array_free(&uids_config);
    safe_free(bn_uri);
    safe_free(binding_name);
    bn_binding_free(&display_binding);
    return err;
}

int
cli_ns_uid_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *uids = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_namespace_get_uids(&uids, true, NULL);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &uids);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&uids);
    return err;
}

int
cli_ns_name_help(
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
    const char *namespace = NULL;
    int i = 0, num_names = 0;


    switch(help_type)
    {
    case cht_termination:
        if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context, 
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if(cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&names, NULL);
        bail_error(err);

        err = cli_ns_name_completion(data, cmd, cmd_line, params, 
                curr_word, names);
        bail_error(err);

        num_names = tstr_array_length_quick(names);
        for(i = 0; i < num_names; ++i) {
            namespace = tstr_array_get_str_quick(names, i);
            bail_null(namespace);
            
            err = cli_add_expansion_help(context, namespace, NULL);
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


/*--------------------------------------------------------------------------*/


int
cli_ns_cache_inherit_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    char *binding_name = NULL;
    tstr_array *names = NULL;
    bn_binding_array *lost_uid_array = NULL;
    tstring *t_current_uid = NULL;
    uint32 binding_array_length = 0;
    tstring *t_lost_uid = NULL;
    char buff [8] = { 0 };
    uint32 ret_err = 0;
    uint32 num_names = 0;
    uint32 idx = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(curr_word);

    err = cli_namespace_get_names(&names, true);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);

    if (num_names > 0) {
        for(idx = 0; idx < num_names; ++idx) {
    	    binding_name = smprintf("/nkn/nvsd/namespace/%s/uid",
	    			tstr_array_get_str_quick(names, idx));
    	    bail_null(binding_name);
    	    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_current_uid, binding_name, NULL);
    	    bail_error(err);
            if (0 != safe_strcmp(ts_str_maybe_empty(t_current_uid),""))				
            { 
                err = tstr_array_append(ret_completions, t_current_uid);
	        bail_error(err);
            }
            safe_free(binding_name);
            ts_free(&t_current_uid); 
        }
    }
    /* Now get the unmapped UIDs from the disk */
    err = mdc_send_action_with_bindings_and_results (cli_mcc, &ret_err, NULL, 
                   "/nkn/nvsd/namespace/actions/get_lost_uids", NULL, &lost_uid_array);
    bail_error (err);
     if (ret_err != 0)
    {
         cli_printf_error(_("error: failed to get lost uids from disk\n"));
	 goto bail;
    }
    err = bn_binding_array_length( lost_uid_array, &binding_array_length);
    bail_error (err);

    
    for (idx = 1 ; idx <= binding_array_length ; ++idx)
    {
        sprintf(buff,"%d",idx);
        err = bn_binding_array_get_value_tstr_by_name ( lost_uid_array,
                       buff, NULL, &t_lost_uid);
        bail_error(err);
	if (0 != safe_strcmp(ts_str_maybe_empty(t_lost_uid),""))				
	{
        	err = tstr_array_append(ret_completions, t_lost_uid);
		bail_error(err);
	}
        ts_free(&t_lost_uid); 
    }

bail:
   safe_free(binding_name);
   ts_free(&t_lost_uid); 
   tstr_array_free (&names);
   ts_free(&t_current_uid); 
   bn_binding_array_free(&lost_uid_array);
   return err;   
}


#if 0
int
cli_ns_uri_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *uris = NULL;
    const char *ns_name = NULL;

    ns_name = tstr_array_get_str_quick(params, 0);
    
    
    err = cli_namespace_get_uris(&uris, true, ns_name);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &uris);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&uris);
    return err;
}


int
cli_ns_uri_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *uris = NULL;
    const char *uri = NULL;
    int i = 0, num_uris = 0;


    switch(help_type)
    {
    case cht_termination:
        if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context, 
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if(cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&uris, NULL);
        bail_error(err);

        err = cli_ns_uri_completion(data, cmd, cmd_line, params, 
                curr_word, uris);
        bail_error(err);

        num_uris = tstr_array_length_quick(uris);
        for(i = 0; i < num_uris; ++i) {
            uri = tstr_array_get_str_quick(uris, i);
            bail_null(uri);
            
            err = cli_add_expansion_help(context, uri, NULL);
            bail_error(err);
        }
        break;
    
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }
bail:
    tstr_array_free(&uris);
    return err;
}
#endif
/*--------------------------------------------------------------------------*/

#if 0
int 
cli_nvsd_config_uri_proto(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *proto = NULL;
    const char *ns = NULL;
    char *uri = NULL;
    const char *hostname = NULL;
    const char *port = NULL;
    const char *keepalive = NULL;
    const char *weight = NULL;
    char *bn_name_host = NULL;
    char *bn_name_port = NULL;
    char *bn_name_keepalive = NULL;
    char *bn_name_weight = NULL;
    char *bn_name_local_cache = NULL;
    uint32 idx = 0;
    tbool is_nfs = false;
    uint32 ret_err = 0;
    const char *smap[] = {"http", "nfs"};
    const char *c_proto = NULL;

    //tstr_array_print(cmd_line, "cmd");
    //tstr_array_print(params, "params");

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    err = cli_nvsd_uri_escaped_name(tstr_array_get_str_quick(params, 1), &uri);
    bail_error_null(err, uri);

    // fix bug : 1659
    // keyword nfs can occur at position 5, so a minimum start index of 4 is ok
    //err = tstr_array_linear_search_str(cmd_line, "nfs", 4, &idx);
    // fix bug : 1882
    // Supercedes fix for 1659
    // Get the keyword at position 4 and check if it is a short form 
    // of one of the smap
    c_proto = tstr_array_get_str_quick(cmd_line, 5);
    bail_null(c_proto);

    if (lc_is_prefix(c_proto, "nfs", false)) {
        is_nfs = true;
    }

    //uri = tstr_array_get_str_quick(params, 1);
    //bail_null(uri);

    proto = tstr_array_get_str_quick(cmd_line, 5);
    bail_null(proto);

    hostname = tstr_array_get_str_quick(params, 2);
    bail_null(hostname);

    // bug fix 995 - guard against short form entry of node name
    for(idx = 0; idx < sizeof(smap)/sizeof(char*); idx++) {
        if ( lc_is_prefix(proto, smap[idx], false) ) {
            proto = smap[idx];
            break;
        }
    }

    // Create parent node
    if(is_nfs) {
        bn_name_host = smprintf(
                "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/host", 
                ns, uri, proto);
        bail_null(bn_name_host);
        err = mdc_set_binding(cli_mcc, &ret_err, NULL, bsso_modify, bt_string, 
                hostname, bn_name_host);
        bail_error(err);
        bail_error(ret_err);
    } 
    else {
        bn_name_host = smprintf(
                "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/host/%s", 
                ns, uri, proto, hostname);
        bail_null(bn_name_host);
        err = mdc_create_binding(cli_mcc, &ret_err, NULL, 
                bt_string, hostname, bn_name_host);
        bail_error(err);
        bail_error_quiet(ret_err);
    }


    err = tstr_array_linear_search_str(cmd_line, "port", 0, &idx);
    if (err != lc_err_not_found) {
        port = tstr_array_get_str_quick(cmd_line, idx + 1);
        bail_null(port);

        if (is_nfs) {
            bn_name_port = smprintf(
                    "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/port", 
                    ns, uri, proto);
            bail_null(bn_name_port);
        }
        else {
            bn_name_port = smprintf(
                    "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/host/%s/port", 
                    ns, uri, proto, hostname);
            bail_null(bn_name_port);
        }

        err = mdc_set_binding(cli_mcc,
                &ret_err, NULL, bsso_modify, bt_uint16, 
                port, bn_name_port);
        bail_error(err);
        bail_error(ret_err);
    }

    err = tstr_array_linear_search_str(cmd_line, "keepalive", 0, &idx);
    if (err != lc_err_not_found) {
        keepalive = "true";
        
        bn_name_keepalive = smprintf(
                "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/host/%s/keep-alive", 
                ns, uri, proto, hostname);
        bail_null(bn_name_keepalive);

        err = mdc_set_binding(cli_mcc,
                NULL, NULL, bsso_modify, bt_bool, 
                keepalive, bn_name_keepalive);
        bail_error(err);
    }

    err = tstr_array_linear_search_str(cmd_line, "weight", 0, &idx);
    if (err != lc_err_not_found) {
        weight = tstr_array_get_str_quick(cmd_line, idx + 1);
        bail_null(weight);

        bn_name_weight = smprintf(
                "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/host/%s/weight", 
                ns, uri, proto, hostname);
        bail_null(bn_name_weight);

        err = mdc_set_binding(cli_mcc,
                NULL, NULL, bsso_modify, bt_uint32, 
                weight, bn_name_weight);
        bail_error(err);
    }

    err = tstr_array_linear_search_str(cmd_line, "local-cache", 0, &idx);
    if (err != lc_err_not_found) {

        if (is_nfs) {
            bn_name_local_cache = smprintf(
                    "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/local-cache", 
                    ns, uri, proto);
            bail_null(bn_name_local_cache);
        }
        else {
            bn_name_local_cache = smprintf(
                    "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/%s/host/%s/local-cache", 
                    ns, uri, proto, hostname);
            bail_null(bn_name_local_cache);
        }

        if (cmd->cc_magic == 1) {
            err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_bool,
                    "true", bn_name_local_cache);
            bail_error(err);
        }
        else if (cmd->cc_magic == 2) {
            err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_bool,
                    "false", bn_name_local_cache);
            bail_error(err);
        }
        
    }

    // if we came upto here without bailing, then err = 0
    // need to set this so that the CLI doesnt throw a 
    // useles "internal error" message on the screen.
    err = 0;

bail:
    safe_free(bn_name_host);
    safe_free(bn_name_port);
    safe_free(bn_name_weight);
    safe_free(bn_name_keepalive);
    safe_free(bn_name_local_cache);
    safe_free(uri);
    return err;
}

int 
cli_nvsd_uri_delivery_proto(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

bail:
    return err;
}
#endif

int 
cli_namespace_generic_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    char *node_name = NULL;
    tstring *ret_msg;
    uint32 ret_err = 0;

    UNUSED_ARGUMENT(data);

    if(cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
        if(cmd->cc_magic == cc_magic_delivery_rtsp)
        {
		err = mdc_send_action_with_bindings_str_va (cli_mcc, &ret_err, &ret_msg, 
				"/nkn/nvsd/namespace/actions/validate_namespace_match_header", 1, "namespace_name", bt_string,
				tstr_array_get_str_quick(params, 0));
		bail_error (err);

		if(ret_err) {
			err = cli_prefix_pop();
			bail_error(err);

			err = clish_prompt_update(false);
			bail_error(err);

			goto bail;
		}
		node_name = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/rtsp",
			tstr_array_get_str_quick(params, 0));
		bail_null(node_name);

		err = mdc_create_binding(cli_mcc, NULL, NULL, bt_bool,
			"true", node_name);
		bail_error(err);
		safe_free(node_name);
        }
        if(cmd->cc_magic == cc_magic_delivery_http)
        {
        	node_name = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/http",
			tstr_array_get_str_quick(params, 0));
		bail_null(node_name);

		err = mdc_create_binding(cli_mcc, NULL, NULL, bt_bool,
			"true", node_name);
		bail_error(err);
		safe_free(node_name);
        }

    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;
}

#if 0
int 
cli_ns_uri_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tbool valid = false;
    char *binding_name = NULL;
    char *service_policy_binding = NULL;
    tstring *ret_msg = NULL;
    char *esc_name = NULL;
    uint32_t ret_code = 0;


    err = cli_ns_uri_validate(tstr_array_get_str_quick(params, 1),
            tstr_array_get_str_quick(params, 0),
            &valid);
    //bail_error(err);
    //

    err = cli_nvsd_uri_escaped_name(tstr_array_get_str_quick(params, 1), &esc_name);
    bail_error_null(err, esc_name);


    if(!valid) {
        // Node doesnt exist.. create it

        binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s", 
                tstr_array_get_str_quick(params, 0),
                esc_name);
                //tstr_array_get_str_quick(params, 1));
        bail_null(binding_name);

        err = mdc_create_binding(cli_mcc,
                &ret_code,
                NULL,
                bt_uri,
                tstr_array_get_str_quick(params, 1),
                binding_name);
        bail_error(err);

	if (ret_code) // Create node failed 
		goto bail;

        valid = true;
    }

    if (cli_prefix_modes_is_enabled()) {
        err = cli_ns_uri_validate(tstr_array_get_str_quick(params, 1),
                tstr_array_get_str_quick(params, 0),
                &valid);
        bail_error(err);

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
    safe_free(binding_name);
    safe_free(service_policy_binding);
    safe_free(esc_name);
    return err;
}

static int 
cli_ns_uri_validate(
        const char *uri,
        const char *ns_name,
        tbool *ret_valid)
{
    int err = 0;
    tstr_array *ns = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_namespace_get_uris(&ns, false, ns_name);
    bail_error(err);

    err = tstr_array_linear_search_str(ns, uri, 0, &idx);
    if (lc_err_not_found != err) {
        bail_error(err);
        *ret_valid = true;
    }
    else {
        // This condition doesnt exist since
        //  - if name does not exist, we create it
        //  - if name exists then this is never hit
        //err = cli_printf_error(_("Unknown namespace - %s\n"), namespace);
        //bail_error(err);
    }
bail:
    tstr_array_free(&ns);
    return err;
}

int
cli_nvsd_config_uri_delivery_proto(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *proto = NULL;
    char *bn_name = NULL;
    char *bn_proto = NULL;
    char *uri = NULL;
    tstr_array *proto_words = NULL;
    uint32 i = 0; 
    uint32 num_protos = 0;

    err = ts_tokenize_str(
            tstr_array_get_str_quick(params, 2), ',', '\0', '\0', 0, &proto_words);
    bail_error(err);


    err = cli_nvsd_uri_escaped_name(tstr_array_get_str_quick(params, 1), &uri);
    bail_error_null(err, uri);

    bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/delivery/proto",
            tstr_array_get_str_quick(params, 0),
            uri);
    bail_null(bn_name);

    num_protos = tstr_array_length_quick(proto_words);

    switch(cmd->cc_magic)
    {
    case true:
        /* This case is hit when user wishes to set the protocols */
        for(i = 0; i < num_protos; ++i) {

            if ((strcmp(tstr_array_get_str_quick(proto_words, i), "rtmp") == 0)) {
                continue;
            }

            bn_proto = smprintf("%s/%s", 
                    bn_name, tstr_array_get_str_quick(proto_words, i));
            bail_null(bn_proto);

            err = mdc_set_binding(cli_mcc, NULL, NULL, 
                    bsso_modify, bt_bool, "true", bn_proto);
            bail_error(err);

            safe_free(bn_proto);
        }
        break;

    case false:
        /* This case is hit when user wishes to unset the protocols */
        for(i = 0; i < num_protos; ++i) {

            if ((strcmp(tstr_array_get_str_quick(proto_words, i), "rtmp") == 0)) {
                continue;
            }
            bn_proto = smprintf("%s/%s", 
                    bn_name, tstr_array_get_str_quick(proto_words, i));
            bail_null(bn_proto);

            err = mdc_set_binding(cli_mcc, NULL, NULL, 
                    bsso_modify, bt_bool, "false", bn_proto);
            bail_error(err);

            safe_free(bn_proto);
        }
        break;
    default:
        bail_error(lc_err_unexpected_case);
    }

bail:
    safe_free(bn_name);
    safe_free(bn_proto);
    safe_free(uri);
    tstr_array_free(&proto_words);
    return err;
    
}
#endif

int
cli_nvsd_namespace_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL;
    char *pattern = NULL;
    bn_binding_array *bindings = NULL;
    uint32 num_matches = 0;
    bn_binding *binding = NULL;
    char *bn_name = NULL;
node_name_t uf_node_name = {0};
    tstring *t_cache_inherit = NULL, *t_filter_action = NULL;
    tstring *t_domain = NULL;
    tstring *t_mode = NULL;
    tstring *t_ip = NULL;
    tstring *t_cluster_hash = NULL;
    char *ns_origin_node = NULL;
    char *node_name = NULL;
    tstring *ns_origin_type = NULL;
    uint32 num_clusters = 0;
    node_name_t nd_name = {0};
    uint32_t num_vips = 0;

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    bn_name = smprintf("/nkn/nvsd/namespace/%s", ns);
    bail_null(bn_name);

    // Check if namespace exists.
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);

    if (NULL == binding) {
	cli_printf_error(_("Invalid namespace : %s\n"), ns);
	goto bail;
    }


    err = cli_printf_query(_("Namespace: "
		"#/nkn/nvsd/namespace/%s#\n"), ns);
    bail_error(err);


    err = cli_printf_query(_("  Active: "
		"#/nkn/nvsd/namespace/%s/status/active#\n"), ns);
    bail_error(err);

    err = cli_printf_query(_("  Precedence: "
		"#/nkn/nvsd/namespace/%s/ns-precedence#\n"), ns);
    bail_error(err);

    err = cli_printf_query(_("  Policy: "
		"#/nkn/nvsd/namespace/%s/policy/file#\n"), ns);
    bail_error(err);

    err = cli_printf_query(_("  Resource-Pool: "
		"#/nkn/nvsd/namespace/%s/resource_pool#\n"), ns);
    bail_error(err);

    err = cli_printf_query(_("  AccessLog Profile: "
		"#/nkn/nvsd/namespace/%s/accesslog#\n"), ns);
    bail_error(err);

    ns_origin_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", ns);
    bail_null(ns_origin_node);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
	    &ns_origin_type, ns_origin_node, NULL);
    bail_error_null(err, ns_origin_type);

    if((atoi(ts_str(ns_origin_type))) == osvr_http_node_map)
    {
	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_cluster_hash, NULL,
		"/nkn/nvsd/namespace/%s/cluster-hash",
		ns);
	bail_error(err);

	if (t_cluster_hash && (ts_equal_str(t_cluster_hash, "0", false)))
	{
	    err = cli_printf(_("  Cluster-Hash: %s\n"), "None");
	    bail_error(err);
	}
	else if (t_cluster_hash && (ts_equal_str(t_cluster_hash, "1", false)))
	{
	    err = cli_printf(_("  Cluster-Hash: %s\n"), "Base url");
	    bail_error(err);
	}else if (t_cluster_hash && (ts_equal_str(t_cluster_hash, "2", false)))
	{
	    err = cli_printf(_("  Cluster-Hash: %s\n"), "Complete url");
	    bail_error(err);
	}
    }

    cli_printf(_("  Cluster Association: \n"));

    pattern = smprintf("/nkn/nvsd/namespace/%s/cluster/*/cl-name", ns);
    bail_null(pattern);

    err = mdc_foreach_binding(cli_mcc, pattern, NULL, cli_nvsd_ns_cluster_show,
	    NULL, &num_clusters);
    bail_error(err);

    if (num_clusters == 0)
	cli_printf(_("   NONE\n"));


    bn_name = smprintf("/nkn/nvsd/namespace/%s/cache_inherit", ns);
    bail_null(bn_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_cache_inherit, bn_name, NULL);
    bail_error(err);

    if (t_cache_inherit && !strcmp(ts_str(t_cache_inherit), CACHE_INHERIT_SET_STR))
    {
	err = cli_printf_query(_("  Cache-Inherit: "
		    "#/nkn/nvsd/namespace/%s/uid#\n"), ns);
	bail_error(err);
    }

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, 
	    &t_domain, NULL,
	    "/nkn/nvsd/namespace/%s/domain/regex",
	    ns);
    bail_error(err);

    if (t_domain && (ts_length(t_domain) > 0)) {
	err = cli_printf(_("  Domain Regex: \"%s\"\n"),
		ts_str(t_domain));
	bail_error(err);
	ts_free(&t_domain);
    } else {
	node_name_t fqdn_node = {0};
	tstr_array *fqdn_list = NULL;
	const char *fqdn_name = NULL;
	uint32_t i = 0;

	snprintf(fqdn_node, sizeof(fqdn_node),
		"/nkn/nvsd/namespace/%s/fqdn-list", ns);
	err = mdc_get_binding_children_tstr_array(cli_mcc, NULL, NULL,
		&fqdn_list, fqdn_node, NULL);
	bail_error(err);

	fqdn_name = tstr_array_get_str_quick(fqdn_list, 0);
	cli_printf("  Domain Name: %s\n", fqdn_name);

	/* iterate over all FQDNs, and print */
	for (i=1; i < tstr_array_length_quick(fqdn_list); i++) {

	    fqdn_name = tstr_array_get_str_quick(fqdn_list, i);
	    if (fqdn_name == NULL) {
		continue;
	    }
	    err = cli_printf("             : %s\n", fqdn_name);
	    bail_error(err);
	}
	tstr_array_free(&fqdn_list);
    }

    origin_server_type_t ot = osvr_none;
    err = cli_get_origin_type(ns, &ot, NULL);
    bail_error(err);

    if ( osvr_http_follow_header == ot ) {
	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/header", ns);
	bail_null(node_name);
	tstring *t_header = NULL;
	tbool t_use_client = false;

	err = mdc_get_binding_tstr(cli_mcc, 
		NULL, NULL, NULL, 
		&t_header, 
		node_name, 
		NULL);
	bail_error(err);
	if (t_header && (ts_length(t_header) > 0)) {
	    node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip", ns);
	    bail_null(node_name);
	    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
		    &t_use_client, node_name, NULL);
	    bail_error(err);
	    if( t_use_client) {
		err = cli_printf("  Proxy Mode: "
			"Transparent (Origin selection based on Host Header)\n");
		bail_error(err);
	    }else{
		err = cli_printf("  Proxy Mode: "
			"Transparent \n");
		bail_error(err);
	    }
	}
	ts_free(&t_header);
    } 
    else if ( osvr_http_dest_ip == ot ) {

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip", ns);
	bail_null(node_name);
	tbool t_use_client = false;
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
		&t_use_client, node_name, NULL);
	bail_error(err);
	if( t_use_client) {
	    err = cli_printf("  Proxy Mode: "
		    "Transparent (Origin selection based on Destination IP from user)\n");
	    bail_error(err);
	}else{
	    err = cli_printf("  Proxy Mode: "
		    "Transparent \n");
	    bail_error(err);
	}
    }
    else {
	err = cli_printf_query(_("  Proxy Mode: "
		    "#/nkn/nvsd/namespace/%s/proxy-mode#\n"), ns);
	bail_error(err);
    }

    err = cli_nvsd_namespace_delivery_proto_match_details(data, cmd, cmd_line, params, "http");
    bail_error(err);

    err = cli_nvsd_namespace_origin_server_show_config(data, cmd, cmd_line, params);
    bail_error(err);

    err = cli_nvsd_namespace_show_http_proto_config(data, cmd, cmd_line, params);
    bail_error(err);

    err = cli_nvsd_namespace_show_rtsp_proto_config(data, cmd, cmd_line, params);
    bail_error(err);

    cli_printf(_("\n"));
    err = cli_printf_query(_("  Virtual Player: "
		"#/nkn/nvsd/namespace/%s/virtual_player#\n"), ns);
    bail_error(err);

    err = cli_nvsd_ns_live_pub_point_show_config(data, cmd, cmd_line, params);
    bail_error(err);

    /*Enabling this for bug 8344*/
    err = cli_nvsd_ns_cache_pin_show_config(data, cmd, cmd_line, params); 
    bail_error(err);

    err = cli_nvsd_ns_media_cache_show_config(data, cmd, cmd_line, params);
    bail_error(err);

    err = cli_nvsd_ns_compression_show_config(data, cmd, cmd_line, params); 
    bail_error(err);

#if 0 // prestage is hidden now so disabling the show for now 
    err = cli_printf(
	    _("\n  Pre-stage FTP Configuration:\n"));
    bail_error(err);

    safe_free(pattern);
    bn_binding_array_free(&bindings);

    pattern = smprintf("/nkn/nvsd/namespace/%s/prestage/user/*", ns);
    bail_null(pattern);

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
	    &bindings, true, true, pattern);
    bail_error(err);
    err = bn_binding_array_sort(bindings);
    bail_error(err);
    err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
	    cli_nvsd_namespace_prestage_user_show_one, NULL, &num_matches);
    bail_error(err);

    safe_free(pattern);
#endif
    pattern = smprintf("/nkn/nvsd/namespace/%s/cluster/*/cl-name", ns);
    bail_null(pattern);

    cli_printf("\n");
    err = mdc_foreach_binding(cli_mcc, pattern, NULL, cli_nvsd_ns_cluster_show,
	    (void *)ns, &num_clusters);
    bail_error(err);


    cli_printf(_("  Bound IP Address: \n"));

    snprintf(nd_name, sizeof(nd_name), "/nkn/nvsd/namespace/%s/vip/*", ns);

    err = mdc_foreach_binding(cli_mcc, nd_name, NULL, cli_nvsd_ns_vip_show,
	    NULL, &num_vips);
    bail_error(err);

    if (num_vips == 0)
	    cli_printf(_("    Default(All IP's)\n"));

    cli_printf("\n");
    cli_printf_query("  Filter-Map : #/nkn/nvsd/namespace/%s/client-request/filter-map#\n", ns);
    cli_printf_query("      Action : #/nkn/nvsd/namespace/%s/client-request/filter-action#\n", ns);
    cli_printf_query("        Type : #/nkn/nvsd/namespace/%s/client-request/filter-type#\n", ns);

    snprintf(uf_node_name, sizeof(node_name_t), "/nkn/nvsd/namespace/%s/client-request/filter-action", ns);
    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_filter_action, uf_node_name, NULL);
    bail_error(err);

    if (t_filter_action && (0 == strcmp(ts_str(t_filter_action), "200"))) {
	    cli_printf_query(" 200-OK Response : #/nkn/nvsd/namespace/%s/client-request/uf/200-msg#\n", ns);
    } else if (t_filter_action && (0 == strcmp(ts_str(t_filter_action), "301"))) {
	    cli_printf_query(" 301 FQDN : #/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn#\n", ns);
	    cli_printf_query(" 301 URI : #/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri#\n", ns);
    } else if (t_filter_action && (0 == strcmp(ts_str(t_filter_action), "302"))) {
	    cli_printf_query(" 302 FQDN : #/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn#\n", ns);
	    cli_printf_query(" 302 URI : #/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri#\n", ns);
    }
    cli_printf_query("  Filter uri-size : #/nkn/nvsd/namespace/%s/client-request/filter-uri-size#\n", ns);
    cli_printf_query("      Action : #/nkn/nvsd/namespace/%s/client-request/filter-uri-size-action#\n", ns);
    snprintf(uf_node_name, sizeof(node_name_t), "/nkn/nvsd/namespace/%s/client-request/filter-uri-size-action", ns);
    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_filter_action, uf_node_name, NULL);
    bail_error(err);

    if (t_filter_action && (0 == strcmp(ts_str(t_filter_action), "200"))) {
	    cli_printf_query(" 200-OK Response : #/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg#\n", ns);
    } else if (t_filter_action && (0 == strcmp(ts_str(t_filter_action), "301"))) {
	    cli_printf_query(" 301 FQDN : #/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn#\n", ns);
	    cli_printf_query(" 301 URI : #/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri#\n", ns);
    } else if (t_filter_action && (0 == strcmp(ts_str(t_filter_action), "302"))) {
	    cli_printf_query(" 302 FQDN : #/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn#\n", ns);
	    cli_printf_query(" 302 URI : #/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri#\n", ns);
    }


#if 0
    err = cli_printf_query(
	    _("    Offline Smooth Flow Enabled: "
		"#/nkn/nvsd/origin_fetch/config/%s/offline/fetch/smooth-flow#\n"), ns);
    bail_error(err);
#endif /* 0 */


bail:
    safe_free(bn_name);
    safe_free(pattern);
    safe_free(node_name);
    bn_binding_free(&binding);
    bn_binding_array_free(&bindings);
    ts_free(&t_domain);
    ts_free(&t_mode);
    ts_free(&t_ip);
    return err;
}

int
cli_nvsd_namespace_prestage_user_show_one(
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

    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    cli_printf_prequeried(bindings, _(
                "    User: %s\n"), ts_str(value));
    return err;
}


int 
cli_nvsd_namespace_show_one(
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
    char *pattern = NULL;
    bn_binding_array *http_bindings = NULL;
    uint32 num_matches = 0;
    char *nfs_binding_name = NULL;
    char *smap_binding_name = NULL;
    char *smap_binding = NULL;
    tstring *smap_name = NULL;
    tstring *nfs_host = NULL;
    tstring *smap_host = NULL;
    bn_type ret_type = bt_none;
	uint32 num_server_maps = 0;
    tbool http = false, rtmp = false, rtsp = false, rtp = false;
    char *http_proto = NULL, 
         *rtmp_proto = NULL, 
         *rtsp_proto = NULL,
         *rtp_proto = NULL;

    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    //cli_printf(_("Name: %s\n"), ts_str(name));
    //cli_printf(_("Name: %s\n"), ts_str(value));
    //cli_printf(_("Name: %s\n"), ts_str(name_last_part));


    cli_printf(_("\n"));
    cli_printf_prequeried(bindings, _(
                "  URI Prefix: #%s# \n"), ts_str(name));


    http_proto = smprintf("%s/delivery/proto/http", ts_str(name));
    bail_null(http_proto);
    rtmp_proto = smprintf("%s/delivery/proto/rtmp", ts_str(name));
    bail_null(rtmp_proto);
    rtsp_proto = smprintf("%s/delivery/proto/rtsp", ts_str(name));
    bail_null(rtsp_proto);
    rtp_proto =  smprintf("%s/delivery/proto/rtp", ts_str(name));
    bail_null(rtp_proto);

    err = bn_binding_array_get_value_tbool_by_name(bindings, 
            http_proto, NULL, &http);
    bail_error(err);
    err = bn_binding_array_get_value_tbool_by_name(bindings, 
            rtmp_proto, NULL, &rtmp);
    bail_error(err);
    err = bn_binding_array_get_value_tbool_by_name(bindings, 
            rtsp_proto, NULL, &rtsp);
    bail_error(err);
    err = bn_binding_array_get_value_tbool_by_name(bindings, 
            rtp_proto, NULL, &rtp);
    bail_error(err);

    cli_printf(_(
                "    Delivery Protocols: %s %s %s %s\n"),
            http ? "http" : "", 
            rtmp ? "rtmp" : "", 
            rtsp ? "rtsp" : "", 
            rtp ? "rtp" : "" );

    cli_printf_prequeried(bindings, _(
                "    Domain Host: #%s/domain/host#\n"), ts_str(name));
    cli_printf_prequeried(bindings, _(
                "    Domain Regex: #%s/domain/regex#\n"), ts_str(name));
    //cli_printf_prequeried(bindings, _(
                //"     Origin-Server: \n"));

    // Grab all http origins
    pattern = smprintf("%s/origin-server/http/host/*", ts_str(name));
    bail_null(pattern);

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, &http_bindings,
            true, true, pattern);
    bail_error(err);
    err = bn_binding_array_sort(http_bindings);
    bail_error(err);

    err = mdc_foreach_binding_prequeried(http_bindings, pattern, NULL,
            cli_nvsd_origin_http_show_one, NULL, &num_matches);
    bail_error(err);

    /* Check whether we have a Server Map */
    if (num_matches == 0) {
	    smap_binding = smprintf("%s/origin-server/http/server-map", ts_str(name));
	    bail_null(smap_binding);

	    err = bn_binding_array_get_value_tstr_by_name(bindings, 
			    smap_binding, &ret_type, &smap_name);
	    bail_error(err);
	    if (ts_num_chars(smap_name) > 0) {
		    cli_printf(_("     Origin-Server Server Map : %s\n"), ts_str(smap_name)); 
	    }
	    safe_free(smap_binding);
	    ts_free(&smap_name);
    }

    nfs_binding_name = smprintf("%s/origin-server/nfs/host", ts_str(name));
    bail_null(nfs_binding_name);
    err = bn_binding_array_get_value_tstr_by_name(bindings, 
            nfs_binding_name, &ret_type, &nfs_host);
    bail_error(err);


    if (ts_num_chars(nfs_host) > 0) {
        cli_printf_prequeried(bindings, _(
                    "    Origin-Server: nfs://"
                    "#%s/origin-server/nfs/host#:#%s/origin-server/nfs/port#\n"), 
                ts_str(name), ts_str(name));
    }
    else {
	    /* Check whether we have a Server Map */
	    smap_binding = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name", ts_str(name));
		bail_null(smap_binding);

		err = mdc_foreach_binding(cli_mcc, smap_binding, NULL, cli_nvsd_http_server_map_show,
		   NULL, &num_server_maps);
		bail_error(err);
/*
	    err = bn_binding_array_get_value_tstr_by_name(bindings, 
			    smap_binding, &ret_type, &smap_name);
	    bail_error(err);
	    if (ts_num_chars(smap_name) > 0) {
		    cli_printf(_("     Origin-Server Server Map : %s\n"), ts_str(smap_name)); 
	    }
	    */
	    safe_free(smap_binding);
	    ts_free(&smap_name);
    }

#if 0
    smap_binding_name = smprintf("%s/origin-server/server-map", ts_str(name));
    bail_null(smap_binding_name);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
            smap_binding_name, &ret_type, &smap_host);
    bail_error(err);

    /* Print Server-map only if it is configured.
     */
    if (ts_num_chars(smap_host) > 0) {
        cli_printf(_(
                    "    Server-Map: %s\n"), ts_str(smap_host));
        cli_printf_query(_(
                    "      Protocol: #/nkn/nvsd/server-map/%s/protocol#\n"), ts_str(smap_host));
        cli_printf_query(_(
                    "      Map File: #/nkn/nvsd/server-map/%s/file-url#\n"), ts_str(smap_host));
        cli_printf_query(_(
                    "      Refresh Interval: #/nkn/nvsd/server-map/%s/refresh# second(s)\n"), ts_str(smap_host));
    }
#endif


bail:
    safe_free(smap_binding);
    ts_free(&smap_name);
    ts_free(&nfs_host);
    ts_free(&smap_host);
    //bn_binding_free(&nfs_binding_name);
    safe_free(nfs_binding_name);
    safe_free(smap_binding_name);
    safe_free(pattern);
    return err;
}

int 
cli_nvsd_origin_http_show_one(
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

    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    cli_printf_prequeried(bindings, _(
                "    Origin-Server: http://"
                "#%s#:#%s/port#\n"), ts_str(name), ts_str(name));
    cli_printf_prequeried(bindings, _(
                "      KeepAlive:   #%s/keep-alive#\n"), ts_str(name));
    cli_printf_prequeried(bindings, _(
                "      Weight:      #%s/weight#\n"), ts_str(name));

    return err;
}


/* 
 *	Function : uid_prefix_match ()
 *	Description : This checks if the UID has a prefix match with any of
 *		the valie UIDs. This is primarily for mid-tier and virtual
 *		case where the UID is suffixed with MD5 of the domain name
 */
tbool
uid_prefix_match (const char *t_lost_uid, tstr_array *names)
{
	int err = 0;
	uint32	idx = 0;
	uint32	num_names = 0;
	char *bn_name = NULL;
	tstring  *t_valid_uid = NULL;

	/* Sanity first */
	if (!t_lost_uid || !names) return false;

	num_names = tstr_array_length_quick(names);
	for (idx = 0; idx < num_names; idx++)
	{
		bn_name = smprintf("/nkn/nvsd/namespace/%s/uid",
	    			tstr_array_get_str_quick(names, idx));
		bail_null(bn_name);

		/* Get the UID value */
		err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_valid_uid, bn_name, NULL);
		bail_error(err);

		safe_free(bn_name);

		if (t_valid_uid == NULL)
		{
			lc_log_basic(LOG_NOTICE, _("NULL value in the UID[node]. Lost uid value: %s"), t_lost_uid);
			goto bail;
		}

		/* Now check for prefix match */
		if (!strncmp (ts_str(t_valid_uid), t_lost_uid, ts_length (t_valid_uid)))
		{
			ts_free (&t_valid_uid);
			return true;
		}

		ts_free (&t_valid_uid);
	}

bail:
	/* No match hence return false */
	return (false);

} /* end of uid_prefix_match () */

int 
cli_nvsd_namespace_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_err = 0;
    uint32 idx = 0;
    uint32 num_names = 0;
    tstr_array *names = NULL;
    tbool t_status;
    char *bn_name = NULL;
    bn_binding_array *lost_uid_array = NULL;
    uint32 binding_array_length = 0;
    tstring *t_lost_uid = NULL;
    char buff [8] = { 0 };

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_namespace_get_names(&names, true);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);

    err = cli_printf(_("Currently defined namespaces :\n"));
    bail_error(err);
    if (num_names > 0) {
        err = cli_printf_query(_
		    ("    %16s : %s\n\n"),
			    "Name", "UID");
        for(idx = 0; idx < num_names; ++idx) {
    	    bn_name = smprintf("/nkn/nvsd/namespace/%s/status/active",
	    			tstr_array_get_str_quick(names, idx));
    	    bail_null(bn_name);

    	    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &t_status, bn_name, NULL);
    	    bail_error(err);
            safe_free(bn_name);

            err = cli_printf_query(_
		    ("    %16s : #/nkn/nvsd/namespace/%s/uid# (%s)\n"),
			    tstr_array_get_str_quick(names, idx),
			    tstr_array_get_str_quick(names, idx),
			    t_status ? "active" : "inactive");
            bail_error(err);
        }
    }
    else
        err = cli_printf(_("No configured namespaces in the system\n"));

    err = cli_printf(_("-------------------------\n"));
    err = cli_printf(_("List of unmapped/deleted namespace UIDs (if any)\n"));
    bail_error(err);

    /* Now get the unmapped UIDs from the disk */
    err = mdc_send_action_with_bindings_and_results (cli_mcc, &ret_err, NULL, 
                   "/nkn/nvsd/namespace/actions/get_lost_uids", NULL, &lost_uid_array);
    bail_error (err);
     if (ret_err != 0)
    {
         cli_printf_error(_("error: failed to get lost uids from disk\n"));
	 goto bail;
    }
    err = bn_binding_array_length( lost_uid_array, &binding_array_length);
    bail_error (err);

    for ( idx = 1 ; idx <= binding_array_length ; ++idx)
    {
        sprintf(buff,"%d",idx);
        err = bn_binding_array_get_value_tstr_by_name ( lost_uid_array,
                       buff, NULL, &t_lost_uid);
        bail_error(err);

     if (!uid_prefix_match (ts_str(t_lost_uid), names))
	{
            if ( strstr(ts_str(t_lost_uid),"/mfc_probe:" )==0) {
                err = cli_printf(_(" %s \n"),ts_str(t_lost_uid));
                bail_error(err);
            }
	}
        ts_free(&t_lost_uid); 
    }
     
#if 0
    err = mdc_send_action(cli_mcc, &ret_err, 
    		NULL, "/nkn/nvsd/namespace/actions/get_lost_uids");
    bail_error(err);
#endif
    err = cli_printf(_("-------------------------\n"));


bail:
    ts_free(&t_lost_uid); 
    bn_binding_array_free(&lost_uid_array);
    safe_free(bn_name);
    tstr_array_free(&names);
    return err;
}

#if 0
int cli_nvsd_ns_add_virtual_player(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL;
    const char *name = NULL;

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    name = tstr_array_get_str_quick(params, 1);
    bail_null(name);

    bn_name = smprintf("/nkn/nvsd/namespace/config/%s/virtual-player/%s",
            ns, name);
    bail_null(bn_name);

    err = mdc_create_binding(cli_mcc, NULL, NULL,
            bt_string, name, bn_name);

    bail_error(err);

bail:
    safe_free(bn_name);
    return err;
}
#endif


/*
 * Helper function to escape a URI string
 */
static int
cli_nvsd_uri_escaped_name(
        const char *str,
        char **ret_str)
{
    int err = 0;
    tstring *tstr = NULL;
    int i = 0;
    int ch = 0;

    bail_null(str);
    bail_null(ret_str);
    *ret_str = NULL;

    err = ts_new(&tstr);
    bail_error(err);

    for(i = 0; str[i]; ++i) {
        ch = str[i];
        switch (ch) {
        case '/':
            err = ts_append_str(tstr, "\\");
            bail_error(err);
            err = ts_append_char(tstr, ch);
            bail_error(err);
            break;
        default:
            err = ts_append_char(tstr, ch);
            bail_error(err);
        }
    }

    err = ts_detach(&tstr, ret_str, NULL);
    bail_error_null(err, *ret_str);

bail:
    ts_free(&tstr);
    return err;
}

int 
cli_nvsd_no_origin_reset_one(
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
    tstring *ret_msg = NULL;
    uint32 ret_code = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    err = mdc_reset_binding(cli_mcc, &ret_code, &ret_msg, 
            ts_str(name));
    bail_error(err);

bail:
    ts_free(&ret_msg);
    return err;
}

int 
cli_nvsd_no_origin_delete_cache_age(
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

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    err = mdc_delete_binding(cli_mcc, NULL, NULL, ts_str(name));
    bail_error(err);

bail:
    return err;
}

int
cli_nvsd_no_bind_policy(
	void *data,
        const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
	int err = 0;
	const char *ns = NULL;
	const char *policy = NULL;
	char *bn_name = NULL;
	tstring *t_policy = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	policy = tstr_array_get_str_quick(params, 1);
	bail_null(policy);

	bn_name = smprintf("/nkn/nvsd/namespace/%s/policy/file",
			ns);
	bail_null(bn_name);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_policy, bn_name, NULL);
	bail_error(err);

	if((t_policy != NULL) && (strcmp(ts_str_maybe(t_policy),policy)== 0)) {
		err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_reset, bt_string, "", bn_name);
		bail_error(err);
	} else {
		cli_printf_error("Policy not associated with namespace");
	}

bail:
	safe_free(bn_name);
	ts_free(&t_policy);
	return err;
}

int
cli_nvsd_no_origin_fetch(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL;
    char *bn_name = NULL;
    char *bn_cache_age_ct = NULL;
    tstr_array *ret_names = NULL;
    bn_binding_array *ret_bindings = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    bn_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/**", ns);
    bail_null(bn_name);

    err = mdc_foreach_binding(cli_mcc, 
            bn_name, NULL, cli_nvsd_no_origin_reset_one, NULL, NULL);
    bail_error(err);

    bn_cache_age_ct = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type/**", ns);
    bail_null(bn_name);

    err = mdc_foreach_binding(cli_mcc, 
            bn_cache_age_ct, NULL, cli_nvsd_no_origin_delete_cache_age, NULL, NULL);
    bail_error(err);

bail:
    safe_free(bn_name);
    safe_free(bn_cache_age_ct);
    tstr_array_free(&ret_names);
    bn_binding_array_free(&ret_bindings);
    return err;
}


int
cli_nvsd_origin_http_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *origins = NULL;
    const char *ns_name = NULL;
    const char *ns_uri = NULL;
    char *uri = NULL;
    tstr_array *origins_config = NULL;
    tstr_array_options opts;
    char *bn_name = NULL;
    tstring *nfs_origin = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    ns_uri = tstr_array_get_str_quick(params, 1);
    bail_null(ns_uri);

    err = cli_nvsd_uri_escaped_name(ns_uri, &uri);
    bail_error_null(err, uri);

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&origins, &opts);
    bail_error(err);

    if ( strcmp(tstr_array_get_str_quick(cmd_line, 6), "http") == 0 ) {
        bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/http/host",
                ns_name, uri);
        bail_null(bn_name);
        
        err = mdc_get_binding_children_tstr_array(cli_mcc, NULL, NULL, 
                &origins_config, bn_name, NULL);
        bail_error(err);

        err = tstr_array_concat(origins, &origins_config);
        bail_error(err);
    } 
    else {
        bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/host",
                ns_name, uri);
        bail_null(bn_name);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &nfs_origin, bn_name, NULL);
        bail_error(err);

        err = tstr_array_append_str(origins, ts_str(nfs_origin));
        bail_error(err);
    }

    err = tstr_array_concat(ret_completions, &origins);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    ts_free(&nfs_origin);
    tstr_array_free(&origins);
    tstr_array_free(&origins_config);
    safe_free(bn_name);
    safe_free(uri);
    return err;
}

int
cli_nvsd_origin_http_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *uris = NULL;
    const char *uri = NULL;
    int i = 0, num_uris = 0;


    switch(help_type)
    {
    case cht_termination:
        if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context, 
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if(cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&uris, NULL);
        bail_error(err);

        err = cli_nvsd_origin_http_comp(data, cmd, cmd_line, params, 
                curr_word, uris);
        bail_error(err);

        num_uris = tstr_array_length_quick(uris);
        for(i = 0; i < num_uris; ++i) {
            uri = tstr_array_get_str_quick(uris, i);
            bail_null(uri);
            
            err = cli_add_expansion_help(context, uri, NULL);
            bail_error(err);
        }
        break;
    
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }
bail:
    tstr_array_free(&uris);
    return err;
}


int
cli_nvsd_origin_server_disable(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns_name = NULL;
    const char *ns_uri = NULL;
    char *uri = NULL;
    char *bn_name = NULL;
    char *hostname = NULL;
    char *port = NULL;
    
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    ns_uri = tstr_array_get_str_quick(params, 1);
    bail_null(ns_uri);

    err = cli_nvsd_uri_escaped_name(ns_uri, &uri);
    bail_error_null(err, uri);

    if (strcmp(tstr_array_get_str_quick(cmd_line, 6), "http") == 0) {

#if 0   // FIX PENDING
        // FIX BUG 1729
        // Extract only the hostname if the user enters a <hostname>:<port>
        // at the CLI.
        err = cli_nvsd_origin_server_get_params(tstr_array_get_str_quick(params, 2), 
                &hostname, 
                &port);
        bail_error(err);
#endif // FIX PENDING

        bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/http/host/%s",
                ns_name, uri, tstr_array_get_str_quick(params, 2));
        bail_null(bn_name);
    
        err = mdc_delete_binding(cli_mcc, NULL, NULL, bn_name);
        bail_error(err);
    }
    else {
        bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/host",
                ns_name, uri);
        bail_null(bn_name);

        err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_reset, bt_string, "", bn_name);
        bail_error(err);
    }


bail:
    safe_free(bn_name);
    safe_free(uri);
    safe_free(hostname);
    safe_free(port);
    return err;
}

int 
cli_nvsd_config_uri_delivery_proto_reset_one(
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
    tstring *ret_msg = NULL;
    uint32 ret_code = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    err = mdc_reset_binding(cli_mcc, &ret_code, &ret_msg, 
            ts_str(name));
    bail_error(err);
bail:
    return err;
}

int
cli_nvsd_config_uri_delivery_proto_reset(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL;
    char *bn_name = NULL;
    tstr_array *ret_names = NULL;
    bn_binding_array *ret_bindings = NULL;
    char *uri = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    err = cli_nvsd_uri_escaped_name(tstr_array_get_str_quick(params, 1), &uri);
    bail_error_null(err, uri);

    bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/delivery/proto/**", ns, uri);
    bail_null(bn_name);

    err = mdc_foreach_binding(cli_mcc, 
            bn_name, NULL, cli_nvsd_config_uri_delivery_proto_reset_one, NULL, NULL);
    bail_error(err);

bail:
    safe_free(bn_name);
    safe_free(uri);
    tstr_array_free(&ret_names);
    bn_binding_array_free(&ret_bindings);
    return err;
}

int
cli_namespace_cache_inherit_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    char *bn_name = NULL;
    tstring *t_uid = NULL;
    tstring *t_cache_inherit = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    /* Get the cache-inherit node value */
# if 0
    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_cache_inherit, name, NULL);
    bail_error(err);
#endif
    err = bn_binding_array_get_value_tstr_by_name(bindings, name, NULL, &t_cache_inherit);
    bail_error_null(err, t_cache_inherit);

    if (!t_cache_inherit)
    	goto bail;

    if (strncmp(ts_str(t_cache_inherit), CACHE_INHERIT_SET_STR, strlen(CACHE_INHERIT_SET_STR)))
    	goto bail;

    bn_name = smprintf("/nkn/nvsd/namespace/%s/uid",
	    		tstr_array_get_str_quick(name_parts, 3));
    bail_null(bn_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_uid, bn_name, NULL);
    bail_error(err);

    if (t_uid)
    {
        err = tstr_array_append_sprintf
            (ret_reply->crr_cmds, "namespace %s cache-inherit %s", 
	    		tstr_array_get_str_quick(name_parts, 3), ts_str(t_uid));
        bail_error(err);
    }

 bail:
    safe_free(bn_name);
    return(err);
}


/* ----------------------------------------------------------------------- */
char *
get_extended_uid (
	const char *ns_uid,
	const char *proxy_domain,
	const char *proxy_domain_port)
{
	char convert_domain[300]; // should be more than enough
	const char *p2 = NULL;
	char *p1 = NULL;
	char *ret_uid_str = NULL;
	int totlen;

	/* Sanity test */
	if (!proxy_domain) return strdup (ns_uid) ;

	if (!proxy_domain_port)
		proxy_domain_port = NS_DEFAULT_PORT;

	/* tolowercase and append port number with a colon as seperator*/
	p1 = convert_domain;
	p2 = (char *)proxy_domain;
	totlen = strlen (proxy_domain);
	while (totlen) {
		*p1 = tolower(*p2); 
		p1++; p2++; totlen--;
	}
	*p1 = ':'; // Add the port seperator color
	p1++;

	p2 = proxy_domain_port;
	totlen = strlen (proxy_domain_port);
	while (totlen) {
		*p1 = *p2; 
		p1++; p2++; totlen--;
	}
	*p1 = '\0';
	totlen = strlen (convert_domain);

	/* Append the domain:port to the uid */
	/* Allocate the return string : UID + _ + domain:port + '\0' */
	ret_uid_str = calloc (strlen (ns_uid) + 1 + strlen (convert_domain) + 1,
				sizeof (char));
	sprintf (ret_uid_str, "%s_", ns_uid);
	strncat (ret_uid_str, convert_domain, strlen(convert_domain));

	return (ret_uid_str);

} /* end of get_extended_uid () */

int
cli_ns_validity_header_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    const char *ns_name = NULL;
    const char *header = NULL;
    tstring *t_header = NULL;
    int err = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    header = tstr_array_get_str_quick(params, 1);
    bail_null(header);
   
    err = ts_new_sprintf(&t_header, "%s",header);
    bail_error_null(err,t_header);

    err = ts_trim_whitespace(t_header);
    bail_error(err);

    if(strncmp(ts_str(t_header),"",1)==0) {
        cli_printf_error("error: validity-begin-header should not be empty");
        goto bail; 
    }
    
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
            bt_string, 0, header, NULL, "/nkn/nvsd/origin_fetch/config/%s/object/validity/start_header_name", ns_name);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    ts_free(&t_header);
    return err;
}

#if 0
/* PR 789822
 * Launch object delete script from CLI,
 * Untill we device an optimum method
 */

int
cli_ns_object_delete_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    const char *ns_name = NULL;
    const char *uripattern = NULL;
    uint32 num_params = 0;
    uint32 num_cmd_params = 0;
    bn_request *req = NULL;
    const char *domain = NULL;
    const char *domain_port = NULL;
    const char *pinned = NULL;
    tbool pin = false;
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    uripattern = tstr_array_get_str_quick(cmd_line, 4);
    bail_null(uripattern);

    err = bn_action_request_msg_create(&req, "/nkn/nvsd/namespace/actions/delete_uri");
    bail_error(err);

    num_params = tstr_array_length_quick(params);
    num_cmd_params = tstr_array_length_quick(cmd_line);

    if(num_params == 1) {
        if((num_cmd_params == 6)) {
            pinned = tstr_array_get_str_quick(cmd_line,5);
            bail_null(pinned);
            if(strcmp(pinned,"pinned")==0){
                pin = true; 
            }
        }
        err = bn_action_request_msg_append_new_binding
                (req, 0, "namespace", bt_string, ns_name, NULL);
        bail_error(err);
    
        err = bn_action_request_msg_append_new_binding
                (req, 0, "uri-pattern", bt_string, uripattern, NULL);
        bail_error(err);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "pinned", bt_bool, lc_bool_to_str(pin), NULL);
        bail_error(err);
    } else if(num_params == 2) {
        err = bn_action_request_msg_append_new_binding
                (req, 0, "namespace", bt_string, ns_name, NULL);
        bail_error(err);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "uri-pattern", bt_string, uripattern, NULL);
        bail_error(err);
    } else if(num_params == 3) {
        domain = tstr_array_get_str_quick(params, 2);
        bail_null(domain);
        
        err = bn_action_request_msg_append_new_binding
                (req, 0, "namespace", bt_string, ns_name, NULL);
        bail_error(err);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "uri-pattern", bt_string, uripattern, NULL);
        bail_error(err);
 
        err = bn_action_request_msg_append_new_binding
                (req, 0, "proxy-domain", bt_string, domain, NULL);
        bail_error(err);
    } else if (num_params == 4){
        domain = tstr_array_get_str_quick(params, 2);
        bail_null(domain);
        domain_port = tstr_array_get_str_quick(params, 3);
        bail_null(domain_port);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "namespace", bt_string, ns_name, NULL);
        bail_error(err);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "uri-pattern", bt_string, uripattern, NULL);
        bail_error(err);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "proxy-domain", bt_string, domain, NULL);
        bail_error(err);

        err = bn_action_request_msg_append_new_binding
                (req, 0, "proxy-port", bt_string, domain_port, NULL);
        bail_error(err);
    } else {
        cli_printf_error("error: Invalid paramters passed");
        goto bail; 
    }
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err;
}
#endif

int
cli_ns_object_list_delete_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_disks;
    uint32 param_count = 0;
    uint32 num_cmds = 0; // CLI param count
    uint32 ret_err = 0;
    char *node_name = NULL;
    char *uri_prefix_str = NULL;
    char *uri_prefix_str_esc = NULL;
    char *ns_uid_str = NULL;
    const char *ns_cmd = NULL;
    const char *ns_name = NULL;
    const char *uri_pattern = NULL;
    const char *proxy_domain = ALL_STR;
    const char *proxy_domain_port = "*";
    tstring *ns_uid = NULL;
//    tstring *ns_domain = NULL;
    tstring *ns_proxy_mode = NULL;
    tstring *uri_prefix = NULL;
    tstr_array *cmd_params = NULL;
    en_proxy_type proxy_type = REVERSE_PROXY; 
    const char *remote_url = NULL;
    char *password = NULL;
    FILE *fp, *fp1;
    int domain_given = 0;
    lt_time_ms tv;
    char *timestamp = NULL;
    char *remote_path = NULL;
    char *remote_url_filename = NULL;
    char *remote_password_filename = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    /* First get the namespace name and the uri/all/pattern */
    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    uri_pattern = tstr_array_get_str_quick(cmd_line, 4);

    /*Initiate a rate limit check for this command */
    if(uri_pattern && strncmp(uri_pattern, "all", 3)== 0)
    {
        /* triger an action to see whether we can allow this command at this time */
    }   

    /* Get the number of command line words */
    num_cmds = tstr_array_length_quick(cmd_line);

    /* Now find if it was list or delete command */
    ns_cmd = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(ns_cmd);

    /* Get the proxy-mode of the namespace */
    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
				&ns_proxy_mode, NULL,
				"/nkn/nvsd/namespace/%s/proxy-mode",
				ns_name);

    /* Set a flag to indicate if the namespace is reverse proxy */
    if (ts_equal_str (ns_proxy_mode, "reverse", true))
    	proxy_type = REVERSE_PROXY;
    else if (ts_equal_str (ns_proxy_mode, "mid-tier", true))
    	proxy_type = MID_TIER_PROXY;
    else
    	proxy_type = VIRTUAL_PROXY;

    /* Now check if we had a domain and port in the case of mid-tier */
    if ((REVERSE_PROXY == proxy_type) && (num_cmds > 5) )
    {
    	if((safe_strcmp(ns_cmd, "list") == 0) && (0 != safe_strcmp(tstr_array_get_str_quick(cmd_line, 5),"export")))
    	{
    		cli_printf("error: domain and port should not be given for reverse proxy-mode");
    		goto bail;
    	}
    	else if(safe_strcmp(ns_cmd, "delete") == 0)
    	{
    		cli_printf("error: domain and port should not be given for reverse proxy-mode");
    		goto bail;
    	}
    }

    /* Get the uri-prefix of the namespace */
    node_name = smprintf("/nkn/nvsd/namespace/%s/match/uri/uri_name", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&uri_prefix, node_name, NULL);
    bail_error(err);
    safe_free(node_name);

    /* Check if uri-prefix has been defined */
    if (!uri_prefix) {
	cli_printf("error: no uri-prefix defined for this namespace");
	goto bail;
    }

    err = ts_detach(&uri_prefix, &uri_prefix_str, NULL);
    bail_error(err);

    /* Get the domain for the namespace */
    err = bn_binding_name_escape_str(uri_prefix_str, &uri_prefix_str_esc);
    bail_error_null(err, uri_prefix_str_esc);

    /* Get the UID for the namespace */
    node_name = smprintf("/nkn/nvsd/namespace/%s/uid", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, 
    			&ns_uid, node_name, NULL);
    bail_error_null(err, ns_uid);

    /* Check if mid-tier or virtual and domain/port are given */
    if ((num_cmds > 5) && (0 != safe_strcmp(tstr_array_get_str_quick(cmd_line, 5),"export")))
    {
        proxy_domain = tstr_array_get_str_quick(cmd_line, 5);
        bail_null(ns_name);
        domain_given = 1;
    }

    if ((num_cmds > 6) && (0 != safe_strcmp(tstr_array_get_str_quick(cmd_line, 5),"export")))
    {
	uint64_t port = 0;
        proxy_domain_port = tstr_array_get_str_quick(cmd_line, 6);
        bail_null(proxy_domain_port);

	port = strtoul(proxy_domain_port, NULL,10);

	if(port > MAX_PORT || port < 1) {
	    cli_printf_error("Port value should be between 0 and 65536");
	    goto bail;
	}
    }
    /* If domain given then get extended UID */
    if (domain_given == 1)
    	ns_uid_str = get_extended_uid (ts_str(ns_uid), proxy_domain,
					proxy_domain_port);
    else
    	ns_uid_str = strdup (ts_str(ns_uid));
    bail_error_null(err, ns_uid_str);

    if (lc_is_prefix(ns_cmd, "delete",false))
    {
    	int32 ret_offset = 0;
	tstring *t_uri_pattern = NULL;
	uint32 num_cmd_params = 0;
	const char *pinned = NULL;

	num_cmd_params = tstr_array_length_quick(cmd_line); 

	err = ts_new_str(&t_uri_pattern, uri_pattern);
	bail_error(err);

	//Check to see if ,we have a %,if so then normalize it %25.
	err = ts_find_char(t_uri_pattern, 0, '%', &ret_offset);
	bail_error(err);

	if(ret_offset != -1) {
		err = ts_insert_str(t_uri_pattern, ret_offset + 1 , "25");
		bail_error(err);
	}

	if (num_cmd_params > 5 ) {   
		pinned = tstr_array_get_str_quick(cmd_line, 5);
		bail_null(pinned);
	}

	if(pinned && (!strncmp("pinned", pinned, 6)) ) {
		err = tstr_array_new_va_str(&cmd_params, NULL, 7,
				"/opt/nkn/bin/cache_mgmt_del.py",
				ns_name, ns_uid_str, NULL, ts_str(t_uri_pattern), proxy_domain,"pin");
		param_count = 7;
	}else {
		/*NS Domain is not needed ls/del script */
		err = tstr_array_new_va_str(&cmd_params, NULL, 7,
				"/opt/nkn/bin/cache_mgmt_del.py",
				ns_name, ts_str(ns_uid), NULL, ts_str(t_uri_pattern), proxy_domain, proxy_domain_port);
		param_count = 6;
	}
    }
    else if (lc_is_prefix(ns_cmd, "list",false) )
    {

        if (0 == safe_strcmp(tstr_array_get_str_quick(cmd_line, 5),"export"))
        {
            remote_url = tstr_array_get_str_quick(cmd_line, 6);
            bail_null(remote_url);

            if ((!lc_is_prefix("scp://", remote_url, false)) &&
                    (!lc_is_prefix("sftp://", remote_url, false)) &&
                    (!lc_is_prefix("ftp://", remote_url, false))){
                cli_printf_error(_("Bad destination specifier"));
                goto bail;
            }

            err = lu_parse_scp_url(remote_url, NULL, &password, NULL, NULL, NULL);

             if(err) {
                 err = cli_printf_error("Invalid SCP URL format.\n");
                 bail_error(err);
                 goto bail;
             }

             tv = lt_curr_time();

             lt_datetime_sec_to_str(tv, false, &timestamp);
    		 *(timestamp+4)='_';
    		 *(timestamp+7)='_';
    		 *(timestamp+4)='_';
    		 *(timestamp+10)='_';

             remote_path = smprintf("%s/ns_objects_%s_%s.lst", remote_url, ns_name, timestamp);
             bail_null(remote_path);

             remote_url_filename = smprintf("/nkn/ns_objects/remote_url_%s", ns_name);
             bail_null(remote_url_filename);

             fp = fopen(remote_url_filename,"w+");
             bail_null(fp);
             fprintf(fp,"%s",remote_path);
             fclose(fp);

             if(password != NULL) {
                 remote_password_filename = smprintf("/nkn/ns_objects/remote_password_%s", ns_name);
                 bail_null(remote_password_filename);

                 fp1 = fopen(remote_password_filename,"w+");
            	 bail_null(fp1);
            	 fprintf(fp1,"%s",password);
            	 fclose(fp1);
             }
        }
        if (0 == safe_strcmp(tstr_array_get_str_quick(cmd_line, 7),"export"))
        {
            remote_url = tstr_array_get_str_quick(cmd_line, 8);
            bail_null(remote_url);

            if ((!lc_is_prefix("scp://", remote_url, false)) &&
                    (!lc_is_prefix("sftp://", remote_url, false)) &&
                    (!lc_is_prefix("ftp://", remote_url, false))){
                cli_printf_error(_("Bad destination specifier"));
                goto bail;
            }

            err = lu_parse_scp_url(remote_url, NULL, &password, NULL, NULL, NULL);

             if(err) {
                 err = cli_printf_error("Invalid SCP URL format.\n");
                 bail_error(err);
                 goto bail;
             }

             tv = lt_curr_time();

             lt_datetime_sec_to_str(tv, false, &timestamp);
    		 *(timestamp+4)='_';
    		 *(timestamp+7)='_';
    		 *(timestamp+4)='_';
    		 *(timestamp+10)='_';

             remote_path = smprintf("%s/ns_objects_%s_%s.lst", remote_url, ns_name, timestamp);
             bail_null(node_name);

             remote_url_filename = smprintf("/nkn/ns_objects/remote_url_%s", ns_name);
             bail_null(remote_url_filename);

             fp = fopen(remote_url_filename,"w+");
             bail_null(fp);
             fprintf(fp,"%s",remote_path);
             fclose(fp);

             if(password != NULL) {
                 remote_password_filename = smprintf("/nkn/ns_objects/remote_password_%s", ns_name);
                 bail_null(remote_password_filename);

                 fp1 = fopen(remote_password_filename,"w+");
            	 bail_null(fp1);
            	 fprintf(fp1,"%s",password);
            	 fclose(fp1);
             }
        }

     	err = tstr_array_new_va_str(&cmd_params, NULL, 6,
 			"/opt/nkn/bin/cache_mgmt_ls.py",
 			ns_name,uri_pattern, proxy_domain, ts_str(ns_uid), proxy_domain_port);
     	param_count = 5;
     }
    bail_error(err);

    /* Now call the function with the command */
    err = clish_paging_suspend();
    if (lc_is_prefix(ns_cmd, "delete",false))
    {
    	err = nkn_clish_run_program_fg("/opt/nkn/bin/cache_mgmt_del.py", cmd_params, NULL, NULL, NULL);
    }
    else if(lc_is_prefix(ns_cmd, "list",false))
    {
    	err = nkn_clish_run_program_fg("/opt/nkn/bin/cache_mgmt_ls.py", cmd_params, NULL, NULL, NULL);
    }

bail:
    safe_free(node_name);
    safe_free(uri_prefix_str);
    safe_free(ns_uid_str);
    safe_free(password);
    tstr_array_free(&cmd_params);
    safe_free(remote_path);
    return err;

} /* end of cli_ns_object_list_delete_cmd_hdlr() */

int
cli_ns_object_reval_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *ns_name = NULL;
	tstring *t_proxy_mode = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns_name = tstr_array_get_str_quick(params, 0);
	bail_null(ns_name);

	/* check to see if its a t-proxy namespace, We dont allow this for a t-proxy
	 * namespace */
	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
			&t_proxy_mode, NULL,
			"/nkn/nvsd/namespace/%s/proxy-mode",
			ns_name);
	bail_error_null(err, t_proxy_mode);

	if((strcmp(ts_str(t_proxy_mode), "transparent") == 0) ||
	    (strcmp(ts_str(t_proxy_mode), "virtual") == 0)) {

		cli_printf_error(_("This command is not allowed for T-Proxy namespace"));

		goto bail;
	}

	err = mdc_send_action_with_bindings_str_va (cli_mcc, &ret_err, &ret_msg, 
			"/nkn/nvsd/namespace/actions/revalidate_cache_timer", 2,
			"namespace", bt_string, ns_name,
			"type", bt_string, "all");
	bail_error (err);


bail:
	ts_free(&t_proxy_mode);
	ts_free(&ret_msg);
	return err;
}

int 
cli_ns_get_disknames(
        tstr_array **ret_labels,
        tbool hide_display)
{
    int err = 0;
    char *node_name = NULL;
    const char *t_diskid = NULL;
    tstr_array_options opts;
    tstr_array *labels_config = NULL;
    uint32 i = 0;
    tstr_array *labels = NULL;
    tbool t_cache_enabled = false;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_labels);
    *ret_labels = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&labels, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &labels_config, 
            "/nkn/nvsd/diskcache/config/disk_id", NULL);
    bail_error_null(err, labels_config);

    /* For each disk_id get the Disk Name */
    i = 0;
    t_diskid = tstr_array_get_str_quick (labels_config, i++);
    while (NULL != t_diskid)
    {
    	tstring *t_diskname = NULL;

	/* Now check if the disk is cacheable only then we are interested */
    	node_name = smprintf ("/nkn/nvsd/diskcache/config/disk_id/%s/cache_enabled", t_diskid);

    	/* Now get the cache_enabled flag value */
	err = mdc_get_binding_tbool (cli_mcc, NULL, NULL, NULL,
				&t_cache_enabled, node_name, NULL);
	bail_error(err);
	safe_free(node_name);

	/* If cache not enabled then skip this disk */
	if (!t_cache_enabled) 
	{
    		t_diskid = tstr_array_get_str_quick (labels_config, i++);
		continue;
	}

    	node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname", t_diskid);
    	/* Now get the disk_name */
	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
				&t_diskname, node_name, NULL);
	bail_error(err);
	safe_free(node_name);

    	//err = tstr_array_append_str(labels, ts_str(t_diskname));
    	err = tstr_array_insert_str_sorted(labels, ts_str(t_diskname));
    	bail_error(err);

    	t_diskid = tstr_array_get_str_quick (labels_config, i++);
    }

    *ret_labels = labels;
    labels = NULL;
bail:
    safe_free(node_name);
    tstr_array_free(&labels_config);
    return err;
}


int 
cli_nvsd_origin_fetch_cache_age_content_type(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    char *node_name = NULL;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *cache_age_indices = NULL;
    int32 db_rev = 0;
    uint32 num_cache_ages = 0;
    const char *cword = NULL;
    const char *param = NULL;

    UNUSED_ARGUMENT(data);


    node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type", 
            tstr_array_get_str_quick(params, 0));
    bail_null(node_name);

    switch(cmd->cc_magic)
    {
    case cid_nvsd_origin_fetch_cache_age_delete_one:
        lc_log_basic(LOG_WARNING, _("Content-type value:%s"), tstr_array_get_str_quick(params, 1)); 
        err = mdc_array_delete_by_value_single(cli_mcc,
                node_name,
                true, "type", bt_string, 
                tstr_array_get_str_quick(params, 1), NULL, NULL, NULL);
        bail_error(err);
        break;

    case cid_nvsd_origin_fetch_cache_age_add_one:

        /* Need to check if the type already exists.. If it does, then
         * we only need to update the Age value.
         * If binding doesnt exist, then create it and then set the age
         */

        err = bn_binding_array_new(&barr);
        bail_error(err);

        err = bn_binding_new_str_autoinval(&binding, "type", ba_value, 
                bt_string, 0, tstr_array_get_str_quick(params, 1));
        bail_error(err);

        err = bn_binding_array_append_takeover(barr, &binding);
        bail_error(err);

        /* Get all matching indices first */
        err = mdc_array_get_matching_indices(cli_mcc, 
                node_name, NULL, barr, bn_db_revision_id_none,
                NULL, &cache_age_indices, NULL, NULL);
        bail_error(err);

        num_cache_ages = uint32_array_length_quick(cache_age_indices);

        if (num_cache_ages == 0) {
            err = cli_nvsd_cache_age_entry_create(
                    node_name, db_rev, cmd_line, params, &code);
            bail_error(err);
        } 
        else {

            cword = tstr_array_get_str_quick(params, 1); // content-type
            bail_null(cword);

            param = tstr_array_get_str_quick(params, 2); // cache-age
            bail_null(param);

            err = cli_nvsd_cache_age_entry_modify(node_name,
                    uint32_array_get_quick(cache_age_indices, 0),
		    db_rev, cmd_line, cword, param);
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
    }

bail:
    safe_free(node_name);
    return err;
}

int cli_ns_host_header_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    const char *ns_name = NULL;
    const char *header = NULL;
    tstring *t_header = NULL;
    int err = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    header = tstr_array_get_str_quick(params, 1);
    bail_null(header);

    err = ts_new_sprintf(&t_header, "%s",header);
    bail_error_null(err,t_header);

    err = ts_trim_whitespace(t_header);
    bail_error(err);

    if(strncmp(ts_str(t_header),"",1)==0) {
        cli_printf("error: host-header should not be empty");
        goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
                  0, bt_string, 0, header, NULL,
          "/nkn/nvsd/origin_fetch/config/%s/host-header/header-value",
          ns_name);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    ts_free(&t_header);
    return err;
}

int cli_ns_dscp_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    const char *ns_name = NULL;
    const char *dscp = NULL;
    int err = 0;
    int dscp_value = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    bail_null(params);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    dscp = tstr_array_get_str_quick(params, 1);
    bail_null(dscp);

    dscp_value = atoi(dscp);
    /*This node will have -1 as reset value*/
    if((dscp_value < 0)||(dscp_value > 63)) {
        cli_printf_error("error: invalid dscp range.valid range is 0-63.");
        goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
                  0, bt_int32, 0, dscp, NULL,
          "/nkn/nvsd/namespace/%s/client-response/dscp",
          ns_name);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err;
}

int cli_ns_obj_checksum_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    const char *ns_name = NULL;
    const char *checksum = NULL;
    int err = 0;
    int checksum_value = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    bail_null(params);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    checksum = tstr_array_get_str_quick(params, 1);
    bail_null(checksum);

    checksum_value = atoi(checksum);
    /*This node will have 0 as reset value*/
    if((checksum_value < 1) || (checksum_value > 128000)) {
        cli_printf_error("error: invalid checksum value.valid range is 1-128000.");
        goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
                  0, bt_uint32, 0, checksum, NULL,
          "/nkn/nvsd/namespace/%s/client-response/checksum",
          ns_name);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err;
}

int 
cli_nvsd_origin_fetch_cache_age_content_type_any(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    char *node_name = NULL;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *cache_age_indices = NULL;
    int32 db_rev = 0;
    uint32 num_cache_ages = 0;
    const char *param = NULL;

    UNUSED_ARGUMENT(data);

    node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type", 
            tstr_array_get_str_quick(params, 0));
    bail_null(node_name);

    switch(cmd->cc_magic)
    {
    case cid_nvsd_origin_fetch_cache_age_delete_one:

        err = mdc_array_delete_by_value_single(cli_mcc,
                node_name,
                true, "type", bt_string, 
                "", NULL, NULL, NULL);
        bail_error(err);
        break;

    case cid_nvsd_origin_fetch_cache_age_add_one:

        /* Need to check if the type already exists.. If it does, then
         * we only need to update the Age value.
         * If binding doesnt exist, then create it and then set the age
         */

        err = bn_binding_array_new(&barr);
        bail_error(err);

        err = bn_binding_new_str_autoinval(&binding, "type", ba_value, 
                bt_string, 0, "");
        bail_error(err);

        err = bn_binding_array_append_takeover(barr, &binding);
        bail_error(err);

        /* Get all matching indices first */
        err = mdc_array_get_matching_indices(cli_mcc, 
                node_name, NULL, barr, bn_db_revision_id_none,
                NULL, &cache_age_indices, NULL, NULL);
        bail_error(err);

        num_cache_ages = uint32_array_length_quick(cache_age_indices);

        if (num_cache_ages == 0) {
            // Get timeout
            err = bn_binding_new_str_autoinval(&binding, "value", 
                    ba_value, bt_uint32, 0, 
                    tstr_array_get_str_quick(params, 1));
            bail_error_null(err, binding);
        
            err = bn_binding_array_append(barr, binding);
            bail_error(err);
        
            err = mdc_array_append(cli_mcc,
                    node_name, barr, true, 0, NULL, NULL, &code, NULL);
            bail_error(err);
        } 
        else {

            param = tstr_array_get_str_quick(params, 1); // cache-age
            bail_null(param);

            err = cli_nvsd_cache_age_entry_modify(node_name,
                    uint32_array_get_quick(cache_age_indices, 0),
		    db_rev, cmd_line, "", param);
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
    }

bail:
    safe_free(node_name);
    return err;
}

static int
cli_nvsd_cache_age_entry_create(const char *name, 
                                int32 db_dev, 
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                uint32 *ret_code)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    char *node_name = NULL;

    UNUSED_ARGUMENT(db_dev);
    UNUSED_ARGUMENT(cmd_line);

    bail_null(name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = bn_binding_new_str_autoinval(&binding, "type", 
            ba_value, bt_string, 0, 
            tstr_array_get_str_quick(params, 1));
    bail_error_null(err, binding);

    err = bn_binding_array_append(barr, binding);
    bail_error(err);

    // Set timeout
    err = bn_binding_new_str_autoinval(&binding, "value", 
            ba_value, bt_uint32, 0, 
            tstr_array_get_str_quick(params, 2));
    bail_error_null(err, binding);

    err = bn_binding_array_append(barr, binding);
    bail_error(err);

    err = mdc_array_append(cli_mcc,
            name, barr, true, 0, NULL, NULL, &code, NULL);
    bail_error(err);

bail:
    safe_free(node_name);
    bn_binding_free(&binding);
    bn_binding_array_free(&barr);
    if(ret_code) {
        *ret_code = code;
    }
    return err;
}

static int
cli_nvsd_cache_age_entry_modify(const char *name, 
                                int32 cache_age_index, 
                                int32 db_rev,
                                const tstr_array *cmd_line, 
				const char *cword,
				const char *param)
{
    int err = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(cword);

    bail_require(cache_age_index != -1);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0, 
            bt_uint32, 0, param, NULL, "%s/%d/value", name, cache_age_index);
    bail_error(err);

    err = bn_set_request_msg_set_option_cond_db_revision_id(req, db_rev);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);



bail:
    bn_request_msg_free(&req);
    return err;
}

int 
cli_nvsd_namespace_delete(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns_name = NULL;
    char *ns_cl_node = NULL;
    tstr_array *ns_clusters = NULL;
    int num_clusters = 0;
    const char *cluster_name = NULL;
    char *cl_name_node = NULL;
    tstring *t_cluster_name = NULL;
    tstring *t_rp_name = NULL;
    bn_request *req = NULL, *req_reset = NULL;
    node_name_t rp_ns_binding_nd = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);
    err = bn_set_request_msg_create(&req_reset, 0);
    bail_error(err);

/* Thilak - 05/06/2010 */
/* Commenting out as the mfc_probe doesn't have a default namespace as of now */
#if 0
    if(!strcmp(tstr_array_get_str_quick(params, 0),"mfc_probe")) {
	    cli_printf(_("Probe namespace will not allowed to be deleted"));
	    goto bail;
    }
#endif

    //Check if bound to resource-pool
    err = mdc_get_binding_tstr_fmt(cli_mcc, 
		    NULL, NULL, NULL, 
		    &t_rp_name, 
		    NULL, 
		    "/nkn/nvsd/namespace/%s/resource_pool",
		    tstr_array_get_str_quick(params, 0));
    bail_error(err);

    if(!t_rp_name)
	goto bail;

    /* Remove the association of the namespace with resource pool
     * when namespace is removed
     */
    if(!ts_equal_str(t_rp_name, "global_pool", false)) {
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_delete , 0,
			    bt_string, 0, "", NULL,
			    "/nkn/nvsd/resource_mgr/config/%s/namespace/%s", ts_str(t_rp_name), ns_name);
	    bail_error(err);
    }
    ns_cl_node = smprintf("/nkn/nvsd/namespace/%s/cluster", tstr_array_get_str_quick(params, 0));
    bail_null(ns_cl_node);
    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &ns_clusters,
            ns_cl_node, NULL);
    bail_error(err);

    num_clusters = tstr_array_length_quick (ns_clusters);


    for(int i = 1; i <= num_clusters; i++) {
	    safe_free(cl_name_node);
	    cl_name_node = smprintf("/nkn/nvsd/namespace/%s/cluster/%d/cl-name", tstr_array_get_str_quick(params, 0), i);
	    bail_null(cl_name_node);

	    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_cluster_name, cl_name_node, NULL);
	    bail_error(err);

	    cluster_name = ts_str(t_cluster_name);

	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
			    bt_string, 0, "", NULL,
			    "/nkn/nvsd/cluster/config/%s/namespace", cluster_name);
	    bail_error(err);

	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			    bt_bool, 0, "true", NULL,
			    "/nkn/nvsd/cluster/config/%s/reset", cluster_name);
	    bail_error(err);

	    /* preapre request for resetting the reset flag to false
	     * NOTE - this is is not req, this is req_reset
	     */
	    err = bn_set_request_msg_append_new_binding_fmt(req_reset, bsso_modify, 0,
			    bt_bool, 0, "false", NULL,
			    "/nkn/nvsd/cluster/config/%s/reset", cluster_name);
	    bail_error(err);
    }

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_delete , 0,
		    bt_string, 0, "", NULL,
		    "/nkn/nvsd/namespace/%s", ns_name);
    bail_error(err);

    /* XXX - we don't use err_code */
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

    /* XXX - we don't use err_code */
    /* this is message is needed to set the reset flag of cluster to false, else
       setting it again will not have any impact */
    err = mdc_send_mgmt_msg(cli_mcc, req_reset, false, NULL, NULL, NULL);
    bail_error(err);

bail:

    bn_request_msg_free(&req);
    bn_request_msg_free(&req_reset);
    safe_free(cl_name_node);
    ts_free(&t_rp_name);
    return err;
}

static int 
cli_nvsd_origin_fetch_cache_revalidation_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	tstring *t_no_revalidation = NULL;
	tstring *t_set_cookie_cache = NULL;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_set_cookie_cache,
	"/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache", namespace);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_no_revalidation,
			"/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache/no-revalidation", namespace);
	bail_error(err);


	if ((ts_equal_str(t_no_revalidation, "true", false)) && (ts_equal_str(t_set_cookie_cache, "true", false))) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s delivery protocol http origin-fetch header set-cookie any cache permit no-revalidation", namespace);
		bail_error(err);
	}
	else if ((ts_equal_str(t_no_revalidation, "false", false)) && (ts_equal_str(t_set_cookie_cache, "false", false))) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s delivery protocol http origin-fetch header set-cookie any cache deny", namespace);
		bail_error(err);
	}
	else if ((ts_equal_str(t_no_revalidation, "false", false)) && (ts_equal_str(t_set_cookie_cache, "true", false))) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s delivery protocol http origin-fetch header set-cookie any cache permit", namespace);
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
	}
bail:
    return err;
}
	
static int 
cli_nvsd_origin_fetch_cache_age_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    char *c_type = NULL;
    char *c_value = NULL;
    tstring *t_content_type = NULL;
    tstring *t_timeout = NULL;
    const char *cc_namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);


    c_type = smprintf("%s/type", name);
    bail_null(c_type);

    c_value = smprintf("%s/value", name);
    bail_null(c_value);


    cc_namespace = tstr_array_get_str_quick(name_parts, 4);
    bail_null(cc_namespace);

    err = bn_binding_array_get_value_tstr_by_name(bindings, c_type, NULL, &t_content_type);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, c_value, NULL, &t_timeout);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, c_type);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, c_value);
    bail_error(err);

    /* Handle the case of content-type-any */
    if (!strcmp (ts_str(t_content_type), ""))
    {
        err = tstr_array_append_sprintf(ret_reply->crr_cmds,
	        "namespace %s delivery protocol http origin-fetch cache-age content-type-any %s",
                cc_namespace, ts_str(t_timeout));
    }
    else
    {
        err = tstr_array_append_sprintf(ret_reply->crr_cmds,
	        "namespace %s delivery protocol http origin-fetch cache-age content-type \"%s\" %s",
                cc_namespace, ts_str(t_content_type), ts_str(t_timeout));
    }
    bail_error(err);

bail:
    safe_free(c_value);
    safe_free(c_type);

    return err;
}

int 
cli_nvsd_origin_fetch_cache_age_show_one(
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
    char *c_type = NULL;
    char *c_value = NULL;
    tstring *t_type = NULL;
    tstring *t_value = NULL;

    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    if (idx == 0) {
        cli_printf(_(
                    "    Cache-Age Per Content-Type:\n"));
        cli_printf(_(
                    "      Content-Type           Cache-Control: max-age\n"));
        cli_printf(_("      ---------------------------------------------\n"));
    }

    c_type = smprintf("%s/type", ts_str(name));
    bail_null(c_type);
    c_value = smprintf("%s/value", ts_str(name));
    bail_null(c_value);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
            c_type, NULL, &t_type);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
            c_value, NULL, &t_value);
    bail_error(err);

    /* Check for the content-typ-any case */
    if (!strcmp (ts_str(t_type), ""))
        cli_printf(_(
                "      %-15s     %8s (secs) \n"),
                "(ANY)", ts_str(t_value));
    else
        cli_printf(_(
                "      %-15s     %8s (secs) \n"),
                ts_str(t_type), ts_str(t_value));

bail:
    safe_free(c_type);
    safe_free(c_value);
    ts_free(&t_type);
    ts_free(&t_value);
    return err;
}
int cli_nvsd_origin_fetch_unsuccessful_cache_age_show_one(
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
    char *c_age = NULL;
    tstring *t_code = NULL;
    tstring *t_age = NULL;

    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    if (idx == 0) {
        cli_printf(_(
                    "    Unsuccessful Response code and cache age:\n"));
        cli_printf(_(
                    "      Response-Code           Age\n"));
        cli_printf(_("      ---------------------------------------------\n"));
    }

    c_age = smprintf("%s/age", ts_str(name));
    bail_null(c_age);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
            ts_str(name), NULL, &t_code);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
            c_age, NULL, &t_age);
    bail_error(err);

    if(t_age != NULL && t_code != NULL) {
	cli_printf(_(
	        "      %-15s     %8s (secs) \n"),
		ts_str(t_code), ts_str(t_age));
    }

bail:
    safe_free(c_age);
    ts_free(&t_code);
    ts_free(&t_age);
    return err;
}

int
cli_nvsd_namespace_http_origin_fetch_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch";
    cmd->cc_help_descr =        N_("Configure Origin fetch parameters");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_namespace_generic_enter_mode;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch";
    cmd->cc_help_descr =        N_("Reset Origin fetch parameters to defaults");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_no_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch no";
    cmd->cc_help_descr =        N_("Clear/negate origin fetch settings");
    cmd->cc_req_prefix_count =  6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch tunnel-override";
    cmd->cc_help_descr =        N_("Configure the tunnel reason code for response that MFC "
								"will try to cache");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch tunnel-override";
    cmd->cc_help_descr =        N_("Revert the Configured tunnel reason code for response that MFC "
								"will try to cache");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch tunnel-override cache-control-no-transform";
    cmd->cc_help_descr =        N_("MFC will try to cache response with Cache control set to no-transform - Tunnel_77");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/response/cc-notrans";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch tunnel-override cache-control-no-transform";
    cmd->cc_help_descr =        N_("Revert the Tunnel-override config for cache-control-no-transform");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/response/cc-notrans";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch tunnel-override object-expired";
    cmd->cc_help_descr =        N_("Cache expired objects from origin and set expiry date to current + cache_age_default (Tunnel_87)");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/response/obj-expired";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch tunnel-override object-expired";
    cmd->cc_help_descr =        N_("Revert the Tunnel-override config for object-expired");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/response/obj-expired";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-directive";
    cmd->cc_help_descr =        N_("MFC behavior for Cache-Control directives received from the origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-directive no-cache";
    cmd->cc_help_descr =        N_("Configure MFC behavior for Cache-Control directives that mark the content as non-cacheable");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-directive no-cache follow";
    cmd->cc_help_descr =        N_("Cache the content, but revalidate with origin before serving");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-directive/no-cache";
    cmd->cc_exec_value =        "follow";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;
    

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-directive no-cache override";
    cmd->cc_help_descr =        N_("Cache the content, but do not revalidate with origin before serving");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-directive/no-cache";
    cmd->cc_exec_value =        "override";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-directive no-cache tunnel";
    cmd->cc_help_descr =        N_("Tunnel the response from origin without caching in the disk");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-directive/no-cache";
    cmd->cc_exec_value =        "tunnel";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age-default";
    cmd->cc_help_descr =        N_("Configure default Cache Age value "
                                   "when the retrieved content has no cache-age");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age-default 0";
    cmd->cc_help_descr =        N_("Default cache age - 0 seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-age/default";
    cmd->cc_exec_value =        "0";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
   

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache";
    cmd->cc_help_descr =        N_("Configure object size threshold for cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache object-size";
    cmd->cc_help_descr =        N_("Configure object size threshold for cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache object-size range";
    cmd->cc_help_descr =        N_("Configure object size threshold for cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
								"cache object-size range *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Minimum object size in KB");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
								"cache object-size range * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Maximum object size in KB");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/obj-thresh-min";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/obj-thresh-max";
    cmd->cc_exec_value2 =       "$3$";
    cmd->cc_exec_type2 =        bt_uint32;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*";
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_origin_fetch_obj_thres_revmap;
    cmd->cc_revmap_suborder =	crso_origin_fetch;
    CLI_CMD_REGISTER;

    // 
    // BZ 2375 : Node .../cache-age changed to .../cache-age/default
    //
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age-default *";
    cmd->cc_help_exp_hint =     N_("number of seconds");
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-age/default";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;


    // 
    // BZ 2375 : new command
    //
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age";
    cmd->cc_help_descr =        N_("Configure Cache age for specific Content types");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age content-type";
    cmd->cc_help_descr =        N_("Configure a cache age value for a content as seen in the "
            "HTTP header Content-Type");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age content-type *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("A Content-Type header value");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/nkn/nvsd/origin_fetch/config/$1$/cache-age/content-type/*/type";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age content-type * *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Cache-Age, in seconds. MFD caches objects of this Content-Type for "
            "number of seconds specified here");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             cid_nvsd_origin_fetch_cache_age_add_one;
    cmd->cc_exec_callback =     cli_nvsd_origin_fetch_cache_age_content_type;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type/*";
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_origin_fetch_cache_age_revmap;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request "
				"cache-hit action revalidate-async";
    cmd->cc_help_descr =        N_("Serve irrespective of expiry and revalidate later");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/serve-expired/enable";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request "
				"cache-hit action revalidate-async";
    cmd->cc_help_descr =        N_("Serve irrespective of expiry and revalidate later");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/serve-expired/enable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http client-request cache-index vary-header";
    cmd->cc_help_descr =            N_("Handle Vary header, cache multiple objects based on Vary User-agent");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no namespace * delivery protocol http client-request cache-index vary-header";
    cmd->cc_help_descr =            N_("Revert the cache-index config for vary-header");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http client-request "
				    "cache-index vary-header user-agent";
    cmd->cc_help_descr =            N_("Specify regex to match user agents");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no namespace * delivery protocol http client-request "
                                    "cache-index vary-header user-agent";
    cmd->cc_help_descr =            N_("Revert the vary-header config for user-agent");
    CLI_CMD_REGISTER;

 
    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http client-request "
                                    "cache-index vary-header user-agent pc";
    cmd->cc_help_descr =            N_("Specify regex to match user agents from PC based browsers");
    CLI_CMD_REGISTER;




    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http client-request "
                                    "cache-index vary-header user-agent tablet";
    cmd->cc_help_descr =            N_("Specify regex to match user agents from tablet based browsers");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http client-request "
                                    "cache-index vary-header user-agent phone";
    cmd->cc_help_descr =            N_("Specify regex to match user agents from smartphone based browsers");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request "
                                "cache-index vary-header user-agent pc *";
    cmd->cc_help_exp =          N_("<regex>");
    cmd->cc_help_exp_hint =     N_("Specify the regex to match the required user agents in the request");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/vary-header/pc";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request "
                                "cache-index vary-header user-agent pc";
    cmd->cc_help_descr =        N_("Revert the user-agent config for pc");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/vary-header/pc";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request "
                                "cache-index vary-header user-agent tablet *";
    cmd->cc_help_exp =          N_("<regex>");
    cmd->cc_help_exp_hint =     N_("Specify the regex to match the required user agents in the request");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/vary-header/tablet";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request "
                                "cache-index vary-header user-agent tablet";
    cmd->cc_help_descr =        N_("Revert the user-agent config for tablet");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/vary-header/tablet";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;




    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request "
                                "cache-index vary-header user-agent phone *";
    cmd->cc_help_exp =          N_("<regex>");
    cmd->cc_help_exp_hint =     N_("Specify the regex to match the required user agents in the request");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/vary-header/phone";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;

 
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request "
                                "cache-index vary-header user-agent phone";
    cmd->cc_help_descr =        N_("Revert the user-agent config for phone");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/vary-header/phone";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;


    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-age";
    cmd->cc_help_descr =        N_("Reset/Clear Cache-age settings for specific Content-types");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-age content-type";
    cmd->cc_help_descr =        N_("Clear Cache-age settings for a Content-Type");
    CLI_CMD_REGISTER;
    

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-age content-type *";
    cmd->cc_help_exp =          N_("<content-type string>");
    cmd->cc_help_exp_hint =     N_("A Content-Type header value");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/nkn/nvsd/origin_fetch/config/$1$/cache-age/content-type/*/type";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             cid_nvsd_origin_fetch_cache_age_delete_one;
    cmd->cc_exec_callback =     cli_nvsd_origin_fetch_cache_age_content_type;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age content-type-any";
    cmd->cc_help_descr =        N_("Configure cache-age irrespective of the Content-Type");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-age content-type-any *";
    cmd->cc_help_exp =          N_("<age, in seconds>");
    cmd->cc_help_exp_hint =     N_("Cache-Age, in seconds.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             cid_nvsd_origin_fetch_cache_age_add_one;
    cmd->cc_exec_callback =     cli_nvsd_origin_fetch_cache_age_content_type_any;
    CLI_CMD_REGISTER;
                

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-age content-type-any";
    cmd->cc_help_descr =        N_("Clear cache-age timeout");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             cid_nvsd_origin_fetch_cache_age_delete_one;
    cmd->cc_exec_callback =     cli_nvsd_origin_fetch_cache_age_content_type_any;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "date-header";
    cmd->cc_help_descr =        N_("Configure MFD to allow modification "
                                   "or retention of the Date header retrieved "
                                   "from the origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "date-header modify";
    cmd->cc_help_descr =        N_("Configure MFD to modify the Date header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "date-header modify permit";
    cmd->cc_help_descr =        N_("Permit MFD to modify the Date header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/date-header/modify";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "date-header modify deny";
    cmd->cc_help_descr =        N_("Retain the Date header retrieved from the origin");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/date-header/modify";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;
 
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store";
    cmd->cc_help_descr =        N_("Configure content store options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store";
    cmd->cc_help_descr =        N_("Clear content store options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-fill";
    cmd->cc_help_descr =        N_("Set cache-fill options like <aggressive> or <client-driven>");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-fill";
    cmd->cc_help_descr =        N_("Negate cache-fill options");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-fill";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-fill aggressive";
    cmd->cc_help_descr =        N_("Get all the data irrespective of how much the client requested");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-fill";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_origin_fetch_cache_fill_revmap;
    cmd->cc_revmap_names =  	"/nkn/nvsd/origin_fetch/config/*/cache-fill";	
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-fill client-driven";
    cmd->cc_help_descr =        N_("Fetch only as much data as the client requested");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-fill";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-fill client-driven aggressive-threshold";
    cmd->cc_help_descr =        N_("Configure the hotness threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-fill client-driven ";
    cmd->cc_help_descr =        N_("Clear the client-driven  paramaters");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-fill client-driven aggressive-threshold";
    cmd->cc_help_descr =        N_("Clear aggressive-threshold configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-fill";
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/cache-fill/client-driven/aggressive_threshold";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-fill client-driven aggressive-threshold *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("hotness threshold number");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-fill";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/cache-fill/client-driven/aggressive_threshold";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint32;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*";
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_origin_fetch_agg_thres_revmap;
    cmd->cc_revmap_suborder =   crso_origin_fetch;   
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media";
    cmd->cc_help_descr =        N_("Configure caching options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media";
    cmd->cc_help_descr =        N_("Clear content store options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media cache-age-threshold";
    cmd->cc_help_descr =        N_("Configure Cache age threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media cache-age-threshold";
    cmd->cc_help_descr =        N_("Clear Cache age threshold");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/cache-age-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media cache-age-threshold 60";
    cmd->cc_help_descr =        N_("Default cache age threshold - 60 seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/cache-age-threshold";
    cmd->cc_exec_value =        "60";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
    

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media cache-age-threshold *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Number of seconds to store content");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/cache-age-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media uri-depth-threshold";
    cmd->cc_help_descr =        N_("Configure directory depth of URIs");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media uri-depth-threshold";
    cmd->cc_help_descr =        N_("Clear directory depth of URIs threshold");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/uri-depth-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media uri-depth-threshold *";
    cmd->cc_help_exp =          N_("<Number>");
    cmd->cc_help_exp_hint =     N_("Configure directory depth of URIs");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/uri-depth-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    //////////////////////////////////////////////////////////////////////////
    // BZ 2389
    //////////////////////////////////////////////////////////////////////////
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header";
    cmd->cc_help_descr =        N_("Configure header settings for content "
                                    "fetched from origin servers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header set-cookie";
    cmd->cc_help_descr =        N_("Configure Set-Cookie Header options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header set-cookie any";
    cmd->cc_help_descr =        N_("Header Value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header set-cookie any cache";
    cmd->cc_help_descr =        N_("Configure MFD behavior on whether to cache this header or not");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header set-cookie any cache permit";
    cmd->cc_help_descr =        N_("Allow MFD to cache this header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/header/set-cookie/cache";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/header/set-cookie/cache/no-revalidation";
    cmd->cc_exec_value2 =       "false";
    cmd->cc_exec_type2 =        bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header set-cookie any cache permit no-revalidation";
    cmd->cc_help_descr =        N_("Allow MFD to cache this header with no-revalidation");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/header/set-cookie/cache/no-revalidation";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/header/set-cookie/cache";
    cmd->cc_exec_value2 =       "true";
    cmd->cc_exec_type2 =        bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header set-cookie any cache deny";
    cmd->cc_help_descr =        N_("Disallow MFD to cache this header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/header/set-cookie/cache";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/header/set-cookie/cache/no-revalidation";
    cmd->cc_exec_value2 =       "false";
    cmd->cc_exec_type2 =        bt_bool;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*";
    cmd->cc_revmap_callback =	cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_origin_fetch_cache_revalidation_revmap;
    CLI_CMD_REGISTER;




    //////////////////////////////////////////////////////////////////////////
    // BZ 1822
    //////////////////////////////////////////////////////////////////////////
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media object-size";
    cmd->cc_help_descr =        
        N_("Configure object size threshold at which origin "
                "manager caches the object");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media object-size";
    cmd->cc_help_descr =        
        N_("Set default object size, All objects "
            "are cached to disks");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         
        "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/object-size-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media object-size *";
    cmd->cc_help_exp =          N_("<size, bytes>");
    cmd->cc_help_exp_hint =     
        N_("Objects greater than this size are cached into disks, "
                "otherwise they are not cached");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         
        "/nkn/nvsd/origin_fetch/config/$1$/content-store/media/object-size-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    //////////////////////////////////////////////////////////////////////////////////////////////
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "offline";
    cmd->cc_help_descr =        N_("Configure Offline OM (origin manager) options");
    cmd->cc_flags =		ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "offline";
    cmd->cc_help_descr =        N_("Clear/Reset Offline OM (origin manager) options");
    cmd->cc_flags =		ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "offline fetch";
    cmd->cc_help_descr =        N_("Configure Offline fetch options");
    cmd->cc_flags =		ccf_hidden;
    CLI_CMD_REGISTER;
  

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "offline fetch";
    cmd->cc_help_descr =        N_("Clear Offline fetch options");
    cmd->cc_flags =		ccf_hidden;
    CLI_CMD_REGISTER;
 
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "offline fetch file-size";
    cmd->cc_help_descr =        N_("Configure Offline fetch file size option");
    cmd->cc_flags =		ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "offline fetch file-size";
    cmd->cc_help_descr =        N_("Clear Offline fetch file size option");
    cmd->cc_flags =             ccf_terminal|ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/offline/fetch/size";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
  
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "offline fetch file-size 10240";
    cmd->cc_help_descr =        N_("Set default file-size - 10 KiB,");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/offline/fetch/size";
    cmd->cc_exec_value =        "10240";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "offline fetch file-size *";
    cmd->cc_help_exp =          N_("<KiB>");
    cmd->cc_help_exp_hint =     N_("Size of fetch");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/offline/fetch/size";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "byte-range";
    cmd->cc_help_descr = 	N_("Set byte-range options like <ignore> or <preserve>");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "byte-range preserve";
    cmd->cc_help_descr = 	N_("The byte-range request is preserved");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "byte-range ignore";
    cmd->cc_help_descr = 	N_("The received byte-range request is ignored and the entire file is fetched");
    cmd->cc_flags =             ccf_terminal|ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/byte-range/preserve";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "byte-range preserve align ";
    cmd->cc_help_descr = 	N_("To allow the request to be modified for optimization");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/byte-range/preserve";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_name2 =         "/nkn/nvsd/origin_fetch/config/$1$/byte-range/align";
    cmd->cc_exec_value2 =        "true";
    cmd->cc_exec_type2 =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "byte-range preserve no-align ";
    cmd->cc_help_descr = 	N_("To disallow the request to be modified for optimization");
    cmd->cc_flags =             ccf_terminal|ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/byte-range/preserve";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_name2 =         "/nkn/nvsd/origin_fetch/config/$1$/byte-range/align";
    cmd->cc_exec_value2 =        "false";
    cmd->cc_exec_type2 =         bt_bool;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "byte-range";
    cmd->cc_help_descr = 	N_("Reset byte-range option");
    cmd->cc_flags =             ccf_terminal|ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/byte-range/preserve";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_name2 =        "/nkn/nvsd/origin_fetch/config/$1$/byte-range/align";
    cmd->cc_exec_value2 =       "true";
    cmd->cc_exec_type2 =        bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-directive";
    cmd->cc_help_descr =        N_("Reset cache directive options to MFD default");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-directive no-cache";
    cmd->cc_help_descr =        N_("Follow cache directive received from the origin");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-directive/no-cache";
    cmd->cc_exec_value =        "follow";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-age-default";
    cmd->cc_help_descr =        N_("Set Cache age to MFD default (0 seconds)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-age/default";
    cmd->cc_exec_value =        "0";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "date-header";
    cmd->cc_help_descr =        N_("Reset Date header options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "date-header modify";
    cmd->cc_help_descr =        N_("Reset Date header options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/date-header/modify";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "packing-policy";
    cmd->cc_help_descr =        N_("Configure packing-policy options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "packing-policy";
    cmd->cc_help_descr =        N_("Negate packing-policy options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/ingest-policy";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "packing-policy lifo";
    cmd->cc_help_descr =        N_("Configure packing-policy: lifo");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/ingest-policy";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "packing-policy fifo";
    cmd->cc_help_descr =        N_("Configure packing-policy: fifo");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/ingest-policy";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * delivery protocol http origin-fetch "
				"eod-on-close";
    cmd->cc_help_descr =	N_("Configure whether partial fetch should enabled or disabled");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "eod-on-close enable";
    cmd->cc_help_descr =        N_("Allow rewriting on content length if connection is closed.");
    cmd->cc_flags =             ccf_terminal|ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/detect_eod_on_close";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "eod-on-close disable";
    cmd->cc_help_descr =        N_("Disallow rewriting of content length if connection is closed.");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/detect_eod_on_close";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * delivery protocol http origin-fetch "
				"cache-control";
    cmd->cc_help_descr = 	N_("Allow MFD to set the custom cache-control header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * delivery protocol http origin-fetch "
				"cache-control s-maxage";
    cmd->cc_help_descr = 	N_("Configure the s-maxage header usage option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * delivery protocol http origin-fetch "
				"cache-control s-maxage ignore";
    cmd->cc_help_descr = 	N_("ignore s-maxage header and use max-age header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-control/s-maxage-ignore";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * delivery protocol http origin-fetch "
				"cache-control header";
    cmd->cc_help_descr = 	N_("Allow MFD to set the custom cache-control header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-control header *";
    cmd->cc_help_exp =          N_("<header-name>");
    cmd->cc_help_exp_hint =     N_("Configure the custom Header for cache control");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-control/custom-header";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no namespace * delivery protocol http origin-fetch "
				"cache-control";
    cmd->cc_help_descr = 	N_("Allow MFD to re-set the custom cache-control header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no namespace * delivery protocol http origin-fetch "
				"cache-control s-maxage";
    cmd->cc_help_descr = 	N_("Unconfigure the s-maxage header usage option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no namespace * delivery protocol http origin-fetch "
				"cache-control s-maxage ignore";
    cmd->cc_help_descr = 	N_("Use s-maxage header");
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_capab_required =ccp_set_rstr_curr;
    cmd->cc_exec_operation =cxo_reset;
    cmd->cc_exec_name =     "/nkn/nvsd/origin_fetch/config/$1$/cache-control/s-maxage-ignore";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-control header";
    cmd->cc_help_descr =        N_("Removes the Configured custom Header for cache control");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-control/custom-header";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * delivery protocol http origin-fetch "
				"redirect-302";
    cmd->cc_help_descr =	N_("Configure behavior when a HTTP 302 response is received by MFC");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * delivery protocol http origin-fetch "
				"redirect-302 pass-through";
    cmd->cc_help_descr =	N_("Tunnel HTTP 302 (Redirect) response received from origin to clients");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/redirect_302";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =	"false";
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * delivery protocol http origin-fetch "
				"redirect-302 handle";
    cmd->cc_help_descr =	N_("Handle HTTP 302 (Redirect) response received from origin and request new origin from Location header");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/redirect_302";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =	"true";
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "header vary";
    cmd->cc_help_descr =        N_("Handle Vary header, cache multiple objects based on Vary User-agent or Accept-Encoding");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http origin-fetch "
                                    "header vary enable";
    cmd->cc_help_descr =            N_("Enable Vary header handing. (Default is to tunnel all response with Vary header.");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/origin_fetch/config/$1$/header/vary";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_namespace;
    cmd->cc_revmap_suborder =       crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "header";
    cmd->cc_help_descr =        N_("Reset header configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "header vary";
    cmd->cc_help_descr =        N_("Reset vary-header handling");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "header vary enable";
    cmd->cc_help_descr =        N_("Disable vary-header");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/header/vary";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "object";
    cmd->cc_help_descr =        N_("Specify handling policies for objects fetched from origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "object correlation";
    cmd->cc_help_descr =        N_("How various parts of the same object are correlated");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "object correlation etag";
    cmd->cc_help_descr =        N_("Policy on usage of ETag header as strong validator");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "object correlation etag ignore";
    cmd->cc_help_descr =        N_("Ignore ETag header value and use only Last Modified Header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/correlation/etag/ignore";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "object correlation validators-all";
    cmd->cc_help_descr =        N_("Check cache freshness by using all validators (such as Etag/Last Modified Header)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "object correlation validators-all ignore";
    cmd->cc_help_descr =        N_("Ignore differences in validators during cache freshness check");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/correlation/validatorsall/ignore";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_origin_fetch;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "object";
    cmd->cc_help_descr =        N_("Reset handling policies for objects fetched from origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "object correlation";
    cmd->cc_help_descr =        N_("Reset various parts of the same object are correlated");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "object correlation etag";
    cmd->cc_help_descr =        N_("Reset Policy on usage of ETag header as strong validator");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "object correlation etag ignore";
    cmd->cc_help_descr =        N_("Consider ETag header value");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/correlation/etag/ignore";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "object correlation validators-all";
    cmd->cc_help_descr =        N_("Reset Policy to Check cache freshness by using all validators (such as Etag/Last Modified Header)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "object correlation validators-all ignore";
    cmd->cc_help_descr =        N_("Compare validate-headers stored in cache to that in revalidation response from origin");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/correlation/validatorsall/ignore";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-ingest";
    cmd->cc_help_descr =        N_("Configure cache-ingest options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-ingest";
    cmd->cc_help_descr =        N_("Clear cache-ingest options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-ingest hotness-threshold";
    cmd->cc_help_descr =        N_("Configure hotness threshold value for cache-ingest");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-ingest hotness-threshold";
    cmd->cc_help_descr =        N_("Clear cache-ingest hotness-threshold");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-ingest/hotness-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-ingest hotness-threshold *";
    cmd->cc_help_exp =          N_("<Number>");
    cmd->cc_help_exp_hint =     N_("Hotness threshold value [3 - 65535]");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-ingest/hotness-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-ingest byte-range-incapable";
    cmd->cc_help_descr =        N_("Configure Byte-range incapable origin settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-ingest byte-range-incapable";
    cmd->cc_help_descr =        N_("Reset Byte-range incapable origin settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache-ingest byte-range-incapable whole-object";
    cmd->cc_help_descr =        N_("Reset full object igestion for R-Proxy and also reset skip ingestion in case of T-proxy");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-ingest/byte-range-incapable";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache-ingest byte-range-incapable whole-object";
    cmd->cc_help_descr =          N_("Ingest full object for R-Proxy and skip ingestion in case of T-Proxy");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-ingest/byte-range-incapable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media cache-tier";
    cmd->cc_help_descr =        N_("Configure content store options for cache-tier");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media cache-tier";
    cmd->cc_help_descr =        N_("Clears content store options for cache-tier");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media cache-tier highest";
    cmd->cc_help_descr =        N_("Configure content store options for cache-tier");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media cache-tier highest";
    cmd->cc_help_descr =        N_("Clears content store options for cache-tier");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "content-store media cache-tier highest object-size";
    cmd->cc_help_descr =        N_("Configure the maximum size of the object that can be "
								"directly ingested to the highest tier");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "content-store media cache-tier highest object-size";
    cmd->cc_help_descr =        N_("Clears the maximum size of the object that can be "
								"directly ingested to the highest tier");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-ingest/size-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
								"content-store media cache-tier highest object-size *";
    cmd->cc_help_exp =          N_("<bytes>");
    cmd->cc_help_exp_hint =     N_("Size threshold value");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/cache-ingest/size-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache";
    cmd->cc_help_descr =	"cache related parameters";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache responses";
    cmd->cc_help_descr =        "Forcefully cache non-2xx responses in RAM cache";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache responses";
    cmd->cc_help_descr =        "Forcefully cache non-2xx responses in RAM cache";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "namespace * delivery protocol http origin-fetch "
                                "cache responses *";
		
    cmd->cc_help_exp =              N_("<response-code>");

    cmd->cc_help_exp_hint =         N_("response code");
 //   cmd->cc_help_term =             N_("Select the non-2xx responses to be cached in RAM cache");
    cmd->cc_help_options =      cli_unsuccessful_response_opts;
    cmd->cc_help_num_options =
sizeof(cli_unsuccessful_response_opts)/sizeof(cli_expansion_option);
    cmd->cc_revmap_type =       crt_none;

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no namespace * delivery protocol http origin-fetch "
                                "cache responses *";
		
    cmd->cc_help_exp =              N_("<response-code>");

    cmd->cc_help_exp_hint =         N_("response code");

    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_pattern =          "/nkn/nvsd/origin_fetch/config/$1$/cache/response/*";
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =		    "/nkn/nvsd/origin_fetch/config/$1$/cache/response/$2$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_value =            "$2$";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache responses * age";
		
    cmd->cc_help_descr =        N_("Set the age for non-2xx responses cached in RAM cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache responses * age *";
    cmd->cc_help_exp = 		N_("<age>");
    cmd->cc_help_exp_hint = 	N_("age in seconds , min-60, max-28800");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/cache/response/$2$";
    cmd->cc_exec_value = 	"$2$";
    cmd->cc_exec_type = 	bt_uint32;
    cmd->cc_exec_name2 =	"/nkn/nvsd/origin_fetch/config/$1$/cache/response/$2$/age";
    cmd->cc_exec_value2 = 	"$3$";
    cmd->cc_exec_type2 = 	bt_uint32;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names =	"/nkn/nvsd/origin_fetch/config/*/cache/response/*/age";
    cmd->cc_revmap_cmds =       "namespace $5$ delivery protocol http origin-fetch cache responses $8$ age $v$";

    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache read-size";
    cmd->cc_help_descr =	N_("Keyword to qualify amount of data fetched from origin before serving client");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http origin-fetch "
                                "cache read-size";
    cmd->cc_help_descr =	N_("Reset Cache read-size configuration to defaults");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/fetch-page";
    cmd->cc_exec_value =	"false";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_name2 =	"/nkn/nvsd/origin_fetch/config/$1$/fetch-page-count";
    cmd->cc_exec_type2 =	bt_uint32;
    cmd->cc_exec_value =	"1"; /* the value is ignored, its reset */
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http origin-fetch "
                                "cache read-size *";
    cmd->cc_help_exp =		N_("<number>");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_help_exp_hint =	N_("Between 1 and 4, 1 unit is equivalent to 32KB");
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/fetch-page";
    cmd->cc_exec_value =	"true";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_name2 =	"/nkn/nvsd/origin_fetch/config/$1$/fetch-page-count";
    cmd->cc_exec_value2 =	"$2$";
    cmd->cc_exec_type2 =	bt_uint32;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_suborder =   crso_origin_request;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names =	"/nkn/nvsd/origin_fetch/config/*/fetch-page-count";
    cmd->cc_revmap_cmds =       "namespace $5$ delivery protocol http origin-fetch cache read-size $v$";

    CLI_CMD_REGISTER;
    err = cli_revmap_ignore_bindings_va(10
		    ,"/nkn/nvsd/origin_fetch/config/*/byte-range/preserve"
		    ,"/nkn/nvsd/origin_fetch/config/*/byte-range/align"
		    ,"/nkn/nvsd/origin_fetch/config/*/date-header/modify"
		    ,"/nkn/nvsd/origin_fetch/config/*/detect_eod_on_close"
		    ,"/nkn/nvsd/origin_fetch/config/*/offline/fetch/size"
		    ,"/nkn/nvsd/origin_fetch/config/*/ingest-policy"
		    ,"/nkn/nvsd/origin_fetch/config/*/cache-control/s-maxage-ignore"
		    ,"/nkn/nvsd/origin_fetch/config/*/cache/response/*"
		    ,"/nkn/nvsd/origin_fetch/config/*/cache-ingest/byte-range-incapable"
		    ,"/nkn/nvsd/origin_fetch/config/*/fetch-page"
		    );
    bail_error(err);

bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_http_origin_request_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request";
	cmd->cc_help_descr = 		N_("Configure request parameters when MFD fetches from an origin server");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
					"host-header";
	cmd->cc_help_descr =        N_("Configure whether to use Host HTTP header while selecting an origin");
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
                                        "host-header set";
        cmd->cc_help_descr =        N_("Set Host header to the configured string in the request to origin");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
                                        "host-header set *";
        cmd->cc_help_exp =          N_("<header-value>");
        cmd->cc_help_exp_hint =     N_("Header to be set in the request to origin");
        cmd->cc_flags =             ccf_terminal;
        cmd->cc_capab_required =    ccp_set_rstr_curr;
        cmd->cc_exec_callback =     cli_ns_host_header_cmd_hdlr;
        cmd->cc_revmap_type =       crt_manual;
        cmd->cc_revmap_order =      cro_namespace;
        cmd->cc_revmap_suborder =   crso_origin_request;
        cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =       (void*)cli_nvsd_ns_origin_request_host_header_revmap;
        cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/host-header/header-value";

        CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
					"host-header inherit";
	cmd->cc_help_descr =        N_("Retain Host header from client in the request to origin");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
					"host-header inherit incoming-req";
	cmd->cc_help_descr =        N_("Configure whether MFD should use the Host header in the incoming HTTP request");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
					"host-header inherit incoming-req permit";
	cmd->cc_help_descr =        N_("Allow MFD to use the Host HTTP header value as the origin-server to fetch the content from");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/host-header/inherit";
	cmd->cc_exec_value =        "true";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
					"host-header inherit incoming-req deny";
	cmd->cc_help_descr =        N_("Disallow MFD to use the Host HTTP header value. The origin server is selected from the namespace configuration.");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/host-header/inherit";
	cmd->cc_exec_value =        "false";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;


	// BZ-2256/2053
	//
	//
	// BZ 2328 - origin-request convert * * was disabled as fix.
	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				"convert";
	cmd->cc_help_descr =        N_("Convert specific request parameters while fetching "
				    "the object from the origin");
	cmd->cc_flags = 	ccf_unavailable;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				"convert head";
	cmd->cc_help_descr =        N_("Control if HEAD request from client should be "
				    "converted to GET toward origin or not");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				"convert head permit";
	cmd->cc_help_descr =        N_("Allow MFD to convert a HEAD request to GET request "
				    "when fetching an object from the origin. (default)");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/convert/head";
	cmd->cc_exec_value =        "true";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				"convert head deny";
	cmd->cc_help_descr =        N_("Disallow MFD to convert a HEAD request to GET request "
				    "when fetching an object from the origin");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/convert/head";
	cmd->cc_exec_value =        "false";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;


	err = cli_nvsd_namespace_http_origin_request_cache_reval_cmds_init(info, context);
	bail_error(err);

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				"cache-revalidation permit flush-caches";
	cmd->cc_help_descr =        N_("Set Cache-Control: max-age=0 in revalidation requests to flush intermediate caches");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_ns_origin_req_cache_revalidate_config;
	cmd->cc_revmap_type =       crt_none;
	//cmd->cc_revmap_names = 	"/nkn/nvsd/origin_fetch/config/*/cache-revalidate";
	//cmd->cc_revmap_callback =  	cli_nvsd_ns_origin_req_cache_revalidation_revmap;
	//cmd->cc_revmap_order =      cro_namespace;
	//cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"x-forwarded-for";
	cmd->cc_help_descr = 		N_("Configure whether MFD should set the X-Forwarded-For"
			" HTTP header in the request while fetching an object from the origin server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"x-forwarded-for enable";
	cmd->cc_help_descr = 		N_("Enable setting of X-Forwarded-For when a request is made to origin server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-request/x-forwarded-for/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type =       	crt_auto;
	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_suborder =   	crso_origin_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"x-forwarded-for disable";
	cmd->cc_help_descr = 		N_("Disable setting of X-Forwarded-For when a request is made to origin server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-request/x-forwarded-for/enable";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type =       	crt_auto;
	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_suborder =   	crso_origin_request;
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "no namespace * delivery protocol http origin-request "
                                        "host-header";
        cmd->cc_help_descr =            N_("Reset host-header settings");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "no namespace * delivery protocol http origin-request "
                                        "host-header set";
        cmd->cc_help_descr =            N_("Reset host-header in the request to origin");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_operation =        cxo_reset;
        cmd->cc_exec_name =             "/nkn/nvsd/origin_fetch/config/$1$/host-header/header-value";
        cmd->cc_revmap_type =           crt_none;
        CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"header";
	cmd->cc_help_descr = 		N_("Append headers when a request is made to the origin server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"header *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("Header Name");
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_type = 		cct_matching_values;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/namespace/$1$/origin-request/header/add/*/name";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"header * *";
	cmd->cc_help_exp = 		N_("<value>");
	cmd->cc_help_exp_hint = 	N_("Header Value");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"header * * action";
	cmd->cc_help_descr = 		N_("Define the action that MFD must take on this header");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"header * * action add";
	cmd->cc_help_descr = 		N_("Append the header to the request sent to the origin-server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_ns_origin_request_header_add;
	cmd->cc_magic = 		cc_oreq_header_add_one;
	cmd->cc_revmap_type = 		crt_manual;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/origin-request/header/add/*";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data = 	        (void*)cli_nvsd_ns_origin_request_header_revmap;
	cmd->cc_revmap_suborder =   	crso_origin_request;
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * delivery protocol http origin-request "
                                        "header type";
        cmd->cc_help_descr =            N_("Type of header");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * delivery protocol http origin-request "
                                        "header type transparent-cluster";
        cmd->cc_help_descr =            N_("Transparent-cluster header");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * delivery protocol http origin-request "
                                        "header type transparent-cluster action";
        cmd->cc_help_descr =            N_("Define the action that MFD must take on this header");
        CLI_CMD_REGISTER;

    	CLI_CMD_NEW;
    	cmd->cc_words_str = 		"namespace * delivery protocol http origin-request "
					"header type transparent-cluster action add";
    	cmd->cc_help_descr = 		N_("Append the header to the request sent to the origin-server");
    	cmd->cc_flags = 		ccf_terminal;
    	cmd->cc_capab_required = 	ccp_set_rstr_curr;
    	cmd->cc_exec_operation = 	cxo_set;
    	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-request/include-orig-ip-port";
    	cmd->cc_exec_value = 		"true";
    	cmd->cc_exec_type = 		bt_bool;
    	cmd->cc_revmap_type = 		crt_auto;
    	cmd->cc_revmap_order = 		cro_namespace;
        cmd->cc_revmap_suborder =       crso_origin_request;
    	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request connect";
	cmd->cc_help_descr =	    N_("Configure Origin connect Parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request connect timeout";
	cmd->cc_help_descr =	    N_("Configure Origin connect timeout in milliseconds");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request connect timeout *";
        cmd->cc_help_exp =          N_("<mSec>");
        cmd->cc_help_exp_hint =     N_("Origin connect timeout in mSec");
        cmd->cc_flags =             ccf_terminal;
        cmd->cc_capab_required =    ccp_set_rstr_curr;
        cmd->cc_exec_operation =    cxo_set;
        cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/origin-request/connect/timeout";
        cmd->cc_exec_value =        "$2$";
        cmd->cc_exec_type =         bt_uint32;
        cmd->cc_revmap_type =       crt_manual;
        cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
        cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data = 	    (void*)cli_nvsd_ns_origin_request_connect_timeout_revmap;
	cmd->cc_revmap_names =	    "/nkn/nvsd/namespace/*/origin-request/connect/timeout";
        CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request connect retry-delay";
	cmd->cc_help_descr =	    N_("Configure Origin connect retry delay in milliseconds");
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request connect retry-delay *";
        cmd->cc_help_exp =          N_("<mSec>");
        cmd->cc_help_exp_hint =     N_("Origin connect retry delay in mSec");
        cmd->cc_flags =             ccf_terminal;
        cmd->cc_capab_required =    ccp_set_rstr_curr;
        cmd->cc_exec_operation =    cxo_set;
        cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/origin-request/connect/retry-delay";
        cmd->cc_exec_value =        "$2$";
        cmd->cc_exec_type =         bt_uint32;
        cmd->cc_revmap_type =       crt_manual;
        cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
        cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data = 	    (void*)cli_nvsd_ns_origin_request_connect_retry_delay_revmap;
	cmd->cc_revmap_names =	    "/nkn/nvsd/namespace/*/origin-request/connect/retry-delay";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request read";
	cmd->cc_help_descr =	    N_("Configure Origin read Parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request read retry-delay";
	cmd->cc_help_descr =        N_("Configure Origin read retry delay in milliseconds");
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request read retry-delay *";
        cmd->cc_help_exp =          N_("<mSec>");
        cmd->cc_help_exp_hint =     N_("Origin read retry delay in mSec");
	cmd->cc_flags =             ccf_terminal;
        cmd->cc_capab_required =    ccp_set_rstr_curr;
        cmd->cc_exec_operation =    cxo_set;
        cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/origin-request/read/retry-delay";
        cmd->cc_exec_value =        "$2$";
        cmd->cc_exec_type =         bt_uint32;
        cmd->cc_revmap_type =       crt_manual;
        cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
        cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data = 	    (void*)cli_nvsd_ns_origin_request_read_retry_delay_revmap;
	cmd->cc_revmap_names =	    "/nkn/nvsd/namespace/*/origin-request/read/retry-delay";
        CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request read interval-timeout";
	cmd->cc_help_descr =	    N_("Configure Origin read timeout in milliseconds");
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =         "namespace * delivery protocol http origin-request read interval-timeout *";
        cmd->cc_help_exp =          N_("<mSec>");
        cmd->cc_help_exp_hint =     N_("Origin read interval timeout in mSec");
        cmd->cc_flags =             ccf_terminal;
        cmd->cc_capab_required =    ccp_set_rstr_curr;
        cmd->cc_exec_operation =    cxo_set;
        cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/origin-request/read/interval-timeout";
        cmd->cc_exec_value =        "$2$";
        cmd->cc_exec_type =         bt_uint32;
        cmd->cc_revmap_type =       crt_manual;
        cmd->cc_revmap_order =      cro_namespace;
        cmd->cc_revmap_suborder =   crso_origin_request;
        cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data = 	    (void*)cli_nvsd_ns_origin_request_read_timeout_revmap;
	cmd->cc_revmap_names =	    "/nkn/nvsd/namespace/*/origin-request/read/interval-timeout";
        CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http origin-request";
	cmd->cc_help_descr = 		N_("Disable or Reset origin request parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http origin-request "
					"header";
	cmd->cc_help_descr = 		N_("Remove a header from the configured list");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http origin-request "
					"header *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("");
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_type = 		cct_matching_values;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/namespace/$1$/origin-request/header/add/*/name";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_oreq_header_delete_one;
	cmd->cc_exec_callback = 	cli_nvsd_ns_origin_request_header_add;
	cmd->cc_revmap_type = 		crt_none;
	CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "no namespace * delivery protocol http origin-request "
                                        "header type";
        cmd->cc_help_descr =            N_("Type of header");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "no namespace * delivery protocol http origin-request "
                                        "header type transparent-cluster";
        cmd->cc_help_descr =            N_("Transparent-cluster header");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_operation =        cxo_reset;
        cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/origin-request/include-orig-ip-port";
        cmd->cc_revmap_type =           crt_none;
        CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(1
                    ,"nkn/nvsd/namespace/*/origin-request/include-orig-ip-port"
                    );
    bail_error(err);

bail:
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_http_client_request_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request";
	cmd->cc_help_descr = 		N_("Configure client request parameters");
	CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request tunnel-override";
    cmd->cc_help_descr =        N_("Configure the tunnel reason code for request that MFC "
                                                                "will try to cache");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request tunnel-override";
    cmd->cc_help_descr =        N_("Revert the Configured tunnel reason code for Request that MFC "
                                                                "will try to cache");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request tunnel-override auth-header";
    cmd->cc_help_descr =        N_("MFC will try to cache request containing AUTH header - Tunnel_14");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/auth-header";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request tunnel-override auth-header";
    cmd->cc_help_descr =        N_("Revert the Tunnel-override config for AUTH header");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/auth-header";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request tunnel-override cookie";
    cmd->cc_help_descr =        N_("MFC will try to cache request containing COOKIE header - Tunnel_15");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/cookie-header";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request tunnel-override cookie";
    cmd->cc_help_descr =        N_("Revert the Tunnel-override config for COOKIE header");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/cookie-header";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request tunnel-override query-string";
    cmd->cc_help_descr =        N_("MFC will try to cache request URI containing query string - Tunnel_16");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/query-string";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request tunnel-override query-string";
    cmd->cc_help_descr =        N_("Revert the Tunnel-override config for query-string");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length |  ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/query-string";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request tunnel-override cache-control";
    cmd->cc_help_descr =        N_("MFC will try to cache requests containing either or both "
                                                                "cache-control: no-cache , PRAGMA: no-cache - Tunnel_17 - Tunnel_18");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/cache-control";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request tunnel-override cache-control";
    cmd->cc_help_descr =        N_("Revert the Tunnel-override config for cache-control");
    cmd->cc_flags =             ccf_terminal |  ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/tunnel-override/request/cache-control";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index";
    cmd->cc_help_descr =        N_("Configure cache-index parameters");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index";
    cmd->cc_help_descr =        N_("Revert the Configured cache-index parameters");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match";
    cmd->cc_help_descr =        N_("Configure an expression to match on the request URL");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index url-match";
    cmd->cc_help_descr =        N_("Revert the Configured expression to match on the request URL");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_magic = 		cc_magic_ns_dyn_uri_all_delete;
    cmd->cc_exec_callback =	cli_nvsd_ns_dynamic_uri_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match *";
    cmd->cc_help_exp = 		N_("<regex>");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index url-match *";
    cmd->cc_help_exp =          N_("<regex>");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match * map-to";
    cmd->cc_help_descr =        N_("Configure mapping string when a match is found");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match * map-to *";
    cmd->cc_help_exp = 		N_("<map-string>");
    cmd->cc_flags = 		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_magic =		cc_magic_ns_dyn_uri_regex_add;
    cmd->cc_exec_callback =	cli_nvsd_ns_dynamic_uri_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index url-match * map-to";
    cmd->cc_help_descr =        N_("Revert the configured mapping string");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_magic =		cc_magic_ns_dyn_uri_str_delete;
    cmd->cc_exec_callback =	cli_nvsd_ns_dynamic_uri_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index url-match * map-to *";
    cmd->cc_help_exp =        	N_("<map-string>");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match * map-to * tokenize";
    cmd->cc_help_descr =        N_("Enable tokens to keywords mapping");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match * map-to * tokenize *";
	cmd->cc_help_exp = 			N_("<Up to 9 mappings in (<keyword>, <token>) format, Ex: (http.req.custom.range_start, $2)>");
	cmd->cc_flags = 			ccf_terminal | ccf_ignore_length;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_ns_dyn_uri_regex_tokenize_add;
	cmd->cc_exec_callback =		cli_nvsd_ns_dynamic_uri_exec_cb;
	CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index url-match * map-to * tokenize";
    cmd->cc_help_descr =        	N_("<Disables tokens to keywords mapping>");
	cmd->cc_flags = 			ccf_terminal | ccf_ignore_length;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_ns_dyn_uri_regex_tokenize_delete;
	cmd->cc_exec_callback =		cli_nvsd_ns_dynamic_uri_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request cache-index url-match * map-to * no-match-tunnel";
    cmd->cc_help_descr =	N_("Configure to tunnel the request when no regex match found");
    cmd->cc_flags = 		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_magic	=	cc_magic_ns_dyn_uri_all_add;
    cmd->cc_exec_callback =	cli_nvsd_ns_dynamic_uri_exec_cb;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_client_request;
    cmd->cc_revmap_names =      "/nkn/nvsd/namespace/*";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_ns_dynamic_uri_revmap;
    cmd->cc_revmap_type =       crt_manual;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request cache-index url-match * map-to * no-match-tunnel";
    cmd->cc_help_descr =	N_("Revert the Configuration of tunnel the request when no regex match found");
    cmd->cc_flags = 		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_magic =		cc_magic_ns_dyn_uri_tun_unset;
    cmd->cc_exec_callback =	cli_nvsd_ns_dynamic_uri_exec_cb;
    CLI_CMD_REGISTER;

	CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request seek-uri";
    cmd->cc_help_descr =        N_("Specify URL mapping to handle seek requests by client player");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request seek-uri";
    cmd->cc_help_descr =        N_("Revert URL mapping to handle seek requests by client player");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request seek-uri url-match";
    cmd->cc_help_descr =        N_("Configure an expression to match on the request URL");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request seek-uri url-match";
    cmd->cc_help_descr =        N_("Revert the Configured expression to match on the request URL");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
   	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_magic = 			cc_magic_ns_seek_uri_all_delete;
	cmd->cc_exec_callback =		cli_nvsd_ns_seek_uri_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request seek-uri url-match *";
	cmd->cc_help_exp = 			N_("<regex>");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request seek-uri url-match *";
    cmd->cc_help_exp =          N_("<regex>");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request seek-uri url-match * map-to";
    cmd->cc_help_descr =        N_("Configure mapping string when a match is found");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request seek-uri url-match * map-to *";
	cmd->cc_help_exp = 			N_("<map-string>");
	cmd->cc_flags = 			ccf_terminal | ccf_ignore_length;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_ns_seek_uri_regex_add;
	cmd->cc_exec_callback =		cli_nvsd_ns_seek_uri_exec_cb;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_client_request;
    cmd->cc_revmap_names =      "/nkn/nvsd/namespace/*";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data = 	(void*)cli_nvsd_ns_seek_uri_revmap;
    cmd->cc_revmap_type =       crt_manual;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request seek-uri url-match * map-to";
    cmd->cc_help_descr =        N_("Revert the configured mapping string");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_ns_seek_uri_str_delete;
	cmd->cc_exec_callback =		cli_nvsd_ns_seek_uri_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"query-string";
	cmd->cc_help_descr = 		N_("Configure parameters for objects with a query string");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"query-string action";
	cmd->cc_help_descr = 		N_("Configure parameters for objects with a query string");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"query-string action no-cache";
	cmd->cc_help_descr = 		N_("Disallow MFD to cache objects with a query string");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/origin_fetch/config/$1$/query/cache/enable";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/origin_fetch/config/$1$/query/strip";
	cmd->cc_exec_value2 = 		"false";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_manual;
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_client_request_revmap;
	cmd->cc_revmap_names = 		"/nkn/nvsd/origin_fetch/config/*";
	cmd->cc_revmap_suborder = 	crso_client_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"query-string action cache";
	cmd->cc_help_descr = 		N_("Allow MFD to cache objects along with the query string");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/origin_fetch/config/$1$/query/cache/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/origin_fetch/config/$1$/query/strip";
	cmd->cc_exec_value2 = 		"false";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_type =		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"query-string action cache exclude-query-string";
	cmd->cc_help_descr = 		N_("Strip the query string parameters before caching the object");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/origin_fetch/config/$1$/query/cache/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/origin_fetch/config/$1$/query/strip";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_type =		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cookie";
	cmd->cc_help_descr = 		N_("Configure actions when Cookie header is seen in the request");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cookie action";
	cmd->cc_help_descr = 		N_("Configure actions when Cookie header is seen in the request");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cookie action cache";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the response content even if Cookie header "
					   "is seen in the request");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/origin_fetch/config/$1$/cookie/cache/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type =		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cookie action no-cache";
	cmd->cc_help_descr = 		N_("Disallow MFD to cache the response content if Cookie header "
					   "is seen in the request");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/origin_fetch/config/$1$/cookie/cache/enable";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_manual;
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_client_request_cookie_revmap;
	cmd->cc_revmap_names = 		"/nkn/nvsd/origin_fetch/config/*";
	cmd->cc_revmap_suborder = 	crso_client_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cache-control";
	cmd->cc_help_descr = 		N_("Configure actions when cache-control header is seen in the request");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cache-control max-age";
	cmd->cc_help_descr = 		N_("Allow MFD to set the  max-age value");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
	                        	"cache-control max-age *" ;
	cmd->cc_help_exp =              N_("<number>");
	cmd->cc_help_exp_hint =             N_("Configure max-age for client request");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cache-control max-age * action"; 
	cmd->cc_help_descr = 		N_("Set the action for max-age directive");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-request "
					"cache-control max-age * action serve-from-origin";
	cmd->cc_help_descr  = 		N_("Configure max-age for client request with action to serve-from-origin");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name =             "/nkn/nvsd/origin_fetch/config/$1$/client-request/cache-control/max_age";
	cmd->cc_exec_value =		"$2$";
	cmd->cc_exec_type =             bt_uint32;
	cmd->cc_exec_name2 =		"/nkn/nvsd/origin_fetch/config/$1$/client-request/cache-control/action";
	cmd->cc_exec_value2 = 		"serve-from-origin";
	cmd->cc_exec_type2 = 		bt_string;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_manual;
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_cache_control_revmap;
	cmd->cc_revmap_names = 		"/nkn/nvsd/origin_fetch/config/*";
	cmd->cc_revmap_suborder = 	crso_client_request;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =             "namespace * delivery protocol http client-request "
					"cache-hit";
	cmd->cc_help_descr =            N_("Client request is matched to an object in cache");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =       	"namespace * delivery protocol http client-request "
					"cache-hit action";
	cmd->cc_help_descr =        	N_("Action to be taken upon finding hit in the cache");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         	"namespace * delivery protocol http client-request "
					"cache-hit action revalidate-always";
	cmd->cc_help_descr =        	N_("Serve if not expired and then revalidate, else revalidate and serve");
	cmd->cc_help_term =             N_("Revalidate objects after serving to clients [default]");
	cmd->cc_flags =             	ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_callback =     	cli_nvsd_origin_fetch_reval_always;
	cmd->cc_magic =             	cc_clreq_reval_set | cc_clreq_reval_offline;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         	"namespace * delivery protocol http client-request "
					"cache-hit action revalidate-always inline";
	cmd->cc_help_descr =        	N_("Revalidate objects before serving to clients");
	cmd->cc_flags =             	ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_callback =     	cli_nvsd_origin_fetch_reval_always;
	cmd->cc_magic =             	cc_clreq_reval_set | cc_clreq_reval_inline;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 	    	"no namespace * delivery protocol http client-request ";
	cmd->cc_help_descr =        	N_("Reset/Disable client side request paramaters");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 	    	"no namespace * delivery protocol http client-request "
				    	"cache-hit";
	cmd->cc_help_descr = 	    	N_("Reset the client-request parameters for cache-hit");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	    	"no namespace * delivery protocol http client-request "
				    	"cache-hit action";
	cmd->cc_help_descr = 	    	N_("Reset the client-request parameters for cache-hit");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	    	"no namespace * delivery protocol http client-request "
				    	"cache-hit action revalidate-always";
	cmd->cc_help_descr = 	    	N_("Reset the behavior of revalidating all client requests");
	cmd->cc_flags =             	ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_callback =     	cli_nvsd_origin_fetch_reval_always; 
	cmd->cc_magic =             	cc_clreq_reval_unset;
	cmd->cc_revmap_type =		crt_manual;
	cmd->cc_revmap_order = 		cro_namespace;
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_client_request_reval_always_revmap;
	cmd->cc_revmap_names = 		"/nkn/nvsd/origin_fetch/config/*";
	cmd->cc_revmap_suborder = 	crso_client_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	    	"namespace * delivery protocol http client-request "
					"tunnel-all";
	cmd->cc_help_descr = 	    	N_("Enable tunneling of all requests to origin, bypassing any cache lookups.");
	cmd->cc_flags =             	ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/client-request/tunnel-all";
	cmd->cc_exec_value =		"true";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	//cmd->cc_revmap_callback = 	cli_nvsd_ns_client_request_revmap;
	//cmd->cc_revmap_names = 		"/nkn/nvsd/origin_fetch/config/*";
	cmd->cc_revmap_suborder = 	crso_client_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	    	"no namespace * delivery protocol http client-request "
					"tunnel-all";
	cmd->cc_help_descr = 	    	N_("Disable tunneling all requests to origin.");
	cmd->cc_flags =             	ccf_terminal;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/client-request/tunnel-all";
	cmd->cc_exec_value =		"false";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_revmap_type =		crt_none;
	CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request method";
    cmd->cc_help_descr =        N_("Configure the client-request method parameters");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request method";
    cmd->cc_help_descr =        N_("Clears the client-request method parameters");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request method connect";
    cmd->cc_help_descr =        N_("Configure the client-request method connect parameters");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request method connect";
    cmd->cc_help_descr =        N_("Clears the client-request method connect parameters");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol http client-request method connect action";
    cmd->cc_help_descr =        N_("Configure the action on connect method on client request");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol http client-request method connect action";
    cmd->cc_help_descr =        N_("Clears the action on connect method on client request");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * delivery protocol http client-request method connect action handle";
    cmd->cc_help_descr =        N_("Handle the connect method on client request");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/client-request/method/connect/handle";
    cmd->cc_exec_value =	"true";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_client_request;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no namespace * delivery protocol http client-request method connect action handle";
    cmd->cc_help_descr =        N_("Clears the Handle config for connect method");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/client-request/method/connect/handle";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head";
    cmd->cc_help_descr =        N_("Parameter to specify the method. HEAD is the only accepted method now");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head cache-miss";
    cmd->cc_help_descr =        N_("Specifies condition (object doesn't exist or expired in cache) when client request is processed");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head cache-miss action";
    cmd->cc_help_descr =        N_("Specifies the action to be taken by MFC when handling certain request from client");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head cache-miss action tunnel";
    cmd->cc_help_descr =        N_("Tunnel the client request to origin");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/client-request/method/head/cache-miss/action/tunnel";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_name2 =        "/nkn/nvsd/namespace/$1$/client-request/method/head/cache-miss/action/convert-to/method/get";
    cmd->cc_exec_type2 =        bt_bool;
    cmd->cc_exec_value2 =       "false";
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_none;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head cache-miss action convert-to";
    cmd->cc_help_descr =        N_("Directive for conversion of request from client");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head cache-miss action convert-to method";
    cmd->cc_help_descr =        N_("Keyword that qualifies HTTP request sent by MFC to origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "method head cache-miss action convert-to method get";
    cmd->cc_help_descr =        N_("HTTP request method to be used toward origin from MFC");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/client-request/method/head/cache-miss/action/convert-to/method/get";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_name2 =        "/nkn/nvsd/namespace/$1$/client-request/method/head/cache-miss/action/tunnel";
    cmd->cc_exec_type2 =        bt_bool;
    cmd->cc_exec_value2 =       "false";
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_none;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =        NS_HTTP_CLIENT_REQ "header";
    cmd->cc_help_descr =       N_("Keyword to specify http header name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =        NS_HTTP_CLIENT_REQ "header *";
    cmd->cc_help_exp =         N_("<name>");
    cmd->cc_help_exp_hint =    N_("Specify http header name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =        NS_HTTP_CLIENT_REQ "header * action";
    cmd->cc_help_descr =       N_("Specify the action to be taken when serving content to client");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =        NS_HTTP_CLIENT_REQ "header * action accept";
    cmd->cc_help_descr =       N_("Validate the http header parameters when serving content to client");
    cmd->cc_flags =            ccf_terminal;
    cmd->cc_capab_required =   ccp_set_rstr_curr;
    cmd->cc_exec_callback =    cli_nvsd_ns_client_request_header_accept_hdlr;
    cmd->cc_revmap_order =     cro_namespace;
    cmd->cc_magic =            cc_clreq_header_accept_add_one;
    cmd->cc_revmap_names =     "/nkn/nvsd/namespace/*/client-request/header/accept/*";
    cmd->cc_revmap_callback =  cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =      (void*)cli_nvsd_ns_client_request_header_accept_revmap;
    cmd->cc_revmap_suborder = 	crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =        NS_HTTP_CLIENT_REQ "header * action ignore";
    cmd->cc_help_descr =       N_("Do not consider http header when serving content to client");
    cmd->cc_flags =            ccf_terminal;
    cmd->cc_capab_required =   ccp_set_rstr_curr;
    cmd->cc_exec_callback =    cli_nvsd_ns_client_request_header_accept_hdlr;
    cmd->cc_revmap_order =     cro_namespace;
    cmd->cc_magic =            cc_clreq_header_accept_delete_one;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "dns";
    cmd->cc_help_descr =        N_("Keyword to specify conditions and actions when DNS resolution of Host: header is involved");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "dns mismatch";
    cmd->cc_help_descr =        N_("Used with only \"follow header host\" for origin-server; directive when dest IP as seen by client is not in the list returned by DNS");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "dns mismatch action";
    cmd->cc_help_descr =        N_("Keyword to specify action in the event of a mismatch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         NS_HTTP_CLIENT_REQ "dns mismatch action tunnel";
    cmd->cc_help_descr =        N_("Tunnel the request without caching if client's destination IP isn't in the list returned by DNS");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/client-request/dns/mismatch/action/tunnel";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " NS_HTTP_CLIENT_REQ "dns";
    cmd->cc_help_descr =        N_("Keyword to specify conditions and actions when DNS resolution of Host: header is involved");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " NS_HTTP_CLIENT_REQ "dns mismatch";
    cmd->cc_help_descr =        N_("Used with only \"follow header host\" for origin-server; directive when dest IP as seen by client is not in the list returned by DNS");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no " NS_HTTP_CLIENT_REQ "dns mismatch action";
    cmd->cc_help_descr =        N_("Keyword to specify action in the event of a mismatch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " NS_HTTP_CLIENT_REQ "dns mismatch action tunnel";
    cmd->cc_help_descr =        N_("Clears the action tunneling if client's destination IP isn't in the list returned by DNS");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/client-request/dns/mismatch/action/tunnel";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service";
    cmd->cc_help_descr =        N_("Enable GEO lookup service to translate source IP in client request to GEO data");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service maxmind";
    cmd->cc_help_descr =        N_("Specify the database for GeoIP lookups (MaxMind)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/service";
    cmd->cc_exec_value =	"maxmind";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 =	"/nkn/nvsd/namespace/$1$/client-request/geo-service/api-url";
    cmd->cc_exec_value2 =	"";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_geo_service_revmap;
    cmd->cc_revmap_suborder = 	crso_client_request_geo;
    cmd->cc_revmap_names =      "/nkn/nvsd/namespace/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service lookup-timeout";
    cmd->cc_help_descr =        N_("Set lookup timeout value in milliseconds");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service lookup-timeout *";
    cmd->cc_help_exp = 		N_("<value>");
    cmd->cc_help_exp_hint = 	N_("Timeout value in milliseconds, default 5000");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/lookup-timeout";
    cmd->cc_exec_value =	"$2$";
    cmd->cc_exec_type =		bt_uint32;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_client_request_geo;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service quova";
    cmd->cc_help_descr =        N_("Specify the database for GeoIP lookups (Quova)");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service quova api-url";
    cmd->cc_help_descr =        N_("Specify URL for sending request to Quova server");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service quova api-url *";
    cmd->cc_help_exp = 		N_("<url>");
    cmd->cc_help_exp_hint = 	N_("Full URL (Ex. http://<host>/<version> where <host> is .api.quova.com. for hosted service \n and .<local_server>/GeoDirectoryServer. for local service. <version> is .v1.)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/service";
    cmd->cc_exec_value =	"quova";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 =	"/nkn/nvsd/namespace/$1$/client-request/geo-service/api-url";
    cmd->cc_exec_value2 =	"$2$";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_revmap_type =	crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service failover-mode";
    cmd->cc_help_descr =        N_("Set action to be taken when the server is down");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "geo-service failover-mode bypass";
    cmd->cc_help_descr =        N_("Bypass  requests when there is GeoIP lookup failure, (default is to reject the request)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/failover-mode";
    cmd->cc_exec_value =	"true";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_client_request_geo;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no";
    cmd->cc_help_descr =        N_("Disable GEO lookup service to translate source IP in client request to GEO data");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service";
    cmd->cc_help_descr =        N_("Disable GEO lookup service to translate source IP in client request to GEO data");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service failover-mode";
    cmd->cc_help_descr =        N_("Reset the action to be taken when the server is down");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service failover-mode bypass";
    cmd->cc_help_descr =        N_("Don't bypass the requests when there is GeoIP lookup failure");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/failover-mode";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_type =	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service maxmind";
    cmd->cc_help_descr =        N_("Reset the database option set for GeoIP lookups (MaxMind)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/service";
    cmd->cc_exec_type =		bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service quova";
    cmd->cc_help_descr =        N_("Reset the database option set for GeoIP lookups (Quova)");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service quova api-url";
    cmd->cc_help_descr =        N_("Specify URL for sending request to Quova server");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service quova api-url *";
    cmd->cc_help_exp = 		N_("<url>");
    cmd->cc_help_exp_hint = 	N_("Full URL (Ex. http://<host>/<version> where <host> is .api.quova.com. for hosted service \n and .<local_server>/GeoDirectoryServer. for local service. <version> is .v1.)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/service";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_name2 =	"/nkn/nvsd/namespace/$1$/client-request/geo-service/api-url";
    cmd->cc_exec_type2 =	bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "no geo-service lookup-timeout";
    cmd->cc_help_descr =        N_("Reset the timeout for geo-service lookup");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/geo-service/lookup-timeout";
    cmd->cc_exec_type =		bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "secure";
    cmd->cc_help_descr =        N_("Enable the secure delivery (HTTPS) for client");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/secure";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_CLIENT_REQ "plain-text";
    cmd->cc_help_descr =        N_("Enable the HTTP delivery for client");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/plain-text";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_client_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " NS_HTTP_CLIENT_REQ "secure";
    cmd->cc_help_descr =        N_("Disable the secure delivery (HTTPS) for client");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/secure";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_type =	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no " NS_HTTP_CLIENT_REQ "plain-text";
    cmd->cc_help_descr =        N_("Disable non-secure delivery for client");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/plain-text";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =        "false";
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_client_request;
    CLI_CMD_REGISTER;

	/* Origin Fetch SSL support */
    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_ORIGIN_REQ "secure";
    cmd->cc_help_descr =        N_("Enable the secure fetch (HTTPS) from origin");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/origin-request/secure";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_origin_request;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	NS_HTTP_ORIGIN_REQ "plain-text";
    cmd->cc_help_descr =        N_("Enable the HTTP fetch from origin");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name = 	"/nkn/nvsd/namespace/$1$/origin-request/secure";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_exec_value =        "false";
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_suborder = 	crso_origin_request;
    CLI_CMD_REGISTER;


    err = cli_revmap_ignore_bindings_va(10,
		"/nkn/nvsd/origin_fetch/config/*/client-request/tunnel-all",
		"/nkn/nvsd/origin_fetch/config/*/client-request/method/connect/handle",
		"/nkn/nvsd/namespace/*/client-request/geo-service/failover-mode",
		"/nkn/nvsd/namespace/*/client-request/geo-service/lookup-timeout",
		"/nkn/nvsd/namespace/*/client-request/secure",
		"/nkn/nvsd/namespace/*/client-request/plain-text",
		"/nkn/nvsd/namespace/*/origin-request/secure",
		"/nkn/nvsd/namespace/*/client-request/method/head/cache-miss/action/tunnel",
		"/nkn/nvsd/namespace/*/client-request/method/head/cache-miss/action/convert-to/method/get",
		"/nkn/nvsd/namespace/*/client-request/dns/mismatch/action/tunnel");
    bail_error(err);

bail:
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_http_client_response_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response";
	cmd->cc_help_descr = 		N_("Configure Client side response parameters");
	cmd->cc_flags =             	ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
	cmd->cc_capab_required =    	ccp_set_rstr_curr;
	cmd->cc_exec_callback =     	cli_namespace_generic_enter_mode;
	cmd->cc_revmap_type = 		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http client-response";
	cmd->cc_help_descr = 		N_("Reset/Disable client side response paramaters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response no";
	cmd->cc_help_descr = 		N_("Reset/Disable client side response paramaters");
	cmd->cc_req_prefix_count =  	3;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header";
	cmd->cc_help_descr = 		N_("Configure response header properties");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("A header name");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header * *";
	cmd->cc_help_exp = 		N_("<value>");
	cmd->cc_help_exp_hint = 	N_("A header value");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header * * action";
	cmd->cc_help_descr = 		N_("Specify an add or delete action for this header");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header * * action add";
	cmd->cc_help_descr = 		N_("Add this header name/value to the client response");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_ns_client_response_header_add;
	cmd->cc_magic = 		cc_clreq_header_add_one;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_manual;
	cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/client-response/header/add/*";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_client_response_header_add_revmap;
	cmd->cc_revmap_suborder = 	crso_client_response;
	CLI_CMD_REGISTER;


	/*-------------------------------------------------------------*/
        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * delivery protocol http client-response "
                                        "dscp";
        cmd->cc_help_descr =            N_("Set DSCP value in the response sent to client");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * delivery protocol http client-response "
                                        "dscp *";
        cmd->cc_help_exp=               N_("<number>");
        cmd->cc_help_exp_hint =         N_("DSCP value (min: 0; max: 63)");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_callback =         cli_ns_dscp_cmd_hdlr;
        cmd->cc_revmap_order =          cro_namespace;
        cmd->cc_revmap_type =           crt_manual;
        cmd->cc_revmap_suborder =       crso_client_response;
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =           (void*)cli_nvsd_ns_client_response_dscp_revmap;
        cmd->cc_revmap_names =          "/nkn/nvsd/namespace/*/client-response/dscp"; 
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "no namespace * delivery protocol http client-response "
                                        "dscp";
        cmd->cc_help_descr =            N_("Disable the feature. Set the value to -1.");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_operation =        cxo_reset;
        cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/client-response/dscp";
        cmd->cc_revmap_type =           crt_none;
        CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http client-response "
					"header";
	cmd->cc_help_descr = 		N_("Clear response header properties");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http client-response "
					"header *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("A header name");
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_type = 		cct_matching_values;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/namespace/$1$/client-response/header/add/*/name";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http client-response "
					"header * action";
	cmd->cc_help_descr = 		N_("Specify an add header or delete header action for this header");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http client-response "
					"header * action add";
	cmd->cc_help_descr = 		N_("Remove this header from the list of headers to be added to the client response");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_ns_client_response_header_add;
	cmd->cc_magic = 		cc_clreq_header_delete_one;
	CLI_CMD_REGISTER;
	/*-------------------------------------------------------------*/


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header * action";
	cmd->cc_help_descr = 		N_("Specify an add or delete action for this header");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http client-response "
					"header * action delete";
	cmd->cc_help_descr = 		N_("Delete a header from the client response");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_client_response_header_delete;
	cmd->cc_magic = 		cc_clreq_header_add_one;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_manual;
	cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/client-response/header/delete/*";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_client_response_header_delete_revmap;
	cmd->cc_revmap_suborder = 	crso_client_response;
	CLI_CMD_REGISTER;

	/*-------------------------------------------------------------*/

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http client-response "
					"header * action delete";
	cmd->cc_help_descr = 		N_("Delete a header from the client response");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_client_response_header_delete;
	cmd->cc_magic = 		cc_clreq_header_delete_one;
	CLI_CMD_REGISTER;
	/*-------------------------------------------------------------*/

bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_domain_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * domain";
    cmd->cc_help_descr = 		N_(
	    "Configure a domain name or a regular expression that "
	    "will select this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * domain *";
    cmd->cc_help_exp =		N_("<FQDN>");
    cmd->cc_help_exp_hint =		N_("Fully qualified domain name to match");
    cmd->cc_help_term =		N_("Add FQDN to domain match list");
    cmd->cc_flags =			ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_comp_callback =     cli_ns_fqdn_completion;
    cmd->cc_exec_callback =		cli_config_domain_fqdn;
    cmd->cc_magic =			ns_domain_fqdn_add;
    cmd->cc_revmap_order =		cro_namespace;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_suborder =       crso_domain;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/fqdn-list/*";
    cmd->cc_revmap_callback =		cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =		cli_nvsd_domain_name_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"no namespace * domain";
    cmd->cc_help_descr =		"delete domain FQDN";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"no namespace * domain *";
    cmd->cc_help_exp =		N_("<FQDN>");
    cmd->cc_help_exp_hint =		N_("Fully qualified domain name to match");
    cmd->cc_help_term =		N_("Delete FQDN to domain match list");
    cmd->cc_flags =			ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_comp_callback =     cli_ns_fqdn_completion;
    cmd->cc_exec_callback		= cli_config_domain_fqdn;
    cmd->cc_magic			= ns_domain_fqdn_delete;
    cmd->cc_revmap_order =		cro_namespace;
    cmd->cc_revmap_type =		crt_none;
    cmd->cc_revmap_suborder =       crso_domain;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * domain any";
    cmd->cc_help_descr = 		N_("Configure namespace to use any domain name");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback		= cli_config_domain_fqdn;
    cmd->cc_magic			= ns_domain_fqdn_add;
    cmd->cc_revmap_order		= cro_namespace;
    cmd->cc_revmap_type		= crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * domain regex";
    cmd->cc_help_descr = 		N_(
	    "Configure a regular expression to match against the "
	    "Host header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * domain regex *";
    cmd->cc_help_exp = 		N_("<regular expression>");
    cmd->cc_help_exp_hint =		N_("A regex to match the incoming Host header value.");
    cmd->cc_help_term = 		N_("Configure namespace to apply a regex "
	    "while matching the domain in the Host header field");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback		= cli_config_domain_fqdn;
    cmd->cc_magic			= ns_domain_regex;
    cmd->cc_revmap_order =		cro_namespace;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_callback =		cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =		cli_nvsd_domain_name_revmap;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/domain/regex";
    cmd->cc_revmap_suborder =	crso_domain_regex;
    CLI_CMD_REGISTER;

    const char *ignore_bindings[] = {
	"/nkn/nvsd/namespace/*/domain/host",
	"/nkn/nvsd/namespace/mfc_probe/fqdn-list/127.0.0.2",
    };

    err = cli_revmap_ignore_bindings_arr(ignore_bindings);
    bail_error(err);

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_match_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match";
    cmd->cc_help_descr = 		N_(
	    "Configure a match criteria for this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri";
    cmd->cc_help_descr = 		N_(
	    "Configure a URI prefix match for this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header";
    cmd->cc_help_descr = 		N_(
	    "Configure a header field match for this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string";
    cmd->cc_help_descr = 		N_(
	    "Configure a query-string name/value match criteria for this namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host";
    cmd->cc_help_descr = 		N_(
	    "Configure this namespace to use a virtual-host match criteria");
    CLI_CMD_REGISTER;


    err = cli_nvsd_namespace_http_match_uri_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_match_header_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_match_query_string_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_http_match_vhost_cmds_init(info, context);
    bail_error(err);
bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_http_match_uri_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    /*------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * match uri *";
    cmd->cc_help_exp =		N_("<uri-prefix>");
    cmd->cc_help_exp_hint =	N_("A URI prefix to match");
    cmd->cc_help_term =		N_("Configure namespace to match this URI prefix");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/match/uri/uri_name";
    cmd->cc_exec_value =	"$2$";
    cmd->cc_exec_type =		bt_uri;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_names =	"/nkn/nvsd/namespace/*/match/uri/uri_name";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =	(void*)cli_nvsd_uri_name_revmap;
    cmd->cc_revmap_suborder =	crso_match;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this "
	    "match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_help_term = 		N_(
	    "Configure namespace to match this "
	    "URI prefix with the given precedence number");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/uri/uri_name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_uri;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/precedence";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_uint32;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/uri/uri_name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_uri_name_revmap;
    cmd->cc_revmap_suborder = 	crso_match;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri regex";
    cmd->cc_help_descr = 		N_(
	    "Configure a regular expression to "
	    "match for the uri prefix");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri regex *";
    cmd->cc_help_exp = 		N_("<regex>");
    cmd->cc_help_exp_hint = 	N_("");
    cmd->cc_help_term = 		N_(
	    "Configure this namespace to use "
	    "a regular expression while matching "
	    "the URI prefix");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/match/uri/regex";
    cmd->cc_exec_value =	"$2$";
    cmd->cc_exec_type =		bt_regex;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_names =	"/nkn/nvsd/namespace/*/match/uri/regex";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =	(void*)cli_nvsd_uri_regex_revmap;
    cmd->cc_revmap_suborder =	crso_match;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri regex * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for "
	    "this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match uri regex * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_help_term = 		N_(
	    "Configure namespace to use a "
	    "regular expression for URI prefix "
	    "matching with an additional precedence "
	    "number for matching");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/uri/regex";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_regex;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/precedence";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_uint32;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/uri/regex";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_uri_regex_revmap;
    cmd->cc_revmap_suborder = 	crso_match;
    CLI_CMD_REGISTER;
#endif


bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_http_match_header_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    /*------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header *"; 
    cmd->cc_help_exp = 		N_("<name>");
    cmd->cc_help_exp_hint = 	N_("Header Name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header * *"; 
    cmd->cc_help_exp = 		N_("<value>");
    cmd->cc_help_exp_hint = 	N_("Header Value");
    cmd->cc_help_term = 		N_("Configure namespace to match this Header name and value");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/header/name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/header/value";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_string;
    cmd->cc_revmap_type =       	crt_none;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/header/name";
    cmd->cc_revmap_order =      	cro_namespace;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header * any";
    cmd->cc_help_descr = 		N_("Match any value");
    cmd->cc_help_term = 		N_("Configure namespace to match this Header name and value");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/header/name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/header/value";
    cmd->cc_exec_value2 = 		"*";
    cmd->cc_exec_type2 = 		bt_string;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/header/name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_header_name_revmap;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header * * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header * any precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header * * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_callback = 	cli_nvsd_namespace_match_header_config;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/header/name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_header_name_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header * any precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_callback = 	cli_nvsd_namespace_match_header_config;
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header regex";
    cmd->cc_help_descr = 		N_(
	    "Configure a regular expression to match for the header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header regex *";
    cmd->cc_help_exp = 		N_("<regex>");
    cmd->cc_help_exp_hint = 	N_("");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/header/regex";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_regex;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/header/regex";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_header_regex_revmap;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header regex * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match header regex * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/http/match/header/regex";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_regex;
    cmd->cc_exec_name2 =            "/nkn/nvsd/namespace/$1$/http/match/precedence";
    cmd->cc_exec_value2 =           "$3$";
    cmd->cc_exec_type2 =            bt_uint32;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/header/regex";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_header_regex_revmap;
    CLI_CMD_REGISTER;
    /*------------------------------------------------------------------*/
#endif

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_http_match_query_string_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);


    /*------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string *";
    cmd->cc_help_exp = 		N_("<name>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string * *";
    cmd->cc_help_exp = 		N_("<value>");
    cmd->cc_help_exp_hint = 	N_("A query string parameter value");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/query-string/name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/query-string/value";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_string;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/query-string/name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_query_string_name_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string * any";
    cmd->cc_help_descr = 		N_("Match any value");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/query-string/name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/query-string/value";
    cmd->cc_exec_value2 = 		"*";
    cmd->cc_exec_type2 = 		bt_string;
    cmd->cc_revmap_type =       	crt_none;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string * * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string * any precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string * * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_callback =		cli_nvsd_namespace_match_query_string_config;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/query-string/name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_query_string_name_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string * any precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_callback =		cli_nvsd_namespace_match_query_string_config;
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string regex";
    cmd->cc_help_descr = 		N_(
	    "Configure a regular expression to match for the header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string regex *";
    cmd->cc_help_exp = 		N_("<regex>");
    cmd->cc_help_exp_hint = 	N_("");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/query-string/regex";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_regex;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/query-string/regex";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_query_string_regex_revmap;
    cmd->cc_revmap_type =       	crt_manual;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string regex * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match query-string regex * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/query-string/regex";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_regex;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/precedence";
    cmd->cc_exec_value = 		"$3$";
    cmd->cc_exec_type = 		bt_uint32;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_match;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/query-string/regex";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_query_string_regex_revmap;
    CLI_CMD_REGISTER;
    /*------------------------------------------------------------------*/
#endif

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_http_match_vhost_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    /*------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host *";
    cmd->cc_help_exp = 		N_("<IPv4 address>");
    cmd->cc_help_exp_hint = 	N_("An IP address");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/virtual-host/ip";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_ipv4addr;
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/virtual-host/ip";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_ipv4addr;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/precedence";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_uint32;
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host * *";
    cmd->cc_help_exp = 		N_("<port number>");
    cmd->cc_help_exp_hint = 	N_("A Port number");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/match/virtual-host/ip";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_ipv4addr;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/match/virtual-host/port";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_uint16;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/virtual-host/ip";
    cmd->cc_revmap_callback =		cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =		(void*)cli_nvsd_vhost_name_port_revmap;
    cmd->cc_revmap_suborder =		crso_match;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host * * precedence";
    cmd->cc_help_descr = 		N_(
	    "Configure a precedence value for this match criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * match virtual-host * * precedence *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_callback = 	cli_nvsd_namespace_match_vhost_config;
    cmd->cc_revmap_type =       	crt_manual;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/match/virtual-host/ip";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_vhost_name_port_revmap;
    cmd->cc_revmap_suborder = 	crso_match;
    CLI_CMD_REGISTER;
    /*------------------------------------------------------------------*/
#endif


bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_origin_server_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    /*------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server";
    cmd->cc_help_descr =
	N_("Configure origin server to contact when MFD is unable to serve the object from cache.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http";
    cmd->cc_help_descr = 		N_("Configure a HTTP origin server");
    CLI_CMD_REGISTER;

    err = cli_nvsd_namespace_origin_server_http_cmds_init(info, context);
    bail_error(err);


    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server nfs";
    cmd->cc_help_descr = 		N_("Configure a NFS origin");
    CLI_CMD_REGISTER;

    err = cli_nvsd_namespace_origin_server_nfs_cmds_init(info, context);
    bail_error(err);

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server rtsp";
    cmd->cc_help_descr = 		N_("Configure a Real-time stream origin");
    CLI_CMD_REGISTER;

    err = cli_nvsd_namespace_origin_server_rtsp_cmds_init(info, context);
    bail_error(err);

    /*------------------------------------------------------------------*/

bail:
    return err;
}


/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_origin_server_http_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http absolute-url";
    cmd->cc_help_descr = 		N_("Use the Absolute URL in the client request to derive the origin server address");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/absolute-url";
    cmd->cc_exec_value = 		"true";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/origin-server/http/absolute-url";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_http_abs_url_revmap;
    cmd->cc_revmap_suborder = 	crso_origin_server;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http follow";
    cmd->cc_help_descr = 		N_("Use a Header field in the client request to derive the origin server address");
    CLI_CMD_REGISTER;

    err = cli_nvsd_namespace_origin_server_http_follow_cmds_init(info, context);
    bail_error(err);

    err = cli_nvsd_namespace_origin_server_http_ipversion_cmds_init(info, context);
    bail_error(err);


    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http *";
    cmd->cc_help_exp = 		N_("<FQDN>");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/host/name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/http/host/port";
    cmd->cc_exec_value2 = 		"80";
    cmd->cc_exec_type2 = 		bt_uint16;
    cmd->cc_exec_callback  =	cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http idle-timeout";
    cmd->cc_help_descr = 		N_("Origin Server Idle Time out");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http idle-timeout *";
    cmd->cc_help_exp = 		N_("<idle-timeout>");
    cmd->cc_help_exp_hint = 	N_("Idle Time Out");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/idle-timeout";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_namespace;
    cmd->cc_revmap_suborder =       crso_origin_server;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * aws";
    cmd->cc_help_descr =	N_("Keyword to specify optional parameters for accessing AWS based S3");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * aws access-key";
    cmd->cc_help_descr =	N_("Keyword to specify the access key provides by AWS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * aws access-key *";
    cmd->cc_help_exp =		N_("<key>");
    cmd->cc_help_exp_hint =	N_("Specify the access key provided by AWS");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * aws access-key * secret-key";
    cmd->cc_help_descr =	N_("Keyword to specify the secret key provided by AWS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * aws access-key * secret-key *";
    cmd->cc_help_exp =		N_("<key>");
    cmd->cc_help_exp_hint =	N_("Specify the secret key provided by AWS");
    cmd->cc_flags =		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_exec_callback  =	cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http * dns-query";
    cmd->cc_help_descr =        N_("Keyword preceding the specification of an identification string in DNS query");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * dns-query *";
    cmd->cc_help_exp =		N_("<value>");
    cmd->cc_help_exp_hint =	N_("Alphanumeric string used to indicate to DNS that the query came from cache");
    cmd->cc_flags =		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_exec_callback  =	cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http * aws access-key * secret-key * dns-query";
    cmd->cc_help_descr =        N_("Keyword preceding the specification of an identification string in DNS query");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * aws access-key * secret-key * dns-query *";
    cmd->cc_help_exp =		N_("<value>");
    cmd->cc_help_exp_hint =	N_("Alphanumeric string used to indicate to DNS that the query came from cache");
    cmd->cc_flags =		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_exec_callback  =	cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http * *";
    cmd->cc_help_exp = 		N_("<port number>");
    cmd->cc_help_exp_hint = 	N_("Port number");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/host/name";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/http/host/port";
    cmd->cc_exec_value2 = 		"$3$";
    cmd->cc_exec_type2 = 		bt_uint16;
    cmd->cc_revmap_order =      	cro_namespace;
    cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/origin-server/http/host/name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_http_host_revmap;
    cmd->cc_revmap_suborder = 	crso_origin_server;
    cmd->cc_exec_callback  =	cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * * aws";
    cmd->cc_help_descr =	N_("Keyword to specify optional parameters for accessing AWS based S3");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * * aws access-key";
    cmd->cc_help_descr =	N_("Keyword to specify the access key provides by AWS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * * aws access-key *";
    cmd->cc_help_exp =		N_("<key>");
    cmd->cc_help_exp_hint =	N_("Specify the access key provided by AWS");
    cmd->cc_flags =             ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * * aws access-key * secret-key";
    cmd->cc_help_descr =	N_("Keyword to specify the secret key provided by AWS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * origin-server http * * aws access-key * secret-key *";
    cmd->cc_help_exp =		N_("<key>");
    cmd->cc_help_exp_hint =	N_("Specify the secret key provided by AWS");
    cmd->cc_flags =		ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_revmap_order =	cro_namespace;
    cmd->cc_exec_callback  =	cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http * * dns-query";
    cmd->cc_help_descr =        N_("Keyword preceding the specification of an identification string in DNS query");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http * * dns-query *";
    cmd->cc_help_exp =        N_("<value>");
    cmd->cc_help_exp_hint =     N_("Alphanumeric string used to indicate to DNS that the query came from cache");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http * * aws access-key * secret-key * dns-query";
    cmd->cc_help_descr =        N_("Keyword preceding the specification of an identification string in DNS query");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http * * aws access-key * secret-key * dns-query *";
    cmd->cc_help_exp =        N_("<value>");
    cmd->cc_help_exp_hint =     N_("Alphanumeric string used to indicate to DNS that the query came from cache");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http server-map";
    cmd->cc_help_descr = 		N_("Configure a server-map reference that contains details on origin-servers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http server-map *";
    cmd->cc_help_exp = 			N_("<name>");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 		"/nkn/nvsd/server-map/config/*";
    cmd->cc_comp_type = 		cct_matching_values;
    cmd->cc_flags =		        ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_basic_curr;
    cmd->cc_exec_callback  =	cli_nvsd_ns_servermap_add;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_order =		cro_namespace;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/origin-server/http/server-map/*/map-name";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_http_server_map_revmap;
    cmd->cc_revmap_suborder = 	crso_origin_server;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"no namespace * origin-server";
    cmd->cc_help_descr = 		N_("Configure a server-map reference that contains details on origin-servers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"no namespace * origin-server http";
    cmd->cc_help_descr = 		N_("Configure a server-map reference that contains details on origin-servers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"no namespace * origin-server http server-map";
    cmd->cc_help_descr = 		N_("Configure a server-map reference that contains details on origin-servers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"no namespace * origin-server http server-map *";
    cmd->cc_help_exp = 			N_("<name>");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 		"/nkn/nvsd/namespace/$1$/origin-server/http/server-map/*/map-name";
    cmd->cc_comp_type = 		cct_matching_values;
    cmd->cc_flags =		        ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_basic_curr;
    cmd->cc_exec_callback  =	cli_nvsd_ns_servermap_delete;
    CLI_CMD_REGISTER;

    err = cli_revmap_hide_bindings("/nkn/nvsd/namespace/mfc_probe/**");
    bail_error(err);
    err = cli_revmap_hide_bindings("/nkn/nvsd/origin_fetch/config/mfc_probe/**");
    bail_error(err);

    err = cli_revmap_ignore_bindings_va(7,
	    "/nkn/nvsd/namespace/*/origin-server/http/host/*/weight",
	    "/nkn/nvsd/namespace/*/origin-server/http/host/*/keep-alive",
	    "/nkn/nvsd/namespace/*/origin-server/http/server-map/*",
	    "/nkn/nvsd/origin_fetch/config/*/convert/head",
	    "/nkn/nvsd/namespace/*/origin-server/http/host/aws/access-key",
	    "/nkn/nvsd/namespace/*/origin-server/http/host/aws/secret-key",
	    "nkn/nvsd/namespace/*/origin-server/http/host/dns-query");
    bail_error(err);

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_origin_server_http_follow_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);


    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http follow dest-ip";
    cmd->cc_help_descr = 		N_("Use a destination IP address on incoming client request as the origin server");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/dest-ip";
    cmd->cc_exec_value =		"true";
    cmd->cc_exec_type = 		bt_bool;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/use-client-ip";
    cmd->cc_exec_value2 =		"false";
    cmd->cc_exec_type2 = 		bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http follow header";
    cmd->cc_help_descr = 		N_("Use the value of a header in incoming client request as the origin server");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http follow header *";
    cmd->cc_help_exp = 		N_("<name>");
    cmd->cc_help_exp_hint =		N_("Header name");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/header";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/use-client-ip";
    cmd->cc_exec_value2 =		"false";
    cmd->cc_exec_type2 = 		bt_bool;
    cmd->cc_revmap_type = 		crt_none;
    cmd->cc_exec_callback =		cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http follow header * use-client-ip";
    cmd->cc_help_descr = 		N_("Configures ip address to use when fetching an object from the origin-server");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/header";
    cmd->cc_exec_value = 		"$2$";
    cmd->cc_exec_type = 		bt_string;
    cmd->cc_magic	=		osvr_http_follow_header;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/use-client-ip";
    cmd->cc_exec_value2 = 		"true";
    cmd->cc_exec_type2 = 		bt_bool;
    cmd->cc_exec_callback =		cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws";
    cmd->cc_help_descr =        N_("Keyword to specify optional parameters for accessing AWS based S3");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key";
    cmd->cc_help_descr =        N_("Keyword to specify the access key provides by AWS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key *";
    cmd->cc_help_exp =          N_("<key>");
    cmd->cc_help_exp_hint =     N_("Specify the access key provided by AWS");
    cmd->cc_flags =		ccf_ignore_length;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key * secret-key";
    cmd->cc_help_descr =        N_("Keyword to specify the secret key provided by AWS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key * secret-key *";
    cmd->cc_help_exp =          N_("<key>");
    cmd->cc_help_exp_hint =     N_("Specify the secret key provided by AWS");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key * secret-key * use-client-ip";
    cmd->cc_help_descr =	N_("Configures ip address to use when fetching an object from the origin-server");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * dns-query";
    cmd->cc_help_descr =        N_("Keyword preceding the specification of an identification string in DNS query");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * dns-query *";
    cmd->cc_help_exp =		N_("<value>");
    cmd->cc_help_exp_hint =     N_("Alphanumeric string used to indicate to DNS that the query came from cache");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * dns-query * use-client-ip";
    cmd->cc_help_descr =	N_("Configures ip address to use when fetching an object from the origin-server");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key * secret-key * dns-query";
    cmd->cc_help_descr =        N_("Keyword preceding the specification of an identification string in DNS query");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key * secret-key * dns-query *";
    cmd->cc_help_exp =		N_("<value>");
    cmd->cc_help_exp_hint =     N_("Alphanumeric string used to indicate to DNS that the query came from cache");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * origin-server http follow header * aws access-key * secret-key * dns-query * use-client-ip";
    cmd->cc_help_descr =	N_("Configures ip address to use when fetching an object from the origin-server");
    cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_exec_callback  =    cli_nvsd_ns_origin_server_follow_hdr_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"namespace * origin-server http follow dest-ip use-client-ip";
    cmd->cc_help_descr  = 		N_("Configures ip address to use when fetching an object from the origin-server");
    cmd->cc_help_use_comp =		true;
    cmd->cc_flags = 			ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/dest-ip";
    cmd->cc_exec_value = 		"true";
    cmd->cc_exec_type = 		bt_bool;
    cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/http/follow/use-client-ip";
    cmd->cc_exec_value2 = 		"true";
    cmd->cc_exec_type2 = 		bt_bool;
    cmd->cc_magic	=		osvr_http_dest_ip;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/origin-server/http/follow/dest-ip";
    cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_http_follow_revmap;
    cmd->cc_revmap_order = 		cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_server;
    cmd->cc_revmap_type = 		crt_manual;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(4,
	    "/nkn/nvsd/namespace/*/origin-server/http/follow/use-client-ip/header",
	    "/nkn/nvsd/namespace/*/origin-server/http/follow/aws/access-key",
	    "/nkn/nvsd/namespace/*/origin-server/http/follow/aws/secret-key",
	    "nkn/nvsd/namespace/*/origin-server/http/follow/dns-query");
bail:
    return err;
}
/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_origin_server_http_ipversion_cmds_init(
                const lc_dso_info *info,
                const cli_module_context *context)
{
        int err = 0;
        cli_command *cmd = NULL;

        UNUSED_ARGUMENT(info);
        UNUSED_ARGUMENT(context);

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * origin-server http ip-version";
        cmd->cc_help_descr =            N_("Choose the ip-version for connecting to origin server");
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * origin-server http ip-version follow-client";
        cmd->cc_help_descr =            N_("Use the ipversion of the client to connect to origin server");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_operation =        cxo_set;
        cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/origin-server/http/ip-version";
        cmd->cc_exec_value =            "follow-client";
        cmd->cc_exec_type =             bt_string;
	cmd->cc_revmap_type =           crt_auto;
	cmd->cc_revmap_order =          cro_namespace;
	cmd->cc_revmap_suborder =       crso_delivery_proto;
        CLI_CMD_REGISTER;

        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * origin-server http ip-version force-ipv4";
        cmd->cc_help_descr =            N_("Always use the ipv4 to connect to origin server");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_operation =        cxo_set;
        cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/origin-server/http/ip-version";
        cmd->cc_exec_value =            "force-ipv4";
        cmd->cc_exec_type =             bt_string;
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_namespace;
        cmd->cc_revmap_suborder =       crso_delivery_proto;
        CLI_CMD_REGISTER;


        CLI_CMD_NEW;
        cmd->cc_words_str =             "namespace * origin-server http ip-version force-ipv6";
        cmd->cc_help_descr =            N_("Always use the ipv6 to connect to origin server");
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_exec_operation =        cxo_set;
        cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/origin-server/http/ip-version";
        cmd->cc_exec_value =            "force-ipv6";
        cmd->cc_exec_type =             bt_string;
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_namespace;
        cmd->cc_revmap_suborder =       crso_delivery_proto;
        CLI_CMD_REGISTER;

bail:
        return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_origin_server_nfs_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server nfs *";
	cmd->cc_help_exp = 		N_("<FQDN:export_path>");
	cmd->cc_help_exp_hint = 	N_("NFS server configuration");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/nfs/host";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server nfs * *";
	cmd->cc_help_exp = 		N_("<port number>");
	cmd->cc_help_exp_hint = 	N_("Port number");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/nfs/host";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_string;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/nfs/port";
	cmd->cc_exec_value2 = 		"$3$";
	cmd->cc_exec_type2 = 		bt_uint16;
    	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/origin-server/nfs/host";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_nfs_host_revmap;
	cmd->cc_revmap_suborder = 	crso_origin_server;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server nfs server-map";
	cmd->cc_help_descr = 		N_("Configure a server-map reference that contains details on origin-servers");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server nfs server-map *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/server-map/config/*";
	cmd->cc_comp_type = 		cct_matching_values;
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
#if 0
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/nfs/server-map";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_string;
#endif
	cmd->cc_exec_callback = 	cli_nvsd_namespace_smap_nfs_print_msg;
    	cmd->cc_revmap_type =       	crt_manual;
    	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/origin-server/nfs/server-map";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_nfs_server_map_revmap;
	cmd->cc_revmap_suborder = 	crso_origin_server;
	CLI_CMD_REGISTER;

bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_origin_server_rtsp_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp *";
	cmd->cc_help_exp = 		N_("<FQDN>");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =	        cli_nvsd_namespace_origin_server_rtsp;
	cmd->cc_magic = 		cc_oreq_rtsp_rtp_client;

	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * rtp-udp";
	cmd->cc_help_descr = 		N_("set transport as UDP");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =	        cli_nvsd_namespace_origin_server_rtsp;
	cmd->cc_magic = 		cc_oreq_rtsp_rtp_udp;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * rtp-rtsp";
	cmd->cc_help_descr = 		N_("set transport based on rtsp");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =	        cli_nvsd_namespace_origin_server_rtsp;
	cmd->cc_magic = 		cc_oreq_rtsp_rtp_rtsp;
	CLI_CMD_REGISTER;

	/* Optional [port #] */
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * *";
	cmd->cc_help_exp = 		N_("<port number>");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =	        cli_nvsd_namespace_origin_server_rtsp;
	cmd->cc_magic = 		cc_oreq_rtsp_rtp_client;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * * rtp-udp";
	cmd->cc_help_descr = 		N_("set transport as UDP");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =	        cli_nvsd_namespace_origin_server_rtsp;
	cmd->cc_magic = 		cc_oreq_rtsp_rtp_udp;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type = 		crt_manual;
	cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/origin-server/rtsp/host/name";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_rtsp_host_revmap;
	cmd->cc_revmap_suborder = 	crso_origin_server;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * * rtp-rtsp";
	cmd->cc_help_descr = 		N_("set transport based on rtsp");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =	        cli_nvsd_namespace_origin_server_rtsp;
	cmd->cc_magic = 		cc_oreq_rtsp_rtp_rtsp;
	cmd->cc_revmap_type = 		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp follow";
	cmd->cc_help_descr = 		N_("Configure RTSP to follow the origin server address from the client request");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp follow dest-ip";
	cmd->cc_help_descr = 		N_("Use the destination IP address from the client request as the origin server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/rtsp/follow/dest-ip";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/rtsp/follow/use-client-ip";
	cmd->cc_exec_value2 = 		"false";
	cmd->cc_exec_type2 = 		bt_bool;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp follow dest-ip use-client-ip";
	cmd->cc_help_descr = 		N_("Spoof request to Client's IP address when connecting to the Origin server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/origin-server/rtsp/follow/dest-ip";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/origin-server/rtsp/follow/use-client-ip";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_exec_type2 = 		bt_bool;
    	cmd->cc_revmap_type =       	crt_manual;
    	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/origin-server/rtsp/follow/dest-ip";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_ns_rtsp_follow_revmap;
	cmd->cc_revmap_suborder = 	crso_origin_server;
	CLI_CMD_REGISTER;
#if 0
	/* Optional alternate HTTP port to connect on, when an
	 * optional [port #] is given in the command line */
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * * alternate";
	cmd->cc_help_descr = 		N_("Configure alternate protocol options for this real-time stream origin");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * * alternate http";
	cmd->cc_help_descr = 		N_("Configure alternate protocol HTTP options");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * origin-server rtsp * * alternate http *";
	cmd->cc_help_exp = 		N_("<port number>");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback = 	cli_nvsd_namespace_origin_server_rtsp_config;
	cmd->cc_revmap_type =		crt_manual;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_names = 		"/nkn/nvsd/namespace/*/origin-server/rtsp";
	cmd->cc_revmap_callback = 	cli_nvsd_ns_rtsp_revmap;
	cmd->cc_revmap_suborder =	crso_origin_server;
	CLI_CMD_REGISTER;
#endif

bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_delivery_proto_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * delivery";
	cmd->cc_help_descr =		N_("Configure delivery protocols");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * delivery protocol";
	cmd->cc_help_descr =		N_("Configure delivery protocols");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * delivery protocol http";
	cmd->cc_help_descr =		N_("Configure HTTP delivery protocol");
	cmd->cc_help_term =             N_("Enable HTTP for this namespace");
	cmd->cc_flags = 		ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
	cmd->cc_magic =			cc_magic_delivery_http;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback = 	cli_namespace_generic_enter_mode;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * delivery protocol rtsp";
	cmd->cc_help_descr =		N_("Configure RTSP delivery protocol");
	cmd->cc_help_term =             N_("Enable RTSP for this namespace");
	cmd->cc_flags = 		ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
	cmd->cc_magic =			cc_magic_delivery_rtsp;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback = 	cli_namespace_generic_enter_mode;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery";
	cmd->cc_help_descr = 		N_("Reset delivery options for this namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol";
	cmd->cc_help_descr = 		N_("Reset delivery options for this namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol http";
	cmd->cc_help_descr = 		N_("Reset HTTP delivery options for this namespace");
	cmd->cc_help_term =             N_("Disable HTTP for this namespace");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/delivery/proto/http";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type = 		crt_auto;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_delivery_proto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * delivery protocol rtsp";
	cmd->cc_help_descr = 		N_("Reset RTSP delivery options for this namespace");
	cmd->cc_help_term =             N_("Disable RTSP for this namespace");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/delivery/proto/rtsp";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_names =          "/nkn/nvsd/namespace/*/delivery/proto/rtsp";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_origin_fetch_rtsp_revmap;
	cmd->cc_revmap_order =          cro_namespace;
	cmd->cc_revmap_suborder = 	crso_delivery_proto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http no";
	cmd->cc_help_descr = 		N_("Clear/remove HTTP protocol options");
	cmd->cc_req_prefix_count = 	5;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp no";
	cmd->cc_help_descr = 		N_("Clear/remove RTSP protocol options");
	cmd->cc_req_prefix_count = 	5;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http connection";
	cmd->cc_help_descr = 		N_("Configure client connections options.");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http connection concurrent";
	cmd->cc_help_descr = 		N_("Configure concurrent session options");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http connection concurrent session";
	cmd->cc_help_descr = 		N_("Configure concurrent session options");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http connection concurrent session retry-after";
	cmd->cc_help_descr = 		N_("Configure retry-after header option");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol http connection concurrent session retry-after *";
	cmd->cc_help_exp = 		N_("<seconds>");
	cmd->cc_help_exp_hint = 	N_("seconds to retry the sessions");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/resource-pool/limit/http/max-session/retry-after";
	cmd->cc_exec_value =		"$2$";
	cmd->cc_exec_type = 		bt_uint32;
	cmd->cc_revmap_type = 		crt_auto;
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_suborder =	crso_delivery_proto;
	CLI_CMD_REGISTER;


	err = cli_revmap_ignore_bindings_va(3,
			"/nkn/nvsd/namespace/*/resource-pool/limit/http/max-session/count",
			"/nkn/nvsd/namespace/*/resource-pool/limit/rtsp/max-session/count",
			"/nkn/nvsd/namespace/*/resource-pool/limit/rtsp/max-session/retry-after");
	bail_error(err);

bail:
	return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_object_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object";
    cmd->cc_help_descr =        N_("Operations on a URI object");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list";
    cmd->cc_help_descr =        N_("Check if URI exists");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list all";
    cmd->cc_help_descr =        N_("Lists all the objects in the cache associated to this namespace");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =     cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list all export";
    cmd->cc_help_descr =        N_("Upload the generated objects list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list all export *";
    cmd->cc_help_exp =          N_("Remote URL");
    cmd->cc_help_exp_hint =     N_("<scp://username:password@hostname/path>");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =     cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list *";
    cmd->cc_help_exp =          N_("URI | pattern");
    cmd->cc_help_exp_hint =     N_("Lists the specific object matching the uri or given pattern");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list * export";
    cmd->cc_help_descr =        N_("Upload the generated objects list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list * export *";
    cmd->cc_help_exp =          N_("Remote URL");
    cmd->cc_help_exp_hint =     N_("<scp://username:password@hostname/path>");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =     cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list * *";
    cmd->cc_help_exp   =        N_("<domain name>");
    cmd->cc_help_exp_hint =     N_("Valid only in the case of transparent & mid-tier proxy");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list * * *";
    cmd->cc_help_exp =          N_("<port>");
    cmd->cc_help_exp_hint =     N_("Defaults  to 80");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list * * * export";
    cmd->cc_help_descr =        N_("Upload the generated objects list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object list * * * export *";
    cmd->cc_help_exp =          N_("Remote URL");
    cmd->cc_help_exp_hint =     N_("<scp://username:password@hostname/path>");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =     cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object delete";
    cmd->cc_help_descr =        N_("Remove the URI if it exists");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object delete all";
    cmd->cc_help_descr =        N_("Removes objects in the cache associated to this namespace");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_priv_curr;
    cmd->cc_exec_callback =     cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object delete *";
    cmd->cc_help_exp =          N_("URI | pattern");
    cmd->cc_help_exp_hint =     N_("Removes objects matching the uri or given pattern");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object delete * *";
    cmd->cc_help_exp   =        N_("<domain name>");
    cmd->cc_help_exp_hint =     N_("Provide this only in the case of mid-tier or virtual");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object delete * * *";
    cmd->cc_help_exp =          N_("<port>");
    cmd->cc_help_exp_hint =     N_("Default if not specified is 80");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =	cli_ns_object_list_delete_cmd_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object checksum";
    cmd->cc_flags =             ccf_hidden;
    cmd->cc_help_descr =        N_("Calculate MD5 checksum of response data and store in a internal HTTP header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object checksum *";
    cmd->cc_help_exp =          N_("<num>");
    cmd->cc_help_exp_hint =     N_("Number of bytes from the response data (min: 1; max: 128000)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_ns_obj_checksum_cmd_hdlr;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_pin;
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =       (void*)cli_nvsd_ns_obj_checksum_revmap;
    cmd->cc_revmap_names =      "/nkn/nvsd/namespace/*/client-response/checksum";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * object checksum";
    cmd->cc_help_descr =        N_("Reset MD5 checksum of response data");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/client-response/checksum";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object revalidate";
    cmd->cc_help_descr =        N_("Re-validate the cached objects");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object revalidate all";
    cmd->cc_help_descr = 	N_("Re-Validate all the cached objects");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback = 	cli_ns_object_reval_cmd_hdlr;
    CLI_CMD_REGISTER;

    //Cache pinning
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * pinned-object cache-capacity";
    cmd->cc_help_descr = 	N_("Specify the disk-cache space reserved for pinned objects");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * pinned-object cache-capacity *";
    cmd->cc_help_exp = 		N_("<GB>");
    cmd->cc_help_exp_hint = 	N_("Amount of Bytes allocated for cache-pinning per namespace");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 	"/nkn/nvsd/origin_fetch/config/$1$/object/cache_pin/cache_size";
    cmd->cc_exec_value = 	"$2$";
    cmd->cc_exec_type = 	bt_uint32;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * pinned-object max-obj-size";
    cmd->cc_help_descr = 	N_("Specify the maximum size of the object that can be pinned");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object max-obj-size *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Specify the maximum size of the object that can be pinned");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/cache_pin/max_obj_size";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object pin-header";
    cmd->cc_help_descr = 	N_("Specify header for cache-pinning");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str =         "namespace * pinned-object pin-header *";
    cmd->cc_help_exp =          N_("<header-name>");
    cmd->cc_help_exp_hint =     N_("Specify header for cache-pinning");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/cache_pin/pin_header_name";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_names = 	"/nkn/nvsd/origin_fetch/config/*/object/cache_pin/enable";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_namespace_object_revmap;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_pin;
    CLI_CMD_REGISTER;

    /* NOTE:  parent is registered else where */
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pinned-object pin-header";
    cmd->cc_help_descr = 	N_("Reset header for cache-pinning");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/cache_pin/pin_header_name";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"namespace * object validity-begin-header";
    cmd->cc_help_descr = 	N_("Specify the header which determines when the objects can be delivered");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str =         "namespace * object validity-begin-header *";
    cmd->cc_help_exp =          N_("<header-name>");
    cmd->cc_help_exp_hint =     N_("Header to be matched for validitystart");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_ns_validity_header_cmd_hdlr;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * object";
    cmd->cc_help_descr = 	N_("Reset object settings");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no namespace * object validity-begin-header";
    cmd->cc_help_descr = 	N_("Reset the validity-begin-header which determines when the objects can be delivered");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/validity/start_header_name";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object status";
    cmd->cc_help_descr = 	N_("Enable cache-pinning");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object status enable";
    cmd->cc_help_descr = 	N_("Enable cache-pinning");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/cache_pin/enable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_pin;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pinned-object status disable";
    cmd->cc_help_descr = 	N_("Disable cache-pinning");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/object/cache_pin/enable";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object delete all pinned";
    cmd->cc_help_descr = 	N_("Remove all pinned object from cache");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =    	cli_ns_object_list_delete_cmd_hdlr;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    /* Compression related CLI commands */
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression";
    cmd->cc_help_descr = 	"Configure Compression parameters";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression enable";
    cmd->cc_help_descr = 	"Enable Object Compression";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/object/compression/enable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_compression;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression disable";
    cmd->cc_help_descr = 	"Disable Object Compression";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/object/compression/enable";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression size-range ";
    cmd->cc_help_descr = 	"Object size threshold for compression";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression size-range *";
    cmd->cc_help_exp = 		"<min_obj_size_in_bytes>";
    cmd->cc_help_exp_hint = 	"<min> - Minimum object size (128 bytes or above)";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           = "namespace * object compression size-range * *";
    cmd->cc_help_exp            = "<max-obj-size-inbytes>";
    cmd->cc_help_exp_hint       = "<max> - Maximum object size (up to 10 MB)";
    cmd->cc_flags               = ccf_terminal;
    cmd->cc_capab_required      = ccp_set_rstr_curr;
    cmd->cc_exec_operation      = cxo_set;
    cmd->cc_exec_name           = "/nkn/nvsd/origin_fetch/config/$1$/object/compression/obj_range_min";
    cmd->cc_exec_value          = "$2$";
    cmd->cc_exec_type           = bt_uint64;
    cmd->cc_exec_name2          = "/nkn/nvsd/origin_fetch/config/$1$/object/compression/obj_range_max";
    cmd->cc_exec_value2         = "$3$";
    cmd->cc_exec_type2          = bt_uint64;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_compression;
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =	(void*)cli_nvsd_origin_fetch_compression_revmap;
    cmd->cc_revmap_names =	"/nkn/nvsd/origin_fetch/config/*/object/compression/obj_range_min";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression algorithm";
    cmd->cc_help_descr = 	"Type of compression ";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression algorithm *";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_exp =          "<compression_algorithm>";
    cmd->cc_help_exp_hint =     "Type of compression algorithm";
    cmd->cc_help_options =      cli_namespace_compression_types;
    cmd->cc_help_num_options =  sizeof(cli_namespace_compression_types)/sizeof(cli_expansion_option);
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =		"/nkn/nvsd/origin_fetch/config/$1$/object/compression/algorithm";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_compression;
    cmd->cc_revmap_type =       crt_auto;

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression file-types";
    cmd->cc_help_descr = 	"Type of files to be compressed ";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * object compression";
    cmd->cc_help_descr = 	"Type of files to be compressed ";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * object compression file-types";
    cmd->cc_help_descr = 	"Type of files to be compressed ";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * object compression file-types *";
    cmd->cc_help_exp =          "<file-type-extension>";
    cmd->cc_help_exp_hint =     "Type of files to be compressed (e.g .txt, .js, .html, .htm, .css)";
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_values ;
    cmd->cc_comp_pattern =      "/nkn/nvsd/origin_fetch/config/$1$/object/compression/file_types/*";
    cmd->cc_flags =             ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_callback  =    cli_compression_filetypes;
    cmd->cc_magic = 		cid_compress_filetype_add;
    cmd->cc_revmap_type = 	crt_manual;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_suborder = 	crso_compression;
    cmd->cc_revmap_names =	"/nkn/nvsd/origin_fetch/config/*/object/compression/file_types/*";
    cmd->cc_revmap_cmds =       "namespace $5$ object compression file-types $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * object compression file-types *";
    cmd->cc_help_exp =          "<file-type-extension>";
    cmd->cc_help_exp_hint =     "Type of files to be compressed (e.g .txt, .js, .html, .htm, .css)";
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_values ;
    cmd->cc_comp_pattern =      "/nkn/nvsd/origin_fetch/config/$1$/object/compression/file_types/*";
    cmd->cc_flags =             ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_reset;
    cmd->cc_exec_callback  =    cli_compression_filetypes;
    cmd->cc_magic = 		cid_compress_filetype_delete;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(1,
                    "/nkn/nvsd/namespace/*/resource/cache/*/capacity");
    bail_error(err);

bail:
	return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_prestage_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * pre-stage";
	cmd->cc_help_descr =		N_("Configure pre-stage options");
	cmd->cc_flags = 		ccf_hidden;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * pre-stage password";
	cmd->cc_help_descr =		N_("Set password authentication mechanism");
	cmd->cc_flags = 		ccf_hidden;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * pre-stage password RADIUS";
	cmd->cc_help_descr = 		N_("Use a RADIUS authetication server");
	cmd->cc_flags = 		ccf_terminal | ccf_hidden;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pre-stage/auth-mechanism";
	cmd->cc_exec_value = 		"radius";
	cmd->cc_exec_type = 		bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * pre-stage password TACACS";
	cmd->cc_help_descr = 		N_("Use a TACACS authetication server");
	cmd->cc_flags = 		ccf_terminal | ccf_hidden;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pre-stage/auth-mechanism";
	cmd->cc_exec_value = 		"tacacs";
	cmd->cc_exec_type = 		bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =		"namespace * pre-stage password *";
	cmd->cc_help_exp =		N_("<password>");
	cmd->cc_help_exp_hint =		N_("Password");
	cmd->cc_flags =			ccf_terminal | ccf_sensitive_param;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pre-stage/auth-mechanism";
	cmd->cc_exec_value = 		"local";
	cmd->cc_exec_type = 		bt_string;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/pre-stage/password";
	cmd->cc_exec_value2 = 		"$2$";
	cmd->cc_exec_type2 = 		bt_string;
	CLI_CMD_REGISTER;

bail:
	return err;
}

/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_policy_map_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	err = cli_revmap_ignore_bindings_va(1,
			"/nkn/nvsd/namespace/*/policy-map");
	bail_error(err);

bail:
	return err;
}
/*---------------------------------------------------------------------------*/


int cli_nvsd_namespace_vip_bind_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * bind ip";
	cmd->cc_help_descr = 	  	"Specify an IPv4 address through which requests are accepted by the namespace";
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * bind ip *";
	cmd->cc_help_exp = 		"<IPV4>";
	cmd->cc_help_exp_hint = 	"Specify an IPv4 address through which requests are accepted by the namespace";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_comp_type =        	cct_matching_values;
	cmd->cc_help_use_comp =		true;
	cmd->cc_comp_pattern =      	"/net/interface/config/*/addr/ipv4/static/1/ip";
	cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/vip/$2$";
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_type =		bt_string;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_suborder =	crso_name;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * bind ip";
	cmd->cc_help_descr = 		"Remove an IPv4 address through which requests are accepted by the namespace";
   	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * bind ip *";
	cmd->cc_help_exp = 		"<IPV4>";
	cmd->cc_help_exp_hint = 	"Remove an IPv4 address through which requests are accepted by the namespace";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_help_use_comp =		true;
	cmd->cc_comp_pattern =      	"/nkn/nvsd/namespace/$1$/vip/*";
	cmd->cc_comp_type =        	cct_matching_names;
	cmd->cc_exec_name =             "/nkn/nvsd/namespace/$1$/vip/$2$";
	cmd->cc_exec_operation =	cxo_delete;
	cmd->cc_exec_type =		bt_string;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_type =		crt_none;
	CLI_CMD_REGISTER;

    bail:
	return err;
}


int
cli_nvsd_namespace_policy_engine_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;

	cli_command *cmd = NULL;

	void *cli_pe_is_valid_item_func_v = NULL;

	err = cli_get_symbol("cli_ssld_cfg_cmds", "cli_ssld_cfg_is_valid_item",
			&cli_pe_is_valid_item_func_v);
	cli_nvsd_pe_is_valid_item = cli_pe_is_valid_item_func_v;
	bail_error_null(err, cli_nvsd_pe_is_valid_item);


	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * bind";
	cmd->cc_help_descr = 		N_("Create an association to the namespace");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * bind policy";
	cmd->cc_help_descr = 		N_("Bind a policy to the namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * bind policy *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	"";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_namespace_policy_config;
	cmd->cc_comp_type =        	cct_matching_names;
	cmd->cc_comp_pattern =      	"/nkn/nvsd/policy_engine/config/policy/script/*";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/policy/file";
	cmd->cc_revmap_type =		crt_manual;
	cmd->cc_revmap_order =		cro_policy;
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_namespace_pe_revmap;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * bind";
	cmd->cc_help_descr = 		N_("Remove the association from the namespace");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * bind policy";
	cmd->cc_help_descr = 		N_("Remove the policy binding from the namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * bind policy *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	"";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_no_bind_policy;
	cmd->cc_comp_type =        	cct_matching_values;
	cmd->cc_comp_pattern =		"/nkn/nvsd/namespace/*/policy/file";
	cmd->cc_revmap_type = 		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy";
	cmd->cc_help_descr = 		N_("Bind a policy to the namespace");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server";
	cmd->cc_help_descr = 		N_("Bind a policy to the namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("Associate a policy server with"
								    "namespace");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_comp_type =        	cct_matching_names;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$";
	cmd->cc_exec_type =		bt_string;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_comp_pattern =      	"/nkn/nvsd/policy_engine/config/server/*";
	cmd->cc_help_use_comp =		true;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * precedence";
	cmd->cc_help_descr = 		N_("Set precedence of the server when multiple external policy servers are defined");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * precedence *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	"Set precedence of the server when multiple external policy servers are defined";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/precedence";
	cmd->cc_exec_type =		bt_uint16;
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_exec_value =		"$3$";
	cmd->cc_revmap_suborder = 	crso_policy;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * failover";
	cmd->cc_help_descr = 		N_("Set default action when the server is down");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * failover bypass";
	cmd->cc_help_descr = 		N_("Set default action when the server is downbypass: Bypass external policy and continue");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/failover";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * failover reject";
	cmd->cc_help_descr = 		N_("Set default action when the server is downbypass: Reject: reject the request");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/failover";
	cmd->cc_exec_type =		bt_uint16;
	cmd->cc_exec_value =		"1";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_policy;

	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout";
	cmd->cc_help_descr = 		N_("Enable or disable external callout points");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout client-request";
	cmd->cc_help_descr = 		N_("External callout point after client request is parsed");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout client-request"
					    " enable";
	cmd->cc_help_descr = 		N_("Enable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/client_req";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"true";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_policy;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout client-request"
					    " disable";
	cmd->cc_help_descr = 		N_("Disable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/client_req";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout client-response";
	cmd->cc_help_descr = 		N_("External callout point before client response is sent");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout client-response"
					    " enable";
	cmd->cc_help_descr = 		N_("Enable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/client_resp";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"true";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_policy;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout client-response"
					    " disable";
	cmd->cc_help_descr = 		N_("Disable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/client_resp";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout origin-request";
	cmd->cc_help_descr = 		N_("External callout point before request to origin is sent");
	CLI_CMD_REGISTER;



	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout origin-request"
					    " enable";
	cmd->cc_help_descr = 		N_("Enable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/origin_req";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"true";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_policy;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout origin-request"
					    " disable";
	cmd->cc_help_descr = 		N_("Disable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/origin_req";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout origin-response";
	cmd->cc_help_descr = 		N_("External callout point after origin response is parsed");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout origin-response"
					    " enable";
	cmd->cc_help_descr = 		N_("Enable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
					"callout/origin_resp";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"true";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_policy;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * policy server * callout origin-response"
					    " disable";
	cmd->cc_help_descr = 		N_("Disable this external callout");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =		cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$/"
		"callout/origin_resp";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * policy";
	cmd->cc_help_descr = 		N_("Remove a policy configuration from namespace");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * policy server";
	cmd->cc_help_descr = 		N_("Remove a policy server configuration");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * policy server *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	"Remove policy server configuration from namespace";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_comp_type =        	cct_matching_names;
	cmd->cc_exec_operation =	cxo_delete;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/policy/server/$2$";
	cmd->cc_comp_pattern =      	"/nkn/nvsd/namespace/$1$/policy/server/*";
	cmd->cc_exec_callback =		cli_nvsd_namespace_pe_hdlr;
	cmd->cc_help_use_comp =		true;
	CLI_CMD_REGISTER;


	err = cli_revmap_ignore_bindings_va(9,
			"/nkn/nvsd/namespace/*/policy/file",
			"/nkn/nvsd/namespace/*/policy/enable",
			"/nkn/nvsd/namespace/*/policy/time_stamp",
			"/nkn/nvsd/namespace/*/resource_pool",
			"/nkn/nvsd/namespace/*/deleted",
			"/nkn/nvsd/namespace/*/policy/server/*",
			"/nkn/nvsd/namespace/*/policy/server/*/callout/*",
			"/nkn/nvsd/namespace/*/policy/server/*/failover",
			"/nkn/nvsd/namespace/$1$/policy/server/*/precedence");

	bail_error(err);


bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_pub_point_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point";
	cmd->cc_help_descr = 		N_("Add a live stream publishing service");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	"";
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_type = 		cct_matching_values;
	cmd->cc_comp_pattern =		"/nkn/nvsd/namespace/$1$/pub-point/*";
	cmd->cc_flags = 		ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_string;
//	cmd->cc_revmap_type = 		crt_auto;
//	cmd->cc_revmap_order = 		cro_namespace;
//	cmd->cc_revmap_suborder = 	crso_pub_point;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * live-pub-point";
	cmd->cc_help_descr = 		N_("Remove a live stream publishing service");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no namespace * live-pub-point *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("A live stream publishing service");
	cmd->cc_comp_type =		cct_matching_values;
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_pattern =		"/nkn/nvsd/namespace/$1$/pub-point/*";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_delete;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_string;
	cmd->cc_revmap_type = 		crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * status";
	cmd->cc_help_descr = 		N_("Enable/disable this publish server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * status active";
	cmd->cc_help_descr = 		N_("Activate this publish server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/active";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type = 		crt_auto;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_pub_point;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * status inactive";
	cmd->cc_help_descr = 		N_("Deactivate this publish server");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/active";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type = 		crt_auto;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_pub_point;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode";
	cmd->cc_help_descr = 		N_("Configure recieve-mode options for this publish server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode on-demand";
	cmd->cc_help_descr = 		N_("Configure this publish point as On-demand");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/mode";
	cmd->cc_exec_value = 		"on-demand";
	cmd->cc_exec_type = 		bt_string;
#if 0
	cmd->cc_revmap_type = 		crt_manual;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_pub_point;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/pub-point/*/sdp";
	cmd->cc_revmap_callback = 	cli_nvsd_namespace_rtsp_pub_point_revmap;
#endif
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp";
	cmd->cc_help_descr = 		N_("Configure SDP uri for this publish point");
	cmd->cc_flags = 		ccf_unavailable;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp *";
	cmd->cc_help_exp_hint = 	N_("Name");
	cmd->cc_help_exp = 		N_("<name>");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp *  immediate";
	cmd->cc_help_descr = 		N_("Configure SDP uri for this publish point");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/mode";
	cmd->cc_exec_value = 		"immediate";
	cmd->cc_exec_type = 		bt_string;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/sdp";
	cmd->cc_exec_value2 = 		"$3$";
	cmd->cc_exec_type2 = 		bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp * start-time";
	cmd->cc_help_descr = 		N_("Configure Start time for this event");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp * start-time *";
	cmd->cc_help_exp = 		N_("<time>");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =		cli_nvsd_namespace_rtsp_pub_point_config;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp * start-time * end-time";
	cmd->cc_help_descr = 		N_("Configure Start time for this event");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * receive-mode sdp * start-time * end-time *";
	cmd->cc_help_exp = 		N_("<time>");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback =		cli_nvsd_namespace_rtsp_pub_point_config;
	cmd->cc_revmap_type = 		crt_manual;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_pub_point;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/pub-point/*/sdp";
        cmd->cc_revmap_callback =       cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =     	(void*)cli_nvsd_namespace_rtsp_pub_point_revmap;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching";
	cmd->cc_help_descr = 		N_("Configure caching options");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching disable";
	cmd->cc_help_descr = 		N_("Disllow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/enable";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type = 		crt_auto;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_pub_point;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching enable";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_type = 		crt_auto;
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_suborder = 	crso_pub_point;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching enable format-translate";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_hidden;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching enable format-translate chunked-ts";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/format/chunked-ts";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_type = 		crt_none;
//	cmd->cc_revmap_order = 		cro_namespace;
//	cmd->cc_revmap_suborder = 	crso_pub_point;
//	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/%s/pub-point/%s";
//	cmd->cc_revmap_callback = 	cli_nvsd_namespace_rtsp_pub_point_revmap;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching enable format-translate frag-mp4";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/format/frag-mp4";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_type = 		crt_none;
//	cmd->cc_revmap_order = 		cro_namespace;
//	cmd->cc_revmap_suborder = 	crso_pub_point;
//	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/%s/pub-point/%s";
//	cmd->cc_revmap_callback = 	cli_nvsd_namespace_rtsp_pub_point_revmap;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching enable format-translate moof";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/format/moof";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_type = 		crt_none;
//	cmd->cc_revmap_order = 		cro_namespace;
//	cmd->cc_revmap_suborder = 	crso_pub_point;
//	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/%s/pub-point/%s";
//	cmd->cc_revmap_callback = 	cli_nvsd_namespace_rtsp_pub_point_revmap;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * live-pub-point * caching enable format-translate chunk-flv";
	cmd->cc_help_descr = 		N_("Allow MFD to cache the live streamed content");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/enable";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/pub-point/$2$/caching/format/chunk-flv";
	cmd->cc_exec_value2 = 		"true";
	cmd->cc_exec_type2 = 		bt_bool;
	cmd->cc_revmap_type = 		crt_none;
//	cmd->cc_revmap_order = 		cro_namespace;
//	cmd->cc_revmap_suborder = 	crso_pub_point;
//	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/%s/pub-point/%s";
//	cmd->cc_revmap_callback = 	cli_nvsd_namespace_rtsp_pub_point_revmap;
	CLI_CMD_REGISTER;
#if 1
	err = cli_revmap_ignore_bindings_va(2,
			"/nkn/nvsd/namespace/*/pub-point/*",
			"/nkn/nvsd/namespace/*/pub-point/*/caching/format/**");

	bail_error(err);
#endif
bail:
	return err;
}


/*---------------------------------------------------------------------------*/
/* This is called when the user give 3 parameters in the CLI -
 *  - header name
 *  - header value
 *  - precedence
 */
int cli_nvsd_namespace_match_header_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_namespace = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_header_name = NULL;
	char *bn_header_value = NULL;
	char *bn_precedence = NULL;
	const char *hdr_value = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	bn_header_name = smprintf("/nkn/nvsd/namespace/%s/match/header/name",
			c_namespace);
	bail_null(bn_header_name);

	bn_header_value = smprintf("/nkn/nvsd/namespace/%s/match/header/value",
			c_namespace);
	bail_null(bn_header_value);

	bn_precedence = smprintf("/nkn/nvsd/namespace/%s/match/precedence",
			c_namespace);
	bail_null(bn_precedence);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_string,
			tstr_array_get_str_quick(params, 1),
			bn_header_name);
	bail_error(err);
	if (ret_err)
		goto bail;

	if (safe_strcmp(tstr_array_get_str_quick(params, 2), "any") == 0) {
		hdr_value = "*";
	} else {
		hdr_value = tstr_array_get_str_quick(params, 2);
	}
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_string,
			hdr_value,
			bn_header_value);
	bail_error(err);
	if (ret_err)
		goto bail;
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32,
			tstr_array_get_str_quick(params, 3),
			bn_precedence);
	bail_error(err);
	if (ret_err)
		goto bail;

bail:
	if (ret_err && ret_msg) {
		cli_printf_error(_("error: %s\n"), ts_str(ret_msg));
	}
	safe_free(bn_header_name);
	safe_free(bn_header_value);
	safe_free(bn_precedence);
	ts_free(&ret_msg);
	return err;
}

/*---------------------------------------------------------------------------*/
/* This is called when the user give 3 parameters in the CLI -
 *  - query name
 *  - query value
 *  - precedence
 */
int cli_nvsd_namespace_match_query_string_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_namespace = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_query_name = NULL;
	char *bn_query_value = NULL;
	char *bn_precedence = NULL;
	const char *query_value = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	bn_query_name = smprintf("/nkn/nvsd/namespace/%s/match/query-string/name",
			c_namespace);
	bail_null(bn_query_name);

	bn_query_value = smprintf("/nkn/nvsd/namespace/%s/match/query-string/value",
			c_namespace);
	bail_null(bn_query_value);

	bn_precedence = smprintf("/nkn/nvsd/namespace/%s/match/precedence",
			c_namespace);
	bail_null(bn_precedence);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_string,
			tstr_array_get_str_quick(params, 1),
			bn_query_name);
	bail_error(err);
	if (ret_err)
		goto bail;

	if (safe_strcmp(tstr_array_get_str_quick(params, 2), "any") == 0) {
		query_value = "*";
	} else {
		query_value = tstr_array_get_str_quick(params, 2);
	}
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_string,
			query_value,
			bn_query_value);
	bail_error(err);
	if (ret_err)
		goto bail;

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32,
			tstr_array_get_str_quick(params, 3),
			bn_precedence);
	bail_error(err);
	if (ret_err)
		goto bail;

bail:
	if (ret_err && ret_msg) {
		cli_printf_error(_("error: %s\n"), ts_str(ret_msg));
	}
	safe_free(bn_query_name);
	safe_free(bn_query_value);
	safe_free(bn_precedence);
	ts_free(&ret_msg);
	return err;
}

/*---------------------------------------------------------------------------*/
/* This is called when the user give 3 parameters in the CLI -
 *  - vhost ip
 *  - vhost port
 *  - precedence
 */
int cli_nvsd_namespace_match_vhost_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_namespace = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_vhost_ip = NULL;
	char *bn_vhost_port = NULL;
	char *bn_precedence = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	bn_vhost_ip = smprintf("/nkn/nvsd/namespace/%s/match/virtual-host/ip",
			c_namespace);
	bail_null(bn_vhost_ip);

	bn_vhost_port = smprintf("/nkn/nvsd/namespace/%s/match/virtual-host/port",
			c_namespace);
	bail_null(bn_vhost_port);

	bn_precedence = smprintf("/nkn/nvsd/namespace/%s/match/precedence",
			c_namespace);
	bail_null(bn_precedence);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_ipv4addr,
			tstr_array_get_str_quick(params, 1),
			bn_vhost_ip);
	bail_error(err);
	if (ret_err)
		goto bail;

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint16,
			tstr_array_get_str_quick(params, 2),
			bn_vhost_port);
	bail_error(err);
	if (ret_err)
		goto bail;

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32,
			tstr_array_get_str_quick(params, 3),
			bn_precedence);
	bail_error(err);
	if (ret_err)
		goto bail;


bail:
	return err;
}


int cli_nvsd_namespace_delivery_proto_match_details(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params,
		const char *c_proto_name)
{

	int err = 0;
	tstring *t_type = NULL;
	uint8 type = 0;
	const char *c_namespace = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	(void) c_proto_name;
	err = mdc_get_binding_tstr_fmt(cli_mcc,
			NULL, NULL, NULL,
			&t_type,
			NULL,
			"/nkn/nvsd/namespace/%s/match/type",
			c_namespace);
	bail_error(err);

	type = atoi (ts_str(t_type));

	switch(type)
	{
	case 0:
		cli_printf(_("  Match Type: \n      - Not specified -\n"));
		break;
	case 1:
		/* Uri Prefix only */
		cli_printf_query(_("  Match Type: \n      URI-Prefix - #/nkn/nvsd/namespace/%s/match/uri/uri_name#\n"),
				c_namespace);
		break;
	case 2:
		/* URI Regex */
		cli_printf_query(_("  Match Type: \n      URI Regex - \"#/nkn/nvsd/namespace/%s/match/uri/regex#\"\n"),
				c_namespace);
		break;
	case 3:
		/* Header Name/Value */
		cli_printf(_("  Match Type: \n      Header Name/Value\n"));
		cli_printf_query(_("      Header Name: #/nkn/nvsd/namespace/%s/match/header/name#\n"),
				c_namespace);
		cli_printf_query(_("      Header Value: #/nkn/nvsd/namespace/%s/match/header/value#\n"),
				c_namespace);
		break;
	case 4:
		/* Header Regex */
		cli_printf_query(_("  Match Type: \n      Header Regex - \"#/nkn/nvsd/namespace/%s/match/header/regex#\"\n"),
				c_namespace);
		break;
	case 5:
		/* Query string Name/Value */
		cli_printf(_("  Match Type: \n      Query-String Name/Value\n"));
		cli_printf_query(_("      Query-String Name: #/nkn/nvsd/namespace/%s/match/query-string/name#\n"),
				c_namespace);
		cli_printf_query(_("      Query-String Value: #/nkn/nvsd/namespace/%s/match/query-string/value#\n"),
				c_namespace);
		break;
	case 6:
		/* Query-string Regex */
		cli_printf_query(_("  Match Type: \n      Query-String Regex - \"#/nkn/nvsd/namespace/%s/match/query-string/regex#\"\n"),
				c_namespace);
		break;
	case 7:
		/* Virtual-host IP/Port */
		cli_printf(_("  Match Type: \n      Virtual-Host IP/Port\n"));
		cli_printf_query(_("      Virtual-host IP:   #/nkn/nvsd/namespace/%s/match/virtual-host/ip#\n"),
				c_namespace);
		cli_printf_query(_("      Virtual-host Port: #/nkn/nvsd/namespace/%s/match/virtual-host/port#\n"),
				c_namespace);
		break;
#if 0
	case 8:
		/* RTSP Uri Prefix only */
		cli_printf_query(_("  Match Type: \n      URI-Prefix - #/nkn/nvsd/namespace/%s/%s/match/uri/uri_name#\n"),
				c_namespace,c_proto_name);
		break;
	case 9:
		/* RTSP URI Regex */
		cli_printf_query(_("  Match Type: \n      URI Regex - \"#/nkn/nvsd/namespace/%s/%s/match/uri/regex#\"\n"),
				c_namespace, c_proto_name);
		break;
	case 10:
		/* RTSP Virtual-host IP/Port */
		cli_printf(_("  Match Type: \n      Virtual-Host IP/Port\n"));
		cli_printf_query(_("      Virtual-host IP:   #/nkn/nvsd/namespace/%s/%s/match/virtual-host/ip#\n"),
				c_namespace, c_proto_name);
		cli_printf_query(_("      Virtual-host Port: #/nkn/nvsd/namespace/%s/%s/match/virtual-host/port#\n"),
				c_namespace, c_proto_name);
		break;
#endif
	default:
		cli_printf(_("  Match Type: \n      UNKNOWN\n"));
		break;
	}

bail:
	ts_free(&t_type);
	return err;

}


int cli_nvsd_http_server_map_show(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *server_map = NULL;
    tstring* format_type = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    if (idx != 0)
	cli_printf(_("\n"));

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &server_map);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		    &format_type, NULL,
		    "/nkn/nvsd/server-map/config/%s/format-type",
		    ts_str(server_map));
    bail_error(err);


    if (ts_equal_str(format_type, "1", true)) {
       err = cli_printf(
                _("    %s   -    host-origin-map\n"), ts_str(server_map));
    }
    else if (ts_equal_str(format_type, "2", true)) {

        err = cli_printf(
                _("    %s   -    cluster-map\n"), ts_str(server_map));
    }
    else if (ts_equal_str(format_type, "3", true)) {
       err = cli_printf(
                _("    %s   -    origin-escalation-map\n"), ts_str(server_map));
    } /* "4" is assigned to NFS -Server-map that is not supported*/
    else if (ts_equal_str(format_type, "5", true)) {
       err = cli_printf(
                _("    %s   -    origin-round-robin-map\n"), ts_str(server_map));
    }
    else {
        err = cli_printf(
                _("    Format-Type : None\n"));

    }
    bail_error(err);

    bail:
	ts_free(&server_map);
	return(err);
}


int cli_nvsd_ns_vip_show(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err = 0;


    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(callback_data);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(idx);


    cli_printf("    %s\n", ts_str(value));
    bail:
    return err;

}

int cli_nvsd_ns_cluster_show(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *cluster = NULL;
    const char *ns = callback_data;


    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);

    if (idx != 0)
	cli_printf(_("\n"));

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &cluster);
    bail_error(err);

    if (ns != NULL) {
	   err = cli_show_cl_suffix(ns, ts_str(cluster), NULL, false);
	   bail_error(err);
    } else {
	    err = cli_nvsd_show_cluster(ts_str(cluster), 1);
	    bail_error(err);
    }

bail:
    ts_free(&cluster);
    return(err);
}
/*---------------------------------------------------------------------------*/
int cli_nvsd_namespace_origin_server_show_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	char *pattern = NULL;
	const char *c_namespace = NULL;
	tstring *t_type = NULL;
	uint8 type = 0;
	bn_binding_array *http_origins = NULL;
	uint32 num_server_maps = 0;
	tstring *aws_access_key = NULL;
	tstring *aws_secret_key = NULL;
	node_name_t node_name = {0};

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);


	err = mdc_get_binding_tstr_fmt(cli_mcc,
			NULL, NULL, NULL,
			&t_type,
			NULL,
			"/nkn/nvsd/namespace/%s/origin-server/type",
			c_namespace);
	bail_error(err);

	type = atoi (ts_str(t_type));

	switch(type)
	{
	case osvr_none:
		cli_printf(_("  Origin-Server: - Not Specified -\n"));
		break;
	case osvr_http_host:
		cli_printf_query(_("  Origin-Server: http://#/nkn/nvsd/namespace/%s/origin-server/http/host/name#"
					":#/nkn/nvsd/namespace/%s/origin-server/http/host/port#\n"),
				c_namespace, c_namespace);
		err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &aws_access_key, NULL,
			"/nkn/nvsd/namespace/%s/origin-server/http/host/aws/access-key", c_namespace);
		bail_error(err);
		if (aws_access_key && ts_length(aws_access_key)) {
			cli_printf_query(_("  Origin-Server: http: aws: access-key: "
				"#/nkn/nvsd/namespace/%s/origin-server/http/host/aws/access-key#\n"),  c_namespace);
		}
		err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &aws_secret_key, NULL,
			"/nkn/nvsd/namespace/%s/origin-server/http/host/aws/secret-key", c_namespace);
		bail_error(err);
		if (aws_secret_key && ts_length(aws_secret_key)) {
			cli_printf_query(_("  Origin-Server: http: aws: secret-key: "
				"#/nkn/nvsd/namespace/%s/origin-server/http/host/aws/secret-key#\n"),  c_namespace);
		}
#if 0
		pattern = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/*", c_namespace);
		bail_null(pattern);

		err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, &http_origins,
				true, true, pattern);
		bail_error(err);
		err = bn_binding_array_sort(http_origins);
		bail_error(err);
		err = mdc_foreach_binding_prequeried(http_origins, pattern,
				NULL, cli_nvsd_origin_http_show_one, NULL,
				&num_matches);
		bail_error(err);
#endif
		break;
	case osvr_http_abs_url:
		cli_printf(_("  Origin-Server: ABSOLUTE URL FROM REQUEST\n"));
		break;
	case osvr_http_follow_header:
		cli_printf_query(_("  Origin-Server: Follow HTTP Header "
					"'#/nkn/nvsd/namespace/%s/origin-server/http/follow/header#'\n"),
				c_namespace);
		err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &aws_access_key, NULL,
			"/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/access-key",  c_namespace);
		bail_error(err);
		if (aws_access_key && ts_length(aws_access_key)) {
			cli_printf_query(_("  Origin-Server: http: aws: access-key: "
				"#/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/access-key#\n"),  c_namespace);
		}
		snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/secret-key", c_namespace);
		err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &aws_secret_key, NULL,
			 "/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/secret-key",c_namespace );
		bail_error(err);
		if (aws_secret_key && ts_length(aws_secret_key)) {
			cli_printf_query(_("  Origin-Server: http: aws: secret-key: "
				"#/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/secret-key#\n"),  c_namespace);
		}
		break;
	case osvr_http_dest_ip:
		cli_printf(_("  Origin-Server: Use destination IP address from Request\n"));
		break;
	case osvr_http_server_map:
		cli_printf(_("  Origin-Server: server-map Type: host \n"));
		pattern = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name", c_namespace);
		bail_null(pattern);
		err = mdc_foreach_binding(cli_mcc, pattern, NULL, cli_nvsd_http_server_map_show,
		   NULL, &num_server_maps);
		bail_error(err);

		if (num_server_maps == 0)
		   cli_printf(_("   NONE\n"));
		break;
	case osvr_nfs_host:
		cli_printf_query(_("  Origin-Server: nfs://#/nkn/nvsd/namespace/%s/origin-server/nfs/host#:"
					"#/nkn/nvsd/namespace/%s/origin-server/nfs/port#\n"),
				c_namespace, c_namespace);
		break;
	case osvr_nfs_server_map:
		cli_printf_query(_("  Origin-Server: SMAP - #/nkn/nvsd/namespace/%s/origin-server/nfs/server-map#\n"),
				c_namespace);
		break;
	case osvr_rtsp_host:
		cli_printf_query(_("  Origin-Server: rtsp://#/nkn/nvsd/namespace/%s/origin-server/rtsp/host/name#:"
					"#/nkn/nvsd/namespace/%s/origin-server/rtsp/host/port#\n"),
				c_namespace, c_namespace);
		break;
	case osvr_http_node_map:
		cli_printf(_("  Origin-Server: server-map Type: node \n"));
		pattern = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name", c_namespace);
		bail_null(pattern);
		err = mdc_foreach_binding(cli_mcc, pattern, NULL, cli_nvsd_http_server_map_show,
		    NULL, &num_server_maps);
			bail_error(err);

		if (num_server_maps == 0)
		    cli_printf(_("   NONE\n"));
		break;
	default:
		break;
	}

    	err = cli_printf_query(
                _("  Origin-Server: Idle-timeout: #/nkn/nvsd/namespace/%s/origin-server/http/idle-timeout#\n"), c_namespace);
    	bail_error(err);
    	err = cli_printf_query(
                _("  Origin-Server: IP-version type: #/nkn/nvsd/namespace/%s/origin-server/http/ip-version#\n"), c_namespace);
    	bail_error(err);

bail:
	safe_free(pattern);
	ts_free(&t_type);
	ts_free(&aws_access_key);
	ts_free(&aws_secret_key);
	bn_binding_array_free(&http_origins);
	return err;
}


/*---------------------------------------------------------------------------*/
static int
cli_nvsd_domain_name_revmap (void *data, const cli_command *cmd,
	const bn_binding_array *bindings,
	const char *name,
	const tstr_array *name_parts,
	const char *value, const bn_binding *binding,
	cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_domain_regex = NULL;
    tstring *t_domain = NULL;
    tstring *rev_cmd = NULL;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    //bn_binding_dump(LOG_NOTICE, "DOMAIN", binding);
    //lc_log_basic(LOG_NOTICE, "value: %s", value);

    if (bn_binding_name_pattern_match(name, "/nkn/nvsd/namespace/*/domain/regex")) {
	/* Get the regex value */
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_domain_regex,
		"/nkn/nvsd/namespace/%s/domain/regex", namespace);
	bail_error_null(err, t_domain_regex);

	if (t_domain_regex &&(ts_length(t_domain_regex) > 0)) {
	    err = ts_new_sprintf(&rev_cmd, "namespace %s domain regex \"%s\"",
		    namespace, value);
	    bail_error(err);
	}
	/* consume nodes */
	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/domain/regex",
		namespace);
	bail_error(err);
    } else {
	err = bn_binding_array_get_value_tstr_by_name(bindings,name,NULL, &t_domain);
	bail_error(err);

	err = ts_new_sprintf(&rev_cmd, "namespace %s domain %s",
		namespace, ts_str(t_domain));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"%s",name );
	bail_error(err);
    }

    if (NULL != rev_cmd) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }


bail:
    ts_free(&t_domain);
    ts_free(&t_domain_regex);
    ts_free(&rev_cmd);
    return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_get_match_type (const char *namespace, match_type_t *mt, const bn_binding_array *bindings)
{
	int err =0;
	tstring *t_type = NULL;

	bail_null(namespace);
	bail_null(mt);

	*mt = mt_none;

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_type,
			"/nkn/nvsd/namespace/%s/match/type", namespace);
	bail_error_null(err, t_type);

	if (t_type)
	  *mt = atoi(ts_str(t_type));

bail:
	ts_free(&t_type);
	return err;

}

static int
cli_get_origin_type (const char *namespace, origin_server_type_t *ot, const bn_binding_array *bindings)
{
	int err =0;
	tstring *t_type = NULL;

	bail_null(ot);
	bail_null(namespace);

	*ot = osvr_none;
	if(bindings) {
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_type,
				"/nkn/nvsd/namespace/%s/origin-server/type", namespace);
		bail_error_null(err, t_type);
	} else {
		err = mdc_get_binding_tstr_fmt(cli_mcc,
				NULL, NULL, NULL, &t_type,
				NULL,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}

	if (t_type)
		*ot = atoi(ts_str(t_type));

bail:
	ts_free(&t_type);
	return err;
}




/*---------------------------------------------------------------------------*/
static int
cli_nvsd_uri_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *rev_cmd = NULL;
    tstring *t_uri_name= NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_uri_name == mt) {
	err = bn_binding_array_get_value_tstr_by_name(bindings, name, NULL, &t_uri_name);
	bail_error_null(err, t_uri_name);

	if (!t_uri_name)
	    goto bail;
	if (ts_length(t_uri_name) > 0) {
	    err = ts_new_sprintf(&rev_cmd,
		    "namespace %s match uri %s",
		    namespace, ts_str(t_uri_name));
	    bail_error(err);

	    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	    bail_error(err);
	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/match/type",
		namespace);
	bail_error(err);
    }


    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    return(err);
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_uri_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_uri_regex = NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_uri_regex == mt) {

	err = mdc_get_binding_tstr(cli_mcc,
		NULL, NULL, NULL, &t_uri_regex, name, NULL);
	bail_error(err);

	if (!t_uri_regex)
	    goto bail;
	if (ts_length(t_uri_regex) > 0) {

	    err = tstr_array_append_sprintf(
		    ret_reply->crr_cmds,
		    "namespace %s match uri regex \"%s\"",
		    namespace,
		    ts_str(t_uri_regex));
	    bail_error(err);
	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/match/type",
		namespace);
	bail_error(err);

    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

bail:
    return(err);
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_header_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *rev_cmd = NULL;
    tstring *t_header_value = NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_header_name == mt) {

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_header_value,
		"/nkn/nvsd/namespace/%s/match/header/value",
		namespace);
	bail_error(err);

	if (value && (strlen(value) > 0)) {

	    err = ts_new_sprintf(
		    &rev_cmd,
		    "namespace %s match header \"%s\" \"%s\"",
		    namespace,
		    value,
		    t_header_value ? ts_str(t_header_value) : "");
	    bail_error(err);
	    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	    bail_error(err);
	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/match/type",
		namespace);
	bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
	    "/nkn/nvsd/namespace/%s/match/header/value", namespace);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&t_header_value);
    return(err);
}



/*---------------------------------------------------------------------------*/
static int
cli_nvsd_header_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_hdr_regex = NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_header_regex == mt) {

		err = mdc_get_binding_tstr(cli_mcc,
			    NULL, NULL, NULL, &t_hdr_regex, name, NULL);
		bail_error(err);

		if (!t_hdr_regex)
			goto bail;
		if (ts_length(t_hdr_regex) > 0) {

				err = tstr_array_append_sprintf(
						ret_reply->crr_cmds,
						"namespace %s match header regex \"%s\"",
						namespace,
						ts_str(t_hdr_regex));
				bail_error(err);
		}

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/match/type",
				namespace);
		bail_error(err);
    }
    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);
bail:
    return(err);
}


/*---------------------------------------------------------------------------*/
static int
cli_nvsd_query_string_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *rev_cmd = NULL;
    tstring *t_query_value = NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_query_string_name == mt) {

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_query_value,
		"/nkn/nvsd/namespace/%s/match/query-string/value",
		namespace);
	bail_error(err);

	if (value && (strlen(value) > 0)) {

	    err = ts_new_sprintf(
		    &rev_cmd,
		    "namespace %s match query-string \"%s\" \"%s\"",
		    namespace,
		    value,
		    t_query_value ? ts_str(t_query_value) : "");
	    bail_error(err);
	}
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/match/type",
		namespace);
	bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
	    "/nkn/nvsd/namespace/%s/match/query-string/value", namespace);
    bail_error(err);

bail:
    return(err);
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_query_string_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_qry_regex = NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_query_string_regex == mt) {

	err = bn_binding_array_get_value_tstr_by_name(bindings, name, NULL, &t_qry_regex);
	bail_error_null(err, t_qry_regex);

	if (ts_length(t_qry_regex) > 0) {

	    err = tstr_array_append_sprintf(
		    ret_reply->crr_cmds,
		    "namespace %s match query-string regex \"%s\"",
		    namespace,
		    ts_str(t_qry_regex));
	    bail_error(err);
	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/match/type",
		namespace);
	bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);
bail:
    return(err);
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_vhost_name_port_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_vhost_name= NULL;
    tstring *t_vhost_value = NULL;
    match_type_t mt = mt_none;
    const char *namespace = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_match_type(namespace, &mt, bindings);
    bail_error(err);

    /* If the match type is for this node,
     * only then create the command
     */
    if (mt_vhost == mt) {

	err = mdc_get_binding_tstr(cli_mcc,
		NULL, NULL, NULL,
		&t_vhost_name,
		name, NULL);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name(bindings, name,NULL, &t_vhost_name);
	bail_error_null(err, t_vhost_name);

	if (t_vhost_name
		&& (ts_length(t_vhost_name) > 0)
		&& !(ts_equal_str(t_vhost_name, "255.255.255.255", false))) {

	    /* Get the header value too */
	    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_vhost_value,
		    "/nkn/nvsd/namespace/%s/match/virtual-host/port", namespace);
	    bail_error_null(err, t_vhost_value);

	    err = ts_new_sprintf(&rev_cmd,
		    "namespace %s match virtual-host \"%s\" \"%s\"",
		    namespace,
		    ts_str(t_vhost_name),
		    t_vhost_value ? ts_str(t_vhost_value) : "");
	    bail_error(err);

	    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	    bail_error(err);

	    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		    "/nkn/nvsd/namespace/%s/match/type",
		    namespace);
	    bail_error(err);
	}

    }

    /* Consume bindings */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
	    "/nkn/nvsd/namespace/%s/match/virtual-host/port",
	    namespace);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    return(err);
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_http_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tstring *t_port = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	if (osvr_http_host == ot) {

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
				NULL, &t_port, "/nkn/nvsd/namespace/%s/origin-server/http/host/port", namespace);
		bail_error(err);

		err = ts_new_sprintf(&rev_cmd, "namespace %s origin-server http \"%s\"",
				namespace, value);
		bail_error(err);

		if (!ts_equal_str(t_port, "80", false)) {
			err = ts_append_sprintf(rev_cmd, " \"%s\"",
					ts_str(t_port));
			bail_error(err);
		}

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}


	/* Consume nodes - even though this is wildcarded,
	 * it is possible some stale config exists.
	 */

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/http/host/port", namespace);
	bail_error(err);
#if 0
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/weight", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/keep-alive", name);
	bail_error(err);
#endif
bail:
	ts_free(&rev_cmd);
	ts_free(&t_port);
	return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_http_server_map_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *namespace = NULL;
    const char *position = NULL;
    origin_server_type_t ot = osvr_none;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);

    /* BUG FIX: PR:787639
     * Check the origin server type before displaying the details of
     * the associated server-map.
     */
    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_origin_type(namespace, &ot, bindings);
    bail_error(err);

    position = tstr_array_get_str_quick(name_parts, 7);
    bail_null(position);

    if( osvr_http_node_map == ot || osvr_http_server_map == ot) {
	    err = ts_new_sprintf(&rev_cmd, "namespace %s origin-server http server-map \"%s\"",
			    namespace, value);
	    bail_error(err);
	    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	    bail_error(err);
    }
    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		    "/nkn/nvsd/namespace/%s/origin-server/http/server-map/%d/map-name",
		    namespace, atoi(position));
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_nfs_server_map_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	if (osvr_nfs_server_map == ot) {

		err = ts_new_sprintf(&rev_cmd, "namespace %s origin-server nfs server-map \"%s\"",
				namespace, value);
		bail_error(err);

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}

	/* Consume nodes -
	 */
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);
bail:
	ts_free(&rev_cmd);
	return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_http_abs_url_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	if (osvr_http_abs_url == ot) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s origin-server http absolute-url",
				namespace);
		bail_error(err);

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_nfs_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tstring *t_port = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	if (osvr_nfs_host == ot) {

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
				NULL, &t_port, "/nkn/nvsd/namespace/%s/origin-server/nfs/port", namespace);
		bail_error(err);

		err = ts_new_sprintf(&rev_cmd, "namespace %s origin-server nfs \"%s\"",
				namespace, value);
		bail_error(err);

		if (!ts_equal_str(t_port, "2049", false)) {
			err = ts_append_sprintf(rev_cmd, " %s",
					ts_str(t_port));
			bail_error(err);
		}

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}


	/* Consume nodes - even though this is wildcarded,
	 * it is possible some stale config exists.
	 */

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/nfs/port", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/nfs/local-cache", namespace);
	bail_error(err);
bail:
	ts_free(&rev_cmd);
	ts_free(&t_port);
	return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_http_follow_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tstring *t_header = NULL, *t_node_value = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;
	char *node_name = NULL;
	tbool t_use_client = false;
	char tproxy_ip[256];

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	snprintf(tproxy_ip, sizeof(tproxy_ip), "/nkn/nvsd/namespace/%s/ip_tproxy", namespace);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_node_value, tproxy_ip, NULL);
	bail_error(err);

	if ( osvr_http_follow_header == ot ) {
		node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/header", namespace);
		bail_null(node_name);

		err = mdc_get_binding_tstr(cli_mcc,
				NULL, NULL, NULL,
				&t_header,
				node_name,
				NULL);
		bail_error(err);

		if (t_header && (ts_length(t_header) > 0)) {
			node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip", namespace);
			bail_null(node_name);
			err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
					&t_use_client, node_name, NULL);
			bail_error(err);
			if( !t_use_client) {

				err = ts_new_sprintf(&rev_cmd,
						"namespace %s origin-server http follow header \"%s\"",
						namespace, ts_str(t_header));
				bail_error(err);
			} else {
				/* ip_tproxy must not be null if use-client-ip is true */
				err = ts_new_sprintf(&rev_cmd,
						"namespace %s origin-server http follow"
						" header \"%s\" use-client-ip",
						namespace, ts_str(t_header));
				bail_error(err);
			}

			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
		}

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}
	else if ( osvr_http_dest_ip == ot ) {

		node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip", namespace);
		bail_null(node_name);
		err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_use_client, node_name, NULL);
		bail_error(err);

		if (!t_use_client) {

			err = ts_new_sprintf(&rev_cmd,
					"namespace %s origin-server http follow dest-ip ",
					namespace);
			bail_error(err);
		} else {
			/* ip_tproxy must not be null if use-client-ip is true */
			err = ts_new_sprintf(&rev_cmd,
					"namespace %s origin-server http follow "
					"dest-ip use-client-ip",
					namespace);
			bail_error(err);

		}

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes,
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}

	/* Consume nodes */
#if 0
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);
#endif
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/http/follow/header", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/http/follow/dest-ip", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/ip_tproxy", namespace);
	bail_error(err);

bail:
	safe_free(node_name);
	ts_free(&rev_cmd);
	ts_free(&t_header);
	return err;
}

/*---------------------------------------------------------------------------*/
int
cli_ns_object_show_cmd_hdlr(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_cmds = 0; // CLI param count
    uint32 ret_err = 0;
    char *node_name = NULL;
    char *uri_prefix_str = NULL;
    char *uri_prefix_str_esc = NULL;
    char *ns_uid_str = NULL;
    const char *ns_name = NULL;
    const char *uri_pattern = NULL;
    const char *proxy_domain = ALL_STR;
    const char *proxy_domain_port = NS_DEFAULT_PORT;
    tstring *ns_uid = NULL;
    tstring *ns_proxy_mode = NULL;
    tstring *uri_prefix = NULL;
    tstr_array *cmd_params = NULL;
    en_proxy_type proxy_type = REVERSE_PROXY;
    const char *remote_url = NULL;
    char *password = NULL;
    char *remote_path = NULL;
    int domain_given = 0;
    FILE *fp, *fp1;
    lt_time_ms tv;
    char *timestamp = NULL;
    char *remote_url_filename = NULL;
    char *remote_password_filename = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    /* First get the namespace name and the uri/all/pattern */
    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    uri_pattern = tstr_array_get_str_quick(cmd_line, 5);
    bail_null(uri_pattern);

    /* Initiate rate limit check for this command */
    if(strncmp(uri_pattern, "all", 3)== 0)
    {
        /* triger an action to see whether we can allow this command at this time */
    }

    /* Get the number of parameters */
    num_cmds = tstr_array_length_quick(cmd_line);

    /* Get the proxy-mode of the namespace */
    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
				&ns_proxy_mode, NULL,
				"/nkn/nvsd/namespace/%s/proxy-mode",
				ns_name);

    /* Set a flag to indicate if the namespace is reverse proxy */
    if (ts_equal_str (ns_proxy_mode, "reverse", true))
    	proxy_type = REVERSE_PROXY;
    else if (ts_equal_str (ns_proxy_mode, "mid-tier", true))
    	proxy_type = MID_TIER_PROXY;
    else
    	proxy_type = VIRTUAL_PROXY;

    /* Now check if we had a domain and port in the case of mid-tier */
    if ((REVERSE_PROXY == proxy_type) && (num_cmds > 6)  && (0 != safe_strcmp(tstr_array_get_str_quick(cmd_line, 6),"export")))
    {
        cli_printf("error: domain and port should not be given for reverse proxy-mode");
        goto bail;
    }
    /* Get the uri-prefix of the namespace */
    node_name = smprintf("/nkn/nvsd/namespace/%s/match/uri/uri_name", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
		    &uri_prefix, node_name, NULL);
    bail_error(err);
    safe_free(node_name);

    err = ts_detach(&uri_prefix, &uri_prefix_str, NULL);
    bail_error(err);

    /* Check if it is "all" if not check the uri-prefix */
    if ((strcmp(uri_pattern, "all")) &&
    		(!lc_is_prefix(uri_prefix_str, uri_pattern, false)))
    {
	    cli_printf("error: uri-prefix does not match that of this namespace");
	    goto bail;
    }

    /* Get the domain for the namespace */
    err = bn_binding_name_escape_str(uri_prefix_str, &uri_prefix_str_esc);
    bail_error_null(err, uri_prefix_str_esc);
    node_name = smprintf("/nkn/nvsd/namespace/%s/domain/host", ns_name);
    bail_null(node_name);


    /* Get the UID for the namespace */
    node_name = smprintf("/nkn/nvsd/namespace/%s/uid", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
    			&ns_uid, node_name, NULL);
    bail_error_null(err, ns_uid);


    /* Check if mid-tier or virtual and domain/port are given */
    if ((num_cmds > 6) && (0 != safe_strcmp(tstr_array_get_str_quick(cmd_line, 6),"export")))
    {
    	uint64_t port;

    	proxy_domain = tstr_array_get_str_quick(cmd_line, 6);
        bail_null(proxy_domain);
        domain_given = 1;

        proxy_domain_port = tstr_array_get_str_quick(cmd_line, 7);
        bail_null(proxy_domain_port);

        port = strtoul(proxy_domain_port, NULL,10);

        if(port > MAX_PORT || port < 1) {
        	cli_printf_error("Port value should be between 0 and 65536");
        	goto bail;
        }
    }

    /* If domain given then get extended UID */
    if (domain_given == 1)
    	ns_uid_str = get_extended_uid (ts_str(ns_uid), proxy_domain,
					proxy_domain_port);
    else
    	ns_uid_str = strdup (ts_str(ns_uid));
    bail_error_null(err, ns_uid_str);

   	err = tstr_array_new_va_str(&cmd_params, NULL, 6,
		"/opt/nkn/bin/cache_mgmt_ls.py",
		ns_name,uri_pattern, proxy_domain, ts_str(ns_uid), proxy_domain_port);
    bail_error(err);

    if (0 == safe_strcmp(tstr_array_get_str_quick(cmd_line, 6),"export"))
    {
        remote_url = tstr_array_get_str_quick(cmd_line, 7);
        bail_null(remote_url);

        if ((!lc_is_prefix("scp://", remote_url, false)) &&
                (!lc_is_prefix("sftp://", remote_url, false)) &&
                (!lc_is_prefix("ftp://", remote_url, false))){
            cli_printf_error(_("Bad destination specifier"));
            goto bail;
        }

        err = lu_parse_scp_url(remote_url, NULL, &password, NULL, NULL, NULL);

         if(err) {
             err = cli_printf_error("Invalid SCP URL format.\n");
             bail_error(err);
             goto bail;
         }

         tv = lt_curr_time();

         lt_datetime_sec_to_str(tv, false, &timestamp);
		 *(timestamp+4)='_';
		 *(timestamp+7)='_';
		 *(timestamp+4)='_';
		 *(timestamp+10)='_';

         remote_path = smprintf("%s/ns_objects_%s_%s.lst", remote_url, ns_name, timestamp);
         bail_null(node_name);

         remote_url_filename = smprintf("/nkn/ns_objects/remote_url_%s", ns_name);
         bail_null(remote_url_filename);

         fp = fopen(remote_url_filename,"w+");
         bail_null(fp);
         fprintf(fp,"%s",remote_path);
         fclose(fp);

         if(password != NULL) {
             remote_password_filename = smprintf("/nkn/ns_objects/remote_password_%s", ns_name);
             bail_null(remote_password_filename);

             fp1 = fopen(remote_password_filename,"w+");
        	 bail_null(fp1);
        	 fprintf(fp1,"%s",password);
        	 fclose(fp1);
         }
    }
    if (0 == safe_strcmp(tstr_array_get_str_quick(cmd_line, 8),"export"))
    {
        remote_url = tstr_array_get_str_quick(cmd_line, 9);
        bail_null(remote_url);

        if ((!lc_is_prefix("scp://", remote_url, false)) &&
                (!lc_is_prefix("sftp://", remote_url, false)) &&
                (!lc_is_prefix("ftp://", remote_url, false))){
            cli_printf_error(_("Bad destination specifier"));
            goto bail;
        }

        err = lu_parse_scp_url(remote_url, NULL, &password, NULL, NULL, NULL);

         if(err) {
             err = cli_printf_error("Invalid SCP URL format.\n");
             bail_error(err);
             goto bail;
         }

         tv = lt_curr_time();

         lt_datetime_sec_to_str(tv, false, &timestamp);
		 *(timestamp+4)='_';
		 *(timestamp+7)='_';
		 *(timestamp+4)='_';
		 *(timestamp+10)='_';

         remote_path = smprintf("%s/ns_objects_%s_%s.lst", remote_url, ns_name, timestamp);
         bail_null(node_name);

         remote_url_filename = smprintf("/nkn/ns_objects/remote_url_%s", ns_name);
         bail_null(remote_url_filename);

         fp = fopen(remote_url_filename,"w+");
         bail_null(fp);
         fprintf(fp,"%s",remote_path);
         fclose(fp);

         if(password != NULL) {
             remote_password_filename = smprintf("/nkn/ns_objects/remote_password_%s", ns_name);
             bail_null(remote_password_filename);

             fp1 = fopen(remote_password_filename,"w+");
        	 bail_null(fp1);
        	 fprintf(fp1,"%s",password);
        	 fclose(fp1);
         }
    }

    /* Now call the function with the command */
    err = clish_paging_suspend();

   	err = nkn_clish_run_program_fg("/opt/nkn/bin/cache_mgmt_ls.py", cmd_params, NULL, NULL, NULL);

bail:
    safe_free(node_name);
    safe_free(uri_prefix_str);
    safe_free(ns_uid_str);
    safe_free(password);
    tstr_array_free(&cmd_params);
    safe_free(remote_path);
    return err;

} /* end of cli_ns_object_show_cmd_hdlr () */

/*---------------------------------------------------------------------------*/
/*
* Reval command handler,
* Is valid only for a T-Proxy namespace,
* Same hdlr used to set and unset.
*/
static int
cli_nvsd_origin_fetch_reval_always(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *namespace = NULL;
	tstring *t_proxy_mode = NULL;
	char *bn_node = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	namespace = tstr_array_get_str_quick(params, 0);
	bail_null(namespace);

	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
			&t_proxy_mode, NULL,
			"/nkn/nvsd/namespace/%s/proxy-mode",
			namespace);
	bail_error_null(err, t_proxy_mode);

	/* This command is allowed only for a transparent namespace */
	if((strcmp(ts_str(t_proxy_mode),"transparent") == 0) ||
	    (strcmp(ts_str(t_proxy_mode),"virtual") == 0)) {
		/* Set time to future date 25 yeasrs from now ,till we figure out a better a logic from backend */
		/* 2541441258 -> Thu, 14 Jul 2050 19:54:18 GMT  */

		const char *type = NULL;
		const char *reval_time = NULL;

		if (cmd->cc_magic & cc_clreq_reval_set) {
			if (cmd->cc_magic & cc_clreq_reval_inline)
				type = "all_inline";
			else
				type = "all_offline";
			reval_time = "2541441258";
		} else {
			type = " ";
			reval_time = "0";
		}

		bn_node = smprintf("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/reval_type",
				namespace);
		bail_null(bn_node);

		err = mdc_set_binding(cli_mcc,
				NULL, NULL, bsso_modify,
				bt_string,
				type,
				bn_node);
		bail_error(err);
		safe_free(bn_node);

		bn_node = smprintf("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/time_barrier",
				namespace);
		bail_null(bn_node);

		err = mdc_set_binding(cli_mcc,
				NULL, NULL, bsso_modify,
				bt_string,
				reval_time,
				bn_node);
		bail_error(err);
		safe_free(bn_node);

	} else {
		cli_printf_error(_("This command is supported only for a T-Proxy namespace\n"));
	}
bail:
	safe_free(bn_node);
	ts_free(&t_proxy_mode);
	return err;


}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_origin_req_cache_revalidate_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	uint32 err_code = 0;
	const char *namespace = NULL;
	char *bn_node = NULL;
	const char *state = NULL;
	const char *date_hdr = NULL;
	const char *method = NULL;
	const char *header_name = NULL;
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);

	namespace = tstr_array_get_str_quick(params, 0);
	bail_null(namespace);

	/* Get the cache-revalidation state - permit/deny */
	state = tstr_array_get_str_quick(cmd_line, 7);
	bail_null(state);

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);


	if (lc_is_prefix(state, "permit", true)) {

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
				0, bt_bool, 0, "true", NULL,
				"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate",
				namespace);
		bail_error(err);


		/* Check if use-date-header is given */
		if (tstr_array_length_quick(cmd_line) > 8) {
			date_hdr = tstr_array_get_str_quick(cmd_line, 8);
			bail_null(date_hdr);

			if (lc_is_prefix(date_hdr, "use-date-header", true)) {

				err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
						0, bt_bool, 0, "true", NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/use-date-header",
						namespace);
				bail_error(err);
			} else if (lc_is_prefix(date_hdr, "method", true)) {
				// Maybe the user is trying to set the method to use
				method = tstr_array_get_str_quick(cmd_line, 9);
				if (lc_is_prefix(method, "head", true)) {
					err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
						0, bt_string, 0, "head", NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/method",
						namespace);
					bail_error(err);
				}
				else {
					err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
						0, bt_string, 0, "get", NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/method",
						namespace);
					bail_error(err);
				}

				// Set if validate-header is also used
				if (tstr_array_length_quick(params) == 2) {
					header_name = tstr_array_get_str_quick(params, 1);
					bail_null(header_name);

					if (strlen(header_name) == 0){
						cli_printf_error(_("Header Name cannot be Empty \n"));
						goto bail;
					}

					err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
						0, bt_string, 0, header_name, NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header/header-name",
						namespace);
					bail_error(err);

					err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
						0, bt_bool, 0, "true", NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header",
						namespace);
					bail_error(err);

				} else {
					// No validate-header was given.. reset it in the config
					err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset,
						0, bt_bool, 0, "false", NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header",
						namespace);
					bail_error(err);

					err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset,
						0, bt_string, 0, "etag", NULL,
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header/header-name",
						namespace);
					bail_error(err);
				}
			}
			else if (lc_is_prefix(date_hdr, "flush-caches", true)) {
				bn_node = smprintf(
						"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/flush-intermediate-cache",
						namespace);
				bail_null(bn_node);

				err = mdc_set_binding(cli_mcc,
						NULL, NULL,
						bsso_modify,
						bt_bool,
						"true",
						bn_node);
				bail_error(err);

			}
			else {
				cli_printf_error(_("Unrecognized command \"%s\"\n"), date_hdr);
				goto bail;
			}
		}
	} else if (lc_is_prefix(state, "deny", true)) {

		// Reset all values

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify,
			0, bt_bool, 0, "false", NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate",
			namespace);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset,
			0, bt_bool, 0, "false", NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/use-date-header",
			namespace);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset,
			0, bt_string, 0, "head", NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/method",
			namespace);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset,
			0, bt_bool, 0, "false", NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header",
			namespace);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset,
			0, bt_string, 0, "", NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header/header-name",
			namespace);
		bail_error(err);

		safe_free(bn_node);


		bn_node = smprintf(
				"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/flush-intermediate-cache",
				namespace);
		bail_null(bn_node);

		err = mdc_set_binding(cli_mcc,
				NULL, NULL,
				bsso_reset,
				bt_bool,
				"false",
				bn_node);
		bail_error(err);

	} else {
		cli_printf_error(_("Unrecognized command \"%s\"\n"), state);
		goto bail;
	}

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &err_code, NULL);
	bail_error(err);

bail:
	safe_free(bn_node);
	bn_request_msg_free(&req);
	return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_origin_req_cache_revalidation_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	tbool t_permitted = false;
	tstring *t_date_hdr = NULL;
	tstring *t_flush_cache = NULL;
	tstring *t_method = NULL;
	tstring *t_validate = NULL;
	tstring *t_validate_header = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	err = lc_str_to_bool(value, &t_permitted);
	bail_error(err);

	if (t_permitted) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s delivery protocol http origin-request cache-revalidation permit",
				namespace);
		bail_error(err);

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_date_hdr,
				"%s/use-date-header", name);
		bail_error_null(err, t_date_hdr);

		if (ts_equal_str(t_date_hdr, "true", false)) {
			err = ts_append_sprintf(rev_cmd, " use-date-header");
			bail_error(err);
		}

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_flush_cache,
				"%s/flush-intermediate-cache", name);
		bail_error(err);

		if (t_flush_cache && ts_equal_str(t_flush_cache, "true", false)) {

			err = ts_new_sprintf(&rev_cmd,
				    "namespace %s delivery protocol http origin-request cache-revalidation permit flush-caches",
				    namespace);
			bail_error(err);

			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
			ts_free(&rev_cmd);
		}


		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_method,
				"%s/method", name);
		bail_error(err);

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_validate,
				"%s/validate-header", name);
		bail_error(err);

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_validate_header,
				"%s/validate-header/header-name", name);
		bail_error(err);

		err = ts_new_sprintf(&rev_cmd,
			"namespace %s delivery protocol http origin-request cache-revalidation permit method %s",
			namespace, ts_str(t_method));
		bail_error(err);

		if (ts_equal_str(t_validate, "true", false)) {
		    err = ts_append_sprintf(rev_cmd, " validate-header %s", ts_str(t_validate_header));
		    bail_error(err);
		}

	} else {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s delivery protocol http origin-request cache-revalidation deny",
				namespace);
		bail_error(err);
	}

	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/use-date-header", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/flush-intermediate-cache", name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/method", name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/validate-header", name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/validate-header/header-name", name);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	ts_free(&t_date_hdr);
	ts_free(&t_flush_cache);
	ts_free(&t_method);
	ts_free(&t_validate);
	ts_free(&t_validate_header);
	return err;
}

static int
cli_nvsd_ns_client_request_header_accept_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	const char *n = NULL;
	tstring *t_name = NULL;
	tstring *rev_cmd = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	n = tstr_array_get_str_quick(name_parts, 3);
	bail_null(n);
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_name, 
			"%s/name", name);
	bail_error_null(err, t_name);

	err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request header \"%s\" action accept",
			n, ts_str(t_name));
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/name", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_cmds, "%s", ts_str(rev_cmd));
	bail_error(err);

bail:
	ts_free(&t_name);
	ts_free(&rev_cmd);
	return err;
}

int
cli_nvsd_ns_client_request_header_accept_hdlr(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	char *node_name = NULL;
	bn_binding_array *barr = NULL;
	bn_binding *binding = NULL;
	uint32 code = 0;
	uint32_array *header_indices = NULL;
	int32 db_rev = 0;
	uint32 num_headers = 0;
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);

	node_name = smprintf("/nkn/nvsd/namespace/%s/client-request/header/accept",
			tstr_array_get_str_quick(params, 0));
	bail_null(node_name);

	switch(cmd->cc_magic)
	{
	case cc_clreq_header_accept_delete_one:
		err = mdc_array_delete_by_value_single(cli_mcc,
				node_name,
				true, "name", bt_string,
				tstr_array_get_str_quick(params, 1), NULL, NULL, NULL);
		bail_error(err);

		break;
	case cc_clreq_header_accept_add_one:
		/* Check if the type already exists.. If it does, then
		 * we need to update the value,
		 * If it doesnt exist, then create it and then set the
		 * value
		 */

		err = bn_binding_array_new(&barr);
		bail_error(err);

		err = bn_binding_new_str_autoinval(&binding, "name", ba_value,
				bt_string, 0, tstr_array_get_str_quick(params, 1));
		bail_error(err);

		err = bn_binding_array_append_takeover(barr, &binding);
		bail_error(err);

		/* Get all matching indices */
		err = mdc_array_get_matching_indices(cli_mcc,
				node_name, NULL, barr, bn_db_revision_id_none,
				NULL, &header_indices, NULL, NULL);
		bail_error(err);

		num_headers = uint32_array_length_quick(header_indices);
		if ( num_headers == 0 ) {
			err = mdc_array_append(cli_mcc,
				node_name, barr, true, 0, NULL, NULL, &code, NULL);
			bail_error(err);
		}
		err = cli_nvsd_ns_warn214_header_mods(tstr_array_get_str_quick(params, 1));
		bail_error(err);
		break;
	}

bail:
	safe_free(node_name);
	bn_binding_array_free(&barr);
	bn_binding_free(&binding);
	return err;
}

#if 0
static int
cli_nvsd_client_request_header_accept_add(const char *name,
                                int32 db_dev,
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                uint32 *ret_code)
{
	int err = 0;
	bn_binding_array *barr = NULL;
	bn_binding *binding = NULL;
	uint32 code = 0;
	char *node_name = NULL;

	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(db_dev);

	bail_null(name);

	err = bn_binding_array_new(&barr);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&binding, "name",
		ba_value, bt_string, 0,
		tstr_array_get_str_quick(params, 1));
	bail_error_null(err, binding);

	err = bn_binding_array_append(barr, binding);
	bail_error(err);

	// Set timeout
	err = bn_binding_new_str_autoinval(&binding, "value",
		ba_value, bt_string, 0,
		"");
	bail_error_null(err, binding);

	err = bn_binding_array_append(barr, binding);
	bail_error(err);

	err = mdc_array_append(cli_mcc,
		name, barr, true, 0, NULL, NULL, &code, NULL);
	bail_error(err);

bail:
	safe_free(node_name);
	bn_binding_free(&binding);
	bn_binding_array_free(&barr);
	if(ret_code) {
		*ret_code = code;
	}
	return err;
}
#endif

/*---------------------------------------------------------------------------*/
int
cli_nvsd_ns_origin_request_header_add(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	char *node_name = NULL;
	bn_binding_array *barr = NULL;
	bn_binding *binding = NULL;
	uint32 code = 0;
	uint32_array *header_indices = NULL;
	int32 db_rev = 0;
	uint32 num_headers = 0;
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-request/header/add",
			tstr_array_get_str_quick(params, 0));
	bail_null(node_name);

	switch(cmd->cc_magic)
	{
	case cc_oreq_header_delete_one:

		err = mdc_array_delete_by_value_single(cli_mcc,
				node_name,
				true, "name", bt_string,
				tstr_array_get_str_quick(params, 1), NULL, NULL, NULL);
		bail_error(err);

		break;
	case cc_oreq_header_add_one:
		/* Check if the type already exists.. If it does, then
		 * we need to update the value,
		 * If it doesnt exist, then create it and then set the
		 * value
		 */

		err = bn_binding_array_new(&barr);
		bail_error(err);

		err = bn_binding_new_str_autoinval(&binding, "name", ba_value,
				bt_string, 0, tstr_array_get_str_quick(params, 1));
		bail_error(err);

		err = bn_binding_array_append_takeover(barr, &binding);
		bail_error(err);

		/* Get all matching indices */
		err = cli_nvsd_ns_mdc_array_get_matching_indices(cli_mcc,
				node_name, NULL, tstr_array_get_str_quick(params, 1),
				bn_db_revision_id_none,
				NULL, &header_indices, NULL, NULL);
		bail_error(err);

		num_headers = uint32_array_length_quick(header_indices);
		if ( num_headers == 0 ) {
			err = cli_nvsd_origin_request_header_add(node_name,
					db_rev, cmd_line, params, &code);
			bail_error(err);
		}
		else {
			err = bn_set_request_msg_create(&req, 0);
			bail_error(err);

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
					bt_string, 0, tstr_array_get_str_quick(params, 2),
					NULL, "%s/%d/value",
					node_name,
					uint32_array_get_quick(header_indices, 0));
			bail_error(err);

			err = bn_set_request_msg_set_option_cond_db_revision_id(req, db_rev);
			bail_error(err);

			err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
			bail_error(err);
		}
		break;
	}

bail:
	bn_request_msg_free(&req);
	safe_free(node_name);
	bn_binding_array_free(&barr);
	bn_binding_free(&binding);
	return err;
}


/*---------------------------------------------------------------------------*/
static int
cli_nvsd_origin_request_header_add(const char *name,
                                int32 db_dev,
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                uint32 *ret_code)
{
	int err = 0;
	bn_binding_array *barr = NULL;
	bn_binding *binding = NULL;
	uint32 code = 0;
	char *node_name = NULL;

	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(db_dev);

	bail_null(name);

	err = bn_binding_array_new(&barr);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&binding, "name",
		ba_value, bt_string, 0,
		tstr_array_get_str_quick(params, 1));
	bail_error_null(err, binding);

	err = bn_binding_array_append(barr, binding);
	bail_error(err);

	// Set timeout
	err = bn_binding_new_str_autoinval(&binding, "value",
		ba_value, bt_string, 0,
		tstr_array_get_str_quick(params, 2));
	bail_error_null(err, binding);

	err = bn_binding_array_append(barr, binding);
	bail_error(err);

	err = mdc_array_append(cli_mcc,
		name, barr, true, 0, NULL, NULL, &code, NULL);
	bail_error(err);

bail:
	safe_free(node_name);
	bn_binding_free(&binding);
	bn_binding_array_free(&barr);
	if(ret_code) {
		*ret_code = code;
	}
	return err;
}


/*---------------------------------------------------------------------------*/
int
cli_nvsd_ns_origin_request_header_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	char *c_header = NULL;
	char *c_value = NULL;
	tstring *t_header = NULL;
	tstring *t_value = NULL;
	const char *cc_namespace = NULL;
	tstring *rev_cmd = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);


	c_header = smprintf("%s/name", name);
	bail_null(c_header);

	c_value = smprintf("%s/value", name);
	bail_null(c_value);


	cc_namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(cc_namespace);

	err = bn_binding_array_get_value_tstr_by_name(bindings, c_header, NULL, &t_header);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name(bindings, c_value, NULL, &t_value);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_nodes, c_header);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_nodes, c_value);
	bail_error(err);

	err = ts_new_sprintf(&rev_cmd,
		"namespace %s delivery protocol http origin-request header %s \"%s\" action add",
		cc_namespace, ts_str(t_header), ts_str(t_value));
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	safe_free(c_value);
	safe_free(c_header);
	return err;
}


/*---------------------------------------------------------------------------*/
int
cli_nvsd_ns_client_request_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tbool is_cacheable = false;
	tbool is_strip = false;
	const char *namespace = NULL;
	char *node_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	node_name = smprintf("%s/query/cache/enable", name);
	bail_null(node_name);


	err = bn_binding_array_get_value_tbool_by_name(bindings, node_name, NULL, &is_cacheable);
	bail_error(err);
	safe_free(node_name);

	node_name = smprintf("%s/query/strip", name);
	bail_null(node_name);

	err = bn_binding_array_get_value_tbool_by_name(bindings, node_name, NULL, &is_strip);
	bail_error(err);
	safe_free(node_name);

	if ( !is_cacheable ) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request query-string action no-cache",
				namespace);
		bail_error(err);
	} else {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request query-string action cache",
				namespace);
		bail_error(err);

		if (is_strip) {
			ts_free(&rev_cmd);
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request query-string action cache exclude-query-string", namespace);
			bail_error(err);
		}
	}

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/query/cache/enable", name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/query/strip", name);
	bail_error(err);

bail:
	safe_free(node_name);
	ts_free(&rev_cmd);
	return err;
}



/*---------------------------------------------------------------------------*/
int
cli_nvsd_ns_client_response_header_add(void *data,
		const cli_command 	*cmd,
		const tstr_array 	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;

	char *node_name = NULL;;
	bn_binding_array *barr = NULL;
	bn_binding *binding = NULL;
	uint32 code = 0;
	uint32_array *header_indices = NULL;
	int32 db_rev = 0;
	uint32 num_headers = 0;
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);

	node_name = smprintf("/nkn/nvsd/namespace/%s/client-response/header/add",
			tstr_array_get_str_quick(params, 0));

	switch(cmd->cc_magic)
	{
	case cc_clreq_header_delete_one:
		err = mdc_array_delete_by_value_single(cli_mcc,
				node_name,
				true, "name", bt_string,
				tstr_array_get_str_quick(params, 1), NULL, NULL, NULL);
		bail_error(err);

		break;

	case cc_clreq_header_add_one:

		/* Check if the type already exists.. If it does, then
		 * we need to update the value,
		 * If it doesnt exist, then create it and then set the
		 * value
		 */
		err = bn_binding_array_new(&barr);
		bail_error(err);

		err = bn_binding_new_str_autoinval(&binding, "name", ba_value,
				bt_string, 0, tstr_array_get_str_quick(params, 1));
		bail_error(err);

		err = bn_binding_array_append_takeover(barr, &binding);
		bail_error(err);

		/* Get all matching indices */
		err = cli_nvsd_ns_mdc_array_get_matching_indices(cli_mcc,
				node_name, NULL, tstr_array_get_str_quick(params, 1),
				bn_db_revision_id_none,
				NULL, &header_indices, NULL, NULL);
		bail_error(err);

		num_headers = uint32_array_length_quick(header_indices);
		if ( num_headers == 0 ) {
			err = cli_nvsd_origin_request_header_add(node_name,
					db_rev, cmd_line, params, &code);
			bail_error(err);
		}
		else {
			err = bn_set_request_msg_create(&req, 0);
			bail_error(err);

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0, 
					bt_string, 0, tstr_array_get_str_quick(params, 2),
					NULL, "%s/%d/value", 
					node_name, 
					uint32_array_get_quick(header_indices, 0));
			bail_error(err);

			err = bn_set_request_msg_set_option_cond_db_revision_id(req, db_rev);
			bail_error(err);

			err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
			bail_error(err);
		}

		err = cli_nvsd_ns_warn214_header_mods(tstr_array_get_str_quick(params, 1));
		bail_error(err);
		break;
	}

bail:
	return err;
}



/*---------------------------------------------------------------------------*/
int
cli_nvsd_ns_client_response_header_delete(void *data,
		const cli_command 	*cmd,
		const tstr_array 	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	char *node_name = NULL;;
	bn_binding_array *barr = NULL;
	bn_binding *binding = NULL;
	uint32 code = 0;
	uint32_array *header_indices = NULL;
	uint32 num_headers = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	node_name = smprintf("/nkn/nvsd/namespace/%s/client-response/header/delete",
			tstr_array_get_str_quick(params, 0));

	switch(cmd->cc_magic)
	{
	case cc_clreq_header_delete_one:
		err = mdc_array_delete_by_value_single(cli_mcc, 
				node_name, 
				true, "name", bt_string, 
				tstr_array_get_str_quick(params, 1), NULL, NULL, NULL);
		bail_error(err);

		break;

	case cc_clreq_header_add_one:
		
		/* Check if the type already exists.. If it does, then
		 * we need to update the value, 
		 * If it doesnt exist, then create it and then set the
		 * value 
		 */
		err = bn_binding_array_new(&barr);
		bail_error(err);


		err = bn_binding_new_str_autoinval(&binding, "name", ba_value,
				bt_string, 0, tstr_array_get_str_quick(params, 1));
		bail_error(err);

		err = bn_binding_array_append_takeover(barr, &binding);
		bail_error(err);

		/* Get all matching indices */
		err = mdc_array_get_matching_indices(cli_mcc, 
				node_name, NULL, barr, bn_db_revision_id_none, 
				NULL, &header_indices, NULL, NULL);
		bail_error(err);

		num_headers = uint32_array_length_quick(header_indices);
		if ( num_headers == 0 ) {

			err = mdc_array_append(cli_mcc,
				node_name, barr, true, 0, NULL, NULL, &code, NULL);
			bail_error(err);
		}

		err = cli_nvsd_ns_warn214_header_mods(tstr_array_get_str_quick(params, 1));
		bail_error(err);

		break;
	}

bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_origin_fetch_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;
	char *pattern = NULL;
	unsigned int num_matches = 0;
	bn_binding_array *bindings = NULL;
	bn_binding *bn_origin_fetch_obj_size = NULL;
	bn_binding *bn_origin_fetch_policy_type = NULL;
	uint32 origin_fetch_threshold = 0;
	uint32 policy_type = 0;
	char *date_modify_node_name = NULL;
	tbool t_date_modify = false;
	tbool t_part_fetch = false, t_node_value = false;
	char *eod_on_close_node_name = NULL;
	char *custom_header_node = NULL;
	tstring *custom_header = NULL;
	char *redirect_302_node = NULL;
	tbool redirect_302 = false;
	char node_name[256];
	tstring *ts_node_value = NULL;
	tbool t_set_cookie = false;
	tbool t_set_cookie_no_reval = false;
	uint32 cache_fill = 0;
	tbool t_tor_cc_notrans= false;
   	tbool t_tor_obj_expired = false;
	char *node_name_tunnel = NULL;
	char etag_node_name[256];
	char validatorsall_node_name[256];
	tbool t_etag_enabled = false;
	tbool t_validatorsall_enabled = false;
	char s_maxage_config_node[256];
	tbool s_maxage_ignore = false;
	tbool tbool_non206_origin = false;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	err = cli_printf(
	    _("\n  Origin Fetch Configuration:\n"));
	bail_error(err);

	node_name[0] = '\0';
	snprintf(node_name, sizeof(node_name), 
			"/nkn/nvsd/origin_fetch/config/%s/byte-range/preserve", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_node_value, node_name, NULL);
	bail_error(err);

	if (t_node_value) {
	node_name[0] = '\0';
	snprintf(node_name, sizeof(node_name), 
			"/nkn/nvsd/origin_fetch/config/%s/byte-range/align", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_node_value, node_name, NULL);
	bail_error(err);
	if (t_node_value) {
		err = cli_printf(_("    Byte Range : preserve (aligned)\n"));
		bail_error(err);
	} else {
		err = cli_printf(_("    Byte Range : preserve (not aligned)\n"));
		bail_error(err);
	}
	} else {
		err = cli_printf( _("    Byte Range : ignore\n"));
		bail_error(err);
	}

	err = cli_printf_query(
	    _("    Cache-Age (Default): "
		"#/nkn/nvsd/origin_fetch/config/%s/cache-age/default# (seconds)\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("    Cache-ingest Hotness Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/cache-ingest/hotness-threshold# \n"), ns);
	bail_error(err);


	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/cache-ingest/byte-range-incapable", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
					&tbool_non206_origin, node_name, NULL);
    	bail_error(err);
        err = cli_printf(
            _("    Cache-ingest Byte-Range Incapable Origin: %s\n"),
               (tbool_non206_origin == 1) ? "Yes" : "No");
        bail_error(err);



	err = cli_printf_query(
	    _("    Cache-ingest Size Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/cache-ingest/size-threshold# \n"), ns);
	bail_error(err);


	node_name[0] = '\0';
	snprintf(node_name, sizeof(node_name),
			"/nkn/nvsd/origin_fetch/config/%s/cache-fill", ns);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
			&ts_node_value, node_name, NULL);
	bail_error(err);

	cache_fill = atoi(ts_str(ts_node_value));
	ts_free(&ts_node_value);

	if (1 == cache_fill ) {
		cli_printf("    Cache-Fill : Aggressive\n");
	} else if (2 == cache_fill) {
		cli_printf("    Cache-Fill : Client-Driven\n");

		err = cli_printf_query(
		    _("    Client-Driven Aggressive Threshold: "
			"#/nkn/nvsd/origin_fetch/config/%s/cache-fill/client-driven/aggressive_threshold# \n"), ns);
		bail_error(err);

	} else {
		/* do nothing */
		cli_printf("    Cache-Fill : None\n");
	}
	pattern = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type/*", ns);
	bail_null(pattern);

	err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
	    &bindings, true, true, pattern);
	bail_error(err);
	err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
	    cli_nvsd_origin_fetch_cache_age_show_one, NULL, &num_matches);
	bail_error(err);
	safe_free(pattern);
	bn_binding_array_free(&bindings);

	if (num_matches > 0) {
	cli_printf(_(
		    "      ---------------------------------------------\n\n"));
	}
	
	err = cli_printf_query(
	    _("    Cache Age Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/content-store/media/cache-age-threshold# (seconds)\n"), ns);
	bail_error(err);


	err = cli_printf_query(
	    _("    Cache-Directive: "
		"#/nkn/nvsd/origin_fetch/config/%s/cache-directive/no-cache#\n"), ns);
	bail_error(err);

	err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_origin_fetch_obj_size, 
	    NULL, "/nkn/nvsd/origin_fetch/config/%s/content-store/media/object-size-threshold", ns);
	bail_error(err);

	err = bn_binding_get_uint32(bn_origin_fetch_obj_size, ba_value, NULL, &origin_fetch_threshold);
	bail_error(err);
	if (origin_fetch_threshold) {
	err = cli_printf(
		_("    Object Size Threshold: %u Bytes\n"), origin_fetch_threshold);
	bail_error(err);
	} else {
	err = cli_printf(
		_("    Object Size Threshold: NONE (Always Cached)\n"));
	bail_error(err);

	}
	err = cli_printf_query(
	    _("    Object Size Minimum Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/obj-thresh-min# (KiB)\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("    Object Size Maximum Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/obj-thresh-max# (KiB)\n"), ns);
	bail_error(err);

	date_modify_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/date-header/modify",ns);
	bail_null(date_modify_node_name);

	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
		    &t_date_modify, date_modify_node_name, NULL);
	bail_error(err);

	err = cli_printf(
	    _("    Modify Date Header: %s\n"),
	       t_date_modify ? "permit" : "deny");
	bail_error(err);

	err = cli_printf_query(
	    _("    URI Depth Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/content-store/media/uri-depth-threshold#\n"), ns);
	bail_error(err);


	eod_on_close_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/detect_eod_on_close", 
			ns);
	bail_null(eod_on_close_node_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
			&t_part_fetch, eod_on_close_node_name, NULL);
	bail_error(err);

	err = cli_printf(
	    _("    Treat End of Transaction on connection close (partial-fetch): %s\n"),
		t_part_fetch ? "Yes" : "No");
	bail_error(err);

   	custom_header_node = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-control/custom-header", ns);
	bail_null(custom_header_node);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		    &custom_header, custom_header_node, NULL);
    	bail_error(err);

	if (ts_nonempty(custom_header))
	{
		err = cli_printf(
				_("    Custom Cache Control Header: %s \n"), ts_str(custom_header));
		bail_error(err);
	}
	else
	{
		err = cli_printf(
				_("    Custom Cache Control Header: None \n"));
		bail_error(err);
	
	}

	redirect_302_node = smprintf("/nkn/nvsd/origin_fetch/config/%s/redirect_302", ns);
	bail_null(redirect_302_node);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
		    &redirect_302, redirect_302_node, NULL);

	if (redirect_302)
	{
		err = cli_printf(_("    Redirect-302 : Handle\n"));
		bail_error(err);
	}else {
		err = cli_printf(_("    Redirect-302 : Pass-through\n"));
                bail_error(err);
	}

	snprintf(node_name, sizeof(node_name),
			"/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_set_cookie, node_name, NULL);
	bail_error(err);

	snprintf(node_name, sizeof(node_name),
			"/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache/no-revalidation", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_set_cookie_no_reval, node_name, NULL);
	bail_error(err);

        err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_origin_fetch_policy_type,
            NULL, "/nkn/nvsd/origin_fetch/config/%s/ingest-policy", ns);
        bail_error(err);
	
	if (t_set_cookie_no_reval)
	{
		err = cli_printf( _("    Cache Set-Cookie Header : Permit - No Revalidation\n"));
			bail_error(err);
	}
	else
	{
		err = cli_printf( _("    Cache Set-Cookie Header : %s\n"),
				t_set_cookie? "Permit" : "Deny");
		bail_error(err);
	}

        err = bn_binding_get_uint32(bn_origin_fetch_policy_type, ba_value, NULL, &policy_type);
        bail_error(err);

        err = cli_printf(
            _("    Packing Policy type : %s\n"),
               (policy_type == 1) ? "lifo" : "fifo");
        bail_error(err);



        err = cli_printf(
            _("    Tunnel-Override Configuration:\n"));
        bail_error(err);

    	node_name_tunnel = smprintf("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/response/cc-notrans", ns);
    	bail_null(node_name_tunnel);

    	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
    			&t_tor_cc_notrans, node_name_tunnel, NULL);
    	bail_error(err);

    	safe_free(node_name_tunnel);

    	if (t_tor_cc_notrans){
    	err = cli_printf( _("    Cache-control-no-transform\n"));
    	bail_error(err);
    	}

    	node_name_tunnel = smprintf("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/response/obj-expired", ns);
    	bail_null(node_name);

    	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
    			&t_tor_obj_expired, node_name_tunnel, NULL);
    	bail_error(err);

    	safe_free(node_name_tunnel);

    	if (t_tor_obj_expired){
    	err = cli_printf( _("    Object-expired\n"));
    	bail_error(err);
    	}

        snprintf(etag_node_name, sizeof(etag_node_name),
                        "/nkn/nvsd/origin_fetch/config/%s/object/correlation/etag/ignore", ns);
        err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                        &t_etag_enabled, etag_node_name, NULL);
        bail_error(err);

        err = cli_printf( _("    Object Correlation Etag Ignore : %s\n"),
                t_etag_enabled? "Yes" : "No");
        bail_error(err);

        snprintf(validatorsall_node_name, sizeof(validatorsall_node_name),
                        "/nkn/nvsd/origin_fetch/config/%s/object/correlation/validatorsall/ignore", ns);
        err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                        &t_validatorsall_enabled, validatorsall_node_name, NULL);
        bail_error(err);

        err = cli_printf( _("    Object Correlation Validators-all Ignore : %s\n"),
                t_validatorsall_enabled? "Yes" : "No");
        bail_error(err);

        snprintf(s_maxage_config_node, sizeof(s_maxage_config_node),
                        "/nkn/nvsd/origin_fetch/config/%s/cache-control/s-maxage-ignore", ns);
        err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                        &s_maxage_ignore , s_maxage_config_node, NULL);
        bail_error(err);

        err = cli_printf( _("    Ignore s-maxage header : %s\n"),
        		s_maxage_ignore ? "Yes" : "No");
        bail_error(err);

	num_matches = 0;
	pattern = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache/response/*", ns);
	bail_null(pattern);

	err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
	    &bindings, true, true, pattern);
	bail_error(err);
	
	if(bindings) {
		err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
		    cli_nvsd_origin_fetch_unsuccessful_cache_age_show_one, NULL, &num_matches);
		bail_error(err);
	}

	safe_free(pattern);
	bn_binding_array_free(&bindings);

	if (num_matches > 0) {
	cli_printf(_(
		    "      ---------------------------------------------\n\n"));
	}
bail:
	safe_free(date_modify_node_name);
	safe_free(pattern);
	bn_binding_array_free(&bindings);
	bn_binding_free(&bn_origin_fetch_obj_size);
	safe_free(eod_on_close_node_name);
	return err;
}


/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_origin_request_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;
	char *cache_revalidate_node_name = NULL;
	tbool t_cache_revalidate = false;
	char *client_headers_action_node_name = NULL;
	tbool t_client_headers_action = false;
	char *flush_node_name = NULL;
	tbool t_cache_flush = false;
	char *headto_get_node_name = NULL;
	char *host_inherit_node_name = NULL;
	tbool t_host_header_inherit = false;
	char *pattern = NULL;
	bn_binding_array *bindings = NULL;
	unsigned int num_matches = 0;
	tstring *t_method = NULL;
	tstring *t_validate = NULL;
	tstring *t_validate_header = NULL;
	node_name_t node_name = {0};
	tbool t_is_secure = false;
	tbool t_is_plaintext = false;

	char *ns_origin_node = NULL;
	tstring *ns_origin_type = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	err = cli_printf(
	    _("\n  Origin Request Configuration:\n"));
	bail_error(err);


	/*Does the namespace support http / https*/
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/origin-request/secure", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
			&t_is_secure, node_name, NULL);
	bail_error(err);

	err = cli_printf( _("    Secure Origin Fetch(HTTPS over SSL):%s\n"),
			t_is_secure ? "Enable" : "Disable");
	bail_error(err);

	cache_revalidate_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate", ns);
	bail_null(cache_revalidate_node_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_cache_revalidate, cache_revalidate_node_name, NULL);
	bail_error(err);

	err = cli_printf_query(
		_("    Cache revalidate: %s\n"),
		t_cache_revalidate ? "permit" : "deny");
	bail_error(err);

	client_headers_action_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/client-header-inherit", ns);
	bail_null(client_headers_action_node_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_client_headers_action, client_headers_action_node_name, NULL);
	bail_error(err);

	err = cli_printf_query(
		_("    Cache revalidation - Client Headers action : %s\n"),
		t_client_headers_action ? "Include" : "Exclude");
	bail_error(err);

	flush_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/flush-intermediate-cache", ns);
	bail_null(flush_node_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
		    &t_cache_flush, flush_node_name, NULL);
	bail_error(err);
	err = cli_printf_query(
		_("    Force end-to-end cache revalidation: %s\n"),
		t_cache_flush ? "Enabled" : "Disabled");
	bail_error(err);

	err = cli_printf_query(
		_("      Use 'Date' Header when Last-Modified is not present: "
			"#/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/use-date-header#\n"),
		ns);
	bail_error(err);

	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
			&t_method, NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/method",
			ns);
	bail_error(err);
	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
			&t_validate, NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header",
			ns);
	bail_error(err);
	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
			&t_validate_header, NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header/header-name",
			ns);
	bail_error(err);

	err = cli_printf(
		_("      HTTP Request Method: %s\n"),
		ts_equal_str(t_method, "get", true) ? "GET" : "HEAD");
	bail_error(err);

	if (ts_equal_str(t_validate, "true", true)) {
		err = cli_printf(
			_("      HTTP response header to use to validate object : %s\n"),
			ts_str(t_validate_header));
		bail_error(err);
	}



	host_inherit_node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/host-header/inherit", ns);
	bail_null(host_inherit_node_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_host_header_inherit, host_inherit_node_name, NULL);
	bail_error(err);
	err = cli_printf_query(
		_("    Host-header Inherit: %s\n"), 
		t_host_header_inherit ? "permit" : "deny");
	bail_error(err);

        err = cli_printf_query(
                _("    Host-header value: " 
                        "#/nkn/nvsd/origin_fetch/config/%s/host-header/header-value#\n"),
                ns);
        bail_error(err);


	err = cli_printf_query(
		_("    Set X-Forwaded-For Header : #/nkn/nvsd/namespace/%s/origin-request/x-forwarded-for/enable#\n"),
		ns);
	bail_error(err);

         err = cli_printf_query(_("    Set vary-header : #/nkn/nvsd/origin_fetch/config/%s/header/vary#\n"),
                ns);
        bail_error(err);

	err = cli_printf("  Vary-Header value for: \n");
	err = cli_printf_query(
                _("    PC : #/nkn/nvsd/origin_fetch/config/%s/vary-header/pc#\n"),
                ns);
        bail_error(err);

        err = cli_printf_query(
                _("    Tablet : #/nkn/nvsd/origin_fetch/config/%s/vary-header/tablet#\n"),
                ns);
        bail_error(err);


        err = cli_printf_query(
                _("    Phone : #/nkn/nvsd/origin_fetch/config/%s/vary-header/phone#\n\n"),
                ns);
        bail_error(err);



        err = cli_printf_query(
                _("    Add Transparent-cluster Header : #/nkn/nvsd/namespace/%s/origin-request/include-orig-ip-port#\n"),
                ns);
        bail_error(err);

	ns_origin_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", ns);
	bail_null(ns_origin_node);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, 
		    &ns_origin_type, ns_origin_node, NULL);
	bail_error(err);
	
	if((atoi(ts_str(ns_origin_type))) == osvr_http_node_map) 
	{
		err = cli_printf_query(
			_("    Connect Timeout Value : #/nkn/nvsd/namespace/%s/origin-request/connect/timeout#\n"),
 			ns);
 		bail_error(err);

		err = cli_printf_query(
			_("    Connect Retry Delay Value : #/nkn/nvsd/namespace/%s/origin-request/connect/retry-delay#\n"),
			ns);
		bail_error(err);

		err = cli_printf_query(
			_("    Read interval Timeout Value : #/nkn/nvsd/namespace/%s/origin-request/read/interval-timeout#\n"),
			ns);
		bail_error(err);

		err = cli_printf_query(
			_("    Read Retry delay Value : #/nkn/nvsd/namespace/%s/origin-request/read/retry-delay#\n"),
			ns);
		bail_error(err);
	}
	pattern = smprintf("/nkn/nvsd/namespace/%s/origin-request/header/add/*", ns);
	bail_null(pattern);

	err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
			&bindings, true, true, pattern);
	bail_error(err);

	err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
			cli_nvsd_origin_request_header_add_show_one, NULL, &num_matches);
	bail_error(err);
	safe_free(pattern);
	bn_binding_array_free(&bindings);

	if (num_matches > 0) {
		cli_printf(_("        --------------------------------------\n\n"));
	}

bail:
	safe_free(host_inherit_node_name);
	safe_free(cache_revalidate_node_name);
	safe_free(client_headers_action_node_name);
	safe_free(flush_node_name);
	safe_free(headto_get_node_name);
	ts_free(&t_method);
	ts_free(&t_validate);
	ts_free(&t_validate_header);
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_client_request_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;
	tbool t_cache_query = false;
	tbool t_strip_query = false;
	tstring *t_age = NULL;
	tstring *t_action = NULL;
	tbool t_cookie_action = false;
	node_name_t node_name = {0};
	int retval_reval = -1;
	unsigned int num_matches = 0;
	tbool t_tunnel = false;
	tbool t_con_handle = false;
	tbool t_tor_auth_header = false;
	tbool t_tor_cookie_header = false;
	tbool t_tor_query_string = false;
	tbool t_tor_cache_control = false;
	node_name_t node_name_tunnel = {0};
	node_name_t t_service_node= {0};
	tbool t_fail_mode = false;
	tstring *t_service = NULL;
	tstring *t_url = NULL;
	tbool t_seek_uri_enable = false;
	node_name_t node_seek_t = {0};
	tbool t_is_secure = false;
	tbool t_is_plaintext = false;
	tbool tbool_serve_expired = false;
	tbool t_is_head_to_get = false;
	tbool t_dns_mismatch = false;
	bn_binding_array *bindings = NULL;


	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);


	ns = tstr_array_get_str_quick(params, 0);
	bail_error(err);




	err = cli_printf(
		_("\n  Client-Request Configuration:\n"));
	bail_error(err);

	/*Does the namespace support http / https*/
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/client-request/secure", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
			&t_is_secure, node_name, NULL);
	bail_error(err);

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/client-request/plain-text", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
			&t_is_plaintext, node_name, NULL);
	bail_error(err);

	err = cli_printf( _("    Secure Delivery(HTTPS over SSL):%s\n"),
			t_is_secure ? "Enable" : "Disable");
	bail_error(err);

	err = cli_printf( _("    Plain-text Delivery:%s\n"),
			t_is_plaintext ? "Enable" : "Disable");
	bail_error(err);

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/client-request/method/head/cache-miss/action/convert-to/method/get", ns);
        err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &t_is_head_to_get, node_name, NULL);
        bail_error(err);

        err = cli_printf( _("    Action on Cache-miss Head method : %s\n"), t_is_head_to_get ? "Convert to Get method" : "Tunnel");
        bail_error(err);

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/client-request/dns/mismatch/action/tunnel", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &t_dns_mismatch, node_name, NULL);
	bail_error(err);
	if (t_dns_mismatch){
	    err = cli_printf( _("    Action on dns mis-match : Tunnel\n"));
	    bail_error(err);
	}

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/query/cache/enable", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
			&t_cache_query, node_name, NULL);
	bail_error(err);

	err = cli_printf(
		_("    Allow objects with a query-string to be cached: %s\n"), 
		t_cache_query ? "yes" : "no");
	bail_error(err);


	if (t_cache_query) {
		snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/query/strip", ns);

		err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
				&t_strip_query, node_name, NULL);
		bail_error(err);

		err = cli_printf(
			_("      Exclude query string while caching the object: %s\n"), 
			t_strip_query ? "yes" : "no");
		bail_error(err);
	}


	snprintf(node_name, sizeof(node_name),"/nkn/nvsd/origin_fetch/config/%s/client-request/cache-control/max_age", ns);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
			&t_age, node_name, NULL);

	bail_error(err);
	
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/client-request/cache-control/action", ns);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
			&t_action, node_name, NULL);
	bail_error(err);

	if (t_age && t_action) {
		err = cli_printf(
				_("    Cache control max age : %s\n"), ts_str(t_age));
		bail_error(err);

		err = cli_printf(
				("    Cache control max age action : %s\n"), ts_str(t_action));
		bail_error(err);

	}

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/cookie/cache/enable", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
			&t_cookie_action, node_name, NULL);
	bail_error(err);

	err = cli_printf(
		_("    Allow objects with a cookie header to be cached: %s\n"), 
		t_cookie_action ? "yes" : "no");

	bail_error(err);

	cli_nvsd_ns_is_reval_always_enabled(ns, &retval_reval, NULL);

	err = cli_printf(_("    Revalidate object always : %s %s\n"),
			((retval_reval == client_request_reval_always_inline_enabled) ||
			 (retval_reval == client_request_reval_always_offline_enabled)
			 ? "Yes" : "No"),
                         ((retval_reval == client_request_reval_always_inline_enabled) ? " (inline)" :
                         (retval_reval == client_request_reval_always_offline_enabled) ? " (offline)" : ""));

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/serve-expired/enable", ns);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
					&tbool_serve_expired, node_name, NULL);
    	bail_error(err);
        err = cli_printf(
            _("    Revalidate asynchronously on object expiry : %s\n"),
               (tbool_serve_expired == 1) ? "Yes" : "No");
        bail_error(err);



	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/client-request/tunnel-all", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_tunnel, node_name, NULL);
	bail_error(err);

	err = cli_printf(
		_("    Tunnel All Requests : %s\n"),
		(t_tunnel ? "***** ENABLED *****" : "Disabled"));
	bail_error(err);

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/client-request/method/connect/handle", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_con_handle, node_name, NULL);
	bail_error(err);

	err = cli_printf(
		_("    Action on Connect method : %s\n"),
		(t_con_handle ? " HANDLE " : " TUNNEL "));
	bail_error(err);

	/*Geo-service configuration details*/
	snprintf(t_service_node, sizeof(t_service_node), "/nkn/nvsd/namespace/%s/client-request/geo-service/service", ns);
	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
			&t_service, t_service_node, NULL);
	bail_error(err);
	if(t_service && ts_length(t_service) > 0) {
		cli_printf( _("    Geo-service configuration:\n       Service Type:%s\n"), ts_str(t_service));
		snprintf(t_service_node, sizeof(t_service_node), "/nkn/nvsd/namespace/%s/client-request/geo-service/api-url", ns);
		err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
				&t_url, t_service_node, NULL);
		bail_error(err);
		if(t_url && ts_length(t_url) > 0){
			cli_printf( _("       Api-url:%s\n"), ts_str(t_url));
		}

		snprintf(t_service_node, sizeof(t_service_node), "/nkn/nvsd/namespace/%s/client-request/geo-service/failover-mode", ns);
		err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_fail_mode, t_service_node, NULL);
		bail_error(err);
		if(t_fail_mode) {// The default option of REJECT is not to be shown.Still listing it here for user 
			err = cli_printf(_("       Failover mode : %s\n"),	(t_fail_mode ? " BYPASS " : " REJECT "));
			bail_error(err);
		}
		err = cli_printf_query(_("       Lookup timeout: "
					"#/nkn/nvsd/namespace/%s/client-request/geo-service/lookup-timeout# (msec)\n"), ns);
		bail_error(err);
	}

	err = cli_printf(
	    _("    Tunnel-Override Configuration:\n"));
	bail_error(err);

	snprintf(node_name_tunnel, sizeof(node_name_tunnel), "/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/auth-header", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_tor_auth_header, node_name_tunnel, NULL);
	bail_error(err);

	if (t_tor_auth_header){
	err = cli_printf( _("    AUTH Header\n"));
	bail_error(err);
	}
	snprintf(node_name_tunnel, sizeof(node_name_tunnel), "/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/cookie-header", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_tor_cookie_header, node_name_tunnel, NULL);
	bail_error(err);

	if (t_tor_cookie_header){
	err = cli_printf( _("    COOKIE Header\n"));
	bail_error(err);
	}

	snprintf(node_name_tunnel, sizeof(node_name_tunnel), "/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/query-string", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_tor_query_string, node_name_tunnel, NULL);
	bail_error(err);

	if (t_tor_query_string){
	err = cli_printf( _("    Query-string\n"));
	bail_error(err);
	}

	snprintf(node_name_tunnel, sizeof(node_name_tunnel), "/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/cache-control", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_tor_cache_control, node_name_tunnel, NULL);
	bail_error(err);

	if (t_tor_cache_control){
	err = cli_printf( _("    Cache-control\n"));
	bail_error(err);
	}

	snprintf(node_name, sizeof(node_name),"/nkn/nvsd/namespace/%s/client-request/header/accept/*", ns);

	err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, &bindings, true, true, node_name);
	bail_error(err);

	err = mdc_foreach_binding_prequeried(bindings, node_name, NULL,
			cli_nvsd_ns_client_request_header_accept_show_one, NULL, &num_matches);
	bail_error(err);
	bn_binding_array_free(&bindings);

	if (num_matches > 0) {
		cli_printf(_(
			"      ---------------\n"));
	}

	err = cli_printf(
	    _("\n  Dynamic-URI Configuration:\n"));
	bail_error(err);

	err = cli_printf_query(
	    _("   Regex to match on Request URL: "
		"#/nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex#\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("   Mapping string to URL: "
		"#/nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string#\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("   Tokenize string to URL: "
		"#/nkn/nvsd/namespace/%s/client-request/dynamic-uri/tokenize-string#\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("   Tunnel the request when no regex match: "
		"#/nkn/nvsd/namespace/%s/client-request/dynamic-uri/no-match-tunnel#\n"), ns);
	bail_error(err);
	
	err = cli_printf(
			_("\n  Seek-URI Configuration:\n"));
	bail_error(err);

	snprintf(node_seek_t, sizeof(node_seek_t), "/nkn/nvsd/namespace/%s/client-request/seek-uri/enable", ns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t_seek_uri_enable, node_seek_t, NULL);
	bail_error(err);

	if (!t_seek_uri_enable){
	    err = cli_printf(
	       _("				Not Configured\n"));
	    bail_error(err);
	}

	err = cli_printf_query(
	    _("   Regex to match on Request URL: "
		"#/nkn/nvsd/namespace/%s/client-request/seek-uri/regex#\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("   Mapping string to URL: "
		"#/nkn/nvsd/namespace/%s/client-request/seek-uri/map-string#\n"), ns);
	bail_error(err);

bail:
	bn_binding_array_free(&bindings);
	ts_free(&t_age);
	ts_free(&t_action);
	return err;
}

/* Helper function used to find whether revalidate always is enabled or not 
Called from show namepsace and revmap handler
*/
static int 
cli_nvsd_ns_is_reval_always_enabled(const char* ns, 
		int* retval,
		const bn_binding_array *bindings)
{
	int err = 0;
	char *node_name = NULL;
	tstring *t_proxy_mode = NULL;
	tstring *t_reval_type = NULL;
	tstring *t_reval_time = NULL;
	*retval = -1;

	node_name = smprintf("/nkn/nvsd/namespace/%s/proxy-mode", ns);
	bail_null(node_name);

	if (bindings) {
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_proxy_mode,
				"/nkn/nvsd/namespace/%s/proxy-mode", ns);
		bail_error_null(err, t_proxy_mode);
	} else {

		err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
				&t_proxy_mode, node_name, NULL);
		bail_error(err);
		safe_free(node_name);

		bail_null(t_proxy_mode);
	}

	if((strncmp("transparent", ts_str(t_proxy_mode), ts_num_chars(t_proxy_mode))== 0) ||
		(strncmp("virtual", ts_str(t_proxy_mode), ts_num_chars(t_proxy_mode))== 0)) {

		if(bindings) {
			err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_reval_type,
					"/nkn/nvsd/origin_fetch/config/%s/object/revalidate/reval_type", ns);
			bail_error_null(err, t_reval_type);

			err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_reval_time,
					"/nkn/nvsd/origin_fetch/config/%s/object/revalidate/time_barrier", ns);
			bail_error_null(err, t_reval_time);


		} else {

			node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/reval_type", ns);
			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
					&t_reval_type, node_name, NULL);
			bail_error(err);
			safe_free(node_name);
			bail_null(t_reval_type);

			node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/time_barrier", ns);

			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
					&t_reval_time, node_name, NULL);
			bail_error(err);
			safe_free(node_name);
			bail_null(t_reval_time);

		}


	
		if ((strncmp("2541441258", ts_str(t_reval_time), ts_num_chars(t_reval_time)) == 0)) {
			if(strncmp("all_offline", ts_str(t_reval_type), ts_num_chars(t_reval_type)) == 0)
				*retval = client_request_reval_always_offline_enabled;
			else
				*retval = client_request_reval_always_inline_enabled;
		} else if (strncmp("0", ts_str(t_reval_time), ts_num_chars(t_reval_time)) == 0){ 
			*retval = client_request_reval_always_disabled;
		}

	}

bail:
	safe_free(node_name);
	ts_free(&t_proxy_mode);
	ts_free(&t_reval_type);
	ts_free(&t_reval_time);
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_client_response_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;
	node_name_t pattern = {0};
	unsigned int num_matches = 0;
	bn_binding_array *bindings = NULL;
        tstring *t_dscp = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	err = cli_printf(
		_("\n  Client-Response Configuration :\n"));
	bail_error(err);

        snprintf(pattern,sizeof(pattern),"/nkn/nvsd/namespace/%s/client-response/dscp",ns);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                                &t_dscp, pattern, NULL);
        bail_null(t_dscp);
       
        if(atoi(ts_str(t_dscp)) >= 0) {
            cli_printf(_("    DSCP setting: %s\n"),ts_str(t_dscp)); 
        }

	snprintf(pattern, sizeof(pattern),"/nkn/nvsd/namespace/%s/client-response/header/add/*", ns);

	err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, 
			&bindings, true, true, pattern);
	bail_error(err);

	err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
			cli_nvsd_ns_client_response_header_add_show_one, NULL, &num_matches);
	bail_error(err);
	bn_binding_array_free(&bindings);
	
	if (num_matches > 0) {
		cli_printf(_(
			"        -----------------------------------\n"));
	}

	snprintf(pattern, sizeof(pattern),"/nkn/nvsd/namespace/%s/client-response/header/delete/*", ns);

	err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, 
			&bindings, true, true, pattern);
	bail_error(err);

	err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
			cli_nvsd_ns_client_response_header_delete_show_one, NULL, &num_matches);
	bail_error(err);
	bn_binding_array_free(&bindings);
	
	if (num_matches > 0) {
		cli_printf(_(
			"        ---------------\n"));
	}
bail:
	bn_binding_array_free(&bindings);
        ts_free(&t_dscp);
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_client_response_header_add_show_one(
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

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name_components);
	UNUSED_ARGUMENT(name_last_part);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(cb_data);

	if (idx == 0) {
		cli_printf(_(
			"    Add Headers in Client reponse:\n"));
		cli_printf(_(
			"        Header Name                         Value\n"));
		cli_printf(_(
			"        ---------------------------------------------------\n"));
	}

	cli_printf_query(_(
			"        #w:32~j:left~%s/name# #w:32~j:left~%s/value#\n"),
			ts_str(name), ts_str(name));

	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_client_request_header_accept_show_one(
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

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name_components);
	UNUSED_ARGUMENT(name_last_part);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(cb_data);

	if (idx == 0) {
		cli_printf(_(
			"    Accept Headers from Client request:\n"));
		cli_printf(_(
			"      Header Name	\n"));
		cli_printf(_(
			"      ---------------\n"));
	}

	cli_printf_query(_(
			"         #%s/name#\n"),
			ts_str(name));

	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_client_response_header_delete_show_one(
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

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name_components);
	UNUSED_ARGUMENT(name_last_part);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(cb_data);

	if (idx == 0) {
		cli_printf(_(
			"      Delete Headers from Client reponse:\n"));
		cli_printf(_(
			"        Header Name	\n"));
		cli_printf(_(
			"        ---------------\n"));
	}

	cli_printf_query(_(
			"           #%s/name#\n"),
			ts_str(name));

	return err;
}


/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_client_response_header_add_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	const char *n = NULL;
	tstring *t_name = NULL;
	tstring *t_value = NULL;
	tstring *rev_cmd = NULL;
	
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	n = tstr_array_get_str_quick(name_parts, 3);
	bail_null(n);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_name,
			"%s/name", name);
	bail_error_null(err, t_name);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_value,
			"%s/value", name);
	bail_error_null(err, t_value);

	err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-response header \"%s\" \"%s\" action add",
			n, ts_str(t_name), ts_str(t_value));
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/name", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/value", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_cmds, "%s", ts_str(rev_cmd));
	bail_error(err);

bail:
	ts_free(&t_name);
	ts_free(&t_value);
	ts_free(&rev_cmd);
	return err;
}


/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_client_response_header_delete_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	const char *n = NULL;
	tstring *t_name = NULL;
	tstring *rev_cmd = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	n = tstr_array_get_str_quick(name_parts, 3);
	bail_null(n);
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_name, 
			"%s/name", name);
	bail_error_null(err, t_name);

	err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-response header \"%s\" action delete",
			n, ts_str(t_name));
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/name", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_cmds, "%s", ts_str(rev_cmd));
	bail_error(err);

bail:
	ts_free(&t_name);
	ts_free(&rev_cmd);
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_origin_request_header_add_show_one(
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

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name_components);
	UNUSED_ARGUMENT(name_last_part);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(cb_data);

	if (idx == 0) {
		cli_printf(_(
		"    Headers Added to request:\n"));
		cli_printf(_(
		"        Header Name          Value\n"));
		cli_printf(_(
		"        --------------------------------------\n"));
	}

	cli_printf_query(_(
		"        #w:20~j:left~%s/name# #w:16~j:left~%s/value#\n"),
		ts_str(name), ts_str(name));

	return err;
}

/*---------------------------------------------------------------------------*/
tbool
cli_nvsd_ns_is_active(const char *name)
{
	int err = 0;
	tbool t = false;
	char *node_name = NULL;

	node_name = smprintf("/nkn/nvsd/namespace/%s/status/active", name);

	if (node_name == NULL)
		goto bail;

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&t, node_name, NULL);
	if (err) goto bail;

bail:
	safe_free(node_name);
	return t;
}


/*---------------------------------------------------------------------------*/
/* This is similar to mdc_array_get_matching_indices(), except
 * that this function does a case sensitive match for the
 * name that we want.
 */
int
cli_nvsd_ns_match_binding(
		const bn_binding_array *bindings,
		uint32 idx,
		const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value,
		void *cb_data);

typedef struct {

	const char	*child_name;
	uint32_array	*indices;

} ns_header_match_context;
static int
cli_nvsd_ns_mdc_array_get_matching_indices(
		md_client_context *mcc, 
		const char *root_name, 
		const char *db_name, 
		const char *value,
		//const bn_binding_array *children, 
		int32 rev_before, 
		int32 *ret_rev_after, 
		uint32_array **ret_indices, 
		uint32 *ret_code, 
		tstring **ret_msg)
{
	int err = 0;
	char *pattern = NULL;
	uint32 code = 0;
	bn_binding_array *bindings = NULL;
	ns_header_match_context cb_data;
	unsigned int num_matches = 0;

	UNUSED_ARGUMENT(ret_msg);
	UNUSED_ARGUMENT(ret_code);
	UNUSED_ARGUMENT(ret_rev_after);
	UNUSED_ARGUMENT(rev_before);
	UNUSED_ARGUMENT(db_name);
	UNUSED_ARGUMENT(mcc);

	bail_null(root_name);
	bail_null(value);
	bail_null(ret_indices);


	err = uint32_array_new(ret_indices);
	bail_error(err);

	cb_data.child_name = value;
	cb_data.indices  = *ret_indices;

	pattern = smprintf("%s/*", root_name);
	bail_null(pattern);

	err = mdc_get_binding_children(
			cli_mcc, 
			&code, 
			NULL, 
			true, 
			&bindings, 
			true,
			true, 
			root_name);
	if (code)
		goto bail;

	safe_free(pattern);

	pattern = smprintf("%s/*/name", root_name);
	err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, 
			cli_nvsd_ns_match_binding, 
			(void *) &cb_data,
			&num_matches);
	bail_error(err);

bail:
	bn_binding_array_free(&bindings);
	safe_free(pattern);
	return err;
}

/*---------------------------------------------------------------------------*/
int 
cli_nvsd_ns_match_binding(
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
	uint32 int_index = 0;
	const char *node_idx = NULL;
	ns_header_match_context *ctxt = (ns_header_match_context *) cb_data;

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(idx);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);


	if (ts_equal_str(name_last_part, "name", false)) {
		if (ts_equal_str(value, ctxt->child_name, true)) {
			node_idx = tstr_array_get_str_quick(name_components, 7);

			int_index = strtoul(node_idx, NULL, 10);
			err = uint32_array_append(ctxt->indices, int_index);
			bail_error(err);
		}
	}
	
bail:
	return err;
}

/*---------------------------------------------------------------------------*/
static int 
cli_nvsd_ns_warn214_header_mods(const char *header)
{
	int err = 0;

	if ( (0 == strcmpi(header, "Content-Encoding")) ||
		(0 == strcmpi(header, "Content-Range")) ||
		(0 == strcmpi(header, "Content-Type"))) {

		err = cli_printf(
				_("Warning: Modification to end-to-end "
				"headers; no Warning Headers will be added.\n"));
	}

	return err;
}


/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_rtsp_match_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	/* Removed "delivery protocol rtsp match <> CLIs */
	goto bail;
#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match";
	cmd->cc_help_descr = 		N_(
			"Configure a match criteria for this namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri";
	cmd->cc_help_descr = 		N_(
			"Configure a URI prefix match for this namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host";
	cmd->cc_help_descr = 		N_(
			"Configure this namespace to use a virtual-host match criteria");
	CLI_CMD_REGISTER;


	err = cli_nvsd_namespace_rtsp_match_uri_cmds_init(info, context);
	bail_error(err);

	err = cli_nvsd_namespace_rtsp_match_vhost_cmds_init(info, context);
	bail_error(err);
#endif
bail:
	return err;
}


/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_rtsp_match_uri_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	goto bail;
#if 0
	/*------------------------------------------------------------------*/
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri *"; 
	cmd->cc_help_exp = 		N_("<uri-prefix>");
	cmd->cc_help_exp_hint = 	N_("A URI prefix to match");
	cmd->cc_help_term = 		N_("Configure namespace to match this URI prefix");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/uri/uri_name";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_uri;
    	cmd->cc_revmap_type =       	crt_none;
    	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_suborder = 	crso_match;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri * precedence";
	cmd->cc_help_descr = 		N_(
					"Configure a precedence value for this "
					"match criteria");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri * precedence *";
	cmd->cc_help_exp = 		N_("<number>");
	cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
	cmd->cc_help_term = 		N_(
					"Configure namespace to match this "
					"URI prefix with the given precedence number");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/uri/uri_name";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_uri;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/rtsp/match/precedence";
	cmd->cc_exec_value2 = 		"$3$";
	cmd->cc_exec_type2 = 		bt_uint32;
    	cmd->cc_revmap_order =      	cro_namespace;
    	cmd->cc_revmap_type =       	crt_manual;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/rtsp/match/uri/uri_name";
	cmd->cc_revmap_callback = 	cli_nvsd_rtsp_uri_name_revmap;
	cmd->cc_revmap_suborder = 	crso_match;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri regex";
	cmd->cc_help_descr = 		N_(
					"Configure a regular expression to "
					"match for the uri prefix");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri regex *";
	cmd->cc_help_exp = 		N_("<regex>");
	cmd->cc_help_exp_hint = 	N_("");
	cmd->cc_help_term = 		N_(
					"Configure this namespace to use "
					"a regular expression while matching "
					"the URI prefix");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/uri/regex";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_regex;
    	cmd->cc_revmap_type =       	crt_none;
    	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_suborder = 	crso_match;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri regex * precedence";
	cmd->cc_help_descr = 		N_(
					"Configure a precedence value for "
					"this match criteria");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match uri regex * precedence *";
	cmd->cc_help_exp = 		N_("<number>");
	cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
	cmd->cc_help_term = 		N_(
					"Configure namespace to use a "
					"regular expression for URI prefix "
					"matching with an additional precedence "
					"number for matching");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/uri/regex";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_regex;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/rtsp/match/precedence";
	cmd->cc_exec_value2 = 		"$3$";
	cmd->cc_exec_type2 = 		bt_uint32;
    	cmd->cc_revmap_order =      	cro_namespace;
    	cmd->cc_revmap_type =       	crt_manual;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/rtsp/match/uri/regex";
	cmd->cc_revmap_callback = 	cli_nvsd_rtsp_uri_regex_revmap;
	cmd->cc_revmap_suborder = 	crso_match;
	CLI_CMD_REGISTER;
#endif

bail:
	return err;
}



/*---------------------------------------------------------------------------*/
int 
cli_nvsd_namespace_rtsp_match_vhost_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	goto bail;
#if 0
	/*------------------------------------------------------------------*/
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host *"; 
	cmd->cc_help_exp = 		N_("<IPv4 address>");
	cmd->cc_help_exp_hint = 	N_("An IP address");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/virtual-host/ip";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_ipv4addr;
    	cmd->cc_revmap_type =       	crt_none;
    	cmd->cc_revmap_order =      	cro_namespace;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host * precedence";
	cmd->cc_help_descr = 		N_(
			"Configure a precedence value for this match criteria");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host * precedence *";
	cmd->cc_help_exp = 		N_("<number>");
	cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/virtual-host/ip";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_ipv4addr;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/rtsp/match/precedence";
	cmd->cc_exec_value2 = 		"$3$";
	cmd->cc_exec_type2 = 		bt_uint32;
    	cmd->cc_revmap_type =       	crt_none;
    	cmd->cc_revmap_order =      	cro_namespace;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host * *";
	cmd->cc_help_exp = 		N_("<port number>");
	cmd->cc_help_exp_hint = 	N_("A Port number");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/namespace/$1$/rtsp/match/virtual-host/ip";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_ipv4addr;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/namespace/$1$/rtsp/match/virtual-host/port";
	cmd->cc_exec_value2 = 		"$3$";
	cmd->cc_exec_type2 = 		bt_uint16;
    	cmd->cc_revmap_type =       	crt_none;
    	cmd->cc_revmap_order =      	cro_namespace;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host * * precedence";
	cmd->cc_help_descr = 		N_(
			"Configure a precedence value for this match criteria");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * delivery protocol rtsp match virtual-host * * precedence *";
	cmd->cc_help_exp = 		N_("<number>");
	cmd->cc_help_exp_hint = 	cli_nvsd_ns_precedence_help_msg;
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_callback = 	cli_nvsd_namespace_rtsp_match_vhost_config;
    	cmd->cc_revmap_type =       	crt_manual;
    	cmd->cc_revmap_order =      	cro_namespace;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/rtsp/match/virtual-host/ip";
	cmd->cc_revmap_callback = 	cli_nvsd_rtsp_vhost_name_port_revmap;
	cmd->cc_revmap_suborder = 	crso_match;
	CLI_CMD_REGISTER;
	/*------------------------------------------------------------------*/

#endif
bail:
	return err;
}


int
cli_nvsd_namespace_rtsp_origin_fetch_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch";
    cmd->cc_help_descr =        N_("Configure Origin fetch parameters");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root | ccf_prefix_mode_no_revmap;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_namespace_generic_enter_mode;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch";
    cmd->cc_help_descr =        N_("Configure Origin fetch parameters");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_no_origin_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch no";
    cmd->cc_help_descr =        N_("Clear/negate origin fetch settings");
    cmd->cc_req_prefix_count =  3;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-directive";
    cmd->cc_flags =             ccf_unavailable;
    cmd->cc_help_descr =        N_("Configure whether MFD should follow "
                                   "cache directive (Cache-Control) received "
                                   "from the origin in RTSP header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-directive no-cache";
    cmd->cc_help_descr =        N_("Configure MFD behavior for \"no-cache\" directive");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-directive no-cache follow";
    cmd->cc_help_descr =        N_("Follow the no-cache directive specified in the RTSP header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-directive/no-cache";
    cmd->cc_exec_value =        "follow";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-directive/no-cache";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_origin_fetch_rtsp_revmap;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;
    

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-directive no-cache override";
    cmd->cc_help_descr =        N_("Override the \"no-cache\" directive in the RTSP header");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-directive/no-cache";
    cmd->cc_exec_value =        "override";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-age-default";
    cmd->cc_help_descr =        N_("Configure default Cache Age value "
                                   "when the retrieved content has no cache-age");
    CLI_CMD_REGISTER;


    // 
    // BZ 2375 : Node .../cache-age changed to .../cache-age/default
    //
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-age-default 28800";
    cmd->cc_help_descr =        N_("Default cache age - 28800 seconds (8 hours)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-age/default";
    cmd->cc_exec_value =        "28800";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
   
    // 
    // BZ 2375 : Node .../cache-age changed to .../cache-age/default
    //
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "cache-age-default *";
    cmd->cc_help_exp_hint =     N_("number of seconds");
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-age/default";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-age/default";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_origin_fetch_rtsp_revmap;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_delivery_proto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store";
    cmd->cc_flags =             ccf_unavailable;
    cmd->cc_help_descr =        N_("Configure content store options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "content-store";
    cmd->cc_help_descr =        N_("Clear content store options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store media";
    cmd->cc_help_descr =        N_("Configure content store options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "content-store media";
    cmd->cc_help_descr =        N_("Clear content store options");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store media cache-age-threshold";
    cmd->cc_help_descr =        N_("Configure Cache age threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "content-store media cache-age-threshold";
    cmd->cc_help_descr =        N_("Clear Cache age threshold");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/content-store/media/cache-age-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store media cache-age-threshold 60";
    cmd->cc_help_descr =        N_("Default cache age threshold - 60 seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/content-store/media/cache-age-threshold";
    cmd->cc_exec_value =        "60";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;
    

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store media cache-age-threshold *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Number of seconds to store content");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/content-store/media/cache-age-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/cache-age-threshold";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_origin_fetch_rtsp_revmap;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store media object-size";
    cmd->cc_help_descr =        
        N_("Configure object size threshold at which origin "
                "manager caches the object");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "content-store media object-size";
    cmd->cc_help_descr =        
        N_("Set default object size, All objects "
            "are cached to disks");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         
        "/nkn/nvsd/origin_fetch/config/$1$/rtsp/content-store/media/object-size-threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * delivery protocol rtsp origin-fetch "
                                "content-store media object-size *";
    cmd->cc_help_exp =          N_("<size, bytes>");
    cmd->cc_help_exp_hint =     
        N_("Objects greater than this size are cached into disks, "
                "otherwise they are not cached");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         
        "/nkn/nvsd/origin_fetch/config/$1$/rtsp/content-store/media/object-size-threshold";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/object-size-threshold";
    cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =     	(void*)cli_nvsd_origin_fetch_rtsp_revmap;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder = 	crso_origin_fetch;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "cache-directive";
    cmd->cc_help_descr =        N_("Reset cache directive options to MFD default");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "cache-directive no-cache";
    cmd->cc_help_descr =        N_("Follow cache directive received from the origin");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-directive/no-cache";
    cmd->cc_exec_value =        "follow";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * delivery protocol rtsp origin-fetch "
                                "cache-age-default";
    cmd->cc_help_descr =        N_("Set Cache age to MFD default (28800 seconds)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-age/default";
    cmd->cc_exec_value =        "28800";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;
    
bail:
	return err;
}




int 
cli_nvsd_ns_client_request_cookie_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tbool is_cacheable = false;
	const char *namespace = NULL;
	char *node_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);


	node_name = smprintf("%s/cookie/cache/enable", name);
	bail_null(node_name);

	err = bn_binding_array_get_value_tbool_by_name(bindings, node_name, NULL, &is_cacheable);
	bail_error(err);
	safe_free(node_name);

	if ( !is_cacheable ) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request cookie action no-cache", 
				namespace);
		bail_error(err);
	} else {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request cookie action cache",
				namespace);
		bail_error(err);

	}

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/cookie/cache/enable", name);
	bail_error(err);



bail:
	safe_free(node_name);
	ts_free(&rev_cmd);
	return err;
}
#if 0
static int
cli_nvsd_rtsp_uri_name_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *rev_cmd = NULL;
    tstring *t_uri_name= NULL;
    match_type_t mt = mt_none;
    uint32 precedence = 0;
    const char *namespace = NULL;

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_rtsp_match_type(namespace, &mt);
    bail_error(err);

    err = cli_nvsd_ns_get_rtsp_precedence(namespace, &precedence);
    bail_error(err);

    /* If the match type is for this node, 
     * only then create the command 
     */
    if (mt_rtsp_uri == mt) {

		err = mdc_get_binding_tstr(cli_mcc, 
			    NULL, NULL, NULL, &t_uri_name , name, NULL);
		bail_error(err);

		if (!t_uri_name)
			goto bail;
		if (ts_length(t_uri_name) > 0) {

			if (precedence != 0) {

				err = ts_new_sprintf(&rev_cmd, 
						"namespace %s delivery protocol rtsp match uri %s precedence %d",
						namespace, ts_str(t_uri_name), precedence);
				bail_error(err);
			}
			else {
				err = ts_new_sprintf(&rev_cmd, 
						"namespace %s delivery protocol rtsp match uri %s",
						namespace, ts_str(t_uri_name));
				bail_error(err);
			}

			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
		}    

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/namespace/%s/rtsp/match/type",
				namespace); 
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/namespace/%s/rtsp/match/precedence",
				namespace); 
		bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

bail:
	ts_free(&rev_cmd);
	return(err);
}

static int
cli_get_rtsp_match_type (const char *namespace, match_type_t *mt)
{
	int err =0;
	tstring *t_type = NULL;

	bail_null(namespace);
	bail_null(mt);

	*mt = mt_none;

    	err = mdc_get_binding_tstr_fmt(cli_mcc, 
			NULL, NULL, NULL, 
			&t_type, 
			NULL, 
			"/nkn/nvsd/namespace/%s/rtsp/match/type", namespace);
    	bail_error(err);

	if (t_type)
	  *mt = atoi(ts_str(t_type));

bail:
	ts_free(&t_type);
	return err;

}

static int
cli_nvsd_ns_get_rtsp_precedence(const char *namespace, uint32 *precedence)
{
	int err = 0;
	tstring *t_prec = NULL;

	bail_null(namespace);
	bail_null(precedence);

	*precedence = 0;

	err = mdc_get_binding_tstr_fmt(cli_mcc, 
			NULL, NULL, NULL, 
			&t_prec,
			NULL,
			"/nkn/nvsd/namespace/%s/rtsp/match/precedence", 
			namespace);
	bail_error(err);

	if (t_prec)
		*precedence = atoi(ts_str(t_prec));

bail:
	ts_free(&t_prec);
	return err;
}
#endif


/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_rtsp_follow_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tstring *t_header = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;
	char *node_name = NULL;
	tbool t_use_client = false;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	if ( osvr_rtsp_dest_ip == ot ) {

		node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/rtsp/follow/use-client-ip", namespace);
		bail_null(node_name);
		err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_use_client, node_name, NULL);
		bail_error(err);

		if (!t_use_client) {

			err = ts_new_sprintf(&rev_cmd, 
					"namespace %s origin-server rtsp follow dest-ip ",
					namespace);
			bail_error(err);
		} else {
			err = ts_new_sprintf(&rev_cmd, 
					"namespace %s origin-server rtsp follow dest-ip use-client-ip",
					namespace);
			bail_error(err);

		}

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}

	/* Consume nodes */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
			"/nkn/nvsd/namespace/%s/origin-server/rtsp/follow/use-client-ip", namespace);
	bail_error(err);

bail:
	safe_free(node_name);
	ts_free(&rev_cmd);
	ts_free(&t_header);
	return err;
}


int cli_nvsd_ns_check_if_consumed(const tstr_array *crr_nodes, 
		const char *fmt, ...)
{

	uint32 ret_index = 0;
	va_list ap;
	char *str = NULL;
	int err = 0;
	
	
	va_start(ap, fmt);
	str = vsmprintf(fmt, ap);
	va_end(ap);

	err = tstr_array_linear_search_str(crr_nodes, str, 0, &ret_index);

	if (err != 0) {
		goto bail;
	}
	else if (ret_index == (uint32)(-1)) {
		goto bail;
	} else if (ret_index > 0) {
		// Consumed 
		err = 1;
	}

bail:
	safe_free(str);
	return err;
}

/*---------------------------------------------------------------------------*/
#if 0
static int
cli_nvsd_rtsp_vhost_name_port_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_vhost_name= NULL;
    tstring *t_vhost_value = NULL;
    match_type_t mt = mt_none;
    uint32 precedence = 0;
    const char *namespace = NULL;
    tstring *rev_cmd = NULL;

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_rtsp_match_type(namespace, &mt);
    bail_error(err);

    err = cli_nvsd_ns_get_precedence(namespace, "rtsp", &precedence);
    bail_error(err);


    /* If the match type is for this node, 
     * only then create the command 
     */
    if (mt_rtsp_vhost == mt) {

	    err = mdc_get_binding_tstr_fmt(cli_mcc, 
			    NULL, NULL, NULL, 
			    &t_vhost_name, NULL, 
			    "%s", name);
	    bail_error(err);

	    if (t_vhost_name 
			    && (ts_length(t_vhost_name) > 0) 
			    && !(ts_equal_str(t_vhost_name, "255.255.255.255", false))) {

		    /* Get the header value too */
		    err = mdc_get_binding_tstr_fmt(cli_mcc, 
				    NULL, NULL, NULL,
				    &t_vhost_value,
				    NULL,
				    "/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/port",
				    namespace);
		    bail_error(err);

		    if (precedence != 0) {
			    err = ts_new_sprintf(&rev_cmd, 
					    "namespace %s delivery protocol rtsp match virtual-host \"%s\" \"%s\" precedence %d",
					    namespace,
					    ts_str(t_vhost_name), 
					    t_vhost_value ? ts_str(t_vhost_value) : "", 
					    precedence);
			    bail_error(err);
		    } else {
			    err = ts_new_sprintf(&rev_cmd, 
					    "namespace %s delivery protocol rtsp match virtual-host \"%s\" \"%s\"",
					    namespace,
					    ts_str(t_vhost_name), 
					    t_vhost_value ? ts_str(t_vhost_value) : "");
			    bail_error(err);

		    }

		    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		    bail_error(err);

			err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
					"/nkn/nvsd/namespace/%s/rtsp/match/type",
					namespace); 
			bail_error(err);
			err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
					"/nkn/nvsd/namespace/%s/rtsp/match/precedence",
					namespace); 
			bail_error(err);
	    }

    }

    /* Consume bindings */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
		    "/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/port", 
		    namespace);
    bail_error(err);
     
bail:
	ts_free(&rev_cmd);
	return(err);
}

/*---------------------------------------------------------------------------*/
/* This is called when the user give 3 parameters in the CLI -
 *  - vhost ip
 *  - vhost port
 *  - precedence
 */
int cli_nvsd_namespace_rtsp_match_vhost_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_namespace = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_vhost_ip = NULL;
	char *bn_vhost_port = NULL;
	char *bn_precedence = NULL;

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	bn_vhost_ip = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/ip", 
			c_namespace);
	bail_null(bn_vhost_ip);

	bn_vhost_port = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/port", 
			c_namespace);
	bail_null(bn_vhost_port);

	bn_precedence = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/precedence", 
			c_namespace);
	bail_null(bn_precedence);

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_ipv4addr, 
			tstr_array_get_str_quick(params, 1),
			bn_vhost_ip);
	bail_error(err);
	if (ret_err)
		goto bail;

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint16,
			tstr_array_get_str_quick(params, 2),
			bn_vhost_port);
	bail_error(err);
	if (ret_err)
		goto bail;

	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32,
			tstr_array_get_str_quick(params, 3),
			bn_precedence);
	bail_error(err);
	if (ret_err)
		goto bail;


bail:
	return err;
}
#endif

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_rtsp_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tstring *t_port = NULL;
	tstring *t_transport = NULL;
	const char *namespace = NULL;
	origin_server_type_t ot = osvr_none;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = cli_get_origin_type(namespace, &ot, bindings);
	bail_error(err);

	if (osvr_rtsp_host == ot) {

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
				NULL, &t_port, "/nkn/nvsd/namespace/%s/origin-server/rtsp/host/port", namespace);
		bail_error(err);

		err = ts_new_sprintf(&rev_cmd, "namespace %s origin-server rtsp \"%s\"",
				namespace, value);
		bail_error(err);

		if (!ts_equal_str(t_port, "554", false)) {
			err = ts_append_sprintf(rev_cmd, " \"%s\"",
					ts_str(t_port));
			bail_error(err);
		}

		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
				NULL, &t_transport, "/nkn/nvsd/namespace/%s/origin-server/rtsp/host/transport", namespace);
		bail_error(err);    

		if (!ts_equal_str(t_transport, "rtp-rtsp", false)) {
			err = ts_append_sprintf(rev_cmd, " %s ",
					ts_str(t_transport));
			bail_error(err);
		} else if(!ts_equal_str(t_transport, "rtp-udp", false)) {
			err = ts_append_sprintf(rev_cmd, " %s ",
					ts_str(t_transport));
			bail_error(err);
		}

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/namespace/%s/origin-server/type",
				namespace);
		bail_error(err);
	}

	/* Consume nodes - even though this is wildcarded,
	 * it is possible some stale config exists.
	 */

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);
	
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/rtsp/host/port", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/origin-server/rtsp/host/transport", namespace);
	bail_error(err);

#if 0	
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/weight", name);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/keep-alive", name);
	bail_error(err);
#endif
bail:
	ts_free(&rev_cmd);
	ts_free(&t_port);
	return err;
}

int cli_nvsd_namespace_rtsp_pub_point_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    char *node_name = NULL;
    uint32 code = 0;
    const char *namespace_value = NULL;
    const char *pub_point_value = NULL;
    const char *sdp_value = NULL;
    const char *start_time = NULL;
    const char *end_time = NULL;
    const char *mode_value = NULL;
    tstring *msg = NULL;
    
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    namespace_value = tstr_array_get_str_quick(params, 0);
    bail_null(namespace_value);
	
    pub_point_value = tstr_array_get_str_quick(params, 1);
    bail_null(pub_point_value);
	
    sdp_value = tstr_array_get_str_quick(params, 2);
    bail_null(sdp_value);
	
    start_time = tstr_array_get_str_quick(params, 3);
    bail_null(start_time);

    mode_value = smprintf("start-time");
    bail_null(mode_value);

    node_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/sdp", 
           	namespace_value, pub_point_value );
    bail_null(node_name);

    err = mdc_create_binding(cli_mcc,
		    &code, &msg,
		    bt_string,
		    sdp_value,
		    node_name);
    bail_error(err);
    if ( code != 0) {
	    cli_printf( _("Error:%s\n"), ts_str( msg));
	    goto bail;
    }

    node_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/start-time", 
           	namespace_value, pub_point_value );
    bail_null(node_name);

    err = mdc_create_binding(cli_mcc,
		    &code, &msg,
		    bt_string,
		    start_time,
		    node_name);
    bail_error(err);
    if ( code != 0) {
	    cli_printf( _("Error:%s\n"), ts_str( msg));
	    goto bail;
    }

    node_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/mode", 
           	namespace_value, pub_point_value );
    bail_null(node_name);

    err = mdc_create_binding(cli_mcc,
		    &code, &msg,
		    bt_string,
		    mode_value,
		    node_name);
    bail_error(err);
    if ( code != 0) {
	    cli_printf( _("Error:%s\n"), ts_str( msg));
	    goto bail;
    }

    end_time = tstr_array_get_str_quick(params, 4);
    //bail_null(end_time);
  
    if ( end_time != NULL ) {
	    node_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/end-time", 
       			    	namespace_value, pub_point_value );
	    bail_null(node_name);

	    err = mdc_create_binding(cli_mcc,
		    &code, &msg,
		    bt_string,
		    end_time,
		    node_name);
	    bail_error(err);
	    if ( code != 0) {
		   cli_printf( _("Error:%s\n"), ts_str( msg));
		   goto bail;
	    }	
    }

bail :
    ts_free(&msg);
    safe_free (node_name);
    return err;
}

int cli_nvsd_namespace_policy_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	uint32 ret_err = 0;
	const char *ns = NULL;
	const char *policy = NULL;
	tbool is_commited = false;
	char *node_name = NULL;
	tstring *t_ns = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	policy = tstr_array_get_str_quick(params, 1);

	bail_null(ns);
	bail_null(policy);

	node_name = smprintf("/nkn/nvsd/policy_engine/config/policy/script/%s/commited", policy);
	bail_null(node_name);

	err = mdc_get_binding_tbool(cli_mcc, &ret_err, NULL, NULL, &is_commited,
			node_name, NULL);
	bail_error(err);
	bail_error(ret_err);
	safe_free(node_name);

	if(!is_commited) {
		cli_printf_error("Only a commited policy can be bound");
		goto bail;
	}

	/* trigger an action to see if the policy is already bound to someother namespace */
	nkn_cli_is_policy_bound(policy, &t_ns, "false", "true");
	if(t_ns && (strcmp(ts_str(t_ns), ns) != 0)) {
		cli_printf_error("Policy is bound to namespace %s."
			"A policy can be associated only with a single namespace",ts_str(t_ns));
		goto bail;
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/policy/file", ns);
	bail_null(node_name);

	err = mdc_set_binding(cli_mcc, &ret_err, NULL ,
			bsso_modify, bt_string,
			policy,
			node_name);

	bail_error(err);
	bail_error(ret_err);
	safe_free(node_name);

	/* Trigger an action to see if policy is valid */

bail:
	safe_free(node_name);
	ts_free(&t_ns);
	return err;
}

int cli_nvsd_namespace_origin_server_rtsp(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    char *node_name = NULL;
    uint32 num_params = 0;
    const char *namespace_value = NULL;
    const char *fqdn_value = NULL;
    const char *port_value = NULL;
    const char *transport_value = NULL;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    num_params = tstr_array_length_quick(params);

    namespace_value = tstr_array_get_str_quick(params, 0);
    bail_null(namespace_value);
	
    fqdn_value = tstr_array_get_str_quick(params, 1);
    bail_null(fqdn_value);
    
    if(num_params > 2) {
	    port_value = tstr_array_get_str_quick(params, 2);
	    bail_null(port_value);
    } else {
	    port_value = "554";
    }

    switch(cmd->cc_magic) {
	    case cc_oreq_rtsp_rtp_udp:
		    transport_value = "rtp-udp";
		    break;
	    case cc_oreq_rtsp_rtp_rtsp:
		    transport_value = "rtp-rtsp";
		    break;
	    case cc_oreq_rtsp_rtp_client:
		    transport_value = "";
		    break;
    }

    node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/rtsp/host/name", 
           	namespace_value );
    bail_null(node_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string,
		    fqdn_value,
		    node_name);

    bail_error(err);
    if ( ret_err != 0) {
	    cli_printf( _("Error:%s\n"), ts_str( ret_msg ));
	    goto bail;
    }

    node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/rtsp/host/port", 
           	namespace_value );
    bail_null(node_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_uint16,
		    port_value,
		    node_name);

    bail_error(err);
    if ( ret_err != 0) {
	    cli_printf( _("Error:%s\n"), ts_str( ret_msg ));
	    goto bail;
    }

    node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/rtsp/host/transport", 
           	namespace_value );
    bail_null(node_name);

    err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string,
		    transport_value,
		    node_name);

    bail_error(err);
    if ( ret_err != 0) {
	    cli_printf( _("Error:%s\n"), ts_str( ret_msg ));
	    goto bail;
    }

bail :
    ts_free(&ret_msg);
    safe_free (node_name);
    return err;

}

static int
cli_nvsd_namespace_pe_revmap (void *data, const cli_command *cmd,
		const bn_binding_array *bindings,
		const char *name,
		const tstr_array *name_parts,
		const char *value, const bn_binding *binding,
		cli_revmap_reply *ret_reply)
{
	int err = 0;
	const char *namespace = NULL;
	tstring *rev_cmd = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	bail_null(value);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	if(strlen(value) >1) {

		err = ts_new_sprintf(&rev_cmd,
				"# namespace %s bind policy %s",
				namespace, value);
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);
	}



bail:
	ts_free(&rev_cmd);
	return err;
}

static int
cli_nvsd_namespace_object_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	tstring *t_pin_hdr = NULL;
	tstring *t_val_end_hdr = NULL;
	tstring *t_val_start_hdr = NULL;
	const bn_binding *bn_num = NULL;
	uint32_t allocated = 0;
	uint32_t size = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(bindings);

	bail_null(name_parts);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	/* Form Pin header CLI */
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_pin_hdr,
			"/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/pin_header_name", namespace);
	bail_error_null(err, t_pin_hdr);

	if(!ts_equal_str(t_pin_hdr, "", false)) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s pinned-object pin-header %s",
				namespace, ts_str(t_pin_hdr));
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);
	}

	/* Form val start header CLI */
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_val_start_hdr,
			"/nkn/nvsd/origin_fetch/config/%s/object/validity/start_header_name", namespace);
	bail_error_null(err, t_val_start_hdr);

	if(!ts_equal_str(t_val_start_hdr, "", false)) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s object validity-begin-header %s",
				namespace, ts_str(t_val_start_hdr));
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);
	}


#if 0
	/* Form val end header CLI */
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_val_end_hdr,
			"/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/validity/end_header_name", namespace);
	bail_error_null(err, t_val_end_hdr);

	if(!ts_equal_str(t_val_end_hdr, " ", false)) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s pinned-object validity-end-header %s",
				namespace, ts_str(t_val_end_hdr));
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);
	}

#endif

	/* Form allocate CLI */
	err = bn_binding_array_get_binding_by_name_fmt(bindings, &bn_num,
			"/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/cache_size", namespace);
	bail_error_null(err, bn_num);

	err = bn_binding_get_uint32(bn_num, ba_value, NULL, &allocated);
	bail_error(err);

	if(allocated != 0 ) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s pinned-object cache-capacity %u",
				namespace, allocated);
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);
	}

	/* Form size CLI */
	err = bn_binding_array_get_binding_by_name_fmt(bindings, &bn_num,
			"/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/max_obj_size", namespace);
	bail_error_null(err, bn_num);

	err = bn_binding_get_uint32(bn_num, ba_value, NULL, &size);
	bail_error(err);


	if(size != 0 ) {
		err = ts_new_sprintf(&rev_cmd,
				"namespace %s pinned-object max-obj-size %u",
				namespace, size);
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
		ts_free(&rev_cmd);
	}

	/* Consume bindings */
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/pin_header_name", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/object/validity/start_header_name", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/validity/end_header_name", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/cache_size", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/max_obj_size", namespace);
	bail_error(err);



bail:
	ts_free(&t_val_end_hdr);
	ts_free(&t_val_start_hdr);
	ts_free(&t_pin_hdr);
	ts_free(&rev_cmd);
	return err;

}



static int
cli_nvsd_namespace_rtsp_pub_point_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	const char *pub_point = NULL;
	tstring *t_sdp_value = NULL;
	tstring *t_start_time = NULL;
	tstring *t_end_time = NULL;
	tstring *t_mode_value = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(bindings);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	pub_point = tstr_array_get_str_quick(name_parts, 5);
	bail_null(pub_point);

	/* Get the sdp value */
	err = mdc_get_binding_tstr_fmt(cli_mcc, 
			NULL, NULL, NULL, 
			&t_sdp_value, 
			NULL,
			"/nkn/nvsd/namespace/%s/pub-point/%s/sdp",
			namespace, pub_point);
	bail_error(err);

	/* Get the start-time  */
	err = mdc_get_binding_tstr_fmt(cli_mcc, 
			NULL, NULL, NULL, 
			&t_start_time, 
			NULL, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/start-time", 
			namespace, pub_point);
	bail_error(err);

	/* Get the end-time  */
	err = mdc_get_binding_tstr_fmt(cli_mcc, 
			NULL, NULL, NULL, 
			&t_end_time, 
			NULL, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/end-time", 
			namespace, pub_point);
	bail_error(err);

	/* Get the mode value */
	err = mdc_get_binding_tstr_fmt(cli_mcc, 
			NULL, NULL, NULL, 
			&t_mode_value, 
			NULL, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/mode", 
			namespace, pub_point);
	bail_error(err);


	if (t_sdp_value && (ts_length(t_sdp_value) == 0) ) {
		/* Default case  - Do nothing */
		if( t_mode_value &&
			(ts_length(t_mode_value) > 0) &&
			(ts_equal_str(t_mode_value, "on-demand", true))) {
			err = ts_new_sprintf(&rev_cmd, "namespace %s live-pub-point %s receive-mode on-demand",
				namespace, pub_point);
			bail_error(err);
		}
	}
	else if (t_sdp_value && ts_length(t_sdp_value) > 0) {
		if ( t_mode_value && 
			(ts_length(t_mode_value) > 0) &&
			(ts_equal_str(t_mode_value, "immediate", true))) {

			err = ts_new_sprintf(&rev_cmd, "namespace %s live-pub-point %s sdp %s immediate",
				namespace, pub_point, ts_str(t_sdp_value));
			bail_error(err);
		} else if ( (ts_length(t_mode_value) > 0) &&
				(ts_equal_str(t_mode_value, "start-time", true))) {
			if ( t_end_time && ts_length(t_end_time) > 0) {
				err = ts_new_sprintf(&rev_cmd, "namespace %s live-pub-point %s" 
						" receive-mode sdp %s start-time %s end-time %s",
						namespace, pub_point, ts_str(t_sdp_value),
						ts_str(t_start_time), ts_str(t_end_time));
				bail_error(err);
			} else {
				err = ts_new_sprintf(&rev_cmd, "namespace %s live-pub-point %s"
						" receive-mode sdp %s start-time %s",
						namespace, pub_point, ts_str(t_sdp_value),
						ts_str(t_start_time));
				bail_error(err);
			}
		} else {
			// This is not a terminal
			err = ts_new_sprintf(&rev_cmd, "namespace %s live-pub-point %s receive-mode sdp %s",
				namespace, pub_point, ts_str(t_sdp_value));
			bail_error(err);
		}
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
	}

	/* Consume all nodes */
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/sdp",
			namespace, pub_point);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/start-time",
			namespace, pub_point);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/end-time",
			namespace, pub_point);
	bail_error(err);
#if 1
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
			"/nkn/nvsd/namespace/%s/pub-point/%s/mode",
			namespace, pub_point);
	bail_error(err);
#endif
//    	err = tstr_array_append_str(ret_reply->crr_nodes, name);
//	bail_error(err);

bail:
	ts_free(&t_sdp_value);
	ts_free(&t_start_time);
	ts_free(&t_end_time);
	ts_free(&t_mode_value);
	ts_free(&rev_cmd);
	return err;
}

int
cli_nvsd_ns_cache_pin_show_config(void			*data,
				  const cli_command 	*cmd,
				  const tstr_array	*cmd_line,
				  const tstr_array	*params)
{
	int err = 0;
	const char *c_namespace = NULL;
        node_name_t nodename = {0};
        tstring *checksum = NULL;
        

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	cli_printf( _("\n  Object Settings:\n"));

	cli_printf_query( _("    Validity-Begin-Header:"
   "#/nkn/nvsd/origin_fetch/config/%s/object/validity/start_header_name# \n"),
			  c_namespace);

        snprintf(nodename,sizeof(nodename),"/nkn/nvsd/namespace/%s/client-response/checksum",c_namespace);
 
        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                                &checksum, nodename, NULL);
        bail_null(checksum);
       
        if(atoi(ts_str(checksum)) == 0) {
            cli_printf(_("    Object Checksum included: no\n"));
        }else {
            cli_printf(_("    Object Checksum included: yes\n"));
        }
        cli_printf(_("    No.of bytes Included in Object checksum: %s\n"),ts_str(checksum));

	cli_printf( _("\n  Cache Pinning:\n"));

	cli_printf_query( _("    Auto Cache-Pinning Enabled: "
	"#/nkn/nvsd/namespace/%s/pinned_object/auto_pin# \n"),
			  c_namespace);

	cli_printf_query( _("    Cache-Pinning Enabled: "
	"#/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/enable# \n"),
			  c_namespace);

	cli_printf_query( _("    Max-Object-Size: "
	"#/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/max_obj_size# (KB)\n"),
			  c_namespace);

	cli_printf_query( _("    Cache-Capacity: "
	"#/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/cache_size# (GB)\n"),
			  c_namespace);

	cli_printf_query( _("    Pin-Header: "
   "#/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/pin_header_name# \n"),
			  c_namespace);

#if 0
	cli_printf_query( _("    Validity-End-Header: "
"#/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/validity/end_header_name# \n"),
			  c_namespace);
#endif

bail:
        ts_free(&checksum);
	return err;
}


int
cli_nvsd_ns_media_cache_show_config(void			*data,
				  const cli_command 	*cmd,
				  const tstr_array	*cmd_line,
				  const tstr_array	*params)
{
	int err = 0;
	const char *c_namespace = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	cli_printf( _("\n  Cache-Tier Specific Read Size Configs:\n"));

	cli_printf_query( _("    For SATA:"
   "#/nkn/nvsd/namespace/%s/mediacache/sata/readsize# Kbytes\n"),
			  c_namespace);

	cli_printf_query( _("    For SAS:"
   "#/nkn/nvsd/namespace/%s/mediacache/sas/readsize# Kbytes\n"),
			  c_namespace);

	cli_printf_query( _("    For SSD:"
   "#/nkn/nvsd/namespace/%s/mediacache/ssd/readsize# Kbytes\n"),
			  c_namespace);

	cli_printf( _("\n  Cache-Tier Specific Free Block Threshold Configs:\n"));

	cli_printf_query( _("    For SATA:"
   "#/nkn/nvsd/namespace/%s/mediacache/sata/free_block_thres# percentage\n"),
			  c_namespace);

	cli_printf_query( _("    For SAS:"
   "#/nkn/nvsd/namespace/%s/mediacache/sas/free_block_thres# percentage\n"),
			  c_namespace);

	cli_printf_query( _("    For SSD:"
   "#/nkn/nvsd/namespace/%s/mediacache/ssd/free_block_thres# percentage\n"),
			  c_namespace);

	cli_printf( _("\n  Cache-Tier Specific Group Read Configs:\n"));

	cli_printf_query( _("    Group read enabled For SATA:"
   "#/nkn/nvsd/namespace/%s/mediacache/sata/group_read_enable# \n"),
			  c_namespace);

	cli_printf_query( _("    Group read enabled For SAS:"
   "#/nkn/nvsd/namespace/%s/mediacache/sas/group_read_enable# \n"),
			  c_namespace);

	cli_printf_query( _("    Group read enabled For SSD:"
   "#/nkn/nvsd/namespace/%s/mediacache/ssd/group_read_enable# \n"),
			  c_namespace);
	/*
	 * Adding the resource limit cache-tier capacity introduced for crawler here
	 */
	cli_printf( _("\n  Cache-Tier Specific Capacity Limit Configs:\n"));

	cli_printf_query( _("    For SATA:"
   "#/nkn/nvsd/namespace/%s/resource/cache/sata/capacity# (MB) \n"),
			  c_namespace);

	cli_printf_query( _("    For SAS:"
   "#/nkn/nvsd/namespace/%s/resource/cache/sas/capacity# (MB) \n"),
			  c_namespace);

	cli_printf_query( _("    For SSD:"
   "#/nkn/nvsd/namespace/%s/resource/cache/ssd/capacity# (MB)\n"),
			  c_namespace);

bail:
	return err;
}
int
cli_nvsd_ns_live_pub_point_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	tstr_array *pubs = NULL;
	const char *c_namespace = NULL;
	int num_pubs = 0, idx = 0;
	const char *pub_point = NULL;
	tbool t_status = false, t_caching = false;
	char *bn_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	cli_printf( _("\n  Live Pub-Point Details:\n")); 
	err = cli_namespace_get_pub_points(&pubs, false, c_namespace);
	bail_error(err);

    	num_pubs = tstr_array_length_quick(pubs);

	for( idx = 0; idx < num_pubs; idx++) {
		pub_point = tstr_array_get_str_quick(pubs, idx);
		bail_null(pub_point);

		cli_printf(_("    Pub-point Name: %s\n"), pub_point);
		bail_error(err);

		bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/active", c_namespace, pub_point);
		bail_null(bn_name);

	 	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_status, bn_name, NULL);
		bail_error(err);

		cli_printf_query( _(" %24s: %s\n"), "Status",
				t_status ? "Active" : "Inactive");

		bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/caching/enable", c_namespace, pub_point);
		bail_null(bn_name);

	 	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_caching, bn_name, NULL);
		bail_error(err);

		cli_printf_query( _(" %24s: %s\n"), "Caching",
				t_caching ? "Enable" : "Disable");
#if 0
		bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/caching/format/chunk-flv", c_namespace, pub_point);
		bail_null(bn_name);

	 	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_chunk_flv, bn_name, NULL);
		bail_error(err);

		cli_printf_query( _(" %24s: %s\n"), "Chunk-flv Format",
				t_chunk_flv ? "Enable" : "Disable");

		bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/caching/format/chunked-ts", c_namespace, pub_point);
		bail_null(bn_name);

	 	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_chunked_ts, bn_name, NULL);
		bail_error(err);

		cli_printf_query( _(" %24s: %s\n"), "Chunked-ts Format",
				t_chunked_ts ? "Enable" : "Disable");

		bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/caching/format/frag-mp4", c_namespace, pub_point);
		bail_null(bn_name);

	 	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_frag_mp4, bn_name, NULL);
		bail_error(err);

		cli_printf_query( _(" %24s: %s\n"), "Frag-mp4 Format",
				t_frag_mp4 ? "Enable" : "Disable");

		bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point/%s/caching/format/moof", c_namespace, pub_point);
		bail_null(bn_name);

	 	err =  mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
				&t_moof, bn_name, NULL);
		bail_error(err);

		cli_printf_query( _(" %24s: %s\n"), "Moof Format",
				t_moof ? "Enable" : "Disable");
#endif
		cli_printf(_(" %24s: %s\n"), "Mode", "On-Demand");

#if 0
Commented for bug# 4847
		cli_printf_query( _(" %24s: "
				"#/nkn/nvsd/namespace/%s/pub-point/%s/mode# \n"), 
				"Mode", c_namespace, pub_point);

		cli_printf_query( _(" %24s: "
				"#/nkn/nvsd/namespace/%s/pub-point/%s/sdp# \n"), 
				"Sdp", c_namespace, pub_point);

		cli_printf_query( _(" %24s: "
				"#/nkn/nvsd/namespace/%s/pub-point/%s/start-time# \n"), 
				"Start-time", c_namespace, pub_point);

		cli_printf_query( _(" %24s: "
				"#/nkn/nvsd/namespace/%s/pub-point/%s/end-time# \n"), 
				"End-time", c_namespace, pub_point);
#endif
	}



bail:
	return err;
}

int 
cli_namespace_get_pub_points(
        tstr_array **ret_names,
        tbool hide_display, const char *namespace)
{
    int err = 0;
    tstr_array *pub_points = NULL;
    tstr_array_options opts;
    tstr_array *pub_config = NULL;
    char *bn_name = NULL;
    bn_binding *display_binding = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&pub_points, &opts);
    bail_error(err);

    bn_name = smprintf("/nkn/nvsd/namespace/%s/pub-point", namespace);
    bail_null(bn_name);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &pub_config, 
            bn_name, NULL);
    bail_error_null(err, pub_config);

    err = tstr_array_concat(pub_points, &pub_config);
    bail_error(err);

    *ret_names = pub_points;
    pub_points = NULL;
bail:
    tstr_array_free(&pub_points);
    tstr_array_free(&pub_config);
    safe_free(bn_name);
    bn_binding_free(&display_binding);
    return err;
}

int
cli_nvsd_namespace_show_http_proto_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	err = cli_printf_query( _("\n  Delivery Protocol: HTTP	    Status Enabled: " 	
		 "#/nkn/nvsd/namespace/%s/delivery/proto/http#\n"), ns);
	bail_error(err);

	err = cli_nvsd_ns_origin_fetch_show_config(data, cmd, cmd_line, params);
	bail_error(err);

	err = cli_nvsd_ns_origin_request_show_config(data, cmd, cmd_line, params);
	bail_error(err);

	err = cli_nvsd_ns_client_request_show_config(data, cmd, cmd_line, params);
	bail_error(err);

	err = cli_nvsd_ns_client_response_show_config(data, cmd, cmd_line, params);
	bail_error(err);

	err = cli_nvsd_ns_cache_index_show_config(data, cmd, cmd_line, params);
	bail_error(err);

bail:
	return err;
}


int
cli_nvsd_namespace_show_rtsp_proto_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;
	
	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	err = cli_printf_query( _("\n  Delivery Protocol: RTSP	    Status Enabled: " 	
		 "#/nkn/nvsd/namespace/%s/delivery/proto/rtsp#\n"), ns);
	bail_error(err);

	//err = cli_nvsd_namespace_delivery_proto_match_details(data, cmd, cmd_line, params, "rtsp");
	//bail_error(err);

	err = cli_nvsd_ns_origin_fetch_rtsp_show_config(data, cmd, cmd_line, params);
	bail_error(err);

bail:
	return err;

}

int 
cli_nvsd_ns_origin_fetch_rtsp_show_config(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
	int err = 0;
	const char *ns = NULL;
	bn_binding *bn_origin_fetch_obj_size = NULL;
	uint32 origin_fetch_threshold = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	err = cli_printf(
	    _("\n  Origin Fetch Configuration:\n"));
	bail_error(err);

	err = cli_printf_query(
	    _("    Cache-Age (Default): "
		"#/nkn/nvsd/origin_fetch/config/%s/rtsp/cache-age/default# (seconds)\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("    Cache Age Threshold: "
		"#/nkn/nvsd/origin_fetch/config/%s/rtsp/content-store/media/cache-age-threshold# (seconds)\n"), ns);
	bail_error(err);

	err = cli_printf_query(
	    _("    Cache-Directive: "
		"#/nkn/nvsd/origin_fetch/config/%s/rtsp/cache-directive/no-cache#\n"), ns);
	bail_error(err);

	err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_origin_fetch_obj_size, 
	    NULL, "/nkn/nvsd/origin_fetch/config/%s/rtsp/content-store/media/object-size-threshold", ns);
	bail_error(err);

	err = bn_binding_get_uint32(bn_origin_fetch_obj_size, ba_value, NULL, &origin_fetch_threshold);
	bail_error(err);
	if (origin_fetch_threshold) {
	err = cli_printf(
		_("    Object Size Threshold: %u Bytes\n"), origin_fetch_threshold);
	bail_error(err);
	} else {
	err = cli_printf(
		_("    Object Size Threshold: NONE (Always Cached)\n"));
	bail_error(err);

	}

	err = cli_nvsd_ns_rtsp_cache_index_show_config(data, cmd, cmd_line, params);
	bail_error(err);


bail:
	bn_binding_free(&bn_origin_fetch_obj_size);
	return err;
}

#if 0
static int
cli_nvsd_rtsp_uri_regex_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err =0;
    tstring *t_uri_regex = NULL;
    match_type_t mt = mt_none;
    uint32 precedence = 0;
    const char *namespace = NULL;

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = cli_get_rtsp_match_type(namespace, &mt);
    bail_error(err);

    err = cli_nvsd_ns_get_precedence(namespace, "rtsp", &precedence);
    bail_error(err);


    /* If the match type is for this node, 
     * only then create the command 
     */
    if (mt_rtsp_uri_regex == mt) {

		err = mdc_get_binding_tstr(cli_mcc, 
			    NULL, NULL, NULL, &t_uri_regex, name, NULL);
		bail_error(err);

		if (!t_uri_regex)
			goto bail;
		if (ts_length(t_uri_regex) > 0) {

			if (precedence != 0) {

				err = tstr_array_append_sprintf(
						ret_reply->crr_cmds, 
						"namespace %s delivery protocol rtsp match uri regex \"%s\" precedence %d",
						namespace, 
						ts_str(t_uri_regex), precedence);
				bail_error(err);
			}
			else {
				err = tstr_array_append_sprintf(
						ret_reply->crr_cmds, 
						"namespace %s delivery protocol rtsp match uri regex \"%s\"",
						namespace, 
						ts_str(t_uri_regex));
				bail_error(err);
			}
		}    

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/namespace/%s/rtsp/match/type",
				namespace); 
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/namespace/%s/rtsp/match/precedence",
				namespace); 
		bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

bail:
    return(err);
}
#endif


int
cli_nvsd_ns_client_request_reval_always_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	const char *namespace = NULL;
	tstring *rev_cmd = NULL;
	int retval_reval = -1;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	err = cli_nvsd_ns_is_reval_always_enabled(namespace, &retval_reval, bindings);
	bail_error(err);

	if(retval_reval == client_request_reval_always_inline_enabled) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request cache-hit action revalidate-always inline",
				namespace);
		bail_error(err);
	}
	else if(retval_reval == client_request_reval_always_offline_enabled) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request cache-hit action revalidate-always", 
				namespace);
		bail_error(err);
	}

        if(rev_cmd){
	    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	    bail_error(err);
        }

bail:
	ts_free(&rev_cmd);
	return err;
}

int 
cli_nvsd_ns_cache_control_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tstring *age = NULL;
	tstring *action = NULL;
	const char *namespace = NULL;
	char *node_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &age,
			"%s/client-request/cache-control/max_age", name);
	bail_error_null(err, age);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &action,
			"%s/client-request/cache-control/action", name);
	bail_error_null(err, action);

	if(action == NULL)
		goto bail;

	if(age == NULL)
		goto bail;

	if(strcmp(ts_str(action), "")) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request cache-control max-age %s action %s", 
				namespace, ts_str(age), ts_str(action));
		bail_error(err);
	} else {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request cache-control max-age %s", 
				namespace, ts_str(age));
		bail_error(err);
	}
	
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/client-request/cache-control/action", name);
	bail_error(err);

bail:
	safe_free(node_name);
	ts_free(&rev_cmd);
	ts_free(&age);
	ts_free(&action);
	return err;
}

int 
cli_nvsd_namespace_cluster_hash_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	uint32 cluster_hash = 0;
	const char *namespace = NULL;
	char *node_name = NULL;
        tstring *rev_cmd = NULL;
	tstring *t_binding_val = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	node_name = smprintf("/nkn/nvsd/namespace/%s/cluster-hash", namespace);
	bail_null(node_name);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val,
			"/nkn/nvsd/namespace/%s/cluster-hash", namespace);
	bail_error_null(err, t_binding_val);
	cluster_hash = atoi(ts_str(t_binding_val));

	if(cluster_hash == 1) {
		err = ts_new_sprintf(&rev_cmd, 
			"namespace %s cluster-hash base-url",
			namespace);
		bail_error(err);

	}else if(cluster_hash == 2) {
		err = ts_new_sprintf(&rev_cmd, 
			"namespace %s cluster-hash complete-url",
			namespace);
		bail_error(err);
	}
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	/* Consume bindings */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	safe_free(node_name);
	return err;
	
}

int 
cli_nvsd_namespace_generic_callback(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	const char *namespace = NULL;
        cli_revmap_callback callback_func_ptr = data;
 
	if( safe_strcmp(tstr_array_get_str_quick(name_parts, 2),"origin_fetch"))
		namespace = tstr_array_get_str_quick(name_parts, 3);
        else 
		namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

        if ( safe_strcmp(namespace,"mfc_probe"))
		err = (*callback_func_ptr)(0,cmd,bindings,name,name_parts,value,binding,ret_reply);

bail:
	return err;
}

int 
cli_nvsd_origin_fetch_byte_range_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	tbool t_preserve = false;
	tbool t_align = false;
	const char *namespace = NULL;
	char *node_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/byte-range/preserve", namespace);
	bail_null(node_name);

	err = bn_binding_array_get_value_tbool_by_name(bindings, node_name, NULL, &t_preserve);
	bail_error(err);
	safe_free(node_name);

	node_name = smprintf("/nkn/nvsd/origin_fetch/config/%s/byte-range/align", namespace);
	bail_null(node_name);

	err = bn_binding_array_get_value_tbool_by_name(bindings, node_name, NULL, &t_align);
	bail_error(err);
	safe_free(node_name);

	if ( !t_preserve ) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http origin-fetch byte-range ignore", 
				namespace);
		bail_error(err);
	} else {
		if (t_align) {
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http origin-fetch byte-range preserve align", namespace);
			bail_error(err);
		} else {
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http origin-fetch byte-range preserve no-align", namespace);
			bail_error(err);
		}
	}

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/byte-range/preserve", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/byte-range/align", namespace);
	bail_error(err);

bail:
	safe_free(node_name);
	ts_free(&rev_cmd);
	return err;
}

static int cli_nvsd_origin_fetch_rtsp_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(name);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	bail_null(value);
	bail_null(name);

	if (bn_binding_name_pattern_match(name, "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-directive/no-cache")) {
		if(strcmp(value, "follow")) {

			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol rtsp origin-fetch cache-directive no-cache %s", 
					namespace, value);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);

		}

	} else if (bn_binding_name_pattern_match(name, "/nkn/nvsd/origin_fetch/config/*/rtsp/cache-age/default")) {
		if(strcmp(value, "28800")) {

			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol rtsp origin-fetch cache-age-default %s", 
					namespace, value);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);

		}
	} else if (bn_binding_name_pattern_match(name, "/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/cache-age-threshold")) {

		if(strcmp(value, "60")) {
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol rtsp origin-fetch content-store media cache-age-threshold %s", 
					namespace, value);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);

		}
	} else if (bn_binding_name_pattern_match(name,"/nkn/nvsd/origin_fetch/config/*/rtsp/content-store/media/object-size-threshold")) {

		if(strcmp(value, "0")) {
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol rtsp origin-fetch content-store media object-size %s", 
					namespace, value);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);

		}
	} else if (bn_binding_name_pattern_match(name,"/nkn/nvsd/namespace/*/delivery/proto/rtsp")) {
		namespace = tstr_array_get_str_quick(name_parts, 3);
		bail_null(namespace);

		if(strcmp(value, "false")) {
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol rtsp\n   exit", 
					namespace);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);

		}
	}	
	/* Consume bindings */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);


bail:
	ts_free(&rev_cmd);
	return err;
}

int 
cli_nvsd_origin_fetch_cache_fill_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	uint32 t_cache_fill = 0;
	const char *namespace = NULL;
	tstring *tstr_cache_fill = NULL;
	bn_type ret_type;;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);
	
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, &ret_type, &tstr_cache_fill, 
			"/nkn/nvsd/origin_fetch/config/%s/cache-fill", namespace);
	bail_error(err);

	t_cache_fill = atoi(ts_str(tstr_cache_fill));

	if ( t_cache_fill == 1 ) {
		err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http origin-fetch cache-fill aggressive", namespace);
		bail_error(err);
	} else if (t_cache_fill == 2) {
			err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http origin-fetch cache-fill client-driven", namespace);
			bail_error(err);
	}

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/cache-fill", namespace);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	return err;
}

int 
cli_nvsd_origin_fetch_compression_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *namespace = NULL;
    tstring *tstr_obj_min_size = NULL;
    tstring *tstr_obj_max_size = NULL;
    bn_type ret_type;;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);

    namespace = tstr_array_get_str_quick(name_parts, 4);
    bail_null(namespace);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    &ret_type, &tstr_obj_min_size,
	    "/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_min", namespace);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	    &ret_type, &tstr_obj_max_size,
	    "/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_max", namespace);
    bail_error(err);

    if(tstr_obj_max_size && tstr_obj_min_size) {
	err = ts_new_sprintf(&rev_cmd,
		"namespace %s object compression size-range %s %s",
		namespace, ts_str(tstr_obj_min_size),ts_str(tstr_obj_max_size));
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

    }
    /* Consume bindings */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
	    "/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_min",
	    namespace);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
	    "/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_max",
	    namespace);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    return err;
}
static int
cli_nvsd_ns_origin_request_read_retry_delay_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	tstring *rev_cmd = NULL;
	uint32 time_delay = 0;
	const char *namespace = NULL;
	bn_binding *bn_read_time_delay = NULL;
	int type = 0;
	tstring *t_type = NULL;
	tstring *t_binding_val = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-request/read/retry-delay", namespace);
	bail_error_null(err, t_binding_val);
	time_delay = atoi(ts_str(t_binding_val));
	ts_free(&t_binding_val);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-server/type", namespace);
	bail_error_null(err, t_binding_val);
	type = atoi(ts_str(t_binding_val));

	if(type == osvr_http_node_map) {
		if (time_delay != 100) {
			err = ts_new_sprintf(&rev_cmd, 
				"namespace %s delivery protocol http origin-request read retry-delay %u",
				namespace, time_delay);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
		}
	}
	/* Consume bindings */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	ts_free(&t_type);
	ts_free(&t_binding_val);
	bn_binding_free(&bn_read_time_delay);
	return(err);
}

static int
cli_nvsd_ns_origin_request_read_timeout_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	tstring *rev_cmd = NULL;
	uint32 time_out = 0;
	const char *namespace = NULL;
	bn_binding *bn_read_timeout = NULL;
	int type = 0;
	tstring *t_type = NULL;
	tstring *t_binding_val = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-request/read/interval-timeout", namespace);
	bail_error_null(err, t_binding_val);
	time_out = atoi(ts_str(t_binding_val));

	ts_free(&t_binding_val);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-server/type", namespace);
	bail_error_null(err, t_binding_val);
	type = atoi(ts_str(t_binding_val));

	if(type == osvr_http_node_map) {
		if (time_out != 100) {
			err = ts_new_sprintf(&rev_cmd, 
				"namespace %s delivery protocol http origin-request read interval-timeout %u",
				namespace, time_out);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
		}
	}
	/* Consume bindings */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	ts_free(&t_type);
	ts_free(&t_binding_val);
	bn_binding_free(&bn_read_timeout);
	return(err);
}

static int
cli_nvsd_ns_origin_request_connect_retry_delay_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	tstring *rev_cmd = NULL;
	uint32 time_delay = 0;
	const char *namespace = NULL;
	bn_binding *bn_connect_time_delay = NULL;
	int type = 0;
	tstring *t_type = NULL;
	tstring *t_binding_val = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-request/connect/retry-delay", namespace);
	bail_error_null(err, t_binding_val);
	time_delay = atoi(ts_str(t_binding_val));
	ts_free(&t_binding_val);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-server/type", namespace);
	bail_error_null(err, t_binding_val);
	type = atoi(ts_str(t_binding_val));

	if(type == osvr_http_node_map) {
		if (time_delay != 100) {
			err = ts_new_sprintf(&rev_cmd, 
				"namespace %s delivery protocol http origin-request connect retry-delay %u",
				namespace, time_delay);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
		}
	}
	/* Consume bindings */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	bn_binding_free(&bn_connect_time_delay);
	ts_free(&t_type);
	ts_free(&t_binding_val);
	return(err);
}

static int
cli_nvsd_ns_origin_request_connect_timeout_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err =0;
	tstring *rev_cmd = NULL;
	uint32 time_out = 0;
	const char *namespace = NULL;
	bn_binding *bn_connect_timeout = NULL;
	int type = 0;
	tstring *t_type = NULL;
	tstring *t_binding_val = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-request/connect/timeout", namespace);
	bail_error_null(err, t_binding_val);
	time_out = atoi(ts_str(t_binding_val));
	ts_free(&t_binding_val);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val, 
			"/nkn/nvsd/namespace/%s/origin-server/type", namespace);
	bail_error_null(err, t_binding_val);
	type = atoi(ts_str(t_binding_val));

	if(type == osvr_http_node_map) {
		if (time_out != 100) {
			err = ts_new_sprintf(&rev_cmd, 
				"namespace %s delivery protocol http origin-request connect timeout %u",
				namespace, time_out);
			bail_error(err);
			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			bail_error(err);
		}	
	}
	/* Consume bindings */
	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	ts_free(&t_type);
	ts_free(&t_binding_val);
	bn_binding_free(&bn_connect_timeout);
	return(err);
}

static int
cli_nvsd_ns_origin_request_host_header_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
        int err =0;
        tstring *rev_cmd = NULL;
        const char *namespace = NULL;
        bn_binding *bn_connect_timeout = NULL;
        tstring *t_binding_val = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(value);
        UNUSED_ARGUMENT(binding);

        namespace = tstr_array_get_str_quick(name_parts, 4);
        bail_null(namespace);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val,
                        "/nkn/nvsd/origin_fetch/config/%s/host-header/header-value", namespace);
        bail_error_null(err, t_binding_val);
        if(strcmp(ts_str(t_binding_val),"")!=0) {
                err = ts_new_sprintf(&rev_cmd,
                            "namespace %s delivery protocol http origin-request host-header set %s",
                            namespace, ts_str(t_binding_val));
                bail_error(err);
                err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
                bail_error(err);
        }
        /* Consume bindings */
        err = tstr_array_append_str(ret_reply->crr_nodes, name);
        bail_error(err);

bail:
        ts_free(&rev_cmd);
        ts_free(&t_binding_val);
        return(err);
}

static int
cli_nvsd_ns_client_response_dscp_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
        int err =0;
        tstring *rev_cmd = NULL;
        const char *namespace = NULL;
        tstring *t_binding_val = NULL;
        int dscp = 0;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(value);
        UNUSED_ARGUMENT(binding);

        namespace = tstr_array_get_str_quick(name_parts, 3);
        bail_null(namespace);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val,
                        "/nkn/nvsd/namespace/%s/client-response/dscp", namespace);
        bail_error_null(err, t_binding_val);
        dscp = strtoul(ts_str(t_binding_val), NULL, 10);
        if(dscp >= 0) {
                err = ts_new_sprintf(&rev_cmd,
                            "namespace %s delivery protocol http client-response dscp %s",
                            namespace, ts_str(t_binding_val));
                bail_error(err);
                err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
                bail_error(err);
        }
        /* Consume bindings */
        err = tstr_array_append_str(ret_reply->crr_nodes, name);
        bail_error(err);

bail:
        ts_free(&rev_cmd);
        ts_free(&t_binding_val);
        return(err);
}


static int
cli_nvsd_ns_obj_checksum_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
        int err =0;
        tstring *rev_cmd = NULL;
        const char *namespace = NULL;
        tstring *t_binding_val = NULL;
        int checksum = 0;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(value);
        UNUSED_ARGUMENT(binding);

        namespace = tstr_array_get_str_quick(name_parts, 3);
        bail_null(namespace);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_binding_val,
                        "/nkn/nvsd/namespace/%s/client-response/checksum", namespace);
        bail_error_null(err, t_binding_val);
        checksum = strtoul(ts_str(t_binding_val), NULL, 10); 
        if(checksum > 0) {
                err = ts_new_sprintf(&rev_cmd,
                            "namespace %s object checksum %s",
                            namespace, ts_str(t_binding_val));
                bail_error(err);
                err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
                bail_error(err);
        }
        /* Consume bindings */
        err = tstr_array_append_str(ret_reply->crr_nodes, name);
        bail_error(err);

bail:
        ts_free(&rev_cmd);
        ts_free(&t_binding_val);
        return(err);
}

int
cli_nvsd_namespace_smap_nfs_print_msg(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_printf_error(_("NFS server map not supported\n"));

	return err;

}

int
cli_ns_show_resource_monitoring_counters(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *ns = NULL;
	tbool is_http = false;
	tbool is_rtsp = false;
	char *node = NULL;
	char *bw_node = NULL;
	char *trans_node = NULL;
	float32 totalbw = 0;
	int64_t totalbwbps;
	int64_t trans_per_sec = 0;
	bn_binding *binding = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	node = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/http", ns);
	bail_null(node);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &is_http, node, NULL);
	bail_error(err);

	safe_free(node);
	node = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/rtsp", ns);
	bail_null(node);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&is_rtsp, node, NULL);
	bail_error(err);

	/* If HTTP is configured for this namespace, display http
	 * counters.
	 */
	bw_node = smprintf("/stats/state/sample/ns_served_bytes/node"
		    "/\\/nkn\\/nvsd\\/resource_mgr\\/"
		    "namespace\\/%s\\/%s\\/served_bytes/last/value",
		    ns,is_http ? "http":"rtsp" );
	bail_null(bw_node);

	err = mdc_get_binding(cli_mcc, NULL, NULL,
			false, &binding, bw_node, NULL);
	bail_error(err);


	if (binding ) {
		err = bn_binding_get_int64(binding, ba_value, NULL, &totalbwbps);
		bail_error(err);
		bn_binding_free(&binding);
		/* sample for every 5 secs
		* so divide by 5 for every sec
		* mutlity by 8 get to get in BITS
		* divide by 1000000 to get in Mbps.
		*/

		totalbw = (((totalbwbps/5 )* 8)/1000000.0f);
	}

	trans_node = smprintf("/stats/state/sample/ns_transactions/node"
		    "/\\/nkn\\/nvsd\\/resource_mgr\\/"
		    "namespace\\/%s\\/%s\\/transactions/last/value",
		    ns,is_http ? "http":"rtsp" );
	bail_null(trans_node);

	err = mdc_get_binding(cli_mcc, NULL, NULL,
			false, &binding, trans_node, NULL);
	bail_error(err);


	if (binding ) {
		err = bn_binding_get_int64(binding, ba_value, NULL, &trans_per_sec);
		bail_error(err);
		bn_binding_free(&binding);
	}


	if (is_http) {
		cli_printf(_("HTTP Resource Monitoring Counters:\n"));
		cli_printf_query(_("    Client Active Connections:         #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/http/curr_ses#\n"), ns);
		cli_printf(_("    Current Application Bandwidth:     %.3f Mbps\n"), totalbw);
		cli_printf_query(_("    Served Bytes:                      #e:N.A.~w:20~/nkn/nvsd/resource_mgr/namespace/%s/http/served_bytes#\n"), ns);
		cli_printf(_("    Transaction Per sec:               %lu\n"),trans_per_sec);
		cli_printf_query(_("    Cache Hit Ratio(Bytes):            #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/http/cache_hit_ratio_on_bytes#\n"), ns); 
		cli_printf_query(_("    Cache Hit Ratio(Req):              #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/http/cache_hit_ratio_on_req#\n"), ns);
		cli_printf_query(_("    Total Object cached:               #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/disk/obj_count#\n"), ns);
		cli_printf_query(_("        SATA:                          #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/disk/sata/obj_count#\n"), ns);
		cli_printf_query(_("        SAS:                           #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/disk/sas/obj_count#\n"), ns);
		cli_printf_query(_("        SSD:                           #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/disk/ssd/obj_count#\n"), ns);


	}

	/* If RTSP is configured for this namespace, display rtsp
	 * counters.
	 */
	if (is_rtsp) {
		cli_printf(_("RTSP Resource Monitoring Counters:\n"));
		cli_printf_query(_("    Client Active Connections:         #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/rtsp/curr_ses#\n"), ns);
		cli_printf(_("    Current Application Bandwidth:     %.3f Mbps\n"), totalbw);
		cli_printf_query(_("    Served Bytes:                      #e:N.A.~w:20~/nkn/nvsd/resource_mgr/namespace/%s/rtsp/served_bytes#\n"), ns);
		cli_printf(_("    Transaction Per sec:               %lu\n"),trans_per_sec);
		cli_printf_query(_("    Cache Hit Ratio(Bytes):            #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/rtsp/cache_hit_ratio_on_bytes #\n")
					        , ns);
		cli_printf_query(_("    Cache Hit Ratio(Req):              #e:N.A.~w:20~/nkn/nvsd/resource_mgr/ns/%s/http/cache_hit_ratio_on_req#\n"), ns);
 
	}


bail:
	safe_free(node);
	safe_free(bw_node);
	safe_free(trans_node);
	bn_binding_free(&binding);
	return err;

}

int
cli_ns_show_counters(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *ns = NULL;
	tbool is_http = false;
	tbool is_rtsp = false;
	uint64_t filter_bw = 0;
	int64_t filter_acp_rate = 0;
	int64_t filter_rej_rate = 0;
	char *node = NULL;
        tstring *filter_mode = NULL;
	bn_binding *binding = NULL;

	ns = tstr_array_get_str_quick(params, 0);
	bail_null(ns);

	cli_ns_show_resource_monitoring_counters(data,cmd,cmd_line,params);

	node = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/http", ns);
	bail_null(node);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &is_http, node, NULL);
	bail_error(err);
	safe_free(node);

	node = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/rtsp", ns);
	bail_null(node);
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
			&is_rtsp, node, NULL);
	bail_error(err);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &filter_mode,
			"/nkn/nvsd/http/config/filter/mode", NULL);
        bail_null(filter_mode);
	safe_free(node);

	/* Display filter bw only in packet mode */
	node = smprintf("/stat/nkn/namespace/%s/url_filter_bw", ns);
	bail_null(node);
	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, node, NULL);
	bail_error(err);
	if (binding ) {
		err = bn_binding_get_uint64(binding, ba_value, NULL, &filter_bw);
		bail_error(err);
		bn_binding_free(&binding);
	}

	safe_free(node);
	node = smprintf("/stats/state/sample/filter_acp_rate/node/"
		    "\\/stat\\/nkn\\/namespace\\/%s\\/url_filter_acp_cnt/last/value", ns);
	bail_null(node);
	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, node, NULL);
	bail_error(err);
	if (binding ) {
		err = bn_binding_get_int64(binding, ba_value, NULL,
					    &filter_acp_rate);
		bail_error(err);
		bn_binding_free(&binding);
	}
	/* Stats interval is set to 5 secs. So divide by 5 */
	filter_acp_rate = filter_acp_rate / 5;

	safe_free(node);
	node = smprintf("/stats/state/sample/filter_rej_rate/node/"
		    "\\/stat\\/nkn\\/namespace\\/%s\\/url_filter_rej_cnt/last/value", ns);
	bail_null(node);
	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, node, NULL);
	bail_error(err);
	if (binding ) {
		err = bn_binding_get_int64(binding, ba_value, NULL,
					    &filter_rej_rate);
		bail_error(err);
		bn_binding_free(&binding);
	}
	/* Stats interval is set to 5 secs. So divide by 5 */
	filter_rej_rate = filter_rej_rate / 5;

	/* If HTTP is configured for this namespace, display http
	 * counters.
	 */
	if (is_http) {
		cli_printf(_("HTTP Client Counters:\n"));
		cli_printf_query(_("    Number of requests:                #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/requests#\n"), ns);
		cli_printf_query(_("    Number of responses:               #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/responses#\n"), ns);
		cli_printf_query(_("    Total Bytes Received:              #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/in_bytes#\n"), ns);
		cli_printf_query(_("    Total Bytes Sent:                  #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/out_bytes#\n"), ns);
		cli_printf_query(_("      From Memory Cache:               #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/out_bytes_ram#\n"), ns);
		cli_printf_query(_("      From Disk Cache:                 #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/out_bytes_disk#\n"), ns);
		cli_printf_query(_("      From Origin:                     #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/out_bytes_origin#\n"), ns);
		cli_printf_query(_("      From Tunnel:                     #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/out_bytes_tunnel#\n"), ns);
		cli_printf_query(_("    Cache Hits:                        #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/cache_hits#\n"), ns);
		cli_printf_query(_("    Cache Misses:                      #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/cache_miss#\n"), ns);
		cli_printf_query(_("    Responses with 2xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/resp_2xx#\n"), ns);
		cli_printf_query(_("    Responses with 3xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/resp_3xx#\n"), ns);
		cli_printf_query(_("    Responses with 4xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/resp_4xx#\n"), ns);
		cli_printf_query(_("    Responses with 5xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/resp_5xx#\n"), ns);
		cli_printf_query(_("    Number of expired objects served:  #e:N.A.~w:20~/stat/nkn/namespace/%s/http/client/expired_obj_served#\n"), ns);
		cli_printf(_("HTTP Origin Counters:\n"));
		cli_printf_query(_("    Number of requests:                #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/requests#\n"), ns);
		cli_printf_query(_("    Number of responses:               #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/responses#\n"), ns);
		cli_printf_query(_("    Total Bytes Received:              #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/in_bytes#\n"), ns);
		cli_printf_query(_("    Total Bytes Sent:                  #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/out_bytes#\n"), ns);
		cli_printf_query(_("    Responses with 2xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/resp_2xx#\n"), ns);
		cli_printf_query(_("    Responses with 3xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/resp_3xx#\n"), ns);
		cli_printf_query(_("    Responses with 4xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/resp_4xx#\n"), ns);
		cli_printf_query(_("    Responses with 5xx status code:    #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/resp_5xx#\n"), ns);
		cli_printf_query(_("    Number of Active Connections:      #e:N.A.~w:20~/stat/nkn/namespace/%s/http/server/active_conx#\n"), ns);
		cli_printf(_("Cache Pinning Counters:\n"));
		cli_printf_query(_("    Number of objects pinned:          #e:N.A.~w:20~/stat/nkn/namespace/%s/cache_pin_obj_count#\n"), ns);
		cli_printf_query(_("    Total bytes pinned:                #e:N.A.~w:20~/stat/nkn/namespace/%s/cache_pin_bytes_used#\n"), ns);
		cli_printf(_("URL Filter Counters:\n"));
		cli_printf_query(_("    Number of Transactions accepted:   #e:N.A.~w:20~/stat/nkn/namespace/%s/url_filter_acp_cnt#\n"), ns);
		cli_printf_query(_("    Number of Transactions rejected:   #e:N.A.~w:20~/stat/nkn/namespace/%s/url_filter_rej_cnt#\n"), ns);
		cli_printf_query(_("    Transaction accept rate:           %ld /sec\n"), filter_acp_rate);
		cli_printf_query(_("    Transaction reject rate:           %ld /sec\n"), filter_rej_rate);
		if (!strcmp(ts_str(filter_mode), "packet")) {
		cli_printf_query(_("    Packet mode Bandwidth:             %ld Mbps\n"), filter_bw);
		}

	}

	/* If RTSP is configured for this namespace, display rtsp
	 * counters.
	 */
	if (is_rtsp) {
		cli_printf(_("RTSP Client Counters:\n"));
		cli_printf_query(_("    Number of requests:                #w:20~/stat/nkn/namespace/%s/rtsp/client/requests#\n"), ns);
		cli_printf_query(_("    Number of responses:               #w:20~/stat/nkn/namespace/%s/rtsp/client/responses#\n"), ns);
		cli_printf_query(_("    Total Bytes Received:              #w:20~/stat/nkn/namespace/%s/rtsp/client/in_bytes#\n"), ns);
		cli_printf_query(_("    Total Bytes Sent:                  #w:20~/stat/nkn/namespace/%s/rtsp/client/out_bytes#\n"), ns);
		cli_printf_query(_("      From Memory Cache:               #w:20~/stat/nkn/namespace/%s/rtsp/client/out_bytes_ram#\n"), ns);
		cli_printf_query(_("      From Disk Cache:                 #w:20~/stat/nkn/namespace/%s/rtsp/client/out_bytes_disk#\n"), ns);
		cli_printf_query(_("      From Origin:                     #w:20~/stat/nkn/namespace/%s/rtsp/client/out_bytes_origin#\n"), ns);
		cli_printf_query(_("    Cache Hits:                        #w:20~/stat/nkn/namespace/%s/rtsp/client/cache_hits#\n"), ns);
		cli_printf_query(_("    Cache Misses:                      #w:20~/stat/nkn/namespace/%s/rtsp/client/cache_miss#\n"), ns);
		cli_printf_query(_("    Responses with 2xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/client/resp_2xx#\n"), ns);
		cli_printf_query(_("    Responses with 3xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/client/resp_3xx#\n"), ns);
		cli_printf_query(_("    Responses with 4xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/client/resp_4xx#\n"), ns);
		cli_printf_query(_("    Responses with 5xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/client/resp_5xx#\n"), ns);
		cli_printf(_("RTSP Origin Counters:\n"));
		cli_printf_query(_("    Number of requests:                #w:20~/stat/nkn/namespace/%s/rtsp/server/requests#\n"), ns);
		cli_printf_query(_("    Number of responses:               #w:20~/stat/nkn/namespace/%s/rtsp/server/responses#\n"), ns);
		cli_printf_query(_("    Total Bytes Received:              #w:20~/stat/nkn/namespace/%s/rtsp/server/in_bytes#\n"), ns);
		cli_printf_query(_("    Total Bytes Sent:                  #w:20~/stat/nkn/namespace/%s/rtsp/server/out_bytes#\n"), ns);
		cli_printf_query(_("    Responses with 2xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/server/resp_2xx#\n"), ns);
		cli_printf_query(_("    Responses with 3xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/server/resp_3xx#\n"), ns);
		cli_printf_query(_("    Responses with 4xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/server/resp_4xx#\n"), ns);
		cli_printf_query(_("    Responses with 5xx status code:    #w:20~/stat/nkn/namespace/%s/rtsp/server/resp_5xx#\n"), ns);
	}

bail:
        ts_free(&filter_mode);
	bn_binding_free(&binding);
	safe_free(node);
	return err;
}



int
cli_nvsd_namespace_http_cache_index_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);
    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index";
    cmd->cc_help_descr =	    N_("Configure cache-index scheme for HTTP URIs");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index";
    cmd->cc_help_descr =	    N_("Clears cache-index scheme configuration for HTTP URIs");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index header";
    cmd->cc_help_descr =	    N_("Configure scheme to use response header based cache-index");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index header";
    cmd->cc_help_descr =	    N_("Clears scheme to use response header based cache-index");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index header include";
    cmd->cc_help_descr =	    N_("Include 1 to 8 headers whose values will be added to cache-index name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index header include";
    cmd->cc_help_descr =	    N_("Clears All the Included headers whose values have added to cache-index name");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_basic_curr;
    cmd->cc_exec_callback  =	    cli_nvsd_ns_cache_index_header_delete_all;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index header include *";
    cmd->cc_help_exp =		    N_("<Header name>");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_basic_curr;
    cmd->cc_exec_callback =	    cli_nvsd_ns_cache_index_header_add;
    cmd->cc_revmap_type =	    crt_manual;
    cmd->cc_revmap_order =	    cro_namespace;
    cmd->cc_revmap_names =	    "/nkn/nvsd/origin_fetch/config/*/cache-index/include-headers/*/header-name";
    cmd->cc_revmap_callback =	    cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =	    (void*)cli_nvsd_ns_cache_index_header_revmap;
    cmd->cc_revmap_suborder =	    crso_delivery_proto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index header include *";
    cmd->cc_help_exp =		    N_("<Header name>");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_help_use_comp =	    true;
    cmd->cc_comp_pattern =	    "/nkn/nvsd/origin_fetch/config/$1$/cache-index/include-headers/*/header-name";
    cmd->cc_comp_type =		    cct_matching_values;
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_basic_curr;
    cmd->cc_exec_callback  =	    cli_nvsd_ns_cache_index_header_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object";
    cmd->cc_help_descr =	    N_("Configure a scheme to use response object based cache-index");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index object";
    cmd->cc_help_descr =	    N_("Clears scheme to use response objecy based cache-index");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include";
    cmd->cc_help_descr =	    N_("Include response object data for deriving the cache-index name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index object include";
    cmd->cc_help_descr =	    N_("Clears the Included response object data for deriving the cache-index name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum";
    cmd->cc_help_descr =	    N_("Include data checksum of n Bytes of payload in the cache-index name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no namespace * delivery protocol http cache-index object include checksum";
    cmd->cc_help_descr =	    N_("Clears Included data checksum of n Bytes of payload in the cache-index name");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_reset;
    cmd->cc_exec_name =		    "/nkn/nvsd/origin_fetch/config/$1$/cache-index/include-object/chksum-bytes";
    cmd->cc_exec_type =		    bt_uint32;
    cmd->cc_exec_name2 =	    "/nkn/nvsd/origin_fetch/config/$1$/cache-index/include-object";
    cmd->cc_exec_type2 =	    bt_bool;
    cmd->cc_exec_value2 =	    "false";
    cmd->cc_revmap_type =	    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum size";
    cmd->cc_help_descr =	    N_("Size of the object that needs to be used for checksum calculation");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum size *";
    cmd->cc_help_exp =		    N_("<Bytes>");
    cmd->cc_help_exp_hint =	    N_("Number of bytes to use for checksum calculation <number 0 to 32768>");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	    cli_nvsd_origin_fetch_cache_index_config;
    cmd->cc_revmap_type =	    crt_manual;
    cmd->cc_revmap_order =	    cro_namespace;
    cmd->cc_revmap_suborder =	    crso_delivery_proto;
    cmd->cc_revmap_names =	    "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-bytes";
    cmd->cc_revmap_callback =	    cli_nvsd_namespace_generic_callback;
    cmd->cc_revmap_data =	    (void*)cli_nvsd_origin_fetch_cache_index_offset_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum size * from-end";
    cmd->cc_help_descr =	    N_("Offset is from the end of file");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	    cli_nvsd_origin_fetch_cache_index_config;
    cmd->cc_revmap_type =	    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum size * offset";
    cmd->cc_help_descr =	    N_("Offset of the object for checksum calculation");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum size * offset *";
    cmd->cc_help_exp =		    N_("<Offset>");
    cmd->cc_help_exp_hint =	    N_("Object offset for checksum calculation <0 - 1073741823>");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	    cli_nvsd_origin_fetch_cache_index_config;
    cmd->cc_revmap_type =	    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index object include checksum size * offset * from-end";
    cmd->cc_help_descr =	    N_("Offset is from the end of file");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	    cli_nvsd_origin_fetch_cache_index_config;
    cmd->cc_revmap_type =	    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index "
				    "domain-name";
    cmd->cc_help_descr =	    N_("Configure scheme to handle domain name part in"
				    " the cache-index name for a URI");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index "
				    "domain-name include";
    cmd->cc_help_descr =	    N_("Always include the domain name in the "
				    "cache-index when creating a cache-index name");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/origin_fetch/config/$1$/cache-index/domain-name";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_exec_value =	    "include";
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_namespace;
    cmd->cc_revmap_suborder =	    crso_delivery_proto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol http cache-index "
				    "domain-name exclude";
    cmd->cc_help_descr =	    N_("Always exclude the domain name from the "
				    "cache-index when creating a cache-index name");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/origin_fetch/config/$1$/cache-index/domain-name";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_exec_value =	    "exclude";
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_namespace;
    cmd->cc_revmap_suborder =	    crso_delivery_proto;
    CLI_CMD_REGISTER;


bail:
    return err;
}


int
cli_nvsd_namespace_rtsp_cache_index_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);
    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol rtsp cache-index";
    cmd->cc_help_descr =	    N_("Configure cache-index scheme for HTTP URIs");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol rtsp cache-index "
				    "domain-name";
    cmd->cc_help_descr =	    N_("Configure scheme to handle domain name part in"
				    " the cache-index name for a URI");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol rtsp cache-index "
				    "domain-name include";
    cmd->cc_help_descr =	    N_("Always include the domain name in the "
				    "cache-index when creating a cache-index name");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-index/domain-name";
    cmd->cc_exec_value =	    "include";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_namespace;
    cmd->cc_revmap_suborder =	    crso_delivery_proto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "namespace * delivery protocol rtsp cache-index "
				    "domain-name exclude";
    cmd->cc_help_descr =	    N_("Always exclude the domain name from the "
				    "cache-index when creating a cache-index name");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/origin_fetch/config/$1$/rtsp/cache-index/domain-name";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_exec_value =	    "exclude";
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_namespace;
    cmd->cc_revmap_suborder =	    crso_delivery_proto;
    CLI_CMD_REGISTER;

bail:
    return err;
}

int
cli_nvsd_ns_cache_index_show_config(void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL;
    char *headers_binding = NULL;
    uint32 num_headers =0;
    tbool from_end;
    char *node = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    node = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/from-end", ns);
    bail_null(node);

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &from_end, node, NULL);
    bail_error(err);

    err = cli_printf(
		_("\n  Http Cache-Index Naming scheme :\n"));
    bail_error(err);

    err = cli_printf_query(
		_("    Domain name: #/nkn/nvsd/origin_fetch/config/%s/cache-index/domain-name#\n"), ns);
    bail_error(err);

    err = cli_printf_query(
		_("    Object checksum Included	: #/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object#\n"), ns);
    bail_error(err);

    if (from_end) {
	err = cli_printf_query(
		    _("    Object offset from end for checksum calculation 	: #/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-offset#\n"), ns);
    } else {
	err = cli_printf_query(
		    _("    Object offset from start for checksum calculation 	: #/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-offset#\n"), ns);
    }
    bail_error(err);

    err = cli_printf_query(
		_("    No.of bytes included in Object checksum	: #/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-bytes#\n"), ns);
    bail_error(err);

    err = cli_printf_query(
		_("    Headers Included	: #/nkn/nvsd/origin_fetch/config/%s/cache-index/include-header#\n"), ns);
    bail_error(err);

    headers_binding = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers/*/header-name", ns);
    bail_null(headers_binding);

    err = mdc_foreach_binding(cli_mcc, headers_binding, NULL, cli_nvsd_http_cache_index_include_headers_show,
 	   NULL, &num_headers);
    bail_error(err);

bail:
    safe_free(headers_binding);
    safe_free(node);
    return err;
}


int cli_nvsd_http_cache_index_include_headers_show(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *header_name = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(callback_data);
    UNUSED_ARGUMENT(value);

    if (idx != 0)
	cli_printf(_("\n"));

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &header_name);
    bail_error(err);

    if (header_name == NULL)
	goto bail;

    err = cli_printf(_("    %s   "), ts_str(header_name));
    bail_error(err);

    bail:
	ts_free(&header_name);
	return(err);
}

int
cli_nvsd_ns_rtsp_cache_index_show_config(void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    err = cli_printf(
		_("\n  Cache-Index Naming scheme :\n"));
    bail_error(err);

    err = cli_printf_query(
		_("    Domain Name: #/nkn/nvsd/origin_fetch/config/%s/rtsp/cache-index/domain-name#\n"), ns);
    bail_error(err);

bail:
    return err;
}


/*
 * exec_callback function for handling of follwoing commands:
 * namespace * origin-server http follow dest-ip use-client-ip bind-to *
 * namespace * origin-server http follow header * use-client-ip bind-to *
 */
int
cli_nvsd_ns_use_clip_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	const char *ns_name = NULL, *bind_ip = NULL, *header_value = NULL;
	int err = 0;
	uint32 err_code = 0;
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);


	/* get namespace name */
	ns_name = tstr_array_get_str_quick(params, 0);
	bail_null(ns_name);

	switch(cmd->cc_magic) {
		case cc_magic_ns_follow_dest_ip:
			/* param 1 is bind-ip address */
			bind_ip = tstr_array_get_str_quick(params, 1);
			bail_null(bind_ip);

			/* dest-ip as true */
			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
					bt_bool, 0, "true", NULL,
					"/nkn/nvsd/namespace/%s/origin-server/http/follow/dest-ip",
					ns_name);
			bail_error(err);
			break;

		case cc_magic_ns_follow_host_hdr:
			/* param 1 is header value */
			header_value = tstr_array_get_str_quick(params, 1);
			bail_null(header_value);

			/* 2 is bind ip address */
			bind_ip = tstr_array_get_str_quick(params, 2);
			bail_null(bind_ip);

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
					bt_string, 0, header_value, NULL,
					"/nkn/nvsd/namespace/%s/origin-server/http/follow/header",
					ns_name);
			bail_error(err);
			break;

		default:
			/* MUST NEVER COME HERE */
			err = 1;
			goto bail;
	}

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_bool, 0, "true", NULL,
			"/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip",
			ns_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_ipv4addr, 0, bind_ip, NULL,
			"/nkn/nvsd/namespace/%s/ip_tproxy",
			ns_name);
	bail_error(err);

	/* we don't use err_code, adding here as good practice */
	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &err_code, NULL);
	bail_error(err);

bail:
	bn_request_msg_free(&req);
	return err;
}

int
cli_nvsd_ns_seek_uri_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	const char *namespace = NULL;
    char *cmd_str = NULL;
	tstring *t_regex = NULL;
	tstring *t_map_string = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(bindings);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_regex, NULL,
		"/nkn/nvsd/namespace/%s/client-request/seek-uri/regex",
		namespace);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_map_string, NULL,
		"/nkn/nvsd/namespace/%s/client-request/seek-uri/map-string",
		namespace);
    bail_error(err);

    if (t_regex && (ts_length(t_regex) > 0) ) {

    cmd_str = smprintf("namespace %s delivery protocol http client-request "
    	"seek-uri url-match %s map-to %s",
		namespace, ts_str(t_regex),ts_str(t_map_string) );
    bail_null(cmd_str);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/seek-uri/regex", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/seek-uri/map-string", namespace);
	bail_error(err);

	/* Add to the revmap */
	err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
	bail_error(err);
    }
    else
    {
    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/seek-uri/regex", namespace);
    	bail_error(err);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/seek-uri/map-string", namespace);
    	bail_error(err);
    }

bail:
	safe_free(cmd_str);
	ts_free(&t_map_string);
	ts_free(&t_regex);
	return err;
}
/*---------------------------------------------------------------------------*/
int
cli_nvsd_ns_dynamic_uri_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    const char *namespace = NULL;
    char *cmd_str = NULL;
    tstring *t_no_match_tunnel = NULL;
    tstring *t_regex = NULL;
    tstring *t_map_string = NULL;
	tstring *t_tok_string = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);

    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_no_match_tunnel, NULL,
		"/nkn/nvsd/namespace/%s/client-request/dynamic-uri/no-match-tunnel",
		namespace);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_regex, NULL,
		"/nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex",
		namespace);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_map_string, NULL,
		"/nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string",
		namespace);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_tok_string, NULL,
		"/nkn/nvsd/namespace/%s/client-request/dynamic-uri/tokenize-string",
		namespace);
    bail_error(err);

    if (t_regex && (ts_length(t_regex) > 0) ) {
    	if (ts_equal_str(t_no_match_tunnel, "true", false)) {
    		cmd_str = smprintf("namespace %s delivery protocol http client-request "
    				"cache-index url-match %s map-to %s no-match-tunnel",
    				namespace, ts_str(t_regex),ts_str(t_map_string) );
    		bail_null(cmd_str);
    	}
    	else
    	{
    	    if (t_tok_string && (ts_length(t_tok_string) > 0) ) {
    	    	cmd_str = smprintf("namespace %s delivery protocol http client-request "
							"cache-index url-match %s map-to %s tokenize %s",
							namespace, ts_str(t_regex),ts_str(t_map_string),ts_str(t_tok_string) );
    	    	bail_null(cmd_str);
    	    }
    	    else
    	    {
    	    	cmd_str = smprintf("namespace %s delivery protocol http client-request "
							"cache-index url-match %s map-to %s",
							namespace, ts_str(t_regex),ts_str(t_map_string) );
    	    	bail_null(cmd_str);
    	    }
    	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/no-match-tunnel", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/tokenize-string", namespace);
	bail_error(err);

	/* Add to the revmap */
	err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
	bail_error(err);
    }
    else
    {
    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex", namespace);
    	bail_error(err);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string", namespace);
    	bail_error(err);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/no-match-tunnel", namespace);
    	bail_error(err);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/dynamic-uri/tokenize-string", namespace);
    	bail_error(err);

    }

bail:
	safe_free(cmd_str);
	ts_free(&t_no_match_tunnel);
	ts_free(&t_map_string);
	ts_free(&t_regex);
	return err;
}

int
cli_nvsd_ns_dynamic_uri_exec_cb(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_namespace = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_uri_regex = NULL;
	char *bn_map_str = NULL;
	char *bn_tokenize_str = NULL;
	char *bn_tunnel = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	bn_uri_regex = smprintf("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex",
			c_namespace);
	bail_null(bn_uri_regex);

	bn_tunnel = smprintf("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/no-match-tunnel",
			c_namespace);
	bail_null(bn_tunnel);

	bn_map_str = smprintf("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string",
			c_namespace);
	bail_null(bn_map_str);

	bn_tokenize_str = smprintf("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/tokenize-string",
			c_namespace);
	bail_null(bn_tokenize_str);

	if (cmd->cc_magic == cc_magic_ns_dyn_uri_all_add){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_regex,
				tstr_array_get_str_quick(params, 1),
				bn_uri_regex);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				tstr_array_get_str_quick(params, 2),
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_bool,
				"true",
				bn_tunnel);
		bail_error(err);
		if (ret_err)
			goto bail;
	}
	if (cmd->cc_magic == cc_magic_ns_dyn_uri_regex_add ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_regex,
				tstr_array_get_str_quick(params, 1),
				bn_uri_regex);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				tstr_array_get_str_quick(params, 2),
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;
	}
	if (cmd->cc_magic == cc_magic_ns_dyn_uri_regex_tokenize_add ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_regex,
				tstr_array_get_str_quick(params, 1),
				bn_uri_regex);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				tstr_array_get_str_quick(params, 2),
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;

	    if(!strcmp(tstr_array_get_str_quick(params, 3),"")) {
		    cli_printf(_("Tokenize string cannot be a empty one\n"));
		    goto bail;
	    }

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				tstr_array_get_str_quick(params, 3),
				bn_tokenize_str);
		bail_error(err);
		if (ret_err)
			goto bail;
	}
	if (cmd->cc_magic == cc_magic_ns_dyn_uri_regex_tokenize_delete ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				"",
				bn_tokenize_str);
		bail_error(err);
		if (ret_err)
			goto bail;
	}
	if (cmd->cc_magic == cc_magic_ns_dyn_uri_str_delete ){
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				"",
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;

	}
	if (cmd->cc_magic == cc_magic_ns_dyn_uri_all_delete ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_regex,
				"",
				bn_uri_regex);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				"",
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_bool,
				"false",
				bn_tunnel);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				"",
				bn_tokenize_str);
		bail_error(err);
		if (ret_err)
			goto bail;
	}
	if (cmd->cc_magic == cc_magic_ns_dyn_uri_tun_unset){
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_bool,
				"false",
				bn_tunnel);
		bail_error(err);
		if (ret_err)
			goto bail;

	}
bail:
	safe_free(bn_map_str);
	safe_free(bn_tokenize_str);
	safe_free(bn_uri_regex);
	safe_free(bn_tunnel);
	ts_free(&ret_msg);
	return err;
}

static int
cli_nvsd_origin_fetch_obj_thres_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    char *cmd_str = NULL;
    tstring *t_obj_size_thres_min = NULL;
    tstring *t_obj_size_thres_max = NULL;
    const char *namespace = NULL;
    uint32 t_obj_min_size = 0;
    uint32 t_obj_max_size = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);

    namespace = tstr_array_get_str_quick(name_parts, 4);
    bail_null(namespace);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_obj_size_thres_min, NULL,
		"/nkn/nvsd/origin_fetch/config/%s/obj-thresh-min",
		namespace);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		&t_obj_size_thres_max, NULL,
		"/nkn/nvsd/origin_fetch/config/%s/obj-thresh-max",
		namespace);
    bail_error(err);

    if ((t_obj_size_thres_min == NULL) ||(t_obj_size_thres_max == NULL))
    	goto bail;

    t_obj_min_size = atoi(ts_str(t_obj_size_thres_min));
    t_obj_max_size = atoi(ts_str(t_obj_size_thres_max));

    if((t_obj_max_size != 0) || (t_obj_min_size != 0)){
    	/* Build the command */
    	cmd_str = smprintf("namespace %s delivery protocol http origin-fetch "
    			"cache object-size range %d %d",
    			namespace, t_obj_min_size,t_obj_max_size );
    	bail_null(cmd_str);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/obj-thresh-min", namespace);
    	bail_error(err);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/obj-thresh-max", namespace);
    	bail_error(err);

    	/* Add to the revmap */
    	err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    	bail_error(err);
    }
    else
    {
    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/obj-thresh-min", namespace);
    	bail_error(err);

    	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/obj-thresh-max", namespace);
    	bail_error(err);

    }

bail:
	safe_free(cmd_str); /* Only in error case */
	ts_free(&t_obj_size_thres_min);
	ts_free(&t_obj_size_thres_max);

    return err;
}

int
cli_nvsd_ns_seek_uri_exec_cb(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_namespace = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	char *bn_uri_regex = NULL;
	char *bn_map_str = NULL;
	char *bn_seek_uri = NULL;

	UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);
	
	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	bn_seek_uri = smprintf("/nkn/nvsd/namespace/%s/client-request/seek-uri/enable",
			c_namespace);
	bail_null(bn_seek_uri);

	bn_uri_regex = smprintf("/nkn/nvsd/namespace/%s/client-request/seek-uri/regex",
			c_namespace);
	bail_null(bn_uri_regex);

	bn_map_str = smprintf("/nkn/nvsd/namespace/%s/client-request/seek-uri/map-string",
			c_namespace);
	bail_null(bn_map_str);

	if (cmd->cc_magic == cc_magic_ns_seek_uri_regex_add ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_regex,
				tstr_array_get_str_quick(params, 1),
				bn_uri_regex);
		bail_error(err);
		if (ret_err)
			goto bail;

	    if(!strcmp(tstr_array_get_str_quick(params, 2),"")) {
		    cli_printf(_("Map string cannot be a empty one\n"));
		    goto bail;
	    }

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				tstr_array_get_str_quick(params, 2),
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_bool,
				"true",
				bn_seek_uri);
		bail_error(err);
		if (ret_err)
			goto bail;

	}
	if (cmd->cc_magic == cc_magic_ns_seek_uri_all_delete ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_regex,
				"",
				bn_uri_regex);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				"",
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_bool,
				"false",
				bn_seek_uri);
		bail_error(err);
		if (ret_err)
			goto bail;

	}
	if (cmd->cc_magic == cc_magic_ns_seek_uri_str_delete ){

		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string,
				"",
				bn_map_str);
		bail_error(err);
		if (ret_err)
			goto bail;
	}

bail:
	safe_free(bn_map_str);
	safe_free(bn_uri_regex);
	ts_free(&ret_msg);
	return err;
}

int
cli_nvsd_namespace_http_origin_request_cache_reval_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;
	
	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation";
	cmd->cc_help_descr =        N_("Configure Cache revalidation options");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit";
	cmd->cc_help_descr =        N_("Allow revalidation of cached content");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_nvsd_ns_origin_req_cache_revalidate_config;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit use-date-header";
	cmd->cc_help_descr =        N_("Use the HTTP Date header for revalidation if Last-Modified header doesnt exist");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_ns_origin_req_cache_revalidate_config;
	cmd->cc_revmap_names =	    "/nkn/nvsd/origin_fetch/config/*/cache-revalidate";
        cmd->cc_revmap_callback =   cli_nvsd_namespace_generic_callback;
        cmd->cc_revmap_data =       (void*)cli_nvsd_ns_origin_req_cache_revalidation_revmap;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				"cache-revalidation deny";
	cmd->cc_help_descr =        N_("Disable revalidation of cached content");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_ns_origin_req_cache_revalidate_config;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	/*--------------------------------------------------------------------*/
	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method";
	cmd->cc_help_descr =	    N_("Configure HTTP method to use for a revalidation request");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method get";
	cmd->cc_help_descr =	    N_("Use GET method on a cache-revalidation request");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_ns_origin_req_cache_revalidate_config;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method head";
	cmd->cc_help_descr =	    N_("Use HEAD method on a cache-revalidation request");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_nvsd_ns_origin_req_cache_revalidate_config;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method get validate-header";
	cmd->cc_help_descr =	    N_("Configure HTTP response header to use to validate "
					"whether object was modified or not");
        cmd->cc_revmap_names =      "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/validate-header";
        cmd->cc_revmap_order =      cro_namespace;
        cmd->cc_revmap_suborder =   crso_origin_request;


	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method get validate-header *";
	cmd->cc_help_exp =	    N_("<header-name>");
	cmd->cc_help_exp_hint =	    N_("HTTP response header");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_nvsd_ns_origin_req_cache_revalidate_config;

	CLI_CMD_REGISTER;
#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method get validate-header *";
	cmd->cc_help_exp =	    N_("<header-name>");
	cmd->cc_help_exp_hint =	    N_("HTTP response header");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_nvsd_ns_origin_req_cache_revalidate_config;
	CLI_CMD_REGISTER;
#endif

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method head validate-header";
	cmd->cc_help_descr =	    N_("Configure HTTP response header to use to validate "
					"whether object was modified or not");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation permit method head validate-header *";
	cmd->cc_help_exp =	    N_("<header-name>");
	cmd->cc_help_exp_hint =	    N_("HTTP response header");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_nvsd_ns_origin_req_cache_revalidate_config;

	CLI_CMD_REGISTER;

	/* client-header related commands */
	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation client-headers";
	cmd->cc_help_descr =        N_("Set client-header related configuration for cache-revalidation");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation client-headers action";
	cmd->cc_help_descr =        N_("Setup actions for the client-headers in cache revalidation");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "namespace * delivery protocol http origin-request "
				    "cache-revalidation client-headers action inherit";
	cmd->cc_help_descr =        N_("Setup cache-revalidation to include client-headers");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name      =    "/nkn/nvsd/origin_fetch/config/$1$/cache-revalidate/client-header-inherit",
	cmd->cc_exec_type      =    bt_bool;
	cmd->cc_exec_value     =    "true";
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no namespace * delivery protocol http origin-request "
				    "cache-revalidation";
	cmd->cc_help_descr =        N_("Reset cache-revalidation related configuration");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no namespace * delivery protocol http origin-request "
				    "cache-revalidation client-headers";
	cmd->cc_help_descr =        N_("Reset client-header related configuration for cache-revalidation");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no namespace * delivery protocol http origin-request "
				    "cache-revalidation client-headers action";
	cmd->cc_help_descr =        N_("Reset actions for the client-headers in cache revalidation");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no namespace * delivery protocol http origin-request "
				    "cache-revalidation client-headers action inherit";
	cmd->cc_help_descr =        N_("Setup cache-revalidation to exclude client-headers");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name      =    "/nkn/nvsd/origin_fetch/config/$1$/cache-revalidate/client-header-inherit",
	cmd->cc_exec_type      =    bt_bool;
	cmd->cc_exec_value     =    "false";
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =      cro_namespace;
	cmd->cc_revmap_suborder =   crso_origin_request;
	CLI_CMD_REGISTER;

	/*--------------------------------------------------------------------*/
bail:
	return err;
}

static int
cli_nvsd_origin_fetch_agg_thres_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{

    int err = 0;
    char *cmd_str = NULL;
    tstring *t_agg_thres = NULL;
    tstring *t_cache_fill = NULL;
    const char *namespace = NULL;
    uint32 t_agg_threshold = 0;
    uint32 t_cache_fill_type = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    namespace = tstr_array_get_str_quick(name_parts, 4);
    bail_null(namespace);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
                &t_cache_fill, NULL,
                "/nkn/nvsd/origin_fetch/config/%s/cache-fill",
                namespace);
    bail_error(err);

    if(t_cache_fill == NULL)
    	goto bail;

    t_cache_fill_type = atoi(ts_str(t_cache_fill));

    if (t_cache_fill_type != 2)
    {
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/cache-fill/client-driven/aggressive_threshold", namespace);
        bail_error(err);
    	 goto bail;
    }
    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
                &t_agg_thres, NULL,
                "/nkn/nvsd/origin_fetch/config/%s/cache-fill/client-driven/aggressive_threshold",
                namespace);
    bail_error(err);

    if(t_agg_thres == NULL)
    	goto bail;

    t_agg_threshold = atoi(ts_str(t_agg_thres));

    cmd_str = smprintf("namespace %s delivery protocol http origin-fetch "
                   "cache-fill client-driven aggressive-threshold %d",
                   namespace, t_agg_threshold );
    bail_null(cmd_str);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/cache-fill/client-driven/aggressive_threshold", namespace);
    bail_error(err);

    /* Add to the revmap */
    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

bail:
        safe_free(cmd_str); /* Only in error case */
        ts_free(&t_agg_thres);

    return err;
}

int cli_nvsd_ns_origin_server_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	int dns_present = 0;
	int aws_present = 0;
	char *node_name = NULL;
	const char *c_value = NULL;
	const char *c_namespace = NULL;
	const char *c_origin_server = NULL;
	const char *c_port = NULL;
	const char *c_access_key_value = "";
	const char *c_secret_key_value = "";
	const char *c_dns_query_value = "";
	const char default_port[8] = "80";
	unsigned int cmd_length = 0;
	int port_present = 0;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	c_origin_server = tstr_array_get_str_quick(params, 1);
	bail_null(c_origin_server);

	err = tstr_array_length(cmd_line, &cmd_length);
	if (err) {
		goto bail;
	}

	if (cmd_length == 5) {
		port_present = 0;
	} else if (cmd_length == 6) {
		port_present = 1;
	} else {
		c_value =  tstr_array_get_str_quick(cmd_line, 5);
re_check:
		if (c_value) {
			if (!strcmp(c_value, "aws")) {
				aws_present = 2;
			} else if (!strcmp(c_value, "dns-query")) {
				dns_present = 1;
			} else {
				port_present = 1;
				c_value = tstr_array_get_str_quick(cmd_line, 6);
				goto re_check;
			}
		}
	}

	if (port_present) {
		c_port = tstr_array_get_str_quick(params, 2);
		bail_null(c_port);
	} else {
		c_port = default_port;
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/name", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string, c_origin_server, node_name);
	bail_error(err);
	if (ret_err != 0) {
	    cli_printf( _("Error:%s\n"), ts_str(ret_msg));
	    goto bail;
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/port", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_uint16, c_port, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str( ret_msg ));
		goto bail;
	}

	if (aws_present) {
		c_access_key_value = tstr_array_get_str_quick(params, 2 + port_present);
		bail_null(c_access_key_value);

		c_secret_key_value = tstr_array_get_str_quick(params, 3 + port_present);
		bail_null(c_secret_key_value);

                c_value =  tstr_array_get_str_quick(cmd_line, 10);
                if (c_value) {
                        if (!strcmp(c_value, "dns-query")) {
				dns_present = 1;
			}
		}
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/aws/access-key", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string, c_access_key_value, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/aws/secret-key", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string, c_secret_key_value, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	if (dns_present) {
		c_dns_query_value = tstr_array_get_str_quick(params, 2 + aws_present + port_present);
                bail_null(c_dns_query_value);
	}

        node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/dns-query", c_namespace);
        bail_null(node_name);
        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
                    bsso_modify, bt_string, c_dns_query_value, node_name);
        bail_error(err);
        if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
        }

bail:
	ts_free(&ret_msg);
	safe_free (node_name);

	return err;
}

int cli_nvsd_ns_origin_server_follow_hdr_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	char *node_name = NULL;
	const char *c_secret_key = NULL;
	const char *c_namespace = NULL;
	const char *c_hdr_name = NULL;
	const char *c_access_key_value = "";
	const char *c_secret_key_value = "";
	const char *c_dns_query_value = "";
	const char *c_use_client_ip = NULL;
	const char *c_value = NULL;
	unsigned int cmd_length = 0;
	int use_client_ip = 0;
	int aws_present = 0;
	int dns_present = 0;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	c_hdr_name = tstr_array_get_str_quick(params, 1);
	bail_null(c_hdr_name);

	err = tstr_array_length(cmd_line, &cmd_length);
	if (err) {
		goto bail;
	}

	if (cmd_length == 7) {
		use_client_ip = 0;
	} else if (cmd_length == 8) {
		use_client_ip = 1;
	} else {
		c_value =  tstr_array_get_str_quick(cmd_line, 7);
		if (c_value) {
			if (!strcmp(c_value, "aws")) {
				aws_present = 2;
				dns_present = 0;
			} else if (!strcmp(c_value, "dns-query")) {
				aws_present = 0;
				dns_present = 1;
			}
		}
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/header", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string, c_hdr_name, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	if (aws_present) {
		c_access_key_value = tstr_array_get_str_quick(params, 2);
		bail_null(c_access_key_value);

		c_secret_key_value = tstr_array_get_str_quick(params, 3);
		bail_null(c_secret_key_value);

		c_value = tstr_array_get_str_quick(cmd_line, 12);
		if (c_value) {
			    if (!strcmp(c_value, "dns-query")) {
				    dns_present = 1;
			    } else if (!strcmp(c_value, "use-client-ip")) {
				    use_client_ip = 1;
			    }
		}
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/access-key", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string, c_access_key_value, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/aws/secret-key", c_namespace);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		    bsso_modify, bt_string, c_secret_key_value, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	if (dns_present) {
		c_dns_query_value = tstr_array_get_str_quick(params, 2 + aws_present);
		bail_null(c_access_key_value);

		if (aws_present) {
			c_value = tstr_array_get_str_quick(cmd_line, 14);
		} else {
			c_value = tstr_array_get_str_quick(cmd_line, 9);
		}
		if (c_value) {
			if (!strcmp(c_value, "use-client-ip")) {
				use_client_ip = 1;
			}
		}
	}

        node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/dns-query", c_namespace);
        bail_null(node_name);
        err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
                    bsso_modify, bt_string, c_dns_query_value, node_name);
        bail_error(err);
        if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
        }

	node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip", c_namespace);
	bail_null(node_name);
	if (use_client_ip) {
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
					bsso_modify, bt_bool, "true", node_name);
	} else {
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
					bsso_modify, bt_bool, "false", node_name);
	}
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

bail:
	ts_free(&ret_msg);
	safe_free (node_name);

	return err;
}

int
cli_nvsd_namespace_pe_hdlr(
                void *data,
                const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params)
{
	int err = 0;
	const char *pe_srvr_name = NULL;
	const char *ns_name = NULL;
	node_name_t check_node = {0};
	tbool is_valid = false;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(cmd);


	pe_srvr_name = tstr_array_get_str_quick(params, 1);
	bail_null(pe_srvr_name);

	ns_name = tstr_array_get_str_quick(params, 0);
	bail_null(ns_name);

	snprintf(check_node, sizeof(check_node), "/nkn/nvsd/namespace/%s/policy/server/%s", ns_name, pe_srvr_name);

	err = cli_nvsd_pe_is_valid_item(check_node, NULL, &is_valid);
	bail_error(err);

	if(!is_valid)
		cli_printf_error("Invalid Policy Engine server");

	err = mdc_delete_binding(cli_mcc, NULL, NULL, check_node);
	bail_error(err);

bail:
	return err;
}
static int
cli_nvsd_ns_geo_service_revmap(void *data, const cli_command *cmd,
		const bn_binding_array *bindings, const char *name,
		const tstr_array *name_parts, const char *value,
		const bn_binding *binding,
		cli_revmap_reply *ret_reply)
{
	int err = 0;
    tstring *t_service = NULL;
    tstring *t_api = NULL;
    tstring *rev_cmd = NULL;
    const char *namespace;
    node_name_t node_name = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);


    namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(namespace);
    
    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/client-request/geo-service/service", namespace);

    err = bn_binding_array_get_value_tstr_by_name(bindings, node_name, NULL, &t_service);
    bail_error(err);

    if(t_service && ts_length(t_service) > 0) {
	    if(ts_equal_str(t_service, "maxmind", false)) {
		    ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request geo-service maxmind", namespace);
	    }else {
		    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/client-request/geo-service/api-url", namespace);
		    err = bn_binding_array_get_value_tstr_by_name(bindings, node_name, NULL, &t_api);
		    bail_error(err);
		    ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http client-request"
				    " geo-service quova api-url %s", namespace, ts_str(t_api));
	    }
    }

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/geo-service/api-url", namespace);
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/namespace/%s/client-request/geo-service/service", namespace);
    bail_error(err);

    if(t_service && ts_length(t_service) > 0) {
        err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
        bail_error(err);
    }

bail:
	ts_free(&t_service);
	ts_free(&t_api);
	ts_free(&rev_cmd);
    return err;
}

/*---------------------------------------------------------------------------*/
static int
cli_nvsd_ns_cache_index_header_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	const char *position = NULL;

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	position = tstr_array_get_str_quick(name_parts, 7);
	bail_null(position);

	err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http cache-index header include \"%s\"",
			namespace, value);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
			"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers/%d/header-name",
			namespace, atoi(position));
	bail_error(err);

	/* Consume nodes -
	 */
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);
bail:
	ts_free(&rev_cmd);
	return err;
}

int cli_nvsd_ns_cache_index_header_dupchk(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *header_name = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &header_name);
    bail_error(err);

    if(ts_equal_str((const tstring *)header_name, (const char *)callback_data, true))
    	{
			 cli_printf(_("Header-name %s already included. Blocking the duplicate entry\n"), ts_str(header_name));
			 header_dup = 1;
    	}

    bail:
	ts_free(&header_name);
	return(err);
}

int
cli_nvsd_ns_cache_index_header_add(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    uint32 code = 0;
    tbool dup_deleted = false;
    tstring *msg = NULL;
    const char *header_name = NULL;
    char *node_name = NULL;
    char *status_node_name = NULL;
    const char *ns_name = NULL;
    char *header_binding = NULL;
    uint32 num_headers =0;
    tstring *tstr_hdr_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    header_name = tstr_array_get_str_quick(params, 1);
    bail_null(header_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    ts_new_str(&tstr_hdr_name, header_name);

    ts_trans_lowercase(tstr_hdr_name);

    header_binding = smprintf("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers/*/header-name", ns_name);
    bail_null(header_binding);

    err = mdc_foreach_binding(cli_mcc, header_binding, NULL, cli_nvsd_ns_cache_index_header_dupchk,
	 	   (void*)header_name, &num_headers);
    bail_error(err);

	if(header_dup != 0)
		goto bail;

	node_name = smprintf ("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers", ns_name);
    bail_null(node_name);
   
    err = mdc_array_append_single(cli_mcc,
			node_name,
			"header-name",
			bt_string,
			ts_str(tstr_hdr_name),
			true,
			&dup_deleted,
			&code,
			&msg);
    bail_error(err);

    status_node_name = smprintf ("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-header", ns_name);
    bail_null(status_node_name);

	err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify, bt_bool,
			"true",
			status_node_name);
	bail_error(err);

 bail:
	header_dup = 0;
    ts_free(&tstr_hdr_name);
    bn_binding_array_free(&barr);

  return(err);
}

int
cli_nvsd_ns_cache_index_header_delete(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    int t_header_count = 0;
    const char *header_name = NULL;
    char *node_name = NULL;
    char *status_node_name = NULL;
    const char *ns_name = NULL;
    tstr_array *t_header_names = NULL;
    uint32 num_deleted_hdrs = 0;
    tstring *tstr_hdr_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    header_name = tstr_array_get_str_quick(params, 1);
    bail_null(header_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    ts_new_str(&tstr_hdr_name, header_name);

    ts_trans_lowercase(tstr_hdr_name);

    node_name = smprintf ("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers", ns_name);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &t_header_names,
            node_name, NULL);
    bail_error(err);

    err = mdc_array_delete_by_value_single(cli_mcc,
                node_name,
                true, "header-name", bt_string,
                ts_str(tstr_hdr_name), &num_deleted_hdrs, NULL, NULL);
    bail_error(err);

    t_header_count = tstr_array_length_quick(t_header_names);

    if ((t_header_count == 1) && (num_deleted_hdrs != 0)) {
        status_node_name = smprintf ("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-header", ns_name);
        bail_null(status_node_name);

    	err = mdc_set_binding(cli_mcc, NULL, NULL,
    			bsso_modify, bt_bool,
    			"false",
    			status_node_name);
    	bail_error(err);
    }
bail:

    bn_binding_array_free(&barr);
    ts_free(&tstr_hdr_name);
  return(err);
}


int
cli_nvsd_ns_cache_index_header_delete_all(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    char *node_name = NULL;
    char *status_node_name = NULL;
    const char *ns_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    node_name = smprintf ("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers", ns_name);
    bail_null(node_name);

    err = mdc_delete_binding_children(cli_mcc, NULL, NULL, node_name);
    bail_error(err);

    status_node_name = smprintf ("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-header", ns_name);
    bail_null(status_node_name);

   	err = mdc_set_binding(cli_mcc, NULL, NULL,
   			bsso_modify, bt_bool,
   			"false",
   			status_node_name);
   	bail_error(err);

bail:
  safe_free(node_name);
  safe_free(status_node_name);
  return(err);
}

int
cli_nvsd_origin_fetch_cache_index_config(void *data, const cli_command *cmd,
					 const tstr_array *cmd_line,
					 const tstr_array *params)
{
	const char *c_namespace = NULL;
	uint32_t err = 0;
	const char *t1;
	const char *cmd_line_11 = NULL;
	const char *cmd_line_13 = NULL;
	int offset_present = 0;
	int from_end = 0;
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	cmd_line_11 = tstr_array_get_str_quick(cmd_line, 11);

	cmd_line_13 = tstr_array_get_str_quick(cmd_line, 13);

	if (!strcmp(tstr_array_get_str_quick(params, 1),"")) {
		cli_printf(_("Size cannot be empty\n"));
		goto bail;
	}

	if(cmd_line_11 && !strcmp(cmd_line_11, "offset")) {
		if (!strcmp(tstr_array_get_str_quick(params, 2),"")) {
			cli_printf(_("Offset cannot be empty\n"));
			goto bail;
		}
		offset_present = 1;
	}

	if (offset_present) {
		if (cmd_line_13 && !strcmp(cmd_line_13,"from-end"))
			from_end = 1;
	} else {
		if (cmd_line_11 && !strcmp(cmd_line_11,"from-end"))
			from_end = 1;
	}

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_uint32, 0, tstr_array_get_str_quick(params, 1), NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-bytes",
			c_namespace);
	bail_error(err);

	if (offset_present) {
		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_uint32, 0, tstr_array_get_str_quick(params, 2), NULL,
				"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-offset",
				c_namespace);
		bail_error(err);
	} else {
		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_uint32, 0, "0", NULL,
				"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-offset",
				c_namespace);
		bail_error(err);
	}

	if (from_end) {
		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_bool, 0, "true", NULL,
				"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/from-end",
				c_namespace);
		bail_error(err);
	} else {
		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_bool, 0, "false", NULL,
				"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/from-end",
				c_namespace);
		bail_error(err);
	}

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_bool, 0, "true", NULL,
			"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object",
			c_namespace);
	bail_error(err);

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
	bail_error(err);

	if (err)
		goto bail;

bail:
	bn_request_msg_free(&req);
	return err;
}

int
cli_nvsd_origin_fetch_cache_index_offset_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	uint32 t_offset = 0, t_bytes = 0;
	const char *namespace = NULL;
	tstring *tstr_offset = NULL;
	tstring *tstr_bytes = NULL;
	tbool tbool_from_end = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 4);
	bail_null(namespace);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &tstr_offset,
			"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-offset", namespace);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &tstr_bytes,
			"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-bytes", namespace);
	bail_error(err);

	err = bn_binding_array_get_value_tbool_by_name_fmt(bindings, NULL, &tbool_from_end,
			"/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/from-end", namespace);
	bail_error(err);


	if (tstr_offset == NULL || tstr_bytes == NULL)
		goto bail;

	t_offset = atoi(ts_str(tstr_offset));

	t_bytes = atoi(ts_str(tstr_bytes));


	if ( t_bytes != 0 ) {
	        if ( t_offset ) {
			if ( tbool_from_end )
				err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http cache-index object include checksum size %d offset %d from-end",
					    namespace, t_bytes, t_offset);
			else
				err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http cache-index object include checksum size %d offset %d",
					    namespace, t_bytes, t_offset);
		} else {
			if ( tbool_from_end )
				err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http cache-index object include checksum size %d from-end",
					    namespace, t_bytes);
			else
				err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http cache-index object include checksum size %d",
					    namespace, t_bytes);
		}

		bail_error(err);

		if (rev_cmd == NULL)
			goto bail;

		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);

	}

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-bytes", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-offset", namespace);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/from-end", namespace);
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	ts_free(&tstr_offset);
	ts_free(&tstr_bytes);
	return err;
}
/*---------------------------------------------------------------------------*/
int
cli_nvsd_namespace_disk_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache";
	cmd->cc_help_descr = 		N_("Namespace specific media-cache properties");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk";
	cmd->cc_help_descr = 		N_("Disk cache properties");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier";
	cmd->cc_help_descr = 		N_("Disk cache properties per cache-tier");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata";
	cmd->cc_help_descr = 		N_("Disk cache properties for sata");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas";
	cmd->cc_help_descr = 		N_("Disk cache properties for sas");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd";
	cmd->cc_help_descr = 		N_("Disk cache properties for ssd");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata read-size";
	cmd->cc_help_descr = 		N_("Size of the bytes read against a full block of data");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas read-size";
	cmd->cc_help_descr = 		N_("Size of the bytes read against a full block of data");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd read-size";
	cmd->cc_help_descr = 		N_("Size of the bytes read against a full block of data");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata read-size *";
	cmd->cc_help_exp =		N_("<number>");
	cmd->cc_help_exp_hint =		N_("Size in KB [2048(default) or 256]");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sata/readsize";
	cmd->cc_exec_type =		bt_uint32;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas read-size *";
	cmd->cc_help_exp =		N_("<number>");
	cmd->cc_help_exp_hint =		N_("Size in KB [2048(default) or 256]");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sas/readsize";
	cmd->cc_exec_type =		bt_uint32;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder =	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =	    "namespace * media-cache disk cache-tier ssd read-size *";
	cmd->cc_help_exp =		N_("<number>");
	cmd->cc_help_exp_hint =		N_("Size in KB [256(default) or 32]");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/ssd/readsize";
	cmd->cc_exec_type =		bt_uint32;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata free-block-threshold";
	cmd->cc_help_descr = 		N_("Delete all the objects in a block if the block usage falls below  "
									"the specified percentage");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas free-block-threshold";
	cmd->cc_help_descr = 		N_("Delete all the objects in a block if the block usage falls below  "
									"the specified percentage");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd free-block-threshold";
	cmd->cc_help_descr = 		N_("Delete all the objects in a block if the block usage falls below  "
									"the specified percentage");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata free-block-threshold *";
	cmd->cc_help_exp =		N_("<number>");
	cmd->cc_help_exp_hint =		N_("Value in precentage [0 to 100]");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sata/free_block_thres";
	cmd->cc_exec_type =		bt_uint32;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas free-block-threshold *";
	cmd->cc_help_exp =		N_("<number>");
	cmd->cc_help_exp_hint =		N_("Value in precentage [0 to 100]");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sas/free_block_thres";
	cmd->cc_exec_type =		bt_uint32;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd free-block-threshold *";
	cmd->cc_help_exp =		N_("<number>");
	cmd->cc_help_exp_hint =		N_("Value in precentage [0 to 100]");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/ssd/free_block_thres";
	cmd->cc_exec_type =		bt_uint32;
	cmd->cc_exec_value =		"$2$";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata group-read";
	cmd->cc_help_descr = 		N_("Enable/Disable reading all the objects from a disk block");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas group-read";
	cmd->cc_help_descr = 		N_("Enable/Disable reading all the objects from a disk block");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd group-read";
	cmd->cc_help_descr = 		N_("Enable/Disable reading all the objects from a disk block");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata group-read enable";
	cmd->cc_help_descr = 		N_("Enable reading all the objects from a disk block");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =			"/nkn/nvsd/namespace/$1$/mediacache/sata/group_read_enable";
	cmd->cc_exec_type =			bt_bool;
	cmd->cc_exec_value =        "true";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas group-read enable";
	cmd->cc_help_descr = 		N_("Enable reading all the objects from a disk block");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sas/group_read_enable";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"true";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd group-read enable";
	cmd->cc_help_descr = 		N_("Enable reading all the objects from a disk block");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/ssd/group_read_enable";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"true";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sata group-read disable";
	cmd->cc_help_descr = 		N_("Disable reading all the objects from a disk block");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sata/group_read_enable";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"false";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier sas group-read disable";
	cmd->cc_help_descr = 		N_("Disable reading all the objects from a disk block");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/sas/group_read_enable";
	cmd->cc_exec_type =		bt_bool;
	cmd->cc_exec_value =		"false";
	cmd->cc_revmap_order = 		cro_namespace;
	cmd->cc_revmap_type =		crt_auto;
	cmd->cc_revmap_suborder = 	crso_media_cache;
 	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"namespace * media-cache disk cache-tier ssd group-read disable";
	cmd->cc_help_descr = 		N_("Disable reading all the objects from a disk block");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_reset;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/mediacache/ssd/group_read_enable";
	CLI_CMD_REGISTER;

	err = cli_revmap_ignore_bindings_va(1,
		"/nkn/nvsd/namespace/*/mediacache/**");
	bail_error(err);

bail:
	return err;
}
int
cli_compression_filetypes(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params)

{
    int err = 0;
    bn_binding_array *barr = NULL;
    uint32 code = 0;
    uint32 num_params = 0;
    const char *param = NULL;
    uint32 i = 0;
    uint16 j = 0;
    const char *type = NULL;
    bn_set_subop mode = 0;
    bn_request *req = NULL;
    int node_id = 0;
    unsigned int ret_val = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    type = tstr_array_get_str_quick(params, 0);
    bail_null(type);

    num_params = tstr_array_length_quick(params);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    switch(cmd->cc_magic)
    {
    case cid_compress_filetype_delete:
        mode = bsso_delete;
        break;
    case cid_compress_filetype_add:
        mode = bsso_create;
        break;
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

    for(i = 1; i < num_params; i++) {
        param = tstr_array_get_str_quick(params, i);
        bail_null(param);

        err = bn_set_request_msg_append_new_binding_fmt(req,
                mode, 0, bt_string, 0,
                param, &node_id,
                "/nkn/nvsd/origin_fetch/config/%s/object/compression/file_types/%s",
                type, param);
        bail_error(err);
    }

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_val, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}
int
cli_nvsd_ns_compression_file_types_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *file_type = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    if ((idx != 0) && ((idx % 3) == 0)) {
        cli_printf(_("\n"));
    }

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &file_type);
    bail_error(err);

    err = cli_printf(
                _("     %s"), ts_str(file_type));
    bail_error(err);

bail:
    ts_free(&file_type);
    return err;
}

int
cli_nvsd_ns_compression_show_config(void *data,
				  const cli_command 	*cmd,
				  const tstr_array	*cmd_line,
				  const tstr_array	*params)
{
	int err = 0;
	const char *c_namespace = NULL;
        node_name_t node_name = {0};
        tstring *checksum = NULL;
        uint32 num_files = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	c_namespace = tstr_array_get_str_quick(params, 0);
	bail_null(c_namespace);

	cli_printf( _("\n  Compression Settings:\n"));

	cli_printf_query( _("    Compression Enable: "
   "#/nkn/nvsd/origin_fetch/config/%s/object/compression/enable# \n"),
			  c_namespace);

	cli_printf_query( _("    Max-Object-Size: "
	"#/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_max# (B)\n"),
			  c_namespace);

	cli_printf_query( _("    Min-Object-Size: "
	"#/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_min# (B)\n"),
			  c_namespace);

	cli_printf_query( _("    Compression Algorithm: "
	"#/nkn/nvsd/origin_fetch/config/%s/object/compression/algorithm#\n"),
			  c_namespace);
	cli_printf( _("    File-Types:\n"));

	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/object/compression/file_types/*",
c_namespace);
	bail_null(node_name);

	err = mdc_foreach_binding(cli_mcc, node_name, NULL,
            cli_nvsd_ns_compression_file_types_show, NULL, &num_files);
        bail_error(err);

	if (num_files == 0) {
	    cli_printf(
		    _("     NONE\n"));
        } else {
	    cli_printf( _("    \n"));
	}

bail:
	return err;
}
/*
 * namespace * domain *
 * no namespace * domain *
 * namespace * domain regex *
 */
int
cli_config_domain_fqdn(void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    uint32_t i = 0;
    bn_request *req = NULL;
    const char *ns_name = NULL, *param = NULL;
    tstring *domain = NULL;
    bn_binding_array *fqdn_list = NULL;
    const bn_binding *binding = NULL;
    node_name_t domain_node = {0}, regex_node = {0};
    bn_set_subop  domain_op = bsso_none , regex_op = bsso_none;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    /* could be regex/fqdn */
    param = tstr_array_get_str_quick(params, 1);
    if (param == NULL) {
	/* check if "namespace * domain any" is issued */
	param = tstr_array_get_str_quick(cmd_line, 3);
	bail_null(param);

	if (0 != strcmp(param, "any")) {
	    err = 1;
	    bail_error(err);
	}
    }

    /* duping for lower cases */
    err = ts_new_str(&domain, param);
    bail_error(err);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    switch(cmd->cc_magic) {
	case ns_domain_fqdn_add:
	    err = ts_trans_lowercase(domain);
	    bail_error(err);

	    snprintf(domain_node, sizeof(domain_node),
		    "/nkn/nvsd/namespace/%s/fqdn-list/%s",ns_name, ts_str(domain));
	    snprintf(regex_node, sizeof(regex_node),
		    "/nkn/nvsd/namespace/%s/domain/regex",ns_name);

	    domain_op = bsso_modify;
	    regex_op = bsso_reset;

	    break;

	case ns_domain_fqdn_delete:
	    err = ts_trans_lowercase(domain);
	    bail_error(err);

	    snprintf(domain_node, sizeof(domain_node),
		    "/nkn/nvsd/namespace/%s/fqdn-list/%s",ns_name, ts_str(domain));
	    snprintf(regex_node, sizeof(regex_node),
		    "/nkn/nvsd/namespace/%s/domain/regex",ns_name);

	    domain_op = bsso_delete;
	    regex_op = bsso_reset;
	    break;

	case ns_domain_regex:
	    snprintf(domain_node, sizeof(domain_node),
		    "/nkn/nvsd/namespace/%s/fqdn-list",ns_name);

	    err = mdc_get_binding_children(cli_mcc, NULL, NULL, false,
		    &fqdn_list, false, false, domain_node);
	    bail_error(err);

	    //bn_binding_array_dump("DG-FQDN-LIST", fqdn_list, LOG_NOTICE);
	    for (i = 0; i < bn_binding_array_length_quick(fqdn_list); i++) {
		binding = bn_binding_array_get_quick(fqdn_list, i);
		if (binding == NULL) {
		    continue;
		}
		err = bn_set_request_msg_append_binding(req, bsso_delete, 0,
			binding, NULL);
		bail_error(err);
	    }
	    snprintf(regex_node, sizeof(regex_node),
		    "/nkn/nvsd/namespace/%s/domain/regex", ns_name);

	    regex_op = bsso_modify;
	    break;

	default:
	    goto bail;
	    break;
    }
    if (cmd->cc_magic != ns_domain_regex) {
	/* no need to set if is domain-regex */
	err = bn_set_request_msg_append_new_binding(req, domain_op, 0,
		domain_node, bt_string, 0, ts_str(domain), NULL);
	bail_error(err);
    }

    if (cmd->cc_magic != ns_domain_fqdn_delete) {
	err = bn_set_request_msg_append_new_binding(req, regex_op, 0,
		regex_node, bt_regex, 0, ts_str(domain), NULL);
	bail_error(err);
    }

    //bn_request_msg_dump(NULL, req, LOG_NOTICE, "DGAUTAM");

    err = mdc_send_mgmt_msg(cli_mcc,
                            req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    ts_free(&domain);
    bn_request_msg_free(&req);
    return err;
}
int 
cli_nvsd_namespace_filter_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

#define NS_CLIENT_REQ "namespace * delivery protocol http client-request "

	CLI_CMD_NEW;
	cmd->cc_words_str =		"no " NS_CLIENT_REQ "filter";
	cmd->cc_help_descr =		N_("Remove URL filtering for requests matching this namespace");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/filter-map";
	cmd->cc_exec_value =		"";
	cmd->cc_exec_type =		bt_string;
	cmd->cc_exec_name2 =		"/nkn/nvsd/namespace/$1$/client-request/filter-uri-size";
	cmd->cc_exec_value2 =		"0";
	cmd->cc_exec_type2 =		bt_uint32;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter";
	cmd->cc_help_descr = 		N_("Enable URL filtering for requests matching this namespace");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list";
	cmd->cc_help_descr = 		N_("URL map file is a black-list");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list";
	cmd->cc_help_descr = 		N_("URL map file is a white-list");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list *";
	cmd->cc_help_exp =          	N_("<url-map-name>");
	cmd->cc_help_exp_hint =     	N_("Associate URL map");
	cmd->cc_comp_type =         	cct_matching_names;
	cmd->cc_comp_pattern =      	"/nkn/nvsd/url-filter/config/id/*";
	cmd->cc_help_use_comp =         true;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action";
	cmd->cc_help_descr = 		N_("Action on hitting url-map");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action close-conn";
	cmd->cc_help_descr = 		N_("Close TCP connection toward origin");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_close;
	cmd->cc_exec_callback =         cli_nvsd_ns_filter_action;
	cmd->cc_exec_data =		(void *)"white";
	cmd->cc_revmap_callback =	cli_ns_filter_map_revmap;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/client-request/filter-map";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_suborder =	crso_url_filter;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action reset-conn";
	cmd->cc_help_descr = 		N_("Reset connection toward client");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_reset;
	cmd->cc_exec_callback =         cli_nvsd_ns_filter_action;
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 200-ok";
	cmd->cc_help_descr = 		N_("Send 200 OK response");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 200-ok *";
	cmd->cc_help_exp =          N_("<ascii-text>");
	cmd->cc_help_exp_hint =     N_("ASCII text to accompany the 200 OK ");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_200;
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 403-forbidden";
	cmd->cc_help_descr = 		N_("Response to indicate that requested object is forbidden");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_403;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 404-not-found";
	cmd->cc_help_descr = 		N_("Response to indicate that requested object is not found");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_404;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_exec_data =		(void *) "white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 301-redirect";
	cmd->cc_help_descr = 		N_("Send 301 response to redirect client");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 301-redirect *";
	cmd->cc_help_exp =          N_("<FQDN>");
	cmd->cc_help_exp_hint =     N_("New location for client");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_301;
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 301-redirect * *";
	cmd->cc_help_exp =          N_("<URI>");
	cmd->cc_help_exp_hint =     N_("New URI for client request");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_magic = 		cc_magic_ns_filter_301_uri;
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 302-redirect";
	cmd->cc_help_descr = 		N_("Send 302 response to redirect client");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 302-redirect *";
	cmd->cc_help_exp =          N_("<FQDN>");
	cmd->cc_help_exp_hint =     N_("New location for client");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_302;
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter white-list * action 302-redirect * *";
	cmd->cc_help_exp =          N_("<URI>");
	cmd->cc_help_exp_hint =     N_("New URI for client request");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_302_uri;
	cmd->cc_exec_data =		(void *)"white";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list *";
	cmd->cc_help_exp =          	N_("<url-map-name>");
	cmd->cc_help_exp_hint =     	N_("Associate URL map");
	cmd->cc_comp_type =         	cct_matching_names;
	cmd->cc_comp_pattern =      	"/nkn/nvsd/url-filter/config/id/*";
	cmd->cc_help_use_comp =         true;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action";
	cmd->cc_help_descr = 		N_("Action on hitting url-map");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action reset-conn";
	cmd->cc_help_descr = 		N_("Reset connection toward client");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_reset;
	cmd->cc_exec_callback =         cli_nvsd_ns_filter_action;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action close-conn";
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_help_descr = 		N_("Close TCP connection toward origin");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_magic = 		cc_magic_ns_filter_close;
	cmd->cc_exec_callback =         cli_nvsd_ns_filter_action;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 200-ok";
	cmd->cc_help_descr = 		N_("Send 200 OK response");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 200-ok *";
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_help_exp =          N_("<ascii-text>");
	cmd->cc_help_exp_hint =     N_("ASCII text to accompany the 200 OK ");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_200;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 403-forbidden";
	cmd->cc_help_descr = 		N_("Response to indicate that requested object is forbidden");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_403;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 404-not-found";
	cmd->cc_help_descr = 		N_("Response to indicate that requested object is not found");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_404;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_data =		(void *) "black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 301-redirect";
	cmd->cc_help_descr = 		N_("Send 301 response to redirect client");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 301-redirect *";
	cmd->cc_help_exp =          N_("<FQDN>");
	cmd->cc_help_exp_hint =     N_("New location for client");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_301;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 301-redirect * *";
	cmd->cc_help_exp =          N_("<URI>");
	cmd->cc_help_exp_hint =     N_("New URI for client request");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_301_uri;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 302-redirect";
	cmd->cc_help_descr = 		N_("Send 302 response to redirect client");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 302-redirect *";
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_help_exp =          N_("<FQDN>");
	cmd->cc_help_exp_hint =     N_("New location for client");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_302;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter black-list * action 302-redirect * *";
	cmd->cc_help_exp =          N_("<URI>");
	cmd->cc_help_exp_hint =     N_("New URI for client request");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_action;
	cmd->cc_magic = 		cc_magic_ns_filter_302_uri;
	cmd->cc_exec_data =		(void *)"black";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size";
	cmd->cc_help_descr = 		N_("Set the max allowed uri size");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no " NS_CLIENT_REQ "filter max-uri-size";
	cmd->cc_help_descr = 		N_("Remove the uri size filter");
	cmd->cc_flags =			ccf_terminal;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =	cxo_set;
	cmd->cc_exec_name =		"/nkn/nvsd/namespace/$1$/client-request/filter-uri-size";
	cmd->cc_exec_value =		"0";
	cmd->cc_exec_type =		bt_uint32;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size *";
	cmd->cc_help_exp =		"<1-32768>";
	cmd->cc_help_exp_hint =		N_("block uri based on the size "
					   "configured");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action";
	cmd->cc_help_descr = 		N_("Action on hitting url-map");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action reset-conn";
	cmd->cc_help_descr = 		N_("Reset connection toward client");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_reset;
	cmd->cc_exec_callback =         cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_revmap_callback =	cli_ns_filter_map_revmap;
	cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/client-request/filter-uri-size";
	cmd->cc_revmap_order =		cro_namespace;
	cmd->cc_revmap_suborder =	crso_url_filter;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action close-conn";
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_help_descr = 		N_("Close TCP connection toward origin");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_magic = 		cc_magic_ns_filter_close;
	cmd->cc_exec_callback =         cli_nvsd_ns_filter_uri_size_action;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 200-ok";
	cmd->cc_help_descr = 		N_("Send 200 OK response");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 200-ok *";
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_help_exp =          N_("<ascii-text>");
	cmd->cc_help_exp_hint =     N_("ASCII text to accompany the 200 OK ");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_magic = 		cc_magic_ns_filter_200;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 403-forbidden";
	cmd->cc_help_descr = 		N_("Response to indicate that requested object is forbidden");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_magic = 		cc_magic_ns_filter_403;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 404-not-found";
	cmd->cc_help_descr = 		N_("Response to indicate that requested object is not found");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_magic = 		cc_magic_ns_filter_404;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 301-redirect";
	cmd->cc_help_descr = 		N_("Send 301 response to redirect client");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 301-redirect *";
	cmd->cc_help_exp =		N_("<FQDN>");
	cmd->cc_help_exp_hint =		N_("New location for client");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_301;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 301-redirect * *";
	cmd->cc_help_exp =		N_("<URI>");
	cmd->cc_help_exp_hint =		N_("New URI for client request");
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_magic = 		cc_magic_ns_filter_301_uri;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 302-redirect";
	cmd->cc_help_descr = 		N_("Send 302 response to redirect client");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 302-redirect *";
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_help_exp =		N_("<FQDN>");
	cmd->cc_help_exp_hint =		N_("New location for client");
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_magic = 		cc_magic_ns_filter_302;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		NS_CLIENT_REQ "filter max-uri-size * action 302-redirect * *";
	cmd->cc_help_exp =		N_("<URI>");
	cmd->cc_help_exp_hint =		N_("New URI for client request");
	cmd->cc_flags =             	ccf_terminal; 
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_callback =		cli_nvsd_ns_filter_uri_size_action;
	cmd->cc_magic = 		cc_magic_ns_filter_302_uri;
	CLI_CMD_REGISTER;
bail:
	return err;
}

int
cli_nvsd_ns_filter_uri_size_action(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_request *req = NULL;
    const char *ns_name = NULL, *msg_200 = NULL, *filter_uri_size = NULL;
    const char *filter_action = NULL, *filter_data = NULL, *uri_3xx = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    filter_uri_size = tstr_array_get_str_quick(params, 1);
    bail_null(filter_uri_size);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_uint32, 0, filter_uri_size, NULL,
	    "/nkn/nvsd/namespace/%s/client-request/filter-uri-size", ns_name);
    bail_error(err);

    switch(cmd->cc_magic) {
	case cc_magic_ns_filter_close:
	    filter_action = "close";
	    break;
	case cc_magic_ns_filter_reset:
	    filter_action = "reset";
	    break;
	case cc_magic_ns_filter_200:
	    filter_action = "200";
	    msg_200 = tstr_array_get_str_quick(params, 2);
	    bail_null(msg_200);
	    break;
	case cc_magic_ns_filter_301:
	    filter_action = "301";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    break;
	case cc_magic_ns_filter_301_uri:
	    filter_action = "301";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    uri_3xx = tstr_array_get_str_quick(params, 3);
	    bail_null(uri_3xx);
	    break;
	case cc_magic_ns_filter_302:
	    filter_action = "302";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    break;
	case cc_magic_ns_filter_302_uri:
	    filter_action = "302";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    uri_3xx = tstr_array_get_str_quick(params, 3);
	    bail_null(uri_3xx);
	    break;
	case cc_magic_ns_filter_403:
	    filter_action = "403";
	    break;
	case cc_magic_ns_filter_404:
	    filter_action = "404";
	    break;
	default:
	    filter_action = "";
	    break;
    }

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, filter_action, NULL,
	    "/nkn/nvsd/namespace/%s/client-request/filter-uri-size-action",
	    ns_name);
    bail_error(err);

    if (msg_200) {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, msg_200, NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg",
	       	ns_name);
	bail_error(err);
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri",
	       	ns_name);
	bail_error(err);
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn",
	       	ns_name);
	bail_error(err);
    }
    if (uri_3xx) {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, uri_3xx, NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri",
	       	ns_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg",
	       	ns_name);
	bail_error(err);
    } else {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri",
	       	ns_name);
	bail_error(err);
    }
    if (filter_data) {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg",
	       	ns_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, filter_data, NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn",
	       	ns_name);
	bail_error(err);
    }

    switch(cmd->cc_magic) {
	case cc_magic_ns_filter_close:
	case cc_magic_ns_filter_reset:
	case cc_magic_ns_filter_403:
	case cc_magic_ns_filter_404:
	    /* reset all non-required data */
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		    bt_string, 0, "", NULL,
		    "/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg",
		    ns_name);
	    bail_error(err);
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		    bt_string, 0, "", NULL,
		    "/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri",
		    ns_name);
	    bail_error(err);
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		    bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn",
		    ns_name);
	    bail_error(err);
	    break;
    }
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}


int
cli_nvsd_ns_filter_action(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_request *req = NULL;
    const char *filter_type = data;
    const char *filter_action = NULL, *filter_data = NULL, *uri_3xx = NULL;
    const char *ns_name = NULL, *msg_200 = NULL, *filter_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    filter_name = tstr_array_get_str_quick(params, 1);
    bail_null(filter_name);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, filter_type, NULL,
	    "/nkn/nvsd/namespace/%s/client-request/filter-type", ns_name);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, filter_name, NULL,
	    "/nkn/nvsd/namespace/%s/client-request/filter-map", ns_name);
    bail_error(err);

    switch(cmd->cc_magic) {
	case cc_magic_ns_filter_close:
	    filter_action = "close";
	    break;
	case cc_magic_ns_filter_reset:
	    filter_action = "reset";
	    break;
	case cc_magic_ns_filter_200:
	    filter_action = "200";
	    msg_200 = tstr_array_get_str_quick(params, 2);
	    bail_null(msg_200);
	    break;
	case cc_magic_ns_filter_301:
	    filter_action = "301";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    break;
	case cc_magic_ns_filter_301_uri:
	    filter_action = "301";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    uri_3xx = tstr_array_get_str_quick(params, 3);
	    bail_null(uri_3xx);
	    break;
	case cc_magic_ns_filter_302:
	    filter_action = "302";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    break;
	case cc_magic_ns_filter_302_uri:
	    filter_action = "302";
	    filter_data = tstr_array_get_str_quick(params, 2);
	    bail_null(filter_data);
	    uri_3xx = tstr_array_get_str_quick(params, 3);
	    bail_null(uri_3xx);
	    break;
	case cc_magic_ns_filter_403:
	    filter_action = "403";
	    break;
	case cc_magic_ns_filter_404:
	    filter_action = "404";
	    break;
	default:
	    filter_action = "";
	    break;
    }
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, filter_action, NULL,
	    "/nkn/nvsd/namespace/%s/client-request/filter-action", ns_name);
    bail_error(err);


    if (msg_200) {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, msg_200, NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/200-msg", ns_name);
	bail_error(err);
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri", ns_name);
	bail_error(err);
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn", ns_name);
	bail_error(err);
    }
    if (uri_3xx) {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, uri_3xx, NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri", ns_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/200-msg", ns_name);
	bail_error(err);
    } else {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri", ns_name);
	bail_error(err);
    }
    if (filter_data) {
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_string, 0, "", NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/200-msg", ns_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_string, 0, filter_data, NULL,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn", ns_name);
	bail_error(err);
    }

    switch(cmd->cc_magic) {
	case cc_magic_ns_filter_close:
	case cc_magic_ns_filter_reset:
	case cc_magic_ns_filter_403:
	case cc_magic_ns_filter_404:
	    /* reset all non-required data */
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		    bt_string, 0, "", NULL,
		    "/nkn/nvsd/namespace/%s/client-request/uf/200-msg", ns_name);
	    bail_error(err);
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		    bt_string, 0, "", NULL,
		    "/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri", ns_name);
	    bail_error(err);
	    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		    bt_string, 0, "", NULL,
		    "/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn", ns_name);
	    bail_error(err);
	    break;
    }
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}

static int
cli_ns_filter_map_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err =0 ;
    const char *ns = NULL, *type = NULL, *action = NULL;
    tstring *t_filter_map = NULL, *rev_cmd = NULL, *t_filter_type = NULL,
	    *t_filter_action = NULL, *t_200_msg = NULL, *t_3xx_uri = NULL,
	    *t_3xx_fqdn = NULL;
    uint32_t filter_uri_size;

    /* only interested in following nodes */
    if (bn_binding_name_parts_pattern_match_va(name_parts,5, 1, "filter-map" )) {

	ns = tstr_array_get_str_quick(name_parts, 3 );
	bail_null(ns);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_filter_map,
		"/nkn/nvsd/namespace/%s/client-request/filter-map", ns);
	bail_error_null(err, t_filter_map);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_filter_type,
		"/nkn/nvsd/namespace/%s/client-request/filter-type", ns);
	bail_error_null(err, t_filter_type);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_filter_action,
		"/nkn/nvsd/namespace/%s/client-request/filter-action", ns);
	bail_error_null(err, t_filter_type);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_200_msg,
		"/nkn/nvsd/namespace/%s/client-request/uf/200-msg", ns);
	bail_error_null(err, t_200_msg);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_3xx_fqdn,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn", ns);
	bail_error_null(err, t_3xx_fqdn);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_3xx_uri,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri", ns);
	bail_error_null(err, t_3xx_uri);

	if (ts_equal_str(t_filter_map, "", true)) {
	    err = ts_new_sprintf(&rev_cmd, "no namespace %s delivery protocol http client-request filter",
		    ns);
	    bail_error(err);

	    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
	    bail_error(err);
	} else {
	    if (ts_equal_str(t_filter_type, "black", false)) {
		type = "black-list";
	    } else {
		type = "white-list";
	    }

	    if (ts_equal_str(t_filter_action, "200", false)) {
		action = "200-ok";
	    } else if (ts_equal_str(t_filter_action, "301", false)) {
		action = "301-redirect";
	    } else if (ts_equal_str(t_filter_action, "302", false)) {
		action = "302-redirect";
	    } else if (ts_equal_str(t_filter_action, "403", false)) {
		action = "403-forbidden";
	    } else if (ts_equal_str(t_filter_action, "404", false)) {
		action = "404-not-found";
	    } else if (ts_equal_str(t_filter_action, "close", false)) {
		action = "close-conn";
	    } else if (ts_equal_str(t_filter_action, "reset", false)) {
		action = "reset-conn";
	    }

	    err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http"
		    " client-request filter %s %s action %s ", ns, type,
		    value,action);
	    bail_error(err);

	    if (0 == ts_equal_str(t_200_msg, "", false)) {
		ts_append_str(rev_cmd,ts_str(t_200_msg) );
	    }
	    /* else this is non-200 response */
	    if (0 == ts_equal_str(t_3xx_fqdn, "", false)) {
		ts_append_str(rev_cmd,ts_str(t_3xx_fqdn) );
		if (0 == ts_equal_str(t_3xx_uri, "", false)) {
		    ts_append_sprintf(rev_cmd," %s",ts_str(t_3xx_uri) );
		} else {
		    ts_append_str(rev_cmd," \"\"" );
		}
	    }
	    /* else this is non-3xx response */
	    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
	    bail_error(err);
	}
	/* consume all nodes */

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/filter-map", ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/filter-action", ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/filter-type", ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/uf/200-msg", ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-uri", ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/uf/3xx-fqdn", ns);
	bail_error(err);
    }

    /* only interested in following nodes */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 1,
						"filter-uri-size" )) {

	ns = tstr_array_get_str_quick(name_parts, 3 );
	bail_null(ns);

	err = bn_binding_get_uint32(binding, ba_value, NULL, &filter_uri_size);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_filter_action,
		"/nkn/nvsd/namespace/%s/client-request/filter-uri-size-action",
	       	ns);
	bail_error_null(err, t_filter_action);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_200_msg,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg",
		ns);
	bail_error_null(err, t_200_msg);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_3xx_fqdn,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn",
		ns);
	bail_error_null(err, t_3xx_fqdn);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &t_3xx_uri,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri",
		ns);
	bail_error_null(err, t_3xx_uri);

	if (filter_uri_size != 0) {
	    if (ts_equal_str(t_filter_type, "black", false)) {
		type = "black-list";
	    } else {
		type = "white-list";
	    }

	    if (ts_equal_str(t_filter_action, "200", false)) {
		action = "200-ok";
	    } else if (ts_equal_str(t_filter_action, "301", false)) {
		action = "301-redirect";
	    } else if (ts_equal_str(t_filter_action, "302", false)) {
		action = "302-redirect";
	    } else if (ts_equal_str(t_filter_action, "403", false)) {
		action = "403-forbidden";
	    } else if (ts_equal_str(t_filter_action, "404", false)) {
		action = "404-not-found";
	    } else if (ts_equal_str(t_filter_action, "close", false)) {
		action = "close-conn";
	    } else if (ts_equal_str(t_filter_action, "reset", false)) {
		action = "reset-conn";
	    }

	    err = ts_new_sprintf(&rev_cmd, "namespace %s delivery protocol http"
		    " client-request filter max-uri-size %s action %s ", ns,
		    value, action);
	    bail_error(err);

	    if (0 == ts_equal_str(t_200_msg, "", false)) {
		ts_append_str(rev_cmd,ts_str(t_200_msg) );
	    }
	    /* else this is non-200 response */
	    if (0 == ts_equal_str(t_3xx_fqdn, "", false)) {
		ts_append_str(rev_cmd,ts_str(t_3xx_fqdn) );
		if (0 == ts_equal_str(t_3xx_uri, "", false)) {
		    ts_append_sprintf(rev_cmd," %s",ts_str(t_3xx_uri) );
		} else {
		    ts_append_str(rev_cmd," \"\"" );
		}
	    }
	    /* else this is non-3xx response */
	    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
	    bail_error(err);
	}
	/* consume all nodes */

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/filter-uri-size", ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/filter-uri-size-action",
	       	ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/200-msg",
		ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-uri",
		ns);
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
		"/nkn/nvsd/namespace/%s/client-request/uf-uri-size/3xx-fqdn",
		ns);
	bail_error(err);
    }
bail:
    return err;

}
/* End of cli_nvsd_namespace_cmds.c */

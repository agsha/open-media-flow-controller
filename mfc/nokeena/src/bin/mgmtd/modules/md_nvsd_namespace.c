/*
 *
 * Filename:  md_nvsd_namespace.c
 * Date:      2008/12/17
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <openssl/md5.h>
#include "tcrypt.h"
#include <ctype.h>
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "mdb_dbbe.h"
#include "nkn_defs.h"
#include "tpaths.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "regex.h"
#include "nkn_mgmt_defs.h"
#include "nkn_cntr_api.h"
#include "nkn_common_config.h"
#include "nvsd_mgmt_pe.h"
#include "nvsd_mgmt_namespace.h"
#include "jmd_common.h"

#define	UUID_SALT	"NK"
#define MAX_NS_WITH_MATCH_REGX   32
#define MAX_NAMESPACE_WITH_SMAPS 64


extern unsigned int jnpr_log_level;
#if 1
/* commenting it out as of now,
might be needing this sometime */
#define nm "/nkn/nvsd/namespace/"
const bn_str_value md_nvsd_namespace_probe_initial_values[] = {
{nm "mfc_probe",bt_string,"mfc_probe"},
{nm "mfc_probe/cache_inherit",bt_string,""},
{nm "mfc_probe/cluster-hash",bt_uint32,"2"},
{nm "mfc_probe/delivery/proto/http",bt_bool,"true"},
{nm "mfc_probe/delivery/proto/rtsp",bt_bool,"false"},
{nm "mfc_probe/domain/host",bt_string,"127.0.0.2"},
{nm "mfc_probe/fqdn-list/127.0.0.2",bt_string,"127.0.0.2"},
{nm "mfc_probe/domain/regex",bt_regex,""},
{nm "mfc_probe/ns-precedence",bt_uint32,"10"},
{nm "mfc_probe/ip_tproxy",bt_ipv4addr,"0.0.0.0"},
{nm "mfc_probe/match/header/name",bt_string,""},
{nm "mfc_probe/match/header/regex",bt_regex,""},
{nm "mfc_probe/match/header/value",bt_string,""},
{nm "mfc_probe/match/precedence",bt_uint32,"1"},
{nm "mfc_probe/match/query-string/name",bt_string,""},
{nm "mfc_probe/match/query-string/regex",bt_regex,""},
{nm "mfc_probe/match/query-string/value",bt_string,""},
{nm "mfc_probe/match/type",bt_uint8,"1"},
{nm "mfc_probe/match/uri/regex",bt_regex,""},
{nm "mfc_probe/match/uri/uri_name",bt_uri,"/mfc_probe"},
{nm "mfc_probe/match/virtual-host/ip",bt_ipv4addr,"255.255.255.255"},
{nm "mfc_probe/match/virtual-host/port",bt_uint16,"80"},
{nm "mfc_probe/origin-request/connect/retry-delay",bt_uint32,"100"},
{nm "mfc_probe/origin-request/connect/timeout",bt_uint32,"100"},
{nm "mfc_probe/origin-request/read/interval-timeout",bt_uint32,"100"},
{nm "mfc_probe/origin-request/read/retry-delay",bt_uint32,"100"},
{nm "mfc_probe/origin-request/x-forwarded-for/enable",bt_bool,"true"},
{nm "mfc_probe/origin-request/include-orig-ip-port",bt_bool,"false"},
{nm "mfc_probe/origin-server/http/absolute-url",bt_bool,"false"},
{nm "mfc_probe/origin-server/http/follow/dest-ip",bt_bool,"false"},
{nm "mfc_probe/origin-server/http/follow/header",bt_string,""},
{nm "mfc_probe/origin-server/http/follow/use-client-ip",bt_bool,"false"},
{nm "mfc_probe/origin-server/http/follow/use-client-ip/header",bt_string,""},
{nm "mfc_probe/origin-server/http/host/name",bt_string,"127.0.0.1"},
{nm "mfc_probe/origin-server/http/host/port",bt_uint16,"8081"},
{nm "mfc_probe/origin-server/nfs/host",bt_string,""},
{nm "mfc_probe/origin-server/nfs/local-cache",bt_bool,"true"},
{nm "mfc_probe/origin-server/nfs/port",bt_uint16,"2049"},
{nm "mfc_probe/origin-server/nfs/server-map",bt_string,""},
{nm "mfc_probe/origin-server/rtsp/follow/dest-ip",bt_bool,"false"},
{nm "mfc_probe/origin-server/rtsp/follow/use-client-ip",bt_bool,"false"},
{nm "mfc_probe/origin-server/rtsp/host/name",bt_string,""},
{nm "mfc_probe/origin-server/rtsp/host/port",bt_uint16,"554"},
{nm "mfc_probe/origin-server/rtsp/host/transport",bt_string,""},
{nm "mfc_probe/origin-server/type",bt_uint8,"1"},
{nm "mfc_probe/policy-map",bt_string,""},
{nm "mfc_probe/prestage/user/mfc_probe_ftpuser",bt_string,"mfc_probe_ftpuser"},
{nm "mfc_probe/prestage/user/mfc_probe_ftpuser/password",bt_string,""},
{nm "mfc_probe/proxy-mode",bt_string,"reverse"},
{nm "mfc_probe/status/active",bt_bool,"true"},
{nm "mfc_probe/uid",bt_string,"/mfc_probe:463faaa9"},
{nm "mfc_probe/virtual_player",bt_string,""},
{"/nkn/nvsd/origin_fetch/config/mfc_probe/cache-age/default",bt_uint32,"120"},
};
#endif
md_commit_forward_action_args md_namespace_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; Unable to reach MFD Subsystem"),
    mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};

md_commit_forward_action_args md_gmgmthd_fwd_args =
{
    GCL_CLIENT_GMGMTHD, true, N_("Request failed; MFD subsystem 'gmgmthd' did not respond"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};


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

typedef enum {
    dt_match_header_name = 0,
    dt_match_header_regex,
} dependency_type_t;

typedef enum {
    domain_host = 0,
    domain_regex,
    domain_none,
} domain_type_t;

const char *match_types[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};


const char *match_type_str [] = {
					"NONE",
					"URI",
					"URI",
					"HEADER",
					"HEADER",
					"Q-STRING",
					"Q-STRING",
					"V-HOST",
					"RTSP-URI",
					"RTSP-URI",
					"RTSP-VHOST"
				};

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
	osvr_http_node_map = 10,
} origin_server_type_t;

const char *osvr_types[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

typedef struct {
	origin_server_type_t	type;
	struct {
		const char *host;
		uint16 port;
	} http;
	const char *server_map;
	struct {
		const char *host;
		uint16 port;
		tbool local_cache;
	} nfs;

	struct {
		tbool dest_ip;
		tbool client_ip;
		const char *dip_hdr_name;
		const char *cip_hdr_name;

	} follow;

	const bn_attrib_array *new_attribs;
	const bn_attrib_array *old_attribs;
	
} origin_server_data_t;


typedef struct {

	match_type_t	match_type;
	const char *uri_name;
	const char *uri_regex;
	struct{
		const char *q_n;
		const char *q_v;
	} query_name;
	const char *q_regex;
	struct {
		const char *h_n;
		const char *h_v;
	} header_name;
	const char *header_regex;
	struct {
		const char *vhost_ip;
		uint16	port;
	} vhost;
} match_type_data_t;


static md_upgrade_rule_array *md_nvsd_ns_ug_rules = NULL;
static md_upgrade_rule_array *md_nvsd_service_ug_rules = NULL;
/*--------------------------------------------------------------------------*/
int
md_check_if_smap_attached(md_commit *commit, const mdb_db *old_db,
		const mdb_db  *new_db,  const mdb_db_change *change,
		const char *smap_name, int *attached);
int
md_check_if_node_attached(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db,  const mdb_db_change *change,
		const char *check_name, const char *node_pattern, int *error);
int
md_check_if_node_exists(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db, const char *check_name, 
		const char *node_root, int *exists);
int
md_check_ns_smap( md_commit *commit,
	const mdb_db *new_db,
	mdb_db_change_array *change_list);
static int
md_encrypt_string(md_commit *commit, const mdb_db *db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg);
int md_lib_send_action_from_module(md_commit *commit, const char *action_name);
int
md_nvsd_namespace_init(
        const lc_dso_info * info,
        void *data );

int
md_namespace_init(
        const lc_dso_info * info,
        void *data );

static long
hextol_len(const char *str, int len);

static int
md_namespace_set_node_map_proxy_mode(md_commit *commit, mdb_db *inout_new_db,
                                     const char *t_namespace);

static int
md_namespace_node_map_enabled(md_commit *commit, mdb_db *inout_new_db,
                              const char *t_namespace, tbool *enabled_p);

static int
md_namespace_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_namespace_validate_lost_uids (
				md_commit *commit ,
				tstring *t_cache_inherit_name);

static int
md_namespace_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_nvsd_ns_has_origin_configured( md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg,
		tbool *origin_configed);

static int
md_nvsd_namespace_validate_name(const char *name,
			tstring **ret_msg);


static int delete_temp_files(const char *ns_uid);

static int
md_nvsd_ns_acclog_bind_check_func(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs, void
		*arg);

static int
md_namespace_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg);

int get_next_ns_precedence(md_commit *commit, mdb_db *inout_new_db,
	uint32_t *next_value);
int md_check_ns_precedence(md_commit *commit, const mdb_db *db, int *unique);


static int
md_nvsd_ns_upgrade_domain_host(
		mdb_db	*inout_new_db,
		const char *bn_name);

static int
md_nvsd_ns_upgrade_domain_regex(
		mdb_db	*inout_new_db,
		const char *bn_name);

static int
md_nvsd_ns_upgrade_delivery_proto(
		mdb_db	*inout_new_db,
		const char *namespace,
		const char *uri_prefix,
		const char *proto);

static int md_nvsd_namespace_http_match_criteria_set(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		match_type_data_t *mt);

static int md_nvsd_namespace_current_origin_server_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		origin_server_type_t *ret_type);

static int md_nvsd_namespace_origin_server_set(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		origin_server_data_t *ot);

static int md_nvsd_namespace_proxy_mode_set(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		const char *mode);

static int
md_nvsd_namespace_ug_nfs_6_to_7(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg);


/* Moved from md_nvsd_uri.c */
int
md_nvsd_uri_nfs_host_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);
int md_nvsd_uri_name_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);

int
md_nvsd_ns_http_host_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);

int
md_nvsd_ip_tproxy_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);

int
md_nvsd_namespace_set_lst_node(md_commit *commit, const mdb_db *old_db,
					mdb_db *inout_new_db, const mdb_db_change *change,
					const char *lst_nd_str, int item_idx);
static int
md_nvsd_namespace_validate_on_active(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        const char *namespace,
	tbool *invalid);

static tbool
md_nvsd_namespace_check_if_default(const bn_attrib_array *new_attribs,
		bn_type bt_type,
		const char *default_value);

static int
validate_on_type(md_commit *commit,
		 const mdb_db *mdb,
                 const char *namespace,
                 const char *current_value,
		 tstring **return_value,
		 match_type_t incoming_type,
                 match_type_t match_type,
		 tbool *invalid);

static int md_nvsd_namespace_is_active(const char *namespace,
		const mdb_db *db, tbool *ret_active);

static int
md_nvsd_ns_host_header_inherit_set(md_commit *commit,
		mdb_db *inout_new_db,
		const char *namespace,
		tbool  permit);

static int md_nvsd_namespace_ug_match_criteria_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		match_type_t	*ret_match_type);


static int md_nvsd_namespace_ug_match_11_to_12(
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		match_type_t mt,
		void *arg);

static int md_nvsd_namespace_ug_11_to_12_delete_old_nodes(
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		void *arg);

static int md_nvsd_namespace_rtsp_current_match_criteria_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		match_type_t	*ret_match_type);

static int
md_nvsd_namespace_add_initial(
               md_commit *commit,
               mdb_db    *db,
               void *arg);

static int
md_upgrade_servermap(md_commit *commit, const mdb_db *db,
                       const bn_binding *binding, void *arg);

static int md_namespace_commit_rtsp_check (md_commit *commit, mdb_db **db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg);

static int md_namespace_reval_hdlr(md_commit *commit, mdb_db **db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg);


static int md_namespace_commit_dependency_check(
		const mdb_db_change *change,
		mdb_db *inout_new_db,
		dependency_type_t dependency,
		tbool *retval);

static int
md_nvsd_policy_bind_commit_check(md_commit *commit,
                           const mdb_db *old_db, const mdb_db *new_db,
                           const mdb_db_change_array *change_list,
                           const tstring *node_name,
                           const tstr_array *node_name_parts,
                           mdb_db_change_type change_type,
                           const bn_attrib_array *old_attribs,
                           const bn_attrib_array *new_attribs, void *arg);

static int
md_nvsd_vip_commit_check(md_commit *commit,
                           const mdb_db *old_db, const mdb_db *new_db,
                           const mdb_db_change_array *change_list,
                           const tstring *node_name,
                           const tstr_array *node_name_parts,
                           mdb_db_change_type change_type,
                           const bn_attrib_array *old_attribs,
                           const bn_attrib_array *new_attribs, void *arg);

int
md_nvsd_regex_check(const char *regex, error_msg_t errorbuf, int errorbuf_size);

const bn_str_value md_probe_email_init_val[] = {
	{"/email/notify/events/"
		"\\/nkn\\/nvsd\\/notify\\/nvsd_stuck",
		bt_name, "/nkn/nvsd/notify/nvsd_stuck"},
	{"/email/notify/events/"
		"\\/nkn\\/nvsd\\/notify\\/nvsd_stuck/enable",
		bt_bool, "true"},
};

#include "md_nvsd_namespace_ipt.inc.c"

typedef int  (*md_lib_check_valid_bind) (md_commit *commit, const mdb_db *old_db,
				tbool *vaild, const char *match_val,
				const char *nd_fmt, const char *val);

md_lib_check_valid_bind md_nvsd_namespace_pe_check_bind = NULL;
/*--------------------------------------------------------------------------*/
int
md_nvsd_namespace_init(
        const lc_dso_info* info,
        void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;
    void *md_ssld_check_func_v = NULL;

    err = mdm_get_symbol("md_ssld_cfg", "md_ssld_cfg_is_cert_valid",
		    (void*)&md_ssld_check_func_v);
    bail_error_null(err, md_ssld_check_func_v);

    md_nvsd_namespace_pe_check_bind = md_ssld_check_func_v;

    //TODO:need to add upgrade ?
    err = mdm_add_module("nvsd-namespace", 53,
            "/nkn/nvsd/namespace",
            NULL,
            modrf_upgrade_from | modrf_namespace_unrestricted,
            md_namespace_commit_side_effects,   // side_effects_func
            NULL,				// side_effects_arg
            md_namespace_commit_check,          // check_func
            NULL,                               // check_arg
            NULL,		              // apply_func
            NULL,                               // apply_arg
            0,                                  // commit_order
            5000,                                  // apply_order
            md_namespace_upgrade_downgrade,       // updown_func
            &md_nvsd_ns_ug_rules,                // updown_arg
            md_nvsd_namespace_add_initial,      // initial_func
            NULL,                               // initial_arg
            NULL,                               // mon_get_func
            NULL,                               // mon_get_arg
            NULL,                               // mon_iterate_func
            NULL,                               // mon_iterate_arg
            &module);                           // ret_module
    bail_error(err);
    lc_log_basic(LOG_DEBUG, "module namespace invoked" );

    err = mdm_set_upgrade_from(module, "nvsd-uri", 6, true);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "module nvsd-uri upgrade from called" );
#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*";
    node->mrn_value_type =      bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_NAMESPACE_LENGTH;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_limit_str_no_charlist = " /\\*~=.-+:|`\"?";
    node->mrn_description =     "Namespace name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pinned_object/auto_pin";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Objects ingested to this namespace will be pinned or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/status/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Is this namespace active or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		 = "/nkn/nvsd/namespace/*/cluster/*";
    node->mrn_value_type	 = bt_uint32;
    node->mrn_node_reg_flags	 = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask		 = mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 1;
    node->mrn_bad_value_msg	 = "error: Remove the associated cluster and add";
    node->mrn_description	= "Cluster Map(s)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/namespace/*/cluster/*/cl-name";
    node->mrn_value_type	= bt_string;
    node->mrn_initial_value	= "";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask	   	= mcf_cap_node_basic;
    node->mrn_description	= "Cluster Map(s) associates with the namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/virtual_player";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Virtual player associated with this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/uid";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Auto-generated Unique ID for the namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/delivery/proto/http";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "true";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Delivery protocol for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/delivery/proto/rtsp";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value =   "false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Delivery protocol for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/prestage/user/*";
    node->mrn_value_type =      bt_string;
    node->mrn_limit_str_max_chars = 32;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Prestage user list";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/prestage/user/*/password";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Prestage user Password";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/prestage/cron_time";
    node->mrn_value_type =      bt_uint16;
    node->mrn_initial_value_int =  5;
    node->mrn_limit_num_min_int =  2;
    node->mrn_limit_num_max_int =  10;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "script will be run every this mins";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/prestage/file_age";
    node->mrn_value_type =      bt_uint16;
    node->mrn_initial_value_int =  30;
    node->mrn_limit_num_min_int =  10;
    node->mrn_limit_num_max_int =  240;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Age of file, beyond that these will be deleted";
    err = mdm_add_node(module, &node);
    bail_error(err);

    // TODO: Should do a check on whether the namespace exists or not
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/cache_inherit";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Inherit cache parameters from this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/cluster-hash";
    node->mrn_value_type =      bt_uint32;
    node->mrn_initial_value =   "2";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Cluster-hash calculation option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/proxy-mode";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "reverse";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =
        "one to specify the operating mode of the namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/ip_tproxy";
    node->mrn_value_type =      bt_ipv4addr;
    node->mrn_initial_value =   "0.0.0.0";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "IP Address on which transparent proxying is done";
    node->mrn_check_node_func 	=     	NULL;
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/domain/host";
    node->mrn_value_type 	=	bt_string;
    node->mrn_initial_value	=	"*";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"domain name for this namespace";
    node->mrn_check_node_func 	=     	md_rfc952_domain_validate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		=	"/nkn/nvsd/namespace/*/fqdn-list/*";
    node->mrn_value_type	=	bt_string;
    node->mrn_limit_wc_count_max =      NKN_MAX_FQDNS;
    node->mrn_limit_str_max_chars =	NKN_MAX_FQDN_LENGTH;
    node->mrn_bad_value_msg	=       "maximum of 16 FQDNs allowed";
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask		=       mcf_cap_node_basic;
    node->mrn_description	=	"domain name for this namespace";
    node->mrn_check_node_func	=	md_rfc952_domain_validate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value = 	"";
    node->mrn_limit_str_no_charlist = 	"/\\*:|`\"?";
    node->mrn_limit_wc_count_max = 4;
    node->mrn_limit_wc_count_min = 0;
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 4 live publish points");
    node->mrn_description =     "Live stream publish point index - Max of 4 allowed";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/active";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value = 	"false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point active or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/mode";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value = 	"";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : on-demand or immediate";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/sdp";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value = 	"";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : SDP name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/start-time";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value = 	"";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : immediate start-time";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/end-time";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value = 	"";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : immediate end-time";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/caching/enable";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value = 	"false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point : allow MFD to cache the stream";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/caching/format/chunked-ts";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value = 	"false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : Caching format - chunked-ts";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/caching/format/frag-mp4";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value = 	"false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : Caching format - frag-mp4";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/caching/format/moof";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value = 	"false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : Caching format - moof";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/pub-point/*/caching/format/chunk-flv";
    node->mrn_value_type =      bt_bool;
    node->mrn_initial_value = 	"false";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_restricted;
    node->mrn_description =     "Pub point receive-mode : Caching format - chunk-flv";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/domain/regex";
    node->mrn_value_type 	=	bt_regex;
    node->mrn_initial_value	=	"";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"domain regex for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* These nodes reappeared in Version 12 of this module. */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/type";
    node->mrn_value_type 	=	bt_uint8;
    node->mrn_initial_value	=	"0";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"match_type_t - holds info on which node is set";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/header/name";
    node->mrn_value_type 	=	bt_string;
    node->mrn_initial_value	=	"";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"header name for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/header/value";
    node->mrn_value_type 	=	bt_string;
    node->mrn_initial_value	=	"*";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"header value for this namespace, "
	    				"'*' is a special case to indicate any value";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/header/regex";
    node->mrn_value_type 	=	bt_regex;
    node->mrn_initial_value	=	"";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"header regex for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);



    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/query-string/name";
    node->mrn_value_type 	=	bt_string;
    node->mrn_initial_value	=	"";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"query string name for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/query-string/value";
    node->mrn_value_type 	=	bt_string;
    node->mrn_initial_value	=	"";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"query string value for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/query-string/regex";
    node->mrn_value_type 	=	bt_regex;
    node->mrn_initial_value	=	"";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"query string regex for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/virtual-host/ip";
    node->mrn_value_type 	=	bt_ipv4addr;
    node->mrn_initial_value	=	"255.255.255.255";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"virtual host ip for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		=	"/nkn/nvsd/namespace/*/match/virtual-host/port";
    node->mrn_value_type 	=	bt_uint16;
    node->mrn_initial_value	=	"80";
    node->mrn_node_reg_flags 	=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		=       mcf_cap_node_basic;
    node->mrn_description 	=     	"virtual host port for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/match/uri/uri_name";
    node->mrn_value_type 		=	bt_uri;
    node->mrn_initial_value		=	"";
    node->mrn_limit_str_max_chars 	= 	256;
    node->mrn_node_reg_flags 		=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=       mcf_cap_node_basic;
    node->mrn_check_node_func 		=     	md_nvsd_uri_name_validate;
    node->mrn_description 		=     	"uri prefix";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/match/uri/regex";
    node->mrn_value_type 		=	bt_regex;
    node->mrn_initial_value		=	"";
    node->mrn_limit_str_max_chars 	= 	256;
    node->mrn_node_reg_flags 		=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=       mcf_cap_node_basic;
    node->mrn_description 		=     	"uri prefix regex based";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name			=	"/nkn/nvsd/namespace/*/ns-precedence";
    node->mrn_value_type		=	bt_uint32;
    node->mrn_initial_value		=	"0";
    node->mrn_limit_num_min_int		=	0;
    node->mrn_limit_num_max_int		=	65535; // 2^16 - 1
    node->mrn_node_reg_flags		=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask			=       mcf_cap_node_basic;
    node->mrn_description		=	"namespace precedence";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/match/precedence";
    node->mrn_value_type 		=	bt_uint32;
    node->mrn_initial_value		=	"0";
    node->mrn_limit_num_min_int 	=  	0;
    node->mrn_limit_num_max_int 	=  	10;
    node->mrn_node_reg_flags 		=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=       mcf_cap_node_basic;
    node->mrn_description 		=     	"namespace match precedence";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/type";
    node->mrn_value_type 		=	bt_uint8;
    node->mrn_initial_value		=	"0";
    node->mrn_node_reg_flags 		=  	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=       mcf_cap_node_basic;
    node->mrn_description 		=     	"refers to the type of origin server being used for this namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*--------------------------*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/origin_fetch/config/*/serve-expired/enable";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Deliver expired content";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/host/name";
    node->mrn_value_type 		=	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		= 	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			= 	mcf_cap_node_basic;
    node->mrn_check_node_func		= 	md_nvsd_ns_http_host_validate;
    node->mrn_check_node_arg 		= 	NULL;
    node->mrn_description 		=  	"HTTP Host origin";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/host/port";
    node->mrn_value_type 		=	bt_uint16;
    node->mrn_initial_value 		=       "80";
    node->mrn_node_reg_flags 		= 	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			= 	mcf_cap_node_basic;
    node->mrn_description 		=  	"HTTP Host origin port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                     =       "/nkn/nvsd/namespace/*/origin-server/http/host/aws/access-key";
    node->mrn_value_type               =       bt_string;
    node->mrn_initial_value            =       "";
    node->mrn_node_reg_flags           =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                 =       mcf_cap_node_basic;
    node->mrn_description              =       "HTTP Host origin aws access-key";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                     =       "/nkn/nvsd/namespace/*/origin-server/http/host/aws/secret-key";
    node->mrn_value_type               =       bt_string;
    node->mrn_initial_value            =       "";
    node->mrn_node_reg_flags           =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                 =       mcf_cap_node_basic;
    node->mrn_description              =       "HTTP Host origin aws secret-key";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                     =       "/nkn/nvsd/namespace/*/origin-server/http/host/dns-query";
    node->mrn_value_type               =       bt_string;
    node->mrn_initial_value            =       "";
    node->mrn_node_reg_flags           =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                 =       mcf_cap_node_basic;
    node->mrn_description              =       "HTTP Host origin dns-query";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*--------------------------*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name	    ="/nkn/nvsd/namespace/*/origin-server/http/server-map/*";
    node->mrn_value_type	=	bt_uint32;
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask	    =	mcf_cap_node_basic;
    node->mrn_limit_wc_count_max	=   3;
    node->mrn_bad_value_msg	    =	"error: maximum of 3 Servermaps allowed";
    node->mrn_description		=   "Server-maps";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name
	    ="/nkn/nvsd/namespace/*/origin-server/http/server-map/*/map-name";
    node->mrn_value_type	    =	    bt_string;
    node->mrn_initial_value	    =       "";
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask	   		=	mcf_cap_node_basic;
    node->mrn_description		=   "server-map associates with the namespace";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/absolute-url";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"enable / disbale  absolute-url lookup";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/namespace/*/origin-server/http/ip-version";
    node->mrn_value_type =      bt_string;
    node->mrn_initial_value =   "follow-client";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =        mcf_cap_node_basic;
    node->mrn_description =     "set ip-version to origin server as follow client's version";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		    =	"/nkn/nvsd/namespace/*/origin-server/http/idle-timeout";
    node->mrn_value_type	    =	bt_uint32;
    node->mrn_initial_value	    =	"60";
    node->mrn_limit_num_min_int	    =	0;
    node->mrn_limit_num_max_int	    =	600;
    node->mrn_node_reg_flags	    =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask		    =   mcf_cap_node_basic;
    node->mrn_description	    =	"Origin Idle Timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/follow/header";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Points to a follow-header http origin-server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/follow/dest-ip";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Allow MFD to use the destination IP on incoming request as origin server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/follow/use-client-ip";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Allow MFD to use the Client Src IP on incoming request as source IP (tproxy)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/http/follow/use-client-ip/header";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Allow MFD to use the value in this header "
	    "as the Client Src IP on incoming request as source IP (tproxy)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                     =       "/nkn/nvsd/namespace/*/origin-server/http/follow/aws/access-key";
    node->mrn_value_type               =       bt_string;
    node->mrn_initial_value            =       "";
    node->mrn_node_reg_flags           =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                 =       mcf_cap_node_basic;
    node->mrn_description              =       "follow-header http origin-server aws access-key";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                     =       "/nkn/nvsd/namespace/*/origin-server/http/follow/aws/secret-key";
    node->mrn_value_type               =       bt_string;
    node->mrn_initial_value            =       "";
    node->mrn_node_reg_flags           =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                 =       mcf_cap_node_basic;
    node->mrn_description              =       "follow-header http origin-server aws secret-key";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                     =       "/nkn/nvsd/namespace/*/origin-server/http/follow/dns-query";
    node->mrn_value_type               =       bt_string;
    node->mrn_initial_value            =       "";
    node->mrn_node_reg_flags           =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                 =       mcf_cap_node_basic;
    node->mrn_description              =       "follow-header http origin-server dns-query";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/nfs/host";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_limit_str_max_chars 	= 	256;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_check_node_func 		=     	md_nvsd_uri_nfs_host_validate;
    node->mrn_description 		= 	"nfs host path";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/nfs/port";
    node->mrn_value_type 		=      	bt_uint16;
    node->mrn_initial_value 		=       "2049";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"nfs host port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/nfs/local-cache";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "true";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"nfs local cache ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/nfs/server-map";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"nfs server map definition";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/rtsp/host/name";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"RTSP fqdn name definition";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/rtsp/host/port";
    node->mrn_value_type 		=      	bt_uint16;
    node->mrn_initial_value 		=       "554";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"RTSP fqdn port definition";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/rtsp/host/transport";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"RTSP Transport Type";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/rtsp/follow/dest-ip";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"RTSP Follow the destination IP as the origin-server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-server/rtsp/follow/use-client-ip";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"RTSP - Spoof the client-IP as ours when connecting to the origin";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy-map";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"namespace policy-mapping";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-request/x-forwarded-for/enable";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Set X-Forwarded-For header when request is made to Origin-Server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                      =       "/nkn/nvsd/namespace/*/origin-request/include-orig-ip-port";
    node->mrn_value_type                =       bt_bool;
    node->mrn_initial_value             =       "false";
    node->mrn_node_reg_flags            =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                  =       mcf_cap_node_basic;
    node->mrn_description               =       "Include original ip and port in a private HTTP header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-request/header/add/*";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_limit_wc_count_max 	=  	4;
    node->mrn_bad_value_msg 		= 	"error: maximum of 4 Headers allowed";
    node->mrn_description 		= 	"Add/Append for HTTP headers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-request/header/add/*/name";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Add/Append a HTTP header name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/origin-request/header/add/*/value";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Add/Append a HTTP header value";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name	                =      "/nkn/nvsd/namespace/*/origin-request/connect/timeout";
    node->mrn_value_type	    =	    bt_uint32;
    node->mrn_initial_value	    =       "100";
    node->mrn_limit_num_min_int	=       0;
    node->mrn_limit_num_max_int	    =       3600000;
    node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
    node->mrn_cap_mask		=   mcf_cap_node_basic;
    node->mrn_description	    =	"OM socket connect timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name   = "/nkn/nvsd/namespace/*/origin-request/connect/retry-delay";
    node->mrn_value_type	    =	    bt_uint32;
    node->mrn_initial_value	    =       "100";
    node->mrn_limit_num_min_int	=       0;
    node->mrn_limit_num_max_int	    =       3600000;
    node->mrn_node_reg_flags	=    mrf_flags_reg_config_literal;
    node->mrn_cap_mask		=   mcf_cap_node_basic;
    node->mrn_description	    =	"OM socket connect timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name  ="/nkn/nvsd/namespace/*/origin-request/read/interval-timeout";
    node->mrn_value_type    =	    bt_uint32;
    node->mrn_initial_value    =       "100";
    node->mrn_limit_num_min_int	    =       0;
    node->mrn_limit_num_max_int	    =	    3600000;
    node->mrn_node_reg_flags	    =	    mrf_flags_reg_config_literal;
    node->mrn_cap_mask	    =	    mcf_cap_node_basic;
    node->mrn_description	    =	    "OM socket interval read timeout";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module,&node);
    bail_error(err);
    node->mrn_name ="/nkn/nvsd/namespace/*/origin-request/read/retry-delay";
    node->mrn_value_type =  bt_uint32;
    node->mrn_initial_value =  "100";
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 3600000;
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_description ="OM socket read timeout";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                      =       "/nkn/nvsd/namespace/*/client-response/dscp";
    node->mrn_value_type                =       bt_int32;
    node->mrn_initial_value             =       "-1";
    node->mrn_node_reg_flags            =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                  =       mcf_cap_node_basic;
    node->mrn_description               =       "DSCP value set out in client-response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name                      =       "/nkn/nvsd/namespace/*/client-response/checksum";
    node->mrn_value_type                =       bt_uint32;
    node->mrn_initial_value             =       "0";
    node->mrn_node_reg_flags            =       mrf_flags_reg_config_literal;
    node->mrn_cap_mask                  =       mcf_cap_node_basic;
    node->mrn_description               =       "MD5 checksum bytes in client-response";
    err = mdm_add_node(module, &node);



    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-response/header/add/*";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_limit_wc_count_max 	=  	8;
    node->mrn_bad_value_msg 		= 	"error: maximum of 8 Headers allowed";
    node->mrn_description 		= 	"Add/Append for HTTP headers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-response/header/add/*/name";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Add/Append a HTTP header name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-response/header/add/*/value";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Add/Append a HTTP header value";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-response/header/delete/*";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_limit_wc_count_max 	=  	8;
    node->mrn_bad_value_msg 		= 	"error: maximum of 8 Headers allowed";
    node->mrn_description 		= 	"Add/Append for HTTP headers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-response/header/delete/*/name";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Add/Append a HTTP header name";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* As part of HTTP Session admission control based on
     * Session count
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/resource-pool/limit/http/max-session/count";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Maximum client sessions allowed by "
	    					"HTTP clients. 0 - indicates no limit";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/resource-pool/limit/http/max-session/retry-after";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_initial_value 		=       "0";
    node->mrn_limit_num_min_int		= 	0;
    node->mrn_limit_num_max_int		=	86400;
    node->mrn_bad_value_msg		= 	N_("Value must be between 0 and 86400 (24 hours)");
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Retry-After header value. "
	    					"0 - indicates no header to be inserted";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* As part of RTSP Session admission control based on
     * Session count
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/resource-pool/limit/rtsp/max-session/count";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Maximum client sessions allowed by "
	    					"RTSP clients. 0 - indicates no limit";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/resource-pool/limit/rtsp/max-session/retry-after";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Retry-After header value. "
	    					"0 - indicates no header to be inserted";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/dynamic-uri/regex";
    node->mrn_value_type =          bt_regex;
    node->mrn_initial_value =       "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/dynamic-uri/map-string";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_limit_str_max_chars = 2048;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/dynamic-uri/tokenize-string";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/dynamic-uri/no-match-tunnel";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/seek-uri/enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/seek-uri/regex";
    node->mrn_value_type =          bt_regex;
    node->mrn_initial_value =       "";
    node->mrn_limit_str_max_chars = 1024;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/seek-uri/map-string";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_limit_str_max_chars = 2048;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-request/header/accept/*";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_limit_wc_count_max 	=  	8;
    node->mrn_bad_value_msg 		= 	"error: maximum of 8 Headers allowed";
    node->mrn_description 		= 	"accept/ignore for HTTP headers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-request/header/accept/*/name";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"accept/ignore HTTP header name";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*The service can be either maxmind or quova*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/geo-service/service";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*The api-url node is valid only for the quova service*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/geo-service/api-url";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* By default the failover mode is reject - false
     * The bypass mode has the value - true
     */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/geo-service/failover-mode";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Timeout option for the geo-ip lookup
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/client-request/geo-service/lookup-timeout";
    node->mrn_value_type 		=      	bt_uint32;
    node->mrn_initial_value 		=       "5000";
    node->mrn_limit_num_min_int         =       0;
    node->mrn_limit_num_max_int         =       300000;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Timeout for geo-service lookup in msec ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* http secure option */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/secure";
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
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/plain-text";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* origin http secure option */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/origin-request/secure";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "HTTPs Origin Fetch support enable";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/method/head/cache-miss/action/tunnel";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Tunnel all cache-miss head method requests";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/method/head/cache-miss/action/convert-to/method/get";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Convert method head to get for all cache miss requests";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/dns/mismatch/action/tunnel";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/filter-map";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/filter-action";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/filter-type";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/uf/200-msg";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/uf/3xx-fqdn";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/uf/3xx-uri";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/filter-uri-size";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   32768;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "max allowed uri size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/filter-uri-size-action";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/uf-uri-size/200-msg";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/uf-uri-size/3xx-fqdn";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/client-request/uf-uri-size/3xx-uri";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/get_ramcache_list";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func     = md_commit_forward_action;
    node->mrn_action_arg        = &md_namespace_fwd_args;
    node->mrn_actions[0]->mra_param_name = "namespace";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "uid";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "filename";
    node->mrn_actions[2]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/is_obj_cached";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func     = md_commit_forward_action;
    node->mrn_action_arg        = &md_namespace_fwd_args;
    node->mrn_actions[0]->mra_param_name = "namespace";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "uid";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "uri";
    node->mrn_actions[2]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* policy engine
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/file";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"TCL file containing the policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_limit_str_max_chars       =	NKN_MAX_PE_SRVR_STR;
    node->mrn_limit_str_no_charlist     =	NKN_PE_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max        =	NKN_MAX_PE_SRVR_BIND;
    node->mrn_bad_value_msg             =       "Error creating Node";
    node->mrn_initial_value 		=       "";
    node->mrn_check_node_func		=       md_nvsd_policy_bind_commit_check;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*/failover";
    node->mrn_value_type 		=      	bt_uint16;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_restricted;
    node->mrn_description 		= 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*/precedence";
    node->mrn_value_type 		=      	bt_uint16;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_restricted;
    node->mrn_description 		= 	"";
    node->mrn_limit_num_max             =	"10";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*/callout/client_req";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_restricted;
    node->mrn_description 		= 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*/callout/client_resp";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_restricted;
    node->mrn_description 		= 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*/callout/origin_req";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_restricted;
    node->mrn_description 		= 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/server/*/callout/origin_resp";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_restricted;
    node->mrn_description 		= 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/policy/time_stamp";
    node->mrn_value_type 		=      	bt_int32;
    node->mrn_initial_value 		=       "0";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"TCL file containing the policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*  Resource Pool
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/resource_pool";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       GLOBAL_RSRC_POOL;
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"Reference to resource pool bound";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Resource Pool deleted */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/deleted";
    node->mrn_value_type 		=      	bt_bool;
    node->mrn_initial_value 		=       "false";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_description 		= 	"namespace deleted or not";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/sata/readsize";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =	2048;
    node->mrn_limit_str_choices_str     = ",2048,256";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_bad_value_msg =       "error: supported read sizes are 2048, 256";
    node->mrn_description =         "Use 2048/256 for read size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*  Accesslog profile
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 			=	"/nkn/nvsd/namespace/*/accesslog";
    node->mrn_value_type 		=      	bt_string;
    node->mrn_initial_value 		=       "default";
    node->mrn_node_reg_flags 		=      	mrf_flags_reg_config_literal;
    node->mrn_cap_mask 			=  	mcf_cap_node_basic;
    node->mrn_check_node_func		=	md_nvsd_ns_acclog_bind_check_func;
    node->mrn_description 		= 	"Accesslog profile that this namespace will use";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/sas/readsize";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =	2048;
    node->mrn_limit_str_choices_str     = ",2048,256";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_bad_value_msg =       "error: supported read sizes are 2048, 256";
    node->mrn_description =         "Use 2048/256 for read size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/ssd/readsize";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =	256;
    node->mrn_limit_str_choices_str     = ",256,32";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_bad_value_msg =       "error: supported read sizes are 256, 32";
    node->mrn_description =         "Use 256/32 for read size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/sata/free_block_thres";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =	50;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int=    100;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/sas/free_block_thres";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =	50;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int=    100;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/ssd/free_block_thres";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =	50;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int=    100;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/sata/group_read_enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value	=       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/sas/group_read_enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value	=       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/mediacache/ssd/group_read_enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value	=       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/resource/cache/sas/capacity";
    node->mrn_value_type =          bt_uint64;
    node->mrn_initial_value	=       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/resource/cache/ssd/capacity";
    node->mrn_value_type =          bt_uint64;
    node->mrn_initial_value	=       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/resource/cache/sata/capacity";
    node->mrn_value_type =          bt_uint64;
    node->mrn_initial_value	=       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/vip/*";
    node->mrn_value_type =          bt_string;
    node->mrn_limit_wc_count_max =  NKN_MAX_VIPS;
    node->mrn_bad_value_msg =       N_("Cannot bind more than 8 IP's");
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_check_node_func =     md_nvsd_vip_commit_check;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/namespace/*/distrib-id";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Distribution-ID to be used with REST-APIs";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 5);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/delete_uri";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_gmgmthd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "namespace";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "uri-pattern";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "proxy-domain";
    node->mrn_actions[2]->mra_param_type = bt_string;
    node->mrn_actions[3]->mra_param_name = "proxy-port";
    node->mrn_actions[3]->mra_param_type = bt_string;
    node->mrn_actions[4]->mra_param_name = "pinned";
    node->mrn_actions[4]->mra_param_type = bt_bool;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/get_lost_uids";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_namespace_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/get_namespace_precedence_list";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_namespace_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name               = "/nkn/nvsd/namespace/actions/validate_namespace_match_header";
    node->mrn_node_reg_flags     = mrf_flags_reg_action;
    node->mrn_cap_mask           = mcf_cap_action_privileged;
    node->mrn_action_config_func = md_namespace_commit_rtsp_check;
    node->mrn_action_arg         = (void *)NULL;
    node->mrn_description        = "action node for match header check";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/revalidate_cache_timer";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_config_func = md_namespace_reval_hdlr;
    node->mrn_action_arg        = NULL;
    node->mrn_actions[0]->mra_param_name = "namespace";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "type";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*Event to send email when we think NVSD might be stuck */
    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/notify/nvsd_stuck";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "failed_check";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_description = "Event sent to notify that nvsd might be stuck";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Event to send SNMP trap when cluster node changes state */
    err = mdm_new_event(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/cluster/node_up";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/cluster-namespace";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "/nkn/monitor/cluster-instance";
    node->mrn_events[1]->mre_binding_type = bt_int32;
    node->mrn_events[2]->mre_binding_name = "/nkn/monitor/nodename";
    node->mrn_events[2]->mre_binding_type = bt_string;
    node->mrn_description = "Cluster node is up";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Event to send SNMP trap when cluster node changes state */
    err = mdm_new_event(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/cluster/node_down";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/cluster-namespace";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "/nkn/monitor/cluster-instance";
    node->mrn_events[1]->mre_binding_type = bt_int32;
    node->mrn_events[2]->mre_binding_name = "/nkn/monitor/nodename";
    node->mrn_events[2]->mre_binding_type = bt_string;
    node->mrn_description = "Cluster node status down";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* this will return one binding named "output" string */
    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/namespace/actions/encrypt_string";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_privileged;
    node->mrn_action_func	= md_encrypt_string;
    node->mrn_actions[0]->mra_param_name = "input_string";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "action to convert string to encrypted form";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_nvsd_ns_ug_rules);
    bail_error(err);
    ra = md_nvsd_ns_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			= 	"/nkn/nvsd/namespace/*/proxy-mode";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"reverse";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=	"/nkn/nvsd/namespace/*/match/header/name";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=       "/nkn/nvsd/namespace/*/match/header/value";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str	 	=   	"*";
    rule->mur_cond_name_does_not_exist 	=	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=       "/nkn/nvsd/namespace/*/tproxy_interface";
    rule->mur_new_value_type 		=  	bt_ipv4addr;
    rule->mur_new_value_str		=   	"0.0.0.0";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=       "/nkn/nvsd/namespace/*/match/header/regex";
    rule->mur_new_value_type 		=  	bt_regex;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=       "/nkn/nvsd/namespace/*/domain/host";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"*";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			= 	"/nkn/nvsd/namespace/*/domain/regex";
    rule->mur_new_value_type 		=  	bt_regex;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	=	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=     	"/nkn/nvsd/namespace/*/match/uri/uri_name";
    rule->mur_new_value_type 		=  	bt_uri;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	=	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/match/type";
    rule->mur_new_value_type 		=  	bt_uint8;
    rule->mur_new_value_str 		=   	"0";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=       "/nkn/nvsd/namespace/*/match/uri/regex";
    rule->mur_new_value_type 		=  	bt_regex;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			= 	"/nkn/nvsd/namespace/*/match/query-string/name";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=       "/nkn/nvsd/namespace/*/match/query-string/value";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=  	"/nkn/nvsd/namespace/*/match/query-string/regex";
    rule->mur_new_value_type 		=  	bt_regex;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/match/virtual-host/ip";
    rule->mur_new_value_type 		=  	bt_ipv4addr;
    rule->mur_new_value_str 		=   	"255.255.255.255";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=   	"/nkn/nvsd/namespace/*/match/virtual-host/port";
    rule->mur_new_value_type 		=  	bt_uint16;
    rule->mur_new_value_str 		=   	"80";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/type";
    rule->mur_new_value_type 		=  	bt_uint8;
    rule->mur_new_value_str 		=   	"0";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/http/server-map";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/http/absolute-url";
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"false";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/nvsd/namespace/*/origin-server/http/ip-version/follow-client";
    rule->mur_new_value_type            =       bt_bool;
    rule->mur_new_value_str             =       "true";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/nvsd/namespace/*/origin-server/http/ip-version/force-ipv4";
    rule->mur_new_value_type            =       bt_bool;
    rule->mur_new_value_str             =       "false";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/nvsd/namespace/*/origin-server/http/ip-version/force-ipv6";
    rule->mur_new_value_type            =       bt_bool;
    rule->mur_new_value_str             =       "false";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/http/follow/header";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/http/follow/dest-ip";
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"false";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/http/follow/use-client-ip";
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"false";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/http/follow/use-client-ip/header";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/nfs/host";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/nfs/port";
    rule->mur_new_value_type 		=  	bt_uint16;
    rule->mur_new_value_str 		=   	"2049";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/nfs/local-cache";
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"true";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/origin-server/nfs/server-map";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			=    	"/nkn/nvsd/namespace/*/policy-map";
    rule->mur_new_value_type 		=  	bt_string;
    rule->mur_new_value_str 		=   	"";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type 			=	MUTT_REPARENT;
    rule->mur_name 				=	"/nkn/nvsd/namespace/*/precedence";
    rule->mur_reparent_self_value_follows_name 	= 	true;
    rule->mur_reparent_graft_under_name 	= 	"/nkn/nvsd/namespace/*/match";
    rule->mur_reparent_new_self_name 		= 	NULL;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/tproxy_interface";
    rule->mur_new_value_type =  bt_ipv4addr;
    rule->mur_new_value_str =   "0.0.0.0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type 			=	MUTT_REPARENT;
    rule->mur_name 				=	"/nkn/nvsd/namespace/*/tproxy_interface";
    rule->mur_reparent_self_value_follows_name 	= 	false;
    rule->mur_reparent_graft_under_name 	= 	NULL; //"/nkn/nvsd/namespace/*/ip_tproxy";
    rule->mur_reparent_new_self_name 		= 	"ip_tproxy";
    MD_ADD_RULE(ra);

    /* Bug 5843 */
    /* Add the ip_tproxy node,Just in case, if we don't have this node already */
    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type 			=	MUTT_ADD;
    rule->mur_name 				=	"/nkn/nvsd/namespace/*/ip_tproxy";
    rule->mur_new_value_type 			=  	bt_ipv4addr;
    rule->mur_new_value_str 			=   	"0.0.0.0";
    rule->mur_cond_name_does_not_exist 		= 	true;
    MD_ADD_RULE(ra);

    ///// CMM Additions here @ version 13->14 for BZ 4958
    /* The 2.0.1 branch was not fixed correctly for these nodes
     * hence bumping up the version numbers to ensure the 2.0.1
     * upgrade to later version creates these nodes
     * It was 10->11 now changed to 13->14
     * By Ramanand Narayanan (June 3rd 2010)
     */

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type		=MUTT_ADD;
    rule->mur_name		="/nkn/nvsd/namespace/*/cluster-hash";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "2";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type	    =MUTT_ADD;
    rule->mur_name		    ="/nkn/nvsd/namespace/*/origin-request/connect/timeout";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type		=MUTT_ADD;
    rule->mur_name    ="/nkn/nvsd/namespace/*/origin-request/connect/retry-delay";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type   =MUTT_ADD;
    rule->mur_name   ="/nkn/nvsd/namespace/*/origin-request/read/interval-timeout";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "100";
    rule->mur_cond_name_does_not_exist   = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type   =MUTT_ADD;
    rule->mur_name    ="/nkn/nvsd/namespace/*/origin-request/read/retry-delay";
    rule->mur_new_value_type   =  bt_uint32;
    rule->mur_new_value_str    =	    "100";
    rule->mur_cond_name_does_not_exist    =	    true;
    MD_ADD_RULE(ra);
    //// End of CMM nodes


    /// RTSP nodes begin at 11->12 (BZ 4958)
    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-server/rtsp/host/name";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-server/rtsp/host/port";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "554";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-server/rtsp/follow/dest-ip";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-server/rtsp/follow/use-client-ip";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);
    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/namespace/*/pub-point";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-server/rtsp/host/transport";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);
 /*BUG :9734
  * The version is out of sync. conejos is at 19. This node is introduced in denali.
  * Moving this node creation upgrade rule to 19 ,20)
  */
    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource-pool/limit/http/max-session/count";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource-pool/limit/http/max-session/retry-after";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource-pool/limit/rtsp/max-session/count";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource-pool/limit/rtsp/max-session/retry-after";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/file";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    /* not bumping up the module versions */
    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource_pool";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   GLOBAL_RSRC_POOL;
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/deleted";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type = 	MUTT_CLAMP;
    rule->mur_name = 		"/nkn/nvsd/namespace/*/resource-pool/limit/http/max-session/retry-after";
    rule->mur_lowerbound =	0;
    rule->mur_upperbound = 	86400;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/dynamic-uri/regex";
    rule->mur_new_value_type =  bt_regex;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/dynamic-uri/map-string";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/dynamic-uri/no-match-tunnel";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/failover";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/precedence";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/callout/client_req";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/callout/client_resp";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/callout/origin_req";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/callout/origin_resp";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/server/*/callout/client_resp";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/origin_fetch/config/mfc_probe/cache-age/default";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "120";
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/dynamic-uri/tokenize-string";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/seek-uri/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/seek-uri/map-string";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/seek-uri/regex";
    rule->mur_new_value_type =  bt_regex;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 22, 23);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/namespace/mfc_probe/domain/host";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "127.0.0.2";
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 22, 23);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/prestage/cron_time";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "5";
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 22, 23);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/prestage/file_age";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "30";
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 24, 25);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/geo-service/service";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 24, 25);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/geo-service/api-url";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 24, 25);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/geo-service/failover-mode";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/geo-service/lookup-timeout";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "5000";
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 26, 27);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/policy/time_stamp";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/secure";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 27, 28);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/plain-text";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/sata/readsize";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "2048";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/sas/readsize";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "2048";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 28, 29);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/ssd/readsize";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "256";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/sata/free_block_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "50";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/sas/free_block_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "50";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/ssd/free_block_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "50";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/sata/group_read_enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/sas/group_read_enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/mediacache/ssd/group_read_enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 29, 30);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/accesslog";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "default";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 30, 31);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource/cache/sas/capacity";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 30, 31);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource/cache/ssd/capacity";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 30, 31);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/resource/cache/sata/capacity";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 31, 32);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/pinned_object/auto_pin";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 32, 33);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-request/secure";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 32, 33);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-request/plain-text";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 33, 34);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-request/include-orig-ip-port";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 34, 35);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/namespace/*/origin-request/plain-text";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/client-response/dscp";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str =   "-1";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 35, 36);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/client-response/checksum";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 36, 37);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/origin_fetch/config/*/serve-expired/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 37, 38);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/nvsd/namespace/*/origin-server/http/idle-timeout";
    rule->mur_new_value_type            =       bt_uint32;
    rule->mur_new_value_str             =       "60";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    /*
    * This fixes the upgrade path from 11.B.10 (everett commit 28054).
    * because it may ignore the upgrade rule (33->34).
    * So Rule (33->34) is applied as (38->39).
    */
    MD_NEW_RULE(ra, 38, 39);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-request/include-orig-ip-port";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 45, 46);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/ns-precedence";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 45, 46);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/namespace/mfc_probe/ns-precedence";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "14000";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 46, 47);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/namespace/mfc_probe/ns-precedence";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "10";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 47, 48);
    rule->mur_upgrade_type =    MUTT_RESTRICT_LENGTH;
    rule->mur_name =            "/nkn/nvsd/namespace/*/fqdn-list/*";
    rule->mur_len_max =         256;
    rule->mur_len_pad_char =    'x';
    rule->mur_allowed_chars =  CLIST_HOSTNAME_NOIPV6;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 47, 48);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/method/head/cache-miss/action/tunnel";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 47, 48);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/method/head/cache-miss/action/convert-to/method/get";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 48, 49);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/distrib-id";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-server/http/host/aws/access-key";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-server/http/host/aws/secret-key";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-server/http/follow/aws/access-key";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-server/http/follow/aws/secret-key";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/client-request/header/accept/*";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/client-request/header/accept/*/name";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 49, 50);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/dns/mismatch/action/tunnel";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  50, 51);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-server/http/host/dns-query";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra,  50, 51);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/namespace/*/origin-server/http/follow/dns-query";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =   "";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 51, 52);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/filter-map";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 51, 52);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/filter-action";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 51, 52);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/filter-type";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 51, 52);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/uf/200-msg";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 51, 52);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/uf/3xx-fqdn";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 51, 52);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/uf/3xx-uri";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 52, 53);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/filter-uri-size";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 52, 53);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/uf-uri-size/200-msg";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 52, 53);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/uf-uri-size/3xx-fqdn";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 52, 53);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/uf-uri-size/3xx-uri";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 52, 53);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/namespace/*/client-request/filter-uri-size-action";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);


    lc_log_basic(LOG_DEBUG, "namespace upgrade rules added" );

bail:
    return err;
}

static int
md_namespace_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg)
{
	int err = 0, any_configured = 0;
	tstr_array *host_tstr_bindings = NULL, *domain_tstr_bindings = NULL;
	tstr_array *binding_parts= NULL;
	tstring *host_value = NULL, *domain_value = NULL, *regex_value = NULL;
	const char *t_namespace = NULL, *host_binding = NULL, *ns_name = NULL;
	const char *t_host= NULL;
	const char *t_uri = NULL;
	char *binding_name = NULL;
	const char *old_binding_name = NULL, *new_fqdn = NULL;
	tbool found = false;
	unsigned int arr_index, domain_array_length = 0;
	tstring *uri_name = NULL;
	char *node_port = NULL;
	tstring *t_port = NULL;
	tstr_array *ns_bindings = NULL;
	tstring *server_map = NULL;
    	char *ns_probe = NULL;
	tbool probefound = false;
	tstring *t_ns_probe = NULL;
	node_name_t regex_node = {0};



    	lc_log_basic(LOG_INFO, "called from %d to %d", from_module_version,to_module_version );

    	err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
            					to_module_version, arg);
    	bail_error(err);

	if ((to_module_version == 7)) {

		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/../domain/host
		 * To	: /nkn/nvsd/namespace/.../domain/host
		 *----------------------------------------------------------*/
		err = mdb_get_matching_tstr_array( NULL,
						inout_new_db,
						"/nkn/nvsd/uri/config/*/uri-prefix/*/domain/host",
						0,
						&host_tstr_bindings);
		bail_error(err);
		bail_null(host_tstr_bindings);
    		lc_log_basic(LOG_INFO, "total host_tstr_bindings %d", tstr_array_length_quick(host_tstr_bindings)  );

		for ( arr_index = 0; arr_index < tstr_array_length_quick(host_tstr_bindings); ++arr_index)
		{
			old_binding_name = tstr_array_get_str_quick(
					host_tstr_bindings, arr_index);
			err = md_nvsd_ns_upgrade_domain_host(
					inout_new_db, old_binding_name);
			bail_error(err);
		}
		tstr_array_free(&host_tstr_bindings);


		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/../domain/regex
		 * To	: /nkn/nvsd/namespace/.../domain/regex
		 *----------------------------------------------------------*/
		err = mdb_get_matching_tstr_array( NULL,
						inout_new_db,
						"/nkn/nvsd/uri/config/*/uri-prefix/*/domain/regex",
						0,
						&host_tstr_bindings);
		bail_error(err);
		bail_null(host_tstr_bindings);
    		
		lc_log_basic(LOG_INFO, "total host_tstr_bindings regex %d", tstr_array_length_quick(host_tstr_bindings)  );

		for ( arr_index = 0 ; arr_index < tstr_array_length_quick(host_tstr_bindings) ; ++arr_index)
		{
			old_binding_name = tstr_array_get_str_quick(
					host_tstr_bindings, arr_index);

			err = md_nvsd_ns_upgrade_domain_regex(
					inout_new_db,
					old_binding_name);
			bail_error(err);

		}
		tstr_array_free(&host_tstr_bindings);


		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/../origin-server/http/host/..
		 * To	: /nkn/nvsd/namespace/.../origin-server/http/host/name
		 *----------------------------------------------------------*/
		err = mdb_get_matching_tstr_array( NULL,
						inout_new_db,
						"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/host/*",
						0,
						&host_tstr_bindings);
		bail_error(err);
    		lc_log_basic(LOG_INFO, "total origin server host %d", tstr_array_length_quick(host_tstr_bindings)  );
		bail_null(host_tstr_bindings);
		for ( arr_index = 0 ; arr_index < tstr_array_length_quick(host_tstr_bindings) ; ++arr_index)
		{
			old_binding_name = tstr_array_get_str_quick(host_tstr_bindings, arr_index);

			err = bn_binding_name_to_parts (old_binding_name,
							 &binding_parts, NULL);
			bail_error(err);
			t_namespace = tstr_array_get_str_quick (binding_parts, 4);
			t_host = tstr_array_get_str_quick (binding_parts, 10);
			t_uri = tstr_array_get_str_quick (binding_parts, 6);

			binding_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host",
					t_namespace);

			err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0,
					bt_string,
					t_host,
					"%s/name",
					binding_name);
			bail_error(err);

			/* Get value of HOST/.../PORT */
			node_port = smprintf("%s/port", old_binding_name);
			bail_null(node_port);

			err = mdb_get_node_value_tstr(NULL, inout_new_db,
					node_port,
					0,
					NULL,
					&t_port);
			bail_error(err);

			err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0,
					bt_uint16,
					ts_str(t_port),
					"%s/port",
					binding_name);
			bail_error(err);
			safe_free(node_port);
			ts_free(&t_port);

			err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0,
					bt_string,
					t_host,
					"%s",
					old_binding_name);
			bail_error(err);
			
			err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
					0, bt_uint8,
					osvr_types[osvr_http_host],
					"/nkn/nvsd/namespace/%s/origin-server/type",
					t_namespace);
			bail_error(err);
			safe_free(binding_name);
			tstr_array_free(&binding_parts);
		}
		tstr_array_free(&host_tstr_bindings);


		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/../origin-server/http/server-map
		 * To	: /nkn/nvsd/namespace/.../origin-server/http/server-map
		 *----------------------------------------------------------*/
		err = mdb_get_matching_tstr_array( NULL,
						inout_new_db,
						"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/server-map",
						0,
						&host_tstr_bindings);
		bail_error(err);
		bail_null(host_tstr_bindings);
    		lc_log_basic(LOG_INFO, "total http server-map %d", tstr_array_length_quick(host_tstr_bindings)  );
		for ( arr_index = 0 ; arr_index < tstr_array_length_quick(host_tstr_bindings) ; ++arr_index)
		{
			err = bn_binding_name_to_parts (tstr_array_get_str_quick(host_tstr_bindings, arr_index),
							 &binding_parts, NULL);
			bail_error(err);
			t_namespace = tstr_array_get_str_quick (binding_parts, 4);
			
			err = mdb_get_node_value_tstr(NULL, inout_new_db, tstr_array_get_str_quick(host_tstr_bindings, arr_index),
							0, &found, &host_value);
			bail_error(err);
			if (found && (ts_length(host_value) > 0))
			{
				err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
					0, bt_string,
					ts_str(host_value),
					"/nkn/nvsd/namespace/%s/origin-server/http/server-map",
					t_namespace);
				bail_error(err);
				err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
						0, bt_uint8,
						osvr_types[osvr_http_server_map],
						"/nkn/nvsd/namespace/%s/origin-server/type",
						t_namespace);
				bail_error(err);
			}
			err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
					0, bt_string, "", "%s", tstr_array_get_str_quick(host_tstr_bindings, arr_index));
			bail_error(err);

			tstr_array_free(&binding_parts);
		}
		tstr_array_free(&host_tstr_bindings);




		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/../origin-server/nfs/...
		 * To	: /nkn/nvsd/namespace/.../origin-server/nfs/...
		 *----------------------------------------------------------*/

		err = md_nvsd_namespace_ug_nfs_6_to_7(
				old_db, inout_new_db,
				from_module_version, to_module_version,
				arg);
		bail_error(err);

		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/../delivery/proto
		 * To	: /nkn/nvsd/namespace/.../delivery/protocol
		 *----------------------------------------------------------*/
		err = mdb_get_matching_tstr_array( NULL,
				inout_new_db,
				"/nkn/nvsd/uri/config/*/uri-prefix/*/delivery/proto/*",
				0,
				&host_tstr_bindings);
		bail_error(err);
		bail_null(host_tstr_bindings);

		for (arr_index = 0; arr_index < tstr_array_length_quick(host_tstr_bindings); ++arr_index)
		{
			const char *proto = NULL;
			char *uri_escaped = NULL;
			err = bn_binding_name_to_parts (
					tstr_array_get_str_quick(host_tstr_bindings, arr_index),
					 &binding_parts, NULL);
			bail_error(err);
			t_namespace = tstr_array_get_str_quick (binding_parts, 4);
			t_uri = tstr_array_get_str_quick (binding_parts, 6);
			proto = tstr_array_get_str_quick(binding_parts, 9);

			err = bn_binding_name_escape_str(t_uri, &uri_escaped);
			bail_error(err);

			err = md_nvsd_ns_upgrade_delivery_proto(
					inout_new_db,
					t_namespace, uri_escaped, proto);
			bail_error(err);
			tstr_array_free(&binding_parts);
		}
		tstr_array_free(&host_tstr_bindings);


		/*-----------------------------------------------------------
		 * Move : /nkn/nvsd/uri/config/../uri-prefix/..
		 * To	: /nkn/nvsd/namespace/.../match/uri_name
		 *----------------------------------------------------------*/
		err = mdb_get_matching_tstr_array(NULL,
				inout_new_db,
				"/nkn/nvsd/uri/config/*/uri-prefix/*",
				0,
				&host_tstr_bindings);
		bail_error(err);
		bail_null(host_tstr_bindings);

		for (arr_index = 0; arr_index < tstr_array_length_quick(host_tstr_bindings); ++arr_index)
		{
			old_binding_name = tstr_array_get_str_quick(
					host_tstr_bindings, arr_index);
			err = bn_binding_name_to_parts (old_binding_name,
							 &binding_parts, NULL);
			bail_error(err);
			t_namespace = tstr_array_get_str_quick (binding_parts, 4);

			err = mdb_get_node_value_tstr(NULL, inout_new_db,
					old_binding_name,
					0, &found,
					&uri_name);
			bail_error(err);

			if (found) {
				match_type_data_t mt;
				mt.match_type = mt_uri_name;

				err = md_nvsd_namespace_http_match_criteria_set(
						NULL,			// md_commit
						old_db,			// old_db
						inout_new_db,		// new_db
						t_namespace,		// namespace
						&mt);
				bail_error(err);

				err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
						0, bt_uri,
						ts_str(uri_name),
						"/nkn/nvsd/namespace/%s/match/uri/uri_name",
						t_namespace);
				bail_error(err);

				// delete old node here
				err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
						0, bt_uri,
						ts_str(uri_name),
						"%s",
						old_binding_name);
				bail_error(err);
				
				err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
						0, bt_uri,
						t_namespace,
						"/nkn/nvsd/uri/config/%s",
						t_namespace);
				bail_error(err);

			}
		}
		tstr_array_free(&host_tstr_bindings);

	}
	//check version
	if ((from_module_version == 10) && (to_module_version == 11)) {
                mdb_db *t_inout_new_db = inout_new_db;

    		lc_log_basic(LOG_INFO, "Upgrading server-map nodes");
		err = mdb_iterate_binding_cb(NULL, inout_new_db,
				 "/nkn/nvsd/namespace",
                                 mdqf_sel_iterate_subtree |
                                 mdqf_sel_iterate_leaf_nodes_only,
                                 md_upgrade_servermap, &t_inout_new_db);
		bail_error(err);
	}
/* Thilak - Commenting this out,as the probe may not be used as of now*/	
#if 1
	else if(to_module_version == 16) {

		ns_probe = smprintf("%s", "/nkn/nvsd/namespace/mfc_probe");
		err = mdb_get_node_value_tstr(NULL, inout_new_db,
				ns_probe,
				0, &probefound,
				&t_ns_probe);

		bail_error(err);
		if(1/*probefound*/) {
			err = mdb_create_node_bindings(NULL, inout_new_db,
					md_nvsd_namespace_probe_initial_values,
					sizeof( md_nvsd_namespace_probe_initial_values )/sizeof(bn_str_value));
			bail_error(err);
		}else
			lc_log_basic(LOG_DEBUG,"Probe namespace found!!!");

		err = mdb_create_node_bindings
			(NULL, inout_new_db, md_probe_email_init_val,
			 sizeof(md_probe_email_init_val) / sizeof(bn_str_value));
		bail_error(err);
	}

#endif
	else if (to_module_version == 45) {
	    /* move domain/host => fqdn-list/<host> */
	    tstr_array_free(&domain_tstr_bindings);
	    err = mdb_get_matching_tstr_array(NULL,
		    inout_new_db,
		    "/nkn/nvsd/namespace/*/domain/host",
		    0,
		    &domain_tstr_bindings);
	    bail_error(err);

	    domain_array_length = tstr_array_length_quick(domain_tstr_bindings);
	    for (arr_index = 0; arr_index < domain_array_length; arr_index++) {
		host_binding = tstr_array_get_str_quick(domain_tstr_bindings
			, arr_index);
		if (host_binding == NULL) {
		    continue;
		}
		ts_free(&domain_value);
		err = mdb_get_node_value_tstr(NULL, inout_new_db,
			host_binding,
			0, &found,
			&domain_value);
		bail_error(err);

		if (domain_value == NULL || ts_equal_str(domain_value, "", true)) {
		    /* this means regx is configured, no need to do anything */
		    continue;
		}
		any_configured = 0;
		if (ts_equal_str(domain_value, "*", true)) {
		    /* "*" is configured, either regex is configured or
		       any is configured */
		    any_configured = 1;
		}
		tstr_array_free(&binding_parts);
		err = bn_binding_name_to_parts(host_binding, &binding_parts, NULL);
		bail_error(err);
		ns_name = tstr_array_get_str_quick(binding_parts, 3);
		if (ns_name == NULL) {
		    continue;
		}
		snprintf(regex_node, sizeof(regex_node),
			"/nkn/nvsd/namespace/%s/domain/regex", ns_name);
		ts_free(&regex_value);
		err = mdb_get_node_value_tstr(NULL, inout_new_db,
			regex_node, 0, NULL, &regex_value);
		bail_error(err);

		if ((regex_value == NULL)
			|| (ts_equal_str(regex_value, "", true))) {
		    /* regex is not configured, so should create fqdn binding */
		    if (any_configured) {
			new_fqdn = "any";
		    } else {
			new_fqdn = ts_str(domain_value);
		    }
		} else {
		    new_fqdn = NULL;
		}

		if (new_fqdn != NULL) {
		    err = mdb_set_node_str(NULL, inout_new_db, bsso_create
			    , 0, bt_string, new_fqdn,
			    "/nkn/nvsd/namespace/%s/fqdn-list/%s",
			    ns_name, new_fqdn);
		}
	    }
	}
	lc_log_basic(LOG_INFO, "namespace upgrade call done");
bail:
	ts_free(&regex_value);
	ts_free(&domain_value);
	safe_free(node_port);
	ts_free(&t_port);
	tstr_array_free(&binding_parts);
	tstr_array_free(&host_tstr_bindings);
	tstr_array_free(&domain_tstr_bindings);
	safe_free(binding_name);
	safe_free(ns_probe);
	ts_free(&t_ns_probe);
	return err;
}


/*
 *******************************************************************************
 * hextol_len() -- [ptr, len] hex ascii to long
 * Fudged as is from nvsd/common/http_header.c
 *******************************************************************************
 */
static long
hextol_len(const char *str, int len)
{
    long sign = 1;
    long val = 0;

    while (len && (*str == ' ')) --len, str++;

    if (*str == '-') {
    	sign = -1;
	len--;
	str++;
    }

    while (len) {
    	if (('0' <= *str) && (*str <= '9')) {
	    val = val * 16 + (*str - '0');
	    len--;
	    str++;
    	} else if (('a' <= *str) && (*str <= 'f')) {
	    val = val * 16 + (10 + (*str - 'a'));
	    len--;
	    str++;
    	} else if (('A' <= *str) && (*str <= 'F')) {
	    val = val * 16 + (10 + (*str - 'A'));
	    len--;
	    str++;
	} else {
	    break;
	}
    }
    return sign * val;
}

static int
md_namespace_set_node_map_proxy_mode(md_commit *commit, mdb_db *inout_new_db,
                                     const char *t_namespace)
{
    node_name_t tproxy_cluster_node = {0};
    tbool tproxy_cluster_node_found = false;
    int err = 0;
    tbool tproxy_cluster_enabled;

    snprintf(tproxy_cluster_node, sizeof(tproxy_cluster_node),
             "/nkn/nvsd/namespace/%s/origin-request/include-orig-ip-port", t_namespace);

    err = mdb_get_node_value_tbool(commit, inout_new_db, tproxy_cluster_node, 0,
                                   &tproxy_cluster_node_found,
                                   &tproxy_cluster_enabled);
    bail_error(err);

    if (tproxy_cluster_enabled) {
        err = md_nvsd_namespace_proxy_mode_set(commit, NULL,
                  inout_new_db, t_namespace, "transparent");
    } else {
        err = md_nvsd_namespace_proxy_mode_set(commit, NULL,
                  inout_new_db, t_namespace, "reverse");
    }
    bail_error(err);

bail:
    return err;
}

static int
md_namespace_node_map_enabled(md_commit *commit, mdb_db *inout_new_db,
                        const char *t_namespace, tbool *enabled_p)

{	
    tstr_array *t_smaps_http = NULL;
    node_name_t t_ns_smap_node = {0};
    int err = 0;
    int asso_count;

    *enabled_p = false;

    snprintf(t_ns_smap_node, sizeof(t_ns_smap_node),
             "/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name",
             t_namespace);

    err = mdb_get_matching_tstr_array(commit, inout_new_db,
                                      t_ns_smap_node, 0, &t_smaps_http);
    bail_error(err);

    if (t_smaps_http != NULL) {
        asso_count = tstr_array_length_quick(t_smaps_http);
	
        if (asso_count) {
            *enabled_p = true;
        }
    }

bail:
    tstr_array_free(&t_smaps_http);
    return err;
}

/*----------------------------------------------------------------------------
 * MODULE: nvsd-namespace
 *
 * WHAT : Side effects function. Called when a node change request is issued
 * Always gets called first, before the commit_check and/or commit_apply
 *--------------------------------------------------------------------------*/
static int
md_namespace_commit_side_effects(
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
    bn_binding *binding = NULL;
    const char *ns_name = NULL;
    tstr_array *fqdn_list = NULL;
    uint32_t num_fqdn = 0;
    const char *username = NULL;
    char *auth_user = NULL;
    char *auth_uid = NULL;
    char *auth_gid = NULL;
    char *auth_home = NULL;
    char *auth_shell = NULL;
    char *auth_enable = NULL;
    char *auth_passwd = NULL;
    const char *passwd = NULL;
    char *crypted_passwd = NULL;
    char *ns_passwd = NULL;
    tstring *t_pass = NULL;
    char *user_name = NULL;
    char *user_name_binding = NULL;
    tstring	*t_namespace = NULL, *ns_value = NULL;
    tstring	*t_uid = NULL;
    char *origin_fetch_binding = NULL;
    char *uri_prefix_binding = NULL;
    char *uid_temp = NULL;
    tstring *ns_uid = NULL;
    tstring *ret_msg = NULL;
    match_type_data_t mt;
    origin_server_data_t ot;
    char *node_name = NULL;
    tstring *t_value = NULL;
    int32 status = 0;
    char *val = NULL;
    const bn_attrib *new_val = NULL;
    tstring *t_server_map = NULL;
    tstring *t_format_type = NULL;
    char *smap_fmt_type_node = NULL;
    tstring *t_ipaddr = NULL;
    const bn_attrib *old_val = NULL;
    tstring *t_old_ip = NULL;
    char *node_status = NULL;
    tstring *ts_is_active = NULL;
    int ftp_cron_installed = false;
    tbool node_map_enabled;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);
 	
	
	/* Check if it is namespace creation */
	if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 4, "nkn", "nvsd", "namespace", "*"))
		&& (mdct_add == change->mdc_change_type))
	{
	    bn_str_value	binding_value;
	    bn_str_value    binding_value_user;

	    /* First get the new namespace name */
	    err = mdb_get_node_value_tstr(commit, inout_new_db,
		    ts_str(change->mdc_name), 0, &found,
		    &t_namespace);
	    bail_error (err);

	    // Create origin-fetch nodes here
	    origin_fetch_binding = smprintf("/nkn/nvsd/origin_fetch/config/%s", ts_str (t_namespace));
	    bail_null(origin_fetch_binding);

	    binding_value.bv_name = origin_fetch_binding;
	    binding_value.bv_type = bt_string;
	    binding_value.bv_value = ts_str (t_namespace);

	    err = mdb_create_node_bindings (commit, inout_new_db, &binding_value, 1);
	    bail_error (err);

	    // Bug Fix - 961
	    // Also create a pre-stage user for this namespace.
	    //
	    user_name = smprintf("%s_ftpuser", ts_str(t_namespace));
	    bail_null(user_name);

	    user_name_binding = smprintf("/nkn/nvsd/namespace/%s/prestage/user/%s",
		    ts_str(t_namespace), user_name);
	    bail_null(user_name_binding);

	    binding_value_user.bv_name = user_name_binding;
	    binding_value_user.bv_type = bt_string;
	    binding_value_user.bv_value = user_name;

	    err = mdb_create_node_bindings(commit, inout_new_db, &binding_value_user, 1);
	    bail_error(err);
	}
	/* Check if it is domain creation */
	else if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 5, "nkn", "nvsd", "namespace", "*", "uid"))
		&& (mdct_add == change->mdc_change_type))
	{
	    unsigned char	*digest;
	    char		uuid_str [80];
	    char		date_str [40];
	    char		uuid_key [80];
	    time_t		t;
	    struct tm	*tmp_time;
	    const char 	*c_namespace = NULL;
	    int64_t 	k1, k2;


	    /* First check to see if value already exits */
	    err = mdb_get_node_value_tstr(commit, inout_new_db,
		    ts_str(change->mdc_name), 0, &found,
		    &t_uid);
	    bail_error (err);

	    if ((NULL == t_uid) || ts_equal_str (t_uid, "", false))
	    {

		c_namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);
		if (NULL == c_namespace)
		    goto bail;

		/* Generate a unique ID */
		/* The current plan is to use the namespace name and do a MD5
		 * on it as namespace has to be unique */
		t = time (NULL);
		tmp_time = localtime(&t);
		if (tmp_time == NULL)
		    date_str [35] = '\0';	// Use the random data in the string
		else
		    strftime (date_str, 35, "%a %D %T", tmp_time);

		memset (uuid_key, 0, sizeof (uuid_key));
		strncat (uuid_key, c_namespace, 35);
		strncat (uuid_key, date_str, 35);

		digest = MD5 ((const unsigned char*)uuid_key, strlen (uuid_key), NULL);

		memset (uuid_str, 0, sizeof (uuid_str));
		uuid_str [0] = '\0';
		for (i = 0; i < 16; i++)
		{
		    char t_str[3];

		    snprintf(t_str, 2, "%02x", digest[i]);
		    t_str[2] = '\0';
		    strcat (uuid_str, t_str);
		}

		// Compress UID to 32 bits
		k1 = hextol_len(uuid_str, 16);
		k2 = hextol_len(&uuid_str[16], 16);
		k1 = k1 ^ k2;
		k1 = (k1 & 0xffffffff) ^ ((k1 >> 32) & 0xffffffff);

		memset (uuid_str, 0, sizeof (uuid_str));
		snprintf(uuid_str, 79, "/%s:%lx", c_namespace, k1);

		/* Set the node to this unique id */
		err = mdb_set_node_str (commit, inout_new_db, bsso_modify, 0,
			bt_string, uuid_str, "%s", ts_str(change->mdc_name));
		bail_error (err);
	    }
	}

	/* Check if it is namespace deletion */
	else if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 4, "nkn", "nvsd", "namespace", "*"))
		&& (mdct_delete == change->mdc_change_type))
	{
	    // BZ 2734
	    /* First get the namespace name */
	    err = mdb_get_node_value_tstr(commit, old_db,
		    ts_str(change->mdc_name), 0, &found,
		    &t_namespace);
	    bail_error (err);

	    uid_temp = smprintf("/nkn/nvsd/namespace/%s/uid", ts_str(t_namespace));
	    bail_null(uid_temp);


	    err = mdb_get_node_value_tstr(commit, old_db, uid_temp, 0, NULL, &ns_uid);
	    bail_error(err);

	    err = delete_temp_files (ts_str_maybe_empty(ns_uid));
	    bail_error(err);

	    // Remove origin-fetch nodes here
	    origin_fetch_binding = smprintf("/nkn/nvsd/origin_fetch/config/%s", ts_str (t_namespace));
	    bail_null(origin_fetch_binding);

	    auth_user = smprintf("/auth/passwd/user/%s_ftpuser", ts_str(t_namespace));
	    bail_null(auth_user);

	    user_name = smprintf("%s_ftpuser", ts_str(t_namespace));
	    bail_null(user_name);

	    err = bn_binding_new_str(&binding, origin_fetch_binding, ba_value, bt_string, 0, ts_str(t_namespace));
	    bail_error (err);

	    err = mdb_set_node_binding (commit, inout_new_db, bsso_delete, 0, binding);
	    bail_error (err);
	    bn_binding_free(&binding);

	    err = bn_binding_new_str(&binding, auth_user, ba_value, bt_string, 0, user_name);
	    bail_error(err);
	    // Also need to delete the ftp prestage user
	    //
	    err = mdb_set_node_binding(commit, inout_new_db, bsso_delete, 0, binding);
	    bail_error(err);
	    bn_binding_free(&binding);

	}
	/* Check if pre-stage user creation */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "prestage", "user", "*")
		    || (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
			    "nkn", "nvsd", "namespace", "*", "prestage", "user", "*", "password")))
		&& (mdct_add == change->mdc_change_type))
	{
	    tbool node_found = false;
	    tbool enabled = false;
	    bn_str_value binding_value[5];
	    node_name_t passwd_node = {0};
	    str_value_t password = {0};
	    tstring *ns_password = NULL;

	    ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(ns_name);

	    username = tstr_array_get_str_quick(change->mdc_name_parts, 6);
	    bail_null(username);
	    lc_log_debug(jnpr_log_level, "GOT a user=> %s", username);

	    auth_user = smprintf("/auth/passwd/user/%s", username);
	    bail_null(auth_user);
	    auth_passwd = smprintf("/auth/passwd/user/%s/password", username);
	    bail_null(auth_passwd);
	    auth_shell = smprintf("/auth/passwd/user/%s/shell", username);
	    bail_null(auth_shell);
	    auth_gid = smprintf("/auth/passwd/user/%s/gid", username);
	    bail_null(auth_gid);
	    auth_enable = smprintf("/auth/passwd/user/%s/enable", username);
	    bail_null(auth_enable);

	    err = mdb_get_node_value_tbool(commit, inout_new_db, auth_enable, 0, &node_found, &enabled);
	    bail_error(err);

	    snprintf(passwd_node, sizeof(passwd_node),
		    "/nkn/nvsd/namespace/%s/prestage/user/%s/password",
		    ns_name,username);

	    if (node_found && enabled)
		continue;

	    err = mdb_get_node_value_tstr(commit, inout_new_db, passwd_node,
		    0, NULL, &ns_password);
	    bail_error(err);

	    if (ns_password == NULL || ts_equal_str(ns_password, "", false)) {
		snprintf(password, sizeof(password), "*");
	    } else {
		snprintf(password, sizeof(password), "%s", ts_str(ns_password));
	    }
	    ts_free(&ns_password);
	    // Add a user into /auth/passwd/user/$1$
	    // Set the GID to 50 for /auth/passwd/user/$1$/gid
	    // Set the HOME_DIR for /auth/passwd/user/$1$/home_dir
	    // Set the UID to 1000+ for /auth/passwd/user/$1$/uid
	    // Set the password for /auth/passwd/user/$1$/password
	    err = bn_binding_new_str(&binding, auth_user, ba_value, bt_string, 0, username);
	    bail_error(err);
	    binding_value[0].bv_name = auth_user;
	    binding_value[0].bv_type = bt_string;
	    binding_value[0].bv_value = username;

	    binding_value[1].bv_name = auth_gid;
	    binding_value[1].bv_type = bt_uint32;
	    binding_value[1].bv_value = "2000"; //"100";

	    binding_value[2].bv_name = auth_shell;
	    binding_value[2].bv_type = bt_string;
	    binding_value[2].bv_value = "/sbin/nologin";

	    binding_value[3].bv_name = auth_enable;
	    binding_value[3].bv_type = bt_bool;
	    binding_value[3].bv_value = "true";

	    binding_value[4].bv_name = auth_passwd;
	    binding_value[4].bv_type = bt_string;
	    binding_value[4].bv_value = password;

	    err = mdb_create_node_bindings(commit, inout_new_db, binding_value,
		    sizeof(binding_value)/sizeof(bn_str_value));
	    bail_error(err);

	}
	/* Check if user sets up a password */
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
		    "nkn", "nvsd", "namespace", "*", "prestage", "user", "*", "password")
		&& (mdct_modify == change->mdc_change_type)) {
	    char ftp_username[64] = {0};

	    /* XXX - assuming the ftp username */
	    snprintf(ftp_username, sizeof(ftp_username), "%s_ftpuser",
		    tstr_array_get_str_quick(change->mdc_name_parts, 3));
	    username = ftp_username;

	    safe_free(ns_passwd);
	    ns_passwd = smprintf("/nkn/nvsd/namespace/%s/prestage/user/%s/password",
		    tstr_array_get_str_quick(change->mdc_name_parts, 3), username);
	    bail_null(ns_passwd);

	    safe_free(auth_passwd);
	    auth_passwd = smprintf("/auth/passwd/user/%s/password", username);
	    bail_null(auth_passwd);

	    ts_free(&t_pass);
	    // Get password from the node
	    err = mdb_get_node_value_tstr(commit, inout_new_db, ns_passwd, 0, NULL, &t_pass);
	    bail_error(err);

	    err = bn_binding_new_str(&binding, auth_passwd, ba_value, bt_string, 0, ts_str(t_pass));
	    bail_error_null(err, binding);

	    err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0, binding);
	    bail_error(err);

	}
	/* Check if match type being set.. if so then reset existing match type */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "uri", "uri_name"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    mt.match_type = mt_uri_name;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_uri, "")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "uri", "regex"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    mt.match_type = mt_uri_regex;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_regex, "")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "header","name"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    tbool b_dependency;
	    mt.match_type = mt_header_name;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	    err = md_namespace_commit_dependency_check(change, inout_new_db, dt_match_header_name, &b_dependency);
	    bail_error(err);
	    if(b_dependency) {
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			_("error: %s\n"), "Match header cannot be set as delivery protocol is RTSP");
		bail_error(err);

	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "header", "regex"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    tbool b_dependency;
	    mt.match_type = mt_header_regex;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_regex, "")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	    err = md_namespace_commit_dependency_check(change, inout_new_db, dt_match_header_regex, &b_dependency);
	    bail_error(err);
	    if(b_dependency) {
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			_("error: %s\n"), "Match header regex cannot be set as delivery protocol is RTSP");
		bail_error(err);

	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "query-string", "name"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    mt.match_type = mt_query_string_name;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "query-string", "regex"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    mt.match_type = mt_query_string_regex;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_regex, "")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "virtual-host", "ip"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    mt.match_type = mt_vhost;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_ipv4addr, "255.255.255.255")) {
		err = md_nvsd_namespace_http_match_criteria_set(
			commit,			// md_commit
			old_db,			// old_db
			inout_new_db,		// new_db
			tstr_array_get_str_quick(change->mdc_name_parts, 3),		// namespace
			&mt);
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "host", "name"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    ot.type = osvr_http_host;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"reverse");
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 9, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "server-map", "*", "map-name"))
		&& (mdct_add == change->mdc_change_type))
	{
	    /* Get the server-map name */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);

	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_server_map);
		bail_error(err);

		if (ts_num_chars(t_server_map) > 0) {
		    /* user is setting up a server-map */

		    smap_fmt_type_node = smprintf("/nkn/nvsd/server-map/config/%s/format-type",
			    ts_str(t_server_map));
		    bail_null(smap_fmt_type_node);

		    /* Get the format type of the server-map */
		    err = mdb_get_node_value_tstr(commit, inout_new_db, smap_fmt_type_node, 0, &found,
			    &t_format_type);
		    bail_error(err);

		    if (!found) {
			/* Bad server-map name */
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error: non-existant server-map : '%s'\n"),
				ts_str(t_server_map));
			bail_error(err);
			goto bail;
		    }
		    else {
			/* Server map name exists. Check format type */
			if (ts_equal_str(t_format_type, "0", true)) {
			    /* Bad! this is not a valid smap for this origin type */
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("error: format type of server-map '%s' should be a valid one.\n"),
				    ts_str(t_server_map));
			    bail_error(err);
			    goto bail;
			}
			else if(ts_equal_str(t_format_type, "1", true)) {
			    ot.type = osvr_http_server_map;
			}
			else if(ts_equal_str(t_format_type, "2", true)) {
			    ot.type = osvr_http_node_map;
			}	
			else if(ts_equal_str(t_format_type, "3", true)) {
			    ot.type = osvr_http_node_map;
			}	
			else if(ts_equal_str(t_format_type, "4", true)) {
			    ot.type = osvr_nfs_server_map;
			}
			else if(ts_equal_str(t_format_type, "5", true)) {
			    ot.type = osvr_http_node_map;
			}
		    }
		}
	    }
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_namespace_set_node_map_proxy_mode(commit,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3));
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 9,
			"nkn", "nvsd", "namespace", "*", "origin-server", "http", "server-map", "*", "map-name"))
		&& (mdct_delete == change->mdc_change_type))
	{
	    tstr_array *t_smaps_http = NULL;
	    const char *t_ns_name = NULL;
	    const char *t_ns_smap_node = NULL;

	    t_ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    if(t_ns_name != NULL){

		t_ns_smap_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name",
			t_ns_name);

		err = mdb_get_matching_tstr_array(commit,
			old_db,
			t_ns_smap_node,
			0,
			&t_smaps_http);
		bail_error(err);

		if (t_smaps_http != NULL) {

		    int asso_count = tstr_array_length_quick(t_smaps_http);

		    if (asso_count == 1) {
			err = mdb_set_node_str(commit, inout_new_db, bsso_reset,
				0, bt_uint8,
				"0",
				"/nkn/nvsd/namespace/%s/origin-server/type",
				t_ns_name);
			bail_error(err);
		    }
		}
	    }
	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
		    "nkn", "nvsd", "namespace", "*", "origin-request", "include-orig-ip-port") &&
		(mdct_add == change->mdc_change_type ||
		 mdct_modify == change->mdc_change_type)) {

	    err = md_namespace_node_map_enabled(commit, inout_new_db,
		    tstr_array_get_str_quick(change->mdc_name_parts, 3),
		    &node_map_enabled);
	    bail_error(err);

	    if (node_map_enabled) {	
		err = md_namespace_set_node_map_proxy_mode(commit,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3));
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "absolute-url"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    ot.type = osvr_http_abs_url;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_bool, "false")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"mid-tier");
		bail_error(err);
	    }
	}
	/*--------------------------------------
	 * Pure Virtual Proxy mode
	 *------------------------------------*/
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "follow", "header"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    ot.type = osvr_http_follow_header;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"virtual");
		bail_error(err);
	    }

	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-server", "nfs", "host"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    ot.type = osvr_nfs_host;

	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"reverse");
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-server", "nfs", "server-map"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{

	    ot.type = osvr_nfs_server_map;
	    ot.new_attribs = change->mdc_new_attribs;
	    ot.old_attribs = change->mdc_old_attribs;

	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"reverse");
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "follow", "dest-ip"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{

	    ot.type = osvr_http_dest_ip;
	    ot.new_attribs = change->mdc_new_attribs;
	    ot.old_attribs = change->mdc_old_attribs;

	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_bool, "false")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"virtual");
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "follow", "use-client-ip"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_bool, "false")) {
		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"transparent");
		bail_error(err);

	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "rtsp", "host", "name"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    ot.type = osvr_rtsp_host;
	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_string, "")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"reverse");
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "rtsp", "follow", "dest-ip"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{

	    ot.type = osvr_rtsp_dest_ip;
	    ot.new_attribs = change->mdc_new_attribs;
	    ot.old_attribs = change->mdc_old_attribs;

	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_bool, "false")) {
		err = md_nvsd_namespace_origin_server_set(
			commit, old_db,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			&ot);
		bail_error(err);

		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"virtual");
		bail_error(err);

	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "rtsp", "follow", "use-client-ip"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{

	    if (!md_nvsd_namespace_check_if_default(change->mdc_new_attribs, bt_bool, "false")) {
		err = md_nvsd_namespace_proxy_mode_set(
			commit, NULL,
			inout_new_db,
			tstr_array_get_str_quick(change->mdc_name_parts, 3),
			"transparent");
		bail_error(err);
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "policy", "server", "*")))
	{
	    err = md_nvsd_namespace_set_lst_node(commit, old_db,
		    inout_new_db, change,
		    "/nkn/nvsd/policy_engine/config/server/%s/namespace/%s", 3);
	    bail_error(err);
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 6, "nkn", "nvsd","namespace", "*", "fqdn-list", "*")) {
		    //3, 3, "*", "fqdn-list", "*")) {
	    node_name_t  ns_binding = {0},  bn_fqdn = {0};

	    //lc_log_basic(LOG_NOTICE, "FQDN-MATCH");
	    /* get ns-name, check if ns-name exist in new db */
	    ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(ns_name);

	    snprintf(ns_binding,sizeof(ns_binding),
		    "/nkn/nvsd/namespace/%s", ns_name);
	    /* check if namespace is found */
	    ts_free(&ns_value);
	    err = mdb_get_node_value_tstr(commit, inout_new_db, ns_binding,
		    0, &found, &ns_value);
	    ts_free(&ns_value);
	    bail_error(err);

	    if (false == found) {
		/* namespace not found, ignore fqdn deletion also */
		continue;
	    }
	    /* get all fqdn list from new db */
	    snprintf(bn_fqdn, sizeof(bn_fqdn), "/nkn/nvsd/namespace/%s/fqdn-list", ns_name);

	    tstr_array_free(&fqdn_list);
	    err = mdb_get_tstr_array(commit, inout_new_db, bn_fqdn, NULL, false, 0, &fqdn_list);
	    bail_error(err);

	    //tstr_array_dump(fqdn_list, "FDQN-LIST");
	    num_fqdn = tstr_array_length_quick(fqdn_list);
	    /* if list has more than 1 item, check if "any" is there  */
	    if (num_fqdn > 0 ) {
		err = tstr_array_linear_search_str(fqdn_list, "any",0, NULL);
		if (err != lc_err_not_found) {
		    bail_error(err);
		}
		if (err == lc_err_not_found) {
		    err = 0;
		} else if (num_fqdn == 1) {
		    /* only "any" is configured, do nothing */
		} else {
		    /* "any" is found, delete from inout_new_db */
		    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0,
			    bt_string, "any", "/nkn/nvsd/namespace/%s/fqdn-list/%s"
			    ,ns_name, "any");
		    bail_error(err);
		}
	    } else {
		node_name_t domain_regex_node = {0};
		/* check if domain-regex has value */
		snprintf(domain_regex_node, sizeof(domain_regex_node),
			"/nkn/nvsd/namespace/%s/domain/regex", ns_name);

		ts_free(&ns_value);
		err = mdb_get_node_value_tstr(commit, inout_new_db,
			domain_regex_node, 0, &found, &ns_value);
		bail_error(err);
		if (ns_value && (ts_equal_str(ns_value, "", false))) {
		    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0,
			    bt_string, "any", "/nkn/nvsd/namespace/%s/fqdn-list/%s"
			    ,ns_name, "any");
		    bail_error(err);
		}
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    //3, 3, "*", "domain", "regex")
		    0, 6,"nkn","nvsd", "namespace", "*", "domain", "regex")
		&& (change->mdc_change_type != mdct_delete)) {
	    node_name_t domain_regex_node = {0};
	    //lc_log_basic(LOG_NOTICE, "REGEX MATCH");
	    /* if regex is empty, then set "any" to fqdn-list */
	    ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(ns_name);

	    snprintf(domain_regex_node, sizeof(domain_regex_node),
		    "/nkn/nvsd/namespace/%s/domain/regex", ns_name);

	    ts_free(&ns_value);
	    err = mdb_get_node_value_tstr(commit, inout_new_db,
		    domain_regex_node, 0, &found, &ns_value);
	    bail_error(err);

	    if (ns_value && (ts_equal_str(ns_value, "", false))) {
		err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0,
			bt_string, "any", "/nkn/nvsd/namespace/%s/fqdn-list/%s"
			,ns_name, "any");
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 5, "nkn", "nvsd", "namespace", "*","ns-precedence")
		&& (change->mdc_change_type == mdct_add)) {
	    /* if added, check precedence is default ("0") or it is configured */
	    node_name_t  prece_node = {0};
	    uint32_t precedence_value = 0, next_value = 0;
	    str_value_t ns_pvalue = {0};

	    snprintf(prece_node, sizeof(prece_node),
		    "/nkn/nvsd/namespace/%s/ns-precedence", ns_name);

	    err = mdb_get_node_value_uint32(commit, inout_new_db,
		    prece_node, 0, NULL, &precedence_value);
	    bail_error(err);
	    if (precedence_value == 0) {
		/* findout next available precedence */
		err = get_next_ns_precedence(commit, inout_new_db,
			&next_value);
		bail_error(err);

		snprintf(ns_pvalue, sizeof(ns_pvalue), "%d", next_value);
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
			bt_uint32, ns_pvalue, "/nkn/nvsd/namespace/%s/ns-precedence",
			ns_name);
		bail_error(err);
	    }
	}

	/* Free as we loop and allocate again */
	safe_free(auth_user);
	safe_free(auth_shell);
	safe_free(auth_gid);
	safe_free(auth_passwd);
	safe_free(auth_enable);
	safe_free(auth_home);
	safe_free(auth_uid);
	safe_free(origin_fetch_binding);
	safe_free(user_name);
	safe_free(user_name_binding);
	safe_free(ns_passwd);
	safe_free(uid_temp);
	ts_free(&t_namespace);
	ts_free(&t_uid);
	ts_free(&ns_uid);
	bn_binding_free(&binding);
	safe_free(node_name);
	safe_free(val);
	ts_free(&t_value);
	ts_free(&t_ipaddr);
	ts_free(&t_old_ip);
	ts_free(&ts_is_active);
	safe_free(node_status);
    } /* end of for (i = 0; i < num_changes; i++) */

bail:
    tstr_array_free(&fqdn_list);
    safe_free(crypted_passwd);
    safe_free(auth_user);
    ts_free(&ns_value);
    safe_free(auth_passwd);
    safe_free(auth_shell);
    safe_free(auth_gid);
    safe_free(auth_home);
    safe_free(auth_uid);
    safe_free(auth_enable);
    safe_free(user_name);
    safe_free(ns_passwd);
    safe_free(user_name_binding);
    safe_free(origin_fetch_binding);
    ts_free(&t_pass);
    ts_free(&t_namespace);
    ts_free(&t_uid);
    bn_binding_free(&binding);
    safe_free(node_name);
    safe_free(val);
    ts_free(&t_value);
    safe_free(node_status);
    ts_free(&ts_is_active);
    return err;
}


static int
md_namespace_validate_lost_uids (
				md_commit *commit ,
				tstring *t_cache_inherit_name)
{

    int err = 0;
    bn_request *lost_uid = NULL;
    uint16 retError = -1;
    bn_binding_array *lost_uid_array = NULL;
    uint32 lost_uid_arr_len = 0;
    char buff[8] = { 0 };
    tstring *t_lost_uid_list = NULL;

    err = bn_action_request_msg_create(&lost_uid,
	    "/nkn/nvsd/namespace/actions/get_lost_uids");
    bail_error_null (err, lost_uid);

    err =  md_commit_handle_action_from_module(NULL,
	    lost_uid, NULL, &retError, NULL,
	    &lost_uid_array, NULL);
    bail_error(err);
    bail_error(retError);

    lost_uid_arr_len = bn_binding_array_length_quick(lost_uid_array);
    if (lost_uid_arr_len != 0)
    {
	uint32 index = 1;
	for ( ; index <= lost_uid_arr_len ; ++index)
	{
	    snprintf(buff,7, "%d",index);
	    buff[7] = '\0';
	    err = bn_binding_array_get_value_tstr_by_name ( lost_uid_array,
		    buff, NULL, &t_lost_uid_list);
	    bail_error(err);
	    if (ts_equal(t_lost_uid_list, t_cache_inherit_name) == true)
	    {
		ts_free(&t_lost_uid_list);
		break;
	    }
	    ts_free(&t_lost_uid_list);
	}
	if (index == (lost_uid_arr_len + 1))
	{
	    err = md_commit_set_return_status_msg_fmt(commit, 1,
		    _("error: Invalid UID, not found in the existing list\n"));
	    bail_error(err);
	    goto bail;
	}
    }
    else {
	/*! return failure */
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error: Invalid UID, not found in lost uid list\n"));
	bail_error(err);
	goto bail;
    }

bail:
    ts_free(&t_lost_uid_list);
    bn_request_msg_free(&lost_uid);
    bn_binding_array_free(&lost_uid_array);
    return err;    	
}



static int
md_namespace_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0, check_unique_precedence = 0;
    char *node_name = NULL;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    tbool found = false;
    tstr_array *t_name_parts= NULL;
    const bn_attrib *new_val = NULL;
    tstring *proxy_mode = NULL;
    tstr_array *t_active_namespaces = NULL;
    tstring *t_cache_inherit_name = NULL;
    tstring *t_cache_inherit_set_name= NULL;
    tstring *tuid_string = NULL;
    tstring *t_active_namespace = NULL;
    tstring *ret_msg = NULL;
    tstring	*t_virtual_player = NULL;
    tstring *dom_regex_value = NULL;
    bn_request *req = NULL;
    uint16 code = 0;
    tstring *t_server_map = NULL, *t_filter_map = NULL;
    tstring *t_format_type = NULL;
    char *smap_fmt_type_node = NULL;
    tstring *msg = NULL;
    tstring *t_header = NULL;
    char *smap_count_node = NULL;
    char *origin_type_node = NULL;
    tstring *t_smap_count = NULL;
    uint32 num_bindings = 0;
    uint8_t origin_type = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    tstring* t_smap_name = NULL;
    char *smap_node = NULL;
    uint32 t_active_namespaces_count = 0;
    tstr_array *t_smaps_http = NULL;
    tstring *t_filter_action = NULL, *t_200_msg = NULL, *t_3xx_fqdn = NULL; 
    node_name_t uf_node = {0};

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 4, "nkn", "nvsd", "namespace", "*")))
	{
	    const char *t_current_namespace = NULL;
	    /* Validate the namespace name here */
	    t_current_namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    if(mdct_delete == change->mdc_change_type && !strcmp(t_current_namespace, "mfc_probe")) {
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			_("error: Deletion of mfc_probe namespace not allowed\n") );
		bail_error(err);
		goto bail;
	    } else {
		err = md_nvsd_namespace_validate_name (t_current_namespace, &ret_msg);
		if (err) {
		    err = 0;
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    _("error: %s\n"), ts_str(ret_msg));
		    ts_free(&ret_msg);
		    bail_error(err);
		    goto bail;
		}
	    }


	    node_name = smprintf ("/nkn/nvsd/namespace") ;
	    bail_null(node_name);
	    err = mdb_get_tstr_array(commit, old_db, node_name, NULL, false, 0, &t_active_namespaces);
	    bail_error(err);
	    safe_free(node_name);
	    /* Search in the active list */
	    if (t_active_namespaces != NULL)
	    {
		t_active_namespaces_count = tstr_array_length_quick(t_active_namespaces);
		if ((t_active_namespaces_count >= NKN_MAX_NAMESPACES )
			&& (mdct_add == change->mdc_change_type))
		{
		    err = 0;
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1, _("error: Allowed Namespace limit[1024] Reached\n"));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "namespace", "*", "uid"))
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type)))
	{
	    const char *t_current_namespace = NULL;
	    uint32 retIndex = -1;
	    uint32 idx = 0;
            uint32 chid;

	    t_current_namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    err = ts_new_sprintf(&tuid_string, "/nkn/nvsd/namespace/%s/cache_inherit", t_current_namespace);
	    bail_error_null(err, tuid_string);

	    err = mdb_get_node_value_tstr(commit, old_db, ts_str(tuid_string), 0, &found,
		    &t_cache_inherit_set_name);
	    bail_error(err);

	    err = ts_new_sprintf(&tuid_string,"/nkn/nvsd/namespace/%s/uid",t_current_namespace);
	    bail_error(err);

	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(tuid_string), 0, &found,
					  &t_cache_inherit_name);
	    bail_error(err);

	    // first check for "/" to validate the uuid
	    if (t_cache_inherit_name && ts_length(t_cache_inherit_name) > 0) {
		chid = ts_get_char(t_cache_inherit_name, 0);
		if (chid != '/')
		{
			err = md_commit_set_return_status_msg_fmt(
			      commit, 1,
			      _("error: specified UID didn't start with a slash: %s\n"),
			      ts_str(t_cache_inherit_name));
		    	      bail_error(err);
		    	      goto bail;
		}
	    }

	    if (ts_equal_str (t_cache_inherit_set_name, "set", false))
	    {
		/* Post-inherit code. Run only if user explicitly
		   inherits cache of some other namespace.
		   ../uid is already set for the namespace!!
		   ../cache_inherit is being "set" here
		 */

		/*! first check: search all the name spaces for cache inherit
		  if matches return success otherwise search all the lost uids
		 */
		if (t_active_namespaces != NULL)
		{

		    if (t_active_namespaces_count > 0)
		    {
			for ( idx = 0; idx < t_active_namespaces_count ; ++ idx)
			{
			    node_name = smprintf("/nkn/nvsd/namespace/%s/uid",
				    tstr_array_get_str_quick(t_active_namespaces,idx));
			    bail_null(node_name);
			    err = mdb_get_node_value_tstr(commit, old_db, node_name, 0, &found,
				    &t_active_namespace);
			    bail_error(err);
			    if (ts_equal(t_active_namespace, t_cache_inherit_name) == true)
			    {
				ts_free(&t_active_namespace);
				break;
			    }		
			    ts_free(&t_active_namespace);

			}
			if (idx == t_active_namespaces_count)
			{
			    /*! second check: search all the lost name spaces for the current cache inherit
			      if matches return success otherwise return error*/
			    err = md_namespace_validate_lost_uids (commit, t_cache_inherit_name);
			    bail_error (err);
			}
		    }
		    else {
			err = md_namespace_validate_lost_uids (commit, t_cache_inherit_name);
			bail_error (err);
		    }
		}
		/* NO active UID, so search in the Lost UID list */
		else {

		    err = md_namespace_validate_lost_uids (commit, t_cache_inherit_name);
		    bail_error (err);
		}
	    }
	}
	/*----------------------------------------------------
	 * Check if virtual player is valid and exists.
	 *--------------------------------------------------*/
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "namespace", "*", "virtual_player"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    tstring	*t_temp = NULL;

	    /* First get the virtual-player name */
	    err = mdb_get_node_value_tstr(commit, new_db,
		    ts_str(change->mdc_name), 0, &found,
		    &t_virtual_player);

	    /* If the new value is an empty string then it was
	       the no command hence nothing to check */
	    if (0 != strcmp (ts_str(t_virtual_player), ""))
	    {
		/* Now check if this is a valid virtual-player */
		node_name = smprintf ("/nkn/nvsd/virtual_player/config/%s", ts_str(t_virtual_player));
		err = mdb_get_node_value_tstr(commit, new_db, node_name, 0, NULL, &t_temp);
		bail_error(err);
		if (NULL == t_temp)
		{
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    _("error: non-existent virtual-player: %s\n"),
			    ts_str(t_virtual_player));
		    bail_error(err);
		    goto bail;
		}
		ts_free(&t_temp); // Free the tstring
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "namespace", "*", "cluster-hash"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    char *os_type_node = NULL;
	    const char *t_name_space = NULL;
	    uint8_t os_type = 0;

	    t_name_space = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    os_type_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", t_name_space);
	    bail_null(os_type_node);

	    err = mdb_get_node_value_uint8(NULL, new_db,
		    os_type_node, 0,
		    &found, &os_type);
	    bail_error(err);
	    /* If found then only compare it */
	    if(found) {
		if (os_type != osvr_http_node_map){

		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("Value for %s "
				"can be set Only for node type server-map(origin-server)"), ts_str(change->mdc_name));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	/*----------------------------------------------------
	 * Check if match type (criteria) and origin-server
	 * is configured.. otherwise, fail with an error
	 *--------------------------------------------------*/
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "namespace", "*", "status", "active"))
		&& ((mdct_modify == change->mdc_change_type)
		    || (mdct_add == change->mdc_change_type)))
	{
	    const char	*t_namespace = NULL;
	    tbool	t_active = false;
	    tbool invalid = false;

	    /* First get the status */
	    err = mdb_get_node_value_tbool(commit, new_db,
		    ts_str(change->mdc_name), 0, &found,
		    &t_active);

	    t_namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    if (t_active) {
		err = md_nvsd_namespace_validate_on_active(commit,
			old_db,
			new_db,
			t_namespace,
			&invalid);
		bail_error(err);
	    }
	}

	/*----------------------------------------------------
	 * Check if URI is configured and if origin is also
	 * configured.
	 * If so, dont allow setting the proxy-mode to "mid-tier"
	 *--------------------------------------------------*/
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "namespace", "*", "proxy-mode"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    const char *t_namespace = NULL;
	    tbool origin_exists = false;

	    /* Ascertain proxy mode here.
	     *
	     * 1. if origin-server is absolute-url => FORWARD_PROXY
	     * 2. If orgin-server is follow header
	     * 	a.
	     */
	}

	/*--------------------------------------------------------
	 * Check  if Domain/URI/Header/Query-String regex is
	 * valid or not.
	 *------------------------------------------------------*/
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "namespace", "*", "domain", "regex")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "uri", "regex")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "header","regex")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "match", "query-string", "regex")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,"nkn", "nvsd", "namespace", "*", "client-request", "dynamic-uri","regex")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,"nkn", "nvsd", "namespace", "*", "client-request", "seek-uri", "regex"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)) )
	{
	    /* fire action to NVSD to validate the regex.
	     * If regex is unsupported, then fail on commit
	     */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {

		err = bn_attrib_get_tstr(new_val, NULL, bt_regex, NULL, &dom_regex_value);
		bail_error(err);

		if ( dom_regex_value ) {
		    /* NOTE: The action that we fire MUST not change the database
		     *
		     * Send regex for validation only if it is being set. When the
		     * value is being reset, no need to validate.
		     */
		    if ( ts_num_chars(dom_regex_value) > 0 ) {
			/*
			err = bn_action_request_msg_create(&req, "/nkn/nvsd/uri/actions/validate_domain_regex");
			bail_error(err);

			err = bn_action_request_msg_append_new_binding(req, 0, "regex", bt_regex,
				ts_str(dom_regex_value), NULL);
			bail_error(err);

			err = md_commit_handle_action_from_module(commit, req, NULL, &code, &msg, NULL, NULL);
			bail_error(err);

			*/
			error_msg_t error_msg ={0};

			err = md_nvsd_regex_check(ts_str(dom_regex_value), error_msg, sizeof(error_msg));
			if (err) {
			    /* ignoring the error_msg as of now */
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("error: Bad or unrecognized expression '%s'\n"),
				    ts_str(dom_regex_value));
			    bail_error(err);
			    goto bail;
			}
		    }
		}
	    }
	}

	/*--------------------------------------------------------
	 * Check that the server-map type corresponds to one
	 * for a HTTP origin only.
	 *------------------------------------------------------*/
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 9,
		    "nkn", "nvsd", "namespace", "*", "origin-server", "http", "server-map", "*", "map-name")
		&& (mdct_add == change->mdc_change_type))
	{
	    const char *t_current_namespace = NULL;
	    char *smap_status_node = NULL;
	    tbool status_found = false, t_smap_status =false;

	    t_current_namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    /* Get the server-map name */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		int attached = 0;
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_server_map);
		bail_error(err);

		err = md_check_if_smap_attached(commit, old_db, new_db,
			change, ts_str_maybe_empty(t_server_map), &attached);
		bail_error(err);

		if (attached) {
		    /* commit message would have been set by md_check_if_smap_attached() */
		    lc_log_debug(jnpr_log_level, "server-map is already attached ");
		    goto bail;
		}

		/* BZ 2858
		 * For this node, when the value is reset, the node value is set as
		 * "" - an empty string. Hence we need to check if this a set or
		 * a reset operation by doing this the crooked way. The variable
		 * mdc_new_attribs is always non-null in this case (when user
		 * does a reset).
		 */
		if (ts_num_chars(t_server_map) > 0) {
		    /* user is setting up a server-map */

		    smap_fmt_type_node = smprintf("/nkn/nvsd/server-map/config/%s/format-type",
			    ts_str(t_server_map));
		    bail_null(smap_fmt_type_node);

		    /* Get the format type of the server-map */
		    err = mdb_get_node_value_tstr(commit, new_db, smap_fmt_type_node, 0, &found,
			    &t_format_type);
		    bail_error(err);

		    smap_status_node = smprintf("/nkn/nvsd/server-map/config/%s/map-status",
			    ts_str(t_server_map));
		    bail_null(smap_status_node);

		    /* Get the file-refresh status of the server-map */
		    err = mdb_get_node_value_tbool(commit, new_db, smap_status_node, 0, &status_found,
			    &t_smap_status);
		    bail_error(err);

		    if (!found) {
			/* Bad server-map name */
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error: non-existant server-map : '%s'\n"),
				ts_str(t_server_map));
			bail_error(err);
			goto bail;
		    }
		    else {
			/* Server map name exists. Check format type */
			if (ts_equal_str(t_format_type, "0", true)) {
			    /* Bad! this is not a valid smap for this origin type */
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("error: format type of server-map '%s' should be a valid one\n"),
				    ts_str(t_server_map));
			    bail_error(err);
			    goto bail;
			}
			if (!ts_equal_str(t_format_type, "1", true)){
			    if(status_found)
			    {
			    }
			    else
			    {
				err = md_commit_set_return_status_msg_fmt(commit, 1,
					_("error: server-map '%s' file refresh STATUS node was not found\n"),
					ts_str(t_server_map));
				bail_error(err);
				goto bail;
			    }
			}
		    }
		    smap_count_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map", t_current_namespace);
		    bail_null(smap_count_node);

		    err = mdb_iterate_binding(commit, old_db, smap_count_node,
			    0, &binding_arrays);
		    bail_error(err);

		    err = bn_binding_array_length(binding_arrays, &num_bindings);
		    bail_error(err);

		    if (num_bindings != 0) {

			for(uint32 j = 1; j <= num_bindings; j++) {
			    smap_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map/%d/map-name", t_current_namespace, j);
			    bail_null(smap_node);

			    err = mdb_get_node_value_tstr(NULL,
				    old_db,
				    smap_node,
				    0, NULL,
				    &t_smap_name);
			    bail_error(err);

			    if (t_smap_name && ts_equal_str(t_smap_name, ts_str(t_server_map), false)) {
				/* Name matched... REJECT the commit */
				err = md_commit_set_return_status_msg_fmt(
					commit, 1,
					_("Server-map \"%s\" is already in the list"),ts_str(t_server_map));
				bail_error(err);
			    }

			    ts_free(&t_smap_name);
			    t_smap_name = NULL;
			}

			origin_type_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", t_current_namespace);
			bail_null(origin_type_node);

			err = mdb_get_node_value_uint8(NULL, old_db,
				origin_type_node, 0,
				NULL, &origin_type);
			bail_error(err);

			if (origin_type == osvr_http_server_map) {
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("host-origin-map is associated already, Blocking more than one\n"));
			    bail_error(err);
			}

			if (origin_type == osvr_http_node_map) {
			    if (ts_equal_str(t_format_type, "1", true)) {
				err = md_commit_set_return_status_msg_fmt(commit, 1,
					_("host-origin-map can't be associated in the current origin[NODE_MAP].\n"));
				bail_error(err);
			    }
			}
		    }
		}
	    }
	}

	/*--------------------------------------------------------
	 * Check that the server-map type corresponds to one
	 * for a NFS origin only.
	 *------------------------------------------------------*/
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
		    "nkn", "nvsd", "namespace", "*", "origin-server", "nfs", "server-map")
		&& (mdct_modify == change->mdc_change_type))
	{
	    const char *t_current_namespace = NULL;

	    /* Get the server-map name */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_server_map);
		bail_error(err);

		/* BZ 2858
		 * For this node, when the value is reset, the node value is set as
		 * "" - an empty string. Hence we need to check if this a set or
		 * a reset operation by doing this the crooked way. The variable
		 * mdc_new_attribs is always non-null in this case (when user
		 * does a reset).
		 */
		if (ts_num_chars(t_server_map) > 0) {
		    /* user is setting up a server-map */

		    smap_fmt_type_node = smprintf("/nkn/nvsd/server-map/config/%s/format-type",
			    ts_str(t_server_map));
		    bail_null(smap_fmt_type_node);

		    /* Get the format type of the server-map */
		    err = mdb_get_node_value_tstr(commit, new_db, smap_fmt_type_node, 0, &found,
			    &t_format_type);
		    bail_error(err);

		    if (!found) {
			/* Bad server-map name */
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error: non-existant server-map : '%s'\n"),
				ts_str(t_server_map));
			bail_error(err);
			goto bail;
		    }
		    else {
			/* Server map name exists. Check nfs-map format type */
			if (!(ts_equal_str(t_format_type, "4", true))) {
			    /* Bad! this is not a valid smap for this origin type */
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("error: format type of server-map '%s' should be nfs-map.\n"),
				    ts_str(t_server_map));
			    bail_error(err);
			    goto bail;
			}
		    }
		}
	    }
	}
	/*--------------------------------------------------------
	 * Check follow header is nothing but "Host"
	 *------------------------------------------------------*/
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
		    "nkn", "nvsd", "namespace", "*", "origin-server", "http", "follow", "header")
		&& (mdct_modify == change->mdc_change_type))
	{
	    /* Get the value */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL,
			bt_string, NULL, &t_header);
		bail_error(err);

		if ( ts_num_chars(t_header) > 0 ) {
		    /* User is setting up a header name */
		    if (!ts_equal_str(t_header, "Host", true)) {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error : Header name must be 'Host'"));
			bail_error(err);
			goto bail;
		    }
		}
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, "nkn", "nvsd", "namespace", "*", "origin-server", "http", "host", "name"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    char *os_type_node = NULL;
	    const char *t_name_space = NULL;
	    uint8_t os_type = 0;

	    t_name_space = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    os_type_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", t_name_space);
	    bail_null(os_type_node);

	    err = mdb_get_node_value_uint8(NULL, old_db,
		    os_type_node, 0,
		    NULL, &os_type);
	    bail_error(err);

	    if ((os_type == osvr_http_server_map) || (os_type == osvr_http_node_map)){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Origin-server server-map is associated with namespace <%s>\n"
			    "Remove the server-map(s) configured and Configure FQDN"), t_name_space);
		bail_error(err);
		goto bail;
	    }
	}
	else if ( ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-request", "read", "interval-timeout"))
		    || (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-request", "read", "retry-delay"))
		    || (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-request", "connect", "timeout"))
		    || (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "origin-request", "connect", "retry-delay")))
		&& (mdct_modify == change->mdc_change_type) )
	{
	    char *os_type_node = NULL;
	    const char *t_name_space = NULL;
	    uint8_t os_type = 0;

	    t_name_space = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    os_type_node = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", t_name_space);
	    bail_null(os_type_node);

	    err = mdb_get_node_value_uint8(NULL, old_db,
		    os_type_node, 0,
		    NULL, &os_type);
	    bail_error(err);

	    if (os_type != osvr_http_node_map){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("This Value can be set Only for node type server-map(origin-server)"));
		bail_error(err);
		goto bail;
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "namespace", "*", "client-request", "dynamic-uri", "tokenize-string"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    int32 status = 0;
	    tstring *ret_output = NULL;
	    tstring *t_tokenize_str = NULL;

	    err = mdb_get_node_value_tstr(commit, new_db,
		    ts_str(change->mdc_name), 0, &found,
		    &t_tokenize_str);
	    bail_error(err);

	    if((t_tokenize_str) && (ts_length(t_tokenize_str) > 0))
	    {
		err = lc_launch_quick_status(&status, &ret_output, false, 3, "/opt/nkn/bin/nv_validator", "-d", ts_str(t_tokenize_str));
		bail_error(err);

		if (WEXITSTATUS(status) != 0)
		{
		    lc_log_basic(LOG_NOTICE, "failed to launch the token validator: status %d",  WEXITSTATUS(status));
		}
		if((ret_output) && (ts_length(ret_output) > 0))
		{
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("%s"), ts_str(ret_output));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 9,
			"nkn", "nvsd", "namespace", "*", "origin-server", "http", "server-map", "*", "map-name"))
		&& (mdct_delete == change->mdc_change_type))
	{
	    const char *t_ns_name = NULL;
	    node_name_t ns_smap_nd = {0};
	    tbool ret_active = false;

	    t_ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    if(t_ns_name != NULL){


		snprintf(ns_smap_nd, sizeof(ns_smap_nd), "/nkn/nvsd/namespace/%s/origin-server/http/server-map/*/map-name",
			t_ns_name);

		err = mdb_get_matching_tstr_array(commit,
			new_db,
			ns_smap_nd,
			0,
			&t_smaps_http);
		bail_error(err);

		if (t_smaps_http != NULL) {

		    int asso_count = tstr_array_length_quick(t_smaps_http);

		    if (asso_count == 0) {
			/* When this is the last server-map aasociated with namespace
			 * and namespace is active
			 * dis-allow
			 */

			err = md_nvsd_namespace_is_active(t_ns_name,
				new_db, &ret_active);
			bail_error(err);

			if(ret_active) {
			    err = md_commit_set_return_status_msg_fmt(
				    commit, 2,
				    _("error: Cannot remove last server-map when namespace '%s' is active."
					"Please inactivate the namespace and try again."),
				    t_ns_name);
			}
		    }
		}
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 5, "nkn", "nvsd", "namespace", "*","ns-precedence")
		&& (change->mdc_change_type != mdct_delete)) {
	    uint32_t precedence = 0;
	    const char *ns = NULL;
	    /* ns-precedence must not be zero */
	    /* ns-precedence must not exceed 65535 */
	    err = mdb_get_node_value_uint32(commit, new_db,
		    ts_str(change->mdc_name), 0, NULL, &precedence);
	    bail_error(err);

	    if (precedence <= 10 || precedence > 65535) {
		if ((precedence <= 10) && (precedence > 0 )) {
		    /* if ns == mfc_probe, precedence <= 10 is allowed */
		    ns = tstr_array_get_str_quick(change->mdc_name_parts, 3);
		    bail_null(ns);
		    if (0 == strcmp(ns, "mfc_probe")) {
			/* do nothing */
		    } else {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				"precedence between 1 to 10 is reserved");
			bail_error(err);
			goto bail;
		    }
		} else {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "precedence must be between 11 and 65535");
		    bail_error(err);
		    goto bail;
		}
	    }
	    /* ns-precedence must be unique */
	    check_unique_precedence = 1;
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 6, "nkn", "nvsd", "namespace", "*","client-request", "filter-map")
		&& (change->mdc_change_type != mdct_delete)) {
	    /* check if filter-map is already associated with other namespace */
	    int attached = 0, exists = 0;

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);

	    ts_free(&t_filter_map);
	    err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_filter_map);
	    bail_error(err);

	    if (strcmp(ts_str(t_filter_map), "")) {

		err = md_check_if_node_exists(commit, old_db, new_db, ts_str(t_filter_map),
			"/nkn/nvsd/url-filter/config/id", &exists);
		bail_error(err);

		if (exists == 0 ) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "filter-map %s doesn't exits", ts_str(t_filter_map));
		    bail_error(err);
		}
		err = md_check_if_node_attached(commit, old_db, new_db, change, ts_str(t_filter_map),
			"/nkn/nvsd/namespace/*/client-request/filter-map", &attached);
		bail_error(err);
		if (attached) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "filter-map %s already attached to another namepsace",
			    ts_str(t_filter_map));
		    bail_error(err);
		    goto bail;
		}
	    }
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 7, "nkn", "nvsd", "namespace", "*","client-request", "uf", "200-msg")
		|| bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 7, "nkn", "nvsd", "namespace", "*","client-request", "uf", "3xx-fqdn")
		|| bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 6, "nkn", "nvsd", "namespace", "*","client-request", "filter-action"))
		&& (change->mdc_change_type != mdct_delete)) {

	    const char *t_ns_name = NULL;
	    t_ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    snprintf(uf_node, sizeof(uf_node), "/nkn/nvsd/namespace"
		    "/%s/client-request/filter-action", t_ns_name);

	    ts_free(&t_filter_action);
	    err = mdb_get_node_value_tstr(commit, new_db,
		    uf_node, 0, NULL, &t_filter_action);
	    bail_error(err);

	    if (ts_equal_str(t_filter_action, "200", true)) {
		/* check uf/200-msg is non-empty */
		snprintf(uf_node, sizeof(uf_node), "/nkn/nvsd/namespace"
			"/%s/client-request/uf/200-msg", t_ns_name);

		ts_free(&t_200_msg);
		err = mdb_get_node_value_tstr(commit, new_db,
			uf_node, 0, NULL, &t_200_msg);
		bail_error(err);

		if (ts_equal_str(t_200_msg, "", true)){
		    /* commit failed */
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "For 200-msg action, message cannot be empty");
		    bail_error(err);
		    goto bail;
		}
	    } else if (ts_equal_str(t_filter_action, "301", true)
		    || ts_equal_str(t_filter_action, "302", true)) {
		/* check uf/200-msg is non-empty */
		snprintf(uf_node, sizeof(uf_node), "/nkn/nvsd/namespace"
			"/%s/client-request/uf/3xx-fqdn", t_ns_name);

		ts_free(&t_3xx_fqdn);
		err = mdb_get_node_value_tstr(commit, new_db,
			uf_node, 0, NULL, &t_3xx_fqdn);
		bail_error(err);

		if (ts_equal_str(t_3xx_fqdn, "", true)){
		    /* commit failed */
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "For %s action, FQDN cannot be empty",
			    ts_str(t_filter_action));
		    bail_error(err);
		    goto bail;
		} else {
		    /* validate FQDN and URI */
		    if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
				4, 3, "client-request", "uf", "3xx-fqdn")) {
			err = md_rfc952_domain_validate(commit, old_db, new_db,NULL,
				NULL, NULL, change->mdc_change_type, NULL,
				change->mdc_new_attribs, NULL);
			bail_error(err);
		    }
		}
	    }
	}

	/*--------------------------------------------------------
	 * Check  if NFS Host is setup properly
	 *  - This check is done in the node-specific commit_check
	 *    function itself.
	 *  - See the node definition (above) for how this is done.
	 *------------------------------------------------------*/

	/* Since we are looping on the number of changes,
	 * we should free whatever got allocated here..
	 * lest we leak!
	 */
	ts_free(&msg);
	ts_free(&t_server_map);
	ts_free(&t_format_type);
	ts_free(&dom_regex_value);
	bn_request_msg_free(&req);
	safe_free(smap_fmt_type_node);
	ts_free(&tuid_string);
	ts_free(&t_active_namespace);
	ts_free(&t_cache_inherit_name);
	ts_free(&t_cache_inherit_set_name);
	tstr_array_free(&t_active_namespaces);
	ts_free(&proxy_mode);
	ts_free(&t_virtual_player);
	tstr_array_free(&t_name_parts);
	safe_free(node_name);
	ts_free(&t_header);
	tstr_array_free(&t_smaps_http);
    }

    err = md_check_ns_smap(commit, new_db, change_list);
    bail_error(err);

    if (check_unique_precedence) {
	int unique = 0;
	err = md_check_ns_precedence(commit, new_db, &unique);
	bail_error(err);

	if (unique == 0) {
	    err = md_commit_set_return_status_msg_fmt(commit, 1,
		    "namespace precedence is not unique");
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_filter_map);
    ts_free(&msg);
    ts_free(&t_server_map);
    ts_free(&t_format_type);
    ts_free(&dom_regex_value);
    bn_request_msg_free(&req);
    safe_free(smap_fmt_type_node);
    ts_free(&tuid_string);
    ts_free(&t_active_namespace);
    ts_free(&t_cache_inherit_name);
    ts_free(&t_cache_inherit_set_name);
    tstr_array_free(&t_active_namespaces);
    ts_free(&proxy_mode);
    ts_free(&t_virtual_player);
    tstr_array_free(&t_name_parts);
    safe_free(node_name);
    ts_free(&t_header);
    tstr_array_free(&t_smaps_http);
    ts_free(&t_filter_action);
    ts_free(&t_3xx_fqdn);
    ts_free(&t_200_msg);
    return err;
}

static int
md_nvsd_ns_has_origin_configured( md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg,
		tbool *origin_configed)
{
    int err = 0;
    const char *t_namespace = NULL;
    char *node_name = NULL;
    tstr_array *uris_config = NULL;
    int num_uris = 0, i = 0;
    char *origin_server = NULL;
    const char *t_uri = NULL;
    char *t_uri_esc = NULL;
    tstr_array *ta_http_hosts = NULL;
    tstring *t_nfs_host = NULL;
    tstring *t_smap_host = NULL;

    bail_require(origin_configed); // return val
    bail_require(arg); // This is where the namespace name is passed.

    t_namespace = (const char *) arg;

    *origin_configed = false;

    /* Get the URI-prefix, if it already has an origin server set
     * reject the commit
     */
    node_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix", t_namespace);
    bail_null(node_name);

    err = mdb_get_tstr_array(commit, new_db,
	    node_name, NULL, false,
	    0, &uris_config);
    bail_error_null(err, uris_config);

    num_uris = tstr_array_length_quick(uris_config);
    for( i = 0; i < num_uris; ++i) {
	t_uri = tstr_array_get_str_quick(uris_config, i);
	bail_null(t_uri);

	err = bn_binding_name_escape_str(t_uri, &t_uri_esc);
	bail_error(err);

	/* Check if http origin-server is set for this URI */
	origin_server = smprintf(
		"/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/http/host",
		t_namespace, t_uri_esc);
	bail_null(origin_server);

	/* grab the list of host origins.. if at least one.. then fail */
	err = mdb_get_tstr_array(commit, new_db,
		origin_server, NULL, false,
		0, &ta_http_hosts);
	bail_error(err);

	if (ta_http_hosts && tstr_array_length_quick(ta_http_hosts) > 0) {
	    /* HTTP origin configured!!
	     * Bail!
	     */
	    *origin_configed = true;
	    goto bail;
	}
	else {
	    /* Cleanup code */
	    safe_free(origin_server);
	}

	/* No HTTP origin, perhaps NFS origin configed */
	origin_server = smprintf( "/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/host",
		t_namespace, t_uri_esc);
	bail_null(origin_server);
	err = mdb_get_node_value_tstr(commit, new_db,
		origin_server, 0, NULL, &t_nfs_host);
	bail_error(err);

	if (t_nfs_host && ts_length(t_nfs_host) > 0) {
	    /* NFS host configured!!
	     * bail!
	     */
	    *origin_configed = true;
	    goto bail;
	}
	else {
	    /* cleanup code */
	    safe_free(origin_server);
	}

	/* No NFS origin configured either, perhaps a server-map?
	 */
	origin_server = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/server-map",
		t_namespace, t_uri_esc);
	bail_null(origin_server);
	err = mdb_get_node_value_tstr(commit, new_db,
		origin_server, 0, NULL, &t_smap_host);
	bail_error(err);

	if (t_smap_host && ts_length(t_smap_host) > 0) {
	    /* Server map configured!!
	     * bail!
	     */
	    *origin_configed = true;
	    goto bail;

	}
	else {
	    /* cleanup code */
	    safe_free(origin_server);
	}

	safe_free(t_uri_esc);


    }


bail:
    safe_free(t_uri_esc);
    safe_free(node_name);
    safe_free(origin_server);
    tstr_array_free(&uris_config);
    tstr_array_free(&ta_http_hosts);
    ts_free(&t_nfs_host);
    ts_free(&t_smap_host);
    return err;
}

static int
md_nvsd_namespace_validate_name(const char *name,
			tstring **ret_msg)
{
    int err = 0;
    int i = 0;
    const char *p = "/\\*:|`\"?";
    int j = 0;
    int k = 0;
    int l = strlen(p);

    if (strlen(name) == 0) {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad namespace name.");
	    err = lc_err_not_found;
	    goto bail;
	}
    }

    if (strcmp(name, "list") == 0) {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad namespace name. Use of reserved keyword 'list' is not allowed");
	}
	err = lc_err_not_found;
	goto bail;

    }

    if (name[0] == '_') {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad namespace name. "
		    "Name cannot start with a leading underscore ('_')");
	}
	err = lc_err_not_found;
	goto bail;
    }
    k = strlen(name);

    for (i = 0; i < k; i++) {
	for (j = 0; j < l; j++) {
	    if (p[j] == name[i]) {
		if (ret_msg)
		    err = ts_new_sprintf(ret_msg,
			    "Bad namespace name. "
			    "Name cannot contain the characters '%s'", p);
		err = lc_err_not_found;
		goto bail;
	    }
	}
    }

bail:
    return err;
}


static int delete_temp_files(const char *ns_uid)
{
    int err = 0;
    int32 status = 0;
    tstring *ret_output = NULL;

    if (ns_uid && (strlen(ns_uid) > 0)){
	err = lc_launch_quick_status(&status, &ret_output, false, 2, "/opt/nkn/bin/mgmt_namespace_cleanup.sh", ns_uid);
	bail_error(err);
	if (status)
	{
	    lc_log_basic(LOG_NOTICE, "failed to cleanup the tmp files for namespace uid %s status:%d", ns_uid, WEXITSTATUS(status));
	}
    }
bail:
    ts_free(&ret_output);
    return err;	
}

static int
md_nvsd_ns_upgrade_delivery_proto(
		mdb_db	*inout_new_db,
		const char *namespace,
		const char *uri_prefix,
		const char *proto)
{
    int err = 0;
    tbool http_enabled = false;
    char *bn_name = NULL;
    char *bn_proto = NULL;
    tbool found = false;
    uint32 i = 0;

    bn_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/delivery/proto/%s",
	    namespace, uri_prefix, proto);
    bail_null(bn_name);

    err = mdb_get_node_value_tbool(NULL, inout_new_db, bn_name, 0, &found, &http_enabled);
    bail_error(err);

    if (found &&
	    !((strcmp(proto, "rtmp") == 0) /* Skip rtmp/rtp nodes - no longer supported */
		|| (strcmp(proto, "rtp") == 0))  ) {
	lc_log_basic(LOG_INFO,
		"UG : moving '/nkn/nvsd/uri/config/%s/uri-prefix/%s/delivery/proto/%s' "
		"to '/nkn/nvsd/namespace/%s/delivery/proto/%s",
		namespace, uri_prefix, proto, namespace, proto);

	err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		0, bt_bool,
		lc_bool_to_str(http_enabled),
		"/nkn/nvsd/namespace/%s/delivery/proto/%s",
		namespace, proto);
	bail_error(err);
    }

    /*---------------------------------------------------------
     *  Delete ../delivery/proto/.. nodes here
     *-------------------------------------------------------*/
    bn_proto = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/delivery/proto/%s",
	    namespace, uri_prefix, proto);
    bail_null(bn_proto);

    lc_log_basic(LOG_INFO, "UG : deleting ('%s')",
	    bn_proto);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
	    0, bt_string, "", "%s", bn_proto);
    bail_error(err);
    safe_free(bn_proto);
bail:
    safe_free(bn_proto);
    safe_free(bn_name);
    return err;
}

static int
md_nvsd_ns_upgrade_domain_host(
		mdb_db	*inout_new_db,
		const char *bn_name)
{
    int err = 0;
    tstr_array *binding_parts = NULL;
    const char *t_namespace = NULL;
    tstring *host_value = NULL;
    char *binding_name = NULL;
    tbool found = false;

    err = bn_binding_name_to_parts (bn_name, &binding_parts, NULL);
    bail_error(err);

    t_namespace = tstr_array_get_str_quick (binding_parts, 4);

    err = mdb_get_node_value_tstr(
	    NULL, inout_new_db, bn_name, 0, &found, &host_value);
    bail_error(err);

    if (found && (ts_length(host_value) > 0))
    {
	binding_name = smprintf("/nkn/nvsd/namespace/%s/domain/host", t_namespace);
	bail_null(binding_name);

	lc_log_basic(LOG_INFO, "UG : moving '%s' to '%s'",
		bn_name, binding_name);

	err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		0, bt_string,
		ts_str(host_value),
		"%s",
		binding_name);
	bail_error(err);
    }

    lc_log_basic(LOG_INFO, "UG : deleting ('%s')", bn_name);
    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
	    0, bt_string, "", "%s", bn_name);
    bail_error(err);

bail:
    safe_free(binding_name);
    tstr_array_free(&binding_parts);
    return err;
}

static int
md_nvsd_ns_upgrade_domain_regex(
		mdb_db	*inout_new_db,
		const char *bn_name)
{
    int err = 0;
    tstr_array *binding_parts = NULL;
    const char *t_namespace = NULL;
    tstring *host_value = NULL;
    char *binding_name = NULL;
    tbool found = false;

    err = bn_binding_name_to_parts (bn_name, &binding_parts, NULL);
    bail_error(err);

    t_namespace = tstr_array_get_str_quick (binding_parts, 4);

    err = mdb_get_node_value_tstr(NULL, inout_new_db, bn_name,
	    0, &found, &host_value);
    bail_error(err);

    if (found && (ts_length(host_value) > 0))
    {
	binding_name = smprintf("/nkn/nvsd/namespace/%s/domain/regex", t_namespace);
	bail_null(binding_name);

	lc_log_basic(LOG_INFO, "UG : moving '%s' to '%s'",
		bn_name, binding_name);

	err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		0, bt_regex,
		ts_str(host_value),
		"%s",
		binding_name);
	bail_error(err);
    }


    lc_log_basic(LOG_INFO, "UG : deleting ('%s')", bn_name);
    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
	    0, bt_regex, "", "%s", bn_name);
    bail_error(err);

bail:
    safe_free(binding_name);
    tstr_array_free(&binding_parts);
    return err;
}

static int md_nvsd_namespace_current_origin_server_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		origin_server_type_t *ret_type)
{
    int err = 0;
    char *bn_name = NULL;
    origin_server_type_t type = osvr_none;

    bail_null(ret_type);
    bail_require(namespace);

    bn_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", namespace);
    bail_null(bn_name);

    err = mdb_get_node_value_uint8(commit, db, bn_name, 0, NULL, (uint8_t *) &type);
    bail_error(err);

    *ret_type = type;
    lc_log_basic(LOG_INFO, "cuurent match type for namespace '%s' : %d",
	    namespace, type);

bail:
    safe_free(bn_name);
    return err;
}

static int md_nvsd_namespace_current_match_criteria_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		match_type_t	*ret_match_type)
{
    int err = 0;
    char *bn_name = NULL;
    match_type_t match_type = mt_none;

    bail_null(ret_match_type);
    bail_null(namespace);

    *ret_match_type = match_type;

    /* Find out which match type is set for this namespace.
     */
    bn_name = smprintf("/nkn/nvsd/namespace/%s/match/type", namespace);
    bail_null(bn_name);

    err = mdb_get_node_value_uint8(commit, db, bn_name, 0, NULL, (uint8_t *) &match_type);
    bail_error(err);

    *ret_match_type = match_type;
    lc_log_basic(LOG_INFO, "curent match type for namespace '%s' : %d",
	    namespace, match_type);

bail:
    safe_free(bn_name);
    return err;

}

static int md_nvsd_namespace_rtsp_current_match_criteria_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		match_type_t	*ret_match_type)
{
    int err = 0;
    char *bn_name = NULL;
    match_type_t match_type = mt_none;

    bail_null(ret_match_type);
    bail_null(namespace);

    *ret_match_type = match_type;

    /* Find out which match type is set for this namespace.
     */
    bn_name = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/type", namespace);
    bail_null(bn_name);

    err = mdb_get_node_value_uint8(commit, db, bn_name, 0, NULL, (uint8_t *) &match_type);
    bail_error(err);

    *ret_match_type = match_type;
    lc_log_basic(LOG_INFO, "curent RTSP match type for namespace '%s' : %d",
	    namespace, match_type);

bail:
    safe_free(bn_name);
    return err;

}


static int md_nvsd_namespace_origin_server_set(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		origin_server_data_t *ot)
{
    int err = 0;
    origin_server_type_t old_type = osvr_none;

    bail_require(ot);
    bail_null(t_namespace);

    lc_log_basic(LOG_DEBUG, "Setting origin server type to : %d",
	    ot->type);



    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
	    0, bt_uint8,
	    osvr_types[ot->type],
	    "/nkn/nvsd/namespace/%s/origin-server/type",
	    t_namespace);
    bail_error(err);

    if (old_db == NULL)
	goto bail;

    /* Get the old origin type */
    err = md_nvsd_namespace_current_origin_server_get(NULL, old_db,
	    t_namespace, &old_type);
    bail_error(err);

    if (old_type == ot->type)
	goto bail;

    switch(old_type)
    {
	case osvr_none:
	    break;
	case osvr_http_host:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/origin-server/http/host/name",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_uint16,
		    "80",
		    "/nkn/nvsd/namespace/%s/origin-server/http/host/port",
		    t_namespace);
	    bail_error(err);
	    break;
	case osvr_http_server_map:
	    /*
	       err = mdb_set_node_str(commit, inout_new_db,
	       bsso_reset, 0, bt_string,
	       "",
	       "/nkn/nvsd/namespace/%s/origin-server/http/server-map/1/map-name",
	       t_namespace);
	       bail_error(err);
	     */
	    break;
	case osvr_http_follow_header:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/origin-server/http/follow/header",
		    t_namespace);
	    bail_error(err);
	    break;

	case osvr_http_dest_ip:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_bool,
		    "false",
		    "/nkn/nvsd/namespace/%s/origin-server/http/follow/dest-ip",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_bool,
		    "false",
		    "/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_ipv4addr,
		    "0.0.0.0",
		    "/nkn/nvsd/namespace/%s/ip_tproxy",
		    t_namespace);
	    bail_error(err);
	    break;

	case osvr_http_abs_url:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_bool,
		    "false",
		    "/nkn/nvsd/namespace/%s/origin-server/http/absolute-url",
		    t_namespace);
	    bail_error(err);
	    break;

	case osvr_nfs_host:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/origin-server/nfs/host",
		    t_namespace);
	    bail_error(err);
	    break;
	case osvr_nfs_server_map:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/origin-server/nfs/server-map",
		    t_namespace);
	    bail_error(err);
	    break;
	case osvr_http_node_map:
	    /* TBD:
	       err = mdb_set_node_str(commit, inout_new_db,
	       bsso_reset, 0, bt_string,
	       "",
	       "/nkn/nvsd/namespace/%s/origin-server/http/server-map/",
	       t_namespace);
	       bail_error(err);
	     */
	    break;

	case osvr_rtsp_host:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/origin-server/rtsp/host/name",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_uint16,
		    "554",
		    "/nkn/nvsd/namespace/%s/origin-server/rtsp/host/port",
		    t_namespace);
	    bail_error(err);
	    break;
	case osvr_rtsp_dest_ip:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_bool,
		    "false",
		    "/nkn/nvsd/namespace/%s/origin-server/rtsp/follow/dest-ip",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_bool,
		    "false",
		    "/nkn/nvsd/namespace/%s/origin-server/rtsp/follow/use-client-ip",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_ipv4addr,
		    "0.0.0.0",
		    "/nkn/nvsd/namespace/%s/ip_tproxy",
		    t_namespace);
	    bail_error(err);
	    break;
	default:
	    break;
    }
bail:
    return err;
}


static int md_nvsd_namespace_http_match_criteria_set(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		match_type_data_t *mt)
{
    int err = 0;
    match_type_t old_type = mt_none;
    bail_require(mt);
    bail_null(t_namespace);

    lc_log_basic(LOG_DEBUG, "Setting match type for namespace '%s' as : '%d'",
	    t_namespace, (uint8_t) mt->match_type);

    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
	    0, bt_uint8,
	    match_types[mt->match_type],
	    "/nkn/nvsd/namespace/%s/match/type",
	    t_namespace);
    bail_error(err);
    /* Resetting other match-criteria set BUG# 3922*/
    if (old_db == NULL)
	goto bail;

    /* Get the old match type */
    err = md_nvsd_namespace_current_match_criteria_get(NULL, old_db,
	    t_namespace, &old_type);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "Got old match type as '%d'", old_type);

    if (old_type == mt->match_type)
	goto bail;

    switch(old_type)
    {
	case mt_none:
	    break;
	case mt_uri_name:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/match/uri/uri_name",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_uri_regex:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/match/uri/regex",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_header_name:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/match/header/name",
		    t_namespace);
	    bail_error(err);
	    break;

	case mt_header_regex:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/match/header/regex",
		    t_namespace);
	    bail_error(err);
	    break;

	case mt_query_string_name:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/match/query-string/name",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_query_string_regex:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_string,
		    "",
		    "/nkn/nvsd/namespace/%s/match/query-string/regex",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_vhost:
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_ipv4addr,
		    "255.255.255.255",
		    "/nkn/nvsd/namespace/%s/match/virtual-host/ip",
		    t_namespace);
	    bail_error(err);
	    err = mdb_set_node_str(commit, inout_new_db,
		    bsso_reset, 0, bt_uint16,
		    "80",
		    "/nkn/nvsd/namespace/%s/match/virtual-host/port",
		    t_namespace);
	    bail_error(err);
	    break;
	default:
	    break;
    }
bail:
    return err;
}

static int md_nvsd_namespace_is_active(const char *namespace, const mdb_db *db, tbool *ret_active)
{
    int err = 0;
    char *bn_name = NULL;
    tbool is_active = false;

    bail_null(namespace);
    bail_null(ret_active);

    bn_name = smprintf("/nkn/nvsd/namespace/%s/status/active", namespace);
    bail_null(bn_name);

    err = mdb_get_node_value_tbool(NULL, db, bn_name, 0, NULL, &is_active);
    bail_error(err);

    *ret_active = is_active;
bail:
    safe_free(bn_name);
    return err;
}

static int
md_nvsd_namespace_ug_nfs_6_to_7(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg)
{
    int err = 0;
    uint32 i = 0;
    tstr_array *nfs_host_bindings = NULL;
    tstr_array *nfs_server_map_bindings = NULL;
    tstr_array *smap_binings = NULL;
    tstring *nfs_host = NULL;
    tstring *nfs_smap = NULL;
    tstr_array *binding_parts = NULL;
    const char *t_namespace = NULL;
    tbool found = false;
    const char *t_uri = NULL;
    char *uri_escaped = NULL;


    err = mdb_get_matching_tstr_array(NULL,
	    inout_new_db,
	    "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/host",
	    0,
	    &nfs_host_bindings);
    bail_error(err);

    for (i = 0; i < tstr_array_length_quick(nfs_host_bindings); i++)
    {
	err = bn_binding_name_to_parts(
		tstr_array_get_str_quick(nfs_host_bindings, i),
		&binding_parts, NULL);
	bail_error(err);

	t_namespace = tstr_array_get_str_quick(binding_parts, 4);

	err = mdb_get_node_value_tstr(NULL, inout_new_db,
		tstr_array_get_str_quick(nfs_host_bindings, i),
		0, &found,
		&nfs_host);
	bail_error(err);

	t_uri = tstr_array_get_str_quick(binding_parts, 6);
	err = bn_binding_name_escape_str(t_uri, &uri_escaped);
	bail_error(err);

	if (found && (ts_length(nfs_host) > 0)) {
	    /* Move only nodes that are set */
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_string,
		    ts_str(nfs_host),
		    "/nkn/nvsd/namespace/%s/origin-server/nfs/host",
		    t_namespace);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_uint8,
		    osvr_types[osvr_nfs_host],
		    "/nkn/nvsd/namespace/%s/origin-server/type",
		    t_namespace);
	    bail_error(err);
	}

	/* Irrespective of whether the NFS origin is set or not,
	 * delete the nodes !
	 */
	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		0, bt_string,
		"",
		"/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/host",
		t_namespace, uri_escaped);
	bail_error(err);

	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		0, bt_uint16,
		"2049",
		"/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/port",
		t_namespace, uri_escaped);
	bail_error(err);
	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		0, bt_bool,
		"true",
		"/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/local-cache",
		t_namespace, uri_escaped);
	bail_error(err);
	safe_free(uri_escaped);
	ts_free(&nfs_host);
	tstr_array_free(&binding_parts);
    }

    /*------------------- Move server maps ------------------------*/
    err = mdb_get_matching_tstr_array(NULL,
	    inout_new_db,
	    "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/server-map",
	    0,
	    &nfs_server_map_bindings);
    bail_error(err);

    for(i = 0; i < tstr_array_length_quick(nfs_server_map_bindings); i++)
    {
	err = bn_binding_name_to_parts(
		tstr_array_get_str_quick(nfs_server_map_bindings, i),
		&binding_parts, NULL);
	bail_error(err);

	t_namespace = tstr_array_get_str_quick(binding_parts, 4);

	err = mdb_get_node_value_tstr(NULL, inout_new_db,
		tstr_array_get_str_quick(nfs_server_map_bindings, i),
		0, &found,
		&nfs_smap);
	bail_error(err);

	if (found && (ts_length(nfs_smap) > 0))
	{
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_string,
		    ts_str(nfs_smap),
		    "/nkn/nvsd/namespace/%s/origin-server/nfs/server-map",
		    t_namespace);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_uint8,
		    osvr_types[osvr_nfs_server_map],
		    "/nkn/nvsd/namespace/%s/origin-server/type",
		    t_namespace);
	    bail_error(err);


	    ts_free(&nfs_smap);
	    tstr_array_free(&binding_parts);
	}

	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		0, bt_string,
		"",
		"%s",
		tstr_array_get_str_quick(nfs_server_map_bindings, i));
	bail_error(err);
    }


    /*------------------------ delete redundant nodes --------------------*/
    err = mdb_get_matching_tstr_array(NULL,
	    inout_new_db,
	    "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/server-map",
	    0,
	    &smap_binings);
    bail_error(err);

    for (i = 0; i < tstr_array_length_quick(smap_binings); i++)
    {
	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		0, bt_string,
		"",
		"%s",
		tstr_array_get_str_quick(smap_binings, i));
	bail_error(err);
    }

bail:
    safe_free(uri_escaped);
    ts_free(&nfs_smap);
    ts_free(&nfs_host);
    tstr_array_free(&binding_parts);
    tstr_array_free(&nfs_host_bindings);
    tstr_array_free(&nfs_server_map_bindings);
    tstr_array_free(&smap_binings);
    return err;
}

int md_nvsd_uri_name_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg)
{
    int err = 0;
    const bn_attrib *new_value = NULL;
    const char *c_uri_name = NULL;
    tstring *t_uri_prefix = NULL;

    if(!new_attribs) {
	goto bail;
    }
    new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (new_value == NULL) {
	goto bail;
    }
    err = bn_attrib_get_tstr(new_value, NULL, bt_uri, NULL, &t_uri_prefix);
    bail_error(err);

    c_uri_name = ts_str(t_uri_prefix);
    if ((strlen(c_uri_name) > 0)
	    && (c_uri_name[0] != '/')) {
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error: uri-prefix names must start with a '/'"));
	bail_error(err);
	goto bail;
    }
bail:
    ts_free(&t_uri_prefix);
    return err;
}



int md_nvsd_uri_nfs_host_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg)
{
    int err = 0;
    const bn_attrib *new_value = NULL;
    tstring *t_host = NULL;
    int32 at_offset = 0;

    if(!new_attribs) {
	goto bail;
    }
    new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (new_value == NULL) {
	goto bail;
    }

    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_host);
    bail_error(err);

    if (!ts_length(t_host)) {
	goto bail;
    }

    err = ts_find_str(t_host, 0, ":", &at_offset);
    if (err != 0) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1, "error validating NFS Origin name: \"%s\"", ts_str(t_host));
	bail_error(err);
    }
    else if ( at_offset < 0 ) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1,
		"bad NFS export specification: \"%s\" (Should be %c  hostname:export_path %c)",
		ts_str(t_host), '<', '>');
	bail_error(err);
    } else if (at_offset >= (ts_length(t_host)-1)) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1,
		"bad NFS export specification: \"%s\" (Should be %c hostname:export_path %c)",
		ts_str(t_host), '<', '>');
	bail_error(err);
    }

bail:
    ts_free(&t_host);
    return err;
}


static int
md_nvsd_namespace_validate_on_active(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        const char *namespace,
	tbool *invalid)
{
    int err = 0;
    char *node_name = NULL;
    match_type_t mt = mt_none;
    origin_server_type_t ot = osvr_none;

    /* Check if match and origin-server is configured.
     * If not, fail
     */
    node_name = smprintf("/nkn/nvsd/namespace/%s/match/type", namespace);
    bail_null(node_name);

    err = mdb_get_node_value_uint8(NULL, new_db,
	    node_name, 0,
	    NULL, (uint8_t *) &mt);
    bail_error(err);

    safe_free(node_name);

    node_name = smprintf("/nkn/nvsd/namespace/%s/origin-server/type", namespace);
    bail_null(node_name);

    err = mdb_get_node_value_uint8(NULL, new_db,
	    node_name, 0,
	    NULL, (uint8_t *) &ot);
    bail_error(err);

    if ((mt_none == mt)) {
	*invalid = true;
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error : At least one match type must be set "
		    "for namespace '%s'. Set this before activating"
		    " the namespace"),
		namespace);
	bail_error(err);
	goto bail;
    }

    if (mt_none == ot) {
	*invalid = true;
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error : origin-server not set for "
		    "namespace '%s'. Set this before activating "
		    "the namespace"),
		namespace);
	bail_error(err);
	goto bail;
    }
    *invalid = false;
bail:
    safe_free(node_name);
    return err;
}

static int md_nvsd_namespace_proxy_mode_set(md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		const char *mode)
{
    int err = 0;

    bail_null(mode);
    bail_null(t_namespace);

    lc_log_basic(LOG_DEBUG, "Setting proxy-mode for namespace '%s' as : '%s'",
	    t_namespace, mode);

    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
	    0, bt_string,
	    mode,
	    "/nkn/nvsd/namespace/%s/proxy-mode",
	    t_namespace);
    bail_error(err);

bail:
    return err;
}


static tbool
md_nvsd_namespace_check_if_default(const bn_attrib_array *new_attribs,
		bn_type bt_type,
		const char *default_value)
{
    int err = 0;
    const bn_attrib *new_val = NULL;
    tstring *value = NULL;

    if (new_attribs == NULL) {
	/* This is a node delete.. dont do anything
	 */
	goto bail_true;
    }

    new_val = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (!new_val)
	goto bail_true;

    err = bn_attrib_get_tstr(new_val, NULL, bt_type, NULL, &value);
    if (err)
	goto bail_true;

    if (ts_equal_str(value, default_value, false))
	goto bail_true;
    else
	goto bail_false;



bail_true:
    ts_free(&value);
    return true;
bail_false:
    ts_free(&value);
    return false;
}

int
md_nvsd_ns_http_host_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg)
{
    int err = 0;
    const bn_attrib *new_value = NULL;
    tstring *t_host = NULL;
    int32 ret_off = -1;

    if ( change_type != mdct_delete) {
	/* get the value */
	new_value = bn_attrib_array_get_quick(new_attribs, ba_value);

	if (new_value) {
	    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_host);
	    bail_error(err);

	    if (err != 0) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error valdating this hostname: \"%s\""),
			ts_str(t_host));
		bail_error(err);
	    }
	    else if (ret_off != -1) {
		/* Some one gave a value as <host>:<> - Reject this
		 */
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: bad host name - '%s'"),
			ts_str(t_host));
		bail_error(err);
		goto bail;
	    } else {
		err = md_rfc952_domain_validate(commit,
			old_db, new_db, change_list,
			node_name, node_name_parts,
			change_type, old_attribs,
			new_attribs, arg);
		bail_error(err);
	    }
	}
    }

bail:
    ts_free(&t_host);
    return err;
}


static int
validate_domain_regex_expression(md_commit *commit,
					const char *current_domain,
					const char *in_hand_domain,
					tbool *invalid)
{

    int err = 0;
    int match = 0;	
    regex_t current_domain_regex;
    regex_t in_hand_domain_regex;
    char *current_domain_escape = NULL;
    char *in_hand_domain_escape = NULL;

    if (current_domain == NULL)
    {
	current_domain = "*";
	err = bn_binding_name_escape_str(".*", &current_domain_escape);
	bail_error(err);
    } else if (strcmp(current_domain,"*") == 0)
    {
	err = bn_binding_name_escape_str(".*", &current_domain_escape);
	bail_error(err);
    } else
    {
	err = bn_binding_name_escape_str(current_domain, &current_domain_escape);
	bail_error(err);
    }

    if (strcmp(in_hand_domain,"*") == 0)
    {
	err = bn_binding_name_escape_str(".*", &in_hand_domain_escape);
	bail_error(err);

    } else {
	err = bn_binding_name_escape_str(in_hand_domain, &in_hand_domain_escape);
	bail_error(err);
    }

    if (regcomp (&current_domain_regex, current_domain_escape, REG_EXTENDED|REG_NOSUB|REG_NEWLINE) != 0)
    {
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error: regex compilation failure for '%s'\n"),
		current_domain);
	bail_error(err);
	goto bail;
    }
    if (regcomp (&in_hand_domain_regex, in_hand_domain_escape, REG_EXTENDED|REG_NOSUB|REG_NEWLINE) != 0)
    {
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error: regex compilation failure for '%s'\n"),
		in_hand_domain);
	bail_error(err);
	goto bail;
    }

    /*! case 1*/
    match = (regexec(&in_hand_domain_regex, current_domain_escape, (size_t)0, NULL, 0) == 0);
    if (match)
    {
	*invalid = true;
	goto bail;
    }


    /*! case 2*/
    match = (regexec(&current_domain_regex, in_hand_domain_escape, (size_t)0, NULL, 0) == 0);
    if (match)
    {
	*invalid = true;
	goto bail;
    }

    *invalid = false;
bail:
    regfree(&current_domain_regex);
    regfree(&in_hand_domain_regex);	
    safe_free(current_domain_escape);
    safe_free(in_hand_domain_escape);
    return err;
}	


static int
validate_on_regex_expression(md_commit *commit,
			const char *current_value,
			const char *in_hand_value,
			tbool *invalid)
{

    int err = 0;
    int match = 0;	
    regex_t current_regex;
    regex_t in_hand_regex;

    if (regcomp (&current_regex, current_value, REG_EXTENDED|REG_NOSUB|REG_NEWLINE) != 0)
    {
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error: regex compilation failure for '%s'\n"),
		current_value);
	bail_error(err);
	goto bail;
    }
    if (regcomp (&in_hand_regex, in_hand_value , REG_EXTENDED|REG_NOSUB|REG_NEWLINE) != 0)
    {
	err = md_commit_set_return_status_msg_fmt(commit, 1,
		_("error: regex compilation failure for '%s'\n"),
		in_hand_value);
	bail_error(err);
	goto bail;
    }

    /*! case 1*/
    if ((regexec(&in_hand_regex, current_value , (size_t)0, NULL, 0) == 0)) {
	*invalid = true;
	goto bail;
    }

    /*! case 2*/
    if ((regexec(&current_regex, in_hand_value, (size_t)0, NULL, 0) == 0)) {
	*invalid = true;
	goto bail;
    }

    *invalid = false;
bail:
    regfree(&current_regex);
    regfree(&in_hand_regex);
    return err;
}

	

static int
validate_on_type(md_commit *commit,
		 const mdb_db *mdb,
                 const char *namespace,
                 const char *current_value,
		 tstring **return_value,
		 match_type_t incoming_type,
                 match_type_t match_type,
		 tbool *invalid)
{

    int err = 0;
    char *node_name = NULL;
    tstring *inhand_value = NULL;
    regex_t current_regex;
    regex_t in_hand_regex;
    char *current_escape = NULL;
    char *in_hand_escape = NULL;


    switch (incoming_type)
    {
	case mt_uri_name:
	case mt_uri_regex:
	    switch(match_type) {
		case mt_uri_name:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/uri/uri_name",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);

		    break;
		case mt_uri_regex:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/uri/regex",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);

		    break;
		case mt_none:
		    *return_value = NULL;
		    *invalid = true;
		    goto bail;
		    break;
		default:
		    *invalid = false;
		    goto bail;
	    }
	    if (ts_length (inhand_value) > 0 &&
		    strlen(current_value) > 0) {
		err = bn_binding_name_escape_str(current_value, &current_escape);
		bail_error(err);
		err = bn_binding_name_escape_str(ts_str(inhand_value), &in_hand_escape);
		bail_error(err);

		err = validate_on_regex_expression(commit,
			current_escape,
			in_hand_escape,
			invalid);
		if (err != 0)
		    ts_free(&inhand_value);
		bail_error(err);
		if (*invalid == true) {
		    *return_value = inhand_value; 	
		} else {
		    ts_free(&inhand_value);
		}	
	    } else {
		*invalid = false;
		ts_free(&inhand_value);
		goto bail;
	    }
	    break;
	case mt_header_name:
	case mt_header_regex:
	    switch(match_type) {
		case mt_header_name:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/header/name",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);

		    break;
		case mt_header_regex:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/header/regex",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);

		    break;
		case mt_none:
		    *return_value = NULL;
		    *invalid = true;
		    goto bail;
		    break;
		default:
		    *invalid = false;
		    goto bail;
	    }
	    if (ts_length(inhand_value) > 0 &&
		    strlen(current_value) > 0) {
		err = validate_on_regex_expression(commit,
			current_value,
			ts_str(inhand_value),
			invalid);
		if (err != 0)
		    ts_free(&inhand_value);
		bail_error(err);
		if (*invalid == true) {
		    *return_value = inhand_value; 	
		} else {
		    ts_free(&inhand_value);
		}
	    } else {
		*invalid = false;	
		ts_free(&inhand_value);
		goto bail;
	    }	
	    break;
	case mt_query_string_name:
	case mt_query_string_regex:
	    switch(match_type) {
		case mt_query_string_name:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/query-string/name",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);

		    break;
		case mt_query_string_regex:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/query-string/regex",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);

		    break;
		case mt_none:
		    *return_value = NULL;
		    *invalid = true;
		    goto bail;
		    break;
		default:
		    *invalid = false;
		    goto bail;
	    }
	    if (ts_length(inhand_value) > 0 &&
		    strlen(current_value) > 0) {
		err = validate_on_regex_expression(commit,
			current_value,
			ts_str(inhand_value),
			invalid);
		if (err != 0)
		    ts_free(&inhand_value);
		bail_error(err);
		if (*invalid == true) {
		    *return_value = inhand_value; 	
		} else {
		    ts_free(&inhand_value);
		}
	    } else {
		*invalid = false;	
		ts_free(&inhand_value);
		goto bail;
	    }	

	    break;
	case mt_vhost:
	    switch(match_type) {
		case mt_vhost:
		    node_name =
			smprintf("/nkn/nvsd/namespace/%s/match/virtual-host/ip",
				namespace);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr
			(commit, mdb, node_name, 0, NULL, &inhand_value);
		    bail_error(err);
		    break;
		case mt_none:
		    *return_value = NULL;
		    *invalid = true;
		    goto bail;
		    break;
		default:
		    *invalid = false;
		    goto bail;
	    }
	    if (ts_length(inhand_value) > 0 &&
		    strlen(current_value) > 0) {
		err = validate_on_regex_expression(commit,
			current_value,
			ts_str(inhand_value),
			invalid);
		if (err != 0)
		    ts_free(&inhand_value);
		bail_error(err);
		if (*invalid == true) {
		    *return_value = inhand_value; 	
		} else {
		    ts_free(&inhand_value);
		}
	    } else {
		*invalid = false;	
		ts_free(&inhand_value);
		goto bail;
	    }	

	    break;
	default:
	    break;	
    }
bail:
    safe_free(current_escape);
    safe_free(in_hand_escape);
    return err;
}


static int
md_get_domain_value( md_commit *commit,
			const mdb_db *new_db,
			const char *ns,
			tstring **current_domain,
			domain_type_t *dom_type)
{

    int err = 0;
    char * node_name = NULL;
    tstring *t_domain = NULL;


    node_name = smprintf("/nkn/nvsd/namespace/%s/domain/regex",
	    ns);
    bail_null(node_name);
    err = mdb_get_node_value_tstr(commit, new_db,
	    node_name, 0 , NULL,
	    &t_domain);
    bail_error(err);
    safe_free(node_name);
    if (t_domain == NULL || ts_length (t_domain) == 0)
    {
	ts_free(&t_domain);
	node_name = smprintf("/nkn/nvsd/namespace/%s/domain/host",
		ns);
	bail_null(node_name);
	err = mdb_get_node_value_tstr(commit, new_db,
		node_name, 0 , NULL,
		current_domain);
	bail_error_null(err,*current_domain);
	safe_free(node_name);
	*dom_type = domain_host;
    }
    else {
	*current_domain = t_domain;
	*dom_type = domain_regex;
    }
bail:
    safe_free(node_name);
    return err;
}
static int
md_get_precendence_value(md_commit *commit,
			 const mdb_db *new_db,
			 const char *ns,
			 uint32 *precedence)
{
    int err = 0;
    char * node_name = NULL;

    node_name = smprintf("/nkn/nvsd/namespace/%s/match/precedence", ns);
    bail_null(node_name);
    err = mdb_get_node_value_uint32(commit, new_db, node_name,
	    0, NULL, precedence);
    bail_error(err);
    safe_free(node_name);

bail:
    safe_free(node_name);
    return err;

}

static int
md_get_type_value(  md_commit *commit,
			const mdb_db *new_db,
			const char *ns,
			match_type_t *type,
			tstring **current_value)
{

    int err = 0;
    char * node_name = NULL;

    node_name = smprintf("/nkn/nvsd/namespace/%s/match/type", ns);
    bail_null(node_name);
    err = mdb_get_node_value_uint8(commit, new_db, node_name,
	    0, NULL, (uint8_t *) type);
    bail_error(err);
    safe_free(node_name);

    switch (*type) {
	case mt_none:
	    goto bail;  // this case should not happen
	case mt_uri_name:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/uri/uri_name",
		    ns);
	    break;
	case mt_uri_regex:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/uri/regex",
		    ns);
	    break;
	case mt_header_name:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/header/name",
		    ns);
	    break;
	case mt_header_regex:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/header/regex",
		    ns);
	    break;
	case mt_query_string_name:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/query-string/name",
		    ns);
	    break;
	case mt_query_string_regex:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/query-string/regex",
		    ns);
	    break;
	case mt_vhost:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/virtual-host/ip",
		    ns);
	    break;
	default:
	    break;
    }
    if (node_name) {
	err = mdb_get_node_value_tstr(commit, new_db,
		node_name, 0 , NULL,
		current_value);
	bail_error_null(err,*current_value);
	safe_free(node_name);
    }
bail:
    safe_free(node_name);
    return err;
}


static int
md_namespace_conflicts(	md_commit *commit ,
			const mdb_db *new_db,
			const char *incoming_ns
			)
{

    int err = 0;
    bn_request *ns_namespace= NULL;
    uint16 retError = -1;
    bn_binding_array *ns_precedence_list= NULL;
    uint32 ns_precedence_list_len = 0;
    char buff[8] = { 0 };
    tstring *ns_name= NULL;
    match_type_t match_type = mt_none;
    match_type_t type = mt_none;
    tstring *type_name = NULL;
    tstring *inhand_value = NULL;
    tstring *ret_value = NULL;
    tbool first_time_hit = false;
    tstring *current_domain = NULL;
    tstring *domain = NULL;
    char *node_name = NULL;
    tbool invalid = false;
    bn_binding *curr_binding = NULL;
    uint32 current_precedence = 0;
    uint32 inhand_precedence = 0;
    domain_type_t curr_domain_type = domain_none;

    err = bn_action_request_msg_create(&ns_namespace,
	    "/nkn/nvsd/namespace/actions/get_namespace_precedence_list");
    bail_error_null (err, ns_namespace);

    err =  md_commit_handle_action_from_module(commit,
	    ns_namespace, NULL, &retError, NULL,
	    &ns_precedence_list, NULL);
    bail_error(err);
    if (retError == 1)
	goto bail;

    lc_log_basic(LOG_DEBUG, "incoming ns: %s", incoming_ns);
    ns_precedence_list_len = bn_binding_array_length_quick(ns_precedence_list);

    err = md_get_domain_value(commit, new_db, incoming_ns, &current_domain, &curr_domain_type);
    bail_error(err);

    err = md_get_type_value(commit, new_db, incoming_ns, &type, &type_name);
    bail_error(err);

    err = md_get_precendence_value(commit, new_db, incoming_ns, &current_precedence);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "incoming domain %s type %s precedence %d",
	    ts_str(current_domain)?ts_str(current_domain):"",
	    ts_str(type_name)?ts_str(type_name):"",
	    current_precedence);
    if (ns_precedence_list_len != 0) {
	uint32 index = 1;
	for ( ; index <= ns_precedence_list_len; ++index) {
	    domain_type_t inhand_domain_type = domain_none;
	    tbool found = false;

	    snprintf(buff, 7,"%d", index);
	    buff[7] = '\0';
	    err = bn_binding_array_get_value_tstr_by_name ( ns_precedence_list,
		    buff, NULL, &ns_name);
	    bail_error_null(err, ns_name);
	    if (incoming_ns && strcmp (ts_str(ns_name), incoming_ns) == 0) {
		ts_free(&ns_name);
		continue;
	    }

	    err = md_nvsd_ns_exists(commit, new_db, ts_str(ns_name), &found);
	    bail_error(err);
	    if (found == false) {
		/* namespace doesn't exists in database, ignore further processing */
		ts_free(&ns_name);
		continue;
	    }
	    err = md_get_precendence_value(commit, new_db, ts_str(ns_name), &inhand_precedence);
	    bail_error(err);
	    if (current_precedence != inhand_precedence) {
		ts_free(&ns_name);
		continue;
	    }

	    node_name = smprintf("/nkn/nvsd/namespace/%s/match/type", ts_str(ns_name));
	    bail_null(node_name);
	    err = mdb_get_node_value_uint8(commit, new_db, node_name,
		    0, NULL,(uint8_t *) &match_type);
	    bail_error(err);
	    safe_free(node_name);

	    err = md_get_domain_value(commit, new_db, ts_str(ns_name), &domain,
		    &inhand_domain_type);
	    bail_error(err);

	    if(curr_domain_type == domain_host && inhand_domain_type == domain_host) {
		invalid = ts_equal(current_domain, domain);
	    } else {

		err = validate_domain_regex_expression (commit,
			ts_str(current_domain), ts_str(domain), &invalid);
		bail_error(err);
	    }
	    if (invalid == false) {  // no match hence no conflict
		lc_log_basic(LOG_DEBUG, "no conflict: incoming ns %s domain %s inhand ns %s domain %s",
			incoming_ns, ts_str(current_domain)?ts_str(current_domain):"",
			ts_str(ns_name)?ts_str(ns_name):"", ts_str(domain)?ts_str(domain):"");
		ts_free(&domain);
		ts_free(&ns_name);
		continue;
	    } else {      // domain matches so we need to check type conflict

		lc_log_basic(LOG_DEBUG, "conflict exists: incoming ns %s domain %s inhand ns %s domain %s",
			incoming_ns, ts_str(current_domain)?ts_str(current_domain):"",
			ts_str(ns_name)?ts_str(ns_name):"", ts_str(domain)?ts_str(domain):"");
		if (type != mt_none) {
		    invalid = false;
		    err =  validate_on_type(commit, new_db,
			    ts_str(ns_name), ts_str(type_name), &inhand_value, type,
			    match_type, &invalid);
		    bail_error(err);
		}
		node_name = NULL;
		if (type == mt_none) {
		    if (match_type != mt_none) {
			match_type_t temp_type = mt_none;
			err = md_get_type_value(commit, new_db,
				ts_str(ns_name), &temp_type, &inhand_value);
			bail_error(err);
			if (inhand_value && ts_length(inhand_value) > 0) {	
			    node_name = smprintf ("%s\t\t%s\t\t%s[%s]\n",
				    ts_str(ns_name), ts_str(domain), ts_str(inhand_value),
				    match_type_str[match_type]);
			}
		    } else {
			node_name = smprintf ("%s\t\t%s\t\tEMPTY\n",
				ts_str(ns_name), ts_str(domain));
		    }
		} else if (invalid) {
		    if (inhand_value && ts_length(inhand_value) > 0) {
			node_name = smprintf ("%s\t\t%s\t\t%s[%s]\n",
				ts_str(ns_name), ts_str(domain), ts_str(inhand_value),
				match_type_str[match_type]);
		    } else {
			node_name = smprintf ("%s\t\t%s\t\tEMPTY\n",
				ts_str(ns_name), ts_str(domain));
		    }
		}
		if (node_name) {
		    if (first_time_hit == false) {
			err = ts_new(&ret_value);
			bail_error_null(err,ret_value);
			err = ts_append_str (ret_value,
				"Conflict in existing namespaces\n"
				"namespace\t\tdomain\t\ttype\n"
				"---------------------------------------------------\n");
			bail_error(err);
			first_time_hit = true;

		    }
		    lc_log_basic(LOG_NOTICE, "conflicting namespace: %s", ts_str(ns_name));
		    lc_log_basic(LOG_DEBUG, "conflicting namespace list: %s", node_name);
		    err = ts_append_str (ret_value, node_name);
		    bail_error(err);	
		}
		safe_free(node_name);				
		ts_free(&inhand_value);
	    }
	    ts_free(&domain);
	    ts_free(&ns_name);
	}
    }
    if (ret_value)
    {
	// some conflict exists
	err = md_commit_set_return_status_msg_fmt(commit, 0,
		_("%s"), ts_str(ret_value));
	bail_error(err);
	goto bail;
    }


bail:
    ts_free(&ret_value);
    ts_free(&ns_name);
    ts_free(&current_domain);
    bn_binding_free(&curr_binding);
    ts_free(&domain);
    ts_free(&type_name);
    safe_free(node_name);
    ts_free(&inhand_value);
    bn_request_msg_free(&ns_namespace);
    bn_binding_array_free(&ns_precedence_list);
    return err;    	
}

static int
md_nvsd_ns_host_header_inherit_set(md_commit *commit,
		mdb_db *inout_new_db,
		const char *namespace,
		tbool  permit)
{
    int err = 0;

    bail_null(inout_new_db);

    lc_log_basic(LOG_DEBUG, "Setting /nkn/nvsd/origin_fetch/config/%s/host-header/inherit : %s",
	    namespace, lc_bool_to_str(permit));

    err = mdb_set_node_str(commit,
	    inout_new_db,  bsso_modify,
	    0, bt_bool,
	    lc_bool_to_str(permit),
	    "/nkn/nvsd/origin_fetch/config/%s/host-header/inherit",
	    namespace);
    bail_error(err);

bail:
    return err;
}



/* ---- This is valid only while upgrade rules
 * ---- are called from version 11 to 12
 */
static int md_nvsd_namespace_ug_match_criteria_get(md_commit *commit,
		const mdb_db	*db,
		const char 	*namespace,
		match_type_t	*ret_match_type)
{
    int err = 0;
    char *bn_name = NULL;
    match_type_t match_type = mt_none;

    bail_null(ret_match_type);
    bail_null(namespace);

    *ret_match_type = match_type;

    /* Find out which match type is set for this namespace.
     */
    bn_name = smprintf("/nkn/nvsd/namespace/%s/http/match/type", namespace);
    bail_null(bn_name);

    err = mdb_get_node_value_uint8(commit, db, bn_name, 0, NULL, (uint8_t *) &match_type);
    bail_error(err);

    *ret_match_type = match_type;
    lc_log_basic(LOG_INFO, "curent match type for namespace '%s' : %d",
	    namespace, match_type);

bail:
    safe_free(bn_name);
    return err;

}


static int md_nvsd_namespace_ug_match_11_to_12(
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		match_type_t mt,
		void *arg)
{
    int err = 0;
    char *node_name = NULL;
    union {
	tstring	 *string;
	struct {
	    tstring *name, *value;
	} hnv;
	struct {
	    tstring *name, *value;
	} qnv;
	struct {
	    tstring *ip, *port;
	}vh;
    } value;

    value.string = NULL;
    value.hnv.value = NULL;

    switch(mt)
    {
	case mt_none:
	    break;
	case mt_rtsp_uri:
	case mt_uri_name:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/%s/match/uri/uri_name", t_namespace,
		    (mt == mt_rtsp_uri) ? "rtsp" : "http");
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.string);
	    bail_error(err);

	    lc_log_debug(LOG_DEBUG, "node: %s: %s", node_name, ts_str(value.string));

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_uri,
		    ts_str(value.string),
		    "/nkn/nvsd/namespace/%s/match/uri/uri_name",
		    t_namespace);
	    bail_error(err);
	    break;

	case mt_rtsp_uri_regex:
	case mt_uri_regex:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/%s/match/uri/regex", t_namespace,
		    (mt == mt_rtsp_uri_regex) ? "rtsp" : "http");
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.string);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_regex,
		    ts_str(value.string),
		    "/nkn/nvsd/namespace/%s/match/uri/regex",
		    t_namespace);
	    bail_error(err);
	    break;

	case mt_header_name:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/http/match/header/name", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.hnv.name);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_string,
		    ts_str(value.hnv.name),
		    "/nkn/nvsd/namespace/%s/match/header/name",
		    t_namespace);
	    bail_error(err);

	    safe_free(node_name);
	    node_name = NULL;
	    /* Now set the value */
	    node_name = smprintf("/nkn/nvsd/namespace/%s/http/match/header/value", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.hnv.name);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_string,
		    ts_str(value.hnv.value),
		    "/nkn/nvsd/namespace/%s/match/header/value",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_header_regex:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/http/match/header/regex", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.string);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_regex,
		    ts_str(value.string),
		    "/nkn/nvsd/namespace/%s/match/header/regex",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_query_string_name:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/http/match/query-string/name", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.qnv.name);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_string,
		    ts_str(value.qnv.name),
		    "/nkn/nvsd/namespace/%s/match/query-string/name",
		    t_namespace);
	    bail_error(err);


	    safe_free(node_name);
	    node_name = NULL;
	    /* now set the value */
	    node_name = smprintf("/nkn/nvsd/namespace/%s/http/match/query-string/value", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.qnv.value);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_string,
		    ts_str(value.qnv.value),
		    "/nkn/nvsd/namespace/%s/match/query-string/value",
		    t_namespace);
	    bail_error(err);
	    break;
	case mt_query_string_regex:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/http/match/query-string/regex", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.string);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_regex,
		    ts_str(value.string),
		    "/nkn/nvsd/namespace/%s/match/query-string/regex",
		    t_namespace);
	    bail_error(err);
	    break;


	case mt_rtsp_vhost:
	case mt_vhost:
	    node_name = smprintf("/nkn/nvsd/namespace/%s/%s/match/virtual-host/ip", t_namespace,
		    (mt == mt_rtsp_vhost) ? "rtsp" : "http");
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.vh.ip);
	    bail_error(err);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_ipv4addr,
		    ts_str(value.vh.ip),
		    "/nkn/nvsd/namespace/%s/match/virtual-host/ip",
		    t_namespace);
	    bail_error(err);

	    safe_free(node_name);
	    node_name = smprintf("/nkn/nvsd/namespace/%s/%s/match/virtual-host/port",
		    (mt == mt_rtsp_vhost) ? "rtsp" : "http", t_namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name,
		    0, NULL,
		    &value.vh.port);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_uint16,
		    ts_str(value.vh.port),
		    "/nkn/nvsd/namespace/%s/match/virtual-host/port",
		    t_namespace);
	    bail_error(err);
	    break;

	default:
	    break;
    }

    /* Start deleting all unwanted nodes */
    err = md_nvsd_namespace_ug_11_to_12_delete_old_nodes(
	    old_db, inout_new_db, t_namespace, arg);
    bail_error(err);

bail:
    safe_free(node_name);
    ts_free(&value.string);
    ts_free(&value.qnv.name);
    ts_free(&value.qnv.value);
    return err;
}


static int md_nvsd_namespace_ug_11_to_12_delete_old_nodes(
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		const char *t_namespace,
		void *arg)
{
    int err = 0;

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_uri, "",
	    "/nkn/nvsd/namespace/%s/http/match/uri/uri_name", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_regex, "",
	    "/nkn/nvsd/namespace/%s/http/match/uri/regex", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_uri, "",
	    "/nkn/nvsd/namespace/%s/rtsp/match/uri/uri_name", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_regex, "",
	    "/nkn/nvsd/namespace/%s/rtsp/match/uri/regex", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_string, "",
	    "/nkn/nvsd/namespace/%s/http/match/header/name", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_string, "",
	    "/nkn/nvsd/namespace/%s/http/match/header/value", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_regex, "",
	    "/nkn/nvsd/namespace/%s/http/match/header/regex", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_string, "",
	    "/nkn/nvsd/namespace/%s/http/match/query-string/name", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_string, "",
	    "/nkn/nvsd/namespace/%s/http/match/query-string/value", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_regex, "",
	    "/nkn/nvsd/namespace/%s/http/match/query-string/regex", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_ipv4addr, "255.255.255.255",
	    "/nkn/nvsd/namespace/%s/http/match/virtual-host/ip", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_uint16, "80",
	    "/nkn/nvsd/namespace/%s/http/match/virtual-host/port", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_ipv4addr, "255.255.255.255",
	    "/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/ip", t_namespace);
    bail_error(err);

    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_uint16, "554",
	    "/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/port", t_namespace);
    bail_error(err);

bail:
    return err;
}

static int
md_nvsd_namespace_add_initial(
		md_commit *commit,
		mdb_db    *db,
		void *arg)
{
    int err = 0;
    /*Thilak - 05/06/2010 this may not be needed as we don't need probe for now */
#if 1
    err = mdb_create_node_bindings(commit, db,
	    md_nvsd_namespace_probe_initial_values,
	    sizeof( md_nvsd_namespace_probe_initial_values )/sizeof(bn_str_value));
    bail_error(err);

    err = mdb_create_node_bindings
	(commit, db, md_probe_email_init_val,
	 sizeof(md_probe_email_init_val) / sizeof(bn_str_value));
    bail_error(err);

#endif
bail:
    return err;
}

/* ------------------------------------------------------------------------- */
static int
md_upgrade_servermap(md_commit *commit, const mdb_db *db,
                       const bn_binding *binding, void *arg)
{
    int err = 0;
    const tstring *name = NULL;
    char *binding_name = NULL;
    char *servermap_name = NULL;
    tbool found = false;
    mdb_db *t_inout_new_db = *(mdb_db**)(arg);

    bail_null(t_inout_new_db);
    bail_null(binding);

    /* Get the binding name */
    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is namespace's servermap node */
    if (bn_binding_name_pattern_match (ts_str(name),
		"/nkn/nvsd/namespace/*/origin-server/http/server-map"))
    {
	bn_str_value binding_value[2];
	tstring *t_server_map = NULL;

	err = mdb_get_node_value_tstr(NULL, db, ts_str(name),
		0, &found, &t_server_map);
	bail_error(err);

	/* Now delete the old node */
	err = mdb_dbbe_delete_node(NULL, t_inout_new_db, ts_str(name));

	/* Now create the new nodes only if a valid servermap
	 * exists */
	if((t_server_map != NULL) &&
		((ts_length(t_server_map) > 0))){

	    lc_log_basic(LOG_INFO,
		    "Upgrading server-map value : %s",
		    ts_str(t_server_map));
	    /* Create the new wildcard node name */
	    binding_name = smprintf("%s/1", ts_str(name));

	    /* Create the first wildcard node */
	    binding_value[0].bv_name = binding_name;
	    binding_value[0].bv_type = bt_uint32;
	    binding_value[0].bv_value = "1";

	    /* Create the new servermap node name */
	    binding_name = smprintf("%s/1/map-name", ts_str(name));
	    servermap_name = smprintf("%s", ts_str(t_server_map));

	    /* Create the first wildcard servermap node */
	    binding_value[1].bv_name = binding_name;
	    binding_value[1].bv_type = bt_string;
	    binding_value[1].bv_value = servermap_name;

	    err = mdb_create_node_bindings(NULL, t_inout_new_db,
		    binding_value,
		    sizeof(binding_value)/sizeof(bn_str_value));
	    bail_error(err);
	}
    }
bail:
    return(err);
}

static int
md_namespace_reval_hdlr(md_commit *commit, mdb_db **db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg)
{
    int err = 0;
    lt_time_sec curr_time;
    char time_buf[128] = {0};
    char time_buf_local[128] = {0};
    bn_request *req = NULL;
    tstring *t_ns = NULL;
    tstring *t_type = NULL;
    node_name_t node = {0};
    uint16_t ret_code = 0;
    bn_binding *binding = NULL;

    /* Get the namespace field */
    err = bn_binding_array_get_value_tstr_by_name
	(params, "namespace", NULL, &t_ns);
    bail_error_null(err, t_ns);

    /* Get the type field */
    err = bn_binding_array_get_value_tstr_by_name
	(params, "type", NULL, &t_type);
    bail_error_null(err, t_type);

    curr_time = lt_curr_time();
    if(curr_time) {
	snprintf(time_buf, sizeof(time_buf), "%d", curr_time);
    }
    err = lt_datetime_sec_to_buf (curr_time , false, sizeof(time_buf_local), time_buf_local);

    lc_log_basic(LOG_DEBUG, "revalidate_hdlr : %s",time_buf);


    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    snprintf(node, sizeof(node), "/nkn/nvsd/origin_fetch/config/%s/object/revalidate/time_barrier",
	    t_ns ? ts_str(t_ns): "");

    err = bn_binding_new_str(&binding, node, ba_value, bt_string,
	    0, time_buf);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    snprintf(node, sizeof(node), "/nkn/nvsd/origin_fetch/config/%s/object/revalidate/reval_type",
	    t_ns ? ts_str(t_ns): "");

    err = bn_binding_new_str(&binding, node, ba_value, bt_string,
	    0, "all");
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, NULL, NULL, NULL);
    complain_error(err);

    if(!ret_code) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 0,
		"Objects cached up to <%s> will be "
		"revalidated", time_buf_local);
    } else {
	err = md_commit_set_return_status_msg(
		commit, 1,
		"Error setting revalidation time");
    }

bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&t_ns);
    ts_free(&t_type);
    return err;

}
static int md_namespace_commit_rtsp_check (md_commit *commit, mdb_db **db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg)
{
    tstring *t_header_name = NULL;
    tstring *t_header_regex = NULL;
    tstring *t_namespace_name = NULL;
    char *header = NULL;
    char *regex = NULL;
    bn_binding *binding = NULL;
    int err = 0;

    bail_require_quiet(!strcmp(action_name,"/nkn/nvsd/namespace/actions/validate_namespace_match_header"));

    err = bn_binding_array_get_value_tstr_by_name(params, "namespace_name", NULL,
	    &t_namespace_name);
    bail_error(err);

    header = smprintf( "/nkn/nvsd/namespace/%s/match/header/name",ts_str(t_namespace_name));
    bail_null(header);
    regex = smprintf( "/nkn/nvsd/namespace/%s/match/header/regex",ts_str(t_namespace_name));
    bail_null(regex);


    if (t_namespace_name) {
	err = mdb_get_node_binding(commit, *db,
		header,
		0, &binding);
	bail_error(err);

	if (binding) {
	    err = bn_binding_get_tstr(binding, ba_value, bt_string, 0, &t_header_name);
	    bail_error(err);
	    bn_binding_free(&binding);
	}
	err = mdb_get_node_binding(commit, *db,
		regex,
		0, &binding);
	bail_error(err);

	if (binding) {
	    err = bn_binding_get_tstr(binding, ba_value, bt_regex, 0, &t_header_regex);
	    bail_error(err);
	    bn_binding_free(&binding);
	}
	if((t_header_name != NULL && strcmp("",ts_str(t_header_name))) || (t_header_regex != NULL && strcmp("",ts_str(t_header_regex)))) {
	    err = md_commit_set_return_status_msg_fmt(
		    commit, 1,
		    _("error: %s\n"), "Delivery protocol cannot be RTSP as Match Header is set");
	    bail_error(err);
	    goto bail;
	}

    }

bail:
    ts_free(&t_header_regex);
    ts_free(&t_header_name);
    ts_free(&t_namespace_name);
    safe_free(header);
    safe_free(regex);
    return err;
}

int md_namespace_commit_dependency_check(const mdb_db_change *change,  mdb_db *inout_new_db,
						    dependency_type_t dependency, tbool *retval )
{
    char *node_name = NULL;
    tstring *t_value = NULL;
    int err = 0;
    const char *namespace = NULL;
    *retval = false;

    if(dependency == dt_match_header_name) {
	if(mdct_modify == change->mdc_change_type) {	

	    namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    node_name = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/rtsp", namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name, 0, NULL, &t_value);
	    bail_error(err);


	    if(t_value != NULL && (!ts_cmp_str(t_value, "true", true)) ) {
		*retval = true;
	    }
	}
    } else if (dependency == dt_match_header_regex) {
	if( mdct_modify == change->mdc_change_type ) {

	    namespace = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    node_name = smprintf("/nkn/nvsd/namespace/%s/delivery/proto/rtsp", namespace);
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    node_name, 0, NULL, &t_value);
	    bail_error(err);

	    if(t_value != NULL && (!ts_cmp_str(t_value, "true", true))) {
		*retval = true;	
	    }
	}

    }
bail:
    safe_free(node_name);
    ts_free(&t_value);
    return err;
}

int
md_lib_send_action_from_module(md_commit *commit, const char *action_name)
{
    bn_request *req = NULL;
    int err = 0;

    bail_null(action_name);

    err = bn_action_request_msg_create(&req, action_name);
    bail_error_null (err, req);

    err =  md_commit_handle_action_from_module(commit,
	    req, NULL, NULL, NULL, NULL, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err ;
}

int
md_nvsd_namespace_set_lst_node(md_commit *commit, const mdb_db *old_db,
					mdb_db *inout_new_db, const mdb_db_change *change,
					const char *lst_nd_str, int item_idx)
{

    int err = 0;
    const 	bn_attrib *new_val = NULL;
    const 	bn_attrib *old_val = NULL;
    tstring *ts_old_val = NULL;
    tstring *ts_new_val = NULL;

    const char *old_bind_item_name = NULL;
    const char *new_bind_item_name = NULL;


    if(change->mdc_old_attribs) {
	old_val = bn_attrib_array_get_quick(change->mdc_old_attribs,
		ba_value);

	err = bn_attrib_get_tstr(old_val, NULL, bt_string,
		NULL, &ts_old_val);
	bail_error(err);
	old_bind_item_name = ts_str(ts_old_val);

	/* add the new val to the list mainitained by cert object
	   remove the old val from the same list */
	/* remove the old entry */
	const char *item = NULL;
	item = tstr_array_get_str_quick(change->mdc_name_parts, item_idx);

	err = mdb_set_node_str(commit, inout_new_db,
		bsso_delete, 0, bt_none,
		"",
		lst_nd_str,
		old_bind_item_name, item);
	bail_error(err);

    }

    if(change->mdc_new_attribs) {
	new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
		ba_value);

	err = bn_attrib_get_tstr(new_val, NULL, bt_string,
		NULL, &ts_new_val);
	bail_error(err);

	if(ts_equal_str(ts_new_val, "", false))
	    goto bail;

	new_bind_item_name = ts_str(ts_new_val);

	/* remove the old entry */
	const char *item = NULL;
	item = tstr_array_get_str_quick(change->mdc_name_parts, item_idx);

	err = mdb_set_node_str(commit, inout_new_db,
		bsso_create, 0, bt_string,
		item,
		lst_nd_str,
		new_bind_item_name, item);
	bail_error(err);

    }
bail:
    ts_free(&ts_old_val);
    ts_free(&ts_new_val);
    return err;
}


static int
md_nvsd_vip_commit_check(md_commit *commit,
                           const mdb_db *old_db, const mdb_db *new_db,
                           const mdb_db_change_array *change_list,
                           const tstring *node_name,
                           const tstr_array *node_name_parts,
                           mdb_db_change_type change_type,
                           const bn_attrib_array *old_attribs,
                           const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const   bn_attrib *new_val = NULL;
    tstring *value = NULL;
    tbool valid = false;
    tstr_array *config_ip = NULL;
    tbool ret_found = false;
    tstring *ret_value = NULL;
    uint32_t i = 0;
    tbool ip_found = false;

    if(mdct_add == change_type ||
	    mdct_modify == change_type ) {
	if(!new_attribs)
	    goto bail;

	new_val = bn_attrib_array_get_quick(new_attribs,
		ba_value);
	bail_null(new_val);

	err = bn_attrib_get_tstr(new_val, NULL, bt_string,
		NULL, &value);
	bail_error_null(err, value);

	err = lia_str_is_ipv4addr(ts_str(value), &valid);
	bail_error(err);

	if(!valid) {
	    err = md_commit_set_return_status_msg_fmt(
		    commit, 1,
		    "Invalid IPV4 address");
	    bail_error(err);
	    goto bail;

	}

	//TODO-Check if its IP address configured to any of the interfaces.
	// /net/interface/config/*/addr/ipv4/static/1/ip
	err = mdb_get_matching_tstr_array(commit, new_db,
		"/net/interface/config/*/addr/ipv4/static/1/ip",
		0, &config_ip);
	bail_error(err);

	for(i = 0; i < tstr_array_length_quick(config_ip); i++) {
	    err = mdb_get_node_value_tstr(commit, new_db, tstr_array_get_str_quick(config_ip, i),
		    0, &ret_found, &ret_value);
	    bail_error(err);
	    if(ts_equal(value, ret_value)) {
		ip_found = true;
		break;
	    }
	    ts_free(&ret_value);
	}

	if(!ip_found) {
	    err = md_commit_set_return_status_msg_fmt(
		    commit, 1,
		    "IP address doesn't belong to any of the configured interfaces");
	    bail_error(err);
	}
    }

bail:
    ts_free(&value);
    ts_free(&ret_value);
    tstr_array_free(&config_ip);
    return err;

}

static int
md_nvsd_policy_bind_commit_check(md_commit *commit,
                           const mdb_db *old_db, const mdb_db *new_db,
                           const mdb_db_change_array *change_list,
                           const tstring *node_name,
                           const tstr_array *node_name_parts,
                           mdb_db_change_type change_type,
                           const bn_attrib_array *old_attribs,
                           const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    tstring *value = NULL;
    const char *item_name = NULL;
    const   bn_attrib *new_val = NULL;

    if(mdct_add == change_type ||
	    mdct_modify == change_type) {
	new_val = bn_attrib_array_get_quick(new_attribs,
		ba_value);

	err = bn_attrib_get_tstr(new_val, NULL, bt_string,
		NULL, &value);
	bail_error_null(err, value);

	//If its a node rst,Do nothing
	if(ts_equal_str(value, "", false))
	    goto bail;
	item_name = ts_str(value);

	tbool is_valid = false;

	err = md_nvsd_namespace_pe_check_bind(commit,old_db,
		&is_valid, NULL,"/nkn/nvsd/policy_engine/config/server/%s", item_name);

	if(!is_valid) {
	    err = md_commit_set_return_status_msg_fmt(
		    commit, 1,
		    _("error:Invalid Policy Server\n") );
	    bail_error(err);
	    goto bail;
	}

    }
bail:
    ts_free(&value);
    return(err);
}

/* End of md_nvsd_namespace.c */

static int
md_encrypt_string(md_commit *commit,const mdb_db *db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg)
{
    int err = 0;
    tstring *string = NULL;
    char *crypt_string = NULL;
    bn_binding *binding = NULL;

    /* Get the input_string field */
    err = bn_binding_array_get_value_tstr_by_name
	(params, "input_string", NULL, &string);
    bail_error_null(err, string);

    err = ltc_encrypt_password(ts_str(string), lpa_default, &crypt_string);
    bail_error(err);

    if (crypt_string == NULL) {
	lc_log_basic(LOG_NOTICE, "crypt_string is NULL");
	goto bail;
    }

    lc_log_debug(jnpr_log_level, "encrypted %s, to %s", ts_str(string), crypt_string);
    err = bn_binding_new_str(&binding, "output_string", ba_value, bt_string, 0,
	    crypt_string);

    err = md_commit_binding_append_takeover(commit, 0, &binding);
    bail_error(err);

bail:
    /* NOTE : no need to free binding, it has been taken over */
    safe_free(crypt_string);
    return err;
}

int
md_check_if_node_exists(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db, const char *check_name,
		const char *node_root, int *exists)
{
    int err = 0;
    tbool found = false;
    tstring *t_name = NULL;
    node_name_t node_name = {0};

    *exists = 0;

    if (check_name == NULL ) {
	md_commit_set_return_status_msg_fmt(commit, 1, "Invalid input ");
	goto bail;
    }

    if (0 == strcmp(check_name, "")) {
	goto bail;
    }
    snprintf(node_name, sizeof(node_name), "%s/%s", node_root, check_name);

    ts_free(&t_name);
    err = mdb_get_node_value_tstr(commit, new_db, node_name, 0,
	    &found , &t_name);
    bail_error(err);

    if (found) {
	*exists = 1;
    }

bail:
    return err;
}
int
md_check_if_node_attached(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db,  const mdb_db_change *change,
		const char *check_name, const char *node_pattern, int *error)
{
    int err = 0, num = 0, i = 0;
    tstr_array *smap_nodes = NULL;
    tstring  * t_smap_name = NULL;
    const char *smap_node = node_pattern, *node = NULL;

    if (check_name == NULL || (change == NULL)) {
	md_commit_set_return_status_msg_fmt(commit, 1, "Invalid input ");
	*error = 1;
	goto bail;
    }

    err = mdb_get_matching_tstr_array(commit, new_db, smap_node,
	    0, &smap_nodes);
    bail_error(err);

    if (smap_nodes == NULL) {
	*error = 0;
	goto bail;
    }

    num = tstr_array_length_quick(smap_nodes);

    *error = 0;
    for (i = 0; i < num; i++) {
	node = tstr_array_get_str_quick(smap_nodes, i);

	ts_free(&t_smap_name);
	err = mdb_get_node_value_tstr(commit, new_db, node, 0,
		NULL, &t_smap_name);
	bail_error(err);

	if (0 == strcmp(ts_str_maybe_empty(t_smap_name), "")) {
	    /* ignore if NULL */
	    continue;
	}
	if (0 == strcmp(ts_str_maybe_empty(t_smap_name), check_name)) {
	    /* there an instance of this, are checking ourselves */
	    if (0 == strcmp(node,ts_str(change->mdc_name))) {
		continue;
	    }
	    *error = 1;
	    goto bail;
	}
    }

bail:
    tstr_array_free(&smap_nodes);
    ts_free(&t_smap_name);
    return err;
}
int
md_check_if_smap_attached(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db,  const mdb_db_change *change,
		const char *smap_name, int *error)
{
    int err = 0, num = 0, i = 0;
    tstr_array *smap_nodes = NULL;
    tstring  * t_smap_name = NULL;
    const char *smap_node = "/nkn/nvsd/namespace/*/origin-server/http"
	"/server-map/*/map-name", *node = NULL;

    if (smap_name == NULL || (change == NULL)) {
	md_commit_set_return_status_msg_fmt(commit, 1, "Invalid sever-map name");
	*error = 1;
	goto bail;
    }

    lc_log_debug(jnpr_log_level, "looking %s", ts_str(change->mdc_name));
    err = mdb_get_matching_tstr_array(commit, new_db, smap_node,
	    0, &smap_nodes);
    bail_error(err);

    if (smap_nodes == NULL) {
	*error = 0;
	goto bail;
    }

    num = tstr_array_length_quick(smap_nodes);

    *error = 0;
    for (i = 0; i < num; i++) {
	node = tstr_array_get_str_quick(smap_nodes, i);
	lc_log_debug(jnpr_log_level, "%d: %s", i, node);

	ts_free(&t_smap_name);
	err = mdb_get_node_value_tstr(commit, new_db, node, 0,
		NULL, &t_smap_name);
	bail_error(err);

	if (0 == strcmp(ts_str_maybe_empty(t_smap_name), "")) {
	    /* ignore if NULL */
	    continue;
	}
	if (0 == strcmp(ts_str_maybe_empty(t_smap_name), smap_name)) {
	    /* there an instance of this, are checking ourselves */
	    if (0 == strcmp(node,ts_str(change->mdc_name))) {
		lc_log_debug(jnpr_log_level, "THIS is my namespace");
		continue;
	    }
	    lc_log_debug(jnpr_log_level, "got match - %s, %s",
		    smap_name, node);
	    *error = 1;
	    err = md_commit_set_return_status_msg_fmt(commit, 1,
		    "server-map %s already attached to another namepsace",
		    smap_name);
	    bail_error(err);
	    goto bail;
	}
    }

bail:
    tstr_array_free(&smap_nodes);
    ts_free(&t_smap_name);
    return err;
}
static int
md_nvsd_ns_acclog_bind_check_func(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs, void
		*arg)
{

    int err = 0;
    const bn_attrib *attrib = NULL;
    char *acclog_name = NULL;
    node_name_t acclog_nd = {0};
    bn_binding *binding = NULL;

    if(change_type == mdct_add || change_type == mdct_modify) {
	bail_null(new_attribs);
	attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
	bail_null(attrib);

	err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &acclog_name);
	bail_error_null(err, acclog_name);

	snprintf(acclog_nd, sizeof(acclog_nd), "/nkn/accesslog/config/profile/%s",
		acclog_name);

	err = mdb_get_node_binding(commit, new_db, acclog_nd, 0, &binding);
	bail_error(err);

	if(!binding) {
	    err = md_commit_set_return_status_msg(commit, 1,
		    "Invalid access log profile.");
	    bail_error(err);
	    goto bail;
	}
    }

bail:
    safe_free(acclog_name);
    bn_binding_free(&binding);
    return err;

}

int
md_check_ns_smap( md_commit *commit,
	const mdb_db *new_db,
	mdb_db_change_array *change_list)
{
    int err = 0, i = 0;
    tstr_array *attached_smaps = NULL, *binding_parts = NULL;
    tstr_array *ns_names = NULL;
    int total_ns = 0, ns_with_smaps = 0;
    tstr_array_options opts;
    const char *bname = NULL, *ns_name = NULL;

    err = mdb_get_matching_tstr_array(commit, new_db,
	    "/nkn/nvsd/namespace/*/origin-server/http/server-map/*",
	    0, &attached_smaps);
    bail_error(err);

    total_ns = tstr_array_length_quick(attached_smaps);

    if (total_ns <= MAX_NAMESPACE_WITH_SMAPS) {
	/* total ns is less than required check, so it is success */
	err = 0;
	goto bail;
    }

    /* create a new array with all namespaces names with duplicates to be
       deleted */
    err = tstr_array_sort(attached_smaps);
    bail_error(err);

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&ns_names, &opts);
    bail_error(err);

    /* count unique ns names associated with server-maps */
    for (i = 0; i < total_ns; i++) {
	bname = tstr_array_get_str_quick(attached_smaps, i);
	if (bname == NULL) {
	    continue;
	}

	tstr_array_free(&binding_parts);
	err = bn_binding_name_to_parts(bname, &binding_parts, NULL);
	bail_error_null(err, binding_parts);

	ns_name = tstr_array_get_str_quick(binding_parts, 3);
	if (ns_name == NULL) {
	    continue;
	}

	err = tstr_array_append_str(ns_names, ns_name);
	bail_error(err);
    }

    ns_with_smaps = tstr_array_length_quick(ns_names);
    if (ns_with_smaps >  MAX_NAMESPACE_WITH_SMAPS) {
	err = md_commit_set_return_status_msg(commit, 1, "max limit of"
		" namespaces with server-maps reached");
	bail_error(err);
	goto bail;
    }

bail:
    tstr_array_free(&binding_parts);
    tstr_array_free(&ns_names);
    tstr_array_free(&attached_smaps);
    return err;
}

int
get_next_ns_precedence(md_commit *commit, mdb_db *inout_new_db,
	uint32_t*next_value)
{
    int err = 0, ns_array_len = 0;
    uint32_t highest_num = 0;
    const char *highest_prece = NULL;
    tstr_array *ns_pr_arr = NULL;

    bail_null(next_value);

    err = mdb_get_tstr_array(commit, inout_new_db,
	    "/nkn/nvsd/namespace", "ns-precedence", false,
	    0, &ns_pr_arr);
    bail_error(err);

    err = tstr_array_sort_numeric(ns_pr_arr);
    bail_error(err);

    //tstr_array_dump(ns_pr_arr, "NS-PRECEDENCE");

    ns_array_len = tstr_array_length_quick(ns_pr_arr);
    if (ns_array_len > 0) {
	highest_prece = tstr_array_get_str_quick(ns_pr_arr, ns_array_len -1 );
	if (highest_prece == NULL) {
	    highest_num = 10;
	} else {
	    highest_num = strtoul(highest_prece, NULL, 10);
	}
    } else {
	highest_num = 10;
    }
    *next_value = highest_num + 10;

bail:
    tstr_array_free(&ns_pr_arr);
    return err;
}

int
md_check_ns_precedence(md_commit *commit, const mdb_db *db, int *unique)
{
    int err = 0, ns_array_len1 = 0, ns_array_len2 = 0, i = 0;
    tstr_array *ns_pr_arr = NULL, *ns_pr_arr2 = NULL;
    tstr_array_options opts;
    const char *value = NULL;

    bail_null(unique);

    *unique = 1;

    err = mdb_get_tstr_array(commit,db,
	    "/nkn/nvsd/namespace", "ns-precedence", false,
	    0, &ns_pr_arr);
    bail_error(err);

    err = tstr_array_sort(ns_pr_arr);
    bail_error(err);

    //tstr_array_dump(ns_pr_arr, "CHECK_PRECEDENCE");

    /* length before removing duplicates */
    ns_array_len1 = tstr_array_length_quick(ns_pr_arr);

    memset(&opts, 0, sizeof(opts));

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&ns_pr_arr2, &opts);
    bail_error(err);

    for (i = 0; i < ns_array_len1; i++) {
	value = tstr_array_get_str_quick(ns_pr_arr, i);
	if (value == NULL) {
	    continue;
	}

	err = tstr_array_append_str(ns_pr_arr2, value);
	bail_error(err);
    }
    //tstr_array_dump(ns_pr_arr2, "CHECK_PRECEDENCE-NO-DUPS");

    /* length after remove duplicates */
    ns_array_len2 = tstr_array_length_quick(ns_pr_arr2);

    if (ns_array_len2 != ns_array_len1) {
	/* this means there were duplicates */
	*unique = 0;
    }

bail:
    tstr_array_free(&ns_pr_arr);
    tstr_array_free(&ns_pr_arr2);
    return err;
}

int
md_nvsd_regex_check(const char *regex, error_msg_t errorbuf, int errorbuf_size)
{
    regex_t temp_regex;
    int rv = 0;

    rv = regcomp(&temp_regex, regex, REG_EXTENDED|REG_NOSUB|REG_NEWLINE);
    if (rv) {
        if (errorbuf && errorbuf_size) {
	    regerror(rv, &temp_regex, errorbuf, errorbuf_size);
	}
    }
    regfree(&temp_regex);
    return rv;
}

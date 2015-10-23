/*
 *
 * src/bin/web/request_handler/web_internal.h
 *
 *
 *
 */

#ifndef __WEB_INTERNAL_H_
#define __WEB_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <tcl.h>
#include <tclDecls.h>
#include "web.h"
#include "hash_table.h"
#include "dso.h"
#include "cgi.h"
#include "gcl.h"
#include "bnode.h"

/* ========================================================================= */
/** \file web_internal.h request handler API
 * \ingroup web
 *
 * These functions are available to all modules of the request handler.
 *
 * XXXX/EMT: this is a big abstraction mess.  Lots of the stuff in here
 * should not be seen by anyone outside the request handler.  The public
 * stuff should be moved elsewhere that does not mention "internal".
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

#define WEB_CMD_TMS_PUTS                 "tms::puts"
#define WEB_CMD_TMS_LOG                  "tms::log"
#define WEB_CMD_TMS_LOG_BASIC            "tms::log-basic"
#define WEB_CMD_TMS_STREAM_MODE          "tms::stream-mode"
#define WEB_CMD_TMS_CLEAR_HEADER         "tms::clear-header"
#define WEB_CMD_TMS_APPEND_HEADER        "tms::append-header"
#define WEB_CMD_TMS_REMOVE_HEADER        "tms::remove-header"
#define WEB_CMD_TMS_GET_PARAM            "tms::get-param"
#define WEB_CMD_TMS_GET_PARAM_MULT       "tms::get-param-mult"
#define WEB_CMD_TMS_SET_PARAM            "tms::set-param"
#define WEB_CMD_TMS_GET_CGI_VAR          "tms::get-cgi-var"
#define WEB_CMD_TMS_SET_CGI_VAR          "tms::set-cgi-var"
#define WEB_CMD_TMS_GET_CUSTOM_VAR       "tms::get-custom-var"
#define WEB_CMD_TMS_SET_CUSTOM_VAR       "tms::set-custom-var"
#define WEB_CMD_TMS_GET_SESSION_VAR      "tms::get-session-var"
#define WEB_CMD_TMS_SET_SESSION_VAR      "tms::set-session-var"
#define WEB_CMD_TMS_GET_SESSION_ID       "tms::get-session-id"
#define WEB_CMD_TMS_GET_COOKIE           "tms::get-cookie"
#define WEB_CMD_TMS_SET_COOKIE           "tms::set-cookie"
#define WEB_CMD_TMS_GET_URL_PARAM        "tms::get-url-param"
#define WEB_CMD_TMS_SET_URL_PARAM        "tms::set-url-param"
#define WEB_CMD_TMS_CLEAR_URL_PARAMS     "tms::clear-url-params"
#define WEB_CMD_TMS_URL_BUILDER \
"tms::url-builder"
#define WEB_CMD_TMS_URL_BUILDER_PAIRED \
"tms::url-builder-paired"

#ifndef TMSLIBS_DISABLE_WEB_OLD_CONSTRUCT_URL
#define WEB_CMD_TMS_CONSTRUCT_URL        "tms::construct-url"
#define WEB_CMD_TMS_CONSTRUCT_URL_PAIRED "tms::construct-url-paired"
#endif /* not TMSLIBS_DISABLE_WEB_OLD_CONSTRUCT_URL */

#define WEB_CMD_TMS_HTML_ESCAPE          "tms::html-escape";
#define WEB_CMD_TMS_HTML_BODY_ESCAPE     "tms::html-body-escape";
#define WEB_CMD_TMS_HTML_BODY_ESCAPE_TRANSCODE_WS \
"tms::html-body-escape-transcode-ws";
#define WEB_CMD_TMS_HTML_TRANSCODE_WS    "tms::html-transcode-ws";
#define WEB_CMD_TMS_URI_UNESCAPE         "tms::uri-unescape";
#define WEB_CMD_TMS_URI_COMP_ESCAPE      "tms::uri-component-escape";
#define WEB_CMD_TMS_URI_UNESCAPE         "tms::uri-unescape";
/*
 * XXX/SML: as there's only one type of java-specific escaping function for
 * javascript, keep the template function name shorter, but in the future it's
 * conceivable we might add options for non-default types of javascript
 * escaping, and a javascript-value-escape form that does the default, as was
 * done with html-escape and html-body-escape.
 */
#define WEB_CMD_TMS_JAVASCRIPT_VALUE_ESCAPE \
"tms::javascript-escape";
#define web_cmd_tms_hex_encode "tms::hex-encode"
#define web_cmd_tms_hex_decode "tms::hex-decode"

#ifndef TMSLIBS_DISABLE_WEB_OLD_ESCAPING
#define WEB_CMD_TMS_URL_ESCAPE           "tms::url-escape"
#define WEB_CMD_TMS_URL_UNESCAPE         "tms::url-unescape"
#endif /* not TMSLIBS_DISABLE_WEB_OLD_ESCAPING */

#define WEB_CMD_TMS_GRAFTPOINT           "tms::graftpoint"
#define WEB_CMD_TMS_GET_CONFIG           "tms::get-config"
#ifdef PROD_FEATURE_CMC_SERVER
#define WEB_CMD_TMS_GET_CMC_CONFIGS      "tms::get-cmc-configs"
#endif /* PROD_FEATURE_CMC_SERVER */
#define WEB_CMD_TMS_SHOW_CONFIG          "tms::show-config"
#define WEB_CMD_TMS_ITERATE_CONFIG       "tms::iterate-config"
#define WEB_CMD_TMS_ITERATE_CONFIG_LAST "tms::iterate-config-last"
#define WEB_CMD_TMS_GET_BINDING          "tms::get-binding"
#define WEB_CMD_TMS_GET_ATTRIB           "tms::get-attrib"
#define WEB_CMD_TMS_GET_BINDING_CHILDREN    "tms::get-binding-children"
#define WEB_CMD_TMS_SORT_BINDINGS    "tms::sort-bindings"
#define WEB_CMD_TMS_GET_BINDING_CHILDREN_NAMES    "tms::get-binding-children-names"
#define WEB_CMD_TMS_GET_BINDING_CHILD_BY_NAME    "tms::get-binding-child-by-name"
#define WEB_CMD_TMS_GET_CHILD_VALUE_BY_NAME    "tms::get-child-value-by-name"
#define WEB_CMD_TMS_CALL_ACTION          "tms::call-action"
#define WEB_CMD_TMS_CALL_ACTION_EX       "tms::call-action-ex"
#define WEB_CMD_TMS_GET_MFD              "tms::get-mfd"
#define WEB_CMD_TMS_ITERATE_MFD          "tms::iterate-mfd"
#define WEB_CMD_TMS_GET_UNIQUE_ID        "tms::get-unique-id"

#ifdef PROD_FEATURE_CAPABS
#define WEB_CMD_TMS_HAS_CAPABILITY     "tms::has-capability"
#define WEB_CMD_TMS_HAS_CAPABILITY_MAX "tms::has-capability-max"
#define WEB_CMD_TMS_GET_CAPABILITIES     "tms::get-capabilities"
#define WEB_CMD_TMS_GET_CAPABILITIES_MAX "tms::get-capabilities-max"
#define WEB_CMD_TMS_GET_CAPABILITY_LIST "tms::get-capability-list"
#define WEB_CMD_TMS_GET_CAPABILITY_DEFAULT "tms::get-capability-default"
#endif

#ifdef PROD_FEATURE_ACLS
#define WEB_CMD_TMS_ACL_CHECK            "tms::acl-check"
#define WEB_CMD_TMS_ACL_GET_ROLES        "tms::acl-get-roles"
#define WEB_CMD_TMS_ACL_HAS_SET_POTENTIAL  "tms::acl-has-set-potential"
#endif

#define WEB_CMD_TMS_CHECK_AUTHORIZATION    "tms::check-authorization"
#define WEB_CMD_TMS_NEED_CONFIG_CONFIRM     "tms::need-config-confirm"
#define WEB_CMD_TMS_BINDING_NAME_TO_PARTS    "tms::binding-name-to-parts"
#define WEB_CMD_TMS_BINDING_NAME_LAST_PART    "tms::binding-name-last-part"
#define WEB_CMD_TMS_IPV4PFX_TO_IP        "tms::ipv4pfx-to-ip"
#define WEB_CMD_TMS_IPV4PFX_TO_MASK      "tms::ipv4pfx-to-mask"
#define WEB_CMD_TMS_IPV4PFX_TO_MASKLEN    "tms::ipv4pfx-to-masklen"
#define WEB_CMD_TMS_IPV4ADDR_TO_NUM      "tms::ipv4addr-to-num"
#define WEB_CMD_TMS_TIME_TO_COUNTER      "tms::time-to-counter"
#define WEB_CMD_TMS_TIME_TO_COUNTER_MS    "tms::time-to-counter-ms"
#define WEB_CMD_TMS_TIME_TO_COUNTER_EX  "tms::time-to-counter-ex"
#define WEB_CMD_TMS_MASK_TO_MASKLEN      "tms::mask-to-masklen"
#define WEB_CMD_TMS_MASKLEN_TO_MASK      "tms::masklen-to-mask"

#define WEB_CMD_TMS_IPV4ADDR_TO_MASKLEN   "tms::ipv4addr-to-masklen"
#define WEB_CMD_TMS_MASKLEN_TO_IPV4ADDR   "tms::masklen-to-ipv4addr"
#define WEB_CMD_TMS_IPV6ADDR_TO_MASKLEN   "tms::ipv6addr-to-masklen"
#define WEB_CMD_TMS_MASKLEN_TO_IPV6ADDR   "tms::masklen-to-ipv6addr"
#define WEB_CMD_TMS_INETADDR_TO_MASKLEN   "tms::inetaddr-to-masklen"
#define WEB_CMD_TMS_MASKLEN_TO_INETADDR   "tms::masklen-to-inetaddr"

#define WEB_CMD_TMS_IPV4PREFIX_TO_IP      "tms::ipv4prefix-to-ip"
#define WEB_CMD_TMS_IPV4PREFIX_TO_MASK    "tms::ipv4prefix-to-mask"
#define WEB_CMD_TMS_IPV4PREFIX_TO_MASKLEN "tms::ipv4prefix-to-masklen"

#define WEB_CMD_TMS_IPV6PREFIX_TO_IP      "tms::ipv6prefix-to-ip"
#define WEB_CMD_TMS_IPV6PREFIX_TO_MASK    "tms::ipv6prefix-to-mask"
#define WEB_CMD_TMS_IPV6PREFIX_TO_MASKLEN "tms::ipv6prefix-to-masklen"

#define WEB_CMD_TMS_INETPREFIX_TO_IP      "tms::inetprefix-to-ip"
#define WEB_CMD_TMS_INETPREFIX_TO_MASK    "tms::inetprefix-to-mask"
#define WEB_CMD_TMS_INETPREFIX_TO_MASKLEN "tms::inetprefix-to-masklen"

#define WEB_CMD_TMS_INETADDR_TO_FAMILY    "tms::inetaddr-to-family"
#define WEB_CMD_TMS_INETADDRZ_TO_FAMILY   "tms::inetaddrz-to-family"
#define WEB_CMD_TMS_INETPREFIX_TO_FAMILY  "tms::inetprefix-to-family"

#define WEB_CMD_TMS_IS_IPV4ADDR           "tms::is-ipv4addr"
#define WEB_CMD_TMS_IS_IPV6ADDR           "tms::is-ipv6addr"
#define WEB_CMD_TMS_IS_INETADDR           "tms::is-inetaddr"

#define WEB_CMD_TMS_INETADDR_IS_LINKLOCAL "tms::inetaddr-is-linklocal"

#define WEB_CMD_TMS_GET_VERSION          "tms::get-version"
#define WEB_CMD_TMS_HAVE_PROD_FEATURE    "tms::have-prod-feature"
#define WEB_CMD_TMS_MAY_OBFUSCATE        "tms::may-obfuscate"
#define WEB_CMD_TMS_IF_PRIMARY_NAME      "tms::if-primary-name"
#define WEB_CMD_TMS_GET_UPTIME           "tms::get-uptime"
#define WEB_CMD_TMS_IS_PRODUCTION        "tms::is-production"
#define WEB_CMD_TMS_IS_AUTHENTICATED     "tms::is-authenticated"

#ifdef PROD_FEATURE_CAPABS
#define WEB_CMD_TMS_GID_TO_CAPAB         "tms::gid-to-capab"
#define WEB_CMD_TMS_CAPAB_TO_GID         "tms::capab-to-gid"
#endif /* PROD_FEATURE_CAPABS */

#define WEB_CMD_TMS_EMIT_ETC_ISSUE       "tms::emit-etc-issue"
#define WEB_CMD_TMS_GET_PROGRESS_OPERS_BY_TYPE    "tms::get-progress-opers-by-type"
#define WEB_CMD_TMS_DUMP_ENV             "tms::dump-env"
#define WEB_CMD_TMS_REDIRECT_URL         "tms::redirect-url"
#define WEB_CMD_TMS_REQUEST_SCHEME       "tms::request-scheme"

extern const char web_cmd_tms_puts[];
extern const char web_cmd_tms_log[];
extern const char web_cmd_tms_log_basic[];
extern const char web_cmd_tms_stream_mode[];
extern const char web_cmd_tms_clear_header[];
extern const char web_cmd_tms_append_header[];
extern const char web_cmd_tms_remove_header[];
extern const char web_cmd_tms_get_param[];
extern const char web_cmd_tms_get_param_mult[];
extern const char web_cmd_tms_set_param[];
extern const char web_cmd_tms_get_cgi_var[];
extern const char web_cmd_tms_set_cgi_var[];
extern const char web_cmd_tms_get_custom_var[];
extern const char web_cmd_tms_set_custom_var[];
extern const char web_cmd_tms_get_session_var[];
extern const char web_cmd_tms_set_session_var[];
extern const char web_cmd_tms_get_session_id[];
extern const char web_cmd_tms_get_cookie[];
extern const char web_cmd_tms_set_cookie[];
extern const char web_cmd_tms_get_url_param[];
extern const char web_cmd_tms_set_url_param[];
extern const char web_cmd_tms_clear_url_params[];
extern const char web_cmd_tms_url_builder[];
extern const char web_cmd_tms_url_builder_paired[];
#ifndef TMSLIBS_DISABLE_WEB_OLD_CONSTRUCT_URL
extern const char web_cmd_tms_construct_url[];
extern const char web_cmd_tms_construct_url_paired[];
#endif /* not TMSLIBS_DISABLE_WEB_OLD_CONSTRUCT_URL */
extern const char web_cmd_tms_html_escape[];
extern const char web_cmd_tms_html_body_escape[];
extern const char web_cmd_tms_html_body_escape_transcode_ws[];
extern const char web_cmd_tms_html_transcode_ws[];
extern const char web_cmd_tms_uri_unescape[];
extern const char web_cmd_tms_uri_comp_escape[];
extern const char web_cmd_tms_javascript_value_escape[];
#ifndef TMSLIBS_DISABLE_WEB_OLD_ESCAPING
extern const char web_cmd_tms_url_escape[];
extern const char web_cmd_tms_url_unescape[];
#endif /* not TMSLIBS_DISABLE_WEB_OLD_ESCAPING */
extern const char web_cmd_tms_graftpoint[];
extern const char web_cmd_tms_get_config[];
#ifdef PROD_FEATURE_CMC_SERVER
extern const char web_cmd_tms_get_cmc_configs[];
#endif /* PROD_FEATURE_CMC_SERVER */
extern const char web_cmd_tms_show_config[];
extern const char web_cmd_tms_iterate_config[];
extern const char web_cmd_tms_iterate_config_last[];
extern const char web_cmd_tms_get_binding[];
extern const char web_cmd_tms_get_attrib[];
extern const char web_cmd_tms_get_binding_children[];
extern const char web_cmd_tms_sort_bindings[];
extern const char web_cmd_tms_get_binding_children_names[];
extern const char web_cmd_tms_get_binding_child_by_name[];
extern const char web_cmd_tms_get_child_value_by_name[];
extern const char web_cmd_tms_call_action[];
extern const char web_cmd_tms_call_action_ex[];
extern const char web_cmd_tms_get_mfd[];
extern const char web_cmd_tms_iterate_mfd[];
extern const char web_cmd_tms_get_unique_id[];

#ifdef PROD_FEATURE_CAPABS
extern const char web_cmd_tms_has_capability[];
extern const char web_cmd_tms_has_capability_max[];
extern const char web_cmd_tms_get_capabilities[];
extern const char web_cmd_tms_get_capabilities_max[];
extern const char web_cmd_tms_get_capability_list[];
extern const char web_cmd_tms_get_capability_default[];
#endif

#ifdef PROD_FEATURE_ACLS
extern const char web_cmd_tms_acl_check[];
extern const char web_cmd_tms_acl_get_roles[];
extern const char web_cmd_tms_acl_has_set_potential[];
#endif

extern const char web_cmd_tms_check_authorization[];
extern const char web_cmd_tms_need_config_confirm[];
extern const char web_cmd_tms_binding_name_to_parts[];
extern const char web_cmd_tms_binding_name_last_part[];
extern const char web_cmd_tms_ipv4pfx_to_ip[];
extern const char web_cmd_tms_ipv4pfx_to_mask[];
extern const char web_cmd_tms_ipv4pfx_to_masklen[];
extern const char web_cmd_tms_ipv4addr_to_num[];
extern const char web_cmd_tms_time_to_counter[];
extern const char web_cmd_tms_time_to_counter_ms[];
extern const char web_cmd_tms_time_to_counter_ex[];
extern const char web_cmd_tms_mask_to_masklen[];
extern const char web_cmd_tms_masklen_to_mask[];

extern const char web_cmd_tms_ipv4addr_to_masklen[];
extern const char web_cmd_tms_masklen_to_ipv4addr[];
extern const char web_cmd_tms_ipv6addr_to_masklen[];
extern const char web_cmd_tms_masklen_to_ipv6addr[];
extern const char web_cmd_tms_inetaddr_to_masklen[];
extern const char web_cmd_tms_masklen_to_inetaddr[];

extern const char web_cmd_tms_ipv4prefix_to_ip[];
extern const char web_cmd_tms_ipv4prefix_to_mask[];
extern const char web_cmd_tms_ipv4prefix_to_masklen[];

extern const char web_cmd_tms_ipv6prefix_to_ip[];
extern const char web_cmd_tms_ipv6prefix_to_mask[];
extern const char web_cmd_tms_ipv6prefix_to_masklen[];

extern const char web_cmd_tms_inetprefix_to_ip[];
extern const char web_cmd_tms_inetprefix_to_mask[];
extern const char web_cmd_tms_inetprefix_to_masklen[];

extern const char web_cmd_tms_inetaddr_to_family[];
extern const char web_cmd_tms_inetaddrz_to_family[];
extern const char web_cmd_tms_inetprefix_to_family[];

extern const char web_cmd_tms_is_ipv4addr[];
extern const char web_cmd_tms_is_ipv6addr[];
extern const char web_cmd_tms_is_inetaddr[];

extern const char web_cmd_tms_inetaddr_is_linklocal[];

extern const char web_cmd_tms_get_version[];
extern const char web_cmd_tms_have_prod_feature[];
extern const char web_cmd_tms_may_obfuscate[];
extern const char web_cmd_tms_if_primary_name[];
extern const char web_cmd_tms_get_uptime[];
extern const char web_cmd_tms_is_production[];
extern const char web_cmd_tms_is_authenticated[];

#ifdef PROD_FEATURE_CAPABS
extern const char web_cmd_tms_gid_to_capab[];
extern const char web_cmd_tms_capab_to_gid[];
#endif /* PROD_FEATURE_CAPABS */

extern const char web_cmd_tms_emit_etc_issue[];
extern const char web_cmd_tms_get_progress_opers_by_type[];
extern const char web_cmd_tms_dump_env[];
extern const char web_cmd_tms_redirect_url[];
extern const char web_cmd_tms_request_scheme[];


#define WEB_PARAM_TEMPLATE "template"
#define WEB_ACTION_PREFIX "action"

extern const char web_param_template[];
extern const char web_action_prefix[];


/*-----------------------------------------------------------------------------
 * STRUCTURES
 */

typedef struct rh_request_data {
    Tcl_Interp *rrd_interpreter;             /* TCL interpreter */

    ht_table *rrd_actions;                   /* registered actions */
    ht_table *rrd_auto_actions;              /* registered auto actions */

    lc_dso_context *rrd_dso_context;         /* context for modules */

    ht_table *rrd_cached_queries;            /* cached queries */

    web_request_data *rrd_web_data;          /* web per request state */
} rh_request_data;

/**
 * These are constants that should be returned from an action function
 * (web_action_func).
 *
 * If an action returns war_override_output, it is expected that the
 * action sent direct printf output (stdout) to the browser including
 * all headers.
 */
typedef enum {
    war_ok,             /* request processed - continue to next action */
    war_skip_rest,      /* request processed - skip other actions/templates */
    war_override_output,/* override output including HTTP headers */
    war_error           /* error ocurred during execution of action */
} web_action_result;

/**
 * Structure that holds information regarding the current web context.
 * This structure is provided to a module's init function and provides
 * context data that can be used, for example, to register a special
 * TCL command.
 */
typedef struct web_context {
    Tcl_Interp *wctx_interpreter;     /* TCL interpreter */
    gcl_session *wctx_gcl_session;    /* gcl session */
} web_context;

/**
 * Function for request actions.
 * \param param pointer to special callback parameter.
 * \return the result of the action.
 */
typedef web_action_result (*web_action_func)(web_context *, void *param);

/* internal structure representing an action */
typedef struct web_action {
    tstring *wa_name;                    /* name of the action */
    int32 wa_priority;                   /* not used for non-auto actions */
    void *wa_param;                      /* callback parameter */
    web_action_func wa_callback;         /* the actual callback */
} web_action;

/**
 * Prototype of a module to module direct call function.
 * This is just a generic function used for type checking funcs.
 */
typedef int (*web_module_func)(void *);

/*-----------------------------------------------------------------------------
 * GLOBALS
 */

extern web_request_data *g_web_data;
extern rh_request_data *g_rh_data;


/**
 * Convenience free function for use in Tcl_SetResult.
 * \param buf the buffer to free.
 */
void web_tcl_free(char *buf);

/**
 * Register an action.
 * If the name is already registered, this call will replace the existing
 * action with the new action.
 * \param rh_data Request handler global context
 * \param name the name of the action - used in the URL. (duped)
 * \param passback a parameter that will be passed to the callback.
 * \param callback the callback function.
 * \return 0 on success, error code on failure.
 */
int web_register_action(rh_request_data *rh_data,
                        const tstring *name, void *passback,
                        web_action_func callback);
int web_register_action_str(rh_request_data *rh_data, const char *name,
                            void *passback,
                            web_action_func callback);

/**
 * Unregister an action.
 * \param rh_data Request handler global context
 * \param name the name of the action - used in the URL.
 * \return 0 on success, error code on failure.
 */
int web_unregister_action(rh_request_data *rh_data, const tstring *name);
int web_unregister_action_str(rh_request_data *rh_data, const char *name);

/**
 * Register an automatic action that gets called for every request.
 * The given priority number is equivalent to the priority number
 * that can be tacked onto an action specified in the URL.
 * If the name is already registered, the existing callback will be
 * replaced with this new one.
 * \param rh_data Request handler global context
 * \param name the name of the action - used in the URL. (duped)
 * \param passback a parameter that will be passed back to the callback.
 * \param callback the callback function.
 * \param priority the priority of this auto action.
 * \return 0 on success, error code on failure.
 */
int web_register_auto_action(rh_request_data *rh_data, const tstring *name,
                             void *passback,
                             web_action_func callback, int32 priority);
int web_register_auto_action_str(rh_request_data *rh_data, const char *name,
                                 void *passback,
                                 web_action_func callback, int32 priority);

/**
 * Unregister an automatic action.
 * \param rh_data Request handler global context
 * \param name the name of the action - used in the URL.
 * \return 0 on success, error code on failure.
 */
int web_unregister_auto_action(rh_request_data *rh_data,
                               const tstring *name);
int web_unregister_auto_action_str(rh_request_data *rh_data, 
                                   const char *name);

/**
 * Find the path to a requested template relative to the template base
 * directory.  Essentially this searches various locale-specific
 * directories beneath the base for localized versions of the
 * template.  If nothing locale-specific is found, we fall back on the
 * base directory.  We do not check if it exists in the base
 * directory; if it does not, someone else will have to handle it.
 * \param rh_data Request handler global context
 * \param tmpl_name the name of the template to look for
 * \param ret_tmpl_rel_path returns the relative path to the template if
 * a locale-specific version was found, or simply the name if not.
 * Note that it will NOT have a .tem or .tcl extension on it.
 */
int web_get_tmpl_rel_path(rh_request_data *rh_data,
                          const tstring *tmpl_name,
                          tstring **ret_tmpl_rel_path);

/**
 * Render a template.
 * For debug builds, this function will compile the template if it
 * doesn't exist or is older than the template file.
 * The include versions do not output the content type header.
 * \param rh_data Request handler global context
 * \param tmpl_rel_path the path to the template to render, relative to
 * the template base directory.
 * \return 0 on success, error code on failure.
 */
int web_render_template(rh_request_data *rh_data, 
                        const tstring *tmpl_rel_path);
int web_render_template_str(rh_request_data *rh_data,
                            const char *tmpl_rel_path);
int web_include_template(rh_request_data *rh_data,
                         const tstring *tmpl_rel_path);
int web_include_template_str(rh_request_data *rh_data,
                             const char *tmpl_rel_path);


/**
 * Returns a function pointer to the named function inside the named
 * module. Provides a means of calling functions provided by extension
 * modules. Will return NULL in ret_func if the function was not found.
 * \param rh_data Request handler global context
 * \param module_name the name of the module.
 * \param func_name the name of the function.
 * \param ret_func the found function pointer or NULL.
 * \return 0 on success, error code on failure.
 */
int web_get_module_function(rh_request_data *rh_data, const char *module_name,
                            const char *func_name,
                            web_module_func *ret_func);

/**
 * Returns a pointer to a symbol from another web module.
 * This is a more generic version of web_get_module_function(), as
 * it works with any symbol, rather than just functions with a
 * particular prototype.
 */
int web_get_symbol(lc_dso_context *context, const char *module_name,
                   const char *symbol_name, void **ret_symbol);

/**
 * Runs the specified action immediately.  This is slightly easier than
 * having to tangle with web_get_symbol(), and only works on actions
 * that have actually been registered.
 */
web_action_result web_run_action(web_context *ctx, const char *action_name);


/*-----------------------------------------------------------------------------
 * WEB MODULES
 */

/**
 * Load the web modules.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_init_modules(rh_request_data *rh_data);

/**
 * Unload the web modules.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_deinit_modules(rh_request_data *rh_data);

/**
 * Execute the actions specified for the current request.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_execute_actions(rh_request_data *rh_data);


/*-----------------------------------------------------------------------------
 * TCL
 */

/**
 * Initialize the tcl interpreter.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_init_interpreter(rh_request_data *rh_data);

/**
 * Initialize the query cache.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_init_query_cache(rh_request_data *rh_data);

/**
 * Add data to the query cache.
 */
int web_insert_cached_data(rh_request_data *rh_data,
                           void *data, char **ret_token);

/**
 * Search for data in the query cache.
 */
int web_lookup_cached_data(rh_request_data *rh_data, const char *token,
                           void **ret_data, tbool *ret_match);

/**
 * Register built in TCL commands.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_register_builtin_tcl_commands(rh_request_data *rh_data);

/**
 * Deinitialize the tcl interpreter.
 * \param rh_data Request handler global context
 * \return 0 on success, error code on failure.
 */
int web_deinit_interpreter(rh_request_data *rh_data);

/*-----------------------------------------------------------------------------
 * BUILT IN TCL COMMANDS
 */

/**
 * Implementation of the TCL tms::puts command.
 *
 * tms::puts \<string\>
 */
int web_tcl_puts(ClientData client_data, Tcl_Interp *interpreter,
                 int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::log command.
 *
 * tms::log \<string\>
 */
int web_tcl_log(ClientData client_data, Tcl_Interp *interpreter,
                int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::log-basic command.
 *
 * tms::log-basic \<log level\> \<string\>
 */
int web_tcl_log_basic(ClientData client_data, Tcl_Interp *interpreter,
                      int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::stream-mode command.
 *
 * tms::stream-mode
 */
int web_tcl_stream_mode(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::clear-header command.
 *
 * tms::clear-header
 */
int web_tcl_clear_header(ClientData client_data, Tcl_Interp *interpreter,
                         int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::append-header command.
 *
 * tms::append-header \<name\> \<value\>
 */
int web_tcl_append_header(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::remove-header command.
 *
 * tms::remove-header \<name\>
 */
int web_tcl_remove_header(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-param command.
 *
 * tms::get-param \<src\> \<param-name\>
 *
 * where \<src\> is one of:
 *
 *    query  - find in query data
 *    post   - find in post data
 *    either - find in either query or post
 *
 * returns the param as a string or an empty string if not found.
 */
int web_tcl_get_param(ClientData client_data, Tcl_Interp *interpreter,
                      int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-param-mult command.
 *
 * tms::get-param-mult \<src\> \<param-name\>
 *
 * where \<src\> is one of:
 *
 *    query  - find in query data
 *    post   - find in post data
 *    either - find in either query or post
 *
 * returns the param as a string or an empty string if not found.
 */
int web_tcl_get_param_mult(ClientData client_data, Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::set-param command.
 *
 * tms::set-param \<dest\> \<policy\> \<param-name\> \<param-value\>
 *
 * where \<dest\> is one of:
 *
 *    query  - set in query data
 *    post   - set in post data
 *    either - set in query or post
 *
 * where \<policy\> is one of:
 *
 *    append - create param or append to existing values
 *    replace - create param or replace existing values
 *
 * use a value of "" to remove the parameter.
 */
int web_tcl_set_param(ClientData client_data, Tcl_Interp *interpreter,
                      int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-cgi-var command.
 *
 * tms::get-cgi-var \<name\>
 *
 * returns the var as a string or an empty string if not found.
 */
int web_tcl_get_cgi_var(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::set-cgi-var command.
 *
 * tms::set-cgi-var \<name\> \<value\>
 *
 * use a value of "" to remove the var.
 */
int web_tcl_set_cgi_var(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-custom-var command.
 *
 * tms::get-custom-var \<name\>
 *
 * returns the var as a string or an empty string if not found.
 */
int web_tcl_get_custom_var(ClientData client_data, Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::set-custom-var command.
 *
 * tms::set-custom-var \<name\> \<value\>
 *
 * use a value of "" to remove the var.
 */
int web_tcl_set_custom_var(ClientData client_data, Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-session-var command.
 *
 * tms::get-session-var \<name\>
 *
 * returns the var as a string or an empty string if not found.
 */
int web_tcl_get_session_var(ClientData client_data, Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::set-session-var command.
 *
 * tms::set-session-var \<name\> \<value\>
 *
 * use a value of "" to remove the var.
 */
int web_tcl_set_session_var(ClientData client_data, Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-session-id command.
 *
 * tms::get-session-id
 *
 * returns the session ID, a unique number (in string form) which
 * identifies this login session.
 */
int web_tcl_get_session_id(ClientData client_data, Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-cookie command.
 *
 * tms::get-cookie \<name\>
 *
 * returns the cookie as a string or an empty string if not found.
 */
int web_tcl_get_cookie(ClientData client_data, Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::set-cookie command.
 *
 * tms::set-cookie \<name\> \<value\>
 *                 [-e\<epoch_time_sec\>] [-p\<path\>] [-d\<domain\>] [-h]
 *
 * where \<epoch_time_sec\> is local time in seconds since epoch.
 *
 * use a value of NULL to remove the cookie.
 */
int web_tcl_set_cookie(ClientData client_data, Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-url-param command.
 *
 * tms::get-url-param \<name\>
 *
 * returns the url param as a string or an empty string if not found.
 */
int web_tcl_get_url_param(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::set-url-param command.
 *
 * tms::set-url-param \<name\> \<value\>
 *
 * use a value of "" to remove the param.
 */
int web_tcl_set_url_param(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::clear-url-params command.
 *
 * tms::clear-url-params
 */
int web_tcl_clear_url_params(ClientData client_data, Tcl_Interp *interpreter,
                             int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::url-builder command.
 *
 * tms::url-builder [-n] [-a] [-s] ...
 *
 * where -n means do not entity-encode '&' as "&amp;"
 * where -a means the parameters are already percent-encoded (by default we
 * escape the name and value data strings)
 * where -s means singular data string(s) rather than name=value pair(s).
 * where ... is a variable list of params in the form 'name=value', unless -s
 * is used.
 */
int web_tcl_url_builder(ClientData client_data,
                                  Tcl_Interp *interpreter,
                                  int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::url-builder-paired command.
 *
 * tms::construct-url-paired [-n] [-a] ...
 *
 * where -n means do not entity-encode '&' as "&amp;"
 * where -a means the parameters are already percent-encoded (by default we
 * escape the name and value data strings)
 * where ... is a variable list of interleaved name value pairs.
 */
int web_tcl_url_builder_paired(ClientData client_data,
                                         Tcl_Interp *interpreter,
                                         int objc, Tcl_Obj *CONST objv[]);

#ifndef TMSLIBS_DISABLE_WEB_OLD_CONSTRUCT_URL
/**
 * Implementation of the TCL tms::construct-url command.
 *
 * tms::construct-url [-n] ...
 *
 * where -n is optional, and means do not do (amp) entity encoding.
 * where ... is a variable list of params in the form 'name=value'.
 */
int web_tcl_construct_url(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::construct-url command.
 *
 * tms::construct-url-paired [-n] ...
 *
 * where -n is optional, and means do not do (amp) entity encoding.
 * where ... is a variable list of interleaved name value pairs.
 */
int web_tcl_construct_url_paired(ClientData client_data,
                                 Tcl_Interp *interpreter,
                                 int objc, Tcl_Obj *CONST objv[]);
#endif /* not TMSLIBS_DISABLE_WEB_OLD_CONSTRUCT_URL */

#ifndef TMSLIBS_DISABLE_WEB_OLD_ESCAPING
/**
 * Implementation of the TCL tms::url-escape command.
 *
 * tms::url-escape \<url\>
 */
int web_tcl_url_escape(ClientData client_data, Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);
#endif /* not TMSLIBS_DISABLE_WEB_OLD_ESCAPING */

/**
 * Implementation of the TCL tms::html-escape command.
 *
 * tms::html-escape \<string\>
 */
int web_tcl_html_escape(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::html-body-escape command.
 *
 * tms::html-body-escape \<string\>
 */
int web_tcl_html_body_escape(ClientData client_data, Tcl_Interp *interpreter,
                             int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::html-body-escape-transcode-ws command.
 *
 * tms::html-body-escape-transcode-ws \<string\>
 */
int web_tcl_html_body_escape_transcode_ws(ClientData client_data,
                                          Tcl_Interp *interpreter,
                                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::html-transcode-ws command.
 *
 * tms::html-transcode-ws \<string\>
 */
int web_tcl_html_transcode_ws(ClientData client_data,
                              Tcl_Interp *interpreter,
                              int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::uri-component-escape command.
 *
 * tms::uri-component-escape \<uri\>
 */
int web_tcl_uri_comp_escape(ClientData clientData, Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::uri-unescape command.
 *
 * tms::uri-unescape \<uri\>
 */
int web_tcl_uri_unescape(ClientData clientData, Tcl_Interp *interpreter,
                         int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::javascript-escape command.
 *
 * tms::javascript-escape \<string\>
 */
int web_tcl_javascript_value_escape(ClientData client_data, Tcl_Interp
                                    *interpreter, int objc,
                                    Tcl_Obj *CONST objv[]);

#ifndef TMSLIBS_DISABLE_WEB_OLD_ESCAPING
/**
 * Implementation of the TCL tms::url-unescape command.
 *
 * tms::url-unescape \<url\>
 */
int web_tcl_url_unescape(ClientData client_data, Tcl_Interp *interpreter,
                         int objc, Tcl_Obj *CONST objv[]);
#endif /* not TMSLIBS_DISABLE_WEB_OLD_ESCAPING */

/**
 * Implementation of the TCL tms::hex-encode command.
 *
 * tms::hex-encode \<text\>
 */
int web_tcl_hex_encode(ClientData client_data,
                       Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::hex-decode command.
 *
 * tms::hex-decode \<text\>
 */
int web_tcl_hex_decode(ClientData client_data,
                       Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::graftpoint command.
 *
 * tms::graftpoint prefix
 */
int web_tcl_graftpoint(ClientData client_data,
                       Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-config command.
 *
 * tms::get-config node-name [format-bool] [db-name]
 */
int web_tcl_get_config(ClientData client_data, Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

#ifdef PROD_FEATURE_CMC_SERVER
/**
 * Implementation of the TCL tms::get-cmc-configs command.
 *
 * tms::get-cmc-configs node-name [strip-bn-groups] [db-name] 
 */
int web_tcl_get_cmc_configs(ClientData clientData, Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);
#endif /* PROD_FEATURE_CMC_SERVER */

/**
 * Implementation of the TCL tms::show-config command.
 *
 * tms::show-config db-name
 */
int web_tcl_show_config(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::iterate-config command.
 *
 * tms::iterate-config node-name [db-name]
 */
int web_tcl_iterate_config(ClientData client_data, Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::iterate-config-last command.
 *
 * tms::iterate-config-last node-name [db-name]
 */
int web_tcl_iterate_config_last(ClientData client_data,
                                Tcl_Interp *interpreter,
                                int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-binding command.
 *
 * tms::get-binding node-name [db-name]
 *
 * returns a list containing the following:
 * idx 0: name
 * idx 1: ba_value type
 * idx 2: value
 */
int web_tcl_get_binding(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-attrib command.
 *
 * tms::get-attrib node-name attrib-id [db-name]
 *
 * returns a list containing the following:
 * idx 0: node name
 * idx 1: type (from attrib specified)
 * idx 2: value (from attrib specified)
 */
int web_tcl_get_attrib (ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-binding-children command.
 *
 * tms::get-binding-children \<parent-node\> \<include-self\> \<subtree\> [db-name]
 *
 * returns a token which can be used in the get-binding-child-by-name
 * command below.
 */
int web_tcl_get_binding_children(ClientData client_data,
                                 Tcl_Interp *interpreter,
                                 int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::sort-bindings command.
 *
 * tms::sort-bindings \<token\>
 */
int web_tcl_sort_bindings(ClientData client_data,
                          Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-binding-children-names command.
 *
 * tms::get-binding-children-names \<token\> \<pattern\>
 *
 * returns a list of the children matching the pattern in the
 * binding children specified by the given token.
 */
int web_tcl_get_binding_children_names(ClientData client_data,
                                       Tcl_Interp *interpreter,
                                       int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-binding-child-by-name command.
 *
 * tms::get-binding-child-by-name \<token\> \<child-name\>
 *
 * returns a list containing the following:
 * idx 0: name
 * idx 1: ba_value type
 * idx 2: value
 */
int web_tcl_get_binding_child_by_name(ClientData client_data,
                                      Tcl_Interp *interpreter,
                                      int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-child-value-by-name command.
 *
 * tms::get-child-value-by-name \<token\> \<node-name\>
 *
 * returns a string containing the value of the node requested.
 */
int web_tcl_get_child_value_by_name(ClientData client_data,
                                    Tcl_Interp *interpreter,
                                    int objc, Tcl_Obj *CONST objv[]);

/**
 * Convert a bn_binding_array to a Tcl list of lists.
 * The resulting Tcl object will be in the format:
 * [list [list $b1name $b1type $b1value] ...]
 *
 * \param bn_array the bn_binding_array object pointer.
 * \param interpreter the TCL interpreter.
 * \param ret_tcl_list the resulting Tcl list of lists.
 */
int web_tcl_bn_binding_array_to_tcl_list(Tcl_Interp *interpreter,
                    const bn_binding_array *bn_array,
                    Tcl_Obj **ret_tcl_list);

/**
 * Convert a bn_binding_array to the components of a bn_reponse.
 * The resulting Tcl object will be in the format:
 * [list $code $msg [list [list $b1name $b1type $b1value] ...]]
 *
 * \param bn_array the bn_binding_array object pointer.
 * \param code code that is embedded in the resulting Tcl list.
 * \param msg message that is embedded in the resulting Tcl list.
 * \param interpreter the TCL interpreter.
 * \param ret_tcl_list the resulting Tcl list of lists.
 */
int web_tcl_bn_binding_array_to_binding_tcl_list(
                   Tcl_Interp *interpreter,
                   const bn_binding_array *bn_array,
                   uint32 code, const char *msg,
                   Tcl_Obj **ret_tcl_list);

/**
 * Implementation of the TCL tms::call-action command.
 *
 * tms::call-action node-name [{name type value} ...]
 */
int web_tcl_call_action(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::call-action command.
 *
 * tms::call-action-ex node-name [{name type value} ...]
 */
int web_tcl_call_action_ex(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-mfd command.
 *
 * tms::get-mfd node-name
 */
int web_tcl_get_mfd(ClientData client_data, Tcl_Interp *interpreter,
                    int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::iterate-mfd command.
 *
 * tms::iterate-mfd node-name
 */
int web_tcl_iterate_mfd(ClientData client_data, Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-unique-id command.
 *
 * tms::get-unique-id
 */
int web_tcl_get_unique_id(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

#ifdef PROD_FEATURE_CAPABS
/**
 * Implementation of the TCL tms::has-capability command.
 *
 * tms::has-capability
 */
int web_tcl_has_capability(ClientData client_data, Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::has-capability-max command.
 *
 * tms::has-capability-max
 */
int web_tcl_has_capability_max(ClientData client_data, Tcl_Interp *interpreter,
                               int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-capabilities command.
 *
 * tms::get-capabilities
 */
int web_tcl_get_capabilities(ClientData client_data, Tcl_Interp *interpreter,
                             int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-capabilities-max command.
 *
 * tms::get-capabilities-max
 */
int web_tcl_get_capabilities_max(ClientData client_data, 
                                 Tcl_Interp *interpreter,
                                 int objc, Tcl_Obj *CONST objv[]);

/*
 * Implementation of the TCL tms::get-capability-list command.
 */
int web_tcl_get_capability_list(ClientData client_data, 
                                Tcl_Interp *interpreter, int objc, 
                                Tcl_Obj *CONST objv[]);

/*
 * Implementation of the tms::get-capability-default command.
 */
int web_tcl_get_capability_default(ClientData client_data, 
                                   Tcl_Interp *interpreter, int objc, 
                                   Tcl_Obj *CONST objv[]);

#endif /* PROD_FEATURE_CAPABS */


/**
 * Implementation of the TCL tms::need-config-confirm command.
 *
 * tms::need-config-confirm
 */
int web_tcl_need_config_confirm(ClientData client_data, 
                                Tcl_Interp *interpreter,
                                int objc, Tcl_Obj *CONST objv[]);

#ifdef PROD_FEATURE_ACLS
/**
 * Implementation of the TCL tms::acl-check command.
 *
 * tms::acl-check &lt;ACLs&gt; &lt;operations&gt;
 */
int web_tcl_acl_check(ClientData client_data, Tcl_Interp *interpreter,
                      int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::acl-get-roles command.
 *
 * tms::check-acl-get-roles
 */
int web_tcl_acl_get_roles(ClientData client_data,
                          Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::acl-has-set-potential command.
 *
 * tms::acl-has-set-potential
 */
int web_tcl_acl_has_set_potential(ClientData client_data,
                                  Tcl_Interp *interpreter,
                                  int objc, Tcl_Obj *CONST objv[]);
#endif

/**
 * Implementation of the TCL tms::check-authorization command.
 *
 * tms::check-authorization &lt;capability&gt;
 *                          &lt;ACLs&gt; &lt;operations&gt;
 */
int web_tcl_check_authorization(ClientData client_data,
                                Tcl_Interp *interpreter,
                                int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::binding-name-to-parts command.
 */
int web_tcl_binding_name_to_parts(ClientData client_data,
                                  Tcl_Interp *interpreter,
                                  int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::binding-name-last-part command.
 */
int web_tcl_binding_name_last_part(ClientData client_data,
                                   Tcl_Interp *interpreter,
                                   int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv4pfx-to-ip command.
 *
 * tms::ipv4pfx-to-ip \<IPv4 prefix\>
 */
int web_tcl_ipv4pfx_to_ip(ClientData client_data, Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv4pfx-to-mask command.
 *
 * tms::ipv4pfx-to-mask \<IPv4 prefix\>
 */
int web_tcl_ipv4pfx_to_mask(ClientData client_data, Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv4pfx-to-masklen command.
 *
 * tms::ipv4pfx-to-masklen \<IPv4 prefix\>
 */
int web_tcl_ipv4pfx_to_masklen(ClientData client_data,
                               Tcl_Interp *interpreter,
                               int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv4addr-to-num command.
 *
 * tms::ipv4addr-to-num \<IPv4 address\>
 */
int web_tcl_ipv4addr_to_num(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::mask-to-masklen command.
 *
 * tms::mask-to-masklen \<IPv4 address\>
 */
int web_tcl_mask_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::masklen-to-mask command.
 *
 * tms::masklen-to-mask \<IPv4 masklen\>
 */
int web_tcl_masklen_to_mask(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::ipv4addr-to-masklen command.
 *
 * tms::ipv4addr-to-masklen \<IPv4 address\>
 */
int web_tcl_ipv4addr_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::masklen-to-ipv4addr command.
 *
 * tms::masklen-to-ipv4addr \<IPv4 masklen\>
 */
int web_tcl_masklen_to_ipv4addr(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv6addr-to-masklen command.
 *
 * tms::ipv6addr-to-masklen \<IPv6 address\>
 */
int web_tcl_ipv6addr_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::masklen-to-ipv6addr command.
 *
 * tms::masklen-to-ipv6addr \<IPv6 masklen\>
 */
int web_tcl_masklen_to_ipv6addr(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::inetaddr-to-masklen command.
 *
 * tms::inetaddr-to-masklen \<IPv4 or IPv6 address\>
 */
int web_tcl_inetaddr_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::masklen-to-inetaddr command.
 *
 * tms::masklen-to-inetaddr [-4] [-6] \<IPv4 or IPv6 masklen\>
 */
int web_tcl_masklen_to_inetaddr(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);
/**
 * Implementation of the TCL tms::ipv4prefix-to-ip command.
 *
 * tms::ipv4prefix-to-ip \<IPv4 prefix\>
 */
int web_tcl_ipv4prefix_to_ip(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv4prefix-to-mask command.
 *
 * tms::ipv4prefix-to-mask \<IPv4 prefix\>
 */
int web_tcl_ipv4prefix_to_mask(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv4prefix-to-masklen command.
 *
 * tms::ipv4prefix-to-masklen \<IPv4 prefix\>
 */
int web_tcl_ipv4prefix_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv6prefix-to-ip command.
 *
 * tms::ipv6prefix-to-ip \<IPv6 prefix\>
 */
int web_tcl_ipv6prefix_to_ip(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv6prefix-to-mask command.
 *
 * tms::ipv6prefix-to-mask \<IPv6 prefix\>
 */
int web_tcl_ipv6prefix_to_mask(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::ipv6prefix-to-masklen command.
 *
 * tms::ipv6prefix-to-masklen \<IPv6 prefix\>
 */
int web_tcl_ipv6prefix_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::inetprefix-to-ip command.
 *
 * tms::inetprefix-to-ip \<IPv4 or IPv6 prefix\>
 */
int web_tcl_inetprefix_to_ip(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::inetprefix-to-mask command.
 *
 * tms::inetprefix-to-mask \<IPv4 or IPv6 prefix\>
 */
int web_tcl_inetprefix_to_mask(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::inetprefix-to-masklen command.
 *
 * tms::inetprefix-to-masklen \<IPv4 or IPv6 prefix\>
 */
int web_tcl_inetprefix_to_masklen(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::inetaddr-to-family command.
 *
 * tms::inetaddr-to-family [-inetip] \<IPv4 or IPv6 address\>
 *
 * where -inetip is optional, and means to return "IP" for IPv4
 * addresses, instead of "IPv4" (the default).
 */
int web_tcl_inetaddr_to_family(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::inetaddrz-to-family command.
 *
 * tms::inetaddrz-to-family [-inetip] \<IPv6 zone scoped address\>
 *
 * where -inetip is optional, and means to return "IP" for IPv4
 * addresses, instead of "IPv4" (the default).
 */
int web_tcl_inetaddrz_to_family(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::inetprefix-to-family command.
 *
 * tms:inetprefix-to-family [-inetip] \<IPv4 or IPv6 prefix\>
 *
 * where -inetip is optional, and means to return "IP" for IPv4
 * addresses, instead of "IPv4" (the default).
 */
int web_tcl_inetprefix_to_family(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::is-ipv4addr command.  Returns "true"
 * or "false".
 *
 * tms::is-ipv4addr \<string\>
 */
int web_tcl_is_ipv4addr(ClientData client_data,
                        Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::is-ipv4addr command.  Returns "true"
 * or "false".
 *
 * tms::is-ipv6addr \<string\>
 */
int web_tcl_is_ipv6addr(ClientData client_data,
                        Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::is-inetaddr command.  Returns "true"
 * or "false".
 *
 * tms::is-inetaddr \<string\>
 */
int web_tcl_is_inetaddr(ClientData client_data,
                        Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::inetaddr-is-linklocal command.
 * Returns "true" or "false".  If the string is not an inetaddr, returns
 * "false".
 *
 * tms::inetaddr-is-linklocal \<string\>
 */
int web_tcl_inetaddr_is_linklocal(ClientData client_data,
                                  Tcl_Interp *interpreter,
                                  int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::time-to-counter command.
 *
 * tms::time-to-counter \<time\>
 */
int web_tcl_time_to_counter(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::time-to-counter-ms command.
 *
 * tms::time-to-counter-ms \<time\>
 */
int web_tcl_time_to_counter_ms(ClientData client_data,
                               Tcl_Interp *interpreter,
                               int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-version command.
 *
 * tms::get-version
 */
int web_tcl_get_version(ClientData client_data,
                        Tcl_Interp *interpreter,
                        int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::have-prod-feature command.
 *
 * tms::have-prod-feature \<feature\>
 */
int web_tcl_have_prod_feature(ClientData client_data,
                              Tcl_Interp *interpreter,
                              int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::may-obfuscate command.
 *
 * tms::may-obfuscate
 */
int web_tcl_may_obfuscate(ClientData client_data,
                          Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::if-primary-name command.
 *
 * tms::if-primary-name
 */
int web_tcl_if_primary_name(ClientData client_data,
                            Tcl_Interp *interpreter,
                            int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::is-production command.
 *
 * tms::is-production
 */
int web_tcl_is_production(ClientData client_data,
                          Tcl_Interp *interpreter,
                          int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::is-authenticated command.
 *
 * tms::is-authenticated
 */
int web_tcl_is_authenticated(ClientData client_data,
                             Tcl_Interp *interpreter,
                             int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::get-uptime command.
 *
 * tms::get-uptime
 */
int web_tcl_get_uptime(ClientData client_data,
                       Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);


#ifdef PROD_FEATURE_CAPABS
/**
 * Implementation of the TCL tms::gid-to-capab command.
 *
 * tms::gid-to-capab \<gid\>
 */
int web_tcl_gid_to_capab(ClientData client_data,
                         Tcl_Interp *interpreter,
                         int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::capab-to-gid command.
 *
 * tms::capab-to-gid \<capability\>
 */
int web_tcl_capab_to_gid(ClientData client_data,
                         Tcl_Interp *interpreter,
                         int objc, Tcl_Obj *CONST objv[]);
#endif /* PROD_FEATURE_CAPABS */


/**
 * Implementation of the TCL tms::request-scheme command.
 *
 * tms::request_scheme
 */
int web_tcl_request_scheme(ClientData client_data,
                           Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::emit-etc-issue command.
 *
 * tms::emit-etc-issue
 */
int web_tcl_emit_etc_issue(ClientData client_data,
                           Tcl_Interp *interpreter,
                           int objc, Tcl_Obj *CONST objv[]);

/**
 * Implementation of the TCL tms::redirect-url command.
 *
 * tms::redirect-url \<url\>
 */
int web_tcl_redirect_url(ClientData client_data,
                         Tcl_Interp *interpreter,
                         int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::get-progress-opers-by-type command.
 *
 * tms::get-progress-opers-by-type \<type\>
 *
 * Returns a list of strings with the operation IDs of the operations 
 * found.
 */
int web_tcl_get_progress_opers_by_type(ClientData client_data,
                                       Tcl_Interp *interpreter,
                                       int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::dump-env command.
 *
 * tms::dump-env
 *
 * Dumps the contents of the environment to the logs.
 */
int web_tcl_dump_env(ClientData client_data,
                     Tcl_Interp *interpreter,
                     int objc, Tcl_Obj *CONST objv[]);


/**
 * Implementation of the TCL tms::time-to-counter-ex command.
 *
 * tms::time-to-counter-ex time_ms [render_fmt] [render_fmt] [render_fmt]
 *
 * Converts time in milliseconds to properly formatted string.
 * This function is a wrapper to lt_dur_ms_to_counter_str_ex2
 * function. One could specify the format of the output string.
 * Allowed formats are:
 *   none (default)
 *   fixed
 *   long_words
 *   packed
 *
 * One could specify multiple rendering formats and they will be ORed.
 * Invalid formats are ignored but logged as errors.
 */
int web_tcl_time_to_counter_ex(ClientData client_data, Tcl_Interp *interpreter,
                       int objc, Tcl_Obj *CONST objv[]);

#ifdef __cplusplus
}
#endif

#endif /* __WEB_INTERNAL_H_ */

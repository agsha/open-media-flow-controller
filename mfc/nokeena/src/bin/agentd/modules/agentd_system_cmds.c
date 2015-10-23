#include "agentd_mgmt.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* Some timezone values are defined in a different name in DDL
   because of redefinition errors thrown by DDL compiler.
   Those timezone strings should be mapped to the ones defined in TM 
*/
static agentd_string_map clock_timezone_map[] = {
{"Etc/GMT+Plus+1", "Etc/GMT+1"}, 
{"Etc/GMT+Plus+2", "Etc/GMT+2"},
{"Etc/GMT+Plus+3", "Etc/GMT+3"},
{"Etc/GMT+Plus+4", "Etc/GMT+4"},
{"Etc/GMT+Plus+5", "Etc/GMT+5"},
{"Etc/GMT+Plus+6", "Etc/GMT+6"},
{"Etc/GMT+Plus+7", "Etc/GMT+7"},
{"Etc/GMT+Plus+8", "Etc/GMT+8"},
{"Etc/GMT+Plus+9", "Etc/GMT+9"},
{"Etc/GMT+Plus+10","Etc/GMT+10"},
{"Etc/GMT+Plus+11","Etc/GMT+11"},
{"Etc/GMT+Plus+12","Etc/GMT+12"},
{"Etc/GMT-Minus-0","Etc/GMT-0"},
{"Etc/GMT-Minus-1","Etc/GMT-1"},
{"Etc/GMT-Minus-2","Etc/GMT-2"},
{"Etc/GMT-Minus-3","Etc/GMT-3"},
{"Etc/GMT-Minus-4","Etc/GMT-4"},
{"Etc/GMT-Minus-5","Etc/GMT-5"},
{"Etc/GMT-Minus-6","Etc/GMT-6"},
{"Etc/GMT-Minus-7","Etc/GMT-7"},
{"Etc/GMT-Minus-8","Etc/GMT-8"},
{"Etc/GMT-Minus-9","Etc/GMT-9"},
{"Etc/GMT-Minus-10","Etc/GMT-10"},
{"Etc/GMT-Minus-11","Etc/GMT-11"},
{"Etc/GMT-Minus-12","Etc/GMT-12"},
{"Etc/GMT-Minus-13","Etc/GMT-13"},
{"Etc/GMT-Minus-14","Etc/GMT-14"},
{NULL, NULL},
};

/* ---- Function prototype for initializing / deinitializing module ---- */

int
agentd_system_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_system_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */

/* TODO - Handling cleanup of array context by modules */
extern  agentd_array_move_context license_key;   //Context for license key
extern  agentd_array_move_context nameserver_ctxt;  //Context for nameserver configuration
extern  agentd_array_move_context domainlist_ctxt;  // Context for domain-list configuration

int agentd_system_set_timezone (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int agentd_system_set_license (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int agentd_system_set_resolver (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int agentd_system_delete_params (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int agentd_system_set_route (agentd_context_t * context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int agentd_system_delete_route (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */

/* ---- Translation table array for configuration commands ---- */
static  mfc_agentd_nd_trans_tbl_t system_cmds_trans_tbl[] =  {

TRANSL_ENTRY(PREPEND_TRANS_STR"systemlicense*")
TRANSL_CUST (agentd_system_set_license, "/license/key")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemhostname*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/system/hostname",
		TYPE_HOSTNAME,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemtelnet-server*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/xinetd/service/telnet/enable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemntpserver*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/ntp/server/address/%s",
		TYPE_HOSTNAME,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipname-server*")
TRANSL_CUST (agentd_system_set_resolver, "/resolver/nameserver")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipdomain-list*")
TRANSL_CUST (agentd_system_set_resolver, "/resolver/domain_search")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipdefault-gatewayipaddress*")
TRANSL_NUM_NDS(2)
TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/type",
		TYPE_STR,
		ND_HARDCODED,
		0, "gw")
TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/gw",
		TYPE_IPADDRV4,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipdefault-gatewayinterface*")
TRANSL_NUM_NDS(2)
TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "interface")
TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipdefault-gatewayipdest-interface*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemiproute*")
TRANSL_CUST (agentd_custom_entry_ignore, "" )
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemiproute*next-hopipaddress*")
TRANSL_CUST (agentd_system_set_route, "gw")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemiproute*next-hopinterface*")
TRANSL_CUST (agentd_system_set_route, "interface")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemiproute*next-hopipdest-interface*")
TRANSL_CUST (agentd_system_set_route, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemclocktimezone*")
TRANSL_CUST (agentd_system_set_timezone, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemlicense*")
TRANSL_CUST (agentd_system_delete_params, "/license/key")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemhostname*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/system/hostname",
		TYPE_HOSTNAME,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemtelnet-server*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/xinetd/service/telnet/enable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemntpserver*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS_BASIC ("/ntp/server/address/%s",
		TYPE_HOSTNAME,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipname-server*")
TRANSL_CUST (agentd_system_delete_params, "/resolver/nameserver")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipdomain-list*")
TRANSL_CUST (agentd_system_delete_params, "/resolver/domain_search")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipdefault-gatewayipaddress*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "gw")
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/gw",
                TYPE_IPADDRV4,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipdefault-gatewayinterface*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "interface")
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipdefault-gatewayipdest-interface*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv4/prefix/0.0.0.0\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemiproute*")
TRANSL_CUST (agentd_system_delete_route, "route")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemiproute*next-hopipaddress*")
TRANSL_CUST (agentd_system_delete_route, "gw")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemiproute*next-hopinterface*")
TRANSL_CUST (agentd_system_delete_route, "interface")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemiproute*next-hopipdest-interface*")
TRANSL_CUST (agentd_system_delete_route, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemclocktimezone*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/time/zone",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemservicemod-deliverysnapshot*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/system/config/coredump",
                TYPE_UINT32,
                ND_HARDCODED,
                0, "1")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemservicemod-deliverysnapshot*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/system/config/coredump",
                TYPE_UINT32,
                ND_HARDCODED,
                0, "1")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemservicemod-deliverystatus*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/system/config/init/preread",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemservicemod-deliverystatus*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/system/config/init/preread",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6default-gatewayipaddress*")
TRANSL_NUM_NDS(2)
TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "unicast")
TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/gw",
                TYPE_IPADDRV6,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6default-gatewayinterface*")
TRANSL_NUM_NDS(3)
TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "unicast")
TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/gw",
                TYPE_IPADDRV6,
                ND_HARDCODED,
                0, "::")
TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6default-gatewayipdest-interface*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6default-gatewayipaddress*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "unicast")
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/gw",
                TYPE_IPADDRV6,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6default-gatewayinterface*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/type",
                TYPE_STR,
                ND_HARDCODED,
                0, "unicast")
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6default-gatewayipdest-interface*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/net/routes/config/ipv6/prefix/::\\/0/nh/1/interface",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6route*")
TRANSL_CUST (agentd_custom_entry_ignore, "" )
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6route*next-hopipaddress*")
TRANSL_CUST (agentd_system_set_route, "unicast-address")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6route*next-hopinterface*")
TRANSL_CUST (agentd_system_set_route, "unicast-interface")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemipv6route*next-hopipdest-interface*")
TRANSL_CUST (agentd_system_set_route, "unicast-dest-interface")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6route*")
TRANSL_CUST (agentd_system_delete_route, "unicast-route")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6route*next-hopipaddress*")
TRANSL_CUST (agentd_system_delete_route, "unicast-address")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6route*next-hopinterface*")
TRANSL_CUST (agentd_system_delete_route, "unicast-interface")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemipv6route*next-hopipdest-interface*")
TRANSL_CUST (agentd_system_delete_route, "unicast-dest-interface")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY_NULL
};

/*---------------------------------------------------------------------------*/
int 
agentd_system_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (system_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_system_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_system_cmds_deinit called");

bail:
    return err;
}

int agentd_system_set_timezone (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff){
    int err = 0;
    temp_buf_t tzone = {0};
    const char * tm_timezone = NULL;

    /* args[1] contains the timezone */
    snprintf(tzone, sizeof(tzone), "%s", br_cmd->arg_vals.val_args[1]);

    /* Get TM timezone string for 'tzone'.
       If no mapping is available, then set 'tzone' as TM timezone 
    */
    tm_timezone = agentd_util_get_tm_str (clock_timezone_map, tzone);
    if (tm_timezone) {
        snprintf(tzone, sizeof(tzone), "%s", tm_timezone);
    }
    err = agentd_append_req_msg (context, "/time/zone", SUBOP_MODIFY, TYPE_STR, tzone);
    bail_error (err);

bail:
    return err;
}

int agentd_system_set_license (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff){
    int err = 0;
    uint32_array *key_indices = NULL;
    node_name_t node_name = {0};
    temp_buf_t key = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;

    bail_null(cb_arg);

    /* args[1] has the license key */
    snprintf(key, sizeof(key), "%s",  br_cmd->arg_vals.val_args[1]);
    snprintf(node_name, sizeof(node_name),"%s",  (char *)cb_arg);

    /* Get the index of the particular key */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
                        node_name, NULL, "license_key", key,2,
                        bn_db_revision_id_none,
                        NULL, &key_indices, NULL, NULL);
    bail_error (err);

    if (key_indices && uint32_array_length_quick(key_indices) > 0) {
        /* License is already installed. Ignore request to install again*/
        goto bail;
    }

    err = bn_binding_array_new(&barr);
    bail_error_null(err, barr);
    err = bn_binding_new_str_autoinval(&binding, "license_key", ba_value, bt_string, 0,
               key); 
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    err = agentd_array_append(agentd_mcc, &license_key, context->mod_bindings,
            node_name, barr, &code, NULL);
    bail_error(err);

bail:
    uint32_array_free(&key_indices);
    bn_binding_array_free(&barr);
    return err;
}

int agentd_system_set_resolver (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t node_value = {0};
    const char * bname = NULL; 
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    agentd_array_move_context *arr_ctxt = NULL;
    nd_dt_type_t btype = TYPE_MAX;

    bail_null (cb_arg);

    /* args[1] has nameserver or domain name*/
    snprintf(node_value, sizeof(node_value), "%s",  br_cmd->arg_vals.val_args[1]);
    snprintf(node_name, sizeof(node_name), "%s", (char *)cb_arg);

    if (strcmp(node_name, "/resolver/nameserver") == 0) {
        bname = "inetz_address";
        btype = TYPE_INETADDRZ;
        arr_ctxt = &nameserver_ctxt;
    } else { /* domain search */
        bname = "domainname";
        btype = TYPE_HOSTNAME;
        arr_ctxt = &domainlist_ctxt;
    }

    err = bn_binding_array_new(&barr);
    bail_error_null(err, barr);
    err = bn_binding_new_str_autoinval(&binding, bname, ba_value, btype, 0,
               node_value);
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    err = agentd_array_append(agentd_mcc, arr_ctxt, context->mod_bindings,
            node_name, barr, &code, NULL);
    bail_error(err);

bail:
    bn_binding_array_free(&barr);
    return err;
}

int agentd_system_delete_params (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t node_value = {0};
    const char *bname = NULL;
    uint32_array *indices = NULL;
    uint32 num_index = 0;
    node_name_t node_to_delete = {0};
    int index_pos = 0;

    bail_null (cb_arg);

    //args[1] has the nameserver or domain name
    snprintf(node_value, sizeof(node_value), "%s", br_cmd->arg_vals.val_args[1]);
    snprintf(node_name, sizeof(node_name),"%s", (char *)(cb_arg));

    if (strcmp(node_name, "/resolver/nameserver") == 0){
        bname = "inetz_address";
        index_pos = 2;
    } else if (strcmp (node_name, "/resolver/domain_search") == 0){ /* domain_search */
        bname = "domainname";
        index_pos = 2;
    } else if (strcmp (node_name, "/license/key") == 0 ) {
        bname = "license_key";
        index_pos = 2;
    } else {
        lc_log_basic (jnpr_log_level, "Invalid case");
        goto bail;
    }

    /* Get the index of the particular key */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
                        node_name, NULL, bname, node_value, index_pos,
                        bn_db_revision_id_none,
                        NULL, &indices, NULL, NULL);
    bail_error_null(err, indices);

    num_index = uint32_array_length_quick(indices);
    if(num_index == 0){
        /* Ignore. Cannot happen */
    } else {
        snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d",
                    node_name, uint32_array_get_quick(indices,0));
        err = agentd_append_req_msg (context, node_to_delete, SUBOP_DELETE, bt_string, "");
        bail_error(err);
    }
bail:
    uint32_array_free(&indices);
    return err;
}

int agentd_system_set_route (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t parent = {0}, node_name = {0};
    temp_buf_t route = {0}, nexthop = {0};
    uint32 route_prefix = 0;
    char * ch = NULL;
    const char * type = (char *)cb_arg;

    //args[1] has the route with prefix
    snprintf(route, sizeof(route), "%s", br_cmd->arg_vals.val_args[1]);
    ch = strchr (route, '/');
    if (ch) {
        route_prefix = atoi (ch+1);
        *ch = '\0';
    } else {
        lc_log_basic (LOG_ERR, "Route does not have prefix");
        err = 1;
        bail_error (err);
    }
    //args[2] has the nexthop or destination interface 
    snprintf(nexthop, sizeof(nexthop), "%s", br_cmd->arg_vals.val_args[2]);

    if (strstr (type, "unicast")) { /* IPV6 route configuration */
        snprintf(parent, sizeof(parent), "/net/routes/config/ipv6/prefix/%s\\/%d/nh/1",
						route, route_prefix);
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, "unicast");
        bail_error (err);
    } else {
        snprintf(parent, sizeof(parent), "/net/routes/config/ipv4/prefix/%s\\/%d/nh/1",
						route, route_prefix);
    }

    if (strcmp(type, "gw") == 0) {/* ip or interface is configured as next hop */
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, type);
        bail_error (err);
        snprintf(node_name, sizeof(node_name), "%s/%s", parent, type);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_IPADDRV4, nexthop);
        bail_error (err);
    } else if (strcmp(type, "interface") == 0) { /* interface is configured as next hop */
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, type);
        bail_error (err);
        snprintf(node_name, sizeof(node_name), "%s/%s", parent, type);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, nexthop);
        bail_error (err);
    } else if (strcmp(type, "unicast-address") == 0) { /* ipv6 address is configured */
        snprintf(node_name, sizeof(node_name), "%s/gw", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_IPADDRV6, nexthop);
        bail_error (err);
    } else if (strcmp(type, "unicast-interface") == 0) { /* interface is configured for ipv6 route next-hop */
        snprintf(node_name, sizeof(node_name), "%s/gw", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_IPADDRV6, "::");
        bail_error(err);
        snprintf(node_name, sizeof(node_name), "%s/interface", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, nexthop);
        bail_error (err);
    } else { /* destination interface is configured for gw or unicast type */ 
        snprintf(node_name, sizeof(node_name), "%s/interface", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, nexthop);
        bail_error (err);
    }

bail:
    return err;
}

int agentd_system_delete_route (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t parent = {0}, node_name = {0};
    temp_buf_t route = {0};
    uint32 route_prefix = 0;
    const char * type = (char *)cb_arg;
    char * ch = NULL;

    //args[1] has the route with prefix
    snprintf(route, sizeof(route), "%s", br_cmd->arg_vals.val_args[1]);
    ch = strchr (route, '/');
    if (ch) {
        route_prefix = atoi (ch+1);
        *ch = '\0';
    } else {
        lc_log_basic (LOG_ERR, "Route does not have prefix");
        err = 1;
        bail_error (err);
    }

    if (strstr (type, "unicast")) { /* IPV6 route deletion */
        snprintf(parent, sizeof(parent), "/net/routes/config/ipv6/prefix/%s\\/%d/nh/1",
                                                route, route_prefix);
    } else {
        snprintf(parent, sizeof(parent), "/net/routes/config/ipv4/prefix/%s\\/%d/nh/1",
						route, route_prefix);
    }

    if (strcmp(type, "gw") == 0) { /* ip next hop is deleted */
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, type);
        bail_error (err);
        snprintf(node_name, sizeof(node_name), "%s/%s", parent, type);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_IPADDRV4, "");
        bail_error (err);
    } else if (strcmp(type, "interface") == 0) { /* interface next hop is deleted */
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, type);
        bail_error (err);
        snprintf(node_name, sizeof(node_name), "%s/%s", parent, type);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, "");
        bail_error (err);
    } else if (strcmp(type, "route") == 0) { /* entire route is deleted */
        snprintf(node_name, sizeof(node_name), "/net/routes/config/ipv4/prefix/%s\\/%d",
                                           route, route_prefix);
        err = agentd_append_req_msg (context, node_name, SUBOP_DELETE, TYPE_IPADDRV4, "");
        bail_error (err);
    } else if (strcmp(type, "unicast-address") == 0) { /* ipv6 route next hop address is deleted */
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, type);
        bail_error (err);
        snprintf(node_name, sizeof(node_name), "%s/gw", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_IPADDRV6, "");
        bail_error (err);
    } else if (strcmp(type, "unicast-interface") == 0) { /* ipv6 route next hop interface is deleted */
        snprintf(node_name, sizeof(node_name), "%s/type", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, type);
        bail_error (err);
        snprintf(node_name, sizeof(node_name), "%s/interface", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, "");
        bail_error (err);
    } else if (strcmp(type, "unicast-route") == 0) { /* entire ipv6 route is deleted */
        snprintf(node_name, sizeof(node_name), "/net/routes/config/ipv6/prefix/%s\\/%d",
                                           route, route_prefix);
        err = agentd_append_req_msg (context, node_name, SUBOP_DELETE, TYPE_IPADDRV6, "");
        bail_error (err);
    } else { /* destination interface configured for gw or unicast type is deleted */ 
        snprintf(node_name, sizeof(node_name), "%s/interface", parent);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, TYPE_STR, "");
        bail_error (err);
    }

bail:
    return err;
}

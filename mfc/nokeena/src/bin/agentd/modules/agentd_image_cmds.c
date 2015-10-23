#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"
#include "type_conv.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_image_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_image_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 

#define POS_IMAGE_URL 4 /* Position of image download URL in oper_cmd pattern */
#define POS_NEXTBOOT_PART_ID 4 /* Position of next boot partition Id in oper_cmd pattern */

#define IMAGE_DWNLD_DIR "/var/opt/tms/images"

#define PARTITION_ID_1 1
#define PARTITION_ID_2 2

int agentd_show_partition_details (agentd_context_t *context, void *data);
int agentd_op_req_reboot_system (agentd_context_t *context, void *data);
int agentd_op_req_image_install (agentd_context_t *context, void *data);
static int agentd_show_installed_image (agentd_context_t * context, const bn_binding_array * bindings, int partition_num);

/*---------------------------------------------------------------------------*/
int 
agentd_image_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    /* Translation table array for operational commands */
    oper_table_entry_t op_tbl[] = {
        {"/reboot/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_op_req_reboot_system, NULL},
        {"/reboot/mfc-cluster/*/next-boot-partition/*", DUMMY_WC_ENTRY, agentd_op_req_reboot_system, NULL},
        {"/image-install/mfc-cluster/*/url/*", DUMMY_WC_ENTRY, agentd_op_req_image_install, NULL},
        {"/image-install/mfc-cluster/*/url/*/delete-image", DUMMY_WC_ENTRY, agentd_op_req_image_install, NULL},
        {"/partition-info/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_show_partition_details, NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_op_cmds_array (op_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_image_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_image_cmds_deinit called");

bail:
    return err;
}

/* This function handles the patterns  */
// "/image-install/mfc-cluster/*/url/*"
// "/image-install/mfc-cluster/*/url/*/delete-image"
int agentd_op_req_image_install(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    node_name_t pattern = {0};
    const char * mfc_cluster_name = NULL;
    const char * url = NULL;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    char * image_name = NULL;
    const char * last_arg = NULL;
    int num_params = 0;
    tbool delete_image = false;
    temp_buf_t unquoted_url = {0};

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME) ;
    url = tstr_array_get_str_quick (context->op_cmd_parts, POS_IMAGE_URL);

    bail_null (mfc_cluster_name);
    bail_null (url);

    /* VJX sends URL within quotes. Remove the quotes before sending mdc requests */
    sscanf (url, "\"%s\"", unquoted_url);
    unquoted_url[strlen(unquoted_url)-1] = '\0';

    lc_log_debug (jnpr_log_level, "MFC cluster %s, url %s", mfc_cluster_name, unquoted_url);

    num_params = tstr_array_length_quick(context->op_cmd_parts);
    last_arg = tstr_array_get_str_quick (context->op_cmd_parts, num_params-1);
    bail_null (last_arg);

    if (strcmp(last_arg, "delete-image") == 0) {
        delete_image = true;
    }

    /* Download file */
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                "/file/transfer/download", 4,
                "remote_url", bt_string, unquoted_url,
                "local_dir", bt_string, IMAGE_DWNLD_DIR,
                "allow_overwrite", bt_bool, "true",
                "mode", bt_uint16, "0600");
    bail_error(err);

    lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
    if (ret_code != 0) {
        goto bail;
    }

    /* Install image */
    image_name = strrchr(unquoted_url, '/') + 1;
    bail_null (image_name);
    lc_log_debug (jnpr_log_level, "image name :%s", image_name);
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                  "/image/install", 3,
                  "image_name", bt_string, image_name,
                  "install_bootmgr", bt_bool, "false",
                  "use_tmpfs", bt_bool, "false");
    bail_error (err);
    lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
    if (ret_code != 0) {
        goto bail;
    }

    /* If delete-image is set, then delete the downloaded image from file system */
    if (delete_image) {
        err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                  "/file/delete", 2,
                  "local_dir", bt_string, IMAGE_DWNLD_DIR,
                  "local_filename", bt_string, image_name);
        bail_error (err);
        lc_log_debug (jnpr_log_level, "mdc_send_action - Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
        if (ret_code != 0) {
            goto bail;
        }
    }

bail:
    if (ret_code != 0) {
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
    }
    return err;
}

/* This function handles the following patterns */
// "/reboot/mfc-cluster/*"
// "/reboot/mfc-cluster/*/next-boot-partition/*"
int agentd_op_req_reboot_system(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    uint32 length = 0;
    const char *mfc_cluster_name = NULL;
    const char *boot_part_str = NULL;
    uint32 boot_part_id = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);

    length = tstr_array_length_quick(context->op_cmd_parts);
    mfc_cluster_name =  tstr_array_get_str_quick (context->op_cmd_parts, POS_CLUSTER_NAME);
    bail_null (mfc_cluster_name);

    if (length > POS_CLUSTER_NAME+1) {
        /* Next boot partition is set. Send the action to set it in MFC */
        boot_part_str = tstr_array_get_str_quick (context->op_cmd_parts, POS_NEXTBOOT_PART_ID);
        bail_null (boot_part_str);

        err = lc_str_to_uint32(boot_part_str, &boot_part_id);
        bail_error (err);

        if (boot_part_id != 1 && boot_part_id != 2) {
            set_ret_code (context, 1);
            set_ret_msg (context, "Invalid next boot partition id");
            goto bail;
        }

        err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                                                 "/image/boot_location/nextboot/set", 1,
                                                 "location_id", bt_uint32, boot_part_str);
        bail_error (err);
        lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
        if (ret_code != 0) goto bail;
    }

    err = mdc_send_action(agentd_mcc, &ret_code, &ret_msg, "/pm/actions/reboot");
    bail_error(err);
    lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
    if (ret_code != 0) goto bail;

bail :
    if (ret_code != 0) {
        set_ret_code (context, ret_code);
        set_ret_msg (context, ts_str(ret_msg));
    }
    return err;
}

int agentd_show_partition_details (agentd_context_t *context, void *data){
/* This function handles the following command pattern */
// "/partition-info/mfc-cluster/*"
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    const char * mfc_cluster_name = NULL;
    const char * node = "/image";
    bn_binding_array *bindings = NULL;
    tstring * last_boot = NULL;
    tstring * next_boot = NULL;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME) ;
    bail_null (mfc_cluster_name);

    err = mdc_get_binding_children(agentd_mcc, NULL, NULL, true, &bindings,
                                  true, true, node);
    bail_error_null(err, bindings);
    bn_binding_array_dump("IMAGE-BINDINGS", bindings, jnpr_log_level);

    err = agentd_binding_array_get_value_tstr_fmt (bindings, &last_boot,  "/image/boot_location/booted/location_id");
    bail_error (err);
    err = agentd_binding_array_get_value_tstr_fmt (bindings, &next_boot,  "/image/boot_location/nextboot/location_id");
    bail_error (err);

    OP_EMIT_TAG_START (context, ODCI_MFC_PARTITION_INFO);
        OP_EMIT_TAG_VALUE (context, ODCI_LAST_BOOT_PARTITION, ts_str(last_boot));
        OP_EMIT_TAG_VALUE (context, ODCI_NEXT_BOOT_PARTITION, ts_str(next_boot));
        err = agentd_show_installed_image (context, bindings, PARTITION_ID_1);
        bail_error (err);
        err = agentd_show_installed_image (context, bindings, PARTITION_ID_2);
        bail_error (err);
    OP_EMIT_TAG_END (context, ODCI_MFC_PARTITION_INFO);

bail:
    ts_free (&next_boot);
    ts_free (&last_boot);
    bn_binding_array_free (&bindings);
    return err;
}

static int agentd_show_installed_image (agentd_context_t * context, const bn_binding_array * bindings, int partition_num) {
    int err = 0;
    tstring * image_name = NULL;
    str_value_t partition_id_str = {0};

    bail_null (context);
    bail_null (bindings);

    if (partition_num != PARTITION_ID_1 && partition_num != PARTITION_ID_2) {
        lc_log_basic (LOG_WARNING, "Invalid partition number");
        goto bail;
    }
    snprintf (partition_id_str, sizeof(partition_id_str), "%d", partition_num);

    err = agentd_binding_array_get_value_tstr_fmt (bindings, &image_name, 
                                "/image/info/installed_image/curr_device/location_id/%d/build_prod_version", partition_num);
    bail_error (err);

    OP_EMIT_TAG_START (context, ODCI_PARTITION_DETAIL);
        OP_EMIT_TAG_VALUE (context, ODCI_ID, partition_id_str);
        OP_EMIT_TAG_VALUE (context, ODCI_INSTALLED_IMAGE, 
                                    image_name? ts_str(image_name): "Could not determine image version -- install in progress ?");
    OP_EMIT_TAG_END (context, ODCI_PARTITION_DETAIL);

bail: 
    ts_free (&image_name);
    return err;
}

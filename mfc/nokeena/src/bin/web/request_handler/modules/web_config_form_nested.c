/*
 *
 * Filename:  web_config_form_nested.c
 * Date:      2008/12/26
 *
 * Nokeena Networks, Inc.  
 *
 */

#include <string.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_internal.h"
#include "web_config_form.h"
#include "md_client.h"

/*-----------------------------------------------------------------------------
 * INSTRUCTIONS
 *
 * This module provides generic wildcard/nested support for web templates.
 * This module only works for unordered wildcards for example:
 *
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wcf_action_name[] = "config-form-nested";
static const char wcf_id_nested_levels[] = "nested_levels";
static const char wcf_id_nested_root_prefix[] = "nested_root_";
static const char wcf_id_nested_index_prefix[] = "nested_index_";
static const char wcf_id_nested_fields_prefix[] = "nested_children_";

static const char wcf_btn_add[] = "add";
static const char wcf_btn_remove[] = "remove";
static const char wcf_row_prefix[] = "v_row_";

/*-----------------------------------------------------------------------------
 * STRUCTURES
 */

/*
 * XXX/EMT: this looks very generic and should be declared at
 * some higher level.
 */
typedef struct wcf_remove_data {
    uint32 wrd_code;
    tstring *wrd_msg;
} wcf_remove_data;

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wcf_remove_child(web_param_source src, const tstring *name,
                            const tstring *value, void *param);

static int wcf_handle_add(void);
static int wcf_handle_remove(void);

web_action_result
wcf_config_form_nested_handler(web_context *ctx, void *param);

int web_config_form_nested_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_handle_add(void)
{
    int err = 0;
    bn_request *req = NULL;
    uint32 i = 0, num_levels = 0;
    tstring *msg = NULL;
    tstring *nested_levels = NULL;
    tstr_array *levels = NULL;
    char *nested_root_base = NULL;
    tstring *root = NULL;
    char *root_base = NULL;
    bn_binding *binding = NULL;
    tstr_array *fields = NULL;
    tstr_array *nested_fields = NULL;
    const tstring *display_name = NULL;
    const tstring *field = NULL;
    const tstring *nested_field = NULL;
    char *node_name = NULL;
    char *nested_name = NULL;
    char *nested_root_fn = NULL, *nested_index_fn = NULL;
    char *nested_fields_fn = NULL;
    tstring *windex = NULL;
    tstring *windex_value = NULL;
    char *windex_value_esc = NULL;
    tstring *value = NULL; 
    tstring *fields_str = NULL;
    tstring *nested_value_fields_str = NULL;
    tstring *nested_field_value = NULL;
    char *nested_field_value_esc = NULL;
    bn_type type = bt_none;
    tbool required = false;
    uint32 code = 0; 
    uint32 num_fields = 0;
    uint32 num_nested_fields = 0;
    uint32 j = 0;
 

    err = web_get_trimmed_form_field_str(g_web_data, wcf_id_nested_levels,
                                         &nested_levels);
    bail_error(err);

    /* create the message */
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);
    
    num_levels = atoi(ts_str(nested_levels));
    for (i = 0; i < num_levels; i++) {
        /* get the root */
        nested_root_fn = smprintf("%s%d",wcf_id_nested_root_prefix,i+1);
        bail_null(nested_root_fn);
      

        err = web_get_trimmed_form_field_str(g_web_data, 
                                        nested_root_fn, &root);
        bail_error_null(err, root);

        /* get the index */
        nested_index_fn = smprintf("%s%d", wcf_id_nested_index_prefix, i+1);
        bail_null(nested_index_fn);

        err = web_get_trimmed_form_field_str(g_web_data, nested_index_fn,
                                         &windex);
        bail_error_null(err, windex);

        /* get the index value */
        err = web_get_trimmed_form_field(g_web_data, windex, &windex_value);
        bail_error_null(err, windex_value);
        if (ts_length(windex_value) == 0 )
        {
            break;
        }

        err = bn_binding_name_escape_str(ts_str(windex_value), &windex_value_esc);
        bail_error_null(err, windex_value_esc);

        /* get the fields to process */
        nested_fields_fn = smprintf("%s%d", wcf_id_nested_fields_prefix, i+1);
        bail_null(nested_fields_fn);

        err = web_get_trimmed_form_field_str(g_web_data, nested_fields_fn,
                                         &fields_str);
        //bail_error_null(err, fields_str);
        if (fields_str != NULL ) {
            err = ts_tokenize(fields_str, ',', 0, 0,
                          ttf_ignore_leading_separator |
                          ttf_ignore_trailing_separator |
                          ttf_single_empty_token_null, &fields);
            bail_error(err);
        }

        /* create the main node */
        err = web_get_type_for_form_field(g_web_data, windex, &type);
        bail_error(err);

        if (root_base != NULL) {
            node_name = smprintf("%s/%s/%s", root_base, 
                                  ts_str(root), windex_value_esc);
            bail_null(node_name);
        } 
        else {
            node_name = smprintf("%s/%s", ts_str(root), windex_value_esc);
            bail_null(node_name);
        }
        root_base = smprintf("%s",node_name);
    

        err = bn_binding_new_str_autoinval(
            &binding, node_name, ba_value, type, 0, ts_str(windex_value));
        bail_error(err);
        safe_free(node_name);

        err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
        bail_error(err);
        bn_binding_free(&binding);

        /* go through all the fields */
        if (fields != NULL) {
            err = tstr_array_length(fields, &num_fields);
            bail_error(err);
        }
        else {
            num_fields = 0;
        }
        for (j = 0; j < num_fields; ++j) {
            field = tstr_array_get_quick(fields, j);
            bail_null(field);

            err = web_get_type_for_form_field(g_web_data, field, &type);
            bail_error(err);

            err = web_get_trimmed_form_field(g_web_data, field, &value);
            bail_error(err);

            if ((value == NULL) && (type == bt_bool)) {
            /* XXX should we really default to false if no value? */
                err = ts_new_str(&value, "false");
                bail_error(err);
            }
#if 0
            else {
            lc_log_basic(LOG_NOTICE,"Bail Error Null : %s",ts_str(value));
               bail_null(value);
            }
#endif

            err = web_is_required_form_field(g_web_data, field, &required);
            bail_error(err);

            /*
             * check if the field is required. if the field is required
             * and there is no value, inform the user. if the field is not
             * required and there is no value, don't insert a node for it
             * so that the mgmt module fills in the default value.
             */
            if (required && ts_length(value) == 0) {
                err = web_get_field_display_name(g_web_data, field, &display_name);
                bail_error(err);
    
                code = 1;
                err = ts_new_sprintf(&msg, _("No value specified for %s."),
                                     ts_str(display_name));
                bail_error(err);
                goto bail;
            } else if (!required && ts_length(value) == 0) {
                goto loop_clean;
            }

            node_name = smprintf("%s/%s", root_base,
                                 ts_str(field));
            bail_null(node_name);

            err = bn_binding_new_str_autoinval(
                &binding, node_name, ba_value, type, 0, ts_str(value));
            bail_error(err);

            err = bn_set_request_msg_append(req, bsso_modify, 0, 
                                            binding, NULL);
            bail_error(err);
     loop_clean:
            tstr_array_free(&nested_fields);
            ts_free(&value);
            ts_free(&nested_value_fields_str);
            ts_free(&nested_field_value);
            safe_free(nested_field_value_esc);
            safe_free(node_name);
            safe_free(nested_name);
            bn_binding_free(&binding);
       }
       bn_binding_free(&binding);
       tstr_array_free(&fields);
       tstr_array_free(&nested_fields);
       ts_free(&nested_value_fields_str);
       ts_free(&root); 
       ts_free(&windex);
       ts_free(&windex_value);
       safe_free(windex_value_esc);
       ts_free(&value);
       ts_free(&fields_str);
       ts_free(&nested_field_value);
       safe_free(nested_field_value_esc);
       safe_free(node_name);
       safe_free(nested_name);
       safe_free(nested_root_fn);
       safe_free(nested_index_fn);
       safe_free(nested_fields_fn);
   }

   if (!code) {
        err = mdc_send_mgmt_msg(web_mcc, 
                           req, false, NULL, &code, &msg);
        bail_error(err);
   }

 
 check_error:    
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        err = web_clear_post_data(g_web_data);
        bail_error(err);
    }

 bail:
    ts_free(&msg);
    ts_free(&nested_levels);
    bn_request_msg_free(&req);
    return(err);
}

/* ------------------------------------------------------------------------- */

static int 
wcf_remove_child(web_param_source src, const tstring *name,
                 const tstring *value, void *param)
{
    wcf_remove_data *remove_data = (wcf_remove_data *)param;
    tstring *id = NULL;
    tstring *msg = NULL;
    char *field_name = NULL;
    uint32 code = 0;
    int err = 0;

    bail_null(name);
    bail_null(value);
    bail_null(remove_data);

    if (remove_data->wrd_code) {
        goto bail;
    }

    if (ts_has_prefix_str(name, wcf_row_prefix, false)) {
        err = ts_substr(name, strlen(web_value_prefix), -1, &id);
        bail_error(err);

        field_name = smprintf("%s%s", web_field_prefix, ts_str(id));
        bail_null(field_name);

        if (!web_request_param_exists_str(g_web_data, field_name,
                                          ps_post_data)) {
            goto bail;
        }

        err = mdc_delete_binding(web_mcc, 
                                 &code, &msg, ts_str(value));
        bail_error(err);
        if (code) {
            remove_data->wrd_code = code;
            remove_data->wrd_msg = msg;
            msg = NULL;
            goto bail;
        }
    }
            
 bail:
    ts_free(&msg);
    ts_free(&id);
    safe_free(field_name);
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
wcf_handle_remove(void)
{
    wcf_remove_data remove_data;
    int err = 0;

    remove_data.wrd_code = 0;
    remove_data.wrd_msg = NULL;
    
    err = web_foreach_request_param(g_web_data, ps_post_data, wcf_remove_child,
                                    &remove_data);
    bail_error(err);

 check_error:
    err = web_set_msg_result(g_web_data, remove_data.wrd_code,
                             remove_data.wrd_msg);
    bail_error(err);

 bail:
    ts_free(&(remove_data.wrd_msg));
    return(err);
}

/* ------------------------------------------------------------------------- */


web_action_result
wcf_config_form_nested_handler(web_context *ctx, void *param)
{
    int err = 0;

    if (web_request_param_exists_str(g_web_data, wcf_btn_add, ps_post_data)) {
        err = wcf_handle_add();
        bail_error(err);
    } else if (web_request_param_exists_str(g_web_data, wcf_btn_remove,
                                            ps_post_data)) {
        err = wcf_handle_remove();
        bail_error(err);

        err = web_clear_post_data(g_web_data);
        bail_error(err);
    } 
    
 bail:
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }
}

/* ------------------------------------------------------------------------- */

int
web_config_form_nested_init(const lc_dso_info *info, void *data)
{
    web_context *ctx = NULL;
    int err = 0;

    bail_null(data);

    /*
     * the context contains useful information including a pointer
     * to the TCL interpreter so that modules can register their own
     * TCL commands.
     */
    ctx = (web_context *)data;

    /*
     * register all commands that are handled by this module.
     */
    err = web_register_action_str(g_rh_data, wcf_action_name, NULL,
                                  wcf_config_form_nested_handler);
    bail_error(err);

 bail:
    return(err);
}

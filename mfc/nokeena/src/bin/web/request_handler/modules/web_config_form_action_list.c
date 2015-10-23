/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

static const char rcsid[] = "$Id: web_config_form_list.c,v 1.26 2008/12/29 19:56:01 et Exp $";

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
 * This module provides generic wildcard/list support for web templates.
 * This module only works for unordered wildcards for example:
 *
 * /ntp/server/address/<IPV4>
 *
 * Adds:
 *
 *   The submit button for the add form should be named 'add'.
 *
 *   In addition, the following hidden input fields are required:
 *
 *      name=f_list_root       value=parent-node-name-of-new-entry
 *      name=f_list_index      value=field-to-use-for-index
 *      name=f_list_children   value=comma-separated-list-of-children
 *
 *   So for the ntp example above:
 *
 *      <CFG-IPV4 -id "addr" ...>
 *      <CFG-TEXTBOX -id "version" ...>
 *
 *      <input type="hidden" name="f_list_root" value="/ntp/server/address">
 *      <input type="hidden" name="f_list_index" value="addr">
 *      <input type="hidden" name="f_list_children" value="version">
 *
 *      <input type="submit" name="add" value="Add NTP Server">
 *
 *   On submit, this module keys off the name of the button 'add', and
 *   looks up the list of children in the hidden field "f_list_children".
 *   Then the module looks up form fields for each of these children and
 *   then adds a new array child to the root "/ntp/server/address" with 
 *   sub children matching the names given in "f_list_children":
 *
 *      /ntp/server/address/<addr>
 *                         /version
 *
 *   Error checking is done automatically as well as whether or not a 
 *   required parameter was filled in or not. You can mark a parameter as
 *   required by using the CFG-TEXTBOX's required attribute.
 *
 *   Error messages are automatically generated as well and stuck in the
 *   usual error custom variable 'v_error'.
 *
 *   The following optional fields are also available:
 *
 *      name=f_list_child_<child_name>_list
 *      name=f_list_clear_after_success
 *
 *   The 'f_list_child_<child_name>_list' value allows you to specify that
 *   a child is a list and should be populated using the fields given in
 *   the value. So for example, if you had the following node structure:
 *
 *      /foo/ * /bar/ *
 *
 *   and you want to create a new 'foo' and at the same time populate an
 *   entry under 'bar' via a value inside a field named 'barvalue':
 *
 *      <input type="hidden" name="f_list_child_bar_list" value="barvalue">
 *
 *   The result of this is when the entry under /foo is created, an entry
 *   under /foo/ * /bar will be created as well with the value taken from
 *   the barvalue input field.
 *
 *   The 'f_list_clear_after_success' when set to true, will clear the form
 *   values after a successful submit. It defaults to true.
 *
 *   Another optional field is:
 *
 *      name=f_list_others
 *
 *   The 'f_list_others' value is a comma separated (if more than one)
 *   list of suffixes to append to field names to allow for multiple
 *   'f_list_root's (and associated fields) if desired.
 *
 *   For example, 'f_list_others' could have a value of '_2,_3'.
 *   This would cause the form to process fields:
 *       'f_list_root_2,f_list_index_2,f_list_children_2'
 *   and 'f_list_root_3,f_list_index_3,f_list_children_3'
 *   in the same way as the first 'f_list_root' (and friends).
 *
 *   One limitation with this additional capability is that the
 *   'f_list_children*' must still refer to forms in this single
 *   config form.
 *
 * Removes:
 *
 *   The submit button for the remove form should be named 'remove'.
 *
 *   This feature facilitates the removal of a list of wildcard nodes in
 *   a web template. This feature keys off of a CFG-CHECKBOX for each
 *   entry in the array.
 *
 *   The id attribute of the CFG-CHECKBOX should begin with "row_".
 *
 *   The value attribute of the CFG-CHECKBOX should contain the full node
 *   name of that particular child. For example:
 *
 *      <CFG-CHECKBOX -id "row_1" ... -value "/ntp/server/address/10.0.0.1">
 *
 *   So for the ntp example:
 *
 *      <CFG-CHECKBOX -id "row_1" ... -value "/ntp/server/address/10.0.0.1">
 *      <CFG-CHECKBOX -id "row_2" ... -value "/ntp/server/address/10.0.0.2">
 *      <CFG-CHECKBOX -id "row_3" ... -value "/ntp/server/address/10.0.0.3">
 *
 *      <input type="submit" name="remove" value="Remove Selected Server">
 *
 *   On submit, for example if you had the 2nd item checked, this module
 *   would delete the /ntp/server/address/10.0.0.2 node.
 *
 * Custom:
 *
 *   This module also supports some minor custom functionality for each
 *   item in an array list.
 *
 *   The base table should be created just like the remove with checkboxes
 *   that follow the same format as removes:
 *
 *      <CFG-CHECKBOX -id "row_1" ... -value "/ntp/server/address/10.0.0.1">
 *
 *   Then the following hidden fields are necessary:
 *
 *      name=f_list_custom_buttons  value=comma-separated-list-of-buttons
 *      name=f_list_button_*_action      value=action-to-perform
 *      name=f_list_button_*_child_name  value=child-name-to-perform-on
 *      name=f_list_button_*_child_value value=child-value-for-action
 *
 *   So this could be useful for example to provide Enable/Disable buttons
 *   to toggle on various NTP servers:
 *
 *      <CFG-CHECKBOX -id "row_1" ... -value "/ntp/server/address/10.0.0.1">
 *      <CFG-CHECKBOX -id "row_2" ... -value "/ntp/server/address/10.0.0.2">
 *      <CFG-CHECKBOX -id "row_3" ... -value "/ntp/server/address/10.0.0.3">
 *
 *      <input type="hidden" name="f_list_custom_buttons"
 *             value="enable,disable">
 *      <input type="hidden" name="f_list_button_enable_action" value="set">
 *      <input type="hidden" name="f_list_button_enable_child_name"
 *             value="enable">
 *      <input type="hidden" name="f_list_button_enable_child_value"
 *             value="true">
 *      <input type="hidden" name="f_list_button_disable_action" value="set">
 *      <input type="hidden" name="f_list_button_disable_child_name"
 *             value="enable">
 *      <input type="hidden" name="f_list_button_disable_child_value"
 *             value="false">
 *
 *      <input type="submit" name="enable" value="Enable Server">
 *      <input type="submit" name="disable" value="Disable Server">
 *
 *   On submission, for example if the 2nd server was checked, this module
 *   would change the value of /ntp/server/address/10.0.0.2/enable to "true"
 *   if the Enable button was clicked or /ntp/server/address/10.0.0.2/enable
 *   to "false" if the Disable button was clicked.
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wcf_action_name[] = "config-form-list-action";
static const char wma_action_name[] = "config-form-list-action";
static const char wcf_id_list_others[] = "list_others";
static const char wcf_id_list_root[] = "list_root";
static const char wcf_id_list_index[] = "list_index";
static const char wcf_id_list_buttons[] = "list_custom_buttons";
static const char wcf_id_list_fields[] = "list_children";
static const char wcf_id_list_clear_after_success[] =
    "list_clear_after_success";
static const char wcf_btn_add[] = "add";
static const char wcf_btn_remove[] = "remove";
static const char wcf_row_prefix[] = "v_row_";
static const char wcf_button_prefix[] = "f_list_button_";
static const char wcf_action_suffix[] = "_action";
static const char wcf_child_name_suffix[] = "_child_name";
static const char wcf_child_value_suffix[] = "_child_value";
static const char wcf_child_prefix[] = "f_list_child_";
static const char wcf_child_list_suffix[] = "_list";
static const char wcf_set_action[] = "set";

static const char wma_field_action_binding[] = "mfd_action_binding";
static const char wma_field_append_binding[] = "mfd_append_list";
static const char wma_name_prefix[] = "n_";
static const char wma_type_prefix[] = "t_";
static const char wma_value_prefix[] = "v_";

/*-----------------------------------------------------------------------------
 * STRUCTURES
 */

typedef struct wcf_custom_data {
    uint32 wcd_code;
    tstring *wcd_msg;
    tstring *wcd_action;
    tstring *wcd_child_name;
    tstring *wcd_child_value;
} wcf_custom_data;

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

static int wcf_custom_child_handler(web_param_source src, const tstring *name,
                                    const tstring *value, void *param);

static int wcf_remove_child(web_param_source src, const tstring *name,
                            const tstring *value, void *param);

static int wcf_handle_add(void);
static int wcf_do_list_root(bn_request *req, const char *list_sfx,
                            tstring **inout_msg, uint32 *ret_code);
static int wcf_handle_remove(void);
static int wcf_handle_custom_buttons(void);

web_action_result
wcf_config_form_list_handler(web_context *ctx, void *param);

//int web_config_form_list_init(const lc_dso_info *info, void *data);
int web_config_form_action_list_init(const lc_dso_info *info, void *data);

static web_action_result wma_action_handler(web_context *ctx, void *param);

int web_config_form_action_init(const lc_dso_info *info, void *data);
/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_handle_add(void)
{
    int err = 0;
    bn_request *req = NULL;
    uint32 code = 0;
    uint32 i = 0, num_others = 0;
    tstring *msg = NULL;
    tstring *clear_str = NULL;
    tstring *list_others = NULL;
    tstr_array *others = NULL;
    const char *suffix = NULL;
    const bn_response *resp = NULL;
    tstring *action = NULL;
    tstr_array *fields = NULL;
    const tstring *field = NULL;
    bn_type type = bt_none;
    tstring *field_value = NULL;
    tstring *fields_str = NULL;
    uint32 num_fields = 0;
    /* check whether or not we need to clear after a success */
    err = web_get_trimmed_form_field_str(g_web_data,
                                         wcf_id_list_clear_after_success,
                                         &clear_str);
    bail_error(err);

    err = web_get_trimmed_form_field_str(g_web_data, wcf_id_list_others,
                                         &list_others);
    bail_error(err);

    if (list_others) {
        err = ts_tokenize(list_others, ',', 0, 0,
                          ttf_ignore_leading_separator |
                          ttf_ignore_trailing_separator |
                          ttf_single_empty_token_null, &others);
        bail_error(err);
    }
 
    /* create the message */
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = wcf_do_list_root(req, "", &msg, &code);
    bail_error(err);

    if (code) {
        goto check_error;
    }

    if (others) {
        num_others = tstr_array_length_quick(others);
        for (i = 0; i < num_others; i++) {
            suffix = tstr_array_get_str_quick(others, i);
            bail_null(suffix);

            err = wcf_do_list_root(req, suffix, &msg, &code);
            bail_error(err);

            if (code) {
                goto check_error;
            }
        }
    }

    if (!code) {
        err = mdc_send_mgmt_msg(web_mcc, 
                                req, false, NULL, &code, &msg);
        bail_error(err);
    }

    /*If Action specified Action handler. (Added for virtual player to load default values for new virtual player)*/
    err = web_get_trimmed_form_field_str(g_web_data, wma_field_action_binding,
                                    &action);
    bail_error(err);

    if (!action || ts_length(action) <= 0) {
        goto action_bail;
    }
    err = bn_action_request_msg_create(&req, ts_str(action));
    bail_error(err);

    err = web_get_trimmed_form_field_str(g_web_data, wma_field_append_binding,
                                         &fields_str);
    bail_error(err);

    if (fields_str != NULL ) {
        err = ts_tokenize(fields_str, ',', 0, 0,
                      ttf_ignore_leading_separator |
                      ttf_ignore_trailing_separator |
                      ttf_single_empty_token_null, &fields);
        bail_error(err);
        /* go through all the fields */
        if (fields != NULL) {
            err = tstr_array_length(fields, &num_fields);
            bail_error(err);
        }
        for (i = 0; i < num_fields; ++i) {
            field = tstr_array_get_quick(fields, i);
            bail_null(field);
            err = web_get_trimmed_form_field_str(g_web_data, ts_str(field),
                                        &field_value);
            bail_error(err);

            err = web_get_type_for_form_field(g_web_data, field, &type);
            bail_error(err);

            err = bn_action_request_msg_append_new_binding(
                        req, 0, ts_str(field), 
                        type, ts_str(field_value), NULL);
            bail_error(err);
         
            ts_free(&field_value);
 
 
        }
        ts_free(&fields_str);
        tstr_array_free(&fields);
    }
   
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);

    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);

    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }

action_bail:
    bn_request_msg_free(&req);
    ts_free(&msg);
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }

check_error:    
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        if (clear_str == NULL || !ts_equal_str(clear_str, "false", false)) {
            err = web_clear_post_data(g_web_data);
            bail_error(err);
        }
    }
    /*Adding action code here*/

 bail:
    ts_free(&msg);
    ts_free(&clear_str);
    ts_free(&list_others);
    bn_request_msg_free(&req);
    tstr_array_free(&others);
    return(err);
}

static int
wcf_do_list_root(bn_request *req, const char *list_sfx, tstring **inout_msg,
                 uint32 *ret_code)
{
    bn_binding *binding = NULL;
    tstr_array *fields = NULL;
    tstr_array *list_fields = NULL;
    const tstring *display_name = NULL;
    const tstring *field = NULL;
    const tstring *list_field = NULL;
    char *node_name = NULL;
    char *list_name = NULL;
    char *list_root_fn = NULL, *list_index_fn = NULL;
    char *list_fields_fn = NULL;
    tstring *root = NULL;
    tstring *windex = NULL;
    tstring *windex_value = NULL;
    char *windex_value_esc = NULL;
    tstring *value = NULL;
    tstring *fields_str = NULL;
    tstring *list_value_fields_str = NULL;
    tstring *list_field_value = NULL;
    char *list_field_value_esc = NULL;
    bn_type type = bt_none;
    tbool required = false;
    uint32 code = 0;
    uint32 num_fields = 0;
    uint32 num_list_fields = 0;
    uint32 i = 0;
    uint32 j = 0;
    int err = 0;
    tstring *msg = NULL;

    bail_null(req);
    bail_null(inout_msg);
    bail_null(ret_code);

    /* get the root */
    list_root_fn = smprintf("%s%s", wcf_id_list_root, list_sfx);
    bail_null(list_root_fn);

    err = web_get_trimmed_form_field_str(g_web_data, list_root_fn, &root);
    bail_error_null(err, root);

    /* get the index */
    list_index_fn = smprintf("%s%s", wcf_id_list_index, list_sfx);
    bail_null(list_index_fn);

    err = web_get_trimmed_form_field_str(g_web_data, list_index_fn,
                                         &windex);
    bail_error_null(err, windex);

    /* get the index value */
    err = web_get_trimmed_form_field(g_web_data, windex, &windex_value);
    bail_error_null(err, windex_value);
    if((ts_length(windex_value)==0)){
	    lc_log_basic(LOG_NOTICE,"Windex_value is null\n");
            code = 1;
	    err = ts_new_sprintf(&msg, _("Please enter a valid name"));
            bail_error(err);
            err = web_set_msg_result(g_web_data,code,msg);
            bail_error(err);
	    ts_free(&msg);
	    goto bail;
    }
    
    err = bn_binding_name_escape_str(ts_str(windex_value), &windex_value_esc);
    bail_error_null(err, windex_value_esc);

    /* get the fields to process */
    list_fields_fn = smprintf("%s%s", wcf_id_list_fields, list_sfx);
    bail_null(list_fields_fn);

    err = web_get_trimmed_form_field_str(g_web_data, list_fields_fn,
                                         &fields_str);
    bail_error_null(err, fields_str);

    err = ts_tokenize(fields_str, ',', 0, 0,
                      ttf_ignore_leading_separator |
                      ttf_ignore_trailing_separator |
                      ttf_single_empty_token_null, &fields);
    bail_error(err);

    /* create the main node */
    err = web_get_type_for_form_field(g_web_data, windex, &type);
    bail_error(err);

    node_name = smprintf("%s/%s", ts_str(root), windex_value_esc);
    bail_null(node_name);

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

    for (i = 0; i < num_fields; ++i) {
        field = tstr_array_get_quick(fields, i);
        bail_null(field);
        /*
         * if list values exist for this field, then this field is a
         * wildcard so we need to populate with other fields.
         */
        list_name = smprintf("%s%s%s", wcf_child_prefix, ts_str(field),
                             wcf_child_list_suffix);
        bail_null(list_name);

        err = web_get_trimmed_value_str(g_web_data, list_name,
                                        &list_value_fields_str);
        bail_error(err);

        if (list_value_fields_str) {
            err = ts_tokenize(list_value_fields_str, ',', 0, 0,
                              ttf_ignore_leading_separator |
                              ttf_ignore_trailing_separator |
                              ttf_single_empty_token_null, &list_fields);
            bail_error(err);

            if (list_fields) {
                err = tstr_array_length(list_fields, &num_list_fields);
                bail_error(err);
            }

            for (j = 0; j < num_list_fields; ++j) {
                list_field = tstr_array_get_quick(list_fields, j);
                bail_null(list_field);

                err = web_get_type_for_form_field(g_web_data, list_field,
                                                  &type);
                bail_error(err);

                err = web_get_trimmed_form_field(g_web_data, list_field,
                                                 &list_field_value);
                bail_error_null(err, list_field_value);

                err = bn_binding_name_escape_str(ts_str(list_field_value),
                                                 &list_field_value_esc);
                bail_error_null(err, list_field_value_esc);

                err = web_is_required_form_field(g_web_data, list_field,
                                                 &required);
                bail_error(err);

                /*
                 * check if the field is required. if the field is required
                 * and there is no value, inform the user. if the field is not
                 * required and there is no value, don't insert a node for it
                 * so that the mgmt module fills in the default value.
                 */
                if (required && ts_length(list_field_value) == 0) {
                    err = web_get_field_display_name(g_web_data, list_field,
                                                     &display_name);
                    bail_error(err);
                       
                    code = 1;
                    err = ts_new_sprintf(inout_msg,
                                         _("No value specified for %s."),
                                         ts_str(display_name));
                    bail_error(err);
                    goto bail;
                } else if (!required && ts_length(list_field_value) == 0) {
                    goto list_loop_clean;
                }

                node_name = smprintf("%s/%s/%s/%s",
                                     ts_str(root), windex_value_esc,
                                     ts_str(field), list_field_value_esc);
                bail_null(node_name);
                err = bn_binding_new_str_autoinval(
                    &binding, node_name, ba_value, type, 0,
                    ts_str(list_field_value));
                bail_error(err);

                err = bn_set_request_msg_append(
                    req, bsso_modify, 0, binding, NULL);
                bail_error(err);

 list_loop_clean:
                ts_free(&list_field_value);
                safe_free(list_field_value_esc);
                safe_free(node_name);
                bn_binding_free(&binding);
            }

            goto loop_clean;
        }

        err = web_get_type_for_form_field(g_web_data, field, &type);
        bail_error(err);

        err = web_get_trimmed_form_field(g_web_data, field, &value);
        bail_error(err);

        if ((value == NULL) && (type == bt_bool)) {
            /* XXX should we really default to false if no value? */
            err = ts_new_str(&value, "false");
            bail_error(err);
        }
        else {
            bail_null(value);
        }

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
            err = ts_new_sprintf(inout_msg, _("No value specified for %s."),
                                 ts_str(display_name));
            bail_error(err);
            goto bail;
        } else if (!required && ts_length(value) == 0) {
            goto loop_clean;
        }

        node_name = smprintf("%s/%s/%s", ts_str(root), windex_value_esc,
                             ts_str(field));
        bail_null(node_name);

        err = bn_binding_new_str_autoinval(
            &binding, node_name, ba_value, type, 0, ts_str(value));
        bail_error(err);

        err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
        bail_error(err);

 loop_clean:
        tstr_array_free(&list_fields);
        ts_free(&value);
        ts_free(&list_value_fields_str);
        ts_free(&list_field_value);
        safe_free(list_field_value_esc);
        safe_free(node_name);
        safe_free(list_name);
        bn_binding_free(&binding);
    }

 bail:
    *ret_code = code;
    bn_binding_free(&binding);
    tstr_array_free(&fields);
    tstr_array_free(&list_fields);
    ts_free(&list_value_fields_str);
    ts_free(&root);
    ts_free(&windex);
    ts_free(&windex_value);
    safe_free(windex_value_esc);
    ts_free(&value);
    ts_free(&fields_str);
    ts_free(&list_field_value);
    safe_free(list_field_value_esc);
    safe_free(node_name);
    safe_free(list_name);
    safe_free(list_root_fn);
    safe_free(list_index_fn);
    safe_free(list_fields_fn);
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

static int 
wcf_custom_child_handler(web_param_source src, const tstring *name,
                         const tstring *value, void *param)
{
    wcf_custom_data *custom_data = (wcf_custom_data *)param;
    bn_binding *binding = NULL;
    tstring *id = NULL;
    tstring *msg = NULL;
    bn_type type = bt_none;
    char *field_name = NULL;
    char *node_name = NULL;
    uint32 code = 0;
    int err = 0;

    bail_null(name);
    bail_null(value);
    bail_null(custom_data);

    if (custom_data->wcd_msg) {
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

        node_name = smprintf("%s/%s",
                             ts_str(value),
                             ts_str(custom_data->wcd_child_name));
        bail_null(node_name);

        if (ts_equal_str(custom_data->wcd_action, "set", true)) {
            err = mdc_get_binding(web_mcc, &code, &msg,
                                  false, &binding, node_name, NULL);
            bail_error(err);
            if (code) {
                custom_data->wcd_code = code;
                custom_data->wcd_msg = msg;
                msg = NULL;
                goto bail;
            } else { ts_free(&msg); }
	    if (NULL == binding) {
		    lc_log_basic(LOG_INFO, "Invalid object while setting for '%s'", ts_str(value));
		    goto bail;
	    }

            err = bn_binding_get_type(binding, ba_value, &type, NULL);
            bail_error(err);

            err = mdc_modify_binding(web_mcc, 
                                     &code, &msg, type,
                                     ts_str(custom_data->wcd_child_value),
                                     node_name);
            bail_error(err);
            if (code) {
                custom_data->wcd_code = code;
                custom_data->wcd_msg = msg;
                msg = NULL;
                goto bail;
            } else { ts_free(&msg); }
        }
    }
            
 bail:
    bn_binding_free(&binding);
    ts_free(&id);
    ts_free(&msg);
    safe_free(field_name);
    safe_free(node_name);
    return(err);    
}

/* ------------------------------------------------------------------------- */

static int
wcf_handle_custom_buttons(void)
{
    wcf_custom_data custom_data;
    tstr_array *buttons = NULL;
    tstring *buttons_str = NULL;
    tstring *button = NULL;
    tstring *action = NULL;
    tstring *child_name = NULL;
    tstring *child_value = NULL;
    char *action_name = NULL;
    char *child_name_name = NULL;
    char *child_value_name = NULL;
    uint32 num_buttons = 0;
    uint32 i = 0;
    int err = 0;

    custom_data.wcd_code = 0;
    custom_data.wcd_msg = NULL;

    /* get the list of custom buttons */
    err = web_get_trimmed_form_field_str(g_web_data, wcf_id_list_buttons,
                                         &buttons_str);
    bail_error_null(err, buttons_str);

    err = ts_tokenize(buttons_str, ',', 0, 0,
                      ttf_ignore_leading_separator |
                      ttf_ignore_trailing_separator |
                      ttf_single_empty_token_null, &buttons);
    bail_error(err);

    /* go through the buttons and find the one that was pressed */
    if (buttons) {
        err = tstr_array_length(buttons, &num_buttons);
        bail_error(err);
    }

    for (i = 0; i < num_buttons; ++i) {
        err = tstr_array_get(buttons, i, &button);
        bail_error_null(err, button);

        /* check if this was the button that was pressed */
        if (!web_request_param_exists(g_web_data, button, ps_post_data)) {
            goto loop_clean;
        }

        /* get the action for this button */
        action_name = smprintf("%s%s%s", wcf_button_prefix, ts_str(button),
                               wcf_action_suffix);
        bail_null(action_name);

        err = web_get_trimmed_value_str(g_web_data, action_name, &action);
        bail_error_null(err, action);

        /* get the child name for this button */
        child_name_name = smprintf("%s%s%s", wcf_button_prefix, ts_str(button),
                                   wcf_child_name_suffix);
        bail_null(child_name_name);

        err = web_get_trimmed_value_str(g_web_data, child_name_name,
                                        &child_name);
        bail_error_null(err, child_name);

        /* get the child value for this button */
        child_value_name = smprintf("%s%s%s", wcf_button_prefix,
                                    ts_str(button),
                                    wcf_child_value_suffix);
        bail_null(child_value_name);

        err = web_get_trimmed_value_str(g_web_data, child_value_name,
                                        &child_value);
        bail_error_null(err, child_value);

        /* go through the marked items */
        custom_data.wcd_action = action;
        custom_data.wcd_child_name = child_name;
        custom_data.wcd_child_value = child_value;

        err = web_foreach_request_param(g_web_data, ps_post_data,
                                        wcf_custom_child_handler,
                                        &custom_data);
        bail_error(err);

        if (custom_data.wcd_code) {
            goto check_error;
        }

 loop_clean:
        ts_free(&(custom_data.wcd_msg));
        ts_free(&action);
        ts_free(&child_name);
        ts_free(&child_value);
        safe_free(action_name);
        safe_free(child_name_name);
        safe_free(child_value_name);
    }

 check_error:
    err = web_set_msg_result(g_web_data, custom_data.wcd_code,
                             custom_data.wcd_msg);
    bail_error(err);

 bail:
    /* don't free button as it's owned by buttons */
    tstr_array_free(&buttons);
    ts_free(&(custom_data.wcd_msg));
    ts_free(&buttons_str);
    ts_free(&action);
    ts_free(&child_name);
    ts_free(&child_value);
    safe_free(action_name);
    safe_free(child_name_name);
    safe_free(child_value_name);
    return(err);
}

/* ------------------------------------------------------------------------- */

web_action_result
wcf_config_form_list_handler(web_context *ctx, void *param)
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
    } else {
        err = wcf_handle_custom_buttons();
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
web_config_form_action_list_init(const lc_dso_info *info, void *data)
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
                                  wcf_config_form_list_handler);
    bail_error(err);
 bail:
    return(err);
}

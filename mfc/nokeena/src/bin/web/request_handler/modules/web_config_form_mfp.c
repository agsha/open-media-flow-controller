/*
 *
 * Filename:  web_config_form_mfp.c
 * Date: 
 * Author:    Dhivyamanohary R
 *
 * Juniper Networks, Inc
 */

#include <string.h>
#include <sys/inotify.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_internal.h"
#include "web_config_form.h"
#include "timezone.h"
#include <sys/queue.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "file_utils.h"


/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wcf_mfp_name[] = "config-form-action-mfp";
static const char wcf_field_name[] = "f_mfp";
static const char wcf_field_streamname[] = "f_streamname";
static const char wcf_field_media[] = "f_media";
static const char wcf_field_live_encap[] = "f_live-encapsulation";
static const char wcf_field_file_encap[] = "f_file-encapsulation";
static const char wcf_field_old_stream[] = "f_stream-apply";
static const char wcf_field_stream[] = "f_mfp";
static const char wcf_id_list_fields[] = "list_children";

static const char wcf_btn_publish[] = "publish";
static const char wcf_btn_mfpadd[] = "mfpadd";
static const char wcf_btn_mfpsave[] = "mfpsave";
static const char wcf_btn_stop[] = "stop";
static const char wcf_btn_status[] = "status";
static const char wcf_btn_remove[] = "mfpremove";
static const char wcf_btn_restart[] = "mfprestart";
static const char wcf_btn_mfpclose[] = "mfpclose";
#define INOTIFY_EVENT_SIZE sizeof(struct inotify_event)
#define EVENT_BUFLEN ((N_FD * INOTIFY_EVENT_SIZE) + 1024)
#define N_FD 1024
int32_t dir_watch_fd, dir_watch_wd;

/*Added to support the save option in PUBLISH button click 
 * and in the save option
 */
static const char wcf_child_name_suffix[] = "_child_name";
static const char wcf_child_value_suffix[] = "_child_value";
static const char wcf_child_prefix[] = "f_list_child_";
static const char wcf_child_list_suffix[] = "_list";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */
static int wcf_handle_mfpadd(void);
static int wcf_handle_mfpsave(void);
static int wcf_handle_publish(void);
static int wcf_handle_stop(void);
static int wcf_handle_status(void);
static int wcf_handle_remove(void);
static int wcf_handle_restart(void);
static web_action_result wcf_mfp_form_handler(web_context *ctx,
	void *param);
int web_config_form_mfp_init(const lc_dso_info *info, void *data);
static int web_control_response_directory_watch (tstring **filename);
static int wcf_handle_madd(void);
static int wcf_do_list_root(bn_request *req, const char *list_sfx,
                            tstring **inout_msg, uint32 *ret_code);
static int wcf_handle_save_close(void);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */
static int
wcf_handle_mfpadd(void)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *mfp_value = NULL;
    const tstring *action_value = NULL;
    const tstring *media_value = NULL;
    const tstring *live_encap_value = NULL;
    const tstring *file_encap_value = NULL;
    char *bn_mfp_name = NULL;
    char *bn_mfp_media = NULL;
    char *bn_mfp_encap = NULL;
    char *bn_mfp_status = NULL;
    char *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    tstring *profile_rec = NULL;
    uint32 code = 0;
    int err = 0;
    const tstring *old_stream_profile = NULL;

    err = web_get_request_param_str(g_web_data, wcf_field_streamname, ps_post_data,
	    &mfp_value);
    bail_error(err);

    if (mfp_value == NULL || ts_num_chars(mfp_value) <= 0) {
	err = ts_new_str(&msg, _("No session name entered."));
	bail_error(err);
        ret_err = 1;
        goto check_error;
    }

    err = web_get_request_param_str(g_web_data, wcf_field_media, ps_post_data,
	    &media_value);
    bail_error(err);

    err = web_get_request_param_str(g_web_data, wcf_field_file_encap, ps_post_data,
	    &file_encap_value);
    bail_error(err);

    err = web_get_request_param_str(g_web_data, wcf_field_live_encap, ps_post_data,
	    &live_encap_value);
    bail_error(err);

    err = web_get_request_param_str(g_web_data, wcf_field_old_stream, ps_post_data,
	    &old_stream_profile);
    bail_error(err);

    /*session name*/
    err = mdc_get_binding_tstr_fmt
	(web_mcc, NULL, NULL, NULL, &profile_rec, NULL,
	 "/nkn/nvsd/mfp/config/%s", ts_str(mfp_value));
    bail_error(err);
    if (profile_rec && ts_num_chars(profile_rec) > 0) {
	ret_msg = smprintf(_("Profile \"%s\" already exists."),
		ts_str(mfp_value));
	bail_null(ret_msg);
	err = web_set_msg_result_str(g_web_data, 1, ret_msg);
	bail_error(err);
	goto bail;
    }

    if(!ts_equal_str(old_stream_profile, "(none)", false)){
	bn_mfp_name = smprintf("/nkn/nvsd/mfp/config/%s", ts_str(old_stream_profile));
	bail_null(bn_mfp_name);
	err = mdc_reparent_binding_ex(web_mcc, &code, &msg,
		bn_mfp_name, NULL, ts_str(mfp_value), 0,
		bsnf_reparent_by_copy);
	bail_error(err);
	if (code) {
	    err = web_set_msg_result_str(g_web_data, code, ts_str(msg));
	    bail_error(err);
	    goto bail;
	}
    } else {
	/*Get the values from the  different fields*/
	if(ts_equal_str(media_value,"File",true) &&
		(ts_equal_str(file_encap_value,"not_selected",true))) {
	    err = ts_new_str(&msg, _("No encapsulation type selected for File media."));
	    bail_error(err);
	    ret_err = 1;
	    goto check_error;
	}
	if(ts_equal_str(media_value,"Live",true) &&
		(ts_equal_str(live_encap_value,"not_selected",true))) {
	    err = ts_new_str(&msg, _("No encapsulation type selected for Live media."));
	    bail_error(err);
	    ret_err = 1;
	    goto check_error;
	}

	/*session name*/
	bn_mfp_name= smprintf("/nkn/nvsd/mfp/config/%s",ts_str(mfp_value));
	bail_null(bn_mfp_name);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
		bsso_modify,bt_string,ts_str(mfp_value),
		bn_mfp_name);
	bail_error(err);
	if(ret_err) {
	    goto check_error;
	}

	/* media */
	bn_mfp_media=smprintf("/nkn/nvsd/mfp/config/%s/media-type",ts_str(mfp_value));
	bail_null(bn_mfp_name);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
		bsso_modify,bt_string,ts_str(media_value),
		bn_mfp_media);
	bail_error(err);
	if(ret_err) {
	    goto check_error;
	}

	/* encap */
	bn_mfp_encap=smprintf("/nkn/nvsd/mfp/config/%s/media-encap",ts_str(mfp_value));
	bail_null(bn_mfp_encap);
	if(ts_equal_str(media_value,"File",true)){
	    err = mdc_set_binding(web_mcc, &ret_err, &msg,
		    bsso_modify,bt_string,ts_str(file_encap_value),
		    bn_mfp_encap);
	    bail_error(err);
	    if(ret_err) {
		goto check_error;
	    }
	}else{
	    err = mdc_set_binding(web_mcc, &ret_err, &msg,
		    bsso_modify,bt_string,ts_str(live_encap_value),
		    bn_mfp_encap);
	    bail_error(err);
	    if(ret_err) {
		goto check_error;
	    }
	}
    }
    /* status */
    bn_mfp_status=smprintf("/nkn/nvsd/mfp/config/%s/status",ts_str(mfp_value));
    bail_null(bn_mfp_status);
    err = mdc_set_binding(web_mcc, &ret_err, &msg,
	    bsso_modify,bt_string,"NEW",
	    bn_mfp_status);
    bail_error(err);
    if(ret_err) {
	goto check_error;
    }
    /*session value name*/
    bn_mfp_name= smprintf("/nkn/nvsd/mfp/config/%s/name",ts_str(mfp_value));
    bail_null(bn_mfp_name);
    err = mdc_set_binding(web_mcc, &ret_err, &msg,
	    bsso_modify,bt_string,ts_str(mfp_value),
	    bn_mfp_name);
    bail_error(err);
    if(ret_err) {
	goto check_error;
    }
check_error:
    err = web_set_msg_result(g_web_data, ret_err,msg);
    bail_error(err);
    if (!ret_err) {
	err = web_set_msg_result(g_web_data,ret_err,msg);
	bail_error(err);
	err = web_clear_post_data(g_web_data);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&msg);
    safe_free(bn_mfp_name);
    safe_free(bn_mfp_media);
    safe_free(bn_mfp_encap);
    safe_free(bn_mfp_status);
    return(err);
}

static int
wcf_handle_publish(void)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *mfp_value = NULL;
    tstring *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    //get the mfp name

    err = web_get_request_param_str(g_web_data, wcf_field_name, ps_post_data,&mfp_value);
    bail_error(err);

    //calling the save function to save the data
    err = wcf_handle_mfpsave();
    if(err) {
	err = 0;
	goto bail;
    }

    //form req
    err = bn_action_request_msg_create(&req, "/nkn/nvsd/mfp/actions/publish");
    bail_error(err);

    //bind stream name with req
    err = bn_action_request_msg_append_new_binding(
	    req, 0, "stream-name", bt_string,ts_str(mfp_value), NULL);
    bail_error(err);

    //send msg
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);

    //get response
    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);

    if (code != 0) {
	goto check_error;
    } else {
	err = web_set_msg_result(g_web_data, code, msg);
	bail_error(err);
	ts_free(&msg);
    }
check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    ts_free(&msg);
    return(err);
}

static web_action_result
wcf_mfp_form_handler(web_context *ctx, void *param)
{
    int err = 0;
    
    if (web_request_param_exists_str(g_web_data, wcf_btn_publish, ps_post_data)) {
	/*
	 * BUG 6185: Handle the unsaved data on clicking the PUBLISH button event
	 */
	//err = wcf_handle_mfpsave();
        //bail_error(err);
	//web_dump_params(g_web_data);
        err = wcf_handle_publish();
        bail_error(err);
//	err = web_set_msg_result_str(g_web_data, 0, "<script>window.refresh();</script>");
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_mfpadd, ps_post_data)){
		err = wcf_handle_mfpadd();
		bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_stop, ps_post_data)){
		err = wcf_handle_stop();
		bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_status, ps_post_data)){
		err = wcf_handle_status();
		bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_remove, ps_post_data)){
		err = wcf_handle_remove();
		bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_mfpsave, ps_post_data)){
		err = wcf_handle_mfpsave();
		//bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_mfpclose, ps_post_data)){
		err = wcf_handle_mfpsave();
		//bail_error(err);
		err = wcf_handle_save_close();
		bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_restart, ps_post_data)){
		err = wcf_handle_restart();
		bail_error(err);
            	err = web_clear_post_data(g_web_data);
            	bail_error(err);
    }else {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }
	bail:
		if (err) {
			return(war_error);
		} else {
			return(war_ok);
		}
}

int
web_config_form_mfp_init(const lc_dso_info *info, void *data)
{
    Tcl_Command cmd = NULL;
    web_context *ctx = NULL;
    int err = 0;

    bail_null(data);

    /*
     * * the context contains useful information including a pointer
     * * to the TCL interpreter so that modules can register their own
     * * TCL commands.
     * */
    ctx = (web_context *)data;

    /*
     * * register all commands that are handled by this module.
     * */
    err = web_register_action_str(g_rh_data, wcf_mfp_name, NULL,
	    wcf_mfp_form_handler);
    bail_error(err);

bail:
    return(err);
}

static int
wcf_handle_stop(void)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *mfp_value = NULL;
    tstring *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    //get the mfp name
    err = web_get_request_param_str(g_web_data, wcf_field_stream, ps_post_data,&mfp_value);
    bail_error(err);

    //form req
    err = bn_action_request_msg_create(&req, "/nkn/nvsd/mfp/actions/stop");
    bail_error(err);
    //bind stream name with req
    err = bn_action_request_msg_append_new_binding(
	    req, 0, "stream-name", bt_string,ts_str(mfp_value), NULL);
    bail_error(err);
    //send msg
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);
    //get response
    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);

    if (code != 0) {
	goto check_error;
    } else {
	err = web_set_msg_result(g_web_data, code, msg);
	bail_error(err);
	ts_free(&msg);
    }
check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    ts_free(&msg);
    return(err);
}

static int
wcf_handle_status(void)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *mfp_value = NULL;
    tstring *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    //get the mfp name
    err = web_get_request_param_str(g_web_data, wcf_field_stream, ps_post_data,&mfp_value);
    bail_error(err);

    //form req
    err = bn_action_request_msg_create(&req, "/nkn/nvsd/mfp/actions/status");
    bail_error(err);
    //bind stream name with req
    err = bn_action_request_msg_append_new_binding(
	    req, 0, "stream-name", bt_string,ts_str(mfp_value), NULL);
    bail_error(err);
    //send msg
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);
    //get response
    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);
    if (code != 0) {
	goto check_error;
    } else {
	err = web_set_msg_result(g_web_data, code, msg);
	bail_error(err);
	ts_free(&msg);
    }
    /*Start the directory watch*/
    err = web_control_response_directory_watch(&msg);
check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    ts_free(&msg);
    return(err);
}

static int
wcf_handle_remove(void)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *mfp_value = NULL;
    tstring *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    //get the mfp name
    err = web_get_request_param_str(g_web_data, wcf_field_stream, ps_post_data,&mfp_value);
    bail_error(err);

    //form req
    err = bn_action_request_msg_create(&req, "/nkn/nvsd/mfp/actions/remove");
    bail_error(err);
    //bind stream name with req
    err = bn_action_request_msg_append_new_binding(
	    req, 0, "stream-name", bt_string,ts_str(mfp_value), NULL);
    bail_error(err);
    //send msg
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);
    //get response
    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);
    if (code != 0) {
	goto check_error;
    } else {
	err = web_set_msg_result(g_web_data, code, msg);
	bail_error(err);
	ts_free(&msg);
    }
check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    ts_free(&msg);
    return(err);
}

static int
web_control_response_directory_watch (tstring **filename)
{
    int err = 0;
    /* FD's for directory watching */
    char *file_name = NULL;
 
    lc_log_basic(LOG_DEBUG, _("Entering directory watch\n"));
    dir_watch_fd = inotify_init();
    dir_watch_wd = inotify_add_watch
	(dir_watch_fd, "/nkn/vpe/stat", IN_CLOSE_WRITE);

    /*Check for events*/
    while(1) {
	int32_t n_events, i;
	char event_buf[EVENT_BUFLEN];
	i = 0;
    	n_events = read(dir_watch_fd, event_buf, EVENT_BUFLEN);
	/*process fd list*/
	while(i < n_events) {
	lc_log_basic(LOG_DEBUG, _("Nevents:%d\n"), (int)n_events);
	    struct inotify_event *ev;
	    ev = (struct inotify_event *)(&event_buf[i]);
	    if(ev->len && (ev->mask & IN_CLOSE_WRITE)) {
		file_name = smprintf("/nkn/vpe/stat/%s", ev->name);
	lc_log_basic(LOG_DEBUG, _("Filename:%s\n"), file_name);
		break;
	    }
	    i += INOTIFY_EVENT_SIZE + ev->len;
	}
	if(file_name){
	    break;
	}
    }
    if(file_name) {
	lc_log_basic(LOG_DEBUG, _("Filename for status:%s\n"), file_name);
	err = lf_read_file_tstr(file_name, EVENT_BUFLEN, filename);
	bail_error(err);
    }

bail:
    return err;
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
    tstring *profile_rec = NULL;

    bail_null(req);
    bail_null(inout_msg);
    bail_null(ret_code);
    const tstring *mfp_value = NULL;

    err = web_get_request_param_str(g_web_data, wcf_field_name, ps_post_data,&mfp_value);
    bail_error(err);
    /* get the root */
    list_root_fn = smprintf("%s%s", "/nkn/nvsd/mfp/config", list_sfx);
    bail_null(list_root_fn);
    /* get the fields to process */
    list_fields_fn = smprintf("%s%s", wcf_id_list_fields, list_sfx);
    bail_null(list_fields_fn);

    err = web_get_trimmed_form_field_str(g_web_data, list_fields_fn,
                                         &fields_str);
    bail_error_null(err, fields_str);
    lc_log_basic(LOG_DEBUG, _("field_str:%s\n"), ts_str(fields_str));

    err = ts_tokenize(fields_str, ',', 0, 0,
                      ttf_ignore_leading_separator |
                      ttf_ignore_trailing_separator |
                      ttf_single_empty_token_null, &fields);
    bail_error(err);

    /*BUG:6580: Check if the node exists first.Else throw error saying the node wasn't even created
     * or was deleted recently before the config changes could be saved.
     */

    err = mdc_get_binding_tstr_fmt
	(web_mcc, &code, NULL, NULL, &profile_rec, NULL,
	 "/nkn/nvsd/mfp/config/%s/media-type", ts_str(mfp_value));
    bail_error(err);
    if (code || (!profile_rec) || (ts_length(profile_rec)== 0)) {
	err = ts_new_sprintf(inout_msg,
		_("The session \"%s\"doesn't exists or was deleted recently.\n"),
                                         ts_str(mfp_value));
	code = 1; //Set the error code
	goto bail;
    }




    node_name = smprintf("/nkn/nvsd/mfp/config/%s", ts_str(mfp_value));
    bail_null(node_name);

    err = bn_binding_new_str_autoinval(
        &binding, node_name, ba_value, bt_string, 0, ts_str(mfp_value));
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
	lc_log_basic(LOG_DEBUG, _("field:%s,list_name:%s\n"), ts_str(field),list_name);
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

                node_name = smprintf("/nkn/nvsd/mfp/config/%s/%s/%s",
                                     ts_str(mfp_value),
                                     ts_str(field), list_field_value_esc);
		lc_log_basic(LOG_DEBUG, "Nodename:%s\n", node_name);
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

	lc_log_basic(LOG_DEBUG, ("Field:%s\n"), ts_str(field));
        if ((value == NULL) && (type == bt_bool)) {
            /* XXX should we really default to false if no value? */
            err = ts_new_str(&value, "false");
            bail_error(err);
        } else if (value == NULL && 
		   !ts_cmp_str(field,"smooth/dvrlength", 0)) {
	    err = ts_new_str(&value, "30");
	    bail_error(err);
        } else {
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

	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/%s", ts_str(mfp_value),
				 ts_str(field));
	    bail_null(node_name);
	    
	    err = bn_binding_new_str_autoinval(
	        &binding, node_name, ba_value, type, 0, ts_str(value));
	    bail_error(err);

	    err = bn_set_request_msg_append(req, bsso_reset, 0,
					    binding, NULL);
	    bail_error(err);

            goto loop_clean;
        }

        node_name = smprintf("/nkn/nvsd/mfp/config/%s/%s", ts_str(mfp_value),
                             ts_str(field));
        bail_null(node_name);
	lc_log_basic(LOG_DEBUG, "Nodename:%s\n", node_name);

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
    ts_free(&profile_rec);
    return(err);
}

static int
wcf_handle_mfpsave(void)
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
    char *ret_msg = NULL;
 
    /*dump params*/
    //web_dump_params(g_web_data);
    /* create the message */
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = wcf_do_list_root(req, "", &msg, &code);
    bail_error(err);

    if (code) {
        goto check_error;
    }

    if (!code) {
	lc_log_basic(LOG_NOTICE, _("Sending data to mgmt\n"));
        err = mdc_send_mgmt_msg(web_mcc, 
                                req, false, NULL, &code, &msg);
        bail_error(err);
    }

check_error:    
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);
    /*
     * Dont clear the data here , since this function is used for action handler for publish
     * to save any unsaved  data
     */
#if 0
    if (!code) {
        if (clear_str == NULL || !ts_equal_str(clear_str, "false", false)) {
            err = web_clear_post_data(g_web_data);
            bail_error(err);
        }
    }
#endif
bail:
    ts_free(&msg);
    if(code) {
	return code;
    }
    return err;
}
static int
wcf_handle_restart(void)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *mfp_value = NULL;
    tstring *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    //get the mfp name
    err = web_get_request_param_str(g_web_data, wcf_field_stream, ps_post_data,&mfp_value);
    bail_error(err);

    //form req
    err = bn_action_request_msg_create(&req, "/nkn/nvsd/mfp/actions/restart");
    bail_error(err);
    //bind stream name with req
    err = bn_action_request_msg_append_new_binding(
	    req, 0, "stream-name", bt_string,ts_str(mfp_value), NULL);
    bail_error(err);
    //send msg
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);
    //get response
    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);
    if (code != 0) {
	goto check_error;
    } else {
	err = web_set_msg_result(g_web_data, code, msg);
	bail_error(err);
	ts_free(&msg);
    }
check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    ts_free(&msg);
    return(err);
}

static int
wcf_handle_save_close(void)
{
	int err = 0;
	err = web_set_msg_result_str(g_web_data, 1, "<script>window.close();</script>");
	return err;
}


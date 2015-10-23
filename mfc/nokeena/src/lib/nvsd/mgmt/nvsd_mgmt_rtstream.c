/*
 *
 * Filename:  nkn_mgmt_rtstream.c
 * Date:      2008/11/23 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <unistd.h>

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nkn_cfg_params.h"
#include "nkn_common_config.h"
#include "nkn_defs.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_rtstream_cfg_query(const bn_binding_array * bindings);

int nvsd_rtstream_module_cfg_handle_change(bn_binding_array * old_bindings,
		bn_binding_array * new_bindings);

int nvsd_rtstream_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data);
int nvsd_rtstream_delete_media_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array *
		bndgs_name_components,
		const tstring *
		bndgs_name_last_part,
		const tstring * b_value,
		void *data);

int
nvsd_rtstream_media_encode_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array *
		bndgs_name_components,
		const tstring *
		bndgs_name_last_part,
		const tstring * b_value,
		void *data);

int nvsd_rtstream_cfg_vod_server_port(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data);

int nvsd_rtstream_cfg_live_server_port(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data);

int nvsd_rtsp_cfg_server_interface(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern int nkn_rtstream_trace_enable;
extern int rtsp_origin_idle_timeout;
extern int rtsp_player_idle_timeout;
extern int glob_rtsp_format_convert_enabled;
const char rtsp_config_server_interface_prefix[] =
"/nkn/nvsd/rtstream/config/interface";

const char rtstream_config_prefix[] = "/nkn/nvsd/rtstream/config";
const char rtstream_vod_server_port_prefix[] =
"/nkn/nvsd/rtstream/config/vod/server_port";
const char rtstream_live_server_port_prefix[] =
"/nkn/nvsd/rtstream/config/live/server_port";
const char rtstream_media_encode_prefix[] =
"/nkn/nvsd/rtstream/config/media-encode";

/* Local Variables */
static boolean dynamic_node_change = NKN_FALSE;
static const char rtstream_rate_control_binding[] =
"/nkn/nvsd/rtstream/config/rate_control";
static const char rtstream_max_req_size_binding[] =
"/nkn/nvsd/rtstream/config/max_rtstream_req_size";
static const char rtstream_om_idle_timeout[] =
"/nkn/nvsd/rtstream/config/om_idle_timeout";
static const char rtstream_player_idle_timeout[] =
"/nkn/nvsd/rtstream/config/player_idle_timeout";
static const char rtstream_format_convert_enabled[] =
"/nkn/nvsd/rtstream/config/rtsp-origin/format/convert/enable";

tstring *vod_server_port_list = NULL;
tstring *live_server_port_list = NULL;
tstring *rtsp_server_interface_list = NULL;

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_cfg_query()
 *	purpose : query for rtstream config parameters
 */
int
nvsd_rtstream_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd rtstream module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		    rtstream_config_prefix,
		    NULL,
		    nvsd_rtstream_cfg_handle_change,
		    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		    rtstream_media_encode_prefix,
		    NULL,
		    nvsd_rtstream_media_encode_cfg_handle_change,
		    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		    rtstream_vod_server_port_prefix,
		    NULL,
		    nvsd_rtstream_cfg_vod_server_port,
		    &rechecked_licenses);
    bail_error(err);

    /*
     * We should have read all the ports that need to be opened for listening
     * on. Send the port list to the config module 
     */
    nkn_rtsp_vod_portlist_callback((char *) ts_str(vod_server_port_list));
    ts_free(&vod_server_port_list);

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		    rtstream_live_server_port_prefix,
		    NULL,
		    nvsd_rtstream_cfg_live_server_port,
		    &rechecked_licenses);
    bail_error(err);

    /*
     * We should have read all the ports that need to be opened for listening
     * on. Send the port list to the config module 
     */
    nkn_rtsp_live_portlist_callback((char *) ts_str(live_server_port_list));
    ts_free(&live_server_port_list);

    /*----------- RTSP LISTEN INTERFACES -----------*/
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		    rtsp_config_server_interface_prefix,
		    NULL,
		    nvsd_rtsp_cfg_server_interface,
		    &rechecked_licenses);
    bail_error(err);

    /*
     * We should have read all the interfaces that need to be opened for
     * listening on. Sent the interface list to the config module. 
     */
    if (rtsp_server_interface_list != NULL) {
	    nkn_rtsp_interfacelist_callback((char *)
			    ts_str(rtsp_server_interface_list));
	    ts_free(&rtsp_server_interface_list);
    }

bail:
    ts_free(&vod_server_port_list);
    ts_free(&live_server_port_list);
    return err;
}	/* end of nvsd_rtstream_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtsp_cfg_server_interface()
 *	purpose : handler for config changes for rtsp interface
 */
int
nvsd_rtsp_cfg_server_interface(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data)
{
    int err = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    tstring *t_interface = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    bail_null(rechecked_licenses_p);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "---------------> Node is : %s", ts_str(name));

    if (!bn_binding_name_pattern_match
		    (ts_str(name), "/nkn/nvsd/rtstream/config/interface/*"))
	goto bail;

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_interface);
    bail_error(err);

    if (NULL == rtsp_server_interface_list)
	err = ts_new_sprintf(&rtsp_server_interface_list, "%s ",
			ts_str(t_interface));
    else
	err = ts_append_sprintf(rtsp_server_interface_list, "%s ",
				ts_str(t_interface));

    bail_error(err);
bail:
    ts_free(&t_interface);
    return err;
}	/* end of nvsd_rtsp_cfg_server_interface () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_module_cfg_handle_change()
 *	purpose : handler for config changes for rtstream module
 */
int
nvsd_rtstream_module_cfg_handle_change(bn_binding_array * old_bindings,
		bn_binding_array * new_bindings) 
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
    * Call the callbacks 
    */
    dynamic_node_change = NKN_TRUE;

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
		    rtstream_config_prefix,
		    NULL,
		    nvsd_rtstream_cfg_handle_change,
		    &rechecked_licenses);

    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
		    rtstream_media_encode_prefix,
		    NULL,
		    nvsd_rtstream_media_encode_cfg_handle_change,
		    &rechecked_licenses);

    bail_error(err);

    /*
     * Call the callbacks for the deletion 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
		    "/nkn/nvsd/rtstream/config/media-encode",
		    NULL,
		    nvsd_rtstream_delete_media_cfg_handle_change,
		    &rechecked_licenses);
    bail_error(err);

bail:
    dynamic_node_change = NKN_FALSE;
    return err;
}	/* end of * nvsd_rtstream_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_cfg_handle_change()
 *	purpose : handler for config changes for rtstream module
 */
int
nvsd_rtstream_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    bail_null(rechecked_licenses_p);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- OM-IDLE-TIMEOUT ----------*/
    if (ts_equal_str(name, rtstream_om_idle_timeout, false)) {
	uint32 t_om_idle_timeout = 0;

	err = bn_binding_get_uint32(binding,
			ba_value, NULL, &t_om_idle_timeout);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
			"Read .../rtstream/config/om_idle_timeout as :\"%d\"",
			t_om_idle_timeout);

	rtsp_origin_idle_timeout = t_om_idle_timeout;

    }

    /*-------- PLAYER-IDLE-TIMEOUT -------*/
    else if (ts_equal_str(name, rtstream_player_idle_timeout, false)) {
	uint32 t_player_idle_timeout = 0;

	err = bn_binding_get_uint32(binding,
			ba_value, NULL, &t_player_idle_timeout);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
			"Read .../rtstream/config/player_idle_timeout as :\"%d\"",
			t_player_idle_timeout);

	rtsp_player_idle_timeout = t_player_idle_timeout;
    }

    /*-------- PLAYER-IDLE-TIMEOUT -------*/
    else if (ts_equal_str(name, rtstream_format_convert_enabled, false)) {
	tbool t_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		    "Read .../rtstream/config/format/convert/enable as :\"%d\"",
		    t_enabled);

	glob_rtsp_format_convert_enabled = (t_enabled) ? NKN_TRUE : NKN_FALSE;
    }
bail:
    return err;
}	/* end of nvsd_rtstream_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_media_encode_cfg_handle_change()
 *	purpose : handler for config changes for rtstream media-encode
 */
int
nvsd_rtstream_media_encode_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array *
		bndgs_name_components,
		const tstring *
		bndgs_name_last_part,
		const tstring * b_value,
		void *data)
{
    int err = 0;
    int t_index = 0;
    tbool *rechecked_licenses_p = data;
    char *node_name = NULL;
    tstring *t_extension = NULL;
    tstring *t_libname = NULL;
    const char *t_temp_str = NULL;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    char buf[1024];

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);
    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
    * check if this is our node 
    */
    if (bn_binding_name_pattern_match
		(ts_str(name), "/nkn/nvsd/rtstream/config/media-encode/**")) {
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
    } else {
	/*
	 * this is not a node we are interested in.. so bail 
	 */
	goto bail;
    }

    t_temp_str = tstr_array_get_str_quick(name_parts, 5);
    if (!t_temp_str)			// sanity check
	goto bail;

    t_index = atoi(t_temp_str);

    /*
    * Get value of MEDIA_ENCODE/[]/EXTENSION 
    */
    if (bn_binding_name_pattern_match(ts_str(name),
			    "/nkn/nvsd/rtstream/config/media-encode/*/extension")) {
        err = bn_binding_get_tstr(binding,
			    ba_value, bt_string, NULL, &t_extension);
        bail_error(err);

	lc_log_basic(LOG_DEBUG,
		    "Read .../rtstream/config/media-encode/%d/extension as : \"%s\"",
		    t_index, t_extension ? ts_str(t_extension) : "");

        node_name = smprintf("/nkn/nvsd/rtstream/config/media-encode/%d/module",
				    t_index);
	err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL, &t_libname,
				    node_name, NULL);
        bail_error(err);

        lc_log_basic(LOG_DEBUG,
		    "Read .../rtstream/config/media-encode/%d/module as : \"%s\"",
		    t_index, ts_str(t_libname));

        if (t_extension) {
	    snprintf(buf, sizeof (buf), "%s %s",
			    ts_str(t_extension), ts_str(t_libname));
	    /*
	    * TODO: Add callback here to push this libname config to rtsp 
	    */
	    lc_log_basic(LOG_DEBUG, "Field being sent to RTSP: %s", buf);

	    rtsp_cfg_so_callback(buf);
	}
    }

bail:
    safe_free(node_name);
    ts_free(&t_extension);
    ts_free(&t_libname);
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_rtstream_media_encode_cfg_handle_change() */


/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_delete_media_cfg_handle_change()
 *	purpose : handler for config delete for response header of rtstream module
 */
int
nvsd_rtstream_delete_media_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array *
		bndgs_name_components,
		const tstring *
		bndgs_name_last_part,
		const tstring * b_value,
		void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstring *t_extension = NULL;
    tbool *rechecked_licenses_p = data;
    char buf[256];

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
    * Check if this is our node 
    */
    if (bn_binding_name_pattern_match
		    (ts_str(name), "/nkn/nvsd/rtstream/config/media-encode/*/extension")) {
	/*-------- Get Content Response Header ------------*/
	err = bn_binding_get_tstr(binding,
			ba_value, bt_string, NULL, &t_extension);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
			"Remove :  .../rtstream/config/media-encode/*/extension as : \"%s\"",
			ts_str(t_extension));

	/*
	 * Call the callback function with the string read 
	 */
	memset(buf, 0, sizeof (buf));
	snprintf(buf, sizeof (buf), "%s", ts_str(t_extension));
	rtsp_no_cfg_so_callback(buf);
    }

bail:
    ts_free(&t_extension);
    return err;
}	/* end of * nvsd_rtstream_delete_media_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_cfg_vod_server_port()
 *	purpose : handler for configuring multiple listen ports for VOD/RTSP
 */
int
nvsd_rtstream_cfg_vod_server_port(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data)
{
    int err = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    uint16 serv_port = 0;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "Node is : '%s'", ts_str(name));

    if (!bn_binding_name_pattern_match
		    (ts_str(name), "/nkn/nvsd/rtstream/config/vod/server_port/*/port"))
	goto bail;

    err = bn_binding_get_uint16(binding, ba_value, NULL, &serv_port);
    bail_error(err);

    if (NULL == vod_server_port_list)
	err = ts_new_sprintf(&vod_server_port_list, "%d ", serv_port);
    else
	err = ts_append_sprintf(vod_server_port_list, "%d ", serv_port);

    bail_error(err);
bail:
    return err;
}	/* end of nvsd_rtstream_cfg_vod_server_port() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_rtstream_cfg_live_server_port()
 *	purpose : handler for configuring multiple listen ports for LIVE/RTSP
 */
int
nvsd_rtstream_cfg_live_server_port(const bn_binding_array * arr,
		uint32 idx,
		const bn_binding * binding,
		const tstring * bndgs_name,
		const tstr_array * bndgs_name_components,
		const tstring * bndgs_name_last_part,
		const tstring * b_value, void *data)
{
    int err = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    uint16 serv_port = 0;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "Node is : '%s'", ts_str(name));

    if (!bn_binding_name_pattern_match
		    (ts_str(name), "/nkn/nvsd/rtstream/config/live/server_port/*/port"))
	goto bail;

    err = bn_binding_get_uint16(binding, ba_value, NULL, &serv_port);
    bail_error(err);

    if (NULL == live_server_port_list)
	err = ts_new_sprintf(&live_server_port_list, "%d ", serv_port);
    else
	err = ts_append_sprintf(live_server_port_list, "%d ", serv_port);

    bail_error(err);

bail:
    return err;
}	/* end of * nvsd_rtstream_cfg_live_server_port() */ 
/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_rtstream.c */

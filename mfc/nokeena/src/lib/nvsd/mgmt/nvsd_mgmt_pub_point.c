/*
 *
 * Filename:  nvsd_mgmt_pub_point.c
 * Date:      2009/09/20
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/*-----------------------------------------------------------------------
 * BZ 2468
 * Dhruva (02/02/10)
 * This module is no longer used since the publish point is 
 * handled within the namespace
 ----------------------------------------------------------------------*/

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nvsd_mgmt.h"
#include "nvsd_mgmt_publish_point.h"
#include "nkn_namespace.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_defs.h"

/* local function prototypes */
//int nvsd_pub_point_cfg_query(void);
#if 0
int nvsd_pub_point_module_cfg_handle_change(bn_binding_array * old_bindings,
		bn_binding_array * new_bindings);

int nvsd_pub_point_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		bn_binding * binding, void *data);

int nvsd_pub_point_delete_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		bn_binding * binding, void *data);
#endif

/* Local macros */
#define	MAX_PUBLISH_POINTS	256

/* Extern Variables */
extern md_client_context *nvsd_mcc;

/* Local variables */
//static const char pub_point_config_prefix[] = "/nkn/nvsd/pub-point/config";
//static pub_point_node_data_t  *pstPublishPoint = NULL;
//pub_point_node_data_t lstPublishPoints [MAX_PUBLISH_POINTS];
//static tbool  dynamic_change = false ;

/* Extern Functions */
extern void remove_pub_point_in_namespace(const char *t_pub_point);

//extern void rtsp_cfg_pub_point_callback(pub_point_node_data_t *pstPublishPoint);
//extern void rtsp_no_cfg_pub_point_callback(pub_point_node_data_t *pstPublishPoint);

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_pub_point_cfg_query ()
 *	purpose : query for pub-point config parameters
 */
int
nvsd_pub_point_cfg_query(void
		)
{
	int err = 0;
	bn_binding_array *bindings = NULL;
	tbool rechecked_licenses = false;

	(void) bindings;
	(void) rechecked_licenses;

	bail_error(err);

	/*
	 * BZ 2468 Dhruva (02/02/10) This module is no longer used since the
	 * publish point is handled within the namespace 
	 */
#if 0
	lc_log_basic(LOG_NOTICE,
			"nvsd pub-point module mgmtd query initializeing ..");

	/*
	 * Initialize the publish point list 
	 */
	memset(lstPublishPoints, 0,
			MAX_PUBLISH_POINTS * sizeof (pub_point_node_data_t));

	err = mdc_get_binding_children(nvsd_mcc,
			NULL,
			NULL,
			true,
			&bindings,
			false, true, pub_point_config_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings,
			nvsd_pub_point_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);
	bn_binding_array_free(&bindings);
#endif
bail:
	return err;

}								/* end of nvsd_pub_point_cfg_query () */

#if 0

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_pub_point_module_cfg_handle_change ()
 *	purpose : handler for config changes for pub-point module
 */
	int
nvsd_pub_point_module_cfg_handle_change(bn_binding_array * old_bindings,
		bn_binding_array * new_bindings)
{
	int err = 0;
	tbool rechecked_licenses = false;

	/*
	 * Reset the flag 
	 */
	dynamic_change = false;

	/*
	 * Lock namespace first 
	 */
	nvsd_mgmt_namespace_lock();

	/*
	 * Call all calbacks that are interested in this change 
	 */
	err = bn_binding_array_foreach(new_bindings,
			nvsd_pub_point_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);

	/*
	 * Call the old bindings callbacks 
	 */
	err = bn_binding_array_foreach(old_bindings,
			nvsd_pub_point_delete_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);

	nvsd_mgmt_namespace_unlock();

	if (dynamic_change)
		nkn_namespace_config_update(NULL);

	return err;

bail:
	nvsd_mgmt_namespace_unlock();
	return err;
}								/* end of
								 * nvsd_pub_point_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : get_publish_point_element ()
 *	purpose : get the element in the array for the given pub-point, 
 *		if not found then return the first free element
 */
	static pub_point_node_data_t *
get_publish_point_element(const char *cpPubPoint)
{
	int i = 0;
	int free_entry_index = -1;

	for (i = 0; i < MAX_PUBLISH_POINTS; i++) {
		if (NULL == lstPublishPoints[i].name) {
			/*
			 * Save the first free entry 
			 */
			if (-1 == free_entry_index)
				free_entry_index = i;

			/*
			 * Empty entry.. hence continue 
			 */
			continue;
		} else if (0 == strcmp(cpPubPoint, lstPublishPoints[i].name)) {
			/*
			 * Found match hence return this entry 
			 */
			return (&(lstPublishPoints[i]));
		}
	}

	/*
	 * Now that we have gone through the list and no match return the
	 * free_entry_index 
	 */
	if (-1 != free_entry_index)
		return (&(lstPublishPoints[free_entry_index]));

	/*
	 * No match and no free entry 
	 */
	return (NULL);
}								/* end of get_publish_point_element () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_pub_point_cfg_handle_change()
 *	purpose : handler for config changes for pub-point module
 */
	int
nvsd_pub_point_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx, bn_binding * binding, void *data)
{
	int err = 0;
	const tstring *name = NULL;
	const char *t_pub_point = NULL;
	tstr_array *name_parts = NULL;
	tbool *rechecked_licenses_p = data;

	UNUSED_ARGUMENT(arr);
	UNUSED_ARGUMENT(idx);

	bail_null(rechecked_licenses_p);

	err = bn_binding_get_name(binding, &name);
	bail_error(err);

	/*
	 * Check if this is our node 
	 */
	if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/**")) {
		/*
		 * get the publish point 
		 */
		err = bn_binding_get_name_parts(binding, &name_parts);
		bail_error(err);

		t_pub_point = tstr_array_get_str_quick(name_parts, 4);

		lc_log_basic(LOG_DEBUG,
				"Read .../nkn/nvsd/pub-point/config as : \"%s\"",
				t_pub_point);

		pstPublishPoint = get_publish_point_element(t_pub_point);
		if (!pstPublishPoint)
			goto bail;

		if (NULL == pstPublishPoint->name)
			pstPublishPoint->name = strdup(t_pub_point);

	} else {
		/*
		 * This is not a pub-point node.. hence bail 
		 */
		goto bail;
	}

	/*
	 * Get the value of EVENT-DURATION 
	 */
	if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/*/event/duration")) {
		char *t_event_duration = NULL;

		err = bn_binding_get_str(binding,
				ba_value, bt_string, NULL, &t_event_duration);
		bail_error(err);

		lc_log_basic(LOG_DEBUG,
				"Read .../pub-point/config/%s/event/duration as \"%s\"",
				t_pub_point, t_event_duration);

		if (NULL != pstPublishPoint->event_duration)
			free(pstPublishPoint->event_duration);

		if (t_event_duration)
			pstPublishPoint->event_duration = strdup(t_event_duration);
		else
			pstPublishPoint->event_duration = NULL;

		if (t_event_duration)
			safe_free(t_event_duration);
	}

	/*
	 * Get the value of SERVER-URI 
	 */
	else if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/*/pub-server/uri")) {
		char *t_server_uri = NULL;

		err = bn_binding_get_str(binding,
				ba_value, bt_uri, NULL, &t_server_uri);
		bail_error(err);

		lc_log_basic(LOG_DEBUG,
				"Read .../pub-point/config/%s/pub-server/uri as \"%s\"",
				t_pub_point, t_server_uri);

		if (NULL != pstPublishPoint->server_uri)
			free(pstPublishPoint->server_uri);

		if (t_server_uri)
			pstPublishPoint->server_uri = strdup(t_server_uri);
		else
			pstPublishPoint->server_uri = NULL;

		// rtsp_cfg_pub_point_callback (pstPublishPoint);

		if (t_server_uri)
			safe_free(t_server_uri);
	}

	/*
	 * Get the value of SERVER-URI MODE 
	 */
	else if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/*/pub-server/mode")) {
		char *t_mode = NULL;

		err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_mode);
		bail_error(err);

		lc_log_basic(LOG_DEBUG,
				"Read .../pub-point/config/%s/pub-server/uri/mode as \"%s\"",
				t_pub_point, t_mode);

		if (t_mode && (0 == strcmp(t_mode, "pull")))
			pstPublishPoint->mode = PUB_SERVER_MODE_PULL;

		// rtsp_cfg_pub_point_callback (pstPublishPoint);

		if (t_mode)
			safe_free(t_mode);

	}

	/*
	 * Get the value of SDP-STATIC 
	 */
	else if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/*/sdp/static")) {
		tstring *t_sdp_static = NULL;

		err = bn_binding_get_tstr(binding,
				ba_value, bt_uri, NULL, &t_sdp_static);
		bail_error(err);

		lc_log_basic(LOG_DEBUG,
				"Read .../pub-point/config/%s/sdp/static as \"%s\"",
				t_pub_point, ts_str(t_sdp_static));

		if (NULL != pstPublishPoint->sdp_static)
			free(pstPublishPoint->sdp_static);

		if (t_sdp_static)
			pstPublishPoint->sdp_static = strdup(ts_str(t_sdp_static));
		else
			pstPublishPoint->sdp_static = NULL;

		// rtsp_cfg_pub_point_callback (pstPublishPoint);

		if (t_sdp_static)
			ts_free(&t_sdp_static);
	}

	/*
	 * Get the value of ACTIVE 
	 */
	else if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/*/active")) {
		tbool t_active = false;

		err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);

		lc_log_basic(LOG_DEBUG, "Read .../pub-point/config/%s/active as \"%s\"",
				t_pub_point, (t_active) ? "true" : "false");

		pstPublishPoint->enable = t_active;

		// rtsp_cfg_pub_point_callback (pstPublishPoint);
	}

	/*
	 * Crazy hack to send the configuration to RTSP 
	 */
bail:
	tstr_array_free(&name_parts);
	return err;
}								/* end of nvsd_pub_point_cfg_handle_change () 
								 */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_pub_point_delete_cfg_handle_change ()
 *	purpose : handler for config delete changes for publish point module
 */
	int
nvsd_pub_point_delete_cfg_handle_change(const bn_binding_array * arr,
		uint32 idx,
		bn_binding * binding, void *data)
{
	int err = 0;
	const tstring *name = NULL;
	const char *t_pub_point = NULL;
	tstr_array *name_parts = NULL;
	tbool *rechecked_licenses_p = data;

	UNUSED_ARGUMENT(arr);
	UNUSED_ARGUMENT(idx);

	bail_null(rechecked_licenses_p);

	err = bn_binding_get_name(binding, &name);
	bail_error(err);

	/*
	 * Check if this is our node 
	 */
	if (bn_binding_name_pattern_match
			(ts_str(name), "/nkn/nvsd/pub-point/config/**")) {
		err = bn_binding_get_name_parts(binding, &name_parts);
		bail_error(err);

		t_pub_point = tstr_array_get_str_quick(name_parts, 4);

		lc_log_basic(LOG_DEBUG,
				"Read .../nkn/nvsd/virtual_player/config as : \"%s\"",
				t_pub_point);

		/*
		 * Delete this only if the wildcard change is received 
		 */
		if (bn_binding_name_pattern_match
				(ts_str(name), "/nkn/nvsd/pub-point/config/*")) {
			/*
			 * Get the pub-point entry in the global array 
			 */
			pstPublishPoint = get_publish_point_element(t_pub_point);
			if (pstPublishPoint && (NULL != pstPublishPoint->name)) {
				// rtsp_no_cfg_pub_point_callback (pstPublishPoint);

				free(pstPublishPoint->name);
				memset(pstPublishPoint, 0, sizeof (pub_point_node_data_t));

				remove_pub_point_in_namespace(t_pub_point);
			}
			/*
			 * Set the flag so that we know this is a dynamic change 
			 */
			dynamic_change = true;
		}
	} else {
		/*
		 * We arent interested in this node .. hence leave 
		 */
		goto bail;
	}

bail:
	tstr_array_free(&name_parts);
	return err;
}								/* end of
								 * nvsd_pub_point_delete_cfg_handle_change () 
								 */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_get_publish_point ()
 *	purpose : get the matching Publish Point's structure
 */
	pub_point_node_data_t *
nvsd_mgmt_get_publish_point(char *cpPublishPoint)
{
	int i = 0;

	/*
	 * Sanity Check 
	 */
	if (NULL == cpPublishPoint)
		return (NULL);

	lc_log_basic(LOG_NOTICE, "Request received for %s publish point",
			cpPublishPoint);

	/*
	 * Now find the matchin publish point entry 
	 */
	for (i = 0; i < MAX_PUBLISH_POINTS; i++) {
		if ((NULL != lstPublishPoints[i].name) &&
				(0 == strcmp(cpPublishPoint, lstPublishPoints[i].name)))
			/*
			 * return this entry 
			 */
			return (&(lstPublishPoints[i]));
	}

	/*
	 * No match 
	 */
	return (NULL);
}								/* end of nvsd_mgmt_get_publish_point () */
#endif

/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_rtstream.c */

/*
 * vim:ts=8:sw=8:wm=8:noexpandtab
 */

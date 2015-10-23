/*
 *
 * Filename:  nkn_mgmt_virtual_player.c
 * Date:      2009/1/14 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-09 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nvsd_mgmt_virtual_player.h"
#include "nkn_namespace.h"
#include "nvsd_mgmt_namespace.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_virtual_player_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);

static int
nvsd_virtual_player_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int nvsd_virtual_player_delete_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding *
	binding,
	const tstring *
	bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring *
	bndgs_value,
	void *data);

/* Local Macros */
#define	MAX_VIRTUAL_PLAYERS	256

/* Extern Variables */
extern md_client_context *nvsd_mcc;

const char virtual_player_config_prefix[] = "/nkn/nvsd/virtual_player";

/* Local Variables */
static virtual_player_node_data_t *pstVirtualPlayer = NULL;
virtual_player_node_data_t lstVirtualPlayers[MAX_VIRTUAL_PLAYERS];
static tbool dynamic_change = false;

/* Extern Functions */
void remove_virtual_player_in_namespace(const char *t_virtual_player);

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_virtual_player_cfg_query()
 *	purpose : query for virtual_player config parameters
 */
int
nvsd_virtual_player_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG,
	    "nvsd virtual_player module mgmtd query initializing ..");

    /*
     * Initialize the VirtualPlayer list 
     */
    memset(lstVirtualPlayers, 0,
	    MAX_VIRTUAL_PLAYERS * sizeof (virtual_player_node_data_t));

    /*
     * Bind to get NAMESPACE 
     */

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    virtual_player_config_prefix,
	    NULL,
	    nvsd_virtual_player_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}	/* end of nvsd_virtual_player_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_virtual_player_module_cfg_handle_change()
 *	purpose : handler for config changes for virtual_player module
 */
int
nvsd_virtual_player_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Reset the flag 
     */
    dynamic_change = false;

    /*
     * Lock first 
     */
    nvsd_mgmt_namespace_lock();

    /*
     * Call the new bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    virtual_player_config_prefix,
	    NULL,
	    nvsd_virtual_player_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Call the old bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    virtual_player_config_prefix,
	    NULL,
	    nvsd_virtual_player_delete_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_namespace_unlock();

    /*
     * If relevant node has changed then indicate to the module 
     */
    if (dynamic_change)
	nkn_namespace_config_update(NULL);
    return err;

bail:
    if (err) {
	/*
	 * Unlock when leaving 
	 */
	nvsd_mgmt_namespace_unlock();
    }
    return err;
}	/* end of * nvsd_virtual_player_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : get_virtual_player_element ()
 *	purpose : get the element in the array for the given virtual_player, 
 *		if not found then return the first free element
 */
static virtual_player_node_data_t *
get_virtual_player_element(const char *cpVirtualPlayer)
{
    int i;
    int free_entry_index = -1;

    for (i = 0; i < MAX_VIRTUAL_PLAYERS; i++) {
	if (NULL == lstVirtualPlayers[i].name) {
	    /*
	     * Save the first free entry 
	     */
	    if (-1 == free_entry_index)
		free_entry_index = i;

	    /*
	     * Empty entry hence continue 
	     */
	    continue;
	} else if (0 == strcmp(cpVirtualPlayer, lstVirtualPlayers[i].name)) {
	    /*
	     * Found match hence return this entry 
	     */
	    return (&(lstVirtualPlayers[i]));
	}
    }

    /*
     * Now that we have gone thru the list and no match return the
     * free_entry_index 
     */
    if (-1 != free_entry_index)
	return (&(lstVirtualPlayers[free_entry_index]));

    /*
     * No match and no free entry 
     */
    return (NULL);
}	/* end of get_virtual_player_element () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_virtual_player_cfg_handle_change()
 *	purpose : handler for config changes for virtual_player module
 */
static int
nvsd_virtual_player_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_virtual_player = NULL;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);
    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/virtual_player/config/**")) {

	/*-------- Get the VirtualPlayer ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_virtual_player = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/virtual_player/config as : \"%s\"",
		t_virtual_player);

	/*
	 * Get the virtual_player entry in the global array 
	 */
	pstVirtualPlayer = get_virtual_player_element(t_virtual_player);
	if (!pstVirtualPlayer)
	    goto bail;

	if (NULL == pstVirtualPlayer->name)
	    pstVirtualPlayer->name =
		nkn_strdup_type(t_virtual_player, mod_mgmt_charbuf);
    } else {
	/*
	 * This is not the virtual_player node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the value of TYPE 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*", "type")) {
	uint32 t_type = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_type);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/type as : \"%d\"",
		t_virtual_player, t_type);
	pstVirtualPlayer->type = t_type;
    }

    /*
     * Get the value of CONNECTION MAX BANDWIDTH 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "max_session_rate")) {
	uint32 t_max_bandwidth = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_max_bandwidth);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/max_session_rate as : \"%d\"",
		t_virtual_player, t_max_bandwidth);
	pstVirtualPlayer->connection_max_bandwidth = t_max_bandwidth;
    }

    /*
     * Get the HASH VERIFY ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "hash_verify", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->hash_verify.active = t_active;
    }

    /*
     * Get the HASH VERIFY DIGEST 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "hash_verify", "digest")) {
	tstring *t_digest = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_digest);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/digest as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_digest));

	if (NULL != pstVirtualPlayer->hash_verify.digest)
	    safe_free(pstVirtualPlayer->hash_verify.digest);

	if (t_digest)
	    pstVirtualPlayer->hash_verify.digest =
		nkn_strdup_type(ts_str(t_digest), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.digest = NULL;

	ts_free(&t_digest);
    }

    /*
     * Get the HASH VERIFY DATA STRING 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "data", "string")) {
	tstring *t_data_string = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_data_string);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/data_string as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_data_string));

	if (NULL != pstVirtualPlayer->hash_verify.data_string)
	    safe_free(pstVirtualPlayer->hash_verify.data_string);

	if (t_data_string)
	    pstVirtualPlayer->hash_verify.data_string =
		nkn_strdup_type(ts_str(t_data_string), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.data_string = NULL;

	ts_free(&t_data_string);
    }

    /*
     * Get the HASH VERIFY DATA URI OFFSET 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "data", "uri_offset")) {
	uint32 t_uri_offset = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_uri_offset);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/data/uri_offset as : \"%d\"",
		t_virtual_player, t_uri_offset);
	pstVirtualPlayer->hash_verify.data_uri_offset = t_uri_offset;
    }

    /*
     * Get the HASH VERIFY DATA URI LENGTH 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "data", "uri_len")) {
	uint32 t_uri_len = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_uri_len);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/data/uri_len as : \"%d\"",
		t_virtual_player, t_uri_len);
	pstVirtualPlayer->hash_verify.data_uri_len = t_uri_len;
    }
    /*
     * Get the HASH VERIFY SECRET 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "secret", "value")) {
	tstring *t_secret = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_secret);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/secret as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_secret));

	if (NULL != pstVirtualPlayer->hash_verify.secret)
	    safe_free(pstVirtualPlayer->hash_verify.secret);

	if (t_secret)
	    pstVirtualPlayer->hash_verify.secret =
		nkn_strdup_type(ts_str(t_secret), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.secret = NULL;

	ts_free(&t_secret);
    }

    /*
     * Get the HASH VERIFY SECRET TYPE 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "secret", "type")) {
	tstring *t_secret_type = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_secret_type);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/secret_type as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_secret_type));

	if (NULL != pstVirtualPlayer->hash_verify.secret_type)
	    safe_free(pstVirtualPlayer->hash_verify.secret_type);

	if (t_secret_type)
	    pstVirtualPlayer->hash_verify.secret_type =
		nkn_strdup_type(ts_str(t_secret_type), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.secret_type = NULL;
	ts_free(&t_secret_type);
    }

    /*
     * Get the HASH VERIFY MATCH URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "match", "uri_query")) {
	tstring *t_match_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_match_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/match/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_match_uri_query));

	if (NULL != pstVirtualPlayer->hash_verify.match_uri_query)
	    safe_free(pstVirtualPlayer->hash_verify.match_uri_query);

	if (t_match_uri_query)
	    pstVirtualPlayer->hash_verify.match_uri_query =
		nkn_strdup_type(ts_str(t_match_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.match_uri_query = NULL;

	ts_free(&t_match_uri_query);
    }
    /*
     * Get the HASH VERIFY Expiry time option 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "hash_verify", "expiry_time",
	     "epochtime")) {
	tstring *t_expiry_time = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_expiry_time);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/expiry_time/epochtime as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_expiry_time));

	if (NULL != pstVirtualPlayer->hash_verify.expiry_time_query)
	    safe_free(pstVirtualPlayer->hash_verify.expiry_time_query);

	if (t_expiry_time && ts_length(t_expiry_time) > 0)
	    pstVirtualPlayer->hash_verify.expiry_time_query =
		nkn_strdup_type(ts_str(t_expiry_time), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.expiry_time_query = NULL;
	ts_free(&t_expiry_time);
    }
    /*
     * Get the HASH VERIFY Url format 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "hash_verify", "url_type")) {
	tstring *t_url = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_url);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/hash_verify/expiry_time/url_type as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_url));

	if (NULL != pstVirtualPlayer->hash_verify.url_format)
	    safe_free(pstVirtualPlayer->hash_verify.url_format);

	if (t_url)
	    pstVirtualPlayer->hash_verify.url_format =
		nkn_strdup_type(ts_str(t_url), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->hash_verify.url_format = NULL;
	ts_free(&t_url);
    }
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "fast_start", "active")) {
	/*
	 * Get the FAST START ACTIVE FLAG 
	 */
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->fast_start.active = t_active;
    }

    /*
     * Get the FAST START VALID NODE 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "fast_start", "valid")) {
	tstring *t_valid;

	err = bn_binding_get_tstr(binding, ba_value, bt_link, NULL, &t_valid);
	bail_error(err);

	/*
	 * Now check this node to see which parameter is valid 
	 */
	if (bn_binding_name_pattern_match
		(ts_str_maybe_empty(t_valid),
		 "/nkn/nvsd/virtual_player/config/*/fast_start/uri_query"))
	    pstVirtualPlayer->fast_start.active_type = VP_PARAM_TYPE_URI_QUERY;
	else if (bn_binding_name_pattern_match
		(ts_str_maybe_empty(t_valid),
		 "/nkn/nvsd/virtual_player/config/*/fast_start/size"))
	    pstVirtualPlayer->fast_start.active_type = VP_PARAM_TYPE_SIZE;
	else if (bn_binding_name_pattern_match
		(ts_str_maybe_empty(t_valid),
		 "/nkn/nvsd/virtual_player/config/*/fast_start/time"))
	    pstVirtualPlayer->fast_start.active_type = VP_PARAM_TYPE_TIME;
	else
	    pstVirtualPlayer->fast_start.active_type = VP_PARAM_TYPE_SIZE;

	ts_free(&t_valid);
    }

    /*
     * Get the FAST START URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "fast_start", "uri_query")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/fast_start/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_query));

	if (NULL != pstVirtualPlayer->fast_start.uri_query)
	    safe_free(pstVirtualPlayer->fast_start.uri_query);

	if (t_uri_query)
	    pstVirtualPlayer->fast_start.uri_query =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->fast_start.uri_query = NULL;

	ts_free(&t_uri_query);
    }

    /*
     * Get the FAST START SIZE 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "fast_start", "size")) {
	uint32 t_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_size);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/fast_start/size as : \"%d\"",
		t_virtual_player, t_size);
	pstVirtualPlayer->fast_start.size = t_size;
    }
#if 1
    /*
     * Get the FAST START TIME 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "fast_start", "time")) {
	uint32 t_time = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_time);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/fast_start/time as : \"%d\"",
		t_virtual_player, t_time);
	pstVirtualPlayer->fast_start.time = t_time;
    }
#endif
    /*
     * Get the SEEK ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "seek", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->seek.active = t_active;
    }
    /*
     * Get SEEK's TUNNEL ENABLE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "seek", "enable_tunnel")) {
	tbool t_enable_tunnel;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable_tunnel);
	bail_error(err);
	pstVirtualPlayer->seek.enable_tunnel = t_enable_tunnel;

    }

    /*
     * Get the SEEK URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "seek", "uri_query")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/seek/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_query));

	if (NULL != pstVirtualPlayer->seek.uri_query)
	    safe_free(pstVirtualPlayer->seek.uri_query);

	if (t_uri_query)
	    pstVirtualPlayer->seek.uri_query =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->seek.uri_query = NULL;

	ts_free(&t_uri_query);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "seek", "length", "uri_query")) {
	tstring *t_length_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_length_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/seek/length/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_length_uri_query));

	if (NULL != pstVirtualPlayer->seek.length_uri_query)
	    safe_free(pstVirtualPlayer->seek.length_uri_query);

	if (t_length_uri_query)
	    pstVirtualPlayer->seek.length_uri_query =
		nkn_strdup_type(ts_str(t_length_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->seek.length_uri_query = NULL;

	ts_free(&t_length_uri_query);
    }
    /*
     * Get seek mp4 type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "seek", "flv-type")) {
	tstring *t_flv_type = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_flv_type);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/seek/flv-type as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_flv_type));

	if (NULL != pstVirtualPlayer->seek.flv_type)
	    safe_free(pstVirtualPlayer->seek.flv_type);

	if (t_flv_type)
	    pstVirtualPlayer->seek.flv_type =
		nkn_strdup_type(ts_str(t_flv_type), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->seek.flv_type = NULL;

	ts_free(&t_flv_type);
    }
    /*
     * Get seek mp4 type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "seek", "mp4-type")) {
	tstring *t_mp4_type = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_mp4_type);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/seek/mp4-type as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_mp4_type));

	if (NULL != pstVirtualPlayer->seek.mp4_type)
	    safe_free(pstVirtualPlayer->seek.mp4_type);

	if (t_mp4_type)
	    pstVirtualPlayer->seek.mp4_type =
		nkn_strdup_type(ts_str(t_mp4_type), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->seek.mp4_type = NULL;

	ts_free(&t_mp4_type);
    }
    /*
     * Get the SEEK flv-byte-offset use-9byte-hdr 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "seek", "use_9byte_hdr")) {
	tbool t_9byte_hdr;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_9byte_hdr);
	bail_error(err);
	pstVirtualPlayer->seek.use_9byte_hdr = t_9byte_hdr;
    }

    /*
     * Get the FULL DOWNLOAD ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "full_download", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->full_download.active = t_active;
    }

    /*
     * Get the FULL DOWNLOAD URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "full_download", "uri_query")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/full_download/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_query));

	if (NULL != pstVirtualPlayer->full_download.uri_query)
	    safe_free(pstVirtualPlayer->full_download.uri_query);

	if (t_uri_query)
	    pstVirtualPlayer->full_download.uri_query =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->full_download.uri_query = NULL;

	ts_free(&t_uri_query);
    }

    /*
     * Get the FULL DOWNLOAD ALWAYS FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "full_download", "always")) {
	tbool t_always;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_always);
	bail_error(err);
	pstVirtualPlayer->full_download.always = t_always;
    }

    /*
     * Get the FULL DOWNLOAD MATCH 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "full_download", "match")) {
	tstring *t_match = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_match);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/full_download/match as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_match));

	if (NULL != pstVirtualPlayer->full_download.match)
	    safe_free(pstVirtualPlayer->full_download.match);

	if (t_match)
	    pstVirtualPlayer->full_download.match =
		nkn_strdup_type(ts_str(t_match), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->full_download.match = NULL;

	ts_free(&t_match);
    }

    /*
     * Get the FULL DOWNLOAD URI HEADER 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "full_download", "uri_hdr")) {
	tstring *t_uri_hdr = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_hdr);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/full_download/uri_hdr as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_hdr));

	if (NULL != pstVirtualPlayer->full_download.uri_hdr)
	    safe_free(pstVirtualPlayer->full_download.uri_hdr);

	if (t_uri_hdr)
	    pstVirtualPlayer->full_download.uri_hdr =
		nkn_strdup_type(ts_str(t_uri_hdr), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->full_download.uri_hdr = NULL;

	ts_free(&t_uri_hdr);
    }
    /*
     * Get the SMOOTH FLOW ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "smooth_flow", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->smooth_flow.active = t_active;
    }

    /*
     * Get the SMOOTH FLOW URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "smooth_flow", "uri_query")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/smooth_flow/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_query));

	if (NULL != pstVirtualPlayer->smooth_flow.uri_query)
	    safe_free(pstVirtualPlayer->smooth_flow.uri_query);

	if (t_uri_query)
	    pstVirtualPlayer->smooth_flow.uri_query =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->smooth_flow.uri_query = NULL;

	ts_free(&t_uri_query);
    }

    /*
     * Get the RATE MAP ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "rate_map", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->rate_map_active = t_active;
    }

    /*
     * Get the RATE MAP MATCH 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "rate_map", "match", "**")) {
	int i;
	const char *t_match = NULL;

	/*
	 * Get the match string 
	 */
	t_match = tstr_array_get_str_quick(name_parts, 7);
	bail_error(err);

	/*
	 * Find free or matching match entry 
	 */
	i = 0;
	while ((MAX_RATE_MAPS > i)
		&& (NULL != pstVirtualPlayer->rate_map[i].match)) {
	    if (0 == strcmp(pstVirtualPlayer->rate_map[i].match, t_match))
		break;
	    i++;
	}

	/*
	 * No entry hence bail out 
	 */
	if (MAX_RATE_MAPS == i)
	    goto bail;

	if (NULL != pstVirtualPlayer->rate_map[i].match)
	    safe_free(pstVirtualPlayer->rate_map[i].match);

	if (t_match)
	    pstVirtualPlayer->rate_map[i].match =
		nkn_strdup_type(t_match, mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->rate_map[i].match = NULL;

	/*
	 * Get the RATE MAP RATE 
	 */
	if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 5, "*", "rate_map", "match", "*", "rate")) {
	    uint32 t_rate = 0;

	    err = bn_binding_get_uint32(binding, ba_value, NULL, &t_rate);

	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/rate_map/match/%s/rate as : \"%d\"",
		    t_virtual_player, t_match, t_rate);
	    pstVirtualPlayer->rate_map[i].rate = t_rate;
	}

	/*
	 * Get the PROFILE RATE MAP QUERY STRING 
	 */
	else if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 5, "*", "rate_map", "match", "*",
		 "query_string")) {
	    tstring *t_query_string = NULL;

	    err = bn_binding_get_tstr(binding,
		    ba_value, bt_string, NULL,
		    &t_query_string);
	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/rate_map/match/%s/query_string as : \"%s\"",
		    t_virtual_player, t_match, ts_str(t_query_string));

	    if (NULL != pstVirtualPlayer->rate_map[i].query_string)
		safe_free(pstVirtualPlayer->rate_map[i].query_string);

	    if (t_query_string)
		pstVirtualPlayer->rate_map[i].query_string =
		    nkn_strdup_type(ts_str(t_query_string), mod_mgmt_charbuf);
	    else
		pstVirtualPlayer->rate_map[i].query_string = NULL;

	    ts_free(&t_query_string);
	}

	/*
	 * Get the PROFILE RATE MAP UOL OFFSET 
	 */
	else if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 6, "*", "rate_map", "match", "*", "uol",
		 "offset")) {
	    uint32 t_offset = 0;

	    err = bn_binding_get_uint32(binding, ba_value, NULL, &t_offset);

	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/rate_map/match/%s/uol/offset as : \"%d\"",
		    t_virtual_player, t_match, t_offset);
	    pstVirtualPlayer->rate_map[i].uol_offset = t_offset;
	}

	/*
	 * Get the PROFILE RATE MAP UOL LENGTH 
	 */
	else if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 6, "*", "rate_map", "match", "*", "uol",
		 "length")) {
	    uint32 t_length = 0;

	    err = bn_binding_get_uint32(binding, ba_value, NULL, &t_length);

	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/rate_map/match/%s/uol/length as : \"%d\"",
		    t_virtual_player, t_match, t_length);
	    pstVirtualPlayer->rate_map[i].uol_length = t_length;
	}
    }

    /*
     * Get the CONTROL POINT 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "control_point")) {
	tstring *t_control_point = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_control_point);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/control_point as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_control_point));

	if (NULL != pstVirtualPlayer->control_point)
	    safe_free(pstVirtualPlayer->control_point);

	if (t_control_point)
	    pstVirtualPlayer->control_point =
		nkn_strdup_type(ts_str(t_control_point), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->control_point = NULL;

	ts_free(&t_control_point);
    }

    /*
     * Get the HEALTH PROBE ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "health_probe", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->health_probe.active = t_active;
    }

    /*
     * Get the HEALTH PROBE URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "health_probe", "uri_query")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/health_probe/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_query));

	if (NULL != pstVirtualPlayer->health_probe.uri_query)
	    safe_free(pstVirtualPlayer->health_probe.uri_query);

	if (t_uri_query)
	    pstVirtualPlayer->health_probe.uri_query =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->health_probe.uri_query = NULL;

	ts_free(&t_uri_query);
    }

    /*
     * Get the HEALTH PROBE MATCH 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "health_probe", "match")) {
	tstring *t_match = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_match);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/health_probe/match as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_match));

	if (NULL != pstVirtualPlayer->health_probe.match)
	    safe_free(pstVirtualPlayer->health_probe.match);

	if (t_match)
	    pstVirtualPlayer->health_probe.match =
		nkn_strdup_type(ts_str(t_match), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->health_probe.match = NULL;

	ts_free(&t_match);
    }

    /*
     * Get the REQ AUTH ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "req_auth", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->req_auth.active = t_active;
    }

    /*
     * Get the REQ AUTH DIGEST 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "req_auth", "digest")) {
	tstring *t_digest = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_digest);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/req_auth/digest as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_digest));

	if (NULL != pstVirtualPlayer->req_auth.digest)
	    safe_free(pstVirtualPlayer->req_auth.digest);

	if (t_digest)
	    pstVirtualPlayer->req_auth.digest =
		nkn_strdup_type(ts_str(t_digest), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->req_auth.digest = NULL;

	ts_free(&t_digest);
    }

    /*
     * Get the REQ AUTH SECRET VALUE 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "req_auth", "secret", "value")) {
	tstring *t_secret_value = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_secret_value);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/req_auth/secret_value as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_secret_value));

	if (NULL != pstVirtualPlayer->req_auth.secret_value)
	    safe_free(pstVirtualPlayer->req_auth.secret_value);

	if (t_secret_value)
	    pstVirtualPlayer->req_auth.secret_value =
		nkn_strdup_type(ts_str(t_secret_value), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->req_auth.secret_value = NULL;

	ts_free(&t_secret_value);
    }

    /*
     * Get the REQ AUTH STREAM ID URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "req_auth", "stream_id", "uri_query")) {
	tstring *t_stream_id_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_stream_id_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/req_auth/stream_id_uri_query as : \"%s\"",
		t_virtual_player,
		ts_str_maybe_empty(t_stream_id_uri_query));

	if (NULL != pstVirtualPlayer->req_auth.stream_id_uri_query)
	    safe_free(pstVirtualPlayer->req_auth.stream_id_uri_query);

	if (t_stream_id_uri_query)
	    pstVirtualPlayer->req_auth.stream_id_uri_query =
		nkn_strdup_type(ts_str(t_stream_id_uri_query),
			mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->req_auth.stream_id_uri_query = NULL;

	ts_free(&t_stream_id_uri_query);
    }

    /*
     * Get the REQ AUTH AUTH-ID URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "req_auth", "auth_id", "uri_query")) {
	tstring *t_auth_id_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_auth_id_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/req_auth/auth_id_uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_auth_id_uri_query));

	if (NULL != pstVirtualPlayer->req_auth.auth_id_uri_query)
	    safe_free(pstVirtualPlayer->req_auth.auth_id_uri_query);

	if (t_auth_id_uri_query)
	    pstVirtualPlayer->req_auth.auth_id_uri_query =
		nkn_strdup_type(ts_str(t_auth_id_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->req_auth.auth_id_uri_query = NULL;

	ts_free(&t_auth_id_uri_query);
    }

    /*
     * Get the REQ AUTH MATCH URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "req_auth", "match", "uri_query")) {
	tstring *t_match_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_match_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/req_auth/match_uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_match_uri_query));

	if (NULL != pstVirtualPlayer->req_auth.match_uri_query)
	    safe_free(pstVirtualPlayer->req_auth.match_uri_query);

	if (t_match_uri_query)
	    pstVirtualPlayer->req_auth.match_uri_query =
		nkn_strdup_type(ts_str(t_match_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->req_auth.match_uri_query = NULL;

	ts_free(&t_match_uri_query);
    }

    /*
     * Get the REQ AUTH TIME-INTERVAL 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "req_auth", "time_interval")) {
	uint32 t_time_interval = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_time_interval);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/req_auth/time_interval as : \"%d\"",
		t_virtual_player, t_time_interval);
	pstVirtualPlayer->req_auth.time_interval = t_time_interval;
    }

    /*
     * Get the SIGNALS ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "signals", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->signals.active = t_active;
    }

    /*
     * Get the SIGNALS SESSION-ID URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "signals", "session_id", "uri_query")) {
	tstring *t_session_id_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_session_id_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/signals/session_id_uri_query as : \"%s\"",
		t_virtual_player,
		ts_str_maybe_empty(t_session_id_uri_query));

	if (NULL != pstVirtualPlayer->signals.session_id_uri_query)
	    safe_free(pstVirtualPlayer->signals.session_id_uri_query);

	if (t_session_id_uri_query)
	    pstVirtualPlayer->signals.session_id_uri_query =
		nkn_strdup_type(ts_str(t_session_id_uri_query),
			mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->signals.session_id_uri_query = NULL;

	ts_free(&t_session_id_uri_query);
    }

    /*
     * Get the SIGNALS STATE URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "signals", "state", "uri_query")) {
	tstring *t_state_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_state_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/signals/state_uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_state_uri_query));

	if (NULL != pstVirtualPlayer->signals.state_uri_query)
	    safe_free(pstVirtualPlayer->signals.state_uri_query);

	if (t_state_uri_query)
	    pstVirtualPlayer->signals.state_uri_query =
		nkn_strdup_type(ts_str(t_state_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->signals.state_uri_query = NULL;

	ts_free(&t_state_uri_query);
    }

    /*
     * Get the SIGNALS PROFILE URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "signals", "profile", "uri_query")) {
	tstring *t_profile_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_profile_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/signals/profile_uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_profile_uri_query));

	if (NULL != pstVirtualPlayer->signals.profile_uri_query)
	    safe_free(pstVirtualPlayer->signals.profile_uri_query);

	if (t_profile_uri_query)
	    pstVirtualPlayer->signals.profile_uri_query =
		nkn_strdup_type(ts_str(t_profile_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->signals.profile_uri_query = NULL;

	ts_free(&t_profile_uri_query);
    }

    /*
     * Get the SIGNALS CHUNK URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "signals", "chunk", "uri_query")) {
	tstring *t_chunk_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_chunk_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/signals/chunk_uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_chunk_uri_query));

	if (NULL != pstVirtualPlayer->signals.chunk_uri_query)
	    safe_free(pstVirtualPlayer->signals.chunk_uri_query);

	if (t_chunk_uri_query)
	    pstVirtualPlayer->signals.chunk_uri_query =
		nkn_strdup_type(ts_str(t_chunk_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->signals.chunk_uri_query = NULL;

	ts_free(&t_chunk_uri_query);
    }

    /*
     * Get the PRE-STAGE ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "prestage", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->prestage.active = t_active;
    }

    /*
     * Get the PRE-STAGE INDICATOR URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "prestage", "indicator", "uri_query")) {
	tstring *t_indicator_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_indicator_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/prestage/indicator/uri_query as : \"%s\"",
		t_virtual_player,
		ts_str_maybe_empty(t_indicator_uri_query));

	if (NULL != pstVirtualPlayer->prestage.indicator_uri_query)
	    safe_free(pstVirtualPlayer->prestage.indicator_uri_query);

	if (t_indicator_uri_query)
	    pstVirtualPlayer->prestage.indicator_uri_query =
		nkn_strdup_type(ts_str(t_indicator_uri_query),
			mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->prestage.indicator_uri_query = NULL;

	ts_free(&t_indicator_uri_query);
    }

    /*
     * Get the PRE-STAGE NAMESPACE URI QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "prestage", "namespace", "uri_query")) {
	tstring *t_namespace_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_namespace_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/prestage/namespace/uri_query as : \"%s\"",
		t_virtual_player,
		ts_str_maybe_empty(t_namespace_uri_query));

	if (NULL != pstVirtualPlayer->prestage.namespace_uri_query)
	    safe_free(pstVirtualPlayer->prestage.namespace_uri_query);

	if (t_namespace_uri_query)
	    pstVirtualPlayer->prestage.namespace_uri_query =
		nkn_strdup_type(ts_str(t_namespace_uri_query),
			mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->prestage.namespace_uri_query = NULL;

	ts_free(&t_namespace_uri_query);
    }

    /*
     * Get the VIDEO-ID URI-QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "video_id", "uri_query")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/video_id/uri_query as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_uri_query));

	if (NULL != pstVirtualPlayer->video_id.video_id_uri_query)
	    safe_free(pstVirtualPlayer->video_id.video_id_uri_query);

	if (t_uri_query)
	    pstVirtualPlayer->video_id.video_id_uri_query =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->video_id.video_id_uri_query = NULL;

	ts_free(&t_uri_query);
    }

    /*
     * Get the VIDEO-ID FORMAT-TAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "video_id", "format_tag")) {
	tstring *t_format_tag = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_format_tag);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../virtual_player/config/%s/video_id/format_tag as : \"%s\"",
		t_virtual_player, ts_str_maybe_empty(t_format_tag));

	if (NULL != pstVirtualPlayer->video_id.format_tag)
	    safe_free(pstVirtualPlayer->video_id.format_tag);

	if (t_format_tag)
	    pstVirtualPlayer->video_id.format_tag =
		nkn_strdup_type(ts_str(t_format_tag), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->video_id.format_tag = NULL;

	ts_free(&t_format_tag);
    }

    /*
     * Get the VIDEO-ID ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "video_id", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->video_id.active = t_active;
    }
    /*
     * Get the qaulity tag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "quality-tag")) {

	tstring *t_quality_tag = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_quality_tag);
	bail_error(err);

	if (t_quality_tag) {
	    if (NULL != pstVirtualPlayer->smoothstream.quality_tag)
		safe_free(pstVirtualPlayer->smoothstream.quality_tag);
	    pstVirtualPlayer->smoothstream.quality_tag =
		nkn_strdup_type(ts_str(t_quality_tag), mod_mgmt_charbuf);
	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/quality-tag as : \"%s\"",
		    t_virtual_player, ts_str_maybe_empty(t_quality_tag));
	}
	ts_free(&t_quality_tag);
    }
    /*
     * Get the fragment tag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "fragment-tag")) {
	tstring *t_fragment_tag = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_fragment_tag);
	bail_error(err);
	if (t_fragment_tag) {
	    if (NULL != pstVirtualPlayer->smoothstream.fragment_tag)
		safe_free(pstVirtualPlayer->smoothstream.fragment_tag);
	    pstVirtualPlayer->smoothstream.fragment_tag =
		nkn_strdup_type(ts_str(t_fragment_tag), mod_mgmt_charbuf);
	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/fragment-tag as : \"%s\"",
		    t_virtual_player, ts_str_maybe_empty(t_fragment_tag));
	}
	ts_free(&t_fragment_tag);
    }
    /*
     * Get the segment tag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "segment-tag")) {

	tstring *t_segment_tag = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_segment_tag);
	bail_error(err);
	if (t_segment_tag) {
	    if (NULL != pstVirtualPlayer->flashstream.segment_tag)
		safe_free(pstVirtualPlayer->flashstream.segment_tag);
	    pstVirtualPlayer->flashstream.segment_tag =
		nkn_strdup_type(ts_str(t_segment_tag), mod_mgmt_charbuf);
	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/segment-tag as : \"%s\"",
		    t_virtual_player, ts_str_maybe_empty(t_segment_tag));
	}
	ts_free(&t_segment_tag);

    }
    /*
     * Get the segment frag delimiter 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "segment-delimiter-tag")) {

	tstring *t_seg_tag = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_seg_tag);
	bail_error(err);
	if (t_seg_tag) {
	    if (NULL != pstVirtualPlayer->flashstream.segment_frag_delimiter)
		safe_free(pstVirtualPlayer->flashstream.segment_frag_delimiter);
	    pstVirtualPlayer->flashstream.segment_frag_delimiter =
		nkn_strdup_type(ts_str(t_seg_tag), mod_mgmt_charbuf);
	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/segment-delimiter-tag as : \"%s\"",
		    t_virtual_player, ts_str_maybe_empty(t_seg_tag));
	}
	ts_free(&t_seg_tag);

    }
    /*
     * FIX: Created a new node for flashstream fragment tag. Get the
     * flashstream-pub fragment tag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "flash-fragment-tag")) {
	tstring *t_fragment_tag = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_fragment_tag);
	bail_error(err);
	if (t_fragment_tag) {
	    if (NULL != pstVirtualPlayer->flashstream.fragment_tag)
		safe_free(pstVirtualPlayer->flashstream.fragment_tag);
	    pstVirtualPlayer->flashstream.fragment_tag =
		nkn_strdup_type(ts_str(t_fragment_tag), mod_mgmt_charbuf);
	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/flash-fragment-tag as : \"%s\"",
		    t_virtual_player, ts_str_maybe_empty(t_fragment_tag));
	}
	ts_free(&t_fragment_tag);
    }
    /*
     * Bandwidth optimization settings 
     */
    /*
     * Get the bandwidth opt ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "bw_opt", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->bw_opt.active = t_active;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "bw_opt", "ftype", "**")) {
	int i;
	const char *t_ftype = NULL;

	/*
	 * Get the file type string 
	 */
	t_ftype = tstr_array_get_str_quick(name_parts, 7);
	bail_error(err);

	/*
	 * Find free or file type entry 
	 */
	i = 0;
	while ((MAX_FILE_TYPE > i)
		&& (NULL != pstVirtualPlayer->bw_opt.file_info[i].file_type)) {
	    if (0 ==
		    strcmp(pstVirtualPlayer->bw_opt.file_info[i].file_type,
			t_ftype))
		break;
	    i++;
	}

	/*
	 * No entry hence bail out 
	 */
	if (MAX_FILE_TYPE == i)
	    goto bail;

	if (NULL != pstVirtualPlayer->bw_opt.file_info[i].file_type)
	    safe_free(pstVirtualPlayer->bw_opt.file_info[i].file_type);

	if (t_ftype)
	    pstVirtualPlayer->bw_opt.file_info[i].file_type =
		nkn_strdup_type(t_ftype, mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->bw_opt.file_info[i].file_type = NULL;

	/*
	 * Get the Hotness threshold 
	 */
	if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 5, "*", "bw_opt", "ftype", "*",
		 "hotness_threshold")) {
	    int32 t_trigger = 0;

	    err = bn_binding_get_int32(binding, ba_value, NULL, &t_trigger);

	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/bw_opt/ftype/%s/hotness_threshold rate as : \"%d\"",
		    t_virtual_player, t_ftype, t_trigger);
	    pstVirtualPlayer->bw_opt.file_info[i].hotness_threshold = t_trigger;
	}

	/*
	 * Get the Transcode ratio 
	 */
	else if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 5, "*", "bw_opt", "ftype", "*",
		 "transcode_ratio")) {
	    tstring *t_transcode = NULL;

	    err = bn_binding_get_tstr(binding,
		    ba_value, bt_string, NULL, &t_transcode);
	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/bw_opt/ftype/%s/transcode_ratio as : \"%s\"",
		    t_virtual_player, t_ftype, ts_str(t_transcode));

	    if (NULL !=
		    pstVirtualPlayer->bw_opt.file_info[i].transcode_comp_ratio)
		safe_free(pstVirtualPlayer->bw_opt.file_info[i].
			transcode_comp_ratio);

	    if (t_transcode)
		pstVirtualPlayer->bw_opt.file_info[i].transcode_comp_ratio =
		    nkn_strdup_type(ts_str(t_transcode), mod_mgmt_charbuf);
	    else
		pstVirtualPlayer->bw_opt.file_info[i].transcode_comp_ratio =
		    NULL;

	    ts_free(&t_transcode);
	}
	/*
	 * Get the switch limit rate 
	 */
	if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 5, "*", "bw_opt", "ftype", "*", "switch_limit")) {
	    int32 t_limit = 0;

	    err = bn_binding_get_int32(binding, ba_value, NULL, &t_limit);

	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/bw_opt/ftype/%s/switch_limit rate as : \"%d\"",
		    t_virtual_player, t_ftype, t_limit);
	    pstVirtualPlayer->bw_opt.file_info[i].switch_limit_rate = t_limit;
	}

	/*
	 * Get the switch option - lower / higher 
	 */
	else if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 5, "*", "bw_opt", "ftype", "*",
		 "switch_rate")) {
	    tstring *t_rate = NULL;

	    err = bn_binding_get_tstr(binding,
		    ba_value, bt_string, NULL, &t_rate);
	    bail_error(err);

	    lc_log_basic(LOG_DEBUG,
		    "Read .../virtual_player/config/%s/bw_opt/ftype/%s/switch_rate as : \"%s\"",
		    t_virtual_player, t_ftype, ts_str(t_rate));

	    if (NULL != pstVirtualPlayer->bw_opt.file_info[i].switch_rate)
		safe_free(pstVirtualPlayer->bw_opt.file_info[i].switch_rate);

	    if (t_rate)
		pstVirtualPlayer->bw_opt.file_info[i].switch_rate =
		    nkn_strdup_type(ts_str(t_rate), mod_mgmt_charbuf);
	    else
		pstVirtualPlayer->bw_opt.file_info[i].switch_rate = NULL;
	    ts_free(&t_rate);
	}
    }
    /*
     * Get the rate control ACTIVE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "rate_control", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstVirtualPlayer->rate_control.active = t_active;
    }
    /*
     * Get the rate control VALID NODE 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "rate_control", "valid")) {
	tstring *t_valid;

	err = bn_binding_get_tstr(binding, ba_value, bt_link, NULL, &t_valid);
	bail_error(err);
	/*
	 * Now check this node to see which parameter is valid 
	 */
	if (bn_binding_name_pattern_match
		(ts_str_maybe_empty(t_valid),
		 "/nkn/nvsd/virtual_player/config/*/rate_control/query/str"))
	    pstVirtualPlayer->rate_control.active_type =
		VP_PARAM_TYPE_URI_QUERY;
	else if (bn_binding_name_pattern_match
		(ts_str_maybe_empty(t_valid),
		 "/nkn/nvsd/virtual_player/config/*/rate_control/auto_detect"))
	    pstVirtualPlayer->rate_control.active_type = VP_PARAM_TYPE_AUTO;
	else if (bn_binding_name_pattern_match
		(ts_str_maybe_empty(t_valid),
		 "/nkn/nvsd/virtual_player/config/*/rate_control/static/rate"))
	    pstVirtualPlayer->rate_control.active_type = VP_PARAM_TYPE_RATE;
	else
	    pstVirtualPlayer->rate_control.active_type = VP_PARAM_TYPE_AUTO;
	ts_free(&t_valid);
    }
    /*
     * Get the rate control QUERY 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "rate_control", "query", "str")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);
	if (NULL != pstVirtualPlayer->rate_control.qstr)
	    safe_free(pstVirtualPlayer->rate_control.qstr);
	if (t_uri_query)
	    pstVirtualPlayer->rate_control.qstr =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->rate_control.qstr = NULL;
	ts_free(&t_uri_query);
    }
    /*
     * Get the rate control QUERY -rate 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "rate_control", "query", "rate")) {
	tstring *t_uri_query = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_uri_query);
	bail_error(err);
	if (NULL != pstVirtualPlayer->rate_control.qrate)
	    safe_free(pstVirtualPlayer->rate_control.qrate);
	if (t_uri_query)
	    pstVirtualPlayer->rate_control.qrate =
		nkn_strdup_type(ts_str(t_uri_query), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->rate_control.qrate = NULL;
	ts_free(&t_uri_query);
    }
    /*
     * Get the rate-control AUTO FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "rate_control", "auto_detect")) {
	tbool t_auto;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_auto);
	bail_error(err);
	pstVirtualPlayer->rate_control.auto_flag = t_auto;
    }
    /*
     * Get the rate-control static rate 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "rate_control", "static", "rate")) {
	uint32 t_rate = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_rate);
	bail_error(err);
	pstVirtualPlayer->rate_control.static_rate = t_rate;
    }
    /*
     * Get the rate-control scheme 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "rate_control", "scheme")) {
	uint32 t_scheme = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_scheme);
	bail_error(err);
	pstVirtualPlayer->rate_control.scheme = t_scheme;
    }
    /*
     * Get the rate-control burst factor 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "rate_control", "burst")) {
	tstring *t_overhead = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_overhead);
	bail_error(err);
	if (NULL != pstVirtualPlayer->rate_control.burst)
	    safe_free(pstVirtualPlayer->rate_control.burst);

	if (t_overhead && (ts_length(t_overhead) > 0))
	    pstVirtualPlayer->rate_control.burst =
		nkn_strdup_type(ts_str(t_overhead), mod_mgmt_charbuf);
	else
	    pstVirtualPlayer->rate_control.burst = NULL;
	ts_free(&t_overhead);
    }

    /*
     * Set the flag so that we know this is a dynamic change 
     */
    dynamic_change = true;

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_virtual_player_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_virtual_player_delete_cfg_handle_change()
 *	purpose : handler for config delete changes for virtual_player module
 */
static int
nvsd_virtual_player_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_virtual_player = NULL;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/virtual_player/config/**")) {

	/*-------- Get the VirtualPlayer ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_virtual_player = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/virtual_player/config as : \"%s\"",
		t_virtual_player);

	/*
	 * Delete this only if the wildcard change is received 
	 */
	if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 1, "*")) {
	    /*
	     * Get the virtual_player entry in the global array 
	     */
	    pstVirtualPlayer = get_virtual_player_element(t_virtual_player);
	    if (pstVirtualPlayer && (NULL != pstVirtualPlayer->name)) {
		safe_free(pstVirtualPlayer->name);

		if (pstVirtualPlayer->flashstream.fragment_tag)
		    safe_free(pstVirtualPlayer->flashstream.fragment_tag);

		if (pstVirtualPlayer->smoothstream.fragment_tag)
		    safe_free(pstVirtualPlayer->smoothstream.fragment_tag);

		memset(pstVirtualPlayer, 0,
			sizeof (virtual_player_node_data_t));

		/*
		 * Now search thr Namespace list and remove this entry 
		 */
		remove_virtual_player_in_namespace(t_virtual_player);
	    }

	    /*
	     * Set the flag so that we know this is a dynamic change 
	     */
	    dynamic_change = true;

	}
    } else {
	/*
	 * This is not the virtual_player node hence leave 
	 */
	goto bail;
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_service_delete_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_get_virtual_player()
 *	purpose : get the matching Virtual Player's structure
 */
virtual_player_node_data_t *
nvsd_mgmt_get_virtual_player(char *cpVirtualPlayer)
{
    int i;

    /*
     * Sanity Check 
     */
    if (NULL == cpVirtualPlayer)
	return (NULL);

    lc_log_basic(LOG_DEBUG, "Request received for %s virtual_player",
	    cpVirtualPlayer);

    /*
     * Now find the matching Virtual Player entry 
     */
    for (i = 0; i < MAX_VIRTUAL_PLAYERS; i++) {
	if ((NULL != lstVirtualPlayers[i].name) &&
		(0 == strcmp(cpVirtualPlayer, lstVirtualPlayers[i].name)))
	    /*
	     * Return this entry 
	     */
	    return (&(lstVirtualPlayers[i]));
    }

    /*
     * No Match 
     */
    return (NULL);
}	/* end of nvsd_mgmt_get_virtual_player() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_virtual_player_action_hdlr ()
 *	purpose : the api to the virtual player module to handle the various
 *		actions from northbound interfaces
 */
int
nvsd_mgmt_virtual_player_action_hdlr(nvsd_mgmt_actions_t en_action,
	const char *cpname, const char *cpvalue)
{
    char *node_name = NULL;
    char *node_value = NULL;
    int err = 0;

    /*
     * Sanity Test 
     */
    if (!cpname || !cpvalue)
	return (-1);

    /*
     * Check the action type 
     */
    switch (en_action) {
	case NVSD_VP_FS_URI_QUERY:
	    /*
	     * Update the uri_query node 
	     */
	    node_name =
		smprintf
		("/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query",
		 cpname);
	    err = nvsd_mgmt_update_node_str(node_name, cpvalue, bt_string);
	    safe_free(node_name);
	    bail_error(err);

	    /*
	     * Update the active node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
			cpname);
	    err = nvsd_mgmt_update_node_bool(node_name, true);
	    safe_free(node_name);
	    bail_error(err);

	    /*
	     * Update the valid node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
			cpname);
	    node_value =
		smprintf
		("/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query",
		 cpname);
	    err = nvsd_mgmt_update_node_str(node_name, node_value, bt_link);
	    safe_free(node_name);
	    safe_free(node_value);
	    bail_error(err);
	    break;
#if 1
	case NVSD_VP_FS_TIME:
	    /*
	     * Update the time node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/time",
			cpname);
	    err = nvsd_mgmt_update_node_uint32(node_name, atoi(cpvalue));
	    safe_free(node_name);
	    bail_error(err);

	    /*
	     * Update the active node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
			cpname);
	    err = nvsd_mgmt_update_node_bool(node_name, true);
	    safe_free(node_name);
	    bail_error(err);

	    /*
	     * Update the valid node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
			cpname);
	    node_value =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/time",
			cpname);
	    err = nvsd_mgmt_update_node_str(node_name, node_value, bt_link);
	    safe_free(node_name);
	    safe_free(node_value);
	    bail_error(err);
	    break;
#endif
	case NVSD_VP_FS_SIZE:
	    /*
	     * Update the size node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/size",
			cpname);
	    err = nvsd_mgmt_update_node_uint32(node_name, atoi(cpvalue));
	    safe_free(node_name);
	    bail_error(err);

	    /*
	     * Update the active node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
			cpname);
	    err = nvsd_mgmt_update_node_bool(node_name, true);
	    safe_free(node_name);
	    bail_error(err);

	    /*
	     * Update the valid node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
			cpname);
	    node_value =
		smprintf("/nkn/nvsd/virtual_player/config/%s/fast_start/size",
			cpname);
	    err = nvsd_mgmt_update_node_str(node_name, node_value, bt_link);
	    safe_free(node_name);
	    safe_free(node_value);
	    bail_error(err);
	    break;
	default:
	    break;
    }

bail:
    safe_free(node_name);
    return err;
}	/* end of * nvsd_mgmt_virtual_player_action_hdlr () */

/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_virtual_player.c */

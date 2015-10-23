
#include <string.h>
#include <strings.h>
#include <cprops/hashtable.h>

#include "common.h"
#include "dso.h"
#include "md_utils.h"
#include "mdm_events.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "md_main.h"



extern unsigned int jnpr_log_level;

typedef struct uf_context_data {
    wc_name_t uf_name;
    /* configuration of filter-map */
    char *	url;
    char *	crypto_key;
    char *	type;
    uint32_t	refresh_interval;

    /* ++ when there is config update, else zero */
    uint32_t	cfg_updated;

    /* current state of filter-map */
    lt_dur_ms	poll_frequency;
    uint32_t	valid;
    uint32_t	consumed;
    uint32_t	state;
    lt_time_sec last_triggered_time;
    lt_time_sec last_refresh_time;
    lt_time_sec next_refresh_time;
    uint32_t	version;
    file_path_t file_path;

    /* timer data */
    lew_event		*poll_event;
    lew_event_handler	callback;
    void		*context_data; /* Points to iteself */
} uf_context_data_t;

#define NKN_MAX_FILTER_MAPS 256

static uf_context_data_t uf_timers[NKN_MAX_FILTER_MAPS];


typedef struct uf_refresh_data {
    wc_name_t	    uf_name;
    lc_launch_result   *lr;
    file_path_t	    file_path;
    file_path_t     dl_fname;
    uint32	    version;
    uf_context_data_t *context;
} uf_refresh_data_t;

typedef enum {
    eFileURL = 0,
    eCryptoKey,
    eRefreshInterval,
    eFormatType,
} uf_param_type;


enum {
        STATE_UNKNOWN = 0,
        STATE_DOWNLOADING,
        STATE_PARSE_FAILED,
	STATE_DOWNLOAD_FAILED,
	STATE_NO_CHANGE,
	STATE_REFRESHED,
	STATE_INTERNAL_ERROR
};

static md_upgrade_rule_array *md_uf_map_ug_rules = NULL;

int md_url_filter_init(const lc_dso_info *info, void *data);
static int
md_uf_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg);

static int
md_uf_mon_get_time(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg);
static int
md_uf_mon_iterate(md_commit *commit, const mdb_db *db,
	const char *parent_node_name,
	const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg);
static int
md_uf_mon_get_name(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg);
static int
md_uf_mon_get_state(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg);
int
md_uf_update_timer(uf_context_data_t *context);
int
md_uf_timer_callback(int fd,
	short event_type,
	void *data,
	lew_context *ctxt);
int
md_uf_delete_context(const char *uf_name);
int
md_uf_update_param(const char *uf_name, uf_param_type type, const char *value);
int
md_uf_find_context(const char *uf_name, uf_context_data_t **uf_context);
int
md_uf_add_context(const char * uf_name);
static int
md_uf_refresh_async(lc_child_completion_handle *cmpl_hand,
        pid_t pid, int wait_status, tbool completed,
        void *arg);
static int
md_uf_send_update_event(md_commit *commit, char *name,
                            uint32 status, char *filename, const char *error);
int
parse_url(const char *url, char **ret_hostname, char **ret_file);
int
md_uf_refresh_file(md_commit *commit, const mdb_db *db, const char *filter_name,
	const char *force_refresh);
static int
md_uf_refresh_file_action(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg);
static int
md_uf_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg);
int
md_jce_is_attached(md_commit *commit, const mdb_db *db,
	const char *check_name, const char *node_pattern, tbool *attached,
	tstring **matching_node, int position, tstring **matching_name);


int
md_url_filter_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule_array *ra = NULL;
    md_upgrade_rule *rule = NULL;

    err = mdm_add_module("nvsd-url-filter", 2,
	    "/nkn/nvsd/url-filter", "/nkn/nvsd/url-filter/config", 0,
	    NULL, NULL,
	    md_uf_commit_check, NULL,
	    md_uf_commit_apply, NULL,
	    0, 20000,
	    md_generic_upgrade_downgrade, &md_uf_map_ug_rules, NULL,
	    NULL, NULL, NULL,
	    NULL, NULL, &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/config/id/*";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_limit_wc_count_max =  NKN_MAX_FILTER_MAPS;
    node->mrn_limit_str_max_chars = MAX_FILTER_MAP_NAMELEN;
    node->mrn_limit_str_charlist =  CLIST_ALPHANUM "_";
    node->mrn_bad_value_msg =	    "Invalid Name/Max URL Filters configured";
    node->mrn_description =	    "Resource Mgr config nodes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/url-filter/config/id/*/file-url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value = 	   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "URL of file to be downloaded";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/url-filter/config/id/*/type";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value = 	   "url";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_choices_str = ",iwf,calea,url";
    node->mrn_description =        "Format type of file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/url-filter/config/id/*/refresh-interval";
    node->mrn_value_type =	   bt_uint8;
    node->mrn_initial_value_int =  12;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  24;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Time Interval for File refresh";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/url-filter/config/id/*/key";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =	   "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Key to Download file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/mon/id/*";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_mon_get_func =	    md_uf_mon_get_name;
    node->mrn_mon_iterate_func =    md_uf_mon_iterate;;
    node->mrn_description =	    "List of all valid filter-maps";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/mon/id/*/status";
    node->mrn_value_type =	    bt_uint32;
    node->mrn_node_reg_flags =	    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_mon_get_func =	    md_uf_mon_get_state;
    node->mrn_description =	    "Current state of filter-map";
    node->mrn_mon_get_arg =	    (void *) "status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/mon/id/*/file-path";
    node->mrn_value_type =	    bt_uint32;
    node->mrn_node_reg_flags =	    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_mon_get_func =	    md_uf_mon_get_state;
    node->mrn_description =	    "Current state of filter-map";
    node->mrn_mon_get_arg =	    (void *) "fpath";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/mon/id/*/state";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_mon_get_func =	    md_uf_mon_get_state;
    node->mrn_description =	    "Current state of filter-map";
    node->mrn_mon_get_arg =	    (void *) "state";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/mon/id/*/next-time";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_mon_get_func =	    md_uf_mon_get_time;
    node->mrn_mon_get_arg =	    (void *) "nt" ; /* next refresh time */;
    node->mrn_description =	    "Current state of filter-map";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/nvsd/url-filter/mon/id/*/last-trigger";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_mon_get_func =	    md_uf_mon_get_time;
    node->mrn_mon_get_arg =	    (void *) "lt" ; /* last trigger */;
    node->mrn_description =	    "Current state of filter-map";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/url-filter/action/refresh-now";
    node->mrn_node_reg_flags =  mrf_flags_reg_action;
    node->mrn_cap_mask =	mcf_cap_action_basic;
    node->mrn_action_func =	md_uf_refresh_file_action;
    node->mrn_action_arg =	NULL;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description =	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 4);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/url-filter/event/map-status-change";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "";
    node->mrn_events[0]->mre_binding_name = "map_name";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "status";
    node->mrn_events[1]->mre_binding_type = bt_uint32;
    node->mrn_events[2]->mre_binding_name = "file_name";
    node->mrn_events[2]->mre_binding_type = bt_string;
    node->mrn_events[3]->mre_binding_name = "error";
    node->mrn_events[3]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_uf_map_ug_rules);
    bail_error(err);
    ra = md_uf_map_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = 	MUTT_RESTRICT_CHARS;
    rule->mur_name = 		"/nkn/nvsd/url-filter/config/id/*";
    rule->mur_replace_char =	'_';
    rule->mur_allowed_chars =   CLIST_ALPHANUM "_";
    MD_ADD_RULE(ra);

bail:
    return(err);
}

static int
md_uf_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg)
{
    int err = 0;
    int32 num_changes = 0;
    mdb_db_change *change = NULL;
    int i = 0;
    const char *t_name = NULL;
    tstring *t_ns_node = NULL, *t_ns_name = NULL;
    tbool attached = false;


    /*
     * only check we need to put is while deleting, it is not associated
     * with a namespace
     */

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 6, "nkn", "nvsd", "url-filter", "config", "id", "*")) {
	    if (mdct_delete == change->mdc_change_type) {
		/* download file, we have url, type */
		t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
		err = md_jce_is_attached(commit, new_db, t_name,
			"/nkn/nvsd/namespace/*/client-request/filter-map",
			&attached, &t_ns_node, 4, &t_ns_name);
		bail_error(err);

		if (attached) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1, "Filter-map \"%s\" "
			    "is associated with namespace \"%s\". Unable to delete filter-map",
			    t_name, ts_str(t_ns_name));
		    bail_error(err);
		    goto bail;
		}
	    } else if (mdct_add == change->mdc_change_type) {
		t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
		if (t_name && (0 == strcmp(t_name, "list"))) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "list is a reserved name");
		    bail_error(err);
		    goto bail;
		} else if (t_name && (t_name[0] == '_')) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    "Bad Filter-map name. Name cannot"
			    " start with a leading underscore ('_')");
		    bail_error(err);
		    goto bail;
		}
	    }
	}
    }

bail:
    ts_free(&t_ns_node);
    ts_free(&t_ns_name);
    return err;
}

static int
md_uf_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int32 num_changes = 0;
    mdb_db_change *change = NULL;
    int i = 0;
    const char *t_name = NULL;
    tstring *t_value = NULL;
    tbool one_shot = false;

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);
    if (one_shot) {
	goto bail;
    }
    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 6, "nkn", "nvsd", "url-filter", "config", "id", "*")) {
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
	    if ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type)){
		/* download file, we have url, type */
		err = md_uf_add_context(t_name);
		bail_error(err);
	    } else if (mdct_delete == change->mdc_change_type) {
		err = md_uf_delete_context(t_name);
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 7, "nkn", "nvsd", "url-filter", "config", "id", "*", "file-url")
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type))) {
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
	    ts_free(&t_value);
	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
		    0, NULL, &t_value);
	    bail_error(err);

	    err = md_uf_update_param(t_name, eFileURL, ts_str(t_value));
	    bail_error(err);

	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 7, "nkn", "nvsd", "url-filter", "config", "id", "*", "type")
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type))) {
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
	    ts_free(&t_value);
	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
		    0, NULL, &t_value);
	    bail_error(err);

	    err = md_uf_update_param(t_name,eFormatType, ts_str(t_value));
	    bail_error(err);

	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 7, "nkn", "nvsd", "url-filter", "config", "id", "*", "refresh-interval")
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type))) {
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
	    ts_free(&t_value);
	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
		    0, NULL, &t_value);
	    bail_error(err);

	    err = md_uf_update_param(t_name, eRefreshInterval, ts_str(t_value));
	    bail_error(err);

	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 7, "nkn", "nvsd", "url-filter", "config", "id", "*", "key")
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type))) {
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
	    ts_free(&t_value);
	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
		    0, NULL, &t_value);
	    bail_error(err);

	    err = md_uf_update_param(t_name, eCryptoKey, ts_str(t_value));
	    bail_error(err);
	}
    }

    for (i = 0 ; i < NKN_MAX_FILTER_MAPS; i++) {
	if (uf_timers[i].cfg_updated && uf_timers[i].valid) {
	    /* one of the param has been updated, add/reset timer */
	    err = md_uf_update_timer(&uf_timers[i]);
	    bail_error(err);
	}
    }


bail:
    ts_free(&t_value);
    return err;
}

static int
md_uf_refresh_file_action(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg)
{
    int err = 0;
    char *name = NULL;
    const bn_binding *binding = NULL;

    /* XXX: add action handling here */

    err = bn_binding_array_get_binding_by_name(params, "name", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, 0, &name);
    bail_error_null(err, name);

    err = md_uf_refresh_file(commit, db, name, "1");
    bail_error(err);

bail:
    return err;
}

int
md_uf_refresh_file(md_commit *commit, const mdb_db *db, const char *filter_name,
	const char *force_refresh)
{
    int err = 0;
    node_name_t node_name = {0};
    char *uri = NULL;
    char *file = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    char *filter_data = NULL;
    tstring *t_url = NULL, *t_type = NULL;
    uf_refresh_data_t *edata = NULL;
    uf_context_data_t *context = NULL;

    err = md_uf_find_context(filter_name, &context);
    bail_error(err);
    bail_null(context);

    /* ensure context is valid */
    if (context->valid == 0)  {
	lc_log_basic(LOG_NOTICE, "Filter-map %s is not valid, so not refreshing",
		context->uf_name);
	goto bail;
    }

    /* check if we are already downloading a file, dont download again */
    if (context->state == STATE_DOWNLOADING ) {
	lc_log_basic(LOG_INFO, "Already a download is in process, please try later");
	if (commit) {
	    err = md_commit_set_return_status_msg_fmt(commit, 1, "Filter-map \"%s\" "
		    "is being processed, please try again later.",
		    filter_name );
	    bail_error(err);
	}
	goto bail;
    } else if ((lt_curr_time() - context->last_refresh_time) < 20) {
	/* check if we completed a download in last 20 secs, don't download again */
	lc_log_basic(LOG_INFO, "Last refresh happened at 20 secs ago, not triggering again");
	if (commit) {
	    err = md_commit_set_return_status_msg_fmt(commit, 1, "Filter-map \"%s\" "
		    "is being processed, please try again later.",
		    filter_name );
	    bail_error(err);
	}
	goto bail;
    }
    err = parse_url(context->url, &uri, &file);
    bail_error(err);

    bail_null(uri);
    bail_null(file);

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    err = ts_new_str(&(lp->lp_path), "/opt/nkn/bin/fetch_uf_list");
    bail_error(err);

    /*
     * /root/fetch_uf_list -t url -r 2 -c abc -f abc
     * -d cmbu-build04.juniper.net/data/users/dgautam -l sample-uf_data.txt
     */
    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 11,
	    "/opt/nkn/bin/fetch_uf_list", "-f", filter_name, "-d", uri,
	    "-l", file, "-t", context->type, "-r", force_refresh);
    bail_error(err);;

    if (context->crypto_key) {
	err = tstr_array_append_str(lp->lp_argv, "-c");
	bail_error(err);
	err = tstr_array_append_str(lp->lp_argv, context->crypto_key);
	bail_error(err);
    }
    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    lr = malloc(sizeof(lc_launch_result));
    bail_null(lr);

    memset(lr, 0, sizeof(lc_launch_result));

    context->state = STATE_DOWNLOADING;
    context->last_triggered_time = lt_curr_time();

    err = lc_launch(lp,lr);
    bail_error(err);

    edata = malloc(sizeof(uf_refresh_data_t));
    bail_null(edata);

    edata->lr = lr;
    edata->context  = context;
    snprintf(edata->uf_name, sizeof(wc_name_t), "%s", filter_name);
    snprintf(edata->dl_fname, sizeof(file_path_t), "%s", file);

    err = lc_child_completion_register
	(md_cmpl_hand, lr->lr_child_pid,
	 md_uf_refresh_async,(void * )edata);
    bail_error(err);


bail:
    if (err) {
	safe_free(filter_data);
	context->state = STATE_INTERNAL_ERROR;
    }
    lc_launch_params_free(&lp);
    return err;
}

int
parse_url(const char *url, char **ret_hostname, char **ret_file)
{
    int err = 0;
    const char *cl = NULL, *nl = NULL;

    if (ret_hostname) {
	*ret_hostname = NULL;
    }
    if (*ret_file) {
	*ret_file = NULL;
    }
    bail_null(url);

    if (!lc_is_prefix("http://", url, false)) {
	goto bail;
    }

    cl = url + strlen("http://");
    if (!*cl) {
	err = lc_err_bad_path;
	goto bail;
    }

    nl = rindex(cl, '/');
    if (!nl) {
	err = lc_err_bad_path;
	goto bail;
    }

    if (ret_hostname) {
	*ret_hostname = strldup(cl, nl - cl + 1);
    }
    if (ret_file) {
	*ret_file = strdup(nl+1);
    }
bail:
    return (err);
}

static int
md_uf_refresh_async(lc_child_completion_handle *cmpl_hand,
        pid_t pid, int wait_status, tbool completed,
        void *arg)
{
    int err = 0, status = 0;
    const tstring *output = NULL;
    file_path_t fpath = {0};
    uf_refresh_data_t *edata = arg;
    uf_context_data_t *context = edata->context;

    bail_null(edata);
    bail_null(context);


    if(completed) {
	edata->lr->lr_child_pid = -1;
	edata->lr->lr_exit_status = wait_status;
    } else {
	goto bail;
    }

    bzero(edata->file_path, sizeof(edata->file_path));
    bzero(context->file_path, sizeof(context->file_path));

    err = lc_launch_complete_capture(edata->lr);
    bail_error(err);

    output = edata->lr->lr_captured_output;
    bail_null(output);

    if(strstr(ts_str(output),"RETURN=0")) {
	/*Successfully refreshed  */
	status = 0;
	context->state = STATE_REFRESHED;
	snprintf(edata->file_path, sizeof(edata->file_path),
		"/nkn/tmp/uf/%s/%s.bin", edata->uf_name, edata->dl_fname);
    } else if (strstr(ts_str(output),"RETURN=1")) {
	/* wget failed */
	status = 1;
	context->state = STATE_DOWNLOAD_FAILED;
	lc_log_basic(LOG_NOTICE, "%s", ts_str(output));
    } else if (strstr(ts_str(output),"RETURN=2")) {
	/* downloaded file is same as last download */
	status = 2;
	context->state = STATE_NO_CHANGE;
	snprintf(edata->file_path, sizeof(edata->file_path),
		"/nkn/tmp/uf/%s/%s.bin", edata->uf_name, edata->dl_fname);
    } else if (strstr(ts_str(output),"RETURN=3")) {
	/* downloaded file is invalid and parsing is failing for it */
	status = 3;
	context->state = STATE_PARSE_FAILED;
	lc_log_basic(LOG_NOTICE, "%s", ts_str(output));
    } else {
	/* some other error occured */
	status = 4;
	context->state = STATE_INTERNAL_ERROR;
    }

    snprintf(context->file_path, sizeof(context->file_path), "%s", edata->file_path);

    err = md_uf_send_update_event(NULL, edata->uf_name, status, edata->file_path,
	    ts_str_maybe_empty(output));
    bail_error(err);

bail:
    if (completed) {
	context->last_refresh_time = lt_curr_time();
	lc_launch_result_free_contents(edata->lr);
	safe_free(edata->lr);
	safe_free(edata);
    }
    return err;
}

static int
md_uf_send_update_event(md_commit *commit, char *name,
                            uint32 status, char *filename, const char *error)
{
    int err = 0;
    bn_request *req = NULL;
    char state[32] = {0};
    tstring *ret_msg = NULL;
    uint16_t ret_code = 0;

    err = bn_event_request_msg_create(&req,
	    "/nkn/nvsd/url-filter/event/map-status-change");
    bail_error(err);

    err = bn_event_request_msg_append_new_binding (req, 0, "map_name",
	    bt_string, name, NULL);
    bail_error(err);

    snprintf(state, 32, "%d", status);

    err = bn_event_request_msg_append_new_binding (req, 0, "status",
	    bt_uint32, (char *)&state, NULL);
    bail_error(err);

    err = bn_event_request_msg_append_new_binding (req, 0, "file_name",
	    bt_string, filename, NULL);
    bail_error(err);

    err = bn_event_request_msg_append_new_binding (req, 0, "error",
	    bt_string, error, NULL);
    bail_error(err);

    err = md_commit_handle_event_from_module
	(commit, req, &ret_code, &ret_msg, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int
md_jce_is_attached(md_commit *commit, const mdb_db *db,
	const char *check_name, const char *node_pattern, tbool *attached,
	tstring **matching_node, int position, tstring **matching_name)
{
    int err = 0, node_num = 0, j = 0;
    tstr_array * node_array = NULL, *t_name_parts = NULL;
    tstring *node_value = NULL;

    bail_null(attached);
    bail_null(check_name);
    bail_null(node_pattern);

    *attached = false;
    if (matching_node) {
	*matching_node = NULL;
    }
    if (matching_name) {
	*matching_name = NULL;
    }


    err = mdb_get_matching_tstr_array(commit,
	    db, node_pattern , 0, &node_array);
    bail_error(err);

    if (node_array == NULL) {
	goto bail;
    }

    node_num = tstr_array_length_quick(node_array);

    for(j = 0; j < node_num; j++) {
	ts_free(&node_value);
	err = mdb_get_node_value_tstr(commit, db,
		tstr_array_get_str_quick(node_array, j),
		0, NULL,
		&node_value);
	bail_error(err);

	if (node_value && ts_equal_str(node_value, check_name, false)) {
	    /* we got the match */
	    if (matching_node) {
		err = ts_new_str(matching_node,
			tstr_array_get_str_quick(node_array, j));
		bail_error(err);
	    }
	    if (matching_name) {
		err = ts_tokenize_str(
			tstr_array_get_str_quick(node_array, j),
			'/', '\0', '\0', 0, &t_name_parts);
		bail_error(err);

		err = ts_new_str(matching_name,
			tstr_array_get_str_quick(t_name_parts, position));
		bail_error(err);
	    }
	    *attached = true;
	    break;
	}
    }


bail:
    if (err) {
	if (matching_name) {
	    ts_free(matching_name);
	}
	if (matching_node) {
	    ts_free(matching_node);
	}
    }
    ts_free(&node_value);
    tstr_array_free(&node_array);
    tstr_array_free(&t_name_parts);
    return err;
}

int
md_uf_add_context(const char * uf_name)
{
    int err = 0, i = 0;
    uf_context_data_t *context = NULL;

    for (i = 0; i < NKN_MAX_FILTER_MAPS; i++) {
	if (uf_timers[i].consumed == 0) {
	    context = &uf_timers[i];
	    break;
	}
    }

    snprintf(context->uf_name, sizeof(context->uf_name), "%s", uf_name);
    context->valid = 0;
    context->consumed = 1;
    context->callback = md_uf_timer_callback;
    context->state = STATE_UNKNOWN;

    return err;
}

int
md_uf_find_context(const char *uf_name, uf_context_data_t **uf_context)
{
    int err = 0, i = 0;
    uf_context_data_t *context = NULL;

    for (i = 0; i < NKN_MAX_FILTER_MAPS; i++) {
	if (uf_timers[i].consumed == 0) {
	    continue;
	}
	if (0 == strcmp(uf_timers[i].uf_name, uf_name)) {
	    context = &uf_timers[i];
	    break;
	}
    }
    *uf_context = context;

    return err;
}

int
md_uf_update_param(const char *uf_name, uf_param_type type, const char *value)
{
    int err = 0;
    uf_context_data_t *context = NULL;

    bail_null(uf_name);
    bail_null(value);

    err = md_uf_find_context(uf_name, &context);
    bail_error(err);
    bail_null(context);

    switch(type) {
	case eFileURL:
	    if (value && strcmp(value, "")) {
		safe_free(context->url);
		context->url = smprintf("%s", value);
		context->valid = 1;
	    } else {
		context->url = NULL;
		context->valid = 0;
	    }
	    break;
	case eCryptoKey:
	    /* if value is non-empty */
	    if (value && strcmp(value, "")) {
		safe_free(context->crypto_key);
		context->crypto_key = smprintf("%s", value);
	    } else {
		context->crypto_key = NULL;
	    }
	    break;
	case eRefreshInterval:
	    context->refresh_interval = strtoul(value, NULL, 10);
	    /* milli seconds in 1 hr: 60*60*1000 */
	    context->poll_frequency = context->refresh_interval * 60 *60 * 1000;
	    break;
	case eFormatType:
	    safe_free(context->type);
	    context->type = smprintf("%s", value);
	    break;
	default:
	    err = 1;
	    goto bail;
	    break;
    }
    context->cfg_updated++;

bail:
    return err;
}

int
md_uf_delete_context(const char *uf_name)
{
    int err = 0;
    uf_context_data_t *context = NULL;
    file_path_t uf_dir = {0};

    bail_null(uf_name);

    err = md_uf_find_context(uf_name, &context);
    bail_error(err);
    bail_null(context);

    context->valid = 0;
    context->consumed = 0;
    context->cfg_updated = 0;
    bzero(context->uf_name, sizeof(context->uf_name));

    safe_free(context->url);
    safe_free(context->crypto_key);
    safe_free(context->type);

    if (context->poll_event) {
	/* delete this timer */
	err = lew_event_delete(md_lwc, &(context->poll_event));
	complain_error(err);
	context->poll_event = NULL;
    }
bail:
    snprintf(uf_dir, sizeof(uf_dir), "/nkn/tmp/uf/%s", uf_name);
    lf_remove_tree(uf_dir);
    return err;
}

int
md_uf_update_timer(uf_context_data_t *context)
{
    int err = 0;

    if (context->poll_event) {
	/* delete this timer */
	err = lew_event_delete(md_lwc, &(context->poll_event));
	complain_error(err);
	context->poll_event = NULL;
    }

    context->cfg_updated = 0;

    if (context->valid == 0) {
	goto bail;
    }

    /* refresh file force-fully now */
    err = md_uf_refresh_file(NULL, NULL, context->uf_name, "1");
    complain_error(err);

    /* adding timer with to trigger timer at scheduled */
    if (context->refresh_interval > 0) {
	context->next_refresh_time = lt_curr_time() + context->poll_frequency/1000;
	err = lew_event_reg_timer(md_lwc,
		&(context->poll_event),
		(context->callback),
		(void *) (context),
		(context->poll_frequency));
	bail_error(err);
    }


bail:
    return err;
}

int
md_uf_timer_callback(int fd,
	short event_type,
	void *data,
	lew_context *ctxt)
{
    int err = 0;
    uf_context_data_t *context = (uf_context_data_t *) data;


    err = md_uf_refresh_file(NULL, NULL, context->uf_name, "0");
    complain_error(err);

    /* add timer again even if error occured */
    if (context->refresh_interval > 0) {
	context->next_refresh_time = lt_curr_time() + context->poll_frequency/1000;

	err = lew_event_reg_timer(md_lwc,
		&(context->poll_event),
		(context->callback),
		(void *) (context),
		(context->poll_frequency));
	bail_error(err);
    }

bail:
    return err;

}
static int
md_uf_mon_iterate(md_commit *commit, const mdb_db *db,
	const char *parent_node_name,
	const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg)
{
    int err = 0, i = 0;

    for (i = 0; i < NKN_MAX_FILTER_MAPS; i++) {
	if (uf_timers[i].consumed == 0) {
	    /* empty slot, goto next */
	    continue;
	}
	err = (*iterate_cb)(commit, db, uf_timers[i].uf_name, iterate_cb_arg);
	bail_error(err);
    }

bail:
    return err;
}

static int
md_uf_mon_get_name(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    const char *uf_name = NULL;
    tstr_array *parts = NULL;


    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    uf_name = tstr_array_get_str_quick(parts, 5);
    bail_null(uf_name);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, uf_name);
    bail_error_null(err, *ret_binding);

bail:
    tstr_array_free(&parts);
    return err;

}
static int
md_uf_mon_get_state(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    const char *uf_name = NULL;
    tstr_array *parts = NULL;
    uf_context_data_t *context = NULL;
    const char *state = NULL;
    const char *status = arg;


    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    uf_name = tstr_array_get_str_quick(parts, 5);
    bail_null(uf_name);

    err = md_uf_find_context(uf_name, &context);
    bail_error(err);

    if (0 == strcmp(status, "status")) {
	str_value_t str_value = {0};
	snprintf(str_value, sizeof(str_value), "%d",context->state);
	err = bn_binding_new_str(ret_binding, node_name, ba_value,
		bt_uint32, 0, str_value);
	bail_error_null(err, *ret_binding);
	goto bail;
    }
    if (0 == strcmp(status, "fpath")) {
	str_value_t str_value = {0};
	err = bn_binding_new_str(ret_binding, node_name, ba_value,
		bt_string, 0, context->file_path);
	bail_error_null(err, *ret_binding);
	goto bail;
    }
    switch(context->state) {
	case STATE_UNKNOWN:
	    state = "Unknown";
	    break;
	case STATE_DOWNLOADING:
	    state = "Refresh Initiated";
	    break;
	case STATE_DOWNLOAD_FAILED:
	    state = "Download Failed";
	    break;
	case STATE_PARSE_FAILED:
	    state = "Parse Failed";
	    break;
	case STATE_REFRESHED:
	    state = "Refreshed";
	    break;
	case STATE_INTERNAL_ERROR:
	    state = "Internal error occured, Check logs";
	    break;
	case STATE_NO_CHANGE:
	    state = "URL file not changed";
	    break;
	default:
	    state = "Unknown status";
	    break;
    }

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, state);
    bail_error_null(err, *ret_binding);

bail:
    tstr_array_free(&parts);
    return err;

}
static int
md_uf_mon_get_time(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    const char *uf_name = NULL, *uf_time_type = arg;
    tstr_array *parts = NULL;
    uf_context_data_t *context = NULL;
    lt_time_sec time_stamp = 0;


    bail_null(uf_time_type);

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    /* /nkn/nvsd/url-filter/mon/id/ * /lr */
    uf_name = tstr_array_get_str_quick(parts, 5);
    bail_null(uf_name);

    err = md_uf_find_context(uf_name, &context);
    bail_error(err);

    if (0 == strcmp(uf_time_type, "nt")) {
	time_stamp = context->next_refresh_time;
    } else if (0 == strcmp(uf_time_type, "lt")) {
	time_stamp = context->last_triggered_time;
    } else {
	lc_log_basic(LOG_WARNING, "Unknown Time Type");
	goto bail;
    }
    err = bn_binding_new_datetime_sec(ret_binding, node_name, ba_value, 0,
	    time_stamp);

    bail_error_null(err, *ret_binding);

bail:
    tstr_array_free(&parts);
    return err;

}

#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"

#include "stdint.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_mgmt_defs.h"
#include "nkn_cse.h"
#include "cse_mgmt.h"


const uint32 cse_num_ext_mon_nodes = 100;
extern md_client_context *cse_mcc;
extern  pthread_mutex_t g_cse_timerq_mutex ;
extern unsigned int jnpr_log_level;
extern TAILQ_HEAD(timer_head, crawler_context) g_cse_timerq_head;

void nkn_cse_get_client_domain_from_url(crawler_context_t *obj);

/* Local Function Prototypes */

int nkn_cse_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
				     bn_binding *binding, void *data);

int nkn_cse_delete_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
				     bn_binding *binding, void *data);

/* Local Variables */
static const char nkn_cse_config_prefix[] = "/nkn/crawler/config/profile";
static const char nkn_cse_url_binding[] = "/nkn/crawler/config/profile/*/url"; /*bt_string */
static const char nkn_cse_transport_binding[] = "/nkn/crawler/config/profile/*/transport"; /*bt_uint16 */
static const char nkn_cse_start_time_binding[] = "/nkn/crawler/config/profile/*/schedule/start"; /*bt_datetime_sec */
static const char nkn_cse_stop_time_binding[] = "/nkn/crawler/config/profile/*/schedule/stop"; /*bt_datetime_sec */
static const char nkn_cse_refresh_binding[] = "/nkn/crawler/config/profile/*/schedule/refresh"; /*bt_uint32 */
static const char nkn_cse_link_depth_binding[] = "/nkn/crawler/config/profile/*/link_depth"; /*bt_uint8 */
static const char nkn_cse_file_extn_list_binding[] = "/nkn/crawler/config/profile/*/file_extension/*"; /*bt_string array */
static const char nkn_cse_status_binding[] = "/nkn/crawler/config/profile/*/status"; /*bt_uint8 */
static const char nkn_cse_client_domain_binding[] = "/nkn/crawler/config/profile/*/client_domain"; /*bt_string */
static const char nkn_cse_skip_preload_binding[] = "/nkn/crawler/config/profile/*/file_extension/*/skip_preload"; /*bt_bool array */
static const char nkn_cse_autogen_binding[] = "/nkn/crawler/config/profile/*/auto_generate"; /*bt_uint8 */
static const char nkn_cse_autogen_type_binding[] = "/nkn/crawler/config/profile/*/auto_generate_dest/*"; /*bt_string array */
static const char nkn_cse_autogen_src_binding[] = "/nkn/crawler/config/profile/*/auto_generate_src/*"; /*bt_string array */
static const char nkn_cse_autogen_expiry_binding[] = "/nkn/crawler/config/profile/*/auto_generate_src/*/auto_gen_expiry"; /*bt_uint64 */
static const char nkn_cse_origin_response_expiry_binding[] = "/nkn/crawler/config/profile/*/origin_response_expiry"; /*bt_bool */
static const char nkn_cse_xdomain_binding[] = "/nkn/crawler/config/profile/*/xdomain"; /*bt_bool */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cse_cfg_query()
 *	purpose : query for nkn 'content sync engine' config parameters
 */
int
nkn_cse_cfg_query(void)
{
        int err = 0;
        bn_binding_array *bindings = NULL;
        tbool rechecked_licenses = false;

        lc_log_debug(LOG_DEBUG,
		     "nkn cse module mgmtd query initializing ..");
        err = mdc_get_binding_children(cse_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
                        true,
                        nkn_cse_config_prefix);
        bail_error_null(err, bindings);

	pthread_mutex_lock(&g_cse_timerq_mutex);
        err = bn_binding_array_foreach(bindings,
						nkn_cse_cfg_handle_change,
				       &rechecked_licenses);
	pthread_mutex_unlock(&g_cse_timerq_mutex);
        bail_error(err);

bail:
        bn_binding_array_free(&bindings);

        return err;

} /* end of nkn_cse_cfg_query() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cse_module_cfg_handle_change()
 *	purpose : handler for config changes for nkn 'content sync engine' config parameters module
 */
int
nkn_cse_module_cfg_handle_change (bn_binding_array *old_bindings,
					 bn_binding_array *new_bindings)
{
	int err = 0;
	tbool rechecked_licenses = false;

	/* Call the callbacks */
	pthread_mutex_lock(&g_cse_timerq_mutex);
	err = bn_binding_array_foreach(new_bindings,
						nkn_cse_cfg_handle_change,
				       &rechecked_licenses);
	bail_error(err);

	err = bn_binding_array_foreach(old_bindings,
						nkn_cse_delete_cfg_handle_change,
				       &rechecked_licenses);
	bail_error(err);


bail:
	pthread_mutex_unlock(&g_cse_timerq_mutex);
	return err;

} /* end of nkn_cse_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cse_cfg_handle_change()
 *	purpose : handler for config changes for cse module
 */
int
nkn_cse_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    const char *t_crawler_profile = NULL;
    crawler_context_t *obj = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(data);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is our node */
    if (bn_binding_name_pattern_match (ts_str(name), "/nkn/crawler/config/profile/**"))
    {
    	/*-------- Get the crawler profile ------------*/
    	bn_binding_get_name_parts (binding, &name_parts);
    	bail_error_null(err, name_parts);

    	t_crawler_profile = tstr_array_get_str_quick (name_parts, 4);
    	bail_error(err);

	lc_log_debug(LOG_DEBUG, "Read .../nkn/crawler/config/profile as : \"%s\"",
    		t_crawler_profile);


	/* Get the crawler profile entry from the TAILQ */
	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {
	    if(!strcmp(t_crawler_profile,obj->cfg.name)){
		break;
	    }
	}
	if (NULL == obj)
	{
	    /* Allocate memory for the crawler context & fill the name*/
	    /* This allocated memory will be freed in the CSE application*/


	    obj = (crawler_context_t *)malloc(sizeof(crawler_context_t));
	    memset(obj, 0, sizeof(crawler_context_t));

	    strlcpy(obj->cfg.name, t_crawler_profile, MAX_CRAWLER_NAME_SIZE);
	    pthread_mutex_init(&obj->crawler_ctxt_mutex, NULL);
	    TAILQ_INSERT_TAIL(&g_cse_timerq_head, obj, timer_entries);
	    lc_log_debug(LOG_DEBUG,"added profile: %s as %s",t_crawler_profile, obj->cfg.name );
	    /*
	     * Add counters to  nkn_counters 
	     */
	    char counter_str[512];
	    snprintf(counter_str, 512,
		    "glob_cse_%s_total_crawls", obj->cfg.name);
	    nkn_mon_add(counter_str, NULL,
			    (void *)&obj->stats.num_total_crawls,
			    sizeof(obj->stats.num_total_crawls));
	    snprintf(counter_str, 512,
		    "glob_cse_%s_crawl_fail_cnt", obj->cfg.name);
	    nkn_mon_add(counter_str, NULL,
			    (void *)&obj->stats.num_failures,
			    sizeof(obj->stats.num_failures));
	    snprintf(counter_str, 512,
		    "glob_cse_%s_schedule_miss_cnt", obj->cfg.name);
	    nkn_mon_add(counter_str, NULL,
			    (void *)&obj->stats.num_schedule_misses,
			    sizeof(obj->stats.num_schedule_misses));
	    snprintf(counter_str, 512,
		    "glob_cse_%s_force_termination_cnt", obj->cfg.name);
	    nkn_mon_add(counter_str, NULL,
			    (void *)&obj->stats.num_wget_kills,
			    sizeof(obj->stats.num_wget_kills));
	    snprintf(counter_str, 512,
		    "glob_cse_%s_origin_down_cnt", obj->cfg.name);
	    nkn_mon_add(counter_str, NULL,
			    (void *)&obj->stats.num_origin_down_cnt,
			    sizeof(obj->stats.num_origin_down_cnt));
	}
    }
    else
    {
    	/* This is not the cse node hence leave */
    	goto bail;
    }

    if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_url_binding)) {
   		tstring *t_url= NULL;

   		err = bn_binding_get_tstr(binding,
   				ba_value, bt_string, NULL,
   				&t_url);
   		bail_error(err);

   		lc_log_debug(LOG_DEBUG, "Read .../crawler/profile/%s/url as : \"%s\"",
   				t_crawler_profile, (t_url) ? ts_str(t_url) : "");

   		if (t_url && ts_length(t_url) > 0){
			strncpy(obj->cfg.url, ts_str(t_url), MAX_CRAWLER_URI_SIZE);
			nkn_cse_get_client_domain_from_url(obj);
   		}

   		ts_free(&t_url);
   	}
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_client_domain_binding)) {
   		tstring *t_client_domain= NULL;

   		err = bn_binding_get_tstr(binding,
   				ba_value, bt_string, NULL,
   				&t_client_domain);
   		bail_error(err);

   		lc_log_debug(LOG_DEBUG, "Read .../crawler/profile/%s/url as : \"%s\"",
   				t_crawler_profile, (t_client_domain) ? ts_str(t_client_domain) : "");

   		if (t_client_domain && ts_length(t_client_domain) > 0){
			if (ts_length(t_client_domain) > CRAWLER_CLIENT_DOMAIN_SIZE - 1){
			    syslog(LOG_ERR,
				    "Client domain size configured is greater than %d characters",
				    CRAWLER_CLIENT_DOMAIN_SIZE);
			} else {
			    strncpy(obj->cfg.client_domain, ts_str(t_client_domain),
				    CRAWLER_CLIENT_DOMAIN_SIZE - 1);
			}
   		}

   		ts_free(&t_client_domain);
   	}

	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_start_time_binding))
	{
		lt_time_sec start_time = 0;

		err = bn_binding_get_datetime_sec(binding,
				ba_value, NULL,
				&start_time);

		bail_error(err);

		lc_log_debug(LOG_DEBUG, "Read ...Read .../crawler/profile/%s/schedule/start as : \"%d\"",
				t_crawler_profile, start_time);

		obj->cfg.start_time = start_time;
	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_stop_time_binding))
	{
		lt_time_sec stop_time = 0;

		err = bn_binding_get_datetime_sec(binding,
				ba_value, NULL,
				&stop_time);

		bail_error(err);

		lc_log_debug(LOG_DEBUG, "Read ...Read .../crawler/profile/%s/schedule/stop as : \"%d\"",
				t_crawler_profile, stop_time);

		obj->cfg.stop_time = stop_time;
	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_refresh_binding))
	{
		uint32 t_refresh = 0;

		err = bn_binding_get_uint32(binding,
				ba_value, NULL,
				&t_refresh);
		bail_error(err);

		lc_log_debug(LOG_DEBUG, "Read ...Read .../crawler/profile/%s/schedule/refresh as : \"%d\"",
				t_crawler_profile, t_refresh);

		obj->cfg.refresh_interval = t_refresh;
	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_link_depth_binding))
	{
		uint8 t_depth = 0;

		err = bn_binding_get_uint8(binding,
				ba_value, NULL,
				&t_depth);
		bail_error(err);

		lc_log_debug(LOG_DEBUG, "Read ...Read .../crawler/profile/%s/link_depth as : \"%d\"",
				t_crawler_profile, t_depth);

		obj->cfg.link_depth = t_depth;
	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_status_binding))
	{
		uint8 t_crawler_status;

		err = bn_binding_get_uint8(binding,
				ba_value, NULL,
				&t_crawler_status);
		bail_error(err);
		if(t_crawler_status){
			obj->cfg.status = ACTIVE;
		}
		else{
			obj->cfg.status = INACTIVE;
		}
	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_file_extn_list_binding)) {
   		const char *t_file_ext = NULL;
   		uint8 position = 0;

		bn_binding_get_name_parts (binding, &name_parts);
		bail_error(err);
		t_file_ext = tstr_array_get_str_quick (name_parts, 6);

   		lc_log_debug(LOG_DEBUG, "Read .../crawler/profile/%s/file_extension/* as : \"%s\"",
   				t_crawler_profile, (t_file_ext) ? t_file_ext : "");

   		if (t_file_ext && strlen(t_file_ext) > 0){
   			/* append in the list if not exists*/
   			for(position =0 ; position <(obj->num_accept_extns); position++)
   			{
   			    if(!strcmp(t_file_ext,obj->cfg.accept_extn_list[position].extn_name))
   				goto bail;
   			}

			strncpy(obj->cfg.accept_extn_list[obj->num_accept_extns].extn_name, t_file_ext, MAX_ACCEPT_EXTN_STR_SIZE);
			obj->cfg.accept_extn_list[obj->num_accept_extns].extn_len = strlen(t_file_ext);
   			obj->num_accept_extns++;
   		}
   	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_skip_preload_binding)) {
   		tbool t_skip_preload = false;
   		uint8 position = 0;
  		const char *t_file_ext = NULL;

		bn_binding_get_name_parts (binding, &name_parts);
		bail_error(err);
		t_file_ext = tstr_array_get_str_quick (name_parts, 6);

   		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_skip_preload);
		bail_error(err);

   		if (t_file_ext && strlen(t_file_ext) > 0){
   			/*search the file extn & get the position*/
   			for(position =0 ; position <(obj->num_accept_extns); position++)
   			{
   			    if(!strcmp(t_file_ext,obj->cfg.accept_extn_list[position].extn_name))
   				break;
   			}
			obj->cfg.accept_extn_list[position].skip_preload = t_skip_preload ? 1:0;
   		}
   	}
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_autogen_binding)) {
   		uint8 t_autogen = 0;

		err = bn_binding_get_uint8(binding,
				ba_value, NULL,
				&t_autogen);
		bail_error(err);

   		obj->cfg.extn_action_list[obj->num_extn_actions].action_type = t_autogen;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_autogen_type_binding)) {
   		tstring *t_autogen_type = NULL;

   		err = bn_binding_get_tstr(binding,
   				ba_value, bt_string, NULL,
   				&t_autogen_type);
   		bail_error(err);

   		lc_log_debug(LOG_DEBUG, "Read .../crawler/profile/%s/url as : \"%s\"",
   				t_crawler_profile, (t_autogen_type) ? ts_str(t_autogen_type) : "");

   		if (t_autogen_type && ts_length(t_autogen_type) > 0){
			strncpy(obj->cfg.extn_action_list[obj->num_extn_actions].action.auto_gen.dest_type, ts_str(t_autogen_type), TYPE_STR_LEN);
  		}
   		ts_free(&t_autogen_type);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_autogen_src_binding)) {
   		tstring *t_autogen_src = NULL;

   		err = bn_binding_get_tstr(binding,
   				ba_value, bt_string, NULL,
   				&t_autogen_src);
   		bail_error(err);

   		lc_log_debug(LOG_DEBUG, "Read .../crawler/profile/%s/url as : \"%s\"",
   				t_crawler_profile, (t_autogen_src) ? ts_str(t_autogen_src) : "");

   		if (t_autogen_src && ts_length(t_autogen_src) > 0){
			strncpy(obj->cfg.extn_action_list[obj->num_extn_actions].action.auto_gen.source_type, ts_str(t_autogen_src), SRC_STR_LEN);
			obj->num_extn_actions++;
    		}
   		ts_free(&t_autogen_src);
    }else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_autogen_expiry_binding)) {
   		uint64 t_autogen_expiry = 0;

		err = bn_binding_get_uint64(binding,
				ba_value, NULL,
				&t_autogen_expiry);
		bail_error(err);

 		obj->cfg.extn_action_list[0].action.auto_gen.expiry = t_autogen_expiry;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_origin_response_expiry_binding)) {
   		tbool origin_response = false;

   		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&origin_response);
		bail_error(err);

		obj->cfg.expiry_mechanism_type = origin_response ? EXP_TYPE_ORIGIN_RESPONSE:EXP_TYPE_REFRESH_INTERVAL;
    } else if (bn_binding_name_pattern_match(ts_str(name), nkn_cse_xdomain_binding)) {
   		tbool t_xdomain = true;

   		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				& t_xdomain);
		obj->cfg.xdomain_fetch = t_xdomain;
    }

bail:
    return err;
}


/* ------------------------------------------------------------------------- */
/*
 *	funtion : nkn_cse_delete_cfg_handle_change()
 *	purpose : handler for config changes for cse module
 */
int
nkn_cse_delete_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    const char *t_crawler_profile = NULL;
    crawler_context_t *obj = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(data);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

   	/* Check if this is our node */
   	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/crawler/config/profile/**"))
   	{
   		/*-------- Get the crawler profile ------------*/
   		bn_binding_get_name_parts (binding, &name_parts);
   		bail_error_null(err, name_parts);

   		t_crawler_profile = tstr_array_get_str_quick (name_parts, 4);
   		bail_error(err);

		lc_log_debug(LOG_DEBUG, "Read .../nkn/crawler/config/profile as : \"%s\"",
    			t_crawler_profile);

        	/* Get the crawler profile entry from the TAILQ */
		TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(t_crawler_profile,obj->cfg.name)){
    			break;
    		}
    	}
    }
    else
    {
    	/* This is not the cse node hence leave */
    	goto bail;
    }

   	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/crawler/config/profile/*"))
   	{
		if (NULL != obj)
		{
		        if (obj->crawling_activity_status == CRAWL_IN_PROGRESS)
			    obj->inprogress_before_delete = 1;
			obj->crawling_activity_status  = CRAWL_CTXT_DELETED;
		}
   	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_file_extn_list_binding)) {
   		const char *t_file_ext = NULL;
   		int file_ext_cnt = 0;
   		int i=0, j=0, pos = 0;

		bn_binding_get_name_parts (binding, &name_parts);
		bail_error(err);
		t_file_ext = tstr_array_get_str_quick (name_parts, 6);

		if (NULL != obj)
		{
	  		if (t_file_ext && strlen(t_file_ext) > 0){
	  			file_ext_cnt = obj->num_accept_extns;
	  			for(i=0;i<file_ext_cnt;i++){
	  				if(!strcmp(t_file_ext, obj->cfg.accept_extn_list[i].extn_name)){
	  					pos = i;
	  					break;
	  				}
	  			}
				for(j=pos;j<file_ext_cnt;j++){
					if(j == (file_ext_cnt -1))
					{
						memset(obj->cfg.accept_extn_list[j].extn_name, 0, MAX_ACCEPT_EXTN_STR_SIZE);
						obj->cfg.accept_extn_list[j].extn_len = 0;
						obj->cfg.accept_extn_list[j].skip_preload = 0;
					}
					else
					{
						strncpy(obj->cfg.accept_extn_list[j].extn_name, obj->cfg.accept_extn_list[j+1].extn_name, MAX_ACCEPT_EXTN_STR_SIZE);
						obj->cfg.accept_extn_list[j].extn_len = obj->cfg.accept_extn_list[j+1].extn_len;
						obj->cfg.accept_extn_list[j].skip_preload = obj->cfg.accept_extn_list[j+1].skip_preload;
					}
				}
				obj->num_accept_extns--;
	  		}
		}
   	}
	else if (bn_binding_name_pattern_match (ts_str(name), nkn_cse_autogen_src_binding)) {
   		const char *t_autogen_src = NULL;
   		int ext_actions_cnt = 0;
   		int i=0, j=0, pos = 0;

		bn_binding_get_name_parts (binding, &name_parts);
		bail_error(err);

		t_autogen_src = tstr_array_get_str_quick (name_parts, 6);

		if (NULL != obj)
		{
	  		if (t_autogen_src && strlen(t_autogen_src) > 0){
	  			ext_actions_cnt = obj->num_extn_actions;
	  			for(i=0;i<ext_actions_cnt;i++){
	  				if(!strcmp(t_autogen_src, obj->cfg.extn_action_list[i].action.auto_gen.source_type)){
	  					pos = i;
	  					break;
	  				}
	  			}
				for(j=pos;j<ext_actions_cnt;j++){
					if(j != (ext_actions_cnt -1))
					{
						strncpy(obj->cfg.extn_action_list[j].action.auto_gen.source_type, obj->cfg.extn_action_list[j+1].action.auto_gen.source_type, SRC_STR_LEN);
						strncpy(obj->cfg.extn_action_list[j].action.auto_gen.dest_type, obj->cfg.extn_action_list[j+1].action.auto_gen.dest_type, TYPE_STR_LEN);
						obj->cfg.extn_action_list[j].action_type = obj->cfg.extn_action_list[j+1].action_type;
					}
					else
					{
						memset(obj->cfg.extn_action_list[j].action.auto_gen.source_type, 0, SRC_STR_LEN);
						memset(obj->cfg.extn_action_list[j].action.auto_gen.dest_type, 0, TYPE_STR_LEN);
						obj->cfg.extn_action_list[j].action_type = 0;
					}
				}
				obj->num_extn_actions--;
	  		}
		}
   	}

bail:
	tstr_array_free(&name_parts);
    return err;
}



int
nkn_cse_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                  void *data, bn_binding_array *resp_bindings)
{
    int err = 0;
    int num_parts = 0;
    const char *crawler_name = NULL;
    crawler_context_t *obj;
    bn_binding *binding = NULL;

	UNUSED_ARGUMENT(data);

    /* Check if the node is of interest to us */
    if ((!lc_is_prefix("/nkn/crawler/monitor/profile/external", bname, false)) )
    {
        /* Not for us */
        goto bail;
    }

    num_parts = tstr_array_length_quick(bname_parts);
	pthread_mutex_lock(&g_cse_timerq_mutex);


    if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/start_ts")) {

        crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
        bail_null(crawler_name);
    

    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(crawler_name,obj->cfg.name)){
    			break;
    		}
    	}

    	if (NULL != obj)
    	{
	   err = bn_binding_new_parts_datetime_sec
    	        (&binding, bname, bname_parts, true,
    	        ba_value, 0, obj->crawl_start_time);
    	   bail_error(err);
    	}
    }
    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/end_ts")) {

        crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
        bail_null(crawler_name);
    
    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

		if(!strcmp(crawler_name,obj->cfg.name)){
			break;
    		}
       	}
	if (NULL != obj)
    	{
	   err = bn_binding_new_parts_datetime_sec
		(&binding, bname, bname_parts, true,
    	        ba_value, 0, obj->crawl_end_time);
    	   bail_error(err);
    	}
    }
    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/status")) {

	crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(crawler_name);
    	
    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(crawler_name,obj->cfg.name)){
    			break;
    		}
    	}
    	if (NULL != obj)
    	{
    	   err = bn_binding_new_parts_uint8
    	      (&binding, bname, bname_parts, true,
    	      ba_value, 0, obj->crawling_activity_status);
    	   bail_error(err);
    	}
    }
    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
                                "/nkn/crawler/monitor/profile/external/*/next_trigger_time")) {

	crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(crawler_name);

	/* Get the crawler profile entry from the TAILQ */
	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {
	    if(!strcmp(crawler_name,obj->cfg.name)){
		break;
	    }
	}
	if (NULL != obj)
	{
	    err = bn_binding_new_parts_uint64
		    (&binding, bname, bname_parts, true, ba_value, 0, obj->next_trigger_time);
	    bail_error(err);
	}
    }

    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/num_objects_add")) {

	crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(crawler_name);

    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(crawler_name,obj->cfg.name)){
    			break;
    		}
    	}
    	if (NULL != obj)
    	{
    	   err = bn_binding_new_parts_uint64
    	      (&binding, bname, bname_parts, true,
    	      ba_value, 0, obj->stats.num_adds);
    	   bail_error(err);
    	}
    }
    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/num_objects_del")) {

	crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(crawler_name);

    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(crawler_name,obj->cfg.name)){
    			break;
    		}
    	}
    	if (NULL != obj)
    	{
    	   err = bn_binding_new_parts_uint64
    	      (&binding, bname, bname_parts, true,
    	      ba_value, 0, obj->stats.num_deletes);
    	   bail_error(err);
    	}
    }
    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/num_objects_add_fail")) {

	crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(crawler_name);

    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(crawler_name,obj->cfg.name)){
    			break;
    		}
    	}
    	if (NULL != obj)
    	{
    	   err = bn_binding_new_parts_uint64
    	      (&binding, bname, bname_parts, true,
    	      ba_value, 0, obj->stats.num_add_fails);
    	   bail_error(err);
    	}
    }
    else if (bn_binding_name_parts_pattern_match(bname_parts, true,
    			    "/nkn/crawler/monitor/profile/external/*/num_objects_del_fail")) {

	crawler_name = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(crawler_name);

    	/* Get the crawler profile entry from the TAILQ */
    	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

    		if(!strcmp(crawler_name,obj->cfg.name)){
    			break;
    		}
    	}
    	if (NULL != obj)
    	{
    	   err = bn_binding_new_parts_uint64
    	      (&binding, bname, bname_parts, true,
    	      ba_value, 0, obj->stats.num_delete_fails);
    	   bail_error(err);
    	}
    }
    if (binding) {
        err = bn_binding_array_append_takeover(resp_bindings, &binding);
        bail_error(err);
    }

bail:
	pthread_mutex_unlock(&g_cse_timerq_mutex);
    bn_binding_free(&binding);
    return err;
}


/* ------------------------------------------------------------------------- */
int
nkn_cse_mon_handle_iterate(const char *bname, const tstr_array *bname_parts,
			void *data, tstr_array *resp_names)
{
    int err = 0;
    uint32 i = 0;
    char *bn_name = NULL;

    UNUSED_ARGUMENT(bname);
    UNUSED_ARGUMENT(bname_parts);
    UNUSED_ARGUMENT(data);

    for (i = 0; i < cse_num_ext_mon_nodes; ++i) {
        bn_name = smprintf("%d", i);
        err = tstr_array_append_str_takeover(resp_names, &bn_name);
        bail_error(err);
    }

 bail:
    safe_free(bn_name);
    return(err);
}

void
nkn_cse_get_client_domain_from_url(crawler_context_t *obj)
{
    char *ptr1, *ptr;
    int len = 0;
    ptr1 = obj->cfg.url;
    if (strncasecmp(obj->cfg.url, "http://", 7)) {
        if (!strncasecmp(obj->cfg.url, "https://", 8)) {
            ptr1 = obj->cfg.url + 8;
        }
    } else {
        ptr1 = obj->cfg.url + 7;
    }
    if ((ptr = strchr(ptr1, '/'))) {
        if (ptr - ptr1 > CRAWLER_CLIENT_DOMAIN_SIZE - 1){
    	    syslog(LOG_ERR,
    		"Domain name in baseurl size is greater than %d characters",
    		CRAWLER_CLIENT_DOMAIN_SIZE);
        } else {
    	    strncpy(obj->cfg.client_domain, ptr1, ptr - ptr1);
	    obj->cfg.client_domain[ptr - ptr1] = '\0';
        }
    } else {
        len = strlen(ptr1);
        if (len > CRAWLER_CLIENT_DOMAIN_SIZE - 1) {
    	    syslog(LOG_ERR,
    		"Domain name in baseurl size is greater than %d characters",
    		CRAWLER_CLIENT_DOMAIN_SIZE);
        } else {
    	    strncpy(obj->cfg.client_domain, ptr1, len);
	    obj->cfg.client_domain[len] = '\0';
        }
    }
}

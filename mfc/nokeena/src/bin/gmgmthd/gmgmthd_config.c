/*
 *
 * Filename:  gmgmthd_config.c
 * Date:      2010/03/25 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "gmgmthd_main.h"
#include "license.h"
#include "proc_utils.h"
#include "bnode.h"
#include "file_utils.h"
#include <string.h>
#include <stdlib.h>
#include "libnknmgmt.h"
#include "nkn_mgmt_defs.h"
#include "nkn_cntr_api.h"
extern unsigned int jnpr_log_level;

#define PROBE_LISTEN_INTERFACE "lo"

static const char *gmgmthd_config_prefix = "/ank/mfc/bw_control/config";
static const char *gmgmthd_mfd_node_binding = "/ank/mfc/bw_control/config/mfd_node";
static const char *gmgmthd_locale_binding = "/system/locale/global";
static const char *gmgmthd_network_param = "/nkn/nvsd/network/config";
static const char *gmgmthd_prefetch_binding = "/nkn/prefetch/config";

static int prefetch_set_poll_timer( prefetch_timer_data_t *context);

static tbool prefetch_is_preread_done(void);

int prefetch_poll_timeout_handler(int fd,
                                short event_type,
                                void *data,
                                lew_context *ctxt);

static int prefetch_context_free(prefetch_timer_data_t *context);

int prefetch_delete_poll_timer(prefetch_timer_data_t *context);

static int
prefetch_create_timer(const char *name, uint32 new_poll_freq);


static int prefetch_context_allocate(
                const char *name,
                lt_dur_ms  poll_frequency,
                prefetch_timer_data_t **ret_context);

static int prefetch_context_find(const char *name, prefetch_timer_data_t **ret_context);

static int prefetch_schedule_del_jobs(const char *name);

/*This var indicates that a cfg_query was initiated*/
int gmgmthd_is_cfg_qry = 0;

/*This flag is used to indicate a disk cache has been added/ deleted*/
extern int g_rebuild_local_disk_stat ;

/* ------------------------------------------------------------------------- */
int
gmgmthd_config_query(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    gmgmthd_is_cfg_qry = 1;
    err = mgmt_log_init(gmgmthd_mcc, "/nkn/debug/log/ps/gmgmthd/level");
    bail_error(err);
    err = mdc_get_binding_children(gmgmthd_mcc,
                                   NULL, NULL, true, &bindings, false, true,
                                   gmgmthd_mfd_node_binding);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, gmgmthd_config_handle_change,
                                   &rechecked_licenses);
    bail_error(err);



    /* Get the syn-cookie values */
    err = mdc_get_binding_children(gmgmthd_mcc,
                    NULL, NULL, true, &bindings, true, true,
                    gmgmthd_network_param);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, gmgmthd_config_handle_change,
                    &rechecked_licenses);
    bail_error(err);
    /*Get the prefetch values*/
    err = mdc_get_binding_children(gmgmthd_mcc,
		   NULL, NULL, true, &bindings, true, true,
		   gmgmthd_prefetch_binding);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, gmgmthd_config_handle_change,
		   &rechecked_licenses);
    bail_error(err);

bail:
    gmgmthd_is_cfg_qry = 0;
    bn_binding_array_free(&bindings);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
gmgmthd_config_handle_change(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    const char *t_mfd_index_str = NULL;
    uint32_t mfd_index;
    uint32 retcode = 0;
    tstring *retmsg = NULL;
    tstring *t_status = NULL;
    tbool *rechecked_licenses_p = data;
//    tstring *t_httpd_port = NULL;

    bail_null(rechecked_licenses_p);

       err = bn_binding_get_name(binding, &name);
    bail_error(err);


    int32 status = 0;
    tstring *ret_output = NULL;


 
    if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/syn-cookie")) {

	    err = bn_binding_get_tstr(binding, ba_value, bt_bool, 0, &t_status);
 	    bail_error_null(err , t_status);

	    if(!strcmp("true", ts_str(t_status))) {
                    err = lc_launch_quick_status(&status, &ret_output, true, 2, "/opt/nkn/bin/set_syncookie.sh","1");
                    bail_error(err);
                    lc_log_basic(jnpr_log_level,"setting syn-cookie as true");
            }
            else {
                    err = lc_launch_quick_status(&status, &ret_output, true, 2, "/opt/nkn/bin/set_syncookie.sh","0");
                    bail_error(err);
                    lc_log_basic(jnpr_log_level, "settting syn-cookie as false");
            } 
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/syn-cookie/half_open_conn")) {

	    tstring *num_conn = NULL;
	    char *c_status = NULL;
	    tbool found = false;	
	    tbool b_syncookie = false;
	    uint32 code = 0;

	    err = bn_binding_get_tstr(binding, ba_value, bt_uint32, 0, &num_conn);
	    bail_error(err);
	    lc_log_basic(jnpr_log_level, "syn-cookie half-open conn change = %s", ts_str(num_conn));

	    err = mdc_get_binding_tbool(gmgmthd_mcc, &code, NULL, &found, &b_syncookie, "/nkn/nvsd/network/config/syn-cookie",NULL);
	    bail_error(err);

	    if(found && b_syncookie) 
		    c_status = smprintf("%u",1);
	    else
		    c_status = smprintf("%u",0);

	    err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/set_syncookie.sh", c_status, ts_str(num_conn));
	    ts_free(&num_conn);
            safe_free(c_status);
	    bail_error(err);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_max_orphans")) {

            tstring *num_conn = NULL;          
	    char *param =smprintf("max-orphan");
    
            err = bn_binding_get_tstr(binding, ba_value, bt_uint32, 0, &num_conn);
            bail_error(err);
            lc_log_basic(LOG_DEBUG, "tcp-max-orphans conn change = %s", ts_str(num_conn));

            err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/tcp_params.sh", param, ts_str(num_conn));
	    ts_free(&num_conn);
	    safe_free(param);
            bail_error(err);

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_max_tw_buckets")) {

            tstring *num_conn = NULL;
            char *param =smprintf("max-buckets");

            err = bn_binding_get_tstr(binding, ba_value, bt_uint32, 0, &num_conn);
            bail_error(err);
            lc_log_basic(LOG_DEBUG, "tcp-max-buckets conn change = %s", ts_str(num_conn));

            err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/tcp_params.sh", param, ts_str(num_conn));
	    ts_free(&num_conn);
            safe_free(param);
            bail_error(err);

    }    
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_fin_timeout")) {

            tstring *num_conn = NULL;
            char *param =smprintf("fin-timeout");

            err = bn_binding_get_tstr(binding, ba_value, bt_uint32, 0, &num_conn);
            bail_error(err);
            lc_log_basic(LOG_DEBUG, "tcp-fin-timeout change = %s", ts_str(num_conn));

            err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/tcp_params.sh", param, ts_str(num_conn));
	    ts_free(&num_conn);
            safe_free(param);
            bail_error(err);

    }
    else if ((bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_mem/maxpage")) ||
	    (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_mem/low")) ||
	    (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_mem/high"))) {

            tstring *maxpage = NULL;
            char *param = smprintf("memory");
            tstring *low = NULL;
            tstring *high = NULL;
            uint32 code = 0;

	    err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &maxpage, "/nkn/nvsd/network/config/tcp_mem/maxpage",NULL);
            bail_error_null(err,maxpage);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &low, "/nkn/nvsd/network/config/tcp_mem/low",NULL);
            bail_error_null(err,low);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &high, "/nkn/nvsd/network/config/tcp_mem/high",NULL);
            bail_error_null(err,high);

            err = lc_launch_quick_status(&status, &ret_output, true, 5, "/opt/nkn/bin/tcp_params.sh", param, ts_str(low), ts_str(high), ts_str(maxpage));
            safe_free(param);
            ts_free(&low);
            ts_free(&high);
            ts_free(&maxpage);
            bail_error(err);
    }
    else if ((bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_rmem/max")) ||
	    (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_rmem/default")) ||
	    (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_rmem/min"))) {

            char *param = smprintf("read-memory");
            uint32 code = 0;
	    tstring *rmemmax = NULL;
	    tstring *rmemmin = NULL;
	    tstring *rmemdef = NULL;

	    err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &rmemmax, "/nkn/nvsd/network/config/tcp_rmem/max",NULL);
            bail_error_null(err,rmemmax);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &rmemmin, "/nkn/nvsd/network/config/tcp_rmem/min",NULL);
            bail_error_null(err,rmemmin);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &rmemdef, "/nkn/nvsd/network/config/tcp_rmem/default",NULL);
            bail_error_null(err,rmemdef);

	    lc_log_basic(jnpr_log_level, "tcp-rmem change = %s  %s  %s", ts_str(rmemmax), ts_str(rmemmin), ts_str(rmemdef));

            err = lc_launch_quick_status(&status, &ret_output, true, 5, "/opt/nkn/bin/tcp_params.sh", param, ts_str(rmemmin), ts_str(rmemdef), ts_str(rmemmax));
            safe_free(param);
	    ts_free(&rmemmin);
            ts_free(&rmemmax);
            ts_free(&rmemdef);
	    bail_error(err);
    }
    else if ((bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_wmem/max")) ||
	    (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_wmem/default")) ||
	    (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/tcp_wmem/min"))) {

            char *param = smprintf("write-memory");
            uint32 code = 0;
	    tstring *wmemmax = NULL;
	    tstring *wmemmin = NULL;
	    tstring *wmemdef = NULL;

	    err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &wmemmax, "/nkn/nvsd/network/config/tcp_wmem/max",NULL);
            bail_null(wmemmax);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &wmemmin, "/nkn/nvsd/network/config/tcp_wmem/min",NULL);
            bail_null(wmemmin);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &code, NULL, NULL, &wmemdef, "/nkn/nvsd/network/config/tcp_wmem/default",NULL);
            bail_null(wmemdef);

            err = lc_launch_quick_status(&status, &ret_output, true, 5, "/opt/nkn/bin/tcp_params.sh", param, ts_str(wmemmin), ts_str(wmemdef), ts_str(wmemmax));
            bail_error(err);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/prefetch/config/*/status")) {

    	    tstring *job_date = NULL;
            const char *job_name = NULL;
            tstring *job_time = NULL;
	    tstring *job_status = NULL;
            uint32 code = 0;
	    char *time_node = NULL;
	    char *date_node = NULL;
	    uint32 err_code = 0;

	    bn_binding_get_name_parts (binding, &name_parts);
	    bail_error(err);

	    job_name = tstr_array_get_str_quick(name_parts, 3);
	    bail_error_null(err, job_name);

            err = mdc_get_binding_tstr(gmgmthd_mcc, &err_code, NULL, NULL, &job_status, ts_str(name), NULL);
 
            if (job_status == NULL)
	   	goto bail; 

            /*get the status here for the particular job*/    
	    if(strcmp("immediate", ts_str(job_status)) == 0) {
		int timer_val = 0;
		/*start the prefetch immediately*/
		/* If called from config query,
		 * i.e from init,
		 * add 90 secs extra to start,eventhough its immediate,
		 * as nvsd might not be up
		 */
		if(gmgmthd_is_cfg_qry) {
		    timer_val = 90;
		} 
		err = prefetch_create_timer(job_name, timer_val);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Immediate prefetch job ");
	    } else if ((strcmp ("scheduled", ts_str(job_status)) == 0)) {
			
		lt_time_sec ret_inpdaytimesec;
    		lt_time_sec curr_time;
    		lt_time_sec inp_timesec;
    		lt_time_sec rem_time;
    		lt_date today_date;
    		lt_time_sec sys_daytimesec;

		/*Get the time and date of the job*/
		time_node = smprintf("/nkn/prefetch/config/%s/time", job_name);
        	err = mdc_get_binding_tstr(gmgmthd_mcc, &err_code, NULL, NULL, &job_time, time_node, NULL);
		bail_error(err);
			
		date_node = smprintf("/nkn/prefetch/config/%s/date", job_name);
        	err = mdc_get_binding_tstr(gmgmthd_mcc, &err_code, NULL, NULL, &job_date, date_node, NULL);
		bail_error(err);

		/*calculate the secs and schedule the prefetch*/
		/*Get the current system time in secs from EPOCH*/
            	curr_time  = lt_curr_time();

            	/*Get today's date*/
            	err = lt_time_to_date_daytime_sec (curr_time, &today_date, &sys_daytimesec);
	    	bail_error(err);

            	/*Convert the input day time to secs from EPOCH*/
            	err = lt_str_to_daytime_sec(ts_str(job_time), &ret_inpdaytimesec);
	    	bail_error(err);
	    	err = lt_str_to_date(ts_str(job_date), &today_date);
	    	bail_error(err);
            	err = lt_date_daytime_to_time_sec(today_date, ret_inpdaytimesec, &inp_timesec);
            	bail_error(err);

            	/*Calculate the remaining sec for the scheduled prefetch*/
            	/*If the scheduled time is less than the current time, then prefetch is done immediately*/
		rem_time = inp_timesec - curr_time;
            	if (rem_time < 0){
                	lc_log_basic(LOG_NOTICE, "The scheduled time is less than the current time. Hence prefetch will be done immediately");
                	rem_time = 0;
            	}

		lc_log_basic(LOG_DEBUG, "Scheduled prefetch job %s time %d", job_name, rem_time);

            	err = prefetch_create_timer(job_name, rem_time);
		ts_free(&job_date);
		ts_free(&job_time);
		ts_free(&job_status);
		safe_free(time_node);
		safe_free(date_node);

            	bail_error(err);
    	} else if (strcmp ("delete", ts_str(job_status)) == 0) {
		
		/*delete the scheduled JOB*/
		err = prefetch_schedule_del_jobs(job_name);
	    	bail_error(err);
	}

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/path-mtu-discovery")) {

	    err = bn_binding_get_tstr(binding, ba_value, bt_bool, 0,&t_status);
            bail_error(err);

	    /*Set 0 to enable the path-mtu discovery in proc files. Set 1 to disable the same*/
            if(!strcmp("true", ts_str(t_status))) {
		    err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/tcp_params.sh", "path-mtu", "0");
	            bail_error(err);
                    lc_log_basic(LOG_DEBUG,"Turning path-mtu discovery on");
            }
            else {
		    err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/tcp_params.sh", "path-mtu", "1");
	            bail_error(err);
                    lc_log_basic(LOG_DEBUG, "Turning path-mtu discovery off");
            }
	    if (ret_output)
		lc_log_basic(LOG_DEBUG, "Return output while writing to the proc file - %s", ts_str(ret_output));

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/ip-forward")) {

	    err = bn_binding_get_tstr(binding, ba_value, bt_bool, 0,&t_status);
	    bail_error(err);

	    if(!strcmp("true", ts_str(t_status))) 
	    {
	    	err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/proc_config.sh", "/proc/sys/net/ipv4/ip_forward","1");
	    }
	    else{
	    	err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/proc_config.sh", "/proc/sys/net/ipv4/ip_forward","0");
	    }
        bail_error(err);
     }
     else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/diskcache/config/**")) {
	 /*Rebuild the local structure*/
	 if(g_rebuild_local_disk_stat == 0) {
	     g_rebuild_local_disk_stat = 1;
	 }

     }

    /* BUG 7701: Moving NFS mount/umount to mgmthd.*/

bail:
    tstr_array_free(&name_parts);
    ts_free(&retmsg);
    ts_free(&t_status);
    ts_free(&ret_output);
    return(err);
}

/* End of gmgmthd_config.c */
int
gmgmthd_config_handle_delete(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    const char *t_mfd_index_str = NULL;
    uint32_t mfd_index;
    tstring *t_status = NULL;
    tbool *rechecked_licenses_p = data;
//    tstring *t_httpd_port = NULL;

    bail_null(rechecked_licenses_p);

       err = bn_binding_get_name(binding, &name);
    bail_error(err);
bail:
    tstr_array_free(&name_parts);
    return(err);
}

int prefetch_set_poll_timer(prefetch_timer_data_t *context)
{
    int err = 0;

    err = lew_event_reg_timer(gmgmthd_lwc,
                        &(context->poll_event),
                        (context->callback),
                        (void *) (context),
                        (context->poll_frequency));
    bail_error(err);

bail:
        return err;
}

static tbool prefetch_is_preread_done(void)
{
    tbool ret_val = true;
    uint64_t sas_preread_done = 0;
    uint64_t sata_preread_done = 0;
    uint64_t ssd_preread_done = 0;


    uint64_t sas_put_thr_cnt = 0;
    uint64_t sata_put_thr_cnt = 0;
    uint64_t ssd_put_thr_cnt = 0;


    sas_preread_done = nkncnt_get_uint64("dictionary.tier.SAS.preread.done");
    sata_preread_done = nkncnt_get_uint64("dictionary.tier.SATA.preread.done");
    ssd_preread_done = nkncnt_get_uint64("dictionary.tier.SSD.preread.done");

    sas_put_thr_cnt = nkncnt_get_uint64("SAS.dm2_put_thr_cnt");
    sata_put_thr_cnt = nkncnt_get_uint64("SATA.dm2_put_thr_cnt");
    ssd_put_thr_cnt = nkncnt_get_uint64("SSD.dm2_put_thr_cnt");

    /*If there it put thrd
     *then that tier is existing
     *see if preread is done
     */
    if(sas_put_thr_cnt && !sas_preread_done){
	ret_val = false;
    }

    if(sata_put_thr_cnt && !sata_preread_done){
	ret_val = false;
    }

    if(ssd_put_thr_cnt && !ssd_preread_done){
	ret_val = false;
    }

    return ret_val;
}

int prefetch_poll_timeout_handler(int fd,
                                short event_type,
                                void *data,
                                lew_context *ctxt)
{
    int err = 0;
    int32 status = 0;
    tstring *ret_output = NULL;
    lc_launch_params *lp=NULL;
    lc_launch_result lr;

    prefetch_timer_data_t *context = (prefetch_timer_data_t *) (data);

    bail_null(context);

    /* Check if DM2 pre-read is done,
     * else schedule again,
     * with additive back off.
     */
     if(context->flags & PREFETCH_FLAG_NVSD_PREREAD) {
	context->retry_count++;
	context->retry_timeout = context->retry_timeout * 2;
     }

     if(context->retry_count >= PREFETCH_MAX_RETRY_CNT) {
	lc_log_basic(LOG_NOTICE, "Prefetch:Cancelling Prefetch Job %s, "
				"After Max retries", context->name);

	prefetch_delete_poll_timer(context);

	err = prefetch_context_free(context);
	bail_error(err);
	return 0;
     }
    if(!prefetch_is_preread_done()) {
	context->flags |= PREFETCH_FLAG_NVSD_PREREAD;

	lc_log_basic(LOG_NOTICE, "Prefetch:Rescheduling Prefetch Job %s ,Retry Count = %d"
				    , context->name, context->retry_count);

	context->poll_frequency += (context->retry_timeout * 1000);
	err = prefetch_set_poll_timer(context);
	bail_error(err);

	return 0;
    }


    lc_log_basic(LOG_NOTICE, _("Timer expired for prefetch file %s."), context->name);

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, &lp);

    err = ts_new_str(&(lp->lp_path), "/opt/nkn/bin/prefetch.py");
    bail_error(err);

    err = tstr_array_new(&(lp->lp_argv), 0);
    bail_error(err);

    err = tstr_array_insert_str(lp->lp_argv, 0, "/opt/nkn/bin/prefetch.py");
    bail_error(err);
    err = tstr_array_insert_str(lp->lp_argv, 1, "download");
    bail_error(err);
    err = tstr_array_insert_str(lp->lp_argv, 2, context->name);
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, &lr);
    bail_error(err);

bail:
    return err;
    lc_launch_params_free(&lp);
}

static int prefetch_context_allocate(
                const char *uname,
                lt_dur_ms  poll_frequency,
                prefetch_timer_data_t **ret_context)
{

    int err = 0;
    int i = 0;
    prefetch_timer_data_t *context = NULL;
    tstr_array *name_parts = NULL;
    const char *name = NULL;
    uint32 length = 0;

    for(i = 0; i < MAX_PREFETCH_JOBS; ++i) {
        if (prefetch_timers[i].name == NULL) {
                context = &prefetch_timers[i];
                break;
        }
    }
    bail_null(context);

    /*err =   ts_tokenize_str(uname, '/', '\\', '"', ttf_ignore_leading_separator, &name_parts);
    length = tstr_array_length_quick(name_parts);
    name = tstr_array_get_str_quick(name_parts, (length-1));
    Sanity check for the file name
    if (!strcmp(name,""))
    {
        return err;
    }*/

    context->name = smprintf("%s", uname);
    bail_null(context->name);

    context->callback = prefetch_poll_timeout_handler;
    context->poll_frequency = poll_frequency;
    context->retry_timeout = 30;//30 secs
    /* THis call ensures that the file is downloaded immediately
     if there is no timer */
    if (poll_frequency == 0) {
        /* Single shot - Not really a timer */
        err = prefetch_poll_timeout_handler(0, 0, (void *) (context), NULL);
        bail_error(err);
    }
    else
    {
        err = prefetch_set_poll_timer(context);
        bail_error(err);
    }

    if (ret_context)
        *ret_context = context;
bail:
        return err;
}

static int
prefetch_create_timer(const char *name, uint32 new_poll_freq)
{
    int err = 0;
    prefetch_timer_data_t *context = NULL;
    uint32 old_poll_freq = 0;

    /* Get the timer context */
    err = prefetch_context_find(name, &context);
    bail_error(err);

    if (NULL == context) {
        /*there is no previous timer allcated for this name.*/
        /*timer is created for the first time*/
        err = prefetch_context_allocate(name,
                        (new_poll_freq * 1000), &context);
	lc_log_basic(LOG_DEBUG, "Job not available. creating new one");
    }
    else {
        /*Delete the old timer and reset the  job schedule*/
        err = prefetch_delete_poll_timer(context);
        bail_error(err);

        err = prefetch_context_free(context);
        bail_error(err);

        err = prefetch_context_allocate(name,
                        (new_poll_freq * 1000), NULL);
        bail_error(err);
	lc_log_basic(LOG_NOTICE, "Deleteing and  creating new Job");
    }
bail:
    return err;
}


int prefetch_delete_poll_timer(prefetch_timer_data_t *context)
{
        int err = 0;

        err = lew_event_delete(gmgmthd_lwc, &(context->poll_event));
        complain_error(err);

bail:
        return err;
}

static int prefetch_context_free(prefetch_timer_data_t *context)
{
        int err = 0;

        if(NULL != context){

                safe_free(context->name);
                context->callback = NULL;
                context->poll_frequency = 0;
		context->retry_timeout = 0;
		context->flags = 0;
		context->retry_count = 0;
        }
bail:
        return err;
}

static int prefetch_context_find(const char *name, prefetch_timer_data_t **ret_context)
{
        int err = 0;
        int i = 0;

        bail_null(ret_context);

        *ret_context = NULL;
        for (i = 0; i < MAX_PREFETCH_JOBS; ++i) {
                if ( safe_strcmp(name, prefetch_timers[i].name) == 0) {
                        *ret_context = &prefetch_timers[i];
                        goto bail;
                }
        }

bail:
        return err;
}

static int prefetch_schedule_del_jobs(const char *name)
{
    int err = 0;
    prefetch_timer_data_t *context = NULL;
    char *name_node = NULL;
    /* Get the timer context */
    err = prefetch_context_find(name, &context);
    bail_error(err);

    if (NULL != context) {
        err = prefetch_delete_poll_timer(context);
        bail_error(err);

        err = prefetch_context_free(context);
        bail_error(err);
        lc_log_basic(LOG_NOTICE, "The scheduled job is successfully deleted - %s", name);
    }

    name_node = smprintf( "/nkn/prefetch/config/%s", name);
    bail_null(name_node);

    /*delete the node*/
    err = mdc_delete_binding(gmgmthd_mcc, NULL, NULL, name_node);
    bail_error(err);

bail:
    safe_free(name_node);
    return err;
}


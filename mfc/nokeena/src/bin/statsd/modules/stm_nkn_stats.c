


#include "common.h"
#include "dso.h"
#include "st_registration.h"
#include "nkn_mgmt_defs.h"

typedef struct _pernodeCtxt
{
    bn_binding_array *ret_instance;
    uint32 elapsedtime;
}pernodeCtxt;

typedef struct stm_cache_hit_ctxt
{
    uint64 client_bytes;
    uint64 total_bytes;
}stm_cache_hit_ctxt;

typedef struct stm_disk_ctxt
{
    bn_binding_array *bindings;
    tstr_array *disknames;
}stm_disk_ctxt;

int stm_nkn_stats_init(const lc_dso_info *info, void *data);
static int
stm_total_sum_elements(st_dataset *dataset, st_series *series, void *data)
{
    int err = 0;
    bn_attrib *elem = NULL;
    uint64 *total_p = data;
    int64 num_diff = 0;

    bail_null(total_p);

    err = st_series_get_last_elem(series, NULL, NULL, &elem, NULL);
    bail_error(err);

    /*
     * The numbers we get are int64s even though the original nodes
     * sampled were uint64s, because the sample method is "delta",
     * and we always convert to signed int types for deltas.
     */
    if (elem) {
        err = bn_attrib_get_int64(elem, NULL, NULL, &num_diff);
        bail_error(err);
        *total_p += num_diff;
    }

 bail:
    bn_attrib_free(&elem);
    return(err);
}
static int
stm_avg32_total_sum_elements(st_dataset *dataset, st_series *series, void *data)
{
    int err = 0;
    bn_attrib *elem = NULL;
    uint32 *total_p = data;
    uint32 num = 0;

    bail_null(total_p);

    err = st_series_get_last_elem(series, NULL, NULL, &elem, NULL);
    bail_error(err);

    if (elem) {
        err = bn_attrib_get_uint32(elem, NULL, NULL, &num);
        bail_error(err);
        *total_p += num;
    }

 bail:
    bn_attrib_free(&elem);
    return(err);
}
static int
stm_avg_total_sum_elements(st_dataset *dataset, st_series *series, void *data)
{
    int err = 0;
    bn_attrib *elem = NULL;
    uint64 *total_p = data;
    uint64 num = 0;

    bail_null(total_p);

    err = st_series_get_last_elem(series, NULL, NULL, &elem, NULL);
    bail_error(err);

    if (elem) {
        err = bn_attrib_get_uint64(elem, NULL, NULL, &num);
        bail_error(err);
        *total_p += num;
    }

 bail:
    bn_attrib_free(&elem);
    return(err);
}

static int
stm_nkn_get_cache_hit_data(st_dataset *dataset, st_series *series, void *data)
{
    int err = 0;
    bn_attrib *elem = NULL;
    stm_cache_hit_ctxt *ctxt = data;
    int64 num_diff = 0;
    int32  min_id = 0;
    int32  max_id = 0;
    tbool num_instance = false;

    int32 i = 0;

    /* Get the total instance id present */
    /*The cache hit ratio data is for the last 24 hours 
     * So the sample values for last 24 hrs (delta increasing) or 
     * whatever is available till this instant is added up*/
    

    err = st_dataset_get_instance_range(dataset, -1 , -1, &min_id, &max_id, &num_instance);

    if(num_instance) {
 
	for( i = min_id; i< max_id; i++) {
	    bn_attrib_free(&elem);
	    err = st_series_lookup_elem(series, i, &elem, NULL);
	    if(elem) {
		err = bn_attrib_get_int64(elem, NULL, NULL, &num_diff);
		bail_error(err);
		    /*The counter tot_cl_send_bm_dm  gives the  total bytes delivered from BM or DM cache*/
		if ( strstr( ts_str(series->ssr_node_name), "tot_cl_send_bm_dm"))
		    ctxt->client_bytes += num_diff;
		else
		    ctxt->total_bytes += num_diff;
	    }

	}
    }

 bail:
    bn_attrib_free(&elem);
    return(err);
}

static int
stm_get_disk_bindings(st_dataset *dataset, st_series *series, void *data)
{
    int err = 0;
    bn_attrib *elem = NULL;
    stm_disk_ctxt *ctxt = data;
    bn_binding *binding = NULL;
    tstr_array *source_node_parts = NULL;
    const tstring *diskname = NULL;

    bail_null(ctxt);

    err = st_series_get_last_elem(series, NULL, NULL, &elem, NULL);
    bail_error(err);

    if (elem) {
        err = bn_binding_new_attrib(&binding, ts_str(series->ssr_node_name), elem); 
	bail_error(err);

	err = bn_binding_array_append(ctxt->bindings, binding);
	bail_error(err);

	err = bn_binding_name_to_parts(ts_str(series->ssr_node_name), &source_node_parts, NULL);
	bail_error(err);

	diskname = tstr_array_get_quick(source_node_parts, 4);
	bail_null(diskname);
	
	if ( tstr_array_linear_search_str(ctxt->disknames, ts_str(diskname), 0, NULL)){ 
	    err = tstr_array_append_str(ctxt->disknames, ts_str(diskname));
	    bail_error(err);
	}
/*	err = bn_attrib_get_uint64(elem, NULL, NULL, &num);
        bail_error(err);*/
    }

 bail:
    tstr_array_free(&source_node_parts);
    bn_attrib_free(&elem);
    bn_binding_free(&binding);
    return(err);
}



static int
stm_nkn_pernode_calc(st_dataset *dataset, st_series *series, void *data)
{
    int err = 0;
    bn_attrib *elem = NULL;
    bn_binding *binding = NULL;
    int64 value = 0;
    tstring *interface_name = NULL;
    tstring *stat_name = NULL;
    char output_binding_name[64];
    pernodeCtxt *ctxt = (pernodeCtxt *)data;
    uint32 elapsed_time = ctxt->elapsedtime;
    uint64 x_per_sec;

    err = st_series_get_last_elem(series, NULL, NULL, &elem, NULL);
    bail_error(err);

    if (elem) {
        err = bn_attrib_get_int64(elem, NULL, NULL, &value);
        bail_error(err);
    }

    /*
     * Extract from the node name of the series which interface id 
     * we are talking about here.
     */

     err = bn_binding_name_to_parts_va
	(ts_str(series->ssr_node_name),false,2,3,&interface_name,4,&stat_name);
     bail_error_null(err,interface_name);
     bail_error_null(err,stat_name);

    strcpy(output_binding_name,"/stat/nkn/nvsd/");
    strcat(output_binding_name,ts_str(interface_name));
    strcat(output_binding_name,"/perportbyte_rate");

    x_per_sec = value/elapsed_time;

    err = bn_binding_new_uint64
        (&binding, output_binding_name,
         ba_value, 0, x_per_sec);
    bail_error(err);
    
    err = bn_binding_array_append(ctxt->ret_instance,binding);
    bail_error(err);

 bail:
    bn_attrib_free(&elem);
    ts_free(&interface_name);
    ts_free(&stat_name);
    bn_binding_free(&binding);
    return(err);
}

static int
stm_nkn_stats_pct_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
	int err = 0;
	tbool ok = false;
	uint32 num_secs = 0;
 	uint64 total_x = 0, x_per_sec = 0;
    	int32 our_inst_id = 0, prev_inst_id = 0;
    	lt_time_sec our_inst_time = 0, prev_inst_time = 0;
    	bn_binding *binding = NULL;
	char output_binding_name[64];
     /*
     * First determine the time interval; if we have trouble with
     * this (probably because this is the first sample, or the
     * most recent sample was missing), we just skip the whole CHD
     * for this round.
     */
    err = st_dataset_get_last_instance
        (source_data, &ok, &our_inst_id, &our_inst_time, NULL);
    bail_error(err);

    if (!ok) {
        lc_log_debug(LOG_WARNING,"Problem getting last instance of %s CHD",chd_id);
        goto bail;
    }
    if (our_inst_id <= 0) {
        goto bail;
    }

    err = st_dataset_get_instance_time(source_data, our_inst_id - 1,
                                       &prev_inst_time, NULL);
    bail_error(err);

    if (prev_inst_time <= 0 || prev_inst_time >= our_inst_time) {
        goto bail;
    }

    num_secs = our_inst_time - prev_inst_time;


    err = st_dataset_iterate_series((st_dataset *)source_data,
                                    stm_total_sum_elements,
                                    &total_x);
    bail_error(err);

    
    x_per_sec = total_x / num_secs;
    strcpy(output_binding_name,"/stat/nkn/nvsd/");
    strcat(output_binding_name,chd_id); 
    err = bn_binding_new_uint64
        (&binding, output_binding_name,
         ba_value, 0, x_per_sec);
    bail_error(err);

    err = bn_binding_array_append_takeover(ret_new_instance, &binding);
    bail_error(err);

 bail:
    return(err);

}

static int
stm_nkn_avg_stats_pct_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
    int err = 0;
    tbool ok = false;
    uint32 num_secs = 0;
    uint64 total_x = 0, x_per_sec = 0;
    lt_dur_ms system_uptime = 0;
    bn_binding *binding = NULL;
    char output_binding_name[64];

    uint32 code = 0;
    err = mdc_get_binding_fmt
         (st_mcc,
         &code, NULL, false, &binding, NULL,
         "/pm/monitor/process/nvsd/uptime");
         //"/system/uptime");
    bail_error(err);

    if (binding) {
        err = bn_binding_get_duration_ms(binding, ba_value, NULL, &system_uptime);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = st_dataset_iterate_series((st_dataset *)source_data,
                                stm_avg_total_sum_elements,
                                    &total_x);
    bail_error(err);
    if (system_uptime != 0 ) {
        x_per_sec = ((total_x * 1000) / system_uptime);
    } 
    else {
        x_per_sec = 0;
    }
    strcpy(output_binding_name,"/stat/nkn/nvsd/");
    strcat(output_binding_name,chd_id);
    err = bn_binding_new_uint64
        (&binding, output_binding_name,
         ba_value, 0, x_per_sec);
    bail_error(err);

    err = bn_binding_array_append_takeover(ret_new_instance, &binding);
    bail_error(err);

 
bail :
    bn_binding_free(&binding);
    return (err);
}
static int
stm_nkn_avg32_stats_pct_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
    int err = 0;
    tbool ok = false;
    uint32 num_secs = 0;
    uint32 total_x = 0, x_per_sec = 0;
    lt_dur_ms system_uptime = 0;
    bn_binding *binding = NULL;
    char output_binding_name[64];

    uint32 code = 0;
    err = mdc_get_binding_fmt
         (st_mcc,
         &code, NULL, false, &binding, NULL,
         "/pm/monitor/process/nvsd/uptime");
         //"/system/uptime");
    bail_error(err);

    if (binding) {
        err = bn_binding_get_duration_ms(binding, ba_value, NULL, &system_uptime);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = st_dataset_iterate_series((st_dataset *)source_data,
                                stm_avg32_total_sum_elements,
                                    &total_x);
    bail_error(err);

    if (system_uptime != 0 ) {
        x_per_sec = ((total_x * 1000 )/ system_uptime) ;
    }
    else {
        x_per_sec = 0;
    }
    strcpy(output_binding_name,"/stat/nkn/nvsd/");
    strcat(output_binding_name,chd_id);
    err = bn_binding_new_uint32
        (&binding, output_binding_name,
         ba_value, 0, x_per_sec);
    bail_error(err);

    err = bn_binding_array_append_takeover(ret_new_instance, &binding);
    bail_error(err);

 
bail :
    bn_binding_free(&binding);
    return (err);
}

static int
stm_nkn_pernode_stats_pct_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
    int err = 0;
    int32 our_inst_id = 0, prev_inst_id = 0;
    lt_time_sec our_inst_time = 0, prev_inst_time = 0;
    bn_binding *binding = NULL;
    char output_binding_name[64];
    tbool ok = false;
    uint32 num_secs = 0;
    pernodeCtxt ctxt;
    ctxt.ret_instance = ret_new_instance;

    /*
    * First determine the time interval; if we have trouble with
    * this (probably because this is the first sample, or the
    * most recent sample was missing), we just skip the whole CHD
    * for this round.
    */
    err = st_dataset_get_last_instance
        (source_data, &ok, &our_inst_id, &our_inst_time, NULL);
    bail_error(err);

    if (!ok) {
        lc_log_debug(LOG_WARNING,"Problem getting last instance of %s CHD",chd_id);
        goto bail;
    }
    if (our_inst_id <= 0) {
        goto bail;
    }

    err = st_dataset_get_instance_time(source_data, our_inst_id - 1,
                                       &prev_inst_time, NULL);
    bail_error(err);

    if (prev_inst_time <= 0 || prev_inst_time >= our_inst_time) {
        goto bail;
    }

    num_secs = our_inst_time - prev_inst_time;
    ctxt.elapsedtime = num_secs;

    err = st_dataset_iterate_series
		((st_dataset *)source_data,stm_nkn_pernode_calc,(void *)&ctxt);
    bail_error(err);

 bail :
	return (err);

}

static int
stm_nkn_cache_hit_ratio_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
    int err = 0;
    uint64 cache_hit_ratio = 0;
    bn_binding *binding = NULL;
    node_name_t binding_name = {0};
    stm_cache_hit_ctxt ctxt;

    memset(&ctxt, 0, sizeof(stm_cache_hit_ctxt));
    err = st_dataset_iterate_series((st_dataset *)source_data,
                                    stm_nkn_get_cache_hit_data,
                                    &ctxt);
    bail_error(err);

    if ( !ctxt.total_bytes ){ 
	cache_hit_ratio = 0; //To prevent 'divide by 0' error
    }
    else{
	cache_hit_ratio = (ctxt.client_bytes * 100)/ ctxt.total_bytes;
	if(cache_hit_ratio > 100) {
	    cache_hit_ratio = 100; // percent cant be more than 100
	}
    }

    snprintf(binding_name, sizeof(binding_name), "/stat/nkn/nvsd/%s", chd_id);
    err = bn_binding_new_uint64(&binding, binding_name,
         			ba_value, 0, cache_hit_ratio);
    bail_error(err);

    err = bn_binding_array_append_takeover(ret_new_instance, &binding);
    bail_error(err);

 bail:
    return(err);

}

static int
stm_nkn_stats_disk_io_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
        int err = 0;
        tbool ok = false;
        uint32 num_secs = 0, i=0;
        int32 our_inst_id = 0, prev_inst_id = 0;
        lt_time_sec our_inst_time = 0, prev_inst_time = 0;
        const bn_binding *binding = NULL;
	bn_binding *new_binding = NULL;
	stm_disk_ctxt ctxt;
	const char *disknameref = NULL;
	str_value_t disktype = {0}, diskname = {0};
	node_name_t bn_name = {0};
	int64 bytes_read = 0, bytes_written = 0;
	uint64 disk_io = 0;
	
     memset( &ctxt, 0, sizeof(stm_disk_ctxt));
     err = bn_binding_array_new(&ctxt.bindings);
     bail_error_null(err, ctxt.bindings);
     
     err = tstr_array_new(&ctxt.disknames, NULL);
     bail_error_null(err, ctxt.disknames);

     if ( strstr(chd_id, "sas"))
	snprintf(disktype, sizeof(disktype),"sas");
     else if ( strstr(chd_id, "sata"))
	snprintf(disktype, sizeof(disktype),"sata");
     else if ( strstr(chd_id, "ssd"))
	snprintf(disktype, sizeof(disktype),"ssd");
     else if ( strstr(chd_id, "root"))
	snprintf(disktype, sizeof(disktype),"root");

     /*
     * First determine the time interval; if we have trouble with
     * this (probably because this is the first sample, or the
     * most recent sample was missing), we just skip the whole CHD
     * for this round.
     */
    err = st_dataset_get_last_instance
        (source_data, &ok, &our_inst_id, &our_inst_time, NULL);
    bail_error(err);

    if (!ok) {
        lc_log_debug(LOG_WARNING,"Problem getting last instance of %s CHD",chd_id);
        goto bail;
    }
    if (our_inst_id <= 0) {
        goto bail;
    }

    err = st_dataset_get_instance_time(source_data, our_inst_id - 1,
                                       &prev_inst_time, NULL);
    bail_error(err);

    if (prev_inst_time <= 0 || prev_inst_time >= our_inst_time) {
        goto bail;
    }

    num_secs = our_inst_time - prev_inst_time;


    err = st_dataset_iterate_series((st_dataset *)source_data,
                                    stm_get_disk_bindings,
                                    &ctxt);
    bail_error(err);

    for ( i = 0; i < tstr_array_length_quick(ctxt.disknames) ; i++ ) {
	disknameref = tstr_array_get_str_quick(ctxt.disknames, i);
	bail_null(disknameref);

	if (! safe_strcmp(disktype, "root"))
	    snprintf(diskname, sizeof(diskname),"\\%s", disknameref);
	else
	    snprintf(diskname, sizeof(diskname), "%s", disknameref);

	err = bn_binding_array_get_binding_by_name_fmt(ctxt.bindings, &binding,
						"/nkn/monitor/%s/disk/%s/read",
					 	disktype, diskname);
	bail_error(err);	

	if ( binding ) {
	    err = bn_binding_get_int64(binding, ba_value, NULL, &bytes_read);
	    bail_error(err);
	} 
	else
	   bytes_read = 0;

	err = bn_binding_array_get_binding_by_name_fmt(ctxt.bindings, &binding,
						"/nkn/monitor/%s/disk/%s/write",
					 	disktype, diskname);
	bail_error(err);
	
	if ( binding ) {
	    err = bn_binding_get_int64(binding, ba_value, NULL, &bytes_written);
	    bail_error(err);
	}
	else
	    bytes_written = 0;

	disk_io = (bytes_read + bytes_written)/num_secs;

	snprintf(bn_name, sizeof(bn_name), "/nkn/monitor/%s/disk/%s/disk_io", disktype, diskname);
	err = bn_binding_new_uint64(&new_binding, bn_name, ba_value, 0, disk_io);
	bail_error(err);

        err = bn_binding_array_append_takeover(ret_new_instance, &new_binding);
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&ctxt.bindings);
    tstr_array_free(&ctxt.disknames);
    return(err);

}

static int
stm_nkn_stats_disk_bw_chd(const char *chd_id,
                const char *chd_func_name,
                const st_dataset *source_data,
                const tstr_array *source_nodes,
                const tstr_array_array *source_nodes_parts,
                const st_dataset *chd_data,
                void *callback_data,
                int32 first_inst_id,
                int32 last_inst_id,
                bn_binding_array *ret_new_instance)
{
        int err = 0;
        tbool ok = false;
        uint32 num_secs = 0, i=0;
        int32 our_inst_id = 0, prev_inst_id = 0;
        lt_time_sec our_inst_time = 0, prev_inst_time = 0;
        const bn_binding *binding = NULL;
        bn_binding *new_binding = NULL;
        stm_disk_ctxt ctxt;
        const char *disknameref = NULL;
        str_value_t disktype = {0}, diskname = {0};
        node_name_t bn_name = {0};
        int64 bytes_read = 0;
        uint64 disk_bw = 0;
     memset( &ctxt, 0, sizeof(stm_disk_ctxt));

     err = bn_binding_array_new(&ctxt.bindings);
     bail_error_null(err, ctxt.bindings);

     err = tstr_array_new(&ctxt.disknames, NULL);
     bail_error_null(err, ctxt.disknames);

     if ( strstr(chd_id, "sas"))
        snprintf(disktype, sizeof(disktype),"sas");
     else if ( strstr(chd_id, "sata"))
        snprintf(disktype, sizeof(disktype),"sata");
     else if ( strstr(chd_id, "ssd"))
        snprintf(disktype, sizeof(disktype),"ssd");
     else if ( strstr(chd_id, "root"))
        snprintf(disktype, sizeof(disktype),"root");

     /*
     * First determine the time interval; if we have trouble with
     * this (probably because this is the first sample, or the
     * most recent sample was missing), we just skip the whole CHD
     * for this round.
     */
    err = st_dataset_get_last_instance
        (source_data, &ok, &our_inst_id, &our_inst_time, NULL);
    bail_error(err);

    if (!ok) {
        lc_log_debug(LOG_WARNING,"Problem getting last instance of %s CHD",chd_id);
        goto bail;
    }
    if (our_inst_id <= 0) {
        goto bail;
    }

    err = st_dataset_get_instance_time(source_data, our_inst_id - 1,
                                       &prev_inst_time, NULL);
    bail_error(err);

    if (prev_inst_time <= 0 || prev_inst_time >= our_inst_time) {
        goto bail;
    }

    num_secs = our_inst_time - prev_inst_time;


    err = st_dataset_iterate_series((st_dataset *)source_data,
                                    stm_get_disk_bindings,
                                    &ctxt);
    bail_error(err);

    for ( i = 0; i < tstr_array_length_quick(ctxt.disknames) ; i++ ) {
        disknameref = tstr_array_get_str_quick(ctxt.disknames, i);
        bail_null(disknameref);

        if (! safe_strcmp(disktype, "root"))
            snprintf(diskname, sizeof(diskname),"\\%s", disknameref);
        else
            snprintf(diskname, sizeof(diskname), "%s", disknameref);

        err = bn_binding_array_get_binding_by_name_fmt(ctxt.bindings, &binding,
                                                "/nkn/monitor/%s/disk/%s/read",
                                                disktype, diskname);
        bail_error(err);

        if ( binding ) {
            err = bn_binding_get_int64(binding, ba_value, NULL, &bytes_read);
            bail_error(err);
        }
        else
           bytes_read = 0;

        disk_bw = ((uint64)bytes_read)/num_secs;

        snprintf(bn_name, sizeof(bn_name), "/nkn/monitor/%s/disk/%s/disk_bw", disktype, diskname);
        err = bn_binding_new_uint64(&new_binding, bn_name, ba_value, 0, disk_bw);
        bail_error(err);

        err = bn_binding_array_append_takeover(ret_new_instance, &new_binding);
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&ctxt.bindings);
    tstr_array_free(&ctxt.disknames);
    return(err);

}


int 
stm_nkn_stats_init(const lc_dso_info *info, 
                void *data)
{
        int err = 0;
        st_report_reg *rpt = NULL;
#if 1
        err = st_register_report
                ("bandwidth_day_avg", _("Avg. Bandwidth Usage"), 
                 sst_chd, "bandwidth_day_avg", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Avg. Bandwidth Usage"), 1,
                 "/stat/nkn/nvsd/total_byte_rate",
                 _("Avg Total Byte Rate "),
                 _("Bytes/sec"));
        bail_error(err);


        err = st_register_report
                ("bandwidth_day_peak", _("Peak. Bandwidth Usage"), 
                 sst_chd, "bandwidth_day_peak", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Peak Bandwidth Usage"), 1,
                 "/stat/nkn/nvsd/total_byte_rate",
                 _("Peak Total Byte Rate "),
                 _("Bytes/sec"));
        bail_error(err);


        err = st_register_report
                ("connection_day_avg", _("Avg Connection Count"), 
                 sst_chd, "connection_day_avg", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Avg Connection Count"), 1,
                 "/stat/nkn/nvsd/num_connections",
                 _("Avg Connection Count "),
                 _("Count"));
        bail_error(err);


        err = st_register_report
                ("connection_day_peak", _("Peak Connection Count"), 
                 sst_chd, "connection_day_peak", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Peak Connection Count"), 1,
                 "/stat/nkn/nvsd/num_connections",
                 _("Peak Connection Count "),
                 _("Count"));
        bail_error(err);

        // Weekly report
        err = st_register_report
                ("bandwidth_week_avg", _("Avg. Bandwidth Usage weekly"), 
                 sst_chd, "bandwidth_week_avg", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Avg. Weekly Bandwidth Usage "), 1,
                 "/stat/nkn/nvsd/total_byte_rate",
                 _("Avg Weekly Total Byte Rate "),
                 _("Bytes/sec"));
        bail_error(err);


        err = st_register_report
                ("bandwidth_week_peak", _("Weekly Peak. Bandwidth Usage"), 
                 sst_chd, "bandwidth_week_peak", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Weekly Peak Bandwidth Usage"), 1,
                 "/stat/nkn/nvsd/total_byte_rate",
                 _("Weekly Peak Total Byte Rate "),
                 _("Bytes/sec"));
        bail_error(err);


        err = st_register_report
                ("connection_week_avg", _("Weekly Avg Connection Count"), 
                 sst_chd, "connection_week_avg", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Weekly Avg Connection Count"), 1,
                 "/stat/nkn/nvsd/num_connections",
                 _("Weekly Avg Connection Count "),
                 _("Count"));
        bail_error(err);


        err = st_register_report
                ("connection_week_peak", _("Weekly Peak Connection Count"), 
                 sst_chd, "connection_week_peak", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Weekly Peak Connection Count"), 1,
                 "/stat/nkn/nvsd/num_connections",
                 _("Weekly Peak Connection Count "),
                 _("Count"));
        bail_error(err);

        // Monthly report
        err = st_register_report
                ("bandwidth_month_avg", _("Monthly Avg. Bandwidth Usage"), 
                 sst_chd, "bandwidth_month_avg", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Avg. Monthly Bandwidth Usage "), 1,
                 "/stat/nkn/nvsd/total_byte_rate",
                 _("Monthly Avg Total Byte Rate "),
                 _("Bytes/sec"));
        bail_error(err);


        err = st_register_report
                ("bandwidth_month_peak", _("Monthly Peak. Bandwidth Usage"), 
                 sst_chd, "bandwidth_month_peak", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Monthly Peak Bandwidth Usage"), 1,
                 "/stat/nkn/nvsd/total_byte_rate",
                 _("Monthly Peak Total Byte Rate "),
                 _("Bytes/sec"));
        bail_error(err);


        err = st_register_report
                ("connection_month_avg", _("Monthly Avg Connection Count"), 
                 sst_chd, "connection_month_avg", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Monthly Avg Connection Count"), 1,
                 "/stat/nkn/nvsd/num_connections",
                 _("Monthly Avg Connection Count "),
                 _("Count "));
        bail_error(err);


        err = st_register_report
                ("connection_month_peak", _("Monthly Peak Connection Count"), 
                 sst_chd, "connection_month_peak", &rpt);
        bail_error_null(err, rpt);

        err = st_register_report_group
                (rpt,  _("Monthly Peak Connection Count"), 1,
                 "/stat/nkn/nvsd/num_connections",
                 _("Monthly Peak Connection Count "),
                 _("Count"));
        bail_error(err);
#endif
	/*! Real time based statistics collection . 
          * ex : rate/sec 
 	  */
        err = st_register_chd("total_byte_rate_pct", stm_nkn_stats_pct_chd, NULL);
        bail_error(err);
//        err = st_register_chd("connection_rate_pct", stm_nkn_avg_stats_pct_chd, NULL);
        err = st_register_chd("connection_rate_pct", stm_nkn_stats_pct_chd, NULL);
        bail_error(err);
        err = st_register_chd("cache_byte_rate_pct", stm_nkn_stats_pct_chd, NULL);
        bail_error(err);
        err = st_register_chd("disk_byte_rate_pct", stm_nkn_stats_pct_chd, NULL);
        bail_error(err);
        err = st_register_chd("origin_byte_rate_pct", stm_nkn_stats_pct_chd, NULL);
        bail_error(err);
//        err = st_register_chd("http_transaction_rate_pct", stm_nkn_avg_stats_pct_chd, NULL);
        err = st_register_chd("http_transaction_rate_pct", stm_nkn_stats_pct_chd, NULL);
        bail_error(err);

        err = st_register_chd("nkn_disk_io", stm_nkn_stats_disk_io_chd, NULL);
        bail_error(err);
        err = st_register_chd("nkn_disk_bw", stm_nkn_stats_disk_bw_chd, NULL);
        bail_error(err);
        err = st_register_chd("cache_hit_ratio", stm_nkn_cache_hit_ratio_chd, NULL);
        bail_error(err);
bail:
        return err;
}


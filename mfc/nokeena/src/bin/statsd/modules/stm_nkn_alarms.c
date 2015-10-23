#include "common.h"
#include "dso.h"
#include "st_registration.h"
#include "md_client.h"
#include "nkn_mgmt_defs.h"

int stm_nkn_alarms_init(const lc_dso_info *info, void *data);
int
stm_reg_rp_alarm( const bn_binding_array *,
	uint32 ,
	const bn_binding *,
	const tstring *,
	const tstr_array *,
	const tstring *,
	const tstring *,
	void *);
int st_reg_rp_alarms(void);

int stm_nkn_alarms_threshold_monitoring(const char *alarm_id, const char *node_name,
                                   const bn_attrib *node_value,
                                   void *callback_data,
                                   bn_binding_array *ret_bindings);

int stm_nkn_disk_alarms(const char *alarm_id, const char *node_name,
                        const bn_attrib *node_value,
                        void *callback_data,
                        bn_binding_array *ret_bindings);

int
stm_nkn_alarms_rp_callback(const char *alarm_id, const char *node_name,
	const bn_attrib *node_value,
	void *callback_data,
	bn_binding_array *ret_bindings);


int
stm_reg_rp_alarm_handler(const tstring *rp_name);


int
st_alarm_handle_config_change(const bn_binding_array *old_bindings,
	const bn_binding_array *new_bindings, void *data);
/* ------------------------------------------------------------------------- */

const str_value_t system_alarms[] = {
    "cache_byte_rate",
    "disk_byte_rate",
    "origin_byte_rate",
    "connection_rate",
    "http_transaction_rate",
    "intf_util",
    "aggr_cpu_util",
    "nkn_mem_util",
    "appl_cpu_util",
    "cache_hit_ratio",
    "paging"
};

const str_value_t disk_alarms[] = {
    "sas_disk_space",
    "sata_disk_space",
    "ssd_disk_space",
    "root_disk_space",
    "sas_disk_bw",
    "sata_disk_bw",
    "ssd_disk_bw",
    "root_disk_bw",
    "sas_disk_io",
    "sata_disk_io",
    "ssd_disk_io",
    "root_disk_io"
};

int
stm_nkn_alarms_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    uint32 i = 0;


    for ( i = 0; i < sizeof(system_alarms)/sizeof(str_value_t); i++ ) {

	err = st_register_alarm_handler
		(system_alarms[i], stm_nkn_alarms_threshold_monitoring, NULL);
	bail_error(err);
    }

    for ( i = 0; i < sizeof(disk_alarms)/sizeof(str_value_t); i++ ) {

	err = st_register_alarm_handler
		(disk_alarms[i], stm_nkn_disk_alarms, NULL);
	bail_error(err);
    }

    /* register callback for resource pool alarms */
    err = st_reg_rp_alarms();
    bail_error(err);

    /* register yourself as a interested party for configureation chages */
    err = st_config_change_notif_register(st_alarm_handle_config_change, NULL);
    bail_error(err);

 bail:
    return(err);
}

/*
 * This function is called by the statsd when ever there is config change.
 * we go through all node names and find out if there is new addition of
 * resource pool. If old_bindings are zero and new bindings matches pattern.
 * of resource pool config nodes. If found, this function will register a
 * callback handler for the newly added resource pool
 */
int
st_alarm_handle_config_change(const bn_binding_array *old_bindings,
	const bn_binding_array *new_bindings, void *data)
{
    int err = 0, num_bindings = 0, i = 0, old_num = 0;
    const tstring *node_name = NULL;
    tstring *rp_name = NULL;
    const bn_binding *node = NULL;

    /* not handling deletion of a resource pool */
    old_num = bn_binding_array_length_quick(old_bindings);
    for (i = 0; i < old_num; i++) {
	node = bn_binding_array_get_quick(old_bindings, i);
	if (node == NULL) continue;
	node_name = bn_binding_get_name_quick(node);
	if (bn_binding_name_pattern_match(ts_str(node_name), "/nkn/nvsd/resource_mgr/config/*")) {
	    /*
	     * deletion of a resource pool has taken place. don't do anything 
	     * simply return
	     */
	    err = 0;
	    goto bail;
	}
    }
    /* check if there is any addition of new resource pool */
    num_bindings = bn_binding_array_length_quick(new_bindings);

    for (i = 0; i < num_bindings; i++) {
	node = bn_binding_array_get_quick(new_bindings, i);
	if (node == NULL) continue;
	node_name = bn_binding_get_name_quick(node);
	if (bn_binding_name_pattern_match(ts_str(node_name), "/nkn/nvsd/resource_mgr/config/*")) {
	    ts_free(&rp_name);

	    err = bn_binding_name_get_last_part(ts_str(node_name), &rp_name);
	    bail_error(err);

	    err = stm_reg_rp_alarm_handler(rp_name);
	    bail_error(err);
	}
    }

bail:
    ts_free(&rp_name);
    return err;
}

/*
 * we need to register alarms for all resource pool alarms, sinc name of those
 * alarms are dynamic in nature, we need to register callback for all the alarms
 * by iterating over all config nodes and then register two callback for each
 * resource pool, one is for max connection and other is for max bandwidth
 * we need callback so that we can send bindings along with the event for
 * snmp traps
 */
int
st_reg_rp_alarms(void)
{
    int err = 0;
    uint32 num_rp;

    err = mdc_foreach_binding(st_mcc,
    	    "/nkn/nvsd/resource_mgr/monitor/external/*",
	    NULL,
	    stm_reg_rp_alarm ,
	    NULL,
	    &num_rp);
    bail_error(err);

bail:
    return (err);
}

/*
 * This is a callback function while iterating over resoure pool config nodes
 */
int
stm_reg_rp_alarm(
	const bn_binding_array *bindings,
	uint32 idx, const bn_binding *binding,
	const tstring *name, const tstr_array *name_components,
	const tstring *name_last_part,
	const tstring *value,
	void *callback_data)
{
    int err  = 0;

    err = stm_reg_rp_alarm_handler(name_last_part);
    bail_error(err);

bail:
    return err;
}

int
stm_reg_rp_alarm_handler(const tstring *rp_name)
{
    int err = 0;
    char * alarm_name = NULL;

    /*
     * We cant avoid the memory leak for alarm_name here
     * the memory allocated here is used up in the registeration as it is. 
     * This leak will  becaused everytime the resource pool is added/deleted.
     */

    lc_log_basic(LOG_DEBUG, "rp-name - %s", ts_str(rp_name));
    alarm_name = smprintf("rp_%s_client_sessions", ts_str(rp_name));
    bail_null(alarm_name);

    err = st_register_alarm_handler(alarm_name, stm_nkn_alarms_rp_callback, NULL);
    bail_error(err);
 
    alarm_name = smprintf("rp_%s_max_bandwidth", ts_str(rp_name));
    bail_null(alarm_name);

    err = st_register_alarm_handler(alarm_name, stm_nkn_alarms_rp_callback,NULL);
    bail_error(err);

bail:
    return err;
}
/* ------------------------------------------------------------------------- */
int
stm_nkn_alarms_threshold_monitoring(const char *alarm_id, const char *node_name,
                               const bn_attrib *node_value,
                               void *callback_data,
                               bn_binding_array *ret_bindings)
{
    int err = 0;
    node_name_t alarm_node = {0}, rising_node = {0}, bn_name = {0}, thr_node = {0} ;
    bn_binding_array *alarmconfig_nodes = NULL;
    tstring *trigger_type = NULL, *trigger_id = NULL, *t_nodename = NULL, *t_err_thr = NULL, *t_clr_thr = NULL;
    tstr_array *node_parts = NULL;
    tbool is_rising = false;
    char *node_name_esc = NULL; 
    bn_binding *binding = NULL;
    uint64_t last_value = 0;
    uint32 thr_val = 0, last_value_int32 = 0;
    bn_type type = 0;
    int64 last_value_int64 = 0;

    //bail_require(!strcmp(node_name, "/demo/echod/state/bad_char_count"));

    lc_log_basic(LOG_DEBUG, "Got error alarm on node %s",node_name);

    snprintf(alarm_node, sizeof(alarm_node), "/stats/config/alarm/%s", alarm_id);

    err = mdc_get_binding_children( st_mcc, NULL, NULL, false,
					&alarmconfig_nodes, true, true, alarm_node);   
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name_fmt( alarmconfig_nodes, NULL, &trigger_type,
							"%s/trigger/type", alarm_node);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name_fmt( alarmconfig_nodes, NULL, &trigger_id,
							"%s/trigger/id", alarm_node);
    bail_error(err);

    err = bn_binding_name_escape_str( node_name, &node_name_esc );
    bail_error(err);

/* /stats/state/chd/mon_conection_rate/node/\\/stat\\/nkn\\/nvsd\\/connection_rate/last/value */

    snprintf(bn_name, sizeof(bn_name), "/stats/state/%s/%s/node/%s/last/value", ts_str(trigger_type),
					ts_str(trigger_id), node_name_esc );
   
    err = bn_binding_new_attrib(&binding, bn_name, node_value);
    bail_error(err);
 
    err = bn_attrib_get_type(node_value, &type, NULL); 
    bail_error(err);
 
    switch(type) {
        case bt_uint64:
            err = bn_binding_get_uint64(binding, ba_value, NULL, &last_value);
       	    bail_error(err);
        break;
    	case bt_uint32:
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &last_value_int32);
            bail_error(err);
	    last_value = (uint64)last_value_int32;
	break;
        case bt_int64:
	    err = bn_binding_get_int64(binding, ba_value, NULL, &last_value_int64);
            bail_error(err);
	    last_value = (uint64)last_value_int64;
        break;
        default:
    	    lc_log_basic(LOG_WARNING, "Unexpected binding type in alarm handler");
	    goto bail;
    }


    err = bn_binding_array_append_takeover(ret_bindings, &binding);
    bail_error(err);

    snprintf(rising_node, sizeof(rising_node), "%s/rising/enable", alarm_node);
    err = bn_binding_array_get_value_tbool_by_name( alarmconfig_nodes, rising_node, NULL, &is_rising);
    bail_error(err);

/* Send error threshold if it's a rise alarm and clear threshold if it's a clear alarm */
    if ( is_rising ) {
        err = bn_binding_array_get_value_tstr_by_name_fmt( alarmconfig_nodes, NULL, &t_err_thr,
	    						    "%s/rising/error_threshold", alarm_node);
        bail_error(err);
 	thr_val = atoi( ts_str(t_err_thr) );
        if ( last_value >= thr_val ) {
   	    snprintf(thr_node, sizeof(thr_node), "%s/rising/error_threshold", alarm_node);
	}
	else {
   	    snprintf(thr_node, sizeof(thr_node), "%s/rising/clear_threshold", alarm_node);
            err = bn_binding_array_get_value_tstr_by_name( alarmconfig_nodes, thr_node, NULL, &t_clr_thr);
	    bail_error(err);
  	    thr_val = atoi( ts_str(t_clr_thr));
	}	
   }
   else {
        err = bn_binding_array_get_value_tstr_by_name_fmt( alarmconfig_nodes, NULL, &t_err_thr,
                                                            "%s/falling/error_threshold", alarm_node);
        bail_error(err);
 	thr_val = atoi( ts_str(t_err_thr) );
        if ( last_value <= thr_val ) {
   	    snprintf(thr_node, sizeof(thr_node), "%s/falling/error_threshold", alarm_node);
        }
        else {
   	    snprintf(thr_node, sizeof(thr_node), "%s/falling/clear_threshold", alarm_node);
            err = bn_binding_array_get_value_tstr_by_name( alarmconfig_nodes, thr_node, NULL, &t_clr_thr);
	    bail_error(err);
  	    thr_val = atoi( ts_str(t_clr_thr));
        }
   }	
   err = bn_binding_new_uint32( &binding, thr_node, ba_value, 0, thr_val); 
   bail_error(err);
   err = bn_binding_array_append_takeover(ret_bindings, &binding);
   bail_error(err);
	
 bail:
    bn_binding_array_free(&alarmconfig_nodes);
    tstr_array_free(&node_parts);
    ts_free(&trigger_type);
    ts_free(&trigger_id);
    ts_free(&t_err_thr);
    ts_free(&t_clr_thr);
    bn_binding_free(&binding);
    return(err);
}
int
stm_nkn_disk_alarms(const char *alarm_id, const char *node_name,
                    const bn_attrib *node_value,
                    void *callback_data,
                    bn_binding_array *ret_bindings)
{
    int err = 0;
    node_name_t alarm_node = {0}, rising_node = {0}, bn_name = {0}, thr_node = {0} ;
    bn_binding_array *alarmconfig_nodes = NULL;
    tstring *t_nodename = NULL, *t_err_thr = NULL, *t_clr_thr = NULL;
    tstr_array *node_parts = NULL;
    tbool is_rising = false;
    char *node_name_esc = NULL;
    bn_binding *binding = NULL;
    uint64_t last_value = 0;
    uint32 thr_val = 0;
    tstring *diskname = NULL;
    str_value_t disk_node = {0};
    str_value_t var_bindings[3] = {{0}, {0}, {0}};


    lc_log_basic(LOG_DEBUG, "Got error alarm on node %s",node_name);

/* Send the disk name */

    err = bn_binding_name_to_parts(node_name, &node_parts, NULL);
    bail_error(err);

    diskname = tstr_array_get_quick(node_parts, 4);
    bail_null(diskname);

    if ( strstr(alarm_id, "root"))
        snprintf(disk_node, sizeof(disk_node), "/nkn/monitor/disk/\\%s", ts_str(diskname));
    else
        snprintf(disk_node, sizeof(disk_node), "/nkn/monitor/disk/%s", ts_str(diskname));

    err = bn_binding_new_str_autoinval(&binding, disk_node, ba_value,
					bt_string, 0, ts_str(diskname));
    bail_error(err);
    
    err = bn_binding_array_append_takeover(ret_bindings, &binding);
    bail_error(err);

    if ( strstr(alarm_id, "disk_space")) {
	snprintf(var_bindings[0], sizeof(var_bindings[0]), "%s", "freespace");
	snprintf(var_bindings[1], sizeof(var_bindings[1]), "%s", "spaceerr");
	snprintf(var_bindings[2], sizeof(var_bindings[2]), "%s", "spaceclr");
    }
    else if ( strstr(alarm_id, "disk_io")) {
        snprintf(var_bindings[0], sizeof(var_bindings[0]), "%s", "iorate");
        snprintf(var_bindings[1], sizeof(var_bindings[1]), "%s", "ioerr");
        snprintf(var_bindings[2], sizeof(var_bindings[2]), "%s", "ioclr");
    }
    else if ( strstr(alarm_id, "disk_bw")) {
        snprintf(var_bindings[0], sizeof(var_bindings[0]), "%s", "diskbw");
        snprintf(var_bindings[1], sizeof(var_bindings[1]), "%s", "bwerr");
        snprintf(var_bindings[2], sizeof(var_bindings[2]), "%s", "bwclr");
    }    

    err = bn_attrib_get_uint64(node_value, NULL, NULL, &last_value);
    bail_error(err);

    snprintf(bn_name, sizeof(bn_name), "%s/%s", disk_node, var_bindings[0]);
    err = bn_binding_new_uint32(&binding, bn_name, ba_value, 0, last_value);
    bail_error(err);

    err = bn_binding_array_append_takeover(ret_bindings, &binding);
    bail_error(err);
    
    snprintf(alarm_node, sizeof(alarm_node), "/stats/config/alarm/%s", alarm_id);

    err = mdc_get_binding_children( st_mcc, NULL, NULL, false,
                                        &alarmconfig_nodes, true, true, alarm_node);
    bail_error(err);

    snprintf(rising_node, sizeof(rising_node), "%s/rising/enable", alarm_node);
    err = bn_binding_array_get_value_tbool_by_name( alarmconfig_nodes, rising_node, NULL, &is_rising);
    bail_error(err);

/* Send error threshold if it's a rise alarm and clear threshold if it's a clear alarm */
    if ( is_rising ) {
        err = bn_binding_array_get_value_tstr_by_name_fmt( alarmconfig_nodes, NULL, &t_err_thr,
                                                            "%s/rising/error_threshold", alarm_node);
        bail_error(err);
        thr_val = atoi( ts_str(t_err_thr) );
        if ( last_value > thr_val ) {
            snprintf(thr_node, sizeof(thr_node), "%s/rising/error_threshold", alarm_node);
            snprintf(bn_name, sizeof(bn_name), "%s/%s", disk_node, var_bindings[1]);
        }
        else {
            snprintf(thr_node, sizeof(thr_node), "%s/rising/clear_threshold", alarm_node);
            err = bn_binding_array_get_value_tstr_by_name( alarmconfig_nodes, thr_node, NULL, &t_clr_thr);
            bail_error(err);
            thr_val = atoi( ts_str(t_clr_thr));
            snprintf(bn_name, sizeof(bn_name), "%s/%s", disk_node, var_bindings[2]);
        }
   }
   else {
        err = bn_binding_array_get_value_tstr_by_name_fmt( alarmconfig_nodes, NULL, &t_err_thr,
                                                            "%s/falling/error_threshold", alarm_node);
        bail_error(err);
        thr_val = atoi( ts_str(t_err_thr) );
        if ( last_value < thr_val ) {
            snprintf(thr_node, sizeof(thr_node), "%s/falling/error_threshold", alarm_node);
            snprintf(bn_name, sizeof(bn_name), "%s/%s", disk_node, var_bindings[1]);
        }
        else {
            snprintf(thr_node, sizeof(thr_node), "%s/falling/clear_threshold", alarm_node);
            err = bn_binding_array_get_value_tstr_by_name( alarmconfig_nodes, thr_node, NULL, &t_clr_thr);
            bail_error(err);
            thr_val = atoi( ts_str(t_clr_thr));
            snprintf(bn_name, sizeof(bn_name), "%s/%s", disk_node, var_bindings[2]);
        }
   }
   err = bn_binding_new_uint32( &binding, bn_name, ba_value, 0, thr_val);
   bail_error(err);
   err = bn_binding_array_append_takeover(ret_bindings, &binding);
   bail_error(err);

 bail:
    bn_binding_array_free(&alarmconfig_nodes);
    tstr_array_free(&node_parts);
    ts_free(&t_err_thr);
    ts_free(&t_clr_thr);
    bn_binding_free(&binding);
    return(err);
}


int
stm_nkn_alarms_rp_callback(const char *alarm_id, const char *node,
	const bn_attrib *alarm_node_value,
	void *callback_data,
	bn_binding_array *ret_bindings)
{
    int err = 0, n = 0;
    node_name_t node_name = {0}, alarm_node = {0};
    tstr_array *binding_parts= NULL;
    const char *t_rp_name= NULL;
    bn_binding *binding = NULL;
    bn_binding_array *rp_nodes = NULL;
    tstring *node_value = NULL;
    uint64 client_sess = 0, client_bw = 0;

    /* /nkn/nvsd/resource_mgr/monitor/counter/ * /used/client_seesions */
    err = bn_binding_name_to_parts(node, &binding_parts, NULL);
    bail_error(err);

    t_rp_name = tstr_array_get_str_quick (binding_parts, 5);
    bail_null(t_rp_name);

    snprintf(node_name, sizeof(node_name),
	    "/nkn/nvsd/resource_mgr/monitor/counter/%s", t_rp_name);

    err = bn_binding_new_str
	(&binding, node_name, ba_value, bt_string, 0, t_rp_name);
    bail_error(err);

    err = bn_binding_array_append_takeover(ret_bindings, &binding);
    bail_error(err);

    snprintf(alarm_node, sizeof(alarm_node), 
	    "/nkn/nvsd/resource_mgr/monitor/counter/%s/used", t_rp_name);

    err = mdc_get_binding_children(st_mcc, NULL, NULL, false,
	    &rp_nodes, true, true, alarm_node);   
    bail_error(err);

    snprintf(alarm_node, sizeof(alarm_node), 
	    "/nkn/nvsd/resource_mgr/monitor/counter/%s/used/bw_mbps",
	    t_rp_name);
    err = bn_binding_array_get_value_tstr_by_name(rp_nodes,
	    alarm_node, NULL, &node_value);
    bail_error(err);

    if(node_value) {
	client_bw = strtoul(ts_str(node_value), NULL, 10);
    }
    err = bn_binding_new_uint64
	(&binding, alarm_node, ba_value, 0, client_bw);
    bail_error(err);

    ts_free(&node_value);

    err = bn_binding_array_append_takeover(ret_bindings, &binding);
    bail_error(err);

    snprintf(alarm_node, sizeof(alarm_node), 
	    "/nkn/nvsd/resource_mgr/monitor/counter/%s/used/client_sessions",
	    t_rp_name);

    err = bn_binding_array_get_value_tstr_by_name(rp_nodes,
	    alarm_node, NULL, &node_value);
    bail_error(err);

    if(node_value) {
	client_sess = strtoul(ts_str(node_value), NULL, 10);
    }
    err = bn_binding_new_uint64
	(&binding, alarm_node, ba_value, 0, client_sess);
    bail_error(err);

#if 0
    err = bn_binding_new_str
	(&binding, alarm_node, ba_value, bt_uint64, 0, ts_str_maybe_empty(node_value));
    bail_error(err);
#endif
    err = bn_binding_array_append_takeover(ret_bindings, &binding);
    bail_error(err);

 bail:
    ts_free(&node_value);
    tstr_array_free(&binding_parts);
    bn_binding_array_free(&rp_nodes);
    return(err);
}

#include "snmp_util.h"

static int is_internal_interface(const char *ifname, 
	tstring *dev_source);
static int is_interface_up(const char *ifname);
int snmp_get_physical_intf_speed(const char *ifname, u_long *ifspeed);
int snmp_get_bond_intf_speed(const char *ifname, u_long *speed);
int snmp_get_bond_speed(const char *bond_mode, bn_binding_array *binding,
	u_long *ifspeed);


/*
 * XXX - moive it to a common library
 * copied from cli_nvsd_namespace_cmds.c
 */
int
snmp_ns_get_names(tstr_array **ret_names)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    char *bn_name = NULL;

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(sn_get_admin_mcc(),
            NULL, NULL, &names_config,
            "/nkn/nvsd/namespace", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_delete_str(names_config, "mfc_probe");
    bail_error(err);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    safe_free(bn_name);
    return err;
}

int 
snmp_dc_get_disks(tstr_array **ret_labels)
{
    int err = 0;
    tstr_array *labels = NULL;

    bail_null(ret_labels);
    *ret_labels = NULL;

    err = mdc_get_binding_children_tstr_array(sn_get_admin_mcc(),
            NULL, NULL, &labels, 
            "/nkn/nvsd/diskcache/monitor/diskname", NULL);
    bail_error_null(err, labels);

    *ret_labels = labels;
    labels = NULL;
bail:
    tstr_array_free(&labels);
    return err;
}

int
snmp_get_interface_names(tstr_array **ret_names)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    char *bn_name = NULL;

    lc_log_debug(LOG_DEBUG, "getting interfaces");
    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(sn_get_admin_mcc(),
            NULL, NULL, &names_config,
            "/net/interface/state", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    safe_free(bn_name);
    return err;
}

int
snmp_get_if_speed(const char *ifname, u_long *if_speed)
{
    int err =0 ;
    node_name_t node_name = {0};
    tstring *dev_source = NULL;
    u_long speed = 0;

    *if_speed = 0;

    snprintf(node_name, sizeof(node_name),
	    "/net/interface/state/%s/devsource", ifname);
    err = mdc_get_binding_tstr(sn_get_admin_mcc(), NULL, NULL, NULL, &dev_source, 
	    node_name, NULL);
    bail_error(err);

    lc_log_debug(LOG_DEBUG, "ifname=> %s, source=> %s", ifname, ts_str(dev_source));
    /* check if bridge, loopback, virtual */
    if (is_internal_interface(ifname, dev_source)) {
	/* yes it is, show 10Mbps */
	*if_speed = 10;
	goto bail;
    }
    if (0 == strcmp(ts_str(dev_source), "bond")) {
	/* ignore link-up status for bonds */
	err = snmp_get_bond_intf_speed(ifname, &speed);
	bail_error(err);

	*if_speed = speed;
    } else if (0 == strcmp(ts_str(dev_source), "physical")) {
	/* check if is up */
	if (is_interface_up(ifname)) {
	    err = snmp_get_physical_intf_speed(ifname, &speed);
	    bail_error(err);
	    *if_speed = speed;
	} else {
	    /* what to show here? IF-MIB shows 10Mb/s */
	    *if_speed = 10;
	}
    } else {
	lc_log_basic(LOG_DEBUG, "unknown interface type: %s",
		ts_str(dev_source));
	/* setting to default speed (10Mb/s)*/
	*if_speed = 10;
	goto bail;
    }

bail:
    ts_free(&dev_source);
    return err;
}

static int 
is_internal_interface(const char *ifname, 
	tstring *dev_source)
{
    int internal = 1;

    if (0 == strcmp(ts_str(dev_source), "virtual")) {
    } else if (0 == strcmp(ts_str(dev_source), "bridge")) {
    } else if (0 == strcmp(ts_str(dev_source), "loopback")) {
    } else {
	internal = 0;
    }
    return internal;
}

static int
is_interface_up(const char *ifname)
{
    int err = 0 ;
    node_name_t node_name = {0};
    tbool link_up = false;
    int interface_up = 0;

    snprintf(node_name, sizeof(node_name),
	    "/net/interface/state/%s/flags/link_up", ifname);

    err = mdc_get_binding_tbool(sn_get_admin_mcc(), NULL, NULL, NULL,
	    &link_up, node_name, NULL);
    bail_error(err);

    if (link_up) {
	interface_up = 1;
    }
    lc_log_debug(LOG_DEBUG, "interface: %s, %s", 
	    ifname,interface_up ? "UP" : "DOWN");

bail:
    return interface_up;
}

int
snmp_get_physical_intf_speed(const char *ifname, u_long *ifspeed)
{
    int err = 0;
    node_name_t node_name = {0};
    tstring *speed = NULL;
    u_long speed_mbps = 0;

    snprintf(node_name, sizeof(node_name), 
	    "/net/interface/state/%s/speed", ifname);

    err = mdc_get_binding_tstr(sn_get_admin_mcc(), NULL, NULL, NULL,
	    &speed, node_name, NULL);
    bail_error_null(err, speed);

    if (speed) {
	/* we have got the speed */
	speed_mbps = strtol(ts_str(speed), NULL, 10);
	*ifspeed = speed_mbps ; 
    } else {
	*ifspeed = 0;
    }
    lc_log_debug(LOG_DEBUG, "if: %s, speed: %s (%ld)",
	    ifname, ts_str(speed), speed_mbps);

bail:
    ts_free(&speed);
    return err;
}

int
snmp_get_bond_intf_speed(const char *ifname, u_long *speed)
{
    int err = 0, found = 0, num = 0, i =0;;
    node_name_t node_name = {0};
    bn_binding_array *bindings = NULL, *slave_interfaces = NULL;
    uint32_t ret_code = 0;
    const char *bond_id = NULL;
    tstr_array *name_parts = NULL;
    const tstring *binding_name = NULL, *bond_name = NULL;
    bn_binding *binding = NULL;
    u_long bond_speed = 0;
    tstring *bond_mode = NULL;

    lc_log_debug(LOG_DEBUG, "bond %s", ifname);
    snprintf(node_name, sizeof(node_name), 
	    "/net/bonding/config" );

    err = mdc_get_binding_children(sn_get_admin_mcc(),
	    &ret_code, NULL,false,
	    &bindings, true, true, node_name);
    bail_error(err);

    num = bn_binding_array_length_quick(bindings);
    if (num == 0) {
	/* this must not happened */
	*speed = 0;
	goto bail;
    }
    for ( i = 0; i < num; i++) {
	binding = bn_binding_array_get_quick(bindings, i);
	if (binding == NULL) {
	    continue;
	}
	binding_name = bn_binding_get_name_quick(binding);
	if (binding_name == NULL) {
	    continue;
	}
	if (bn_binding_name_pattern_match(ts_str(binding_name),
		    "/net/bonding/config/*/name")) {
	    err = bn_binding_get_tstring_const(binding, ba_value,
		    bt_string, NULL, &bond_name);
	    bail_error(err);

	    if (0 == strcmp(ts_str(bond_name), ifname)) {
		/* we found the interface */
		found  = 1;
		break;
	    }
	}
    }
    if (found) {
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	bond_id =  tstr_array_get_str_quick(name_parts, 3);
	bail_null(bond_id);
	snprintf(node_name, sizeof(node_name), 
		"/net/bonding/config/%s/interface", bond_id);
	err = mdc_get_binding_children(sn_get_admin_mcc(), NULL, NULL, false, 
		&slave_interfaces, true, true, node_name);
	bail_error(err);
	bn_binding_array_dump("BOND-SLAVE", slave_interfaces, LOG_DEBUG);

	snprintf(node_name, sizeof(node_name),
		"/net/bonding/config/%s/mode", bond_id);
	err = mdc_get_binding_tstr(sn_get_admin_mcc(), NULL, NULL, NULL, 
		&bond_mode, node_name, NULL);
	bail_error_null(err, bond_mode);
	
	err = snmp_get_bond_speed(ts_str(bond_mode), slave_interfaces,
		&bond_speed);
	bail_error(err);
	*speed = bond_speed;
    }

bail:
    ts_free(&bond_mode);
    tstr_array_free(&name_parts);
    bn_binding_array_free(&slave_interfaces);
    bn_binding_array_free(&bindings);
    return err;

}
int
snmp_get_bond_speed(const char *mode, bn_binding_array *interfaces,
	u_long *ifspeed)
{
    int aggregate = 0, num = 0 , i =0, err= 0;
    const bn_binding *binding = NULL;
    char *ifname = NULL;
    u_long intf_speed = 0;


    if (0 == strcmp(mode, "link-agg-layer3+4")) {
	aggregate = 1;
    }
    /* all others are  maximum */

    *ifspeed = 0;

    num = bn_binding_array_length_quick(interfaces);
    for (i = 0; i < num; i++) {
	binding = bn_binding_array_get_quick(interfaces, i);
	if (binding == NULL) {
	    continue;
	}
	safe_free(ifname);
	err = bn_binding_get_str(binding, ba_value, bt_string, 0, &ifname);
	bail_error(err);
	err = snmp_get_physical_intf_speed(ifname, &intf_speed);
	bail_error(err);

	lc_log_debug(LOG_DEBUG, "if: %s, speed: %ld + %ld", 
		ifname, *ifspeed, intf_speed);
	if (aggregate) {
	    *ifspeed +=intf_speed;
	} else {
	    *ifspeed = max(*ifspeed, intf_speed);
	}
    }

bail:
    lc_log_debug(LOG_DEBUG, "total : %ld", *ifspeed);
    safe_free(ifname);
    return err;

}


/*
 *
 * Filename:  libnknmgmt.c
 * Date:      2010/10/26
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include "md_client.h"
#include <proc_utils.h>
#include <string.h>
#include "bnode.h"
#include "libnknmgmt.h"
#include "nkn_memalloc.h"
#include "nkn_defs.h"
#include "nkn_mgmt_log.h"
#include "file_utils.h"
#include "nkn_mgmt_defs.h"

/* static declarations */
static int
get_node_last_part(const char *full_node,
		tstring **last_name);

static tbool
is_deliv_intf(const char *intf_node,
	    md_client_context *client_mcc ,
		service_type_t service_type);

static uint64_t
nvsd_mgmt_get_aggr_bond_ifspeed(const char *bond_index,
                                        const bn_binding_array *bindings,
					 md_client_context *client_mcc);

static int
nvsd_mgmt_get_bond_ifname(bn_binding_array *bindings,
                                        const char *bind_cfg_index,
                                        char **intf_name);

static tbool
is_internal_intf(const char *name);


tbool is_bound_to_a_bond(const char *intf_name, char **bond_intf_name,
			    md_client_context *client_mcc)
{
        int err = 0;
        tstr_array *child_intf = NULL;
        uint32_t i,j;
        const char *child_intf_name = NULL;
        bn_binding_array *bindings = NULL;
        tstr_array *t_index = NULL;
        tbool found = false;
        char *intf_node = NULL;

        bail_null(intf_name);

        err = mdc_get_binding_children(client_mcc,
                        NULL, NULL,true,
                        &bindings, true, true, "/net/bonding/config");
        bail_error(err);

        if(bindings) {
                /* get the list of child interface */
                err = bn_binding_array_get_name_part_tstr_array(bindings,
                                "/net/bonding/config/*/interface/*",
                                5, &child_intf);
                bail_error(err);

                if(child_intf) {
                        for( i = 0; i < tstr_array_length_quick(child_intf); i++){
                                child_intf_name =  tstr_array_get_str_quick(child_intf, i);
                                bail_null(child_intf_name);

                                /* check to see if the child interface matches with the one provided
                                 * in the argument */
                                if(strcmp(child_intf_name, intf_name) == 0) {
                                        found = true;

                                        /* Find the Bond intf index */
                                        intf_node = smprintf("/net/bonding/config/*/interface/%s",
                                                                                child_intf_name);
                                        bail_null(intf_node);

                                        err = bn_binding_array_get_name_part_tstr_array(bindings,
                                                        intf_node,3,&t_index);
                                        bail_error(err);

                                        for( j = 0; j < tstr_array_length_quick(t_index); j++) {

                                                /*Given the bond intf index get the bond intf name */
                                                err = nvsd_mgmt_get_bond_ifname(bindings,
                                                                tstr_array_get_str_quick(t_index, j),
                                                                        bond_intf_name);
                                                bail_error_null(err, bond_intf_name);
                                                goto bail;
                                        }
                                }
                        }
                }
        }
bail:
        bn_binding_array_free(&bindings);
        safe_free(intf_node);
        tstr_array_free(&t_index);
        tstr_array_free(&child_intf);
        return found;
}

static int nvsd_mgmt_get_bond_ifname(bn_binding_array *bindings,
                                        const char *bind_cfg_index,
                                        char **intf_name)
{
        int err = 0;
        tstring *t_intf_name = NULL;

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL,
                        &t_intf_name,
                        "/net/bonding/config/%s/name", bind_cfg_index);
        bail_error_null(err, t_intf_name);

        *intf_name = strdup(ts_str(t_intf_name));
bail:
        ts_free(&t_intf_name);
        return err;

}


int64_t nvsd_mgmt_get_bond_intf_bw(const char *if_name,
				md_client_context *client_mcc)
{
        int err = 0;
        tstr_array *bond_cfg = NULL;
        uint32_t i;
        const char *bond_index = NULL;
        tstring *t_ifname = NULL;
        bn_binding_array *bindings = NULL;
        int64_t bw_Bytes;

        /* get the bindings under /net/bonding/config/
         *  Get all the interfaces name
         *  which is /net/bonding/config/<if>/name
         *  If the name matches,Get the interfaces bound to it
         *  /net/bonding/config/<if>/interface
         *  Get IFSPEED fro each of the child interfaces
         *  Sum it and return
         */

        /* Decide the bond interface BW based on the bonding mode*/

        err = mdc_get_binding_children(client_mcc,
                        NULL, NULL,true,
                        &bindings, true, true, "/net/bonding/config");
        bail_error(err);

        if(bindings) {
                err = bn_binding_array_get_name_part_tstr_array(bindings,
                                        "/net/bonding/config/*/name",
                                        3, &bond_cfg);
                bail_error_null(err, bond_cfg);

                for( i = 0; i < tstr_array_length_quick(bond_cfg); i++){
                        bond_index =  tstr_array_get_str_quick(bond_cfg, i);
                        bail_null(bond_index);

                        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                                        NULL, &t_ifname,
                                        "/net/bonding/config/%s/name",
                                        bond_index);
                        bail_error_null(err, t_ifname);

                        if(strcmp(ts_str(t_ifname),if_name) == 0) {
                                /* Interface matches */
                                /* Get the ifspeed for the bond interface */
                                bw_Bytes = nvsd_mgmt_get_aggr_bond_ifspeed(bond_index,
                                                                        bindings,client_mcc);

                                bw_Bytes = (GBYTES/8) * (bw_Bytes / 1000);
                        }
                }
        }
bail:
        bn_binding_array_free(&bindings);
        tstr_array_free(&bond_cfg);
        ts_free(&t_ifname);
        return bw_Bytes;
}

/* Calculate the speed of bond interface
 * goind through the child interfaces
 * getting the ifspeed
 */
static uint64_t nvsd_mgmt_get_aggr_bond_ifspeed(const char *bond_index,
                                        const bn_binding_array *bindings, md_client_context *client_mcc)
{

        int err = 0;
        tstr_array *slave_intf_list = NULL;
        const char *slave_intf = NULL;
        uint64_t bw = 0;
        uint32_t i = 0;
        char *intf_node = NULL;
        tstring *t_bonding_type = NULL;

        intf_node = smprintf("/net/bonding/config/%s/interface/*",bond_index);
        bail_null(intf_node);

        err = bn_binding_array_get_name_part_tstr_array(bindings,intf_node,5,
                        &slave_intf_list);
        bail_error_null(err, slave_intf_list);

        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                        NULL, &t_bonding_type,
                        "/net/bonding/config/%s/mode",
                        bond_index);
        bail_error_null(err, t_bonding_type);

        /*Get the bond mode and determine how to decide the BW for the bond */
	/* not handling active-backup bond mode as TM doesnt support it */
        if((!strcmp(ts_str(t_bonding_type), "balance-rr")) ||
                        (!strcmp(ts_str(t_bonding_type), "balance-xor")) ||
                        (!strcmp(ts_str(t_bonding_type), "balance-xor-layer3+4")) ||
                        (!strcmp(ts_str(t_bonding_type), "broadcast")) ||
                        (!strcmp(ts_str(t_bonding_type), "link-agg")) ||
                        (!strcmp(ts_str(t_bonding_type), "link-agg-layer3+4")) ||
                        (!strcmp(ts_str(t_bonding_type), "balance-alb")) ||
                        (!strcmp(ts_str(t_bonding_type), "balance-tlb")) ) {
                for( i = 0; i < tstr_array_length_quick(slave_intf_list); i++){

                        slave_intf = tstr_array_get_str_quick(slave_intf_list, i);
                        bw += nvsd_mgmt_get_interface_link_speed(slave_intf, client_mcc);
                }
        }
bail:
        safe_free(intf_node);
        tstr_array_free(&slave_intf_list);
        return bw;

}

uint64_t
nvsd_mgmt_get_interface_link_speed(const char *cp_intf_name,
				    md_client_context *client_mcc)
{
        int err = 0;
        uint64_t if_speed = 1000; /* DEFAULT : 1000Mb/s  */
        char *node_name = NULL;
        char  *t_linkspeed = NULL;
        bn_binding *linkspeed_binding = NULL;
        tbool link_up = false;

        /* Sanity check */
        bail_null(cp_intf_name);

        /* check to see if the link state is up */
        node_name = smprintf ("/net/interface/state/%s/flags/link_up", cp_intf_name);
        err = mdc_get_binding_tbool(client_mcc, NULL, NULL, false, &link_up,
                        node_name, NULL);
	safe_free(node_name);
        bail_error(err);

        if(!link_up) {
            if_speed = 0;
            goto bail;
        }

        /* Now create the state node to get the link speed binding */
        node_name = smprintf ("/net/interface/state/%s/speed", cp_intf_name);
        err = mdc_get_binding(client_mcc, NULL, NULL, false, &linkspeed_binding,
                                          node_name, NULL);
        bail_error(err);

        /* Now get the link speed string */
        err = bn_binding_get_str(linkspeed_binding,
                            ba_value, bt_string, NULL,
                            &t_linkspeed);
        bail_error_null(err, t_linkspeed);

        /* Parse the link speed from the string */
        sscanf(t_linkspeed, "%luMb/s", &if_speed);

        /* Setting a floor of 1000Mbps */
        if (if_speed < 1000)
                if_speed = 1000;

bail:
        safe_free(node_name);
        safe_free(t_linkspeed);
        bn_binding_free(&linkspeed_binding);
        return (if_speed);

} /* end of nvsd_mgmt_get_interface_link_speed() */



tstr_array* nvsd_mgmt_get_alias_intf(const char *intfname,
				   md_client_context *client_mcc)
{
        int err = 0;
        bn_binding_array *bindings = NULL;
        tstring *t_intf_name = NULL;
        tstr_array *ip_addr_list = NULL;
        tstr_array *alias_intf_names = NULL;
        const char* ip_addr = NULL;
        uint32_t i = 0;
        char *node_name = NULL;

        err = tstr_array_new(&alias_intf_names, NULL);
        bail_error(err);

        node_name = smprintf("/net/interface/address/state/ifdevname/%s",intfname);
        bail_null(node_name);

        err = mdc_get_binding_children(client_mcc,
                        NULL, NULL,true,
                        &bindings, true, true, node_name);
        bail_error(err);

        if(bindings) {
                safe_free(node_name);
                node_name = smprintf("/net/interface/address/state/ifdevname/%s/ipv4addr/*/ifname", intfname);
                bail_null(node_name);

                err = bn_binding_array_get_name_part_tstr_array(bindings,
                                node_name,
                                7, &ip_addr_list);
                bail_error_null(err, ip_addr_list);

                for( i = 0; i < tstr_array_length_quick(ip_addr_list); i++){
                        ip_addr =  tstr_array_get_str_quick(ip_addr_list, i);
                        bail_null(ip_addr);

                        err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                                        NULL, &t_intf_name,
                                        "/net/interface/address/state/ifdevname/%s/ipv4addr/%s/ifname",
                                        intfname,ip_addr);
                        bail_error_null(err, t_intf_name);

                        if(!ts_equal_str(t_intf_name, intfname ,false)) {
                                err = tstr_array_append_str (alias_intf_names, ts_str(t_intf_name));
                                bail_error(err);
                        }
                }

        }


bail:
        bn_binding_array_free(&bindings);
        safe_free(node_name);
        ts_free(&t_intf_name);
        tstr_array_free(&ip_addr_list);
        //Not freeing alias_intf_names
        return alias_intf_names;

}


/* Function to determine if an interface is a bond interface */
tbool nvsd_mgmt_is_bond_intf(const char *intf_name,
				md_client_context *client_mcc )
{
        int err = 0;
        tstring *t_intf_type = NULL;
        tbool is_bond_intf = false;

        err = mdc_get_binding_tstr_fmt(client_mcc, NULL, NULL, NULL,
                        &t_intf_type, NULL, "/net/interface/state/%s/devsource",intf_name);
        bail_error(err);

        if(t_intf_type && (strcmp(ts_str(t_intf_type), "bond") == 0))
                is_bond_intf = true;
bail:
        ts_free(&t_intf_type);
        return is_bond_intf;
}

int
get_tx_bw_total_bps (uint64_t *tx_bw_total_bps,
			md_client_context *client_mcc ,
			service_type_t serv_type)
{
	uint16 i;
	int err = 0;
	uint64 aggr_bw = 0;
	tstring *t_speed = NULL;
	tstring *t_speed_str = NULL;
	const char *binding_name = NULL;
	tstr_array *interfaces_list = NULL;
	char *link_node = NULL;
	char *speed_node = NULL;
	tbool link_up = false;


	/* Sanity test */
	if (!tx_bw_total_bps)
	{
		err = 1;
		goto bail;
	}

	/* Get the list of interfaces */
	err = mdc_get_binding_children_tstr_array(client_mcc, NULL,
			NULL, &interfaces_list,
			"/net/interface/state", NULL);
	bail_error_null(err, interfaces_list);

	/* Loop through the interfaces to total the speed */
	for( i = 0; i < tstr_array_length_quick(interfaces_list) ; i++)
	{

		const char* intf_name = NULL;
		char *bond_intf_name = NULL;

		/* Get the binding name first */
		intf_name = tstr_array_get_str_quick(interfaces_list, i);

		link_node = smprintf("/net/interface/state/%s/flags/link_up",
					intf_name);
		bail_null(link_node);

		err = mdc_get_binding_tbool(client_mcc, NULL, NULL,
				NULL, &link_up, link_node, NULL);
		bail_error(err);

		safe_free(link_node);
		if(!link_up)
			continue;

		/* if its bound to a bond interface,
		 * dont account it again,as its already accounted
		 * with the bond interface BW
		 */
		if(is_bound_to_a_bond(intf_name, &bond_intf_name , client_mcc)) {
			safe_free(bond_intf_name);
			continue;
		}

		/*  Check if the intreface is a delivery interface */
		if(intf_name && is_deliv_intf(intf_name, client_mcc, serv_type)) {
			int32 ret_offset = 0;
			if(nvsd_mgmt_is_bond_intf(intf_name, client_mcc)) {
				uint64_t bw_Bps = nvsd_mgmt_get_bond_intf_bw(intf_name, client_mcc);
				uint64_t Mbps = (bw_Bps * 8)/ (1000 * 1000);

				aggr_bw += Mbps;
			} else {

				/* Create the interface speed monitoring node */
				speed_node = smprintf("/net/interface/state/%s/speed", 
						intf_name);
				bail_null(speed_node);

				/* Get the binding value */
				err = mdc_get_binding_tstr(client_mcc, NULL,
						NULL, NULL, &t_speed, speed_node, NULL);
				bail_error_null(err, t_speed);

				err = ts_find_str(t_speed, 0, "Mb/s", &ret_offset);
				bail_error(err);

				if(ret_offset > 0) {
					err = ts_substr(t_speed, 0, ret_offset,
							&t_speed_str);
					bail_error_null(err, t_speed_str);
					aggr_bw += strtol(ts_str(t_speed_str), NULL, 10);
				}
			}
					/* Free the allocated strings */
		}
		safe_free(speed_node);
		ts_free(&t_speed);
		ts_free(&t_speed_str);
	} // for 

	//For Total Service BW convert Mb/s into b/s
	aggr_bw = aggr_bw * 1000 * 1000;
	*tx_bw_total_bps = aggr_bw;
bail:
	safe_free(speed_node);
	ts_free(&t_speed);
	ts_free(&t_speed_str);
	tstr_array_free(&interfaces_list);
	return err;
}

static tbool
is_internal_intf(const char *name)
{

	if(!strcmp(name, "lo") || !strcmp(name, "sit0") || !strncmp(name, "virbr", 5)) {
		return true;
	}
	return false;
}

static tbool
is_deliv_intf(const char *intf_node,
	    md_client_context *client_mcc ,
	    service_type_t service_type)
{
	int err = 0;
	tstr_array *http_listen_intf = NULL;
	tstr_array *rtsp_listen_intf = NULL;
	tbool found = false;
	const char *interface_name = NULL;
	uint32 i = 0;
	tstring *t_intf_name = NULL;

	/* Dont consider loopback and sit0 interfaces*/
	if(is_internal_intf(intf_node)) {
		goto bail;
	}

	/* look for http delivery interfaces */
	if(service_type == SERVICE_INTF_HTTP || service_type ==
			SERVICE_INTF_ALL) {
		err = mdc_get_binding_children_tstr_array(client_mcc, NULL,
				NULL, &http_listen_intf,
				"/nkn/nvsd/http/config/interface", NULL);
		bail_error_null(err, http_listen_intf);

		/* if the array is empty then all interface
		are to be used as service interfaces */
		if(!tstr_array_length_quick(http_listen_intf)) {
			found = true;
			goto bail;
		}

		for(i = 0; i < tstr_array_length_quick(http_listen_intf) ; i++ ) {
			const char *last_name_part = NULL;
			interface_name = tstr_array_get_str_quick(http_listen_intf, i);
			err = get_node_last_part(interface_name, &t_intf_name);
			bail_error_null(err, t_intf_name);

			last_name_part = ts_str_maybe(t_intf_name);

			if(last_name_part && !strcmp(last_name_part,intf_node)) {
				found = true;
				goto bail;
			}
			ts_free(&t_intf_name);
		}
	}
	/* check if interface is part of rtsp delivery*/
	if(service_type == SERVICE_INTF_RTSP || service_type ==
			SERVICE_INTF_ALL) {

		err = mdc_get_binding_children_tstr_array(client_mcc, NULL, 
				NULL, &rtsp_listen_intf, 
				"/nkn/nvsd/rtstream/config/interface", NULL);
		bail_error_null(err, rtsp_listen_intf);

		if(!tstr_array_length_quick(rtsp_listen_intf)) {
			found = true;
			goto bail;
		}

		for(i = 0; i < tstr_array_length_quick(rtsp_listen_intf) ; i++ ) {
			const char *last_name_part = NULL;
			interface_name = tstr_array_get_str_quick(rtsp_listen_intf, i);

			err = get_node_last_part(interface_name, &t_intf_name);
			bail_error_null(err, t_intf_name);

			last_name_part = ts_str_maybe(t_intf_name);
			if(last_name_part && !strcmp(last_name_part,intf_node)) {
				found = true;
				goto bail;
			}
			ts_free(&t_intf_name);
		}
	}
bail:
	ts_free(&t_intf_name);
	tstr_array_free(&http_listen_intf);
	tstr_array_free(&rtsp_listen_intf);
	return found;

}

static int
get_node_last_part(const char *full_node, 
			tstring **last_name)
{
	int err = 0;
	tstr_array *name_parts = NULL;
	uint32 num_parts = 0;
	const char *last_name_part = NULL;

	bail_null(full_node);

	err = ts_tokenize_str(full_node, '/', '\0', '\0', 0, &name_parts);
	bail_error(err);

	num_parts = tstr_array_length_quick(name_parts);
	last_name_part = tstr_array_get_str_quick (name_parts, num_parts - 1);

	err = ts_new_str(last_name, last_name_part);
	bail_error(err);
bail:
	tstr_array_free(&name_parts);
	//Not freeing last_name as the called will free it.
	return err;

}

int
mgmt_log_init(md_client_context *mcc, const char *daemon_log_node)
{
    int err = 0;
    bn_binding *binding = NULL;
    const char *global_log_node = "/nkn/debug/log/all/log_level";
    uint8 global_log_level = 0, global_node_present = 0,
	  daemon_log_level = 0, daemon_node_present = 0;

    err = mdc_get_binding(mcc, NULL, NULL, true, &binding, global_log_node, NULL);
    bail_error(err);

    if (binding) {
	err = bn_binding_get_uint8(binding, ba_value, NULL, &global_log_level);
	bail_error(err);
	global_node_present = 1;
    }

    if (NULL != daemon_log_node) {
	bn_binding_free(&binding);
	err = mdc_get_binding(mcc, NULL, NULL, true, &binding, daemon_log_node, NULL);
	bail_error(err);

	if (binding) {
	    err = bn_binding_get_uint8(binding, ba_value, NULL, &daemon_log_level);
	    bail_error(err);
	    daemon_node_present = 1;
	}
    }
    if (global_node_present) {
	set_log_level(global_log_level);
    }

    if (daemon_node_present) {
	set_log_level(daemon_log_level);
    }

bail:
    bn_binding_free(&binding);
    return err;
}

int 
mgmt_log_cfg_handle_change(bn_binding_array *old_bindings,
	bn_binding_array *new_bindings, const char *daemon_log_node)
{
    int err = 0;
    const bn_binding *binding = NULL;
    const char *global_log_node = "/nkn/debug/log/all/level";
    uint8 global_log_level = 0, daemon_log_level = 0; 

    UNUSED_ARGUMENT(old_bindings);

    err = bn_binding_array_get_binding_by_name(new_bindings, global_log_node, &binding);
    bail_error(err);

    if (binding) {
	err = bn_binding_get_uint8(binding, ba_value, NULL, &global_log_level);
	bail_error(err);
	set_log_level(global_log_level);
    }

    if (NULL != daemon_log_node) {
	err = bn_binding_array_get_binding_by_name(new_bindings, daemon_log_node, &binding);
	bail_error(err);

	if (binding) {
	    err = bn_binding_get_uint8(binding, ba_value, NULL, &daemon_log_level);
	    bail_error(err);
    	    set_log_level(daemon_log_level);
	}
    }

bail:
    return err;
}

tbool
is_pacifica_blade(void) {
    int err = 0;
    char *buff = NULL;
    uint32 len = 0;
    tstr_array *args = NULL;
    uint32 found_idx = 0;
    tbool ret = false;

    err = lf_read_file("/proc/cmdline", &buff, &len);
    bail_error(err);

    err = ts_tokenize_str(buff, ' ', '\\', '"', 0, &args);
    bail_error(err);

    err = tstr_array_linear_search_str(args, "model=pacifica", 0, &found_idx);
    if(err == lc_err_not_found) {
	ret = false;
    } else if ( (err == 0) && (found_idx > 0)) {
	ret = true;
    }

    lc_log_basic(LOG_INFO, "is_pacifica = %d", ret);
bail:
    safe_free(buff);
    tstr_array_free(&args);
    return ret;

}

int
set_gpio_led(const char *led, int state)
{
    str_value_t proc_file_str = {0};
    str_value_t led_state = {0};
    tbool proc_exists = false;
    int err = 0;

    snprintf(proc_file_str, sizeof(proc_file_str), "/proc/mxc-gpio/%s",
						    led);

    err = lf_test_exists (proc_file_str, &proc_exists);

    if(!proc_exists) {
	lc_log_basic(LOG_INFO, "Invalid proc file,%s", proc_file_str);
	goto bail;
    }

    snprintf(led_state, sizeof(led_state), "%d", state);

    lc_log_basic(LOG_INFO, "Settung the led through %s,value = %s",
		    proc_file_str, led_state);
    err = lf_write_file(proc_file_str, 0600, led_state, true, 1);
    bail_error(err);

bail:
    return err;

}

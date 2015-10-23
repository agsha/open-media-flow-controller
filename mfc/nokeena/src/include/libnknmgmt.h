/*
 *
 * Filename:  libnkncli.h
 * Date:      2010/10/25 
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2010-11 Juniper Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef __LIBNKNMGMT__H
#define __LIBNKNMGMT__H

typedef enum {
    SERVICE_INTF_ALL,
    SERVICE_INTF_HTTP,
    SERVICE_INTF_RTSP
} service_type_t;

#define APP_LED		"app-led"
#define CPU_LED		"cpu-led"


#define GPIO_GREEN	3
#define GPIO_ORANGE	2
#define GPIO_RED	1

int
get_tx_bw_total_bps (uint64_t *tx_bw_total_bps, md_client_context *client_mcc, service_type_t service_type);

int64_t
nvsd_mgmt_get_bond_intf_bw(const char *if_name,
				md_client_context *client_mcc);

uint64_t
nvsd_mgmt_get_interface_link_speed(const char *cp_intf_name,
				    md_client_context *client_mcc);

tbool
is_bound_to_a_bond(const char *intf_name, char **bond_intf_name,
			    md_client_context *client_mcc);

tstr_array*
nvsd_mgmt_get_alias_intf(const char *intfname,
				   md_client_context *client_mcc);

tbool
nvsd_mgmt_is_bond_intf(const char *intf_name,
				md_client_context *client_mcc );

int mgmt_log_init(md_client_context *mcc, const char *daemon_log_node);
int
mgmt_log_cfg_handle_change(bn_binding_array *old_bindings,
	bn_binding_array *new_bindings,
	const char *daemon_log_node);

tbool is_pacifica_blade(void); 

int
set_gpio_led(const char *led, int state);

#endif /* ndef __LIBNKNMGMT__H */

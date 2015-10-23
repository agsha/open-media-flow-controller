#include "md_client.h"
#include "nkn_cntr_api.h"
#include "nkn_mgmt_defs.h"
#include "sn_mod_reg.h"

int snmp_ns_get_names(tstr_array **);
int snmp_dc_get_disks( tstr_array **);
int snmp_get_interface_names(tstr_array **ret_names);
int snmp_get_interface_names(tstr_array **ret_names);
int snmp_get_if_speed(const char *ifname, u_long *if_speed);

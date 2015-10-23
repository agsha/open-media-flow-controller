#ifndef __NVSD_MGMT__H
#define __NVSD_MGMT__H

/*
 *
 * Filename:  nvsd_mgmt.h
 * Date:      2008/11/26 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Include Files */
#include <glib.h>

/* Samara Headers */
#include "md_client.h"
#include "mdc_wrapper.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"

/* Other Headers */
#include "nvsd_mgmt_api.h"

/* Types */
typedef enum nvsd_mgmt_actions_e
{  
	NVSD_DC_PROBE_NEWDISK = 0,
	NVSD_DC_ACTIVATE,
	NVSD_DC_CACHEABLE,
	NVSD_DC_FORMAT,
	NVSD_DC_FORMAT_RESULT,
	NVSD_DC_REPAIR,
	NVSD_DC_FORMAT_BLOCKS,
	
	NVSD_VP_FS_URI_QUERY,
	NVSD_VP_FS_TIME,
	NVSD_VP_FS_SIZE,
	NVSD_VP_AF_URI_QUERY,
	NVSD_VP_AF_AUTO,
	NVSD_VP_AF_RATE,

	NVSD_NS_STAT_URI,
	NVSD_NS_DELETE_URI
} nvsd_mgmt_actions_t;

typedef enum mgmt_disk_tier_en {
    MGMT_TIER_SATA,
    MGMT_TIER_SAS,
    MGMT_TIER_SSD,

    MGMT_TIER_MAX,
}mgmt_disk_tier_t;

typedef struct if_info_state_change {
#define INTF_LINK_UP 1
#define INTF_LINK_DOWN 2
#define INTF_LINK_CHNG 3
#define DELIVERY_ADD 4
#define DELIVERY_DEL 5
	uint8 chng_type;
    char intfname[128];
    char cause[256];
	char ipv4addr[128];
	uint8 ipv6_enable;
	char ipv6addr[256];
	char intfsource[128];
    uint8 ipv4_mask_len;
    uint8 ipv6_mask_len;
	uint8 oper_up;
	uint8 link_up;
} if_info_state_change_t;


/* Extern Function Prototypes */
extern void catcher(int sig);
extern int nvsd_mgmt_init(void);
extern int nvsd_mgmt_deinit(void);
extern int nvsd_system_cfg_query(const bn_binding_array *bindings);
extern int nvsd_network_cfg_query(const bn_binding_array *bindings);
extern int nvsd_http_cfg_query(const bn_binding_array *bindings);
extern int nvsd_rtstream_cfg_query(const bn_binding_array *bindings);
extern int nvsd_buffermgr_cfg_query(const bn_binding_array *bindings);
extern int nvsd_diskcache_cfg_query(void);
extern int nvsd_originmgr_cfg_query(const bn_binding_array *bindings);
extern int nvsd_vpe_cfg_query(void);
extern int nvsd_fmgr_cfg_query(const bn_binding_array *bindings);
extern int nvsd_tfm_cfg_query(const bn_binding_array *bindings);
extern int nvsd_am_cfg_query(const bn_binding_array *bindings);
extern int nvsd_mm_cfg_query(const bn_binding_array *bindings);
extern int nvsd_namespace_cfg_query(const bn_binding_array *bindings);
extern int nvsd_virtual_player_cfg_query(const bn_binding_array *bindings);
extern int nvsd_server_map_cfg_query(const bn_binding_array *bindings);
extern int nvsd_pub_point_cfg_query(void);
extern int nvsd_cluster_cfg_query(const bn_binding_array *bindings);
extern int nvsd_resource_mgr_cfg_query(const bn_binding_array *bindings);
extern int nvsd_acclog_cfg_query(const bn_binding_array *bindings);
extern int nvsd_errlog_cfg_query(const bn_binding_array *bindings);
extern int nvsd_strlog_cfg_query(const bn_binding_array *bindings);
extern int nvsd_disk_mgmt_init(void);
extern int nvsd_disk_mgmt_deinit(void);
extern int nvsd_l4proxy_cfg_query(const bn_binding_array *bindings);
extern int nvsd_pe_srvr_cfg_query(const bn_binding_array *bindings);
extern int
nvsd_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                  void *data, bn_binding_array *resp_bindings);
extern int
nvsd_mon_handle_iterate(const char *bname, const tstr_array *bname_parts,
                      void *data, tstr_array *resp_names);
extern int
nvsd_disk_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                  void *data, bn_binding_array *resp_bindings);
extern int
nvsd_mgmt_update_node_bool(const char* cpNode, tbool new_value);
extern int
nvsd_mgmt_update_node_uint32(const char* cpNode, uint32 new_value);
extern int
nvsd_mgmt_update_node_str(const char* cpNode, const char* new_value, bn_type binding_type);
extern int
nvsd_disk_mgmt_update_node_bool(const char* cpNode, tbool new_value);
extern int
nvsd_disk_mgmt_update_node_uint32(const char* cpNode, uint32 new_value);
extern int
nvsd_disk_mgmt_update_node_str(const char* cpNode, const char* new_value, bn_type binding_type);
extern int
nvsd_interface_up_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data);
extern int get_other_interface_details(if_info_state_change_t *interface_change_info);

extern tbool disk_is_formatable (const char* t_diskid);
extern char* get_diskid_from_diskname(const char *t_diskname);
extern const char* get_diskname_from_diskid(const char *t_diskid);
extern const char* get_type_from_diskid(const char *t_diskid);
extern const char* get_tier_from_diskid(const char *t_diskid);
extern const char* get_state_from_diskid(const char *t_diskid);
extern int get_all_disknames(tstr_array *resp_names, int which_data);
extern int nvsd_mgmt_diskcache_action_hdlr (const char *t_diskname, 
		nvsd_mgmt_actions_t en_action, tbool t_value, char *ret_msg);
extern char* nvsd_mgmt_show_watermark_action_hdlr(void);
extern int nvsd_mgmt_virtual_player_action_hdlr (nvsd_mgmt_actions_t en_action, 
		const char *cpname, const char *cpvalue);
extern int nsvd_mgmt_ns_get_ramcache_list (const char *cp_namespace,
				const char *cp_uid, const char *cp_filename);
extern tbool addable_ns_uid(const char* uid2chk, const char* lst_nsuids);
extern tbool configured_ns_uid(const char* uid2chk);
extern int nvsd_mgmt_diskcache_action_pre_read_stop(void);
extern int nvsd_ns_reset_counter_values(void);

#endif // __NVSD_MGMT__H

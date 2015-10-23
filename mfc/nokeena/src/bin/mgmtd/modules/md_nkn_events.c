/*
 *
 * Filename:  md_nkn_if_events.c
 * Date:      2009-05-19
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */


#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "mdm_events.h"
#include "tpaths.h"
#include "nkn_mgmt_defs.h"

#include <linux/if.h>
#include <fcntl.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>

typedef enum nkn_ethtool_cmd {
    NKN_ETHTOOL_GRO_SET = 1,
    NKN_ETHTOOL_GRO_GET,
    NKN_ETHTOOL_GSO_SET,
    NKN_ETHTOOL_GSO_GET,
    NKN_ETHTOOL_TSO_SET,
    NKN_ETHTOOL_TSO_GET
} nkn_ethtool_cmd_t;

static const char arp_announce_node[] = "/net/interface/config/*/arp/announce";
static const char arp_ignore_node[] = "/net/interface/config/*/arp/ignore";

static md_upgrade_rule_array *md_nkn_if_events_ug_rules= NULL;
static int md_nkn_proc_ctl_int(char *proc_file, int value);


static int md_nkn_get_proc_intf_conf ( md_commit *commit, const mdb_db *db, const char *node_name,
		const bn_attrib_array *node_attribs, bn_binding **ret_binding, uint32 *ret_node_flags, void *arg);

int md_nkn_events_init(const lc_dso_info *info, void *data);
/*
 * Function handlers for external functions from md_if_state.c"
 */
static int md_ifs_state_get_interfaces(md_commit *commit, const mdb_db *db, const char *node_name,
	const bn_attrib_array *node_attribs,  bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg);

static int md_ifs_state_iterate_interfaces(md_commit *commit, const mdb_db *db,
	const char *parent_node_name, const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg);


static int
md_nkn_get_offloading
		(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nkn_if_handle_state_change_event(md_commit *commit, const mdb_db *db,
                                    const char *event_name,
                                    const bn_binding_array *bindings,
                                    void *cb_reg_arg, void *cb_arg);

int
md_nkn_ethtool(const char *intf_name,
			nkn_ethtool_cmd_t cmd, uint32_t *data);

static int
md_nkn_network_offloading_chk_func(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs, void
		*arg);

static int md_nkn_events_commit_side_effects(md_commit *commit,
                                            const mdb_db *old_db,
                                            mdb_db *inout_new_db,
                                            mdb_db_change_array *change_list,
                                            void *arg);

static int
md_nkn_events_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg);


static int
md_nkn_events_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    tstring *val = NULL;
    tstring *source  = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "net", "interface", "config", "*", "offloading", "gro", "enable"))
		&& (mdct_modify == change->mdc_change_type || mdct_add == change->mdc_change_type))
	{
	    const char *intf_name = NULL;
	    const bn_attrib *new_val = NULL;
	    uint32_t gro_toggle = 0;
	    ts_free(&source);
	    node_name_t intf_nd = {0};
	    tbool intf_valid = false;

	    ts_free(&val);
	    intf_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(intf_name);

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
		    NULL, &val);
	    bail_error_null(err, val);

	    if (ts_equal_str(val , "true", false)) {
		gro_toggle = 1;
	    } else {
		gro_toggle = 0;

	    }
	    snprintf(intf_nd, sizeof(intf_nd),
		    "/net/interface/state/%s/devsource", intf_name);
	    err = mdb_get_node_value_tstr(commit, new_db, intf_nd, 0,
		    &intf_valid, &source);
	    bail_error(err);
	    if(source  && ts_equal_str(source, "physical", false)) {
		err = md_nkn_ethtool(intf_name, NKN_ETHTOOL_GRO_SET,
			&gro_toggle);
		bail_error(err);
	    }
	    ts_free(&val);
	}

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "net", "interface", "config", "*", "offloading", "gso", "enable"))
		&& (mdct_modify == change->mdc_change_type || mdct_add == change->mdc_change_type))
	{
	    const char *intf_name = NULL;
	    const bn_attrib *new_val = NULL;
	    uint32_t gso_toggle = 0;
	    ts_free(&source);
	    node_name_t intf_nd = {0};
	    tbool intf_valid = false;

	    ts_free(&val);
	    intf_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(intf_name);

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
		    NULL, &val);
	    bail_error_null(err, val);

	    if (ts_equal_str(val , "true", false)) {
		gso_toggle = 1;
	    } else {
		gso_toggle = 0;

	    }
	    snprintf(intf_nd, sizeof(intf_nd),
		    "/net/interface/state/%s/devsource", intf_name);
	    err = mdb_get_node_value_tstr(commit, new_db, intf_nd, 0,
		    &intf_valid, &source);
	    bail_error(err);
	    if(source  && ts_equal_str(source, "physical", false)) {
		err = md_nkn_ethtool(intf_name, NKN_ETHTOOL_GSO_SET,
			&gso_toggle);
		bail_error(err);
	    }
	    ts_free(&val);
	}

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "net", "interface", "config", "*", "offloading", "tso", "enable"))
		&& (mdct_modify == change->mdc_change_type || mdct_add == change->mdc_change_type))
	{
	    const char *intf_name = NULL;
	    const bn_attrib *new_val = NULL;
	    uint32_t tso_toggle = 0;
	    ts_free(&source);
	    node_name_t intf_nd = {0};
	    tbool intf_valid = false;

	    ts_free(&val);
	    intf_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(intf_name);

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
		    NULL, &val);
	    bail_error_null(err, val);

	    if (ts_equal_str(val , "true", false)) {
		tso_toggle = 1;
	    } else {
		tso_toggle = 0;

	    }
	    snprintf(intf_nd, sizeof(intf_nd),
		    "/net/interface/state/%s/devsource", intf_name);
	    err = mdb_get_node_value_tstr(commit, new_db, intf_nd, 0,
		    &intf_valid, &source);
	    bail_error(err);
	    if(source  && ts_equal_str(source, "physical", false)) {
		err = md_nkn_ethtool(intf_name, NKN_ETHTOOL_TSO_SET,
			&tso_toggle);
		bail_error(err);
	    }
	    ts_free(&val);
	}
	if (bn_binding_name_pattern_match(ts_str(change->mdc_name), arp_announce_node)) {
	    const char *intf_name = NULL;
	    const bn_attrib *new_val = NULL;
	    uint32_t arp_announce = 0;
	    node_name_t intf_nd = {0};
	    node_name_t proc_file = {0};
	    tbool intf_valid = false;

	    ts_free(&source);
	    intf_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(intf_name);

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_uint32(new_val, NULL, NULL, &arp_announce);
	    bail_error(err);

	    snprintf(intf_nd, sizeof(intf_nd), "/net/interface/state/%s/devsource", intf_name);
	    err = mdb_get_node_value_tstr(commit, new_db, intf_nd, 0, &intf_valid, &source);
	    bail_error(err);
	    if (source && (ts_equal_str(source, "physical", false) || ts_equal_str(source, "bond", false))) {
		snprintf(proc_file, sizeof(proc_file), "/proc/sys/net/ipv4/conf/%s/arp_announce", intf_name);
		md_nkn_proc_ctl_int(proc_file, arp_announce);
	    }
	}

	if (bn_binding_name_pattern_match(ts_str(change->mdc_name), arp_ignore_node)) {
	    const char *intf_name = NULL;
	    const bn_attrib *new_val = NULL;
	    uint32_t arp_ignore = 0;
	    node_name_t intf_nd = {0};
	    node_name_t proc_file = {0};
	    tbool intf_valid = false;

	    ts_free(&source);
	    intf_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(intf_name);

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_uint32(new_val, NULL, NULL, &arp_ignore);
	    bail_error(err);

	    snprintf(intf_nd, sizeof(intf_nd), "/net/interface/state/%s/devsource", intf_name);
	    err = mdb_get_node_value_tstr(commit, new_db, intf_nd, 0, &intf_valid, &source);
	    bail_error(err);
	    if (source && (ts_equal_str(source, "physical", false) || ts_equal_str(source, "bond", false))) {
		snprintf(proc_file, sizeof(proc_file), "/proc/sys/net/ipv4/conf/%s/arp_ignore", intf_name);
		md_nkn_proc_ctl_int(proc_file, arp_ignore);
	    }
	}
    }

bail:
    ts_free(&val);
    ts_free(&source);
    return(err);
}

static int
md_nkn_network_offloading_chk_func(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs, void
		*arg)
{
    int err = 0;
    const bn_attrib *attrib = NULL;
    const char *intf_name = NULL;
    node_name_t intf_nd = {0};
    tbool intf_valid = false;
    tstring *interface = NULL;

    if (change_type == mdct_add || change_type == mdct_modify) {
	intf_name = tstr_array_get_str_quick(node_name_parts, 3);
	bail_null(intf_name);

	snprintf(intf_nd, sizeof(intf_nd),
		"/net/interface/config/%s", intf_name);

	err = mdb_get_node_value_tstr(commit, new_db, intf_nd, 0,
		&intf_valid, &interface);
	bail_error(err);

	if (!intf_valid) {
	    lc_log_basic(LOG_NOTICE, "Unknown Interface %s", intf_name);

	    err = md_commit_set_return_status_msg_fmt(commit, ENOENT,
		    "Unknown interface %s", intf_name);
	    bail_error(err);

	}
    }
bail:
    ts_free(&interface);
    return err;
}

#ifndef ETHTOOL_GFLAGS
#define ETHTOOL_GFLAGS      0x00000025 /* Get flags bitmap(ethtool_value) */
#endif // !ETHTOOL_GFLAGS
#ifndef ETHTOOL_SFLAGS
#define ETHTOOL_SFLAGS      0x00000026 /* Set flags bitmap(ethtool_value) */
#endif // !ETHTOOL_SFLAGS
#ifndef ETH_FLAG_LRO
#define ETH_FLAG_LRO        (1 << 15)
#endif // !ETH_FLAG_LRO
/* These are defined in linux/netdevice.h in kernel src */
#ifndef NETIF_F_GSO
#define NETIF_F_GSO (2048)
#endif
#ifndef NETIF_F_TSO
#define NETIF_F_TSO (1 << 16)
#endif

static int
md_nkn_ethtool_ioctl(const char *intf_name, struct ethtool_value *edata) {
    int efd = -1;
    int rv = -1;
    struct ifreq ifr;

    if ((efd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        lc_log_basic(LOG_ERR, "Failed to create ethtool handle: %s\n", strerror(errno));
        return rv;
    }

    strncpy(ifr.ifr_name, intf_name, sizeof(ifr.ifr_name));
    ifr.ifr_data = (char *)edata;

    rv = ioctl(efd, SIOCETHTOOL, (char*)&ifr);

    if (rv < 0) {
        lc_log_basic(LOG_ERR, "Failed to do ethtool_ioctl op: %s\n", strerror(errno));
    }
    else {
        lc_log_basic(LOG_NOTICE, "ethtool_ioctl cmd '%d' for intf '%s' was successful\n", edata->cmd, intf_name);
    }

    close(efd);

    return rv;
}

static int
md_nkn_get_proc_ctl_int(char *proc_file, uint32_t *value)
{
    FILE *FP = NULL;
    int rv = -1;

    *value = 0;

    FP = fopen(proc_file, "r");
    if (FP) {
        rv = fscanf(FP, "%u", value);
        fclose(FP);
        return 0;
    }
    return rv;
}

static int
md_nkn_proc_ctl_int(char *proc_file, int value)
{
    FILE *FP = NULL;
    int rv = -1;

    FP = fopen(proc_file, "w");
    if (FP) {
        rv = fprintf(FP, "%d", value);
        fclose(FP);
        return 0;
    }
    return rv;
}
/* Should be possible to extend this to set/get for
 * all ethtool commands
 */

int
md_nkn_ethtool(const char *intf_name,
	nkn_ethtool_cmd_t cmd, uint32_t *data)
{

    int err = 0;
    tstring *output = NULL;
    tstr_array *ret_params = NULL;
    const char *cmd_str = NULL;
    const char* oper = NULL;
    int32_t status = 0;
    struct ethtool_value edata;
    int need_change = 0;

    bail_null(intf_name);
    bail_null(data);

    if (cmd == NKN_ETHTOOL_GRO_SET) {
	oper = *data? "on":"off";
	cmd_str = "gro";

	err = lc_launch_quick_status(&status, &output, true, 5
		,"/usr/sbin/ethtool", "-K"
		,intf_name, cmd_str, oper);
	bail_error(err);

	memset(&edata, 0, sizeof(struct ethtool_value));
	edata.cmd = ETHTOOL_GFLAGS;
	if (!md_nkn_ethtool_ioctl(intf_name, &edata)) {
	    if(*data && !(ETH_FLAG_LRO & edata.data)) {
		edata.data |= ETH_FLAG_LRO;
		need_change = 1;
		lc_log_basic(LOG_NOTICE, "Enabling RSC/LRO for intf '%s'\n", intf_name);
	    }
	    else if (!*data && (ETH_FLAG_LRO & edata.data)) {
		edata.data &= ~ETH_FLAG_LRO;
		need_change = 1;
		lc_log_basic(LOG_NOTICE, "Disabling RSC/LRO for intf '%s'\n", intf_name);
	    }
	    else {
		need_change = 0;
	    }

	    if (need_change) {
		edata.cmd = ETHTOOL_SFLAGS;
		md_nkn_ethtool_ioctl(intf_name, &edata);
	    }
	}

	complain_error(status);

    } else if (cmd == NKN_ETHTOOL_GRO_GET) {
	uint32_t gro_idx = 0;
	const char *gro_status = NULL;
	str_value_t gro = {0};

	err = lc_launch_quick_status(&status, &output, true,3,
		"/usr/sbin/ethtool", "-k",
		intf_name);
	bail_error(err);

	complain_error(status);

	err = ts_tokenize(output, '\n', '\\', '"', 0, &ret_params);
	bail_error(err);

	err = tstr_array_linear_search_prefix_str(ret_params, "generic-receive-offload:",
		false, true, 0, &gro_idx);
	bail_error(err);

	gro_status = tstr_array_get_str_quick(ret_params, gro_idx);
	bail_null(gro_status);

	sscanf(gro_status, "generic-receive-offload: %s", gro);

	*data = (!strcmp(gro, "on")) ? 1 : 0;
    }

    if (cmd == NKN_ETHTOOL_GSO_SET) {
	oper = *data? "on":"off";
	cmd_str = "gso";

	err = lc_launch_quick_status(&status, &output, true, 5
		,"/usr/sbin/ethtool", "-K"
		,intf_name, cmd_str, oper);
	bail_error(err);

	memset(&edata, 0, sizeof(struct ethtool_value));
	edata.cmd = ETHTOOL_GGSO;
	if (!md_nkn_ethtool_ioctl(intf_name, &edata)) {
	    if(*data && !(NETIF_F_GSO & edata.data)) {
		edata.data |= NETIF_F_GSO;
		need_change = 1;
		lc_log_basic(LOG_NOTICE, "Enabling GSO for intf '%s'\n", intf_name);
	    }
	    else if (!*data && (NETIF_F_GSO & edata.data)) {
		edata.data &= ~NETIF_F_GSO;
		need_change = 1;
		lc_log_basic(LOG_NOTICE, "Disabling GSO for intf '%s'\n", intf_name);
	    }
	    else {
		need_change = 0;
	    }

	    if (need_change) {
		edata.cmd = ETHTOOL_SGSO;
		md_nkn_ethtool_ioctl(intf_name, &edata);
	    }
	}

	complain_error(status);

    } else if (cmd == NKN_ETHTOOL_GSO_GET) {
	uint32_t gso_idx = 0;
	const char *gso_status = NULL;
	str_value_t gso = {0};

	err = lc_launch_quick_status(&status, &output, true,3,
		"/usr/sbin/ethtool", "-k",
		intf_name);
	bail_error(err);

	complain_error(status);

	err = ts_tokenize(output, '\n', '\\', '"', 0, &ret_params);
	bail_error(err);

	err = tstr_array_linear_search_prefix_str(ret_params, "generic-segmentation-offload:",
		false, true, 0, &gso_idx);
	bail_error(err);

	gso_status = tstr_array_get_str_quick(ret_params, gso_idx);
	bail_null(gso_status);

	sscanf(gso_status, "generic-segmentation-offload: %s", gso);

	*data = (!strcmp(gso, "on")) ? 1 : 0;
    }

    if (cmd == NKN_ETHTOOL_TSO_SET) {
	oper = *data? "on":"off";
	cmd_str = "tso";

	err = lc_launch_quick_status(&status, &output, true, 5
		,"/usr/sbin/ethtool", "-K"
		,intf_name, cmd_str, oper);
	bail_error(err);

	memset(&edata, 0, sizeof(struct ethtool_value));
	edata.cmd = ETHTOOL_GTSO;
	if (!md_nkn_ethtool_ioctl(intf_name, &edata)) {
	    if(*data && !(NETIF_F_TSO & edata.data)) {
		edata.data |= NETIF_F_TSO;
		need_change = 1;
		lc_log_basic(LOG_NOTICE, "Enabling TSO for intf '%s'\n", intf_name);
	    }
	    else if (!*data && (NETIF_F_TSO & edata.data)) {
		edata.data &= ~NETIF_F_TSO;
		need_change = 1;
		lc_log_basic(LOG_NOTICE, "Disabling TSO for intf '%s'\n", intf_name);
	    }
	    else {
		need_change = 0;
	    }

	    if (need_change) {
		edata.cmd = ETHTOOL_STSO;
		md_nkn_ethtool_ioctl(intf_name, &edata);
	    }
	}

	complain_error(status);

    } else if (cmd == NKN_ETHTOOL_TSO_GET) {
	uint32_t tso_idx = 0;
	const char *tso_status = NULL;
	str_value_t tso = {0};

	err = lc_launch_quick_status(&status, &output, true,3,
		"/usr/sbin/ethtool", "-k",
		intf_name);
	bail_error(err);

	complain_error(status);

	err = ts_tokenize(output, '\n', '\\', '"', 0, &ret_params);
	bail_error(err);

	err = tstr_array_linear_search_prefix_str(ret_params, "tcp-segmentation-offload:",
		false, true, 0, &tso_idx);
	bail_error(err);

	tso_status = tstr_array_get_str_quick(ret_params, tso_idx);
	bail_null(tso_status);

	sscanf(tso_status, "tcp-segmentation-offload: %s", tso);

	*data = (!strcmp(tso, "on")) ? 1 : 0;
    }

bail:
    tstr_array_free(&ret_params);
    ts_free(&output);
    return err;
}


int md_nkn_events_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("nkn_if_events", 12, "/nkn/nvsd/events", NULL,
	    modrf_upgrade_from | modrf_namespace_unrestricted,
	    md_nkn_events_commit_side_effects, NULL,
	    NULL, NULL, md_nkn_events_commit_apply, NULL, 500, 0,
	    md_generic_upgrade_downgrade,
	    &md_nkn_if_events_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    err = mdm_set_upgrade_from(module, "interface", 9, false);
    bail_error(err);


    err = md_events_handle_int_interest_add(
	    "/net/interface/events/state_change",
	    md_nkn_if_handle_state_change_event, NULL);
    bail_error(err);

    err = md_events_handle_int_interest_add(
	    "/net/interface/events/link_up",
	    md_nkn_if_handle_state_change_event, NULL);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/net/interface/config/*/arp";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Control ARP state on the interface";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/net/interface/config/*/arp/ignore";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Configure ARP request handling behavior";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/net/interface/config/*/arp/announce";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Configure ARP announce behavior";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/net/interface/config/*/offloading/gro/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_check_node_func	= md_nkn_network_offloading_chk_func;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/net/interface/config/*/offloading/gso/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_check_node_func	= md_nkn_network_offloading_chk_func;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/net/interface/config/*/offloading/tso/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_check_node_func	= md_nkn_network_offloading_chk_func;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/net/interface/phy/state/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_restricted;
    node->mrn_mon_get_func =      md_ifs_state_get_interfaces;
    node->mrn_mon_iterate_func =  md_ifs_state_iterate_interfaces;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/net/interface/phy/state/*/offloading/gro";
    node->mrn_value_type = bt_bool;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_get_offloading;
    node->mrn_mon_get_arg = (void*)("gro");
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/net/interface/phy/state/*/offloading/gso";
    node->mrn_value_type = bt_bool;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_get_offloading;
    node->mrn_mon_get_arg = (void*)("gso");
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/net/interface/phy/state/*/offloading/tso";
    node->mrn_value_type = bt_bool;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_get_offloading;
    node->mrn_mon_get_arg = (void*)("tso");
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/net/interface/proc/intf/*/conf/arp_announce";
    node->mrn_value_type = bt_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_get_proc_intf_conf;
    node->mrn_mon_get_arg = (void*)("arp_announce");
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/net/interface/proc/intf/*/conf/arp_ignore";
    node->mrn_value_type = bt_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_get_proc_intf_conf;
    node->mrn_mon_get_arg = (void*)("arp_ignore");
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* IPMI Fan Failure event */
    err = mdm_new_event(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/ipmi/events/fanfailure";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event to notify Fan failure";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* IPMI Fan OK event */
    err = mdm_new_event(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/ipmi/events/fanok";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event to notify Fan OK";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* IPMI Power supply failure event */
    err = mdm_new_event(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/ipmi/events/powerfailure";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event to notify Power Supply failure";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* IPMI Power Supply OK event */
    err = mdm_new_event(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/ipmi/events/powerok";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event to notify Power Supply OK";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Log Export Failed Event */
    err = mdm_new_event(module, &node, 2);
    bail_error(err);
    node->mrn_name = "/nkn/nknlogd/events/logexportfailed";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/nkn/accesslog/config/profile/*";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "/nkn/monitor/filename";
    node->mrn_events[1]->mre_binding_type = bt_string;
    node->mrn_description = "Event sent when export of access log files failed";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Log write Failed Event */
    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nknlogd/events/logwritefailed";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "logfile";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_description = "Event sent when logwrite fails";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* IPMI Temperature high event */
    err = mdm_new_event(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/nkn/ipmi/events/temp_high";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/temp-curr";
    node->mrn_events[0]->mre_binding_type = bt_uint32;
    node->mrn_events[1]->mre_binding_name = "/nkn/monitor/temp-thresh";
    node->mrn_events[1]->mre_binding_type = bt_uint32;
    node->mrn_events[2]->mre_binding_name = "/nkn/monitor/temp-sensor";
    node->mrn_events[2]->mre_binding_type = bt_string;
    node->mrn_description = "Event sent when the temperature of chassis is high";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* IPMI Temperature OK event */
    err = mdm_new_event(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/nkn/ipmi/events/temp_ok";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/temp-curr";
    node->mrn_events[0]->mre_binding_type = bt_uint32;
    node->mrn_events[1]->mre_binding_name = "/nkn/monitor/temp-thresh";
    node->mrn_events[1]->mre_binding_type = bt_uint32;
    node->mrn_events[2]->mre_binding_name = "/nkn/monitor/temp-sensor";
    node->mrn_events[2]->mre_binding_type = bt_string;
    node->mrn_description = "Event sent when the temperature of chassis falls back to normal level";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/events/root_disk_mirror_broken";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event sent when the root disk mirror is broken";
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/mirror-broken-error";
    node->mrn_events[0]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/events/jbod_shelf_unreachable";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event sent when the connectivity to JBOD Shelf is removed";
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/jbod-shelf-error";
    node->mrn_events[0]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/events/root_disk_mirror_complete";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event sent when the root disk re-mirroring is complete";
    node->mrn_events[0]->mre_binding_name = "/nkn/monitor/mirror-complete";
    node->mrn_events[0]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_nkn_if_events_ug_rules);
    bail_error(err);
    ra = md_nkn_if_events_ug_rules;


    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/net/interface/config/*/arp";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/net/interface/config/*/offloading/gro/enable";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/net/interface/config/*/arp/ignore";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/net/interface/config/*/arp/announce";
    rule->mur_new_value_type =      bt_uint32;
    rule->mur_new_value_str =       "0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/net/interface/config/*/offloading/gso/enable";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name =                "/net/interface/config/*/offloading/tso/enable";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

bail:
    return err;
}


static int
md_nkn_if_handle_state_change_event(md_commit *commit, const mdb_db *db,
                                    const char *event_name,
                                    const bn_binding_array *bindings,
                                    void *cb_reg_arg, void *cb_arg)
{
    int err = 0;
    tbool found = false;
    const bn_binding *binding = NULL;
    tstring *ifname = NULL;
    tbool is_up = false;
    tstring *cause = NULL;
    char *c_node = NULL;
    int32 status;;
    tbool is_arp_enab = false;
    const char *c_ifname = NULL;
    tstr_array *name_parts = NULL;


    // BZ 5741: whenever link_up event is detected, force set the
    // txqueulen for said interface to 4096
    if (safe_strcmp(event_name, "/net/interface/events/link_up") == 0) {
	uint32 idx = 0;
	// Link up event. Get the interface name.
	err = bn_binding_array_get_first_matching_binding(bindings, "/net/interface/state/*/ifindex", 0, &idx, &binding);
	bail_error(err);

	if (!binding) {
	    goto bail;
	}

	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);

	c_ifname = tstr_array_get_str_quick(name_parts, 3);
	bail_null(c_ifname);
	//lc_log_debug(LOG_NOTICE, "%s : Link was down, now up", c_ifname);

	err = lc_launch_quick_status(&status,
		NULL, false, 2, "/opt/nkn/bin/set_interface_txqueuelen.sh", c_ifname);
	bail_error(err);

	goto bail;
    }

    err = bn_binding_array_get_binding_by_name(bindings, "ifname", &binding);
    bail_error(err);

    if (!binding) {
	goto bail;
    }

    err = bn_binding_get_tstr(binding, ba_value, 0, NULL, &ifname);
    bail_error_null(err, ifname);

    err = bn_binding_array_get_binding_by_name(bindings, "cause", &binding);
    bail_error(err);

    if (!binding)
	goto bail;

    err = bn_binding_get_tstr(binding, ba_value, 0, NULL, &cause);
    bail_error_null(err, cause);

    if (ts_equal_str(cause, "config", false) ||
	    ts_equal_str(cause, "override", false))
    {
	// BZ1634
	// Set txqueuelen to 4096 when interface comes up
	err = lc_launch_quick_status(&status,
		NULL, false, 2, "/opt/nkn/bin/set_interface_txqueuelen.sh", ts_str(ifname));
	bail_error(err);
    }


    if (ts_equal_str(cause, "config", false)) {
	// Config change .. so we need to apply the ARP
	// config.
	// For DHCP configured interfaces, the cause is
	// set as override.. so almost all config is over-ridden.
	// Hence it makes no sense in setting up ARP for
	// override cases.

	err = bn_binding_array_get_binding_by_name(bindings, "new/flags/oper_up", &binding);

	// if the interface is going down, then dont do anything
	bail_error(err);
	if ( binding == NULL) {
	    goto bail;
	}

	err = bn_binding_get_tbool(binding, ba_value, NULL, &is_up);
	bail_error(err);

	c_node = smprintf("/net/interface/config/%s/arp", ts_str(ifname));
	bail_null(c_node);

	err = mdb_get_node_value_tbool(commit, db, c_node, 0, &found, &is_arp_enab);
	bail_error(err);

	if (!found || !is_up) {
	    goto bail;
	}

	if (is_arp_enab) {
	    err = lc_launch_quick_status(&status,
		    NULL, false, 3, prog_ifconfig_path, ts_str(ifname), "arp");
	    bail_error(err);
	} else {
	    err = lc_launch_quick_status(&status,
		    NULL, false, 3, prog_ifconfig_path, ts_str(ifname), "-arp");
	    bail_error(err);
	}
    }

bail:
    safe_free(c_node);
    tstr_array_free(&name_parts);
    return err;
}

static int md_nkn_events_commit_side_effects(md_commit *commit,
                                            const mdb_db *old_db,
                                            mdb_db *inout_new_db,
                                            mdb_db_change_array *change_list,
                                            void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    bn_attrib *new_value = NULL;
    const char *ifname = NULL;
    tbool is_arp = true;
    int32 status = 0;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; ++i) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (!ts_has_prefix_str(change->mdc_name, "/net/interface/", false)) {
	    continue;
	}

	is_arp = bn_binding_name_parts_pattern_match(
		change->mdc_name_parts, true,
		"/net/interface/config/*/arp");
	if (!is_arp) {
	    continue;
	}

	if (change->mdc_new_attribs == NULL) {
	    continue; /* Maybe whole interface is deleted */
	}

	new_value = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	if(new_value == NULL) {
	    continue; /* Maybe whole interface was deleted */
	}

	/* This is called when the interface is being enumerated
	 */
	err = bn_attrib_get_tbool(new_value, NULL, NULL, &is_arp);
	bail_error(err);

	ifname = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	bail_null(ifname);

	if (change->mdc_change_type == mdct_modify) {
	    if (is_arp) {
		err = lc_launch_quick_status(&status,
			NULL, false, 3, prog_ifconfig_path, ifname, "arp");
		bail_error(err);
	    } else {
		err = lc_launch_quick_status(&status,
			NULL, false, 3, prog_ifconfig_path, ifname, "-arp");
		bail_error(err);
	    }

	    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		err = md_commit_set_return_status_msg_fmt(
			commit, 1, _("Error setting ARP flag for interface %s"),
			ifname);
		bail_error(err);
	    }
	}
    }
bail:
    return err;
}

#if 1
static int
md_nkn_get_offloading
		(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)

{
    int err = 0;
    uint32_t offloading_en = 0;
    tstring *t_str = NULL;
    node_name_t intf_nd = {0};
    tbool intf_valid = false;
    tstring *source = NULL;
    int ethtool_cmd = NKN_ETHTOOL_GRO_GET;

    if (strstr(node_name, "gro")) {
	ethtool_cmd = NKN_ETHTOOL_GRO_GET;
    } else if (strstr(node_name, "gso")) {
	ethtool_cmd = NKN_ETHTOOL_GSO_GET;
    } else if (strstr(node_name, "tso")) {
	ethtool_cmd = NKN_ETHTOOL_TSO_GET;
    }

    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    snprintf(intf_nd, sizeof(intf_nd),
	    "/net/interface/state/%s/devsource", ts_str(t_str));
    err = mdb_get_node_value_tstr(commit, db, intf_nd, 0,
	    &intf_valid, &source);
    bail_error(err);
    if(source  && ts_equal_str(source, "physical", false)) {
	err = md_nkn_ethtool(ts_str(t_str), ethtool_cmd,
		&offloading_en);
	bail_error(err);
    }

    err = bn_binding_new_tbool(ret_binding, node_name, ba_value, 0, offloading_en ? true:
	    false);
    bail_error(err);

bail:
    ts_free(&t_str);
    ts_free(&source);
    return err;
}
#endif

static int
md_nkn_get_proc_intf_conf
		(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    uint32_t arp_value = 0;
    tstring *t_str = NULL;
    node_name_t intf_nd = {0};
    node_name_t proc_file = {0};
    tbool intf_valid = false;
    tstring *source = NULL;
    const char *arp_string = "NA";

    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    snprintf(intf_nd, sizeof(intf_nd), "/net/interface/state/%s/devsource", ts_str(t_str));
    err = mdb_get_node_value_tstr(commit, db, intf_nd, 0, &intf_valid, &source);
    bail_error(err);

    if(source  && (ts_equal_str(source, "physical", false) || ts_equal_str(source, "bond", false))) {
        snprintf(proc_file, sizeof(proc_file), "/proc/sys/net/ipv4/conf/%s/%s", ts_str(t_str), (char *) arg);
        md_nkn_get_proc_ctl_int(proc_file, &arp_value);
    }

    if (0 == strcmp(arg, "arp_announce")) {
        switch(arp_value) {
            case 2:
                arp_string = "best";
                break;
            case 0:
            default:
                arp_string = "any";
        }
    }
    if (0 == strcmp(arg, "arp_ignore")) {
        switch(arp_value) {
            case 1:
                arp_string = "no-ip-match";
                break;
            case 2:
                arp_string = "no-match";
                break;
            case 8:
                arp_string = "all";
                break;
            case 0:
            default:
                arp_string = "none";
        }
    }

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0, arp_string);
    bail_error(err);

bail:
    ts_free(&t_str);
    ts_free(&source);
    return err;
}

static int md_ifs_state_get_interfaces(md_commit *commit, const mdb_db *db, const char *node_name,
	const bn_attrib_array *node_attribs,  bn_binding **ret_binding,
	uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *ifname = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);
    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 5); /* "/net/interface/phy/state/<ifname>"  */
    ifname = tstr_array_get_str_quick(parts, 4);
    bail_null(ifname);

    /* XXX/EMT validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
	    0, ifname);
    bail_error_null(err, *ret_binding);
bail:
    tstr_array_free(&parts);
    return(err);
}

static int md_ifs_state_iterate_interfaces(md_commit *commit, const mdb_db *db,
	const char *parent_node_name, const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 num_ifs = 0, i = 0;
    tstr_array *ifs = NULL;
    tstr_array *binding_parts = NULL;
    const char *ifname = NULL;
    const char *ifnode = NULL;

    err = mdb_get_matching_tstr_array( NULL, db,
	    "/net/interface/state/*/devsource",
	    0, &ifs);
    bail_error(err);
    bail_null(ifs);

    num_ifs = tstr_array_length_quick(ifs);
    for (i = 0; i < num_ifs; ++i) {
	ifnode = tstr_array_get_str_quick(ifs, i);
	bail_null(ifnode);
	tstr_array_free(&binding_parts);
	err = bn_binding_name_to_parts(ifnode, &binding_parts, NULL);
	bail_error(err);
	ifname = tstr_array_get_str_quick(binding_parts, 3);
	bail_null(ifname);
	err = (*iterate_cb)(commit, db, ifname, iterate_cb_arg);
	bail_error(err);
    }


bail:
    tstr_array_free(&ifs);
    tstr_array_free(&binding_parts);
    return err;
}


/*
 *
 * Filename:  watchdog.c
 * Date:      2010/03/25
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "bnode.h"
#include "watchdog_main.h"
#include <proc_utils.h>
#include "nkn_cntr_api.h"
#include "nkn_mgmt_defs.h"


#define GMGMT           1
#define MAX_RESULT      255


/*
 * A made up number for how many monitoring wildcards to indicate.
 * This is mainly used for performance testing.
 * The five fields per wildcard level are entirely arbitrary.
 */

/* Probe external declarations*/
extern int probe_dbg_msg;

extern tbool
is_nvsd_hung(void);

extern tbool
is_nvsd_preread_hung(void);

extern tbool
wd_is_grace_prd_elapsed(const char *proc_name,  int64_t *ret_uptime);

const uint32 watchdog_num_ext_mon_nodes = 1000;

static int watchdog_mon_handle_get_wildcard(const char *bname,
                                      const tstr_array *bname_parts,
                                      uint32 num_parts,
                                      const tstring *last_name_part,
                                      bn_binding_array *resp_bindings,
                                      tbool *ret_done);
static uint64
get_one_mfd_intf_stat (const char* mfd_name, const char* mfd_intf);

char *wd_nvsd_pid = NULL;


// previous state variable to monitor transition
// init to no preread state
int preread_prev_state = NO_PREREAD;

int
watchdog_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                  void *data, bn_binding_array *resp_bindings)
{
    int err = 0;
    bn_binding *binding = NULL;
    tbool done = false;
    tbool ext_mon_node = false;
    uint32 num_parts = 0;
    const tstring *last_name_part = NULL;
    int64_t uptime = 0;
    uint32_t preread_init;

    tbool live_return = false;

    /* Check if it is our node */
    if ((!lc_is_prefix("/nkn/nvsd/namespace", bname, false)) &&
	    (!lc_is_prefix("/nkn/nvsd/state/alive", bname, false))) {
        /* Not for us */
        goto bail;
    }

    num_parts = tstr_array_length_quick(bname_parts);

    last_name_part = tstr_array_get_quick(bname_parts, num_parts - 1);

    bail_error(err);

    if(!strcmp("/nkn/nvsd/state/alive",bname)) {
	FILE *fp;
	char result[MAX_RESULT];
	int status;
	tbool ret = true;
	tbool grace_period_elapsed = true;
	lc_log_basic(probe_dbg_msg,"########################Probing NVSD Liveness###############################");

	//  TODO
/* Determine the Heuristics */
/* processs the return values from the counter array like processing a expression */
/* Liveness counter array should be in such a way that it can from a expression all together */

/* If nvsd was restarted manually check if we elapsed grace period,
 *  If so continure else return true,
 *  This is might be an unwanted check,Doing this as we had bug 8822 and TM Bug 14207
 */
	if(watchdog_config.is_nvsd_rst) {
		// get uptime used for preread transition
		grace_period_elapsed = wd_is_grace_prd_elapsed("nvsd", &uptime);
	}

	preread_init = nkncnt_get_uint32("glob_service_preread_init");	
	if(grace_period_elapsed) {
		if (preread_init == NO_PREREAD ||
			preread_init == PREREAD_END) {
			// transition from init to end -- skip nvsd hung check
			// set 20% of grace period for network thread initialization
			if (preread_prev_state == PREREAD_START &&
				preread_init == PREREAD_END) {
				if (uptime) {
					watchdog_config.grace_period = uptime / 1000 +
							watchdog_config.grace_period / 5; 
				}
				lc_log_basic(LOG_NOTICE, "MFC_Probe: Ignoring liveness query(Extended Grace period not elapsed)");
			} else {
				live_return = is_nvsd_hung();

				//Reset the flag,so that we don't check for grace elapsed here after
				watchdog_config.is_nvsd_rst = false;
			}
			// set current state -- used to check transition 
			preread_prev_state = preread_init;
		} else {
			// preread state -- check only preread counters
			live_return = is_nvsd_preread_hung();
			lc_log_basic(LOG_NOTICE, "MFC_Probe: Ignoring liveness query(Extended Grace period not elapsed)");
			preread_prev_state = preread_init;
		}
	} else {
		if (preread_init == NO_PREREAD)
			lc_log_basic(LOG_NOTICE, "MFC_Probe: Ignoring liveness query(Grace period not elapsed)");
		else
			lc_log_basic(LOG_NOTICE, "MFC_Probe: Ignoring liveness query(Extended Grace period not elapsed)");
	}

	if(!live_return)  {
	    err = bn_binding_new_parts_tbool
		(&binding, bname, bname_parts, true,
		 ba_value, 0, !live_return);
	    lc_log_basic(probe_dbg_msg, "NVSD is Alive");
	} else {
	    lc_log_basic(probe_dbg_msg, "NVSD might be Hung !!!");
	    lc_log_basic(probe_dbg_msg, "restart nvsd is %d",watchdog_config.restart_nvsd);

	    if(!watchdog_config.restart_nvsd) {
		live_return = true;
		lc_log_basic(probe_dbg_msg, "Not restarting nvsd");

		err = bn_binding_new_parts_tbool
		    (&binding, bname, bname_parts, true,
		     ba_value, 0, live_return);
	    }

	}
	if(binding){
	    err = bn_binding_array_append_takeover(resp_bindings, &binding);
	    bail_error(err);
	    lc_log_basic(LOG_DEBUG, "Binding response is set\n");
	}
    }
    lc_log_basic(probe_dbg_msg,"$$$$$$$$$$$$$$$$$$$$$$$$Probing NVSD Liveness End$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
watchdog_mon_handle_iterate(const char *bname, const tstr_array *bname_parts,
                      void *data, tstr_array *resp_names)
{
    int err = 0;
    uint32 i = 0;
    char *bn_name = NULL;

    for (i = 0; i < watchdog_num_ext_mon_nodes; ++i) {
        bn_name = smprintf("%d", i);
        err = tstr_array_append_str_takeover(resp_names, &bn_name);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
watchdog_mon_handle_get_wildcard(const char *bname, const tstr_array *bname_parts,
                           uint32 num_parts, const tstring *last_name_part,
                           bn_binding_array *resp_bindings,
                           tbool *ret_done)
{
    int err = 0;
    bn_binding *binding = NULL;
    bn_type btype = bt_NONE;

    bail_null(last_name_part);
    bail_null(ret_done);
    *ret_done = false;

    /* 
     * We assume the caller has verified that bname_parts begins
     * with "/demo/echod/state/ext/<wc>", so we need only check if
     * we're at the wildcard number of parts and return the last name part
     * as the value.
     */

    if (num_parts == 5) {
        btype = bt_string;
    }
    else {
        goto bail;
    }

    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
                                   ba_value, btype, 0, ts_str(last_name_part));
    bail_error(err);

    err = bn_binding_array_append_takeover(resp_bindings, &binding);
    bail_error(err);
    *ret_done = true;

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */

/* End of watchdog.c */

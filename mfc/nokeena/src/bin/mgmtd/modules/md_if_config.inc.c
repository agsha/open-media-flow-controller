/* This is a graft point for MTU size check */
/* tree/src/bin/mgmtd/modules/md_if_config.c */

/* This was the response from Eric (Tall Maple)
The maximum of 1500 is not user-configurable, but there is a graft
point in md_if_config.c that you can use to override it, if you have
more specific information about what MTU range works with your NICs
and drivers.  Search for "graft point 1" in that file, and you'll see
the context your code will run in.  Your graft point could do the
check, call md_commit_set_return_status_msg_fmt() if it doesn't like
the MTU, and then set 'check_mtu' to false so the base check will not
run.  I'm not sure why the "mtu = atoi(ts_str(value))" line is below
the graft point; so for now, you'll have to copy that line.  Note that
if override the check, you'll also have to enforce the minimum MTU
yourself.
*/

/* We are only going to change the max size to 9216 (Jumbo Frame) */
md_ifc_ether_max_mtu = 9216;


#if MD_IF_CONFIG_INC_GRAFT_POINT == 5
/* For pacifica we set the speed and autoneg options
through udev rules
Dont apply them here */

/* ========================================================================= */
/* Customer-specific graft point 5: override for applying speed and duplex.
 * If we get here, we have a speed or duplex binding for an interface for
 * which speed and duplex have not yet been set during this commit.
 * (We track those together because often, they are applied together.)
 *
 * You may read from the following variables:
 *   - ifname
 *   - change
 *   - new_db
 *   - commit (used for querying new_db)
 *
 * If you want to apply the speed/duplex to this interface yourself,
 * do so, and then set speed_duplex_applied to 'true'.  If you do not,
 * do nothing, and the base Samara apply logic will be used.
 * =========================================================================
 */


#endif


#if MD_IF_CONFIG_INC_GRAFT_POINT == 4
    if (bn_binding_name_parts_pattern_match_va
        (change->mdc_name_parts, 0, 7,
         "net", "interface", "config", "*", "type", "ethernet", "speed")) {

	int if_virtio = 0;
	uint64_t if_speed = 0;

        ifname = tstr_array_get_str_quick(change->mdc_name_parts, 3);
        bail_null(ifname);
	err = lc_is_if_driver_virtio(ifname, md_ifc_sock, &if_virtio);
	bail_error(err);
	if (if_virtio == 1) {
	    if (change->mdc_new_attribs) {
		err = bn_attrib_array_get_attrib_tstr
		    (change->mdc_new_attribs, ba_value, NULL, &value);
		bail_error(err); /* Could be NULL for certain types */
	    }
	    lc_log_basic(LOG_NOTICE, "check: ifname: %s, speed: %s", ifname, ts_str_maybe(value));
	    if (true ==  ts_equal_str(value, "auto", false)) {
		err = md_commit_set_return_status_msg_fmt( commit, 0,
			"auto cannot be config for vMFC interfaces(%s)", ifname);
		bail_error(err);
		goto bail;
	    }
	    if_speed = strtol(ts_str_maybe_empty(value), NULL, 10);
	    if (if_speed < 100 || if_speed > 40000) {
		err = md_commit_set_return_status_msg_fmt( commit, 0,
			"vMFC interfaces cannot have less 100Mbps or more than 40Gbps");
		bail_error(err);
		goto bail;
	    }
	    goto bail;
	}
    }
#endif
/* End of md_if_config.inc.c */

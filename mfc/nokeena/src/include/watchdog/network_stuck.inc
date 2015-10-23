
#ifdef PROBE_CHECKS
PROBE_CHECK("network-stuck", network_stuck, 0, 5000, WD_ACTION_RESTART, true, THRD_INF_AND_BT)
#endif

#ifdef PROBE_FUNC
/*====================================================================================================
 * Network Stuck Alarm
 *
 *===================================================================================================*/

BEGIN_PROBE_CHECK_FUNC(network_stuck)
int ret = 0;

PROBE_CNTR(http_conn, "glob_tot_http_sockets")
PROBE_CNTR(tot_size_cache, "glob_tot_size_from_cache")
PROBE_CNTR(tot_size_disk, "glob_tot_size_from_disk")
PROBE_CNTR(tot_size_origin, "glob_tot_size_from_origin")
PROBE_CNTR(tot_size_tunnel, "glob_tot_size_from_tunnel")

PROBE_CNTR(http_act_conn, "glob_cur_open_http_socket")

char ret_err_msg[1024] = {0};

if( !is_network_thread_dead_lock(ret_err_msg, sizeof(ret_err_msg))) {

	if(PROBE_CNTR_DIFF(http_conn) &&
			( PROBE_CNTR_DIFF(tot_size_cache) || PROBE_CNTR_DIFF(tot_size_disk)
			  || PROBE_CNTR_DIFF(tot_size_origin) || PROBE_CNTR_DIFF(tot_size_tunnel) )) {
		//NVSD is good enough to handle new connections
		//Reset the conn flag
		PROBE_LOG("NVSD is good enough to handle new connections");
	} else if( (PROBE_CNTR_RET_R2(http_act_conn) ) &&
			( PROBE_CNTR_DIFF(tot_size_cache) || PROBE_CNTR_DIFF(tot_size_disk)
			  || PROBE_CNTR_DIFF(tot_size_origin) || PROBE_CNTR_DIFF(tot_size_tunnel) )) {
		// NVSD is Alive for sure
		PROBE_SET_RETVAL(false);

	} else if( (PROBE_CNTR_RET_R2(http_act_conn) ) ) {
		memset(&ret_err_msg, 0 , sizeof(ret_err_msg));
		send_probe_request(&ret, ret_err_msg, sizeof(ret_err_msg));
		if(ret) {
			PROBE_SET_RETVAL(false);
		} else {
			PROBE_SET_RETVAL(true);
			PROBE_LOG_ONCE("NVSD had an active connection but didn't"
				" respond to internal probe:%s,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu",
				    ret_err_msg,
				    PROBE_CNTR_RET_R2(http_act_conn), PROBE_CNTR_RET_R1(http_act_conn),
				    PROBE_CNTR_RET_R2(tot_size_cache), PROBE_CNTR_RET_R1(tot_size_cache),
				    PROBE_CNTR_RET_R2(tot_size_disk), PROBE_CNTR_RET_R1(tot_size_disk),
				    PROBE_CNTR_RET_R2(tot_size_origin), PROBE_CNTR_RET_R1(tot_size_origin),
				    PROBE_CNTR_RET_R2(tot_size_tunnel), PROBE_CNTR_RET_R1(tot_size_tunnel));
		}
	} else {
		/* Now the first check should match
		 * after sending a probe request
		 * since there was new conncetion from the probe*/
		memset(&ret_err_msg, 0 , sizeof(ret_err_msg));
		send_probe_request(&ret, ret_err_msg, sizeof(ret_err_msg));
		if(ret) {
			PROBE_SET_RETVAL(false);
		} else {
			PROBE_SET_RETVAL(true);
			PROBE_LOG_ONCE("NVSD didn't respond to internal probe:%s",
									ret_err_msg);
		}
	}
} else {
	/*Something is definitely wrong with NVSD
	 * as network thread counters are stuck */
	PROBE_SET_RETVAL(true);
	PROBE_LOG_ONCE("Network thread is stuck!!! %s", ret_err_msg);


}

END_PROBE_CHECK_FUNC(network_stuck)
#endif

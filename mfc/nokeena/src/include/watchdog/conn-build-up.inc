#ifdef PROBE_CHECKS
PROBE_CHECK("connection-build-up", conn_build_up, 0, 5000, WD_ACTION_RESTART, true, THRD_INF_AND_BT)
#endif

#ifdef PROBE_FUNC
/*====================================================================================================
 * Connection Build Up Alarm
 *
 *===================================================================================================*/
BEGIN_PROBE_CHECK_FUNC(conn_build_up)
PROBE_CNTR(curr_open_http_conn, "glob_cur_open_http_socket")
PROBE_CNTR(size_from_cache, "glob_tot_size_from_cache")
PROBE_CNTR(size_from_disk, "glob_tot_size_from_disk")
PROBE_CNTR(size_from_origin, "glob_tot_size_from_origin")
PROBE_CNTR(size_from_tunnel, "glob_tot_size_from_tunnel")

        uint32_t code = 0;

	if((PROBE_CNTR_RET_R2(curr_open_http_conn) == (watchdog_config.num_lic_sock + 200)) &&
	    ((!PROBE_CNTR_DIFF(size_from_cache)) && (!PROBE_CNTR_DIFF(size_from_disk)) &&
	     (!PROBE_CNTR_DIFF(size_from_origin)) && (!PROBE_CNTR_DIFF(size_from_tunnel))) ) {
		PROBE_LOG_ONCE("Possible connection build up : %u %lu/%lu, %lu/%lu, %lu/%lu, %lu%lu, %lu/%lu",
		    watchdog_config.num_lic_sock, PROBE_CNTR_RET_R2(curr_open_http_conn), PROBE_CNTR_RET_R1(curr_open_http_conn),
		    PROBE_CNTR_RET_R2(size_from_cache), PROBE_CNTR_RET_R1(size_from_cache),
		    PROBE_CNTR_RET_R2(size_from_disk), PROBE_CNTR_RET_R1(size_from_disk),
		    PROBE_CNTR_RET_R2(size_from_origin), PROBE_CNTR_RET_R1(size_from_origin),
		    PROBE_CNTR_RET_R2(size_from_tunnel), PROBE_CNTR_RET_R1(size_from_tunnel));
		PROBE_SET_RETVAL(true);

	}

END_PROBE_CHECK_FUNC(conn_build_up)

#endif

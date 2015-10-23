/* To be included in CLI,mgmtd,gmgmthd
 */

#ifdef PROBE_VERSION
MD_WD_VER(4)
#endif

#include "watchdog/network_stuck.inc"
#include "watchdog/conn-build-up.inc"
#include "watchdog/dm2_ingestion.inc"
#include "watchdog/dm2_stat.inc"
#include "watchdog/mem_leak.inc"
#include "watchdog/dm2_preread.inc"

/* Alarm Name, Func name, threshold, freq, hard_alarm, enabled */
#ifdef PROBE_CHECKS
//PROBE_CHECK("connection-build-up-alarm", conn_build_up, 0, 5000, true, true)
//PROBE_CHECK("disk-ingestion-alarm", dm2_ingestion , 180, 5000, true, true)
//PROBE_CHECK("disk-stat-alarm", dm2_stat , 180, 5000, true, true)
#endif

/*
#ifdef REMOVE_CHECKS
DELETE_PROBE_CHECK("xyz")
#endif


*/

//To modify check paramteres
/*
#ifdef PROBE_MODIFY_CHECKS
PROBE_MODIFY_CHECK(7,"connection-build-up", "threshold", uint32, 45)
PROBE_MODIFY_CHECK(8,"dm2-stat-alarm", "threshold", uint32, 45)
PROBE_MODIFY_CHECK(9,"dm2-ingestion", "poll_freq", uint32, 12000)
#endif
*/

//To delete checks
/*
#ifdef PROBE_DELETE_CHECKS
PROBE_DELETE_CHECK(10,"dm2-ingestion")
#endif
*/

#ifdef PROBE_FUNC

/*====================================================================================================
 * Sample 
 *
 *===================================================================================================*/
/*
PROBE_CHECK_FUNC(network_stuck)
PROBE_CNTR(network_thread1, "monitor_0_network_thread")

if(!PROBE_CNTR_DIFF(network_thread1)) {
   PROBE_LOG_ONCE("myfirst test:%lu/%lu", PROBE_CNTR_RET_R1(network_thread1), PROBE_CNTR_RET_R2(network_thread1));
   PROBE_SET_RETVAL(true);
}
END_PROBE_CHECK_FUNC(network_stuck)
*/

/* New replacements */




#endif

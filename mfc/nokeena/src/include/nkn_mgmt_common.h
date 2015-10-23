#ifndef _NKN_MGMT_COMMON_H_
#define _NKN_MGMT_COMMON_H_



/*
 * Launch Order : 
 *  Processes with equivalent launch orders will be launched simultaneously.
 *
 * Timeout : 
 *  minimum time PM should wait after launching this process before
 *  launching processes with higher initial launch orders. If multiple 
 *  processes share the same launch order, the overall timeout before 
 *  continuing will be the maximum of all of their initial launch timeouts.
 *
 */

#define NKN_LAUNCH_ORDER_NKNLOGD        "1000"
#define NKN_LAUNCH_TIMEOUT_NKNLOGD      "3000"

/*---- LEGACY STUFF --- */
#define NKN_LAUNCH_ORDER_ERRORLOG       "1500"
#define NKN_LAUNCH_TIMEOUT_ERRORLOG     "2"
#define NKN_LAUNCH_ORDER_ACCESSLOG      "1500"
#define NKN_LAUNCH_TIMEOUT_ACCESSLOG    "2"
/*---- END LEGACY STUFF --- */

#define NKN_LAUNCH_ORDER_OOMGR          "2000"
#define NKN_LAUNCH_TIMEOUT_OOMGR        "3000"

#define NKN_LAUNCH_ORDER_CMM            "2000"
#define NKN_LAUNCH_TIMEOUT_CMM          "3000"

#define NKN_LAUNCH_ORDER_VPEMGR         "2000"
#define NKN_LAUNCH_TIMEOUT_VPEMGR       "3000"

#define NKN_LAUNCH_ORDER_AGENTD         "2000"
#define NKN_LAUNCH_TIMEOUT_AGENTD       "3000"

#define NKN_LAUNCH_ORDER_SSH_TUNNEL     "2500"
#define NKN_LAUNCH_TIMEOUT_SSH_TUNNEL   "3500"

#define NKN_LAUNCH_ORDER_WATCHDOG       "3000"
#define NKN_LAUNCH_TIMEOUT_WATCHDOG     "3000"

#define NKN_LAUNCH_ORDER_GMGMTHD	"3000"
#define NKN_LAUNCH_TIMEOUT_GMGMTHD	"3000"

#define NKN_LAUNCH_ORDER_TMD            "2500"
#define NKN_LAUNCH_TIMEOUT_TMD          "3000"

#define NKN_LAUNCH_ORDER_DROP_CACHE     "3500"
#define NKN_LAUNCH_TIMEOUT_DROP_CACHE   "1000"

#define NKN_LAUNCH_ORDER_PROXYD         "3500"
#define NKN_LAUNCH_TIMEOUT_PROXYD       "5000"

#define NKN_NSMARTD_LAUNCH_ORDER	"3700"
#define NKN_NSMARTD_LAUNCH_TIMEOUT	"3000"

#define NKN_LAUNCH_ORDER_ADNSD	        "3900"
#define NKN_LAUNCH_TIMEOUT_ADNSD        "5000"

/* nknexecd should be launched before nvsd and watchdog */
#define NKN_LAUNCH_ORDER_EXECD		"2900"
/* nvsd should wait for 5 seconds after execd */
#define NKN_LAUNCH_TIMEOUT_EXECD	"5000"

#define NKN_LAUNCH_ORDER_NVSD           "4000"
#define NKN_LAUNCH_TIMEOUT_NVSD         "5000"

#define NKN_LAUNCH_ORDER_UCFLTD         "4500" /* URL Category Filter daemon */
#define NKN_LAUNCH_TIMEOUT_UCFLTD       "3000"

#define NKN_LAUNCH_ORDER_CRAWLER         "5000"
#define NKN_LAUNCH_TIMEOUT_CRAWLER       "5000"

#define NKN_LAUNCH_ORDER_NKNCNT         "5000"
#define NKN_LAUNCH_TIMEOUT_NKNCNT       "2"

#define NKN_LAUNCH_ORDER_CACHE_EVICTD   "6000"
#define NKN_LAUNCH_TIMEOUT_CACHE_EVICTD "3000"


#define NKN_LAUNCH_ORDER_DASHBOARD      "7000"
#define NKN_LAUNCH_TIMEOUT_DASHBOARD    "3000"

#define NKN_LAUNCH_ORDER_INGESTER	"7000"
#define NKN_LAUNCH_TIMEOUT_INGESTER	"3000"

#define NKN_LAUNCH_ORDER_FTPD           "8000"
#define NKN_LAUNCH_TIMEOUT_FTPD         "3000"

#define NKN_LAUNCH_ORDER_FILE_MFPD      "8500"
#define NKN_LAUNCH_TIMEOUT_FILE_MFPD    "3000"

#define NKN_LAUNCH_ORDER_LIVE_MFPD      "8600"
#define NKN_LAUNCH_TIMEOUT_LIVE_MFPD    "3000"

#define NKN_LAUNCH_ORDER_CACHE_META_EVICTD   "9000"
#define NKN_LAUNCH_TIMEOUT_CACHE_META_EVICTD "3000"

#define NKN_LAUNCH_ORDER_PACIFICA_RESILIENCY "3300"
#define NKN_LAUNCH_TIMEOUT_PACIFICA_RESILIENCY "4000"

#define NKN_LAUNCH_ORDER_CACHE_EVICTD_SSD   "9500"
#define NKN_LAUNCH_TIMEOUT_CACHE_EVICTD_SSD "3000"

#define NKN_LAUNCH_ORDER_CACHE_EVICTD_SAS   "9600"
#define NKN_LAUNCH_TIMEOUT_CACHE_EVICTD_SAS "3000"

#define NKN_LAUNCH_ORDER_CACHE_EVICTD_SATA   "9700"
#define NKN_LAUNCH_TIMEOUT_CACHE_EVICTD_SATA "3000"

#define NKN_LAUNCH_ORDER_IRQPIN_IXGBED      "3400"
#define NKN_LAUNCH_TIMEOUT_IRQPIN_IXGBED    "4000"


#define NKN_LAUNCH_ORDER_DISK_AUTO_ENABLE   "9800"
#define NKN_LAUNCH_TIMEOUT_DISK_AUTO_ENABLE "3000"

#define BGPD_LAUNCH_ORDER	"4500"
#define BGPD_LAUNCH_TIMEOUT	"3000"

#define ZEBRA_LAUNCH_ORDER	"5500"
#define ZEBRA_LAUNCH_TIMEOUT	"3000"

#define NKN_LAUNCH_ORDER_LFD    "8800"
#define NKN_LAUNCH_TIMEOUT_LFD  "3000"

#define NKN_LAUNCH_ORDER_CB	"9900"
#define NKN_LAUNCH_TIMEOUT_CB  	"3000"

#define NKN_LAUNCH_ORDER_JCE_GW         "6000"
/* NOTE: No Dependcy of other processes, NO TIMEOUT REQUIRED */

#define NKN_LAUNCH_ORDER_LOGANALYZER_PBR     "7000"
/* NOTE: No Dependcy of other processes, NO TIMEOUT REQUIRED */

#define NKN_LAUNCH_ORDER_DPITPROXY     "7500"
/* NOTE: No Dependcy of other processes, NO TIMEOUT REQUIRED */

#endif

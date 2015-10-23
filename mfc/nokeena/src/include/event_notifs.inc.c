/*
 *
 * Filename:  event_notifs.inc.c
 * Date:      $Date: 2009/01/22$
 * Author:    $Author: Chitra Devi R $
 *
 */

/*
 * Graft point 1 is inside an array of structures, so it can't do this.
 */
#if EVENT_NOTIFS_INC_GRAFT_POINT != 1
#ifndef __EVENT_NOTIFS_INC_C_
#define __EVENT_NOTIFS_INC_C_
#endif /* __EVENT_NOTIFS_INC_C_ */

#endif /* EVENT_NOTIFS_INC_GRAFT_POINT != 1 */

/* 
 * This file is include by include/event_notifs.h.
 * The different blocks are chosen by the caller #define'ing
 * EVENT_NOTIFS_INC_GRAFT_POINT to the numbers seen below.
 */

/*
 * Graft point 1: declarations
 */
#if EVENT_NOTIFS_INC_GRAFT_POINT == 1

#if defined(PROD_FEATURE_I18N_SUPPORT)
    /* Must match GETTEXT_PACKAGE as defined in the include Makefile */
    #define MFD_INCLUDE_GETTEXT_DOMAIN "mfd_incl"
#endif

{
    "/stats/events/connection_rate/rising/error", "connection-rate-high",
    N_("Connection rate has exceeded configured threshold"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/cache_byte_rate/rising/error", "cache-byte-rate-high",
    N_("Delivery bandwidth contributed by memory cache has exceeded configured threshold"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/origin_byte_rate/rising/error", "origin-byte-rate-high",
    N_("Delivery bandwidth contributed by origin has exceeded configured threshold"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_disk_bw/rising/error", "nkn-disk-bw-high",
    N_("Disk Bandwidth has exceeded configured threshold"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
/*
{
    "/stats/events/avg_cache_byte_rate/rising/error", "avg-cache-byte-rate",
    N_("Avg Cache Byte Rate Crossed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/avg_origin_byte_rate/rising/error", "avg-origin-byte-rate",
    N_("Avg Origin Byte Rate Crossed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/avg_disk_byte_rate/rising/error", "avg-disk-byte-rate",
    N_("Avg Disk Byte Rate Crossed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},*/
{
    "/stats/events/http_transaction_rate/rising/error", "http-transaction-rate-high",
    N_("HTTP transaction rate has exceeded configured threshold"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},

{
    "/stats/events/resource_pool_event/rising/error", "rp-usage-high",
    N_("Resource pool usage has exceeded configured threshold"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/resource_pool_event/rising/clear", "rp-usage-high-ok",
    N_("A resource Pool usage has come down within limits"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/resource_pool_event/falling/error", "rp-usage-low",
    N_("Resource Pool usage fallen from its lower limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/resource_pool_event/falling/clear", "rp-usage-low-ok",
    N_("Resource Pool usage came up within limits"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
/* will send only email */
{
    "/nkn/nvsd/notify/nvsd_stuck", "NVSD might be stuck",
    N_("Watchdog suspects that NVSD might be stuck"), false, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/resource_mgr/state/update", "resource pool state updated",
    N_("Resource Pool state has been updated"), false, false,
    eec_none, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/connection_rate/rising/clear", "connection-rate-ok",
    N_("Connection Rate resumed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/cache_byte_rate/rising/clear", "cache-byte-rate-ok",
    N_("Cache Byte Rate resumed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/origin_byte_rate/rising/clear", "origin-byte-rate-ok",
    N_("Origin Byte Rate resumed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_disk_bw/rising/clear", "nkn-disk-bw-ok",
    N_("Disk Bandwidth has fallen back to acceptable level"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/http_transaction_rate/rising/clear", "http-transaction-rate-ok",
    N_("HTTP Transaction Rate resumed its Limit"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/cluster/node_down", "cluster-node-down",
    N_("Cluster node is offline"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/cluster/node_up", "cluster-node-up",
    N_("Cluster node is online"), true, false,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/ipmi/events/fanfailure", "mfc-fan-failure",
    N_("IPMI Event Fan Failure notification"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/ipmi/events/fanok", "mfc-fan-ok",
    N_("IPMI Event Fan OK notification"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/ipmi/events/powerfailure", "mfc-powersupply-failure",
    N_("IPMI Event Power Supply Failure notification"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/ipmi/events/powerok", "mfc-powersupply-ok",
    N_("IPMI Event Power Supply OK notification"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    CUSTOMER_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/paging/rising/clear", "paging-ok",
    N_("Paging activity has fallen back to normal levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_mem_util/rising/error", "memutil-high",
    N_("Memory usage has risen above acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_mem_util/rising/clear", "memutil-ok",
    N_("Memory usage has fallen back to acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/intf_util/rising/clear", "netusage-ok",
    N_("Network utilization has fallen back to acceptable levels"), true, true,
   eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_disk_io/rising/error", "nkn-disk-io-high",
    N_("Disk I/O per second has exceeded acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_disk_io/rising/clear", "nkn-disk-io-ok",
    N_("Disk I/O per second has fallen back to acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/cpu_util_indiv/rising/clear", "cpu-util-ok",
    N_("CPU utilization has fallen back to normal levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/aggr_cpu_util/rising/error", "aggr-cpu-util-high",
    N_("Aggregate CPU utilization has risen above acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/aggr_cpu_util/rising/clear", "aggr-cpu-util-ok",
    N_("Aggregate CPU utilization has fallen back to normal levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/cache_hit_ratio/falling/error", "cache-hit-ratio-low",
    N_("Cache Hit Ratio has fallen below acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/cache_hit_ratio/falling/clear", "cache-hit-ratio-ok",
    N_("Cache Hit Ratio has risen back to normal levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/appl_cpu_util/rising/error", "appl-cpu-util-high",
    N_("Application CPU utilization has risen above acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/appl_cpu_util/rising/clear", "appl-cpu-util-ok",
    N_("Application CPU utilization has fallen back to normal levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_disk_space/falling/error", "nkn-disk-space-low",
    N_("Disk space has fallen below acceptable levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/stats/events/nkn_disk_space/falling/clear", "nkn-disk-space-ok",
    N_("Disk Space has risen back to normal levels"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nknlogd/events/logexportfailed", "log-export-failed",
    N_("Export of Access log files failed"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/diskcache/events/disk_failure", "disk-failure",
    N_("Disk Failure due to read/write errors"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/pm/events/process_launched", "process-up",
    N_("A process in the system is up"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/ipmi/events/temp_high", "temperature-high",
    N_("Temperature of the chassis is high"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/ipmi/events/temp_ok", "temperature-ok",
    N_("Temperature of the chassis is back to normal limits"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/events/root_disk_mirror_broken", "root-disk-mirror-broken",
    N_("Root Disk Mirror is broken"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/events/jbod_shelf_unreachable", "jbod-shelf-unreachable",
    N_("Connectivity to JBOD Shelf is removed"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},
{
    "/nkn/nvsd/events/root_disk_mirror_complete", "root-disk-mirror-complete",
    N_("Root Disk Re-Mirroring is complete"), true, true,
    eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
    MFD_INCLUDE_GETTEXT_DOMAIN
#endif
},

#endif /* GRAFT_POINT 1 */

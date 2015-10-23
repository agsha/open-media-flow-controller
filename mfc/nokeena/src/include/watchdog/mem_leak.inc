#ifdef PROBE_CHECKS
PROBE_CHECK("memory-leak", mem_leak, 0, 5000, WD_ACTION_RESTART, true, THRD_INF_AND_BT)
#endif

#ifdef PROBE_FUNC
/*====================================================================================================
 * Connection Build Up Alarm
 *
 *===================================================================================================*/
uint32_t nvsd_mem_soft_limit = 0;
uint32_t nvsd_mem_hard_limit = 0;

BEGIN_PROBE_CHECK_FUNC(mem_leak)
    uint32_t code = 0;
    int err = 0;
    char *rss_val = NULL;
    uint32_t mem_usage = 0;

    wd_get_mem_usage("nvsd", &mem_usage);

    if(nvsd_mem_soft_limit &&  mem_usage
	&&(mem_usage >= (nvsd_mem_soft_limit * 1024))) {
	PROBE_LOG_NOTICE("NVSD memory usage very high: %u/%u",
		    mem_usage, nvsd_mem_soft_limit * 1024);

    }

    if(nvsd_mem_hard_limit && mem_usage
	&&(mem_usage >= (nvsd_mem_hard_limit * 1024))) {
	PROBE_LOG_ONCE("NVSD memory usage very high: %u/%u",
		    mem_usage, nvsd_mem_hard_limit * 1024);
	PROBE_SET_RETVAL(true);

    }

END_PROBE_CHECK_FUNC(mem_leak)

#endif

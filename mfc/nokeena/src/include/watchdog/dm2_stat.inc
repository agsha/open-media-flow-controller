#ifdef PROBE_CHECKS
PROBE_CHECK("disk-stat", dm2_stat , 180, 5000, WD_ACTION_RESTART, true, THRD_INF_AND_BT)
#endif

#ifdef PROBE_FUNC
/*====================================================================================================
 * DM2 Stat Alarm
 *
 *===================================================================================================*/
BEGIN_PROBE_CHECK_FUNC(dm2_stat)
PROBE_CNTR(sata_dm2_stat_curr,"SATA.dm2_stat_curr_cnt")
PROBE_CNTR(ssd_dm2_stat_curr,"SSD.dm2_stat_curr_cnt")
PROBE_CNTR(sas_dm2_stat_curr,"SAS.dm2_stat_curr_cnt")
PROBE_CNTR(dm2_stat_cnt, "glob_dm2_stat_cnt")

PROBE_CNTR(sata_dm2_preread_stat_curr_cnt,"SATA.dm2_preread_stat_curr_cnt")
PROBE_CNTR(sas_dm2_preread_stat_curr_cnt,"SAS.dm2_preread_stat_curr_cnt")
PROBE_CNTR(ssd_dm2_preread_stat_curr_cnt,"SSD.dm2_preread_stat_curr_cnt")

        if( (((PROBE_CNTR_RET_R2(sata_dm2_stat_curr) - PROBE_CNTR_RET_R2(sata_dm2_preread_stat_curr_cnt) ) > 0) &&
		(!PROBE_CNTR_DIFF(sata_dm2_stat_curr)) && (!PROBE_CNTR_DIFF(dm2_stat_cnt)))) {
                PROBE_LOG_ONCE("Possible issue with DM2 stat for SATA tier : %lu/%lu,%lu/%lu",
                                PROBE_CNTR_RET_R2(sata_dm2_stat_curr) - PROBE_CNTR_RET_R2(sata_dm2_preread_stat_curr_cnt),
				PROBE_CNTR_RET_R1(sata_dm2_stat_curr) - PROBE_CNTR_RET_R1(sata_dm2_preread_stat_curr_cnt),
                                PROBE_CNTR_RET_R2(dm2_stat_cnt), PROBE_CNTR_RET_R1(dm2_stat_cnt));

               PROBE_SET_RETVAL(true);
        }

        if( (((PROBE_CNTR_RET_R2(ssd_dm2_stat_curr)- PROBE_CNTR_RET_R2(ssd_dm2_preread_stat_curr_cnt) )> 0)
		&& (!PROBE_CNTR_DIFF(ssd_dm2_stat_curr)) && (!PROBE_CNTR_DIFF(dm2_stat_cnt)))) {
                PROBE_LOG_ONCE("Possible issue with DM2 stat for ssd tier : %lu/%lu,%lu/%lu",
                                PROBE_CNTR_RET_R2(ssd_dm2_stat_curr) - PROBE_CNTR_RET_R2(ssd_dm2_preread_stat_curr_cnt),
				PROBE_CNTR_RET_R1(ssd_dm2_stat_curr) - PROBE_CNTR_RET_R1(ssd_dm2_preread_stat_curr_cnt),
                                PROBE_CNTR_RET_R2(dm2_stat_cnt), PROBE_CNTR_RET_R1(dm2_stat_cnt));

               PROBE_SET_RETVAL(true);
        }

        if( (((PROBE_CNTR_RET_R2(sas_dm2_stat_curr) - PROBE_CNTR_RET_R2(sas_dm2_preread_stat_curr_cnt))  > 0) &&
		(!PROBE_CNTR_DIFF(sas_dm2_stat_curr)) && (!PROBE_CNTR_DIFF(dm2_stat_cnt)))) {
                PROBE_LOG_ONCE("Possible issue with DM2 stat for sas tier : %lu/%lu,%lu/%lu",
                                PROBE_CNTR_RET_R2(sas_dm2_stat_curr) - PROBE_CNTR_RET_R2(sas_dm2_preread_stat_curr_cnt),
				PROBE_CNTR_RET_R1(sas_dm2_stat_curr) - PROBE_CNTR_RET_R1(sas_dm2_preread_stat_curr_cnt),
                                PROBE_CNTR_RET_R2(dm2_stat_cnt), PROBE_CNTR_RET_R1(dm2_stat_cnt));

               PROBE_SET_RETVAL(true);
        }

END_PROBE_CHECK_FUNC(dm2_stat)

#endif

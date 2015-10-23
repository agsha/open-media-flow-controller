#ifdef PROBE_CHECKS
PROBE_CHECK("disk-ingestion", dm2_ingestion , 180, 15000, WD_ACTION_RESTART, true, THRD_INF_AND_BT)
#endif

#ifdef PROBE_FUNC
/*====================================================================================================
 * DM2 Ingestion Alarm 
 *
 *===================================================================================================*/
BEGIN_PROBE_CHECK_FUNC(dm2_ingestion)

PROBE_CNTR(sata_dm2_put_curr, "SATA.dm2_put_curr_cnt")
PROBE_CNTR(sata_dm2_put_cnt,"SATA.dm2_put_cnt")
PROBE_CNTR(sata_dm2_raw_wr_bytes,"SATA.dm2_raw_write_bytes")
PROBE_CNTR(sata_dm2_put_err,"SATA.dm2_put_err")
PROBE_CNTR(sata_dm2_del_bytes,"SATA.dm2_delete_bytes")
PROBE_CNTR(sata_op_disable, "SATA.dm2_cache_disabling")

PROBE_CNTR(ssd_dm2_put_curr, "SSD.dm2_put_curr_cnt")
PROBE_CNTR(ssd_dm2_put_cnt,"SSD.dm2_put_cnt")
PROBE_CNTR(ssd_dm2_raw_wr_bytes,"SSD.dm2_raw_write_bytes")
PROBE_CNTR(ssd_dm2_put_err,"SSD.dm2_put_err")
PROBE_CNTR(ssd_dm2_del_bytes,"SSD.dm2_delete_bytes")
PROBE_CNTR(ssd_op_disable, "SSD.dm2_cache_disabling")

PROBE_CNTR(sas_dm2_put_curr, "SAS.dm2_put_curr_cnt")
PROBE_CNTR(sas_dm2_put_cnt,"SAS.dm2_put_cnt")
PROBE_CNTR(sas_dm2_raw_wr_bytes,"SAS.dm2_raw_write_bytes")
PROBE_CNTR(sas_dm2_put_err,"SAS.dm2_put_err")
PROBE_CNTR(sas_dm2_del_bytes,"SAS.dm2_delete_bytes")
PROBE_CNTR(sas_op_disable, "SAS.dm2_cache_disabling")


        if( ((PROBE_CNTR_RET_R2(sata_dm2_put_curr) > 0) && (!PROBE_CNTR_DIFF(sata_dm2_put_cnt)) && (!PROBE_CNTR_DIFF(sata_dm2_raw_wr_bytes))
                                && (!PROBE_CNTR_DIFF(sata_dm2_put_err)) && (!PROBE_CNTR_DIFF(sata_dm2_del_bytes))  && (!PROBE_CNTR_RET_R2(sata_op_disable)) )) {
                PROBE_LOG_ONCE("Possible issue with Ingestions for SATA tier : %lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu",
                                PROBE_CNTR_RET_R2(sata_op_disable), PROBE_CNTR_RET_R1(sata_op_disable),
                                PROBE_CNTR_RET_R2(sata_dm2_put_curr), PROBE_CNTR_RET_R1(sata_dm2_put_curr),
                                PROBE_CNTR_RET_R2(sata_dm2_put_cnt), PROBE_CNTR_RET_R1(sata_dm2_put_cnt),
                                PROBE_CNTR_RET_R2(sata_dm2_raw_wr_bytes), PROBE_CNTR_RET_R1(sata_dm2_raw_wr_bytes),
                                PROBE_CNTR_RET_R2(sata_dm2_put_err), PROBE_CNTR_RET_R1(sata_dm2_put_err),
                                PROBE_CNTR_RET_R2(sata_dm2_del_bytes), PROBE_CNTR_RET_R1(sata_dm2_del_bytes));
                PROBE_SET_RETVAL(true);
        }

        if( ((PROBE_CNTR_RET_R2(ssd_dm2_put_curr) > 0) && (!PROBE_CNTR_DIFF(ssd_dm2_put_cnt)) && (!PROBE_CNTR_DIFF(ssd_dm2_raw_wr_bytes))
                                && (!PROBE_CNTR_DIFF(ssd_dm2_put_err)) && (!PROBE_CNTR_DIFF(ssd_dm2_del_bytes))  && (!PROBE_CNTR_RET_R2(ssd_op_disable)) )) {
                PROBE_LOG_ONCE("Possible issue with Ingestions for SSD tier : %lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu",
                                PROBE_CNTR_RET_R2(ssd_op_disable), PROBE_CNTR_RET_R1(ssd_op_disable),
                                PROBE_CNTR_RET_R2(ssd_dm2_put_curr), PROBE_CNTR_RET_R1(ssd_dm2_put_curr),
                                PROBE_CNTR_RET_R2(ssd_dm2_put_cnt), PROBE_CNTR_RET_R1(ssd_dm2_put_cnt),
                                PROBE_CNTR_RET_R2(ssd_dm2_raw_wr_bytes), PROBE_CNTR_RET_R1(ssd_dm2_raw_wr_bytes),
                                PROBE_CNTR_RET_R2(ssd_dm2_put_err), PROBE_CNTR_RET_R1(ssd_dm2_put_err),
                                PROBE_CNTR_RET_R2(ssd_dm2_del_bytes), PROBE_CNTR_RET_R1(ssd_dm2_del_bytes));
                PROBE_SET_RETVAL(true);
        }

        if( ((PROBE_CNTR_RET_R2(sas_dm2_put_curr) > 0) && (!PROBE_CNTR_DIFF(sas_dm2_put_cnt)) && (!PROBE_CNTR_DIFF(sas_dm2_raw_wr_bytes))
                                && (!PROBE_CNTR_DIFF(sas_dm2_put_err)) && (!PROBE_CNTR_DIFF(sas_dm2_del_bytes))  && (!PROBE_CNTR_RET_R2(sas_op_disable)) )) {
                PROBE_LOG_ONCE("Possible issue with Ingestions for SAS tier : %lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu/%lu",
                                PROBE_CNTR_RET_R2(sas_op_disable), PROBE_CNTR_RET_R1(sas_op_disable),
                                PROBE_CNTR_RET_R2(sas_dm2_put_curr), PROBE_CNTR_RET_R1(sas_dm2_put_curr),
                                PROBE_CNTR_RET_R2(sas_dm2_put_cnt), PROBE_CNTR_RET_R1(sas_dm2_put_cnt),
                                PROBE_CNTR_RET_R2(sas_dm2_raw_wr_bytes), PROBE_CNTR_RET_R1(sas_dm2_raw_wr_bytes),
                                PROBE_CNTR_RET_R2(sas_dm2_put_err), PROBE_CNTR_RET_R1(sas_dm2_put_err),
                                PROBE_CNTR_RET_R2(sas_dm2_del_bytes), PROBE_CNTR_RET_R1(sas_dm2_del_bytes));
                PROBE_SET_RETVAL(true);
        }

END_PROBE_CHECK_FUNC(dm2_ingestion)

#endif


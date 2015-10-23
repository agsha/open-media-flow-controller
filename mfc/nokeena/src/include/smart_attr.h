#ifndef _SMART_ATTR_H
#define _SMART_ATTR_H
/*
 * (C) Copyright 2012 Juniper Networks
 * 
 * This file contains symbols necessary to interface with nsmartd
 * 
 * Author : Srihari MS (sriharim@juniper.net)
 *
 */

/* String list for the SMART attributes */
#define RAW_READ_ERR_RATE_1 "raw_read_err_rate_1"
#define REALLOC_SEC_CNT_5 "realloc_sec_cnt_5"
#define POWER_ON_SECS_9 "power_on_secs_9"
#define POWER_CYCLE_COUNT_12 "power_cycle_count_12"
#define READ_SOFT_ERR_RATE_13 "read_soft_err_rate_13"
#define GB_ERASED_100 "gb_erased_100"
#define RESV_BLOCK_COUNT_170 "resv_block_count_170"
#define PROG_FAIL_COUNT_171 "prog_fail_count_171"
#define ERASE_FAIL_COUNT_172 "erase_fail_count_172"
#define UNEXP_POWER_LOSS_COUNT_174 "unexp_power_loss_count_174"
#define WEAR_LEVEL_COUNT_177 "wear_level_count_177"
#define PROG_FAIL_COUNT_181 "prog_fail_count_181"
#define ERASE_FAIL_COUNT_182 "erase_fail_count_182"
#define END_TO_END_ERR_184 "end_to_end_err_184"
#define UNCORRECTABLE_ERRS_REPORTED_187 "uncorrectable_errs_reported_187"
#define TEMP_IN_C_194 "temp_in_C_194"
#define HW_ERR_RECOV_195 "hw_err_recov_195"
#define REALLOC_EVENT_COUNT_196 "realloc_event_count_196"
#define OFFLINE_UNCORRECTABLE_198 "offline_uncorrectable_198"
#define UDMA_CRC_ERR_199 "udma_crc_err_199"
#define SOFT_READ_ERR_RATE_201 "soft_read_err_rate_201"
#define SOFT_ECC_CORRECTION_204 "soft_ecc_correction_204"
#define HEAD_AMPLITUDE_230 "head_amplitude_230"
#define SSD_LIFE_LEFT_231 "ssd_life_left_231"
#define AVL_RESV_SPACE_232 "avl_resv_space_232"
#define MEDIA_WEAROUT_233 "media_wearout_233"
#define UNKNOWN_ATTR_234 "unknown_attr_234"
#define UNKNOWN_ATTR_235 "unknown_attr_235"
#define TOTAL_WRITES_241 "total_writes_241"
#define TOTAL_READ_242 "total_read_242"

#define NEED_THROTTLE "need_throttle"
#define DRIVE_SIZE_IN_GB "drive_size_in_gb"
#define PE_CYCLES "pe_cycle_limit"

#define DM2_DISK_NO_THROTTLE 0
#define DM2_DISK_SOFT_THROTTLE 1
#define DM2_DISK_HARD_THROTTLE 2
#endif

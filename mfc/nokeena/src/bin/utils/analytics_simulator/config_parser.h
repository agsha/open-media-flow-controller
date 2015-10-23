/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Config parser header file
 *
 * Author: Jeya ganesh babu
 *
 */

#ifndef _CONFIG_PARSER_H
#define _CONFIG_PARSER_H
#include <stdint.h>


#define MAX_INGEST_ENTRIES_CMD	   0
#define INGEST_RATE_CMD            1
#define EVICT_RATE_CMD             2
#define SATA_DISK_SIZE_CMD         3
#define SAS_DISK_SIZE_CMD	   4
#define SSD_DISK_SIZE_CMD	   5
#define RAM_SIZE_CMD               6
#define AM_SIZE_CMD		   7
#define INGEST_SIZE_CMD            8
#define SSD_SIZE_CMD               9
#define ORIGIN_FETCH_NUMBER_CMD   10
#define BM_GET_lOOP_DELAY_CMD	  11
#define BM_GET_FETCH_DELAY_CMD	  12
#define OM_GET_DELAY_CMD	  13
#define BM_GET_FAILURE_DELAY_CMD  14
#define BM_PUT_DELAY_CMD	  15
#define PROVIDER_OFFSET_CMD	  16
#define IP_OFFSET_CMD		  17
#define DOMAIN_OFFSET_CMD	  18
#define TIME_OFFSET_CMD		  19
#define URI_OFFSET_CMD		  20
#define RESP_OFFSET_CMD		  21
#define RESP_SIZE_OFFSET_CMD	  22
#define RANGE_OFFSET_CMD	  23
#define	CONTENT_LENGTH_OFFSET_CMD 24
#define CACHE_CONTROL_OFFSET_CMD  25
#define AM_EXTRA_STRUCT_SIZE_CMD  26
#define SMALL_BLOCK_CMD		  27
#define NUMBER_OF_DISKS_CMD	  28
#define NUMBER_OF_SATAS_CMD	  29
#define NUMBER_OF_SSDS_CMD	  30
#define NUMBER_OF_SASS_CMD	  31
#define SSD_BLOCK_SIZE_CMD	  32
#define SAS_BLOCK_SIZE_CMD	  33
#define SATA_BLOCK_SIZE_CMD	  34
#define SMALL_READ_CMD		  35
#define SMALL_READ_SIZE_CMD	  36
#define BUFFER_HOLD_TIME_CMD	  37
#define PARALLEL_INGESTS_CMD      38
#define BUFFER_HOLD_PERCENTAGE_CMD 39
#define DM_PUT_DELAY_CMD	  40
#define DM_GET_DELAY_CMD	  41
#define SIZE_BASED_SELECTION_CMD  42
#define BLOCKS_TO_DELETE_CMD	  43
#define DISK_EVICT_THRESHOLD_CMD  44
#define SSD_MIN_SIZE_CMD	  45
#define SAS_MIN_SIZE_CMD	  46
#define SATA_MIN_SIZE_CMD	  47
#define SSD_HIGH_MARK_CMD	  48
#define SAS_HIGH_MARK_CMD         49
#define SATA_HIGH_MARK_CMD        50
#define SSD_LOW_MARK_CMD	  51
#define SAS_LOW_MARK_CMD          52
#define SATA_LOW_MARK_CMD	  53
#define OFFLINE_INGEST_MODE_CMD	  54
#define CMD_MAX_CONFIG (OFFLINE_INGEST_MODE_CMD + 1)

#define MAX_CONF_LINE_LEN 1024

/* Config params */
extern uint64_t num_ingest_entries;
extern uint64_t sata_disk_size;
extern uint64_t sas_disk_size;
extern uint64_t ssd_disk_size;
extern uint64_t ingest_rate;
extern uint64_t evict_rate;
extern uint64_t ram_size;
extern uint64_t am_size;
extern uint64_t ingest_size_threshold;
extern uint64_t ssd_size_threshold;
extern uint64_t origin_fetch_number;
extern uint64_t am_extra_struct_size;
extern uint64_t small_block;
extern uint64_t number_of_disks;
extern uint64_t number_of_SATAs;
extern uint64_t number_of_SSDs;
extern uint64_t number_of_SASs;
extern uint64_t ssd_block_size;
extern uint64_t sas_block_size;
extern uint64_t sata_block_size;
extern int small_read;
extern uint64_t small_read_size;
extern uint64_t buffer_hold_time;
extern uint64_t parallel_ingests;
extern uint64_t buffer_hold_percentage;
extern uint64_t dm_put_delay;
extern uint64_t dm_get_delay;
extern int size_based_selection;
extern uint64_t blocks_to_delete;
extern int disk_evict_threshold;
extern int ssd_min_size;
extern int sas_min_size;
extern int sata_min_size;
extern int ssd_high_mark;
extern int sas_high_mark;
extern int sata_high_mark;
extern int ssd_low_mark;
extern int sas_low_mark;
extern int sata_low_mark;
extern int offline_ingest_mode;

extern int provider_offset;
extern int ip_offset;
extern int domain_offset;
extern int time_offset;
extern int uri_offset;
extern int resp_offset;
extern int resp_size_offset;
//int range_offset = 12;
extern int range_offset;
extern int content_length_offset;
extern int cache_control_offset;


/* Config parser */
void parse_config(FILE *fp);

#endif /* _CONFIG_PARSER_H */

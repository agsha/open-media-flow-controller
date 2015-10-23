/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which parses the configuration
 * file
 *
 * Author: Jeya ganesh babu
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <glib.h>
//#include "nkn_defs.h"
//#include "nkn_am_api.h"
//#include "log_parser.h"
//#include "simulator.h"
#include "config_parser.h"

/* Config params */
uint64_t num_ingest_entries;
uint64_t sata_disk_size;
uint64_t sas_disk_size;
uint64_t ssd_disk_size;
uint64_t ingest_rate;
uint64_t evict_rate;
uint64_t ram_size;
uint64_t am_size;
uint64_t ingest_size_threshold;
uint64_t ssd_size_threshold;
uint64_t origin_fetch_number;
uint64_t bm_get_loop_delay;
uint64_t bm_get_fetch_delay;
uint64_t om_get_delay;
uint64_t bm_put_delay;
uint64_t bm_get_failure_delay;
uint64_t am_extra_struct_size;
uint64_t small_block;
uint64_t number_of_disks;
uint64_t number_of_SATAs;
uint64_t number_of_SSDs;
uint64_t number_of_SASs;
uint64_t ssd_block_size;
uint64_t sas_block_size;
uint64_t sata_block_size;
int small_read;
uint64_t small_read_size;
uint64_t buffer_hold_time;
uint64_t parallel_ingests;
uint64_t buffer_hold_percentage;
uint64_t dm_put_delay;
uint64_t dm_get_delay;
int size_based_selection;
uint64_t blocks_to_delete;
int disk_evict_threshold;
int ssd_min_size;
int sas_min_size;
int sata_min_size;
int ssd_high_mark;
int sas_high_mark;
int sata_high_mark;
int ssd_low_mark;
int sas_low_mark;
int sata_low_mark;
int offline_ingest_mode;

int provider_offset;
int ip_offset;
int domain_offset;
int time_offset;
int uri_offset;
int resp_offset;
int resp_size_offset;
//int range_offset = 12;
int range_offset;
int content_length_offset;
int cache_control_offset;


char cmd_string[CMD_MAX_CONFIG][128] = {
    "Num_ingest_entries",
    "Ingest_rate",
    "Evict_rate",
    "Sata_Disk_size",
    "Sas_Disk_size",
    "Ssd_Disk_size",
    "Ram_size",
    "AM_size",
    "Ingest_size_threshold",
    "SSD_size_threshold",
    "Origin_fetch_number",
    "BM_get_loop_delay",
    "BM_get_fetch_delay",
    "OM_get_delay",
    "BM_get_failure_delay",
    "BM_put_delay",
    "Provider_offset",
    "Ip_offset",
    "Domain_offset",
    "Time_offset",
    "Uri_offset",
    "Resp_offset",
    "Resp_size_offset",
    "Range_offset",
    "Content_length_offset",
    "Cache_control_offset",
    "am_extra_struct_size",
    "small_block",
    "number_of_disks",
    "number_of_SATAs",
    "number_of_SSDs",
    "number_of_SASs",
    "Ssd_block_size",
    "Sas_block_size",
    "Sata_block_size",
    "small_read",
    "small_read_size",
    "buffer_hold_time",
    "parallel_ingests",
    "buffer_hold_percentage",
    "dm_put_delay",
    "dm_get_delay",
    "size_based_selection",
    "blocks_to_delete",
    "disk_evict_threshold",
    "ssd_min_size",
    "sas_min_size",
    "sata_min_size",
    "ssd_high_mark",
    "sas_high_mark",
    "sata_high_mark",
    "ssd_low_mark",
    "sas_low_mark",
    "sata_low_mark",
    "offline_ingest_mode",
};


static int find_cmd (char * tmp_string)
{
    int cmd = 0;
    for(cmd = 0; cmd < CMD_MAX_CONFIG; cmd++)
    {
        if(!strcasecmp(tmp_string, cmd_string[cmd]))
            return cmd;
    }
    return -1;
}
static void remove_white_space(char *parse_string)
{
    char *tmp_string = parse_string;
    while(*tmp_string == ' '){
        tmp_string++;
    };
    if(tmp_string != parse_string) {
        memmove(parse_string, tmp_string, (strlen(parse_string) - (tmp_string - parse_string)));
    }
}

void parse_config(FILE *fp)
{
    char parse_string[MAX_CONF_LINE_LEN];
    char *tmp_string;
    int cmd_type;
    while(fgets(parse_string, MAX_CONF_LINE_LEN, fp) != NULL) {
        if(parse_string[strlen(parse_string) - 1] == '\n')
            parse_string[strlen(parse_string) - 1] = '\0';
        remove_white_space(parse_string);
        if(*parse_string == '#') {
                continue;
        }
        tmp_string = strtok(parse_string, " ");
        cmd_type = find_cmd(tmp_string);
        while((tmp_string = strtok(NULL, " +-")) != NULL) {

            switch(cmd_type) {
                case MAX_INGEST_ENTRIES_CMD:
                        num_ingest_entries = atoll(tmp_string);
			//printf("Num_ingest_entries = %ld\n", num_ingest_entries);
                        break;
                case INGEST_RATE_CMD:
			ingest_rate = atoll(tmp_string);
			//printf("Ingest_rate = %ld\n", ingest_rate);
                        break;
                case EVICT_RATE_CMD:
			evict_rate = atoll(tmp_string);
			//printf("Evict_rate = %ld\n", evict_rate);
                        break;
                case SATA_DISK_SIZE_CMD:
			sata_disk_size = atoll(tmp_string);
			//printf("Disk_size = %ld\n", disk_size);
                        break;
		case SAS_DISK_SIZE_CMD:
			sas_disk_size = atoll(tmp_string);
			break;
		case SSD_DISK_SIZE_CMD:
			ssd_disk_size = atoll(tmp_string);
			break;
		case RAM_SIZE_CMD:
			ram_size = atoll(tmp_string);
			break;
		case AM_SIZE_CMD:
			am_size = atoll(tmp_string);
			break;
		case INGEST_SIZE_CMD:
			ingest_size_threshold = atoll(tmp_string);
			break;
		case SSD_SIZE_CMD:
			ssd_size_threshold = atoll(tmp_string);
			break;
		case ORIGIN_FETCH_NUMBER_CMD:
			origin_fetch_number = atoll(tmp_string);
			break;
		case BM_GET_lOOP_DELAY_CMD:
			bm_get_loop_delay = atoll(tmp_string);
			break;
		case BM_GET_FETCH_DELAY_CMD:
			bm_get_fetch_delay = atoll(tmp_string);
			break;
		case OM_GET_DELAY_CMD:
			om_get_delay = atoll(tmp_string);
			break;
		case BM_PUT_DELAY_CMD:
			bm_put_delay = atoll(tmp_string);
			break;
		case BM_GET_FAILURE_DELAY_CMD:
			bm_get_failure_delay = atoll(tmp_string);
			break;
		case PROVIDER_OFFSET_CMD:
			provider_offset = atoll(tmp_string);
			break;
		case IP_OFFSET_CMD:
			ip_offset = atoll(tmp_string);
			break;
		case DOMAIN_OFFSET_CMD:
			domain_offset = atoll(tmp_string);
			break;
		case TIME_OFFSET_CMD:
			time_offset = atoll(tmp_string);
			break;
		case URI_OFFSET_CMD:
			uri_offset = atoll(tmp_string);
			break;
		case RESP_OFFSET_CMD:
			resp_offset = atoll(tmp_string);
			break;
		case RESP_SIZE_OFFSET_CMD:
			resp_size_offset = atoll(tmp_string);
			break;
		case RANGE_OFFSET_CMD:
			range_offset = atoll(tmp_string);
			break;
		case CONTENT_LENGTH_OFFSET_CMD:
			content_length_offset = atoll(tmp_string);
			break;
		case CACHE_CONTROL_OFFSET_CMD:
			cache_control_offset = atoll(tmp_string);
			break;
		case AM_EXTRA_STRUCT_SIZE_CMD:
			am_extra_struct_size = atoll(tmp_string);
			break;
		case SMALL_BLOCK_CMD:
			small_block = atoll(tmp_string);
			break;
		case NUMBER_OF_DISKS_CMD:
			number_of_disks = atoll(tmp_string);
			break;
		case NUMBER_OF_SATAS_CMD:
			number_of_SATAs = atoll(tmp_string);
			break;
		case NUMBER_OF_SSDS_CMD:
			number_of_SSDs = atoll(tmp_string);
			break;
		case NUMBER_OF_SASS_CMD:
			number_of_SASs = atoll(tmp_string);
			break;
		case SSD_BLOCK_SIZE_CMD:
			ssd_block_size = atoll(tmp_string);
			break;
		case SAS_BLOCK_SIZE_CMD:
			sas_block_size = atoll(tmp_string);
			break;
		case SATA_BLOCK_SIZE_CMD:
			sata_block_size = atoll(tmp_string);
			break;
		case SMALL_READ_CMD:
			small_read = atoll(tmp_string);
			break;
		case SMALL_READ_SIZE_CMD:
			small_read_size = atoll(tmp_string);
			break;
		case BUFFER_HOLD_TIME_CMD:
			buffer_hold_time = atoll(tmp_string);
			break;
		case PARALLEL_INGESTS_CMD:
			parallel_ingests = atoll(tmp_string);
			break;
		case BUFFER_HOLD_PERCENTAGE_CMD:
			buffer_hold_percentage = atoll(tmp_string);
			break;
		case DM_PUT_DELAY_CMD:
			dm_put_delay = atoll(tmp_string);
			break;
		case DM_GET_DELAY_CMD:
			dm_get_delay = atoll(tmp_string);
			break;
		case SIZE_BASED_SELECTION_CMD:
			size_based_selection = atoll(tmp_string);
			break;
		case BLOCKS_TO_DELETE_CMD:
			blocks_to_delete = atoll(tmp_string);
			break;
		case DISK_EVICT_THRESHOLD_CMD:
			disk_evict_threshold = atoll(tmp_string);
			break;
		case SSD_MIN_SIZE_CMD:
			ssd_min_size = atoll(tmp_string);
			break;
		case SAS_MIN_SIZE_CMD:
			sas_min_size = atoll(tmp_string);
			break;
		case SATA_MIN_SIZE_CMD:
			sata_min_size = atoll(tmp_string);
			break;
		case SSD_HIGH_MARK_CMD:
			ssd_high_mark = atoll(tmp_string);
			break;
		case SAS_HIGH_MARK_CMD:
			sas_high_mark = atoll(tmp_string);
			break;
		case SATA_HIGH_MARK_CMD:
			sata_high_mark = atoll(tmp_string);
			break;
		case SSD_LOW_MARK_CMD:
			ssd_low_mark = atoll(tmp_string);
			break;
		case SAS_LOW_MARK_CMD:
			sas_low_mark = atoll(tmp_string);
			break;
		case SATA_LOW_MARK_CMD:
			sata_low_mark = atoll(tmp_string);
			break;
		case OFFLINE_INGEST_MODE_CMD:
			offline_ingest_mode = atoll(tmp_string);
			break;
		default:
                        break;
            }
        }
    }
}

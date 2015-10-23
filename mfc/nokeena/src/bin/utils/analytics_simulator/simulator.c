/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Core modeler
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
#include "nkn_defs.h"
#include "nkn_am_api.h"
#include "log_parser.h"
#include "config_parser.h"
#include "simulator.h"

time_t nkn_cur_ts;
int nkn_assert_enable = 0;


/* Statistics */
uint64_t total_bytes_delivered;
uint64_t total_bytes_in_ram;
uint64_t total_bytes_in_disk;
uint64_t total_dup_bytes_delivered;
uint64_t total_num_requests;
uint64_t total_dup_requests;
uint64_t total_num_cacheable_entries;
uint64_t cache_hit_ratio;
FILE *log_fp = NULL;

TAILQ_HEAD(hotness_bucket_t, uri_entry_s)
    hotness_bucket[MAX_HOT_VAL];
uint64_t num_hot_objects[MAX_HOT_VAL];

static void update_hotness_bucket(uri_entry_t *objp, nkn_hot_t new_hotness)
{
    int old_hot_val = am_decode_hotness(&objp->hotness);
    int new_hot_val = am_decode_hotness(&new_hotness);

    if(old_hot_val) {
	TAILQ_REMOVE(&hotness_bucket[old_hot_val], objp, hotness_entries);
	num_hot_objects[old_hot_val]--;
    }
    if(new_hot_val) {
	TAILQ_INSERT_HEAD(&hotness_bucket[new_hot_val], objp, hotness_entries);
	num_hot_objects[new_hot_val]++;
    }
}

static void hotness_update(uri_entry_t *objp)
{
    nkn_hot_t new_hotness;

    new_hotness = am_update_hotness(&objp->hotness, 1, objp->last_access_time,
				    0, objp->uri_name);
    update_hotness_bucket(objp, new_hotness);
    objp->hotness = new_hotness;
}

#define MAX_URI_LEN 2048
static void process_logs(char *log_file)
{
    uint64_t content_length, start_offset, end_offset;
    int resp, cacheable, provider;
    time_t acs_time;
    char uri[20480];
    GHashTable *uri_hash_table = NULL;
    uri_entry_t *objp;
    int i;

    g_thread_init(NULL);
    uri_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    for(i=0;i<MAX_HOT_VAL;i++)
	TAILQ_INIT(&hotness_bucket[i]);

    /* Input */
    while(!get_next_entry(log_file, uri, &content_length, &resp,
		   &start_offset, &end_offset, &provider, &cacheable, &acs_time)) {
	/* Runtime Statistics */
	if(!(line_no % 1000000))
	    fprintf(log_fp, "Cache_hit_ratio = %ld\n", cache_hit_ratio);
	if(!uri[0])
	    continue;
	if(!acs_time)
	    continue;
	total_num_requests++;
	objp = g_hash_table_lookup(uri_hash_table, uri);
	if(!objp) {
	    if(cacheable) {
		objp = calloc(1, MAX_URI_LEN);
		objp->uri_name = malloc(strlen(uri) + 1);
		strcpy(objp->uri_name, uri);
		*(uint64_t *)&uri[0] = 0;
		g_hash_table_insert(uri_hash_table, objp->uri_name, objp);
		objp->state = ENTRY_NEW;
	    }
	} else {
	    total_dup_requests++;
	}
	total_bytes_delivered += content_length;
	if(cacheable) {
	    if(objp->state & ENTRY_NEW) {
		total_num_cacheable_entries++;
		total_bytes_in_ram += content_length;
		objp->state = ENTRY_NOT_INGESTED | ENTRY_IN_RAM;
	    } else {
		if(!(objp->state & ENTRY_EVICTED)) {
		    /* Duplicate Hit */
		    if(resp == 200) {
			total_dup_bytes_delivered += content_length;
		    } else if(resp == 206) {
			if(start_offset && (start_offset > objp->start_offset) &&
				(start_offset < objp->end_offset) &&
				(end_offset > objp->end_offset))
			    total_dup_bytes_delivered += (objp->end_offset - start_offset);
			if(start_offset && (start_offset > objp->start_offset) &&
				(start_offset < objp->end_offset) &&
				(end_offset < objp->end_offset))
			    total_dup_bytes_delivered += (end_offset - start_offset);
			if(start_offset && (start_offset < objp->start_offset) &&
				(end_offset > objp->start_offset) &&
				(end_offset < objp->end_offset))
			    total_dup_bytes_delivered += (end_offset - objp->start_offset);
			if(start_offset && (start_offset < objp->start_offset) &&
				(end_offset > objp->start_offset) &&
				(end_offset > objp->end_offset))
			    total_dup_bytes_delivered += (objp->end_offset - objp->start_offset);
		    }
		    cache_hit_ratio = (total_dup_bytes_delivered * 100) /
				    (total_bytes_delivered);
		} else {
		    objp->state = ENTRY_NOT_INGESTED | ENTRY_IN_RAM;
		    objp->hotness = 0;
		}
	    }

	    /* Update Access time and offsets */
	    objp->last_access_time = acs_time;
	    nkn_cur_ts = acs_time;
	    if(resp == 206) {
		if(start_offset && start_offset < objp->start_offset)
		    objp->start_offset = start_offset;
		if(end_offset && end_offset > objp->end_offset)
		    objp->end_offset = end_offset;
	    }
	    if(resp == 200) {
		objp->start_offset = 0;
		objp->end_offset = content_length;
	    }

	    /* Analytics plugin */
	    hotness_update(objp);

	    if(objp->state & ENTRY_NOT_INGESTED) {
		/* Ingest plugin */
		//total_bytes_in_cache += ingest(objp);
	    }
	}
	/* Ram Eviction plugin */
	/* Disk Evict plugin */

    }
    fprintf(log_fp, "\nTotal_bytes_delivered = %ld\n", total_bytes_delivered);
    fprintf(log_fp, "Total_bytes_in_ram = %ld\n", total_bytes_in_ram);
    fprintf(log_fp, "Total_num_requests = %ld\n", total_num_requests);
    fprintf(log_fp, "Total_num_dup_requests = %ld\n", total_dup_requests);
    fprintf(log_fp, "Total_num_cacheable_entries = %ld\n", total_num_cacheable_entries);
    fprintf(log_fp, "Total_dup_bytes_delivered = %ld\n", total_dup_bytes_delivered);
    fprintf(log_fp, "cache_hit_ratio = %ld\n", cache_hit_ratio);
    fprintf(log_fp, "Hotness Table \n");
    fprintf(log_fp, "-----------------------------\n");
    fprintf(log_fp, "Hotness\tNum. objects\n");
    for(i=0;i<MAX_HOT_VAL;i++) {
	if(num_hot_objects[i])
	    fprintf(log_fp, "%d\t%ld\n", i, num_hot_objects[i]);
    }
    fprintf(log_fp, "-----------------------------\n");

    /* Post Process */
}

int main(int argc, char **argv)
{
    char config_file[255];
    char log_file[255] = {0};
    char accesslog_file[255];
    int c;
    FILE *config_fp = NULL;

    while(1) {
	c = getopt(argc, argv, "c:a:l:");
	if(c < 0)
	    break;
	switch(c) {
	    case 'c':
		strcpy(config_file, optarg);
		break;
	    case 'a':
		strcpy(accesslog_file, optarg);
		break;
	    case 'l':
		strcpy(log_file, optarg);
		break;
	    default:
		printf("Invalid argument\n");
		printf("Usage: analytics_simulator -c <config file> -a <accesslog> -l <log file>\n");
		break;
	}
    }
    if(config_file[0]  == '\0' || accesslog_file[0] == '\0') {
	printf("Usage: analytics_simulator -c <config file> -a <accesslog> -l <log file>\n");
	return 0;
    }

    printf("Config file = %s\n", config_file);
    config_fp = fopen(config_file, "r");
    if(config_fp == NULL) {
	printf("Unable to open config file %s\n", config_file);
        return 0;
    }

    /* Parse the config file */
    printf("Parsing Config file \"%s\".....", config_file);
    parse_config(config_fp);
    fclose(config_fp);
    printf("Done\n");

    if(log_file[0]) {
	log_fp = fopen(log_file, "w");
	if(log_fp == NULL) {
	    printf("Unable to open log file %s\n", log_file);
	    return 0;
	}
	printf("log file = %s\n", log_file);
    } else {
	log_fp = stdout;
	printf("log file = stdout\n");
    }

    printf("accesslog file = %s\n", accesslog_file);
    process_logs(accesslog_file);

    fclose(log_fp);
    return 0;
}


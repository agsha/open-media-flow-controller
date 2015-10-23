/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the access log
 * parser
 *
 * Author: Jeya ganesh babu
 *
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
//#include "nkn_defs.h"
#include "config_parser.h"
#include "log_parser.h"

FILE *fp;

//int provider_offset = 0;
//int ip_offset = 1;
//int domain_offset = 2;
//int time_offset = 4;
//int uri_offset = 5;
//int resp_offset = 6;
//int resp_size_offset = 7;
//int range_offset = 12;
//int range_offset = 14;
//int content_length_offset = 13;
//int cache_control_offset = 21;

static char *get_next_token(char *ptr)
{
    static char *orig_ptr = NULL;
    char *ret_ptr = NULL;
    int delim_started = 0;

    if(ptr) {
	orig_ptr = ptr;
    }

    if(!orig_ptr)
	return NULL;

    while(*orig_ptr == ' ') {
	orig_ptr++;
    }

    if(*orig_ptr == '\"' || *orig_ptr == '[') {
	delim_started = 1;
	orig_ptr++;
	if(*orig_ptr == '\"') {
	    delim_started = 2;
	    orig_ptr++;
	}
    }
    ret_ptr = orig_ptr;

    while(*orig_ptr != '\0') {
	if(delim_started) {
	    if(*orig_ptr == '\"' || *orig_ptr == ']') {
		*orig_ptr = '\0';
		orig_ptr++;
		if(delim_started == 2)
		    orig_ptr++;
		return ret_ptr;
	    }
	} else {
	    if(*orig_ptr == ' ' || *orig_ptr == '\n') {
		*orig_ptr = '\0';
		orig_ptr++;
		return ret_ptr;
	    }
	}
	orig_ptr++;
    }
    return NULL;
}

uint64_t line_no = 0;
int get_next_entry(char *log_file, char *uri, uint64_t *content_len, int *resp,
		   uint64_t *start_offset, uint64_t *end_offset, int *provider,
		   int *cacheable, time_t *acs_time, int *partial)
{
    char line[20480];
    char *linep = NULL;
    char *tmp_string = NULL;
    int parse_offset = 0;
    char req_file[20480], method[32], http_id[32];
    memset(req_file,0,sizeof(req_file));
    memset(line,0,sizeof(line));
    memset(method, 0, sizeof(method));
    memset(http_id, 0, sizeof(http_id));
    static char running_char = '|';
    struct tm time_val = { 0 };

    *content_len = 0;
    *resp = 0;
    *start_offset = 0;
    *end_offset = 0;
    *provider = 0;
    *cacheable = 1;
    *acs_time = 0;

    if(!fp) {
	fp = fopen(log_file,"r");
	printf("#|");
	if(!fp)
	    return errno;
	rewind(fp);
    }
next_line:;
    if(feof(fp))
	return -1;
    linep = fgets(line, 20480, fp);
    if(!linep)
	return -1;

    //*uri = malloc(strlen(linep));
    line_no++;
    if(!(line_no % 10000)) {
	printf("");
	if(!(line_no % 1000000))
	    printf("#");
	printf("%c", running_char);
	if(running_char == '|')
	    running_char = '/';
	else if(running_char == '/')
	    running_char = '-';
	else if(running_char == '-')
	    running_char = '\\';
	else if(running_char == '\\')
	    running_char = '|';
	fflush(stdout);
    }

    if(*linep == '#') {
	if(!(memcmp(linep, "#Fields", 7))) {
	    //printf("Format line present\n");
	}
        goto next_line;
    }

    tmp_string = get_next_token(linep);
    parse_offset = 0;
    while(tmp_string) {
	if(parse_offset == provider_offset) {
//	    printf("provider = %s\n", tmp_string);
	    if(strstr(tmp_string, "Tunnel")) {
		*cacheable = 0;
	    }
	    if(tmp_string[0] != 'T' && tmp_string[0] != 'B' &&
		    tmp_string[0] != 'O' && tmp_string[0] != 'N' &&
		    tmp_string[0] != 'S' && tmp_string[0] != 'i')
		return 0;
	} else if(parse_offset == domain_offset) {
//	    printf("uri = %s\n", tmp_string);
	    sprintf(uri, "%s:", tmp_string);
	} else if(parse_offset == time_offset) {
	    if(tmp_string[0] < '0' || tmp_string[0] > '9')
		return 0;
	    strptime(tmp_string, "%d/%b/%Y:%H:%M:%S %z", &time_val);
	    *acs_time = mktime(&time_val);
//	    printf("time = %s\n", tmp_string);
	} else if(parse_offset == uri_offset) {
	    if(tmp_string) {
	//	sscanf(tmp_string, "%s %s %s", method, req_file, http_id);
			sscanf(tmp_string, "%s",req_file);
//	    printf("method = %s\n", method);
//	    printf("req_file = %s\n", req_file);
//	    printf("http_id = %s\n", http_id);
		strcat(uri, req_file);
//	    printf("uri %s\n", *uri);
	    }
	} else if(parse_offset == resp_offset) {
	    *resp = atoi(tmp_string);
//	    printf("resp = %s\n", tmp_string);
	} else if(parse_offset == range_offset) {
	    if(!memcmp(tmp_string, "bytes=", 6)) {
		sscanf(tmp_string, "bytes=%ld-%ld", start_offset, end_offset);
		*partial = 1;
	    }
//	    printf("range = %s\n", tmp_string);
	} else if(parse_offset == resp_size_offset) {
	    *content_len = atoi(tmp_string);
//	    printf("content_length = %s\n", tmp_string);
	} else if(parse_offset == cache_control_offset) {
	    if(strstr(tmp_string, "no_cache") || strstr(tmp_string, "private")) {
		*cacheable = 0;
	    }
//	    printf("cache_control = %s\n", tmp_string);
	}
	tmp_string = get_next_token(NULL);
	if(!tmp_string)
	    return 0;
	parse_offset ++;
    }
    return 0;

}


/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Access log parser header file
 *
 * Author: Jeya ganesh babu
 *
 */

#ifndef _LOG_PARSER_H
#define _LOG_PARSER_H
#include <stdint.h>

extern uint64_t line_no;
int get_next_entry(char *log_file, char *uri, uint64_t *content, int *resp,
		   uint64_t *start_offset, uint64_t *end_offset, int *provider,
		   int *cacheable, time_t *acs_time, int *partial);

#endif /* _LOG_PARSER_H */

/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains ICCP client related defines
 *
 * Author: Jeya ganesh babu
 *
 */
#ifndef _NKN_ICCP_CLI_H
#define _NKN_ICCP_CLI_H

#include "nkn_iccp.h"

void iccp_cli_init(void(*cli_callback_func)(void *));
int iccp_send_task(iccp_info_t *iccp_info);
void * iccp_cli_thrd(void *arg __attribute((unused)));
int iccp_return_and_reset_cim_status(void);
#endif /* _NKN_ICCP_CLI_H */

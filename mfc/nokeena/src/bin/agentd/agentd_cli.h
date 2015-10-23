/*
 * Filename :   agentd_cli.c
 * Date:        26 July 2012
 * Author:      Lakshmi 
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef __AGENTD_CLI_H__
#define __AGENTD_CLI_H__

int agentd_execute_cli_commands (const tstr_array *commands, tstring **ret_errors, tstring **ret_output,
                                        tbool *ret_success);
#endif //__AGENTD_CLI_H__

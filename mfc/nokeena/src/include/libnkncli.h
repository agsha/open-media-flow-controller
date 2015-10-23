/*
 *
 * Filename:  libnkncli.h
 * Date:      2010/01/27 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-10 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef __LIBNKNCLI__H
#define __LIBNKNCLI__H
#include "clish_module.h"
int 
nkn_clish_run_program_fg(const char *abs_path, const tstr_array *argv,
                         const char *working_dir,
                         clish_termination_handler_func termination_handler,
                         void *termination_handler_data);

tbool
nkn_cli_is_policy_bound(const char *policy, 
		tstring **associated_ns, 
		const char *ns_active_check, 
		const char *str_silent);

int
cli_nvsd_show_cluster(const char *cluster, int from_namespace);


#endif /* ndef __LIBNKNCLI__H */

/* End of libnkncli.h */

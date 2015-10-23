#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <tcl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "stdint.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_namespace.h"
#include "pe_defs.h"

NKNCNT_DEF(pe_tot_ilist, AO_t, "", "")
NKNCNT_DEF(pe_tot_tcl_interp, AO_t, "", "")
NKNCNT_DEF(pe_file_read_cnt, AO_t, "", "")
NKNCNT_DEF(pe_file_read_err, AO_t, "", "")
NKNCNT_DEF(pe_create_cnt, AO_t, "", "")
NKNCNT_DEF(pe_delete_cnt, AO_t, "", "")
NKNCNT_DEF(pe_cleanup_cnt, AO_t, "", "")


static pthread_mutex_t pe_create_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *pe_read_policy_file(char * pe_name, int *buf_length);
static int pe_scan_policy(char *buf, int buf_length);

pe_ilist_t * pe_create_ilist(void)
{
	pe_ilist_t *p_list;

	p_list = (pe_ilist_t *)nkn_malloc_type(sizeof(pe_ilist_t), mod_pe_ilist_t);
	if (p_list) {
		memset(p_list, 0, sizeof(pe_ilist_t));
		pthread_mutex_init(&p_list->pe_list_mutex, NULL);
		AO_fetch_and_add1(&glob_pe_tot_ilist);
	}
	return p_list;
}

#if 0
		char *pe_buf = NULL;
		pe_ilist_t * p_list = NULL;

		pe_buf = ocon->oscfg.nsc->http_config->policy_engine_config.policy_buf;
		if (pe_buf == NULL) {
			pe_buf = pe_read_policy_file(ocon->oscfg.nsc->http_config->policy_engine_config.policy_file);
			ocon->oscfg.nsc->http_config->policy_engine_config.policy_buf = pe_buf;
		}

		p_list = (pe_ilist_t *) ocon->oscfg.nsc->http_config->policy_engine_config.policy_data;
		if (p_list == NULL) {
			p_list = pe_create_ilist();
			ocon->oscfg.nsc->http_config->policy_engine_config.policy_data = p_list;
		}
#endif

/*
 * The following two functions are common for all Policy Engine
 * External call function
 */
pe_rules_t * pe_create_rule(policy_engine_config_t *pe_config, pe_ilist_t **pp_list)
{
	pe_rules_t * p_perule = NULL;
	int ret;
	pe_ilist_t * p_list = NULL;
	static uint32_t g_file_id = 0;
	int file_read = 0;
	int buf_length = 0;

	if (pe_config->policy_file == NULL) {
		return NULL;
	}

	p_list = (pe_ilist_t *) pe_config->policy_data;
	if (p_list == NULL) {
		/* Get global lock while creating policy data for this namespace
		 */
		pthread_mutex_lock(&pe_create_mutex);
		/* While this thread was waiting for this mutex, policy_data
		 * could have been created by other thread
		 */
		if (pe_config->policy_data == NULL) {
			p_list = pe_create_ilist();
			pe_config->policy_data = p_list;
		}
		else {
			p_list = (pe_ilist_t *) pe_config->policy_data;
		}
		pthread_mutex_unlock(&pe_create_mutex);
	}
	*pp_list = p_list;

	pthread_mutex_lock(&p_list->pe_list_mutex);
	if (pe_config->update) {
		pe_config->update = 0;
		free(p_list->policy_buf);
		p_list->policy_buf = NULL;
	}
	if (p_list->policy_buf == NULL) {
		p_list->policy_buf = pe_read_policy_file(pe_config->policy_file, &buf_length);
		/* If unable to read file return error 
		 */
		if (p_list->policy_buf == NULL) {
			pthread_mutex_unlock(&p_list->pe_list_mutex);
			return NULL;
		}
		file_read = 1;
		g_file_id++;
		p_list->pe_flag = pe_scan_policy(p_list->policy_buf, buf_length);
	}

	if (file_read) {
		p_list->file_id = g_file_id;
	}
	
	/*
	 * Precompile the script into ruleobj
	 */
	if (p_list->p_rules_free_list) {
		p_perule = p_list->p_rules_free_list;
		p_list->p_rules_free_list = p_list->p_rules_free_list->next;
		p_perule->next = NULL;
		p_list->pe_num_free--;
		if (p_list->file_id == p_perule->file_id) {
			pthread_mutex_unlock(&p_list->pe_list_mutex);
			return p_perule;
		}
		else {
			/* Policy has been updated, delete rule instance
			 */
			Tcl_DecrRefCount(p_perule->ruleobj);
			Tcl_DeleteInterp(p_perule->tip);
			AO_fetch_and_sub1(&glob_pe_tot_tcl_interp);
			AO_fetch_and_add1(&glob_pe_delete_cnt);
			free(p_perule);
		}
	}
	p_perule = (pe_rules_t *)nkn_malloc_type(sizeof(pe_rules_t), mod_pe_rules_t);
	if(!p_perule) {
		pthread_mutex_unlock(&p_list->pe_list_mutex);
		return NULL;
	}
	p_perule->file_id = p_list->file_id;
	p_perule->pe_flag = p_list->pe_flag;
	p_perule->next = NULL;
	p_list->pe_num_interp++;

	p_perule->tip = Tcl_CreateInterp();
	Tcl_Init(p_perule->tip);
	AO_fetch_and_add1(&glob_pe_tot_tcl_interp);
	AO_fetch_and_add1(&glob_pe_create_cnt);

	// Create the rules object where the rule can be compiled and saved.
	p_perule->ruleobj = Tcl_NewStringObj(p_list->policy_buf, strlen(p_list->policy_buf));
	Tcl_IncrRefCount(p_perule->ruleobj);

	ret = Tcl_EvalObjEx(p_perule->tip, p_perule->ruleobj, 0);
	if ( ret != TCL_OK) {
		pthread_mutex_unlock(&p_list->pe_list_mutex);
		DBG_LOG(ERROR, MOD_HTTP, "pe_create_rule returns %d\n", ret);
		//glob_pe_eval_err_http_recv_req++;
		return NULL;
	}

	pthread_mutex_unlock(&p_list->pe_list_mutex);
	return p_perule;
}

void pe_free_rule(pe_rules_t * p_perule, pe_ilist_t *p_list)
{
	if (p_list == NULL) {
		Tcl_DecrRefCount(p_perule->ruleobj);
		Tcl_DeleteInterp(p_perule->tip);
		AO_fetch_and_sub1(&glob_pe_tot_tcl_interp);
		AO_fetch_and_add1(&glob_pe_delete_cnt);
		free(p_perule);
		return;
	}
	pthread_mutex_lock(&p_list->pe_list_mutex);
	if (p_list->pe_num_free > 16) {
		Tcl_DecrRefCount(p_perule->ruleobj);
		Tcl_DeleteInterp(p_perule->tip);
		AO_fetch_and_sub1(&glob_pe_tot_tcl_interp);
		AO_fetch_and_add1(&glob_pe_delete_cnt);
		free(p_perule);
		p_list->pe_num_interp--;
	}
	else {
		p_perule->next = p_list->p_rules_free_list;
		p_list->p_rules_free_list = p_perule;
		p_list->pe_num_free++;
	}
	pthread_mutex_unlock(&p_list->pe_list_mutex);
}

void pe_cleanup(void * plist)
{
	pe_ilist_t * p_list;
	pe_rules_t * p_perule, *p_perule_n;
	
	p_list = (pe_ilist_t *) plist;

	pthread_mutex_lock(&p_list->pe_list_mutex);
	p_perule = p_list->p_rules_free_list;
	while (p_list->pe_num_free && p_perule) {
		p_perule_n = p_perule->next;
		Tcl_DecrRefCount(p_perule->ruleobj);
		Tcl_DeleteInterp(p_perule->tip);
		AO_fetch_and_sub1(&glob_pe_tot_tcl_interp);
		AO_fetch_and_add1(&glob_pe_delete_cnt);
		free(p_perule);
		p_list->pe_num_free--;
		p_list->pe_num_interp--;
		p_perule = p_perule_n;
	}
	pthread_mutex_unlock(&p_list->pe_list_mutex);
	AO_fetch_and_sub1(&glob_pe_tot_ilist);
	AO_fetch_and_add1(&glob_pe_cleanup_cnt);
	free(p_list);
}

char *pe_read_policy_file(char * pe_name, int *buf_length)
{
	FILE * fp;
	char * script;
	size_t ret;
	struct stat bstat;
	int size = 0;
	char script_fn[1024];

	*buf_length = 0;
	/*
	 * Step 1: Load TCL script for each namespace.
	 * If namespace is NULL, load default global rule file.
	 */
	if(pe_name == NULL) {
		AO_fetch_and_add1(&glob_pe_file_read_err);
		return(NULL);
		//snprintf(script_fn, 1024, "/config/nkn/default_rule.tcl");
	}
	else {
		snprintf(script_fn, 1024, "%s", pe_name);
	}

	if (stat(script_fn, &bstat) == -1) {
		DBG_LOG(MSG, MOD_HTTP, "Policy file does not exist %s", script_fn);
		AO_fetch_and_add1(&glob_pe_file_read_err);
		return NULL; // File does not exist
	}
	size = bstat.st_size;
	script = (char *)nkn_malloc_type(sizeof(char) * (size + 1 + 40), 
			mod_pe_policy_buf);
	if (!script) {
		AO_fetch_and_add1(&glob_pe_file_read_err);
		return NULL;
	}

	fp = fopen(script_fn, "r");
	if (fp == NULL) {
		DBG_LOG(MSG, MOD_HTTP, "Cannot open script file %s", script_fn);
		free(script);
		AO_fetch_and_add1(&glob_pe_file_read_err);
		return NULL;
	}
	ret = fread(script, size, 1, fp);
	fclose(fp);
	AO_fetch_and_add1(&glob_pe_file_read_cnt);

	script[size] = 0;
	//snprintf(&script[size], 40, "\nset rule [%s];\n", proc_name);
	DBG_LOG(MSG, MOD_HTTP, "script=<%s>\n", script);

	*buf_length = size;
	return script;
}

int pe_scan_policy(char *buf, int buf_length)
{
	int flag = 0;
	int pos = 0;
	//int level = 0;
	int proc_num = 0;

	while (pos < buf_length) {
		switch (buf[pos]) {
		case '#':
			while (buf[pos] != '\n')
				pos++;
			break;
#if 0			
		case '{':
			level++;
			break;
		case '}':
			level--;
			break;
#endif			
		case '"':
			while (buf[pos] != '"')
				pos++;
			break;
		default:
			if (isalpha(buf[pos])) {
				if ((buf[pos] == 'p' || buf[pos] == 'P') && 
						(strncasecmp(&buf[pos], "pe_", 3) == 0)) {
					if (strncasecmp(&buf[pos], "pe_http_recv_request", 20) == 0) {
						proc_num = 1;
						pos += 20;
					}
					else if (strncasecmp(&buf[pos], "pe_http_send_response", 21) == 0) {
						proc_num = 2;
						pos += 21;
					}
					else if (strncasecmp(&buf[pos], "pe_om_send_request", 18) == 0) {
						proc_num = 3;
						pos += 18;
					}
					else if (strncasecmp(&buf[pos], "pe_om_recv_response", 19) == 0) {
						proc_num = 4;
						pos += 19;
					}
					else if (strncasecmp(&buf[pos], "pe_cl_http_recv_request", 23) == 0) {
						proc_num = 5;
						pos += 23;
					}
				}
				else if (buf[pos] == 'g' || buf[pos] == 'G') {
					if (strncasecmp(&buf[pos], "getparam", 8) == 0) {
						flag |= 1 << proc_num;
						pos += 8;
					}
				}
				else if (buf[pos] == 's' || buf[pos] == 'S') {
					if (strncasecmp(&buf[pos], "setaction", 9) == 0) {
						flag |= 1 << proc_num;
						pos += 9;
					}
				}
			}
		}
		pos++;
	}
	return flag;
}


#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <md_client.h>

#include "nkn_defs.h"
#include "nkn_debug.h"


/* Temperory variables */
int   ts_db_debug_enable = 0;
int   ts_db_max_size = -1;
int   ts_db_max_age = -1;
int   ts_db_port_num = 80;
char *ts_db_server_name = (char*)"";
char *ts_db_searial_num = (char*)"";


////////////////////////////////////////////////////////////////////////
// General configuration parsing functions
////////////////////////////////////////////////////////////////////////

typedef void (* CFG_func)(char * cfg);

#define TYPE_INT 	1
#define TYPE_STRING 	2
#define TYPE_LONG 	3
#define TYPE_FUNC 	4
#define TYPE_IP	 	5

static struct nkncfg_def {
	const char *label;
	int type;
	void * paddr;
	const char *vdefault;
} cfgdef[] =
{
    { "ts_db.debug_enable",  TYPE_INT,  &ts_db_debug_enable, "0"},
    { "ts_db.max_size",  TYPE_INT,  &ts_db_max_size, "-1"}, /* In KB */
    { "ts_db.max_age",  TYPE_INT,  &ts_db_max_age, "-1"},  /* In days */
    { "ts_db.port_num",  TYPE_INT,  &ts_db_max_age, "80"}, 
    { "ts_db.server_name",  TYPE_STRING,  &ts_db_server_name, (char *)""},
    { "ts_db.serial_num",  TYPE_STRING,  &ts_db_searial_num,(char*) ""},

    { NULL,          TYPE_INT,    NULL,                NULL   }
};

//////////////////////////////////////////////////////////////////////////////
// Parse the configuration file
//////////////////////////////////////////////////////////////////////////////

static char * mark_end_of_line(char * p);
static char * mark_end_of_line(char * p)
{
	while (1) {
		if (*p == '\r' || *p == '\n' || *p == '\0') {
			*p = 0;
			return p;
		}
		p++;
	}

	return NULL;
}

static char * skip_to_value(char * p);
static char * skip_to_value(char * p)
{
	while (*p == ' '||*p == '\t') p++;
	if (*p != '=') return NULL;
	p++;
	while (*p == ' '||*p == '\t') p++;
	return p;
}

/*
 *  Read ucfltd config from file '/config/nkn/nkn.ucflt.conf' file.
 */
int read_ucflt_cfg(char * configfile);
int read_ucflt_cfg(char * configfile)
{
	FILE *fp;
	char *p;
	char buf[1024];
	int len, i;
	struct nkncfg_def *pcfgdef;

	// Read the configuration
	fp = fopen(configfile, "r");
	DBG_LOG(MSG, MOD_UCFLT,"opening UCFLT config file <%s>\n", configfile);
	if (!fp) {
		DBG_LOG(SEVERE, MOD_UCFLT,"ERROR: failed to open configure file <%s>\n", configfile);
		DBG_LOG(SEVERE, MOD_UCFLT,"       use all default configuration settings.\n\n");
		return 1;
	}

	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL) break;

		p = buf;
		if (*p == '#') continue;
		mark_end_of_line(p);

		for (i = 0; ;i++) {
			pcfgdef = &cfgdef[i];
			if (pcfgdef->label == NULL) break;

			len = strlen(pcfgdef->label);
			if (strncmp(buf, pcfgdef->label, len) == 0) {
				// found one match
				p = skip_to_value(&buf[len]);
				if (pcfgdef->type == TYPE_INT) {
					int *pint = (int *)pcfgdef->paddr;
					*pint = atoi(p);
				} else if (pcfgdef->type == TYPE_STRING) {
					char **pchar = (char **)pcfgdef->paddr;
					if (strlen(p) > 0) {
						*pchar = strdup(p);
					}
				} else if (pcfgdef->type == TYPE_LONG) {
					int64_t *plong = (int64_t *)pcfgdef->paddr;
					*plong = atol(p);
				} else if (pcfgdef->type == TYPE_FUNC) {
					(* (CFG_func)(pcfgdef->paddr))(p);
				} else if (pcfgdef->type == TYPE_IP) {
					uint32_t *plong = (uint32_t *)pcfgdef->paddr;
					*plong = inet_addr(p);
				}
				break;
			}
		}
	}

	DBG_LOG(MSG, MOD_UCFLT,"Read config parameters:  dbg_en:%d, db_max_sz:%d, db_max_Age:%d"
                                   "db_port:%d, db_svr:%s, db_sl_num:%s\n",
                                    ts_db_debug_enable ,
                                    ts_db_max_size ,
                                    ts_db_max_age,
                                    ts_db_port_num,
                                    ts_db_server_name, 
                                    ts_db_searial_num) ;

	fclose(fp);

	return 1;
}


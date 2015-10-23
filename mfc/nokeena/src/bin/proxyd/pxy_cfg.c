#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <md_client.h>
#include <license.h>

#include "pxy_interface.h"
#include "pxy_defs.h"
#include "pxy_network.h"
#include "nkn_cfg_params.h"

int use_client_ip=0;


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
	const char * label;
	int type;
	void * paddr;
	const char * vdefault;
} cfgdef[] = 
{
//    { "use_client_ip", TYPE_INT, &use_client_ip, NULL }, 

    { NULL,          TYPE_INT,    NULL,                NULL   }
};


//////////////////////////////////////////////////////////////////////////////
// Parse the configuration file
//////////////////////////////////////////////////////////////////////////////

static char * mark_end_of_line(char * p);
static char * mark_end_of_line(char * p)
{
	while(1) {
		if(*p=='\r' || *p=='\n' || *p=='\0') {
			*p=0;
			return p;
		}
		p++;
	}
	return NULL;
}

static char * skip_to_value(char * p);
static char * skip_to_value(char * p)
{
	while(*p==' '||*p=='\t') p++;
	if(*p!='=') return NULL;
	p++;
	while(*p==' '||*p=='\t') p++;
	return p;
}

int read_pxy_cfg(char * configfile)
{
	FILE * fp;
	char * p;
	char buf[1024];
	int len, i;
	struct nkncfg_def * pcfgdef;


	// Read the configuration
	fp=fopen(configfile, "r");
	if(!fp) {
		printf("\n");
		printf("ERROR: failed to open configure file <%s>\n", configfile);
		printf("       use all default configuration settings.\n\n");
		return 1;
	}

	while( !feof(fp) ) {
		if(fgets(buf, 1024, fp) == NULL) break;

		p=buf;
		if(*p=='#') continue;
		mark_end_of_line(p);
		//printf("%s\n",buf);

		for(i=0; ;i++) {
			pcfgdef=&cfgdef[i];
			if(pcfgdef->label == NULL) break;

			len = strlen(pcfgdef->label);
			if(strncmp(buf, pcfgdef->label, len) == 0) {
				// found one match
				p=skip_to_value(&buf[len]);
				if(pcfgdef->type==TYPE_INT) {
					int * pint = (int *)pcfgdef->paddr;
					* pint = atoi(p);
				} else if(pcfgdef->type==TYPE_STRING) {
					char ** pchar = (char **)pcfgdef->paddr;
					*pchar=strdup(p);
				} else if(pcfgdef->type==TYPE_LONG) {
					int64_t * plong = (int64_t *)pcfgdef->paddr;
					*plong=atol(p);
				} else if(pcfgdef->type==TYPE_FUNC) {
					(* (CFG_func)(pcfgdef->paddr))(p);
				} else if(pcfgdef->type==TYPE_IP) {
					uint32_t * plong = (uint32_t *)pcfgdef->paddr;
					*plong=inet_addr(p);
				}
				break;
			}
		}
	}
	fclose(fp);

	return 1;
}

/*
 * convert a hex string to an integrate.
 */
uint64_t convert_hexstr_2_int64(char * s)
{
        //0x1234567812345678
        uint64_t val=0;
        int i;

        if(s == NULL) return 0;

        for(i=0;i<16;i++) {
                if(*s==0) break;

                val <<= 4;
                if (*s<='9' && *s >='0') val += *s - '0';
                else if (*s<='F' && *s >='A') val += (*s - 'A') + 10;
                else if (*s<='f' && *s >='a') val += (*s - 'a') + 10;
                else break;

                s++;
        }
        return val;
}

/*
 * The function call has been made public for Bug 5527
 * The check config function is invoked only after the log config has
 * been updated from the database
 */
int check_pxy_cfg(void)
{
	return 0;
}

/* **********************************************************
 * 
 * **********************************************************/

////////////////////////////////////////////////////////////////////////
// Tool Functions
////////////////////////////////////////////////////////////////////////

// This function will modify the original string to mark the end of token.
// Eventually this function should move to common folder.
// return token and ship pointer of p
static char * get_next_token (char * pin, int * shift)
{
        char * token;
        char * p = pin;

        if(p==NULL) return NULL;

        // skip the leading space
        while(*p==' '||*p=='\t') {
                if(*p==0) return NULL; // No more token
                p++;
        }
        if(*p==0) return NULL; // No more token

        // starting pointer of token
        token = p;

        // mark the end of token
        while(*p!=' ') {
                if(*p==0) { goto done; }
                p++;
        }
        *p=0; p++;

done:
        *shift = p-pin;
        return token;
}

void pxy_http_portlist_callback(char * http_port_list_str)
{
        char * port_str;
        int len, port, i;
        static int called=0;

        if(called == 1) return; // only allow to be called once.

        for(i=0;i<MAX_PORTLIST; i++) {
                nkn_http_serverport[i] = 0;
        }

        for(i=0;i<MAX_PORTLIST; i++) {
                port_str = get_next_token(http_port_list_str, &len);
                if( !port_str ) return ;
                http_port_list_str += len;

                port = atoi(port_str);
                if(port > 0) {
                        nkn_http_serverport[i] = port;
                }
        }
}

void pxy_http_interfacelist_callback(char * http_interface_list_str)
{
        char * interface_str;
        int len, i;
        static int interface_called=0;

        if(interface_called == 1) return; // only allow to be called once.

        for(i=0; i<MAX_NKN_INTERFACE; i++) {
                interface[i].enabled = 0;
        }

        while(1) {
                interface_str = get_next_token(http_interface_list_str, &len);
                if( !interface_str ) return ;
                http_interface_list_str += len;

                strncpy(http_listen_intfname[http_listen_intfcnt], interface_str, len);
                http_listen_intfcnt++;
        }
}


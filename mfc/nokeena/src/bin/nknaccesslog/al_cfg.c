#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include "nkn_defs.h"
#include "accesslog_pub.h"
#include "accesslog_pri.h"


int max_filename_cnt = 1000;
format_field_t * ff_ptr = NULL;
int ff_used =0;
int ff_size =0;
static char * format=NULL;
int use_syslog=0;

uint32_t al_serverip;
int al_serverport;
static char * al_syslog=NULL;
char * al_filename=NULL;
int64_t al_max_filesize = 0;
int64_t al_total_diskspace = 0;
char *remote_url = NULL;
char *pass = NULL;

extern pthread_mutex_t acc_lock;
extern md_client_context *al_mcc;

static const char al_config_prefix[] = "/nkn/accesslog/config";
static const char al_servip_binding[]   = "/nkn/accesslog/config/serverip";
static const char al_servport_binding[] = "/nkn/accesslog/config/serverport";
static const char al_syslog_binding[]   = "/nkn/accesslog/config/syslog";
static const char al_filename_binding[] = "/nkn/accesslog/config/filename";
static const char al_filesize_binding[] = "/nkn/accesslog/config/max-filesize";
static const char al_diskspace_binding[] = "/nkn/accesslog/config/total-diskspace";
static const char al_format_binding[]   = "/nkn/accesslog/config/format";
static const char al_upload_rurl_binding[]   = "/nkn/accesslog/config/upload/url";
static const char al_upload_pass_binding[]   = "/nkn/accesslog/config/upload/pass";


tbool dyn_change = false;



typedef struct _cfg_params_
{
    uint32_t    srvr_ip;
    int         srvr_port;
    int         syslog; 
    char        *filename;
    int64_t     max_filesize;
    int64_t     max_disksize;
    char        *rurl;
    char        *rpass;
    format_field_t *ff_ptr;
    char        *format;

} cfg_params_t;


static cfg_params_t al_cfg_master = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

////////////////////////////////////////////////////////////////////////
// Parse the HTTP accesslog format
////////////////////////////////////////////////////////////////////////

/**
 *
 *
 * "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\""
 *
 */

const format_mapping fmap[] =
{
        { '9', FORMAT_RFC_931_AUTHENTICATION_SERVER },

        { 'b', FORMAT_BYTES_OUT_NO_HEADER },
        { 'c', FORMAT_CACHE_HIT },                  // Available (1.0.2+)
        //{ 'e', FORMAT_ENV },                        // NYI
        { 'f', FORMAT_FILENAME },                   // Available
        { 'h', FORMAT_REMOTE_HOST },                // Available
        { 'i', FORMAT_HEADER },                     // Available
        //{ 'l', FORMAT_REMOTE_IDENT },               // NYI
        { 'm', FORMAT_REQUEST_METHOD },             // Available
        { 'o', FORMAT_RESPONSE_HEADER },            // Available
        { 'q', FORMAT_QUERY_STRING },               // Available
        { 'r', FORMAT_REQUEST_LINE },               // Available
        { 's', FORMAT_STATUS },                     // Available
        { 't', FORMAT_TIMESTAMP },                  // Available
        { 'u', FORMAT_REMOTE_USER },                // Available
        { 'v', FORMAT_SERVER_NAME },                // Available
        //{ 'w', FORMAT_PEER_STATUS },                // NYI
        //{ 'x', FORMAT_PEER_HOST },                  // NYI
        { 'y', FORMAT_STATUS_SUBCODE },             // Available

        { 'C', FORMAT_COOKIE },                     // Available
        { 'D', FORMAT_TIME_USED_MS },               // Available
        { 'E', FORMAT_TIME_USED_SEC },              // Available
        { 'H', FORMAT_REQUEST_PROTOCOL },           // Available
        { 'I', FORMAT_BYTES_IN },                   // Available
        { 'O', FORMAT_BYTES_OUT },                  // Available
        { 'U', FORMAT_URL }, /* w/o querystring */  // Available
        { 'V', FORMAT_HTTP_HOST },                  // Available
        //{ 'W', FORMAT_CONNECTION_STATUS },          // NYI
        { 'X', FORMAT_REMOTE_ADDR },                // Available
        { 'Y', FORMAT_LOCAL_ADDR },                 // Available
        { 'Z', FORMAT_SERVER_PORT },                // Available
    
        { '%', FORMAT_PERCENT },
        { '\0', FORMAT_UNSET }
};

////////////////////////////////////////////////////////////////////////
int al_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
        void *data);

int
al_module_cfg_handle_change(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings);

static char get_next_char(void);
void http_accesslog_format_callback(char * format);



////////////////////////////////////////////////////////////////////////
static char get_next_char(void)
{
	char ch=*format;
	if(ch!=0) format++;
	return ch;
}

////////////////////////////////////////////////////////////////////////
static void init_format_field(void)
{
	ff_size = 100;
	ff_used = 0;
	ff_ptr = malloc(ff_size * sizeof(format_field_t));
	if(ff_ptr == NULL) {
		assert(0);
	}
	return;
}

////////////////////////////////////////////////////////////////////////
static void get_field_value(char ch)
{

	int i;
	for (i = 0; fmap[i].key != '\0'; i++) {
		if (fmap[i].key != ch) continue;

		/* found key */
		ff_ptr[ff_used].field = fmap[i].type;
		return;
	}

	if (fmap[i].key == '\0') {
		al_log_basic("failed to parse config %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}
}

////////////////////////////////////////////////////////////////////////
static int accesslog_parse_format(char * pformat, int len) 
{
	int i, j;
	char ch;

	format = pformat;

	while(1) {
		ch=get_next_char();
		if(ch==0) break;

		// It is string.
		if(ch != '%') {
			ff_ptr[ff_used].field = FORMAT_STRING;
			ff_ptr[ff_used].string[0] = ch;

			i=1;
			while(1) {
				ch=get_next_char();
				if((ch=='%')||(ch==0)) break;
				ff_ptr[ff_used].string[i]=ch;
				i++;
			}
			ff_ptr[ff_used].string[i]=0;

			ff_used++;
			if(ch==0) break; // break the whole while(1) loop
		}
	
		ff_ptr[ff_used].string[0] = 0;

		/* search for the terminating command */
		ch=get_next_char();
		if (ch == '{') {
			i=0;
			while(1) {
				ch=get_next_char();
				if (ch == 0) {
					al_log_basic("\nERROR: failed to parse configuration file.\n");
					exit(1);
				}
				else if (ch == '}') break;
				ff_ptr[ff_used].string[i]=ch;
				i++;
			}
			ff_ptr[ff_used].string[i]=0;

			// Get next char after '}'
			get_field_value(get_next_char());
			if (ff_ptr[ff_used].field == FORMAT_COOKIE) {
				/* 
				 * we need to adjust the string as .cok<cookiname> 
				 * BUG: 64 bytes should be enough.
				 */
				char tmp[64];
				strcpy(tmp, ff_ptr[ff_used].string);
				snprintf(ff_ptr[ff_used].string, 64, ".cok%s", tmp);
			}
		}
		else {
			get_field_value(ch);
			//printf("%d : %d\n", ff_>used, ff_>ptr[ff_>used]->field);
		}

		ff_used++;

	}

	// Special case for URI/Query_string
	// We want to do in this way.
	for(j=0; j<ff_used; j++) {
		switch (ff_ptr[j].field) {
		case FORMAT_URL:
		case FORMAT_QUERY_STRING:
		case FORMAT_REQUEST_LINE:
			strcpy(&ff_ptr[j].string[0], "uri");
			break;
		case FORMAT_HTTP_HOST:
			strcpy(&ff_ptr[j].string[0], "host");
			break;
		case FORMAT_REMOTE_USER:
			strcpy(&ff_ptr[j].string[0], "user"); // HTTP user field.
			break;
		default:
			break;
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////
void http_accesslog_format_callback(char * format_l)
{
	int len;

	if(!format_l) return;
	len=strlen(format_l);
	/* 
	 * In theory, there should be no limit on length of format.
	 * Pratically 256 bytes of format length should be more than enough.
	 * So I add the limitation here.
	 */
	if(len<=0 || len>=MAX_AL_FORMAT_LENGTH) return;

	while(*format_l == ' ' || *format_l == '\t') format_l++;
	al_log_debug("format_l=%s\n", format_l);

	if (-1 == accesslog_parse_format(format_l, len)) {

		al_log_basic("Failed in parsing accesslog format\n");
	}
}


////////////////////////////////////////////////////////////////////////
// General configuration parsing functions
////////////////////////////////////////////////////////////////////////

typedef void (* CFG_func)(char * cfg);

#define TYPE_INT 	1
#define TYPE_STRING 	2
#define TYPE_LONG 	3
#define TYPE_FUNC 	4
#define TYPE_IP 	5

static struct nkncfg_def {
	const char * label;
	int type;
	void * paddr;
	const char * vdefault;
} cfgdef[] = 
{

    { "ServerIP", TYPE_IP,    &al_serverip, "127.0.0.1" },
    { "ServerPort", TYPE_INT,    &al_serverport, "7957" },
    { "Syslog",  TYPE_STRING,  &al_syslog, "no" },
    { "FileName",  TYPE_STRING, &al_filename,  "access.log"   },
    { "Max-Filesize",  TYPE_LONG, &al_max_filesize,  "100"   },
    { "Total-Diskspace",  TYPE_LONG, &al_total_diskspace,  "1000"   },
    { "Format", TYPE_FUNC, &http_accesslog_format_callback, NULL }, 

    { NULL,          TYPE_INT,    NULL,                NULL   }
};


//////////////////////////////////////////////////////////////////////////////
// Parse the configuration file
//////////////////////////////////////////////////////////////////////////////

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

static char * skip_to_value(char * p)
{
	while(*p==' '||*p=='\t') p++;
	if(*p!='=') return NULL;
	p++;
	while(*p==' '||*p=='\t') p++;
	return p;
}

int read_nkn_cfg(char * configfile)
{
	FILE * fp;
	char * p;
	char buf[1024];
	int len, i;
	struct nkncfg_def * pcfgdef;

	init_format_field();

	// Initialize
	for(i=0; ;i++) {
		pcfgdef=&cfgdef[i];
		if(pcfgdef->label == NULL) break;
		if(pcfgdef->type==TYPE_INT) {
			int * pint = (int *)pcfgdef->paddr;
			* pint = atoi(pcfgdef->vdefault);
		} else if(pcfgdef->type==TYPE_STRING) {
			const char ** pchar = (const char **)pcfgdef->paddr;
			* pchar = pcfgdef->vdefault;
		} else if(pcfgdef->type==TYPE_LONG) {
			long * plong = (long *)pcfgdef->paddr;
			* plong = atol(pcfgdef->vdefault);
		} else if(pcfgdef->type==TYPE_FUNC) {
			( * (CFG_func)(pcfgdef->paddr))(NULL);
		} else if(pcfgdef->type==TYPE_IP) {
			uint32_t * plong = (uint32_t *)pcfgdef->paddr;
			* plong = inet_addr("127.0.0.1");
		}
	}

	// Read the configuration
	fp=fopen(configfile, "r");
	if(!fp) {
		al_log_basic("\n");
		al_log_basic("ERROR: failed to open configure file <%s>\n", configfile);
		al_log_basic("       use all default configuration settings.\n\n");
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

	if(al_syslog) {
		if (strcmp(al_syslog, "yes")==0) { use_syslog=1; }
		free(al_syslog);
		al_syslog = NULL;
	}

	if( ff_used==0 ) {
		al_log_basic("\nERROR: Configuration file may not be right.\n\n");
		exit(1);
	}

	al_log_basic("Configuration has been successfully loaded\n");

	return 1;
}

int nkn_check_cfg(void)
{
    if ((ff_ptr == NULL) || (al_filename == NULL)) {
        return -1;
    }
    if ((al_max_filesize == 0) || (al_total_diskspace == 0))
        return -1;
	/* sanity check */
	if(al_total_diskspace < al_max_filesize) {
		al_total_diskspace = al_max_filesize;
        al_log_basic("Using diskspace = %d MBytes", (int) al_total_diskspace);
	}

	/* convert from MBytes to Bytes */
    /* For MGMTD assisted config, use the cfg_master structure */
    if (glob_run_under_pm) {
        al_max_filesize = (al_cfg_master.max_filesize * MBYTES);
        al_total_diskspace = (al_cfg_master.max_disksize * MBYTES);
    }
    else {
        al_max_filesize = (al_max_filesize * MBYTES);
        al_total_diskspace = (al_total_diskspace * MBYTES);
    }


	max_filename_cnt = al_total_diskspace / al_max_filesize + 1;

    return 0;
}

void print_cfg_info()
{
        int j;

        al_log_debug("al_serverip=%x\n", al_serverip);
        al_log_debug("al_serverport=%d\n", al_serverport);
        al_log_debug("use_syslog=%d\n", use_syslog);
        al_log_debug("al_filename=%s\n", al_filename);
        al_log_debug("al_max_filesize=%ld Bytes\n", al_max_filesize);
        al_log_debug("al_total_diskspace=%ld Bytes\n", al_total_diskspace);
        al_log_debug("remote_url =%s \n", remote_url);
        al_log_debug("remote_pass =%s \n", pass);

        if ( ff_ptr == NULL )
            return;
        for(j=0; j<ff_used; j++) {
            if(ff_ptr[j].string[0] != 0) {
                al_log_debug("format=%d string=<%s>\n",
                        ff_ptr[j].field,
                        ff_ptr[j].string);
            } else {
                al_log_debug("format=%d\n", ff_ptr[j].field);
            }
        }
}

void nkn_free_cfg(void)
{
	if(ff_ptr) {
		free(ff_ptr);
		ff_ptr = NULL;
	}
	if(al_filename && glob_run_under_pm) {
		free(al_filename);
		al_filename = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////
int al_cfg_query(void)
{
        int err = 0;
        bn_binding_array *bindings = NULL;
        tbool rechecked_licenses = false;
        

        lc_log_basic(LOG_NOTICE, "accesslog initializing ..");
        err = mdc_get_binding_children(al_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
                        true,
                        al_config_prefix);
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, al_cfg_handle_change, 
                        &rechecked_licenses);
        bail_error(err);

        bn_binding_array_free(&bindings);
        err = mdc_get_binding_children(al_mcc, 
                        NULL, NULL, true, &bindings, false, true, "/license/key");
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, al_cfg_handle_change, 
                        &rechecked_licenses);
        bail_error(err);

        al_apply_config();
bail:
        bn_binding_array_free(&bindings);
        return err;
}

int al_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
        void *data)
{
        int err = 0;
        const tstring *name = NULL;
        tstr_array *name_parts = NULL;
        uint16 serv_port = 0;
        tbool bool_value = false;
        tbool *rechecked_licenses_p = data;

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);


        /*-------- SERVER IP - This is always @ 127.0.0.1 ------------*/
        if (ts_equal_str(name, al_servip_binding, false)) {
                /* IPV4 address is stored as bt_ipv4addr, which is a 
                 * uint32_t.
                 */
                uint32 ip_val = 0;
                err = bn_binding_get_ipv4addr(binding,
                                ba_value, NULL,
                                &ip_val);
                bail_error(err);
                /* TOD: Copy the string into the server ip field, if we
                 * expect this to change */

                al_cfg_master.srvr_ip = htonl(ip_val);
                dyn_change = true;

                //al_serverip = htonl(ip_val);


                lc_log_basic(LOG_DEBUG, "updated server ip to \"%x\"",
                                ip_val);
        }
        /*-------- SERVER PORT ------------*/
        else if (ts_equal_str(name, al_servport_binding, false)) {
                err = bn_binding_get_uint16(binding, ba_value, NULL, &serv_port);
                bail_error(err);
                if (serv_port != al_serverport) {
                        al_serverport = serv_port;
                        /* Init server here. */
                        dyn_change = true;
                }

                lc_log_basic(LOG_DEBUG, "updated server port to %d", 
                                al_serverport);
        }
        /*-------- ENABLE SYSLOG ------------*/
        else if ( ts_equal_str(name, al_syslog_binding, false)) {
            tbool syslog_enable = false;
                //tstring *str = NULL;
                err = bn_binding_get_tbool(binding, ba_value, NULL,
                                &syslog_enable);
                bail_error(err);
                if (syslog_enable == true) {
                    use_syslog = 1;
                    al_log_debug("Syslog replication enabled");
                }
                else {
                    use_syslog = 0;
                    al_log_basic("Syslog replication disabled");
                }
        }
        /*-------- MAX PER FILE SIZE ------------*/
        else if ( ts_equal_str(name, al_filename_binding, false)) {
                tstring *str = NULL;
                err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
                                &str);
                bail_error(err);

                if ( al_cfg_master.filename ) {
                    free(al_cfg_master.filename);
                }

                al_cfg_master.filename = strdup(ts_str(str));
                dyn_change = true;
                //al_filename = strdup(ts_str(str));
        }
        /*-------- MAX PER FILE SIZE ------------*/
        else if (ts_equal_str(name, al_filesize_binding, false)) {
                uint16 size = 0;
                err = bn_binding_get_uint16(binding, ba_value,
                                NULL, &size);
                bail_error(err);
                /* Note that check for whether filesize > diskspace
                 * is done in the mgmtd counterpart
                 */
                al_max_filesize = (int64_t) size;
                al_cfg_master.max_filesize = (int64_t) size;
                dyn_change = true;

        }
        /*-------- TOTAL DISKSPACE ------------*/
        else if (ts_equal_str(name, al_diskspace_binding, false)) {
                uint16 size = 0;
                err = bn_binding_get_uint16(binding, ba_value,
                                NULL, &size);
                bail_error(err);

                al_total_diskspace = (int64_t) size;
                al_cfg_master.max_disksize = (int64_t) size;
                dyn_change = true;

        }
        /*-------- FORMAT STRING ------------*/
        else if (ts_equal_str(name, al_format_binding, false)) {
                tstring *str = NULL;
                err = bn_binding_get_tstr(binding, ba_value, bt_string, 
                                NULL, &str);
                bail_error(err);

                if (ts_length(str) == 0) {
                    al_log_basic("Bad or NULL format specifier");
                    goto bail;
                }

                if ( al_cfg_master.format ) {
                    free(al_cfg_master.format);
                }

                al_cfg_master.format = strdup(ts_str(str));

                dyn_change = true;
                //init_format_field();
                //http_accesslog_format_callback( (char*) ts_str(str));
        }
        /*------- REMOTE URL FOR SCP --------*/
        else if (ts_equal_str(name, al_upload_rurl_binding, false)) {
            tstring *str = NULL;
            err = bn_binding_get_tstr(binding, ba_value, bt_string,
                    NULL, &str);
            bail_error(err);

            al_log_debug("Remote URL: %s\n", ts_str(str));

            if (al_cfg_master.rurl) {
                free(al_cfg_master.rurl);
            }

            safe_free(remote_url);
            remote_url = NULL;
            if ( str && ts_length(str)) {
                remote_url = smprintf("%s", ts_str(str));
                bail_null(remote_url);

                //al_cfg_master.rurl = strdup(remote_url);

            }
        }
        /*------- PASS FOR SCP --------*/
        else if(ts_equal_str(name, al_upload_pass_binding, false)) {
            tstring *str = NULL;
            err = bn_binding_get_tstr(binding, ba_value, bt_string,
                    NULL, &str);
            bail_error(err);

            //al_log_basic("Pass: %s\n", ts_str(str));

            if (al_cfg_master.rpass) {
                free(al_cfg_master.rpass);
            }

            safe_free(pass);
            pass = NULL;
            if ( str && ts_length(str)) {
                pass = smprintf("%s", ts_str(str));
                bail_null(pass);
                //al_cfg_master.rpass = strdup(pass);
            }
        }




bail:
        tstr_array_free(&name_parts);
        return err;
}

int
al_module_cfg_handle_change(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    err = bn_binding_array_foreach(new_bindings, al_cfg_handle_change, 
            &rechecked_licenses);
    bail_error(err);

    al_apply_config();

bail:
    return err;
}



int al_config_done = 0;



int al_apply_config(void)
{
    int err = 0;


    if (!dyn_change)
        return 0;

    // Grab the config lock. so that when we shut down 
    // the socket, the client thread is not in a half 
    // configureed state
    pthread_mutex_lock(&server_lock);

    // Shutdown the socket
    al_close_socket();
    // Wait till the lock is released and we can grab it from the 
    // client socket thread
    LOCK_();
   //while( pthread_mutex_trylock(&acc_lock) == EBUSY) sleep(1);
    
    http_accesslog_exit();
    nkn_save_fileid();
    nkn_free_cfg();

    // Lock grabbed. Apply the new config
    if (al_cfg_master.filename) {
        al_filename = strdup(al_cfg_master.filename);
        lc_log_basic(LOG_DEBUG, "updated filename to \"%s\"",
                        al_cfg_master.filename);

        //free(al_cfg_master.filename);
        //al_cfg_master.filename = NULL;
    }

    if (al_cfg_master.srvr_ip) {
        al_serverip = al_cfg_master.srvr_ip;
    }

    if (al_cfg_master.max_disksize) {
        al_total_diskspace = al_cfg_master.max_disksize;
        lc_log_basic(LOG_DEBUG, "updated total-diskspace to %lld",
                        (long long int) al_total_diskspace);
    }

    if (al_cfg_master.max_filesize) {
        al_max_filesize = al_cfg_master.max_filesize;
        lc_log_basic(LOG_DEBUG, "updated max-filesize to %lld",
                        (long long int) al_max_filesize);
    }

    if (al_cfg_master.format && !ff_ptr) {
        lc_log_basic(LOG_DEBUG, "updated format string to %s",
                al_cfg_master.format);
        init_format_field();
        http_accesslog_format_callback(al_cfg_master.format);
    }

    nkn_check_cfg();
    print_cfg_info();
    nkn_read_fileid();
    http_accesslog_init();


    // release the lock
    //
    //pthread_mutex_unlock(&acc_lock);
    UNLOCK_();

    al_config_done = 1;
    pthread_cond_broadcast(&config_cond);
    pthread_mutex_unlock(&server_lock);
    dyn_change = false; 

bail:
    return err;
}

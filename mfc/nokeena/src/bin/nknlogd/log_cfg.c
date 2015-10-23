#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>		/* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <md_client.h>
#include <license.h>

#include "nkn_defs.h"
#include "accesslog_pub.h"
#include "log_accesslog_pri.h"
#include "log_streamlog_pri.h"
#include "nknlogd.h"

struct log_file_queue g_lf_head;
static logd_file cur_plf;
/* Temperory variables */
char *log_folderpath = NULL;
int log_port_listen = 7958;
int agent_access = 1;
static int firstexit = 0;
static char *filename = NULL;
static int max_filesize = 0;
static int max_fileid = 0;
static int time_interval = 0;
static char *on_hour = NULL;
static char *status = NULL;
static char *cfgsyslog = NULL;
static char *format = NULL;
static char *show_format = NULL;
static char *modstr = NULL;
static char *cfglog_rotation = NULL;
static int level = 0;
static int glob_channelid = 0;

extern int accesslogid, errorlogid, tracelogid, cachelogid, streamlogid,
    mfplogid, crawllogid;
extern int fuselogid;
extern int cblogid;

static char *l_restriction = NULL;
static int r_size = 0;
int al_r_code[32];
int al_r_code_empty = 0;
//int al_rcode_ct = 0; 
static char *r_code = NULL;

extern pthread_mutex_t log_lock;
extern pthread_mutex_t nm_free_queue_lock;
extern md_client_context *mgmt_mcc;


/*! error log mgmt nodes */
static const char er_config_prefix[] = "/nkn/errorlog/config";
static const char er_log_active[] = "/nkn/errorlog/config/enable";
static const char er_syslog_binding[] = "/nkn/errorlog/config/syslog";
static const char er_rotate_binding[] = "/nkn/errorlog/config/rotate";
static const char er_filename_binding[] = "/nkn/errorlog/config/filename";
static const char er_filesize_binding[] =
    "/nkn/errorlog/config/max-filesize";
static const char er_diskspace_binding[] =
    "/nkn/errorlog/config/total-diskspace";
static const char er_timeinterval_binding[] =
    "/nkn/errorlog/config/time-interval";
static const char er_upload_rurl_binding[] =
    "/nkn/errorlog/config/upload/url";
static const char er_upload_pass_binding[] =
    "/nkn/errorlog/config/upload/pass";
static const char er_debug_level_binding[] = "/nkn/errorlog/config/level";
static const char er_debug_mod_binding[] = "/nkn/errorlog/config/mod";
static const char er_on_the_hour_rotate_binding[] =
    "/nkn/errorlog/config/on-the-hour";

/*! trace log mgmt nodes */
static const char tr_config_prefix[] = "/nkn/tracelog/config";
static const char tr_log_active[] = "/nkn/tracelog/config/enable";
static const char tr_syslog_binding[] = "/nkn/tracelog/config/syslog";
static const char tr_filename_binding[] = "/nkn/tracelog/config/filename";
static const char tr_filesize_binding[] =
    "/nkn/tracelog/config/max-filesize";
static const char tr_timeinterval_binding[] =
    "/nkn/tracelog/config/time-interval";
static const char tr_diskspace_binding[] =
    "/nkn/tracelog/config/total-diskspace";
static const char tr_upload_rurl_binding[] =
    "/nkn/tracelog/config/upload/url";
static const char tr_upload_pass_binding[] =
    "/nkn/tracelog/config/upload/pass";
static const char tr_on_the_hour_rotate_binding[] =
    "/nkn/tracelog/config/on-the-hour";

/*! cache log mgmt nodes */
static const char cache_config_prefix[] = "/nkn/cachelog/config";
static const char cache_log_active[] = "/nkn/cachelog/config/enable";
static const char cache_syslog_binding[] = "/nkn/cachelog/config/syslog";
static const char cache_filename_binding[] =
    "/nkn/cachelog/config/filename";
static const char cache_rotate_binding[] = "/nkn/cachelog/config/rotate";
static const char cache_filesize_binding[] =
    "/nkn/cachelog/config/max-filesize";
static const char cache_timeinterval_binding[] =
    "/nkn/cachelog/config/time-interval";
static const char cache_diskspace_binding[] =
    "/nkn/cachelog/config/total-diskspace";
static const char cache_upload_rurl_binding[] =
    "/nkn/cachelog/config/upload/url";
static const char cache_upload_pass_binding[] =
    "/nkn/cachelog/config/upload/pass";
static const char cache_on_the_hour_rotate_binding[] =
    "/nkn/cachelog/config/on-the-hour";

/*! stream log mgmt nodes */
static const char stream_config_prefix[] = "/nkn/streamlog/config";
static const char stream_log_active[] = "/nkn/streamlog/config/enable";
static const char stream_syslog_binding[] = "/nkn/streamlog/config/syslog";
static const char stream_filename_binding[] =
    "/nkn/streamlog/config/filename";
static const char stream_rotate_binding[] = "/nkn/streamlog/config/rotate";
static const char stream_filesize_binding[] =
    "/nkn/streamlog/config/max-filesize";
static const char stream_timeinterval_binding[] =
    "/nkn/streamlog/config/time-interval";
static const char stream_diskspace_binding[] =
    "/nkn/streamlog/config/total-diskspace";
static const char stream_upload_rurl_binding[] =
    "/nkn/streamlog/config/upload/url";
static const char stream_upload_pass_binding[] =
    "/nkn/streamlog/config/upload/pass";
static const char stream_format_binding[] = "/nkn/streamlog/config/format";
static const char stream_on_the_hour_rotate_binding[] =
    "/nkn/streamlog/config/on-the-hour";
static const char stream_format_display_binding[] =
    "/nkn/streamlog/config/format-display";

/*! fuse log mgmt nodes */
static const char fuse_config_prefix[] = "/nkn/fuselog/config";
static const char fuse_log_active[] = "/nkn/fuselog/config/enable";
static const char fuse_syslog_binding[] = "/nkn/fuselog/config/syslog";
static const char fuse_filename_binding[] = "/nkn/fuselog/config/filename";
static const char fuse_filesize_binding[] =
    "/nkn/fuselog/config/max-filesize";
static const char fuse_timeinterval_binding[] =
    "/nkn/fuselog/config/time-interval";
static const char fuse_diskspace_binding[] =
    "/nkn/fuselog/config/total-diskspace";
static const char fuse_upload_rurl_binding[] =
    "/nkn/fuselog/config/upload/url";
static const char fuse_upload_pass_binding[] =
    "/nkn/fuselog/config/upload/pass";
static const char fuse_on_the_hour_rotate_binding[] =
    "/nkn/fuselog/config/on-the-hour";

/*! mfp log mgmt nodes */
static const char mfp_config_prefix[] = "/nkn/mfplog/config";
static const char mfp_log_active[] = "/nkn/mfplog/config/enable";
static const char mfp_syslog_binding[] = "/nkn/mfplog/config/syslog";
static const char mfp_filename_binding[] = "/nkn/mfplog/config/filename";
static const char mfp_filesize_binding[] =
    "/nkn/mfplog/config/max-filesize";
static const char mfp_timeinterval_binding[] =
    "/nkn/mfplog/config/time-interval";
static const char mfp_diskspace_binding[] =
    "/nkn/mfplog/config/total-diskspace";
static const char mfp_upload_rurl_binding[] =
    "/nkn/mfplog/config/upload/url";
static const char mfp_upload_pass_binding[] =
    "/nkn/mfplog/config/upload/pass";
static const char mfp_on_the_hour_rotate_binding[] =
    "/nkn/mfplog/config/on-the-hour";

/*! to be deprecated */
static const char system_config_prefix[] = "/nkn/nvsd/system/config";
static const char system_debug_level_binding[] =
    "/nkn/nvsd/system/config/debug/level";
static const char system_debug_mod_binding[] =
    "/nkn/nvsd/system/config/debug/mod";



////////////////////////////////////////////////////////////////////////
// Parse the HTTP accesslog format
////////////////////////////////////////////////////////////////////////

/**
 *
 *
 * "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\""
 *
 */

const format_mapping fmap[] = {
    //{ '9', FORMAT_RFC_931_AUTHENTICATION_SERVER },

    {'b', FORMAT_BYTES_OUT_NO_HEADER},
    {'c', FORMAT_CACHE_HIT},	// Available (1.0.2+)
    {'d', FORMAT_ORIGIN_SIZE},	// Available
    {'e', FORMAT_CACHE_HISTORY},	// Available
    {'f', FORMAT_FILENAME},	// Available
    {'g', FORMAT_COMPRESSION_STATUS},	// Available
    {'h', FORMAT_REMOTE_HOST},	// Available
    {'i', FORMAT_REQUEST_HEADER},	// Available
    //{ 'l', FORMAT_REMOTE_IDENT },             // NYI
    {'m', FORMAT_REQUEST_METHOD},	// Available
    {'o', FORMAT_RESPONSE_HEADER},	// Available
    {'p', FORMAT_HOTNESS},	// Available
    {'q', FORMAT_QUERY_STRING},	// Available
    {'r', FORMAT_REQUEST_LINE},	// Available
    {'s', FORMAT_STATUS},	// Available
    {'t', FORMAT_TIMESTAMP},	// Available
    {'u', FORMAT_REMOTE_USER},	// Available
    {'v', FORMAT_SERVER_NAME},	// Available
    {'w', FORMAT_EXPIRED_DEL_STATUS}, // Available 
    //{ 'x', FORMAT_PEER_HOST },                  // NYI
    {'y', FORMAT_STATUS_SUBCODE},	// Available
    {'z', FORMAT_CONNECTION_TYPE},	// Available


    {'A', FORMAT_REQUEST_IN_TIME},	// Available
    {'B', FORMAT_FBYTE_OUT_TIME},	// Available
    {'C', FORMAT_COOKIE},	// Available
    {'D', FORMAT_TIME_USED_MS},	// Available
    {'E', FORMAT_TIME_USED_SEC},	// Available
    {'F', FORMAT_LBYTE_OUT_TIME},	// Available
    {'H', FORMAT_REQUEST_PROTOCOL},	// Available
    {'I', FORMAT_BYTES_IN},	// Available
    {'L', FORMAT_LATENCY},	// Available
    {'M', FORMAT_DATA_LENGTH_MSEC},	// Available
    {'N', FORMAT_NAMESPACE_NAME},	// Available
    {'O', FORMAT_BYTES_OUT},	// Available
    {'R', FORMAT_REVALIDATE_CACHE},	// Available
    {'S', FORMAT_ORIGIN_SERVER_NAME}, // Available
    {'U', FORMAT_URL},		/* w/o querystring */// Available
    {'V', FORMAT_HTTP_HOST},	// Available
    //{ 'W', FORMAT_CONNECTION_STATUS },          // NYI
    {'X', FORMAT_REMOTE_ADDR},	// Available
    {'Y', FORMAT_LOCAL_ADDR},	// Available
    {'Z', FORMAT_SERVER_PORT},	// Available

    {'%', FORMAT_PERCENT},
    {'\0', FORMAT_UNSET}
};

////////////////////////////////////////////////////////////////////////
// Parse the RTSP  streamlog format
////////////////////////////////////////////////////////////////////////

/**
 *
 * "%t %d %s %b %x %u %l %C %S %T %B %E %P %n %O %R %A %D %I %X %K %p %o %v %c %f %a %i %r %h %k"
 *  Or
 * *%t %d %s %b %x %u %l %C %S %T %O %R %i %r"
 *
 */

const sl_format_mapping sl_map[] = {
    {'t', FORMAT_X_TIMESTAMP},	//x-localtime
    {'c', FORMAT_S_IP},		//s-ip dst_ip
    {'d', FORMAT_R_IP},		//r-ip Origin server ipaddress
    {'h', FORMAT_C_IP},		//c-ip Client ip address, (src_ip)
    {'x', FORMAT_TRANSACTION_CODE},	//transaction or reply code, 
    {'r', FORMAT_CS_URI},	//Streaming URL access, (urlSuffix)
    {'l', FORMAT_CS_URI_STEM},	//Streaming URL stem only, (urlPreSuffix)

    {'C', FORMAT_CLIENT_ID},	//unique client side streaming id
    {'S', FORMAT_STREAM_ID},	//unique server side streaming id, (fOurSessionId)
    {'T', FORMAT_EMD_TIME},	//GMT time when transaction ended, (end_ts)
    {'B', FORMAT_CLIP_START},	//when client started receiving stream,
    {'E', FORMAT_CLIP_END},	//when the client stop receiving stream,
    {'P', FORMAT_PACKETS},	//Total number of packets delivered to the client

    {'s', FORMAT_CS_STATUS},	//RTSP responce code returned to the client(HTTP status)

    {'L', FORMAT_CON_PROTOCOL},	//Protocol used during connection, RTSP
    {'R', FORMAT_TRA_PROTOCOL},	//Transport protocol used during the connection, TCP
    {'A', FORMAT_ACTION},	//Action performed, SPLIT, PROXY, CACHE, CONNECTING
    {'D', FORMAT_DROPPED_PACKETS},	//#pf packets dropped
    {'M', FORMAT_PLAY_TIME},	//duration of time the client received the content
    {'X', FORMAT_PRODUCT},	//Streaming product used to create and stream conten
    {'K', FORMAT_RECENT_PACKET},	//packets resent to the client

    {'p', FORMAT_C_PLAYERVERSION},	//client media player version
    {'o', FORMAT_C_OS},		//client os
    {'v', FORMAT_C_OSVERSION},	//client os version
    {'u', FORMAT_C_CPU},	//Client CPU type
    {'f', FORMAT_FILELENGTH},	//length of the sream in secs
    {'a', FORMAT_AVG_BANDWIDTH},	//Avg bandwidth in bytes per sec
    {'i', FORMAT_CS_USER_AGENT},	//player info used in the header
    {'j', FORMAT_E_TIMESTAMP},	//number of sec since Jan 1, 1970, when transaction ended

    {'I', FORMAT_S_BYTES_IN},	//RTSP: bytes received from client
    {'O', FORMAT_S_BYTES_OUT},	//RTSP: bytes sent to client
    {'N', FORMAT_S_NAMESPACE_NAME},	//RTSP Namespace name
    //{ 'b', FORMAT_BYTES_DELIVERED }, //Total bytes delivered, (out_bytes)

    //{ 'x', FORMAT_S_CACHEHIT },
    //{ 'Z', FORMAT_S_SERVER_PORT },
    {'%', FORMAT_S_PERCENT},
    {'\0', FORMAT_S_UNSET}

};


//static char * thisformat=NULL;
//static char get_next_char(void);
//static char get_next_char(void)
//{
//      char ch=*thisformat;
//        if(ch!=0) thisformat++;
//        return ch;
//}
char *get_next_char(char *str, char *ch);
char *get_next_char(char *str, char *ch)
{
    *ch = *str;
    if (ch != 0)
	str++;
    return str;
}

static void init_format_field(logd_file * plf);
static void init_format_field(logd_file * plf)
{
    if (plf->type == TYPE_ACCESSLOG) {
	if (plf->ff_ptr)
	    free(plf->ff_ptr);
	plf->ff_size = 100;
	plf->ff_used = 0;
	plf->ff_ptr = malloc(plf->ff_size * sizeof(format_field_t));
	if (plf->ff_ptr == NULL) {
	    complain_error_msg(1, "Memory allocation failed!");
	    exit(EXIT_FAILURE);
	}
	return;
    }
    if (plf->type == TYPE_STREAMLOG) {
	if (plf->sl_ptr)
	    free(plf->sl_ptr);
	plf->sl_size = 100;	//check if this is sufficient
	plf->sl_used = 0;
	plf->sl_ptr = malloc(plf->sl_size * sizeof(format_field_t));
	if (plf->sl_ptr == NULL) {
	    complain_error_msg(1, "Memory allocation failed!");
	    exit(EXIT_FAILURE);
	}
	return;
    }
}

static void get_field_value(logd_file * plf, char ch);
static void get_field_value(logd_file * plf, char ch)
{
    int i;
    if (plf->type == TYPE_ACCESSLOG) {
	for (i = 0; fmap[i].key != '\0'; i++) {
	    if (fmap[i].key != ch)
		continue;

	    /* found key */
	    plf->ff_ptr[plf->ff_used].field = fmap[i].type;
	    return;
	}

	if (fmap[i].key == '\0') {
	    printf("failed to parse config %s:%d\n", __FILE__, __LINE__);
	    exit(1);
	}
    }

    if (plf->type == TYPE_STREAMLOG) {
	for (i = 0; sl_map[i].key != '\0'; i++) {
	    if (sl_map[i].key != ch)
		continue;

	    /* found key */
	    plf->sl_ptr[plf->sl_used].field = sl_map[i].type;
	    return;
	}

	if (sl_map[i].key == '\0') {
	    printf("failed to parse config %s:%d\n", __FILE__, __LINE__);
	    exit(1);
	}
    }				//
}

static int parse_rcode(logd_file * plf, char *rft);
static int parse_rcode(logd_file * plf, char *rft)
{
    char *token;
    char *p;
    int32_t value;
    int i = 0;

    p = rft;
    token = strtok(p, " ");
    while (token != NULL) {
	sscanf(token, "%d", &value);
	al_r_code[i] = value;
	i++;
	token = strtok(NULL, " ");
	if (i >= 32)		//too much, restriction code to compare, consider only first 32
	    break;
    }
    plf->al_rcode_ct = i;
    plf->r_code = &al_r_code[0];
    return i;
}

static int verify_al_format_fields(char *al_format, int len);
static int verify_al_format_fields(char *al_format, int len)
{
    int ret;
    char *ft2;
    char ch1, ch2, ch3, ch4;
    const char *sig = "bcdefghimopqrstuvwyzABDEFHILMNORSUVXYZ%{";

    ret = 0;
    ft2 = al_format;

    while (1) {
	ft2 = get_next_char(ft2, &ch1);
	if (ch1 == 0)
	    break;		//end format
	else if (ch1 != '%') {
	    continue;
	}

	ft2 = get_next_char(ft2, &ch2);
	if (ch2 == 0) {
	    printf("ERROR: Can't have %c as the last character.\n", '%');
	    return 1;
	}
	if (strchr(sig, ch2) == NULL) {
	    printf("ERROR: Invalid format after %c \n", '%');
	    return 2;
	}

	if (ch2 != '{')
	    continue;

	while (*ft2 != '}') {
	    ft2++;
	    if (*ft2 == '\0') {
		printf("ERROR: No format string after { \n");
		return 3;
	    }
	}
	ft2++;
	if (*ft2 == '\0') {
	    printf("ERROR: No format field after }\n");
	    return 4;
	}
	if ((*ft2 != 'i') && (*ft2 != 'o') && (*ft2 != 'C')) {
	    printf("ERROR: Wrong format field after } \n");
	    return 5;
	}
    }				//End_while


    return ret;
}

int accesslog_parse_format(logd_file * plf);
int accesslog_parse_format(logd_file * plf)
{
    int i, j;
    char ch;
    int len;
    char *format_l;
    char *thisformat = NULL;
    char tmp[64];

    if (!plf->format)
	return -1;

    format_l = strdup(plf->format);
    //printf("format = %s\n", plf->format);
    len = strlen(format_l);
    if (len <= 0) {
	free(format_l);
	return -1;
    }

    thisformat = format_l;
    while (*thisformat == ' ' || *thisformat == '\t') {
	len--;
	thisformat++;
    }
    //verify formatfields, 
    if (verify_al_format_fields(thisformat, len) > 0) {
	printf("Error: Wrong format fields.\n");
	exit(1);
    }
    //printf("thisformat=%s\n", thisformat);

    init_format_field(plf);

    while (1) {
	thisformat = get_next_char(thisformat, &ch);
	if (ch == 0)
	    break;

	// It is string.
	if (ch != '%') {
	    plf->ff_ptr[plf->ff_used].field = FORMAT_STRING;
	    plf->ff_ptr[plf->ff_used].string[0] = ch;

	    i = 1;
	    while (1) {
		thisformat = get_next_char(thisformat, &ch);
		if ((ch == '%') || (ch == 0))
		    break;
		plf->ff_ptr[plf->ff_used].string[i] = ch;
		i++;
	    }
	    plf->ff_ptr[plf->ff_used].string[i] = 0;

	    plf->ff_used++;
	    if (ch == 0)
		break;		// break the whole while(1) loop
	}

	plf->ff_ptr[plf->ff_used].string[0] = 0;

	/* search for the terminating command */
	thisformat = get_next_char(thisformat, &ch);
	if (ch == '{') {
	    i = 0;
	    while (1) {
		thisformat = get_next_char(thisformat, &ch);
		if (ch == 0) {
		    printf
			("\nERROR: failed to parse configuration file.\n");
		    exit(1);
		} else if (ch == '}')
		    break;
		plf->ff_ptr[plf->ff_used].string[i] = ch;
		i++;
	    }
	    plf->ff_ptr[plf->ff_used].string[i] = 0;

	    // Get next char after '}'
	    thisformat = get_next_char(thisformat, &ch);
	    //get_field_value(plf, get_next_char());
	    get_field_value(plf, ch);
	    if (plf->ff_ptr[plf->ff_used].field == FORMAT_COOKIE) {
		/* BUG: 64 bytes should be enough. */
		strcpy(tmp, plf->ff_ptr[plf->ff_used].string);
		snprintf(plf->ff_ptr[plf->ff_used].string, 64, ".cok%s",
			 tmp);
	    } else if (plf->ff_ptr[plf->ff_used].field ==
		       FORMAT_REQUEST_HEADER) {
		/* BUG: 64 bytes should be enough. */
		strcpy(tmp, plf->ff_ptr[plf->ff_used].string);
		snprintf(plf->ff_ptr[plf->ff_used].string, 64, ".req%s",
			 tmp);
	    } else if (plf->ff_ptr[plf->ff_used].field ==
		       FORMAT_RESPONSE_HEADER) {
		/* BUG: 64 bytes should be enough. */
		strcpy(tmp, plf->ff_ptr[plf->ff_used].string);
		snprintf(plf->ff_ptr[plf->ff_used].string, 64, ".res%s",
			 tmp);
	    }
	} else {
	    get_field_value(plf, ch);
	    //printf("%d : %d\n", ff_>used, ff_>ptr[ff_>used]->field);
	}

	plf->ff_used++;

    }

    // Special case for URI/Query_string
    // We want to do in this way.
    for (j = 0; j < plf->ff_used; j++) {
	switch (plf->ff_ptr[j].field) {
	case FORMAT_URL:
	case FORMAT_QUERY_STRING:
	    strcpy(&plf->ff_ptr[j].string[0], ".uri");
	    break;
	case FORMAT_REQUEST_LINE:
	    strcpy(&plf->ff_ptr[j].string[0], ".rql");
	    break;
	case FORMAT_NAMESPACE_NAME:
	    strcpy(&plf->ff_ptr[j].string[0], ".nsp");
	    break;
	case FORMAT_FILENAME:
	    strcpy(&plf->ff_ptr[j].string[0], ".fnm");
	    break;
	case FORMAT_HTTP_HOST:
	    strcpy(&plf->ff_ptr[j].string[0], ".reqhost");	// %{Host}i
	    break;
	case FORMAT_REMOTE_USER:
	    strcpy(&plf->ff_ptr[j].string[0], ".requser");	// %{User}i
	    break;
	case FORMAT_SERVER_NAME:
	    strcpy(&plf->ff_ptr[j].string[0], ".snm");
	    break;
	case FORMAT_CACHE_HIT:
	    strcpy(&plf->ff_ptr[j].string[0], ".hit");
	    break;
	case FORMAT_ORIGIN_SIZE:
	    strcpy(&plf->ff_ptr[j].string[0], ".org");
	    break;
	case FORMAT_CACHE_HISTORY:
	    strcpy(&plf->ff_ptr[j].string[0], ".cht");
	    break;
	case FORMAT_HOTNESS:
	    strcpy(&plf->ff_ptr[j].string[0], ".hot");
	    break;
	case FORMAT_ORIGIN_SERVER_NAME:
            strcpy(&plf->ff_ptr[j].string[0], ".osn");
            break;
	case FORMAT_COMPRESSION_STATUS:
            strcpy(&plf->ff_ptr[j].string[0], ".com");
            break;
	case FORMAT_CONNECTION_TYPE:
	    strcpy(&plf->ff_ptr[j].string[0], ".rct");
	    break;
	case FORMAT_EXPIRED_DEL_STATUS:
	    strcpy(&plf->ff_ptr[j].string[0], ".eds");
	    break;	
	default:
	    break;
	}
    }

    free(format_l);
    return 0;
}


////////////////////////////////////////////////////////////////////////
// Streaming log format parser
////////////////////////////////////////////////////////////////////////

static int streamlog_parse_format(logd_file * plf);
static int streamlog_parse_format(logd_file * plf)
{
    int i, j;
    char ch;
    int len;
    char *format_2;
    char *stmformat = NULL;

    if (!plf->format)
	return -1;		// stream log format

    format_2 = strdup(plf->format);
    len = strlen(format_2);
    /*
     * In theory, there should be no limit on length of format.
     * Pratically 256 bytes of format length should be more than enough.
     * So I add the limitation here.
     */
    if (len <= 0 || len >= MAX_SL_FORMAT_LENGTH) {
	free(format_2);
	return -1;
    }

    stmformat = format_2;
    while (*stmformat == ' ' || *stmformat == '\t') {
	len--;
	stmformat++;
    }

    init_format_field(plf);

    while (1) {
	stmformat = get_next_char(stmformat, &ch);
	if (ch == 0)
	    break;

	// It is string.
	if (ch != '%') {
	    plf->sl_ptr[plf->sl_used].field = FORMAT_S_STRING;
	    plf->sl_ptr[plf->sl_used].string[0] = ch;

	    i = 1;
	    while (1) {
		stmformat = get_next_char(stmformat, &ch);
		if ((ch == '%') || (ch == 0))
		    break;
		plf->sl_ptr[plf->sl_used].string[i] = ch;
		i++;
	    }
	    plf->sl_ptr[plf->sl_used].string[i] = 0;

	    plf->sl_used++;
	    if (ch == 0)
		break;		// break the whole while(1) loop
	}

	plf->sl_ptr[plf->sl_used].string[0] = 0;

	/* search for the terminating command */
	stmformat = get_next_char(stmformat, &ch);
	if (ch == '{') {
	    i = 0;
	    while (1) {
		stmformat = get_next_char(stmformat, &ch);
		if (ch == 0) {
		    printf
			("\nERROR: failed to parse configuration file.\n");
		    exit(1);
		} else if (ch == '}')
		    break;
		plf->sl_ptr[plf->sl_used].string[i] = ch;
		i++;
	    }
	    plf->sl_ptr[plf->sl_used].string[i] = 0;

	    // Get next char after '}'
	    stmformat = get_next_char(stmformat, &ch);
	    get_field_value(plf, ch);
	} else {
	    get_field_value(plf, ch);
	    //printf("%d : %d\n", ff_>used, ff_>ptr[ff_>used]->field);
	}

	plf->sl_used++;

    }				// end while

    // We want to do in this way.
    char logid[10];
    for (j = 0; j < plf->sl_used; j++) {
	snprintf(logid, 9, "%d", plf->sl_ptr[j].field);
	switch (plf->sl_ptr[j].field) {
	case FORMAT_CS_URI_STEM:
	case FORMAT_CS_URI:
	    //case FORMAT_TRANSACTION_CODE:
	case FORMAT_R_IP:
	case FORMAT_CLIENT_ID:
	case FORMAT_STREAM_ID:
	case FORMAT_CLIP_START:
	case FORMAT_CLIP_END:
	case FORMAT_PACKETS:
	    //case FORMAT_CON_PROTOCOL:
	    //case FORMAT_TRA_PROTOCOL:
	    //case FORMAT_ACTION:
	case FORMAT_DROPPED_PACKETS:
	    //case FORMAT_PLAY_TIME:
	case FORMAT_PRODUCT:
	case FORMAT_RECENT_PACKET:
	case FORMAT_C_PLAYERVERSION:
	case FORMAT_C_OS:
	case FORMAT_C_OSVERSION:
	case FORMAT_C_CPU:
	    //case FORMAT_FILELENGTH:
	    //case FORMAT_AVG_BANDWIDTH:
	case FORMAT_CS_USER_AGENT:
	    //case FORMAT_E_TIMESTAMP:
	case FORMAT_S_NAMESPACE_NAME:
	    strcpy(&plf->sl_ptr[j].string[0], logid);
	    break;
	default:
	    break;
	}
    }
    free(format_2);
    return 0;
}

////////////////////////////////////////////////////////////////////////
// Get new channel configuration
////////////////////////////////////////////////////////////////////////

void new_channel_callback(char *type);
void new_channel_callback(char *type)
{
    logd_file *plf;
    static int channel_type = 0;
    int nexttype;

    if (strcmp(type, "AccessLog") == 0) {
	nexttype = TYPE_ACCESSLOG;
    } else if (strcmp(type, "ErrorLog") == 0) {
	nexttype = TYPE_ERRORLOG;
    } else if (strcmp(type, "TraceLog") == 0) {
	nexttype = TYPE_TRACELOG;
    } else if (strcmp(type, "CacheLog") == 0) {
	nexttype = TYPE_CACHELOG;
    } else if (strcmp(type, "FuseLog") == 0) {
	nexttype = TYPE_FUSELOG;
    } else if (strcmp(type, "StreamLog") == 0) {
	nexttype = TYPE_STREAMLOG;
    } else if (strcmp(type, "SysLog") == 0) {
	nexttype = TYPE_SYSLOG;
    } else if (strcmp(type, "MfpLog") == 0) {
	nexttype = TYPE_MFPLOG;
    } else if (strcmp(type, "CrawlLog") == 0) {
	nexttype = TYPE_CRAWLLOG;
    } else if (strcmp(type, "CbLog") == 0) {
	nexttype = TYPE_CBLOG;
    } else {
	//return;
	// take last one
    }

    if (channel_type == 0) {
	channel_type = nexttype;
	return;
    }

    plf = (logd_file *) malloc(sizeof(logd_file));
    if (!plf)
	return;
    memset((char *) plf, 0, sizeof(logd_file));

    pthread_mutex_init(&plf->fp_lock, NULL);
    plf->channelid = glob_channelid++;
    plf->type = channel_type;
    plf->filename = filename;
    plf->version_number = 0;
    filename = NULL;
    plf->max_filesize = max_filesize * MiBYTES;
    max_filesize = 0;
    plf->max_fileid = max_fileid;
    max_fileid = 0;
    plf->status = status;
    status = NULL;
    plf->cfgsyslog = cfgsyslog;
    cfgsyslog = NULL;
    plf->format = format;
    format = NULL;
    plf->modstr = modstr;
    modstr = NULL;
    plf->level = level;
    level = 0;
    plf->cfglog_rotation = cfglog_rotation;
    cfglog_rotation = NULL;
    plf->pnm = NULL;
    if (channel_type == TYPE_ACCESSLOG) {
	plf->cur_fileid = accesslogid;
    } else if (channel_type == TYPE_ERRORLOG) {
	plf->cur_fileid = errorlogid;
    } else if (channel_type == TYPE_TRACELOG) {
	plf->cur_fileid = tracelogid;
    } else if (channel_type == TYPE_CACHELOG) {
	plf->cur_fileid = cachelogid;
    } else if (channel_type == TYPE_FUSELOG) {
	plf->cur_fileid = fuselogid;
    } else if (channel_type == TYPE_STREAMLOG) {
	plf->cur_fileid = streamlogid;
    } else if (channel_type == TYPE_MFPLOG) {
	plf->cur_fileid = mfplogid;
    } else if (channel_type == TYPE_CRAWLLOG) {
	plf->cur_fileid = crawllogid;
        crawl_read_version(plf);
    } else if (channel_type == TYPE_CBLOG) {
	plf->cur_fileid = cblogid;
    }

    time_t tv;
    tv = time(NULL);
    plf->st_time = (long) tv;
    plf->rt_interval = time_interval;
    time_interval = 0;
    if (on_hour) {
	if (strncmp(on_hour, "yes", 3) == 0) {
	    free(on_hour);
	    plf->on_the_hour = 1;
	} else {
	    free(on_hour);
	    plf->on_the_hour = 0;
	}
    }
    on_hour = NULL;

    if (show_format) {
	if (strncmp(show_format, "yes", 3) == 0) {
	    free(show_format);
	    plf->show_format = 1;
	} else {
	    free(show_format);
	    plf->show_format = 0;
	}
    }
    show_format = NULL;

    if (plf->cfglog_rotation) {
	if (strncasecmp(plf->cfglog_rotation, "yes", 3) == 0)
	    plf->log_rotation = 1;
	else
	    plf->log_rotation = 0;
    }
    //Log restriction is only applied to accesslog
    if (channel_type == TYPE_ACCESSLOG) {
	plf->r_size = r_size * 1024;	// convert to bytes
	if (l_restriction) {
	    if (strncmp(l_restriction, "yes", 3) == 0) {
		plf->log_restriction = 1;
	    } else {
		plf->log_restriction = 0;
	    }
	}
	if (r_code) {
	    parse_rcode(plf, r_code);
	}
    }
    r_size = 0;
    if (l_restriction) {
	free(l_restriction);
	l_restriction = NULL;
    }
    r_code = NULL;

    pthread_mutex_lock(&log_lock);
    LIST_INSERT_HEAD(&g_lf_head, plf, entries);
    pthread_mutex_unlock(&log_lock);

    channel_type = nexttype;
}

////////////////////////////////////////////////////////////////////////
// General configuration parsing functions
////////////////////////////////////////////////////////////////////////

typedef void (*CFG_func) (char *cfg);

#define TYPE_INT 	1
#define TYPE_STRING 	2
#define TYPE_LONG 	3
#define TYPE_FUNC 	4
#define TYPE_IP	 	5

static struct nkncfg_def {
    const char *label;
    int type;
    void *paddr;
    const char *vdefault;
} cfgdef[] = {

//    { "ServerPort", TYPE_INT,    &serverport, "7958" },
    {
    "LogFolderPath", TYPE_STRING, &log_folderpath, "/var/log/nkn"}, {
    "LogPort", TYPE_INT, &log_port_listen, "7958"}, {
    "AgentAccess", TYPE_INT, &agent_access, "1"}, {
    "LogType", TYPE_FUNC, &new_channel_callback, NULL}, {
    "Status", TYPE_STRING, &status, "active"}, {
    "Syslog", TYPE_STRING, &cfgsyslog, "no"}, {
    "FileName", TYPE_STRING, &filename, NULL}, {
    "Max-FileSize", TYPE_INT, &max_filesize, "100"}, {
    "Log-Rotation", TYPE_STRING, &cfglog_rotation, "no"}, {
    "Max-FileId", TYPE_INT, &max_fileid, "10"}, {
    "On-the-Hour", TYPE_STRING, &on_hour, "no"}, {
    "Time-Interval", TYPE_INT, &time_interval, "0"}, {
    "Format", TYPE_STRING, &format, NULL}, {
    "Show-Format", TYPE_STRING, &show_format, "yes"}, {
    "Level", TYPE_INT, &level, "4"}, {
    "Modules", TYPE_STRING, &modstr, "0x0000000000000000"}, {
    "Log-Restriction", TYPE_STRING, &l_restriction, "no"}, {
    "Object-Size-Restriction", TYPE_INT, &r_size, "0"}, {
    "Resp-Code-Restriction", TYPE_STRING, &r_code, NULL}, {
    NULL, TYPE_INT, NULL, NULL}
};


//////////////////////////////////////////////////////////////////////////////
// Parse the configuration file
//////////////////////////////////////////////////////////////////////////////

static char *mark_end_of_line(char *p);
static char *mark_end_of_line(char *p)
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

static char *skip_to_value(char *p);
static char *skip_to_value(char *p)
{
    while (*p == ' ' || *p == '\t')
	p++;
    if (*p != '=')
	return NULL;
    p++;
    while (*p == ' ' || *p == '\t')
	p++;
    return p;
}

static int read_nkn_cfg(char *configfile);
static int read_nkn_cfg(char *configfile)
{
    FILE *fp;
    char *p;
    char buf[1024];
    int len, i;
    struct nkncfg_def *pcfgdef;

    LIST_INIT(&g_lf_head);
    memset((char *) &cur_plf, 0, sizeof(cur_plf));

    // Read the configuration
    fp = fopen(configfile, "r");
    if (!fp) {
	printf("\n");
	printf("ERROR: failed to open configure file <%s>\n", configfile);
	complain_error_errno(1, "Failed to open %s", configfile);
	//printf("       use all default configuration settings.\n\n");
	return 1;
    }

    while (!feof(fp)) {
	if (fgets(buf, 1024, fp) == NULL)
	    break;

	p = buf;
	if (*p == '#')
	    continue;
	mark_end_of_line(p);
	//printf("%s\n",buf);

	for (i = 0;; i++) {
	    pcfgdef = &cfgdef[i];
	    if (pcfgdef->label == NULL)
		break;

	    len = strlen(pcfgdef->label);
	    if (strncmp(buf, pcfgdef->label, len) == 0) {
		// found one match
		p = skip_to_value(&buf[len]);
		if (pcfgdef->type == TYPE_INT) {
		    int *pint = (int *) pcfgdef->paddr;
		    *pint = atoi(p);
		} else if (pcfgdef->type == TYPE_STRING) {
		    char **pchar = (char **) pcfgdef->paddr;
		    *pchar = strdup(p);
		} else if (pcfgdef->type == TYPE_LONG) {
		    int64_t *plong = (int64_t *) pcfgdef->paddr;
		    *plong = atol(p);
		} else if (pcfgdef->type == TYPE_FUNC) {
		    (*(CFG_func) (pcfgdef->paddr)) (p);
		} else if (pcfgdef->type == TYPE_IP) {
		    uint32_t *plong = (uint32_t *) pcfgdef->paddr;
		    *plong = inet_addr(p);
		}
		break;
	    }
	}
    }
    fclose(fp);

    new_channel_callback((char *) "LastLogType");	// collect last Channel configuration

    return 1;
}

/*
 * convert a hex string to an integrate.
 */
static uint64_t convert_hexstr_2_int64(char *s);
static uint64_t convert_hexstr_2_int64(char *s)
{
    //0x1234567812345678
    uint64_t val = 0;
    int i;

    if (s == NULL)
	return 0;

    for (i = 0; i < 16; i++) {
	if (*s == 0)
	    break;

	val <<= 4;
	if (*s <= '9' && *s >= '0')
	    val += *s - '0';
	else if (*s <= 'F' && *s >= 'A')
	    val += (*s - 'A') + 10;
	else if (*s <= 'f' && *s >= 'a')
	    val += (*s - 'a') + 10;
	else
	    break;

	s++;
    }
    return val;
}

//static int nkn_check_cfg(void); 
/*
 * The function call has been made public for Bug 5527
 * The check config function is invoked only after the log config has
 * been updated from the database
 */
int nkn_check_cfg(void)
{
    logd_file *plf;
    pthread_mutex_lock(&log_lock);
    for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {

	if (plf->cfgsyslog) {
	    if (strncasecmp(plf->cfgsyslog, "yes", 3) == 0) {
		plf->replicate_syslog = TRUE;
	    }
	}
	if (plf->type == TYPE_ACCESSLOG) {
	    printf("format=%s\n", plf->format);
	    accesslog_parse_format(plf);
	} else if (plf->type == TYPE_STREAMLOG) {
	    printf("format=%s\n", plf->format);
	    streamlog_parse_format(plf);
	} else {		/*if(plf->type == TYPE_DEBUGLOG) */

	    if (plf->modstr[0] == '0' && plf->modstr[1] == 'x') {
		plf->mod = convert_hexstr_2_int64(&plf->modstr[2]);
	    }
	}
	if (plf->status) {
	    if (strncasecmp(plf->status, "active", 6) == 0) {
		plf->active = TRUE;
	    }
	}
	if (plf->active) {	//this field could be updated by cli
            if(((plf->type == TYPE_FUSELOG) ||
             (plf->type == TYPE_ERRORLOG) ||
             (plf->type == TYPE_TRACELOG) ||
             (plf->type == TYPE_STREAMLOG) ||
             (plf->type == TYPE_CACHELOG) ||
             (plf->type == TYPE_MFPLOG) ||
             (plf->type == TYPE_CRAWLLOG) ||
             (plf->type == TYPE_CBLOG))) {
                log_file_exit(plf);
            }
	    log_file_init(plf);
	}
    }
    pthread_mutex_unlock(&log_lock);
    return 0;
}

static void print_cfg_info(logd_file * plf);
static void print_cfg_info(logd_file * plf)
{
    int j;
    printf("*****************config********************");
    printf("\nchannelid=%d\n", plf->channelid);

    switch (plf->type) {
    case TYPE_ACCESSLOG:
	printf("type=Access Log\n");
	break;
    case TYPE_ERRORLOG:
	printf("type=Error Log\n");
	break;
    case TYPE_TRACELOG:
	printf("type=Trace Log\n");
	break;
    case TYPE_CACHELOG:
	printf("type=Cache Log\n");
	break;
    case TYPE_FUSELOG:
	printf("type=Fuse Log\n");
	break;
    case TYPE_STREAMLOG:
	printf("type=Stream Log\n");
	break;
    case TYPE_MFPLOG:
	printf("type=Mfp Log\n");
	break;
    case TYPE_SYSLOG:
	printf("type=Sys Log\n");
	break;
    default:
	printf("type=Unknown\n");
	break;
    }

    if (plf->active == TRUE) {
	printf("status=active\n");
    } else {
	printf("status=inactive\n");
    }

    if (plf->replicate_syslog == TRUE) {
	printf("syslog=yes\n");
    } else {
	printf("syslog=no\n");
    }

    printf("filename=%s\n", plf->filename);
    printf("max_filesize=%ld Bytes\n", plf->max_filesize);
    printf("Log Rotation = %s\n", plf->cfglog_rotation);
    printf("max_fileid=%d\n", plf->max_fileid);
    printf("on_the_hour=%d\n", plf->on_the_hour);
    printf("time_interval=%d\n", plf->rt_interval);
    printf("start_time=%ld\n", plf->st_time);
    if (plf->type == TYPE_ACCESSLOG) {
	for (j = 0; j < plf->ff_used; j++) {
	    if (plf->ff_ptr[j].string[0] != 0) {
		printf("field=%d string=<%s>\n",
		       plf->ff_ptr[j].field, plf->ff_ptr[j].string);
	    } else {
		printf("field=%d\n", plf->ff_ptr[j].field);
	    }
	}
	if (plf->log_restriction)
	    printf("Log-Restriction=yes\n");
	else
	    printf("Log-Restriction=no\n");
	printf("Object-Size-Restriction=%d\n", plf->r_size);
	printf("Resp-Code-Restriction=");
	for (j = 0; j < plf->al_rcode_ct; j++) {
	    printf("%d ", plf->r_code[j]);
	}
	printf("\n");
    } else if (plf->type == TYPE_STREAMLOG) {
	for (j = 0; j < plf->sl_used; j++) {
	    if (plf->sl_ptr[j].string[0] != 0) {
		printf("field=%d string=<%s>\n",
		       plf->sl_ptr[j].field, plf->sl_ptr[j].string);
	    } else {
		printf("field=%d\n", plf->sl_ptr[j].field);
	    }
	}
    } else {			/*if(plf->type == TYPE_DEBUGLOG) */

	printf("level=%d\n", plf->level);
	printf("mod=0x%lx\n", plf->mod);
    }
    printf("***************************************\n\n");
}

void nkn_free_cfg(void);
void nkn_free_cfg(void)
{
    logd_file *plf;

    pthread_mutex_lock(&log_lock);
    while (!LIST_EMPTY(&g_lf_head)) {
	plf = g_lf_head.lh_first;
	if (plf->fp)
	    fclose(plf->fp);
	if (plf->filename)
	    free(plf->filename);
	if (plf->status)
	    free(plf->status);
	if (plf->cfgsyslog)
	    free(plf->cfgsyslog);
	if (plf->format)
	    free(plf->format);
	if (plf->modstr)
	    free(plf->modstr);
	if (plf->ff_ptr)
	    free(plf->ff_ptr);
	if (plf->sl_ptr)
	    free(plf->sl_ptr);
	if (plf->cfglog_rotation)
	    free(plf->cfglog_rotation);
	LIST_REMOVE(plf, entries);
	free(plf);
    }
    pthread_mutex_unlock(&log_lock);
}

void nkn_process_cfg(char *cfgfile);
void nkn_process_cfg(char *cfgfile)
{
    logd_file *plf;

    read_nkn_cfg(cfgfile);
    //nkn_check_cfg();

    pthread_mutex_lock(&log_lock);
    for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
	//print_cfg_info(plf);
    }
    pthread_mutex_unlock(&log_lock);

}



/* ****************************************************************************
 * The following functions used for TM integration.
 * ****************************************************************************/
extern int update_from_db_done;	//This flag determines if the config data is updated from the md 
				//db completely before creating log file for the very first time. 

int access_log_query_init(void);

int error_log_query_init(void);

int trace_log_query_init(void);

int cache_log_query_init(void);

int stream_log_query_init(void);

int fuse_log_query_init(void);

int mfp_log_query_init(void);

logd_file *get_log_config(int type);

int log_async_shutdown(logd_file * plf, log_network_mgr_t **pnm);
void log_free_sock_and_addto_freelist(log_network_mgr_t *pnm,
				      logd_file * plf);


int access_log_cfg_handle_add_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data);

int access_log_cfg_handle_delete_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data);

int error_log_cfg_handle_change(const bn_binding_array * arr,
				uint32 idx,
				bn_binding * binding, void *data);

int trace_log_cfg_handle_change(const bn_binding_array * arr,
				uint32 idx,
				bn_binding * binding, void *data);

int cache_log_cfg_handle_change(const bn_binding_array * arr,
				uint32 idx,
				bn_binding * binding, void *data);

int stream_log_cfg_handle_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data);

int fuse_log_cfg_handle_change(const bn_binding_array * arr,
			       uint32 idx,
			       bn_binding * binding, void *data);


int mfp_log_cfg_handle_change(const bn_binding_array * arr,
			      uint32 idx,
			      bn_binding * binding, void *data);

extern void syslog_init(logd_file * plf);

int
module_cfg_handle_change(bn_binding_array * old_bindings,
			 bn_binding_array * new_bindings);

#if 0
int access_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_NOTICE, "accesslog initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, al_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, access_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

    bn_binding_array_free(&bindings);
    bindings = NULL;
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL, NULL, true, &bindings, false,
				   true, "/license/key");
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, access_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    bn_binding_array_free(&bindings);
    return err;

}
#endif

logd_file *get_log_config(int type)
{

    logd_file *plf = NULL;
    for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
	if (plf->type == type)
	    break;
    }

    if (plf == NULL && type == TYPE_ACCESSLOG) {
	/*
	 * This case should never happen because nknlogd configuration
	 * will always create one plf for accesslog.
	 * Not sure why this piece of code is here.
	 */
	plf = (logd_file *) malloc(sizeof(logd_file));
	if (!plf) {
	    return NULL;
	}
	memset((char *) plf, 0, sizeof(logd_file));
	pthread_mutex_init(&plf->fp_lock, NULL);
	plf->type = TYPE_ACCESSLOG;
	plf->replicate_syslog = FALSE;
	plf->max_filesize = 100 * MiBYTES;
	plf->max_fileid = 10;
	plf->cur_fileid = accesslogid;
	plf->filename = strdup("access.log");
	init_format_field(plf);
	plf->format =
	    strdup
	    ("\%h \%V \%u \%t \"%r\" \%s \%b \"\%{Referer}i\" \"\%{User-Agent}i\"");
	accesslog_parse_format(plf);
	plf->pnm = NULL;
	LIST_INSERT_HEAD(&g_lf_head, plf, entries);
    }
    return plf;
}


/*! assign network manager pointer for later async processing */
int log_async_shutdown(logd_file * plf, log_network_mgr_t **log_network)
{
    if (log_network && plf->pnm) {
	*log_network = plf->pnm;
    }
    return 0;
}

/*! unregister the socket && 
    free network manager pointer to the free pool list */
void log_free_sock_and_addto_freelist(log_network_mgr_t *pnm, logd_file * plf)
{
    if (pnm && plf) {
	pthread_mutex_lock(&pnm->network_manager_log_lock);
	if (LOG_CHECK_FLAG(pnm, LOGD_NW_MGR_MAGIC_ALIVE)) {
	    pthread_mutex_lock(&log_lock);
	    plf->pnm = NULL;
	    pthread_mutex_unlock(&log_lock);
	    unregister_free_close_sock(pnm);
	}
	pthread_mutex_unlock(&pnm->network_manager_log_lock);
    }
}


#if 0
int access_log_cfg_handle_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    tstring *str = NULL;
    uint16 serv_port = 0;
    tbool bool_value = false;
    tbool *rechecked_licenses_p = data;
    logd_file *plf = NULL;
    struct channel *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, al_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);
    } else if (ts_equal_str(name, al_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("accesslog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("accesslog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, al_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    if (plf->filename)
		free(plf->filename);
	    plf->filename = strdup(ts_str(str));
	    lc_log_basic(LOG_DEBUG,
			 "accesslog: updated filename to \"%s\"",
			 plf->filename);
	    if (update_from_db_done)	//we can create the new file
		log_file_init(plf);
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, al_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG,
			 "accesslog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, al_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG,
			 "accesslog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }

	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, al_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "accesslog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- FORMAT STRING ------------*/
    else if (ts_equal_str(name, al_format_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (ts_length(str) == 0) {
	    log_basic("Bad or NULL format specifier");
	    goto bail;
	}

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    if (plf->format)
		free(plf->format);
	    init_format_field(plf);
	    plf->format = strdup(ts_str(str));
	    lc_log_basic(LOG_DEBUG,
			 "accesslog: updated format string to %s",
			 plf->format);
	    accesslog_parse_format(plf);
	    log_async_shutdown(plf, &pnm);
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    }
	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, al_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ACCESSLOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "accesslog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ACCESSLOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "accesslog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, al_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ACCESSLOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "accesslog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ACCESSLOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "accesslog: updated url passwordremoved \n ");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, al_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }
    /* -------- Format display in logs------- */
    else if (ts_equal_str(name, al_format_display_binding, false)) {
	tbool t_display = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_display);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->show_format = t_display;
	}
	pthread_mutex_unlock(&log_lock);

    }
    /* -------- Max-file id  in logs------- */
    else if (ts_equal_str(name, al_max_fileid_binding, false)) {
	uint32 fileid = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &fileid);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->max_fileid = fileid;
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*---------- Log Restriction Settings -------*/
    else if (ts_equal_str(name, al_log_restriction_binding, false)) {
	tbool t_restriction = false;
	err =
	    bn_binding_get_tbool(binding, ba_value, NULL, &t_restriction);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (t_restriction && plf != NULL) {
	    plf->log_restriction = 1;
	} else {
	    plf->log_restriction = 0;
	}

	lc_log_basic(LOG_DEBUG, "accesslog: Log restriction %d",
		     (uint32_t) plf->log_restriction);
	pthread_mutex_unlock(&log_lock);
    }
	/*---------- Object size Restriction ----------*/
    else if (ts_equal_str(name, al_r_size_binding, false)) {
	uint32 o_size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &o_size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->r_size = o_size * 1024;	// convert to bytes
	}
	lc_log_basic(LOG_DEBUG, "accesslog: Log object size %d",
		     (uint32_t) plf->r_size);
	o_size = 0;
	pthread_mutex_unlock(&log_lock);
    }
	/*----------- Response code Restriction ------------*/
    else if (ts_equal_str(name, al_r_code_binding, false)) {
	static char *t_code = NULL;
	int i = 0;
	err =
	    bn_binding_get_str(binding, ba_value, bt_string, NULL,
			       &t_code);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	/*Reset the response code and count to zero for the no form */
	if (t_code && strcmp(t_code, "") == 0) {
	    while (i < plf->al_rcode_ct) {
		plf->r_code[i] = 0;
		i++;
	    }
	    plf->al_rcode_ct = 0;
	    t_code = NULL;
	}

	if (t_code && plf != NULL) {
	    parse_rcode(plf, t_code);
	}

	t_code = NULL;
	pthread_mutex_unlock(&log_lock);
    }

	/*----------- Log rotation Enable/Disable ------------*/
    else if (ts_equal_str(name, al_rotate_binding, false)) {
	tbool t_rotate = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rotate);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);

	if (t_rotate && plf != NULL) {
	    plf->acclog_rotation = 1;
	} else {
	    plf->acclog_rotation = 0;
	}

	lc_log_basic(LOG_DEBUG, "accesslog: Log rotation %d",
		     (uint32_t) plf->acclog_rotation);
	pthread_mutex_unlock(&log_lock);
    }

	/*------------ Enable/Disable Log Analyzer --------------*/
    else if (ts_equal_str(name, al_analyzer_binding, false)) {
	tbool t_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ACCESSLOG);
	if (plf != NULL) {
	    plf->al_analyzer_tool_enable = (t_enable) ? 1 : 0;
	}
	pthread_mutex_unlock(&log_lock);
    }

  bail:
    ts_free(&str);
    tstr_array_free(&name_parts);
    return err;
}
#endif



int error_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;


    lc_log_basic(LOG_DEBUG, "nvsd error log query initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, system_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, error_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);
    bindings = NULL;

    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, er_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, error_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);
  bail:
    bn_binding_array_free(&bindings);
    return err;
}


int
error_log_cfg_handle_change(const bn_binding_array * arr, uint32 idx,
			    bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
    tstring *str = NULL;

    logd_file *plf = NULL;
    log_network_mgr_t *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, er_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    } else if (ts_equal_str(name, er_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("errorlog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("errorlog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, er_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
            if (update_from_db_done){
                log_file_exit(plf);
            }
            if (plf->filename)
                free(plf->filename);
            plf->filename = strdup(ts_str(str));
            lc_log_basic(LOG_DEBUG, "errorlog: updated filename to \"%s\"",
                         plf->filename);
            if (update_from_db_done){
                log_file_init(plf);
            }
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, er_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG,
			 "errorlog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, er_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG,
			 "errorlog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, er_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "errorlog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, system_debug_level_binding, false)
	       || ts_equal_str(name, er_debug_level_binding, false)) {
	uint32 alevel = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &alevel);

	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    lc_log_basic(LOG_DEBUG,
			 "errorlog: debug level set to : \"%d\"", alevel);
	    plf->level = alevel;
	    log_async_shutdown(plf, &pnm);
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);
    }
	/*-------- debug mod ------------*/
    else if (ts_equal_str(name, system_debug_mod_binding, false)
	     || ts_equal_str(name, er_debug_mod_binding, false)) {
	tstring *mod = NULL;
	uint64_t dbg_log_mod;
	char strmod[256];
	char *p;
	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL, &mod);

	bail_error(err);
	snprintf(strmod, 255, "%s", ts_str(mod));
	p = &strmod[0];
	if ((strlen(p) >= 3) && (p[0] == '0') && (p[1] == 'x')) {
	    dbg_log_mod = convert_hexstr_2_int64(p + 2);
	} else {
	    dbg_log_mod = atol(p);
	}
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    lc_log_basic(LOG_DEBUG, "errorlog: debug mod set to : %s",
			 ts_str(mod));
	    plf->mod = dbg_log_mod;
	    log_async_shutdown(plf, &pnm);
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);
	ts_free(&mod);
    }
	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, er_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ERRORLOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "errorlog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ERRORLOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG, "errorlog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, er_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ERRORLOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "errorlog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_ERRORLOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "errorlog: updated url password removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, er_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }
	/*----------- Log rotation Enable/Disable ------------*/

    else if (ts_equal_str(name, er_rotate_binding, false)) {
	tbool t_rotate;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rotate);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_ERRORLOG);

	if (plf != NULL) {
	    if(t_rotate){
		plf->errlog_rotation = 1;
	    } else {
		plf->errlog_rotation = 0;
	    }
	    lc_log_basic(LOG_DEBUG, "errorlog: Log rotation %d",
		     (uint32_t) plf->errlog_rotation);
	}
	pthread_mutex_unlock(&log_lock);
    }

  bail:
    ts_free(&str);
    return err;

}


int trace_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "trace log query initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, tr_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, trace_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    bn_binding_array_free(&bindings);

    return err;

}


int
trace_log_cfg_handle_change(const bn_binding_array * arr, uint32 idx,
			    bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
    tstring *str = NULL;

    logd_file *plf = NULL;
    log_network_mgr_t *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, tr_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    } else if (ts_equal_str(name, tr_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("tracelog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("tracelog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, tr_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
            if (update_from_db_done){
                log_file_exit(plf);
            }
            if (plf->filename)
                free(plf->filename);
            plf->filename = strdup(ts_str(str));
            lc_log_basic(LOG_DEBUG, "tracelog: updated filename to \"%s\"",
                         plf->filename);
            if (update_from_db_done){
                log_file_init(plf);
            }
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, tr_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG,
			 "tracelog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, tr_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG,
			 "tracelog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, tr_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "tracelog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, tr_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_TRACELOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "tracelog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_TRACELOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG, "tracelog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, tr_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_TRACELOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "tracelog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_TRACELOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "tracelog: updated url password removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, tr_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }




  bail:
    ts_free(&str);
    return err;

}

int cache_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "cache log query initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, cache_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, cache_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    bn_binding_array_free(&bindings);

    return err;

}


int
cache_log_cfg_handle_change(const bn_binding_array * arr, uint32 idx,
			    bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;

    logd_file *plf = NULL;
    tstring *str = NULL;
    log_network_mgr_t *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, cache_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    } else if (ts_equal_str(name, cache_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("cachelog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("cachelog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, cache_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
            if (update_from_db_done){
                log_file_exit(plf);
            }
            if (plf->filename)
                free(plf->filename);
            plf->filename = strdup(ts_str(str));
            lc_log_basic(LOG_DEBUG, "cachelog: updated filename to \"%s\"",
                         plf->filename);
            if (update_from_db_done){
                log_file_init(plf);
            }
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, cache_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG,
			 "cachelog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, cache_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG,
			 "cachelog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }

	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, cache_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "cachelog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    }

	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, cache_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_CACHELOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "cachelog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_CACHELOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG, "cachelog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, cache_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_CACHELOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "cachelog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_CACHELOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG, "cachelog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, cache_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }
	/*----------- Log rotation Enable/Disable ------------*/

    else if (ts_equal_str(name, cache_rotate_binding, false)) {
	tbool t_rotate;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rotate);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_CACHELOG);

	if (plf != NULL) {
	    if(t_rotate){
		plf->cachelog_rotation = 1;
	    } else {
		plf->cachelog_rotation = 0;
	    }
	    lc_log_basic(LOG_DEBUG, "cachelog: Log rotation %d",
		(uint32_t) plf->cachelog_rotation);
	}
	pthread_mutex_unlock(&log_lock);
    }


  bail:
    ts_free(&str);
    return err;
}

int stream_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "stream log query initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, stream_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, stream_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    bn_binding_array_free(&bindings);
    return err;

}



int stream_log_cfg_handle_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstring *str = NULL;
    tstr_array *name_parts = NULL;
    tbool bool_value = false;
    tbool *rechecked_licenses_p = data;
    logd_file *plf = NULL;
    log_network_mgr_t *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, stream_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    } else if (ts_equal_str(name, stream_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("streamlog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("streamlog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, stream_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
            if (update_from_db_done){
                log_file_exit(plf);
            }
            if (plf->filename)
                free(plf->filename);
            plf->filename = strdup(ts_str(str));
            lc_log_basic(LOG_DEBUG, "streamlog: updated filename to \"%s\"",
                         plf->filename);
            if (update_from_db_done){
                log_file_init(plf);
            }
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, stream_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG,
			 "streamlog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, stream_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG,
			 "streamlog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, stream_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "streamlog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- FORMAT STRING ------------*/
    else if (ts_equal_str(name, stream_format_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (ts_length(str) == 0) {
	    log_basic("Bad or NULL format specifier");
	    goto bail;
	}

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    if (plf->format)
		free(plf->format);
	    init_format_field(plf);
	    plf->format = strdup(ts_str(str));
	    lc_log_basic(LOG_DEBUG,
			 "streamlog: updated format string to %s",
			 plf->format);
	    streamlog_parse_format(plf);
	    log_async_shutdown(plf, &pnm);
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    }
	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, stream_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_STREAMLOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "streamlog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_STREAMLOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG, "stream: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, stream_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_STREAMLOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "streamlog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_STREAMLOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "streamlog: updated url password removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, stream_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }
    /* -------- Format display in logs------- */
    else if (ts_equal_str(name, stream_format_display_binding, false)) {
	tbool t_display;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_display);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);
	if (plf != NULL) {
	    plf->show_format = t_display;
	}
	pthread_mutex_unlock(&log_lock);

    }
	/*----------- Log rotation Enable/Disable ------------*/

    else if (ts_equal_str(name, stream_rotate_binding, false)) {
	tbool t_rotate;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rotate);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_STREAMLOG);

	if (plf != NULL) {
	    if(t_rotate){
		plf->streamlog_rotation = 1;
	    } else {
		plf->streamlog_rotation = 0;
	    }
	    lc_log_basic(LOG_DEBUG, "streamlog: Log rotation %d",
		     (uint32_t) plf->streamlog_rotation);
	}
	pthread_mutex_unlock(&log_lock);
    }

  bail:
    ts_free(&str);
    tstr_array_free(&name_parts);
    return err;
}


int fuse_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "fuse log query initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, fuse_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, fuse_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    bn_binding_array_free(&bindings);

    return err;

}


int
fuse_log_cfg_handle_change(const bn_binding_array * arr, uint32 idx,
			   bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;

    logd_file *plf = NULL;
    tstring *str = NULL;
    log_network_mgr_t *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, fuse_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    } else if (ts_equal_str(name, fuse_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("fuselog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("fuselog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, fuse_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    if (update_from_db_done){
                log_file_exit(plf);
            }
            if (plf->filename)
                free(plf->filename);
            plf->filename = strdup(ts_str(str));
            lc_log_basic(LOG_DEBUG, "fuselog: updated filename to \"%s\"",
                         plf->filename);
            if (update_from_db_done){
		log_file_init(plf);
            }
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, fuse_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG,
			 "fuselog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, fuse_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG, "fuselog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }

	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, fuse_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "fuselog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    }

	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, fuse_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_FUSELOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "fuselog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_FUSELOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG, "fuselog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, fuse_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_FUSELOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "fuselog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_FUSELOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "fuselog: updated url password removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, fuse_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_FUSELOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }


  bail:
    ts_free(&str);
    return err;
}


int mfp_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "mfp log query initializing ..");
    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true, mfp_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, mfp_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    bn_binding_array_free(&bindings);

    return err;

}


int
mfp_log_cfg_handle_change(const bn_binding_array * arr, uint32 idx,
			  bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
    tstring *str = NULL;

    logd_file *plf = NULL;
    log_network_mgr_t *pnm = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (ts_equal_str(name, mfp_log_active, false)) {
	tbool active_flag = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &active_flag);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_MFPLOG);
	if (plf != NULL) {
	    if (plf->active != active_flag) {
		plf->active = active_flag;
		log_async_shutdown(plf, &pnm);
	    }
	}
	pthread_mutex_unlock(&log_lock);
	log_free_sock_and_addto_freelist(pnm, plf);

    } else if (ts_equal_str(name, mfp_syslog_binding, false)) {
	tbool syslog_enable = false;
	err = bn_binding_get_tbool(binding, ba_value, NULL,
				   &syslog_enable);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_MFPLOG);
	if (plf != NULL) {
	    if (syslog_enable == true) {
		plf->replicate_syslog = 1;
		syslog_init(plf);
		log_debug("mfplog: Syslog replication enabled");
	    } else {
		plf->replicate_syslog = 0;
		log_debug("mfplog: Syslog replication disabled");
	    }
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, mfp_filename_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_MFPLOG);
	if (plf != NULL) {
            if (update_from_db_done){
                log_file_exit(plf);
            }
            if (plf->filename)
                free(plf->filename);
            plf->filename = strdup(ts_str(str));
            lc_log_basic(LOG_DEBUG, "mfplog: updated filename to \"%s\"",
                         plf->filename);
            if (update_from_db_done){
                log_file_init(plf);
            }
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- MAX PER FILE SIZE ------------*/
    else if (ts_equal_str(name, mfp_filesize_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	/* Note that check for whether filesize > diskspace
	 * is done in the mgmtd counterpart
	 */
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_MFPLOG);
	if (plf != NULL) {
	    plf->max_filesize = (int64_t) size *MiBYTES;
	    lc_log_basic(LOG_DEBUG, "mfplog: updated max-filesize to %lld",
			 (long long int) plf->max_filesize);
	}
	pthread_mutex_unlock(&log_lock);
    } else if (ts_equal_str(name, mfp_timeinterval_binding, false)) {
	uint32 size = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_MFPLOG);
	if (plf != NULL) {
	    plf->rt_interval = size;
	    lc_log_basic(LOG_DEBUG, "mfplog: updated time-interval to %d",
			 (uint32_t) plf->rt_interval);
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*-------- TOTAL DISKSPACE ------------*/
    else if (ts_equal_str(name, mfp_diskspace_binding, false)) {
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_TRACELOG);
	if (plf != NULL) {
	    plf->max_fileid =
		((int64_t) size * MiBYTES) / plf->max_filesize + 1;
	    lc_log_basic(LOG_DEBUG,
			 "mfplog: updated total-diskspace to %lld",
			 (long long int) ((int64_t) size * MiBYTES));
	}
	pthread_mutex_unlock(&log_lock);
    }
	/*------- REMOTE URL FOR SCP --------*/
    else if (ts_equal_str(name, mfp_upload_rurl_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	log_debug("Remote URL: %s\n", ts_str(str));

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_MFPLOG);
	    if (plf != NULL) {
		if (plf->remote_url)
		    free(plf->remote_url);
		plf->remote_url = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG, "mfplog: updated url to %s",
			     plf->remote_url);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_MFPLOG);
	    if (plf != NULL) {
		if (plf->remote_url) {
		    free(plf->remote_url);
		    plf->remote_url = NULL;
		}
		lc_log_basic(LOG_DEBUG, "mfplog: updated url removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
	/*------- PASS FOR SCP --------*/
    else if (ts_equal_str(name, mfp_upload_pass_binding, false)) {
	err = bn_binding_get_tstr(binding, ba_value, bt_string,
				  NULL, &str);
	bail_error(err);

	if (str && ts_length(str)) {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_MFPLOG);
	    if (plf != NULL) {
		if (plf->remote_pass)
		    free(plf->remote_pass);
		plf->remote_pass = strdup(ts_str(str));
		lc_log_basic(LOG_DEBUG,
			     "mfplog: updated url password to %s",
			     plf->remote_pass);
	    }
	    pthread_mutex_unlock(&log_lock);
	} else {
	    pthread_mutex_lock(&log_lock);
	    plf = get_log_config(TYPE_MFPLOG);
	    if (plf != NULL) {
		if (plf->remote_pass) {
		    free(plf->remote_pass);
		    plf->remote_pass = NULL;
		}
		lc_log_basic(LOG_DEBUG,
			     "mfplog: updated url password removed\n");
	    }
	    pthread_mutex_unlock(&log_lock);
	}
    }
    /* -------- On the hour rotation------- */
    else if (ts_equal_str(name, mfp_on_the_hour_rotate_binding, false)) {
	tbool t_onthehour;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_onthehour);
	bail_error(err);
	pthread_mutex_lock(&log_lock);
	plf = get_log_config(TYPE_MFPLOG);
	if (plf != NULL) {
	    plf->on_the_hour = t_onthehour;
	}
	pthread_mutex_unlock(&log_lock);

    }




  bail:
    ts_free(&str);
    return err;
}

int
module_cfg_handle_change(bn_binding_array * old_bindings,
			 bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*! access log handle change callback */
    err =
	bn_binding_array_foreach(new_bindings,
				 access_log_cfg_handle_add_change,
				 &rechecked_licenses);
    bail_error(err);

    /*! error log handle change callback */
    err =
	bn_binding_array_foreach(new_bindings, error_log_cfg_handle_change,
				 &rechecked_licenses);
    bail_error(err);

    /*! trace log handle change callback */
    err =
	bn_binding_array_foreach(new_bindings, trace_log_cfg_handle_change,
				 &rechecked_licenses);
    bail_error(err);

    /*! cache log handle change callback */
    err =
	bn_binding_array_foreach(new_bindings, cache_log_cfg_handle_change,
				 &rechecked_licenses);
    bail_error(err);

    /*! stream log handle change callback */
    err =
	bn_binding_array_foreach(new_bindings,
				 stream_log_cfg_handle_change,
				 &rechecked_licenses);
    bail_error(err);

    /*! fuse log handle change callback */
    err =
	bn_binding_array_foreach(new_bindings, fuse_log_cfg_handle_change,
				 &rechecked_licenses);
    bail_error(err);

    /*! mfp log handle change callback */
    err = bn_binding_array_foreach(new_bindings, mfp_log_cfg_handle_change,
				   &rechecked_licenses);
    bail_error(err);


    /* Handle accesslog config deletes */
    err = bn_binding_array_foreach(old_bindings,
				   access_log_cfg_handle_delete_change,
				   &rechecked_licenses);
    bail_error(err);

  bail:
    return err;
}

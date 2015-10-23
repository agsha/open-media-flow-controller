/*
 * nkn_oomgr.c -- Fqueue based offline origin manager
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <poll.h>
#include <uuid/uuid.h>

#include "nkn_common_config.h"

#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "http_header.h"
#include "fqueue.h"
#include "nkn_mem_counter_def.h"

int oomgr_debug_output;

#if 1
#define DBG(_fmt, ...) { \
	if (oomgr_debug_output) { \
	    fprintf(stderr, (_fmt), __VA_ARGS__); \
	    fprintf(stderr, "\n"); \
	} \
}
#else
#define DBG(_fmt, ...)
#endif

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

/* Local Macros */
#define MAX_LINE_SIZE		4096
#define	WGET_LOG_FILE		"/nkn/.oomgr.nkn.wget.log"
#define	WGET_DATA_DIR_TEMPLATE	"/nkn/oomgr.nkn.wget."
#define	WGET_DATA_FILE_TEMPLATE	"wget.data."
#define OOMGR_QUEUE_FILE 	"/tmp/OOMGR.queue"
#define FMGR_QUEUE_FILE 	"/tmp/FMGR.queue"
#define WGET_200_OK_STR		"200 OK"

static int cleanup_datafiles = 0;
static int do_not_create_daemon = 0;
static const char *input_queue_filename = OOMGR_QUEUE_FILE;
static const char *output_queue_filename = FMGR_QUEUE_FILE;
static int input_queuefile_maxsize = 1024;
static int enqueue_retries = 2;
static int enqueue_retry_interval_secs = 1;
static fhandle_t enqueue_fh = 0;

/* Local Variables */
static fqueue_element_t qe;
static fqueue_element_t output_qe;
static char data_dirname[256];
static int data_filenumber = 1;

/* Local Function Prototypes */
void 	remove_datafiles(void);
void 	create_data_info_file(const char *data_directory_name);
int 	pid_file_exist(void);
void	create_pid_file (void);
void	exit_originmgr (void);
void	termination_handler (int nSignum);
void	print_usage (char *cpProgName);
int 	get_response_headers(FILE *fpWgetLog, mime_header_t *resphdrs);
boolean	wget_200_ok(mime_header_t *resphdrs);
int 	getoptions(int argc, char *argv[]);
void 	daemonize(void);
int 	send_response_data(const mime_header_t *httpresp,
		const char *content_filename, 
		const char *uri, int uri_len,
		const char *nstokendata, int nstokendata_len,
		const char *fq_destination, int fq_destination_len,
		const char *fq_cookie, int fq_cookie_len);
void 	add_wget_header_arg(char *buf, int *bytesused, 
			const char *name, int name_len,
			const char *data, int data_len);
int 	send_request(const fqueue_element_t *qe_t);
void 	request_handler(void);

/*****************************************************************************
*	Function : remove_datafiles()
*	Purpose : Remove data files associated with last incarnation
*	Parameters : none
*	Return Value : void
*****************************************************************************/
void remove_datafiles(void)
{
    FILE	*fpDataInfoFile;
    char	szLine[MAX_LINE_SIZE + 1];
    int 	bytesused;

    strcpy(szLine, "rm -rf ");
    bytesused = strlen(szLine);

    fpDataInfoFile = fopen(OORIGINMGR_DATA_INFO_FILE, "r");
    if (fpDataInfoFile != NULL) {
        if (fgets(&szLine[bytesused], 
		MAX_LINE_SIZE - bytesused, fpDataInfoFile)) {
	    DBG("%s:%d executing \"%s\"", __FUNCTION__, __LINE__, szLine);
	    system(szLine);
	}
	fclose(fpDataInfoFile);
    }
}

/*****************************************************************************
*	Function : create_data_info_file()
*	Purpose : create the data info file
*	Parameters : directory name
*	Return Value : void
*****************************************************************************/
void create_data_info_file(const char *data_directory_name)
{
    FILE	*fpDataInfoFile;

    /* Write this to the data info file */
    fpDataInfoFile = fopen(OORIGINMGR_DATA_INFO_FILE, "w");
    if (fpDataInfoFile != NULL) {
    	fprintf(fpDataInfoFile, "%s", data_directory_name);
    	fclose(fpDataInfoFile);
    } else {
	fprintf(stderr, "error : Failed to create the "
	     	"data info file <%s>\n", OORIGINMGR_DATA_INFO_FILE);
	exit(1);
    }
    return;
}

/*****************************************************************************
*	Function : pid_file_exist()
*	Purpose : Check for presence of pid file
*	Parameters : void
*	Return Value : 1 if exists else 0
*****************************************************************************/
int pid_file_exist(void)
{
    FILE	*fpPidFile;
    fpPidFile = fopen(OORIGINMGR_PID_FILE, "r");
    if (fpPidFile != NULL) {
    	fclose(fpPidFile);
    	return 1;
    } else {
    	return 0;
    }
}

/*****************************************************************************
*	Function : create_pid_file()
*	Purpose : create the pid file to track the process
*	Parameters : void
*	Return Value : void
*****************************************************************************/
void create_pid_file(void)
{
    FILE	*fpPidFile;
    pid_t	pidSelf;

    /* Get the pid */
    pidSelf = getpid();

    /* Write this to the pid file */
    fpPidFile = fopen(OORIGINMGR_PID_FILE, "w");
    if (fpPidFile != NULL) {
    	fprintf(fpPidFile, "%d", pidSelf);
    	fclose(fpPidFile);
    } else {
	fprintf(stderr, "error : Failed to create the "
	     	"pid file <%s>\n", OORIGINMGR_PID_FILE);
	exit(2);
    }
    return;
}

/*****************************************************************************
*	Function : exit_originmgr()
*	Purpose : function called when app is exiting. Primary activity is to 
*		remove the pid file
*	Parameters : cpProgName - the argv[0] value
*	Return Value : void
*****************************************************************************/
void exit_originmgr(void)
{
    /* unlink the pid file */
    unlink(OORIGINMGR_PID_FILE);
    return;
}

/*****************************************************************************
*	Function : termination_handler()
*	Purpose : signal handler for SIGTERM, SIGINT and SIGHUP
*	Parameters : int signum 
*	Return Value : void
*****************************************************************************/
void termination_handler(int nSignum)
{
    if (nSignum == SIGUSR1) {
	oomgr_debug_output = oomgr_debug_output ? 0 : 1;
	fprintf(stderr, "OOMGR debug logging [%s]\n", 
		oomgr_debug_output ?  "enabled" : "disabled");
	fflush(stderr);
	return;
    }
    /* Call the exit routine */
    exit_originmgr(); 
    exit(3);
}

/*****************************************************************************
*	Function : print_usage()
*	Purpose : print the usage of this utility namely command line options
*	Parameters : cpProgName - the argv[0] value
*	Return Value : void
*****************************************************************************/
void print_usage(char *cpProgName)
{
    fprintf(stderr, "usage : %s [-D-I:-O:-c-h-i:-r:-d:]\n", cpProgName);
    fprintf(stderr, "\t -D : Do not create daemon process \n");
    fprintf(stderr, "\t -I : Input queue filename (default: %s)\n", 
    	input_queue_filename);
    fprintf(stderr, "\t -O : Output queue filename (default: %s)\n",
    	output_queue_filename);
    fprintf(stderr, "\t -c : Cleanup data files from last incarnation\n");
    fprintf(stderr, "\t -h : Help\n");
    fprintf(stderr, "\t -i : Input queue maxsize (default: %d)\n", 
    	input_queuefile_maxsize);
    fprintf(stderr, "\t -r : Output queue retries (default: %d)\n", 
    	enqueue_retries);
    fprintf(stderr, "\t -d : Output queue retry delay in secs (default: %d)\n", 
    	enqueue_retry_interval_secs);
    fprintf(stderr, "\t -v : verbose output (default: no)\n");
    return;
}

/*****************************************************************************
*	Function : get_response_headers()
*	Purpose : Parse wget.log file for response headers and insert
*		them into the given mime_header_t.
*	Parameters : FILE * and http_header
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
int get_response_headers(FILE *fpWgetLog, mime_header_t *resphdrs)
{
#define SKIP_SPACE(_p) while ((_p) && *(_p) == ' ') (_p)++
    int 		rv = 0;
    char		szLine [MAX_LINE_SIZE + 1];
    const char		*p;
    const char		*p_name;
    const char		*p_name_end;
    const char		*p_value;
    const char		*p_value_end;
    int 		hdr_id;

    while (NULL != fgets(szLine, MAX_LINE_SIZE, fpWgetLog)) {
    	if (strncasecmp(szLine, "Length:", 7) == 0) {
	    // End of response headers
	    break;
	}
	p = szLine;
	SKIP_SPACE(p);
	if (p) {
	    p_name = p;
	    p_name_end = strchr(p, ':');
	    if (p_name_end) {
	    	p = p_name_end + 1;
		SKIP_SPACE(p);
		if (p) {
	    	    p_value = p;
		    p_value_end = strchr(p, '\n');
		    if (p_value_end) {
		    	rv = string_to_known_header_enum(p_name, 
						p_name_end - p_name,
						&hdr_id);
			if (!rv) {
			    if (!http_end2end_header[hdr_id]) {
			    	continue;
			    }
			    rv = add_known_header(resphdrs, hdr_id, p_value,
			    			p_value_end - p_value);
			    if (rv) {
			    	TSTR(p_name, p_name_end - p_name, tpname);
			    	TSTR(p_value, p_value_end - p_value, tpvalue);
			    	DBG("%s:%d add_known_header() failed rv=%d, "
				    "name=%s value=%s",
				    __FUNCTION__, __LINE__, rv, 
				    tpname, tpvalue);
				rv = 1;
				break;
			    }
			} else {
			    rv = add_unknown_header(resphdrs, p_name,
			    			p_name_end - p_name,
						p_value, p_value_end - p_value);
			    if (rv) {
			    	TSTR(p_name, p_name_end - p_name, tpname);
			    	TSTR(p_value, p_value_end - p_value, tpvalue);
			    	DBG("%s:%d add_unknown_header() failed rv=%d, "
				    "name=%s value=%s",
				    __FUNCTION__, __LINE__, rv, 
				    tpname, tpvalue);
				rv = 2;
				break;
			    }
			}
		    }
		}
	    }
	}
    } // End of while

    return rv;
}
/*****************************************************************************
*	Function : wget_200_ok()
*	Purpose : This function reads the wget.log file in /tmp to see 
*		if the wget call was successful. If not 200 OK then 
*		video fetched failed
*	Parameters : void
*	Return Value : boolean - NKN_TRUE if 200 ok else NKN_FALSE
*****************************************************************************/
boolean wget_200_ok(mime_header_t *resphdrs)
{
    int 	rv;
    FILE	*fpWgetLog;
    char	*cp200ok;
    char	szLine [MAX_LINE_SIZE + 1];

    fpWgetLog = fopen(WGET_LOG_FILE, "r");
    if (fpWgetLog == NULL)
	    return NKN_FALSE;

    while (NULL != fgets(szLine, MAX_LINE_SIZE, fpWgetLog)) {
	cp200ok = strstr(szLine, WGET_200_OK_STR);
	if (cp200ok != NULL) {
	    if (resphdrs) {
	    	rv = get_response_headers(fpWgetLog, resphdrs);
	    }
	    fclose(fpWgetLog);
	    return NKN_TRUE;
	}
    }

    /* No 200 Ok in the log hence wget failed */
    fclose(fpWgetLog);
    fpWgetLog = fopen(WGET_LOG_FILE, "r");
    while (fgets(szLine, MAX_LINE_SIZE, fpWgetLog) != NULL)
	    puts(szLine);
    fclose(fpWgetLog);
    return NKN_FALSE;
}

/*****************************************************************************
*	Function : getoptions()
*	Purpose : Parse command line arguments
*	Parameters : argc, argv - passed as is from main
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
int getoptions(int argc, char *argv[])
{
    char 	chTemp;

    input_queue_filename = OOMGR_QUEUE_FILE;
    output_queue_filename = FMGR_QUEUE_FILE;

    /* Using getopt get the parameters */
    while ((chTemp = getopt(argc, argv, "DI:O:chi:r:d:v")) != -1) {
    	switch (chTemp) {
	case 'D':
	    do_not_create_daemon = 1;
	    break;
	case 'I':
	    input_queue_filename = optarg;
	    break;
	case 'O':
	    output_queue_filename = optarg;
	    break;
	case 'c':
	    cleanup_datafiles = 1;
	    break;
	case 'i':
	    input_queuefile_maxsize = atoi(optarg);
	    break;
	case 'r':
	    enqueue_retries = atoi(optarg);
	    break;
	case 'd':
	    enqueue_retry_interval_secs = atoi(optarg);
	    break;
	case 'v':
	    oomgr_debug_output = 1;
	    break;
	case 'h':
	case '?':
	    print_usage(argv[0]) ;
	    return 1;
	    break;
	default:
	    abort ();
         }
    }
    return 0;
}

/*****************************************************************************
*	Function : daemonize()
*	Purpose : Create daemon process
*	Parameters : none
*	Return Value : void
*****************************************************************************/
void daemonize(void)
{
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }
    if (setsid() == -1) {
    	exit(0);
    }
    signal(SIGHUP, SIG_IGN);
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }

    /* Do not chdir when this processis managed by the PM
     * if (chdir("/") != 0) exit(0);
     */
}

/*****************************************************************************
*	Function : send_response_data()
*	Purpose : Send response data via output fqueue
*	Parameters : wget_log_file, content_filename, uri, uri_len
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
int send_response_data(const mime_header_t *httpresp,
		const char *content_filename, 
		const char *uri, int uri_len,
		const char *nstokendata, int nstokendata_len,
		const char *fq_destination, int fq_destination_len,
		const char *fq_cookie, int fq_cookie_len)
{
    int rv;
    char *uri_buf;
    char *canon_uri_buf;
    int canon_uri_bufsz;
    const char *uri_path;
    const char *p;
    int serialbufsz;
    char *serialbuf;
    int retries;
    int bytesused;

    // strip "http://host:port" from uri
    uri_buf = (char *)alloca(uri_len+1);
    memcpy((void *)uri_buf, uri, uri_len);
    uri_buf[uri_len] = '\0';
    uri_path = strstr(uri_buf, "http://");
    if (uri_path) {
        uri_path += 7;
	p = strchr(uri_path, '/');
	if (p) {
	    uri_path = p;
	}
    } else {
    	uri_path = uri_buf;
    }

    // Remove query string
    p = strchr(uri_path, '?');
    if (p) {
        *((char *)p) = '\0';
    }
    canon_uri_bufsz = strlen(uri_path)+1;
    canon_uri_buf = (char *)alloca(canon_uri_bufsz);
    rv = canonicalize_url(uri_path, canon_uri_bufsz, 
    			canon_uri_buf, canon_uri_bufsz, &bytesused);
    if (!rv) {
    	uri_path = canon_uri_buf;
    } else {
    	DBG("%s:%d canonicalize_url()failed rv=%d, using raw URL "
	    "url=%p url_len=%d output=%p outbuf_bufsz=%d bytesused=%d",
	    __FUNCTION__, __LINE__,
	    rv, uri_path, canon_uri_bufsz, canon_uri_buf, canon_uri_bufsz,
	    bytesused);
    }

    serialbufsz = serialize_datasize(httpresp);
    serialbuf = (char *)alloca(serialbufsz);
    rv = serialize(httpresp, serialbuf, serialbufsz);
    if (rv) {
    	DBG("%s:%d serialize() failed rv=%d",
	    __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = init_queue_element(&output_qe, 0, content_filename, uri_path);
    if (rv) {
    	DBG("%s:%d init_queue_element() failed rv=%d",
	    __FUNCTION__, __LINE__, rv);
	rv = 2;
	goto exit;
    }

    rv = add_attr_queue_element_len(&output_qe, "http_header", 11,
    				serialbuf, serialbufsz);
    if (rv) {
    	DBG("%s:%d add_attr_queue_element_len() failed rv=%d",
	    __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    if (nstokendata) {
    	rv = add_attr_queue_element_len(&output_qe, "namespace_token", 15,
    				nstokendata, nstokendata_len);
    }

    if (fq_destination) {
    	rv = add_attr_queue_element_len(&output_qe, "fq_destination", 14,
    				fq_destination, fq_destination_len);
    }

    if (fq_cookie) {
    	rv = add_attr_queue_element_len(&output_qe, "fq_cookie", 9,
    				fq_cookie, fq_cookie_len);
    }

    retries = enqueue_retries;
    while (retries--) {
    	rv = enqueue_fqueue_fh_el(enqueue_fh, &output_qe);
	if (!rv) {
	    break;
	} else if (rv == -1) {
	    if (retries) {
	    	sleep(enqueue_retry_interval_secs);
	    }
	} else {
    	    DBG("%s:%d enqueue_fqueue_fh_el() failed rv=%d",
	    	__FUNCTION__, __LINE__, rv);
	    rv  = 1;
	    break;
	}
    }

exit:
    return rv;
}

/*****************************************************************************
*	Function : add_wget_header_arg()
*	Purpose : Add --header arg to wget command buffer
*	Parameters : buf, byteused, name, namelen, data, datalen
*	Return Value :  void
*****************************************************************************/
void add_wget_header_arg(char *buf, int *bytesused, 
			const char *name, int name_len,
			const char *data, int data_len)
{
    // --header "<name>: <value>"<space>
    memcpy(&buf[*bytesused], "--header \"", 10);
    *bytesused += 10;
    memcpy(&buf[*bytesused], name, name_len);
    *bytesused += name_len;
    memcpy(&buf[*bytesused], ": ", 2);
    *bytesused += 2;
    memcpy(&buf[*bytesused], data, data_len);
    *bytesused += data_len;
    memcpy(&buf[*bytesused], "\" ", 2);
    *bytesused += 2;
}

/*****************************************************************************
*	Function : send_request()
*	Purpose : Extract request data from fqueue_element_t and send it.
*	Parameters : fqueue_element_t *
*	Return Value :  void
*****************************************************************************/
int send_request(const fqueue_element_t *qe_t)
{
    int rv;
    int n;
    int nth;

    const char *name;
    int name_len;
    const char *data;
    int data_len;

    const char *uri;
    int uri_len;

    const char *hdrdata;
    int hdrdata_len;
    mime_header_t httpreq;
    mime_header_t httpresp;
    char wgetcmdbuf[MAX_LINE_SIZE];
    int bufsize;
    int bytesused;
    int reqbytes = 0;
    const char *nstokendata;
    int nstokendata_len;
    const char *fq_destination;
    int fq_destination_len;
    const char *fq_cookie;
    int fq_cookie_len;

    char outputfilename[1024];
    u_int32_t attr;
    int hdrcnt;

    char num_buf[64];

    const cooked_data_types_t *ckdata;
    int ckdata_len;
    mime_hdr_datatype_t dtype;
    struct stat statdata;

    outputfilename[0] = '\0';
    rv = init_http_header(&httpreq, 0, 0);
    if (rv) {
    	DBG("%s:%d init_http_header() failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = init_http_header(&httpresp, 0, 0);
    if (rv) {
    	DBG("%s:%d init_http_header() failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
	rv = 2;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_t, T_ATTR_URI, 0, &name, &name_len,
    			&uri, &uri_len);
    if (rv) {
    	DBG("%s:%d get_attr_fqueue_element(T_ATTR_URI) failed, "
	    "rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "http_header", 11,
				&hdrdata, &hdrdata_len);
    if (!rv) {
	rv = deserialize(hdrdata, hdrdata_len, &httpreq, 0, 0);
	if (rv) {
	    DBG("%s:%d deserialize() failed, rv=%d",
		__FUNCTION__, __LINE__, rv);
	    rv = 4;
	    goto exit;
	}
    } else {
    	DBG("%s:%d get_nvattr_fqueue_element_by_name() "
	    "for \"http_header\" failed, rv=%d", 
	    __FUNCTION__, __LINE__, rv);
    }

    nstokendata = 0;
    rv = get_nvattr_fqueue_element_by_name(qe_t, "namespace_token", 15,
				&nstokendata, &nstokendata_len);
    fq_destination = 0;
    rv = get_nvattr_fqueue_element_by_name(qe_t, "fq_destination", 14,
				&fq_destination, &fq_destination_len);
    fq_cookie = 0;
    rv = get_nvattr_fqueue_element_by_name(qe_t, "fq_cookie", 9,
				&fq_cookie, &fq_cookie_len);

    strncpy(outputfilename, data_dirname, sizeof(data_dirname));
    strcat(outputfilename, WGET_DATA_FILE_TEMPLATE);
    snprintf(num_buf, sizeof(num_buf), "%d", data_filenumber++);
    strcat(outputfilename, num_buf);

    /* Build the wget command */
    bufsize = sizeof(wgetcmdbuf)-1;
    wgetcmdbuf[bufsize-1] = '\0';

    bytesused = snprintf(wgetcmdbuf, bufsize, "wget -t 1 -S -o %s -O %s ",
			WGET_LOG_FILE, outputfilename);

    /* Add known end to end headers */
    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
        if (!http_end2end_header[n]) {
	    continue;
	}
	rv = get_known_header(&httpreq, n, &data, &data_len, &attr, &hdrcnt);
	if (!rv) {

	    // --header "<name>: <value>"<space>
	    reqbytes = 10 + http_known_headers[n].namelen + 2 + data_len + 1;
	    if ((reqbytes + bytesused) <= bufsize) {
	    	add_wget_header_arg(wgetcmdbuf, &bytesused, 
				http_known_headers[n].name, 
				http_known_headers[n].namelen, 
				data, data_len);
	    } else {
    		DBG("%s:%d response buffer exceeded, "
		    "reqbytes=%d bytesused=%d bufsize=%d",
		    __FUNCTION__, __LINE__, reqbytes, bytesused, bufsize);
		rv = 5;
		goto exit;
	    }
	} else {
	    continue;
	}

	for (nth = 1; nth < hdrcnt; nth++) {
	    rv = get_nth_known_header(&httpreq, n, nth, &data, &data_len, 
	     			&attr);
	    if (!rv) {
		// --header "<name>: <value>"<space>
		reqbytes = 10 + http_known_headers[n].namelen + 2 + 
			data_len + 1;
		if ((reqbytes + bytesused) <= bufsize) {
		    add_wget_header_arg(wgetcmdbuf, &bytesused, 
				    http_known_headers[n].name, 
				    http_known_headers[n].namelen, 
				    data, data_len);
		} else {
		    DBG("%s:%d response buffer exceeded, "
			"reqbytes=%d bytesused=%d bufsize=%d",
			__FUNCTION__, __LINE__, reqbytes, bytesused, bufsize);
		    rv = 6;
		    goto exit;
		}
	    } else {
	        DBG("%s:%d get_nth_known_header(n=%d nth=%d hdrcnt=%d)",
		    __FUNCTION__, __LINE__, n, nth, hdrcnt);
		rv = 7;
		goto exit;
	    }
	}
    }

    /* Add unknown headers */
    n = 0;
    while ((rv = get_nth_unknown_header(&httpreq, n, &name, &name_len,
    				&data, &data_len, &attr)) == 0) {
	// --header "<name>: <value>"<space>
	reqbytes = 10 + name_len + 2 + data_len + 1;
	if ((reqbytes + bytesused) <= bufsize) {
	    add_wget_header_arg(wgetcmdbuf, &bytesused, 
			    name, name_len, data, data_len);
	} else {
	    DBG("%s:%d response buffer exceeded, "
		"reqbytes=%d bytesused=%d bufsize=%d",
		__FUNCTION__, __LINE__, reqbytes, bytesused, bufsize);
	    rv = 8;
	    goto exit;
	}
    	n++;
    }

    // Add "http://host:port/uri"
    if ((reqbytes + 1 + uri_len + 1) <= bufsize) {
	wgetcmdbuf[bytesused] = '"';
	bytesused++;
	memcpy(&wgetcmdbuf[bytesused], uri, uri_len);
	bytesused += uri_len;
	wgetcmdbuf[bytesused] = '"';
	bytesused++;
    }
    wgetcmdbuf[bytesused] = '\0';


    /* wget the URL and check the return status */
    unlink(WGET_LOG_FILE);
    unlink(outputfilename);

    system(wgetcmdbuf);

    /* Now check the log for 200 */
    if (wget_200_ok(&httpresp)) {
	rv = get_cooked_known_header_data(&httpresp, MIME_HDR_CONTENT_LENGTH,
				&ckdata, &ckdata_len, &dtype);
	if (rv || (dtype != DT_SIZE)) {
	    DBG("%s:%d wget failed [%s]\n", __FUNCTION__, __LINE__, wgetcmdbuf);
	    DBG("%s:%d get_cooked_known_header_data(MIME_HDR_CONTENT_LENGTH) "
	    	"failed=%d dtype=%d", __FUNCTION__, __LINE__, rv, dtype);
	    rv = 9;
	    goto exit;
	}

	rv = lstat(outputfilename, &statdata);
	if (rv) {
	    DBG("%s:%d wget failed [%s]\n", __FUNCTION__, __LINE__, wgetcmdbuf);
	    DBG("%s:%d lstat(%s) failed, errno=%d",
	    	__FUNCTION__, __LINE__, outputfilename, errno);
	    rv = 10;
	    goto exit;
	}

	if ((off_t)ckdata->u.dt_size.ll != statdata.st_size) {
	    DBG("%s:%d wget failed [%s]\n", __FUNCTION__, __LINE__, wgetcmdbuf);
	    DBG("%s:%d Content-Length(%ld) != filesize(%ld)",
	    	__FUNCTION__, __LINE__, ckdata->u.dt_size.ll, statdata.st_size);
	    rv = 11;
	    goto exit;
	}

    	DBG("%s:%d wget success  [%s]\n", __FUNCTION__, __LINE__, wgetcmdbuf);
	rv = send_response_data(&httpresp, outputfilename, uri, uri_len,
				nstokendata, nstokendata_len,
				fq_destination, fq_destination_len,
				fq_cookie, fq_cookie_len);
	if (rv) {
	    DBG("%s:%d send_response_data() failed=%d",
	     	__FUNCTION__, __LINE__, rv);
	    rv = 12;
	    goto exit;
	}
    } else {
	DBG("%s:%d wget failed [%s]\n", __FUNCTION__, __LINE__, wgetcmdbuf);
	rv = 13;
	goto exit;
    }

exit:

    shutdown_http_header(&httpreq);
    shutdown_http_header(&httpresp);
    if (rv) {
    	unlink(outputfilename);
    }
    return rv;
}

/*****************************************************************************
*	Function : request_handler()
*	Purpose : Process fqueue_element_t requests
*	Parameters : none
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
void request_handler(void)
{
    int rv;
    fhandle_t fh;
    struct pollfd pfd;

    // Open output fqueue
    do {
    	enqueue_fh = open_queue_fqueue(output_queue_filename);
	if (enqueue_fh < 0) {
	    fprintf(stderr, "%s:%d Unable to open output queuefile [%s]\n",
	    	__FUNCTION__, __LINE__, output_queue_filename);
	    sleep(10);
	}
    } while (enqueue_fh < 0);

    // Create input fqueue
    do {
    	rv = create_fqueue(input_queue_filename, input_queuefile_maxsize);
	if (rv) {
	    fprintf(stderr, "%s:%d Unable to create input "
	    	    "queuefile [%s], rv=%d\n",
	    	    __FUNCTION__, __LINE__, input_queue_filename, rv);
	    sleep(10);
	}
    } while (rv);

    // Open input fqueue
    do {
    	fh = open_queue_fqueue(input_queue_filename);
	if (fh < 0) {
	    fprintf(stderr, "%s:%d Unable to open input queuefile [%s]\n",
	    	__FUNCTION__, __LINE__, input_queue_filename);
	    sleep(10);
	}
    } while (fh < 0);

    while (1) {
    	rv = dequeue_fqueue_fh_el(fh, &qe, 1);
	if (rv == -1) {
	    continue;
	} else if (rv) {
	    fprintf(stderr, "%s:%d dequeue_fqueue_fh_el(%s) failed, rv=%d\n",
	    	__FUNCTION__, __LINE__, input_queue_filename, rv);
	    poll(&pfd, 0, 100);
	    continue;
	}
	rv = send_request(&qe);
	if (rv) {
	    fprintf(stderr, "%s:%d send_request() failed, rv=%d\n",
	    	__FUNCTION__, __LINE__, rv);
	}
    }
}

/*****************************************************************************
*	Function : main()
*	Purpose : main logic 
*	Parameters : argc, argv 
*	Return Value : int
*****************************************************************************/
int main (int argc, char *argv[])
{
    uuid_t uid;
    char uuid_str[80];

    if (getoptions(argc, argv)) {
    	exit(4);
    }
    mime_hdr_startup(); // Init mime_hdr structures (one time process init)

/* Commenting out this check as this process now runs of PM and hence 
 * this is handled internal to PM.  - Ram (06/08/2009) */
#if 0
    if (pid_file_exist()) {
    	fprintf(stderr, "error : [%s] already running ...\n",
		argv[0]);
	exit(5);
    }
#endif /* 0 */

    /* Install signal handler */
    if (signal(SIGINT, termination_handler) == SIG_IGN)
    	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, termination_handler) == SIG_IGN)
    	signal(SIGHUP, SIG_IGN);
    if (signal(SIGTERM, termination_handler) == SIG_IGN)
    	signal(SIGTERM, SIG_IGN);
    if (signal(SIGQUIT, termination_handler) == SIG_IGN)
    	signal(SIGQUIT, SIG_IGN);
    signal(SIGUSR1, termination_handler);

    if (!do_not_create_daemon) {
    	daemonize();
    }

    /* Set the atexit function */
    if (atexit(exit_originmgr) != 0) {
    	fprintf(stderr, "error : failed to set exit routine, "
	     	"hence exiting... \n");
    	exit(5);
    }

    /* Create the pid file */
    create_pid_file(); 

    if (cleanup_datafiles) {
    	remove_datafiles();
    }

    /* Generate data directory name */
    strcpy(data_dirname, WGET_DATA_DIR_TEMPLATE);
    uuid_generate(uid);
    uuid_unparse(uid, uuid_str);
    strcat(data_dirname, uuid_str);
    strcat(data_dirname, "/");

    /* Save new data directory name */
    create_data_info_file(data_dirname);

    /* "mkdir <dirname>" */
    if (mkdir(data_dirname, 0666)) {
    	perror("mkdir failed");
	exit(6);
    }

    request_handler();

    exit(0);
}

/*
 * End of nkn_oomgr.c
 */
extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
		       uint32_t size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		uint32_t size __attribute((unused)))
{
	return 0;
}

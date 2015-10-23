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
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glib.h>

#include "nkn_defs.h"
#include "errorlog_pub.h"

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"


#define MAX_BUF_SIZE	8192

static const char *errorlog_program_name = "errorlog";

int socketfd = -1;
char totbuf[MAX_BUF_SIZE];
uint32_t el_serverip;
int el_serverport;
FILE * elfd = NULL;
const char * el_filename="error.log";
const char * el_server="127.0.0.1:7957";
uint32_t max_filesize=100*MBYTES;
uint32_t cur_filesize=0;

/* 
 * This global variable is set (1) to indicate that we want TallMaple - PM
 * to control us. This is reset (0) to indicate that the user wants to 
 * control the option. 
 *
 * If user invokes us with -f option, then it assumed tha the config options
 * are being given from the command line, in which case we run outside of
 * the process manager.
 */
int runas_gcl_client = 0;


static const char el_log_path[] = "/var/log/nkn";

static const char el_cfg_prefix[] = "/nkn/errorlog/config";
static const char el_serverip_binding[] = "/nkn/errorlog/config/serverip";
static const char el_serverport_binding[] = "/nkn/errorlog/config/serverport";
static const char el_filename_binding[] = "/nkn/errorlog/config/filename";
static const char el_max_filesize_binding[] = "/nkn/errorlog/config/max-filesize";

md_client_context *el_mcc = NULL;
lew_context *el_lew = NULL;
tbool el_exiting = false;
static GThread	*mgmt_thread;

/* List of signals server can handle */
static const int el_signals_handled[] = {
            SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};

#define el_num_signals_handled (int)(sizeof(el_signals_handled) / sizeof(int))

/* Libevent handles for server */
static lew_event *el_event_signals[el_num_signals_handled];


////////////////////////////////////////////////////////////
// Network functions
////////////////////////////////////////////////////////////

static int el_init_socket(void);
static int el_handshake(void);
static void el_close_socket(void);
static void el_open_file(void);
static void el_close_file(void);
static void el_parse_server(void);
static void usage(void);
static void version(void);
static void catcher(int sig);

int 
el_log_basic(const char *fmt, ...)
    __attribute__ ((__format__ (printf, 1, 2)));


int el_mgmt_handle_event_request(
        gcl_session *session,
        bn_request  **inout_request,
        void        *arg);
int el_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings);
int
el_mgmt_initiate_exit(void);

int el_mgmt_handle_session_close(
        int fd,
        fdic_callback_type cbt,
        void *vsess,
        void *gsec_arg);

int el_mgmt_init(void);


int el_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
        void *data);

int el_gcl_init(void);


int el_cfg_query(void);

int el_mgmt_thread_init(void);

static int el_handle_signal(
        int signum,
        short ev_type,
        void *data,
        lew_context *ctxt);

static void
el_mgmt_thread_hdl(void);
/*--------------------------------------------------------------------------*/

static int el_init_socket(void)
{
        struct sockaddr_in srv;
        int ret;

        // Create a socket for making a connection.
        // here we implemneted non-blocking connection
        if( (socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                el_log_basic("%s: sockfd failed", __FUNCTION__);
                exit(1);
        }

        // Now time to make a socket connection.
        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
        srv.sin_addr.s_addr = el_serverip;
        srv.sin_port = htons(el_serverport);

        ret = connect (socketfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
        if(ret < 0)
        {
		el_log_basic("Failed to connect to server %s\n", el_server);
		return -1;
        }
	el_log_basic("Server has been successfully connected\n");

	return 1;
}

static int el_handshake(void)
{
	struct errorlog_req * req;
	struct errorlog_res * res;
	int ret;

	/*
	 * Send request to nvsd server.
	 */
	req = (struct errorlog_req *)totbuf;
	strcpy(req->magic, EL_MAGIC);
	if(send(socketfd, totbuf, sizeof(struct errorlog_req), 0) <=0 ) {
		return -1;
	}
	el_log_basic("Sends out errorlog request to nvsd ...\n");

	/*
	 * Read response from nvsd server.
	 */
	ret = recv(socketfd, totbuf, sizeof(struct errorlog_res), 0);
	if(ret<(int)errorlog_res_s) {
		perror("Socket recv.");
		return -1;
	}
	res = (struct errorlog_res *)totbuf;
	if(res->status != ELR_STATUS_SUCCESS) {
		el_log_basic("server returns error %d\n", res->status);
		exit(4);
	}
	el_log_basic("Errorlog request is permitted. Starting receiving error message.\n");
	return 1;
}


static void el_close_socket(void)
{
	if(socketfd != -1) {
		shutdown(socketfd, 0);
		close(socketfd);
		socketfd = -1;
	}
}


static void el_launch_socket_loop(void)
{
	int ret;

	while(1) {
		if(el_init_socket()==-1) goto again;
		if(el_handshake()==-1) goto again;

		while(1)
		{
			ret = recv(socketfd, totbuf, MAX_BUF_SIZE, 0);
			if(ret<=0) {
				//perror("Socket recv.");
				goto again;
			}
			if(max_filesize && (ret>(int)(max_filesize-cur_filesize)) ) {
				fseek(elfd, 0L, SEEK_SET);
				cur_filesize=0;
			}
			fwrite(totbuf, ret, 1, elfd);
			fflush(elfd);
			cur_filesize += ret;
		}
again:
		el_close_socket();
		el_log_basic("server is not up, try again after 10 sec...\n");
		sleep(10);
	}
}

////////////////////////////////////////////////////////////
// File operation functions
////////////////////////////////////////////////////////////

static void el_open_file(void)
{
	char abs_filename[256];
	char new_filename[256];
	struct stat st;

	snprintf(abs_filename, 256, "%s/%s", el_log_path, el_filename);
	snprintf(new_filename, 256, "%s/%s.old", el_log_path, el_filename);
	if (stat(abs_filename, &st) == 0) {
		/* File exists, just back it up */
		rename(abs_filename, new_filename);
	}
	elfd = fopen(abs_filename, "w");
        if(elfd == NULL) {
		el_log_basic("Failed to open errorlog file %s (%s)\n", abs_filename, strerror(errno));
		exit(1);
	}

}

static void el_close_file(void)
{
	if(elfd) {
		fclose(elfd);
		elfd=NULL;
	}
}

////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

static void el_parse_server(void)
{
	char * p;
	char * server = strdup(el_server);

	p=strchr(server, ':');
	if(!p) {
		el_serverport=7957;
	}
	else {
		*p=0;
		el_serverport=atoi(p+1);
	}
	el_serverip=inet_addr(server);
	free(server);
}

static void usage(void)
{
        printf("\n");
        printf("nknerrorlog - Nokeena Error Log Tool\n");
        printf("Usage: \n");
        printf("-s <ip:port>  : nvsd server ip and port (default: 127.0.0.1:7957)\n");
        printf("-f <filename> : error log filename, default: error.log. "
                "(Path is fixed at /var/log/nkn)\n");
        printf("-g            : Load all auguments from TM nodes\n");
        printf("-d            : Run as Daemon\n");
        printf("-m            : max file size in MBytes, default: 0 (unlimited)\n");
        printf("-v            : show version\n");
        printf("-h            : show this help\n");
        printf("\n");
        exit(1);
}

static void version(void)
{
        printf("\n");
        printf("nknerrorlog - Nokeena Error Log Application\n");
        printf("Build Date: "__DATE__ " " __TIME__ "\n");
        printf("\n");
        exit(1);

}

static void daemonize(void)
{

        if (0 != fork()) exit(0);

        if (-1 == setsid()) exit(0);

        signal(SIGHUP, SIG_IGN);

        if (0 != fork()) exit(0);

        /* Do not chdir when this processis managed by the PM
         * if (0 != chdir("/")) exit(0);
         */
}

static void catcher(int sig)
{
    if (runas_gcl_client) {
        switch(sig) {
        case SIGHUP:
        case SIGINT:
        case SIGTERM:
        case SIGPIPE:
                el_close_socket();
		el_close_file();
                exit(1);
                break;
        //case SIGPIPE:
                break;
        case SIGUSR1:
                break;
        default:
                break;
        }
    }
    else {
        el_close_socket();
	el_close_file();
        exit(1);
    }
}

////////////////////////////////////////////////////////////////////////
// main Functions
////////////////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
    int runas_daemon=0;
    int ret;

    while ((ret = getopt(argc, argv, "s:f:m:dhvg")) != -1) {
        switch (ret) {
        case 's':
             el_server=optarg;
             break;
        case 'f':
             el_filename=optarg;
             break;
        case 'd':
             runas_daemon=1;
             break;
        case 'm':
             max_filesize=atoi(optarg) * MBYTES;
             break;
        case 'h':
             usage();
             break;
        case 'v':
             version();
             break;
        case 'g':
             runas_gcl_client = 1;
             break;
        }
    }

    if (!runas_gcl_client) {

        if(runas_daemon) daemonize();

        signal(SIGHUP,catcher);
        signal(SIGKILL,catcher);
        signal(SIGTERM,catcher);
        signal(SIGINT,catcher);
        signal(SIGPIPE,SIG_IGN);

        /*
         * parse the server string to get ip/port of nvsd.
         */
        el_parse_server();
    }
    else {

        ret = el_gcl_init();
        bail_error(ret);

        ret = el_mgmt_init();
        bail_error(ret);

        ret = el_mgmt_thread_init();
        bail_error(ret);

        ret = el_cfg_query();
        bail_error(ret);
    }

    //// Common code - for both GCL and non-GCL versions ////
    
    el_open_file();

    // Launch the network socket.
    // This function should never return.
    el_launch_socket_loop();

bail:
    return 1;
}


/*--------------------------------------------------------------------------*/
int el_gcl_init(void)
{
    int err = 0;
    int i = 0;


    err = lc_log_init(errorlog_program_name, NULL, 
            LCO_none, LC_COMPONENT_ID(LCI_NKNERRORLOG),
            LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
    bail_error(err);
    
    err = lew_init(&el_lew);
    bail_error(err);

    /*
     * Register to hear about all the signals that we care about.
     */
    for(i = 0; i < (int) el_num_signals_handled; ++i) {
        err = lew_event_reg_signal(el_lew, 
                &(el_event_signals[i]), el_signals_handled[i], 
                el_handle_signal, NULL);
        bail_error(err);
    }


bail:
    return err;
}

/*--------------------------------------------------------------------------*/
int el_cfg_query(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    lc_log_basic(LOG_NOTICE, "errorlogger initializing..");
    err = mdc_get_binding_children(el_mcc,
            NULL,
            NULL,
            true,
            &bindings,
            false,
            true,
            el_cfg_prefix);
    bail_error_null(err, bindings);
    

    err = bn_binding_array_foreach(bindings, el_cfg_handle_change, NULL);
    bail_error(err);

bail:
    bn_binding_array_free(&bindings);
    return err;
}


/*--------------------------------------------------------------------------*/
int el_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
        void *data)
{
    int err = 0;
    const tstring *name = NULL;
    uint16 serv_port = 0;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- SERVER IP - This is always @ 127.0.0.1 ------------*/
    if (ts_equal_str(name, el_serverip_binding, false)) {
        uint32 ipval = 0;
        err = bn_binding_get_ipv4addr(binding, ba_value, NULL, &ipval);
        bail_error(err);
        el_serverip = htonl(ipval);
    }
    /*-------- SERVER PORT  ------------*/
    else if (ts_equal_str(name, el_serverport_binding, false)) {
        err = bn_binding_get_uint16(binding, ba_value, NULL, &serv_port);
        bail_error(err);
        if (serv_port != el_serverport) {
            el_serverport = serv_port;
        }

    } 
    /*-------- FILENAME ------------*/
    else if (ts_equal_str(name, el_filename_binding, false)) {
        tstring *str = NULL;
        err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &str);
        bail_error(err);

        el_filename = strdup(ts_str(str));
        ts_free(&str);
    } 
    /*-------- MAX FILESIZE ------------*/
    else if (ts_equal_str(name, el_max_filesize_binding, false)) {
        uint16 size = 0;
        err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
        bail_error(err);
        max_filesize = size * MBYTES;
    }

bail:
    return err;
}

static void
el_mgmt_thread_hdl(void)
{
	int err = 0;

	/* Call the main loop of samara */
        lc_log_debug(LOG_DEBUG,
                     _("el_mgmt_thread_hdl () - before lew_dispatch"));
	lew_dispatch (el_lew, NULL, NULL) ;

} /* end of nvsd_mgmt_thread_hdl() */


int el_mgmt_thread_init(void)
{
    int err = 0;
    GError  *t_err = NULL;


    g_thread_init(NULL);

    /* As a test create a thread here */
    mgmt_thread = g_thread_create((GThreadFunc)el_mgmt_thread_hdl,
                        NULL, TRUE, &t_err);
    if (NULL == mgmt_thread){
        lc_log_basic(LOG_ERR, "Mgmt Thread Creation Failed !!!!");
        g_error_free (t_err);
    }
bail:
    return err;
}

/*--------------------------------------------------------------------------*/
int el_mgmt_init(void)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = el_lew;
    mwp->mwp_gcl_client  = gcl_client_nknerrorlog;

    mwp->mwp_session_close_func = el_mgmt_handle_session_close;
    mwp->mwp_event_request_func = el_mgmt_handle_event_request;

    err = mdc_wrapper_init(mwp, &el_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking();
    bail_error(err);

    /*
     * Register to receive config changes.
     */
    err = mdc_send_action_with_bindings_str_va(
            el_mcc,
            NULL,
            NULL,
            "/mgmtd/events/actions/interest/add",
            2,
            "event_name", bt_name, mdc_event_dbchange,
            "match/1/pattern", bt_string, "/nkn/errorlog/config/**");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va(
            el_mcc,
            NULL,
            NULL,
            "/mgmtd/events/actions/interest/add",
            1,
            "event_name", bt_name, "/mgmtd/notify/dbchange");
    bail_error(err);
        
bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
        lc_log_debug(LOG_ERR, _("unable to connect to management daemon."));
    }
    return err;
}



int el_mgmt_handle_session_close(
        int fd,
        fdic_callback_type cbt,
        void *vsess,
        void *gsec_arg)
{
        int err = 0;
        gcl_session *gsec_session = vsess;

        if (el_exiting) {
                goto bail;
        }

        lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
        err = el_mgmt_initiate_exit();
        bail_error(err);

bail:
        return err;
}

int
el_mgmt_initiate_exit(void)
{
        int err = 0;
        
        el_exiting = true;

        err = mdc_wrapper_disconnect(el_mcc, false);
        bail_error(err);

        err = lew_escape_from_dispatch(el_lew, true);
        bail_error(err);

bail:
        return err;
}


int el_mgmt_handle_event_request(
        gcl_session *session,
        bn_request  **inout_request,
        void        *arg)
{
        int err = 0;
        bn_msg_type msg_type = bbmt_none;
        bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
        tstring *event_name = NULL;
        bn_binding_array *bindings = NULL;
        uint32 num_callbacks = 0, i = 0;
        void *data = NULL;

        bail_null(inout_request);
        bail_null(*inout_request);

        err = bn_request_get(*inout_request, &msg_type, 
                        NULL, false, &bindings, NULL, NULL);
        bail_error(err);

        bail_require(msg_type == bbmt_event_request);

        err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
        bail_error_null(err, event_name);

        lc_log_debug(LOG_DEBUG, "Received event: %s", ts_str(event_name));

        if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
                err = mdc_event_config_change_notif_extract(
                                bindings, 
                                &old_bindings,
                                &new_bindings,
                                NULL);
                bail_error(err);

                err = el_config_handle_set_request(old_bindings, new_bindings);
                bail_error(err);
        } else if (ts_equal_str(event_name, "/mgmtd/notify/dbchange", false)) {
                lc_log_debug(LOG_WARNING, I_("Received unexpected event %s"), 
                                ts_str(event_name));
                
        }
        else {
                lc_log_debug(LOG_WARNING, I_("Received unexpected event %s"), 
                                ts_str(event_name));
        }

bail:
        bn_binding_array_free(&bindings);
        bn_binding_array_free(&old_bindings);
        bn_binding_array_free(&new_bindings);
        ts_free(&event_name);

        return err;
}

int el_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;

        lc_log_basic(LOG_NOTICE, "SET REQUEST.");

bail:
        return err;
}

static int el_handle_signal(
        int signum,
        short ev_type,
        void *data,
        lew_context *ctxt)
{
        (void) ev_type;
        (void) data;
        (void) ctxt;

        catcher(signum);
        return 0;
}

int 
el_log_basic(const char *fmt, ...)
{
    int n = 0;
    char str[1024];
    va_list ap;

    va_start(ap, fmt);

    if(runas_gcl_client) {
        n = vsprintf(str, fmt, ap);
        lc_log_basic(LOG_NOTICE, "%s", str);
    }
    else {
        n = vprintf(fmt, ap);
    }

    va_end(ap);

    return n;
}

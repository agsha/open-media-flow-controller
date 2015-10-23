/*
 *
 * Filename:  gmgmthd_main.c
 * Date:      2010/03/25 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <time.h>
#include <sys/time.h>
#include "nkn_defs.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "gmgmthd_main.h"
#include "libnknmgmt.h"
#include "file_utils.h"
/** Local Macros
 */
#define	CMM_NODE_STATUS_HTML_FILE	"/vtmp/www/cmm-node-status.html"

/* ------------------------------------------------------------------------- */
/** Globals and Locals
 */
lew_event *cmm_timer_event = NULL;
lew_context *gmgmthd_lwc = NULL;
lew_context *gmgmthd_timer_lwc = NULL;
tstring *gmgmthd_prefix_str = NULL;
tbool gmgmthd_exiting = false;
static pthread_t gmgmthd_timerid;
int nkn_timer_interval = 3; // in (1/nkn_timer_interval) second

static const char *gmgmthd_program_name = "gmgmthd";
extern unsigned int jnpr_log_level;
extern int ipmi_thread_init(void);

/* ------------------------------------------------------------------------- */
/** Local prototypes & definitions
 */
int
cmm_liveness_timeout_handler(int fd,
		short event_type,
		void *data,
		lew_context
		*ctxt);

static int
gmgmthd_cmm_liveness_timer_register(void);

static int gmgmthd_init(void);
static int gmgmthd_main_loop(void);
static int gmgmthd_deinit(void);
static int gmgmthd_handle_signal(int signum, short ev_type, void *data,
                            lew_context *ctxt);
static int cmm_status_check_func(void);

static int gmgmthd_restart_proc(void);


/* List of signals server can handle */
static const int gmgmthd_signals_handled[] = {
    SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1, SIGCHLD
};

#define gmgmthd_num_signals_handled sizeof(gmgmthd_signals_handled) / sizeof(int)

/* Libevent handles for server */
static lew_event *gmgmthd_event_signals[gmgmthd_num_signals_handled];

/* ------------------------------------------------------------------------- */
static inline int32_t
tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec) * 1e6
          + (tv2->tv_usec - tv1->tv_usec));
}

/* ------------------------------------------------------------------------- */
tbool is_pacifica = false;
int
main(int argc, char **argv)
{
	int err = 0;

	err = lc_init();
	bail_error(err);
    

	err = gmgmthd_init();
	bail_error(err);


	/* Init the context inside the thread */
	//err = mfp_init();
	//bail_error(err);

	/* start the mfp thread and create the context inside the new threads */
	err = mfp_mgmt_thread_init();
	bail_error(err);

        err = ipmi_thread_init();
        bail_error(err);

	err = gmgmthd_main_loop();
	complain_error(err);

	err = gmgmthd_deinit();
	complain_error(err);

	/* De-init the mfp gcl context inside the mfp thread */
	//err = mfp_deinit();
	//complain_error(err);


bail:
    lc_log_basic(LOG_NOTICE, _("MFD monitor daemon exiting with code %d"), err);
    lc_log_close();

    exit(err);
}

/* ------------------------------------------------------------------------- */
int
gmgmthd_initiate_exit(void)
{
    int err = 0;
    uint32 num_recs = 0, i = 0;

    /*
     * Some occurrences are only OK if they happen while we're
     * exiting, such as getting NULL back for a mgmt request.
     */
    gmgmthd_exiting = true;

    /*
     * Now make ourselves fall out of the main loop by removing all
     * active events.
     */
    for (i = 0; i < gmgmthd_num_signals_handled; ++i) {
        err = lew_event_delete(gmgmthd_lwc, &(gmgmthd_event_signals[i]));
        bail_error(err);
    }

    /*
     * Need to do this here to remove our mgmt-related events.
     */
    err = mdc_wrapper_disconnect(gmgmthd_mcc, false);
    bail_error(err);

    /*
     * Calling lew_delete_all_events() is not guaranteed
     * to solve this for us, since the GCL/libevent shim does not use
     * the libevent wrapper.  lew_escape_from_dispatch() will
     * definitely do it, though.
     */
    err = lew_escape_from_dispatch(gmgmthd_lwc, true);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
gmgmthd_init(void)
{
    int err = 0;
    uint32 i = 0;

    /* Don't mess with the modes we choose */
    umask(0);

    /*
     * The LOCAL4 facility is coordinated with an additional facility
     * we add in our graft point in src/include/logging.inc.h
     */
    err = lc_log_init(gmgmthd_program_name, NULL, LCO_none, 
                      LC_COMPONENT_ID(LCI_GMGMTHD),
                      LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
    bail_error(err);

#if defined(PROD_FEATURE_I18N_SUPPORT)
    err = lc_set_locale_from_config(true);
    complain_error(err);
    err = 0;
#endif
    /*Attach to LFD deamon*/
    lc_log_basic(LOG_NOTICE, _("calling shared memory\n"));
    err = lfd_connect();
    bail_error(err);

    lc_log_basic(LOG_NOTICE, _("MFD monitor daemon launched"));


    is_pacifica = is_pacifica_blade();
    lc_log_basic(LOG_INFO, "is_pacifica = %d" , is_pacifica);
    /* Initialize libevent wrapper */
    err = lew_init(&gmgmthd_lwc);
    bail_error_null(err, gmgmthd_lwc);

  //  err = lew_init(&gmgmthd_timer_lwc);
  //  bail_error_null(err, gmgmthd_timer_lwc);

    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */
    for (i = 0; i < gmgmthd_num_signals_handled; ++i) {
        err = lew_event_reg_signal
            (gmgmthd_lwc, &(gmgmthd_event_signals[i]), gmgmthd_signals_handled[i],
             gmgmthd_handle_signal, NULL);
        bail_error(err);
    }

    err = gmgmthd_mgmt_init();
    bail_error(err);

    /*
     * Read in the initial configuration from mgmtd
     */
    err = gmgmthd_config_query();
    bail_error(err);

    /*
     * Restart httpd and sshd to support dynamic interfaces
     */
    err = gmgmthd_restart_proc();
    bail_error(err);

    /* Invoke the timer thread */
    gmgmthd_cmm_liveness_timer_register();

    /*Read the disk cache data*/
    err = gmgmthd_disk_query();
    bail_error(err);



 bail:
    return(err);
}
/* ------------------------------------------------------------------------- */
static int
gmgmthd_restart_proc(void)
{
    int i = 0;
    int err = 0;
    tbool file_exists = false;
    const char *name[] = {"httpd", "sshd", NULL};

    if(!is_pacifica)
	return err;

    err = lf_test_exists("/vtmp/first_boot", &file_exists);
    bail_error(err);

    if(!file_exists) {

	/* Restart procs */
	while(name[i] != NULL) {
	    err = mdc_send_action_with_bindings_str_va(gmgmthd_mcc,
		    NULL, NULL,
		    "/pm/actions/restart_process", 1,
		    "process_name", bt_string, name[i]);
	    bail_error(err);

	    i++;
	}

	/* Touch the file */
	err = lf_touch_command("/vtmp/first_boot");
	bail_error(err);

    }
bail:
    return err;
}

/* ------------------------------------------------------------------------- */
static int
gmgmthd_main_loop(void)
{
    int err = 0;

    err = lew_dispatch(gmgmthd_lwc, NULL, NULL);
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
gmgmthd_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&gmgmthd_mcc);
    bail_error(err);

    err = lew_deinit(&gmgmthd_lwc);
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
gmgmthd_handle_signal(int signum, short ev_type, void *data, lew_context *ctxt)
{
    int err = 0;

    lc_log_basic(jnpr_log_level, "Received %s", lc_signal_num_to_name(signum));

    switch (signum) {

    case SIGHUP:
    case SIGINT:
    case SIGTERM:
        err = gmgmthd_initiate_exit();
        bail_error(err);

    case SIGPIPE:
        /* Currently ignored */
        break;

    case SIGUSR1:
        /* Currently ignored */
        break;

    case SIGCHLD:
	while (waitpid (-1, NULL, WNOHANG) > 0);
	break;

    default:
        lc_log_basic(LOG_WARNING, I_("Got unexpected signal %s"),
                     lc_signal_num_to_name(signum));
        break;
    }

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
gmgmthd_cmm_liveness_timer_register(void) {
	int err = 0;

	err = lew_event_reg_timer(gmgmthd_lwc,
			&cmm_timer_event,
			cmm_liveness_timeout_handler,
			(void *) NULL,
			2000);
	bail_error(err);

bail:
	return err;
}

int
cmm_liveness_timeout_handler(int fd,
		short event_type,
		void *data,
		lew_context
		*ctxt)
{
	cmm_status_check_func();
	gmgmthd_cmm_liveness_timer_register();

	return 0;
}

/* ------------------------------------------------------------------------- *
 * Purpose : This function is called from the timer and it checks to see
 * the used bandwidth every second and also the status of nvsd and 
 * creates the status HTML file for CMM to fetch from other nodes
 * ------------------------------------------------------------------------- */
static int
cmm_status_check_func (void)
{
	int err = 0;
	int fd_status = -1, nvsd_status;
	char	write_buf [512];
	tbool t_nvsd_dead = false;
	tstring *ts_tx_bw_used_bps= NULL;
	uint64_t tx_bw_used_bps, tx_bw_total_bps;
	int bw_percentage;


	/* Get the nvsd status from the monitoring logic */
	t_nvsd_dead = CMM_liveness_check() ;

	/* Get the total bandwidth stat node (kbps) */
	err =  get_tx_bw_total_bps (&tx_bw_total_bps, gmgmthd_mcc,
			SERVICE_INTF_ALL);
	if (err != 0) // In case the function failed
		tx_bw_total_bps = 0;

	/* Get the bandwidth used stat node (bps) */
	err = mdc_get_binding_tstr(gmgmthd_mcc, NULL, NULL, NULL,
				&ts_tx_bw_used_bps, 
				"/stats/state/chd/current_bw_serv_rate_tx/node/\\/stat\\/nkn\\/interface\\/bw\\/service_intf_bw_tx/last/value",
				NULL);
	if (err != 0) // In case the function failed
		ts_tx_bw_used_bps = NULL;

	/* calculate the bw % only if used and total are not zero */
	if (ts_tx_bw_used_bps && tx_bw_total_bps) {
		/* Convert the string to integers */
		tx_bw_used_bps = atoi(ts_str(ts_tx_bw_used_bps));

		bw_percentage = (tx_bw_used_bps * 100) /tx_bw_total_bps;
	}
	else
		bw_percentage = 0;

	/* Now get the nvsd status */
	if (t_nvsd_dead)
		nvsd_status = 0;
	else 
		nvsd_status = 1;

	/* Write to the status html file */
	fd_status = open (CMM_NODE_STATUS_HTML_FILE, O_WRONLY | O_TRUNC);
	if (-1 != fd_status) {
		snprintf (write_buf, 511, "\nvers=%d,bw=%.3d,nvsd=%d;\n",
					CMM_NODE_STATUS_HTML_VERSION,
					bw_percentage, nvsd_status);
		write (fd_status, write_buf, strlen(write_buf));
		close (fd_status);
	}

bail:
	ts_free(&ts_tx_bw_used_bps);
	return err;
}

/* End of gmgmthd_main.c */

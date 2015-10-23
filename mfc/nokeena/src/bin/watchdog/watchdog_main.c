/*
 *
 * Filename:  watchdog_main.c
 * Date:      2010/03/25 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "mdc_wrapper.h"
#include "watchdog_main.h"
#include "watchdog.h"

/* ------------------------------------------------------------------------- */
/** Globals
 */
lew_context *watchdog_lwc = NULL;
tstring *watchdog_prefix_str = NULL;
tbool watchdog_exiting = false;
watchdog_config_t watchdog_config;
extern int
watchdog_watchdog_gdb_file_init(void);

/* ------------------------------------------------------------------------- */
/** Local prototypes & definitions
 */
static int watchdog_init(void);
static int watchdog_main_loop(void);
static int watchdog_deinit(void);
static int watchdog_handle_signal(int signum, short ev_type, void *data,
                            lew_context *ctxt);
extern int watchdog_toggle_poll_timer(tbool set);
extern int
watchdog_watchdog_tmr_hdlr(int fd, short event_type,
		void *data, lew_context *ctxt);
static int
watchdog_watchdog_init(void);

static int
watchdog_watchdog_deinit(void);

static const char *watchdog_program_name = "watchdog";

/* List of signals server can handle */
static const int watchdog_signals_handled[] = {
    SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};

#define watchdog_num_signals_handled sizeof(watchdog_signals_handled) / sizeof(int)

/* Libevent handles for server */
static lew_event *watchdog_event_signals[watchdog_num_signals_handled];

/* ------------------------------------------------------------------------- */
int
main(int argc, char **argv)
{
    int err = 0;

    err = watchdog_watchdog_init();
    bail_error(err);

    err = watchdog_init();
    bail_error(err);

    err = watchdog_toggle_poll_timer(true);
    bail_error(err);
    err = watchdog_main_loop();
    complain_error(err);


    err = watchdog_deinit();
    complain_error(err);

    err = watchdog_watchdog_deinit();
    bail_error(err);


bail:
    lc_log_basic(LOG_NOTICE, _("MFD monitor daemon exiting with code %d"), err);
    lc_log_close();

    exit(err);
}

/* ------------------------------------------------------------------------- */
int
watchdog_initiate_exit(void)
{
    int err = 0;
    uint32 num_recs = 0, i = 0;

    /*
     * Some occurrences are only OK if they happen while we're
     * exiting, such as getting NULL back for a mgmt request.
     */
    watchdog_exiting = true;

    /*
     * Now make ourselves fall out of the main loop by removing all
     * active events.
     */
    for (i = 0; i < watchdog_num_signals_handled; ++i) {
	err = lew_event_delete(watchdog_lwc, &(watchdog_event_signals[i]));
	bail_error(err);
    }

    /*
     * Need to do this here to remove our mgmt-related events.
     */
    err = mdc_wrapper_disconnect(watchdog_mcc, false);
    bail_error(err);

    /*
     * Calling lew_delete_all_events() is not guaranteed
     * to solve this for us, since the GCL/libevent shim does not use
     * the libevent wrapper.  lew_escape_from_dispatch() will
     * definitely do it, though.
     */
    err = lew_escape_from_dispatch(watchdog_lwc, true);
    bail_error(err);

bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
watchdog_init(void)
{
    int err = 0;
    uint32 i = 0;

    /* Don't mess with the modes we choose */
    umask(0);

    /*
     * The LOCAL4 facility is coordinated with an additional facility
     * we add in our graft point in src/include/logging.inc.h
     */
    err = lc_log_init(watchdog_program_name, NULL, LCO_none, 
                      LC_COMPONENT_ID(LCI_WATCHDOG),
                      LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
    bail_error(err);

#if defined(PROD_FEATURE_I18N_SUPPORT)
    err = lc_set_locale_from_config(true);
    complain_error(err);
    err = 0;
#endif

    lc_log_basic(LOG_NOTICE, _("MFD monitor daemon launched"));

    /* Initialize libevent wrapper */
    err = lew_init(&watchdog_lwc);
    bail_error_null(err, watchdog_lwc);

    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */
    for (i = 0; i < watchdog_num_signals_handled; ++i) {
        err = lew_event_reg_signal
            (watchdog_lwc, &(watchdog_event_signals[i]), watchdog_signals_handled[i],
             watchdog_handle_signal, NULL);
        bail_error(err);
    }

    err = watchdog_mgmt_init();
    bail_error(err);

    /*
     * Read in the initial configuration from mgmtd
     */
    err = watchdog_config_query();
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
watchdog_main_loop(void)
{
    int err = 0;

    err = lew_dispatch(watchdog_lwc, NULL, NULL);
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
watchdog_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&watchdog_mcc);
    bail_error(err);

    err = lew_deinit(&watchdog_lwc);
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
watchdog_handle_signal(int signum, short ev_type, void *data, lew_context *ctxt)
{
    int err = 0;

    lc_log_basic(LOG_INFO, "Received %s", lc_signal_num_to_name(signum));

    switch (signum) {
        
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
        err = watchdog_initiate_exit();
        bail_error(err);

    case SIGPIPE:
        /* Currently ignored */
        break;

    case SIGUSR1:
        /* Currently ignored */
        break;

    default:
        lc_log_basic(LOG_WARNING, I_("Got unexpected signal %s"),
                     lc_signal_num_to_name(signum));
        break;
    }

 bail:
    return(err);
}

static int
watchdog_watchdog_init(void)
{
    int err = 0;

    watchdog_config.restart_nvsd = false;
    watchdog_config.is_httpd_enabled = false;
    watchdog_config.is_httpd_listen_enabled = false;
    watchdog_config.is_listen_intf_link_up = false;
    watchdog_config.has_mgmt_intf = false;
    watchdog_config.num_network_threads = 0;
    watchdog_config.hung_count = 0;
    watchdog_config.num_lic_sock = 0;
    watchdog_config.mgmt_intf = NULL;
    watchdog_config.domain = NULL;
    watchdog_config.httpd_listen_port = NULL;
    watchdog_config.mgmt_ip = NULL;
    watchdog_config.is_gdb_ena = true;
    watchdog_config.nvsd_listen_port = 0;
    watchdog_config.probe_uri = NULL;
    watchdog_config.coredump_enabled = 0;
    watchdog_config.probe_o_host = NULL;
    watchdog_config.probe_o_port = NULL;
    /* create gdb cmd files */
    err = watchdog_watchdog_gdb_file_init();
    bail_error(err);

bail:
return err;

}

static int
watchdog_watchdog_deinit(void)
{
    safe_free(watchdog_config.mgmt_intf);
    safe_free(watchdog_config.httpd_listen_port);
    safe_free(watchdog_config.mgmt_ip);
    safe_free(watchdog_config.domain);
    safe_free(watchdog_config.probe_uri);
    safe_free(watchdog_config.probe_o_host);
    safe_free(watchdog_config.probe_o_port);

    return 0;
}
/* ------------------------------------------------------------------------- */
/* End of watchdog_main.c */

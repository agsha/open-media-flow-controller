#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <glib.h>
#include <pthread.h>
#include "md_client.h"
#include "mdc_wrapper.h"

#include <libevent_wrapper.h>
#include "accesslog_pub.h"
#include "accesslog_pri.h"


static void usage(void);
static void version(void);

pthread_mutex_t acc_lock;

int al_exit_req = 0;

const char *accesslog_program_name = "accesslog";

////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

static void usage(void)
{
        printf("\n");
        printf("nknaccesslog - Nokeena Access Log Tool\n");
        printf("Usage: \n");
        printf("-f <name> : configuration file\n");
        printf("-d        : Run as Daemon\n");
        printf("-v        : show version\n");
        printf("-h        : show this help\n");
        printf("Examples:\n");
        printf("1) To run:\n");
        printf("            nknaccesslog -f <config_file>\n");
        printf("2) To display version:\n");
        printf("            nknaccesslog -v\n");
        printf("\n");
        exit(1);
}

static void version(void)
{
        printf("\n");
        printf("nknaccesslog - Nokeena Access Log Application\n");
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

void catcher(int sig)
{
    if (glob_run_under_pm) {
        switch(sig) {
        case SIGHUP:
            break;
        case SIGINT:
        case SIGTERM:
        case SIGPIPE:
                // FIX - 1060
                // Set flag to indicate an exit request
                al_exit_req = 1;
                al_mgmt_initiate_exit();
                //http_accesslog_exit();
                //al_close_socket();
                //nkn_free_cfg();
                //nkn_save_fileid();
                //exit(3);
                break;
        case SIGUSR1:
                break;
        default:
                break;
        }
    }
    else {
        http_accesslog_exit();
        al_close_socket();
        nkn_free_cfg();
        nkn_save_fileid();
        exit(3);
    }
}


/* 
 * This global variable is set (1) to indicate that we want TallMaple - PM
 * to control us. This is reset (0) to indicate that the user wants to 
 * control the option. 
 *
 * If user invokes us with -f option, then it assumed tha the config options
 * are being given from the command line, in which case we run outside of
 * the process manager.
 */
int glob_run_under_pm = 1;

////////////////////////////////////////////////////////////////////////
// main Functions
////////////////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
    int runas_daemon=0;
    char * cfgfile=NULL;
    int ret;
        int err = 0;
        int i = 0;

    while ((ret = getopt(argc, argv, "f:dhv")) != -1) {
        switch (ret) {
        case 'f':
             glob_run_under_pm = 0;
             cfgfile=optarg;
             break;
        case 'd':
             runas_daemon=1;
             break;
        case 'h':
             usage();
             break;
        case 'v':
             version();
             break;
        }
    }

    if(!glob_run_under_pm && runas_daemon) daemonize();

    /* User decides whether we need to use GCL or we run 
     * standalone - outside of PM
     */
    if (glob_run_under_pm) {

        pthread_mutex_init(&acc_lock, NULL);
        g_thread_init(NULL);

        /* Connect/create GCL sessions, initialize mgmtd i/f
         */
        err = al_init();
        bail_error(err);


        err = al_main_loop();
        complain_error(err);

        err = al_deinit();
        complain_error(err);

        goto bail;

    }
    else {

        signal(SIGHUP,catcher);
        signal(SIGKILL,catcher);
        signal(SIGTERM,catcher);
        signal(SIGINT,catcher);
        signal(SIGPIPE,SIG_IGN);

        // Load the configuration file
        if(!cfgfile) {
        printf("ERROR: configuration is not provided.\n");
        exit(2);
        }
        read_nkn_cfg(cfgfile);
        
        nkn_check_cfg();
        print_cfg_info();

        // Open the log file
        nkn_read_fileid();
        http_accesslog_init();

        // Launch the network socket.
        // This function should never return.
        make_socket_to_svr();
    }

    return 1;

bail:
    return err;
}



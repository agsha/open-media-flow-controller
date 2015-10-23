#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <getopt.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/syslog.h>
#include <errno.h>
#include "common.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"

int debug_level  = LOG_INFO;
int check_pid(const char *pid_file);
int parse_args(int argc, char **argv);
int log_to_syslog(int level, const char *format, ...);

int
check_pid(const char *pid_file)
{
    str_value_t sz_buf = {0};
    int	n_size = 0;
    int	pid = 0;
    FILE	*fp_pidfile = NULL;
    /* Sanity test */
    if (!pid_file) return 1;

    /* Now open the pid file */
    fp_pidfile = fopen (pid_file, "r");
    if (NULL == fp_pidfile)
    {
	/* No pid file hence no process */
	return 0;
    }
    /* Now read the PID */
    memset (sz_buf, 0, sizeof(sz_buf));
    n_size = fread (sz_buf, sizeof(char), sizeof(sz_buf), fp_pidfile);
    if (n_size <= 0)
    {
	/* Empty PID file - should never happen */
	fclose(fp_pidfile);
	return 3;
    }

    /* Close the FILE */
    fclose(fp_pidfile);

    /* We assume the buffer has the PID */
    pid = atoi(sz_buf);
    if (pid > 0 && pid != getpid()) {
	/* a ingester is already running into the system */
	exit(1);
    }
    return 0;
}

void exitclean(int num_signal);
int got_signal = 0;
int child_exits = 0;
void exitclean(int num_signal)
{
    lc_log_basic(debug_level, "GOT Signal : %d", num_signal);
    if (num_signal == SIGCHLD) {
	child_exits = 1;
    }
    got_signal = 1;
    return ;
}
int main(int argc, char **argv)
{
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

    //check_pid("/coredump/ingester/ingester.pid");
    parse_args(argc, argv);

    signal(SIGTERM, exitclean);
    signal(SIGHUP, exitclean);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    for (;;) {
	int err = 0;
	int ret_code = 0;
	int i = 0;

	child_exits = 0;

	lc_launch_params_free(&lp);
	if(lr) {
	    lc_launch_result_free_contents(lr);
	    safe_free(lr);
	}
	lr = malloc(sizeof(lc_launch_result));
	bail_null(lr);

	memset(lr, 0 ,sizeof(lc_launch_result));

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);

        err = ts_new_str(&(lp->lp_path),"/usr/bin/ssh-43p2");
	bail_error(err);

        err = tstr_array_new_va_str
            (&(lp->lp_argv), NULL, 7,"/usr/bin/ssh-43p2", 
	     "-N", "-q", "-g", "-F",
	     "/var/opt/tms/output/ssh_43p2_config", "admin@10.84.77.10");
        bail_error(err);

	lp->lp_log_level = LOG_INFO;
	lp->lp_wait = false;
	err = lc_launch(lp, lr);
	bail_error(err);

	for (;;) {
	    int ret_status  =0 ;
	    int ret_pid = 0;

	    i++;
	    err = lc_wait(lr->lr_child_pid, 1, 2000, &ret_pid, &ret_status);
	    lc_log_basic(debug_level, "iter: %d, ret_code = %d, statsus: %d, pid: %d",
		    i, err, ret_status, ret_pid);
	    if (got_signal || (-1 != ret_pid)) {
		lc_log_basic(debug_level, "reaping pid: %d", lr->lr_child_pid);
		err = lc_kill_and_reap(lr->lr_child_pid, SIGTERM, 3, SIGKILL);
		break;
	    }
	}
	if (got_signal) {
	    break;
	}
	lc_log_basic(debug_level, "TUNNEL-EXIT %d", ret_code);
	sleep(10);
    }


bail:
    lc_launch_params_free(&lp);
    return 0;
}

int parse_args(int argc, char **argv)
{
    int n = 1; // how many args we parsed.
    int c;

    while((c = getopt(argc, argv, "v:")) != -1) {
	switch(c) {
	    case 'v':
		debug_level = atoi(optarg);
		if (debug_level > 7) {
		    debug_level = 7;
		}
		break;

	    case '?':
		lc_log_basic(LOG_NOTICE, "Unrecognized option: -%c\n", optopt);
		break;
	}
	n++;
    }

    return n;
}

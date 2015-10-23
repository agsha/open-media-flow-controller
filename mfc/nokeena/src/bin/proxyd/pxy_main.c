#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdint.h>
#include <netdb.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "pxy_network.h"
#include "pxy_defs.h"
#include "pxy_main.h"
#include "pxy_server.h"
#include "nkn_cfg_params.h"


volatile sig_atomic_t pxy_srv_shutdown = 0;
int                   pxy_dont_daemonize = 0;
time_t                pxy_cur_ts;
int                   pxy_system_inited = 0;
unsigned long         glob_proxyd_uptime_since = 0;


int l4proxy_enabled = 0; 

/* 
 * ==================> main functions <======================
 * initialization supporting functions
 */
static void check_proxyd(void)
{
        DIR * dir=opendir("/proc");
        struct dirent * ent;
        pid_t pid, mypid;
        int len;
        int fd;
        char buffer[4096];

	mypid=getpid();

        while ((ent = readdir(dir)) != NULL)
        {
                if (!isdigit(ent->d_name[0]))
                        continue;

                pid=atoi(ent->d_name);
		if(pid == mypid) continue;

                snprintf(buffer, 4096, "/proc/%d/cmdline", pid);
                if ((fd = open(buffer, O_RDONLY)) != -1)
                {
                        // read command line data
                        if ((len = read(fd, buffer, 4096)) > 1)
                        {
                                buffer[len] = '\0';
                                if(strstr(buffer, "proxyd") != 0) {
					printf("\nThere is another running proxyd process (pid=%d)\n", pid);
					printf("Please exit it before continue.\n\n");
					exit(2);
                                }
                        }
                }

                close(fd);
        }

        closedir(dir);

        return;
}

void catcher(int sig)
{
	switch (sig)
	{
		case SIGHUP: exit(1); break;
		case SIGINT: 
		case SIGTERM: 
			pxy_srv_shutdown = 1; 
			pxy_mgmt_initiate_exit();
		break;
		case SIGKILL: exit(1); break;
	}
}


static void daemonize(void) 
{

        if (0 != fork()) exit(0);

        if (-1 == setsid()) exit(0);


        if (0 != fork()) exit(0);

}

static int pxy_server_init(void)
{
	int           i_am_root;
	struct rlimit rlim;
	FILE        * fp;

	/*
	 * Catalog 1: all modules depend on this initialization
	 */
	check_proxyd();
	if (pxy_dont_daemonize == 0) daemonize();

        pxy_server_timer_1sec();

	log_thread_start();
	DBG_LOG(MSG, MOD_PROXYD, "proxyd started initialization.");

	if (getuid() != 0) {
		/* not root account */
		assert(0);
	}

	// Set RLIMIT_NOFILE only
	if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
		DBG_LOG(MSG, MOD_PROXYD,
			"failed to get rlimit errno=%s", strerror(errno));
		return -1;
	}

	rlim.rlim_cur = MAX_GNM; // Just set to 64K
	rlim.rlim_max = MAX_GNM; // Just set to 64K

	if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
		DBG_LOG(MSG, MOD_PROXYD,
			"failed to set rlimit errno=%s",
			strerror(errno));
		return -1;
	}
	DBG_LOG(MSG, MOD_PROXYD,
		"File descriptor rlimit: rlim_cur=%ld rlim_max=%ld",
		rlim.rlim_cur, rlim.rlim_max);

	/*
	 * Catalog 2: these modules depends on no other modules
	 */
	pxy_interface_init();	// Initialize network interface
	pxy_NM_init();		// Initialize Network threads

	/*
	 * Catalog 4: Modules to be initialized at last
	 * 	      Within this module, order might be important too.
	 */
	pxy_server_timer_init();	// Initialize timer thread

	/*
	 * Catalog 5: External services
	 */
        if (l4proxy_enabled) {
	    pxy_httpsvr_init(); 	// Initialize Socket : ==> every modules
	    DBG_LOG(SEVERE, MOD_PROXYD, "L4 proxy enabled & listens on the HTTP ports");
        }

	/*
	 * Give some indication to user
	 */
	DBG_LOG(SEVERE, MOD_PROXYD, "L4 proxyd system is ready");
	DBG_LOG(ALARM, MOD_PROXYD, "-- proxyd initialization is done. System is ready. --");
	pxy_system_inited = 1;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	glob_proxyd_uptime_since = tv.tv_sec;

	return 1;
}

static int pxy_server_exit(void)
{
	return 1;
}

/* 
 * ==================> main functions <======================
 * initialization supporting functions
 */

static void usage(void)
{
	printf("\n");
	printf("proxyd - Juniper L4 Proxy Daemon\n");
	printf("Usage:\n");
	printf("-f <name> : filename of the config-file\n");
	printf("-D        : don't go to background (default: go to background)\n");
	printf("-v        : show version\n");
	printf("-h        : show this help\n");
	exit(1);
}

static void version(void)
{
	printf("\n");
	printf("proxyd: L4 Proxy Daemon\n");
	printf("%s\n", nkn_build_date);
	printf("%s\n", nkn_build_id);
	printf("%s\n", nkn_build_prod_release);
	printf("%s\n", nkn_build_number);
	printf("%s\n", nkn_build_scm_info_short);
	printf("%s\n", nkn_build_scm_info);
	printf("%s\n", nkn_build_extra_cflags);
	printf("\n");
	exit(1);

}


int main(int argc, char **argv)
{
	int ret;
	char *configfile=NULL;

	while ((ret = getopt(argc, argv, "f:hvD")) != -1) {
                switch (ret) {
                case 'f':
			configfile = optarg;
                        break;
		case 'h':
                        usage();
			break;
		case 'v':
                        version();
			break;
		case 'D':
			pxy_dont_daemonize = 1;
			break;
		default:
			usage();
			break;
                }
        }

	/* Get a real time priority */
	setpriority(PRIO_PROCESS, 0, -1);	

	/* Initialize the counters */
	config_and_run_counter_update();	// Initialize counters

	/* Read configuration from nkn.conf.default file */
	read_pxy_cfg(configfile);

	/*  ---- mgmtd init */
	pxy_mgmt_init();

	/* Config Check is internal AND MUST BE THE LAST CALL before 
	 * server_init() is called.
	 */
	check_pxy_cfg();

	// All initialization functions should be called within server_init().
	if(pxy_server_init() != 1) {
		return 0;
	}

	/* 
	 * ============> infinite loop <=============
	 * NM_main() will never exit unless people use Ctrl-C or Kill
	 */
	pxy_NM_main();

	// Done. We exit
	pxy_server_exit();

	return 0;
}

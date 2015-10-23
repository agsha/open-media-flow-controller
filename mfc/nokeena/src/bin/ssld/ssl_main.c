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
#include <limits.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>

#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "ssl_defs.h"
#include "ssld_mgmt.h"

#ifdef HTTP2_SUPPORT
#include "proto_http/proto_http.h"
#endif

NKNCNT_DEF(shutdown_close_wait_socket, uint64_t, "", "version expured")

int dont_daemonize = 0;
int nkn_system_inited = 0;
unsigned long glob_ssld_uptime_since = 0;
time_t nkn_cur_ts;
volatile sig_atomic_t srv_shutdown = 0;
int valgrind_mode = 0;
static char nkn_build_date[] =           ":Build Date: " __DATE__ " " __TIME__ ;
static char nkn_build_id[] =             ":Build ID: " NKN_BUILD_ID ;
static char nkn_build_prod_release[] =   ":Build Release: " NKN_BUILD_PROD_RELEASE ;
static char nkn_build_number[] =         ":Build Number: " NKN_BUILD_NUMBER ;
static char nkn_build_scm_info_short[] = ":Build SCM Info Short: " NKN_BUILD_SCM_INFO_SHORT ;
static char nkn_build_scm_info[] =       ":Build SCM Info: " NKN_BUILD_SCM_INFO ;
static char nkn_build_extra_cflags[] =   ":Build extra cflags: " EXTRA_CFLAGS_DEF ;

extern int update_from_db_done;

void catcher(int sig);
extern void NM_init(void);
extern void server_timer_init(void);
extern void ssl_interface_init(void);
extern int interface_init(void);
extern void config_and_run_counter_update(void);
extern void cfg_params_init(void);
extern int read_ssl_cfg(char * cfgfile);
extern int check_ssl_cfg(void);
extern void ssld_mgmt_thrd_init(void);
extern void NM_main(void);
extern int ssld_mgmt_initiate_exit(void);
extern int mgmt_init_done ;
extern void cp_init(void);
int THREAD_setup(void);
int THREAD_cleanup(void);

#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()



/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE *mutex_buf = NULL;

static void locking_function(int mode, int n, const char * file, int line)
{
	if (mode & CRYPTO_LOCK)
		MUTEX_LOCK(mutex_buf[n]);
	else
		MUTEX_UNLOCK(mutex_buf[n]);
}

static unsigned long id_function(void)
{
	return ((unsigned long)THREAD_ID);
}

struct CRYPTO_dynlock_value
{
	MUTEX_TYPE mutex;
};

static struct CRYPTO_dynlock_value * dyn_create_function(const char *file, int line)
{
	struct CRYPTO_dynlock_value *value;
	value = (struct CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value));

	if (!value)
		return NULL;
	MUTEX_SETUP(value->mutex);
	return value;
}

static void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		MUTEX_LOCK(l->mutex);
	else
	MUTEX_UNLOCK(l->mutex);
}

static void dyn_destroy_function(struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	MUTEX_CLEANUP(l->mutex);
	free(l);
}

int THREAD_setup(void)
{
	int i;
	mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
	if (!mutex_buf)
		return 0;
	for (i = 0; i < CRYPTO_num_locks(); i++)
		MUTEX_SETUP(mutex_buf[i]);

	CRYPTO_set_id_callback(id_function);
	CRYPTO_set_locking_callback(locking_function);
	/* The following three CRYPTO_... functions are the OpenSSL functions
	for registering the callbacks we implemented above */
	CRYPTO_set_dynlock_create_callback(dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
	return 1;
}

int THREAD_cleanup(void)
{
	int i;
	if (!mutex_buf)
		return 0;

	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	CRYPTO_set_dynlock_create_callback(NULL);
	CRYPTO_set_dynlock_lock_callback(NULL);
	CRYPTO_set_dynlock_destroy_callback(NULL);
	for (i = 0; i < CRYPTO_num_locks(); i++)
		MUTEX_CLEANUP(mutex_buf[i]);

	free(mutex_buf);
	mutex_buf = NULL;
	return 1;
}
/*
 * ==================> main functions <======================
 * initialization supporting functions
 */
static void check_ssld(void)
{
        DIR *dir = opendir("/proc");
        struct dirent *ent;
        pid_t pid, mypid;
        int len;
        int fd;
        char buffer[4096];

	mypid = getpid();

        while ((ent = readdir(dir)) != NULL) {
                if (!isdigit(ent->d_name[0])) continue;

                pid = atoi(ent->d_name);
		if (pid == mypid) continue;

                snprintf(buffer, 4096, "/proc/%d/cmdline", pid);
                if ((fd = open(buffer, O_RDONLY)) != -1) {
                        // read command line data
                        if ((len = read(fd, buffer, 4096)) > 1)
                        {
                                buffer[len] = '\0';
                                if (strstr(buffer, "ssld") != 0) {
					printf("\nThere is another running ssld process (pid=%d)\n", pid);
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
			srv_shutdown = 1;
			ssld_mgmt_initiate_exit();
		break;
		case SIGKILL: exit(1); break;
	}

	return;
}

static void daemonize(void)
{

        if (0 != fork()) exit(0);

        if (-1 == setsid()) exit(0);

#if 0
        signal(SIGHUP, SIG_IGN);
#endif /* 0 */

        if (0 != fork()) exit(0);

        /* Do not chdir when this processis managed by the PM
         * if (0 != chdir("/")) exit(0);
         */

	return;
}

static int server_init(void)
{
	int i_am_root;
	FILE *fp;
	struct rlimit rlim;
	struct timeval tv;

	/*
	 * Catalog 1: all modules depend on this initialization
	 */
	check_ssld();
	if (dont_daemonize == 0) daemonize();

	log_thread_start();
	DBG_LOG(MSG, MOD_SYSTEM, "ssld started initialization.");

	if (getuid() != 0) {
		/* not root account */
		assert(0);
	}

	// Set RLIMIT_NOFILE only
	if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
		DBG_LOG(MSG, MOD_SYSTEM,
			"failed to get rlimit errno=%s", strerror(errno));
		return -1;
	}
	if (!valgrind_mode) {
	    rlim.rlim_cur = MAX_GNM; // Just set to 64K
	    rlim.rlim_max = MAX_GNM; // Just set to 64K

	    if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
		    DBG_LOG(MSG, MOD_SYSTEM,
			    "failed to set rlimit errno=%s",
			    strerror(errno));
		    return -1;
	    }
	}
	DBG_LOG(MSG, MOD_SYSTEM,
		"File descriptor rlimit: rlim_cur=%ld rlim_max=%ld",
		rlim.rlim_cur, rlim.rlim_max);

	/*
	 * Catalog 2: these modules depends on no other modules
	 */
	interface_init();	// Initialize network interface
	NM_init();		// Initialize Network threads

	/*
	 * Catalog 4: Modules to be initialized at last
	 * 	      Within this module, order might be important too.
	 */
	server_timer_init();	// Initialize timer thread

	/*
	 * Catalog 5: External services
	 */
	//ssl_server_init(NULL, 0);

	ssl_interface_init();
	cp_init();

#ifdef HTTP2_SUPPORT
	/*
	 * proto_http initialization
	 */
	{
	    int rv;
	    proto_http_config_t cfg;

	    memset((char *)&cfg, 0, sizeof(cfg));
	    cfg.interface_version = PROTO_HTTP_INTF_VERSION;
	    proto_http_init(&cfg);

	    rv = http2_init();
	    if (rv) {
		DBG_LOG(MSG, MOD_SYSTEM, "http2_init() failed, rv=%d", rv);
		return -1;
	    }
	}
#endif /* HTTP2_SUPPORT */

	/*
	 * Give some indication to user
	 */
	DBG_LOG(ALARM, MOD_SSL, "-- ssld initialization is done. --");
	nkn_system_inited = 1;

	gettimeofday(&tv, NULL);
	glob_ssld_uptime_since = tv.tv_sec;

	return 1;
}

static int server_exit(void)
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
	printf("ssld - Juniper SSL Daemon\n");
	printf("Usage:\n");
	printf("-f <name> : filename of the config-file\n");
	printf("-D        : don't go to background (default: go to background)\n");
	printf("-V        : valgrind mode\n");
	printf("-v        : show version\n");
	printf("-h        : show this help\n");
	exit(1);
}

static void version(void)
{
	printf("\n");
	printf("ssld: SSL Daemon\n");
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

	while ((ret = getopt(argc, argv, "f:hvDV")) != -1) {
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
			dont_daemonize = 1;
			break;
		case 'V':
			valgrind_mode = 1;
			break;
		default:
			usage();
			break;
                }
        }

	/* Get a real time priority */
	setpriority(PRIO_PROCESS, 0, -1);

	check_ssld();
	ssl_lib_init();
	THREAD_setup();

	/* Initialize the counters */
	config_and_run_counter_update();// Initialize counters

	/* Read configuration from nkn.conf.default file */
	read_ssl_cfg(configfile);

	/*  ---- mgmtd init */
	ssld_mgmt_thrd_init();

	/* Config Check is internal AND MUST BE THE LAST CALL before
	 * server_init() is called.
	 */
	check_ssl_cfg();

	// All initialization functions should be called within server_init().
	while (!update_from_db_done) {
		sleep(1);
	}

	if (server_init() != 1) {
		THREAD_cleanup();
		return 0;
	}

	/*
	 * ============> infinite loop <=============
	 * NM_main() will never exit unless people use Ctrl-C or Kill
	 */
	NM_main();

	// Done. We exit
	server_exit();

	THREAD_cleanup();
	return 0;
}

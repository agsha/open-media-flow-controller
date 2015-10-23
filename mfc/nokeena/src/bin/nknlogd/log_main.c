#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/un.h>
#include <pthread.h>
#include "md_client.h"
#include "mdc_wrapper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>

#include <strings.h>
#include <curl/curl.h>
#include <libevent_wrapper.h>
#include "accesslog_pub.h"
#include "log_accesslog_pri.h"
#include "nknlogd.h"

extern struct sockaddr_in dip;
extern socklen_t dlen ;
extern void uds_server_log_init(void);
extern void log_init(void);
extern int log_channel_init(void);

struct sockaddr_in srv_addr;
extern void nkn_process_cfg(char * cfgfile);
extern void nkn_launch_logd_server(void);
extern void *nkn_launch_misc_logd_server(void *arg);
//extern int nkn_check_cfg(void);
extern void nkn_free_cfg(void);

extern int lgd_worker_init(void);

static void usage(void);
static void version(void);

struct log_nm_queue g_nm_to_free_queue;
const char * program_name = "nknlogd";
pthread_mutex_t log_lock;
pthread_mutex_t nm_free_queue_lock;
pthread_mutex_t upload_mgmt_log_lock;
extern tbool mgmt_exiting;
pthread_t   non_accesslog_thrd;
int exit_req = 0;
extern int update_from_db_done; // This flag is used to determine if the log config data has been updated from the database. 

//extern void * alan_housekeeping(void * arg);
extern void config_and_run_counter_update(void);

////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

void print_hex(char * name, unsigned char * buf, int size)
{
        int i, j;
        char ch;

        printf("name=%s", name);

        for(i=0;i<size/16;i++) {
                printf("\n%p ", &buf[i*16]);
                for(j=0; j<16;j++) {
                        printf("%02x ", buf[i*16+j]);
                }
                printf("    ");
                for(j=0; j<16;j++) {
                        ch=buf[i*16+j];
                        if(isascii(ch) &&
                           (ch!='\r') &&
                           (ch!='\n') &&
                           (ch!=0x40) )
                           printf("%c", ch);
                        else printf(".");
                }
        }
        printf("\n%p ", &buf[i*16]);
        for(j=0; j<size-i*16;j++) {
                printf("%02x ", buf[i*16+j]);
        }
        printf("    ");
        for(j=0; j<size-i*16;j++) {
                ch=buf[i*16+j];
                if(isascii(ch) &&
                   (ch!='\r') &&
                   (ch!='\n') &&
                   (ch!=0x40) )
                   printf("%c", ch);
                else printf(".");
        }
        printf("\n");
}

void check_nknlogd(void);
void check_nknlogd(void)
{
        DIR * dir=opendir("/proc");
	if(dir == NULL)
	{
	    complain_error_errno(1,"Err in opening /proc" );
	}

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
                                if(strstr(buffer, "nknlogd") != 0) {
                                        printf("\nThere is another running nknlogd process (pid=%d)\n", pid);
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

static void usage(void)
{
        printf("\n");
        printf("nknlogd - Juniper Log Daemon\n");
        printf("	Load configuration from configuration file if provided,\n");
        printf("	Otherwise load configuration information from TM.\n");
        printf("Usage: \n");
        printf("-f <name> : configuration file, e.g. /config/nkn/nknlogd.cfg\n");
        printf("-d        : Run as Daemon\n");
        printf("-s        : Run as standalone, No GCL\n");
        printf("-v        : show version\n");
        printf("-h        : show this help\n");
        printf("Examples:\n");
        printf("1) To run:\n");
        printf("            nknlogd -f <config_file>\n");
        printf("2) To display version:\n");
        printf("            nknlogd -v\n");
        printf("\n");
        exit(1);
}

static void version(void)
{
        printf("\n");
        printf("nknlogd - Juniper Log Daemon\n");
        printf("Build Date: "__DATE__ " " __TIME__ "\n");
        printf("\n");
        exit(1);

}

static void daemonize(void)
{

	printf("daemonizing...\n");
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
	log_read_fileid();
	if (glob_run_under_pm) {
		switch(sig) {
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
		case SIGPIPE:
			// FIX - 1060
			// Set flag to indicate an exit request
			exit_req = 1;
			mgmt_initiate_exit();
			break;
		case SIGUSR1:
			break;
		default:
			break;
		}
	}
	else {
		//nkn_free_cfg();
	}
	printf("Exit on signal!!\n");
	exit(0);
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

int g_log_network_thread_wait = 1;

////////////////////////////////////////////////////////////////////////
// main Functions
////////////////////////////////////////////////////////////////////////

/*
 * TM function is running in a separate thread.
 * Main thread is used for network socket handling.
 */
void * mgmt_func(void * arg);
void * mgmt_func(void * arg)
{
        int err = 0;

	int start = 1;
	/* Connect/create GCL sessions, initialize mgmtd i/f
	*/
	while (1)
	{

		mgmt_exiting = false;
		printf("mgmt connect: initialized\n");
		if (start == 1) {
			err = mgmt_init(1);
			g_log_network_thread_wait = 0;
			bail_error(err);
		} else {
			while ((err = mgmt_init(0)) != 0) {
				deinit();
				sleep(2);
			}
		}

		err = main_loop();
		complain_error(err);

		err = deinit();
		complain_error(err);
		printf("mgmt connect: de-initialized\n");
		start = 0;
	}

bail:
	printf("mgmt_func: exit\n");
	if(err){
	    syslog(LOG_NOTICE,
		    ("error: initing nknlogd mgmt thread"));
	    exit(1);
	}
	return NULL;
}


int flush_otherlogs(void);
int flush_otherlogs( )
{
    int fd = 0;
    int ret ;

    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){

	perror("Failed to open socket! ");
	return 1;
    }

    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = DEF_LOG_IP;//0x0100007F;
    //getsockname gives the port in network order
    srv_addr.sin_port = dip.sin_port;

    ret = sendto(fd, "0", 1, 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
    if (!ret) {
	perror("send failed: ");
    }
    close(fd);
    return 0;
}

void flush_accesslog(void);
void flush_accesslog(void)
{
    int fd = 0;
    const char *u = "/vtmp/nknlogd/accesslog/uds-control";
    struct sockaddr_un addr;
    size_t addr_len;
    size_t l;
    int ret;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(fd == -1)
    {
	complain_error_errno(1,"Acclog control socket open failed");
    }

    addr.sun_family = AF_UNIX;
    l = sprintf(addr.sun_path, "%s", u);
    addr_len = sizeof(addr.sun_family) + l;

    ret = sendto(fd, "0", 1, 0, (struct sockaddr *) &addr, addr_len);
    if (!ret) {
	perror("sendto failed: ");
    }
    close(fd);
}

void * flush_logdata(void * arg);
void * flush_logdata(void * arg)
{
    uint32_t sleep_interval = 2;
    uint32_t i = 0;
    logd_file * plf;

    while(1){
	sleep(sleep_interval);
	for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next){
	    pthread_mutex_lock(&plf->fp_lock);
	    if(plf->fp){
		int ret = 0;
		ret = fflush(plf->fp);
		log_write_report_err(ret, plf);
	    }

	    pthread_mutex_unlock(&plf->fp_lock);
	}

	flush_otherlogs( );
	flush_accesslog();

    }
    return NULL;
}


int main(int argc, char * argv[])
{
	int runas_daemon=0;
	char * cfgfile=NULL;
	int ret;
	pthread_t mgmtid;
	pthread_t al_analyzerid;
	pthread_t timer;

	config_and_run_counter_update();
	check_nknlogd();

	while ((ret = getopt(argc, argv, "f:sdhv")) != -1) {
		switch (ret) {
		case 'f':
			cfgfile=optarg;
		break;
		case 's':
			glob_run_under_pm = 0;
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

	curl_global_init(CURL_GLOBAL_ALL);

	if(!glob_run_under_pm && runas_daemon) daemonize();

	uds_server_log_init();
	log_channel_init();
	lgd_worker_init();

	LIST_INIT(&g_nm_to_free_queue);

	/*
	 * Load the configuration file first.
	 * Any TM configuration will overwrite the configuraton loaded from configuration file.
	 * if -s option is provided, we skip reading TM configuration.
	 */
	// Load the configuration file
	if(!cfgfile) {
		printf("ERROR: configuration is not provided.\n");
		usage();
	}
	log_read_fileid();
	nkn_process_cfg(cfgfile);
	/* 
	 * User decides whether we need to use GCL or we run 
	 * standalone - outside of PM
	 */
	pthread_mutex_init(&log_lock, NULL);
	pthread_mutex_init(&upload_mgmt_log_lock, NULL);
	pthread_mutex_init(&nm_free_queue_lock, NULL);
	//g_thread_init(NULL);

	/*
         * for fork()
         * Moving it here so as to avoid the defunct process
         * being created in log_file_exit
         */
        signal(SIGCHLD,SIG_IGN);


	if (glob_run_under_pm) {
		pthread_create(&mgmtid, NULL, mgmt_func, NULL);
		while (!update_from_db_done) {
			sleep(1);
			//do nothing
		}
	        ret = nkn_check_cfg();
	}
	else {

		signal(SIGHUP,catcher);
		signal(SIGKILL,catcher);
		signal(SIGTERM,catcher);
		signal(SIGINT,catcher);
		signal(SIGPIPE,SIG_IGN);

	}

	//pthread_create(&al_analyzerid, NULL, alan_housekeeping, NULL);
	pthread_create(&non_accesslog_thrd, NULL,
			nkn_launch_misc_logd_server, NULL);
	sleep(2);
	pthread_create(&timer, NULL, flush_logdata, NULL);

	pthread_detach(non_accesslog_thrd);

	nkn_launch_logd_server();
	return 1;

}


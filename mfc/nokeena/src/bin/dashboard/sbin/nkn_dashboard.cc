#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "nkn_dashboard.h"
#include "nkncnt_client.h"

glob_item_t * g_allcnts;
nkn_shmdef_t * pshmdef;
char * varname;
int revision;
extern nkncnt_client_ctx_t g_nvsd_mem_ctx;

static void usage(void);
static void catcher(int sig);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

static void usage(void)
{
        printf("\n");
        printf("nkn_dashboard - Nokeena Dashboard/Display Tool\n\n");
        printf("Build Date: "__DATE__ " " __TIME__ "\n");

        printf("Usage:\n");
        printf("-t <time> : display interval in seconds. default: 10. when 0: it runs only once\n");
        printf("-d        : Run as Daemon\n");
        printf("-h        : show this help\n\n");
        exit(1);
}

static void daemonize(void)
{

        if (0 != fork()) exit(0);

        if (-1 == setsid()) exit(0);

        signal(SIGHUP, SIG_IGN);

        if (0 != fork()) exit(0);

        /* Do not chdir when this processis managed by the PM
         * if (chdir("/") != 0) exit(0);
         */
}

void catcher(int sig)
{
	exit(1);
}


////////////////////////////////////////////////////////////
// Main function
////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
	int shmid;
	char *shm;
	int runas_daemon=0;
	int ret;
	pthread_t pthread1, pthread2;
	uint64_t NKN_SHMSZ;
	int err = 0;

	while ((ret = getopt(argc, argv, "t:hd")) != -1) {
        switch (ret) {
        case 't':
		sleep_time = atoi(optarg);
		break;
        case 'd':
		runas_daemon=1;
		break;
        case 'h':
		usage();
		break;
        }
	}

	/*
	 * necessary unix application signaling handling.
	 */
	if(runas_daemon) daemonize();

	signal(SIGHUP,catcher);
	signal(SIGKILL,catcher);
	signal(SIGTERM,catcher);
	signal(SIGINT,catcher);
	signal(SIGSEGV,catcher);


	/*
	 * Locate the segment, try again in case we started nkn_dashboard before 
	 * nvsd has properly started
	 */
	NKN_SHMSZ = (sizeof(nkn_shmdef_t)+MAX_CNTS_NVSD*(sizeof(glob_item_t)+30));
	while ((shmid = shmget(NKN_SHMKEY, NKN_SHMSZ, 0666)) < 0) {
		perror("shmget error, nvsd may not running on this machine, trying again");
		sleep(10); //exit(1);
	}

	/*
	 * Now we attach the segment to our data space.
	 */
	if ((shm = (char *)shmat(shmid, NULL, 0)) == (char *) -1) {
		perror("shmat error, nvsd may not running on this machine.");
		exit(1);
	}
	pshmdef = (nkn_shmdef_t *) shm;
	g_allcnts = (glob_item_t *)(shm+sizeof(nkn_shmdef_t));
	varname = (char *)(shm+sizeof(nkn_shmdef_t)+sizeof(glob_item_t)*MAX_CNTS_NVSD);

	if(strcmp(pshmdef->version, NKN_VERSION)!=0) {
		printf("Version does not match. It requires %s\n", pshmdef->version);
		printf("This binary %s version is %s\n", argv[0], NKN_VERSION);
		exit(1);
	}

	revision = pshmdef->revision;
	    err = nkncnt_client_init(shm, MAX_CNTS_NVSD, &g_nvsd_mem_ctx);
	  //  bail_error(err);

	/*
	 * Create the thread to update data
	 */
	pthread_create(&pthread1, NULL, generate_data, NULL);

	sleep(2);

	/*
         * Create another thread to display graph
	 */
	pthread_create(&pthread2, NULL, generate_graph, NULL);

	pthread_join(pthread1, NULL);
	pthread_join(pthread2, NULL);

	exit(0);
}



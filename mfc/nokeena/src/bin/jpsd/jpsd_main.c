/*
 * @file jpsd_main.c
 * @brief
 * jpsd_main.c - definations for jpsd functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "nkn_memalloc.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_mem_counter_def.h"

#include "jpsd_defs.h"
#include "jpsd_network.h"

extern int update_from_db_done;

static char jpsd_build_date[] = ":Build Date: " __DATE__ " " __TIME__ ;
static char jpsd_build_id[] = ":Build ID: " NKN_BUILD_ID;
static char jpsd_build_prod_release[] = ":Build Release: " NKN_BUILD_PROD_RELEASE;
static char jpsd_build_number[] = ":Build Number: " NKN_BUILD_NUMBER;
static char jpsd_build_scm_info_short[] = ":Build SCM Info Short: " NKN_BUILD_SCM_INFO_SHORT;
static char jpsd_build_scm_info[] = ":Build SCM Info: " NKN_BUILD_SCM_INFO;
static char jpsd_build_extra_cflags[] = ":Build extra cflags: " EXTRA_CFLAGS_DEF;

extern void jpsd_mgmt_thrd_init(void);

static void jpsd_version(void)
{
	printf("\n");
        printf("nvsd: Nokeena Video Server Daemon\n");
        printf("%s\n", jpsd_build_date);
        printf("%s\n", jpsd_build_id);
        printf("%s\n", jpsd_build_prod_release);
        printf("%s\n", jpsd_build_number);
        printf("%s\n", jpsd_build_scm_info_short);
        printf("%s\n", jpsd_build_scm_info);
        printf("%s\n", jpsd_build_extra_cflags);
        printf("\n");
        exit(1);

}

static void jpsd_daemonize(void)
{
	if (fork() != 0)
		exit(0);

	if (setsid() < 0)
		exit(0);

	if (fork() != 0)
		exit(0);

	return;
}

static void jpsd_check(void)
{
	char buffer[4096];
	int len;
	int fd;
	pid_t pid, mypid;
	struct dirent *ent;
	DIR *dir = opendir("/proc");

	mypid = getpid();

	while ((ent = readdir(dir)) != NULL) {
		if (!isdigit(ent->d_name[0]))
			continue;
		pid = atoi(ent->d_name);
		if (pid == mypid)
			continue;
		snprintf(buffer, 4096, "/proc/%d/cmdline", pid);
		if ((fd = open(buffer, O_RDONLY)) < 0) {
			close(fd);
			continue;
		}
		if ((len = read(fd, buffer, 4096)) > 1) {
			buffer[len] = '\0';
			if (strstr(buffer, "jpsd") != 0) {
				printf("\nThere is another jpsd process" \
					" (pid=%d)\n", pid);
				exit(2);
			}
		}
		close(fd);
	}

	closedir(dir);

	return;
}

static void jpsd_mem_init(void)
{
	void *p;

	/* for gcc lazy binding */
	p = nkn_malloc_type(10, mod_diameter_t);
	if (p == NULL) {
		assert(0);
	}
	free(p);
	p = nkn_calloc_type(1, 10, mod_diameter_t);
	if (p == NULL) {
		assert(0);
	}
	p = nkn_realloc_type(p, 20, mod_diameter_t);
	if (p == NULL) {
		assert(0);
	}
	free(p);
	p = nkn_strdup_type("jpsd", mod_diameter_t);
	if (p == NULL) {
		assert(0);
	}
	free(p);

	/* initialize global mempool after gcc lazy binding call */
	nkn_init_pool_mem();

	return;
}

int main(int argc, char **argv)
{
	int ret;
	int daemonize = 1;
	struct rlimit rlim;

        while ((ret = getopt(argc, argv, "vD")) != -1) {
		switch (ret) {
                case 'v':
                        jpsd_version();
                        break;
                case 'D':
                        daemonize = 0;
                        break;
                default:
                        break;
                }
        }

	/* check another jpsd proccess */
	jpsd_check();

	/* daemonize */
	if (daemonize)
		jpsd_daemonize();

	setpriority(PRIO_PROCESS, 0, -1);

	if (getuid() == 0) {
		rlim.rlim_cur = MAX_GNM;
		rlim.rlim_max = MAX_GNM;

		if (setrlimit(RLIMIT_NOFILE, &rlim)) {
			DBG_LOG(MSG, MOD_JPSD, "setrlimit failed");
			exit(-1);
		}
	}

	/* init jpsd mem pool */
	jpsd_mem_init();

	/* start logger thread */
	log_thread_start();
	DBG_LOG(SEVERE, MOD_JPSD, "jpsd initializing");

	/* init shared mem for counters */
	config_and_run_counter_update();

	/* init mgmt thread*/
        jpsd_mgmt_thrd_init();

	if (!update_from_db_done)
		sleep(2);

	NM_init();

	jpsd_timer_start();

	NM_start();

	/* start jpsd-tdf interfaces */
	jpsd_tdf_start();

	DBG_LOG(SEVERE, MOD_JPSD, "jpsd started");

	NM_wait();

	return 0;
}

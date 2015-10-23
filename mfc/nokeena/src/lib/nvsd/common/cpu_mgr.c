#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/queue.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/unistd.h>

#include "nkn_debug.h"
#include "nkn_util.h"

/* Set the CPU affinity for a task */
/* Declared in /usr/include/sched.h */
/* extern int sched_setaffinity (__pid_t __pid, size_t __cpusetsize,
                              __const cpu_set_t *__cpuset) __THROW;
*/


int glob_num_of_processor=0;

void cpumgr_init(void);
int nkn_choose_cpu(int i);

void cpumgr_init(void)
{
	FILE * fp;
	char buf[1024];

	fp=fopen("/proc/cpuinfo", "r");
	if(!fp) return ;
	while(!feof(fp)) {
		if(fgets(buf, 1024, fp)==NULL) break;
		if(strncmp(buf, "processor", 9)==0) glob_num_of_processor++;
	}
	fclose(fp);

	DBG_LOG(MSG, MOD_SYSTEM, "Total %d cpu cores are detected", glob_num_of_processor);
}

/*
 * Return value is the chosen CPU id.
 */
int nkn_choose_cpu(int i)
{
	cpu_set_t mask;
	int len = sizeof(mask);
	pid_t pid;

	assert(glob_num_of_processor > 0);
	i = i % glob_num_of_processor;

	pid = gettid();

	CPU_ZERO(&mask);
	CPU_SET(i, &mask);
	if( sched_setaffinity(pid, len, &mask) < 0 ) {
		assert( 0 );
	}
	return i;
}

#ifdef CPUMGR_DEBUG
/*
 * The following code is used for unit testing purpose.
 * Compilation command is:
 *	gcc -g -DCPUMGR_DEBUG -o cpumgr cpu_mgr.c
 * Command to run is:
 *	./cpumgr
 */

int main(int argc, void * argv[])
{
	int i;

	cpumgr_init();
	nkn_choose_cpu(getpid(), atoi(argv[1]));

	while(1) {
		i=(2163.723*328.29838923)/328787.238/333.3;
	}

	return 1;
}

#endif // CPUMGR_DEBUG

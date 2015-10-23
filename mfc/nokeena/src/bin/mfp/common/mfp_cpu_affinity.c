#include "mfp_cpu_affinity.h"

#include <string.h>
#define __GNU_SOURCE
#include <sched.h>

#ifndef __USE_MISC
extern int syscall(int number, ...); /* Prototype as in the man page */
#endif

/*
extern int sched_setaffinity (__pid_t __pid, size_t __cpusetsize,
		__const cpu_set_t *__cpuset) __THROW;

extern int sched_getaffinity (__pid_t __pid, size_t __cpusetsize,
		cpu_set_t *__cpuset) __THROW;
		*/

uint32_t glob_mfp_processor_count = 0;

void initPhysicalProcessorCount(void) {

	FILE* fp;
	char buf[1024];

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL)
		return;
	uint32_t phy_id[32] = {0};
	uint32_t phy_id_cpu_count[32] = {0};
	int32_t err = 0;
	int64_t id_val = -1;
	while (!feof(fp)) {

		if (fgets(buf, 1024, fp) == NULL)
			break;
		if (strncmp(buf, "physical id", 11) == 0) {
			char* id_val_str = strchr(buf, ':');
			if (id_val_str == NULL) {
				err = -1;
				break;
			}
			char* tmp_buf = NULL;
			id_val = strtol(id_val_str + 1, &tmp_buf, 0);
			if (strcmp(id_val_str, tmp_buf) == 0) {
				err = -2;
				break;
			}
			phy_id[id_val]++;
		} else if (strncmp(buf, "cpu cores", 9) == 0) {
			char* cc_val_str = strchr(buf, ':');
			if (cc_val_str == NULL) {
				err = -3;
				break;
			}
			char* tmp_buf = NULL;
			int64_t cc_val = strtol(cc_val_str + 1, &tmp_buf, 0);
			if (strcmp(cc_val_str, tmp_buf) == 0) {
				err = -4;
				break;
			}
			if (id_val == -1) {
				// "cpu cores" found before "physical id"
				err =-5;
				break;
			}
			if (phy_id_cpu_count[id_val] == 0)
				phy_id_cpu_count[id_val] = cc_val;
			id_val = -1;
		}
	}
	uint32_t i, phy_proc_count = 0, log_proc_count = 0;
	for (i = 0; i < 32; i++) {
		phy_proc_count += phy_id_cpu_count[i];
		log_proc_count += phy_id[i];
	}
	printf("Count of physical processors: %u\n", phy_proc_count);
	printf("Count of logical processors: %u\n", log_proc_count);

	glob_mfp_processor_count = log_proc_count;

	if (glob_mfp_processor_count == 0) {
	    glob_mfp_processor_count = 1;
	}
	fclose(fp);
	return;
}

int32_t chooseProcessor(uint32_t i) {

	cpu_set_t mask;
	int32_t len = sizeof(mask);
	pid_t pid;

	assert(glob_mfp_processor_count > 0);
	i = i % glob_mfp_processor_count;

	pid = syscall(__NR_gettid); 

	CPU_ZERO(&mask);
	CPU_SET(i, &mask);
	if(sched_setaffinity(pid, len, &mask) < 0) {
		assert( 0 );
	}
	return i;
}


//#define UNIT_TEST

#ifdef UNIT_TEST

int main(int argc, char * argv[]) {
	int32_t i;

	initPhysicalProcessorCount();
	for (i = 0; i < glob_mfp_processor_count; i++) { 
		printf("Processor Id allocated : %d\n", chooseProcessor(i));
		pid_t pid;
		pid = syscall(__NR_gettid );
		cpu_set_t mask;
		CPU_ZERO(&mask);
		int32_t len = sizeof(mask);
		sched_getaffinity(pid, len, &mask);
		int32_t j;
		for (j = 0; j < glob_mfp_processor_count; j++)
			if (CPU_ISSET(j, &mask) > 0)
				printf("Processor Id verification: %d\n", j);
	}
	return 1;
}

#endif


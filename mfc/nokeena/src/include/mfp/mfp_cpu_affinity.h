#ifndef MFP_CPU_AFFINITY_H
#define MFP_CPU_AFFINITY_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>
#include <stdint.h>
#include <linux/unistd.h>


#ifdef __cplusplus 
extern "C" {
#endif

extern uint32_t glob_mfp_processor_count;

void initPhysicalProcessorCount(void); 

int32_t chooseProcessor(uint32_t i);

#ifdef __cplusplus
}
#endif

#endif


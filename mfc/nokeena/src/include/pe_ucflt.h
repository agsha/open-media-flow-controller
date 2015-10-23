#ifndef _PE_UCFLT_H
#define _PE_UCFLT_H


#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>

#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"

#define UNUSED_ARGUMENT(x)      (void)x

#define PE_MAX_EVENTS          65535
#define UCFLT_MAGIC          0xCADBCADB
#define UCFLT_MAGIC_DEAD     0xdeadCADB

enum ucflt_commands {
	UCFLT_NONE = 0,
	UCFLT_LOOKUP,
	UCFLT_VERSION,
	UCFLT_START,
	UCFLT_STOP,
};

typedef struct ucflt_req_s
{
	uint32_t magic;
	uint32_t op_code;
	uint32_t lookup_timeout;
	int64_t pe_task_id;
	char url[512];
} ucflt_req_t;

typedef struct ucflt_info_s
{
	int32_t status_code;
	int64_t expiry;
	char category[256];
        int category_count;
} ucflt_info_t;

typedef struct ucflt_resp_s
{
	uint32_t magic;
	int64_t pe_task_id;
	struct ucflt_info_s info;
} ucflt_resp_t;

int ucflt_open(uint32_t init_flags);
void ucflt_lookup(ucflt_req_t *flt_req) ;
int ucflt_close(void);
int ucflt_version(char *ver, int length) ;
#endif /* _PE_UCFLT_H */

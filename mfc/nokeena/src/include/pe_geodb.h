#ifndef _NKN_GEODB_API_H
#define _NKN_GEODB_API_H


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
#define GEODB_MAGIC          0xE0DBE0DB
#define GEODB_MAGIC_DEAD     0xdeadE0DB

enum geodb_commands {
    GEODB_NONE = 0,
    GEODB_LOOKUP,
    GEODB_VERSION,
    GEODB_START,
    GEODB_STOP,
};

typedef struct geo_ip_req_s
{
    uint32_t magic;
    uint32_t op_code;
    uint32_t ip;
    uint32_t lookup_timeout;
    int64_t pe_task_id;
} geo_ip_req_t;

typedef struct geo_info_s
{
    int32_t status_code;
    char continent_code[4];
    char country_code[4];
    char state_code[4];
    char country[64];
    char state[64];
    char city[64];
    char postal_code[16];
    char isp[64];
    float latitude;
    float longitude;
} geo_info_t;

typedef struct geo_ip_s
{
    uint32_t magic;
    uint32_t ip;
    int32_t ttl;
    int64_t pe_task_id;
    struct geo_info_s ginf;
} geo_ip_t;

int geodb_open(uint32_t init_flags);		//initializes the worker threads
int geodb_lookup(geo_ip_req_t *ip_data, geo_ip_t *geo_data);
int geodb_close(void);
int geodb_version(char *ver, int length);
int geodb_edition(void);

#endif /* _NKN_GEODB_API_H */

#ifndef _LF_PROC_UTILS_H
#define _LF_PROC_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lf_common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

#define LF_PROC_MATCH_TYPE_SUBSTR (0)
#define LF_PROC_MATCH_TYPE_EXACT (1)
typedef struct tag_lf_proc_stat_t {
    int32_t pid;
    char pname[16];
}lf_proc_stat_t;

int32_t lf_find_parent_pid_exact_name_match(const char *pname,
					    int32_t *pid); 
int32_t lf_find_child_pid_name_match(int32_t in_pid,
				     const char *pname,
				     int32_t match_type,
				     struct pid_stat_list_t **out);
int32_t lf_get_proc_name(int32_t pid, char *pname);
int32_t lf_get_proc_cpu_time(int32_t pid, uint64_t *jiffies);
int32_t lf_get_thread_cpu_time(int32_t ppid, int32_t tpid,
			       uint64_t *jiffies);
int32_t lf_get_cpu_clk_freq(void);
int32_t lf_get_thread_name(int32_t ppid, int32_t tpid, char *pname);

static inline uint64_t
lf_calc_cpu_time_elapsed(uint64_t *c, uint64_t * p)
{
    uint64_t tot = 0;

    tot = ((c[0] - p[0]) + (c[1] - p[1]) +
	   (c[2] - p[2]) + (c[3] - p[3]));
    
    return tot;
}

static inline void 
lf_save_cpu_time_stat(uint64_t *c, uint64_t *p)
{
    p[0] = c[0];
    p[1] = c[1];
    p[2] = c[2];
    p[3] = c[3];
}

int32_t lf_cleanup_pid_stat_list(struct pid_stat_list_t *head);
#ifdef __cplusplus
}
#endif

#endif //_LF_UTILS_H

#include <string.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <assert.h>
#include <unistd.h>

// for pid query
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>

// nkn defs
#include "nkn_memalloc.h"

// lf includes
#include "lf_err.h"
#include "lf_proc_utils.h"
#include "lf_common_defs.h"

typedef int32_t (*pid_name_match_fnp)(char *str, char *match);
static int32_t lf_pid_name_exact_match(char *s1, char *s2);
static int32_t lf_pid_name_substr_match(char *s1, char *s2);

int32_t
lf_find_parent_pid_exact_name_match(const char *pname,
				    int32_t *pid)
{
    DIR *d = NULL;
    struct dirent *dent = NULL;
    int32_t tmp_pid = 0, err = 0, found = 0;
    char tmp_pname[16];

    assert(pid);
    *pid = -1;

    d = opendir("/proc");
    if (!d) {
	err = -1;
	goto error;
    }

    while (1) {
	dent = readdir(d);
	memset(tmp_pname, 0, 16);
	if (dent) {
	    if ((dent->d_name[0] > '0' &&
		 dent->d_name[0] <= '9')) {
		tmp_pid = strtoul(dent->d_name, NULL, 10);
		lf_get_proc_name(tmp_pid, tmp_pname);
		if (!strcmp(pname, tmp_pname)) {
		    found = 1;
		    break;
		}
	    }
	} else {
	    break;
	}
   }

    if (found) {
	*pid = tmp_pid;
    }

 error:
    if (d) closedir(d);
    return err;
}

static int32_t
lf_pid_name_substr_match(char *s1, char *s2)
{
    char *p;

    p = strstr(s1, s2);
    if (p) 
	return 1;
    else 
	return 0;
}

static int32_t
lf_pid_name_exact_match(char *s1, char *s2)
{
    return !(strcmp(s1, s2));
}

int32_t
lf_find_child_pid_name_match(int32_t parent_pid,
			     const char *pname,
			     int32_t match_type,
			     struct pid_stat_list_t **out)
{
    DIR *d = NULL;
    struct dirent *dent = NULL;
    int32_t tmp_pid = 0, err = 0, found = 0;
    char tmp_pname[16], path[256];
    struct pid_stat_list_t *pid_list = NULL;
    pid_name_match_fnp match_fn;

    assert(out);
    if (match_type == LF_PROC_MATCH_TYPE_SUBSTR) {
	match_fn = lf_pid_name_substr_match;
    } else {
	match_fn = lf_pid_name_exact_match;
    }

    if ((*out) == NULL) {
	pid_list = (struct pid_stat_list_t *)
	    nkn_calloc_type(1, sizeof(struct pid_stat_list_t), 100);
	if (!pid_list) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	TAILQ_INIT(pid_list);
    } else {
	pid_list = *out;
    }
	
    snprintf(path, 256, "/proc/%d/task/", parent_pid);
    d = opendir(path);
    if (!d) {
	err = -1;
	goto error;
    }

    while (1) {
	dent = readdir(d);
	if (dent) {
	    if ((dent->d_name[0] > '0' &&
		 dent->d_name[0] <= '9')) {
		tmp_pid = strtoul(dent->d_name, NULL, 10);
		if (tmp_pid == 1913) {
		    int uu = 0;
		    uu++;
		}
		memset(tmp_pname, 0, 16);
		lf_get_thread_name(parent_pid, tmp_pid, tmp_pname);
		if (match_fn(tmp_pname, (char *)pname)) {
		    pid_stat_list_elm_t *pid_item = NULL;
		    pid_item = (pid_stat_list_elm_t *)
			nkn_calloc_type(1, sizeof(pid_stat_list_elm_t),
					100);
		    if (!pid_item) {
			err = -E_LF_NO_MEM;
			goto error;
		    }
		    pid_item->st.pid = tmp_pid;
		    pid_item->st.first_flag = 1;
		    TAILQ_INSERT_TAIL(pid_list, pid_item,
				      entries);
		    found = 1;
		}
	    }
	} else {
	    break;
	}
   }

    if (found) {
	*out = pid_list;
    }

    if (d) closedir(d);
    return err;
 error:
    if (d) closedir(d);
    if (pid_list) {
	lf_cleanup_pid_stat_list(pid_list);
    }
    return err;
}

int32_t
lf_get_proc_name(int32_t pid, char *pname)
{
    
    FILE *f = NULL;
    int32_t err = 0;
    const size_t buf_size = 1024;
    size_t read_size = 0, pname_len = 16;
    char *p1 = NULL, *p2 = NULL, buf[buf_size], path[256];
		
    assert(pname);
    
    p2 = NULL;
    buf[0] = '\0';
    p1 = &buf[0];

    snprintf(path, 256, "/proc/%d/stat", pid);

    f = fopen(path, "r");
    if (!f) {
	err = -1;
	goto error;
    }
    read_size = fread(buf, 1, buf_size, f);
    if (!read_size || read_size > buf_size) {
	err = -1;
	goto error;
    }
    p1 = strchr(buf, '(');
    if (!p1) {
	err = -1;
	goto error;
    }
	
    p2 = strchr(p1 + 1, ')');
    if (!p2) {
	err = -1;
	goto error;
    }

    pname_len = p2 - (p1 + 1);
    memcpy(pname, p1 + 1, pname_len);
     	
    fclose(f);
    f = NULL;

 error:
    if (f) fclose(f);
    return err;
}

int32_t
lf_get_thread_name(int32_t ppid, int32_t tpid, char *pname)
{
    
    FILE *f = NULL;
    int32_t err = 0;
    const size_t buf_size = 1024;
    size_t read_size = 0, pname_len = 16;
    char *p1, *p2, buf[buf_size], path[256];
		
    assert(pname);
    
    p2 = NULL;
    buf[0] = '\0';
    p1 = &buf[0];

    snprintf(path, 256, "/proc/%d/task/%d/stat", 
	     ppid, tpid);

    f = fopen(path, "r");
    if (!f) {
	err = -1;
	goto error;
    }
    read_size = fread(buf, 1, buf_size, f);
    if (!read_size || read_size > buf_size) {
	err = -1;
	goto error;
    }

    p1 = strchr(buf, '(');
    if (!p1) {
	err = -1;
	goto error;
    }

    p2 = strchr(p1 + 1, ')');
    if (!p2) {
	err = -1;
	goto error;
    }

    pname_len = p2 - (p1 + 1);
    memcpy(pname, p1 + 1, pname_len);
     	
    fclose(f);
    f = NULL;

 error:
    if (f) fclose(f);
    return err;
}


int32_t lf_get_proc_cpu_time(int32_t pid,
			     uint64_t *jiffies)
{
    const char *proc_stat_fmt = "%*c "
	"%*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu "
	"%Lu %Lu %Lu %Lu "  /* cpu times */
	"%*ld %*ld %*d %*ld %*Lu %*lu %*ld %*lu %*u %*u %*u %*u %*u "
	"%*s %*s %*s %*s %*u %*lu %*lu %*d %*d %*lu %*lu";
    FILE *f = NULL;
    int32_t err = 0, buf_size = 1024;
    size_t read_size = 0;
    char path[256], buf[1024], *p1 = NULL, *p2 = NULL;
    uint64_t *cpu_times = jiffies;

    buf[0] = '\0';
    snprintf(path, 256, "/proc/%d/stat", pid);
    f = fopen(path, "r");
    if (!f) {
	err = -1;
	goto error;
	
    }
    read_size = fread(buf, 1, buf_size, f);
    if (!read_size || read_size > (size_t)buf_size) {
	err = -1;
	goto error;
    }

    /* skip the proc name */
    p1 = strchr(buf, '(');
    if (!p1) {
	err = -1;
	goto error;
    }

    p2 = strchr(p1 + 1, ')');
    if (!p2) {
	err = -1;
	goto error;
    }

    sscanf(p2 + 2, proc_stat_fmt, &cpu_times[0], &cpu_times[1],
	   &cpu_times[2], &cpu_times[3]);

 error:
    if (f) fclose(f);
    return err;

}

int32_t lf_get_thread_cpu_time(int32_t ppid,
			       int32_t tpid,
			       uint64_t *jiffies)
{
    const char *proc_stat_fmt = "%*c "
	"%*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu "
	"%Lu %Lu %Lu %Lu "  /* cpu times */
	"%*ld %*ld %*d %*ld %*Lu %*lu %*ld %*lu %*u %*u %*u %*u %*u "
	"%*s %*s %*s %*s %*u %*lu %*lu %*d %*d %*lu %*lu";
    FILE *f = NULL;
    int32_t err = 0, buf_size = 1024;
    size_t read_size = 0;
    char path[256], buf[1024], *p1 = NULL, *p2 =  NULL;
    uint64_t *cpu_times = jiffies;

    buf[0] = '\0';
    snprintf(path, 256, "/proc/%d/task/%d/stat", ppid, tpid);
    f = fopen(path, "r");
    if (!f) {
	err = -1;
	goto error;
	
    }
    read_size = fread(buf, 1, buf_size, f);
    if (!read_size || read_size > (size_t)buf_size) {
	err = -1;
	goto error;
    }

    /* skip the proc name */
    p1 = strchr(buf, '(');
    if (!p1) {
	err = -1;
	goto error;
    }
    p2 = strchr(p1 + 1, ')');
    if (!p2) {
	err = -1;
	goto error;
    }
   
    sscanf(p2 + 2, proc_stat_fmt, &cpu_times[0], &cpu_times[1],
	   &cpu_times[2], &cpu_times[3]);

 error:
    if (f) fclose(f);
    return err;

}

int32_t
lf_get_cpu_clk_freq(void)
{
    uint64_t *p = (uint64_t*)environ;
    while(*p) {
	p++;
    }
    
    p++;
    while(*p) {
	if (p[0] == 17) {
	    return p[1];
	}
	p += 2;
    }

    return 0;	
}

int32_t
lf_cleanup_pid_stat_list(struct pid_stat_list_t *head)
{
    if(head) {
	pid_stat_list_elm_t *elm = NULL;
	while( (elm = TAILQ_FIRST(head)) ){
	    TAILQ_REMOVE(head, elm, entries);
	    if (elm) {
		free(elm);
	    }
	}
    }

    return 0;
}

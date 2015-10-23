#ifndef NKN_DISKMGR_API_H
#define NKN_DISKMGR_API_H
#include <assert.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_diskmgr_common_api.h"

#define NKN_DM_BLK_SIZE_KB	1024			/* In Kilobytes */
#define NKN_DISK_BLK_SZ		(200 * 1024)
#define NKN_NUM_BLK_PER_SEGMENT (NKN_DM_BLK_SIZE_KB / NKN_DISK_BLK_SZ)

#define NKN_MAX_CACHE_SIZE_KB	(250 * 1024 * 1024)	/* In Kilobytes */
// For testing only, setting to 1MB
//#define NKN_MAX_CACHE_SIZE_KB (1024) /* In Kilobytes*/

#define NKN_MAX_SEG_PER_CACHE (NKN_MAX_CACHE_SIZE_KB / NKN_DM_BLK_SIZE_KB)

struct DM_offset_length {
	unsigned long long offset;
	unsigned long long length;
};

struct DM_clip_location {
	char *storage_file;
	struct DM_offset_length offlen;
	unsigned long long   *off_in_storage_file;
	int     segmentnum;     
	char *uri_dir; 
	char *uri_file; 
#ifdef NKN_DM_RR
	int s_input_cur_write_dir_idx;
#endif
};

typedef struct DM_video_stat_obj {
    int     magic;
    char*   stat_objid;
    int     reads_from_disk;
    int     reads_from_memory;
    unsigned long long LRU; 
    int     views; 
    int     total_requests; 
    int     cleanup_ref_count;
    int     eviction_count;
} DM_video_stat_obj_t;

typedef struct DM_obj {
    int			    magic;
    char		    *objid;
    char		    *filename;  
    int			    segmentnum;
    unsigned int	    lock;
    int			    read_count;
    int			    in_use;
    int			    write_count;
    char		    *content;
    unsigned char	    content_available;
    unsigned long long	    uri_size;
    struct DM_offset_length off_len;
    nkn_task_id_t	    disk_taskid;
    int			    _mgr_retval;
    DM_video_stat_obj_t     *pstat_obj;
} DM_video_obj_t;


enum online_flag {
	dm_offline = 1,
	dm_online = 2,
};

enum error_flag {
	dm_error_continue = 1,
	dm_error_stop = 2,
};

enum {
	NKN_DM_OK = 0,
	NKN_DM_GENERAL_ERROR = 2,
	NKN_DM_DISK_SPACE_EXCEEDED = 3, 
	NKN_DM_WRITE_ERROR = 4,
	NKN_DM_READ_ERROR = 5,
};

int DM_put(struct MM_put_data *a_info, video_types_t video_type);
void *DM_lookup(char *uri);
char *DM_get_uri_file_path(char *uri);
void DM_print_stats(void);
void exp_print_clip_struct(struct DM_clip_location *clip, FILE *file);
void dm_api_dump_counters(void);
void dm2_evict_dump_list(const char* disk_name, int bucket);
void DM_cleanup(void);
#endif	/* NKN_DISKMGR_API_H */

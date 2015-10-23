#ifndef __LOGD_WORKER_H__
#define __LOGD_WORKER_H__

#include <cprops/hashtable.h>

#define MAX_FILE_SZ                 1024
#define CODE_MOVE_LOG               1000

typedef struct uploadInfo{
    char *profile;
    char *fullfilename;
    char *filename;
    char *remote_url;
    char *password;
    int req_id;
    int logtype;
} uploadInfo ;
cp_hashtable *ht_uploadInfo ;
typedef int (*lwe_wrkq_work)(void *work_arg);
extern tbool lgd_exiting;

struct lgd_work_elem {
    /* Private */
    uint32 lwe_id;

    /* Public */
    lwe_wrkq_work  lwe_wrk;
    void *lwe_wrk_arg;

    TAILQ_ENTRY(lgd_work_elem) lwe_entries;
};

extern int lgd_lw_work_new(struct lgd_work_elem **p);
extern int lgd_lw_work_add(struct lgd_work_elem *p);

extern void lgd_file_deflate(struct uploadInfo *p_uploadInfo);
extern int lgd_file_upload(uploadInfo *uploadINFO);
extern int mv_file_to_logexport(struct uploadInfo *p_uploadInfo);
extern int post_msg_to_gzip(struct uploadInfo *p_uploadInfo);
extern int post_msg_to_upload(struct uploadInfo *p_uploadInfo);
extern int post_msg_to_mv_to_logexport(struct uploadInfo *p_uploadInfo);
// Constructor and destructor functions for key and value of hashtable
extern void *cp_copy_key(void *key);
extern void *cp_copy_value(void *value);
extern void cp_free_key(void *key);
extern void cp_free_value(void *key);
#endif /* __LOGD_WORKER_H__ */

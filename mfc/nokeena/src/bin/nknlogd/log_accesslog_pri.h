#ifndef __NKN_ACCESSLOG__
#define __NKN_ACCESSLOG__

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>

#define HASH_UNITTEST
#include "nkn_hash.h"


//#ifdef INCLUDE_GCL
#include "signal_utils.h"
#include "libevent_wrapper.h"


extern lew_context *lew;
extern tbool exiting;

//#endif // INCLUDE_GCL

enum fmt_type {
	//FORMAT_RFC_931_AUTHENTICATION_SERVER,

	FORMAT_BYTES_OUT_NO_HEADER,
	FORMAT_CACHE_HIT,
	FORMAT_ORIGIN_SIZE,
	FORMAT_CACHE_HISTORY,
	FORMAT_FILENAME,
	FORMAT_REMOTE_HOST,
	FORMAT_REQUEST_HEADER,
	//FORMAT_REMOTE_IDENT,
	FORMAT_REQUEST_METHOD,
	FORMAT_RESPONSE_HEADER,
	FORMAT_HOTNESS,
	FORMAT_QUERY_STRING,
	FORMAT_REQUEST_LINE,
	FORMAT_STATUS,
	FORMAT_TIMESTAMP,
	FORMAT_REMOTE_USER,
	FORMAT_SERVER_NAME,
	FORMAT_PEER_STATUS,
	FORMAT_PEER_HOST,
	FORMAT_STATUS_SUBCODE,
	FORMAT_CONNECTION_TYPE,

	FORMAT_COOKIE,
	FORMAT_TIME_USED_MS,
	FORMAT_TIME_USED_SEC,
	FORMAT_REQUEST_PROTOCOL,
	FORMAT_BYTES_IN,
	FORMAT_BYTES_OUT,
	FORMAT_URL,
	FORMAT_HTTP_HOST,
	FORMAT_CONNECTION_STATUS,
	FORMAT_REMOTE_ADDR,
	FORMAT_LOCAL_ADDR,
	FORMAT_SERVER_PORT,
	FORMAT_NAMESPACE_NAME,
	FORMAT_REQUEST_IN_TIME,
	FORMAT_FBYTE_OUT_TIME,
	FORMAT_LBYTE_OUT_TIME,
	FORMAT_LATENCY,
	FORMAT_DATA_LENGTH_MSEC,
	FORMAT_REVALIDATE_CACHE,
	FORMAT_ORIGIN_SERVER_NAME,
	FORMAT_COMPRESSION_STATUS,
	FORMAT_EXPIRED_DEL_STATUS,

	FORMAT_PERCENT,
	FORMAT_UNSET,
	FORMAT_STRING	// Special format, defined by configure
};


typedef struct {
	char key;
	enum fmt_type type;
} format_mapping;

typedef struct format_field {
	int field;
	char string[64];
	char * pdata;
} format_field_t;

extern format_field_t * ff_ptr;
extern int ff_used;
extern int ff_size;
extern int glob_run_under_pm;

extern const char * program_name;

extern int 
log_basic(const char *__restrict fmt, ...) 
    __attribute__ ((__format__ (printf, 1, 2)));

extern int 
log_debug(const char *__restrict fmt, ...) 
    __attribute__ ((__format__ (printf, 1, 2)));

#if 0
#define LOCK_() do { while(pthread_mutex_trylock(&acc_lock) == EBUSY) {al_log_debug("Acquiring lock....."); sleep(1);} al_log_debug("Acquired lock....."); } while(0)

#define UNLOCK_() do { al_log_debug("Releasing lock....."); pthread_mutex_unlock(&acc_lock); } while(0)
#endif // 0


//// From al_main.c
extern void catcher(int sig);


//// From al_mgmt.c
extern int init(void);
extern int deinit(void);
extern int main_loop(void);
extern int mgmt_initiate_exit(void);
extern int mgmt_thread_init(void);
extern int mgmt_init(int);





//// From al_network.c
extern void al_network_init(void);
extern void al_close_socket(void);
extern void make_socket_to_svr(void);
extern int al_apply_config(void);

extern int al_serv_init(void);


//// From al_file.c
extern void log_read_fileid(void);
extern void log_save_fileid(void);
extern void http_accesslog_init(void);
extern void http_accesslog_exit(void);


//// From al_cfg.c

//extern int read_nkn_cfg(char * configfile);
//extern int nkn_check_cfg(void);
extern int al_cfg_query(void);
extern int nvsd_system_cfg_query(void);
//extern void print_cfg_info(void);
//extern void nkn_free_cfg(void);


extern pthread_mutex_t server_lock;
extern pthread_cond_t config_cond;

extern int al_config_done;


typedef struct {

    NHashTable	*ht;
    pthread_rwlock_t rw_lock;

} nknlog_hash_t;

static inline nknlog_hash_t *nknlog_hash_table_new(void) {
    nknlog_hash_t *p = calloc(1, sizeof(nknlog_hash_t));
    p->ht = nhash_table_new(n_str_hash, n_str_equal, 0);
    pthread_rwlock_init(&p->rw_lock, NULL);
    return p;
}

static inline void __acquire_ht_rd_lock(nknlog_hash_t *ht) {
    pthread_rwlock_rdlock(&ht->rw_lock);
}

static inline void __release_ht_rd_lock(nknlog_hash_t *ht) {
    pthread_rwlock_unlock(&ht->rw_lock);
}

static inline void __acquire_ht_wr_lock(nknlog_hash_t *ht) {
    pthread_rwlock_wrlock(&ht->rw_lock);
}

static inline void __release_ht_wr_lock(nknlog_hash_t *ht) {
    pthread_rwlock_unlock(&ht->rw_lock);
}

static inline int nknlog_hash_table_remove(nknlog_hash_t *ht,
	uint64_t keyhash, void *key)
{
    int ret;
    __acquire_ht_wr_lock(ht);
    ret = nhash_remove(ht->ht, keyhash, key);
    __release_ht_wr_lock(ht);
    return ret;
}

static inline void nknlog_hash_table_insert(nknlog_hash_t *ht,
	uint64_t keyhash, void *key, void *value)
{
    __acquire_ht_wr_lock(ht);
    nhash_insert(ht->ht, keyhash, key, value);
    __release_ht_wr_lock(ht);
}

static inline void *nknlog_hash_table_lookup(nknlog_hash_t *ht,
	uint64_t keyhash, void *key)
{
    void *p;
    __acquire_ht_rd_lock(ht);
    p = nhash_lookup(ht->ht, keyhash, key);
    __release_ht_rd_lock(ht);
    return p;
}



enum fmt_type_flag {
    fmt_type_unknown = 0,
    fmt_type_basic,
    fmt_type_name,
};

struct fmt_map {
    const char    *s;
    const char    *w3c_name;
    enum fmt_type_flag flag;
};


static struct fmt_map fmtmap[] = {
    {"9", "c-auth", fmt_type_basic},
    {"b", "sc-bytes-content", fmt_type_basic},
    {"c", "x-cache-hit", fmt_type_basic},
    {"d", "date", fmt_type_basic},
    {"f", "cs-uri-stem", fmt_type_basic},
    {"h", "c-ip", fmt_type_basic},
    {"i", "cs(%%s)", fmt_type_name},
    {"m", "cs-method", fmt_type_basic},
    {"o", "sc(%%s)", fmt_type_name},
    {"q", "cs-uri-query", fmt_type_basic},
    {"r", "cs-request", fmt_type_basic},
    {"s", "sc-status", fmt_type_basic},
    {"t", "time", fmt_type_basic},
    {"u", "cs-user", fmt_type_basic},
    {"v", "x-server", fmt_type_basic},
    {"y", "sc-substatus", fmt_type_basic},

    {"A", "x-request-in-time", fmt_type_basic},
    {"B", "x-first-byte-out", fmt_type_basic},
    {"C", "cs(Cookie)", fmt_type_basic},
    {"D", "time-taken", fmt_type_basic},
    {"E", "x-time-used", fmt_type_basic},
    {"F", "x-last-byte-out", fmt_type_basic},
    {"H", "cs-proto", fmt_type_basic},
    {"I", "cs-bytes", fmt_type_basic},
    {"L", "x-latency", fmt_type_basic},
    {"M", "x-unknown", fmt_type_basic},
    {"N", "x-namespace", fmt_type_basic},
    {"O", "sc-bytes", fmt_type_basic},
    {"R", "x-revalidate-cache", fmt_type_basic},
    {"S", "s-origin-ips", fmt_type_basic},
    {"U", "cs-uri", fmt_type_basic},
    {"X", "c-ip", fmt_type_basic},
    {"Y", "s-ip", fmt_type_basic},
    {"Z", "s-port", fmt_type_basic}
};

#endif // __NKN_ACCESSLOG__

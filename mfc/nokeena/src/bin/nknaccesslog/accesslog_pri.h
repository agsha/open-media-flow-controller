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

//#ifdef INCLUDE_GCL
#include "signal_utils.h"
#include "libevent_wrapper.h"

#define MAX_AL_FORMAT_LENGTH	256

extern lew_context *al_lew;
//extern md_client_context *al_mcc;
extern tbool al_exiting;

//#endif // INCLUDE_GCL

typedef struct {
	char key;
	enum {
		FORMAT_RFC_931_AUTHENTICATION_SERVER,

		FORMAT_BYTES_OUT_NO_HEADER,
		FORMAT_CACHE_HIT,
		FORMAT_ENV,
		FORMAT_FILENAME,
		FORMAT_REMOTE_HOST,
		FORMAT_HEADER,
		FORMAT_REMOTE_IDENT,
		FORMAT_REQUEST_METHOD,
		FORMAT_RESPONSE_HEADER,
		FORMAT_QUERY_STRING,
		FORMAT_REQUEST_LINE,
		FORMAT_STATUS,
		FORMAT_TIMESTAMP,
		FORMAT_REMOTE_USER,
		FORMAT_SERVER_NAME,
		FORMAT_PEER_STATUS,
		FORMAT_PEER_HOST,
		FORMAT_STATUS_SUBCODE,

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

		FORMAT_PERCENT,
		FORMAT_UNSET,
		FORMAT_STRING	// Special format, defined by configure
	} type;
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

extern const char *accesslog_program_name;

extern int 
al_log_basic(const char *__restrict fmt, ...) 
    __attribute__ ((__format__ (printf, 1, 2)));

extern int 
al_log_debug(const char *__restrict fmt, ...) 
    __attribute__ ((__format__ (printf, 1, 2)));

#define LOCK_() do { while(pthread_mutex_trylock(&acc_lock) == EBUSY) {al_log_debug("Acquiring lock....."); sleep(1);} al_log_debug("Acquired lock....."); } while(0)

#define UNLOCK_() do { al_log_debug("Releasing lock....."); pthread_mutex_unlock(&acc_lock); } while(0)


//// From al_main.c
extern void catcher(int sig);


//// From al_mgmt.c
extern int al_init(void);
extern int al_deinit(void);
extern int al_main_loop(void);
extern int al_mgmt_initiate_exit(void);
extern int al_log_upload(const char *this, const char *that);
extern int al_mgmt_thread_init(void);
extern int al_mgmt_init(void);





//// From al_network.c
extern void al_network_init(void);
extern void al_close_socket(void);
extern void make_socket_to_svr(void);
extern int al_apply_config(void);

extern int al_serv_init(void);


//// From al_file.c
extern void nkn_read_fileid(void);
extern void nkn_save_fileid(void);
extern void http_accesslog_init(void);
extern void http_accesslog_exit(void);


//// From al_cfg.c

extern int read_nkn_cfg(char * configfile);
extern int nkn_check_cfg(void);
extern int al_cfg_query(void);
extern void print_cfg_info(void);
extern void nkn_free_cfg(void);


extern pthread_mutex_t server_lock;
extern pthread_cond_t config_cond;

extern int al_config_done;


#endif // __NKN_ACCESSLOG__

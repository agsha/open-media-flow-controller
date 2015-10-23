#ifndef __NKNLOGD__H
#define __NKNLOGD__H

#include <stdint.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include "nknlog_pub.h"
//#include "log_accesslog_pri.h"

#define UNUSED_ARGUMENT(x) (void)x
#define LOGD_PORT		7958

#ifndef FALSE
#define FALSE 	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

struct channel;
/*
 * structure used to describe log file attribute.
 * TYPE_ACCESSLOG: a binary protocol.
 * All other three types are ascii protocol. They are treated in the same way.
 */

/*! forward reference */
struct log_network_mgr;
#define IOBUF_SIZE      (32 * 1024) //(4*1024*1024)

#define TOT_AL_PROFILE		32
#define MAX_RESP_CODE_FILTER	(32)


#define LOGD_FLAG_NEW_PROFILE	    1 << 0
#define LOGD_FLAG_PROFILE_READ	    1 << 1

#define SIZE_OF_LOGEXPORTPATH_FILE  512
#define SIZE_OF_TIMEBUFFER	    40

/*
 * Array of (TYPE_MAXLOG+1) struct  to configure the file names aligned to the
 * rotation interval.
 * This structure includes the list of variables to maintain
 * between the function invocation.
 * Each index corresponds to the TYPE_XXXX of the log.
 * Say for eg index 1 correxponds to TYPE_ACCESSLOG.
 * NOTE:
 * This array  starts from 1 and not 0 inorder to correspond to TYPE_XXXlog
 */
typedef struct align_interval_t {
    int cur_rt_interval ;   // current rotation interval
    int prev_rt_interval;   // previus rotation interval
    int cal_rt_interval ;   // calculation - to rotate or not
    int mod_val ;           //  modulo value -ref for start time
    int file_start_min ;    // file start min based on the calculation made
    int file_end_min ;      // same as above
    int cur_min ;           // current minute
    int minute_interval;    // ref to calculate file_start_min
    int use_file_time ;     // use the file_start_min and file_end_min
    int prev_rt_interval_ref;// PR 782719
} aln_interval;


typedef struct logd_file_t
{
	int channelid;
	int type;
	unsigned int active;
        int replicate_syslog;
	LIST_ENTRY(logd_file_t) entries;
	pthread_mutex_t fp_lock; // to protect fp

	pthread_mutex_t lock; // to protect config

	char *file_id_name; /* Used by accessslog per ns to maintain current fileid */
        char * filename;
        FILE * fp;
	char *io_buf;
        uint64_t cur_filesize;
        uint64_t max_filesize;
	int log_rotation; //0 = no, 1 = yes, default no
        int cur_fileid;
        int max_fileid;
	uint64_t st_time; //start time of the log file
	int rt_interval;  //roration interval in hours
	char hour[3];
	char * fullfilename;
	int on_the_hour; //1 if set to "yes", 0 otherwie, default 0
	int al_analyzer_tool_enable;
        int version_number; //To be used for accesslog and crawllog for now
        uint64_t version_etime; //end time used to find out whether we need to increase version number  

	/* Accesslog */
	format_field_t *ff_ptr;
	int ff_used;
	int ff_size;
	char *w3c_fmt;

	/* logging restriction for accesslog */
	int log_restriction; 	//yes or no, default no
	int acclog_rotation; 	//yes or no, default yes
	int errlog_rotation; 	//yes or no, default yes
	int cachelog_rotation; 	//yes or no, default yes
	int streamlog_rotation; 	//yes or no, default yes
	uint32_t r_size;	//in k-bytes
	int32_t *r_code;	//resp code 200 206
	int al_rcode_ct;	//default 0
	/* Streamlog */
	format_field_t *sl_ptr;
        int sl_used;
        int sl_size;

	/* configuration */
	char * status;
	char * cfgsyslog;
	char * format;	// Accesslog Format
	int show_format; // 1, if set to "yes", 0 otherwise, show accesslog format in the log file
	char * modstr;
	int level;
	char * cfglog_rotation; //no = 0, yes = 1, default no
	uint64_t mod;
	struct log_network_mgr * pnm;
	char * remote_url;
	char * remote_pass;
	char *profile;
	void *data; // used in accesslogger to remember reference to the channel
	int flags;
	/*Align file to the rotation interval */
	aln_interval  * pai;
	char file_start_time[SIZE_OF_TIMEBUFFER];
	char file_end_time[SIZE_OF_TIMEBUFFER];
	void *channel[8];
} logd_file;


LIST_HEAD(log_file_queue, logd_file_t);
extern struct log_file_queue g_lf_head;
extern void log_file_init(logd_file * plf);
extern void log_file_exit(logd_file * plf);
extern void log_write(logd_file * plf, char * p, int len);
extern int nkn_check_cfg(void);
extern void log_write_report_err(int ret, logd_file *plf);

/*
 * Network Manager table is the global table for storing socket information.
 * This table is indexed by socket id.
 * Every operation on any table entry should get this entries' mutex first.
 */

typedef int (* LOG_func)(struct log_network_mgr *data);
typedef int (* LOG_func_timeout)(struct log_network_mgr *data, double timeout);

#define LOGF_IN_USE	0x0000000000000001

#define MAX_FDS 	128

typedef struct log_network_mgr {
	uint64_t flag;
	int32_t  fd;
	int32_t  closed;

	/* Timer */
	struct timeval         last_active;    // Last active time

	LIST_ENTRY(log_network_mgr) entries;

        /* Mutex to pretec this epoll group */
        pthread_mutex_t network_manager_log_lock;		// Mutex to protect this structure only.

	

	/* callback functions */
	void *private_data;		// caller private data
	LOG_func f_epollin;
	LOG_func f_epollout;
	LOG_func f_epollerr;
	LOG_func f_epollhup;
	LOG_func_timeout f_timeout;

} log_network_mgr_t;

#define LOG_SET_FLAG(x, f)	(x)->flag |= (f);
#define LOG_UNSET_FLAG(x, f)	(x)->flag &= ~(f);
#define LOG_CHECK_FLAG(x, f)	(((x)->flag & (f)) == f)

#define LOGD_NW_MGR_MAGIC_ALIVE 0xabcdefef



LIST_HEAD(log_nm_queue, log_network_mgr);
extern struct log_nm_queue g_nm_to_free_queue;

/* 
 * socket is registered by each application module.
 * register_LOG_socket() inside handles to acquire/release mutex.
 * multiple application modules can call in the same time.
 */
int register_log_socket(int fd,
        void * private_data,
        LOG_func f_epollin,
        LOG_func f_epollout,
        LOG_func f_epollerr,
        LOG_func f_epollhup,
        LOG_func_timeout f_timeout,
	log_network_mgr_t **pnm);

/* 
 * socket is unregistered by each application module.
 * unregister_LOG_socket()/LOG_close_socket() inside handle to acquire/release mutex.
 * multiple application modules can call in the same time.
 * The difference between these two functions are close socket only.
 * LOG_close_socket() closes the socket.
 */
int unregister_log_socket(log_network_mgr_t *pnm);

/*
 * Before calling any following functions, caller should get the gnm.mutex locker first
 */
int log_set_socket_nonblock(int fd);
int log_add_event_epollin(log_network_mgr_t *pnm);
int log_add_event_epollout(log_network_mgr_t *pnm);
int log_del_event_epoll(int fd);
int log_del_event_epoll_inout(log_network_mgr_t *pnm);


/*
 * Structure used by private_data in the log_network_mgr_t.
 * This structure for handling recv data.
 */
#define MAX_BUFSIZE     (8092)
#define LOCAL_MAX_BUFSIZE     (4 * MAX_BUFSIZE)

/*
 * PR803637
 * Allocate more buffer for other logs approximately 1MB
 * As the inflow of data increases we might end up in losing them
*/
#define MAX_OTHERLOG_BUFSIZE (1048000)
struct log_recv_t {
        LIST_ENTRY(log_recv_t) entries;
        int fd;
        char buf[MAX_OTHERLOG_BUFSIZE];
        uint32_t offsetlen;
        uint32_t totlen;
} ;
//LIST_HEAD(log_recv_queue, log_recv_t);
//extern struct log_recv_queue g_log_recv_head;
void unregister_free_close_sock ( log_network_mgr_t *pnm);
void unregister_async_free_close_sock ( log_network_mgr_t *pnm);


// function to add to be freed network manager log to the free list
void log_add_nm_to_freelist_queue(log_network_mgr_t *pnm);

/*
 * Mutex is managed within these functions.
 * Caller has no need to acquire the mutex.
 */
//void log_init(void);
void log_timer(void);
void log_main(void);
void print_hex(char * name, unsigned char * buf, int size);
void create_mgmt_thread(int thread_arg);

void crawl_read_version(logd_file *plf);
void crawl_write_version(logd_file *plf);


int al_analyzer_svr_init(log_network_mgr_t **listener_pnm);

int align_to_rotation_interval( logd_file *plf, time_t now);

void donotreachme(const char *func_name);

#define safe_close(f) \
    if ((f) > 0) { \
	close((f)); \
	(f) = 0;    \
    }
#endif // __NKNLOGD__H


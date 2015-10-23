#ifndef _NKN_CSE_H_
#define _NKN_CSE_H_

#include <sys/types.h>
#include <inttypes.h>
#include <sys/queue.h>
#include "nkn_ccp_client.h"
#include "nkn_defs.h"

#define MAX_CRAWLERS 64
#define MAX_CRAWLER_NAME_SIZE 64
#define MAX_INPROGRESS_CRAWLER_CNT 25
#define MAX_ACCEPT_EXTN_STR_SIZE 20
#define MAX_ACCEPT_EXTNS 10
#define MAX_EXTN_ACTIONS 10
#define MAX_CRAWLER_URI_SIZE 800
#define CRAWLER_CLIENT_DOMAIN_SIZE NKN_MAX_FQDN_LENGTH
#define TYPE_STR_LEN	16
#define SRC_STR_LEN	16

#define MAX_EXTN_ACTIONS 10
#define TYPE_STR_LEN   16
#define SRC_STR_LEN    16

#define OUTPUT_PATH "/nkn/crawler"
#define SCRIPT_PATH "/opt/nkn/bin";
#define SCRIPT_NAME "generate_change_lists.pl";

#define ADDLISTASX_FILENAME "addlistasx.xml"
#define DELLISTASX_FILENAME "masterdellistasx.xml"
#define ADDLIST_FILENAME "addlist.xml"
#define DELLIST_FILENAME "masterdellist.xml"

typedef enum {
    INACTIVE,
    ACTIVE,
} crawler_config_status_t;

typedef enum {
    NOT_SCHEDULED,
    POSTED_TIMER_QUEUE,
    CRAWL_IN_PROGRESS,
    CRAWL_CTXT_STOPPED,
    CRAWL_CTXT_DELETED,
} crawling_status_t;

typedef enum {
    CSE_WGET_TRIGGERED = 1,
    CSE_WGET_TERMINATED,
    CSE_WGET_CLEAN_TERMINATED,
    CSE_WGET_NOT_CLEAN_TERMINATED,
} wget_status_t;

typedef struct auto_generate_t {
    char source_type[SRC_STR_LEN];
    char dest_type[TYPE_STR_LEN];
    uint64_t expiry;
}auto_generate_t;

typedef struct extn_action_struct{
#define ACTION_AUTO_GENERATE 1
    int action_type;
    union {
	auto_generate_t auto_gen;
    }action;
}extn_action_struct_t;

typedef struct accept_extn_struct{
    char extn_name[MAX_ACCEPT_EXTN_STR_SIZE];
    uint8_t skip_preload:1;
    int extn_len;
}accept_extn_struct_t;

typedef
 struct crawler_context_config {
    /*
     * crawler name
     */
    char name[MAX_CRAWLER_NAME_SIZE];

    /*
     * Schedule variables
     */
    uint64_t start_time, stop_time;

    uint32_t refresh_interval;

    /*
     * crawl size limit per crawl operation
     */
    uint64_t crawl_size_limit;

    /*
     * accept_extn list
     */
     extn_action_struct_t extn_action_list[MAX_EXTN_ACTIONS];
     accept_extn_struct_t accept_extn_list[MAX_ACCEPT_EXTNS];

     /*
      * actions for this crawler context.
      */
     uint8_t link_depth;
     /*
      * base url of the origin server
      */
     char url[MAX_CRAWLER_URI_SIZE];

     /*
      * crawler instance status
      */
     crawler_config_status_t status;
     char client_domain[CRAWLER_CLIENT_DOMAIN_SIZE];
#define EXP_TYPE_REFRESH_INTERVAL 0
#define EXP_TYPE_ORIGIN_RESPONSE 1
     int expiry_mechanism_type;
     int xdomain_fetch;
} crawler_context_config_t;


typedef struct cse_crawl_stats {
    uint64_t num_total_crawls;
    uint64_t num_failures;
    uint64_t num_schedule_misses;
    uint64_t num_wget_kills;
    uint64_t num_origin_down_cnt;
    uint64_t num_adds;
    uint64_t num_deletes;
    uint64_t num_add_fails;
    uint64_t num_delete_fails;
} cse_crawl_stats_t;

/*
 * Crawler context structure.
 * Each node correspond to one particular origin server.
 */
typedef struct crawler_context {
    struct crawler_context_config cfg;
    struct cse_crawl_stats stats;

    int inprogress_before_delete;
    crawling_status_t crawling_activity_status;
    wget_status_t wget_status;
    int in_job_routine;
    uint64_t next_trigger_time;
    uint64_t crawl_start_time;
    uint64_t crawl_end_time;
    int crawl_not_run;

    TAILQ_ENTRY (crawler_context) crawler_config_entries;
    TAILQ_ENTRY (crawler_context) timer_entries;

    pid_t crawler_pid;

    FILE *payload_fd;
    int  change_lists_state;
    int (*change_lists_callback) (struct crawler_context *obj);
    change_list_type_t payload_file_type;

    int num_accept_extns;
    int num_extn_actions;
    /*
     * This is the ccp client context object.
     */
    ccp_client_session_ctxt_t *conn_obj;
    pthread_mutex_t crawler_ctxt_mutex;
} crawler_context_t;

typedef struct cse_url_struct {
    char *url;
    char *etag;
    char *domain;
    int64_t size;
} cse_url_struct_t;

int32_t nkn_cse_set_payload_fd (crawler_context_t *obj, change_list_type_t payload_type);
int8_t nkn_cse_stop_prev_crawl_instance(crawler_context_t *obj);
uint64_t get_cur_time(void);
void nkn_cse_timer_routine(void);
void nkn_cse_set_next_trigger_time(crawler_context_t *obj, uint32_t refresh_interval);
void timer_unit_test(void);
void nkn_cse_job_routine(crawler_context_t *obj);
int32_t nkn_cse_status_callback_func(void *obj);
void nkn_cse_cleanup_func(void *ptr);
int32_t nkn_cse_form_post_message(crawler_context_t *obj);
ccp_epoll_struct_t *g_ccp_epoll_node;
int32_t nkn_cse_get_url_attributes(char *line, cse_url_struct_t *urlnode);
void nkn_cse_generate_script_args(char *script_args, crawler_context_t *obj);
int32_t nkn_cse_update_master_del_list(crawler_context_t *obj);
int32_t nkn_cse_check_preload_eligibility(char *url, crawler_context_t *obj);
int32_t nkn_cse_check_if_valid_schedule(crawler_context_t *obj);
#endif //_NKN_CSE_H

#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#define MAX_THRDS 1

typedef enum {
    buffer = 0,
    sas,
    ssd,
    sata,
    origin,
    tunnel,
    unknown,
    non_cacheable,
} cache_indicator_t;

/*
 * Pointer this structure is given as key to domain hash table.
 */
typedef struct domain {
    char *domain;
    //int index;
}domain_t;

/*
 * Pointer this structure is given as key to uri hash table.
 */
typedef struct uri {
    char *uri;
    //int index;
    int64_t duplicate_bw;
}uri_t;

extern GHashTable *g_lru_hash_table;

/*
 * This is node of the list which stores the domain hash table values 
 * for all the domains
 */
typedef struct hash_mem {
    domain_hash_val_t *dom_val;
    struct hash_mem * next;
} hash_mem_t;

/*
 * This is node of the list which stores the g_pop_domain_hash table values 
 * for all the domains
 */
typedef struct pop_hash_mem {
    pop_domain_hash_val_t *dom_val;
    struct pop_hash_mem * next;
} pop_hash_mem_t;

/*
 * This is node of the list which stores the uri hash table values 
 * for all the uris
 */
typedef struct urihash_mem {
    uri_hash_val_t *uri_val;
    struct urihash_mem * next;
} urihash_mem_t;

typedef struct pop_dom{
    char *domain;
    struct pop_domain *next;
}pop_dom_t;

#define NEW_DOMAIN_INSERT(x,y,z) x[y].domain = (char *)calloc(strlen(z)+1, 1);\
				 strncpy(x[y].domain, z, strlen(z));\

#define NEW_URI_INSERT(x,y,z) x[y].uri = (char *)calloc(strlen(z)+1, 1);\
				 strncpy(x[y].uri, z, strlen(z));\


#define INC_BYTES_AND_HITS_VALUES(x, y, z) \
    if (!z) {\
	(x)->hits##_##y = 1;\
	if (line_arr[g_bytes_served_index]) {\
	    (x)->bytes_served##_##y = atol(line_arr[g_bytes_served_index]);\
	    (x)->tot_bytes_served = atol(line_arr[g_bytes_served_index]);\
	}\
        (x)->tot_hit_cnt = 1;\
	if (line_arr[g_content_length_index] && *line_arr[g_content_length_index] == '"') {\
	    (x)->tot_content_length = atol(line_arr[g_content_length_index]+1);\
	    if (!(x)->tot_content_length)\
		(x)->tot_content_length = atol(line_arr[g_bytes_served_index]);\
	} else {\
	    (x)->tot_content_length = atol(line_arr[g_bytes_served_index]);\
	}\
    } else {\
	(x)->hits##_##y += 1;\
	if (line_arr[g_bytes_served_index]) {\
	    (x)->bytes_served##_##y += atol(line_arr[g_bytes_served_index]);\
	    (x)->tot_bytes_served += atol(line_arr[g_bytes_served_index]);\
	}\
        (x)->tot_hit_cnt += 1;\
	if (line_arr[g_content_length_index] && *line_arr[g_content_length_index] == '"') {\
	    long long temp_cl = atol(line_arr[g_content_length_index]+1);\
	    if (temp_cl)\
		(x)->tot_content_length += temp_cl;\
	    else \
		(x)->tot_content_length += atol(line_arr[g_bytes_served_index]);\
	} else {\
	    (x)->tot_content_length += atol(line_arr[g_bytes_served_index]);\
	}\
    }\

#define UPDATE_MASTER_ST(x, y, z) \
	(x)->hits##_##y += (z)->hits##_##y;\
        (x)->bytes_served##_##y += (z)->bytes_served##_##y;\

#endif
void * LA_malloc(int64_t size);
void * LA_calloc(int64_t num, int64_t objsize);
void * LA_realloc(void *ptr, int64_t objsize);
void LA_clean_up(void);
void LA_create_single_accesslog(void);
void LA_get_hostname(void);
void LA_init(void);
void LA_get_access_log_list(char *acclog_path);
int check_if_top_domain(char *domain);
void quicksort_pop_domains(int32_t *list, int32_t m, int32_t n);
void LA_populate_top_domain_list(char *acclog, int32_t thrd_id);
void replace_domain_with_type(int64_t bytes_served, char **domain) ;
void LA_parse_config_file(char *config_file);
void clear_arr(char **line_arr, int32_t field_cnt);
void LA_update_uri_hash_table(uri_hash_val_t *uri_val, char **line_arr);
void LA_update_domain_hash_table(domain_hash_val_t *dom_val, char **line_arr);
void LA_insert_uri_hash_table(urihash_mem_t **cur_urihash_mem_node, char **line_arr, char *url, int32_t thrd_id);
void LA_insert_domain_hash_table(hash_mem_t **cur_hash_mem_node, char **line_arr, int32_t thrd_id);
void LA_increment_counters(domain_hash_val_t *ptr, int32_t flag, char **line_arr);
void LA_increment_uricounters(uri_hash_val_t *ptr, int32_t flag, char **line_arr);
void LA_update_bucketwise_stats(domain_hash_val_t *dom_val, char **line_arr, int32_t flag, char *cacheable_flag);
void quicksort(int32_t *list, int32_t m, int32_t n);
void LA_publish_results(int32_t thrd_id);
void iterator(gpointer key, domain_hash_val_t *value, gpointer user_data);
int32_t LA_line_split(char **line_arr, char *line);
void read_access_log_fill_hash(char *acclog, int32_t thrd_id);
void create_hash_table(int thrd_id);
void insert_replace_hash(char *key, domain_hash_val_t *dom_val, int32_t thrd_id);
void insert_replace_urihash(char *key, uri_hash_val_t *dom_val, int32_t thrd_id);
void start_thread(void *data);
void *sig_handler(int sig);
void print_srcips_domainwise(gpointer key, domain_hash_val_t *value);
void merge_domain_hash(void *data);
void merge_uri_hash(void *data);
void update_tunnel_stats_global (char *cacheable_flag, int32_t thrd_id, char **line_arr);
//void update_tunnel_stats_dwise (char *cacheable_flag, char **line_arr, domain_hash_val_t *dom_val);
void update_tunnel_stats_dwise (char **line_arr, domain_hash_val_t *dom_val);



tunnel_cntr_t *tunnel_head[MAX_THRDS];

static int32_t g_dom_list_size[MAX_THRDS];
static int32_t g_pop_dom_list_size[MAX_THRDS];
static int32_t g_uri_list_size[MAX_THRDS];

/*
 * File pointers to various output files.
 */
FILE *con_FP;
FILE *oall_FP;
FILE *CE_FP[MAX_THRDS];
FILE *Uriwise_FP[MAX_THRDS];
FILE *DW_TFP[MAX_THRDS];
//FILE *urirepFP[MAX_THRDS];
FILE *dupFP[MAX_THRDS];
FILE *SRCIP_FP[MAX_THRDS];

pthread_t thrds[MAX_THRDS];
int t_num[MAX_THRDS];

domain_t *g_pop_domain_list[MAX_THRDS];
domain_t *g_domain_list[MAX_THRDS];
uri_t *g_uri_list[MAX_THRDS];

int32_t *g_master_domain_indices_sorted;
int32_t *g_pop_domain_indices_sorted[MAX_THRDS];
int *g_master_uri_indices_sorted;
domain_t *g_master_domain_list;
uri_t *g_master_uri_list;

/*
 * Memory pool to store domain hash values.
 */
hash_mem_t *g_hash_mem[MAX_THRDS];
/*
 * Memory pool to store popular domain hash values.
 */
pop_hash_mem_t *g_pop_hash_mem[MAX_THRDS];
/*
 * Memory pool to store uri hash values.
 */
urihash_mem_t *g_urihash_mem[MAX_THRDS];

hash_mem_t *g_master_dom_hashmem;
urihash_mem_t *g_master_uri_hashmem;
int g_init_mem_nodes;
hash_mem_t *g_cur_master_dom_hash_memnode;
urihash_mem_t *g_cur_master_uri_hash_memnode;
pthread_mutex_t merge_mutex;

int xxxx =1;
int check_if_dup_uri_already_present(domain_hash_val_t *dom_val, int32_t value, int32_t start_index, int32_t end_index);
void LA_increment_bucketwise_counters(CE_CHR_t *ptr, int32_t flag, char **line_arr);
void update_dom_val_st(domain_hash_val_t *dom_val, domain_hash_val_t *dom_val_thrd);
void update_uri_val_st(uri_hash_val_t *uri_val, uri_hash_val_t *uri_val_thrd);

pthread_t memory_tracking_thread_id;

/*
 * This variable stores the max virtual memory the process can 
 * consume. Its a configurable value.
 * default is 2 GB.
 */
int32_t g_max_process_memory = 2000*1024*1024;
int g_end_run = 0;

/*
 * This routine is called by the memory tracker thread.
  */
void * memory_tracker(void *max_memory);
int g_lru_list_empty = 0;

/*
 * This mutex ensures that when the lru list is protected from parallel updates
 * by the memory tracker thread and the main thread.
 */
pthread_mutex_t lru_mutex;

void LA_create_free_space(void);
void LA_combine_multiple_access_logs(void);
void LA_parse_access_log_header(char **line_arr);
void LA_create_pop_domain_list(void);
void LA_insert_LRU_list(char *uri);
int get_cur_memory(pid_t pid);
void LA_update_LRU_list(char *uri);
int get_bucket_num(int64_t bytes_served);
int check_for_new_logs(char *path);
void swap_pop_domains(int32_t x, int32_t y);
void swap(int32_t x, int32_t y);
int choose_pivot(int32_t i, int32_t j);
void uri_iterator(gpointer key, uri_hash_val_t *value, gpointer user_data);

typedef struct prov_stats {
    char *prov_name;
    char *bytes;
    struct prov_stats * next;
} prov_stats_t;

prov_stats_t * LA_parse_provider_list (char **line_arr, int32_t field_cnt);
int LA_update_hash_tables(char **line_arr, char *uri, char *temp_buf, int8_t update_dup_bw);


typedef struct inc_values {
    uint64_t dup_bw;
    uint64_t cache_size_req;
} inc_values_t;

void parse_byte_range (struct inc_values *obj, uri_hash_val_t *uri_val, char **line_arr);

#define OVERALL_REP_FIELD_CNT 39
#define DW_REP_FIELD_CNT 22
/*
 * data structures used for merging multiple domainwise reports
 */
typedef struct merge_dw_hash_val {
    uint64_t counter[DW_REP_FIELD_CNT];
    struct merge_dw_hash_val *next;
} merge_dw_hash_val_t;


typedef struct merge_dw_domlist {
    char *domain_name;
    struct merge_dw_domlist *next;
} merge_dw_domlist_t;

typedef struct merge_overall_stats {
    uint64_t counter[OVERALL_REP_FIELD_CNT];
} merge_overall_stats_t;

#define MAX_THRDS 1
#define MAX_TUNNEL_RESP_CODES 58

/*
 * Various index values from the access log. These are the default values 
 * however when a Fields line is encounterd, new index values are determined
 * after parsing that line. 
 */
int32_t g_bytes_served_index	    = 10;
int32_t g_status_index		    = 9;
int32_t g_namespace_index 	    = 11;
int32_t g_cacheable_flag_index      = 0;
int32_t g_domain_index              = 2;
int32_t g_uri_index                 = 7;
int32_t g_src_ip_index		    = 0;
int32_t g_print_srcip_domainwise    = 0;
int32_t g_content_length_index	    = 16;
int32_t g_method_index		    = 6;
int32_t g_group_tproxy_domains 	    = 0;
int32_t g_age_index		    = 14;

uint8_t g_create_uri_wise_report    = 0;
uint8_t g_print_md5_uri_map    = 0;
uint8_t g_origin_bytes_configured = 0;
int32_t g_uniq_pop_domain_cnt[MAX_THRDS];
int32_t g_uniq_domain_cnt[MAX_THRDS];
int32_t g_uniq_uri_cnt[MAX_THRDS];
int32_t g_uniq_master_domain_cnt;
int32_t g_uniq_master_uri_cnt;
int32_t g_line_cnt[MAX_THRDS];

/*
 * These are the various buckets to generate the size-wise stats.
 */
int32_t g_CE_num_buckets    = 12;
int32_t g_CE_buckets[]	    = {1, 4, 8, 12, 16, 20, 24, 28, 32, 128, 512, 2048, 4096};

typedef struct bucket_stats {
    int64_t hit_cnt;
    int64_t bytes_served;
}bucket_stats_t;

#define MAX_BUCKET_NUM  13
bucket_stats_t g_bucket_stats[MAX_BUCKET_NUM];
char *g_hostname;
char *g_access_log_path, *g_acclog_name;

/*
 * These counters maintain the overall stats (hits & bytes served) 
 * from various providers (buffer, sas, sata, ssd, origin, tunnel)
 */
int64_t g_overall_hits[MAX_THRDS]                     ;
int64_t g_overall_bytes_served[MAX_THRDS]             ;
int64_t g_overall_cnt_buffer[MAX_THRDS]	        ;
int64_t g_overall_cnt_ssd[MAX_THRDS]	        ;
int64_t g_overall_cnt_sas[MAX_THRDS]	        ;
int64_t g_overall_cnt_sata[MAX_THRDS]	        ;
int64_t g_overall_cnt_origin[MAX_THRDS]	        ;
int64_t g_overall_cnt_tunnel[MAX_THRDS]               ;
int64_t g_overall_bytes_served_from_sata[MAX_THRDS]  ;
int64_t g_overall_bytes_served_from_sas[MAX_THRDS]  ;
int64_t g_overall_bytes_served_from_ssd[MAX_THRDS]  ;
int64_t g_overall_bytes_served_from_buffer[MAX_THRDS]  ;
int64_t g_overall_bytes_served_from_origin[MAX_THRDS] ;
int64_t g_overall_bytes_served_from_tunnel[MAX_THRDS] ;
int64_t g_overall_bytes_served_from_cache[MAX_THRDS] ;

typedef struct acclog_name{
    char *log_name;
    struct acclog_name *next;
}acclog_name_t;

acclog_name_t *g_acclog_list;
acclog_name_t *g_acclog_list_new;

int32_t g_only_top_domains_report = 1;

/*
 * access log reports are generated only for this number of 
 * popular domains based on bytes served.
 * Default is 1000, actual value is calculated based on 
 * the max process memory.
 */
int32_t g_num_pop_domains = 1000;

/*
 * Objects below this size are ignored for analysis.
 * configurable value. default id 12 KB
 */
uint64_t g_size_threshold = 0*1024;

typedef struct tunnel_cntr {
    char *reasoncode;
    int32_t cntr;
    struct tunnel_cntr *next;
    int64_t bw;
}tunnel_cntr_t;


#define TRESP_CODE_GAP 35
char tunnel_resp_codes[][50] = {"NKN_TR_UNKNOWN",
				"NKN_TR_REQ_BAD_REQUEST",
				"NKN_TR_REQ_ABS_URL_NOT_CACHEABLE",
				"NKN_TR_REQ_NO_HOST",
				"NKN_TR_REQ_POLICY_NO_CACHE",
				"NKN_TR_REQ_SSP_FORCE_TUNNEL",
				"NKN_TR_REQ_GET_REQ_COD_ERR",
				"NKN_TR_REQ_BIG_URI",
				"NKN_TR_REQ_NOT_GET",
				"NKN_TR_REQ_UNSUPP_CONT_TYPE",
				"NKN_TR_REQ_TUNNEL_ONLY",
				"NKN_TR_REQ_CONT_LEN",
				"NKN_TR_REQ_MULT_BYTE_RANGE",
				"NKN_TR_REQ_HDR_LEN",
				"NKN_TR_REQ_AUTH_HDR",
				"NKN_TR_REQ_COOKIE",
				"NKN_TR_REQ_QUERY_STR",
				"NKN_TR_REQ_CC_NO_CACHE",
				"NKN_TR_REQ_PRAGMA_NO_CACHE",
				"NKN_TR_REQ_CHUNK_ENCODED",
				"NKN_TR_REQ_HTTP_BAD_HOST_HEADER",
				"NKN_TR_REQ_HTTP_BAD_URL_HOST_HEAD",
				"NKN_TR_REQ_HTTP_URI",
				"NKN_TR_REQ_HTTP_REQ_RANGE1",
				"NKN_TR_REQ_HTTP_UNSUPP_VER",
				"NKN_TR_REQ_HTTP_NO_HOST_HTTP11",
				"NKN_TR_REQ_HTTP_ERR_CHUNKED_REQ",
				"NKN_TR_REQ_HTTP_REQ_RANGE2",
				"NKN_TR_REQ_DYNAMIC_URI_REGEX_FAIL",
				"NKN_TR_MAX_SIZE_LIMIT",
				"NKN_TR_RES_POLICY_NO_CACHE",
				"NKN_TR_RES_QUERY_STR",
				"NKN_TR_RES_RANGE_CHUNKED",
				"NKN_TR_RES_NO_CONTLEN_NO_CHUNK",
				"NKN_TR_RES_CONTLEN_ZERO",
				"NKN_TR_RES_HEX_ENCODED",
				"NKN_TR_RES_SSP_CONFIG_ERR",
				"NKN_TR_RES_UNSUPP_RESP",
				"NKN_TR_RES_NO_302_NOT_RPROXY",
				"NKN_TR_RES_NO_LOC_HDR",
				"NKN_TR_RES_LOC_HDR_ERR",
				"NKN_TR_RES_COD_EXPIRED",
				"NKN_TR_RES_CC_NO_TRANSFORM",
				"NKN_TR_RES_SAVE_VAL_ST_ERR",
				"NKN_TR_RES_SAVE_MIME_HDR_ERR",
				"NKN_TR_RES_MIME_HDR_TO_BUF_ERR",
				"NKN_TR_RES_CONT_RANGE_ERR",
				"NKN_TR_RES_INVALID_CONT_RANGE",
				"NKN_TR_RES_NO_CONT_LEN",
				"NKN_TR_RES_RANGE_OFFSET_ERR",
				"NKN_TR_RES_CC_PRIVATE",
				"NKN_TR_RES_SET_COOKIE",
				"NKN_TR_RES_OBJ_EXPIRED",
				"NKN_TR_RES_ADD_NKN_QUERY_ERR",
				"NKN_TR_RES_CACHE_SIZE_LIMIT",
				"NKN_TR_RES_NON_CACHEABLE",
				"NKN_TR_CACHE_SIZE_LIMIT",
			        "NKN_TR_MAX_DEFS"};

/*
 * Structure to store 1-hit uris in the lru list.
 */
typedef struct lru_uri {
    char *uri;
    int32_t index;
    struct lru_uri *next;
}lru_uri_t;

/*
 * Data structure to maintain agewise stats.
 * g_age_index iindicates the age field in the access log.
 */
#define NUM_AGE_BUCKETS 10
#define MIN_AGE 60
#define AGE_SCALE_FACTOR 10 //60, 600, 6000 ..
typedef struct agewise_stats {
    uint64_t cached_bw;
    uint64_t total_bw;
    uint64_t tunneled_bw;
    uint64_t origin_bw;
} agewise_stats_t;
agewise_stats_t g_agewise_stats[NUM_AGE_BUCKETS];

lru_uri_t *g_lru_uri_list_start_node, *g_lru_uri_list_end_node;
int32_t g_lru_cur_node_num=0;
int32_t g_lru_deleted_nodes = 0;

/*
 * This is the timeperiod for polling for new access logs in live mode.
 * configurable value.
 */
int32_t g_log_wait_period = 0;

int8_t g_byte_range_index = -1;
int8_t g_access_log_has_byte_ranges = 0;

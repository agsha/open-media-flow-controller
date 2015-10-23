#include <sys/epoll.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <err.h>
#include <stdarg.h>
#include <ctype.h>
#include <search.h>
#include <glib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <assert.h>
#include <sys/prctl.h>
#include <dirent.h>

#include "nkn_debug.h"
#include "hash.h"
#include "globals.h"
#include "log_analyzer.h"
//#define __USE_XOPEN
#include <time.h>
#include "global.h"
#include "md5.h"

#define MD_CTX MD5_CTX
#define MDInit MD5Init
#define MDUpdate MD5Update
#define MDFinal MD5Final

#define MAX_ACCESS_LOG_FIELDS 130

int g_treat_tunnel_as_origin = 0;
int g_only_youtube_googlevideo = 0;
char *g_acclog_list_filename;
int LA_process_dynamic_uri(char **line_arr);
int g_get_agewise_stats;
/*
 * Peak time analysis
 */
int g_do_peak_time_analysis = 0;
int g_treat_all_cacheable_as_origin = 0;
int g_peak_time = 0;
int g_time_stamp_index = 4;
int64_t g_10_dup = 0;
/*
 * If g_live_mode
 * = 0 : takes access log as input and generates the reports.
 * = 1 : reads the .gz access logs  from /var/nkn/logs and generates the reports
 */
int64_t g_tot_br_transactions, g_tot_br_repeat_transactions = 0;
int64_t g_missed_bw_today = 0;
int32_t g_live_mode = 0;
int32_t g_find_actual_dup_bw = 1;
FILE *md5urimapFP;
extern char * strcasestr(const char *, const char *);
extern int32_t strcasecmp(const char *, const char *);
extern int32_t strncasecmp(const char *, const char *, size_t );
void create_outdir (char *dir);
void usage(void);
uint8_t get_cur_rep_num(void);
uint8_t g_max_reports_cnt = 100; /* Max no. of reports in /var/log/report*/
char g_outdir[200];
uint8_t LA_filter_entry (char **line_arr);
static void LA_print_agewise_stats(FILE *fp);
static void LA_fill_agewise_stats(uint64_t bytes_served, int age);
static void LA_replace_uri_md5_cksum(char **uri, char *temp_buf, int len);
#define log_error(err_str, file_name, line_no)\
{\
    DBG_LOG(ERROR, MOD_ANY_MOD, "%s at %s, line no: %d", err_str, file_name, line_no);\
    exit (-1);\
}
struct range_uri {
    int8_t flag;
    unsigned long long lower_range;
    unsigned long long upper_range;
} g_range_uri;

void merge_reports (void);

void merge_domain_wise_reports (char *dir_name,
			    GHashTable *merge_dw_hash,
			    merge_dw_domlist_t *merdw_domlist_cur_node,
			    merge_dw_hash_val_t *merdw_val_cur_node);

void parse_dw_report (char *full_file_name,
			GHashTable *merge_dw_hash,
			merge_dw_hash_val_t *merdw_val_cur_node,
			merge_dw_domlist_t *merdw_domlist_cur_node);

void publish_merged_reports (gpointer key,
		    merge_dw_hash_val_t *merdw_val, gpointer userdata);

void parse_overall_report (char *full_file_name,
	merge_overall_stats_t *oall_statsp);

void
parse_byte_range1 (struct inc_values *obj, uri_hash_val_t *uri_val,
		    unsigned long long bytes_served);
static uint8_t g_cache_indicator;
static pop_hash_mem_t *g_pop_cur_hash_mem_node;
static hash_mem_t *g_cur_hash_mem_node;
static urihash_mem_t *g_cur_urihash_mem_node;
FILE *tempfp;

int
main(int argc, char **argv)
{
    tempfp = fopen ("tempurifile", "w");
    char *cfgfile = NULL;
    char *cp = NULL;
    char *acclog_list = NULL;
    struct stat st;
    int32_t thrd_id = 0;
    char command[2000];
    char *full_file_name = (char *)LA_calloc(2000, 1);
    int ret;
    DIR *dirp;
    struct dirent *dp;
    char line[5000];
    pid_t pid;
    g_live_mode = -1;
    g_access_log_has_byte_ranges = 1;

    while ((ret = getopt(argc, argv, "f:m:p:l:h")) != -1) {
	switch (ret) {
	case 'f':
	    cfgfile = optarg;
	    break;
	case 'm':
	    if (*optarg == '1') {
		g_live_mode = 1;
	    } else {
		g_live_mode = 0;
	    }
	    break;
	case 'p':
	    cp = optarg;
	    break;
	case 'l':
	    acclog_list=optarg;
	    break;
	case 'h':
	    usage();
	    break;
	}
    }

    if (cfgfile == NULL || g_live_mode == -1 || cp == NULL)
	usage();

    LA_parse_config_file(cfgfile);

    pid = getpid();
    pthread_mutex_init(&lru_mutex, NULL);


    if(g_live_mode) {
	g_access_log_path = cp;
    } else {
	g_acclog_list_filename = acclog_list;
	if (stat(cp, &st) == 0){
	    /* 
	     * If the arg is a dir, read each access* file and process
	     */
	    if (S_ISDIR (st.st_mode)) {
		g_access_log_path = cp;
		g_acclog_name = (char *) LA_malloc (200); 
	    } else {
		g_access_log_path = NULL;
		g_acclog_name = cp;
	    }
	} else {
	    log_error ("File open error", __FILE__, __LINE__);
	}
    }

    /*
     * Limiting the no. of domains to be analyzed to 5% of the 
     * total number of domains that can be accomodated.
     */
    g_num_pop_domains = (g_max_process_memory*0.03)/sizeof(domain_hash_val_t);

    (void) signal(SIGINT, (void *) sig_handler);
    (void) signal(SIGPIPE, SIG_IGN);
    (void) signal(SIGHUP, (void *) sig_handler);
    (void) signal(SIGKILL, (void *) sig_handler);
    (void) signal(SIGTERM, (void *) sig_handler);
    /*g_thread_init(NULL);*/

    LA_init();

    /*
     * This thread checks the process's virtual memory every second
     * and clears the 1-hit uri related data when the memory is crossed.
     */
    //pthread_create(&memory_tracking_thread_id, NULL, memory_tracker, (void *)&pid);

    if(g_live_mode){
	/* 
	 * get the list  of compressed access logs in the 
	 * access log path and store 
	 */
	LA_get_access_log_list(g_access_log_path);

	acclog_name_t *cur_log;

start_processing:
	cur_log = g_acclog_list;
	/*cat all the gz files into access.gz file*/
	LA_combine_multiple_access_logs();


	sprintf(full_file_name, "%s/%s", g_access_log_path, "access.gz");
	if (stat(full_file_name, &st) == 0){
	    g_acclog_name = strdup(full_file_name);
	    /* 
	     * Run through the entire access log once and get the top 'x' domains. This
	     *  list will be used to generate reports only for these domains, to cut down
	     *   on RAM memory usage.
	     */
	    LA_populate_top_domain_list(g_acclog_name, 0);
	}
	LA_create_pop_domain_list();
 
	/*Start processing the access log and collect the stats*/
	if (stat(full_file_name, &st) == 0){
	    g_acclog_name = strdup(full_file_name);
	    read_access_log_fill_hash(g_acclog_name, 0);
	}
	/*remove the file formed from all gz files*/
	sprintf(command,"rm -rf %s", full_file_name);
	system(command);

	/*Check for newer access logs*/
	ret = check_for_new_logs(g_access_log_path);
	if(ret){
	    goto start_processing;
	}

    } else { /*Offline mode*/

	if (g_only_top_domains_report) {
	    if (g_access_log_path == NULL) {
		LA_populate_top_domain_list(g_acclog_name, 0);
	    } else {
		/*
		* Read each access log file from the dir 
		* and process
		*/
		if ((dirp = opendir(g_access_log_path)) == NULL) {
		    log_error ("Failed to open dir", __FILE__, __LINE__);
		}

		do {
		    errno = 0;
		    if ((dp = readdir(dirp)) != NULL) {
			if (strcasestr (dp->d_name, "access")) {
			    snprintf (g_acclog_name, 200, "%s/%s", g_access_log_path, dp->d_name);
			    LA_populate_top_domain_list(g_acclog_name, 0);
			} else {
			    continue;
			}
		    }
		} while (dp != NULL);
		(void) closedir(dirp);
	    }

	    LA_create_pop_domain_list();
	}

	if (g_access_log_path == NULL) {
	    read_access_log_fill_hash(g_acclog_name, 0);
	} else {

	    /*
	     * Read each access log file from the dir 
	     * and process
	     */
	    if ((dirp = opendir(g_access_log_path)) == NULL) {
		log_error ("Failed to open dir", __FILE__, __LINE__);
	    } else {

		if (g_acclog_list_filename) {
		    FILE *fp;
		    fp = fopen(g_acclog_list_filename, "r");
		    while(fgets(line, sizeof(line), fp)){ 
			if (line[strlen(line)-1] == '\n')
			    line[strlen(line)-1] = '\0';
			read_access_log_fill_hash(line, 0);
		    }
		    fclose(fp);
		} else {
		    do {
			errno = 0;
			if ((dp = readdir(dirp)) != NULL) {
			    if (strcasestr (dp->d_name, "access")) {
				snprintf (g_acclog_name, 200, "%s/%s", g_access_log_path, dp->d_name);
				fprintf (stdout, "Processing %s\n", g_acclog_name);
				read_access_log_fill_hash(g_acclog_name, 0);
			    } else {
				continue;
			    }
			}
		    } while (dp != NULL);
		}
		(void) closedir(dirp);
	    }
	}
    }

    g_end_run = 1;

    /*Publish the results*/
    LA_publish_results(0);

    if(g_print_srcip_domainwise) {
	fclose(SRCIP_FP[thrd_id]);
    }

    //if (g_live_mode)
    //pthread_join(memory_tracking_thread_id, NULL);
    free (full_file_name);
    return(1);
}

/*
 * This function parses the access log and collects the uri wise and domainwise stats
 */
void
read_access_log_fill_hash(char *acclog, int32_t thrd_id)
{
    char line[5000], command[50];
    char *line_arr[MAX_ACCESS_LOG_FIELDS]; 
    int32_t tmp = 0;
    FILE *fp;
    char *domain;
    char *uri = NULL;
    char *temp_buf = NULL;
    unsigned int len;
    int ret_val;
    int32_t field_cnt;
    prov_stats_t *provider_list = NULL, *temp_prov;
    thrd_id = 0;

    while(tmp < MAX_ACCESS_LOG_FIELDS){
	line_arr[tmp] =  NULL;
	tmp++;
    }

    if (g_live_mode)
	fp = popen("zcat -d ../access.gz", "r");
    else
	if (strstr(acclog, ".gz")) {
	    sprintf (command, "zcat -d %s", acclog);
	    fp = popen(command, "r");
	} else
	    fp = fopen(acclog, "r");

    if (fp)
    {
	while(fgets(line, sizeof(line), fp)){

	    /*
	     * split the access log entry and populate line_arr
	     */
	    field_cnt = LA_line_split(line_arr, line);

	    /*
	     * Parse the header line and get the index values
	    if (line_arr[0] && strcasestr(line_arr[0], "Fields")){
	        LA_parse_access_log_header(line_arr);
	    }
	     */

	    if (LA_filter_entry(line_arr)) {
		clear_arr(line_arr, field_cnt);
		continue;
	    }

	    domain = line_arr[g_domain_index];
	    uri = (char *) LA_malloc (strlen(line_arr[g_uri_index]) + 1);
	    sprintf(uri, "%s", line_arr[g_uri_index]);
	    if (temp_buf) {
		free(temp_buf);
		temp_buf = NULL;
	    }
	    temp_buf = LA_calloc(strlen(uri)+42, 1);

	    /*
	     * If uri length > 32, represent the uri with md5 chksum
	     */
	    sprintf(temp_buf, "%s<delim>", uri);
	    len = strlen(uri);
	    if (len > 32){
		LA_replace_uri_md5_cksum(&uri, temp_buf, len);
	    }

	    /*
	     * Parse the %e field (Buffer_100_Origin_200_SAS_10_Origin_900_Buffer_100)
	     * and get provider wise stats.
	     */
	    provider_list = LA_parse_provider_list (line_arr, field_cnt);

	    /*
	     * Either %e is not configured or its a tunnel request.
	    */
	    if (provider_list == NULL) {
		if ((ret_val = LA_update_hash_tables(
		    	    line_arr, uri, temp_buf, 1)) == -1) {
		    clear_arr(line_arr, field_cnt);
		    if (uri) {
		        free(uri);
		        uri = NULL;
		    }
		    continue;
		}
	    } else {
		int8_t update_dup_bw = 1;
		while (provider_list) {
		    free (line_arr[g_bytes_served_index]);
		    free (line_arr[g_cacheable_flag_index]);
		    line_arr[g_bytes_served_index] = strdup(
							provider_list->bytes);
		    line_arr[g_cacheable_flag_index] = strdup(
							provider_list->prov_name);
		    if ((ret_val = LA_update_hash_tables(line_arr, uri,
					    temp_buf, update_dup_bw)) == -1) {
			while (provider_list) {
			    temp_prov = provider_list;
			    provider_list = provider_list->next;
			    free (temp_prov->prov_name);
			    free (temp_prov->bytes);
			    free (temp_prov);
			}
			break;
		    }

		    temp_prov = provider_list;
		    provider_list = provider_list->next;
		    free(temp_prov->prov_name);
		    free(temp_prov->bytes);
		    free(temp_prov);
		    update_dup_bw = 0;
		}
	    }
	    clear_arr(line_arr, field_cnt);
	    if (uri) {
		free(uri);
		uri = NULL;
	    }
	    if (temp_buf) {
		free(temp_buf);
		temp_buf = NULL;
	    }

	}
    }
}

/*
 * Apply various filters before populating the hash tables
 */
uint8_t
LA_filter_entry (char **line_arr)
{
    int ret_val;
    int thrd_id = 0;    
    g_range_uri.flag = 0;
    g_range_uri.lower_range = 0;
    g_range_uri.upper_range = 0;
    g_line_cnt[thrd_id]++;
    if (g_line_cnt[thrd_id]%100000 == 0) {
        int bytes_gb = g_overall_bytes_served[thrd_id]>>30;
        printf("line_cnt :%d  uniq_domain_cnt: %d cur_missed_chr: %ld "
        	"tot_bytes: %dGB\n", g_line_cnt[thrd_id],
        	g_uniq_domain_cnt[thrd_id],
        	(g_missed_bw_today*100/g_overall_bytes_served[thrd_id]),
        	bytes_gb);
    }

    if (g_uri_index == -1 || g_domain_index == -1 ||
	    g_bytes_served_index == -1 || g_status_index == -1 ||
	    g_method_index == -1)
	return -1;

    if(line_arr[g_uri_index] == NULL || line_arr[g_domain_index] == NULL ||
	    line_arr[g_bytes_served_index] == NULL || 
        strchr(line_arr[g_bytes_served_index], '%') ||
	(atoi(line_arr[g_status_index])!= 200 && atoi(line_arr[g_status_index])
	    != 206) || !strcasestr( line_arr[g_method_index],"get")) {
	return -1;
    }

    ret_val = LA_process_dynamic_uri(line_arr);
    if (ret_val == -1) {
	return -1;
    }

    if (line_arr[g_namespace_index] && g_group_tproxy_domains &&
         !(strcasecmp("tproxy", line_arr[g_namespace_index]))) {
        free(line_arr[g_domain_index]);
        line_arr[g_domain_index] = strdup(line_arr[g_namespace_index]);
    }
    
    if (g_do_peak_time_analysis) {
        if (strcasestr(line_arr[g_time_stamp_index], "15/Nov/2011:21:") ||
            strcasestr(line_arr[g_time_stamp_index], "15/Nov/2011:22:")) {
            g_peak_time = 1;
        } else {
            g_peak_time = 0;
        }
    }

    /*
     * Treat every thing as served from origin
     * can be used to get an estimate of the possible CHR
     */
    if (g_treat_all_cacheable_as_origin) {
        if (g_treat_tunnel_as_origin || !strcasestr(
        		line_arr[g_cacheable_flag_index], "Tunnel")) {
            free(line_arr[g_cacheable_flag_index]);
            line_arr[g_cacheable_flag_index] = (char *) calloc(7,1);
            sprintf(line_arr[g_cacheable_flag_index], "%s", "origin");
        }
    }
    

    return 0;
}

/*
 * This function splits the access log line and populates the fields in line_arr(array of pointers)
 */
int32_t
LA_line_split(char **line_arr, char *line)
{
    char *field, *field1;
    int32_t field_cnt = 0;
    char *sp, *p;
    field = strtok_r(line, " ", &sp);
    field1 = field;
    uint32_t str_len, i;
 
    while(field1){
	field1 = strtok_r(NULL, " ", &sp);
	if (field1){
	    str_len = field1 - field - 1;
	}else
	    str_len = strlen(field);

	line_arr[field_cnt] = (char *)LA_calloc(str_len+1, 1);
	memcpy(line_arr[field_cnt], field, str_len);
	field_cnt++;
	field = field1;

	if (field_cnt >= MAX_ACCESS_LOG_FIELDS){
	    DBG_LOG(ERROR, MOD_ANY_MOD,
		"Too many fields in the access log line: %d %s", field_cnt,
		line_arr[g_domain_index]);
	    clear_arr(line_arr, field_cnt);
	    return (field_cnt);
	}

    }

    if (line_arr[g_cacheable_flag_index]){
	p = line_arr[g_cacheable_flag_index];
	i = 0;
	while (i < strlen(line_arr[g_cacheable_flag_index])){
	    if (*p == '\n')
		*p = 0;
	    i++;
	    p++;
	}
    }
    return (field_cnt);
}

/*
 * This function prints the domainwise stats for a specific key
 */
void
iterator(gpointer key, domain_hash_val_t *value, gpointer user_data)
{
    int32_t i, j, low_limit, high_limit;
    int32_t thrd_id = *(int32_t *)user_data;
    float bw_percentage = 0;
    float bucket_wise_CHR; 

    for (i = 0; i <= g_CE_num_buckets; i++){
	if (i == 0){
	    low_limit = 0;
	} else {
	    low_limit = g_CE_buckets[i-1];
	}

	if (i < g_CE_num_buckets)
	    high_limit = g_CE_buckets[i];

	if (g_overall_bytes_served[thrd_id]){
	    bw_percentage = (value->ce_chr[i].tot_bytes_served * 100);
	    bw_percentage = bw_percentage/(float)g_overall_bytes_served[thrd_id];
	}
	if (value->ce_chr[i].tot_bytes_served) {
	    bucket_wise_CHR = (double)(value->ce_chr[i].bw_saved)/
				(double)(value->ce_chr[i].tot_bytes_served);
	} else {
	    bucket_wise_CHR = 0;
	}

	if (low_limit < high_limit) {
	    fprintf(CE_FP[0], "%s, (%d-%d), %lld, %lld, %f, %f, %lld, %lld,"
		    "%lld, %lld, %lld, %lld, %lld, %lld, %3.5f, %d, %d, %d, %d,"
		    "%d, %d, %d, %3.5f, %lld\n", (char *)key , low_limit,
		    high_limit, value->ce_chr[i].bw_saved, value->ce_chr[i].
		    cache_size_req , value->ce_chr[i].CE, bucket_wise_CHR,
		    value->ce_chr[i].bytes_served_tunnel, value-> ce_chr[i].
		    bytes_served_origin, value->ce_chr[i].bytes_served_sas,
		    value->ce_chr[i].bytes_served_sata, value->ce_chr[i].
		    bytes_served_ssd, value->ce_chr[i].bytes_served_buffer,
		    value->ce_chr[i].tot_bytes_served, value->duplicate_bw,
		    bw_percentage, value->ce_chr[i].hits_tunnel, value->ce_chr[
		    i].hits_origin, value->ce_chr[i].hits_sas, value->ce_chr[i].
		    hits_ssd, value->ce_chr[i].hits_sata, value->ce_chr[i].
		    hits_buffer, value->ce_chr[i].tot_hit_cnt, (value->ce_chr[i]
		    .tot_hit_cnt*100/(float)g_overall_hits[thrd_id]), value->
		    ce_chr[i].tot_content_length);
	} else {
	    fprintf(CE_FP[0], "%s, (>%d), %lld, %lld, %f, %f, %lld, %lld, %lld,"
		"%lld, %lld, %lld, %lld, %lld, %3.5f, %d, %d, %d, %d, %d, %d, %d,"
		"%3.5f, %lld\n", (char *)key, high_limit, value->ce_chr[i].
		bw_saved, value->ce_chr[i].cache_size_req, value->ce_chr[i].CE,
		bucket_wise_CHR, value->ce_chr[i].bytes_served_tunnel, value->
		ce_chr[i].bytes_served_origin, value->ce_chr[i].bytes_served_sas,
		value->ce_chr[i].bytes_served_sata, value->ce_chr[i].bytes_served_ssd,
		value->ce_chr[i].bytes_served_buffer, value->ce_chr[i].
		tot_bytes_served, value->duplicate_bw, bw_percentage, value->
		ce_chr[i].hits_tunnel, value->ce_chr[i].hits_origin, value->
		ce_chr[i].hits_sas, value->ce_chr[i].hits_ssd, value->ce_chr[i].
		hits_sata, value->ce_chr[i].hits_buffer, value->ce_chr[i].tot_hit_cnt,
		(value->ce_chr[i].tot_hit_cnt*100/(float)g_overall_hits[thrd_id]),
		value->ce_chr[i].tot_content_length);
	}
	fflush(CE_FP[0]);
    }

    if (g_print_srcip_domainwise)
	print_srcips_domainwise(key, value);

    fprintf(DW_TFP[0], "%s,", (char *)key);

    for (j = 0; j < MAX_TUNNEL_RESP_CODES; j++){
        fprintf(DW_TFP[0], "%lld,", value->dw_tun[j].bw);
    }

    fprintf(DW_TFP[0], "\n");
    if (value->cur_dup_uri){
	for (i = 0; i < value->cur_dup_uri; i++){
	    fprintf(dupFP[thrd_id], "%s <abcd> %s <abcd> %lu\n", (char *)key,
		    g_uri_list[thrd_id][value->dup_bw_uri_index_list[i]].uri,
		    g_uri_list[thrd_id][value->dup_bw_uri_index_list[i]].
		    duplicate_bw);
	}
    }
}

void
uri_iterator (gpointer key, uri_hash_val_t *value, gpointer user_data)
{
    int32_t thrd_id = *(int32_t *)user_data;
    float chr, bw_percentage;
    int64_t bw_saved;

    if (value->tot_hit_cnt > 1){
	/*
	 * Create uri-wise stats
	 */
	bw_saved = (value->bytes_served_ssd + value->bytes_served_sas + \
		value->bytes_served_sata + value->bytes_served_buffer);

	if (value->tot_bytes_served)
	    chr = bw_saved/value->tot_bytes_served;

	bw_percentage = (value->tot_bytes_served*100)/
			    g_overall_bytes_served[thrd_id];
	fprintf(Uriwise_FP[0], "%s, %s, %lu, %f, %lld, %lld, %lld, %lld, %lld,"
		"%lld, %lld, %3.5f, %d, %d, %d, %d, %d, %d, %d, %3.5f\n",
		(char *)key, value->domain_name, bw_saved, chr, value->
		bytes_served_tunnel, value->bytes_served_origin, value->
		bytes_served_sas, value->bytes_served_sata, value->
		bytes_served_ssd, value->bytes_served_buffer, value->
		tot_bytes_served, bw_percentage, value->hits_tunnel, value->
		hits_origin, value->hits_sas, value->hits_ssd, value->hits_sata,
		value->hits_buffer, value->tot_hit_cnt, (value->tot_hit_cnt*100/
		(float)g_overall_hits[thrd_id]));
    }
}

void
print_srcips_domainwise(gpointer key, domain_hash_val_t *value)
{
    struct srcip *ptr = value->ip_list;

    fprintf(SRCIP_FP[0], "domain: %s\n", (char *)key);
    while (ptr) {
	fprintf(SRCIP_FP[0],"%s\n", ptr->ip);
	ptr = ptr->next;
    }
}

/*
 * This function generates the report files
 */
void
LA_publish_results(int32_t thrd_id)
{
    int32_t i, low_limit, high_limit;
    float CHR;

    fprintf(DW_TFP[0], "domain name, bws for 58 tunnel codes\n");
    fprintf (CE_FP[0], "domain_name, bucket (KB), bw_saved, cache_size_req,"
			"cache_efficiency, cache_hit_ratio, bytes_tunnel, "
			"bytes_origin, bytes_sas, bytes_sata, bytes_ssd,"
			"bytes_buffer, tot_bytes, duplicate_bw, bw_percentage,"
			"hits_tunnel, hits_origin, hits_sas, hits_ssd, hits_sata,"
			"hits_buffer, tot_hits, tot_hits_percentage,"
			"tot_content_length\n");
    g_hash_table_foreach(g_domain_hash_table[thrd_id], (GHFunc)iterator, &thrd_id);

    if (g_create_uri_wise_report) {
	fprintf(Uriwise_FP[0], "uri_name, domain_name, bandwidth_saved, CHR,"
		"bytes_tunnel, bytes_origin, bytes_sas, bytes_sata, bytes_ssd,"
		"bytes_buffer, tot_bytes_served, bw_percentage, hits_tunnel,"
		"hits_origin, hits_sas, hits_ssd, hits_sata, hits_buffer,"
		"tot_hit_cnt, tot_hit_cnt_percentage\n");

	g_hash_table_foreach(g_uri_hash_table[thrd_id],
		(GHFunc)uri_iterator, &thrd_id);
    }

    /*
     * General Stats
     */
    CHR = ((float)(g_overall_bytes_served_from_sas[thrd_id] +
		g_overall_bytes_served_from_ssd[thrd_id] +
		g_overall_bytes_served_from_sata[thrd_id] +
		g_overall_bytes_served_from_buffer[thrd_id]) /
		(float)g_overall_bytes_served[thrd_id]);

    g_overall_bytes_served_from_cache[thrd_id] =
	    (g_overall_bytes_served_from_sas[thrd_id] + 
	    g_overall_bytes_served_from_ssd[thrd_id] + 
	    g_overall_bytes_served_from_sata[thrd_id] + 
	    g_overall_bytes_served_from_buffer[thrd_id]);

    /*
     * overalll reports file has same stats as consolidated reports
     * but in csv format, this will be useful for merging multiple 
     * reports.
     */
    fprintf (oall_FP, "tot_hits, tot_bytes, tot_bytes_cache, bucket_wise_hits,"
	    "bucket_wise_bytes (13 buckets), buffer_bytes, buffer_hits,"
	    "sas_bytes, sas_hits, ssd_bytes, ssd_hits, sata_bytes, sata_hits,"
	    "origin_bytes, origin_hitss\n");

    fprintf (oall_FP, "%ld, %ld, %ld, ", g_overall_hits[thrd_id],
				 g_overall_bytes_served[thrd_id],
				 g_overall_bytes_served_from_cache[thrd_id]);


    fprintf(con_FP,"General stats:\n--------------\n");
    fprintf(con_FP, "Total Requests handled              : %5ld\n",
	    g_overall_hits[thrd_id]);
    fprintf(con_FP, "Total bytes served                  : %5ld\n",
	    g_overall_bytes_served[thrd_id]);
    fprintf(con_FP, "Total bytes served from origin      : %5ld\n",
	    g_overall_bytes_served_from_origin[thrd_id]);
    fprintf(con_FP, "Total bytes served from cache       : %5ld\n",
	    g_overall_bytes_served_from_cache[thrd_id]);
    fprintf(con_FP, "Cache_hit_ratio                     : %0.5f\n",
	    CHR);
    fprintf(con_FP, "total br requests:			 : %5ld\n",
	    g_tot_br_transactions);
    fprintf(con_FP, "total br rep requests:	         : %5ld\n",
	    g_tot_br_repeat_transactions);
    fprintf(con_FP, "total missed cacheable bytes served today	 : %5ld\n",
	    g_missed_bw_today);
    fprintf(con_FP, "total missed cacheable each > 10mb bytes served today"
	    ": %5ld\n", g_10_dup);

    LA_print_agewise_stats(con_FP);
    /*
     * Tranaction size vs bandwidth savings stats
     */
    fprintf(con_FP, "\nTransaction size wise distribution:\n-------------------"
	    "----------------\n");
    fprintf(con_FP, "Bucket_name\tHit Percentage\tBW Percentage\n");

    for(i=0; i<=g_CE_num_buckets; i++){

	fprintf (oall_FP, "%ld, %ld, ", g_bucket_stats[i].hit_cnt,
					g_bucket_stats[i].bytes_served);
	if(i == 0){
	    low_limit = 0;
	} else {
	    low_limit = g_CE_buckets[i-1];
	}
	if(i < g_CE_num_buckets)
	    high_limit = g_CE_buckets[i];

	if (low_limit < high_limit)
	    fprintf(con_FP,"(%d-%d KB)\t%3.3f\t%3.3f\n",
		    low_limit, high_limit, g_bucket_stats[i] .hit_cnt*100/
		    (float)g_overall_hits[thrd_id],
		    g_bucket_stats[i].bytes_served*100 /
		    (float)g_overall_bytes_served[thrd_id]);
	else
	    fprintf(con_FP,"(>%d KB)\t%3.3f\t%3.3f\n", high_limit,
		    g_bucket_stats[i].hit_cnt*100/(float)g_overall_hits[thrd_id]
		    , g_bucket_stats[i].bytes_served*100/
		    (float)g_overall_bytes_served[thrd_id]);
    }

    fprintf(con_FP, "\nRequests distribution: \n----------------------\n");
    fprintf(con_FP, "Type\t(Hits percentage)\t(bytes served percentage)\n------"
	    "------------- ---------------------\n");
    fprintf(con_FP, "Buffer\t%3.2f\t%3.2f\n",
	    ((float)(g_overall_cnt_buffer[thrd_id] * 100)/
	    (float)g_overall_hits[thrd_id]),
	    ((float)(g_overall_bytes_served_from_buffer[thrd_id] * 100)/
	     (float)g_overall_bytes_served[thrd_id]));
    fprintf(con_FP, "SAS\t%3.2f\t%3.2f\n",
	    (float)(g_overall_cnt_sas[thrd_id]*100)/
	    (float)g_overall_hits[thrd_id],
	    (float)(g_overall_bytes_served_from_sas[thrd_id] * 100)/
	    (float)g_overall_bytes_served[thrd_id]);
    fprintf(con_FP, "SSD\t%3.2f\t%3.2f\n",
	    (float)(g_overall_cnt_ssd[thrd_id]*100)/
	    (float)g_overall_hits[thrd_id],
	    (float)(g_overall_bytes_served_from_ssd[thrd_id] * 100)/
	    (float)g_overall_bytes_served[thrd_id]);
    fprintf(con_FP, "SATA\t%3.2f\t%3.2f\n",
	    (float)(g_overall_cnt_sata[thrd_id]*100)/
	    (float)g_overall_hits[thrd_id],
	    (float)(g_overall_bytes_served_from_sata[thrd_id]
	    * 100)/(float)g_overall_bytes_served[thrd_id]);
    fprintf(con_FP, "Origin\t%3.2f\t%3.2f\n",
	    (float)(g_overall_cnt_origin[thrd_id]*100) /
	    (float)g_overall_hits[thrd_id],
	    (float)(g_overall_bytes_served_from_origin[thrd_id] * 100)/
	    (float)g_overall_bytes_served[thrd_id]);
    fprintf(con_FP, "Tunnel\t%3.2f\t%3.2f\n\n",
	    (float)(g_overall_cnt_tunnel[thrd_id] * 100) /
	    (float)g_overall_hits[thrd_id],
	    (float)(g_overall_bytes_served_from_tunnel[thrd_id] * 100)/
	    (float)g_overall_bytes_served[thrd_id]);

    fprintf(con_FP, "Tunnel stats:\n-------------\n"
	    "Tunnel Response Code\tHits percentage\tbytes "
	    "served percentage\n--------------------------"
	    "------------------------------\n");

    tunnel_cntr_t *ptr = tunnel_head[thrd_id];


    fprintf (oall_FP, "%ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld\n",
		g_overall_bytes_served_from_buffer[thrd_id],
		g_overall_cnt_buffer[thrd_id],
		g_overall_bytes_served_from_sas[thrd_id],
		g_overall_cnt_sas[thrd_id],
		g_overall_bytes_served_from_ssd[thrd_id],
		g_overall_cnt_ssd[thrd_id],
		g_overall_bytes_served_from_sata[thrd_id],
		g_overall_cnt_sata[thrd_id],
		g_overall_bytes_served_from_origin[thrd_id],
		g_overall_cnt_origin[thrd_id]
		);

    while(ptr){
	fprintf(con_FP, "%-30s\t%-10.3f\t%-10.3f\n", ptr->reasoncode,
		    ((float)(ptr->cntr*100)/(float)g_overall_hits[thrd_id]),
		    ((float)(ptr->bw*100)/(float)g_overall_bytes_served[thrd_id]));
	ptr = ptr->next;
    }

    fclose(con_FP);
    fclose(CE_FP[thrd_id]);
    fclose (oall_FP);

    if (g_create_uri_wise_report)
	fclose(Uriwise_FP[thrd_id]);
}

/*
 * This function takes the value of the domain hash table and increments various
 * counters appropriately
 */
void
LA_increment_counters(domain_hash_val_t *dom_val, int32_t flag, char **line_arr)
{
    switch (g_cache_indicator) {
	case origin: INC_BYTES_AND_HITS_VALUES(dom_val, origin, flag); break;
	case tunnel: INC_BYTES_AND_HITS_VALUES(dom_val, tunnel, flag);
		     update_tunnel_stats_dwise(line_arr, dom_val);
		     break;
	case buffer: INC_BYTES_AND_HITS_VALUES(dom_val, buffer, flag); break;
	case sas   : INC_BYTES_AND_HITS_VALUES(dom_val, sas, flag); break;
	case sata  : INC_BYTES_AND_HITS_VALUES(dom_val, sata, flag); break;
	case ssd   : INC_BYTES_AND_HITS_VALUES(dom_val, ssd, flag); break;
	default: break;
    }
}

/*
 * This function takes the value of the uri hash table and increments various
 * counters appropriately
 */
void
LA_increment_uricounters(uri_hash_val_t *uri_val, int32_t flag, char **line_arr)
{
    switch (g_cache_indicator) {
	case origin: INC_BYTES_AND_HITS_VALUES(uri_val, origin, flag); break;
	case tunnel: INC_BYTES_AND_HITS_VALUES(uri_val, tunnel, flag); break;
	case buffer: INC_BYTES_AND_HITS_VALUES(uri_val, buffer, flag); break;
	case sas   : INC_BYTES_AND_HITS_VALUES(uri_val, sas, flag); break;
	case sata  : INC_BYTES_AND_HITS_VALUES(uri_val, sata, flag); break;
	case ssd   : INC_BYTES_AND_HITS_VALUES(uri_val, ssd, flag); break;
	default: break;
    }
}

void
swap_pop_domains (int32_t x, int32_t y)
{
    int32_t temp;
    temp = g_pop_domain_indices_sorted[0][x];
    g_pop_domain_indices_sorted[0][x] = g_pop_domain_indices_sorted[0][y];
    g_pop_domain_indices_sorted[0][y] = temp;
}

/* 
 * swap the elements of the g_domain_list_sorted
 */
void
swap(int32_t x, int32_t y)
{
    int32_t temp;
    temp = g_master_domain_indices_sorted[x];
    g_master_domain_indices_sorted[x] = g_master_domain_indices_sorted[y];
    g_master_domain_indices_sorted[y] = temp;
}

int
choose_pivot(int32_t i, int32_t j )
{
    return((i+j)/2);
}

/* 
 * sorts the elements of the g_domain_list_sorted
 */
void
quicksort(int32_t *list, int32_t m, int32_t n)
{
    int32_t key, key1, key2, i, j, k;

    if (m < n) {
        k = choose_pivot(m,n);
	swap( m, k);
        domain_hash_val_t *dom_val = NULL;
        dom_val = g_hash_table_lookup(g_master_domain_hash_table, \
		    g_master_domain_list[list[m]].domain);
        key = dom_val->tot_bytes_served;
        i = m+1;
        j = n;

	while (i <= j) {
            dom_val = g_hash_table_lookup(g_master_domain_hash_table, \
			g_master_domain_list[list[i]].domain);
            key1 = dom_val->tot_bytes_served;
            dom_val = g_hash_table_lookup(g_master_domain_hash_table, \
			g_master_domain_list[list[j]].domain);
            key2 = dom_val->tot_bytes_served;

	    while ((i <= n) && (key1 >= key)){
                i++;
		if (i > n)
		    break;
                dom_val = g_hash_table_lookup(g_master_domain_hash_table,\
				g_master_domain_list[list[i]].domain);
                key1 = dom_val->tot_bytes_served;
	    }

	    while ((j >= m) && (key2 < key)){
                j--;
		if (j < m)
		    break;
		dom_val = g_hash_table_lookup(g_master_domain_hash_table, \
			    g_master_domain_list[list[j]].domain);
		key2 = dom_val->tot_bytes_served;
	    }

	    if (i < j)
                swap(i, j);
        }
        swap(m, j);

	/* 
	 * recursively sort the lesser list
	 */
        quicksort(list, m, j-1);
        quicksort(list, j+1, n);
    }
}

/*
 * This function gives the bucket_name based on the bytes_served
 */
int
get_bucket_num(int64_t bytes_served)
{
    int32_t i;

    for (i = 0; i < g_CE_num_buckets; i++){
	if (bytes_served/1024 < g_CE_buckets[i]){
		return i;
	} else {
	    if (i == g_CE_num_buckets-1){
		return i+1;
	    }
	}
    }

    return(g_CE_num_buckets);
}

/*
 * This function updates the domain-wise bucket-wise stats
 */
void
LA_update_bucketwise_stats(domain_hash_val_t *dom_val, char **line_arr,
			    int32_t flag, char *cacheable_flag)
{
    char *bytes_srvd_ptr = line_arr[g_bytes_served_index];
    int32_t bucket_num = get_bucket_num(atol(bytes_srvd_ptr));

    if (!(strcasestr(cacheable_flag, "Tunnel") ||
	    !strcasecmp(cacheable_flag, "-") ||
	    !strcasecmp(cacheable_flag, "NON-CACHEABLE"))) {
	/*
	 * first hit, update cache_size
	 */
	if (flag == 0) {
	    dom_val->ce_chr[bucket_num].cache_size_req += atol(bytes_srvd_ptr);
	    if (strcasecmp(cacheable_flag, "origin"))
		dom_val->ce_chr[bucket_num].bw_saved += atol(bytes_srvd_ptr);
	} else {
	    if (strcasecmp(cacheable_flag, "origin"))
		dom_val->ce_chr[bucket_num].bw_saved += atol(bytes_srvd_ptr);
	}

	if(dom_val->ce_chr[bucket_num].cache_size_req)
	    dom_val->ce_chr[bucket_num].CE  =
		(float)(dom_val->ce_chr[bucket_num].bw_saved)/
		(float)(dom_val->ce_chr[bucket_num].cache_size_req);
	else
	    dom_val->ce_chr[bucket_num].CE  = 0;
    }

    LA_increment_bucketwise_counters(&dom_val->ce_chr[bucket_num], 1, line_arr);
}

/*
 * This function inserts the new domain into the hash table
 */
void
LA_insert_domain_hash_table(hash_mem_t **cur_hash_mem_node,
			    char **line_arr, int32_t thrd_id)
{
    int32_t uniq_domain_cnt;
    char *cacheable_flag;
    domain_hash_val_t *dom_val;

    if (g_uniq_domain_cnt[thrd_id]%10 == 0){
	g_dom_list_size[thrd_id] = 20 + g_dom_list_size[thrd_id];
	g_domain_list[thrd_id] = (domain_t *)LA_realloc(g_domain_list[thrd_id],\
				g_dom_list_size[thrd_id] * sizeof(domain_t));
    }

    NEW_DOMAIN_INSERT(g_domain_list[thrd_id], g_uniq_domain_cnt[thrd_id],\
		    line_arr[g_domain_index]);
    uniq_domain_cnt = g_uniq_domain_cnt[thrd_id];
    //g_domain_list[thrd_id][uniq_domain_cnt].index = uniq_domain_cnt;

    (*cur_hash_mem_node)->dom_val = (domain_hash_val_t *) LA_calloc(1, \
				    sizeof(domain_hash_val_t));
    dom_val = (*cur_hash_mem_node)->dom_val;
    cacheable_flag = line_arr[g_cacheable_flag_index];
    LA_increment_counters(dom_val, 0, line_arr);
 
    if(g_print_srcip_domainwise) {
	dom_val->ip_list = (struct srcip *)LA_malloc(sizeof(struct srcip));
	if(dom_val->ip_list){
	    dom_val->ip_list->ip = strdup(line_arr[g_src_ip_index]);
	    dom_val->ip_list->next = NULL;
	} else{
	    fprintf(stderr, "LA_insert_domain_hash_table: Malloc Failed");
	    fflush(stderr);
	}
    }

    insert_replace_hash(g_domain_list[thrd_id][uniq_domain_cnt].domain,\
			 (*cur_hash_mem_node)->dom_val, thrd_id);
    (*cur_hash_mem_node)->next	  = (hash_mem_t *)LA_malloc(sizeof(hash_mem_t));
    (*cur_hash_mem_node)          = (*cur_hash_mem_node)->next;
    g_cur_hash_mem_node		  = *cur_hash_mem_node;
    (*cur_hash_mem_node)->next    = NULL;
    (*cur_hash_mem_node)->dom_val = NULL;
    g_uniq_domain_cnt[thrd_id]++;

    LA_update_bucketwise_stats(dom_val, line_arr, 0, cacheable_flag);

    dom_val->CHR = (float)(dom_val->bytes_served_ssd + dom_val->bytes_served_sas \
		    + dom_val->bytes_served_sata + dom_val->bytes_served_buffer)\
		    /(float)(dom_val->tot_bytes_served);

    /*
     * Allocate memory to store dup uri indices
     */
    dom_val->dup_bw_uri_index_list = (int32_t *)LA_calloc(100, sizeof(int));
}

/*
 * Insert new uri into the hash table
 */
void
LA_insert_uri_hash_table(urihash_mem_t **cur_urihash_mem_node, char **line_arr,
			char *url, int32_t thrd_id)
{
    char *cacheable_flag;
    uri_hash_val_t *uri_val;

    if (g_uniq_uri_cnt[thrd_id]%10 == 0){
	g_uri_list_size[thrd_id] = 20 + g_uri_list_size[thrd_id];
	g_uri_list[thrd_id] = (uri_t *)LA_realloc(g_uri_list[thrd_id], \
		g_uri_list_size[thrd_id] * sizeof(uri_t));
    }

    /*
     * Insert new uri into the hash table and index value in the uri
     * index hash table
     */
    NEW_URI_INSERT(g_uri_list[thrd_id], g_uniq_uri_cnt[thrd_id], url);

    (*cur_urihash_mem_node)->uri_val = (uri_hash_val_t *)
					LA_calloc(1,sizeof(uri_hash_val_t));

    uri_val = (*cur_urihash_mem_node)->uri_val;
    cacheable_flag = line_arr[g_cacheable_flag_index];

    LA_increment_uricounters(uri_val, 0, line_arr);

    uri_val->domain_name = strdup(line_arr[g_domain_index]);
    uri_val->uri_list_index = g_uniq_uri_cnt[thrd_id];

    insert_replace_urihash(g_uri_list[thrd_id][g_uniq_uri_cnt[thrd_id]].uri,\
	    (*cur_urihash_mem_node)->uri_val, thrd_id);

    (*cur_urihash_mem_node)->next    = (urihash_mem_t *)
					LA_malloc(sizeof(urihash_mem_t));
    (*cur_urihash_mem_node)	     = (*cur_urihash_mem_node)->next;
    g_cur_urihash_mem_node	     = *cur_urihash_mem_node;
    (*cur_urihash_mem_node)->next    = NULL;
    (*cur_urihash_mem_node)->uri_val = NULL;
    g_uniq_uri_cnt[thrd_id]++;

    /*
     * repeat objs identification: set the flag if cache miss
     */
    if (g_cache_indicator == origin || g_cache_indicator == sas ||
	g_cache_indicator == ssd || g_cache_indicator == sata||
	g_cache_indicator == buffer) {
	uri_val->prev_origin = 1;
	int status = atoi(line_arr[g_status_index]);
	if ((status == 206 ||
		    g_range_uri.flag)&& g_access_log_has_byte_ranges) {
	    struct inc_values obj;
	    obj.cache_size_req = 0;
	    obj.dup_bw = 0;
	    if (g_range_uri.flag)
		parse_byte_range1(&obj, uri_val,
			    atoll(line_arr[g_bytes_served_index]));
	    else
		parse_byte_range(&obj, uri_val, line_arr);

	    if (obj.dup_bw && (!g_do_peak_time_analysis ||
		(g_do_peak_time_analysis && g_peak_time)))
		g_tot_br_repeat_transactions++;

	}
    }
}

/*
 * This function updates the domain-wise stats for the domain already present
 * in the domain hash table
 */
void
LA_update_domain_hash_table(domain_hash_val_t *dom_val, char **line_arr)
{
    int32_t ip_already_present = 0;
    struct srcip *ptr;

    LA_increment_counters(dom_val, 1, line_arr);
    if (g_print_srcip_domainwise) {
	ptr = dom_val->ip_list;

	while (ptr->next){
	    if (!strcmp(ptr->ip, line_arr[g_src_ip_index])){
		ip_already_present = 1;
		break;
	    }
	    ptr=ptr->next;
	}

	if (!strcmp(ptr->ip, line_arr[g_src_ip_index])){
	    ip_already_present = 1;
	}

	if (!ip_already_present) {
	    ptr->next = (struct srcip *)LA_malloc(sizeof(struct srcip));
	    ptr = ptr->next;
	    if (ptr){
		ptr->ip = strdup(line_arr[g_src_ip_index]);
		ptr->next = NULL;
	    } else {
		fprintf(stderr, "LA_insert_domain_hash_table: Malloc Failed");
		fflush(stderr);
	    }
	}
    }
}

void
LA_update_uri_hash_table(uri_hash_val_t *uri_val, char **line_arr)
{
    LA_increment_uricounters(uri_val, 1, line_arr);
}

/*
 * This function clears the line_arr array of pointers which hold the 
 * various fields of the access log
 */
void
clear_arr(char **line_arr, int32_t field_cnt)
{
    int32_t tmp=0;
    while (tmp < field_cnt){
	if (line_arr[tmp]){
	    free(line_arr[tmp]);
	   line_arr[tmp] =  NULL;
	}
	tmp++;
    }
}

void *
LA_malloc(int64_t size)
{
    void *ptr = (void *)malloc(size);
    int thrd_id = 0;

    if (ptr == NULL){
	for (thrd_id = 0; thrd_id < MAX_THRDS; thrd_id++) {
	    LA_publish_results(thrd_id);
	    fclose(con_FP);
	    fclose(CE_FP[thrd_id]);

	    if (g_create_uri_wise_report)
		fclose(Uriwise_FP[thrd_id]);

	    if (g_print_srcip_domainwise)
		fclose(SRCIP_FP[thrd_id]);
	}
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"malloc error: %d", errno);
	exit(-1);
    }

    return(ptr);
}

void *
LA_calloc(int64_t num, int64_t objsize)
{
    void *ptr = (void *)calloc(num, objsize);
    int thrd_id = 0;

    if (ptr == NULL){
	for (thrd_id = 0; thrd_id < MAX_THRDS; thrd_id++) {
	    LA_publish_results(thrd_id);
	    fclose(con_FP);
	    fclose(CE_FP[thrd_id]);

	    if (g_create_uri_wise_report)
		fclose(Uriwise_FP[thrd_id]);

	    if (g_print_srcip_domainwise) {
		fclose(SRCIP_FP[thrd_id]);
	    }
	}
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"calloc error: %d", errno);
	exit(-1);
    }

    return(ptr);
}

void *
LA_realloc(void *ptr, int64_t objsize)
{
    ptr = (void *)realloc(ptr, objsize);
    int thrd_id = 0;

    if (ptr == NULL){
	for (thrd_id = 0; thrd_id < MAX_THRDS; thrd_id++) {
	    LA_publish_results(thrd_id);
	    fclose(con_FP);
	    fclose(CE_FP[thrd_id]);

	    if (g_create_uri_wise_report)
		fclose(Uriwise_FP[thrd_id]);

	    if (g_print_srcip_domainwise) {
		fclose(SRCIP_FP[thrd_id]);
	    }
	}
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"realloc error: %d", errno);
	exit(-1);
    }

    return(ptr);
}

void *
sig_handler(int sig)
{
    int32_t thrd_id;
    switch (sig) {
	case SIGHUP:
	    break;
	case SIGKILL:
	case SIGTERM:
	case SIGPIPE:
	case SIGINT:
	    for (thrd_id = 0; thrd_id < MAX_THRDS; thrd_id++) {
		LA_publish_results(thrd_id);
		fclose(CE_FP[thrd_id]);

		if (g_create_uri_wise_report)
		    fclose(Uriwise_FP[thrd_id]);

		if (g_print_srcip_domainwise) {
		    fclose(SRCIP_FP[thrd_id]);
		}
	    }
	default:
	    break; 
    }

    exit(1);
}

/*
 * This function updates the tunnel-wise global counters
 */
void
update_tunnel_stats_global(char *cacheable_flag, int32_t thrd_id,
			    char **line_arr)
{
    char *p = strchr(cacheable_flag, '_');
    int32_t tunnel_code;

    if (p)
	tunnel_code = atoi(p+1);
    else
	tunnel_code = 0;

    if (tunnel_code > 29){
	tunnel_code = tunnel_code - TRESP_CODE_GAP;
	if (tunnel_code > MAX_TUNNEL_RESP_CODES)
	    tunnel_code = 0;
    }

    tunnel_cntr_t *ptr = tunnel_head[thrd_id];

    if (ptr){
	while (strcasecmp(ptr->reasoncode, tunnel_resp_codes[tunnel_code])){
	    if (!ptr->next){
		ptr->next = (tunnel_cntr_t *)
				LA_calloc (1, sizeof(tunnel_cntr_t));
		ptr = ptr->next;
		ptr->reasoncode = strdup(tunnel_resp_codes[tunnel_code]);
		break;
	    }
	    ptr = ptr->next;
	}
    } else {
	tunnel_head[thrd_id] = (tunnel_cntr_t *)
				    LA_calloc (1, sizeof(tunnel_cntr_t));
	ptr = tunnel_head[thrd_id];
	ptr->reasoncode = strdup(tunnel_resp_codes[tunnel_code]);
    }

    ptr->cntr++;
    ptr->bw += atoll(line_arr[g_bytes_served_index]);
}

/*
 * This function increments the domainwise tunnel counters
 */
void
update_tunnel_stats_dwise (char **line_arr, \
			    domain_hash_val_t *dom_val)
{
    char *cacheable_flag = line_arr[g_cacheable_flag_index];
    char *p = strchr(cacheable_flag, '_');
    int32_t tunnel_code;

    if (p)
	tunnel_code = atoi(p+1);
    else
	tunnel_code = 0;

    if(tunnel_code > 29){
	tunnel_code = tunnel_code - TRESP_CODE_GAP;
	if(tunnel_code > MAX_TUNNEL_RESP_CODES)
	    tunnel_code = 0;
    }

    dom_val->dw_tun[tunnel_code].cntr++;
    dom_val->dw_tun[tunnel_code].bw += atoll(line_arr[g_bytes_served_index]);
}

void
LA_increment_bucketwise_counters(CE_CHR_t *ptr, int32_t flag, char **line_arr)
{
    switch (g_cache_indicator) {
	case origin: INC_BYTES_AND_HITS_VALUES(ptr, origin, flag); break;
	case tunnel: INC_BYTES_AND_HITS_VALUES(ptr, tunnel, flag); break;
	case buffer: INC_BYTES_AND_HITS_VALUES(ptr, buffer, flag); break;
	case sas   : INC_BYTES_AND_HITS_VALUES(ptr, sas, flag); break;
	case sata  : INC_BYTES_AND_HITS_VALUES(ptr, sata, flag); break;
	case ssd   : INC_BYTES_AND_HITS_VALUES(ptr, ssd, flag); break;
	default: break;
    }
}

int
check_if_dup_uri_already_present(domain_hash_val_t *dom_val, int32_t value,\
				int32_t start_index, int32_t end_index) 
{
    int32_t i;
    for (i = start_index; i < end_index; i++){
	if (value == dom_val->dup_bw_uri_index_list[i])
	    return 1;
    }
    return 0;
}

void
LA_parse_config_file(char *config_file)
{
    char line[5000], field[500], value[500], equals[10];
    int32_t line_no = 0;
    int32_t ret;
    FILE *FP = fopen(config_file, "r");

    if (FP == NULL) {
	log_error ("File open error", __FILE__, __LINE__);
    }

    while (fgets(line, sizeof(line), FP)) {
	line_no++;
	if ((line[0] == '\n') || (line[0] == '#') || (line[0] == ';')) {
	    continue;
	}
	ret = sscanf(line, "%s%s%s", field, equals, value);
	if ((ret < 3) || (equals[0] != '=')) {
	    log_error ("Invalid configuration line", __FILE__, __LINE__);
	}

	if (!strcmp(field, "enable_tool")) {
	    if (!strcasecmp (value, "no"))
		exit (-1);
	} else if (!strcmp(field, "bytes_served_index")) {
	    g_bytes_served_index = atoi(value);
	} else if (!strcmp(field, "status_index")) {
	    g_status_index = atoi(value);
	} else if (!strcmp(field, "served_from_index")) {
	    g_cacheable_flag_index = atoi(value);
	} else if (!strcmp(field, "domain_index")) {
	    g_domain_index = atoi(value);
	} else if (!strcmp(field, "uri_index")) {
	    g_uri_index = atoi(value);
	} else if (!strcmp(field, "namespace_index")) {
	    g_namespace_index = atoi(value);
	} else if (!strcmp(field, "group_tproxy_domains")) {
	     if (!strcasecmp (value, "yes"))
                g_group_tproxy_domains = 1;
            else
                g_group_tproxy_domains = 0;
	} else if (!strcmp(field, "src_ip_index")) {
	    g_src_ip_index = atoi(value);
	} else if (!strcmp(field, "content_length_index")) {
	    g_content_length_index = atoi(value);
	} else if (!strcmp(field, "method_index")) {
	    g_method_index = atoi(value);
	} else if (!strcmp(field, "byte_range_index")) {
	    g_byte_range_index = atoi(value);
	} else if (!strcmp(field, "age_index")) {
	    g_age_index = atoi(value);
	} else if (!strcmp(field, "size_threshold")) {
	    g_size_threshold = atoi(value);
	} else if (!strcmp(field, "max_process_memory")) {
	    g_max_process_memory = atoll(value);
	} else if (!strcmp(field, "max_reports_count")) {
	    g_max_reports_cnt = atoi(value);
	} else if (!strcmp(field, "log_wait_period")) {
	    g_log_wait_period = atoi(value);
	} else if (!strcmp(field, "only_top_domains_report")) {
	    if (!strcasecmp (value, "yes"))
		g_only_top_domains_report = 1;
	    else
		g_only_top_domains_report = 0;
	} else if (!strcmp(field, "origin_bytes_configured")) {
	    if (!strcasecmp (value, "yes"))
		g_origin_bytes_configured = 1;
	    else
		g_origin_bytes_configured = 0;
	} else if (!strcmp(field, "create_uri_wise_report")) {
	    if (!strcasecmp (value, "yes"))
		g_create_uri_wise_report = 1;
	    else
		g_create_uri_wise_report = 0;
	} else if (!strcmp(field, "only_youtube_googlevideo_analysis")) {
	    if (!strcasecmp (value, "yes"))
		g_only_youtube_googlevideo = 1;
	    else
		g_only_youtube_googlevideo = 0;
	} else if (!strcmp(field, "print_md5_uri_map")) {
	    if (!strcasecmp (value, "yes"))
		g_print_md5_uri_map = 1;
	    else
		g_print_md5_uri_map = 0;
	} else if (!strcmp(field, "treat_all_as_origin")) {
	    if (!strcasecmp (value, "yes"))
		g_treat_all_cacheable_as_origin = 1;
	    else
		g_treat_all_cacheable_as_origin = 0;
	} else if (!strcmp(field, "treat_tunnel_as_origin")) {
	    if (!strcasecmp (value, "yes"))
		g_treat_tunnel_as_origin = 1;
	    else
		g_treat_tunnel_as_origin = 0;
	} else if (!strcmp(field, "get_agewise_stats")) {
	    if (!strcasecmp (value, "yes"))
		g_get_agewise_stats = 1;
	    else
		g_get_agewise_stats = 0;
	}
    }
}

/*
 * This function parses the entreis in the access log and 
 * populates the g_pop_domain_hash_table
 */
void
LA_populate_top_domain_list(char *acclog, int32_t thrd_id)
{
    char line[5000], *line_arr[MAX_ACCESS_LOG_FIELDS], *domain, command[50];
    int32_t tmp = 0;
    int32_t field_cnt;
    pop_hash_mem_t *cur_hash_mem_node = g_pop_cur_hash_mem_node;
    pop_domain_hash_val_t *dom_val = NULL;
    FILE *fp;

    while (tmp < MAX_ACCESS_LOG_FIELDS){
	line_arr[tmp] =  NULL;
	tmp++;
    }
    if (g_live_mode){
	fp = popen("zcat -d ../access.gz", "r");
    } else {
	if (strstr(acclog, ".gz")) {
	    sprintf (command, "zcat -d %s", acclog);
	    fp = popen(command, "r");
	} else
	    fp = fopen(acclog, "r");
    }

    if (fp)
    {
	while (fgets(line, sizeof(line), fp)){
	    /* 
	    * split the access log entry and populate line_arr
	    */
	    field_cnt = LA_line_split(line_arr, line);

	    /*
	    * Parse the header line and get the index values
	    */
	    if (line_arr[0] && strcasestr(line_arr[0], "Fields")){
		LA_parse_access_log_header(line_arr);
	    }

	    if (LA_filter_entry (line_arr)) {
		clear_arr(line_arr, field_cnt);
		continue;
	    }

	    domain = line_arr[g_domain_index];

	    dom_val = g_hash_table_lookup(
		    g_pop_domain_hash_table[thrd_id], line_arr[g_domain_index]);
	    if (dom_val == NULL) {
		g_pop_domain_indices_sorted[thrd_id][
			g_uniq_pop_domain_cnt[thrd_id]] = g_uniq_pop_domain_cnt[
							    thrd_id];

		if (g_uniq_pop_domain_cnt[thrd_id]%10 == 0){
		    g_pop_dom_list_size[thrd_id] = 20 +
				g_pop_dom_list_size[thrd_id];

		    g_pop_domain_list[thrd_id] = (domain_t *)
			LA_realloc(g_pop_domain_list[thrd_id], 
			    g_pop_dom_list_size[thrd_id] * sizeof(domain_t));

		    g_pop_domain_indices_sorted[thrd_id] = (int32_t *)
			LA_realloc(g_pop_domain_indices_sorted[thrd_id],
				g_pop_dom_list_size[thrd_id] * sizeof(int));
		}

		g_pop_domain_list[thrd_id][
		    g_uniq_pop_domain_cnt[thrd_id]].domain = strdup(domain);

		cur_hash_mem_node->dom_val =
		    (pop_domain_hash_val_t *)LA_calloc(1,
						sizeof(pop_domain_hash_val_t));
		/*
		* Popular domains are decided based on bytes served.
		*/
		cur_hash_mem_node->dom_val->bytes_served =
			    atoll(line_arr[g_bytes_served_index]);

		g_hash_table_insert( g_pop_domain_hash_table[thrd_id],
			g_pop_domain_list[thrd_id][g_uniq_pop_domain_cnt[
			thrd_id]].domain, cur_hash_mem_node->dom_val);

		cur_hash_mem_node->next	= (pop_hash_mem_t *)LA_malloc(
						sizeof(pop_hash_mem_t));
		cur_hash_mem_node = cur_hash_mem_node->next;
		g_pop_cur_hash_mem_node	= cur_hash_mem_node;
		cur_hash_mem_node->next	= NULL;
		cur_hash_mem_node->dom_val = NULL;
		g_uniq_pop_domain_cnt[thrd_id]++;

	    } else {
		dom_val->bytes_served += atoll(line_arr[g_bytes_served_index]);
	    }

	    clear_arr(line_arr, field_cnt);
	}
    }
}

/*
 * This function sorts the g_pop_domain_indices_sorted list according to
 * the bandwidth top g_num_pop_domains are picked based on the sorted list and
 * analysis is done only on these domains.
 */
void
quicksort_pop_domains(int32_t *list, int32_t m, int32_t n)
{
    int32_t key, key1, key2, i, j, k;

    if (m < n) {
        k = choose_pivot(m,n);
	swap_pop_domains( m, k);
        pop_domain_hash_val_t *dom_val = NULL;
        dom_val = g_hash_table_lookup(g_pop_domain_hash_table[0], 
		    g_pop_domain_list[0][list[m]].domain);
        key = dom_val->bytes_served;
        i = m+1;
        j = n;

	while (i <= j) {
            dom_val = g_hash_table_lookup(g_pop_domain_hash_table[0],
			 g_pop_domain_list[0][list[i]].domain);
            key1 = dom_val->bytes_served;
            dom_val = g_hash_table_lookup(g_pop_domain_hash_table[0],
			g_pop_domain_list[0][list[j]].domain);
            key2 = dom_val->bytes_served;
            while ((i <= n) && (key1 >= key)){
                i++;
		if (i > n)
		    break;
                dom_val = g_hash_table_lookup(g_pop_domain_hash_table[0],
			    g_pop_domain_list[0][list[i]].domain);
                key1 = dom_val->bytes_served;
	    }

	    while ((j >= m) && (key2 < key)){
                j--;
		if (j < m)
		    break;
		dom_val = g_hash_table_lookup(g_pop_domain_hash_table[0],
				g_pop_domain_list[0][list[j]].domain);
		key2 = dom_val->bytes_served;
	    }
            if (i < j)
                swap_pop_domains(i, j);
        }

        swap_pop_domains(m, j);
        /*
	 * recursively sort the lesser list
	 */
        quicksort_pop_domains(list, m, j-1);
        quicksort_pop_domains(list, j+1, n);
    }
}

/*
 * This function checks if the input domain is one of the popular domains*/
int
check_if_top_domain(char *domain)
{
	gpointer ptr = g_hash_table_lookup(g_pop_domains, domain);
	if (ptr)
	    return 1;
	else
	    return 0;
}

/*
 * This function checks for access.*.gz files in the g_access_log_path
 * and populates in g_acclog_list
 */
void
LA_get_access_log_list(char *acclog_path)
{
    char command[200], *line;
    FILE *fp;
    acclog_name_t *curnode = g_acclog_list;
    int32_t len;

    sprintf(command, "ls %s|grep gz| grep access > file.txt", acclog_path);
    system(command);

    if ((fp = fopen("file.txt", "r")) != NULL) {

	line = (char *)LA_calloc(200, 1);
	curnode = g_acclog_list;

	while(fgets(line, 200, fp)) {
	    len =strlen(line);

	    if (line[len-1] == '\n') {
		line[len-1] = '\0';
	    }

	    if(!g_acclog_list) {
		g_acclog_list = (acclog_name_t *)LA_calloc(1,
						    sizeof(acclog_name_t));
		curnode = g_acclog_list;
		curnode->log_name = strdup(line);
		curnode->next = NULL;
	    } else {
		curnode->next = (acclog_name_t *)LA_calloc(1,
						    sizeof(acclog_name_t));
		curnode = curnode->next;
		curnode->log_name = strdup(line);
		curnode->next = NULL;
	    }
	}
	fclose(fp);
	sprintf(command, "rm -rf file.txt");
	system(command);
    }
}

/*
 * Init various data structures and report files
 */
void
LA_init()
{
    int32_t thrd_id;
    /*
    * buffers to store various output file names.
    */
    char CE_file[100], ip_list_file[100],
	 tunnel_file[100], consolidated_file[100],
	 repeat_miss_file[100], md5urimap_file[100],
	 uriwise_file[100], overall_file[100];

    sprintf (g_outdir, "/var/log/report");

    create_outdir(g_outdir);
    LA_get_hostname();

    for (thrd_id = 0; thrd_id < MAX_THRDS; thrd_id++) {
	sprintf(CE_file, "%s/%s_domainwise_stats.csv", g_outdir, g_hostname);
	sprintf(tunnel_file, "%s/%s_tunnel_stats.csv", g_outdir, g_hostname);
	sprintf(ip_list_file,"%s/%s_ip_list_domainwise.csv", g_outdir,
							     g_hostname);
	sprintf(repeat_miss_file,"%s/%s_repeat_miss_stats.csv", g_outdir,
								g_hostname);
	sprintf(consolidated_file, "%s/%s_consolidated_stats.txt", g_outdir,
								g_hostname);
	sprintf(md5urimap_file, "%s/%s_md5urimap_file.csv", g_outdir,
								g_hostname);
	sprintf(overall_file, "%s/%s_overall_stats.csv", g_outdir, g_hostname);

	if (g_create_uri_wise_report)
	    sprintf(uriwise_file, "%s/%s_uriwise_file.csv", g_outdir,
							    g_hostname);

	if ((con_FP = fopen(consolidated_file, "w")) == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	if ((CE_FP[thrd_id]= fopen(CE_file, "w")) == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	if ((md5urimapFP = fopen(md5urimap_file, "w")) == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	if ((DW_TFP[thrd_id]= fopen(tunnel_file, "w")) == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	if ((dupFP[thrd_id] = fopen(repeat_miss_file,"w")) == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	if ((oall_FP = fopen(overall_file,"w")) == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	if (g_create_uri_wise_report) {
	    if ((Uriwise_FP[thrd_id]= fopen(uriwise_file, "w")) == NULL)
		log_error ("file open failed", __FILE__, __LINE__);
	}

	if (g_print_srcip_domainwise) {
	    if ((SRCIP_FP[thrd_id] = fopen(ip_list_file, "w")) == NULL)
		log_error ("file open failed", __FILE__, __LINE__);
	}

	create_hash_table(thrd_id);
	g_lru_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	/*
	 *  Memory pool to store domain_hash values
	 */
	g_hash_mem[thrd_id] = (hash_mem_t *)LA_malloc(sizeof(hash_mem_t)); 
	g_hash_mem[thrd_id]->next    = NULL;
	g_cur_hash_mem_node = g_hash_mem[thrd_id];
	g_hash_mem[thrd_id]->dom_val = NULL;
	/*
	 *  Memory pool to store uri hash values
	 */
	g_urihash_mem[thrd_id] = (urihash_mem_t *)LA_malloc(
						    sizeof(urihash_mem_t));
	g_urihash_mem[thrd_id]->next    = NULL;
	g_cur_urihash_mem_node = g_urihash_mem[thrd_id];
	g_urihash_mem[thrd_id]->uri_val = NULL;
	/*
	 * Memory pool to store keys of domain hash table
	 */
	g_domain_list[thrd_id] = (domain_t *)LA_calloc(10, sizeof(domain_t));
	/*
	 * Memory pool to store keys of uri hash table
	 */
	g_uri_list[thrd_id] = (uri_t *)LA_calloc(10, sizeof(uri_t));
	g_uniq_domain_cnt[thrd_id] = 0;
	g_uniq_uri_cnt[thrd_id]    = 0;
	g_line_cnt[thrd_id]	   = 0;
	/* 
	 * Memory pool to store pop_domain_hash values
	 */
	g_pop_hash_mem[thrd_id] = (pop_hash_mem_t *)LA_malloc(
						    sizeof(pop_hash_mem_t));
        g_pop_hash_mem[thrd_id]->next    = NULL;
	g_pop_hash_mem[thrd_id]->dom_val = NULL;
	g_pop_cur_hash_mem_node = g_pop_hash_mem[thrd_id];
	g_pop_domain_list[thrd_id] = (domain_t *)LA_calloc(10,
						    sizeof(domain_t));
	g_pop_domain_indices_sorted[thrd_id] = (int32_t *)LA_calloc(10,
							    sizeof(int));
	/*
	 * New hash table to keep track of domain vs hits
	 */
	g_pop_domain_hash_table[thrd_id] = g_hash_table_new(g_str_hash,
							g_str_equal);
    }
}

void
LA_get_hostname()
{
    struct stat st;
    FILE *fp;
    char command[220];
    char tmp_file[200];

    snprintf (tmp_file, 200, "%s/file.txt", g_outdir);
    sprintf (command, "hostname > %s", tmp_file);
    system (command);

    if (stat(tmp_file, &st) == 0) {
	fp = fopen(tmp_file, "r");

	if (fp == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);

	g_hostname = (char *)LA_calloc(st.st_size, 1);
	fgets(g_hostname, st.st_size, fp);
	if (g_hostname[strlen(g_hostname)-1] == '\n')
	    g_hostname[strlen(g_hostname)-1] = '\0';
	fclose(fp);
    } else {
	log_error ("file open failed", __FILE__, __LINE__);
    }
}


void
LA_clean_up()
{
    char command[30];
    sprintf(command, "rm -rf acclog");
    system(command);
}

/*
 * Remove the entries from the uri hash table based on the lru list 
 * (free max 10000 (app. 20KB) entries at a time )
 */
void
LA_create_free_space()
{
    int32_t bytes_freed = 0, i = 20000;
    lru_uri_t *next_node;
    uri_hash_val_t *uri_val;

    while (i && (g_lru_uri_list_start_node != g_lru_uri_list_end_node)) {

	if (g_lru_uri_list_start_node){
	    next_node = g_lru_uri_list_start_node->next;

	    /*
	     * free the entry corresponding to this uri from the g_uri_hash_table
	     */
	    if (g_lru_uri_list_start_node->uri) {
		uri_val = g_hash_table_lookup(g_uri_hash_table[0],\
			    g_lru_uri_list_start_node->uri);

		if (uri_val) {
		    bytes_freed += strlen(g_lru_uri_list_start_node->uri);
		    g_hash_table_remove(g_uri_hash_table[0], \
				    g_lru_uri_list_start_node->uri);
		    free(g_uri_list[0][uri_val->uri_list_index].uri);
		    g_uri_list[0][uri_val->uri_list_index].uri = NULL;
		    bytes_freed += sizeof(uri_val);
		    free(uri_val);
		    uri_val = NULL;
		}

		bytes_freed += strlen(g_lru_uri_list_start_node->uri);
		g_hash_table_remove(g_lru_hash_table,\
				    g_lru_uri_list_start_node->uri);
		free(g_lru_uri_list_start_node->uri);
	    }
	    g_lru_deleted_nodes++;
	    free(g_lru_uri_list_start_node);
	    g_lru_uri_list_start_node = next_node;
	    i--;
	} else {
	    g_lru_uri_list_start_node = NULL;
	    g_lru_uri_list_end_node = NULL;
	    return;
	}
    }

    if (g_lru_uri_list_start_node == g_lru_uri_list_end_node)
	g_lru_list_empty = 1;
    else {
	g_lru_list_empty =0;
    }
    return;
}

/* #Fields: %c %h %V %u %t "%r" %s %b %N %y %E "%{Age}o" "%{Range}i"
 * "%{Content-Length}o" "%{Pragma}i" "%{Cache-Control}i" "%{Etag}o"
 */
void
LA_parse_access_log_header(char **line_arr)
{
    int32_t i = 0;
    char *field;
    /*
     * to take care of spaces as well
     */
    int32_t actual_index = i-1;

    g_cacheable_flag_index = g_domain_index = g_uri_index = \
		    g_content_length_index = g_src_ip_index = \
		    g_bytes_served_index = g_status_index = -1;

    while (i < MAX_ACCESS_LOG_FIELDS){
	field = line_arr[i];
	if (field){
	    if (strcasestr(field, "Fields")) {
		actual_index++;
	    } else if (!strcasecmp(field, "\%c")) {
		g_cacheable_flag_index = actual_index;
		actual_index++;
	    } else if (!strcasecmp(field,"%V")) {
		g_domain_index = actual_index;
		actual_index++;
	    } else if(!strcasecmp(field,"\"%r\"")){
		g_uri_index = actual_index+1;
		/*
		 * uri portion has three subfields
		 */
		actual_index += 3;
	    } else if (!strcasecmp(field,"%h")) {
		g_src_ip_index = actual_index;
		actual_index++;
	    } else if (!strcasecmp(field,"\%u")) {
		actual_index++;
	    } else if (!strcasecmp(field,"%t")) {
		/*
		 * time stamp has two subfields
		 */
		actual_index += 2;
	    } else if (!strcasecmp(field,"\%s")) {
		g_status_index = actual_index;
		actual_index++;
	    } else if (!strcasecmp(field,"\%b")) {
		g_bytes_served_index = actual_index;
		actual_index++;
	    } else if (!strcasecmp(field,"%N")) {
		actual_index++;
	    } else if (!strcasecmp(field,"%y")) {
		actual_index++;
	    } else if (!strcasecmp(field,"\%E")) {
		actual_index++;
	    } else if (!strcasecmp(field,"\"%{Age}o\"")) {
		actual_index++;
	    } else if (!strcasecmp(field,"\"%{Range}i\"")) {
		actual_index++;
	    } else if (!strcasecmp(field,"\"%{Content-Length}o\"")) {
		g_content_length_index = actual_index;
		actual_index++;
	    } else if (!strcasecmp(field,"\"%{Pragma}i\"")) {
		actual_index++;
	    } else if (!strcasecmp(field,"\"%{Cache-Control}i\"")) {
		actual_index++;
	    } else if (!strcasecmp(field,"\"%{Etag}o\"")) {
		actual_index++;
	    }
	}
	i++;
    }
}

/*
 * Maintain a LRU list of 1-hit objects keep the index in the hash table for
 * easy traversing
 */
void
LA_insert_LRU_list(char *uri)
{
    if (!g_lru_uri_list_start_node)
    {
	g_lru_uri_list_start_node = (lru_uri_t *)LA_calloc(1,
						    sizeof(lru_uri_t));
	g_lru_uri_list_start_node->uri = strdup(uri);
	g_lru_uri_list_start_node->index = g_lru_cur_node_num;
	g_hash_table_insert(g_lru_hash_table, g_lru_uri_list_start_node->uri,\
			    &(g_lru_uri_list_start_node->index));
	g_lru_cur_node_num++;
	g_lru_uri_list_end_node = g_lru_uri_list_start_node;
	g_lru_uri_list_start_node->next = NULL;
    } else {
	g_lru_uri_list_end_node->next = (lru_uri_t *)LA_calloc(1,
							sizeof(lru_uri_t));
	g_lru_uri_list_end_node = g_lru_uri_list_end_node->next;
	g_lru_uri_list_end_node->uri = strdup(uri);
	g_lru_uri_list_end_node->index = g_lru_cur_node_num;
	g_hash_table_insert(g_lru_hash_table, g_lru_uri_list_end_node->uri,\
			    &(g_lru_uri_list_end_node->index));
	g_lru_cur_node_num++;
	g_lru_uri_list_end_node->next = NULL;
	g_lru_list_empty = 0;
    }
}

/*
 * If the uri hash more than 1 hit remove the uri from the 1-hit lru list and
 * lru hash table. drawback: 20 byte holes are left. will get cleaned up when
 * evicting the uris when memory limit is hit
 */
void
LA_update_LRU_list(char *uri){
    int64_t *index;
    int32_t i;
    lru_uri_t *cur_node;

    index = g_hash_table_lookup(g_lru_hash_table, uri);

    if (index){
        i = *index - g_lru_deleted_nodes;
        cur_node = g_lru_uri_list_start_node;

        while (i){
	    /*
	   if (cur_node == g_lru_uri_list_end_node){
		fprintf(stdout, "End node reached\n");
		fflush(stdout);
	   }
	   */
	    cur_node = cur_node->next;
	    i--;
	}

        g_hash_table_remove(g_lru_hash_table, uri);
        free(cur_node->uri);
        cur_node->uri = NULL;
    }
}

/*
 * This function is run in a thread, It checks if the virtual memory usage
 * crossed a configured g_max_process_memory and triggers LA_create_free_space 
 * to clear the space based on the LRU list
 */
void *
memory_tracker(void *data)
{
    int32_t mem = 0;
    pid_t pid= *((pid_t *)data);

    while (!g_end_run){
	mem = get_cur_memory(pid);
	pthread_mutex_lock(&lru_mutex);

	while (mem > g_max_process_memory && !g_lru_list_empty){
	    LA_create_free_space();
	    mem = get_cur_memory(pid);
	}
	pthread_mutex_unlock(&lru_mutex);
	sleep(10);
    }

    return (NULL);
}

/*
 * This function checks for the current virtual memory usage by the process and
 * returns
 */
int
get_cur_memory(pid_t pid)
{
    char command[250], line[50];
    FILE *fp;
    char tmp_file[200];

    snprintf (tmp_file, 200, "%s/memfile.txt", g_outdir);
    sprintf(command, "pmap %d| tail -1|awk -F \" \" \'{print $2}\' > %s", pid,
	    tmp_file);
    system(command);

    fp = fopen(tmp_file, "r");

    if (fp == NULL)
	log_error ("file open failed", __FILE__, __LINE__);

    memset(line, 0, 50);
    fgets(line , 50, fp);
    fclose(fp);

    return(atoi(line) * 1024);
}

/* Check Max 10 times for new set of gzipped access logs in every 15s interval
 * populate new logs in g_acclog_list_new subsequently remove old entries in
 * g_acclog_list and point g_acclog_list to g_acclog_list_new
 */
int
check_for_new_logs(char *acclog_path)
{
    int32_t new_logs = 0, new_log_attempts = 10, len, old_entry;
    char command[250], *line;
    FILE *fp;
    char tmp_file[200];

    line = (char *)LA_calloc(200, 1);

    snprintf (tmp_file, 200, "%s/file.txt", g_outdir);
    sprintf(command, "ls %s|grep gz| grep access > %s", acclog_path, tmp_file);

    acclog_name_t *curnode = g_acclog_list_new;
    acclog_name_t *p = g_acclog_list;
    /*
     * check if atleast 5 new logs are generated in a 150s interval
     */
    while(new_log_attempts){
	system(command);
	fp = fopen(tmp_file, "r");

	while(fgets(line, 200, fp)) {

	    len =strlen(line);

	    if (line[len-1] == '\n'){
		line[len-1] = '\0';
	    }
	    old_entry = 0;

	    while(p){
		if (!strcasecmp(line, p->log_name)){
		    old_entry = 1;
		    break;
		}
		p = p->next;
	    }
	    if(old_entry)
		continue;
	    new_logs++;
	}
	fclose(fp);
	if (new_logs > 5)
	    break;
	new_log_attempts--;
    }

    if(!new_log_attempts)
	return 0;

    /*
     * Populate the new logs in g_acclog_list_new
     */
    fp = fopen(tmp_file, "r");
    if (fp == NULL)
	log_error ("file open failed", __FILE__, __LINE__);

    curnode = g_acclog_list_new;
    p = g_acclog_list;

    while(fgets(line, 200, fp)) {
	len = strlen(line);

	if (line[len-1] == '\n') {
	    line[len-1] = '\0';
	}

	old_entry = 0;

	while (p) {
	    if (!strcasecmp(line, p->log_name)) {
		old_entry = 1;
		break;
	    }
	    p = p->next;
	}

	if (old_entry)
	    continue;

	if (!g_acclog_list_new) {
	    g_acclog_list_new = (acclog_name_t *)LA_calloc(1,
						    sizeof(acclog_name_t));
	    curnode = g_acclog_list_new;
	    curnode->log_name = strdup(line);
	    curnode->next = NULL;
	} else {
	    curnode->next = (acclog_name_t *)LA_calloc(1,
						sizeof(acclog_name_t));
	    curnode = curnode->next;
	    curnode->log_name = strdup(line);
	    curnode->next = NULL;
	}
    }

    sprintf(command, "rm -rf %s", tmp_file);
    system(command);
    /*
     * Clear old logs list and point g_acclog_list to new logs list
     */
    p = g_acclog_list;

    while (g_acclog_list) {
	p = p->next;
	free(g_acclog_list);
	g_acclog_list = p;
    }

    g_acclog_list = g_acclog_list_new;
    g_acclog_list_new = NULL;
    return 1;
}

/*
 * Combine multiple .gz files into one
 */
void
LA_combine_multiple_access_logs()
{
    char command[2000];
    acclog_name_t *cur_log = g_acclog_list;
    sprintf (command, "%s ", "cat");

    while(cur_log){
	strcat(command,g_access_log_path);
	strcat(command,"/");
	strcat(command, cur_log->log_name);
	strcat(command, " ");
	cur_log = cur_log->next;
    }

    strcat(command, " >");
    strcat(command,g_access_log_path);
    strcat(command,"/");
    strcat(command, "access.gz");
    system(command);
}

/*
 * This function sorts the g_pop_domain_indices_sorted list 
 * based on the bytes served and populates top g_num_pop_domains in 
 * g_pop_domains hash tablei
 */
void
LA_create_pop_domain_list()
{
    int32_t thrd_id = 0;
    int32_t tmp;
    pop_domain_hash_val_t *dom_val = NULL;
    pop_hash_mem_t *cur_hash_mem_node, *next_node;

    /* 
     * sort domains based on hit count
     * 0 is the thrd id. currently only one thread is used
     * */
    quicksort_pop_domains(g_pop_domain_indices_sorted[0], 0,
				    g_uniq_pop_domain_cnt[0] - 1);

    /* 
     * copy the top g_pop_domains from g_pop_domain_list
     * to g_pop_domains list and free all the data structures.
     */
    g_pop_domains = g_hash_table_new(g_str_hash, g_str_equal); 

    for (tmp = 0; tmp < g_uniq_pop_domain_cnt[0]; tmp++) {
	if (tmp < g_num_pop_domains) {
	    g_hash_table_insert(g_pop_domains, g_pop_domain_list[0]
			[g_pop_domain_indices_sorted[0][tmp]].domain, &xxxx);
	}
	/* 
	 * free value
	 */
	dom_val = g_hash_table_lookup(g_pop_domain_hash_table[0],
		g_pop_domain_list[0][g_pop_domain_indices_sorted[0][tmp]].
		domain);
	free(dom_val);
	/*
	 * free key
	 */
	if (tmp > g_num_pop_domains)
	    free(g_pop_domain_list[0]
			[g_pop_domain_indices_sorted[0][tmp]].domain);
    }
    cur_hash_mem_node = g_pop_hash_mem[thrd_id];

    while (cur_hash_mem_node) {
	next_node = cur_hash_mem_node->next;
	free(cur_hash_mem_node);
	cur_hash_mem_node = next_node;
    }

    g_hash_table_destroy(g_pop_domain_hash_table[0]);
}

void
usage(void)
{
    printf ("Usage: \n");
    printf ("-f <name>      : configuration file \n");
    printf ("-m <mode>      : live: 1, offline: 0\n");
    printf ("-p <path/file> : live_mode : specify accesslog path\n");
    printf ("                 offline_mode : specify accesslog name\n");
    printf ("-l <access log list_name>\n");
    printf ("-h             : show help\n"); 
    exit(-1);
}

void
create_outdir (char *dir)
{
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    char *buf = (char *) LA_calloc(200, 1);
    struct stat st;
    uint8_t cur_report_num;
    char command[200];

    tmp = localtime(&t);

    if (strftime(buf, 200, "%F_%T", tmp) == 0) {
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"strftime returned 0: %d", errno);
	exit(EXIT_FAILURE);
    }

    if (stat("/var", &st) == -1){
	if (mkdir ("/var", S_IRWXU) == -1) {
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"mkdir returned -1: %d", errno);
	    exit(EXIT_FAILURE);
	}
    }

    if (stat ("/var/log", &st) == -1) {
	if (mkdir ("/var/log", S_IRWXU) == -1) {
	    DBG_LOG(ERROR, MOD_ANY_MOD,
		"mkdir returned -1: %d", errno);
	    exit(EXIT_FAILURE);
	}
    }

    if (stat ("/var/log/report", &st) == -1) {
	if (mkdir ("/var/log/report", S_IRWXU) == -1) {
	    DBG_LOG(ERROR, MOD_ANY_MOD,
		"mkdir returned -1: %d", errno);
	    exit(EXIT_FAILURE);
	}
    }
    strcat (dir, "/reports_");

    cur_report_num = get_cur_rep_num();

    if (cur_report_num % g_max_reports_cnt == 0) {
	merge_reports();
    }

    if (cur_report_num >= g_max_reports_cnt) {
	snprintf (command, 200, "rm -rf /var/log/report/reports_%d_*",
			cur_report_num - g_max_reports_cnt);
	system (command);
    }

    char temp_buf[10];
    snprintf (temp_buf, 10, "%d_", cur_report_num);

    strcat (dir, temp_buf);
    strcat (dir, buf);

    if (stat (dir, &st) == -1) {
	if (mkdir (dir, S_IRWXU) == -1) {
	    DBG_LOG(ERROR, MOD_ANY_MOD,
		"mkdir returned -1: %d", errno);
	    exit(EXIT_FAILURE);
	}
    }
    free (buf);
}

prov_stats_t *
LA_parse_provider_list (char **line_arr, int32_t field_cnt)
{
    if (!g_origin_bytes_configured)
	return NULL;

    prov_stats_t *ptr = NULL, *p = NULL;
    uint8_t flag = 1;
    /*
     * Parse and get the indices.
     */
    int g_prov_stats_index = field_cnt - 1;
    int g_origin_bytes_index = field_cnt - 1;
    char *cp1, *cp2;
    char *provider, *bytes;

    uint64_t origin_bytes;
    provider = bytes = NULL;

    if (g_prov_stats_index == -1 ||
	    !(strcasestr (line_arr[g_cacheable_flag_index], "BUFFER") ||
	      strcasestr (line_arr[g_cacheable_flag_index], "SATA") ||
	      strcasestr (line_arr[g_cacheable_flag_index], "SSD") ||
	      strcasestr (line_arr[g_cacheable_flag_index], "SAS") ||
	      strcasestr (line_arr[g_cacheable_flag_index], "Origin")
	     )){
	return (NULL);
    }

    //%d usage
    origin_bytes = atol (line_arr[g_prov_stats_index]);

    if (g_origin_bytes_index != -1) {
	if (origin_bytes == 0 || strcasestr(line_arr[g_cacheable_flag_index],
					    "Origin")){
	    return (NULL);
	} else {
	    /* 
	     * split into two nodes, one based on cache_indicator field and 
	     * other as origin
	     */
	    p = (prov_stats_t *) LA_calloc(1, sizeof (prov_stats_t));
	    provider = strdup(line_arr[g_cacheable_flag_index]);
	    p->prov_name = strdup(provider);
	    bytes = (char *) LA_calloc (100, 1);
	    sprintf (bytes, "%ld",
		    atol(line_arr[g_bytes_served_index]) - origin_bytes);
	    p->bytes     = strdup(bytes);
	    ptr = p;
	    free(provider);
	    free(bytes);
	    provider = bytes = NULL;
	    p->next = (prov_stats_t *) LA_calloc(1, sizeof (prov_stats_t));
	    p = p->next;
	    provider = (char *) LA_calloc(7, 1);
	    strcpy(provider, "Origin");
	    p->prov_name = strdup(provider);
	    p->bytes     = strdup(line_arr[g_prov_stats_index]);
	    if (p->bytes[strlen(p->bytes) -1 ] == '\n')
	        p->bytes[strlen(p->bytes) - 1] = 0;
	    free (provider);
	    return (ptr); 
	}
    } else {
	/*
	 * Buffer_100_Origin_200_SAS_10_Origin_900_Buffer_100
	 */
	cp1 = strchr(line_arr[g_prov_stats_index], '_');
	cp2 = line_arr[g_prov_stats_index];

	while (cp1) {
#define FLIP(a) \
	if (a)\
	    a = 0;\
	else \
	    a = 1;

	    FLIP(flag);
	    if (!flag) {
		/* Provider name*/
		provider = (char *) LA_calloc (cp1 - cp2, 1);
		strncpy (provider, cp2, cp1 - cp2);
	    } else if (flag) { 
		/* bytes for this provider. */
		bytes = (char *) LA_calloc (cp1 - cp2, 1);
		strncpy (bytes, cp2, cp1 - cp2);
		/*both provider and bytes are captured.*/
		if (provider != NULL && bytes != NULL){
		    if (!p) {
			p = (prov_stats_t *) LA_calloc(1, sizeof (prov_stats_t));
			p->prov_name = strdup(provider);
			p->bytes     = strdup(bytes);
			ptr = p;
			free (provider);
			free (bytes);
			provider = bytes = NULL;
		    } else {
			p->next = (prov_stats_t *) LA_calloc(1,
						    sizeof (prov_stats_t));
			p = p->next;
			p->prov_name = strdup(provider);
			p->bytes     = strdup(bytes);
			free (provider);
			free (bytes);
			provider = bytes = NULL;
		    }
	    	}
	    }
	    cp2 = cp1 + 1;
	    cp1 = strchr (cp2, '_');
	}

	if (!flag) {
	    /*Last bytes is not read*/
	    bytes = strdup (cp2);
	    if (!p) {
		p = (prov_stats_t *) LA_calloc(1, sizeof (prov_stats_t));
		p->prov_name = strdup(provider);
		p->bytes     = strdup(bytes);
		ptr = p;
		free (provider);
		free (bytes);
		provider = bytes = NULL;
	    } else {
		p->next = (prov_stats_t *) LA_calloc(1, sizeof (prov_stats_t));
		p = p->next;
		p->prov_name = strdup(provider);
		p->bytes     = strdup(bytes);
		free (provider);
		free (bytes);
		provider = bytes = NULL;
	    }
	}
	return (ptr);
    }
}

int
LA_update_hash_tables(char **line_arr, char *uri, char *temp_buf,
			int8_t update_dup_bw)
{
    /* Assuming 130 fields in the access log*/
    domain_hash_val_t *dom_val = NULL;
    uri_hash_val_t *uri_val = NULL;
    int32_t status, thrd_id = 0;
    int32_t bucket_num;
    uint64_t bytes_served = atol(line_arr[g_bytes_served_index]);
    char *cacheable_flag = line_arr[g_cacheable_flag_index];
    status = atoi(line_arr[g_status_index]);
    /*
     * cur_hash_mem_node is the current node to store the value of 
     * domain hash table
     */
    hash_mem_t *cur_hash_mem_node = g_cur_hash_mem_node;

    /*
     * cur_urihash_mem_node is the current node to store the value 
     * of uri hash table
     */
    urihash_mem_t *cur_urihash_mem_node = g_cur_urihash_mem_node;

    /*
     * Between two different access log runs, get to the end of the 
     * list for the current node
    while(cur_hash_mem_node->next != NULL)
	cur_hash_mem_node = cur_hash_mem_node->next;
    while(cur_urihash_mem_node->next != NULL)
	cur_urihash_mem_node = cur_urihash_mem_node->next;
     */

    dom_val = NULL;
    uri_val = NULL;

    /*
    * Increment global counters. we take into account all the lines including
    * those which are filtered out based on domains
    */
     g_overall_hits[thrd_id]++;
     g_overall_bytes_served[thrd_id] += bytes_served;

     if (!strcasecmp(cacheable_flag, "Buffer")){
         g_overall_bytes_served_from_buffer[thrd_id] += bytes_served;
         g_overall_cnt_buffer[thrd_id]++;
	 g_cache_indicator = buffer;
     } else if(!strcasecmp(cacheable_flag, "ORIGIN")){
         g_overall_bytes_served_from_origin[thrd_id] += bytes_served;
         g_overall_cnt_origin[thrd_id]++;
	 g_cache_indicator = origin;
     } else if(strcasestr(cacheable_flag, "Tunnel")){
         g_overall_bytes_served_from_tunnel[thrd_id] += bytes_served;
         g_overall_cnt_tunnel[thrd_id]++;
	 g_cache_indicator = tunnel;
         update_tunnel_stats_global(cacheable_flag, thrd_id, line_arr);
     } else if(!strcasecmp(cacheable_flag, "SAS")){
         g_overall_bytes_served_from_sas[thrd_id] += bytes_served;
         g_overall_cnt_sas[thrd_id]++;
	 g_cache_indicator = sas;
     } else if(!strcasecmp(cacheable_flag, "SSD")){
         g_overall_bytes_served_from_ssd[thrd_id] += bytes_served;
         g_overall_cnt_ssd[thrd_id]++;
	 g_cache_indicator = ssd;
     } else if(!strcasecmp(cacheable_flag, "SATA")){
         g_overall_bytes_served_from_sata[thrd_id] += bytes_served;
         g_overall_cnt_sata[thrd_id]++;
	 g_cache_indicator = sata;
     } else {
	 g_cache_indicator = unknown;
     }
     bucket_num = get_bucket_num(bytes_served);

     /*
      * Incrementing overall bucketwise stats
      */
     g_bucket_stats[bucket_num].hit_cnt++;
     g_bucket_stats[bucket_num].bytes_served += bytes_served;
     if(g_get_agewise_stats) {
	int age;
	if(line_arr[g_age_index] && *line_arr[g_age_index] != '-')
	    age = atoi(line_arr[g_age_index]);
	LA_fill_agewise_stats(bytes_served, age);
     }
 
     /*
      * Check if the current domain is one of the popular domains or not
      */
     if (g_only_top_domains_report) {
         if(!check_if_top_domain(line_arr[g_domain_index])) {
	    return (-1);
	 }
     }

     /*
      * Filter objects based on size 
      */
     if (bytes_served < g_size_threshold) {
	 return (-1);
     } else {
        /*
         * check if domain is present in the domain_hash table. If present
	 * insert it else update it
         */
         dom_val = g_hash_table_lookup(g_domain_hash_table[thrd_id],
					line_arr[g_domain_index]);

         if (dom_val == NULL){
         /*
     	 * LA_create_free_space() deletes the entries in the uri hash table in
	 * memory_tracker thread.
     	 */
             pthread_mutex_lock(&lru_mutex);
             LA_insert_domain_hash_table(&cur_hash_mem_node, line_arr,
					    thrd_id);
             LA_insert_uri_hash_table(&cur_urihash_mem_node, line_arr, uri,
					    thrd_id);
	     if (g_print_md5_uri_map) fprintf (md5urimapFP, "%s\n", temp_buf);
         //if(g_live_mode){

         /* 
     	 * Inserting the uri in the LRU list. This list is used to free up
	 * memory when memory crosses the configured value
     	 */
             LA_insert_LRU_list(uri);
         //}
             pthread_mutex_unlock(&lru_mutex);
         } else {
             pthread_mutex_lock(&lru_mutex);
             LA_update_domain_hash_table(dom_val, line_arr);

         /* 
     	 * Check if the uri is present in the hash or not and insert/update
	 * hashes accordingly
     	 */
             uri_val = g_hash_table_lookup(g_uri_hash_table[thrd_id], uri);

             if (uri_val == NULL){
		LA_update_bucketwise_stats(dom_val, line_arr, 0,
					    cacheable_flag);
		dom_val->CHR = (float)(dom_val->bytes_served_ssd + 
				    dom_val->bytes_served_sas + 
				    dom_val->bytes_served_sata +
				    dom_val->bytes_served_buffer)/
				    (float)(dom_val->tot_bytes_served);

		LA_insert_uri_hash_table(&cur_urihash_mem_node, line_arr, uri,
					thrd_id);
		if (g_print_md5_uri_map) fprintf (md5urimapFP, "%s\n",
					temp_buf);

            // if (g_live_mode){
		LA_insert_LRU_list(uri);
            // }
             } else {


             /* 
     	     * Update the duplicate bw stats
     	     */
		int update_bw_stats_206_response = 1;
		if ((g_cache_indicator == origin &&
			    (status == 200 || status == 206))){
		    int update_flag = 1;
		    if (uri_val->prev_origin) {
			if((status == 206 || g_range_uri.flag) &&
				    g_access_log_has_byte_ranges) {
			    update_flag = 0;
			    update_bw_stats_206_response = 0;
			    struct inc_values obj;
			    obj.cache_size_req = 0;
			    obj.dup_bw = 0;
			    if (g_range_uri.flag)
				parse_byte_range1(&obj, uri_val,
					atoll(line_arr[g_bytes_served_index]));
			    else
				parse_byte_range(&obj, uri_val, line_arr);

			    if (obj.dup_bw && (!g_do_peak_time_analysis ||
				(g_do_peak_time_analysis && g_peak_time)))
				g_tot_br_repeat_transactions++;

			    bucket_num = get_bucket_num(bytes_served);
			    dom_val->ce_chr[bucket_num].cache_size_req += 
							    obj.cache_size_req;
			    if (update_dup_bw) {
				if (obj.dup_bw && (!g_do_peak_time_analysis ||
				    (g_do_peak_time_analysis && g_peak_time))) {
				    dom_val->duplicate_bw += obj.dup_bw;
				    g_uri_list[thrd_id]
					[uri_val->uri_list_index].duplicate_bw
					+= atoll(line_arr[g_bytes_served_index]);
				    g_missed_bw_today += obj.dup_bw;
				    if (obj.dup_bw > 10000000) {
					g_10_dup += obj.dup_bw;
					fprintf (tempfp, "%s\n", uri);
				    }
				}
			    }
			    LA_increment_bucketwise_counters
				(&dom_val->ce_chr[bucket_num], 1, line_arr);
			}

			if (update_flag == 1) {
			    if (update_dup_bw) {
				if (!g_do_peak_time_analysis ||
				    (g_do_peak_time_analysis && g_peak_time)) {
				    dom_val->duplicate_bw +=
					atoll(line_arr[g_bytes_served_index]);
				    if (atoll(line_arr[g_bytes_served_index]) >
							10000000) {
					g_10_dup +=
					    atoll(line_arr[g_bytes_served_index]);
					fprintf (tempfp, "%s\n", uri);
				    }
				    g_uri_list[thrd_id][uri_val->uri_list_index].
					duplicate_bw += 
					atoll(line_arr[g_bytes_served_index]);
				    g_missed_bw_today +=
					atol(line_arr[g_bytes_served_index]);
				}
			    }

			    if (check_if_dup_uri_already_present(
					dom_val, uri_val->uri_list_index, 0,
					dom_val->cur_dup_uri) == 0){
				dom_val->dup_bw_uri_index_list[
					    dom_val->cur_dup_uri] =
					    uri_val->uri_list_index;
				dom_val->cur_dup_uri++;

				if (dom_val->cur_dup_uri%100 == 0)
				    dom_val->dup_bw_uri_index_list =
					(int32_t *)LA_realloc(dom_val->
						dup_bw_uri_index_list,
						(dom_val->cur_dup_uri+100)*
							sizeof(int));
			    }
			}
		    } else {
			uri_val->prev_origin = 1;
			if ((status == 206 || g_range_uri.flag)&&
					g_access_log_has_byte_ranges) {
			    update_flag = 0;
			    update_bw_stats_206_response = 0;
			    struct inc_values obj;
			    obj.cache_size_req = 0;
			    obj.dup_bw = 0;
			    if (g_range_uri.flag)
				parse_byte_range1(&obj, uri_val,
					atoll(line_arr[g_bytes_served_index]));
			    else
				parse_byte_range(&obj, uri_val, line_arr);

			    if (obj.dup_bw && (!g_do_peak_time_analysis ||
				(g_do_peak_time_analysis && g_peak_time)))
				g_tot_br_repeat_transactions++;

			    bucket_num = get_bucket_num(bytes_served);
			    dom_val->ce_chr[bucket_num].
				cache_size_req += obj.cache_size_req;
			    if (update_dup_bw) {
				if (obj.dup_bw && (!g_do_peak_time_analysis ||
				    (g_do_peak_time_analysis && g_peak_time))) {
				    dom_val->duplicate_bw += obj.dup_bw;
				    g_uri_list[thrd_id][uri_val->uri_list_index]
					.duplicate_bw +=
					atoll(line_arr[g_bytes_served_index]);
				    g_missed_bw_today += obj.dup_bw;
				    if (obj.dup_bw > 10000000) {
					g_10_dup += obj.dup_bw;
					fprintf (tempfp, "%s\n", uri);
				    }
				}
			    }
			    LA_increment_bucketwise_counters(
				    &dom_val->ce_chr[bucket_num], 1, line_arr);
			}
		    }
		} else if (g_cache_indicator == origin || 
			    g_cache_indicator == sas ||
			    g_cache_indicator == ssd ||
			    g_cache_indicator == sata||
			    g_cache_indicator == buffer) {
		/*
     		 * This flag tells if the object is served from origin the last time
     		 */
		    uri_val->prev_origin = 1;
		    if ((status == 206 || g_range_uri.flag)&&
				g_access_log_has_byte_ranges) {
		        update_bw_stats_206_response = 0;
		        struct inc_values obj;
		        obj.cache_size_req = 0;
		        obj.dup_bw = 0;
			if (g_range_uri.flag)
			    parse_byte_range1(&obj, uri_val,
				    atoll(line_arr[g_bytes_served_index]));
			else
			    parse_byte_range(&obj, uri_val, line_arr);

			if ((obj.dup_bw && !g_do_peak_time_analysis) ||
			    (g_do_peak_time_analysis && g_peak_time))
			    g_tot_br_repeat_transactions++;
		        bucket_num = get_bucket_num(bytes_served);
			if (g_cache_indicator == origin) {
			    dom_val->ce_chr[bucket_num].cache_size_req +=
					    obj.cache_size_req;
			    if (update_dup_bw) {
				if (obj.dup_bw && (!g_do_peak_time_analysis ||
				    (g_do_peak_time_analysis && g_peak_time))) {
				    dom_val->duplicate_bw += obj.dup_bw;
				    g_uri_list[thrd_id][uri_val->uri_list_index]
					.duplicate_bw += 
					atoll(line_arr[g_bytes_served_index]);
				    g_missed_bw_today += obj.dup_bw;
				    if (obj.dup_bw > 10000000) {
					g_10_dup += obj.dup_bw;
					fprintf (tempfp, "%s\n", uri);
				    }
				}
			    }
			}
		        LA_increment_bucketwise_counters(
				&dom_val->ce_chr[bucket_num], 1, line_arr);
		    }
		} else {
		    uri_val->prev_origin = 0;
		}
		if (update_bw_stats_206_response)
		    LA_update_bucketwise_stats(dom_val, line_arr, 1,
						cacheable_flag);

		dom_val->CHR = (float)(dom_val->bytes_served_ssd +
			dom_val->bytes_served_sas +
			dom_val->bytes_served_sata +
			dom_val->bytes_served_buffer)/
			(float)(dom_val->tot_bytes_served);
		LA_update_uri_hash_table(uri_val, line_arr);

	        /*
         	* Remove the uri from the lru list because it has more than 1 hit
		LA_update_LRU_list(uri);
     	 	*/
             }
             pthread_mutex_unlock(&lru_mutex);
         }
     }
     return (0);
}

/*
 * the number of reports is stored in rep_cnt.tmp in the 
 * /var/log/report/ directory.
 * if the rep_cnt crosses 100, old reports are deleted.
 */
uint8_t
get_cur_rep_num (void)
{
    FILE *fp;
    char tmp_file[200];
    uint8_t i = 0, rep_cnt = 0;
    char buf[11];

    snprintf (tmp_file, 200, "/var/log/report/rep_cnt.tmp");
    fp = fopen (tmp_file, "r");

    /* first report dir, rep_cnt file doesn't exist*/
    if (fp == NULL) {
	fp = fopen(tmp_file, "w");
	if (fp == NULL)
	    log_error ("file open failed", __FILE__, __LINE__);
	fprintf (fp , "%d", 0);
	fclose (fp);
    } else {
	i = fread(buf, 1, 10, fp);
	buf[i] = 0;

	if (i == 0) {
	    log_error ("file read failed", __FILE__, __LINE__);
	} else {
	    rep_cnt = atoi(buf);
	    fclose (fp);
	    rep_cnt ++;
	    fp = fopen(tmp_file, "w");
	    if (fp == NULL)
		log_error ("file open failed", __FILE__, __LINE__);
	    fprintf (fp , "%d", rep_cnt);
	    fclose (fp);
	}
    }

    return (rep_cnt);
}

/*
 * Merge the individual reports in the 
 * var/log/report dir. to generate a
 * consolidated report. 
 */
void
merge_reports ()
{
    char merge_op_filename[200], merge_op_dir[200];

    DIR *dirp, *dirp1;
    struct dirent *dp, *dp1;

    time_t t;
    struct tm *tmp;
    t = time(NULL);
    char *buf = (char *) LA_calloc(200, 1);
    FILE *merOPFP;
    struct stat st;
    int i = 0;

    char full_file_path[200];
    char full_file_name[200];

    GHashTable *merge_dw_hash = g_hash_table_new (g_str_hash, g_str_equal);
    merge_dw_hash_val_t *merdw_val_cur_node =
	(merge_dw_hash_val_t *)LA_calloc (1, sizeof (merge_dw_hash_val_t));
    merge_dw_hash_val_t *merdw_val_pool_head =
	merdw_val_cur_node; 
    merge_dw_domlist_t  *merdw_domlist_cur_node =
	(merge_dw_domlist_t *)LA_calloc (1, sizeof (merge_dw_domlist_t));
    merge_dw_domlist_t *merdw_domlist_pool_head = merdw_domlist_cur_node;
    merge_overall_stats_t *oall_stats =
	(merge_overall_stats_t *)LA_calloc (1, sizeof(merge_overall_stats_t));

    /*
     * Read the /var/log/report dir
     * read each dir, parse the consolidated and domain-wise reports and 
     * populate hash tables.
     */
     if ((dirp = opendir("/var/log/report")) == NULL)
	 log_error ("Failed to open dir",  __FILE__, __LINE__);

     /*
      * go through each reports_* dir in /var/log/report
      * and parse domainwise and overall reports.
      */
     do {
	 errno = 0;
	 if ((dp = readdir(dirp)) != NULL) {
	     if (!strncasecmp(dp->d_name, "reports_", 8)) {
		snprintf (full_file_path, 200, "%s/%s", "/var/log/report",
				dp->d_name);

		if ((stat(full_file_path, &st) == 0) && S_ISDIR(st.st_mode)) {
		    if ((dirp1 = opendir (full_file_path)) == NULL)
			log_error ("Failed to open dir", __FILE__, __LINE__);
		    do {
			errno = 0;
			if ((dp1 = readdir(dirp1)) != NULL) {
			    /*
			    * dir exists, read the domain_wise file now
			    */
			    if (strcasestr (dp1->d_name, "domainwise_stats")) {
				snprintf (full_file_name , 200, "%s/%s",
						full_file_path, dp1->d_name);
				parse_dw_report (full_file_name, merge_dw_hash,
						    merdw_val_cur_node,
						    merdw_domlist_cur_node);
			    } else if (strcasestr(dp1->d_name,
						"overall_stats")) {
				snprintf (full_file_name , 200, "%s/%s",
					full_file_path, dp1->d_name);
				parse_overall_report (full_file_name,
					oall_stats);
			    } else {
				continue;
			    }
			}
		    } while (dp1 != NULL);
		}
	     } else {
		continue;
	     }
	 }
     } while (dp != NULL);
    
    tmp = localtime(&t);
    if (strftime(buf, 200, "%F_%T", tmp) == 0) {
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"strftime returned 0: %d", errno);
	exit(EXIT_FAILURE);
    }
    snprintf (merge_op_dir, 100, "%s/%s_%s", "/var/log/report",
		    "merged_reports", buf);

    if (mkdir (merge_op_dir, S_IRWXU) == -1) {
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"mkdir returned -1: %d", errno);
	exit(EXIT_FAILURE);
    }

    /* print domain wise merged stats*/
    snprintf (merge_op_filename, 100, "%s/%s", merge_op_dir,
		    "domain_wise_report.csv");

    merOPFP = fopen (merge_op_filename, "w");
    if (merOPFP == NULL) {
	log_error("File open failed", __FILE__, __LINE__);
    }

    fprintf (merOPFP, "dom_name, bw_saved, cache_size_req, bytes_tunnel,"
	    "bytes_origin, bytes_sas, bytes_sata, bytes_ssd, bytes_buffer,"
	    "tot_bytes, dup_bw, hits_tunnel, hits_origin, hits_sas, hits_ssd,"
	    "hits_sata, hits_buffer, tot_hits, tot_content_length,"
	    "chr, cache_efficiency\n");
    g_hash_table_foreach(merge_dw_hash,
			    (GHFunc)publish_merged_reports, merOPFP);

    void *temp;
#define clear_mem_pool(type, head, temp) {\
    while (head) {\
	temp = head->next;\
	free (head);\
	head = (type *)temp;\
    }\
}
    clear_mem_pool (merge_dw_hash_val_t, merdw_val_pool_head, temp);
    clear_mem_pool (merge_dw_domlist_t, merdw_domlist_pool_head, temp);
    fclose (merOPFP);

    /*
     * Generate merged overall report
     */
    snprintf (merge_op_filename, 100, "%s/%s", merge_op_dir,
		"overall_report.csv");

    merOPFP = fopen (merge_op_filename, "w");
    if (merOPFP == NULL) {
	log_error("File open failed", __FILE__, __LINE__);
    }

    fprintf (merOPFP, "tot_hits, tot_bytes, tot_bytes_cache, bucket_wise_hits,\
	    bucket_wise_bytes (13 buckets), buffer_bytes, buffer_hits, \
	    sas_bytes, sas_hits, ssd_bytes, ssd_hits, sata_bytes, sata_hits,\
	    origin_bytes, origin_hitss\n");
    for (i = 0; i < OVERALL_REP_FIELD_CNT; i++) {
	fprintf (merOPFP, "%ld, ", oall_stats->counter[i]);
    }
    fclose (merOPFP);
    free(oall_stats);
}

void
parse_overall_report (char *full_file_name, merge_overall_stats_t *oall_statsp)
{
    FILE *fp;
    char line[5000];
    fp = fopen (full_file_name, "r");
    int i = 0;
    char *p;

    if (fp) {
	while (fgets (line, sizeof(line), fp)) {
	    if (strcasestr(line, "tot_hits"))
		    continue;
	    p = strtok(line, ",");
	    while (p) {
		oall_statsp->counter[i] += atol(p);
		p = strtok(NULL, ",");
		i++;
	    }
	}
	fclose (fp);
    }
}

void
parse_dw_report (char *full_file_name, GHashTable *merge_dw_hash,
		merge_dw_hash_val_t *merdw_val_cur_node,
		merge_dw_domlist_t *merdw_domlist_cur_node)
{
    FILE *fp;
    char line[5000], *p;
    fp = fopen (full_file_name, "r");
    char *domain_name;
    /* This holds the DW_REP_FIELD_CNT counter values present in the domain
     * wise report
     */
    uint64_t temp[DW_REP_FIELD_CNT];
    int i = 0;
    merge_dw_hash_val_t * merdw_val;

    if (!fp)
	log_error("Failed to open file", __FILE__, __LINE__);

    while (fgets (line, sizeof(line), fp)) {
	i = 0;
	p = strtok (line, ",");
	domain_name = p;
	if (domain_name == NULL)
	    continue;
	p = strtok (NULL, ",");

	/*Populate all the counters*/
	while ((p = strtok (NULL, ","))) {
	    temp[i] = atol (p);
	    i++;
	}

	merdw_val = g_hash_table_lookup(merge_dw_hash, domain_name);
	if (merdw_val == NULL) {
	    /*New domain*/
	    for (i = 0; i < DW_REP_FIELD_CNT; i++){
		merdw_val_cur_node->counter[i] = temp[i];
	    }
	    merdw_domlist_cur_node->domain_name = strdup (domain_name);
	    merdw_val = merdw_val_cur_node;

	    g_hash_table_insert(merge_dw_hash,
		    merdw_domlist_cur_node->domain_name, merdw_val);

	    merdw_val_cur_node->next = (merge_dw_hash_val_t *) LA_calloc (1,
					    sizeof (merge_dw_hash_val_t));
	    merdw_domlist_cur_node->next = (merge_dw_domlist_t *) LA_calloc (1,
					    sizeof (merge_dw_domlist_t));

	    merdw_val_cur_node	    = merdw_val_cur_node->next;
	    merdw_domlist_cur_node  = merdw_domlist_cur_node->next;
	} else {
	    /* domain already present update the counter values*/
	    for (i = 0; i < DW_REP_FIELD_CNT; i++){
		merdw_val->counter[i] += temp[i];
	    }
	}
    }
}

void
publish_merged_reports (gpointer key, merge_dw_hash_val_t *merdw_val,
			gpointer userdata)
{
    FILE *fp = (FILE *)userdata;
    int i;
    fprintf (fp, "%s, ", (char *)key);

    for (i = 0; i < DW_REP_FIELD_CNT; i++) {
	/*ratios and percentages printed at end*/
	if (i == 2 || i == 3 || i == 12 || i == 20)
	    continue;
	else
	    fprintf (fp, "%ld, ", merdw_val->counter[i]);
    }

    float chr, cache_efficiency;

    chr = cache_efficiency = 0;

    if (merdw_val->counter[10])
	chr = (float)merdw_val->counter[0]/(float) merdw_val->counter[10];
    if (merdw_val->counter[1])
	cache_efficiency = (float)merdw_val->counter[0]/
			    (float)merdw_val->counter[1];

   fprintf (fp, "%3.2f, %3.2f\n", chr, cache_efficiency);
}




void
parse_byte_range (struct inc_values *obj, uri_hash_val_t *uri_val,
		    char **line_arr)
{
    unsigned long long cur_br_lower, cur_br_upper;

    if ((!g_do_peak_time_analysis) ||
        (g_do_peak_time_analysis && g_peak_time))
        g_tot_br_transactions++;

    if (g_byte_range_index == -1) {
	DBG_LOG(ERROR, MOD_ANY_MOD,
		"byte_range_index is not configured in the config file");
	return;
    }
    char *ptr_br = strchr(line_arr[g_byte_range_index], '=');
    if (!ptr_br)
	return;
    ptr_br++;
    byte_range_t *brp = uri_val->byte_range_head;
    char *sp;
    char *p1 = strtok_r(ptr_br, "-", &sp);
    char *p2 = strtok_r(NULL, "-", &sp);
    if(p1){
	cur_br_lower = atol(p1);
    } else
	cur_br_lower = 0;
    if (p2){
	cur_br_upper = atol(p2);
    } else {
	cur_br_upper = cur_br_lower + atol(line_arr[g_bytes_served_index]);
    }
    uint64_t bytes_served = atol(line_arr[g_bytes_served_index]);
    if (!cur_br_upper || (cur_br_upper - cur_br_lower > bytes_served))
	cur_br_upper = cur_br_lower + atol(line_arr[g_bytes_served_index]);
    unsigned long long cur_high_temp = cur_br_upper;
    unsigned long long cur_low_temp = cur_br_lower;
    brp = uri_val->byte_range_head;
    while(brp){
	if (cur_low_temp <= brp->low_range){
	    if (cur_high_temp <= brp->high_range){
		if (cur_high_temp >= brp->low_range){
		    obj->dup_bw += cur_high_temp - brp->low_range;
		    obj->cache_size_req += brp->low_range - cur_low_temp;
		    break;
		} else {
		    obj->dup_bw += 0;
		    obj->cache_size_req += cur_high_temp - cur_low_temp;
		    break;
		}
	    } else {
		obj->dup_bw += brp->high_range - brp->low_range;
		obj->cache_size_req += brp->low_range - cur_low_temp;
		cur_low_temp = brp->high_range;
	    }
	} else if (cur_low_temp > brp->low_range){
	    if (cur_low_temp > brp->high_range){
		brp = brp->next;
		if (!brp) {
		    obj->dup_bw += 0;
		    obj->cache_size_req += (cur_high_temp - cur_low_temp);
		}
		continue;
	    } else {
		if (cur_high_temp <= brp->high_range){
		    obj->dup_bw += cur_high_temp - cur_low_temp;
		    obj->cache_size_req += 0;
		    break;
		} else {
		    obj->dup_bw += brp->high_range - cur_low_temp;
		    if (brp->next == NULL)
			obj->cache_size_req += (cur_high_temp - brp->high_range);
		    else
			obj->cache_size_req += 0;
		    cur_low_temp = brp->high_range;
		}
	    }
	}
	brp = brp->next;
    }

    brp = uri_val->byte_range_head;
    if (!brp){
	uri_val->byte_range_head = (byte_range_t *)LA_calloc(1,
						sizeof(byte_range_t));
	brp = uri_val->byte_range_head;
	brp->low_range = cur_br_lower;
	brp->high_range = cur_br_upper;
	brp->next = NULL;
    } else {
	while(brp){
	    if (cur_br_lower >= brp->low_range &&
			cur_br_lower <= brp->high_range){
		if(cur_br_upper <= brp->high_range){
		    break;
		} else {
		    brp->high_range = cur_br_upper;
		    break;
		}
	    } else if (cur_br_upper <= brp->high_range &&
			    cur_br_upper >= brp->low_range){
		if(cur_br_lower >= brp->low_range){
		    break;
		} else {
		    brp->low_range = cur_br_lower;
		    break;
		}
	    } else if (cur_br_upper >= brp->high_range &&
			    cur_br_lower <= brp->low_range){
		    brp->low_range = cur_br_lower;
		    brp->high_range = cur_br_upper;
		break;
	    }
	    if (brp->next)
		brp = brp->next;
	    else{
		byte_range_t *node = (byte_range_t *)LA_calloc(1,
							sizeof(byte_range_t));
		node->low_range = cur_br_lower;
		node->high_range = cur_br_upper;
		node->next = NULL;
		byte_range_t *ptr = uri_val->byte_range_head;
		byte_range_t *ptr1 = ptr;
		while(ptr){
		    if (cur_br_lower <= ptr->low_range){
			if (ptr != uri_val->byte_range_head){
			    ptr1->next = node;
			} else {
			    uri_val->byte_range_head = node;
			}
			node->next = ptr;
			break;
		    }
		    ptr1 = ptr;
		    ptr = ptr->next;
		}
		if (!ptr)
		    ptr1->next = node;
		brp = NULL;
	    }
	}
    }
    /*merge multiple overlapping ranges*/
    brp = uri_val->byte_range_head;
    while(brp){
	byte_range_t *ptemp = brp;
	unsigned int val = brp->high_range;
	while(ptemp->next != NULL && ptemp->next->low_range < val){
	    ptemp = ptemp->next;
	}
	if (ptemp != brp){
	    ptemp->low_range = brp->low_range;
	    while (brp != ptemp){
		byte_range_t *pnext = brp->next;
		free(brp);
		brp = pnext;
		uri_val->byte_range_head = brp;
	    }
	} else
	    brp = brp->next;
    }
}

void
parse_byte_range1 (struct inc_values *obj, uri_hash_val_t *uri_val,
		    unsigned long long bytes_served)
{

    unsigned long long cur_br_upper, cur_br_lower;

    if ((!g_do_peak_time_analysis) ||
        (g_do_peak_time_analysis && g_peak_time))
    g_tot_br_transactions++;

    cur_br_upper = g_range_uri.upper_range;
    cur_br_lower = g_range_uri.lower_range;

    byte_range_t *brp = uri_val->byte_range_head;

    if (!cur_br_upper || (cur_br_upper - cur_br_lower > bytes_served))
	cur_br_upper = cur_br_lower + bytes_served;

    unsigned long long cur_high_temp = cur_br_upper;
    unsigned long long cur_low_temp = cur_br_lower;

    brp = uri_val->byte_range_head;

    while(brp){
	if (cur_low_temp <= brp->low_range){
	    if (cur_high_temp <= brp->high_range){
		if (cur_high_temp >= brp->low_range){
		    obj->dup_bw += cur_high_temp - brp->low_range;
		    obj->cache_size_req += brp->low_range - cur_low_temp;
		    break;
		} else {
		    obj->dup_bw += 0;
		    obj->cache_size_req += cur_high_temp - cur_low_temp;
		    break;
		}
	    } else {
		obj->dup_bw += brp->high_range - brp->low_range;
		obj->cache_size_req += brp->low_range - cur_low_temp;
		cur_low_temp = brp->high_range;
	    }
	} else if (cur_low_temp > brp->low_range){
	    if (cur_low_temp > brp->high_range){
		brp = brp->next;
		if (!brp) {
		    obj->dup_bw += 0;
		    obj->cache_size_req += (cur_high_temp - cur_low_temp);
		}
		continue;
	    } else {
		if (cur_high_temp <= brp->high_range){
		    obj->dup_bw += cur_high_temp - cur_low_temp;
		    obj->cache_size_req += 0;
		    break;
		} else {
		    obj->dup_bw += brp->high_range - cur_low_temp;
		    if (brp->next == NULL)
			obj->cache_size_req += (cur_high_temp - brp->high_range);
		    else
			obj->cache_size_req += 0;
		    cur_low_temp = brp->high_range;
		}
	    }
	}
	brp = brp->next;
    }

    brp = uri_val->byte_range_head;
    if (!brp){
	uri_val->byte_range_head = (byte_range_t *)LA_calloc(1,
						    sizeof(byte_range_t));
	brp = uri_val->byte_range_head;
	brp->low_range = cur_br_lower;
	brp->high_range = cur_br_upper;
	brp->next = NULL;
    } else {
	while(brp){
	    if (cur_br_lower >= brp->low_range &&
			    cur_br_lower <= brp->high_range){
		if(cur_br_upper <= brp->high_range){
		    break;
		} else {
		    brp->high_range = cur_br_upper;
		    break;
		}
	    } else if (cur_br_upper <= brp->high_range &&
			    cur_br_upper >= brp->low_range){
		if(cur_br_lower >= brp->low_range){
		    break;
		} else {
		    brp->low_range = cur_br_lower;
		    break;
		}
	    } else if (cur_br_upper >= brp->high_range && 
			    cur_br_lower <= brp->low_range){
		    brp->low_range = cur_br_lower;
		    brp->high_range = cur_br_upper;
		break;
	    }
	    if (brp->next)
		brp = brp->next;
	    else{
		byte_range_t *node = (byte_range_t *)LA_calloc(1,
							sizeof(byte_range_t));
		node->low_range = cur_br_lower;
		node->high_range = cur_br_upper;
		node->next = NULL;
		byte_range_t *ptr = uri_val->byte_range_head;
		byte_range_t *ptr1 = ptr;
		while(ptr){
		    if (cur_br_lower <= ptr->low_range){
			if (ptr != uri_val->byte_range_head){
			    ptr1->next = node;
			} else {
			    uri_val->byte_range_head = node;
			}
			node->next = ptr;
			break;
		    }
		    ptr1 = ptr;
		    ptr = ptr->next;
		}
		if (!ptr)
		    ptr1->next = node;
		brp = NULL;
	    }
	}
    }
    /*merge multiple overlapping ranges*/
    brp = uri_val->byte_range_head;
    while(brp){
	byte_range_t *ptemp = brp;
	unsigned int val = brp->high_range;
	while(ptemp->next != NULL && ptemp->next->low_range < val){
	    ptemp = ptemp->next;
	}
	if (ptemp != brp){
	    ptemp->low_range = brp->low_range;
	    while (brp != ptemp){
		byte_range_t *pnext = brp->next;
		free(brp);
		brp = pnext;
		uri_val->byte_range_head = brp;
	    }
	} else
	    brp = brp->next;
    }
}

int
LA_process_dynamic_uri(char **line_arr)
{
    char *cp1;
    char *t1 = NULL;
    char *id = NULL;
    char *itag = NULL;
    if (strcasestr(line_arr[g_domain_index],"youtube") ||
	    strcasestr(line_arr[g_domain_index], "googlevideo")){
	if((t1 = strcasestr(line_arr[g_uri_index],"&id=")) ||
		(t1 = strcasestr(line_arr[g_uri_index],"?id="))){
	    char *p1 = strchr(t1+1, '&');
	    if (p1 != NULL){
		id = (char *)LA_calloc(p1-(t1+4)+1,1);
		strncpy(id, t1+4, p1-(t1+4));
	    } else {
		id = (char *)LA_calloc(strlen(t1+4)+1, 1);
		strncpy(id, t1+4, strlen(t1+4)+1);
	    }
	}
	if(id && ((t1 = strcasestr(line_arr[g_uri_index],"&itag=")) || 
		    (t1 = strcasestr(line_arr[g_uri_index],"?itag=")))){
	    char *p1 = strchr(t1+1, '&');
	    if (p1 != NULL){
		itag = (char *)LA_calloc(p1-(t1+6)+1,1);
		strncpy(itag, t1+6, p1-(t1+6));
	    } else {
		itag = (char *)LA_calloc(strlen(t1+6)+1, 1);
		strncpy(itag, t1+6, strlen(t1+6)+1); }
	}
	if (id && itag) {
	    free(line_arr[g_domain_index]);
	    line_arr[g_domain_index] = (char *)LA_calloc(10,1);
	    sprintf(line_arr[g_domain_index], "youtube");
	    free(line_arr[g_uri_index]);
	    line_arr[g_uri_index] = (char *)LA_calloc(
					    strlen(id)+strlen(itag)+1,1);
	    sprintf(line_arr[g_uri_index], "%s_%s", id, itag);
	}
    } else {
	if (g_only_youtube_googlevideo)
	    return -1;
	if (strcasestr (line_arr[g_domain_index], "netflix") || 
	    strcasestr (line_arr[g_domain_index], "nflximg")) {
	    free(line_arr[g_domain_index]);
	     line_arr[g_domain_index] = malloc(20);
	     sprintf (line_arr[g_domain_index], "%s","netflix_br");
	    if ((cp1 = strcasestr (line_arr[g_uri_index], "/range/"))) {
	        char *cp;		
		g_range_uri.flag = 1;
	        cp1 = cp1 + 7;
	        if (cp1)
	    	g_range_uri.lower_range = atol(cp1);
	        cp = strchr(cp1, '-');
	        if (cp)
	    	g_range_uri.upper_range = atol(cp + 1);
	        *cp1 = '\0';
	    }
	
	    char *cp = strchr(line_arr[g_uri_index], '?');
	    if (cp)
	        *cp = '\0';
	    /*
	     * strip dirs
	     */
	    cp = strstr(line_arr[g_uri_index], "isma");
	    if (!cp)
	        cp = strstr(line_arr[g_uri_index], "ismv");
	    if (!cp)
	        cp = strstr(line_arr[g_uri_index], "wmv");
	    if (!cp)
	        cp = strstr(line_arr[g_uri_index], "wma");
	    if (!cp)
	        cp = strstr(line_arr[g_uri_index], "bif");
	    if (!cp)
	        cp = strstr(line_arr[g_uri_index], "prdy");
	
	    if (cp) {
	        *(cp+4) = '\0';
	        while (*cp != '/' && cp != line_arr[g_uri_index]){
	    	cp --;
	        }
	    	if (*cp == '/' && cp != line_arr[g_uri_index])
	    	    cp --;
	        while (*cp != '/' && cp != line_arr[g_uri_index]){
	    	cp --;
	        }
	
	        char *uri1 = strdup (cp + 1);
	        free (line_arr[g_uri_index]);
	        line_arr[g_uri_index] =  strdup (uri1);
	        free (uri1);
	        uri1 = NULL;
	    }
	}
    }
    return 0;
}

static void
LA_fill_agewise_stats(uint64_t bytes_served, int age)
{
    int unscaled_age;
    int buck_num = 0;
    unscaled_age = (age/MIN_AGE);
    while(unscaled_age) {
	unscaled_age = unscaled_age/AGE_SCALE_FACTOR;
	if(unscaled_age)
	    buck_num++;
	if(buck_num > NUM_AGE_BUCKETS) {
	    buck_num = NUM_AGE_BUCKETS;
	    break;
	}
    }
    g_agewise_stats[buck_num].total_bw += bytes_served;
    if(g_cache_indicator == tunnel)
	g_agewise_stats[buck_num].tunneled_bw += bytes_served;
    else if(g_cache_indicator == origin)
	g_agewise_stats[buck_num].origin_bw += bytes_served;
    else if (g_cache_indicator != unknown)
	g_agewise_stats[buck_num].cached_bw += bytes_served;
}

static void
LA_print_agewise_stats(FILE *fp)
{
    int i;
    fprintf(fp, "\n\nAge-wise Stats(percentages wrt bytes served per bucket)\n"
	    "Bucket(secs), cached_bw, Origin_bw, Tunnel_bw\n");
    for(i = 0; i<NUM_AGE_BUCKETS; i++){
	fprintf(fp, "%d, %f, %f, %f\n", i,
	       (float)g_agewise_stats[i].cached_bw * 100/
			g_agewise_stats[i].total_bw,
	       (float)g_agewise_stats[i].origin_bw * 100/
			g_agewise_stats[i].total_bw,
	       (float)g_agewise_stats[i].tunneled_bw * 100/
			g_agewise_stats[i].total_bw);
    }

    fprintf(fp, "\n\nAge-wise Stats(percentages wrt total bytes served)"
	    "\n Bucket(secs), cached_bw, Origin_bw, Tunnel_bw\n");
    for(i = 0; i<NUM_AGE_BUCKETS; i++){
	fprintf(fp, "%d, %f, %f, %f\n", i,
	       (float)g_agewise_stats[i].cached_bw * 100/
			g_overall_bytes_served[0],
	       (float)g_agewise_stats[i].origin_bw * 100/
			g_overall_bytes_served[0],
	       (float)g_agewise_stats[i].tunneled_bw * 100/
			g_overall_bytes_served[0]);
    }
}

static void
LA_replace_uri_md5_cksum(char **uri, char *temp_buf, int len)
{
    unsigned int i;
    unsigned char *digest;
    char *temp = (char *)LA_calloc(4,1);
    MD_CTX context;
    digest = (unsigned char *)LA_calloc(17, 1);
    MDInit (&context);
    MDUpdate (&context, (unsigned char*)*uri, len);
    MDFinal (digest, &context);
    free(*uri);
    *uri = (char *)LA_calloc(33,1);
    for (i = 0; i < 16; i++){
      sprintf (temp, "%02x", digest[i]);
      strcat(*uri, temp);
    }
    strcat(temp_buf, *uri);
    if (temp)
       free(temp);
    if (digest)
       free(digest);
}

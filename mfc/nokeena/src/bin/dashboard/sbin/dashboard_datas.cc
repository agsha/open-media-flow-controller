#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <syslog.h>
#include "common.h"
#include "nkn_stat.h"
#include "nkncnt_client.h"
#include "nkn_dashboard.h"
#include "tstr_array.h"
#include "nkn_mgmt_defs.h"
#include "nkn_mem_counter_def.h"



nkncnt_client_ctx_t g_nvsd_mem_ctx;

int sleep_time=10;

time_t last_sample_time;
struct tm *last_time = NULL;
struct tm last_time_s;
double  global_om_connection_data[MAX_DATA_NUMBER];	 // Needed for Avtive session Graph:   Origin manager ipv4 connection
double  global_om_connection_data_ipv6[MAX_DATA_NUMBER];     // Needed for Avtive session Graph:   Origin manager ipv6 connection
double  global_http_con_data[MAX_DATA_NUMBER];		     // Http ipv4 connection
double  global_http_con_data_ipv6[MAX_DATA_NUMBER];	     // Http ipv6 connection
double  global_rtsp_con_data[MAX_DATA_NUMBER];		     // RTSP connection
double  glob_tot_size_from_cache[MAX_DATA_NUMBER];       // These 4 needed for  Data Source aggregate values in MBytes

double  hr_client_recv_tot_bytes[HOURS];
double  hr_client_send_tot_bytes[HOURS];
double  hr_origin_recv_tot_bytes[HOURS];
double  hr_origin_send_tot_bytes[HOURS];      //added to get the correct cache hit ratio (Bytes)

double  glob_tot_size_from_disk[MAX_DATA_NUMBER];
double  glob_tot_size_from_origin[MAX_DATA_NUMBER];
double  glob_tot_size_from_nfs[MAX_DATA_NUMBER];
double  glob_tot_size_from_tfm[MAX_DATA_NUMBER];      //added to get the correct cache hit ratio (Bytes)
double  glob_tot_size_from_tunnel[MAX_DATA_NUMBER];   //added tunnel delivery data
double  glob_rtsp_tot_size_from_cache[MAX_DATA_NUMBER];
double  glob_rtsp_tot_size_from_origin[MAX_DATA_NUMBER];

/*
 * For Cache Hit Ratio
 */
double  hr_tot_client_send_from_bm_or_dm[HOURS];
double  hr_tot_client_send[HOURS];

double  cache_rate[MAX_DATA_NUMBER];                     //These 4 are needed for Data Source rate calculation
double  disk_rate[MAX_DATA_NUMBER];
double  origin_rate[MAX_DATA_NUMBER];
double  nfs_rate[MAX_DATA_NUMBER];
double  tfm_rate[MAX_DATA_NUMBER];
double  tunnel_rate[MAX_DATA_NUMBER];
double  rtsp_cache_rate[MAX_DATA_NUMBER];
double  rtsp_origin_rate[MAX_DATA_NUMBER];

int total_num_cache = 0;			//These are needed for disk throughput

dm2_data_t dm2_data[MAX_DISK];
cache_tier_data ctdata[MAX_CACHE_TIER];
double network_bandwidth[MAX_DATA_NUMBER]; //Used for  Bandwidth graph
double network_bandwidth_rate[MAX_DATA_NUMBER];
struct chart_mem_t g_mem;
struct chart_cpu_t g_cpu;
struct ns_data_t ns_data[MAX_NAMESPACE];
int total_ns_data = 0;

uint64_t tot_video_delivered;		//total video delivered all together
uint64_t tot_video_delivered_with_hit;  //total video delivered from cache or disk
uint64_t hr_tot_video_delivered[HOURS];
uint64_t hr_tot_video_delivered_with_hit[HOURS];

double tot_gb_delivered;		//Total Giga Bytes delivered to be shown
double tot_gb_delivered_once;		//Total Giga Bytes collected using counters

static time_data t_data[MAX_DATA_NUMBER];
char * time_str[MAX_DATA_NUMBER];
unsigned long nvsd_uptime_since;
unsigned long nvsd_uptime_last;


hourly_network_data h_network_data[HIST_MAX_DATA_COUNT]; // 24*7 + 1 = 169, we have to show it for the day or week
gr_data net_data_hr[HOUR_NETWORK_DATA_POINT]; // data is teken every 10 sec for an hour
daily_bw_data day_bw_data[WEEK]; //for a week
daily_bw_data day_bw_data0;//for rate calculation
hourly_bandwidth_data hr_bw_data[HOURS]; //For a day
hourly_bandwidth_data hr_bw_data_2[HOURS]; //For a day
int protect;

int
nvsd_get_all_disk_names(nkncnt_client_ctx_t *ctx, tstr_array **disk_names);
int nvsd_get_all_ns_names(nkncnt_client_ctx_t *ctx, tstr_array **ns_names);
int nvsd_lib_get_names(nkncnt_client_ctx_t *ctx, uint32_t len,
        cp_vector *matches, tstr_array **names);

int32_t
nvsd_counter_strip_char(const char *str,
                         uint32_t base_name_len,
                         char *out,char strip_char);

int32_t
nvsd_counter_strip_name(const char *str,
                         uint32_t base_name_len,
                         char *out);

static void shm_update_all_counters(void);

static char out_bytes[MAX_NAMESPACE][64];
static char out_bytes_origin[MAX_NAMESPACE][64];
static char out_bytes_disk[MAX_NAMESPACE][64];
static char out_bytes_ram[MAX_NAMESPACE][64];
static char out_bytes_tunnel[MAX_NAMESPACE][64];

static char dc2_read_name[MAX_DISK][30];
static char dc2_write_name[MAX_DISK][30];
static char dc2_type_name[MAX_DISK][30];
static char dm2_total_block_name[MAX_DISK][30];
static char dm2_free_block_name[MAX_DISK][30];
static char dm2_free_resv_block_name[MAX_DISK][30];
static char dm2_block_size_name[MAX_DISK][30];


static void *
trie_copy_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return nd;
}

static void
trie_destruct_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return;
}

int32_t
nvsd_counter_strip_char(const char *str,
                         uint32_t base_name_len,
                         char *out,char strip_char)
{
    char *p1  = (char*) (str);
    char *p2 = p1, *p3;
    int32_t size = 0, err = 0;

    p3 = (p2 += base_name_len);
    while ( *p2++ != strip_char ) {
            };
    size = (p2 - p3);
    if (size <= 0) {
        out[0] = '\0';
        goto error;
    }
    memcpy(out, p3, size - 1);
    out[size] = '\0';

 error:
    return err;
}


/////////////////////////////////////////////////////////
// Infinite loop to update/ save data
////////////////////////////////////////////////////////////
void * generate_data(void * arg)
{
    //Initialize the data
    initialize_data();

    while(1)
    {
	pthread_mutex_lock( &mutex1 );
	shm_update_all_counters();
	pthread_mutex_unlock( &mutex1 );

	if( 0 == sleep_time)
	    break; // come out after one time
	sleep(sleep_time);
    }
    return NULL;
}



//////////////////////////////////////////////////////
//Initialize all data here
////////////////////////////////////////////////////////
void initialize_data()
{
    const char *initial_timestr = "00:00";
    int i, j;

    /*
     *initialize global connection data
     */
    for (i = 0; i < MAX_DATA_NUMBER; i++){
	global_http_con_data[i] = 0;
	global_rtsp_con_data[i] = 0;
	global_om_connection_data[i] = 0;

	glob_tot_size_from_cache[i] = 0;
	glob_tot_size_from_disk[i] = 0;
	glob_tot_size_from_origin[i] = 0;
	glob_tot_size_from_nfs[i] = 0;
	glob_tot_size_from_tfm[i] = 0;
	glob_tot_size_from_tunnel[i] = 0;
	glob_rtsp_tot_size_from_cache[i] = 0;
	glob_rtsp_tot_size_from_origin[i] = 0;

	cache_rate[i] = 0.0;
	disk_rate[i] = 0.0;
	origin_rate[i] = 0.0;
	nfs_rate[i] = 0.0;
	tfm_rate[i] = 0.0;
	tunnel_rate[i] =0.0;
	rtsp_cache_rate[i] = 0.0;
	rtsp_origin_rate[i] = 0.0;

	network_bandwidth[i] = 0.0;
	network_bandwidth_rate[i] = 0.0;
    }

    for (i = 0; i < HOURS; i++){
	hr_tot_client_send_from_bm_or_dm[i] = 0;
	hr_tot_client_send[i] = 0;
	hr_client_recv_tot_bytes[i] = 0;
	hr_client_send_tot_bytes[i] = 0;
	hr_origin_recv_tot_bytes[i] = 0;
	hr_origin_send_tot_bytes[i] = 0;
	hr_tot_video_delivered[i] = 0;
	hr_tot_video_delivered_with_hit[i] = 0;
    }

    for (i = 0; i < MAX_DATA_NUMBER; i++) {
	time_str[i] = (char *)malloc(6);
    }
    for (i = 0; i < MAX_DATA_NUMBER; i++){
	memcpy(time_str[i],initial_timestr,5);
	time_str[i][5] = 0;
    }
    memset((char *)&g_cpu, 0, sizeof(g_cpu));
    memset((char *)&g_mem, 0, sizeof(g_mem));

    tot_video_delivered = 0;
    tot_video_delivered_with_hit = 0;
    tot_gb_delivered = 0;
    tot_gb_delivered_once = 0;

    memset((char *)&t_data, 0, MAX_DATA_NUMBER * sizeof(time_data));
    memset((char *)&ctdata[0], 0, MAX_CACHE_TIER*sizeof(cache_tier_data));
    memset((char *)&dm2_data[0], 0, MAX_DISK*sizeof(dm2_data_t));
    memset((char *)&ns_data[0], 0, MAX_NAMESPACE*sizeof(struct ns_data_t));
};


////////////////////////////////////////////////////////////
// Display from shared Memory
////////////////////////////////////////////////////////////

/*
 * We need to sort the counters for performance.
 */
static int shm_find_counter(char * name)
{
    int i;
    for(i=0; i<pshmdef->tot_cnts; i++) {
	if (g_allcnts[i].size == 0) { continue; }
	if (strcmp (varname+g_allcnts[i].name, name) == 0){
	    return i;
	}
    }
    return -1;
}

static void
shm_update_dm2_counters( tstr_array *disk_names)
{
    int i, j, m, len;
    char * pname;
    char disk_exists_name[64];
    const char *disk_name = NULL;
    int dc2_read_name_is_missing[MAX_DISK];
    int dc2_write_name_is_missing[MAX_DISK];
    uint64_t value;
    int32_t err = 0;
    glob_item_t *item = NULL;

    len = tstr_array_length_quick(disk_names);

    /* Set up DM2 counter names */
    if (len == 0) return;
    total_num_cache = len;
    if(total_num_cache > MAX_DISK) {
	total_num_cache = MAX_DISK; //Just ignore the extra disks
    }

    for (i = 0; i < total_num_cache; i++) {
	disk_name = tstr_array_get_str_quick(disk_names, i);
	if (disk_name == NULL) continue;
	if (strlen(disk_name) == 0) continue;

	sprintf(disk_exists_name,"%s.dm2_provider_type",disk_name);
	sprintf(dc2_read_name[i], "%s.dm2_raw_read_bytes", disk_name);
	sprintf(dc2_write_name[i], "%s.dm2_raw_write_bytes", disk_name);
	sprintf(dc2_type_name[i], "%s.dm2_provider_type", disk_name);

	sprintf(dm2_total_block_name[i], "%s.dm2_total_blocks", disk_name);
	sprintf(dm2_free_block_name[i], "%s.dm2_free_blocks", disk_name);
	sprintf(dm2_free_resv_block_name[i], "%s.dm2_free_resv_blocks", disk_name);
	sprintf(dm2_block_size_name[i], "%s.dm2_block_size", disk_name);

	sprintf(dm2_data[i].disk_name, "%s", disk_name);
    }
    /*
     * The number of valid disk count is given by j at the
     * end of the for loop. Assigning that value to total_num_cache.
     * Issue with using MAX_DISK for  iterator is that we are able to see
     * the last valid disk getting repeatedly displayed. Root cause: when
     * cache is disabled , the dm2_provider_type counter is also lost and
     * hence the valid disk is not getting reset for the disk properly.
     */
    //total_num_cache = j;
    /*If the lower number disk dc_3 dc_4 are disabled, then this makes the
     * logic go wrong here. Suppose we have disk till dc_40.
     * The total_num_cache count will become say 38 but we will be reading
     * dc_3 , dc_4 values that not anyway going to be displayed,
     * but we will be missing dc_39 ,dc_40.
     */
    for (j=0; j<MAX_DISK; j++) {
	dc2_read_name_is_missing[j] = 1;
	dc2_write_name_is_missing[j] = 1;
	/* Assumption in dashboard is that if  the total
	 * blocks is 0 then the disk is invalid and is
	 * not considered for graph display.
	 * Initialize this variable to 0 ever time
	 * before the counter value is taken.
	 * This will ensure that stale data are not
	 * displayed as reported in PR 787325
	 */
	dm2_data[j].total_blocks = 0;
	dm2_data[j].valid_disk = false;
    }

    for (j=0; j<total_num_cache; j++) {

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dc2_read_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    dc2_read_name_is_missing[j] = 0;
	    push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		    dm2_data[j].raw_read_bytes, dm2_data[j].raw_read_rate);
	}

	//These are needed for cache_throughput Graph
	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dc2_write_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		    dm2_data[j].raw_write_bytes, dm2_data[j].raw_write_rate);
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dc2_type_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    dm2_data[j].provider_type = (int)value;
	}

	//These are needed for disk_usage Graph
	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dm2_total_block_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    dm2_data[j].total_blocks = CONVERT_TO_MBYTES((double)value);
	    if(dm2_data[j].total_blocks > 0) {
		dm2_data[j].valid_disk = true;
	    } else {
		dm2_data[j].valid_disk = false;
	    }
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dm2_free_block_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    dm2_data[j].free_blocks = CONVERT_TO_MBYTES((double)value);
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dm2_free_resv_block_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    dm2_data[j].free_resv_blocks = CONVERT_TO_MBYTES((double)value);
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		dm2_block_size_name[j], &item);
	if (item) {
	    value = (uint64_t)item->value;
	    dm2_data[j].block_size = ((double)value);
	}
    }//end of for

    /* XXX: this loop is useless, now as total_num_cache will represent
     * actual number of disk instead of old way */
    for (j=0; j<total_num_cache; j++) {
	if (dc2_read_name_is_missing[j] == 1) {
	    push_data_and_rate(dm2_data[j].raw_read_bytes[MAX_DATA_NUMBER-1],
		    dm2_data[j].raw_read_bytes, dm2_data[j].raw_read_rate);
	}
	if (dc2_write_name_is_missing[j] == 1) {
	    push_data_and_rate(dm2_data[j].raw_write_bytes[MAX_DATA_NUMBER-1],
		    dm2_data[j].raw_write_bytes, dm2_data[j].raw_write_rate);
	}
    }
}

static void
shm_update_namespace_counters( tstr_array *ns_names )
{
    int i, j, k;
    char * pmatch, * pstart;
    char * pname;
    time_t now;
    struct tm * t;
    int last_hour;
    double mbytes;
    uint32_t len;
    int length = 0,len_ns = 0;
    const char *name = NULL;
    now = time(NULL);
    t = localtime(&now);
    last_hour = (t->tm_hour + 23)%24;
    uint64_t value;
    int32_t err = 0;
    glob_item_t *item = NULL;

    // Find out namespace names
    length = tstr_array_length_quick(ns_names);
    for(i = 0; i < length; i++)
    {
	name = tstr_array_get_str_quick(ns_names, i);
	len_ns = strlen(name);
	memcpy(ns_data[i].name, name, len_ns);
	ns_data[i].name[len_ns] = 0;
    }

    total_ns_data = length;
    if (t->tm_hour != last_time->tm_hour) {
	// hour has just changed, so reset saved values to zero
	lc_log_debug(LOG_DEBUG, "Hour changed from: %d => %d, ns=> %d", last_time->tm_hour, t->tm_hour, total_ns_data);
	for(i = 0; i < total_ns_data; i++) {
	    // carry the values from previous hour
	    ns_data[i].saved_values.out_bytes[t->tm_hour] =
		ns_data[i].saved_values.out_bytes[last_time->tm_hour] ;
	    ns_data[i].saved_values.out_bytes_origin[t->tm_hour] =
		ns_data[i].saved_values.out_bytes_origin[last_time->tm_hour];
	    ns_data[i].saved_values.out_bytes_disk[t->tm_hour] =
		ns_data[i].saved_values.out_bytes_disk[last_time->tm_hour];
	    ns_data[i].saved_values.out_bytes_ram[t->tm_hour] =
		ns_data[i].saved_values.out_bytes_ram[last_time->tm_hour];
	    ns_data[i].saved_values.out_bytes_tunnel[t->tm_hour] =
		ns_data[i].saved_values.out_bytes_tunnel[last_time->tm_hour];

	    // makes sure, these values are accounted, when nvsd is restarted
	    ns_data[i].client.out_bytes[t->tm_hour] =
		ns_data[i].client.out_bytes[last_time->tm_hour] ;
	    ns_data[i].client.out_bytes_origin[t->tm_hour] =
		ns_data[i].client.out_bytes_origin[last_time->tm_hour];
	    ns_data[i].client.out_bytes_disk[t->tm_hour] =
		ns_data[i].client.out_bytes_disk[last_time->tm_hour];
	    ns_data[i].client.out_bytes_ram[t->tm_hour] =
		ns_data[i].client.out_bytes_ram[last_time->tm_hour];
	    ns_data[i].client.out_bytes_tunnel[t->tm_hour] =
		ns_data[i].client.out_bytes_tunnel[last_time->tm_hour];
	}
    }

    for (j=0; j < total_ns_data; j++) {
	sprintf(out_bytes[j], "ns.%s.http.client.out_bytes",
		ns_data[j].name);
	sprintf(out_bytes_origin[j], "ns.%s.http.client.out_bytes_origin",
		ns_data[j].name);
	sprintf(out_bytes_disk[j], "ns.%s.http.client.out_bytes_disk",
		ns_data[j].name);
	sprintf(out_bytes_ram[j], "ns.%s.http.client.out_bytes_ram",
		ns_data[j].name);
	sprintf(out_bytes_tunnel[j], "ns.%s.http.client.out_bytes_tunnel",
		ns_data[j].name);
    }

    /* NOTE: this code must come before reading current values */
    if ((nvsd_uptime_since != nvsd_uptime_last) && (nvsd_uptime_since != 0)){
	// nvsd has been restarted
	lc_log_basic(LOG_INFO, "nvsd has been restarted: last : %ld, new: %ld",
		nvsd_uptime_last, nvsd_uptime_since);
	nvsd_uptime_last = nvsd_uptime_since;
	for(i = 0; i < total_ns_data; i++) {
	    ns_data[i].saved_values.out_bytes[t->tm_hour] += ns_data[i].client.out_bytes[t->tm_hour];
	    ns_data[i].saved_values.out_bytes_origin[t->tm_hour] += ns_data[i].client.out_bytes_origin[t->tm_hour];
	    ns_data[i].saved_values.out_bytes_disk[t->tm_hour] += ns_data[i].client.out_bytes_disk[t->tm_hour];
	    ns_data[i].saved_values.out_bytes_ram[t->tm_hour] += ns_data[i].client.out_bytes_ram[t->tm_hour];
	    ns_data[i].saved_values.out_bytes_tunnel[t->tm_hour] += ns_data[i].client.out_bytes_tunnel[t->tm_hour];
	}
    }

    // Get counter values.

    for (j=0; j < total_ns_data; j++) {

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,  out_bytes[j], &item);
	if(item) {
	    value = (uint64_t)item->value;
	    mbytes = CONVERT_TO_MBYTES((double)value);
	    ns_data[j].client.out_bytes[t->tm_hour] = mbytes;
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,  out_bytes_origin[j], &item);
	if(item) {
	    value = (uint64_t)item->value;
	    mbytes = CONVERT_TO_MBYTES((double)value);
	    ns_data[j].client.out_bytes_origin[t->tm_hour] = mbytes;
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,  out_bytes_disk[j], &item);
	if(item) {
	    value = (uint64_t)item->value;
	    mbytes = CONVERT_TO_MBYTES((double)value);
	    ns_data[j].client.out_bytes_disk[t->tm_hour] = mbytes;
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,  out_bytes_ram[j], &item);
	if(item) {
	    value = (uint64_t)item->value;
	    mbytes = CONVERT_TO_MBYTES((double)value);
	    ns_data[j].client.out_bytes_ram[t->tm_hour] = mbytes;
	}

	item = NULL;
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,  out_bytes_tunnel[j], &item);
	if(item) {
	    value = (uint64_t)item->value;
	    mbytes = CONVERT_TO_MBYTES((double)value);
	    ns_data[j].client.out_bytes_tunnel[t->tm_hour] = mbytes;
	}
    }
    // put cumulative value at total_values
    // total_values = last + curr
    for (i = 0; i < total_ns_data; i++) {
	ns_data[i].total_values.out_bytes[t->tm_hour] =
	    ns_data[i].saved_values.out_bytes[t->tm_hour] + ns_data[i].client.out_bytes[t->tm_hour];

	ns_data[i].total_values.out_bytes_origin[t->tm_hour] =
	    ns_data[i].saved_values.out_bytes_origin[t->tm_hour] + ns_data[i].client.out_bytes_origin[t->tm_hour];

	ns_data[i].total_values.out_bytes_disk[t->tm_hour] =
	    ns_data[i].saved_values.out_bytes_disk[t->tm_hour] + ns_data[i].client.out_bytes_disk[t->tm_hour];

	ns_data[i].total_values.out_bytes_ram[t->tm_hour] =
	    ns_data[i].saved_values.out_bytes_ram[t->tm_hour]+ ns_data[i].client.out_bytes_ram[t->tm_hour];

	ns_data[i].total_values.out_bytes_tunnel[t->tm_hour] =
	    ns_data[i].saved_values.out_bytes_tunnel[t->tm_hour] + ns_data[i].client.out_bytes_tunnel[t->tm_hour];
    }
    return;
}




static void
shm_update_bandwidth_counters(void)
{
    int i, j, k;
    char * pmatch, * pstart;
    char * pname;
    time_t now;
    struct tm * t;
    int last_hour;
    double mbytes;
    uint64_t value;
    glob_item_t *item = NULL;
    now = time(NULL);
    t = localtime(&now);
    last_hour = (t->tm_hour + 23)%24;
    int32_t err = 0;
    // Get counter values.

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_client_send_from_bm_or_dm", &item);
    if(item) {
	value = (uint64_t)item->value;
	mbytes = CONVERT_TO_MBYTES((double)value);
	hr_tot_client_send_from_bm_or_dm[t->tm_hour] = mbytes;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_client_send", &item);
    if(item) {
	value = (uint64_t)item->value;
	mbytes = CONVERT_TO_MBYTES((double)value);
	hr_tot_client_send[t->tm_hour] = mbytes;
    }


    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_client_recv_tot_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	mbytes = CONVERT_TO_MBYTES((double)value);
	hr_client_recv_tot_bytes[t->tm_hour] = mbytes;
    }


    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_client_send_tot_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	mbytes = CONVERT_TO_MBYTES((double)value);
	hr_client_send_tot_bytes[t->tm_hour] = mbytes;
    }


    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_origin_recv_tot_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	mbytes = CONVERT_TO_MBYTES((double)value);
	hr_origin_recv_tot_bytes[t->tm_hour] = mbytes;
    }


    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_origin_send_tot_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	mbytes = CONVERT_TO_MBYTES((double)value);
	hr_origin_send_tot_bytes[t->tm_hour] = mbytes;
    }
}

static void
shm_update_all_counters(void)
{
    int i, j;
    time_t tv;
    struct tm * tm;
    char buffer[6];
    char * pname;
    time_t now ;
    struct tm * t;
    int last_hour;
    uint64_t value;
    glob_item_t *item = NULL;
    time(&tv);
    int32_t err = 0;
    //print_out_time(&tv);
    tstr_array *disk_names = NULL, *ns_names = NULL;

    /*
     * put time to a time array for Graph
     */
    tm = localtime(&tv);
    strftime(buffer,6,"%M:%S", tm);
    push_time(tm->tm_min,  tm->tm_sec);

    push_time_str(buffer);

    now = time(NULL);
    t = localtime(&now);
    last_hour = (t->tm_hour + 23)%24;

    localtime_r(&last_sample_time, &last_time_s);
    last_time = &last_time_s;

    tot_gb_delivered_once = 0;
    /*
     *   Display the counters
     */
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_cur_open_http_socket", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data (value,
		global_http_con_data);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_cur_open_http_socket_ipv6", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data (value,
		global_http_con_data_ipv6);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_cur_open_rtsp_socket", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data (value,
		global_rtsp_con_data);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_cp_tot_cur_open_sockets", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data (value,
		global_om_connection_data);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_cp_tot_cur_open_sockets_ipv6", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data (value,
		global_om_connection_data_ipv6);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_size_from_cache", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_tot_size_from_cache, cache_rate);
    }

    //The next 4 are needed for DataSource Graph
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_size_from_disk", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_tot_size_from_disk, disk_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_size_from_origin", &item);
    if (item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_tot_size_from_origin, origin_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_size_from_nfs", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_tot_size_from_nfs, nfs_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_size_from_tmf", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_tot_size_from_tfm, tfm_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "bm.global.bm.bufcache.cache_cnt", &item);
    if(item) {
	value = (uint64_t)item->value;
	g_mem.cache_mem = (double)value/32; //Pages = 32KBytes, KBytes
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_video_delivered", &item);
    if(item) {
	value = (uint64_t)item->value;
	tot_video_delivered = value ;
	hr_tot_video_delivered[t->tm_hour] = value ;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_http_cache_hit_cnt", &item);
    if(item) {
	value = (uint64_t)item->value;
	tot_video_delivered_with_hit = value ;
	hr_tot_video_delivered_with_hit[t->tm_hour] = value ;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_bytes_sent", &item);
    if(item) {
	value = (uint64_t)item->value;
	tot_gb_delivered_once += ((double)value /1000000000); //Giga Bytes;
    }


    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_rtsp_tot_bytes_sent", &item);
    if(item) {
	value = (uint64_t)item->value;
	tot_gb_delivered_once += ((double)value /1000000000); //Giga Bytes
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_tot_size_from_tunnel", &item);
    if(item) {
	value = (uint64_t)item->value;
	//Add tunnel data to our counter bug 3692
	tot_gb_delivered_once += ((double)value /1000000000); //Giga Bytes
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_tot_size_from_tunnel, tunnel_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_rtsp_tot_size_from_cache", &item);
    if(item) {
	value = (uint64_t)item->value;
	//Add tunnel data to our counter bug 3692
	tot_gb_delivered_once += ((double)value /1000000000); //Giga Bytes
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_rtsp_tot_size_from_cache, rtsp_cache_rate);
    }


    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_rtsp_tot_size_from_origin", &item);
    if(item) {
	value = (uint64_t)item->value;
	//Add tunnel data to our counter bug 3692
	tot_gb_delivered_once += ((double)value /1000000000); //Giga Bytes
	push_data_and_rate(CONVERT_TO_MBYTES((double)value),
		glob_rtsp_tot_size_from_origin, rtsp_origin_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "glob_nvsd_uptime_since", &item);
    if(item) {
	value = (uint64_t)item->value;
	//printf("%-40s = %-13d\t\n", varname+g_allcnts[i].name,  g_allcnts[i].value);
	nvsd_uptime_since = (unsigned long)value;
    }


    //The following 12 needed for cache_tier_throughput graph
    // 0 = RAM/ buffer cache
    // 1 = SSD, 5 = SAS, 6 = SATA

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "bm.global.bm.bufcache.hitbytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	ctdata[0].flag = 1;
	//Bufcache hit in pages, 1 page =  32K bytes
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[0].read_byte, ctdata[0].read_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "bm.global.bm.bufcache.fillbytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[0].write_byte, ctdata[0].write_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "bm.global.bm.bufcache.evictbytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[0].evict_byte, ctdata[0].evict_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SSD.dm2_raw_read_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	ctdata[1].flag = 1;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[1].read_byte, ctdata[1].read_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SSD.dm2_raw_write_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[1].write_byte, ctdata[1].write_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SSD.dm2_delete_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[1].evict_byte, ctdata[1].evict_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SAS.dm2_raw_read_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	ctdata[5].flag = 1;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[5].read_byte, ctdata[5].read_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SAS.dm2_raw_write_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[5].write_byte, ctdata[5].write_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SAS.dm2_delete_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[5].evict_byte, ctdata[5].evict_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SATA.dm2_raw_read_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	ctdata[6].flag = 1;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[6].read_byte, ctdata[6].read_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SATA.dm2_raw_write_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[6].write_byte, ctdata[6].write_rate);
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
	    "SATA.dm2_delete_bytes", &item);
    if(item) {
	value = (uint64_t)item->value;
	push_data_and_rate(CONVERT_TO_MBITS((double)value),
		ctdata[6].evict_byte, ctdata[6].evict_rate);
    }

    tot_gb_delivered = tot_gb_delivered_once;

    /* update DM2 counters */
    err = nvsd_get_all_disk_names(&g_nvsd_mem_ctx, &disk_names);
    shm_update_dm2_counters(disk_names);
    tstr_array_free(&disk_names);

    /* update namespace counters */
    err = nvsd_get_all_ns_names(&g_nvsd_mem_ctx, &ns_names);
    shm_update_namespace_counters(ns_names);
    tstr_array_free(&ns_names);
    //bail_error(err);

    /* update bandwidth counters */
    shm_update_bandwidth_counters();

    /*
     * update cpu data, not taken from the shared memory
     */
    update_cpu_data();

    /*
     * Update Bandwidth saving data, This data is shown for a week,
     */
    update_daily_bw_data(tm);

    /*
     * Update memory and network_bandwidth data only when show_other flag is set.
     * We don't show these two graphs on dashboard or disk cache tab on the web page
     */
    if (show_other){
	update_memory_data();
	update_network_bandwidth();
    }

    /*
     * Update historical_network data and hourly_network data
     * only when show_network_data flag is set
     */
    if (show_network_data){
	update_historical_network(tm);
	update_hourly_network(tm);
    }

    /* save the sample time, needed for next iteration */
    last_sample_time = now;
    return;
};
////////////////////////////////////////////////////////////////
// Common function
////////////////////////////////////////////////////////////////
uint64_t  push_data_delta_uint64(uint64_t value, uint64_t last_value)
{
    if ( value < last_value) {
	// crashed
	return value;
    }
    else {
	// Get increment size only
	return value - last_value;
    }
}

double  push_data_delta(double value, double last_value)
{
    if ( value < last_value) {
	// crashed
	return value;
    }
    else {
	// Get increment size only
	return value - last_value;
    }
}

/*
 *  Move the data one place and add the new data to the end of the array
 */
void  push_data(double value, double * array)
{
    int i = 0;
    for (i = 1; i < MAX_DATA_NUMBER; i++){
	array[i-1] = array[i];
    }
    array[ MAX_DATA_NUMBER - 1 ] = (double)value;

    return;
};

/*
 * Push the time string for graph purpose
 */

void push_time(int min, int sec)
{
    int i;

    for (int i = 1; i < MAX_DATA_NUMBER; i++){
	t_data[i-1].min = t_data[i].min;
	t_data[i-1].sec = t_data[i].sec;
    }
    t_data[MAX_DATA_NUMBER - 1].min = min;
    t_data[MAX_DATA_NUMBER - 1].sec = sec;

    return;
};

/*
 * Push the time str as a string
 */
void push_time_str(char * buf)
{
    int i;
    char * psrc;
    char * pdst;

    for (i = 1; i < MAX_DATA_NUMBER; i++){
	psrc = time_str[i];
	pdst = time_str[i-1];
	memcpy(pdst, psrc, 5);
	pdst[5] = 0;
    }
    memcpy(time_str[MAX_DATA_NUMBER - 1], buf, 5);
    time_str[MAX_DATA_NUMBER - 1][5] = 0;

    return;
};

/*
 *  Push aggregate data and rate
 */
void push_data_and_rate(double value, double * data_array, double * rate_array)
{
    int i;
    double rate;
    double prev;

    prev = data_array[MAX_DATA_NUMBER-1];
    rate = (value - prev)/(1.0 * sleep_time);

    if(rate < 0.0)
	rate = 0.0; // At this point rate can't be negetive, 
    // if it is -ve, nvsd has crashed, while nkn_dashboard running

    push_data(value, data_array);
    push_data(rate, rate_array);

    return;
};

/*
 * This method uses "ifconfig" to get network bandwith and saves the data 
 * in /var/nkn/dashboard/net_data.txt
 */
double get_network_data()
{
    FILE *fp;
    char line[1024];
    char net_data[MAX_INTERFACE][30];
    int i = 0;
    double bandwidth;

    line[0] = 0;
    for(i = 0; i < MAX_INTERFACE; i++) {
	net_data[i][0] = 0;
    }

    system("ifconfig | grep \"TX bytes\" | tr -d  \"bytes:\" | awk '{print $6}'> \"/var/nkn/dashboard/net_dataTX.txt\"");
    fp = fopen( "/var/nkn/dashboard/net_dataTX.txt", "r");

    if (fp != 0){
	//printf("fopen fine\n");
	i = 0;
	while( fgets(line, sizeof(line), fp) != NULL){
	    //printf("%s", line);
	    strcpy(net_data[i], line);
	    i++;
	    if(i>=MAX_INTERFACE) break;
	}
	fclose(fp);
    }

    bandwidth = 0.0;
    double temp;
    for(i = 0; i < MAX_INTERFACE; i++){
	temp = atof(net_data[i]);
	bandwidth += CONVERT_TO_MBITS( (double)temp );
    }

    for(i = 0; i < MAX_INTERFACE; i++) {
	net_data[i][0] = 0;
    }

    system("ifconfig | grep \"RX bytes\" | tr -d  \"bytes:\" | awk '{print $2}'> \"/var/nkn/dashboard/net_dataRX.txt\"");
    fp = fopen( "/var/nkn/dashboard/net_dataRX.txt", "r");

    if (fp != 0){
	//printf("fopen fine\n");
	i = 0;
	while( fgets(line, sizeof(line), fp) != NULL){
	    //printf("%s", line);
	    strcpy(net_data[i], line);
	    i++;
	    if(i>=MAX_INTERFACE) break;
	}
	fclose(fp);
    }

    for(i = 0; i < MAX_INTERFACE; i++){
	temp = atof(net_data[i]);
	bandwidth += CONVERT_TO_MBITS( (double)temp );
    }
    //printf("Bandwidth = %f\n", bandwidth);
    return bandwidth;
};


/*
 * Update array network_bandwidth network_bandwidth_rate
 */

void update_network_bandwidth()
{
    double bandwidth;

    bandwidth = get_network_data();
    push_data_and_rate(bandwidth, network_bandwidth, network_bandwidth_rate);

    return;
};

/*
 * This method uses /proc/stat to get the CPU information
 */
void update_cpu_data()
{
    int i, j , k, len;
    char line[1024];
    FILE *fp;
    char *next;
    char c;
    char data[MAX_CPU_FIELD][20];
    int t_cpu = 0;
    int u_cpu = 0;


    //initialize the data
    for(i = 0; i < MAX_CPU_FIELD; i++) {
	data[i][0] = 0;
    }
    line[0] = 0;

    fp = fopen("/proc/stat", "r");
    if( fp != 0){
	fgets(line, sizeof(line), fp);//just read the first line
	fclose(fp);
    }

    next = line + 5;
    len = strlen(next);

    i = 0;
    j = 0;

    while(i < len && j<MAX_CPU_FIELD)
    {
	c = next[i];
	k = 0;
	if (c == '\n')
	    break;
	while (c != ' ' && i < len){
	    data[j][k] = c;
	    k++;
	    i++;
	    c = next[i];
	}
	data[j][k] = 0;
	i++;
	j++;
    }//End while


    t_cpu = 0;
    for(i = 0; i < MAX_CPU_FIELD; i++) {
	t_cpu += atoi(data[i]);
    }

    //Used cpu = total cpu - idel cpu
    u_cpu = t_cpu - (atoi(data[3]) + atoi(data[4])); // idle_cpu = data[3], iowait = data[4]
    g_cpu.avg_cpu = 100.0 *  ((double)(u_cpu - g_cpu.used_cpu) / (t_cpu - g_cpu.tot_cpu));
    //save the values for rate purpose
    g_cpu.tot_cpu = t_cpu;
    g_cpu.used_cpu = u_cpu;

    return;
};

/*
 * This method uses free command to get memory information
 */
void update_memory_data()
{
    int i, j;
    char command[1024];
    FILE * pipe;
    char inchar;
    char buffer[30];

    //initialize all the variables we are using
    command[0] = 0;
    buffer[0] = 0;

    g_mem.tot_mem = 0.0;
    g_mem.used_mem = 0.0;
    g_mem.free_mem = 0.0;
    g_mem.scratch_buffer = 0.0;

    buffer[0] = 0;
    sprintf(command, "%s", "free -t -m | grep Total |awk '{print $3}'"); //used_mem
    pipe = popen(command,"r");
    if (pipe == NULL){
	printf("Unable to open pipe\n");
	return;
    }
    inchar = fgetc(pipe);
    i = 0;
    while(!feof(pipe))
    {
	buffer[i] = inchar;
	i++;
	inchar = fgetc(pipe);
    }

    buffer[i] = '\0'; // Insert string terminator.
    pclose(pipe);
    g_mem.used_mem = (double)atoi(buffer);

    g_mem.scratch_buffer = g_mem.used_mem - g_mem.cache_mem;

    return;
};

void update_historical_network(struct tm* tm)
{
    static bool init_hist_net_flag = true;
    int i;
    double netd, predata;
    double netr;
    char buffer[10]; //enough to copy the day

    if (init_hist_net_flag){//initialize date, ignore ratedata, no pushing needed
	h_network_data[HIST_MAX_DATA_COUNT-1].hr = tm->tm_hour;
	strftime(h_network_data[HIST_MAX_DATA_COUNT-1].day, 10, "%m/%d", tm);
	h_network_data[HIST_MAX_DATA_COUNT-1].netdata = get_network_data();
	h_network_data[HIST_MAX_DATA_COUNT-1].netrate = 0.0;
	init_hist_net_flag = false;//unset the flag

	//back track date for the graph
	time_t now;
	struct tm * t;

	now = time(NULL);
	t = localtime(&now);
	for (i = HIST_MAX_DATA_COUNT-2; i >= 0; i--){
	    now -= 60 * 60; //every hour beforea
	    t = localtime(&now);
	    h_network_data[i].hr = t->tm_hour;
	    strftime(h_network_data[i].day, 10, "%m/%d", t);
	}
	//Set the correct time
	now = time(NULL);
	t = localtime(&now);

	return;
    }

    predata = h_network_data[HIST_MAX_DATA_COUNT-1].netdata;//save the last input
    if (h_network_data[HIST_MAX_DATA_COUNT-1].hr != tm->tm_hour){//an hour has elasped
	//push data
	for(i = 1; i < HIST_MAX_DATA_COUNT; i++){
	    h_network_data[i-1].hr = h_network_data[i].hr;
	    strcpy(h_network_data[i-1].day, h_network_data[i].day);
	    h_network_data[i-1].netdata = h_network_data[i].netdata;
	    h_network_data[i-1].netrate = h_network_data[i].netrate;
	}
	h_network_data[HIST_MAX_DATA_COUNT-1].hr = tm->tm_hour;
	strftime(h_network_data[HIST_MAX_DATA_COUNT-1].day, 10, "%m/%d", tm);  //month/day
	netd = get_network_data();
	if (netd < predata) //wrong input, we should never get this
	    netd = predata;//hack

	h_network_data[HIST_MAX_DATA_COUNT-1].netdata = netd;
	netr = (netd - predata)/360.0; //1hr = 60 * 60 secs
	h_network_data[HIST_MAX_DATA_COUNT-1].netrate = (double) ((int)(netr * 100)/ 100.0);//only 2 decimal points

    }
    return;
}



void update_hourly_network(struct tm* tm)
{
    static bool init_hr_net_flag = true;    //Flag used to initialize h_network_data and net_data_hr
    int i;
    double netr;
    double netd, predata;

    if(init_hr_net_flag){//very first rate input , make it 0)
	strftime(net_data_hr[HOUR_NETWORK_DATA_POINT -1].time, 10, "%I:%M", tm);  // hour:min, ex 2:50
	net_data_hr[HOUR_NETWORK_DATA_POINT -1].tval = get_network_data();
	net_data_hr[HOUR_NETWORK_DATA_POINT -1].rval = 0.0;
	//back track date for the graph
	time_t now;
	struct tm * t;

	now = time(NULL);
	t = localtime(&now);
	for (i = HOUR_NETWORK_DATA_POINT-2; i >= 0; --i){
	    now -= 10; //every 10 sec before
	    t = localtime(&now);
	    strftime(net_data_hr[i].time, 10, "%I:%M", t);  // hour:min, ex 2:50
	}
	//Setback the correct time
	now = time(NULL);
	t = localtime(&now);

	init_hr_net_flag = false; //unset the flag

	return;
    }

    predata = net_data_hr[HOUR_NETWORK_DATA_POINT-1].tval; // save the last data
    for (i = 1; i < HOUR_NETWORK_DATA_POINT; i++){//push one position
	strcpy(net_data_hr[i-1].time, net_data_hr[i].time);
	net_data_hr[i-1].tval = net_data_hr[i].tval;
	net_data_hr[i-1].rval = net_data_hr[i].rval;
    }
    strftime(net_data_hr[HOUR_NETWORK_DATA_POINT -1].time, 10, "%I:%M", tm);  // hour:min, ex 2:50
    netd = get_network_data();
    if (netd < predata) //some thing wrong
	netd = predata; //hack

    net_data_hr[HOUR_NETWORK_DATA_POINT -1].tval = netd;
    netr = (netd - predata)/ (1.0 * sleep_time);
    net_data_hr[HOUR_NETWORK_DATA_POINT -1].rval = (double) ((int)(netr * 100)/ 100.0);//only 2 decimal points

    return;
}


//This function updates daily network data and hourly network data
void update_daily_bw_data(struct tm* tm)
{
    static bool init_bw_data_flag = true;
    int i;
    int day;
    double cache_bw;
    double disk_bw;
    double origin_bw;
    double nfs_bw;
    double total_bw;
    double saved_bw;
    double tunnel_bw;

    int hr;
    time_t now;
    struct tm * t;
    double ch_ratio;

    now = time(NULL);
    t = localtime(&now);

    //How much BW we added in each update
    //Negetive values are possible when nvsd had restarted while nkn dashboard is still running
    cache_bw = (glob_tot_size_from_cache[MAX_DATA_NUMBER-1]/1000)
	+ (glob_rtsp_tot_size_from_cache[MAX_DATA_NUMBER-1]/1000)
	- (glob_tot_size_from_cache[MAX_DATA_NUMBER-2]/1000)
	- (glob_rtsp_tot_size_from_cache[MAX_DATA_NUMBER-2]/1000);
    if (cache_bw < 0.0) cache_bw = 0.0;
    disk_bw = (glob_tot_size_from_disk[MAX_DATA_NUMBER-1]/1000)
	- (glob_tot_size_from_disk[MAX_DATA_NUMBER-2]/1000);
    if(disk_bw < 0.0) disk_bw = 0.0;
    origin_bw = (glob_tot_size_from_origin[MAX_DATA_NUMBER-1]/1000)
	+ (glob_rtsp_tot_size_from_origin[MAX_DATA_NUMBER-1]/1000)
	- (glob_tot_size_from_origin[MAX_DATA_NUMBER-2]/1000)
	- (glob_rtsp_tot_size_from_origin[MAX_DATA_NUMBER-2]/1000);
    if (origin_bw < 0.0) origin_bw = 0.0;
    nfs_bw = (glob_tot_size_from_nfs[MAX_DATA_NUMBER-1]/1000)
	- (glob_tot_size_from_nfs[MAX_DATA_NUMBER-2]/1000);
    if (nfs_bw < 0.0) nfs_bw = 0.0;
    tunnel_bw = (glob_tot_size_from_tunnel[MAX_DATA_NUMBER-1]/1000)
	- (glob_tot_size_from_tunnel[MAX_DATA_NUMBER-2]/1000);
    if (tunnel_bw < 0.0) tunnel_bw = 0.0;
    total_bw = cache_bw + disk_bw + origin_bw + nfs_bw + tunnel_bw;
    saved_bw = total_bw - (origin_bw + nfs_bw + tunnel_bw);

    if(init_bw_data_flag){ // initialize; do it for the first time
	//strftime(day_bw_data[WEEK-1].date, 10, "%m/%d", tm);//date string
	strftime(day_bw_data[WEEK-1].date, 10, "%a", tm);//date string
	day_bw_data[WEEK-1].day = tm->tm_yday; // day of the year

	for (i = 0; i < WEEK; i++){
	    day_bw_data[i].cache_bw = 0.0;
	    day_bw_data[i].disk_bw  = 0.0;
	    day_bw_data[i].origin_bw = 0.0;
	    day_bw_data[i].nfs_bw = 0.0;
	    day_bw_data[i].total_bw = 0.0;
	    day_bw_data[i].tunnel_bw = 0.0;
	    day_bw_data[i].saved_bw = 0.0;
	}

	////back track date for the graph
	//time_t now;
	//struct tm * t;

	now = time(NULL);
	//printf("time in sec = %d\n", (int)now);
	t = localtime(&now);
	for ( i = WEEK-2; i >= 0; i-- ) {
	    now -= 24 * 60 * 60; //every day before
	    //printf("now = %d\n", (int)now);
	    t = localtime(&now);
	    day_bw_data[i].day = t->tm_yday; // day of the year
	    //strftime(day_bw_data[i].date, 10, "%m/%d", t);
	    strftime(day_bw_data[i].date, 10, "%a", t);
	    //Set the correct time
	}//end of for
	now = time(NULL);
	t = localtime(&now); 

	//Now update the hourly bandwidth data
	strftime(hr_bw_data[HOURS-1].hrstr, 3, "%H", t); //hour string
	hr_bw_data[HOURS-1].hr = t->tm_hour;//hour of the day
	strftime(hr_bw_data_2[HOURS-1].hrstr, 3, "%H", t); //hour string
	hr_bw_data_2[HOURS-1].hr = t->tm_hour;//hour of the day

	for ( i = 0; i < HOURS; i++){
	    hr_bw_data[i].cache_bw = 0.0;
	    hr_bw_data[i].total_bw = 0.0;
	    hr_bw_data[i].ch_ratio = 0.0;

	    hr_bw_data_2[i].cache_bw = 0.0;
	    hr_bw_data_2[i].total_bw = 0.0;
	    hr_bw_data_2[i].ch_ratio = 0.0;
	}
	now = time(NULL);
	t = localtime(&now);
	for(i = HOURS-2; i >= 0; i-- ){
	    now -= 60 * 60; //every hour before
	    t = localtime(&now);
	    hr_bw_data[i].hr = t->tm_hour;
	    strftime(hr_bw_data[i].hrstr, 3, "%H", t);//initialize the time

	    hr_bw_data_2[i].hr = t->tm_hour;
	    strftime(hr_bw_data_2[i].hrstr, 3, "%H", t);//initialize the time
	}
	now = time(NULL);
	t = localtime(&now);


	init_bw_data_flag = false;
	return;
    }//end if(init_bw_data_flag)
    //else
    day = tm->tm_yday;
    if (day != day_bw_data[WEEK-1].day){//We moved to the next day
	//push data 
	for ( i = 1; i < WEEK; i++){
	    day_bw_data[i-1].day = day_bw_data[i].day;
	    day_bw_data[i-1].cache_bw = day_bw_data[i].cache_bw;
	    day_bw_data[i-1].disk_bw = day_bw_data[i].disk_bw;
	    day_bw_data[i-1].origin_bw = day_bw_data[i].origin_bw;
	    day_bw_data[i-1].nfs_bw = day_bw_data[i].nfs_bw;
	    day_bw_data[i-1].total_bw = day_bw_data[i].total_bw;
	    day_bw_data[i-1].saved_bw = day_bw_data[i].saved_bw;
	    day_bw_data[i-1].tunnel_bw = day_bw_data[i].tunnel_bw;
	    strncpy( day_bw_data[i-1].date,  day_bw_data[i].date, 10);
	}//end of for
	//update the latest info
	day_bw_data[WEEK-1].day = day;
	strftime(day_bw_data[WEEK-1].date, 10, "%a", tm);
	day_bw_data[WEEK-1].cache_bw = 0.0;
	day_bw_data[WEEK-1].disk_bw = 0.0;
	day_bw_data[WEEK-1].origin_bw = 0.0;
	day_bw_data[WEEK-1].nfs_bw = 0.0; 
	day_bw_data[WEEK-1].total_bw = 0.0;
	day_bw_data[WEEK-1].saved_bw = 0.0;
	day_bw_data[WEEK-1].tunnel_bw = 0.0;
    }

    day_bw_data[WEEK-1].cache_bw = day_bw_data[WEEK-1].cache_bw + cache_bw;
    day_bw_data[WEEK-1].disk_bw = day_bw_data[WEEK-1].disk_bw + disk_bw;
    day_bw_data[WEEK-1].origin_bw = day_bw_data[WEEK-1].origin_bw + origin_bw;
    day_bw_data[WEEK-1].nfs_bw = day_bw_data[WEEK-1].nfs_bw + nfs_bw;
    day_bw_data[WEEK-1].tunnel_bw = day_bw_data[WEEK-1].tunnel_bw + tunnel_bw;
    day_bw_data[WEEK-1].total_bw = day_bw_data[WEEK-1].total_bw + total_bw;
    day_bw_data[WEEK-1].saved_bw = day_bw_data[WEEK-1].saved_bw + saved_bw;


    //update the hour data if we have moved one hour
    hr = tm->tm_hour;
    if(hr != hr_bw_data[HOURS-1].hr){//we have moved one hour, push data
	for (i = 1; i < HOURS; i++){
	    hr_bw_data[i-1].hr = hr_bw_data[i].hr;
	    hr_bw_data[i-1].total_bw = hr_bw_data[i].total_bw;
	    hr_bw_data[i-1].cache_bw = hr_bw_data[i].cache_bw;
	    hr_bw_data[i-1].ch_ratio = hr_bw_data[i].ch_ratio;
	    strncpy(hr_bw_data[i-1].hrstr, hr_bw_data[i].hrstr, 3);
	}
	//update the latest info
	hr_bw_data[HOURS-1].hr = hr;
	strftime(hr_bw_data[HOURS-1].hrstr, 3, "%H", tm); //hour string
	hr_bw_data[HOURS-1].total_bw = 0.0;
	hr_bw_data[HOURS-1].cache_bw = 0.0;
	hr_bw_data[HOURS-1].ch_ratio = 0.0;
    }

    //just update the bandwidth value
    hr_bw_data[HOURS-1].total_bw += total_bw;
    hr_bw_data[HOURS-1].cache_bw += cache_bw + disk_bw;
    if (hr_bw_data[HOURS-1].total_bw > 0.0){
	hr_bw_data[HOURS-1].ch_ratio = (hr_bw_data[HOURS-1].cache_bw / hr_bw_data[HOURS-1].total_bw) * 100;
    }
    if ( hr_bw_data[HOURS-1].ch_ratio > 100.0)
	hr_bw_data[HOURS-1].ch_ratio = 100.0;
    if ( hr_bw_data[HOURS-1].ch_ratio < 0.0)
	hr_bw_data[HOURS-1].ch_ratio = 0.0;
    return;
}

int
nvsd_get_all_disk_names(nkncnt_client_ctx_t *ctx, tstr_array **disk_names)
{
    int err = 0;
    cp_vector *matches = NULL;

    /* check if there is a change or not */
    err = nkncnt_client_get_submatch(ctx, "dc_", &matches);
    if (err == E_NKNCNT_CLIENT_SEARCH_FAIL) {
	/* this is not a error */
	err = 0;
    }
    bail_error(err);

    if (matches == NULL) {
	/* no valid disks */
	goto bail;
    }
    /* get unique namespaces names */
    err = nvsd_lib_get_names(ctx, 0, matches, disk_names);
    bail_error(err);

    //tstr_array_dump(*disk_names, "DISKS");
bail:
    if (matches) cp_vector_destroy(matches);
    return err;
}

int32_t
nvsd_counter_strip_name(const char *str,
                         uint32_t base_name_len,
                         char *out)
{
    return nvsd_counter_strip_char(str, base_name_len, out, '.');
}


int
nvsd_lib_get_names(nkncnt_client_ctx_t *ctx, uint32_t len,
        cp_vector *matches, tstr_array **names)
{
    int32_t err = 0, num_elements = 0, i = 0, num_rp = 0;
    glob_item_t *item = NULL, *item1 = NULL;
    char *varname = (char *)(ctx->shm +  sizeof(nkn_shmdef_t) +
                           sizeof(glob_item_t) * ctx->max_cnts);
    temp_buf_t tmp_name = {0} ;
    cp_trie *tmp_trie = NULL;
    tstr_array *tmp_names = NULL;

    err = tstr_array_new(&tmp_names, 0);
    bail_error(err);

    tmp_trie = cp_trie_create_trie(
                (COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
                 COLLECTION_MODE_DEEP),
                trie_copy_func, trie_destruct_func);
    bail_null(tmp_trie);


    num_elements = cp_vector_size(matches);

    for (i = 0; i < num_elements; i++) {
        item = (glob_item_t *)cp_vector_element_at(matches, i);

        bzero(tmp_name, sizeof(tmp_name));
        nvsd_counter_strip_name(varname + item->name,
                len, tmp_name);

        item1 = (glob_item_t*)cp_trie_exact_match(tmp_trie,
                                          tmp_name);
        if (!item1) {
            /* new entry in cache */
            err = cp_trie_add(tmp_trie,
                              tmp_name, item);
            bail_error(err);
            /* add into the names array */
            err = tstr_array_append_str(tmp_names, tmp_name);
            bail_error(err);
        }
    }

    *names = tmp_names;
    tmp_names = NULL;
bail:
    if (tmp_trie != NULL)
        cp_trie_destroy(tmp_trie);
    tstr_array_free(&tmp_names);
    return err;
}





int
nvsd_get_all_ns_names(nkncnt_client_ctx_t *ctx, tstr_array **ns_names)
{
    int err = 0, len = 0, i = 0;
    cp_vector *matches = NULL;
    tstr_array *tmp_names = NULL, *tmp_array = NULL;
    tstring *ts = NULL;
    int32_t offset = 0;

    /* check if there is a change or not */
    err = nkncnt_client_get_submatch(ctx, "ns.", &matches);
    if (err == E_NKNCNT_CLIENT_SEARCH_FAIL) {
        /* this is not a error */
        err = 0;
    }
    bail_error(err);

    if (matches == NULL) {
        goto bail;
    }

    /* get unique namespaces names */
    err = nvsd_lib_get_names(ctx, strlen("ns."), matches, &tmp_names);
    bail_error(err);

    /* now we have all names with "<ns-name:UUID>" and "<ns-name>" */
    //tstr_array_dump(tmp_names, "NS-NAMES");

    err = tstr_array_new(&tmp_array, NULL);
    bail_error(err);

    len = tstr_array_length_quick(tmp_names);
    for (i = 0; i < len; i++) {
        ts = tstr_array_get_quick(tmp_names, i);
        if(ts == NULL) continue;

        err = ts_find_char(ts, 0,':', &offset);
        bail_error(err);

        /* ignore "<ns-name:UUID>" */
        if (offset == -1) {
            /* ignore mfc_probe namespace */
            if (!ts_equal_str(ts, "mfc_probe",false)) {
                err = tstr_array_append_str(tmp_array, ts_str_maybe(ts));
                bail_error(err);
            }
        }
    }

    *ns_names = tmp_array;
    tmp_array = NULL;

    //tstr_array_dump(*ns_names, "NS-NAMES");

bail:
    tstr_array_free(&tmp_names);
    tstr_array_free(&tmp_array);
    if (matches) cp_vector_destroy(matches);
    return err;
}

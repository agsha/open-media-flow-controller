#ifndef  __NKN_DASHBOARD__H
#define  __NKN_DASHBOARD__H

#include <sys/types.h>
#include <stdint.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#define USE_SPRINTF

#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_common_config.h"

#define MAX_CPU_FIELD   8
#define MAX_DISK        60	//Note: If we disable a disk, it still stays in shared memory
#define MAX_INTERFACE   16
#define MAX_MEMORY      2

#define X_POINTS_10     10
#define X_POINTS_DAY    24

#define CONVERT_TO_MBITS(x)     ((x)/1000000 * 8)
#define CONVERT_TO_MBYTES(x)	((x)/1000000)


#define MAX_DATA_NUMBER 25
#define HIST_MAX_DATA_COUNT 169 // 24 * 7 + 1  Hourly data points for a week
#define HOUR_NETWORK_DATA_POINT 361  // Data is taken every 10 secs
#define WEEK	7
#define MAX_CACHE_TIER	10	//we have only 4 tier, 0=cache, 1=SSD, 5=SAS, 6=SATA
#define MAX_DISK_NAME_SIZE 10 	//names are dc_1 to dc_16, char[6] is enough at this point
#define HOURS  24      		//total hours in a day 0 .. 23

extern nkn_shmdef_t * pshmdef;
extern char * varname;
extern glob_item_t * g_allcnts;
extern int revision;
extern pthread_mutex_t mutex1;
extern int sleep_time;

extern bool show_dashboard;	//Flag used to create and show graphs on dashboard tab  on the web page	
extern bool show_disk_cache;	//Flag used to create and show graphs on disk_cache tab on the web page
extern bool show_network_data;	//Flag used to create and show graphs on network tab on the web page	
extern bool show_other;		//Flag used to create and show other graphs

extern double  global_http_con_data[MAX_DATA_NUMBER]; // Needed for Active ipv4 session Graph
extern double  global_http_con_data_ipv6[MAX_DATA_NUMBER]; // Needed for Active ipv6 session Graph
extern double  global_rtsp_con_data[MAX_DATA_NUMBER];
extern double  global_om_connection_data[MAX_DATA_NUMBER];
extern double  global_om_connection_data_ipv6[MAX_DATA_NUMBER];

/*
 * For Bandwidth Savings
 */
extern double  hr_client_recv_tot_bytes[HOURS];
extern double  hr_client_send_tot_bytes[HOURS];
extern double  hr_origin_recv_tot_bytes[HOURS];
extern double  hr_origin_send_tot_bytes[HOURS];      //added to get the correct cache hit ratio (Bytes)

extern double  glob_tot_size_from_cache[MAX_DATA_NUMBER];       // These 4 needed for  Data Source aggregate values
extern double  glob_tot_size_from_disk[MAX_DATA_NUMBER];
extern double  glob_tot_size_from_origin[MAX_DATA_NUMBER];
extern double  glob_tot_size_from_nfs[MAX_DATA_NUMBER];
extern double  glob_tot_size_from_tfm[MAX_DATA_NUMBER];
extern double  glob_tot_size_from_tunnel[MAX_DATA_NUMBER];
extern double  glob_rtsp_tot_size_from_cache[MAX_DATA_NUMBER];
extern double  glob_rtsp_tot_size_from_origin[MAX_DATA_NUMBER];

extern double  cache_rate[MAX_DATA_NUMBER];                     //These 4 are needed for Data Source rate calculation
extern double  disk_rate[MAX_DATA_NUMBER];
extern double  origin_rate[MAX_DATA_NUMBER];
extern double  nfs_rate[MAX_DATA_NUMBER];
extern double  tfm_rate[MAX_DATA_NUMBER];
extern double  tunnel_rate[MAX_DATA_NUMBER];
extern double  rtsp_cache_rate[MAX_DATA_NUMBER];
extern double  rtsp_origin_rate[MAX_DATA_NUMBER];

extern int total_num_cache;                                     //These are needed for disk throughput

extern double network_bandwidth[MAX_DATA_NUMBER];               //Used for  Bandwidth graph
extern double network_bandwidth_rate[MAX_DATA_NUMBER];

/*
 * For Cache Hit Ratio.
 */
extern double  hr_tot_client_send_from_bm_or_dm[HOURS];
extern double  hr_tot_client_send[HOURS];

/*
 * struct used for memory usage graph.
 */
struct chart_mem_t {
	double cache_mem;
	double tot_mem;
	double used_mem;
	double free_mem;
	double scratch_buffer; //used_mem - cache_mem;
};
extern struct chart_mem_t g_mem;

/*
 * struct used for cpu_usage graph.
 */
struct chart_cpu_t {
	int tot_cpu;
	int used_cpu;
	double avg_cpu;
};
extern struct chart_cpu_t g_cpu;

extern uint64_t tot_video_delivered;
extern uint64_t tot_video_delivered_with_hit;
extern uint64_t hr_tot_video_delivered[HOURS];
extern uint64_t hr_tot_video_delivered_with_hit[HOURS];
extern double tot_gb_delivered;       //Total Giga bits delivered

extern char * time_str[MAX_DATA_NUMBER];
extern unsigned long nvsd_uptime_since;

typedef struct timedata{
        int min;
        int sec;
} time_data;


/*
 * Display Hourly based Cache Hit Ratio.
 */
typedef struct hourly_network_data{
        int hr;			// time in hr 0..23
        char day[10];		// mon/day    05/11 (May 11th)
        double netdata;		// aggregate data
        double netrate;		// rate
} hourly_network_data;
extern hourly_network_data h_network_data[HIST_MAX_DATA_COUNT];

typedef struct gr_data{
        char time[10]; 		// time as hr:min 
        double tval; 		// Aggregate value
        double rval; 		// Rate value
}gr_data;
extern gr_data net_data_hr[HOUR_NETWORK_DATA_POINT]; // data is teken every 10 sec for an hour

/*
 * Needed for Cache_tier_throughput data and graph
 */
typedef struct cache_tier_data{
        int flag;           //0 = not initialized, 1 = initialized
        char tier_name[10];     //RAM, SSD, SAS, SATA
        double read_byte[MAX_DATA_NUMBER];
        double write_byte[MAX_DATA_NUMBER];
        double evict_byte[MAX_DATA_NUMBER];
        double read_rate[MAX_DATA_NUMBER];
        double write_rate[MAX_DATA_NUMBER];
        double evict_rate[MAX_DATA_NUMBER];
}cache_tier_data;
extern cache_tier_data ctdata[MAX_CACHE_TIER];

/*
 * Support for 16 disks,
 * Specify if a disk is valid or not based on the total blocks, 
 * if total blocks == 0, disk is invalid
 * nvsd doesn't remove the entry from the shared memory for an invalid disk
 *
 * used for Disk Throughput and Disk Usage graph
 */
typedef struct dm2_data_t{
        bool valid_disk;

        double raw_read_bytes[MAX_DATA_NUMBER];
        double raw_read_rate[MAX_DATA_NUMBER];
        double raw_write_bytes[MAX_DATA_NUMBER];
        double raw_write_rate[MAX_DATA_NUMBER];

        int provider_type;
	char disk_name[MAX_DISK_NAME_SIZE];

        double free_blocks;
        double total_blocks;
        double free_resv_blocks;
        double block_size;
}dm2_data_t;
extern dm2_data_t dm2_data[MAX_DISK];

typedef struct hourly_bandwidth_data{ // The units in Giga Bytes
	int hr;			// time in hr 0..23
	char hrstr[3];          // hour string 00 .. 23
	double total_bw; 	// total bandwidth
	double cache_bw;	// bandwidth from RAM cache
	double ch_ratio;        // cache hit ratio
}hourly_bandwidth_data;
extern hourly_bandwidth_data hr_bw_data[HOURS]; //for a day
extern hourly_bandwidth_data hr_bw_data0;
//extern hourly_bandwidth_data hr_bw_data_2[HOURS]; //for a day


typedef struct daily_bw_data{//in GB
	int day;
	char date[10];
	double total_bw;        // total bandwidth
        double cache_bw;        // bandwidth from RAM cache
        double disk_bw;         // bandwidth from disk
        double origin_bw;
	double nfs_bw;
	double saved_bw; 	// total - origin
	double tunnel_bw;
} daily_bw_data;
extern daily_bw_data day_bw_data[WEEK]; //for a week
extern daily_bw_data day_bw_data0;

/*
 * namespace level counters display.
 */
#define MAX_NAMESPACE	NKN_MAX_NAMESPACES

typedef struct cmn_client_stats_s {
    double out_bytes[HOURS];;
    double out_bytes_origin[HOURS];;
    double out_bytes_disk[HOURS];;
    double out_bytes_ram[HOURS];;
    double out_bytes_tunnel[HOURS];;
} cmn_client_stats_t;

struct ns_data_t {
    char name[64];
    cmn_client_stats_t client;
    cmn_client_stats_t saved_values;
    cmn_client_stats_t total_values;
};

extern struct ns_data_t ns_data[MAX_NAMESPACE];
extern int total_ns_data;

/*
 * Functions defined in dashboard_datas.cc file
 */

void * generate_data(void * arg);
void initialize_data();
void update_cpu_data();
double push_data_delta(double value, double last_value);
void push_data(double value, double * array);
void push_data_and_rate(double value, double * data_array, double * rate_array);
void push_time(int min, int sec);
void push_time_str( char *buf);
void update_memory_data();
void update_network_bandwidth();
double get_network_data();
void update_hourly_network(struct tm* tm);
void update_historical_network(struct tm* tm);
void update_daily_bw_data(struct tm* tm);

/*
 * Functions defined in dashboard_graphs.cpp file
 */

void * generate_graph(void * arg);
void make_graph_open_connection();
void make_graph_data_source();
void make_graph_disk_throughput_2();
void make_graph_disk_usage();
void make_graph_network_bandwidth();
void make_graph_memory();
void make_graph_cpu_usage();
void make_graph_total_video_delivered();
void make_graph_cache_hit_rate();
void display_media_delivery_bandwidth();
void make_daily_network_graph();
void make_weekly_network_graph();
void make_graph_info_2(); // GB Delivered, Cache hit Ratio, video delivered
void make_hourly_network_graph();

#endif //__NKN_DASHBOARD__H

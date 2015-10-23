
#define sp "/stat/nkn"
struct sample_config
{
    /*! Sample node specific details */
    char *name;             /* string */
    char *enable;           /* bool */
    char *node;             /* name */
    uint32 interval;        /* duration */
    uint32 num_to_keep;     /* uint32 */
    int32 num_to_cache;	    /* int32 */
    char * sample_method;   /* string {direect,delta,aggregate}    */
    char * delta_constraint;/* string {none,increasing,decreasing} */
    uint32 sync_interval;   /* uint32 */
    char *label;            /* string - description*/
    char *unit;             /* string - unit*/
    char *delta_avg;
};

#define define_sample_generic(n, nde, intvl, num_keep, metd, del_cons, lbl, ut) \
 { \
   .name = (char *)n , \
   .enable = (char *)"true" , \
   .node =  (char *)nde  , \
   .interval = intvl , \
   .num_to_keep = num_keep , \
   .num_to_cache = -1, \
   .sample_method =  (char *)metd  , \
   .delta_constraint = (char *)del_cons, \
   .sync_interval = 10 , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
   .delta_avg = (char *)"false", \
 },


/* Defined for cmm BW monitoring,samples only for a small number */
#define define_sample_small(n,nde,metd,lbl,ut) \
 { \
   .name = (char *)n , \
   .enable = (char *)"true" , \
   .node =  (char *)nde  , \
   .interval = 1 , \
   .num_to_keep = 10 , \
   .num_to_cache = -1, \
   .sample_method =  (char *)metd  , \
   .delta_constraint = (char *)"increasing", \
   .sync_interval = 10 , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
   .delta_avg = (char *)"false", \
 },



#define define_sample(n,nde,metd,lbl,ut) \
 { \
   .name = (char *)n , \
   .enable = (char *)"true" , \
   .node =  (char *)nde  , \
   .interval = 10 , \
   .num_to_keep = 120 , \
   .num_to_cache = -1, \
   .sample_method =  (char *)metd  , \
   .delta_constraint = (char *)"increasing", \
   .sync_interval = 10 , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
   .delta_avg = (char *)"false", \
 },


#define define_sample_day(n,nde,metd,lbl,ut) \
 { \
   .name = (char *)n , \
   .enable = (char *)"true" , \
   .node =  (char *)nde  , \
   .interval = 300 , \
   .num_to_keep = 288 , \
   .num_to_cache = -1, \
   .sample_method =  (char *)metd  , \
   .delta_constraint = (char *)"increasing", \
   .sync_interval = 10 , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
   .delta_avg = (char *)"false", \
 },

#define define_sample_week(n,nde,metd,lbl,ut) \
 { \
   .name = (char *)n , \
   .enable = (char *)"true" , \
   .node =  (char *)nde  , \
   .interval = 1800 , \
   .num_to_keep = 336 , \
   .num_to_cache = -1, \
   .sample_method =  (char *)metd  , \
   .delta_constraint = (char *)"increasing", \
   .sync_interval = 10 , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
   .delta_avg = (char *)"false", \
 },

#define define_sample_month(n,nde,metd,lbl,ut) \
 { \
   .name = (char *)n , \
   .enable = (char *)"true" , \
   .node =  (char *)nde  , \
   .interval = 14400 , \
   .num_to_keep = 168 , \
   .num_to_cache = -1, \
   .sample_method =  (char *)metd  , \
   .delta_constraint = (char *)"increasing", \
   .sync_interval = 10 , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
   .delta_avg = (char *)"false", \
 },

struct chd_config
{
    /*! CHD node specific details */
    char *name;         /* string */
    char *enable;            /* bool */
    char *source_type;      /* string {sample,chd} */
    char *source_id;        /* string */
    char **source_node;     /* name */
    uint32 num_to_keep;     /* unit32 */
    char *select_type;      /* string {instances,time} */
    uint32 inst_num_to_use; /* uint32 */
    uint32 inst_num_to_advance; /* unit32 */
    int32 num_to_cache;    /* uint32 */
    uint32 time_interval_length;/* duration_sec */
    uint32 time_interval_distance;/* duration_sec */
    uint32 time_interval_phase; /* duration_sec */
    char *function;        /* string {min.max,mean,first,last,sum} */
    uint32 sync_interval;  /* uint32 */
    char *calc_partial;   /* bool */
    char *alarm_partial;  /* bool */
    char *label;            /* string - description*/
    char *unit;             /* string - unit*/

};
/* Defind for cmm BW monitoring,keeps only a small number */
/* average over the last 5 samples */

#define define_chd_generic(n, type, id, num_keep, num_use, num_adv, func, lbl, ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = num_keep , \
   .select_type = (char *)"instances" , \
   .inst_num_to_use = num_use, \
   .inst_num_to_advance = num_adv , \
   .num_to_cache = -1, \
   .time_interval_length = 0 , \
   .time_interval_distance = 0 , \
   .time_interval_phase = 0 , \
   .function =  (char *)func , \
   .sync_interval = 10 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},


#define define_chd_small(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 10 , \
   .select_type = (char *)"instances" , \
   .inst_num_to_use = 5 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 0 , \
   .time_interval_distance = 0 , \
   .time_interval_phase = 0 , \
   .function =  (char *) fn , \
   .sync_interval = 10 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},

#define define_chd(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 128 , \
   .select_type = (char *)"instances" , \
   .inst_num_to_use = 1 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 0 , \
   .time_interval_distance = 0 , \
   .time_interval_phase = 0 , \
   .function =  (char *) fn , \
   .sync_interval = 10 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},
#define define_mon_chd(n,type,id,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 128 , \
   .select_type = (char *)"instances" , \
   .inst_num_to_use = 3, \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 0 , \
   .time_interval_distance = 0 , \
   .time_interval_phase = 0 , \
   .function =  (char *)"min" , \
   .sync_interval = 10 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},

#define define_chd_day(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 96 , \
   .select_type = (char *)"time" , \
   .inst_num_to_use = 1 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 900 , \
   .time_interval_distance = 900 , \
   .time_interval_phase = 900 , \
   .function =  (char *) fn , \
   .sync_interval = 1 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},
#define define_chd_week(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 168 , \
   .select_type = (char *)"time" , \
   .inst_num_to_use = 1 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 3600 , \
   .time_interval_distance = 3600 , \
   .time_interval_phase = 3600 , \
   .function =  (char *) fn , \
   .sync_interval = 1 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},
#define define_chd_month(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 168 , \
   .select_type = (char *)"time" , \
   .inst_num_to_use = 1 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 14400 , \
   .time_interval_distance = 14400 , \
   .time_interval_phase = 14400 , \
   .function =  (char *) fn , \
   .sync_interval = 1 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},

#define define_chd_daily(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 24 , \
   .select_type = (char *)"time" , \
   .inst_num_to_use = 1 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 60 , \
   .time_interval_distance = 60 , \
   .time_interval_phase = 60 , \
   .function =  (char *) fn , \
   .sync_interval = 1 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},
#define define_chd_monthly(n,type,id,fn,lbl,ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = 28 , \
   .select_type = (char *)"time" , \
   .inst_num_to_use = 1 , \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = 600 , \
   .time_interval_distance = 600 , \
   .time_interval_phase = 600 , \
   .function =  (char *) fn , \
   .sync_interval = 20 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},


#define define_time_chd_generic(n, type, id, num_keep, num_use, int_length, int_dist, int_phase, func, lbl, ut) \
 { .name =  (char *)n  , \
   .enable =  (char *)"true"  , \
   .source_type =  (char *)type  , \
   .source_id = (char *)id  , \
   .source_node = NULL , \
   .num_to_keep = num_keep , \
   .select_type = (char *)"time" , \
   .inst_num_to_use = num_use, \
   .inst_num_to_advance = 1 , \
   .num_to_cache = -1, \
   .time_interval_length = int_length, \
   .time_interval_distance = int_dist, \
   .time_interval_phase = int_phase , \
   .function =  (char *)func , \
   .sync_interval = 10 , \
   .calc_partial = (char *)"false" , \
   .alarm_partial = (char *)"false" , \
   .label = (char *)lbl , \
   .unit = (char *)ut , \
},

struct alarm_config
{
    /*! Alarm node specific details */
    char *name;                  /* string */
    char *enable;                 /* bool */
    char *disable_allowed;        /* bool */
    char *ignore_first_value;     /* bool */
    char *trigger_type;          /* string {sample,chd} */
    char *trigger_id;            /* string */
    char *trigger_node_pattern;  /* name */
    char *rising_enable;          /* bool */
    char *falling_enable;         /* bool */
    bn_type data_type;           /* bt_type to specify datatype */
    char *rising_err_threshold;  /* any */
    char *falling_err_threshold; /* any */
    char *rising_clr_threshold;  /* any */
    char *falling_clr_threshold; /* any */
    char *rising_event_on_clr;    /* bool */
    char *falling_event_on_clr;   /* bool */
    char *event_name_root;       /* string */
    char *clear_if_missing;       /* bool */

    char *rate_limit_win_short;   /* duration sec*/
    char *rate_limit_max_short;   /* uint32 */
    char *rate_limit_win_medium;  /* duration sec*/
    char *rate_limit_max_medium;  /* uint32 */
    char *rate_limit_win_long;    /* duration sec*/
    char *rate_limit_max_long;    /* uint32 */

};
#define define_alarm_rising(n,en,nde,data_t,err_thres,clr_thres) \
 { .name =  (char *)n  , \
   .enable =  (char *)en  , \
   .disable_allowed = (char *)"true" , \
   .ignore_first_value = (char *)"true" , \
   .trigger_type =  (char *)"chd"  , \
   .trigger_id =  (char *)n  , \
   .trigger_node_pattern =  (char *)nde  , \
   .rising_enable = (char *)"true" , \
   .falling_enable = (char *)"false" , \
   .data_type =  data_t  , \
   .rising_err_threshold =  (char *)err_thres  , \
   .rising_clr_threshold =  (char *)clr_thres  , \
   .falling_err_threshold = (char *)"0" , \
   .falling_clr_threshold = (char *)"0" , \
   .rising_event_on_clr = (char *)"true" , \
   .falling_event_on_clr = (char *)"true" , \
   .event_name_root =  (char *)n  , \
   .clear_if_missing = (char *)"true" , \
   .rate_limit_max_type = NULL , \
   .rate_limit_win_type = NULL , \
},

#define define_alarm(n, en, tr_id, nde, ris_en, fall_en, data_t, ris_err_thres,ris_clr_thres, fall_err_thres, fall_clr_thres, event) \
 { .name =  (char *)n  , \
   .enable =  (char *)en  , \
   .disable_allowed = (char *)"true" , \
   .ignore_first_value = (char *)"true" , \
   .trigger_type =  (char *)"chd"  , \
   .trigger_id =  (char *)tr_id  , \
   .trigger_node_pattern =  (char *)nde  , \
   .rising_enable = (char *)ris_en , \
   .falling_enable = (char *)fall_en , \
   .data_type =  data_t  , \
   .rising_err_threshold =  (char *)ris_err_thres  , \
   .rising_clr_threshold =  (char *)ris_clr_thres  , \
   .falling_err_threshold = (char *)fall_err_thres , \
   .falling_clr_threshold = (char *)fall_clr_thres, \
   .rising_event_on_clr = (char *)"true" , \
   .falling_event_on_clr = (char *)"true" , \
   .event_name_root =  (char *)event  , \
   .clear_if_missing = (char *)"true" , \
   .rate_limit_max_short = (char *)"5" , \
   .rate_limit_win_short = (char *)"3600" , \
   .rate_limit_max_medium = (char *)"20" , \
   .rate_limit_win_medium = (char *)"86400" , \
   .rate_limit_max_long = (char *)"50" , \
   .rate_limit_win_long = (char *)"604800" , \
},

#define define_sample_end define_sample

struct sample_config nkn_sample_cfg_entries[] = {

    define_sample_small("current_bw_serv_tx", "/stat/nkn/interface/bw/service_intf_bw_tx","delta",
	"Total tx BW","Bytes")

    define_sample("resource_pool", "/nkn/nvsd/resource_mgr/monitor/state/*/used/p_age/*" ,"direct",
	"Resource Monitoring"," ")
    define_sample("total_bytes", sp "/nvsd/total_byte_count","delta",
	"Total Data served","Bytes")
    define_sample("num_of_connections", sp "/nvsd/num_http_req","delta",
	"Number of new Connections"," ")
    define_sample("cache_byte_count", sp "/nvsd/cache_byte_count","delta",
	"Cache Data served","Bytes")
    define_sample("origin_byte_count", sp "/nvsd/origin_byte_count","delta",
	"Origin Data served","Bytes")
    define_sample("disk_byte_count", sp "/nvsd/disk_byte_count",
	"delta","Disk Data served","Bytes")
    define_sample("http_transaction_count", sp "/nvsd/num_http_transaction",
	"delta","HTTP Transactions Count", " ")

    define_sample("connection_count", sp "/nvsd/num_connections", "direct",
        "Total Active Connections", " ")

    define_sample_day("intf_day", "/net/interface/state/*/stats/*/bytes",
        "delta","Data served on Port","Bytes")
    define_sample_week("intf_week", "/net/interface/state/*/stats/*/bytes",
        "delta","Data served on Port","Bytes")
    define_sample_month("intf_month", "/net/interface/state/*/stats/*/bytes",
        "delta","Data served on Port","Bytes")

    define_sample_day("total_bytes_day", sp "/nvsd/total_byte_count","delta",
	"Total Data served on a Day","Bytes")
    define_sample_day("cache_byte_count_day", sp "/nvsd/cache_byte_count",
        "delta", "Cache Data served On a Day","Bytes")
    define_sample_day("origin_byte_count_day", sp "/nvsd/origin_byte_count",
        "delta", "Origin Data served","Bytes")
    define_sample_day("disk_byte_count_day", sp "/nvsd/disk_byte_count",
	"delta","Disk Data served On a Day","Bytes")

    define_sample_week("total_bytes_week", sp "/nvsd/total_byte_count",
        "delta", "Total Data served On a week","Bytes")
    define_sample_week("cache_byte_count_week", sp "/nvsd/cache_byte_count",
        "delta", "Cache Data served On a week","Bytes")
    define_sample_week("origin_byte_count_week", sp "/nvsd/origin_byte_count",
        "delta", "Origin Data served On a week","Bytes")
    define_sample_week("disk_byte_count_week", sp "/nvsd/disk_byte_count",
	"delta","Disk Data served On a week","Bytes")

    define_sample_generic("mem_util", "/stat/nkn/system/memory/memutil_pct", 10, 120,
				"direct", "none", "Memory Utilization", "Percent")

    define_sample_generic("sas_disk_space", "/nkn/monitor/sas/disk/*/freespace", 10, 120,
				"direct", "none", "Free Space on SAS Disks", "Bytes")
    define_sample_generic("sata_disk_space", "/nkn/monitor/sata/disk/*/freespace", 10, 120,
				"direct", "none", "Free Space on SATA Disks", "Bytes")
    define_sample_generic("ssd_disk_space", "/nkn/monitor/ssd/disk/*/freespace", 10, 120,
				"direct", "none", "Free Space on SSD Disks", "Bytes")
    define_sample_generic("root_disk_space", "/nkn/monitor/root/disk/*/freespace", 10, 120,
				"direct", "none", "Free Space on Root Disk", "Bytes")

    define_sample_generic("sas_disk_read", "/nkn/monitor/sas/disk/*/read", 10, 120,
                                "delta", "increasing", "Bytes read from SAS Disks", "Bytes")
    define_sample_generic("sata_disk_read", "/nkn/monitor/sata/disk/*/read", 10, 120,
                                "delta", "increasing", "Bytes read from SATA Disks", "Bytes")
    define_sample_generic("ssd_disk_read", "/nkn/monitor/ssd/disk/*/read", 10, 120,
                                "delta", "increasing", "Bytes read from SSD Disks", "Bytes")
    define_sample_generic("root_disk_read", "/nkn/monitor/root/disk/*/read", 10, 120,
                                "delta", "increasing", "Bytes read from Root Disk", "Bytes")

    define_sample_generic("sas_disk_io", "/nkn/monitor/sas/disk/*/read,/nkn/monitor/sas/disk/*/write", 10, 120,
                                "delta", "increasing", "Bytes written to SAS Disks", "Bytes")
    define_sample_generic("sata_disk_io", "/nkn/monitor/sata/disk/*/read,/nkn/monitor/sata/disk/*/write", 10, 120,
                                "delta", "increasing", "Bytes written to SATA Disks", "Bytes")
    define_sample_generic("ssd_disk_io", "/nkn/monitor/ssd/disk/*/read,/nkn/monitor/ssd/disk/*/write", 10, 120,
                                "delta", "increasing", "Bytes written to SSD Disks", "Bytes")
    define_sample_generic("root_disk_io", "/nkn/monitor/root/disk/*/read,/nkn/monitor/root/disk/*/write", 10, 120,
                                "delta", "increasing", "Bytes written to Root Disk", "Bytes")

    define_sample_generic("appl_cpu_util", "/nkn/monitor/lfd/appmaxutil", 10, 120,
                                "direct", "none", "Application CPU Utilization", "Percent")

    define_sample_generic("cache_hit",
		"/stat/nkn/nvsd/tot_cl_send,/stat/nkn/nvsd/tot_cl_send_bm_dm", 60, 1440,
		"delta", "increasing", "Data delivered from cache and total data delivered", "Bytes")
    define_sample_generic("filter_acp_rate", sp"namespace/*/url_filter_acp_cnt",
		5, 5, "delta", "none", "filter_accept_rate",
		"Number of accepted transactions per sec")
    define_sample_generic("filter_rej_rate", sp"namespace/*/url_filter_rej_cnt",
		5, 5, "delta", "none", "filter_reject_rate",
		"Number of rejected transactions per sec")

    define_sample_end(NULL,NULL,NULL,NULL,NULL) // { NULL, NULL, NULL, NULL }
};

struct chd_config nkn_chd_cfg_entries[] = {

    define_chd_small("current_bw_serv_rate_tx","sample","current_bw_serv_tx","mean",
	"Service Intf tx Bandwidth","Bytes/Sec")

    define_chd("total_byte_rate","sample","total_bytes","total_byte_rate_pct",
	"Bandwidth","Bytes/Sec")
    define_chd("connection_rate","sample","num_of_connections",
	"connection_rate_pct","Connection Rate","Per Sec")
    define_chd("cache_byte_rate","sample","cache_byte_count",
	"cache_byte_rate_pct","Cache Bandwidth","Bytes/Sec")
    define_chd("origin_byte_rate","sample","origin_byte_count",
	"origin_byte_rate_pct","Origin Bandwidth","Bytes/Sec")
    define_chd("disk_byte_rate","sample","disk_byte_count",
	"disk_byte_rate_pct","Disk Bandwidth","Bytes/Sec")
    define_chd("http_transaction_rate","sample","http_transaction_count",
	"http_transaction_rate_pct","HTTP Transaction Rate","Per Sec")

    define_chd_day("bandwidth_day_avg", "chd" , "total_byte_rate",
        "mean", "Average Bandwidth Usage Last 15 Mins", "Bytes/Sec")
    define_chd_day("bandwidth_day_peak", "chd" , "total_byte_rate",
        "max", "Peak Bandwidth Usage Last 15 Mins", "Bytes/Sec")
    define_chd_day("connection_day_avg", "sample", "connection_count",
        "mean", "Average Connection Count Last 15 Mins", "Count")
    define_chd_day("connection_day_peak", "sample", "connection_count",
        "max", "Peak Connection Count Last 15 Mins", "Count")

    define_chd_week("bandwidth_week_avg", "chd", "bandwidth_day_avg",
        "mean", "Average Bandwidth Usage Last one hour", "Bytes/Sec")
    define_chd_week("bandwidth_week_peak", "chd", "bandwidth_day_peak",
        "max", "Peak Bandwidth Usage Last one hour", "Bytes/Sec")
    define_chd_week("connection_week_avg", "chd", "connection_day_avg",
        "mean", "Average Connection Count Last one hour", "Count")
    define_chd_week("connection_week_peak", "chd", "connection_day_peak",
        "max", "Peak Connection in Last one hour", "Count")

    define_chd_month("bandwidth_month_avg", "chd", "bandwidth_day_avg",
        "mean", "Average Bandwidth Usage Last four hours", "Bytes/Sec")
    define_chd_month("bandwidth_month_peak", "chd", "bandwidth_day_peak",
        "max", "Peak Bandwidth Usage Last four hours", "Bytes/Sec")
    define_chd_month("connection_month_avg", "chd", "connection_day_avg",
        "mean", "Average Connection Count Last four hours", "Count")
    define_chd_month("connection_month_peak", "chd", "connection_day_peak",
        "max", "Peak Connection in Last four hours", "Count")

    define_chd("tot_bandwidth_day","sample","total_bytes_day",
        "total_byte_rate_pct", "Hourly Bandwidth Usage ","Bytes/Sec")
    define_chd("cache_bandwidth_day","sample","cache_byte_count_day",
	"cache_byte_rate_pct","Hourly Cache Bandwidth Usage","Bytes/Sec")
    define_chd("origin_bandwidth_day","sample","origin_byte_count_day",
	"origin_byte_rate_pct","Hourly Origin Bandwidth Usage","Bytes/Sec")
    define_chd("disk_bandwidth_day","sample","disk_byte_count_day",
	"disk_byte_rate_pct","Hourly Disk Bandwidth Usage","Bytes/Sec")

    define_chd("tot_bandwidth_week","sample","total_bytes_week",
        "total_byte_rate_pct", "Daily Bandwidth Usage ","Bytes/Sec")
    define_chd("cache_bandwidth_week","sample","cache_byte_count_week",
	"cache_byte_rate_pct","Daily Cache Bandwidth Usage","Bytes/Sec")
    define_chd("origin_bandwidth_week","sample","origin_byte_count_week",
	"origin_byte_rate_pct","Daily Origin Bandwidth Usage","Bytes/Sec")
    define_chd("disk_bandwidth_week","sample","disk_byte_count_week",
	"disk_byte_rate_pct","Daily Disk Bandwidth Usage","Bytes/Sec")
    define_chd("intf_month","sample", "intf_month",
         "total_byte_rate_pct","Monthly Interface statistics","Bytes/Sec")

    define_mon_chd("mon_total_byte_rate", "chd", "total_byte_rate",
	"Monitor Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_connection_rate", "chd", "connection_rate",
	"Monitor Connection Rate", "Per Sec")
    define_mon_chd("mon_cache_byte_rate", "chd", "cache_byte_rate",
	"Monitor Cache Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_origin_byte_rate", "chd", "origin_byte_rate",
	"Monitor Origin Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_disk_byte_rate", "chd", "disk_byte_rate",
	"Monitor Disk Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_http_transaction_rate", "chd", "http_transaction_rate",
	"Monitor HTTP Transaction Rate", "Per Sec")
    define_mon_chd("mon_intf_util", "chd", "intf_util",
	"Monitor Net Utilization", "Per Sec")
    define_mon_chd("mon_paging", "sample", "paging",
	"Monitor Paging", "Number")
    define_chd_generic("mem_util_ave", "sample", "mem_util", 128, 6, 1, "mean",
	"Average Memory Utilization", "Percent")

    define_chd_generic("sas_disk_space", "sample", "sas_disk_space", 128, 3, 1, "max",
        "Monitor free space on SAS Disks", "Bytes")
    define_chd_generic("sata_disk_space", "sample", "sata_disk_space", 128, 3, 1, "max",
        "Monitor free space on SATA Disks", "Bytes")
    define_chd_generic("ssd_disk_space", "sample", "ssd_disk_space", 128, 3, 1, "max",
        "Monitor free space on SSD Disks", "Bytes")
    define_chd_generic("root_disk_space", "sample", "root_disk_space", 128, 3, 1, "max",
        "Monitor free space on Root Disk Partitions", "Bytes")

    define_chd_generic("sas_disk_bw", "sample", "sas_disk_read", 128, 1, 1, "nkn_disk_bw",
        "Disk Bandwidth of SAS Disks", "Bytes/sec")
    define_chd_generic("sata_disk_bw", "sample", "sata_disk_read", 128, 1, 1, "nkn_disk_bw",
        "Disk Bandwidth of SATA Disks", "Bytes/sec")
    define_chd_generic("ssd_disk_bw", "sample", "ssd_disk_read", 128, 1, 1, "nkn_disk_bw",
        "Disk Bandwidth of SSD Disks", "Bytes/sec")
    define_chd_generic("root_disk_bw", "sample", "root_disk_read", 128, 1, 1, "nkn_disk_bw",
        "Disk Bandwidth of Root Disk Partitions", "Bytes/sec")

    define_chd_generic("mon_sas_disk_bw", "chd", "sas_disk_bw", 128, 3, 1, "min",
        "Monitor Disk Bandwidth of SAS Disks", "Bytes/sec")
    define_chd_generic("mon_sata_disk_bw", "chd", "sata_disk_bw", 128, 3, 1, "max",
        "Monitor Disk Bandwidth of SATA Disks", "Bytes/sec")
    define_chd_generic("mon_ssd_disk_bw", "chd", "ssd_disk_bw", 128, 3, 1, "min",
        "Monitor Disk Bandwidth of SSD Disks", "Bytes/sec")
    define_chd_generic("mon_root_disk_bw", "chd", "root_disk_bw", 128, 3, 1, "min",
        "Monitor Disk Bandwidth of Root Disk Partitions", "Bytes/sec")

    define_chd_generic("sas_disk_io", "sample", "sas_disk_io", 128, 1, 1, "nkn_disk_io",
        "Disk I/O of SAS Disks", "Bytes/sec")
    define_chd_generic("sata_disk_io", "sample", "sata_disk_io", 128, 1, 1, "nkn_disk_io",
        "Disk I/O of SATA Disks", "Bytes/sec")
    define_chd_generic("ssd_disk_io", "sample", "ssd_disk_io", 128, 1, 1, "nkn_disk_io",
        "Disk I/O of SSD Disks", "Bytes/sec")
    define_chd_generic("root_disk_io", "sample", "root_disk_io", 128, 1, 1, "nkn_disk_io",
        "Disk I/O of Root Disk Partitions", "Bytes/sec")

    define_chd_generic("sas_disk_io_ave", "chd", "sas_disk_io", 128, 6, 1, "mean",
        "Average Disk I/O of SAS Disks", "Bytes/sec")
    define_chd_generic("sata_disk_io_ave", "chd", "sata_disk_io", 128, 6, 1, "mean",
        "Average Disk I/O of SATA Disks", "Bytes/sec")
    define_chd_generic("ssd_disk_io_ave", "chd", "ssd_disk_io", 128, 6, 1, "mean",
        "Average Disk I/O of SSD Disks", "Bytes/sec")
    define_chd_generic("root_disk_io_ave", "chd", "root_disk_io", 128, 6, 1, "mean",
        "Average Disk I/O of Root Disk Partitions", "Bytes/sec")

    define_chd_generic("appl_cpu_util", "sample", "appl_cpu_util", 128, 6, 1, "mean",
        "1 min Average of Application CPU Utilization", "Percent")
    define_time_chd_generic("cache_hit_ratio", "sample", "cache_hit", 1440, 1, 86400, 60, 0, "cache_hit_ratio",
	"Cache Hit Ratio", "Percent")
};

/* This sample array is deprecated.  It's not removed from the code only because it's used in upgrade rules.  Modify nkn_sample_cfg_entries instead. */

struct sample_config sample_cfg_entries[] = {
//    define_sample_small("current_bw_serv", "/stat/nkn/interface/bw/service_intf_bw","delta",
//	"Total txrx BW","Bytes")
    define_sample_small("current_bw_serv_tx", "/stat/nkn/interface/bw/service_intf_bw_tx","delta",
	"Total tx BW","Bytes")
//    define_sample_small("current_bw_serv_rx", "/stat/nkn/interface/bw/service_intf_bw_rx","delta",
//	"Total rx BW","Bytes")
    define_sample("resource_pool", "/nkn/nvsd/resource_mgr/monitor/state/*/used/p_age/*" ,"direct",
	"Resource Monitoring"," ")
    define_sample("total_bytes", sp "/nvsd/total_byte_count","delta",
	"Total Data served","Bytes")
    define_sample("num_of_connections", sp "/nvsd/num_http_req","delta",
	"Number of new Connections"," ")
    define_sample("cache_byte_count", sp "/nvsd/cache_byte_count","delta",
	"Cache Data served","Bytes")
    define_sample("origin_byte_count", sp "/nvsd/origin_byte_count","delta",
	"Origin Data served","Bytes")
    define_sample("disk_byte_count", sp "/nvsd/disk_byte_count",
	"delta","Disk Data served","Bytes")
    define_sample("http_transaction_count", sp "/nvsd/num_http_transaction",
	"delta","HTTP Transactions Count", " ")
    define_sample("total_cache_byte_count", sp "/nvsd/cache_byte_count",
	"direct","Total Cache Data Served","Bytes")
    define_sample("total_disk_byte_count", sp "/nvsd/disk_byte_count","direct",
	"Total Disk Data Served","Bytes")
    define_sample("total_origin_byte_count", sp "/nvsd/origin_byte_count",
	"direct","Total Origin Data served","Bytes")
    define_sample("perportbytes", "/net/interface/state/*/stats/tx/bytes",
	"delta","Data served on Port","Bytes")
    define_sample("peroriginbytes", sp "/proxy/origin_fetched_bytes",
	"direct","Data served from Origin","Bytes")
    define_sample("perdiskbytes", sp "/disk_global/read_bytes","direct",
	"Data served from Disk","Bytes")
    define_sample("connection_count", sp "/nvsd/num_connections", "direct",
        "Total Active Connections", " ")

    define_sample_day("intf_day", "/net/interface/state/*/stats/*/bytes",
        "delta","Data served on Port","Bytes")
    define_sample_week("intf_week", "/net/interface/state/*/stats/*/bytes",
        "delta","Data served on Port","Bytes")
    define_sample_month("intf_month", "/net/interface/state/*/stats/*/bytes",
        "delta","Data served on Port","Bytes")

    define_sample_day("total_bytes_day", sp "/nvsd/total_byte_count","delta",
	"Total Data served on a Day","Bytes")
    define_sample_day("cache_byte_count_day", sp "/nvsd/cache_byte_count",
        "delta", "Cache Data served On a Day","Bytes")
    define_sample_day("origin_byte_count_day", sp "/nvsd/origin_byte_count",
        "delta", "Origin Data served","Bytes")
    define_sample_day("disk_byte_count_day", sp "/nvsd/disk_byte_count",
	"delta","Disk Data served On a Day","Bytes")

    define_sample_week("total_bytes_week", sp "/nvsd/total_byte_count",
        "delta", "Total Data served On a week","Bytes")
    define_sample_week("cache_byte_count_week", sp "/nvsd/cache_byte_count",
        "delta", "Cache Data served On a week","Bytes")
    define_sample_week("origin_byte_count_week", sp "/nvsd/origin_byte_count",
        "delta", "Origin Data served On a week","Bytes")
    define_sample_week("disk_byte_count_week", sp "/nvsd/disk_byte_count",
	"delta","Disk Data served On a week","Bytes")

    define_sample_end(NULL,NULL,NULL,NULL,NULL) // { NULL, NULL, NULL, NULL }
};


/* This chd array is deprecated.  It's not removed from the code only because it's used in upgrade rules.  Modify nkn_chd_cfg_entries instead. */
struct chd_config chd_cfg_entries[] = {
//    define_chd_small("current_bw_serv_rate","sample","current_bw_serv","mean",
//	"Service Intf Bandwidth","Bytes/Sec")
    define_chd_small("current_bw_serv_rate_tx","sample","current_bw_serv_tx","mean",
	"Service Intf tx Bandwidth","Bytes/Sec")
//    define_chd_small("current_bw_serv_rate_rx","sample","current_bw_serv_rx","mean",
//	"Service Intf rx Bandwidth","Bytes/Sec")
    define_chd("total_byte_rate","sample","total_bytes","total_byte_rate_pct",
	"Bandwidth","Bytes/Sec")
    define_chd("connection_rate","sample","num_of_connections",
	"connection_rate_pct","Connection Rate","Per Sec")
    define_chd("cache_byte_rate","sample","cache_byte_count",
	"cache_byte_rate_pct","Cache Bandwidth","Bytes/Sec")
    define_chd("origin_byte_rate","sample","origin_byte_count",
	"origin_byte_rate_pct","Origin Bandwidth","Bytes/Sec")
    define_chd("disk_byte_rate","sample","disk_byte_count",
	"disk_byte_rate_pct","Disk Bandwidth","Bytes/Sec")
    define_chd("http_transaction_rate","sample","http_transaction_count",
	"http_transaction_rate_pct","HTTP Transaction Rate","Per Sec")
    define_chd("avg_cache_byte_rate","sample","total_cache_byte_count",
	"avg_cache_byte_rate_pct","Avg Cache Bandwidth","Bytes/Sec")
    define_chd("avg_origin_byte_rate","sample","total_origin_byte_count",
	"avg_origin_byte_rate_pct","Avg Origin Bandwidth","Bytes/Sec")
    define_chd("avg_disk_byte_rate","sample","total_disk_byte_count",
	"avg_disk_byte_rate_pct","Avg Disk Bandwidth","Bytes/Sec")
    define_chd("perportbyte_rate","sample","perportbytes",
	"perportbyte_rate_pct","Bandwidth served on Port ","Bytes/Sec")
    define_chd("peroriginbyte_rate","sample","peroriginbytes","peroriginbyte_rate_pct","Bandwidth served from Origin ","Bytes/Sec")
    define_chd("perdiskbyte_rate","sample","perdiskbytes",
	"perdiskbyte_rate_pct","Bandwidth Served from Disk","Bytes/Sec")

    define_chd_day("bandwidth_day_avg", "chd" , "total_byte_rate",
        "mean", "Average Bandwidth Usage Last 15 Mins", "Bytes/Sec")
    define_chd_day("bandwidth_day_peak", "chd" , "total_byte_rate",
        "max", "Peak Bandwidth Usage Last 15 Mins", "Bytes/Sec")
    define_chd_day("connection_day_avg", "sample", "connection_count",
        "mean", "Average Connection Count Last 15 Mins", "Count")
    define_chd_day("connection_day_peak", "sample", "connection_count",
        "max", "Peak Connection Count Last 15 Mins", "Count")

    define_chd_week("bandwidth_week_avg", "chd", "bandwidth_day_avg",
        "mean", "Average Bandwidth Usage Last one hour", "Bytes/Sec")
    define_chd_week("bandwidth_week_peak", "chd", "bandwidth_day_peak",
        "max", "Peak Bandwidth Usage Last one hour", "Bytes/Sec")
    define_chd_week("connection_week_avg", "chd", "connection_day_avg",
        "mean", "Average Connection Count Last one hour", "Count")
    define_chd_week("connection_week_peak", "chd", "connection_day_peak",
        "max", "Peak Connection in Last one hour", "Count")

    define_chd_month("bandwidth_month_avg", "chd", "bandwidth_day_avg",
        "mean", "Average Bandwidth Usage Last four hours", "Bytes/Sec")
    define_chd_month("bandwidth_month_peak", "chd", "bandwidth_day_peak",
        "max", "Peak Bandwidth Usage Last four hours", "Bytes/Sec")
    define_chd_month("connection_month_avg", "chd", "connection_day_avg",
        "mean", "Average Connection Count Last four hours", "Count")
    define_chd_month("connection_month_peak", "chd", "connection_day_peak",
        "max", "Peak Connection in Last four hours", "Count")

    define_chd("tot_bandwidth_day","sample","total_bytes_day",
        "total_byte_rate_pct", "Hourly Bandwidth Usage ","Bytes/Sec")
    define_chd("cache_bandwidth_day","sample","cache_byte_count_day",
	"cache_byte_rate_pct","Hourly Cache Bandwidth Usage","Bytes/Sec")
    define_chd("origin_bandwidth_day","sample","origin_byte_count_day",
	"origin_byte_rate_pct","Hourly Origin Bandwidth Usage","Bytes/Sec")
    define_chd("disk_bandwidth_day","sample","disk_byte_count_day",
	"disk_byte_rate_pct","Hourly Disk Bandwidth Usage","Bytes/Sec")

    define_chd("tot_bandwidth_week","sample","total_bytes_week",
        "total_byte_rate_pct", "Daily Bandwidth Usage ","Bytes/Sec")
    define_chd("cache_bandwidth_week","sample","cache_byte_count_week",
	"cache_byte_rate_pct","Daily Cache Bandwidth Usage","Bytes/Sec")
    define_chd("origin_bandwidth_week","sample","origin_byte_count_week",
	"origin_byte_rate_pct","Daily Origin Bandwidth Usage","Bytes/Sec")
    define_chd("disk_bandwidth_week","sample","disk_byte_count_week",
	"disk_byte_rate_pct","Daily Disk Bandwidth Usage","Bytes/Sec")
    define_chd("intf_month","sample", "intf_month",
         "total_byte_rate_pct","Monthly Interface statistics","Bytes/Sec")

    define_mon_chd("mon_total_byte_rate", "chd", "total_byte_rate",
	"Monitor Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_connection_rate", "chd", "connection_rate",
	"Monitor Connection Rate", "Per Sec")
    define_mon_chd("mon_cache_byte_rate", "chd", "cache_byte_rate",
	"Monitor Cache Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_origin_byte_rate", "chd", "origin_byte_rate",
	"Monitor Origin Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_disk_byte_rate", "chd", "disk_byte_rate",
	"Monitor Disk Bandwidth", "Bytes/Sec")
    define_mon_chd("mon_http_transaction_rate", "chd", "http_transaction_rate",
	"Monitor HTTP Transaction Rate", "Per Sec")

};

struct alarm_config alarm_cfg_entries[] = {

    define_alarm("connection_rate", "true", "mon_connection_rate", sp "/nvsd/connection_rate",
	"true", "false", bt_uint32, "20000", "10000", "0", "0", "connection_rate")
    define_alarm("cache_byte_rate", "true", "mon_cache_byte_rate", sp "/nvsd/cache_byte_rate",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "cache_byte_rate")
    define_alarm("origin_byte_rate", "true","mon_origin_byte_rate", sp "/nvsd/origin_byte_rate",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "origin_byte_rate")
    define_alarm("disk_byte_rate", "false", "mon_disk_byte_rate", sp "/nvsd/disk_byte_rate",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "disk_byte_rate")
    define_alarm("http_transaction_rate", "true", "mon_http_transaction_rate", sp "/nvsd/http_transaction_rate",
	"true", "false", bt_uint32, "40000", "20000", "0", "0", "http_transaction_rate")
    define_alarm("aggr_cpu_util", "true", "cpu_util_ave", "/system/cpu/all/busy_pct",
	"true", "true", bt_uint32, "90", "70", "0", "0", "aggr_cpu_util")
    define_alarm("nkn_mem_util", "true", "mem_util_ave", "/stat/nkn/system/memory/memutil_pct",
	"true", "true", bt_uint32, "90", "87", "0", "0", "nkn_mem_util")

    define_alarm("sas_disk_space", "true", "sas_disk_space", "/nkn/monitor/sas/disk/*/freespace",
	"false", "true", bt_uint32, "0", "0", "7", "10", "nkn_disk_space")
    define_alarm("sata_disk_space", "true", "sata_disk_space", "/nkn/monitor/sata/disk/*/freespace",
	"false", "true", bt_uint32, "0", "0", "7", "10", "nkn_disk_space")
    define_alarm("ssd_disk_space", "true", "ssd_disk_space", "/nkn/monitor/ssd/disk/*/freespace",
	"false", "true", bt_uint32, "0", "0", "7", "10", "nkn_disk_space")
    define_alarm("root_disk_space", "true", "root_disk_space", "/nkn/monitor/root/disk/*/freespace",
	"false", "true", bt_uint32, "0", "0", "7", "10", "nkn_disk_space")

    define_alarm("sas_disk_bw", "true", "mon_sas_disk_bw", "/nkn/monitor/sas/disk/*/disk_bw",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "nkn_disk_bw")
    define_alarm("sata_disk_bw", "true", "mon_sata_disk_bw", "/nkn/monitor/sata/disk/*/disk_bw",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "nkn_disk_bw")
    define_alarm("ssd_disk_bw", "true", "mon_ssd_disk_bw", "/nkn/monitor/ssd/disk/*/disk_bw",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "nkn_disk_bw")
    define_alarm("root_disk_bw", "true", "mon_root_disk_bw", "/nkn/monitor/root/disk/*/disk_bw",
	"true", "false", bt_uint32, "200000000", "100000000", "0", "0", "nkn_disk_bw")

    define_alarm("sas_disk_io", "true", "sas_disk_io_ave", "/nkn/monitor/sas/disk/*/disk_io",
	"true", "false", bt_uint32, "5120000", "4608000", "0", "0", "nkn_disk_io")
    define_alarm("sata_disk_io", "true", "sata_disk_io_ave", "/nkn/monitor/sata/disk/*/disk_io",
	"true", "false", bt_uint32, "5120000", "4608000", "0", "0", "nkn_disk_io")
    define_alarm("ssd_disk_io", "true", "ssd_disk_io_ave", "/nkn/monitor/ssd/disk/*/disk_io",
	"true", "false", bt_uint32, "5120000", "4608000", "0", "0", "nkn_disk_io")
    define_alarm("root_disk_io", "true", "root_disk_io_ave", "/nkn/monitor/root/disk/*/disk_io",
	"true", "false", bt_uint32, "5120000", "4608000", "0", "0", "nkn_disk_io")

    define_alarm("appl_cpu_util", "true", "appl_cpu_util", "/nkn/monitor/lfd/appmaxutil",
	"true", "false", bt_uint32, "90", "70", "0", "0", "appl_cpu_util")

    define_alarm("cache_hit_ratio", "true", "cache_hit_ratio", "/stat/nkn/nvsd/cache_hit_ratio",
	"false", "true", bt_uint32, "0", "0", "5", "20", "cache_hit_ratio")

};

#define sc "/stats/config/"


/*! Real time statistics collection node definition and
 * their corresponding thresholding logic
 */
const bn_str_value md_nkn_bw_stats_initial_values[] = {
        {sc "sample/total_bytes",          bt_string,   "total_bytes"},
        {sc "sample/total_bytes/enable",   bt_bool,     "true"},
        {sc "sample/total_bytes/node/\\/stat\\/nkn\\/nvsd\\/total_byte_count",
                bt_name, "/stat/nkn/nvsd/total_byte_count"},
        {sc "sample/total_bytes/interval", bt_duration, "60"},
        {sc "sample/total_bytes/num_to_keep", bt_uint32, "10"},
        {sc "sample/total_bytes/sample_method", bt_string, "delta"},
        {sc "chd/total_byte_rate",              bt_string,      "total_byte_rate"},
        {sc "chd/total_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/total_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/total_byte_rate/source/id",    bt_string,      "total_bytes"},
        {sc "chd/total_byte_rate/num_to_keep",  bt_uint32,      "120"},
        {sc "chd/total_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/total_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/total_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/total_byte_rate/function", bt_string, "total_byte_rate_pct"},

	{sc "alarm/total_byte_rate", bt_string, "total_byte_rate"},
    	{sc "alarm/total_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/total_byte_rate/event_name_root",  bt_string, "total_byte_rate"},
    	{sc "alarm/total_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/total_byte_rate/trigger/id",       bt_string, "total_byte_rate"},
   	{sc "alarm/total_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/total_byte_rate"},
    	{sc "alarm/total_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/total_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/total_byte_rate/rising/clear_threshold", bt_uint32,  "100000000"},
    	{sc "alarm/total_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/total_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/total_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/total_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/total_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/total_byte_rate/clear_if_missing",       bt_bool,    "false"},
};

const bn_str_value md_nkn_con_rate_stats_initial_values[] = {
        {sc "sample/num_of_connections",          bt_string,   "num_of_connections"},
        {sc "sample/num_of_connections/enable",   bt_bool,     "true"},
        {sc "sample/num_of_connections/node/\\/stat\\/nkn\\/nvsd\\/num_http_req",
                bt_name, "/stat/nkn/nvsd/num_http_req"},
        {sc "sample/num_of_connections/interval", bt_duration, "60"},
        {sc "sample/num_of_connections/num_to_keep", bt_uint32, "10"},
        {sc "sample/num_of_connections/sample_method", bt_string, "direct"},

        {sc "chd/connection_rate",              bt_string,      "connection_rate"},
        {sc "chd/connection_rate/enable",       bt_bool,        "true"},
        {sc "chd/connection_rate/source/type",  bt_string,      "sample"},
        {sc "chd/connection_rate/source/id",    bt_string,      "num_of_connections"},
        {sc "chd/connection_rate/num_to_keep",  bt_uint32,      "10"},
        {sc "chd/connection_rate/select_type",  bt_string,      "instances"},
        {sc "chd/connection_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/connection_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/connection_rate/function", bt_string, "connection_rate_pct"},

	{sc "alarm/connection_rate", bt_string, "connection_rate"},
    	{sc "alarm/connection_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/connection_rate/event_name_root",  bt_string, "connection_rate"},
    	{sc "alarm/connection_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/connection_rate/trigger/id",       bt_string, "connection_rate"},
   	{sc "alarm/connection_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/connection_rate"},
    	{sc "alarm/connection_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/connection_rate/rising/error_threshold", bt_uint32,  "20000"},
    	{sc "alarm/connection_rate/rising/clear_threshold", bt_uint32,  "10000"},
    	{sc "alarm/connection_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/connection_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/connection_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/connection_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/connection_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/connection_rate/clear_if_missing",       bt_bool,    "false"},
};


const bn_str_value md_nkn_cache_bw_stats_initial_values[] = {
        {sc "sample/cache_byte_count",          bt_string,   "cache_byte_count"},
        {sc "sample/cache_byte_count/enable",   bt_bool,     "true"},
        {sc "sample/cache_byte_count/node/\\/stat\\/nkn\\/nvsd\\/cache_byte_count",
                bt_name, "/stat/nkn/nvsd/cache_byte_count"},
        {sc "sample/cache_byte_count/interval", bt_duration, "60"},
        {sc "sample/cache_byte_count/num_to_keep", bt_uint32, "10"},
        {sc "sample/cache_byte_count/sample_method", bt_string, "delta"},

        {sc "chd/cache_byte_rate",              bt_string,      "cache_byte_rate"},
        {sc "chd/cache_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/cache_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/cache_byte_rate/source/id",    bt_string,      "cache_byte_count"},
        {sc "chd/cache_byte_rate/num_to_keep",  bt_uint32,      "10"},
        {sc "chd/cache_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/cache_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/cache_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/cache_byte_rate/function", bt_string, "cache_byte_rate_pct"},

	{sc "alarm/cache_byte_rate", bt_string, "cache_byte_rate"},
    	{sc "alarm/cache_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/cache_byte_rate/event_name_root",  bt_string, "cache_byte_rate"},
    	{sc "alarm/cache_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/cache_byte_rate/trigger/id",       bt_string, "cache_byte_rate"},
   	{sc "alarm/cache_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/cache_byte_rate"},
    	{sc "alarm/cache_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/cache_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/cache_byte_rate/rising/clear_threshold", bt_uint32,  "10000000"},
    	{sc "alarm/cache_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/cache_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/cache_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/cache_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/cache_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/cache_byte_rate/clear_if_missing",       bt_bool,    "false"},
};

const bn_str_value md_nkn_origin_bw_stats_initial_values[] = {
        {sc "sample/origin_byte_count",          bt_string,   "origin_byte_count"},
        {sc "sample/origin_byte_count/enable",   bt_bool,     "true"},
        {sc "sample/origin_byte_count/node/\\/stat\\/nkn\\/nvsd\\/origin_byte_count",
                bt_name, "/stat/nkn/nvsd/origin_byte_count"},
        {sc "sample/origin_byte_count/interval", bt_duration, "60"},
        {sc "sample/origin_byte_count/num_to_keep", bt_uint32, "10"},
        {sc "sample/origin_byte_count/sample_method", bt_string, "delta"},

        {sc "chd/origin_byte_rate",              bt_string,      "origin_byte_rate"},
        {sc "chd/origin_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/origin_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/origin_byte_rate/source/id",    bt_string,      "origin_byte_count"},
        {sc "chd/origin_byte_rate/num_to_keep",  bt_uint32,      "10"},
        {sc "chd/origin_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/origin_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/origin_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/origin_byte_rate/function", bt_string, "origin_byte_rate_pct"},

	{sc "alarm/origin_byte_rate", bt_string, "origin_byte_rate"},
    	{sc "alarm/origin_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/origin_byte_rate/event_name_root",  bt_string, "origin_byte_rate"},
    	{sc "alarm/origin_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/origin_byte_rate/trigger/id",       bt_string, "origin_byte_rate"},
   	{sc "alarm/origin_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/origin_byte_rate"},
    	{sc "alarm/origin_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/origin_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/origin_byte_rate/rising/clear_threshold", bt_uint32,  "10000000"},
    	{sc "alarm/origin_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/origin_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/origin_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/origin_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/origin_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/origin_byte_rate/clear_if_missing",       bt_bool,    "false"},
};

const bn_str_value md_nkn_disk_bw_stats_initial_values[] = {
        {sc "sample/disk_byte_count",          bt_string,   "disk_byte_count"},
        {sc "sample/disk_byte_count/enable",   bt_bool,     "true"},
        {sc "sample/disk_byte_count/node/\\/stat\\/nkn\\/nvsd\\/disk_byte_count",
                bt_name, "/stat/nkn/nvsd/disk_byte_count"},
        {sc "sample/disk_byte_count/interval", bt_duration, "60"},
        {sc "sample/disk_byte_count/num_to_keep", bt_uint32, "10"},
        {sc "sample/disk_byte_count/sample_method", bt_string, "delta"},

        {sc "chd/disk_byte_rate",              bt_string,      "disk_byte_rate"},
        {sc "chd/disk_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/disk_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/disk_byte_rate/source/id",    bt_string,      "disk_byte_count"},
        {sc "chd/disk_byte_rate/num_to_keep",  bt_uint32,      "10"},
        {sc "chd/disk_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/disk_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/disk_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/disk_byte_rate/function", bt_string, "disk_byte_rate_pct"},

	{sc "alarm/disk_byte_rate", bt_string, "disk_byte_rate"},
    	{sc "alarm/disk_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/disk_byte_rate/event_name_root",  bt_string, "disk_byte_rate"},
    	{sc "alarm/disk_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/disk_byte_rate/trigger/id",       bt_string, "disk_byte_rate"},
   	{sc "alarm/disk_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/disk_byte_rate"},
    	{sc "alarm/disk_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/disk_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/disk_byte_rate/rising/clear_threshold", bt_uint32,  "10000000"},
    	{sc "alarm/disk_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/disk_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/disk_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/disk_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/disk_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/disk_byte_rate/clear_if_missing",       bt_bool,    "false"},
};
const bn_str_value md_nkn_http_trans_rate_stats_initial_values[] = {
        {sc "sample/http_transaction_count",          bt_string,   "http_transaction_count"},
        {sc "sample/http_transaction_count/enable",   bt_bool,     "true"},
        {sc "sample/http_transaction_count/node/\\/stat\\/nkn\\/nvsd\\/num_http_transaction",
                bt_name, "/stat/nkn/nvsd/num_http_transaction"},
        {sc "sample/http_transaction_count/interval", bt_duration, "60"},
        {sc "sample/http_transaction_count/num_to_keep", bt_uint32, "10"},
        {sc "sample/http_transaction_count/sample_method", bt_string, "direct"},

        {sc "chd/http_transaction_rate",              bt_string,      "http_transaction_rate"},
        {sc "chd/http_transaction_rate/enable",       bt_bool,        "true"},
        {sc "chd/http_transaction_rate/source/type",  bt_string,      "sample"},
        {sc "chd/http_transaction_rate/source/id",    bt_string,      "http_transaction_count"},
        {sc "chd/http_transaction_rate/num_to_keep",  bt_uint32,      "10"},
        {sc "chd/http_transaction_rate/select_type",  bt_string,      "instances"},
        {sc "chd/http_transaction_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/http_transaction_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/http_transaction_rate/function", bt_string, "http_transaction_rate_pct"},

	{sc "alarm/http_transaction_rate", bt_string, "http_transaction_rate"},
    	{sc "alarm/http_transaction_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/http_transaction_rate/event_name_root",  bt_string, "http_transaction_rate"},
    	{sc "alarm/http_transaction_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/http_transaction_rate/trigger/id",       bt_string, "http_transaction_rate"},
   	{sc "alarm/http_transaction_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/http_transaction_rate"},
    	{sc "alarm/http_transaction_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/http_transaction_rate/rising/error_threshold", bt_uint32,  "40000"},
    	{sc "alarm/http_transaction_rate/rising/clear_threshold", bt_uint32,  "20000"},
    	{sc "alarm/http_transaction_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/http_transaction_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/http_transaction_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/http_transaction_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/http_transaction_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/http_transaction_rate/clear_if_missing",       bt_bool,    "false"},
};


/*! Average time statistics node definitions
*/

const bn_str_value md_nkn_avg_cache_bw_stats_initial_values[] = {
        {sc "sample/total_cache_byte_count",          bt_string,   "total_cache_byte_count"},
        {sc "sample/total_cache_byte_count/enable",   bt_bool,     "true"},
        {sc "sample/total_cache_byte_count/node/\\/stat\\/nkn\\/nvsd\\/cache_byte_count",
                bt_name, "/stat/nkn/nvsd/cache_byte_count"},
        {sc "sample/total_cache_byte_count/interval", bt_duration, "60"},
        {sc "sample/total_cache_byte_count/num_to_keep", bt_uint32, "2"},
        {sc "sample/total_cache_byte_count/sample_method", bt_string, "direct"},

        {sc "chd/avg_cache_byte_rate",              bt_string,      "avg_cache_byte_rate"},
        {sc "chd/avg_cache_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/avg_cache_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/avg_cache_byte_rate/source/id",    bt_string,      "total_cache_byte_count"},
        {sc "chd/avg_cache_byte_rate/num_to_keep",  bt_uint32,      "2"},
        {sc "chd/avg_cache_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/avg_cache_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/avg_cache_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/avg_cache_byte_rate/function", bt_string, "avg_cache_byte_rate_pct"},

	{sc "alarm/avg_cache_byte_rate", bt_string, "avg_cache_byte_rate"},
    	{sc "alarm/avg_cache_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/avg_cache_byte_rate/event_name_root",  bt_string, "avg_cache_byte_rate"},
    	{sc "alarm/avg_cache_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/avg_cache_byte_rate/trigger/id",       bt_string, "avg_cache_byte_rate"},
   	{sc "alarm/avg_cache_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/avg_cache_byte_rate"},
    	{sc "alarm/avg_cache_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/avg_cache_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/avg_cache_byte_rate/rising/clear_threshold", bt_uint32,  "10000000"},
    	{sc "alarm/avg_cache_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/avg_cache_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/avg_cache_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/avg_cache_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/avg_cache_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/avg_cache_byte_rate/clear_if_missing",       bt_bool,    "false"},
};

const bn_str_value md_nkn_avg_origin_bw_stats_initial_values[] = {
        {sc "sample/total_origin_byte_count",          bt_string,   "total_origin_byte_count"},
        {sc "sample/total_origin_byte_count/enable",   bt_bool,     "true"},
        {sc "sample/total_origin_byte_count/node/\\/stat\\/nkn\\/nvsd\\/origin_byte_count",
                bt_name, "/stat/nkn/nvsd/origin_byte_count"},
        {sc "sample/total_origin_byte_count/interval", bt_duration, "60"},
        {sc "sample/total_origin_byte_count/num_to_keep", bt_uint32, "2"},
        {sc "sample/total_origin_byte_count/sample_method", bt_string, "direct"},

        {sc "chd/avg_origin_byte_rate",              bt_string,      "avg_origin_byte_rate"},
        {sc "chd/avg_origin_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/avg_origin_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/avg_origin_byte_rate/source/id",    bt_string,      "total_origin_byte_count"},
        {sc "chd/avg_origin_byte_rate/num_to_keep",  bt_uint32,      "2"},
        {sc "chd/avg_origin_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/avg_origin_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/avg_origin_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/avg_origin_byte_rate/function", bt_string, "avg_origin_byte_rate_pct"},

	{sc "alarm/avg_origin_byte_rate", bt_string, "avg_origin_byte_rate"},
    	{sc "alarm/avg_origin_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/avg_origin_byte_rate/event_name_root",  bt_string, "avg_origin_byte_rate"},
    	{sc "alarm/avg_origin_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/avg_origin_byte_rate/trigger/id",       bt_string, "avg_origin_byte_rate"},
   	{sc "alarm/avg_origin_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/avg_origin_byte_rate"},
    	{sc "alarm/avg_origin_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/avg_origin_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/avg_origin_byte_rate/rising/clear_threshold", bt_uint32,  "10000000"},
    	{sc "alarm/avg_origin_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/avg_origin_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/avg_origin_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/avg_origin_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/avg_origin_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/avg_origin_byte_rate/clear_if_missing",       bt_bool,    "false"},
};

const bn_str_value md_nkn_avg_disk_bw_stats_initial_values[] = {
        {sc "sample/total_disk_byte_count",          bt_string,   "total_disk_byte_count"},
        {sc "sample/total_disk_byte_count/enable",   bt_bool,     "true"},
        {sc "sample/total_disk_byte_count/node/\\/stat\\/nkn\\/nvsd\\/disk_byte_count",
                bt_name, "/stat/nkn/nvsd/disk_byte_count"},
        {sc "sample/total_disk_byte_count/interval", bt_duration, "60"},
        {sc "sample/total_disk_byte_count/num_to_keep", bt_uint32, "2"},
        {sc "sample/total_disk_byte_count/sample_method", bt_string, "direct"},

        {sc "chd/avg_disk_byte_rate",              bt_string,      "avg_disk_byte_rate"},
        {sc "chd/avg_disk_byte_rate/enable",       bt_bool,        "true"},
        {sc "chd/avg_disk_byte_rate/source/type",  bt_string,      "sample"},
        {sc "chd/avg_disk_byte_rate/source/id",    bt_string,      "total_disk_byte_count"},
        {sc "chd/avg_disk_byte_rate/num_to_keep",  bt_uint32,      "10"},
        {sc "chd/avg_disk_byte_rate/select_type",  bt_string,      "instances"},
        {sc "chd/avg_disk_byte_rate/instances/num_to_use",  bt_uint32,      "1"},
        {sc "chd/avg_disk_byte_rate/instances/num_to_advance",  bt_uint32,      "1"},
        {sc "chd/avg_disk_byte_rate/function", bt_string, "avg_disk_byte_rate_pct"},

	{sc "alarm/avg_disk_byte_rate", bt_string, "avg_disk_byte_rate"},
    	{sc "alarm/avg_disk_byte_rate/enable",           bt_bool,   "true"},
    	{sc "alarm/avg_disk_byte_rate/event_name_root",  bt_string, "avg_disk_byte_rate"},
    	{sc "alarm/avg_disk_byte_rate/trigger/type",     bt_string, "chd"},
    	{sc "alarm/avg_disk_byte_rate/trigger/id",       bt_string, "avg_disk_byte_rate"},
   	{sc "alarm/avg_disk_byte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/avg_disk_byte_rate"},
    	{sc "alarm/avg_disk_byte_rate/rising/enable",          bt_bool,    "true"},
  	{sc "alarm/avg_disk_byte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    	{sc "alarm/avg_disk_byte_rate/rising/clear_threshold", bt_uint32,  "10000000"},
    	{sc "alarm/avg_disk_byte_rate/rising/event_on_clear",  bt_bool,    "true"},
    	{sc "alarm/avg_disk_byte_rate/falling/enable",         bt_bool,    "false"},
  	{sc "alarm/avg_disk_byte_rate/falling/error_threshold", bt_uint32, "0"},
  	{sc "alarm/avg_disk_byte_rate/falling/clear_threshold", bt_uint32, "0"},
  	{sc "alarm/avg_disk_byte_rate/falling/event_on_clear",  bt_bool,   "false"},
    	{sc "alarm/avg_disk_byte_rate/clear_if_missing",       bt_bool,    "false"},
};
const bn_str_value md_nkn_avg_perport_bw_stats_initial_values[] = {
    {sc "sample/perportbytes",	bt_string,   "perportbytes"},
    {sc "sample/perportbytes/enable",	bt_bool,     "true"},
    {sc "sample/perportbytes/node/"
     "\\/net\\/interface\\/state\\/*\\/stats\\/tx\\/bytes", bt_name,
     "/net/interface/state/*/stats/tx/bytes"},
    {sc "sample/perportbytes/interval",         bt_duration,     "60"},
    {sc "sample/perportbytes/num_to_keep",      bt_uint32,       "2"},
    {sc "sample/perportbytes/sample_method",    bt_string,       "delta"},
    {sc "sample/perportbytes/sync_interval",    bt_uint32,       "10"},

    {sc "chd/perportbyte_rate",		        bt_string,      "perportbyte_rate"},
    {sc "chd/perportbyte_rate/enable",       bt_bool,        "true"},
    {sc "chd/perportbyte_rate/source/type",  bt_string,      "sample"},
    {sc "chd/perportbyte_rate/source/id",    bt_string,      "perportbytes"},
    {sc "chd/perportbyte_rate/num_to_keep",  bt_uint32,      "120"},
    {sc "chd/perportbyte_rate/select_type",  bt_string,      "instances"},
    {sc "chd/perportbyte_rate/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/perportbyte_rate/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/perportbyte_rate/function", bt_string, "perportbyte_rate_pct"},

    {sc "alarm/perportbyte_rate", bt_string, "perportbyte_rate"},
    {sc "alarm/perportbyte_rate/enable",           bt_bool,   "true"},
    {sc "alarm/perportbyte_rate/event_name_root",  bt_string, "perportbyte_rate"},
    {sc "alarm/perportbyte_rate/trigger/type",     bt_string, "chd"},
    {sc "alarm/perportbyte_rate/trigger/id",       bt_string, "perportbyte_rate"},
    {sc "alarm/perportbyte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/*/perportbyte_rate"},
    {sc "alarm/perportbyte_rate/rising/enable",          bt_bool,    "true"},
    {sc "alarm/perportbyte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    {sc "alarm/perportbyte_rate/rising/clear_threshold", bt_uint32,  "100000000"},
    {sc "alarm/perportbyte_rate/rising/event_on_clear",  bt_bool,    "true"},
    {sc "alarm/perportbyte_rate/falling/enable",         bt_bool,    "false"},
    {sc "alarm/perportbyte_rate/falling/error_threshold", bt_uint32, "0"},
    {sc "alarm/perportbyte_rate/falling/clear_threshold", bt_uint32, "0"},
    {sc "alarm/perportbyte_rate/falling/event_on_clear",  bt_bool,   "false"},
    {sc "alarm/perportbyte_rate/clear_if_missing",       bt_bool,    "false"},
	
};
const bn_str_value md_nkn_avg_perorigin_bw_stats_initial_values[] = {
    {sc "sample/peroriginbytes",	bt_string,   "peroriginbytes"},
    {sc "sample/peroriginbytes/enable",	bt_bool,     "true"},
    {sc "sample/peroriginbytes/node/"
     "\\/stat\\/nkn\\/proxy\\/origin_fetched_bytes", bt_name,
     "/stat/nkn/proxy/origin_fetched_bytes"},
    {sc "sample/peroriginbytes/interval",         bt_duration,     "60"},
    {sc "sample/peroriginbytes/num_to_keep",      bt_uint32,       "2"},
    {sc "sample/peroriginbytes/sample_method",    bt_string,       "direct"},
    {sc "sample/peroriginbytes/sync_interval",    bt_uint32,       "10"},

    {sc "chd/peroriginbyte_rate",		        bt_string,      "peroriginbyte_rate"},
    {sc "chd/peroriginbyte_rate/enable",       bt_bool,        "true"},
    {sc "chd/peroriginbyte_rate/source/type",  bt_string,      "sample"},
    {sc "chd/peroriginbyte_rate/source/id",    bt_string,      "peroriginbytes"},
    {sc "chd/peroriginbyte_rate/num_to_keep",  bt_uint32,      "120"},
    {sc "chd/peroriginbyte_rate/select_type",  bt_string,      "instances"},
    {sc "chd/peroriginbyte_rate/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/peroriginbyte_rate/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/peroriginbyte_rate/function", bt_string, "peroriginbyte_rate_pct"},

    {sc "alarm/peroriginbyte_rate", bt_string, "peroriginbyte_rate"},
    {sc "alarm/peroriginbyte_rate/enable",           bt_bool,   "true"},
    {sc "alarm/peroriginbyte_rate/event_name_root",  bt_string, "peroriginbyte_rate"},
    {sc "alarm/peroriginbyte_rate/trigger/type",     bt_string, "chd"},
    {sc "alarm/peroriginbyte_rate/trigger/id",       bt_string, "peroriginbyte_rate"},
    {sc "alarm/peroriginbyte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/peroriginbyte_rate"},
    {sc "alarm/peroriginbyte_rate/rising/enable",          bt_bool,    "true"},
    {sc "alarm/peroriginbyte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    {sc "alarm/peroriginbyte_rate/rising/clear_threshold", bt_uint32,  "100000000"},
    {sc "alarm/peroriginbyte_rate/rising/event_on_clear",  bt_bool,    "true"},
    {sc "alarm/peroriginbyte_rate/falling/enable",         bt_bool,    "false"},
    {sc "alarm/peroriginbyte_rate/falling/error_threshold", bt_uint32, "0"},
    {sc "alarm/peroriginbyte_rate/falling/clear_threshold", bt_uint32, "0"},
    {sc "alarm/peroriginbyte_rate/falling/event_on_clear",  bt_bool,   "false"},
    {sc "alarm/peroriginbyte_rate/clear_if_missing",       bt_bool,    "false"},
	
};
const bn_str_value md_nkn_avg_perdisk_bw_stats_initial_values[] = {
    {sc "sample/perdiskbytes",	bt_string,   "perdiskbytes"},
    {sc "sample/perdiskbytes/enable",	bt_bool,     "true"},
    {sc "sample/perdiskbytes/node/"
     "\\/stat\\/nkn\\/disk_global\\/read_bytes", bt_name,
     "/stat/nkn/disk_global/read_bytes"},
    {sc "sample/perdiskbytes/interval",         bt_duration,     "60"},
    {sc "sample/perdiskbytes/num_to_keep",      bt_uint32,       "2"},
    {sc "sample/perdiskbytes/sample_method",    bt_string,       "direct"},
    {sc "sample/perdiskbytes/sync_interval",    bt_uint32,       "10"},

    {sc "chd/perdiskbyte_rate",		        bt_string,      "perdiskbyte_rate"},
    {sc "chd/perdiskbyte_rate/enable",       bt_bool,        "true"},
    {sc "chd/perdiskbyte_rate/source/type",  bt_string,      "sample"},
    {sc "chd/perdiskbyte_rate/source/id",    bt_string,      "perdiskbytes"},
    {sc "chd/perdiskbyte_rate/num_to_keep",  bt_uint32,      "120"},
    {sc "chd/perdiskbyte_rate/select_type",  bt_string,      "instances"},
    {sc "chd/perdiskbyte_rate/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/perdiskbyte_rate/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/perdiskbyte_rate/function", bt_string, "perdiskbyte_rate_pct"},

    {sc "alarm/perdiskbyte_rate", bt_string, "perdiskbyte_rate"},
    {sc "alarm/perdiskbyte_rate/enable",           bt_bool,   "true"},
    {sc "alarm/perdiskbyte_rate/event_name_root",  bt_string, "perdiskbyte_rate"},
    {sc "alarm/perdiskbyte_rate/trigger/type",     bt_string, "chd"},
    {sc "alarm/perdiskbyte_rate/trigger/id",       bt_string, "perdiskbyte_rate"},
    {sc "alarm/perdiskbyte_rate/trigger/node_pattern",   bt_name,    "/stat/nkn/nvsd/perdiskbyte_rate"},
    {sc "alarm/perdiskbyte_rate/rising/enable",          bt_bool,    "true"},
    {sc "alarm/perdiskbyte_rate/rising/error_threshold", bt_uint32,  "200000000"},
    {sc "alarm/perdiskbyte_rate/rising/clear_threshold", bt_uint32,  "100000000"},
    {sc "alarm/perdiskbyte_rate/rising/event_on_clear",  bt_bool,    "true"},
    {sc "alarm/perdiskbyte_rate/falling/enable",         bt_bool,    "false"},
    {sc "alarm/perdiskbyte_rate/falling/error_threshold", bt_uint32, "0"},
    {sc "alarm/perdiskbyte_rate/falling/clear_threshold", bt_uint32, "0"},
    {sc "alarm/perdiskbyte_rate/falling/event_on_clear",  bt_bool,   "false"},
    {sc "alarm/perdiskbyte_rate/clear_if_missing",       bt_bool,    "false"},
};

const bn_str_value md_nkn_cpu_util_average_initial_values[] = {
        {sc "alarm/nkn_cpu_util_ave", bt_string, "nkn_cpu_util_ave"},
        {sc "alarm/nkn_cpu_util_ave/enable",           bt_bool,   "true"},
        {sc "alarm/nkn_cpu_util_ave/event_name_root",  bt_string, "nkn_cpu_util_ave"},
        {sc "alarm/nkn_cpu_util_ave/trigger/type",     bt_string, "chd"},
        {sc "alarm/nkn_cpu_util_ave/trigger/id",       bt_string, "cpu_util_ave"},
        {sc "alarm/nkn_cpu_util_ave/trigger/node_pattern",   bt_name,    "/system/cpu/all/busy_pct"},
        {sc "alarm/nkn_cpu_util_ave/rising/enable",          bt_bool,    "true"},
        {sc "alarm/nkn_cpu_util_ave/rising/error_threshold", bt_uint32,  "90"},
        {sc "alarm/nkn_cpu_util_ave/rising/clear_threshold", bt_uint32,  "70"},
        {sc "alarm/nkn_cpu_util_ave/rising/event_on_clear",  bt_bool,    "true"},
        {sc "alarm/nkn_cpu_util_ave/falling/enable",         bt_bool,    "false"},
        {sc "alarm/nkn_cpu_util_ave/falling/error_threshold", bt_uint32, "0"},
        {sc "alarm/nkn_cpu_util_ave/falling/clear_threshold", bt_uint32, "0"},
        {sc "alarm/nkn_cpu_util_ave/falling/event_on_clear",  bt_bool,   "true"},
        {sc "alarm/nkn_cpu_util_ave/clear_if_missing",       bt_bool,    "false"},
        {sc "alarm/nkn_cpu_util_ave/disable_allowed",       bt_bool,    "true"},
        {sc "alarm/nkn_cpu_util_ave/ignore_first_value",    bt_bool,    "true"},
        {sc "alarm/nkn_cpu_util_ave/long/rate_limit_max",   bt_uint32,  "50"},
        {sc "alarm/nkn_cpu_util_ave/long/rate_limit_win",   bt_duration_sec,  "604800"},
        {sc "alarm/nkn_cpu_util_ave/medium/rate_limit_max",   bt_uint32,  "20"},
        {sc "alarm/nkn_cpu_util_ave/medium/rate_limit_win",   bt_duration_sec,  "604800"},
        {sc "alarm/nkn_cpu_util_ave/short/rate_limit_max",   bt_uint32,  "5"},
        {sc "alarm/nkn_cpu_util_ave/short/rate_limit_win",   bt_duration_sec,  "3600"},
};

const bn_str_value md_nkn_proc_mem_inital_values[] = {
    {sc "sample/proc_mem",                  bt_string,      "proc_mem"},
    {sc "sample/proc_mem/enable",           bt_bool,        "false"},
    {sc "sample/proc_mem/delta_constraint", bt_string,      "none"},
    {sc "sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/*\\/data",
        bt_name, "/stat/nkn/vmem/*/data"},
    {sc "sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/*\\/peak",
        bt_name, "/stat/nkn/vmem/*/peak"},
    {sc "sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/*\\/resident",
        bt_name, "/stat/nkn/vmem/*/resident"},
    {sc "sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/*\\/size",
        bt_name, "/stat/nkn/vmem/*/size"},
    {sc "sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/*\\/lock",
        bt_name, "/stat/nkn/vmem/*/lock"},
    {sc "sample/proc_mem/interval",         bt_duration,    "30"},
    {sc "sample/proc_mem/num_to_keep",      bt_uint32,      "120"},
    {sc "sample/proc_mem/sample_method",    bt_string,      "direct"},
    {sc "sample/proc_mem/sync_interval",    bt_uint32,      "5"},
};


const bn_str_value md_nkn_cache_bw_day_inital_values[] = {
    {sc "sample/cache_byte_count_day",      bt_string,      "cache_byte_count_day"},
    {sc "sample/cache_byte_count_day/enable",           bt_bool,        "true"},
    {sc "sample/cache_byte_count_day/delta_constraint", bt_string,      "increasing"},
    {sc "sample/cache_byte_count_day/node/\\/stat\\/nkn\\/nvsd\\/cache_byte_count",
        bt_name, "/stat/nkn/nvsd/cache_byte_count"},
    {sc "sample/cache_byte_count_day/interval",         bt_duration,    "300"},
    {sc "sample/cache_byte_count_day/num_to_keep",      bt_uint32,      "288"},
    {sc "sample/cache_byte_count_day/sample_method",    bt_string,      "delta"},
    {sc "sample/cache_byte_count_day/sync_interval",    bt_uint32,      "10"},
    {sc "chd/cache_bandwidth_day",	       bt_string,    "cache_bandwidth_day"},
    {sc "chd/cache_bandwidth_day/enable",       bt_bool,        "true"},
    {sc "chd/cache_bandwidth_day/source/type",  bt_string,      "sample"},
    {sc "chd/cache_bandwidth_day/source/id",    bt_string,      "cache_byte_count_day"},
    {sc "chd/cache_bandwidth_day/num_to_keep",  bt_uint32,      "288"},
    {sc "chd/cache_bandwidth_day/select_type",  bt_string,      "instances"},
    {sc "chd/cache_bandwidth_day/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/cache_bandwidth_day/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/cache_bandwidth_day/alarm_partial", bt_bool, "false"},
    {sc "chd/cache_bandwidth_day/calc_partial", bt_bool, "false"},
    {sc "chd/cache_bandwidth_day/function", bt_string, "cache_byte_rate_pct"},
    {sc "chd/cache_bandwidth_day/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/cache_bandwidth_day/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/cache_bandwidth_day/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/cache_bandwidth_day/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_cache_bw_week_inital_values[] = {
    {sc "sample/cache_byte_count_week",      bt_string,      "cache_byte_count_week"},
    {sc "sample/cache_byte_count_week/enable",           bt_bool,        "true"},
    {sc "sample/cache_byte_count_week/delta_constraint", bt_string,      "increasing"},
    {sc "sample/cache_byte_count_week/node/\\/stat\\/nkn\\/nvsd\\/cache_byte_count",
        bt_name, "/stat/nkn/nvsd/cache_byte_count"},
    {sc "sample/cache_byte_count_week/interval",         bt_duration,    "1800"},
    {sc "sample/cache_byte_count_week/num_to_keep",      bt_uint32,      "336"},
    {sc "sample/cache_byte_count_week/sample_method",    bt_string,      "delta"},
    {sc "sample/cache_byte_count_week/sync_interval",    bt_uint32,      "10"},
    {sc "chd/cache_bandwidth_week",	       bt_string,    "cache_bandwidth_week"},
    {sc "chd/cache_bandwidth_week/enable",       bt_bool,        "true"},
    {sc "chd/cache_bandwidth_week/source/type",  bt_string,      "sample"},
    {sc "chd/cache_bandwidth_week/source/id",    bt_string,      "cache_byte_count_week"},
    {sc "chd/cache_bandwidth_week/num_to_keep",  bt_uint32,      "336"},
    {sc "chd/cache_bandwidth_week/select_type",  bt_string,      "instances"},
    {sc "chd/cache_bandwidth_week/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/cache_bandwidth_week/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/cache_bandwidth_week/alarm_partial", bt_bool, "false"},
    {sc "chd/cache_bandwidth_week/calc_partial", bt_bool, "false"},
    {sc "chd/cache_bandwidth_week/function", bt_string, "cache_byte_rate_pct"},
    {sc "chd/cache_bandwidth_week/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/cache_bandwidth_week/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/cache_bandwidth_week/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/cache_bandwidth_week/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_disk_bw_day_inital_values[] = {
    {sc "sample/disk_byte_count_day",      bt_string,      "disk_byte_count_day"},
    {sc "sample/disk_byte_count_day/enable",           bt_bool,        "true"},
    {sc "sample/disk_byte_count_day/delta_constraint", bt_string,      "increasing"},
    {sc "sample/disk_byte_count_day/node/\\/stat\\/nkn\\/nvsd\\/disk_byte_count",
        bt_name, "/stat/nkn/nvsd/disk_byte_count"},
    {sc "sample/disk_byte_count_day/interval",         bt_duration,    "300"},
    {sc "sample/disk_byte_count_day/num_to_keep",      bt_uint32,      "288"},
    {sc "sample/disk_byte_count_day/sample_method",    bt_string,      "delta"},
    {sc "sample/disk_byte_count_day/sync_interval",    bt_uint32,      "10"},
    {sc "chd/disk_bandwidth_day",	       bt_string,    "disk_bandwidth_day"},
    {sc "chd/disk_bandwidth_day/enable",       bt_bool,        "true"},
    {sc "chd/disk_bandwidth_day/source/type",  bt_string,      "sample"},
    {sc "chd/disk_bandwidth_day/source/id",    bt_string,      "disk_byte_count_day"},
    {sc "chd/disk_bandwidth_day/num_to_keep",  bt_uint32,      "288"},
    {sc "chd/disk_bandwidth_day/select_type",  bt_string,      "instances"},
    {sc "chd/disk_bandwidth_day/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/disk_bandwidth_day/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/disk_bandwidth_day/alarm_partial", bt_bool, "false"},
    {sc "chd/disk_bandwidth_day/calc_partial", bt_bool, "false"},
    {sc "chd/disk_bandwidth_day/function", bt_string, "disk_byte_rate_pct"},
    {sc "chd/disk_bandwidth_day/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/disk_bandwidth_day/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/disk_bandwidth_day/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/disk_bandwidth_day/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_disk_bw_week_inital_values[] = {
    {sc "sample/disk_byte_count_week",      bt_string,      "disk_byte_count_week"},
    {sc "sample/disk_byte_count_week/enable",           bt_bool,        "true"},
    {sc "sample/disk_byte_count_week/delta_constraint", bt_string,      "increasing"},
    {sc "sample/disk_byte_count_week/node/\\/stat\\/nkn\\/nvsd\\/disk_byte_count",
        bt_name, "/stat/nkn/nvsd/disk_byte_count"},
    {sc "sample/disk_byte_count_week/interval",         bt_duration,    "1800"},
    {sc "sample/disk_byte_count_week/num_to_keep",      bt_uint32,      "336"},
    {sc "sample/disk_byte_count_week/sample_method",    bt_string,      "delta"},
    {sc "sample/disk_byte_count_week/sync_interval",    bt_uint32,      "10"},
    {sc "chd/disk_bandwidth_week",	       bt_string,    "disk_bandwidth_week"},
    {sc "chd/disk_bandwidth_week/enable",       bt_bool,        "true"},
    {sc "chd/disk_bandwidth_week/source/type",  bt_string,      "sample"},
    {sc "chd/disk_bandwidth_week/source/id",    bt_string,      "disk_byte_count_week"},
    {sc "chd/disk_bandwidth_week/num_to_keep",  bt_uint32,      "336"},
    {sc "chd/disk_bandwidth_week/select_type",  bt_string,      "instances"},
    {sc "chd/disk_bandwidth_week/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/disk_bandwidth_week/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/disk_bandwidth_week/alarm_partial", bt_bool, "false"},
    {sc "chd/disk_bandwidth_week/calc_partial", bt_bool, "false"},
    {sc "chd/disk_bandwidth_week/function", bt_string, "disk_byte_rate_pct"},
    {sc "chd/disk_bandwidth_week/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/disk_bandwidth_week/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/disk_bandwidth_week/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/disk_bandwidth_week/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_origin_bw_day_inital_values[] = {
    {sc "sample/origin_byte_count_day",      bt_string,      "origin_byte_count_day"},
    {sc "sample/origin_byte_count_day/enable",           bt_bool,        "true"},
    {sc "sample/origin_byte_count_day/delta_constraint", bt_string,      "increasing"},
    {sc "sample/origin_byte_count_day/node/\\/stat\\/nkn\\/nvsd\\/origin_byte_count",
        bt_name, "/stat/nkn/nvsd/origin_byte_count"},
    {sc "sample/origin_byte_count_day/interval",         bt_duration,    "300"},
    {sc "sample/origin_byte_count_day/num_to_keep",      bt_uint32,      "288"},
    {sc "sample/origin_byte_count_day/sample_method",    bt_string,      "delta"},
    {sc "sample/origin_byte_count_day/sync_interval",    bt_uint32,      "10"},
    {sc "chd/origin_bandwidth_day",	       bt_string,    "origin_bandwidth_day"},
    {sc "chd/origin_bandwidth_day/enable",       bt_bool,        "true"},
    {sc "chd/origin_bandwidth_day/source/type",  bt_string,      "sample"},
    {sc "chd/origin_bandwidth_day/source/id",    bt_string,      "origin_byte_count_day"},
    {sc "chd/origin_bandwidth_day/num_to_keep",  bt_uint32,      "288"},
    {sc "chd/origin_bandwidth_day/select_type",  bt_string,      "instances"},
    {sc "chd/origin_bandwidth_day/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/origin_bandwidth_day/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/origin_bandwidth_day/alarm_partial", bt_bool, "false"},
    {sc "chd/origin_bandwidth_day/calc_partial", bt_bool, "false"},
    {sc "chd/origin_bandwidth_day/function", bt_string, "origin_byte_rate_pct"},
    {sc "chd/origin_bandwidth_day/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/origin_bandwidth_day/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/origin_bandwidth_day/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/origin_bandwidth_day/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_origin_bw_week_inital_values[] = {
    {sc "sample/origin_byte_count_week",      bt_string,      "origin_byte_count_week"},
    {sc "sample/origin_byte_count_week/enable",           bt_bool,        "true"},
    {sc "sample/origin_byte_count_week/delta_constraint", bt_string,      "increasing"},
    {sc "sample/origin_byte_count_week/node/\\/stat\\/nkn\\/nvsd\\/origin_byte_count",
        bt_name, "/stat/nkn/nvsd/origin_byte_count"},
    {sc "sample/origin_byte_count_week/interval",         bt_duration,    "1800"},
    {sc "sample/origin_byte_count_week/num_to_keep",      bt_uint32,      "336"},
    {sc "sample/origin_byte_count_week/sample_method",    bt_string,      "delta"},
    {sc "sample/origin_byte_count_week/sync_interval",    bt_uint32,      "10"},
    {sc "chd/origin_bandwidth_week",	       bt_string,    "origin_bandwidth_week"},
    {sc "chd/origin_bandwidth_week/enable",       bt_bool,        "true"},
    {sc "chd/origin_bandwidth_week/source/type",  bt_string,      "sample"},
    {sc "chd/origin_bandwidth_week/source/id",    bt_string,      "origin_byte_count_week"},
    {sc "chd/origin_bandwidth_week/num_to_keep",  bt_uint32,      "336"},
    {sc "chd/origin_bandwidth_week/select_type",  bt_string,      "instances"},
    {sc "chd/origin_bandwidth_week/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/origin_bandwidth_week/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/origin_bandwidth_week/alarm_partial", bt_bool, "false"},
    {sc "chd/origin_bandwidth_week/calc_partial", bt_bool, "false"},
    {sc "chd/origin_bandwidth_week/function", bt_string, "origin_byte_rate_pct"},
    {sc "chd/origin_bandwidth_week/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/origin_bandwidth_week/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/origin_bandwidth_week/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/origin_bandwidth_week/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_total_bw_day_inital_values[] = {
    {sc "sample/total_bytes_day",      bt_string,      "total_bytes_day"},
    {sc "sample/total_bytes_day/enable",           bt_bool,        "true"},
    {sc "sample/total_bytes_day/delta_constraint", bt_string,      "increasing"},
    {sc "sample/total_bytes_day/node/\\/stat\\/nkn\\/nvsd\\/total_byte_count",
        bt_name, "/stat/nkn/nvsd/total_byte_count"},
    {sc "sample/total_bytes_day/interval",         bt_duration,    "300"},
    {sc "sample/total_bytes_day/num_to_keep",      bt_uint32,      "288"},
    {sc "sample/total_bytes_day/sample_method",    bt_string,      "delta"},
    {sc "sample/total_bytes_day/sync_interval",    bt_uint32,      "10"},
    {sc "chd/tot_bandwidth_day",	       bt_string,    "tot_bandwidth_day"},
    {sc "chd/tot_bandwidth_day/enable",       bt_bool,        "true"},
    {sc "chd/tot_bandwidth_day/source/type",  bt_string,      "sample"},
    {sc "chd/tot_bandwidth_day/source/id",    bt_string,      "total_bytes_day"},
    {sc "chd/tot_bandwidth_day/num_to_keep",  bt_uint32,      "288"},
    {sc "chd/tot_bandwidth_day/select_type",  bt_string,      "instances"},
    {sc "chd/tot_bandwidth_day/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/tot_bandwidth_day/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/tot_bandwidth_day/alarm_partial", bt_bool, "false"},
    {sc "chd/tot_bandwidth_day/calc_partial", bt_bool, "false"},
    {sc "chd/tot_bandwidth_day/function", bt_string, "total_byte_rate_pct"},
    {sc "chd/tot_bandwidth_day/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/tot_bandwidth_day/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/tot_bandwidth_day/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/tot_bandwidth_day/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_total_bw_week_inital_values[] = {
    {sc "sample/total_bytes_week",      bt_string,      "total_bytes_week"},
    {sc "sample/total_bytes_week/enable",           bt_bool,        "true"},
    {sc "sample/total_bytes_week/delta_constraint", bt_string,      "increasing"},
    {sc "sample/total_bytes_week/node/\\/stat\\/nkn\\/nvsd\\/tot_byte_count",
        bt_name, "/stat/nkn/nvsd/tot_byte_count"},
    {sc "sample/total_bytes_week/interval",         bt_duration,    "1800"},
    {sc "sample/total_bytes_week/num_to_keep",      bt_uint32,      "336"},
    {sc "sample/total_bytes_week/sample_method",    bt_string,      "delta"},
    {sc "sample/total_bytes_week/sync_interval",    bt_uint32,      "10"},
    {sc "chd/tot_bandwidth_week",	       bt_string,    "tot_bandwidth_week"},
    {sc "chd/tot_bandwidth_week/enable",       bt_bool,        "true"},
    {sc "chd/tot_bandwidth_week/source/type",  bt_string,      "sample"},
    {sc "chd/tot_bandwidth_week/source/id",    bt_string,      "total_bytes_week"},
    {sc "chd/tot_bandwidth_week/num_to_keep",  bt_uint32,      "336"},
    {sc "chd/tot_bandwidth_week/select_type",  bt_string,      "instances"},
    {sc "chd/tot_bandwidth_week/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/tot_bandwidth_week/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/tot_bandwidth_week/alarm_partial", bt_bool, "false"},
    {sc "chd/tot_bandwidth_week/calc_partial", bt_bool, "false"},
    {sc "chd/tot_bandwidth_week/function", bt_string, "total_byte_rate_pct"},
    {sc "chd/tot_bandwidth_week/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/tot_bandwidth_week/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/tot_bandwidth_week/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/tot_bandwidth_week/sync_interval", bt_uint32, "10"},
};


const bn_str_value md_nkn_intf_day_inital_values[] = {
    {sc "sample/intf_day",      bt_string,      "intf_day"},
    {sc "sample/intf_day/enable",           bt_bool,        "true"},
    {sc "sample/intf_day/delta_constraint", bt_string,      "increasing"},
    {sc "sample/intf_day/node/\\/net\\/interface\\/state\\/*\\/stats\\/*\\/bytes",
        bt_name, "/net/interface/state/*/stats/*/bytes"},
    {sc "sample/intf_day/interval",         bt_duration,    "300"},
    {sc "sample/intf_day/num_to_keep",      bt_uint32,      "288"},
    {sc "sample/intf_day/sample_method",    bt_string,      "delta"},
    {sc "sample/intf_day/sync_interval",    bt_uint32,      "10"},
};

const bn_str_value md_nkn_intf_week_inital_values[] = {
    {sc "sample/intf_week",      bt_string,      "intf_week"},
    {sc "sample/intf_week/enable",           bt_bool,        "true"},
    {sc "sample/intf_week/delta_constraint", bt_string,      "increasing"},
    {sc "sample/intf_week/node/\\/net\\/interface\\/state\\/*\\/stats\\/*\\/bytes",
        bt_name, "/net/interface/state/*/stats/*/bytes"},
    {sc "sample/intf_week/interval",         bt_duration,    "1800"},
    {sc "sample/intf_week/num_to_keep",      bt_uint32,      "336"},
    {sc "sample/intf_week/sample_method",    bt_string,      "delta"},
    {sc "sample/intf_week/sync_interval",    bt_uint32,      "10"},
};

const bn_str_value md_nkn_intf_month_inital_values[] = {
    {sc "sample/intf_month",      bt_string,      "intf_month"},
    {sc "sample/intf_month/enable",           bt_bool,        "true"},
    {sc "sample/intf_month/delta_constraint", bt_string,      "increasing"},
    {sc "sample/intf_month/node/\\/net\\/interface\\/state\\/*\\/stats\\/*\\/bytes",
        bt_name, "/net/interface/state/*/stats/*/bytes"},
    {sc "sample/intf_month/interval",         bt_duration,    "14400"},
    {sc "sample/intf_month/num_to_keep",      bt_uint32,      "168"},
    {sc "sample/intf_month/sample_method",    bt_string,      "delta"},
    {sc "sample/intf_month/sync_interval",    bt_uint32,      "10"},
    {sc "chd/intf_month",	       bt_string,    "intf_month"},
    {sc "chd/intf_month/enable",       bt_bool,        "true"},
    {sc "chd/intf_month/source/type",  bt_string,      "sample"},
    {sc "chd/intf_month/source/id",    bt_string,      "intf_month"},
    {sc "chd/intf_month/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/intf_month/select_type",  bt_string,      "instances"},
    {sc "chd/intf_month/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/intf_month/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/intf_month/alarm_partial", bt_bool, "false"},
    {sc "chd/intf_month/calc_partial", bt_bool, "false"},
    {sc "chd/intf_month/function", bt_string, "total_byte_rate_pct"},
    {sc "chd/intf_month/time/interval_distance", bt_duration_sec, "0"},
    {sc "chd/intf_month/time/interval_length", bt_duration_sec, "0"},
    {sc "chd/intf_month/time/interval_phase", bt_duration_sec, "0"},
    {sc "chd/intf_month/sync_interval", bt_uint32, "10"},
};

const bn_str_value md_nkn_bw_month_avg_inital_values[] = {
    {sc "chd/bandwidth_month_avg",	       bt_string,    "bandwidth_month_avg"},
    {sc "chd/bandwidth_month_avg/enable",       bt_bool,        "true"},
    {sc "chd/bandwidth_month_avg/source/type",  bt_string,      "chd"},
    {sc "chd/bandwidth_month_avg/source/id",    bt_string,      "bandwidth_day_avg"},
    {sc "chd/bandwidth_month_avg/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/bandwidth_month_avg/select_type",  bt_string,      "time"},
    {sc "chd/bandwidth_month_avg/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/bandwidth_month_avg/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/bandwidth_month_avg/alarm_partial", bt_bool, "false"},
    {sc "chd/bandwidth_month_avg/calc_partial", bt_bool, "false"},
    {sc "chd/bandwidth_month_avg/function", bt_string, "mean"},
    {sc "chd/bandwidth_month_avg/time/interval_distance", bt_duration_sec, "14400"},
    {sc "chd/bandwidth_month_avg/time/interval_length", bt_duration_sec, "14400"},
    {sc "chd/bandwidth_month_avg/time/interval_phase", bt_duration_sec, "14400"},
    {sc "chd/bandwidth_month_avg/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_bw_month_peak_inital_values[] = {
    {sc "chd/bandwidth_month_peak",	       bt_string,    "bandwidth_month_peak"},
    {sc "chd/bandwidth_month_peak/enable",       bt_bool,        "true"},
    {sc "chd/bandwidth_month_peak/source/type",  bt_string,      "chd"},
    {sc "chd/bandwidth_month_peak/source/id",    bt_string,      "bandwidth_day_peak"},
    {sc "chd/bandwidth_month_peak/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/bandwidth_month_peak/select_type",  bt_string,      "time"},
    {sc "chd/bandwidth_month_peak/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/bandwidth_month_peak/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/bandwidth_month_peak/alarm_partial", bt_bool, "false"},
    {sc "chd/bandwidth_month_peak/calc_partial", bt_bool, "false"},
    {sc "chd/bandwidth_month_peak/function", bt_string, "max"},
    {sc "chd/bandwidth_month_peak/time/interval_distance", bt_duration_sec, "14400"},
    {sc "chd/bandwidth_month_peak/time/interval_length", bt_duration_sec, "14400"},
    {sc "chd/bandwidth_month_peak/time/interval_phase", bt_duration_sec, "14400"},
    {sc "chd/bandwidth_month_peak/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_bw_week_avg_inital_values[] = {
    {sc "chd/bandwidth_week_avg",	       bt_string,    "bandwidth_week_avg"},
    {sc "chd/bandwidth_week_avg/enable",       bt_bool,        "true"},
    {sc "chd/bandwidth_week_avg/source/type",  bt_string,      "chd"},
    {sc "chd/bandwidth_week_avg/source/id",    bt_string,      "bandwidth_day_avg"},
    {sc "chd/bandwidth_week_avg/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/bandwidth_week_avg/select_type",  bt_string,      "time"},
    {sc "chd/bandwidth_week_avg/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/bandwidth_week_avg/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/bandwidth_week_avg/alarm_partial", bt_bool, "false"},
    {sc "chd/bandwidth_week_avg/calc_partial", bt_bool, "false"},
    {sc "chd/bandwidth_week_avg/function", bt_string, "mean"},
    {sc "chd/bandwidth_week_avg/time/interval_distance", bt_duration_sec, "3600"},
    {sc "chd/bandwidth_week_avg/time/interval_length", bt_duration_sec, "3600"},
    {sc "chd/bandwidth_week_avg/time/interval_phase", bt_duration_sec, "3600"},
    {sc "chd/bandwidth_week_avg/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_bw_week_peak_inital_values[] = {
    {sc "chd/bandwidth_week_peak",	       bt_string,    "bandwidth_week_peak"},
    {sc "chd/bandwidth_week_peak/enable",       bt_bool,        "true"},
    {sc "chd/bandwidth_week_peak/source/type",  bt_string,      "chd"},
    {sc "chd/bandwidth_week_peak/source/id",    bt_string,      "bandwidth_day_peak"},
    {sc "chd/bandwidth_week_peak/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/bandwidth_week_peak/select_type",  bt_string,      "time"},
    {sc "chd/bandwidth_week_peak/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/bandwidth_week_peak/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/bandwidth_week_peak/alarm_partial", bt_bool, "false"},
    {sc "chd/bandwidth_week_peak/calc_partial", bt_bool, "false"},
    {sc "chd/bandwidth_week_peak/function", bt_string, "max"},
    {sc "chd/bandwidth_week_peak/time/interval_distance", bt_duration_sec, "3600"},
    {sc "chd/bandwidth_week_peak/time/interval_length", bt_duration_sec, "3600"},
    {sc "chd/bandwidth_week_peak/time/interval_phase", bt_duration_sec, "3600"},
    {sc "chd/bandwidth_week_peak/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_connection_month_avg_inital_values[] = {
    {sc "chd/connection_month_avg",	       bt_string,    "connection_month_avg"},
    {sc "chd/connection_month_avg/enable",       bt_bool,        "true"},
    {sc "chd/connection_month_avg/source/type",  bt_string,      "chd"},
    {sc "chd/connection_month_avg/source/id",    bt_string,      "connection_day_avg"},
    {sc "chd/connection_month_avg/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/connection_month_avg/select_type",  bt_string,      "time"},
    {sc "chd/connection_month_avg/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/connection_month_avg/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/connection_month_avg/alarm_partial", bt_bool, "false"},
    {sc "chd/connection_month_avg/calc_partial", bt_bool, "false"},
    {sc "chd/connection_month_avg/function", bt_string, "mean"},
    {sc "chd/connection_month_avg/time/interval_distance", bt_duration_sec, "14400"},
    {sc "chd/connection_month_avg/time/interval_length", bt_duration_sec, "14400"},
    {sc "chd/connection_month_avg/time/interval_phase", bt_duration_sec, "14400"},
    {sc "chd/connection_month_avg/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_connection_month_peak_inital_values[] = {
    {sc "chd/connection_month_peak",	       bt_string,    "connection_month_peak"},
    {sc "chd/connection_month_peak/enable",       bt_bool,        "true"},
    {sc "chd/connection_month_peak/source/type",  bt_string,      "chd"},
    {sc "chd/connection_month_peak/source/id",    bt_string,      "connection_day_peak"},
    {sc "chd/connection_month_peak/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/connection_month_peak/select_type",  bt_string,      "time"},
    {sc "chd/connection_month_peak/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/connection_month_peak/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/connection_month_peak/alarm_partial", bt_bool, "false"},
    {sc "chd/connection_month_peak/calc_partial", bt_bool, "false"},
    {sc "chd/connection_month_peak/function", bt_string, "max"},
    {sc "chd/connection_month_peak/time/interval_distance", bt_duration_sec, "14400"},
    {sc "chd/connection_month_peak/time/interval_length", bt_duration_sec, "14400"},
    {sc "chd/connection_month_peak/time/interval_phase", bt_duration_sec, "14400"},
    {sc "chd/connection_month_peak/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_connection_week_avg_inital_values[] = {
    {sc "chd/connection_week_avg",	       bt_string,    "connection_week_avg"},
    {sc "chd/connection_week_avg/enable",       bt_bool,        "true"},
    {sc "chd/connection_week_avg/source/type",  bt_string,      "chd"},
    {sc "chd/connection_week_avg/source/id",    bt_string,      "connection_day_avg"},
    {sc "chd/connection_week_avg/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/connection_week_avg/select_type",  bt_string,      "time"},
    {sc "chd/connection_week_avg/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/connection_week_avg/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/connection_week_avg/alarm_partial", bt_bool, "false"},
    {sc "chd/connection_week_avg/calc_partial", bt_bool, "false"},
    {sc "chd/connection_week_avg/function", bt_string, "mean"},
    {sc "chd/connection_week_avg/time/interval_distance", bt_duration_sec, "3600"},
    {sc "chd/connection_week_avg/time/interval_length", bt_duration_sec, "3600"},
    {sc "chd/connection_week_avg/time/interval_phase", bt_duration_sec, "3600"},
    {sc "chd/connection_week_avg/sync_interval", bt_uint32, "1"},
};

const bn_str_value md_nkn_connection_week_peak_inital_values[] = {
    {sc "chd/connection_week_peak",	       bt_string,    "connection_week_peak"},
    {sc "chd/connection_week_peak/enable",       bt_bool,        "true"},
    {sc "chd/connection_week_peak/source/type",  bt_string,      "chd"},
    {sc "chd/connection_week_peak/source/id",    bt_string,      "connection_day_peak"},
    {sc "chd/connection_week_peak/num_to_keep",  bt_uint32,      "168"},
    {sc "chd/connection_week_peak/select_type",  bt_string,      "time"},
    {sc "chd/connection_week_peak/instances/num_to_use",  bt_uint32,      "1"},
    {sc "chd/connection_week_peak/instances/num_to_advance",  bt_uint32,  "1"},
    {sc "chd/connection_week_peak/alarm_partial", bt_bool, "false"},
    {sc "chd/connection_week_peak/calc_partial", bt_bool, "false"},
    {sc "chd/connection_week_peak/function", bt_string, "max"},
    {sc "chd/connection_week_peak/time/interval_distance", bt_duration_sec, "3600"},
    {sc "chd/connection_week_peak/time/interval_length", bt_duration_sec, "3600"},
    {sc "chd/connection_week_peak/time/interval_phase", bt_duration_sec, "3600"},
    {sc "chd/connection_week_peak/sync_interval", bt_uint32, "1"},
};

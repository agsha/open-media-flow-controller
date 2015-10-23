/*
 *
 * Filename:  $Source: cli_stats_cmds.inc.c $
 * Date:      $Date: 2009/01/28 $
 * Author:    $Author:  $
 *
 */

/* 
 * This file is include by src/bin/cli/modules/cli_stats_cmds.c.
 * The different blocks are chosen by the caller #define'ing
 * CLI_STATS_CMDS_INC_GRAFT_POINT to the numbers seen below.
 */

/* Graft point 1: additional stats object mappings */
#if CLI_STATS_CMDS_INC_GRAFT_POINT == 1

#if defined(PROD_FEATURE_I18N_SUPPORT)
    /* Must match GETTEXT_PACKAGE as defined in our CLI modules Makefile */
    #define CUSTOMER_CLIMOD_GETTEXT_DOMAIN "cli_generic_mods"

    #define stats_cmd_desc(type,id,desc,units,node_desc) \
    { type,id,N_(desc),N_(units),N_(node_desc), \
      CUSTOMER_CLIMOD_GETTEXT_DOMAIN \
    },

    
#else
    #define stats_cmd_desc(type,id,desc,units,node_desc) \
    { type,id,N_(desc),N_(units),N_(node_desc), \
    },
#endif

#define sample_desc(id,desc,units,node_desc) stats_cmd_desc(csot_sample,id,desc,units,node_desc)
#define chd_desc(id,desc,units,node_desc) stats_cmd_desc(csot_chd,id,desc,units,node_desc)
#define alarm_desc(id,desc,units,node_desc) stats_cmd_desc(csot_alarm,id,desc,units,node_desc)

    sample_desc("cache_byte_count","Data served from Cache","bytes",NULL)
    sample_desc("disk_byte_count","Data Served from Disk","bytes",NULL)
    sample_desc("http_transaction_count","HTTP Transaction Count","Count",NULL)
    sample_desc("num_of_connections","Active Connections Count","Count",NULL)
    sample_desc("origin_byte_count","Data Served from Origin","bytes",NULL)
    sample_desc("total_bytes","Total Data served","bytes",NULL)
    sample_desc("total_cache_byte_count","Total Data served from Cache","bytes",NULL)
    sample_desc("total_disk_byte_count","Total Data served from Disk","bytes",NULL)
    sample_desc("total_origin_byte_count","Total Data served from Origin","bytes",NULL)
    sample_desc("mem_util", "Memory Utilization", "Percent", NULL)
    sample_desc("sas_disk_space", "Free Space on SAS Disks", "Bytes", NULL)
    sample_desc("sata_disk_space", "Free Space on SATA Disks", "Bytes", NULL)
    sample_desc("ssd_disk_space", "Free Space on SSD Disks", "Bytes", NULL)
    sample_desc("root_disk_space", "Free Space on Root Disk", "Bytes", NULL)
    sample_desc("sas_disk_read", "Bytes read from SAS Disks", "Bytes", NULL)
    sample_desc("sata_disk_read", "Bytes read from SATA Disks", "Bytes", NULL)
    sample_desc("ssd_disk_read", "Bytes read from SSD Disks", "Bytes", NULL)
    sample_desc("root_disk_read", "Bytes read from Root Disk", "Bytes", NULL)
    sample_desc("sas_disk_io", "Bytes read from and written to SAS Disks", "Bytes", NULL)
    sample_desc("sata_disk_io", "Bytes read from and written to SATA Disks", "Bytes", NULL)
    sample_desc("ssd_disk_io", "Bytes read from and written to SSD Disks", "Bytes", NULL)
    sample_desc("root_disk_io", "Bytes read from and written to Root Disk Partitions", "Bytes", NULL)
    sample_desc("appl_cpu_util", "Application CPU Utilization", "Percent", NULL)
    sample_desc("cache_hit", "Bytes sent to client and bytes from origin", "Bytes", NULL)
    
    chd_desc("cache_byte_rate","Current Cache Bandwidth","bytes per sec",NULL)
    chd_desc("connection_rate","Connection Rate","Per Sec",NULL)
    chd_desc("disk_byte_rate","Current Disk Bandwidth","Per Sec",NULL)
    chd_desc("http_transaction_rate","HTTP Transaction Rate","Per Sec",NULL)
    chd_desc("origin_byte_rate","Current Origin Bandwidth","bytes per sec",NULL)
    chd_desc("total_byte_rate","Bandwidth","bytes per sec",NULL)
    chd_desc("mon_total_byte_rate", "Monitor Bandwidth", "Bytes/Sec", NULL)
    chd_desc("mon_connection_rate", "Monitor Connection Rate", "Per Sec", NULL)
    chd_desc("mon_origin_byte_rate", "Monitor Origin Bandwidth", "Bytes/Sec", NULL)
    chd_desc("mon_disk_byte_rate", "Monitor Disk Bandwidth", "Bytes/Sec", NULL)
    chd_desc("mon_http_transaction_rate", "Monitor HTTP Transaction Rate", "Per Sec", NULL)
    chd_desc("mon_intf_util", "Monitor Net Utilization", "Per Sec", NULL)
    chd_desc("mon_origin_byte_rate", "Monitor  Origin Bandwidth", "Bytes/Sec", NULL)
    chd_desc("mon_paging", "Monitor Paging", "Page Faults", NULL)
    chd_desc("mem_util_ave", "Average Memory Utilization", "Percent", NULL)
    
    chd_desc("sas_disk_space", "Monitor free space on SAS Disks", "Bytes", NULL)
    chd_desc("sata_disk_space", "Monitor free space on SATA Disks", "Bytes", NULL)
    chd_desc("ssd_disk_space", "Monitor free space on SSD Disks", "Bytes", NULL)
    chd_desc("root_disk_space", "Monitor free space on Root Disk Partitions", "Bytes", NULL)
    
    chd_desc("sas_disk_bw", "Disk Bandwidth of SAS Disks", "Bytes/sec", NULL)
    chd_desc("sata_disk_bw", "Disk Bandwidth of SATA Disks", "Bytes/sec", NULL)
    chd_desc("ssd_disk_bw", "Disk Bandwidth of SSD Disks", "Bytes/sec", NULL)
    chd_desc("root_disk_bw", "Disk Bandwidth of Root Disk Partitions", "Bytes/sec", NULL)
   
    chd_desc("mon_sas_disk_bw", "Monitor Disk Bandwidth of SAS Disks", "Bytes/sec", NULL)
    chd_desc("mon_sata_disk_bw", "Monitor Disk Bandwidth of SATA Disks", "Bytes/sec", NULL)
    chd_desc("mon_ssd_disk_bw", "Monitor Disk Bandwidth of SSD Disks", "Bytes/sec", NULL)
    chd_desc("mon_root_disk_bw", "Monitor Disk Bandwidth of Root Disk Partitions", "Bytes/sec", NULL)
    
    chd_desc("sas_disk_io", "Disk I/O of SAS Disks", "Bytes/sec", NULL)
    chd_desc("sata_disk_io", "Disk I/O of SATA Disks", "Bytes/sec", NULL)
    chd_desc("ssd_disk_io", "Disk I/O of SSD Disks", "Bytes/sec", NULL)
    chd_desc("root_disk_io", "Disk I/O of Root Disk Partitions", "Bytes/sec", NULL)
       
    chd_desc("sas_disk_io_ave", "Average Disk I/O of SAS Disks", "Bytes/sec", NULL)
    chd_desc("sata_disk_io_ave", "Average Disk I/O of SATA Disks", "Bytes/sec", NULL)
    chd_desc("ssd_disk_io_ave", "Average Disk I/O of SSD Disks", "Bytes/sec", NULL)
    chd_desc("root_disk_io_ave", "Average Disk I/O of Root Disk Partitions", "Bytes/sec", NULL)
     
    chd_desc("appl_cpu_util", "1 min Average of Application CPU Utilization", "Percent", NULL)
    chd_desc("cache_hit_ratio", "Cache Hit Ratio", "Ratio", NULL)
    chd_desc("cache_hit_ratio_ave", "Average Cache Hit Ratio", "Ratio", NULL)
     
    alarm_desc("cache_byte_rate","Current Cache Bandwidth too high","bytes per sec",NULL)
    alarm_desc("connection_rate","Connection Rate too high","Per Sec",NULL)
    alarm_desc("disk_byte_rate","Current Disk Bandwidth too high","Per Sec",NULL)
    alarm_desc("http_transaction_rate","HTTP Transaction Rate too high","Per Sec",NULL)
    alarm_desc("origin_byte_rate","Current Origin Bandwidth too high","bytes per sec",NULL)
    alarm_desc("aggr_cpu_util", "Average CPU util across all cores is high", "Percent", NULL)
    alarm_desc("nkn_mem_util", "Memory utilization is high", "Percent", NULL)
    alarm_desc("sas_disk_space", "Disk Freespace of a SAS disk is low", "Bytes", N_("Free space on SAS/$5$"))
    alarm_desc("sata_disk_space", "Disk Freespace of a SATA disk is low", "Bytes",  N_("Free space on SATA/$5$"))
    alarm_desc("ssd_disk_space", "Disk Freespace of a SSD disk is low", "Bytes",  N_("Free space on SSD/$5$"))
    alarm_desc("root_disk_space", "Freespace of a Root Disk Partition is low", "Bytes",  N_("Free space on $5$ partition"))
    alarm_desc("sas_disk_bw", "Disk Bandwidth of a SAS disk is high", "Bytes/sec", NULL)
    alarm_desc("sata_disk_bw", "Disk Bandwidth of a SATA disk is high", "Bytes/sec", NULL)
    alarm_desc("ssd_disk_bw", "Disk Bandwidth of a SSD disk is high", "Bytes/sec", NULL)
    alarm_desc("root_disk_bw", "Disk Bandwidth of a Root Disk Partition is high", "Bytes/sec", NULL)
    alarm_desc("sas_disk_io", "Disk I/O rate of a SAS disk is high", "Bytes/sec", NULL)
    alarm_desc("sata_disk_io", "Disk I/O rate of a SATA disk is high", "Bytes/sec", NULL)
    alarm_desc("ssd_disk_io", "Disk I/O rate of a SSD disk is high", "Bytes/sec", NULL)
    alarm_desc("root_disk_io", "Disk I/O rate of a Root Disk Partition is high", "Bytes/sec", NULL)
    alarm_desc("appl_cpu_util", "Application CPU utilization is high", "Percent", NULL)
    alarm_desc("cache_hit_ratio", "Cache Hit Ratio is low", "Ratio", NULL)
    alarm_desc("rp_global_pool_max_bandwidth", "Resource pool bandwidth high", "Percent", NULL)
    alarm_desc("rp_global_pool_client_sessions", "Resource pool client sessions high", "Percent", NULL)
    
#endif /* GRAFT_POINT 1 */

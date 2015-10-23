/*
 *
 * Filename:  gmgmthd.c
 * Date:      2010/03/25
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "bnode.h"
#include "gmgmthd_main.h"
#include <proc_utils.h>
#include <math.h>
#include "nkn_cntr_api.h"
#include "nkn_mgmt_defs.h"
#include "network.h"
#include <ttime.h>
#include "nkn_defs.h"
#include "file_utils.h"
#include "ttime.h"
#include "math.h"
#ifdef PROD_TARGET_OS_FREEBSD
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#else
#include <sys/statvfs.h>
#endif
#include "nkncnt_client.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "nkn_mem_counter_def.h"
#include "libnknmgmt.h"

/*
 * COPIED from  md_system.c
 *  * XXX/SML: This is commonly understood to be 512 for Linux 2.6, and is defined
 *   * that way in many of the kernel header files.  It's unclear whether there's
 *    * an authoratative place for this though.
 *     */
#define MD_SYSTEM_BYTES_PER_SECTOR 512

/*Adding structure to add disk details for SNMP/ monitoring nodes*/
#define MAX_DISK 80
typedef struct mon_disk_data_st 
{
    int index;
    char *name ;//char name[16];
    char serial[64];
    char state[32];
//    char type[8]; //root,cache
    int type ;
    char model[8]; //sad,sata,ssd,any
    uint32 imodel;
    uint32 blocksize;
    uint32 totalsize;
    uint32 usedblock;
    uint32 freespace;
    uint64 capacity;
    uint64 util;
    uint64 iorate;
    uint32 ioerr;
    uint32 ioclr;
    uint32 spaceerr;
    uint32 spaceclr;
    uint64 diskbw;
    uint32 bwerr;
    uint32 bwclr;
    uint64 readb;
    uint64 writeb;
    lt_time_sec snap;

}mon_disk_data_t;


static mon_disk_data_t       *pstDisk = NULL;
mon_disk_data_t lstDisk[MAX_DISK];
#if 0
mon_disk_data_t      lstDisk[] =   {
    {0, "dc_01", "9X333", "running", "cache","sas", 62942, 62942, 300, 62642, 62642, 5000, 40, 100, 80, 3000, 2500, 45,100,80},
    {1, "dc_02", "9X3133", "disable", "cache","sata", 62942, 62942, 300, 62642, 62642, 5000, 40, 100, 80, 3000, 2500, 45,100,80},
    {2, "dc_03", "9X3dd33", "disable", "cache","sata", 62942, 62942, 300, 62642, 62642, 5000, 40, 100, 80, 3000, 2500, 45,100,80},
    {3, "dc_04", "9X33ss3", "disable", "cache","ssd", 62942, 62942, 300, 62642, 62642, 5000, 40, 100, 80, 3000, 2500, 45,100,80},
    {4, "dc_05", "9X333dd", "running", "cache","sas", 62942, 62942, 300, 62642, 62642, 5000, 40, 100, 80, 3000, 2500, 45,100,80},
    {5, "dc_06", "9X333ss", "running", "cache","sas", 62942, 62942, 300, 62642, 62642, 5000, 40, 100, 80, 3000, 2500, 45,100,80},
};
#endif
/*hash table for disk*/
//GHashTable *ht;
char disk_cache_provider_names[21][64] = {
    {   "unknown" },
    {   "ssd"     },
    {   "flash"   },
    {   "buffer"  },
    {   "null"    },
    {   "sas"      },
    {   "sata"     },
    {   "null"     },
    {   "null"     },
    {   "null"     },
    {   "peer"     },
    {   "tfm"     },
    {   "null"     },
    {   "null"     },
    {   "null"     },
    {   "null"     },
    {   "null"     },
    {   "null"     },
    {   "null"     },
    {   "nfs"     },
    {   "origin"     },
};

char disk_state[17][128] = {
    {"-"},
    {"unknown disk state"},
    {"disk no longer found (might not be present)"},
    {"disk cacheable, but cache not enabled"},
    {"disk has wrong format hence not cacheable"},
    {" "},
    {"disk has been deactivated"},
    {"disk has been activated"},
    {"soft disk error, try to clear"},
    {"soft disk error, try to clear"},
    {"cache running"},
    {"unknown state, please try again"},
    {"disk has wrong format hence not cacheable"},
    {"unknown state, please try again"},
    {"conversion of disk cache version failed"},
    {"disk has the wrong cache version"},
    {"disk disable under progress"},
};
#define DISK_CACHE 0
#define DISK_ROOT 1
char disk_type[2][8] = {
    {"cache"},
    {"root"}
};
/*Global context for LFD*/
nkncnt_client_ctx_t g_lfd_mem_ctx  ;

/*Timer context to query disk cache details on nvsd restart*/
typedef struct watch_dir_st {
    int	 index;
    lew_event *poll_event;
    lew_event_handler callback;
    void *context_data;
    lt_dur_ms   poll_frequency;
} watch_dir_t;

static watch_dir_t  g_nvsd_watch;
int   g_rebuild_local_disk_stat = 0;
int
gmgmthd_watch_nvsd_up_context_init(void);
int
gmgmthd_handle_nvsd_up(int fd, short event_type,
	void *data, lew_context *ctxt);
int
gmgmthd_set_nvsd_watch_timer(watch_dir_t *context);

int gmgmthd_status_timer_context_allocate(lt_dur_ms  poll_frequency);
/*
 * A made up number for how many monitoring wildcards to indicate.
 * This is mainly used for performance testing.
 * The five fields per wildcard level are entirely arbitrary.
 */
static mon_disk_data_t *get_disk_element(const char *cpdisk ,const char *type);

const uint32 gmgmthd_num_ext_mon_nodes = 1000;

static int gmgmthd_mon_handle_get_wildcard(const char *bname,
                                      const tstr_array *bname_parts,
                                      uint32 num_parts,
                                      const tstring *last_name_part,
                                      bn_binding_array *resp_bindings,
                                      tbool *ret_done);
static uint64
get_one_mfd_intf_stat (const char* mfd_name, const char* mfd_intf);
static int
gmgmthd_disk_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
	                                bn_binding *binding, void *data);
int gmgmthd_get_blocksize(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_totalsize(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_get_util(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_get_freespace(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_get_usedblock(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_provider(mon_disk_data_t *disk, uint32 *type);
int gmgmthd_get_state(mon_disk_data_t *disk, uint32 *type);
int gmgmthd_get_read_bytes(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_get_write_bytes(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_get_iorate(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_disk_get_root_disk(void);
int gmgmthd_get_root_disk_data(mon_disk_data_t *disk) ;
int gmgmthd_proc_diskstats(mon_disk_data_t *disk);
int gmgmthd_lfd_stats_get_value(const char *counter, uint64 *value, bn_type type);
int gmgmthd_get_diskbw(mon_disk_data_t *disk, uint64 *size);
int gmgmthd_get_ioerr_threshold(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_ioclr_threshold(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_dsperr_threshold(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_dspclr_threshold(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_bwerr_threshold(mon_disk_data_t *disk, uint32 *size);
int gmgmthd_get_bwclr_threshold(mon_disk_data_t *disk, uint32 *size);


static int
gmgmthd_system_get_mem_stat(const char *meminfo, const char *field, uint32 *ret_kb);

int gmgmthd_do_memory_calculation(uint64 *memutil_pct);
int is_new_disk(const char *diskname, tbool *present);
/*Static Declarations */

static tbool
is_CMM_network_thread_dead_lock(void);


/*   Counter Bases Logic
 *   Have a list of counters which are to be collected with help of nkncnt,
 *   monitoring nodes(Monitoring nodes not yet fully implemented)
 *   Evertime the livenes check happens sample the counter in the list and store the latest 
 *   value in r2,move the existing value of r2 to r1,
 *   see if there is a diff and store the return value in ret,
 */
//Thilak - 2011-02-16 Enabled again

static uint64 get_mfd_intf_stats (void);
/* ------------------------------------------------------------------------- */
int
gmgmthd_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                  void *data, bn_binding_array *resp_bindings)
{
    int err = 0;
    bn_binding *binding = NULL;
    tbool done = false;
    tbool ext_mon_node = false;
    uint32 num_parts = 0;
    const tstring *last_name_part = NULL;
    const char *t_disk = NULL;
    const char *t_type = NULL;

    tbool live_return = false;
    bn_binding *ret_binding = NULL;
    uint32 tot_mem = 0, free_mem = 0, ram_cache = 0;
    uint64 used_mem =0, nkn_tot_mem = 0, memutil_pct = 0;

    if(g_rebuild_local_disk_stat) {
	gmgmthd_disk_query();
    }

    /* Check if it is our node */
    if ((!lc_is_prefix("/nkn/nvsd/namespace", bname, false)) &&
	    (!lc_is_prefix("/nkn/monitor/lfd/appmaxutil", bname, false))&& 
	    (!lc_is_prefix("/nkn/nvsd/state/alive", bname, false))&& 
	    (!lc_is_prefix("/nkn/monitor/sas/disk", bname, false))&& 
	    (!lc_is_prefix("/nkn/monitor/sata/disk", bname, false))&& 
	    (!lc_is_prefix("/nkn/monitor/ssd/disk", bname, false))&& 
	    (!lc_is_prefix("/nkn/monitor/disk", bname, false))&&
	    (!lc_is_prefix("/nkn/monitor/root/disk", bname, false))&&
	    (!lc_is_prefix("/stat/nkn/system/memory/memutil_pct", bname, false))&&
            (!lc_is_prefix("/nkn/monitor/lfd/sasdiskutil", bname, false))&&
            (!lc_is_prefix("/nkn/monitor/lfd/satadiskutil", bname, false))&&
            (!lc_is_prefix("/nkn/monitor/lfd/ssddiskutil", bname, false))&&
            (!lc_is_prefix("/nkn/monitor/lfd/maxthroughput", bname, false))&&
            (!lc_is_prefix("/nkn/monitor/lfd/maxopenconnections", bname, false))
	){
        /*Not for us */
        goto bail;
    }

    if ( lc_is_prefix("/stat/nkn/system/memory/memutil_pct", bname, false)) {

	err  = gmgmthd_do_memory_calculation(&memutil_pct);
	bail_error(err);
        err = bn_binding_new_uint64( &ret_binding,
                                "/stat/nkn/system/memory/memutil_pct", ba_value, 0, memutil_pct);
        bail_error(err);

        err = bn_binding_array_append_takeover( resp_bindings, &ret_binding);
        bail_error(err);

    } else if(lc_is_prefix("/nkn/monitor/lfd/appmaxutil", bname, false)) {
	uint64 val  = 0;
	err = gmgmthd_lfd_stats_get_value("na.http.lf", &val, bt_uint64);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, val);
	bail_error(err);
    } else if(lc_is_prefix("/nkn/monitor/lfd/sasdiskutil", bname, false)) {
	uint64 val = 0; 
        err = gmgmthd_lfd_stats_get_value("na.http.disk.sas.lf", &val, bt_float64);
        err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
                ba_value, bt_uint64, val);
        bail_error(err);
    } else if(lc_is_prefix("/nkn/monitor/lfd/satadiskutil", bname, false)) {
	uint64 val = 0; 

        err = gmgmthd_lfd_stats_get_value("na.http.disk.sata.lf", &val, bt_float64);
        err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
                ba_value, bt_uint64, val);
        bail_error(err);
    } else if(lc_is_prefix("/nkn/monitor/lfd/ssddiskutil", bname, false)) {
	uint64 val = 0; 

        err = gmgmthd_lfd_stats_get_value("na.http.disk.ssd.lf", &val, bt_float64);
        err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
                ba_value, bt_uint64, val);
        bail_error(err);
    } else if(lc_is_prefix("/nkn/monitor/lfd/maxthroughput", bname, false)) {
        uint64 val  = 0;

        err = gmgmthd_lfd_stats_get_value("sys.if_bw_max", &val, bt_uint64);
        err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
                ba_value, bt_uint64, val);
        bail_error(err);
    } else if(lc_is_prefix("/nkn/monitor/lfd/maxopenconnections", bname, false)) {
        uint64 val  = 0;

        err = gmgmthd_lfd_stats_get_value("sys.active_conn_max", &val, bt_uint64);
        err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
                ba_value, bt_uint64, val);
        bail_error(err);
    }
    else {

        num_parts = tstr_array_length_quick(bname_parts);

        last_name_part = tstr_array_get_quick(bname_parts, num_parts - 1);
        if(bn_binding_name_parts_pattern_match(bname_parts, true,
	        	"/nkn/monitor/disk/**")) {
	    ext_mon_node = true;
	    t_disk =  tstr_array_get_str_quick(bname_parts, 3);
	    t_type = "any";
	    if(!t_disk) {
	        goto bail;
	    }
	    pstDisk = get_disk_element(t_disk, t_type);
	    if(!pstDisk->name){
		goto bail;
	    }
	    if(pstDisk->type == DISK_ROOT) {
		gmgmthd_get_root_disk_data(pstDisk) ;
	    }
        }
        if((bn_binding_name_parts_pattern_match(bname_parts, true,
	    	    "/nkn/monitor/sas/disk/**")) ||
	        (bn_binding_name_parts_pattern_match(bname_parts, true,
						 "/nkn/monitor/sata/disk/**"))||
	        (bn_binding_name_parts_pattern_match(bname_parts, true,
						 "/nkn/monitor/root/disk/**"))||
	        (bn_binding_name_parts_pattern_match(bname_parts, true,
						 "/nkn/monitor/ssd/disk/**"))) {
	    ext_mon_node = true;
	    t_disk =  tstr_array_get_str_quick(bname_parts, 4);
	    t_type = tstr_array_get_str_quick(bname_parts, 2);
	    if(!t_disk) {
	        goto bail;
	    }
	    pstDisk = get_disk_element(t_disk, t_type);
	    if(!pstDisk->name){
		goto bail;
	    }
	}
        if((lc_is_prefix("/nkn/monitor/sas/disk", bname, false))||
	    (lc_is_prefix("/nkn/monitor/sata/disk", bname, false)) ||
	    (lc_is_prefix("/nkn/monitor/disk", bname, false)) ||
	    (lc_is_prefix("/nkn/monitor/root/disk", bname, false)) ||
	    (lc_is_prefix("/nkn/monitor/ssd/disk", bname, false))) {
	    err = gmgmthd_mon_handle_get_wildcard(bname,bname_parts, num_parts,
		    last_name_part, resp_bindings, &done);
	    bail_error(err);
	    if(done) {
	        goto bail;
	    }
        }

    if(ext_mon_node && !strcmp(ts_str(last_name_part), "serial")) {
	const char *t_diskname = NULL;
	if(pstDisk->serial) {
	    t_diskname = pstDisk->serial;
	}
	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, t_diskname);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "state")) {
	uint32 type = 0;
	const char *state = NULL;
	err = gmgmthd_get_state(pstDisk , &type);
	bail_error(err);
	state = disk_state[type];
	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, state);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "type")) {
	const char *t_diskname = disk_type[pstDisk->type];
	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, t_diskname);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "model")) {
	uint32 type = 0;
	const char *provider = NULL;
	err = gmgmthd_get_provider(pstDisk , &type);
	bail_error(err);
	provider = disk_cache_provider_names[type];
	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, provider);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "blocksize")) {
	uint32 size = 0;
	err = gmgmthd_get_blocksize(pstDisk, &size);
	bail_error(err);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "totalsize")) {
	uint64 size = 0;
	err = gmgmthd_get_totalsize(pstDisk, &size);
	bail_error(err);
	uint32 val = (uint32)(size/1024);

	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "usedblock")) {
	uint32 size = 0;
	err = gmgmthd_get_usedblock(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "freespace")) {
	uint64 size = 0;
	err = gmgmthd_get_freespace(pstDisk, &size);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "capacity")) {
	uint64 size = 0;
	err = gmgmthd_get_totalsize(pstDisk, &size);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "util")) {
	uint64 size = 0;
	err = gmgmthd_get_util(pstDisk, &size);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "iorate")) {
	uint64 size = 0;
	err=  gmgmthd_get_iorate(pstDisk, &size);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "ioerr")) {
	uint32 size = 0;
	err = gmgmthd_get_ioerr_threshold(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "ioclr")) {
	uint32 size = 0;
	err = gmgmthd_get_ioclr_threshold(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "spaceerr")) {
	uint32 size = 0;
	err = gmgmthd_get_dsperr_threshold(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "spaceclr")) {
	uint32 size = 0;
	err = gmgmthd_get_dspclr_threshold(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "bwerr")) {
	uint32 size = 0;
	err = gmgmthd_get_bwerr_threshold(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "bwclr")) {
	uint32 size = 0;
	err = gmgmthd_get_bwclr_threshold(pstDisk, &size);
	err = bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		ba_value, bt_uint32, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "diskbw")) {
	uint64 size = 0;
	err = gmgmthd_get_diskbw(pstDisk, &size);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "read")) {
	uint64 size = 0;
	err = gmgmthd_get_read_bytes(pstDisk, &size);
	bail_error(err);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    if(ext_mon_node && !strcmp(ts_str(last_name_part), "write")) {
	uint64 size = 0;
	err = gmgmthd_get_write_bytes(pstDisk, &size);
	bail_error(err);
	err = bn_binding_new_parts_uint64(&binding, bname, bname_parts, true,
		ba_value, bt_uint64, size);
	bail_error(err);
    }
    }
    if(binding) {
	err = bn_binding_array_append_takeover(resp_bindings, &binding);
    }

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
gmgmthd_mon_handle_iterate(const char *bname, const tstr_array *bname_parts,
                      void *data, tstr_array *resp_names)
{
    int err = 0;
    uint32 i = 0;
    char *bn_name = NULL;
    const char *type = NULL;
    tbool compare = false;


    if(lc_is_prefix("/nkn/monitor/disk", bname, false)) {
	type = "any";
    }else {
	type = tstr_array_get_str_quick(bname_parts, 2);
	compare = true;
    }



    for (i = 0; i < MAX_DISK; ++i) {
	if(lstDisk[i].name) {
	    if(compare ) {
		 if(0== (strcmp(type, disk_cache_provider_names[lstDisk[i].imodel]))){
		     bn_name = smprintf("%s", lstDisk[i].name);
		     err = tstr_array_append_str_takeover(resp_names, &bn_name);
		     bail_error(err);
		 }else if(0 ==(strcmp(type, "root"))){
		     if(lstDisk[i].type == DISK_ROOT) {
			 bn_name = smprintf("%s", lstDisk[i].name);
			 err = tstr_array_append_str_takeover(resp_names, &bn_name);
			 bail_error(err);
		     }
		 }
	    }else {
		bn_name = smprintf("%s", lstDisk[i].name);
		err = tstr_array_append_str_takeover(resp_names, &bn_name);
		bail_error(err);
	    }
	}
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
gmgmthd_mon_handle_get_wildcard(const char *bname, const tstr_array *bname_parts,
                           uint32 num_parts, const tstring *last_name_part,
                           bn_binding_array *resp_bindings,
                           tbool *ret_done)
{
    int err = 0;
    bn_binding *binding = NULL;
    bn_type btype = bt_NONE;

    bail_null(last_name_part);
    bail_null(ret_done);
    *ret_done = false;

    /* 
     * We assume the caller has verified that bname_parts begins
     * with "/nkn/monitor/disk/<wc>", so we need only check if
     * we're at the wildcard number of parts and return the last name part
     * as the value.
     */
    if(bn_binding_name_parts_pattern_match(bname_parts, true,
		    "/nkn/monitor/disk/**")) {
	if (num_parts == 4) {
	    btype = bt_string;
	}
	else {
	    goto bail;
	}
    }else {
	if (num_parts == 5) {
	    btype = bt_string;
	}
	else {
	    goto bail;
	}
    }
    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
                                   ba_value, btype, 0, ts_str(last_name_part));
    bail_error(err);

    err = bn_binding_array_append_takeover(resp_bindings, &binding);
    bail_error(err);
    *ret_done = true;

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */

tbool CMM_liveness_check(void)
{
        tbool ret_val = false;
	lt_time_sec curr_time = 0;
	static lt_time_sec prev_time = 0;
	static tbool last_ret_val = false;
	lt_time_sec time_diff = 0;

	curr_time = lt_curr_time ();
	time_diff = curr_time - prev_time;

	/*Check to make sure the network thread deadlock function is called at a 2 sec time interval
	Since the nkn_counters are updated at a 2 sec time interval*/
	if (time_diff >= 2)
	{
        	/* Check to see if any of the network thread is alive */
		ret_val = is_CMM_network_thread_dead_lock();
        	if(ret_val ) {

                	/*Something is definitely wrong with NVSD
                 	* as network thread counters are stuck */
                	lc_log_basic(LOG_DEBUG, "CMM_liveness_check: Network thread is stuck!!!\n");
		}
		prev_time = curr_time;
		last_ret_val = ret_val;
        } else {
		ret_val = last_ret_val;
	}
bail:
        return ret_val;
}


/*Seperate function for CMM liveness check. see ther network thread counters and get a diff between the cur and prev one,
as long as there is a diff ,we are safe */
static tbool
is_CMM_network_thread_dead_lock(void)
{
        int err = 0;
        uint32 i = 0;
        tbool is_network_thread_stuck = false;
        tbool retval = false;
	char counter_network_thread[256] = {0};
	uint32 num_network_threads = 0;
	/*Defining seperate counters for CMM liveness check*/
	static uint32 CMM_network_thread_counter_val_r1[MAX_EPOLL_THREADS];
	static uint32 CMM_network_thread_counter_val_r2[MAX_EPOLL_THREADS];

	num_network_threads = nkncnt_get_uint32("glob_network_num_threads");

        for(i = 0; i < num_network_threads; i++) {

		snprintf(counter_network_thread, sizeof(counter_network_thread), "monitor_%u_network_thread",i);

                CMM_network_thread_counter_val_r2[i] = nkncnt_get_uint32(counter_network_thread);

		if((CMM_network_thread_counter_val_r2[i] == 0 && CMM_network_thread_counter_val_r1[i] == 0))
		{
			lc_log_basic(LOG_DEBUG, "CMM_liveness_check:Any one zero %u r2 = %u, r1 = %u",
                                        i, CMM_network_thread_counter_val_r2[i], CMM_network_thread_counter_val_r1[i] );
			continue;
		}

                if(!(CMM_network_thread_counter_val_r2[i] - CMM_network_thread_counter_val_r1[i]) ) {
                        //Some network thread is stuck
                        lc_log_basic(LOG_DEBUG, "CMM_liveness_check:Network thread might be stuck:network thread %u r2 = %u, r1 = %u",
                                        i, CMM_network_thread_counter_val_r2[i], CMM_network_thread_counter_val_r1[i] );
                        is_network_thread_stuck = true ;
                }
                CMM_network_thread_counter_val_r1[i] = CMM_network_thread_counter_val_r2[i];

        }
        if(is_network_thread_stuck) {
                        lc_log_basic(LOG_DEBUG, "CMM_liveness_check:Network thread got stuck");
                        retval = true;
        }
bail:
        return retval;
}
static mon_disk_data_t *get_disk_element(const char *cpdisk, const char *type) {
    int i = 0;
    int free_entry_index = -1;

    for(i = 0; i < MAX_DISK; i++) {
	if(NULL == lstDisk[i].name) {
	//if(strlen(lstDisk[i].name) == 0) {//This condition must be changedif(NULL == lstDisk[i].name) {
	    if(-1 == free_entry_index)
		free_entry_index = i;
	    continue;
	}else if (0 == (strcmp(cpdisk, lstDisk[i].name))) {
	    if(0== (strcmp(type,disk_cache_provider_names[ lstDisk[i].imodel]))) {
		return (&(lstDisk[i]));
	    }else if(0 == (strcmp(type, "any"))) {
		return (&(lstDisk[i]));
	    }else if(0 == (strcmp(type, "root"))) {
		if(lstDisk[i].type == DISK_ROOT) {
		    return (&(lstDisk[i]));
		}
	    }
	}
    }
    if(-1 != free_entry_index) {
	return (&(lstDisk[free_entry_index]));
    }
    return NULL;
}
#if 0
static int init_disk_hash_table(void)
{
    err = 0;
    ht =  g_hash_table(g_Str_hash, g_str_equal);
    if(!ht) {
	lc_log_basic(LOG_NOTICE, _("Error:creating disk hash table\n"));
	err =1 ;
	goto bail;
    }

bail:
    
    return err;
}
#endif
int gmgmthd_disk_query()
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;


    memset(lstDisk, 0, MAX_DISK*sizeof(mon_disk_data_t));
    /*Get the  disk cache*/
    err = mdc_get_binding_children(gmgmthd_mcc, NULL,NULL,
	    true, &bindings, false, true, "/nkn/nvsd/diskcache/monitor/diskname");
    if(bindings) {
	err = bn_binding_array_foreach(bindings, gmgmthd_disk_cfg_handle_change, &rechecked_licenses);
	bail_error(err);
    }
    /*get the root file system*/
    err = gmgmthd_disk_get_root_disk();
    bail_error(err);

    /*Reset the flag*/
    g_rebuild_local_disk_stat = 0;


bail:
    bn_binding_array_free(&bindings);
    return err;
}


static int
gmgmthd_disk_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
	                        bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_disk = NULL;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;
    
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    bail_null(rechecked_licenses_p);
    err = bn_binding_get_name(binding, &name);
    bail_error(err);
    /* Check if this is our node */
    if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/diskcache/monitor/diskname/**"))
    {
	/*-------- Get the disk name ------------*/
	bn_binding_get_name_parts (binding, &name_parts);
	bail_error(err);
	t_disk = tstr_array_get_str_quick (name_parts, 5);
	pstDisk = get_disk_element (t_disk, "any");
	if (!pstDisk)
	    goto bail;
	if (NULL == pstDisk->name) {
	    pstDisk->name = nkn_strdup_type(t_disk, mod_mgmt_charbuf);

	    pstDisk->type = DISK_CACHE;
	    err= gmgmthd_get_provider(pstDisk, &pstDisk->imodel);
	    bail_error(err);
	}
    }else {
	goto bail;

    }
    /*Get the disk_id- gives the serial number*/
    if(bn_binding_name_pattern_match((ts_str(name)), "/nkn/nvsd/diskcache/monitor/diskname/*/disk_id")) {
	tstring *serial = NULL;
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&serial);
	bail_error(err);
	if (strlen(pstDisk->serial)!= 0)
	    strcpy(pstDisk->serial, "\0");
	if (serial)
	    strlcpy(pstDisk->serial,ts_str(serial), ts_length(serial)+1);
	else
	    strcpy(pstDisk->serial, "\0");
	ts_free(&serial);

    }

bail:
    return err;
}

int gmgmthd_get_blocksize(mon_disk_data_t *disk, uint32 *size)
{
	counter_name_t blocksize = {0};
	if(disk->type ==  DISK_CACHE) {
	    snprintf(blocksize, sizeof(blocksize),"%s.dm2_block_size",disk->name);
	    *size = nkncnt_get_uint32(blocksize);
	}else {
	    *size = disk->blocksize;
	}
	return 0;
}
int gmgmthd_get_totalsize(mon_disk_data_t *disk, uint64 *size)
{
    /*total blocks * block size*/
	counter_name_t blocksize = {0};
	counter_name_t total_blocks = {0};
	uint32 blk = 0;
	uint32 tot = 0;
	uint64 tsize = 0;
	if(disk->type ==  DISK_CACHE) {
	    snprintf(blocksize, sizeof(blocksize),"%s.dm2_block_size",disk->name);
	    snprintf(total_blocks, sizeof(total_blocks),"%s.dm2_total_blocks",disk->name);
	    blk = nkncnt_get_uint32(blocksize);
	    tot = nkncnt_get_uint32(total_blocks);
	    tsize = (uint64)tot *(uint64) blk;
	}else {
	    tsize = disk->totalsize;
	}

	*size = tsize/ (1024 * 1024);
	return 0;
}
int gmgmthd_get_util(mon_disk_data_t *disk, uint64 *size)
{
    /*(total blocks  -free blocks)* block size*/
	counter_name_t blocksize = {0};
	counter_name_t total_blocks = {0};
	counter_name_t free_blocks = {0};
	uint32 total = 0;
	uint32 used = 0;
	uint64 tsize = 0;
	if(disk->type == DISK_CACHE) {
	    snprintf(blocksize, sizeof(blocksize),"%s.dm2_block_size",disk->name);
	    snprintf(total_blocks, sizeof(total_blocks),"%s.dm2_total_blocks",disk->name);
	    snprintf(free_blocks, sizeof(free_blocks),"%s.dm2_free_blocks",disk->name);
	    total =  nkncnt_get_uint32(total_blocks);
	    if (total > 0) {
		used = total - nkncnt_get_uint32(free_blocks);
		tsize = floor((used * 100) /total);
	    }
	}else {
	    tsize = disk->util;
	}

	if (tsize > 100) {
	    lc_log_basic(LOG_NOTICE, "disk util calculation: %lu, type : %d",
		    tsize, disk->type);
	}
	*size = tsize;
	return 0;
}
int gmgmthd_get_usedblock(mon_disk_data_t *disk, uint32 *size)
{
    /*(total blocks  -free blocks)*/
	counter_name_t total_blocks = {0};
	counter_name_t free_blocks = {0};
	uint32 blk = 0;
	uint32 used = 0;
	if(disk->type == DISK_CACHE) {
	    snprintf(total_blocks, sizeof(total_blocks),"%s.dm2_total_blocks",disk->name);
	    snprintf(free_blocks, sizeof(free_blocks),"%s.dm2_free_blocks",disk->name);
	    used = nkncnt_get_uint32(total_blocks)- nkncnt_get_uint32(free_blocks);
	}else {
	    used =  disk->usedblock;
	}
	    *size = used;
	return 0;
}

int gmgmthd_get_freespace(mon_disk_data_t *disk, uint64 *size)
{
    /*(free blocks)* block size*/
	counter_name_t blocksize = {0};
	counter_name_t free_blocks = {0};

	uint32 blk = 0;
	uint32 fre = 0;
	uint64 tsize = 0;
	if(disk->type == DISK_CACHE) {
	    snprintf(blocksize, sizeof(blocksize),"%s.dm2_block_size",disk->name);
	    snprintf(free_blocks, sizeof(free_blocks),"%s.dm2_free_blocks",disk->name);
	    blk = nkncnt_get_uint32(blocksize);
	    fre = nkncnt_get_uint32(free_blocks);
	    tsize = ((uint64)fre *(uint64) blk);
	}else {
	    tsize = pstDisk->freespace;
	}

	/* convert it to MB */
	*size = tsize / (1024 * 1024);
	return 0;
}

int gmgmthd_get_provider(mon_disk_data_t *disk, uint32 *type)
{
    counter_name_t p_type = {0};
    snprintf(p_type, sizeof(p_type), "%s.dm2_provider_type", disk->name);
    *type = nkncnt_get_uint32(p_type);
    return 0;
}
int gmgmthd_get_state(mon_disk_data_t *disk, uint32 *type)
{
    counter_name_t p_type = {0};
    snprintf(p_type, sizeof(p_type), "%s.dm2_state", disk->name);
    *type = nkncnt_get_uint32(p_type);
    return 0;
}
int gmgmthd_get_read_bytes(mon_disk_data_t *disk, uint64 *size)
{
    counter_name_t readb = {0};
    if(disk->type == DISK_CACHE) {
	snprintf(readb, sizeof(readb), "%s.dm2_raw_read_bytes", disk->name);
	*size = nkncnt_get_uint64(readb);
    }else {
	*size = disk->readb;
    }
    return 0;
}
int gmgmthd_get_write_bytes(mon_disk_data_t *disk, uint64 *size)
{
    counter_name_t writeb = {0};
    if(disk->type == DISK_CACHE) {
	snprintf(writeb, sizeof(writeb), "%s.dm2_raw_write_bytes", disk->name);
	*size = nkncnt_get_uint64(writeb);
    }else {
	*size = disk->writeb;
    }
    return 0;
}
int gmgmthd_get_iorate(mon_disk_data_t *disk, uint64 *size)
{
    node_name_t diskn  = {0};
    uint32 type = 0;
    int err = 0;
    bn_binding *binding = NULL;
    char *node_name_esc = NULL;
    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);

	snprintf(diskn, sizeof(diskn), "/stats/state/chd/%s_disk_io/node/\\/nkn\\/monitor\\/%s\\/disk\\/%s\\/disk_io/last/value", disk_cache_provider_names[type], disk_cache_provider_names[type], disk->name);
    }else {
	node_name_t node = {0};
	snprintf(node, sizeof(node),"/nkn/monitor/root/disk/\\%s/disk_io", disk->name);
	err = bn_binding_name_escape_str( node, &node_name_esc );
	bail_error(err);
	snprintf(diskn, sizeof(diskn), "/stats/state/chd/root_disk_io/node/%s/last/value",node_name_esc);
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    diskn, NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint64( binding, ba_value, NULL, size);
	    bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    safe_free(node_name_esc);
    return 0;
}

int gmgmthd_disk_get_root_disk(void)
{
    int err = 0;
    tstr_array *mount_names = NULL;
    tstr_array *dev_names = NULL;
    tbool_array *remotes =  NULL;
    uint32 i = 0, num_mounts = 0, num_remotes = 0;
    tbool is_remote = false;
    err = lf_get_mount_points(&mount_names, &dev_names, NULL, &remotes);
    bail_error(err);
    
    num_mounts = tstr_array_length_quick(mount_names);
    num_remotes = tbool_array_length_quick(remotes);
    if (num_mounts > 0 && num_remotes > 0) {
	bail_require(num_mounts == num_remotes);
    }
    for (i = 0; i < num_mounts; i++) {
	is_remote = tbool_array_get_quick(remotes, i);
	if (is_remote) {
	    continue;
	}
	const char *name = tstr_array_get_str_quick(mount_names, i);
	const char *dev = tstr_array_get_str_quick(dev_names, i);
	pstDisk = get_disk_element(name, "any");
	if(!pstDisk) {
	    goto bail;
	}
	if (NULL == pstDisk->name) {
	    //pstDisk->name = calloc(1, (strlen(name) + 1)*sizeof(char));
	    pstDisk->name = nkn_strdup_type(name, mod_mgmt_charbuf);

	//if (strlen(pstDisk->name)== 0) 
	    //strlcpy(pstDisk->name, name, strlen(name)+1);
	    pstDisk->type = DISK_ROOT;
	    strlcpy(pstDisk->serial, dev, strlen(dev) + 1);
//	    strlcpy(pstDisk->type, "root", 5);
	    gmgmthd_get_root_disk_data(pstDisk);
	}

    }
bail:
    tstr_array_free(&mount_names);
    tbool_array_free(&remotes);
    return err;
}

int gmgmthd_get_root_disk_data(mon_disk_data_t *disk) 
{
    int err = 0;
    lt_time_sec curr = 0;
    
    const char *path = disk->name;
#ifdef PROD_TARGET_OS_FREEBSD
    struct statfs stats;
#else
    struct statvfs stats;
#endif

    /*See if  the time diff is 5 sec, if diff is < 5 dont get the stats, use the old ones*/
    curr = lt_curr_time();
    if(curr - disk->snap < 5) {
	/*do nothing*/
	goto bail;
    }
    disk->snap = curr;
#ifdef PROD_TARGET_OS_FREEBSD
    err = statfs(path, &stats);
    bail_error_quiet(err);
    disk->blocksize = stats.f_bsize;
    disk->usedblock = stats.f_blocks - stats.f_bfree;
    disk->totalsize = (stats.f_blocks * stats.f_bsize)/1024;
    disk->freespace = stats.f_bfree * stats.f_bsize;
    disk->capacity = (stats.f_blocks * stats.f_bsize);
    disk->util = floor((stats.f_blocks - stats.f_bfree) * 100 )/stats.f_blocks;
#else
    err = statvfs(path, &stats);
    bail_error_quiet(err);
    disk->blocksize = stats.f_frsize;
    disk->usedblock = stats.f_blocks - stats.f_bfree;
    disk->totalsize = (stats.f_blocks * stats.f_frsize)/1024;
    disk->freespace = stats.f_bfree * stats.f_frsize;
    disk->capacity = (stats.f_blocks * stats.f_frsize);
    disk->util = floor((stats.f_blocks - stats.f_bfree) * 100 )/stats.f_blocks;
#endif
    /*get the read bytes , write bytes*/
    err = gmgmthd_proc_diskstats(disk);


bail:
    return err;
}

int gmgmthd_proc_diskstats(mon_disk_data_t *disk)
{
    int err = 0;
    char *file_contents = NULL;
    uint64 diskstats_field[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    tstr_array *diskstats_lines = NULL;
    const char *line = NULL;
    uint32 field_num = 0; /* /proc/diskstats ordinal field position*/
    int nf = 0;
    uint32 i = 0, num_lines = 0;
    uint32 dev_maj = 0, dev_min = 0;
    char devname[32];
    uint32 counter = 0;
    int multiplier = 1;
    memset(devname, '\0', (sizeof(devname)/ sizeof(char)));

    /*Logic is taken from md_system.c from TM source*/
    err = lf_read_file("/proc/diskstats", &file_contents, NULL);
    bail_error(err);
    /*for read bytes -> field needed is 3 , multiplier = MD_SYSTEM_BYTES_PER_SECTOR*/
    /* msi_disk_io_read_bytes, msi_disk_io_write_bytes -> field is 7 */
    multiplier = MD_SYSTEM_BYTES_PER_SECTOR;
    err = ts_tokenize_str(file_contents, '\n', 0 ,0,
	    ttf_ignore_trailing_separator, &diskstats_lines);
    bail_error_null(err, diskstats_lines);
    num_lines = tstr_array_length_quick(diskstats_lines);
    for( i = 0; i < num_lines; i++) {
	line = tstr_array_get_str_quick(diskstats_lines, i);
	bail_null(line);
	nf =  sscanf(line, "%u %u %31s %" PRIu64 "%" PRIu64 "%" PRIu64
		"%" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64 "%" PRIu64
		"%" PRIu64 "%" PRIu64 "%" PRIu64,
		&dev_maj, &dev_min, devname,
		&diskstats_field[0], &diskstats_field[1],
		&diskstats_field[2], &diskstats_field[3],
		&diskstats_field[4], &diskstats_field[5],
		&diskstats_field[6], &diskstats_field[7],
		&diskstats_field[8], &diskstats_field[9],
		&diskstats_field[10]);
	bail_require_msg(nf == 14, "Unexpected line '%s' from %s",
		line, file_contents );
	if (lc_is_suffix(devname, disk->serial,false) ) {
	    disk->readb = (diskstats_field[2]) * multiplier;       
	    disk->writeb = (diskstats_field[6]) * multiplier;       
	    break;
	}
    }
bail:
    safe_free(file_contents);
    tstr_array_free(&diskstats_lines);
    return err;


}
int  lfd_connect(void)
{
    int err = 0;
    int shmid, max_cnt_space= 0;
    char *shm = NULL;
    uint64_t shm_size = 0;
    key_t shmkey;
    const char *producer = "/opt/nkn/sbin/lfd";

    memset(&g_lfd_mem_ctx, 0, sizeof(nkncnt_client_ctx_t));
    
    if((shmkey = ftok(producer, NKN_SHMKEY)) < 0) {
	lc_log_basic(LOG_NOTICE, _("ftok failed, lfd may not have shared counterin this machine\n"));
	goto bail;
    }
    max_cnt_space = MAX_CNTS_OTHERS;
    shm_size = (sizeof(nkn_shmdef_t) + max_cnt_space*(sizeof(glob_item_t) + 30));
    shmid = shmget(shmkey, shm_size, 0666);
    if (shmid < 0) {
	lc_log_basic(LOG_NOTICE, _("shmget error for lfd shared memory\n"));
	goto bail;
    }
    
    shm = shmat(shmid, NULL, 0);
    if(shm == (char *)-1) {
	lc_log_basic(LOG_NOTICE, _("shmat error for lfd shared memory\n"));
	goto bail;
    }

    err = nkncnt_client_init(shm, max_cnt_space, &g_lfd_mem_ctx);
    lc_log_basic(LOG_DEBUG, _("Error code :%d \n"), err);
    bail_error(err);
    lc_log_basic(LOG_NOTICE, _("Successfully connected to LFD shared memory\n"));
bail:
    return err;
}

/*counter needed for cache disk
 * dc_1.dm2_free_pages                      = 13735552             0.00 /sec
 * dc_1.dm2_total_pages                     = 13735552             0.00 /sec
 * dc_1.dm2_free_blocks                     = 214618               0.00 /sec
 * dc_1.dm2_total_blocks                    = 214618               0.00 /sec
 * dc_1.dm2_free_resv_blocks                = 214618               0.00 /sec
 * dc_1.dm2_raw_read_cnt                    = 0                    0.00 /sec
 * dc_1.dm2_raw_read_bytes                  = 0                    0.00 /sec
 * dc_1.dm2_raw_write_cnt                   = 0                    0.00 /sec
 * dc_1.dm2_raw_write_bytes                 = 0                    0.00 /sec
 * dc_1.dm2_state                           = 10                   0.00 /sec
 * dc_1.dm2_provider_type                   = 5                    0.00 /sec
 */

int
gmgmthd_lfd_stats_get_value(const char *counter, uint64 *value, bn_type type)
{
        int err = 0;
	glob_item_t *out = NULL;
	uint64_t val = 0;
	if(g_lfd_mem_ctx.max_cnts) {
	    err = nkncnt_client_get_exact_match(&g_lfd_mem_ctx, counter, &out);
	    bail_error(err);
	    if(out) {
		/*The jmfcSas/Sata/Ssd Disk util value is of type float. Handling
		 * the float type value from LF shared memory
		 */
		if(type == bt_float64) {
		    val = round(*((double *)&out->value));
		}else {
		    val = out->value;
		}
	    }
	}
	else{
	    lc_log_basic(LOG_NOTICE, _("Waiting for lfd connect\n"));
	    lfd_connect();
	}
	*value  = val;
bail:
	return err;
}
/*Dummy function*/
int nkn_mon_add(const char *name, const char *instance, void *obj,
	                uint32_t size)
{
        return 0;
}
/* End of gmgmthd.c */
int gmgmthd_get_diskbw(mon_disk_data_t *disk, uint64 *size)
{
    node_name_t diskn  = {0};
    uint32 type = 0;
    int err = 0;
    char *node_name_esc = NULL;
    bn_binding *binding = NULL;
    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);

	snprintf(diskn, sizeof(diskn), "/stats/state/chd/%s_disk_bw/node/\\/nkn\\/monitor\\/%s\\/disk\\/%s\\/disk_bw/last/value", disk_cache_provider_names[type], disk_cache_provider_names[type], disk->name);
    }else {
	node_name_t node = {0};
	snprintf(node, sizeof(node),"/nkn/monitor/root/disk/\\%s/disk_bw", disk->name);
	err = bn_binding_name_escape_str( node, &node_name_esc );
	bail_error(err);
	snprintf(diskn, sizeof(diskn), "/stats/state/chd/root_disk_bw/node/%s/last/value",node_name_esc);
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    diskn, NULL);
    bail_error(err);

    if(binding) {
	err = bn_binding_get_uint64( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    safe_free(node_name_esc);

    return 0;
}

int gmgmthd_get_ioerr_threshold(mon_disk_data_t *disk, uint32 *size)
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding *binding = NULL;
    uint32 type = 0;


    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);
	snprintf(node, sizeof(node), "/stats/config/alarm/%s_disk_io/rising/error_threshold", disk_cache_provider_names[type]);
    }else {
	snprintf(node, sizeof(node), "/stats/config/alarm/root_disk_io/rising/error_threshold");
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    node, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    return err;

}
int gmgmthd_get_ioclr_threshold(mon_disk_data_t *disk, uint32 *size)
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding *binding = NULL;
    uint32 type = 0;

    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);
	snprintf(node, sizeof(node), "/stats/config/alarm/%s_disk_io/rising/clear_threshold", disk_cache_provider_names[type]);
    }else {
	snprintf(node, sizeof(node), "/stats/config/alarm/root_disk_io/rising/clear_threshold");
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    node, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    return err;

}

int gmgmthd_get_dsperr_threshold(mon_disk_data_t *disk, uint32 *size)
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding *binding = NULL;
    uint32 type = 0;

    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);
	snprintf(node, sizeof(node), "/stats/config/alarm/%s_disk_space/falling/error_threshold", disk_cache_provider_names[type]);
    }else {
	snprintf(node, sizeof(node), "/stats/config/alarm/root_disk_space/falling/error_threshold");
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    node, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    return err;

}
int gmgmthd_get_dspclr_threshold(mon_disk_data_t *disk, uint32 *size)
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding *binding = NULL;
    uint32 type = 0;

    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);
	snprintf(node, sizeof(node), "/stats/config/alarm/%s_disk_space/falling/clear_threshold", disk_cache_provider_names[type]);
    }else {
	snprintf(node, sizeof(node), "/stats/config/alarm/root_disk_space/falling/clear_threshold");
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    node, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    return err;

}

int gmgmthd_get_bwerr_threshold(mon_disk_data_t *disk, uint32 *size)
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding *binding = NULL;
    uint32 type = 0;

    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);
	snprintf(node, sizeof(node), "/stats/config/alarm/%s_disk_bw/rising/error_threshold", disk_cache_provider_names[type]);
    }else {
	snprintf(node, sizeof(node), "/stats/config/alarm/root_disk_bw/rising/error_threshold");
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    node, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    return err;

}
int gmgmthd_get_bwclr_threshold(mon_disk_data_t *disk, uint32 *size)
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding *binding = NULL;
    uint32 type = 0;

    if(disk->type == DISK_CACHE) {
	gmgmthd_get_provider(pstDisk, &type);
	snprintf(node, sizeof(node), "/stats/config/alarm/%s_disk_bw/rising/clear_threshold", disk_cache_provider_names[type]);
    }else {
	snprintf(node, sizeof(node), "/stats/config/alarm/root_disk_bw/rising/clear_threshold");
    }
    err = mdc_get_binding( gmgmthd_mcc, NULL, NULL, false, &binding,
	    node, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32( binding, ba_value, NULL, size);
	bail_error(err);
    }
bail:
    bn_binding_free(&binding);
    return err;

}

int gmgmthd_watch_nvsd_up_context_init(void)
{
    int err = 0;
    watch_dir_t *watch_context = NULL;
    memset(&(g_nvsd_watch), 0, sizeof(watch_dir_t));
    g_nvsd_watch.index = 0;
    g_nvsd_watch.context_data = &(g_nvsd_watch);

bail:
    return err;
}

int gmgmthd_set_nvsd_watch_timer(watch_dir_t *context)
{
    int err = 0;

    if(context->poll_event) {
	lc_log_basic(LOG_INFO, _("Already event registered\n"));
	goto bail;
    }
    err = lew_event_reg_timer(gmgmthd_lwc,
	    &(context->poll_event),
	    (context->callback),
	    (void *) (context),
	    (context->poll_frequency));
    bail_error(err);
bail:
    return err;
}

int
gmgmthd_handle_nvsd_up(int fd, short event_type,
	void *data, lew_context *ctxt)
{
    int err = 0;
    watch_dir_t *watch_context = (watch_dir_t *)data;

    uint32 is_nvsd_up = 0;

    is_nvsd_up = nkncnt_get_uint32("nvsd.global.system_init_done");
    if(is_nvsd_up) {
        err = lew_event_delete(gmgmthd_lwc, &(watch_context->poll_event));

	/* NVSD is up set app-led to green(3) */
	if(is_pacifica) {
	    lc_log_basic(LOG_INFO, "NVSD is up, set app-led to green(3)");
	    err = set_gpio_led(APP_LED, GPIO_GREEN);
	    bail_error(err);
	}

	gmgmthd_disk_query();
    } else {
        err = lew_event_delete(gmgmthd_lwc, &(watch_context->poll_event));
	err = gmgmthd_status_timer_context_allocate(10000);
    }


bail:
    return err;
}

int is_new_disk(const char *diskname, tbool *present) 
{
    int err = 0;
    node_name_t  node = {0};
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

        /*Get the  disk cache*/
    snprintf(node, sizeof(node), "/nkn/nvsd/diskcache/monitor/diskname/%s", diskname);
    err = mdc_get_binding_children(gmgmthd_mcc, NULL,NULL,
	    true, &bindings, false, true, node);
    if(bindings) {
	err = bn_binding_array_foreach(bindings, gmgmthd_disk_cfg_handle_change, &rechecked_licenses);
	bail_error(err);
	*present = true;
    }else {
	*present = false;
    }
 
bail:
    bn_binding_array_free(&bindings);
    return err;
}


int gmgmthd_status_timer_context_allocate(lt_dur_ms  poll_frequency)
{
        int err = 0;
	int i = 0;
	watch_dir_t *watch_context = NULL;
	watch_context = &g_nvsd_watch;
	bail_null(watch_context);
	
	watch_context = &g_nvsd_watch;
	bail_null(watch_context);
	watch_context->callback = gmgmthd_handle_nvsd_up;
	watch_context->poll_frequency = 10000; // 10sec timer. This is the time needed to ensure nvsd is up
	err = gmgmthd_set_nvsd_watch_timer(watch_context);
	bail_error(err);
bail:
	return err;
}

int gmgmthd_do_memory_calculation(uint64 *memutil_pct)
{
    int err = 0;
    uint32 tot_mem = 0, free_mem = 0, ram_cache = 0;
    uint64 used_mem =0, nkn_tot_mem = 0;
    uint32 mem_free = 0, cached = 0, buffers = 0;

    /*Read the /proc/memstats to get the free_plus_buffers_cached and 
     * total physical memory details. This is done to avoid looping to statsd
     * to get these values.
     * Copied the code from the TM  to get the data 
     */

     char *file_contents = NULL;
     lf_read_file("/proc/meminfo", &file_contents, NULL);
     bail_null(file_contents);

     err = gmgmthd_system_get_mem_stat(file_contents, "MemTotal", &tot_mem);
     bail_error(err);

     /*Get the free + buffers_cached value*/
     err = gmgmthd_system_get_mem_stat(file_contents, "MemFree", &mem_free);
     bail_error(err);
     err = gmgmthd_system_get_mem_stat(file_contents, "Buffers", &buffers);
     bail_error(err);
     err = gmgmthd_system_get_mem_stat(file_contents, "Cached", &cached);
     bail_error(err);
     free_mem = mem_free + buffers + cached;

     /*Call the NVSD shm to get the  max cachesize calculated*/
     ram_cache = nkncnt_get_uint32("glob_max_ram_cachesize_calculated");

     ram_cache *= 1024; //Converting MB to KB
     used_mem = tot_mem - free_mem;
     nkn_tot_mem = tot_mem - ram_cache;
     if(nkn_tot_mem) {
	 *memutil_pct = (used_mem * 100)/nkn_tot_mem;
     }
bail:
     safe_free(file_contents);
     return err;
}


static int
gmgmthd_system_get_mem_stat(const char *meminfo, const char *field, uint32 *ret_kb)
{
    int err = 0;
    const char *line = NULL;
    int nm = 0;
    line = strstr(meminfo, field);
    bail_null(line);
    line += strlen(field) + 1;  /* Add one byte for colon after field name */
    nm = sscanf(line, "%u", ret_kb);
    bail_require(nm == 1);
bail:
    return(err);
}

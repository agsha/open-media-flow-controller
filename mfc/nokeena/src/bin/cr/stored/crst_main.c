#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/un.h>

#include "event_dispatcher.h"
#include "entity_data.h"

#include "crst_nw_handler.h"
#include "crst_common_defs.h"
#include "crst_geodb_mgr.h"
#include "crst_mgmt.h"
#include "common/cr_cfg_file_api.h"
#include "common/nkn_dir_watch.h"
#include "cr_common_intf.h"
#include "nkn_mem_counter_def.h"
#include "libmgmtclient.h"
#include "cr/de_intf.h"

#include "ds_api.h"
#include "ds_glob_ctx.h"
#include "ds_hash_table_mgmt.h"
#include "ds_tables.h"
#include "ds_config.h"

/* global declarations */
event_disp_t* gl_disp = NULL;
pthread_t gl_disp_thread, gl_mgmt_thread;
event_timer_t* gl_ev_timer = NULL;
pthread_t gl_ev_thread, gl_dir_watch_process_thread;
int32_t listen_fd;
obj_store_t *store_list[CRST_STORE_TYPE_MAX] = {NULL};
dir_watch_proc_mgr_t *gl_dwmgr = NULL;
cr_dns_store_cfg_t cr_dns_store_cfg;
cr_cfg_param_val_t crst_cfg [] = {
    {{"global.geodb_name", CR_STR_VAL_TYPE}, 
     &cr_dns_store_cfg.geodb_file_name},
};

/* static declarations */
static void initSignalHandlers(void); 
static void exitClean(int signal_num);
static int32_t openStoreSocket(void);
static int32_t initGeoDB(void);
static int32_t setupConfigDefaults(void);
static int32_t handleGeoDBInstall(char *path);

/* extern definitions */
extern void config_and_run_counter_update(void); //API as an extern?!

#define MAX_EVENT_DISP_DESC 10000
#define STORE_SOCK_FILE "/var/cr_store.sock"

int32_t main(int32_t argc, char *argv[]) {

    const char *cfg_file_name = 
	"/config/nkn/cr_store.conf.default";
    int32_t err = 0;
    dir_watch_th_obj_t *dwto = NULL;
    pthread_t dir_watch_thread;

    /* init signal handlers */
    //initSignalHandlers();
    g_thread_init(NULL);

    /* start nknlog thread */
    log_thread_start();
    sleep(2);

    /* read from config file */
    setupConfigDefaults();
    err = cr_read_cfg_file(cfg_file_name, crst_cfg);
    if (err) {
	DBG_LOG(WARNING, MOD_CRST, "error reading config file, "
		"reverting to defaults [err=%d]\n", err);
    }

    /* create an timed event scehduler */
    gl_ev_timer = createEventTimer(NULL, NULL);
    pthread_create(&gl_ev_thread, NULL, 
	    gl_ev_timer->start_event_timer, gl_ev_timer);

    /* start the counter shared memory copy infra */
    config_and_run_counter_update();

    /* start mgmt plane */
    pthread_create(&gl_mgmt_thread, NULL, crst_mgmt_init, 
		   &cr_dns_store_cfg);

    // TEST    
    init_dstore();
    char* config_fname = argv[1];
    config_parms_t* cfg = (config_parms_t*)nkn_calloc_type(1,
	    sizeof(config_parms_t),  mod_cr_ds);
    //    parse_config_file(config_fname,cfg);
    //configure_all_entities(cfg);

    /* start the event dispatcher */
    gl_disp = newEventDispatcher(0, MAX_EVENT_DISP_DESC, 0);
    pthread_create(&gl_disp_thread, NULL, gl_disp->start_hdlr, gl_disp);

    HOLD_CRST_CFG(&cr_dns_store_cfg);
    if ((err = initGeoDB())) {
	DBG_LOG(SEVERE, MOD_CRST, "Error Loading GEODB, Please "
		" install the GEODB for full functionality" 
		" if this is the fresh install then you will install"
		" have to install this later for the Geo-Load routing"
		" functionality [err=%d]", err);
	cr_dns_store_cfg.geodb_installed = 0;
    } else {
	cr_dns_store_cfg.geodb_installed = 1;
    }
    REL_CRST_CFG(&cr_dns_store_cfg);
    init_nkn_dir_watch_processor();
    gl_dwmgr = 
	create_dir_watch_processor_mgr(handleGeoDBInstall);
    if (!gl_dwmgr) {
	DBG_LOG(SEVERE, MOD_CRST, "Error starting dir watch, "
		"exiting");
	exit(1);
    }
    // start pmf processor
    pthread_create(&gl_dir_watch_process_thread, NULL, 
		   nkn_process_dir_watch,
		   (void*)gl_dwmgr); 

    // add the watch folder 
    err = create_dir_watch_obj("/nkn/maxmind/db", gl_dwmgr, &dwto);
    if (err) {
	DBG_LOG(SEVERE, MOD_CRST, "exiting daemon, unable to "
		"create directory watch obj [err=%d]", err);
	exit(1);
    }
    dir_watch_thread = start_dir_watch(dwto);
     
    /* start the IPC listner for dnsd <--> stored IPC */
    listen_fd = openStoreSocket();
    if (listen_fd < 0) {
	DBG_LOG(SEVERE, MOD_CRST, "error starting the IPC channel "
		"[err=%d], exiting the daemone", errno);
	exit(1);
    }
    entity_data_t* entity_ctx = newEntityData(listen_fd, NULL, NULL,
	    crst_listen_handler, NULL,
	    crst_epollerr_handler,
	    crst_epollhup_handler, NULL);
    if (entity_ctx == NULL) {
	DBG_LOG(SEVERE, MOD_CRST, "error starting the IPC channel "
		"[err=%d], exiting the daemone", errno);
	close(listen_fd);
	exit(-1);
    }
    entity_ctx->event_flags |= EPOLLIN;
    gl_disp->reg_entity_hdlr(gl_disp, entity_ctx);

    DBG_LOG(MSG, MOD_CRST, "CR Store Daemon Ready");
    pthread_join(gl_disp_thread, NULL);
    return 0;
}


static void initSignalHandlers(void) {

    struct sigaction action_cleanup;
    memset(&action_cleanup, 0, sizeof(struct sigaction));
    action_cleanup.sa_handler = exitClean;
    action_cleanup.sa_flags = 0;
    sigaction(SIGINT, &action_cleanup, NULL);
    sigaction(SIGTERM, &action_cleanup, NULL);
}


static void exitClean(int signal_num) {

    printf("SIGNAL: %d received\n", signal_num);
    //TODO : termination cleanup 
    close(listen_fd);
    exit(-1);
}

static int32_t openStoreSocket(void) {
    
    int32_t err=unlink(STORE_SOCK_FILE);    
    if (err) {
	if (errno != ENOENT) {
	    perror("error unlinking previous socket: ");
	    exit (-1);
	}
    }
    int32_t sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) {
	perror("socket create failed: ");
	exit(-1);
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(&addr.sun_path[0], STORE_SOCK_FILE);
    uint32_t addr_len = sizeof(addr.sun_family) + strlen(STORE_SOCK_FILE);
    if (bind(sd, (struct sockaddr*)&addr, addr_len) < 0) {
	perror("bind failed: ");
	exit(-1);
    }
    if (listen(sd, 1000) < 0) {
	perror("socket create failed: ");
	exit(-1);
    }
    return sd;
}

static int32_t initGeoDB() {
   
    obj_store_t *st = NULL;
    geodb_ctx_t *gc = NULL;
    int32_t err = 0;

    err = crst_geodb_store_create(&store_list[CRST_STORE_TYPE_GEO]);
    if (err) {
	goto error;
    }
    
    st = store_list[CRST_STORE_TYPE_GEO];
    err = crst_geodb_db_ctx_create(1000, &gc);
    if (err) {
	goto error;
    }

    gc->db_file_name[0] = '\0';
    err = st->init(st, gc);
    if (err) {
	if (err == -2) {
	    DBG_LOG(SEVERE, MOD_CRST, "GeoDB City Version "
		    "not installed");
	}
	goto error;
    }

    /* Do a sample lookup */
    const char *key = "24.24.24.24";
    uint32_t key_len = strlen(key), 
	resp_len = sizeof(geo_ip_t) * 2;
    geo_ip_t gip[2];
    err = st->read(st, (char *)key, key_len, 
		   (char *)&gip[0], &resp_len);
    if (err) {
	DBG_LOG(ERROR, MOD_CRST, "error making sample query to "
		"GeoDB [err=%d]", err);
	goto error;
    }
    if (gip[0].ginf.status_code != 200) {
	err = -1;
	DBG_LOG(ERROR, MOD_CRST, "error making sample query to "
		"GeoDB [err=%d]", err);
	goto error;
    }
    DBG_LOG(MSG, MOD_CRST, "Geo DB installed successfully and "
	    "sample DB query completed successfully");
    cr_dns_store_cfg.geodb_installed = 1;
    return err;

 error:
    if (gc) gc->delete(gc);
    return err;
}

static int32_t setupConfigDefaults(void) {
    
    cr_dns_store_cfg.max_domains = 10000;
    cr_dns_store_cfg.max_cache_entities = 200;

    return 0;
}

static int32_t handleGeoDBInstall(char *path) {
    
    obj_store_t *st = NULL;
    geodb_ctx_t *gc = NULL;
    int32_t err = 0;

    if (path) {
	if (strstr(path, ".new")) {
	    return 0;
	}
	
	if (strcmp(path, "/nkn/maxmind/db/GeoIPCity.dat")) {
	    DBG_LOG(MSG, MOD_CRST, "None GeoIPCity DB installed");
	    return 0;
	}
    }

    HOLD_CRST_CFG(&cr_dns_store_cfg);
    st = store_list[CRST_STORE_TYPE_GEO];
    if (cr_dns_store_cfg.geodb_installed) {
	st->shutdown(st);
    }
    err = crst_geodb_db_ctx_create(1000, &gc);
    if (err) {
	goto error;
    }

    gc->db_file_name[0] = '\0';
    err = st->init(st, gc);
    if (err) {
	if (err == -2) {
	    DBG_LOG(SEVERE, MOD_CRST, "GeoDB City Version "
		    "not installed");
	}
	goto error;
    }

    /* Do a sample lookup */
    const char *key = "24.24.24.24";
    uint32_t key_len = strlen(key), 
	resp_len = sizeof(geo_ip_t) * 2;
    geo_ip_t gip[2];
    err = st->read(st, (char *)key, key_len, 
		   (char *)&gip[0], &resp_len);
    if (err) {
	DBG_LOG(ERROR, MOD_CRST, "error making sample query to "
		"GeoDB [err=%d]", err);
	goto error;
    }
    if (gip[0].ginf.status_code != 200) {
	err = -1;
	DBG_LOG(ERROR, MOD_CRST, "error making sample query to "
		"GeoDB [err=%d]", err);
	goto error;
    }
    DBG_LOG(MSG, MOD_CRST, "Geo DB installed successfully and "
	    "sample DB query completed successfully");
    cr_dns_store_cfg.geodb_installed = 1;
    REL_CRST_CFG(&cr_dns_store_cfg);
    return err;
    
 error:    
    if (gc) gc->delete(gc);
    cr_dns_store_cfg.geodb_installed = 0;
    REL_CRST_CFG(&cr_dns_store_cfg);
    return err;
}
  

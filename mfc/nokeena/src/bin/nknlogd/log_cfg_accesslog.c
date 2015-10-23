#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define _GNU_SOURCE
#include <string.h>
#include <strings.h>
#define __USE_GNU
#include <fcntl.h>		/* only the defines on windows */
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <md_client.h>
#include <license.h>
#include <alloca.h>
#include <libgen.h>
#include <curl/curl.h>
#define HASH_UNITTEST
#include "nkn_hash.h"
#include "log_accesslog_pri.h"
#include "nknlogd.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "file_utils.h"
#include "proc_utils.h"

#include "lgd_worker.h"
#include "log_channel.h"
#include "nknlog_pub.h"

#define BASE_PATH	"/var/log/nkn/accesslog"

extern gcl_session *sess;
logd_file g_plf[MAX_FDS];
pthread_mutex_t	plf_lock = PTHREAD_MUTEX_INITIALIZER;

//Global response Callback for the accesslog session
int action_upload_callback(gcl_session *gcls, bn_response **inout_resp,
	void *arg);

static void ali_free(logd_file *p);
extern int channel_close(struct channel *c);
extern void unlink_svr_array(struct channel *c);

static int log_profile_get(const char *name, logd_file **plf)
{
    int err = -1;
    int i = 0;

    pthread_mutex_lock(&plf_lock);

    bail_null(plf);
    bail_null(name);

    *plf = NULL;

    for(i = 0; i < MAX_FDS; i++) {
	if (g_plf[i].profile && safe_strcmp(g_plf[i].profile, name) == 0) {
	    *plf = &g_plf[i];
	    err = 0;
	    break;
	}
    }

bail:
    pthread_mutex_unlock(&plf_lock);
    return err;
}

void *cp_copy_key(void *key)
{
    int *dst;

    if (key == NULL) return NULL;

    dst = (int *)malloc(sizeof(int));
    if (dst == NULL) return NULL;
    *dst = *(int *)key;

    return dst;
}

void cp_free_key(void *key)
{
    int *dst;

    if (key != NULL) {
	dst = (int *)key;
	free(dst);
    }
}

void *cp_copy_value(void *value)
{
    uploadInfo *dst;
    uploadInfo *src = (uploadInfo *)value;

    if (value == NULL)  return NULL;

    dst = (uploadInfo *)malloc(sizeof(uploadInfo));
    if (dst == NULL) return NULL;
    if((src->logtype == TYPE_ACCESSLOG) && (src->profile)){
	dst->profile = strdup(src->profile);
    }
    if(src->fullfilename) {
	dst->fullfilename = strdup(src->fullfilename);
    }
    if(src->filename){
	dst->filename = strdup(src->filename);
    }
    if(src->remote_url){
	dst->remote_url = strdup(src->remote_url);
    }
    if(src->password){
	dst->password = strdup(src->password);
    }
    dst->req_id = src->req_id;
    dst->logtype = src->logtype;

    return dst;
}

void cp_free_value(void *value)
{
    uploadInfo *src = (uploadInfo *)value;

    if (value != NULL) {
	if(src->logtype == TYPE_ACCESSLOG){
	    safe_free(src->profile);
	}
	safe_free(src->fullfilename);
	safe_free(src->filename);
	safe_free(src->remote_url);
	safe_free(src->password);
	src->req_id = 0;
	src->logtype = 0;
	safe_free(src);
    }
}

static int get_plf_free(logd_file **plf)
{
    int i = 0;
    int err = 0;

    bail_null(plf);
    *plf = NULL;
    pthread_mutex_lock(&plf_lock);
    for(; i < MAX_FDS; i++) {
	if (g_plf[i].profile == NULL) {
	    *plf = &(g_plf[i]);
	    memset(*plf, 0, sizeof(logd_file));
	    break;
	}
    }
    pthread_mutex_unlock(&plf_lock);
    complain_require_msg((*plf != NULL), "Unable to get free plf slot, dropping config");
bail:
    return err;
}

static void plf_free(logd_file **plf)
{
    int err = 0;
    logd_file *p = NULL;
    bail_null(plf);
    bail_null(*plf);

    p = *plf;
    complain_require_msg((p->fp == NULL), "fp not closed, but freeing plf");
    ali_free(p);

    if (p->io_buf) {
	free(p->io_buf);
	p->io_buf = NULL;
    }
    if (p->fullfilename) {
	free(p->fullfilename);
	p->fullfilename = NULL;
    }

bail:
    *plf = NULL;
    return;
}

static int log_profile_set_channel(logd_file *plf, struct channel *c)
{
    int err = 0;
    int i = 0;
    bail_null(plf);
    bail_null(c);

    pthread_mutex_lock(&plf_lock);
    pthread_mutex_lock(&(plf->lock));
    for(i = 0; i < 8; i++) {
	if (plf->channel[i] == NULL) {
	    plf->channel[i] = c;
	    break;
	}
    }

    pthread_mutex_unlock(&(plf->lock));
    pthread_mutex_unlock(&plf_lock);

bail:
    return err;
}

static int log_profile_unset_channel(logd_file *plf, struct channel *c)
{
    int err = 0;
    int i = 0;
    bail_null(plf);
    bail_null(c);

    pthread_mutex_lock(&plf_lock);
    pthread_mutex_lock(&(plf->lock));
    for(i = 0; i < 8; i++) {
	if (plf->channel[i] == c) {
	    plf->channel[i] = NULL;
	    break;
	}
    }

    pthread_mutex_unlock(&(plf->lock));
    pthread_mutex_unlock(&plf_lock);

bail:
    return err;
}

static int log_profile_close_channels(logd_file *plf)
{
    int err = 0;
    int i = 0;
    struct channel *c = NULL;

    bail_null(plf);


    for(i = 0; i < 8; i++) {
	if (plf->channel[i]) {
	    c = plf->channel[i];
	    channel_close(c);
	    plf->channel[i] = NULL;
	    //close(c->sku_fd);
	    c->accesslog = NULL;
	    c->sku_fd = -1;
	    log_channel_free(c);
	    unlink_svr_array(c);
	}
    }

bail:
    return err;
}


static const char * logexportpath = LOG_EXPORT_HOME;

extern void log_file_post_process(logd_file * plf) ;

extern void setbuffer(FILE *stream, char *buf, size_t size);
extern char *get_next_char(char *str, char *ch);
extern int add_flist(struct channel *c);

void accesslog_file_exit(struct channel *chan);
void accesslog_file_print_format(struct channel *chan);

extern int log_delete_accesslog_server(const char *name);
static int delete_log_dir(const char *name);

extern md_client_context *mgmt_mcc;
extern tbool mgmt_exiting;
extern int exit_req;

int
module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

int access_log_cfg_handle_add_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data);

int access_log_cfg_handle_delete_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data);

int access_log_query_init(void);


int log_cfg_acclog_conf_profile(const char *profile, bn_binding * binding);


static int create_log_dir(const char *name);

static int accesslog_save_fileid(logd_file *plf);
static int accesslog_read_fileid(logd_file *plf);

extern int accesslog_parse_format(logd_file * plf);
void accesslog_file_init(struct channel *chan);

int access_log_query_init(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_NOTICE, "accesslog initializing ..");

    err = mdc_get_binding_children(mgmt_mcc,
				   NULL,
				   NULL,
				   true,
				   &bindings,
				   false, true,
				   "/nkn/accesslog/config/profile");
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, access_log_cfg_handle_add_change,
				   &rechecked_licenses);
    bail_error(err);

    ht_uploadInfo = cp_hashtable_create_by_option(COLLECTION_MODE_DEEP |
	    COLLECTION_MODE_COPY,
	    64,
	    cp_hash_int,
	    (cp_compare_fn) cp_hash_compare_int,
	    (cp_copy_fn) cp_copy_key,
	    (cp_destructor_fn) cp_free_key,
	    (cp_copy_fn) cp_copy_value,
	    (cp_destructor_fn) cp_free_value);

    bail_null(ht_uploadInfo);

bail:
    bn_binding_array_free(&bindings);
    return err;

}

static void ali_free(logd_file *p)
{
    if (!p)
	return;
    if (p->ff_ptr) {
	free(p->ff_ptr); p->ff_ptr = NULL;
	p->ff_size = 0;
	p->ff_used = 0;
    }
    if (p->r_code) {
	free(p->r_code); p->r_code = NULL;
    }
}


int access_log_cfg_handle_add_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    tstring *str = NULL;
    uint16 serv_port = 0;
    tbool bool_value = false;
    tbool *rechecked_licenses_p = data;
    char *profile = NULL;
    struct channel *channel = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (bn_binding_name_pattern_match
    (ts_str(name), "/nkn/accesslog/config/profile/**")) {
	const char *pname = NULL;

	/* Get profile name first */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);

	pname = (char *) tstr_array_get_str_quick(name_parts, 4);
	if (pname == NULL)
	    goto bail; // Node deleted

	log_cfg_acclog_conf_profile(pname, binding);
    }

bail:
    safe_free(profile);
    ts_free(&str);
    tstr_array_free(&name_parts);
    return err;
}




static const char *al_filename =
    "/nkn/accesslog/config/profile/*/filename";
static const char *al_format = "/nkn/accesslog/config/profile/*/format";
static const char *al_max_filesz =
    "/nkn/accesslog/config/profile/*/max-filesize";
static const char *al_time_interval =
    "/nkn/accesslog/config/profile/*/time-interval";
static const char *al_url = "/nkn/accesslog/config/profile/*/upload/url";
static const char *al_pass = "/nkn/accesslog/config/profile/*/upload/pass";
static const char *al_filter_size =
    "/nkn/accesslog/config/profile/*/object/filter/size";
static const char *al_filter_resp =
    "/nkn/accesslog/config/profile/*/object/filter/resp-code/*";


int log_cfg_acclog_conf_profile(const char *profile, bn_binding * binding)
{
    int err = 0;
    struct channel *channel = NULL;
    const tstring *name = NULL;
    tstring *str = NULL;
    struct logd_file_t *al = NULL;
    tstr_array *name_parts = NULL;
    char *s_code = NULL;


    log_profile_get(profile, &al);

    if (al == NULL) {
	/* no profile. create one */
	//lc_log_basic(LOG_NOTICE, "Channel not present, "
		//"creating new channel for : %s", profile);
	err = get_plf_free(&al);
	bail_error_null(err, al);
	al->profile = safe_strdup(profile);
	al->r_code = malloc(MAX_RESP_CODE_FILTER * sizeof(int32_t));
	channel = log_channel_new(profile);
	log_channel_set_type(channel, ACCESSLOG, al);

	log_profile_set_channel(al, channel);
	channel->accesslog = al;

	err = log_channel_add((const char *) profile, channel);
	if (err < 0) {
	    /* failed to insert a new profile */
	    lc_log_basic(LOG_ERR,
		     "failed to insert a new profile config.");
	    log_channel_free(channel);
	    goto bail;
	}

	/* Construct a absolute file id name */
	char *p = alloca(strlen(BASE_PATH) + strlen(profile) + 16);
	sprintf(p, "%s/%s/.fileid", BASE_PATH, profile);
	al->file_id_name = strdup(p);

	al->flags = LOGD_FLAG_NEW_PROFILE;

    }

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    al->type = TYPE_ACCESSLOG;

    if (bn_binding_name_pattern_match(ts_str(name), al_filename)) {
	/* Configure filename for this profile */
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	if (al->filename != NULL) {
	    free(al->filename);
	}
	if (str) {
	    al->filename = strdup(ts_str(str));

	    al->channelid ++;
	}

    } else if (bn_binding_name_pattern_match(ts_str(name), al_format)) {
	/* configure format */
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &str);
	bail_error(err);

	if (str && ts_length(str) == 0) {
	    lc_log_basic(LOG_ERR, "bad or NULL format class/specifier for channel.");
	    bail_error(lc_err_unexpected_case);
	}

	if (al->format) {
	    free(al->format);
	}

	if (str) {
	    al->format = strdup(ts_str(str));
	    accesslog_parse_format((logd_file *) al);
	    al->channelid ++;
	}

    } else if (bn_binding_name_pattern_match(ts_str(name), al_max_filesz)) {
	/* configure max file size */
	uint16 size = 0;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &size);
	bail_error(err);
	al->max_filesize = (int64_t) size *(1024 * 1024);

    } else if (bn_binding_name_pattern_match(ts_str(name), al_url)) {
	/* configure SCP URL */
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
		&str);
	bail_error(err);

	/*Determine that full config was read and
	  set a flag,
	  on setting the flag start the uds log server,
	  This is done to start the uds log server
	  only after the all config is read for that profile
	 */

	if (al->flags & LOGD_FLAG_NEW_PROFILE)
	    al->flags = LOGD_FLAG_PROFILE_READ;


	if (al->remote_url) {
	    free(al->remote_url);
	}

	if (str && ts_length(str)) {
	    al->remote_url = strdup(ts_str(str));
	} else {
	    al->remote_url = NULL;
	}

    } else if (bn_binding_name_pattern_match(ts_str(name), al_pass)) {
	/* configure SCP password */
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
				  &str);
	bail_error(err);

	if (al->remote_pass) {
	    free(al->remote_pass);
	}

	if (str) {
	    /* copy the new value */
	    al->remote_pass = strdup(ts_str(str));
	} else {
	    al->remote_pass = NULL;
	}

    } else
	if (bn_binding_name_pattern_match(ts_str(name), al_time_interval))
	{
	    /* Configure rotation time interval */
	    uint32 size = 0;
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	    bail_error(err);

	    al->rt_interval = size;

	} else if (bn_binding_name_pattern_match(ts_str(name), al_filter_size)) {
	    /* configure filter size restriction */
	    uint32 size = 0;
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &size);
	    bail_error(err);

	    al->r_size = (size);

	} else if (bn_binding_name_pattern_match(ts_str(name), al_filter_resp)) {
	    /* configure filter response code */
	    uint16_t code = 0;

	    err = bn_binding_get_name_parts(binding, &name_parts);
	    bail_error_null(err, name_parts);

	    err = bn_binding_get_str(binding, ba_value, bt_uint16, NULL, &s_code);
	    bail_error(err);

	    if (safe_strcmp(s_code, tstr_array_get_str_quick(name_parts, 8)) == 0) {
		/* This is an add */
		int i;

		/* TODO: Useless binding_get */
		err = bn_binding_get_uint16(binding, ba_value, NULL, &code);
		bail_error(err);
		for(i = 0; i < MAX_RESP_CODE_FILTER; i++) {
		    if(i == 0) {
			if(al->r_code == NULL){
			    complain_error_msg(1, "Memory allocation failed!");
			    break;
			}

			memset(al->r_code , -1, MAX_RESP_CODE_FILTER * sizeof(int32_t));
		    }
		    if (al->r_code[i] == -1) {
			al->r_code[i] = code;
			al->al_rcode_ct ++;
			break;
		    }
		}
	    }
	}
    if(al->flags == LOGD_FLAG_PROFILE_READ) {
	/* Create a directory structure for this profile */
	if ((create_log_dir(profile)) > 0) {
	    accesslog_read_fileid(al);
	}
	accesslog_save_fileid(al);
	al->flags = 0;
    }

bail:
    ts_free(&str);
    tstr_array_free(&name_parts);
    safe_free(s_code);
    return err;
}


int access_log_cfg_handle_delete_change(const bn_binding_array * arr,
				 uint32 idx,
				 bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    tstring *str = NULL;
    uint16 serv_port = 0;
    tbool bool_value = false;
    tbool *rechecked_licenses_p = data;
    char *profile = NULL;
    struct channel *channel = NULL;
    logd_file *al = NULL;
    const char *pname = NULL;

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    if (bn_binding_name_pattern_match
	(ts_str(name), "/nkn/accesslog/config/profile/*")) {
	/* check if we have a channel for this profile */
	err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
				 &profile);
	bail_error(err);

	if (log_profile_get(profile, &al) < 0) {
	    bail_force(lc_err_unexpected_case);
	}

	pthread_mutex_lock(&plf_lock);
	pthread_mutex_lock(&al->lock);
	log_profile_close_channels(al);

	if (al->fp) {
	    fclose(al->fp);
	    al->fp = NULL;
	}
	safe_free(al->profile);
	al->profile = NULL;
	pthread_mutex_unlock(&al->lock);
	pthread_mutex_unlock(&plf_lock);
	plf_free(&al);

	delete_log_dir((const char *) profile);

    } else if (bn_binding_name_pattern_match(ts_str(name), al_filter_resp)) {
	/* configure filter response code */
	uint16_t code = 0;
	char *s_code = NULL;

	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);

	pname = tstr_array_get_str_quick(name_parts, 4);

	if (log_profile_get(pname, &al) < 0) {
	    bail_error_quiet(lc_err_not_found);
	}

	err = bn_binding_get_str(binding, ba_value, bt_uint16, NULL, &s_code);
	bail_error(err);

	if (safe_strcmp(s_code, tstr_array_get_str_quick(name_parts, 8)) == 0) {
	    /* This is an add */
	    int i;

	    /* TODO: Useless binding_get */
	    err = bn_binding_get_uint16(binding, ba_value, NULL, &code);
	    bail_error(err);
	    if(al->r_code){
		for(i = 0; i < MAX_RESP_CODE_FILTER; i++) {
		    if (al->r_code[i] == code) {
			al->r_code[i] = -1;
			al->al_rcode_ct --;
			//printf("removing resp-code: %d\n", code);
			break;
		    }
		}
	    }
	}
    }

bail:
    safe_free(profile);
    tstr_array_free(&name_parts);
    return err;
}


extern int log_create_accesslog_server(const char *name, void *plf);

/* < 0 : errno
 * = 0 : create a directory, new profile
 * > 0 : directory exists
 */
static int create_log_dir(const char *name)
{
    int ret = 0;
    struct stat statb;
    char *sock_name = alloca(strlen("uds-acclog-") + strlen(name) + 8);
    char *p = alloca(strlen(BASE_PATH) + strlen(name) + 8);
    int found = 1;
    logd_file *plf = NULL;

    log_profile_get(name, &plf);

    sprintf(p, "%s/%s", BASE_PATH, name);

    if ((ret = stat(BASE_PATH, &statb)) == -1) {
	if (errno == ENOENT) {
	    mkdir(BASE_PATH, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
	}
	else{
	    complain_error_errno(1,"stat failed");
	    return -errno;
	}
    }

    if ((ret = stat(p, &statb)) == -1) {
	/* directory not found, create it */
	if (errno == ENOENT) {
	    found = 0;
	    mkdir(p, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
	}
	else{
	    complain_error_errno(1,"stat failed");
	    return -errno;
	}
    }

    /* Create a listener for this profile */
    sprintf(sock_name, "uds-acclog-%s", name);
    log_create_accesslog_server(sock_name, plf);

    return found;
}


static int delete_log_dir(const char *name)
{
    int ret = 0;
    char *sock_name = alloca(strlen("uds-acclog-") + strlen(name) + 8);
    char *p = alloca(strlen(BASE_PATH) + strlen(name) + 8);

    if(p){
	sprintf(p, "%s/%s", BASE_PATH, name);
	lf_remove_tree(p);
    }
    if(sock_name){
	sprintf(sock_name, "uds-acclog-%s", name);
	log_delete_accesslog_server(sock_name);
    }
    return 0;
}

void accesslog_file_init(struct channel *chan)
{
    int ret = 0;
    char abs_fname[512];
    char mylink[512];
    const char *symfilename;
    char date_str[256];
    char cmd_str[1024];
    char gtime_str[256];
    logd_file *plf = chan->accesslog;
    time_t now = time(NULL);
    time_t et;
    struct tm etime;
    struct tm ltime;
    struct tm gtime;
    gmtime_r(&now, &gtime);

    aln_interval *pal = NULL;

    if (plf == NULL) {
	return;
    }

    pthread_mutex_lock(&(plf->lock));
    pal = (aln_interval *)malloc(sizeof(aln_interval));
    if( pal == NULL ){
	complain_error_msg(1, "Memory allocation failed!");
	exit(EXIT_FAILURE);
    }
    memset(pal, 0 ,sizeof(aln_interval));
    plf->pai = pal;
    localtime_r(&now, &ltime);
    plf->st_time = (uint64_t) now;

    strftime(plf->hour, 3, "%H", &ltime);

    plf->cur_fileid ++;
    if(plf->cur_fileid >= plf->max_fileid)
	plf->cur_fileid = 0;

    accesslog_save_fileid(plf);

    /* Delete old file */
    snprintf(cmd_str, sizeof(cmd_str), "%s %s/%s/%s_%d_%s", "rm -f",
	    BASE_PATH, plf->profile, plf->filename, plf->cur_fileid, "*");
    system(cmd_str);

    symfilename = plf->filename;

    // modify the end time to include the file_end_min
    if(pal){
	if((pal->use_file_time) == 1){
	    ltime.tm_min = pal->file_end_min;
	    pal->use_file_time = 0;
	}
    }

    if((ltime.tm_min )&&(plf->rt_interval)){
	int round_off = 0;
	round_off  = ltime.tm_min%plf->rt_interval;
	if(round_off){
	    ltime.tm_min = abs(ltime.tm_min - round_off);
	}
    }
    strftime(plf->file_start_time,
	    SIZE_OF_TIMEBUFFER, "%G%m%d%H%M", &ltime);
    strftime(date_str, 256, "%Y_%m_%d_%H_%M_%S", &ltime);
    strftime(gtime_str, 256, "%Y-%m-%d %T", &gtime);
    snprintf(abs_fname, sizeof(abs_fname),
	    "%s/%s/%s_%d_%s",
	    BASE_PATH, plf->profile, plf->filename, plf->cur_fileid, date_str);

    if(plf->rt_interval){
       et = mktime(&ltime); 
       et = et + (60 * plf->rt_interval);
       localtime_r(&et,&etime);
       strftime(plf->file_end_time,
            SIZE_OF_TIMEBUFFER, "%G%m%d%H%M", &etime);
    } else {
        memset(plf->file_end_time,'\0',sizeof(plf->file_end_time));
    }
    if (plf->fullfilename != NULL ) {
	free(plf->fullfilename);
	plf->fullfilename = NULL;
    }

    plf->fullfilename = strdup(abs_fname);

    plf->fp = fopen(abs_fname, "w");
    if (!plf->fp) {
	complain_error_errno(1, "Opening %s", abs_fname);
	exit(EXIT_FAILURE);
    }

    plf->io_buf = (char *) calloc(IOBUF_SIZE, 1);
    setbuffer(plf->fp, plf->io_buf, IOBUF_SIZE); 

    plf->cur_filesize = 0;

    symfilename = plf->filename; // default: "access.log"
    ret = fprintf(plf->fp, "#Version: 1.0\n");
    plf->cur_filesize += ret;

    accesslog_file_print_format(chan);

    ret = fprintf(plf->fp,
	    "#Software: Media Flow Controller v(%s)\n",
	    NKN_BUILD_PROD_RELEASE);
    plf->cur_filesize += ret;

    ret = fprintf(plf->fp, "#Date: %s\n", gtime_str);
    plf->cur_filesize += ret;

    ret = fprintf(plf->fp, "#Remarks: %s (%s)\n", plf->profile, plf->format);
    plf->cur_filesize += ret;



    ret = fprintf(plf->fp, "#Fields: %s\n", plf->w3c_fmt);
    plf->cur_filesize += ret;
    fflush(plf->fp);

    snprintf(mylink, sizeof(mylink), "%s/%s/%s",
	    BASE_PATH, plf->profile, symfilename);
    unlink(mylink);
    symlink(abs_fname, mylink);

    /* HACK/TODO: Maintain backward compat for default profile only
     */
    if (safe_strcmp(plf->profile, "default") == 0) {
	unlink("/var/log/nkn/access.log");
	symlink(abs_fname, "/var/log/nkn/access.log");
    }

    pthread_mutex_unlock(&(plf->lock));
    return;
}

static char *push_format(char *dest, const char *fmt, ...)    __attribute__((format(printf, 1, 0)));
static char *push_format(char *dest, const char *fmt, ...)
{
    va_list ap;
    char *p = dest;
    int l;


    va_start(ap, fmt);
    l = vsprintf(p, fmt, ap);
    va_end(ap);

    return p+l;
}


void accesslog_file_print_format(struct channel *chan)
{
    int j = 0, i = 0;
    char *ch, *p;
    char *thisfmt = 0;
    const char *fmt;
    char tmp[128];
    int len, l;
    char *header;
    char buf[4096];
    char *head = buf;
    logd_file *plf = (chan->accesslog);
    tbool need_space = true;

    //printf("%s\n", chan->config.accesslog->format);

    fmt = plf->format;
    len = strlen(fmt);
    thisfmt = (char *) fmt;
    ch = (char *) fmt;

    /* skip leading space */
    while (*ch == ' ' || *ch == '\t') {
	len--;
	ch++;
    }

    while (*ch) {
	/* Check if this is a % */
	if (*ch == '%') {
	    /* begining of a format */
	    head = push_format(head, " ");
	    ch++;
	    switch(*ch) {
	    case 'b':
		head = push_format(head, "sc-bytes-content");
		break;

	    case 'c':
		head = push_format(head, "x-cache-hit");
		break;

	    case 'd':
		head = push_format(head, "x-origin-fetch-size");
		break;

	    case 'e':
		head = push_format(head, "x-cache-hit-history");
		break;

	    case 'f':
		head = push_format(head, "cs-uri-stem");
		break;

	    case 'g':
		head = push_format(head, "x-obj-compressed");
		break;

	    case 'h':
		head = push_format(head, "c-ip");
		break;

	    case 'm':
		head = push_format(head, "cs-method");
		break;

	    case 'p':
		head = push_format(head, "x-hotness");
		break;

	    case 'q':
		head = push_format(head, "cs-uri-query");
		break;

	    case 'r':
		head = push_format(head, "cs-request");
		break;

	    case 's':
		head = push_format(head, "sc-status");
		break;

	    case 't':
		head = push_format(head, "time");
		break;

	    case 'u':
		head = push_format(head, "user");
		break;

	    case 'v':
		head = push_format(head, "x-server");
		break;

	    case 'w':
		head = push_format(head, "c-exp-obj-served");
		break;

	    case 'y':
		head = push_format(head, "sc-substatus");
		break;

	    case 'z':
		head = push_format(head, "x-connection-type");
		break;

	    case 'A':
		head = push_format(head, "x-request-time");
		break;

	    case 'B':
		head = push_format(head, "x-first-byte-out-time");
		break;

	    case 'D':
		head = push_format(head, "x-time-used-ms");
		break;

	    case 'E':
		head = push_format(head, "time-taken");
		break;

	    case 'F':
		head = push_format(head, "x-last-byte-out-time");
		break;

	    case 'H':
		head = push_format(head, "cs-proto");
		break;

	    case 'I':
		head = push_format(head, "cs-bytes");
		break;

	    case 'L':
		head = push_format(head, "x-latency");
		break;

	    case 'M':
		head = push_format(head, "x-data-len-ms");
		break;

	    case 'N':
		head = push_format(head, "x-namespace");
		break;

	    case 'O':
		head = push_format(head, "sc-bytes");
		break;

	    case 'R':
		head = push_format(head, "x-revalidate-cache");
		break;

	    case 'S':
		head = push_format(head, "s-origin-ips");
		break;

	    case 'V':
		head = push_format(head, "cs-host");
		break;

	    case 'U':
		head = push_format(head, "cs-uri");
		break;

	    case 'X':
		head = push_format(head, "c-ip");
		break;

	    case 'Y':
		head = push_format(head, "s-ip");
		break;

	    case 'Z':
		head = push_format(head, "s-port");
		break;

	    case '{':
		p = ++ch;
		while(*ch++ != '}');
		header = alloca(ch - p + 16);
		strncpy(header, p, ch - p);
		l = (ch - p - 1); /* ch has moved ahead to } */
		header[l] = 0;
		if (*ch == 'i') {
		    if (strcasecmp(header, "host") == 0) {
			head = push_format(head, "cs-host");
		    } else {
			head = push_format(head, "cs(%s)", header);
		    }
		} else if (*ch == 'o') {
		    head = push_format(head, "sc(%s)", header);
		} else if (*ch == 'C') {
		    head = push_format(head, "cs-Cookie(%s)", header);
		}

		break;
	    default:
		/* Unrecognised */
		// push to next '%'
		//head = push_format(head, "%c", *ch);
		break;
	    }
	    ch++;
	} else if (*ch == 0) {
	    break;
	} else {
	    ch++;
	}
    }

    //printf("Header format: %s\n", buf);

    if (plf->w3c_fmt) {
	free(plf->w3c_fmt);
    }

    plf->w3c_fmt = strdup(buf);


    return;
}



void accesslog_file_exit(struct channel *chan)
{
    logd_file *plf = (chan->accesslog);
    pid_t pid;
    FILE *pipefd = NULL;
    char path[1024] = {0};
    char cmd[1024] = {0};
    int ret = 0;
    char *fullfilename = NULL;
    struct tm ltime ;
    char trailer_record[SIZE_OF_TIMEBUFFER] = {0};
    aln_interval *pal = NULL;
    int round_off = 0;
    unsigned long tmp_et = 0;
    char *tmp_filename = NULL;
    struct stat file_status;
    char *token = NULL;
    struct tm mtime;
    struct tm etime ;
    time_t et ;
    uploadInfo *p_uploadInfo = NULL;
    char gz_filename[MAX_FILE_SZ] = { 0 };
    int err = 0;
    char *bname = NULL, *b = NULL;
    char *bname_upldinfo = NULL, *b_upldinfo = NULL;
    char *dname_upldinfo = NULL, *d_upldinfo = NULL;

    if (!plf)
	return;

    pthread_mutex_lock(&(plf->lock));
    memset(&ltime, 0, sizeof(struct tm) );
    memset(&etime, 0, sizeof(struct tm) );

    if(!plf->fullfilename) {
	snprintf(cmd,1024,"/bin/ls %s/%s/%s_%d* 2>/var/log/nkn/.err.txt",
           BASE_PATH, plf->profile, plf->filename, plf->cur_fileid);
        pipefd = popen(cmd, "r");
        if(pipefd == NULL) {
	   pthread_mutex_unlock(&(plf->lock));
	    complain_error_errno(1, "popen failed");
           return;
        }
        fscanf(pipefd,"%s\n",path);
        plf->fullfilename = strdup(path);
        pclose(pipefd);
    }
    fullfilename = alloca(strlen(plf->fullfilename));
    sprintf(fullfilename, "%s", plf->fullfilename);
    tmp_filename = basename(fullfilename);

    /*For a new profile, the old file wont be there for exporting*/
    if(strstr(tmp_filename,plf->filename)== NULL){
       pthread_mutex_unlock(&(plf->lock));
        return;
    }

    if(strcmp(plf->file_start_time,"")==0) {
	int field = 0;
	int filename_length = strlen(plf->filename);
        token = strtok(tmp_filename+filename_length,"_");

        while(token != NULL) {
            /* The 2nd field till 6th field indicates the start timestamp*/
            if((field > 0)&&(field < 6)) {
                strlcat(plf->file_start_time ,token,
                sizeof(plf->file_start_time));
	    }
	    token = strtok(NULL, "_");
	    field++;
	}
    }
    //Fix for PR782738
    if(strcmp(plf->file_end_time, "") == 0){
	if(strcmp(plf->file_start_time, "") != 0) {
	    if(plf->rt_interval){
		strptime(plf->file_start_time, "%Y%m%d%H%M", &ltime );
		et =  mktime(&ltime);
		et = et + (60 * plf->rt_interval);
		localtime_r(&et, &etime);
	    }
	    else {
		time_t now = time(NULL);
		localtime_r(&now ,&etime);
	    }
	    strftime(plf->file_end_time,
		    SIZE_OF_TIMEBUFFER, "%G%m%d%H%M", &etime);
        }

    }
    sscanf(plf->file_end_time,"%lu",&tmp_et);
    if(plf->version_etime == 0){
        plf->version_etime = tmp_et;
        plf->version_number ++;
    }else{
        if(tmp_et > plf->version_etime){
            plf->version_number = 0;
        }else {
            plf->version_number ++;
        }
        plf->version_etime = tmp_et;
    }
    if(stat(plf->fullfilename, &file_status) == 0) {
	//Got the file status
        if(plf->fp == NULL){
            plf->fp = fopen(plf->fullfilename, "a+");
        }
	if(plf->fp != NULL)
	{
	    localtime_r(&file_status.st_mtime, &mtime);
	    strftime (trailer_record, SIZE_OF_TIMEBUFFER, "%G%m%d%H%M%S",
                    &mtime);
            fprintf(plf->fp,"\n#Last Modified: %s",trailer_record);
        }
        else {
            lc_log_basic(LOG_ERR, "[%s] Failed to update trailer in previous file %s", plf->profile, plf->fullfilename);
        }
    } else {
	lc_log_basic(LOG_INFO, "[%s] Failed to get stat of previous file %s", plf->profile, plf->fullfilename);
    }

    if(plf->fp){
	fflush(plf->fp);
	fclose(plf->fp);
    }

    plf->fp = NULL;
    plf->st_time = 0;
    if (plf->io_buf) {
	free(plf->io_buf);
	plf->io_buf = NULL;
    }
    if(plf->pai){
	free(plf->pai);
    }

    p_uploadInfo = malloc(sizeof(struct uploadInfo));
    if(p_uploadInfo == NULL){
	complain_error_msg(1, "Memory allocation failed may not zip the log file!");
	bail_error(1);
    }
    memset(p_uploadInfo, 0, sizeof(struct uploadInfo));
    b = basename(bname = strdup(plf->fullfilename));
    if((plf->profile)&&(plf->fullfilename)&&(plf->filename)){
	p_uploadInfo->profile = strdup(plf->profile);
	snprintf(gz_filename, MAX_FILE_SZ, "%s/%s/%s_%d_%s_%s", BASE_PATH,
		plf->profile, plf->filename, plf->version_number,
		plf->file_start_time, plf->file_end_time);

	p_uploadInfo->fullfilename = strdup(gz_filename);
	p_uploadInfo->filename =strdup(b);
	p_uploadInfo->logtype = plf->type;
    }
    if(plf->remote_url) {
	p_uploadInfo->remote_url = strdup(plf->remote_url);
    }
    if ( plf->remote_pass){
	p_uploadInfo->password = strdup(plf->remote_pass);
    }
    memset(gz_filename, 0, MAX_FILE_SZ);
    b_upldinfo = basename(bname_upldinfo = strdup(p_uploadInfo->fullfilename));
    d_upldinfo = dirname(dname_upldinfo = strdup(p_uploadInfo->fullfilename));
    // Append "." to the file name so we dont end up in loosing the file before we zip as in accesslog_file_init the files may be deleted by unlink
    snprintf(gz_filename, MAX_FILE_SZ, "%s/.%s", d_upldinfo,
	    b_upldinfo);
    err = lf_rename_file(plf->fullfilename, gz_filename);
    bail_error(err);
    pthread_mutex_unlock(&(plf->lock));
    err = post_msg_to_gzip(p_uploadInfo);
    bail_error(err);

bail:
    if(err){
	pthread_mutex_unlock(&(plf->lock));
    }
	safe_free(bname);
        safe_free(dname_upldinfo);
        safe_free(bname_upldinfo);
    return;
}


static int accesslog_read_fileid(logd_file *plf)
{
    FILE *fp = NULL;

    fp = fopen(plf->file_id_name, "r");
    if (fp) {
	fscanf(fp, "fileid=%d\n", &(plf->cur_fileid));
	fscanf(fp, "version_number=%d\n", &(plf->version_number));
	fscanf(fp, "end_time=%lu\n", &(plf->version_etime));
	fclose(fp);
	return 0;
    }
    else {
	complain_error_errno(1, "fopen failed");
	return -1;
    }
}


static int accesslog_save_fileid(logd_file *plf)
{
    FILE *fp = NULL;

    fp = fopen(plf->file_id_name, "w");
    if (fp) {
	fprintf(fp, "fileid=%d\n", plf->cur_fileid);
	fprintf(fp, "version_number=%d\n", plf->version_number);
	fprintf(fp, "end_time=%lu\n", plf->version_etime);
	fclose(fp);
	return 0;
    }
    else {
	complain_error_errno(1, "fopen failed");
	return -1;
    }

}


//global callback for the (upload) action response message
int action_upload_callback(gcl_session *gcls, bn_response **inout_resp,
	void *arg)
{
    uint32 req_id = 0, resp_id = 0;
    uint32 ret_code = 0;
    tstring *ret_msg = NULL;
    int err  = 0;
    uploadInfo *ret_upldInfo = NULL;
    char *bname = NULL, *b = NULL;

    err = bn_response_get_msg_ids(*inout_resp, &req_id, &resp_id);
    lc_log_basic(LOG_INFO, "!!!req_id:%d,resp_id:%d ", req_id, resp_id);
    bail_error(err);
    err = bn_response_get(*inout_resp, &ret_code, &ret_msg, false, NULL, NULL);
    bail_error(err);

    ret_upldInfo = cp_hashtable_get(ht_uploadInfo, &req_id);

    if(ret_upldInfo != NULL){ //found the key val pair !!
	b = basename(bname = strdup(ret_upldInfo->fullfilename));
	if(ret_upldInfo->logtype == TYPE_ACCESSLOG){
	    if(ret_code != 0){
		lc_log_basic(LOG_NOTICE,"%s of %s profile failed to upload "
			"ERR: %s ", b,
			ret_upldInfo->profile,
			ts_str(ret_msg));

		//send an event from here for accesslog
		err = mdc_send_event_with_bindings_str_va
		    (mgmt_mcc, NULL, NULL, "/nkn/nknlogd/events/logexportfailed", 2,
		     "/nkn/accesslog/config/profile/*", bt_string,
		     ret_upldInfo->profile,
		     "/nkn/monitor/filename", bt_string, b);
		bail_error(err);
	    }
	    else {
		lc_log_basic(LOG_INFO, "%s of %s profile successfuly uploaded ",
			     b, ret_upldInfo->profile );
	    }
	}
	else {
	    if(ret_code != 0){
		lc_log_basic(LOG_NOTICE,"%s failed to upload "
			"ERR: %s ", b,
			ts_str(ret_msg));
	    }
	    //Incase the nodes are added raise the trap for other logs as well
	}
	/*
	   Since we raised a trap we no longer need the upload info
	   delete the req id and the corresponding info from the hash table
	 */
	cp_hashtable_remove(ht_uploadInfo, &req_id);
    }
bail:
    ts_free(&ret_msg);
    safe_free(bname);
    return err;
}

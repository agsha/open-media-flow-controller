/*
  COPYRIGHT: NOKEENA NETWORKS
  AUTHOR: Vikram Venkataraghavan
*/
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/uio.h>
#include <glib.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <atomic_ops.h>
#include "nkn_nfs_api.h"
#include "nkn_nfs.h"
#include "nkn_mediamgr_api.h"
#include "nkn_defs.h"
#include "nkn_trace.h"
#include "http_header.h"
#include "fqueue.h"
#include "nkn_debug.h"
#include "nkn_namespace.h"
#include "nkn_am_api.h"
#include "queue.h"
#include "aio.h"
#include "nkn_cod.h"
#include "nkn_assert.h"
#include "nkn_nfs_tunnel.h"

#define NFS_YAHOO_LOCAL_CFG_FILE "/nkn/nfs/nfsyahoolocalcfg.txt"
#define NKN_NFS_MAX_CACHES 1
#define NO_FILE_STR "no_file_name_assigned\0"

static pthread_cond_t nfs_promote_thr_cond;
static pthread_mutex_t nfs_promote_mutex;
static pthread_mutex_t nkn_nfs_stat_mutex;
//static NFS_mount_obj *glob_nfs_server_map_obj;

GThread *nfs_rem_file_cfg_thread;
GAsyncQueue *nfs_rem_file_cfg_q;
GThread *nfs_yahoo_cfg_thread;
GAsyncQueue *nfs_yahoo_cfg_q;
int nfs_yahoo_thread_exit = 0;
int nfs_yahoo_req_complete = 0;
GThread *nfs_async_thread;
GAsyncQueue *nfs_async_q;

int nfs_yahoo_poll_interval = 0;
uint32_t nfs_async_q_count;
GList *nfs_prefix_map_list = NULL;
GList *nfs_cfg_list = NULL;
int nkn_nfs_num_get_threads = 1;
int nkn_nfs_get_thr_limit = 50;
int nkn_nfs_stat_thr_limit = 50;
int nkn_nfs_max_server_map_entries= 500;
uint64_t nkn_nfs_outstanding_promote_thr = 10000;
uint64_t glob_nfs_stat_err = 0;
uint64_t glob_nfs_stat_open_err = 0;
uint64_t glob_nfs_stat_cnt = 0;
uint64_t glob_nfs_stat_cnt_started = 0;
uint64_t glob_nfs_stat_cnt_ended = 0;
uint64_t glob_nfs_stat_not_found = 0;
uint64_t glob_nfs_stat_success = 0;
uint64_t glob_nfs_get_cnt = 0;
uint64_t glob_tot_read_from_nfs = 0;
uint64_t glob_nfs_get_err = 0;
uint64_t glob_nfs_get_success = 0;
uint64_t glob_nfs_get_promote_error_cnt = 0;
uint64_t glob_nfs_get_promote_not_worthy = 0;
uint64_t glob_nfs_get_promote_alloc_failed = 0;
uint64_t glob_nfs_get_promote_ptype_err = 0;
uint64_t glob_nfs_uri_err;
uint64_t glob_nfs_put_cnt;
uint64_t glob_nfs_put_err;
uint64_t glob_nfs_new_mounts = 0;
uint64_t glob_nfs_try_new_mount = 0;
uint64_t glob_nfs_db_obj_created = 0;
uint64_t glob_nfs_db_obj_deleted = 0;
uint64_t glob_nfs_db_old_obj_cleanup = 0;
uint64_t glob_nfs_num_files_open = 0;
uint64_t glob_nfs_num_files_closed = 0;
uint64_t glob_nfs_num_open_err = 0;
uint64_t glob_nfs_num_seek_err = 0;
uint64_t glob_nfs_num_read_err = 0;
uint64_t glob_nfs_num_move_files = 0;
uint64_t glob_nfs_num_other_files = 0;
AO_t glob_nfs_files_promote_start = 0;
AO_t glob_nfs_files_promote_end = 0;
uint64_t glob_nfs_promote_err = 0;
uint64_t glob_nfs_server_stat_thr_hit = 0;
uint64_t glob_nfs_server_get_thr_hit = 0;
uint64_t glob_nfs_stat_server_busy = 0;
uint64_t glob_nfs_get_server_busy = 0;
uint64_t glob_nfs_aio_server_busy = 0;
uint64_t glob_nfs_aio_get_err = 0;
uint64_t glob_nfs_aio_sessions_returned_success = 0;
uint64_t glob_nfs_aio_get_cnt = 0;
uint64_t glob_nfs_aio_ave_server_latency_too_high = 0;
uint64_t glob_nfs_aio_ave_server_latency_obj_not_found = 0;
uint64_t glob_nfs_aio_ave_server_latency_ok = 0;
uint64_t glob_nfs_aio_bad_fd_returned = 0;
uint64_t glob_nfs_tot_validate_called = 0;
uint64_t glob_nfs_promote_called = 0;
uint64_t glob_nfs_promote_thread_called = 0;
uint64_t glob_nfs_promote_thread_too_many_promotes = 0;
uint64_t glob_nfs_promote_queue_num_obj = 0;
uint64_t glob_nfs_get_thread_called = 0;
uint64_t glob_nfs_get_thread_err = 0;
uint64_t glob_nfs_get_thread_push = 0;
uint64_t glob_nfs_get_thread_pop = 0;
uint64_t glob_nfs_cod_err = 0;
uint64_t glob_nfs_get_cod_err = 0;
uint64_t glob_nfs_get_cod_err1 = 0;
uint64_t glob_nfs_promote_complete_cod_err = 0;

int nkn_max_server_latency = 600;
int nkn_nfs_max_deadlines_missed = 1;

STAILQ_HEAD(nkn_nfs_pmt_q, NFS_promote_cfg_s) nkn_nfs_promote_q;

/* Config delivered by fetching a remote file */
static int s_nfs_oomgr_request(char *in_uri, char *in_file_str, uint64_t token);
static void s_nfs_yahoo_cfg_thread_hdl(void);
//static void s_nfs_stop_yahoo_config(void);
static int s_nfs_start_yahoo_config(char *in_uri, int in_polltime, uint64_t token);

//static int s_nfs_read_config_file(NFS_mount_obj *pobj, char *in_file);
//static int s_nfs_parse_yahoo_config_file(NFS_mount_obj *pobj,
//					char *file, const char *nfsconfigfile);

/* Config through cli */
static NFS_mount_obj* s_return_obj_from_localdir(NFS_mount_obj *in_pobj,
						 char *localdir);
static NFS_mount_obj* s_return_obj_from_token(uint64_t token);
static void s_nfs_cleanup_obj(NFS_mount_obj *pobj);
static int s_nfs_translate_ns_token(namespace_token_t *ns_token,
				    NFS_mount_obj *pobj, char *uri, int trace);

static int s_nfs_check_ave_server_latency(MM_get_resp_t *in_ptr_resp);
static int s_nfs_set_ave_server_latency(MM_get_resp_t *in_ptr_resp);
static int NFS_validate(MM_validate_t *pvld);

/*
 * Compute a version ID corresponding to an NFS file.  We use the inode number,
 * device ID and creation time to uniquely identify the file.
 */
static void
s_compute_version_id(struct stat *sb, nkn_objv_t *oid)
{
	oid->objv_l[0] = sb->st_ino;
	oid->objv_l[1] = (sb->st_dev << 32) | sb->st_ctime;
}

static int s_create_nfs_dir(char *container_path, int isdir)
{
    char *end_slash, *cptr;
    int ret;

    if (isdir) {
        end_slash = &container_path[strlen(container_path)];
    } else {
        end_slash = strrchr(container_path, '/');
    }
    cptr = container_path;
    while (cptr < end_slash) {
        cptr = strchr(cptr+1, '/');
        if (cptr) {
            *cptr = '\0';
        } else if (isdir) {
            cptr = end_slash;
        } else {
            break;
        }
        ret = mkdir(container_path, 0755);
        if (ret != 0 && errno != EEXIST) {
            DBG_LOG(WARNING, MOD_NFS, "mkdir error (%s): %d\n",
		    container_path, errno);
            return -errno;
        }
        if (!isdir || cptr != end_slash)
            *cptr = '/';
    }
    /* Restore slash, so the container name is complete again */
    if (!isdir)
        *end_slash = '/';
    return 0;
}

static int
s_nfs_delete_mount(NFS_mount_obj *pobj)
{
    int ret = -1;
    int i;

    if(pobj->flag == NFS_CONFIG_FILE) {
	for(i = 0; i < pobj->num_mounts; i++) {
	    if(pobj->type == ORIGIN_TYPE_NFS) {
		ret = umount2(pobj->remp->localdir[i], MNT_FORCE);
		if(ret < 0) {
		    DBG_LOG(MSG, MOD_NFS, "Delete config: CONFIG_FILE: "
			    "Unable to unmount %s",
			    pobj->remp->localdir[i]);
		}
	    } else {
		ret = remove(pobj->remp->localdir[i]);
		if(ret < 0) {
		    DBG_LOG(SEVERE, MOD_NFS, "Delete config: "
			    "remove local directory "
			    "failed: %s, errno=%d", pobj->remp->localdir[i], errno);
		}
	    }
	}
    } else {
	if(pobj->type == ORIGIN_TYPE_NFS) {
	    ret = umount2(pobj->localdir, MNT_FORCE);
	    if(ret < 0) {
		DBG_LOG(MSG, MOD_NFS, "Delete config: Unmount NFS directory "
			"failed: %s", pobj->localdir);
		return -1;
	    }
	} else {
	    ret = remove(pobj->localdir);
	    if(ret < 0) {
		DBG_LOG(SEVERE, MOD_NFS, "Delete config: "
			"remove local directory "
			"failed: %s, errno=%d", pobj->localdir, errno);
	    }
	}
    }
    return 0;
}


/* Need to call this function with the nkn_nfs_stat_mutex obtained */
static void
s_nfs_cleanup_obj(NFS_mount_obj *pobj)
{
    if(!pobj) {
	return;
    }

    nfs_prefix_map_list = g_list_remove(nfs_prefix_map_list, pobj);
    if(pobj->ns_uid) {
        free(pobj->ns_uid);
    }
    free(pobj);
    pobj = NULL;
}

/* Creates an NFS mount directory and does the mount as well.
   NOTE: this is a blocking call; do NOT call this with the mutex held.*/
static int
s_nfs_create_mount(char *localdir, char *remotehost, char *remotedir, int trace)
{
    char            mountcmd[PATH_MAX + MAX_URI_SIZE + 256];
    char            remotepath[PATH_MAX + MAX_URI_SIZE + 256];
    struct stat     sb;
    int             ret = -1;
    FILE	    *fp;
    char	    buf[256];


    if(!strcasecmp(remotehost, "localhost") ||
	!strcasecmp(remotehost, "127.0.0.1")) {
	/* Strip of the "/" */
	localdir[strlen(localdir) - 1] = '\0';

	if (!stat(localdir, &sb)) {
	    ret = remove(localdir);
	    if(ret < 0) {
		DBG_LOG(SEVERE, MOD_NFS, "remove local directory "
			"failed: %s, errno=%d", localdir, errno);
		goto fn_ret;
	    }
	}

	memset(remotepath, 0, sizeof(remotepath));
	snprintf(remotepath, sizeof(remotepath)-1, "/nkn/pers/%s/", remotedir);

	if (stat(remotepath, &sb)) {
	    goto fn_ret;
	}

	memset(mountcmd, 0, sizeof(mountcmd));
	snprintf(mountcmd, sizeof(mountcmd)-1,
	     "ln -sf /nkn/pers/%s/ %s 2>&1", remotedir, localdir);
	DBG_LOG(MSG, MOD_NFS, "\n NFS Origin: Mounting:%s", mountcmd);
	fp = popen(mountcmd, "r");
	if(fp == NULL) {
	    DBG_LOG(SEVERE, MOD_NFS, "NFS Mount Failed. errno=%d", errno);
	    if(trace == 1) {
		DBG_TRACE("Mount failed for mount command: %s", mountcmd);
	    }
	    ret = -1;
	} else {
	    buf[0] = 0;
	    /* Read the output from the pipe */
	    fread(buf, 256, 1, fp);
	    if(buf[0] != 0) {
		DBG_LOG(WARNING, MOD_NFS, "local drive link failed.");
		DBG_TRACE("link failed for command: %s", mountcmd);
		ret = -1;
	    } else
		ret = 0;
	    pclose(fp);
	}
    } else {
	/* Create the dir if it is not there already */
//	if (stat(localdir, &sb) < 0) {
	    DBG_LOG(MSG, MOD_NFS, "\n create nfs dir: %s", localdir);
	    DBG_ERR(MSG, "\n create nfs dir: %s", localdir);
	    ret = s_create_nfs_dir(localdir, 1);
	    if(ret < 0) {
		if(ret == -ESTALE) {
		    DBG_LOG(MSG, MOD_NFS, "Found stale NFS File handle: %s",
			    localdir);
		    DBG_ERR(MSG, "Found stale NFS File handle: %s",  localdir);
		    ret = umount2(localdir, MNT_FORCE);
		    ret = umount2(localdir, MNT_FORCE);
		    if(ret < 0) {
			DBG_LOG(SEVERE, MOD_NFS, "\n Delete config: "
				"Unmount NFS directory "
				"failed: %s, errno=%d", localdir, errno);
			DBG_ERR(SEVERE, "\n Delete config: Unmount NFS directory "
				"failed: %s, errno=%d", localdir, errno);
			if(trace == 1) {
			    DBG_TRACE("Unmount problem: Stale NFS handle %s",
				    localdir);
			}
		    }
		}
		DBG_LOG(SEVERE, MOD_NFS, "%s", "NFS Make directory Failed....");
		DBG_ERR(SEVERE, "%s", "NFS Make directory Failed....");
		ret = -1;
	    }
//	}

	glob_nfs_try_new_mount ++;
	memset(mountcmd, 0, sizeof(mountcmd));
	snprintf(mountcmd, sizeof(mountcmd)-1,
	     "mount -t nfs -n -o ro,soft,timeo=1,retry=0,retrans=1 %s:/%s %s "
	     "2>&1", remotehost, remotedir, localdir);

	DBG_LOG(MSG, MOD_NFS, "\n NFS Origin: Mounting:%s", mountcmd);
	fp = popen(mountcmd, "r");
	if(fp == NULL) {
	    DBG_LOG(SEVERE, MOD_NFS, "NFS Mount Failed. errno=%d", errno);
	    if(trace == 1) {
		DBG_TRACE("Mount failed for mount command: %s", mountcmd);
	    }
	    ret = -1;
	} else {
	    buf[0] = 0;
	    /* Read the output from the pipe */
	    fread(buf, 256, 1, fp);
	    if(strstr(buf, "already mounted") || buf[0] == 0) {
		DBG_LOG(WARNING, MOD_NFS, "NFS Mount Success.");
		ret = 0;
	    } else {
		DBG_LOG(SEVERE, MOD_NFS, "NFS Mount Failed.");
		DBG_TRACE("Mount failed for mount command: %s", mountcmd);
		ret = -1;
	    }
	    pclose(fp);
	}
    }
fn_ret:
    return ret;
}

/* This function returns 0 if an existing NFS mount object has been modified: such
   as origin server, nameserver, etc. Returns -1 if no change. */
static int
s_nfs_object_modified(NFS_mount_obj *pobj, namespace_token_t ns, char *uri, int trace)
{
    int ret = -1;
    NFS_mount_obj mobj;

    ret = s_nfs_translate_ns_token(&ns, &mobj, uri, trace);
    if(mobj.ns_uid) {
	free(mobj.ns_uid);
    }

    if(ret < 0) {
	return 0;
    }

    if(strncmp(mobj.remotehost, pobj->remotehost, strlen(mobj.remotehost)) != 0) {
	/* Strings are same MAX size = PATH_MAX. So, safe copy. */
	strncpy(pobj->remotehost, mobj.remotehost, strlen(mobj.remotehost));
	return -1;
    }
    if(strncmp(mobj.remotedir, pobj->remotedir, strlen(mobj.remotedir)) != 0) {
	strncpy(pobj->remotedir, mobj.remotedir, strlen(mobj.remotedir));
	return -1;
    }
    return 0;
}

/* This function updates the object with any namespace changes.
   For now, we take care of only origin server.
*/
static int
s_nfs_update_obj(NFS_mount_obj *pobj, namespace_token_t ns, char *uri, int trace)
{
    int             ret = -1;

    /* Check if the object is changed or not */
    ret = s_nfs_object_modified(pobj, ns, uri, trace);
    if(ret == 0) {
	/* Object has not changed */
	return 0;
    }

    /* Object has changed. Unmount so that calling function will remount*/
    ret = umount2(pobj->localdir, MNT_FORCE);
    if(ret < 0) {
	DBG_LOG(MSG, MOD_NFS, "Delete config: "
		"Unmount NFS directory "
		"failed: %s, errno=%d", pobj->localdir, errno);

	if(trace == 1) {
	    DBG_TRACE("Could not unmount old mount configuration. Please "
		      "check whether someone is accessing this directory: %s",
		      pobj->localdir);
	}
	return -1;
    }
    glob_nfs_db_old_obj_cleanup ++;

    return -1;
}

/* Create the NFS object and fill in the fields. Lock must be held
   across this function */
static int
s_nfs_create_obj(NFS_mount_obj **in_pobj, namespace_token_t ns, char *uri, int trace)
{
    NFS_mount_obj   *ret_obj = NULL;
    NFS_mount_obj   *pobj = *in_pobj;
    int             ret = -1;
    uint64_t        token = ns.u.token;

    pobj = (NFS_mount_obj *) nkn_calloc_type (1, sizeof(NFS_mount_obj),
                                              mod_nfs_mount_obj);
    if(!pobj) {
	return -1;
    }
    pobj->mounted = 0;
    pobj->average_server_latency = 0;
    pobj->num_latency_deadline_missed = 0;
    pobj->last_latency = 0;

    /* Namespace token is a unique id. We use it here to id the
       mount object. It will be part of all stat requests. */
    pobj->token = token;

    snprintf(pobj->mntdir, sizeof(pobj->mntdir)-1, "/nkn/nfs");
    pobj->mntdir[sizeof(pobj->mntdir)-1] = '\0';

    /* Translate the namespace token to configuration and put relevant
       stuff into the object. */
    ret = s_nfs_translate_ns_token(&ns, pobj, uri, trace);
    if(ret < 0) {
	if(pobj) {
	    if(pobj->ns_uid) {
		free(pobj->ns_uid);
	    }
	    free(pobj);
	}
	return -1;
    }

    if(strcasecmp(pobj->remotehost, "localhost") &&
	    strcasecmp(pobj->remotehost, "127.0.0.1"))
	pobj->type = ORIGIN_TYPE_NFS;
    else
	pobj->type = ORIGIN_TYPE_LOCAL;

    snprintf(pobj->localdir, sizeof(pobj->localdir)-1, "%s%s%s",
	     pobj->mntdir, pobj->ns_uid, pobj->prefix);
    pobj->localdir[sizeof(pobj->localdir)-1] = '\0';

    if(ret == 2) {
	/* Server map object. Don't add this into the db.*/
	if(pobj) {
	    if(pobj->ns_uid) {
		free(pobj->ns_uid);
	    }
	    free(pobj);
	}
	return 2;
    }

    /* If an object is already mounted on this dir, unmount it*/
    ret_obj = s_return_obj_from_localdir(pobj, pobj->localdir);
    if(ret_obj) {
	if(pobj->type == ORIGIN_TYPE_NFS) {
	    ret = umount2(ret_obj->localdir, MNT_FORCE);
	    if(ret < 0) {
		DBG_LOG(SEVERE, MOD_NFS, "Delete config: "
			"Unmount NFS directory "
			"failed: %s, errno=%d", ret_obj->localdir, errno);
		DBG_ERR(SEVERE, "Delete config: Unmount NFS directory "
			"failed: %s, errno=%d", ret_obj->localdir, errno);
	    }
	} else {
	    ret = remove(ret_obj->localdir);
	    if(ret < 0) {
		DBG_LOG(SEVERE, MOD_NFS, "Delete config: "
			"remove local directory "
			"failed: %s, errno=%d", ret_obj->localdir, errno);
	    }
	}
	s_nfs_cleanup_obj(ret_obj);
    }

    *in_pobj = pobj;
    /* Update the database */
    nfs_prefix_map_list =
	g_list_prepend(nfs_prefix_map_list, pobj);

    return 0;
}

void
NFS_config_delete(namespace_token_t *del_tok_list, int num_del)
{
    int i;
    NFS_mount_obj   *pobj = NULL;

    pthread_mutex_lock(&nkn_nfs_stat_mutex);
    for(i=0; i < num_del; i++) {
	pobj = s_return_obj_from_token(del_tok_list[i].u.token);
	if(!pobj)
	    continue;
	s_nfs_delete_mount(pobj);
	glob_nfs_db_obj_deleted ++;
    }
    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
    return;
}

static nkn_nfs_cfg_file_t *
s_return_cfg_from_token(uint64_t in_token)
{
    GList 	         *plist = nfs_cfg_list;
    nkn_nfs_cfg_file_t   *pobj = NULL;

    while(plist) {
	pobj = (nkn_nfs_cfg_file_t *) plist->data;
	if(pobj->token == in_token) {
	    return pobj;
	}
	plist = plist->next;
    }
    return NULL;
}


static int
s_nfs_check_server_map(nkn_nfs_cfg_file_t *in_pobj)
{
    nkn_nfs_cfg_file_t *db_pobj = NULL;

    db_pobj = s_return_cfg_from_token(in_pobj->token);
    if(!db_pobj) {
	nfs_cfg_list =
	    g_list_prepend(nfs_prefix_map_list, in_pobj);
	return 0;
    }

    /* TODO: Need to see if anything changed in this server map*/
    return -1;
}

void
NFS_configure(namespace_token_t ns_token, namespace_config_t *cfg)
{
    int             ret = -1;
    nkn_nfs_cfg_file_t *cfgp = NULL;
    char            message[64];
    GError          *err = NULL ;
    NFS_mount_obj   *pobj = NULL;

    if(!cfg) {
	return;
    }

    if(cfg->http_config->origin.u.nfs.server_map) {
	if(!nfs_yahoo_cfg_q && !nfs_yahoo_cfg_thread) {
	    nfs_yahoo_cfg_q = g_async_queue_new();
	    if(nfs_yahoo_cfg_q == NULL) {
		DBG_LOG(SEVERE, MOD_NFS, "\n%s(): Async Queue did not get "
			"inited", __FUNCTION__);
		DBG_ERR(SEVERE, "\n%s(): Async Queue did not get "
			"inited", __FUNCTION__);
		return;
	    }

	    nfs_yahoo_cfg_thread =
		g_thread_create_full (
				      (GThreadFunc) s_nfs_yahoo_cfg_thread_hdl,
				      (gpointer)message, (128*1024),
				      TRUE, FALSE, G_THREAD_PRIORITY_NORMAL,
				      &err);

	    if(nfs_yahoo_cfg_thread == NULL) {
		g_error_free(err);
		return;
	    }
	}

	/* File to configure NFS connections is available */
	cfgp = (nkn_nfs_cfg_file_t *) nkn_calloc_type (1, 
                                      sizeof(nkn_nfs_cfg_file_t),
                                      mod_nfs_server_map_cfg_file);
	if(!cfgp) {
	    return;
	}

	if(strlen(cfg->http_config->origin.u.nfs.server_map->file_url)
	   >= MAX_URI_SIZE) {
	    return;
	}

	strncpy(cfgp->rem_uri,
		cfg->http_config->origin.u.nfs.server_map->file_url,
		strlen(cfg->http_config->origin.u.nfs.server_map->file_url));

	cfgp->rem_uri[strlen(cfg->http_config->origin.u.nfs.server_map->file_url)] = '\0';

	cfgp->poll_interval =
	    cfg->http_config->origin.u.nfs.server_map->refresh;

	cfgp->token = ns_token.u.token;
	ret = s_nfs_check_server_map(cfgp);
	if(ret < 0) {
	    free (cfgp);
	    return;
	}

	pobj = (NFS_mount_obj *) nkn_calloc_type (1, sizeof(NFS_mount_obj),
                                                  mod_nfs_mount_obj);
	if(!pobj) {
	    return;
	}
	pobj->mounted = 0;
	pobj->cfgmap = cfgp;
	pobj->token = ns_token.u.token;
	pobj->flag = NFS_CONFIG_FILE;
	pobj->ns_uid = nkn_strdup_type(cfg->ns_uid, mod_nfs_strdup1);

	/* Update the database */
	nfs_prefix_map_list =
	    g_list_prepend(nfs_prefix_map_list, pobj);

	ret = s_nfs_start_yahoo_config(cfgp->rem_uri,
				       cfgp->poll_interval, pobj->token);
    }
    return;
}

void *
nfs_promote_thread_hdl(void *dummy __attribute((unused)))
{
    NFS_promote_cfg_t *cfg;
    struct timespec abstime;
    int    initial = 0;
    int    ret = -1;

    abstime.tv_sec = 60;
    abstime.tv_nsec = 0;
    glob_nfs_promote_thread_called = 1;

    prctl(PR_SET_NAME, "nvsd-nfs-promote", 0, 0, 0);

    while(1) {
	pthread_mutex_lock(&nfs_promote_mutex);

	if((AO_load(&glob_nfs_files_promote_start) - AO_load(&glob_nfs_files_promote_end)) >
	   100) {
	    glob_nfs_promote_thread_too_many_promotes ++;
	    goto too_many_promotes;
	}
	glob_nfs_promote_thread_called ++;
        cfg = NULL;
	cfg = STAILQ_FIRST(&nkn_nfs_promote_q);

	if(cfg) {
	    ret = MM_promote_uri(cfg->uri, NFSMgr_provider, cfg->dst, 0, NULL, nkn_cur_ts);
	    if(ret < 0) {
		goto too_many_promotes;
	    }
	}

	if(cfg) {
	    STAILQ_REMOVE_HEAD(&nkn_nfs_promote_q, entries);
	    glob_nfs_promote_queue_num_obj --;
	} else {
            goto wait;
        }

	initial = 1;

	AO_fetch_and_add1(&glob_nfs_files_promote_start);

	if(cfg) {
	    if(cfg->uri) {
		free(cfg->uri);
	    }
	    free(cfg);
	    cfg = NULL;
	}
        glob_nfs_promote_called ++;

	if(cfg) {
	    if(cfg->uri) {
		free(cfg->uri);
	    }
	    free(cfg);
	    cfg = NULL;
	}
	pthread_mutex_unlock(&nfs_promote_mutex);
	continue;

    too_many_promotes:
	if(initial == 0) {
	    pthread_mutex_unlock(&nfs_promote_mutex);
	    sleep(1);
	    continue;
	} else {
	    //pthread_cond_wait(&nfs_promote_thr_cond, &nfs_promote_mutex);
	    pthread_mutex_unlock(&nfs_promote_mutex);
	    sleep(1);
	    continue;
	}

    wait:
	if(cfg) {
	    if(cfg->uri) {
		free(cfg->uri);
	    }
	    free(cfg);
	    cfg = NULL;
	}

	if(initial == 0) {
	    pthread_mutex_unlock(&nfs_promote_mutex);
	    sleep(1);
	    continue;
	} else {
	    pthread_cond_wait(&nfs_promote_thr_cond, &nfs_promote_mutex);
	    pthread_mutex_unlock(&nfs_promote_mutex);
	}
    }
    return NULL;
}

int
NFS_init(void)
{
    int 		ret_val = -1;
    char 		dirname[128] = "/nkn/nfs\0";
    int 		ret = -1;
    struct aioinit init;

    //s_umount_dirs(dirname);
    ret_val = s_create_nfs_dir(dirname, 1);
    if(ret_val < 0) {
	DBG_LOG(SEVERE, MOD_NFS, "Cannot make NFS directory: %s",
		dirname);
	DBG_ERR(SEVERE, "Cannot make NFS directory: %s", dirname);
	return -1;
    }

    init.aio_threads = 1000;
    init.aio_num = MAX_LIST;
    init.aio_idle_time = 28800;
    aio_init((const struct aioinit *)&init);

    MM_register_provider(NFSMgr_provider, 0,
			 NFS_put, NFS_stat, NFS_get, NFS_delete, NFS_shutdown,
			 NFS_discover, NFS_evict, NFS_provider_stat,
			 NFS_validate, NULL, NFS_promote_complete,
			 1,/* # of PUT thrds */
			 1,/* # of GET thrds */
			 5000, 0, MM_THREAD_ACTION_IMMEDIATE);

    nfs_async_q_count = 0;

    /* We need to name threads but this was causing problems with PM
       deleting core files because it did not recognize the name. Taking out
       this change for now. */
    //prctl(PR_SET_NAME, "nvsd-nfs", 0, 0, 0);

    ret = pthread_mutex_init(&nkn_nfs_stat_mutex, NULL);
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_NFS,"NFS Stat Mutex not created. "
		"Severe NFS failure");
        DBG_ERR(SEVERE, "NFS Stat Mutex not created. "
		"Severe NFS failure");
        return -1;
    }

    nfs_rem_file_cfg_q = g_async_queue_new();
    if(nfs_rem_file_cfg_q == NULL) {
        DBG_LOG(SEVERE, MOD_NFS, "Remote file async queue did not get inited");
        DBG_ERR(SEVERE, "Remote file async queue did not get inited");
	return -1;
    }
    nfs_async_q_count = 0;

    nfs_gasync_aiocb_pend_q = g_async_queue_new();
    if(nfs_gasync_aiocb_pend_q == NULL) {
        DBG_LOG(SEVERE, MOD_NFS, "Async IO queue  did not get inited");
        DBG_ERR(SEVERE, "Async IO queue  did not get inited");
	return -1;
    }

    nfs_gasync_aiocb_comp_q = g_async_queue_new();
    if(nfs_gasync_aiocb_comp_q == NULL) {
        DBG_LOG(SEVERE, MOD_NFS, "Async IO queue  did not get inited");
        DBG_ERR(SEVERE, "Async IO queue  did not get inited");
	return -1;
    }

    if ((ret = pthread_create(&nfs_aio_thread_obj, NULL,
			      nfs_async_io_suspend_thread_hdl_2, NULL))) {
        return -1;
    }

    if ((ret = pthread_create(&nfs_aio_completion_thread_obj, NULL,
			      nfs_async_io_completion_thread_hdl, NULL))) {
        return -1;
    }

    /* Promote thread stuff */
    if ((ret = pthread_create(&nfs_promote_thread_obj, NULL,
			      nfs_promote_thread_hdl, NULL))) {
        return -1;
    }
    ret = pthread_mutex_init(&nfs_promote_mutex, NULL);
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_NFS,"NFS Promote Mutex not created. "
		"Severe NFS failure");
        DBG_ERR(SEVERE, "NFS Promote Mutex not created. Severe NFS failure");
        return -1;
    }
    /* Dont use this cond in a pthread_cond_timedwait
     * Initialize the cond variable with CLOCK_MONOTNOIC
     * before using it.
     */
    pthread_cond_init(&nfs_promote_thr_cond, NULL);
    STAILQ_INIT(&nkn_nfs_promote_q);

    /* Tester functions */
    //test_nfs_mount();
    //test_nfs_config_file();
    return 0;
}	/* NFS_init */

/*
  This function extracts the postfix from the uri, i.e. it extracts
  everything after the prefix from the given uri.
*/
static char *
s_nfs_extract_postfix(char *uri, char *prefix)
{
    char *p_uri = (char *) uri;
    char *p_pre = (char *) prefix;
    int  exact_match = 1;

    while(*p_uri && *p_pre) {
        if (*(p_uri++) != *(p_pre++)) {
            exact_match = 0;
            break;
        }
    }

    if(exact_match) {
        return p_uri;
    }
    else {
        return NULL;
    }
}

/*
  This function returns an object which maps a uri prefix to a
  mount point.
*/
static NFS_mount_obj* s_return_obj_from_uri(namespace_token_t ns_token,
                                            char *in_uri, char **tmpp)
{
    GList 	    *plist = nfs_prefix_map_list;
    NFS_mount_obj   *pobj;
    int             urilen = 0;

    /* Remove the beginning slash: '\'*/
    //sprintf(tmp, "%s", &in_uri[1]);

    *tmpp = ns_uri2uri(ns_token, in_uri, &urilen);
    if(urilen <= 0) {
        return NULL;
    }

    while(plist) {
	pobj = (NFS_mount_obj *) plist->data;
	if(pobj->token == ns_token.u.token) {
	    return pobj;
	}
	plist = plist->next;
    }
    return NULL;
}

/*
  This function returns an object which maps a uri prefix to a
  mount point.
*/
static NFS_mount_obj* s_return_obj_from_token(uint64_t in_token)
{
    GList 	    *plist = nfs_prefix_map_list;
    NFS_mount_obj   *pobj;

    while(plist) {
	pobj = (NFS_mount_obj *) plist->data;
	if(pobj->token == in_token) {
	    return pobj;
	}
	plist = plist->next;
    }
    return NULL;
}

static NFS_mount_obj* s_return_obj_from_localdir(NFS_mount_obj *in_pobj,
						 char *in_localdir)
{
    GList 	    *plist = nfs_prefix_map_list;
    NFS_mount_obj   *pobj;

    while(plist) {
	pobj = (NFS_mount_obj *) plist->data;
	if((in_pobj != pobj) && strcmp(pobj->localdir, in_localdir) == 0) {
	    return pobj;
	}
	plist = plist->next;
    }
    return NULL;
}


int
NFS_put(struct MM_put_data *map __attribute((unused)),
        uint32_t dummy __attribute((unused)))
{
    return -1;
}

/* This function translates the ns_token in the stat or get
   call to the namespace configuration and stores it in the
   NFS_mount_obj_t object passed in.*/
static int
s_nfs_translate_ns_token(namespace_token_t *ns_token,
			 NFS_mount_obj *pobj, char *uri, int trace)
{
    const namespace_config_t *cfg = NULL;
    int   entries = 0;
    int   i;
    uint32_t   len;
    char  *token;
    char  *saveptr = NULL;
    char *p;
    int nsuid_len;
    char  hostname[PATH_MAX+MAX_URI_SIZE];
#define NFS_CONFIG(cfg) cfg->http_config->origin.u.nfs

    cfg = namespace_to_config(*ns_token);
    if(!cfg) {
	if(trace == 1) {
	    DBG_TRACE("Namespace configuration does not exist");
	}
        DBG_LOG(SEVERE, MOD_NFS, "No configuration for this token");
        DBG_ERR(SEVERE, "No configuration for this token");
	return -1;
    }

    if(!cfg->http_config) {
	DBG_LOG(SEVERE, MOD_NFS, "Illegal HTTP config in token");
	DBG_ERR(SEVERE, "Illegal HTTP config in token");
	if(trace == 1) {
	    DBG_TRACE("HTTP config does not exist in namespace "
		      "configuration");
	}
	goto error;
    }

    p = strchr(uri + 1, '/');
    if(p) {
	nsuid_len = p - uri;
    } else {
	nsuid_len = strlen(uri);
    }

    pobj->ns_uid = nkn_malloc_type(nsuid_len + 1, mod_nfs_strdup2);
    memcpy(pobj->ns_uid, uri, nsuid_len);
    pobj->ns_uid[nsuid_len] = '\0';


    entries = NFS_CONFIG(cfg).entries;
    entries = 1;

    /* There is just one prefix for now*/
    for(i = 0; i < entries; i++) {
        memcpy(pobj->prefix, "/", 1);

//	pobj->ns_uid = nkn_strdup_type(cfg->ns_uid, mod_nfs_strdup2);

        /* Protect from overrun */
        if(!cfg->http_config->origin.u.nfs.server) {
            continue;
        }

        len = strlen(NFS_CONFIG(cfg).server[i]->hostname_path);
        if(len > sizeof(hostname)-1) {
            continue;
        }
	memcpy(hostname, (char *) NFS_CONFIG(cfg).server[i]->hostname_path, len);
	hostname[len] = '\0';

        token = strtok_r(hostname, "/", &saveptr);
        snprintf(pobj->remotehost, sizeof(pobj->remotehost)-1, "%s",
		 token);
	/* Remove the ":" */
        pobj->remotehost[strlen(pobj->remotehost)-1] = '\0';
        snprintf(pobj->remotedir, sizeof(pobj->remotedir)-1, "%s",
		 saveptr);
	if(strlen(saveptr) < sizeof(pobj->remotedir)) {
	    pobj->remotedir[strlen(saveptr)] = '\0';
	} else {
	    pobj->remotedir[sizeof(pobj->remotedir)-1] = '\0';
	}

        pobj->flag = NFS_CONFIG_CREATE;
    }

    if(cfg->http_config->origin.select_method == OSS_NFS_SERVER_MAP) {
        DBG_LOG(SEVERE, MOD_NFS, "Server map detected.");
        DBG_ERR(SEVERE, "Server map detected.");
	//goto server_map;
	pobj->flag = NFS_CONFIG_FILE;
	goto server_map;
    }

    release_namespace_token_t(*ns_token);
    return 0;

 error:
    release_namespace_token_t(*ns_token);
    return -1;
 server_map:
    release_namespace_token_t(*ns_token);
    return 2;
}

void
nkn_nfs_set_stat_ended(namespace_token_t ns_token)
{
    NFS_mount_obj 	*pobj = NULL;

    pthread_mutex_lock(&nkn_nfs_stat_mutex);
    pobj = s_return_obj_from_token(ns_token.u.token);
    if(pobj == NULL) {
        pthread_mutex_unlock(&nkn_nfs_stat_mutex);
        return ;
    }
    AO_fetch_and_add1(&pobj->num_stats_ended);
    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
}

int
nkn_nfs_do_file_stat(char *filename, nkn_cod_t cod, struct stat *sb)
{
    int ret = -1;
    int fd  = -1;

    fd = open(filename, O_DIRECTORY | O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
        close(fd);
	return -1;
    }

    /* Doing the open first to make sure that the file is accessible*/
    fd = open(filename, O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
	return -1;
    }
    close(fd);
    ret = stat(filename, sb);
    if (ret == 0) {
	if(cod) {
	    nkn_objv_t oid;
	    s_compute_version_id(sb, &oid);
	    if (nkn_cod_test_and_set_version(cod, oid, NKN_COD_SET_ORIGIN)) {
		nkn_cod_set_expired(cod);
		return NKN_NFS_VERSION_MISMATCH;
	    }
	}
    }
    return ret;
}

/* Mount function common for NFS_stat and NFS_validate
 */
int
NFS_mount(nkn_uol_t uol, namespace_token_t ns_token, char *filename,
	    int filename_len, char *uri, int trace)
{
    NFS_mount_obj	*pobj = NULL;
    char	        tmp[PATH_MAX + MAX_URI_SIZE];
    char		localdir[PATH_MAX + MAX_URI_SIZE];
    char                *remotehost = NULL, *remotedir = NULL;
    int                 ret = -1;
    char                *postfix;
    char                *part = tmp;
    int                 mounted = 1;
    int                 len;


    if(!uri && uol.cod) {
	ret = nkn_cod_get_cn(uol.cod, &uri, &len);
	if(ret == NKN_COD_INVALID_COD) {
	    glob_nfs_cod_err ++;
	    return NKN_MM_STAT_COD_ERR;
	}
    } else if(uri) {
	len = strlen(uri);
    } else {
	glob_nfs_uri_err ++;
	return NKN_MM_STAT_URI_ERR;
    }

    pthread_mutex_lock(&nkn_nfs_stat_mutex);
    /* Retrieve the object by the same namespace token */
    pobj = s_return_obj_from_uri(ns_token, uri, &part);
    if(!pobj) {
        mounted = 0;
	/* Namespace token is a unique id. We use it here to id the
	   mount object. It will be part of all stat requests. */
        ret = s_nfs_create_obj(&pobj, ns_token, uri, trace);
        if(ret < 0) {
            pthread_mutex_unlock(&nkn_nfs_stat_mutex);
            return NKN_NFS_MOUNT_ERR;
        } else if (ret == 2) {
	    mounted = 1;
	}
	glob_nfs_db_obj_created ++;
        AO_store(&pobj->num_stats_started, 0);
        AO_store(&pobj->num_stats_ended, 0);
    } else {
        if(pobj->flag == NFS_CONFIG_FILE) {
            mounted = 1;
        } else {
            ret = s_nfs_update_obj(pobj, ns_token, uri, trace);
	    if(ret < 0) {
	        /* Object changed and old dir was unmounted. Remount below. */
	        mounted = 0;
	    } else {
	        mounted = pobj->mounted;
	    }
        }
    }

    AO_fetch_and_add1(&pobj->num_stats_started);

    /* Didn't want to mount with the lock held. So, copying the
       relevant things here. */
    if(pobj->flag == NFS_CONFIG_FILE) {
	snprintf(localdir, sizeof(localdir)-1, "%s",
		 pobj->localdir);
	localdir[sizeof(localdir)-1] = '\0';

    } else {
	snprintf(localdir, sizeof(localdir)-1, "%s",
		 pobj->localdir);
	localdir[sizeof(localdir)-1] = '\0';
    }

    DBG_LOG(MSG, MOD_NFS,"NFS_stat: localdir=%s, gets_started=%ld, "
	    "gets_ended=%ld", pobj->localdir,
	    AO_load(&pobj->num_stats_started), AO_load(&pobj->num_stats_ended));

    remotehost = nkn_strdup_type(pobj->remotehost, mod_nfs_strdup3);
    if(!remotehost) {
	ret = NKN_NFS_MOUNT_ERR;
	goto nfs_mount_err;
    }
    remotedir = nkn_strdup_type(pobj->remotedir, mod_nfs_strdup4);
    if(!remotedir) {
	ret = NKN_NFS_MOUNT_ERR;
	goto nfs_mount_err;
    }
    /* Extract the last part of the uri. The problem is that the prefix of
       the uri could match partially. So, there is no way for us to use
       uri to find the mounted filesystem. So, we need to use the stored params.*/
    postfix = s_nfs_extract_postfix(part, pobj->prefix);
    if(postfix == NULL) {
	if(trace == 1) {
	    DBG_TRACE("Programming error. Could not get URI "
		      "postfix, prefix = %s postfix = %s", pobj->prefix,
		      postfix);
	}
	ret = NKN_NFS_MOUNT_ERR;
	goto nfs_mount_err;
    }

    snprintf(filename, filename_len-1, "%s%s",
             pobj->localdir, postfix);
    filename[filename_len-1] = '\0';

    strtok(filename, "?");

    if (mounted == 0) {
	ret = s_nfs_create_mount(localdir, remotehost, remotedir, trace);
        if(ret == 0) {
	    glob_nfs_new_mounts ++;
	    pobj->mounted = 1;
        }
    }
    ret = 0;
nfs_mount_err:;
    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
    if(remotehost)
	free(remotehost);
    if(remotedir)
	free(remotedir);
    return ret;
}

int
NFS_stat(nkn_uol_t	uol,
	 MM_stat_resp_t	*in_ptr_resp)
{
    char               filename[MAX_URI_SIZE + MAX_URI_SIZE + 100];
    char               *uri = NULL;
    int                trace = 0, ret, len;
    struct stat	       sb;
    char tmp[PATH_MAX + MAX_URI_SIZE];
    char *part = tmp;
    NFS_mount_obj *pobj = NULL;


    glob_nfs_stat_cnt ++;
    glob_nfs_stat_cnt_started ++;

    if(in_ptr_resp->in_flags & MM_FLAG_TRACE_REQUEST) {
	trace = 1;
    }

    if ((ret = NFS_mount(uol, in_ptr_resp->ns_token, filename, sizeof(filename),
		    uri, trace))) {
        goto nfs_stat_error;
    }

    if(trace == 1) {
	DBG_TRACE("Received TRACE URI: %s", uri);
    }

	/* Retry the stat command after the mount. */
    if(nkn_nfs_do_file_stat(filename, in_ptr_resp->in_uol.cod, &sb) != 0) {

	/*
	 * Umount the directory here otherwise stray directory is left behind
	 * and sub sequent calls gets delayed
	 */
	if (errno == EIO) {
	    if(!uri && uol.cod) {
		ret = nkn_cod_get_cn(uol.cod, &uri, &len);
		if(ret == NKN_COD_INVALID_COD) {
		    glob_nfs_cod_err ++;
		    return NKN_MM_STAT_COD_ERR;
		}
	    }

	    pthread_mutex_lock(&nkn_nfs_stat_mutex);
	    /* Retrieve the object by the same namespace token */
	    pobj = s_return_obj_from_uri(in_ptr_resp->ns_token, uri, &part);
	    if (pobj) {
		DBG_LOG(MSG, MOD_NFS, "Received IO error,\
		    NFS service may not be running on the origin server: %s",  pobj->localdir);
		DBG_ERR(MSG, "Received IO error, \
		    NFS service may not be running on the origin server: %s",  pobj->localdir);
		ret = umount2(pobj->localdir, MNT_FORCE);
		ret = umount2(pobj->localdir, MNT_FORCE);
		if(ret < 0) {
		    DBG_LOG(SEVERE, MOD_NFS, "\n Delete config: "
			"Unmount NFS directory "
			"failed: %s, errno=%d", pobj->localdir, errno);
		    DBG_ERR(SEVERE, "\n Delete config: Unmount NFS directory "
			"failed: %s, errno=%d", pobj->localdir, errno);
		}
		s_nfs_cleanup_obj(pobj);
	    }
	    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
	}

	glob_nfs_stat_err ++;
	DBG_LOG(MSG, MOD_NFS, "Stat failed after remount "
		"file: %s errno:%d",
		filename, errno);
        nkn_nfs_set_stat_ended(in_ptr_resp->ns_token);
	if(trace == 1) {
	    DBG_TRACE("Could not mount file. Please check "
			"filename: %s. errno = %d", filename, errno);
	}
	goto nfs_stat_not_found;
    } else {
	DBG_LOG(MSG, MOD_NFS, "Stat found for file %s, size = %ld",
		filename, sb.st_size);
    }

    in_ptr_resp->ptype = NFSMgr_provider;
    in_ptr_resp->media_blk_len = NKN_NFS_LARGE_BLK_LEN;

    in_ptr_resp->physid_len = snprintf(in_ptr_resp->physid, NKN_PHYSID_MAXLEN,
					"NFS_%lx_%lx", uol.cod, uol.offset);
    in_ptr_resp->physid_len = MIN(in_ptr_resp->physid_len, NKN_PHYSID_MAXLEN - 1);

    in_ptr_resp->tot_content_len 	= sb.st_size;

    if(uol.offset >= sb.st_size) {
	in_ptr_resp->mm_stat_ret = NKN_NFS_STAT_INV_OFFSET;
        glob_nfs_stat_cnt_ended ++;
        nkn_nfs_set_stat_ended(in_ptr_resp->ns_token);
	return NKN_NFS_STAT_INV_OFFSET;
    }

    nkn_nfs_set_stat_ended(in_ptr_resp->ns_token);
    glob_nfs_stat_cnt_ended ++;
    glob_nfs_stat_success ++;

    if(trace == 1) {
	DBG_TRACE("End TRACE URI: %s. End NFS_stat. Success.", 
		  uri);
    }

    return 0;
 nfs_stat_not_found:
    if(trace == 1) {
	DBG_TRACE("TRACE URI: %s. End NFS stat Error: URI not found",
		  uri);
    }
    in_ptr_resp->mm_stat_ret = NKN_NFS_STAT_URI_ERR;
    glob_nfs_stat_not_found++;
    glob_nfs_stat_cnt_ended ++;
    in_ptr_resp->tot_content_len = 0;
    return NKN_NFS_STAT_URI_ERR;

 nfs_stat_error:
    if(trace == 1) {
	DBG_TRACE("End TRACE URI: %s. Error while doing STAT",
		  uri);
    }

    in_ptr_resp->mm_stat_ret = ret;
    glob_nfs_stat_cnt_ended ++;
    nkn_nfs_set_stat_ended(in_ptr_resp->ns_token);
    in_ptr_resp->tot_content_len = 0;
    return ret;
}

void
nkn_nfs_set_attr_content_length(nkn_attr_t *in_ap, mime_header_t *mh,
				uint64_t in_len)
{
    char	ascii_length[64];

    in_ap->content_length = in_len;

    snprintf(ascii_length, sizeof(ascii_length)-1, "%ld", in_len);
    ascii_length[sizeof(ascii_length)-1] = '\0';

    mime_hdr_add_known(mh, MIME_HDR_CONTENT_LENGTH, ascii_length,
		       strlen(ascii_length));
}

int
nkn_nfs_set_attr_stat_params(char *filename, nkn_cod_t cod, nkn_attr_t *in_ap,
		       mime_header_t *mh)
{
    struct stat         sb;
    char                ascii_str[64];
    int                 ret = -1;
    int                 datestr_len;

    /* Update the current date */
    mk_rfc1123_time(&nkn_cur_ts, ascii_str, sizeof(ascii_str), &datestr_len);

    ret = mime_hdr_add_known(mh, MIME_HDR_DATE, ascii_str, datestr_len);
    if(ret != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "Cannot set cache_control attribute");
	DBG_ERR(SEVERE, "Cannot set cache_control attribute");
	return -1;
    }

    /* Update the last modified time */
    if (stat (filename, &sb) >= 0) {
	mk_rfc1123_time(&sb.st_mtime, ascii_str,
			sizeof(ascii_str), &datestr_len);
	ret = mime_hdr_add_known(mh, MIME_HDR_LAST_MODIFIED, ascii_str,
				 datestr_len);
	if(ret != 0) {
	    DBG_LOG(SEVERE, MOD_NFS, "Cannot set cache_control attribute");
	    DBG_ERR(SEVERE, "Cannot set cache_control attribute");
	    return -1;
	}

	if(cod) {
	    /* Test and set the version ID */
	    s_compute_version_id(&sb, &in_ap->obj_version);
	    if (nkn_cod_test_and_set_version(cod, in_ap->obj_version,
			NKN_COD_SET_ORIGIN)) {
		nkn_cod_set_expired(cod);
		return -1;
	    }
	}
    } else {
	return -1;
    }
    return 0;
}


int
nkn_nfs_set_attr_cache_control(nkn_attr_t *in_ap, mime_header_t *mh,
				int32_t exp_time)
{
    char                ascii_str[64];
    int                 ret;

    if(!in_ap) {
	return -1;
    }
    in_ap->cache_expiry = nkn_cur_ts + exp_time;

    snprintf(ascii_str, sizeof(ascii_str)-1, "max-age=%d", exp_time);
    ascii_str[sizeof(ascii_str)-1] = '\0';

    ret = mime_hdr_add_known(mh, MIME_HDR_CACHE_CONTROL, ascii_str,
			     strlen(ascii_str));
    if(ret != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "Cannot set cache_control attribute");
	DBG_ERR(SEVERE, "Cannot set cache_control attribute");
	return -1;
    }
    return 0;
}

static void
s_nfs_get_ended(namespace_token_t ns_token)
{
    NFS_mount_obj       *pobj = NULL;

    return;

    pthread_mutex_lock(&nkn_nfs_stat_mutex);
    pobj = s_return_obj_from_token(ns_token.u.token);
    if(pobj == NULL) {
        pthread_mutex_unlock(&nkn_nfs_stat_mutex);
        return ;
    }
    AO_fetch_and_add1(&pobj->num_gets_ended);
    AO_fetch_and_add1(&pobj->aio_requests_ended);
    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
}


static void
s_nfs_resume_task(MM_get_resp_t *in_ptr_resp)
{
    nkn_task_set_action_and_state(in_ptr_resp->get_task_id,
			TASK_ACTION_OUTPUT,
			TASK_STATE_RUNNABLE);
}

static int
s_nfs_handle_returned_content(MM_get_resp_t *in_ptr_resp, uint8_t *in_buf,
			      int64_t in_buflen)
{
    int 	        j;
    void                *content_ptr = NULL;
    nkn_attr_t          *attr_ptr = NULL;
    void                *ptr_vobj;
    nkn_uol_t	        uol;
    int		        cont_idx = 0;
    int                 num_pages = 0;
    int                 partial_pages = 0;
    uint64_t	        read_length = 0;
    uint64_t 	        read_offset = 0, write_offset = 0;
    int		        ret_val = -1;
    uint64_t	        tot_len = 0;
    int                 cache_age_default = 0;
    const namespace_config_t *cfg = NULL;
    char                nfsfile[PATH_MAX + MAX_URI_SIZE];
    int                 ret = -1;
    char                *uri;
    int			len;

    ret_val = nkn_cod_get_cn(in_ptr_resp->in_uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
        in_ptr_resp->err_code = NKN_MM_GET_COD_ERR;
	glob_nfs_get_cod_err1 ++;
        s_nfs_get_ended(in_ptr_resp->ns_token);
        goto nfs_get_error;
    }

    if(in_buflen < 0) {
        in_ptr_resp->err_code = -in_buflen;
	glob_nfs_get_cod_err1 ++;
        s_nfs_get_ended(in_ptr_resp->ns_token);
        goto nfs_get_error;
    }

    uol.cod = in_ptr_resp->in_uol.cod;
    uol.offset  = in_ptr_resp->in_uol.offset;
    uol.length  = in_ptr_resp->in_uol.length;
    /* Align the read offset to the current page size */
    read_offset = uol.offset & ~(NKN_NFS_LARGE_BLK_LEN - 1);

    snprintf(nfsfile, sizeof(nfsfile), "%s%s", "/nkn/nfs", uri);
    tot_len = in_ptr_resp->tot_content_len;
 
    if(in_ptr_resp->in_attr != NULL) {
    	mime_header_t mh;

	mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);
	attr_ptr = (nkn_attr_t *) nkn_buffer_getcontent( in_ptr_resp->in_attr);

	/* Set the content length attribute in the mime header. */
	nkn_nfs_set_attr_content_length(attr_ptr, &mh, tot_len);

	cfg = namespace_to_config(in_ptr_resp->ns_token);
	if(!cfg) {
	    DBG_LOG(WARNING, MOD_NFS,
		    "\nNo configuration for this token \n");
	    in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
            s_nfs_get_ended(in_ptr_resp->ns_token);
	    mime_hdr_shutdown(&mh);
	    goto nfs_get_error;
	}
	cache_age_default = cfg->http_config->policies.cache_age_default;
	/* cache expiry time can be set in the cache-control header
	   Per Davey, this is the correct way to do it. */
	nkn_nfs_set_attr_cache_control(attr_ptr, &mh, cache_age_default);
	release_namespace_token_t(in_ptr_resp->ns_token);

	if (nkn_nfs_set_attr_stat_params(nfsfile, in_ptr_resp->in_uol.cod, 
				   attr_ptr, &mh)) {
	    in_ptr_resp->err_code = NKN_NFS_VERSION_MISMATCH;
            s_nfs_get_ended(in_ptr_resp->ns_token);
	    mime_hdr_shutdown(&mh);
	    goto nfs_get_error;
	}

	ret = http_header2nkn_attr(&mh, 0, attr_ptr);
	if (ret) {
	    DBG_LOG(WARNING, MOD_NFS,
		    "http_header2nkn_attr() failed ret=%d", ret);
	    in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
            s_nfs_get_ended(in_ptr_resp->ns_token);
	    mime_hdr_shutdown(&mh);
	    goto nfs_get_error;
	}

	nkn_buffer_setid(in_ptr_resp->in_attr, &uol,
			 NFSMgr_provider, 0);
	mime_hdr_shutdown(&mh);
    }

    /* Need to set the correct ids on the buffers of bufmgr*/
    num_pages = in_buflen / CM_MEM_PAGE_SIZE;
    partial_pages = in_buflen % CM_MEM_PAGE_SIZE;
    if(partial_pages > 0)
	num_pages ++;

    read_length = in_buflen;;
    glob_tot_read_from_nfs += read_length;
    write_offset = 0;
    /* Set the IDs of each buffer for bufmgr to identify  */
    for (j = 1; j <= num_pages; j++) {
	if(in_ptr_resp->out_num_content_objects ==
	   in_ptr_resp->in_num_content_objects) {
	    break;
	}
	ptr_vobj = in_ptr_resp->in_content_array[cont_idx];
	assert(ptr_vobj);

	content_ptr = nkn_buffer_getcontent(ptr_vobj);
	assert(content_ptr);

	uol.offset = read_offset;
	if(read_length >= CM_MEM_PAGE_SIZE) {
	    /* Copy buf contents into the bufmgr buffers 32K at a time */
	    memcpy(content_ptr, in_buf + write_offset, CM_MEM_PAGE_SIZE);
	    uol.length = CM_MEM_PAGE_SIZE;
	    read_offset += CM_MEM_PAGE_SIZE;
	    read_length -= CM_MEM_PAGE_SIZE;
	    write_offset += CM_MEM_PAGE_SIZE;
	} else {
	    memcpy(content_ptr, in_buf + write_offset, read_length);
	    uol.length = read_length;
	    read_offset += read_length;
	    read_length = 0;
	    write_offset += read_length;
	}

	nkn_buffer_setid(ptr_vobj, &uol, NFSMgr_provider, 0);
	in_ptr_resp->out_num_content_objects++;
	cont_idx ++;
    }

    s_nfs_set_ave_server_latency(in_ptr_resp);

    /* Update cache_create based on Response end time . In NFS , there is 
     * no HTTP header values to be processed from another Origin/Cache ,
     * using response end time as Cache_create time . */
    if(attr_ptr != NULL ) {
	    attr_ptr->origin_cache_create = in_ptr_resp->end_time ;
    }
    s_nfs_get_ended(in_ptr_resp->ns_token);
    glob_nfs_get_success ++;
    s_nfs_resume_task(in_ptr_resp);
    glob_nfs_aio_sessions_returned_success ++;
    return 0;


 nfs_get_error:
    in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
    glob_nfs_aio_get_err ++;
    s_nfs_resume_task(in_ptr_resp);
    return -1;
}

/* This function returns the read data to the task through the iovecs in
   the task. We need to copy in 32K chunks into the content buffers. */
int
nfs_handle_aio_content(int fd, uint8_t *in_buf, int64_t in_buflen)
{
    MM_get_resp_t *in_ptr_resp = NULL;
    nkn_task_id_t  task_id = 0;

    if((fd < 0) || (fd >= NFS_MAX_FD)) {
        glob_nfs_aio_bad_fd_returned ++;
	NKN_ASSERT(0);
	return -1;
    }

    if(!nfs_aio_hash_tbl[fd].ptr) {
	NKN_ASSERT(0)
	return -1;
    }

    if(nfs_aio_hash_tbl[fd].type == NFS_GET_TYPE_CACHE) {
	in_ptr_resp = (MM_get_resp_t *)nfs_aio_hash_tbl[fd].ptr;
	nfs_aio_hash_tbl[fd].ptr  = NULL;
	nfs_aio_hash_tbl[fd].type = 0;
	s_nfs_handle_returned_content(in_ptr_resp, in_buf, in_buflen);
	return 0;
    }

    if(nfs_aio_hash_tbl[fd].type == NFS_GET_TYPE_TUNNEL) {
	task_id = (nkn_task_id_t)nfs_aio_hash_tbl[fd].ptr;
	nfs_aio_hash_tbl[fd].ptr  = NULL;
	nfs_aio_hash_tbl[fd].type = 0;
	nkn_nfs_tunnel_handle_returned_content(task_id, in_buf, in_buflen);
	return 0;
    }

    NKN_ASSERT(0);
    return -1;
}

static int
s_nfs_check_ave_server_latency(MM_get_resp_t *in_ptr_resp)
{
    NFS_mount_obj   *pobj = NULL;

    pthread_mutex_lock(&nkn_nfs_stat_mutex);
    pobj = s_return_obj_from_token(in_ptr_resp->ns_token.u.token);
    if(pobj == NULL) {
	in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
	glob_nfs_aio_ave_server_latency_obj_not_found ++;
	pthread_mutex_unlock(&nkn_nfs_stat_mutex);
	return -2;
    }

    /* Allow only 100 outstanding connections per namespace/NFS server*/
    if((AO_load(&pobj->aio_requests_started) -
	AO_load(&pobj->aio_requests_ended)) > 100) {
	/* If the number of total outstanding connections is less than
	   200, allow the number of outstanding connections to grow*/
	if( (AO_load(&glob_nfs_aio_requests_started) -
	     AO_load(&glob_nfs_aio_requests_ended)) > 300) {
	    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
	    return -1;
	}
    }

    if(pobj->last_latency > nkn_max_server_latency) {
        pobj->num_latency_deadline_missed ++;
        pobj->last_latency = 0;
    }
    if(pobj->num_latency_deadline_missed > nkn_nfs_max_deadlines_missed) {
        glob_nfs_aio_ave_server_latency_too_high ++;
        pthread_mutex_unlock(&nkn_nfs_stat_mutex);
        return -1;
    }

    glob_nfs_aio_ave_server_latency_ok ++;
    AO_fetch_and_add1(&pobj->aio_requests_started);
    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
    return 0;
}

static int
s_nfs_set_ave_server_latency(MM_get_resp_t *in_ptr_resp)
{
    NFS_mount_obj   *pobj = NULL;
    int latency = 0;

    pthread_mutex_lock(&nkn_nfs_stat_mutex);
    pobj = s_return_obj_from_token(in_ptr_resp->ns_token.u.token);
    if(pobj == NULL) {
	in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
	pthread_mutex_unlock(&nkn_nfs_stat_mutex);
	glob_nfs_aio_ave_server_latency_obj_not_found ++;
	return -2;
    }

    in_ptr_resp->end_time = nkn_cur_ts;
    latency = in_ptr_resp->end_time - in_ptr_resp->start_time;

    pobj->last_latency = latency;

    AO_fetch_and_add1(&pobj->total_num_gets);
    AO_fetch_and_add1(&pobj->aio_requests_ended);

    pobj->average_server_latency = ((pobj->average_server_latency
                                    * (AO_load(&pobj->total_num_gets) - 1) )
                                    + latency) / AO_load(&pobj->total_num_gets);

    DBG_LOG(MSG, MOD_NFS, "ave latency %d, total num get = %ld, "
                             "latency = %d", pobj->average_server_latency,
                             AO_load(&pobj->total_num_gets), latency);
    pthread_mutex_unlock(&nkn_nfs_stat_mutex);
    return 0;
}

int
NFS_get(MM_get_resp_t *in_ptr_resp)
{
    nkn_uol_t	    uol;
    uint64_t 	    read_offset = 0;
    int		    ret_val = -1;
    uint64_t	    tot_len = 0;
    AM_pk_t         pk;
    char            nfsfile[PATH_MAX + MAX_URI_SIZE];
    char            *uri;
    int             len;

    glob_nfs_aio_get_cnt ++;

    ret_val = nkn_cod_get_cn(in_ptr_resp->in_uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
        in_ptr_resp->err_code = NKN_MM_GET_COD_ERR;
	glob_nfs_get_cod_err ++;
        s_nfs_get_ended(in_ptr_resp->ns_token);
        goto nfs_get_error;
    }

    in_ptr_resp->start_time = nkn_cur_ts;

    if(s_nfs_check_ave_server_latency(in_ptr_resp) < 0) {
	goto nfs_get_server_busy;
    }

    pk.name = uri;
    pk.provider_id = NFSMgr_provider;

    uol.offset = in_ptr_resp->in_uol.offset;
    uol.length = in_ptr_resp->in_uol.length;
    /* Align the read offset to the current page size */
    read_offset = uol.offset & ~(NKN_NFS_LARGE_BLK_LEN - 1);
    snprintf(nfsfile, sizeof(nfsfile), "%s%s", "/nkn/nfs", uri);
    tot_len = in_ptr_resp->tot_content_len;

    /* Align the read offset to the current page size */
    read_offset = uol.offset & ~(NKN_NFS_LARGE_BLK_LEN - 1);

    ret_val = nkn_nfs_do_async_read(nfsfile, read_offset,
				    NKN_NFS_LARGE_BLK_LEN, (void *)in_ptr_resp,
				    NFS_GET_TYPE_CACHE);
    if(ret_val != 0) {
	in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
        s_nfs_get_ended(in_ptr_resp->ns_token);
	goto nfs_get_error;
    }
    return 0;

 nfs_get_server_busy:
    in_ptr_resp->err_code = NKN_NFS_GET_SERVER_BUSY;
    glob_nfs_get_server_busy ++;
    nkn_task_set_action_and_state(in_ptr_resp->get_task_id, TASK_ACTION_OUTPUT,
				    TASK_STATE_RUNNABLE);
    return -1;

 nfs_get_error:
    in_ptr_resp->err_code = NKN_NFS_GET_URI_ERR;
    glob_nfs_get_err ++;
    nkn_task_set_action_and_state(in_ptr_resp->get_task_id, TASK_ACTION_OUTPUT,
				    TASK_STATE_RUNNABLE);
    return -1;
}

void
NFS_cleanup(void)
{
    return;
}

int
NFS_delete(MM_delete_resp_t *in_ptr_resp __attribute((unused)))
{
    return 0;
}

int
NFS_shutdown(void)
{
    return 0;
}

int
NFS_discover(struct mm_provider_priv *p __attribute((unused)))
{
    return 0;
}

void
NFS_promote_complete(MM_promote_data_t *pdata)
{
    AM_pk_t  pk;
    char    *uri;
    int      len;
    int      ret;

    ret = nkn_cod_get_cn(pdata->in_uol.cod, &uri, &len);
    if(ret == NKN_COD_INVALID_COD) {
	glob_nfs_promote_complete_cod_err ++;
	AO_fetch_and_add1(&glob_nfs_files_promote_end);
	return;
    }

    if(pdata->out_ret_code == 0) {
	pk.name = uri;
	pk.provider_id = pdata->in_ptype;
    } else {
	glob_nfs_promote_err ++;
    }
    AO_fetch_and_add1(&glob_nfs_files_promote_end);
    return;
}

int
NFS_evict(void)
{
    return 0;
}

int
NFS_provider_stat (MM_provider_stat_t *tmp)
{
    tmp->weight = 0;
    tmp->percent_full = 0;
    return 0;
}

static int
s_nfs_oomgr_request(char *in_uri, char *in_file_str, uint64_t token)
{

    mime_header_t    httpresp;
    mime_header_t    *oomgr_req = &httpresp;
    char             *serialbuf;
    int              serialbufsz;
    char             requestbuf[4096];
    fqueue_element_t fq_element;
    fhandle_t        fh;
    int              rv = -1;
    char             token_str[64];

    init_http_header(oomgr_req, 0, 0);

    /* Serialize HTTP header */
    serialbufsz = serialize_datasize(oomgr_req);
    serialbuf = (char *)alloca(serialbufsz);
    rv = serialize(oomgr_req, serialbuf, serialbufsz);
    /* rv == 0 Success */
    if(rv != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Serialize failed");
	DBG_ERR(SEVERE, "%s", "Serialize failed");
    }

    snprintf(requestbuf, sizeof(requestbuf)-1, "%s", in_uri);
    requestbuf[sizeof(requestbuf)-1] = '\0';

    /* Initialize fqueue element */
    rv = init_queue_element(&fq_element, 1, "", requestbuf);
    /* rv == 0 Success */
    if(rv != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Init FQUEUE failed");
	DBG_ERR(SEVERE, "%s", "Init FQUEUE failed");
	goto error;
    }

    /* Add "http_header" fqueue element attribute */
    rv = add_attr_queue_element_len(&fq_element, "http_header", 11,
				    serialbuf, serialbufsz);
    /* rv == 0 Success */
    if(rv != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Add attr http_header failed");
	DBG_ERR(SEVERE, "%s", "Add attr http_header failed");
	goto error;
    }

    /* Add "fq_destination" fqueue element attribute
     * which controls the routing of the reply fqueue element
     * within the nvsd file manager.
     */
    rv = add_attr_queue_element_len(&fq_element, "fq_destination", 14,
				    in_file_str, strlen(in_file_str));
    /* rv == 0 Success */
    if(rv != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Add attr fq_destination failed");
	DBG_ERR(SEVERE, "%s", "Add attr fq_destination failed");
	goto error;
    }

    snprintf(token_str, sizeof(token_str), "%ld", token);
    rv = add_attr_queue_element_len(&fq_element, "fq_cookie", 9,
				    token_str, strlen(token_str));
    /* rv == 0 Success */
    if(rv != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Add attr fq_cookie failed");
	DBG_ERR(SEVERE, "%s", "Add attr fq_cookie failed");
	goto error;
    }

    /* Add to the FQueue queue */
    fh = open_queue_fqueue("/tmp/OOMGR.queue");
    if(fh < 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Open fqueue failed");
	DBG_ERR(SEVERE, "%s", "Open fqueue failed");
	goto error;
    }

    rv = enqueue_fqueue_fh_el(fh, &fq_element);
    if(rv != 0) {
	DBG_LOG(SEVERE, MOD_NFS, "%s", "Enqueue fqueue failed");
	DBG_ERR(SEVERE,  "%s", "Enqueue fqueue failed");
	goto error;
    }

    // TODO: Need a strategy for retry
    return 0;

 error:
    return -1;
}

void
NFS_oomgr_response(const char *in_token, const char *in_filename, char *token)
{
    NFS_mount_obj   *pobj = NULL;
    int             queue_obj = 0;
    uint64_t        l_token = atol(token);

    /* TODO HACK: I could not get the token to come through the fqueue
       so, I used a global pointer. I will change it once Yahoo demo
       is checked in. */
    //pobj = glob_nfs_server_map_obj;

    /* Search for the object by token passed in */
    pobj = s_return_obj_from_token(l_token);
    if(pobj == NULL) {
        return ;
    }

    pobj->remp = (NFS_mount_rem_obj_t *) nkn_calloc_type
	(1, sizeof(NFS_mount_rem_obj_t), mod_nfs_mount_rem_obj_t);
    if(!pobj->remp) {
	DBG_LOG(SEVERE, MOD_NFS, "Memory allocation failure. ");
	DBG_ERR(SEVERE, "Memory allocation failure. ");
	return;
    }


    if(strcmp(in_token, YAHOO_NFS_FILE_STR) == 0) {
	snprintf(pobj->remp->nfsconfigfile,
		 sizeof(pobj->remp->nfsconfigfile)-1, "%s", in_filename);
	pobj->remp->nfsconfigfile[sizeof(pobj->remp->nfsconfigfile)-1]
	    = '\0';
	queue_obj = 1;
	pobj->flag = NFS_CONFIG_FILE;
	snprintf(pobj->remp->nfscfgfiletoken,
		 sizeof(pobj->remp->nfscfgfiletoken)-1, "%s",
		 YAHOO_NFS_FILE_STR);
	pobj->remp->nfscfgfiletoken[sizeof(pobj->remp->nfscfgfiletoken)-1] = '\0';
    }

    if(queue_obj == 0) {
	free(pobj);
    } else {
	g_async_queue_push(nfs_rem_file_cfg_q, pobj);
	nfs_async_q_count ++;
    }
    return;
}

/* This thread will make an offline origin manager request for the yahoo
   config file. It also polls the file according to an interval that is
   specified by the configuration. The interval also provides a
   way to stop the thread from running. */
static void
s_nfs_yahoo_cfg_thread_hdl(void)
{
    nkn_nfs_cfg_file_t *ptr = NULL;
    char *in_uri;
    char *in_custtoken;
    int  in_poll_interval;
    int  print_count = 0;
    int  count;
    char dummy[4096];
    int ret = -1;
    int first = 0;

    prctl(PR_SET_NAME, "nvsd-nfs-servermap-cfg", 0, 0, 0);

    count = 0;
    while(1) {
	/* If there is any new configuration, this loop will get it*/
        ptr = (nkn_nfs_cfg_file_t *)g_async_queue_pop(nfs_yahoo_cfg_q);
	if((!ptr) && (first == 0)) {
	    continue;
	} else if(ptr){
	    in_uri = ptr->rem_uri;
	    in_custtoken = ptr->custtoken;
	    in_poll_interval = ptr->poll_interval;
	}
	first = 1;
	/* If the first config has already been read, continue looping*/

	if(nfs_yahoo_thread_exit == 1) {
	    free(ptr);
	    g_thread_exit(&dummy);
	    break;
	}
	while(ret < 0) {
	    ret = s_nfs_oomgr_request(in_uri, in_custtoken, ptr->token);
	    if(ret < 0) {
		/* Try constantly until we enque the request */
		if((print_count % 5) == 0) {
		    DBG_LOG(SEVERE, MOD_NFS, "%s", "OOMGR fqueue "
			    "request failed");
		    DBG_ERR(SEVERE, "%s", "OOMGR fqueue request failed");
		}
		print_count ++;
	    }
	    /* Retry if OOMGR request fails */
	    sleep(2);
	}
	nfs_yahoo_req_complete = 0;

	if(count < 10) {
	    if(nfs_yahoo_req_complete == 0) {
		/* Something went wrong with the response */
		/* Try the request again constantly. TODO: May need to
		   exit at some point */
		if((print_count % 5) == 0) {
		    DBG_LOG(SEVERE, MOD_NFS, "OOMGR response failed. "
			    "Retry count: %d", print_count);
		    DBG_ERR(SEVERE, "OOMGR response failed. Retry count: %d", print_count);
		}
		count ++;
		print_count ++;
		sleep(20);
		ret = -1;
		continue;
	    } else {
		/* Request was satisfied. Continue the big loop*/
		/* Get the file after the time specified and see whether any
		   config has changed. */
		if(in_poll_interval != 0) {
		    sleep(in_poll_interval);
		}
		ret = -1;
		continue;
	    }
	}
	count = 0;
    }
}

static int
s_nfs_start_yahoo_config(char *in_uri, int in_polltime, uint64_t token)
{
    nkn_nfs_cfg_file_t *ptr;

    ptr = (nkn_nfs_cfg_file_t *)nkn_malloc_type
	(sizeof (nkn_nfs_cfg_file_t), mod_nfs_server_map_cfg_file);
    if(!ptr) {
	return -1;
    }

    snprintf(ptr->rem_uri, sizeof(ptr->rem_uri)-1, "%s", in_uri);
    ptr->rem_uri[sizeof(ptr->rem_uri)-1] = '\0';
    ptr->poll_interval = in_polltime;
    snprintf(ptr->custtoken, sizeof(ptr->custtoken)-1, "%s", YAHOO_NFS_FILE_STR);
    ptr->custtoken[sizeof(ptr->custtoken)-1] = '\0';
    nfs_yahoo_poll_interval = in_polltime;
    ptr->token = token;

    g_async_queue_push(nfs_yahoo_cfg_q, ptr);

    return 0;
}

static int
NFS_validate(MM_validate_t *pvld)
{
    mime_header_t hdr;
    char          filename[PATH_MAX + MAX_URI_SIZE];
    char          *uri=NULL;
    const char    *val;
    int           vallen;
    uint32_t      attr;
    int           header_cnt;
    time_t        tlm;
    int           ret, fd;
    struct stat   sb;
    nkn_attr_t    *pnew_attr;
    const namespace_config_t *cfg = NULL;

    pvld->ret_code = MM_VALIDATE_ERROR;
    glob_nfs_tot_validate_called ++;
    assert(pvld);
    if ((ret = NFS_mount(pvld->in_uol, pvld->ns_token, filename,
		    sizeof(filename), uri, 0))) {
        goto nfs_validate_error;
    }

    fd = open(filename, O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
        goto nfs_validate_error;
    }
    close(fd);
    ret = stat(filename, &sb);
    if (ret != 0) {
        goto nfs_validate_error;
    }

    init_http_header(&hdr, 0, 0);
    if (pvld->in_attr) {
	ret = nkn_attr2http_header(pvld->in_attr, 0, &hdr);
	if(!ret) {
	    ret = mime_hdr_get_known(&hdr, MIME_HDR_LAST_MODIFIED, &val, &vallen, &attr,
				&header_cnt);
	    if(!ret) {
		tlm = parse_date(val, val+(vallen-1));
		if(tlm == sb.st_mtime) {
		    cfg = namespace_to_config(pvld->ns_token);
		    if(!cfg) {
			DBG_LOG(WARNING, MOD_NFS,
				"\nNo configuration for this token \n");
		    } else {
			pnew_attr = (nkn_attr_t *) 
			    nkn_buffer_getcontent(pvld->new_attr);
			pnew_attr->cache_expiry = nkn_cur_ts +
			    cfg->http_config->policies.cache_age_default;
			// cache corrected age is zero in this case.
			pnew_attr->origin_cache_create = nkn_cur_ts;
			release_namespace_token_t(pvld->ns_token);
		    }
		    pvld->ret_code = MM_VALIDATE_NOT_MODIFIED;
		} else
		    pvld->ret_code = MM_VALIDATE_MODIFIED;
	    }
	}
    }
    shutdown_http_header(&hdr);

nfs_validate_error:;
    nkn_task_set_action_and_state(pvld->get_task_id, TASK_ACTION_OUTPUT,
				    TASK_STATE_RUNNABLE);
    return 0;
}


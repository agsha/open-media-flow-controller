#ifndef _NKN_NFS_H
#define _NKN_NFS_H
/*
 * (C) Copyright 2008-2011 Juniper Networks, Inc
 *
 * This file contains private definitions of NFS module
 *
 * Author: Jeya ganesh babu
 *
 */

#include <limits.h>
#include "nkn_defs.h"
#include "nkn_mediamgr_api.h"

#define MAX_LIST 128
#define NKN_NFS_LARGE_BLK_LEN (128 * 1024)
#define MAX_NFS_MOUNTS 500
#define NFS_MAX_FD MAX_GNM /* Depends on the global config */

#define NFS_GET_TYPE_CACHE  1
#define NFS_GET_TYPE_TUNNEL 2

pthread_t nfs_aio_thread_obj;
pthread_t nfs_aio_completion_thread_obj;
pthread_t nfs_promote_thread_obj;
pthread_t nfs_get_thread_obj;
GAsyncQueue *nfs_gasync_aiocb_pend_q;
GAsyncQueue *nfs_gasync_aiocb_comp_q;
extern uint64_t glob_nfs_aio_requests_started;
extern uint64_t glob_nfs_aio_requests_ended;

typedef struct nfs_get_resp_t {
    void *ptr;
    char  type;
}nfs_get_resp_t;
extern nfs_get_resp_t nfs_aio_hash_tbl[NFS_MAX_FD];

typedef enum {
    NFS_CONFIG_CREATE = 1,
    NFS_CONFIG_DELETE = 2,
    NFS_CONFIG_FILE = 3,
} nkn_config_flag_t;

#define MAX_NFS_TOKEN_SIZE 64

typedef struct nkn_nfs_cfg_file_s {
    char rem_uri[MAX_URI_SIZE];
    char custtoken[MAX_NFS_TOKEN_SIZE];
    uint64_t token;
    int  poll_interval;
    int  req_complete;
} nkn_nfs_cfg_file_t;

#define NFS_ATTR_SIZE      4096

typedef struct nfst_priv_data_t {
    char        *filename;
    char        *ret_buf;
    uint64_t    filesize;
    char        attr[4096];
} nfst_priv_data_t;

typedef struct NFS_mount_rem_obj_s {
    char		      *remotedir[MAX_NFS_MOUNTS];
    char		      *localdir[MAX_NFS_MOUNTS];
    char                      *remotehost[MAX_NFS_MOUNTS];
    char                      nfsconfigfile[PATH_MAX];
    char                      nfscfgfiletoken[PATH_MAX];
    int                       mounted[MAX_NFS_MOUNTS];
} NFS_mount_rem_obj_t;

typedef struct NFS_promote_cfg_s {
    char                      *uri;
    nkn_provider_type_t       src;
    nkn_provider_type_t       dst;
    uint64_t                  token;
    STAILQ_ENTRY(NFS_promote_cfg_s) entries;
} NFS_promote_cfg_t;

typedef struct NFS_mount_obj_s {
    char 		      prefix[PATH_MAX];
    char 		      mntdir[PATH_MAX];
    char		      remotedir[PATH_MAX];
    char		      localdir[PATH_MAX];
    char                      remotehost[PATH_MAX];
    int                       mounted;
    char                      *ns_uid;
    NFS_mount_rem_obj_t       *remp;
    nkn_nfs_cfg_file_t        *cfgmap;
    nkn_config_flag_t         flag;
    uint64_t                  token;
    int                       num_mounts;
    AO_t                      num_stats_started;
    AO_t                      num_stats_ended;
    AO_t                      num_gets_started;
    AO_t                      num_gets_ended;
    AO_t                      aio_requests_started;
    AO_t                      aio_requests_ended;
    AO_t                      total_num_gets;
    int                       average_server_latency;
    int                       num_latency_deadline_missed;
    int                       last_latency;
#define ORIGIN_TYPE_NFS	    1
#define ORIGIN_TYPE_LOCAL   2
    int                       type;
} NFS_mount_obj;

void test_nfs_mount(void);
void test_nfs_config_file(void);
int nkn_nfs_do_async_read(char *filename, uint64_t in_offset,
			  uint64_t in_length, void *ptr, int type);
int nkn_nfs_do_file_stat(char *filename, nkn_cod_t cod, struct stat *sb);
void nkn_nfs_set_stat_ended(namespace_token_t ns_token);
void *nfs_async_io_suspend_thread_hdl(void *arg);
void *nfs_async_io_suspend_thread_hdl_2(void *arg);
void *nfs_async_io_completion_thread_hdl(void *arg);
int nfs_handle_aio_content(int fd, uint8_t *in_buf, int64_t buflen);
void *nfs_promote_thread_hdl(void *);
void nkn_nfs_set_attr_content_length(nkn_attr_t *in_ap, mime_header_t *mh,
				    uint64_t in_len);
int nkn_nfs_set_attr_stat_params(char *filename, nkn_cod_t cod,
				    nkn_attr_t *in_ap, mime_header_t *mh);
int nkn_nfs_set_attr_cache_control(nkn_attr_t *in_ap, mime_header_t *mh,
				    int32_t exp_time);
int NFS_mount(nkn_uol_t uol, namespace_token_t ns_token, char *filename,
	    int filename_len, char *uri, int trace);
#endif /* _NKN_NFS_H */

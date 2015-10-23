#include <stdint.h>
#include <limits.h>
#include "nkn_attr.h"
#include "nkn_mediamgr_api.h"
#include "nkn_defs.h"

#define NKN_SYSTEM_MAX_NFS 1000000
#define YAHOO_NFS_FILE_STR "YAHOONFSCFGFILE"

/*
  This structure contains information about a URI.
  The URI could be contained across virtual blocks (physid).
  Hence, many of these structures could constitute a single URI.
*/
typedef struct NFS_obj_s {
    char      uri[MAX_URI_SIZE];
    char      filename[PATH_MAX];
    uint32_t  bitmap;
    nkn_attr_t *attr;
    GList     *br_list;
    uint64_t  tot_content_len;              // not used
    uint8_t   is_complete;
    uint64_t  downloaded_len;
    uint32_t  inUse;
    uint64_t  in_bytes;
} NFS_obj;

enum {
    NKN_NFS_OK = 0,
    NKN_NFS_COMPLETE = 1,
    NKN_NFS_GENERAL_ERROR = 2,
    NKN_NFS_DISK_SPACE_EXCEEDED = 3,
    NKN_NFS_WRITE_ERROR = 4,
    NKN_NFS_READ_ERROR = 5,
};


int NFS_init(void);
int NFS_put(struct MM_put_data *map, uint32_t dummy);
int NFS_get(MM_get_resp_t *in_ptr_resp);
void NFS_cleanup(void);
int NFS_delete(MM_delete_resp_t *in_ptr_resp);
int NFS_shutdown(void);
int NFS_discover(struct mm_provider_priv *p);
int NFS_evict(void);
int NFS_provider_stat (MM_provider_stat_t *tmp);
int NFS_stat(nkn_uol_t      uol,
             MM_stat_resp_t  *in_ptr_resp);
void NFS_configure(namespace_token_t ns_token, namespace_config_t *cfg);
void NFS_delete_config(namespace_config_t *cfg);
void NFS_promote_complete(MM_promote_data_t *pdata);
void NFS_config_delete(namespace_token_t *del_tok_list, int num_del);
/* This function is called by the file_manager when a response arrives
   for a file request */
void NFS_oomgr_response(const char *in_token, const char *in_filename, char *token);


void s_test_nfs_put(gpointer data,
		    gpointer user_data);
void test_nfs_multi_thread_start(void);
void test_nfs_multi_thread_init(void);


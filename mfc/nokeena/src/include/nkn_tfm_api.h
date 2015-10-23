#ifndef nkn_tfm_api_h
#define nkn_tfm_api_h
#include <stdint.h>
#include "nkn_attr.h"
#include "nkn_mediamgr_api.h"
#include "nkn_defs.h"
#include <glib.h>

#define NKN_SYSTEM_MAX_TFM 1000000

#define TFM_OBJ_LOCK(pobj) pthread_mutex_lock(&pobj->obj_mutex);
#define TFM_OBJ_UNLOCK(pobj) pthread_mutex_unlock(&pobj->obj_mutex);

extern GHashTable *nkn_tfm_uri_hash_table;
/*
        This structure contains information about a URI.
        The URI could be contained across virtual blocks (physid).
        Hence, many of these structures could constitute a single URI.
*/
#define TFM_MAX_FILENAME_LEN 100
typedef struct TFM_obj_s {
    nkn_cod_t           cod;
    char                filename[TFM_MAX_FILENAME_LEN];
    nkn_attr_t           *attr;
    GList               *br_list;
    uint64_t            tot_content_len;
    uint8_t             is_complete;
    uint8_t             is_promoted;
    uint8_t             is_buf_only;
    uint64_t            downloaded_len;
    uint32_t            inUse;
    uint64_t            in_bytes;
    time_t              time_modified;
    time_t              time_last_promoted;
    int                 defer_delete;
    int                 pht_spc_resv;
    uint64_t            partial_promote_offset;
    uint64_t            partial_promote_start;
    uint64_t            partial_promote_complete;
    uint64_t		partial_promote_err_complete;
    uint64_t            total_blocks;
    uint8_t             partial_file_write;
    uint8_t             partial_queue_added;
    uint8_t             partial_promote_in_progress;
    int			file_id;
    nkn_provider_type_t dst;
    pthread_mutex_t	obj_mutex;

    TAILQ_ENTRY(TFM_obj_s) promote_entries;
    TAILQ_ENTRY(TFM_obj_s) entries;
} TFM_obj;

typedef struct TFM_promote_cfg_s {
    char                      *uri;
    nkn_cod_t                 cod;
    nkn_provider_type_t       src;
    nkn_provider_type_t       dst;
    uint64_t                  token;
    STAILQ_ENTRY(TFM_promote_cfg_s) entries;
} TFM_promote_cfg_t;

typedef struct TFM_promote_comp_s {
    nkn_cod_t cod;
    int       out_ret_code;
    int       in_ptype;
    int       in_sub_ptype;
    int       partial_file_write;
    STAILQ_ENTRY(TFM_promote_comp_s) entries;
}TFM_promote_comp_t;

enum {
        NKN_TFM_OK = 0,
        NKN_TFM_COMPLETE = 1,
        NKN_TFM_GENERAL_ERROR = 2,
        NKN_TFM_DISK_SPACE_EXCEEDED = 3,
        NKN_TFM_WRITE_ERROR = 4,
        NKN_TFM_READ_ERROR = 5,
};


int TFM_init(void);
int TFM_put(struct MM_put_data *map, uint32_t dummy);
int TFM_get(MM_get_resp_t *in_ptr_resp);
void TFM_cleanup(void);
int TFM_delete(MM_delete_resp_t *in_ptr_resp);
int TFM_shutdown(void);
int TFM_discover(struct mm_provider_priv *p);
int TFM_evict(void);
int TFM_provider_stat (MM_provider_stat_t *tmp);
int TFM_stat(nkn_uol_t      uol,
             MM_stat_resp_t  *in_ptr_resp);
void TFM_promote_complete(MM_promote_data_t *tfm_data);
void s_test_tfm_put(gpointer data,
               gpointer user_data);
void test_tfm_multi_thread_start(void);
void test_tfm_multi_thread_init(void);
void *TFM_promote_thread_hdl(void *dummy);
void TFM_promote_wakeup(void);
#endif

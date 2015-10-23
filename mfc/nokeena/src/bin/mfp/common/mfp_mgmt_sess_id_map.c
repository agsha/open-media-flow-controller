#include <stdio.h>
#include <pthread.h>
#include <glib.h>
#include <string.h>

#include "mfp_mgmt_sess_id_map.h"
#include "mfp_publ_context.h"
#include "nkn_memalloc.h"

static GHashTable *sess_table;
static pthread_mutex_t sess_table_lock;
uint64_t glob_mfp_mgmt_sess_tbl_entries;

int32_t
mfp_mgmt_sess_tbl_init(void)
{
    sess_table = g_hash_table_new(g_str_hash, g_str_equal);
    pthread_mutex_init(&sess_table_lock, NULL);
    
    return 0;
}

int32_t
mfp_mgmt_sess_tbl_add(int32_t sess_id, char *mgmt_sess_id)
{
    int32_t rv;
    mgmt_sess_state_t *sess_obj, *sess_existing_obj;
    
    rv = 0;

    pthread_mutex_lock(&sess_table_lock);
    sess_existing_obj = (mgmt_sess_state_t *)\
	g_hash_table_lookup(sess_table, mgmt_sess_id);
    if (sess_existing_obj != NULL) {
	rv = 1;
    } else {
	sess_obj = (mgmt_sess_state_t*)\
	    nkn_calloc_type(1, sizeof(mgmt_sess_state_t), mod_mfp_mgmt_sess_tbl_add);
	sess_obj->sess_id = sess_id;
	pthread_mutex_init(&sess_obj->lock, NULL);
	g_hash_table_insert(sess_table, strdup(mgmt_sess_id), sess_obj);
	glob_mfp_mgmt_sess_tbl_entries++;
    }

    pthread_mutex_unlock(&sess_table_lock);
    return rv;

}

mgmt_sess_state_t *
mfp_mgmt_sess_tbl_find(char *mgmt_sess_id)
{
    mgmt_sess_state_t *sess_obj;

    pthread_mutex_lock(&sess_table_lock);
    sess_obj = g_hash_table_lookup(sess_table, mgmt_sess_id);
    pthread_mutex_unlock(&sess_table_lock);

    return sess_obj;
}

mgmt_sess_state_t *
mfp_mgmt_sess_tbl_acq(char *mgmt_sess_id)
{
    mgmt_sess_state_t *sess_obj;

    pthread_mutex_lock(&sess_table_lock);
    sess_obj = g_hash_table_lookup(sess_table, mgmt_sess_id);
    if (sess_obj) { 
	pthread_mutex_lock(&sess_obj->lock);
    }
    pthread_mutex_unlock(&sess_table_lock);

    return sess_obj;
}

int32_t
mfp_mgmt_sess_tbl_rel(char *mgmt_sess_id)
{
    int32_t rc = -1;
    mgmt_sess_state_t *sess_obj;
    pthread_mutex_lock(&sess_table_lock);
    sess_obj = g_hash_table_lookup(sess_table, mgmt_sess_id);
    if (sess_obj != NULL) {
	pthread_mutex_unlock(&sess_obj->lock);
	rc = 1;
    }
    pthread_mutex_unlock(&sess_table_lock);

    return rc;
}

int32_t
mfp_mgmt_sess_tbl_remove(char *mgmt_sess_id)
{
    mgmt_sess_state_t *sess_obj = NULL;

    pthread_mutex_lock(&sess_table_lock);
    sess_obj = g_hash_table_lookup(sess_table, mgmt_sess_id);
    if (!sess_obj) {
	pthread_mutex_unlock(&sess_table_lock);
	return 0;
    }
    g_hash_table_remove(sess_table, mgmt_sess_id);
    free(mgmt_sess_id);
    glob_mfp_mgmt_sess_tbl_entries--;
    pthread_mutex_unlock(&sess_table_lock);

    return 0;
}

int32_t mfp_mgmt_sess_unlink(char *mgmt_sess_id, int32_t status,
			     int32_t err)
{

    mgmt_sess_state_t *sess_obj;

    pthread_mutex_lock(&sess_table_lock);
    sess_obj = g_hash_table_lookup(sess_table, mgmt_sess_id);
    if (!sess_obj) {
	pthread_mutex_unlock(&sess_table_lock);
	return 0;
    }
    pthread_mutex_lock(&sess_obj->lock);
    sess_obj->sess_id = -1;
    sess_obj->status = status;
    sess_obj->err = err;
    pthread_mutex_unlock(&sess_obj->lock);
    pthread_mutex_unlock(&sess_table_lock);

    return 0;
}

void mfp_cleanup_mgmt_sess_state(mgmt_sess_state_t *p)
{

    if (p) {
	free(p);
    }
}

/**
 * replace an existing session id with a new session id and
 * re-initialize the status code and error codes;
 * NOTE: this function should be called with the session object lock
 * held and it should be noted that we will lose all history of the
 * previous incarnation of this session object
 */
int32_t mfp_mgmt_sess_replace(mgmt_sess_state_t *sess_obj,
			      int32_t sess_id)
{

    sess_obj->sess_id = sess_id;
    sess_obj->status = PUB_SESS_STATE_INIT;
    sess_obj->err = 0;
 
   return 0;
}

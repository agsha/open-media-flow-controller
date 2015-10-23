/*
 * Implementation of the Session Binding Design for Smoothflow
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <glib.h>

#include "nkn_defs.h"
#include "nkn_errno.h"
#include "nkn_vpe_metadata.h"
#include "nkn_ssp_sessionbind.h"

// Hash Table for Smoothflow Session Binding and its associated lock
static GHashTable *sess_table;
static pthread_mutex_t sess_table_lock;

static unsigned long long glob_sess_pending=0; // Global counter to track hash entries

// Initialize Hash table
void nkn_ssp_sessbind_init(void)
{
    sess_table = g_hash_table_new(g_str_hash, g_str_equal);
    pthread_mutex_init(&sess_table_lock, NULL);
}

/*
 * Adds a new entry. 0 on success and non-zero if
 * there is already an existing entry in the table.
 */
int nkn_ssp_sess_add(nkn_ssp_session_t *sess_obj)
{
    nkn_ssp_session_t *sess_existing_obj;
    int rv=0;

    pthread_mutex_lock(&sess_table_lock);

    sess_existing_obj = (nkn_ssp_session_t *)g_hash_table_lookup(sess_table, sess_obj->session_id);
    if (sess_existing_obj != NULL) {
	rv = 1;
    } else {
	g_hash_table_insert(sess_table, sess_obj->session_id, sess_obj);
	glob_sess_pending++;
    }

    pthread_mutex_unlock(&sess_table_lock);

    return rv;
}

/*
 * Find an existing entry using session id.
 * It is returned with its lock held.
 * The caller must release the lock and/or free the object.
 */
nkn_ssp_session_t* nkn_ssp_sess_get(char *session_id)
{
    nkn_ssp_session_t *sess_obj;

    pthread_mutex_lock(&sess_table_lock);

    sess_obj = g_hash_table_lookup(sess_table, session_id);
    if(sess_obj)
	pthread_mutex_lock(&sess_obj->lock);

    pthread_mutex_unlock(&sess_table_lock);

    return sess_obj;
}


/* Remove an entry from the table with lock */
void nkn_ssp_sess_remove(nkn_ssp_session_t *sess_obj)
{
    pthread_mutex_lock(&sess_table_lock);

    g_hash_table_remove(sess_table, sess_obj->session_id);
    glob_sess_pending--;

    pthread_mutex_lock(&sess_obj->lock);
    pthread_mutex_unlock(&sess_table_lock); 
}


nkn_ssp_session_t* sess_obj_alloc(void)
{
    nkn_ssp_session_t *sess_obj;

    sess_obj = nkn_calloc_type(1, sizeof(nkn_ssp_session_t),
    				mod_ssp_session_t);
    if (sess_obj == NULL) {
	return NULL;
    }

    sess_obj->sflow_data = nkn_calloc_type(1, sizeof(sf_meta_data),
    					mod_ssp_sf_meta_data);
    if (sess_obj->sflow_data == NULL){
	free(sess_obj);
	return NULL;
    }

    return sess_obj;
}

void sess_obj_free(nkn_ssp_session_t *sess_obj)
{
    free(sess_obj);
}

#if 0

/*
 * Unit Test 1
 * gcc -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include 
 * -I/home/zubair/samara/trunk/nokeena/src/include/ -lglib-2.0 -g -o bind nkn_ssp_sessionbind.c
 *
 * valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes -v ./bind
 */
#define MAX_UNITTEST_ENTRIES 20
int unit_test1(int argc, char* argv[])
{
    unsigned int i, j;
    char buf[40];
    nkn_ssp_session_t *sobj[MAX_UNITTEST_ENTRIES];
    nkn_ssp_session_t *get_sobj;

    nkn_ssp_sessbind_init();

    // Generate and add entries
    for (i=0; i < MAX_UNITTEST_ENTRIES; i++){
	sobj[i] = sess_obj_alloc();

	sobj[i]->sflow_data->version = i;

	memset(buf, 0, 40);
        snprintf(buf, 15, "ID:%010ld", i);
	strcpy(sobj[i]->session_id, buf);
        pthread_mutex_init(&sobj[i]->lock, NULL);

	nkn_ssp_sess_add(sobj[i]);
    }

    printf(" Hash Table Size %ld \n", g_hash_table_size(sess_table));

    //Lookup iterator
    for (i=0; i < MAX_UNITTEST_ENTRIES; i++){
	memset(buf, 0, 40);
        snprintf(buf, 15, "ID:%010ld", i);

	get_sobj = nkn_ssp_sess_get(buf);
	printf(" Key: %s\t Session Object ID: %s \t Version: %d\n", buf, get_sobj->session_id, get_sobj->sflow_data->version);
	get_sobj->sflow_data->version = i*100;

	pthread_mutex_unlock(&get_sobj->lock);

	printf(" Hash Table Size %ld \n", g_hash_table_size(sess_table));
    }

   //Lookup iterator
    for (i=0; i < MAX_UNITTEST_ENTRIES; i++){
	memset(buf, 0, 40);
        snprintf(buf, 15, "ID:%010ld", i);

	get_sobj = nkn_ssp_sess_get(buf);
	printf(" Key: %s\t Session Object ID: %s \t Version: %d\n", buf, get_sobj->session_id, get_sobj->sflow_data->version);
	pthread_mutex_unlock(&get_sobj->lock);

	nkn_ssp_sess_remove(get_sobj);
	pthread_mutex_unlock(&get_sobj->lock);
	sess_obj_free(get_sobj);

	printf(" Hash Table Size %ld \n", g_hash_table_size(sess_table));
    }

    g_hash_table_destroy(sess_table);

    return 0;
}
#endif

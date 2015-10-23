#ifndef nkn_tfm_h
#define nkn_tfm_h
#include <glib.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <pthread.h>
#include <glib.h>
#include "nkn_cod.h"

#define DIRECT_AL_MASK (512 - 1)
#define NO_TFM_WRITE 0
#define O_DIRECT_WRITE 1
#define NORMAL_WRITE 2
#define MAX_PROMOTE_QUEUE_PER_FILE 1

extern uint64_t glob_tfm_pobj_glib_mem;
extern uint64_t glob_tfm_promote_pobj_glib_mem;
void tfm_promote_db_print(GHashTable *tfm_db);
void tfm_db_print(GHashTable *tfm_db);
void tfm_cod_db_print(GHashTable *tfm_db);
void nkn_tfm_uri_tbl_init(GHashTable **tfm_db);
int nkn_tfm_uri_tbl_insert(GHashTable *tfm_db,
		       char *key,
		       void *value);

void nkn_tfm_uri_tbl_delete(GHashTable *tfm_db, char *key);
void *nkn_tfm_uri_tbl_get(GHashTable *tfm_db, char *key);
void nkn_tfm_uri_tbl_cleanup(GHashTable *tfm_db);
void s_cleanup_obj(gpointer data);
void nkn_tfm_uri_tbl_init_full(GHashTable **tfm_db,
                          GDestroyNotify delete_key,
                          GDestroyNotify delete_value);

void nkn_tfm_cod_tbl_init(GHashTable **tfm_db);
int nkn_tfm_cod_tbl_insert(GHashTable *tfm_db,
			   void *key,
			   void *value);

void nkn_tfm_cod_tbl_delete(GHashTable *tfm_db, nkn_cod_t key);
void *nkn_tfm_cod_tbl_get(GHashTable *tfm_db, nkn_cod_t key);
void nkn_tfm_cod_tbl_cleanup(GHashTable *tfm_db);
void nkn_tfm_cod_tbl_init_full(GHashTable **tfm_db,
                          GDestroyNotify delete_key,
                          GDestroyNotify delete_value);


int tfm_cod_test_and_get_uri(nkn_cod_t cod, char **uri);
#endif

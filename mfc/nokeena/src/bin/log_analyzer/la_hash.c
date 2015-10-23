#include <sys/epoll.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <err.h>
#include <stdarg.h>
#include <ctype.h>
#include <search.h>
#include <glib.h>

#include <sys/stat.h>
#include <syslog.h>
#include <assert.h>
#include "hash.h"


void create_hash_table(int thrd_id);
void insert_replace_hash(char *key, domain_hash_val_t *dom_val, int thrd_id);
void insert_replace_urihash(char *key, uri_hash_val_t *uri_val, int thrd_id);

/*
 * hash table to store popular domains based on bytes served.
 */
GHashTable *g_pop_domain_hash_table[MAX_THRDS];

/*
 * Key is pointer to domain.
 * value is pointer to domain_hash_val structure object
 */
GHashTable *g_domain_hash_table[MAX_THRDS];

/*
 * Key is pointer to uri (if uri size < 32bytes) or 
 * md5 checksum of uri (if urisize > 32bytes).
 * value is pointer to uri_hash_val structure object
 */
GHashTable *g_uri_hash_table[MAX_THRDS];

/*
 * Master hash tables will be useful for multithreaded 
 * environment, where each thread processes its own access log
 * and populates its own domain and uri hash tables. Once all the
 * access logs are processed, all the hash tables are merged into the 
 * master hash tables.
 */
GHashTable *g_master_domain_hash_table;
GHashTable *g_master_uri_hash_table;
GHashTable *g_pop_domains;

/*
 * This hash table stores the 1-hit lru uris 
 */
GHashTable *g_lru_hash_table;
domain_hash_val_t * hash_lookup(char *key, int thrd_id);
void insert_replace_master_domhash(char *key, domain_hash_val_t *dom_val);
void insert_replace_master_urihash(char *key, uri_hash_val_t *uri_val);

/*
 * This function creates the domain and uri hash tables.
 */
void
create_hash_table(int thrd_id)
{
    g_domain_hash_table[thrd_id]       = g_hash_table_new(g_str_hash, g_str_equal);
    g_uri_hash_table[thrd_id]	       = g_hash_table_new(g_str_hash, g_str_equal);
}

domain_hash_val_t *
hash_lookup(char *key, int thrd_id){
    return(g_hash_table_lookup(g_domain_hash_table[thrd_id], key));
}

/*
 * This function inserts an entry into domain hash table 
 * in case of new entry or updates the hash table if 
 * the entry is already present.
 */
void
insert_replace_hash(char *key, domain_hash_val_t *dom_val, int thrd_id)
{
    if (g_domain_hash_table[thrd_id] != NULL) {
	domain_hash_val_t *dom_p = NULL;
	dom_p = g_hash_table_lookup(g_domain_hash_table[thrd_id], key);
	if (dom_p == NULL){
	    g_hash_table_insert(g_domain_hash_table[thrd_id], key, dom_val);
	}else {
	    g_hash_table_replace(g_domain_hash_table[thrd_id], key, dom_val);
	    printf("In replace\n");
	}
    }
}

/*
 * This function inserts an entry into uri hash table 
 * in case of new entry or updates the hash table if 
 * the entry is already present.
 */
void
insert_replace_urihash(char *key, uri_hash_val_t *uri_val, int thrd_id)
{
    if (g_uri_hash_table[thrd_id] != NULL) {
	uri_hash_val_t *uri_p = NULL;
	uri_p = g_hash_table_lookup(g_uri_hash_table[thrd_id], key);
	if (uri_p == NULL){
	    g_hash_table_insert(g_uri_hash_table[thrd_id], key, uri_val);
	}else {
	    g_hash_table_replace(g_uri_hash_table[thrd_id], key, uri_val);
	}
    }
}

void
insert_replace_master_domhash(char *key, domain_hash_val_t *dom_val)
{
    if (g_master_domain_hash_table != NULL) {
	domain_hash_val_t *dom_p = NULL;
	dom_p = g_hash_table_lookup(g_master_domain_hash_table, key);
	if (dom_p == NULL){
	    g_hash_table_insert(g_master_domain_hash_table, key, dom_val);
	}else {
	    g_hash_table_replace(g_master_domain_hash_table, key, dom_val);
	    printf("In replace\n");
	}
    }
}

void
insert_replace_master_urihash(char *key, uri_hash_val_t *uri_val)
{
    if (g_master_uri_hash_table != NULL) {
	uri_hash_val_t *uri_p = NULL;
	uri_p = g_hash_table_lookup(g_master_uri_hash_table, key);
	if (uri_p == NULL){
	    g_hash_table_insert(g_master_uri_hash_table, key, uri_val);
	}else {
	    g_hash_table_replace(g_master_uri_hash_table, key, uri_val);
	}
    }
}


/**
 * @file   crst_geodb_mgr.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Apr 10 08:45:47 2012
 * 
 * @brief  implements the obj_store_t interface for geodb.
 * 
 * 
 */
#include "crst_geodb_mgr.h"

#include "GeoIP.h"
#include "GeoIPCity.h"

static int32_t crst_geodb_init(obj_store_t *st, void *state_data);

static int32_t crst_geodb_read(obj_store_t *st, 
       char *key, uint32_t key_len, char *buf, uint32_t *buf_len);

static int32_t crst_geodb_write(obj_store_t *st, 
	char *key, uint32_t key_len, char *buf, uint32_t buf_len);

static int32_t crst_geodb_shutdown(obj_store_t *st);

static void * crst_geodb_result_copy(void *item);

static void * crst_geodb_key_copy(void *item);

static void crst_geodb_result_free(void *p);

static int32_t crst_geodb_db_ctx_delete(geodb_ctx_t *gc);

static inline void crst_geodb_ctx_update_miss_stats(geodb_ctx_t *gd);

static inline void crst_geodb_ctx_update_hit_stats(geodb_ctx_t *gd);


// extern
extern int geodb_install( const char *file_path_gz, 
			  char *ret_output);

int32_t
crst_geodb_store_create(obj_store_t **out)
{
    obj_store_t *ctx = NULL;
    int32_t err = 0;

    ctx = (obj_store_t *)
	nkn_calloc_type(1, sizeof(obj_store_t), 100);
    if (!ctx) {
	err = -ENOMEM;
	goto error;
    }

    ctx->init = crst_geodb_init;
    ctx->read = crst_geodb_read;
    ctx->write = NULL;
    ctx->shutdown = crst_geodb_shutdown;
    pthread_mutex_init(&ctx->lock, NULL);
    
    *out = ctx;
    return err;
    
 error:
    if (ctx) free(ctx);
    return err;

}

/** 
 * install or initialize a database. Only supported geodb is maxmind
 * 
 * @param st [in] - store object instance
 * @param state_data [in] - db specific state
 * 
 * @return 
 */
static int32_t
crst_geodb_init(obj_store_t *st, void *state_data) 
{
    geodb_ctx_t *gc = NULL;
    int32_t err = 0;
    char err_buf[256] = {0};
    int dbtype;

    pthread_mutex_lock(&st->lock);
    st->state_data = state_data;
    gc = (geodb_ctx_t *)state_data;    
    
    /* if there is a change in the config management to load a new db
     * then we install the new db and load it.
     * when called from the startup, 
     */
    if (gc->db_file_name[0] != '\0') {
	err = geodb_install(gc->db_file_name, err_buf);
	if (err) {
	    /* if install fails the init the existing db's
	     * if no db exists then return error
	     */
	    err = geodb_open(gc->init_flags);
	    if (err) {
		goto error;
	    }
	}
    } else {
	/* open existing db's */
	err = geodb_open(gc->init_flags);
	if (err) {
	    goto error;
	}
    }

    dbtype = geodb_edition();
    if (dbtype != GEOIP_CITY_EDITION_REV1) {
	err = -2;
	goto error;
    }

 error:
    pthread_mutex_unlock(&st->lock);
    return err;
}

/** 
 * read the geodb (only MaxMind supported currently) using the
 * existing geodb_lookup API.
 * 
 * @param st [in] - store object instance
 * @param key [in] - lookup key
 * @param key_len [in] - key length
 * @param buf [in/out] - output, should be atleast 1KB in length
 * @param buf_len [in/out] - sizeof of the output after the lookup
 * 
 * @return 
 */
static int32_t
crst_geodb_read(obj_store_t *st, char *key, uint32_t key_len,
		char *buf, uint32_t *buf_len)
{
    int32_t err = 0;
    geodb_ctx_t *gd = NULL;
    geo_ip_req_t ip_data;
    geodb_result_cache_elm_t *elm = NULL;
    geo_ip_t *geo_data = NULL;
    char ip_str[16] = {0};
    struct in_addr addr;
    
    pthread_mutex_lock(&st->lock);
    gd = (geodb_ctx_t *)st->state_data;

    /* OOB check */
    if (key_len > 16) {
	err = -1;
	goto error;
    }
    if (*buf_len < sizeof(geo_ip_t)) {
	err = -1;
	goto error;
    }

    /* setup geodb query */
    memcpy(ip_str, key, key_len);
    inet_aton((const char *)ip_str, &addr);    
    ip_data.magic = GEODB_MAGIC;
    ip_data.op_code = GEODB_LOOKUP;
    ip_data.ip = ntohl(addr.s_addr);

    /* check results cache if the query exists in cache */
    elm = (geodb_result_cache_elm_t*)
	cp_hashtable_get(gd->results_cache, &ip_data.ip);
    if (elm) {

	geo_data = &elm->res;
	memcpy(buf, geo_data, sizeof(geo_ip_t));
	*buf_len = sizeof(geo_ip_t);
	/* exists in cache done! */
	pthread_mutex_lock(&gd->lock);
	crst_geodb_ctx_update_hit_stats(gd);
	/* keep lru sorted */
	TAILQ_REMOVE(&gd->lru_list, elm, entries);
	TAILQ_INSERT_HEAD(&gd->lru_list, elm, entries);
	pthread_mutex_unlock(&gd->lock);
	goto error;
    }
    
    /* add new entry to cache. if the cache size is exceeded remove
     * last entry in lru list  
     */
    elm = (geodb_result_cache_elm_t *)
	nkn_calloc_type(1, sizeof(geodb_result_cache_elm_t), 100);
    if (!elm) {
	err = -ENOMEM;
	goto error;
    }
    geo_data = &elm->res;
    err = geodb_lookup(&ip_data, geo_data);
    cp_hashtable_put(gd->results_cache, &ip_data.ip, elm);

    /* copy result to caller */
    memcpy(buf, geo_data, sizeof(geo_ip_t));
    *buf_len = sizeof(geo_ip_t);

    /* result cache state update */
    pthread_mutex_lock(&gd->lock);
    crst_geodb_ctx_update_miss_stats(gd);
    if (gd->num_cache_entries == gd->tot_cache_entries + 1) {
	/* evict the oldest entry (tail of LRU) */
	geodb_result_cache_elm_t *old_elm = NULL;
	old_elm = TAILQ_LAST(&gd->lru_list, tag_geodb_result_cache_list);
	TAILQ_REMOVE(&gd->lru_list, old_elm, entries);

	/* cprops hash table set to DEEP COLLECTION which will ensure
	 * that registree cleanup callback will be called. 
	 */
	cp_hashtable_remove(gd->results_cache, &old_elm->res.ip);

	/* store the last evicted entry for debuggin and unit test check */
	free(gd->dbg_last_evicted_entry);
	gd->dbg_last_evicted_entry = old_elm;
	
    }
    TAILQ_INSERT_HEAD(&gd->lru_list, elm, entries);
    
    pthread_mutex_unlock(&gd->lock);

 error:
    pthread_mutex_unlock(&st->lock);
    return err;
}

static int32_t
crst_geodb_write(obj_store_t *st, char *key, uint32_t key_len, 
		char *buf, uint32_t buf_len)
{

    return 0;
}

static int32_t
crst_geodb_shutdown(obj_store_t *st) 
{
    geodb_ctx_t *gd = NULL;
    
    pthread_mutex_lock(&st->lock);
    gd = (geodb_ctx_t *)st->state_data;
    crst_geodb_db_ctx_delete(gd);
    geodb_close();
    pthread_mutex_unlock(&st->lock);
    
    return 0;
}

static void *
crst_geodb_key_copy(void *item)
{
    struct in_addr *addr = NULL;
    
    addr = (struct in_addr *)
	nkn_calloc_type(1, sizeof(struct in_addr), 100);
    if (!addr) {
	return NULL;
    }
    
    memcpy(addr, item, sizeof(struct in_addr));
    
    return addr;
}
   
static void *
crst_geodb_result_copy(void *item)
{
    return item;    
}

static void
crst_geodb_result_free(void *p)
{
    /* do nothing */
}

int32_t
crst_geodb_db_ctx_create(uint32_t num_cache_entries, 
			 geodb_ctx_t **out)
{
    geodb_ctx_t *gc =NULL;
    int32_t err = 0;

    assert(out);
    *out = NULL;

    gc = (geodb_ctx_t *)
	nkn_calloc_type(1, sizeof(geodb_ctx_t), 100);
    if (!gc) {
	err = -ENOMEM;
	goto error;
    }
    TAILQ_INIT(&gc->lru_list);
    gc->init_flags = 0;
    gc->tot_cache_entries = num_cache_entries;
    gc->results_cache =
	cp_hashtable_create_by_option(COLLECTION_MODE_COPY |
				COLLECTION_MODE_DEEP,
				300,
			  	cp_hash_int,
				(cp_compare_fn) cp_hash_compare_int,
				(cp_copy_fn) crst_geodb_key_copy,
			  	(cp_destructor_fn) free,
			  	(cp_copy_fn) crst_geodb_result_copy,
			  	(cp_destructor_fn) crst_geodb_result_free);
    if (!gc->results_cache) {
	err = -ENOMEM;
	goto error;
    }
    gc->delete = crst_geodb_db_ctx_delete;
    pthread_mutex_init(&gc->lock, NULL);
    *out = gc;
    return err;

 error:
    *out = NULL;
    crst_geodb_db_ctx_delete(gc);
    return err;
}

static int32_t
crst_geodb_db_ctx_delete(geodb_ctx_t *gc)
{
    if (gc) {
	if (gc->results_cache) {
	    cp_hashtable_destroy(gc->results_cache);
	}
	free(gc);
    }
    
    return 0;
}

/** 
 * updates the results cache miss stats; SHOULD always be called with
 * the lock held
 * 
 * @param gd - geodb context to update
 */
static inline void
crst_geodb_ctx_update_miss_stats(geodb_ctx_t *gd)
{
    gd->tot_hits++;
    if (gd->num_cache_entries != gd->tot_cache_entries + 1)
	gd->num_cache_entries++;
}

static inline void
crst_geodb_ctx_update_hit_stats(geodb_ctx_t *gd)
{
    gd->num_cache_hits++;
}

#ifdef CRST_GEODB_MGR_UT
#include "nkn_mem_counter_def.h"

int nkn_mon_add(const char *name, const char *instance, void *obj,
		uint32_t size)
{   
    return 0;
}

obj_store_t *h_geodb = NULL;
#define GEODB_MGR_UT_DB_FILE_NAME "/var/home/root/sunil/GeoIPCity.dat.gz"

static int32_t GEODB_MGR_UT01(void)
{
    const char *key = "74.125.236.84"; //www.google.com
    const char *key1 = "63.150.131.27"; 
    uint32_t key_len = strlen(key), 
	key_len1 = strlen(key1);
    uint32_t resp_len = 1024;
    char resp[1024], resp1[1024];
    int32_t err = 0;

    err =  h_geodb->read(h_geodb, (char*)key, key_len,
			 resp, &resp_len);
    if (err) {
	goto error;
    }
    err = h_geodb->read(h_geodb, (char *)key1, key_len1,
			resp1, &resp_len);
    

    char res[1024];
    err = h_geodb->read(h_geodb, (char *)key, key_len,
			res, &resp_len);
    
 error:
    return err;
}

static int32_t GEODB_MGR_UT02(void)
{
    geodb_ctx_t *gc = NULL;
    char key[256]; 
    geodb_result_cache_elm_t *resp;
    uint32_t key_len, resp_len;
    int32_t err = 0, i;

    err = crst_geodb_db_ctx_create(10, &gc);
    if (err) {
	goto error;
    }
    snprintf(gc->db_file_name, 256, "%s", GEODB_MGR_UT_DB_FILE_NAME);

    err = h_geodb->init(h_geodb, gc);
    if (err) {
	goto error;
    }

    /* build the cache */
    for (i = 0; i < 10; i++) {
	snprintf(key, 256, "74.125.236.%d", i);
	key_len = strlen(key);
	resp = NULL;
	resp = (geodb_result_cache_elm_t *)
	    malloc(sizeof(geodb_result_cache_elm_t));
	resp_len = sizeof(geodb_result_cache_elm_t);
	h_geodb->read(h_geodb, (char *)key, key_len,
		      (char *)resp, &resp_len);
	free(resp);
    }

    /* access the oldest and ensure that it
     * moves to the head of the lru list
     */
    snprintf(key, 256, "74.125.236.0");
    key_len = strlen(key);
    resp = (geodb_result_cache_elm_t *)
	malloc(sizeof(geodb_result_cache_elm_t));
    resp_len = sizeof(geodb_result_cache_elm_t);
    h_geodb->read(h_geodb, key, key_len,
		  (char *)resp, &resp_len);
    free(resp);

    /* eviction test: should evict the 74.125.236.1 */
    snprintf(key, 256, "74.125.236.12");
    key_len = strlen(key);
    resp = NULL;
    resp = (geodb_result_cache_elm_t *)
	malloc(sizeof(geodb_result_cache_elm_t));
    resp_len = sizeof(geodb_result_cache_elm_t);
    h_geodb->read(h_geodb, key, key_len,
		  (char *)resp, &resp_len);
    {
	geodb_ctx_t *g = (geodb_ctx_t *)h_geodb->state_data;
	geodb_result_cache_elm_t *e = (geodb_result_cache_elm_t *)g->dbg_last_evicted_entry;
	assert(e->res.ip == 0x4a7dec01 /*74.125.236.1*/);
	assert(g->num_cache_entries == 11);
    }
    free(resp);

 error:
    return err;
    
}

int32_t main(int argc, char *argv[]) 
{
    int32_t err = 0;
    geodb_ctx_t *gc = NULL;

    err = crst_geodb_db_ctx_create(10, &gc);
    if (err) {
	goto error;
    }
    snprintf(gc->db_file_name, 256, "%s", GEODB_MGR_UT_DB_FILE_NAME);

    err = crst_geodb_store_create(&h_geodb);
    if (err) {
	goto error;
    }
    
   err =  h_geodb->init(h_geodb, gc);
   if (err) {
       goto error;
   }

   err = GEODB_MGR_UT01();
   assert(err == 0);
   err = GEODB_MGR_UT02();
       
   
 error:
    exit(1);    
    
}
#endif

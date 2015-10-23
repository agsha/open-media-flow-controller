/**
 * @file   ds_store.c
 * @author Karthik Narayanan
 * @date   2:33 PM 3/21/2012
 *
 * @brief  entry point for the DNS server daemon.Initializes the
 * modules, loads defaults, starts the DNS server
 *
 *
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#include "ds_hash_table_mgmt.h"
#include "nkn_memalloc.h"
#include "nkn_debug.h"
#include "ds_api.h"
#include "ds_glob_ctx.h"
#include "de_intf.h"
#include "ds_tables.h"
#include "store_errno.h"
#include "common/nkn_ref_count_mem.h"
#include "common/cr_utils.h"

/* Global variables if any*/
extern  ds_glob_ctx_t * ds_glob_ctx;
extern event_timer_t* gl_ev_timer;
extern de_intf_t gl_obj_de_rr;
extern cr_dns_store_cfg_t cr_dns_store_cfg;
de_intf_t gl_obj_de_geo_lf;

/* COUNTERS */
AO_t glob_crst_num_domains_active;
AO_t glob_crst_num_cache_entities_active;

uint64_t glob_crst_lookup_err = 0;
uint64_t glob_crst_de_fail = 0;

/* Function declarations*/
static int32_t cleanup_CE(cache_entity_t *ce);
static int32_t cleanup_POP(pop_t *pop);
static int32_t cleanup_CG(cache_group_t *cg);
static void cleanup_domain(domain_t *domain);

static void free_ref_CE(void *arg);
static void free_ref_POP(void *arg);
static void free_ref_CG(void *arg);
static void free_ref_domain(void *arg);

static cache_group_t* get_cg_from_name(char* cg_name);
static int32_t lookup_hash(hash_tbl_props_t * htable,
                           char* key,
                           int32_t len,
                           void** ret_ptr);
static int32_t lookup_hash_ref(hash_tbl_props_t *htable,
			       char* key,
			       int32_t len,
			       void** ret_ptr);

static int32_t lookup_trie_ref(hash_tbl_props_t *htable,
			       char* key,
			       int32_t len,
			       void** ret_ptr);
static int32_t lookup_trie(hash_tbl_props_t *htable,
			   char* key,
			   int32_t len,
			   void** ret_ptr);

static int32_t CE_lf_update(void *arg, const 
			    lf_stats_t *const stats,
			    uint32_t stats_size);
static inline int32_t CE_lf_create(ref_count_mem_t *ref);
static inline int32_t CE_lf_construct_uri(cache_entity_t *ce, 
			  char *fqdn, uint32_t fqdn_len);
static int32_t create_new_domain(char *domain_name, 
				 domain_t **out);
static int32_t static_route_list_cleanup(
		 struct ip_addr_range_list_t *head);
/*===============================
   Data plane access commands
===============================*/


static int32_t searchCR_Store(char const* dname, char const* src_ip,
	      cache_entity_addr_type_t qtype, domain_t* domain_ctx, 
	      char** result, uint32_t *result_size); 

static int32_t lookup_dns(char const* dns, char const* ip, void** domain_ds){
    hash_tbl_props_t* dns_htable = NULL;
    int32_t ret;
    char* rev_str;
    ref_count_mem_t *ref = NULL;

    rev_str = strrev((char *)dns);
    dns_htable  = ds_glob_ctx->domainname_domainptr;
    ret =  lookup_trie(dns_htable,
			   rev_str,
			   strlen(dns),
			   (void **)&ref);
    *domain_ds = ref;
    free(rev_str);
    return ret;
}


/*Leaf level module to hash lookup hash and return*/
static int32_t lookup_hash(hash_tbl_props_t * htable,
			   char* key,
			   int32_t len,
			   void** ret_ptr){
    ds_hash_table_t* obj;
    obj = ds_hash_table_mgmt_find(key, htable);
    if (obj != NULL)
	*ret_ptr = (void*)(obj->val_ptr);
    else
	*ret_ptr = NULL;
    return  0;
}

static int32_t lookup_hash_ref(hash_tbl_props_t *htable,
			       char* key,
			       int32_t len,
			       void** ret_ptr){
    ds_hash_table_t* obj;
    
    obj = ds_hash_table_mgmt_find(key, htable);
    if (obj != NULL) {
	ref_count_mem_t *ref = NULL;
	*ret_ptr = (void*)(obj->val_ptr);
	ref = (ref_count_mem_t *)(*ret_ptr);
	ref->hold_ref_cont(ref);
    }
    else
	*ret_ptr = NULL;
    return  0;
}

static int32_t lookup_trie(hash_tbl_props_t *htable,
			   char* key,
			   int32_t len,
			   void** ret_ptr){
    ds_hash_table_t* obj;
    
    obj = ds_trie_mgmt_find(key, htable);
    if (obj != NULL) {
	REL_TBL_OBJ(obj);
	*ret_ptr = obj->val_ptr;
    }
    else
	*ret_ptr = NULL;
    return  0;
}

static int32_t lookup_trie_ref(hash_tbl_props_t *htable,
			       char* key,
			       int32_t len,
			       void** ret_ptr){
    ds_hash_table_t* obj;
    
    obj = ds_trie_mgmt_find(key, htable);
    if (obj != NULL) {
	ref_count_mem_t *ref = NULL;
	*ret_ptr = (void*)(obj->val_ptr);
	ref = (ref_count_mem_t *)(*ret_ptr);
	ref->hold_ref_cont(ref);
	REL_TBL_OBJ(obj);
    }
    else
	*ret_ptr = NULL;
    return  0;
}

/*===============================
   Command plane access commands
 ===============================*/
/*=== Cache Group APIs ===*/

int32_t create_new_CG(char* name){

    if (name == NULL) {
	DBG_LOG(MSG, MOD_CRST, "create_new_CG: Called with NULL\n");
	return 0;
    }
    //Populate CG structure
    cache_group_t *cg =  NULL;
    hash_tbl_props_t*  ds_cg;
    int32_t rv = 0;
    ref_count_mem_t *ref = NULL;

    cg = ( cache_group_t*) 
	nkn_calloc_type(1, sizeof(cache_group_t), mod_cr_ds);
    if (!cg) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM when adding a CG %s", name);
	goto error;
    }
    cg->name = (char *) 
	nkn_calloc_type(1,sizeof(char)* (strlen(name)+1),  mod_cr_ds);
    if (!cg->name) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM when adding a CG %s", name);
	goto error;
    }
    cg->attributes = (hash_tbl_props_t*) 
	nkn_calloc_type(1, sizeof(hash_tbl_props_t),  mod_cr_ds);
    if (!cg->attributes) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM when adding a CG %s", name);
	goto error;
    }
    ds_hash_table_mgmt_init(cg->attributes);
    strncpy(cg->name, name, strlen(name));

    ref = createRefCountedContainer(cg, free_ref_CG);
    if (!ref) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM when adding a CG %s", name);
	goto error;
    }

    ref->hold_ref_cont(ref);
    ds_cg = ds_glob_ctx->CGname_CGptr;
    ds_cont_write_lock(ds_cg);
    rv = ds_hash_table_mgmt_add(ref, cg->name, ds_cg);
    if(rv != 0) {
	rv = -E_CG_DUP;
	DBG_LOG(ERROR, MOD_CRST, "Duplicate CG %s [err=%d]", name, rv);
	ds_cont_write_unlock(ds_cg);
	goto error;
    }
    ds_cont_write_unlock(ds_cg);
    return rv;

 error:
    cleanup_CG(cg);
    free(ref);
    return rv;
}

static void free_ref_CG(void *arg) {
    
    cache_group_t *cg = NULL;
    
    cg = (cache_group_t *) arg;
    cleanup_CG(cg);
}

static int32_t cleanup_CG(cache_group_t *cg) {
    
    if (cg) {
	if (cg->name) free(cg->name);
	if (cg->attributes) {
	    ds_hash_table_mgmt_deinit(cg->attributes);
	}
	GList *list = NULL;

	list = cg->ce_ptr_list;
	if (list) {
	    assert(g_list_length(list) == 0);
	    g_list_free(list);
	}
	list = cg->domain_list;
	if (list) {
	    assert(g_list_length(list) == 0);
	    g_list_free(list);
	}
	free(cg);
    }
    return 0;
}
    
   
/* 
*********************************
Notes:
 - Assumes that CE is already hashed. 
 - 
Return Values: 
 - -E_CE_ABSENT: if CE is not created.
 - 0: normal 
**********************************
*/


int32_t add_CE_to_CG(char* cg_name,
		     char* ce_name){

    int32_t ret = 0;
    cache_group_t *cg;
    cache_entity_t* ce = NULL;
    ref_count_mem_t *ref = NULL;

    if (!cg_name || !ce_name) {
	DBG_LOG(MSG, MOD_CRST, "CG name or CE Name is NULL, "
		"nothing to do");
	return 0;
    }
    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    /*Get CG*/
    ds_cont_write_lock(ds_cg);
    cg = get_cg_from_name(cg_name);
    if(cg==NULL){
	DBG_LOG(ERROR, MOD_CRST, "CG name: %s not found", 
		cg_name);
	ds_cont_write_unlock(ds_cg);
	return -E_CG_ABSENT;
    }

    /*Get  CE from Hash table*/
    ds_cont_write_lock(ds_ce);
    lookup_hash(ds_ce, ce_name, 0,(void**)&ref);
    if(ref == NULL){
	DBG_LOG(ERROR, MOD_CRST, "CG name: %s not found", 
		ce_name);
	ds_cont_write_unlock(ds_ce);
	ds_cont_write_unlock(ds_cg);
	return -E_CE_ABSENT;
    }
    ce = ref->mem;

    /*Update CE in CG and vice versa*/
    cg->ce_ptr_list = g_list_append(cg->ce_ptr_list, ce);
    ce->cg_ptr_list = g_list_append(ce->cg_ptr_list, cg);
    ds_cont_write_unlock(ds_ce);
    ds_cont_write_unlock(ds_cg);
    return 0;
}


int32_t del_CE_from_CG(char* cg_name, char* ce_name){

    int32_t ret = 0;
    cache_group_t *cg;
    cache_entity_t* ce = NULL;
    ref_count_mem_t *ref = NULL;

    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    ds_cont_write_lock(ds_cg);
    /*Get CG*/
    cg = get_cg_from_name(cg_name);
    if(cg==NULL){
	DBG_LOG(ERROR, MOD_CRST, "CG name: %s not found",
		cg_name);
	ds_cont_write_unlock(ds_cg);
        return -E_CG_ABSENT;
    }

    /*Get CE from hash Table*/
    ds_cont_write_lock(ds_ce);
    lookup_hash(ds_glob_ctx->CEname_CEptr,
                ce_name, 0,(void**)&ref);
    if(ref == NULL){
	DBG_LOG(ERROR, MOD_CRST, "CE name: %s not found",
		ce_name);
	ds_cont_write_unlock(ds_ce);
	ds_cont_write_unlock(ds_cg);
        return -E_CE_ABSENT;
    }
    ce = ref->mem;

    /*Update CE in CG and vice versa*/
    cg->ce_ptr_list = g_list_remove(cg->ce_ptr_list, ce);
    ce->cg_ptr_list = g_list_remove(ce->cg_ptr_list, cg);
    ds_cont_write_unlock(ds_ce);
    ds_cont_write_unlock(ds_cg);
    return 0;
}


static cache_group_t * get_cg_from_name(char* cg_name){

    hash_tbl_props_t*  hash_cg;
    int32_t rv = 0;
    cache_group_t *cg = NULL;
    ref_count_mem_t *ref = NULL;

    hash_cg = ds_glob_ctx->CGname_CGptr;
    lookup_hash(hash_cg,cg_name,strlen(cg_name),(void**)&ref);
    if (ref) {
	cg = ref->mem;
    }

    return cg;
}


int32_t set_attr_CG(char* cg_name, char* attr, void* val){

    cache_group_t *cg;
    /*Get CG*/
    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    ds_cont_write_lock(ds_cg);
    cg = get_cg_from_name(cg_name);
    if(cg==NULL){
        printf("CE does not EXIST");
	ds_cont_write_unlock(ds_cg);
        return -E_CG_ABSENT;
    }
    ds_hash_table_mgmt_alter(val, attr, cg->attributes);
    ds_cont_write_unlock(ds_cg);
    return 0;
}


int32_t del_CG(char* cg_name){

    int32_t rc = 0;
    ref_count_mem_t *ref = NULL;
    cache_group_t *cg = NULL;

    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    ds_cont_write_lock(ds_cg);
    lookup_hash(ds_cg, cg_name, 0 ,(void**)&ref);
    if (!ref) {
	DBG_LOG(ERROR, MOD_CRST, "CG %s not found, nothing to delete",
		cg_name);
	goto error;
    }
    cg = ref->mem;
    if ((g_list_first(cg->ce_ptr_list) == NULL) &&
	    (g_list_first(cg->domain_list) == NULL)) {
	ds_hash_table_mgmt_remove(cg_name, ds_cg);
	ref->release_ref_cont(ref);/* release the config plane token */
    } else {
	DBG_LOG(ERROR, MOD_CRST, "CG %s has CE or Domain links",
		cg_name);
	rc = -1;
    }
 error:
    ds_cont_write_unlock(ds_cg);
    return rc;
}



/*=== Cache Entity APIs ===*/
/** 
 * Adds/Updates a Cache Entity. This function can be called once with
 * all the parameters set or multiple times with any combination of
 * parameters set (typically happens when we receive inputs from
 * configuration plane). Checks if the cache entity exists in
 * the CE HasH Table and updates the exisiting CE with a new set of
 * attributes. It the CE is not found in the hash table then it
 * creates a new entry to be added to the hash table. the 'add_flag'
 * variable distinguishes this
 * 
 * When updating the cache entity, we hold a reference to the CE to
 * avoid data races between the config plane (current thread) and the
 * data plane.
 *
 * AS a part of the CE creation, we also initialize and start an
 * instance of the LF client to constantly update the cache statistics
 * for the cache entity
 *
 * Object Sharing
 * ==============
 * The CE object is shared between the config thread, data plane
 * thread if it is being accessed and the LF update thread. To
 * facilitate this, the CE is wrapped in a reference counted memory
 * structure and shared between the various threads.
 * 
 * @param ce_name - cache entity name a logical identifier for the cache
 * @param ce_address  - IP address of the type @ce_addr_type (IPv4,
 * IPv6, FQDN)
 * @param ce_addr_type - see above
 * @param lf_method - only XML is accepted for now
 * @param lf_poll_freq - polling frequency to poll and update from the
 * cache, defaults to 5s
 * @param lf_port - the port in which the LF client needs to operate,
 * defaiults to 2010
 * @param ce_load_watermark - the load threshold at which we mark the
 * cache as 'down'
 * 
 * @return 
 */
int32_t create_new_CE(const char* ce_name, 
		      const char *ce_address, 
		      cache_entity_addr_type_t ce_addr_type, 
		      uint8_t lf_method, 
		      uint32_t lf_poll_freq, 
		      uint32_t lf_port,
		      uint32_t ce_load_watermark){

    cache_entity_t* ce = NULL;
    hash_tbl_props_t* ds_ce;
    int32_t rv;
    char ipchar[] = "ip_address";
    ref_count_mem_t *ref = NULL;
    uint8_t add_flag = 0;

    ds_ce = ds_glob_ctx->CEname_CEptr;
    ds_cont_write_lock(ds_ce);
    rv = lookup_hash(ds_ce, (char *)ce_name, strlen(ce_name), 
		     (void**)&ref); 

    if (!ref) {
	DBG_LOG(MSG, MOD_CRST, "Adding new CE %s", ce_name);
	add_flag = 1;

	pthread_mutex_lock(&cr_dns_store_cfg.lock);
	if (AO_load(&glob_crst_num_cache_entities_active) >= 
	    cr_dns_store_cfg.max_cache_entities) {
	    rv = -E_MAX_CE_LIMIT_HIT;
	    DBG_LOG(ERROR, MOD_CRST, "Reached maximum configured"
		    " cache entity count of %d", 
		    cr_dns_store_cfg.max_cache_entities);
	    pthread_mutex_unlock(&cr_dns_store_cfg.lock);
	    goto error;
	}
	pthread_mutex_unlock(&cr_dns_store_cfg.lock);

	ce = (cache_entity_t*) 
	    nkn_calloc_type(1,sizeof(cache_entity_t), mod_cr_ds);
	if (!ce) {
	    rv = -ENOMEM;
	    goto error;
	}

	/* initialize CE */
	pthread_mutex_init(&ce->lock,NULL);
	strncpy(ce->name, ce_name, strlen(ce_name));
	ce->attributes = (hash_tbl_props_t*) 
	    nkn_calloc_type(1, sizeof(hash_tbl_props_t),  mod_cr_ds);
	ce->state = CE_ACTIVE;
	ce->load_watermark = 90;
	ce->lf_attr.port = 2010;
	ce->lf_attr.poll_freq = 5000;
	ds_hash_table_mgmt_init( ce->attributes);

	ref = createRefCountedContainer(ce, free_ref_CE);
	if (!ref) { 
	    rv = -ENOMEM;
	    DBG_LOG(ERROR, MOD_CRST, "OOM Error creating "
		    "new CE [err=%d]", rv);
	    goto error;
	}
	ref->hold_ref_cont(ref);
    } else {
	ce = ref->mem;
    }

    /* add or update */
    if (ce_address) {
	if (ce_addr_type && ce_addr_type < ce_addr_type_max)  {
	    ce->addr_type = ce_addr_type;
	} else {	 
	    rv = -EINVAL;
	    DBG_LOG(ERROR, MOD_CRST, "Invalid address type for cache "
		    "entity [err=%d]", rv);
	    goto error;
	}
	if (ce_addr_type == ce_addr_type_cname) {
	    if (strlen(ce_address)) {
		if (ce_address[0] == '.') {
		    ce->addr_len[ce_addr_type] =
			snprintf(ce->addr[ce->addr_type], 256,
				"%s", ce_address);
		} else {
		    ce->addr_len[ce_addr_type] =
			snprintf(ce->addr[ce->addr_type], 256,
				".%s", ce_address);
		}
	    } else {
		ce->addr_len[ce->addr_type] = 0;
		ce->addr[ce->addr_type][0] = '\0';
	    }
	} else {
	    ce->addr_len[ce_addr_type] = 
		snprintf(ce->addr[ce->addr_type], 256, "%s", ce_address);
	}
	if (ce->lfc) {
	    char fqdn[256] = {0};
	    rv = CE_lf_construct_uri(ce, fqdn, 256);
	    if (!rv) {
		ce->lfc->set_uri(ce->lfc, fqdn);
	    }
	} else {
	    rv = CE_lf_create(ref);
	    if (!rv) {
		ref->hold_ref_cont(ref);
	    }
	}
    }

    /* if there is an existing LFC then update the polling frequency
     * else simply create a new one with the polling frequency entered
     */
    if (lf_poll_freq) {
	ce->lf_attr.poll_freq = lf_poll_freq * 1000;//convert to ms
	if (ce->lfc) {
	    ce->lfc->set_update_interval(ce->lfc,
			 ce->lf_attr.poll_freq);
	} else {
	    rv = CE_lf_create(ref);
	    if (!rv) {
		ref->hold_ref_cont(ref);
	    }
	}
    }	    

    /* if there is an existing LFC then update the LFC port
     * else simply create a new one with the LFC port entered
     */
    if (lf_port) {
	ce->lf_attr.port = lf_port;
	if (ce->lfc) {
	    char fqdn[256] = {0};
	    rv = CE_lf_construct_uri(ce, fqdn, 256);
	    if (!rv) {
		ce->lfc->set_uri(ce->lfc, fqdn);
	    }
	} else {
	    rv = CE_lf_create(ref);
	    if (!rv) {
		ref->hold_ref_cont(ref);
	    }
	}
    }
    if (ce_load_watermark) ce->load_watermark = ce_load_watermark;

    /* create a new LF client if the LF method is XML. do
     * nothing otherwise 
     */
    if (lf_method) {
	if (lf_method > 2) {
	    rv = -E_CE_UNSUPP_LF;
	    DBG_LOG(ERROR, MOD_CRST, "CE %s with unsupported LF Method"
		    " - Only XML supported [err=%d]", ce_name, rv);
	    goto error;
	}
	if (!ce->lfc) {
	    rv = CE_lf_create(ref);
	    if (rv) {
		ref->hold_ref_cont(ref);
	    }
	}
    }

    if (add_flag) {
	/*Add CE to its hash table*/
	rv = ds_hash_table_mgmt_add(ref, (char *)ce_name, ds_ce);
	if(rv!=0) {
	    DBG_LOG(ERROR, MOD_CRST, "CE name: %s already present",
		    ce->name);
	    rv = -E_CE_DUP;
	    goto error;
	}
	AO_fetch_and_add1(&glob_crst_num_cache_entities_active);
    }
    ds_cont_write_unlock(ds_ce);
    return rv;

 error:
    if (!add_flag) {
	ds_hash_table_mgmt_remove((char *)ce_name, ds_ce);
    }
    ds_cont_write_unlock(ds_ce);
    cleanup_CE(ce);
    return rv;
}

static void free_ref_CE(void *arg) {

    cache_entity_t *ce = NULL;

    ce = (cache_entity_t *)arg;
    cleanup_CE(ce);
    
    return;
}

static int32_t cleanup_CE(cache_entity_t *ce) {

    if (ce) {
	if (ce->attributes) free(ce->attributes);
	if (ce->lfc) ce->lfc->set_state(ce->lfc, LF_CLIENT_STOP);
	free(ce);
    }
    AO_fetch_and_sub1(&glob_crst_num_cache_entities_active);
    
    return 0;
}    

static inline int32_t CE_lf_construct_uri(cache_entity_t *ce, 
			  char *fqdn, uint32_t fqdn_len) {

    cache_entity_addr_type_t lf_use_addr_type = ce_addr_type_none;
    int32_t rv = 0;

    if (ce->addr[ce_addr_type_ipv4][0] == '\0' &&
	ce->addr[ce_addr_type_cname][0] == '\0') {
	rv = -E_CE_NO_IPV4_CNAME_ADDR_TYPE;
	DBG_LOG(ERROR, MOD_CRST, "Error trying to start LF client" 
		" either IPv4 or CNAME should be configured for "
		" CE %s [err=%d]", ce->name, rv);
	goto error;
    }
	
    if (ce->addr[ce_addr_type_ipv4][0] != '\0') {
	lf_use_addr_type = ce_addr_type_ipv4;
    } else { 
	lf_use_addr_type = ce_addr_type_cname;
    }

    snprintf(fqdn, fqdn_len, "http://%s:%d/lf", 
	     ce->addr[lf_use_addr_type], 
	     ce->lf_attr.port);
 error:
    return rv;
}
    
static inline int32_t CE_lf_create(ref_count_mem_t *ref) {

    char fqdn[256] = {0};
    int32_t rv = 0;
    cache_entity_t *ce = ref->mem;
    
    rv = CE_lf_construct_uri(ce, fqdn, 256);
    if (rv) {
	goto error;
    }

    /* create an LF client */
    rv = lf_client_create(ce->lf_attr.poll_freq, CE_lf_update,
			  ref, fqdn, ce->lf_attr.port,
			  gl_ev_timer, &ce->lfc);
    if (rv) {
	goto error;
    }

    ce->lfc->set_state(ce->lfc, LF_CLIENT_RUNNING);

 error:
    return rv;

}

static int32_t CE_lf_update(void *arg, const lf_stats_t *const stats,
			    uint32_t stats_size) {

    ref_count_mem_t *ref = NULL;
    cache_entity_t *ce = NULL;
    int32_t err = 0;

    ref = (ref_count_mem_t *) arg;
    ce = ref->mem;
    
    switch (ce->state) {
	case CE_ACTIVE:
	    memcpy(&ce->fb, stats, sizeof(lf_stats_t));
	    break;
	case CE_DEAD:
	    ce->lfc->set_state(ce->lfc, LF_CLIENT_STOP);
	    ref->release_ref_cont(ref);
	    break;
    }
    return err;

}
    
int32_t set_attr_CE(char* ce_name, char* attr, void* val){
    cache_entity_t* ce;
    ref_count_mem_t *ref = NULL;
    int32_t rv;
    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    ds_cont_write_lock(ds_ce);
    /*Get  CE from Hash table*/
    lookup_hash(ds_glob_ctx->CEname_CEptr,ce_name, 0,(void**)&ref);
    if(ref==NULL){
	DBG_LOG(ERROR, MOD_CRST, "CE does not exisit");
	ds_cont_write_unlock(ds_ce);
        return -E_CE_ABSENT;
    }
    ce = ref->mem;
    rv = ds_hash_table_mgmt_alter(val, attr, ce->attributes);
    ds_cont_write_unlock(ds_ce);
    return 0;
}



int32_t del_CE(char* ce_name){
    /*Remove CE from CG*/
    //del_CE_from_CG(cg_name, ce_name);
    
    /*Remove CE from POP*/
    //add_CE_to_POP(char* pop_name, char* ce_name)

    /*Delete CE*/
    //cleanup_CE(ce);

    int32_t rc = 0;
    ref_count_mem_t *ref = NULL;
    cache_entity_t *ce = NULL;

    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    ds_cont_write_lock(ds_ce);
    lookup_hash(ds_glob_ctx->CEname_CEptr,
		ce_name, 0 ,(void**)&ref);
    if (!ref) {
	DBG_LOG(ERROR, MOD_CRST, "CE %s not found, nothing to delete",
		ce_name);
	goto error;
    }
    ce = ref->mem;
    if ((g_list_first(ce->cg_ptr_list) == NULL) && (ce->pop == NULL)) {
	ds_hash_table_mgmt_remove(ce_name, ds_ce);
	ref->release_ref_cont(ref);/* release the config plane token */
    } else {
	DBG_LOG(ERROR, MOD_CRST, "CE %s has CG or POP links",
		ce_name);
	rc = -1;
    }

 error:
    ds_cont_write_unlock(ds_ce);
    return rc;
}

/*=== Point of Presence PoP APIs ===*/


int32_t create_new_POP(char* pop_name,
		       char* latitude,
		       char* longitude){

    pop_t * pop;
    hash_tbl_props_t* ds_pp;
    ref_count_mem_t *ref = NULL;
    int32_t rv;
    uint8_t add_flag = 0;

    if (!pop_name) {
	rv = -EINVAL;
	DBG_LOG(ERROR, MOD_CRST, "POP name is NULL [err=%d]", rv);
	return rv;
    }

    ds_pp = ds_glob_ctx->POPname_POPptr;
    ds_cont_write_lock(ds_pp);
    pop = NULL;
    lookup_hash(ds_pp, pop_name, 0, (void**)&ref);
    if (ref) {
       	DBG_LOG(MSG, MOD_CRST, "POP %s already present, updating "
		"params", pop_name);
	pop = ref->mem;
    } else {
	DBG_LOG(MSG, MOD_CRST, "Adding new POP %s", pop_name);
	add_flag = 1;
	pop = (pop_t*) nkn_calloc_type(1,sizeof(pop_t), mod_cr_ds);
	if (!pop) {
	    rv = -ENOMEM;
	    DBG_LOG(ERROR, MOD_CRST, "Error adding POP %s [err=%d]",
		    pop_name, rv);
	    goto error;
	}
	pop->name = (char *) 
	    nkn_calloc_type(1,sizeof(char) * (strlen(pop_name)+1),
			    mod_cr_ds);
	if (!pop->name) {
	    rv = -ENOMEM;
	    DBG_LOG(ERROR, MOD_CRST, "Error adding POP %s [err=%d]",
		    pop_name, rv);
	    goto error;
	}

	strncpy(pop->name, pop_name, strlen(pop_name));
	pthread_mutex_init(&pop->lock,NULL);
	pop->attributes = (hash_tbl_props_t*) 
	    nkn_calloc_type(1, sizeof(hash_tbl_props_t),  mod_cr_ds);
	ds_hash_table_mgmt_init(pop->attributes);
	ref = createRefCountedContainer(pop, free_ref_POP);
	if (!ref) {
	    rv = -ENOMEM;
	    DBG_LOG(ERROR, MOD_CRST, "Error adding POP %s [err=%d]",
		    pop_name, rv);
	    goto error;
	}
	ref->hold_ref_cont(ref);
    } 

    if (latitude) pop->location.latitude = atof(latitude);
    if (longitude) pop->location.longitude = atof(longitude);

    if (add_flag) {
	/*Add POP to its hash table*/
	rv = ds_hash_table_mgmt_add(ref, pop->name, ds_pp);
	if(rv!=0) {
	    rv = -E_POP_DUP;
	    DBG_LOG(ERROR, MOD_CRST, "Trying to add a duplicate POP %s", 
		    pop_name);
	goto error;
	}
    }
  
    ds_cont_write_unlock(ds_pp);
    return rv;

 error:
    ds_cont_write_unlock(ds_pp);
    if (pop) {
	cleanup_POP(pop);
    }
    return rv;
}


int32_t map_CE_to_POP(char* pop_name, char* ce_name){

    ref_count_mem_t *ref_ce = NULL, *ref_pop = NULL;
    cache_entity_t* ce;
    int32_t rv;
    pop_t * pop;

    if (!pop_name || !ce_name) {
	DBG_LOG(MSG, MOD_CRST, "POP name of CE name is NULL "
		"nothing to do");
	return 0;
    }
    hash_tbl_props_t* ds_pp = ds_glob_ctx->POPname_POPptr;
    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    ds_cont_write_lock(ds_ce);
    /*Get  CE from Hash table*/
    lookup_hash(ds_glob_ctx->CEname_CEptr, 
		ce_name, 0,(void**)&ref_ce);
    if(ref_ce==NULL){
	rv = -E_CE_ABSENT;
	DBG_LOG(ERROR, MOD_CRST, "Error +(CE(%s)->POP(%s)) [err=%d]",
		ce_name, pop_name, rv);
	ds_cont_write_unlock(ds_ce);
        return rv;
    }
    ce = ref_ce->mem;
    if (ce->pop != NULL) {
	rv = -E_CE_POP_EXISTS;
	DBG_LOG(ERROR, MOD_CRST, "Error +(CE(%s)->POP(%s)) [err=%d]",
		ce_name, pop_name, rv);
	ds_cont_write_unlock(ds_ce);
	return rv;
    }

    ds_cont_write_lock(ds_pp);
    /*Get POP from hash table*/
    lookup_hash(ds_glob_ctx->POPname_POPptr, 
		pop_name, 0 ,(void**)&ref_pop);
    if(ref_pop==NULL){
	rv = -E_POP_ABSENT;
	DBG_LOG(ERROR, MOD_CRST, "Error +(CE(%s)->POP(%s)) [err=%d]",
		ce_name, pop_name, rv);
	ds_cont_write_unlock(ds_pp);
	ds_cont_write_unlock(ds_ce);
	return rv;
    }
    pop = ref_pop->mem;	

    /*Add CE to pop list*/
    /* TO DO: What happens when a CE is being deleted in another thread?
     * At this point this condition is moot since the config plane
     * operations happen sequentially on a SINGLE thread
     */
    pop->ce_ptr_list = g_list_append(pop->ce_ptr_list, ce);
    ce->pop = pop;

    ds_cont_write_unlock(ds_pp);
    ds_cont_write_unlock(ds_ce);
    return 0;
}


int32_t del_CE_from_POP(char* pop_name, char* ce_name){

    ref_count_mem_t *ref_ce = NULL, *ref_pop = NULL;
    cache_entity_t* ce;
    int32_t rv;
    pop_t * pop;

    hash_tbl_props_t* ds_pp = ds_glob_ctx->POPname_POPptr;
    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    ds_cont_write_lock(ds_ce);
    /*Get  CE from Hash table*/
    lookup_hash(ds_glob_ctx->CEname_CEptr, 
		ce_name, 0,(void**)&ref_ce);
    if(ref_ce==NULL){
	rv = -E_CE_ABSENT;
	DBG_LOG(ERROR, MOD_CRST, "Error -(CE(%s)->POP(%s)) [err=%d]",
		ce_name, pop_name, rv);
	ds_cont_write_unlock(ds_ce);
        return rv;
    }

    ds_cont_write_lock(ds_pp);
    /*Get POP from hash table*/
    lookup_hash(ds_pp, pop_name, 0 ,(void**)&ref_pop);
    if(ref_pop==NULL){
	rv = -E_POP_ABSENT;
	DBG_LOG(ERROR, MOD_CRST, "Error -(CE(%s)->POP(%s)) [err=%d]",
		ce_name, pop_name, rv);
	ds_cont_write_unlock(ds_pp);
	ds_cont_write_unlock(ds_ce);
	return rv;
    }
    ce = ref_ce->mem;
    pop = ref_pop->mem;

    pop->ce_ptr_list= g_list_remove(pop->ce_ptr_list, ce);
    ce->pop = NULL;
    
    ds_cont_write_unlock(ds_pp);
    ds_cont_write_unlock(ds_ce);
    return 0;
}


int32_t set_attr_POP(char* pop_name, char* attr, void* val){

    hash_tbl_props_t* ds_pp = ds_glob_ctx->POPname_POPptr;
    ds_cont_write_lock(ds_pp);
    pop_t * pop;
    /*Get POP from hash table*/
    lookup_hash(ds_pp, pop_name, 0 ,(void**) &pop);
    if(pop==NULL){
        printf("POP does not EXIST");
	ds_cont_write_unlock(ds_pp);
        return -E_POP_ABSENT;
    }
    
    ds_hash_table_mgmt_alter(val, attr, pop->attributes);
    ds_cont_write_unlock(ds_pp);
    return 0;

}

int32_t del_POP(char* pop_name) {

    ref_count_mem_t *ref = NULL;
    pop_t *pop; 

    hash_tbl_props_t* ds_pp = ds_glob_ctx->POPname_POPptr;
    ds_cont_write_lock(ds_pp);
    pop = NULL;
    lookup_hash(ds_glob_ctx->POPname_POPptr, 
		pop_name, 0 ,(void**)&ref);
    if (!ref) {
	DBG_LOG(ERROR, MOD_CRST, "POP %s not found, nothing to delete",
		pop_name);
	goto error;
    }
    pop = ref->mem;
    if (g_list_first(pop->ce_ptr_list) != NULL) {

	DBG_LOG(ERROR, MOD_CRST, "POP : CE list not empty", pop_name);
	goto error;
    }
    ds_hash_table_mgmt_remove(pop_name, ds_pp);
    ref->release_ref_cont(ref);
        
 error:
    ds_cont_write_unlock(ds_pp);
    return 0;
}

int32_t
cleanup_POP(pop_t *pop) {

    if (pop) {
	if (pop->name) free(pop->name);
	pthread_mutex_destroy(&pop->lock);
	free(pop);	
    }
    return 0;
}

void 
free_ref_POP(void *arg) {
    
    pop_t *pop = NULL;
    
    pop = (pop_t *)arg;
    cleanup_POP(pop);

}

/*Domain related APIs*/
int32_t update_attr_domain(char* domain_name, 
			   const char *const last_resort,
			   uint8_t routing_type,
			   uint32_t static_rid,
			   const char *const static_ip,
			   const char *const static_ip_end,
			   double geo_weight, 
			   double load_weight, 
			   uint32_t ttl) {

    domain_t * domain = NULL;
    hash_tbl_props_t* ds_dn;
    int32_t rv=0;
    char* rev_str = NULL;
    ref_count_mem_t *ref = NULL;
    uint8_t add_flag = 0;

    rev_str = strrev(domain_name);
    ds_dn = ds_glob_ctx->domainname_domainptr;
    ds_cont_write_lock(ds_dn);
    rv = lookup_trie(ds_dn, (char *)rev_str, 
		     strlen(rev_str), (void**)&ref); 

    if (ref) {
	domain = ref->mem;
	if (domain->wildcard) {
	    char tmp_str[256];
	    snprintf(tmp_str, 256, "%s*", domain->name);
	    if (strcmp(tmp_str, rev_str)) {
		ref = NULL;
	    }
	}
    }

    /* create a new domain with defaults */
    if (!ref) {
	add_flag = 1;

	/* check max domain limit */
	pthread_mutex_lock(&cr_dns_store_cfg.lock);
	if (AO_load(&glob_crst_num_domains_active) >= 
	    cr_dns_store_cfg.max_domains) {
	    rv = -E_MAX_DOMAIN_LIMIT_HIT;
	    DBG_LOG(ERROR, MOD_CRST, "Reached maximum configured"
		    " domain count of %d", 
		    cr_dns_store_cfg.max_domains);
	    pthread_mutex_unlock(&cr_dns_store_cfg.lock);
	    goto error;
	}
	pthread_mutex_unlock(&cr_dns_store_cfg.lock);

	rv = create_new_domain(rev_str, &domain);
	if (rv) {
	    goto error;
	}
	ref = createRefCountedContainer(domain, free_ref_domain);
	if (!ref) {
	    rv = -ENOMEM;
	    DBG_LOG(ERROR, MOD_CRST, "Adding new domain %s OOM "
		    "error [err=%d]", rev_str, rv);
	    goto error;
	}
    } else {
	domain = ref->mem;
    }
   
    if (last_resort) {
	if (strlen(last_resort)) {
	    if (last_resort[0] == '.') {
		domain->cname_last_resort_len =
		    snprintf(domain->cname_last_resort,
			    256, "%s", last_resort);
	    } else {
		domain->cname_last_resort_len =
		    snprintf(domain->cname_last_resort,
			    256, ".%s", last_resort);
	    }
	} else {
	    domain->cname_last_resort_len = 0;
	    domain->cname_last_resort[0] = '\0';
	}
    }

    if (routing_type) {
	domain->rp.type = routing_type;
	/* clean old de if any */
	if (domain->de_state) {
	    domain->de_cb->cleanup(domain->de_state);
	    domain->de_state = NULL;
	    domain->de_cb = NULL;
	}
	if (domain->rp.type == ROUTING_TYPE_STATIC) {
	    static_routing_t *sr = &domain->rp.p.sr;
	    /* cleanup existing routes if any */
	    DBG_LOG(MSG, MOD_CRST, "This will clear all"
		    " existing static routes configured");
	    static_route_list_cleanup(&sr->addr_list);
	    TAILQ_INIT(&sr->addr_list);
	} else if(domain->rp.type == ROUTING_TYPE_RR) {
	    rv = gl_obj_de_rr.init(&domain->de_state);
	    if (rv) {
		DBG_LOG(ERROR, MOD_CRST, "Error creating "
			"DE instance for domain name %s" 
			"[err=%d]", domain->name, rv);
		goto error;
	    }
	    domain->de_cb = &gl_obj_de_rr;
	}
    }

    if (static_ip) {
	if (domain->rp.type != ROUTING_TYPE_STATIC) {
	    rv = -E_DOMAIN_INVALID_ROUTING_POLICY;
	    DBG_LOG(ERROR, MOD_CRST, "Domain %s Error incompatible "
		    "routing type %d for which static ip "
		    "is being set", domain->name, 
		    domain->rp.type);
	    goto error;
	}
	
	struct tag_ip_addr_range_list_elm *elm = NULL;
	static_routing_t *sr = &domain->rp.p.sr;	
	uint8_t new_static_route = 0;
	if (TAILQ_EMPTY(&sr->addr_list)) {
	    elm = (struct tag_ip_addr_range_list_elm *)
	    nkn_calloc_type(1, sizeof(struct tag_ip_addr_range_list_elm),
			    mod_cr_ds);
	    if (!elm) {
		rv = -ENOMEM;
		DBG_LOG(ERROR, MOD_CRST, "Adding new domain %s OOM "
			"error [err=%d]", rev_str, rv);
		goto error;
	    }
	    new_static_route = 1;
	} else { 
	    /* right now we assume only one entry here, we should use
	     * route identifier UID based search to determine,
	     * which static route to use */
	    elm = TAILQ_FIRST(&sr->addr_list);
	}
	char *p = strdup(static_ip);
	uint32_t tmp;
	struct in_addr start_addr;

	inet_pton(AF_INET, p, &start_addr);
	tmp = ntohl(start_addr.s_addr);
	elm->addr_range.start_ip = tmp;
       	if (new_static_route)
	    TAILQ_INSERT_HEAD(&sr->addr_list, elm, entries);
	free(p);
    }
    if (static_ip_end) {
	if (domain->rp.type != ROUTING_TYPE_STATIC) {
	    rv = -E_DOMAIN_INVALID_ROUTING_POLICY;
	    DBG_LOG(ERROR, MOD_CRST, "Domain %s Error incompatible "
		    "routing type %d for which static ip "
		    "is being set", domain->name, 
		    domain->rp.type);
	    goto error;
	}
	struct tag_ip_addr_range_list_elm *elm = NULL;
	static_routing_t *sr = &domain->rp.p.sr;
	uint8_t new_static_route = 0;
	if (TAILQ_EMPTY(&sr->addr_list)) {
	    elm = (struct tag_ip_addr_range_list_elm *)
	    nkn_calloc_type(1, sizeof(struct tag_ip_addr_range_list_elm),
			    mod_cr_ds);
	    if (!elm) {
		rv = -ENOMEM;
		DBG_LOG(ERROR, MOD_CRST, "Adding new domain %s OOM "
			"error [err=%d]", rev_str, rv);
		goto error;
	    }
	    new_static_route = 1;
	} else { 
	    /* right now we assume only one entry here, we should use
	     * route identifier UID based search to determine,
	     * which static route to use */
	    elm = TAILQ_FIRST(&sr->addr_list);
	}

	char *p = strdup(static_ip_end), *p1 = NULL;
	struct in_addr start_addr;
	uint32_t prefix, num_hosts, end_addr, mask, shift, tmp;

	if (p[0] == '/') {	/* netmask */
	    mask = atoi(&p[1]);
	    mask = atoi(p1+1);
	    shift = 32 - mask;
	    prefix = (tmp) & (((1<<mask) - 1)<<shift);
	    num_hosts = pow(2, 32-mask);
	    end_addr =  prefix + num_hosts;
	} else {
	    inet_pton(AF_INET, p, &start_addr);
	    tmp = ntohl(start_addr.s_addr);
	    elm->addr_range.end_ip = end_addr;
	}
       	if (new_static_route)
	    TAILQ_INSERT_HEAD(&sr->addr_list, elm, entries);

	free(p);
    }	
    
    if (geo_weight) {
	if (domain->rp.type != ROUTING_TYPE_GEOLOAD) {
	    rv = -E_DOMAIN_INVALID_ROUTING_POLICY;
	    DBG_LOG(ERROR, MOD_CRST, "Domain %s Error incompatible "
		    "routing type %d for which geo weight "
		    "is being set", domain->name, 
		    domain->rp.type);
	    goto error;
	}
	domain->rp.p.glr.geo_weighting = geo_weight;
    }

    if (load_weight) {
	if (domain->rp.type != ROUTING_TYPE_GEOLOAD) {
	    rv = -E_DOMAIN_INVALID_ROUTING_POLICY;
	    DBG_LOG(ERROR, MOD_CRST, "Domain %s Error incompatible "
		    "routing type %d for which load weight "
		    "is being set", domain->name, 
		    domain->rp.type);
	    goto error;
	}
	domain->rp.p.glr.load_weighting = load_weight;
    }

    if (ttl) {
	domain->ttl = ttl;
    }

    if (add_flag) {
	/*Add CE to its hash table*/
	rv = ds_trie_mgmt_add(ref, domain->name, ds_dn);
	if(rv!=0) {
	    rv = -E_DOMAIN_DUP;
	    DBG_LOG(ERROR, MOD_CRST, "Error adding domain, duplicate "
		    "entries present [err=%d]", rv);
	    goto error;
	}
	AO_fetch_and_add1(&glob_crst_num_domains_active);
	ref->hold_ref_cont(ref);
    }

    free(rev_str);
    ds_cont_write_unlock(ds_dn);
    return rv;
 error:
    free(rev_str);
    ds_cont_write_unlock(ds_dn);
    return rv;
}

static int32_t static_route_list_cleanup(
			 struct ip_addr_range_list_t *head) {
    
    if (head) {
	struct tag_ip_addr_range_list_elm *elm = NULL;
	if (TAILQ_EMPTY(head)) {
	    return 0;
	}
	while ( (elm = TAILQ_FIRST(head)) ) {
	    TAILQ_REMOVE(head, elm, entries);
	    if (elm) {
		free(elm);
	    }
	}
    }
    
    return 0;
}

static int32_t create_new_domain(char *domain_name, 
				 domain_t **out) {
    domain_t *domain = NULL;
    int32_t rv = 0;
    
    DBG_LOG(MSG, MOD_CRST, "Adding new Domain %s", domain_name);

    domain = (domain_t*) 
	nkn_calloc_type(1,sizeof(domain_t), mod_cr_ds);
    if (!domain) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM creating a new domain %s",
		domain_name);
	goto error;
    }
    domain->name = (char *)
	nkn_calloc_type(1,sizeof(char)* (strlen(domain_name)+1), 
			mod_cr_ds);
    if (!domain->name) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM creating a new domain %s",
		domain_name);
	goto error;
    }
    pthread_mutex_init(&domain->lock,NULL);
    domain->name_len = strlen(domain_name);
    strncpy(domain->name, domain_name, domain->name_len);
    if (domain->name[domain->name_len - 1] == '*') {
	domain->wildcard = 1;
	domain->name[domain->name_len - 1] = '\0';
    }
    domain->attributes = (hash_tbl_props_t*)
	nkn_calloc_type(1, sizeof(hash_tbl_props_t),  mod_cr_ds);
    if (!domain->attributes) {
	rv = -ENOMEM;
	DBG_LOG(ERROR, MOD_CRST, "OOM creating a new domain %s",
		domain_name);
	goto error;
    }
    domain->rp.type = CRST_STORE_TYPE_GEO;
    domain->de_cb = &gl_obj_de_geo_lf;
    domain->ttl = 15*60;//15 mins
    rv = gl_obj_de_geo_lf.init(&domain->de_state);
    if (rv) {
	DBG_LOG(ERROR, MOD_CRST, "Error creating "
		"DE instance for domain name %s" 
		"[err=%d]", domain->name, rv);
	goto error;
    }

    ds_hash_table_mgmt_init(domain->attributes);
    *out = domain;
    return rv;

 error:
    if (domain) {
	//cleanup_domain();
	;
    }
    *out = NULL;
    return rv;
    
}

static void free_ref_domain(void *arg) {
    
    domain_t *domain = NULL;

    domain = (domain_t*)arg;
    cleanup_domain(domain);
}
    
static void cleanup_domain(domain_t *domain) {
    
    if (domain) {
	domain->de_cb->cleanup(domain->de_state);
	if(domain->attributes) 
	    ds_hash_table_mgmt_deinit(domain->attributes);
	free(domain);
	AO_fetch_and_sub1(&glob_crst_num_domains_active);
    }

    return;
}
/*Assumes that both the domain and CG are already created
@ret: 0 : if all OK
    : -E_CG_ABSENT : if CG not created
    :-E_DOMAIN_ABSENT : if domain not created
*/
int32_t add_cg_to_domain(char* domain_name, char* cg_name){

    cache_group_t* cg=NULL;
    int32_t rv;
    domain_t * domain;
    char* rev_str;
    ref_count_mem_t *ref_domain = NULL;

    if (!domain_name || !cg_name) {
	DBG_LOG(MSG, MOD_CRST, "Domain name or CG name is NULL, "
		"noting to do");
	return 0;
    }

    rev_str = strrev(domain_name);

    hash_tbl_props_t* ds_dn = ds_glob_ctx->domainname_domainptr;
    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    ds_cont_write_lock(ds_dn);
    ds_cont_write_lock(ds_cg);
    /*Get  CG from Hash table*/
    cg = get_cg_from_name(cg_name);
    if(cg==NULL){
        printf("CG does not EXIST\n \
		Create the CG befoore adding it to the domain\n");
	ds_cont_write_unlock(ds_cg);
	ds_cont_write_unlock(ds_dn);
        return -E_CG_ABSENT;
    }

    /*Get domain from hash table*/
    lookup_trie(ds_glob_ctx->domainname_domainptr, 
		rev_str, 0 ,(void**)&ref_domain);
    if(ref_domain==NULL){
        printf("Domain does not EXIST");
	ds_cont_write_unlock(ds_cg);
	ds_cont_write_unlock(ds_dn);
        return -E_DOMAIN_ABSENT;
    }
    domain = ref_domain->mem;
    domain->cg_ptr_list = g_list_append(domain->cg_ptr_list, cg);
    cg->domain_list = g_list_append(cg->domain_list, domain);

    free(rev_str);
    ds_cont_write_unlock(ds_cg);
    ds_cont_write_unlock(ds_dn);
    return 0;
}

int32_t del_cg_from_domain(char*domain_name, char* cg_name){

    cache_group_t* cg;
    int32_t rv;
    domain_t * domain;
    char* rev_str;
    ref_count_mem_t *ref_domain = NULL;
    
    if (!domain_name || !cg_name) {
	DBG_LOG(MSG, MOD_CRST, "Domain name or CG name is NULL "
		"nothing to do");
	return 0;
    }

    rev_str = strrev(domain_name);

    hash_tbl_props_t* ds_dn = ds_glob_ctx->domainname_domainptr;
    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    ds_cont_write_lock(ds_dn);
    ds_cont_write_lock(ds_cg);
    /*Get  CG from Hash table*/
    cg = get_cg_from_name(cg_name);
    if(cg==NULL){
        printf("CG does not EXIST\n Create the CG before adding it to the domain\n");
	ds_cont_write_unlock(ds_cg);
	ds_cont_write_unlock(ds_dn);
        return -E_CG_ABSENT;
    }

    /*Get domain from hash table*/
    lookup_trie(ds_dn, 	rev_str, 
		strlen(rev_str), (void**)&ref_domain);
    if(ref_domain==NULL){
        printf("domain does not EXIST");
	ds_cont_write_unlock(ds_cg);
	ds_cont_write_unlock(ds_dn);
        return -E_DOMAIN_ABSENT;
    }
    domain = ref_domain->mem;

    /*Delete CG from domain list*/
    domain->cg_ptr_list= g_list_remove(domain->cg_ptr_list, cg);
    cg->domain_list = g_list_remove(cg->domain_list, domain);
    free(rev_str);
    ds_cont_write_unlock(ds_cg);
    ds_cont_write_unlock(ds_dn);
    return 0;
}

int32_t set_attr_domain(char* domain_name, char* attr, void*val){
    domain_t* domain;
    char* rev_str;
    rev_str = strrev(domain_name);

    hash_tbl_props_t* ds_dn = ds_glob_ctx->domainname_domainptr;
    ds_cont_write_lock(ds_dn);
    /*Get POP from hash table*/
    lookup_hash(ds_dn, rev_str, 0 ,(void**) &domain);
    if(domain==NULL){
        printf("Domain does not EXIST. Call to set_attr_domain failed\n");
	ds_cont_write_unlock(ds_dn);
        return -E_DOMAIN_ABSENT;
    }
    ds_hash_table_mgmt_alter(val, attr, domain->attributes);
    free(rev_str);
    ds_cont_write_unlock(ds_dn);
    return 0;
}


int32_t get_attr_domain(char* domain_name, char* attr, void*val){


    return 0;
}



int32_t get_attr_names_domain(char* domain_name){


    return 0;
}


int32_t del_domain(char *domain_name){
    
    char* rev_str;
    ref_count_mem_t *ref = NULL;

    rev_str = strrev(domain_name);

    hash_tbl_props_t* ds_dn = ds_glob_ctx->domainname_domainptr;
    ds_cont_write_lock(ds_dn);
    /*Get domain from hash table*/
    lookup_trie(ds_dn, rev_str, 0 , (void**)&ref);
    if(ref == NULL){
	DBG_LOG(ERROR, MOD_CRST, "Error deleting domain %s, domain "
		"does not exist [err=%d]", 
		domain_name, -E_DOMAIN_ABSENT);
	ds_cont_write_unlock(ds_dn);
        return -E_DOMAIN_ABSENT;
    }
    domain_t* dn = ref->mem;
    if (g_list_first(dn->cg_ptr_list) != NULL) {

	DBG_LOG(ERROR, MOD_CRST, "Error deleting domain %s, domain "
		"CG List not empty", domain_name);
	ds_cont_write_unlock(ds_dn);
	return -E_DOMAIN_HAS_CG;
    }

    ds_trie_mgmt_remove(rev_str, ds_dn);
    ref->release_ref_cont(ref);
    ds_cont_write_unlock(ds_dn);

    return 0;
}

/* Context initialization for session*/


int32_t init_dstore(void){
    ds_glob_ctx =(ds_glob_ctx_t*) nkn_calloc_type(1, sizeof(ds_glob_ctx_t), mod_cr_ds);
    // ds_glob_ctx->dns_CG = (hash_tbl_props_t*)nkn_calloc_type(1, sizeof(hash_tbl_props_t), mod_cr_ds);
    ds_glob_ctx->domainname_domainptr = (hash_tbl_props_t*)nkn_calloc_type(1, sizeof(hash_tbl_props_t), mod_cr_ds);
    ds_glob_ctx->CGname_CGptr = (hash_tbl_props_t*)nkn_calloc_type(1, sizeof(hash_tbl_props_t), mod_cr_ds);
    ds_glob_ctx->CEname_CEptr = (hash_tbl_props_t*)nkn_calloc_type(1, sizeof(hash_tbl_props_t), mod_cr_ds);
    ds_glob_ctx->POPname_POPptr = (hash_tbl_props_t*)nkn_calloc_type(1, sizeof(hash_tbl_props_t), mod_cr_ds);
    // ds_hash_table_mgmt_init( ds_glob_ctx->dns_CG);
    ds_hash_table_mgmt_init( ds_glob_ctx->domainname_domainptr);
    ds_hash_table_mgmt_init( ds_glob_ctx->CGname_CGptr);
    ds_hash_table_mgmt_init( ds_glob_ctx->CEname_CEptr);
    ds_hash_table_mgmt_init( ds_glob_ctx->POPname_POPptr);
    ds_trie_mgmt_init( ds_glob_ctx->domainname_domainptr);
    return  0;
}


int32_t get_domain_lookup_resp(char const* dname, char const* qtype_str, 
	char const* src_ip, char** response, uint32_t* response_len) {

    ref_count_mem_t* ref = NULL;
    int32_t rv = 0;
    cache_entity_addr_type_t qtype = atoi(qtype_str);
    hash_tbl_props_t* ds_dn = ds_glob_ctx->domainname_domainptr;
    hash_tbl_props_t* ds_cg = ds_glob_ctx->CGname_CGptr;
    hash_tbl_props_t* ds_ce = ds_glob_ctx->CEname_CEptr;
    hash_tbl_props_t* ds_pp = ds_glob_ctx->POPname_POPptr;
    ds_cont_read_lock(ds_dn);
    ds_cont_read_lock(ds_cg);
    ds_cont_read_lock(ds_ce);
    ds_cont_read_lock(ds_pp);
    lookup_dns(dname, src_ip, (void**)&ref);
    if (ref == NULL) {
	rv = 2;
	goto clean_return;
    }
    domain_t* domain_ctx = ref->mem;
    if (searchCR_Store(dname, src_ip, qtype, 
		       domain_ctx,
		       response, response_len) < 0) {
	rv = 2;
	goto clean_return;
    }

clean_return:
    ds_cont_read_unlock(ds_pp);
    ds_cont_read_unlock(ds_ce);
    ds_cont_read_unlock(ds_cg);
    ds_cont_read_unlock(ds_dn);
    return rv;
}


static int32_t searchCR_Store(char const* dname, char const* src_ip,
	      cache_entity_addr_type_t qtype, domain_t* domain_ctx, 
	      char** result, uint32_t *result_size) {

    int32_t rc = 0;
    if (g_list_first(domain_ctx->cg_ptr_list) == NULL) {
	glob_crst_lookup_err++;
	rc = -E_CG_DOMAIN_BINDING_ABSENT;
	DBG_LOG(ERROR, MOD_CRST, "Error no CG bound to domain %s "
		"[err=%d]", domain_ctx->name, rc);
	return -1;
    }

    crst_search_out_t* out = NULL;
    cache_group_t* cg_ctx = (cache_group_t*)domain_ctx->cg_ptr_list->data;
    uint32_t i = 0;
    de_input_t di;
    int32_t de_res = 0;
    rrecord_msg_fmt_builder_t *rrb = NULL;

    GList *ce_list = cg_ctx->ce_ptr_list, *first = NULL;
    if (g_list_first(ce_list) == NULL) {
	rc = -E_CE_CG_BINDING_ABSENT;
	DBG_LOG(ERROR, MOD_CRST, "Error no CE bound to CG %s "
		"[err=%d]", cg_ctx->name, rc);
	goto clear_return;
    }

    /* fill 'out's' global fields */
    out = nkn_malloc_type(1 * sizeof(crst_search_out_t), mod_cr_ds);
    out->ce_count = g_list_length(ce_list);
    out->ce_attr = nkn_malloc_type(out->ce_count * sizeof(crst_ce_attr_t),
				   mod_cr_ds);

    out->ttl = domain_ctx->ttl;
    out->in_addr_type = qtype; /* def */
    if (src_ip) {
	snprintf(out->resolv_addr, 32, "%s", src_ip);
    } else {
	out->resolv_addr[0] = '\0';
    }
    for (; i < out->ce_count; i++)
	out->ce_attr[i].num_addr = 0;

    i = 0;

    do {

	cache_entity_t* ce = (cache_entity_t*)ce_list->data;
	if (!ce->pop) {
	    rc = -E_CE_POP_BINDING_ABSENT;
	    DBG_LOG(ERROR, MOD_CRST, "Error CE %s to POP binding "
		    "not available [err=%d]", ce->name, rc);
	    goto clear_return;
	}
	memcpy(&out->ce_attr[i].stats, &ce->fb, sizeof(lf_stats_t));
	memcpy(&out->ce_attr[i].loc_attr, &ce->pop->location,
		sizeof(conjugate_graticule_t));
	out->ce_attr[i].load_watermark = ce->load_watermark;
	uint32_t j = 0;

	if (ce->addr[ce_addr_type_ipv4][0] != '\0') {
	    out->ce_attr[i].addr[out->ce_attr[i].num_addr] =
		&ce->addr[ce_addr_type_ipv4][0];
	    out->ce_attr[i].addr_type[out->ce_attr[i].num_addr] =
		ce_addr_type_ipv4;
	    out->ce_attr[i].addr_len[out->ce_attr[i].num_addr] =
		&ce->addr_len[ce_addr_type_ipv4];
	    out->ce_attr[i].num_addr++;
	}
	if (ce->addr[ce_addr_type_ipv6][0] != '\0') {
	    out->ce_attr[i].addr[out->ce_attr[i].num_addr] =
		&ce->addr[ce_addr_type_ipv6][0];
	    out->ce_attr[i].addr_type[out->ce_attr[i].num_addr] =
		ce_addr_type_ipv6;
	    out->ce_attr[i].addr_len[out->ce_attr[i].num_addr] =
		&ce->addr_len[ce_addr_type_ipv6];
	    out->ce_attr[i].num_addr++;
	}
	if (ce->addr[ce_addr_type_cname][0] != '\0') {
	    out->ce_attr[i].addr[out->ce_attr[i].num_addr] =
		&ce->addr[ce_addr_type_cname][0];
	    out->ce_attr[i].addr_type[out->ce_attr[i].num_addr] =
		ce_addr_type_cname;
	    out->ce_attr[i].addr_len[out->ce_attr[i].num_addr] =
		&ce->addr_len[ce_addr_type_cname];
	    out->ce_attr[i].num_addr++;
	}
	i++;
    } while ((ce_list = g_list_next(ce_list)) != NULL);
    de_res = 1;
    switch(domain_ctx->rp.type) {
	case ROUTING_TYPE_STATIC:
	    //de_static(di, &result, &num_results);
	    break;
	case ROUTING_TYPE_RR:
	    assert(domain_ctx->de_state);
	    de_res = gl_obj_de_rr.decide(domain_ctx->de_state,
					 out, 
					 (uint8_t**) result, 
					 result_size);
	    break;
	case ROUTING_TYPE_GEOLOAD:
            de_res = gl_obj_de_geo_lf.decide(domain_ctx->de_state,
					     out,
					     (uint8_t**) result,
					     result_size);

	    break;
	default:
	    assert(0);
	    break;
    }

    /* DE returned an error for whatever reason, need to construct
     * a CNAME of last resort message
     */
    if (de_res) { 
	glob_crst_de_fail++;
	uint32_t rlen, ttl = 0;

	if (domain_ctx->cname_last_resort[0] == '\0') {
	    result = NULL;
	    result_size = 0;
	    rc = -1;
	    DBG_LOG(ERROR, MOD_CRST, "DE [type=%d] failed, "
		    "and CNAME of last resort is absent", 
		    domain_ctx->rp.type);
	    goto clear_return;
	}
	
	rlen = rrecord_msg_fmt_builder_compute_record_size(
			   domain_ctx->cname_last_resort_len);

	rc = rrecord_msg_fmt_builder_create(rlen, &rrb);
	if (rc) {
	    goto clear_return;
	}

	rc = rrb->add_hdr(rrb, 1, ttl);
	if (rc) {
	    goto clear_return;
	}

	rc = rrb->add_record(rrb, ce_addr_type_ns,
	      (uint8_t *)domain_ctx->cname_last_resort,
	      domain_ctx->cname_last_resort_len);
	if (rc) {
	    goto clear_return;
	}
	
	rc = rrb->get_buf(rrb, (uint8_t **) result, 
			   result_size);
	if (rc) {
	    goto clear_return;
	}

    } else {
	domain_ctx->stats.num_hits++;
    }

    free(out->ce_attr);
    free(out);
     
clear_return:
    
    if (rrb) rrb->delete(rrb);
    if (rc) {
	glob_crst_lookup_err++;
	domain_ctx->stats.num_err++;
    }
    return rc;
}

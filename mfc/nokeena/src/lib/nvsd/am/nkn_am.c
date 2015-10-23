/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Analytics Manager
 *
 * Author: Jeya ganesh babu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include "nkn_assert.h"
#include "nkn_am_api.h"
#include "nkn_am.h"
#include "nkn_mediamgr_api.h"
#include "nkn_pseudo_con.h"
#include "rtsp_om_defs.h"
#include <sys/prctl.h>
#include "nkn_lockstat.h"
#include "cache_mgr.h"
#include "nkn_namespace.h"
#include "nkn_am.h"
#include "nkn_mm_mem_pool.h"
#include "nkn_cod.h"

AM_provider_data_t nkn_am_provider_data[NKN_MM_MAX_CACHE_PROVIDERS];

/* API xfer data structures/Queue */
nkn_mm_malloc_t *nkn_am_xfer_data_pool[NKN_SCHED_MAX_THREADS+1];
nkn_mm_malloc_t *nkn_am_xfer_ext_data_pool[NKN_SCHED_MAX_THREADS+1];

TAILQ_HEAD(nkn_am_xfer_list_t, am_xfer_data_t)
	    nkn_am_xfer_list[NKN_SCHED_MAX_THREADS+1];
nkn_mutex_t nkn_am_xfer_lock[NKN_SCHED_MAX_THREADS+1];

/* Small object min hit count */
int glob_am_small_obj_hit;

/* Client driven Push Ingest */
int glob_am_push_ingest_enabled = 1;

/* Counters */
uint64_t glob_am_cod_get_cn_failed;
uint64_t glob_am_avoided_large_name_entry_cnt;
uint64_t glob_am_avoided_large_attr_size;
uint64_t glob_am_ingest_worthy_check_fail_cnt;
uint64_t glob_am_ingest_hotness_check_fail_cnt;
uint64_t glob_am_ingest_obj_high_watermark_cnt;
uint64_t glob_am_ingest_attr_size_not_ingested_cnt;
uint64_t glob_am_ingest_unknown_provider_cnt;
uint64_t glob_am_ingest_provider_stat_err;
uint64_t glob_am_process_thread_called_cnt;
uint64_t glob_am_ingest_videos_cnt;
uint64_t glob_am_push_ingest_videos_cnt;
uint64_t glob_am_push_ingest_cont_req;
uint64_t glob_am_push_ingest_comp_req;
uint64_t glob_am_push_ingest_cache_worthy_failure;
AO_t glob_bm_to_am_hotness_update;
AO_t glob_bm_to_am_hotness_update_queue;
AO_t glob_am_push_ingest_data_buffer_hold;
AO_t glob_am_push_ingest_attr_buffer_hold;
AO_t nkn_am_xfer_data_in_use[NKN_SCHED_MAX_THREADS+1];
AO_t nkn_am_ext_data_in_use[NKN_SCHED_MAX_THREADS+1];

/* Debug counters */
uint64_t glob_am_update_called;

uint64_t glob_am_promote_sas_thread_called_cnt;
uint64_t glob_am_promote_ssd_thread_called_cnt;
uint64_t glob_am_promote_sata_thread_called_cnt;

uint64_t glob_am_promote_to_ssd_called_cnt;
uint64_t glob_am_promote_to_sas_called_cnt;
uint64_t glob_am_promote_to_sata_called_cnt;

uint64_t glob_am_promote_to_ssd_failed_cnt;
uint64_t glob_am_promote_to_sas_failed_cnt;
uint64_t glob_am_promote_to_sata_failed_cnt;

uint64_t glob_am_promote_ssd_found_ing_obj_cnt;
uint64_t glob_am_promote_sata_found_ing_obj_cnt;
uint64_t glob_am_promote_sas_found_ing_obj_cnt;

uint64_t glob_am_promote_skip_cnt;
uint64_t glob_am_promote_hotness_chk_failed_cnt;
uint64_t glob_am_ingest_check_directory_depth_cnt;
uint64_t glob_am_ingest_check_object_size_hit_cnt;
uint64_t glob_am_ingest_check_object_expiry_cnt;
uint64_t glob_am_promote_threshold_skip_cnt;
uint64_t glob_am_promote_high_tier_size_threshold_cnt;
uint64_t glob_am_inline_comp_hot_threshold_ingest_failure_cnt;


GThread *am_ingest_thread;
pthread_cond_t am_ingest_thr_cond;
static pthread_mutex_t am_ingest_mutex;
int am_ingest_interval = 1;
int g_am_client_driven_hotness_threshold = 9;
int glob_am_ingest_ignore_hotness_check = 0;
uint64_t g_am_promote_size_threshold = 0;
unsigned long glob_am_bytes_based_hotness_threshold = 2097152;
int glob_am_bytes_based_hotness = 0;

int hotness_scale[NKN_MM_MAX_CACHE_PROVIDERS] = {
	0, /* Unknown_provider */
	6, /* SolidStateMgr_provider */
	0, /* FlashMgr_provider */
	0, /* BufferCache_provider */
	2, /* SASDiskMgr_provider */
	2, /* SAS2DiskMgr_provider */
	1, /* SATADiskMgr_provider */
	0, /* NKN_MM_max_pci_providers */
};

/*
 * Regular pull model ingest. Objects which are failed to get
 * ingested in the push model, will be tried through the pull
 * model
 */
static int
s_am_regular_ingest(AM_obj_t *pobj, nkn_provider_type_t srcp,
                   nkn_provider_type_t dstp)
{
    int ret = -1;
    int hotness;
    int client_driven_hotness_threshold;

    if(pobj->in_object_data) {

	hotness = am_decode_hotness(&pobj->hotness);

	if(!VALID_NS_TOKEN(pobj->ns_token))
	    pobj->ns_token = cachekey_to_nstoken(pobj->pk.name);

	client_driven_hotness_threshold =
	    nkn_get_namespace_client_driven_aggr_threshold(
				    pobj->ns_token);

	if(client_driven_hotness_threshold &&
	    (hotness >= client_driven_hotness_threshold))
	    pobj->in_object_data->flags |= AM_INGEST_AGGRESSIVE;
	else
	    pobj->in_object_data->flags &= ~AM_INGEST_AGGRESSIVE;

	if(pobj->in_object_data->flags & AM_CIM_INGEST)
	    pobj->in_object_data->flags |= AM_INGEST_AGGRESSIVE;

	if(pobj->policy & AM_POLICY_HP_QUEUE)
	    pobj->in_object_data->flags |= AM_INGEST_HP_QUEUE;


    }

    DBG_LOG(MSG3, MOD_AM,"\nMM promote called for: pobj->pk.name = %s dst = %d",
                pobj->pk.name, dstp);
    ret = MM_promote_uri(pobj->pk.name, srcp, dstp, 0, pobj->in_object_data,
			 am_decode_update_time(&pobj->hotness));
    if(ret < 0) {
        DBG_LOG(MSG, MOD_AM, "\nCannot ingest: pobj->pk.name = %s",
                pobj->pk.name);
    }
    return ret;
}

static int
s_am_is_ingest_worthy(AM_obj_t *objp, nkn_provider_type_t dst_ptype)
{
    int                 h1, h2, ret;
    MM_provider_stat_t  pstat;
    nkn_provider_type_t out_ptype = Unknown_provider;

    pstat.ptype = dst_ptype;
    pstat.sub_ptype = 0;
    /* Find out the cache threshold for this level*/
    ret = MM_provider_stat(&pstat);
    if(ret < 0) {
	glob_am_ingest_provider_stat_err++;
	goto failed;
    }

    if(pstat.caches_enabled != 0) {
	out_ptype = dst_ptype;
    }

    if(out_ptype == Unknown_provider) {
	glob_am_ingest_unknown_provider_cnt++;
	ret = NKN_MM_CIM_NO_DISK_AVAIL;
	goto failed;
    }

    if(objp->in_object_data &&
	    (objp->in_object_data->flags & AM_CIM_INGEST)) {
	return 0;
    }

    /* Bug 3177: Check for the attr size with the max attr size
     * supported by the provider. Incase of error return failure
     */
    if(objp->in_object_data &&
	    objp->in_object_data->attr_size > pstat.max_attr_size) {
	glob_am_ingest_attr_size_not_ingested_cnt++;
	goto failed;
    }

    /* Ignore hotness check 
     * Used by Prefetch
     */
    if(objp->in_object_data &&
	    (objp->in_object_data->flags & AM_IGNORE_HOTNESS)) {
	return 0;
    }

    /* If the disk is not filled to the low threshold, ingest */
    /* The low water mark needs to be check for less than and equal
     * because in DM2, the check is done for greater than.
     */
    if(pstat.percent_full <= pstat.low_water_mark) {
	return 0;
    } else if(pstat.percent_full >= pstat.high_water_mark) {
	glob_am_ingest_obj_high_watermark_cnt++;
    }

    /* Return true, if configuration is set to ignore the hotness check */
    if(glob_am_ingest_ignore_hotness_check) {
	return 0;
    }

    h1 = am_decode_hotness(&objp->hotness);
    h2 = am_decode_hotness(&pstat.hotness_threshold);
    /* If the disk hotness is 0, just ingest the object if the % filled
     * is between low and high. The TFM objects will not be in AM, because
     * the chunked objects are not cacheable initially.
     * So the hotness of the objects getting ingested from TFM will always
     * be 0.
     */
    if(!h2 || h1 > h2) {
	return 0;
    } else {
	if(objp->in_object_data &&
		(objp->in_object_data->flags & AM_COMP_OBJECT))
	    glob_am_inline_comp_hot_threshold_ingest_failure_cnt++;
	glob_am_ingest_hotness_check_fail_cnt++;
    }

failed:
    glob_am_ingest_worthy_check_fail_cnt++;
    /* Don't ingest */
    return -1;
}

static inline void
s_nkn_am_set_small_obj_hotness_value(AM_obj_t *objp, int hotness_threshold)
{
    int h1;

    if((glob_mm_max_local_providers > 1) && objp) {
	objp->policy |= AM_POLICY_ONLY_HI_TIER;
        h1 = am_decode_hotness(&objp->hotness);
	if(h1 < hotness_threshold * glob_am_small_obj_hit)
	    h1 += hotness_threshold * (glob_am_small_obj_hit - 1);
        am_encode_hotness(&objp->hotness, h1);
    }
}

void
nkn_am_log_error(const char *uriname, int errcode)
{
    switch(errcode) {
	case NKN_MM_CONF_SMALL_OBJECT_SIZE:
	    glob_am_ingest_check_object_size_hit_cnt++;
	    break;
	case NKN_MM_CONF_DIRECTORY_DEPTH:
	    glob_am_ingest_check_directory_depth_cnt++;
	    break;
	case NKN_MM_CONF_OBJECT_EXPIRED:
	    glob_am_ingest_check_object_expiry_cnt++;
	    break;
	case NKN_MM_CONF_BIG_ATTRIBUTE:
	    glob_am_avoided_large_attr_size++;
	    break;
	default:
	    assert(0);
    }
    if (glob_log_cache_ingest_failure) {
	DBG_CACHE(" INGESTFAIL \"%s\" %d - - - - - - noageout",
		    uriname, errcode);
    }
}

static void
s_nkn_am_update_hits(am_xfer_data_t *am_xfer_data)
{
    AM_obj_t	*objp = NULL;
    AM_pk_t	am_pk, *pk = &am_pk;
    nkn_hot_t	out_hotness;
    nkn_hot_t	in_seed = am_xfer_data->obj_hotness; /* in_seed */
    nkn_attr_t	*ap = NULL;
    uint64_t	memory_limit = (nkn_am_memory_limit ?
				nkn_am_memory_limit
				: AM_DEFAULT_MEMORY_LIMIT);
    uint64_t	content_length = 0, cache_ingest_size_threshold = 0;
    uint64_t	bytes_served_from_cache = 0;
    uint64_t	hotness_increment = 0;
    uint32_t	in_hotness, obj_hotness;
    uint32_t	num_hits = am_xfer_data->num_hits;
    time_t	object_ts = nkn_cur_ts;
    off_t	cache_min_size=0;
    int		next_provider_id;
    int		new_entry = 0;
    int		cache_age_threshold=0;
    int		uri_min_depth=10, orig_hit_count=0;
    int		cache_ingest_hotness_threshold = AM_DEF_HOT_THRESHOLD;
    int		cim_ingest = 0, policy = 0, ret, i;
    int         cache_fill_policy = 0;
    int         push_ret = -1;
#if 0
    int         set_pending = 0;
#endif

    glob_am_update_called++;

    pk->provider_id = am_xfer_data->provider_id;
    ret = nkn_cod_get_cn(am_xfer_data->in_cod, &pk->name, (int *)&pk->key_len);
    if(ret) {
	glob_am_cod_get_cn_failed++;
	goto cleanup;
    }

    if(pk->key_len > MAX_URI_SIZE) {
	NKN_ASSERT(0);
	glob_am_avoided_large_name_entry_cnt++;
	DBG_LOG(WARNING, MOD_AM, "Entry request for size greater than "
		"MAX_URI_SIZE: %s", pk->name);
	goto cleanup;
    }

    if((pk->provider_id && ((pk->provider_id < NKN_MM_max_pci_providers &&
	    ((glob_am_tbl_total_cnt - glob_am_origin_entry_total_cnt) >=
	     (nkn_am_max_prov_entries))))) ||
	    ((glob_malloc_mod_am_tbl_malloc_key +
	     glob_malloc_mod_am_object_data +
	     glob_malloc_mod_am_tbl_create_am_obj +
	     glob_malloc_mod_http_heap_ext +
	     glob_malloc_mod_normalize_pcon_pcon_t +
	     glob_malloc_mod_pseudo_con_host_hdr) > memory_limit)) {
	nkn_am_tbl_age_out_entry(pk->provider_id);
    }

    if(am_xfer_data->attr_buf) {
	ap = nkn_buffer_getcontent(am_xfer_data->attr_buf);
    }

    if(ap) {
	content_length = ap->content_length;
	if(glob_am_bytes_based_hotness) {
	    do {
		bytes_served_from_cache = AO_load(&ap->cached_bytes_delivered);
	    } while(!AO_compare_and_swap(&ap->cached_bytes_delivered,
		    bytes_served_from_cache, 0));
	    if(bytes_served_from_cache >
		    glob_am_bytes_based_hotness_threshold) {
		hotness_increment = lround((double)bytes_served_from_cache /
				(double)glob_am_bytes_based_hotness_threshold);
	    }
	}
	if((ap->total_attrsize > glob_mm_largest_provider_attr_size)) {
	    DBG_LOG(WARNING, MOD_AM, "Entry request for attribute size "
		"greater than max. Size: %u ", ap->total_attrsize);
	    nkn_am_log_error(pk->name, NKN_MM_CONF_BIG_ATTRIBUTE);
	    goto cleanup;
	}
	orig_hit_count = ap->hit_count;
    }


    if(!(am_xfer_data->client_flags & (AM_CIM_INGEST | AM_PUSH_CONT_REQ)) &&
	    VALID_NS_TOKEN(am_xfer_data->ns_token)) {
	/* Check for content length and directory depth before adding to
	 * the AM table
	 */
	nkn_get_namespace_cache_limitation(am_xfer_data->ns_token,
					   &cache_min_size, &uri_min_depth,
					   &cache_age_threshold,
					   &cache_ingest_size_threshold,
					   &cache_ingest_hotness_threshold,
					   &cache_fill_policy);

	if(ap && ap->content_length && (ap->content_length <
	         (unsigned)cache_min_size)) {
	    DBG_LOG(MSG3, MOD_AM, "[URI=%s] object size less than min"
			" cacheable %ld", pk->name, ap->content_length);
	    nkn_am_log_error(pk->name, NKN_MM_CONF_SMALL_OBJECT_SIZE);
	    goto cleanup;
	}

	if(nkn_mm_validate_dir_depth(pk->name, uri_min_depth)) {
	    DBG_LOG(MSG3, MOD_AM, "[URI=%s] Too many directory levels",
		pk->name);
	    nkn_am_log_error(pk->name, NKN_MM_CONF_DIRECTORY_DEPTH);
	    goto cleanup;
	}

	if(cache_age_threshold && ap &&
		(ap->cache_expiry <= (nkn_cur_ts + cache_age_threshold ))) {
	    DBG_LOG(MSG3, MOD_AM,
		    "[URI=%s] expiry time less than store cache min age %ld",
		      pk->name, ap->cache_expiry );
	    nkn_am_log_error(pk->name, NKN_MM_CONF_OBJECT_EXPIRED);
	    goto cleanup;
	}

	if(ap && ap->content_length &&
		ap->content_length <= cache_ingest_size_threshold) {
	    policy |= AM_POLICY_ONLY_HI_TIER;
	    glob_am_promote_high_tier_size_threshold_cnt++;
	}

    }


    objp = nkn_am_tbl_get(pk);
    if(!objp) {
	if(am_xfer_data->client_flags & AM_PUSH_CONT_REQ)
	    goto cleanup;

	/* This should not happen */
	if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
	    NKN_ASSERT(0);
	    goto cleanup;
	}

	objp = nkn_am_tbl_create_entry(pk);
	if(!objp)
	    goto cleanup;

	objp->ns_token  = am_xfer_data->ns_token;
	objp->policy   |= policy;
	nkn_am_tbl_add_entry(objp);
	new_entry = 1;
    } else {

	if(!VALID_NS_TOKEN(objp->ns_token))
	    objp->ns_token  = am_xfer_data->ns_token;

	if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
	    if(objp->push_ptr) {
		goto push_ingest;
	    } else {
		/* This should not happen */
		if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
		    NKN_ASSERT(0);
		}
		goto cleanup;
	    }
	}
    }

    if(ap) {
	if(g_am_promote_size_threshold &&
		(ap->content_length > g_am_promote_size_threshold)) {
	    objp->policy |= AM_POLICY_ONLY_LO_TIER;
	    glob_am_promote_threshold_skip_cnt++;
	} else {
	    objp->policy &= ~AM_POLICY_ONLY_LO_TIER;
	}
    }

    /* If the existing entry is a CIM update entry and there is an
     * update from BM for a client request, do not update the provider
     * type or the in_object_data. The CIM requests should be processed
     * even if the object is in Disk for expiry updates.
     */
    if(!new_entry && objp->in_object_data &&
	    (objp->in_object_data->flags & AM_CIM_INGEST) && num_hits) {
	goto skip_provider_update;
    }

    if(objp->in_object_data &&
	    (objp->in_object_data->flags & AM_CIM_INGEST)) {
	objp->policy |= AM_POLICY_HP_QUEUE;
    }

    if(am_xfer_data->flags & AM_FLAG_SET_INGEST_COMPLETE)
	objp->flags &= ~AM_FLAG_INGEST_PENDING;

    if((pk->provider_id != objp->pk.provider_id)) {
	/* Free the object data after the object has been ingested
	 */
	if(objp->pk.provider_id >= NKN_MM_max_pci_providers) {
	    if(objp->in_object_data) {
		AM_cleanup_client_data(objp->pk.provider_id,
					    objp->in_object_data);
		free(objp->in_object_data);
		objp->in_object_data = NULL;
	    }
	}
	/* If there is a change in provider 
	 * delete the entry from the existing list
	 * and add it to the new provider list
	 */
	nkn_am_list_delete(objp);

	if(objp->pk.provider_id > NKN_MM_max_pci_providers)
	    glob_am_origin_entry_total_cnt --;

	objp->pk.provider_id = pk->provider_id;

	if(objp->pk.provider_id > NKN_MM_max_pci_providers)
	    glob_am_origin_entry_total_cnt ++;

	nkn_am_list_add_provider_entry(objp);

	objp->flags &= ~AM_FLAG_INGEST_PENDING;
    } else {
	/* Update the lru list */
	nkn_am_list_update_provider_entry(objp);
    }

    if(am_xfer_data->client_flags & AM_PUSH_CONT_REQ) {
	nkn_am_update_ingest_list(objp);
	goto push_ingest;
    }

    if(pk->provider_id > NKN_MM_max_pci_providers) {
	if(!objp->in_object_data) {
		objp->in_object_data = nkn_calloc_type(1,
						    sizeof(am_object_data_t),
						    mod_am_object_data);
	} else {
		AM_cleanup_client_data(pk->provider_id, objp->in_object_data);
	}
	if(ap) {
	    /* If attr_size is present save it in the AM st */
	    objp->in_object_data->attr_size   = ap->total_attrsize;
	}

	memcpy(&(objp->in_object_data->client_ipv4v6),
		&(am_xfer_data->client_ipv4v6), sizeof(ip_addr_t));
	objp->in_object_data->client_port = am_xfer_data->client_port;
	objp->in_object_data->client_ip_family = am_xfer_data->client_ip_family;
	objp->in_object_data->ns_token    = am_xfer_data->ns_token;
	if(IPv4(am_xfer_data->origin_ipv4v6)!=0 ||
		    IPv6(am_xfer_data->origin_ipv4v6)!=0 ) {
	    memcpy(&(objp->in_object_data->origin_ipv4v6),
		    &(am_xfer_data->origin_ipv4v6), sizeof(ip_addr_t));
	    objp->in_object_data->origin_port = am_xfer_data->origin_port;
	}
	if(am_xfer_data->proto_data || am_xfer_data->host_hdr) {
	    AM_copy_client_data_from_xfer_data(pk->provider_id, am_xfer_data,
					       objp->in_object_data);
	}
	objp->in_object_data->flags  = am_xfer_data->client_flags;
	objp->in_object_data->in_cod = am_xfer_data->in_cod;
    }

skip_provider_update:;

    if(in_seed) {
	in_hotness  = am_decode_hotness(&in_seed);
	obj_hotness = am_decode_hotness(&objp->hotness);
	if(in_hotness > obj_hotness) {
	    objp->hotness = in_seed;
	    objp->interval_check_start = am_decode_update_time(&objp->hotness);
	    objp->num_hits = orig_hit_count;
	}
    }

    if(num_hits) {
	if(am_xfer_data->client_flags & AM_CIM_INGEST)
	    cim_ingest = 1;
	objp->flags &= ~(AM_FLAG_INGEST_ERROR | AM_FLAG_INGEST_SKIP);
	if((cim_ingest && !objp->hotness) || (!cim_ingest)) {
	    objp->num_hits += num_hits;
	    objp->hotness = am_update_hotness(&objp->hotness, num_hits,
						object_ts, hotness_increment,
						objp->pk.name);
	}
	if(objp->policy & AM_POLICY_ONLY_HI_TIER)
	    s_nkn_am_set_small_obj_hotness_value(objp,
			    cache_ingest_hotness_threshold);
    }

    nkn_am_update_ingest_list(objp);

    next_provider_id = objp->next_provider_id;

    /* If its a Crawl ingest and we are not able to find the tier to ingest,
     * return error to complete the crawl.
     */
    if(cim_ingest && !next_provider_id) {
	nkn_mm_cim_ingest_failed(objp->pk.name, NKN_MM_CIM_NO_VALID_TIER);
    }

    out_hotness = objp->hotness;

    if(am_xfer_data->attr_buf && ap) {
	ap->hotval = out_hotness;
	ap->hit_count = objp->num_hits;
	nkn_buffer_setmodified_sync(am_xfer_data->attr_buf);
    }

    if(objp->in_object_data)
	objp->in_object_data->obj_hotness = out_hotness;

    /* Check all conditions for non-Push ingest
     * Conditions - 
     * 1. Push Ingest disabled
     * 2. CIM ingest
     * 3. Streaming/Aggressive flag set in Object
     * 4. Namespace policy is aggressive
     * 5. No next provider
     * 6. Worthy check failed
     * 7. Ingest Trigger failure (Due to parallel limit/Buffer limit)
     */

    if(!glob_am_push_ingest_enabled)
	goto cleanup;

    if(cim_ingest)
	goto cleanup;

    if(objp->in_object_data &&
	    (objp->in_object_data->flags &
		(AM_OBJ_TYPE_STREAMING|AM_INGEST_AGGRESSIVE)))
	goto cleanup;

    if(cache_fill_policy && cache_fill_policy != INGEST_CLIENT_DRIVEN)
	goto cleanup;

    if(!objp->next_provider_id)
	goto cleanup;

    if(!am_xfer_data->ext_info)
	goto cleanup;


    ret = s_am_is_ingest_worthy(objp, objp->next_provider_id);
    if(ret < 0) {
	glob_am_push_ingest_cache_worthy_failure++;
	/* In case of error set the ingest_ingest_error bit
	    * so that we wont try again and again.
	    */
	objp->flags |= AM_FLAG_INGEST_ERROR;
	if(objp->in_object_data) {
	    AM_cleanup_client_data(objp->pk.provider_id, objp->in_object_data);
	    free(objp->in_object_data);
	    objp->in_object_data = NULL;
	}
	/* Go to the next provider and check */
	goto cleanup;
    }

push_ingest:;
    if((objp->flags & AM_FLAG_INGEST_PENDING) && !(objp->push_ptr)) {
	goto cleanup;
    }

#if 0
    if((am_xfer_data->client_flags & AM_PUSH_CONT_REQ) && !(objp->push_ptr))
	set_pending = 1;
#endif

    push_ret = MM_push_ingest(objp, am_xfer_data,
			   objp->pk.provider_id, objp->next_provider_id);

    if(!push_ret) {
	if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
	    glob_am_push_ingest_comp_req++;
	} else if(!(am_xfer_data->client_flags &
		    AM_PUSH_CONT_REQ)) {
	    AM_MM_TIER_COUNTER_INC(glob_am_promote_to, objp->next_provider_id,
				    called_cnt);
	    objp->flags |= AM_FLAG_INGEST_PENDING;
	    glob_am_push_ingest_videos_cnt++;
	} else {
	    objp->flags |= AM_FLAG_INGEST_PENDING;
	    glob_am_push_ingest_cont_req++;
	}
    }

    /* In case of no Push Ingest,
     * release the data buffer,
     * release the attribute buffer,
     * close the COD.
     * Ingest will happen with the normal ingest method of GET->PUT.
     */
cleanup:;

    if(push_ret) {
	if(am_xfer_data->ext_info) {
	    for(i=0;i<am_xfer_data->ext_info->num_bufs;i++)
		nkn_buffer_release(am_xfer_data->ext_info->buffer[i]);
	    AO_fetch_and_add(&glob_am_push_ingest_data_buffer_hold,
			    -am_xfer_data->ext_info->num_bufs);
	}

	if(objp && !objp->push_ptr)
	    nkn_cod_clear_push_in_progress(am_xfer_data->in_cod);
    }

    if(am_xfer_data->proto_data || am_xfer_data->host_hdr)
	AM_cleanup_xfer_client_data(pk->provider_id, am_xfer_data);

    if(am_xfer_data->attr_buf) {
	nkn_buffer_release(am_xfer_data->attr_buf);
	AO_fetch_and_sub1(&glob_am_push_ingest_attr_buffer_hold);
    }

    if(am_xfer_data->in_cod != NKN_COD_NULL) {
	nkn_cod_close(am_xfer_data->in_cod, NKN_COD_AM_MGR);
    }

}

static void
s_nkn_am_delete_obj(nkn_cod_t in_cod)
{
    AM_pk_t  pk;

    pk.name = (char *)nkn_cod_get_cnp(in_cod);
    nkn_cod_close(in_cod, NKN_COD_AM_MGR);
    nkn_am_tbl_delete_entry(&pk);
}

static void
s_nkn_am_set_ingest_error(nkn_cod_t in_cod, int flags)
{
    AM_obj_t *objp;
    AM_pk_t  pk;

    pk.name = (char *)nkn_cod_get_cnp(in_cod);
    nkn_cod_close(in_cod, NKN_COD_AM_MGR);
    objp = nkn_am_tbl_get(&pk);
    if(objp) {
	objp->flags &= ~AM_FLAG_INGEST_PENDING;
	if(!(flags & AM_FLAG_SET_INGEST_RETRY)) {
	    objp->flags |= AM_FLAG_INGEST_ERROR;
	    if(objp->in_object_data) {
		AM_cleanup_client_data(objp->pk.provider_id,
					    objp->in_object_data);
		free(objp->in_object_data);
		objp->in_object_data = NULL;
	    }
	}
    }

}

static void
s_nkn_am_set_small_obj_hotness(nkn_cod_t in_cod,
			       int cache_promotion_hotness_threshold)
{
    AM_obj_t *objp;
    AM_pk_t  pk;

    pk.name = (char *)nkn_cod_get_cnp(in_cod);
    nkn_cod_close(in_cod, NKN_COD_AM_MGR);
    objp = nkn_am_tbl_get(&pk);
    s_nkn_am_set_small_obj_hotness_value(objp,
					 cache_promotion_hotness_threshold);
    return;
}

/*
 * AM process thread - 
 *	Processes object entries for hit update/delete/error update
 *	Process entries for ingestion/promotion
 */
static void
s_am_process_thread(void)
{
    AM_pk_t             uri_pk;
    AM_obj_t            *hot_uri_obj = NULL;
    am_xfer_data_t      *am_xfer_data = NULL;
    uint32_t            h1;
    int                 i, ret=-1;
    struct timespec     abstime;
    uint32_t            cache_ingest_hotness_threshold;
    int                 am_xfer_data_in_queue, am_xfer_queue_retry_count;

    prctl(PR_SET_NAME, "nvsd-am-process", 0, 0, 0);

    while(1) {
loop_again:;
	glob_am_process_thread_called_cnt++;

	/* Object hits/delete/update will be processed here before 
	 * checking for promotion/ingestion
	 */
	am_xfer_queue_retry_count = 0;

retry_again:;

	am_xfer_data_in_queue = 0;

	for(i=0;i<=glob_sched_num_core_threads;i++) {
	    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[i]);
	    am_xfer_data = TAILQ_FIRST(&nkn_am_xfer_list[i]);
	    if(am_xfer_data)
		TAILQ_REMOVE(&nkn_am_xfer_list[i], am_xfer_data, xfer_entries);
	    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[i]);

	    if(am_xfer_data) {

		am_xfer_data_in_queue = 1;

		if(AO_load(&glob_bm_to_am_hotness_update_queue) > 1)
		    AO_fetch_and_sub1(&glob_bm_to_am_hotness_update_queue);

		if(am_xfer_data->flags & AM_FLAG_ADD_ENTRY) {
		    am_xfer_data->flags &= ~AM_FLAG_ADD_ENTRY;
		    s_nkn_am_update_hits(am_xfer_data);
		} else if(am_xfer_data->flags & AM_FLAG_DEL_ENTRY) {
		    am_xfer_data->flags &= ~AM_FLAG_DEL_ENTRY;
		    s_nkn_am_delete_obj(am_xfer_data->in_cod);
		} else if(am_xfer_data->flags & AM_FLAG_SET_INGEST_ERROR) {
		    am_xfer_data->flags &= ~AM_FLAG_SET_INGEST_ERROR;
		    s_nkn_am_set_ingest_error(am_xfer_data->in_cod,
						am_xfer_data->flags);
		} else if(am_xfer_data->flags & AM_FLAG_SET_SMALL_OBJ_HOT) {
		    am_xfer_data->flags &= ~AM_FLAG_SET_SMALL_OBJ_HOT;
		    s_nkn_am_set_small_obj_hotness(am_xfer_data->in_cod,
						   am_xfer_data->obj_hotness);
		}

		if(am_xfer_data->ext_info) {
		    nkn_mm_free_memory(nkn_am_xfer_ext_data_pool[i],
				       am_xfer_data->ext_info);
		    AO_fetch_and_sub1(&nkn_am_ext_data_in_use[i]);
		}
		if(am_xfer_data) {
		    nkn_mm_free_memory(nkn_am_xfer_data_pool[i], am_xfer_data);
		    AO_fetch_and_sub1(&nkn_am_xfer_data_in_use[i]);
		}

		continue;
	    }
	}

	if(am_xfer_data_in_queue && (am_xfer_queue_retry_count < 500)) {
	    am_xfer_queue_retry_count++;
	    goto retry_again;
	}

	for(i=Unknown_provider; i<NKN_MM_max_pci_providers; i++) {
	    if(nkn_am_provider_data[i].state == AM_PROVIDER_DISABLED)
		continue;

try_again:
	    AM_MM_TIER_COUNTER_INC(glob_am_promote, i, thread_called_cnt);
	    uri_pk.provider_id = i;

	    hot_uri_obj = nkn_am_list_get_hottest(&uri_pk);
	    if(!hot_uri_obj) {
		continue;
	    }

	    AM_MM_TIER_COUNTER_INC(glob_am_promote, i, found_ing_obj_cnt);

	    /* Do the check only for Promotes */
	    if(hot_uri_obj->pk.provider_id < NKN_MM_max_pci_providers) {
		/* Skip promotion check if the object is queued for
		 * ingestion and the policy is set to ingest to highest
		 * tier
		 */
		if(hot_uri_obj->policy & AM_POLICY_ONLY_HI_TIER)
		    goto skip_promote_check;
		/* Promote Only if promotion is enabled */
		if(am_cache_promotion_enabled != 1) {
		    hot_uri_obj->flags |= AM_FLAG_INGEST_SKIP;
		    glob_am_promote_skip_cnt++;
		    continue;
		}
		/* Promote only if the hotness is greater than
		    the hotness threshold of the dest cache */
		h1 = am_decode_hotness(&hot_uri_obj->hotness);
		cache_ingest_hotness_threshold =
		    nkn_get_namespace_cache_ingest_hotness_threshold(
						hot_uri_obj->ns_token);
		if(h1 < (cache_ingest_hotness_threshold * hotness_scale[i])) {
		    glob_am_promote_hotness_chk_failed_cnt++;
		    continue;
		}
	    }

skip_promote_check:;

	    /* Check if this video belongs in the dest cache*/
	    ret = s_am_is_ingest_worthy(hot_uri_obj, i);
	    if(ret < 0) {
		/* In case of error set the ingest_ingest_error bit
		 * so that we wont try again and again.
		 */
		hot_uri_obj->flags |= AM_FLAG_INGEST_ERROR;
		if(hot_uri_obj->in_object_data) {
		    AM_cleanup_client_data(hot_uri_obj->pk.provider_id,
				    hot_uri_obj->in_object_data);
		    free(hot_uri_obj->in_object_data);
		    hot_uri_obj->in_object_data = NULL;
		}
		/* Go to the next provider and check */
		goto try_again;
	    }

	    ret = s_am_regular_ingest(hot_uri_obj,
                                       hot_uri_obj->pk.provider_id, i);
            if(ret < 0) {
                /* There are too many ingests in the system.
                    Wait for some time and then try to ingest.*/
		AM_MM_TIER_COUNTER_INC(glob_am_promote_to, i, failed_cnt);
                continue;
            }

	    AM_MM_TIER_COUNTER_INC(glob_am_promote_to, i, called_cnt);

	    hot_uri_obj->flags |= AM_FLAG_INGEST_PENDING;

	    glob_am_ingest_videos_cnt++;
	    goto try_again;

	} /* for(i=NKN_MM_max_pci_providers; */

	for(i=0;i<=glob_sched_num_core_threads;i++) {
	    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[i]);
	    am_xfer_data = TAILQ_FIRST(&nkn_am_xfer_list[i]);
	    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[i]);
	    if(am_xfer_data)
		goto loop_again;
	}

        clock_gettime(CLOCK_MONOTONIC, &abstime);
        abstime.tv_sec += am_ingest_interval;
        abstime.tv_nsec = 0;
	pthread_mutex_lock(&am_ingest_mutex);
	pthread_cond_timedwait(&am_ingest_thr_cond, &am_ingest_mutex, &abstime);
	pthread_mutex_unlock(&am_ingest_mutex);
    } /* while(1)*/
}

/* 
 * Hit update routine - 
 *	Creates an object and posts it to the process thread
 */
nkn_hot_t
AM_inc_num_hits(AM_pk_t *pk, uint32_t num_hits,
                nkn_hot_t in_seed,
		void *attr_buf,
		am_object_data_t *in_object_data,
		am_ext_info_t *am_ext_info)
{
    am_xfer_data_t *am_xfer_data = NULL;
    int            tid           = 0, i;
    int            attr_buffer_held = 0, data_buffer_held = 0;

    if(!pk)
	goto error;

    tid = t_scheduler_id;
    if(tid > (glob_sched_num_core_threads + 1)) {
	tid = glob_sched_num_core_threads + 1;
    }


    am_xfer_data = nkn_mm_get_memory(nkn_am_xfer_data_pool[tid]);
    if(!am_xfer_data) {
	goto error;
    }
    AO_fetch_and_add1(&nkn_am_xfer_data_in_use[tid]);

    am_xfer_data->obj_hotness = in_seed;
    am_xfer_data->num_hits    = num_hits;
    am_xfer_data->provider_id = pk->provider_id;
    am_xfer_data->ext_info    = am_ext_info;

    if(attr_buf) {
	nkn_buffer_hold(attr_buf);
	AO_fetch_and_add1(&glob_am_push_ingest_attr_buffer_hold);
	attr_buffer_held = 1;
	am_xfer_data->attr_buf = attr_buf;
    }

    if(glob_am_push_ingest_data_buffer_hold > glob_mm_push_ingest_max_bufs) {
	am_xfer_data->ext_info = NULL;
	if(am_ext_info) {
	    nkn_mm_free_memory(nkn_am_xfer_ext_data_pool[tid],
			    am_ext_info);
	    AO_fetch_and_sub1(&nkn_am_ext_data_in_use[tid]);
	}
	am_ext_info = NULL;
    }


    if(am_xfer_data->ext_info) {
	for(i=0;i<am_ext_info->num_bufs;i++) {
	    nkn_buffer_hold(am_ext_info->buffer[i]);
	    data_buffer_held = 1;
	}
	AO_fetch_and_add(&glob_am_push_ingest_data_buffer_hold,
			 am_ext_info->num_bufs);
    }

    if(in_object_data && in_object_data->flags & AM_PUSH_CONT_REQ) {
	am_xfer_data->client_flags     = in_object_data->flags;
	am_xfer_data->ns_token         = in_object_data->ns_token;
	am_xfer_data->in_cod           = nkn_cod_dup(in_object_data->in_cod,
						     NKN_COD_AM_MGR);
	if(am_xfer_data->in_cod == NKN_COD_NULL)
	    goto error;
    } else {

	if(pk->provider_id >= NKN_MM_max_pci_providers) {

	    if(in_object_data) {
		am_xfer_data->provider_id      = pk->provider_id;
		am_xfer_data->in_cod           = nkn_cod_dup(in_object_data->in_cod,
							    NKN_COD_AM_MGR);
		am_xfer_data->ns_token         = in_object_data->ns_token;
		am_xfer_data->client_port      = in_object_data->client_port;
		am_xfer_data->origin_port      = in_object_data->origin_port;
		am_xfer_data->encoding_type    = in_object_data->encoding_type;
		am_xfer_data->client_ip_family = in_object_data->client_ip_family;
		am_xfer_data->client_flags     = in_object_data->flags;
		am_xfer_data->client_ipv4v6    = in_object_data->client_ipv4v6;
		am_xfer_data->origin_ipv4v6    = in_object_data->origin_ipv4v6;
		AM_copy_client_data_to_xfer_data(pk->provider_id, in_object_data,
					am_xfer_data);
	    }

	}

	if(am_xfer_data->in_cod == NKN_COD_NULL) {
	    if(pk->name) {
		am_xfer_data->in_cod = nkn_cod_open(pk->name, strlen(pk->name),
						    NKN_COD_AM_MGR);
	    }
	    if(am_xfer_data->in_cod == NKN_COD_NULL)
		goto error;
	}
    }

    am_xfer_data->flags |= AM_FLAG_ADD_ENTRY;

    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[tid]);
    TAILQ_INSERT_TAIL(&nkn_am_xfer_list[tid], am_xfer_data,
		      xfer_entries);
    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[tid]);

    AO_fetch_and_add1(&glob_bm_to_am_hotness_update);
    AO_fetch_and_add1(&glob_bm_to_am_hotness_update_queue);
    pthread_cond_signal(&am_ingest_thr_cond);

    return in_seed;

error:;

    if(attr_buf && attr_buffer_held) {
	nkn_buffer_release(attr_buf);
	AO_fetch_and_sub1(&glob_am_push_ingest_attr_buffer_hold);
    }

    if(am_ext_info) {
	if(data_buffer_held) {
	    for(i=0;i<am_ext_info->num_bufs;i++) {
		nkn_buffer_release(am_ext_info->buffer[i]);
	    }
	    AO_fetch_and_add(&glob_am_push_ingest_data_buffer_hold,
				-am_ext_info->num_bufs);
	}
	nkn_mm_free_memory(nkn_am_xfer_ext_data_pool[tid],
			   am_ext_info);
	AO_fetch_and_sub1(&nkn_am_ext_data_in_use[tid]);
    }

    if(am_xfer_data) {
	nkn_mm_free_memory(nkn_am_xfer_data_pool[tid], am_xfer_data);
	AO_fetch_and_sub1(&nkn_am_xfer_data_in_use[tid]);
    }

    return 0;

}

static void
s_am_copy_client_data(nkn_provider_type_t ptype, void *in_proto_data,
		    void *in_host_hdr, void **out_proto_data,
		    void **out_host_hdr)
{

    switch(ptype) {
	case OriginMgr_provider:
	    /* In case of OM, copy both the host header and con data */
	    if(in_host_hdr)
		nkn_copy_host_hdr(in_host_hdr, out_host_hdr);
	    else
		nkn_copy_con_host_hdr(in_proto_data, out_host_hdr);
	    nkn_copy_con_client_data(0, in_proto_data, out_proto_data);
	    break;
	case RTSPMgr_provider:
	    nkn_copy_orp_data(in_proto_data, out_proto_data);
	    break;
	default:
	    break;
    }
}

void
AM_copy_client_data(nkn_provider_type_t ptype,
			 am_object_data_t *in_object_data,
			 am_object_data_t *out_object_data)
{
    s_am_copy_client_data(ptype, in_object_data->proto_data,
			  in_object_data->host_hdr,
			  &out_object_data->proto_data,
			  &out_object_data->host_hdr);
}

void
AM_copy_client_data_from_xfer_data(nkn_provider_type_t ptype,
			 am_xfer_data_t *in_am_xfer_data,
			 am_object_data_t *out_object_data)
{
    s_am_copy_client_data(ptype, in_am_xfer_data->proto_data,
			  in_am_xfer_data->host_hdr,
			  &out_object_data->proto_data,
			  &out_object_data->host_hdr);
}

void
AM_copy_client_data_to_xfer_data(nkn_provider_type_t ptype,
			 am_object_data_t *in_object_data,
			 am_xfer_data_t *am_xfer_data)
{
    s_am_copy_client_data(ptype, in_object_data->proto_data,
			  in_object_data->host_hdr,
			  &am_xfer_data->proto_data,
			  &am_xfer_data->host_hdr);
}


static void
s_nkn_am_cleanup_client_data(nkn_provider_type_t ptype, void *proto_data,
			     void *host_hdr)
{
    switch(ptype) {
	case OriginMgr_provider:
	    if(host_hdr)
		nkn_free_con_host_hdr(host_hdr);
	    if(proto_data)
		nkn_free_con_client_data(proto_data);
	    break;
	case RTSPMgr_provider:
	    if(proto_data)
		nkn_free_orp_data(proto_data);
	    break;
	default:
	    break;
    }
}

void
AM_cleanup_xfer_client_data(nkn_provider_type_t ptype,
			    am_xfer_data_t *am_xfer_data)
{
    s_nkn_am_cleanup_client_data(ptype, am_xfer_data->proto_data,
				 am_xfer_data->host_hdr);
    am_xfer_data->proto_data = NULL;
    am_xfer_data->host_hdr   = NULL;
}

void
AM_cleanup_client_data(nkn_provider_type_t ptype, am_object_data_t *obj_data)
{
    s_nkn_am_cleanup_client_data(ptype, obj_data->proto_data,
				 obj_data->host_hdr);
    obj_data->proto_data = NULL;
    obj_data->host_hdr   = NULL;
}

void
AM_init_hotness(nkn_hot_t *out_hotness)
{
    am_encode_hotness(out_hotness, 0);
    am_encode_update_time(out_hotness, 0);
}

void
AM_init_provider_thresholds(AM_pk_t *pk, uint32_t max_io_throughput,
				 int32_t hotness_threshold)
{
    nkn_am_provider_data[pk->provider_id].state = AM_PROVIDER_ENABLED;
    nkn_am_provider_data[pk->provider_id].max_io_throughput =
						max_io_throughput;
    nkn_am_provider_data[pk->provider_id].hotness_threshold =
						hotness_threshold;
}

void
AM_init()
{
    int ret, i;
    char xfer_name[255];
    char counter_str[512];

    /* Scheduler thread specific xfer queues
     * Last queue is for non-scheduler threads
     */
    for(i=0;i<=glob_sched_num_core_threads;i++) {
	nkn_am_xfer_data_pool[i] = nkn_mm_mem_pool_init(
					    200000/glob_sched_num_core_threads,
					    sizeof(am_xfer_data_t),
					    mod_am_xfer_data);
	nkn_am_xfer_ext_data_pool[i] = nkn_mm_mem_pool_init(
					    100000/glob_sched_num_core_threads,
					    sizeof(am_ext_info_t),
					    mod_am_xfer_ext_data);

	snprintf(xfer_name, 255, "am-xfer-mutex-%d", i);
	ret = NKN_MUTEX_INIT(&nkn_am_xfer_lock[i], NULL, xfer_name);
	if(ret < 0) {
	    assert(0);
	}

	TAILQ_INIT(&nkn_am_xfer_list[i]);

	snprintf(counter_str, 512, "am_xfer_data_%d_use_cnt", i);
	(void)nkn_mon_add(counter_str, NULL,
			    (void *)&nkn_am_xfer_data_in_use[i],
			    sizeof(nkn_am_xfer_data_in_use[i]));

	snprintf(counter_str, 512, "am_ext_data_%d_use_cnt", i);
	(void)nkn_mon_add(counter_str, NULL,
			    (void *)&nkn_am_ext_data_in_use[i],
			    sizeof(nkn_am_ext_data_in_use[i]));
    }

    nkn_am_list_init();
    nkn_am_tbl_init();
    nkn_am_ingest_init();
}

void
nkn_am_ingest_init(void)
{
    GError *err1 = NULL;
    int ret_val;
    pthread_condattr_t a;

    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);

    pthread_cond_init(&am_ingest_thr_cond, &a);

    ret_val = pthread_mutex_init(&am_ingest_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_AM, "AM Promote Mutex not created. ");
        return;
    }

    am_ingest_thread = g_thread_create_full(
				      (GThreadFunc)s_am_process_thread,
				      NULL, (128*1024), TRUE,
				      FALSE, G_THREAD_PRIORITY_NORMAL, &err1);
    if(am_ingest_thread == NULL) {
	g_error_free ( err1 ) ;
    }
}

int
AM_delete_push_ingest(nkn_cod_t in_cod, void *attr_buf)
{
    am_xfer_data_t *am_xfer_data;

    am_xfer_data = nkn_mm_get_memory(
			nkn_am_xfer_data_pool[glob_sched_num_core_threads]);
    if(!am_xfer_data) {
	return -1;
    }

    am_xfer_data->in_cod = nkn_cod_dup(in_cod, NKN_COD_AM_MGR);
    if(am_xfer_data->in_cod == NKN_COD_NULL) {
	nkn_mm_free_memory(nkn_am_xfer_data_pool[glob_sched_num_core_threads],
			   am_xfer_data);
	return -1;
    }

    AO_fetch_and_add1(&nkn_am_xfer_data_in_use[glob_sched_num_core_threads]);
    am_xfer_data->flags   |= (AM_FLAG_ADD_ENTRY | AM_FLAG_PUSH_INGEST_DEL);
    if(attr_buf) {
	am_xfer_data->attr_buf = attr_buf;
	AO_fetch_and_add1(&glob_am_push_ingest_attr_buffer_hold);
	nkn_buffer_hold(attr_buf);
    }
    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    TAILQ_INSERT_TAIL(&nkn_am_xfer_list[glob_sched_num_core_threads],
		      am_xfer_data, xfer_entries);
    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    pthread_cond_signal(&am_ingest_thr_cond);

    return 0;
}

int
AM_delete_obj(nkn_cod_t in_cod, char *uri)
{
    am_xfer_data_t *am_xfer_data;

    am_xfer_data = nkn_mm_get_memory(
			nkn_am_xfer_data_pool[glob_sched_num_core_threads]);
    if(!am_xfer_data) {
	return -1;
    }

    if(in_cod != NKN_COD_NULL)
	am_xfer_data->in_cod = nkn_cod_dup(in_cod, NKN_COD_AM_MGR);
    else if(uri)
	am_xfer_data->in_cod = nkn_cod_open(uri, strlen(uri), NKN_COD_AM_MGR);

    if(am_xfer_data->in_cod == NKN_COD_NULL) {
	nkn_mm_free_memory(nkn_am_xfer_data_pool[glob_sched_num_core_threads],
			   am_xfer_data);
	return -1;
    }

    AO_fetch_and_add1(&nkn_am_xfer_data_in_use[glob_sched_num_core_threads]);
    am_xfer_data->flags |= AM_FLAG_DEL_ENTRY;
    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    TAILQ_INSERT_TAIL(&nkn_am_xfer_list[glob_sched_num_core_threads],
		      am_xfer_data, xfer_entries);
    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    pthread_cond_signal(&am_ingest_thr_cond);
    return 0;
}

void
AM_change_obj_provider(nkn_cod_t in_cod, nkn_provider_type_t dst_ptype)
{
    am_xfer_data_t *am_xfer_data = NULL;


    am_xfer_data = nkn_mm_get_memory(
			nkn_am_xfer_data_pool[glob_sched_num_core_threads]);
    if(!am_xfer_data) {
	return;
    }

    am_xfer_data->in_cod      = nkn_cod_dup(in_cod, NKN_COD_AM_MGR);
    if(am_xfer_data->in_cod == NKN_COD_NULL) {
	nkn_mm_free_memory(nkn_am_xfer_data_pool[glob_sched_num_core_threads],
			   am_xfer_data);
	return;
    }
    AO_fetch_and_add1(&nkn_am_xfer_data_in_use[glob_sched_num_core_threads]);
    am_xfer_data->provider_id = dst_ptype;
    am_xfer_data->flags      |= (AM_FLAG_ADD_ENTRY | AM_FLAG_SET_INGEST_COMPLETE);

    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    TAILQ_INSERT_TAIL(&nkn_am_xfer_list[glob_sched_num_core_threads], am_xfer_data,
		      xfer_entries);
    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);

    pthread_cond_signal(&am_ingest_thr_cond);
}

void
AM_set_dont_ingest(nkn_cod_t in_cod)
{
    AM_delete_obj(in_cod, NULL);
}

void
AM_set_ingest_error(nkn_cod_t in_cod, int retry)
{
    am_xfer_data_t *am_xfer_data;

    am_xfer_data = nkn_mm_get_memory(
			nkn_am_xfer_data_pool[glob_sched_num_core_threads]);
    if(!am_xfer_data) {
	return;
    }

    am_xfer_data->in_cod = nkn_cod_dup(in_cod, NKN_COD_AM_MGR);
    if(am_xfer_data->in_cod == NKN_COD_NULL) {
	nkn_mm_free_memory(nkn_am_xfer_data_pool[glob_sched_num_core_threads],
			   am_xfer_data);
	return;
    }
    AO_fetch_and_add1(&nkn_am_xfer_data_in_use[glob_sched_num_core_threads]);
    am_xfer_data->in_cod = in_cod;
    am_xfer_data->flags |= AM_FLAG_SET_INGEST_ERROR;
    if(retry)
	am_xfer_data->flags |= AM_FLAG_SET_INGEST_RETRY;
    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    TAILQ_INSERT_TAIL(&nkn_am_xfer_list[glob_sched_num_core_threads],
		      am_xfer_data, xfer_entries);
    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    pthread_cond_signal(&am_ingest_thr_cond);
    return;
}

void
AM_set_small_obj_hotness(nkn_cod_t in_cod, int hotness_threshold)
{
    am_xfer_data_t *am_xfer_data;

    am_xfer_data = nkn_mm_get_memory(
			nkn_am_xfer_data_pool[glob_sched_num_core_threads]);
    if(!am_xfer_data) {
	return;
    }

    am_xfer_data->in_cod = nkn_cod_dup(in_cod, NKN_COD_AM_MGR);
    if(am_xfer_data->in_cod == NKN_COD_NULL) {
	nkn_mm_free_memory(nkn_am_xfer_data_pool[glob_sched_num_core_threads],
			   am_xfer_data);
	return;
    }
    AO_fetch_and_add1(&nkn_am_xfer_data_in_use[glob_sched_num_core_threads]);
    am_xfer_data->in_cod = in_cod;
    am_xfer_data->flags |= AM_FLAG_SET_SMALL_OBJ_HOT;
    am_xfer_data->obj_hotness = hotness_threshold;
    NKN_MUTEX_LOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    TAILQ_INSERT_TAIL(&nkn_am_xfer_list[glob_sched_num_core_threads],
		      am_xfer_data, xfer_entries);
    NKN_MUTEX_UNLOCK(&nkn_am_xfer_lock[glob_sched_num_core_threads]);
    pthread_cond_signal(&am_ingest_thr_cond);
    return;
}

void
AM_ingest_wakeup()
{
    pthread_cond_signal(&am_ingest_thr_cond);
}


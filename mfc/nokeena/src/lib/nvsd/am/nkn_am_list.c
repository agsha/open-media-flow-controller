/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Analytics Manager
 *  lists for ingestion/promtion
 *
 * Author: Jeya ganesh babu
 *
 */

#include "nkn_am_api.h"
#include "nkn_am.h"
#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_mediamgr_api.h"
#include "nkn_assert.h"
#include "nkn_static_st_queue.h"
#define AM_LIST_OPTIMIZED 1

nkn_mutex_t nkn_am_extern_mutex;

TAILQ_HEAD(nkn_am_ingest_cdt_list_t, AM_analytics_obj)
    nkn_am_ingest_cdt_list[NKN_MM_MAX_CACHE_PROVIDERS];
TAILQ_HEAD(nkn_am_provider_list_t, AM_analytics_obj)
    nkn_am_provider_list[NKN_MM_MAX_CACHE_PROVIDERS];

nkn_st_queue_t *nkn_am_ingest_list[NKN_MM_MAX_CACHE_PROVIDERS];
nkn_st_queue_t *nkn_am_hp_ingest_list[NKN_MM_MAX_CACHE_PROVIDERS];

uint32_t am_cache_ingest_policy = 1;
uint32_t glob_am_promote_num_hyb_entries = 1000;

/* Error Counters */
uint64_t glob_am_invalid_add_provider_entry_err;
uint64_t glob_am_invalid_add_ingest_entry_err;

/* Debug counters */
uint64_t nkn_am_promote_ingest_cdt_add_cnt[NKN_MM_MAX_CACHE_PROVIDERS];
uint64_t nkn_am_promote_ingest_list_add_cnt[NKN_MM_MAX_CACHE_PROVIDERS];
uint64_t nkn_am_promote_ingest_list_walk[NKN_MM_MAX_CACHE_PROVIDERS];
uint64_t nkn_am_object_evict_cnt[NKN_MM_MAX_CACHE_PROVIDERS];

uint64_t glob_am_ssd_ingest_ageout_cnt;
uint64_t glob_am_sas_ingest_ageout_cnt;
uint64_t glob_am_sata_ingest_ageout_cnt;
uint64_t glob_am_origin_entry_ageout_cnt;
uint64_t glob_am_origin_entry_error_ageout_cnt;
uint64_t nkn_am_hp_entries[NKN_MM_MAX_CACHE_PROVIDERS];

static
void s_am_list_add_ingest_queue_entries(int list_index, int count);

static int
s_am_find_lowest_cache_tier_for_ingestion(AM_obj_t *objp)
{
    int			i;
    int			ret;
    MM_provider_stat_t  pstat;
    uint32_t		obj_hotness;
    uint32_t		hotness_threshold;

    hotness_threshold = nkn_get_namespace_cache_ingest_hotness_threshold(
				objp->ns_token);

    if(!hotness_threshold)
	hotness_threshold = AM_DEF_HOT_THRESHOLD;

    obj_hotness = am_decode_hotness(&objp->hotness);

    for(i = (NKN_MM_max_pci_providers - 1); i >= Unknown_provider; i--) {
	if(nkn_am_provider_data[i].state == AM_PROVIDER_DISABLED)
	    continue;

	if(obj_hotness < hotness_threshold)
	    continue;

	pstat.ptype = i;
	pstat.sub_ptype = 0;
	/* Find out the cache threshold for this level*/
	ret = MM_provider_stat(&pstat);
	if(ret < 0) {
	    continue;
	}
	if(pstat.caches_enabled == 0)
	    continue;
	return i;
    }
    return 0;
}

static int
s_am_find_highest_cache_tier_for_ingestion(void)
{
    int			i;
    int			ret;
    MM_provider_stat_t  pstat;

    for(i = Unknown_provider; i < NKN_MM_max_pci_providers; i++) {
	if(nkn_am_provider_data[i].state == AM_PROVIDER_DISABLED)
	    continue;
	pstat.ptype = i;
	pstat.sub_ptype = 0;
	/* Find out the cache threshold for this level*/
	ret = MM_provider_stat(&pstat);
	if(ret < 0) {
	    continue;
	}
	if(pstat.caches_enabled == 0)
	    continue;
	return i;
    }
    return 0;
}

static int
s_am_find_next_cache_tier_for_ingestion(AM_obj_t *objp)
{
    nkn_provider_type_t	i;
    int			ret;
    MM_provider_stat_t  pstat;
    uint32_t		obj_hotness;
    uint32_t		hotness_threshold;

    hotness_threshold = nkn_get_namespace_cache_ingest_hotness_threshold(
				objp->ns_token);

    if(!hotness_threshold)
	hotness_threshold = AM_DEF_HOT_THRESHOLD;


    obj_hotness = am_decode_hotness(&objp->hotness);
    for(i = Unknown_provider; i < objp->pk.provider_id; i++) {

	if(nkn_am_provider_data[i].state == AM_PROVIDER_DISABLED)
	    continue;

	if(obj_hotness >= (hotness_threshold * hotness_scale[i])) {
	    pstat.ptype = i;
	    pstat.sub_ptype = 0;
	    /* Find out the cache threshold for this level*/
	    ret = MM_provider_stat(&pstat);
	    if(ret < 0) {
		continue;
	    }
	    if(pstat.caches_enabled == 0)
		continue;
	    return i;
	}
    }
    return objp->pk.provider_id;
}

static int
s_am_check_cache_tier_for_ingestion(int tier)
{
    MM_provider_stat_t  pstat;
    int			ret;

    if(nkn_am_provider_data[tier].state == AM_PROVIDER_DISABLED)
	    return 0;

    pstat.ptype = tier;
    pstat.sub_ptype = 0;
    /* Find out the cache threshold for this level*/
    ret = MM_provider_stat(&pstat);
    if(ret == 0) {
	if(pstat.caches_enabled)
	    return tier;
    }
    return 0;
}

/* Add to High Priority list
 * Add to this list is always FIFO
 */
static
void s_am_list_add_hp_ingest_queue_entries(int list_index)
{

    AM_obj_t *objp = NULL, *objp_temp = NULL;

    objp = TAILQ_FIRST(&nkn_am_ingest_cdt_list[list_index]);
    TAILQ_FOREACH_SAFE(objp, &nkn_am_ingest_cdt_list[list_index],
			ingest_cdt_entries, objp_temp) {

	if(nkn_am_hp_entries[list_index] <=
	    nkn_st_queue_entries(nkn_am_hp_ingest_list[list_index]))
	    return;

	if(!(objp->policy & AM_POLICY_HP_QUEUE))
	    continue;

	if(!(objp->flags & AM_FLAG_HP_ADDED)) {
	    objp->hp_id = nkn_st_queue_insert(nkn_am_hp_ingest_list[list_index],
					      objp);
	    if(objp->hp_id == -1)
		return;

	    objp->flags |= AM_FLAG_HP_ADDED;
	}
    }
}

AM_obj_t *
nkn_am_list_get_hp_ingest_entry(AM_pk_t *pk)
{

    AM_obj_t *objp;
    int      list_index = pk->provider_id;
    int      obj_id = -1, next_obj_id = 0;

    if(!nkn_am_hp_entries[pk->provider_id])
	return 0;

    if((nkn_am_hp_entries[pk->provider_id] >
	    nkn_st_queue_entries(nkn_am_hp_ingest_list[list_index])) &&
	(nkn_st_queue_entries(nkn_am_hp_ingest_list[list_index]) <
	    MAX_AM_INGEST_QUEUE_ENTRIES))
	    s_am_list_add_hp_ingest_queue_entries(list_index);

    while(1) {

	if(next_obj_id == -1)
	    break;

	objp = (AM_obj_t *)nkn_st_queue_and_next_id(nkn_am_hp_ingest_list[list_index],
						obj_id, &next_obj_id);

	if(!objp)
	    return objp;

	obj_id = next_obj_id;

	if(objp->flags & AM_FLAG_INGEST_PENDING )
	    continue;

	if(objp->flags & (AM_FLAG_INGEST_SKIP |
				AM_FLAG_INGEST_ERROR)) {
	    nkn_am_ingest_list_delete(objp);
	    continue;
	}
	return objp;

    }

    return NULL;
}

AM_obj_t *
nkn_am_list_get_hottest(AM_pk_t *pk)
{
    uint32_t list_index = 0;
    AM_obj_t *objp = NULL, *objp_temp = NULL;
    uint32_t list_walk_cnt = 0, list_pending_cnt = 0;
    int      obj_id = -1, next_obj_id = 0;

    /* Check the high priority queue first */
    objp = nkn_am_list_get_hp_ingest_entry(pk);
    if(objp)
	return objp;

    list_index = pk->provider_id;
    if(am_cache_ingest_policy == AM_INGEST_POLICY_AUTO ||
	     am_cache_ingest_policy == AM_INGEST_POLICY_NAMESPACE) {
	objp = TAILQ_FIRST(&nkn_am_ingest_cdt_list[list_index]);
	TAILQ_FOREACH_SAFE(objp, &nkn_am_ingest_cdt_list[list_index],
		ingest_cdt_entries, objp_temp) {
	    if(!objp) {
		NKN_ASSERT(0);
		return NULL;
	    }

	    /* Bug: 3238, 3178, in case of object already pending for ingest
	    * or having ingest error or dont_ingest bit is set, then dont give it
	    * for promotion/ingestion
	    */
	    if(objp->flags & AM_FLAG_INGEST_PENDING )
		continue;

	    if(objp->flags & (AM_FLAG_INGEST_SKIP |
				AM_FLAG_INGEST_ERROR)) {
		nkn_am_ingest_list_delete(objp);
		continue;
	    }
	    return objp;
	}
    } else {


	while(1) {

	    if((next_obj_id == -1))
		break;

	    objp = (AM_obj_t *)nkn_st_queue_and_next_id(nkn_am_ingest_list[list_index],
						obj_id, &next_obj_id);

	    if(!objp)
		break;

	    obj_id = next_obj_id;

	    list_walk_cnt++;
	    nkn_am_promote_ingest_list_walk[list_index]++;

	    if(objp->flags & AM_FLAG_INGEST_PENDING) {
		list_pending_cnt++;
		continue;
	    }

	    if(objp->flags & (AM_FLAG_INGEST_SKIP |
				    AM_FLAG_INGEST_ERROR)) {
		nkn_am_ingest_list_delete(objp);
		continue;
	    }

	    return objp;

	}

	if(list_walk_cnt == 0 || (list_walk_cnt &&
		(list_walk_cnt < glob_am_promote_num_hyb_entries / 2) &&
		(list_walk_cnt == list_pending_cnt)))
	    s_am_list_add_ingest_queue_entries(list_index,
				    (glob_am_promote_num_hyb_entries/2));

    }
    return NULL;
}

void
nkn_am_update_ingest_list(AM_obj_t *objp)
{

    uint32_t cache_ingest_hotness_threshold;

    /* Bug 6205 - Need to remove the entry from the ingest lists before
     * changing the next provider_id
     */
    nkn_am_ingest_list_delete(objp);
    objp->next_provider_id = 0;
    switch(objp->policy) {
	case AM_POLICY_DONT_INGEST:
	    objp->next_provider_id = 0;
	    break;

	case AM_POLICY_ONLY_SATA:
	    if(objp->pk.provider_id != SATADiskMgr_provider) {
		objp->next_provider_id =
		    s_am_check_cache_tier_for_ingestion(SATADiskMgr_provider);
	    } else
		objp->next_provider_id = SATADiskMgr_provider;
	    break;

	case AM_POLICY_ONLY_SAS:
	    if(objp->pk.provider_id != SAS2DiskMgr_provider) {
		objp->next_provider_id =
		    s_am_check_cache_tier_for_ingestion(SAS2DiskMgr_provider);
	    } else
		objp->next_provider_id = SAS2DiskMgr_provider;
	    break;

	case AM_POLICY_ONLY_SSD:
	    if(objp->pk.provider_id != SolidStateMgr_provider) {
		objp->next_provider_id =
		    s_am_check_cache_tier_for_ingestion(SolidStateMgr_provider);
	    } else
		objp->next_provider_id = SolidStateMgr_provider;
	    break;

	case AM_POLICY_ONLY_HI_TIER:
	    objp->next_provider_id =
		s_am_find_highest_cache_tier_for_ingestion();
	    break;

	case AM_POLICY_ONLY_LO_TIER:
	    objp->next_provider_id =
		s_am_find_lowest_cache_tier_for_ingestion(objp);
	    break;

	default:
	    cache_ingest_hotness_threshold =
		    nkn_get_namespace_cache_ingest_hotness_threshold(
					    objp->ns_token);
	    if(objp->pk.provider_id > NKN_MM_max_pci_providers) {
		objp->next_provider_id =
		    s_am_find_lowest_cache_tier_for_ingestion(objp);
	    } else {
		objp->next_provider_id =
		    s_am_find_next_cache_tier_for_ingestion(objp);
	    }
	    break;
    }
    if(objp->next_provider_id && (objp->next_provider_id != objp->pk.provider_id))
	nkn_am_list_add_ingest_cdt_entry(objp);
}

static
void s_am_list_add_ingest_queue_entries(int list_index, int count)
{
    int i;
    AM_obj_t     *objp = NULL;

    objp = TAILQ_LAST(&nkn_am_ingest_cdt_list[list_index],
		      nkn_am_ingest_cdt_list_t);
    for(i=0;i<count;i++) {
	if(!objp)
	    break;
	if(objp->flags & (AM_FLAG_INGEST_ERROR | AM_FLAG_INGEST_SKIP |
		    AM_FLAG_INGEST_PENDING | AM_FLAG_INGEST_ADDED)) {
	    objp = TAILQ_PREV(objp, nkn_am_ingest_cdt_list_t,
			      ingest_cdt_entries);
	    continue;
	}

	objp->ingest_id = nkn_st_queue_insert(nkn_am_ingest_list[list_index],
					      objp);

	if(objp->ingest_id == -1)
	    break;

	//TAILQ_INSERT_HEAD(&nkn_am_ingest_list[list_index], objp,
	//		    ingest_entries);
	objp->flags |= AM_FLAG_INGEST_ADDED;
	nkn_am_promote_ingest_cdt_add_cnt[list_index]++;
	objp = TAILQ_PREV(objp, nkn_am_ingest_cdt_list_t, ingest_cdt_entries);

    }
}


void
nkn_am_list_remove_hp_ingest_entry(AM_obj_t *objp)
{
    if(objp->flags & AM_FLAG_HP_ADDED) {
	if(!nkn_st_queue_remove(nkn_am_hp_ingest_list[objp->next_provider_id],
			    objp->hp_id)) {
	    objp->flags &= ~AM_FLAG_HP_ADDED;
	    objp->hp_id = -1;
	} else
	    NKN_ASSERT(0);
    }
}

void
nkn_am_list_remove_ingest_entry(AM_obj_t *objp)
{
    if(objp->flags & AM_FLAG_INGEST_ADDED) {
	if(!nkn_st_queue_remove(nkn_am_ingest_list[objp->next_provider_id],
			    objp->ingest_id)) {
	    objp->flags &= ~AM_FLAG_INGEST_ADDED;
	    objp->ingest_id = -1;
	} else
	    NKN_ASSERT(0);
    }
}

void
nkn_am_list_add_ingest_cdt_entry(AM_obj_t *objp)
{
    int ingest_policy;
    if((objp->pk.provider_id  >= NKN_MM_max_pci_providers) &&
	(objp->in_object_data)) {
	if(am_cache_ingest_policy == AM_INGEST_POLICY_AUTO) {
	    ingest_policy =
		nkn_get_proxy_mode_ingest_policy(objp->in_object_data->ns_token);
	} else if(am_cache_ingest_policy == AM_INGEST_POLICY_NAMESPACE) {
	    ingest_policy =
		nkn_get_namespace_ingest_policy(objp->in_object_data->ns_token);
	} else {
	    ingest_policy = INGEST_POLICY_FIFO;
	}
    } else {
	if(am_cache_ingest_policy == AM_INGEST_POLICY_AUTO ||
		am_cache_ingest_policy == AM_INGEST_POLICY_NAMESPACE)
	    ingest_policy = INGEST_POLICY_LIFO;
	else
	    ingest_policy = INGEST_POLICY_FIFO;

    }
    if(!(objp->flags & AM_FLAG_INGEST_CDT_ADDED)) {
	if(ingest_policy == INGEST_POLICY_LIFO)
	    TAILQ_INSERT_HEAD(&nkn_am_ingest_cdt_list[objp->next_provider_id],
		objp, ingest_cdt_entries);
	else
	    TAILQ_INSERT_TAIL(&nkn_am_ingest_cdt_list[objp->next_provider_id],
		objp, ingest_cdt_entries);
	objp->flags |= AM_FLAG_INGEST_CDT_ADDED;
	nkn_am_promote_ingest_list_add_cnt[objp->next_provider_id]++;

	if(objp->policy & AM_POLICY_HP_QUEUE)
	    nkn_am_hp_entries[objp->next_provider_id]++;

    } else {
	NKN_ASSERT(0);
	glob_am_invalid_add_ingest_entry_err++;
    }
}


void
nkn_am_list_remove_ingest_cdt_entry(AM_obj_t *objp)
{
    if(objp->flags & AM_FLAG_INGEST_CDT_ADDED) {
	TAILQ_REMOVE(&nkn_am_ingest_cdt_list[objp->next_provider_id],
		objp, ingest_cdt_entries);
	objp->flags &= ~AM_FLAG_INGEST_CDT_ADDED;
	if(objp->policy & AM_POLICY_HP_QUEUE)
	    nkn_am_hp_entries[objp->next_provider_id]--;
    }
}

void
nkn_am_list_add_provider_entry(AM_obj_t *objp)
{
    NKN_MUTEX_LOCK(&nkn_am_extern_mutex);
    if(!(objp->flags & AM_FLAG_PROVIDER_ADDED)) {
	TAILQ_INSERT_TAIL(&nkn_am_provider_list[objp->pk.provider_id],
		objp, lru_entries);
	objp->flags |= AM_FLAG_PROVIDER_ADDED;
    } else {
	NKN_ASSERT(0);
	glob_am_invalid_add_provider_entry_err++;
    }
    NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
}

void
nkn_am_list_remove_provider_entry(AM_obj_t *objp)
{
    NKN_MUTEX_LOCK(&nkn_am_extern_mutex);
    if(objp->flags & AM_FLAG_PROVIDER_ADDED) {
	TAILQ_REMOVE(&nkn_am_provider_list[objp->pk.provider_id],
		objp, lru_entries);
	objp->flags &= ~AM_FLAG_PROVIDER_ADDED;
    }
    NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
}

void
nkn_am_list_update_provider_entry(AM_obj_t *objp)
{
    NKN_MUTEX_LOCK(&nkn_am_extern_mutex);
    if(objp->flags & AM_FLAG_PROVIDER_ADDED) {
	TAILQ_REMOVE(&nkn_am_provider_list[objp->pk.provider_id],
		objp, lru_entries);
    }
    TAILQ_INSERT_TAIL(&nkn_am_provider_list[objp->pk.provider_id],
	    objp, lru_entries);
    objp->flags |= AM_FLAG_PROVIDER_ADDED;
    NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
}

void
nkn_am_ingest_list_delete(AM_obj_t *objp)
{
    /* Ingest list delete */
    nkn_am_list_remove_ingest_entry(objp);

    /* HP Ingest list delete */
    nkn_am_list_remove_hp_ingest_entry(objp);

    /* Promote list delete */
    nkn_am_list_remove_ingest_cdt_entry(objp);

}

void
nkn_am_list_delete(AM_obj_t *objp)
{
    /* Remove from all Ingest lists */
    nkn_am_ingest_list_delete(objp);

    /* Remove from provider list */
    nkn_am_list_remove_provider_entry(objp);
}

int AM_get_next_evict_candidate_by_provider(nkn_provider_type_t ptype, AM_pk_t *pk)
{
    AM_obj_t *objp;
    int	     ret;
    NKN_MUTEX_LOCK(&nkn_am_extern_mutex);
    objp = TAILQ_FIRST(&nkn_am_provider_list[ptype]);
    if(objp) {
	strcpy(pk->name, objp->pk.name);
	pk->provider_id = objp->pk.provider_id;
	ret = 0;
    } else
	ret = NKN_AM_OBJ_NOT_FOUND;
    NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
    return ret;
}

/*
 * nkn_am_tbl_age_out_entry()
 * This evicts older entries from the AM table when the table
 * gets full
 * ptype - provider type for the new entry
 */
void nkn_am_tbl_age_out_entry(nkn_provider_type_t ptype)
{
    nkn_provider_type_t iptype, start_ptype = 0, end_ptype = 0;
    AM_obj_t *objp = NULL, *evict_objp = NULL;
    time_t obj_time, evict_obj_time = 0;

    /* In case new provider is origin, try evicting disk and origin entries
     * In case new provider is disk, only evict disk entries */
    if(ptype > NKN_MM_max_pci_providers) {
	start_ptype = NKN_MM_max_pci_providers;
	end_ptype = NKN_MM_MAX_CACHE_PROVIDERS;
    } else {
	start_ptype = 0;
	end_ptype = NKN_MM_max_pci_providers;
    }


    NKN_MUTEX_LOCK(&nkn_am_extern_mutex);
    for(iptype = start_ptype; iptype < end_ptype; iptype++) {
	objp = TAILQ_FIRST(&nkn_am_provider_list[iptype]);
try_again:;
	if(objp) {
	    /* Skip entries added by CIM */
	    if((objp->in_object_data &&
		   (objp->in_object_data->flags & AM_CIM_INGEST)) ||
		   (objp->flags & AM_FLAG_INGEST_PENDING) ||
		   (objp->push_ptr)) {
		objp = TAILQ_NEXT(objp, lru_entries);
		goto try_again;
	    } else {
		if(!evict_objp) {
		    evict_objp     = objp;
		    evict_obj_time = am_decode_update_time(&objp->hotness);
		} else {
		    obj_time = am_decode_update_time(&objp->hotness);
		    if(evict_obj_time > obj_time) {
			evict_obj_time = obj_time;
			evict_objp     = objp;
		    }
		}
	    }
	}
    }
    NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
    if(evict_objp) {
	if(evict_objp->pk.provider_id < NKN_MM_max_pci_providers) {
	    AM_MM_TIER_COUNTER_INC(glob_am, evict_objp->pk.provider_id,
				ingest_ageout_cnt);
	} else {
	    if (evict_objp->flags & AM_FLAG_SET_INGEST_ERROR)
		glob_am_origin_entry_error_ageout_cnt++;
	    else {
		glob_am_origin_entry_ageout_cnt++;
		if (glob_log_cache_ingest_failure) {
		    DBG_CACHE(" INGESTFAIL: \"%s\" - - - - - - - - - ageout", 
			      evict_objp->pk.name);
		}

	    }
	}
	nkn_am_tbl_delete_object(evict_objp);
    }
}

void
AM_print_entries(FILE *fp)
{
    int provider;
    AM_obj_t *objp;
    MM_provider_stat_t  pstat;
    int ret;
    int hotness;

    for(provider=0;provider<NKN_MM_max_pci_providers;provider++) {
	NKN_MUTEX_LOCK(&nkn_am_extern_mutex);
	objp = TAILQ_FIRST(&nkn_am_provider_list[provider]);
	if(objp) {
	    pstat.ptype = provider;
	    pstat.sub_ptype = 0;
	    /* Find out the cache threshold for this level*/
	    ret = MM_provider_stat(&pstat);
	    if(ret < 0) {
		NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
		continue;
	    }
	    hotness = pstat.hotness_threshold;
	    fprintf(fp, "---------------------------------------------------------\n");
	    fprintf(fp, "Provider-id\tCache-Enabled\t%% Used\t"
		    "Hotness threshold\n");
	    fprintf(fp, "%d\t\t%s\t\t%3d\t%d\n", provider,
		    (pstat.caches_enabled)?"Yes":"No", pstat.percent_full,
		    hotness);
	    fprintf(fp, "---------------------------------------------------------\n");
	}
	while(objp) {
	    hotness = am_decode_hotness(&objp->hotness);
	    fprintf(fp, "PK: Name: %s\n", objp->pk.name);
	    fprintf(fp, "\tProvider id: %d, Obj: hotness = %d, num hits: %d\n", objp->pk.provider_id, hotness, objp->num_hits);
	    objp = TAILQ_NEXT(objp, lru_entries);
	}
	NKN_MUTEX_UNLOCK(&nkn_am_extern_mutex);
    }
}

void
nkn_am_counter_init(nkn_provider_type_t ptype, char *provider_str)
{
    char counter_str[512];

    snprintf(counter_str, 512, "glob_am_%s_ingest_cdt_add_cnt", provider_str);
    (void)nkn_mon_add(counter_str, NULL,
		    (void *)&nkn_am_promote_ingest_cdt_add_cnt[ptype],
		    sizeof(nkn_am_promote_ingest_cdt_add_cnt[ptype]));

    snprintf(counter_str, 512, "glob_am_%s_ingest_list_add_cnt", provider_str);
    (void)nkn_mon_add(counter_str, NULL,
		    (void *)&nkn_am_promote_ingest_list_add_cnt[ptype],
		    sizeof(nkn_am_promote_ingest_list_add_cnt[ptype]));

    snprintf(counter_str, 512, "glob_am_%s_ingest_list_walk_cnt", provider_str);
    (void)nkn_mon_add(counter_str, NULL,
		    (void *)&nkn_am_promote_ingest_list_walk[ptype],
		    sizeof(nkn_am_promote_ingest_list_walk[ptype]));
}

void
nkn_am_list_init(void)
{
    int i;
    NKN_MUTEX_INIT(&nkn_am_extern_mutex, NULL, "am-evict-mutex");
    for(i=0;i<NKN_MM_MAX_CACHE_PROVIDERS;i++) {
	TAILQ_INIT(&nkn_am_ingest_cdt_list[i]);
	TAILQ_INIT(&nkn_am_provider_list[i]);
	nkn_am_ingest_list[i]    = nkn_st_queue_init(MAX_AM_INGEST_QUEUE_ENTRIES);
	nkn_am_hp_ingest_list[i] = nkn_st_queue_init(MAX_AM_INGEST_QUEUE_ENTRIES);
    }
}


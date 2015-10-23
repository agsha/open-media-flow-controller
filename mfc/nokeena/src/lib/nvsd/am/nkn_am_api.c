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
#include "nkn_mediamgr_api.h"

GThread *am_aging_thread;
uint64_t glob_am_aging_timeout_cnt = 0;
uint64_t glob_change_provider_calls = 0;
uint64_t glob_am_ingest_worthy_check_failed = 0;
uint64_t glob_am_promote_om_to_sata = 0;
uint64_t glob_am_promote_om_to_sas = 0;
uint64_t glob_am_promote_om_to_ssd = 0;
uint64_t glob_am_promote_tfm_to_sata = 0;
uint64_t glob_am_promote_tfm_to_sas = 0;
uint64_t glob_am_promote_tfm_to_ssd = 0;
uint64_t glob_am_promote_nfs_to_sata = 0;
uint64_t glob_am_promote_nfs_to_sas = 0;
uint64_t glob_am_promote_nfs_to_ssd = 0;
uint64_t glob_am_promote_sata_to_sas = 0;
uint64_t glob_am_promote_sata_to_ssd = 0;
uint64_t glob_am_promote_ssd_to_sas = 0;
uint64_t glob_am_tfm_hotness_correction = 0;
extern uint64_t glob_am_promote_attr_size_not_ingested_cnt;

uint64_t am_promote_count_tbl[NKN_MM_MAX_CACHE_PROVIDERS][NKN_MM_MAX_CACHE_PROVIDERS];

static void
s_am_count_promotes(nkn_provider_type_t src, nkn_provider_type_t dst)
{
    if((src != SolidStateMgr_provider) && (src != SATADiskMgr_provider)
       && (src != SAS2DiskMgr_provider) && (src != TFMMgr_provider)
       && (src != NFSMgr_provider) && (src != OriginMgr_provider)
       && (src != RTSPMgr_provider)) {
	return;
    }

    if((dst != SolidStateMgr_provider) && (dst != SATADiskMgr_provider)
       && (dst != SAS2DiskMgr_provider) && (dst != TFMMgr_provider)
       && (dst != NFSMgr_provider) && (dst != OriginMgr_provider)
       && (dst != RTSPMgr_provider)) {
	return;
    }
    am_promote_count_tbl[src][dst] ++;
}

void
AM_print_promote_count(void)
{
    int i, j;
    for(i = SolidStateMgr_provider; i < NKN_MM_MAX_CACHE_PROVIDERS; i++) {
	if((i != SolidStateMgr_provider) && (i != SATADiskMgr_provider)
	   && (i != SAS2DiskMgr_provider) && (i != TFMMgr_provider)
	   && (i != NFSMgr_provider) && (i != OriginMgr_provider)
	   && (i != RTSPMgr_provider)) {
	    continue;
	}
	for(j = NKN_MM_MAX_CACHE_PROVIDERS; j > SolidStateMgr_provider; j--) {
	    if((j != SolidStateMgr_provider) && (j != SATADiskMgr_provider)
	       && (j != SAS2DiskMgr_provider) && (j != TFMMgr_provider)
	       && (j != NFSMgr_provider) && (j != OriginMgr_provider)
	       && (j != RTSPMgr_provider)) {
		continue;
	    }
            if(i > j) {
                continue;
            }
	    printf("\nSrc: %d Dst: %d Number of promotes: %ld", j, i,
		   am_promote_count_tbl[j][i]);
	}
    }
}


void
s_inc_ref_cnt(AM_obj_t *pobj)
{
    return;
    if(!pobj)
	return;
    pobj->ref_cnt ++;
}

void
s_dec_ref_cnt(AM_obj_t *pobj)
{
    return;
    if(!pobj)
	return;
    pobj->ref_cnt --;
}

void
AM_release_obj(AM_obj_t *pobj)
{
    return;
    if(!pobj)
	return;
    s_dec_ref_cnt(pobj);
}

/* This function makes a decision as to whether a video
 * should be cached or not. The decision is based on the
 * 'hotness' number threshold and the last evicted time.
 * */
int
AM_is_video_cache_worthy(AM_pk_t *pk)
{
    AM_obj_t objp;
    AM_obj_type_t type;
    int ret = -1;

    /* At the moment, objects will be cached immediately*/
    return 1;
    assert(pk);
    type = pk->type;

    ret = AM_tbl_get_copy(type, pk, &objp);
    if(ret < 0) {
	AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
	return -1;
    }
    if(objp.pk.name) {
	free(objp.pk.name);
    }
    if(objp.cacheable == 1) {
	return 1;
    }
    return -1;
}

/* This function will decide whether this object should be put on the
   promote list or not. If the object has a hotness greater than the
   lowest cache provider, it is worthy of being on the list.
   Returns a valid cache provider that this object can be promoted into.
*/
static int
s_am_is_ingest_worthy(AM_obj_t *objp, nkn_provider_type_t dst_ptype)
{
    int                 h1, h2;
    int                 i, ret;
    MM_provider_stat_t  pstat;
    nkn_provider_type_t out_ptype = Unknown_provider;
    nkn_provider_type_t start_ptype = Unknown_provider;
    int                 find_next = 0;

    if(dst_ptype <= NKN_MM_max_pci_providers) {
	/* Caller has specified the destination provider
	 and it is a pci provider. */
	start_ptype = dst_ptype;
        find_next = 0;
    } else if(dst_ptype == objp->pk.provider_id) {
        /* Caller does not know which is the next valid provider.
	   Need to start with the next higher pci provider */
        start_ptype = dst_ptype - 1;
        find_next = 1;
        goto failed;
    } else {
        /* We should never be called the dst_ptype not known */
        goto failed;
    }

    if(find_next == 1) {
        /* The caller wants this function to find the next provider */
	for(i = start_ptype;
	    i >= SolidStateMgr_provider; i--) {
	    pstat.ptype = i;
	    pstat.sub_ptype = 0;
	    /* Find out the cache threshold for this level*/
	    ret = MM_provider_stat(&pstat);
	    if(ret < 0) {
		continue;
	    }
	    if(pstat.caches_enabled != 0) {
		out_ptype = i;
		break;
	    }
	}
    } else {
	pstat.ptype = dst_ptype;
	pstat.sub_ptype = 0;
	/* Find out the cache threshold for this level*/
	ret = MM_provider_stat(&pstat);
	if(ret < 0) {
	    goto failed;
	}
	if(pstat.caches_enabled != 0) {
	    out_ptype = dst_ptype;
	}
    }

    if(out_ptype == Unknown_provider) {
       goto failed;
    }

    /* Bug 3290: Check for the attr size with the max attr size
     * supported by the provider. Incase of error return failure
     */
    if(objp->attr_size > pstat.max_attr_size) {
	glob_am_promote_attr_size_not_ingested_cnt ++;
	goto failed;
    }


    /* If the disk is not filled to the low threshold, promote */
    /* The low water mark needs to be check for less than and equal
     * because in DM2, the check is done for greater than.
     */
    if(pstat.percent_full <= pstat.low_water_mark) {
	return 0;
    } else if(pstat.percent_full >= pstat.high_water_mark) {
	goto failed;
    }

    /* If we are between the low and high watermark:
       1. check max throughput possible from disk: if yes, don't change it.
       2. if not, find out if the new object is hotter than the last evicted
          object in disk. This prevents thrashing of disk.
       3. Else don't ingest. */
#if 0
    /* Need DM2 support for this. Turn it on when DM2 is ready. */
    if(pstat.cur_io_throughput >= pstat.max_io_throughput) {
	goto failed;
    }
#endif
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
    }

failed:
    glob_am_ingest_worthy_check_failed ++;
    /* Don't promote */
    return -1;
}

/* This function makes a decision as to whether a video
 * should be cached or not. The decision is based on the
 * 'hotness' number threshold and the last evicted time.
 * Returns success of failure.
 * */
int
AM_is_video_promote_worthy(AM_pk_t *pk, nkn_provider_type_t dst_ptype)
{
    AM_obj_t objp;
    AM_obj_type_t type;
    int ret = 0;

    /* At the moment, objects will be cached immediately*/
    if(!pk)
	return -1;

    type = pk->type;
    ret = AM_tbl_get_copy(type, pk, &objp);
    if(ret < 0) {
	//AM_inc_num_hits(pk, 0, 0, NULL, NULL);
        /* If we are being asked to promote and the entry is not there, 
           we should promote it.*/
	return 0;
    }

    if(objp.promote_pending == 1) {
	goto error;
    }

    if(objp.promoted[dst_ptype] == 1) {
	goto error;
    }

    ret = s_am_is_ingest_worthy(&objp, dst_ptype);
    if(ret < 0) {
	goto error;
    }

    if(objp.pk.name) {
	free(objp.pk.name);
    }
    return 0;

 error:
    if(objp.pk.name) {
	free(objp.pk.name);
    }
    return -1;
}

int
AM_video_demoted(AM_pk_t *pk)
{
    /* At the moment, objects will be cached immediately*/
    return 1;
    AM_tbl_video_demoted(pk);
}

int
AM_set_video_promote_pending(AM_pk_t *pk)
{
    int ret = -1;
    ret = AM_tbl_set_video_promote_pending(pk);
    return (ret);
}

int
AM_set_attr_size(AM_pk_t *pk, int attr_size)
{
    int ret = -1;
    ret = AM_tbl_set_attr_size(pk, attr_size);
    return (ret);
}

int
AM_is_video_promote_pending(AM_pk_t *pk)
{
    int ret = -1;
    ret = AM_tbl_is_video_promote_pending(pk);
    return (ret);
}

int
AM_video_promoted(AM_pk_t *pk)
{
    AM_tbl_video_promoted(pk);
    return 1;
}

void
AM_promote_wakeup(void)
{
    pthread_cond_signal(&am_promote_thr_cond);
}

int
AM_get_next_evict_candidate_by_provider(nkn_provider_type_t provider_id,
				        int32_t sub_provider_id,
					AM_obj_t *pobj)
{
    AM_pk_t 	pk;
    int ret = -1;

    pk.provider_id = provider_id;
    pk.sub_provider_id = sub_provider_id;
    ret = AM_tbl_hotness_get_coldest_by_pk(&pk, pobj);
    if(ret < 0) {
	return -1;
    }
    return 0;
}

int
AM_update_policy(AM_pk_t *pk,
		 AM_trig_pol_t *pol)
{
    int ret = -1;
    AM_obj_type_t type;

    assert(pk);
    AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
    type = pk->type;
    ret = AM_tbl_update_trigger_policy(type, pk, pol);
    return ret;
}

int
AM_delete_obj(char *in_uri, nkn_provider_type_t in_ptype,
	      uint8_t in_sptype __attribute__((unused)),
	      AM_obj_type_t in_type)
{
    int ret = -1;
    AM_pk_t pk;

    pk.type = in_type;
    pk.provider_id = in_ptype;
    pk.sub_provider_id = 0;
    pk.name = in_uri;

    NKN_MUTEX_LOCK(&AM_hash_table_mutex);
    ret = AM_tbl_delete(pk.type, &pk);
    NKN_MUTEX_UNLOCK(&AM_hash_table_mutex);
    return ret;
}

int
AM_get_obj(AM_pk_t *pk, AM_obj_t *outp)
{
    int ret = -1;

    assert(pk);
    ret = AM_tbl_get_copy(pk->type, pk, outp);
    if(ret < 0) {
	return -1;
    }
    return 0;
}

nkn_hot_t
AM_get_obj_hotness(AM_pk_t *pk)
{
    AM_obj_t outp;
    int ret;

    ret = AM_get_obj(pk, &outp);
    if(ret < 0) {
        return 0;
    }
    return outp.hotness;
}

void
AM_set_small_obj_hotness(AM_pk_t *pk)
{
    AM_obj_t *objp;
    int h1;

    NKN_MUTEX_LOCK(&AM_hash_table_mutex);
    objp = AM_tbl_get(pk->type, pk);
    if(objp) {
        h1 = am_decode_hotness(&objp->hotness);
	if(h1 < am_cache_promotion_hotness_threshold * AM_MIN_SSD_HIT_COUNT)
	    h1 += am_cache_promotion_hotness_threshold *
		(AM_MIN_SSD_HIT_COUNT - 1);
        am_encode_hotness(h1, &objp->hotness);
    }
    NKN_MUTEX_UNLOCK(&AM_hash_table_mutex);
    return;
}

int
AM_change_pol_state(AM_pk_t *pk, int state)
{
    return AM_tbl_change_pol_state(pk, state);
}

int
AM_set_promote_error(AM_pk_t *pk)
{
    return AM_tbl_set_promote_error(pk);
}

int
AM_set_dont_ingest(AM_pk_t *pk)
{
    return AM_tbl_set_dont_ingest(pk);
}


int
AM_change_obj_provider(AM_pk_t *pk,
		       nkn_provider_type_t ptype,
		       int32_t sptype)
{
    AM_obj_t	        objp;
    AM_obj_type_t	type;
    nkn_provider_type_t old_ptype;
    int                 ret = -1;
    AM_pk_t             tmp_pk;

    assert(pk);
    type = pk->type;

    old_ptype = pk->provider_id;
    pk->provider_id = ptype;
    ret = AM_tbl_get_copy(type, pk, &objp);
    if(ret >= 0) {
	if(objp.pk.name) {
	    free(objp.pk.name);
	}
        /* The object is already found in the higher provider. Just exit.*/
        return 0;
    }
    pk->provider_id = old_ptype;

    ret = AM_tbl_get_copy(type, pk, &objp);
    if(ret < 0) {
	/* Add a new object in the AM database for the dst ptype if the src ptype is
           not there.  */
	tmp_pk.name = nkn_strdup_type(pk->name, mod_am_strdup8);
	tmp_pk.provider_id = ptype;
	//tmp_pk.sub_provider_id = sptype;
	tmp_pk.sub_provider_id = 0;
	tmp_pk.type = AM_OBJ_URI;
        s_am_count_promotes(ptype, ptype);
	AM_inc_num_hits(&tmp_pk, 0, 0, 0, NULL, NULL);
	if(tmp_pk.name) {
	    free(tmp_pk.name);
	}
	return 0;
    }
    else {
	if(objp.pk.provider_id == ptype) {
	    if(objp.pk.name) {
		free(objp.pk.name);
	    }
	    return 0;
	}

	glob_change_provider_calls ++;
	objp.promoted[objp.pk.provider_id] = 1;

	tmp_pk.name = nkn_strdup_type(objp.pk.name, mod_am_strdup1);
	tmp_pk.provider_id = ptype;
	tmp_pk.sub_provider_id = sptype;
	tmp_pk.type = AM_OBJ_URI;

        s_am_count_promotes(pk->provider_id, ptype);

	/* If the old_ptype is not local cache provider
	 * set the change_obj flag send the hits and hotness
	 * to the AM_inc_num_hits
	 */
        if(old_ptype > NKN_MM_max_pci_providers) {
	    /* In case of TFM provider, which is currenly used 
	     * only for chunked objects. The object would not
	     * have had an entry in the AM table, because the 
	     * chunked objects are non-cachable in the first
	     * get. So increment the num hits by 1 and the hotness
	     * by 3.
	     */
	    if(old_ptype == TFMMgr_provider) {
		objp.stats.total_num_hits ++;
                am_encode_hotness(objp.hotness + 3, &objp.hotness);
	    }
	    /* If old_ptype is Origin Mgr, this is currently done
	     * in the AM_inc_num_hits itself, so send just the
	     * change_obj flag
	     * TODO: Cleanup as part of 3562 in 1.1
	     */
	    if(old_ptype == OriginMgr_provider || old_ptype == RTSPMgr_provider)
		AM_inc_num_hits(&tmp_pk, 0, 0, 0, NULL, NULL);
	    else
		AM_inc_num_hits(&tmp_pk, objp.stats.total_num_hits,
			objp.hotness, 0, NULL, NULL);
        } else {
	    /* Deletion of the old entry will happen in the AM_inc_num_hits
	     * itself. 
	     */
	    AM_inc_num_hits(&tmp_pk, 0, objp.hotness, 0, NULL, NULL);
        }

	if(objp.pk.name) {
	    free(objp.pk.name);
	}
	if(tmp_pk.name) {
	    free(tmp_pk.name);
	}
    }
    return 0;
}

void AM_update_queue_depth(AM_pk_t *pk, uint32_t value)
{
    AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
    AM_tbl_update_queue_depth(pk, value);
}

void AM_update_media_read_latency(AM_pk_t *pk, uint32_t value)
{
    AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
    AM_tbl_update_cache_latency(pk, value);
}

void AM_init_provider_thresholds(AM_pk_t *pk, uint32_t max_io_throughput,
				 int32_t hotness_threhold)
{
    AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
    AM_tbl_update_provider_thresholds(pk, max_io_throughput, hotness_threhold);
}

/* This function updates the percentage use of a cache */
void AM_update_cache_use_percent(AM_pk_t *pk, uint32_t value)
{
    AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
    if(value > 100)
	return;
    AM_tbl_update_cache_use_percent(pk, value);
    return;
}

/* tbd: vikvik:This function returns the provider which should place this
 * video. This function should take disk space and hotness into
 * consideration.
 */
nkn_provider_type_t
AM_get_obj_provider(AM_pk_t *pk __attribute__((unused)))
{
    return NKN_MM_max_pci_providers;
}

void am_encode_hotness(int32_t in_temp, nkn_hot_t *out_hotness)
{
    *out_hotness = ((uint32_t)in_temp & (0xffffffff))
	| (*out_hotness & 0xffffffff00000000);
}

void am_encode_update_time(nkn_hot_t *out_hotness)
{
    *out_hotness = ((nkn_cur_ts & 0xffffffff) << 32) |
	(*out_hotness & 0xffffffff);
    assert(nkn_cur_ts != 0);
    assert(am_decode_update_time(out_hotness) != 0);
}

int32_t am_decode_hotness(nkn_hot_t *in_hotness)
{
    int hotness;

    /* Absolute value is the first 31 bits */
    hotness = (int)(*in_hotness & 0xffffffff);
    return (hotness);
}

uint32_t am_decode_update_time(nkn_hot_t *in_hotness)
{
    return ((*in_hotness >> 32) & 0xffffffff);
}


void AM_init_hotness(nkn_hot_t *out_hotness)
{
    am_encode_hotness(0 /* temperature */, out_hotness);
    am_encode_update_time(out_hotness);
}

void
AM_init()
{

    AM_tbl_init();

#if 0
    am_aging_thread = g_thread_create_full(
				      (GThreadFunc)s_aging_thread_hdl,
				      NULL, (128*1024), TRUE,
				      FALSE, G_THREAD_PRIORITY_NORMAL,
				      &err1);
    if(am_aging_thread == NULL) {
	g_error_free ( err1 ) ;
    }
#endif

    AM_list_init();
    AM_hotness_lru_list_init();
    AM_promote_init();
}

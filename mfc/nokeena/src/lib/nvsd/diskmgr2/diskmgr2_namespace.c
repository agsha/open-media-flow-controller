#include "nkn_defs.h"
#include "nkn_hash.h"
#include "nkn_namespace_stats.h"
#include "nvsd_resource_mgr.h"
#include "nkn_assert.h"

#include "nkn_diskmgr2_local.h"
#include "diskmgr2_util.h"
#include "diskmgr2_common.h"

extern uint64_t NOZCC_DECL(glob_dm2_ns_preread_not_pinned_cnt),
    glob_dm2_ns_preread_pinned_expired_cnt,
    glob_dm2_ns_preread_pinned_cnt,
    glob_dm2_ns_preread_not_lowest_tier_cnt,
    glob_dm2_ns_preread_lowest_tier_cnt,
    glob_dm2_ns_preread_not_cache_pin_enabled_cnt,
    glob_dm2_ns_preread_cache_pin_enabled_cnt,
    glob_dm2_ns_put_namespace_enospc_err,
    glob_dm2_ns_put_cache_pin_enospc_err,
    glob_dm2_ns_put_cache_pin_err,
    glob_dm2_ns_put_namespace_free_err,
    glob_dm2_ns_delete_namespace_free_err;

uint64_t glob_dm2_ns_stat_get_uri_lookup_err;

/*
 */
int
dm2_ns_calc_space_usage(const ns_stat_token_t	  ns_stoken,
			const ns_stat_category_t  ns_stype,
			const ns_stat_category_t  ns_tier_stype,
			const nkn_provider_type_t ptype __attribute((unused)),
			size_t			  raw_length)
{
    int64_t new_space;
    int ret;

    ret = ns_stat_add(ns_stoken, ns_stype, raw_length, &new_space);
    if (ret != 0) {
	DBG_DM2S("Failed to add value to used namespace stat: %d "
		 "(token=%d/%d) (type=%d)", ret,
		 ns_stoken.u.stat_token_data.val,
		 ns_stoken.u.stat_token_data.gen, ns_stype);
	assert(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }
    ret = ns_stat_add(ns_stoken, ns_tier_stype, raw_length, &new_space);
    if (ret != 0) {
	DBG_DM2S("Failed to add value to used namespace tier stat: %d "
		 "(token=%d/%d) (type=%d)", ret,
		 ns_stoken.u.stat_token_data.val,
		 ns_stoken.u.stat_token_data.gen,
		 ns_tier_stype);
	ret = ns_stat_add(ns_stoken, ns_stype, -raw_length, &new_space);
	assert(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    return 0;
}	/* dm2_ns_calc_space_usage */

/*
 * Count the URI->content_length as pinned data if we meet a bunch
 * of requirements.
 */
int
dm2_ns_calc_cache_pin_usage(const ns_stat_token_t	ns_stoken,
			    const ns_stat_category_t	cp_type,
			    const nkn_provider_type_t	ptype,
			    const nkn_attr_t		*attr)
{
    int64_t new_space;
    int ret;

    /* This URI has no cache pinning bit set */
    if (nkn_attr_is_cache_pin(attr) == 0) {
	glob_dm2_ns_preread_not_pinned_cnt++;
	return 0;
    } else if (attr->cache_expiry < nkn_cur_ts) {
    /* We need to account the pinned expired objects also as we dont
     * have a way to remember the non-accounting (while deleting)
     * If we dont account, we will end up going negative on deletion */
	glob_dm2_ns_preread_pinned_expired_cnt++;
    } else {
	glob_dm2_ns_preread_pinned_cnt++;
    }

    /* This URI doesn't exist in the lowest tier */
    if (ptype != glob_dm2_lowest_tier_ptype) {
	glob_dm2_ns_preread_not_lowest_tier_cnt++;
	return 0;
    } else {
	glob_dm2_ns_preread_lowest_tier_cnt++;
    }

    ret = ns_stat_add(ns_stoken, cp_type,
		      ROUNDUP_PWR2(attr->content_length,
				   dm2_tier_pagesize(ptype)),
		      &new_space);
    if (ret != 0) {
	DBG_DM2S("Failed to add value to used disk cache pin stat: %d "
		 "(token=%d/%d) (type=%d)",ret, ns_stoken.u.stat_token_data.val,
		 ns_stoken.u.stat_token_data.gen, cp_type);
	assert(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    ret = ns_stat_add(ns_stoken, NS_CACHE_PIN_SPACE_USED_TOTAL,
		      ROUNDUP_PWR2(attr->content_length,
				   dm2_tier_pagesize(ptype)),
		      &new_space);
    if (ret != 0) {
	DBG_DM2S("Failed to add value to used total cache pin stat: %d "
		 "(token=%d/%d) (type=%d)",ret, ns_stoken.u.stat_token_data.val,
		 ns_stoken.u.stat_token_data.gen, cp_type);
	assert(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    ret = ns_stat_add(ns_stoken, NS_CACHE_PIN_OBJECT_COUNT, 1, NULL);
    if (ret != 0) {
	DBG_DM2S("Failed to add value to total cache pin object count: %d "
		 "(token=%d/%d) (type=%d)",ret, ns_stoken.u.stat_token_data.val,
		 ns_stoken.u.stat_token_data.gen, cp_type);
	assert(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }


    return 0;
}	/* dm2_ns_calc_cache_pin_usage */



/*
 * Find out how much space is used in each namespace per tier and give that
 * total to the resource pool code.
 */
void
dm2_ns_set_namespace_res_pools(const nkn_provider_type_t ptype)
{
    ns_stat_category_t cat;
    int64_t total = 0;
    int ret;

    if (ptype == SolidStateMgr_provider)
	cat = NS_USED_SPACE_SSD;
    else if (ptype == SAS2DiskMgr_provider)
	cat = NS_USED_SPACE_SAS;
    else if (ptype == SATADiskMgr_provider)
	cat = NS_USED_SPACE_SATA;
    else {
	return;
    }

    ret = ns_stat_total_subcategory(cat, &total);
    if (ret != 0) {
	DBG_DM2S("Error getting subcategory/%d total: %d", cat, ret);
    } else {
	DBG_DM2M("Total: %d %ld", cat, total);
    }
}	/* dm2_ns_set_namespace_res_pools */


/*
 * For namespace space, we assume that we'll take up one block for now.
 * Reserve one block, then see how much is actually taken.  If we get an error
 * during the operation, then return all the space to the resource pool.
 * Otherwise, on exit from PUT, we need to update the DM2 disk namespace stats.
 *
 * Cache pin usage is based on the content_length in an attribute.  Block usage
 * is not considered.
 *
 * Assume: put->ns_stoken is valid.
 */
int
dm2_ns_put_calc_space_resv(MM_put_data_t	*put,
			   char			*put_uri,
			   nkn_provider_type_t	ptype)
{
    int64_t pagesize, uri_size, old_total, total_space;
    rp_type_en rp_type;
    int rp_index, ret;

    if ((ret = dm2_ns_rpindex_from_uri_dir(put_uri, &rp_index)) != 0) {
	DBG_DM2S("[URI=%s] Cannot get resource pool index", put_uri);
	return -NKN_STAT_NAMESPACE_EINVAL;
    }

    rp_type = dm2_ptype_to_rptype(ptype);
    pagesize = dm2_tier_pagesize(ptype);
#if 0
    ret = nvsd_rp_alloc_resource(rp_type, rp_index, pagesize);	// boolean ret
    if (ret == 0) {
	glob_dm2_ns_put_namespace_enospc_err++;
	return -NKN_STAT_NAMESPACE_ENOSPC;
    }
#endif
    if (put->attr) {
	/* Cache pinning is only relevant to the lowest tier */
	if (ptype == glob_dm2_lowest_tier_ptype &&
		nkn_attr_is_cache_pin(put->attr)) {
	    /*
	     * We have a PUT for a pinned object going to the lowest tier.
	     * The uri_size must be added to the total only if caching pinning
	     * is enabled.
	     */
	    uri_size = ROUNDUP_PWR2(put->attr->content_length, pagesize);
	    ret = ns_stat_get(put->ns_stoken, NS_CACHE_PIN_SPACE_RESV_TOTAL,
			      &total_space);
	    if (ret != 0) {
		assert(ret == NKN_STAT_GEN_MISMATCH);
		return -ret;
	    }

	    ret = ns_stat_add(put->ns_stoken, NS_CACHE_PIN_SPACE_USED_TOTAL,
			      0, &old_total);
	    if (ret != 0) {
		assert(ret == NKN_STAT_GEN_MISMATCH);
		return -ret;
	    }
	    /* total_space == 0 means there is no limit */
	    if (total_space != 0 && (old_total+uri_size) > total_space) {
		put->cache_pin_enabled = 0;
		glob_dm2_ns_put_cache_pin_enospc_err++;
		nkn_attr_reset_cache_pin(put->attr);
		return 0;
	    }

	    ret = ns_stat_add(put->ns_stoken,
			      NS_CACHE_PIN_OBJECT_COUNT, 1, NULL);
	    if (ret != 0) {
		assert(ret == NKN_STAT_GEN_MISMATCH);
		return -ret;
	    }
	}
    }

    put->namespace_resv_usage = uri_size;
    put->namespace_actual_usage = 0;
    put->cache_pin_resv_usage = uri_size;
    put->cache_pin_actual_usage = 0;
    return 0;
}	/* dm2_ns_put_calc_space_resv */


int
dm2_ns_update_total_objects(const ns_stat_token_t ns_stat_token,
			    const char		  *mgmt_name,
			    dm2_cache_type_t	  *ct,
			    int			  add_object_flag)
{
    ns_stat_category_t to_cat, ti_ptype_cat, td_ptype_cat;
    nkn_provider_type_t ptype = ct->ct_ptype;
    int ret;

    /* Tier specific ingest/delete counts */
    if (add_object_flag)
	 AO_fetch_and_add1(&ct->ct_dm2_ingest_objects);
    else
	 AO_fetch_and_add1(&ct->ct_dm2_delete_objects);

    /* Get total objects for the namespace for the particular disk */
    ret = dm2_stat_get_diskstat_cats(mgmt_name, NULL, NULL, &to_cat);
    if (ret != 0)
	return ret;

    /* Add/subtract one to/from the total for the particular disk */
    ret = ns_stat_add(ns_stat_token, to_cat, add_object_flag ? 1 : -1, NULL);
    if (ret != 0) {
	DBG_DM2S("Failed to add to total objects namespace stat: %d "
		 "(token=%d/%d) (type=%d)", ret,
		 ns_stat_token.u.stat_token_data.val,
		 ns_stat_token.u.stat_token_data.gen, to_cat);
	return -ret;
    }

    /* Get the total ingested, total deleted objects for the NS for the tier */
    ret = dm2_stat_get_tierstat_cats(ptype, &ti_ptype_cat, &td_ptype_cat);
    if (ret != 0)
	return ret;

    if (add_object_flag) {
	/* Update the total ingested objects ns/tier */
	ret = ns_stat_add(ns_stat_token, ti_ptype_cat, 1, NULL);
	if (ret != 0) {
	    DBG_DM2S("Failed to add to total objects namespace stat: %d "
		     "(token=%d/%d) (type=%d)", ret,
		     ns_stat_token.u.stat_token_data.val,
		     ns_stat_token.u.stat_token_data.gen, ti_ptype_cat);
	    return -ret;
	}
    } else {
	/* Update the total deleted objects ns/tier */
	ret = ns_stat_add(ns_stat_token, td_ptype_cat, 1, NULL);
	if (ret != 0) {
	    DBG_DM2S("Failed to add to total objects namespace stat: %d "
		     "(token=%d/%d) (type=%d)", ret,
		     ns_stat_token.u.stat_token_data.val,
		     ns_stat_token.u.stat_token_data.gen, td_ptype_cat);
	    return -ret;
	}
    }

    return 0;
}	/* dm2_ns_update_total_objects */


/*
 * Make up for the fact that the current PUT may not have used all the space
 * which we reserved before we knew what existing containers had in them.
 * In particular, if we are writing a bunch of small objects into the same
 * container, the first PUT will consume a full block but the next 63 objects
 * should consume no more namespace space.
 *
 * Assume put->ns_stoken is valid.
 */
int
dm2_ns_put_calc_space_actual_usage(MM_put_data_t *put,
				   char *put_uri __attribute((unused)),
				   DM2_uri_t *uri_head,
				   nkn_provider_type_t ptype)
{
    char *mgmt_name;
    int64_t new_space, total_space;
    ns_stat_category_t cp_cat, us_cat, ns_tier_cat;
    int ret;
#if 0
    rp_type_en rp_type;
    int rp_index;

    /* Give back space */
    if (put->namespace_actual_usage < put->namespace_resv_usage) {
	ret = dm2_ns_rpindex_from_uri_dir(put_uri, &rp_index);
	if (ret != 0) {
	    DBG_DM2S("[URI=%s] Cannot get resource pool index", put_uri);
	    return -NKN_STAT_NAMESPACE_EINVAL;
	}

	rp_type = dm2_ptype_to_rptype(ptype);
	/* Boolean return value */
	ret = nvsd_rp_free_resource(rp_type, rp_index,
		put->namespace_resv_usage - put->namespace_actual_usage);
	if (ret == 0) {
	    glob_dm2_ns_put_namespace_free_err++;
	    return -NKN_STAT_NAMESPACE_ENOSPC;
	}
    }
#endif

    /* Allocate space to namespace now that we know disk */
    ns_tier_cat = dm2_ptype_to_nscat(ptype);
    mgmt_name = uri_head->uri_container->c_dev_ci->mgmt_name;
    ret = dm2_stat_get_diskstat_cats(mgmt_name, &cp_cat, &us_cat, NULL);
    if (ret != 0)
	return ret;

    /* Assign namespace space to a specific disk */
    ret = ns_stat_add(put->ns_stoken, us_cat, put->namespace_actual_usage,
		      &new_space);
    if (ret != 0) {
	DBG_DM2S("Failed to add to used disk namespace stat: %d "
		 "(token=%d/%d) (type=%d)", ret,
		 put->ns_stoken.u.stat_token_data.val,
		 put->ns_stoken.u.stat_token_data.gen, us_cat);
	return -ret;
    }

    /* Assign namespace space to the tier */
    ret = ns_stat_add(put->ns_stoken, ns_tier_cat, put->namespace_actual_usage,
		      &total_space);
    if (ret != 0) {
	DBG_DM2S("Failed to add to used disk namespace tier stat: %d "
		 "(token=%d/%d) (type=%d)", ret,
		 put->ns_stoken.u.stat_token_data.val,
		 put->ns_stoken.u.stat_token_data.gen, ns_tier_cat);
	return -ret;
    }

    if (ptype == glob_dm2_lowest_tier_ptype) {
	/*
	 * If we are in the lowest tier, then we can assign the cache pin
	 * space to a specific disk.  The space would have already been added
	 * to the total space.  Unlike namespace space, we don't perform
	 * block based accounting.
	 */
	 ret = ns_stat_add(put->ns_stoken, NS_CACHE_PIN_SPACE_USED_TOTAL,
                              put->cache_pin_actual_usage, NULL);
	 if (ret != 0) {
	    assert(ret == NKN_STAT_GEN_MISMATCH);
	    return -ret;
	}

	ret = ns_stat_add(put->ns_stoken, cp_cat, put->cache_pin_actual_usage,
			  &new_space);
	if (ret != 0) {
	    DBG_DM2S("Failed to add to used cache pin space stat: %d "
		     "(token=%d/%d) (type=%d)", ret,
		     put->ns_stoken.u.stat_token_data.val,
		     put->ns_stoken.u.stat_token_data.gen, ns_tier_cat);
	    assert(ret == NKN_STAT_GEN_MISMATCH);
	    return -ret;
	}
    }
    return 0;
}	/* dm2_ns_put_calc_space_actual_usage */

int
dm2_ns_resv_disk_usage(ns_stat_token_t     ns_stat_token,
		       nkn_provider_type_t ptype,
		       int		   blocks)
{
    int64_t old_resv_size = 0;
    ns_stat_category_t cat;
    int64_t max_size, space_used;
    int block_size, ret;

    cat = dm2_ptype_to_ns_maxuse_cat(ptype);

    block_size = dm2_tier_blocksize(ptype);
    space_used = (size_t)blocks * block_size;

    ret = ns_stat_get(ns_stat_token, cat, &max_size);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    ret = ns_stat_add(ns_stat_token, cat+1, space_used, &old_resv_size);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    if (max_size != 0 && (old_resv_size + space_used) > max_size) {
	ns_stat_add(ns_stat_token, cat+1, -space_used, &old_resv_size);
	return -ENOSPC;
    }

    return 0;

}   /* dm2_ns_add_usage */

int
dm2_ns_calc_disk_used(ns_stat_token_t	  ns_stat_token,
		      nkn_provider_type_t ptype,
		      NCHashTable	  *ns_physid_hash,
		      uint64_t		  physid)
{
    uint64_t hashkey, *ns_lookup;
    ns_stat_category_t cat;
    int block_size, ret;
    ns_physid_hash_entry_t  *ph_entry = NULL;

    ph_entry = dm2_calloc(1, sizeof(ns_physid_hash_entry_t),
			  mod_ns_physid_hash);
    if (ph_entry == NULL)
	return -ENOMEM;

    ph_entry->physid = physid;
    hashkey = dm2_physid_hash((void *)physid, 0);
    ns_lookup = nchash_lookup(ns_physid_hash, hashkey,
			      (void *)ph_entry->physid);
    if (ns_lookup) {
	/* already present */
	dm2_free(ph_entry, sizeof(ns_physid_hash_entry_t), DM2_NO_ALIGN);
	return 0;
    }

    block_size = dm2_tier_blocksize(ptype);
    cat = dm2_ptype_to_ns_maxuse_cat(ptype);
    if (ns_lookup == NULL) {

	ret = ns_stat_add(ns_stat_token, cat+1, block_size, NULL);
	if (ret != 0) {
	    NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	    return ret;
	}

	ret = ns_stat_add(ns_stat_token, cat+2, block_size, NULL);
	if (ret != 0) {
	    NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	    return ret;
	}

	nchash_insert(ns_physid_hash, hashkey,
		      (void *)ph_entry->physid,
		      ph_entry);
    }

    return 0;
}   /* dm2_ns_calc_disk_used */

int
dm2_ns_add_disk_usage(ns_stat_token_t	  ns_stat_token,
		      nkn_provider_type_t ptype,
		      int		  blocks)
{
    ns_stat_category_t cat;
    int block_size, ret;
    size_t  space_used;

    cat = dm2_ptype_to_ns_maxuse_cat(ptype);

    block_size = dm2_tier_blocksize(ptype);
    space_used = (size_t)blocks * block_size;

    ret = ns_stat_add(ns_stat_token, cat+2, space_used, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    return 0;
}   /* dm2_ns_add_disk_usage */

int
dm2_ns_sub_disk_usage(ns_stat_token_t     ns_stat_token,
		      nkn_provider_type_t ptype,
		      int		  blocks,
		      int		  resv_only)
{
    ns_stat_category_t cat;
    size_t  space_used;
    int block_size, ret;

    cat = dm2_ptype_to_ns_maxuse_cat(ptype);

    block_size = dm2_tier_blocksize(ptype);
    space_used = (size_t)blocks * block_size;

    ret = ns_stat_add(ns_stat_token, cat+1, -space_used, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    if (resv_only)
	return 0;

    ret = ns_stat_add(ns_stat_token, cat+2, -space_used, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    return 0;
}   /* dm2_ns_sub_usage */

int
dm2_ns_delete_calc_namespace_usage(const char		*uri_dir,
				   const char		*mgmt_name,
				   ns_stat_token_t	ns_stat_token,
				   nkn_provider_type_t	ptype,
				   int			num_freed_disk_pgs,
				   int			uri_pinned)
{
    ns_stat_category_t us_cat, cp_cat, ns_tier_cat;
    rp_type_en rp_type;
    uint64_t free_namespace_bytes = 0, free_cache_pin_bytes = 0;
    int rp_index, ret, pin_count=0;
    uint32_t	page_size;

    /* Get handles to resource pool */
    ret = dm2_ns_rpindex_from_uri_dir(uri_dir, &rp_index);
    if (ret != 0) {
	DBG_DM2S("[URI=%s] Cannot get resource pool index", uri_dir);
	return -NKN_STAT_NAMESPACE_EINVAL;
    }
    rp_type = dm2_ptype_to_rptype(ptype);
    page_size = dm2_tier_pagesize(ptype);

    free_namespace_bytes = num_freed_disk_pgs * page_size;

    /* If the object is pinned, namespace accounting is the pinned accounting*/
    if (uri_pinned) {
	free_cache_pin_bytes = free_namespace_bytes;
	pin_count++;
    }
#if 0
    /* Boolean return value: return space to resource pool */
    ret = nvsd_rp_free_resource(rp_type, rp_index, free_namespace_bytes);
    if (ret == 0) {
	glob_dm2_ns_delete_namespace_free_err++;
	return -NKN_STAT_NAMESPACE_FREE_FAILED;
    }
#endif
    ret = dm2_stat_get_diskstat_cats(mgmt_name, &cp_cat, &us_cat, NULL);
    if (ret != 0) {
	NKN_ASSERT(0);
	return ret;
    }

    /* Reduce the namespace accounting in disk by bytes in blocks */
    ret = ns_stat_add(ns_stat_token, us_cat, -free_namespace_bytes, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    /* Reduce the namespace accounting for tier by bytes in blocks */
    ns_tier_cat = dm2_ptype_to_nscat(ptype);
    ret = ns_stat_add(ns_stat_token, ns_tier_cat, -free_namespace_bytes, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    /* Reduce the cache pin account for a specific disk */
    ret = ns_stat_add(ns_stat_token, cp_cat, -free_cache_pin_bytes, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    /* Reduce the cache pin account for the entire tier */
    ret = ns_stat_add(ns_stat_token, NS_CACHE_PIN_SPACE_USED_TOTAL,
		      -free_cache_pin_bytes, NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    /* Reduce the cache pin object count for the namespace */
    ret = ns_stat_add(ns_stat_token, NS_CACHE_PIN_OBJECT_COUNT, -pin_count,
		      NULL);
    if (ret != 0) {
	NKN_ASSERT(ret == NKN_STAT_GEN_MISMATCH);
	return ret;
    }

    return 0;
}	/* dm2_ns_delete_calc_namespace_usage */


/*
 * Given a cache tier, find the total size of all fully running disk caches
 */
void
dm2_ns_set_tier_size_totals(dm2_cache_type_t *ct)
{
    dm2_cache_info_t	*ci;
    GList		*ci_obj;
    rp_type_en		rp_type;
    uint64_t		total = 0;
    int			ret;

    for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	ci = (dm2_cache_info_t *)ci_obj->data;
	if (ci->state_overall == DM2_MGMT_STATE_CACHE_RUNNING) {
	    total += ci->set_cache_sec_cnt * DEV_BSIZE;
	}
    }
    if (ct->ct_ptype == SolidStateMgr_provider)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SSD;
    else if (ct->ct_ptype == SAS2DiskMgr_provider)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SAS;
    else if (ct->ct_ptype == SATADiskMgr_provider)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SATA;
    else {
	/* VMs may have Unknown_provider */
	return;
    }

    /*
     * We have defined it an error to call set_total with zero value.  This
     * value can occur when no drives are enabled for this tier.
     */
    if (total == 0)
	return;

    /* Boolean return */
    DBG_DM2M("set_tier_size: prov=%d total=%ld", ct->ct_ptype, total);
    if ((ret = nvsd_rp_set_total(rp_type, total)) == 0) {
	DBG_DM2S("Failed to set total size (%ld) for resource pool (%d): %d",
		 total, rp_type, ret);
	assert(0);
    }
}	/* dm2_ns_set_tier_size_totals */


ns_stat_token_t
dm2_ns_make_stat_token_from_uri_dir(const char *uri_dir)
{
    char	    ns_uuid[NKN_MAX_UID_LENGTH];
    ns_stat_token_t ns_stat_token;
    ns_tier_entry_t ns_ssd, ns_sas, ns_sata;
    int		    ret;

    ns_stat_token = dm2_ns_get_stat_token_from_uri_dir(uri_dir);
    if (ns_is_stat_token_null(ns_stat_token)) {
	/* create the deleted namespace */
	DBG_DM2W("Creating new namespace since missing: %s", uri_dir);

	if ((ret = dm2_uridir_to_nsuuid(uri_dir, ns_uuid)) != 0)
	    return NS_NULL_STAT_TOKEN;

	/* Set the defaults for the tier specifc configs */
	ns_ssd.read_size = 256*1024;
	ns_ssd.block_free_threshold = 50;
	ns_ssd.group_read = 0;
	ns_ssd.max_disk_usage = 0;

	ns_sas.read_size = 2048*1024;
	ns_sas.block_free_threshold = 50;
	ns_sas.group_read = 1;
	ns_sas.max_disk_usage = 0;

	ns_sata.read_size = 2048*1024;
	ns_sata.block_free_threshold = 50;
	ns_sata.group_read = 1;
	ns_sata.max_disk_usage = 0;

	ret = ns_stat_add_namespace(-1		     /* nth or rp_index */,
				    NS_STATE_DELETED_BUT_FILLED /* state */,
				    ns_uuid	     /* ns_uuid */,
				    0		  /* cache pin max obj bytes */,
				    0		     /* cache pin resv bytes */,
				    0		     /* cache pin enabled */,
						    /* Tier specific defaults */
				    &ns_ssd, &ns_sas, &ns_sata,
				    1		     /* lock */);
	if (ret != 0)
	    return NS_NULL_STAT_TOKEN;

	ns_stat_token = dm2_ns_get_stat_token_from_uri_dir(uri_dir);
	if (ns_is_stat_token_null(ns_stat_token)) {
	    DBG_DM2S("Can not get Namespace token: %s", uri_dir);
	    assert(0);	// should never happen
	    return NS_NULL_STAT_TOKEN;
	}
    }
    return ns_stat_token;
}	/* dm2_ns_make_stat_token_from_uri_dir */


int
dm2_ns_rpindex_from_uri_dir(const char *uri_dir,
			    int *rp_index)
{
    char ns_uuid[NKN_MAX_UID_LENGTH];
    int ret;

    ret = dm2_uridir_to_nsuuid(uri_dir, ns_uuid);
    if (ret != 0)
	return -ret;

    ret = ns_stat_get_rpindex_from_nsuuid(ns_uuid, rp_index);
    return ret;
}	/* dm2_ns_rpindex_from_uri_dir */


/*
 * Returns a refcnt'd stat token
 */
ns_stat_token_t
dm2_ns_get_stat_token_from_uri_dir(const char *uri_dir)
{
    char ns_uuid[NKN_MAX_UID_LENGTH];
    ns_stat_token_t ns_stat_token;
    int ret;

    ret = dm2_uridir_to_nsuuid(uri_dir, ns_uuid);
    if (ret != 0) {
	glob_dm2_ns_stat_get_uri_lookup_err++;
	return NS_NULL_STAT_TOKEN;
    }

    ns_stat_token = ns_stat_get_stat_token(ns_uuid);
    return ns_stat_token;
}	/* dm2_ns_get_stat_token_from_uri_dir */


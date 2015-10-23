/*
 *	COPYRIGHT: NOKEENA NETWORKS
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */

#include "nkn_assert.h"
#include "diskmgr2_util.h"
#include "diskmgr2_common.h"

char *
dm2_get_time_single_token(time_t t)
{
    char *ptime, *space;

    ptime = ctime(&t);
    ptime[strlen(ptime)-1] = '\0';
    ptime += 4; // skip day of week
    while ((space = strchr(ptime, ' ')) != NULL)
        *space = '_';

    return ptime;
}

void
dm2_cache_log_add(MM_put_data_t    *put,
		  char		   *put_uri,
		  DM2_uri_t	   *uri_head,
		  dm2_cache_type_t *ct)
{
    char timestr[30];
    time_t uri_expiry;
    uint64_t  written_len, uri_content_len;
    char *cp;
    int	tval = 0;
#define ONE_MB (1024*1024)

    /* Take the expiry from the put->attr, if available.
     * DM2 stores only 32 bits in memory and so the expiry will not be correct
     * when the expiry value breaches 32 bits. If there are multiple
     * entries in the cache.log for the same file, only the first entry
     * will be correct (for expiry values that breach 32 bits)
     */
    if (put && put->attr)
	uri_expiry = put->attr->cache_expiry;
    else
	uri_expiry = uri_head->uri_expiry;
    cp = ctime(&uri_expiry);

    /* For chunked objects, uri_content_len will not reflect the actual
     * object length and 'uri_resv_len' will always be greater
     */
    if (uri_head->uri_content_len > uri_head->uri_resv_len)
	written_len = uri_head->uri_content_len - uri_head->uri_resv_len;
    else
	written_len = uri_head->uri_content_len;

    if (uri_head->uri_content_len > (2*ONE_MB)) {
	/* If not fully written, log power of 2 ingests only */
	if (put && uri_head->uri_resv_len) {
	    /* Dont log odd ingests */
	    if (written_len % (2*ONE_MB))
		return;
	    /* Check for power of two */
	    tval = written_len / ONE_MB; //get MB value
	    if (tval & (tval - 1))
		return;
	}
    }

    if (cp == NULL) {
	DBG_DM2S("[cache_type=%s][URI=%s] Illegal expiry: %d",
		 ct->ct_name, put_uri, uri_head->uri_expiry);
	NKN_ASSERT(cp != NULL);
	cp = (char *)"unknown_time";
    }
    strcpy(timestr, cp);
    if ((cp = strchr(timestr, '\n')) != 0)
	*cp = 0;
    /* WARNING: uri_name can have unprintable characters!!! */
    uri_content_len = uri_head->uri_content_len;
    if (uri_head->uri_cache_pinned) {
    DBG_CACHE(" ADD \"%s\" %s %s %ld %ld [%s] PINNED", put_uri, ct->ct_name,
	      uri_head->uri_container->c_dev_ci->mgmt_name,
	      written_len, uri_content_len, timestr);
    } else {
	DBG_CACHE(" ADD \"%s\" %s %s %ld %ld [%s]", put_uri, ct->ct_name,
		  uri_head->uri_container->c_dev_ci->mgmt_name,
		  written_len, uri_content_len, timestr);
    }
}	/* dm2_cache_log_add */


void
dm2_cache_log_delete(const char *uri_name,
		     const char *uri_dir,
		     const char *mgmt_name,
		     int uri_orig_hotness,
		     nkn_hot_t  uri_hotval,
		     uint32_t cache_age,
		     uint32_t del_reason,
		     const dm2_cache_type_t *ct)
{
    char *ptime;
    int32_t update_time;

    update_time = am_decode_update_time(&uri_hotval);
    ptime = dm2_get_time_single_token(update_time);

    DBG_CACHE(" DELETE \"%s%s\" %s %s %d/%s %d %d 0x%x %d", uri_dir, uri_name,
	      ct->ct_name, mgmt_name, update_time, ptime, uri_orig_hotness,
	      am_decode_hotness(&uri_hotval), del_reason, cache_age);
}	/* dm2_cache_log_delete */

void
dm2_cache_log_attr_hotness_update(DM2_uri_t *uri_head,
				  const char* uri,
				  dm2_cache_type_t *ct,
				  nkn_hot_t  hotval)
{
    /* Print the hotness value alone excluding teh update time */
    char timestr[30];
    time_t expiry = uri_head->uri_expiry;
    char *cp = ctime(&expiry);

    if (cp == NULL) {
	DBG_DM2S("[cache_type=%s][URI=%s] Illegal expiry: %d",
		 ct->ct_name, uri_head->uri_name, uri_head->uri_expiry);
	NKN_ASSERT(cp != NULL);
	cp = (char *)"unknown_time";
    }
    strcpy(timestr, cp);
    if ((cp = strchr(timestr, '\n')) != 0)
	*cp = 0;
    /* WARNING: uri_name can have unprintable characters!!! */
    DBG_CACHE(" UPDATE_ATTR \"%s\" %d %s %s [%s]",
	      uri, am_decode_hotness(&hotval), ct->ct_name,
	      uri_head->uri_container->c_dev_ci->mgmt_name, timestr);
}

void
dm2_cache_log_attr_update(DM2_uri_t *uri_head,
			  char	    *uri_name,
			  dm2_cache_type_t *ct)
{
    char timestr[30];
    time_t uri_expiry = uri_head->uri_expiry;
    char *cp = ctime(&uri_expiry);

    if (cp == NULL) {
	DBG_DM2S("[cache_type=%s][URI=%s] Illegal expiry: %d",
		 ct->ct_name, uri_name, uri_head->uri_expiry);
	NKN_ASSERT(cp != NULL);
	cp = (char *)"unknown_time";
    }
    strcpy(timestr, cp);
    if ((cp = strchr(timestr, '\n')) != 0)
	*cp = 0;
    /* WARNING: uri_name can have unprintable characters!!! */
    DBG_CACHE(" UPDATE_ATTR \"%s\" %s %s [%s]",
	      uri_name, ct->ct_name,
	      uri_head->uri_container->c_dev_ci->mgmt_name, timestr);
}	/* dm2_cache_log_attr_update */

void
dm2_cache_log_decay(DM2_uri_t *uri_head,
		    dm2_cache_type_t *ct,
		    nkn_hot_t	old_hotval,
		    nkn_hot_t	new_hotval)
{
    DBG_CACHE(" DECAY	\"%s/%s\" %s %s %d %d",
		uri_head->uri_container->c_uri_dir,
		dm2_uh_uri_basename(uri_head), ct->ct_name,
		uri_head->uri_container->c_dev_ci->mgmt_name,
		am_decode_hotness(&old_hotval),
		am_decode_hotness(&new_hotval));
}	/* dm2_cache_log_decay */

void
dm2_cache_log_disk_format(const char *mgmt_name,
			  dm2_cache_type_t *ct)
{
    DBG_CACHE(" FORMAT %s %s ", ct->ct_name, mgmt_name);
}	/* dm2_cache_log_format */

void
dm2_dec_write_cnt_on_cache_dev(dm2_cache_info_t *freespace_ci)
{
    NKN_ASSERT(AO_load(&freespace_ci->ci_update_cnt));
    AO_fetch_and_sub1(&freespace_ci->ci_update_cnt);
#if 0
    DBG_DM2E("dec update cnt: %s %d",
	     freespace_ci->mgmt_name, freespace_ci->ci_update_cnt);
#endif
}	/* dm2_dec_write_cnt_on_cache_dev */


void
dm2_inc_write_cnt_on_cache_dev(dm2_cache_info_t *freespace_ci)
{
    AO_fetch_and_add1(&freespace_ci->ci_update_cnt);
#if 0
    DBG_DM2E("inc update cnt: %s %d",
	     freespace_ci->mgmt_name, freespace_ci->ci_update_cnt);
#endif
}	/* dm2_inc_write_cnt_on_cache_dev */


int
dm2_tier_blocksize(nkn_provider_type_t ptype)
{
    switch (ptype) {
	case SolidStateMgr_provider:
	    return DM2_DISK_BLKSIZE_256K;
	case SAS2DiskMgr_provider:
	case SATADiskMgr_provider:
	    return DM2_DISK_BLKSIZE_2M;
	default:
	    assert(0);
	    return 0;
    }
}	/* dm2_tier_blocksize */


int
dm2_tier_pagesize(nkn_provider_type_t ptype)
{
    switch (ptype) {
	case SolidStateMgr_provider:
	    return DM2_DISK_PAGESIZE_4K;
	case SAS2DiskMgr_provider:
	case SATADiskMgr_provider:
	    return DM2_DISK_PAGESIZE_32K;
	default:
	    assert(0);
	    return 0;
    }
}	/* dm2_tier_pagesize */


/*
 * For the purpose of this function, we have only 3 valid enums:
 *  SSD: 1
 *  SAS: 5
 *  SATA: 6
 */
void
dm2_set_lowest_tier(nkn_provider_type_t ptype)
{
    if (glob_dm2_lowest_tier_ptype == 0) {	// 0 is initial value
	glob_dm2_lowest_tier_ptype = ptype;
	return;
    }
    if (glob_dm2_lowest_tier_ptype < ptype)
	glob_dm2_lowest_tier_ptype = ptype;
    return;
}	/* dm2_set_lowest_tier */

void
dm2_update_lowest_tier(void)
{
    dm2_cache_type_t *ct;
    nkn_provider_type_t cur_lowest_ptype = 0;
    int i;

    for (i = 0; i < glob_dm2_num_cache_types; i++) {
        ct = &g_cache2_types[i];
        switch(ct->ct_ptype) {
	   case SolidStateMgr_provider:
		if (ct->ct_info_list &&
		    (cur_lowest_ptype < SolidStateMgr_provider)) {
		    cur_lowest_ptype = SolidStateMgr_provider;
		}
		break;
	   case SAS2DiskMgr_provider:
		if (ct->ct_info_list &&
		    (cur_lowest_ptype < SAS2DiskMgr_provider)) {
		    cur_lowest_ptype = SAS2DiskMgr_provider;
		}
		break;
	   case SATADiskMgr_provider:
		if (ct->ct_info_list &&
		    (cur_lowest_ptype < SATADiskMgr_provider)) {
		    cur_lowest_ptype = SATADiskMgr_provider;
		}
		break;
	   default:
	    break;
	}
    }
    if (glob_dm2_lowest_tier_ptype != cur_lowest_ptype) {
	DBG_DM2S("Lowest cache tier changed from %d to %d",
		    glob_dm2_lowest_tier_ptype, cur_lowest_ptype);
	glob_dm2_lowest_tier_ptype = cur_lowest_ptype;
    }
    return;
}	/* dm2_update_lowest_tier */

nkn_provider_type_t
dm2_nscat_to_ptype(ns_stat_category_t category)
{
    nkn_provider_type_t ptype;

    if (category == NS_USED_SPACE_SSD)
	ptype = SolidStateMgr_provider;
    else if (category == NS_USED_SPACE_SAS)
	ptype = SAS2DiskMgr_provider;
    else if (category == NS_USED_SPACE_SATA)
	ptype = SATADiskMgr_provider;
    else
	assert(0);

    return ptype;
}	/* dm2_nscat_to_ptype */


ns_stat_category_t
dm2_ptype_to_nscat(nkn_provider_type_t ptype)
{
    ns_stat_category_t cat;

    if (ptype == SolidStateMgr_provider)
	cat = NS_USED_SPACE_SSD;
    else if (ptype == SAS2DiskMgr_provider)
	cat = NS_USED_SPACE_SAS;
    else if (ptype == SATADiskMgr_provider)
	cat = NS_USED_SPACE_SATA;
    else
	assert(0);

    return cat;
}	/* dm2_ptype_to_nscat */


rp_type_en
dm2_nscat_to_rptype(ns_stat_category_t cat)
{
    rp_type_en rp_type;

    if (cat == NS_USED_SPACE_SSD)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SSD;
    else if (cat == NS_USED_SPACE_SAS)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SAS;
    else if (cat == NS_USED_SPACE_SATA)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SATA;
    else {
	assert(0);
    }
    return rp_type;
}	/* dm2_nscat_to_rptype */


rp_type_en
dm2_ptype_to_rptype(nkn_provider_type_t ptype)
{
    rp_type_en rp_type;

    if (ptype == SolidStateMgr_provider)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SSD;
    else if (ptype == SAS2DiskMgr_provider)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SAS;
    else if (ptype == SATADiskMgr_provider)
	rp_type = RESOURCE_POOL_TYPE_UTIL_SATA;
    else {
	assert(0);
    }
    return rp_type;
}	/* dm2_ptype_to_rptype */

ns_stat_category_t
dm2_ptype_to_ns_maxuse_cat(nkn_provider_type_t ptype)
{
    ns_stat_category_t cat;

    if (ptype == SolidStateMgr_provider)
        cat = NS_SSD_MAX_USAGE;
    else if (ptype == SAS2DiskMgr_provider)
        cat = NS_SAS_MAX_USAGE;
    else if (ptype == SATADiskMgr_provider)
        cat = NS_SATA_MAX_USAGE;
    else
        assert(0);

    return cat;
}       /* dm2_ptype_to_ns_maxuse_cat */


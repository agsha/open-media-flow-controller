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
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_container.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_debug.h"
#include "diskmgr2_locking.h"


void dm2_print_mem_extent(void *value);
void dm2_print_ext_list(GList *ext_list);

static void
s_print_list(gpointer data,
	     gpointer user_data __attribute((unused)))
{
    dm2_print_mem_extent(data);
}

void
dm2_print_ext_list(GList *ext_list)
{
    g_list_foreach(ext_list, s_print_list, NULL);
}

/*
 * Both get functions need to be protected if the lists are
 * going to be worked on. Some other thread could add to or delete
 * this list.
 */
#if 0
void
dm2_get_extents_by_physid(const char	*in_physid,
			  GList		**out_extent_list)
{
    assert(in_physid);
    *out_extent_list = (GList *)nkn_locmgr2_physid_tbl_get(in_physid);
}
#endif


/*
 * Insert an extent into a hash table, given a unique 64-bit value.
 * - Since physids are unique and no two objects can map to the same
 *   physid, we do not need a list.
 * - Locking required for the hash table is done within the lower function.
 *   Individual object locks must be handled separately.
 */
int
dm2_extent_insert_by_physid(const uint64_t *in_physid,
			    DM2_extent_t   *in_ext,
			    dm2_cache_type_t *ct)
{
    int	ret_val;
    char *uri = in_ext->ext_uri_head->uri_name;

    ret_val = dm2_physid_tbl_ext_insert(in_physid, in_ext, ct);
    if (ret_val < 0) {
	DBG_DM2S("Error: physid/extent should not be already present: "
		 "physid=%lx uri=%s", *in_physid, uri ? uri : "no name");
	return ret_val;
    }

    return 0;
}	/* dm2_extent_insert_by_physid */


DM2_physid_head_t *
dm2_extent_get_by_physid(const uint64_t *in_physid,
			 dm2_cache_type_t *ct)
{
    DM2_physid_head_t *physid_head;

    physid_head = dm2_physid_tbl_get(in_physid, ct);
    return physid_head;
}	/* dm2_extent_get_by_physid */


DM2_physid_head_t *
dm2_extent_get_by_physid_rlocked(const uint64_t   *in_physid,
				 dm2_cache_type_t *ct)
{
    DM2_physid_head_t *physid_head;

    physid_head = dm2_physid_tbl_get(in_physid, ct);
    if (physid_head)
	dm2_physid_rwlock_rlock(physid_head);
    return physid_head;
}	/* dm2_extent_get_by_physid_rlocked */


DM2_physid_head_t *
dm2_extent_get_by_physid_wlocked(const uint64_t   *in_physid,
				 dm2_cache_type_t *ct)
{
    DM2_physid_head_t *physid_head;

    physid_head = dm2_physid_tbl_get(in_physid, ct);
    if (physid_head)
	dm2_physid_rwlock_wlock(physid_head);
    return physid_head;
}	/* dm2_extent_get_by_physid_wlocked */


#if 0
/*
 * - This function deletes all extents given a uri string.
 * - This function needs to be called with a lock so that delete
 *   will not trod on some other get.
 */
int
dm2_extent_delete_all_by_uri(char *in_string)
{
    GList *extent_list = NULL;
    int   list_found = 0;
    int   ret_val = 0;

    extent_list = (GList *) nkn_locmgr2_uri_tbl_get(in_string);
    if (extent_list != NULL) {
	list_found = 1;
    }

    if (list_found == 1) {
	nkn_locmgr2_uri_tbl_delete(in_string);
    }
    return ret_val;
}	/* dm2_extent_delete_all_by_uri */


/*
 * - This function deletes all extents given a physid string.
 * - This function needs to be called with a lock so that delete
 *   will not trod on some other get.
 */
int
dm2_extent_delete_all_by_physid(char *in_string)
{
    GList *extent_list = NULL;
    int   list_found = 0;
    int   ret_val = 0;

    extent_list = (GList *) nkn_locmgr2_physid_tbl_get(in_string);
    if (extent_list != NULL) {
	list_found = 1;
    }

    if (list_found == 1) {
	nkn_locmgr2_physid_tbl_delete(in_string);
    }
    return ret_val;
}	/* dm2_extent_delete_all_by_physid */
#endif


void
dm2_print_mem_extent(void *value)
{
    const DM2_extent_t *ext = (DM2_extent_t *)value;
    const guint32 w1 = (ext->ext_physid & 0xffffffff);
    const guint32 w2 = (ext->ext_physid & 0xffffffff00000000) >> 32;

    printf("%s %lu %u 0x%lx %lu %u %u 0x%x\n", ext->ext_uri_head->uri_name,
	   ext->ext_offset, ext->ext_length, ext->ext_physid,
	   ext->ext_start_sector, ext->ext_start_attr, ext->ext_cont_off,
	   w1 ^ w2);
}	/* dm2_print_extent */


DM2_extent_t *
dm2_find_extent_by_offset(GList		*in_ext_list, // need to copy ptr
			  const off_t	uri_offset,
			  const off_t	uri_len __attribute__((unused)),
			  off_t		*tot_len,
			  off_t         *unavail_offset,
			  int		*object_with_hole)
{
    DM2_extent_t *ext;
    GList	 *ext_list;
    GList	 *ext_targ = NULL;
    int          get_unavail_offset = (unavail_offset?1:0);

    ext_list = in_ext_list;

    /*
     * Extent list is ordered for efficiency
     *
     * TBD: we can make this search more efficient in the future
     * Right now O(n) but the length of the list is so small that
     * it is insignificant.
     */
    while (ext_list != NULL) {
	ext = (DM2_extent_t *) ext_list->data;
	*tot_len += ext->ext_length;

	if(get_unavail_offset) {
	    if(*unavail_offset == ext->ext_offset) {
		*unavail_offset = ext->ext_offset + ext->ext_length;
		*object_with_hole = 0;
	    } else {
		*object_with_hole = 1;
	    }
	}

	if ((ext->ext_offset <= uri_offset) &&
	    (ext->ext_offset + ext->ext_length > uri_offset)) {
	    ext_targ = ext_list;
	    break;
	}
	ext_list = ext_list->next;
    }

    /* Go to the end of the list to find the total length */
    if (ext_list)
	ext_list = ext_list->next;
    while (ext_list != NULL) {
	ext = (DM2_extent_t *) ext_list->data;
	*tot_len += ext->ext_length;

	if(get_unavail_offset) {
	    if(*unavail_offset == ext->ext_offset) {
		*unavail_offset = ext->ext_offset + ext->ext_length;
		*object_with_hole = 0;
	    } else {
		*object_with_hole = 1;
	    }
	}

	ext_list = ext_list->next;
	continue;
    }

    if (ext_targ != NULL)
	return ((DM2_extent_t *)ext_targ->data);
    else
	return NULL;
}	/* dm2_find_extent_by_offset */


/*
 * Does a match by sector offset
 */
DM2_extent_t *
dm2_find_extent_by_offset2(GList	*in_ext_list, // need to copy ptr
			   const off_t	uri_offset)
{
    DM2_extent_t *ext;
    GList	 *ext_list;
    GList	 *ext_targ = NULL;
    off_t	 off;

    ext_list = in_ext_list;

    /*
     * Extent list is ordered for efficiency
     *
     * TBD: we can make this search more efficient in the future
     * Right now O(n) but the length of the list is so small that
     * it is insignificant.
     */
    while (ext_list != NULL) {
	ext = (DM2_extent_t *) ext_list->data;
	off = ext->ext_start_sector*DEV_BSIZE;

	if ((off <= uri_offset) && (off + ext->ext_length > uri_offset)) {
	    ext_targ = ext_list;
	    break;
	}
	ext_list = ext_list->next;
    }

    if (ext_targ != NULL)
	return ((DM2_extent_t *)ext_targ->data);
    else
	return NULL;
}	/* dm2_find_extent_by_offset2 */


/*
 * This routine will leak a bit of memory in the error case, but leaking
 * is the least of our worries.  If we can't allocate new hash tables, this
 * cache type/sub-provider is not valid and we will not be able to bring
 * some storage online.
 */
int
dm2_tbl_init_hash_tables(dm2_cache_type_t *ct)
{
    int ret;

    if ((ret = dm2_conthead_tbl_init(ct)) < 0)
	return ret;
    if ((ret = dm2_physid_tbl_init(ct)) < 0)
	return ret;
    return 0;
}	/* dm2_tbl_init_hash_tables */

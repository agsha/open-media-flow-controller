#include <string.h>
#include <sys/param.h>
#include "nkn_attr.h"
#include "nkn_errno.h"
#include "nkn_defs.h"
#include "nkn_memalloc.h"
#if 1
#include "nkn_assert.h"
#else
/* The NKN_ASSERT macro definition is not working properly in this directory */
/* TBD:NKN_ASSERT */
#define NKN_ASSERT(_e)
#include <assert.h>
#endif

uint64_t glob_nkn_attr_copy_err;
int nkn_attr_copy_zero = 1;

int
nkn_attr_set(nkn_attr_t	   *ap,
	     nkn_attr_id_t id,
	     uint32_t      len,
	     void	   *value)
{
	nkn_attr_entry_t *entry;
	int addlen;

	assert(sizeof(nkn_attr_entry_data_t) == 4);
	assert(sizeof(nkn_attr_entry_t) == 8);
	assert(NKN_ATTR_HEADER_LEN == (int)(256));

	/*
	 * No need to check the validity of 'len'.  If it were too large, we
	 * would overflow against total_attrsize
	 */

	// Check for overflow
	addlen = sizeof(nkn_attr_entry_t) + len;
	if ((ap->blob_attrsize + addlen) > ap->total_attrsize)
		return NKN_ATTR_OVERFLOW;

	/*
	 * By adding these 2 values, we could have an odd address which is
	 * supposed on x86 but not on other ARCHs, like MIPS
	 */
	entry = (nkn_attr_entry_t *)((char *)ap + ap->blob_attrsize);
	entry->id = id;
	entry->attr_entry_len = len;
	memcpy((char *)entry + sizeof(nkn_attr_entry_t), value, len);
	ap->na_entries++;
	ap->blob_attrsize += addlen;
	return 0;
}

void *
nkn_attr_get(nkn_attr_t		 *ap,
	     const nkn_attr_id_t id,
	     uint32_t		 *len)
{
	int i = 0, elen;
	void *val = NULL;
	nkn_attr_entry_t *entry;

	entry = (nkn_attr_entry_t *)ap->na_blob;
	while (i < ap->na_entries) {
		if (entry->id.u.attr_id == id.u.attr_id) {
			val = (char *)entry + sizeof(nkn_attr_entry_t);
			*len = entry->attr_entry_len;
			break;
		}
		elen = sizeof(nkn_attr_entry_t) + entry->attr_entry_len;
		i++;
		entry = (nkn_attr_entry_t *)((char *)entry + elen);
		assert(((uint64_t)entry - (uint64_t)ap) < ap->total_attrsize);
	}
	return val;
}

int
nkn_attr_get_nth(nkn_attr_t	*ap,
		 const int	nth,
		 nkn_attr_id_t	*id,
		 void		**data,
		 uint32_t	*len)
{
	int i = 0, elen;
	nkn_attr_entry_t *entry;

	if (nth >= ap->na_entries) {
		return 1; // Invalid nth value
	}

	entry = (nkn_attr_entry_t *)ap->na_blob;
	while (i < ap->na_entries) {
		if (i == nth) {
			*id = entry->id;
			*data = (char *)entry + sizeof(nkn_attr_entry_t);
			*len = entry->attr_entry_len;
			break;
		}
		elen = sizeof(nkn_attr_entry_t) + entry->attr_entry_len;
		i++;
		entry = (nkn_attr_entry_t *)((char *)entry + elen);
		assert(((uint64_t)entry - (uint64_t)ap) < ap->total_attrsize);
	}
	return 0; // Success
}

void
nkn_attr_reset_blob(nkn_attr_t *ap, int zero_used_blob)
{
	if (zero_used_blob) {
		memset((char *)ap->na_blob, 0, 
	       		ap->blob_attrsize - NKN_ATTR_HEADER_LEN);
	}
	ap->na_entries = 0;
	ap->blob_attrsize = NKN_ATTR_HEADER_LEN;
}

void
nkn_attr_init(nkn_attr_t *ap, int maxlen)
{
	/* Assume that the caller has allocated a correct size buffer */
	memset(ap, 0, maxlen);
	ap->na_magic = NKN_ATTR_MAGIC;
	ap->na_version = NKN_ATTR_VERSION;
	ap->na_entries = 0;
	ap->blob_attrsize = NKN_ATTR_HEADER_LEN;
	ap->total_attrsize = maxlen;	// Current maximum
	ap->start_offset = INT64_MAX - 1;
}

void
nkn_attr_docopy(nkn_attr_t *ap_src, nkn_attr_t *ap_dst, uint32_t dstsize)
{
	assert(dstsize >= ap_src->total_attrsize);
    	memcpy(ap_dst, ap_src, ap_src->blob_attrsize);
	ap_dst->total_attrsize = dstsize;
	memset((char *)((uint64_t)(ap_dst) + (uint64_t)ap_src->blob_attrsize), 0, ap_src->total_attrsize - ap_src->blob_attrsize);
}

int
nkn_attr_copy(nkn_attr_t *ap_src,
	      nkn_attr_t **ap_dst,
	      nkn_obj_type_t objtype)
{
    int ret;

    ret = nkn_posix_memalign_type((void *)ap_dst, DEV_BSIZE,
				  ap_src->total_attrsize, objtype);
    if (ret != 0) {
	glob_nkn_attr_copy_err++;
	return ret;
    }

    nkn_attr_docopy(ap_src, *ap_dst, ap_src->total_attrsize);
    return 0;
}


void
nkn_attr_set_streaming(nkn_attr_t *ap)
{
    ap->na_flags |= NKN_OBJ_STREAMING;
    return;
}

void
nkn_attr_reset_streaming(nkn_attr_t *ap)
{
    ap->na_flags &= ~NKN_OBJ_STREAMING;
    return;
}

int
nkn_attr_is_streaming(nkn_attr_t *ap)
{
    return (ap->na_flags & NKN_OBJ_STREAMING) ? 1 : 0;
}

void
nkn_attr_update_content_len(nkn_attr_t *ap, int len)
{
    ap->content_length = len;
    return;
}

void
nkn_attr_set_cache_pin(nkn_attr_t *ap)
{
    ap->na_flags |= NKN_OBJ_CACHE_PIN;
}

void
nkn_attr_reset_cache_pin(nkn_attr_t *ap)
{
    ap->na_flags &= ~NKN_OBJ_CACHE_PIN;
}

int
nkn_attr_is_cache_pin(const nkn_attr_t *ap)
{
    return (ap->na_flags & NKN_OBJ_CACHE_PIN) ? 1 : 0;
}

void
nkn_attr_set_negcache(nkn_attr_t *ap)
{
    ap->na_flags2 |= NKN_OBJ_NEGCACHE;
}

void
nkn_attr_reset_negcache(nkn_attr_t *ap)
{
    ap->na_flags2 &= ~NKN_OBJ_NEGCACHE;
}

int
nkn_attr_is_negcache(const nkn_attr_t *ap)
{
    return (ap->na_flags2 & NKN_OBJ_NEGCACHE) ? 1 : 0;
}

void
nkn_attr_set_vary(nkn_attr_t *ap)
{
    ap->na_flags2 |= NKN_OBJ_VARY;
}

void
nkn_attr_reset_vary(nkn_attr_t *ap)
{
    ap->na_flags2 &= ~NKN_OBJ_VARY;
}

int
nkn_attr_is_vary(const nkn_attr_t *ap)
{
    return (ap->na_flags2 & NKN_OBJ_VARY) ? 1 : 0;
}


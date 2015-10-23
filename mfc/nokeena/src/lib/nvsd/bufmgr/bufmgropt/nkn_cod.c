#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/queue.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <glib.h>
#include <string.h>

#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_cod.h"
#include "nkn_assert.h"
#include "nkn_memalloc.h"
#include "../nkn_cod_private.h"
#include "nkn_lockstat.h"
#include "zvm_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_hash.h"

#define NKN_CEFLAG_PUSH_INGEST 16	        // Object is ingested via push
#define NKN_CEFLAG_PULL_INGEST 32	        // Object is ingested via push

static struct nkn_cod_entry_list nkn_cod_entry_free;	/* free list of entries */
static struct nkn_cod_head_list nkn_cod_head_free;	/* free list of heads */
extern nkn_cod_entry_t *nkn_cod_entries;	/* pre-allocated entries */
static nkn_cod_head_t *nkn_cod_heads;		/* pre-allocated heads */
static nkn_mutex_t nkn_cod_mutex[MAX_COD_HASH];
static nkn_mutex_t nkn_cod_entry_free_mutex;
static nkn_mutex_t nkn_cod_head_free_mutex;

static NCHashTable *nkn_cod_hash[MAX_COD_HASH];
static u_int32_t glob_cod_entries = 0;
static int glob_cod_head_free = 0;
static int glob_cod_entry_free = 0;
static int glob_cod_head_hash = 0;
static unsigned long glob_cod_hit = 0;
static unsigned long glob_cod_miss = 0;
static int glob_cod_head_overflow = 0;
static int glob_cod_entry_overflow = 0;
static int glob_cod_cn_allocfail = 0;
static int glob_cod_expire = 0;
static int glob_cod_reclaim = 0;
static int glob_cod_rev_mismatch = 0;
static int glob_cod_zeroref = 0;
static int glob_cod_invalid_expire = 0;
static int glob_cod_unknown_expire = 0;
static int glob_cod_new_version = 0;
static int glob_cod_version_update = 0;
static int glob_cod_stream_failopen = 0;
static int glob_cod_invalid_stream = 0;
static int glob_cod_stream = 0;
static int glob_cod_stream_complete = 0;
uint64_t glob_cod_genbuf_ingestion_failure = 0;
AO_t cod_owner_counter[NKN_COD_MAX_MGR];

#define COD_REV_SHIFT	40
#define COD_INDEX_MASK	0xffffffffff
#define CP_TO_COD(cp)	((cp)->index + (((u_int64_t)AO_load(&(cp)->revision)) << COD_REV_SHIFT))

#define INC_COWN_CNT(X) {			\
	AO_fetch_and_add1(&cod_owner_counter[X]);			\
    }

#define DEC_COWN_CNT(X) {			\
	AO_fetch_and_sub1(&cod_owner_counter[X]);			\
    }

static void
nkn_cod_ingest_detach(nkn_cod_entry_t *cp, void **iptr, char **cn);

static inline nkn_cod_entry_t *
nkn_cod_to_cp(nkn_cod_t cod)
{
	uint64_t c_index = cod & COD_INDEX_MASK;
	if (c_index >= glob_cod_entries)
		return NULL;
	return &nkn_cod_entries[c_index];
}

static inline int
nkn_cod_is_valid(nkn_cod_entry_t *cp, nkn_cod_t cod)
{
	uint32_t rev = cod >> COD_REV_SHIFT;

	if (AO_load(&cp->revision) != rev) {
		glob_cod_rev_mismatch++;
		return 0;
	}
	if (!AO_load(&cp->refcnt)) {
		glob_cod_zeroref++;
		return 0;
	}
	return 1;
}

// #define COD_UNIT_TEST
#ifdef COD_UNIT_TEST
void nkn_cod_unit_test(void);
#endif

void nkn_cod_init(int entries)
{
	u_int32_t i;
	nkn_cod_entry_t *cp;
	nkn_cod_head_t *hp;
	int bucket;
	uint64_t size;

	glob_cod_entries = entries;
	nkn_cod_entries = nkn_calloc_type(glob_cod_entries, sizeof(nkn_cod_entry_t), mod_cod_entry_t);
	if (nkn_cod_entries == NULL) {
		err(1, "Failed to allocate COD entries\n");
	}
	int cindex;
	char name[100];
	for (cindex  =0; cindex < MAX_COD_HASH ; cindex++) {
		snprintf(name, 100, "cod-mutex-%d", cindex);
		NKN_MUTEX_INIT(&nkn_cod_mutex[cindex], NULL, name);
	}
	snprintf(name, 100, "cod-entry-free");
	NKN_MUTEX_INIT(&nkn_cod_entry_free_mutex, NULL, name);
	snprintf(name, 100, "cod-head-free");
	NKN_MUTEX_INIT(&nkn_cod_head_free_mutex, NULL, name);
	TAILQ_INIT(&nkn_cod_entry_free);
	for (i=0,cp=nkn_cod_entries; i<glob_cod_entries; i++,cp++) {
		cp->index = i;
		pthread_mutex_init(&cp->cod_entry_mutex, NULL);
		TAILQ_INSERT_TAIL(&nkn_cod_entry_free, cp, entrylist);
	}
	glob_cod_entry_free = glob_cod_entries;
	nkn_cod_heads = nkn_calloc_type(glob_cod_entries, sizeof(nkn_cod_head_t), mod_cod_head_t);
	if (nkn_cod_heads == NULL) {
		err(1, "Failed to allocate COD heads\n");
	}
	TAILQ_INIT(&nkn_cod_head_free);
	for (i=0,hp=nkn_cod_heads; i<glob_cod_entries; i++,hp++) {
		pthread_mutex_init(&hp->lock, NULL);
		TAILQ_INSERT_TAIL(&nkn_cod_head_free, hp, headlist);
	}
	glob_cod_head_free = glob_cod_entries;

	bucket = nhash_find_hash_bucket(glob_cod_entries / MAX_COD_HASH);
	if (bucket == -1)
		err(1, "Failed to get bucket for cod entries\n");
	size = NHashPrime[bucket];
	bucket = nhash_find_hash_bucket(size + 1);
	if (bucket == -1)
		err(1, "Failed to get bucket for cod entries\n");
	int hindex;
	for ( hindex = 0; hindex < MAX_COD_HASH; hindex++) {
	nkn_cod_hash[hindex] = nchash_table_new(hashlittle, n_str_equal,
					NHashPrime[bucket],
					NKN_CHAINFIELD_OFFSET(nkn_cod_head_t, __entry),
					mod_con_hash_t);
	if (nkn_cod_hash[hindex] == NULL) {
		err(1, "Failed to allocate COD hash\n");
	}

	}

#ifdef COD_UNIT_TEST
	nkn_cod_unit_test();
#endif
	/* Initialize cod Ownership counters */
	nkn_mon_add("cod_owner_filemgr_cnt"   , NULL,
		(void *)&cod_owner_counter[NKN_COD_FILE_MGR],
		sizeof(cod_owner_counter[NKN_COD_FILE_MGR]));
	nkn_mon_add("cod_owner_bufmgr_cnt"    , NULL,
		(void *)&cod_owner_counter[NKN_COD_BUF_MGR],
		sizeof(cod_owner_counter[NKN_COD_BUF_MGR]));
	nkn_mon_add("cod_owner_analytics_mgr_cnt" , NULL,
		(void *)&cod_owner_counter[NKN_COD_AM_MGR],
		sizeof(cod_owner_counter[NKN_COD_AM_MGR]));
	nkn_mon_add("cod_owner_media_mgr_cnt" , NULL,
		(void *)&cod_owner_counter[NKN_COD_MEDIA_MGR],
		sizeof(cod_owner_counter[NKN_COD_MEDIA_MGR]));
	nkn_mon_add("cod_owner_tfm_mgr_cnt"   , NULL,
		(void *)&cod_owner_counter[NKN_COD_TFM_MGR],
		sizeof(cod_owner_counter[NKN_COD_TFM_MGR]));
	nkn_mon_add("cod_owner_disk_mgr_cnt"  , NULL,
		(void *)&cod_owner_counter[NKN_COD_DISK_MGR],
		sizeof(cod_owner_counter[NKN_COD_DISK_MGR]));
	nkn_mon_add("cod_owner_nw_mgr_cnt"    , NULL,
		(void *)&cod_owner_counter[NKN_COD_NW_MGR],
		sizeof(cod_owner_counter[NKN_COD_NW_MGR]));
	nkn_mon_add("cod_owner_vfs_mgr_cnt"   , NULL,
		(void *)&cod_owner_counter[NKN_COD_VFS_MGR],
		sizeof(cod_owner_counter[NKN_COD_VFS_MGR]));
	nkn_mon_add("cod_owner_get_mgr_cnt"   , NULL,
		(void *)&cod_owner_counter[NKN_COD_GET_MGR],
		sizeof(cod_owner_counter[NKN_COD_GET_MGR]));
	nkn_mon_add("cod_owner_origin_mgr_cnt", NULL,
		(void *)&cod_owner_counter[NKN_COD_ORIGIN_MGR],
		sizeof(cod_owner_counter[NKN_COD_ORIGIN_MGR]));
	nkn_mon_add("cod_owner_ns_mgr_cnt"    , NULL,
		(void *)&cod_owner_counter[NKN_COD_NS_MGR],
		sizeof(cod_owner_counter[NKN_COD_NS_MGR]));
	nkn_mon_add("cod_owner_cod_mgr_cnt"   , NULL,
		(void *)&cod_owner_counter[NKN_COD_COD_MGR],
		sizeof(cod_owner_counter[NKN_COD_COD_MGR]));
}

/*
 * Put a COD entry on the free list.  Called with COD mutex held.
 */
static void
nkn_cod_put_entry(nkn_cod_entry_t *cp)
{
	// Treat as stack so that we get a high water mark usage and
	// also expose dangling references faster
	TAILQ_INSERT_HEAD(&nkn_cod_entry_free, cp, entrylist);
	glob_cod_entry_free++;
}

/*
 * Add head to free list.  Called with mutex held.
 */
static void
nkn_cod_head_add_free(nkn_cod_head_t *hp)
{
	// LRU behavior for now.  Could take hits into account.
	TAILQ_INSERT_TAIL(&nkn_cod_head_free, hp, headlist);
	hp->flags |= NKN_CHFLAG_FREE;
	glob_cod_head_free++;
}

/*
 * Remove head from free list.  Called with mutex held.
 */
static void
nkn_cod_head_rem_free(nkn_cod_head_t *hp)
{
	TAILQ_REMOVE(&nkn_cod_head_free, hp, headlist);
	hp->flags &= ~NKN_CHFLAG_FREE;
	glob_cod_head_free--;
}

/*
 * Get a head entry from the free list.  If it is still in the hash, it will
 * be removed from it.
 */
static nkn_cod_head_t *
nkn_cod_get_hp(void)
{
	nkn_cod_head_t *hp;
	uint64_t hash;
	int hindex;

retry:
	NKN_MUTEX_LOCK(&nkn_cod_head_free_mutex);
	hp = nkn_cod_head_free.tqh_first;
	if (hp == NULL) {
		glob_cod_head_overflow++;
		NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
		return NULL;
	}
	NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
	pthread_mutex_lock(&hp->lock);
	NKN_MUTEX_LOCK(&nkn_cod_head_free_mutex);
	if (hp != nkn_cod_head_free.tqh_first) {
		NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
		pthread_mutex_unlock(&hp->lock);
		goto retry;
	}
	nkn_cod_head_rem_free(hp);
	NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
	if (hp->flags & NKN_CHFLAG_HASH) {
		hash = hashlittle(hp->cn, hp->cnlen);
		if (hp->cnlen >= MAX_COD_URI_HASH)
			hindex = *((uint32_t *)(&hp->cn[hp->cnlen-
				   MAX_COD_URI_HASH])) % MAX_COD_HASH;
		else
			hindex = COD_URI_HASH_DEFAULT;
		NKN_MUTEX_LOCK(&nkn_cod_mutex[hindex]);
		nchash_remove_off0(nkn_cod_hash[hindex], hash, hp->cn);
		glob_cod_head_hash--;
                ZVM_DEQUEUE(hp);
		NKN_MUTEX_UNLOCK(&nkn_cod_mutex[hindex]);
		hp->flags &= ~NKN_CHFLAG_HASH;
	}
	return hp;
}

static void
nkn_cod_put_hp(nkn_cod_head_t *hp)
{
	NKN_MUTEX_LOCK(&nkn_cod_head_free_mutex);
        ZVM_ENQUEUE(hp);
	nkn_cod_head_add_free(hp);
	NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
}

/* Get a new COD head for the specified cache name.  If the cache name is
* larger than the name buffer, we need to allocate a new one.
* XXX - Organize the free head entries by name buffer size.
*/
static nkn_cod_head_t *
nkn_cod_alloc_hp(const char *cn, size_t length)
{
	nkn_cod_head_t *hp;
	int namelen;

	hp = nkn_cod_get_hp();
	if (hp == NULL)
		return NULL;
	namelen = length + 1;		// includes terminating null
	if (namelen > (hp->cnlen + 1)) {
		char *newcn;

		newcn = nkn_malloc_type(namelen, mod_cod_cn);
		if (!newcn) {
			glob_cod_cn_allocfail++;
			nkn_cod_put_hp(hp);
			pthread_mutex_unlock(&hp->lock);
			return NULL;
		}
		if (hp->cn)
			free(hp->cn);
		hp->cn = newcn;
	}
	hp->cnlen = namelen - 1;
	memcpy(hp->cn, cn, namelen);
	hp->flags &= ~NKN_CHFLAG_NEWVER;
	return hp;
}

 /*
  * Get a COD entry from the free list.
  */
static nkn_cod_entry_t *
nkn_cod_get_cp_entry(void)
{
	nkn_cod_entry_t *cp;

retry:
	NKN_MUTEX_LOCK(&nkn_cod_entry_free_mutex);
	cp = nkn_cod_entry_free.tqh_first;
	if (cp == NULL) {
		glob_cod_entry_overflow++;
		NKN_MUTEX_UNLOCK(&nkn_cod_entry_free_mutex);
		return NULL;
	}
	NKN_MUTEX_UNLOCK(&nkn_cod_entry_free_mutex);
	pthread_mutex_lock(&cp->cod_entry_mutex);
	NKN_MUTEX_LOCK(&nkn_cod_entry_free_mutex);
	if (cp != nkn_cod_entry_free.tqh_first) {
		NKN_MUTEX_UNLOCK(&nkn_cod_entry_free_mutex);
		pthread_mutex_unlock(&cp->cod_entry_mutex);
		goto retry;
	}
	TAILQ_REMOVE(&nkn_cod_entry_free, cp, entrylist);
	glob_cod_entry_free--;
	NKN_MUTEX_UNLOCK(&nkn_cod_entry_free_mutex);
	AO_store(&cp->refcnt, 0);
	AO_fetch_and_add1(&cp->revision);
	// only 32 bits is used for revision
	if ((AO_load(&cp->revision) % UINT32_MAX) == 0)	// revision is uint64_t
		AO_store(&cp->revision, 1);
	cp->flags = NKN_CEFLAG_UNKNOWN;
	memset(&cp->version, 0, sizeof(nkn_objv_t));
	cp->head = 0;
	cp->client_offset = 0;
	cp->ingest_offset = 0;
	return cp;
}

/*
 * Returns true(1) if push in progress
 */
int
nkn_cod_is_push_in_progress(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	int ret = 0;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return ret;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	if(cp->flags & NKN_CEFLAG_PUSH_INGEST)
		ret = 1;
	pthread_mutex_unlock(&cp->cod_entry_mutex);

	return ret;
}

/*
 * Returns true(1) if pull ingest in progress
 */
int
nkn_cod_is_ingest_in_progress(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	int ret = 0;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return ret;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	if(cp->flags & NKN_CEFLAG_PULL_INGEST)
		ret = 1;
	pthread_mutex_unlock(&cp->cod_entry_mutex);

	return ret;
}

/*
 * Set pull ingest in progress flag
 */
void
nkn_cod_set_ingest_in_progress(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	cp->flags |= NKN_CEFLAG_PULL_INGEST;
	pthread_mutex_unlock(&cp->cod_entry_mutex);

}

/*
 * Set push in progress flag
 */
void
nkn_cod_set_push_in_progress(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	cp->flags |= NKN_CEFLAG_PUSH_INGEST;
	pthread_mutex_unlock(&cp->cod_entry_mutex);

}

/*
 * Clear push in progress flag
 */
void
nkn_cod_clear_push_in_progress(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	cp->flags &= ~NKN_CEFLAG_PUSH_INGEST;
	pthread_mutex_unlock(&cp->cod_entry_mutex);

}

/*
 * Clear pull ingest in progress flag
 */
void
nkn_cod_clear_ingest_in_progress(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	cp->flags &= ~NKN_CEFLAG_PULL_INGEST;
	pthread_mutex_unlock(&cp->cod_entry_mutex);

}

/*
 * Return the number of real clients
 * Based on the return value the ingest pointer will be
 * cleaned up
 */
int
nkn_cod_push_ingest_get_clients(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	int ret = 0;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return -1;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if(AO_load(&cp->client_refcnt) > 1) {
		ret = -1;
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);

	return ret;
}

/*
 * Attach ingest pointer to a COD.
 * Caller must ensure that the version in the attributes matches the COD.
 * Caller must ensure that the attribute buffer will be valid until a detach
 * is called.
 * Returns zero on success and error code on failure:
 *	NKN_COD_INVALID_COD:  the COD is no longer valid 
 *	NKN_COD_UNKNOWN_COD:  the COD has no version assigned
 *	NKN_COD_EXPIRED:      the COD has expired
 */
int
nkn_cod_ingest_attach(nkn_cod_t cod, void *ingest_ptr, off_t ingest_offset)
{
	nkn_cod_entry_t *cp;
	int ret = -1;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_INVALID_COD;

	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (AO_load(&cp->client_refcnt) > 1) {
		cp->iptr = ingest_ptr;
		cp->ingest_offset = ingest_offset;

		/* Reduce the ref count that was taken up by MM cod open */
		AO_fetch_and_sub1(&cp->refcnt);
		AO_fetch_and_sub1(&cp->client_refcnt);

		ret = 0;
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return ret;
}

/*
 * Detach ingest pointer from a COD.
 * Returns zero on success and error code on failure:
 *      NKN_COD_INVALID_COD:  the COD is no longer valid
 */
static void
nkn_cod_ingest_detach(nkn_cod_entry_t *cp, void **iptr,
		      char **cn)
{
	*iptr = NULL;
	*cn = cp->head->cn;
	if(cp->iptr) {
		*iptr = cp->iptr;
		cp->iptr = NULL;
		AO_fetch_and_add1(&cp->refcnt);
		AO_fetch_and_add1(&cp->client_refcnt);
	}
}

/*
 * Get a COD (reference counted) for the specified cache object.  Returns NULL
 * if there are no more entries available.
 */
nkn_cod_t 
nkn_cod_open(const char *cn, size_t length,
	     nkn_cod_owner_t cown)
{
	nkn_cod_entry_t *cp;
	uint64_t hash;
	nkn_cod_head_t *hp, *newhp=NULL;
	nkn_cod_t rcod = NKN_COD_NULL;
	NKN_ASSERT(cown < NKN_COD_MAX_MGR);
	hash = hashlittle(cn, length);
	int hindex;
	if (length >= MAX_COD_URI_HASH)
		hindex = *((uint32_t *)(&cn[length - MAX_COD_URI_HASH])) % MAX_COD_HASH;
	else
		hindex = COD_URI_HASH_DEFAULT;
cod_relookup:
	NKN_MUTEX_LOCK(&nkn_cod_mutex[hindex]);
	hp = nchash_lookup_off0(nkn_cod_hash[hindex], hash, cn);
	if (hp) {
		NKN_MUTEX_UNLOCK(&nkn_cod_mutex[hindex]);
		glob_cod_hit++;
		pthread_mutex_lock(&hp->lock);
		if (!(hp->flags & NKN_CHFLAG_HASH)) {
			pthread_mutex_unlock(&hp->lock);
			goto cod_relookup;
		}
	} else {
		glob_cod_miss++;
		if (newhp) {
			nchash_insert_new_off0(nkn_cod_hash[hindex], hash, newhp->cn, newhp);
			glob_cod_head_hash++;
			NKN_MUTEX_UNLOCK(&nkn_cod_mutex[hindex]);
			hp = newhp;
			hp->flags |= NKN_CHFLAG_HASH;
			newhp = NULL;
		} else {
			NKN_MUTEX_UNLOCK(&nkn_cod_mutex[hindex]);
			newhp = nkn_cod_alloc_hp(cn, length); // newhp locked if not null
			if (!newhp) {
				return NKN_COD_NULL;
			}
			goto cod_relookup;
		}
	}
	if (newhp) {
		NKN_MUTEX_LOCK(&nkn_cod_head_free_mutex);
                ZVM_ENQUEUE(newhp);
		nkn_cod_head_add_free(newhp);
		NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
		pthread_mutex_unlock(&newhp->lock);
	}
	if (hp->flags & NKN_CHFLAG_FREE) {
		NKN_MUTEX_LOCK(&nkn_cod_head_free_mutex);
                ZVM_ENQUEUE(hp);
		nkn_cod_head_rem_free(hp);
		glob_cod_reclaim++;
		NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
	}
	cp = hp->entries.tqh_first;
	if (cp)
		pthread_mutex_lock(&cp->cod_entry_mutex);
	if (!cp || (cp->flags & NKN_CEFLAG_EXPIRED)) {
		if (cp) {
                        // expired MM cod open will fail incase of
                        // client driven request
                        if (cown == NKN_COD_MEDIA_MGR) {
				glob_cod_genbuf_ingestion_failure++;
                        }
			pthread_mutex_unlock(&cp->cod_entry_mutex);
		}
		cp = nkn_cod_get_cp_entry();   // cp locked if cp not null
                ZVM_DEQUEUE(cp);
		if (!cp) {
			glob_cod_entry_overflow++;
			pthread_mutex_unlock(&hp->lock);
			return NKN_COD_NULL;
		}
		TAILQ_INSERT_HEAD(&hp->entries, cp, entrylist);
		cp->head = hp;
		if (hp->flags & NKN_CHFLAG_NEWVER) {
			memcpy(&cp->version, &cp->head->new_version, sizeof(nkn_objv_t));
			cp->flags &= ~NKN_CEFLAG_UNKNOWN;
			cp->flags |= NKN_CEFLAG_NEWVER;
			glob_cod_new_version++;
		}
	}
	if (cp->flags & NKN_CEFLAG_STREAMING) {
		pthread_mutex_unlock(&cp->cod_entry_mutex);
		pthread_mutex_unlock(&hp->lock);
		glob_cod_stream_failopen++;
		return NKN_COD_NULL;
	}
	AO_fetch_and_add1(&cp->refcnt);
	if(cown != NKN_COD_BUF_MGR)
	    AO_fetch_and_add1(&cp->client_refcnt);
	cp->open_mod_entry = 1 << cown;
	rcod = CP_TO_COD(cp);
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	pthread_mutex_unlock(&hp->lock);
	INC_COWN_CNT(cown);
	return rcod;


}
/*
 * Get a COD (reference counted) for the specified cache object.  Returns NULL
 * if there are no more entries available.
 */
nkn_cod_t 
nkn_cod_dup(nkn_cod_t cod, nkn_cod_owner_t cown)
{
	nkn_cod_entry_t *cp;
	nkn_cod_t rcod;
	NKN_ASSERT(cown < NKN_COD_MAX_MGR);
	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_NULL;
	if (nkn_cod_is_valid(cp, cod)) {
		AO_fetch_and_add1(&cp->refcnt);
		cp->open_mod_entry = 1 << cown;
		if(cown != NKN_COD_BUF_MGR)
		    AO_fetch_and_add1(&cp->client_refcnt);
		INC_COWN_CNT(cown);
		rcod = cod;
	} else
		rcod = NKN_COD_NULL;
	return rcod;
}


/* Close a COD previously opened */
void nkn_cod_close(nkn_cod_t cod, nkn_cod_owner_t cown)
{
	nkn_cod_entry_t *cp;
	int cod_detach = 0;
	void *iptr = NULL;
	char *cn = NULL;
	uint64_t client_offset = 0;
	NKN_ASSERT(cown < NKN_COD_MAX_MGR);
	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;
	if (nkn_cod_is_valid(cp, cod)) {
		nkn_cod_head_t *hp;
		assert(AO_load(&cp->refcnt));

		/* This is the last client
		 * trigger other modues during close for cleanup
		 * For now its only ingest
		 */
#if 0
		if (cown != NKN_COD_BUF_MGR &&
			AO_load(&cp->client_refcnt) == 1) {
			nkn_cod_ingest_detach(cod, INGEST_CLEANUP_RETRY);
		}
#endif

		hp = cp->head;
		pthread_mutex_lock(&hp->lock);

		pthread_mutex_lock(&cp->cod_entry_mutex);
		cp->close_mod_entry = 1 << cown;
		if(cown != NKN_COD_BUF_MGR) {
			if (AO_load(&cp->client_refcnt) == 1) {
				if(!(cp->flags & NKN_CEFLAG_PUSH_INGEST)) {
					nkn_cod_ingest_detach(cp, &iptr, &cn);
					cod_detach = 1;
					client_offset = cp->client_offset;
				}
			}
			AO_fetch_and_sub1(&cp->client_refcnt);
		}
		AO_fetch_and_sub1(&cp->refcnt);
		if (AO_load(&cp->refcnt) == 0) {
			assert(cp->abuf == NULL);
			TAILQ_REMOVE(&hp->entries, cp, entrylist);
                        ZVM_ENQUEUE(cp);
			NKN_MUTEX_LOCK(&nkn_cod_entry_free_mutex);
			nkn_cod_put_entry(cp);
			NKN_MUTEX_UNLOCK(&nkn_cod_entry_free_mutex);
		}
		if (!hp->entries.tqh_first) {
			NKN_MUTEX_LOCK(&nkn_cod_head_free_mutex);
                        ZVM_ENQUEUE(hp);
			nkn_cod_head_add_free(hp);
			NKN_MUTEX_UNLOCK(&nkn_cod_head_free_mutex);
		}
		pthread_mutex_unlock(&cp->cod_entry_mutex);
		pthread_mutex_unlock(&hp->lock);
		if (cod_detach == 1 && iptr)
			if(!(cp->flags & NKN_CEFLAG_PUSH_INGEST))
				nkn_mm_resume_ingest(iptr, cn, client_offset,
						    INGEST_CLEANUP_RETRY);
		DEC_COWN_CNT(cown);

	}
}

/*
 * Get pointer to the name of the object.  Returns NULL is the COD is not
 * valid.  The returned pointer is only valid while the COD is ref counted.
 */
int nkn_cod_get_cn(nkn_cod_t cod, char **cnp, int *lenp)
{
	nkn_cod_entry_t *cp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp || !nkn_cod_is_valid(cp, cod))
		return NKN_COD_INVALID_COD;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	*cnp = cp->head->cn;
	if (lenp)
		*lenp = cp->head->cnlen;
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return 0;
}

/*
 * Get pointer to the name of the object.
 * Returns the null string if the COD is not valid.
 */
const char *nkn_cod_get_cnp(nkn_cod_t cod) 
{
	int rv;
	char *cnp;

	rv = nkn_cod_get_cn(cod, &cnp, 0);
	if (!rv) {
		return cnp;
	} else {
		return "";
	}
}

/*
 * Set the version ID for the COD.  Will be ignored if the COD is invalid
 * or has an existing version that is different from the specified one.
 */
int nkn_cod_test_and_set_version(nkn_cod_t cod, nkn_objv_t ver, int flags)
{
	nkn_cod_entry_t *cp;
	nkn_cod_head_t *hp;
	int ret;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_INVALID_COD;
	if (nkn_cod_is_valid(cp, cod)) {
		hp = cp->head;
		pthread_mutex_lock(&hp->lock);
		pthread_mutex_lock(&cp->cod_entry_mutex);
		if (cp->flags & NKN_CEFLAG_UNKNOWN) {
			memcpy(&cp->version, &ver, sizeof(nkn_objv_t));
			cp->flags &= ~NKN_CEFLAG_UNKNOWN;
			cp->head->flags &= ~(NKN_CHFLAG_STALE|NKN_CHFLAG_NEWVER);
			if (flags & NKN_COD_SET_STREAMING) {
				cp->flags |= NKN_CEFLAG_STREAMING;
				glob_cod_stream++;
			}
			ret = 0;
		} else if (!NKN_OBJV_EQ(&cp->version, &ver))
			// A new version from an origin provider can be
			// supplied if the COD entry was marked as new and
			// we do not have any existing data.  This accounts
			// for cases where the Origin is supplying dynamic
			// data (e.g. BZ 3836).
			if ((cp->flags & NKN_CEFLAG_NEWVER) &&
			    (flags & NKN_COD_SET_ORIGIN) &&
			    !(cp->abuf)) {
				memcpy(&cp->version, &ver, sizeof(nkn_objv_t));
				cp->flags &= ~NKN_CEFLAG_NEWVER;
				glob_cod_version_update++;
				ret = 0;
			} else
				ret = NKN_COD_VERSION_MISMATCH;
		else
			ret = 0;
		pthread_mutex_unlock(&cp->cod_entry_mutex);
		pthread_mutex_unlock(&hp->lock);
	} else
		ret = NKN_COD_INVALID_COD;
	return ret;
}

int nkn_cod_set_complete(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	int ret = NKN_COD_INVALID_COD;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_INVALID_COD;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (!nkn_cod_is_valid(cp, cod)) {
		glob_cod_invalid_stream++;
		ret = NKN_COD_INVALID_COD;
		// NKN_ASSERT(1);
	} else if (cp->flags & NKN_CEFLAG_STREAMING) {
		cp->flags &= ~NKN_CEFLAG_STREAMING;
		glob_cod_stream_complete++;
		ret = 0;
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return ret;
}

/*
 * Set the new version ID for the COD.
 */
void nkn_cod_set_new_version(nkn_cod_t cod, nkn_objv_t ver)
{
	nkn_cod_entry_t *cp;
	nkn_cod_head_t *hp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;
	if (nkn_cod_is_valid(cp, cod)) {
		hp = cp->head;
		pthread_mutex_lock(&hp->lock);
		pthread_mutex_lock(&cp->cod_entry_mutex);
		memcpy(&cp->head->new_version, &ver, sizeof(nkn_objv_t));
		cp->head->flags |= NKN_CHFLAG_NEWVER;
		cp->head->flags &= ~NKN_CHFLAG_STALE;
		pthread_mutex_unlock(&cp->cod_entry_mutex);
		pthread_mutex_unlock(&hp->lock);
	}
}

/*
 * Get the version ID for the COD.  Returns a null version if not set and
 * invalid if the COD is not valid.
 */
int nkn_cod_get_version(nkn_cod_t cod, nkn_objv_t *ver)
{
	nkn_cod_entry_t *cp;
	int ret = 0;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp) {
		ret = NKN_COD_INVALID_COD;
		return ret;
	}
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (!nkn_cod_is_valid(cp, cod)) {
		ret = NKN_COD_INVALID_COD;
		goto error;
	}
	if (cp->flags & NKN_CEFLAG_UNKNOWN) {
		ret = NKN_COD_INVALID_COD;
		goto error;
	}
	memcpy(ver, &cp->version, sizeof(nkn_objv_t));
error:
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return ret;
}

/*
 * Returns COD status - valid/expired/invalid
 */
nkn_cod_status_t 
nkn_cod_get_status(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	nkn_cod_head_t *hp;
	int ret = NKN_COD_STATUS_OK;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_STATUS_INVALID;
	if (!nkn_cod_is_valid(cp, cod))
		return NKN_COD_STATUS_INVALID;
	hp = cp->head;
	pthread_mutex_lock(&hp->lock);
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (cp->flags & NKN_CEFLAG_EXPIRED) {
		ret = NKN_COD_STATUS_EXPIRED;
		goto end;
	}
	if (cp->flags & NKN_CEFLAG_STREAMING) {
		ret = NKN_COD_STATUS_STREAMING;
		goto end;
	}
	if (cp->flags & NKN_CEFLAG_UNKNOWN) {
		if (cp->head->flags & NKN_CHFLAG_STALE) {
			ret = NKN_COD_STATUS_STALE;
			goto end;
		}
		else {
			ret = NKN_COD_STATUS_NULL;
			goto end;
		}
	}
end:
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	pthread_mutex_unlock(&hp->lock);
	return ret;
}

/*
 * Mark COD as expired.
 */
void
nkn_cod_set_expired(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	nkn_cod_head_t *hp;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;
	if (!nkn_cod_is_valid(cp, cod)) {
		glob_cod_invalid_expire++;
		return;
	}
	hp = cp->head;
	pthread_mutex_lock(&hp->lock);
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (cp->flags & NKN_CEFLAG_UNKNOWN) {
		glob_cod_unknown_expire++;
		// NKN_ASSERT(1);
	} else if (!(cp->flags & NKN_CEFLAG_EXPIRED)) {
		cp->flags |= NKN_CEFLAG_EXPIRED;
		glob_cod_expire++;
		if (cp == cp->head->entries.tqh_first) {
			cp->head->flags |= NKN_CHFLAG_STALE;
			cp->head->flags &= ~NKN_CHFLAG_NEWVER;
		}
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	pthread_mutex_unlock(&hp->lock);
}

// Implementation of Private API

/*
 * Attach attribute buffer to a COD.
 * Caller must ensure that the version in the attributes matches the COD.
 * Caller must ensure that the attribute buffer will be valid until a detach
 * is called.
 * Returns zero on success and error code on failure:
 *	NKN_COD_INVALID_COD:  the COD is no longer valid 
 *	NKN_COD_UNKNOWN_COD:  the COD has no version assigned
 *	NKN_COD_EXPIRED:      the COD has expired
 */
int 
nkn_cod_attr_attach(nkn_cod_t cod, nkn_buffer_t *buffer)
{
	nkn_cod_entry_t *cp;
	int ret = 0;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_INVALID_COD;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (!nkn_cod_is_valid(cp, cod))
		ret = NKN_COD_INVALID_COD;
	else if (cp->flags & NKN_CEFLAG_EXPIRED) {
		cp->abuf = buffer;
		ret = NKN_COD_EXPIRED;
	}
	else if (cp->flags & NKN_CEFLAG_UNKNOWN)
		ret = NKN_COD_UNKNOWN_COD;
	else {
		assert(cp->abuf != NULL);
		cp->abuf = buffer;
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return ret;
}

/*
 * Detach attributes from a COD.
 * Returns zero on success and error code on failure:
 *      NKN_COD_INVALID_COD:  the COD is no longer valid
 */
int 
nkn_cod_attr_detach(nkn_cod_t cod, nkn_buffer_t *buffer)
{
	nkn_cod_entry_t *cp;
	int ret = 0;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NKN_COD_INVALID_COD;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (!nkn_cod_is_valid(cp, cod))
		ret = NKN_COD_INVALID_COD;
	else {
		assert(cp->abuf == buffer);
		cp->abuf = NULL;
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return ret;
}

nkn_buffer_t *
nkn_cod_attr_lookup_insert(nkn_cod_t cod, nkn_buffer_t * inbuffer,
			   int *ret)
{
	nkn_cod_entry_t *cp;
	nkn_buffer_t *buffer = NULL;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NULL;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (nkn_cod_is_valid(cp, cod))
		buffer = cp->abuf;
	if (!buffer) {
		if (!nkn_cod_is_valid(cp, cod))
			*ret = NKN_COD_INVALID_COD;
		else if (cp->flags & NKN_CEFLAG_EXPIRED) {
			cp->abuf = inbuffer;
			*ret = NKN_COD_EXPIRED;
		}
		else if (cp->flags & NKN_CEFLAG_UNKNOWN)
			*ret = NKN_COD_UNKNOWN_COD;
		else {
			cp->abuf = inbuffer;
			*ret = 0; 
		}
	}
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return buffer;
}

/*
 * Lookup attributes from a COD.
 * Caller must get an additional ref count to hand out the reference.
 */
nkn_buffer_t *
nkn_cod_attr_lookup(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	nkn_buffer_t *buffer = NULL;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NULL;
	pthread_mutex_lock(&cp->cod_entry_mutex);
	if (nkn_cod_is_valid(cp, cod))
		buffer = cp->abuf;
	pthread_mutex_unlock(&cp->cod_entry_mutex);
	return buffer;
}

void *
nkn_cod_update_hit_offset(nkn_cod_t cod, off_t client_offset,
			  void *objp, uint32_t *am_hits,
			  uint32_t *update, const char **uri)
{
	nkn_cod_entry_t *cp;
	void *iptr = NULL;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return NULL;
	pthread_mutex_lock(&cp->cod_entry_mutex);

	if((client_offset > cp->client_offset))
		cp->client_offset  = client_offset;

	if(!(cp->flags & NKN_CEFLAG_PUSH_INGEST)) {
		nkn_cod_ingest_detach(cp, &iptr, (char **)uri);
	}

	nkn_buf_hit_ts(objp, am_hits, update);

	pthread_mutex_unlock(&cp->cod_entry_mutex);

	return iptr;

}

void
nkn_cod_update_client_offset(nkn_cod_t cod, off_t client_offset)
{
	nkn_cod_entry_t *cp;
	void *iptr = NULL;
	char *cn;

	cp = nkn_cod_to_cp(cod);
	// NKN_ASSERT(cp);
	if (!cp)
		return;
	pthread_mutex_lock(&cp->cod_entry_mutex);

	if(!(cp->flags & NKN_CEFLAG_PUSH_INGEST)) {
		if((client_offset > cp->client_offset))
			cp->client_offset  = client_offset;

		nkn_cod_ingest_detach(cp, &iptr, &cn);
	}

	pthread_mutex_unlock(&cp->cod_entry_mutex);

	if (iptr) {
		nkn_mm_resume_ingest(iptr, cn, client_offset,
					 INGEST_NO_CLEANUP);
	}
}

off_t
nkn_cod_get_client_offset(nkn_cod_t cod)
{
	nkn_cod_entry_t *cp;
	off_t ret = 0;

	cp = nkn_cod_to_cp(cod);

	if (cp) {
		pthread_mutex_lock(&cp->cod_entry_mutex);
		ret = cp->client_offset;
		pthread_mutex_unlock(&cp->cod_entry_mutex);
	}

	return ret;
}

int
nkn_cod_get_next_entry(nkn_cod_t cod, char *prefix, int plen,
		       nkn_cod_t *next, char **uri, int *urilen,
		       nkn_cod_owner_t cown)
{
	nkn_cod_entry_t *cp = NULL;
	nkn_cod_head_t *hp;
	u_int32_t c_index = cod & COD_INDEX_MASK;
	NKN_ASSERT(cown < NKN_COD_MAX_MGR);

	if (cod != NKN_COD_NULL)
		c_index++;
	for ( ; c_index < glob_cod_entries; c_index++) {
		cp = &nkn_cod_entries[c_index];
		// We don't check validity since we are using this only as an 
		// index
		if (!AO_load(&cp->refcnt))
			continue;
		hp = cp->head;
		if (!hp)
			continue;
		pthread_mutex_lock(&hp->lock);
		pthread_mutex_lock(&cp->cod_entry_mutex);
		// Recheck refcnt under lock
		if (!AO_load(&cp->refcnt) || 
		    (cp->flags & NKN_CEFLAG_UNKNOWN) ||
		    !cp->abuf ||
		    (cp->flags & NKN_CEFLAG_EXPIRED)) {
			pthread_mutex_unlock(&cp->cod_entry_mutex);
			pthread_mutex_unlock(&hp->lock);
			continue;
		}
		if (prefix && strncmp(prefix, cp->head->cn, plen)) {
			pthread_mutex_unlock(&cp->cod_entry_mutex);
			pthread_mutex_unlock(&hp->lock);
			continue;
		}
		AO_fetch_and_add1(&cp->refcnt);
		if(cown != NKN_COD_BUF_MGR)
		    AO_fetch_and_add1(&cp->client_refcnt);
		cp->open_mod_entry = 1 << cown;
		*next = CP_TO_COD(cp);
		if (uri)
		    *uri = hp->cn;
		if (urilen)
		    *urilen = hp->cnlen;
		pthread_mutex_unlock(&cp->cod_entry_mutex);
		pthread_mutex_unlock(&hp->lock);
		INC_COWN_CNT(cown);
		return 0;
	}
	*next = NKN_COD_NULL;
	return 0;
}

/* 
 * Test/debug routine for enumerting the valid COD entries.  A similar
 * schemem can be used by the CLI (mgmt code in nvsd) to list RAM cache
 * entries.
 */
static void nkn_cod_list_entries(char *pfx) __attribute__ ((unused));
static void
nkn_cod_list_entries(char *pfx)
{
	nkn_cod_t cod, next;
	int ret, plen = 0;

	cod = NKN_COD_NULL;
	next = NKN_COD_NULL;
	if (pfx)
		plen = strlen(pfx);	
	do {
		ret = nkn_cod_get_next_entry(cod, pfx, plen, &next, NULL, NULL, NKN_COD_COD_MGR);
		if (ret)
			break;
		printf("%s\n", nkn_cod_get_cnp(next));
		nkn_cod_close(next, NKN_COD_COD_MGR);
		cod = next;
	} while (next != NKN_COD_NULL);
}

#ifdef COD_UNIT_TEST
void
nkn_cod_unit_test(void)
{
	nkn_cod_t cod;
	char *cn;
	int len, ret;
	nkn_cod_status_t status;
	nkn_objv_t version;

	cod = nkn_cod_open("/abc/123.flv", NKN_COD_COD_MGR);
	ret = nkn_cod_get_cn(cod, &cn, &len);
	status = nkn_cod_get_status(cod);
	ret = nkn_cod_get_version(cod, &version);	
	memcpy(&version, "0000000000000023", sizeof(nkn_objv_t));
	ret = nkn_cod_test_and_set_version(cod, version, 0);
	memcpy(&version, "0000000000000024", sizeof(nkn_objv_t));
	ret = nkn_cod_test_and_set_version(cod, version, 0);
	ret = nkn_cod_get_version(cod, &version);	
	status = nkn_cod_get_status(cod);
	nkn_cod_set_expired(cod);
	status = nkn_cod_get_status(cod);
	nkn_cod_close(cod, NKN_COD_COD_MGR);
	status = nkn_cod_get_status(cod);
}
#endif

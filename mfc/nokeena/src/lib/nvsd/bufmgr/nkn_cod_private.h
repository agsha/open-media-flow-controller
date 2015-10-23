#ifndef COD_PRIVATE_H
#define COD_PRIVATE_H
#include "nkn_defs.h"
#include "nkn_cod.h"
#include "cache_mgr.h"
#include "nkn_hash.h"

/* 
 * COD private API that allows coupling between a COD and the corresponding
 * attributes.
 *
 * A cached attrbute buffer is always tied to a COD entry and holds a
 * reference count to it.  A COD entry may not have an attribute buffer if
 * one does not exist in the cache.
 *
 * Locking hierarchy.  The lock acquisition order is buffer followed by COD.
 *
 * The COD-attribute couple is protected by the buffer mgr lock (not by the
 * the COD layer).
 */
struct nkn_cod_head;			// forward declaration
struct nkn_cod_entry;			// forward declaration
#define NKN_CEFLAG_EXPIRED	1		// Version has expired
#define NKN_CEFLAG_UNKNOWN	2		// Version is unknown
#define NKN_CEFLAG_NEWVER	4		// Version is new from origin
#define NKN_CEFLAG_STREAMING	8		// Object is streaming

#define NKN_INGEST_TRIGGER_OFFSET (2*1024*1024)
/*
 * The COD entry represents a specific version of a cached object.
 */
typedef struct nkn_cod_entry {
	pthread_mutex_t cod_entry_mutex;
	nkn_objv_t version;
	TAILQ_ENTRY(nkn_cod_entry) entrylist;	// List of CODs with same CN
	AO_t revision;			// detects stale CODs
	AO_t refcnt;
	uint64_t index;
	nkn_buffer_t *abuf;			// associated attribute buffer
	struct nkn_cod_head *head;
	off_t client_offset;
	off_t ingest_offset;
	void *iptr;
	AO_t client_refcnt;
	uint16_t open_mod_entry;
	uint16_t close_mod_entry;
	uint8_t	 flags;
} nkn_cod_entry_t;

TAILQ_HEAD(nkn_cod_entry_list, nkn_cod_entry);
TAILQ_HEAD(nkn_cod_head_list, nkn_cod_head);

#define NKN_CHFLAG_HASH		1		// Head is in the hash table
#define NKN_CHFLAG_FREE		2		// Head is in the free list
#define NKN_CHFLAG_STALE	4		// Head is stale.  Last known
						// entry was expired
#define NKN_CHFLAG_NEWVER	8		// New version available

#define MAX_COD_HASH		200
#define MAX_COD_URI_HASH	4
#define COD_URI_HASH_DEFAULT	100

/*
 * The COD head represents a unique entry per cache name.  It is organized
 * in a hash table for quick lookup and insertion.  It points to a list of
 * active COD entries, one per version.
 */
typedef struct nkn_cod_head {
	NKN_CHAIN_ENTRY __entry;
	pthread_mutex_t lock;
	struct nkn_cod_entry_list entries;	// free list pointers
	TAILQ_ENTRY(nkn_cod_head) headlist;	// List of COD heads
	nkn_objv_t new_version;			// new version id 
	char *cn;
	uint16_t cnlen, flags;
} nkn_cod_head_t;

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
int nkn_cod_attr_attach(nkn_cod_t cod, nkn_buffer_t *buffer);


void nkn_cod_lock(nkn_cod_t cod);

void nkn_cod_unlock(nkn_cod_t cod);

/*
 * Detach attributes from a COD.
 * Returns zero on success and error code on failure:
 *      NKN_COD_INVALID_COD:  the COD is no longer valid
 */
int nkn_cod_attr_detach(nkn_cod_t cod, nkn_buffer_t *buffer);

/*
 * Lookup attributes from a COD.
 * Caller must get an additional ref count to hand out the reference.
 */
nkn_buffer_t *nkn_cod_attr_lookup_insert(nkn_cod_t cod,
					 nkn_buffer_t *bp,
					 int *retcode);
#endif

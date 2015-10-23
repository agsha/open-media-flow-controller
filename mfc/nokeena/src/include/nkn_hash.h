#ifndef NKN_HASH_H
#define NKN_HASH_H

#include <stdint.h>
#include <sys/time.h>
#include <assert.h>

#ifndef HASH_UNITTEST
#include "nkn_memalloc.h"
#endif

typedef int (*NEqualFunc)(const void *key1,const void *key2);
typedef uint64_t (*NHashFunc)(const void *key);
typedef uint64_t (*NHashFuncArg)(const void *key, size_t len);
typedef int (*NHRFunc) (void *key, void *value, void *userdata);
typedef void (*NHFunc) (void *key, void *value, void *userdata);

struct sNHashTable {
    void		**key;
    void		**value;
    uint64_t		*state;
    uint64_t		tot_buckets;
    uint64_t		buckets_occupied;
    uint64_t		size;
    uint64_t		buckets_threshold; // computed one time & faster lookup
    NEqualFunc		keyequalfunc;
    NHashFunc		hashfunc;
    uint64_t            resize_passed_cnt;
#ifdef HASH_DEBUG
    uint64_t            resize_entered_cnt;
    uint64_t            max_time;
    uint64_t            inserted;
    uint64_t            removed;
    uint64_t            max_inserted;
    uint64_t            max_removed;
    uint64_t            avg_time;
#endif
#ifndef HASH_UNITTEST
    nkn_obj_type_t	obj_type;
#endif
};

extern uint64_t glob_resize_hash_cnt;

typedef struct sNHashTable NHashTable;


struct sNHTableIter {
    NHashTable *tbl;
    uint64_t    position;
};

typedef struct sNHTableIter NHTableIter;


// chained hashing declaration
struct sNCHTableNode {		    
    void *key;			    
    struct sNCHTableNode *next;	   
};
   
typedef struct sNCHTableNode sNCHTableNode_t;

#define NKN_CHAIN_ENTRY sNCHTableNode_t
#define NKN_CHAINFIELD_OFFSET(type, field)    ((unsigned long) &(((type *) 0)->field)) 

struct sNCHashTable {
    sNCHTableNode_t	**nodes;
    uint64_t		tot_buckets;
    uint64_t		size;
    uint64_t		offset;
    NEqualFunc		keyequalfunc;
    NHashFuncArg	hashfunc;
    uint64_t            resize_passed_cnt;
#ifndef HASH_UNITTEST
    nkn_obj_type_t	obj_type;
#endif
};


typedef struct sNCHashTable NCHashTable;

//MAX supported prime - used for hashtable sizes
#define MAX_PRIME_HASH  26
#define START_PRIME	53
static const uint64_t NHashPrime[MAX_PRIME_HASH] = {
53,
97,
193,
389,
769,
1543,
3079,
6151,
12289,
24593,
49157,
98317,
196613,
393241,
786433,
1572869,
3145739,
6291469,
12582917,
25165843,
50331653,
100663319,
201326611,
402653189,
805306457,
1610612741
};




#ifndef FALSE
#define FALSE    (0)
#endif
#ifndef TRUE
#define TRUE    (!FALSE)
#endif


#define UNUSED_ARGUMENT(x) (void)x

#define NCHASH_TABLE_RESIZE(tbl)                                        \
    do {                                                                \
        if (tbl->tot_buckets <=  (tbl->size << 1))                      \
            nchash_resize(tbl);                                         \
    } while (0);

#define OFFSET_NODE(node, offset)                       \
        ((sNCHTableNode_t *)((char *)node + offset))

//open addressing apis

#ifndef HASH_UNITTEST
NHashTable *nhash_table_new(NHashFunc hashfunc,	NEqualFunc keyequalfunc,
			    uint64_t init_size, nkn_obj_type_t type);
#else
NHashTable *nhash_table_new(NHashFunc hashfunc,	NEqualFunc keyequalfunc,
			    uint64_t init_size);
#endif

void	*nhash_lookup(NHashTable *tbl, uint64_t keyhash, const void *key);
int	nhash_lookup_extented(NHashTable *tbl, uint64_t keyhash, const void *key,
			      void **orig_key, void **value);
int	nhash_remove(NHashTable *tbl, uint64_t keyhash, void *key);
void	nhash_insert(NHashTable *tbl, uint64_t keyhash, void *key, void *value);
void	nhash_foreach(NHashTable *tbl, NHFunc iterfunc, void *userdata);
uint64_t nhash_foreach_remove(NHashTable *tbl, NHRFunc iterfunc, void *userdata);
void	*nhash_find(NHashTable *tbl, NHRFunc iterfunc, void *userdata);
uint64_t nhash_size(NHashTable *tbl);
void	nhash_iter_init(NHTableIter *iter, NHashTable *tbl);
int	nhash_iter_next(NHTableIter *iter, void **key, void **value);
void	nhash_iter_remove(NHTableIter *iter);
void	nhash_destroy(NHashTable *tbl);
int32_t nhash_find_hash_bucket(uint64_t size);

//chained hash functions
//CAUTION: NKN_CHAIN_ENTRY should be declared at beginning of value structure
//CAUTION: user defined type structure are only supported.    
#ifndef HASH_UNITTEST
NCHashTable* nchash_table_new(NHashFuncArg hashfunc, NEqualFunc keyequalfunc,
			      uint64_t init_size, uint64_t offset,
			      nkn_obj_type_t type);
#else
NCHashTable* nchash_table_new(NHashFuncArg hashfunc, NEqualFunc keyequalfunc,
			      uint64_t init_size, uint64_t offset);
#endif

// value is returned based on the key/keyhash passed
static inline void* nchash_lookup(NCHashTable *tbl, uint64_t keyhash, const void *key);

// original key/value is returned based on the key/keyhash passed
static inline int nchash_lookup_extended(NCHashTable *tbl, uint64_t keyhash,
				  const void *key, void **orig_key,
				  void **value);

// hash entry deleted for key/keyhash passed
static inline int nchash_remove(NCHashTable *tbl, uint64_t keyhash, void *key);

// hash entry created for key/keyhash passed.
// if lookup/insert are under same lock then use this for fast insert
static inline void nchash_insert_new(NCHashTable *tbl, uint64_t keyhash, void *key,
			      void *value);

// hash entry created for key/keyhash passed.
static inline void nchash_insert(NCHashTable *tbl, uint64_t keyhash, void *key,
			  void *value);

// same as nchash_insert. passed key is used for replacement.
static inline void nhash_replace(NCHashTable *tbl, uint64_t keyhash, void *key,
			  void *value);

static inline void nchash_resize(NCHashTable *tbl);

//NOTE: use _off0 function only if hash entry at beginning of structure
// _off0 will not do offset computation -- This gives performance boost
// value is returned based on the key/keyhash passed
static inline void* nchash_lookup_off0(NCHashTable *tbl, uint64_t keyhash, const void *key);

// original key/value is returned based on the key/keyhash passed
static inline int nchash_lookup_extended_off0(NCHashTable *tbl, uint64_t keyhash,
				       const void *key, void **orig_key,
				       void **value);

// hash entry deleted for key/keyhash passed
static inline int nchash_remove_off0(NCHashTable *tbl, uint64_t keyhash, void *key);

// hash entry created for key/keyhash passed.
// if lookup/insert are under same lock then use this for fast insert
static inline void nchash_insert_new_off0(NCHashTable *tbl, uint64_t keyhash, void *key,
				   void *value);

// hash entry created for key/keyhash passed.
static inline void nchash_insert_off0(NCHashTable *tbl, uint64_t keyhash, void *key,
			       void *value);

// same as nchash_insert. passed key is used for replacement.
static inline void nhash_replace_off0(NCHashTable *tbl, uint64_t keyhash, void *key,
			       void *value);


/* Hash functions */
uint64_t n_uint64_hash(const void *v);
uint64_t n_uint64_hash_arg(const void *v, size_t length);
uint64_t n_dtois_hash(const void *v);
uint64_t n_dtois_hash_arg(const void *v, size_t length);
int      n_uint64_equal(const void *v1, const void *v2);
int      n_dtois_equal(const void *v1, const void *v2);
int      n_str_equal(const void *v1, const void *v2);
uint64_t n_str_hash(const void *v);
uint64_t n_str_hash_wlen(const void *v, size_t length);

//bob string hash function
uint64_t hashlittle( const void *key, size_t length);


// Define the inline functions.

static inline sNCHTableNode_t**
nchash_lookup_internal_off0(NCHashTable *tbl,
			    uint64_t keyhash,
			    const void *key)
{
    sNCHTableNode_t **node = &tbl->nodes[keyhash % tbl->tot_buckets];

    if (tbl->keyequalfunc)
	while (*node && !(*tbl->keyequalfunc) ((*node)->key, key))
	    node = &(*node)->next;
    else
	while (*node && ((*node)->key != key))
	    node = &(*node)->next;

    return node;
}

static inline sNCHTableNode_t**
nchash_lookup_interret_off0(NCHashTable *tbl,
		   uint64_t keyhash,
		   const void *key,
		   sNCHTableNode_t ***retnode)
{
    sNCHTableNode_t **node = &tbl->nodes[keyhash % tbl->tot_buckets];
    *retnode = node;

    if (tbl->keyequalfunc)
	while (*node && !(*tbl->keyequalfunc) ((*node)->key, key)) {
	    *retnode = node;
	    node = &(*node)->next;
	}
    else
	while (*node && ((*node)->key != key)) {
	    *retnode = node;
	    node = &(*node)->next;
	}

    return node;
}

//Made this static
static inline void*
nchash_lookup_off0(NCHashTable *tbl,
		   uint64_t keyhash,
		   const void *key)
{
    return *nchash_lookup_internal_off0(tbl, keyhash, key);
}

//Made this static
static inline int
nchash_lookup_extended_off0(NCHashTable *tbl,
			    uint64_t keyhash,
			    const void *key,
			    void **orig_key,
			    void **value)
{

    sNCHTableNode_t *node = *nchash_lookup_internal_off0(tbl, keyhash, key);
    if (node) {
	if (orig_key)
	    *orig_key = node->key;
	if (value)
	    *value = node;
	return TRUE;
    } else
	return FALSE;
}


//Made this static
static inline int
nchash_remove_off0(NCHashTable *tbl,
	      uint64_t keyhash,
	      void *key)
{
    sNCHTableNode_t **node = nchash_lookup_internal_off0(tbl, keyhash, key);
    if (*node) {
	--tbl->size;
	*node = (*node)->next;
	//NCHASH_TABLE_RESIZE(tbl);
	return TRUE;
    }
    return FALSE;
}


//Made this static
static inline void
nchash_insert_new_off0(NCHashTable *tbl,
             uint64_t keyhash,
             void *key,
             void *value)
{
    sNCHTableNode_t **node = nchash_lookup_internal_off0(tbl, keyhash, key);
    uint64_t offset;
    if (*node == NULL) {
	++tbl->size;
	offset = tbl->offset;
	*node = value;
	(OFFSET_NODE(*node, offset))->key = key;
	(OFFSET_NODE(*node, offset))->next = NULL;
	NCHASH_TABLE_RESIZE(tbl);
    } else
	assert(0);
}

//Made this static
static inline void
nchash_insert_off0(NCHashTable *tbl,
		   uint64_t keyhash,
		   void *key,
		   void *value)
{
    sNCHTableNode_t **retnode;
    sNCHTableNode_t **node = nchash_lookup_interret_off0(tbl, keyhash, key, &retnode);
    if (*node) {
	((sNCHTableNode_t *)value)->next = (*node)->next;
	*retnode = value;
    } else {
	*node = value;
	(*node)->key = key;
	(*node)->next = NULL;
	++tbl->size;
	NCHASH_TABLE_RESIZE(tbl);
    }
}

//Made this static
static inline void
nhash_replace_off0(NCHashTable *tbl,
		   uint64_t keyhash,
		   void *key,
		   void *value)
{
    sNCHTableNode_t **retnode;
    sNCHTableNode_t **node = nchash_lookup_interret_off0(tbl, keyhash, key, &retnode);
    if (*node) {
	((sNCHTableNode_t *)value)->next = (*node)->next;
	((sNCHTableNode_t *)value)->key = key;
	*retnode = value;
    } else {
	*node = value;
	(*node)->key = key;
	(*node)->next = NULL;
	++tbl->size;
	NCHASH_TABLE_RESIZE(tbl);
    }
}

static inline sNCHTableNode_t**
nchash_lookup_internal(NCHashTable *tbl,
		   uint64_t keyhash,
		   const void *key)
{
    sNCHTableNode_t **node = &tbl->nodes[keyhash % tbl->tot_buckets];
    uint64_t offset = tbl->offset;

    if (tbl->keyequalfunc)
	while (*node && !(*tbl->keyequalfunc) ((OFFSET_NODE(*node, offset))->key, key))
	    node = &(OFFSET_NODE(*node, offset))->next;
    else
	while (*node && (OFFSET_NODE(*node, offset))->key != key)
	    node = &(OFFSET_NODE(*node, offset))->next;

    return node;
}

static inline sNCHTableNode_t**
nchash_lookup_interret(NCHashTable *tbl,
		   uint64_t keyhash,
		   const void *key,
		   sNCHTableNode_t ***retnode)
{
    sNCHTableNode_t **node = &tbl->nodes[keyhash % tbl->tot_buckets];
    uint64_t offset = tbl->offset;
    *retnode = node;

    if (tbl->keyequalfunc)
	while (*node && !(*tbl->keyequalfunc) ((OFFSET_NODE(*node, offset))->key, key)) {
	    *retnode = node;
	    node = &(OFFSET_NODE(*node, offset))->next;
	}
    else
	while (*node && (OFFSET_NODE(*node, offset))->key != key) {
	    *retnode = node;
	    node = &(OFFSET_NODE(*node, offset))->next;
	}

    return node;
}

//Made this static
static inline void*
nchash_lookup(NCHashTable *tbl,
	      uint64_t keyhash,
	      const void *key)
{
    return *nchash_lookup_internal(tbl, keyhash, key);
}

//Made this static
static inline int
nchash_lookup_extended(NCHashTable *tbl,
		   uint64_t keyhash,
		   const void *key,
		   void **orig_key,
		   void **value)
{

    sNCHTableNode_t *node = *nchash_lookup_internal(tbl, keyhash, key);
    uint64_t offset;
    if (node) {
	offset = tbl->offset;
	if (orig_key)
	    *orig_key = OFFSET_NODE(node, offset)->key;
	if (value)
	    *value = node;
	return TRUE;
    } else
	return FALSE;
}


//Made this static
static inline int
nchash_remove(NCHashTable *tbl,
	      uint64_t keyhash,
	      void *key)
{
    sNCHTableNode_t **node = nchash_lookup_internal(tbl, keyhash, key);
    uint64_t offset;
    if (*node) {
	--tbl->size;
	offset = tbl->offset;
	*node = (OFFSET_NODE(*node, offset))->next;
	//NCHASH_TABLE_RESIZE(tbl);
	return TRUE;
    }
    return FALSE;
}


//Made this static
static inline void
nchash_insert_new(NCHashTable *tbl,
             uint64_t keyhash,
             void *key,
             void *value)
{
    sNCHTableNode_t **node = nchash_lookup_internal(tbl, keyhash, key);
    uint64_t offset;
    if (*node == NULL) {
	++tbl->size;
	offset = tbl->offset;
	*node = value;
	(OFFSET_NODE(*node, offset))->key = key;
	(OFFSET_NODE(*node, offset))->next = NULL;
	NCHASH_TABLE_RESIZE(tbl);
    } else
	assert(0);
}

//Made this static
static inline void
nchash_insert(NCHashTable *tbl,
             uint64_t keyhash,
             void *key,
             void *value)
{
    sNCHTableNode_t **retnode;
    uint64_t offset = tbl->offset;
    sNCHTableNode_t **node = nchash_lookup_interret(tbl, keyhash, key, &retnode);
    if (*node) {
	(OFFSET_NODE(value, offset))->next = (OFFSET_NODE(*node, offset))->next;
	*retnode = value;
    } else {
	*node = value;
	(OFFSET_NODE(*node, offset))->key = key;
	(OFFSET_NODE(*node, offset))->next = NULL;
	++tbl->size;
	NCHASH_TABLE_RESIZE(tbl);
    }
}

//Made this static
static inline void
nhash_replace(NCHashTable *tbl,
              uint64_t keyhash,
              void *key,
              void *value)
{
    sNCHTableNode_t **retnode;
    uint64_t offset = tbl->offset;
    sNCHTableNode_t **node = nchash_lookup_interret(tbl, keyhash, key, &retnode);
    if (*node) {
	(OFFSET_NODE(value, offset))->next = (OFFSET_NODE(*node, offset))->next;
	(OFFSET_NODE(value, offset))->key = key;
	*retnode = value;
    } else {
	*node = value;
	(OFFSET_NODE(*node, offset))->key = key;
	(OFFSET_NODE(*node, offset))->next = NULL;
	++tbl->size;
	NCHASH_TABLE_RESIZE(tbl);
    }
}


static inline void
nchash_resize(NCHashTable *tbl)
{
    sNCHTableNode_t **new_node;
    sNCHTableNode_t *node;
    sNCHTableNode_t *next;
    uint64_t new_size, i, hash, offset;
    int32_t bucket = nhash_find_hash_bucket(tbl->tot_buckets << 1);
    if (bucket == -1)
	assert(0);
    new_size = NHashPrime[bucket];
#ifndef HASH_UNITTEST
    new_node = nkn_calloc_type(new_size, sizeof(sNCHTableNode_t *), tbl->obj_type);
#else
    new_node = calloc(new_size, sizeof(sNCHTableNode_t *));
#endif
    offset = tbl->offset;
    glob_resize_hash_cnt ++;
    for (i =0; i < tbl->tot_buckets ; ++i) {
	for( node = tbl->nodes[i] ; node ; node = next) {
	    next = OFFSET_NODE(node, offset)->next;
	    hash = (*tbl->hashfunc) (OFFSET_NODE(node, offset)->key, 0) % new_size;
	    OFFSET_NODE(node, offset)->next = new_node[hash];
	    new_node[hash] = node;
	}
    }
    free(tbl->nodes);
    tbl->nodes = new_node;
    tbl->tot_buckets = new_size;
    tbl->resize_passed_cnt++;
}

// based on user func return status hash entry will be returned
// userdata will be passed as one of the user func arguement
// key will be returned if match is there..
static inline void*
nchash_find_match(NCHashTable *tbl,
		  NHRFunc func,
		  void *userdata,
		  void **key)
{
    
    uint64_t i;
    sNCHTableNode_t *node;
    uint64_t offset = tbl->offset;
    for (i = 0; i < tbl->tot_buckets ; ++i) {
	for ( node = tbl->nodes[i] ; node; node = OFFSET_NODE(node, offset)->next) {
	    if ((*func)(OFFSET_NODE(node, offset)->key, node, userdata)) {
		*key = OFFSET_NODE(node, offset)->key;
		return node;
	    }
	}
    }
    return NULL;
}


// user func will be called for all hash entries
// userdata will be passed as one of the user func arguement
// caution: Don't delete hash entry in nchash_foreach
static inline void
nchash_foreach(NCHashTable *tbl,
	       NHFunc func,
	       void *userdata)
{
    
    uint64_t i;
    sNCHTableNode_t *node, *next;
    uint64_t offset = tbl->offset;
    for (i = 0; i < tbl->tot_buckets ; ++i) {
	for ( node = tbl->nodes[i] ; node; node = next) {
	    next = OFFSET_NODE(node, offset)->next;
	    (*func)(OFFSET_NODE(node, offset)->key, node, userdata);
	}
    }
}

// return hash table entries count
static inline uint64_t
nchash_size(NCHashTable *tbl)
{
    return tbl->size;
}

// destroy hashtable
// NOTE: all hash entries should be removed before calling this api.
// all hash entries can be removed by calling
// nchash_find_match [ func always returning success ]
// caution: Don't delete hash entry in nchash_foreach
static inline void
nchash_destroy(NCHashTable *tbl)
{
    uint64_t i;
    for (i =0; i< tbl->tot_buckets; ++i) {
	if (tbl->nodes[i])
	    assert(0);
    }
    free(tbl->nodes);
    free(tbl);
}


#endif	/* NKN_HASH_H */

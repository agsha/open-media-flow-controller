
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "nkn_hash.h"

//
uint64_t glob_resize_hash_cnt = 0;

/*
 * find the corresponding bucket based on the input size
 */
int32_t
nhash_find_hash_bucket(uint64_t size)
{
    int32_t b_index = 0;

    if (size > NHashPrime[MAX_PRIME_HASH - 1])
	return -1;
    while (size > NHashPrime[b_index++])
	;
    return (b_index - 1);
}


/*
 * to create new hash table
 */
#ifndef HASH_UNITTEST
NCHashTable*
nchash_table_new(NHashFuncArg hashfunc,
		 NEqualFunc keyequalfunc,
		 uint64_t init_size,
		 uint64_t offset,
		 nkn_obj_type_t type)
#else
NCHashTable*
nchash_table_new(NHashFuncArg hashfunc,
		 NEqualFunc keyequalfunc,
		 uint64_t init_size,
		 uint64_t offset)
#endif
{
    NCHashTable *temp;
#ifndef HASH_UNITTEST
    temp = nkn_calloc_type(1, sizeof(NCHashTable), type);
#else
    temp = calloc(1, sizeof(NCHashTable));
#endif
    if (temp) {
	temp->keyequalfunc = keyequalfunc;
	temp->hashfunc = hashfunc;
	if (!init_size)
	    init_size =  START_PRIME;
	int32_t bucket = nhash_find_hash_bucket(init_size);
	if (bucket == -1) {
	    free(temp);
	    return NULL;
	}
#ifndef HASH_UNITTEST
	temp->obj_type = type;
	temp->nodes = nkn_calloc_type(NHashPrime[bucket],
			sizeof(sNCHTableNode_t *), type);
#else
	temp->nodes = calloc(NHashPrime[bucket],
			sizeof(sNCHTableNode_t *));
#endif
	if (!temp->nodes) {
	    free(temp);
	    return NULL;
	} else
	    temp->tot_buckets = NHashPrime[bucket];
	temp->offset = offset;
    }
    // if failed null will be returned
    return temp;
}


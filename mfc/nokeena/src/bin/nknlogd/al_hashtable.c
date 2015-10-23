#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bail.h"
#define HASH_UNITTEST
#include "nkn_hash.h"

#ifndef __NKN_PRIVATE_HASH
#define __NKN_PRIVATE_HASH

// open addressing bucket threshold limit
#define HASH_THRESHOLD(size) (uint64_t)((size * 3) >> 2)

// possible states for bucket
#define STATE_OCCUPIED          0UL
#define STATE_EMPTY             1UL
#define STATE_TOMB              2UL
#define STATE_EMPTY_TOMB        3UL

//state index specifies the corresponding hash bucket
//state of a bucket requires 2 bits
#define STATE_INDEX(index) \
        ((index & 0x1FUL) << 1)
//to check the corresponding bucket state
//MAX states available in 64 bit is 32 so shift by 5 is used
#define CHECK_STATE(state, index) \
        ((state[index >> 5] >> STATE_INDEX(index)) & 3UL)

//set state for a hash bucket
#define SET_STATE(state, index, type) \
        state[index >> 5] |= (type << (STATE_INDEX(index)))

//unset/clear state for a hash bucket
#define UNSET_STATE(state, index, type) \
        state[index >> 5] &= ~(type << (STATE_INDEX(index)))

//MAX supported prime - used for hashtable sizes
#define MAX_PRIME_HASH 26

// macro to compute state size
#define BUCKET_STATE_SIZE(size) \
	((bucket_size >> 5) + 1)

#endif

/*
 * source hash prime data taken from
 *   http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 */

void isdatanull(void* data, const char* func_name);
void isdatanull(void* data, const char* func_name)
{
 if(data == NULL){
        complain_error_msg(1, "data is NULL! from %s", func_name);
        exit(EXIT_FAILURE);
    }

}

uint64_t glob_nhash_bucket_find_fail;
uint64_t glob_nhash_keyrealloc_fail_1;
uint64_t glob_nhash_valuerealloc_fail_1;
uint64_t glob_nhash_staterealloc_fail;
uint64_t glob_nhash_keyrealloc_fail_2;
uint64_t glob_nhash_valuerealloc_fail_2;

void timeval_subtract (uint64_t *result,  struct timeval * x,  struct timeval * y);
void timeval_subtract (uint64_t *result,  struct timeval * x,  struct timeval * y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  *result = (x->tv_sec - y->tv_sec) * 1e6 + ( x->tv_usec - y->tv_usec);

}

/*
 * find the corresponding bucket based on the input size
 */
int32_t
nhash_find_hash_bucket(uint64_t size)
{
    int32_t index = 0;

    if (size > NHashPrime[MAX_PRIME_HASH - 1])
	return -1;
    while (size > NHashPrime[index++])
	;
    return (index - 1);
}

/*
 * used to resize the hash table based on the bucket size
 */
static int
nhash_table_resize(NHashTable *tbl,
		   uint64_t bucket_size)
{
    uint64_t *state;
    uint64_t index, new_index, oper_inter, keyhash;
    void *key, *value, *temp;
    int32_t bucket;
#ifdef HASH_DEBUG
    struct timeval start, end;
    uint64_t result;
    tbl->resize_entered_cnt++;
    gettimeofday(&start, NULL);
#endif
    bucket = nhash_find_hash_bucket(bucket_size);
    if (bucket == -1) {
	glob_nhash_bucket_find_fail++;
	return -1;
    }
    bucket_size = NHashPrime[bucket];
    if (tbl->size < HASH_THRESHOLD(bucket_size)) {
	if (tbl->tot_buckets < bucket_size) {
            // realloc keys and values
#ifndef HASH_UNITTEST
	    tbl->key = nkn_realloc_type(tbl->key, bucket_size * sizeof(void *),
					tbl->obj_type);
#else
	    tbl->key = realloc(tbl->key, bucket_size * sizeof(void *));
#endif
	    if (tbl->key == NULL) {
		glob_nhash_keyrealloc_fail_1++;
		return -1;
	    }
#ifndef HASH_UNITTEST
	    tbl->value = nkn_realloc_type(tbl->value,
					  bucket_size * sizeof(void *),
					  tbl->obj_type);
#else
	    tbl->value = realloc(tbl->value, bucket_size * sizeof(void *));
#endif
	    if (tbl->value == NULL) {
		glob_nhash_valuerealloc_fail_1++;
		return -1;
	    }
	}
        // calloc state and assign to empty state value
#ifndef HASH_UNITTEST
	state = (uint64_t *)nkn_calloc_type(BUCKET_STATE_SIZE(bucket_size),
					    sizeof(uint64_t), tbl->obj_type);
#else
	state = (uint64_t *)calloc(BUCKET_STATE_SIZE(bucket_size),
				   sizeof(uint64_t));
#endif
	if (state == NULL) {
	    glob_nhash_staterealloc_fail++;
	    return -1;
	}
	memset(state, 0x5555,
	       (BUCKET_STATE_SIZE(bucket_size) * sizeof(uint64_t)));
	index = 0;
	while (index < tbl->tot_buckets) {
	    if (CHECK_STATE(tbl->state, index) == STATE_OCCUPIED) {
                SET_STATE(tbl->state, index, STATE_TOMB);
		key = tbl->key[index];
                value = tbl->value[index];
		for (;;) {  // search based on index
		    keyhash = (*tbl->hashfunc)(key);
		    new_index = keyhash % bucket_size;
		    oper_inter = keyhash % (bucket_size - 1);
		    ++oper_inter;
		    //find nonempty bucket
		    while ( !(CHECK_STATE(state, new_index) & STATE_EMPTY)) {
			new_index += oper_inter;
			if (new_index >= bucket_size) {
			    new_index -= bucket_size;
			}
		    }
		    UNSET_STATE(state, new_index, STATE_EMPTY);
		    if ((new_index < tbl->tot_buckets) &&
			(CHECK_STATE(tbl->state, new_index) == STATE_OCCUPIED)){

			temp = tbl->key[new_index];
			tbl->key[new_index] = key;
			key = temp;
			temp = tbl->value[new_index];
			tbl->value[new_index] = value;
			value = temp;
			SET_STATE(tbl->state, new_index, STATE_TOMB);
		    } else {
			tbl->key[new_index] = key;
			tbl->value[new_index] = value;
			break;
		    }
		}
	    }
	    ++index;
	}
	free(tbl->state);
	tbl->state = state;
	tbl->buckets_occupied = tbl->size;
	// reduce table size
	if (tbl->tot_buckets > bucket_size) {
#ifndef HASH_UNITTEST
	    tbl->key = nkn_realloc_type(tbl->key, bucket_size * sizeof(void *),
					tbl->obj_type);
#else
	    tbl->key = realloc(tbl->key, bucket_size * sizeof(void *));
#endif
	    if (tbl->key == NULL) {
		glob_nhash_keyrealloc_fail_2++;
		return -1;
	    }
#ifndef HASH_UNITTEST
	    tbl->value = nkn_realloc_type(tbl->value,
					  bucket_size * sizeof(void *),
					  tbl->obj_type);
#else
	    tbl->value = realloc(tbl->value, bucket_size * sizeof(void *));
#endif
	    if (tbl->value == NULL) {
		glob_nhash_valuerealloc_fail_2++;
		return -1;
	    }
	}
	tbl->tot_buckets = bucket_size;
	// currently 0.75 is used as open addressing hash threshold
	tbl->buckets_threshold = HASH_THRESHOLD(bucket_size);
    }
#ifdef HASH_DEBUG
    gettimeofday(&end, NULL);
    timeval_subtract(&result, &end, &start);
    tbl->avg_time += result;
    if (tbl->max_time < result) {
	tbl->max_time = result;
	tbl->max_inserted = tbl->inserted;
	tbl->max_removed= tbl->removed;
    }
#endif
    tbl->resize_passed_cnt++;
    return 0;
}

/*
 * to create new hash table
 */
#ifndef HASH_UNITTEST
NHashTable*
nhash_table_new(NHashFunc hashfunc,
		NEqualFunc keyequalfunc,
		uint64_t init_size,
		nkn_obj_type_t type)
#else
NHashTable*
nhash_table_new(NHashFunc hashfunc,
		NEqualFunc keyequalfunc,
		uint64_t init_size)
#endif
{
    NHashTable *temp;
#ifndef HASH_UNITTEST
    temp = nkn_calloc_type(1, sizeof(NHashTable), type);
#else
    temp = calloc(1, sizeof(NHashTable));
#endif
    if (temp) {
	temp->keyequalfunc = keyequalfunc;
	temp->hashfunc = hashfunc;
#ifndef HASH_UNITTEST
	temp->obj_type = type;
#endif
	if (nhash_table_resize(temp, init_size)) {
	    free(temp);
	    return NULL;
	}
    }
    // if failed null will be returned
    return temp;
}

/*
 * look up the corresponding key in the hash table and return its bucket index
 */
static inline uint64_t
nhash_lookup_index(NHashTable *tbl,
		   uint64_t keyhash,
		   const void *key)
{
    uint64_t index, end_index, open_hash_inter;
    int state;

    index = keyhash % tbl->tot_buckets;
    end_index = index;
    open_hash_inter = keyhash % (tbl->tot_buckets - 1);
    ++open_hash_inter;
    state = CHECK_STATE(tbl->state,index);
    while (!(state & STATE_EMPTY) &&
	((state & STATE_TOMB) ||
	!(*tbl->keyequalfunc)(tbl->key[index], key))) {
	index += open_hash_inter;
	if (index >= tbl->tot_buckets) {
	    index -= tbl->tot_buckets;
	}
	if (index == end_index) {  // reached end
	    return tbl->tot_buckets;
	}
	state = CHECK_STATE(tbl->state,index);
    }
    if (state == STATE_OCCUPIED) {
	return index;
    }
    return tbl->tot_buckets;
}

/*
 * lookup the corresponding key in the hash table and return its bucket value
 */
void*
nhash_lookup(NHashTable *tbl,
	     uint64_t keyhash,
	     const void *key)
{
    uint64_t index;

    index = nhash_lookup_index(tbl, keyhash, key);
    if (index != tbl->tot_buckets) {
	return tbl->value[index];
    }
    return NULL;
}

/*
 * lookup the corresponding key in the hash table and
 * return original key and value with return status
 */
int
nhash_lookup_extented(NHashTable *tbl,
		      uint64_t keyhash,
		      const void *key,
		      void **orig_key,
		      void **value)
{
    uint64_t index;

    index = nhash_lookup_index(tbl, keyhash, key);
    if ((index == tbl->tot_buckets)||
	(CHECK_STATE(tbl->state,index) & STATE_EMPTY))
	return FALSE;

    if (orig_key) {
	*orig_key = tbl->key[index];
    }
    if (value) {
	*value = tbl->value[index];
    }
    return TRUE;
}

/*
 * remove the corresponding key in the hash table and
 * return the status of the removed entry
 */
int
nhash_remove(NHashTable *tbl,
	     uint64_t keyhash,
	     void *key)
{
    uint64_t index;

    index = nhash_lookup_index(tbl, keyhash, key);
    // first check the index and then check for state
    if (index != tbl->tot_buckets) {
	--tbl->size;
	SET_STATE(tbl->state, index, STATE_TOMB);
#ifdef HASH_DEBUG
        tbl->removed++;
#endif
	if ((((tbl->tot_buckets >> 2) > tbl->size) &&
	    tbl->tot_buckets > NHashPrime[0]) ||
	    ((tbl->buckets_occupied > tbl->buckets_threshold) &&
	    (tbl->tot_buckets > NHashPrime[0])))
		nhash_table_resize(tbl, tbl->size << 1);
	return TRUE;
    }
    return FALSE;
}

/*
 * remove the corresponding key in the hash tabled based
 * on the bucket index and return the status of the removed entry
 */
static int
nhash_remove_index(NHashTable *tbl,
		   uint64_t index)
{
    if ((index != tbl->tot_buckets) &&
	(CHECK_STATE(tbl->state, index) == STATE_OCCUPIED)) {
	tbl->size--;
#ifdef HASH_DEBUG
        tbl->removed++;
#endif
	SET_STATE(tbl->state, index, STATE_TOMB);
	return TRUE;
    }
    return FALSE;
}



/*
 * insert key and value into the hash table using key as index
 */
void
nhash_insert(NHashTable *tbl,
	     uint64_t keyhash,
	     void *key,
	     void *value)
{

    uint64_t index, end_index, open_hash_inter;
    int current_state;
    uint64_t tomb_index, final_index;

    tomb_index = final_index = tbl->tot_buckets;
    index = keyhash % tbl->tot_buckets;
    current_state = CHECK_STATE(tbl->state, index);
    if (current_state & STATE_EMPTY) { // direct hit case
	tbl->key[index ] = key;
	tbl->value[index] = value;
	UNSET_STATE(tbl->state, index, STATE_EMPTY_TOMB);
	tbl->size++;
	tbl->buckets_occupied++;
	if ((((tbl->tot_buckets >> 2) > tbl->size) &&
	    tbl->tot_buckets > NHashPrime[0]) ||
	    ((tbl->buckets_occupied > tbl->buckets_threshold) &&
	    ((tbl->size << 1) >= NHashPrime[0])))
		nhash_table_resize(tbl, tbl->size << 1);

    } else {   // collision case
	end_index = index;
	open_hash_inter = keyhash % (tbl->tot_buckets - 1);
	++open_hash_inter;
	while (!(current_state & STATE_EMPTY) &&
	       ((current_state & STATE_TOMB) ||
		!(*tbl->keyequalfunc)(tbl->key[index], key))) {
	    if (current_state & STATE_TOMB) {
		// a  deleted entry found
		tomb_index = index;
	    }
	    index += open_hash_inter;
	    if (index >= tbl->tot_buckets) {
		index -= tbl->tot_buckets;
	    }
	    if (index == end_index) {  // reached end
		// if tomb present else -1
		final_index = tomb_index;
		break;
	    }
	    current_state = CHECK_STATE(tbl->state, index);
	}
	if (final_index == tbl->tot_buckets) {
	    // if some key already there this needs
	    //to be deleted and added
	    if (!(CHECK_STATE(tbl->state,index) & STATE_EMPTY)) {
		final_index = index;
	    } else if (tomb_index != tbl->tot_buckets) {
		// preference given to tomb
		final_index = tomb_index;
	    } else {
		// least goes to empty
		final_index = index;
	    }
	}
	current_state = CHECK_STATE(tbl->state, final_index);
	if ((current_state & STATE_EMPTY) || (current_state & STATE_TOMB)) {
	    tbl->key[ final_index ] = key;
	    tbl->value[ final_index ] = value;
	    UNSET_STATE(tbl->state, final_index, STATE_EMPTY_TOMB);
	    tbl->size++;
	    if (current_state & STATE_EMPTY) {
		tbl->buckets_occupied++;
	    }
	    if ((((tbl->tot_buckets >> 2) > tbl->size) &&
	        tbl->tot_buckets > NHashPrime[0]) ||
		((tbl->buckets_occupied > tbl->buckets_threshold) &&
		((tbl->size << 1) >= NHashPrime[0])))
		    nhash_table_resize(tbl, tbl->size << 1);
	} else {
	    tbl->value[ final_index ] = value;
	}
    }
#ifdef HASH_DEBUG
    tbl->inserted++;
#endif
}

/*
 * call the given iterator function for valid key/value pairs
 */
void
nhash_foreach(NHashTable *tbl,
	      NHFunc iterfunc,
	      void *userdata)
{
    uint64_t index;
    isdatanull((void *)iterfunc, "nhash_foreach");
    for (index = 0; index < tbl->tot_buckets; ++index) {
	if (CHECK_STATE(tbl->state, index) == STATE_OCCUPIED) {
	    (*iterfunc)(tbl->key[index], tbl->value[index], userdata);
	}
    }
}

/*
 * call the given iterator function for valid key/value pairs. If iterator
 * function returns true, then remove the corresponding entry from hash table
 */
uint64_t
nhash_foreach_remove(NHashTable *tbl,
		     NHRFunc iterfunc,
		     void *userdata)
{
    uint64_t deleted = 0;
    uint64_t index;

    for (index = 0; index < tbl->tot_buckets; ++index) {
	if ((CHECK_STATE(tbl->state, index) == STATE_OCCUPIED) &&
	    ((*iterfunc)(tbl->key[index], tbl->value[index], userdata))) {
	    nhash_remove_index(tbl, index);
	    ++deleted;
	}
    }
    if ((((tbl->tot_buckets >> 2) > tbl->size) &&
	tbl->tot_buckets > NHashPrime[0]) ||
	((tbl->buckets_occupied > tbl->buckets_threshold) &&
	(tbl->tot_buckets > NHashPrime[0])))
	    nhash_table_resize(tbl, tbl->size << 1);

    return deleted;
}

/*
 * call the given iterator function for valid key/value pairs. If iterator
 * function returns true then return the corresondping value entry.
 */
void*
nhash_find(NHashTable *tbl,
	   NHRFunc iterfunc,
	   void *userdata)
{
    uint64_t index;
    isdatanull((void *)iterfunc, "nhash_find");
    for (index = 0; index < tbl->tot_buckets; ++index) {
	if ((CHECK_STATE(tbl->state, index) == STATE_OCCUPIED) &&
	    ((*iterfunc)(tbl->key[index], tbl->value[index], userdata))) {
	    return tbl->value[index];
	}
    }
    return NULL;
}

/*
 * hash table size [ excluding tomb values] is returned
 */
uint64_t
nhash_size(NHashTable *tbl)
{
    isdatanull((void *)tbl, "nhash_size");
    return tbl->size;
}

/*
 * init hashtable iterator
 */
void
nhash_iter_init(NHTableIter *iter,
		NHashTable *tbl)
{
    isdatanull((void *)iter, "nhash_iter_init");
    iter->tbl = tbl;
    iter->position = UINT64_MAX;
}

/*
 * get the next valid key/value pair using hashtable iterator
 */
int
nhash_iter_next(NHTableIter *iter,
		void **key,
		void **value)
{
    uint64_t position;
    int state;

    if (iter == NULL)
	return FALSE;
    position = iter->position;
    do {
	++position;
	if (position >= iter->tbl->tot_buckets) {
	    iter->position = position;
	    return FALSE;
	}
	state = CHECK_STATE(iter->tbl->state, position);
    } while (state != STATE_OCCUPIED);
    if (key) {
	*key = iter->tbl->key[position];
    }
    if (value) {
	*value = iter->tbl->value[position];
    }
    iter->position = position;
    return TRUE;
}

/*
 * remove the entry from hash table based on the hash table iterator value
 */
void
nhash_iter_remove(NHTableIter *iter)
{
    nhash_remove_index(iter->tbl, iter->position);
}

/*
 * destroy the hashtable by freeing key/value pairs and state
 */
void
nhash_destroy(NHashTable *tbl)
{
    if (tbl) {
	free(tbl->key);
	free(tbl->value);
	free(tbl->state);
	free(tbl);
    }
}

uint64_t
n_uint64_hash(const void *v)
{
    const uint64_t w1 = ((*(const uint64_t *)v) & 0xffffffff);
    const uint64_t w2 = ((*(const uint64_t *)v) & 0xffffffff00000000) >> 32;

    return (w1 ^ w2);
}


int
n_uint64_equal(const void *v1,
	      const void *v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}

int
n_str_equal(const void *v1,
	    const void *v2)
{
    const char *s1 = v1;
    const char *s2 = v2;

    return strcmp (s1, s2) == 0;
}

uint64_t
n_str_hash(const void *v)
{
    /* 31 bit hash function */
    const signed char *p = v;
    uint32_t h = *p;

    if (h)
	for (p += 1; *p != '\0'; p++)
	    h = (h << 5) - h + *p;

    return (uint64_t)h;
}


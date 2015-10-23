
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "nkn_hash.h"
#include "nkn_defs.h"

#ifndef __NKN_PRIVATE_HASH
#define __NKN_PRIVATE_HASH

#define UNUSED_ARGUMENT(x) (void)x

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


// macro to compute state size
#define BUCKET_STATE_SIZE(size) \
	((bucket_size >> 5) + 1)

#endif

/*
 * nkn_cmm_shm_api.h - CMM shared memory access API
 */

#ifndef _NKN_CMM_SHM_API_H
#define _NKN_CMM_SHM_API_H

#include "nkn_cmm_shm.h"

#define CMM_CHAN_MAGICNO 0x18129988

typedef struct cmm_chan {
    union {
	uint64_t data[2];
	struct cmm_chan_data {
	    int magicno;
	    int index;
	    uint64_t incarnation;
	} chan;
    } u;
}  cmm_shm_chan_t;

typedef enum {
    CMMSHM_CH_LOADMETRIC=1 /* node_load_metric_t */
} cmm_shm_chan_type_t;

/*
 * Establish channel to specified resource for given node_handle.
 *
 *  Returns: 
 *	0  => Success
 *	!0 => Failure
 */
int
cmm_shm_open_chan(const char *node_handle, cmm_shm_chan_type_t type, 
		  cmm_shm_chan_t *chan);

/*
 * Close channel.
 *
 *  Returns: 
 *	0  => Success
 *	!0 => Failure
 */
int
cmm_shm_close_chan(cmm_shm_chan_t *chan);

/*
 * Determine if a channel is stale.
 *
 *  Returns: 
 *	!0 => True
 *	0  => False
 */
int
cmm_shm_is_chan_invalid(cmm_shm_chan_t *chan);


/*
 * Get data pointer/length associated with channel.
 *
 *  Input:
 *	version - optional
 *
 *  Returns: 
 *	0  => Success
 *	!0 => Failure
 */
int
cmm_shm_chan_getdata_ptr(cmm_shm_chan_t *chan, void **data, int *datalen,
			 uint64_t *version);

#endif /* _NKN_CMM_SHM_API_H */

/*
 * End of nkn_cmm_shm_api.h
 */

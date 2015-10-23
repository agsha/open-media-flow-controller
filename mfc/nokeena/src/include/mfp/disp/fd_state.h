#ifndef FD_STATE_HH
#define FD_STATE_HH


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "displib_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fd_lock;

typedef int8_t (*lockIntf)(struct fd_lock*, int32_t idx);
typedef void (*deleteState)(struct fd_lock*);

typedef struct fd_lock {

	pthread_mutex_t** state;
	int32_t max_fds;

	lockIntf acquire_lock;
	lockIntf release_lock;	

	deleteState delete_fd_state;
} fd_lock_t;


fd_lock_t* newFdState(int32_t max_fds);

#ifdef __cplusplus
}
#endif

#endif

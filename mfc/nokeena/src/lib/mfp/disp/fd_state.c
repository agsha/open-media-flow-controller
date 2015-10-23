#include "disp/fd_state.h"


static int8_t acquireFdLock(fd_lock_t* fd_lk, int32_t idx);
static int8_t releaseFdLock(fd_lock_t* fd_lk, int32_t idx);
static void deleteFdState(fd_lock_t*);

fd_lock_t* newFdState(int32_t max_fds) {

	fd_lock_t* fd_lk = disp_calloc(sizeof(fd_lock_t), 1);
	if (fd_lk) {
		fd_lk->max_fds = max_fds;
		fd_lk->state = disp_calloc(max_fds, sizeof(pthread_mutex_t*));
		if (!fd_lk->state)
			goto del_fd_lock;
		fd_lk->state[0] = disp_calloc(max_fds, sizeof(pthread_mutex_t));
		if (!fd_lk->state[0])
			goto del_fd_lock;
		int i =1;
		pthread_mutex_init((fd_lk->state[0]), NULL);
		for (;i < max_fds; i++) {
			fd_lk->state[i] = 
				fd_lk->state[i-1] + 1;
			pthread_mutex_init((fd_lk->state[i]), NULL);
		}
		fd_lk->acquire_lock = acquireFdLock;
		fd_lk->release_lock = releaseFdLock;
		fd_lk->delete_fd_state = deleteFdState;
	}
	return fd_lk;

del_fd_lock:
	deleteFdState(fd_lk);
	return NULL;
}


static void deleteFdState(fd_lock_t* fd_lk) {

	if (fd_lk->state) {
		if (fd_lk->state[0]) {
			int i = 0;
			for (; i < fd_lk->max_fds; i++)
				pthread_mutex_destroy(fd_lk->state[i]);
			free(fd_lk->state[0]);
		}
		free(fd_lk->state);
	}
	free(fd_lk);
}


int8_t acquireFdLock(fd_lock_t* fd_lk, int32_t idx) {

	if (!fd_lk)
		return -1;
	if ((idx < 0) || (idx >= fd_lk->max_fds))
		return -2;
	pthread_mutex_lock(fd_lk->state[idx]);
	return 1;
}


int8_t releaseFdLock(fd_lock_t* fd_lk, int32_t idx) {

	if (!fd_lk)
		return -1;
	if ((idx < 0) || (idx >= fd_lk->max_fds))
		return -2;
	pthread_mutex_unlock(fd_lk->state[idx]);
	return 1;
}



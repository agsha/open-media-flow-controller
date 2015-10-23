#ifndef NKN_LOCKSTAT_H
#define	NKN_LOCKSTAT_H

/*
 * Lock abstraction over pthreads to incorporate lock statistics
 * There are two usage models.
 * 1.  Global/static locks.  These can be declared as nkn_mutex_t and
 *     used as such with the embedded statistics via the *_LOCK macros.
 * 2.  Dynamic/multi-instance locks.  The statistics can be declared separately
 *     from the lock instances and used via the *_LOCKSTAT macros.
 */

#include <pthread.h>
#include "nkn_stat.h"

typedef struct {
	uint64_t	count;		// number of calls to lock
	uint64_t	wait;		// number of times lock was busy
} nkn_lockstat_t;

#define NKN_LOCKSTAT_INITVAL 	{0,0}

typedef struct nkn_mutex {
	pthread_mutex_t	lock;
	nkn_lockstat_t stats;
} nkn_mutex_t;

typedef struct nkn_rwmutex {
	pthread_rwlock_t lock;
	nkn_lockstat_t rd_stats;
	nkn_lockstat_t wr_stats;
} nkn_rwmutex_t;

// Macros for global locks
// m is nkn_mutex_t *
// rw is nkn_rwmutex_t

#define	NKN_MUTEX_LOCK(m) {					\
	if (pthread_mutex_trylock(&((m)->lock))) {		\
		pthread_mutex_lock(&((m)->lock));		\
		(m)->stats.wait++;				\
	}							\
	(m)->stats.count++;	/* will race */}

#define NKN_MUTEX_LOCKR(m) {					\
	macro_ret = pthread_mutex_trylock(&((m)->lock));	\
	if (macro_ret == EBUSY) {				\
	    macro_ret = pthread_mutex_lock(&((m)->lock));	\
	    (m)->stats.wait++;					\
	}							\
	assert(macro_ret == 0);					\
	(m)->stats.count++;	/* will race */}

#define NKN_MUTEX_UNLOCK(m)	pthread_mutex_unlock(&((m)->lock));
#define NKN_MUTEX_UNLOCKR(m) {					\
	macro_ret = pthread_mutex_unlock(&((m)->lock));		\
	assert(macro_ret == 0);}

#define NKN_RWLOCK_RLOCK(rw) {					\
	if (pthread_rwlock_tryrdlock(&((rw)->lock))) {		\
		pthread_rwlock_rdlock(&((rw)->lock));		\
		(rw)->rd_stats.wait++;				\
	}							\
	(rw)->rd_stats.count++;	/* will race */}


#define NKN_RWLOCK_WLOCK(rw) {					\
	if (pthread_rwlock_trywrlock(&((rw)->lock))) {		\
		pthread_rwlock_wrlock(&((rw)->lock));		\
		(rw)->wr_stats.wait++;				\
	}							\
	(rw)->wr_stats.count++;	/* will race */}

#define NKN_RWLOCK_UNLOCK(rw)	pthread_rwlock_unlock(&((rw)->lock));

#define NKN_MUTEX_INIT(m, ptr, name)				\
	pthread_mutex_init(&((m)->lock), ptr);			\
	(m)->stats.count = 0;					\
	(m)->stats.wait = 0;					\
	nkn_mon_add("mLOCK.count", name, (void *)&((m)->stats.count), sizeof((m)->stats.count));  \
	nkn_mon_add("mLOCK.wait", name, (void *)&((m)->stats.wait), sizeof((m)->stats.wait));

#define NKN_MUTEX_INITR(m, ptr, name) {				\
	macro_ret = pthread_mutex_init(&((m)->lock), ptr);	\
	assert(macro_ret == 0);					\
	(m)->stats.count = 0;					\
	(m)->stats.wait = 0;					\
	nkn_mon_add("mLOCK.count", name, (void *)&((m)->stats.count), sizeof((m)->stats.count));  \
	nkn_mon_add("mLOCK.wait", name, (void *)&((m)->stats.wait), sizeof((m)->stats.wait));	\
    }

#define NKN_RWLOCK_INIT(rw, ptr, name)				\
	macro_ret = pthread_rwlock_init(&((rw)->lock), ptr);	\
	assert(macro_ret == 0);					\
	(rw)->rd_stats.count = 0;				\
	(rw)->rd_stats.wait = 0;				\
	(rw)->wr_stats.count = 0;				\
	(rw)->wr_stats.wait = 0;				\
	nkn_mon_add("rwLOCK.rd.count", name, (void *)&((rw)->rd_stats.count), sizeof((rw)->rd_stats.count));	\
	nkn_mon_add("rwLOCK.rd.wait", name, (void *)&((rw)->rd_stats.wait), sizeof((rw)->rd_stats.wait));	\
	nkn_mon_add("rwLOCK.wr.count", name, (void *)&((rw)->wr_stats.count), sizeof((rw)->wr_stats.count));	\
	nkn_mon_add("rwLOCK.wr.wait", name, (void *)&((rw)->wr_stats.wait), sizeof((rw)->wr_stats.wait));

#define NKN_FAIR_RWLOCK_INIT(rw, ptr, name)			\
	macro_ret = pthread_rwlockattr_init(ptr);		\
	assert(macro_ret == 0);					\
	macro_ret = pthread_rwlockattr_setkind_np(ptr,		\
		    PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);	\
	assert(macro_ret == 0);					\
	macro_ret = pthread_rwlock_init(&((rw)->lock), ptr);	\
	assert(macro_ret == 0);					\
	(rw)->rd_stats.count = 0;				\
	(rw)->rd_stats.wait = 0;				\
	(rw)->wr_stats.count = 0;				\
	(rw)->wr_stats.wait = 0;				\
	nkn_mon_add("rwLOCK.rd.count", name, (void *)&((rw)->rd_stats.count), sizeof((rw)->rd_stats.count));	\
	nkn_mon_add("rwLOCK.rd.wait", name, (void *)&((rw)->rd_stats.wait), sizeof((rw)->rd_stats.wait));	\
	nkn_mon_add("rwLOCK.wr.count", name, (void *)&((rw)->wr_stats.count), sizeof((rw)->wr_stats.count));	\
	nkn_mon_add("rwLOCK.wr.wait", name, (void *)&((rw)->wr_stats.wait), sizeof((rw)->wr_stats.wait));


// Macros for multi-instance locks with seprate global stats
// m is pthread_mutex_t *
// s is nkn_lockstat_t *

#define NKN_MUTEX_LOCKSTAT(m, s) {				\
	if (pthread_mutex_trylock(m)) {				\
		pthread_mutex_lock(m);				\
		(s)->wait++;					\
	}							\
	(s)->count++;	/* will race */}

#define NKN_MUTEX_UNLOCKSTAT(m)	pthread_mutex_unlock(m);

#define NKN_LOCKSTAT_INIT(s, name)				\
	(s)->count = 0;						\
	(s)->wait = 0;						\
	nkn_mon_add("mLOCK.count", name, (void *)&((s)->count), sizeof((s)->count));  \
	nkn_mon_add("mLOCK.wait", name, (void *)&((s)->wait), sizeof((s)->wait));


#endif /* NKN_LOCKSTAT_H */

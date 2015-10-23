#include <glib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include "cache_mgr.h"
#include "nkn_defs.h"
#include "nkn_page.h"
#include "nkn_stat.h"
#include <linux/mman.h>
#include <errno.h>
#include "nkn_cfg_params.h"
#include "nkn_shm.h"


/*
 * Implementation of the page pool.
 */
struct nkn_pp_entry;

/* Each page is linked on a free list */
typedef struct nkn_pp_entry {
	TAILQ_ENTRY(nkn_pp_entry) plist;
} nkn_pp_entry_t;

TAILQ_HEAD(nkn_pp_flist, nkn_pp_entry);

typedef struct nkn_pp_stats {
	uint64_t alloc, free, allocfail;
} nkn_pp_stats_t;

#define MAXPOOLNAME 		(8)
#define ONE_GB 			((uint64_t)(1024 * 1024 * 1024))
#define MAX_SHM_SIZE		((uint64_t)((8 * 2) * ONE_GB))
#define DEFAULT_MEM_POOL	((uint64_t)(36 * ONE_GB))
#define MAX_SHM_SEGMENT		(((int64_t)(2 * 1024)) / (8 * 2))


typedef struct nkn_mmap_pool {
	int 		segments;
	void 		*poolmap[MAX_SHM_SEGMENT];
	uint64_t 	poolsize[MAX_SHM_SEGMENT];  // init code don't worry about cache line
} nkn_map_pool_t;

static nkn_map_pool_t nkn_mmap_pools;

/* Each pool is a collection of fixed size pages */
typedef struct nkn_page_pool {
	pthread_mutex_t lock;
	int pagesize;
	int poolx;
	void *initpages;		// Initial set of pages
	uint64_t pooloff;
	char	name[MAXPOOLNAME];	// name of the pool (for stats)
	int totalpages, freepages;
	struct nkn_pp_flist flist;
	nkn_pp_stats_t stats;		// pool stats
} nkn_page_pool_t;

static nkn_page_pool_t *nkn_page_pools;
static unsigned int nkn_num_page_pools = 0;	// pools in increasing page size
static void *nkn_page_pool_pages;
unsigned long long glob_bm_total_buffers = 0;

#define PAGEID_POOL_MASK	0xff	// stored in lower two bytes
#define PAGE_TO_PAGEID(page, pn) (nkn_pageid_t)((u_int64_t)page | (pn & PAGEID_POOL_MASK))
#define PAGEID_TO_PAGE(pageid)	NKN_PAGEID_TO_MEM(pageid)
#define PAGEID_TO_POOLNUM(pageid) (pageid & PAGEID_POOL_MASK)

/*
 * when using nokeena user space TCP stack,
 * driver creates a MMAP memory with size as cm_max_cache_size in this pointer.
 * so buffer manager should use it to avoid data copy.
 *
 * when using linux kernel TCP stack, this pointer is == NULL.
 */
extern void * p_data_mmap;

#if 0
int 
nkn_page_get_size(nkn_pageid_t pageid, int *size)
{
	unsigned int pn = PAGEID_TO_POOLNUM(pageid);
	
	if (pn > nkn_num_page_pools)
		return -1;
	*size = nkn_page_pools[pn].pagesize;
	return 0;
}
#endif

static void *
nkn_pool_page_alloc(nkn_page_pool_t *pool)
{
	void *page = NULL;
	nkn_pp_entry_t *ppe;	

	pthread_mutex_lock(&pool->lock);
	ppe = pool->flist.tqh_first;
	if (ppe) {
		assert(pool->freepages);
		TAILQ_REMOVE(&pool->flist, ppe, plist);
		pool->freepages--;
		pool->stats.alloc++;
		page = (void *)ppe;
	} else
		pool->stats.allocfail++;
	pthread_mutex_unlock(&pool->lock);
	return page;
}

#if 0
static void
nkn_pool_page_free(nkn_page_pool_t *pool, void *page)
{
	nkn_pp_entry_t *ppe = (nkn_pp_entry_t *)page;
	
	pthread_mutex_lock(&pool->lock);
	TAILQ_INSERT_HEAD(&pool->flist, ppe, plist);
	pool->freepages++;
	pool->stats.free++;
	pthread_mutex_unlock(&pool->lock);
}

#endif
/* 
 * Return a page from specified pool.  Returns NULL if no pages are available
 */
void*
nkn_page_alloc(int poolnum)
{
	void *page;

	nkn_page_pool_t *pp = &nkn_page_pools[poolnum];
	page = nkn_pool_page_alloc(pp);
	if (page == NULL)
		return 0;
	return page;
}

/*
 * Free the specified page id
 */
#if 0
void 
nkn_page_free(nkn_pageid_t pageid)
{
	nkn_page_pool_t *pp;
	unsigned int pn = PAGEID_TO_POOLNUM(pageid);

	assert(pn < nkn_num_page_pools);
	pp = &nkn_page_pools[pn];
	nkn_pool_page_free(pp, PAGEID_TO_PAGE(pageid));	
}
#endif

static void
nkn_page_pool_init(nkn_page_pool_t *pp, const char *poolname, u_int64_t *pooloff, int *poolx,
		   int pagesize, int numpages)
{
	u_int64_t pn, pgoff, poolend;
	void *pages;

	pthread_mutex_init(&pp->lock, NULL);
	pp->pagesize = pagesize;	
	pp->totalpages = numpages;
	pp->initpages = ((char *)nkn_mmap_pools.poolmap[*poolx]) + *pooloff;
	pp->pooloff = *pooloff;
	pp->poolx = *poolx;
	strncpy(pp->name, poolname, MAXPOOLNAME);
	TAILQ_INIT(&pp->flist);

	pages = pp->initpages;
	poolend = nkn_mmap_pools.poolsize[*poolx];
	// insert each page in the pool
	for (pn=0,pgoff=0; pn<(u_int64_t)numpages; pn++, pgoff+=pagesize) {
		nkn_pp_entry_t *ppe;

		if ((*pooloff + pgoff + pagesize) > poolend) {
			++(*poolx);
			assert (*poolx < nkn_mmap_pools.segments);
			pages = nkn_mmap_pools.poolmap[*poolx];
			*pooloff = pgoff = 0;	
		}	
		ppe = (nkn_pp_entry_t *)((u_int64_t)pages + (u_int64_t)pgoff);
		TAILQ_INSERT_TAIL(&pp->flist, ppe, plist);
	}
	pp->freepages = numpages;
	if (*poolx == pp->poolx) {
		*pooloff = pp->pooloff + pgoff ; 
	} else { 
		*pooloff = pgoff;
	}
	
	(void)nkn_mon_add("totalpages", poolname, (void *)&pp->totalpages, sizeof(pp->totalpages));
	(void)nkn_mon_add("freepages", poolname, (void *)&pp->freepages, sizeof(pp->freepages));
	(void)nkn_mon_add("alloc", poolname, (void *)&pp->stats.alloc, sizeof(pp->stats.alloc));
	(void)nkn_mon_add("free", poolname, (void *)&pp->stats.free, sizeof(pp->stats.free));
	(void)nkn_mon_add("allocfail", poolname, (void *)&pp->stats.allocfail, sizeof(pp->stats.allocfail));
}



#if 0
int set_huge_pages(size_t pages);
int set_huge_pages(size_t pages)
{

	char cmd[256];
	snprintf(cmd, 256,  "echo %d > /proc/sys/vm/nr_hugepages", (int)pages);
	int ret = system(cmd);
	return ret;
}
#endif


/*
 * Add multipool support 
 */
void
nkn_multi_pagepool_init(uint64_t poolsize)
{

    	size_t request_pages = 0;
	if (poolsize > DEFAULT_MEM_POOL) {	
	    	request_pages = poolsize/1024/1024/2 + 1;
    		int errr = set_huge_pages("nvsd", request_pages);
    		if( errr == -1)
        		assert(0);

		// before doing mlock check network threads are up
		// this helps to unblock tunnel path in big ram case
		while(!nkn_system_inited) {
			sleep(1);
		}
		sleep(5);
	} else {
		int errr = set_huge_pages("nvsd", 0);
		if (errr == -1)
			assert(0);
	}
	
	int num_segment;
	int ret, i;
	uint64_t rem_segment;
	// single pool mmap should be enough
	if (poolsize <= DEFAULT_MEM_POOL) {
		nkn_mmap_pools.segments = 1;

		nkn_mmap_pools.poolmap[0] = mmap(NULL, poolsize, 
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

		if (nkn_mmap_pools.poolmap[0] == NULL)
			err(1, "Failed to mmap buffer pages");
		
		ret = mlock(nkn_mmap_pools.poolmap[0], poolsize);
		if (ret)
			err(1, "Failed to mlock buffer pages");
		nkn_mmap_pools.poolsize[0] = poolsize;
		
	} else {  // multipool support 
		num_segment = poolsize / MAX_SHM_SIZE;
		rem_segment = poolsize % MAX_SHM_SIZE;

		// general sanity check
		nkn_mmap_pools.segments = num_segment + (rem_segment? 1:0);
		if (nkn_mmap_pools.segments  > MAX_SHM_SEGMENT) 
			err(1, "Failed to create segments for buffer pages");

		for ( i = 0; i < num_segment; ++i) {
			nkn_mmap_pools.poolmap[i] = mmap(NULL, MAX_SHM_SIZE,
				PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);

			if (nkn_mmap_pools.poolmap[i] == NULL)
				err(1, "Failed to mmap pool buffer pages");
			ret = mlock(nkn_mmap_pools.poolmap[i], MAX_SHM_SIZE);
			if (ret)
				err(1, "Failed to mlock pool buffer pages");
			nkn_mmap_pools.poolsize[i] = MAX_SHM_SIZE;
			sleep(1);

		}

		if (rem_segment) {  // mmap the remaining memory
			nkn_mmap_pools.poolmap[i] = mmap(NULL, rem_segment,
				PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
			if (nkn_mmap_pools.poolmap[i] == NULL)
				err(1, "Failed to mmap rem pool buffer pages");
			ret = mlock(nkn_mmap_pools.poolmap[i], rem_segment);
			if (ret)
				err(1, "Failed to mlock rem pool buffer pages");
			nkn_mmap_pools.poolsize[i] = rem_segment;
		}
	}
}

/*
 * Initialize page pool
 */
void 
nkn_page_init(uint64_t poolsize, int numpools, nkn_page_config_t *pools)
{
	int ret;
	u_int64_t pooloff;
	int i, poolx;
	nkn_page_config_t *pc = pools;

	// Allocate buffer pages and populate free list
	// Use anonymous private memory for buffer pages to avoid core dumping
	// them
	if(p_data_mmap) {
		// using nokeena user space TCP stack
		nkn_page_pool_pages = p_data_mmap;
	} else {
		// using linux kernel TCP stack
		nkn_multi_pagepool_init(poolsize);
#ifdef NKN_BUFFER_PARANOIA
		mprotect(nkn_page_pool_pages, poolsize, PROT_READ);
#endif
	}

	nkn_num_page_pools = numpools;
	nkn_page_pools = nkn_calloc_type(numpools, sizeof(nkn_page_pool_t), mod_bm_page_pool_t);
	if (nkn_page_pools == NULL)
		err(1, "Failed to allocate page pools\n");

	pooloff = poolx = 0;
	for (i=0; i<numpools; i++) {
		nkn_page_pool_init(&nkn_page_pools[i], pc->name, &pooloff, &poolx, pc->pagesize, pc->numbufs);
		pc++;
	}
}

uint64_t 
nkn_page_get_total_pages(void)
{
	int i;
	uint64_t total_pages =0;
	if (nkn_page_pools) {
		for (i = 0; i < NUMFBPOOLS; i++) {
			total_pages += nkn_page_pools[i].totalpages;
		}
		glob_bm_total_buffers = total_pages;
		return total_pages;
	}
	return 0;
}

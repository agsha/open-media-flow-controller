/*
 * Implementation of the buffer manager.
 *
 * The buffer are allocated statically as an array of objects and underlying
 * pages.  Buffers with identity are inserted into a hash table for quick
 * lookup.  Each buffer has a ref count.  Buffers with a zero ref count
 * are linked into a free list (in popularity order) for reuse.
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <err.h>
#include <glib.h>
#include "nkn_defs.h"
#include "cache_mgr.h"
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_buf.h"
#include "cache_mgr_private.h"
#include "nkn_cfg_params.h"
#include "nkn_trace.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_lockstat.h"
#include "nkn_cod.h"
#include "nkn_page.h"
#include "nkn_cod_private.h"
#include "math.h"
#include "zvm_api.h"
#include "nkn_assert.h"
#include "md_client.h"

#define NUM_FREE_LIST	 	5
#define MAX_BUCKET_STATS        128
#define MAX_SETID_HASH		1024
#define MAX_ZEROPOOL            20


uint64_t glob_bufmgr_server_busy_err; 	//error code counter for NKN_SERVER_BUSY  10003
uint64_t glob_bm_refcnted_attribute_cache = 0;
uint64_t glob_bm_min_attrsize = 1024;
uint64_t glob_bm_overriden_min_attrsize = 0;
uint64_t glob_bm_min_attrcount = 0;
uint64_t glob_attr_normal_retry = 0;
uint64_t glob_attr_abnormal_retry = 0;
uint64_t glob_attr_normal2_retry = 0;
uint64_t glob_bm_negcache_percent = 0;

uint64_t glob_bm_tot_attrcnt = 0;
uint64_t bm_tot_attrcnt[CUR_NUM_CACHE];
AO_t bm_cur_attrcnt[CUR_NUM_CACHE];


extern long bm_cfg_buf_lruscale;
long glob_bm_dyn_lruscale = 3 * GRANULAR_TIMESTAMP;
int bm_min_attrsize = 1024;
int glob_bm_sta_discardcost = GRANULAR_TIMESTAMP / 5;
uint32_t bm_min_attrcount = 0;
uint64_t bm_negcache_percent = 0;
extern int cr_am_report_interval;
AO_t provlistbytes[NKN_MM_MAX_CACHE_PROVIDERS];
nkn_cod_entry_t *nkn_cod_entries = NULL; 
/*
 * when using nokeena user space TCP stack,
 * driver creates a MMAP memory with size as cm_max_cache_size in this pointer.
 * so buffer manager should use it to avoid data copy.
 * 
 * when using linux kernel TCP stack, this pointer is == NULL.
 */
extern void * p_data_mmap;


/*
 * Buffer stats counters for cache and free pools
 */

typedef struct {
	AO_t numcache;
	AO_t numcache_32, numcache_4, numcache_1;
	AO_t hits, misses, evict, reclaim, expire, discard, replace;
	AO_t numalloc, allocfail;
	AO_t setid_error, dupid;
	AO_t partial_mismatch, partial_merge, partial_repeat, cod_expired;
	AO_t hashallocfail;
	AO_t hit_bytes, fill_bytes, evict_bytes, c_buffer_bytes, c_fill_bytes;
	AO_t hit_bucket[MAX_BUCKET_STATS], object_size[MAX_BUCKET_STATS];
	AO_t age_bucket[MAX_BUCKET_STATS], expired_bytes;
} nkn_buffer_stats_t;



typedef struct {
	AO_t numfree;
	AO_t nattrfree;
} nkn_buffer_poolstats_t;

/* Buffer pool:  cache consisting of a Hash Table and associated stats */
typedef struct {
	GHashTable *buf_hash[MAX_SETID_HASH];
#ifdef LOCKSTAT_DEBUG 
	nkn_mutex_t buf_hashlock[MAX_SETID_HASH];
#else
	pthread_mutex_t buf_hashlock[MAX_SETID_HASH];
#endif
	nkn_buffer_stats_t stats[CUR_NUM_CACHE];
	nkn_buffer_stats_t p_stats[CUR_NUM_CACHE][NKN_MM_MAX_CACHE_PROVIDERS];
} nkn_buffer_pool_t;

/* 
 * Free pool:  pool of available buffers organized by popularity for fast
 * eviction.  Each pool has fixed size buffers.
 */

TAILQ_HEAD(nkn_freepool_head, nkn_buf);
typedef struct  nkn_freepool_head nkn_fp_head_t;

typedef struct {
	int bufsize;			// of of buffers in this pool
	AO_t nfree[NUM_FREE_LIST][MAX_ZEROPOOL];
	struct  nkn_freepool_head buf_free[NUM_FREE_LIST][MAX_ZEROPOOL];
	struct  nkn_freepool_head buf_attrfree[NUM_FREE_LIST][MAX_ZEROPOOL];
#ifdef LOCKSTAT_DEBUG
	nkn_mutex_t buf_free_lock[NUM_FREE_LIST][MAX_ZEROPOOL];
#else
	pthread_mutex_t buf_free_lock[NUM_FREE_LIST][MAX_ZEROPOOL];
#endif
	nkn_buffer_poolstats_t stats;
	AO_t cur_attr;
	unsigned long long tot_attr;
	AO_t cur_bufs;
	unsigned long long tot_bufs; 
	AO_t cur_freepool0_add;
	AO_t cur_freepool0_remove;
} nkn_buffer_freepool_t;

static uint32_t bufhdrsize;	// common size for buffers TMP - use union
static void *nkn_buf_allbufs;

static nkn_buffer_pool_t bufcache, attrcache;



/*
 *
 * We have 3 pools with 1K, 4K and 32K buffers.  By default,
 * attributes are allocated as 1K buffers and data as 32K.  Both
 * can be allocated/resized by request.
 *
 * The current breakdown is static as follows.  We will evolve to
 * a more dynamic scheme later.
 *	32K - N
 *	4K  - N
 *	1k  - N
 */
//#define NKN_INIT_ATTR_SIZE NKN_DEFAULT_ATTR_SIZE
static nkn_page_config_t nkn_pp_config[NUMFBPOOLS] = {
#define FBPOOL_32K  0
	{1, 0, CM_MEM_PAGE_SIZE, 0, "pp32k", 0},
#define FBPOOL_4K   1
	{1.25, 0, NKN_DEFAULT_ATTR_SIZE, 0, "pp4k", 0},
#define FBPOOL_1K   2
	{0, 0, NKN_INIT_ATTR_SIZE, 0, "pp1k", 0},
};

static nkn_page_config_t nkn_pp_negconfig[NUMFBPOOLS] = {
	{1, 0, CM_MEM_PAGE_SIZE, 0, "pp32k.neg", 0},
	{1.25, 0, NKN_DEFAULT_ATTR_SIZE, 0, "pp4k.neg", 0},
	{0, 0, NKN_INIT_ATTR_SIZE, 0, "pp1k.neg", 0},
};


#define MINPAGESIZE	512
#define MINPAGEMASK	(MINPAGESIZE - 1)
#define FREEMAPSIZE 	(CM_MEM_PAGE_SIZE/MINPAGESIZE + 1)

static int freepoolmap[FREEMAPSIZE];
static nkn_buffer_freepool_t *freebufpools[CUR_NUM_CACHE];
static int glob_nkn_buf_acct_err = 0;

static void nkn_obj_purge(nkn_obj_t *, int );
static
void nkn_buf_internal_alloc(nkn_buf_t *bp, 
                            nkn_buffer_type_t type);
static
void nkn_buf_deref(nkn_buf_t *bp);
// static void nkn_buf_page_free(nkn_buf_t *bp);

#define HASHID_URI_OFF 9		/* 8 digits of off plus '_' */
static void
uol_to_bufid(nkn_uol_t *uol, nkn_buffer_type_t type, char *id)
{
	int ret;

	if (type == NKN_BUFFER_DATA)
		ret = snprintf(id, MAXBUFID, "%8ld_%lx", uol->offset/CM_MEM_PAGE_SIZE, uol->cod);
	else if (type == NKN_BUFFER_ATTR)
		ret = snprintf(id, MAXBUFID, "%lx", uol->cod);
	else
		assert(0);
	assert(ret < MAXBUFID);
}

static int
nkn_buf_size_to_freepoolnum(int bufsize)
{
	int rsize;

	rsize = bufsize / MINPAGESIZE;	// compiler should make this a shift
	if (bufsize & MINPAGEMASK)
		rsize++;
	assert(rsize < FREEMAPSIZE); 
	return freepoolmap[rsize];
}

static nkn_buffer_freepool_t *
nkn_buf_size_to_freepool(int bufsize, int pooltype)
{
	int poolnum;

	poolnum = nkn_buf_size_to_freepoolnum(bufsize);
	return &freebufpools[pooltype][poolnum];
}

static nkn_buffer_pool_t *
nkn_buf_type_to_pool(nkn_buffer_type_t type)
{
	if (type == NKN_BUFFER_DATA)
		return &bufcache;
	else if (type == NKN_BUFFER_ATTR)
		return &attrcache;
	else
		assert(0);
}

static int
nkn_buf_type_to_default_size(nkn_buffer_type_t type)
{
	if (type == NKN_BUFFER_DATA)
		return CM_MEM_PAGE_SIZE;
	else if (type == NKN_BUFFER_ATTR) {
		if (glob_bm_min_attrcount)
			return (glob_bm_min_attrsize);
		else
			return NKN_DEFAULT_ATTR_SIZE;
	}
	assert(0);
}

/* Get the next available page size for the requested size */
static int
nkn_buf_get_pagesize(int rsize, int pooltype)
{
	nkn_buffer_freepool_t *pool = nkn_buf_size_to_freepool(rsize, pooltype);
	return pool->bufsize;
}

static void
nkn_buf_pool_init(int pool, int bufsize, size_t numbufs,
		  size_t numattr, void *buffers, const char *name, int pooltype)
{
	nkn_buf_t *bp;
	nkn_buffer_freepool_t *fpool;
#ifdef LOCKSTAT_DEBUG
	char buf[100];
#endif
	size_t pn, pn1 = 0;
	int i, j, flist0;

	fpool = &freebufpools[pooltype][pool];
	fpool->bufsize = bufsize;
	fpool->tot_attr = numattr;
	for (i=0; i<NUM_FREE_LIST; i++) {
		for (j =0; j < MAX_ZEROPOOL; j++) {
			if (i > 0 && j > 0)
				continue;
			TAILQ_INIT(&fpool->buf_free[i][j]);
			TAILQ_INIT(&fpool->buf_attrfree[i][j]);
#ifdef LOCKSTAT_DEBUG
			snprintf(buf, 100, "buf_free_%s_%d_%d", name, i, j);
			NKN_MUTEX_INIT(&fpool->buf_free_lock[i][j], NULL, buf);
#else
			pthread_mutex_init(&fpool->buf_free_lock[i][j], NULL);
#endif
		}
	}

	bp = (nkn_buf_t *)buffers;
	for (pn=0; pn<numbufs; pn++) {
		flist0 = pn1++ % MAX_ZEROPOOL;
		bp->bufsize = bufsize;
		pthread_mutex_init(&bp->lock, NULL);
		bp->flags |= NKN_BUF_FREE_FLIST;
		TAILQ_INSERT_TAIL(&fpool->buf_free[0][flist0], bp, entries);
		bp->flist0 = flist0;
		bp = (nkn_buf_t *)((uint64_t)bp + bufhdrsize);
		AO_fetch_and_add1(&fpool->nfree[0][flist0]);
	}
	AO_fetch_and_add(&fpool->stats.numfree, numbufs);

	(void)nkn_mon_add("numfree", name, (void *)&fpool->stats.numfree, sizeof(fpool->stats.numfree));
	(void)nkn_mon_add("numattrfree", name, (void *)&fpool->stats.nattrfree,
			  sizeof(fpool->stats.nattrfree));
}

/* Initialize */
void nkn_buf_init()
{
	uint64_t unit_bufs, num_attr, total_bufs = 0;
	uint64_t total_bytes, unit_size = 0;
	void *buffers;
	int ret, i, psize, poolnum;
	char name[100];
	uint64_t temp_total_bytes;
	uint64_t cod_entries = 0;
	double min_page_ratio = 0;

	/* 
	 * We use a common buffer structure (sized to be larger of the two)
	 * for attributes and data.  The typing is done when the buffer is
	 * allocated.
	 */
	total_bytes = cm_max_cache_size;  /* from MBs */
	temp_total_bytes = total_bytes;
	glob_bm_min_attrsize = bm_min_attrsize;
	glob_bm_min_attrcount = bm_min_attrcount;
	glob_bm_negcache_percent = bm_negcache_percent;
	if (!glob_bm_min_attrcount) {
		glob_bm_negcache_percent = 0;
	} else {
		if (glob_bm_negcache_percent > 20)
			glob_bm_negcache_percent = 20;
	}
	if (bm_page_ratio != 0) {
		nkn_page_config_t *pc4k = &nkn_pp_config[FBPOOL_4K];
		pc4k->initratio = powf(2.0, (double)bm_page_ratio);
	}
	if (glob_bm_min_attrcount != 0) {
		nkn_page_config_t *pc4k = &nkn_pp_config[FBPOOL_4K];
		if (pc4k->initratio < 3.0) {
			pc4k->initratio = 2.25;
		}
		min_page_ratio =  9.0 * (pc4k->initratio + 1.0) / 4.0;

		nkn_page_config_t *pc1k = &nkn_pp_config[FBPOOL_1K];
		pc1k->pagesize = glob_bm_min_attrsize;
		for (i=0; i<NUMFBPOOLS - 1 ; i++) {
		    nkn_page_config_t *pc = &nkn_pp_config[i];
		    unit_size += (uint64_t)(pc->initratio * (double)pc->pagesize);
		}
		unit_size += (pc1k->pagesize * min_page_ratio);
		unit_bufs = temp_total_bytes / unit_size;
		uint64_t num_1kbufs = (uint64_t)((double)unit_bufs * min_page_ratio);
		// minimum boundary check
		if (glob_bm_min_attrcount  < num_1kbufs)
		{
			glob_bm_min_attrcount = num_1kbufs;
			glob_bm_overriden_min_attrsize++;
			if (glob_bm_min_attrcount != bm_min_attrcount)
			    lc_log_basic(LOG_ALERT, "ram-cache attribute count %d(%d)",
				(uint32_t)glob_bm_min_attrcount, bm_min_attrcount);
		}
		// higher boundary check
		if ((glob_bm_min_attrcount * pc1k->pagesize) > (total_bytes * 90 / 100)) {
			glob_bm_min_attrcount = (total_bytes * 90 / 100) / pc1k->pagesize;
			glob_bm_overriden_min_attrsize++;
			if (glob_bm_min_attrcount != bm_min_attrcount)
			    lc_log_basic(LOG_ALERT, "ram-cache attribute count %d(%d)",
				(uint32_t)glob_bm_min_attrcount, bm_min_attrcount);
		}
		pc1k->numbufs = glob_bm_min_attrcount;
		temp_total_bytes -= (glob_bm_min_attrcount * pc1k->pagesize);
		unit_size = 0;
		switch(pc1k->pagesize) {
		    case 512:
				strcpy(pc1k->name, "pp512");
				break;
		    case 1024:
				strcpy(pc1k->name, "pp1k");
				break;
		    case 1536:
				strcpy(pc1k->name, "pp1.5k");
				break;
		    case 2048:
				strcpy(pc1k->name, "pp2k");
				break;
		    case 2560:
				strcpy(pc1k->name, "pp2.5k");
				break;
		    case 3072:
				strcpy(pc1k->name, "pp3k");
				break;
		}

	}

	for (i=0; i<NUMFBPOOLS - 1; i++) {
		nkn_page_config_t *pc = &nkn_pp_config[i];
		unit_size += (uint64_t)(pc->initratio * (double)pc->pagesize);
	}
	unit_bufs = temp_total_bytes / unit_size;
	for (i=0; i<NUMFBPOOLS - 1; i++) {
		nkn_page_config_t *pc = &nkn_pp_config[i];
		pc->numbufs = (uint64_t)((double)unit_bufs * pc->initratio);
		total_bufs += pc->numbufs;
		if (glob_bm_negcache_percent) {
			nkn_page_config_t *negpc = &nkn_pp_negconfig[i];
			negpc->tot_bufs = pc->numbufs * glob_bm_negcache_percent / 100;
			pc->tot_bufs = pc->numbufs - negpc->tot_bufs;
			negpc->pagesize = pc->pagesize;
		}
	}

	if (glob_bm_min_attrcount) {
		total_bufs += glob_bm_min_attrcount;
		if (!glob_bm_negcache_percent) {
			nkn_page_config_t *pc4k = &nkn_pp_config[FBPOOL_4K];
			bm_tot_attrcnt[BM_CACHE_ALLOC] = glob_bm_min_attrcount + (pc4k->numbufs * 5 / 8);
			pc4k->num_attr = pc4k->numbufs;
			nkn_page_config_t *pc1k = &nkn_pp_config[FBPOOL_1K];
			pc1k->num_attr = glob_bm_min_attrcount;
			cod_entries = bm_tot_attrcnt[BM_CACHE_ALLOC] + (bm_tot_attrcnt[BM_CACHE_ALLOC] / 10 );
		} else {
			nkn_page_config_t *pc1k = &nkn_pp_config[FBPOOL_1K];
			nkn_page_config_t *negpc1k = &nkn_pp_negconfig[FBPOOL_1K];
			nkn_page_config_t *pc4k = &nkn_pp_config[FBPOOL_4K];
			nkn_page_config_t *negpc4k = &nkn_pp_negconfig[FBPOOL_4K];
			nkn_page_config_t *negpc32k = &nkn_pp_negconfig[FBPOOL_32K];
			negpc1k->pagesize = pc1k->pagesize;
			snprintf(negpc1k->name, sizeof(negpc1k->name), "%s.neg", pc1k->name);
			negpc1k->tot_bufs = pc1k->numbufs * glob_bm_negcache_percent / 100;
			pc1k->tot_bufs = pc1k->numbufs - negpc1k->tot_bufs;
			bm_tot_attrcnt[BM_CACHE_ALLOC] = pc1k->tot_bufs + (pc4k->numbufs * 5 / 8);
			uint64_t i4k_32k = (negpc4k->tot_bufs + negpc32k->tot_bufs); 
			//                      one attr = one data page + extra 10% attribute 
			// total 4k_32k + (total 1k - total 4k_32k) / 2 + (total 1k - total 4k_32k) / 20
			bm_tot_attrcnt[BM_NEGCACHE_ALLOC] = i4k_32k + (negpc1k->tot_bufs - i4k_32k) * 3 / 5;    
			glob_bm_tot_attrcnt = bm_tot_attrcnt[BM_CACHE_ALLOC] + bm_tot_attrcnt[BM_NEGCACHE_ALLOC];
			cod_entries = glob_bm_tot_attrcnt + glob_bm_tot_attrcnt / 10;
			 
			
		}
	} else {
		num_attr = total_bufs / 2;// worst case 1 data page
		/* 10% extra allocated to handle traffic when system went to ideal
		   case where num of attr = data buffers
		*/
		glob_bm_tot_attrcnt = bm_tot_attrcnt[BM_CACHE_ALLOC] =
					num_attr + (num_attr / 10);
		nkn_page_config_t *pc4k = &nkn_pp_config[FBPOOL_4K];
		pc4k->num_attr = glob_bm_tot_attrcnt;
		nkn_page_config_t *pc32k = &nkn_pp_config[FBPOOL_32K];
		pc32k->num_attr = pc32k->numbufs;
		cod_entries = glob_bm_tot_attrcnt + (num_attr / 5 );
	}

	// Initialize free pool map
	poolnum = NUMFBPOOLS-1;
	for (i = 0,psize=0; i<FREEMAPSIZE; i++,psize+=MINPAGESIZE) {
		if (psize > nkn_pp_config[poolnum].pagesize) {
			assert(poolnum > 0);
			poolnum--;
		}
		freepoolmap[i] = poolnum;
	}
		
	// Initialize page pool
	nkn_page_init(total_bytes, NUMFBPOOLS, nkn_pp_config);

	// Initialize buffer headers
	bufhdrsize = sizeof(nkn_obj_t);
	if (bufhdrsize < sizeof(nkn_page_t))
		bufhdrsize = sizeof(nkn_page_t);
	nkn_buf_allbufs = nkn_calloc_type(total_bufs, bufhdrsize,
					mod_bm_nkn_page_t);
	if (nkn_buf_allbufs == NULL)
		err(1, "Failed to allocate buffer headers\n");
	ret = mlock(nkn_buf_allbufs, total_bufs*bufhdrsize);
	if (ret)
		err(1, "Failed to mlock buffer headers\n");

	// Initialize free pools and distribute the headers into them
	for (i = 0; i < CUR_NUM_CACHE; ++i) {
		freebufpools[i] = nkn_calloc_type(NUMFBPOOLS, sizeof(nkn_buffer_freepool_t), mod_bm_page_pool_t);
		if (freebufpools[i] == NULL)
			err(1, "Failed to allocate buffer pools\n");
	}
	buffers = nkn_buf_allbufs;

	for (i=0; i<NUMFBPOOLS; i++) {
		nkn_page_config_t *pc = &nkn_pp_config[i];
		nkn_buf_pool_init(i, pc->pagesize, pc->numbufs, pc->num_attr, buffers, pc->name, BM_CACHE_ALLOC);
		buffers = (void *)((uint64_t)buffers + pc->numbufs*bufhdrsize);
	}

	for (i=0; i<NUMFBPOOLS; i++) {
		nkn_page_config_t *pc = &nkn_pp_negconfig[i];
		nkn_buf_pool_init(i, pc->pagesize, pc->numbufs, pc->num_attr, buffers, pc->name, BM_NEGCACHE_ALLOC);
		freebufpools[BM_NEGCACHE_ALLOC][i].tot_bufs = pc->tot_bufs;
	}
	// Initialize pool for buffers
	for (i = 0; i < MAX_SETID_HASH; i++) {
		bufcache.buf_hash[i] = g_hash_table_new(g_str_hash, g_str_equal);
#ifdef LOCKSTAT_DEBUG
		snprintf(name, 100, "buf-hashmutex-%d", i);
		NKN_MUTEX_INIT(&bufcache.buf_hashlock[i], NULL, name);
#else
		pthread_mutex_init(&bufcache.buf_hashlock[i], NULL);
#endif
	}

	//total cache attribute count
	nkn_mon_add("bm.global.bm.attrcache.tot_cnt", NULL,
		(void *)&bm_tot_attrcnt[BM_CACHE_ALLOC],
			sizeof(bm_tot_attrcnt[BM_CACHE_ALLOC]));
	// current cache attribute count
	nkn_mon_add("bm.global.bm.attrcache.cur_cnt", NULL,
		(void *)&bm_cur_attrcnt[BM_CACHE_ALLOC],
			sizeof(bm_cur_attrcnt[BM_CACHE_ALLOC]));
		
	// total negative cache attribute count
	nkn_mon_add("bm.global.bm.attrnegcache.tot_cnt", NULL,
		(void *)&bm_tot_attrcnt[BM_NEGCACHE_ALLOC],
			sizeof(bm_tot_attrcnt[BM_NEGCACHE_ALLOC]));
	// current negative cache attribute count
	nkn_mon_add("bm.global.bm.attrnegcache.cur_cnt", NULL,
		(void *)&bm_cur_attrcnt[BM_NEGCACHE_ALLOC],
			sizeof(bm_cur_attrcnt[BM_NEGCACHE_ALLOC]));

	nkn_mon_add("bufcache_hit", NULL,
		    (void *)&bufcache.stats[BM_CACHE_ALLOC].hits,
		    sizeof(bufcache.stats[BM_CACHE_ALLOC].hits));
	nkn_mon_add("bufcache_evict", NULL,
		    (void *)&bufcache.stats[BM_CACHE_ALLOC].evict,
		    sizeof(bufcache.stats[BM_CACHE_ALLOC].evict));
	
	nkn_mon_add("bufcache_discard", NULL,
		    (void *)&bufcache.stats[BM_CACHE_ALLOC].discard,
		    sizeof(bufcache.stats[BM_CACHE_ALLOC].discard));
	
	nkn_mon_add("bufcache_entries", NULL,
		    (void *)&bufcache.stats[BM_CACHE_ALLOC].numcache,
		    sizeof(bufcache.stats[BM_CACHE_ALLOC].numcache));
	nkn_mon_add("attrcache_evict", NULL,
		    (void *)&attrcache.stats[BM_CACHE_ALLOC].evict,
		    sizeof(attrcache.stats[BM_CACHE_ALLOC].evict));
	nkn_mon_add("attrcache_entries", NULL,
		    (void *)&attrcache.stats[BM_CACHE_ALLOC].numcache,
		    sizeof(attrcache.stats[BM_CACHE_ALLOC].numcache));

	int l;
	const char *cachetype;
	for (l = 0; l < CUR_NUM_CACHE ; ++l) {
		if ( l == 0)
			cachetype = "bufcache";
		else
			cachetype = "bufnegcache";
	snprintf(name, 100, "bm.global.bm.%s.hit_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].hits,
				sizeof(bufcache.stats[l].hits));
	snprintf(name, 100, "bm.global.bm.%s.miss_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].misses,
				sizeof(bufcache.stats[l].misses));
	snprintf(name, 100, "bm.global.bm.%s.evict_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].evict,
				sizeof(bufcache.stats[l].evict));

	snprintf(name, 100, "bm.global.bm.%s.reclaim_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].reclaim,
				sizeof(bufcache.stats[i].reclaim));

	snprintf(name, 100, "bm.global.bm.%s.expire_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].expire,
				sizeof(bufcache.stats[l].expire));
	
	snprintf(name, 100, "bm.global.bm.%s.discard_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].discard,
				sizeof(bufcache.stats[l].discard));

	snprintf(name, 100, "bm.global.bm.%s.replace_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].replace,
				sizeof(bufcache.stats[l].replace));

	snprintf(name, 100, "bm.global.bm.%s.allocfail_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].allocfail,
				sizeof(bufcache.stats[l].allocfail));

	snprintf(name, 100, "bm.global.bm.%s.alloc_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].numalloc,
				sizeof(bufcache.stats[l].numalloc));

	snprintf(name, 100, "bm.global.bm.%s.cache_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].numcache,
				sizeof(bufcache.stats[l].numcache));

	snprintf(name, 100, "bm.global.bm.%s.dupid_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].dupid,
				sizeof(bufcache.stats[l].dupid));

	snprintf(name, 100, "bm.global.bm.%s.hitbytes", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].hit_bytes,
		    		sizeof(bufcache.stats[l].hit_bytes));

	snprintf(name, 100, "bm.global.bm.%s.fillbytes", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].fill_bytes,
				sizeof(bufcache.stats[l].fill_bytes));

	snprintf(name, 100, "bm.global.bm.%s.evictbytes", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].evict_bytes,
				sizeof(bufcache.stats[l].evict_bytes));

	snprintf(name, 100, "bm.global.bm.%s.expiredbytes", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].expired_bytes,
		    		sizeof(bufcache.stats[l].expired_bytes));

	snprintf(name, 100, "bm.global.bm.%s.instant_fillbytes", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].c_fill_bytes,
		    		sizeof(bufcache.stats[l].c_fill_bytes));

	snprintf(name, 100, "bm.global.bm.%s.instant_totusedbufbytes", cachetype);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].c_buffer_bytes,
		    		sizeof(bufcache.stats[l].c_buffer_bytes));
	for ( i = 0 ; i < (MAX_BUCKET_STATS / 2 - 1); ++i) {
		snprintf(name, 100, "bm.global.bm.%s.hitbucket_%d", cachetype, i);
		nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].hit_bucket[i],
			    sizeof(bufcache.stats[l].hit_bucket[i]));
		snprintf(name, 100,
			"bm.global.bm.%s.objsizebucket_%dK_to_%dK", cachetype, i, (i+1));
		nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].object_size[i],
			    sizeof(bufcache.stats[l].object_size[i]));
	}
	snprintf(name, 100, "bm.global.bm.%s.hitbucket_gte_%d", cachetype, i);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].hit_bucket[i],
		    sizeof(bufcache.stats[l].hit_bucket[i]));
	snprintf(name, 100, "bm.global.bm.%s.objsizebucket_gte_%dK", cachetype, i);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].object_size[i],
		    sizeof(bufcache.stats[l].object_size[i]));
	for ( i = 0 ; i < (MAX_BUCKET_STATS - 1); ++i) {
		snprintf(name, 100,
			"bm.global.bm.%s.agebucket_%ds_to_%ds",
			cachetype, i * 10, ((i + 1) * 10) - 1);
		nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].age_bucket[i],
			    sizeof(bufcache.stats[l].age_bucket[i]));
	}
	snprintf(name, 100, "bm.global.bm.%s.agebucket_gte_%ds", cachetype, i  * 10);
	nkn_mon_add(name, NULL, (void *)&bufcache.stats[l].age_bucket[i],
		    sizeof(bufcache.stats[l].age_bucket[i]));

	for ( i = 0; i < NKN_MM_MAX_CACHE_PROVIDERS; ++i) {
		if ((i !=  SolidStateMgr_provider) && (i != SAS2DiskMgr_provider) &&
		    (i != SATADiskMgr_provider) && (i != OriginMgr_provider)
		    && (i != NFSMgr_provider))
			continue;
		const char *provider = MM_get_provider_name(i);
		snprintf(name, 100, "bm.provider.%s.%s.evict_cnt",
			 provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].evict,
			    sizeof(bufcache.p_stats[l][i].evict));
		snprintf(name, 100, "bm.provider.%s.%s.expire_cnt",
			 provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].expire,
			    sizeof(bufcache.p_stats[l][i].expire));
		snprintf(name, 100, "bm.provider.%s.%s.discard_cnt",
			 provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].discard,
			    sizeof(bufcache.p_stats[l][i].discard));
		snprintf(name, 100, "bm.provider.%s.%s.replace_cnt", provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].replace,
			    sizeof(bufcache.p_stats[l][i].replace));
		snprintf(name, 100, "bm.provider.%s.%s.cache_cnt", provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].numcache,
			    sizeof(bufcache.p_stats[l][i].numcache));
		snprintf(name, 100, "bm.provider.%s.%s.dupid_cnt", provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].dupid,
			    sizeof(bufcache.p_stats[l][i].dupid));
		snprintf(name, 100, "bm.provider.%s.%s.fillbytes", provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].fill_bytes,
			    sizeof(bufcache.p_stats[l][i].fill_bytes));
		snprintf(name, 100, "bm.provider.%s.%s.evictbytes", provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].evict_bytes,
			    sizeof(bufcache.p_stats[l][i].evict_bytes));
		snprintf(name, 100, "bm.provider.%s.%s.expiredbytes", provider, cachetype);
		nkn_mon_add(name, NULL,
			    (void *)&bufcache.p_stats[l][i].expired_bytes,
			    sizeof(bufcache.p_stats[l][i].expired_bytes));
		snprintf(name, 100, "bm.provider.%s.%s.instant_fillbytes", provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].c_fill_bytes,
			    sizeof(bufcache.p_stats[l][i].c_fill_bytes));
		snprintf(name, 100, "bm.provider.%s.%s.instant_totusedbufbytes",
			 provider, cachetype);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].c_buffer_bytes,
			    sizeof(bufcache.p_stats[l][i].c_buffer_bytes));
		int j = 0;
		for ( ; j < (MAX_BUCKET_STATS / 2 - 1); ++j) {
			snprintf(name, 100, "bm.provider.%s.%s.hitbucket_%d", provider, cachetype, j);
			nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].hit_bucket[j],
				    sizeof(bufcache.p_stats[l][i].hit_bucket[j]));
			snprintf(name, 100, "bm.provider.%s.%s.objsizebucket_%dK_to_%dK",
				 provider, cachetype, j, (j +1));
			nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].object_size[j],
				    sizeof(bufcache.p_stats[l][i].object_size[j]));
		}
		snprintf(name, 100, "bm.provider.%s.%s.hitbucket_gte_%d", provider, cachetype, j);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].hit_bucket[j],
			    sizeof(bufcache.p_stats[l][i].hit_bucket[j]));
		snprintf(name, 100, "bm.provider.%s.%s.objsizebucket_gte_%dK",
			 provider, cachetype, j);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].object_size[j],
			    sizeof(bufcache.p_stats[l][i].object_size[j]));
		for ( j = 0 ; j < (MAX_BUCKET_STATS - 1); ++j) {
			snprintf(name, 100, "bm.provider.%s.%s.agebucket_%ds_to_%ds",
				 provider, cachetype, j * 10, ((j + 1) * 10) - 1);
			nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].age_bucket[j],
				    sizeof(bufcache.p_stats[l][i].age_bucket[j]));
		}
		snprintf(name, 100, "bm.provider.%s.%s.agebucket_gte_%ds",
			 provider, cachetype, j * 10);
		nkn_mon_add(name, NULL, (void *)&bufcache.p_stats[l][i].age_bucket[j],
			    sizeof(bufcache.p_stats[l][i].age_bucket[j]));
	}

	if (l == 0)
		cachetype = "attrcache";
	else
		cachetype = "attrnegcache";
	snprintf(name, 100, "bm.global.bm.%s.hit_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].hits,
				sizeof(attrcache.stats[l].hits));
	snprintf(name, 100, "bm.global.bm.%s.miss_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].misses,
				sizeof(attrcache.stats[l].misses));
	snprintf(name, 100, "bm.global.bm.%s.evict_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].evict,
				sizeof(attrcache.stats[l].evict));
	snprintf(name, 100, "bm.global.bm.%s.reclaim_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].reclaim,
				sizeof(attrcache.stats[l].reclaim));
	snprintf(name, 100, "bm.global.bm.%s.expire_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].expire,
				sizeof(attrcache.stats[l].expire));
	snprintf(name, 100, "bm.global.bm.%s.dicard_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].discard,
				sizeof(attrcache.stats[l].discard));
	snprintf(name, 100, "bm.global.bm.%s.replace_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].replace,
				sizeof(attrcache.stats[l].replace));
	snprintf(name, 100, "bm.global.bm.%s.allocfail_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].allocfail,
				sizeof(attrcache.stats[l].allocfail));
	snprintf(name, 100, "bm.global.bm.%s.alloc_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].numalloc,
				sizeof(attrcache.stats[l].numalloc));
	snprintf(name, 100, "bm.global.bm.%s.cache_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].numcache,
				sizeof(attrcache.stats[l].numcache));
	snprintf(name, 100, "bm.global.bm.%s.cache32k_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].numcache_32,
				sizeof(attrcache.stats[l].numcache_32));
	snprintf(name, 100, "bm.global.bm.%s.cache4k_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].numcache_4,
				sizeof(attrcache.stats[l].numcache_4));
	snprintf(name, 100, "bm.global.bm.%s.cache1k_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].numcache_1,
				sizeof(attrcache.stats[l].numcache_1));
	snprintf(name, 100, "bm.global.bm.%s.dupid_cnt", cachetype);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].dupid,
				sizeof(attrcache.stats[l].dupid));
	for ( i = 0; i < (MAX_BUCKET_STATS / 2 -1); ++i) {
		snprintf(name, 100, "bm.global.bm.%s.hitbucket_%d", cachetype, i);
		nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].hit_bucket[i],
			    sizeof(attrcache.stats[l].hit_bucket[i]));
		snprintf(name, 100, "bm.global.bm.%s.objsizebucket_%dK_to_%dK",
			 cachetype, i, (i+1));
		nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].object_size[i],
			    sizeof(attrcache.stats[l].object_size[i]));
	}
	snprintf(name, 100, "bm.global.bm.%s.hitbucket_gte_%d", cachetype, i);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].hit_bucket[i],
		    sizeof(attrcache.stats[l].hit_bucket[i]));
	snprintf(name, 100, "bm.global.bm.%s.objsizebucket_gte_%dK", cachetype, i);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].object_size[i],
		    sizeof(attrcache.stats[l].object_size[i]));
	for ( i = 0 ; i < (MAX_BUCKET_STATS - 1); ++i) {
		snprintf(name, 100, "bm.global.bm.%s.agebucket_%ds_to_%ds",
			 cachetype, i * 10, ((i + 1) * 10) - 1);
		nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].age_bucket[i],
			    sizeof(attrcache.stats[l].age_bucket[i]));
	}
	snprintf(name, 100, "bm.global.bm.%s.agebucket_gte_%ds", cachetype, i * 10);
	nkn_mon_add(name, NULL, (void *)&attrcache.stats[l].age_bucket[i],
		    sizeof(attrcache.stats[l].age_bucket[i]));

	for ( i = 0; i < NKN_MM_MAX_CACHE_PROVIDERS; ++i) {
		if ((i !=  SolidStateMgr_provider) && (i != SAS2DiskMgr_provider)&&
		    (i != SATADiskMgr_provider) && (i != OriginMgr_provider)
		    && (i != NFSMgr_provider))
			continue;
		const char *provider = MM_get_provider_name(i);

		snprintf(name, 100, "bm.provider.%s.%s.evict_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].evict,
			    sizeof(attrcache.p_stats[l][i].evict));
		snprintf(name, 100, "bm.provider.%s.%s.expire_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].expire, 
			    sizeof(attrcache.p_stats[l][i].expire));
		snprintf(name, 100, "bm.provider.%s.%s.discard_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].discard, 
			    sizeof(attrcache.p_stats[l][i].discard));
		snprintf(name, 100, "bm.provider.%s.%s.replace_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].replace, 
			    sizeof(attrcache.p_stats[l][i].replace));
		snprintf(name, 100, "bm.provider.%s.%s.cache_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].numcache, 
			    sizeof(attrcache.p_stats[l][i].numcache));
		snprintf(name, 100, "bm.provider.%s.%s.cache_32k_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].numcache_32,
			    sizeof(attrcache.p_stats[l][i].numcache_32));
		snprintf(name, 100, "bm.provider.%s.%s.cache_4k_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].numcache_4, 
			    sizeof(attrcache.p_stats[l][i].numcache_4));
		snprintf(name, 100, "bm.provider.%s.%s.cache_1k_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].numcache_1, 
			    sizeof(attrcache.p_stats[l][i].numcache_1));
		snprintf(name, 100, "bm.provider.%s.%s.dupid_cnt", cachetype, provider);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].dupid, 
			    sizeof(attrcache.p_stats[l][i].dupid));
		int j = 0;
		for ( ; j < (MAX_BUCKET_STATS / 2 - 1); ++j) {
			snprintf(name, 100, "bm.provider.%s.%s.hitbucket_%d", cachetype, provider, j);
			nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].hit_bucket[j],
				    sizeof(attrcache.p_stats[l][i].hit_bucket[j]));
			snprintf(name, 100, "bm.provider.%s.%s.objsizebucket_%dK_to_%dK",
				 cachetype, provider, j, (j + 1));
			nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].object_size[j],
				    sizeof(attrcache.p_stats[l][i].object_size[j]));
		}
		snprintf(name, 100, "bm.provider.%s.%s.hitbucket_gte_%d", cachetype, provider, j);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].hit_bucket[j],
			    sizeof(attrcache.p_stats[l][i].hit_bucket[j]));
		snprintf(name, 100, "bm.provider.%s.%s.objsizebucket_gte_%dK",
			 cachetype, provider, j);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].object_size[j],
			    sizeof(attrcache.p_stats[l][i].object_size[j]));
		for ( j = 0; j < (MAX_BUCKET_STATS - 1); ++j) {
			snprintf(name, 100, "bm.provider.%s.%s.agebucket_%ds_to_%ds",
				 cachetype, provider, j * 10, ((j + 1) * 10) - 1);
			nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].age_bucket[j],
				    sizeof(attrcache.p_stats[l][i].age_bucket[j]));
		}
		snprintf(name, 100, "bm.provider.%s.%s.agebucket_gte_%ds",
			 cachetype, provider, j * 10);
		nkn_mon_add(name, NULL, (void *)&attrcache.p_stats[l][i].age_bucket[j],
			    sizeof(attrcache.p_stats[l][i].age_bucket[j]));
	    }
	}


	// Initialize the COD with entries equal to attribute buffers plus 20%.
	// 10% extra allocated to handle traffic when system went to ideal
	//   case where num of attr = data buffers
	nkn_cod_init(cod_entries);
	i = 0;
	while (i < NKN_MM_MAX_CACHE_PROVIDERS) {
            AO_store(&provlistbytes[i], 0);
            ++i;
        }

	if (!bm_cfg_discard) {
	    glob_bm_sta_discardcost = 0;
	}
}
/*
 * Free list access routines.  In order to have a low overhead way or ordering
 * buffers in the free list (currently based on use count), we use multiple
 * list queues.  This lets us keep the insertion/removal time as constant.
 * Must be called with pool mutex locked.
 */
static
void nkn_buf_free_link(nkn_buf_t *bp)
{
	int i, flist0;
	nkn_buffer_freepool_t *pool;
	nkn_fp_head_t *buf_free;

	pool = nkn_buf_size_to_freepool(bp->bufsize, bp->ftype);

	/* Purge the object if it is cached and the COD is expired */
	if ((bp->flags & NKN_BUF_CACHE) &&
	    (bp->type == NKN_BUFFER_ATTR) &&
	    (nkn_cod_get_status(bp->id.cod) == NKN_COD_STATUS_EXPIRED)) {
		nkn_obj_purge((nkn_obj_t *)bp, NKN_BUF_DETACH_FROM_COD);
	}

	/* 
	 * If the buffer is not in the cache, we put it on the head of the
	 * of the lowest list for immediate reuse.
	 */
	flist0 = AO_fetch_and_add1(&pool->cur_freepool0_add) % MAX_ZEROPOOL;
	bp->flags &= ~NKN_BUF_ATTR_FLIST;
	if (!(bp->flags & NKN_BUF_CACHE)) {
		if (bp->flags & NKN_BUF_ID) {
			nkn_cod_close(bp->id.cod, NKN_COD_BUF_MGR);
			bp->id.cod = NKN_COD_NULL;
			bp->flags = 0;
		}
		buf_free = &pool->buf_free[0][flist0];
		bp->use_ts = 0;
		bp->flist = 0;
		bp->flist0 = flist0;
		if (bp->type == NKN_BUFFER_ATTR) {
		    bp->flags |= NKN_BUF_ATTR_FLIST;
		}
		bp->flags |= NKN_BUF_FREE_FLIST;
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&pool->buf_free_lock[0][flist0]);
#else
		pthread_mutex_lock(&pool->buf_free_lock[0][flist0]);
#endif
		if (bp->type == NKN_BUFFER_ATTR) {
		    AO_fetch_and_add1(&pool->stats.nattrfree);
		    TAILQ_INSERT_HEAD(&pool->buf_attrfree[0][flist0], bp, attrentries);
		}
		TAILQ_INSERT_HEAD(buf_free, bp, entries);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&pool->buf_free_lock[0][flist0]);
#else
		pthread_mutex_unlock(&pool->buf_free_lock[0][flist0]);
#endif
		AO_fetch_and_add1(&pool->nfree[0][flist0]);
		AO_fetch_and_add1(&pool->stats.numfree);
		return;
	}

	/*
	 * Find the lowest list where we are <= to the tail or the first 
	 * empty list.  Falls thru to the highest list.
	 */
	bp->use_ts = nkn_cur_100_tms & TIME40_MASK;
	if (bp->type == NKN_BUFFER_ATTR) {
		bp->flags |= NKN_BUF_ATTR_FLIST;
	}
	if ((bp->flags & NKN_BUF_GROUPLIST) && !bp->discard_ts) {
		bp->discard_ts = (bp->discard_cnt * glob_bm_sta_discardcost);
	}
	nkn_buf_t *ebp;
	for (i=0; i<NUM_FREE_LIST-1; flist0 = 0, i++) {
		buf_free = &pool->buf_free[i][flist0];
		ebp = (nkn_buf_t *)TAILQ_LAST(buf_free, nkn_freepool_head);
		if (!ebp || (bp->use <= ebp->use))
			break;
	}
	if (i == NUM_FREE_LIST-1) {
		buf_free = &pool->buf_free[i][flist0];
		/* Cap for Top use is done toavoid keeping hot object in BM
		 * Cap calc = Top - 1 bucket usecnt + constant 2 
		 * Reason for constant 2: In failure case usecnt will be
		 * decremented by 1 which forces this buffer to be at Top -1
                 * bucket. In order to avoid that constant 2 is taken
		 */
		bp->use = ebp->use + 2;
	}

	bp->flist = i;
	bp->flist0 = flist0;
	bp->flags |= NKN_BUF_FREE_FLIST;
	if (bp->type != NKN_BUFFER_ATTR) {
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&pool->buf_free_lock[i][flist0]);
#else
		pthread_mutex_lock(&pool->buf_free_lock[i][flist0]);
#endif
		TAILQ_INSERT_TAIL(buf_free, bp, entries);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&pool->buf_free_lock[i][flist0]);
#else
		pthread_mutex_unlock(&pool->buf_free_lock[i][flist0]);
#endif
	} else {
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&pool->buf_free_lock[i][flist0]);
#else
		pthread_mutex_lock(&pool->buf_free_lock[i][flist0]);
#endif
		TAILQ_INSERT_TAIL(buf_free, bp, entries);
	    	AO_fetch_and_add1(&pool->stats.nattrfree);
	    	TAILQ_INSERT_TAIL(&pool->buf_attrfree[i][flist0], bp, attrentries);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&pool->buf_free_lock[i][flist0]);
#else
		pthread_mutex_unlock(&pool->buf_free_lock[i][flist0]);
#endif

	}
	AO_fetch_and_add1(&pool->nfree[i][flist0]);
	AO_fetch_and_add1(&pool->stats.numfree);
}

static
void nkn_buf_free_unlink(nkn_buf_t *bp)
{
	uint8_t list = bp->flist;
	uint8_t flist0 = bp->flist0;
	int flag = bp->flags & NKN_BUF_ATTR_FLIST;
	nkn_buffer_freepool_t *pool;
	nkn_fp_head_t *buf_free;

	pool = nkn_buf_size_to_freepool(bp->bufsize, bp->ftype);

	assert(list < NUM_FREE_LIST);
	buf_free = &pool->buf_free[list][flist0];
	bp->flags &= ~NKN_BUF_FREE_FLIST;
	if (flag == 0) {
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&pool->buf_free_lock[list][flist0]);
#else
		pthread_mutex_lock(&pool->buf_free_lock[list][flist0]);
#endif
		TAILQ_REMOVE(buf_free, bp, entries);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&pool->buf_free_lock[list][flist0]);
#else
		pthread_mutex_unlock(&pool->buf_free_lock[list][flist0]);
#endif

	} else {
	    	bp->flags &= ~NKN_BUF_ATTR_FLIST;
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&pool->buf_free_lock[list][flist0]);
#else
		pthread_mutex_lock(&pool->buf_free_lock[list][flist0]);
#endif
	    	TAILQ_REMOVE(buf_free, bp, entries);
	    	TAILQ_REMOVE(&pool->buf_attrfree[list][flist0], bp, attrentries);
	    	AO_fetch_and_sub1(&pool->stats.nattrfree);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&pool->buf_free_lock[list][flist0]);
#else
		pthread_mutex_unlock(&pool->buf_free_lock[list][flist0]);
#endif
	}
	AO_fetch_and_sub1(&pool->nfree[list][flist0]);
	AO_fetch_and_sub1(&pool->stats.numfree);
}

/*
 * Find the lowest "cost" buffer in the LRU chains.  The "cost" is calculated
 * as a simple function of use count and age (we can make this more 
 * clever down the road).  We look at the cost of the head element in each LRU
 * chain.  This allows the older entries with a high use count to decay over
 * time.
 */
static
nkn_buf_t * nkn_buf_free_get(int bufsize, nkn_obj_t **objpp,
			     nkn_buffer_type_t type, int passtype,
			     int pooltype)
{
	nkn_buf_t *bp, *nbp, *nbp1, *nbp2;
	int i, list, cond_hit, flist0, cur_flist0;
	int64_t cost, ncost, ncost1, ncost2, currtick;
	nkn_buffer_freepool_t *pool;
	nkn_fp_head_t *buf_free;
	nkn_obj_t *obj_temp = NULL;
	nkn_fp_head_t (*buf_free_list)[NUM_FREE_LIST][MAX_ZEROPOOL];

attr_retry:
	pool = nkn_buf_size_to_freepool(bufsize, passtype);
	buf_free_list = &pool->buf_free;
	cond_hit = NKN_BUF_ATTRLIMIT_NORMAL;
	/* logical attr limit is done to keep cods in control*/
	if (type == NKN_BUFFER_ATTR) {
	    if (AO_load(&bm_cur_attrcnt[pooltype]) >
		    bm_tot_attrcnt[pooltype]) {
		    if (pooltype != passtype) {
			    passtype = pooltype;
			    goto attr_retry;
		    }
		    /* current attr pool is empty then try opposite side pool */
		    if (AO_load(&pool->stats.nattrfree) == 0) {
			    pool = nkn_buf_size_to_freepool(nkn_buf_type_to_default_size(NKN_BUFFER_ATTR),
							    passtype);
			    buf_free_list = &pool->buf_attrfree;
			    cond_hit = NKN_BUF_ATTRLIMIT_OPP;
		    } else {
			    /* get the current attr pool */
			    buf_free_list = &pool->buf_attrfree;
			    cond_hit = NKN_BUF_ATTRLIMIT_CUR;
		    }
	    }
	}
	cur_flist0 = AO_fetch_and_add1(&pool->cur_freepool0_remove) % MAX_ZEROPOOL;
retry:
	flist0 = cur_flist0;
	bp = NULL;
	bm_cfg_buf_lruscale = glob_bm_dyn_lruscale;
	currtick = nkn_cur_100_tms & TIME40_MASK;
	for (i=0; i<NUM_FREE_LIST; flist0 = 0, i++) {
		buf_free = &(*buf_free_list)[i][flist0];
		nbp2 = NULL;
		nbp = (nkn_buf_t *)TAILQ_FIRST(buf_free);
		if (cond_hit == NKN_BUF_ATTRLIMIT_NORMAL) {
			if (nbp) {
				nbp1 = (nkn_buf_t *)
					TAILQ_NEXT(nbp, entries);
				if (nbp1) {
					nbp2 = (nkn_buf_t *)
					    TAILQ_NEXT(nbp1, entries);
				}
			}
		} else {
			if (nbp) {
				nbp1 = (nkn_buf_t *)
					TAILQ_NEXT(nbp, attrentries);
				if (nbp1) {
					nbp2 = (nkn_buf_t *)
					    TAILQ_NEXT(nbp1, attrentries);
				}
			}
		}
		if (nbp) {
			// cost is use count minus age
			// cost lookup 1 deep
			ncost = (nbp->use * bm_cfg_buf_lruscale) -
				((int64_t)currtick - ((int64_t)nbp->use_ts)) +
				((!nbp->use) * nbp->discard_ts);
			if (bp) {
				if (ncost <= cost) {
				    bp = nbp;
				    cost = ncost;
				}
			} else { // no previous lookup so assign nbp becomes bp
				bp = nbp;
				cost = ncost;
			}
			// cost lookup 2 deep
			if (nbp1) {
				ncost1 = (nbp1->use * bm_cfg_buf_lruscale) -
				((int64_t)currtick - ((int64_t)nbp1->use_ts)) +
				((!nbp1->use) * nbp1->discard_ts);
				if (ncost1 < cost) {
					bp = nbp1;
					cost = ncost1;
				}
			}
			// cost lookup 3 deep
			if (nbp2) {
				ncost2 = (nbp2->use * bm_cfg_buf_lruscale) -
				((int64_t)currtick - ((int64_t)nbp2->use_ts)) +
				((!nbp2->use) * nbp2->discard_ts);
				if (ncost2 < cost) {
					bp = nbp2;
					cost = ncost2;
				}
			}
		}
	}
	if (bp) {
		// immediate check whether in list or not
		if (!(bp->flags & NKN_BUF_FREE_FLIST)) {
			goto retry;
		}
		pthread_mutex_lock(&bp->lock);
		if ((!(bp->flags & NKN_BUF_FREE_FLIST)) || (bp->ref != 0)) {
			pthread_mutex_unlock(&bp->lock);
			goto retry;
		}
		
		if (bp->flags & NKN_BUF_CACHE &&
			bp->type != NKN_BUFFER_ATTR)
		{
			obj_temp = ((nkn_page_t *)bp)->obj;
			if (obj_temp) {
				pthread_mutex_unlock(&bp->lock);
				pthread_mutex_lock(&obj_temp->buf.lock);
				pthread_mutex_lock(&bp->lock);
				if ((!(bp->flags & NKN_BUF_FREE_FLIST)) || (bp->ref != 0)) {
					pthread_mutex_unlock(&bp->lock);
					pthread_mutex_unlock(&obj_temp->buf.lock);
					obj_temp = NULL;
					glob_attr_normal2_retry++;
					goto retry;	
				}
			}
		}
		list = bp->flist;
		flist0 = bp->flist0;
		buf_free = &(*buf_free_list)[list][flist0];
		nbp1 = nbp2 = NULL;
		if (cond_hit == NKN_BUF_ATTRLIMIT_NORMAL) {
#ifdef LOCKSTAT_DEBUG
			NKN_MUTEX_LOCK(&pool->buf_free_lock[list][flist0]);
#else
			pthread_mutex_lock(&pool->buf_free_lock[list][flist0]);
#endif
			nbp = (nkn_buf_t *)TAILQ_FIRST(buf_free);
			if (nbp) {
				nbp1 = (nkn_buf_t *)TAILQ_NEXT(nbp, entries);
				if (nbp1)
					nbp2 = (nkn_buf_t *)
					    TAILQ_NEXT(nbp1, entries);
			}
			if ((nbp != bp && nbp1 != bp && nbp2 != bp)) {
#ifdef LOCKSTAT_DEBUG
			    NKN_MUTEX_UNLOCK(&pool->buf_free_lock[list][flist0]);
#else
			    pthread_mutex_unlock(&pool->buf_free_lock[list][flist0]);
#endif
			    pthread_mutex_unlock(&bp->lock);
			    if (obj_temp) {
				pthread_mutex_unlock(&obj_temp->buf.lock);
				obj_temp = NULL;
			    }
			    glob_attr_normal_retry++;
			    goto retry;
			}
			TAILQ_REMOVE(buf_free, bp, entries);
			if (bp->flags & NKN_BUF_ATTR_FLIST) {
			    TAILQ_REMOVE(&pool->buf_attrfree[list][flist0],
					  bp, attrentries);
			    AO_fetch_and_sub1(&pool->stats.nattrfree);
			    AO_fetch_and_sub1(&bm_cur_attrcnt[bp->ftype]);
			    AO_fetch_and_sub1(&pool->cur_attr);
			    bp->flags &= ~NKN_BUF_ATTR_FLIST;
			}
			if (type == NKN_BUFFER_ATTR) {
			    AO_fetch_and_add1(&bm_cur_attrcnt[pooltype]);
			    AO_fetch_and_add1(&pool->cur_attr);
			}
#ifdef LOCKSTAT_DEBUG
			NKN_MUTEX_UNLOCK(&pool->buf_free_lock[list][flist0]);
#else
			pthread_mutex_unlock(&pool->buf_free_lock[list][flist0]);
#endif
		} else {
#ifdef LOCKSTAT_DEBUG
			NKN_MUTEX_LOCK(&pool->buf_free_lock[list][flist0]);
#else
			pthread_mutex_lock(&pool->buf_free_lock[list][flist0]);
#endif
			nbp = (nkn_buf_t *)TAILQ_FIRST(buf_free);
			if (nbp) {
				nbp1 = (nkn_buf_t *)TAILQ_NEXT(nbp, attrentries);
				if (nbp1)
					nbp2 = (nkn_buf_t *)
					    TAILQ_NEXT(nbp1, attrentries);
			}
			if ((nbp != bp && nbp1 != bp && nbp2 != bp)) {
#ifdef LOCKSTAT_DEBUG
			    NKN_MUTEX_UNLOCK(&pool->buf_free_lock[list][flist0]);
#else
			    pthread_mutex_unlock(&pool->buf_free_lock[list][flist0]);
#endif
			    pthread_mutex_unlock(&bp->lock);
			    if (obj_temp) {
				pthread_mutex_unlock(&obj_temp->buf.lock);
				obj_temp = NULL;
			    }
			    glob_attr_abnormal_retry++;
			    goto retry;
			}
			assert(bp->flags & NKN_BUF_ATTR_FLIST);
			TAILQ_REMOVE(buf_free, bp, attrentries);
			TAILQ_REMOVE(&pool->buf_free[list][flist0], bp, entries);
			bp->flags &= ~NKN_BUF_ATTR_FLIST;
			AO_fetch_and_sub1(&pool->stats.nattrfree);
#ifdef LOCKSTAT_DEBUG
			NKN_MUTEX_UNLOCK(&pool->buf_free_lock[list][flist0]);
#else
			pthread_mutex_unlock(&pool->buf_free_lock[list][flist0]);
#endif

		}
		bp->flags &= ~NKN_BUF_FREE_FLIST;
		AO_fetch_and_sub1(&pool->nfree[list][flist0]);
		AO_fetch_and_sub1(&pool->stats.numfree);
		if (cond_hit == NKN_BUF_ATTRLIMIT_OPP) {
			AO_fetch_and_sub1(&bm_cur_attrcnt[pooltype]);
			AO_fetch_and_sub1(&pool->cur_attr);
			nkn_buffer_pool_t *pl= nkn_buf_type_to_pool(NKN_BUFFER_DATA);
			nkn_buf_internal_alloc(bp, NKN_BUFFER_DATA);
			AO_fetch_and_add1(&pl->stats[pooltype].numalloc);
			nkn_buf_deref(bp);
			pthread_mutex_unlock(&bp->lock);
			if (obj_temp) {
			    pthread_mutex_unlock(&obj_temp->buf.lock);
			    obj_temp = NULL;
			}
			glob_attr_abnormal_retry++;
			goto attr_retry;
		}
		*objpp = obj_temp;
		if (pooltype == BM_NEGCACHE_ALLOC && 
			pooltype != passtype) {
			pool = nkn_buf_size_to_freepool(bufsize, pooltype);
			AO_fetch_and_add1(&pool->cur_bufs);
		}
	}
	return bp;
}

static
int nkn_buf_page_alloc(nkn_buf_t *bp, int rsize)
{
	int size = rsize, poolnum;
	if (bp->buffer)
		return 0;
	if (!size)
		size = nkn_buf_type_to_default_size(bp->type);
	poolnum = nkn_buf_size_to_freepoolnum(size);
	bp->buffer = nkn_page_alloc(poolnum);
	if (bp->buffer == 0)
		return -1;
	bp->bufsize = size;
	return 0;
}

/*
 * Add ref count and reclain from free list (if needed).  Called with
 * global lock held.
 */
static
void nkn_buf_ref(nkn_buf_t *bp, nkn_buffer_pool_t *pool, nkn_uol_t *uol, int flags)
{
	if (bp->ref == 0) {
		/* reclaim from free list */
		nkn_buf_free_unlink(bp);
		AO_fetch_and_add1(&pool->stats[bp->ftype].reclaim);
	}
	bp->ref++;
	if (!(flags & NKN_BUF_IGNORE_STATS)) {
		// We count a hit only if the buffer has previously been used
		if (uol) {
                        off_t uolrange =  ((uol->length == 0)? bp->id.offset + bp->id.length:
                                          uol->offset + uol->length);
			if (uolrange < (bp->id.offset + bp->id.length)) {
				if (uolrange > bp->endhit) {
					bp->endhit = uolrange;
				}
    			} else {
				bp->endhit = bp->id.offset + bp->id.length;
			}
		}
		if (bp->use)
			AO_fetch_and_add1(&pool->stats[bp->ftype].hits);
		else {
			AO_fetch_and_add1(&pool->stats[bp->ftype].misses);
		}
		bp->use++;
	}
}

/*
 * Decrement ref count and add to the free list (if needed).  Called with
 * global lock held.
 */
static
void nkn_buf_deref(nkn_buf_t *bp)
{
	bp->ref--;
	if (bp->ref == 0) {
		/* add to free list */
		nkn_buf_free_link(bp);
	}
}

/* 
 * Link a page into an object 
 * Called with global lock held
 */
static
void nkn_page_link(nkn_obj_t *objp, nkn_page_t *pgp)
{
	nkn_buf_t *obp = (nkn_buf_t *)objp;

	assert(pgp->nextpage == NULL);
	assert(pgp->prevpage == NULL);
	/* Always adds to the head */
	pgp->nextpage = objp->pglist;
	if (objp->pglist)
		objp->pglist->prevpage = pgp;
	objp->pglist = pgp;
	objp->pagecnt++;

	/*
 	 * Each page holds a ref on the object, so bump up the ref
	 * count but don't treat it as an access for stats.
	 */
	if (obp->ref == 0)
		nkn_buf_free_unlink(obp);
	((nkn_buf_t *)objp)->ref++;
	if (objp->pagecnt == 1)	
		glob_bm_refcnted_attribute_cache++;
	pgp->obj = objp;
}

/* 
 * Unlink a page from an object
 * Called with global lock held
 */
static
void nkn_page_unlink(nkn_page_t *pgp)
{
	nkn_obj_t *objp = pgp->obj;
	nkn_buf_t *obp = (nkn_buf_t *)objp;

	if (pgp->nextpage)
		pgp->nextpage->prevpage = pgp->prevpage;
	if (pgp->prevpage)
		pgp->prevpage->nextpage = pgp->nextpage;
	else
		objp->pglist = pgp->nextpage;	// was first page
	pgp->nextpage = NULL;
	pgp->prevpage = NULL;
	pgp->obj = NULL;
	assert(objp->pagecnt);
	if (objp->pagecnt == 1)
		glob_bm_refcnted_attribute_cache--;
	objp->pagecnt--;
	// each page holds a ref on the object
	nkn_buf_deref(obp);
}

/* Remove object from global hash.  Called with global lock held. */
static
void nkn_obj_remove(nkn_obj_t *objp, int detach)
{
	nkn_buf_t *bp = (nkn_buf_t *)objp;
	int ret, ftype = bp->ftype;

	bp->flags &= ~NKN_BUF_CACHE;
	assert(objp->pagecnt == 0);
	if (detach) {
	    ret = nkn_cod_attr_detach(bp->id.cod, (nkn_buffer_t *)bp);
	    assert(ret == 0);
	}
	objp->flags = 0;
	AO_fetch_and_sub1(&attrcache.stats[ftype].numcache);
	AO_fetch_and_sub1(&attrcache.p_stats[ftype][bp->provider].numcache);
	if (bp->bufsize == CM_MEM_PAGE_SIZE){
		AO_fetch_and_sub1(&attrcache.stats[ftype].numcache_32);
		AO_fetch_and_sub1(
			&attrcache.p_stats[ftype][bp->provider].numcache_32);
	} else {
		if (bp->bufsize == CM_MEM_MIN_NONSMALLATTR_PAGE_SIZE) {
			AO_fetch_and_sub1(&attrcache.stats[ftype].numcache_4);
			AO_fetch_and_sub1(
				&attrcache.p_stats[ftype][bp->provider].numcache_4);
		} else {
			AO_fetch_and_sub1(&attrcache.stats[ftype].numcache_1);
			AO_fetch_and_sub1(
				&attrcache.p_stats[ftype][bp->provider].numcache_1);
		}
	}
}

/* 
 * Remove page from global hash and the object page list.
 * Called with global lock held. 
 */
static
void nkn_page_remove(nkn_page_t *pgp)
{
	nkn_buf_t *bp = (nkn_buf_t *)pgp;
	nkn_obj_t *bp_attr;
	uint32_t hindex, ftype = bp->ftype;

	bp->flags &= ~NKN_BUF_CACHE;
	if ((pgp->buf.id.offset >= 0) &&
		(pgp->buf.id.offset < CM_MEM_PAGE_SIZE)) {
		bp_attr = (nkn_obj_t *)
			nkn_cod_attr_lookup(pgp->buf.id.cod);
		/* during object setid call if cod is expired then cod_attr_attach
		   should fail. In this case bp_attr can be null. so bp_attr null check
		   is needed
		*/
		if (bp_attr && (bp_attr == pgp->obj)) {
			NKN_ASSERT(bp_attr->page0 != NULL);
			bp_attr->page0 = NULL;
		}
	} else {
		gboolean found;
		hindex = pgp->buf.id.cod % MAX_SETID_HASH;
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&bufcache.buf_hashlock[hindex]);
#else
		pthread_mutex_lock(&bufcache.buf_hashlock[hindex]);
#endif
		found = g_hash_table_remove(bufcache.buf_hash[hindex], pgp->hashid);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&bufcache.buf_hashlock[hindex]);
#else
		pthread_mutex_unlock(&bufcache.buf_hashlock[hindex]);
#endif
		NKN_ASSERT(found == TRUE);
	}
	nkn_page_unlink(pgp);
	AO_fetch_and_sub1(&bufcache.stats[ftype].numcache);
	AO_fetch_and_sub1(&bufcache.p_stats[ftype][bp->provider].numcache);
}

static
void nkn_page_add(nkn_uol_t *uol, nkn_obj_t * objp,
			nkn_page_t *pgp)
{
	nkn_buf_t *bp = (nkn_buf_t *)pgp;
	uint32_t hindex, ftype = bp->ftype;
	if ((uol->offset >= 0) && (uol->offset < CM_MEM_PAGE_SIZE))
		objp->page0 = (nkn_page_t *)bp;
	else {
		hindex = uol->cod % MAX_SETID_HASH;
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_LOCK(&bufcache.buf_hashlock[hindex]);
#else
		pthread_mutex_lock(&bufcache.buf_hashlock[hindex]);
#endif
		g_hash_table_insert(bufcache.buf_hash[hindex], pgp->hashid, bp);
#ifdef LOCKSTAT_DEBUG
		NKN_MUTEX_UNLOCK(&bufcache.buf_hashlock[hindex]);
#else
		pthread_mutex_unlock(&bufcache.buf_hashlock[hindex]);
#endif
	}
	nkn_page_link(objp, pgp);
	bp->flags |= NKN_BUF_CACHE;
	AO_fetch_and_add1(&bufcache.stats[ftype].numcache);
	AO_fetch_and_add1(&bufcache.p_stats[ftype][bp->provider].numcache);
}

static
nkn_page_t *nkn_page0_lookup(nkn_obj_t *objp)
{
	nkn_page_t * temp = NULL;
	if (objp) {
		temp = objp->page0;
	}
	return temp;
}

static
nkn_page_t *nkn_pagen_lookup(nkn_uol_t *uol, nkn_buffer_pool_t * pool)
{
	nkn_page_t *pgp;
	char id[MAXBUFID];
	uint32_t hindex;
	uol_to_bufid(uol, NKN_BUFFER_DATA, id);
	hindex = uol->cod % MAX_SETID_HASH;
#ifdef LOCKSTAT_DEBUG
	NKN_MUTEX_LOCK(&bufcache.buf_hashlock[hindex]);
#else
	pthread_mutex_lock(&bufcache.buf_hashlock[hindex]);
#endif
	pgp = g_hash_table_lookup(pool->buf_hash[hindex], id);
#ifdef LOCKSTAT_DEBUG
	NKN_MUTEX_UNLOCK(&bufcache.buf_hashlock[hindex]);
#else
	pthread_mutex_unlock(&bufcache.buf_hashlock[hindex]);
#endif
	return pgp;
}

/*
 * Set the reval timespamp.  Currently set to 1/8 of expiry time.
 */
static void
nkn_buf_set_reval(nkn_obj_t *objp)
{
	nkn_attr_t *ap;
	time_t val;

	ap = (nkn_attr_t *)(objp->buf.buffer);
	if (ap->cache_expiry <= nkn_cur_ts)
		return;
	val = ap->cache_expiry - ((ap->cache_expiry - nkn_cur_ts) >> 3);
	objp->reval_ts = val & TIME32_MASK;
}

/*
 * Check if the buffer has expired based on the expiry time in the attributes.
 */
int nkn_buf_has_expired(nkn_buf_t *bp)
{
	nkn_obj_t *objp;
	nkn_attr_t *ap;

	if (bp->type == NKN_BUFFER_ATTR)
		objp = (nkn_obj_t *)bp;
	else
		objp = ((nkn_page_t *)bp)->obj;
	if (!objp)		// missing attributes => expired
		return 1;
	if (!(objp->flags & NKN_OBJ_VALID))
		return 0;
	// Treat non-cacheable as always valid (origin can set headers that
	// make them look expired).
	if (!(bp->flags & NKN_BUF_CACHE))
		return 0;

	ap = (nkn_attr_t *)(objp->buf.buffer);
	
	// its expired if current time has reached the expiry time.
	if ((ap->cache_expiry <= nkn_cur_ts) && (ap->cache_expiry > 0))
		return 1;
	return 0;
}


/*
 * stats function
 */
static
void nkn_obj_remove_stats(uint64_t use, uint64_t length, uint32_t createtime,
			  nkn_provider_type_t provider, int ftype)
{
	AO_fetch_and_add1(&attrcache.stats[ftype].evict);
	AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].evict);
	if (use == 0) {
		AO_fetch_and_add1(&attrcache.stats[ftype].discard);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].discard);
	}
	if (use >= 64) {
		AO_fetch_and_add1(&attrcache.stats[ftype].hit_bucket[63]);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].hit_bucket[63]);
	} else {
		AO_fetch_and_add1(&attrcache.stats[ftype].hit_bucket[use]);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].hit_bucket[use]);
	}
	long long buflength = length / 1024;
	if (buflength >= 64) {
		AO_fetch_and_add1(&attrcache.stats[ftype].object_size[63]);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].object_size[63]);
	} else {
		AO_fetch_and_add1(&attrcache.stats[ftype].object_size[buflength]);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].object_size[buflength]);
	}
	long long age = ((nkn_cur_ts & TIME32_MASK) - createtime) / 10;
	if (age >= 128) {
		AO_fetch_and_add1(&attrcache.stats[ftype].age_bucket[127]);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].age_bucket[127]);
	} else {
		AO_fetch_and_add1(&attrcache.stats[ftype].age_bucket[age]);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].age_bucket[age]);
	}
}
static
void nkn_buf_remove_stats(uint64_t length, uint64_t size,
			  nkn_provider_type_t provider, uint64_t use,
			  uint32_t createtime, int ftype)
{
	AO_fetch_and_add(&bufcache.stats[ftype].evict_bytes, length);
	AO_fetch_and_add(&bufcache.p_stats[ftype][provider].evict_bytes, length);
	AO_fetch_and_add(&bufcache.stats[ftype].c_fill_bytes, -length);
	AO_fetch_and_add(&bufcache.p_stats[ftype][provider].c_fill_bytes, -length);
	AO_fetch_and_add(&bufcache.stats[ftype].c_buffer_bytes, -size);
	AO_fetch_and_add(&bufcache.p_stats[ftype][provider].c_buffer_bytes, -size);
	AO_fetch_and_add1(&bufcache.stats[ftype].evict);
	AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].evict);
	if (use == 0) {
		AO_fetch_and_add1(&bufcache.stats[ftype].discard);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].discard);
	}
	if (use >= 64) {
		AO_fetch_and_add1(&bufcache.stats[ftype].hit_bucket[63]);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].hit_bucket[63]);
	} else {
		AO_fetch_and_add1(&bufcache.stats[ftype].hit_bucket[use]);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].hit_bucket[use]);
	}
	long long buflength = length / 1024;
	if (buflength >= 64) {
		AO_fetch_and_add1(&bufcache.stats[ftype].object_size[63]);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].object_size[63]);
	} else {
		AO_fetch_and_add1(&bufcache.stats[ftype].object_size[buflength]);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].object_size[buflength]);
	}
	long long age = ((nkn_cur_ts & TIME32_MASK) - createtime) / 10;
	if (age >= 128) {
		AO_fetch_and_add1(&bufcache.stats[ftype].age_bucket[127]);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].age_bucket[127]);
	} else {
		AO_fetch_and_add1(&bufcache.stats[ftype].age_bucket[age]);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][provider].age_bucket[age]);
	}
}

static
void nkn_buf_add_stats(uint64_t length, uint64_t size,
		       nkn_provider_type_t provider, int ftype)
{
	AO_fetch_and_add(&bufcache.stats[ftype].fill_bytes, length);
	AO_fetch_and_add(&bufcache.p_stats[ftype][provider].fill_bytes, length);
	AO_fetch_and_add(&bufcache.stats[ftype].c_fill_bytes, length);
	AO_fetch_and_add(&bufcache.p_stats[ftype][provider].c_fill_bytes, length);
	AO_fetch_and_add(&bufcache.stats[ftype].c_buffer_bytes, size);
	AO_fetch_and_add(&bufcache.p_stats[ftype][provider].c_buffer_bytes, size);
}



/*
 * Purge specified object from the cache.  Called with global lock held.
 */
static
void nkn_obj_purge(nkn_obj_t *objp, int detach)
{
	nkn_page_t *pgp;
	nkn_buf_t *bp;
	nkn_attr_t *ap;
	int ftype;

	bp = (nkn_buf_t *)objp;
	// Remove each page followed by the attributes
	AO_fetch_and_add(&bufcache.stats[bp->ftype].expire ,objp->pagecnt);
	while ((pgp = objp->pglist) != NULL) {
		pthread_mutex_lock(&pgp->buf.lock);
		bp = (nkn_buf_t *)pgp;
		ftype = bp->ftype;
		nkn_buf_remove_stats(bp->id.length, bp->bufsize,
				     bp->provider, bp->use,
				     bp->create_ts, ftype);
		AO_fetch_and_add(&provlistbytes[bp->provider],
				-bp->id.length);
		AO_fetch_and_add1(&bufcache.p_stats[ftype][bp->provider].expire);
		AO_fetch_and_add(&bufcache.stats[ftype].expired_bytes, bp->id.length);
		AO_fetch_and_add(&bufcache.p_stats[ftype][bp->provider].expired_bytes,
				 bp->id.length);
		nkn_page_remove(pgp);	// also updates pglist
		pthread_mutex_unlock(&pgp->buf.lock);
	}
	// Its possible for the object to have been removed from the cache
	// after unlinking all the pages.
	bp = (nkn_buf_t *)objp;
	ftype = bp->ftype;
	if (objp->buf.flags & NKN_BUF_CACHE) {
		ap = (nkn_attr_t *)(objp->buf.buffer);
		nkn_obj_remove_stats(bp->use, ap->content_length,
				     bp->create_ts, bp->provider, ftype);
		nkn_obj_remove(objp, detach);
	}
	AO_fetch_and_add1(&attrcache.p_stats[ftype][bp->provider].expire);
	AO_fetch_and_add1(&attrcache.stats[ftype].expire);
}

/*
 * Check if the buffer has expired based on the expiry time in the attrinbutes.
 * If it has expired, all the buffers (attr and data) for this object are
 * removed from the cache.  They may still be used by tasks that have already
 * acquired a refrerence count.  They buffer will be released back to the
 * free pool when any existing refs as released.
 */
void nkn_buf_expire(nkn_buf_t *bp)
{
	nkn_obj_t *objp;
	nkn_attr_t *ap;

	if (bp->type == NKN_BUFFER_ATTR)
		objp = (nkn_obj_t *)bp;
	else
		objp = ((nkn_page_t *)bp)->obj;

	pthread_mutex_lock(&objp->buf.lock);
	if (!(objp->flags & NKN_OBJ_VALID)) {
		pthread_mutex_unlock(&objp->buf.lock);
		return;
	}
	ap = (nkn_attr_t *)(objp->buf.buffer);
	
	// its expired if current time has reached the expiry time.
	if (ap->cache_expiry > nkn_cur_ts) {
		pthread_mutex_unlock(&objp->buf.lock);
		return;
	}

	/* 
	 * Mark the COD as expired so that subsequent requests can pickup
	 * a new version.  We do not purge the existing version here to allow
	 * existing readers to continue.  The purge will be done when the
	 * ref count for this version goes to zero.
	 */
	nkn_cod_set_expired(bp->id.cod);
	pthread_mutex_unlock(&objp->buf.lock);
}

nkn_buf_t *nkn_buf_getdata(nkn_uol_t *uol, int flags, int *use,
			   nkn_uol_t *actuol, off_t *endhit)
{
	nkn_buf_t *bp, *temp_bp;
	nkn_obj_t *objp = (nkn_obj_t *)nkn_cod_attr_lookup(uol->cod);
	if (objp) {
		pthread_mutex_lock(&objp->buf.lock);
		if (!(objp->flags & NKN_OBJ_VALID) ||
			(objp->buf.id.cod != uol->cod)) {
			pthread_mutex_unlock(&objp->buf.lock);
			return NULL;
		}
	}
	if ((uol->offset >= 0) && (uol->offset < CM_MEM_PAGE_SIZE)) {
		bp = (nkn_buf_t *)nkn_page0_lookup(objp);
	} else {
		bp = (nkn_buf_t *)nkn_pagen_lookup(uol, &bufcache);
	}
	if (bp) {
		temp_bp = bp;
		pthread_mutex_lock(&bp->lock);
		assert(bp->flags & NKN_BUF_CACHE);
		assert(bp->flags & NKN_BUF_ID);
		if (!(flags & NKN_BUF_IGNORE_EXPIRY) &&
			nkn_buf_has_expired(bp))
			bp = NULL;
		else {
			*endhit = bp->endhit;
			nkn_buf_ref(bp, &bufcache, actuol, flags);
			*use = bp->use;
		}
		pthread_mutex_unlock(&temp_bp->lock);
	}
	if (objp) {
		pthread_mutex_unlock(&objp->buf.lock);
	}
	return bp;
}

/* 
 * Get:  return a buffer if one exists, null otherwise.  The returned
 * buffer is reference counted and must be returned via a put call.
 */
nkn_buf_t *nkn_buf_get(nkn_uol_t *uol, nkn_buffer_type_t type, int flags)
{
	nkn_buffer_pool_t *pool;
	nkn_buf_t *bp, *temp_bp;
	nkn_obj_t *objp;
	pool = nkn_buf_type_to_pool(type);
	if (type == NKN_BUFFER_ATTR) {
		bp = (nkn_buf_t *)nkn_cod_attr_lookup(uol->cod);
		if (bp) {
			temp_bp = bp;
			pthread_mutex_lock(&bp->lock);
			if (!(((nkn_obj_t *)bp)->flags & NKN_OBJ_VALID) ||
			    (bp->id.cod != uol->cod)) {
				bp = NULL;
			} else {
				assert(bp->flags & NKN_BUF_CACHE);
				assert(bp->flags & NKN_BUF_ID);
				if (!(flags & NKN_BUF_IGNORE_EXPIRY) &&
					nkn_buf_has_expired(bp))
					bp = NULL;
				else
					nkn_buf_ref(bp, pool, NULL, flags);
			}
			pthread_mutex_unlock(&temp_bp->lock);
		}
	} else {
		objp = (nkn_obj_t *)
                                nkn_cod_attr_lookup(uol->cod);
		if (objp) {
			pthread_mutex_lock(&objp->buf.lock);
			if (!(objp->flags & NKN_OBJ_VALID) ||
			    (objp->buf.id.cod != uol->cod)) {
				pthread_mutex_unlock(&objp->buf.lock);
				return NULL;
			}
		}
		if ((uol->offset >= 0) && (uol->offset < CM_MEM_PAGE_SIZE)) {
			bp = (nkn_buf_t *)nkn_page0_lookup(objp);
		} else {
			bp = (nkn_buf_t *)nkn_pagen_lookup(uol, pool);
		}
		if (bp) {
			temp_bp = bp;
			pthread_mutex_lock(&bp->lock);
			assert(bp->flags & NKN_BUF_CACHE);
			assert(bp->flags & NKN_BUF_ID);
			if (!(flags & NKN_BUF_IGNORE_EXPIRY) &&
				nkn_buf_has_expired(bp))
				bp = NULL;
			else
				nkn_buf_ref(bp, pool, NULL, flags);
			pthread_mutex_unlock(&temp_bp->lock);
		}
		if (objp) {
			pthread_mutex_unlock(&objp->buf.lock);
		}
	}
	return bp;
}

/*
 * Check:  return one if the buffer exists.
 */
int nkn_buf_check(nkn_uol_t *uol, nkn_buffer_type_t type, int flags)
{
	nkn_obj_t *objp;
	int exists = 0;

	objp = (nkn_obj_t *)nkn_cod_attr_lookup(uol->cod);
	if (objp) {
		pthread_mutex_lock(&objp->buf.lock);
		if (!(objp->flags & NKN_OBJ_VALID) ||
			(objp->buf.id.cod != uol->cod)) {
			pthread_mutex_unlock(&objp->buf.lock);
			return exists;
		}
		if (type == NKN_BUFFER_ATTR) {
				exists = 1;
		// avoid the lookup if the bytes cached is less than 
		// the offset.
		} else {
			nkn_buf_t *bp;
			nkn_buffer_pool_t *pool = nkn_buf_type_to_pool(type);
			if ((uol->offset >= 0) && (uol->offset < CM_MEM_PAGE_SIZE)) {
				bp = (nkn_buf_t *)nkn_page0_lookup(objp);
			} else {
				bp = (nkn_buf_t *)nkn_pagen_lookup(uol, pool);
			}
			if (bp) {
				// 1. do offset match first.
				// 2. once offset match passed check the content is within
				//    the same buffer or
				// 3. once offset match passed check the content requested
				//    crossing the current lookedup buffer
				// 4. If 2 or 3 passes then check for expiry and once 
				//    not expired then mark hit.
				pthread_mutex_lock(&bp->lock);
				if ((bp->id.offset == uol->offset) &&
					((uol->length <= bp->id.length) ||
					((uol->length > bp->id.length) &&
					 ((bp->id.offset % bp->bufsize) + bp->id.length ==
					  bp->bufsize))) &&
					((flags & NKN_BUF_IGNORE_EXPIRY) ||
					!nkn_buf_has_expired(bp)))
					exists = 1;
				pthread_mutex_unlock(&bp->lock);
			}
		}
		pthread_mutex_unlock(&objp->buf.lock);
	}
	return exists;
}

/*
 * Dup:  Get additional ref count.
 */
void nkn_buf_dup(nkn_buf_t *bp, int flags, int *use,
		 nkn_uol_t *uol, nkn_uol_t *actuol, off_t *endhit)
{
	nkn_buf_t *tempbp;
	nkn_buffer_pool_t *pool;
	int cflag;
	pthread_mutex_lock(&bp->lock);
	assert(bp->ref);
	bp->ref++;
	cflag = bp->flags & NKN_BUF_CACHE;
	if (!(flags & NKN_BUF_IGNORE_STATS)) {
		if (cflag) {
			pool = nkn_buf_type_to_pool(bp->type);
                    	if (bp->use)
                        	AO_fetch_and_add1(&pool->stats[bp->ftype].hits);
                	else {
                        	AO_fetch_and_add1(&pool->stats[bp->ftype].misses);
                	}
			if (actuol) {
				*endhit = bp->endhit;
                        	off_t uolrange =  ((actuol->length == 0)? bp->id.offset + bp->id.length:
                                          	  actuol->offset + actuol->length);
				if (uolrange < (bp->id.offset + bp->id.length)) {
					if (uolrange > bp->endhit) {
						bp->endhit = uolrange;
					}
				} else {
					bp->endhit = bp->id.offset + bp->id.length;
				}
			}
		}
		*use = ++bp->use;
	}
	pthread_mutex_unlock(&bp->lock);
	if (actuol && !(flags & NKN_BUF_IGNORE_STATS) &&
		!(cflag)) {
		tempbp = nkn_buf_getdata(uol, flags, &cflag, actuol, endhit);
		if (tempbp) {
			nkn_buf_put(tempbp);
		} 
	}
}

/*
 * Put:  return a buffer previously acquired via get.
 */

static void
nkn_buf_put_common(nkn_buf_t *bp, int flags, int dis_index, int64_t endhit)
{
	nkn_buffer_pool_t *pool;
	pthread_mutex_lock(&bp->lock);
	ZVM_ENQUEUE(bp);
	assert(bp->ref);
	bp->discard_cnt = dis_index;
	if (!(flags & NKN_BUF_IGNORE_STATS)) {
		pool = nkn_buf_type_to_pool(bp->type);
		--bp->use;
		if (bp->use)
			AO_fetch_and_sub1(&pool->stats[bp->ftype].hits);
		else
			AO_fetch_and_sub1(&pool->stats[bp->ftype].misses);
		if (endhit != -1)
			bp->endhit = endhit;
	}
	nkn_buf_deref(bp);
	pthread_mutex_unlock(&bp->lock);
}

void nkn_buf_put(nkn_buf_t *bp)
{
	nkn_buf_put_common(bp, NKN_BUF_IGNORE_STATS, 0, -1);
}

void nkn_buf_put_downhits(nkn_buf_t *bp, int flags, off_t endhit)
{
	nkn_buf_put_common(bp, flags, 0, endhit);
}

void nkn_buf_put_discardid(nkn_buf_t *bp, int i)
{
	nkn_buf_put_common(bp, NKN_BUF_IGNORE_STATS, i, -1);	
}


static
void nkn_buf_internal_alloc(nkn_buf_t *bp,
			    nkn_buffer_type_t type)
{
	nkn_attr_t *ap;
	uint64_t length;
	assert(bp->ref == 0);
	/* remove from cache if it has an ID */
	if (bp->flags & NKN_BUF_CACHE) {
		if (bp->type == NKN_BUFFER_ATTR) {
			nkn_obj_t *objp = (nkn_obj_t *)bp;

			objp->pagecnt = 0;
			ap = (nkn_attr_t *)(objp->buf.buffer);
			nkn_obj_remove_stats(bp->use, ap->content_length,
					     bp->create_ts, bp->provider,
					     bp->ftype);
			nkn_obj_remove((nkn_obj_t *)bp,
				       NKN_BUF_DETACH_FROM_COD);
		} else {
			nkn_page_t *pgp = (nkn_page_t *)bp;
			length = bp->id.length;

			assert(bp->type == NKN_BUFFER_DATA);
			if (pgp->obj->bytes_cached >= length) {
				pgp->obj->bytes_cached -= length;
			} else {
				pgp->obj->bytes_cached = 0;
				glob_nkn_buf_acct_err++;
			}
			nkn_buf_remove_stats(length, bp->bufsize,
					     bp->provider, bp->use,
					     bp->create_ts, bp->ftype);
			AO_fetch_and_add(&provlistbytes[bp->provider],
					 -length);
			nkn_page_remove((nkn_page_t *)bp);
		}
		NKN_TRACE1("buf_rem", bp);
	}
	if (bp->flags & NKN_BUF_ID) {
		nkn_cod_close(bp->id.cod, NKN_COD_BUF_MGR);
		bp->id.cod = NKN_COD_NULL;
	}
	bp->flags = 0;		/* no id, not in cache */
	bp->ref = 1;
	bp->use = 0;
	bp->endhit = 0;
	bp->type = type;
	bp->create_ts = 0;
	bp->provider = 0;
	bp->discard_ts = 0;
	bp->discard_cnt = 0;
	// Since the buffer can switch type, we must initialize
	// the type specific fields that need to have a known value
	if (type == NKN_BUFFER_ATTR) {
		nkn_obj_t *objp = (nkn_obj_t *)bp;
		objp->pglist = NULL;
		objp->bytes_cached = 0;
		objp->pagecnt = 0;
		objp->flags = 0;
		objp->validate_pending = 0;
		objp->am_hits = 0;
		objp->am_ts = 0;
		objp->page0 = NULL;
		TAILQ_INIT(&objp->revalq);
	} else {
		nkn_page_t *pgp = (nkn_page_t *)bp;
		pgp->nextpage = NULL;
		pgp->prevpage = NULL;
		pgp->obj = NULL;
	}
#ifdef NKN_BUFFER_PARANOIA
	mprotect(bp->buffer, CM_MEM_PAGE_SIZE, PROT_READ|PROT_WRITE);
#endif

}

/*
 * Internal allocation routine.  Called with global lock held.
 */
static
nkn_buf_t *nkn_buf_alloc_locked(nkn_buffer_type_t type, int rsize, size_t flags)
{
	nkn_buf_t *bp;
	nkn_obj_t *objpp = NULL;
	nkn_buffer_freepool_t *sizepool;
	int passtype;
	int pooltype = passtype = flags & BM_MAXCACHE_ALLOC;
	nkn_buffer_pool_t *pool = nkn_buf_type_to_pool(type);
	int bufsize = rsize ? rsize : nkn_buf_type_to_default_size(type);
	sizepool = nkn_buf_size_to_freepool(bufsize, pooltype);

	if ((flags & BM_PREREAD_ALLOC) && type == NKN_BUFFER_ATTR) {
		if (AO_load(&sizepool->cur_attr) >= sizepool->tot_attr) {
			return NULL;
		}
	}
	if (pooltype == BM_NEGCACHE_ALLOC) {
		if (AO_load(&sizepool->cur_bufs) < sizepool->tot_bufs) {
			passtype = BM_CACHE_ALLOC;
		}
	}
	bp = nkn_buf_free_get(bufsize, &objpp, type, passtype, pooltype);  // object locked/buf locked
	if (bp) {
		nkn_buf_internal_alloc(bp, type);
		AO_fetch_and_add1(&pool->stats[pooltype].numalloc);
		bp->bufsize = sizepool->bufsize;
		bp->ftype = pooltype;
		pthread_mutex_unlock(&bp->lock);
		if (objpp)
		    pthread_mutex_unlock(&objpp->buf.lock);
	} else
		AO_fetch_and_add1(&pool->stats[pooltype].allocfail);
                ZVM_DEQUEUE(bp);
	return bp;
}

/*
 * Alloc:  Allocate an anonymous buffer.  Returns a buffer or NULL if no
 * buffers are available.  The returned buffer is reference counted and
 * must be returned via a put.  The buffer can be assigned an ID 
 * with the set call.
 */
nkn_buf_t *nkn_buf_alloc(nkn_buffer_type_t type, int size, size_t flags)
{
	nkn_buf_t *bp;
	bp = nkn_buf_alloc_locked(type, size, flags);
	if (!bp)
		return NULL;
	if (nkn_buf_page_alloc(bp, bp->bufsize)) {
		nkn_buf_put(bp);
		return NULL;
	}
	if (type == NKN_BUFFER_ATTR)
		nkn_attr_init((nkn_attr_t *)bp->buffer, bp->bufsize);
	return bp;
}

/*
 * Set identity for an object buffer.  Called with global lock held.
 */
static
int nkn_obj_setid(nkn_obj_t *objp, nkn_uol_t *uol, nkn_provider_type_t provider, u_int32_t flags)
{
	nkn_buf_t *ebp, *ebp_lock, *bp = (nkn_buf_t *)objp;
	nkn_attr_t *ap = (nkn_attr_t *)bp->buffer;
#ifdef TMPCOD
	int ret;
#endif
	nkn_buffer_pool_t *pool = &attrcache;
	int retcode, ftype = bp->ftype;

	assert(!(bp->flags & NKN_BUF_ID));
	bp->id.offset = 0;
	bp->id.length = 0;
	bp->id.cod = nkn_cod_dup(uol->cod, NKN_COD_BUF_MGR);
	assert(bp->id.cod != NKN_COD_NULL);
#ifdef TMPCOD
	ret = nkn_cod_test_and_set_version(uol->cod, ap->obj_version);
	assert((ret == 0) || ((ap->obj_version.objv_l[0] == 0) &&
			      (ap->obj_version.objv_l[0] == 0)));
	(void)nkn_cod_get_cn(bp->id.cod, &bp->id.uri, 0);
#endif
	bp->flags |= NKN_BUF_ID;

	objp->orig_hotval = ap->hotval;
	if (objp->orig_hotval == 0)
		AM_init_hotness(&objp->orig_hotval);
	
	/* Support for uncached buffers */
	if (flags & NKN_BUFFER_SETID_NOCACHE) {
		bp->provider = provider;
		return 0;
	}

	objp->flags |= NKN_OBJ_VALID;
	ap->total_attrsize = bp->bufsize;

retry:
	/* 
	 * Check if the ID already exists in the hash.  This can happen
	 * if a task has a cache miss and another task is already
	 * inserting buffers before this one makes its request.
	 * Also, the existing buffer may not have valid contents if pages
	 * were added before the object.
	 */
	ebp = (nkn_buf_t *)nkn_cod_attr_lookup_insert(uol->cod,
			   (nkn_buffer_t *)bp, &retcode);
	if (ebp == NULL) {
		if (retcode == NKN_COD_EXPIRED) {
			AO_fetch_and_add1(&pool->stats[ftype].cod_expired);
		} else
			assert (retcode == 0);
		bp->flags |= NKN_BUF_CACHE;
		nkn_buf_set_reval(objp);
		AO_fetch_and_add1(&pool->stats[ftype].numcache);
		AO_fetch_and_add1(&pool->p_stats[ftype][provider].numcache);
		bp->provider = provider;
		if (!ebp || !(flags & NKN_BUFFER_UPDATE))
			bp->create_ts = nkn_cur_ts & TIME32_MASK;
		if (bp->bufsize == CM_MEM_PAGE_SIZE){
			AO_fetch_and_add1(&pool->stats[ftype].numcache_32);
			AO_fetch_and_add1(&pool->p_stats[ftype][provider].numcache_32);
		} else {
			if (bp->bufsize == CM_MEM_MIN_NONSMALLATTR_PAGE_SIZE) {
				AO_fetch_and_add1(&pool->stats[ftype].numcache_4);
				AO_fetch_and_add1(
					&pool->p_stats[ftype][provider].numcache_4);
			} else {
				AO_fetch_and_add1(&pool->stats[ftype].numcache_1);
				AO_fetch_and_add1(
					&pool->p_stats[ftype][provider].numcache_1);
			}
		}
		NKN_TRACE1("buf_add", bp);
		return 0;
	}
	pthread_mutex_lock(&ebp->lock);
	if (!(ebp->flags & NKN_BUF_CACHE) ||
		(ebp->id.cod != uol->cod)) {
		pthread_mutex_unlock(&ebp->lock);
		ebp = NULL;
		goto retry;
	}
	ebp_lock = ebp;
	// Purge the existing buffer if it is expired
	if (ebp && !(flags & NKN_BUFFER_UPDATE) &&
	    nkn_buf_has_expired(ebp)) {
		nkn_obj_t *eobjp = (nkn_obj_t *)ebp;
		/* race in object setid where new objp will be set.
                   To avoid that dont detach objp from cod
                */
		nkn_obj_purge(eobjp, NKN_BUF_NODETACH_FROM_COD);
		ebp = NULL;
	}
	if (ebp) {
		nkn_obj_t *eobjp = (nkn_obj_t *)ebp;

		/* If we are updating the attributes, then we need to
		 * take the existing buffer out of the cache and replace
		 * it with this one.  Any existing references will 
		 * continue to use the old one till they are done.  Note that
		 * the existing data pages have to updated to point to the
		 * new buffer as well.
		 * The same approach applies if the existing buffer is not
		 * valid.  This happens if the provider sets data buffers 
		 * prior to setting the attribute buffer.
		 */
		if ((flags & NKN_BUFFER_UPDATE) ||
		    !(eobjp->flags & NKN_OBJ_VALID)) {
			nkn_page_t *np;
			int pref;

			objp->pglist = eobjp->pglist;
			objp->pagecnt = eobjp->pagecnt;
			objp->buf.ref += eobjp->pagecnt;
			objp->buf.use = eobjp->buf.use;		// keep warm	
			objp->bytes_cached = eobjp->bytes_cached;
			objp->page0 = eobjp->page0;
			eobjp->page0 = NULL;
			eobjp->pglist = NULL;
			pref = eobjp->buf.ref;
			assert(pref >= eobjp->pagecnt);
			eobjp->buf.ref -= eobjp->pagecnt;
			eobjp->pagecnt = 0;
			if (flags & NKN_BUFFER_UPDATE) {
			    bp->create_ts = ebp->create_ts;
			}
			/* race in object setid where new objp will be set.
			   To avoid that dont detach objp from cod
			*/
			nkn_obj_remove(eobjp, NKN_BUF_NODETACH_FROM_COD);
			if (pref && (ebp->ref == 0)) {
				/* add to free list */
				nkn_buf_free_link(ebp);
			}
			np = objp->pglist;
			while (np) {
				np->obj = objp;
				np = np->nextpage;
			}
		} else {
			AO_fetch_and_add1(&pool->stats[ftype].dupid);
			AO_fetch_and_add1(&pool->p_stats[ftype][provider].dupid);
			pthread_mutex_unlock(&ebp_lock->lock);
			return 0;
		}
	}

	retcode =nkn_cod_attr_attach(bp->id.cod, (nkn_buffer_t *)bp);
	// Its possible have a race where the COD expires by the time
	// DM2 completes a GET request.  In this case, it is fine to
	// leave this object out of the cache.  The cache mgr layer will
	// also notice that the COD has expired and will error out correctly
	// for the client to retry.
	if (retcode == NKN_COD_EXPIRED) {
		AO_fetch_and_add1(&pool->stats[ftype].cod_expired);
		AO_fetch_and_add1(&pool->p_stats[ftype][provider].cod_expired);
	} else
		assert (retcode == 0);
	nkn_buf_set_reval(objp);
	bp->flags |= NKN_BUF_CACHE;
	if (ebp_lock)
		pthread_mutex_unlock(&ebp_lock->lock);
	AO_fetch_and_add1(&pool->stats[ftype].numcache);
	bp->provider = provider;
	if (!ebp || !(flags & NKN_BUFFER_UPDATE))
		bp->create_ts = nkn_cur_ts & TIME32_MASK;
	AO_fetch_and_add1(&pool->p_stats[ftype][provider].numcache);
	if (bp->bufsize == CM_MEM_PAGE_SIZE){
		AO_fetch_and_add1(&pool->stats[ftype].numcache_32);
		AO_fetch_and_add1(&pool->p_stats[ftype][provider].numcache_32);
	} else {
		if (bp->bufsize == CM_MEM_MIN_NONSMALLATTR_PAGE_SIZE) {
			AO_fetch_and_add1(&pool->stats[ftype].numcache_4);
			AO_fetch_and_add1(
				&pool->p_stats[ftype][provider].numcache_4);
		} else {
			AO_fetch_and_add1(&pool->stats[ftype].numcache_1);
			AO_fetch_and_add1(
				&pool->p_stats[ftype][provider].numcache_1);
		}
	}

	NKN_TRACE1("buf_add", bp);
	return 0;
}


/*
 * Allocate an object corresponding to this UOL, without content.  Called
 * with the global lock held.
 */
static
nkn_obj_t *nkn_obj_alloc(nkn_uol_t *uol, nkn_provider_type_t provider,
			 int pooltype)
{
	nkn_buf_t *bp;
	nkn_obj_t *objp;
	int ret;

	bp = nkn_buf_alloc_locked(NKN_BUFFER_ATTR, 0, pooltype);
	if (!bp)
		return NULL;
	if (nkn_buf_page_alloc(bp, 0)) {
		nkn_buf_put(bp);
		return NULL;
	}
	nkn_attr_init((nkn_attr_t *)bp->buffer, bp->bufsize);
	objp = (nkn_obj_t *)bp;
	pthread_mutex_lock(&bp->lock);
	ret = nkn_obj_setid(objp, uol, provider, 0);
	assert(ret == 0);
	objp->flags &= ~NKN_OBJ_VALID;		// no valid content	
	pthread_mutex_unlock(&bp->lock);
	return objp;
}

/*
 * Set the identity for an anonymous buffer.  Returns 0 on success
 * and non-zero on error (e.g. buffer is not anonymous or id is not valid)
 * Called with global lock held.
 */
static
int nkn_page_setid(nkn_page_t *pgp, nkn_obj_t * objp, nkn_uol_t *uol,
		   nkn_provider_type_t provider, u_int32_t flags,
		   nkn_obj_t *objalloc,
		   int *provider_change)
{
	nkn_buf_t *ebp =NULL, *bp = (nkn_buf_t *)pgp;
	int ftype = bp->ftype;
	nkn_attr_t *ap;
	nkn_buffer_pool_t *pool = &bufcache;
	off_t pgoff = 0;
#ifdef NKN_BUF_DATACHECK
	char datacheck[16];
#endif

	/*
	 * Enforce alignment and offset rules for cacheable pages.
	 */
	if (!(flags & NKN_BUFFER_SETID_NOCACHE))
		pgoff = uol->offset % CM_MEM_PAGE_SIZE;

	if ((pgoff + uol->length) > CM_MEM_PAGE_SIZE) {
		DBG_LOG(ERROR, MOD_BM, "invalid length in setid request %s, %ld, %ld", 
			nkn_cod_get_cnp(uol->cod), uol->offset, uol->length);
		return NKN_BUF_INVALID_INPUT;
	}

#ifdef NKN_BUF_DATACHECK
	snprintf(datacheck, 16, "%8d\n", (int)(uol->offset/CM_MEM_PAGE_SIZE));
	if (strncmp(datacheck, (char *)bp->buffer, 8)) {
		DBG_LOG(ERROR, MOD_BM, "Data mismatch");
	}
#endif

#ifdef NKN_BUFFER_PARANOIA
	mprotect(bp->buffer, CM_MEM_PAGE_SIZE, PROT_READ);
#endif

	assert(!(bp->flags & NKN_BUF_ID));
	bp->id.offset = uol->offset;
	bp->id.length = uol->length;
	bp->id.cod = nkn_cod_dup(uol->cod, NKN_COD_BUF_MGR);
	assert(bp->id.cod != NKN_COD_NULL);
#ifdef TMPCOD
	(void)nkn_cod_get_cn(bp->id.cod, &bp->id.uri, 0);
#endif
	bp->flags |= NKN_BUF_ID;
	uol_to_bufid(uol, NKN_BUFFER_DATA, pgp->hashid);
	/* Support for uncached buffers */
	if (flags & NKN_BUFFER_SETID_NOCACHE) {
		bp->provider = provider;
		return 0;
	}

	if (flags & NKN_BUFFER_GROUPREAD)
		bp->flags |= NKN_BUF_GROUPLIST;

	/* 
	 * Check if the ID already exists in the hash.  This can happen
	 * if a task has a cache miss and another task is already
	 * inserting buffers before this one makes its request.
	 * Inserting again will cause the previous one to get over written.
	 * Instead, we leave this one out of the cache.
	 *
	 * We also have the case where an existing buffer is partially filled
	 * and this buffer can fill some part of the remaining portion.
	 */
	if ((uol->offset >= 0) && (uol->offset < CM_MEM_PAGE_SIZE)) {
		ebp = (nkn_buf_t *)nkn_page0_lookup(objp);
	} else {
		ebp = (nkn_buf_t *)nkn_pagen_lookup(uol, &bufcache);
	}
	if (ebp) {
		pthread_mutex_lock(&ebp->lock);
		if (objalloc) {
			if ((objp = ((nkn_page_t *)ebp)->obj) == NULL) {
				assert(0);
			}
		}
		off_t eb_pgoff, pgend, eb_pgend;
		bp->provider = provider;
		bp->create_ts = ebp->create_ts;
		if (ebp->bufsize < bp->bufsize) {
			pgoff = ebp->id.offset % CM_MEM_PAGE_SIZE;
			eb_pgoff = bp->id.offset % CM_MEM_PAGE_SIZE;
			pgend = pgoff + ebp->id.length;
			eb_pgend = eb_pgoff + bp->id.length;
		} else {
			eb_pgoff = ebp->id.offset % CM_MEM_PAGE_SIZE;
			pgend = pgoff + bp->id.length;
			eb_pgend = eb_pgoff + ebp->id.length;
		}
		// discard duplicate 
		if ((ebp->id.offset % CM_MEM_PAGE_SIZE == 0)
			&& (ebp->id.length == CM_MEM_PAGE_SIZE)) {
			AO_fetch_and_add1(&pool->stats[ftype].dupid);
			AO_fetch_and_add1(&pool->p_stats[ftype][provider].dupid);
			pthread_mutex_unlock(&ebp->lock);
			return 0;
		}
		// Check for hole. We allow an overlap since we don't do
		// partial hits that do not have the first byte.
		// Always copy the new buffer in its entirety to avoid
		// code complexity with the different cases of leading,
		// trailing or both overlaps.  We need to figure out
		// the new resulting length based on new bytes being added.
		off_t nlen = 0;		// bytes being added
		if (pgoff < eb_pgoff) {
			// check for preceding hole
			if (pgend < eb_pgoff) {
				AO_fetch_and_add1(&pool->stats[ftype].partial_mismatch);
				AO_fetch_and_add1(
					&pool->p_stats[ftype][provider].partial_mismatch);
				pthread_mutex_unlock(&ebp->lock);
				return 0;
			}
			nlen = eb_pgoff - pgoff;
			if (pgend > eb_pgend)
				nlen  += pgend - eb_pgend;
		} else {
			// check for succeeding hole
			if (pgoff > eb_pgend) {
				AO_fetch_and_add1(&pool->stats[ftype].partial_mismatch);
				AO_fetch_and_add1(
					&pool->p_stats[ftype][provider].partial_mismatch);
				pthread_mutex_unlock(&ebp->lock);
				return 0;
			}
			nlen = pgend - eb_pgend;
		}
		if (nlen == 0) {		// nothing to copy
			AO_fetch_and_add1(&pool->stats[ftype].partial_repeat);
			AO_fetch_and_add1(&pool->p_stats[ftype][provider].partial_repeat);
			pthread_mutex_unlock(&ebp->lock);
			return 0;
		}
                /*
                 * Check for buffer sizes. If inhand buffer size is less than
                 * incoming then remove the inhand buffer from cache and insert
                 * the new incoming buffer. if inhand buffer's refcnt is zero
                 * then link to freelist otherwise it will be added to freelist
                 * by nkn_buf_put.
                 */
		if (ebp->bufsize < bp->bufsize) {
			memcpy((void *)((off_t)bp->buffer + pgoff),
			    (void *)((off_t)ebp->buffer + pgoff),
			    ebp->id.length);
			if (ebp->id.offset < uol->offset)
				bp->id.offset = ebp->id.offset;
			bp->id.length += nlen;
			nlen = bp->id.length - ebp->id.length;
			nkn_buf_remove_stats(ebp->id.length, ebp->bufsize,
					     ebp->provider, ebp->use,
					     ebp->create_ts, ebp->ftype);
			AO_fetch_and_add(&provlistbytes[ebp->provider],
					 -ebp->id.length);
			nkn_buf_ref((nkn_buf_t *)objp, NULL, NULL, NKN_BUF_IGNORE_STATS);
                        nkn_page_remove((nkn_page_t *)ebp);
                        if (ebp->ref == 0) {
                                nkn_buf_free_unlink(ebp);
                                nkn_buf_free_link(ebp);
                        }
                        nkn_page_add(uol, objp, pgp);
			nkn_buf_deref((nkn_buf_t *)objp);
			nkn_buf_add_stats(uol->length, bp->bufsize, provider,
					  ftype);
			AO_fetch_and_add(&provlistbytes[provider],
					 uol->length);
			AO_fetch_and_add1(&pool->p_stats[ftype][provider].replace);
			AO_fetch_and_add1(&pool->stats[ftype].replace);
			objp->bytes_cached += nlen;
		} else {
		/*
		 * Do the merge
		 * NOTE:  Since the existing buffer may be in active use
		 * we have to careful as to how we update the fields since
		 * there is no explicit synchronization.  The order of update
		 * is memory, offset, length.  The reader picks up length
		 * first and therefore ensures that invalid content cannot
		 * be served.
		 */
			memcpy((void *)((off_t)ebp->buffer+pgoff),
				(void *)((off_t)bp->buffer+pgoff), uol->length);
			if (uol->offset < ebp->id.offset)
				ebp->id.offset = uol->offset;
			ebp->id.length += nlen;
			assert(ebp->id.length <= CM_MEM_PAGE_SIZE);
			AO_fetch_and_add1(&pool->stats[ftype].partial_merge);
			AO_fetch_and_add1(&pool->p_stats[ftype][provider].partial_merge);
			AO_fetch_and_add(&pool->p_stats[ftype][provider].fill_bytes, nlen);
			AO_fetch_and_add(&pool->stats[ftype].fill_bytes, nlen);
			AO_fetch_and_add(&pool->stats[ftype].c_fill_bytes, nlen);
			AO_fetch_and_add(&pool->p_stats[ftype][provider].c_fill_bytes, nlen);
			((nkn_page_t *)ebp)->obj->bytes_cached += nlen;
			AO_fetch_and_add(&provlistbytes[provider],
					 nlen);
		}
		pthread_mutex_unlock(&ebp->lock);

		if((objp->buf.provider != provider) &&
			(provider > NKN_MM_max_pci_providers)) {
			objp->buf.provider = provider;
			*provider_change = 1;
		}
		if (provider > NKN_MM_max_pci_providers) {
			ap = (nkn_attr_t *)(objp->buf.buffer);
			if (ap->start_offset > uol->offset) {
				ap->start_offset = uol->offset;
			}
			if (ap->end_offset < (uol->offset + uol->length)) {
				ap->end_offset = (uol->offset + uol->length);
			}
		}

		return 0;
	}

	/* 
	 * The page has to be inserted into the hash map associated with
	 * its object.
	 */

	if((objp->buf.provider != provider) &&
		(provider > NKN_MM_max_pci_providers)) {
		objp->buf.provider = provider;
		*provider_change = 1;
	}
	if (provider > NKN_MM_max_pci_providers) {
		ap = (nkn_attr_t *)(objp->buf.buffer);
		if (ap->start_offset > uol->offset) {
			ap->start_offset = uol->offset;
		}
		if (ap->end_offset < (uol->offset + uol->length)) {
			ap->end_offset = (uol->offset + uol->length);
		}
	}

	bp->provider = provider;
	bp->create_ts = nkn_cur_ts & TIME32_MASK;
	nkn_page_add(uol, objp, pgp);
	nkn_buf_add_stats(uol->length, bp->bufsize, provider, ftype);
	AO_fetch_and_add(&provlistbytes[provider], uol->length);

	objp->bytes_cached += uol->length;
	NKN_TRACE1("buf_add", bp);
	return 0;
}

/*
 * Set:  Set the identity for an anonymous buffer.  Returns 0 on success
 * and non-zero on error (e.g. buffer is not anonymous or id is not valid)
 */
int nkn_buf_setid(nkn_buf_t *bp, nkn_uol_t *uol, nkn_provider_type_t provider, u_int32_t flags)
{
	int ret = 0;
	nkn_obj_t *objp;
	nkn_obj_t *objalloc = NULL;
	int provider_change = 0;
#if 0
	char *uri;
	AM_pk_t am_pk;
#endif

	if (bp->type == NKN_BUFFER_ATTR) {
		pthread_mutex_lock(&bp->lock);
		ret = nkn_obj_setid((nkn_obj_t *)bp, uol, provider, flags);
		pthread_mutex_unlock(&bp->lock);
	} else {
		assert(bp->type == NKN_BUFFER_DATA);
		objp = (nkn_obj_t *)nkn_cod_attr_lookup(uol->cod);
		// first attribute should be set. so allocate attribute. This is needed for
		//DM2 since there is a chance where data buffers are set without attrib bufs
		if (objp == NULL) {
			objalloc = nkn_obj_alloc(uol, provider, bp->ftype);
			objp = objalloc;
		}
		if (objp) {
			pthread_mutex_lock(&objp->buf.lock);
			if (!(objp->buf.flags & NKN_BUF_CACHE) ||
				(objp->buf.id.cod != uol->cod)) {
				pthread_mutex_unlock(&objp->buf.lock);
				return 0;
			}
			pthread_mutex_lock(&bp->lock);
			ret = nkn_page_setid((nkn_page_t *)bp, objp,
				uol, provider, flags, objalloc,
				&provider_change);
			pthread_mutex_unlock(&bp->lock);
			// Release ref count for allocated object.  We should now have one
			// from linking the page to it.
			if (objalloc) {
				nkn_buf_deref((nkn_buf_t *)objalloc);
			}
			pthread_mutex_unlock(&objp->buf.lock);
		}
	}
	if(provider_change) {
		nkn_cod_set_push_in_progress(uol->cod);
#if 0
		uri = (char *)nkn_cod_get_cnp(uol->cod);
		am_pk.name = (char *)uri;
		am_pk.provider_id = provider;
		am_pk.key_len = 0;
		AM_inc_num_hits(&am_pk, 0, 0, NULL, NULL, NULL);
#endif
	}
	return ret;
}

void
nkn_buf_hit_ts(void *obp, uint32_t *am_hit, uint32_t *update)
{
	uint32_t curtime;
	nkn_obj_t *objp = (nkn_obj_t *)obp;
	curtime = (nkn_cur_ts & TIME32_MASK);
	if (objp->am_hits < AM_MAX_HITS) {
		objp->am_hits++;
	}
	if ((objp->am_ts == 0) ||
		(objp->am_ts + cr_am_report_interval <= curtime)) {
	    *update = 1;
	    *am_hit = objp->am_hits;
	    objp->am_ts = curtime; 
	    objp->am_hits = 0;
	}
}

int
nkn_buf_incache(nkn_buf_t *bp)
{
	int in_cache;
	pthread_mutex_lock(&bp->lock);
	in_cache = bp->flags;
	pthread_mutex_unlock(&bp->lock);
	if (in_cache & NKN_BUF_CACHE)
		return 1;
	else
		return 0;
}

void
nkn_buf_getid(nkn_buf_t *bp, nkn_uol_t *uol)
{
	assert(bp->ref);
	if (bp->flags & NKN_BUF_ID) {
		*uol = bp->id;
	}
}

inline nkn_provider_type_t 
nkn_buf_get_provider(nkn_buf_t *bp)
{
	if (!(bp->flags & NKN_BUF_ID))
		return Unknown_provider;
	return bp->provider;
}

void 
nkn_buf_set_provider(nkn_buf_t *bp, nkn_provider_type_t provider)
{
	assert(0);
	if (!(bp->flags & NKN_BUF_ID))
		return;
	bp->provider = provider;
}

/*
 * Get Content:  Get the content pointer.  Caller must have a ref count
 * on the buffer via the get call.
 */
inline 
void *nkn_buf_get_content(nkn_buf_t *bp)
{
	return bp->buffer;
}

#if 0
static int
nkn_buf_iscached(nkn_buf_t *bp)
{
	assert(bp->ref);
	return (bp->flags & NKN_BUF_CACHE);
}
#endif // 0

void 
nkn_buf_purge(nkn_uol_t *uol)
{
	nkn_buf_t *bp;
	nkn_obj_t *objp;
	nkn_page_t *pgp;
	nkn_attr_t *ap;

	bp = nkn_buf_get(uol, NKN_BUFFER_ATTR, NKN_BUF_IGNORE_EXPIRY);
	if (bp == NULL)
		return;

	// We now have a ref counted valid object. Remove it and all its
	// pages from the cache and release the extra ref count.
	// of all its pages.
	objp = (nkn_obj_t *)bp;
	pthread_mutex_lock(&bp->lock);
	if (!(bp->flags & NKN_BUF_CACHE)) {
		nkn_buf_deref(bp);
		pthread_mutex_unlock(&bp->lock);
		return;
	}
	while ((pgp = objp->pglist) != NULL) {
		nkn_buf_t *pbp = (nkn_buf_t *)pgp;
		pthread_mutex_lock(&pbp->lock);
		nkn_buf_remove_stats(pbp->id.length, pbp->bufsize,
				     pbp->provider, pbp->use,
				     pbp->create_ts,
				     pbp->ftype);
		AO_fetch_and_add(&provlistbytes[pbp->provider],
				 -pbp->id.length);
		nkn_page_remove(pgp);   // also updates pglist
		// If the buffer is free (no references), unlink from the
		// free list and put it back.  This will trigger the
		// release of the COD reference and put it back on the
		// head of the free list.
		if (pbp->ref == 0) {
			nkn_buf_free_unlink(pbp);
			nkn_buf_free_link(pbp);
		}
		pthread_mutex_unlock(&pbp->lock);
	}
	// Could be out of the cache while we release the lock
	if (bp->flags & NKN_BUF_CACHE) {
		ap = (nkn_attr_t *)(objp->buf.buffer);
		nkn_obj_remove_stats(bp->use, ap->content_length, bp->create_ts,
				     bp->provider, bp->ftype);
		nkn_obj_remove(objp, NKN_BUF_DETACH_FROM_COD);
	}
	nkn_buf_deref(bp);
	pthread_mutex_unlock(&bp->lock);
}

nkn_provider_type_t
nkn_uol_getattribute_provider(nkn_uol_t *uol)
{
	nkn_provider_type_t provider = 0;
	nkn_buf_t *bp;

	bp = nkn_buf_get(uol, NKN_BUFFER_ATTR,
			NKN_BUF_IGNORE_EXPIRY|NKN_BUF_IGNORE_STATS);
	if (bp != NULL) {
		provider = bp->provider;
		nkn_buf_deref(bp);
	}
	return provider;
}

void
nkn_uol_setattribute_provider(nkn_uol_t *uol, nkn_provider_type_t provider)
{
	nkn_buf_t *bp;
	nkn_obj_t *objp;
	int ftype;
	bp = nkn_buf_get(uol, NKN_BUFFER_ATTR, NKN_BUF_IGNORE_EXPIRY|NKN_BUF_IGNORE_STATS);
	if (bp == NULL)
		return;
	objp = (nkn_obj_t *)bp;
	pthread_mutex_lock(&bp->lock);
	if (!(objp->flags & NKN_OBJ_VALID)) {
		nkn_buf_deref(bp);
		pthread_mutex_unlock(&bp->lock);
		return;
	}
	ftype = bp->ftype;
	AO_fetch_and_sub1(&attrcache.p_stats[ftype][bp->provider].numcache);
	AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].numcache);
	if (bp->bufsize == CM_MEM_PAGE_SIZE){
		AO_fetch_and_sub1(
			&attrcache.p_stats[ftype][bp->provider].numcache_32);
		AO_fetch_and_add1(
			&attrcache.p_stats[ftype][provider].numcache_32);
	} else {
		if (bp->bufsize == CM_MEM_MIN_NONSMALLATTR_PAGE_SIZE) {
			AO_fetch_and_sub1(
				&attrcache.p_stats[ftype][bp->provider].numcache_4);
			AO_fetch_and_add1(
				&attrcache.p_stats[ftype][provider].numcache_4);
		} else {
			AO_fetch_and_sub1(
				&attrcache.p_stats[ftype][bp->provider].numcache_1);
			AO_fetch_and_add1(
				&attrcache.p_stats[ftype][provider].numcache_1);
			
		}
	}
	bp->provider = provider;
	nkn_buf_deref(bp);
	pthread_mutex_unlock(&bp->lock);
}

void 
nkn_uol_setprovider(nkn_uol_t *uol, nkn_provider_type_t provider)
{
	nkn_buf_t *bp, *pagebp;
	nkn_obj_t *objp;
	nkn_page_t *pgp;
	int ftype;

#ifdef TMPCOD
	nkn_cod_t lcod = NKN_COD_NULL;
	if (nkn_cod_get_status(uol->cod) == NKN_COD_STATUS_INVALID) {
		uol->cod = nkn_cod_open(uol->uri, NKN_COD_BUF_MGR);
		lcod = uol->cod;
	}
#endif
	bp = nkn_buf_get(uol, NKN_BUFFER_ATTR, NKN_BUF_IGNORE_EXPIRY|NKN_BUF_IGNORE_STATS);
	if (bp == NULL)
		goto skip_step;

	// We now have a ref counted object.  Change its provider and that
	// of all its pages.
	objp = (nkn_obj_t *)bp;
	pthread_mutex_lock(&bp->lock);
	if (!(objp->flags & NKN_OBJ_VALID)) {
		nkn_buf_deref(bp);
		pthread_mutex_unlock(&bp->lock);
		return;
	}
	pgp = objp->pglist;
	while (pgp) {
		pagebp = (nkn_buf_t *)pgp;
		pthread_mutex_lock(&pagebp->lock);
		ftype = pagebp->ftype;
		AO_fetch_and_add(
			&bufcache.p_stats[ftype][pgp->buf.provider].c_fill_bytes,
			-pagebp->id.length);
		AO_fetch_and_add(
			&bufcache.p_stats[ftype][provider].c_fill_bytes,
			pagebp->id.length);
		AO_fetch_and_add(
			&bufcache.p_stats[ftype][pgp->buf.provider].c_buffer_bytes,
			-pagebp->bufsize);
		AO_fetch_and_add(
			&bufcache.p_stats[ftype][provider].c_buffer_bytes,
			pagebp->bufsize);
		AO_fetch_and_add(
			&bufcache.p_stats[ftype][pgp->buf.provider].fill_bytes,
			-pagebp->id.length);
		AO_fetch_and_add(
			&bufcache.p_stats[ftype][provider].fill_bytes,
			pagebp->id.length);
		AO_fetch_and_sub1(
			&bufcache.p_stats[ftype][pgp->buf.provider].numcache);
		AO_fetch_and_add1(
			&bufcache.p_stats[ftype][provider].numcache);
		pgp->buf.provider = provider;
		pgp = pgp->nextpage;
		pthread_mutex_unlock(&pagebp->lock);
	}
	ftype = bp->ftype;
	AO_fetch_and_sub1(&attrcache.p_stats[ftype][bp->provider].numcache);
	AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].numcache);
	if (bp->bufsize == CM_MEM_PAGE_SIZE){
		AO_fetch_and_sub1(&attrcache.p_stats[ftype][bp->provider].numcache_32);
		AO_fetch_and_add1(&attrcache.p_stats[ftype][provider].numcache_32);
	} else {
		if (bp->bufsize == CM_MEM_MIN_NONSMALLATTR_PAGE_SIZE) {
			AO_fetch_and_sub1(
				&attrcache.p_stats[ftype][bp->provider].numcache_4);
			AO_fetch_and_add1(
				&attrcache.p_stats[ftype][provider].numcache_4);
		} else {
			AO_fetch_and_sub1(
				&attrcache.p_stats[ftype][bp->provider].numcache_1);
			AO_fetch_and_add1(
				&attrcache.p_stats[ftype][provider].numcache_1);
		}
	}
	bp->provider = provider;
	nkn_buf_deref(bp);
	pthread_mutex_unlock(&bp->lock);
skip_step:
#ifdef TMPCOD
	if (lcod != NKN_COD_NULL) {
		nkn_cod_close(uol->cod, NKN_COD_BUF_MGR);
		uol->cod = NKN_COD_NULL;
	}
#endif
	return;
}

/* Atmoic test and set for modified flag on object. */
void 
nkn_buf_set_modified(nkn_buf_t *bp)
{
	nkn_obj_t *objp;

	if (bp->type == NKN_BUFFER_ATTR)
		objp = (nkn_obj_t *)bp;
	else
		objp = ((nkn_page_t *)bp)->obj;
	pthread_mutex_lock(&objp->buf.lock);
	if (objp->flags & NKN_OBJ_VALID) {
		objp->flags |= NKN_OBJ_MODIFIED;
	}
	pthread_mutex_unlock(&objp->buf.lock);
}

inline void
nkn_obj_set_dirty(nkn_obj_t *bp)
{
	pthread_mutex_lock(&bp->buf.lock);
        bp->flags |= NKN_OBJ_DIRTY;
	pthread_mutex_unlock(&bp->buf.lock);
}

inline void
nkn_obj_unset_dirty(nkn_obj_t *bp)
{
	pthread_mutex_lock(&bp->buf.lock);
        bp->flags &= ~NKN_OBJ_DIRTY;
	pthread_mutex_unlock(&bp->buf.lock);
}

nkn_attr_t *
nkn_buf_get_attr(nkn_buf_t *bp)
{
	nkn_obj_t *objp;
	nkn_attr_t *attr = NULL;

	if (bp->type == NKN_BUFFER_ATTR)
		objp = (nkn_obj_t *)bp;
	else
		objp = ((nkn_page_t *)bp)->obj;
	if (!objp)
		return NULL;
	pthread_mutex_lock(&objp->buf.lock);
	if ((objp->flags & NKN_OBJ_VALID)) {
		attr = (nkn_attr_t *)(objp->buf.buffer);
	}
	pthread_mutex_unlock(&objp->buf.lock);
	return attr;
}

void
nkn_buf_update_hitbytes(uint64_t length, int pooltype)
{
	AO_store(&bufcache.stats[pooltype].hit_bytes, length);
}

int
nkn_buf_get_bufsize(nkn_buf_t *bp)
{
	int size;
	pthread_mutex_lock(&bp->lock);
	assert(bp->ref);
	size = bp->bufsize;
	pthread_mutex_unlock(&bp->lock);
	return size;
}

// must be called with bp lock
int
nkn_buf_set_bufsize(nkn_buf_t *bp, int rsize, int pooltype)
{
	nkn_buf_t *newbp;
	int bufsize;
	nkn_pageid_t pid;
	void *buffer;
	int ftype;

	if (rsize < 0) 
		return NKN_BUF_INVALID_INPUT;

	if (pooltype == BM_NEGCACHE_ALLOC &&
		!glob_bm_negcache_percent)
		return NKN_BUF_INVALID_INPUT;
		

	if (bp->type == NKN_BUFFER_ATTR) {
		if (glob_bm_min_attrcount != 0) {
			 if (rsize > NKN_DEFAULT_ATTR_SIZE) {
				return NKN_BUF_INVALID_INPUT;
			 }
			 if (pooltype == BM_NEGCACHE_ALLOC) {
				if ((unsigned int)
					rsize > glob_bm_min_attrsize)
					return NKN_BUF_INVALID_INPUT;
			 }
		} else if (rsize > CM_MEM_PAGE_SIZE) {
			return NKN_BUF_INVALID_INPUT;
		}
	}
	// Must not have an ID already set
	assert(bp->ref);
	if (bp->flags & NKN_BUF_ID) {
		return NKN_BUF_INVALID_INPUT;
	}
	// A reduction is possible only if it can go to a separate pool
	bufsize = nkn_buf_get_pagesize(rsize, pooltype);
	if (bufsize == bp->bufsize &&
			bp->ftype == pooltype) {
		return 0;
	}
	// Get a new buffer with the required page before giving up ours
	newbp = nkn_buf_alloc_locked(bp->type, bufsize, pooltype);
	if (!newbp){
		glob_bufmgr_server_busy_err++;
		return NKN_SERVER_BUSY;
	}
	if (nkn_buf_page_alloc(newbp, bufsize)) {
		nkn_buf_put(newbp);
		glob_bufmgr_server_busy_err++;
		return NKN_SERVER_BUSY;
	}
	assert(newbp->bufsize == bufsize);
	if (newbp->type == NKN_BUFFER_ATTR)
		nkn_attr_init((nkn_attr_t *)newbp->buffer, newbp->bufsize);

	// Swap page with the new buffer
	newbp->bufsize = bp->bufsize;
	buffer = bp->buffer;
	ftype = bp->ftype;

	bp->ftype = pooltype;
	bp->bufsize = bufsize;
	bp->buffer = newbp->buffer;

	newbp->ftype = ftype;
	newbp->buffer = buffer;
	nkn_buf_put(newbp);
	return 0;
}

int
nkn_buf_lock_set_bufsize(nkn_buf_t *bp, int rsize, int pooltype)
{
	int ret;
	pthread_mutex_lock(&bp->lock);
	ret = nkn_buf_set_bufsize(bp, rsize, pooltype);
	pthread_mutex_unlock(&bp->lock);
	return ret;
}

void 
nkn_buf_getstats(nkn_buf_t *bp, nkn_buffer_stat_t *statp)
{
	assert(bp->ref);
	statp->hits = bp->use;
	if (bp->type == NKN_BUFFER_ATTR)
		statp->bytes_cached = ((nkn_obj_t *)bp)->bytes_cached;
	else 
		statp->bytes_cached = bp->id.length;
}

int nkn_buf_set_attrbufsize(nkn_buf_t *bp, int entries, int tot_blob_bytes)
{
	int rv;
	nkn_attr_t old_attr;
	nkn_attr_t *pnkn_attr;

	pthread_mutex_lock(&bp->lock);
	// Preserve existing C struct values
	memcpy(&old_attr, nkn_buf_get_content(bp), sizeof(old_attr));
	
	rv = nkn_buf_set_bufsize(bp, 
			    sizeof(nkn_attr_t) +
			    (entries * sizeof(nkn_attr_entry_t)) + 
			    tot_blob_bytes, bp->ftype);
	if (rv) {
		pthread_mutex_unlock(&bp->lock);
		return rv;
	}

	pnkn_attr = (nkn_attr_t *)nkn_buf_get_content(bp);
	old_attr.total_attrsize = pnkn_attr->total_attrsize;
	nkn_attr_reset_blob(&old_attr, 0);
	memcpy(pnkn_attr, &old_attr, sizeof(old_attr));
	pthread_mutex_unlock(&bp->lock);
	return 0;
}

uint64_t
nkn_buf_get_totaltransit_pages(void)
{
	int i, j;
	uint64_t numfree_pages =0;
	uint64_t tot_pages = nkn_page_get_total_pages();
	if (tot_pages) {
		for (j =0; j < CUR_NUM_CACHE; j++) {
			for (i = 0; i < NUMFBPOOLS; i++) {
				numfree_pages +=
					freebufpools[j][i].stats.numfree;
			}
		}
		return (tot_pages - (numfree_pages +
					glob_bm_refcnted_attribute_cache));
	}
	return 0;
}

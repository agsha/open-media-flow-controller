#ifndef NKN_PAGE_H
#define NKN_PAGE_H
#include <glib.h>

#include "cache_mgr.h"
#include "nkn_defs.h"
#include <sys/queue.h>

#define NUMFBPOOLS 3

/*
 * A page is a contiguous and aligned piece of memory that is used to provide
 * the storage behind a buffer (data and attributes).  While data buffers are
 * fixed in size (32KB) to allow for a uniform mapping, pages can vary in size
 * in powers of 2 to optimize RAM utilization.  Similarly, attribute buffers
 * can typically use a small page (e.g. 1KB) while exceptional cases can get
 * a large page size (upto 32KB or even beyond).
 *
 * The RAM cache configured in nvsd is managed as a pool of pages
 */

typedef uint64_t nkn_pageid_t;		// opaque page id

#define NKN_PAGE_MIN_SIZE	512
#define NKN_PAGEID_NULL		0

// Get memory pointer from page id
#define NKN_PAGEID_TO_MEM(pn)	((void *)(pn & ~0xFF))

/*
 * Return the page size for the specified page id in the size parameter.
 * Returns zero if the size is set and non-zero if the page is not valid.
 */
int nkn_page_get_size(nkn_pageid_t page, int *size);

/* 
 * Return a page from the specified pool.  Returns NULL if no pages are 
 * available.
 */
void* nkn_page_alloc(int poolnum);

/*
 * Free the specified page id
 */
void nkn_page_free(nkn_pageid_t page);

/*
 * Initialize page pools using specified config array
 */
typedef struct nkn_page_config {
	float initratio;		// Should use power of two fractions
	int numbufs, pagesize, tot_bufs;
	char name[16];
	unsigned long long num_attr;
} nkn_page_config_t;

void nkn_page_init(uint64_t poolsize, int numpools, nkn_page_config_t *pools);

void nkn_multi_pagepool_init(uint64_t poolsize);

uint64_t nkn_page_get_total_pages(void);

#endif

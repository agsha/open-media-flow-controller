#include <sys/cdefs.h>

#include "opt_ddb.h"
#include "opt_init_path.h"
#include "opt_mac.h"
#include "opt_printf.h"


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/sysent.h>
#include <sys/reboot.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/conf.h>

#include <sys/msgbuf.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#include <sys/ctype.h>
#include <sys/protosw.h>
#include <sys/sx.h>

#include <nknshim/nkns_osl.h>
#include <nknshim/nkns_api.h>

static pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;

#define SIZEOF_512K	0x00080000
#define SIZEOF_256K	0x00040000
#define SIZEOF_128K	0x00020000
#define SIZEOF_64K	0x00010000
#define SIZEOF_32K	0x00008000
#define SIZEOF_16K	0x00004000
#define SIZEOF_8K	0x00002000
#define SIZEOF_4K 	0x00001000
#define SIZEOF_2K	0x00000800
#define SIZEOF_1K	0x00000400
#define SIZEOF_512B	0x00000200
#define SIZEOF_256B	0x00000100
#define SIZEOF_128B	0x00000080
#define SIZEOF_64B	0x00000040
#define SIZEOF_32B	0x00000020

//#define USE_LINUX_MALLOC	1

struct nknmem_block {
	long tot_pages;
	long tot_used;
	void * p_cur;	// Location of memory of this block
	void * p_first;	// first available this size buffer
	void * p_last;	// last pointer, out of this block
} gmem, g8k, g2k, g1k, g512b, g32b;

struct nknlink_t {
	struct nknlink_t * next;
};

static int glob_mem_malloc=0;
static int glob_mem_tot_used=0;
static int glob_mem_tot_percentage=0;
	
///////////////////////////////////////////////////////////
// memory mamagement functions
///////////////////////////////////////////////////////////

static void * nkn_netmalloc_blk(struct nknmem_block * blk, int blksize);
static void * nkn_netmalloc_blk(struct nknmem_block * blk, int blksize)
{
	struct nknlink_t * pl;
	void * ret;
	
	if(blk->p_cur>=blk->p_last) {
		printf("nkn_netmalloc_blk: no more memory blksize=%d p_cur=%p p_last=%p\n", 
			blksize, blk->p_cur, blk->p_last);
		return NULL;
	}
	if(blk->p_cur==NULL) {
		printf("nkn_netmalloc_blk: blk->p_cur==NULL\n");
		return NULL;
	}
	/*
	 * First 8 bytes of each block stores the address of next avaliable buffer
	 */
	ret=blk->p_cur;
	pl=(struct nknlink_t *)ret;
	blk->p_cur=(void *)pl->next;
	blk->tot_used++;

	glob_mem_tot_used += blksize;
	glob_mem_tot_percentage=100 * glob_mem_tot_used / (gmem.tot_pages);

	/*
	 * The first 8 bytes reserved for link list pointers.
	 */
	return (void *)((char *)ret+8); 
}

static void nkn_netfree_blk(struct nknmem_block * blk, void ** p, int blksize);
static void nkn_netfree_blk(struct nknmem_block * blk, void ** p, int blksize)
{
	struct nknlink_t * pl;


	/*
	 * The first 8 bytes reserved for link list pointers.
	 */
	pl=(struct nknlink_t *)((char *)(*p) - 8);
	pl->next=(struct nknlink_t *)blk->p_cur;
	blk->p_cur=(void *)pl;
	*p=NULL;
	blk->tot_used--;

	glob_mem_tot_used -= blksize;
	glob_mem_tot_percentage=100 * glob_mem_tot_used / (gmem.tot_pages);

	return;
}

#define nkn_netmalloc_8k() nkn_netmalloc_blk(&g8k, SIZEOF_8K);
#define nkn_netfree_8k(x) nkn_netfree_blk(&g8k, x, SIZEOF_8K)

#define nkn_netmalloc_2k() nkn_netmalloc_blk(&g2k, SIZEOF_2K);
#define nkn_netfree_2k(x) nkn_netfree_blk(&g2k, x, SIZEOF_2K);

#define nkn_netmalloc_1k() nkn_netmalloc_blk(&g1k, SIZEOF_1K);
#define nkn_netfree_1k(x) nkn_netfree_blk(&g1k, x, SIZEOF_1K);

#define nkn_netmalloc_512b() nkn_netmalloc_blk(&g512b, SIZEOF_512B);
#define nkn_netfree_512b(x) nkn_netfree_blk(&g512b, x, SIZEOF_512B);

#define nkn_netmalloc_32b() nkn_netmalloc_blk(&g32b, SIZEOF_32B);
#define nkn_netfree_32b(x) nkn_netfree_blk(&g32b, x, SIZEOF_32B);

void * nkn_netmalloc(int size)
{
	void * p=NULL;

#ifdef USE_LINUX_MALLOC
	return  nkn_linux_malloc(size);
#endif // USE_LINUX_MALLOC

	nkn_pthread_mutex_lock(&mem_mutex);

	glob_mem_malloc++;
	
	// Order is very important
	if(size<=0) { 
		nkn_pthread_mutex_unlock(&mem_mutex);
		return NULL; 
	}
	size+=8; // the first 8 bytes reserved for link list pointers.
	if(size<=SIZEOF_32B) { p=nkn_netmalloc_32b(); }
	else if(size<=SIZEOF_512B) { p=nkn_netmalloc_512b(); }
	else if(size<=SIZEOF_1K) { p=nkn_netmalloc_1k(); }
	else if(size<=SIZEOF_2K) { p=nkn_netmalloc_2k(); }
	else if(size<=SIZEOF_8K) { p=nkn_netmalloc_8k(); }
	if(!p) {
		/* if size is > 8K or run out buffer */
		printf("nkn_netmalloc: code requires memory size of %d bytes\n", size);
		p=nkn_linux_malloc(size);
	}

	nkn_pthread_mutex_unlock(&mem_mutex);
	return p;
}

void nkn_netfree(void ** p)
{
#ifdef USE_LINUX_MALLOC
	if(p && *p) {
		nkn_linux_free((*p));
		*p=NULL;
	}
	return;
#endif // USE_LINUX_MALLOC

	nkn_pthread_mutex_lock(&mem_mutex);

	if(!p) {
		printf("ERROR: nkn_netfree: p==NULL\n");
		nkn_pthread_mutex_unlock(&mem_mutex);
		return;
	}
	if(!(*p)) {
		printf("ERROR: nkn_netfree: *p==NULL\n");
		nkn_print_trace();
		nkn_pthread_mutex_unlock(&mem_mutex);
		return;
	}
	
	if((*p)<gmem.p_first || (*p)>=gmem.p_last) {
		printf("nkn_netfree: linux memory management.\n");
		nkn_linux_free((*p));
		*p=NULL;
		nkn_pthread_mutex_unlock(&mem_mutex);
		return;
	}
	glob_mem_malloc--;

	// Order is very important
	if((*p)<g8k.p_last) { nkn_netfree_8k(p); }
	else if((*p)<g2k.p_last) { nkn_netfree_2k(p); }
	else if((*p)<g1k.p_last) { nkn_netfree_1k(p); }
	else if((*p)<g512b.p_last) { nkn_netfree_512b(p); }
	else if((*p)<g32b.p_last) { nkn_netfree_32b(p); }
	else {
		/* 
		 * impossible memory pointer, higher than gmem.p_last.
		 * Pointer cannot be higher than g32b.p_last
		 * because (g32b.p_last == gmem.p_last)
		 */
		net_assert(__FILE__, __LINE__, "impossible memory pointer");
	}

	nkn_pthread_mutex_unlock(&mem_mutex);
	return;
}

void *nkn_netmalloc_zero(int size);
void *nkn_netmalloc_zero(int size)
{
	void *p;

	p = nkn_netmalloc(size);
	if(p) memset((char *)p, 0, size);
	return(p);
}


static void nkn_set_block(struct nknmem_block * blk, long pages, void * first, long blksize);
static void nkn_set_block(struct nknmem_block * blk, long pages, void * first, long blksize)
{
	struct nknlink_t * pl;
	char * pnext;
	
	blk->tot_pages=pages;
	blk->p_cur=first;
	blk->p_first=first;
	blk->p_last=(void *)((char *)first + pages);
	blk->tot_used=0;
	// First 4 bytes of each block stores the address of next avaliable buffer
	pl=(struct nknlink_t *)first;
	while(1) {
		pnext=(char *)pl+blksize;
		if((void *)pnext >= blk->p_last) {
			pl->next = NULL;
			printf("blksize=%ld p_first=%p p_last=%p tot_cnt=%ld\n",
				blksize, blk->p_first, blk->p_last, 
				(int)((char *)blk->p_last-(char *)blk->p_first)/blksize);
			break;
		}
		pl->next=(struct nknlink_t *)pnext;
		pl=pl->next;
	}
}

int nkn_set_memory(void * pmem, long lmem);
int nkn_set_memory(void * pmem, long lmem)
{
	static short hascalled=0;

	if(hascalled) return 1;	// this function has been called before

	/* We want to get 20% of physical memory */
	gmem.p_first  = pmem;
	gmem.tot_pages  = lmem;
	printf("nkn_set_memory: nkn_tot_mem_pages=%ld\n", gmem.tot_pages );

	if(gmem.tot_pages < 20*1024*1024) {
		printf("Too less memory. Minimum 20Mbytes are required . Please contact vendor.\n");
		nkn_exit(0);
	}

	gmem.p_cur=gmem.p_first;
	gmem.p_last=(void *)((char *)gmem.p_first + gmem.tot_pages);
	memset(gmem.p_first, 0, gmem.tot_pages);
	printf("gmem: p_first=%p  p_last=%p\n", gmem.p_first, gmem.p_last);

	/* 
	 * After we got all memory, we will mamage the memory ourself.
	 * The whole memory will be grouped into several chains of different memory size
	 * Minimum size is 32M.
	 *	512K:  
	 *	256K:  
	 *	128K: 
	 *	64K:
	 *	32K:
	 *	16K:
	 *	8K:	5%
	 *	4K:
	 *	2K:	15%
	 *	1K:  	10%
	 *	512b: 	50%
	 *	256b: 
	 *	128b:
	 *	64b: 
	 *	32b: 	20% (whatever left)
	 */
	 // for scalable cache first
	nkn_set_block(&g8k, (long)(gmem.tot_pages*0.05), gmem.p_first, SIZEOF_8K);
	nkn_set_block(&g2k, (long)(gmem.tot_pages*0.15), g8k.p_last, SIZEOF_2K);
	nkn_set_block(&g1k, (long)(gmem.tot_pages*0.10), g2k.p_last, SIZEOF_1K);
	nkn_set_block(&g512b, (long)(gmem.tot_pages*0.50), g1k.p_last, SIZEOF_512B);
	nkn_set_block(&g32b, (long)(((long)gmem.p_last-(long)g512b.p_last)), 	
				g512b.p_last, SIZEOF_32B);
	
	hascalled=1;
	return 1;

}

/*
int main(int argc, char * argv[])
{
	void * p1, * p2, * p3, * p4, *p5, *p6;
	char * pmem;
	long lmem=40*1024*1024;

	pmem=malloc(lmem);
	if(!pmem) {
		printf("failed \n");
		exit(0);
	}
	nkn_set_memory(pmem, lmem);
	// TESTING CODE
	p1=nkn_netmalloc(9012);
	nkn_netfree(&p1);
	p2=nkn_netmalloc(9034);
	p3=nkn_netmalloc(2000);
	p4=nkn_netmalloc(1672);
	p5=nkn_netmalloc(3838);
	nkn_netfree(&p3);
	nkn_netfree(&p5);
	p6=nkn_netmalloc(56);
	nkn_netfree(&p2);
	p2=nkn_netmalloc(8933);
	p1=nkn_netmalloc(2783);
	printf("p1=0x%x p2=0x%x p3=0x%x p4=0x%x p5=0x%x p6=0x%x\n", p1,p2,p3,p4,p5,p6);

	return 1;
}
*/

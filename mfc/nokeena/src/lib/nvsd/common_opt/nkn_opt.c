#include "nkn_defs.h"
#include "nkn_rtsched_api.h"
#include "nkn_lockstat.h"
#include "nkn_slotapi.h"
#include "network.h"
#include "server.h"

time_t nkn_cur_ts;
struct http_mgr_output_q  * hmoq_head[MAX_EPOLL_THREADS];
struct http_mgr_output_q  * hmoq_plast[MAX_EPOLL_THREADS]={NULL};
pthread_mutex_t hmoq_mutex[MAX_EPOLL_THREADS]
 = 
{ 
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER
};
pthread_cond_t hmoq_cond[MAX_EPOLL_THREADS] =
{ 
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER
};

nkn_lockstat_t nm_lockstat[MAX_EPOLL_THREADS];

nkn_lockstat_t slotlockstat;

void
add_slot_item(nkn_slot_mgr *restrict pslotmgr, int insert_slot,
	      struct nkn_task * restrict item)  
{
	NKN_MUTEX_LOCKSTAT(&pslotmgr->muts[insert_slot], &slotlockstat);
	TAILQ_INSERT_TAIL(&pslotmgr->head[insert_slot], item, slot_entry);
	NKN_MUTEX_UNLOCKSTAT(&pslotmgr->muts[insert_slot]);
    	AO_fetch_and_add1(&pslotmgr->num_tasks);
}

struct nkn_task *
remove_slot_item(nkn_slot_mgr *restrict pslotmgr, int slot)
{
	struct nkn_task *pslotitem;
	NKN_MUTEX_LOCKSTAT(&pslotmgr->muts[slot], &slotlockstat);
	if ((pslotitem = TAILQ_FIRST(&pslotmgr->head[slot])) != NULL) {
		TAILQ_REMOVE(&pslotmgr->head[slot], pslotitem, slot_entry);
		pthread_mutex_unlock(&pslotmgr->muts[slot]);
		AO_fetch_and_sub1(&pslotmgr->num_tasks);
		return (pslotitem);
	}
	NKN_MUTEX_UNLOCKSTAT(&pslotmgr->muts[slot])
	return NULL;
}


void
add_hmoq_item(int num, struct network_mgr * restrict pnm, void * restrict phmoq)
{
	pthread_mutex_lock(&hmoq_mutex[num]);
	if (hmoq_plast[num] == NULL) {
		hmoq_head[num] = phmoq;
	}
	else {
		hmoq_plast[num]->next = phmoq;
	}
	hmoq_plast[num] = phmoq;
	pthread_mutex_unlock(&hmoq_mutex[num]);
	pthread_mutex_lock(&hmoq_mutex[pnm->pthr->send_thread_mutex_num]);
	pthread_cond_signal(&hmoq_cond[pnm->pthr->send_thread_mutex_num]);
	pthread_mutex_unlock(&hmoq_mutex[pnm->pthr->send_thread_mutex_num]);

}

void*
remove_hmoq_item(int num)
{
	http_mgr_output_q_t * phmoq;
	// load one task from queue.
	pthread_mutex_lock(&hmoq_mutex[num]);
	if(hmoq_head[num] == NULL) 
	{
		hmoq_plast[num] = NULL;
		pthread_mutex_unlock(&hmoq_mutex[num]);
		return NULL;
	}
	phmoq = hmoq_head[num];
	hmoq_head[num] = hmoq_head[num]->next;
	if (hmoq_head[num] == NULL) {
		hmoq_plast[num] = NULL;
	}
	pthread_mutex_unlock(&hmoq_mutex[num]);
	return phmoq;
}

#if 1
void
mime_header_check_and_copy(char * restrict bitmap,
			   dataddr_t * restrict tocopy,
			   dataddr_t * restrict fromcopy)
{
	int n = 0;
	int kcnt = 0;
	for ( 	 ; n < MIME_MAX_HDR_DEFS ; ++n) {
		if (isset(bitmap, n)) {
			tocopy[n] = fromcopy[kcnt++];
		}
	}
} 

void
mime_header_update_hdrcnt(mime_header_t *hd)
{
	int n =0;
	int kcnt;
    	heap_data_t * pheap_data;
	dataddr_t hpd;
	for (; n<MIME_MAX_HDR_DEFS; ++n) {
		if(hd->known_header_map[n] == 0) continue;
		kcnt = 0;
		hpd = hd->known_header_map[n];
	     	while (hpd) {
			kcnt++;
                	pheap_data = OFF2PTR(hd, hpd);
                	if (pheap_data->u.v.value_next == 0) break;
			hpd = pheap_data->u.v.value_next;
            	}
	     	pheap_data = OFF2PTR(hd, hd->known_header_map[n]);
	     	pheap_data->u.v.hdrcnt = kcnt;
	}
}
#endif

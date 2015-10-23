/*
 * Implementation of the cache manager
 *
 * The cache manager implements the in memory buffer and caching system.
 * Memory is physically managed in fixed size pages.
 * A buffer (currently called video_obj_t) maps a logical ID (chunk of an
 * object) to a page.  Currently, there is a 1:1 relationship between a buffer
 * and a page.  However, we may relax this in the future to allow more
 * efficient use of memory.
 * 
 * Buffers are reference counted to allow sharing across sessions. 
 * Buffers are cached via a hash table.  Buffers that are not in use
 * are put on an eviction list to be reused as needed based on an LRU
 * policy.  We will investigate more elaborate policies down the road as
 * needed.
 */
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <time.h>
#include "nkn_defs.h"
#include "cache_mgr.h"
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_buf.h"
#include "nkn_am_api.h"
#include "cache_mgr_private.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_cfg_params.h"
#include "nkn_memalloc.h"
#include "nkn_cod.h"
#include "nkn_attr.h"
#include "nkn_assert.h"
#include <sys/prctl.h>

/* TBD - Existing counters to be supported */
extern long glob_bm_dyn_lruscale;
extern int glob_cachemgr_init_done;
extern AO_t provlistbytes[NKN_MM_MAX_CACHE_PROVIDERS];
unsigned long long glob_chunk_mgr_responses=0,
    glob_error_tasks=0,
    glob_disk_tasks=0,
    glob_chunk_mgr_cleanup=0,
    glob_chunk_mgr_output=0,
    glob_chunk_mgr_input=0,
    glob_no_content=0,
    glob_invalid_wakeup=0,
    glob_invalid_obj_or_task=0,
    glob_start_xmit=0,
    glob_cache_request_error=0,
    glob_bad_cm_request=0,
    glob_cr_mm_miss=0,
    glob_cr_stat_miss=0,
    glob_cr_data_miss,
    glob_cr_no_data,
    glob_cr_no_attr,
    glob_cm_inval_off=0,
    glob_cm_eof=0,
    glob_cr_invalid_request=0,
    glob_cr_bad_response=0,
    glob_cr_origin_expired=0,
    glob_cr_cod_expired=0,
    glob_cr_cod_got_expired=0,
    glob_cr_eagain=0,
    glob_nkn_buf_no_reval=0,
    glob_cr_conf_timed_wait=0,
    glob_cr_conf_timed_wait_failed=0,
    glob_cr_conf_timed_wait_done=0,
    glob_cr_uncond_timed_wait=0,
    glob_cr_uncond_timed_wait_failed=0,
    glob_cr_uncond_timed_wait_done=0,
    glob_cm_cr_error=0,
    glob_cm_wait_to_eagain=0,
    glob_cr_reval_conf_timed_wait = 0,
    glob_cr_reval_uncond_timed_wait = 0,
    glob_cr_invalid_eod_lenrequest = 0,
    glob_cr_sync_revalidate_tunnel = 0,
    glob_cr_object_notvalid_yet = 0,
    glob_bm_total_transit_buffers = 0,
    glob_cr_negcache_object_invalidated = 0,
    glob_cr_invalid_provider_attribute =0;

/* converting the following counters to Atomic Counters */
uint64_t glob_tot_size_from_cache=0,
    glob_tot_size_from_sata_disk=0,
    glob_tot_size_from_sas_disk=0,
    glob_tot_size_from_ssd_disk=0,
    glob_tot_size_from_disk=0,
    glob_tot_size_from_nfs=0,
    glob_tot_size_from_origin=0,
    glob_tot_size_from_tfm=0;

AO_t tot_size_from_cache[CUR_NUM_CACHE],
     tot_size_from_sata_disk[CUR_NUM_CACHE],
     tot_size_from_sas_disk[CUR_NUM_CACHE],
     tot_size_from_ssd_disk[CUR_NUM_CACHE],
     tot_size_from_disk[CUR_NUM_CACHE],
     tot_size_from_nfs[CUR_NUM_CACHE],
     tot_size_from_origin[CUR_NUM_CACHE],
     tot_size_from_tfm[CUR_NUM_CACHE];

AO_t glob_tot_hdr_size_from_cache=0,
    glob_tot_hdr_size_from_sata_disk=0,
    glob_tot_hdr_size_from_sas_disk=0,
    glob_tot_hdr_size_from_ssd_disk=0,
    glob_tot_hdr_size_from_disk=0,
    glob_tot_hdr_size_from_nfs=0,
    glob_tot_hdr_size_from_origin=0,
    glob_tot_hdr_size_from_tfm=0;

uint64_t glob_bm_cur_attrcnt = 0;  
extern AO_t bm_cur_attrcnt[CUR_NUM_CACHE];

void
cache_manager_threaded_init(void);
void * cache_manager_init(void *) ;
void chunk_mgr_input(nkn_task_id_t id);
void chunk_mgr_cleanup(nkn_task_id_t id);

/* Check the request against the object attributes */
static int
cr_valid_request(cache_response_t *cr)
{
	if (cr->attr && !nkn_attr_is_streaming(cr->attr) && cr->attr->content_length &&
	    ((uint64_t)cr->uol.offset >= cr->attr->content_length)) {
		DBG_LOG(ERROR, MOD_BM, "Invalid offset in GET request %s, %ld, Len = %ld", 
			nkn_cod_get_cnp(cr->uol.cod), cr->uol.offset,
			cr->attr->content_length);
		return 0;
	}
	return 1;
}

void nkn_update_combined_stats()
{
	int i;
	uint64_t provider[NKN_MM_MAX_CACHE_PROVIDERS];
	uint64_t totalbytes = 0;
	uint64_t internalcache = 0, externalcache = 0;

	if (!glob_cachemgr_init_done)
		return;
	glob_tot_size_from_cache = AO_load(&tot_size_from_cache[BM_CACHE_ALLOC]) +
				   AO_load(&tot_size_from_cache[BM_NEGCACHE_ALLOC]) +
				   AO_load(&glob_tot_hdr_size_from_cache);
	glob_tot_size_from_ssd_disk = AO_load(&tot_size_from_ssd_disk[BM_CACHE_ALLOC]) +
				AO_load(&tot_size_from_ssd_disk[BM_NEGCACHE_ALLOC]) +
				AO_load(&glob_tot_hdr_size_from_ssd_disk);
	glob_tot_size_from_sas_disk = AO_load(&tot_size_from_sas_disk[BM_CACHE_ALLOC]) +
				AO_load(&tot_size_from_sas_disk[BM_NEGCACHE_ALLOC]) +
				AO_load(&glob_tot_hdr_size_from_sas_disk);
	glob_tot_size_from_sata_disk = AO_load(&tot_size_from_sata_disk[BM_CACHE_ALLOC]) +
				AO_load(&tot_size_from_sata_disk[BM_NEGCACHE_ALLOC]) +
				AO_load(&glob_tot_hdr_size_from_sata_disk);
	glob_tot_size_from_origin = AO_load(&tot_size_from_origin[BM_CACHE_ALLOC]) +
				AO_load(&tot_size_from_origin[BM_NEGCACHE_ALLOC]) +
				AO_load(&glob_tot_hdr_size_from_origin);
	glob_tot_size_from_nfs = AO_load(&tot_size_from_nfs[BM_CACHE_ALLOC]) +
				AO_load(&tot_size_from_nfs[BM_NEGCACHE_ALLOC]) +
				AO_load(&glob_tot_hdr_size_from_nfs);
	nkn_buf_update_hitbytes(AO_load(&tot_size_from_cache[BM_CACHE_ALLOC]),
				BM_CACHE_ALLOC);
	nkn_buf_update_hitbytes(AO_load(&tot_size_from_cache[BM_NEGCACHE_ALLOC]),
				BM_NEGCACHE_ALLOC);
	AO_store(&glob_tot_size_from_disk, (AO_load(&glob_tot_size_from_ssd_disk) +
					    AO_load(&glob_tot_size_from_sas_disk) +
					    AO_load(&glob_tot_size_from_sata_disk)));

	i = 0;
	glob_bm_total_transit_buffers = nkn_buf_get_totaltransit_pages();
	while (i < NKN_MM_MAX_CACHE_PROVIDERS) {
	    provider[i] = AO_load(&provlistbytes[i]);
	    ++i;
	}
	i = 0;
	while (i < NKN_MM_MAX_CACHE_PROVIDERS) {
	    totalbytes += provider[i] ;
	    if (i < NKN_MM_max_pci_providers)
		internalcache += provider[i];
	    if (i >= NKN_MM_max_pci_providers && i < NKN_MM_MAX_CACHE_PROVIDERS)
		externalcache += provider[i];
	    ++i;
	}
	if (totalbytes != 0) {
	    glob_bm_dyn_lruscale = (long)((((((long double) internalcache) * MIN_DYN_LRU_SCALE) +
				(((long double) externalcache) * MAX_DYN_LRU_SCALE)) /
				((long double) totalbytes)));
	}

	if (!bm_cfg_dynscale) {
	    glob_bm_dyn_lruscale = 30 * GRANULAR_TIMESTAMP; 
	}
	// set current global attributes
	glob_bm_cur_attrcnt = AO_load(&bm_cur_attrcnt[BM_NEGCACHE_ALLOC]) +
			      AO_load(&bm_cur_attrcnt[BM_CACHE_ALLOC]);	
}

/* 
 * Report analytics.  We report a hit for an object when
 * the attributes are fetched.  Do no record hits for internal
 * fetches (e.g. to promote an object), no data or cache bypass 
 * requests.
 */

int cr_am_report_interval = 3;		// could be configurable
static void
cr_report_am_hits(cache_response_t *cr, cmpriv_t *cp, const char **uri)
{
	am_object_data_t am_object_data;
	nkn_provider_type_t provider;
	uint32_t am_hits = 0 ;
	uint32_t update = 0;
	void *iptr = NULL;
	am_ext_info_t *am_ext_info = NULL;
	nkn_attr_t *ap = NULL;
	nkn_hot_t hotval = 0;
	int push_in_progress = 0;
	AM_pk_t pk;
	int i;

	if ((cp && ((cp->attrbuf && (cp->attrbuf->flags & NKN_BUF_CACHE)) &&
		!(cp->flags & CM_PRIV_NOAM_HITS) && /* No need to update AM. */
		!(cr->in_rflags &
		(CR_RFLAG_INTERNAL|CR_RFLAG_NO_CACHE|CR_RFLAG_NO_DATA|
		CR_RFLAG_NO_AMHITS)))) ||
	    ((cr->provider > NKN_MM_max_pci_providers) &&
		(push_in_progress = nkn_cod_is_push_in_progress(cr->uol.cod)))) {

		pk.key_len = 0;

		/* cp->attrbuf will not be set for continual requests
		 */
		if (push_in_progress) {
			/* This is Push in progress case */
			provider = cr->provider;
		} else if (cp->attrbuf) {
			ap = (nkn_attr_t *)nkn_buf_get_content(cp->attrbuf);
			// If object is from negative cache then avoid am hit
			if ((ap->content_length &&
				ap->content_length < cr->nscfg_contlen) ||
				(nkn_attr_is_negcache(ap))) {
				//nkn_am_log_error(uri, NKN_MM_CONF_SMALL_OBJECT_SIZE);
				return;
			}
			provider = nkn_buf_get_provider(cp->attrbuf);
			//provider = cr->provider;
			if ((cr->in_rflags & CR_RFLAG_FUSE_HITS) &&
				(provider != OriginMgr_provider) &&
				(provider != RTSPMgr_provider))
				return;
			nkn_obj_t *objp = (nkn_obj_t *)cp->attrbuf;
			iptr = nkn_cod_update_hit_offset(cr->uol.cod,
				    cr->uol.offset, objp, &am_hits, &update, uri);
			if (*uri == NULL) {
				*uri = nkn_cod_get_cnp(cr->uol.cod);
			}
			pk.name = (char *)*uri;
		}

		pk.provider_id = provider;

		if (update || (push_in_progress &&
				(cr->provider > NKN_MM_max_pci_providers))) {
			/* Update client related data to AM only for the first
			 * request
			 */
			if (update) {
				memcpy(&am_object_data.client_ipv4v6,
					&cr->in_client_data.ipv4v6_address,
					sizeof(ip_addr_t));
				am_object_data.client_port
					= cr->in_client_data.port;
				am_object_data.client_ip_family
					= cr->in_client_data.client_ip_family;

				am_object_data.flags =
					(cr->in_rflags &
					     CR_RFLAG_PREFETCH_REQ)?AM_IGNORE_HOTNESS:0;
				if (ap && ap->na_flags & NKN_OBJ_COMPRESSED) {
					am_object_data.flags |= AM_COMP_OBJECT;
				}
				if (ap)
					hotval = ap->hotval;

				am_object_data.proto_data = cr->in_proto_data;
				memcpy(&am_object_data.origin_ipv4v6,
					&cp->out_proto_resp.u.OM.remote_ipv4v6,
					sizeof(ip_addr_t));
				am_object_data.origin_port
					= cp->out_proto_resp.u.OM.remote_port;
				am_object_data.ns_token = cr->ns_token;
				am_object_data.host_hdr = NULL;
				nkn_cod_set_push_in_progress(cr->uol.cod);
			} else {
			/* Continuation request */
				am_object_data.flags    = AM_PUSH_CONT_REQ;
				am_object_data.ns_token = cr->ns_token;
			}
			am_object_data.in_cod           = cr->uol.cod;

			/* In case of origin providers, send buffer to AM */
			if ((update && (provider >= NKN_MM_max_pci_providers)) ||
			    cr->provider >= NKN_MM_max_pci_providers) {
				am_ext_info = nkn_mm_get_memory(
					    nkn_am_xfer_ext_data_pool[t_scheduler_id]);
				if (am_ext_info && cp) {
					for (i=0;i<cp->num_bufs;i++) {
						am_ext_info->buffer[i] = cp->bufs[i];
					}
					am_ext_info->num_bufs = cp->num_bufs;
				}
				if(am_ext_info)
				    AO_fetch_and_add1(&nkn_am_ext_data_in_use[t_scheduler_id]);
			}

			AM_inc_num_hits(&pk, am_hits, hotval,
						(void *)cp->attrbuf,
						&am_object_data,
						am_ext_info);
		}
		if (iptr) {
			nkn_mm_resume_ingest(iptr, (char *)*uri, cr->uol.offset,
					     INGEST_NO_CLEANUP);
		}
	} else {
		if(!(cr->in_rflags & CR_RFLAG_INTERNAL))
			nkn_cod_update_client_offset(cr->uol.cod, cr->uol.offset);
	}
}

/* Update the status of the requst to return back to the source module */
static void
cr_update_status(nkn_task_id_t id, cache_response_t *cr, cmpriv_t *cp, nkn_task_action_t action, int errcode)
{
	const char *uri = NULL;

	if (cp) {
		cr->provider = cp->provider;
		if (cp->flags & CM_PRIV_UNCOND_RETRY_STATE) {
			// If uncod retry stat is present then wake-up non-cp
			// tasks from cp->cr list with current response 
			// error and finally free the cache request context
			cr_free_uncond_retry(cp->cr, errcode, cp);
			free(cp->cr);
		}
	}
	cr->errcode = errcode;
	if (cp) {
		cr->out_proto_resp = cp->out_proto_resp;
	}

	if(errcode) {
		glob_cm_cr_error++;
		goto out;
	}

	if (cr->attr && (cr->length_response > (int)cr->attr->content_length))
		glob_cr_bad_response++;

	// Do not update for internal requests
	if (cr->in_rflags & CR_RFLAG_INTERNAL) {
		goto out;
	}


	if (!(cr->in_rflags & CR_RFLAG_ING_ONLYIN_RAM)) {
		cr_report_am_hits(cr, cp, &uri);
	}

	if (uri) {
		DBG_LOG(MSG, MOD_BM, "BM GET %s %ld %d", uri, cr->uol.offset, cr->length_response);
		if (cr->in_rflags & CR_RFLAG_TRACE)
			DBG_TRACE("Object %s bytes %ld to %ld served from %s", 
					uri,
					cr->uol.offset,
					cr->uol.offset + cr->length_response,
					nkn_provider_name(cr->provider));
	}

	//cr_update_provider_stats(cr);

out:
	cr->magic = CACHE_RESP_RESP_MAGIC;
       	nkn_task_set_action_and_state(id, action, TASK_STATE_RUNNABLE);
	return;
}


inline static void
update_provider_stats(nkn_provider_type_t provider, int ftype, off_t length)
{
	switch(provider) {
		case OriginMgr_provider:
			AO_fetch_and_add(&tot_size_from_origin[ftype], length);
			break;
		case SolidStateMgr_provider:
			AO_fetch_and_add(&tot_size_from_ssd_disk[ftype], length);
			break;
		case SAS2DiskMgr_provider:
			AO_fetch_and_add(&tot_size_from_sas_disk[ftype], length);
			break;
		case SATADiskMgr_provider:
			AO_fetch_and_add(&tot_size_from_sata_disk[ftype], length);
			break;
		case NFSMgr_provider:
			AO_fetch_and_add(&tot_size_from_nfs[ftype], length);
			break;
		default:
			break;
	}
}
/* Collect buffers from the cache or the media request response */
static void
cr_get_buffers(cache_response_t *cr, cmpriv_t *cp, cache_req_t *cq)
{
	nkn_uol_t uol, actuol;
	int num_bufs=0, cq_index=0;
	off_t gotlen=0, needed, endhit;
	nkn_provider_type_t provider;
	off_t object_size = -1;
	int ateof = 0, use, ftype;
	nkn_attr_t *attr=0;
	int flag = NKN_BUF_IGNORE_EXPIRY;

	// For no-cache requests, we will only find buffers from the MM
	// request structure
	if ((cr->in_rflags & CR_RFLAG_NO_CACHE) && !cq)
		return;

	// No data requested
	if (cr->in_rflags & CR_RFLAG_NO_DATA){
		cp->provider = BufferCache_provider;	
		return;
	}

	if ((cr->in_rflags & CR_RFLAG_INTERNAL) ||
		(cr->in_rflags & CR_RFLAG_IGNORE_COUNTERSTAT)) {
		flag |= (NKN_BUF_IGNORE_STATS | NKN_BUF_IGNORE_COUNT_STATS);
	}

	if (cr->in_rflags & CR_RFLAG_MFC_PROBE_REQ)
		flag |= NKN_BUF_IGNORE_COUNT_STATS;

	assert(cr->uol.length >= 0);
	needed = cr->uol.length;
	if (needed == 0)
		needed = CM_MEM_PAGE_SIZE*MAX_CM_IOVECS;
	memcpy(&uol, &cr->uol, sizeof(nkn_uol_t));
	actuol.offset = uol.offset;
	actuol.length = uol.length;
	while ((num_bufs < MAX_CM_IOVECS) && (gotlen < needed) && !ateof) {
		nkn_buf_t *bp;
		off_t buflen, bpoff, bplen;
#ifdef NKN_BUF_DATACHECK
		char datacheck[16];
#endif

		/*
		 * We copy out the offset and length from the buf structure
		 * to avoid a race condition with an update to a partial
		 * buffer.  Rather than explicit locking, we copy the
		 * length field before the offset field.  That way a race
		 * condition will simply mean that we get a smaller
		 * part of the page that is still valid.  The update code
		 * (see nkn_buf_setid) write the memory first and then
		 * updates offset and length in that order.
		 */
		if (cq) {		// from MM
			/* 
			 * If there is a media response, check if the next 
			 * buffer is valid and matches what we need.  
			 */
			if (cq_index >= cq->out_bufs)
				break;
			bp = cq->bufs[cq_index];
			bplen = bp->id.length;	// NOTE: length before offset
			bpoff = bp->id.offset;
			cq_index++;
			if (!(bp->flags & NKN_BUF_ID))
				continue;
			if (uol.cod != bp->id.cod)
				continue;
			if (uol.offset < bpoff)
				continue;
			if (uol.offset >= (bpoff + bplen))
				continue;
			// A non-cacheable buffer must match the requestor
		    	if (!(bp->flags & NKN_BUF_CACHE) && 
			     (cq->cp != cp)) {
				break;
			}
			nkn_buf_dup(bp, flag, &use, &uol, &actuol, &endhit);
		} else {		// lookup cache
			// expiry check is now done based on COD
			//bp = nkn_buf_get(&uol, NKN_BUFFER_DATA, (NKN_BUF_IGNORE_EXPIRY | flag));	
			bp = nkn_buf_getdata(&uol, flag, &use, &actuol, &endhit);
			if (bp == NULL)
				break;
			bplen = bp->id.length;	// NOTE: length before offset
			bpoff = bp->id.offset;
			// Check for partial page with no overlap
			if ((uol.offset < bpoff) ||
			    (uol.offset >= (bpoff + bplen))) {
				nkn_buf_put_downhits(bp, flag, endhit);
				break;
			}
		}

		/* The request offset must fit within the page */
		assert((bpoff <= uol.offset) &&
		      ((uol.offset-bpoff) < CM_MEM_PAGE_SIZE));

#ifdef NKN_BUF_DATACHECK
		snprintf(datacheck, 16, "%8d\n", (int)(uol.offset/CM_MEM_PAGE_SIZE));
		if (strncmp(datacheck, (char *)bp->buffer, 8)) {
			DBG_LOG(ERROR, MOD_BM, "Data mismatch");
		}
#endif

		/*
		 * Determine the usable length.
		 */
		buflen = bplen - (uol.offset-bpoff);

		/*
  		 * Make sure we do not return data past the object size.
		 * Ideally this should be prevented by the providers, but it
		 * is hard to enforce that in all cases (e.g. due to an
		 * errant origin server).
		 */
		if (object_size == -1) {
			if (cp->attrbuf) {
				attr = nkn_buf_get_content(cp->attrbuf);
			}
			else {
				attr = nkn_buf_get_attr(bp);
			}
			if (attr && !nkn_attr_is_streaming(attr))
				object_size = attr->content_length;
		}
		if (object_size > 0) {
			if (uol.offset > object_size) {
				nkn_buf_put(bp);
				cp->errcode = NKN_BUF_INVALID_OFFSET;
				glob_cm_inval_off++;
				return;
			}
			if ((uol.offset + buflen) > object_size) {
				buflen = object_size - uol.offset;
				glob_cm_eof++;
				DBG_LOG(ERROR, MOD_BM, "Truncated GET at content length for %s, %ld", 
					nkn_cod_get_cnp(uol.cod), object_size);
				if (buflen == 0)
					break;
				ateof = 1;	/* exit loop */
			}
		}

		// Truncate to requested length.
		gotlen += buflen;
		if (gotlen > needed) {
			off_t extra;

			extra = gotlen - needed;
			buflen -= extra;
			gotlen = needed;
		}

		cp->bufs[num_bufs] = bp;
		provider = bp->provider;

		/*
		 * Set the provider to be returned to the caller.
		 * If the buffer is coming from the cache and 
		 * has a use count of greater than
		 * one, we treat it as served from RAM.
		 * TBD.  We need to do a per buffer accounting of stats
		 * here to record data served from each provider.
		 */
		if (!(flag & NKN_BUF_IGNORE_COUNT_STATS)) {
			ftype = bp->ftype;
			if (use > 1) {
				if (uol.offset < endhit) {
					AO_fetch_and_add(&tot_size_from_cache[ftype], buflen);
					if(attr)
					    AO_fetch_and_add(&attr->cached_bytes_delivered,
							    buflen);
					if (cp->provider == Unknown_provider)
						cp->provider = BufferCache_provider;
				} else {
					update_provider_stats(provider, ftype, buflen);
					if(provider < NKN_MM_max_pci_providers && attr)
					    AO_fetch_and_add(&attr->cached_bytes_delivered,
							    buflen);
					if (cp->provider == Unknown_provider)
						cp->provider = provider;
				}
			} else {
				update_provider_stats(provider, ftype, buflen);
				if(provider < NKN_MM_max_pci_providers && attr)
				    AO_fetch_and_add(&attr->cached_bytes_delivered,
						    buflen);
				if (cp->provider == Unknown_provider)
					cp->provider = provider;
			}
		}

		/* 
		 * Set the corresponding iovec in the response structure.
		 * Both cacheable and non-cacheable pages follow alignment 
		 * and offset rules>
		 */
		cr->content[num_bufs].iov_base = (char*)(bp->buffer) + (uol.offset%CM_MEM_PAGE_SIZE);
		cr->content[num_bufs].iov_len = buflen;
		num_bufs++;

		uol.offset += buflen;
	}
	cp->num_bufs = num_bufs;
	cp->gotlen = gotlen;
}

/*
 * Assign the attribute buffer in the cache response structure from the
 * attribute cache or the cache request data.  The reference to the buffer
 * is kept in the cache private structure and released when the task is freed.
 */
static void
cr_get_attr(cache_response_t *cr, cmpriv_t *cp, cache_req_t *cq)
{
	int use;
	if (cq && cq->attrbuf) {
		// Check if the attribute buffer is for this request since
		// multiple objects could be serviced by a request.
		// Also, in case the object is not in the cache, only the
		// actual requestor can consume it.  The other requestors
		// must make a separate request.
		if ((cr->uol.cod == cq->attrbuf->id.cod) &&
		    ((cq->attrbuf->flags & NKN_BUF_CACHE) || (cq->cp == cp))) {
			nkn_buf_dup(cq->attrbuf, 0, &use, NULL, NULL, NULL);// ref counted
			cp->attrbuf = cq->attrbuf;
			if ((cp->flags & CM_PRIV_REVAL_ALL) &&
                           (cp->ns_reval_barrier ==NS_REVAL_ALWAYS_TIME_BARRIER)
			   && !(cp->flags & CM_PRIV_SYNC_REVALALWAYS)) {
                                if (!(cp->flags & CM_PRIV_REVAL_DONE)) {
                                        cache_revalidate(cp->attrbuf,
                                                 cp->ns_token, NULL,
                                                 &cr->in_client_data,
                                                 FORCE_REVAL_ALWAYS,
                                                 cr->in_proto_data);
				}
                        }
		} else if (cq->cp != cp) {
			// If there were multiple requestors, we have
			// to reissue the request.
			cp->flags &= ~CM_PRIV_TWICE;
			glob_cr_stat_miss++;
		}
	} else if (!(cr->in_rflags & CR_RFLAG_NO_CACHE)) {
		// Lookup attribute cache only if the "no-cache" flag is not specified
		int bflag = NKN_BUF_IGNORE_EXPIRY;

		if (cr->in_rflags & CR_RFLAG_INTERNAL)
			bflag |= NKN_BUF_IGNORE_STATS;
		cp->attrbuf = nkn_buf_get(&cr->uol, NKN_BUFFER_ATTR, bflag);	// ref counted
	}
	if (cp->attrbuf)
		cr->attr = (nkn_attr_t *)nkn_buf_get_content(cp->attrbuf);
}

static void
cr_get_attr_for_length(cache_response_t *cr, cmpriv_t *cp)
{
        int bflag = NKN_BUF_IGNORE_EXPIRY;

        if (cr->in_rflags & CR_RFLAG_INTERNAL)
                bflag |= NKN_BUF_IGNORE_STATS;
        cp->attrbuf = nkn_buf_get(&cr->uol, NKN_BUFFER_ATTR, bflag);    // ref counted
	if (cp->attrbuf)
		cr->attr = (nkn_attr_t *)nkn_buf_get_content(cp->attrbuf);
}

/*
 * Ensure that we have not done this before for this task and then
 * put the task into timed wait.
 */
static void
cr_do_conflict_timed_wait(nkn_task_id_t id,
			cache_response_t *cr, cmpriv_t *cp)
{
	int ret;
	if (cp->flags & CM_PRIV_CONF_TIMED_WAIT) {
		DBG_LOG(ERROR, MOD_BM, "Repeat request to retry for %s",
				nkn_cod_get_cnp(cr->uol.cod));
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_WAIT_LOOP);
		return;
	}
	cp->flags |= CM_PRIV_CONF_TIMED_WAIT;
	cp->errcode = 0;
	ret = nkn_task_timed_wait(id, cp->delay_ms);
	if (ret) {
		DBG_LOG(ERROR, MOD_BM, "Conflict event wait failed for %s, %d",
				nkn_cod_get_cnp(cr->uol.cod), cp->delay_ms);
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_WAIT_FAILED);
		glob_cr_conf_timed_wait_failed++;
		return;
	}
	glob_cr_conf_timed_wait++;
}

static void
cr_do_uncond_timed_wait(nkn_task_id_t id,
			cache_response_t *cr, cmpriv_t *cp)
{
	int ret;
	if (cp->flags & CM_PRIV_UNCOND_TIMED_WAIT &&
		cp->retries == cp->max_retries) {
		DBG_LOG(ERROR, MOD_BM, "Repeat request to retry for %s max retries %d",
				nkn_cod_get_cnp(cr->uol.cod), cp->retries);
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_WAIT_LOOP);
		return;
	}
	cp->flags |= CM_PRIV_UNCOND_TIMED_WAIT;
	++cp->retries;
	cp->errcode = 0;
	ret = nkn_task_timed_wait(id, cp->delay_ms);
	if (ret) {
		DBG_LOG(ERROR, MOD_BM, "Un conditional Event wait failed for %s, %d",
				nkn_cod_get_cnp(cr->uol.cod), cp->delay_ms);
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_WAIT_FAILED);
		glob_cr_uncond_timed_wait_failed++;
		return;
	}
	glob_cr_uncond_timed_wait++;
}

/* 
 * Input Task Handler.
 */
void
chunk_mgr_input(nkn_task_id_t id)
{
	cmpriv_t *cp;
	const namespace_config_t * ns_cfg;
	int force_revalchk = 0;
	int ret, errcode;
	uint32_t force_revalidate = NO_FORCE_REVAL;
	uint32_t maxage_reval;
	time_t cache_create;
	cache_response_t *cr = (cache_response_t *) nkn_task_get_data(id);
	namespace_token_t ns_token = cr->ns_token;
	int in_rflags = cr->in_rflags;
	nkn_cod_t cod = cr->uol.cod;
	off_t length = cr->uol.length;

	assert(cr->magic == CACHE_RESP_REQ_MAGIC);

	/*
	 * Check if our private data is already set (i.e. we are coming
 	 * back from completion of an MM task).  Otherwise, we need to
	 * allocate one.
	 * TBD - avoid this allocation for the common case of a buffer hit.
	 * This may require an API change and semantics of where
	 * the buffer is released.
	 */
	cp = nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, id);
	if (cp == NULL) {
		cp = nkn_calloc_type(1, sizeof(cmpriv_t), mod_bm_cmpriv_t);
		if (cp == NULL) {
			cr_update_status(id, cr, NULL, TASK_ACTION_OUTPUT, ENOMEM);
			return;
		}
		cp->magic = CM_PRIV_MAGIC;
		nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, id, (void *)cp);
		cp->provider = Unknown_provider;
		cp->ns_token = ns_token;
		cp->id = id;
		cp->encoding_type = -1;
		glob_chunk_mgr_input++;
		if ((in_rflags & CR_RFLAG_FIRST_REQUEST) &&
			!(in_rflags & CR_RFLAG_NO_REVAL)) {
			if (in_rflags & CR_RFLAG_DELIVER_EXP) {
				cp->flags |= CM_PRIV_DELIVER_EXP;
			} else {
				cp->flags |= CM_PRIV_SYNC_REVAL;
			}
			if (in_rflags & CR_RFLAG_POLICY_REVAL) {
				ns_cfg = namespace_to_config(cr->ns_token);
				if (ns_cfg != NULL) {
					if (ns_cfg->http_config &&
						ns_cfg->http_config->policies.reval_type == NS_OBJ_REVAL_ALL) {
						cp->flags |= CM_PRIV_REVAL_ALL;
						cp->ns_reval_barrier =
							ns_cfg->http_config->policies.reval_barrier;
						if (cr->in_rflags & CR_RFLAG_SYNC_REVALWAYS) {
							cp->flags |= (CM_PRIV_SYNC_REVALALWAYS
									| CM_PRIV_SYNC_REVALTUNNEL);
						} else if (ns_cfg->http_config->policies.reval_trigger ==
							NS_OBJ_REVAL_INLINE) {
							cp->flags |= CM_PRIV_SYNCQUEUE_REVALALWAYS;
						}
					}
					release_namespace_token_t(ns_token);
				} else {
					DBG_LOG(ERROR, MOD_BM, "cannot open namespace config gen=%d, value=%d",
					ns_token.u.token_data.gen, ns_token.u.token_data.val);
					cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_INVALID_INPUT);
					return;
				}
			}
		}

		/* validate input params */
#ifdef TMPCOD
		if (nkn_cod_get_status(cr->uol.cod) == NKN_COD_STATUS_INVALID) {
			cp->tmpcod = nkn_cod_open(cr->uol.uri, NKN_COD_BUF_MGR);
			if (cp->tmpcod == NKN_COD_NULL) {
				DBG_LOG(ERROR, MOD_BM, "COD open failed in GET request %s, %ld", cr->uol.uri, cr->uol.length);
				cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_INVALID_INPUT);
				return;
			}
			cr->uol.cod = cp->tmpcod;
		}
#endif
		if (length < 0) {
			glob_cr_invalid_request++;
			DBG_LOG(ERROR, MOD_BM, "invalid length in GET request %s, %ld", 
				nkn_cod_get_cnp(cod), length);
			cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_INVALID_INPUT);
			return;
		}
		if (!VALID_NS_TOKEN(ns_token)) {
			DBG_LOG(ERROR, MOD_BM, "invalid ns token in GET request %s", 
				nkn_cod_get_cnp(cod));
			cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_INVALID_INPUT);
			return;
		}
	} else {
		assert(cp->flags & (CM_PRIV_REQ_DONE|CM_PRIV_REVAL_DONE));

                /*! once request is done reset force reval barrier
                    only for higher providers*/
                if (cp->flags & CM_PRIV_REVAL_DONE) {
                        cp->flags &= ~(CM_PRIV_REVAL_ALL | CM_PRIV_SYNC_REVAL);
		}

		if (cp->errcode == NKN_OM_SERVER_NO_DATA) {
			cp->errcode = 0; 
		} 
		/* 
		 * If a previous request has failed, we need to return an 
		 * error unless there is an indication to retry the request. 
		 */
		if (cp->errcode) {
			// sync revalidate check before cod expiry check is needed
			if (cp->errcode == MM_FLAG_VALIDATE_TUNNEL) {
				glob_cr_sync_revalidate_tunnel++;
				cr_update_status(id, cr, cp, 
					    TASK_ACTION_OUTPUT, MM_FLAG_VALIDATE_TUNNEL);
				return;
			}

			// invalidated cod and version. mark disk provider as stale
			if (cp->errcode == NKN_OM_INV_PARTIAL_OBJ) {
				provider_cache_invalidate(&cr->uol);
				glob_cr_invalid_provider_attribute++;
				cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_OM_INV_PARTIAL_OBJ);
				return;
			}	
			// If the COD has been marked as expired, let the
			// caller know that so that a reopen could be used
			// to get fresh content.
			if (nkn_cod_get_status(cod) == NKN_COD_STATUS_EXPIRED) {
				DBG_LOG(MSG, MOD_BM, "COD expired while processing GET request %s, %ld", 
					nkn_cod_get_cnp(cod), length);
				glob_cr_cod_got_expired++;
				cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_VERSION_EXPIRED);
				return;
			}

			// If the provider has requested a retry after a delay
			// put the task in timed wait.
			if (cp->errcode == NKN_MM_CONF_RETRY_REQUEST) {
				cr_do_conflict_timed_wait(id, cr, cp);
				return;
			}

			if (cp->errcode == NKN_MM_UNCOND_RETRY_REQUEST) {
				cr_do_uncond_timed_wait(id, cr, cp);
				return;
			}

			if (cp->errcode == NKN_OM_CI_END_OFFSET_RESPONSE)
				cr->length_response = cp->gotlen;

			if  (cp->errcode != EAGAIN) {
				cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, cp->errcode);
				return;
			}
			glob_cr_eagain++;
			cp->errcode = 0;
		}
		if (!(cp->flags & CM_PRIV_CONF_TIMED_WAIT_DONE) &&
			(cp->flags & CM_PRIV_CONF_TIMED_WAIT)) {
			cp->flags |= CM_PRIV_CONF_TIMED_WAIT_DONE;
			cp->flags &= ~CM_PRIV_TWICE;
			glob_cr_conf_timed_wait_done++;
		}
		if ((cp->flags & CM_PRIV_UNCOND_TIMED_WAIT)) {
			// reset TIME_WAIT flag to retry again 
			// max retry variable tracks max retries to be done.
			cp->flags &= ~CM_PRIV_UNCOND_TIMED_WAIT;	
			cp->flags &= ~CM_PRIV_TWICE;
			glob_cr_uncond_timed_wait_done++;
		}

	}

	/* 
	 * Check if the attrubutes (if requested) and any portion of the 
	 * request content is already in the private struct or in the buffer 
	 * cache.  If so, return it immediately.  If we miss the attributes
	 * cache, we ignore the buffer cache and do the full cache request.
  	 * If needed, we can optimize this further by checking the buffer
	 * cache as well and then doing a cache request only for attributes
	 * only.
	 * TBD - Does it make sense to fire off a task to get the remaining
 	 * content here or wait for the caller to come back?
	 */
	if (in_rflags & CR_RFLAG_GETATTR) {
		/* 
		 * If the COD has expired and this the first request, return
		 * an error to the caller so that it can re-open the COD
		 * to get access to a new version.
		 */
#ifdef TMPCOD
		/* Reopen here until callers start to use the COD */
		if ((cp->tmpcod != NKN_COD_NULL) &&
		    (nkn_cod_get_status(cp->tmpcod) == NKN_COD_STATUS_EXPIRED)) {
			if (cp->attrbuf) {
				nkn_buf_put(cp->attrbuf);
				cr->attr = NULL;
				cp->attrbuf = NULL;
			}
			nkn_cod_close(cp->tmpcod, NKN_COD_BUF_MGR);
			cp->tmpcod = nkn_cod_open(cr->uol.uri, NKN_COD_BUF_MGR);
			if (cp->tmpcod == NKN_COD_NULL) {
				DBG_LOG(ERROR, MOD_BM, "COD re-open failed in GET request %s, %ld", cr->uol.uri, cr->uol.length);
				cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_VERSION_EXPIRED);
				return;
			}
			cr->uol.cod = cp->tmpcod;
		}
#endif
		if (nkn_cod_get_status(cod) == NKN_COD_STATUS_EXPIRED) {
			DBG_LOG(MSG, MOD_BM, "COD expired in GET request %s, %ld", 
				nkn_cod_get_cnp(cod), length);
			glob_cr_cod_expired++;
			cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_VERSION_EXPIRED);
			return;
		}

		if (!cr->attr)
			cr_get_attr(cr, cp, NULL);

                if (cr->attr) {
			 if (!(in_rflags & CR_RFLAG_CLIENT_INTERNAL) &&
				!(in_rflags & CR_RFLAG_INTERNAL) &&
				(cr->attr->cache_time0 > nkn_cur_ts)) {
				glob_cr_object_notvalid_yet++;
				cr_update_status(id, cr, cp,
				    TASK_ACTION_OUTPUT, NKN_FOUND_BUT_NOT_VALID_YET);
				return;
			}

			// if negative cache then if object expired
			if (nkn_attr_is_negcache(cr->attr)) { 
				if (cr->attr->cache_expiry <= nkn_cur_ts) {
				    nkn_cod_set_expired(cod);
				    glob_cr_negcache_object_invalidated++;
				    cr_update_status(id, cr, cp,
					TASK_ACTION_OUTPUT, NKN_BUF_VERSION_EXPIRED);
				    return;	
				}
				if (cr->attr->content_length == 0) {
				    cr->in_rflags |= CR_RFLAG_NO_DATA;
				    in_rflags = cr->in_rflags;
				}
				cp->flags &= (~(CM_PRIV_REVAL_ALL | CR_RFLAG_REVAL | CM_PRIV_SYNC_REVAL));
				cp->flags |= CR_RFLAG_NO_REVAL;
			}
			/* cache age = origin age + time it is present in cache */
			cache_create = cr->attr->origin_cache_create +
				       cr->attr->cache_correctedage;
			maxage_reval = ((in_rflags & CR_RFLAG_REVAL) && 
					(cr->req_max_age <
					(nkn_cur_ts - cache_create)) &&
					(cp->provider <= NKN_MM_max_pci_providers ));
                        if ((cp->flags & CM_PRIV_REVAL_ALL)) {
                                if ((cp->ns_reval_barrier != NS_REVAL_ALWAYS_TIME_BARRIER)) {
                                        if (cr->attr->cache_reval_time == 0) {
                                                if (cache_create < cp->ns_reval_barrier) {
                                                        force_revalchk = 1;
                                                        force_revalidate = FORCE_REVAL;
                                                }
                                        } else {
                                                if (cr->attr->cache_reval_time < cp->ns_reval_barrier) {
                                                        force_revalchk = 1;
                                                        force_revalidate = FORCE_REVAL;
                                                }
                                        }
                                }
                                else if ((cp->flags & CM_PRIV_SYNC_REVALALWAYS)){
                                        force_revalchk = 1;
                                        force_revalidate = FORCE_REVAL_ALWAYS;
				}
			} else if ((cp->flags & CM_PRIV_SYNC_REVAL) &&
					(cr->attr->na_flags & NKN_PROXY_REVAL)) {
					force_revalchk = 1;
					force_revalidate = FORCE_REVAL_ALWAYS;
					cp->flags |= CM_PRIV_SYNC_REVALALWAYS;
                        }
			// Trigger pre expiry revalidation if required.  We do this
			// only is case of a RAM cache hit and not when the object is
			// coming from a provider.  Otherwise, we end up with
			// extra requests when it is supplied from an origin provider.
			if ((cp->flags & CM_PRIV_REVAL_ALL) &&
			   (cp->ns_reval_barrier == NS_REVAL_ALWAYS_TIME_BARRIER)) {
				if (!(cp->flags & CM_PRIV_SYNC_REVALALWAYS) &&
				    !(cp->flags & CM_PRIV_REVAL_DONE)) {
					if (cp->flags & CM_PRIV_SYNCQUEUE_REVALALWAYS) {
						force_revalchk = 1;
						force_revalidate = FORCE_REVAL_ALWAYS;
					} else {
						uint32_t forcerevalflg = FORCE_REVAL_ALWAYS;
						if (force_revalidate == FORCE_REVAL)
					    		forcerevalflg |= FORCE_REVAL;
						if (maxage_reval && force_revalchk == 0)
					    		forcerevalflg |= FORCE_REVAL_COND_CHECK;
						int rev_ret = 1;
						if (!(cp->flags & CM_PRIV_INTERNAL_ASYNC_DONE)) {
							rev_ret = cache_revalidate(cp->attrbuf,
						 		 	   cp->ns_token, NULL,
						 		 	   &cr->in_client_data,
						 		  	   forcerevalflg,
						 		 	   cr->in_proto_data);
							// revalidation passed
							if (rev_ret == 0) {
								cp->flags |= 
									CM_PRIV_INTERNAL_ASYNC_DONE;
							}
							
						}
					}
				}
			} else if (!force_revalchk) {
				if (!(cp->flags & CM_PRIV_REVAL_DONE) &&
				    !(cr->in_rflags & CR_RFLAG_NO_REVAL)) {
					uint32_t forcerevalflg = NO_FORCE_REVAL;
					if (cp->flags & CM_PRIV_DELIVER_EXP)
						forcerevalflg |= NO_FORCE_REVAL_EXPIRED; 
					cache_revalidate(cp->attrbuf,
						 cp->ns_token, NULL,
						 &cr->in_client_data,
						 forcerevalflg, 
						 cr->in_proto_data);
				}
			}
                }

		// If the object has expired, we need to wait for a 
		// revalidation to occur unless we have already done it
		// or the object just came from the origin
		// It is ok to serve this request if a reval has been done
		// even though the expiry may be hit again (e.g. max-age=0).

		// BZ 3073 If max-age=0 comes from client , 
		// revalidation has to be forced if the data is not from origin
		if (cr->attr && 
		    !(cp->flags & (CM_PRIV_REVAL_DONE|CM_PRIV_FROM_ORIGIN)) &&
		    !(cp->flags & CM_PRIV_DELIVER_EXP) && cp->attrbuf &&
			(nkn_buf_has_expired(cp->attrbuf) ||
			(maxage_reval) ||
			(force_revalchk == 1))) {
			// Return error if the client does not want to trigger
			// a revalidation.
			if (in_rflags & CR_RFLAG_NO_REVAL) {
				glob_nkn_buf_no_reval++;
				nkn_buf_put(cp->attrbuf);
				cp->attrbuf = NULL;
				cr->attr = NULL;
				cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_VERSION_EXPIRED);
				return;
			}
			cp->id = id;
			if ((in_rflags & CR_RFLAG_REVAL) &&
				(force_revalchk == 0)) {
				force_revalidate = FORCE_REVAL_COND_CHECK;
			}
			ret = cache_revalidate(cp->attrbuf, cp->ns_token, cp,
					       &cr->in_client_data,
					       force_revalidate,
					       cr->in_proto_data);
			if (ret == 0) {
				return;
			}
			// cannot revalidate, expire the buffer and
			// treat as miss
			if (!(cp->flags & CM_PRIV_SYNC_REVALTUNNEL)) {
				nkn_buf_expire(cp->attrbuf);
				errcode = NKN_BUF_VERSION_EXPIRED;
			} else
				errcode = MM_FLAG_VALIDATE_TUNNEL;
			nkn_buf_put(cp->attrbuf);
			cp->attrbuf = NULL;
			cr->attr = NULL;
			DBG_LOG(MSG, MOD_BM, "Cache revalidate failed in GET request %s, %ld", 
				nkn_cod_get_cnp(cod), length);
			cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, errcode);
			return;
		}

		if (cr->attr && !cr_valid_request(cr)) {
			glob_cr_invalid_request++;
			cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_INVALID_OFFSET);
			return;
		}
		if (cr->attr && !cp->num_bufs)
			cr_get_buffers(cr, cp, NULL);
	} else if (!cp->num_bufs) {
		if ((in_rflags & CR_RFLAG_EOD_CLOSE)) {
			if (!cr->attr) {
				cr_get_attr_for_length(cr, cp);
				if (cr->attr) {
					if (!nkn_attr_is_streaming(cr->attr) &&
					cr->attr->content_length && 
					((uint64_t)cr->uol.offset >= cr->attr->content_length)) {
						glob_cr_invalid_eod_lenrequest++;
						nkn_buf_put(cp->attrbuf);
						cp->attrbuf = NULL;
						cr->attr = NULL;
						cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT,
						NKN_BUF_INV_EOD_OFFSET);
						return;
					}
					nkn_buf_put(cp->attrbuf);
                        		cp->attrbuf = NULL;
                        		cr->attr = NULL;
				}
			} else {
				if (!nkn_attr_is_streaming(cr->attr) &&
                                cr->attr->content_length &&
                                ((uint64_t)cr->uol.offset >= cr->attr->content_length)) {
                                         glob_cr_invalid_eod_lenrequest++;
                                         cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_INV_EOD_OFFSET);
                                         return;
                                }

			}
		} 
		cr_get_buffers(cr, cp, NULL);
	}

	// Look for errors (e.g. bad offset)
	if (cp->errcode) {
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, cp->errcode);
		return;
	}

	// Request is complete is we have buffers and requested attrs
	if ((cp->num_bufs || (in_rflags & CR_RFLAG_NO_DATA)) && 
	    (cr->attr || !(in_rflags & CR_RFLAG_GETATTR))) {

		cr->num_nkn_iovecs = cp->num_bufs;
		cr->length_response = cp->gotlen;

		cp->flags |= CM_PRIV_OUTPUT;
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, 0);
		return;
	}


	/* 
	 * If the COD is stale, we make sure that the cache providers
	 * do not return expired content.  Otherwise, we will keep
	 * expiring the COD and retrying the request.
	 */
	if (nkn_cod_get_status(cod) == NKN_COD_STATUS_STALE) {
		if (!(in_rflags & CR_RFLAG_REVAL) &&
			!(cp->flags & CM_PRIV_REVAL_ALL))
			cp->flags |= CM_PRIV_CHECK_EXPIRY;
		else {
			if (cr->in_rflags & CR_RFLAG_CACHE_ONLY) {
			    cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT,
					     NKN_MM_OBJ_NOT_FOUND_IN_CACHE);
			    return;
			}
			cp->flags |= CM_PRIV_NO_CACHE;
		}
	}
#ifdef TMPCOD
	/*
	 * Workaround for lack of versioning in the cache.
	 * By default, we ask the cache providers to ignore expiry since
	 * the revalidation logic is done in BM.  However, during the
	 * period where we are fetching an update version of the object,
	 * we don't want to serve up expired content from the cache.
	 * This should be handled via version checking.  For now, we
	 * prevent this by enforcing expiry checking when the attributes
	 * have been supplied by an Origin provider.
	 */
	if (!(cp->flags & CM_PRIV_CHECK_EXPIRY)) {
		nkn_buf_t *abp, *attrbuf = NULL;

		// Lookup for attributes if not already requested
		if (!(in_rflags & CR_RFLAG_GETATTR) &&
		    !cr->attr) {

			attrbuf = nkn_buf_get(&cr->uol, NKN_BUFFER_ATTR, NKN_BUF_IGNORE_STATS);   // ref counted	
			abp = attrbuf;
		} else
			abp = cp->attrbuf;
		if (abp && (abp->provider > NKN_MM_max_pci_providers))
			cp->flags |= CM_PRIV_CHECK_EXPIRY;
			
		if (attrbuf)
			nkn_buf_put(attrbuf);		// release ref count
	}
#endif

	/* 
	 * If we have already made a request before, we retry once
	 * to cover the corner case that the request did not get the
	 * required buffers due to cache movement.  Otherwise, we fail the
	 * request since we can't keep looping forever.
	 */
	if (cp->flags & CM_PRIV_REQ_DONE) {
		if (cp->flags & CM_PRIV_TWICE) {
			DBG_LOG(ERROR, MOD_BM, "No data from MM request %s, %ld",
				nkn_cod_get_cnp(cod), length);
			glob_cr_mm_miss++;
			cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, NKN_BUF_MMREQ_FAILED);
			return;
		}
		cp->flags &= ~(CM_PRIV_REQ_ISSUED|CM_PRIV_REQ_DONE);
		cp->flags |= CM_PRIV_TWICE;
	}

	/* Create a task for MM/OM and wait for the response */
	cp->id = id;
	/*
         * copy the client data for furthur requests/revalidation
	 */
	cp->crflags = in_rflags;
	cp->errcode = 0;
	// Force expiry check if no revalidation is specified
	if (in_rflags & CR_RFLAG_NO_REVAL)
		cp->flags |= CM_PRIV_CHECK_EXPIRY;
	// Force a separate request to the Origin.
	if (in_rflags & CR_RFLAG_SEP_REQUEST)
		cp->flags |= CM_PRIV_SEP_REQUEST;
	// BM only request
	if (in_rflags & CR_RFLAG_BM_ONLY)
		cp->flags |= CM_PRIV_BM_ONLY;

	memcpy(&cp->in_client_data,&cr->in_client_data,sizeof(nkn_client_data_t));
	cp->proto_data = cr->in_proto_data;
	// Ask for attributes if needed
	if ((in_rflags & CR_RFLAG_GETATTR) && !cr->attr)
		cp->flags |= CM_PRIV_GETATTR;

	DBG_LOG(MSG, MOD_BM, "BM CR %s %ld", 
		nkn_cod_get_cnp(cod), cr->uol.offset);
	ret = cache_request(&cr->uol, cp);
	if (ret) {
		cp->errcode = ret;
		cp->flags |= CM_PRIV_OUTPUT;
		cr_update_status(id, cr, cp, TASK_ACTION_OUTPUT, ret);
		return;
	}
	return;
}


/* Cleanup handler.  Put back the buffers we got and free the priv struct */
void
chunk_mgr_cleanup(nkn_task_id_t id)
{
	cmpriv_t *cp;
	int i;

	glob_chunk_mgr_cleanup++;
	cp = nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, id);
	if (cp == NULL)
		return;
	if (cp->flags & CM_PRIV_CLEANUP_DONE)
		return;

	// release reference for data and attribute buffers
	for (i=0; i<cp->num_bufs; i++) {
		if (cp->bufs[i]) {
			nkn_buf_put(cp->bufs[i]);
			cp->bufs[i] = NULL;
		}
	}
	if (cp->attrbuf) {
		nkn_buf_put(cp->attrbuf);
		cp->attrbuf = NULL;
	}
	cp->flags |= CM_PRIV_CLEANUP_DONE;

#ifdef TMPCOD
	if (cp->tmpcod != NKN_COD_NULL)
		nkn_cod_close(cp->tmpcod, NKN_COD_BUF_MGR);
#endif
	free(cp);
}

/* 
 * Handle completion of a cache revalidation request.  
 */
void 
cache_revalidation_done(cmpriv_t *cp)
{
	cache_response_t *cr = (cache_response_t *) nkn_task_get_data(cp->id);

	assert(cr->magic == CACHE_RESP_REQ_MAGIC);
	cp->flags |= CM_PRIV_REVAL_DONE;
	cr->in_rflags |= CR_RFLAG_RET_REVAL;
	/*
	 * On failure, expire the previous buffers and mark the failure so
	 * that we can ask the cache providers to honor expiry on the 
	 * subsequent cache request.
	 */
	if (cp->errcode && cp->errcode != MM_FLAG_VALIDATE_TUNNEL) {
		/*
		 * In case of failure, like file not found, leave the error code
		 * as is and do not update flags, as nothing needs to be done.
		 */
		if (cp->errcode != MM_VALIDATE_FAIL) {
			cp->flags |= CM_PRIV_CHECK_EXPIRY;
			cp->errcode = 0;
		}
		if (cp->attrbuf) {
			nkn_buf_expire(cp->attrbuf);
		}
	}
	/* 
	 * Release the existing attribute buffer so that the task can
	 * look up the updated one.
	 */
	if (cp->attrbuf) {
		nkn_buf_put(cp->attrbuf);
		cp->attrbuf = NULL;
		cr->attr = NULL;
	}
        nkn_task_set_state(cp->id, TASK_STATE_RUNNABLE);
}

/* 
 * Handle completion of a cache request.  Pickup buffers that match the
 * request directly.  This also takes care of buffer that are non-cacheable.
 * The remaining buffers will get picked up from the buffer cache on
 * subsequent requests.
 */
void 
cache_request_done(cmpriv_t *cp, cache_req_t *cq)
{
	cache_response_t *cr = (cache_response_t *) nkn_task_get_data(cp->id);
	nkn_attr_t *attr;

	assert(cr->magic == CACHE_RESP_REQ_MAGIC);
	cp->flags |= CM_PRIV_REQ_DONE;
	if (cp->errcode) {
		glob_cache_request_error++;
		// If I am not the requestor convert wait to eagain.  This
		// prevents all requestors from ending up in a wait state.
		if ((cp->errcode == NKN_MM_CONF_RETRY_REQUEST) &&
		    (cq->cp != cp)) {
			cp->errcode = EAGAIN;
			cp->flags &= ~CM_PRIV_TWICE;
			glob_cm_wait_to_eagain++;
		}
	} else {
		// Pickup attributes if requested and not present
		if ((cr->in_rflags & CR_RFLAG_GETATTR) && 
		    !cr->attr) {
			if (cq->attrbuf &&
		    	    (cq->attrbuf->flags & NKN_BUF_ID)) {
			        /*! once request is done reset force reval barrier
				 only for higher providers*/
				if (cq->attrbuf->provider > NKN_MM_max_pci_providers) {
					attr = (nkn_attr_t *)nkn_buf_get_content(cq->attrbuf);
					if ((cp->flags & CM_PRIV_SYNC_REVALALWAYS) ||
						(attr->na_flags & NKN_PROXY_REVAL)) {
						if (cq->cp == cp) {
							cp->flags &= ~(CM_PRIV_REVAL_ALL | CM_PRIV_SYNC_REVAL);
						}
					} else {
						cp->flags &= ~CM_PRIV_REVAL_ALL;
					}
				}
				cr_get_attr(cr, cp, cq);
				// If the object has expired, skip data buffers
				// and go back to the task.  It will trigger
				// the revalidation.
				if (cr->attr && nkn_buf_has_expired(cp->attrbuf)) {
					// Expired data from an origin 
					// We will serve it to this client
					// even if is expired.  Subsequent 
					// clients will revalidate.
					if (cp->attrbuf->provider > NKN_MM_max_pci_providers) {
						cp->flags |= CM_PRIV_FROM_ORIGIN;
						glob_cr_origin_expired++;
					} else {
						cp->flags &= ~CM_PRIV_TWICE;
						nkn_buf_put(cp->attrbuf);
						cp->attrbuf = NULL;
						cr->attr = NULL;
                				goto crdone_complete;	
					}
				}
			} else {
				if (cq->cp != cp)
					cp->flags &= ~CM_PRIV_TWICE;
				else
					glob_cr_no_attr++;
                		goto crdone_complete;	
			}
		}
		// Pickup buffers if not already assigned
		if (!cp->num_bufs) {
			cr_get_buffers(cr, cp, cq);
			if (!cp->num_bufs) {
				glob_cr_data_miss++;
				// Do not count request if piggybacking
				if (cq->cp != cp)
					cp->flags &= ~CM_PRIV_TWICE;
				else
					glob_cr_no_data++;	
			}
		}
	}
crdone_complete:
	// if uncod retry state present then cache request
	// context will be reused. so NULL assignment not done.
	if (!(cp->flags & CM_PRIV_UNCOND_RETRY_STATE))
		cp->cr = NULL;
        nkn_task_set_state(cp->id, TASK_STATE_RUNNABLE);
}

/*
 * Initialize CM
 */


 
void*
cache_manager_init(void * arg)
{

    	prctl(PR_SET_NAME, "nvsd-cachemgr-init", 0, 0, 0);
	AO_store(&tot_size_from_cache[BM_CACHE_ALLOC], 0);
	nkn_mon_add("bm.global.cache.size_from_buffer", NULL,
		    (void *)&tot_size_from_cache[BM_CACHE_ALLOC],
		    sizeof(tot_size_from_cache[BM_CACHE_ALLOC]));
	AO_store(&tot_size_from_cache[BM_NEGCACHE_ALLOC], 0);
	nkn_mon_add("bm.global.negcache.size_from_buffer", NULL,
		    (void *)&tot_size_from_cache[BM_NEGCACHE_ALLOC],
		    sizeof(tot_size_from_cache[BM_NEGCACHE_ALLOC]));

	AO_store(&tot_size_from_ssd_disk[BM_CACHE_ALLOC], 0);
	nkn_mon_add("bm.global.cache.size_from_ssd", NULL,
		    (void *)&tot_size_from_ssd_disk[BM_CACHE_ALLOC],
		    sizeof(tot_size_from_ssd_disk[BM_CACHE_ALLOC]));

	AO_store(&tot_size_from_ssd_disk[BM_NEGCACHE_ALLOC], 0);
	nkn_mon_add("bm.global.negcache.size_from_ssd", NULL,
		    (void *)&tot_size_from_ssd_disk[BM_NEGCACHE_ALLOC],
		    sizeof(tot_size_from_ssd_disk[BM_NEGCACHE_ALLOC]));

	AO_store(&tot_size_from_sas_disk[BM_CACHE_ALLOC], 0);
	nkn_mon_add("bm.global.cache.size_from_sas", NULL,
		    (void *)&tot_size_from_sas_disk[BM_CACHE_ALLOC],
		    sizeof(tot_size_from_sas_disk[BM_CACHE_ALLOC]));

	AO_store(&tot_size_from_sas_disk[BM_NEGCACHE_ALLOC], 0);
	nkn_mon_add("bm.global.negcache.size_from_sas", NULL,
		    (void *)&tot_size_from_sas_disk[BM_NEGCACHE_ALLOC],
		    sizeof(tot_size_from_sas_disk[BM_NEGCACHE_ALLOC]));

	AO_store(&tot_size_from_sata_disk[BM_CACHE_ALLOC], 0);
	nkn_mon_add("bm.global.cache.size_from_sata", NULL,
		    (void *)&tot_size_from_sata_disk[BM_CACHE_ALLOC],
		    sizeof(tot_size_from_sata_disk[BM_CACHE_ALLOC]));

	AO_store(&tot_size_from_sata_disk[BM_NEGCACHE_ALLOC], 0);
	nkn_mon_add("bm.global.negcache.size_from_sata", NULL,
		    (void *)&tot_size_from_sata_disk[BM_NEGCACHE_ALLOC],
		    sizeof(tot_size_from_sata_disk[BM_NEGCACHE_ALLOC]));


	AO_store(&tot_size_from_origin[BM_CACHE_ALLOC], 0);
	nkn_mon_add("bm.global.cache.size_from_origin", NULL,
		    (void *)&tot_size_from_origin[BM_CACHE_ALLOC],
		    sizeof(tot_size_from_origin[BM_CACHE_ALLOC]));

	AO_store(&tot_size_from_origin[BM_NEGCACHE_ALLOC], 0);
	nkn_mon_add("bm.global.negcache.size_from_origin", NULL,
		    (void *)&tot_size_from_origin[BM_NEGCACHE_ALLOC],
		    sizeof(tot_size_from_origin[BM_NEGCACHE_ALLOC]));

	AO_store(&tot_size_from_nfs[BM_CACHE_ALLOC], 0);
	nkn_mon_add("bm.global.cache.size_from_nfs", NULL,
		    (void *)&tot_size_from_nfs[BM_CACHE_ALLOC],
		    sizeof(tot_size_from_nfs[BM_CACHE_ALLOC]));

	AO_store(&tot_size_from_nfs[BM_NEGCACHE_ALLOC], 0);
	nkn_mon_add("bm.global.negcache.size_from_nfs", NULL,
		    (void *)&tot_size_from_nfs[BM_NEGCACHE_ALLOC],
		    sizeof(tot_size_from_nfs[BM_NEGCACHE_ALLOC]));

	cache_request_init();
	nkn_buf_init();
	glob_cachemgr_init_done = 1;
	printf("\nCache Manager Initialization Done\n");
	return NULL;
}

void
cache_manager_threaded_init(void)
{
	pthread_t pid;
	pthread_create(&pid, NULL, cache_manager_init, NULL);
}

void
nkn_buffer_setid(nkn_buffer_t *bp, nkn_uol_t *uol, nkn_provider_type_t provider,
	         u_int32_t flags)
{
	int ret;

#ifdef TMPCOD
	nkn_cod_t lcod = NKN_COD_NULL;
	if (nkn_cod_get_status(uol->cod) == NKN_COD_STATUS_INVALID) {
		uol->cod = nkn_cod_open(uol->uri, NKN_COD_BUF_MGR);
		lcod = uol->cod;
	}
#endif
	ret = nkn_buf_setid((nkn_buf_t *)bp, uol, provider, flags);
	assert(ret == 0);
#ifdef TMPCOD
	if (lcod != NKN_COD_NULL) {
		nkn_cod_close(uol->cod, NKN_COD_BUF_MGR);
		uol->cod = NKN_COD_NULL;
	}
#endif
}

/*
 * Check whether the buffer is in cache
 * Caution: This does not check whether the buffer is valid or not
 * It assumes that the buffer is valid and is ref counted
 */
int
nkn_buffer_incache(nkn_buffer_t *bp)
{
	return nkn_buf_incache((nkn_buf_t *)bp);
}

void
nkn_buffer_getid(nkn_buffer_t *bp, nkn_uol_t *uol)
{
	uol->cod = NKN_COD_NULL;
	nkn_buf_getid((nkn_buf_t *)bp, uol);
}

void *
nkn_buffer_getcontent(nkn_buffer_t *bp)
{
	return nkn_buf_get_content((nkn_buf_t *)bp);
}

void
nkn_buffer_setmodified(nkn_buffer_t *bufp)
{
	assert(bufp != NULL);
	nkn_obj_set_dirty((nkn_obj_t *)bufp);	
	cache_write_attributes((nkn_obj_t *)bufp, ATTRIBUTE_UPDATE);
}

void
nkn_buffer_setmodified_sync(nkn_buffer_t *bufp)
{
	assert(bufp != NULL);
	cache_write_attributes((nkn_obj_t *)bufp, ATTRIBUTE_SYNC);
}

nkn_buffer_t *
nkn_buffer_get(nkn_uol_t *uol, nkn_buffer_type_t type)
{
	nkn_buffer_t *bufp;
#ifdef TMPCOD
	nkn_cod_t lcod = NKN_COD_NULL;
	if (nkn_cod_get_status(uol->cod) == NKN_COD_STATUS_INVALID) {
		uol->cod = nkn_cod_open(uol->uri, NKN_COD_BUF_MGR);
		lcod = uol->cod;
	}
#endif
	bufp = (nkn_buffer_t *)nkn_buf_get(uol, type, NKN_BUF_IGNORE_STATS|NKN_BUF_IGNORE_EXPIRY);
#ifdef TMPCOD
	if (lcod != NKN_COD_NULL) {
		nkn_cod_close(uol->cod, NKN_COD_BUF_MGR);
		uol->cod = NKN_COD_NULL;
	}
#endif
	return bufp;
}

void
nkn_buffer_hold(nkn_buffer_t *bp)
{
	int tempuse;
	nkn_buf_dup((nkn_buf_t *)bp, NKN_BUF_IGNORE_STATS, &tempuse,
		    NULL, NULL, NULL);
}

void
nkn_buffer_release(nkn_buffer_t *bp)
{
	nkn_buf_put((nkn_buf_t *)bp);
}

/*
 * Ensure that the specified iov is still valid and release its reference
 * count.  We null out the reference from the cm private structure so that
 * we do not repeat this during task cleanup.
 */
void
nkn_cr_content_done(nkn_task_id_t id, int iov_num)
{
	cmpriv_t *cp;

	cp = nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, id);
	if (!cp) {
	    NKN_ASSERT(0);
	    return;
	}
	assert((unsigned int)cp->magic == CM_PRIV_MAGIC);

	assert(iov_num < cp->num_bufs);
	nkn_buf_put(cp->bufs[iov_num]);
	cp->bufs[iov_num] = NULL;
}

nkn_buffer_t *nkn_buffer_alloc(nkn_buffer_type_t type, int size, size_t flags)
{
	return nkn_buf_alloc(type, size, flags);
}

void
nkn_buffer_purge(nkn_uol_t *uol)
{
	nkn_buf_purge(uol);
}

nkn_provider_type_t
nkn_buffer_get_provider(nkn_buffer_t *bp)
{
	return nkn_buf_get_provider((nkn_buf_t *)bp);
}

void
nkn_buffer_set_provider(nkn_buffer_t *bp, nkn_provider_type_t provider)
{
	nkn_buf_set_provider((nkn_buf_t *)bp, provider);
}

int
nkn_buffer_get_bufsize(nkn_buffer_t *bp)
{
	return nkn_buf_get_bufsize((nkn_buf_t *)bp);
}

int
nkn_buffer_set_bufsize(nkn_buffer_t *bp, int bufsize, int pooltype)
{
	return nkn_buf_lock_set_bufsize((nkn_buf_t *)bp, bufsize, pooltype);
}

int
nkn_buffer_set_attrbufsize(nkn_buffer_t *bp, int entries, int tot_blob_bytes)
{
	return nkn_buf_set_attrbufsize((nkn_buf_t *)bp, entries, 
					tot_blob_bytes);
}

void
nkn_buffer_getstats(nkn_buffer_t *bp, nkn_buffer_stat_t *statp)
{
	nkn_buf_getstats((nkn_buf_t *)bp, statp);
}

/*
 * Debug/test routine to list the entries in the RAM cache.
 */
static void nkn_buffer_list_entries(char *pfx) __attribute__ ((unused));
static void
nkn_buffer_list_entries(char *pfx)
{
	nkn_cod_t cod, next;
	int ret, plen = 0;
	nkn_uol_t uol = {0,0,0};

	cod = NKN_COD_NULL;
	next = NKN_COD_NULL;
	if (pfx)
		plen = strlen(pfx);	
	do {
		nkn_buffer_t *abuf;

		ret = nkn_cod_get_next_entry(cod, pfx, plen, &next, NULL, NULL, NKN_COD_BUF_MGR);
		if (ret)
			break;
		cod = next;
		uol.cod = cod;
		abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
		if (abuf) {
			nkn_buffer_stat_t stbuf;
			nkn_attr_t *ap;
		
			nkn_buffer_getstats(abuf, &stbuf);
			ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
			printf("%s\t%d\t%ld\t%s\n", nkn_cod_get_cnp(cod),
				stbuf.hits, stbuf.bytes_cached,
				ctime(&ap->cache_expiry));
			nkn_buffer_release(abuf);
		}
		nkn_cod_close(next, NKN_COD_BUF_MGR);
	} while (next != NKN_COD_NULL);
}

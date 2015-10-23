// nkn defs
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_memalloc.h"

#include "lf_metrics_monitor.h"
#include "lf_err.h"
#include "lf_dbg.h"

static void lf_metrics_monitor_thread(void *data);
static void* event_timer_calloc_custom(uint32_t num, 
				       uint32_t size);

lf_app_list_t hyperv_list = {1, {"qemu:///system"}};

static void* 
event_timer_calloc_custom(uint32_t num, uint32_t size) 
{

    return nkn_calloc_type(num, size, mod_mfp_event_timer);
}

static int32_t diffTimevalToMs(struct timeval const* from,
                               struct timeval const * val) 
{
    //if -ve, return 0
    double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
    double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
    double diff = d_from - d_val;
    if (diff < 0)
        return 0;
    return (int32_t)(diff * 1000);
}

int32_t 
lf_metrics_create_ctx(lf_app_list_t *na_list, 
		      lf_app_list_t *vma_list,
		      uint32_t sampling_interval, 
		      uint32_t window_len,
		      uint32_t ext_window_len,
		      uint32_t max_tokens, lf_metrics_ctx_t **out)
{
    int32_t err = 0, i;
    lf_metrics_ctx_t *ctx = NULL;

    ctx = (lf_metrics_ctx_t *)\
	nkn_calloc_type(1, sizeof(lf_metrics_ctx_t),
			100);
    if (!ctx) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    ctx->sampling_interval = sampling_interval;
    ctx->window_len = window_len;
    ctx->num_tokens = max_tokens;
    ctx->monitor_thread = lf_metrics_monitor_thread;
    ctx->evt.ev = createEventTimer(event_timer_calloc_custom, NULL);
    if (!ctx->evt.ev) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    for (i = 0; i < LF_MAX_SECTIONS; i++) {
	ctx->section_map[i] =  LF_SECTION_AVAIL;
    }
    pthread_mutex_init(&ctx->lock, NULL);
    
    /**
     * create the vma connector
     */
    ctx->ci[LF_VM_SECTION].app_list = vma_list;
    ctx->ci[LF_VM_SECTION].sampling_interval = sampling_interval;
    ctx->ci[LF_VM_SECTION].window_len = window_len;
    ctx->ci[LF_VM_SECTION].num_tokens = max_tokens;
    ctx->ci[LF_VM_SECTION].connector_specific_params = \
	(void *)(&hyperv_list);
    err = lf_vm_connector_create(&ctx->ci[LF_VM_SECTION], 
				 (void**)(&ctx->vmc));
    if (err) {
	/* we can live without the VM */
	ctx->section_map[LF_VM_SECTION] = LF_SECTION_UNAVAIL;
	err = 0;
    }

    /* create the na connector */
    ctx->ci[LF_NA_SECTION].app_list = na_list;
    ctx->ci[LF_NA_SECTION].sampling_interval = sampling_interval;
    ctx->ci[LF_NA_SECTION].window_len = window_len;
    ctx->ci[LF_NA_SECTION].ext_window_len = ext_window_len;
    ctx->ci[LF_NA_SECTION].num_tokens = max_tokens;
    ctx->ci[LF_NA_SECTION].connector_specific_params = NULL;
    err = lf_na_connector_create(&ctx->ci[LF_NA_SECTION], 
				 (void**)(&ctx->nac));
    if (err) {
	goto error;
    }

    /* create the sys connector */
    ctx->ci[LF_SYSTEM_SECTION].app_list = NULL;
    ctx->ci[LF_SYSTEM_SECTION].sampling_interval = sampling_interval;
    ctx->ci[LF_SYSTEM_SECTION].window_len = window_len;
    ctx->ci[LF_SYSTEM_SECTION].num_tokens = max_tokens;
    ctx->ci[LF_SYSTEM_SECTION].connector_specific_params = NULL;
    err = lf_sys_connector_create(&ctx->ci[LF_SYSTEM_SECTION], 
				 (void**)(&ctx->sc));
    if (err) {
	goto error;
    }

    *out = ctx;

    return err;
 error:
    if (ctx->evt.ev) ctx->evt.ev->delete_event_timer(ctx->evt.ev);
    if (ctx) free(ctx);
  
    return err;
}

static void 
lf_metrics_monitor_thread(void *data)
{
    lf_metrics_ctx_t *lfm = NULL;
    lf_ev_timer_ctx_t *evt = NULL;
    struct timeval tv, tv1;
    struct tm *nowtm;
    int32_t elapsed, err = 0;
    lf_vm_connector_t *vmc;
    lf_na_connector_t *nac;
    lf_sys_connector_t *sc;
    time_t rawtime;
    struct tm timeinfo, *ptimeinfo = NULL;
    char *tbuf;
    
    lfm = (lf_metrics_ctx_t*)data;
    assert(lfm);

    memset(&tv, 0, sizeof(struct timeval));
    time (&rawtime);
    ptimeinfo = gmtime_r(&rawtime, &timeinfo );
    tbuf = lfm->tbuf;
    pthread_mutex_lock(&lfm->lock);
    strftime (tbuf, 80,"%Y-%m-%dT%XZ", &timeinfo);
    pthread_mutex_unlock(&lfm->lock);

    LF_LOG(LOG_NOTICE, LFD_METRICS_MONITOR, "monitor time: %s\n", tbuf);

    if (lfm->section_map[LF_VM_SECTION] == LF_SECTION_AVAIL) {
	vmc = lfm->vmc;
	vmc->update_all_app_metrics(lfm->sampling_interval, vmc);
    }
	
    nac = lfm->nac;
    err = nac->update_all_app_metrics(lfm->sampling_interval, nac);
    if (err) {
	lfm->section_map[LF_NA_SECTION] = LF_SECTION_UNAVAIL;
    } else {
	lfm->section_map[LF_NA_SECTION] = LF_SECTION_AVAIL;
    }

    sc = lfm->sc;
    sc->update_all_app_metrics(lfm->sampling_interval, sc);

    gettimeofday(&tv1, NULL);
    elapsed = diffTimevalToMs(&tv, &tv1);
    LF_LOG(LOG_NOTICE, LFD_METRICS_MONITOR, "time elapsed for "
	   "monitoring %d\n", elapsed);
    evt = &lfm->evt;
    tv.tv_usec += (lfm->sampling_interval * 1000);
    evt->ev->add_timed_event_ms(evt->ev, lfm->sampling_interval,
				lfm->monitor_thread, lfm);
      
}

int32_t
lf_metrics_get_filter_snapshot_ref(lf_metrics_ctx_t *lfm,
				   struct str_list_t **filter,
				   void **out,
				   void **ref_id)
{
    int32_t err = 0;

    assert(lfm);
    assert(out);

    if (lfm->section_map[LF_NA_SECTION] == LF_SECTION_AVAIL) {
	err = lfm->nac->get_filter_snapshot(lfm->nac, 
		        (void **)filter,
			&out[LF_NA_SECTION], 
			&ref_id[LF_NA_SECTION]);
	if (err) {
	    goto error;
	}
    }

#if 0
    if (lfm->section_map[LF_VM_SECTION] == LF_SECTION_AVAIL) {
	err = lfm->vmc->get_snapshot(lfm->vmc,
				     &out[LF_VM_SECTION],
				     &ref_id[LF_VM_SECTION]);
	if (err) {
	    goto error;
	}
    }

    err = lfm->sc->get_snapshot(lfm->sc,
			 &out[LF_SYSTEM_SECTION],
			 &ref_id[LF_SYSTEM_SECTION]);
    if (err) {
	goto error;
    }
#endif

 error:
    return err;

}

int32_t 
lf_metrics_get_snapshot_ref(lf_metrics_ctx_t *lfm,
			    void **out,
			    void **ref_id)
{
    int32_t err = 0;

    assert(lfm);
    assert(out);

    if (lfm->section_map[LF_NA_SECTION] == LF_SECTION_AVAIL) {
	err = lfm->nac->get_snapshot(lfm->nac, 
			&out[LF_NA_SECTION], 
			&ref_id[LF_NA_SECTION]);
	if (err) {
	    goto error;
	}
    }

    if (lfm->section_map[LF_VM_SECTION] == LF_SECTION_AVAIL) {
	err = lfm->vmc->get_snapshot(lfm->vmc,
				     &out[LF_VM_SECTION],
				     &ref_id[LF_VM_SECTION]);
	if (err) {
	    goto error;
	}
    }

    err = lfm->sc->get_snapshot(lfm->sc,
			 &out[LF_SYSTEM_SECTION],
			 &ref_id[LF_SYSTEM_SECTION]);
    if (err) {
	goto error;
    }

 error:
    return err;
}

int32_t 
lf_metrics_release_snapshot_ref(lf_metrics_ctx_t *lfm,
				void **ref_id)
{
    int32_t err = 0;

    assert(lfm);
    assert(ref_id);

    if (lfm->section_map[LF_NA_SECTION] == LF_SECTION_AVAIL) {
	err = lfm->nac->release_snapshot(lfm->nac, 
				     ref_id[LF_NA_SECTION]);
	if (err) {
	    goto error;
	}
    }

    if (lfm->section_map[LF_VM_SECTION] == LF_SECTION_AVAIL) {
	err = lfm->vmc->release_snapshot(lfm->vmc, 
					 ref_id[LF_VM_SECTION]);
	if (err) {
	    goto error;
	}
    }

    err = lfm->sc->release_snapshot(lfm->sc, 
				     ref_id[LF_SYSTEM_SECTION]);
    if (err) {
	goto error;
    }

 error:
    return err;
}

int32_t
lf_metrics_start_monitor(lf_metrics_ctx_t *lfm)
{
    int32_t err = 0;
    lf_ev_timer_ctx_t *evt = NULL;
    struct timeval tv;
    size_t stacksize;
    pthread_attr_t attr;

    assert(lfm);
    evt = &lfm->evt;
    pthread_attr_init(&attr);
    pthread_attr_getstacksize (&attr, &stacksize);
    stacksize = 512*1024;
    pthread_attr_setstacksize (&attr, stacksize);
    
    pthread_create(&evt->ev_thread, &attr, evt->ev->start_event_timer,
		   evt->ev);
    sleep(1);
    memset(&tv, 0, sizeof(struct timeval));
    gettimeofday(&tv, NULL);
    tv.tv_usec += (lfm->sampling_interval * 1000);
    evt->ev->add_timed_event_ms(evt->ev, 500, lfm->monitor_thread,
			     lfm);
    
    return err;
}

char *
lf_get_snapshot_time_safe(lf_metrics_ctx_t *lfm)
{
    pthread_mutex_lock(&lfm->lock);
    return lfm->tbuf;
}


#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/uio.h>
#include <string.h>

// nkn defs
#include "nkn_memalloc.h"

// libxml defs
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>

// lf defs
#include "lf_vm_connector.h"
#include "lf_common_defs.h"
#include "lf_ref_token.h"
#include "lf_stats.h"
#include "lf_err.h"
#include "lf_dbg.h"

/****************************************************************
 * FUNCTIONALITY:
 * Implements the VM connector that does the following
 * a. connects to the list of hypervisors and a list of domains in
 * that hypervision using libvirt
 * b. enumerates the interface and block i/o devices in each domain
 * c. gathers metrics for each domain
 * d. provides a safe copy of the metrics using a token model for
 * a pre-configured number of tokens
 *
 * OBJECT MODEL:
 * the lf_vm_connector_t object encapsulates the following
 * a. an array of lf_vm_conn_info_t objects corresponding to the
 * number of VM applications. this object  maintains the state
 * of every hypervisor & domain connection for every VM application
 * b. lf_vma_metrics_calc_t - one for each app that measures bw, cpu,
 * disk i/o for each VM;
 *
 * INTERFACES:
 ***************************************************************/
/* HELPER FUNCTIONS */

/* HELPER FUNCTIONS for the lf_vm_connector_t object and libvirt
 * error handling
 */
static void lf_vm_connector_lv_err_hdlr(void *ud, virErrorPtr err);
static int32_t lf_vm_connector_connect(const char *const hyperv_name,
				       const char *const vm_name, 
				       lf_vm_conn_info_t *vci);
static int32_t lf_vm_connector_get_metrics(uint64_t sample_duration,
				   const void *vmc);
static int32_t lf_vm_get_snapshot(void *const vmc,
				  void **out, void **ref_id);
static int32_t lf_vm_release_snapshot(void *vmc,
				      void *ref_id);

/* HELPER FUNCTIONS for lf_vma_metrics_calc_t object
 */
static int32_t lf_vma_metrics_init(uint32_t sampling_interval, 
				   uint32_t window_len,
				   const char *name,
				   lf_vma_metrics_calc_t *out);
static uint32_t lf_vma_get_app_count(const void *vmc);
static void lf_vma_metrics_deinit(lf_vma_metrics_calc_t *vmm);
static void lf_vma_metrics_cleanup_dev_listq(struct tt1 *head);
static int32_t lf_vma_metrics_hold(void *app_metrics_ctx);
static int32_t lf_vma_metrics_release(void *app_metrics_ctx);
static int32_t lf_vma_metrics_get_out_size(void *app_metrics_ctx,
					   uint32_t *out_size);
static int32_t lf_vma_metrics_update(void *app_conn,
				     void *app_metrics_ctx);
static int32_t lf_vma_metrics_copy(const void *app_metrics_ctx, 
				   void *const out);
lf_app_metrics_intf_t lf_vma_metrics_cb = {
    .create = NULL,
    .cleanup = NULL,
    .update = lf_vma_metrics_update,
    .get_out_size = lf_vma_metrics_get_out_size,
    .release = lf_vma_metrics_hold,
    .copy = lf_vma_metrics_copy,
    .hold = lf_vma_metrics_release
};

extern lf_app_metrics_intf_t *g_app_intf[LF_MAX_SECTIONS *
					 LF_MAX_APPS];

int32_t
lf_vm_connector_create(\
   const lf_connector_input_params_t *const vmai,
   void **out)
{
    uint32_t j;
    int32_t err = 0, i;
    lf_vm_connector_t *ctx = NULL;
    lf_app_list_t *vma_list, *hyperv_list;
    lf_vm_conn_info_t vci;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    uint8_t *buf = NULL;
    struct iovec *vec = NULL;
    uint32_t out_size = 0;

    assert(vmai);
    if (vmai->sampling_interval < LF_MIN_SAMPLING_RES ||
	vmai->window_len < LF_MIN_WINDOW_LEN ||
	vmai->sampling_interval > LF_MAX_SAMPLING_RES ||
	vmai->window_len > LF_MAX_WINDOW_LEN ||
	vmai->num_tokens > LF_MAX_CLIENT_TOKENS) {
	err = -E_LF_INVAL;
	goto error;
    }

    ctx = (lf_vm_connector_t *)
	nkn_calloc_type(1, sizeof(lf_vm_connector_t),
			100);
    if (!ctx) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    
    if (!vmai->connector_specific_params) {
	err = -E_LF_INVAL;
	goto error;
    }
    vma_list = (lf_app_list_t*)vmai->app_list;
    hyperv_list = (lf_app_list_t *)vmai->connector_specific_params; 
    ctx->get_app_count = lf_vma_get_app_count;
    ctx->update_all_app_metrics = lf_vm_connector_get_metrics;
    ctx->get_snapshot = lf_vm_get_snapshot;
    ctx->release_snapshot = lf_vm_release_snapshot;
    ctx->num_tokens = vmai->num_tokens;
    err = lf_ref_token_init(&ctx->rt, vmai->num_tokens);
    if (err) 
	goto error;

    /**
     * setup the vm connector
     * 1. initialize members
     * 2. setup libvirt connections
     * 3. setup callback handlers
     */
    ctx->num_vma = vmai->app_list->num_apps;
    virSetErrorFunc(NULL, lf_vm_connector_lv_err_hdlr);

    for (i = 0; i < (int32_t)ctx->num_vma; i++) {
	TAILQ_INIT(&ctx->if_list_head[i]);
	TAILQ_INIT(&ctx->blk_dev_list_head[i]);

	err = lf_vma_metrics_init(vmai->sampling_interval,
				  vmai->window_len,
				  vma_list->name[i],
				  &ctx->vmm[i]);
	if (err) 
	    goto error;
	
	vci.vcon = &ctx->vcon[i];
	vci.vdom = &ctx->vdom[i];
	vci.if_list_head = &ctx->if_list_head[i];
	vci.blk_dev_list_head = &ctx->blk_dev_list_head[i];
	err = lf_vm_connector_connect(hyperv_list->name[i],
				      vma_list->name[i],
				      &vci);
	if (err) {
	    LF_LOG(LOG_ERR, LFD_VM_CONN, "unable to connect to the"
		   " VM application %s", vma_list->name[i]);
	    goto error;
	}
	app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
					     LF_VM_SECTION, i);
	app_metrics_intf->get_out_size(&ctx->vmm[i],
				       &out_size);
	assert(out_size);
	buf = (uint8_t*)nkn_malloc_type(ctx->num_tokens * out_size, 100);
	if (!buf) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	vec = (struct iovec *)
	    nkn_calloc_type(ctx->num_tokens, sizeof(struct iovec), 100);
	if (!vec) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	for (j = 0; j < ctx->num_tokens; j++) {
	    vec[j].iov_base = buf + (j * out_size);
	    vec[j].iov_len = out_size;
	}
	ctx->out[i] = vec;
    }
    *out = ctx;
    return err;

 error:
    if (ctx) lf_vm_connector_cleanup(ctx);
    return err;
    
}

int32_t 
lf_vm_connector_cleanup(lf_vm_connector_t *ctx)
{
    uint32_t i;
    int32_t err = 0;

    if (ctx) {
	for (i = 0; i < ctx->num_vma; i++) {
	    if (ctx->out[i]) {
		if (ctx->out[i][0].iov_base) {
		    free(ctx->out[i][0].iov_base);
		}
	    }
	    if (&ctx->if_list_head[i]) {
		lf_vma_metrics_cleanup_dev_listq(
		 (struct tt1*)(&ctx->if_list_head[i]));
	    }
	    if (&ctx->blk_dev_list_head[i]) {
		lf_vma_metrics_cleanup_dev_listq(
		 (struct tt1*)(&ctx->blk_dev_list_head[i]));
	    }
	    lf_vma_metrics_deinit(&ctx->vmm[i]);
	}
	lf_ref_token_deinit(&ctx->rt);
    }

    return err;
}


static int32_t
lf_vm_connector_get_metrics(uint64_t sample_duration,
			    const void *ctx)
{
    int32_t err = 0, i, free_token = LF_REF_TOKEN_UNAVAIL;
    lf_vm_connector_t *vmc = NULL;
    lf_vma_metrics_calc_t *vmm = NULL;
    lf_vm_conn_info_t ci;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;

    assert(ctx);
    vmc = (lf_vm_connector_t *)ctx;

    for (i = 0; i < (int32_t)vmc->num_vma; i++) {
	ci.vdom = &vmc->vdom[i];
	ci.vdom_info = (virDomainInfo *)&vmc->vdom_info[i];
	ci.if_list_head = &vmc->if_list_head[i];
	ci.blk_dev_list_head = &vmc->blk_dev_list_head[i];
	ci.sample_duration = sample_duration;
	vmm = (lf_vma_metrics_calc_t *)&vmc->vmm[i];
	//	vmm->update_metrics(sample_duration, &ci, vmm);
	app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
						     LF_VM_SECTION, i);
	app_metrics_intf->hold(vmm);
	app_metrics_intf->update(&ci, vmm);
	lf_ref_token_hold(&vmc->rt);
	free_token = lf_ref_token_get_free(&vmc->rt);
	if (free_token < 0) {
	    lf_ref_token_release(&vmc->rt);
	    app_metrics_intf->release(vmm);
	    err = -E_LF_NO_FREE_TOKEN;
	    goto error;
	}
	lf_ref_token_release(&vmc->rt);
	app_metrics_intf->copy(vmm, vmc->out[i][free_token].iov_base);
	app_metrics_intf->release(vmm);
    }

 error:
    return err;

}

static int32_t
lf_vm_get_snapshot(void *const ctx,
		   void **out, void **ref_id)
{
    int32_t err = 0, free_token = LF_REF_TOKEN_UNAVAIL;
    uint32_t i;
    uint64_t *token_no = NULL;
    lf_vm_connector_t *vmc = NULL;

    assert(ctx);
    assert(out);
    assert(ref_id);

    vmc = (lf_vm_connector_t *)ctx;
    token_no = (uint64_t *)
	nkn_calloc_type(1, sizeof(uint64_t), 100);
    if (!token_no) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    lf_ref_token_hold(&vmc->rt);
    free_token = lf_ref_token_get_free(&vmc->rt);
    if (free_token < 0) {
	lf_ref_token_release(&vmc->rt);
	err = -E_LF_NO_FREE_TOKEN;
	goto error;
    }
    for (i = 0; i < vmc->num_vma; i++) {
	lf_ref_token_add_ref(&vmc->rt, free_token);
	*out = vmc->out[i][free_token].iov_base;
	*token_no = free_token;
	*ref_id = token_no;
    }
    lf_ref_token_release(&vmc->rt);

 error:
    return err;

}

static int32_t
lf_vm_release_snapshot(void *ctx,
		       void *ref_id)
{
    uint64_t *free_token = NULL;
    lf_vm_connector_t *vmc = NULL;

    assert(ref_id);
    assert(ctx);
    
    vmc = (lf_vm_connector_t *)ctx;
    lf_ref_token_hold(&vmc->rt);
    free_token = (uint64_t*)ref_id;
    lf_ref_token_unref(&vmc->rt, *free_token);
    free(free_token);
    lf_ref_token_release(&vmc->rt);
    
    return 0;
}

static int32_t
lf_vm_connector_connect(const char *const hyperv_name,
			const char *const vm_name, 
			lf_vm_conn_info_t *vci)
{
    int32_t err = 0, i;
    virConnectPtr *vcon = vci->vcon;
    virDomainPtr *vdom = vci->vdom;
    char *xml = NULL;
    xmlDocPtr xml_doc = NULL;
    xmlXPathContextPtr xpath_ctx = NULL;
    xmlXPathObjectPtr xpath_obj = NULL;

    if (!(vcon) || !(vdom)) {
	err = -E_LF_INVAL;
	goto done;
    }

    /* Connect to HyperV */
    *vcon = virConnectOpen(hyperv_name);
    if (*vcon == NULL) {
	err = -E_LF_VM_HYPERV_CONN;
	goto done;
    }

    /* Connect to the guest OS (domain in libvirt parlance */
    *vdom = virDomainLookupByName(*vcon, vm_name);
    if (*vdom == NULL) {
	err = -E_LF_VM_DOMAIN_CONN;
	goto done;
    }
    
    /* ennumerate the interfaces and blk devices that need to
     * monitored. libvirt gives the device info in XML!!; the
     * following block parses the XML and enumerates network
     * interfaces and block i/o devices
     */
    xml = virDomainGetXMLDesc(*vdom, 0);
    if (!xml) {
	err = -E_LF_VM_DEV_ENUM_FAIL;
	goto done;
    }
    xml_doc = xmlReadDoc ((xmlChar *) xml, NULL, NULL,
			  XML_PARSE_NONET);
    if (!xml_doc) {
	err = -E_LF_VM_DEV_ENUM_FAIL;
	goto done;
    }

    xpath_ctx = xmlXPathNewContext (xml_doc);

    /* XPATH the blk devices */
    xpath_obj = xmlXPathEval
	((xmlChar *) "/domain/devices/disk/target[@dev]",
	 xpath_ctx);
    if (xpath_obj == NULL || xpath_obj->type != XPATH_NODESET ||
	xpath_obj->nodesetval == NULL) {
	goto done;
    }

    for (i = 0; i < xpath_obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node;
	char *dev_name;
	str_list_elm_t *elm = NULL;

	dev_name = NULL;
	node = xpath_obj->nodesetval->nodeTab[i];
	if (!node) continue;
	
	dev_name = (char *) xmlGetProp (node, (xmlChar *) "dev");
	if (dev_name) {
	    elm = (str_list_elm_t *)
		nkn_calloc_type(1, sizeof(str_list_elm_t), 100);
	    if (!elm) {
		err = -E_LF_NO_MEM;
		goto done;
	    }
	    elm->name = strdup(dev_name);
	    TAILQ_INSERT_TAIL(vci->blk_dev_list_head, 
			      elm,
			      entries);
	}
	free(dev_name);
	dev_name = NULL;
	
    }
    if (xpath_obj) xmlXPathFreeObject (xpath_obj);
    xpath_obj = NULL;

    /* XPATH the network devices */
    xpath_obj = xmlXPathEval
	((xmlChar *) "/domain/devices/interface/target[@dev]",
	 xpath_ctx);
    if (xpath_obj == NULL || xpath_obj->type != XPATH_NODESET ||
	xpath_obj->nodesetval == NULL) {
	goto done;
    }

    for (i = 0; i < xpath_obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node;
	char *dev_name;
	str_list_elm_t *elm = NULL;

	dev_name = NULL;
	node = xpath_obj->nodesetval->nodeTab[i];
	if (!node) continue;
	
	dev_name = (char *) xmlGetProp (node, (xmlChar *) "dev");
	if (dev_name) {
	    elm = (str_list_elm_t *)
		nkn_calloc_type(1, sizeof(str_list_elm_t), 100);
	    if (!elm) {
		err = -E_LF_NO_MEM;
		goto done;
	    }
	    elm->name = strdup(dev_name);
	    TAILQ_INSERT_TAIL(vci->if_list_head, 
			      elm,
			      entries);
	}
	free(dev_name);
	dev_name = NULL;
    }
    if (xpath_obj) xmlXPathFreeObject (xpath_obj);
    xpath_obj = NULL;

 done:
    if (xpath_obj) xmlXPathFreeObject (xpath_obj);
    if (xpath_ctx) xmlXPathFreeContext (xpath_ctx);
    if (xml_doc) xmlFreeDoc (xml_doc);
    if (xml) free(xml);
    if (err) {
	/* In case of error need to free any nodes in the if/blk
	 * device list
	 * Note: both struct tt1 and struct tt2 can be safely typecast
	 * to each other
	 */
	lf_vma_metrics_cleanup_dev_listq(
			(struct tt1*)vci->if_list_head);
	lf_vma_metrics_cleanup_dev_listq(
			(struct tt1*)vci->blk_dev_list_head);
	
    }
    return err;
    
}

static void
lf_vm_connector_lv_err_hdlr(void *ud, virErrorPtr err)
{

    LF_LOG(LOG_NOTICE, LF_VM_CONN, "Error code: %d\n", err->code); 
    LF_LOG(LOG_NOTICE, LF_VM_CONN, "Domain name: %d\n", err->domain); 
    LF_LOG(LOG_WARNING, LF_VM_CONN, "Message: %s\n", err->message);
}

static uint32_t
lf_vma_get_app_count(const void *ctx)
{
    lf_vm_connector_t *vmc = NULL;
    
    assert(ctx);
    vmc = (lf_vm_connector_t *)ctx;

    return vmc->num_vma;
    
}

static int32_t
lf_vma_metrics_init(uint32_t sampling_interval, 
		    uint32_t window_len,
		    const char *name,
		    lf_vma_metrics_calc_t *out)
{
    int32_t err = 0;
    uint32_t num_slots = 0;
    lf_vma_metrics_calc_t *vmm;

    assert(out);
    vmm = out;
    num_slots = (window_len / sampling_interval) + 1;

    /* Initialize the vm metrics structure */
    vmm->prev_bw_use = 0xffffffffffffffff;
    vmm->prev_blk_dev_bw_use = 0xffffffffffffffff;
    vmm->size = sizeof(lf_vma_metrics_t);
    pthread_mutex_init(&vmm->lock, NULL);
    err = ma_create(num_slots, 0, &vmm->cpu_use_ma);
    if (err) 
	goto error;
    err = ma_create(num_slots, 1, &vmm->if_bw_use_ma);
    if (err)
	goto error;
    memset(vmm->name, 0, 64);
    memcpy(vmm->name, name, 64);

    return err;
 error:
    lf_vma_metrics_deinit(vmm);
    return err;
}

static void
lf_vma_metrics_deinit(lf_vma_metrics_calc_t *vmm)
{
    if (vmm) {
	if (vmm->cpu_use_ma) {
	    ma_cleanup(vmm->cpu_use_ma);
	}
	if (vmm->if_bw_use_ma) {
	    ma_cleanup(vmm->if_bw_use_ma);
	}
    }
}

static int32_t 
lf_vma_metrics_hold(void *app_metrics_ctx)
{
    lf_vma_metrics_calc_t *vmm = NULL;

    assert(app_metrics_ctx);
    vmm = (lf_vma_metrics_calc_t *)app_metrics_ctx;

    pthread_mutex_lock(&vmm->lock);
    
    return 0;
}

static int32_t
lf_vma_metrics_release(void *app_metrics_ctx)
{
    lf_vma_metrics_calc_t *vmm = NULL;

    assert(app_metrics_ctx);
    vmm = (lf_vma_metrics_calc_t *)app_metrics_ctx;

    pthread_mutex_unlock(&vmm->lock);
    
    return 0;
}

static int32_t
lf_vma_metrics_get_out_size(void *app_metrics_ctx,
				uint32_t *out_size)
{
    lf_vma_metrics_calc_t *vmm = NULL;

    assert(app_metrics_ctx);
    assert(out_size);

    vmm = (lf_vma_metrics_calc_t *)app_metrics_ctx;
    *out_size = vmm->size;

    return 0;
}

static int32_t 
lf_vma_metrics_copy(const void *app_metrics_ctx, 
		    void *const out)
{
    lf_vma_metrics_calc_t *vmm = NULL;

    assert(app_metrics_ctx);
    vmm = (lf_vma_metrics_calc_t *)app_metrics_ctx;
    memcpy(out, vmm, vmm->size);

    return vmm->size;    
}

static int32_t
lf_vma_metrics_update(void *app_conn,
		      void *app_metrics_ctx)
{
    int32_t status = 0, err = 0;
    lf_vm_conn_info_t *ci = NULL;
    lf_vma_metrics_calc_t *vmm = NULL;
    virDomainPtr vdom;// = *ci->vdom;
    virDomainInfo *info;// = (virDomainInfo *)ci->vdom_info;
    sample_t s;
    str_list_elm_t *dev_list = NULL;
    int64_t tot_bytes = 0;
    uint64_t sample_duration;
    double bw = 0.0;

    assert(app_conn);
    assert(app_metrics_ctx);
    ci = (lf_vm_conn_info_t*)app_conn;
    vmm = (lf_vma_metrics_calc_t *)app_metrics_ctx;
    vdom = *ci->vdom;
    info = (virDomainInfo *)ci->vdom_info;
    sample_duration = ci->sample_duration;
    
    /* get CPU stats */
    status = virDomainGetInfo(vdom, info);
	if (status < 0)
		goto error;
    s.f32 = (info->cpuTime - vmm->prev_cpu_time) / 
	(1000 * 1000 * sample_duration * info->nrVirtCpu);
    vmm->prev_cpu_time = info->cpuTime;
    ma_move_window(vmm->cpu_use_ma, s);
    LF_LOG(LOG_NOTICE, LF_VM_CONN, "cpu use: %f moving average: %f\n", s.f32,
	   (double)vmm->cpu_use_ma->prev_ma);   
    vmm->cpu_use = (double)vmm->cpu_use_ma->prev_ma; 
    vmm->cpu_max = info->nrVirtCpu;

    /* get if stats */
    TAILQ_FOREACH(dev_list, ci->if_list_head, entries) {
	struct _virDomainInterfaceStats if_stats;
	err = virDomainInterfaceStats(vdom, dev_list->name,
				      &if_stats, sizeof(if_stats));
	if (err) {
	    goto error;
	}
	if (if_stats.tx_bytes != -1 && if_stats.rx_bytes != -1) {
	    tot_bytes += if_stats.tx_bytes + if_stats.rx_bytes;
	}
	
    }
    if (vmm->prev_bw_use == 0xffffffffffffffff) {
	vmm->prev_bw_use = tot_bytes;
    }
    bw = (tot_bytes - vmm->prev_bw_use) / 
	(((double)(sample_duration)/1000));
    s.i64 = ceil(bw);
    ma_move_window(vmm->if_bw_use_ma, s);
    LF_LOG(LOG_NOTICE, LF_VM_CONN, "bw use: %lu moving average: %lu\n", s.i64,
	   (uint64_t)(vmm->if_bw_use_ma->prev_ma));
    vmm->if_bw_use = (uint64_t)(vmm->if_bw_use_ma->prev_ma);
    if (vmm->if_bw_use_ma->prev_ma < 0) {
	int uu = 0;
    }
    vmm->prev_bw_use = tot_bytes;

    /* get blk dev stats */
    tot_bytes = 0;
    bw = 0;
    TAILQ_FOREACH(dev_list, ci->blk_dev_list_head, entries) {
	struct _virDomainBlockStats blk_dev_stats;
        err = virDomainBlockStats(vdom, dev_list->name,
                                      &blk_dev_stats, sizeof(blk_dev_stats));
        if (err) {
            goto error;
        }
        if (blk_dev_stats.rd_bytes != -1 && blk_dev_stats.wr_bytes != -1) {
            tot_bytes += blk_dev_stats.rd_bytes + blk_dev_stats.wr_bytes;
        }

    }
    if (vmm->prev_blk_dev_bw_use == 0xffffffffffffffff) {
	vmm->prev_bw_use = tot_bytes;
    }
    bw = (tot_bytes - vmm->prev_blk_dev_bw_use) / 
	(((double)(sample_duration)/1000));
    s.i64 = ceil(bw);
    vmm->prev_blk_dev_bw_use = tot_bytes;
    LF_LOG(LOG_NOTICE, LF_VM_CONN, "blk dev bw: %lu\n", s.i64);
    
 error:
    return err;
}


static void
lf_vma_metrics_cleanup_dev_listq(struct tt1 *head)
{
    str_list_elm_t *dev_list = NULL;

    assert(head);

    while( (dev_list = TAILQ_FIRST(head)) ) {
	TAILQ_REMOVE(head, dev_list, entries);
	if (dev_list) {
	    if (dev_list->name) free(dev_list->name);
	    free(dev_list);
	}
    }

}

/**
 * @file   de_rr.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue May  1 23:18:21 2012
 * 
 * @brief  implements round robin decision
 * 
 * 
 */
#include "de_intf.h"
#include "stored/ds_tables.h"

/* static  */
static int32_t de_rr_init(void **state);

static int32_t de_rr_decide(void *state, crst_search_out_t *di,
	     uint8_t **result, uint32_t *result_len);

static int32_t de_rr_cleanup(void *state);

/* global */
de_intf_t gl_obj_de_rr = {
    .init = de_rr_init,
    .decide = de_rr_decide,
    .cleanup = de_rr_cleanup
};

typedef struct tag_de_rr {
    uint32_t last_idx;
    uint32_t prev_len;
}de_rr_t;
    
static int32_t 
de_rr_init(void **state)
{
    
    de_rr_t *ctx = NULL;
    int32_t err = 0;
    
    if (!state) {
	err = -EINVAL;
	goto error;
    }

    ctx = (de_rr_t *)nkn_calloc_type(1, sizeof(de_rr_t), 103);
    if (!ctx) {
	err = -ENOMEM;
	goto error;
    }
    
    *state = ctx;
    return err;

 error:
    if (ctx) de_rr_cleanup(ctx);
    return err;
}

static int32_t
de_rr_decide(void *state, crst_search_out_t *di,
	     uint8_t **result, uint32_t *result_len)
{
    int32_t err = 0;
    de_rr_t *rr = NULL;
    uint32_t next_idx = -1, i, idx;
    de_cache_attr_t *arr = NULL;
    rrecord_msg_fmt_builder_t *rrb = NULL;

    rr = (de_rr_t *)state;
    arr = di->ce_attr;
    next_idx = (rr->last_idx);
    rr->last_idx++;
    if (rr->last_idx >= di->ce_count) 
	rr->last_idx = 0;

    /* if the next cache index is down/ unreachable then
     * find the next available best cache
     */
    if (arr[next_idx].stats.status == LF_UNREACHABLE ||
	arr[next_idx].stats.status == CACHE_DOWN ||
	arr[next_idx].stats.cpu_load > arr[next_idx].load_watermark) {
	err = -1;
	for (i = 0, idx = next_idx; 
	     i < di->ce_count; 
	     i++) {
	    idx = (i+idx) % di->ce_count;
	    if (arr[idx].stats.status == CACHE_UP &&
		arr[idx].stats.cpu_load < arr[idx].load_watermark) {
		next_idx = idx;
		err = 0;
		break;
	    }
	}
    }

    if (!err) {
	de_cache_attr_t *ca =  &arr[next_idx];
	uint32_t rlen = 0, j = 0, num_rr = 0;

	if (di->in_addr_type == ce_addr_type_max) {
	    uint32_t tot_len = 0;
	    num_rr = ca->num_addr;
	    for (j = 0; j < ca->num_addr; j++) {
		tot_len += rrecord_msg_fmt_builder_compute_record_size(
						       *ca->addr_len[j]);
	    }
	    rlen = tot_len;
		
	    err = rrecord_msg_fmt_builder_create(rlen, &rrb);
	    if (err) {
		goto error;
	    }

	    err = rrb->add_hdr(rrb, num_rr, di->ttl);
	    if (err) {
		goto error;
	    }
	    
	    for (j = 0; j < ca->num_addr; j++) {
		cache_entity_addr_type_t type = ca->addr_type[j];
		err = rrb->add_record(rrb, type, (uint8_t *)ca->addr[j], 
				      *ca->addr_len[j]);
		
		if (err) {
		    goto error;
		}
	    }
	    err = rrb->get_buf(rrb, result, result_len);
	    if (err) {
		goto error;
	    }
	    
	} else {
	    num_rr = 1;
	    for (j = 0; j < ca->num_addr; j++) {
		if (ca->addr_type[j] ==  di->in_addr_type) {
		    break;
		}
	    }
	    if (j == ca->num_addr) {
		err = -1;
		goto error;
	    }
	    
	    cache_entity_addr_type_t type = ca->addr_type[j];

	    rlen = rrecord_msg_fmt_builder_compute_record_size(
							       *ca->addr_len[j]);

	    err = rrecord_msg_fmt_builder_create(rlen, &rrb);
	    if (err) {
		goto error;
	    }
	    err = rrb->add_hdr(rrb, num_rr, di->ttl);
	    if (err) {
		goto error;
	    }
	    err = rrb->add_record(rrb, type, (uint8_t *)ca->addr[j], 
				  *ca->addr_len[j]);
			      
	    if (err) {
		goto error;
	    }
	
	    err = rrb->get_buf(rrb, result, result_len);
	    if (err) {
		goto error;
	    }
	}
    }

 error:
    if (rrb) rrb->delete(rrb);
    return err;

}

static int32_t 
de_rr_cleanup(void *state)
{
    de_rr_t *rr = (de_rr_t *)state;
    
    if (rr) {
	free(rr);
    }
    
    return 0;
}

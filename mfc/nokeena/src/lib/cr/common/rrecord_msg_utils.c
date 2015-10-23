/**
 * @file   rrecord_msg_utils.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed May  2 19:52:55 2012
 * 
 * @brief  
 * 
 * 
 */
#include "rrecord_msg_utils.h"

static int32_t rrecord_msg_fmt_builder_get_buf(
			      rrecord_msg_fmt_builder_t *rrb, 
			      uint8_t **buf, uint32_t *buf_len);

static int32_t rrecord_msg_fmt_builder_add_record(
			 rrecord_msg_fmt_builder_t *rrb,
			 cache_entity_addr_type_t rdata_type, 
			 const uint8_t *rdata, 
			 uint32_t rdata_len);

static int32_t rrecord_msg_fmt_builder_add_hdr(
		      rrecord_msg_fmt_builder_t *rrb,
		      uint32_t num_records, uint32_t ttl);

static int32_t rrecord_msg_fmt_builder_delete(
		     rrecord_msg_fmt_builder_t *rrb);

int32_t 
rrecord_msg_fmt_builder_create(uint32_t tot_record_len,
			       rrecord_msg_fmt_builder_t **out)
{
    int32_t err = 0;
    uint32_t alloc_size = 0;
    rrecord_msg_fmt_builder_t *ctx = NULL;
    
    if (!out) {
	err = -EINVAL;
	goto error;
    }
    alloc_size =  sizeof(rrecord_msg_hdr_t) + tot_record_len;

    ctx = (rrecord_msg_fmt_builder_t *)
	nkn_calloc_type(1, sizeof(rrecord_msg_fmt_builder_t), 104);
    if (!ctx) {
	err = -ENOMEM;
	goto error;
    }
    ctx->add_hdr = rrecord_msg_fmt_builder_add_hdr;
    ctx->add_record = rrecord_msg_fmt_builder_add_record;
    ctx->delete = rrecord_msg_fmt_builder_delete;
    ctx->get_buf = rrecord_msg_fmt_builder_get_buf;

    ctx->buf = (uint8_t *) nkn_malloc_type(alloc_size, 105);
    if (!ctx->buf) {
	ctx->state = -1;
	err = -ENOMEM;
	goto error;
    }
    ctx->wpos = 0;
    ctx->buf_size = alloc_size;
    ctx->state = 0;
    
    *out = ctx;
    return err;
 
error:
    if (ctx) { 
	if (ctx->delete)  {
	    ctx->delete(ctx);
	} else {
	    free(ctx);
	}
    }
    return err;
}

static int32_t
rrecord_msg_fmt_builder_delete(rrecord_msg_fmt_builder_t *rrb)
{
    if (rrb) {
	/* buf will be filled by the caller if
	 * state is not set to error
	 */
	if (rrb->state < 0) {
	    if (rrb->buf) free(rrb->buf);
	}
	free(rrb);
    }
    return 0;
}

static int32_t
rrecord_msg_fmt_builder_add_hdr(rrecord_msg_fmt_builder_t *rrb,
				uint32_t num_records, uint32_t ttl)
{
    uint8_t *ptr = NULL;
    uint32_t wpos = 0;
    
    if (!rrb) {
	rrb->state = -1;
	return -1;
    }

    if (rrb->state != 0) {
	rrb->state = -1;
	return -2;
    }

    wpos = rrb->wpos;
    ptr = rrb->buf;

    rrb->num_records = num_records;
    memcpy(ptr + wpos, &num_records, 4);
    wpos += 4;
    memcpy(ptr + wpos, &ttl, 4);
    wpos +=4;
    rrb->wpos += wpos;
    rrb->state = 1;

    return 0;
}

static int32_t 
rrecord_msg_fmt_builder_add_record(rrecord_msg_fmt_builder_t *rrb,
	   cache_entity_addr_type_t rdata_type, const uint8_t *rdata, 
	   uint32_t rdata_len)
{
    uint8_t *ptr = NULL;
    uint32_t wpos = 0, rr_len;


    if (!rrb) {
	rrb->state = -1;
	return -1;
    }

    if (rrb->state != 1) {
	rrb->state = -1;
	return -2;
    }

    wpos = rrb->wpos;
    ptr = rrb->buf;
    /* bounds check */
    if (wpos + rdata_len + 
	sizeof(rrecord_msg_data_preamble_t) >
	rrb->buf_size) {
	rrb->state = -1;
	return -3;
    }

    memcpy(ptr + wpos, &rdata_type, 4);
    wpos += 4;
    rr_len = rdata_len + 1;
    memcpy(ptr + wpos, &rr_len, 4);
    wpos += 4;
    memcpy(ptr + wpos, rdata, rdata_len);
    ptr[wpos+rdata_len] = 0;
    wpos += rdata_len + 1;
    rrb->wpos = wpos;

    rrb->num_records_written++;
    if (rrb->num_records_written == rrb->num_records) {
	rrb->state = 2;
    }

    return 0;
}

static int32_t
rrecord_msg_fmt_builder_get_buf(rrecord_msg_fmt_builder_t *rrb, 
				uint8_t **buf, uint32_t *buf_len)
{

    if (rrb->state != 2) {
	/* dont set the error state in the builder
	 * leave scope for the caller to heal
	 */    
	return -3;
    }
    
    if (!buf) {
	return -2;
    }

    *buf = rrb->buf;
    *buf_len = rrb->wpos;

    return 0;
}

int32_t
rrecord_msg_fmt_reader_fill_hdr(const uint8_t *buf, 
				uint32_t buf_size, 
				const rrecord_msg_hdr_t **hdr)
{
    *hdr = (rrecord_msg_hdr_t*)buf;
    return 0;
}

int32_t
rrecord_msg_fmt_reader_get_record_at(const uint8_t *buf, 
		     uint32_t buf_size, uint32_t idx, 
		     const rrecord_msg_data_preamble_t **rr_preamble, 
		     const uint8_t **rdata)
{
    rrecord_msg_hdr_t *hdr;
    rrecord_msg_data_preamble_t *rrp;
    uint32_t i;
    uint8_t *ptr = NULL;

    ptr = (uint8_t *)buf;
    
    hdr = (rrecord_msg_hdr_t*)buf;
    ptr += sizeof(rrecord_msg_hdr_t);
    for (i = 0; i < hdr->num_records; i++) {
	if (idx == i) {
	    *rr_preamble = (rrecord_msg_data_preamble_t *)ptr;
	    *rdata = ptr + sizeof(rrecord_msg_data_preamble_t);
	    break;
	}
	rrp = (rrecord_msg_data_preamble_t *)ptr;
	ptr += (sizeof(rrecord_msg_data_preamble_t) + rrp->len);
    }

    return 0;
}

/*
 *
 * Filename:  nkn_vpe_f4v_frag_access_api.c
 * Date:      2010/01/01
 * Module:    access a f4v compliant fragment from a segment
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "nkn_memalloc.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_ism_read_api.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_f4v_frag_access_api.h"
#include "nkn_vpe_mp4_parser.h"

static size_t f4v_read_64(uint8_t *p_data);
static size_t f4v_read_32(uint8_t *p_data);
static size_t f4v_read_16(uint8_t *p_data);

#define ABST_ID 0x74736261
/*********************************************************************
 * DATA ORGANIZATION
 * An Adobe Zeri package has the following data heirarchy 
 * profile/segment/fragment
 * using the API's implemented in this file, one can create a profile
 * level context and query the offset and length of any frag using the
 * [segment, fragment] pair
 *********************************************************************/

/**
 * initialize the f4v fragment access context 
 * @return returns a valid context pointer, NULL on error
 */
f4v_frag_access_ctx_t*
init_f4v_frag_access_ctx(void)
{ 
    f4v_frag_access_ctx_t *ctx;

    ctx = (f4v_frag_access_ctx_t*)
	nkn_calloc_type(1, sizeof(f4v_frag_access_ctx_t),
			mod_vpe_f4v_frag_access_ctx_t);
    
    return ctx;    
}

/**
 * attach a new segment index file (f4x) in which we need to seek
 * the fragment offsets
 * @param ctx - the fragment access context
 * @param f4x_data - buffer pointing to the f4x data
 * @param f4x_size - size of the f4x buffer
 * @param seg_num - the id of the segment in which we need to
 * search for the fragment
 * @return returns 0 on success and negative integer on error
 */
int32_t
f4v_next_segment(f4v_frag_access_ctx_t *ctx, 
		 const uint8_t * const f4x_data, 
		 size_t f4x_size,
		 int32_t seg_num) 
{
    if (!ctx) {
	return -E_VPE_PARSER_INVALID_CTX;
    }

    ctx->f4x_data = (uint8_t*)f4x_data;
    ctx->f4x_size = f4x_size;
    ctx->curr_seg = seg_num;
    ctx->curr_frag = 0;
    
    ctx->afra = f4v_read_afra(ctx->f4x_data,
			      ctx->f4x_size);
    
    if (ctx->afra->flags2 & 0x20) {
	ctx->num_frags_in_seg = ctx->afra->global_entry_count;
	ctx->frag_access_tbl_offset = 25;
	ctx->frag_access_tbl_entry_size = 8;
	if (ctx->afra->flags2 & 0x80) {
	    ctx->frag_access_tbl_entry_size += 8;
	    ctx->afra_offset_field_pos = 16;
	    ctx->seg_num_field_size = 4;
	    ctx->frag_num_field_size = 4;
	} else {
	    ctx->frag_access_tbl_entry_size += 4;
	    ctx->afra_offset_field_pos = 12;
	    ctx->seg_num_field_size = 2;
	    ctx->frag_num_field_size = 2;
	}

	if (ctx->afra->flags2 & 0x40) {
	    ctx->frag_access_tbl_entry_size += 16;
	    ctx->afra_offset_field_size = 8;
	    ctx->afra_offset_read = f4v_read_64;
	} else {
	    ctx->frag_access_tbl_entry_size += 8;
	    ctx->afra_offset_field_size = 4;
	    ctx->afra_offset_read = f4v_read_32;
	}
    }

    return 0;
}

/**
 * attach a new segment index file (f4x) in which we need to seek
 * the fragment offsets
 * @param ctx - the fragment access context
 * @param f4x_data - buffer pointing to the f4x data
 * @param f4x_size - size of the f4x buffer
 * @param seg_num - the id of the segment in which we need to
 * search for the fragment
 * @return returns 0 on success and negative integer on error
 */
int32_t
f4v_create_profile_index(f4v_frag_access_ctx_t *ctx, 
			 const uint8_t * const f4x_data, 
			 size_t f4x_size,
			 int32_t seg_num) 
{
    uint8_t *p, *p1;
    size_t curr_seg_num, curr_frag_num, prev_seg_num, prev_frag_num;
    int32_t i, j, k;

    i = j = 0;
    k = -1;
    prev_seg_num = curr_seg_num = 0x0;

    if (!ctx) {
	return -E_VPE_PARSER_INVALID_CTX;
    }

    ctx->f4x_data = (uint8_t*)f4x_data;
    ctx->f4x_size = f4x_size;
    ctx->curr_seg = seg_num;
    ctx->curr_frag = 0;
    
    ctx->afra = f4v_read_afra(ctx->f4x_data,
			      ctx->f4x_size);
    
    if (ctx->afra->flags2 & 0x20) {
	ctx->num_frags_in_seg = ctx->afra->global_entry_count;
	ctx->frag_access_tbl_offset = 25;
	ctx->frag_access_tbl_entry_size = 8;
	if (ctx->afra->flags2 & 0x80) {
	    ctx->frag_access_tbl_entry_size += 8;
	    ctx->afra_offset_field_pos = 16;
	    ctx->seg_num_field_size = 4;
	    ctx->frag_num_field_size = 4;
	} else {
	    ctx->frag_access_tbl_entry_size += 4;
	    ctx->afra_offset_field_pos = 12;
	    ctx->seg_num_field_size = 2;
	    ctx->frag_num_field_size = 2;
	}

	if (ctx->afra->flags2 & 0x40) {
	    ctx->frag_access_tbl_entry_size += 16;
	    ctx->afra_offset_field_size = 8;
	    ctx->afra_offset_read = f4v_read_64;
	    ctx->afra_segment_num_read = f4v_read_32;
	    ctx->afra_fragment_num_read = f4v_read_32;
	} else {
	    ctx->frag_access_tbl_entry_size += 8;
	    ctx->afra_offset_field_size = 4;
	    ctx->afra_offset_read = f4v_read_32;
	    ctx->afra_segment_num_read = f4v_read_16;
	    ctx->afra_fragment_num_read = f4v_read_16;
	}
    }

    ctx->frag = (vpe_olt_t *)
	nkn_calloc_type(1, ctx->afra->global_entry_count * \
			sizeof(vpe_olt_t),
			mod_vpe_ol_t); 
    ctx->seg = (f4v_seg_t*)
	nkn_calloc_type(1, ctx->afra->global_entry_count * \
			sizeof(f4v_seg_t),
			mod_vpe_f4v_seg_t);
    for (i = 0;i < (int32_t) ctx->afra->global_entry_count; i++) {
	p = (uint8_t*)ctx->f4x_data;
	p = p + ctx->frag_access_tbl_offset + \
	    ( (i) * ctx->frag_access_tbl_entry_size);
	p1 = p = p + ctx->afra_offset_field_pos;

	p -= ctx->frag_num_field_size;
	curr_frag_num = ctx->afra_fragment_num_read(p);
	p -= ctx->frag_num_field_size;
	curr_seg_num = ctx->afra_segment_num_read(p);

	if (curr_seg_num != prev_seg_num) {
	    k++;
	    ctx->frag[j].offset = ctx->afra_offset_read(p1);
	    ctx->seg[k].frag = &ctx->frag[j];
	    ctx->seg[k].start_frag = j+1;
	    ctx->num_seg++;
	    ctx->seg[k].num_frag++;
	} else {
	    ctx->frag[j].offset = ctx->afra_offset_read(p1);
	    ctx->seg[k].num_frag++;
	}
	j++;

	prev_frag_num = curr_frag_num;
	prev_seg_num = curr_seg_num;
    }

    return 0;
}

/**
 * find the number of fragments in a segment
 * @param ctx - the fragment access context
 * @param seg_num - the segment number for the which the frag count is
 * required 
 * @return returns 0 on success and negative integer on error
 */
int32_t 
f4v_get_num_frags(f4v_frag_access_ctx_t *ctx, int32_t seg_num)
{

    if (ctx) {
	if (ctx->afra) { 
	    return ctx->seg[seg_num-1].num_frag;
	}  else {
	    return -E_VPE_PARSER_INVALID_CTX;
	} 
    } else {
	return -E_VPE_PARSER_INVALID_CTX;
    }

}

/**
 * find the number of segment in the context
 * @param ctx - the fragment access context
 * @return returns 0 on success and negative integer on error
 */
int32_t 
f4v_get_num_segs(f4v_frag_access_ctx_t *ctx)
{

    if (ctx) {
	if (ctx->afra) { 
	    return ctx->num_seg;
	}  else {
	    return -E_VPE_PARSER_INVALID_CTX;
	} 
    } else {
	return -E_VPE_PARSER_INVALID_CTX;
    }

}

/**
 * reads an afra box and populates the afra_t stucture
 * @param data - buffer containing the start of the afra box
 * @param size - size of the buffer
 * @return returns a correctly populated afra_t structure and NULL
 * error
 */
afra_t* 
f4v_read_afra(const uint8_t * const data, size_t size)
{
    uint8_t *p;
    afra_t* afra;
    uint32_t u32;

    afra = (afra_t*)
	nkn_calloc_type(1, sizeof(afra_t), 
			mod_vpe_afra_t);
    if (!afra) {
	return NULL;
    }

    p = (uint8_t*)data;
    
    /* skip box header */
    p += 8;
    
    /* read version */
    afra->version = *p;
    p++;

    /* flag1 is undefine, skip */
    p += 3;
    
    /* read flags2 */
    afra->flags2 = *p;
    p++;

    /* read timescale */
    u32 = *((uint32_t*)(p));
    afra->timescale = nkn_vpe_swap_uint32((u32));
    p += 4;
    
    /* read num entries*/
    u32 = *((uint32_t*)(p));
    afra->entry_count = nkn_vpe_swap_uint32((u32));
    p += 4;

    /* read num entries if global entry flag is enabled*/
    if (afra->flags2 & 0x20) {
	u32 = *((uint32_t*)(p));
	afra->global_entry_count = nkn_vpe_swap_uint32((u32));
	p += 4;
    }
    
    return afra;
}  

/**
 * reads the fragment offset
 * @param ctx - the fragment access context
 * @param seg_num - the segment number which is being parsed
 * @param frag_num - the fragment whose offset needs to be
 * computed
 * @param olt[out] - the [offset, length, timestamp] for a given afra
 * that is requested using the seg,frag pair. if olt->length is set to
 * -1 then the size of the fragment is computed as follows (length =
 * S-offset) where S is the size of the segment in  bytes 
 * @return returns the offset of the fragement, negative number on
 * error
 */ 
size_t    
f4v_get_afra_olt(f4v_frag_access_ctx_t *ctx, uint32_t seg_num,
		 uint32_t glob_frag_num, vpe_olt_t *olt) 
{
    f4v_seg_t *seg;
    uint32_t frag_num;

    seg = &ctx->seg[seg_num-1];

    if ( glob_frag_num < seg->start_frag || glob_frag_num > (seg->start_frag+seg->num_frag-1) ) {
	return -E_VPE_F4V_INVALID_FRAG;
    }

    frag_num = glob_frag_num - seg->start_frag + 1;/*start index for
						    *frag num is '1' */

    if (frag_num == ctx->seg[seg_num-1].num_frag) {
	olt->offset = seg->frag[frag_num - 1].offset;
	olt->length = 0xffffffffffffffff;
	return olt->offset;
    }

    olt->offset = seg->frag[frag_num - 1].offset;
    olt->length = seg->frag[frag_num].offset - \
	seg->frag[frag_num - 1].offset;

    return olt->offset;
}

int32_t
f4v_attach_bootstrap(f4v_frag_access_ctx_t *ctx, 
		     uint8_t *f4m_data,
		     size_t f4m_size)
{
    xml_read_ctx_t *xml;
    int32_t err, found, *type;
    size_t box_size;
    xmlNode *node;
    uint8_t *raw_bootstrap_data, *p;
    gsize decode_size;
    box_t box;

    xml = NULL;
    node = NULL;
    decode_size = 0;
    found = 0;

    xml = init_xml_read_ctx(f4m_data, f4m_size);
    if (!xml) {
	err = -E_VPE_F4V_INVALID_F4M;
	goto error;
    }

    node = xml_read_till_node(xml->root, "bootstrapInfo");
    if (!node) {
	err = -E_VPE_F4V_INVALID_F4M;
	goto error;
    }

    p = (uint8_t *)node->children->content;
    while (*p != 'A') {
	p++;
    }
	
    raw_bootstrap_data = (uint8_t*)\
	g_base64_decode((char*)p, &decode_size);

    p = raw_bootstrap_data;
    do {
	read_next_root_level_box(p, 8, &box, &box_size);
	type = (int32_t*)box.type;
	switch (*type) {
	    case ABST_ID:
		
		break;
	}

    } while (!found);

 error:
    if (xml) xml_cleanup_ctx(xml);

    return err;
}

/**
 * cleanup profile index tables
 */
void
cleanup_f4v_profile_index(f4v_frag_access_ctx_t *ctx)
{
    if (ctx) {
	if (ctx->frag) free(ctx->frag);
	if (ctx->seg) free(ctx->seg);
    }
    
}

/**
 * cleanup fragment access contex
 */
void 
cleanup_f4v_frag_access_ctx(f4v_frag_access_ctx_t *ctx)
{
    if (ctx) {
	free(ctx);
    }
}

static size_t 
f4v_read_64(uint8_t *p_data)
{
    size_t *p, tmp;

    p = (size_t*)p_data;
    
    tmp = nkn_vpe_swap_uint64((*p));
    
    return tmp;
}

static size_t 
f4v_read_32(uint8_t *p_data)
{
    uint32_t *p, tmp;

    p = (uint32_t*)p_data;
    tmp = nkn_vpe_swap_uint32((*p));
    
    return tmp;
}

static size_t 
f4v_read_16(uint8_t *p_data)
{

    uint16_t *p, tmp;

    p = (uint16_t*)p_data;
    tmp = nkn_vpe_swap_uint32((*p));
    
    return tmp;
}
  
#if 0
    uint8_t *p;
    size_t offset, next_offset;

    p = (uint8_t*)ctx->f4x_data;
    p = p + ctx->frag_access_tbl_offset + ( (frag_num -1) *\
					    ctx->frag_access_tbl_entry_size);
    p = p + ctx->afra_offset_field_pos;
    olt->offset = ctx->afra_offset_read(p);

    if (frag_num == ctx->afra->global_entry_count) {
	olt->length = 0xffffffffffffffff;
	return olt->offset;
    }

    p = (uint8_t*)ctx->f4x_data;
    p = p + ctx->frag_access_tbl_offset + ( (frag_num) *\
					    ctx->frag_access_tbl_entry_size);
    p = p + ctx->afra_offset_field_pos;
    next_offset = ctx->afra_offset_read(p);

    olt->length = next_offset - olt->offset;
#endif

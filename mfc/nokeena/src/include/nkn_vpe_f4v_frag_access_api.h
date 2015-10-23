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

#ifndef _F4V_FRAG_ACCESS_
#define _F4V_FRAG_ACCESS_

#include <stdio.h>
#include <inttypes.h>

#include "nkn_memalloc.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_parser_err.h"

#ifdef __cplusplus 
extern "C" {
#endif

    /**
     * \struct afra box definition
     */
    typedef struct tag_afra {
	uint8_t version;
	uint8_t flags1[3];
	uint8_t flags2;
	uint32_t timescale;
	uint32_t entry_count;
	uint32_t global_entry_count;
    } afra_t;

    typedef struct tag_f4v_seg {
	vpe_olt_t *frag;
	uint32_t num_frag;
	uint32_t start_frag;
    }f4v_seg_t;
    
    /**
     * \struct the fragment access context to search a fragment within
     *  a f4x file
     */
    typedef struct tag_f4v_frag_access_ctx {
	int32_t curr_seg;
	int32_t num_frags_in_seg; 
	int32_t curr_frag;
	const uint8_t * f4x_data;
	afra_t *afra;
	size_t f4x_size;
	uint32_t frag_access_tbl_entry_size;
	uint32_t frag_access_tbl_offset;
	uint32_t frag_num_field_size;
	uint32_t seg_num_field_size;
	uint32_t afra_offset_field_size;
	uint32_t afra_offset_field_pos;
	size_t (*afra_offset_read)(uint8_t*);
	size_t (*afra_segment_num_read)(uint8_t *);
	size_t (*afra_fragment_num_read)(uint8_t *);
	vpe_olt_t *frag;
	f4v_seg_t *seg;
	uint32_t num_seg;
    } f4v_frag_access_ctx_t; 

    /**
     * initialize the f4v fragment access context 
     * @return returns a valid context pointer, NULL on error
     */
    f4v_frag_access_ctx_t* init_f4v_frag_access_ctx(void);

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
    int32_t f4v_next_segment(f4v_frag_access_ctx_t *ctx, 
			     const uint8_t * const f4x_data, 
			     size_t f4x_size,
			     int32_t seg_num);

    /**
     * find the number of fragments in a segment
     * @param ctx - the fragment access context
     * @param seg_num - the segment number for the which the frag count is
     * required 
     * @return returns 0 on success and negative integer on error
     */
    int32_t f4v_get_num_frags(f4v_frag_access_ctx_t *ctx, int32_t seg_num);

    /**
     * find the number of segment in the context
     * @param ctx - the fragment access context
     * @return returns 0 on success and negative integer on error
     */
    int32_t f4v_get_num_segs(f4v_frag_access_ctx_t *ctx);

    /**
     * reads an afra box and populates the afra_t stucture
     * @param data - buffer containing the start of the afra box
     * @param size - size of the buffer
     * @return returns a correctly populated afra_t structure and NULL
     * error
     */
    afra_t* f4v_read_afra(const uint8_t * const data, size_t size);

    /**
     * reads the fragment offset
     * @param ctx - the fragment access context
     * @param seg_num - the segment number which is being parsed
     * @param frag_num - the fragment whose offset needs to be
     * computed
     * @param olt - the [offset, length, timestamp] for a given afra. if
     * the length is -1 then the size of the fragment is computed as
     * follows (length = S-offset) where S is the size of the segment in
     * bytes 
     * @return returns the offset of the fragement, negative number on
     * error
     */
    size_t  f4v_get_afra_olt(f4v_frag_access_ctx_t *ctx, 
			     uint32_t seg_num, 
			     uint32_t frag_num, vpe_olt_t *olt);

    int32_t f4v_create_profile_index(f4v_frag_access_ctx_t *ctx, 
				     const uint8_t * const f4x_data, 
				     size_t f4x_size,
				     int32_t seg_num);

    int32_t f4v_attach_bootstrap(f4v_frag_access_ctx_t *ctx, 
				 uint8_t *f4m_data,
				 size_t f4m_size);
    
    /**
     * cleanup fragment access context
     */
    void cleanup_f4v_frag_access_ctx(f4v_frag_access_ctx_t *ctx);

    /**
     * cleanup profile index tables
     */
    void cleanup_f4v_profile_index(f4v_frag_access_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif //_F4V_FRAG_ACCESS_

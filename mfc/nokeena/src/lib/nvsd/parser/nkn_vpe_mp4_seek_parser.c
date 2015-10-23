#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <alloca.h>

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_seek_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_memalloc.h"

extern io_handlers_t ioh;

#define MP4_FULL_BOX_SIZE 12
#define UDTA_MOD_SIZE (16 * 1024)
//#define _DBG_DUMP_TABLES_

int32_t mp4_set_mvhd_duration(moov_t *moov, size_t new_duration);
int32_t mp4_set_mdhd_duration(trak_t *trak, size_t new_duration);
int32_t mp4_set_tkhd_duration(trak_t *trak, size_t new_duration);
static inline int32_t mp4_write_ilst_text_type(void *data, 
					       uint8_t type,
				      bitstream_state_t *bs_out,
					       uint32_t tag_type); 

int32_t
mp4_set_tkhd_duration(trak_t *trak, size_t new_duration)
{
    bitstream_state_t *bs;
    size_t seek_to;
    int32_t field_size;
    uint8_t version;

    /* initialization */
    bs = NULL;
    seek_to = 0;
    field_size = 0;
    version = 0;

    /* open the I/O descriptor */
    bs = ioh.ioh_open((char*)(trak->data + trak->tkhd_pos - trak->pos), "wb", trak->tkhd_size);
    if (bs == NULL) {
	return -1;
    }
    /* read version */
    ioh.ioh_seek((void*)(bs), 8, SEEK_SET);
    ioh.ioh_read(&version, 1, 1, (void*)(bs));

    if(version == 1) {
	seek_to = 12 + 24;
	field_size = 8;
	new_duration = nkn_vpe_swap_uint64(new_duration);
    } else {
	seek_to = 12 + 16;
	field_size = 4;
	new_duration = nkn_vpe_swap_uint32(new_duration);
    }

    /* seek to duration */
    ioh.ioh_seek((void*)(bs), seek_to, SEEK_SET);

    /* write duration */
    ioh.ioh_write(&new_duration, 1, field_size, (void*)(bs));

    ioh.ioh_close((void*)(bs));

    return 0;
	
}

int32_t
mp4_set_mdhd_duration(trak_t *trak, size_t new_duration)
{
    bitstream_state_t *bs;
    size_t seek_to;
    int32_t field_size;
    uint8_t version;

    /* initialization */
    bs = NULL;
    seek_to = 0;
    field_size = 0;
    version = 0;

    /* open the I/O descriptor */
    bs = ioh.ioh_open((char*)(trak->data + trak->mdia->mdhd_pos - trak->pos), "wb", trak->mdia->mdhd_size);
    if (bs == NULL) {
	return -1;
    }
    /* read version */
    ioh.ioh_seek((void*)(bs), 8, SEEK_SET);
    ioh.ioh_read(&version, 1, 1, (void*)(bs));

    if(version == 1) {
	seek_to = 12 + 20;
	field_size = 8;
	new_duration = nkn_vpe_swap_uint64(new_duration);
    } else {
	seek_to = 12 + 12;
	field_size = 4;
	new_duration = nkn_vpe_swap_uint32(new_duration);
    }

    /* seek to duration */
    ioh.ioh_seek((void*)(bs), seek_to, SEEK_SET);

	
    /* write duration */
    ioh.ioh_write(&new_duration, 1, field_size, (void*)(bs));

    ioh.ioh_close((void*)(bs));

    return 0;
	
}

int32_t
mp4_set_mvhd_duration(moov_t *moov, size_t new_duration)
{
    bitstream_state_t *bs;
    size_t seek_to;
    int32_t field_size;
    uint8_t version;

    /* initialization */
    bs = NULL;
    seek_to = 0;
    field_size = 0;
    version = 0;

    /* open the I/O descriptor */
    bs = ioh.ioh_open((char*)(moov->data + moov->mvhd_pos), "wb", moov->mvhd_size);
    if (bs == NULL) {
	return -1;
    }
    /* read version */
    ioh.ioh_read(&version, 1, 1, (void*)(bs));

    if(version == 1) {
	seek_to = 12 + 20;
	field_size = 8;
	new_duration = nkn_vpe_swap_uint64(new_duration);
    } else {
	seek_to = 12 + 12;
	field_size = 4;
	new_duration = nkn_vpe_swap_uint32(new_duration);
    }

    /* seek to duration */
    ioh.ioh_seek((void*)(bs), seek_to, SEEK_SET);

    /* write duration */
    ioh.ioh_write(&new_duration, 1, field_size, (void*)(bs));

    ioh.ioh_close((void*)(bs));

    return 0;
	
}

size_t
mp4_stts_compute_trak_length(trak_t* trak)
{
    size_t tot_time;
    bitstream_state_t *bs;
    int32_t i, entry_count, sample_count, delta;

    /*initialization */
    tot_time = 0;
    bs = NULL;
    i  = 0;
    entry_count = sample_count = delta = 0;

    /*initialize to a bitstream */
    bs = ioh.ioh_open((char*)trak->stbl->stts_data, "rb", trak->size);
    if (bs == NULL) {
	return 0; // it is size_t
    }
    /* skip the extended box size */
    ioh.ioh_seek((void*)(bs), 12, SEEK_SET);
	
    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    /*do not need to check entry_count, it has been checked during the stts adjust*/
    entry_count = nkn_vpe_swap_uint32(entry_count);
	
    /* search for sample number for timestamp */
    for(i=0; i<entry_count; i++){
	ioh.ioh_read(&sample_count, 1, sizeof(int32_t), (void*)(bs));
	sample_count = nkn_vpe_swap_uint32(sample_count);
		
	ioh.ioh_read(&delta, 4, 1, (void*)(bs));
	delta = nkn_vpe_swap_uint32(delta);

	tot_time += (sample_count * delta);
    }

    ioh.ioh_close(bs);

    return tot_time;

}

size_t
mp4_adjust_sample_tables_copy(moov_t *moov, int32_t n_tracks_to_adjust, int32_t *start_sample_num,
			      int32_t *end_sample_num, int32_t *start_chunk_num, int32_t *end_chunk_num,
			      size_t *mdat_offset, table_shift_params_t *tsp, moov_t **modified_moov,
			      vpe_ol_t* ol, int n_box_to_find)
{
    bitstream_state_t *bs, *bs_out;
    size_t bytes_parsed, bcheck_bytes_left;
    uint32_t container_size, trak_no;
    box_t box;
    size_t box_pos, mdhd_time[MAX_TRACKS], mvhd_time, off, 
	bytes_to_skip;
    int32_t *type, trak_size_diff, adjusted_size, i, rv, fc_rv;
    uint8_t container_box[12];
    moov_t *moov_out;

    bs = bs_out = NULL;
    trak_size_diff = 0;
    bcheck_bytes_left = 0;
    bytes_to_skip = UINT64_MAX;
    rv = 0;

    if (!modified_moov) {
	goto error;
    }
    *modified_moov = NULL;
    *modified_moov = moov_out = (moov_t*)nkn_calloc_type(1, 
							 sizeof(moov_t),
							 mod_vpe_mp4_moov_t);
    if(!moov_out) {
	goto error;
    }
    moov_out->synced_seek_time = moov->synced_seek_time;
    moov_out->n_tracks_to_process = moov->n_tracks_to_process;
    moov_out->size = moov->size + 100;
    moov_out->data = (uint8_t*)nkn_malloc_type(moov_out->size,
					       mod_vpe_mp4_moov_buf);
    if (!moov_out->data) {
	goto error;
    }
    for (i = 0; i < MAX_TRACKS; i++) {
	moov_out->trak_process_map[i] = moov->trak_process_map[i];
    }
    bs = ioh.ioh_open((char*)(moov->data), "rb", moov->size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = ioh.ioh_open((char*)(moov_out->data), "wb", moov_out->size);
    if (bs_out == NULL) {
	goto error;
    }
    bytes_parsed = 0;
    trak_no = 0;

    for(i = 0;i < n_tracks_to_adjust;i++) {
	if(moov->trak_process_map[i]) {
	    off = mp4_get_first_sample_offset(moov->trak[i]);
	    if((mdat_offset[i] - off) < bytes_to_skip) {
		bytes_to_skip = (mdat_offset[i] - off);
	    }
	}
    }

    do {
	mp4_read_next_box(bs, &box_pos, &box);
#if 0
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
            /* corrupt MP4 file , and we do not support long size in moov*/
            goto error;
	}
#endif
	type = (int32_t*)(&box.type);
	if(is_container_box(&box) && (*type != FREE_ID)) {
	    /* if free box, just copy it as non container box */
	    /* this is a container box */
	    if(*type == META_ID) {
		/* META tag is a full box container */
		container_size = 12;
	    } else {
		container_size = 8;
	    }

	    switch(*type) {
		case TRAK_ID:
		    if(moov->trak_process_map[trak_no]) {
			rv = 0;
			moov_out->trak[trak_no] = mp4_init_parent(moov_out->data + ioh.ioh_tell((void*)bs_out),
								  ioh.ioh_tell((void*)bs_out), box.short_size, TRAK_ID);
			if (moov_out->trak[trak_no] == NULL) {
			    goto error;
			}
			trak_size_diff += mp4_adjust_trak(moov, trak_no, start_sample_num,
					     end_sample_num, start_chunk_num, end_chunk_num,
					     mdat_offset, tsp, bytes_to_skip, 
					     moov_out, bs_out,
					     mdhd_time, &rv);
			if (rv) {
#if 0 /* moov_out will be freed in tunnel code path*/
			    if (moov_out) {
				mp4_moov_out_cleanup(moov_out,
						     moov->n_tracks);
			    }
			    *modified_moov = NULL;
#endif
			    goto error;
			}
			trak_size_diff += rv;
		    } else {
			trak_size_diff += (-(int64_t)box.short_size);
		    }

		    trak_no++;
		    ioh.ioh_seek((void*)(bs), box.short_size, SEEK_CUR);
		    continue;
		case UDTA_ID:
		    moov_out->udta = mp4_init_parent(moov_out->data + ioh.ioh_tell((void*)bs_out),
						     ioh.ioh_tell((void*)bs_out),
						     box.short_size, UDTA_ID);
		    if (moov_out->udta == NULL) {
			goto error;
		    }
		    moov_out->udta_mod_size = (UDTA_MOD_SIZE);
		    moov_out->udta_mod =
		    (uint8_t*)nkn_malloc_type(moov_out->udta_mod_size,
					      mod_vpe_mp4_udta_buf);
		    if (moov_out->udta_mod == NULL) {
			goto error;
		    }
		    moov_out->udta_mod_pos = -1; /* indicates this needs to be appended */
		    break;
	    }

	    /* copy the container/ parent box */
	    bcheck_bytes_left = moov->size - bytes_parsed - 8;
	    if (container_size > bcheck_bytes_left) {
#if 0 /* moov_out will be freed in tunnel code path*/
		if (moov_out) {
		    mp4_moov_out_cleanup(moov_out,
				     moov->n_tracks);
		}
		*modified_moov = NULL;
		ioh.ioh_close((void*)bs);
		ioh.ioh_close((void*)bs_out);
#endif
		goto error;
	    }
	    ioh.ioh_read((void*)(container_box), 1, container_size, (void*)bs);
	    ioh.ioh_write((void*)(container_box), 1, container_size, (void*)(bs_out));
	} else {
	    switch(*type) {
		case MVHD_ID:
		    moov_out->mvhd_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->mvhd_pos = ioh.ioh_tell((void*)bs_out);
		    moov_out->mvhd_size = box.short_size;
		    break;
		case ILST_ID:
		    moov_out->udta->ilst_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->udta->ilst_pos = ioh.ioh_tell((void*)bs_out);
		    moov_out->udta->ilst_size = box.short_size;
		    break;
	    }
	    uint8_t *box_data;

	    box_data = (uint8_t*)nkn_malloc_type(box.short_size,
						 mod_vpe_mp4_box_data);
	    if (!box_data) {
		goto error;
	    }
	    ioh.ioh_read((void*)(box_data), 1, box.short_size, (void*)(bs));
	    ioh.ioh_write((void*)(box_data), 1, box.short_size, (void*)(bs_out));
	    free(box_data);
	}
	bytes_parsed = ioh.ioh_tell((void*)(bs));
    } while(bytes_parsed < moov->size);

    moov_out->size = adjusted_size = moov->size + trak_size_diff;
    mp4_write_box_header(moov_out->data, MOOV_ID, adjusted_size);

    mvhd_time = 0;
    for(i = 0; i < n_tracks_to_adjust;i++) {
	int32_t rebase_offset;
	off = mp4_get_first_sample_offset(moov->trak[i]);
	if(mdhd_time[i] > mvhd_time) {
	    mvhd_time = mdhd_time[i];
	}
	//mp4_adjust_stco(moov->trak[i], start_chunk_num[i] - 1, mdat_offset[i], bytes_to_skip, moov_out->trak[i]->stbl->stco_data);
	if (ol[0].offset > ol[1].offset) {
	    /*
	     * moov offset larger than mdat offset
	     * ftyp | box1 | mdat | box2 | moov | box3
	     * stco pos  is ftyp size + box1 size + chunks
	     * the generated MP4 will be
	     * ftyp | moov_out | mdat
	     * stco pos is fype size + moov_out size + chunks
	     * rebase offset = moov_out size - box1 size
	     */
	    rebase_offset = moov_out->size - (ol[1].offset - ol[2].length);

	} else {
	    /*
	     * moov offset samller than mdat offset
	     * ftyp | box1 | moov | box2 | mdat | box3
	     * stco pos is fyp size + box1 size + moov size + box2 size + chunks
	     * the generated MP4 will be
	     * ftyp | moov_out | mdat
	     * stco pos is ftyp size + moov_out size + chunks
	     * rebase offset = trak_size_diff - (box1 size + box2 size)
	     */
	    rebase_offset = trak_size_diff - (ol[1].offset - ol[0].length - ol[2].length);
	}
	fc_rv = mp4_rebase_stco(moov_out->trak[i], rebase_offset);
	if (fc_rv < 0) {
	    goto error;
	}
    }

    fc_rv = mp4_set_mvhd_duration(moov_out, mvhd_time);
    if (fc_rv < 0) {
	goto error;
    }
    ioh.ioh_close((void*)bs_out);
    ioh.ioh_close((void*)bs);

    return (size_t)(bytes_to_skip);
 error:
    if (bs_out) {
	ioh.ioh_close((void*)bs_out);
    }
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    return INT32_MIN;
}

size_t
mp4_adjust_trak(moov_t *moov, uint32_t trak_no, int32_t *start_sample_num, 
		int32_t *end_sample_num, int32_t *start_chunk_num, int32_t *end_chunk_num,
		size_t* mdat_offset,table_shift_params_t *tsp, size_t bytes_to_skip, 
		moov_t *moov_out, bitstream_state_t *out, 
		size_t *moov_time, int32_t *errcode)
{
    uint32_t i;
    uint8_t *p_dst;
    int32_t new_size, tot_size, *type;
    int32_t fc_rv;
    size_t bytes_parsed, sw;
    box_t box;
    bitstream_state_t *bs, *bs_out;
    uint8_t container_box[12];
    uint32_t container_size;
    size_t box_pos, tot_time, mdhd_time;
    int64_t new_stbl_size;

    i = trak_no;
    tot_size = 0;
    bytes_parsed = 0;
    new_stbl_size = 0;
    sw = 0;
    bs = ioh.ioh_open((char*)(moov->trak[i]->data), "rb", moov->size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = out;
    tot_time = mdhd_time = 0;

    do {
	mp4_read_next_box(bs, &box_pos, &box);
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
            /* corrupt MP4 file*/
            goto error;
	}
	type = (int32_t*)(&box.type);
	if(is_container_box(&box)) {
	    /* this is a container box */
	    if(*type == META_ID) {
		/* META tag is a full box container */
		container_size = 12;
	    }else {
		container_size = 8;
	    }

	    /*
	     * Read the boxes in the moov, and modified this box and put it to moov_out
	     * So the processing order of boxes depends on the boxes order in moov
	     * In mp4_read_trak(), we have the check for the order of these boxes is invalid or not.
	     * We will not check again here
	     */
	    switch(*type) {
		case STBL_ID:
		    moov_out->trak[i]->stbl = mp4_init_parent(moov_out->data + ioh.ioh_tell((void*)bs_out),
							      box_pos, box.short_size,
							      STBL_ID);
		    if (moov_out->trak[i]->stbl == NULL) {
			goto error;
		    }
		    break;
		case MDIA_ID:
		    moov_out->trak[i]->mdia = mp4_init_parent(moov_out->data + ioh.ioh_tell((void*)bs_out),
							      box_pos, box.short_size,
							      MDIA_ID);
		    if (moov_out->trak[i]->mdia == NULL) {
			goto error;
		    }
		    break;
		case MINF_ID:
		    moov_out->trak[i]->minf_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->minf_pos = ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->minf_size = box.short_size;
		    break;
		case DINF_ID:
		    moov_out->trak[i]->dinf_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->dinf_pos = ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->dinf_size = box.short_size;
		    break;
		case EDTS_ID:
		    moov_out->trak[i]->edts_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->edts_pos = ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->edts_size = box.short_size;
		    break;
	    }

	    /* copy the container/ parent box */
	    ioh.ioh_read((void*)(container_box), 1, container_size, (void*)bs);
	    ioh.ioh_write((void*)(container_box), 1, container_size, (void*)(bs_out));
	} else {
	    p_dst = moov_out->data + ioh.ioh_tell((void*)bs_out);	    
	    switch(*type) {
		case STTS_ID:
		    moov_out->trak[i]->stbl->stts_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    new_stbl_size += new_size = mp4_adjust_stts(moov->trak[i], start_sample_num[i], end_sample_num[i], &tsp[i].stts_sp, p_dst);
		    if(new_size)
			mp4_write_full_box_header(p_dst, STTS_ID, new_size, 0);
		    p_dst += new_size;
		    break;
		case CTTS_ID:    
		    moov_out->trak[i]->stbl->ctts_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    new_size = mp4_adjust_ctts(moov->trak[i], start_sample_num[i], end_sample_num[i], &tsp[i].ctts_sp, p_dst);
		    if (new_size < 0) {
			goto error;
		    }
		    new_stbl_size += new_size;
		    if(new_size)
			mp4_write_full_box_header(p_dst, CTTS_ID, new_size, 0);
		    p_dst += new_size;
		    break;
		case STSC_ID:
		    moov_out->trak[i]->stbl->stsc_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    new_size = mp4_adjust_stsc(moov->trak[i], start_chunk_num[i] - 1, end_chunk_num[i], start_sample_num[i], &tsp[i].stsc_sp, p_dst);
                    if (new_size < 0) {
		        goto error;
		    }
                    new_stbl_size += new_size;
		    if(new_size)
			mp4_write_full_box_header(p_dst, STSC_ID, new_size, 0);
		    p_dst += new_size;
		    break;
		case STSS_ID:
		    moov_out->trak[i]->stbl->stss_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    new_size = mp4_adjust_stss(moov->trak[i], start_sample_num[i], end_sample_num[i], &tsp[i].stss_sp, p_dst);
                    if (new_size < 0) {
		        goto error;
		    }
                    new_stbl_size += new_size;
		    if(new_size)
			mp4_write_full_box_header(p_dst, STSS_ID, new_size, 0);
		    p_dst += new_size;
		    break;
		case STSZ_ID:
		    moov_out->trak[i]->stbl->stsz_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    new_size = mp4_adjust_stsz(moov->trak[i], start_sample_num[i], end_sample_num[i], p_dst);
                    if (new_size < 0) {
		        goto error;
		    }
                    new_stbl_size += new_size;
		    if(new_size)
			mp4_write_full_box_header(p_dst, STSZ_ID, new_size, 0);
		    p_dst += new_size;
		    break;
		case STCO_ID:
		    moov_out->trak[i]->stbl->stco_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    new_size = mp4_adjust_stco(moov->trak[i], start_chunk_num[i] - 1,
					       mdat_offset[i], bytes_to_skip, p_dst);
                    if (new_size < 0) {
		        goto error;
		    }
                    new_stbl_size += new_size;
		    if(new_size)
			mp4_write_full_box_header(p_dst, STCO_ID, new_size, 0);
		    p_dst += new_size;
		    break;
		case TKHD_ID:
		    moov_out->trak[i]->tkhd_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->tkhd_pos = ioh.ioh_tell((void*)bs_out);;
		    moov_out->trak[i]->tkhd_size = box.short_size;
		    sw = mp4_copy_box(bs, bs_out, &box);
		    if (!sw) {
			goto error;
		    }
		    new_size = box.short_size;
		    break;
		case MDHD_ID:
		    moov_out->trak[i]->mdia->mdhd_data = moov_out->data + ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->mdia->mdhd_pos = ioh.ioh_tell((void*)bs_out);
		    moov_out->trak[i]->mdia->mdhd_size = box.short_size;
		    sw = mp4_copy_box(bs, bs_out, &box);
		    if (!sw) {
			goto error;
		    }
		    new_size = box.short_size;
		    break;
		case STSD_ID:
		case SDTP_ID:
		    new_stbl_size += box.short_size;
		default:
		    sw = mp4_copy_box(bs, bs_out, &box);
		    if (!sw) {
			goto error;
		    }
		    new_size = box.short_size;
		    break;
	    }//switch

	    ioh.ioh_seek((void*)bs, box.short_size, SEEK_CUR);
	    ioh.ioh_seek((void*)bs_out, (new_size), SEEK_CUR);
	}// not a container box

	bytes_parsed = ioh.ioh_tell((void*)bs);
    }while(bytes_parsed < moov->trak[i]->size);

    int32_t adjusted_size;
    new_stbl_size += 8;//account for self size (STBL box= 8 bytes)
    mp4_write_box_header(moov_out->trak[i]->stbl->data, STBL_ID, new_stbl_size);
    adjusted_size = moov_out->trak[i]->minf_size + (new_stbl_size - moov->trak[i]->stbl->size);
    mp4_write_box_header(moov_out->trak[i]->minf_data, MINF_ID, adjusted_size);
    adjusted_size = moov_out->trak[i]->mdia->size + (new_stbl_size - moov->trak[i]->stbl->size);
    mp4_write_box_header(moov_out->trak[i]->mdia->data, MDIA_ID, adjusted_size);
    adjusted_size = moov_out->trak[i]->size + (new_stbl_size - moov->trak[i]->stbl->size);
    mp4_write_box_header(moov_out->trak[i]->data, TRAK_ID, adjusted_size);

    tot_time = mp4_stts_compute_trak_length(moov_out->trak[i]);
    if (tot_time == 0) {
	goto error;
    }
    moov_time[i] = mdhd_time = (uint64_t)((float)(tot_time) * moov->mvhd->timescale / moov->trak[i]->mdia->mdhd->timescale);
    fc_rv = mp4_set_mdhd_duration(moov_out->trak[i], tot_time);
    if (fc_rv < 0) {
	goto error;
    }
    fc_rv = mp4_set_tkhd_duration(moov_out->trak[i], mdhd_time);
    if (fc_rv < 0) {
	goto error;
    }
    ioh.ioh_close(bs);

   return (new_stbl_size - moov->trak[i]->stbl->size);
 error:
   if (bs) {
       ioh.ioh_close((void*)bs);
   }
   *errcode = -E_VPE_MP4_OOB;
   return 0;
}

size_t
mp4_compute_parent_size(uint8_t *p_data)
{
    bitstream_state_t *bs;
    int32_t bytes_parsed, parent_count, *type;
    size_t container_size, rv, box_pos;
    box_t box;

    bytes_parsed = 0;
    container_size = 0;
    rv = 0;
    parent_count = 0;

    bs = ioh.ioh_open((char*)p_data, "rb", 100);
    
    do {
	mp4_read_next_box(bs, &box_pos, &box);
	type = (int32_t*)(&box.type);
	if(is_container_box(&box)) {
	    /* this is a container box */
	    if(*type == META_ID) {
		/* META tag is a full box container */
		container_size = 12;
	    }else { 
		container_size = 8;
	    }
	    
	    parent_count++;
	    ioh.ioh_seek((void*)bs, container_size, SEEK_CUR);
	} else {
	    rv += box.short_size;
	    ioh.ioh_seek((void*)bs, box.short_size, SEEK_CUR);
	}
    }while(parent_count < 2);

    return rv;
}

size_t
mp4_adjust_sample_tables_inplace(moov_t *moov, int32_t n_tracks_to_adjust, int32_t *start_sample_num, 
				 int32_t *end_sample_num, int32_t *start_chunk_num, int32_t *end_chunk_num,
				 size_t *mdat_offset, table_shift_params_t *tsp)
{
    int32_t i;
    size_t bytes_to_skip, off, 
	tot_time, mdhd_time, mvhd_time;

    /*initialization */
    i = 0;
    bytes_to_skip = UINT64_MAX;
    off = 0;
    tot_time = 0;
    mvhd_time = 0;
    mdhd_time = 0;

    for(i = 0; i < n_tracks_to_adjust; i++) {
	off = mp4_get_first_sample_offset(moov->trak[i]);
	if((mdat_offset[i] - off) < bytes_to_skip) {
	    bytes_to_skip = (mdat_offset[i] - off);
	}
    }

    for(i = 0; i < n_tracks_to_adjust; i++) {
	mp4_adjust_stts(moov->trak[i], start_sample_num[i], end_sample_num[i], &tsp[i].stts_sp, NULL);
	mp4_adjust_ctts(moov->trak[i], start_sample_num[i], end_sample_num[i], &tsp[i].ctts_sp, NULL);
	mp4_adjust_stsc(moov->trak[i], start_chunk_num[i] - 1, end_chunk_num[i], start_sample_num[i], &tsp[i].stsc_sp, NULL);
	mp4_adjust_stco(moov->trak[i], start_chunk_num[i] - 1, mdat_offset[i], bytes_to_skip, NULL);
	mp4_adjust_stss(moov->trak[i], start_sample_num[i], end_sample_num[i], &tsp[i].stss_sp, NULL);
	mp4_adjust_stsz(moov->trak[i], start_sample_num[i], end_sample_num[i], NULL);
	tot_time = mp4_stts_compute_trak_length(moov->trak[i]);
	mdhd_time = (uint64_t)((float)(tot_time) * moov->mvhd->timescale / moov->trak[i]->mdia->mdhd->timescale);
	mp4_set_mdhd_duration(moov->trak[i], tot_time);
	mp4_set_tkhd_duration(moov->trak[i], mdhd_time);
	if(mdhd_time > mvhd_time) 
	    mvhd_time = mdhd_time;
    } 
	
    mp4_set_mvhd_duration(moov, mvhd_time);
	
    return (size_t)(bytes_to_skip);
}

int32_t
mp4_adjust_stsz(trak_t *trak, int32_t sample_num, int32_t end_sample_num, uint8_t *out)
{
    bitstream_state_t *bs, *bs_out;
    int32_t entry_count, sample_size, tmp, i, box_size;
    uint8_t *p_src, *p_dst;

    /*initialization */
    bs = bs_out = NULL;
    p_src = p_dst = NULL;
    entry_count = sample_size = 0;
    i = 0;
    tmp = 0;

    /* open I/O descriptors */
    bs = ioh.ioh_open((char*)(trak->stbl->stsz_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    if(out)
	bs_out = ioh.ioh_open((char*)(out), "wb", trak->size);
    else
	bs_out = ioh.ioh_open((char*)(trak->stbl->stsz_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 20) {
        /* corrupted MP4K
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);

    ioh.ioh_read(&sample_size, 1, 4, (void*)(bs));
    if(sample_size != 0) {
	return 0;
    }

    p_src = trak->stbl->stsz_data  + (20 + ( (sample_num) * 4));;
    p_dst = trak->stbl->stsz_data + 20;
    ioh.ioh_read(&entry_count, 1, 4, (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    if (entry_count < 0) { /* corrupted MP4*/
        goto error;
    }
    if (entry_count * 4 + 12 + 8 < box_size) {
        goto error;
    }

    if(!out) {
	memmove(p_dst, p_src, ((entry_count - (sample_num)) * 4));
    } else {
	ioh.ioh_seek((void*)bs_out, 20, SEEK_SET);
	ioh.ioh_write((void*)p_src, 1, ((entry_count - (sample_num)) * 4), (void*)bs_out);
	ioh.ioh_seek((void*)bs_out, 12, SEEK_SET);
    }

    tmp = 0;
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));

    tmp = (entry_count - (sample_num));
    tmp = nkn_vpe_swap_uint32(tmp);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));

    ioh.ioh_close((void*)(bs_out));
    ioh.ioh_close((void*)(bs));

    return (((entry_count - (sample_num)) * 4) + MP4_FULL_BOX_SIZE + 8);
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
        ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

int32_t
mp4_adjust_stss(trak_t *trak, int32_t start_sample, int32_t end_sample, stss_shift_params_t *sp, uint8_t *out)
{
    bitstream_state_t *bs, *bs_out;
    size_t offset;
    int32_t entry_count, i, sync_sample, tmp, new_first_sync_sample_num, rv, box_size;

    /*initialization */
    bs = bs_out = NULL;
    entry_count = 0;
    i = 0;
    sync_sample = 0;
    tmp = 0;
    rv = 0;

    if(!trak->stbl->stss_data) {
	return 0;
    }

    /* open I/O descriptors */
    bs = ioh.ioh_open((char*)(trak->stbl->stss_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    if(out)
	bs_out = ioh.ioh_open((char*)(out), "wb", trak->size);
    else
	bs_out = ioh.ioh_open((char*)(trak->stbl->stss_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }

    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 12) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);
    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, 4, (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count <= 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 4 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }

    offset = 16 + ( (sp->entry_no-1) * 4);
    ioh.ioh_seek((void*)(bs), offset, SEEK_SET);
    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);

    ioh.ioh_read(&new_first_sync_sample_num, 1, 4, (void*)(bs));
    new_first_sync_sample_num = nkn_vpe_swap_uint32(new_first_sync_sample_num);
    ioh.ioh_seek((void*)(bs), offset, SEEK_SET);

    for(i = sp->entry_no; i <= entry_count; i++) {
	ioh.ioh_read(&sync_sample, 1, 4, (void*)(bs));
	tmp = nkn_vpe_swap_uint32(sync_sample);
	tmp = tmp - new_first_sync_sample_num + 1;
	sync_sample = nkn_vpe_swap_uint32(tmp);
	ioh.ioh_write(&sync_sample, 1, 4, (void*)(bs_out));
    }

    tmp = entry_count - (sp->entry_no-1);
    rv = (tmp * 4) + MP4_FULL_BOX_SIZE + 4;
    tmp = nkn_vpe_swap_uint32(tmp);

    ioh.ioh_seek((void*)(bs_out), 12, SEEK_SET);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));

    ioh.ioh_close(bs);
    ioh.ioh_close(bs_out);

    return rv;
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
        ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

size_t
mp4_get_first_sample_offset(trak_t *trak)
{
    bitstream_state_t *bs;
    size_t offset;

    /*initialization */
    bs = NULL;
    offset = 0;

    /* open I/O descriptors */
    bs = ioh.ioh_open((char*)(trak->stbl->stco_data), "rb", trak->size);
    ioh.ioh_seek(bs, 16, SEEK_SET);
    ioh.ioh_read(&offset, 1, 4, (void*)(bs));

    offset = nkn_vpe_swap_uint32(offset);

    ioh.ioh_close(bs);

    return (size_t)(offset);
}


int32_t
mp4_rebase_stco(trak_t *trak, int64_t bytes_to_skip)
{
    bitstream_state_t *bs, *bs_out;
    int32_t entry_count, tmp, i, box_size;
    int64_t off, new_off;

    /*initialization */
    bs = bs_out = NULL;
    entry_count = 0;
    tmp = 0;
    i = off = new_off = 0;

    /* open I/O descriptors */
    bs = ioh.ioh_open((char*)(trak->stbl->stco_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = ioh.ioh_open((char*)(trak->stbl->stco_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 16) {
    	/* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);

    /* read entry count */
    ioh.ioh_read(&entry_count, 1, 4, (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count < 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 4 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }


    //printf("\n\nstco rebase\n");
    for(i = 0; i < entry_count; i++) {
	ioh.ioh_read(&off, 1, 4, (void*)(bs));
	off = nkn_vpe_swap_uint32(off);
	new_off = (off) + (bytes_to_skip);
	//printf("%d\n", (int) new_off);
	tmp = nkn_vpe_swap_uint32(new_off);
	ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));
    }
		
//#ifdef _DBG_DUMP_TABLES_
#if 0
    { 
	FILE *f;
	//	f = fopen("/home/sunil/data/stco", "wb");
	//	printf("stco rebase\n");
	f = fopen("/nkn/vpe/tmp/stco_rebase", "wb");
	fwrite(trak->stbl->stco_data +16, entry_count, 4, f);
	fclose(f);
		
    }
#endif
    //memset(trak->stbl->stco_data + 16 + ((start_chunk) * 4), 
    //     0,
    //     ((entry_count - start_chunk) * 4));
 
    ioh.ioh_close(bs);
    ioh.ioh_close(bs_out);
	
    return 0;
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
        ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

int32_t
mp4_adjust_stco(trak_t *trak, int32_t start_chunk, size_t mdat_offset, size_t bytes_to_skip, uint8_t *out)
{
    bitstream_state_t *bs, *bs_out;
    uint8_t *p_src, *p_dst;
    int32_t entry_count, tmp, i, rv, box_size;
    size_t first_sample_offset, off, new_off;

    /*initialization */
    bs = bs_out = NULL;
    p_src = p_dst = NULL;
    entry_count = 0;
    tmp = 0;
    i = off = new_off = 0;
    rv = 0;
    first_sample_offset = 0;

    /* open I/O descriptors */
    bs = ioh.ioh_open((char*)(trak->stbl->stco_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    if(out)
	bs_out = ioh.ioh_open((char*)(out), "wb", trak->size);
    else
	bs_out = ioh.ioh_open((char*)(trak->stbl->stco_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 16) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    ioh.ioh_seek((void*)(bs_out), 12, SEEK_SET);

    /* read entry count */
    ioh.ioh_read(&entry_count, 1, 4, (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count < 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 4 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }
    tmp = entry_count - start_chunk;
    rv = (tmp * 4) + MP4_FULL_BOX_SIZE + 4;

    /* read first sample offset */
    ioh.ioh_read(&first_sample_offset, 1, 4, (void*)(bs));
    first_sample_offset = nkn_vpe_swap_uint32(first_sample_offset);

    /* re-base the stco box */
    p_src = trak->stbl->stco_data + 16 + (start_chunk * 4);
    if(out) {
	p_dst = out + 16;
    } else {
	p_dst = trak->stbl->stco_data + 16;
    }
    memmove(p_dst, 	p_src, (entry_count - start_chunk) * sizeof(uint32_t));

    tmp = entry_count - start_chunk;
    rv = (tmp * 4) + MP4_FULL_BOX_SIZE + 4;
    tmp = nkn_vpe_swap_uint32(tmp);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));

    /* the first entry should be pointing to the actual sample offset */
    tmp = nkn_vpe_swap_uint32(mdat_offset);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));

    if(out) {
	/* close old handle */
	ioh.ioh_close((void*)(bs));
	bs = ioh.ioh_open((char*)(out), "wb", trak->size);
    }
    ioh.ioh_seek((void*)(bs), 16, SEEK_SET);
    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);
		    
    //printf("\nadjust stco\n");
    for(i = 0; i < entry_count - start_chunk; i++) {
	ioh.ioh_read(&off, 1, 4, (void*)(bs));
	off = nkn_vpe_swap_uint32(off);
	new_off = (off) - (bytes_to_skip);
	//printf("%d\n", (int)new_off);
	tmp = nkn_vpe_swap_uint32(new_off);
	ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));
    }
    //    printf("\n");
		
//#ifdef _DBG_DUMP_TABLES_
#if 0
    { 
	FILE *f;
	//	f = fopen("/home/sunil/data/stco", "wb");
	//	printf("adjust stco\n");
	f = fopen("/nkn/vpe/tmp/stco_adjust", "wb");
	fwrite(trak->stbl->stco_data +16, (entry_count-start_chunk), 4, f);
	fclose(f);
		
    }
#endif
    //memset(trak->stbl->stco_data + 16 + ((start_chunk) * 4), 
    //     0,
    //     ((entry_count - start_chunk) * 4));

    ioh.ioh_close(bs);
    ioh.ioh_close(bs_out);
	
    return rv;
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
        ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

int32_t
mp4_adjust_ctts(trak_t *trak, int32_t start_sample, int32_t end_sample, ctts_shift_params_t *sp, uint8_t *out)
{
    bitstream_state_t *bs, *bs_out;
    int32_t num_entries, new_num_entries, rv, box_size;
    size_t src_offset, dst_offset, size, tmp;

    /*initialization */
    bs = NULL;
    bs_out = NULL;
    num_entries = new_num_entries = 0;
    src_offset = dst_offset = tmp = 0;
    rv = 0;

    /* if ctts table doesnt exist do nothing */
    if(!trak->stbl->ctts_data) {
	return 0;
    }

    /* open I/O descriptors */
    if(out)
	bs_out = ioh.ioh_open((char*)(out), "wb", trak->size);
    else
	bs_out = ioh.ioh_open((char*)(trak->stbl->ctts_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }
    bs = ioh.ioh_open((char*)(trak->stbl->ctts_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }

    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 12) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read num of entries */
    ioh.ioh_read(&num_entries, 1, 4, (void*)(bs));
    num_entries = nkn_vpe_swap_uint32(num_entries);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (num_entries < 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((num_entries * 8 + 12 + 4) > box_size) {
        /* corrupted MP4 */
	goto error;
    }

    /* skip the output descriptor to the start of the table */
    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);

    /* skip to the correct entry */
    src_offset = 24 + ( (sp->entry_no) * 8 );
    dst_offset = 24;
    new_num_entries = (num_entries - (sp->entry_no + 1)) + 1;
    size = (new_num_entries * 8);

    tmp = nkn_vpe_swap_uint32((sp->new_num_samples-1));
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));
    tmp = nkn_vpe_swap_uint32(sp->new_delta);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));
    ioh.ioh_seek(bs_out, dst_offset, SEEK_SET);
    ioh.ioh_write(trak->stbl->ctts_data + src_offset,
		  1, size, bs_out);

    ioh.ioh_seek((void *)(bs_out), 12, SEEK_SET);
    rv = (new_num_entries * 8) + MP4_FULL_BOX_SIZE + 4;
    new_num_entries = nkn_vpe_swap_uint32(new_num_entries);
    ioh.ioh_write(&new_num_entries, 1, 4, (void*)(bs_out));

    ioh.ioh_close((void*)(bs));
    ioh.ioh_close((void*)(bs_out));
	
    return rv;
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
        ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

int32_t
mp4_adjust_stts(trak_t *trak, int32_t start_sample, int32_t end_sample, stts_shift_params_t *sp, uint8_t *out)
{
    bitstream_state_t *bs, *bs_out;
    int32_t num_entries, new_num_entries, tmp, rv, box_size;
    size_t src_offset, dst_offset, size;

    /*initialization */
    bs = NULL;
    bs_out = NULL;
    num_entries = new_num_entries = 0;
    src_offset = dst_offset = 0;
    tmp = 0;

    /* open I/O descriptors */
    if(out)
        bs_out = ioh.ioh_open((char*)(out), "wb", trak->size);
    else
        bs_out = ioh.ioh_open((char*)(trak->stbl->stss_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }
    bs = ioh.ioh_open((char*)(trak->stbl->stts_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }

    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 12) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read num of entries */
    ioh.ioh_read(&num_entries, 1, 4, (void*)(bs));
    num_entries = nkn_vpe_swap_uint32(num_entries);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (num_entries < 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((num_entries * 8 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }

    /* skip the output descriptor to the start of the table */
    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);

    /* skip to the correct entry */
#if 0
    if( (sp->entry_no + 2) == num_entries) {
	dst_offset = src_offset = 24;
    } else {
	src_offset = 24 + (sp->entry_no+1) * 8;
	dst_offset = 24;
    }
#else
    src_offset = 16 + (sp->entry_no+1) * 8;
    dst_offset = 24;
#endif
    new_num_entries = (num_entries - (sp->entry_no + 1)) + 1;
    size = ((new_num_entries-1) * 8);

    tmp = nkn_vpe_swap_uint32(sp->new_num_samples);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));
    tmp = nkn_vpe_swap_uint32(sp->new_delta);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));
	
    if(new_num_entries > 1) {
	if(!out) {
	    memcpy(trak->stbl->stts_data + dst_offset,
		   trak->stbl->stts_data + src_offset,
		   size);
	} else {
	    //	    ioh.ioh_seek((void*)bs_out, 0, SEEK_SET); 
	    ioh.ioh_write((void*)(trak->stbl->stts_data + src_offset), 
			  1, size, (void*)(bs_out));
	}
    }

    ioh.ioh_seek((void *)(bs_out), 12, SEEK_SET);
    rv = (new_num_entries * 8) + MP4_FULL_BOX_SIZE + 4;
    new_num_entries = nkn_vpe_swap_uint32(new_num_entries);
    ioh.ioh_write(&new_num_entries, 1, 4, (void*)(bs_out));

#ifdef _DBG_DUMP_TABLES_
    {
	int32_t tmp;
	FILE *f = fopen("/home/sunil/data/stts", "wb");
	tmp = nkn_vpe_swap_uint32(new_num_entries);
	fwrite(&bs->data[12+4], 1, (tmp * 8), f);
	fclose(f);
    }
#endif
    ioh.ioh_close((void*)(bs));
    ioh.ioh_close((void*)(bs_out));
	
    return (rv);
 error:
    if (bs) {
	ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
	ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

int32_t
mp4_adjust_stsc(trak_t *trak, int32_t start_chunk, int32_t end_chunk, 
		int32_t start_sample, stsc_shift_params_t *sp, 
		uint8_t *out)
{
    int32_t entry_count, i, curr_chunk, num_sample_chnk, sample_desc,
	new_samples_per_chunk, j, tmp, prev_chunk, k, start_entry, rv,
	box_size;
    bitstream_state_t *bs, *bs_out;

    /*initialization */
    bs = NULL;
    entry_count = 0;
    i = 0;
    k = 0;
    rv = 0;
    j= 1;
    start_entry = sp->entry_no + 1;

    bs = ioh.ioh_open((char*)(trak->stbl->stsc_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    if(out)
        bs_out = ioh.ioh_open((char*)(out), "wb", trak->size);
    else
        bs_out = ioh.ioh_open((char*)(trak->stbl->stsc_data), "wb", trak->size);
    if (bs_out == NULL) {
	goto error;
    }

    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 12) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);


    /* skip the output descriptor past the entry count for output */
    ioh.ioh_seek((void*)(bs_out), 16, SEEK_SET);

    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count < 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 8 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }
    /* process the first chunk  
     * Split it into 2 entries, the current chunk's remaining samples forms one entry
     * rest of the current run forms another entry
     */
    k = 1;
    mp4_get_stsc_details(bs, sp->entry_no, &curr_chunk, &num_sample_chnk, &sample_desc);
    prev_chunk = curr_chunk = nkn_vpe_swap_uint32(curr_chunk);
    num_sample_chnk = nkn_vpe_swap_uint32(num_sample_chnk);
    new_samples_per_chunk = (sp->new_samples_per_chunk + sp->new_first_sample_in_chunk) - start_sample;
    k = nkn_vpe_swap_uint32(1);
    new_samples_per_chunk = nkn_vpe_swap_uint32(new_samples_per_chunk);
    mp4_set_stsc_details(bs_out, 0, k, new_samples_per_chunk, sample_desc);

    if ((sp->entry_no + 1) != entry_count) {
    	int32_t next_chunk, next_sample_chnk, next_sample_desc;	
    	mp4_get_stsc_details(bs, sp->entry_no + 1, &next_chunk, &next_sample_chnk, &next_sample_desc);
    	next_chunk = nkn_vpe_swap_uint32(next_chunk);
    	next_sample_chnk = nkn_vpe_swap_uint32(next_sample_chnk);


    	/* check if we need to split the current stsc entry into two parts */
    	if(next_chunk == (prev_chunk + 1)) {
		/* patch with next sample */
		tmp = nkn_vpe_swap_uint32(new_samples_per_chunk);
	
		if(next_sample_chnk == tmp) {
	    	//			k = nkn_vpe_swap_uint32(3);
	    	//tmp = nkn_vpe_swap_uint32(next_sample_chnk);
	    	//mp4_set_stsc_details(bs_out, 1, k, tmp, sample_desc);
	    	//start_chunk--;
	  
		} 
		//start_entry++;
		j--;
    	} else {
		/* current stsc entry splits to two parts */
		k = nkn_vpe_swap_uint32(2);
		num_sample_chnk = nkn_vpe_swap_uint32(num_sample_chnk);
		mp4_set_stsc_details(bs_out, 1, k, num_sample_chnk, sample_desc);
    	}
    } else {
		// last chunk only one entry can be written
		j--;		
    }

	
    /* process rest of the chunks */
    for(i=start_entry; i < entry_count; i++) {
	mp4_get_stsc_details(bs, i, &curr_chunk, &num_sample_chnk, &sample_desc);
	curr_chunk = nkn_vpe_swap_uint32(curr_chunk);
	tmp = curr_chunk - start_chunk;//sp->new_first_sample_in_chunk;
	curr_chunk = nkn_vpe_swap_uint32(tmp);
	mp4_set_stsc_details(bs_out, j + 1, curr_chunk, num_sample_chnk, sample_desc);
	j++;
    }

    ioh.ioh_seek((void*)bs_out, 12, SEEK_SET);
    j++; /*account for the first entry outside the loop */
    rv = (j * 12) + MP4_FULL_BOX_SIZE + 4;
    tmp = nkn_vpe_swap_uint32(j);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs_out));

#ifdef _DBG_DUMP_TABLES_		
    {
	FILE *f = fopen("/home/sunil/data/stsc", "wb");
	fwrite(&bs_out->data[16], 1, ((j) * 12), f);
	fclose(f);
    }
#endif
    
    ioh.ioh_close(bs);
    ioh.ioh_close(bs_out);

    return rv;
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    if (bs_out) {
        ioh.ioh_close((void*)(bs_out));
    }
    return -1;
}

int32_t
mp4_set_stsc_details(bitstream_state_t *bs, int32_t sample_num, int32_t first_chunk, int32_t samples_per_chunk, int32_t sample_description)
{

    int32_t offset, tmp;
    size_t curr_pos;

    /*initialization */
    offset = 0;
    tmp = 0;
    curr_pos = ioh.ioh_tell((void*)(bs));

    offset = 16 + (12 * sample_num);
    ioh.ioh_seek((void*)(bs), offset, SEEK_SET);
    tmp =(first_chunk);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs));
    tmp = (samples_per_chunk);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs));
    tmp = (sample_description);
    ioh.ioh_write(&tmp, 1, 4, (void*)(bs));

    /* restore the io descriptor to original offset */
    ioh.ioh_seek((void*)(bs), curr_pos, SEEK_SET);	
    return 0;
}

int32_t
mp4_get_stsz_num_entries(trak_t *trak)
{
    bitstream_state_t *bs;
    int32_t entry_count, sample_size, box_size;

    /*initialization */
    bs = NULL;
    sample_size = entry_count = 0;

    bs = ioh.ioh_open((char*)(trak->stbl->stsz_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 20) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read the entry count and sample size fields */
    ioh.ioh_read(&sample_size, 1, sizeof(int32_t), (void*)(bs));
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    sample_size = nkn_vpe_swap_uint32(sample_size);
    if (entry_count < 0) { /* corrupted MP4*/
	goto error;
    }
    if (entry_count * 4 + 12 + 8 < box_size) {
	goto error;
    }
    ioh.ioh_close(bs);
    return entry_count;
 error:
    if (bs) {
	ioh.ioh_close(bs);
    }
    return -1;
}


//! new mp4 parser API
//! sample num --> input
//! trak ptr --> input
//! sample duration -->output
//! trak->stbl->stts_ctxt --> input/output
uint64_t
mp4_sample_to_duration(trak_t *trak, int32_t sample)
{
    bitstream_state_t *bs;
    uint64_t rv;
    int32_t entry_count, tot_sample,
	sample_count, sample_duration;
    uint32_t offset, sample_entry_count;
    uint32_t i;

    /* initialization */
    bs = NULL;
    rv = 0;
    entry_count = 0;
    offset = 0;
    sample_entry_count = 0;
    tot_sample= 0;
    
    if(sample >= trak->stbl->stts_ctxt.prev_sample_num){
    	//optimized processing only if requested sample
    	// is same or more than previous requested sample num
    	// in runlength table
    	offset = trak->stbl->stts_ctxt.prev_offset;
    	tot_sample = trak->stbl->stts_ctxt.current_totsam;
	sample_entry_count = (trak->stbl->stts_ctxt.prev_offset>>3);
	trak->stbl->stts_ctxt.prev_sample_num = sample;
    }else{
        memset(&trak->stbl->stts_ctxt, 0, sizeof(runt_ctxt_t));
    }

    bs = ioh.ioh_open((char*)(trak->stbl->stts_data), "rb", trak->size);
	
    if(!trak->stbl->stts_ctxt.entry_count){
       ioh.ioh_seek((void*)(bs), 12, SEEK_SET);
       ioh.ioh_read(&entry_count, 1, 4, (void*)(bs));
       entry_count = nkn_vpe_swap_uint32(entry_count);
       trak->stbl->stts_ctxt.entry_count = entry_count;
    }
    ioh.ioh_seek((void*)(bs), offset + 16, SEEK_SET);

    for(i = sample_entry_count ; i < trak->stbl->stts_ctxt.entry_count; i++) {
	ioh.ioh_read(&sample_count, 1, 4, (void*)(bs));
	ioh.ioh_read(&sample_duration, 1, 4, (void*)(bs));
	sample_count = nkn_vpe_swap_uint32(sample_count);
	sample_duration = nkn_vpe_swap_uint32(sample_duration);
	tot_sample += sample_count;

	if(tot_sample  > sample) {
	    rv = (uint64_t)(sample_duration) ;
	    break;
	}
	trak->stbl->stts_ctxt.prev_offset += 8;
	trak->stbl->stts_ctxt.current_totsam = tot_sample;
    }
    
    ioh.ioh_close((void*)(bs));
    return rv;
}


uint64_t
mp4_sample_to_time(trak_t *trak, int32_t sample)
{
    bitstream_state_t *bs;
    uint64_t rv;
    int32_t entry_count, tot_sample, box_size,
	sample_count, sample_duration;

    uint32_t i, sample_entry_count, offset;
    /* initialization */
    bs = NULL;
    rv = 0;
    entry_count = 0;
    tot_sample = 0;
    offset = 0;
    sample_entry_count = 0;


    if(sample >= trak->stbl->stts_ctxt.prev_sample_num){
    	//optimized processing only if requested sample
    	// is same or more than previous requested sample num
    	// in runlength table
    	offset = trak->stbl->stts_ctxt.prev_offset;
    	tot_sample = trak->stbl->stts_ctxt.current_totsam;
	sample_entry_count = (trak->stbl->stts_ctxt.prev_offset>>3);
	rv +=  trak->stbl->stts_ctxt.current_accum_value;
	trak->stbl->stts_ctxt.prev_sample_num = sample;
    }else{
        memset(&trak->stbl->stts_ctxt, 0, sizeof(runt_ctxt_t));
    }

    bs = ioh.ioh_open((char*)(trak->stbl->stts_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }

    if(!trak->stbl->stts_ctxt.entry_count){
	/* read the box size */
	ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
	box_size = nkn_vpe_swap_uint32(box_size);
	if (box_size < 16) {
	    /* corrupted MP4
	     * and we also do not support long size of this box
	     */
	    goto error;
	}
	/* skip the rest part of the extend box header*/
	ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);
	ioh.ioh_read(&entry_count, 1, 4, (void*)(bs));
	entry_count = nkn_vpe_swap_uint32(entry_count);
	/*
	 * add a check
	 * 1.entry_count is non-negative
	 * 2. size of entry_count*entry_size + header should <= box_size
	 */
	if (entry_count < 0) { /* corrupted MP4 */
	    goto error;
	}
	if ((entry_count * 8 + 12 + 4) > box_size) { /* corrupted MP4 */
	    goto error;
	}
	trak->stbl->stts_ctxt.entry_count = entry_count;
    }
    ioh.ioh_seek((void*)(bs), offset + 16, SEEK_SET);

    for(i = sample_entry_count; i <  trak->stbl->stts_ctxt.entry_count; i++) {
	ioh.ioh_read(&sample_count, 1, 4, (void*)(bs));
	ioh.ioh_read(&sample_duration, 1, 4, (void*)(bs));
	sample_count = nkn_vpe_swap_uint32(sample_count);
	sample_duration = nkn_vpe_swap_uint32(sample_duration);

	if((tot_sample + sample_count) > sample) {
	    rv = rv + ( ((uint64_t)(sample - tot_sample)) * ((uint64_t)(sample_duration)) );
	    break;
	}

	tot_sample += sample_count;
	rv += ( ((uint64_t)(sample_count)) * ((uint64_t)(sample_duration)) );
	trak->stbl->stts_ctxt.prev_offset += 8;
	trak->stbl->stts_ctxt.current_totsam = tot_sample;
	trak->stbl->stts_ctxt.current_accum_value = rv;
    }
    
    ioh.ioh_close((void*)(bs));
    return rv;
 error:
    if (bs) {
	ioh.ioh_close((void*)(bs));
    }
    return SIZE_MAX;
}
		
size_t
mp4_find_cumulative_sample_size(trak_t *trak, int32_t first_sample, int32_t sample_num)
{
    bitstream_state_t *bs;
    uint32_t total_size, i, sample_size, temp;
    int32_t box_size, entry_count;
    /*initialization */
    bs = NULL;
    total_size = temp = sample_size = 0;

    bs = ioh.ioh_open((char*)(trak->stbl->stsz_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 20) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);
    ioh.ioh_read(&sample_size, 1, sizeof(int32_t), (void*)(bs));
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    sample_size = nkn_vpe_swap_uint32(sample_size);
    if (entry_count * 4 + 12 + 8 < box_size) {
        goto error;
    }
    /* whether it is valid */
    if (first_sample > entry_count) {
	goto error;
    }
    if (sample_num > entry_count) {
	goto error;
    }
    if(sample_size!=0)
	total_size= sample_size*(sample_num-first_sample);
    else{
	for(i=first_sample;i< (uint32_t)sample_num;i++){
	    mp4_read_stsz_entry(bs, i ,(int32_t*)(&temp));
	    temp = nkn_vpe_swap_uint32(temp);
	    total_size+=temp;
	}
    }

    ioh.ioh_close(bs);
    return total_size;
 error:
    if (bs) {
	ioh.ioh_close(bs);
    }
    return SIZE_MAX;
}

int32_t
mp4_read_stsz_entry(bitstream_state_t *bs, int32_t entry_no, int32_t *size)
{
    int32_t offset;
    size_t curr_pos;

    /*initialization */
    offset = 0;
    curr_pos = ioh.ioh_tell((void*)(bs));

    offset = 20 + (4 * entry_no);
    ioh.ioh_seek((void*)(bs), offset, SEEK_SET);
    ioh.ioh_read(size, 1, 4, (void*)(bs));
    /*not reading the sample description index */

    /* restore the io descriptor to original offset */
    ioh.ioh_seek((void*)(bs), curr_pos, SEEK_SET);

    return 0;
}

size_t
mp4_chunk_num_to_chunk_offset(trak_t *trak, int32_t chunk_num)
{
    bitstream_state_t *bs;
    size_t offset;
    int32_t box_size, entry_count;

    /*initialization */
    bs = NULL;
    offset = 0;

    bs = ioh.ioh_open((char*)(trak->stbl->stco_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 16) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count < 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 4 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }

    /* check whether this is a valid chunk_num */
    if (chunk_num >= entry_count) {
	goto error;
    }
    /* skip to the correct table entry */
    ioh.ioh_seek((void*)(bs), (chunk_num) * 4, SEEK_CUR);

    /* read the entry */
    ioh.ioh_read(&offset, 1, sizeof(int32_t), (void*)(bs));

    ioh.ioh_close(bs);

    return (size_t)(nkn_vpe_swap_uint32(offset));
 error:
    if (bs) {
	ioh.ioh_close(bs);
    }
    return SIZE_MAX; // size_t return
}
#if 1
//! new optimized function

uint32_t
mp4_get_ctts_offset(trak_t *trak, int32_t sample_num)
{
  bitstream_state_t *bs;
  int32_t entry_count = 0, temp_sam_cnt = 0, sample_cnt = 0;
  uint32_t i = 0;
  uint32_t delta = 0;
  uint32_t offset = 0, sample_entry_count = 0;
  
  /*initialization */
  bs = NULL;
  bs = ioh.ioh_open((char*)(trak->stbl->ctts_data), "rb", trak->size);
  
  if(sample_num >= trak->stbl->ctts_ctxt.prev_sample_num){
    	//optimized processing only if requested sample
    	// is same or more than previous requested sample num
    	// in runlength table 	
 	sample_cnt = trak->stbl->ctts_ctxt.current_totsam;
	offset = trak->stbl->ctts_ctxt.prev_offset;
	sample_entry_count = (trak->stbl->ctts_ctxt.prev_offset>>8);
	trak->stbl->ctts_ctxt.prev_sample_num = sample_num;
  }else{
        memset(&trak->stbl->ctts_ctxt, 0, sizeof(runt_ctxt_t));
  }
  
  if(!trak->stbl->ctts_ctxt.entry_count){
	  /* skip the extended box size*/
	  ioh.ioh_seek((void*)(bs), 12, SEEK_SET);
	  /* read the entry */
	  ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
	  entry_count = nkn_vpe_swap_uint32(entry_count);
	  trak->stbl->ctts_ctxt.entry_count = entry_count;
  }
  /* skip the entry_count*/
  //ioh.ioh_seek((void*)(bs), 4, SEEK_CUR);
  ioh.ioh_seek((void*)(bs), offset + 16, SEEK_SET);

  for(i = sample_entry_count; i<trak->stbl->ctts_ctxt.entry_count; i++) {
    ioh.ioh_read(&temp_sam_cnt, 1, sizeof(int32_t), (void*)(bs));
     temp_sam_cnt = nkn_vpe_swap_uint32(temp_sam_cnt);
    sample_cnt +=temp_sam_cnt;
    /* skip the sample_count*/
    //ioh.ioh_seek((void*)(bs), 4, SEEK_CUR);
    ioh.ioh_read(&delta, 1, sizeof(int32_t), (void*)(bs));
    delta = nkn_vpe_swap_uint32(delta);
    /* skip the delta*/
    //ioh.ioh_seek((void*)(bs), 4, SEEK_CUR);
    if(sample_num<=sample_cnt)
      break;
    trak->stbl->ctts_ctxt.prev_offset += sizeof(int32_t);
    trak->stbl->ctts_ctxt.prev_offset += sizeof(int32_t);
    trak->stbl->ctts_ctxt.current_totsam = sample_cnt;

  }
  ioh.ioh_close(bs);
  return delta;

}
#else
uint32_t
mp4_get_ctts_offset(trak_t *trak, int32_t sample_num)
{
  bitstream_state_t *bs;
  int32_t entry_count = 0, temp_sam_cnt = 0, i = 0, sample_cnt = 0;
  uint32_t delta = 0;
  /*initialization */
  bs = NULL;
  bs = ioh.ioh_open((char*)(trak->stbl->ctts_data), "rb", trak->size);

  /* skip the extended box size*/
  ioh.ioh_seek((void*)(bs), 12, SEEK_SET);
  /* read the entry */
  ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
  entry_count = nkn_vpe_swap_uint32(entry_count);
  /* skip the entry_count*/
  //ioh.ioh_seek((void*)(bs), 4, SEEK_CUR);

  for(i=0; i<entry_count; i++) {
    ioh.ioh_read(&temp_sam_cnt, 1, sizeof(int32_t), (void*)(bs));
    temp_sam_cnt = nkn_vpe_swap_uint32(temp_sam_cnt);
    sample_cnt +=temp_sam_cnt;
    /* skip the sample_count*/
    //ioh.ioh_seek((void*)(bs), 4, SEEK_CUR);

    ioh.ioh_read(&delta, 1, sizeof(int32_t), (void*)(bs));
    delta = nkn_vpe_swap_uint32(delta);
    /* skip the delta*/
    //ioh.ioh_seek((void*)(bs), 4, SEEK_CUR);
    if(sample_num<=sample_cnt)
      break;
  }
  ioh.ioh_close(bs);
  return delta;

}
#endif
uint64_t
mp4_chunk_num_to_chunk_offset_co64(trak_t *trak, int32_t chunk_num)
{
  bitstream_state_t *bs;
  size_t offset;

  /*initialization */
  bs = NULL;
  offset = 0;

  bs = ioh.ioh_open((char*)(trak->stbl->co64_data), "rb", trak->size);

  /* skip the extended box size and num entries*/
  ioh.ioh_seek((void*)(bs), 16, SEEK_SET);

  /* skip to the correct table entry */
  ioh.ioh_seek((void*)(bs), (chunk_num) * 8, SEEK_CUR);

  /* read the entry */
  ioh.ioh_read(&offset, 1, sizeof(int64_t), (void*)(bs));

  ioh.ioh_close(bs);

  return (nkn_vpe_swap_uint64(offset));

}

uint32_t
mp4_get_stco_entry_count(trak_t *trak)
{
    bitstream_state_t *bs;
    uint32_t entry_count;

    /*initialization */
    bs = NULL;
    entry_count = 0;

    bs = ioh.ioh_open((char*)(trak->stbl->stco_data), "rb", trak->size);

    /* skip the extended box size and num entries*/
    ioh.ioh_seek((void*)(bs), 12, SEEK_SET);

    /* read the entry count*/
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));

    ioh.ioh_close(bs);

    return (nkn_vpe_swap_uint32(entry_count));

}

uint32_t
mp4_get_co64_entry_count(trak_t *trak)
{
  bitstream_state_t *bs;
  uint32_t entry_count;

  /*initialization */
  bs = NULL;
  entry_count = 0;

  bs = ioh.ioh_open((char*)(trak->stbl->co64_data), "rb", trak->size);

  /* skip the extended box size and num entries*/
  ioh.ioh_seek((void*)(bs), 12, SEEK_SET);

  /* read the entry count*/
  ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));

  ioh.ioh_close(bs);

  return (nkn_vpe_swap_uint32(entry_count));

}

#if 1
int32_t
mp4_sample_to_chunk_num(
	trak_t *trak, 
	int32_t sample_number, 
	int32_t *first_sample_in_chunk, 
	stsc_shift_params_t *sp)
{
    int32_t entry_count,total_sample_num,starting_sample_num,
	next_chunk,num_chunks,num_sample_chnk,chunk_num,
	first_chunk,total_prev, dummy2, box_size;
    bitstream_state_t *bs;
    uint32_t i;
    uint32_t offset = 0;
    uint32_t sample_entry_count = 0;
    
    /*initialization */
    bs = NULL;
    entry_count = 0;
    total_sample_num = 0;
    i = 0;
    chunk_num = 0;

    if(sample_number >= trak->stbl->stsc_ctxt.prev_sample_num){
    	total_sample_num = trak->stbl->stsc_ctxt.current_totsam;
	offset = trak->stbl->stsc_ctxt.prev_offset;
	sample_entry_count = (trak->stbl->stsc_ctxt.prev_offset/12);	
	trak->stbl->stsc_ctxt.prev_sample_num = sample_number;
    }else{
        memset(&trak->stbl->stts_ctxt, 0, sizeof(runt_ctxt_t));
    }

    bs = ioh.ioh_open((char*)(trak->stbl->stsc_data), "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    if(!trak->stbl->stsc_ctxt.entry_count){
	/* read the box size */
	ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
	box_size = nkn_vpe_swap_uint32(box_size);
	if (box_size < 16) {
	    /* corrupted MP4
	     * and we also do not support long size of this box
	     */
	    goto error;
	}
	/* skip the rest part of the extend box header*/
	ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);
	/* read num of entries */
	ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
	entry_count = nkn_vpe_swap_uint32(entry_count);
	/*
	 * add a check
	 * 1.entry_count is non-negative
	 * 2. size of entry_count*entry_size + header should <= box_size
	 */
	if (entry_count < 0) { /* corrupted MP4 */
	    goto error;
	}
	if ((entry_count * 12 + 12 + 4) > box_size) { /* corrupted MP4 */
	    goto error;
	}
	trak->stbl->stsc_ctxt.entry_count = entry_count;
    }

    ioh.ioh_seek((void*)(bs), offset + 16, SEEK_SET);

    for(i= sample_entry_count ; i<trak->stbl->stsc_ctxt.entry_count; i++) {
	total_prev = total_sample_num;
	starting_sample_num = total_sample_num;

	/* read curr stsc entry */
	mp4_get_stsc_details(bs, i, &first_chunk, &num_sample_chnk, &dummy2);
	first_chunk = nkn_vpe_swap_uint32(first_chunk);
	num_sample_chnk = nkn_vpe_swap_uint32(num_sample_chnk);

	if(i!=trak->stbl->stsc_ctxt.entry_count-1){
	    int32_t dummy;

	    /* read next stsc entry */
	    mp4_get_stsc_details(bs, i + 1, &next_chunk, &dummy, &dummy2);
	    next_chunk = nkn_vpe_swap_uint32(next_chunk);
	    num_chunks = next_chunk-first_chunk;
	} else {
	  if(trak->stbl->stco_data == NULL)
	    num_chunks= (mp4_get_co64_entry_count(trak) - first_chunk) + 1;
	  else
	    num_chunks= (mp4_get_stco_entry_count(trak) - first_chunk) + 1;
	}

	/* save start sample number of each run */
	*first_sample_in_chunk = total_sample_num;

	total_sample_num+=num_chunks*num_sample_chnk;
	if(sample_number<total_sample_num){
	    chunk_num = first_chunk + ((sample_number-total_prev)/num_sample_chnk);
	    *first_sample_in_chunk = *first_sample_in_chunk + ( (chunk_num - first_chunk) * num_sample_chnk);
	    if(sp) {
		sp->new_samples_per_chunk = num_sample_chnk;
		sp->new_first_sample_in_chunk = *first_sample_in_chunk;
		sp->entry_no = i;
	    }
	    break;
	} else if(sample_number >= total_sample_num) {
	    //should not exceed the total num of samples though, might
	    //want to add that check
	    if(num_chunks == 0) {
		chunk_num = first_chunk +
		    ((sample_number-total_prev)/num_sample_chnk);
	    } else {
		chunk_num = i;
	    }
	}
	if(num_chunks){
		trak->stbl->stsc_ctxt.prev_offset += 3*sizeof(uint32_t);
		trak->stbl->stsc_ctxt.current_totsam = total_sample_num;
	}
	
    }

    if(chunk_num<1) {
	chunk_num=1;
    }
    ioh.ioh_close((void*)(bs));
    return chunk_num;
 error:
    if (bs) {
	ioh.ioh_close((void*)(bs));
    }
    return -1;
}

#else

int32_t
mp4_sample_to_chunk_num(trak_t *trak, int32_t sample_number, int32_t *first_sample_in_chunk, stsc_shift_params_t *sp)
{
    int32_t entry_count,i,total_sample_num,starting_sample_num,
	next_chunk,num_chunks,num_sample_chnk,chunk_num,
	first_chunk,total_prev, dummy2;
    bitstream_state_t *bs;

    /*initialization */
    bs = NULL;
    entry_count = 0;
    total_sample_num = 0;
    i = 0;
    chunk_num = 0;

    bs = ioh.ioh_open((char*)(trak->stbl->stsc_data), "rb", trak->size);

    /* skip the extended box size */
    ioh.ioh_seek((void*)(bs), 12, SEEK_SET);
	
    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);

    for(i=0; i<entry_count; i++) {
	total_prev = total_sample_num;
	starting_sample_num = total_sample_num;

	/* read curr stsc entry */
	mp4_get_stsc_details(bs, i, &first_chunk, &num_sample_chnk, &dummy2);
	first_chunk = nkn_vpe_swap_uint32(first_chunk);
	num_sample_chnk = nkn_vpe_swap_uint32(num_sample_chnk);

	if(i!=entry_count-1){
	    int32_t dummy;

	    /* read next stsc entry */
	    mp4_get_stsc_details(bs, i + 1, &next_chunk, &dummy, &dummy2);
	    next_chunk = nkn_vpe_swap_uint32(next_chunk);
	    num_chunks = next_chunk-first_chunk;
	} else {
	  if(trak->stbl->stco_data == NULL)
	    num_chunks= (mp4_get_co64_entry_count(trak) - first_chunk) + 1;
	  else
	    num_chunks= (mp4_get_stco_entry_count(trak) - first_chunk) + 1;
	}

	/* save start sample number of each run */
	*first_sample_in_chunk = total_sample_num;

	total_sample_num+=num_chunks*num_sample_chnk;
	if(sample_number<total_sample_num){
	    chunk_num = first_chunk + ((sample_number-total_prev)/num_sample_chnk);
	    *first_sample_in_chunk = *first_sample_in_chunk + ( (chunk_num - first_chunk) * num_sample_chnk);
	    if(sp) {
		sp->new_samples_per_chunk = num_sample_chnk;
		sp->new_first_sample_in_chunk = *first_sample_in_chunk;
		sp->entry_no = i;
	    }
	    i=entry_count;
	} else if(sample_number >= total_sample_num) {
	    //should not exceed the total num of samples though, might
	    //want to add that check
	    if(num_chunks == 0) {
		chunk_num = first_chunk +
		    ((sample_number-total_prev)/num_sample_chnk);
	    } else {
		chunk_num = i;
	    }
	}
    }

    if(chunk_num<1) {
	chunk_num=1;
    }
    ioh.ioh_close((void*)(bs));
    return chunk_num;
}
#endif

int32_t
mp4_get_stsc_details(bitstream_state_t *bs, int32_t sample_num, int32_t *chunk_num, int32_t *samples_per_chunk, int32_t *sample_description)
{
    int32_t offset;
    size_t curr_pos;
	
    /*initialization */
    offset = 0;
    curr_pos = ioh.ioh_tell((void*)(bs));

    offset = 16 + (12 * sample_num);
    ioh.ioh_seek((void*)(bs), offset, SEEK_SET);
    ioh.ioh_read(chunk_num, 1, 4, (void*)(bs));
    ioh.ioh_read(samples_per_chunk, 1, 4, (void*)(bs));
    ioh.ioh_read(sample_description, 1, 4, (void*)(bs));
    /*not reading the sample description index */

    /* restore the io descriptor to original offset */
    ioh.ioh_seek((void*)(bs), curr_pos, SEEK_SET);

    return 0;
}

int32_t
mp4_find_nearest_sync_sample(trak_t *trak, int32_t sample_num, stss_shift_params_t *sp)
{
    int32_t i,entry_count,smpl_no, prev_smpl_no, box_size;
    bitstream_state_t *bs;

    /*initialization */
    smpl_no= 1;
    prev_smpl_no = 1;
    entry_count = 0;
    bs = NULL;	

    if(!trak->stbl->stss_data) {
	/* all samples are seek samples */
	return sample_num;
    }

    /*initialize to a bitstream */
    bs = ioh.ioh_open((char*)trak->stbl->stss_data, "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }

    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 12) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count <= 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 4 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }

    for(i=0;i<entry_count;i++){
	ioh.ioh_read(&smpl_no, 4, 1, (void*)(bs));
	smpl_no = nkn_vpe_swap_uint32(smpl_no);
	if(sample_num<=smpl_no)
	    break;
	prev_smpl_no=smpl_no;
    }

    ioh.ioh_close(bs);
    if(sp) {
	sp->entry_no = i;
    }
    if(sample_num == smpl_no) { 
	return smpl_no - 1;
    } else {
	return prev_smpl_no - 1;
    }
 error:
    if (bs) {
	ioh.ioh_close(bs);
    }
    return -1;
}

int32_t
mp4_timestamp_to_sample(trak_t *trak, int32_t ts, stts_shift_params_t *sp)
{
    int32_t entry_count,delta,i, box_size;
    int32_t t,maxtime,sample_num,sample_count, sample_run;
    bitstream_state_t *bs;
	
    /*initialization */
    bs = NULL;
    maxtime = t = 0;
    sample_num = sample_count = sample_run = 0;
	
    /*initialize to a bitstream */
    bs = ioh.ioh_open((char*)trak->stbl->stts_data, "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }

    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 16) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count <= 0) {
        /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 8 + 12 + 4) > box_size) {
        /* corrupted MP4 */
        goto error;
    }

    /* search for sample number for timestamp */
    t = ts;
    for(i=0; i<entry_count; i++){
	ioh.ioh_read(&sample_count, 1, sizeof(int32_t), (void*)(bs));
	sample_count = nkn_vpe_swap_uint32(sample_count);
		
	ioh.ioh_read(&delta, 4, 1, (void*)(bs));
	delta = nkn_vpe_swap_uint32(delta);

	maxtime+=sample_count*delta;
	if(t<maxtime){
	    maxtime-=sample_count*delta;
	    sample_num+=((t-maxtime)/delta);
	    if(sample_num==0)
		sample_num=1;

	    /* fill shifting details for seek */
	    if(sp) {
		sp->entry_no = i;
		sp->new_delta = delta;
		sp->new_num_samples = sample_count - (sample_num-sample_run);
	    }
	    sample_run = sample_count;		
	    break;
	}
	sample_num+=sample_count;
    }
	
    ioh.ioh_close((void*)(bs));
    return sample_num;

 error:
    if (bs) {
	ioh.ioh_close((void*)(bs));
    }
    return -1;
}

int32_t
mp4_get_stts_shift_offset(trak_t *trak, int32_t sample, stts_shift_params_t *sp)
{
    int32_t entry_count,delta,i, box_size;
    int32_t t,maxtime,sample_num,sample_count, sample_run;
    bitstream_state_t *bs;
	
    /*initialization */
    bs = NULL;
    maxtime = t = 0;
    sample_num = sample_count = sample_run = 0;
	
    /*initialize to a bitstream */
    bs = ioh.ioh_open((char*)trak->stbl->stts_data, "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 16) {
	/* corrupted MP4
	 * and we also do not support long size of this box
	 */
	goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count < 0) { /* corrupted MP4 */
	goto error;
    }
    if ((entry_count * 8 + 12 + 4) > box_size) { /* corrupted MP4 */
	goto error;
    }
    /* search for sample number for timestamp */
    t = sample;
    for(i=0; i<entry_count; i++){
	ioh.ioh_read(&sample_count, 1, sizeof(int32_t), (void*)(bs));
	sample_count = nkn_vpe_swap_uint32(sample_count);
		
	ioh.ioh_read(&delta, 4, 1, (void*)(bs));
	delta = nkn_vpe_swap_uint32(delta);
		
	sample_num += sample_count;
	if(t < sample_num){
	    /* fill shifting details for seek */
	    if(sp) {
		sp->entry_no = i;
		sp->new_delta = delta;
		sp->new_num_samples = sample_count - (t - sample_run);
	    }
					
	    break;
	}
	sample_run = sample_num;
    }
	
    ioh.ioh_close((void*)(bs));
    return 0;

 error:
    if (bs) {
	ioh.ioh_close((void*)(bs));
    }
    return -1;
}

int32_t
mp4_get_ctts_shift_offset(trak_t *trak, int32_t sample, ctts_shift_params_t *sp)
{
    int32_t entry_count,delta,i, box_size;
    int32_t t,maxtime,sample_num,sample_count, sample_run;
    bitstream_state_t *bs;
	
    /*initialization */
    bs = NULL;
    maxtime = t = 0;
    sample_num = sample_count = sample_run = 0;

    /* if ctts table doesnt exist do nothing */
    if(!trak->stbl->ctts_data) {
	return 0;
    }

    /*initialize to a bitstream */
    bs = ioh.ioh_open((char*)trak->stbl->ctts_data, "rb", trak->size);
    if (bs == NULL) {
	goto error;
    }
    /* read the box size */
    ioh.ioh_read(&box_size, 4, 1, (void*)(bs));
    box_size = nkn_vpe_swap_uint32(box_size);
    if (box_size < 16) {
        /* corrupted MP4
         * and we also do not support long size of this box
         */
        goto error;
    }
    /* skip the rest part of the extend box header*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);
	
    /* read num of entries */
    ioh.ioh_read(&entry_count, 1, sizeof(int32_t), (void*)(bs));
    entry_count = nkn_vpe_swap_uint32(entry_count);
    /*
     * add a check
     * 1.entry_count is non-negative
     * 2. size of entry_count*entry_size + header should <= box_size
     */
    if (entry_count < 0) { /* corrupted MP4 */
        goto error;
    }
    if ((entry_count * 8 + 12 + 4) > box_size) { /* corrupted MP4 */
        goto error;
    }
    /* search for sample number for timestamp */
    t = sample;
    for(i=0; i<entry_count; i++){
	ioh.ioh_read(&sample_count, 1, sizeof(int32_t), (void*)(bs));
	sample_count = nkn_vpe_swap_uint32(sample_count);
		
	ioh.ioh_read(&delta, 4, 1, (void*)(bs));
	delta = nkn_vpe_swap_uint32(delta);
		
	sample_num += sample_count;
	if(t <= sample_num){
	    /* fill shifting details for seek */
	    if(sp) {
		sp->entry_no = i;
		sp->new_delta = delta;
		sp->new_num_samples = sample_count - (t - sample_run) + 1;
	    }
					
	    break;
	}
	sample_run = sample_num;
    }
	
    ioh.ioh_close((void*)(bs));
    return 0;
 error:
    if (bs) {
        ioh.ioh_close((void*)(bs));
    }
    return -1;
}

trak_t*
mp4_read_trak(uint8_t *p_data, size_t pos, size_t size, bitstream_state_t *desc) 
{
    trak_t *trak;
    int32_t found, *type;
    uint8_t *p_trak;
    size_t box_pos;
    box_t box;
    bitstream_state_t *trak_desc;

    found = 0;
    trak = NULL;
    p_trak = p_data;
    type = NULL;
    trak_desc = NULL;
    box_pos = 0;
    memset(&box, 0, sizeof(box_t));

    trak = (trak_t*)mp4_init_parent(p_data + pos, pos, size, TRAK_ID);
    if (!trak) {
	goto error;
    }
    trak_desc = desc;//ioh.ioh_open(p_trak, "rb", size);

    /* move to the correct offset */
    ioh.ioh_seek(trak_desc, pos, SEEK_SET);

    /* skip the current box */
    ioh.ioh_seek(trak_desc, 8, SEEK_CUR);

    do {
	/* read the next box */
	mp4_read_next_box(trak_desc, &box_pos, &box);
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
            /* corrupt MP4 file, and we do not support long size in moov*/
            goto error;
	}
	if(is_container_box(&box)) {
	    ioh.ioh_seek(trak_desc, 8, SEEK_CUR);
	} else {
	    ioh.ioh_seek(trak_desc, box.short_size, SEEK_CUR);
	}

	/* read if there is an associated reader for that box */		
	type = (int32_t*)(&box.type);
	switch(*type) {
	    case TKHD_ID:
		trak->tkhd = tkhd_reader(p_data + box_pos, box.short_size, box.short_size);
		if (trak->tkhd == NULL) {
		    goto error;
		}
		trak->tkhd_pos = box_pos;
		trak->tkhd_size = box.short_size;
		trak->tkhd_data = p_data + pos;
		break;
	    case MDIA_ID:
		trak->mdia = mp4_init_parent(p_data + box_pos, box_pos, box.short_size, MDIA_ID);
		if (trak->mdia == NULL) {
		    goto error;
		}
		break;
	    case MDHD_ID:
		if (trak->mdia == NULL) {
		    goto error;
		}
		trak->mdia->mdhd = mdhd_reader(p_data + box_pos, box.short_size, box.short_size);
		if (trak->mdia->mdhd == NULL) {
		    goto error;
		}
		trak->mdia->mdhd_pos = box_pos;
		trak->mdia->mdhd_size = box.short_size;
		trak->mdia->mdhd_data = p_data + pos;
		break;
	    case HDLR_ID:
		if (trak->mdia == NULL) {
		    goto error;
		}
		trak->mdia->hdlr = hdlr_reader(p_data + box_pos, box.short_size, box.short_size);
		if (trak->mdia->hdlr == NULL) {
		    goto error;
		}
		break;
	    case STBL_ID:
		trak->stbl = mp4_init_parent(p_data + box_pos, box_pos, box.short_size, STBL_ID);
		if (trak->stbl == NULL) {
		    goto error;
		}
		break;
	    case STTS_ID:
		if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->stts_data = p_data + box_pos;
		break;
	    case STSC_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->stsc_data = p_data + box_pos;
		break;
	    case STCO_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->stco_data = p_data + box_pos;
		break;
	    case CO64_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
	        trak->stbl->co64_data = p_data + box_pos;
	        break;
	    case STSZ_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->stsz_data = p_data + box_pos;
		break;
	    case STSS_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->stss_data = p_data + box_pos;
		break;
	    case CTTS_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->ctts_data = p_data + box_pos;
		break;
	    case STSD_ID:
                if (trak->stbl == NULL) {
		    goto error;
		}
		trak->stbl->stsd_data = p_data + box_pos;
		break;
	}

    }while( (ioh.ioh_tell((void*)(trak_desc))) < (size + pos) );

    /*reset the pointers */
    if ((pos + size) < trak_desc->size)
	ioh.ioh_seek((void *)(trak_desc), pos + size, SEEK_SET);
    else /* if array a[256], we should visit a[255], not a[256] */
	ioh.ioh_seek((void *)(trak_desc), trak_desc->size - 1, SEEK_SET);

    return trak;
 error:
    mp4_trak_cleanup(trak);
    return NULL;
}

int32_t 
mp4_validate_trak(trak_t *trak)
{
    if (trak->tkhd == NULL) {
        return -1;
    }

    if (trak->mdia == NULL) {
        return -1;
    }
    if (trak->mdia->mdhd == NULL) {
	return -1;
    }
    if (trak->mdia->hdlr == NULL) {
        return -1;
    }

    if (trak->stbl == NULL) {
	return -1;
    }
    if(!(trak->stbl->stts_data &&
	 trak->stbl->stco_data &&
	 trak->stbl->stsc_data &&
	 trak->stbl->stsz_data)) {
	return -1;
    }

    return 0;
}

int32_t 
mp4_trak_get_codec(trak_t *trak)
{
    stsd_t *stsd;
    int32_t rv;

    /* BITFIELD: 
     * Most significant 4 bytes can be 0/1
     * 1 - video
     * 2 - Audio
     * Lower significant 4 bytes give the codec id
     */
    rv = 0;

    /* read stsd box */
    stsd = mp4_read_stsd_box(trak->stbl->stsd_data, trak->size);

    switch( trak->mdia->hdlr->handler_type ) {
	case VIDE_HEX:
	    {
		mp4_visual_sample_entry_t *vse;
		const char video_codec_identifiers[][4] = {
		    {'a','v','c','1'},
		    {'m','p','4','v'}};

		vse =
		    mp4_read_visual_sample_entry(stsd->p_sample_entry,
						 stsd->sample_entry_size);
		if (vse == NULL) {
		    return 0;
		}
		if ( !memcmp(vse->se.box.type,
			     video_codec_identifiers[0], 4) ) { //AVC
		    rv = 0x00010001;
		} else if ( !memcmp( vse->se.box.type,
				     video_codec_identifiers[1], 4) ) {
		    //MPEG 4 part 2
		    rv = 0x00010003;
		}
		
		mp4_cleanup_visual_sample_entry(vse);
	    }
			
	    break;
	case SOUN_HEX:
	    {
		mp4_audio_sample_entry_t *ase;
		const char audio_codec_identifiers[][4] = {
		    {'m','p','4','a'} };
		ase =
		    mp4_read_audio_sample_entry(stsd->p_sample_entry,
						stsd->sample_entry_size);
		if (ase == NULL) {
		    return 0;
		}
		if ( !memcmp(ase->se.box.type, 
			     audio_codec_identifiers[0], 
			     4) ) { //AAC
		    rv = 0x00020001;
		} 
		
		mp4_cleanup_audio_sample_entry(ase);
	    }
	    break;
    }

    mp4_cleanup_stsd(stsd);
    return rv;
}

mp4_visual_sample_entry_t* 
mp4_read_visual_sample_entry(uint8_t* data, size_t size)
{
    bitstream_state_t *bs;
    mp4_visual_sample_entry_t *vse;
    size_t box_pos;
    
    /* allocate for an audio sample entry */
    vse = (mp4_visual_sample_entry_t*)nkn_calloc_type(1,
        		      sizeof(mp4_visual_sample_entry_t),
			       mod_vpe_mp4_vse_t);

    if (!vse) {
	goto error;
    }
    /* open the bitstream */
    bs = ioh.ioh_open((char*)data, "rb", size);
    if (bs == NULL) {
	goto error;
    }
    /* read the box */
    mp4_read_next_box(bs, &box_pos, &vse->se.box);
    if (vse->se.box.short_size == 0 ||
	(vse->se.box.short_size == 1 && vse->se.box.long_size == 0)) {
	/* corrupt MP4 file*/
	goto error;
    }
    ioh.ioh_seek((void*)bs, 8, SEEK_CUR);

    /* read the sample entry container */
    ioh.ioh_read(&vse->se.reserved1, 1, 6, (void*)bs);
    ioh.ioh_read(&vse->se.data_ref_index, 1, 2, (void*)bs);
    vse->se.data_ref_index =
	nkn_vpe_swap_uint16(vse->se.data_ref_index);

    /* read the visual sample entry box */
    ioh.ioh_read(&vse->pre_defined1, 1, 2, (void*)bs);
    ioh.ioh_read(&vse->reserved1, 1, 2, (void*)bs);
    ioh.ioh_read(&vse->pre_defined2, 1, 12, (void*)bs);
    ioh.ioh_read(&vse->width, 1, 2, (void*)bs);
    vse->width = nkn_vpe_swap_uint16(vse->width);
    ioh.ioh_read(&vse->height, 1, 2, (void*)bs);
    vse->height = nkn_vpe_swap_uint16(vse->height);
    ioh.ioh_read(&vse->horizontal_resolution, 1, 4, (void*)bs);
    ioh.ioh_read(&vse->vertical_resolution, 1, 4, (void*)bs);
    ioh.ioh_read(&vse->reserved2, 1, 4, (void*)bs);
    ioh.ioh_read(&vse->frame_count, 1, 2, (void*)bs);
    ioh.ioh_read(&vse->compressor_name, 1, 4, (void*)bs);
    ioh.ioh_read(&vse->depth, 1, 2, (void*)bs);
    ioh.ioh_read(&vse->pre_defined3, 1, 2, (void*)bs);
    
    /* close bitstream */
    ioh.ioh_close((void*)bs);

    return vse;
 error:
    if (vse) {
	free(vse);
    }
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    return NULL;
}

void
mp4_cleanup_visual_sample_entry(mp4_visual_sample_entry_t *vse)
{
    if( vse ) {
	free(vse);
    }

}

mp4_audio_sample_entry_t * 
mp4_read_audio_sample_entry(uint8_t *data, size_t size)
{
    bitstream_state_t *bs;
    mp4_audio_sample_entry_t *ase;
    size_t box_pos;

    /* allocate for an audio sample entry */
    ase = (mp4_audio_sample_entry_t*)nkn_calloc_type(1,
			  sizeof(mp4_audio_sample_entry_t),
			  mod_vpe_mp4_ase_t);
    if (!ase) {
	goto error;
    }
    /* open the bitstream */
    bs = ioh.ioh_open((char*)data, "rb", size);
    if (bs == NULL) {
	goto error;
    }
    /* read the box */
    mp4_read_next_box(bs, &box_pos, &ase->se.box);
    if (ase->se.box.short_size == 0 ||
	(ase->se.box.short_size == 1 && ase->se.box.long_size == 0)) {
        /* corrupt MP4 file*/
        goto error;
    }
    ioh.ioh_seek((void*)bs, 8, SEEK_CUR);
    
    /* read the sample entry container */
    ioh.ioh_read(&ase->se.reserved1, 1, 6, (void*)bs);
    ioh.ioh_read(&ase->se.data_ref_index, 1, 2, (void*)bs);
    ase->se.data_ref_index =
	nkn_vpe_swap_uint16(ase->se.data_ref_index);


    /* read the audio sample entry */
    ioh.ioh_read(&ase->reserved1, 1, 8, (void*)bs);
    ioh.ioh_read(&ase->channel_count, 1, 2, (void*)bs);
    ase->channel_count = nkn_vpe_swap_uint16(ase->channel_count);
    ioh.ioh_read(&ase->sample_size, 1, 2, (void*)bs);
    ase->sample_size = nkn_vpe_swap_uint16(ase->sample_size);
    ioh.ioh_read(&ase->pre_defined, 1, 2, (void*)bs);
    ioh.ioh_read(&ase->reserved2, 1, 2, (void*)bs);
    
    /* close bitstream */
    ioh.ioh_close((void*)bs);
    
    return ase;
 error:
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    if (ase) {
	free(ase);
    }
    return NULL;
}

void
mp4_cleanup_audio_sample_entry(mp4_audio_sample_entry_t *ase)
{
    if(ase) {
	free(ase);
    }
}

stsd_t *
mp4_read_stsd_box(uint8_t* data, size_t size) 
{
    bitstream_state_t *bs;
    stsd_t *stsd;
    uint32_t box_size;
    
    stsd = (stsd_t*)nkn_calloc_type(1, sizeof(stsd_t),
				    mod_vpe_mp4_stsd_t);
    if ( !stsd ) {
	return NULL;
    }

    box_size = 0;
    bs = ioh.ioh_open((char*)data, "rb", size);

    /* read box size */
    ioh.ioh_read(&box_size, 1, sizeof(uint32_t), (void*)bs);
    box_size = nkn_vpe_swap_uint32(box_size);

    /* skip to the start of the box data */
    ioh.ioh_seek((void*)bs, 12, SEEK_SET);

    /* read entry count */
    ioh.ioh_read(&stsd->entry_count, 1, sizeof(uint32_t), (void*)bs);
    stsd->entry_count = nkn_vpe_swap_uint32(stsd->entry_count);
    
    /* store the start of the sample entry */
    stsd->p_sample_entry = data + ioh.ioh_tell((void*)bs);
    stsd->sample_entry_size = box_size - 16;

    ioh.ioh_close((void*)bs);
    return stsd;
}

void
mp4_cleanup_stsd(stsd_t *stsd)
{
    if( stsd ) {
	free(stsd);
    }
}
	
size_t 
mp4_copy_box(bitstream_state_t *bs, bitstream_state_t *bs_out, box_t *box)
{
    uint8_t *box_data;
    size_t bcheck_bytes_left = 0;

    bcheck_bytes_left = \
	bio_get_remaining_bytes(bs_out);
    if (box->short_size > bcheck_bytes_left) {
	return 0;
    }
    box_data = (uint8_t*)nkn_malloc_type(box->short_size,
					 mod_vpe_mp4_box_data);
    if (!box_data) {
	return 0;
    }
    ioh.ioh_read((void*)(box_data), 1, box->short_size, (void*)(bs));
    ioh.ioh_write((void*)(box_data), 1, box->short_size, (void*)(bs_out));
    ioh.ioh_seek((void*)bs, -((int32_t)box->short_size), SEEK_CUR);
    ioh.ioh_seek((void*)bs_out, -((int32_t)box->short_size), SEEK_CUR);
    free(box_data);	
    
    return box->short_size;
}

void *
mp4_init_parent(uint8_t* p_data, size_t pos, size_t size, int32_t box_id)
{
    void *ptr;

    ptr = NULL;

    switch(box_id) {
	case MOOV_ID:
	    ptr = (void *)mp4_init_moov(p_data, pos, size);
	    break;
	case TRAK_ID:
	    ptr = (void*)mp4_init_trak(p_data, pos, size);
	    break;
	case MDIA_ID:
	    ptr = (void*)mp4_init_mdia(p_data, pos, size);
	    break;
	case STBL_ID:
	    ptr = (void*)mp4_init_stbl(p_data, pos, size);
	    break;
	case UDTA_ID:
	    ptr = (void*)mp4_init_udta(p_data, pos, size);
	    break;
	case MFRA_ID:
	    ptr = (void*)mp4_init_mfra(p_data, pos, size);
	    break;
    }
	
    return ptr;
}

mfra_t* 
mp4_init_mfra(uint8_t *p_data, size_t off, size_t size) 
{
    mfra_t *mfra;

    mfra = (mfra_t*)nkn_calloc_type(1, sizeof(mfra_t),
				    mod_vpe_mp4_mfra_t);
    if (!mfra) {
	return NULL;
    }
    mfra->pos = off;
    mfra->data = p_data;
    mfra->size = size;

    return mfra;

}

udta_t*
mp4_init_udta(uint8_t *p_data, size_t off, size_t size)
{
    udta_t *udta;

    udta = (udta_t*)nkn_calloc_type(1, sizeof(udta_t),
				    mod_vpe_mp4_udta_t);
    if (!udta) {
	return NULL;
    }
    udta->pos = off;
    udta->data = p_data;
    udta->size = size;

    return udta;
}

stbl_t *
mp4_init_stbl(uint8_t *p_data, size_t off, size_t size)
{
    stbl_t *stbl;

    /*initialization */
    stbl = (stbl_t*)nkn_calloc_type(1, sizeof(stbl_t),
				    mod_vpe_mp4_stbl_t);
    if (!stbl) {
	return NULL;
    }
    stbl->pos = off;
    stbl->data = p_data;
    stbl->size = size;

    return stbl;
}

moov_t *
mp4_init_moov(uint8_t *p_data, size_t off, size_t size)
{
    moov_t *moov;

    moov = (moov_t *)nkn_calloc_type(1, sizeof(moov_t), 
				     mod_vpe_mp4_moov_t);
	
    if(!moov) {
	return NULL;
    }

    moov->pos = off;
    moov->data = p_data;
    moov->size = size;

    return moov;
}

trak_t *
mp4_init_trak(uint8_t *p_data, size_t pos, size_t size)
{
   trak_t *trak = (trak_t*)nkn_calloc_type(1, sizeof(trak_t),
					   mod_vpe_mp4_trak_t);

    if(!trak) {
	return NULL;
    }

    trak->data = p_data;
    trak->pos = pos;
    trak->size = size;

    return trak;
}

mdia_t*
mp4_init_mdia(uint8_t *p_data, size_t pos, size_t size)
{

    mdia_t *mdia = (mdia_t*)nkn_calloc_type(1, sizeof(mdia_t),
					    mod_vpe_mp4_mdia_t);
    if(!mdia) {
	return NULL;
    }

    mdia->pos = pos;
    mdia->data = p_data;
    mdia->size = size;
    return mdia;
}

int32_t
mp4_find_child_box(bitstream_state_t *bs, char *child_box_id, size_t *child_box_pos, size_t *child_box_size)
{
    size_t skip_size;
    int32_t box_id, found;

    /* initialization */
    skip_size = box_id = found = 0;

    /*move past the parent box*/
    ioh.ioh_seek((void*)(bs), 8, SEEK_CUR);

    do {
	/* read curr box size */
	ioh.ioh_read(&skip_size, 1, 4, (void*)(bs));
	skip_size = nkn_vpe_swap_uint32(skip_size);

	/* read curr box id */
	ioh.ioh_read(&box_id, 1, 4, (void*)(bs));
	
	/* check if it matches the child */
	if(!(memcmp(child_box_id, &box_id, 4))) {
	    /* rewind to begining of the box */
	    ioh.ioh_seek((void*)(bs), -8, SEEK_CUR);
	    *child_box_size = skip_size;
	    *child_box_pos = ioh.ioh_tell((void*)(bs));
	    found = 1;
	}

	/*skip to next box */
	ioh.ioh_seek((void*)(bs), (skip_size - 8), SEEK_CUR);
    } while( (!found) && (ioh.ioh_tell((void*)(bs))) );

    return 0;
}

int32_t
mp4_read_next_box(bitstream_state_t *bs, size_t *child_box_pos, box_t *box)
{
    size_t skip_size;
    int32_t box_id, found;

    /* initialization */
    skip_size = box_id = found = 0;

    /* read curr box size */
    ioh.ioh_read(&skip_size, 1, 4, (void*)(bs));
    skip_size = nkn_vpe_swap_uint32(skip_size);
	
    /* read curr box id */
    ioh.ioh_read(&box_id, 1, 4, (void*)(bs));
	
    ioh.ioh_seek((void*)(bs), -8, SEEK_CUR);
    box->short_size = skip_size;
    *child_box_pos = ioh.ioh_tell((void*)(bs));
    memcpy(&box->type, &box_id, 4);

    return 0;
}


int32_t
mp4_yt_adjust_ilst_props(moov_t *moov, double seek_time, void *iod)
{
    bitstream_state_t *bs, *bs_out;
    size_t bytes_parsed;//, dst_size;
    int32_t tag_size, rv;
    uint32_t tag_name;
    uint8_t *box_hdr;
    const uint32_t gsst_id = 0x67737374;

    rv = -1;
    bytes_parsed = 0;
    bs = ioh.ioh_open((char*)(moov->udta->ilst_data), "rb", moov->udta->ilst_size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = (bitstream_state_t*)(iod);

    /* allocate resources for the new ilst box */
    //dst_size = moov->udta->ilst_size;
    //p_dst = (uint8_t*)malloc(dst_size);

    /* skip the box name */
    ioh.ioh_read((void*)(&box_hdr), 1, 8, (void*)bs);
    ioh.ioh_write((void*)(&box_hdr), 1, 8, (void*)(bs_out));

    do {
	/* read and copy tag size */
	ioh.ioh_read((void*)(&tag_size), 1, sizeof(int32_t), (void*)(bs));
	ioh.ioh_write((void*)(&tag_size), 1, sizeof(int32_t), (void*)(bs_out));
	tag_size = nkn_vpe_swap_uint32(tag_size);

	/* read and copy tag name */
	ioh.ioh_read((void*)(&tag_name), 1, sizeof(int32_t), (void*)(bs));
	ioh.ioh_write((void*)(&tag_name), 1, sizeof(int32_t), (void*)(bs_out));
	tag_name = nkn_vpe_swap_uint32(tag_name);

	if(tag_name == gsst_id) {
	    /* need to modify this box with the right value */
	    int64_t st_time;
	    int32_t ndigits;
	    int32_t gsst_len;
	    char *st_time_str,format_str[3+1];
	    int32_t data_size, data_tag_id, data_type_id;
	    uint32_t tmp, src_data_size;

	    st_time = 1000*seek_time;
	    if (st_time == 0) {
		ndigits = 1;
	    } else {
		ndigits = ceil(log10(st_time));
	    }
	    gsst_len = 4 + ndigits;
	    st_time_str = (char *)alloca(gsst_len + 1);
	    memset(st_time_str, 0, gsst_len+1);

	    /* read & copy data tag size */
	    ioh.ioh_read((void*)(&data_size), 1, sizeof(int32_t), (void*)(bs));
	    src_data_size = tmp = nkn_vpe_swap_uint32(data_size);
	    tmp = tmp + (rv = (gsst_len - (tmp - 12)));
	    data_size = nkn_vpe_swap_uint32(tmp);
	    ioh.ioh_write((void*)(&data_size), 1, sizeof(int32_t), (void*)(bs_out));

	    /* read & copy data tag */
	    ioh.ioh_read((void*)(&data_tag_id), 1, sizeof(int32_t), (void*)(bs));
	    ioh.ioh_write((void*)(&data_tag_id), 1, sizeof(int32_t), (void*)(bs_out));

	    /* read & copy data tag */
	    ioh.ioh_read((void*)(&data_type_id), 1, sizeof(int32_t), (void*)(bs));
	    ioh.ioh_write((void*)(&data_type_id), 1, sizeof(int32_t), (void*)(bs_out));

	    /* skip reading the data val */
	    ioh.ioh_seek((void*)(bs), src_data_size - 12, SEEK_CUR);
	    snprintf(format_str, 4, "%%%dd", ndigits);
	    snprintf(st_time_str + (gsst_len - ndigits), ndigits+1, format_str, st_time);
	    ioh.ioh_write((void*)(st_time_str), 1, gsst_len, (void*)(bs_out));
	} else {
	    /* read & copy rest of the box */
	    uint8_t *tag_data;
	    tag_data = (uint8_t*)nkn_malloc_type(tag_size-8, mod_vpe_mp4_generic_buf);
	    if (!tag_data) {
		goto error;
	    }
	    ioh.ioh_read((void*)(tag_data), 1, tag_size-8, (void*)bs);
	    ioh.ioh_write((void*)(tag_data), 1, tag_size-8, (void*)bs_out);
	    free(tag_data);
	    //	    ioh.ioh_seek((void*)(bs), (tag_size - 8), SEEK_CUR);
	}

	bytes_parsed = ioh.ioh_tell((void*)(bs));
    }while(bytes_parsed < moov->udta->ilst_size);

    ioh.ioh_close((void*)bs);
    return rv;
 error:
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    return INT32_MIN;
}

/**
 * Modifies the ilst box per DailyMotion's requirements
 * Dailymotion requires the seek provider to add the following ilst
 * tags to the existing tag list; note these tags maybe absent in the
 * original and need to be added afresh. if they are present in the
 * original we will retain the ctoo tag and replace the rest of the
 * tags with the tag list shown below.
 * a. pdur - total duration of the video as text
 * b. coff - the seek offset of the video as text
 * c. pbyt - total size of the video as text
 * d. cbyt - the seeked size of the video as text
 * @param moov - structure containing the moov box
 * @param seek_time - seek time stamp
 * @param seek_provider_specific_data - any other data that is
 * specific to the provider required to create the tags
 * @param iod - io descriptor for the output buffer (new ilist with
 * modified tags
 * @return - returns the difference of new ilist with old ilst 
 *
 */
int32_t
mp4_dm_adjust_ilst_props(moov_t *moov, double seek_time,
			 void *seek_provider_specific_data, 
			 void *iod)
{
    bitstream_state_t *bs, *bs_out;
    size_t bytes_parsed;//, dst_size;
    int32_t tag_size, rv = 0;
    uint32_t tag_name, i, ndigits_1, ndigits_2;
    size_t est_udta_size_with_cbyt, udta_size_wo_cbyt;
    uint8_t *box_hdr, found = 0;
    dm_seek_input_t *dm_si;
    uint32_t dm_ilst_tag_list[] = {0x72756470 /* pdur */,
				   0x66666f63 /* coff */,
				   0x74796270 /* pbyt */,
				   0x74796263 /* cbyt */};

    bytes_parsed = 0;
    bs = ioh.ioh_open((char*)(moov->udta->ilst_data), "rb", moov->udta->ilst_size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = (bitstream_state_t*)(iod);
    dm_si = (dm_seek_input_t*)seek_provider_specific_data;

    rv = ioh.ioh_tell((void *)bs_out);

    /* skip the box name */
    ioh.ioh_read((void*)(&box_hdr), 1, 8, (void*)bs);
    ioh.ioh_write((void*)(&box_hdr), 1, 8, (void*)(bs_out));

    do {
	/* read and copy tag size */
	ioh.ioh_read((void*)(&tag_size), 1, sizeof(int32_t), (void*)(bs));
	ioh.ioh_write((void*)(&tag_size), 1, sizeof(int32_t), (void*)(bs_out));
	tag_size = nkn_vpe_swap_uint32(tag_size);

	/* read and copy tag name */
	ioh.ioh_read((void*)(&tag_name), 1, sizeof(int32_t), (void*)(bs));
	ioh.ioh_write((void*)(&tag_name), 1, sizeof(int32_t), (void*)(bs_out));
	tag_name = nkn_vpe_swap_uint32(tag_name);

	/* read & copy rest of the box */
	uint8_t *tag_data;
	tag_data = (uint8_t*)nkn_malloc_type(tag_size-8, mod_vpe_mp4_generic_buf);
	if (!tag_data) {
	    goto error; /* this is an impossible value, use it as error code */
	}
	ioh.ioh_read((void*)(tag_data), 1, tag_size-8, (void*)bs);
	ioh.ioh_write((void*)(tag_data), 1, tag_size-8, (void*)bs_out);
	free(tag_data);

	bytes_parsed = ioh.ioh_tell((void*)(bs));
	if (tag_name == 0xa9746f6f) {
	    found = 1;
	}
    }while(bytes_parsed < moov->udta->ilst_size && !found);

    for(i = 0; i < sizeof(dm_ilst_tag_list) / sizeof(dm_ilst_tag_list[0]);
	i++) {
	switch (dm_ilst_tag_list[i]) {
	    case 0x72756470:
		mp4_write_ilst_text_type((void *)(&dm_si->duration),
					 0, bs_out, dm_ilst_tag_list[i]);
		break;
	    case 0x66666f63:
		mp4_write_ilst_text_type((void *)(&seek_time), 0,
					 bs_out, dm_ilst_tag_list[i]);
		break;
	    case 0x74796270:
		mp4_write_ilst_text_type((void *)(&dm_si->tot_file_size),
					 1, bs_out,
					 dm_ilst_tag_list[i]);
		break;
	    case 0x74796263:
		/* new seek file size is
		 * moov_size + udta_size + moov_offset + (mdat_size +
		 * mdat hdr), where
		 * udta_size = meta_size + hdlr_size + mdir_size + ilst_size
		 * where,
		 * ilst_size = pdur_size + coff_size + pbyt_size + cbyt_size
		 */		
		udta_size_wo_cbyt = (ioh.ioh_tell((void *)bs_out) +
				     dm_si->seek_mdat_size +
				     moov->size + dm_si->moov_offset);
		ndigits_1 = 1;
		if (udta_size_wo_cbyt) {
		    ndigits_1 = ceil(log10(udta_size_wo_cbyt));
		}
		est_udta_size_with_cbyt = (udta_size_wo_cbyt +
					   ndigits_1 + 24);
		/* test if the estimated udta size is at a 10^x
		 * boundary. for e.g.
		 * if estimated udta size = 997 bytes
		 * the ndigits will be = 3 bytes
		 * total will be = 1000 bytes
		 * when we write this in cbyt we new ndigits will be =
		 * 4 bytes
		 * therefore, the estimated udta size should
		 * incorporate the new ndigits
		 */
		ndigits_2 = ceil(log10(est_udta_size_with_cbyt));
		if (ndigits_2 != ndigits_1) {
		    est_udta_size_with_cbyt = (udta_size_wo_cbyt +
					       ndigits_2 + 16);
		}
		mp4_write_ilst_text_type(\
					 (void*)(&est_udta_size_with_cbyt), 
					 1, bs_out,
					 dm_ilst_tag_list[i]);
		break;
	}
    }
    
    ioh.ioh_close((void*)bs);

    /* obtain the new ilst size */
    rv = (ioh.ioh_tell((void*)bs_out) - rv);
    /* calculate the size diff of new ilst with old ilst */
    rv = rv - moov->udta->ilst_size;

    return rv;
 error:
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    return INT32_MIN; /* this is an impossible value, use it as error code */
} 

typedef union tag_ilst_text_type{
    double d;
    uint64_t u;
}ilst_text_type_t;

static inline int32_t
mp4_write_ilst_text_type(void *data, uint8_t type, 
			 bitstream_state_t *bs_out,
			 uint32_t tag_name)
{
    uint32_t tag_type;
    uint8_t buf[32]; /* max of 20bytes to hold 2^64 as a string */
    char fmt[32];
    uint32_t buf_size, ndigits, tag_data_len;//, pad_bytes;	
    ilst_text_type_t *tt = (ilst_text_type_t*)data;

    memset(buf, 0, 32);
    ndigits = 1;
    tag_type = 0x01000000;

    switch(type) {
	case 0:
	    if (tt->d) {
		ndigits = ceil(log10(tt->d));
	    }
	    snprintf(fmt, 32, "%%%d.3f", ndigits);
	    snprintf((char*)buf, 32, fmt, tt->d);
	    tag_data_len = ndigits+4;
	    break;
	case 1:
	    if (tt->u) {
		ndigits = ceil(log10(tt->u));
	    }
	    snprintf(fmt, 32, "%%%dd", ndigits);
	    snprintf((char*)buf, 32, fmt, tt->u);
	    tag_data_len = ndigits;
	    break;	    
    }
    buf_size = tag_data_len;
    mp4_write_ilst_tag(bs_out, tag_name,
		       tag_type, buf, buf_size);

    return 0;

}

int32_t 
mp4_write_ilst_tag(bitstream_state_t *bs, uint32_t tag_name,
		   uint32_t type, uint8_t *data, uint32_t data_size)
{
    uint32_t tag_size, tag_record_size;
    const uint32_t data_tag = 0x61746164;
    const uint32_t reserved = 0;

    tag_size = 16 + data_size;
    tag_record_size = tag_size + 8;
    tag_size = nkn_vpe_swap_uint32(tag_size);
    tag_record_size = nkn_vpe_swap_uint32(tag_record_size);
    ioh.ioh_write((void*)(&tag_record_size), 1, 4, bs);
    ioh.ioh_write((void*)(&tag_name), 1, 4, bs);
    ioh.ioh_write((void*)(&tag_size), 1, 4, bs);
    ioh.ioh_write((void*)(&data_tag),1, 4, bs);
    ioh.ioh_write((void*)(&type), 1, 4, bs);
    ioh.ioh_write((void*)(&reserved), 1, 4, bs);
    ioh.ioh_write((void*)(data), 1, data_size, bs);

    return tag_size;
}

double
mp4_get_vid_duration(const moov_t *const moov) 
{
    trak_t *trak;
    double duration = 0.0;
    uint32_t i;
    int32_t trak_type;
    
    for (i = 0; i < moov->n_tracks_to_process; i++) {
	trak = moov->trak[i];
	trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	if (trak_type == VIDE_HEX) {
	    duration = trak->mdia->mdhd->duration/
		trak->mdia->mdhd->timescale;
	    break;
	}
    }

    return duration;
}

int32_t 
mp4_search_box_list(uint8_t *data, size_t data_size,
		    const uint32_t *box_list, uint32_t num_boxes,
		    uint32_t *num_found)
{
    bitstream_state_t *bs;
    int32_t i = 0, err = 0, start_box_found = 0;
    box_t box;
    size_t box_pos, container_size, bytes_parsed;
    uint32_t *type, search_start_box;

    /* should be a container box */
    search_start_box = box_list[0];
    *num_found = 0;
    bs = ioh.ioh_open((char*)(data), "rb", data_size);
    if (bs == NULL) {
	goto error;
    }

    do {
	mp4_read_next_box(bs, &box_pos, &box);
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
	    /* corrupt MP4 file*/
	    goto error;
	}
	type = (uint32_t*)(&box.type);
	if (*type == search_start_box) {
	    start_box_found = 1;
	    (*num_found)++;
	    i++;
	    break;
	}
	ioh.ioh_seek((void*)bs, box.short_size, SEEK_CUR);
	bytes_parsed = ioh.ioh_tell((void*)bs);
    } while(bytes_parsed < data_size);

    if (!start_box_found) {
	goto error;
    }

    while (i < (int32_t)num_boxes && bytes_parsed < data_size) {
	mp4_read_next_box(bs, &box_pos, &box);
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
            /* corrupt MP4 file*/
            goto error;
        }
	type = (uint32_t*)(&box.type);
	if (*type == box_list[i]) {
	    i++;
	    (*num_found)++;
	}
	if (is_container_box(&box)) { 
	    if(*type == META_ID) {
		/* META tag is a full box container */
		container_size = 12;
	    } else { 
		container_size = 8;
	    }
	} else {
	    if (*type == ILST_ID) {
		mp4_search_ilst_tags(bs, box.short_size, 
				     &box_list[i], num_boxes - i,
				     num_found);
	    }
	    container_size = box.short_size;
	}
	ioh.ioh_seek((void*)bs, container_size, SEEK_CUR);
	bytes_parsed = ioh.ioh_tell((void *)bs);
    }
	
    ioh.ioh_close((void*)bs);
    
    return err;
 error:
    *num_found = 0;
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    err = -E_VPE_MP4_SEEK_UNKNOWN_PROVIDER;
    return err;
}

/**
 * the ilst tags are searched in the ilst box. the search order and
 * the order of appearance in the MP4 file should be the same.
 * @param bs1 - the bitsream in which the tags are searched; should be
 * unchanged when returning back to the caller. the data in the
 * bitstream should point to the top of the ilst box
 * @param data_size - length of the ilst box
 * @param tag_list - tag list to be searched; should be in the same
 * order as found in the MP4 file
 * @param n_tags - number of tags
 * @param num_found - number tags from the search list that were found
 * @return always returns 0
 */
int32_t
mp4_search_ilst_tags(const bitstream_state_t *const bs1,
		     size_t data_size,
		     const uint32_t *tag_list,
		     uint32_t n_tags,
		     uint32_t *num_found)
{
    int32_t i = 0; // err = 0,
    uint32_t tag_size, tag_name;
    size_t bytes_parsed = 0, pos = 0;
    bitstream_state_t *bs = (bitstream_state_t*)bs1;

    /* store the current pos to restore at the end */
    pos = ioh.ioh_tell((void *)bs);
    ioh.ioh_seek((void *)(bs), 8, SEEK_CUR);
    
    do {
	
	ioh.ioh_read((void*)(&tag_size), 1, sizeof(int32_t), (void*)(bs));
	tag_size = nkn_vpe_swap_uint32(tag_size);

	/* read and copy tag name */
	ioh.ioh_read((void*)(&tag_name), 1, sizeof(int32_t), (void*)(bs));
	if (tag_name == tag_list[i]) {
	    i++;
	    (*num_found)++;
	}
	
	/* read & copy rest of the box */
	ioh.ioh_seek((void*)(bs), tag_size-8, SEEK_CUR);

	bytes_parsed = ioh.ioh_tell((void*)(bs));

    } while (bytes_parsed < data_size);
	
    /* restore to the old pos */
    ioh.ioh_seek((void *)bs, pos, SEEK_SET);

    return 0;
}

int32_t 
mp4_write_full_box_header(uint8_t *p_buf, uint32_t type, uint32_t box_size, uint32_t flags)
{
    uint32_t tmp;
    
    tmp = nkn_vpe_swap_uint32(box_size);
    memcpy(p_buf, &tmp, 4);
    memcpy(p_buf + 4, &type, 4);
    tmp = nkn_vpe_swap_uint32(flags);
    memcpy(p_buf + 8, &tmp, 4);

    return 12;
}

int32_t 
mp4_write_box_header(uint8_t *p_buf, uint32_t type, uint32_t box_size)
{
    uint32_t tmp;
    
    tmp = nkn_vpe_swap_uint32(box_size);
    memcpy(p_buf, &tmp, 4);
    memcpy(p_buf + 4, &type, 4);

    return 8;
}

#if 0
void
mp4_moov_cleanup(moov_t *moov, int32_t num_traks) 
{
    int32_t i;
	
    for(i = 0;i < num_traks; i++) {
	mp4_trak_cleanup(moov->trak[i]);
    }

    free(moov->mvhd);
    free(moov);
}

void
mp4_trak_cleanup(trak_t *trak)
{
    free(trak->tkhd);
    mp4_cleanup_mdia(trak->mdia);
    free(trak->stbl);
    free(trak);
}

void
mp4_cleanup_mdia(mdia_t *mdia)
{
    free(mdia->hdlr);
    free(mdia->mdhd);

    free(mdia);
}

int32_t
mp4_populate_trak(moov_t *moov, int32_t search_trak_type)
{
    bitstream_state_t *moov_desc,
	*trak_desc,
	*mdia_desc;
    char trak_id[] = {'t','r','a','k'};
    char tkhd_id[] = {'t','k','h','d'};
    char mdia_id[] = {'m','d','i','a'};
    char mdhd_id[] = {'m','d','h','d'};
    char hdlr_id[] = {'h','d','l','r'};
    uint8_t *p_parent, 
	*p_trak, *p_tkhd,
	*p_mdia, *p_mdhd, *p_hdlr;
    int32_t found, trak_no;
    size_t  trak_size, trak_pos,
	tkhd_size, tkhd_pos,
	mdia_size, mdia_pos,
	mdhd_size, mdhd_pos,
	hdlr_size, hdlr_pos;

    /*initialization */
    moov_desc = NULL;
    trak_desc = NULL;
    mdia_desc = NULL;
    p_parent = moov->data;
    trak_size = trak_pos = 0;
    tkhd_size = tkhd_pos = 0;
    mdia_size = mdia_pos = 0;
    mdhd_size = mdhd_pos = 0;
    hdlr_size = hdlr_pos = 0;
    found = 0;
    trak_no = 0;

    moov_desc = ioh.ioh_open(moov->data, "rb", moov->size);

    do {
	mp4_find_child_box(moov_desc, trak_id, &trak_pos, &trak_size);
	moov->trak[trak_no] = mp4_init_trak(p_parent + trak_pos, trak_pos, trak_size);
	p_trak = p_parent + trak_pos;
	trak_desc = ioh.ioh_open(p_trak, "rb", trak_size);
	mp4_find_child_box(trak_desc, tkhd_id, &tkhd_pos, &tkhd_size);
	p_tkhd = p_trak + tkhd_pos;
	moov->trak[trak_no]->tkhd = tkhd_reader(p_tkhd, tkhd_size, 8);
	mp4_find_child_box(trak_desc, mdia_id, &mdia_pos, &mdia_size);
	p_mdia = p_trak + mdia_pos;
	moov->trak[trak_no]->mdia = mp4_init_mdia(p_mdia, mdia_pos, mdia_size);
	mdia_desc = ioh.ioh_open(p_mdia, "rb", mdia_size);
	mp4_find_child_box(mdia_desc, mdhd_id, &mdhd_pos, &mdhd_size);
	p_mdhd = p_mdia + mdhd_pos;
	moov->trak[trak_no]->mdia->mdhd = mdhd_reader(p_mdhd, mdhd_size, mdhd_size);
	mp4_find_child_box(mdia_desc, hdlr_id, &hdlr_pos, &hdlr_size);
	p_hdlr = p_mdia + hdlr_pos;
	moov->trak[trak_no]->mdia->hdlr = hdlr_reader(p_hdlr, hdlr_size, hdlr_size);
	if(!(memcmp(&moov->trak[trak_no]->mdia->hdlr->handler_type, &search_trak_type, 4))) {
	    found = 1;
	}
	trak_no++;
    }while(!found);
	
	
    ioh.ioh_close(moov_desc);
    ioh.ioh_close(trak_desc);
    ioh.ioh_close(mdia_desc);
	
}
#endif

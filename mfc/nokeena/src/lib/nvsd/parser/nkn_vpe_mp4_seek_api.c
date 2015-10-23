/*
 *
 * Filename:  nkn_vpe_mp4_seek_parser.c
 * Date:      2010/01/01
 * Module:    mp4 seek API implementation
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

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

io_handlers_t ioh = {
    .ioh_open = nkn_vpe_memopen,
    .ioh_read = nkn_vpe_memread,
    .ioh_tell = nkn_vpe_memtell,
    .ioh_seek = nkn_vpe_memseek,
    .ioh_close = nkn_vpe_memclose,
    .ioh_write = nkn_vpe_memwrite
};

/**********************************************************************************
 * MP4 SEEK API's Implementation
 * All YouTube sepcific API's will have mp4_yt_xxx prefix
 * 1. mp4_get_moov_info - read the moov box details like size and offset in MP file
 * 2. mp4_init_moov - initializes a moov box; (reads mvhd and other relevant boxes).
 *    This function is implemented nkn_vpe_mp4_seek_parser.c    
 * 3. mp4_populate_trak - reads the trak into the moov context (Max tracks supported
      is 4). Only traks with the descriptor code 'vide' and 'soun' are processed in
      supsequent calls.
 * 4. mp4_seek_trak - converts the give seek time offset into a corresponding offset
 *    in the MP4 file. Also modifies all the sample tables to reflect the new seek'ed
 *    data state.
 * 5. mp4_moov_cleanup - cleans up all resources used in moov/trak etc
 * (See .h file for parametrized documentation and data types)
 * (YouTube specific seek API's after the generic seek API's)
 **********************************************************************************/

/*
 * SSP can obtain 32KB data each time.
 * this function will parse 32KB data each time
 * We can call this function multiple times to parse the MP4 file to get moov box info
 *
 * Some MP4 files put the moov box at the end of file or not at the first 32KB
 * This function is used to parse the MP4 file to obtain the moov and mdata box info
 */

nkn_vpe_mp4_find_box_parser_t*
mp4_init_find_box_parser(void)
{
    nkn_vpe_mp4_find_box_parser_t *fp;

    /* allocate the mp4 box parser state */
    fp = (nkn_vpe_mp4_find_box_parser_t*) \
	nkn_calloc_type(1,
			sizeof(nkn_vpe_mp4_find_box_parser_t),
			mod_vpe_mp4_find_box_parser_t);
    if (!fp) {
	return NULL;
    }

    fp->box_hd = (nkn_vpe_mp4_box_hd_t*) \
	nkn_calloc_type(1,
			sizeof(nkn_vpe_mp4_box_hd_t),
			mod_vpe_mp4_box_hd_t);
    if (!fp->box_hd) {
	free(fp);
	return NULL;
    }

    fp->pbox_list = (nkn_vpe_mp4_box_find_list_t*) \
	nkn_calloc_type(1,
			sizeof(nkn_vpe_mp4_box_find_list_t),
			mod_vpe_mp4_box_find_list_t);
    if (!fp->pbox_list) {
	free(fp->box_hd);
	free(fp);
	return NULL;
    }

    fp->ds = (nkn_vpe_mp4_data_span_t*) \
	nkn_calloc_type(1,
			sizeof(nkn_vpe_mp4_data_span_t),
			mod_vpe_mp4_data_span_t);
    if (!fp->ds) {
	free(fp->box_hd);
	free(fp->pbox_list);
	free(fp);
	return NULL;
    }

    return fp;
}

int32_t
mp4_set_find_box_name(nkn_vpe_mp4_find_box_parser_t* fp,
		      const char* find_box_name_list[],
		      size_t n_box_to_find)
{
    int32_t i;
    fp->pbox_list->box_info = (nkn_vpe_mp4_box_info_t*) \
	nkn_calloc_type(n_box_to_find,
			sizeof(nkn_vpe_mp4_box_info_t),
			mod_vpe_mp4_box_info_t);

    if (!fp->pbox_list->box_info) {
	return -1; /* error handle */
    }

    fp->pbox_list->n_box_to_find = n_box_to_find;
    for (i = 0; i < fp->pbox_list->n_box_to_find; i++) {
	fp->pbox_list->box_info[i].name = (char*) find_box_name_list[i];
	fp->pbox_list->box_info[i].offset = 0;
	fp->pbox_list->box_info[i].size = 0;
    }

    return 0;
}

/* open the data buf p_data with fp  */
int32_t
mp4_parse_init_databuf(nkn_vpe_mp4_find_box_parser_t* fp,
		       uint8_t *p_data,
		       size_t size)
{
    size_t box_hd_size;
    uint8_t box_hd[sizeof(nkn_vpe_mp4_box_hd_t)];
    box_t box;
    uint64_t box_size;
    const char ftype_id[] = {'f','t','y','p'};
    fp->iod = ioh.ioh_open((void*)p_data, "rb", size);

    if (size < 20) { /* ftype box at least 20 bytes */
	return  E_VPE_MP4_DATA_INSUFF;
    }

#if 0
    /* read the first 16 bytes for box size and type */
    ioh.ioh_read(box_hd, 1, sizeof(nkn_vpe_mp4_box_hd_t), fp->iod);

    /* parse box type and size */
    read_next_root_level_box(box_hd, sizeof(nkn_vpe_mp4_box_hd_t),
			     &box, &box_hd_size);
    box_size = box.short_size == 1 ?box.long_size : box.short_size;
    if (check_box_type(&box, ftype_id)) {
	/* found ftyp */
	fp->parsed_bytes_in_block += box_size;
	fp->bytes_to_next_box = 0;
	ioh.ioh_seek((void*)(fp->iod), box_size - sizeof(nkn_vpe_mp4_box_hd_t), SEEK_CUR);
	return fp->parsed_bytes_in_block ;
    } else {
	/*
	 * cannot find ftyp, not a valid mp4 file
	 * this situation  should not happen
	 * as we have file type detection before calling this function
	 */
	return E_VPE_MP4_DATA_INVALID;
    }
#endif

    return 0;
}

/*
 * Parse the p_data to find the box info of box set by mp4_set_find_box_name
 * This function can be called several times to parse the entire MP4 file
 *
 */

int32_t
mp4_parse_find_box_info(nkn_vpe_mp4_find_box_parser_t* fp,
			uint8_t *p_data,
			size_t size,
			off_t *brstart,
			off_t *brstop)
{
    size_t bytes_to_read, bytes_remaining, pos;
    int64_t body_length;
    int64_t box_size_remaining;

    size_t box_hd_size, box_size;
    box_t box;

    int32_t box_pos;
    int32_t rv;

    rv = E_VPE_MP4_DATA_INSUFF;
    body_length = 0;
    box_size_remaining = 0;

    if (!fp->iod) {
	/* if all data has been parsed in the prev call */
	fp->iod = ioh.ioh_open((void*)p_data, "rb", size);
    }

    if (fp->ds->data_span_flag) {
	/* box header in previous block */
	uint8_t * p_dst;
	p_dst = (uint8_t*) (fp->box_hd);

	/*
	 * read remaining bytes of the mp4 box header
	 * need to check whether size is larger than fp->ds->bytes_in_next_span
	 */
	if (size < fp->ds->bytes_in_next_span) {
	    /*
	     * With data_span_flag, current parse will start from the 32K block data boundary
	     * The only condition of size < fp->ds->bytes_in_next_span is for the last data block
	     * If this last data block is smaller than 16 bytes, it cannot contain a mdat or moov
	     * It must be an invalid file, and just exit.
	     */
	    return rv;
	}
	ioh.ioh_read((void*)(p_dst + fp->ds->bytes_in_curr_span),
		     1, fp->ds->bytes_in_next_span, (void*)(fp->iod));

	/* reset data spanning info */
	fp->ds->bytes_in_next_span = 0;
	fp->ds->bytes_in_curr_span = 0;
	fp->ds->data_span_flag = 0;

	bytes_to_read = sizeof(nkn_vpe_mp4_box_hd_t);
	goto continue_parsing;
    }

    {
	/* init variables for this run */
	bytes_to_read = sizeof(nkn_vpe_mp4_box_hd_t);
	pos = ioh.ioh_tell(fp->iod);

	/* start parsing till the end of this block */
	do {
	    bytes_remaining = size - nkn_vpe_memtell(fp->iod);
	    if (bytes_to_read <= bytes_remaining) { /* if the box header is within bounds*/
		ioh.ioh_read((void*)(fp->box_hd), 1, sizeof(nkn_vpe_mp4_box_hd_t), fp->iod);
	    continue_parsing:
		read_next_root_level_box((void*)fp->box_hd,
					 sizeof(nkn_vpe_mp4_box_hd_t),
					 /* 16 */
					 &box, &box_hd_size);

		/*check for whether this is a valid box size */
		if (box.short_size <= 0 ||(box.short_size == 1 && box.long_size < 16)
		    || (box.short_size > 1 && box.short_size < 8)) {
		    /* Invalid box size, corrupt MP4 file */
		    return E_VPE_MP4_DATA_INVALID;
		}

		box_size_remaining = (box.short_size == 1 ? box.long_size : box.short_size)
				     - sizeof(nkn_vpe_mp4_box_hd_t);

		for (int i = 0; i < fp->pbox_list->n_box_to_find; i++) {
		    if(check_box_type(&box,fp->pbox_list->box_info[i].name)) {
			/* move to the head of the box */
			fp->pbox_list->box_info[i].offset = pos;
			fp->pbox_list->box_info[i].offset += fp->parsed_bytes_in_file;
			fp->pbox_list->box_info[i].size = \
			    (box.short_size == 1 ? box.long_size : box.short_size);
			fp->pbox_list->n_box_found++;
		    }
		}

		if (fp->pbox_list->n_box_found == fp->pbox_list->n_box_to_find) {
		    /* found all the box that we need */
		    return fp->pbox_list->n_box_found;
		}

                /* check if the next box header is within current data buffer block */
		if ((ioh.ioh_tell(fp->iod) + box_size_remaining) >= size) {
	            /*
		     * next box header is not within this block;
		     * reset bitstream for next parser run
		     */
                    ioh.ioh_close((void*)(fp->iod));
		    fp->iod = NULL;
		    /* set the brstart and brstop to fetch the next box header */
		    fp->parsed_bytes_in_file += (pos  + (box.short_size == 1 ? box.long_size : box.short_size));
		    *brstart = fp->parsed_bytes_in_file;
		    *brstop = 0;
                    return rv;
                }

                /* next box header in current  block, skip to next box header */
                ioh.ioh_seek((void *)fp->iod, box_size_remaining, SEEK_CUR);
                pos = ioh.ioh_tell(fp->iod);

	    } else {
		/* next box header spans between current and next block */
		size_t span_bytes_remaining;

                /*
		 * copy the portion of the box header in the current span.
		 * Setup to read the rest of the data in the next span/block
		 */
                fp->ds->bytes_in_curr_span = span_bytes_remaining = size - pos;
		fp->ds->bytes_in_next_span = sizeof(nkn_vpe_mp4_box_hd_t) - fp->ds->bytes_in_curr_span;
                ioh.ioh_read((void*)fp->box_hd, 1, span_bytes_remaining, (void*)(fp->iod));
                ioh.ioh_close((void*)(fp->iod));

                /* indicate that all the data has been consumed */
                fp->iod = NULL;
                fp->parsed_bytes_in_file += size;
		*brstart = fp->parsed_bytes_in_file;
		*brstop = 0;
		/* set the data spanning flag */
		fp->ds->data_span_flag = 1;
                return rv;
	    } //if (bytes_to_read <= bytes_remaining) {

	} while (pos < size); /* end of parser run */

	/* successful parsed this block, but not find all the boxes */
	fp->parsed_bytes_in_file += size;
	*brstart = fp->parsed_bytes_in_file;
	*brstop = 0;
    }

    return rv;
}

int32_t
mp4_find_box_parser_cleanup(nkn_vpe_mp4_find_box_parser_t* fp)
{
    if (!fp) {
	return -1;
    }

    if (fp->iod) {
	ioh.ioh_close((void*)fp->iod);
    }

    if (fp->box_hd) {
	free(fp->box_hd);
    }

    if (fp->pbox_list) {
	if (fp->pbox_list->box_info) {
	    free(fp->pbox_list->box_info);
	}
	free(fp->pbox_list);
    }

    if (fp->ds) {
	free(fp->ds);
    }

    if (fp) {
	free(fp);
    }
    return 0;
}


int32_t
mp4_get_moov_info(uint8_t *p_data, size_t size, size_t *moov_offset, size_t *moov_size)
{
    size_t curr_pos, box_size, moov_pos;
    box_t box;
    uint8_t *moov_data, box_data[12];
    const char moov_id[] = {'m', 'o', 'o', 'v'};
    bitstream_state_t *bs;

    box_size = 0;
    curr_pos = 0;
    moov_pos = 0;
    moov_data = NULL;

    /* for moov pos and size, 0 is a invalid value*/
    *moov_offset = * moov_size = 0;
    bs = (bitstream_state_t*) ioh.ioh_open( (char*)p_data, "rb", size);
    if (bs == NULL) {
	/* we will check the moov offset and size out side*/
	return 0;
    }
    curr_pos = ioh.ioh_tell((void *)bs);

    do {
	/* read the first 8 bytes for box size and type */
	ioh.ioh_read(box_data, 1, 8, (void *)bs);

	/* parse box type and size*/
	read_next_root_level_box(box_data, 8, &box, &box_size);
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
	    /* Invalid MP4 file*/
	    ioh.ioh_close(bs);
	    return 0;
	}
	/* check if box size is MOOV */
	if(check_box_type(&box, moov_id)) {
	    moov_pos = ioh.ioh_tell( (void*)bs);
	    *moov_offset = moov_pos  -= 8; //move to the head of the box
	    *moov_size = box.short_size;
	    break;
	}

	/* got to next root level box */
	if(is_container_box(&box)) {
	    ioh.ioh_seek((void*)bs, 0, SEEK_CUR);
	} else {
	    ioh.ioh_seek((void*)bs, box.short_size - 8, SEEK_CUR);
	}

    } while( ioh.ioh_tell((void*)bs) < size);

    ioh.ioh_close(bs);
    return 0;
}


int32_t
mp4_populate_trak(moov_t *moov)
{
    bitstream_state_t *moov_desc;
    size_t trak_pos, trak_size, box_pos;
    int32_t found, *type, trak_no, video_id, audio_id;
    box_t box;

    /*initialization */
    moov_desc = NULL;
    trak_pos = trak_size = 0;
    found = 0;
    trak_no = 0;
    box_pos = 0;
    type = NULL;
    video_id = VIDE_HEX;
    audio_id = SOUN_HEX;

    moov_desc = ioh.ioh_open((char *)moov->data, "rb", moov->size);
    if (moov_desc == NULL) {
	goto error;
    }
    //	mp4_find_child_box(moov_desc, trak_id, &trak_pos, &trak_size);
      
    /* build TRAK info for video track */
    do {

	mp4_read_next_box(moov_desc, &box_pos, &box);
	if (box.short_size <= 0 || (box.short_size == 1 && box.long_size < 16)
	    || (box.short_size > 1 && box.short_size < 8)) {
	    /* corrupt MP4 file */
	    goto error;
	}
	type = (int32_t*)(&box.type);
	if(is_container_box(&box)) {
	    if (*type == FREE_ID) {
		// if free box, we should bypass it, no need to parse free box
		ioh.ioh_seek(moov_desc, box.short_size, SEEK_CUR);
	    } else if (*type != META_ID) {
		ioh.ioh_seek(moov_desc, 8, SEEK_CUR);
	    } else {
		ioh.ioh_seek(moov_desc, 12, SEEK_CUR);
	    }
	} else {
	    ioh.ioh_seek(moov_desc, box.short_size, SEEK_CUR);
	}
	
	switch(*type) {
	    case MVHD_ID:
		moov->mvhd = mvhd_reader(moov->data + box_pos, box.short_size, box.short_size);
		if (moov->mvhd == NULL) {
		    goto error;
		}
		moov->mvhd_pos = box_pos;
		moov->mvhd_size = box.short_size;
		moov->mvhd_data = moov->data + box_pos;
		break;		       
	    case TRAK_ID:
		ioh.ioh_seek(moov_desc, box_pos, SEEK_SET);
		moov->trak[trak_no] = mp4_read_trak(moov->data, box_pos, box.short_size, moov_desc);
		if (moov->trak[trak_no] == NULL) {
		    goto error;
		}
		moov->n_tracks++;

		if(mp4_validate_trak(moov->trak[trak_no]) == -1) {
		    goto error;
		}

		if( (!(memcmp(&moov->trak[trak_no]->mdia->hdlr->handler_type, &video_id, 4))) ||
		    (!(memcmp(&moov->trak[trak_no]->mdia->hdlr->handler_type, &audio_id, 4))) ) {
		    moov->trak_process_map[trak_no] = 1;
		    moov->n_tracks_to_process++;
		}
		trak_no++;
		break;
	    case UDTA_ID:
		moov->udta = mp4_init_parent(moov->data + box_pos, box_pos, box.short_size, UDTA_ID);
		if (moov->udta == NULL) {
		    goto error;
		}
		moov->udta_mod_size = moov->udta->size + 100;
		moov->udta_mod =
		    (uint8_t*)nkn_malloc_type(moov->udta_mod_size,
					      mod_vpe_mp4_udta_buf);
		if (moov->udta_mod == NULL) {
		    goto error;
		}
		moov->udta_mod_pos = -1; /* indicates this needs to be appended */
		/* make a copy of the udta box to make a copy of */
		memcpy(moov->udta_mod, moov->udta->data, moov->udta->size);
		break;
	    case ILST_ID:
		if (moov->udta == NULL) {
		    goto error;
		}
		moov->udta->ilst_pos = box_pos;
		moov->udta->ilst_data = moov->data + box_pos;
		moov->udta->ilst_size = box.short_size;
		break;
	}

    }while( (!found) && (ioh.ioh_tell((void*)(moov_desc)) < (moov->size - 1)) );

    ioh.ioh_close(moov_desc);
    return trak_no;
 error:
    if (moov_desc) {
	ioh.ioh_close(moov_desc);
    }

    return -1;

}


size_t
mp4_seek_trak(moov_t *moov, int32_t trak_no, float seek_to, moov_t **moov_out,
	      vpe_ol_t* ol, int n_box_to_find)
{
    int32_t trak_timescale, i, j , moov_timescale, find_timestamp, 
	sample_num, first_sample_in_chunk, trak_type, fc_rv;
    int32_t start_sample_num[MAX_TRACKS], end_sample_num[MAX_TRACKS],
	chunk_num[MAX_TRACKS], dummy[MAX_TRACKS];
    size_t base_offset, mdat_offset[MAX_TRACKS], bytes_to_skip;
    table_shift_params_t tsp[MAX_TRACKS];
    trak_t  *trak;
    double synced_seek_time;
 
    /*initialization */
    i = j = 0;
    find_timestamp = 0;
    base_offset = 0;
    sample_num = 0;
    trak = NULL;
    bytes_to_skip = 0;
    synced_seek_time = (double)seek_to;
    trak_type = 0;

    if(seek_to < 0) {
	seek_to = 0;
    }
    moov_timescale = moov->mvhd->timescale;

    for(j = 0; j < 2; j++) {
	for(i = 0; i < trak_no; i++) {
		
	    /* convert seek time stamp to the TRAK's timescale */
	    trak = moov->trak[i];
		
	    trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));

	    if(moov->trak_process_map[i]) {
		if((j == 0) && trak_type == SOUN_HEX) {
		    continue;
		} else if ((j == 1) && (trak_type == VIDE_HEX)) {
		    continue;
		}
	    } else {
		continue;
	    }

	    trak_timescale = trak->mdia->mdhd->timescale;
	    find_timestamp = (int32_t)(synced_seek_time * trak_timescale);
		
	    /* validate if the seek time is within the duration of the video */
	    if((uint32_t)find_timestamp > trak->mdia->mdhd->duration) {
		goto error;
	    }
		
	    /* map the timestamp to a sample number */
	    sample_num = mp4_timestamp_to_sample(trak, find_timestamp, &tsp[i].stts_sp);
	    if (sample_num < 0) {
		goto error;
	    }
	    /* find the nearest sync sample */
	    sample_num = mp4_find_nearest_sync_sample(trak, sample_num, &tsp[i].stss_sp);
	    if (sample_num < 0) {
		goto error;
	    }
	    moov->synced_seek_time = synced_seek_time = mp4_sample_to_time(trak, sample_num) / (double)trak_timescale;
	    if (synced_seek_time == SIZE_MAX) {
		goto error;
	    }
	    /* get shift offsets for STTS & CTTS tables */
	   fc_rv = mp4_get_stts_shift_offset(trak, sample_num, &tsp[i].stts_sp);
	   if (fc_rv < 0) {
	       goto error;
	   }
	   fc_rv = mp4_get_ctts_shift_offset(trak, sample_num, &tsp[i].ctts_sp);
	   if (fc_rv < 0) {
	       goto error;
	   }
	    /* store the start and end sample numbers */
	    start_sample_num[i] = sample_num;
	    end_sample_num[i] = mp4_get_stsz_num_entries(trak);
	    if (end_sample_num[i] < 0) {
		goto error;
	    }
	    /* map the sample number to a chunk in the MP4 file */
	    chunk_num[i] = mp4_sample_to_chunk_num(trak, sample_num, &first_sample_in_chunk, &tsp[i].stsc_sp);
	    if (chunk_num[i] < 0) {
		goto error;
	    }
	    /* find the start file offset for that chunk */
	    base_offset = mp4_chunk_num_to_chunk_offset(trak, chunk_num[i]-1);
	    if (base_offset == SIZE_MAX) {
		goto error;
	    }
	    /* find the point within the chunk, where the sample lies. the actual offset is caluculated for the sample */
	    mdat_offset[i] = (mp4_find_cumulative_sample_size(trak, first_sample_in_chunk, sample_num));
	    if (mdat_offset[i] == SIZE_MAX) {
		goto error;
	    }
	    mdat_offset[i] += base_offset;
	}
    }
		
    /* adjusts all the sample tables (inplace) for the new [sample num, start offset] pair */
    /*    bytes_to_skip = mp4_adjust_sample_tables_inplace(moov, moov->n_tracks_to_process, start_sample_num, 
	  end_sample_num, chunk_num, dummy, mdat_offset, tsp);*/

    /* adjusts all the sample tables and copies the new tables into moov_out */
    bytes_to_skip = mp4_adjust_sample_tables_copy(moov, moov->n_tracks_to_process, start_sample_num, 
						  end_sample_num, chunk_num, dummy, mdat_offset, tsp,
						  moov_out, ol, n_box_to_find);

    return bytes_to_skip;
 error:
    bytes_to_skip = -1;
    return bytes_to_skip;
}

/**********************************************************************************
 * YOUTUBE SEEK SPECIFIC API's Implementation
 * All YouTube sepcific API's will have mp4_yt_xxx prefix
 * 1. mp4_yt_modify_udta - modifies the UDTA box for the new seek time offset
 * 2. mp4_yt_get_modified_udta_data_buf - retrieves the modified new udta buf from
 *    the parser context
 * 3. mp4_yt_modify_moov - modifies the moov box (markss the old udta box as a free
 *    box, modifies the moov box size to reflect the new state
 **********************************************************************************/
int32_t
mp4_yt_modify_udta(moov_t *moov)
{
    bitstream_state_t *bs, *bs_out;
    size_t bytes_parsed, box_pos, bytes_to_parse;
    uint32_t container_size, new_size;
    box_t box;
    int32_t *type, size_diff = 0, free_id, bcheck_bytes_left = 0;
    uint8_t container_box[12], reached_ilst;
    double seek_time;

    if (!moov->udta)  // no udta 
	return 0;

    /* open the udta box as a bitstream */
    bs = ioh.ioh_open((char*)(moov->udta->data), "rb", moov->udta->size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = ioh.ioh_open((char*)(moov->udta_mod), "wb", moov->udta_mod_size);
    if (bs_out == NULL) {
	goto error;
    }
    seek_time = moov->synced_seek_time;

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
	    } else { 
		container_size = 8;
	    }

	    /* copy the container/ parent box */
	    ioh.ioh_read((void*)(container_box), 1, container_size, (void*)bs);
	    ioh.ioh_write((void*)(container_box), 1, container_size, (void*)(bs_out));
	} else {
	    /* not a container box */
	    if(*type != ILST_ID) {
		/* box is not a ILST box */

		uint8_t *box_data;

		/* allocate memory for box data */
		box_data = (uint8_t*)nkn_malloc_type(box.short_size,
						     mod_vpe_mp4_box_data);
		if (!box_data) {
		    goto error;
		}
		/* copy the child box */
		bcheck_bytes_left = moov->udta_mod_size -bytes_parsed - 8;
		if (box.short_size > (uint32_t)bcheck_bytes_left) {
		    if (box_data) free(box_data);
		    goto error;
		}
		ioh.ioh_read((void*)(box_data), 1, box.short_size, (void*)bs);
		ioh.ioh_write((void*)(box_data), 1, box.short_size, (void*)(bs_out));
		free(box_data);
			      
	    } else {
		/* parse and modify the ILST box */
		size_diff = mp4_yt_adjust_ilst_props(moov, seek_time, (void*)(bs_out));
		if (INT32_MIN == size_diff) {
		    goto error;
		}
		ioh.ioh_seek((void*)(bs), box.short_size, SEEK_CUR);
	    }
	}
	
	bytes_parsed = ioh.ioh_tell((void*)(bs));
    } while(bytes_parsed < moov->udta->size);


    /* adjust the size of all the nodes who are the parent of ilst */
    bytes_parsed = 0;
    reached_ilst = 0;
    bytes_to_parse = moov->udta->size + size_diff;
    ioh.ioh_seek((void*)bs_out, 0, SEEK_SET);
    do {

	mp4_read_next_box(bs_out, &box_pos, &box);
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
	    } else { 
		container_size = 8;
	    }
	    new_size = box.short_size + size_diff;
	    new_size = nkn_vpe_swap_uint32(new_size);

	    if(!reached_ilst) {
		/* write new size */
		ioh.ioh_write((void*)(&new_size), 1, sizeof(int32_t), (void*)(bs_out));
		/* skip to next block */
		ioh.ioh_seek((void*)bs_out, container_size - 4, SEEK_CUR);
	    } else {
		ioh.ioh_seek((void*)bs_out, container_size, SEEK_CUR);
	    }
	    
	} else {
	    /* not a container */
	    switch (*type) {
		case ILST_ID:
		    new_size = box.short_size + size_diff;
		    new_size = nkn_vpe_swap_uint32(new_size);
		    
		    /* write new size */
		    ioh.ioh_write((void*)(&new_size), 1, sizeof(int32_t), (void*)(bs_out));
	    
		    /* skip to next box */
		    ioh.ioh_seek((void*)bs_out, 4, SEEK_CUR);
		    
		    continue;

		case GSST_ID:
		    new_size = box.short_size + size_diff;
		    new_size = nkn_vpe_swap_uint32(new_size);
		    
		    /* write new size */
		    ioh.ioh_write((void*)(&new_size), 1, sizeof(int32_t), (void*)(bs_out));
	    
		    /* skip to next box */
		    ioh.ioh_seek((void*)bs_out, box.short_size - 4, SEEK_CUR);

		    reached_ilst = 1;
		    continue;
	    
	    }

	    /* skip to next box */
	    ioh.ioh_seek((void*)bs_out, box.short_size, SEEK_CUR);	    
	}
	bytes_parsed = ioh.ioh_tell((void*)(bs_out));
    } while(!reached_ilst && bytes_parsed < bytes_to_parse);

    /* update the new udta size */
    moov->udta_mod_size = moov->udta->size + size_diff;

    /* mark the existing udta tag as free */
    ioh.ioh_seek((void*)bs, 4, SEEK_SET);
    free_id = FREE_ID;
    ioh.ioh_write((void*)(&free_id), 1, sizeof(int32_t), (void*)(bs));
    
    ioh.ioh_close(bs);
    ioh.ioh_close(bs_out);
    return 0;
 error:
    if (bs) {
	ioh.ioh_close(bs);
    }
    if (bs_out) {
	ioh.ioh_close(bs_out);
    }
    return -1;
}    

int32_t
mp4_get_modified_udta(moov_t *moov, uint8_t **p_new_udta, size_t *new_udta_size)
{
    *p_new_udta = moov->udta_mod;
    *new_udta_size = moov->udta_mod_size;

    return 0;
}

int32_t
mp4_modify_moov(moov_t *moov, size_t appended_tag_size)
{
    bitstream_state_t *bs;
    uint32_t tmp, i;
    
    bs = ioh.ioh_open((char*)moov->data, "wb", moov->size);
    
    tmp = nkn_vpe_swap_uint32((moov->size + appended_tag_size));
    ioh.ioh_write((void*)(&tmp), 1, sizeof(int32_t), (void*)bs);

    for(i = 0; i < moov->n_tracks_to_process; i++) {
	mp4_rebase_stco(moov->trak[i], appended_tag_size);
    }

    ioh.ioh_close(bs);

    return 0;
}

/*
 * BZ: 8382
 * MP4 file created by ffmpeg and mp4box does not have the youtube udta box
 * Inject this udta box into moov
 *
 */
int32_t
mp4_yt_create_udta(moov_t *moov)
{
    bitstream_state_t *bs_out;
    //uint8_t           *box_data;
    //uint32_t           container_box_size;
    uint32_t           box_name, box_size, data_type;
    uint32_t           version_flag;
    uint64_t           handler_type = 0x6c7070617269646d; //mdiappl
    uint8_t           *temp;
    uint8_t            temp_value_8;
    uint32_t           temp_value_32;
    int                diff_size;
    uint64_t           td_duration;
    uint32_t           ndigits,gstd_len;
    char               *td_duration_str, format_str[3+1];

    td_duration = moov->mvhd->duration*1000/moov->mvhd->timescale;
    ndigits = ceil(log10(td_duration));
    gstd_len = 4 + ndigits;
    diff_size = gstd_len - 10; // when gstd_len == 10, all box sizes are the default
    //	memset(temp,0,12);
    moov->udta_mod_size = YT_UDTA_INJECT_SIZE_WO_SWAP + diff_size;

    moov->udta_mod =(uint8_t*)nkn_malloc_type(moov->udta_mod_size,
                                              mod_vpe_mp4_udta_buf);
    if (moov->udta_mod == NULL) {
	return -1;
    }
    moov->udta_mod_pos = -1; //indicates this needs to be appended
    memset(moov->udta_mod, 0, moov->udta_mod_size);

    bs_out = ioh.ioh_open((char*)(moov->udta_mod), "wb", moov->udta_mod_size);

    //udta box header (box)
    box_size     = nkn_vpe_swap_uint32(moov->udta_mod_size);
    box_name     = UDTA_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));

    //meta box header (full box)
    box_size     = nkn_vpe_swap_uint32(YT_META_INJECT_SIZE_WO_SWAP + diff_size);
    box_name     = META_ID;
    version_flag = 0x00000000; //1 byte version, 3 bytes flag
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&version_flag), 1, sizeof(uint32_t), (void*)(bs_out));

    //hdlr box (full box)
    box_size     = YT_HDLR_INJECT_SIZE;
    box_name     = HDLR_ID;
    version_flag = 0x00000000;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));

    //write version flag and 4 bytes pre_defines(0x00000000);
    ioh.ioh_write((void*)(&version_flag), 1, sizeof(uint32_t), (void*)(bs_out));
    //version flag and pre_defeins are both all 4 bytes zeros, so I write twice
    ioh.ioh_write((void*)(&version_flag), 1, sizeof(uint32_t), (void*)(bs_out));
    //the udta's hdlr is not the same as other's, it costs 8 bytes
    ioh.ioh_write((void*)(&handler_type), 1, sizeof(uint64_t), (void*)(bs_out));
    //9 bytes all 0
    temp = (uint8_t*)nkn_malloc_type(9, mod_vpe_mp4_udta_buf);
    if (temp == NULL) {
	return -1;
    }
    memset(temp, 0, 9);
    ioh.ioh_write((void*)temp, 1, 9, (void*)(bs_out));
    free(temp);

    //ilist box (box)
    box_size     = nkn_vpe_swap_uint32(YT_ILST_INJECT_SIZE_WO_SWAP + diff_size);
    box_name     = ILST_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));

    //start the gsst gstd gssd gspu gspm gshh
    //gsst
    box_size     = YT_GSST_INJECT_SIZE;
    box_name     = GSST_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    box_size     = nkn_vpe_swap_uint32(0x00000011);
    box_name     = DATA_ID;
    data_type    = nkn_vpe_swap_uint32(0x00000001);
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&data_type), 1, sizeof(uint32_t), (void*)(bs_out));
    //then 00000000 30
    temp_value_32 = 0;
    ioh.ioh_write((void*)(&temp_value_32), 1, 4, (void*)(bs_out));
    temp_value_8 = 0x30; // this is value of char '0';
    ioh.ioh_write((void*)(&temp_value_8), 1, sizeof(uint8_t), (void*)(bs_out));

    //gstd
    box_size     = nkn_vpe_swap_uint32(YT_GSTD_INJECT_SIZE_WO_SWAP + diff_size) ;
    box_name     = GSTD_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    //moov->mvhd->duration*1000/moov->mvhd->timescale;
    td_duration_str = (char *)alloca(gstd_len + 1);
    memset(td_duration_str, 0, gstd_len +1);
    snprintf(format_str, 4, "%%%dd", ndigits);
    snprintf(td_duration_str + 4, ndigits+1, format_str, td_duration);

    box_size     = nkn_vpe_swap_uint32(12 + gstd_len);
    box_name     = DATA_ID;
    data_type    = nkn_vpe_swap_uint32(0x00000001);
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&data_type), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(td_duration_str), 1, gstd_len, (void*)(bs_out));

    //gssd
    box_size     = YT_GSSD_INJECT_SIZE;
    box_name     = GSSD_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    box_size     = nkn_vpe_swap_uint32(0x00000030);
    box_name     = DATA_ID;
    data_type    = nkn_vpe_swap_uint32(0x00000001);
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&data_type), 1, sizeof(uint32_t), (void*)(bs_out));
    //36 bytes all 0
    temp = (uint8_t*)nkn_malloc_type(36, mod_vpe_mp4_udta_buf);
    if (temp == NULL) {
	return -1;
    }
    memset(temp, 0, 36);
    ioh.ioh_write((void*)temp, 1, 36, (void*)(bs_out));
    free(temp);

    //gspu
    box_size     = YT_GSPU_INJECT_SIZE;
    box_name     = GSPU_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    box_size     = nkn_vpe_swap_uint32(0x00000090);
    box_name     = DATA_ID;
    data_type    = nkn_vpe_swap_uint32(0x00000001);
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&data_type), 1, sizeof(uint32_t), (void*)(bs_out));
    //132 bytes all 0
    temp = (uint8_t*)nkn_malloc_type(132, mod_vpe_mp4_udta_buf);
    if (temp == NULL) {
	return -1;
    }
    memset(temp, 0, 132);
    ioh.ioh_write((void*)temp, 1, 132, (void*)(bs_out));
    free(temp);

    //gspm
    box_size     = YT_GSPM_INJECT_SIZE;
    box_name     = GSPM_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    box_size     = nkn_vpe_swap_uint32(0x00000090);
    box_name     = DATA_ID;
    data_type    = nkn_vpe_swap_uint32(0x00000001);
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&data_type), 1, sizeof(uint32_t), (void*)(bs_out));
    // 132 bytes all 0
    temp = (uint8_t*)nkn_malloc_type(132, mod_vpe_mp4_udta_buf);
    if (temp == NULL) {
	return -1;
    }
    memset(temp, 0, 132);
    ioh.ioh_write((void*)temp, 1, 132, (void*)(bs_out));
    free(temp);

    //gshh
    box_size     = YT_GSHH_INJECT_SIZE;
    box_name     = GSHH_ID;
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    box_size     = nkn_vpe_swap_uint32(0x00000110);
    box_name     = DATA_ID;
    data_type    = nkn_vpe_swap_uint32(0x00000001);
    ioh.ioh_write((void*)(&box_size), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&box_name), 1, sizeof(uint32_t), (void*)(bs_out));
    ioh.ioh_write((void*)(&data_type), 1, sizeof(uint32_t), (void*)(bs_out));
    //260 bytes all 0
    temp = (uint8_t*)nkn_malloc_type(260, mod_vpe_mp4_udta_buf);
    if (temp == NULL) {
	return -1;
    }
    memset(temp, 0, 260);
    ioh.ioh_write((void*)temp, 1, 260, (void*)(bs_out));
    free(temp);

    //finished
    ioh.ioh_close(bs_out);
    return 0;
}


int32_t
mp4_yt_update_moov_size(moov_t *moov)
{
    bitstream_state_t *bs;
    uint32_t tmp;

    bs = ioh.ioh_open((char*)moov->data, "rb", moov->size);
    ioh.ioh_read((void*)(&tmp), 1, sizeof(uint32_t), (void*)bs);
    moov->size = nkn_vpe_swap_uint32(tmp);
    ioh.ioh_close(bs);
    return 0;
}


//////////////////////////////////////////////////////////////////


/*******************************************************************************
 * DailyMotion specific seek API's implemented below with the
 * mp_dm_xxxxx 
 ******************************************************************************/
int32_t
mp4_dm_modify_udta(moov_t *moov, void *seek_provider_specific_info)
{
    bitstream_state_t *bs, *bs_out;
    size_t bytes_parsed, box_pos, bytes_to_parse;
    uint32_t container_size, new_size;
    box_t box;
    int32_t *type, size_diff = 0, free_id, bcheck_bytes_left = 0;
    uint8_t container_box[12], reached_ilst;
    double seek_time;
    dm_seek_input_t *dsi =\
	(dm_seek_input_t*)seek_provider_specific_info;

    /* open the udta box as a bitstream */
    bs = ioh.ioh_open((char*)(moov->udta->data), "rb", moov->udta->size);
    if (bs == NULL) {
	goto error;
    }
    bs_out = ioh.ioh_open((char*)(moov->udta_mod), "wb", moov->udta_mod_size);
    if (bs == NULL) {
	goto error;
    }
    seek_time = moov->synced_seek_time;

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
	    } else { 
		container_size = 8;
	    }

	    /* copy the container/ parent box */
	    ioh.ioh_read((void*)(container_box), 1, container_size, (void*)bs);
	    ioh.ioh_write((void*)(container_box), 1, container_size, (void*)(bs_out));
	} else {
	    /* not a container box */
	    if(*type != ILST_ID) {
		/* box is not a ILST box */

		uint8_t *box_data;

		/* allocate memory for box data */
		box_data = (uint8_t*)nkn_malloc_type(box.short_size,
						     mod_vpe_mp4_box_data);
		if (!box_data) {
		    goto error;
		}
		/* copy the child box */
		bcheck_bytes_left = moov->udta_mod_size -bytes_parsed - 8;
		if (box.short_size > (uint32_t)bcheck_bytes_left) {
		    if (box_data) free(box_data);
		    goto error;
		}		
		ioh.ioh_read((void*)(box_data), 1, box.short_size, (void*)bs);
		ioh.ioh_write((void*)(box_data), 1, box.short_size, (void*)(bs_out));
		
		free(box_data);
			      
	    } else {
		/* parse and modify the ILST box */
		size_diff = mp4_dm_adjust_ilst_props(moov, seek_time,
						     dsi, (void*)(bs_out));
		if (INT32_MIN == size_diff) {
		    goto error;
		}
		ioh.ioh_seek((void*)(bs), box.short_size, SEEK_CUR);
	    }
	}
	
	bytes_parsed = ioh.ioh_tell((void*)(bs));
    } while(bytes_parsed < moov->udta->size);


    /* adjust the size of all the nodes who are the parent of ilst */
    bytes_parsed = 0;
    reached_ilst = 0;
    bytes_to_parse = moov->udta->size + size_diff;
    ioh.ioh_seek((void*)bs_out, 0, SEEK_SET);
    do {

	mp4_read_next_box(bs_out, &box_pos, &box);
	type = (int32_t*)(&box.type);
	if(is_container_box(&box)) {
	    /* this is a container box */
	    
	    if(*type == META_ID) {
		/* META tag is a full box container */
		container_size = 12;
	    } else { 
		container_size = 8;
	    }
	    new_size = box.short_size + size_diff;
	    new_size = nkn_vpe_swap_uint32(new_size);

	    if(!reached_ilst) {
		/* write new size */
		ioh.ioh_write((void*)(&new_size), 1, sizeof(int32_t), (void*)(bs_out));
		/* skip to next block */
		ioh.ioh_seek((void*)bs_out, container_size - 4, SEEK_CUR);
	    } else {
		ioh.ioh_seek((void*)bs_out, container_size, SEEK_CUR);
	    }
	    
	} else {
	    /* not a container */
	    switch (*type) {
		case ILST_ID:
		    new_size = box.short_size + size_diff;
		    new_size = nkn_vpe_swap_uint32(new_size);
		    
		    /* write new size */
		    ioh.ioh_write((void*)(&new_size), 1, sizeof(int32_t), (void*)(bs_out));
	    
		    /* skip to next box */
		    ioh.ioh_seek((void*)bs_out, 4, SEEK_CUR);
		    
		    reached_ilst = 1;
		    continue;

	    
	    }

	    /* skip to next box */
	    ioh.ioh_seek((void*)bs_out, box.short_size, SEEK_CUR);	    
	}
	bytes_parsed = ioh.ioh_tell((void*)(bs_out));
    } while(!reached_ilst && bytes_parsed < bytes_to_parse);

    /* update the new udta size */
    moov->udta_mod_size = moov->udta->size + size_diff;

    /* mark the existing udta tag as free */
    ioh.ioh_seek((void*)bs, 4, SEEK_SET);
    free_id = FREE_ID;
    ioh.ioh_write((void*)(&free_id), 1, sizeof(int32_t), (void*)(bs));
    
    ioh.ioh_close(bs);
    ioh.ioh_close(bs_out);

    return 0;
 error:
    if (bs) {
	ioh.ioh_close((void*)bs);
    }
    if (bs_out) {
	ioh.ioh_close((void*)bs_out);
    }
    return -1;
}

#if 0
size_t
mp4_seek_trak(moov_t *moov, int32_t trak_no, float seek_to)
{
    int32_t trak_timescale, i, moov_timescale, find_timestamp, 
	sample_num, first_sample_in_chunk;
    int32_t start_sample_num[MAX_TRACKS], end_sample_num[MAX_TRACKS],
	chunk_num[MAX_TRACKS], dummy[MAX_TRACKS];
    size_t base_offset, mdat_offset[MAX_TRACKS], bytes_to_skip;
    table_shift_params_t tsp[MAX_TRACKS];
    trak_t  *trak;

    /*initialization */
    i = 0;
    find_timestamp = 0;
    base_offset = 0;
    sample_num = 0;
    trak = NULL;
    bytes_to_skip = 0;

    if(seek_to < 0) {
	seek_to = 0;
    }
    moov_timescale = moov->mvhd->timescale;
		
    for(i = 0; i < trak_no; i++) {
		
	/* convert seek time stamp to the TRAK's timescale */
	trak = moov->trak[i];
	trak_timescale = trak->mdia->mdhd->timescale;
	find_timestamp = seek_to * trak_timescale;
		
	/* validate if the seek time is within the duration of the video */
	if(((uint32_t)(find_timestamp)) > trak->mdia->mdhd->duration) {
	    bytes_to_skip = -1;
	    goto error;
	}
		
	/* map the timestamp to a sample number */
	sample_num = mp4_timestamp_to_sample(trak, find_timestamp, &tsp[i].stts_sp);
		
	/* find the nearest sync sample */
	sample_num = mp4_find_nearest_sync_sample(trak, sample_num, &tsp[i].stss_sp);

	/* get shift offsets for STTS & CTTS tables */
	mp4_get_stts_shift_offset(trak, sample_num, &tsp[i].stts_sp);
	mp4_get_ctts_shift_offset(trak, sample_num, &tsp[i].ctts_sp);

	/* store the start and end sample numbers */
	start_sample_num[i] = sample_num;
	end_sample_num[i] = mp4_get_stsz_num_entries(trak);

	/* map the sample number to a chunk in the MP4 file */
	chunk_num[i] = mp4_sample_to_chunk_num(trak, sample_num, &first_sample_in_chunk, &tsp[i].stsc_sp); 

	/* find the start file offset for that chunk */
	base_offset = mp4_chunk_num_to_chunk_offset(trak, chunk_num[i]-1);

	/* find the point within the chunk, where the sample lies. the actual offset is caluculated for the sample */
	mdat_offset[i] = base_offset + (size_t)(mp4_find_cumulative_sample_size(trak, first_sample_in_chunk, sample_num));
    }
		
    bytes_to_skip = mp4_adjust_sample_tables(moov, 2, start_sample_num, end_sample_num, chunk_num, dummy, mdat_offset, tsp);

 error:
    return bytes_to_skip;
}
#endif

void
mp4_moov_cleanup(moov_t *moov, int32_t num_traks) 
{
    int32_t i;
    
    if(moov) {
	for(i = 0;i < num_traks; i++) {
	    mp4_trak_cleanup(moov->trak[i]);
	}
	
	if(moov->mvhd) {
	    free(moov->mvhd);
	}
	
	if(moov->udta) {
	    free(moov->udta);
	}

	if(moov->udta_mod)
	    free(moov->udta_mod);	
	free(moov);
    }
}

void
mp4_moov_out_cleanup(moov_t *moov_out, int32_t num_traks)
{
    int32_t i;

    if(moov_out) {
        for(i = 0;i < num_traks; i++) {
	    /*
	     * moov_out is different from moov
	     * moov will alloc memory for each traks
	     * moov_out only alloc memory for traks need to process
	     */
	    if (moov_out->trak_process_map[i]) {
		mp4_trak_cleanup(moov_out->trak[i]);
	    }
        }

        if(moov_out->mvhd) {
            free(moov_out->mvhd);
        }

        if(moov_out->udta) {
            free(moov_out->udta);
        }

        if(moov_out->udta_mod)
            free(moov_out->udta_mod);
        free(moov_out);
    }
}

void
mp4_trak_cleanup(trak_t *trak)
{
    if(trak) {
	if(trak->tkhd) {
	    free(trak->tkhd);
	}
	if(trak->mdia) {
	    mp4_cleanup_mdia(trak->mdia);
	}
	if(trak->stbl) {
	    free(trak->stbl);
	}
	free(trak);
    }
}

void
mp4_cleanup_mdia(mdia_t *mdia)
{
    if(mdia) {
	if(mdia->hdlr) {
	    free(mdia->hdlr->name);
	    free(mdia->hdlr);
	}
	if(mdia->mdhd) {
	    free(mdia->mdhd);
	}
	
	free(mdia);
    }
}

void
mp4_yt_cleanup_udta_copy(moov_t *moov)
{
    if ( moov &&  moov->udta_mod ) {
	free(moov->udta_mod);
    }
}
	
void *
nkn_vpe_memopen(char *p_data, const char *mode, size_t size)
{
    bitstream_state_t *bs;

    bs = bio_init_bitstream((uint8_t*)p_data, size);

    return (void *)(bs);
}

size_t
nkn_vpe_memseek(void *desc, size_t seek, size_t whence)
{

    bitstream_state_t *bs;

    bs = (bitstream_state_t*)(desc);
    return bio_aligned_seek_bitstream(bs, seek, whence);
}

size_t 
nkn_vpe_memtell(void *desc)
{
    return bio_get_pos((bitstream_state_t*)(desc));
}

void
nkn_vpe_memclose(void *desc)
{
    return bio_close_bitstream((bitstream_state_t*)(desc));
}

size_t
nkn_vpe_memwrite(void *buf, size_t n, size_t size, void *desc)
{
    return bio_aligned_write_block((bitstream_state_t *)(desc), buf, size*n);
}


size_t
nkn_vpe_memread(void *buf, size_t n, size_t size, void *desc)
{
    bitstream_state_t *bs;

    bs = (bitstream_state_t *)(desc);
    bio_aligned_read(bs, buf, n*size);
	
    return n;
}

const mp4_seek_provider_detect g_spd[SEEK_PTYPE_MAX] = {
    {{{0}}, {0.0}, {0}, 0},
    {{{0}}, {0.0}, {0}, 0},
    {{{UDTA_ID, ILST_ID, 0x6f6f74a9}}, {1.0}, {3}, 1}
};

   
int32_t 
mp4_guess_seek_provider(moov_t *moov, MP4_SEEK_PROVIDER_TYPE *ptype)
{
    //bitstream_state_t *bs;
    
        
    return 0;
}

int32_t
mp4_test_known_seek_provider(moov_t *moov, 
			     MP4_SEEK_PROVIDER_TYPE ptype, 
			     uint8_t *result)
{
    //bitstream_state_t bs;
    const mp4_seek_provider_detect *spd;
    int32_t err = 0;
    uint32_t i, num_boxes, num_found; //j, 
    float confidence_score = 0.0;

    if (ptype >= SEEK_PTYPE_MAX) {
	*result = 0;
	err = -E_VPE_MP4_SEEK_UNDEFINED_PROVIDER;
	goto error;
    }
    if (ptype == 0) {
	*result = 0;
	goto error;
    }

    *result = 0;
    spd = &g_spd[ptype];
    for (i = 0; i < spd->num_lists; i++) {
	num_boxes = spd->num_boxes_in_list[i];
	mp4_search_box_list(moov->data + 8,
			    moov->size - 8,
			    spd->box_id_list[i],
			    num_boxes,
			    &num_found);
	if (num_found == num_boxes) {
	    confidence_score += spd->confidence[i];
	}
    }

    if (confidence_score * 100 == 100) {
	*result = 1;
    }
    
    return err;    
 error:
    return err;
    
}

			    

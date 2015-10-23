#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "nkn_vpe_flv_parser.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_memalloc.h"


const char* yamdi_meta_name[]= { "duration",
				 "width",
				 "height",
				 "videodatarate",
				 "audiodatarate",
				 "framerate",
				 "filesize" }; // the bytelength in yt_onmetadata

const char* yt_meta_name[]={ "onMetaData",
			     "duration",
			     "starttime",      // yamdi duration
			     "totalduration",  // yamdi duration
			     "width",          // yamdi width
			     "height",         // yamdi height
			     "videodatarate",  // yamdi videodatarate
			     "audiodatarate",  // yamdi audiodatarate
			     "totaldatarate",  // (videosize+audiosize+sizeof(yt_onmetadata))/duration
			     "framerate",      // yamdi framerate
			     "bytelength",     // size of this new flv file
			     "canseekontime",  // true
			     "sourcedata",
			     "purl",
			     "pmsg",
			     "httphostheader" };



io_handlers_t iohldr = {
    .ioh_open = nkn_vpe_memopen,
    .ioh_read = nkn_vpe_memread,
    .ioh_tell = nkn_vpe_memtell,
    .ioh_seek = nkn_vpe_memseek,
    .ioh_close = nkn_vpe_memclose,
    .ioh_write = nkn_vpe_memwrite
};

static inline int32_t flv_parser_setup_data_span(flv_parser_t *fp,
						 uint8_t state, 
						 size_t  size, 
						 size_t pos);
static inline int32_t flv_check_data_span(void *iod, 
					  size_t bytes_to_read, 
					  size_t size);

flv_parser_t* 
flv_init_parser(void)
{
    flv_parser_t *fp;

    /* allocate the FLV parser state */
    fp = (flv_parser_t *)nkn_calloc_type(1, sizeof(flv_parser_t), 
					 mod_vpe_flv_parser_t);
    if(!fp) {
	goto error;
    }

    /* allocate the FLV header */
    fp->fh = (nkn_vpe_flv_header_t *)nkn_calloc_type (1,
						      sizeof(nkn_vpe_flv_header_t),
						      mod_vpe_flv_parser_t);
    if(!fp->fh) {
	goto error;
    }

    /* allocate for FLV tag */
    fp->ft = (nkn_vpe_flv_tag_t *)nkn_calloc_type(1, 
						  sizeof(nkn_vpe_flv_tag_t),
						  mod_vpe_flv_parser_t);
    if(!fp->ft) {
	goto error;
    }

    /* allocate for FLV seek params */
    fp->sp = (flv_seek_params_t*)nkn_calloc_type(1, 
						 sizeof(flv_seek_params_t),
						 mod_vpe_flv_parser_t);
    if(!fp->sp) {
	goto error;
    }

    /* allocate for FLV meta tag */
    fp->mt = (flv_meta_tag_t*)nkn_calloc_type(1, 
					      sizeof(flv_meta_tag_t),
					      mod_vpe_flv_parser_t);
    if(!fp->mt) {
	goto error;
    }

    /*
     * allocate for FLV XMP metadata tag
     * some FLV file may contain the XMP metadata tag
     */
    fp->xmp_mt = (flv_meta_tag_t*)nkn_calloc_type(1,
						  sizeof(flv_meta_tag_t),
						  mod_vpe_flv_parser_t);
    if(!fp->xmp_mt) {
	goto error;
    }

    fp->ds = (data_span_t*)nkn_calloc_type(1, 
					   sizeof(data_span_t), 
					   mod_vpe_flv_parser_t);
    if(!fp->ds) {
	goto error;
    }

    return fp;
 error:
    if (fp) {
	if (fp->fh) {
	    free(fp->fh);
	}
	if (fp->ft) {
	    free(fp->ft);
	}
	if (fp->sp) {
	    free(fp->sp);
	}
	if (fp->mt) {
	    free(fp->mt);
	}
	if (fp->xmp_mt) {
	    free(fp->xmp_mt);
	}
	if (fp->ds) {
	    free(fp->ds);
	}
	free(fp);
    }
    return NULL;
}

int32_t
flv_parse_header(flv_parser_t *fp, uint8_t *data, size_t size)
{
    fp->iod = iohldr.ioh_open((void*)data, "rb", size);
    if (fp->iod == NULL) {
	return E_VPE_FLV_DATA_INSUFF;
    }
    if(size < 13) {
	return E_VPE_FLV_DATA_INSUFF;
    }
    
    iohldr.ioh_read((void*)(fp->fh), 1, sizeof(nkn_vpe_flv_header_t), fp->iod);
    fp->parsed_bytes_in_block += 13;
    fp->bytes_to_next_tag = 0;

    iohldr.ioh_seek((void*)(fp->iod), 4, SEEK_CUR);
    return fp->parsed_bytes_in_block;
}


int32_t 
flv_set_seekto(flv_parser_t *fp, double seek_to, const char *meta_tag_modifier_list[], int32_t n_items_to_modify)
{
    flv_meta_tag_modifier_t *mtm;
    int32_t i;

    fp->sp->seek_to = (int32_t)(seek_to);
    mtm = fp->sp->mtm =
    (flv_meta_tag_modifier_t*)nkn_calloc_type(n_items_to_modify, 
					      sizeof(flv_meta_tag_modifier_t),
					      mod_vpe_flv_meta_tag_modifer_t);
    if (!mtm) {
	return -1;
    }
    fp->sp->n_items_to_modify = n_items_to_modify;
    for(i = 0; i < n_items_to_modify; i++) {
	mtm[i].name = (char*)meta_tag_modifier_list[i];
	mtm[i].pos = 0;
	mtm[i].type = 0;
    }

    return 0;
}

int32_t
flv_parse_data(flv_parser_t *fp, uint8_t *data, size_t size)
{

    size_t bytes_to_read, bytes_remaining,
	   pos;
    uint8_t vd, ad;
    int32_t vts, ats, rv, i;
    int64_t body_length;
    flv_seek_params_t *sp;
    int32_t mtpos;
    char metadatatag_name[13];

    sp = fp->sp;
    vd = 0;
    vts = 0;
    rv = E_VPE_FLV_DATA_INSUFF;
    body_length = 0;

    if(!fp->iod) { 
	/*if all data has been parsed in the prev call*/
	fp->iod = iohldr.ioh_open((void*)data, "rb", size);
	if (fp->iod == NULL) { /* fail to open the bitstream */
	    return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
	}
    }

    if(fp->ds->data_span_flag) {
	/* flv tag in previous block spans to this block */

	uint8_t *p_dst;
	p_dst = (uint8_t*)(fp->ft);

        if (size < fp->ds->bytes_in_next_span) {
            /*
             * With data_span_flag, current parse will start from the 32K block data boundary
             * The only condition of size < fp->ds->bytes_in_next_span is for the last data block
             * If this last data block is smaller than 16 bytes,
             * It must be an invalid file, and just exit.
             */
            return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;/* have to reuse this error code */
        }
	/* read remaining bytes of the flv_tag */
	iohldr.ioh_read((void*)(p_dst + fp->ds->bytes_in_curr_span),
		     1, fp->ds->bytes_in_next_span, (void*)(fp->iod));

	/* reset data spanning info */
	fp->ds->bytes_in_next_span = 0;
	fp->ds->bytes_in_curr_span = 0;
	fp->ds->data_span_flag = 0;


	bytes_to_read = sizeof(nkn_vpe_flv_tag_t) + 1;
	goto continue_parsing;
    }


    if(fp->bytes_to_next_tag >= 0)  {

	/* restart parsing in this block */
	if(fp->bytes_to_next_tag < (int64_t)size) {
	    /* next tag exists in this block */
	    iohldr.ioh_seek((void*)fp->iod, fp->bytes_to_next_tag, SEEK_CUR);
	} else {
	    /* next tag is not in this block; nothing to parse in this data block */
	    fp->bytes_to_next_tag -= size;

	    /* reset the bitstream to allow it to commence parsing from the next block */
	    iohldr.ioh_close((void*)(fp->iod));
	    fp->parsed_bytes_in_file += size;
	    fp->iod = NULL;
	    return rv;
	}

	/* init variables for this run */
	bytes_to_read = sizeof(nkn_vpe_flv_tag_t) + 1;
	/*
	 * to differentiate onmetadata and XMP metadata tag, we need 13 bits
	 * But we do not change the bytes_to_read, as it will effect the entire logic
	 */
	pos = iohldr.ioh_tell(fp->iod);

	/* start parsing till the end of this block */
	do {
	    bytes_remaining = size - nkn_vpe_memtell(fp->iod);
	    if(bytes_to_read <= bytes_remaining) { /* if the video tag is within bounds */
		iohldr.ioh_read((void*)(fp->ft), 1, sizeof(nkn_vpe_flv_tag_t), fp->iod);
	    continue_parsing:
		body_length = nkn_vpe_convert_uint24_to_uint32(fp->ft->body_length);
		switch(fp->ft->type) {
		    case NKN_VPE_FLV_TAG_TYPE_META:
			if(flv_check_data_span(fp->iod, body_length +
					       4, size)) {
			    //flv_parser_setup_data_span(fp, FLV_PARSER_AAC_SI_SPAN);
			    return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;
			}

			if ((bytes_remaining - bytes_to_read) < 12) {
			    /* prevent the invalid memory visit*/
			    return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;
			}
			iohldr.ioh_read((void*)(metadatatag_name), 1, 13, fp->iod);

			if (!memcmp(metadatatag_name + 3, "onMetaData", 10)) { /* onMetaData tag */
			    /* undo the nanme check for parser to continue */
			    iohldr.ioh_seek(fp->iod, -13, SEEK_CUR);

			    /* read meta tag */
			    fp->mt->data =
				(uint8_t*)nkn_malloc_type(body_length + 4
							  /*tag size*/,
							  mod_vpe_flv_generic_buf);
			    if (fp->mt->data == NULL) {
				return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
			    }
			    fp->mt->size = body_length + 4;
			    iohldr.ioh_read((void*)fp->mt->data, 1, fp->mt->size, (void*)(fp->iod));
			    memcpy(&fp->mt->tag, fp->ft, sizeof(nkn_vpe_flv_tag_t));

			    /* undo the meta tag read for parser to continue */
			    iohldr.ioh_seek((void*)fp->iod, -(body_length+4), SEEK_CUR);

			    int32_t rpos;
			    rpos = 0;
			    for(i = 0; i < fp->sp->n_items_to_modify; i++) { 
				mtpos = (rpos = flv_find_meta_tag(fp->mt->data, fp->mt->size,
								  (uint8_t*)fp->sp->mtm[i].name));

				if(mtpos) {
				    uint64_t* tmp_meta_value = NULL;
				    fp->sp->mtm[i].pos = mtpos;
				//fp->sp->mtm[i].val.i64 = *(uint64_t*)(flv_read_meta_value(fp->mt->data + rpos, 8, 0));
				    tmp_meta_value = (uint64_t*)(flv_read_meta_value(fp->mt->data + rpos,
										     8, 0));
				    if (tmp_meta_value == NULL) {
					return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
				    }
				    fp->sp->mtm[i].val.i64 = *tmp_meta_value;
				    free(tmp_meta_value);
				    tmp_meta_value = NULL;
				}
			    }
			}else { /* XMP metadata, we will just copy it */
			    /* undo the nanme check for parser to continue */
			    iohldr.ioh_seek(fp->iod, -13, SEEK_CUR);

			    /* read meta tag */
			    fp->xmp_mt->data = (uint8_t*)nkn_malloc_type(body_length + 4
									 /* tag size */,
									 mod_vpe_flv_generic_buf);
			    if (fp->xmp_mt->data == NULL) {
				return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
			    }
			    fp->xmp_mt->size = body_length + 4;
                            iohldr.ioh_read((void*)fp->xmp_mt->data, 1, fp->xmp_mt->size, (void*)(fp->iod));
			    memcpy(&fp->xmp_mt->tag, fp->ft, sizeof(nkn_vpe_flv_tag_t));

			    /* undo the meta tag read for parser to continue */
                            iohldr.ioh_seek((void*)fp->iod, -(body_length+4), SEEK_CUR);
			}

			break;
			
		    case NKN_VPE_FLV_TAG_TYPE_VIDEO:

			/* read next tag but check if data spans buffer*/
			iohldr.ioh_read((void*)(&vd), 1, 1, fp->iod);
			vts = nkn_vpe_flv_get_timestamp(*(fp->ft));

			if(nkn_vpe_flv_video_frame_type(vd) == NKN_VPE_FLV_VIDEO_FRAME_TYPE_KEYFRAME) {
			    //printf("timestamp of keyframe %d\n", vts);

			    if((nkn_vpe_flv_video_codec_id(vd) == NKN_VPE_FLV_VIDEO_CODEC_AVC) &&
			       (vts == 0) && (!fp->avcc)) {

				/* read the SPS+PPS+ {options SEI} payload if codec is AVC */
				int32_t tag_size = 0;
				uint8_t *p_avcc = NULL;

				if(flv_check_data_span(fp->iod,
						       body_length - 1, size)) {
				    //flv_parser_setup_data_span(fp, FLV_PARSER_AVCC_SPAN);
				    return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;
				}

				/* size = body_length + tag header size + tag size field */
				fp->avcc_size = sizeof(nkn_vpe_flv_tag_t) + body_length + sizeof(uint32_t);

				/* compute the tag size */
				tag_size = body_length + sizeof(nkn_vpe_flv_tag_t);
				
				/* allocate for the SPS+PPS+{optional SEI} */
				p_avcc = fp->avcc =
				(uint8_t*)nkn_malloc_type(fp->avcc_size, mod_vpe_flv_avcc_buf);
				if (!p_avcc) {
				    return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
				}
				/* copy flv tag header */
				memcpy(p_avcc, fp->ft, sizeof(nkn_vpe_flv_tag_t));
				p_avcc  += sizeof(nkn_vpe_flv_tag_t);

				/* copy video data element */
				memcpy(p_avcc, &vd, 1);
				p_avcc++;

				/* copy the actual SPS+PPS payload */
				iohldr.ioh_read((void*)(p_avcc), 1, body_length - 1, fp->iod);
				p_avcc += (body_length - 1);

				/* copy the tag size */
				tag_size = nkn_vpe_swap_uint32(tag_size);
				memcpy(p_avcc, &tag_size, sizeof(uint32_t));
				
				/* reset the position of the IO descriptor */
				iohldr.ioh_seek(fp->iod, -(body_length - 1), SEEK_CUR);

			    }
			       
			    /* hit KEYFRAME */
			    if(vts >= (sp->seek_to * 1000)) {
				/* KEYFRAME match for given seek pos */
				uint32_t new_ts;
				mval v;
				int32_t k;
				uint64_t tmp;

				if (!fp->mt->data) {
				    /* We dont have the meta data to proceed
				     * further with the seek request
				     */
				    return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;
				}

				sp->seek_pos = fp->parsed_bytes_in_file + pos;
				sp->seek_vts = vts;
				
				for(k = 0; k < sp->n_items_to_modify; k++) {
				    if(strcmp(sp->mtm[k].name,"duration") == 0) {
					   v.d = sp->mtm[k].val.d - ((double)vts/1000.0);
					   tmp = nkn_vpe_swap_uint64((uint64_t)v.i64);
					   memcpy(fp->mt->data + sp->mtm[k].pos,
						  &tmp, 8);
					   continue;
				    }
				    if(strcmp(sp->mtm[k].name,"starttime") == 0) {
					   v.d =  ((double)vts/1000.0);
					   tmp = nkn_vpe_swap_uint64((uint64_t)v.i64);
					   memcpy(fp->mt->data + sp->mtm[k].pos,
						  &tmp, 8);
					   continue;
				    }
				    if(strcmp(sp->mtm[k].name,"bytelength") == 0) {
					v.d =  sp->mtm[k].val.d - (double)(sp->seek_pos);
					tmp = nkn_vpe_swap_uint64((uint64_t)v.i64);
					memcpy(fp->mt->data + sp->mtm[k].pos,
					       &tmp, 8);
					continue;
				    }
				}
				       
				/* modify timestamps of avcc and aac_si */
				if(fp->avcc) {
				    new_ts = nkn_vpe_swap_uint32(vts);
				    memcpy(fp->avcc + 4, ((uint8_t*)(&new_ts)) + 1, 3);
				}

				if(fp->aac_si) {
				    new_ts = nkn_vpe_swap_uint32(vts);
				    memcpy(fp->aac_si + 4, ((uint8_t*)(&new_ts)) + 1, 3);
				}

				return sp->seek_pos;
			    }
			} 
			
			/* undo the video data read for parser to continue */
			iohldr.ioh_seek((void*)fp->iod, -1, SEEK_CUR);
			break;
		    case NKN_VPE_FLV_TAG_TYPE_AUDIO:

			/* read next tag */
			iohldr.ioh_read((void*)(&ad), 1, 1, fp->iod);
			ats = nkn_vpe_flv_get_timestamp(*(fp->ft));

			if((nkn_vpe_flv_audio_codec_id(ad) == NKN_VPE_FLV_AUDIO_CODEC_AAC) &&
			   (!fp->aac_si) ) {
			    				/* read the SPS+PPS+ {options SEI} payload if codec is AVC */
				int32_t tag_size = 0;
				uint8_t *p_aac_si;

				/* size = body_length + tag header size + tag size field */
				fp->aac_si_size = sizeof(nkn_vpe_flv_tag_t) + body_length + sizeof(uint32_t);

				if(flv_check_data_span(fp->iod,
						       body_length - 1, size)) {
				    //flv_parser_setup_data_span(fp, FLV_PARSER_AAC_SI_SPAN);
				    return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;
				}

				/* compute the tag size */
				tag_size = body_length + sizeof(nkn_vpe_flv_tag_t);
				
				/* allocate for the SPS+PPS+{optional SEI} */
				p_aac_si = fp->aac_si =
				(uint8_t*)nkn_malloc_type(fp->avcc_size, mod_vpe_flv_avcc_buf);
				if (!p_aac_si) {
				    return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
				}

				/* copy flv tag header */
				memcpy(p_aac_si, fp->ft, sizeof(nkn_vpe_flv_tag_t));
				p_aac_si  += sizeof(nkn_vpe_flv_tag_t);

				/* copy video data element */
				memcpy(p_aac_si, &ad, 1);
				p_aac_si++;

				/* copy the actual SPS+PPS payload */
				iohldr.ioh_read((void*)(p_aac_si), 1, body_length - 1, fp->iod);
				p_aac_si += (body_length - 1);

				/* copy the tag size */
				tag_size = nkn_vpe_swap_uint32(tag_size);
				memcpy(p_aac_si, &tag_size, sizeof(uint32_t));
				
				/* reset the position of the IO descriptor */
				iohldr.ioh_seek(fp->iod, -(body_length - 1), SEEK_CUR);
			}

			/* undo the video data read for parser to continue */
			iohldr.ioh_seek((void*)fp->iod, -1, SEEK_CUR);
			break;
		} 
		
		/* check if the next tag is within bounds */
		if((iohldr.ioh_tell(fp->iod) + body_length + 4) >= size) {
		    /* next tag is not within this block; reset bitstream for next parser run*/
		    fp->bytes_to_next_tag = body_length - (size - iohldr.ioh_tell(fp->iod)) + 4 /*sizeof last tag*/;
		    iohldr.ioh_close((void*)(fp->iod));

		    /*indicate that all the data has been consumed */
		    fp->iod = NULL;
		    fp->parsed_bytes_in_file += size;
		    return rv;
		}
		
		/* next tag is in this block, skip to next tag */
		iohldr.ioh_seek((void *)fp->iod, body_length + 4 /*sizeof last tag*/, SEEK_CUR);
		pos = iohldr.ioh_tell(fp->iod);
	    } else {
		/* next tag spans between current and next block */
		size_t span_bytes_remaining;

		/* copy the portion of the flv_tag_header in the current span. Setup to read the 
		 * rest of the data in the next span/block
		 */
		fp->ds->bytes_in_curr_span = span_bytes_remaining = size - pos;
		fp->ds->bytes_in_next_span = sizeof(nkn_vpe_flv_tag_t) - fp->ds->bytes_in_curr_span;
		iohldr.ioh_read((void*)fp->ft, 1, span_bytes_remaining, (void*)(fp->iod));
		iohldr.ioh_close((void*)(fp->iod));

		/* indicate that all the data has been consumed */
		fp->iod = NULL; 
		fp->parsed_bytes_in_file += size;
		
		/* set the data spanning flag */
		fp->ds->data_span_flag = 1;
		
		return rv;
		
	    }	
	}while(pos < size); /* end of parser run */
	fp->parsed_bytes_in_file += size;
    }

    return rv;
}

int32_t
flv_get_meta_tag_info(flv_parser_t *fp, uint8_t *data, size_t size,
		      size_t *meta_tag_offset,
		      size_t *meta_tag_length,
		      int32_t *video_codec,
		      int32_t *audio_codec)
{

    size_t bytes_to_read, bytes_remaining,
	   pos;
    uint8_t vd, ad;
    int32_t vts, ats, rv, i, found;
    int64_t body_length;
    flv_seek_params_t *sp;
    int32_t mtpos;
    char metadatatag_name[13];

    sp = fp->sp;
    vd = 0;
    vts = 0;
    rv = E_VPE_FLV_DATA_INSUFF;
    body_length = 0;
    found = 0;

    if(!fp->iod) {
	/*if all data has been parsed in the prev call*/
	fp->iod = iohldr.ioh_open((void*)data, "rb", size);
	if (fp->iod == NULL) {
	    return -E_VPE_FLV_PARSER_MEM_ALLOC_ERROR;
	}
    }

    if(fp->ds->data_span_flag) {
	/* flv tag in previous block spans to this block */

	uint8_t *p_dst;
	p_dst = (uint8_t*)(fp->ft);

        if (size < fp->ds->bytes_in_next_span) {
            /*
             * With data_span_flag, current parse will start from the 32K block data boundary
             * The only condition of size < fp->ds->bytes_in_next_span is for the last data block
             * If this last data block is smaller than 16 bytes,
             * It must be an invalid file, and just exit.
             */
            return -E_VPE_FLV_PARSER_SPAN_UNHANDLED;/* have to reuse this error code */
	}
	/* read remaining bytes of the flv_tag */
	iohldr.ioh_read((void*)(p_dst + fp->ds->bytes_in_curr_span),
		     1, fp->ds->bytes_in_next_span, (void*)(fp->iod));

	/* reset data spanning info */
	fp->ds->bytes_in_next_span = 0;
	fp->ds->bytes_in_curr_span = 0;
	fp->ds->data_span_flag = 0;

	bytes_to_read = sizeof(nkn_vpe_flv_tag_t) + 1 + 12;
	goto continue_parsing;
    }

    if(fp->bytes_to_next_tag >= 0)  {

	/* restart parsing in this block */
	if(fp->bytes_to_next_tag < (int64_t)size) {
	    /* next tag exists in this block */
	    iohldr.ioh_seek((void*)fp->iod, fp->bytes_to_next_tag, SEEK_CUR);
	} else {
	    /* next tag is not in this block; nothing to parse in this data block */
	    fp->bytes_to_next_tag -= size;

	    /* reset the bitstream to allow it to commence parsing from the next block */
	    iohldr.ioh_close((void*)(fp->iod));
	    fp->parsed_bytes_in_file += size;
	    fp->iod = NULL;
	    return rv;
	}

	/* init variables for this run */
	bytes_to_read = sizeof(nkn_vpe_flv_tag_t) + 1 + 12;
	/* to differentiate onmetadata and XMP metadata tag, we need 13 bits*/
	pos = iohldr.ioh_tell(fp->iod);

	/* start parsing till the end of this block */
	do {
	    bytes_remaining = size - nkn_vpe_memtell(fp->iod);
	    if(bytes_to_read <= bytes_remaining) { /* if the video tag is within bounds */
		iohldr.ioh_read((void*)(fp->ft), 1,
				sizeof(nkn_vpe_flv_tag_t), fp->iod);
	    continue_parsing:
		body_length =
		    nkn_vpe_convert_uint24_to_uint32(fp->ft->body_length);
		switch(fp->ft->type) {
		    case NKN_VPE_FLV_TAG_TYPE_META:
			/* store meta tag offset and pos */
			if ( found == 7 ) {
			    return 0;
			}
			iohldr.ioh_read((void*)(metadatatag_name), 1, 13, fp->iod);
			if (!memcmp(metadatatag_name + 3, "onMetaData", 10)) {
				*meta_tag_offset = pos;
				*meta_tag_length = body_length +
				    sizeof(nkn_vpe_flv_tag_t) + 4;
				found |= 1;
			    }
			iohldr.ioh_seek(fp->iod, -13, SEEK_CUR);
			break;
		    case NKN_VPE_FLV_TAG_TYPE_VIDEO:
			if ( found == 7 ) {
			    return 0;
			}
			iohldr.ioh_read((void*)(&vd), 1, 1, fp->iod);
			if ( nkn_vpe_flv_video_codec_id(vd) ==
			     NKN_VPE_FLV_VIDEO_CODEC_AVC) {
			    *video_codec = 0x00010001; // AVC
			} else if( nkn_vpe_flv_video_codec_id(vd) ==
				   NKN_VPE_FLV_VIDEO_CODEC_ON2_VP6 ) {
			    *video_codec = 0x00000002; // VP6
			} else if( nkn_vpe_flv_video_codec_id(vd) ==
				   NKN_VPE_FLV_VIDEO_CODEC_SORENSEN_H263 ) {
			    *video_codec = 0x00000004;
			}
			iohldr.ioh_seek(fp->iod, -1, SEEK_CUR);
			found |= 2;
			break;
		    case NKN_VPE_FLV_TAG_TYPE_AUDIO:
			if ( found == 7 ) {
			    return 0;
			}
			iohldr.ioh_read((void*)(&ad), 1, 1, fp->iod);
			if ( nkn_vpe_flv_audio_codec_id(ad) ==
			     NKN_VPE_FLV_AUDIO_CODEC_AAC) {
			    *audio_codec = 0x00000001; // AAC
			} else if ( nkn_vpe_flv_audio_codec_id(ad) ==
				    NKN_VPE_FLV_AUDIO_CODEC_MP3 ) {
			    *audio_codec = 0x00000002; // MP3
			}
			iohldr.ioh_seek(fp->iod, -1, SEEK_CUR);
			found |= 4;
			break;
		}

		/* check if the next tag is within bounds */
		if((iohldr.ioh_tell(fp->iod) + body_length + 4) >= size) {
		    /* next tag is not within this block; reset bitstream for next parser run*/
		    fp->bytes_to_next_tag = body_length - (size - iohldr.ioh_tell(fp->iod)) + 4 /*sizeof last tag*/;
		    iohldr.ioh_close((void*)(fp->iod));

		    /*indicate that all the data has been consumed */
		    fp->iod = NULL;
		    fp->parsed_bytes_in_file += size;
		    return rv;
		}

		/* next tag is in this block, skip to next tag */
		iohldr.ioh_seek((void *)fp->iod, body_length + 4 /*sizeof last tag*/, SEEK_CUR);
		pos = iohldr.ioh_tell(fp->iod);
	    } else {
		/* next tag spans between current and next block */
		size_t span_bytes_remaining;

		/* copy the portion of the flv_tag_header in the current span. Setup to read the
		 * rest of the data in the next span/block
		 */
		fp->ds->bytes_in_curr_span = span_bytes_remaining = size - pos;
		fp->ds->bytes_in_next_span = sizeof(nkn_vpe_flv_tag_t) - fp->ds->bytes_in_curr_span;
		iohldr.ioh_read((void*)fp->ft, 1, span_bytes_remaining, (void*)(fp->iod));
		iohldr.ioh_close((void*)(fp->iod));

		/* indicate that all the data has been consumed */
		fp->iod = NULL;
		fp->parsed_bytes_in_file += size;

		/* set the data spanning flag */
		fp->ds->data_span_flag = 1;

		return rv;

	    }
	}while(pos < size); /* end of parser run */
	fp->parsed_bytes_in_file += size;
    }

    return rv;
}

void
flv_parser_cleanup(flv_parser_t *fp)
{

    if(!fp) {
	return;
    }

    if(fp->fh) {
	free(fp->fh);
    }

    if(fp->ft) {
	free(fp->ft);
    }

     if(fp->iod) {
	iohldr.ioh_close((void*)fp->iod);
     }

    if(fp->sp) {
	if(fp->sp->mtm) {
	    free(fp->sp->mtm);
	}
	free(fp->sp);
    }

    if(fp->mt) {
	if(fp->mt->data) {
	    free(fp->mt->data);
	}
	free(fp->mt);
    }

    if (fp->xmp_mt) {
	if (fp->xmp_mt->data) {
	    free(fp->xmp_mt->data);
	}
	free(fp->xmp_mt);
    }

    if(fp->avcc) {
	free(fp->avcc);
    }

    if(fp->aac_si) {
	free(fp->aac_si);
    }

    if(fp->ds) {
	free(fp->ds);
    }

    if(fp) {
	free(fp);
    }
    
}

int32_t
flv_find_meta_tag(uint8_t *mt_start, int32_t mt_size, uint8_t* search_str)
{
    uint8_t *p_src, field_type;
    bitstream_state_t *bs;
    uint32_t str_size_specfier, len;
    char *field;
    int32_t skip_bytes, rv;

    p_src = mt_start;
    skip_bytes = 0;
    rv = 0;
    bs = bio_init_bitstream(p_src, mt_size);
    if (bs == NULL) {
	goto error;
    }
    str_size_specfier = 0;

    // read the string size specifier
    bio_aligned_read(bs, (uint8_t*)(&str_size_specfier), sizeof(char));

    len = 0;
    while(bio_get_pos(bs) != (uint32_t)mt_size) {

	/* read the length of the field name string */
	bio_aligned_read(bs, (uint8_t*)(&len), str_size_specfier);
	len = nkn_vpe_swap_uint16(len);

	field = (char *)nkn_malloc_type(len, mod_vpe_flv_generic_buf);
	if (!field) {
	    goto error;
	}
	/* read the field name */
	bio_aligned_read(bs, (uint8_t*)(field), len);
	
	/* read the field type */
	bio_aligned_read(bs, (uint8_t*)(&field_type), sizeof(char));
	
	switch(field_type) {
	    case 0:
		if(!memcmp(field, search_str, len)) {
		    rv = bio_get_pos(bs);
		    free(field);
		    goto cleanup;
		}
		skip_bytes = 8;
		break;
	    case 1:
		skip_bytes = 1;
		break;
	    case 2:
		bio_aligned_read(bs, (uint8_t*)(&skip_bytes), 2);
		skip_bytes = nkn_vpe_swap_uint16(len);
		break;
	    case 3:
	    case 8:
		skip_bytes = 4;
		break;
	}
	bio_aligned_seek_bitstream(bs, skip_bytes, SEEK_CUR);
	free(field);
    }

 cleanup:
    bio_close_bitstream(bs);
    return rv;
 error:
    if (bs) {
	bio_close_bitstream(bs);
    }
    return 0;
}

void*
flv_read_meta_value(uint8_t *data, size_t size, uint8_t type)
{
    bitstream_state_t *bs;
    uint64_t *dval;
    void *rv;

    bs = bio_init_bitstream(data, size);
    if (bs == NULL) {
	goto error;
    }
    switch (type) {
	case 0:
	    dval = (uint64_t*)nkn_calloc_type(1, 
					      sizeof(uint64_t),
					      mod_vpe_flv_generic_buf);
	    if (dval == NULL) {
		goto error;
	    }
	    bio_aligned_read(bs, (uint8_t*)(dval), sizeof(uint64_t));
	    *dval = nkn_vpe_swap_uint64((uint64_t)(*dval));
	    rv = (void*)(dval);
	    break;
    }

    bio_close_bitstream(bs);
    return rv;
 error:
    if (bs) {
	bio_close_bitstream(bs);
    }
    return NULL;
}

static inline int32_t flv_check_data_span(void *iod, size_t bytes_to_read, size_t size)
{
    size_t bytes_remaining;
    bytes_remaining = size - nkn_vpe_memtell(iod);
    if(bytes_to_read <= bytes_remaining) { 
	return 0;
    }

    return 1;
}

static inline int32_t flv_parser_setup_data_span(flv_parser_t *fp,
						 uint8_t state, 
						 size_t size, 
						 size_t pos)
{
    /* next tag spans between current and next block */
    size_t span_bytes_remaining;
    
    /* copy the portion of the flv_tag_header in the current span. Setup to read the 
     * rest of the data in the next span/block
     */
    fp->ds->bytes_in_curr_span = span_bytes_remaining = size - pos;
    fp->ds->bytes_in_next_span = sizeof(nkn_vpe_flv_tag_t) - fp->ds->bytes_in_curr_span;
    iohldr.ioh_read((void*)fp->ft, 1, span_bytes_remaining, (void*)(fp->iod));
    iohldr.ioh_close((void*)(fp->iod));
    
    /* indicate that all the data has been consumed */
    fp->iod = NULL; 
    fp->parsed_bytes_in_file += size;
    
    /* set the data spanning flag */
    fp->ds->data_span_flag = 1;
    fp->span_state = state;
    
    return 0;
}

inline uint24_t nkn_vpe_convert_uint32_to_uint24(uint32_t l) 
{
  uint24_t r;
  r.b0 = (uint8_t)((l & 0x00FF0000U) >> 16);
  r.b1 = (uint8_t)((l & 0x0000FF00U) >> 8);
  r.b2 = (uint8_t) (l & 0x000000FFU);
  return r;
}


/*
 * BZ 8382
 * youtube FLV onmetadata injector
 * we use the yamdi to inject several onmetadata properties into onmetadata.
 * but the youtube FLV onmetadata is different from the yamdi injected one
 *
 * yamdi has onmetadata
 *       char   *metaDataCreator;
 *       uint8_t hasKeyFrames;
 *       uint8_t hasVideo;
 *       uint8_t hasAudio;
 *       uint8_t hasMetaData;
 *       uint8_t canSeekToEnd;
 *       double duration;
 *       double dataSize;
 *       double videoSize;
 *       double frameRate;
 *       double videoDataRate;
 *       double videoCodecId;
 *       double width;
 *       double height;
 *       double audioSize;
 *       double audioDataRate;
 *       double audioCodecId;
 *       double audioSampleRate;
 *       double audioSampleSize;
 *       uint8_t stereo;
 *       double fileSize;
 *       double lastTimestamp;
 *       double lastKeyFrameTimestamp;
 *       double lastKeyFrameLocation;
 *       double *filePositions; // these two are for the keyframes
 *       double *times;  // these two are for the keyframes

 * youtube has onmetadata
 *        double duration;// for yt seek
 *        double startTime;// for yt seek
 *        double totalDuration;
 *        double width;
 *        double height;
 *        double videoDataRate;
 *        double audioDataRate;
 *        double totalDataRate;
 *        double frameRate;
 *        double byteLength;// for yt seek
 *        double canSeekOnTime;
 *        char   *sourceData;
 *        char   *purl;
 *        char   *pmsg;
 *        char   *httpHostHeader;
 *
 *
 *
 *  Compared with the above two, we can find that youtube onmetadata only use
 *  certain properities that yamdi injected.
 *  we need to read the below value from yamdi
 *  const char* yamdi_onmetadata[]= { "duration",
 *				  "width",
 *				  "height",
 *				  "videodatarate",
 *				  "audiodatarate",
 *				  "framerate",
 *				  "filesize" }; // is the bytelength in yt_onmetadata
 *  and they are used to generated the following values.
 *  const char* yt_onmetadata[]={ "onMetaData",
 *			      "duration",          // yamdi duration
 *			      "totalduration",     // yamdi duration
 *			      "width",             // yamdi width
 *			      "height",            // yamdi height
 *			      "videodatarate",     // yamdi videodatarate
 *			      "audiodatarate",     // yamdi audiodatarate
 *			      "totaldatarate",     // (videosize+audiosize+sizeof(ytonmetadata))/duration
 *			      "framerate",         // yamdi framerate
 *			      "bytelength",        // size of this new flv file
 *			      "canseekontime",    // true
 *			      "sourcedata",
 *			      "purl",
 *			      "pmsg",
 *			      "httphostheader" };
 *
 */


/*
 * parser onmetadata of yamdi generated
 * do yamdi2yt convert
 * create the yt onmetadata
 */
int32_t
yt_flv_onmetadata_convert(uint8_t  *mt_ori_start,
			  uint32_t  mt_ori_size,
			  uint8_t  *mt_modi_start,
			  uint32_t  mt_modi_size)
{
    flv_yamdi_onmetadata_tag_t yamdi_onmetadata;
    flv_yt_onmetadata_tag_t yt_onmetadata;

    yt_flv_yamdi_onmetadata_parse(mt_ori_start, mt_ori_size, &yamdi_onmetadata);

    yt_flv_yamdi2yt_onmetadata(&yamdi_onmetadata, mt_ori_size,
			       &yt_onmetadata, mt_modi_size);

    yt_flv_yt_onmetadata_create(mt_modi_start, mt_modi_size,&yt_onmetadata);

    return 1;
}


/*
 * read the yamdi onmetadata, and obtain the needed properities for yt_onmetadata
 * 1. read the onmetadata string
 * 2. read the ecmaarry
 * 3. read the needed properities
 *
 */

int32_t
yt_flv_yamdi_onmetadata_parse(uint8_t                    *mt_ori_start,
			      uint32_t                    mt_ori_size,
			      flv_yamdi_onmetadata_tag_t *yamdi_onmetadata)
{
    uint8_t *p_src, field_type;
    bitstream_state_t *bs_in;
    uint32_t str_size_specfier, len;
    char *field;
    int32_t skip_bytes, rv;
    /* we only need 7 yamdi onmetadata properities
       defined in const char* yamdi_meta_name[]
    */
    int32_t yamdi_need_num = 0;
    p_src = mt_ori_start;
    skip_bytes = 0;
    rv = 0;
    bs_in = bio_init_bitstream(p_src, mt_ori_size);
    str_size_specfier = 0;

    // read the string size specifier
    bio_aligned_read(bs_in, (uint8_t*)(&str_size_specfier), sizeof(char));

    len = 0;
    while(bio_get_pos(bs_in) != (uint32_t)mt_ori_size) {

	/* read the length of the field name string */
	bio_aligned_read(bs_in, (uint8_t*)(&len), str_size_specfier);
	len = nkn_vpe_swap_uint16(len);

	field = (char *)nkn_malloc_type(len, mod_vpe_flv_generic_buf);
	if (!field) {
	    return -1;
	}
	/* read the field name */
	bio_aligned_read(bs_in, (uint8_t*)(field), len);

	/* read the field type */
	bio_aligned_read(bs_in, (uint8_t*)(&field_type), sizeof(char));

	switch(field_type) {
	    case 0:
		yamdi_need_num +=
				yt_flv_yamdi_find_useful_meta(bs_in, field, len,
							      yamdi_onmetadata, &skip_bytes);
	        //skip_bytes is decided in yt_flv_yamdi_find_useful_meta
		break;
	    case 1:
		skip_bytes = 1;
		break;
	    case 2:
		bio_aligned_read(bs_in, (uint8_t*)(&skip_bytes), 2);
		skip_bytes = nkn_vpe_swap_uint16(skip_bytes);
		break;
	    case 3:
	    case 8:
		skip_bytes = 4;
		break;
	}
	bio_aligned_seek_bitstream(bs_in, skip_bytes, SEEK_CUR);
	free(field);
	if (yamdi_need_num == YAMDI_META_NEED_NUM) { // found all the information we need
	    break;
	}

    }

    bio_close_bitstream(bs_in);

    return yamdi_need_num;
}

int32_t
yt_flv_yamdi_find_useful_meta(bitstream_state_t          *bs_in,
			      char                       *field,
			      uint32_t                    len,
			      flv_yamdi_onmetadata_tag_t *yamdi_onmetadata,
			      int32_t                    *skip_bytes)
{
    int i;
    int meta_found = 0;
    uint64_t temp;
    mval v;
    for(i=0; i < 7; i++) {
	if (!memcmp(field, yamdi_meta_name[i], len)) {
	    //this properties is what we need
	    meta_found++;
	    //read the valaue
	    bio_aligned_read(bs_in, (uint8_t*)(&temp), 8);
	    v.i64 = nkn_vpe_swap_uint64(temp);

	    switch (i) {
		case 0:
		    yamdi_onmetadata->duration = v.d;
		    break;
		case 1:
		    yamdi_onmetadata->width = v.d;
		    break;
		case 2:
		    yamdi_onmetadata->height = v.d;
		    break;
		case 3:
		    yamdi_onmetadata->videoDataRate = v.d;
		    break;
		case 4:
		    yamdi_onmetadata->audioDataRate = v.d;
		    break;
		case 5:
		    yamdi_onmetadata->frameRate = v.d;
		    break;
		case 6:
		    yamdi_onmetadata->fileSize = v.d;
		    break;
	     }
	} /* if (!memcmp(field, yamdi_meta_name[i], len)) */
    }

    if (meta_found == 1) {
	*skip_bytes = 0; // we have read the 8 bytes double
    }else if (meta_found == 0) {
	*skip_bytes = 8;
    }else {
	*skip_bytes = 0;
	printf("\nthis is error, meta_found cannot be larger than 1\n");
    }

    return meta_found;
}


/*
 *
 *  mapping the yamdi onmetadata properities to youtube onmetadata properities
 *
 */

int32_t
yt_flv_yamdi2yt_onmetadata(flv_yamdi_onmetadata_tag_t *yamdi_onmetadata,
			   uint32_t                    mt_ori_size,
			   flv_yt_onmetadata_tag_t    *yt_onmetadata,
			   uint32_t                    mt_modi_size)

{
    double temp;
    yt_onmetadata->duration      = yamdi_onmetadata->duration;// for yt seek
    yt_onmetadata->startTime     = 0.0;// for yt seek
    yt_onmetadata->totalDuration = yamdi_onmetadata->duration;;
    yt_onmetadata->width         = yamdi_onmetadata->width;
    yt_onmetadata->height        = yamdi_onmetadata->height;
    yt_onmetadata->videoDataRate = yamdi_onmetadata->videoDataRate;

    yt_onmetadata->audioDataRate = yamdi_onmetadata->audioDataRate;

    yt_onmetadata->frameRate     = yamdi_onmetadata->frameRate;
    yt_onmetadata->byteLength    = yamdi_onmetadata->fileSize - mt_ori_size
				 + YT_ONMETADATA_SIZE; // for yt seek;
    // 11 bytes for tag header size, 4 bytes for the following previoustagsize
    temp                         = yt_onmetadata->byteLength - FLV_HEADER_SIZE
				 - FLV_TAG_HEADER_SIZE - mt_modi_size 
				 - FLV_PREVIOUS_TAG_SIZE;
    yt_onmetadata->totalDataRate = (temp* 8.0 / 1024.0) / yamdi_onmetadata->duration;

    yt_onmetadata->canSeekOnTime = 1;

    yt_onmetadata->sourceData.DataLen = 32;
    yt_onmetadata->sourceData.Data = (char*) nkn_malloc_type(yt_onmetadata->sourceData.DataLen,
							     mod_vpe_flv_generic_buf);
    if(yt_onmetadata->sourceData.Data == NULL) {
	printf("\nfail to allocate mem for yt_onmetadata->sourceData.\n");
	return -1;
    }
    memset(yt_onmetadata->sourceData.Data, 0, 32);

    yt_onmetadata->purl.DataLen = 128;
    yt_onmetadata->purl.Data = (char*) nkn_malloc_type(yt_onmetadata->purl.DataLen,
						       mod_vpe_flv_generic_buf);
    if(yt_onmetadata->purl.Data == NULL) {
	printf("\nfail to allocate mem for yt_onmetadata->sourceData.\n");
	return -1;
    }
    memset(yt_onmetadata->purl.Data, 0, 128);

    yt_onmetadata->pmsg.DataLen = 128;
    yt_onmetadata->pmsg.Data = (char*) nkn_malloc_type(yt_onmetadata->pmsg.DataLen,
						       mod_vpe_flv_generic_buf);
    if(yt_onmetadata->pmsg.Data == NULL) {
	printf("\nfail to allocate mem for yt_onmetadata->sourceData.\n");
	return -1;
    }
    memset(yt_onmetadata->pmsg.Data, 0, 128);

    yt_onmetadata->httpHostHeader.DataLen = 256;
    yt_onmetadata->httpHostHeader.Data = (char*) nkn_malloc_type(yt_onmetadata->httpHostHeader.DataLen,
								 mod_vpe_flv_generic_buf);
    if(yt_onmetadata->httpHostHeader.Data == NULL) {
	printf("\nfail to allocate mem for yt_onmetadata->sourceData.\n");
	return -1;
    }
    memset(yt_onmetadata->httpHostHeader.Data, 0, 256);

    return 1;
}

/*
 * write the youtube onmetadata properities into a buffer according to the FLV spec
 *
 */
int32_t
yt_flv_yt_onmetadata_create(uint8_t                 *mt_modi_start,
			    uint32_t 		     mt_modi_size,
			    flv_yt_onmetadata_tag_t *yt_onmetadata)
{
    bitstream_state_t *bs_out;
    memset(mt_modi_start, 0, mt_modi_size);
    bs_out = iohldr.ioh_open((char*)(mt_modi_start), "wb", mt_modi_size);

    //write "onmetadata" string
    yt_flv_write_buf_dataval_str(bs_out, yt_meta_name[0], strlen(yt_meta_name[0]));

    //write ecmaarry
    yt_flv_write_buffer_scriptdata_ecmaarry(bs_out, YT_ECMAAARRAYLEN,
					     yt_onmetadata);
    iohldr.ioh_close((void*)(bs_out));

    return 1;
}

/*
 * write the scriptdatavalue (type bool) into buffer
 * 1. write Type
 * 2. write boolValue
 */
int32_t
yt_flv_write_buf_dataval_bool(bitstream_state_t *bs_out,
			      uint8_t            boolValue)
{
    uint8_t type = 1;

    //write Type 1 UI8
    iohldr.ioh_write((void*)(&type), 1, sizeof(uint8_t), (void*)(bs_out));

    //write bool UI8
    boolValue = (boolValue >= 1 ? 1 : 0);
    iohldr.ioh_write((void*)(&boolValue), 1, sizeof(uint8_t), (void*)(bs_out));

    return 1;
}

/*
 * write the scriptdatavalue (type number) into buffer
 * 1. write type
 * 2. write number
 */

int32_t
yt_flv_write_buf_dataval_num(bitstream_state_t *bs_out,
			     double             number)
{
    //write Type 0  UI8
    uint8_t type = 0;
    mval v;
    uint64_t temp;
    iohldr.ioh_write((void*)(&type), 1, sizeof(uint8_t), (void*)(bs_out));

    //write Double  8 bytes
    // a swap here?
    v.d = number;
    temp = nkn_vpe_swap_uint64(v.i64);
    iohldr.ioh_write((void*)(&temp), 1, sizeof(uint64_t), (void*)(bs_out));

    return 1;
}

/*
 * write the scirptdatavalue (type string) into buffer
 * 1. write type
 * 2. write scriptdata string
 */
int32_t
yt_flv_write_buf_dataval_str(bitstream_state_t *bs_out,
			     const char        *strData,
			     uint16_t           strSize)
{
    //write Type 2 UI8
    uint8_t type = 2;
    iohldr.ioh_write((void*)(&type), 1, sizeof(uint8_t), (void*)(bs_out));

    //write string
    yt_flv_write_buf_data_str(bs_out, strData, strSize);

    return 1;
}

/*
 * write the scriptdate string into buffer
 * 1. write string length
 * 2. write string
 */
int32_t
yt_flv_write_buf_data_str(bitstream_state_t *bs_out,
			  const char        *strData,
			  uint16_t           strSize)
{
    //write string
    uint16_t temp;
    //write StringLen
    temp = nkn_vpe_swap_uint16(strSize);
    iohldr.ioh_write((void*)(&temp), 1, sizeof(uint16_t), (void*)(bs_out));

    //weiteStringData
    iohldr.ioh_write((void*)(strData), 1, strSize, (void*)(bs_out));
    // a swap here?
    return 1;

}

/*
 *  write the property (type bool) into buffer
 *  1. write property name (write a string)
 *  2. write property ifself (write a bool scriptdatavalue)
 */
int32_t
yt_flv_write_buf_objprop_bool(bitstream_state_t *bs_out,
			      const char        *propName,
			      uint8_t            boolValue)
{
    //write ProertyName  scriptdata_string
    yt_flv_write_buf_data_str (bs_out, propName, strlen(propName));

    //write PropertyData scriptdatavalue_bool
    yt_flv_write_buf_dataval_bool(bs_out, boolValue);

    return 1;
}

/*
 *  write the property (type number) into buffer
 *  1. write property name (write a string)
 *  2. write property ifself (write a number scriptdatavalue)
 */
int32_t
yt_flv_write_buf_objprop_num(bitstream_state_t *bs_out,
			     const char        *propName,
			     double             number)
{
    //write PropertyName scriptdata_string
    yt_flv_write_buf_data_str (bs_out, propName, strlen(propName));

    //write PropertyData scriptdatavalue_number
    yt_flv_write_buf_dataval_num(bs_out, number);

    return 1;
}

/*
 *  write the property (type string) into buffer
 *  1. write property name (write a string)
 *  2. write property ifself (write a string scriptdatavalue)
 */
int32_t
yt_flv_write_objprop_str(bitstream_state_t *bs_out,
			 const char        *propName,
			 char              *propData,
			 uint16_t           propDataSize)
{
    //write PropertyName scriptdata_string
    yt_flv_write_buf_data_str (bs_out, propName, strlen(propName));

    //write PropertyData scriptdatavalue_number
    yt_flv_write_buf_dataval_str(bs_out, propData, propDataSize);

    return 1;
}

/*
 * write the ecmaarry to buffer
 * for detail of ecmaarry, please refer to FLV spec
 */

int32_t
yt_flv_write_buffer_scriptdata_ecmaarry(bitstream_state_t       *bs_out,
					uint32_t                 ECMAArrayLength,
					flv_yt_onmetadata_tag_t *yt_onmetadata)
{
    uint32_t temp;
    uint8_t terminator[3] = {0x00, 0x00,0x09 };
    uint8_t Type = 8; // ECMAArray
    //write type
    iohldr.ioh_write((void*)(&Type), 1, sizeof(uint8_t), (void*)(bs_out));
    //write ECMAArrayLength
    temp = nkn_vpe_swap_uint32(ECMAArrayLength);
    iohldr.ioh_write((void*)(&temp), 1, sizeof(uint32_t), (void*)(bs_out));

    //write Variable
    //duration
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[1],
				 yt_onmetadata->duration);
    //starttime
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[2],
				 yt_onmetadata->startTime);
    //totalduration
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[3],
				 yt_onmetadata->totalDuration);
    //width
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[4],
				 yt_onmetadata->width);
    //height
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[5],
				 yt_onmetadata->height);
    //videodatarate
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[6],
				 yt_onmetadata->videoDataRate);
    //audiodatarate
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[7],
				 yt_onmetadata->audioDataRate);
    //totaldatarate
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[8],
				 yt_onmetadata->totalDataRate);
    //framerate
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[9],
				 yt_onmetadata->frameRate);
    //bytelength
    yt_flv_write_buf_objprop_num(bs_out, yt_meta_name[10],
				 yt_onmetadata->byteLength);
    //canseekontime
    yt_flv_write_buf_objprop_bool(bs_out, yt_meta_name[11],
				  yt_onmetadata->canSeekOnTime);
    //sourcedata
    yt_flv_write_objprop_str(bs_out, yt_meta_name[12], yt_onmetadata->sourceData.Data,
			     yt_onmetadata->sourceData.DataLen);
    //purl
    yt_flv_write_objprop_str(bs_out, yt_meta_name[13], yt_onmetadata->purl.Data,
			     yt_onmetadata->purl.DataLen);
    //pmsg
    yt_flv_write_objprop_str(bs_out, yt_meta_name[14], yt_onmetadata->pmsg.Data,
			     yt_onmetadata->pmsg.DataLen);
    //httphostheader
    yt_flv_write_objprop_str(bs_out, yt_meta_name[15], yt_onmetadata->httpHostHeader.Data,
			     yt_onmetadata->httpHostHeader.DataLen);
    //write List Terminator
    iohldr.ioh_write((void*)(&terminator), 1, sizeof(uint24_t), (void*)(bs_out));

    //OK, now we need to clean
    if(yt_onmetadata->sourceData.Data != NULL) {
	free(yt_onmetadata->sourceData.Data);
    }

    if(yt_onmetadata->purl.Data != NULL) {
	free(yt_onmetadata->purl.Data);
    }

    if(yt_onmetadata->pmsg.Data != NULL) {
	free(yt_onmetadata->pmsg.Data);
    }

    if(yt_onmetadata->httpHostHeader.Data != NULL) {
	free(yt_onmetadata->httpHostHeader.Data);
    }

    return 1;
}


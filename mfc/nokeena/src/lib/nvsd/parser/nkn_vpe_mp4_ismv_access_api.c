#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_mp4_seek_parser.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_sync_http_read.h"

static size_t nkn_vpe_fread(void *buf, size_t n, size_t size,
                            void *desc);
static void * nkn_vpe_fopen(char *p_data, const char *mode,
                            size_t size);
static size_t nkn_vpe_ftell(void *desc);
static size_t nkn_vpe_fseek(void *desc, size_t seekto,
                            size_t whence);
static void nkn_vpe_fclose(void *desc);
static size_t nkn_vpe_fwrite(void *buf, size_t n, size_t size,
                             void *desc);

io_handlers_t io_file = {
    .ioh_open = nkn_vpe_fopen,
    .ioh_read = nkn_vpe_fread,
    .ioh_tell = nkn_vpe_ftell,
    .ioh_seek = nkn_vpe_fseek,
    .ioh_close = nkn_vpe_fclose,
    .ioh_write = nkn_vpe_fwrite
};

io_handlers_t io_http = {
    .ioh_open = vpe_http_sync_open,
    .ioh_read = vpe_http_sync_read,
    .ioh_tell = vpe_http_sync_tell,
    .ioh_seek = vpe_http_sync_seek,
    .ioh_close = vpe_http_sync_close,
    .ioh_write = NULL
};

/*********************************************************************
ISMV parser API's implementation to seek and extract MOOF fragments
1. mp4_get_mfra_offset - reads the offset of the MFRA box; the offset 
   of the MFRA box is present in the MFRO box. the MFRO box is usually 
   the last box (and is of a fixed size) in the MP4 file.
2. mp4_get_mfra_size - finds the size of the MFRA box
3. mp4_init_ismv_parser_ctx - initializes the mfra/tfra parser context
   and sets it up for seek and parsing
4. mp4_frag_seek - seeks to a particular timestamp within a give track
   id(profile). converts a timestamp in a profile into its corresponding 
   moof fragment's offset and length
5. mp4_get_next_frag - this API can be used to continue to get the offset 
   and lengths the moof fragments that follow the seeked moof fragment
   (continuous mode)
6. mp4_get_frag_count - finds the number of moof(s) in a traf
7. mp4_cleanup_ismv_ctx - cleanup the parser context
8. mp4_set_reader -- sets the given iohandler to the ismv_ctx
9. mp4_get_next_frag_1 -- this API is used to get the offset and
lengths 
*******************************************************************/
uint32_t
mp4_get_mfra_offset(uint8_t *src, size_t size)
{

    bitstream_state_t *bs;
    box_t box;
    size_t box_pos, parsed;
    uint32_t off;
    int32_t *type;

    bs = ioh.ioh_open((char*)src, "rb", size);
    off = 0;
    parsed = 0;

    do {
	mp4_read_next_box(bs, &box_pos, &box);
	parsed += 8;
	type = (int32_t*)(&box.type);
	if(*type == MFRO_ID) {
	    ioh.ioh_seek((void*)(bs), 12, SEEK_SET);
	    ioh.ioh_read((void*)(&off), 1, sizeof(uint32_t), (void*)(bs));
	    off = nkn_vpe_swap_uint32(off);
	    ioh.ioh_close((void*)bs);
	    return off;
	}
    } while(parsed < size);

    ioh.ioh_close((void*)bs);
    return off;
}

ismv_parser_ctx_t*
mp4_init_ismv_parser_ctx(uint8_t *src, size_t size, moov_t *moov)
{
    mfra_t *mfra;    
    tfra_t *tfra;
    bitstream_state_t *bs;
    box_t box;
    size_t box_pos;
    int32_t *type, i;
    ismv_parser_ctx_t *ctx;

    i = 0;
    bs = ioh.ioh_open((char*)src, "rb", size);
    
    do {
	mp4_read_next_box(bs, &box_pos, &box);
	type = (int32_t*)(&box.type);

	if(is_container_box(&box)) {
	    if(*type != META_ID) {
		ioh.ioh_seek((void*)bs, 8, SEEK_CUR);
	    } else {
		ioh.ioh_seek((void*)bs, 12, SEEK_CUR);
	    }

	    if(*type ==  MFRA_ID) {
		mfra = (mfra_t*)mp4_init_parent(src + ioh.ioh_tell((void*)bs),
						box_pos, box.short_size, MFRA_ID);
	    }
	} else {
	    int32_t skip;
	    switch(*type) {
		case TFRA_ID:
		    mfra->tfra[i] = tfra = (tfra_t*)mp4_tfra_init(src +
								  ioh.ioh_tell((void*)bs), 
								  box_pos, box.short_size);
		    mfra->n_traks++;
		    skip = mp4_read_tfra_header(bs, tfra);		    
		    ioh.ioh_seek((void*)bs, skip, SEEK_CUR);
		    i++;
		    continue;
	    }
	    ioh.ioh_seek((void*)bs, box.short_size, SEEK_CUR);
	}
    } while(ioh.ioh_tell((void*)bs) < size);

    ctx = (ismv_parser_ctx_t*)nkn_calloc_type(1, 
					      sizeof(ismv_parser_ctx_t),
					      mod_vpe_ismv_parser_ctx_t);
    if(!ctx) {
	return NULL;
    }
    //set the default io handler as file handler
    mp4_set_reader(ctx, &io_file);

    ctx->bs = bs;
    ctx->mfra = mfra;
    ctx->moov = moov;
    return ctx;
		    
}


int32_t 
mp4_set_reader(ismv_parser_ctx_t *ctx, io_handlers_t *iocb)
{
    ctx->iocb = iocb;

    return 0;
}
int32_t  
mp4_frag_seek(ismv_parser_ctx_t *ctx, uint32_t track_id, float seek_to, 
	      uint64_t trak_timescale, size_t *moof_offset, 
	      size_t *moof_length, size_t *moof_time)
{
    int32_t i, tbl_entry_size;
    uint32_t seek_trak;
    tfra_t *tfra;
    mfra_t *mfra;
    moov_t *moov;
    bitstream_state_t *bs;
    uint64_t tkhd_time, curr_moof_offset=0, curr_moof_time=0, curr_moof_length=0,
	prev_moof_time=0, prev_moof_offset=0, prev_moof_length=0;

    if(!ctx) {
	return -E_ISMV_INVALID_CTX;
    }

    mfra = ctx->mfra;
    moov = ctx->moov;
    bs = ctx->bs;

    /* find the track id in which we need to seek*/
    for(i = 0;i < (int32_t)mfra->n_traks;i++) {
	if(mfra->tfra[i]->track_id == track_id) {
	    ctx->curr_trak = seek_trak = i;
	    break;
	}
    }

    /* find the fragment number in the correct profile to seek to */
    tfra = mfra->tfra[seek_trak];

    /* seek to the begining of the tfra table */
    ioh.ioh_seek((void*)bs, tfra->pos + 24, SEEK_SET);

    /* convert the input seek time in seconds to TRAK timescale */
    if(moov) {
	tkhd_time = seek_to * moov->trak[seek_trak]->mdia->mdhd->timescale; 
    } else {
	    tkhd_time = trak_timescale;
    }

    prev_moof_time = prev_moof_offset, prev_moof_length = 0;
    curr_moof_time = curr_moof_offset, curr_moof_length = 0;

    /* loop through the entries and find the moof fragment
     * corresponding to the input seek time stamp
     */
    if(tfra->version) {
	/* tfra version is '1' */

	tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
						      &curr_moof_offset,
						      &curr_moof_time); 
	if (prev_moof_time >= tkhd_time) {
	    *moof_offset = curr_moof_offset;
	    i = 0;
	    goto found_fragment;
	}

	prev_moof_length = curr_moof_offset;
	prev_moof_offset = curr_moof_offset;
	prev_moof_time = curr_moof_time;
	
	for(i = 1;i < (int32_t)tfra->n_entry;i++) {
	    tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
							  &curr_moof_offset, &curr_moof_time);
	    /* found the seek time */
	    if(curr_moof_time > tkhd_time) {	
		*moof_offset = prev_moof_offset;
		goto found_fragment;
	    } else if (curr_moof_time == tkhd_time) {
		*moof_offset = curr_moof_offset;
		goto found_fragment;
	    }
	    prev_moof_length = curr_moof_offset - prev_moof_offset;
	    prev_moof_offset = curr_moof_offset;
	    prev_moof_time = curr_moof_time;
	}
    } else {
	/* tfra version is '0' */

	tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
						      &curr_moof_offset,
						      &curr_moof_time); 
	if (prev_moof_time >= tkhd_time) {
	    *moof_offset = curr_moof_offset;
	    i = 0;
	    goto found_fragment;
	}

	prev_moof_length = curr_moof_offset;
	prev_moof_offset = curr_moof_offset;
	prev_moof_time = curr_moof_time;

	for(i = 1;i < (int32_t)tfra->n_entry;i++) {
	    tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
							  &curr_moof_offset,
							  &curr_moof_time); 

	    if(curr_moof_time > tkhd_time) {	
		*moof_offset = prev_moof_offset;
		goto found_fragment;
	    } else if (curr_moof_time == tkhd_time) {
		*moof_offset = curr_moof_offset;
		goto found_fragment;
	    }

	    prev_moof_length = curr_moof_offset - prev_moof_offset;
	    prev_moof_offset = curr_moof_offset;
	    prev_moof_time = curr_moof_time;
	}
    }

 found_fragment:
    /* compute the length of a moof fragment */
    *moof_length = curr_moof_offset - prev_moof_offset;
    *moof_time = curr_moof_time;
    ctx->curr_frag = i;

    /* reset the stream position by one table entry since we read one extra entry to compute the length
     * of a fragment
     */
    ioh.ioh_seek((void*)bs, -(tbl_entry_size), SEEK_CUR);

    return 0;
}

int32_t
mp4_get_frag_count(ismv_parser_ctx_t *ctx, int32_t trak_num)
{
    tfra_t *tfra;

    tfra = ctx->mfra->tfra[trak_num-1];

    return (tfra->n_entry);
}

int32_t
mp4_get_trak_num(ismv_parser_ctx_t *ismv_ctx, int32_t *trak_num, int32_t
trak_id)
{
    int32_t err = 0;
    uint32_t i = 0;
    for(i=0; i<ismv_ctx->mfra->n_traks; i++) {
        if(trak_id == (int32_t)ismv_ctx->mfra->tfra[i]->track_id) {
            *trak_num = i+1;
            break;
        }
    }

    return err;

}
    
   
int32_t 
mp4_get_next_frag(ismv_parser_ctx_t *ctx, size_t *moof_offset, 
		  size_t *moof_length, size_t *moof_time)
{
    mfra_t *mfra;
    tfra_t *tfra;
    moov_t *moov;
    uint32_t seek_trak;
    bitstream_state_t *bs;
    uint64_t curr_moof_time, curr_moof_offset, next_moof_time, next_moof_offset; 
    int32_t tbl_entry_size;
    
    if(!ctx) {
	return -E_ISMV_INVALID_CTX;
    }

    mfra = ctx->mfra;
    moov = ctx->moov;
    bs = ctx->bs;
    seek_trak = ctx->curr_trak;

    tfra = mfra->tfra[seek_trak];
    
    /* check if it is the last entry. Last entry needs to be handled
    separately */ 
    if(ctx->curr_frag == (tfra->n_entry - 1)) {
	tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
						      &curr_moof_offset, 
						      &curr_moof_time); 
	*moof_offset = curr_moof_offset;
	*moof_length = 0;
	return E_ISMV_NO_MORE_FRAG;
    }

    if(tfra->version) {
	/*read the current table entry */
	tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
						      &curr_moof_offset, 
						      &curr_moof_time); 
	ctx->curr_frag++;
	if(ctx->curr_frag <= tfra->n_entry) {
	    /* read the next table entry */
	    tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
							  &next_moof_offset, 
							  &next_moof_time);  
	}
    } else {
	tbl_entry_size = mp4_read_tfra_table_entry_v0(bs, tfra,
						      &curr_moof_offset, 
						      &curr_moof_time); 
	ctx->curr_frag++;
	if(ctx->curr_frag <= tfra->n_entry) {
	    tbl_entry_size = mp4_read_tfra_table_entry_v0(bs, tfra,
							  &next_moof_offset, 
							  &next_moof_time); 
	}
    }

    /* compute the length of the moof fragment */
    *moof_length = next_moof_offset - curr_moof_offset;
    *moof_offset = curr_moof_offset;
    *moof_time = curr_moof_time;

    /* reset the stream position by one table entry since we read one
     * extra entry to  compute the length of a fragment
     */
    ioh.ioh_seek(bs, -(tbl_entry_size), SEEK_CUR);
    return 0;
}


int32_t
mp4_get_next_frag_1(ismv_parser_ctx_t *ctx, void *io_desc,
		    io_handlers_t *iocb, size_t *moof_offset,
		    size_t *moof_length, size_t *moof_time)
{
    mfra_t *mfra;
    tfra_t *tfra;
    moov_t *moov;
    uint32_t seek_trak;
    bitstream_state_t *bs;
    uint64_t curr_moof_time, curr_moof_offset, next_moof_time,
    next_moof_offset;
    int32_t tbl_entry_size;
    uint32_t moof_size_val = 0, mdat_size_val = 0;

    if(!ctx) {
        return -E_ISMV_INVALID_CTX;
    }

    mfra = ctx->mfra;
    moov = ctx->moov;
    bs = ctx->bs;
    seek_trak = ctx->curr_trak;

    tfra = mfra->tfra[seek_trak];

    if(tfra->version) {
        /*read the current table entry */
        tbl_entry_size = mp4_read_tfra_table_entry_v1(bs, tfra,
                                                      &curr_moof_offset, 
						      &curr_moof_time);
    } else {
        tbl_entry_size = mp4_read_tfra_table_entry_v0(bs, tfra,
                                                      &curr_moof_offset, 
						      &curr_moof_time);
    }
    
    *moof_offset = curr_moof_offset;
    *moof_time = curr_moof_time;
    /* compute the length of the moof fragment */
    iocb->ioh_seek(io_desc, curr_moof_offset, SEEK_SET);
    iocb->ioh_read(&moof_size_val, 1, 4, io_desc);
    moof_size_val = nkn_vpe_swap_uint32(moof_size_val);

    iocb->ioh_seek(io_desc, curr_moof_offset+moof_size_val, SEEK_SET);
    iocb->ioh_read(&mdat_size_val, 1, 4, io_desc);
    mdat_size_val = nkn_vpe_swap_uint32(mdat_size_val);
    *moof_length = moof_size_val + mdat_size_val;
    if(ctx->curr_frag == (tfra->n_entry - 1)) {
	return E_ISMV_NO_MORE_FRAG;
    }
    ctx->curr_frag++;


    return 0;
}

/** 
 * API to create an ismv parser context from a file
 * @param ismv_path - path to the ismv file
 * @param out - a fully populated valid ismv context
 * @return returns 0 on success, non-zero negative interger on
 * error
 */
int32_t
mp4_create_ismv_ctx_from_file(char *ismv_path, ismv_parser_ctx_t
			      **out, io_handlers_t *iocb)
{
    size_t mp4_size, mfra_off, moov_offset, moov_size, tmp_size,
	mfra_size, moof_size, moof_offset;
    uint8_t mfro[16], moov_search_data[MOOV_SEARCH_SIZE], *moov_data =
	NULL, *mfra_data = NULL, *ftyp_data = NULL;
    moov_t *moov;
    uint32_t n_traks;
    ismv_parser_ctx_t *ctx;
    int32_t err;
    FILE *fp;
    struct stat st;
    void *io_desc = NULL;

    err = 0;
    mp4_size = 0;
    fp  = NULL;
    ctx = NULL;
    moov_data = NULL;
    mfra_data = NULL;
    moov = NULL;
    if(!stat(ismv_path, &st)) {
	if (S_ISDIR(st.st_mode)) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    goto error;
	}
    } 

    io_desc = iocb->ioh_open((char*)\
                             ismv_path,
                             "rb", 0);
    if (!io_desc) {
	err = -E_VPE_PARSER_INVALID_FILE;
	return err;
    }
    iocb->ioh_seek(io_desc, 0, SEEK_END);

    mp4_size = iocb->ioh_tell(io_desc);
    if (!mp4_size) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    /*check whether the file is an ismv file or not*/
#if 0
    iocb->ioh_seek(io_desc, 8, SEEK_SET);
    ftyp_data = (uint8_t*)calloc(1, 4);
    iocb->ioh_read(ftyp_data, 4, sizeof(uint8_t), io_desc);
    if(strncmp((char*)ftyp_data, "avc1", 4) != 0) {
	err = -E_ISMV_PARSE_ERR;
	goto error;
    }
#endif
    iocb->ioh_seek(io_desc, 0, SEEK_SET);
    iocb->ioh_read(moov_search_data, MOOV_SEARCH_SIZE, sizeof(uint8_t), io_desc);

    /* read information about the moov box */
    mp4_get_moov_info(moov_search_data, MOOV_SEARCH_SIZE,
		      &moov_offset, &moov_size);

    if (!moov_offset) {
	err = -E_VPE_MP4_MOOV_ERR;
	goto error;	
    }

    /* read the moov data into memory */
    iocb->ioh_seek(io_desc, moov_offset, SEEK_SET);
    moov_data = (uint8_t*)calloc(1, moov_size);
    iocb->ioh_read(moov_data, moov_size,
		   sizeof(uint8_t), io_desc);

    /* initialize the moov box */
    moov = mp4_init_moov((uint8_t*)moov_data, moov_offset, moov_size);

    /* initialize the trak structure for the mp4 data */
    n_traks = mp4_populate_trak(moov);
    if (!n_traks) {
	err = -E_VPE_MP4_MOOV_ERR;
	goto error;
    }
    
    /* for fragmented MP4 files, the mfro box is usually the last box
     * in the file and is of fixed size. the mfro box gives the offset
     * in the mp4 file for the mfra box. the mfra box in turn has
     * information about the timestamp and offset of each moof
     * fragment
     */

    /* read the mfro box */
    iocb->ioh_seek(io_desc, -16, SEEK_END);
    iocb->ioh_read(mfro, 16, sizeof(uint8_t), io_desc);

    /* read the mfra offset from the mfro box */
    mfra_off = mp4_size -  mp4_get_mfra_offset(mfro, 16);
    if(mfra_off == mp4_size) {
	err = -E_ISMV_MFRA_MISSING;
	goto error;
    }
    
    /* read the mfra box */
    tmp_size = mp4_size - mfra_off;
    mfra_data = (uint8_t*)malloc(tmp_size);
    iocb->ioh_seek(io_desc, mfra_off, SEEK_SET);
    iocb->ioh_read(mfra_data, tmp_size, sizeof(uint8_t), io_desc);

    /* get exact mfra box size */
    mfra_size = mp4_get_mfra_size(mfra_data, tmp_size);
    
    /* initialize the ismv context */
    ctx = mp4_init_ismv_parser_ctx(mfra_data, mfra_size, moov);

    if (!ctx) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }
    
    *out = ctx;
    goto exit;
 error:
    if (ctx) mp4_cleanup_ismv_ctx(ctx);
    if (moov) mp4_moov_cleanup(moov, moov->n_tracks);
    if (moov_data) free(moov_data);
    if (mfra_data) free(mfra_data);
 exit:   
    if (ftyp_data) free(ftyp_data);
    if (io_desc)
        iocb->ioh_close(io_desc);
    return err;

}

void
mp4_cleanup_ismv_ctx_attach_buf(ismv_parser_ctx_t *ctx)
{
    if (ctx) {
	if (ctx->bs->data) {
	    free(ctx->bs->data);
	}
    }
}

void
mp4_cleanup_ismv_ctx(ismv_parser_ctx_t *ctx)
{
    if(ctx) {
	mp4_cleanup_mfra(ctx->mfra);
	ioh.ioh_close(ctx->bs);
	free(ctx);
    }
}

/************************************************************************
 *                        HELPER FUNCTIONS 
 ************************************************************************/
tfra_t*
mp4_tfra_init(uint8_t *data, size_t pos, size_t size)
{
    tfra_t *tfra;

    tfra = (tfra_t*)nkn_calloc_type(1, sizeof(tfra_t), 
				    mod_vpe_mp4_tfra_t);
    tfra->data = data;
    tfra->pos = pos;
    tfra->size = size;

    return tfra;
}

void
mp4_cleanup_mfra(mfra_t *mfra)
{
    uint32_t i;

    if(mfra) {
	for(i = 0;i < mfra->n_traks; i++) {
	    mp4_cleanup_tfra(mfra->tfra[i]);
	}
	free(mfra);
    }
}

void
mp4_cleanup_tfra(tfra_t *tfra) 
{
    if(tfra) {
	if(tfra->tbl) {
	    free(tfra->tbl);
	}
	free(tfra);
    }
}

int32_t 
mp4_read_tfra_header(bitstream_state_t *bs, tfra_t *tfra)
{
    int32_t i32, rv;
    size_t pos;
    tfra_table_t *tbl;
    int64_t skip_size;

    rv = 0;
    pos = ioh.ioh_tell((void*)bs);
    
    ioh.ioh_seek((void*)bs, 8, SEEK_CUR);

    ioh.ioh_read((void*)(&tfra->version), 1, sizeof(uint8_t), (void*)bs);
    ioh.ioh_seek((void*)bs, 3, SEEK_CUR);

    ioh.ioh_read((void*)&i32, 1, sizeof(int32_t), (void*)bs);
    tfra->track_id = nkn_vpe_swap_uint32(i32);

    ioh.ioh_read((void*)&i32, 1, sizeof(int32_t), (void*)bs);
    //    i32 = nkn_vpe_swap_uint32(i32);
    tfra->length_size_traf_num = (i32 & 0x03000000) >> 24;
    tfra->length_size_trun_num = (i32 & 0x0c000000) >> 26;
    tfra->length_size_sample_num = (i32 & 0x30000000) >> 28;
    
    ioh.ioh_read((void*)&i32, 1, sizeof(int32_t), (void*)bs);
    tfra->n_entry = nkn_vpe_swap_uint32(i32);

    tbl = tfra->tbl = (tfra_table_t*)nkn_malloc_type(tfra->n_entry *\
						     sizeof(tfra_table_t),
						     mod_vpe_mp4_tfra_table_t);
    skip_size = 0;

    if(tfra->version) {
	skip_size = tfra->n_entry *  (16+ 
				      tfra->length_size_traf_num + 1 +
				      tfra->length_size_trun_num + 1 +
				      tfra->length_size_sample_num + 1);
    } else { 
	skip_size = tfra->n_entry * (8 + 
				     tfra->length_size_traf_num + 1 +
				     tfra->length_size_trun_num + 1 +
				     tfra->length_size_sample_num + 1);
    }
    
    ioh.ioh_seek((void*)bs, skip_size, SEEK_CUR);
    /*
    if(tfra->version) {
	for(i = 0;i < tfra->n_entry;i++) {
	    ioh.ioh_read((void*)(&u64), 1, sizeof(uint64_t), (void*)bs);
	    u64 = nkn_vpe_swap_uint64(u64);
	    tfra->time = u64;
	    ioh.ioh_read((void*)(&u64), 1, sizeof(uint64_t), (void*)bs);
	    u64 = nkn_vpe_swap_uint64(u64);
	    tfra-> = u64;
	}
    }
    */
    rv = ioh.ioh_tell((void*)bs) - pos;
    ioh.ioh_seek((void*)bs, pos, SEEK_SET);
 
    return rv;    
}
		  
int32_t 
mp4_read_tfra_table_entry_v1(bitstream_state_t *bs, tfra_t *tfra,
			     size_t *moof_offset,size_t *moof_time)
{
    uint64_t u64;
    size_t skip_bytes;

    ioh.ioh_read((void*)(&u64), 1, sizeof(uint64_t), (void*)bs);
    u64 = nkn_vpe_swap_uint64(u64);
    *moof_time = u64;
    ioh.ioh_read((void*)(&u64), 1, sizeof(uint64_t), (void*)bs);
    u64 = nkn_vpe_swap_uint64(u64);
    *moof_offset = u64;
    skip_bytes =  (tfra->length_size_traf_num + 1 +
		   tfra->length_size_trun_num + 1 +
		   tfra->length_size_sample_num + 1);
    ioh.ioh_seek((void*)bs, skip_bytes, SEEK_CUR);

    return (skip_bytes + 16);
}

int32_t 
mp4_read_tfra_table_entry_v0(bitstream_state_t *bs, tfra_t *tfra,
			     size_t *moof_offset, size_t *moof_time)
{
    uint32_t u32;
    size_t skip_bytes;
    
    ioh.ioh_read((void*)(&u32), 1, sizeof(uint32_t), (void*)bs);
    u32 = nkn_vpe_swap_uint32(u32);
    *moof_time = u32;
    ioh.ioh_read((void*)(&u32), 1, sizeof(uint32_t), (void*)bs);
    u32 = nkn_vpe_swap_uint32(u32);
    *moof_offset = u32;
    skip_bytes =  (tfra->length_size_traf_num + 1 +
		   tfra->length_size_trun_num + 1 +
		   tfra->length_size_sample_num + 1);
    ioh.ioh_seek((void*)bs, skip_bytes, SEEK_CUR);
    
    return (skip_bytes + 8);
}

void *
nkn_vpe_fopen(char *p_data, const char *mode, size_t size)
{
    size = size;
    return (void *)fopen(p_data, mode);
}

static size_t
nkn_vpe_fseek(void *desc, size_t seek, size_t whence)
{
    FILE *f = (FILE*)desc;
    return fseek(f, seek, whence);
}

static size_t
nkn_vpe_ftell(void *desc)
{
    FILE *f = (FILE*)desc;
    return ftell(f);
}

static void
nkn_vpe_fclose(void *desc)
{
    FILE *f = (FILE*)desc;
    fclose(f);

    return;
}

static size_t
nkn_vpe_fwrite(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;
    return fwrite(buf, n, size, f);
}

static size_t
nkn_vpe_fread(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;

    return fread(buf, n, size, f);
}

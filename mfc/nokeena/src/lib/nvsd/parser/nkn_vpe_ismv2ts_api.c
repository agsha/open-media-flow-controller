#include <stdio.h>
#include <inttypes.h>

#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_ismv2ts_api.h"
#include "nkn_vpe_mfu_aac.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_memalloc.h"

#define MOOV_SEARCH_SIZE (32*1024)

static int32_t ismv2ts_fill_buffer(ismv2ts_converter_ctx_t *conv_ctx,
				   void **outxo);
static int32_t ismv2ts_seek_next_trak(
		       ismv2ts_converter_ctx_t *conv_ctx); 
static int32_t init_ismv2ts_builder(int32_t chunk_time,
				    int32_t n_traks,
				    FILE *fin,
				    int32_t is_audio,
				    moov_t *moov,
				    ismv2ts_builder_t **ismv2ts_ctx);

int32_t
init_ismv2ts_converter(file_conv_ip_params_t *fconv_params, 
		       void **conv_ctx)
{
    size_t mp4_size, mfra_off, moov_offset, moov_size, tmp_size,
	mfra_size, moof_size, moof_offset;
    uint8_t mfro[16], moov_search_data[MOOV_SEARCH_SIZE], *moov_data,
	*mfra_data;
    moov_t *moov;
    uint32_t n_traks;
    ismv_parser_ctx_t *ctx;
    ismv2ts_builder_t *bldr_ctx;
    ismv2ts_converter_ctx_t *ismv2ts_ctx;
    ismv_reader_t *reader;
    int32_t err;
    FILE *fp, *fout;

    err = 0;
    mp4_size = 0;
    fp = fout = NULL;
    *conv_ctx = NULL;
    reader = NULL;

    *conv_ctx = ismv2ts_ctx = (ismv2ts_converter_ctx_t*)\
	nkn_calloc_type(1, sizeof(ismv2ts_converter_ctx_t),\
			mod_vpe_ismv2ts_converter_ctx_t);
    if (!ismv2ts_ctx) {
	err = -E_VPE_PARSER_NO_MEM;
	return err;
    }

    fp = fopen(fconv_params->input_path[0], "rb");
    if (!fp) {
	err = -E_VPE_PARSER_INVALID_FILE;
	return err;
    }

    if (fconv_params->output_path) {
	fout = fopen(fconv_params->output_path, "wb");
	if (!fout) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    return err;
	}
    }

    fseek(fp, 0, SEEK_END);
    mp4_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
	
    fseek(fp, 0, SEEK_SET);
    fread(moov_search_data, 1, MOOV_SEARCH_SIZE, fp);
	
    /* read information about the moov box */
    mp4_get_moov_info(moov_search_data, MOOV_SEARCH_SIZE,
		      &moov_offset, &moov_size);

    if (!moov_offset) {
	err = -E_VPE_MP4_MOOV_ERR;
	goto error;	
    }

    /* read the moov data into memory */
    fseek(fp,moov_offset, SEEK_SET);
    moov_data = (uint8_t*)calloc(1, moov_size);
    fread(moov_data, 1, moov_size, fp);

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
    fseek(fp, -16, SEEK_END);
    fread(mfro, 1, 16, fp);

    /* read the mfra offset from the mfro box */
    mfra_off = mp4_size - mp4_get_mfra_offset(mfro, 16);
    if(mfra_off == mp4_size) {
	err = -E_ISMV_MFRA_MISSING;
	goto error;
    }
    
    /* read the mfra box */
    tmp_size = mp4_size - mfra_off;
    mfra_data = (uint8_t*)malloc(tmp_size);
    fseek(fp, mfra_off, SEEK_SET);
    fread(mfra_data, 1, tmp_size, fp);

    /* get exact mfra box size */
    mfra_size = mp4_get_mfra_size(mfra_data, tmp_size);
    
    /* initialize the ismv context */
    ctx = mp4_init_ismv_parser_ctx(mfra_data, mfra_size, moov);

    if (!ctx) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }
    
    err = init_ismv2ts_builder(fconv_params->key_frame_interval,
			       n_traks,
			       fp, 1, moov, &bldr_ctx);
    if (err) {
	goto error;
    }

    err = init_ismv_reader(&reader);
    if (err) {
	goto error;
    }
    
    ismv2ts_ctx->reader = reader;
    ismv2ts_ctx->parser = ctx;
    ismv2ts_ctx->bldr = bldr_ctx;
    ismv2ts_ctx->sess_id = fconv_params->sess_id;

    return err;
 error:
    if (ctx) mp4_cleanup_ismv_ctx(ctx);
    if (moov) mp4_moov_cleanup(moov, moov->n_tracks);
    if (moov_data) free(moov_data);
    if (mfra_data) free(mfra_data);
    if (fp) fclose(fp);
    
    return err;
}

static int32_t
init_ismv2ts_builder(int32_t chunk_time,
		     int32_t n_traks,
		     FILE *fin,
		     int32_t is_audio,
		     moov_t *moov,
		     ismv2ts_builder_t **ismv2ts_ctx)
{

    ismv2ts_builder_t *ctx;
    int32_t err, i, trak_type;
    FILE *fout;
    trak_t  *trak;

    *ismv2ts_ctx = ctx = NULL;
    err = 0;

    *ismv2ts_ctx = ctx = (ismv2ts_builder_t *)nkn_calloc_type(1,
							    sizeof(ismv2ts_builder_t),
							    mod_vpe_ismv2ts_builder_t);
    if (!ctx) {
	err = -E_VPE_PARSER_NO_MEM;
	return err;
    }

    ctx->chunk_time = chunk_time;
    ctx->n_traks = n_traks;
    ctx->fin = fin;
    ctx->fname = NULL;
    //ctx->Is_audio = is_audio;
    ctx->vid_trak_num =ctx->aud_trak_num = 0;
    ctx->file_dump=0;

    for(i=0;i<ctx->n_traks;i++){
        trak = moov->trak[i];
        trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	if(trak_type == VIDE_HEX){
	    ctx->video[ctx->n_vid_traks].list = NULL;
	    ctx->n_vid_traks++;
	}
	if(trak_type == SOUN_HEX){
	    ctx->audio[ctx->n_aud_traks].esds_flag = 0;
	    ctx->audio[ctx->n_aud_traks].adts = (adts_t*)calloc(sizeof(adts_t),1);
	    ctx->audio[ctx->n_aud_traks].list = NULL;
            ctx->n_aud_traks++;
	}
    }
   
    return err;
}


int32_t
ismv2ts_process_frags(void *private_data, void **out)
{
    ismv2ts_converter_ctx_t *conv_ctx;
    int32_t err;

    err = 0;
    conv_ctx = (ismv2ts_converter_ctx_t*)(private_data);
    
    if (conv_ctx->reader->state & ISMV_BUF_STATE_NEXT_TRAK) {
	ismv2ts_seek_next_trak(conv_ctx);
    }

    if (conv_ctx->reader->state & ISMV_BUF_STATE_NODATA) {
	ismv2ts_fill_buffer(conv_ctx, out);
    }

    if (conv_ctx->reader->state & ISMV_BUF_STATE_DATA_READY) {
	//ismv2ts_consume_frags();
    }

    return err;
}

static int32_t 
ismv2ts_seek_next_trak(ismv2ts_converter_ctx_t *conv_ctx)
{

    ismv_parser_ctx_t *ctx;
    ismv2ts_builder_t *bldr;
    moov_t *moov;
    size_t moof_offset, moof_size;
    uint64_t moof_time;
    int32_t i;

    ctx = conv_ctx->parser;
    bldr = conv_ctx->bldr;
    moov = ctx->moov;

    i = 0;
    mp4_frag_seek(ctx, moov->trak[i]->tkhd->track_id, 0,
		  moov->trak[i]->mdia->mdhd->timescale, &moof_offset,
		  &moof_size, &moof_time);

    return 0;
}

static int32_t
ismv2ts_fill_buffer(ismv2ts_converter_ctx_t *conv_ctx, void **out)
{
    ismv_parser_ctx_t *ctx;
    ismv2ts_builder_t *bldr;
    ismv_reader_t *reader;
    ismv_moof_proc_buf_t *proc_buf;

    ctx = conv_ctx->parser;
    bldr = conv_ctx->bldr;
    reader = conv_ctx->reader;

    ismv_reader_fill_buffer(reader, bldr->fin, 0, ctx, &proc_buf);
    *out = proc_buf;

    return 0;
}

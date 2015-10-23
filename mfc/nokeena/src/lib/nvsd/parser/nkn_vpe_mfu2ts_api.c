#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "nkn_vpe_mfu2ts_api.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_vpe_mfu_parser.h"
#include "nkn_memalloc.h"
#include "mfu2iphone.h"
#include "nkn_vpe_moof2mfu.h"

extern gbl_fruit_ctx_t fruit_ctx;
static int32_t
init_mfu2ts_builder(file_conv_ip_params_t *,
                    mfu2ts_builder_t **);

static int32_t
mfu2ts_fill_buffer(mfu2ts_converter_ctx_t *, 
		   void **);
static void
mfu2ts_cleanup_ts_builder(mfu2ts_builder_t *bldr);

int32_t
init_mfu2ts_converter(file_conv_ip_params_t *fconv_params,
			void **conv_ctx)
{
    mfu2ts_builder_t *bldr;
    ismv_reader_t *reader;
    mfu2ts_converter_ctx_t* mfu2ts_ctx;
    int32_t err=0;

    *conv_ctx = NULL;
    reader = NULL;
    *conv_ctx = mfu2ts_ctx = (mfu2ts_converter_ctx_t*)\
        nkn_calloc_type(1, sizeof(mfu2ts_converter_ctx_t),\
                        mod_vpe_mfu2ts_converter_ctx_t);
    if (!mfu2ts_ctx) {
        err = -E_MFP_NO_MEM;
        return err;
    }
#if 1
    err = init_ismv_reader(&reader);
    if (err) {
        goto error;
    }
#endif
    err = init_mfu2ts_builder(fconv_params,
                                 &bldr);
    if (err) {
        goto error;
    }
    mfu2ts_ctx->bldr = bldr;
    mfu2ts_ctx->reader = reader;
    return err;
 error:
    //call cleanup functions
    if(reader) {
	ismv_reader_cleanup_proc_buf(reader->out);
	ismv_reader_cleanup(reader);
    }

    return err;
}


static int32_t
init_mfu2ts_builder(file_conv_ip_params_t *fconv_params,
		    mfu2ts_builder_t **out)
{
    mfu2ts_builder_t *bldr;
    int32_t err=0;
    uint32_t i=0;

    *out = bldr = NULL;
    err = 0;

    *out = bldr = (mfu2ts_builder_t *)nkn_calloc_type(1,
						      sizeof(mfu2ts_builder_t),
						      mod_vpe_mfu2ts_builder_t);
    if (!bldr) {
        err = -E_MFP_NO_MEM;
        return err;
    }
    bldr->mfu_data_orig = NULL;
    bldr->num_mfu_count = 0;
    bldr->mfu_out_fruit_path = fconv_params->output_path;
    bldr->mfu_uri_fruit_prefix = fconv_params->delivery_url;
    bldr->is_mfu2ts_conv_stop = 0;
    bldr->tot_mfu_count = 0;
    bldr->num_mfu_stored = 0;
    bldr->num_mfu_processed = 0;
    bldr->num_pmf = 0;
    bldr->num_active_moof2ts_task = nkn_calloc_type(MAX_TRACKS,
                                                   sizeof(int32_t*),
                                                   mod_vpe_temp_t);
    if(bldr->num_active_moof2ts_task == NULL)
      return -E_VPE_PARSER_NO_MEM;
    for(i=0; i< MAX_TRACKS; i++) {
      bldr->num_active_moof2ts_task[i] = nkn_calloc_type(1,
							 sizeof(int32_t),
							 mod_vpe_temp_t);
      if(bldr->num_active_moof2ts_task[i] == NULL)
        return -E_VPE_PARSER_NO_MEM;
    }
    printf("the allocated is %d \n", i);
    return err;


}


int32_t
mfu2ts_process_frags(void *private_data, void **out)
{
    mfu2ts_converter_ctx_t *conv_ctx;
    int32_t err;

    err = 0;
    conv_ctx = (mfu2ts_converter_ctx_t*)(private_data);
    if ((conv_ctx->reader->state & ISMV_BUF_STATE_NODATA)) {
        err = mfu2ts_fill_buffer(conv_ctx, out);
    }
#if 1
    if (conv_ctx->reader->state & ISMV_BUF_STATE_DATA_READY) {
        //ismv2ts_consume_frags();
    }
#endif
    return err;
}


static int32_t
mfu2ts_fill_buffer(mfu2ts_converter_ctx_t *conv_ctx, void **out)
{
    uint16_t pgm_num =0;
    uint16_t st_id = 0;
    mfu2ts_builder_t *bldr;
    uint32_t i, j;
    ts_desc_t *ts;
    int32_t err=0;
    char* mfu_out_fruit_path =NULL, *mfu_uri_fruit_prefix=NULL;
    char chunk_fname[500] = {'\0'};
    mfu_data_t mfu_data;
    int32_t rv = 0;
    bldr = conv_ctx->bldr;
    if(bldr->is_last_mfu == 1) {
	//fruit_ctx.encaps[bldr->num_pmf].is_end_list = 1;
    }
    if(bldr->is_mfu2ts_conv_stop == 0) {
	for(i=0;i<bldr->num_mfu_count;i++) {
	    if(bldr->is_last_mfu == 1) {
		//fruit_ctx.encaps[bldr->num_pmf].stream[i].is_end_list = 1;
	    }
	    mfub_t mfu_hdr;
	    mfu_data_t *mfu_data_cont = nkn_calloc_type(1, sizeof(mfu_data_t),
							mod_vpe_mfu2ts_builder_t);
	    if (mfu_data_cont == NULL) {
		err = -E_MFP_NO_MEM;
		return err;
	    }
	    mfu_data_cont->data = bldr->mfu_data[i].data;
	    mfu_data_cont->data_size =
		bldr->mfu_data[i].data_size;
	    mfu_get_mfub_box(bldr->mfu_data[i].data, 0, &mfu_hdr);
	    ref_count_mem_t *ref_cont = createRefCountedContainer(mfu_data_cont,
								  destroyMfuData);
	    fruit_fmt_input_t *fmt_input = NULL;
	    fmt_input =							\
		nkn_calloc_type(1, sizeof(fruit_fmt_input_t),
				mod_vpe_mfp_fruit_fmt_input_t);
	    if (!fmt_input) {
		return -E_MFP_NO_MEM;
	    }
	    fmt_input->mfu_seq_num = bldr->seq_num - 1;
	    fmt_input->state = FRUIT_FMT_CONVERT;
	    fmt_input->streams_per_encap = bldr->streams_per_encap;
	    fmt_input->active_task_cntr =\
		bldr->num_active_moof2ts_task[mfu_hdr.stream_id];
	    (*fmt_input->active_task_cntr)++;
	    //fmt_input->encr_props = fruit_ctx.encaps[bldr->num_pmf].fruit_formatter.encr_props;
	    //fmt_input->encr_props = NULL; /*need to fill
	    //			    this */
	    mfu_data_cont->fruit_fmtr_ctx = (void*)fmt_input;
	    
	    ref_cont->hold_ref_cont(ref_cont);
	    if ((mfu_hdr.offset_vdat != 0xffffffffffffffff) &&
		(mfu_hdr.offset_adat != 0xffffffffffffffff))
		    apple_fmtr_hold_encap_ctxt(get_sessid_from_mfu(mfu_data_cont));
	    rv = mfubox_ts(ref_cont);
	    if(rv != VPE_SUCCESS) {
		return -E_VPE_TS_CREATION_FAILED;
	    }

	    bldr->num_mfu_processed++;
	}//for loop
	if(bldr->mfu_data != NULL){
	    free(bldr->mfu_data);
	    bldr->mfu_data = NULL;
	}
	return E_VPE_FILE_CONVERT_CONTINUE;
    }
    else {
	if(bldr->mfu_data != NULL){
            free(bldr->mfu_data);
	    bldr->mfu_data = NULL;
	}
	return E_VPE_FILE_CONVERT_STOP;
    }
}

void
mfu2ts_cleanup_converter(void *private_data)
{
    mfu2ts_converter_ctx_t *conv;
    mfu2ts_builder_t *bldr;
    conv = (mfu2ts_converter_ctx_t*)private_data;
    bldr  = conv->bldr;
    if (conv) {
	ismv_reader_cleanup_proc_buf(conv->reader->out);
	ismv_reader_cleanup(conv->reader);
	mfu2ts_cleanup_ts_builder(conv->bldr);
	free(conv);
    }
}

static void
mfu2ts_cleanup_ts_builder(mfu2ts_builder_t *bldr)
{
    uint32_t i=0;
    if (bldr) {
      printf("the num_traks is %d \n", bldr->n_traks);
      if(bldr->num_active_moof2ts_task) {
	for(i=bldr->n_traks; i<(MAX_TRACKS); i++) {
	  if(bldr->num_active_moof2ts_task[i] != NULL)
	    free(bldr->num_active_moof2ts_task[i]);
	}
        free(bldr->num_active_moof2ts_task);
      }
      if(bldr->mfu_data != NULL) {
	for(i=bldr->num_mfu_processed; i<(bldr->num_mfu_count); i++) {
	  if(&bldr->mfu_data[i] != NULL) {
	    if(bldr->mfu_data[i].data !=  NULL){
	      free(bldr->mfu_data[i].data);
	    }
	  }
	}
	free(bldr->mfu_data);
	bldr->mfu_data = NULL;
      }
      free(bldr);
    }

}

int32_t
mfu2ts_write_output(void *out)
{


    return 0;
}


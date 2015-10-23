#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "nkn_vpe_ismv2moof_api.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_utils.h"
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_error_no.h"

const char *_dbg_output_dir = "/log/sl_mfp";
uint32_t ismv2moof_write_array_free =0;
uint32_t ismv2moof_write_array_alloc = 0;

static int32_t 
ismv2moof_fill_buffer(\
       ismv2moof_converter_ctx_t *conv_ctx, void **out);

static int32_t 
ismv2moof_seek_next_trak(ismv2moof_converter_ctx_t *conv_ctx); 

int32_t 
init_ismv_moof_builder(int32_t chunk_time,
		       int32_t n_traks,
		       uint8_t ss_package_type,
		       char *output_path,
		       char **sl_pkg_filenames,
		       int32_t streaming_type,
		       int32_t num_pmf,
		       ismv_moof_builder_t **out);

static int32_t 
ismv2moof_setup_writer(ismv2moof_write_t *writer, 
		       ismv_moof_builder_t *bldr,
		       ismv_moof_proc_buf_t *proc_buf, 
		       int32_t trak_type_name, int32_t curr_profile,
		       int32_t trak_type_idx, int32_t
		       curr_profile_idx);


static inline int32_t 
ismv2moof_setup_next_trak(ismv_moof_builder_t *bldr);

static void
ismv2moof_cleanup_moof_builder(ismv_moof_builder_t *bldr);

static int32_t
ismv2moof_copy_manifest(ismv_moof_builder_t *bldr);


#if 1
static io_handlers_t iohldr = {
    .ioh_open = nkn_vpe_fopen,
    .ioh_read = nkn_vpe_fread,
    .ioh_tell = nkn_vpe_ftell,
    .ioh_seek = nkn_vpe_fseek,
    .ioh_close = nkn_vpe_fclose,
    .ioh_write = nkn_vpe_fwrite
};

static io_handlers_t ioh_http = {
    .ioh_open = vpe_http_sync_open,
    .ioh_read = vpe_http_sync_read,
    .ioh_tell = vpe_http_sync_tell,
    .ioh_seek = vpe_http_sync_seek,
    .ioh_close = vpe_http_sync_close,
    .ioh_write = NULL
};
#endif

int32_t
init_ismv2moof_converter(file_conv_ip_params_t *fconv_params,
			 void **conv_ctx)
{
    int32_t err = 0, n_traks = 0;
    ismv_parser_ctx_t *ismv_ctx = NULL;
    ismv2moof_converter_ctx_t *ismv2moof_ctx = NULL;
    ismv_reader_t *reader = NULL;
    ismv_moof_builder_t *bldr = NULL;
    int32_t i=0, j=0, num_moofs[MAX_TRACKS];
    uint32_t k=0;
    trak_t  *trak = NULL;
    int32_t temp=0;
    int32_t trak_type_name = 0;
    ismc_cust_attr_t *ca_ismc;
    uint32_t ca_ismc_attr_count = 0, l=0;
    int32_t trak_num = 0;

    *conv_ctx = ismv2moof_ctx = (ismv2moof_converter_ctx_t *)	\
	nkn_calloc_type(1, sizeof(ismv2moof_converter_ctx_t),
			mod_ismv2moof_converter_ctx_t);

    if(!ismv2moof_ctx) {
	err = -E_VPE_PARSER_NO_MEM;
	return err;
    }
    err = init_ismv_moof_builder(fconv_params->key_frame_interval,
                                 n_traks,
                                 fconv_params->ss_package_type,
                                 fconv_params->output_path,
                                 fconv_params->input_path,
				 fconv_params->streaming_type,
				 fconv_params->num_pmf,
                                 &bldr);
    if (err) {
        goto error;
    }
    ismv2moof_ctx->bldr = bldr;

    if(bldr->profiles_to_process > MAX_TRACKS) {
	err = -E_VPE_MAX_PROFILE_ERR;
	goto error;
    }

    /* create a valid ismv context from the ISMV file */
    /* create individual ismv contxt for individual track */
    for (i=0;i< MAX_TRAK_TYPES;i++) {
	temp = bldr->psi->attr[i]->n_profiles;
	if(temp !=0) {
	    for (j=0;j<temp;j++) {
		char ismv_name[256];
		const char *src_video_name;
		char *ca_ismc_name[MAX_ATTR]={NULL};
		char *ca_ismc_val[MAX_ATTR]={NULL};
		ca_ismc_attr_count = 0;
		bldr->trak_type_idx[k] = i;
		bldr->curr_profile_idx[k] = j;
		bldr->curr_profile[k] =
		    bldr->psi->attr[i]->bitrates[j];
		bldr->curr_trak_id[k] =
		    ism_get_track_id(bldr->ism_map,
				     bldr->curr_profile[k],
				     bldr->trak_type_idx[k]);

		TAILQ_FOREACH(ca_ismc, &bldr->psi->attr[i]->cust_attr_head[j], entries) {
		    //printf("ca_ismc->name: %s\n", ca_ismc->name);
		    ca_ismc_name[ca_ismc_attr_count] = strdup(ca_ismc->name);
		    ca_ismc_val[ca_ismc_attr_count] = strdup(ca_ismc->val);
		    ca_ismc_attr_count++;
		}
		src_video_name =
		    ism_get_video_name(bldr->ism_map,
				       bldr->trak_type_idx[k],
				       bldr->curr_trak_id[k],
				       bldr->curr_profile[k],
				       &ca_ismc_name[0],
				       &ca_ismc_val[0], ca_ismc_attr_count);
		if(src_video_name == NULL) {
		    err = -E_VPE_MISMATCH_CUST_ATTR;
		    goto error;
		}
		snprintf(ismv_name, 256, "%s/%s", bldr->ismv_file_path,
			 src_video_name);
		
		bldr->ismv_name[k] = strdup(ismv_name);
		for(l=0; l<ca_ismc_attr_count; l++) {
		    if(ca_ismc_name[l])
			free(ca_ismc_name[l]);
		}

		err = mp4_create_ismv_ctx_from_file(
						    bldr->ismv_name[k], 
						    &ismv_ctx,
						    bldr->iocb);
		if (err) {
		    goto error;
		}
		//set the io handler based on the bldr->iocb
		mp4_set_reader(ismv_ctx, bldr->iocb);
		mp4_get_trak_num(ismv_ctx, &trak_num,
			     bldr->curr_trak_id[k]);
		bldr->trak_num[k] = trak_num;
		
		num_moofs[k] = mp4_get_frag_count(ismv_ctx, bldr->trak_num[k]);
		ismv2moof_ctx->ismv_ctx[k] = ismv_ctx;
		err  = init_ismv_reader(&reader);
		if (err) {
		    goto error;
		}
		ismv2moof_ctx->reader[k] = reader;

		k++;
		bldr->n_traks = k;
	    }//for loop for j
	}//if(temp !=0) 
    }//for loop for i
    /*made assumption that all the traks will have same number of moof
						    count*/
    /* calulate  the number of  moofs to be procssed which is nothing
       but the minimum of number of moofs of  audio and video*/
    bldr->total_moof_count = ismv_ctx->mfra->tfra[0]->n_entry;    
    for(i=0; i<bldr->n_traks; i++) {
	if(i==0)
	    bldr->total_moof_count = num_moofs[i];
	else {
	    if(bldr->total_moof_count > (uint32_t)num_moofs[i])
		bldr->total_moof_count = num_moofs[i];
	}
    }
    return err;
    
 error:
    if (ismv2moof_ctx) {
    	ismv2moof_cleanup_converter(ismv2moof_ctx);
    	free(ismv2moof_ctx);
    }
    return err;

}

int32_t
ismv2moof_process_frags(void *private_data, void **out)
{
    ismv2moof_converter_ctx_t *conv_ctx = NULL;
    int32_t err;

    err = 0;
    conv_ctx = (ismv2moof_converter_ctx_t*)(private_data);
    err = ismv2moof_fill_buffer(conv_ctx, out);

    return err;
}

int32_t
ismv2moof_write_output(void *out)
{
    ismv_moof_proc_buf_t *proc_buf;
    ismv2moof_write_t *writer;
    ismc_av_attr_t *av;
    ismc_cust_attr_t *ca;
    int32_t err, i, n_ol_entries, rv;
    uint32_t k;
    char out_file_name[256], ql_str[256];
    FILE *fout;
    vpe_olt_t *ol;
    const char *trak_type_str[] = {"video",
			       "audio" };
    ismv2moof_write_array_t *writer_array;
    uint32_t j=0;
    size_t temp_offset = 0;
    ism_av_attr_t *attr;
    ism_cust_attr_t *ca_ism;
    err = 0;    
    rv = 0;
    writer_array = (ismv2moof_write_array_t*)out;
    for (j=0;j<writer_array->num_tracks_towrite;j++) {
	temp_offset = 0;
	writer = &writer_array->writer[j];
	proc_buf = writer->out;
	if(proc_buf !=NULL) {
	    ol = proc_buf->ol_list;
	    av = writer->psi->attr[writer->trak_type];
	    ql_str[0] = '\0';
	    
	    rv = snprintf(ql_str, 256,"QualityLevels(%d",
			  writer->profile);
	    TAILQ_FOREACH(ca, &av->cust_attr_head[writer->curr_profile_idx], entries) {
		rv += snprintf(&ql_str[rv], 256, ",%s=%s",
			       ca->name, ca->val);
	    }
	    snprintf(&ql_str[rv], 256, ")");
	    
	    sprintf(out_file_name,"%s/%s/", writer->output_dir,
		    writer->video_name + 1);

	    rv = recursive_mkdir(out_file_name, 0755);
	    if (rv) {
		err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
		printf("invalid output path (errno=%d)\n", err);
		goto error;
	    }
	    
	    sprintf(out_file_name,"%s/%s/%s/", 
		    writer->output_dir, writer->video_name + 1,
	    ql_str);
	    mkdir(out_file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	    if (rv && errno != EEXIST) {
		err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
		printf("invalid output path (errno=%d)\n", err);
		goto error;
	    }
	    

	    /*get the custom attribute from ism file*/
	    attr = writer->ism_map->attr[writer->trak_type];
	    TAILQ_FOREACH(ca_ism, &attr->prop[writer->curr_profile_idx].cust_attr_head,
			  entries) {
		if(!strcmp(ca_ism->name,"trackName")) {
		    trak_type_str[writer->trak_type] =  strdup(ca_ism->val);
		}
	    }
	    /* copy the num entries since we will free the proc_buf after
	     * write 
	     */
	    n_ol_entries = proc_buf->ol_list_num_entries;
	    for(i = 0;  i < n_ol_entries; i++) {
		size_t rv1 = 0;
		sprintf(out_file_name,"%s/%s/%s/Fragments(%s=%ld)", 
		writer->output_dir, writer->video_name + 1, ql_str, 
			trak_type_str[writer->trak_type], ol[i].time);
		fout = fopen(out_file_name, "wb");
		if (!fout) {
		    err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
		    printf("invalid output path (errno=%d)\n", err);
		    break;
		}
		rv1 = fwrite(proc_buf->buf + temp_offset, 1,
			    ol[i].length, fout);
		temp_offset+= ol[i].length;

		fclose(fout);
	    }	       
	}
    }
    
 error:
    ismv2moof_cleanup_writer_array(writer_array);
    return err;
}

void
ismv2moof_cleanup_converter(void *private_data)
{
    ismv2moof_converter_ctx_t *conv;
    ismv_moof_builder_t *bldr;
    int32_t i=0;
    conv = (ismv2moof_converter_ctx_t*)private_data;
    bldr  = conv->bldr;
    if (conv) {
	/* not obvious since the mp4_moov_cleanup API only cleans-up
	 * the context resources and not the moov data buffer attached
	 * to it. same is the case for the ismv ctx where mfra is not
	 * free'ed in the cleanup call
	 */
	for (i=0; i<bldr->n_traks; i++) {
	    if(&conv->ismv_ctx[i]) {
		if(conv->ismv_ctx[i]->moov->data) {
		    free(conv->ismv_ctx[i]->moov->data);
		    conv->ismv_ctx[i]->moov->data = NULL;
		}
		mp4_cleanup_ismv_ctx_attach_buf(conv->ismv_ctx[i]);
		mp4_moov_cleanup(conv->ismv_ctx[i]->moov,
				 conv->ismv_ctx[i]->moov->n_tracks);
		mp4_cleanup_ismv_ctx(conv->ismv_ctx[i]);
	    }
	    if(&conv->reader[i]) {
		ismv_reader_cleanup_proc_buf(conv->reader[i]->out);
		ismv_reader_cleanup(conv->reader[i]);
	    }
	    if(bldr->ismv_name[i])
		free(bldr->ismv_name[i]);
	}
	ismv2moof_cleanup_moof_builder(conv->bldr);
	free(conv);
	conv = NULL;
    }
}

static void
ismv2moof_cleanup_moof_builder(ismv_moof_builder_t *bldr)
{
    if (bldr != NULL) {
	if (bldr->ismc_data != NULL) {
	    free(bldr->ismc_data);
	    bldr->ismc_data = NULL;
	}
	if (bldr->output_path != NULL) {
	    free(bldr->output_path);
	    bldr->output_path = NULL;
	}
	if (bldr->output_video_name != NULL) {
	    free(bldr->output_video_name);
	    bldr->output_video_name = NULL;
	}
	if (bldr->ismv_file_path != NULL) {
	    free(bldr->ismv_file_path);
            bldr->ismv_file_path = NULL;
        }
	if (bldr->ism_map != NULL)
	    ism_cleanup_map(bldr->ism_map);
	if (bldr->psi != NULL) 
	    ismc_cleanup_profile_map(		\
				     bldr->psi);
	if (bldr->ismv_file != NULL)
	    free(bldr->ismv_file);
	free(bldr);
	bldr = NULL;
    }
}

void
ismv2moof_cleanup_writer_array(ismv2moof_write_array_t *writer_array)
{
    uint32_t i=0;
    ismv2moof_write_t *writer;
    if (writer_array != NULL) {
	for(i=0;i<writer_array->num_tracks_towrite;i++) {
	    writer = &writer_array->writer[i];
	    if (writer->out != NULL)
                ismv_reader_cleanup_proc_buf(writer->out);
	}
	free(writer_array);
        writer_array = NULL;
        ismv2moof_write_array_free++;
    }

}
static int32_t 
ismv2moof_setup_writer(ismv2moof_write_t *writer,
		       ismv_moof_builder_t *bldr,
		       ismv_moof_proc_buf_t *proc_buf, 
		       int32_t trak_type_name, int32_t curr_profile,
		       int32_t trak_type_idx, int32_t
		       curr_profile_idx)
{
    writer->out = proc_buf;
    writer->video_name = bldr->output_video_name;
    writer->output_dir = bldr->output_path;
    writer->profile = curr_profile;
    writer->trak_type = trak_type_idx;
    writer->trak_type_name = trak_type_name;
    writer->psi = bldr->psi;
    writer->ism_map = bldr->ism_map;
    writer->curr_profile_idx = curr_profile_idx;

    return 0;
}
 


static int32_t
ismv2moof_fill_buffer(ismv2moof_converter_ctx_t *conv_ctx, void **out)
{
    ismv_parser_ctx_t *ctx = NULL;
    ismv_reader_t *reader = NULL;
    ismv_moof_proc_buf_t *proc_buf = NULL;
    ismv_moof_builder_t *bldr = NULL;
    ismv2moof_write_array_t *writer_array = NULL;
    int32_t err = 0;
    int32_t i=0, m=0;
    uint8_t total_processed =1;
    int32_t k=0;
    uint32_t j = 0;
    uint32_t temp =0;
    trak_t  *trak = NULL;
    int32_t trak_type_name = 0;
    uint32_t n_cust_attrs = 0;

    bldr = conv_ctx->bldr;
    //ctx = conv_ctx->ismv_ctx;
    // bldr->curr_profile_idx = 0;
    //check whether all the tracks are processed
    for(i=0;i< bldr->n_traks;i++) {
	total_processed &= bldr->is_processed[i];
    }
    if(total_processed == 0) {
	writer_array = (ismv2moof_write_array_t*)nkn_calloc_type\
	  (1,
	   sizeof(ismv2moof_write_array_t),
	   mod_vpe_ismv2moof_write_array_t);
	if (!writer_array) {
	  return -E_VPE_PARSER_NO_MEM;
	}

	writer_array->total_moof_count = bldr->total_moof_count;
	writer_array->iocb = bldr->iocb;
	writer_array->num_tracks_towrite = 0;
	ismv2moof_write_array_alloc++;
	for (i=0;i< MAX_TRAK_TYPES;i++) {
	    temp = bldr->psi->attr[i]->n_profiles;
	    if(temp !=0) {
		for (j=0;j<temp;j++) {
		    ctx = conv_ctx->ismv_ctx[k];
                    reader = conv_ctx->reader[k];

		    //bldr->ismv_file = fopen(bldr->ismv_name[k],
		    //"rb");
		    bldr->io_desc = bldr->iocb->ioh_open((char*)\
							bldr->ismv_name[k],
							"rb", 0);
		    if (!bldr->io_desc) {
			err = -E_VPE_PARSER_INVALID_FILE;
			printf("invalid input path (errno=%d)\n",
			       err);
			for(m=0; m<k; m++) {
			  writer_array->writer[m].out = proc_buf;
			  ismv2moof_cleanup_writer_array(writer_array);
			}
			return err;
		    }
		    if(bldr->is_processed[k] == 0) {
			err = ismv_reader_fill_buffer(reader, bldr->io_desc,
						      bldr->trak_num[k], ctx,
						      &proc_buf);
			if (err == E_ISMV_NO_MORE_FRAG) {
			    /* need to flush the reader state */
			    /* end of a track/profile. update builder state */
			    bldr->is_processed[k] = 1;
			    ismv_reader_flush(reader, bldr->io_desc,
					      (void**)&proc_buf, ctx->iocb); 
			    ismv_reader_reset(reader);
			}
			if (!proc_buf) {
			    if( bldr->io_desc)
				bldr->iocb->ioh_close( bldr->io_desc);
			    return E_VPE_FILE_CONVERT_CONTINUE;
			}
			ismv2moof_setup_writer(&writer_array->writer[k], bldr,
					       proc_buf,
					       bldr->trak_type_name[k], 
					       bldr->curr_profile[k], 
					       bldr->trak_type_idx[k], 
					       bldr->curr_profile_idx[k]);
		    }
		    k++;
		    if( bldr->io_desc)
			bldr->iocb->ioh_close( bldr->io_desc);
		}//for loop for j
	    }//if(temp !=0)
	}//for loop for i
	*out = writer_array;
	writer_array->num_tracks_towrite = k;
	writer_array->num_pmf = bldr->num_pmf;
    } else {
	if (bldr->streaming_type == MOBILE_STREAMING)
	    return E_VPE_FILE_CONVERT_SKIP;
	if (bldr->streaming_type == SMOOTH_STREAMING) {
	    err = ismv2moof_copy_manifest(bldr);
	    if (err) {
		return err;
	    }
	    return E_VPE_FILE_CONVERT_STOP;
	}
    }
    
    return E_VPE_FILE_CONVERT_CONTINUE;
}

int32_t 
init_ismv_moof_builder(int32_t chunk_time,
		       int32_t n_traks,
		       uint8_t ss_package_type,
		       char *output_path,
		       char **sl_pkg_filenames,
		       int32_t streaming_type,
		       int32_t num_pmf,
		       ismv_moof_builder_t **out)
{

    ismv_moof_builder_t *bldr = NULL;
    xml_read_ctx_t *ismc = NULL;
    ismc_publish_stream_info_t *psi = NULL;
    int32_t err;
    char *tmp = NULL, tmp1[500] =  {'\0'}, *tmp2 = NULL;
    uint8_t *ismc_data = NULL;
    FILE *fismc = NULL;
    struct stat st;
    uint32_t string_len = 0;
    char ismc_name[256];
    uint32_t bytes_read = 0;
    err = 0;


    *out = bldr = (ismv_moof_builder_t*)
	nkn_calloc_type(1, sizeof(ismv_moof_builder_t),
			mod_ismv_moof_builder_t);
    if (!bldr) {
	return -E_VPE_PARSER_NO_MEM;
    }
    bldr->io_desc_output = NULL;
    bldr->io_desc_ism = NULL;
    bldr->io_desc = NULL;
    /*extracting the path of .ism file to use it for .ismc file and
    .ismv file*/
    //check whether ther is no src in pmf
    if (sl_pkg_filenames[1]==NULL) {
      return -E_VPE_INVALID_FILE_FROMAT;
    }
    // check whether the src is a valid format
    if (strlen(sl_pkg_filenames[1]) < 5) { //x.ism strlen will be at least 5
      return -E_VPE_INVALID_FILE_FROMAT;
    }
    tmp = strrchr(sl_pkg_filenames[1], '/')+1;
    string_len = tmp - sl_pkg_filenames[1] ;
    strncpy(tmp1,sl_pkg_filenames[1],string_len);
    tmp1[string_len] = '\0';
    bldr->ismv_file_path = strdup(tmp1);

    /* checking whther the correct file format is given as input
    argument*/
    tmp2 = strrchr(tmp, '.');
    if(tmp2 != NULL ) {
	if (strncmp(tmp2+1, "ism", 3) != 0) {
	    return -E_VPE_INVALID_FILE_FROMAT;
	}
    } else {
	return -E_VPE_INVALID_FILE_FROMAT;
    }


    bldr->streaming_type = streaming_type;
    if (strstr(sl_pkg_filenames[1], "http://")) {
        bldr->iocb = &ioh_http;
    } else {
        bldr->iocb = &iohldr;
    }

    /* extraxcting the elements from .ism file and update in ism_map
       structure */
    err = ism_read_map_from_file(sl_pkg_filenames[1], &bldr->ism_map, bldr->iocb);
    if (err) {
        goto error;
    }

    tmp = strrchr(sl_pkg_filenames[1], '/');
    if (tmp) {
        bldr->output_video_name = strdup(tmp);
    } else {
        bldr->output_video_name = strdup((char*)sl_pkg_filenames[1]);
    }

    //forming ismc file name along with the path
    snprintf(ismc_name, 256, "%s/%s", bldr->ismv_file_path,
	     bldr->ism_map->ismc_name);

    if(!stat(ismc_name, &st)) {
	if (S_ISDIR(st.st_mode)) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    goto error;
	}
    } 
    bldr->io_desc_ismc = bldr->iocb->ioh_open((char*)\
					ismc_name,
					"rb", 0);
    if (!bldr->io_desc_ismc) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    bldr->iocb->ioh_seek(bldr->io_desc_ismc, 0, SEEK_END);
    bldr->ismc_size = bldr->iocb->ioh_tell(bldr->io_desc_ismc);
    ismc_data = bldr->ismc_data = \
	nkn_calloc_type(1, bldr->ismc_size,
			mod_vpe_media_buffer1);
    if (!bldr->ismc_data) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }
    bldr->iocb->ioh_seek(bldr->io_desc_ismc, 0, SEEK_SET);
    bytes_read = bldr->iocb->ioh_read(bldr->ismc_data, 1, bldr->ismc_size,
				      bldr->io_desc_ismc);
    if(bytes_read != bldr->ismc_size) {
      err = -E_VPE_READ_INCOMPLETE;
      goto error;
    }
    ismc = init_xml_read_ctx((uint8_t*)bldr->ismc_data,
			     bldr->ismc_size);
    if (!ismc) {
	err = -E_ISMC_PARSE_ERR;
	goto error;
    }


    bldr->psi = ismc_read_profile_map(ismc);
    bldr->output_path = strdup(output_path);
    bldr->chunk_time = chunk_time;
    bldr->n_traks = n_traks;

    //considering only video
    bldr->profiles_to_process =
	bldr->psi->attr[0]->n_profiles;

    bldr->ss_package_type = ss_package_type;
    bldr->num_pmf = num_pmf;
    bldr->num_moof_processed = 0;
    bldr->total_moof_count = 0;

    if(bldr->io_desc_ismc) 
	bldr->iocb->ioh_close(bldr->io_desc_ismc);
    xml_cleanup_ctx(ismc);
    return err;
 error:
    if(ismc_data) free(ismc_data);
    if(bldr->io_desc_ismc)
        bldr->iocb->ioh_close(bldr->io_desc_ismc);
    if(bldr) free(bldr);
    if(ismc) xml_cleanup_ctx(ismc);
    return err;
}

static int32_t
ismv2moof_copy_manifest(ismv_moof_builder_t *bldr)
{
    char out_ismc_path[256];
    int32_t err;
    FILE *fismc = NULL;

    err = 0;

    snprintf(out_ismc_path, 256, "%s/%s/Manifest", bldr->output_path,
	    bldr->output_video_name + 1);

    fismc = fopen(out_ismc_path, "wb");
    if (!fismc) {
	return -E_VPE_PARSER_INVALID_OUTPUT_PATH;
    }
    fwrite(bldr->ismc_data, 1, bldr->ismc_size, fismc);
    fclose(fismc);

    return err;
    
}


#if 0
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

#endif

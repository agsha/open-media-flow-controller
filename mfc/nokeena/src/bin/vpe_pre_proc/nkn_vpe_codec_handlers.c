/*
 *
 * Filename:  nkn_vpe_codec_handlers.c
 * Date:      2009/02/06
 * Module:    Handler function for different audio and video codecs
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "nkn_vpe_codec_handlers.h"
#include "nkn_vpe_mp4_parser_legacy.h"
#include "nkn_vpe_metadata.h"
#include "math.h"
#include "inttypes.h"
#include "nkn_vpe_mpeg2ts_parser.h"
#include "nkn_vpe_flv_parser.h"

#define MAX_PATH 256
#define MAX_SUPPORTED_TRACKS 2

int 
VPE_PP_generic_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data)
{

    void  *out_desc;
    void  *in_desc;
    void  *meta_desc;
    uint32_t vts, n_video_packets, n_chunk;
    size_t pos;
    uint32_t prev_iframe_vts, curr_iframe_vts;
    nkn_vpe_flv_header_t fh;
    nkn_vpe_flv_tag_t ft;
    uint8_t end_of_chunk,meta_flag;
    uint32_t last_tag_size;
    uint32_t body_length, chunk_duration;
    char vd;
    char out_fname[256];
    size_t input_size;
    char *p_flv_tag_data, *video_name;
    int error_no;

    private_data = private_data;  
    profile = 0;
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    input_size = 0;
    last_tag_size = 0;
    error_no = 0;
    p_flv_tag_data = NULL;
    in_desc = out_desc = meta_desc = NULL;
    prev_iframe_vts = curr_iframe_vts = 0;
    video_name = md->video_name;
    chunk_duration = md->smooth_flow_duration;
    vts = 0;    

    in_desc=(FILE*)in_de;

    //get size of input data
    md->content_length = input_size = iof->nkn_get_size(in_desc);
    if(input_size <= 13){
	printf("Ilegal FLV structure\n");
	error_no = NKN_VPE_E_CONTAINER_DATA;
	goto cleanup;
    }
 
    if(!check_free_space(out_path, input_size)){
	printf("not enought free space\n");
	error_no = NKN_VPE_E_FREE_SPACE;
	goto cleanup;
    }

    //read FLV header and first tag size
    iof->nkn_read(&fh, 1, sizeof(fh), in_desc);
    iof->nkn_seek(in_desc, sizeof(uint32_t), 1);

    if(!check_flv_header(&fh)){
	error_no = -1;
	goto cleanup;
    }
 
    //open the output file (ATM we support only FILE I/O for output
    //but it is via the IO_BACKEND_PLUGIN
    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
    if(io_mode == 0){
	out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
    }else{
	out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
    }

    if(out_desc == NULL){
	printf("unable to open output file\n");
	return -NKN_VPE_E_INVALID_HANDLE;
    }

    //copy FLV header and first tag size into output stream
    iof->nkn_write(&fh, 1, sizeof(fh), out_desc);
    iof->nkn_write(&last_tag_size, 1, sizeof(last_tag_size), out_desc);
      
   
    //store the current position of the stream, before parsing the FLV header!
    pos = iof->nkn_tell(in_desc);
    // if(pos >= input_size)
    //   end_of_chunk = 1;	  
 
    //parse tags sequentially, check if they are Audio/Video/Meta and if the 
    //Video tag timestamp crosses the chunk duration cut till that point. Obviously 
    //this is done only at I-Frame boundaries
    while(pos < input_size){
      
	pos = iof->nkn_tell(in_desc);
      
	//read FLV Tag
	iof->nkn_read(&ft, 1, sizeof(ft), in_desc);
      
	//read FLV Body Length (Big Endian :-( )
	body_length = nkn_vpe_convert_uint24_to_uint32(ft.body_length);
      
	//allocate data for the tag, 
	//NOTE do we need to do this everytime? can we allocate a big block of memory and reuse it?
	p_flv_tag_data = (char*)malloc(body_length);

	//sanity
	if(p_flv_tag_data==NULL){
	    printf("unable to allocate memory of %d bytes\n", body_length);
	    error_no = NKN_VPE_E_MEM;
	    goto cleanup;
	}
      
	//handler for Audio/Video and Meta Tags
	switch(ft.type){
	    case NKN_VPE_FLV_TAG_TYPE_VIDEO:
				
		iof->nkn_read(&vd, 1, sizeof(char), in_desc);//(flv_video_data*)(bs->_data);
		iof->nkn_seek(in_desc, ((int)(-1*sizeof(char))), 1);
		meta_flag = 0;
		vts = nkn_vpe_flv_get_timestamp(ft);
				
		if(nkn_vpe_flv_video_frame_type(vd) == NKN_VPE_FLV_VIDEO_FRAME_TYPE_KEYFRAME){
		    if(vts > (n_chunk *chunk_duration * 1000)){
			curr_iframe_vts = vts;
			md->iframe_interval = curr_iframe_vts - prev_iframe_vts;
			prev_iframe_vts = curr_iframe_vts;
			end_of_chunk = 1;
			break;
		    }
			
		}
		n_video_packets++;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_AUDIO:
		vts = nkn_vpe_flv_get_timestamp(ft);
		meta_flag = 0;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_META:
		meta_flag = 1;
		//end_of_chunk=1;
		break;
	    default:
		break;
	}
      
	//read data packet and the current tag size (inclusive of the FLV tag header '11 bytes'
	iof->nkn_read(p_flv_tag_data, 1,body_length,in_desc); 
	iof->nkn_read(&last_tag_size, 1,sizeof(last_tag_size),in_desc);
        
	if(last_tag_size==0)
	    {
		printf("File/Tag corrupted\n");
		return -1;}
      
	//if not end of chunk continue parsing and copying into output stream
	if(!end_of_chunk){
	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);
	    //  iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
			
	}
	else if(end_of_chunk){
	    n_chunk++;
	    if(io_mode == 0){
		iof->nkn_write_stream_to_file(out_desc, out_fname);
	    }
	    iof->nkn_close(out_desc);
	    snprintf(out_fname,strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
	  
	    if(io_mode == 0){
		out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
	    }else{
		out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
	    }

	    if(out_desc == NULL){
		printf("unable to open output file\n");
		error_no = NKN_VPE_E_INVALID_HANDLE;
		return -1;
	    }

	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);	
	    //iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
	    end_of_chunk = 0;
			
	}
	free(p_flv_tag_data);
	p_flv_tag_data = NULL;
	    
	pos = iof->nkn_tell(in_desc); 
    }

    if(io_mode == 0&&!end_of_chunk){
	iof->nkn_write_stream_to_file(out_desc, out_fname);
    }

    md->n_chunks = n_chunk;
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    last_tag_size = 0;

    md->sequence_duration = vts;
    //close file descriptors for the last chunk
    iof->nkn_close(out_desc);
    out_desc = NULL;
    
    //set the bitrate of the media asset
    //videobitrate+audiobitrate+containeroverheads
    md->bitrate = ((input_size/vts)*8); //Kbits per second

    return error_no;

 cleanup:
    if(p_flv_tag_data)
	free(p_flv_tag_data);
    if(out_desc)
	iof->nkn_close(out_desc);
    if(meta_desc)
	iof->nkn_close(meta_desc);

    return error_no;

}

//#define DIAG
/** This function splits a FLV file at I-Frames closest to the chunk interval specified
 * void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 * uint32_t n_profiles [in]          - number of encoded bitrates/profiles 
 * uint32_t chunk_interval [in]      - sync point interval
 * char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 * char *out_path [in]             - null ternminated string with output path to which to write the files
 * charn *video_name [in]          - null terminated string with the file name
 * io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int 
VPE_PP_h264_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data)
{

    void  *out_desc;
    void  *in_desc;
    void  *meta_desc;
    uint32_t vts, n_video_packets, n_chunk, n_avcc_packets;
    uint32_t curr_iframe_vts, prev_iframe_vts;
    nkn_vpe_flv_header_t fh;
    nkn_vpe_flv_tag_t ft;
    uint8_t end_of_chunk,meta_flag;
    uint32_t last_tag_size;
    uint32_t body_length;
    char vd;
    char out_fname[MAX_PATH];
    size_t input_size;
    char *p_flv_tag_data, *p_avcc_data, *avcc_data,*video_name;
    int  avcc_size;
    char *str;//numbers more than 256 decimal digits are not supported
    int  error_no;
    size_t pos;
    uint32_t chunk_interval;
    avcc_config *avcc;

#ifdef DIAG
    FILE *f_diag;
    char diag_fname[1024];
    size_t diag_reset_pos;
    int32_t fr_no, diag_dur;
    uint32_t diag_tagsize;
    nkn_vpe_flv_tag_t diag_ft;
    int32_t gop_bitrate, max_bitrate;
    size_t gop_start_offset;
#endif
  
    //Chunker State Init --> Should we put all this in a state variable?
    //it becomes elegant and easy to cleanup and recover from errors!
    //ugly goto's can be avoided
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    input_size = 0;
    last_tag_size = 0;
    error_no = 0;
    avcc_size = 0;
    p_flv_tag_data = NULL;
    p_avcc_data = avcc_data =  NULL;
    str = NULL;
    in_desc = out_desc = meta_desc = NULL;
    avcc = NULL;
    n_avcc_packets = 0;
    curr_iframe_vts = prev_iframe_vts = 0;
    vts = 0;
    private_data = private_data;

    video_name = md->video_name;
    chunk_interval = md->smooth_flow_duration;

    in_desc=(FILE*)in_de;

    avcc = (avcc_config*)calloc(1, sizeof(avcc_config));

    //get size of input data
    md->content_length = input_size = iof->nkn_get_size(in_desc);
    if(input_size <= 13){
	printf("Ilegal FLV structure\n");
	error_no = NKN_VPE_E_CONTAINER_DATA;
	goto cleanup;
    }
 
    if(!check_free_space(out_path, input_size)){
	printf("not enought free space\n");
	error_no = NKN_VPE_E_FREE_SPACE;
	goto cleanup;
    }

    //read FLV header and first tag size
    iof->nkn_read(&fh, 1, sizeof(fh), in_desc);
    iof->nkn_seek(in_desc, sizeof(uint32_t), 1);

    if(!check_flv_header(&fh)){
	error_no = NKN_VPE_E_CONTAINER_DATA;
	goto cleanup;
    }
 
    //open the output file (ATM we support only FILE I/O for output
    //but it is via the IO_BACKEND_PLUGIN
    snprintf(out_fname,strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
    if(io_mode == 0){
	out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
    }else{
	out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
    }

    if(out_desc == NULL){
	printf("unable to open output file\n");
	return NKN_VPE_E_INVALID_HANDLE;
    }

    //copy FLV header and first tag size into output stream
    iof->nkn_write(&fh, 1, sizeof(fh), out_desc);
    iof->nkn_write(&last_tag_size, 1, sizeof(last_tag_size), out_desc);

#ifdef DIAG
    max_bitrate =0;
    gop_bitrate = 0;
    gop_start_offset = 0;
    memset(diag_fname, 0, sizeof(diag_fname));
    strcat(diag_fname, out_fname);
    strcat(diag_fname, ".csv");
    f_diag = fopen(diag_fname, "wb");
    fr_no = 1;
    diag_dur = 0;
    diag_reset_pos = 0;
    diag_tagsize = 0;

    //read the last timestamp for the duration
    diag_reset_pos = iof->nkn_tell(in_desc);
    iof->nkn_seek(in_desc, -sizeof(int32_t), SEEK_END);
    iof->nkn_read(&diag_tagsize, 1, sizeof(int32_t), in_desc);
    diag_tagsize = swap_uint32(diag_tagsize);
    iof->nkn_seek(in_desc, -((int32_t)(diag_tagsize))-sizeof(int32_t), SEEK_END);
    iof->nkn_read(&diag_ft, 1, sizeof(flv_tag), in_desc);
    diag_dur = ceil(flv_tag_get_timestamp(diag_ft)/1000);
    iof->nkn_seek(in_desc, diag_reset_pos, SEEK_SET);
    fprintf(f_diag, "File Size,%ld\n", md->content_length);
    fprintf(f_diag, "I-Frame TimeStamp,Frame Number,I-Frame Interval(ms),GOP size (bytes),GOP bitrate(Bytes/s),Avg Bitrate(Bytes/s)");
#endif
   
    //store the current position of the stream, before parsing the FLV header!
    pos = iof->nkn_tell(in_desc);

    // if(pos >= input_size)
    //   end_of_chunk = 1;	  
 
    //parse tags sequentially, check if they are Audio/Video/Meta and if the 
    //Video tag timestamp crosses the chunk duration cut till that point. Obviously 
    //this is done only at I-Frame boundaries
    while(pos < input_size){
      
	pos = iof->nkn_tell(in_desc);
      
	//read FLV Tag
	iof->nkn_read(&ft, 1, sizeof(ft), in_desc);
      
	//read FLV Body Length (Big Endian :-( )
	body_length = nkn_vpe_convert_uint24_to_uint32(ft.body_length);
      
	//allocate data for the tag, 
	//NOTE do we need to do this everytime? can we allocate a big block of memory and reuse it?
	p_flv_tag_data = (char*)malloc(body_length);

	//sanity
	if(p_flv_tag_data==NULL){
	    printf("unable to allocate memory of %d bytes\n", body_length);
	    error_no = NKN_VPE_E_MEM;
	    goto cleanup;
	}
      
	//handler for Audio/Video and Meta Tags
	switch(ft.type){
	    case NKN_VPE_FLV_TAG_TYPE_VIDEO:
				
		iof->nkn_read(&vd, 1, sizeof(char), in_desc);//(flv_video_data*)(bs->_data);
		iof->nkn_seek(in_desc, ((int)(-1*sizeof(char))), 1);
		meta_flag = 0;
		vts = nkn_vpe_flv_get_timestamp(ft);
#ifdef DIAG
		fr_no++;
#endif				
		if(nkn_vpe_flv_video_frame_type(vd) == NKN_VPE_FLV_VIDEO_FRAME_TYPE_KEYFRAME){

		    if(vts > (n_chunk * chunk_interval * 1000)){
			curr_iframe_vts = vts;
			md->iframe_interval = curr_iframe_vts - prev_iframe_vts;
			prev_iframe_vts = curr_iframe_vts;
			end_of_chunk = 1;
#ifdef DIAG
			gop_bitrate = (pos - gop_start_offset)/(md->iframe_interval/1000);
			max_bitrate = ((gop_bitrate > max_bitrate) * gop_bitrate) + ((gop_bitrate <= max_bitrate) * max_bitrate);
			fprintf(f_diag, "\n%d,%d,%d,%ld", vts, fr_no, md->iframe_interval,pos-gop_start_offset);
			gop_start_offset = pos;
#endif
			break;
		    }
		}
		
		meta_flag = 2;
		n_video_packets++;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_AUDIO:
		vts = nkn_vpe_flv_get_timestamp(ft);
		meta_flag = 0;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_META:
		meta_flag = 1;
	}
      
	//read data packet and the current tag size (inclusive of the FLV tag header '11 bytes'
	iof->nkn_read(p_flv_tag_data, 1,body_length,in_desc); 
	iof->nkn_read(&last_tag_size, 1,sizeof(last_tag_size),in_desc);
	
	if(last_tag_size==0){
	    printf("FLV tag size corrupted\n");
	    return NKN_VPE_E_CONTAINER_DATA;
	}

	if(n_video_packets == 1 && meta_flag == 2){
	    avcc_size = body_length+sizeof(nkn_vpe_flv_tag_t)+sizeof(uint32_t);
	    p_avcc_data = avcc_data = (char*)malloc(avcc_size);

	    //make a copy of AVCC Atom = SPS+PPS+{SEI optional}
	    memcpy(p_avcc_data, &ft, sizeof(ft));
	    p_avcc_data = p_avcc_data + sizeof(ft);
	    memcpy(p_avcc_data, p_flv_tag_data, body_length);
	    p_avcc_data = p_avcc_data + body_length;
	    memcpy(p_avcc_data, &last_tag_size, sizeof(uint32_t));
	    p_avcc_data = p_avcc_data + sizeof(uint32_t);

	    avcc->p_avcc_data = avcc_data;		
	    avcc->avcc_data_size = avcc_size;

	    *private_data = avcc;

	}

	//if not end of chunk continue parsing and copying into output stream
	if(!end_of_chunk){
	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);
	    //  iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
			
	}
	else if(end_of_chunk){
	    n_chunk++;
	    if(io_mode == 0){
		iof->nkn_write_stream_to_file(out_desc, out_fname);
	    }
#ifdef DIAG
	    fprintf(f_diag, ",%ld,%ld", iof->nkn_tell(out_desc)/(md->iframe_interval/1000), input_size/diag_dur);
#endif 
	    iof->nkn_close(out_desc);
	    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
  
	    if(io_mode == 0){
		out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
	    }else{
		out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");

	    }

	    if(out_desc == NULL){
		printf("unable to open output file\n");
		error_no = NKN_VPE_E_INVALID_HANDLE;
		return -1;
	    }

	    //the first packet of the file is always AVCC (in case there is change in coding tools
	    //between different profiles
	    //iof->nkn_write(avcc_data, 1, avcc_size, out_desc);
	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);	
	    //iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
	    end_of_chunk = 0;
			
	}
	free(p_flv_tag_data);
	p_flv_tag_data = NULL;
	pos = iof->nkn_tell(in_desc); 
    }

    if(io_mode == 0&&!end_of_chunk){
	iof->nkn_write_stream_to_file(out_desc, out_fname);
    }
    md->n_chunks = n_chunk;


    n_avcc_packets = 0;
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    last_tag_size = 0;
    avcc_size = 0;

    md->sequence_duration = vts;
    //close file descriptors for the last chunk
    iof->nkn_close(out_desc);

    out_desc = NULL;

    //set the bitrate of the media asset
    //videobitrate+audiobitrate+containeroverheads
#ifdef DIAG
    fprintf(f_diag,"\nMax Bitrate (Bytes/s)%d\n",max_bitrate);
#endif

    md->bitrate = ((input_size/vts)*8); //Kbits per second
#ifdef DIAG
    max_bitrate = max_bitrate*8/1000;//Kbits per second
    md->bitrate = ((max_bitrate > md->bitrate) * max_bitrate) + ((max_bitrate <= md->bitrate) * md->bitrate);
    fclose(f_diag);
#endif

    return error_no;

    //Chunker enters clean up state
 cleanup:
    if(p_flv_tag_data)
	free(p_flv_tag_data);
    if(out_desc)
	iof->nkn_close(out_desc);
    if(meta_desc)
	iof->nkn_close(meta_desc);
    //if(avcc_data)
    //free(avcc_data);

    
    return error_no;

}


/** This function splits a FLV file at I-Frames closest to the chunk interval specified
 * void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 * uint32_t n_profiles [in]          - number of encoded bitrates/profiles 
 * uint32_t chunk_interval [in]      - sync point interval
 * char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 * char *out_path [in]             - null ternminated string with output path to which to write the files
 * charn *video_name [in]          - null terminated string with the file name
 * io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int 
VPE_PP_generic_chunking_with_hint(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof,  void **private_data )
{

    void  *out_desc;
    void  *in_desc;
    void  *meta_desc;
    void *hint_desc;
    uint32_t  vts, n_video_packets, n_chunk, n_avcc_packets;
    size_t pos;
    uint32_t curr_iframe_vts, prev_iframe_vts;
    nkn_vpe_flv_header_t fh;
    nkn_vpe_flv_tag_t ft;
    uint8_t end_of_chunk,meta_flag;
    uint32_t last_tag_size;
    uint32_t body_length;
    char vd;
    char out_fname[MAX_PATH];
    size_t input_size;
    char *p_flv_tag_data, *p_avcc_data, *avcc_data, *video_name;
    int  avcc_size, n_iframes;
    char *str;//numbers more than 256 decimal digits are not supported
    int error_no;
    uint32_t chunk_interval;
    GList *tplay_hint_list;
    packet_info *pkt;
  
    //Chunker State Init --> Should we put all this in a state variable?
    //it becomes elegant and easy to cleanup and recover from errors!
    //ugly goto's can be avoided
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    input_size = 0;
    last_tag_size = 0;
    error_no = 0;
    avcc_size = 0;
    p_flv_tag_data = NULL;
    p_avcc_data = avcc_data =  NULL;
    str = NULL;
    in_desc = out_desc = meta_desc = NULL;
    n_avcc_packets = 0;
    tplay_hint_list = NULL;
    pkt = NULL;
    hint_desc = NULL;
    n_iframes = 0;
    curr_iframe_vts = prev_iframe_vts = 0;
    vts = 0;
    private_data = private_data;

    video_name = md->video_name;
    chunk_interval = md->smooth_flow_duration;


    in_desc=(FILE*)in_de;

    //get size of input data
    md->content_length = input_size = iof->nkn_get_size(in_desc);
    if(input_size <= 13){
	printf("Ilegal FLV structure\n");
	error_no = NKN_VPE_E_CONTAINER_DATA;
	goto cleanup;
    }
 
    if(!check_free_space(out_path, input_size)){
	printf("not enought free space\n");
	error_no = NKN_VPE_E_FREE_SPACE;
	goto cleanup;
    }

    //read FLV header and first tag size
    iof->nkn_read(&fh, 1, sizeof(fh), in_desc);
    iof->nkn_seek(in_desc, sizeof(uint32_t), 1);

    if(!check_flv_header(&fh)){
	error_no = NKN_VPE_E_CONTAINER_DATA;
	goto cleanup;
    }
 
    //open the output file (ATM we support only FILE I/O for output
    //but it is via the IO_BACKEND_PLUGIN
    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
    if(io_mode == 0){
	out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
    }else{
	out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
    }

    if(out_desc == NULL){
	printf("unable to open output file\n");
	return NKN_VPE_E_INVALID_HANDLE;
    }

    //copy FLV header and first tag size into output stream
    iof->nkn_write(&fh, 1, sizeof(fh), out_desc);
    iof->nkn_write(&last_tag_size, 1, sizeof(last_tag_size), out_desc);

   
    //store the current position of the stream, before parsing the FLV header!
    pos = iof->nkn_tell(in_desc);

    // if(pos >= input_size)
    //   end_of_chunk = 1;	  
 
    //parse tags sequentially, check if they are Audio/Video/Meta and if the 
    //Video tag timestamp crosses the chunk duration cut till that point. Obviously 
    //this is done only at I-Frame boundaries
    while(pos < input_size){
      
	pos = iof->nkn_tell(in_desc);
      
	//read FLV Tag
	iof->nkn_read(&ft, 1, sizeof(ft), in_desc);
      
	//read FLV Body Length (Big Endian :-( )
	body_length = nkn_vpe_convert_uint24_to_uint32(ft.body_length);
      
	//allocate data for the tag, 
	//NOTE do we need to do this everytime? can we allocate a big block of memory and reuse it?
	p_flv_tag_data = (char*)malloc(body_length);

	//sanity
	if(p_flv_tag_data==NULL){
	    printf("unable to allocate memory of %d bytes\n", body_length);
	    error_no = -1;
	    goto cleanup;
	}
      
	//handler for Audio/Video and Meta Tags
	switch(ft.type){
	    case NKN_VPE_FLV_TAG_TYPE_VIDEO:
				
		iof->nkn_read(&vd, 1, sizeof(char), in_desc);//(flv_video_data*)(bs->_data);
		iof->nkn_seek(in_desc, ((int)(-1*sizeof(char))), 1);
		meta_flag = 0;
		vts = nkn_vpe_flv_get_timestamp(ft);
				
		if(nkn_vpe_flv_video_frame_type(vd) == NKN_VPE_FLV_VIDEO_FRAME_TYPE_KEYFRAME){
		    n_iframes++;
		    curr_iframe_vts = vts;
		    md->iframe_interval = curr_iframe_vts - prev_iframe_vts;
		    prev_iframe_vts = curr_iframe_vts;
		    pkt = (packet_info*)malloc(sizeof(packet_info));
		    pkt->byte_offset = pos;
		    pkt->time_offset = vts;
		    pkt->size = body_length + sizeof(ft) + sizeof(uint32_t);
		    if(!(tplay_hint_list = g_list_prepend(tplay_hint_list, pkt))){
			printf("Unable to append to list\n");
			error_no = NKN_VPE_E_TPLAY_LIST;
			goto cleanup;
		    }
		    if(vts > (n_chunk * chunk_interval * 1000)){
			end_of_chunk = 1;
			break;
		    }
		}
			
		meta_flag = 2;
		n_video_packets++;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_AUDIO:
		vts = nkn_vpe_flv_get_timestamp(ft);
		meta_flag = 0;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_META:
		meta_flag = 1;
	}
      
	//read data packet and the current tag size (inclusive of the FLV tag header '11 bytes'
	iof->nkn_read(p_flv_tag_data, 1,body_length,in_desc); 
	iof->nkn_read(&last_tag_size, 1,sizeof(last_tag_size),in_desc);

	if(last_tag_size==0){
	    printf("FLV tag size corrupted\n");
	    return -1;}

	//if not end of chunk continue parsing and copying into output stream
	if(!end_of_chunk){
	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);
	    //  iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
			
	}
	else if(end_of_chunk){
	    n_chunk++;
	    if(io_mode == 0){
		iof->nkn_write_stream_to_file(out_desc, out_fname);
	    }
	    iof->nkn_close(out_desc);
	    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
	  
	    if(io_mode == 0){
		out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
	    }else{
		out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
	    }

	    if(out_desc == NULL){
		printf("unable to open output file\n");
		error_no = NKN_VPE_E_INVALID_HANDLE;
		return -1;
	    }

	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);	
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
	    end_of_chunk = 0;
			
	}
	free(p_flv_tag_data);
	p_flv_tag_data = NULL;
	pos = iof->nkn_tell(in_desc); 
    }

    if(io_mode == 0&&!end_of_chunk){
	iof->nkn_write_stream_to_file(out_desc, out_fname);
    }
    md->n_chunks = n_chunk;

    n_avcc_packets = 0;
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    last_tag_size = 0;
    avcc_size = 0;
    free(avcc_data);
    avcc_data =  p_avcc_data = NULL;

    md->sequence_duration = vts;

    //set the bitrate of the media asset
    //videobitrate+audiobitrate+containeroverheads
    md->bitrate = ((input_size/vts)*8); //Kbits per second

    //close file descriptors for the last chunk
    iof->nkn_close(out_desc);

    out_desc = NULL;
 
    //return error_no;

    //set the bitrate of the media asset
    //videobitrate+audiobitrate+containeroverheads
    //md->bitrate = ((input_size/vts)*8); //Kbits per second

    //write hint track 
    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%.2d.hint", out_path, video_name, profile);
    hint_desc = (void*)fopen(out_fname, "wb");
    if(!hint_desc){
	error_no = NKN_VPE_E_INVALID_HANDLE;
	goto cleanup;
    }
    if((error_no = write_hint_track(hint_desc, tplay_hint_list, n_iframes, vts))){
	printf("error generating hint track\n");
	goto cleanup;
    }

    fclose(hint_desc);
    free_tplay_list(tplay_hint_list);
    return error_no;

    //Chunker enters clean up state
 cleanup:
    if(p_flv_tag_data)
	free(p_flv_tag_data);
    if(out_desc)
	iof->nkn_close(out_desc);
    if(meta_desc)
	iof->nkn_close(meta_desc);
    if(avcc_data)
	free(avcc_data);
    
    return error_no;

}

//#define _DEBUG
/** This function splits a FLV file at I-Frames closest to the chunk interval specified
 * void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 * uint32_t n_profiles [in]          - number of encoded bitrates/profiles 
 * uint32_t chunk_interval [in]      - sync point interval
 * char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 * char *out_path [in]             - null ternminated string with output path to which to write the files
 * charn *video_name [in]          - null terminated string with the file name
 * io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int 
VPE_PP_h264_chunking_with_hint(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data )
{

    void  *out_desc;
    void  *in_desc;
    void  *meta_desc;
    void *hint_desc;
    uint32_t  vts, n_video_packets, n_chunk, n_avcc_packets;
    size_t pos;
    uint32_t prev_iframe_vts, curr_iframe_vts;
    nkn_vpe_flv_header_t fh;
    nkn_vpe_flv_tag_t ft;
    uint8_t end_of_chunk,meta_flag;
    uint32_t last_tag_size;
    uint32_t body_length;
    char vd;
    char out_fname[MAX_PATH];
    size_t input_size;
    char *p_flv_tag_data, *p_avcc_data, *avcc_data, *video_name;
    int  avcc_size, n_iframes;
    char *str;//numbers more than 256 decimal digits are not supported
    int error_no;
    uint32_t chunk_interval;
    GList *tplay_hint_list;
    packet_info *pkt;
    avcc_config *avcc;

#ifdef _DEBUG
    int pkt_no;
#endif
  
    //Chunker State Init --> Should we put all this in a state variable?
    //it becomes elegant and easy to cleanup and recover from errors!
    //ugly goto's can be avoided
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    input_size = 0;
    last_tag_size = 0;
    error_no = 0;
    avcc_size = 0;
    p_flv_tag_data = NULL;
    p_avcc_data = avcc_data =  NULL;
    str = NULL;
    in_desc = out_desc = meta_desc = NULL;
    n_avcc_packets = 0;
    tplay_hint_list = NULL;
    pkt = NULL;
    hint_desc = NULL;
    n_iframes = 0;
    avcc = NULL;
    curr_iframe_vts = prev_iframe_vts = 0;
    vts = 0;
    private_data = private_data;

#ifdef _DEBUG
    pkt_no = 0;
#endif

    video_name = md->video_name;
    chunk_interval = md->smooth_flow_duration;

    avcc = (avcc_config*)calloc(1, sizeof(avcc_config));

    in_desc=(FILE*)in_de;

    //get size of input data
    md->content_length = input_size = iof->nkn_get_size(in_desc);
    if(input_size <= 13){
	printf("Ilegal FLV structure\n");
	error_no = NKN_VPE_E_CONTAINER_DATA;
	goto cleanup;
    }
 
    if(!check_free_space(out_path, input_size)){
	printf("not enought free space\n");
	error_no = NKN_VPE_E_FREE_SPACE;
	goto cleanup;
    }

    //read FLV header and first tag size
    iof->nkn_read(&fh, 1, sizeof(fh), in_desc);
    iof->nkn_seek(in_desc, sizeof(uint32_t), 1);

    if(!check_flv_header(&fh)){
	error_no = -1;
	goto cleanup;
    }
 
    //open the output file (ATM we support only FILE I/O for output
    //but it is via the IO_BACKEND_PLUGIN
    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
    if(io_mode == 0){
	out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
    }else{
	out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
    }

    if(out_desc == NULL){
	printf("unable to open output file\n");
	error_no = NKN_VPE_E_INVALID_HANDLE;
	return -1;
    }

    //copy FLV header and first tag size into output stream
    iof->nkn_write(&fh, 1, sizeof(fh), out_desc);
    iof->nkn_write(&last_tag_size, 1, sizeof(last_tag_size), out_desc);

   
    //store the current position of the stream, before parsing the FLV header!
    pos = iof->nkn_tell(in_desc);

    // if(pos >= input_size)
    //   end_of_chunk = 1;	  
 
    //parse tags sequentially, check if they are Audio/Video/Meta and if the 
    //Video tag timestamp crosses the chunk duration cut till that point. Obviously 
    //this is done only at I-Frame boundaries
    while(pos < input_size){
      
	pos = iof->nkn_tell(in_desc);
      
	//read FLV Tag
	iof->nkn_read(&ft, 1, sizeof(ft), in_desc);
      
	//read FLV Body Length (Big Endian :-( )
	body_length = nkn_vpe_convert_uint24_to_uint32(ft.body_length);
      
	//allocate data for the tag, 
	//NOTE do we need to do this everytime? can we allocate a big block of memory and reuse it?
	p_flv_tag_data = (char*)malloc(body_length);

	//sanity
	if(p_flv_tag_data==NULL){
	    printf("unable to allocate memory of %d bytes\n", body_length);
	    error_no = NKN_VPE_E_MEM;
	    goto cleanup;
	}
      
	//handler for Audio/Video and Meta Tags
	switch(ft.type){
	    case NKN_VPE_FLV_TAG_TYPE_VIDEO:
				
		iof->nkn_read(&vd, 1, sizeof(char), in_desc);//(flv_video_data*)(bs->_data);
		iof->nkn_seek(in_desc, ((int)(-1*sizeof(char))), 1);
		meta_flag = 0;
		vts = nkn_vpe_flv_get_timestamp(ft);
		
		if(nkn_vpe_flv_video_frame_type(vd) == NKN_VPE_FLV_VIDEO_FRAME_TYPE_KEYFRAME){
		    n_iframes++;
		    curr_iframe_vts = vts;
		    md->iframe_interval = curr_iframe_vts - prev_iframe_vts;
		    prev_iframe_vts = curr_iframe_vts;
		    pkt = (packet_info*)malloc(sizeof(packet_info));
		    pkt->byte_offset = pos;
		    pkt->time_offset = vts;
		    pkt->size = body_length + sizeof(ft) + sizeof(uint32_t);
		    if(!(tplay_hint_list = g_list_prepend(tplay_hint_list, pkt))){
			printf("Unable to append to list\n");
			error_no = NKN_VPE_E_TPLAY_LIST;
			goto cleanup;
		    }
		    if(vts > (n_chunk * chunk_interval * 1000)){
			end_of_chunk = 1;
			break;
		    }
		}
			
		meta_flag = 2;
		n_video_packets++;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_AUDIO:
		vts = nkn_vpe_flv_get_timestamp(ft);
		meta_flag = 0;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_META:
		meta_flag = 1;
		break;
	}
      
	//read data packet and the current tag size (inclusive of the FLV tag header '11 bytes'
	iof->nkn_read(p_flv_tag_data, 1,body_length,in_desc); 
	iof->nkn_read(&last_tag_size, 1,sizeof(last_tag_size),in_desc);
	
	if(last_tag_size==0)
	    printf("FLV Tag size Corrupted\n");

	if(n_video_packets == 1 && meta_flag == 2){
	    avcc_size = body_length+sizeof(nkn_vpe_flv_tag_t)+sizeof(uint32_t);	       	
	    p_avcc_data = avcc_data = (char*)malloc(avcc_size);
	    if(!p_avcc_data){
		error_no = NKN_VPE_E_MEM;
		goto cleanup;
	    }

	    //make a copy of AVCC Atom = SPS+PPS+{SEI optional}
	    memcpy(p_avcc_data, &ft, sizeof(ft));
	    p_avcc_data = p_avcc_data + sizeof(ft);
	    memcpy(p_avcc_data, p_flv_tag_data, body_length);
	    p_avcc_data = p_avcc_data + body_length;
	    memcpy(p_avcc_data, &last_tag_size, sizeof(uint32_t));
	    p_avcc_data = p_avcc_data + sizeof(uint32_t);

            avcc->p_avcc_data = avcc_data;
            avcc->avcc_data_size = avcc_size;

            *private_data = avcc;

		

	}
	//if not end of chunk continue parsing and copying into output stream
	if(!end_of_chunk){
#ifdef _DEBUG
	    printf("tag type - %d, @ time stamp = %d with pakcet no %d and size %d\n", meta_flag, vts, pkt_no, body_length);
	    pkt_no++;
#endif

	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);
	    //  iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
			
	}
	else if(end_of_chunk){
	    n_chunk++;
#ifdef _DEBUG
	    printf("----------------START OF NEW CHUNK---------------------------\n");
	    printf("tag type - %d, @ time stamp = %d with packet no %d and size %d\n", meta_flag, vts, pkt_no, body_length);
	    pkt_no++;
#endif
	    if(io_mode == 0){
		iof->nkn_write_stream_to_file(out_desc, out_fname);
	    }
	    iof->nkn_close(out_desc);
	    snprintf(out_fname,strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.flv", out_path, video_name, profile+1, n_chunk);  
	  
	    if(io_mode == 0){
		out_desc = iof->nkn_create_raw_descriptor(10*1024*1024);
	    }else{
		out_desc = iof->nkn_create_descriptor_from_file(out_fname,"wb");
	    }

	    if(out_desc == NULL){
		printf("unable to open output file\n");
		error_no = NKN_VPE_E_INVALID_HANDLE;
		return -1;
	    }
	    

	    //the first packet of the file is always AVCC (in case there is change in coding tools
	    //between different profiles
	    //iof->nkn_write(avcc_data, 1, avcc_size, out_desc);
	    iof->nkn_write(&ft, 1, sizeof(ft), out_desc);
	    iof->nkn_write(p_flv_tag_data, 1, body_length, out_desc);	
	    //iof->nkn_read(&last_tag_size, sizeof(last_tag_size), 1, in_desc);
	    iof->nkn_write(&last_tag_size, sizeof(last_tag_size), 1, out_desc);
	    end_of_chunk = 0;
			
	}
	free(p_flv_tag_data);
	p_flv_tag_data = NULL;
	pos = iof->nkn_tell(in_desc); 
    }

    if(io_mode == 0&&!end_of_chunk){
	iof->nkn_write_stream_to_file(out_desc, out_fname);
    }

    md->n_chunks = n_chunk;
    n_avcc_packets = 0;
    pos = 0;
    meta_flag = 0;
    end_of_chunk = 0;
    n_chunk = 1;
    n_video_packets = 0;
    last_tag_size = 0;
    avcc_size = 0;

    md->sequence_duration = vts;

    //close file descriptors for the last chunk
    iof->nkn_close(out_desc);

    out_desc = NULL;

    //set the bitrate of the media asset
    //videobitrate+audiobitrate+containeroverheads
    md->bitrate = ((input_size/vts)*8); //Kbits per second

    //write hint track
    snprintf(out_fname, strlen(out_path)+strlen(video_name)+22, "%s/%s_p%.2d.hint", out_path, video_name, profile);
    hint_desc = (void*)fopen(out_fname, "wb");

    if(hint_desc == NULL){
	printf("unable to open output file\n");
	error_no = NKN_VPE_E_INVALID_HANDLE;
	return -1;
    }

    if((error_no = write_hint_track(hint_desc, tplay_hint_list, n_iframes, vts))){
	printf("error generating hint track\n");
	goto cleanup;
    }
     
    fclose(hint_desc);
    free_tplay_list(tplay_hint_list);
    return error_no;

    //Chunker enters clean up state
 cleanup:
    if(p_flv_tag_data)
	free(p_flv_tag_data);
    if(out_desc)
	iof->nkn_close(out_desc);
    if(meta_desc)
	iof->nkn_close(meta_desc);
    //if(avcc_data)
    //free(avcc_data);
    
    free_tplay_list(tplay_hint_list);
    	
    return error_no;

}


box_list_reader moov_parser[]={
	{{'m','d','h','d'}, mdhd_reader},
	{{'t','k','h','d'}, tkhd_reader}
};
int32_t moov_parser_length = 2;

box_list_reader moof_parser[]={
	{{'t','f','h','d'}, tfhd_reader},
	{{'t','r','u','n'}, trun_reader}
};
int32_t moof_parser_length = 2;

int 
VPE_PP_mp4_frag_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data )
{
    int64_t pos, size;
    box_t box;
    int32_t error_no, box_size, n_chunk, done;
    uint8_t *p_data, *p_parent;
    char *video_name, out_fname[MAX_PATH];
    void *out_desc;
    const char moov_id[] = {'m','o','o','v'};
    const char moof_id[] = {'m','o','o','f'};
    int32_t timescale;
    uint32_t audio_track_id, video_track_id;
    uint8_t frag_type;
    uint32_t num_audio_frags, num_video_frags;

    pos = size = error_no = box_size = 0;
    n_chunk = 0;
    p_data = NULL;
    p_parent = NULL;
    video_name = NULL;
    done = 0;
    timescale = 0;
    audio_track_id = video_track_id = 0;
    frag_type = 0;
    num_audio_frags = num_video_frags = 0;
    private_data = private_data;

    size  = iof->nkn_get_size(in_de);
    pos = iof->nkn_tell(in_de);
    video_name = md->video_name;

    out_path = out_path;
    md = md;
    io_mode = io_mode;
    profile = profile;
    
    n_chunk = n_chunk+1;
    snprintf(out_fname,strlen(out_path)+strlen(video_name)+22, "%s/%s_p%02d_c%010d.mp4", out_path, video_name, profile+1, n_chunk);  
    out_desc = iof->nkn_create_descriptor_from_file(out_fname, "wb");
    while(pos<size){
	box_size = read_next_root_level_box(in_de, iof, &box);
	p_data = malloc(box_size);
	iof->nkn_read(p_data, 1, box_size-8, in_de);
 	p_parent = p_data;
	if(check_box_type(&box, moov_id)){
	    box_t child_box;
	    int32_t i, child_box_size, boxes_to_read, track_no;
	    mdhd_t *mdhd;
	    tkhd_t *tkhd[MAX_SUPPORTED_TRACKS];//support 2 tracks max now, needs to be changed

	    child_box_size = 0;
	    boxes_to_read = 3;
	    track_no = 0;

	    while(done!=boxes_to_read){
		child_box_size = read_next_child(p_data, &child_box);
		for(i=0;i < moov_parser_length;i++){
		    if(!memcmp(&moov_parser[i].type, &child_box.type, 4)){
			switch(i){
			    case 0:
				mdhd = (mdhd_t*)(moov_parser[i].box_reader(p_data, child_box_size, 8));
				md->sequence_duration = get_sequence_duration(mdhd);
				timescale = mdhd->timescale;
				done++;
				break;
			    case 1:
				tkhd[track_no] = (tkhd_t*)(moov_parser[i].box_reader(p_data, child_box_size, 8));
				if(tkhd[track_no]->width == 0){
				    audio_track_id = tkhd[track_no]->track_id;
				} else {
				    video_track_id = tkhd[track_no]->track_id;
				}
				done++;
				track_no = (track_no+1)%MAX_SUPPORTED_TRACKS;
				break;
			}
		    }
		}
		if(is_container_box(&child_box)){
		    p_data += 8;
		}else{
		    p_data += child_box_size;
		}
	    }
	    p_data = p_parent;
	}
	
	if(check_box_type(&box, moof_id)){
	    box_t child_box;
            int32_t i, child_box_size, frag_duration, frag_size;
	    uint32_t s;
            tfhd_t *tfhd;
	    trun_t *trun;
	    
	    done = 0;
            child_box_size = 0;
	    frag_duration = 0;
	    frag_size = 0;
	    s = 0;

            while(done!=moof_parser_length){
                child_box_size = read_next_child(p_data, &child_box);
                for(i=0;i < moof_parser_length;i++){
                    if(!memcmp(&moof_parser[i].type, &child_box.type, 4)){
			switch(i){
			    case 0:
				tfhd = (tfhd_t*)(moof_parser[i].box_reader(p_data, child_box_size, 8));
				if(tfhd->track_id == audio_track_id){
				    frag_type = 1;
				} else { 
				    frag_type = 2;
				}
				done++;
				break;
			    case 1:
				trun = (trun_t*)(moof_parser[i].box_reader(p_data, child_box_size, 8));
				if(frag_type ==2) {
				    //       md->smooth_flow_duration = trun->sample_count * trun->tr_stbl[0].sample_duration;
				    num_video_frags++;
				    for(s=0;s < trun->sample_count;s++){
					frag_duration += trun->tr_stbl[s].sample_duration;
					frag_size += trun->tr_stbl[s].sample_size;
				    }
				    md->smooth_flow_duration = frag_duration/timescale;
				} else {
				    num_audio_frags++;
				}
				printf("frag duration %f, frag_size %d\n", (frag_duration/(double)(timescale)), frag_duration);
				done++;
				break;
			}
		    }
                }
                if(is_container_box(&child_box)){
                    p_data += 8;
                }else{
                    p_data += child_box_size;
                }
            }
            p_data = p_parent;
        }

	
	if(box_type_moof(&box)){
	    iof->nkn_close(out_desc);
	    n_chunk = n_chunk+1;
	    if(frag_type == 1){
		snprintf(out_fname,strlen(out_path)+strlen(video_name)+23, "%s/%s_p%02d_ac%010d.mp4", out_path, video_name, profile+1, num_audio_frags);  
	    } else if(frag_type == 2){
		snprintf(out_fname,strlen(out_path)+strlen(video_name)+23, "%s/%s_p%02d_vc%010d.mp4", out_path, video_name, profile+1, num_video_frags)
;  
	    } else {
	    snprintf(out_fname,strlen(out_path)+strlen(video_name)+23, "%s/%s_p%02d_mc%010d.mp4", out_path, video_name, profile+1, n_chunk);  	    
	    }
	    out_desc = iof->nkn_create_descriptor_from_file(out_fname, "wb");
	}
	   
	write_box(out_desc, iof, &box, p_data);
	free(p_data);

	pos = iof->nkn_tell(in_de);
    }

    return error_no;

}

#define TS_PKT_SIZE 188
#define TS_CLK_FREQ 90
int 
VPE_PP_MP2TS_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data )
{
    size_t pos, size;
    uint8_t *ts_pkt;
    Misc_data_t* extra_data;
    tplay_index_record *tplay_record;
    Ts_t *ts;
    int32_t ret, chunk_no, PTS, startPTS, lastPTS;
    GList *pkt_list;
    char output_file_name[MAX_PATH];
    void *out_de;
    uint32_t chunk_interval;

    ts = (Ts_t*)malloc(sizeof(Ts_t));
    io_mode = io_mode;
    size  = iof->nkn_get_size(in_de);
    pos = iof->nkn_tell(in_de);
    ret = 0;
    pkt_list = NULL;
    chunk_no = 1;
    out_de = NULL;
    PTS = 0;
    startPTS = 0;
    chunk_interval = md->smooth_flow_duration;
    UNUSED_ARGUMENT(profile);
    UNUSED_ARGUMENT(private_data);
    tplay_record = (tplay_index_record *)malloc(sizeof(tplay_index_record));
    extra_data=(Misc_data_t*)malloc(sizeof(Misc_data_t));
    if(Ts_alloc(ts)==VPE_ERR)
	return VPE_ERR;
    ts->vsinfo->num_pids=0;
    ts->asinfo->num_pids=0;
    ts->num_pids=0;
    ts->num_pkts=0;
    ts->pas->prev_cc=0x0F;

    snprintf(output_file_name, strlen(out_path) + strlen(md->video_name) + 14, "%s/%s_p%02d_%04d.ts", out_path, md->video_name, profile + 1, chunk_no);
    out_de = iof->nkn_create_descriptor_from_file(output_file_name, "wb");

    chunk_interval = chunk_interval * 1000;//convert to milliseconds
    while(pos < size) {
	ts_pkt = (uint8_t*)malloc(TS_PKT_SIZE);
	iof->nkn_read(ts_pkt, 1, TS_PKT_SIZE, in_de);
	ret = handle_pkt_tplay(ts_pkt, ts, tplay_record, extra_data);
	if(extra_data->frame_type_pending) {
	    pkt_list = g_list_prepend(pkt_list, ts_pkt);
	    continue;
	} else {
	    if(tplay_record->type == TPLAY_RECORD_PIC_IFRAME) { 
		PTS = (tplay_record->PTS / TS_CLK_FREQ) - startPTS;
		startPTS = (startPTS == 0) * PTS;
		if(PTS >= (int32_t)(chunk_no * chunk_interval)) {
		    lastPTS = PTS;
		    iof->nkn_close(out_de); //close current chunk
		    chunk_no++; //increment chunk number
		    snprintf(output_file_name, strlen(out_path) + strlen(md->video_name) + 14, "%s/%s_p%02d_%04d.ts", out_path, md->video_name, profile + 1, chunk_no);
		    out_de = iof->nkn_create_descriptor_from_file(output_file_name, "wb");//op
		    if(pkt_list) {
			write_ts_pkt_list(pkt_list, out_de, iof);
			g_list_free1(pkt_list);
			pkt_list = NULL;
		    } else {
			iof->nkn_write(ts_pkt, 1, TS_PKT_SIZE, out_de);
			free(ts_pkt);
		    }
		} else {
		    iof->nkn_write(ts_pkt, 1, TS_PKT_SIZE, out_de);
		    free(ts_pkt);
		}
 
	    }else {
		iof->nkn_write(ts_pkt, 1, TS_PKT_SIZE, out_de);
		free(ts_pkt);
	    }
	    
	}
	pos = iof->nkn_tell(in_de);
    }

    md->n_chunks = chunk_no;
    md->bitrate = size/(lastPTS/1000);

    return 0;
}

int32_t 
write_ts_pkt_list(GList *list, void *desc, io_funcs *iof)
{
    GList *ptr;

    ptr = list;
    ptr = g_list_last(ptr);

    while(ptr) {
	iof->nkn_write(ptr->data, 1, TS_PKT_SIZE, desc);
	free(ptr->data);
	ptr = g_list_previous(ptr);
    }
    
    return 0;
}

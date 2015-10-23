#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include "nkn_vpe_types.h"
#include "nkn_debug.h"
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_utils.h"
//#include "nkn_vpe_mp4_feature.h"
//#include "nkn_vfs.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_raw_h264.h"
//#include "nkn_vpe_bitio.h"
#include <byteswap.h>
#include "nkn_memalloc.h"



#define MOOV_HEX_ID 0x6d6f6f76
#define VIDEO_TRAK 0

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

//static function declarations
static inline uint32_t _read32(uint8_t*,int32_t);
static inline uint16_t _read16(uint8_t*,int32_t);
//static inline int32_t mfu_check_h264_profile_level(stream_AVC_conf_Record_t *);

//static inline int32_t mp4_read_avc1(stream_AVC_conf_Record_t *,uint8_t *);
static inline int32_t mp4_ret_track_id(uint8_t *);
static void mfu_free_stream_param_set(stream_param_sets_t *sps);

#define nkn_vfs_fwrite(buf, n1, n2, fd) fwrite(buf, n1, n2, fd)
#define nkn_vfs_fread(buf, n1, n2, fd) fread(buf, n1, n2, fd)
static uint32_t uint32_byte_swap(uint32_t input){
    uint32_t ret=0;
    ret = (((input&0x000000FF)<<24) + ((input&0x0000FF00)<<8) +
           ((input&0x00FF0000)>>8) + ((input&0xFF000000)>>24));
    return ret;
}

static inline uint32_t _read32(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
}

static inline uint16_t _read16(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<8)|(buf[pos+1]);
}


int32_t mfu_init_ind_video_stream(video_pt *present_stream){

    present_stream->dump_pps_sps = 1;
    present_stream->avc1 =
	(stream_AVC_conf_Record_t*)nkn_calloc_type(1,
						   sizeof(stream_AVC_conf_Record_t),
						   mod_vpe_stream_AVC_conf_Record_t);
    //present_stream->avc1->sps = (stream_param_sets_t *)calloc(1,sizeof(uint8_t));
    //present_stream->avc1->pps = (stream_param_sets_t *)calloc(1,sizeof(stream_param_sets_t));

    return VPE_SUCCESS;
}

static void mfu_free_stream_param_set(stream_param_sets_t *sps) 
{
    if (sps) {
	if (sps->param_set) {
	    free (sps->param_set);
	}
	free(sps);
	sps = NULL;
    }
}

int32_t mfu_free_ind_video_stream(video_pt *present_stream)
{
    stream_param_sets_t *sps;
    stream_AVC_conf_Record_t *avcc;
    
    if (present_stream) {
	avcc = present_stream->avc1;
	if (avcc) {
	  //sps = avcc->pps;
	    if (avcc->pps != NULL) {
		mfu_free_stream_param_set(avcc->pps);
	    }
	    //sps = avcc->sps;
	    if(avcc->sps != NULL) {
		mfu_free_stream_param_set(avcc->sps);
	    }
	    free(avcc);
	    avcc = NULL;
	}
    }

    return VPE_SUCCESS;
}

int32_t mfu_allocate_video_streams(video_pt *present_stream,int32_t vid_trak_num){

    char fname[30];
    sprintf(fname,"%s%d",VPE_OUTPUT_PATH,vid_trak_num);
    strcat(fname,".h264");
    
    present_stream->fout = fopen(fname,"wb");
    //present_stream->fname = fname;
    //strcpy(rtp->vfname[i],fname);
    return VPE_SUCCESS;
}


int32_t mfu_get_pps_sps(void *io_desc, io_handlers_t *iocb, video_pt *present_stream){

    uint8_t *avcC_data;
    int32_t ret= VPE_SUCCESS;
    int32_t bytes_read = 0;
    avcC_data = (uint8_t*) nkn_malloc_type(present_stream->avc1_size,
					   mod_vpe_avcC_data);
    //fseek(fid,present_stream->avcC_offset,SEEK_SET);
    //(*nkn_fread)(avcC_data,present_stream->avcC_size,sizeof(uint8_t),fid);
    iocb->ioh_seek(io_desc, present_stream->avcC_offset, SEEK_SET);
    bytes_read = iocb->ioh_read(avcC_data, 1, present_stream->avcC_size, io_desc);
    if(bytes_read != present_stream->avcC_size) {
      ret = -E_VPE_READ_INCOMPLETE;
      goto error;
    }
    mp4_read_avc1(present_stream->avc1,avcC_data);
    ret = mfu_check_h264_profile_level(present_stream->avc1);
 error:
    if(avcC_data != NULL)
	free(avcC_data);
    return ret;
}

int32_t mfu_check_h264_profile_level(stream_AVC_conf_Record_t *avc){
    uint8_t *data;
    uint8_t profile_idc=0,level_idc=0;
    int32_t err = 0;
    data = avc->sps->param_set;
    data++;
    profile_idc = *data++;
    data++;
    level_idc = *data;
#if 0    
    if((profile_idc != 66)||(level_idc > 30))
	{
	    DBG_MFPLOG("H264Tool", ERROR, MOD_MFPFILE,"Only Baseline profile and level 3.0 is are supported\n");
	    DBG_MFPLOG("H264Tool", ERROR, MOD_MFPFILE,"This file shall be skipped\n");
	    err = -E_VPE_PROFILE_LEVEL_ERROR;
	    return err;
	}
#endif    
    return VPE_SUCCESS;
}

int32_t mp4_read_avc1(stream_AVC_conf_Record_t *avc,uint8_t *data)
{

    int32_t box_size,bytes_left,i;
    box_size = _read32(data,0);
    bytes_left = box_size;
    bytes_left-=8;
    data+=8;

    //avc->version = *data++;
    data++;
    avc->AVCProfileIndication = *data++;
    avc->profile_compatibility = *data++;
    avc->AVCLevelIndication = *data++;
    avc->NALUnitLength = (*data&0x03)+1;
    data++;
    avc->numOfSequenceParameterSets = *data&0x1f;
    data++;
    avc->sps = (stream_param_sets_t*)
	nkn_malloc_type(sizeof(stream_param_sets_t)*avc->numOfSequenceParameterSets, 
			mod_vpe_stream_param_sets_t);
    for(i=0;i< avc->numOfSequenceParameterSets;i++){
        avc->sps->length = _read16(data,0);
	avc->sps->param_set = (uint8_t*)
	    nkn_malloc_type(avc->sps->length,
			    mod_vpe_sps_pps_param_set);
        data+=2;
        memcpy(avc->sps->param_set,data,avc->sps->length);
        data+=avc->sps->length;
    }
    avc->numOfPictureParameterSets = *data&0x1f;
    data++;
    avc->pps = (stream_param_sets_t*)
	nkn_malloc_type(sizeof(stream_param_sets_t)*avc->numOfPictureParameterSets, 
			mod_vpe_stream_param_sets_t);
    for(i=0;i<avc->numOfPictureParameterSets;i++){
        avc->pps->length = _read16(data,0);
	avc->pps->param_set = (uint8_t*)
	    nkn_malloc_type(avc->pps->length,
			    mod_vpe_sps_pps_param_set);
        data+=2;
        memcpy(avc->pps->param_set,data,avc->pps->length);
        data+=avc->pps->length;
    }
    return VPE_SUCCESS;
}



int32_t mp4_get_stsd_info(uint8_t *stbl_data,video_pt *present_stream){
    int32_t box_size,size,stbl_box_size;
    size_t bytes_read;
    box_t box;
    uint8_t* p_data;

    char stsd_id[] = {'s', 't', 's', 'd'};
    size = 10000;
    stbl_box_size = read_next_root_level_box(stbl_data, size, &box, &bytes_read);
    p_data=stbl_data;
    stbl_data+=bytes_read;
    while(stbl_data-p_data<stbl_box_size){
        box_size = read_next_root_level_box(stbl_data, size, &box, &bytes_read);
        if (box_size < 0)
            {
                return VPE_ERROR;
            }
        if(check_box_type(&box,stsd_id)){
            present_stream->stsd_offset= present_stream->stbl_offset+(int32_t)(stbl_data-p_data);
            present_stream->stsd_size = box_size;
            stbl_data+=box_size;
        }
        else
            stbl_data+=box_size;
    }

    return VPE_SUCCESS;
}


int32_t  mp4_get_avcC_info(void *io_desc, io_handlers_t *iocb, video_pt *present_stream){

    box_t box;
    size_t bytes_read, total_bytes_read = 0;
    int32_t box_size,size = 10000;//initial_data;
    char avc1_id[] = {'a', 'v', 'c', '1'};
    char avcC_id[] = {'a', 'v', 'c', 'C'};
    //int32_t avc1_size,avcC_size;
    uint8_t *data,*initial_data;
    int32_t read_size = 0, ret = VPE_SUCCESS;
    data = (uint8_t*) nkn_malloc_type(present_stream->stsd_size,
				      mod_vpe_stsd_buf);

    initial_data =data;
    //fseek(fid,present_stream->stsd_offset,SEEK_SET);
    //(*nkn_fread)(data,present_stream->stsd_size,1,fid);
    iocb->ioh_seek(io_desc, present_stream->stsd_offset, SEEK_SET);
    read_size = iocb->ioh_read(data, 1, present_stream->stsd_size, io_desc);
    if(read_size != present_stream->stsd_size) {
      ret = -E_VPE_READ_INCOMPLETE;
      goto error;
    }
    data+=16;
    while(total_bytes_read < (size_t)present_stream->stsd_size)
        {
            box_size = read_next_root_level_box(data, size, &box, &bytes_read);
            total_bytes_read += box_size;
            if(check_box_type(&box,avc1_id))
                {
                    present_stream->avc1_offset = present_stream->stsd_offset +(data - initial_data);
                    present_stream->avc1_size = box_size;
		    present_stream->codec_type = H264_VIDEO;
                    data+=86;
                    box_size = read_next_root_level_box(data, size, &box, &bytes_read);
                    //total_bytes_read += box_size;
                    if(check_box_type(&box,avcC_id))
                        {
                            present_stream->avcC_offset = present_stream->stsd_offset + (data - initial_data);
                            present_stream->avcC_size = box_size;
                            data+=box_size;
			    if(initial_data != NULL)
				free(initial_data);
                            return VPE_SUCCESS;
                        }

                }
        }

    present_stream->avcC_offset  = -1;
    present_stream->avcC_size = -1;
 error:
    if(initial_data != NULL)
	free(initial_data);
    return ret;
}


static inline int32_t mp4_ret_track_id(uint8_t *p_dat){
    uint8_t version;
    uint32_t pos=0,track_id;
    version=p_dat[pos];
    pos+=4;
    if(version)
        pos+=16;
    else
        pos+=8;
    track_id=(p_dat[pos]<<24)|(p_dat[pos+1]<<16)|(p_dat[pos+2]<<8)|p_dat[pos+3];
    return track_id;
}


int32_t  mfu_write_sps_pps(stream_AVC_conf_Record_t
			   *avc1, moof2mfu_desc_t* moof2mfu_desc){

    int32_t word,i, total_size=0;
    stream_param_sets_t  *temp_sps_set,*temp_pps_set;
    uint8_t* org_sps_pps;
    mfu_rw_desc_t *videofg = &moof2mfu_desc->mfu_raw_box->videofg;

    word = NALU_DELIMIT;
    word = htonl(word);

    /* get the sps and pps sixe */
    temp_sps_set = avc1->sps;
    for(i=0;i<avc1->numOfSequenceParameterSets;i++)
	total_size += sizeof(int32_t)+temp_sps_set->length;
    temp_pps_set = avc1->pps;
    for(i=0;i<avc1->numOfPictureParameterSets;i++)
	total_size += sizeof(int32_t)+temp_pps_set->length;
    videofg->codec_info_size = total_size;
    videofg->codec_specific_data = (uint8_t*)
	nkn_malloc_type(total_size,
			mod_vpe_videofg_codec_specific_data);
    org_sps_pps =  videofg->codec_specific_data;
    
    /* get the sps data */
    temp_sps_set = avc1->sps;
    for (i=0;i<avc1->numOfSequenceParameterSets;i++) {
	memcpy(videofg->codec_specific_data, &word,
	       sizeof(int32_t));
	videofg->codec_specific_data+=sizeof(int32_t);
	memcpy(videofg->codec_specific_data,
	       temp_sps_set->param_set, temp_sps_set->length);
	videofg->codec_specific_data +=
	    temp_sps_set->length;
        temp_sps_set++;
    }
    /* get the pps data */
    temp_pps_set = avc1->pps;
    for (i=0;i<avc1->numOfPictureParameterSets;i++) {
	memcpy(videofg->codec_specific_data, &word,
	       sizeof(int32_t));
	videofg->codec_specific_data += sizeof(int32_t);
	memcpy(videofg->codec_specific_data,
	       temp_pps_set->param_set, temp_pps_set->length);
	videofg->codec_specific_data +=
	    temp_pps_set->length;
        temp_pps_set++;
    }
    videofg->codec_specific_data = org_sps_pps;
    return VPE_SUCCESS;
}

#define MAX_NALS 500 //Max number of NAls per AU
int32_t mfu_get_h264_samples(mdat_desc_t *mdat_desc, moof2mfu_desc_t* moof2mfu_desc)//FILE *fout)
{
    int32_t word,NAL_size=0,total_sample_size=0,new_sample_size = 0;
    uint8_t *data,**fdata,*tdat;
    uint32_t NAL_offset=4,data_offset=0, num_NALs=0,nal_sizes[MAX_NALS],i,j;
    mdat_desc_t *md;
    mfu_rw_desc_t *videofg = &moof2mfu_desc->mfu_raw_box->videofg;
    uint32_t pos =0;
    md = mdat_desc;
    fdata = (uint8_t**) nkn_malloc_type(MAX_NALS, mod_vpe_tot_NAL_buf);
    word = NALU_DELIMIT;
    word = htonl(word);
    data = mdat_desc->mdat_pos+8;

    if(mdat_desc->default_sample_size != 0){
	for(i=0;i<mdat_desc->sample_count;i++){
	    total_sample_size = mdat_desc->default_sample_size;
	    data_offset=0;
	    NAL_offset=4;
	    while(total_sample_size){
		//reading the individual NAL size from sample
		NAL_size=
		    bswap_32(*((int32_t*)(data+data_offset)));

		uint8_t* dt;
		dt = fdata[num_NALs];
		dt = (uint8_t*) nkn_malloc_type(4+NAL_size,
						mod_vpe_NAL_buf);
		memcpy(dt,&word,sizeof(int32_t));
		memcpy(dt+sizeof(int32_t),data+NAL_offset,NAL_size);
		nal_sizes[num_NALs] =  sizeof(int32_t)+NAL_size;
		new_sample_size += sizeof(int32_t)+NAL_size;
		num_NALs++;
		NAL_offset=NAL_offset+4+NAL_size;
		data_offset= data_offset+4+NAL_size;
		total_sample_size= total_sample_size-4-NAL_size;
	    }
#ifdef _BIG_ENDIAN_
	    videofg->sample_sizes[i] = uint32_byte_swap(new_sample_size);
#else
	    videofg->sample_sizes[i] = new_sample_size;
#endif
	    tdat = moof2mfu_desc->vdat;
	    for(j=0;j<num_NALs;j++){
		tdat = moof2mfu_desc->vdat;
		memcpy(tdat,fdata[j],nal_sizes[j]);
		new_sample_size = 0;
		tdat+=nal_sizes[j];
		moof2mfu_desc->vdat_size +=nal_sizes[j];
		free(fdata[j]);
	    }
	    num_NALs = 0;
	    data=data+mdat_desc->default_sample_size;
	}
    }
    else{//sample sizes are different
	for(i=0;i<mdat_desc->sample_count;i++){
		total_sample_size = mdat_desc->sample_sizes[i];
		data_offset=0;
		NAL_offset=4;
		while(total_sample_size){
		    NAL_size= bswap_32(*((int32_t *)(data+data_offset)));//reading the individual NAL size from sample
		    uint8_t* dt;
		    //			dt = fdata[num_NALs];
		    fdata[num_NALs] = (uint8_t*) nkn_malloc_type(4+NAL_size,
								 mod_vpe_NAL_buf);
		    dt = fdata[num_NALs];
		    memcpy(dt,&word,sizeof(int32_t));
		    memcpy(dt+sizeof(int32_t),data+NAL_offset,NAL_size);
		    nal_sizes[num_NALs] =  sizeof(int32_t)+NAL_size;
		    new_sample_size += sizeof(int32_t)+NAL_size;
		    num_NALs++;
		    NAL_offset=NAL_offset+4+NAL_size;
		    data_offset= data_offset+4+NAL_size;
		    total_sample_size= total_sample_size-4-NAL_size;
		}
#ifdef _BIG_ENDIAN_
		videofg->sample_sizes[i] =
		    uint32_byte_swap(new_sample_size);
#else
		videofg->sample_sizes[i] = new_sample_size;
#endif
		moof2mfu_desc->vdat_size += new_sample_size;
		//video->out->dat_pos[i] = (uint8_t*)malloc(sizeof(uint8_t*)*new_sample_size);
		tdat = moof2mfu_desc->vdat + pos;
		for(j=0;j<num_NALs;j++){		    
		    memcpy(moof2mfu_desc->vdat+pos,fdata[j],nal_sizes[j]);
		    pos += nal_sizes[j];
		    //moof2mfu_desc->vdat_size +=nal_sizes[j];

		    new_sample_size = 0;
		    free(fdata[j]);
		}
		num_NALs = 0;
		
		data=data+mdat_desc->sample_sizes[i];
	}
    }
    free(fdata);
    
    
    return VPE_SUCCESS;
}

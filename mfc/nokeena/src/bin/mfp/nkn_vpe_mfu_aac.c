#include "nkn_vpe_mfu_aac.h"
#include <string.h>
#include <byteswap.h>
#include "nkn_vpe_mfu_writer.h"
#include "nkn_memalloc.h"
#define ESDS_OFFSET 52

//static function declarations
//static inline int32_t mp4_handle_esds(uint8_t*, adts_t*);
static inline uint8_t mp4_read_object_type_indication(uint8_t);




int32_t mfu_get_aac_headers(stbl_t* stbl, adts_t *adts){
    uint8_t *esds;
    int32_t ret = VPE_SUCCESS;
    /*Get the ESDS box*/
    ret = mp4_get_esds_box(stbl->stsd_data,&esds);
    if(ret <= 0) {
      return ret;
    }

    /*Handle esds to get adts data*/
    ret = mp4_handle_esds(esds,adts);
    if(ret <= 0) {
      return ret;
    }
    return VPE_SUCCESS;

}



int32_t mp4_get_esds_box(uint8_t* stsd, uint8_t **esds){

    box_t box;
    size_t box_size;
    const char esds_id[]={'e','s','d','s'};
    *esds = stsd + ESDS_OFFSET;
    //    read_next_root_level_box(*esds+1, 8, &box, &box_size);
    read_next_root_level_box(*esds, 8, &box, &box_size);
    /*Get the esds box within */
    if(check_box_type(&box, esds_id))
	return VPE_SUCCESS;
    else
	return -E_VPE_INVALID_FILE_FROMAT;
}



int32_t mp4_handle_esds(uint8_t *esds, adts_t *adts){

    bitstream_state_t *bs;
    int32_t esds_size;
    uint8_t * esds_data;
    mpeg4_es_desc_t *esd;
    size_t esd_size;

    /*the ESDS parsing code is legacy from rtsp unhinted. The size manipulation needs a revisit - NOT clear.*/

    /*Get basic ESDS info*/
    //    esds_size = bswap_32(*((int32_t *)(esds+1)))+4;
    esds_size = bswap_32(*((int32_t *)(esds)))+4;
    esds_data = esds;//+4; //Skip the 4 bytes for size

    /*Initialize ES desc*/
    esd = mpeg4_init_es_descriptor();

    /*Glean esd*/
    bs = bio_init_bitstream(esds_data + 12, esds_size - 12);
    mpeg4_es_read_object_descriptor(bs, esds_size, esd, &esd_size);
    bio_close_bitstream(bs);

    /*Get the decoder specific information*/ 
    if(esd->dc->specific_info) {
        bs = bio_init_bitstream(esd->dc->specific_info, esd->dc->specific_info_size);
        m4a_read_decoder_specific_info(bs, esd->dsi);
        bio_close_bitstream(bs);
    }
    else
	return -E_VPE_MP4_ESDS_PARSE_ERR;

    /*Map decoder sp info values to adts*/
    switch(mp4_read_object_type_indication(esd->dc->profile_level_indicator)){
	case MP2_AAC:
	    adts->is_mpeg2 = 1;
	    break;
	case MP4_AAC:
	    adts->is_mpeg2 =0;
	    break;
	default:
	    return VPE_ERROR;
	    break;
    }
    adts->is_mpeg2 =1;
    adts->object_type = esd->dsi->base_object_type-1;
    adts->sr_index = esd->dsi->sampling_freq_idx;
    adts->num_channels = esd->dsi->channel_config;

    mpeg4_cleanup_es_descriptor(esd);
    return VPE_SUCCESS;
}

static inline uint8_t mp4_read_object_type_indication(uint8_t pli){

    if(pli>=0x60 && pli <=0x69)
	return MP2_AAC;
    if(pli == 0x40)
	return MP4_AAC;
    return VPE_ERROR;
}



int32_t mfu_form_aac_header(adts_t* adts,uint8_t** hdr,int32_t* hdr_len){
    bitstream_state_t *bs;    
    uint8_t *buf;
    *hdr_len = FR_LEN_NO_PROTECTION; 
    *hdr = (uint8_t*) nkn_calloc_type(1, *hdr_len, mod_vpe_aac_header_buf);
    buf = *hdr;//(uint8_t*)calloc(sizeof(uint8_t)**hdr_len,1);
    buf[0] =  0xFF;
    buf[1] = 0xF0|(adts->is_mpeg2<<3)|0x01;
    bs = bio_init_bitstream(&buf[2],5);
    bio_write_int(bs,adts->object_type,2);
    bio_write_int(bs,adts->sr_index,4);
    bio_write_int(bs,0,1);
    bio_write_int(bs,adts->num_channels,3);
    bio_write_int(bs,0,4);
    bio_write_int(bs,adts->sample_len+7,13);
    bio_write_int(bs,0x7FF,11);
    bio_write_int(bs,0,2);
    bio_close_bitstream(bs);
    return VPE_SUCCESS;
}


int32_t  mfu_dump_aac_unit(uint8_t *frame, int32_t frame_len,audio_pt*
    audio,  moof2mfu_desc_t* moof2mfu_desc){
    uint8_t *buf;
    int32_t pos,flen;
    uint8_t* hdr;
    int32_t hdr_len;
    FILE* fout;
    if(audio->fout != NULL)
	fout = audio->fout;

    hdr = audio->header;
    hdr_len =audio->header_size;
    buf = hdr;
    flen=frame_len+hdr_len;
    pos=hdr_len-1-3;
    buf[pos]&=0xFC;
    buf[pos++]|=flen>>11;
    buf[pos++]=(flen>>3)&0xFF;
    buf[pos]&=0x1F;
    buf[pos]|=(flen&0x7)<<5;
    memcpy(moof2mfu_desc->adat,hdr,hdr_len);
    memcpy(moof2mfu_desc->adat+hdr_len,frame,frame_len);
    moof2mfu_desc->adat+=flen;
    //moof2mfu_desc->adat_size += flen;

    return VPE_SUCCESS;
}

#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_rtp_packetizer.h"
#include "rtp_mpeg4_es.h"
#include "nkn_vpe_bufio.h"
#include "nkn_vpe_bitio.h"
#include "rtp_packetizer_common.h"
#include "nkn_memalloc.h"


mpeg4_es_desc_t*
mpeg4_init_es_descriptor( void )
{

    mpeg4_es_desc_t *esd;
    esd = (mpeg4_es_desc_t*)nkn_calloc_type(1,
					    sizeof(mpeg4_es_desc_t), 
					    mod_vpe_mpeg4_es_desc_t);
    esd->dc = mpeg4_init_decoder_config();
    esd->slc = mpeg4_init_sync_layer_config();
    esd->dsi = m4a_init_decoder_specific_info();
    esd->auh = mpeg4_init_au_header();
    return esd;
}

void
mpeg4_cleanup_es_descriptor(mpeg4_es_desc_t *esd)
{
    mpeg4_cleanup_decoder_config(esd->dc);
    mpeg4_cleanup_sync_layer_config(esd->slc);
    m4a_cleanup_decoder_specific_info(esd->dsi);
    mpeg4_cleanup_au_header(esd->auh);

    SAFE_FREE(esd);
}

mpeg4_decoder_config_t *
mpeg4_init_decoder_config( void )
{
    /* need to free the decoder specific data when cleaning up */
    return (mpeg4_decoder_config_t *)nkn_calloc_type(1, 
						     sizeof(mpeg4_decoder_config_t),
						     mod_vpe_mpeg4_decoder_config_t);
}

void
mpeg4_cleanup_decoder_config(mpeg4_decoder_config_t *dc)
{
    SAFE_FREE(dc->specific_info);
    SAFE_FREE(dc);
}

mpeg4_au_header_t*
mpeg4_init_au_header( void ) 
{
    return (mpeg4_au_header_t*)nkn_calloc_type(1,
					       sizeof(mpeg4_au_header_t),
					       mod_vpe_mpeg4_auh_t);
}

void
mpeg4_cleanup_au_header(mpeg4_au_header_t *auh) 
{
    SAFE_FREE(auh);
}

mpeg4_sync_layer_config_t *
mpeg4_init_sync_layer_config( void )
{
	return (mpeg4_sync_layer_config_t*)nkn_calloc_type(1, 
							   sizeof(mpeg4_sync_layer_config_t),
							   mod_vpe_mpeg4_slc_t);
}

void
mpeg4_cleanup_sync_layer_config(mpeg4_sync_layer_config_t *slc)
{
    SAFE_FREE(slc);
}

int32_t
mpeg4_es_read_object_descriptor(bitstream_state_t *bs, size_t size, void *parent, size_t *ts)
{
    uint8_t tag;
    size_t tag_size;
    size_t fixed_sizeofinstance;
    uint32_t val;

    fixed_sizeofinstance = 1;
    tag = bio_read_bits(bs, 8);
    tag_size = 0;

    do {
	val = bio_read_bits(bs, 8);
	fixed_sizeofinstance++;
	tag_size = tag_size << 7;
	tag_size |= val & 0x7f;
    }while(val & 0x80);

    switch(tag) {
	case ODF_ESD_TAG:
	    mpeg4_es_read_esd((mpeg4_es_desc_t*)parent, bs, tag_size);
	    break;
	case ODF_DCC_TAG:
	    mpeg4_es_read_decoder_config((mpeg4_es_desc_t*)parent, bs, tag_size);
	    break;
	case ODF_SLC_TAG:
	    mpeg4_es_read_sync_layer_config((mpeg4_es_desc_t*)parent, bs, tag_size);
	    break;
	case ODF_DECODER_SPECIFIC_INFO_TAG:
	    mpeg4_es_read_decoder_specific_info((mpeg4_es_desc_t*)parent, bs, tag_size);
	    break;
    }

    tag_size += fixed_sizeofinstance - get_object_descriptor_size(tag_size);
    *ts = tag_size;
    return 0;
}

int32_t
select_from_sync_layer_presets(uint8_t id, mpeg4_sync_layer_config_t *auc)
{
    
    switch(id) {
	case 0x02: //MP4 preset
	    auc->use_au_rap_flag = 1;
	    auc->use_ts_flag = 1;
	    break;
	case 0x01: //do nothing all init'ed to zero
	default: //init'ed to zero
	    break;
    }
    
    return 0;
}

int32_t
mpeg4_es_read_decoder_specific_info(mpeg4_es_desc_t *esd, bitstream_state_t *bs, size_t size)
{
    esd->dc->specific_info = (uint8_t*)nkn_calloc_type(1, size, mod_vpe_mpeg4_generic);
    esd->dc->specific_info_size = size;

    bio_aligned_read(bs, esd->dc->specific_info, size);

    return 0;
}

int32_t
mpeg4_es_read_sync_layer_config(mpeg4_es_desc_t *esd, bitstream_state_t *bs, size_t size)
{
    uint8_t au_config_preset_id;
    mpeg4_sync_layer_config_t *slc;
    int32_t skip;
    size_t bytes_read;

    bytes_read = 0;
    //    esd->slc = slc = (mpeg4_sync_layer_config_t*)nkn_calloc_type(1, sizeof(mpeg4_sync_layer_config_t));
    slc = esd->slc;
    skip = 0;
    au_config_preset_id = 0;

    au_config_preset_id = bio_read_bits(bs, 8);
    bytes_read += 1;

    if(au_config_preset_id) {
	//load from presets
	select_from_sync_layer_presets(au_config_preset_id, slc);
    } else {
	//read custom au config values
	slc->use_au_start_flag = bio_read_bits(bs, 1);
	slc->use_au_end_flag = bio_read_bits(bs, 1);
	slc->use_au_rap_flag = bio_read_bits(bs, 1);
	slc->rap_only_flag = bio_read_bits(bs, 1);
	slc->use_padding_flag = bio_read_bits(bs, 1);
	slc->use_ts_flag = bio_read_bits(bs, 1);
	slc->use_idle_flag = bio_read_bits(bs, 1);
	slc->use_duration_flag = bio_read_bits(bs, 1);
	slc->ts_resolution = bio_read_bits(bs, 32);
	slc->ocr_resolution = bio_read_bits(bs, 32);
	slc->ts_length = bio_read_bits(bs, 8);
	slc->ocr_length = bio_read_bits(bs, 8);
	slc->au_length = bio_read_bits(bs, 8);
	slc->instant_bitrate_length = bio_read_bits(bs, 8);
	slc->degradation_priority_length = bio_read_bits(bs, 4);
	slc->sequence_num_length = bio_read_bits(bs, 5);
	slc->packet_sequence_num_length = bio_read_bits(bs, 5);
	skip = bio_read_bits(bs, 2);
	bytes_read += 15;
    }

    if(slc->use_duration_flag) {
	slc->time_scale = bio_read_bits(bs, 32);
	slc->au_rate = bio_read_bits(bs, 16);
	slc->cu_rate = bio_read_bits(bs, 16);
	bytes_read += 8;
    }

    if(!slc->use_ts_flag) {
	slc->start_dts = bio_read_bits_to_uint64(bs, slc->ts_length);
	slc->start_cts = bio_read_bits_to_uint64(bs, slc->ts_length);
	bytes_read = (((slc->ts_length * 2) % 8 == 0) * ((slc->ts_length * 2) / 8)) +
	    (((slc->ts_length * 2) % 8 != 0) * (((slc->ts_length * 2) / 8) + 1)) ;
    }

    return 0;
}

int32_t
mpeg4_es_read_esd(mpeg4_es_desc_t *esd, bitstream_state_t *bs, size_t size)
{
    size_t bytes_read, tag_size;
    uint8_t depends_on_flag, uri_flag, ocr_flag;

    ocr_flag = uri_flag = depends_on_flag = 0;
    tag_size = 0;
    bytes_read = 0;

    esd->esid = bio_read_bits(bs, 16);
    depends_on_flag = bio_read_bits(bs, 1);
    uri_flag = bio_read_bits(bs, 1);
    ocr_flag = bio_read_bits(bs, 1);
    esd->stream_priority = bio_read_bits(bs, 5);
    bytes_read += 3;

    if(depends_on_flag) {
	esd->dependsOn_esid = bio_read_bits(bs, 16);
	bytes_read += 2;
    }

    if(uri_flag) {
	//not handling this case
	return -1;
    }

    if(ocr_flag) {
	esd->ocresid = bio_read_bits(bs, 16);
	bytes_read += 2;
    }

    while(bytes_read < size) {
        mpeg4_es_read_object_descriptor(bs, size - bytes_read, esd, &tag_size);
        bytes_read += tag_size + get_object_descriptor_size(tag_size);
    }

    return 0;    
}

int32_t
mpeg4_es_read_decoder_config(mpeg4_es_desc_t *esd, bitstream_state_t *bs, size_t size)
{
    size_t bytes_read, tag_size;
    uint32_t val;
    int32_t rv;
    mpeg4_decoder_config_t *dc;

    bytes_read = 0;
    val = 0;
    rv = 0;
    tag_size = 0;

    dc = esd->dc;
    //esd->dc = dc = (mpeg4_decoder_config_t *)nkn_calloc_type(1, sizeof(mpeg4_decoder_config_t));
    dc->profile_level_indicator = bio_read_bits(bs, 8);
    dc->stream_type = bio_read_bits(bs, 6);
    dc->upstream = bio_read_bits(bs, 1);
    dc->specific_info_flag = bio_read_bits(bs, 1);
    dc->decoder_buffer_size = bio_read_bits(bs, 24);
    dc->max_bitrate = bio_read_bits(bs, 32);
    dc->avg_bitrate = bio_read_bits(bs, 32);

    bytes_read += 13;

    while(bytes_read < size) {
	rv = mpeg4_es_read_object_descriptor(bs, size - bytes_read, esd, &tag_size);
	bytes_read += tag_size + get_object_descriptor_size(tag_size);
    }
	
    return 0;   
}

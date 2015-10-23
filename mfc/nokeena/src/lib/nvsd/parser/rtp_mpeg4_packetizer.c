/*
 *
 * Filename:  rtp_mpeg4_packetizer.c
 * Date:      2009/03/28
 * Module:    MPEG4 RTP packetization based on RFC 3640 for AAC
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */
#include <sys/uio.h>
#include "rtp_mpeg4_packetizer.h"
#include "rtp_packetizer_common.h"
#include "nkn_vpe_bufio.h"
#include "nkn_memalloc.h"
#include "glib.h"
#include "nkn_vfs.h"

#define AU_HDR_SIZE 200
#define RTP_PKT_SIZE 12
/* build MPEG 4 RTP packet based on the RFC 3640
 */
int32_t
build_mpeg4_rtp_packet(rtp_packet_builder_t* bldr,int32_t mode,int32_t nal_size,uint8_t *nalu,int32_t AUEnd, uint32_t AUStart)
{
    int32_t bytes_rem_in_pkt, tmp;
    int32_t auh_size, split_pkt, ret;
    mpeg4_es_desc_t *esd;
    nal_info_t *nal;

    esd = bldr->es_desc;
    nal = bldr->nal;
    bytes_rem_in_pkt = nal_size;
    split_pkt = 0;
    tmp = 0;
    ret = RTP_PKT_NOT_READY;
    
    /* only 3 cases are possible now!
     * a. nal needs to be added to current packet
     *    a1. nal can be the first nal in the RTP packet, needs to be handled differently
     * b. nal needs to be split between current and next packet
     *    b1. packet needs to be flushed
     *    b2. remaining data needs to be carried over to the next packet
     * c. nal exactly completes the RTP packet and packet
     *    c1. packet needs to be flushed
     */
    
    while(bytes_rem_in_pkt) {

	if(bldr->new_pkt) { //case (a)
	    create_new_rtp_packet(bldr);
	}

	auh_size = esd->auh->size_length + esd->auh->index_length;
	tmp = auh_size + bldr->payload_hdr_size;//store temporarily (we may not write the payload hdrs into this packet)
	tmp = byte_align_size(tmp);
	
	if(bytes_rem_in_pkt + tmp + bldr->payload_size <= (int32_t)bldr->mtu_size) { // case (b) add to current packet
	    add_au_header(bldr, bytes_rem_in_pkt);
	    add_to_payload(bldr, nalu, nal->num_bytes_written, bytes_rem_in_pkt);
	    bldr->payload_hdr_size += auh_size;
	    bldr->payload_size += bytes_rem_in_pkt;
	    bldr->nal->num_bytes_written = 0;
	    bytes_rem_in_pkt = 0;//bldr->nal->num_bytes_written;
	    bldr->new_pkt = 0;
	    if(bldr->nal_data != NULL)
		SAFE_FREE(bldr->nal_data);
	    bldr->nal_data = NULL;
	    bldr->nal_bytes = 0;
	    if(ret == RTP_PKT_READY) {//one packet has been finalized and a new packet has been created
		//bio_close_bitstream(bldr->payload_writer);
		//bio_close_bitstream(bldr->payload_hdr_writer);
		return ret;
	    } else { //continue pumping in more samples to complete this packet
		return RTP_PKT_NOT_READY;
	    }
	} else {
	    finalize_rtp_packet(bldr);
	    bldr->packet_ready(bldr, bldr->cb);
	    bldr->new_pkt = 1;
	    ret = RTP_PKT_READY;
	    bio_close_bitstream(bldr->payload_writer);
	    bio_close_bitstream(bldr->payload_hdr_writer);
	}
    }
    return ret;
}

/**********************************************************************************
 *              RTP PACKET DATA ORGANIZATION                          
 *  data is stored in an un-packed manner as we cannot determine the AU-HEADER size
 *  before construction of the RTP packet. Hence we leave a 200 byte forbidden space 
 *  in the packet for AU-HEADERs and later pack the data on finalization
 *    0-----11-----------------------211----------------------------------200+MTU
 *    |RTP  |    AU-HEADER            |      RTP-PAYLOAD                       |
 *    |HDR  |                         |                                        |
 *    --------------------------------------------------------------------------
 *******************************************************************************/
static inline int32_t
finalize_rtp_packet(rtp_packet_builder_t *bldr)
{
    int8_t *src, *dst;
    int32_t size, tmp;
    bitstream_state_t *bs;

    //write the number of AU-HEADERS
    bs = bldr->payload_hdr_writer;
    bio_aligned_seek_bitstream(bs, 0, SEEK_SET);
    bio_write_int(bs, bldr->payload_hdr_size - 16, 16);

    //start packing the data
    tmp = byte_align_size(bldr->payload_hdr_size);
    dst = bldr->payload_writer->data + 12 + tmp;
    src = bldr->payload_writer->data + 12 + AU_HDR_SIZE;
    size = bldr->payload_size;

    memmove(dst, src, size);    
    size = 12 + tmp + bldr->payload_size;
    bldr->payload_size = size;
    return 0;
}

static inline int32_t 
add_to_payload(rtp_packet_builder_t *bldr, uint8_t* payload, int32_t offset, int32_t size)
{
    uint8_t *src;
    bitstream_state_t *bs;

    src = payload + offset;
    bs = bldr->payload_writer;
    bio_aligned_write_block(bs, payload, size);    

    return size;    
}

int32_t
add_au_header(rtp_packet_builder_t *bldr, int32_t nal_size)
{
    mpeg4_au_header_t *auh;

    auh = bldr->es_desc->auh;
    auh->last_au_sequence_num = auh->au_sequence_num;
    auh->au_sequence_num++;

    bio_write_int(bldr->payload_hdr_writer, nal_size, auh->size_length);
    if(auh->index_delta_length) { //section 3.2.1.1 in rfc 3640
	if(bldr->new_pkt) {
	    bio_write_int(bldr->payload_hdr_writer, auh->au_sequence_num, auh->index_delta_length);
	} else {
	    int32_t diff;
	    diff = auh->au_sequence_num - auh->last_au_sequence_num;
	    diff--;
	    bio_write_int(bldr->payload_hdr_writer, diff, auh->index_delta_length);
	}
    }

    return 0;
}

int32_t
create_new_rtp_packet(rtp_packet_builder_t *bldr)
{
    rtp_header_t *rtp;
    mpeg4_es_desc_t *esd;
    size_t alloc_size;

    alloc_size = 0;

    esd = bldr->es_desc;
    rtp = &bldr->header;
    rtp->version = 0x02;
    rtp->padding = 0;
    rtp->extension = 0;
    rtp->CSRC_count = 0;
    rtp->marker = 1;
    rtp->payload_type = bldr->rtp_payload_type;
    rtp->sequence_num++;
    rtp->timestamp = bldr->nal->DTS;

    bldr->payload_size = 0;
    bldr->payload_hdr_size = 0;

    bldr->rtp_pkts = (uint8_t*)nkn_calloc_type(1, RTP_PKT_SIZE +\
					       bldr->mtu_size +\
					       AU_HDR_SIZE, 
					       mod_vpe_rtp_pkt_buf);
    alloc_size = RTP_PKT_SIZE + bldr->mtu_size + AU_HDR_SIZE;

    //bldr->rtp_pkt_size = (int32_t*)nkn_calloc_type(1, sizeof(int32_t));
    //*bldr->rtp_pkt_size = bldr->mtu_size;
    bldr->payload_hdr = bldr->rtp_pkts + 12;
    bldr->payload_hdr_size = 0;

    bldr->payload_writer = bio_init_bitstream(bldr->rtp_pkts, RTP_PKT_SIZE + bldr->mtu_size + AU_HDR_SIZE);//*bldr->rtp_pkt_size);
 
    //write the RTP header
    write_rtp_header(bldr);
    //skip the forbidden section reseverved for AU-HEADERS, we will pack later
    bio_aligned_seek_bitstream(bldr->payload_writer, 12 + AU_HDR_SIZE, SEEK_SET);

    //reserve space for AU-Headers
    bldr->payload_hdr_writer = bio_init_bitstream(bldr->payload_hdr, AU_HDR_SIZE);
    //write the AU-Header Size Field*/
    bio_write_int(bldr->payload_hdr_writer, 0, esd->auh->size_length + esd->auh->index_length);
    //reserve space for AU-Header Size
    bldr->payload_hdr_size += esd->auh->size_length + esd->auh->index_length;

    return 0;

}

int32_t
init_mpeg4_packet_builder(parser_t *parser, rtp_packet_builder_t *bldr, int32_t payload_type,\
			  int32_t(*mpeg4_packet_ready_handler)(void *, void*), void *cb)
{
    int32_t esds_offset, esds_size, sdp_size, nal_size;
    size_t esd_size;
    uint8_t *esds_data, *nalu;
    bitstream_state_t *bs;
    mpeg4_es_desc_t *esd;
    //    rtp_packet_builder_t *bldr;
    rtp_header_t *rtp;
    
    char *sdp;
    FILE *fid;
    
    fid = parser->fid;
    sdp_size = 0;
    nal_size = 0;
    sdp = NULL;
    nalu = NULL;
    rtp = &bldr->header;
    memset(bldr, 0, sizeof(rtp_packet_builder_t));
    bldr->es_desc = esd = mpeg4_init_es_descriptor();

    //read the esd information
    esds_size = getesds(parser->st_all_info, parser->trak_type, nkn_vpe_buffered_read, parser->fid, &esds_offset) + 4;
    esds_data = (uint8_t*)nkn_malloc_type(esds_size, mod_vpe_mp4_esds_buf);
    nkn_vfs_fseek(fid, esds_offset, SEEK_SET);
    nkn_vpe_buffered_read(esds_data, 1, esds_size, fid);

    //glean esd info from the MPEG4 systems stream
    bs = bio_init_bitstream(esds_data + 13, esds_size - 13);
    mpeg4_es_read_object_descriptor(bs, esds_size, esd, &esd_size);
    bio_close_bitstream(bs);
    if(esds_data != NULL)
	SAFE_FREE(esds_data);

    //glean decoder specific info from the MPEG4 systems stream
    if(esd->dc->specific_info) {
	bs = bio_init_bitstream(esd->dc->specific_info, esd->dc->specific_info_size);
	m4a_read_decoder_specific_info(bs, esd->dsi);
	bio_close_bitstream(bs);
    } 

   //initialize the builder structure
    bldr->rtp_payload_type = 96;
    bldr->mtu_size = MTU_SIZE;
    bldr->es_desc = esd;
    //bldr->rtp_pkts = (uint8_t*)nkn_calloc_type(1, bldr->mtu_size + AU_HDR_SIZE);
    //bldr->rtp_pkt_size = (int32_t*)nkn_calloc_type(1, sizeof(int32_t));
    //*bldr->rtp_pkt_size = bldr->mtu_size;
    //bldr->payload_hdr = bldr->rtp_pkts + 12;
    //bldr->payload_hdr_size = 0;
    bldr->nal = (nal_info_t*)nkn_calloc_type(1, sizeof(nal_info_t), mod_vpe_rtp_nal_buf);
    bldr->nal->num_bytes_written = 0;
    bldr->nal->DTS = 0;
    bldr->new_pkt = 1;

    bldr->packet_ready = mpeg4_packet_ready_handler;
    bldr->cb = cb;
    map_mpeg4_systems_to_rfc_3640(bldr, 64);

    //read sdp data
    //build_mpeg4_sdp_lines(bldr, &sdp, &sdp_size);

    return 0;
}

int32_t 
cleanup_mpeg4_packet_builder(rtp_packet_builder_t *bldr) 
{
    if(bldr->rtp_pkts != NULL)
	SAFE_FREE(bldr->rtp_pkts);
    if(bldr->payload_writer != NULL)
	SAFE_FREE(bldr->payload_writer);
    if(bldr->payload_hdr_writer != NULL)
	SAFE_FREE(bldr->payload_hdr_writer);
    mpeg4_cleanup_es_descriptor(bldr->es_desc);
    if(bldr->nal !=NULL)
	SAFE_FREE(bldr->nal);
    //    if(bldr != NULL)
    //SAFE_FREE(bldr);

    return 0;
}    

/* refer section 1.5.2.4 of 14496-3 sp-1, table 1.12 & 1.11 */
int32_t
compute_profile_level_id(uint32_t base_object_type, uint32_t channel_config, uint32_t sampling_rate)
{
    switch(base_object_type) {
	case 2: /* AAC- LoCo */
	    if(channel_config <= 2) {
		return (sampling_rate <= 24000) ? 0x28 : 0x29; /*LC@L1 & LC@L2 */
	     }
	    return (sampling_rate <= 48000) ? 0x2A : 0x2B; /*LC@L3 & LC@L4 */
	case 5: /* AAC- High Efficiency */
	    if(channel_config <= 2) {
                return (sampling_rate <= 24000) ? 0x2C : 0x2D; /*HE@L2 & HE@L3 */
	    }
            return (sampling_rate <= 48000) ? 0x2A : 0x2B; /*HE@L4 & HE@L5 */
	default: /* nothing else but HQ to default to! */
	    if(channel_config <= 2) {
                return (sampling_rate <= 24000) ? 0x0E : 0x0f; /*HQ@L1 & HQ@L2 */
	    }
            return 0x10; /* HQ@L3 */
    }

}

int32_t
map_mpeg4_systems_to_rfc_3640(rtp_packet_builder_t *bldr, size_t max_au_size)
{
    mpeg4_es_desc_t *esd;
    mpeg4_au_header_t *auh;

    esd = bldr->es_desc;
    auh = esd->auh;

    auh->stream_type = esd->dc->stream_type;
    if(esd->dc->specific_info) {
	auh->pl_id = compute_profile_level_id(esd->dsi->base_object_type, esd->dsi->channel_config, esd->dc->avg_bitrate);
    } else {
	auh->pl_id = compute_profile_level_id(0, 0, 0);
    }

    auh->object_type_indicator = esd->dc->profile_level_indicator;

    if(max_au_size< 63) { /* AAC-lbr */
	strcpy(auh->mode, "AAC-lbr");
	auh->index_length = auh->index_delta_length = 2;
	auh->size_length = 6;
    } else {
	strcpy(auh->mode, "AAC-hbr");
	auh->index_length = auh->index_delta_length = 3;
        auh->size_length = 13;
    }

    return 0;
    
}

int32_t 
build_mpeg4_sdp_lines(rtp_packet_builder_t *bldr, char **sdp, int32_t *sdp_size,int32_t height,int32_t width)
{
    char *buf;
    char pl_name[50], media_type[50], *dsi_str;
    int32_t profile_level_id;
    mpeg4_es_desc_t *esd;
    mpeg4_au_header_t *auh;

    esd = bldr->es_desc;
    auh = esd->auh;
    *sdp = buf = (char*)nkn_calloc_type(1, MAX_SDP_SIZE, mod_vpe_rtp_sdp_buf);
    if(esd->dc->specific_info) {
	    dsi_str = (char*)nkn_calloc_type(1,
					     (2*esd->dc->specific_info_size)+1, 
					     mod_vpe_rtp_generic_buf);
    } else {
	dsi_str = NULL;
    }

    switch(auh->stream_type) {
	case 5:
	    strcpy(pl_name, "audio");
	    break;
	case 4:
	    strcpy(pl_name, "video");
	    break;
    }
    strcpy(media_type, "mpeg4-generic");

    if(esd->dc->specific_info) {
	snprintf(buf, MAX_SDP_SIZE, "m=%s 0 RTP/AVP %d\r\nb=AS:%d\r\na=rtpmap:%d %s/%d/%d\r\na=control:trackID=%d\r\n", pl_name, bldr->rtp_payload_type, esd->dc->avg_bitrate/1000, bldr->rtp_payload_type, \
		media_type, esd->dsi->sampling_freq, esd->dsi->channel_config, 3);
    } else {
	snprintf(buf, MAX_SDP_SIZE, "m=%s 0 RTP/AVP %d\r\nb=AS:%d\r\na=rtpmap:%d %s/%d/%d\r\na=control:trackID=%d\r\n", pl_name, bldr->rtp_payload_type, esd->dc->avg_bitrate/1000, bldr->rtp_payload_type, \
		media_type, 44100, 2, 3);
    }

    strcat(buf, "a=fmtp:96 ");//, bldr->rtp_payload_type);
    add_first_name_value_pair_int(buf, "profile-level-id", auh->pl_id);
    if(esd->dc->specific_info) 
	constuct_m4a_config_str(esd->dc->specific_info, esd->dc->specific_info_size, dsi_str);
    if(dsi_str) 
	add_name_value_pair_char(buf, "config", dsi_str);
    add_name_value_pair_int(buf, "streamType", auh->stream_type);
    add_name_value_pair_char(buf, "mode", auh->mode);
    add_name_value_pair_int(buf, "objectType", auh->object_type_indicator);
    add_name_value_pair_int(buf, "sizeLength", auh->size_length);
    add_name_value_pair_int(buf, "indexLength", auh->index_length);
    add_name_value_pair_int(buf, "indexDeltaLength", auh->index_delta_length);
    strcat(buf,"\r\n");

    *sdp_size = strlen(buf);

    if(dsi_str)
	free(dsi_str);
    //if(buf != NULL)
    //free(buf);
    return 0;
    
}

int32_t
write_rtp_header(rtp_packet_builder_t *bldr)
{
    rtp_header_t *rtp;
    bitstream_state_t *bs;
    
    rtp = &bldr->header;
    bs = bldr->payload_writer;

    bio_write_int(bs, rtp->version, 2);//version
    bio_write_int(bs, rtp->padding, 1);//padding
    bio_write_int(bs, rtp->extension, 1);//extension bit
    bio_write_int(bs, 0, 4);//csrc identifer
    bio_write_int(bs, rtp->marker, 1);//marker bit
    bio_write_int(bs, rtp->payload_type, 7);//payload type
    bio_write_int(bs, rtp->sequence_num, 16);//sequence number
    bio_write_int(bs, rtp->timestamp, 32);//DTS
    bio_write_int(bs, 0x7013, 32);//ssrc

    return 0;
}

    
    

/***********************HELPER FUNCTIONS *****************************/
static inline void
constuct_m4a_config_str(uint8_t* data, size_t size, char *out)
{
    size_t i, j;


    for(i=0, j=0; i < size; i++,j+=2) {
	snprintf(&out[j], 3, "%02x", data[i]);
    }
    out[j]='\0';
	
}

static inline void
add_first_name_value_pair_int(char *buf, const char *name, int32_t value)
{
    char tmp[256];

    snprintf(tmp, 256, "%s=%d", name, value);
    strcat(buf, tmp);
}

static inline void
add_first_name_value_pair_char(char *buf, const char *name, char *value)
{
    char tmp[256];

    snprintf(tmp, 256, "%s=%s", name, value);
    strcat(buf, tmp);
}

static inline void
add_name_value_pair_int(char *buf, const char *name, int32_t value)
{
    char tmp[256];

    strcat(buf, ";");
    snprintf(tmp, 256, "%s=%d", name, value);
    strcat(buf, tmp);
}

static inline void
add_name_value_pair_char(char *buf, const char *name, char *value)
{
    char tmp[256];

    strcat(buf, ";");
    snprintf(tmp, 256, "%s=%s", name, value);
    strcat(buf, tmp);
}

int32_t 
get_mpeg4_pl_name(mpeg4_es_desc_t *esd, char *pl_name, char *media_type)
{
    const char *pl_strs[2] = { "video", "audio" };
    const char *mt_strs = "mpeg4-generic";
    int32_t pl_str_idx = 0;
    char *tmp;

    switch(esd->dc->stream_type) {
	case 5:
	    pl_str_idx = 1;
	    break;
	default:
	    pl_str_idx = 0;
	    break;
    }

    strcpy(pl_name, pl_strs[pl_str_idx]);//, strlen(pl_name[pl_str_idx]));
    strcpy(media_type, mt_strs);//, strlen(mt_strs[0]));

    return 0;
}

static inline int32_t
byte_align_size(int32_t s)
{
    int32_t ret;
    
    ret = s/8;
    if(s % 8) { 
	ret++;
    }

    return ret;
}

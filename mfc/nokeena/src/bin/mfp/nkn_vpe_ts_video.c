//#include "nkn_vpe_mfp_ts.h"

#include "nkn_vpe_ts_video.h"
#include "nkn_vpe_bitio.h"
#include <stdlib.h>
#include "math.h"
#include "string.h"
#include "nkn_memalloc.h"
//#include "nkn_vpe_ismv_mfu.h"

#define STREAM_ID 0xE0
#define VID_PID 257
#define TS_PKT_SIZE 188
#define TS_HEADER_SIZE 4
#define PES_HEADER_SIZE 19
#define ADAPTATION_FIELD_SIZE_WITH_PCR 7
#define ADAPTATION_FIELD_SIZE_WITHOUT_PCR 1

#define ACC_UNI_DEL
//#define bit_write

static int32_t 
mfu_packetize_AU(uint8_t*, ts_pkt_list_t* , mfu_videots_desc_t *);
static inline int32_t 
mfu_packetize_TS_packet(uint8_t *, ts_pkt_list_t*, mfu_AU_t *,
			pld_pkt_t*, uint32_t, mfu_videots_desc_t *);
static inline int32_t 
mfu_initialize_ind_AU(mfu_AU_t *);
static inline uint32_t 
mfu_find_num_TS_pkt_per_AU(uint32_t );
static inline int32_t 
mfu_initialize_ind_AU_TS_pkts(pld_pkt_t *, mfu_videots_desc_t *);


/*
This function is used to form ts pkts for given number of
AU's("num_AUs").
*data -> i/p which will have the no:of AU's, AU size, AU data
* pktlist ->o/p which will have the ts pkts generated
*/ 
int32_t 
mfu_get_ts_h264_pkts(mdat_desc_t* data, ts_pkt_list_t* pktlist,
		     mfu_videots_desc_t *video, int32_t num_AUs,
		     int32_t start_AU_num)
{
    uint8_t *initial_data = NULL, *initial_pktlist_data = NULL;
    uint32_t pos=0,duration_pos=0,data_pos=0,AU_temp_size=0;
    uint32_t num_ts_pkts = 0;
    int32_t i=0;

    video->timescale = data->timescale;
    video->num_AUs = num_AUs;
    pos = (start_AU_num -1)*1;
    duration_pos = (start_AU_num -1)*1;

    //calculate the total number of TS pkts for given number of samples
    for (i=0;i<num_AUs;i++) {
	if (data->default_sample_size != 0) {
	    //same sample size
	    AU_temp_size= data->default_sample_size;
	} else {
	    //different sample size
	    AU_temp_size =*(data->sample_sizes+pos);
	    pos+=1;
	}
	if (video->Is_sps_dumped == 0) {
	    AU_temp_size = AU_temp_size + video->sps_size; 
	}
#ifdef ACC_UNI_DEL
	AU_temp_size+=6;
#endif
	num_ts_pkts +=  mfu_find_num_TS_pkt_per_AU(AU_temp_size);
	}
    //allocate memory for entire TS pkts for num_AUs
    pktlist->data = (uint8_t*)
      nkn_calloc_type(1, TS_PKT_SIZE*num_ts_pkts,
			mod_vpe_pktlist_vid_data_buf);

    initial_pktlist_data = pktlist->data;
    
    pktlist->num_entries = num_ts_pkts;
    pos = 0;
    for (i=0;i<(start_AU_num-1);i++) {
	data_pos += *(data->sample_sizes+pos);
	pos+=1;
    }
    pos = (start_AU_num -1)*1;
    duration_pos = (start_AU_num -1)*1;
    for (i=0;i<num_AUs;i++) {
	video->ind_AU = (mfu_AU_t*)
	  nkn_calloc_type(1,sizeof(mfu_AU_t)*1/*num_AUs*/,
			    mod_vpe_mfu_AU_t);
	/*initialize the individual AU */
	mfu_initialize_ind_AU(video->ind_AU);

	if (data->default_sample_size != 0){
	    //same sample size
	    video->ind_AU->AU_size= data->default_sample_size;
	} else {
	    //different sample size
	    video->ind_AU->AU_size =*(data->sample_sizes+pos);
	    pos+=1;
	}
	video->ind_AU->AU_temp_size = video->ind_AU->AU_size;
	if (video->Is_sps_dumped == 0) {
	    video->ind_AU->AU_temp_size= video->ind_AU->AU_size +
		video->sps_size;
	}
#ifdef ACC_UNI_DEL
	video->ind_AU->AU_temp_size+=6;
#endif

	video->ind_AU->AU_time_stamp = (uint64_t)
		(((double)(video->base_time*90000)/video->timescale));

	if (data->default_sample_duration != 0) {
            //same sample duration
	    video->base_time += data->default_sample_duration;
        } else {
            //different sample duration
	    video->base_time += data->sample_duration[duration_pos];
            video->ind_AU->AU_comp_time_stamp= (int64_t)
	    	((((double)(int32_t)data->composition_duration[duration_pos])*90000)/video->timescale);
            duration_pos+=1;
        }
	
	video->ind_AU->num_TS_packets_per_AU =
	    mfu_find_num_TS_pkt_per_AU(video->ind_AU->AU_temp_size);
	initial_data= data->mdat_pos+data_pos;
	//packetizeing each AU
	mfu_packetize_AU(initial_data,pktlist,video);
	
	data_pos+=video->ind_AU->AU_size;
	free(video->ind_AU);
    }
    pktlist->data = initial_pktlist_data;
    
    return VPE_SUCCESS;
}


static int32_t 
mfu_packetize_AU(uint8_t* data, ts_pkt_list_t* pktlist,
		 mfu_videots_desc_t *video)
{
    uint32_t i=0;
    
    video->ind_AU->num_TS_packets_per_AU_written = 0;
    video->ind_AU->Is_first_TS_pkt = 1;
    
    for (i=0;i<video->ind_AU->num_TS_packets_per_AU;i++) {
	//allocation of each TS_packet
	video->ind_AU->TS_pkts = (pld_pkt_t*)
	  nkn_calloc_type(1, sizeof(pld_pkt_t), mod_vpe_pld_pkt_t);
	video->ind_AU->TS_pkts->hdr.af = (adapt_field_t *)
	  nkn_calloc_type(1, sizeof(adapt_field_t),
			    mod_vpe_adapt_field_t);
	video->ind_AU->TS_pkts->pes.pf = (pes_fields_t *)
	  nkn_calloc_type(1, sizeof(pes_fields_t),
			    mod_vpe_pes_fields_t);

	//initializing individual Ts pkts
	mfu_initialize_ind_AU_TS_pkts(video->ind_AU->TS_pkts,video);
	video->ind_AU->AU_size_tobe_written = video->ind_AU->AU_size -
	    video->ind_AU->AU_size_written;
	
	if (video->ind_AU->Is_first_TS_pkt == 1) {
	    //first TS pkt for AU
	    video->ind_AU->PCR_flag =1;
	    video->ind_AU->payload_unit_start_indicator = 1;
	    video->ind_AU->adaptation_field_control = 0x3;
	    if (video->ind_AU->PCR_flag ==1) {
		video->ind_AU->adaptation_field_len =
		    ADAPTATION_FIELD_SIZE_WITH_PCR;
	    } else {
		video->ind_AU->adaptation_field_len =
		    ADAPTATION_FIELD_SIZE_WITHOUT_PCR;
	    }
	}
	if ((video->ind_AU->num_TS_packets_per_AU -
	     video->ind_AU->num_TS_packets_per_AU_written) == 1) {
	    //last TS  packet per AU
	    if (video->ind_AU->Is_first_TS_pkt == 1) {
		//suppose first pkt itself is last pkt
		video->ind_AU->Is_first_last_TS_pkt = 1;
		video->ind_AU->Is_intermediate_TS_pkt =0;
		video->ind_AU->payload_unit_start_indicator = 1;
		video->ind_AU->PCR_flag =1;
		video->ind_AU->adaptation_field_control = 0x3;
		if (video->ind_AU->PCR_flag ==1) {
		    video->ind_AU->adaptation_field_len =
			ADAPTATION_FIELD_SIZE_WITH_PCR;
		} else {
		    video->ind_AU->adaptation_field_len =
			ADAPTATION_FIELD_SIZE_WITHOUT_PCR;
		}
		video->ind_AU->stuffing_bytes= TS_PKT_SIZE -
		    (TS_HEADER_SIZE + 1 +
		     video->ind_AU->adaptation_field_len +
		     PES_HEADER_SIZE +
		     (video->ind_AU->AU_temp_size-video->ind_AU->AU_size) +
		     video->ind_AU->AU_size);
		video->ind_AU->adaptation_field_len += video->ind_AU->stuffing_bytes;
#if 0
		if(video->ind_AU->stuffing_bytes > 188) {
		  video->ind_AU->stuffing_bytes = video->ind_AU->stuffing_bytes;
		  video->ind_AU->stuffing_bytes = 188;
		}
#endif
	    } else {
		video->ind_AU->Is_last_TS_pkt = 1;
		video->ind_AU->Is_intermediate_TS_pkt =0;
		video->ind_AU->payload_unit_start_indicator = 0;
		video->ind_AU->PCR_flag =0;
		video->ind_AU->adaptation_field_control = 0x3;
		video->ind_AU->adaptation_field_len = TS_PKT_SIZE -
		    (video->ind_AU->AU_size_tobe_written + TS_HEADER_SIZE + 1);
		video->ind_AU->stuffing_bytes =
		    video->ind_AU->adaptation_field_len - 1;
#if 0
		if(video->ind_AU->stuffing_bytes > 188) {
		  video->ind_AU->stuffing_bytes = video->ind_AU->stuffing_bytes;
		  video->ind_AU->stuffing_bytes = 188;
		}
#endif
	    }
	}
	if ((video->ind_AU->Is_first_TS_pkt == 0) &&
	    (video->ind_AU->Is_last_TS_pkt == 0) &&
	    (video->ind_AU->Is_first_last_TS_pkt == 0)) { 
	    //intermediate pkts (not first and last TS pkts)
	    video->ind_AU->Is_intermediate_TS_pkt =1;
	    video->ind_AU->adaptation_field_control = 0x3;
	    
	    video->ind_AU->adaptation_field_len = 1;
	    video->ind_AU->PCR_flag =0;
	    video->ind_AU->payload_unit_start_indicator = 0;
	}
	/*packetise each TS packet */
	mfu_packetize_TS_packet(data, pktlist, video->ind_AU,
				video->ind_AU->TS_pkts, video->num_TS_pkts, video);
	
	data += video->ind_AU->AU_bytes_written_per_TS;
	pktlist->data = pktlist->data+TS_PKT_SIZE;
	video->ind_AU->num_TS_packets_per_AU_written++;
	video->num_TS_pkts++;
	if (video->ind_AU->TS_pkts != NULL) {
	  if(video->ind_AU->TS_pkts->hdr.af != NULL)
	    free(video->ind_AU->TS_pkts->hdr.af);
	    if(video->ind_AU->TS_pkts->pes.pf != NULL)
		free(video->ind_AU->TS_pkts->pes.pf);
	    free(video->ind_AU->TS_pkts);
	}
    }
    return VPE_SUCCESS;
}

/* This function is used to packetise the each TS pkt
 */
static inline int32_t 
mfu_packetize_TS_packet(uint8_t *data, ts_pkt_list_t* pktlist,
			mfu_AU_t *ind_AU, pld_pkt_t* TS_pkts,
			uint32_t num_TS_pkts,
			mfu_videots_desc_t *video)
{
    uint32_t num_bytes_used = 0;
    uint32_t total_num_bytes = TS_PKT_SIZE;
    uint8_t dump=0xff;
    uint32_t i=0;
    uint8_t *output_data = pktlist->data;
    bitstream_state_t * bs;
    double time_temp;

    ind_AU->AU_bytes_written_per_TS = 0;    
    
    TS_pkts->hdr.payload_unit_start_indicator =
	ind_AU->payload_unit_start_indicator;
    TS_pkts->hdr.adaptation_field_control =
	ind_AU->adaptation_field_control;
    TS_pkts->hdr.continuity_counter = (video->continuity_counter++)%16;
    TS_pkts->hdr.af->adaptation_field_len =
	ind_AU->adaptation_field_len;
    TS_pkts->hdr.af->PCR_flag = ind_AU->PCR_flag;

    bs = bio_init_bitstream(output_data, TS_PKT_SIZE*8);
    /*write TS header*/
    bio_write_int(bs, TS_pkts->hdr.sync_byte, 8);
    bio_write_int(bs, TS_pkts->hdr.transport_error_indicator, 1);
    bio_write_int(bs, TS_pkts->hdr.payload_unit_start_indicator, 1);
    //TS_pkts->hdr.payload_unit_start_indicator = 0;
    bio_write_int(bs, TS_pkts->hdr.transport_priority, 1);
    bio_write_int(bs, TS_pkts->hdr.PID, 13);
    bio_write_int(bs, TS_pkts->hdr.transport_scrambling_control, 2);
    bio_write_int(bs, TS_pkts->hdr.adaptation_field_control, 2);
    bio_write_int(bs, TS_pkts->hdr.continuity_counter, 4);
    num_bytes_used += 4;
    /*write Adaptation feilds ***/
    bio_write_int(bs, TS_pkts->hdr.af->adaptation_field_len, 8);
    bio_write_int(bs, TS_pkts->hdr.af->discontinuity_indicator, 1);
    bio_write_int(bs, TS_pkts->hdr.af->random_access_indicator, 1);//1 bit
    bio_write_int(bs,
		  TS_pkts->hdr.af->elementary_stream_priority_indicator, 1);
    bio_write_int(bs, TS_pkts->hdr.af->PCR_flag, 1);
    bio_write_int(bs, TS_pkts->hdr.af->OPCR_flag, 1);
    bio_write_int(bs, TS_pkts->hdr.af->splicing_point_flag, 1);
    bio_write_int(bs, TS_pkts->hdr.af->transport_private_data_flag,
		  1);
    bio_write_int(bs,
		  TS_pkts->hdr.af->adaptation_field_extension_flag, 1);
    num_bytes_used += 2;
    if (TS_pkts->hdr.af->PCR_flag ==1) {
	uint64_t PCR_base =0;
	uint32_t PCR_ext=0;
#if 1
	time_temp =video->time_of_day+0.6*video->frame_rate;
	video->time_of_day = time_temp;
#else
	time_temp = 0.5*ind_AU->AU_time_stamp/video->timescale;
#endif
	PCR_base = 90000*(time_temp);
	PCR_ext = (uint32_t)(27000000*time_temp)%300;
	TS_pkts->hdr.af->pcr.program_clock_reference_base = PCR_base;
	bio_write_int(bs,
		      TS_pkts->hdr.af->pcr.program_clock_reference_base>>31, 2);
	bio_write_int(bs,
		      TS_pkts->hdr.af->pcr.program_clock_reference_base&0x7FFFFFFF, 31);
	TS_pkts->hdr.af->pcr.program_clock_reference_extension = PCR_ext;
	bio_write_int(bs, 0x3F, 6);
	bio_write_int(bs,
		      TS_pkts->hdr.af->pcr.program_clock_reference_extension, 9);
	num_bytes_used += 6;
    }//TS_pkts->hdr.af.PCR_flag

    //write the stuffing bytes before writting PES header
    if((ind_AU->Is_first_last_TS_pkt == 1)) {
        uint32_t temp = ind_AU->stuffing_bytes;
#if 0
	if(ind_AU->stuffing_bytes > 188) {
	  ind_AU->stuffing_bytes = ind_AU->stuffing_bytes;
	  ind_AU->stuffing_bytes = 188;
	}
#endif
        for (i=0;i<temp;i++) {
            *(bs->data+bs->pos) = dump;
            bs->pos+=1;
            num_bytes_used++;
        }
    }    
    // write PES header if it is first TS packet
    if ((ind_AU->Is_first_TS_pkt == 1) ||(ind_AU->Is_first_last_TS_pkt
					  ==1)) {
	uint64_t PTS=0,PTS_temp=0,DTS=0,DTS_temp=0;
	uint8_t reserved=0x2;
	uint8_t reserved1=0x3;
	uint8_t reserved2 = 0x1;
	uint8_t marker_bit = 0x1;
	bio_write_int(bs, TS_pkts->pes.packet_start_code_prefix, 24);
	bio_write_int(bs, TS_pkts->pes.stream_id, 8);
	bio_write_int(bs, TS_pkts->pes.PES_packet_length, 16);
	num_bytes_used += 6 ;
	//include fixed bits"10"//2 bits
	bio_write_int(bs, reserved, 2);
	bio_write_int(bs, TS_pkts->pes.pf->PES_scrambling_control, 2);
	bio_write_int(bs, TS_pkts->pes.pf->PES_priority, 1);
	bio_write_int(bs, TS_pkts->pes.pf->data_alignment_indicator, 1);
	bio_write_int(bs, TS_pkts->pes.pf->copyright, 1);
	bio_write_int(bs, TS_pkts->pes.pf->original_or_copy, 1);//1 bit
	bio_write_int(bs, TS_pkts->pes.pf->PTS_DTS_flags, 2);
	bio_write_int(bs, TS_pkts->pes.pf->ESCR_flag, 1);
	bio_write_int(bs, TS_pkts->pes.pf->ES_rate_flag, 1);
	bio_write_int(bs, TS_pkts->pes.pf->DSM_trick_mode_flag, 1);
	bio_write_int(bs, TS_pkts->pes.pf->additional_copy_info_flag, 1);
	bio_write_int(bs, TS_pkts->pes.pf->PES_CRC_flag, 1);
	bio_write_int(bs, TS_pkts->pes.pf->PES_extension_flag, 1);
	bio_write_int(bs, TS_pkts->pes.pf->PES_header_data_length, 8);
	num_bytes_used += 3 ;
	if (TS_pkts->pes.pf->PTS_DTS_flags == 0x3) {
	    double pts_time;
	    double dts_time;
	    uint64_t CTS;
	    {
		pts_time = video->frame_time+video->frame_rate;
		video->frame_time = pts_time;
		pts_time-=video->frame_rate;
		PTS = (uint64_t)(pts_time*90000)%POW2_33;//%(pow(2,33));
		CTS = PTS;
	    }

	    dts_time = ind_AU->AU_time_stamp;///video->timescale;
	    pts_time = dts_time + ind_AU->AU_comp_time_stamp;
	    PTS = (uint64_t)(pts_time)%POW2_33;
	    DTS = (uint64_t)(dts_time)%POW2_33;
	    PTS &= 0xFFFFFFFF;
	    DTS &= 0xFFFFFFFF;
#ifdef PRINT
	    printf("OLD PTS = %lu New PTS = %lu, Diff = %d, \n",CTS,PTS, (int32_t)PTS-(int32_t)CTS);
#endif
	    bio_write_int(bs,reserved1,4);
	    PTS_temp = (PTS >>30)&0x7;
	    bio_write_int(bs, PTS_temp, 3);
	    bio_write_int(bs, marker_bit, 1);
	    PTS_temp = (PTS >>15)&0x7fff;
	    bio_write_int(bs, PTS_temp, 15);
	    bio_write_int(bs, marker_bit, 1);
	    PTS_temp = PTS &0x7fff;
	    bio_write_int(bs, PTS_temp, 15);
	    bio_write_int(bs, marker_bit, 1);
	    num_bytes_used+=5 ;
	    bio_write_int(bs, reserved2, 4);
	    DTS_temp = (DTS >>30)&0x7;
	    bio_write_int(bs, DTS_temp, 3);
	    bio_write_int(bs, marker_bit, 1);
	    DTS_temp = (DTS >>15)&0x7fff;
	    bio_write_int(bs, DTS_temp, 15);
	    bio_write_int(bs, marker_bit, 1);
	    DTS_temp = DTS &0x7fff;
	    bio_write_int(bs, DTS_temp, 15);
	    bio_write_int(bs, marker_bit, 1);
	    num_bytes_used+=5 ;
	}//TS_pkts->pes.pf->PTS_DTS_flags
#ifdef ACC_UNI_DEL
	
	bio_write_int(bs, 0x00, 8);
	bio_write_int(bs, 0x00, 8);
	bio_write_int(bs, 0x00, 8);
	bio_write_int(bs, 0x01, 8);
	bio_write_int(bs, 0x09, 8);
	bio_write_int(bs, 0xF0, 8);
	num_bytes_used+=6;
	//ind_AU->AU_size_written+=6;
#endif
	if (video->Is_sps_dumped == 0) {
	    //only for first AU and first TS pkt of first AU
	    uint8_t *temp_dest;
	    temp_dest = (uint8_t *)(bs->data+bs->pos);
	    //writting sps data
	    memcpy(temp_dest, video->sps_data, video->sps_size);
	    bs->pos+=video->sps_size;
	    num_bytes_used = num_bytes_used + video->sps_size;
	    video->Is_sps_dumped = 1;
	}
    }
    /*if the AU has only one pkt in it (first and last ppkt are same) */
    if((ind_AU->Is_first_last_TS_pkt == 1)) {
	uint8_t *temp_dest;
	uint32_t temp_data = total_num_bytes-num_bytes_used;
	temp_dest = (uint8_t *)(bs->data+bs->pos);
	memcpy(temp_dest,data,temp_data);
	bs->pos+=temp_data;
	ind_AU->AU_size_written+=temp_data;
	data+=temp_data;
	ind_AU->AU_bytes_written_per_TS+=temp_data;
	ind_AU->Is_first_TS_pkt = 0;
    }
    /* if the current ts pkt is the first pkt or the intermeadiate pkt */
    if((ind_AU->Is_first_TS_pkt == 1)||(ind_AU->Is_intermediate_TS_pkt
					== 1)) {
	uint8_t *temp_dest;
	uint32_t temp_data = total_num_bytes-num_bytes_used;
	temp_dest = (uint8_t *)(bs->data+bs->pos);
	memcpy(temp_dest, data, temp_data);
	bs->pos += temp_data;
	ind_AU->AU_size_written += temp_data;
	data += temp_data;
	ind_AU->AU_bytes_written_per_TS += temp_data;
	video->ind_AU->Is_first_TS_pkt = 0;
    }
    /* if the current TS pkt is the last pkt */
    if((ind_AU->Is_last_TS_pkt == 1)) {
	for (i=0;i<ind_AU->stuffing_bytes;i++) {
	    *(bs->data+bs->pos) = dump;
	    bs->pos+=1;
	    num_bytes_used++;
	}
	uint8_t *temp_dest;
	uint32_t temp_data = total_num_bytes-num_bytes_used;
	temp_dest = (uint8_t *)(bs->data+bs->pos);
	memcpy(temp_dest,data,temp_data);
	bs->pos+=temp_data;
	ind_AU->AU_size_written+=temp_data;
	data+=temp_data;
	ind_AU->AU_bytes_written_per_TS+=temp_data;
    }
    bio_close_bitstream(bs);
    return VPE_SUCCESS;
}

/* 
This function is used to initialize the ind_AU structure elements
*/
static inline int32_t 
mfu_initialize_ind_AU(mfu_AU_t *ind_AU)
{
    ind_AU->AU_size =0;
    ind_AU->AU_size_written =0;
    ind_AU->AU_size_tobe_written =0;
    ind_AU->AU_time_stamp = 0;
    ind_AU->num_TS_packets_per_AU =0;
    ind_AU->num_TS_packets_per_AU_written = 0;
    ind_AU->num_stuff_bytes_per_AU =0;
    ind_AU->Is_first_TS_pkt = 1;
    ind_AU->Is_last_TS_pkt = 0;
    ind_AU->Is_first_last_TS_pkt = 0;
    ind_AU->payload_unit_start_indicator = 1;
    ind_AU->adaptation_field_control = 3;
    ind_AU->adaptation_field_len = ADAPTATION_FIELD_SIZE_WITH_PCR;
    ind_AU->PCR_flag=0;
    return VPE_SUCCESS;
}

/*
This fucntion is used to number of TS pkts tobe formed for each AU
based on AU size 
*/
static inline uint32_t 
mfu_find_num_TS_pkt_per_AU(uint32_t AU_size)
{
    int32_t avail = AU_size;
    uint32_t num_ts_pkts=0;
    int32_t first_pkt_size=0;
    //while turning off the pcr flag, ADAPTATION_FIELD_SIZE_WITH_PCR
    //is replaced with ADAPTATION_FIELD_SIZE_WITHOUT_PCR
    first_pkt_size = (TS_PKT_SIZE - (TS_HEADER_SIZE + 1 +
				     ADAPTATION_FIELD_SIZE_WITH_PCR + PES_HEADER_SIZE));
    if(num_ts_pkts ==0) {
	if(avail <= first_pkt_size){
	    num_ts_pkts++;
	    return num_ts_pkts;
	} else {
	    avail = avail - first_pkt_size;
	    num_ts_pkts++;
	}
    }
    if(num_ts_pkts !=0) {
	while(avail > 0)
	    {
		num_ts_pkts++;
		avail = avail - 182;
	    }
    }
    return num_ts_pkts;
}
/*
This function initializes the individual TSpkt with TS header,
adaptation feild, PES header 
*/
static inline int32_t 
mfu_initialize_ind_AU_TS_pkts(pld_pkt_t *TS_pkts, mfu_videots_desc_t
			      *video)
{
    //initialize TS header
    TS_pkts->hdr.sync_byte = 0x47;//8 bits
    TS_pkts->hdr.transport_error_indicator = 0;// 1 bit
    TS_pkts->hdr.payload_unit_start_indicator = 0;// 1 bit
    TS_pkts->hdr.transport_priority = 0;// 1 bit
    TS_pkts->hdr.PID = VID_PID;//video->PID;//13 bits
    TS_pkts->hdr.transport_scrambling_control = 0;//2 bits
    TS_pkts->hdr.adaptation_field_control = 0x1;//2 bits
    //TS_pkts->hdr.continuity_counter = 0;;//4 bits
    //initialise adaptation feild
    TS_pkts->hdr.af->adaptation_field_len =
	ADAPTATION_FIELD_SIZE_WITH_PCR;//8 bits
    TS_pkts->hdr.af->discontinuity_indicator = 0;//1 bit
    TS_pkts->hdr.af->random_access_indicator = 0;//1 bit
    TS_pkts->hdr.af->elementary_stream_priority_indicator = 0;//1 bit
    TS_pkts->hdr.af->PCR_flag = 0;//1 bit
    TS_pkts->hdr.af->OPCR_flag = 0;//1 bit1
    TS_pkts->hdr.af->splicing_point_flag = 0;//1 bit
    TS_pkts->hdr.af->transport_private_data_flag = 0;//1 bit
    TS_pkts->hdr.af->adaptation_field_extension_flag = 0;//1 bit
    TS_pkts->hdr.af->pcr.program_clock_reference_base = 0;//33 bit
    TS_pkts->hdr.af->pcr.reserved = 0;//6 bit
    TS_pkts->hdr.af->pcr.program_clock_reference_extension = 0;//9 bit
    //initialize the PES header

    TS_pkts->pes.packet_start_code_prefix = 0x1;//24 bits
    TS_pkts->pes.stream_id = (uint8_t)STREAM_ID;// video->stream_id;//8 bits
    TS_pkts->pes.PES_packet_length = 0;//16 bits
    //include fixed bits"10"//2 bits
    TS_pkts->pes.pf->PES_scrambling_control = 0;//2 bits
    TS_pkts->pes.pf->PES_priority = 0;//1 bit
    TS_pkts->pes.pf->data_alignment_indicator = 1;//1 bit
    TS_pkts->pes.pf->copyright = 0;//1 bit
    TS_pkts->pes.pf->original_or_copy = 0;//1 bit
    TS_pkts->pes.pf->PTS_DTS_flags = 0x3;//2 bits
    TS_pkts->pes.pf->ESCR_flag = 0;//1 bit
    TS_pkts->pes.pf->ES_rate_flag = 0;// 1 bit
    TS_pkts->pes.pf->DSM_trick_mode_flag = 0;//1 bit
    TS_pkts->pes.pf->additional_copy_info_flag = 0;//1 bit
    TS_pkts->pes.pf->PES_CRC_flag = 0;//1 bit
    TS_pkts->pes.pf->PES_extension_flag = 0;//1 bit
    TS_pkts->pes.pf->PES_header_data_length = 10;//8 bit
    TS_pkts->pes.pf->PTS = 0;//40 bits
    TS_pkts->pes.pf->DTS =0;//40 bits
    return VPE_SUCCESS;
}

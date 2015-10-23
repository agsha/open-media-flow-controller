#ifndef _NKN_VPE_TS_VIDEO__
#define _NKN_VPE_TS_VIDEO__

#include "nkn_vpe_types.h"
#include "nkn_vpe_mfp_ts.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_ismv_mfu.h"

typedef struct mfu_AU{
    uint32_t AU_size;// AU size
    uint32_t AU_temp_size;//original AU size + SPS data+additional bytes if necessary
    uint32_t AU_size_written;// AU size written in single TS packet
    uint32_t AU_sps_size_written;
    uint32_t AU_bytes_written_per_TS;
    uint32_t AU_size_tobe_written;//AU size to be written into single TS packet
    uint64_t  AU_time_stamp; // DTS
    int64_t  AU_comp_time_stamp; //composition time
    uint32_t num_TS_packets_per_AU;//no:of: TS packets per AU
    uint32_t num_TS_packets_per_AU_written;//no:of: TS packets per AU
    uint32_t num_stuff_bytes_per_AU;//no:of stuffing bytes per AU
    //uint32_t Is_adaptation_field;//flag to indicate whether adaptation flag is written in TS
    uint32_t Is_last_TS_pkt;// whether TS pkt is last Ts pkt for single AU
    uint32_t Is_first_TS_pkt;//whether TS pkt is first Ts pkt for single AU
    uint32_t Is_first_last_TS_pkt;
    uint32_t Is_intermediate_TS_pkt;
    uint32_t Is_stuf_pkt;
    uint32_t stuffing_bytes;
    uint8_t  payload_unit_start_indicator;
    uint8_t adaptation_field_control;
    uint32_t adaptation_field_len;
    uint8_t PCR_flag;
    pld_pkt_t* TS_pkts;//pointer to TS pkt descriptor
    //uint8_t* TS_pkts;
}mfu_AU_t;

typedef struct mfu_videots_desc{
    uint32_t num_AUs;//no:of:AUs 
    uint32_t num_TS_pkts;//total no:of :TS pkts fro given number of AU's
    uint8_t continuity_counter;
    //uint32_t num_entry;//no:of:entries
    //uint32_t entry_duration;//duration of each entry
    uint64_t timescale;//timescale of the file
    uint8_t * sps_data;//pointer for sps and pps data
    uint32_t sps_size;//sps and pps data size
    uint8_t Is_sps_dumped;//whethr sps and pps data has been dumped already or not
    uint32_t stream_id;//stream id 
    uint16_t PID;
    uint64_t base_time;
    double time_of_day;
    double frame_rate;
    double frame_time;
    mfu_AU_t * ind_AU;//pointer for individual AU descriptor
}mfu_videots_desc_t;


int32_t mfu_get_ts_h264_pkts(mdat_desc_t* ,  ts_pkt_list_t*, mfu_videots_desc_t *,int32_t ,int32_t);
//int32_t mfu_initialize_ind_AU(mfu_AU_t *);
//uint32_t mfu_find_num_TS_pkt_per_AU(uint32_t );
//static int32_t mfu_packetize_AU(uint8_t* , /* bitstream_state_t **/ts_pkt_list_t* ,mfu_videots_desc_t *);

//int32_t mfu_initialize_ind_AU_TS_pkts(pld_pkt_t *,mfu_videots_desc_t *);
//int32_t initialize_ind_AU_TS_pkts(pld_pkt_t *);
//uint32_t find_AUs_entry(imfu_data_t* ,uint32_t);
//int32_t parse_mfu_header(imfu_data_t *, videots_desc_t *);
//int32_t mfu_packetize_TS_packet(uint8_t *, /*bitstream_state_t **/ts_pkt_list_t*  , mfu_AU_t *,pld_pkt_t* ,uint32_t,mfu_videots_desc_t *);


#endif

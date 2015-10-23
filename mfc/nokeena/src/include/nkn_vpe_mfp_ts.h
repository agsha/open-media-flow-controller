#ifndef _NKN_VPE_MFP_TS__
#define _NKN_VPE_MFP_TS__

#include <stdio.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_bitio.h"
#include "mfp_safe_io.h"
#include "mfp_limits.h"

#ifdef __cplusplus
extern "C" {
#endif


/*Specific Error codes*/
#define VPE_MEM_ERR 2


#define VPE_PAT_PID 0
#define VPE_PMT_PID 256
#define VPE_VID_PID 257
#define VPE_AUD_PID 258
#define ADAF_STUFF_MAX 150
#define POW2_33 0x200000000
#define STUFF_BYTES_MAX 165 //180-AAC_PES_SIZE-1

/*Table 2-29 MPEG2TS*/
#define TS_H264_ST 0x1B 
#define TS_AAC_ADTS_ST 0x0F

#define TS_PKT_SIZE 188

#define AAC_PES_SIZE 14 //PES size for AAC is 14 bytes w/ only PTS
#define AAC_ADAF 2  // adaptation field is 2 bytes for ADTS/AAc.
#define PES_HR_LEN 5 // pes header len - considers only PTS pressent

#define MFU_MIN_FRAME_RATE 8
/*Structures for MPEG2TS*/

typedef struct pcr{
    uint64_t program_clock_reference_base;
    uint8_t reserved;
    uint16_t program_clock_reference_extension;
}pcr_t;

/*adaptation Fields - ONLY those defined by APPLE TS -reverse engineered*/
typedef struct afld{
    uint32_t adaptation_field_len; // Assume it is always > 0
    uint8_t discontinuity_indicator;
    uint8_t random_access_indicator;
    uint8_t elementary_stream_priority_indicator;
    uint8_t PCR_flag;
    uint8_t OPCR_flag;
    uint8_t splicing_point_flag;
    uint8_t transport_private_data_flag;
    uint8_t adaptation_field_extension_flag;
    pcr_t pcr;
    pcr_t opcr;
}adapt_field_t;

typedef struct tspkt{
    uint8_t sync_byte;
    uint8_t transport_error_indicator;
    uint8_t payload_unit_start_indicator;
    uint8_t transport_priority;
    uint16_t PID;
    uint8_t transport_scrambling_control;
    uint8_t adaptation_field_control;
    uint8_t continuity_counter;
    adapt_field_t *af;
}ts_pkt_t;

typedef struct pesfield{
    uint32_t packet_stat_code_prefix;
    uint8_t stream_id;
    uint16_t PES_packet_length;
    uint8_t PES_scrambling_control;
    uint8_t PES_priority;
    uint8_t data_alignment_indicator;
    uint8_t copyright;
    uint8_t original_or_copy;
    uint8_t PTS_DTS_flags;
    uint8_t ESCR_flag;
    uint8_t ES_rate_flag;
    uint8_t DSM_trick_mode_flag;
    uint8_t additional_copy_info_flag;
    uint8_t PES_CRC_flag;
    uint8_t PES_extension_flag;
    uint8_t PES_header_data_length;
    uint64_t PTS;
    uint64_t DTS;
}pes_fields_t;

typedef struct pes{
    int32_t packet_start_code_prefix;
    uint8_t stream_id;
    uint16_t PES_packet_length;
    pes_fields_t *pf;
}pes_t;

typedef struct pas{
    uint8_t table_id;
    uint8_t section_syntax_indicator;
    uint16_t section_length;
    uint16_t transport_stream_id;
    uint8_t version_number;
    uint8_t current_next_indicator;
    uint8_t section_number;
    uint8_t last_section_number;
}pas_t;


typedef struct pat{
    ts_pkt_t pat_hr;
    pas_t pas;
    uint16_t program_map_PID; //Always set to 256: which is PMTs id
}pat_t;



typedef struct pkt{
    ts_pkt_t hdr;
    pes_t pes;
}pld_pkt_t;


typedef struct sec{
    uint8_t  stream_type;// 8 uimsbf
    uint8_t stream_id; //table 2-18 of MPEG2 standard
    uint16_t elementary_PID;// 13 uimsbf
    uint16_t ES_info_length;
    uint8_t header_af[4];
    uint8_t header_no_af[4];
    uint8_t cc;
    uint64_t base_time;
    double frame_time;
    double frame_rate;
    uint32_t Is_stream_dumped;
    char* first_file_name;
    FILE* fout;//dump the ts pkts for first time and use it later
    FILE* fin;
    uint32_t aud_pos;
    uint32_t vid_pos;
    uint32_t aud_size;
    uint32_t vid_size;
    uint32_t aud_temp_size;
    uint32_t vid_temp_size;

}stream_t;

typedef struct pmtr{
    uint16_t program_number;
    uint16_t PCR_PID;
    uint16_t program_info_length;
    uint8_t num_streams;
    stream_t *streams; //will contain the stream data for num_streams.
}pmt_rest_t;


typedef struct pmt{
    ts_pkt_t hr;
    pas_t pas;  //overlap sections with pas sections.
    pmt_rest_t pmt;
}pmt_t;


typedef struct dd{
    uint8_t* data;// pointer to the entire inline MFU structure
    uint32_t data_size;// total iMFU size. 
}imfu_data_t;

typedef struct tspktl{
    uint8_t* data ;// pointer to the entire TS packets
    uint32_t num_entries;//  - total number of entries
    uint32_t *entry_num_pkts;// .number of packets per entry
}ts_pkt_list_t;


typedef struct unitdesc{
    uint32_t sample_count; //number of samples
    uint32_t total_sample_size;
    uint32_t default_sample_duration; // 0 if smpl durations are not the same
    uint32_t default_sample_size;// 0 if sample sizes are not the same for all samples
    uint64_t timescale;
    uint32_t* sample_duration; // NULL if all sampels of same durations
    uint32_t* composition_duration;
    uint32_t* sample_sizes; // NULL if all samples of same size.
    uint8_t* mdat_pos; //Pointer to all sample data
    uint8_t** dat_pos; //pointer to each sample.
}unit_desc_t;

typedef struct aacdesc{
    uint8_t stream_id;
    uint16_t cc;
}aac_desc_t;

//#define TS_FILE
/*the universal structure*/
typedef struct tsds{
    uint32_t pat_size;
    uint32_t pmt_size;
    uint8_t* pat_data;
    uint8_t* pmt_data;
    uint16_t pcr_pid; //pid holding pcr
    uint8_t num_streams; // number of streams
    uint8_t num_aud_streams;
    uint8_t num_vid_streams;
    stream_t stream_aud[16]; // Each stream's info
    stream_t stream_vid[16]; // Each stream's info
    stream_t* st;
    pat_t *pat;
    pmt_t* pmt;
    uint32_t chunk_num;
    uint64_t base_vid_time; //Terrible HACK - change it once it works
    double time_of_day;
    double frame_rate;
    double frame_time;
    uint32_t chunk_time;
#ifdef TS_FILE
    FILE* fout;
#endif
    char* out_name;
    uint32_t Is_audio;
    char *audio_file_name;
    uint8_t continuity_counter;
    uint8_t *aud_mdat_pos_add;//it will maintain the alloced address
			      //for audio mdat
    uint8_t *aud_new_mdat_buffer;
    size_t chunk_size;
    uint32_t avg_bitrate;
    uint32_t max_bitrate;
    float standard_deviation;
    int32_t inst_deviation;
    float sum_of_deviation_square;
    uint32_t num_samples;
    
}ts_desc_t;




/*Function declaration*/


//int32_t mfu_form_pat_pkt(pat_t*);
//int32_t mfu_form_pmt_pkt(pmt_t*,ts_desc_t*);
int32_t mfu2ts_init(ts_desc_t*);
int32_t mfu_clean_ts(ts_desc_t*);
int32_t mfu2_ts(ts_desc_t *,unit_desc_t*, unit_desc_t*, uint8_t *,
		uint32_t, char*, size_t, uint32_t, const mfp_safe_io_ctx_t *, bitstream_state_t **);
int32_t mfu2_ts_multi_bit_rate(ts_desc_t *,unit_desc_t*, unit_desc_t*, uint8_t *, uint32_t,uint32_t,uint32_t);
//uint32_t mfu_write_pat(uint8_t* ,pat_t*);
//void mfu_write_ts_hdr(ts_pkt_t ,uint8_t*);
//uint32_t mfu_get_unit_num(unit_desc_t*, uint32_t,uint32_t);
//int32_t mfu_get_time_sync_info(uint32_t,uint32_t, uint32_t*, uint32_t *);
//int32_t mfu_get_aac(unit_desc_t* , uint32_t, uint32_t,ts_pkt_list_t*,stream_t*);
//void mfu_form_ts_hdr(uint16_t, stream_t*);
//void mfu_form_aud_pes_hdr(pes_fields_t *, uint8_t*);

#ifdef __cplusplus
}
#endif


#endif

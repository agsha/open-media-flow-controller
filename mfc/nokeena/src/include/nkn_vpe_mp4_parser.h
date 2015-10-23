/*
 *
 * Filename:  nkn_vpe_mp4_parser.c
 * Date:      2009/03/28
 * Module:    Basic MP4 Parser
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MP4_PARSER_
#define _MP4_PARSER_
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

//#include "nkn_vpe_backend_io_plugin.h"
#define MAX_TRACKS 20
#define MAX_PROFILES MAX_TRACKS

#ifdef __cplusplus
extern "C"{
#endif

#pragma pack(push, 1)

    /*basic data types */

    /*BOX - is the basic building block of a MP4 file */
    typedef struct tag_box{
	char type[4];
	uint32_t short_size;
	uint64_t long_size;
    }box_t;

    /*BOX - reader */
    typedef struct tag_box_list {
	int32_t type;
	void*(*box_reader)(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
	void*(*box_cleanup)(void *);
    }box_reader;

    /*mdhd - box */
    typedef struct tag_mdhd
    {
	unsigned int version;
	unsigned int flags;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t timescale;
	uint64_t duration;
	unsigned int language[3];
	uint16_t predefined;
    }mdhd_t;

    /*mvhd - box */
    typedef struct tag_mvhd
    {
	unsigned int version;
	unsigned int flags;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t timescale;
	uint64_t duration;
	int32_t rate;
	int16_t volume;
	int16_t reserved;
	uint32_t reserved1[2];
	int32_t matix[9];
	uint32_t predefined[6];
	uint32_t next_track_id;
    }mvhd_t;

    /*tfhd box */
    typedef struct tag_tfhd{
	uint32_t version;
	uint32_t flags;
	uint32_t track_id;
	uint64_t base_data_offset;
	uint32_t sample_desc_index;
	uint32_t def_sample_duration;
	uint32_t def_sample_size;
	uint32_t def_sample_flags;
    }tfhd_t;
       
    /*trun box sample table*/
    typedef struct tag_trun_stable{
	uint32_t sample_duration;
	uint32_t sample_size;
	uint32_t sample_flags;
	uint32_t scto;
    }trun_stable_t;

    /*trun box */
    typedef struct tag_trun{
	uint32_t version;
	uint32_t flags;
	uint32_t sample_count;
	int32_t data_offset;
	uint32_t first_sample_flags;
	trun_stable_t *tr_stbl;
    }trun_t;

    /*tkhd box */
    typedef struct tag_tkhd{
	uint32_t version;
	uint32_t flags;

	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t track_id;
	uint32_t reserved ;
	uint64_t duration;

	uint32_t reserved1[2];
	int16_t layer;
	int16_t alternate_group;
	int16_t volume;
	uint16_t reserved2;
	int32_t matrix[9];

	uint32_t width;
	uint32_t height;
    }tkhd_t;

    /*hdlr box*/
    typedef struct tag_hdlr{
	uint32_t pre_defined;
	uint32_t handler_type;
	uint32_t reserved[3];
	char *name;
    }hdlr_t;
    
    /*******************************************************************
     *                 SAMPLE TABLE BOXES
     ******************************************************************/
    /*STSZ box */
    typedef struct tag_stsz{
	uint32_t sample_size;
	uint32_t sample_count;
	uint32_t *entry_size;
    }stsz_t;

    typedef struct tag_chunk_info{
	uint32_t first_chunk;
	uint32_t samples_per_chunk;
	uint32_t sample_description_index;
    }chunk_info_t;

    typedef struct tag_stsc{
	uint32_t entry_count;
	chunk_info_t *info;
    }stsc_t;

    typedef struct tag_stco{
	uint32_t entry_count;
	uint32_t *chunk_offset;
    }stco_t;

    typedef struct tag_co64{
	uint32_t entry_count;
	uint64_t* chunk_offset;
    }co64_t;

    /*STSD Box */
    typedef struct tag_stsd{
	uint32_t entry_count;
	uint8_t  *p_sample_entry;
	size_t   sample_entry_size;
    }stsd_t;

    /*RTP Packet */
    typedef struct tag_RTP_pkt{
	int32_t relative_time;
	uint16_t rtp_flags1;

	uint16_t RTPSequenceseed;
	uint16_t rtp_flags2;
	uint16_t entrycount;
    }RTP_pkt_t;

    /*RTP Sample */
    typedef struct tag_RTP_sample{
	uint16_t packetcount;
	uint16_t reserved;
	RTP_pkt_t *rtp_pkts;
	uint8_t extradata;
    }RTP_sample_t;

    typedef struct tag_box_list_reader {
	int32_t n_readers;
	box_reader *blr;
    }box_list_reader;

    /*BOX CONTAINERS - TRAK, MDIA etc
      Note:- Containers here are essentially from one root node to another. This can be further grouped
      to get the entire box structure/layout. For e.g. MDIA will have mdhd, hdlr, minf but will not contain
      the STBL or be contained in TRAK*/
    /* MDIA Container*/
    typedef struct tag_mdia {
	uint8_t *data;
	size_t pos;
	size_t size;

	mdhd_t *mdhd;
	size_t mdhd_pos;
	size_t mdhd_size;
	uint8_t *mdhd_data;

	hdlr_t *hdlr;
	size_t hdlr_pos;
	size_t hdlr_size;
	uint8_t* hdlr_data;
    }mdia_t;
    typedef struct run_table_ctxt{
    	int32_t prev_sample_num;
    	uint32_t prev_offset;
    	uint32_t current_totsam;
	uint32_t entry_count;
	uint64_t current_accum_value;
    }runt_ctxt_t;

    /*SAMPLE TABLE Container */
    typedef struct tag_stbl{
	uint8_t *data;
	size_t size;
	size_t pos;

	uint8_t *stsc_data;
	runt_ctxt_t stsc_ctxt;

	uint8_t *stco_data;
        uint8_t *co64_data;
	
	uint8_t *stts_data;
	runt_ctxt_t stts_ctxt;

	uint8_t *stss_data;
	
	uint8_t *ctts_data;
	runt_ctxt_t ctts_ctxt;

	uint8_t *stsz_data;
	uint8_t *stsd_data;
    }stbl_t;

    /* TRAK Container*/
    typedef struct tag_trak{
	uint8_t *data;
	size_t size;
	size_t pos;
	
	tkhd_t *tkhd;
	size_t tkhd_pos;
	size_t tkhd_size;
	uint8_t *tkhd_data;

	uint8_t *minf_data;
	size_t minf_size;
	size_t minf_pos;

	uint8_t *dinf_data;
	size_t dinf_size;
	size_t dinf_pos;	

	uint8_t *edts_data;
	size_t edts_size;
	size_t edts_pos;

	mdia_t *mdia;
	
	stbl_t *stbl;
    }trak_t;

    typedef struct tag_udta {
	size_t pos;
	size_t size;
	uint8_t *data;

	/* youtube seek support */
	size_t ilst_pos;
	size_t ilst_size;
	uint8_t *ilst_data;
    }udta_t;
	
    /* MOOV CONTAINER */
    typedef struct tag_moov{
	uint8_t *data;
	size_t pos;
	size_t size;
	trak_t *trak[MAX_TRACKS];
	uint8_t trak_process_map[MAX_TRACKS];
	mvhd_t *mvhd;
	size_t mvhd_pos;
	size_t mvhd_size;
	uint8_t *mvhd_data;
	udta_t *udta;

	uint8_t* udta_mod;
	size_t udta_mod_size;
	size_t udta_mod_pos;
	uint32_t n_tracks;
	
	uint32_t n_tracks_to_process;
	double synced_seek_time;

    }moov_t;

    typedef struct tag_tfra_table {
	uint64_t moof_time;
	uint64_t moof_offset;
	uint32_t traf_number;
	uint32_t trun_number;
	uint32_t sample_number;
    }tfra_table_t;

    typedef struct tag_tfra {
	size_t pos;
	size_t size;
	uint8_t *data;
	uint8_t version;
	uint32_t track_id;
	uint32_t length_size_traf_num;
	uint32_t length_size_trun_num;
	uint32_t length_size_sample_num;
	uint32_t n_entry;
	tfra_table_t *tbl;
    }tfra_t;

    typedef struct tag_mfra {
	size_t pos;
	size_t size;
	uint8_t *data;
	tfra_t *tfra[MAX_TRACKS];
	uint32_t n_traks;
    }mfra_t;

    typedef struct tag_mp4_sample_entry {
	box_t box;
	uint8_t reserved1[6];
	uint16_t data_ref_index;
    } mp4_sample_entry_t;

    typedef struct tag_mp4_audio_sample_entry {
	mp4_sample_entry_t se;
	uint32_t reserved1[2];
	uint16_t channel_count;
	uint16_t sample_size;
	uint16_t pre_defined;
	uint16_t reserved2;
	uint32_t sample_rate;
    }mp4_audio_sample_entry_t;

    typedef struct tag_mp4_visual_sample_entry {
	mp4_sample_entry_t se;
	uint16_t pre_defined1; 
	uint16_t reserved1; 
	uint32_t pre_defined2[3];
	uint16_t width;
	uint16_t height;
	uint32_t horizontal_resolution;
	uint32_t vertical_resolution;
	uint32_t reserved2;
	uint16_t frame_count;
	uint8_t compressor_name[4];
	uint16_t depth;
	uint16_t pre_defined3;
    } mp4_visual_sample_entry_t;

#pragma pack(pop)

    /*API */

    /** Reads a root level box - A root level box could be one of the following - ftyp, moov, 
     * moof, pdin, mdat, free, skip boexs
     *@param void *desc[in] - input i/o descriptor
     *@param box_t *box[in/out] - the box is populated with the appropriate paramters
     *@param bytes_read - number of bytes parsed
     *@return - returns the box size of the root level box 
     */
    int32_t read_next_root_level_box(void *desc, size_t size, box_t *box, size_t* bytes_read);

    /*reads a child box
     *@param uint8_t *p_data[in] - pointer to the input mp4 stream pointing to the head of the child box
     *@param box_t *p_box[in/out] - returns the populated box
     *@return returns the child box size, <0 if error is encountered
     */ 
    int32_t read_next_child(uint8_t* p_data, box_t *p_box);

    /**writes the content of a box
     *@param void *desc[in] - input i/o descriptor
     *@param io_funcs *iof[in] - plugin for i/o handling
     *@param box_t *box[in] - the box is populated with the appropriate paramters
     *@param uint8_t* p_box_data - data contained in the box, apart from headers
     *@return - returns the box size of the root level box 
     */
    int32_t write_box(void *desc, void  *iof, box_t *p_box, uint8_t *p_box_data);

    /* tests a box for a type
     *@param box_t *box[in] - pointer to the box
     *@param const char *id[in] - FOUR chaaracter code for the box
     *@return returns 1 if true, 0 if false
     */
    int32_t check_box_type(box_t *box, const char *id);
    
    /*depracated function */
    int32_t box_type_moof(box_t *box);

    /* tests if a box is a root level box
     *@param box_T *box[in] - input box to test
     *@return returns 1 if the box is a root level box, 0 otherwise
     */
    int32_t is_container_box(box_t *box);

    /* Box Readers */
    void * mdhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * tfhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * trun_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * tkhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size); 
    void * hdlr_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * stsd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * stsz_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * stsc_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * stco_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    void * mvhd_reader(uint8_t *pdata, uint64_t size, uint32_t box_hdr_size);

    /*Box Specific Cleanup Functions */
    /** frees the stsz box
     *@param box - box to be freed
     *@return returns NULL if successful and the box pointer if unsuccesful
     */
    void *hdlr_cleanup(void *box);

    /** frees the stsz box
     *@param box - box to be freed
     *@return returns NULL if successful and the box pointer if unsuccesful
     */
    void *stsz_cleanup(void *box);

    /** frees the stco box
     *@param box - box to be freed
     *@return returns NULL if successful and the box pointer if unsuccesful
     */
    void *stco_cleanup(void *box);

    /** frees the stsc box
     *@param box - box to be freed
     *@return returns NULL if successful and the box pointer if unsuccesful
     */
    void *stsc_cleanup(void *box);

    /*generic cleanup function
     *@param box - box to be freed
     *@return returns NULL if successful and the box pointer if unsuccesful
     */
    void *box_cleanup(void *box);

    /* UTILITY READ FUNCTIONS TO READ VARIOUS MP4 SPECIFIC DATA TYPEs */
    int64_t read_signed_qword(uint8_t *p_buf);
    int32_t read_signed_dword(uint8_t *p_buf);
    uint16_t read_word(uint8_t *p_buf);
    uint8_t read_byte(uint8_t *p_buf);
    int32_t read_signed_24bit(uint8_t *p_buf);
    uint64_t read_unsigned_qword(uint8_t *p_buf);
    uint32_t read_unsigned_dword(uint8_t *p_buf);
    double get_sequence_duration(mdhd_t *mdhd);



#ifdef __cplusplus
}
#endif
#endif 

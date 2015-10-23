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

#include "nkn_vpe_backend_io_plugin.h"

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
    typedef struct tag_box_list_reader{
	char type[4];
	void*(*box_reader)(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);
    }box_list_reader;

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



    /*API */

    /** Reads a root level box - A root level box could be one of the following - ftyp, moov, 
     * moof, pdin, mdat, free, skip boexs
     *@param void *desc[in] - input i/o descriptor
     *@param io_funcs *iof[in] - plugin for i/o handling
     *@param box_t *box[in/out] - the box is populated with the appropriate paramters
     *@return - returns the box size of the root level box 
    */
    int32_t read_next_root_level_box(void *desc, io_funcs *iof, box_t *box);

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
    int32_t write_box(void *desc, io_funcs *iof, box_t *p_box, uint8_t *p_box_data);

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
    void* tkhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size);


    /* UTILITY READ FUNCTIONS TO READ VARIOUS MP4 SPECIFIC DATA TYPEs */
    int64_t read_signed_qword(uint8_t *p_buf);
    int32_t read_signed_dword(uint8_t *p_buf);
    uint16_t read_word(uint8_t *p_buf);
    uint8_t read_byte(uint8_t *p_buf);
    int32_t read_signed_24bit(uint8_t *p_buf);
    uint64_t read_unsigned_qword(uint8_t *p_buf);
    uint32_t read_unsigned_dword(uint8_t *p_buf);
    double get_sequence_duration(mdhd_t *mdhd);

#pragma pack(pop)


#ifdef __cplusplus
}
#endif
#endif 

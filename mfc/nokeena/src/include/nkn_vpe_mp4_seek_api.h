/*
 *
 * Filename:  nkn_vpe_mp4_seek_api.h
 * Date:      2010/01/01
 * Module:    mp4 seek API implementation
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MP4_SEEK_
#define _MP4_SEEK_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "nkn_vpe_types.h"

#include "nkn_vpe_mp4_parser.h"

#ifdef __cplusplus 
extern "C" {
#endif

#define SEEK_DETECT_BOX_LIST_MAX 10
#define SEEK_DETECT_MAX_LIST 10

#define E_VPE_MP4_DATA_INSUFF -1
#define E_VPE_MP4_DATA_INVALID -2

    typedef enum tag_mp4_seek_provider_type {
	SEEK_PTYPE_UNKNOWN,
	SEEK_PTYPE_YOUTUBE,
	SEEK_PTYPE_DAILYMOTION,
	SEEK_PTYPE_MAX
    } MP4_SEEK_PROVIDER_TYPE;

    typedef struct tag_mp4_seek_provider_detect {
	const uint32_t box_id_list[SEEK_DETECT_MAX_LIST][SEEK_DETECT_BOX_LIST_MAX];
	const float confidence[SEEK_DETECT_MAX_LIST];
	const uint32_t num_boxes_in_list[SEEK_DETECT_MAX_LIST];
	uint32_t num_lists;
    }mp4_seek_provider_detect;	

    typedef struct tag_dm_seek_input_t {
	size_t tot_file_size;
	size_t seek_mdat_size;
	size_t moov_offset;
	double duration;
    } dm_seek_input_t;

    /*
     * Struct used for
     * handle mp4 with moov box not present in first 32KB
     */
    typedef struct box_hd_data_span {
        uint8_t data_span_flag;
        size_t bytes_in_curr_span;
        size_t bytes_in_next_span;
    } nkn_vpe_mp4_data_span_t; /* MP4 data span*/

    typedef struct box_info_s {
        char* name;
        uint64_t offset;
	uint64_t size;
    } nkn_vpe_mp4_box_info_t;


    typedef struct box_hd_s {
        uint32_t box_size;
        char box_name[4];
        uint64_t box_large_size;
    } nkn_vpe_mp4_box_hd_t;

    typedef struct box_find_list_s {
        int32_t n_box_found;
        int32_t n_box_to_find;
        nkn_vpe_mp4_box_info_t* box_info;
    } nkn_vpe_mp4_box_find_list_t;

    typedef struct mp4_find_box_parser_s{
        void *iod;                 /* io read  */
        nkn_vpe_mp4_box_hd_t* box_hd;      /* box Header */
        nkn_vpe_mp4_data_span_t* ds;       /* data block */
        nkn_vpe_mp4_box_find_list_t*  pbox_list; /* box need to search */
        //int32_t bytes_to_next_box;
        //size_t parsed_bytes_in_block;
        size_t parsed_bytes_in_file;
    } nkn_vpe_mp4_find_box_parser_t;

    /***************************************** SEEK API *****************************************
     *
     *******************************************************************************************/
    /*
     * handle mp4 with moov box not present in first 32KB
     */
    nkn_vpe_mp4_find_box_parser_t* mp4_init_find_box_parser(void);

    int32_t mp4_set_find_box_name(nkn_vpe_mp4_find_box_parser_t* fp,
				  const char* find_box_name_list[],
				  size_t n_box_to_find);

    int32_t mp4_parse_init_databuf(nkn_vpe_mp4_find_box_parser_t* fp,
				   uint8_t *p_data, size_t size);

    int32_t mp4_parse_find_box_info(nkn_vpe_mp4_find_box_parser_t* fp,
				    uint8_t *p_data, size_t size,
				    off_t *brstart, off_t *brstop);

    int32_t mp4_find_box_parser_cleanup(nkn_vpe_mp4_find_box_parser_t* fp);


    /* read information about the moov box
     * @param p_data - should point to the begining of the MP4 file or a valid MP4 box
     * @param size - size of data in p_data
     * @param moov_offset [out] - offset from the begining of the buffer to start of moov box;
     *                            this value will be '0' if the moov box is not found in the buffer
     * @param moov_size [out] - the size of the moov box in the MP4 file
     * @return should be ignored
     */
    int32_t mp4_get_moov_info(uint8_t *p_data, size_t size, size_t *moov_offset, size_t *moov_size);

    /* initializes resources for build a moov box
     * @param p_data - should point to the begining of the moov box
     * @param off - offset of the moov box in the MP4 file (needed for sample table re-adjustments)
     * @param size - size of the moov box
     * @return Returns NULL if unable to create the context
     */
    moov_t *mp4_init_moov(uint8_t *p_data, size_t off, size_t size);

    /* builds the trak information found in the moov box; maximum supported trak's are 4 and the seek
     * parser will modify traks with descriptor 'vide' and 'soun'
     * @param - the moov context returned in the previous mp4_moov_init call
     * @return - returns the number of traks found in the file
     */
    int32_t mp4_populate_trak(moov_t *moov);

    /* converts a seek time offset to a byte offset in the file. also rebuilds all the sample tables
     * for the new seek'ed file
     * @param moov - the parser context stored as moov box
     * @param trak_no - the number of traks in the file; also returned in the previous call to 
     *                  'mp4_populate_trak'
     * @param seek_to - the time offset to seek to
     * @param moov_out [out] - the new moov context with the rebuilt sample tables
     * @param vpe_ol_t - box information (size, offset) used to handle moov at EOF or inserted free box
     *                   current implementation is box info of moov, mdat and ftyp
     * @param n_box_to_find - the number of boxes used to handle moov at EOF, currently it is 3
     * @return - returns the byte offset in the MP4 file for the give time offset. returns '0' on error
     */
    size_t mp4_seek_trak(moov_t *moov, int32_t trak_no, float seek_to, moov_t **moov_out,
			 vpe_ol_t* ol, int n_box_to_find);


    /* YouTube specific API for modifying the udta box with the new time stamp
     * @param moov - the parser context on which the new time stamp needs to be reflected; should be
                     'moov_out' context returned in the previous call.
     * @return ignore the return value
     */		     
    int32_t mp4_yt_modify_udta(moov_t *moov);

    /* Seek specific API for retrieving the buffers of the modified udta box
     * @param moov - the parser context; should be 'moov_out' context returned in the previous call.
     * @param **p_udta - pointer to the new udta buffer
     * @param *new_udta_size - size of the new udta box
     * @return - ignore the return value
     */
    int32_t mp4_get_modified_udta(moov_t *moov, uint8_t **p_new_udta, size_t *new_udta_size);

    /* Seek specific API which marks the existing moov box as free and modifies the moov size 
     * to reflect the new udta box size
     * @param moov - the parser context; should be 'moov_out' context returned in the previous call
     * @param appended_tag_size - size of the udta tag to be appended at the end of the current moov box
     * return - ignore the return value
     */
    int32_t mp4_modify_moov(moov_t *moov, size_t appended_tag_size);
    
    /* cleanup the moov resources */
    void mp4_moov_cleanup(moov_t *moov, int32_t num_traks);
    void mp4_moov_out_cleanup(moov_t *moov_out, int32_t num_traks);

    /* cleanup the output udta box once the calling routines are done
     * with their work
     */
    void mp4_yt_cleanup_udta_copy(moov_t *moov);

    /**
     * Modifies the ilst box per DailyMotion's requirements
     * Dailymotion requires the seek provider to add the following ilst
     * tags to the existing tag list; note these tags maybe absent in the
     * original and need to be added afresh. if they are present in the
     * original we will retain the ctoo tag and replace the rest of the
     * tags with the tag list shown below.
     * a. pdur - total duration of the video as text
     * b. coff - the seek offset of the video as text
     * c. pbyt - total size of the video as text
     * d. cbyt - the seeked size of the video as text
     * @param moov - the moov structure which contains the asset's
     * metadata
     * @param seek_provider_specific_info - the provider specific
     * information to construct and populate provider specific tags
     * @return returns 0 on succes and a non - zero negative error
     * code on error
    */
    int32_t mp4_dm_modify_udta(moov_t *moov,
			       void *seek_provider_specific_info);

    /* tests a moov box to find if the moov box has any signature that
     * matches any known seek provider type supported in
     * MFC. Currently supported ones include
     * a. YouTube
     * b. DailyMotion
     * @param moov - the moov structure that contains the asset's
     * metadata
     * @param ptype - the provider type whose signature needs to be checked
     * @param result [out] - a boolean answer whether the given moov
     * box matches the 'ptype' signature
     * @return returns 0 on succes and a non - zero negative error
     * code on error
     * */
    int32_t mp4_test_known_seek_provider(moov_t *moov, 
					 MP4_SEEK_PROVIDER_TYPE ptype, 
					 uint8_t *result);

    /* NOT FULLY IMPLEMENTED; DO NOT USE */
    int32_t mp4_guess_seek_provider(moov_t *moov, 
				    MP4_SEEK_PROVIDER_TYPE *ptype);



    /****************************** I/O HANDLING FUNCTIONS ****************************************
     *
     *********************************************************************************************/

    size_t nkn_vpe_memread(void *buf, size_t n, size_t size, void *desc);
    void * nkn_vpe_memopen(char *p_data, const char *mode, size_t size);
    size_t nkn_vpe_memtell(void *desc);
    size_t nkn_vpe_memseek(void *desc, size_t seekto, size_t whence);
    void nkn_vpe_memclose(void *desc);
    size_t nkn_vpe_memwrite(void *buf, size_t n, size_t size, void *desc);

    /***************************HELPER FUNCTIONS**************************************************
     *
     ********************************************************************************************/
    void mp4_cleanup_mdia(mdia_t *mdia);
    void mp4_trak_cleanup(trak_t *trak);


    /*
     * BZ: 8382
     * mp4box to youtube mp4 (udta box injection)
     *
     */
    int32_t mp4_yt_create_udta(moov_t *moov);
    int32_t mp4_yt_update_moov_size(moov_t *moov);


#ifdef __cplusplus
}
#endif

#endif //HEADER PROTECTION

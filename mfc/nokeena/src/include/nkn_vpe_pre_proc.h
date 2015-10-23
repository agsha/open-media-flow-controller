/*
 *
 * Filename:  nkn_vpe_preproc.h
 * Date:      2009/02/06
 * Module:    FLV pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef __FLV_PREPROC_UTILS__
#define __FLV_PREPROC_UTILS__

#include <stdio.h>
#include "glib.h"

#include "nkn_vpe_types.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_codec_plugin.h"
#include "nkn_vpe_common.h"
#include "nkn_defs.h"

#ifdef __cplusplus
extern "C"{
#endif

    //#define UNIT_TEST

#define TPLAY_ENABLE

#ifdef UNIT_TEST
#define MAIN ut_main
    int ut_main(int, char**);
#else
#define MAIN main
    int ut_main(int, char**);
#endif

    typedef struct _tag_profile_order{
	int bitrate;
	int profile_id;
    }profile_order;

    typedef struct _tag_packet_info{
	uint64_t byte_offset;
	uint64_t time_offset;
	uint64_t size;
    }packet_info;

    /** Wrapper function that calls the codec handlers
     */
    int VPE_PP_chunk_flv_data(void *, char, uint32_t, meta_data *, char *, io_funcs *, codec_handler *,char, void **);
    int VPE_PP_chunk_MP4_data(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iod, codec_handler *cd, char thint, void **);
    int VPE_PP_chunk_TS_data(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, codec_handler *cd, char thint, void **private_data);
    
    /* UTILITY FUNCTIONS */
    /*Print Usage
     */
    void print_usage(void);

     /** Validates the FLV header
     *@param fh [in] - object of the type FLV header
     */
    char check_flv_header(nkn_vpe_flv_header_t *fh);

    /** check free space before writing data
     *@param path [in] - path to check
     *@param size [in] - amount of space required
     *@return returns '1' if requested free space is available, '0' if unavailable
     */ 
    char check_free_space(const char *, uint64_t);

    /** retrieves the underlying codecs (audio and video) in a valid FLV stream
     *@param in_desc [in] - I/O descriptor
     *@param iof [in] - I/O hanlders
     */
    unsigned char get_codec_info(void *in_desc, io_funcs *iof);

    /** writes server side meta data [See NKN_VPE_MetaDataSpecification.doc in Wiki for more information 
     *@param in_desc [in] - I/O descriptor
     *@param md [in] - array of meta data objecs (one for each profile)
     *@param profiles [in] - no of profiles
     *@return returns '0' for no error, <0 on error
     */
    int write_meta_data(void *desc, meta_data **md, int profiles);

    /** Clean up the Trick Play meta dat list (list of I-Frame positions and sizes)
     *@param list - pointer to the linked list
     *@return returns '0' for no error, <0 on error
     */
    void free_tplay_list(GList *list);

    /** writes the hint track for Trick Play
     *@param in_desc [in] - I/O descriptor
     *@param list [in] - link list containing the I-Frame information
     *@param n_iframes - number of i-frames in the sequence
     *@param duration - duration of the sequence
     *@return returns '0' for no error, <0 on error
     */
    int write_hint_track(void *desc, GList *list, int n_iframes, int duration);

    /** WARNING - EXPERIMENTAL generates a trick play stream from the trick play meta data
     *@param hint_desc [in] - I/O descriptor for the hint file
     *@param in_desc [in] - I/O descriptor for the intput sequence (should be the same profile as the hint file
     *@param iof [in] - I/O handlers
     *@param speed - speed of playback
     *@param direction [in] - 1 for FWD and 2 for REW
     *@param pos - position in the video sequence from where the trick play sequence should begin
     *@return returns '0' for no error, <0 on error
     */
    int create_tplay_data_from_index(void *hint_desc, void *in_desc,void *out_desc, io_funcs *iof, int speed, int direction, double pos);

    /** writes server side meta data [See NKN_VPE_MetaDataSpecification.doc in Wiki for more information 
     *@param str [in] - removes trailing slashes at the end of a string
     *@return returns '0' for no error, <0 on error
     */
    char* remove_trailing_slash(char *str);

    /** writes the meta data in XML format for the client
     *@param desc[in] - I/O descriptor
     *@param md[in] - array of meta data objects, one per profile
     *@param n_profiles[in] - number of profiles
     */
    int write_client_meta_data(void *, meta_data **md, int n_profiles);

    /** writes the job status information into a log file
     *@param fp - IO descriptor
     *@param n_profiles - total number of profiles processed
     *@param time_elapsed - total time take for processing the data
     *@param scratch_path - output path where the data was written
     *@param error_no - error on processing
     *@param n_chunks - number of chunks created
     *@param tot_size - total bytes processed by the system
     */
    int write_job_log(void *fd, int n_profiles, float time_elapsed, char *scratch_path, int error_no, size_t tot_size, uint32_t n_chunks);

    /** detects the container type for the file (looks only at the extension)
     *@param fname - name of the file
     *@return - returns 0 for FLV and 1 for MP4 containers
     */
    int32_t get_container_type(char *fname);

    /*writes the m3u8 files, one for each profile and one parent m3u8 file for iPhone adaptive bitrate streaming
     *@param p_md - array of meta data information 
     *@param n_profiles - no of input profiles
     *@param video_name - video name prefix for the output video
     *@param out_path - output path/ scratch directory
     *@param uri_prefix - publish uri prefix
     *@return returns 0 on success
     */
    int32_t write_m3u8_file(meta_data **p_md, int32_t n_profiles, char *video_name, char *out_path, char *uri_prefix);

#ifdef __cplusplus
}
#endif

#endif

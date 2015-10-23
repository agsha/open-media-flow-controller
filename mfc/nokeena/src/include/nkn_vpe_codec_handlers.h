/*
 *
 * Filename:  nkn_vpe_codec_handlers.h
 * Date:      2009/02/06
 * Module:    Handler function for different audio and video codecs
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef _CODEC_HANDLERS_
#define _CODEC_HANDLERS_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nkn_vpe_common.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_flv.h"
#include "nkn_vpe_pre_proc.h"
#include "nkn_vpe_error_no.h"

/* CODEC HANDLER FUNCTIONS */

/** This function splits a FLV file at I-Frames closest to the chunk interval specified - Applicable for H.263/VP6 video codecs.
 *and agnostic to Audio Codecs (Does not generate Hint Tracks for Trick Play)
 *@param void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 *@param char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 *@param uint32 profile [in]             - The profile id as an integer
 *@param char *out_path [in]             - null ternminated string with output path to which to write the files
 *@param charn *video_name [in]          - null terminated string with the file name
 *@param io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int VPE_PP_generic_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data );

/** This function splits a FLV file at I-Frames closest to the chunk interval specified - Applicable for H.264 video codec.
 *@param void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 *@param char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 *@param uint32 profile [in]             - The profile id as an integer
 *@param char *out_path [in]             - null ternminated string with output path to which to write the files
 *@param charn *video_name [in]          - null terminated string with the file name
 *@param io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int VPE_PP_h264_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data );  

/** This function splits a FLV file at I-Frames closest to the chunk interval specified. Applicable for H.263/VP6 video codecs.
 *and agnostic to Audio Codec. Generates Hint Tracks for Trick Play
 *@param void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 *@param char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 *@param uint32 profile [in]             - The profile id as an integer
 *@param char *out_path [in]             - null ternminated string with output path to which to write the files
 *@param charn *video_name [in]          - null terminated string with the file name
 *@param io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int VPE_PP_h264_chunking_with_hint(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data );    

/** This function splits a FLV file at I-Frames closest to the chunk interval specified.  Applicable for H.263/VP6 video codecs.
 *and agnostic to Audio Codec. Generates Hint Tracks for Trick Play
 *@param void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 *@param char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 *@param uint32 profile [in]             - The profile id as an integer
 *@param char *out_path [in]             - null ternminated string with output path to which to write the files
 *@param charn *video_name [in]          - null terminated string with the file name
 *@param io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int VPE_PP_generic_chunking_with_hint(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data );

/** This function splits a MP4 Fragmented file into fragments
 *@param void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 *@param char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 *@param uint32 profile [in]             - The profile id as an integer
 *@param char *out_path [in]             - null ternminated string with output path to which to write the files
 *@param charn *video_name [in]          - null terminated string with the file name
 *@param io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int VPE_PP_mp4_frag_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data );

/** This function segments an MPEG2 TS file that contains H.264 data in it. The segments are compatible with iPhone's adaptive streaming
 *@param void **in_de[in]                - array of I/O descriptors, the array has n_profiles entires
 *@param char io_mode [in]               - I/O backend mode '0' for Buffer based I/O and '1' for File based I/O
 *@param uint32 profile [in]             - The profile id as an integer
 *@param char *out_path [in]             - null ternminated string with output path to which to write the files
 *@param charn *video_name [in]          - null terminated string with the file name
 *@param io_funcs *iof [in]              - struct containing the I/O backend functions 
 */
int VPE_PP_MP2TS_chunking(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, void **private_data );

/*Helper Functions */
/* writes a list of TS packets
 *@param list [in] - list of packets
 *@param desc [in] - I/O descriptor
 *@param iof [in] - I/O handler
 */
int32_t write_ts_pkt_list(GList *list, void *desc, io_funcs *iof);

#endif //_CODEC_HANDLERS_

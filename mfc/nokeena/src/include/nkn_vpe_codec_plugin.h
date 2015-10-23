/*
 *
 * Filename:  codec_plugin.h
 * Date:      2009/02/06
 * Module:    FLV pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef _CODEC_PLUGIN_
#define _CODEC_PLUGIN_

#include <stdio.h>
#include <stdlib.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_common.h"
#include "nkn_vpe_error_no.h"

#ifdef __cplusplus
extern "C"{
#endif

    //No of Codec Handlers
#define N_CODEC_HANDLERS 6

    /*-----------------------------------------------------
    |  VIDEO CODEC   |  AUDIO CODEC  | IDENTIFIER          |
    | -----------------------------------------------------|
    |  VP6           |  MP3          | 0x24                |
    |------------------------------------------------------|
    |  AVC           |  AAC          | 0xa7                |
    |------------------------------------------------------|
    |  AVC           |  MP3          | 0x27                |
    |------------------------------------------------------|
    |  H.263         |  MP3          | 0x22                |
     -----------------------------------------------------*/
     
#define MP3VP6       0x24 
#define MP3AVC       0x27 
#define AACAVC       0xa7
#define MP3H263      0x22
#define AACVP6       0xa4
#define AACH263      0xa2

     /** CODEC HANDLER INTERFACE DEFINITION
     */
    typedef int (*codec_handler_interface)(void *, char, uint32_t, meta_data *, char *, io_funcs *iof, void **private_data );
    
    typedef struct __codec_handler_funcs{
	codec_handler_interface *hdl;
	unsigned char *codec_id;
	int n_handlers;
	int ref;
    }codec_handler;

    //CODEC HANDLER API FUNCTIONS */
    
    /** Initiallize the Codec Handler Plug-in structure
     *@param codec_handler *cd_hdl - pointer to the codec handler structure which needs to be initialized
     *@param int n_handlers - no of codec handlers that need to be registered
     *@return returns '0' if there no error and a negative value if there is an error
     */
    int VPE_PP_init_codec_handler(codec_handler *cd_hdl, int n_handlers);

    /**Add a codec handler
     *@param codec_handler *cd_hdl - pointer to the codec handler structure
     *@param codec_handler_interface hdl - function pointer of the codec handler
     *@param char - codec id (byte) that identifies the codec(s) Audio and Video Used
     *@return returns '0' if there no error and a negative value if there is an error 
     */
    int VPE_PP_add_codec_handler(codec_handler *cd_hdl, codec_handler_interface hdl, char );

    /**Destroy the Codec Handler once you are done, deallocates the memory allocated for the codec table
     *@param codec_handler *cd_hdl - pointer to the codec handler structure
     *@return returns '0' if there is no error and negative value if there is an error
     */
    int VPE_PP_destroy_codec_handler(codec_handler *cd_hdl);
   

#ifdef __cplusplus
}
#endif

#endif //_CODEC_HANDLER_

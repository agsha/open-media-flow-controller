/*
 *
 * Filename:  codec_plugin.c
 * Date:      2009/02/06
 * Module:    FLV pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */
#include "nkn_vpe_codec_plugin.h"


/********************************************************************************************************/
//
//                                   CODEC HANDLER MECHANISM
// 
/********************************************************************************************************/
int 
VPE_PP_init_codec_handler(codec_handler *cd_hdl, int n_handlers)
{
    int i;
    i = 0;

   
    cd_hdl->hdl=(codec_handler_interface*)malloc(n_handlers * sizeof(codec_handler_interface));
   

    cd_hdl->codec_id = (unsigned char*)malloc(n_handlers*sizeof(char));
    cd_hdl->n_handlers = n_handlers;
    cd_hdl->ref = 0;

    if(cd_hdl->hdl == NULL || cd_hdl->codec_id == NULL){
	return NKN_VPE_E_MEM;
    }

    return n_handlers;   
}

int 
VPE_PP_add_codec_handler(codec_handler *cd, codec_handler_interface hdl, char id)
{
    if(cd->ref >= cd->n_handlers)
	return NKN_VPE_E_OTHER;

    if(!hdl)
	return NKN_VPE_E_INVALID_HANDLE;
    
    cd->hdl[cd->ref] = hdl;
    cd->codec_id[cd->ref] = id;
    cd->ref++;


    return cd->ref;
    
    
}

int 
VPE_PP_destroy_codec_handler(codec_handler *cd)
{
    if(cd->hdl)
	free(cd->hdl);

    if(cd->codec_id)
	free(cd->codec_id);

    cd->ref = 0;
    cd->n_handlers = 0;
    
    return 0;
}

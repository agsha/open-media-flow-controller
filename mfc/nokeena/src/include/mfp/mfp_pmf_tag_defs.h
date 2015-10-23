/*
 *
 * Filename:  mfp_pmf_tag_defs.h
 * Date:      2010/08/26
 * Module:    pmf tag definitions
 *
 * (C) Copyright 2020 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _PMF_TAG_DEFS_
#define _PMF_TAG_DEFS_

#include <stdio.h>
#include <inttypes.h>

#include "mfp_publ_context.h"
#include "nkn_vpe_ism_read_api.h"
#ifdef __cplusplus 
extern "C" {
#endif

    typedef struct tag_xml_tag_parser {
	const char *tag_names;
	int32_t (*parse_tag)(void *private_data1, void *private_data2,
			     const void *out);
	struct tag_xml_tag_parser *child;
    }xml_tag_parser_t;

    typedef struct tag_pmf_tag_props {
	int32_t n_entries;
	xml_tag_parser_t *tag_parser;
    }pmf_tag_props_t;

    /* TAG SPECIFIC PARSERS */
    int32_t readMediaType(void *p1, void *p2, const void *out);
    int32_t readMediaSourceIP(void *p1, void *p2, const void *out);
    int32_t readMediaSrc(void *p1, void *p2, const void *out);
    int32_t readMediaBitrate(void *p1, void *p2, const void *out);
    int32_t readMediaProfileId(void *p1, void *p2, 
			       const void *out);
    int32_t readMediaAudPID(void *p1, void *p2, const void *out); 
    int32_t readMediaVidPID(void *p1, void *p2, const void *out);
    int32_t readFileSourceSetting(void *p1, void *p2, const void *out);
    int32_t readMediaInpFile(void *p1, void *p2, const void *out);
    int32_t readFilePumpModeSetting(void *p1, void *p2, const void *out);
    int32_t parseSSPMediaInfo(void *p1, void *p2, const void *out);
	int32_t parseMP4MediaInfo(void *p1, void *p2, const void *out);
    int32_t parseZeriMediaInfo(void *p1, void *p2, const void *out);
    int32_t parseMP2TS_SPTS_MediaInfo(void *p1, void *p2, 
				      const void *out); 
    int32_t populateSmoothStreamingProps(xml_read_ctx_t *xml,
					 mfp_publ_t *pub);
    int32_t populateFlashStreamingProps(xml_read_ctx_t *xml,
					 mfp_publ_t *pub);
    int32_t populateMobileStreamingProps(xml_read_ctx_t *xml,
					 mfp_publ_t *pub);
    int32_t parseConfigNode(void *p1, void *p2, const void *out);

    /* ROOT node definition */
#define ROOT_TAG "//PubManifest"
    
    /* Smooth Streaming Tag Defs */
#define SSP_MBR_ENCAPSULATION_TAG "//SSP_MBR_Encapsulation"
#define SSP_SBR_ENCAPSULATION_TAG "//SSP_SBR_Encapsulation"
#define SSP_MBR_MEDIA_TAG SSP_MBR_ENCAPSULATION_TAG"/SSP_Media"
#define SSP_SBR_MEDIA_TAG SSP_SBR_ENCAPSULATION_TAG"/SSP_Media"
#define SS_PUBSCHEME_TAG "//PubSchemes/SmoothStreaming"

    /* MP4 Encapsulation Tag Definitions */
#define MP4_ENCAPSULATION_TAG "//MP4_Encapsulation"
#define MP4_MEDIA_TAG MP4_ENCAPSULATION_TAG"/MP4_Media"

    /* Adobe Flash Streaming (Zeri) definition */
#define ZERI_ENCAPSULATION_TAG "//Zeri_Encapsulation"
#define ZERI_MEDIA_TAG ZERI_ENCAPSULATION_TAG"/Zeri_Media"
#define FS_PUBSCHEME_TAG "//PubSchemes/FlashStreaming"    
    
    /* MP2TS (SPTS) UDP/IP Encapsulation */
#define MP2TS_SPTS_ENCAPSULATION_TAG "//MP2TS_SPTS_UDP_Encapsulation"
#define MP2TS_SPTS_MEDIA_TAG MP2TS_SPTS_ENCAPSULATION_TAG"/SPTS_Media"

    /* Schedule Tag Definitions */
#define SCHEDULE_TAG "//ScheduleInfo"
    
    /* Mobile Streaming Pub Scheme Definition */
#define MS_PUBSCHEME_TAG "//PubSchemes/MobileStreaming"

    /* Global Output Configuration */
#define GLOBAL_OP_CFG_TAG "//PubSchemes/GlobalOutputConfig"
    
    int32_t pmfLoadTagNames(void);
#ifdef __cplusplus
}
#endif

#endif //_PMF_TAG_DEFS_

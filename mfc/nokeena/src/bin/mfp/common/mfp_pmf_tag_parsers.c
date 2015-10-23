/*
 *
 * Filename:  mfp_pmf_tag_parser.c
 * Date:      2010/08/26
 * Module:    pmf tag specific parser callback(s) implementations
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <inttypes.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//mfp includes
#include "mfp_publ_context.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_pmf_tag_defs.h"

//video parser includes (for XML parsing)
#include "nkn_vpe_ism_read_api.h"

static int32_t convertStrToEncapsType(const char *str);
static int32_t convertStrToLiveSrc(const char *src, 
				   struct sockaddr_in *sin);
static int32_t convertStrToMediaType(const char *str);
static int32_t convertStrToTransportType(const char *str);
static int32_t validateStreamParam(stream_param_t *sp, 
				   int32_t src_type);
static int32_t validateLiveStreamParam(stream_param_t *sp);
static int32_t validateFileStreamParam(stream_param_t *sp);
static int32_t validateSockAddr(struct sockaddr_in *sin);
static int8_t isMulticast(const char*);
static int8_t isSSM_IP(char const* ip_addr); 
static int32_t populateSSPMedia(xml_read_ctx_t *xml, 
	mfp_publ_t *pub, 
	pmf_tag_props_t SSPMediaTagProp);
static int32_t parseRootTagAttr(void *p1, void *p2, 
				const void *out);
static int32_t parseSchedulingInfo(void *p1, void *p2, 
				   const void *out); 
static int32_t getAuthTypeFromStr(const char *str);
static int32_t convertStrToConfigType(const char *str);

char pppttt[] =
    {0x2F,0x76,0x61,0x72,0x2F,0x6F,0x70,0x74,0x2F,0x74,0x6D,0x73,0x2F,0x6F,0x75,
     0x74,0x70,0x75,0x74,0x2F,0x73,0x73,0x6C,0x5F,0x6C,0x69,0x63,0x65,0x6E,0x73,0x65,0x0};

/* ROOT tag parser defs */
xml_tag_parser_t pmfRootAttrParser[] = {
    {ROOT_TAG"/@name", NULL, NULL},
    {ROOT_TAG"/@version", NULL, NULL}
};

xml_tag_parser_t pmfRootTagParser[] = {
    {ROOT_TAG, parseRootTagAttr, pmfRootAttrParser}
};

/* Smooth Streaming Media Tag Parser Defs */
xml_tag_parser_t pmfSSP_MBRMediaTagParser[] = {
    {SSP_MBR_MEDIA_TAG"/Type", readMediaType, NULL},
    {SSP_MBR_MEDIA_TAG"/Src", readMediaSrc, NULL},
    {SSP_MBR_MEDIA_TAG"/BitRate", NULL, NULL} /*Packaged SmootStreaming 
					       *does not have bitrate*/
};

xml_tag_parser_t pmfSSP_SBRMediaTagParser[] = {
    {SSP_SBR_MEDIA_TAG"/Type", readMediaType, NULL},
    {SSP_SBR_MEDIA_TAG"/Src", readMediaSrc, NULL},
    {SSP_SBR_MEDIA_TAG"/BitRate", NULL, NULL} /*Packaged SmootStreaming 
					       *does not have bitrate*/
};

/* Smooth Streaming PubSchemes Tag Defs */
xml_tag_parser_t pmfSSPubSchemeTagParser[] = {
    {SS_PUBSCHEME_TAG"/@KeyFrameInterval", NULL, NULL},
    {SS_PUBSCHEME_TAG"/@StorageURL", NULL, NULL},
    {SS_PUBSCHEME_TAG"/@DeliveryURL", NULL, NULL},
    {SS_PUBSCHEME_TAG"/@DVR", NULL, NULL},
    {SS_PUBSCHEME_TAG"/@DiskUsage", NULL, NULL},
    {SS_PUBSCHEME_TAG"/@DVRWindowLength", NULL, NULL}
};

xml_tag_parser_t pmfZeri_MediaTagParser[] = {
    {ZERI_MEDIA_TAG"/Type", readMediaType, NULL},
    {ZERI_MEDIA_TAG"/Src", readMediaSrc, NULL},
    {ZERI_MEDIA_TAG"/BitRate", NULL, NULL} /*Packaged Zeri 
					    *does not have bitrate*/
};

xml_tag_parser_t pmfMP4_MediaTagParser[] = {
    {MP4_MEDIA_TAG"/Src", readMediaSrc, NULL},
    {MP4_MEDIA_TAG"/Type", readMediaType, NULL},
};

/* flash Streaming PubSchemes Tag Defs */
xml_tag_parser_t pmfFSPubSchemeTagParser[] = {
    {FS_PUBSCHEME_TAG"/@FragmentDuration", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@SegmentDuration", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@StorageURL", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@DeliveryURL", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@StreamType", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@DVRSeconds", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@Encryption", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@CommonKey", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@LicenseServerUrl", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@LicenseServerCert", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@TransportCert", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@PackagerCredential", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@CredentialPwd", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@PolicyFile", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@ContentId", NULL, NULL},
    {FS_PUBSCHEME_TAG"/@OrigModReq", NULL, NULL}
};

/* MP2TS (SPTS) Media Tag Parser Defs */
xml_tag_parser_t pmfMP2TS_SPTS_MediaTagParser[] = {
    {MP2TS_SPTS_MEDIA_TAG"/Type", readMediaType, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/SourceIP", readMediaSourceIP, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/Dest", readMediaSrc, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/Src", readMediaSrc, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/BitRate", readMediaBitrate, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/ProfileId", readMediaProfileId, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/apid", readMediaAudPID, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/vpid", readMediaVidPID, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/FileSource", readFileSourceSetting, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/inpFile", readMediaInpFile, NULL},
    {MP2TS_SPTS_MEDIA_TAG"/FilePumpMode", readFilePumpModeSetting, NULL}
};


/* Mobile Streaming PubSchemes Tag Defs */
xml_tag_parser_t pmfMSPubSchemeTagParser[] = {
    {MS_PUBSCHEME_TAG"/@KeyFrameInterval", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@SegmentStartIndex", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@MinSegsInChildPlaylist", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@SegmentIndexPrecision", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@SegmentRolloverInterval", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@DVR", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@DVRWindowLength", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@StorageURL", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@DeliveryURL", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@KMS_ServerAddress", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@KMS_ServerPort", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@KMS_Type", NULL, NULL}, 
    {MS_PUBSCHEME_TAG"/@KeyRotationInterval", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@IVLen", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@KMS_FileStorageUrl", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@DiskUsage", NULL, NULL},
    {MS_PUBSCHEME_TAG"/@Encryption", NULL, NULL}
};

xml_tag_parser_t pmfSrcTagParser[] ={
    {"//LiveMediaSource", NULL, NULL},
    {"//FileMediaSource", NULL, NULL} ,
    {"//ControlMediaSource", NULL, NULL}
};

xml_tag_parser_t pmfEncapsTagParser[] = {
    {SCHEDULE_TAG, parseSchedulingInfo, NULL},
    {SSP_MBR_ENCAPSULATION_TAG, parseSSPMediaInfo, pmfSSP_MBRMediaTagParser},
    {SSP_SBR_ENCAPSULATION_TAG, parseSSPMediaInfo, pmfSSP_SBRMediaTagParser},
    {MP4_ENCAPSULATION_TAG, parseMP4MediaInfo, pmfMP4_MediaTagParser},
    {ZERI_ENCAPSULATION_TAG, parseZeriMediaInfo, pmfZeri_MediaTagParser},
    {"//MP2TS_SPTS_UDP_Encapsulation", parseMP2TS_SPTS_MediaInfo, NULL},
    {"//MP2_TS_SBR", NULL, NULL}
};

xml_tag_parser_t pmfConfigNodeTagParser [] = {
    {"//ControlMediaSource/Config/@type", NULL, NULL},
    {"//ControlMediaSource/Config/@value", NULL, NULL}
};

xml_tag_parser_t pmfControlTagParser[] = {
    {"//ControlMediaSource/Action", NULL, NULL},
    {"//ControlMediaSource/SessionList/name", NULL, NULL},
    {"//ControlMediaSource/Config", parseConfigNode, pmfConfigNodeTagParser}
};

xml_tag_parser_t pmfScheduleInfoParser[] = {
    {SCHEDULE_TAG"/SessionStart", NULL, NULL},
    {SCHEDULE_TAG"/Duration", NULL, NULL}
};

xml_tag_parser_t pmfGlobalOPCfgParser[] = {
    {GLOBAL_OP_CFG_TAG"/SessionDiskUsage", NULL, NULL}
};
    
/*************************************************************
 *           GLOBAL DECLARATIONS
 ************************************************************/
pmf_tag_props_t srcTagProp, encapsTagProp, SSP_MBRMediaTagProp,
    MP2TS_SPTS_MediaTagProp, msPubSchemeTagProp, ssPubSchemeTagProp, 
    SSP_SBRMediaTagProp, rootTagProp, controlTagProp,
    zeriMediaTagProp, fsPubSchemeTagProp, MP4_MediaTagProp,
    globalOPCfgProp, schedInfo, configNodeInfo;

int32_t
pmfLoadTagNames(void)
{
    srcTagProp.n_entries = \
	sizeof(pmfSrcTagParser)/sizeof(pmfSrcTagParser[0]);
    srcTagProp.tag_parser = pmfSrcTagParser;

    encapsTagProp.n_entries = \
	sizeof(pmfEncapsTagParser)/sizeof(pmfEncapsTagParser[0]);
    encapsTagProp.tag_parser = pmfEncapsTagParser;

    SSP_MBRMediaTagProp.n_entries = \
	sizeof(pmfSSP_MBRMediaTagParser)/sizeof(pmfSSP_MBRMediaTagParser[0]);
    SSP_MBRMediaTagProp.tag_parser = pmfSSP_MBRMediaTagParser;

    SSP_SBRMediaTagProp.n_entries = \
	sizeof(pmfSSP_SBRMediaTagParser)/sizeof(pmfSSP_SBRMediaTagParser[0]);
    SSP_SBRMediaTagProp.tag_parser = pmfSSP_SBRMediaTagParser;

    MP2TS_SPTS_MediaTagProp.n_entries = \
					sizeof(pmfMP2TS_SPTS_MediaTagParser)/sizeof(pmfMP2TS_SPTS_MediaTagParser[0]);
    MP2TS_SPTS_MediaTagProp.tag_parser = pmfMP2TS_SPTS_MediaTagParser;

    MP4_MediaTagProp.n_entries =	sizeof(pmfMP4_MediaTagParser)/
		sizeof(pmfMP4_MediaTagParser[0]);
    MP4_MediaTagProp.tag_parser = pmfMP4_MediaTagParser;

    msPubSchemeTagProp.n_entries =\
	sizeof(pmfMSPubSchemeTagParser)/sizeof(pmfMSPubSchemeTagParser[0]);
    msPubSchemeTagProp.tag_parser = pmfMSPubSchemeTagParser;

    ssPubSchemeTagProp.n_entries =\
	sizeof(pmfSSPubSchemeTagParser)/sizeof(pmfSSPubSchemeTagParser[0]);
    ssPubSchemeTagProp.tag_parser = pmfSSPubSchemeTagParser;

    rootTagProp.n_entries = \
	sizeof(pmfRootTagParser)/sizeof(pmfRootTagParser[0]);
    rootTagProp.tag_parser = pmfRootTagParser;

    controlTagProp.n_entries = \
	sizeof(pmfControlTagParser)/sizeof(pmfControlTagParser[0]);
    controlTagProp.tag_parser = pmfControlTagParser;

    zeriMediaTagProp.n_entries = \
	sizeof(pmfZeri_MediaTagParser)/sizeof(pmfZeri_MediaTagParser[0]);
    zeriMediaTagProp.tag_parser = pmfZeri_MediaTagParser;

    fsPubSchemeTagProp.n_entries = \
	sizeof(pmfFSPubSchemeTagParser)/sizeof(pmfFSPubSchemeTagParser[0]);
    fsPubSchemeTagProp.tag_parser = pmfFSPubSchemeTagParser;

    globalOPCfgProp.n_entries = \
	sizeof(pmfGlobalOPCfgParser)/sizeof(pmfGlobalOPCfgParser[0]);
    globalOPCfgProp.tag_parser = pmfGlobalOPCfgParser;

    schedInfo.n_entries = \
	sizeof(pmfScheduleInfoParser)/sizeof(pmfScheduleInfoParser[0]);
    schedInfo.tag_parser = pmfScheduleInfoParser;

    configNodeInfo.n_entries =			\
	sizeof(pmfConfigNodeTagParser)/sizeof(pmfConfigNodeTagParser[0]);
    configNodeInfo.tag_parser = pmfConfigNodeTagParser;

    return 0;
}

/*****************************************************************
 *          TAG SPECIFIC PARSING CALLBACK HANDLERS
 ****************************************************************/
/**
 * Callback for SmoothStreaming Package Encapsulation tag parsing
 * @param p1 - xml context
 * @param p2 - publisher context
 * @param out - the stream parameters structure that needs to be
 * populated 
 * @return returns 0 on error and negative integer on error
 */
int32_t
parseSSPMediaInfo(void *p1, void *p2, const void *out)
{
    xml_read_ctx_t *xml;
    xmlXPathObject *xpath_obj;
    stream_param_t *sp;
    mfp_publ_t *pub;
    int32_t i, err;
    pmf_tag_props_t *ssp_med_tag;

    err = 0;
    xml = (xml_read_ctx_t *)p1;
    i = *((int32_t*)p2);
    pub = (mfp_publ_t*)out;

    sp = pub->encaps[0];
    xpath_obj = NULL;

    xpath_obj =	\
	xmlXPathEvalExpression((xmlChar *)\
			       "//*/SSP_Media",
			       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto error;
    }

    pub->streams_per_encaps[0] = xpath_obj->nodesetval->nodeNr;
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    pub->encaps[0]->encaps_type = ISMV_SBR;
    populateSSPMedia(xml, pub, SSP_SBRMediaTagProp);
 error:
    return err;

}

int32_t
parseMP4MediaInfo(void *p1, void *p2, const void *out)
{   
	xml_read_ctx_t *xml;
	xmlXPathObject *xpath_obj;
	stream_param_t *sp;
	mfp_publ_t *pub;
	int32_t i, err;

	err = 0;
	xml = (xml_read_ctx_t *)p1;
	pub = (mfp_publ_t*)out;
	pub->encaps[0]->encaps_type = MP4;
	sp = pub->encaps[0];
	xpath_obj = NULL;

	xpath_obj = xmlXPathEvalExpression( (xmlChar *) MP4_MEDIA_TAG, xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
		err = -E_MFP_PMF_INVALID_TAG;
		goto error;
	}

	pub->streams_per_encaps[0] = xpath_obj->nodesetval->nodeNr;
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);

	for(i = 0; i < MP4_MediaTagProp.n_entries; i++ ) {
		xpath_obj = xmlXPathEvalExpression(
				(xmlChar *) MP4_MediaTagProp.tag_parser[i].tag_names,
				xml->xpath);
		if (MP4_MediaTagProp.tag_parser[i].parse_tag) {
			MP4_MediaTagProp.tag_parser[i].parse_tag(xpath_obj,
					&pub->src_type, sp);
		}
		if (xpath_obj) xmlXPathFreeObject(xpath_obj);
		xpath_obj = NULL;
	}

 error:
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	return err;
}


int32_t
parseZeriMediaInfo(void *p1, void *p2, const void *out)
{
    xml_read_ctx_t *xml;
    xmlXPathObject *xpath_obj;
    stream_param_t *sp;
    mfp_publ_t *pub;
    int32_t i, err;
    pmf_tag_props_t *ssp_med_tag;

    err = 0;
    xml = (xml_read_ctx_t *)p1;
    i = *((int32_t*)p2);
    pub = (mfp_publ_t*)out;

    sp = pub->encaps[0];
    xpath_obj = NULL;

    xpath_obj =	\
	xmlXPathEvalExpression((xmlChar *)\
			       "//*/Zeri_Media",
			       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto error;
    }

    pub->streams_per_encaps[0] = xpath_obj->nodesetval->nodeNr;
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    err = 0;    
    sp = pub->encaps[0];
    for(i = 0; i < zeriMediaTagProp.n_entries; i++ ) {
	xpath_obj = \
	    xmlXPathEvalExpression(
				   (xmlChar *)\
				   zeriMediaTagProp.tag_parser[i].tag_names,
				   xml->xpath);
	if (zeriMediaTagProp.tag_parser[i].parse_tag) {
	    zeriMediaTagProp.tag_parser[i].parse_tag(xpath_obj, 
						     &pub->src_type, sp);
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	xpath_obj = NULL;
    }

 error:
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return err;


}

static int32_t
populateSSPMedia(xml_read_ctx_t *xml, mfp_publ_t *pub, 
	pmf_tag_props_t SSPMediaTagProp) 
{
    xmlXPathObject *xpath_obj;
    stream_param_t *sp;
    int32_t i, err;

    err = 0;    
    sp = pub->encaps[0];
    for(i = 0; i < SSPMediaTagProp.n_entries; i++ ) {
	xpath_obj = \
	    xmlXPathEvalExpression(
				   (xmlChar *)\
				   SSPMediaTagProp.tag_parser[i].tag_names,
				   xml->xpath);
	if (SSPMediaTagProp.tag_parser[i].parse_tag) {
	    SSPMediaTagProp.tag_parser[i].parse_tag(xpath_obj, 
						    &pub->src_type, sp);
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	xpath_obj = NULL;
    }

 error:
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return err;

}

int32_t
parseMP2TS_SPTS_MediaInfo(void *p1, void *p2, const void *out)
{
    xml_read_ctx_t *xml;
    xmlXPathObject *xpath_obj;
    stream_param_t *sp;
    mfp_publ_t *pub;
    int32_t i, err;

    err = 0;
    xml = (xml_read_ctx_t *)p1;
    pub = (mfp_publ_t*)out;
    pub->encaps[0]->encaps_type = TS;
    sp = pub->encaps[0];
    xpath_obj = NULL;

    xpath_obj =\
	xmlXPathEvalExpression(
			       (xmlChar *)\
			       MP2TS_SPTS_MEDIA_TAG,
			       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto error;
    }

    pub->streams_per_encaps[0] = xpath_obj->nodesetval->nodeNr;
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    for(i = 0; i < MP2TS_SPTS_MediaTagProp.n_entries; i++ ) {
	xpath_obj =				\
	    xmlXPathEvalExpression(
				   (xmlChar *)\
				   MP2TS_SPTS_MediaTagProp.tag_parser[i].tag_names,
				   xml->xpath);
	if (MP2TS_SPTS_MediaTagProp.tag_parser[i].parse_tag) {
	    MP2TS_SPTS_MediaTagProp.tag_parser[i].parse_tag(xpath_obj, 
							    &pub->src_type, sp);
	}
	if(!strcmp(MP2TS_SPTS_MediaTagProp.tag_parser[i].tag_names, "//MP2TS_SPTS_UDP_Encapsulation/SPTS_Media/Dest")){
	    int j;
	    stream_param_t *st;
	    st = sp;
	    for(j = 0; j < xpath_obj->nodesetval->nodeNr; j++) {
		if(pub->src_type == LIVE_SRC){
		    if(st->media_src.live_src.is_multicast == 1) {
			pub->stats.num_multicast_strms += 1;
		    }else{
			pub->stats.num_unicast_strms += 1;
		    }
		}
		st++;
	    }
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	xpath_obj = NULL;
    }

 error:
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return err;

}

static int32_t
parseSchedulingInfo(void *p1, void *p2, const void *out)
{
    int32_t err = 0;
    xmlXPathObject *xpath_obj = NULL;
    xml_read_ctx_t *xml;
    mfp_publ_t *pub;
    time_t curr_time, sess_start_time;
    struct tm tm_sess_start_time;
    int32_t i;
    char *tbuf = NULL;

    xml = (xml_read_ctx_t*)p1;
    pub = (mfp_publ_t *)out;

    for (i = 0; i < schedInfo.n_entries; i++) {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)			\
					   pmfScheduleInfoParser[i].tag_names,
					   xml->xpath);
	switch (i) {
	    case 0:
		if (!xpath_obj->nodesetval->nodeNr) {
		    err = -E_MFP_PMF_INVALID_TAG;
		    xmlXPathFreeObject(xpath_obj);
		    return err;
		}
		tbuf =							\
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content;
		if (strlen(tbuf) == 1 && tbuf[0] == '0') {
		    /* handle special case where the schedule start time is 'now'
		     */ 
		    pub->ssi.st_time = 0;
		} else {
		    curr_time = time(NULL);
		    strptime(tbuf, "%d-%m-%Y %H:%M:%S", &tm_sess_start_time);
		    sess_start_time = mktime(&tm_sess_start_time);
		    if (sess_start_time < curr_time) {
			err = -E_MFP_PMF_INVALID_SCHED_TIME;
			xmlXPathFreeObject(xpath_obj);
			goto error;
		    }
		    pub->ssi.st_time = sess_start_time - curr_time;
		}
		break;
	    case 1:
		if (!xpath_obj->nodesetval->nodeNr) {
		    err = -E_MFP_PMF_INVALID_TAG;
		    xmlXPathFreeObject(xpath_obj);
		    goto error;
		}
		pub->ssi.duration = \
		    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
		break;
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }
    
    return err;

 error:
    return err;
}

static int32_t
parseRootTagAttr(void *p1, void *p2, const void *out)
{
    int32_t err;
    xmlXPathObject *xpath_obj;
    xml_read_ctx_t *xml;
    mfp_publ_t *pub;

    xml = (xml_read_ctx_t*)p1;
    pub = (mfp_publ_t*)out;
    err = 0;

    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       pmfRootAttrParser[0].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	xmlXPathFreeObject(xpath_obj);
	return err;
    }

    strcpy((char*)pub->name,
	    ((char*)xpath_obj->nodesetval->nodeTab[0]->children->content)); 

    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return 0;
}

int32_t
readMediaType(void *p1, void *p2, const void *out)
{
    xmlXPathObject *obj;
    stream_param_t *sp;
    int32_t i;

    obj = (xmlXPathObject*)p1;
    sp = (stream_param_t*)out;

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	sp->med_type = 
	    convertStrToMediaType((char *)\
				  obj->nodesetval->nodeTab[i]->children->content);
	sp++;
    }

    return 0;	    
}


int32_t
readMediaSourceIP(void *p1, void *p2, const void *out)
{
	xmlXPathObject *obj;
	stream_param_t *st;
	int32_t *stype, err;
	int32_t i;

	obj = (xmlXPathObject*)p1;
	st = (stream_param_t*)out;
	stype = (int32_t*)p2;
	err = 0;
	if (obj->nodesetval->nodeTab == NULL)
		return err;

	for (i = 0; i < obj->nodesetval->nodeNr; i++) {
		if (*stype == LIVE_SRC) {
			if (inet_pton(AF_INET, (const char*)obj->
						nodesetval->nodeTab[i]->children->content,
						&st->media_src.live_src.source_if) != 1)
				err = -E_MFP_PMF_INVALID_TAG;
		} else
			err = -E_MFP_PMF_INVALID_TAG;
		st++;
	}

	return err;
}


int32_t
readMediaSrc(void *p1, void *p2, const void *out)
{

    xmlXPathObject *obj;
    stream_param_t *st;
    int32_t i, *stype, err;

    obj = (xmlXPathObject*)p1;
    st = (stream_param_t*)out;
    stype = (int32_t*)p2;
    err = 0;

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	if (*stype == LIVE_SRC) {
	    st->media_src.live_src.is_multicast = 0;
	    convertStrToLiveSrc((const char *)\
				obj->nodesetval->nodeTab[i]->children->content,
				&st->media_src.live_src.to);
	    st->media_src.live_src.is_multicast =
		isMulticast((const char *)
			    obj->nodesetval->nodeTab[i]->children->content);
	    if (st->media_src.live_src.is_multicast != 1)
		printf("NOT A MULTICAST ADDRESS\n");
	    st->media_src.live_src.is_ssm =
		isSSM_IP((const char *)
			    obj->nodesetval->nodeTab[i]->children->content);
	} else if (*stype == FILE_SRC) {
	    strcpy((char*)st->media_src.file_src.file_url, 
		   (char *)obj->nodesetval->nodeTab[i]->children->content);
	} else {
	    err = -E_MFP_PMF_INVALID_TAG;
	    goto error;
	}
	st++;
    }

 error:
    return err;
}

int32_t
readMediaBitrate(void *p1, void *p2, const void *out)
{
    xmlXPathObject *obj;
    stream_param_t *sp;
    int32_t i;

    obj = (xmlXPathObject*)p1;
    sp = (stream_param_t*)out;

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	sp->bit_rate = atoi((const char*)\
			    obj->nodesetval->nodeTab[i]->children->content);
	sp++;
    }

    return 0;	    
}

int32_t
readMediaProfileId(void *p1, void *p2, const void *out)
{
    xmlXPathObject *obj;
    stream_param_t *sp;
    int32_t i;

    obj = (xmlXPathObject*)p1;
    sp = (stream_param_t*)out;

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	sp->profile_id = atoi((const char*)\
			      obj->nodesetval->nodeTab[i]->children->content);
	sp++;
    }

    return 0;	    
}


int32_t
readMediaAudPID(void *p1, void *p2, const void *out)
{
    xmlXPathObject *obj;
    stream_param_t *sp;
    int32_t i;

    obj = (xmlXPathObject*)p1;
    sp = (stream_param_t*)out;

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	sp->apid = atoi((const char*)\
			obj->nodesetval->nodeTab[i]->children->content);
	sp++;
    }

    return 0;	    
}

int32_t
readMediaVidPID(void *p1, void *p2, const void *out)
{
    xmlXPathObject *obj;
    stream_param_t *sp;
    int32_t i;

    obj = (xmlXPathObject*)p1;
    sp = (stream_param_t*)out;

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	sp->vpid = atoi((const char*)\
			obj->nodesetval->nodeTab[i]->children->content);
	sp++;
    }

    return 0;	    
}

int32_t
readFileSourceSetting(void *p1, void *p2, const void *out)
{
	xmlXPathObject *obj;
	stream_param_t *sp;
	int32_t i;

	obj = (xmlXPathObject*)p1;
	sp = (stream_param_t*)out;

	for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	    sp->file_source_mode = 0;
	    if(!strcasecmp((char *)obj->nodesetval->nodeTab[i]->children->content ,"on")){
	    	sp->file_source_mode = 1;
	    }
	    sp++;
	}    
	return 0;
}



int32_t
readMediaInpFile(void *p1, void *p2, const void *out)
{
    xmlXPathObject *obj;
    stream_param_t *sp;
    int32_t i;

    obj = (xmlXPathObject*)p1;
    sp = (stream_param_t*)out;
    
    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	strcpy((char *)sp->inp_file_name,
		(char *)obj->nodesetval->nodeTab[i]->children->content);
	sp++;
    }    
    return 0;	    
}

int32_t
readFilePumpModeSetting(void *p1, void *p2, const void *out)
{
	xmlXPathObject *obj;
	stream_param_t *sp;
	int32_t i;

	obj = (xmlXPathObject*)p1;
	sp = (stream_param_t*)out;

	for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	    sp->file_pump_mode = TS_INPUT;
	    if(!strcasecmp((char *)obj->nodesetval->nodeTab[i]->children->content ,"upcp")){
	    	sp->file_pump_mode = UPCP_INPUT;
	    }
	    sp++;
	}    
	return 0;
}


int32_t
parseConfigNode(void *p1, void *p2, const void *out)
{
    xml_read_ctx_t *xml = (xml_read_ctx_t*)p1;
    mfp_publ_t *pub = (mfp_publ_t*)out;
    int32_t i = 0, rv = 0;
    xmlXPathObject *xpath_obj = NULL;

    for (i = 0; i < configNodeInfo.n_entries; i++) {
	xpath_obj = NULL;
	xpath_obj =\
	    xmlXPathEvalExpression(\
			       (xmlChar*)(configNodeInfo.tag_parser[i].tag_names),
			       xml->xpath);
	if (!xpath_obj) {
	    continue;
	}
	if (!xpath_obj->nodesetval) {
	    xmlXPathFreeObject(xpath_obj);
	    continue;
	}
	switch(i) {
	    case 0:
		rv = convertStrToConfigType(				\
					    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
		if (rv < 0) {
		    xmlXPathFreeObject(xpath_obj);
		    goto error;
		}
		pub->cfg.type = rv;
		break;
	    case 1:
		strcpy(&pub->cfg.val[0],
		       (const char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
		break;
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }
    return 0;

 error:
    return rv;
}

static int32_t
convertStrToConfigType(const char *str)
{
    if (!strcasecmp(str, "AddWatchDir")) {
	return MFP_CFG_ADD_WATCHDIR;
    } else if (!strcasecmp(str, "DelWatchDir")) {
	return MFP_CFG_DEL_WATCHDIR;
    } else {
	return -E_MFP_PMF_INVALID_TAG;
    }
}

/**
 * fill the flash streaming properties
 * @param obj - xml object pointer containing the property node
 * @param ss - the smooth stremaing parameter context
 */
int32_t populateMobileStreamingProps(xml_read_ctx_t *xml,
				     mfp_publ_t *pub)
{
    int32_t err, len;
    xmlXPathObject *xpath_obj;
    mobile_parm_t *ss;

    err = 0;
    ss = &pub->ms_parm;
    ss->status = FMTR_ON;

    //3 parse key_frame_interval
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[0].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	xmlXPathFreeObject(xpath_obj);
	return err;
    }
    ss->key_frame_interval = 
	atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);


    //3 parse segment_start_idx
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[1].tag_names,
				       xml->xpath);

    if (!xpath_obj->nodesetval->nodeNr) {
	ss->segment_start_idx = 1;
    } else {
	ss->segment_start_idx = 
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
    }
    if(xpath_obj) xmlXPathFreeObject(xpath_obj);


    //3 parse min_segment_child_plist
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[2].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	ss->min_segment_child_plist = 6;
    } else {
	ss->min_segment_child_plist = 
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
    }
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);



    //3 segment_idx_precision
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[3].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	xmlXPathFreeObject(xpath_obj);
	return err;
    }
    ss->segment_idx_precision = 
	atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);



    //3 parse segment_rollover_interval
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[4].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	ss->segment_rollover_interval = 10000;
    } else {
	ss->segment_rollover_interval = 
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
    }
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);


    //3 parse DVR on or off
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[5].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	xmlXPathFreeObject(xpath_obj);
    } else { 
	ss->dvr = 0;
	if(!strcasecmp((char*)xpath_obj->nodesetval->nodeTab[0]->children->content,"on"))
	    ss->dvr = 1;
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }


    //3 parse DVRWindowLength
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[6].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	ss->dvr_window_length = 0;
    } else {
	ss->dvr_window_length =
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
    }
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);    




    //3 parse ms_store_url 
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[7].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	xmlXPathFreeObject(xpath_obj);
	return err;
    }
    strcpy((char*)pub->ms_store_url,
	    ((char*)xpath_obj->nodesetval->nodeTab[0]->children->content)); 
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);



    //3 parse ms_delivery_url
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[8].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	xmlXPathFreeObject(xpath_obj);
	return err;
    }
    strcpy((char*)pub->ms_delivery_url, 
	    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    /* validate the delivery uri 
     * (a) has .m3u8 extension (BZ 6304) 
     * (b) has atleast one char before .m3u8 extension (BZ 7713)
     */
    len = strlen((char*)pub->ms_delivery_url);
    if (strcasecmp((char*)pub->ms_delivery_url + len - 5, ".m3u8")) {
	err = -E_MFP_PMF_INVALID_DELIVERY_URI;
	return err;
    }
    if (len <= 5) {
	err = -E_MFP_PMF_INVALID_DELIVERY_URI;
	return err;
    }

    /* KMS Params */
    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[16].tag_names,
				       xml->xpath);
    if (xpath_obj && xpath_obj->nodesetval) {
	if (xpath_obj->nodesetval->nodeNr) {
	    char tmp[4] = {0};
	    strncpy((char*)tmp,
		   (char*)xpath_obj->nodesetval->nodeTab[0]->children->content, 4); 
	    if (strcasecmp(tmp, "on")) {
		ss->encr_flag = 0;
		xmlXPathFreeObject(xpath_obj);
		goto parse_disk_usage;
	    } else {
		ss->encr_flag = 1;
	    }
	    xmlXPathFreeObject(xpath_obj);
	}
    }

    /* check for SSL license */
    if (ss->encr_flag) {
	struct stat st;
	int32_t rv = 0;
	char *lnk_name = NULL;
	rv = lstat(pppttt, &st);
	if (rv == -1) {
	    err = -E_MFP_SSL_LICENSE_UNAVAIL;
	    DBG_MFPLOG(pub->name, SEVERE, MOD_MFPLIVE, 
		       "SSL license not installed; cannot proceed with"
		       " Apple HLS encrypted session. exiting with"
		       " error code %d", err);
	    return err;
	}
	lnk_name = (char *)mfp_live_calloc(1, st.st_size + 1);
	rv = readlink(pppttt, lnk_name, st.st_size + 1);
	if (rv == -1 || rv != 1) {
	    err = -E_MFP_SSL_LICENSE_ERR;
	    DBG_MFPLOG(pub->name, SEVERE, MOD_MFPLIVE, 
		       "Error querying SSL license; cannot proceed with"
		       " Apple HLS encrypted session. exiting with"
		       " error code %d", err);
	    free(lnk_name);
	    return err;
	}
	
	if (lnk_name[0] != '1') {
	    err = -E_MFP_SSL_LICENSE_FAIL;
	    DBG_MFPLOG(pub->name, SEVERE, MOD_MFPLIVE, 
		       "SSL license failed/expired; cannot proceed with"
		       " Apple HLS encrypted session. exiting with"
		       " error code %d", err);
	    free(lnk_name);
	    return err;
	}
	if (lnk_name) {
	    free(lnk_name);
	}
	    
    }
    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[9].tag_names,
				       xml->xpath);
    if(xpath_obj->nodesetval->nodeNr) {
	strcpy((char*)ss->kms_addr,
	       (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	xmlXPathFreeObject(xpath_obj);
    }
    

    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[10].tag_names,
				       xml->xpath);
    if(xpath_obj->nodesetval->nodeNr) {
	ss->kms_port = \
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	xmlXPathFreeObject(xpath_obj);
    }

    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[11].tag_names,
				       xml->xpath);
    if(xpath_obj->nodesetval->nodeNr) {
	err = \
	    getAuthTypeFromStr((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	if (err < 0) {
	    return err;
	} else {
	ss->kms_type = err;
	err = 0;
	}
	xmlXPathFreeObject(xpath_obj);
    }
       
    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[12].tag_names,
				       xml->xpath);
    ss->key_rotation_interval = 1;
    if(xpath_obj->nodesetval->nodeNr) {
		ss->key_rotation_interval = \
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	xmlXPathFreeObject(xpath_obj);
    }

    ss->iv_len = 16;
    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[13].tag_names,
				       xml->xpath);
    if(xpath_obj->nodesetval->nodeNr) {
	ss->iv_len = \
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	xmlXPathFreeObject(xpath_obj);
    }

    xpath_obj = xmlXPathEvalExpression((xmlChar*)\
				       msPubSchemeTagProp.tag_parser[14].tag_names,
				       xml->xpath);
    if(xpath_obj->nodesetval->nodeNr) {
	strcpy((char*)ss->key_delivery_base_uri,
	       (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	xmlXPathFreeObject(xpath_obj);
    } else {
		memset(&ss->key_delivery_base_uri[0], 0, 512);
    }

 parse_disk_usage:
    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       msPubSchemeTagProp.tag_parser[15].tag_names,
				       xml->xpath);
    /**
     * if the disk usage parameter is set for the stream then use it;
     * if it is zero or absent inherit from the global op cfg
     */    
    if (xpath_obj->nodesetval->nodeNr) {
	ss->disk_usage = atoi((char *)\
			      xpath_obj->nodesetval->nodeTab[0]->children->content);
	if (ss->disk_usage > 0) {
	    err = mfp_safe_io_init(NULL, 0.0, ss->disk_usage, &ss->sioc);
	    if (err) {
		xmlXPathFreeObject(xpath_obj);
		return err;
	    }
	} else if (ss->disk_usage == 0) {
	    ss->sioc = pub->op_cfg.sioc;
	}
    } else {
	ss->sioc = pub->op_cfg.sioc;
    }
    
    xmlXPathFreeObject(xpath_obj);
    return err;
}

/**
 * fill the silverlight smooth streaming properties
 * @param obj - xml object pointer containing the property node
 * @param ss - the smooth stremaing parameter context
 */
int32_t populateSmoothStreamingProps(xml_read_ctx_t *xml,
				     mfp_publ_t *pub)
{
    int32_t err;
    xmlXPathObject *xpath_obj;
    silver_parm_t *ss;

    err = 0;
    ss = &pub->ss_parm;

    ss->status = FMTR_ON;
    xpath_obj = xmlXPathEvalExpression((xmlChar *)			\
				       ssPubSchemeTagProp.tag_parser[0].tag_names,
				       xml->xpath);
    if (xpath_obj->nodesetval->nodeNr) {
	ss->key_frame_interval =
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    } else {
        if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }

    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       ssPubSchemeTagProp.tag_parser[1].tag_names,
				       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
	err = -E_MFP_PMF_INVALID_TAG;
	xmlXPathFreeObject(xpath_obj);
	return err;
    }
    strcpy((char*)pub->ss_store_url,
	    ((char*)xpath_obj->nodesetval->nodeTab[0]->children->content)); 
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
				       ssPubSchemeTagProp.tag_parser[2].tag_names,
				       xml->xpath);
    if (xpath_obj->nodesetval->nodeNr) {

	strcpy((char*)pub->ss_delivery_url,
	       (char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	//if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }

    if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
    	ssPubSchemeTagProp.tag_parser[3].tag_names,
    	xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr) {
    	xmlXPathFreeObject(xpath_obj);
    } else { 
	ss->dvr = 0;
	if (!strcasecmp((char*)xpath_obj->nodesetval->nodeTab[0]->
		children->content,"on"))
	   ss->dvr = 1;
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }

    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
                                       ssPubSchemeTagProp.tag_parser[4].tag_names,
                                       xml->xpath);
    /**
     * if the disk usage parameter is set for the stream then use it;
     * if it is zero or absent inherit from the global op cfg
     */
    if (xpath_obj->nodesetval->nodeNr) {
        ss->disk_usage = atoi((char *)\
                              xpath_obj->nodesetval->nodeTab[0]->children->content);
        if (ss->disk_usage > 0) {
            err = mfp_safe_io_init(NULL, 0.0, ss->disk_usage,
                                       &ss->sioc);
            if (err) {
                xmlXPathFreeObject(xpath_obj);
                return err;
            }
        } else if (ss->disk_usage == 0) {
            ss->sioc = pub->op_cfg.sioc;
        }
    } else {
        ss->sioc = pub->op_cfg.sioc;
    }

    xmlXPathFreeObject(xpath_obj);

    xpath_obj = xmlXPathEvalExpression((xmlChar *)\
                                       ssPubSchemeTagProp.tag_parser[5].tag_names,
                                       xml->xpath);
    if (xpath_obj->nodesetval->nodeNr) {
	ss->dvr_seconds = atoi((char *)\
			       xpath_obj->nodesetval->nodeTab[0]->children->content);
	ss->dvr_seconds*=60;
    }
    xmlXPathFreeObject(xpath_obj);

    return err;
}

/**
 * fill the flash streaming properties
 * @param obj - xml object pointer containing the property node
 * @param ss - the smooth stremaing parameter context
 */
int32_t populateFlashStreamingProps(xml_read_ctx_t *xml,
				    mfp_publ_t *pub)
{
    int32_t err;
    xmlXPathObject *xpath_obj;
    flash_parm_t *fs;

    err = 0;
    fs = &pub->fs_parm;

    fs->status = FMTR_ON;

    {
	//parse fragment duration
	xpath_obj = xmlXPathEvalExpression((xmlChar *)			\
		fsPubSchemeTagProp.tag_parser[0].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    err = -E_MFP_PMF_INVALID_TAG;
	    xmlXPathFreeObject(xpath_obj);
	    return err;
	}
	fs->fragment_duration= 
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    }
    {
	//parse segment duration
	xpath_obj = xmlXPathEvalExpression((xmlChar *)			\
		fsPubSchemeTagProp.tag_parser[1].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    err = -E_MFP_PMF_INVALID_TAG;
	    xmlXPathFreeObject(xpath_obj);
	    return err;
	}
	fs->segment_duration= 
	    atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content);
	if(fs->segment_duration < fs->fragment_duration){
	    err = -E_MFP_PMF_INVALID_ARG;
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	    return err;		
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }	
    {

	//parse storage URL
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[2].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    err = -E_MFP_PMF_INVALID_TAG;
	    xmlXPathFreeObject(xpath_obj);
	    return err;
	}
	strcpy((char*)pub->fs_store_url,
		((char*)xpath_obj->nodesetval->nodeTab[0]->children->content)); 
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    }
    {
	//parse deliery URL
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[3].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    err = -E_MFP_PMF_INVALID_TAG;
	    xmlXPathFreeObject(xpath_obj);
	    return err;
	}
	strcpy((char*)pub->fs_delivery_url, 
		(char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);

    }
    {
	//parse stream Type parameter
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[4].tag_names,
		xml->xpath);

	fs->stream_type = ZERI_VOD;

	if (!xpath_obj->nodesetval->nodeNr) {    	
	    xmlXPathFreeObject(xpath_obj);
	} else { 
	    if(!strcasecmp((char*)xpath_obj->nodesetval->nodeTab[0]->children->content,"live"))
		fs->stream_type = ZERI_LIVE;
	    if(!strcasecmp((char*)xpath_obj->nodesetval->nodeTab[0]->children->content,"dvr"))
		fs->stream_type = ZERI_LIVE_DVR;
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }
    {
	//parse DVRSeconds parameter
	xpath_obj = xmlXPathEvalExpression((xmlChar *)			\
		fsPubSchemeTagProp.tag_parser[5].tag_names,
		xml->xpath);
	
	fs->dvr_seconds = 0;
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	}else{
	    fs->dvr_seconds= 
		atoi((char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}

    }

    //parse encryption -on or off
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[6].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {    	
	    xmlXPathFreeObject(xpath_obj);
	} else { 
	    fs->encryption= 0;
	    if(!strcasecmp((char*)xpath_obj->nodesetval->nodeTab[0]->children->content,"on"))
		fs->encryption = 1;
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse common key file name
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[7].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->common_key, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse license server url
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[8].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->license_server_url, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse license server certificate file name
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[9].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->license_server_cert, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse transport certificate file name
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[10].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->transport_cert, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse packager certificate file name
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[11].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->packager_cert, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse packager credential password - string
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[12].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->credential_pwd, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse policy file name
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[13].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->policy_file, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse conent id - string
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[14].tag_names,
		xml->xpath);
	if (!xpath_obj->nodesetval->nodeNr) {
	    xmlXPathFreeObject(xpath_obj);
	    if(fs->encryption == 1){
		err = -E_MFP_PMF_INVALID_TAG;
		return err;
	    }
	}else {
	    strcpy((char*)fs->content_id, 
		    (char*)xpath_obj->nodesetval->nodeTab[0]->children->content); 
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    //parse OrigModReq -on or off
    //user input to decided to produce zeri assets that require http origin modue
    //default is off; ON produces assets that require http origin module
    
    {
	xpath_obj = xmlXPathEvalExpression((xmlChar *)\
		fsPubSchemeTagProp.tag_parser[15].tag_names,
		xml->xpath);
	fs->orig_mod_req= 0;
	if (!xpath_obj->nodesetval->nodeNr) {    	
	    xmlXPathFreeObject(xpath_obj);
	} else { 
	    if(!strcasecmp((char*)xpath_obj->nodesetval->nodeTab[0]->children->content,"on"))
		fs->orig_mod_req = 1;
	    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
	}
    }

    return err;
}

/**
 * convert string in uri format e.g.
 * <transport>://<domain/ip>:<port>
 * into a sock addr structure
 * @param str - string in human readable form
 * @param sin [out] - fully populated sockaddr_in structure will be
 * returned
 * @return returns 0 on success and a negative number on error
 */
static int32_t convertStrToLiveSrc(const char *src, 
				   struct sockaddr_in *saddr)
{
    char *p, *p1, *p2, *p3, *p4;
    char uri[256], port[10];
    int32_t err, uri_len, port_len;

    /* initialization */
    err = 0;
    uri_len = 0;
    port_len = 0;
    p = (char *)src;

    /* check for unicast/ multicast */
    p1 = strchr(p, '@');
    if (p1) {
	/* find the from address and port */
	/* extract the domain and port from the uri */
	if ( (p2 = strstr(p1 + 1, ":")) ) {
	    p3 = p2 + 1;
	    p4 = strstr(p3 + 1, "/");
	    if (!p4) {
		p4 = (p3) +
		    (strlen(p3));
	    }
	    port_len = p4 - p3;
	} else if ( (p2 = strstr(p1 + 1, "/")) ){
	    /* handle trailing slash */
	    ;
	} else {
	    /* the end of the uri is the end of the string */
	    p2 = (p1 + 1) + (strlen(p1 + 1) - 1);
	}
	uri_len = p2 - (p1 + 1);

	/* copy the uri and port into temporary strings*/
	memcpy(uri, p1 + 1, uri_len);
	memcpy(port, p3, port_len);
	uri[uri_len] = '\0';
	port[port_len] = '\0';

	/* fill the sockaddr_in structure */
	inet_pton(AF_INET, (const char *)uri, &saddr->sin_addr);
	saddr->sin_family = AF_INET;
	saddr->sin_port = htons(atoi(port));
    }

    return err;
}

/** convert string to Encaupsulation type enumeration
 * @param str - string in human readable form
 * @return returns the enumeration on success and a negative value on
 * error
 */			   
static int32_t convertStrToEncapsType(const char *str)
{
    if (!strcasecmp(str, "mpeg2-ts")) {
	return TS;
    } else if (!strcasecmp(str, "SmoothStreamingPackage")) {
	return ISMV_MBR;
    } else if (!strcasecmp(str, "MP4-streams")) {
	return MP4;
    } else {
	return -E_MFP_PMF_UNSUPPORTED_TYPE;
    } 

}

/**
 * converts string to media type enumeration
 * @param str - string in human readable form
 * @return returns the enumeration on success and a negative value on
 * error
 */
static int32_t convertStrToMediaType(const char *str)
{
    if (!strcasecmp(str, "video")) {
	return VIDEO;
    } else if (!strcasecmp(str, "audio")) {
	return AUDIO;
    } else if (!strcasecmp(str, "mux")) {
	return MUX;
    } else if (!strcasecmp(str, "svr-manifest")) {
	return SVR_MANIFEST;
    } else if (!strcasecmp(str, "client-manifest")) {
	return CLIENT_MANIFEST;
    } else if (!strcasecmp(str, "zeri-manifest")) {
	return ZERI_MANIFEST;
    } else if (!strcasecmp(str, "zeri-index")) {
	return ZERI_INDEX;
    } else {
	return -E_MFP_PMF_UNSUPPORTED_TYPE;
    }
}

/**
 * converts string to transport type enumeration
 * @param str - string in human readable form
 * @return returns the enumeration on success and a negative value on
 * error
 */
static int32_t convertStrToTransportType(const char *str)
{
    if (!strcasecmp(str, "UDP/IP")) {
	return UDP_IP;
    } else {
	return -E_MFP_PMF_UNSUPPORTED_TYPE;
    }
}


/**
 * Input: Valid IP address
 * Output: "1" if it is a local or global scope multicast address else "-ve"
 */
static int8_t isMulticast(const char* addr) 
{

    if (strncmp(addr, "udp://@", 7) != 0)
	return -1;

    char const* p1 = NULL; 
    char const* p2 = NULL; 
    p1 = strchr(addr, '@');
    if (p1 != NULL) {
	p2 = strchr(p1, '.');
	if (p2 == NULL)
	    return -1;
    } else
	return -1;

    char addr_w1[p2-p1];
    strncpy(addr_w1, p1 + 1, (p2-p1-1));

    uint32_t maddr_w1 = strtol(addr_w1, NULL, 0);
    if (maddr_w1 == 0)
	return -1;
    if ((maddr_w1 < 224) || (maddr_w1 > 239))
	return -1;

    return 1;
}

static int8_t isSSM_IP(char const* ip_addr) {

    if (strncmp(ip_addr, "udp://@", 7) != 0)
	return -1;

    char const* p1 = NULL;
    char const* p2 = NULL;
    p1 = strchr(ip_addr, '@');
    if (p1 != NULL) {
	p2 = strchr(p1, ':');
	if (p2 == NULL)
	    return -1;
    } else
	return -1;

    char ip_addr_dup[p2-p1];
    strncpy(ip_addr_dup, p1 + 1, (p2-p1-1));

	char* word = NULL, *save_ptr, *process_str = ip_addr_dup;
	uint32_t w1, w2, w3, w4, w_count;
	w1 = w2 = w3 = w4 = w_count = 0;
	do {
		word = strtok_r(process_str, ".", &save_ptr); 
		if (word == NULL)
			break;
		w_count++;
		if (w_count == 1)
			w1 = atoi(word);
		else if (w_count == 2)
			w2 = atoi(word);
		else if (w_count == 3)
			w3 = atoi(word);
		else if (w_count == 4)
			w4 = atoi(word);
		else
			break;
		process_str = save_ptr;
	} while (word != NULL);

	int8_t rc = 0;
	if (w_count == 4) {
		if (w1 == 232)
			if (w2 < 256)
				if (w3 < 256)
					if (w4 < 256)
						rc = 1;
	}
	return rc;
}


static int32_t getAuthTypeFromStr(const char *str)
{
    if (!strcasecmp(str, "VERIMATRIX")) {
	return VERIMATRIX;
	} else if (!strcasecmp(str, "NATIVE")) {
	return NATIVE;
    } else {
	return -E_MFP_PMF_INVALID_TAG;
    }
}

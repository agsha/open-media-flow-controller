/*
 *
 * Filename:  nkn_vpe_ism_read_api.h
 * Date:      2010/05/13
 * Module:    ism xml read API implementation
 *
 * (C) Copyright 2020 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _ISM_READ_API_
#define _ISM_READ_API_

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif
    /** \def number of track types allowed */
#define MAX_TRAK_TYPES 3
#define MAX_ATTR 10
#define ISM_USE_DEF_QPARAM 1

    typedef struct tag_xml_read_ctx{
	xmlDoc *doc;
	xmlNode *root;
	xmlXPathContext *xpath;
    }xml_read_ctx_t;

    typedef struct tag_ism_cust_attr {
        char *name;
        char *val;
        TAILQ_ENTRY(tag_ism_cust_attr) entries;
    }ism_cust_attr_t;

   typedef struct tag_ism_av_props {
       uint64_t bitrate;
       uint32_t track_id;
       char *video_name;
       uint32_t n_cust_attrs;
       TAILQ_HEAD(, tag_ism_cust_attr) cust_attr_head;
    }ism_av_props_t;

    typedef struct tag_ism_av_attr {
	uint32_t n_profiles;
	ism_av_props_t prop[MAX_TRACKS];
    }ism_av_attr_t;

    typedef struct tag_ism_bitrate_map {
	ism_av_attr_t *attr[MAX_TRAK_TYPES];
	char *ismc_name;
    }ism_bitrate_map_t;
    
    /**
     * reads the bitrate map from an ism file
     * @param *ism_file - path to the ISM file
     * @param out - the bitrate map which will be read from the ISM
     * file
     * @return  returns 0 on error and negative integer on error
     */
    int32_t
    ism_read_map_from_file(char* ism_file,
			   ism_bitrate_map_t **out, io_handlers_t  *iocb);

	
    /** 
     * read the [bitrate, trak_id] map from a valid ism file
     * @param ctx -  xml context obtained with a call to
     * 'init_xml_read'
     * @return - returns the populated [bitrate, trak_in] structure;
     * NULL if error
     */
    ism_bitrate_map_t* ism_read_bitrate_map(xml_read_ctx_t *ctx);

    /**
     * find a track id for a given bitrate in the [bitrate, track_id]
     * map
     * @param map - a valid map populated by a prior call to
     * 'ism_read_bitrate_map'
     * @param bitrate - the bitrate whose track id needs to be found
     * @param trak_type - the trak type '0' for Video, '1' for audio
     * @return - returns a valid trak_id (non - zero +ve number); '0'
     * if unable to find the bitrate
     */
    uint32_t ism_get_track_id(ism_bitrate_map_t *map, uint32_t bitrate,
			      uint32_t trak_type);

    /** finds the video name for a give trak_id
     * @param map - a valid map populated by a prior call to
     * 'ism_read_bitrate_map'
     * @param trak_type - the trak type '0' for Video, '1' for audio
     * @param trak_id - the trak id for which the video name needs to
     *                  be found
     * @param bitrate - the bitrate can be an additional parameter to
     *                  search for the video name. if this is '0' then
     *                  only the trak_id is used as a search parameter
     * @return returns the video name for a give trak_id; NULL otherwise
     */
     const char * ism_get_video_name(ism_bitrate_map_t *map, uint32_t trak_type, 
				     uint32_t trak_id, uint32_t
				     bitrate, char *ismc_attr_name[],
				     char * ismc_attr_val[], uint32_t
				     ca_ismc_attr_count);

    /** 
     * find the video query param name, if avaiable as the "trackName"
     * attribute in the custom attributes
     * @param map - the ism bitrate map struture
     * @param bitrate - bitrate for which the search is performed
     * @param video_qparam_name - reference to the query param name if
     *                            available in the custom attribute
     *                            list
     * @return returns ISM_USE_DEF_QPARAM if the "trackName" attribute
     * is not found, 0 otherwise
     */
    int32_t ism_get_video_qparam_name(ism_bitrate_map_t *map,
				      uint32_t bitrate,
				      const char **video_qparam_name);

    /** 
     * find the video query param name, if avaiable as the "trackName"
     * attribute in the custom attributes
     * @param map - the ism bitrate map struture
     * @param bitrate - bitrate for which the search is performed
     * @param video_qparam_name - reference to the query param name if
     *                            available in the custom attribute
     *                            list
     * @return returns ISM_USE_DEF_QPARAM if the "trackName" attribute
     * is not found, 0 otherwise
     */
    int32_t ism_get_audio_qparam_name(ism_bitrate_map_t *map,
				      uint32_t bitrate,
				      const char **audio_qparam_name);

    /** 
     * cleanup the map structure and all its associated resources
     * @param - a valid map structure
     * return returns nothing
     */
    void ism_cleanup_map(ism_bitrate_map_t *map);
    
    /*****************************************************************
     *                 XML HELPER FUNCTIONS
     ****************************************************************/
    xml_read_ctx_t* init_xml_read_ctx(uint8_t *p_xml, size_t size);
    void xml_print_elements(xmlNode *node);
    xmlNode* xml_read_till_node(xmlNode *node, const char *name);
    xmlAttr* xml_read_till_attr(xmlNode *node, const char *name);
    void xml_cleanup_ctx(xml_read_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif

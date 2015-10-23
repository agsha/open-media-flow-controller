#ifndef __NVSD_MGMT_PUBLISH_POINT__H
#define __NVSD_MGMT_PUBLISH_POINT__H

/*
 *
 * Filename:  nvsd_mgmt_publish_point.h
 * Date:      2009/08/19
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "nkn_common_config.h"

typedef enum pub_server_mode {
	PUB_SERVER_MODE_NONE = 0,
	PUB_SERVER_MODE_ON_DEMAND = 1,
	PUB_SERVER_MODE_IMMEDIATE = 2,
	PUB_SERVER_MODE_TIMED = 3

} pub_server_mode;

typedef struct pub_point_node_data_st {

	char		*name;
	boolean		enable;
	char 		*event_duration; 	/* String in format: HH:MM:SS */
	char		*server_uri;
	pub_server_mode	mode;
	char		*sdp_static;
	char 		*start_time;
	char		*end_time;
	boolean		caching_enabled;
	boolean		cache_format_chunked_ts;
	boolean		cache_format_moof;
	boolean		cache_format_frag_mp4;
	boolean		cache_format_chunk_flv;
} pub_point_node_data_t;

typedef pub_point_node_data_t pub_point_data_t;


/* function prototypes */

pub_point_node_data_t* nvsd_mgmt_get_publish_point (char *cpPublishPoint);


#endif /* __NVSD_MGMT_PUBLISH_POINT__H */

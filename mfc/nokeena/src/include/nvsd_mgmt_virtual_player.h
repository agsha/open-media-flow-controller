#ifndef __NVSD_MGMT_VIRTUAL_PLAYER__H
#define __NVSD_MGMT_VIRTUAL_PLAYER__H

/*
 *
 * Filename:  nvsd_mgmt_virtual_player.h
 * Date:      2009/1/14 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-09 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "nkn_common_config.h"


/* Macros */
#define	MAX_RATE_MAPS	50
#define	MAX_FILE_TYPE	2

/* Enums */
typedef enum vp_param_type_en
{
	VP_PARAM_TYPE_URI_QUERY = 0,
	VP_PARAM_TYPE_SIZE = 1,
	VP_PARAM_TYPE_TIME = 2,
	VP_PARAM_TYPE_RATE = 3,
	VP_PARAM_TYPE_AUTO = 4,

} vp_param_type_en;

typedef enum vp_rc_scheme_en
{
    RC_MBR = 1,
    RC_AFR,
}vp_rc_scheme_en;

/* Types */
typedef	struct	hash_verify_node_data_st
{
	boolean	active;
	char	*digest;
	char	*data_string;
	int	data_uri_offset;
	int	data_uri_len;
	char	*secret;
	char	*secret_type;
	char	*match_uri_query;
	char	*expiry_time_query;
	char	*url_format;
} hash_verify_node_data_t;

typedef	struct	fast_start_node_data_st
{
	boolean	active;
	char	*uri_query;
	int	size;
	int	time;
	vp_param_type_en	active_type;
} fast_start_node_data_t;

typedef	struct	seek_node_data_st
{
	boolean	active;
	boolean enable_tunnel;
	boolean use_9byte_hdr;
	char	*uri_query;
        char    * length_uri_query;
	char	*flv_type;
	char	*mp4_type;
} seek_node_data_t;

typedef	struct	full_download_node_data_st
{
	boolean	active;
	char	*uri_query;
	boolean	always;
	char	*match;
	char	*uri_hdr;
} full_download_node_data_t;

typedef	struct	assured_flow_node_data_st
{
	boolean	active;
	char	*uri_query;
	boolean	auto_flag;
	int	rate;
	boolean use_mbr;
	char *overhead;	 // Delivery overhead
	vp_param_type_en	active_type;
} assured_flow_node_data_t;

typedef	struct	smooth_flow_node_data_st
{
	boolean	active;
	char	*uri_query;
} smooth_flow_node_data_t;

typedef	struct	rate_map_node_data_st
{
	boolean	active;
	char	*match;
	int	rate;
	char	*query_string;
	int	uol_offset;
	int	uol_length;
} rate_map_node_data_t;

typedef struct health_probe_node_data_st
{
	boolean	active;
	char	*uri_query;
	char	*match;
} health_probe_node_data_t;

typedef struct req_auth_node_data_st
{
	boolean	active;
	char	*digest;
	char	*secret_value;
	int	time_interval;
	char	*stream_id_uri_query;
	char	*auth_id_uri_query;
	char	*match_uri_query;
} req_auth_node_data_t;

typedef struct signals_node_data_st
{
	boolean	active;
	char	*session_id_uri_query;
	char	*state_uri_query;
	char	*profile_uri_query;
	char	*chunk_uri_query;
} signals_node_data_t;

typedef struct prestage_node_data_st
{
	boolean	active;
	char	*indicator_uri_query;
	char	*namespace_uri_query;
} prestage_node_data_t;

typedef struct video_id_node_data_st
{
	boolean	active;
	char 	*video_id_uri_query;
	char 	*format_tag;
} video_id_node_data_t;

/* used for virtual player type 6 */
typedef struct smoothstream_pub_node_data_st
{
	char    *quality_tag;
	char    *fragment_tag;
} smoothstream_pub_node_data_t;

/* used for virtual player type 6 */
typedef struct flashstream_pub_node_data_st
{
	char    *segment_tag;
	char    *segment_frag_delimiter;
	char    *fragment_tag;
} flashstream_pub_node_data_t;


typedef struct bw_file_type_node_data_st
{
    char *file_type;
    char *transcode_comp_ratio;
    char *switch_rate;
    int hotness_threshold;
    int switch_limit_rate;

} bw_file_type_node_data_t;

/* bandwith-opt settings based on file-type*/
typedef struct bandwidth_opt_node_data_st
{
    boolean active;
    bw_file_type_node_data_t file_info[MAX_FILE_TYPE];
} bandwidth_opt_node_data_t;

/*rate control options */
typedef struct rc_opt_node_data_st
{
    boolean active;
    boolean auto_flag;
    vp_param_type_en  active_type;
    vp_rc_scheme_en scheme;
    char    *qstr;
    char    *qrate;
    char *burst;
    int     static_rate;
}rc_opt_node_data_t;

typedef struct virtual_player_node_data_st
{
	/* These are nodes in the virtual_player module */
	char	*name;
	int	type;
	int	connection_max_bandwidth;

	hash_verify_node_data_t		hash_verify;
	fast_start_node_data_t		fast_start;
	seek_node_data_t		seek;
	full_download_node_data_t	full_download;
	assured_flow_node_data_t	assured_flow;
	smooth_flow_node_data_t		smooth_flow;

	boolean				rate_map_active;
	rate_map_node_data_t		rate_map [MAX_RATE_MAPS];
	char				*control_point;
	health_probe_node_data_t	health_probe;
	req_auth_node_data_t		req_auth;
	signals_node_data_t		signals;
	prestage_node_data_t		prestage;
	video_id_node_data_t		video_id;
	smoothstream_pub_node_data_t    smoothstream;
	flashstream_pub_node_data_t     flashstream;
	bandwidth_opt_node_data_t	bw_opt;
	rc_opt_node_data_t              rate_control;
} virtual_player_node_data_t;

/* Function Prototypes */
virtual_player_node_data_t* nvsd_mgmt_get_virtual_player (char *cpVirtualPlayer);

#endif // __NVSD_MGMT_VIRTUAL_PLAYER__H

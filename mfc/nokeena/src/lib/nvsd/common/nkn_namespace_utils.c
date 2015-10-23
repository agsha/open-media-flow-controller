/*
 *******************************************************************************
 * nkn_namespace_utils.c -- Public namespace object constructors/destructors
 *******************************************************************************
 */
#include "stdlib.h"
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>


#include "nkn_namespace.h"
#include "nkn_namespace_utils.h"
#include "nkn_debug.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_hash.h"
#include "nkn_cfg_params.h"

static const char *pre_html_body = 
	"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
	"<HTML>"
	  "<HEAD>"
	    "<TITLE>200 OK</TITLE>"
	    "<BODY>";

static const char *post_html_body = 
	    "</BODY>"
	  "</HEAD>"
	"</HTML>";

void pe_cleanup(void * plist);

static int setup_origin_server_map(const host_origin_map_bin_t *hom, 
			    	   hash_entry_t **hash_hostnames,
			       	   hash_table_def_t **ht_hostnames);

static int setup_origin_nodemap(namespace_node_data_t *nd, 
			 	http_config_t *httpcfg, 
				namespace_config_t *nsc,
				const namespace_config_t *old_nsc);

int glob_url_filter_read = 0;

ssp_config_t *new_ssp_config_t(namespace_node_data_t *nd)
{
    int i;

    ssp_config_t *ssp = (ssp_config_t *)nkn_calloc_type(1, sizeof(ssp_config_t),
    						mod_ns_ssp_config_t);

    if (!ssp) {
    	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	return 0;
    }

    virtual_player_node_data_t *vp = nd->virtual_player;

    if (nd->virtual_player_name && nd->virtual_player!= NULL){

	// Virtual Player name
	if (nd->virtual_player_name &&
	    strncmp(nd->virtual_player_name, "", 1) != 0 ) {
	    ssp->player_name 		= nkn_strdup_type(nd->virtual_player_name, mod_ssp_cli_strdup);
	}
	else {
	    delete_ssp_config_t(ssp);
	    return 0;
	}

	// Player type
	ssp->player_type 			= vp->type;

	// Hash Authentication
	ssp->hash_verify.enable = vp->hash_verify.active;

	if (vp->hash_verify.digest && strncmp(vp->hash_verify.digest, "", 1) != 0 )
	    ssp->hash_verify.digest_type 	= nkn_strdup_type(vp->hash_verify.digest, mod_ssp_cli_strdup);
	if (vp->hash_verify.data_string && strncmp(vp->hash_verify.data_string , "", 1) != 0)
	    ssp->hash_verify.datastr 		= nkn_strdup_type(vp->hash_verify.data_string, mod_ssp_cli_strdup);

	ssp->hash_verify.uol_offset 		= vp->hash_verify.data_uri_offset;
	ssp->hash_verify.uol_length 		= vp->hash_verify.data_uri_len;

	if (vp->hash_verify.secret && strncmp(vp->hash_verify.secret, "", 1) != 0)
	    ssp->hash_verify.secret_key 	= nkn_strdup_type(vp->hash_verify.secret, mod_ssp_cli_strdup);
	if (vp->hash_verify.secret_type && strncmp(vp->hash_verify.secret_type, "", 1) != 0)
	    ssp->hash_verify.secret_position 	= nkn_strdup_type(vp->hash_verify.secret_type, mod_ssp_cli_strdup);
	if (vp->hash_verify.match_uri_query && strncmp(vp->hash_verify.match_uri_query, "", 1) != 0)
	    ssp->hash_verify.uri_query 		= nkn_strdup_type(vp->hash_verify.match_uri_query, mod_ssp_cli_strdup);
	if (vp->hash_verify.expiry_time_query)
            ssp->hash_verify.expirytime_query   = nkn_strdup_type(vp->hash_verify.expiry_time_query, mod_ssp_cli_strdup);
        if (vp->hash_verify.url_format)
            ssp->hash_verify.url_type           = nkn_strdup_type(vp->hash_verify.url_format, mod_ssp_cli_strdup);


	// Fast start
	ssp->fast_start.enable = vp->fast_start.active ;

	if (vp->fast_start.uri_query && strncmp(vp->fast_start.uri_query, "", 1) != 0)
	    ssp->fast_start.uri_query 		= nkn_strdup_type(vp->fast_start.uri_query, mod_ssp_cli_strdup);
	ssp->fast_start.size 			= vp->fast_start.size * KBYTES; //bytes
	ssp->fast_start.time 			= vp->fast_start.time;
	ssp->fast_start.active_param            = vp->fast_start.active_type;

	// Max connection bandwidth
	ssp->con_max_bandwidth 			= vp->connection_max_bandwidth * KBYTES/8; //kbps to bytes/sec

	// Seek
	ssp->seek.enable = vp->seek.active;
	ssp->seek.activate_tunnel = vp->seek.enable_tunnel;
	if (vp->seek.uri_query && strncmp(vp->seek.uri_query, "", 1) != 0)
	    ssp->seek.uri_query 		= nkn_strdup_type(vp->seek.uri_query, mod_ssp_cli_strdup);
	if (vp->seek.length_uri_query && strncmp(vp->seek.length_uri_query, "", 1) != 0)
	    ssp->seek.query_length 		= nkn_strdup_type(vp->seek.length_uri_query, mod_ssp_cli_strdup);
	if (vp->seek.flv_type && strncmp(vp->seek.flv_type, "", 1) != 0)
	    ssp->seek.flv_off_type = nkn_strdup_type(vp->seek.flv_type,
						mod_ssp_cli_strdup);
	if (vp->seek.mp4_type && strncmp(vp->seek.mp4_type, "", 1) != 0)
            ssp->seek.mp4_off_type = nkn_strdup_type(vp->seek.mp4_type,
                                                mod_ssp_cli_strdup);
#if 0 /* Assured Flow is replaced by Rate Control */
	// Assured Flow
	ssp->assured_flow.enable = vp->assured_flow.active;

	if (vp->assured_flow.uri_query && strncmp(vp->assured_flow.uri_query, "", 1) != 0)
	    ssp->assured_flow.uri_query 	= nkn_strdup_type(vp->assured_flow.uri_query, mod_ssp_cli_strdup);
	ssp->assured_flow.automatic 		= vp->assured_flow.auto_flag;
	ssp->assured_flow.rate 			= vp->assured_flow.rate * KBYTES/8; //kbps to bytes/sec
	ssp->assured_flow.use_mbr = vp->assured_flow.use_mbr;
        if (vp->assured_flow.overhead)
            ssp->assured_flow.overhead     = nkn_strdup_type(vp->assured_flow.overhead, mod_ssp_cli_strdup);
	else
	    ssp->assured_flow.overhead = vp->assured_flow.overhead;
	ssp->assured_flow.active_param          = vp->assured_flow.active_type;
#else
	// Rate Control ()
	ssp->rate_control.active = vp->rate_control.active;
	ssp->rate_control.scheme = vp->rate_control.scheme; // MBR/AFR
	ssp->rate_control.active_param = vp->rate_control.active_type;
	if (vp->rate_control.qstr && strncmp(vp->rate_control.qstr, "", 1) != 0) {
	    ssp->rate_control.qstr = nkn_strdup_type(vp->rate_control.qstr, mod_ssp_cli_strdup);
	}
	if (vp->rate_control.qrate && strncmp(vp->rate_control.qrate, "", 1) != 0) {
	    if (strncmp(vp->rate_control.qrate, "Kbps", 4) == 0) {
		ssp->rate_control.qrate_unit = KBYTES/8;
	    } else if (strncmp(vp->rate_control.qrate, "KBps", 4) == 0) {
		ssp->rate_control.qrate_unit = KBYTES;
	    } else if (strncmp(vp->rate_control.qrate, "Mbps", 4) == 0) {
		ssp->rate_control.qrate_unit = MBYTES/8;
	    } else if (strncmp(vp->rate_control.qrate, "MBps", 4) == 0) {
		ssp->rate_control.qrate_unit = MBYTES;
	    } else { /* default value */
		ssp->rate_control.qrate_unit = KBYTES/8;
	    }
	}

	ssp->rate_control.static_rate = vp->rate_control.static_rate;

	if (vp->rate_control.burst && strncmp(vp->rate_control.burst, "",1) != 0) {
	    ssp->rate_control.burst_factor = atof(vp->rate_control.burst);
	}

#endif

	// Full download
	ssp->full_download.enable = vp->full_download.active;
	ssp->full_download.always 		= vp->full_download.always;
	if (vp->full_download.match && strncmp(vp->full_download.match, "", 1) != 0)
	    ssp->full_download.matchstr 	= nkn_strdup_type(vp->full_download.match, mod_ssp_cli_strdup);
	if (vp->full_download.uri_query && strncmp(vp->full_download.uri_query, "", 1) != 0)
	    ssp->full_download.uri_query 	= nkn_strdup_type(vp->full_download.uri_query, mod_ssp_cli_strdup);
	if (vp->full_download.uri_hdr && strncmp(vp->full_download.uri_hdr, "", 1) != 0)
	    ssp->full_download.uri_hdr 		= nkn_strdup_type(vp->full_download.uri_hdr, mod_ssp_cli_strdup);

	// Smooth flow
	ssp->smooth_flow.enable = vp->smooth_flow.active ;
	if (vp->smooth_flow.uri_query && strncmp(vp->smooth_flow.uri_query, "", 1) != 0)
	    ssp->smooth_flow.uri_query	= nkn_strdup_type(vp->smooth_flow.uri_query, mod_ssp_cli_strdup);

	// Profile Rate Map
	i = 0;
	ssp->rate_map.enable = vp->rate_map_active;
	while ( i < MAX_RATE_MAP_ENTRIES && vp->rate_map[i].rate > 0) {
	    ssp->rate_map.profile_list[i].rate = vp->rate_map[i].rate * KBYTES/8; //kbps to bytes/sec
	    if (vp->rate_map[i].match && strncmp(vp->rate_map[i].match, "", 1) != 0)
		ssp->rate_map.profile_list[i].matchstr = nkn_strdup_type(vp->rate_map[i].match, mod_ssp_cli_strdup);
	    i++;
	}
	ssp->rate_map.entries = i;

	// Health Probe
	ssp->health_probe.enable = vp->health_probe.active;
	if (vp->health_probe.uri_query && strncmp(vp->health_probe.uri_query, "", 1) != 0)
	    ssp->health_probe.uri_query = nkn_strdup_type(vp->health_probe.uri_query, mod_ssp_cli_strdup);
	if (vp->health_probe.match && strncmp(vp->health_probe.match, "", 1) != 0)
	    ssp->health_probe.matchstr = nkn_strdup_type(vp->health_probe.match, mod_ssp_cli_strdup);

	// Req Authentication (Yahoo)
	ssp->req_auth.enable = vp->req_auth.active;
	ssp->req_auth.time_interval = vp->req_auth.time_interval;
	if (vp->req_auth.digest && strncmp(vp->req_auth.digest, "", 1) != 0)
	    ssp->req_auth.digest_type = nkn_strdup_type(vp->req_auth.digest, mod_ssp_cli_strdup);
	if (vp->req_auth.secret_value && strncmp(vp->req_auth.secret_value, "", 1) != 0)
	    ssp->req_auth.shared_secret = nkn_strdup_type(vp->req_auth.secret_value, mod_ssp_cli_strdup);
	if (vp->req_auth.stream_id_uri_query && strncmp(vp->req_auth.stream_id_uri_query, "", 1) != 0)
	    ssp->req_auth.stream_id_uri_query = nkn_strdup_type(vp->req_auth.stream_id_uri_query, mod_ssp_cli_strdup);
	if (vp->req_auth.auth_id_uri_query && strncmp(vp->req_auth.auth_id_uri_query, "", 1) != 0)
	    ssp->req_auth.auth_id_uri_query = nkn_strdup_type(vp->req_auth.auth_id_uri_query, mod_ssp_cli_strdup);
	if (vp->req_auth.match_uri_query && strncmp(vp->req_auth.match_uri_query, "", 1) != 0)
	    ssp->req_auth.match_uri_query = nkn_strdup_type(vp->req_auth.match_uri_query, mod_ssp_cli_strdup);

	// Smoothflow Type 4
	ssp->sf_signals.enable = vp->signals.active;
	if (vp->control_point && strncmp(vp->control_point, "", 1) != 0)
	    ssp->sf_control = nkn_strdup_type(vp->control_point, mod_ssp_cli_strdup);
	if (vp->signals.session_id_uri_query && strncmp(vp->signals.session_id_uri_query, "", 1) != 0)
	    ssp->sf_signals.session_id_uri_query = nkn_strdup_type(vp->signals.session_id_uri_query, mod_ssp_cli_strdup);
	if (vp->signals.state_uri_query && strncmp(vp->signals.state_uri_query, "", 1) != 0)
	    ssp->sf_signals.state_uri_query = nkn_strdup_type(vp->signals.state_uri_query, mod_ssp_cli_strdup);
	if (vp->signals.profile_uri_query && strncmp(vp->signals.profile_uri_query, "", 1) != 0)
	    ssp->sf_signals.profile_uri_query = nkn_strdup_type(vp->signals.profile_uri_query, mod_ssp_cli_strdup);
	if (vp->signals.chunk_uri_query && strncmp(vp->signals.chunk_uri_query, "", 1) != 0)
	    ssp->sf_signals.chunk_uri_query = nkn_strdup_type(vp->signals.chunk_uri_query, mod_ssp_cli_strdup);

	// Youtube Query parameters Type 5
	ssp->video_id.enable = vp->video_id.active;
	if (vp->video_id.video_id_uri_query && strncmp(vp->video_id.video_id_uri_query, "", 1) != 0)
	    ssp->video_id.video_id_uri_query = nkn_strdup_type(vp->video_id.video_id_uri_query, mod_ssp_cli_strdup);
	if (vp->video_id.format_tag && strncmp(vp->video_id.format_tag, "", 1) != 0)
            ssp->video_id.format_tag = nkn_strdup_type(vp->video_id.format_tag, mod_ssp_cli_strdup);

	// Smoothstream-pub Type 6
	if (vp->smoothstream.quality_tag && strncmp(vp->smoothstream.quality_tag, "", 1) != 0)
	  ssp->smoothstream_pub.quality_tag = nkn_strdup_type(vp->smoothstream.quality_tag, mod_ssp_cli_strdup);
	if (vp->smoothstream.fragment_tag && strncmp(vp->smoothstream.fragment_tag, "", 1) != 0)
	  ssp->smoothstream_pub.fragment_tag = nkn_strdup_type(vp->smoothstream.fragment_tag, mod_ssp_cli_strdup);

	// Flashstream-pub Type 7
	if (vp->flashstream.segment_tag &&
	    strncmp(vp->flashstream.segment_tag, "", 1) != 0)
	    ssp->flashstream_pub.seg_tag = nkn_strdup_type(vp->flashstream.segment_tag,
							   mod_ssp_cli_strdup);
	if (vp->flashstream.segment_frag_delimiter &&
	    strncmp(vp->flashstream.segment_frag_delimiter, "", 1) != 0)
            ssp->flashstream_pub.seg_frag_delimiter = nkn_strdup_type(vp->flashstream.segment_frag_delimiter,
								      mod_ssp_cli_strdup);
	if (vp->flashstream.fragment_tag &&
	    strncmp(vp->flashstream.fragment_tag, "", 1) != 0)
            ssp->flashstream_pub.frag_tag = nkn_strdup_type(vp->flashstream.fragment_tag,
							    mod_ssp_cli_strdup);
	// Bandwidth optimization feature
	// transcode
	i = 0;
	ssp->bandwidth_opt.enable = vp->bw_opt.active;
	while ( i < MAX_SSP_BW_FILE_TYPES) {

	    if (vp->bw_opt.file_info[i].file_type &&
		strncmp(vp->bw_opt.file_info[i].file_type, "", 1) != 0)
                ssp->bandwidth_opt.info[i].file_type =
		    nkn_strdup_type(vp->bw_opt.file_info[i].file_type, mod_ssp_cli_strdup);

	    if (vp->bw_opt.file_info[i].transcode_comp_ratio &&
		strncmp(vp->bw_opt.file_info[i].transcode_comp_ratio, "", 1) != 0)
                ssp->bandwidth_opt.info[i].transcode_comp_ratio =
                    nkn_strdup_type(vp->bw_opt.file_info[i].transcode_comp_ratio, mod_ssp_cli_strdup);

	    ssp->bandwidth_opt.info[i].hotness_threshold = vp->bw_opt.file_info[i].hotness_threshold;

	    i++;
	}
	i = 0;

	// Bandwidth optimization feature
	// Youtube up/downgrade
	i = 0;
	while ( i < MAX_SSP_BW_FILE_TYPES) {

	    if (vp->bw_opt.file_info[i].file_type &&
		strncmp(vp->bw_opt.file_info[i].file_type, "", 1) != 0)
                ssp->bandwidth_opt.info[i].file_type =
		    nkn_strdup_type(vp->bw_opt.file_info[i].file_type, mod_ssp_cli_strdup);

	    if (vp->bw_opt.file_info[i].switch_rate &&
		strncmp(vp->bw_opt.file_info[i].switch_rate, "", 1) != 0)
                ssp->bandwidth_opt.info[i].switch_rate =
                    nkn_strdup_type(vp->bw_opt.file_info[i].switch_rate, mod_ssp_cli_strdup);

	    ssp->bandwidth_opt.info[i].switch_limit_rate = vp->bw_opt.file_info[i].switch_limit_rate;

	    i++;
	}
	i = 0;
    }
    else{
	// Log error for undefined vp

	//	delete_ssp_config_t(ssp);
	// Dont do anything now, delete will be called automatically later
	ssp->player_type = -1; // Unused value
    }

    return ssp;
}

void delete_ssp_config_t(ssp_config_t *ssp)
{
    int i=0;

    if (!ssp) {
    	return;
    }

    if (ssp->player_name){
	free(ssp->player_name);
	ssp->player_name = 0;
    }

    if (ssp->hash_verify.digest_type){
	free(ssp->hash_verify.digest_type);
	ssp->hash_verify.digest_type=0;
    }

    if (ssp->hash_verify.datastr){
	free(ssp->hash_verify.datastr);
	ssp->hash_verify.datastr=0;
    }

    if (ssp->hash_verify.secret_key){
	free(ssp->hash_verify.secret_key);
	ssp->hash_verify.secret_key=0;
    }

    if (ssp->hash_verify.secret_position){
	free(ssp->hash_verify.secret_position);
	ssp->hash_verify.secret_position=0;
    }

    if (ssp->hash_verify.uri_query){
	free(ssp->hash_verify.uri_query);
	ssp->hash_verify.uri_query=0;
    }

    if (ssp->hash_verify.expirytime_query) {
        free(ssp->hash_verify.expirytime_query);
        ssp->hash_verify.expirytime_query = 0;
    }

    if (ssp->hash_verify.url_type) {
        free(ssp->hash_verify.url_type);
        ssp->hash_verify.url_type=0;
    }

    if (ssp->fast_start.uri_query){
	free(ssp->fast_start.uri_query);
	ssp->fast_start.uri_query=0;
    }

    if (ssp->seek.uri_query){
	free(ssp->seek.uri_query);
	ssp->seek.uri_query=0;
    }

    if (ssp->seek.query_length){
        free(ssp->seek.query_length);
        ssp->seek.query_length=0;
    }

    if (ssp->seek.flv_off_type){
        free(ssp->seek.flv_off_type);
        ssp->seek.flv_off_type=0;
    }

    if (ssp->seek.mp4_off_type){
        free(ssp->seek.mp4_off_type);
        ssp->seek.mp4_off_type=0;
    }
#if 0
    if (ssp->assured_flow.uri_query){
	free(ssp->assured_flow.uri_query);
	ssp->assured_flow.uri_query=0;
    }

    if (ssp->assured_flow.overhead) {
       free(ssp->assured_flow.overhead);
       ssp->assured_flow.overhead = 0;
    }
#else
    if (ssp->rate_control.qstr) {
	free(ssp->rate_control.qstr);
	ssp->rate_control.qstr = 0;
    }
#endif
    if (ssp->full_download.matchstr){
	free(ssp->full_download.matchstr);
	ssp->full_download.matchstr=0;
    }

    if (ssp->full_download.uri_query){
	free(ssp->full_download.uri_query);
	ssp->full_download.uri_query=0;
    }

    if (ssp->full_download.uri_hdr){
	free(ssp->full_download.uri_hdr);
	ssp->full_download.uri_hdr=0;
    }

    if (ssp->smooth_flow.uri_query){
	free(ssp->smooth_flow.uri_query);
	ssp->smooth_flow.uri_query=0;
    }

    // Health Probe
    if (ssp->health_probe.uri_query){
	free(ssp->health_probe.uri_query);
	ssp->health_probe.uri_query=0;
    }

    if (ssp->health_probe.matchstr){
	free(ssp->health_probe.matchstr);
	ssp->health_probe.matchstr=0;
    }

    // Req Auth (Yahoo)
    if (ssp->req_auth.digest_type){
	free(ssp->req_auth.digest_type);
	ssp->req_auth.digest_type=0;
    }

    if (ssp->req_auth.shared_secret){
	free(ssp->req_auth.shared_secret);
	ssp->req_auth.shared_secret=0;
    }

    if (ssp->req_auth.stream_id_uri_query){
	free(ssp->req_auth.stream_id_uri_query);
	ssp->req_auth.stream_id_uri_query=0;
    }

    if (ssp->req_auth.auth_id_uri_query){
	free(ssp->req_auth.auth_id_uri_query);
	ssp->req_auth.auth_id_uri_query=0;
    }

    if (ssp->req_auth.match_uri_query){
	free(ssp->req_auth.match_uri_query);
	ssp->req_auth.match_uri_query=0;
    }

    // Smoothflow Type 4
    if (ssp->sf_control){
	free(ssp->sf_control);
	ssp->sf_control=0;
    }

    if (ssp->sf_signals.session_id_uri_query){
	free(ssp->sf_signals.session_id_uri_query);
	ssp->sf_signals.session_id_uri_query=0;
    }

    if (ssp->sf_signals.state_uri_query){
	free(ssp->sf_signals.state_uri_query);
	ssp->sf_signals.state_uri_query=0;
    }

    if (ssp->sf_signals.profile_uri_query){
	free(ssp->sf_signals.profile_uri_query);
	ssp->sf_signals.profile_uri_query=0;
    }

    if (ssp->sf_signals.chunk_uri_query){
	free(ssp->sf_signals.chunk_uri_query);
	ssp->sf_signals.chunk_uri_query=0;
    }

    //Youtube cache name config Type 5
    if (ssp->video_id.video_id_uri_query) {
	free(ssp->video_id.video_id_uri_query);
	ssp->video_id.video_id_uri_query = 0;
    }
    if (ssp->video_id.format_tag) {
        free(ssp->video_id.format_tag);
        ssp->video_id.format_tag = 0;
    }

    // Smoothstream pub type config (type 6)
    if (ssp->smoothstream_pub.quality_tag) {
      free(ssp->smoothstream_pub.quality_tag);
      ssp->smoothstream_pub.quality_tag  = 0;
    }
    if (ssp->smoothstream_pub.fragment_tag) {
      free(ssp->smoothstream_pub.fragment_tag);
      ssp->smoothstream_pub.fragment_tag = 0;
    }

    // Flashstream pub type config (type 7)
    if (ssp->flashstream_pub.seg_tag) {
	free(ssp->flashstream_pub.seg_tag);
	ssp->flashstream_pub.seg_tag = 0;
    }
    if (ssp->flashstream_pub.seg_frag_delimiter) {
	free(ssp->flashstream_pub.seg_frag_delimiter);
	ssp->flashstream_pub.seg_frag_delimiter = 0;
    }
    if (ssp->flashstream_pub.frag_tag) {
	free(ssp->flashstream_pub.frag_tag);
	ssp->flashstream_pub.frag_tag = 0;
    }

    i=0;
    while (i < MAX_SSP_BW_FILE_TYPES) {
	if (ssp->bandwidth_opt.info[i].file_type) {
	    free(ssp->bandwidth_opt.info[i].file_type);
	    ssp->bandwidth_opt.info[i].file_type = 0;
	}
	if (ssp->bandwidth_opt.info[i].transcode_comp_ratio) {
	    free(ssp->bandwidth_opt.info[i].transcode_comp_ratio);
	    ssp->bandwidth_opt.info[i].transcode_comp_ratio = 0;
	}
        if (ssp->bandwidth_opt.info[i].switch_rate) {
            free(ssp->bandwidth_opt.info[i].switch_rate);
            ssp->bandwidth_opt.info[i].switch_rate = 0;
        }
	i++;
    }

    free(ssp);
}

static int setup_origin_server_t_headers(origin_server_t *osd, 
			     const origin_request_data_t *origin_req_data)
{
    int n;
    osd->u.http.header = nkn_calloc_type(1, 
    				  sizeof(header_descriptor_t) * 
				  	MAX_ORIGIN_REQUEST_HEADERS,
				  mod_ns_header_descriptor_t);
    if (!osd->u.http.header) {
    	return 1; // memory allocation failed
    }

    osd->u.http.send_x_forward_header = origin_req_data->x_forward_header;
    osd->u.http.include_orig_ip_port  = origin_req_data->include_orig_ip_port; 
    osd->u.http.num_headers = MAX_ORIGIN_REQUEST_HEADERS;

    for (n = 0; n < MAX_ORIGIN_REQUEST_HEADERS; n++) {
    	if (origin_req_data->add_headers[n].name) {
	    osd->u.http.header[n].action = ACT_ADD;
	    osd->u.http.header[n].name = 
	    	nkn_strdup_type(origin_req_data->add_headers[n].name,
				mod_ns_header_name);
	    osd->u.http.header[n].name_strlen = 
	    	strlen(osd->u.http.header[n].name);

	    if (origin_req_data->add_headers[n].value) {
	    	osd->u.http.header[n].value =
		    nkn_strdup_type(origin_req_data->add_headers[n].value,
				    mod_ns_header_value);
	    	osd->u.http.header[n].value_strlen =
		    strlen(osd->u.http.header[n].value);
	    }
	}
    }
    return 0; // Success
}

static int setup_response_headers(http_config_t *httpcfg, 
				  const client_response_data_t *response_hdrs)
{
    int n;

    httpcfg->delete_response_headers = 
    	nkn_calloc_type(1, 
		sizeof(header_descriptor_t) * MAX_CLIENT_RESPONSE_HEADERS,
		mod_ns_header_descriptor_t);
    if (!httpcfg->delete_response_headers) {
    	return 1;
    }
    httpcfg->num_delete_response_headers = MAX_CLIENT_RESPONSE_HEADERS;

    httpcfg->add_response_headers =
    	nkn_calloc_type(1, 
		sizeof(header_descriptor_t) * MAX_CLIENT_RESPONSE_HEADERS,
		mod_ns_header_descriptor_t);
    if (!httpcfg->add_response_headers) {
    	free(httpcfg->delete_response_headers);
    	httpcfg->delete_response_headers = 0;
    	return 2;
    }
    httpcfg->num_add_response_headers = MAX_CLIENT_RESPONSE_HEADERS;

    for (n = 0; n < MAX_CLIENT_RESPONSE_HEADERS; n++) {
    	if (response_hdrs->delete_headers[n].name) {
	    httpcfg->delete_response_headers[n].action = ACT_DELETE;
	    httpcfg->delete_response_headers[n].name =
	    	nkn_strdup_type(response_hdrs->delete_headers[n].name,
				mod_ns_header_value);
	    httpcfg->delete_response_headers[n].name_strlen =
		strlen(httpcfg->delete_response_headers[n].name);
	}
    }

    for (n = 0; n < MAX_CLIENT_RESPONSE_HEADERS; n++) {
    	if (response_hdrs->add_headers[n].name) {
	    httpcfg->add_response_headers[n].action = ACT_ADD;
	    httpcfg->add_response_headers[n].name =
	    	nkn_strdup_type(response_hdrs->add_headers[n].name,
				mod_ns_header_value);
	    httpcfg->add_response_headers[n].name_strlen =
	    	strlen(httpcfg->add_response_headers[n].name);

	    if (response_hdrs->add_headers[n].value) {
	    	httpcfg->add_response_headers[n].value =
		    nkn_strdup_type(response_hdrs->add_headers[n].value,
				    mod_ns_header_value);
	    	httpcfg->add_response_headers[n].value_strlen =
		    strlen(httpcfg->add_response_headers[n].value);
	    }
	}
    }

    return 0;
}

http_config_t *new_http_config_t(namespace_node_data_t *nd, 
				 namespace_config_t *nsc,
				 const namespace_config_t *old_nsc)
{
    int n;
    int rv;
    int count;
    cache_age_policies_t *cap;
    content_type_max_age_t *ct2max_age;
    char tbuf[128];
    char errbuf[32*1024];
    http_config_t *httpcfg = (http_config_t *)nkn_calloc_type(1, 
    						sizeof(http_config_t),
    						mod_ns_http_config_t);
    if (!httpcfg) {
    	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	return 0;
    }

    switch(nd->origin_server.proto) {
    case OSF_HTTP_HOST:

    	httpcfg->origin.select_method = OSS_HTTP_CONFIG;
	break;
    case OSF_HTTP_ABS_URL:
    	httpcfg->origin.select_method = OSS_HTTP_ABS_URL;
	break;
    case OSF_HTTP_FOLLOW_HEADER:
    	httpcfg->origin.select_method = OSS_HTTP_FOLLOW_HEADER;
	break;
    case OSF_HTTP_SERVER_MAP:
    	httpcfg->origin.select_method = OSS_HTTP_SERVER_MAP;
	break;
    case OSF_NFS_HOST:
    	httpcfg->origin.select_method = OSS_NFS_CONFIG;
	break;
    case OSF_NFS_SERVER_MAP:
    	httpcfg->origin.select_method = OSS_NFS_SERVER_MAP;
	break;
    case OSF_HTTP_DEST_IP:
	httpcfg->origin.select_method = OSS_HTTP_DEST_IP;
	break;
    case OSF_RTSP_HOST:
	httpcfg->origin.select_method = OSS_RTSP_CONFIG;
	break;
    case OSF_RTSP_DEST_IP:
	httpcfg->origin.select_method = OSS_RTSP_DEST_IP;
	break;
    case OSF_HTTP_NODE_MAP:
	httpcfg->origin.select_method = OSS_HTTP_NODE_MAP;
    	break;
    case OSF_HTTP_PROXY_HOST:
    	httpcfg->origin.select_method = OSS_HTTP_PROXY_CONFIG;
	break;

    default:
	DBG_LOG(MSG, MOD_NAMESPACE, "Invalid origin_server.proto=%d",
		nd->origin_server.proto);
	delete_http_config_t(httpcfg);
	return 0;
    	
    }

    // Origin
    if (nd->origin_server.proto == OSF_NFS_SERVER_MAP) {
    	/* Server map: This is for the customer case where a file with
	   all the mount points is specified. */
	if (nd->origin_server.nfs_server_map_name && (nd->origin_server.nfs_server_map != NULL)) {
	    httpcfg->origin.u.nfs.server_map_name = 
	    	nkn_strdup_type(nd->origin_server.nfs_server_map_name, 
				mod_ns_server_map_name);

	    httpcfg->origin.u.nfs.entries = 1;

	    httpcfg->origin.u.nfs.server_map = (nfs_server_map_node_data_t *)
		nkn_calloc_type (1, sizeof(nfs_server_map_node_data_t),
				mod_ns_nfs_server_map_node_data_t);
	    if (!httpcfg->origin.u.nfs.server_map) {
		delete_http_config_t(httpcfg);
		return 0;
	    }
	    /* Remote file url */
	    if (nd->origin_server.nfs_server_map->file_url) {
		httpcfg->origin.u.nfs.server_map->file_url =
		    nkn_strdup_type(nd->origin_server.nfs_server_map->file_url, 
				    mod_ns_server_map_file_url);
	    }
	    /* Refresh interval*/
	    httpcfg->origin.u.nfs.server_map->refresh =
		nd->origin_server.nfs_server_map->refresh;

	    /* Server map name */
	    if(nd->origin_server.nfs_server_map->name) {
		httpcfg->origin.u.nfs.server_map->name =
		    nkn_strdup_type(nd->origin_server.nfs_server_map->name, 
				    mod_ns_server_map_name);
	    }

	    rv = setup_response_headers(httpcfg, &nd->client_response);
	    if (rv) {
		delete_http_config_t(httpcfg);
		DBG_LOG(MSG, MOD_NAMESPACE,
			"setup_response_headers() failed, rv=%d", rv);
		return 0;
	    }
	}
    } else if (nd->origin_server.proto == OSF_NFS_HOST) {
    	httpcfg->origin.u.nfs.entries = 1;

	httpcfg->origin.u.nfs.server = nkn_calloc_type(1,
				sizeof(origin_server_NFS_t *) * 1024,
				mod_ns_origin_server_NFS_t);
	if (!httpcfg->origin.u.nfs.server) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}
	httpcfg->origin.u.nfs.server[0] = nkn_calloc_type(1,
				sizeof(origin_server_NFS_t),
				mod_ns_origin_server_NFS_t);
	if (!httpcfg->origin.u.nfs.server[0]) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}

	if (nd->origin_server.nfs.host) {
	    httpcfg->origin.u.nfs.server[0]->hostname_path =
		nkn_strdup_type(nd->origin_server.nfs.host, 
				mod_ns_hostname_path);
	    httpcfg->origin.u.nfs.server[0]->hostname_pathlen =
		strlen(httpcfg->origin.u.nfs.server[0]->hostname_path);
	} else {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "origin_server_nfs[0].host == 0");
	    return 0;
	}
	httpcfg->origin.u.nfs.server[0]->port =
		nd->origin_server.nfs.port;
	httpcfg->origin.u.nfs.server[0]->cache_locally =
		nd->origin_server.nfs.local_cache;

	rv = setup_response_headers(httpcfg, &nd->client_response);
	if (rv) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE,
		    "setup_response_headers() failed, rv=%d", rv);
	    return 0;
	}
    } else if ((nd->origin_server.proto == OSF_HTTP_HOST) ||
    		(nd->origin_server.proto == OSF_HTTP_PROXY_HOST) ||
    		(nd->origin_server.proto == OSF_HTTP_SERVER_MAP) ||
    		(nd->origin_server.proto == OSF_HTTP_NODE_MAP)) {
    	httpcfg->origin.u.http.entries = 1;
    	httpcfg->origin.u.http.server = nkn_calloc_type(1,
				sizeof(origin_server_HTTP_t *) * 1024,
				mod_ns_origin_server_HTTP_t);
	if (!httpcfg->origin.u.http.server) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}
	httpcfg->origin.u.http.server[0] = nkn_calloc_type(1,
				sizeof(origin_server_HTTP_t),
				mod_ns_origin_server_HTTP_t);
	if (!httpcfg->origin.u.http.server[0]) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}

	if ((nd->origin_server.proto == OSF_HTTP_HOST) ||
	    (nd->origin_server.proto == OSF_HTTP_PROXY_HOST)) {
	    if (nd->origin_server.http.host) {
	    	httpcfg->origin.u.http.server[0]->hostname =
		    nkn_strdup_type(nd->origin_server.http.host, 
		    			mod_ns_hostname);
		httpcfg->origin.u.http.server[0]->hostname_strlen = 
		    httpcfg->origin.u.http.server[0]->hostname ? 
		    	strlen(httpcfg->origin.u.http.server[0]->hostname) : 0;
	    } else {
	    	delete_http_config_t(httpcfg);
    	    	DBG_LOG(MSG, MOD_NAMESPACE, "origin_server.http.host == 0");
	    	return 0;
	    }

	    if (nd->origin_server.http.aws.active) {
		httpcfg->origin.u.http.server[0]->aws.access_key =
		    nkn_strdup_type(nd->origin_server.http.aws.access_key, mod_ns_hostname);
		httpcfg->origin.u.http.server[0]->aws.access_key_len=
		    httpcfg->origin.u.http.server[0]->aws.access_key ?
			strlen( httpcfg->origin.u.http.server[0]->aws.access_key) : 0;
		httpcfg->origin.u.http.server[0]->aws.secret_key =
		    nkn_strdup_type(nd->origin_server.http.aws.secret_key, mod_ns_hostname);
		httpcfg->origin.u.http.server[0]->aws.secret_key_len=
		    httpcfg->origin.u.http.server[0]->aws.secret_key ?
			strlen( httpcfg->origin.u.http.server[0]->aws.secret_key) : 0;
		httpcfg->origin.u.http.server[0]->aws.active = 1;
	    } else {
		httpcfg->origin.u.http.server[0]->aws.active = 0;
	    }

	    if (nd->origin_server.http.dns_query) {
	    	httpcfg->origin.u.http.server[0]->dns_query =
		    nkn_strdup_type(nd->origin_server.http.dns_query,
		    			mod_ns_hostname);
		httpcfg->origin.u.http.server[0]->dns_query_len =
		    httpcfg->origin.u.http.server[0]->dns_query ?
		    	strlen(httpcfg->origin.u.http.server[0]->dns_query) : 0;
	    }

	    httpcfg->origin.u.http.server[0]->port =
		nd->origin_server.http.port;
	    httpcfg->origin.u.http.server[0]->weight =
		nd->origin_server.http.weight;
	    httpcfg->origin.u.http.server[0]->keepalive =
		nd->origin_server.http.keep_alive;
	} else if ((nd->origin_server.proto == OSF_HTTP_SERVER_MAP) &&
		   (nd->origin_server.http_server_map[0])) {
            if (!nd->origin_server.http_server_map[0]->binary_server_map ||
	    	!nd->origin_server.http_server_map[0]->binary_size) {
	    	delete_http_config_t(httpcfg);
    	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"Invalid binary data, "
			"binary_server_map=%p, binary_server_map_size=%d "
			"format_type=%d",
			nd->origin_server.http_server_map[0]->binary_server_map,
			nd->origin_server.http_server_map[0]->binary_size,
			nd->origin_server.http_server_map[0]->format_type);
		return 0;
	    }
	    httpcfg->origin.u.http.map_data_size = 
	    	nd->origin_server.http_server_map[0]->binary_size;
	    rv = nkn_posix_memalign_type((void **)
	    				 &httpcfg->origin.u.http.map_data,
				    	 sizeof(long),
				    	 httpcfg->origin.u.http.map_data_size,
				    	 mod_ns_http_server_map);

	    if (rv || !httpcfg->origin.u.http.map_data) {
	    	delete_http_config_t(httpcfg);
    	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"nkn_posix_memalign() rv=%d failed",
			rv);
		return 0;
	    }
	    memcpy(httpcfg->origin.u.http.map_data, 
	    	   nd->origin_server.http_server_map[0]->binary_server_map,
		   httpcfg->origin.u.http.map_data_size);

	    if ((httpcfg->origin.u.http.map_data->magicno != HOM_MAGICNO) ||
	      	(httpcfg->origin.u.http.map_data->version != HOM_VERSION)) {
    	    	DBG_LOG(MSG, MOD_NAMESPACE, "Invalid origin.u.http.map_data, "
			"version=%d magic=0x%x",
			httpcfg->origin.u.http.map_data->version,
			httpcfg->origin.u.http.map_data->magicno);
	    	delete_http_config_t(httpcfg);
		return 0;
	    }
	    rv = setup_origin_server_map(httpcfg->origin.u.http.map_data,
				     &httpcfg->origin.u.http.map_hash_hostnames,
				     &httpcfg->origin.u.http.map_ht_hostnames);
	    if (rv) {
	    	delete_http_config_t(httpcfg);
    	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"setup_origin_server_map() failed, rv=%d namespace=%s",
			rv, nd->namespace);
		return 0;
	    }

	    httpcfg->origin.u.http.ho_ldn_name = 
	    	nkn_malloc_type(nsc->namespace_strlen + 16, 
			   	mod_ns_ldn_http_server_map);
	    if (!httpcfg->origin.u.http.ho_ldn_name) {
	    	delete_http_config_t(httpcfg);
    	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"nkn_malloc_type() ho_ldn_name failed");
		return 0;
	    }
	    snprintf((char *)httpcfg->origin.u.http.ho_ldn_name, 
		     nsc->namespace_strlen + 16, "ho-%s",
		     nsc->namespace);
	    httpcfg->origin.u.http.ho_ldn_name_strlen = 
	    	strlen(httpcfg->origin.u.http.ho_ldn_name);

	} else if (nd->origin_server.proto == OSF_HTTP_NODE_MAP) {
	    rv = setup_origin_nodemap(nd, httpcfg, nsc, old_nsc);
	    if (rv) {
	    	delete_http_config_t(httpcfg);
    	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"setup_origin_nodemap() failed, rv=%d namespace=%s",
			rv, nd->namespace);
		return 0;
	    }
	    httpcfg->request_policies.connect_timer_msecs = 
	    	nd->origin_request.orig_conn_values.conn_timeout;
	    httpcfg->request_policies.connect_retry_delay_msecs = 
	    	nd->origin_request.orig_conn_values.conn_retry_delay;
	    httpcfg->request_policies.read_timer_msecs = 
	    	nd->origin_request.orig_conn_values.read_timeout;
	    httpcfg->request_policies.read_retry_delay_msecs =
	    	nd->origin_request.orig_conn_values.read_retry_delay;
	}
	rv = setup_origin_server_t_headers(&httpcfg->origin, 
					   &nd->origin_request);
	if (rv) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, 
	    	    "setup_origin_server_t_headers() failed, rv=%d", rv);
	    return 0;
	}

	rv = setup_response_headers(httpcfg, &nd->client_response);
	if (rv) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, 
	    	    "setup_response_headers() failed, rv=%d", rv);
	    return 0;
	}
    } else if ((nd->origin_server.proto == OSF_HTTP_ABS_URL) ||
    	       (nd->origin_server.proto == OSF_HTTP_FOLLOW_HEADER) ||
    	       (nd->origin_server.proto == OSF_HTTP_DEST_IP)) {
    	httpcfg->origin.u.http.entries = 1;
    	httpcfg->origin.u.http.server = nkn_calloc_type(1,
				sizeof(origin_server_HTTP_t *) * 1024,
				mod_ns_origin_server_HTTP_t);
	if (!httpcfg->origin.u.http.server) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}

	httpcfg->origin.u.http.server[0] = nkn_calloc_type(1,
				sizeof(origin_server_HTTP_t),
				mod_ns_origin_server_HTTP_t);
	if (!httpcfg->origin.u.http.server[0]) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}

	if (nd->origin_server.follow.aws.active) {
	    httpcfg->origin.u.http.server[0]->aws.access_key =
		nkn_strdup_type(nd->origin_server.follow.aws.access_key, mod_ns_hostname);
	    httpcfg->origin.u.http.server[0]->aws.access_key_len =
		httpcfg->origin.u.http.server[0]->aws.access_key ?
			strlen(httpcfg->origin.u.http.server[0]->aws.access_key) : 0;
	    httpcfg->origin.u.http.server[0]->aws.secret_key =
		nkn_strdup_type(nd->origin_server.follow.aws.secret_key, mod_ns_hostname);
	    httpcfg->origin.u.http.server[0]->aws.secret_key_len =
		httpcfg->origin.u.http.server[0]->aws.secret_key ?
		    strlen( httpcfg->origin.u.http.server[0]->aws.secret_key) : 0;
	    httpcfg->origin.u.http.server[0]->aws.active = 1;
	} else {
	    httpcfg->origin.u.http.server[0]->aws.active = 0;
	}

	if (nd->origin_server.follow.dns_query) {
	    httpcfg->origin.u.http.server[0]->dns_query =
		nkn_strdup_type(nd->origin_server.follow.dns_query, mod_ns_hostname);
	    httpcfg->origin.u.http.server[0]->dns_query_len =
		httpcfg->origin.u.http.server[0]->dns_query ?
		    strlen(httpcfg->origin.u.http.server[0]->dns_query) : 0;
	}

	rv = setup_origin_server_t_headers(&httpcfg->origin, 
					   &nd->origin_request);
	if (rv) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, 
	    	    "setup_origin_server_t_headers() failed, rv=%d", rv);
	    return 0;
	}

	rv = setup_response_headers(httpcfg, &nd->client_response);
	if (rv) {
	    delete_http_config_t(httpcfg);
    	    DBG_LOG(MSG, MOD_NAMESPACE, 
	    	    "setup_response_headers() failed, rv=%d", rv);
	    return 0;
	}

	if (nd->origin_server.http_use_client_ip == NKN_TRUE) {
		httpcfg->origin.u.http.use_client_ip = NKN_TRUE;
	}
    } else if (nd->origin_server.proto == OSF_RTSP_HOST) {
	httpcfg->origin.u.rtsp.entries = 1;
	httpcfg->origin.u.rtsp.server = 
		nkn_calloc_type(1,
			        sizeof(origin_server_RTSP_t) * 1024,
			        mod_ns_origin_server_RTSP_t);
	if (!httpcfg->origin.u.rtsp.server) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}
	httpcfg->origin.u.rtsp.server[0] = 
		nkn_calloc_type(1,
			        sizeof(origin_server_RTSP_t),
			        mod_ns_origin_server_RTSP_t);
	if (!httpcfg->origin.u.rtsp.server[0]) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
	}

	if (nd->origin_server.rtsp.host) {
	    httpcfg->origin.u.rtsp.server[0]->hostname = 
		    nkn_strdup_type(nd->origin_server.rtsp.host,
				    mod_ns_hostname);
	    httpcfg->origin.u.rtsp.server[0]->hostname_strlen = 
		    httpcfg->origin.u.rtsp.server[0]->hostname ?
		        strlen(httpcfg->origin.u.rtsp.server[0]->hostname) : 0;
	} else {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "origin_server.rtsp.host == 0");
	    return 0;
	}

	httpcfg->origin.u.rtsp.server[0]->port = 
		nd->origin_server.rtsp.port;
	httpcfg->origin.u.rtsp.server[0]->alt_http_port = 
		nd->origin_server.rtsp.alternate_http_port;
	httpcfg->origin.u.rtsp.server[0]->transport = 
		nd->origin_server.rtsp.transport;

	rv = setup_origin_server_t_headers(&httpcfg->origin,
			    		   &nd->origin_request);
	if (rv) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "setup_origin_server_t_headers() failed, rv=%d", rv);
	    return 0;
	}
	    
	rv = setup_response_headers(httpcfg, &nd->client_response);
	if (rv) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "setup_response_headers() failed, rv=%d", rv);
	    return 0;
	}
    } else if (nd->origin_server.proto == OSF_RTSP_DEST_IP) {
	if (nd->origin_server.rtsp_use_client_ip == NKN_TRUE) {
	    httpcfg->origin.u.rtsp.use_client_ip = NKN_TRUE;
	}
    }
    httpcfg->origin.u.http.ip_version = nd->origin_server.ip_version;
    httpcfg->origin.http_idle_timeout = nd->origin_server.http_idle_timeout;
    // Policies

    if (nd->origin_fetch.cache_directive_no_cache) {
    	if (strcasecmp("follow",
		nd->origin_fetch.cache_directive_no_cache) == 0) {
    	    httpcfg->policies.cache_no_cache = NO_CACHE_FOLLOW;
        } else if (strcasecmp("override",
    		nd->origin_fetch.cache_directive_no_cache) == 0) {
    	    httpcfg->policies.cache_no_cache = NO_CACHE_OVERRIDE;
    	} else if (strcasecmp("tunnel",
		 nd->origin_fetch.cache_directive_no_cache) == 0) {
	    httpcfg->policies.cache_no_cache = NO_CACHE_TUNNEL;
	}
    } else {
	httpcfg->policies.cache_no_cache = NO_CACHE_FOLLOW;
    }
    httpcfg->policies.cache_min_size = (nd->origin_fetch.cache_obj_size_min * 1024);
    httpcfg->policies.cache_max_size = (nd->origin_fetch.cache_obj_size_max * 1024);
    httpcfg->policies.ingest_only_in_ram = nd->origin_fetch.ingest_only_in_ram;

    httpcfg->policies.reval_barrier = nd->origin_fetch.cache_barrier_revalidate.reval_time;
    httpcfg->policies.reval_type= nd->origin_fetch.cache_barrier_revalidate.reval_type;
    httpcfg->policies.delivery_expired = nd->origin_fetch.enable_expired_delivery;
    httpcfg->policies.reval_trigger = nd->origin_fetch.cache_barrier_revalidate.reval_trigger;
    httpcfg->policies.cache_age_default = nd->origin_fetch.cache_age;
    httpcfg->policies.cache_partial_content = nd->origin_fetch.partial_content;
    httpcfg->policies.modify_date_header = nd->origin_fetch.date_header_modify;
    httpcfg->policies.ignore_s_maxage = nd->origin_fetch.ignore_s_maxage;
    httpcfg->policies.cache_revalidate.allow_cache_revalidate =
	nd->origin_fetch.cache_reval.cache_revalidate;
    httpcfg->policies.cache_revalidate.method =
	nd->origin_fetch.cache_reval.method;
    httpcfg->policies.cache_revalidate.validate_header =
	nd->origin_fetch.cache_reval.validate_header;
    if(nd->origin_fetch.cache_reval.header_name){
    	httpcfg->policies.cache_revalidate.header_name =
    			nkn_strdup_type(nd->origin_fetch.cache_reval.header_name,
    			mod_ns_reval_header_buf);
	httpcfg->policies.cache_revalidate.header_len =
	    strlen(nd->origin_fetch.cache_reval.header_name);
    }
    if (httpcfg->policies.cache_revalidate.header_name == NULL) {
	delete_http_config_t(httpcfg);
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	return 0;
    }
    httpcfg->policies.cache_revalidate.use_client_headers =
				nd->origin_fetch.cache_reval.use_client_headers;
    //httpcfg->policies.allow_cache_revalidate =
    	//nd->origin_fetch.cache_revalidate;
    httpcfg->policies.flush_ic_cache = nd->origin_fetch.flush_intermediate_caches;
    httpcfg->policies.store_cache_min_age =
    	nd->origin_fetch.content_store_cache_age_threshold;
    httpcfg->policies.bulk_fetch =
	nd->origin_fetch.bulk_fetch;
    httpcfg->policies.bulk_fetch_pages =
	nd->origin_fetch.bulk_fetch_pages;
    httpcfg->policies.offline_om_fetch_filesize =
    	nd->origin_fetch.offline_fetch_size * 1024; // Use storage KB convention
    httpcfg->policies.offline_om_smooth_flow =
    	nd->origin_fetch.offline_fetch_smooth_flow;
    httpcfg->policies.convert_head = NKN_FALSE;
    	//nd->origin_fetch.convert_head;
    httpcfg->policies.store_cache_min_size =
    	nd->origin_fetch.store_cache_min_size;
    httpcfg->policies.is_set_cookie_cacheable =
    	nd->origin_fetch.is_set_cookie_cacheable;
	httpcfg->policies.cache_cookie_noreval =
		nd->origin_fetch.cacheable_no_revalidation;
    httpcfg->policies.is_host_header_inheritable =
    	nd->origin_fetch.is_host_header_inheritable;

    if (nd->origin_fetch.host_header && 
        (nd->origin_fetch.host_header[0] != '\0')) {
        httpcfg->policies.host_hdr_value =
                      nkn_strdup_type(nd->origin_fetch.host_header,
                                      mod_ns_host_hdr_buf);
    }

    httpcfg->policies.client_req_connect_handle =
        nd->origin_fetch.client_req_connect_handle;
    httpcfg->policies.validate_with_date_hdr =
    	nd->origin_fetch.use_date_header;
    httpcfg->policies.client_req_max_age = 
    	nd->origin_fetch.client_req_max_age;
    httpcfg->policies.client_req_serve_from_origin= 
    	nd->origin_fetch.client_req_serve_from_origin;
    httpcfg->policies.cache_fill=
	nd->origin_fetch.cache_fill;
    httpcfg->policies.ingest_policy=
	nd->origin_fetch.ingest_policy;
    httpcfg->policies.eod_on_origin_close=
	nd->origin_fetch.eod_on_origin_close;
    httpcfg->policies.object_correlation_etag_ignore=
	nd->origin_fetch.object_correlation_etag_ignore;
    httpcfg->policies.object_correlation_validatorsall_ignore=
	nd->origin_fetch.object_correlation_validatorsall_ignore;
    httpcfg->policies.uri_depth_threshold =
    	nd->origin_fetch.content_store_uri_depth_threshold;
    httpcfg->policies.cache_ingest.size_threshold =
	nd->origin_fetch.cache_ingest.size_threshold ;
    httpcfg->policies.cache_ingest.hotness_threshold =
	nd->origin_fetch.cache_ingest.hotness_threshold ;
    httpcfg->policies.cache_ingest.incapable_byte_ran_origin =
	nd->origin_fetch.cache_ingest.incapable_byte_ran_origin;
    httpcfg->policies.client_driven_agg_threshold =
       nd->origin_fetch.client_driven_agg_threshold;

    cap = &httpcfg->policies.cache_age_policies;
    cap->content_type_to_max_age =
    	(content_type_max_age_t **) nkn_calloc_type(1, 
		   (sizeof(content_type_max_age_t *) * MAX_CONTENT_TYPES),
		   mod_ns_content_type_max_age_t_list);
    if (!cap->content_type_to_max_age) {
	delete_http_config_t(httpcfg);
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	return 0;
    }
			
    for (n = 0; n < nd->origin_fetch.content_type_count; n++) {
    	if (nd->origin_fetch.cache_age_map[n].content_type) {
	    ct2max_age = nkn_calloc_type(1, sizeof(content_type_max_age_t),
		   		    	 mod_ns_content_type_max_age_t);
    	    if (!ct2max_age) {
		delete_http_config_t(httpcfg);
		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
		return 0;
	    }
    	    cap->content_type_to_max_age[cap->entries++] = ct2max_age;

	    if (*nd->origin_fetch.cache_age_map[n].content_type) {
	    	ct2max_age->content_type = 
	    	    nkn_strdup_type(
		    	nd->origin_fetch.cache_age_map[n].content_type,
			mod_ns_content_type);
		if (!ct2max_age->content_type) {
		    delete_http_config_t(httpcfg);
		    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
		    return 0;
		}
	    	ct2max_age->content_type_strlen = 
			strlen(ct2max_age->content_type);
	    }
	    snprintf(tbuf, sizeof(tbuf), "max-age=%d", 
	    	     nd->origin_fetch.cache_age_map[n].cache_age);
	    tbuf[sizeof(tbuf)-1] = 0;
	    ct2max_age->max_age = nkn_strdup_type(tbuf, mod_ns_max_age);
	    ct2max_age->max_age_strlen = strlen(tbuf);
	}
    }

    httpcfg->policies.disable_cache_on_query =
	nd->origin_fetch.disable_cache_on_query; //NKN_FALSE;

    // Request policies
    httpcfg->request_policies.strip_query =
	nd->origin_fetch.strip_query; //NKN_TRUE;
    httpcfg->request_policies.tunnel_req_with_cookie =
	nd->origin_fetch.disable_cache_on_cookie; //1; // 1 means tunnel
    // BZ 6682: Set tunnel-all flag for this namespace
    httpcfg->request_policies.tunnel_all =
	nd->origin_fetch.tunnel_all;
    httpcfg->request_policies.passthrough_http_head =
    	nd->client_request.passthrough_http_head;
    httpcfg->request_policies.http_head_to_get =
	nd->client_request.http_head_to_get;
    httpcfg->request_policies.dns_resp_mismatch =
	nd->client_request.dns_resp_mismatch;

    httpcfg->request_policies.accept_headers =
    	nkn_calloc_type(1, sizeof(header_descriptor_t)
	    * MAX_CLIENT_REQUEST_HEADERS, mod_ns_header_descriptor_t);
    if (!httpcfg->request_policies.accept_headers) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	    return 0;
    }
    count = 0;
    for (n = 0; n < MAX_CLIENT_REQUEST_HEADERS; n++) {
    	if (nd->client_request.accept_headers[n].name) {
	    httpcfg->request_policies.accept_headers[n].name =
	    	nkn_strdup_type(nd->client_request.accept_headers[n].name,
				mod_ns_header_value);
	    httpcfg->request_policies.accept_headers[n].name_strlen =
		strlen(httpcfg->request_policies.accept_headers[n].name);
	    count++;
	}
    }
    httpcfg->request_policies.num_accept_headers = count;

    /* support for custom cache control header. */
    if (nd->origin_fetch.custom_cache_control != NULL) {
        httpcfg->policies.cache_redirect.hdr_name = 
	    nkn_strdup_type(nd->origin_fetch.custom_cache_control, 
			    mod_ns_custom_cache_control);
	httpcfg->policies.cache_redirect.hdr_len = 
	    strlen(httpcfg->policies.cache_redirect.hdr_name);
    } else {
	httpcfg->policies.cache_redirect.hdr_len = 0;
    }

    if (nd->origin_fetch.redirect_302 == NKN_TRUE) {
	httpcfg->policies.redirect_302 = NKN_TRUE;
    } else {
	httpcfg->policies.redirect_302 = NKN_FALSE;
    }

    if(nd->http_cache_index.ldn_name)
    {
    	httpcfg->cache_index.ldn_name =	nkn_strdup_type(nd->http_cache_index.ldn_name, mod_ns_ldn_name_buf);
    }
    httpcfg->cache_index.exclude_domain =
	nd->http_cache_index.exclude_domain;
	httpcfg->cache_index.include_header =
		nd->http_cache_index.include_header;
	httpcfg->cache_index.include_object =
		nd->http_cache_index.include_object;
	httpcfg->cache_index.checksum_bytes =
		nd->http_cache_index.checksum_bytes;
	httpcfg->cache_index.checksum_offset =
		nd->http_cache_index.checksum_offset;
	httpcfg->cache_index.checksum_from_end =
		nd->http_cache_index.checksum_from_end;
	httpcfg->cache_index.headers_count =
		nd->http_cache_index.headers_count;
	for(n=0; n<httpcfg->cache_index.headers_count;n++) {
		if(nd->http_cache_index.include_header_names[n]){
			httpcfg->cache_index.include_header_names[n] =
				nkn_strdup_type(nd->http_cache_index.include_header_names[n], mod_ns_ldn_name_buf);
		}
	}

    // regex configuration
    if (nd->dynamic_uri.regex) {
	if (!nkn_valid_regex(nd->dynamic_uri.regex, errbuf, sizeof(errbuf))) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_valid_regex() failed. %s", nd->dynamic_uri.regex);
	    return 0;
        }
	httpcfg->request_policies.dynamic_uri.regex =
	    nkn_strdup_type(nd->dynamic_uri.regex, mod_ns_dynamic_uri_regex);

	if(nd->dynamic_uri.map_string){
		httpcfg->request_policies.dynamic_uri.mapping_string =
		    nkn_strdup_type(nd->dynamic_uri.map_string, mod_ns_dynamic_uri_mapping_string);
	}
	if (nd->dynamic_uri.tokenize_str) {
		httpcfg->request_policies.dynamic_uri.tokenize_string =
			nkn_strdup_type(nd->dynamic_uri.tokenize_str, mod_ns_dynamic_uri_tokenize_string);
	} else {
		httpcfg->request_policies.dynamic_uri.tokenize_string = NULL;
	}
	httpcfg->request_policies.dynamic_uri.regctx = 
	    nkn_regex_ctx_alloc();
	if (!httpcfg->request_policies.dynamic_uri.regctx) {
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regex_ctx_alloc() failed");
	    return 0;
	}
	rv = nkn_regcomp_match(httpcfg->request_policies.dynamic_uri.regctx,
		httpcfg->request_policies.dynamic_uri.regex, errbuf, sizeof(errbuf));
	if (rv) {
	    nkn_regex_ctx_free(httpcfg->request_policies.dynamic_uri.regctx);
	    httpcfg->request_policies.dynamic_uri.regctx = 0;
	    delete_http_config_t(httpcfg);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regcomp_match() failed");
	    return 0;
	}
	httpcfg->request_policies.dynamic_uri.tunnel_no_match =
	    nd->dynamic_uri.no_match_tunnel;
    }

	// Seek URI regex configuration
	if (nd->seek_uri.regex) {
	if (!nkn_valid_regex(nd->seek_uri.regex, errbuf, sizeof(errbuf))) {
		delete_http_config_t(httpcfg);
		DBG_LOG(MSG, MOD_NAMESPACE, "seek-uri configuration: nkn_valid_regex() failed. %s", 
			nd->seek_uri.regex);
		return 0;
	}
	httpcfg->request_policies.seek_uri.regex =
		nkn_strdup_type(nd->seek_uri.regex, mod_ns_seek_uri_regex);

	if(nd->seek_uri.map_string){
		httpcfg->request_policies.seek_uri.mapping_string =
			nkn_strdup_type(nd->seek_uri.map_string, mod_ns_seek_uri_mapping_string);
	}

	httpcfg->request_policies.seek_uri.regctx =
		nkn_regex_ctx_alloc();
	if (!httpcfg->request_policies.seek_uri.regctx) {
		delete_http_config_t(httpcfg);
		DBG_LOG(MSG, MOD_NAMESPACE, "seek-uri configuration: nkn_regex_ctx_alloc() failed");
		return 0;
	}
	rv = nkn_regcomp_match(httpcfg->request_policies.seek_uri.regctx,
			httpcfg->request_policies.seek_uri.regex, errbuf, sizeof(errbuf));
	if (rv) {
		nkn_regex_ctx_free(httpcfg->request_policies.seek_uri.regctx);
		httpcfg->request_policies.seek_uri.regctx = 0;
		delete_http_config_t(httpcfg);
		DBG_LOG(MSG, MOD_NAMESPACE, "seek-uri configuration: nkn_regcomp_match() failed");
		return 0;
	}
	httpcfg->request_policies.seek_uri.seek_uri_enable =
		nd->seek_uri.seek_uri_enable;
	}

	httpcfg->response_policies.dscp = nd->dscp;

    if (nd->proto_http == NKN_TRUE) {
	httpcfg->response_protocol |= DP_HTTP;
    } else {
	httpcfg->response_protocol &= ~(DP_HTTP);
    }

    if (nd->proto_rtsp == NKN_TRUE) {
	httpcfg->response_protocol |= DP_RTSTREAM;
    } else {
	httpcfg->response_protocol &= ~(DP_RTSTREAM);
    }

    httpcfg->max_sessions = nd->http_max_conns;
    httpcfg->retry_after_time = nd->http_retry_after_sec;

    if(nd->client_req.secure) {
        httpcfg->client_delivery_type |= DP_CLIENT_RES_SECURE;
    }

    if(nd->client_req.non_secure) {
        httpcfg->client_delivery_type |= DP_CLIENT_RES_NON_SECURE;
    }

    if(nd->origin_req.secure) {
        httpcfg->origin_request_type |= DP_ORIGIN_REQ_SECURE;
    } else {
        httpcfg->origin_request_type |= DP_ORIGIN_REQ_NON_SECURE;
    }

    // Policy Engine
    if (nd->policy_engine.policy_file) {
	httpcfg->policy_engine_config.policy_file = 
	    nkn_strdup_type(nd->policy_engine.policy_file, 
	    		    mod_pe_ns_file_name);
	httpcfg->policy_engine_config.update = 1;
    }

    if (old_nsc) {
	//if (old_nsc->http_config->policy_engine_config.policy_buf) {
	//    free(old_nsc->http_config->policy_engine_config.policy_buf);
	//    old_nsc->http_config->policy_engine_config.policy_buf = NULL;
	//}
	if (old_nsc->http_config->policy_engine_config.policy_data) {
	    httpcfg->policy_engine_config.policy_data = 
	    	old_nsc->http_config->policy_engine_config.policy_data;
	    //old_nsc->http_config->policy_engine_config.policy_data = NULL;
	}
    }

    // Update GeoIP
    httpcfg->geo_ip_cfg.server = nd->geo_service.service;
    httpcfg->geo_ip_cfg.lookup_timeout = nd->geo_service.lookup_timeout;
    if ((nd->geo_service.service == QUOVA) &&  (nd->geo_service.api_url) ){
	httpcfg->geo_ip_cfg.api_url = nkn_strdup_type(nd->geo_service.api_url, 
							mod_pe_ns_file_name);
    }
    httpcfg->geo_ip_cfg.failover_bypass = nd->geo_service.failover_mode;

    // Vary header config
    httpcfg->vary_hdr_cfg.enable = nd->vary_enable;
    
    // Update Mobile user agent regex
    //httpcfg->vary_hdr_cfg.user_agent_regex[0] = NULL;
    // Update Tablet user agent regex
    //httpcfg->vary_hdr_cfg.user_agent_regex[1] = NULL;
    // Update PC user agent regex
    //httpcfg->vary_hdr_cfg.user_agent_regex[2] = NULL;

    for (n = 0; n < MAX_USER_AGENT_GROUPS; n++) {    
	if (nd->user_agent_regex[n] && nd->user_agent_regex[n][0]) {
	    httpcfg->vary_hdr_cfg.user_agent_regex_ctx[n] = nkn_regex_ctx_alloc();
	    rv = nkn_regcomp(httpcfg->vary_hdr_cfg.user_agent_regex_ctx[n],
			     nd->user_agent_regex[n],
			     errbuf, sizeof(errbuf));
	    if (rv) {
		errbuf[sizeof(errbuf)-1] = '\0';
		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regcomp() failed, rv=%d, "
			"regex=[\"%s\"] error=[%s]", 
			rv, nd->user_agent_regex[n], errbuf);
	    }
	}
    }

    return httpcfg;
}

void delete_http_config_t(http_config_t *httpcfg)
{
    int n;

    if (!httpcfg) {
    	return;
    }

    if ((httpcfg->origin.select_method == OSS_NFS_CONFIG) ||
        (httpcfg->origin.select_method == OSS_NFS_SERVER_MAP)) {
    	if (httpcfg->origin.u.nfs.server) {
	    for(n = 0; n < httpcfg->origin.u.nfs.entries; n++) {
    	  	if (httpcfg->origin.u.nfs.server[n]) {
	    	    if (httpcfg->origin.u.nfs.server[0]->hostname_path) {
	    	    	free((char *)
				httpcfg->origin.u.nfs.server[0]->hostname_path);
		    }
    	  	    free(httpcfg->origin.u.nfs.server[n]);
    	  	    httpcfg->origin.u.nfs.server[n] = 0;
		}
	    }
	    free(httpcfg->origin.u.nfs.server);
	    httpcfg->origin.u.nfs.server = 0;
	}

	if (httpcfg->origin.u.nfs.server_map) {
	    if (httpcfg->origin.u.nfs.server_map->file_url) {
		free(httpcfg->origin.u.nfs.server_map->file_url);
	    }
	    if (httpcfg->origin.u.nfs.server_map->name) {
		free(httpcfg->origin.u.nfs.server_map->name );
	    }
	    if(httpcfg->origin.u.nfs.server_map_name) {
		free(httpcfg->origin.u.nfs.server_map_name);
	    }
	    free(httpcfg->origin.u.nfs.server_map);
	}

    } else if ((httpcfg->origin.select_method == OSS_HTTP_CONFIG) ||
	       (httpcfg->origin.select_method == OSS_HTTP_PROXY_CONFIG) ||
	       (httpcfg->origin.select_method == OSS_HTTP_SERVER_MAP) ||
    	       (httpcfg->origin.select_method == OSS_HTTP_ABS_URL) ||
    	       (httpcfg->origin.select_method == OSS_HTTP_FOLLOW_HEADER) ||
    	       (httpcfg->origin.select_method == OSS_HTTP_DEST_IP) ||
    	       (httpcfg->origin.select_method == OSS_HTTP_NODE_MAP)) {
    	if (httpcfg->origin.u.http.server) {
	    for(n = 0; n < httpcfg->origin.u.http.entries; n++) {
    	  	if (httpcfg->origin.u.http.server[n]) {
		    if (httpcfg->origin.u.http.server[n]->hostname) {
		        free((void*)httpcfg->origin.u.http.server[n]->hostname);
		        httpcfg->origin.u.http.server[n]->hostname = 0;
		    }
		    if (httpcfg->origin.u.http.server[n]->aws.active) {
			free((void *)httpcfg->origin.u.http.server[n]->aws.access_key);
			free((void *)httpcfg->origin.u.http.server[n]->aws.secret_key);
			httpcfg->origin.u.http.server[n]->aws.active = 0;
		    }
		    if (httpcfg->origin.u.http.server[n]->dns_query) {
			free(httpcfg->origin.u.http.server[n]->dns_query);
			httpcfg->origin.u.http.server[n]->dns_query = 0;
		    }
    	  	    free(httpcfg->origin.u.http.server[n]);
    	  	    httpcfg->origin.u.http.server[n] = 0;
		}
	    }
	    free(httpcfg->origin.u.http.server);
	    httpcfg->origin.u.http.server = 0;
	}
	if (httpcfg->origin.u.http.ho_ldn_name) {
	    free((char *)httpcfg->origin.u.http.ho_ldn_name);
	    httpcfg->origin.u.http.ho_ldn_name = 0;
	}
	if (httpcfg->origin.u.http.map_ht_hostnames) {
	    free(httpcfg->origin.u.http.map_ht_hostnames);
	    httpcfg->origin.u.http.map_ht_hostnames = 0;
	}
	if (httpcfg->origin.u.http.map_hash_hostnames) {
	    free(httpcfg->origin.u.http.map_hash_hostnames);
	    httpcfg->origin.u.http.map_hash_hostnames = 0;
	}
	if (httpcfg->origin.u.http.map_data) {
	    free(httpcfg->origin.u.http.map_data);
	    httpcfg->origin.u.http.map_data = 0;
	}

	for (n = 0; n < httpcfg->origin.u.http.node_map_entries; n++) {
	    if (httpcfg->origin.u.http.node_map[n]) {
	    	delete_node_map_descriptor_t(
			httpcfg->origin.u.http.node_map[n]);
	    	httpcfg->origin.u.http.node_map[n] = 0;
	    }
	    if (httpcfg->origin.u.http.node_map_private_data[n]) {
	    	free(httpcfg->origin.u.http.node_map_private_data[n]);
	    	httpcfg->origin.u.http.node_map_private_data[n] = 0;
	    }
	}

	if (httpcfg->origin.u.http.node_map) {
	    free(httpcfg->origin.u.http.node_map);
	}

	if (httpcfg->origin.u.http.node_map_private_data) {
	    free(httpcfg->origin.u.http.node_map_private_data);
	}

	if (httpcfg->origin.u.http.header) {
	    for (n = 0; n < httpcfg->origin.u.http.num_headers; n++) {
	    	if (httpcfg->origin.u.http.header[n].name) {
		    free(httpcfg->origin.u.http.header[n].name);
		    httpcfg->origin.u.http.header[n].name = 0;
		}
	    	if (httpcfg->origin.u.http.header[n].value) {
		    free(httpcfg->origin.u.http.header[n].value);
		    httpcfg->origin.u.http.header[n].value = 0;
		}
	    }
	    free(httpcfg->origin.u.http.header);
	    httpcfg->origin.u.http.header = 0;
	}

    } else if (httpcfg->origin.select_method == OSS_RTSP_CONFIG) {
	if(httpcfg->origin.u.rtsp.server) {
		for(n = 0; n < httpcfg->origin.u.rtsp.entries; n++) {
		    if (httpcfg->origin.u.rtsp.server[n]) {
			if (httpcfg->origin.u.rtsp.server[n]->hostname) {
			    free((void*)httpcfg->origin.u.rtsp.server[n]->hostname);
			    httpcfg->origin.u.rtsp.server[n]->hostname = 0;
		        }
		        free(httpcfg->origin.u.rtsp.server[n]);
		        httpcfg->origin.u.rtsp.server[n] = 0;
		    }
		}
		free(httpcfg->origin.u.rtsp.server);
		httpcfg->origin.u.rtsp.server = 0;
	}
    }


    if (httpcfg->policies.cache_age_policies.entries) {
    	content_type_max_age_t **ctype_maxage_map;
	ctype_maxage_map = 
		httpcfg->policies.cache_age_policies.content_type_to_max_age;
    	for (n = 0; n < httpcfg->policies.cache_age_policies.entries; n++) {
	    if (ctype_maxage_map[n]) {
	    	if (ctype_maxage_map[n]->content_type) {
	    	    free(ctype_maxage_map[n]->content_type);
		}
	    	if (ctype_maxage_map[n]->max_age) {
	    	    free(ctype_maxage_map[n]->max_age);
		}
		free(ctype_maxage_map[n]);
	    }
	}
	free(ctype_maxage_map);
    } else if (
	httpcfg->policies.cache_age_policies.content_type_to_max_age != NULL) {
	// BZ 5574 - if no entries were there, then we do
	// not free this array. this causes a leak. Free it here
	// when no entries were loaded to this policy set.
	free(httpcfg->policies.cache_age_policies.content_type_to_max_age);
	httpcfg->policies.cache_age_policies.content_type_to_max_age = NULL;
    }


    if (httpcfg->delete_response_headers) {
    	for (n = 0; n < httpcfg->num_delete_response_headers; n++) {
	    if (httpcfg->delete_response_headers[n].name) {
	    	free(httpcfg->delete_response_headers[n].name);
		httpcfg->delete_response_headers[n].name = 0;
	    }
	}
	free(httpcfg->delete_response_headers);
	httpcfg->delete_response_headers = 0;
    }

    if (httpcfg->add_response_headers) {
    	for (n = 0; n < httpcfg->num_add_response_headers; n++) {
	    if (httpcfg->add_response_headers[n].name) {
	    	free(httpcfg->add_response_headers[n].name);
	    	httpcfg->add_response_headers[n].name = 0;
	    }
	    if (httpcfg->add_response_headers[n].value) {
	    	free(httpcfg->add_response_headers[n].value);
	    	httpcfg->add_response_headers[n].value = 0;
	    }
	}
	free(httpcfg->add_response_headers);
	httpcfg->add_response_headers = 0;
    }

    if (httpcfg->request_policies.accept_headers) {
	for (n = 0; n < MAX_CLIENT_REQUEST_HEADERS; n++) {
	    if (httpcfg->request_policies.accept_headers[n].name) {
		free(httpcfg->request_policies.accept_headers[n].name);
		httpcfg->request_policies.accept_headers[n].name = 0;
	    }
        }
	free(httpcfg->request_policies.accept_headers);
	httpcfg->request_policies.accept_headers = 0;
    }

    /* remove custom cache-control header, if configured */
    if (httpcfg->policies.cache_redirect.hdr_name != NULL) {
	free(httpcfg->policies.cache_redirect.hdr_name);
	httpcfg->policies.cache_redirect.hdr_len = 0;
    }

    if (httpcfg->policy_engine_config.policy_file)
        free(httpcfg->policy_engine_config.policy_file);

    if (httpcfg->geo_ip_cfg.api_url)
	free(httpcfg->geo_ip_cfg.api_url);
    
    /* cleanup policy engine memory. Do it here till a
     * better place is found.
     */  
#if 0
    if (httpcfg->policy_engine_config.policy_buf)
        free(httpcfg->policy_engine_config.policy_buf);
    if (httpcfg->policy_engine_config.policy_data)
        pe_cleanup(httpcfg->policy_engine_config.policy_data);
#endif

    if (httpcfg->cache_index.ldn_name) {
	free(httpcfg->cache_index.ldn_name);
	httpcfg->cache_index.ldn_name = 0;
    }

    if (httpcfg->policies.cache_revalidate.header_name) {
	free(httpcfg->policies.cache_revalidate.header_name);
	httpcfg->policies.cache_revalidate.header_name = 0;
    }

	for (n=0; n < httpcfg->cache_index.headers_count; n++) {
		free(httpcfg->cache_index.include_header_names[n]);
		httpcfg->cache_index.include_header_names[n] = 0;
	}

    if (httpcfg->request_policies.dynamic_uri.regex) {
	free(httpcfg->request_policies.dynamic_uri.regex);
	httpcfg->request_policies.dynamic_uri.regex = 0;
    }

    if (httpcfg->request_policies.dynamic_uri.mapping_string) {
	free(httpcfg->request_policies.dynamic_uri.mapping_string);
	httpcfg->request_policies.dynamic_uri.mapping_string = 0;
    }

	if (httpcfg->request_policies.dynamic_uri.tokenize_string) {
	free(httpcfg->request_policies.dynamic_uri.tokenize_string);
	httpcfg->request_policies.dynamic_uri.tokenize_string = 0;
	}

    if (httpcfg->request_policies.dynamic_uri.regctx) {
	nkn_regex_ctx_free(httpcfg->request_policies.dynamic_uri.regctx);
	httpcfg->request_policies.dynamic_uri.regctx = 0;
    }

	if (httpcfg->request_policies.seek_uri.regex) {
	free(httpcfg->request_policies.seek_uri.regex);
	httpcfg->request_policies.seek_uri.regex = 0;
	}

	if (httpcfg->request_policies.seek_uri.mapping_string) {
	free(httpcfg->request_policies.seek_uri.mapping_string);
	httpcfg->request_policies.seek_uri.mapping_string = 0;
	}

	if (httpcfg->request_policies.seek_uri.regctx) {
	nkn_regex_ctx_free(httpcfg->request_policies.seek_uri.regctx);
	httpcfg->request_policies.seek_uri.regctx = 0;
	}

    if (httpcfg->policies.host_hdr_value) {
	free(httpcfg->policies.host_hdr_value);
        httpcfg->policies.host_hdr_value = NULL;
    }

    for (n = 0; n < MAX_USER_AGENT_GROUPS; n++) {    
	if (httpcfg->vary_hdr_cfg.user_agent_regex_ctx[n]) {
	    nkn_regex_ctx_free(httpcfg->vary_hdr_cfg.user_agent_regex_ctx[n]);
	}
	//if (httpcfg->vary_hdr_cfg.user_agent_regex[n]) {
	//    free(httpcfg->vary_hdr_cfg.user_agent_regex[n]);
	//}
    }

    free(httpcfg);
}

static
int setup_origin_server_map(const host_origin_map_bin_t *hom, 
			    hash_entry_t **hash_hostnames /* output */,
			    hash_table_def_t **ht_hostnames /* output */)
{
    int rv;
    int ret = 0;
    int hash_entry_size = 128;
    int n;
    const char *hostname;
    int hostname_strlen;
    int hash_ret_data;

    if (!hom || !hash_hostnames || !ht_hostnames) {
	DBG_LOG(MSG, MOD_NAMESPACE, "Invalid args, "
		"hom=%p hash_hostnames=%p ht_hostnames=%p",
		hom, hash_hostnames, ht_hostnames);
	ret = 1;
	goto exit;
    }
    *hash_hostnames = 0;
    *ht_hostnames = 0;

    *hash_hostnames = (hash_entry_t *)nkn_calloc_type(1, 
    				hash_entry_size * sizeof(hash_entry_t), 
				mod_ns_hash_entry_t);
    if (!(*hash_hostnames)) {
	DBG_LOG(MSG, MOD_NAMESPACE, "calloc() hash_entry_t(s) failed, size=%lu",
		hash_entry_size * sizeof(hash_entry_t));
	ret = 2;
	goto exit;
    }

    *ht_hostnames = (hash_table_def_t *)nkn_calloc_type(1, 
    						sizeof(hash_table_def_t), 
						mod_ns_hash_table_def_t);
    if (!(*ht_hostnames)) {
	DBG_LOG(MSG, MOD_NAMESPACE, "calloc() hash_table_def_t(s) "
		"failed, size=%lu", sizeof(hash_table_def_t));
	ret = 3;
	goto exit;
    }

    **ht_hostnames = ht_base_nocase_hash;
    (*ht_hostnames)->ht = *hash_hostnames;
    (*ht_hostnames)->size = hash_entry_size;

    for (n = 0; n < (int)hom->num_entries; n++) {
    	hostname = ISTR(hom, string_table_offset, hom->entry[n].host);
	hostname_strlen = strlen(hostname);

	rv = (*((*ht_hostnames)->lookup_func))(*ht_hostnames,
				hostname, hostname_strlen, &hash_ret_data);
	if (rv) {
	    rv = (*((*ht_hostnames)->add_func))(*ht_hostnames,
				hostname, hostname_strlen, n);
	    if (!rv) {
	    	DBG_LOG(MSG, MOD_NAMESPACE,
			"hash add, n=%d hostname=%s => origin=%s port=%hd", 
			n, hostname, 
			ISTR(hom, string_table_offset, 
				hom->entry[n].origin_host),
			hom->entry[n].origin_port);
	    } else {
	    	DBG_LOG(MSG, MOD_NAMESPACE,
			"hash add failed, rv=%d n=%d hostname=%s", rv, n, hostname);
		ret = 4;
		goto exit;
	    }
	} else {
	    // already exists, ignore entry
	    DBG_LOG(MSG, MOD_NAMESPACE,
	    	    "Entry already exists, ignoring  n=%d hostname=%s", 
	    	    n, hostname);
	}
    }
    return 0;

exit:

    if (hash_hostnames && *hash_hostnames) {
    	free(*hash_hostnames);
    	*hash_hostnames = 0;
    }

    if (ht_hostnames && *ht_hostnames) {
    	free(*ht_hostnames);
	*ht_hostnames = 0;
    }
    return ret;
}

static 
int setup_origin_nodemap(namespace_node_data_t *nd, http_config_t *httpcfg,
			 namespace_config_t *nsc, 
			 const namespace_config_t *old_nsc)
{
    int retval = 0;
    int n;
    int valid_nth = 0;
    node_map_descriptor_t *nmd;
    server_map_node_data_t *nd_smap;
    cluster_monitor_params_t *nd_cmp;
    cluster_hash_config_t *ch_cfg;
    int online;

    if (!nd->origin_server.http_smap_count) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, 
		"namespace[%s]: No NodeMaps defined, http_smap_count=%d", 
		nd->namespace, nd->origin_server.http_smap_count);
	DBG_ERR(SEVERE,
		"namespace[%s]: No NodeMaps defined, http_smap_count=%d", 
		nd->namespace, nd->origin_server.http_smap_count);
	retval = 1;
	goto exit;
    }

    httpcfg->origin.u.http.node_map = 
    	nkn_calloc_type(1, (sizeof(node_map_descriptor_t *) * 1024), 
			mod_ns_node_map_descriptor_t);
    if (!httpcfg->origin.u.http.node_map) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, 
	        "namespace[%s]: nkn_calloc_type() failed", nd->namespace);
	DBG_ERR(SEVERE, "namespace[%s]: nkn_calloc_type() failed", 
		nd->namespace);
	retval = 2;
	goto exit;
    }

    httpcfg->origin.u.http.node_map_private_data = 
    	nkn_calloc_type(1, (sizeof(char *) * 1024), 
			mod_ns_node_map_private_data_descriptor_t);
    if (!httpcfg->origin.u.http.node_map_private_data) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, 
	        "namespace[%s]: nkn_calloc_type() failed", nd->namespace);
	DBG_ERR(SEVERE, "namespace[%s]: nkn_calloc_type() failed", 
		nd->namespace);
	retval = 21;
	goto exit;
    }

    for (n = 0; n < nd->origin_server.http_smap_count; n++) {
    	/*
     	 * Extract the binary form of the NodeMap(s) from namespace_node_data_t
     	 * and instantiate the appropriate node_map_descriptor_t constructors.
     	 */
	nd_smap = nd->origin_server.http_server_map[n];
	if (!nd_smap) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "origin_server.http_server_map[%d]=0", n);
	    continue;
	}

	switch (nd_smap->format_type) {
	case SM_FT_CLUSTER_MAP:
	{
	    cluster_map_bin_t *cl_map = 
	    	(cluster_map_bin_t *) nd_smap->binary_server_map;

	    if (!cl_map || (cl_map->version != CLM_VERSION) || 
	    	(cl_map->magicno != CLM_MAGICNO)) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE,
			"namespace[%s]: Invalid SM_FT_CLUSTER_MAP data, "
			"cl_map=%p vers=%d %d magic=0x%x 0x%x",
			nd->namespace, cl_map,
			cl_map ? cl_map->version : 0, CLM_VERSION,
			cl_map ? cl_map->magicno : 0, CLM_MAGICNO);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: Invalid SM_FT_CLUSTER_MAP data, "
			"cl_map=%p vers=%d %d magic=0x%x 0x%x",
			nd->namespace, cl_map,
			cl_map ? cl_map->version : 0, CLM_VERSION,
			cl_map ? cl_map->magicno : 0, CLM_MAGICNO);
	    	retval = 4;
	    	goto exit;

	    }

	    httpcfg->origin.u.http.node_map_private_data[n] = 
	    	nkn_calloc_type(1, sizeof(cluster_hash_config_t),
				mod_ns_cluster_hash_config_t);
	    ch_cfg = (cluster_hash_config_t *)
	    		httpcfg->origin.u.http.node_map_private_data[n];
	    if (ch_cfg) {
	        ch_cfg->ch_uri_base = (nd->hash_url == CH_BASE_URL);
	    } else {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE, 
			"namespace[%s]: alloc 'cluster_hash_config_t' failed, "
			"[n=%d]", nd->namespace, n);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: alloc 'cluster_hash_config_t' failed, "
			"[n=%d]", nd->namespace, n);
	    	retval = 41;
	    	goto exit;
	    }

	    nmd = new_nmd_cluster_map(cl_map, nd_smap->binary_size,
				      nd->namespace, nd->uid, 
				      ch_cfg, nsc, old_nsc, &online);
	    if (!nmd) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE, 
			"namespace[%s]: new_nmd_cluster_map() failed, "
			"invalid map data [n=%d]", nd->namespace, n);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: new_nmd_cluster_map() failed, "
			"invalid map data [n=%d]", nd->namespace, n);
	    	retval = 5;
	    	goto exit;
	    }

	    if (online) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"namespace[%s]:%d online", nd->namespace, n); 
	    } else {
	    	AO_fetch_and_add1(&nsc->init_in_progress);
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"namespace[%s]:%d initializing", nd->namespace, n); 
	    }
	    break;
	}

	case SM_FT_ORIG_ESC_MAP:
	{
	    origin_escalation_map_bin_t *oe_map = 
	    	(origin_escalation_map_bin_t *) nd_smap->binary_server_map;

	    if (!oe_map || (oe_map->version != OREM_VERSION) || 
	    	(oe_map->magicno != OREM_MAGICNO)) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE,
			"namespace[%s]: Invalid SM_FT_ORIG_ESC_MAP data, "
			"oe_map=%p vers=%d %d magic=0x%x 0x%x",
			nd->namespace, oe_map,
			oe_map ? oe_map->version : 0, OREM_VERSION,
			oe_map ? oe_map->magicno : 0, OREM_MAGICNO);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: Invalid SM_FT_ORIG_ESC_MAP data, "
			"oe_map=%p vers=%d %d magic=0x%x 0x%x",
			nd->namespace, oe_map,
			oe_map ? oe_map->version : 0, OREM_VERSION,
			oe_map ? oe_map->magicno : 0, OREM_MAGICNO);
	    	retval = 6;
	    	goto exit;

	    }
	    nmd = new_nmd_origin_escalation_map(oe_map, nd_smap->binary_size,
    						nd->namespace, nd->uid, 
						nsc, old_nsc, &online);
	    if (!nmd) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE, 
			"namespace[%s]: new_nmd_origin_escalation_map() "
			"failed, invalid map data [n=%d]", nd->namespace, n);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: new_nmd_origin_escalation_map() "
			"failed, invalid map data [n=%d]", nd->namespace, n);
	    	retval = 7;
	    	goto exit;
	    }

	    if (online) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"namespace[%s]:%d online", nd->namespace, n); 
	    } else {
	    	AO_fetch_and_add1(&nsc->init_in_progress);
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"namespace[%s]:%d initializing", nd->namespace, n); 
	    }
	    break;
	}

	case SM_FT_ORIG_RR_MAP:
	{
	    origin_round_robin_map_bin_t *orr_map = 
	    	(origin_round_robin_map_bin_t *) nd_smap->binary_server_map;

	    if (!orr_map || (orr_map->version != ORRRM_VERSION) || 
	    	(orr_map->magicno != ORRRM_MAGICNO)) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE,
			"namespace[%s]: Invalid SM_FT_ORIG_RR_MAP data, "
			"orr_map=%p vers=%d %d magic=0x%x 0x%x",
			nd->namespace, orr_map,
			orr_map ? orr_map->version : 0, ORRRM_VERSION,
			orr_map ? orr_map->magicno : 0, ORRRM_MAGICNO);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: Invalid SM_FT_ORIG_RR_MAP data, "
			"orr_map=%p vers=%d %d magic=0x%x 0x%x",
			nd->namespace, orr_map,
			orr_map ? orr_map->version : 0, ORRRM_VERSION,
			orr_map ? orr_map->magicno : 0, ORRRM_MAGICNO);
	    	retval = 16;
	    	goto exit;

	    }
	    nmd = new_nmd_origin_roundrobin_map(orr_map, nd_smap->binary_size,
    						nd->namespace, nd->uid, 
						nsc, old_nsc, &online);
	    if (!nmd) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE, 
			"namespace[%s]: new_nmd_origin_roundrobin_map() "
			"failed, invalid map data [n=%d]", nd->namespace, n);
	    	DBG_ERR(SEVERE,
			"namespace[%s]: new_nmd_origin_roundrobin_map() "
			"failed, invalid map data [n=%d]", nd->namespace, n);
	    	retval = 17;
	    	goto exit;
	    }

	    if (online) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"namespace[%s]:%d online", nd->namespace, n); 
	    } else {
	    	AO_fetch_and_add1(&nsc->init_in_progress);
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"namespace[%s]:%d initializing", nd->namespace, n); 
	    }
	    break;
	}

	default:
	    DBG_LOG(SEVERE, MOD_NAMESPACE, 
		    "namespace[%s]: Invalid http_server_map[%d].format_type=%d",
		    nd->namespace, n, nd_smap->format_type);
	    DBG_ERR(SEVERE,
		    "namespace[%s]: Invalid http_server_map[%d].format_type=%d",
		    nd->namespace, n, nd_smap->format_type);
	    retval = 8;
	    goto exit;
	    break;
	}

	nd_cmp = &nd->origin_server.http_server_map[n]->monitor_values;

	nmd->cmm_heartbeat_interval = nd_cmp->heartbeat_interval;
	nmd->cmm_connect_timeout = nd_cmp->connect_timeout;
	nmd->cmm_allowed_connect_failures = nd_cmp->allowed_fails;
	nmd->cmm_read_timeout = nd_cmp->read_timeout;

    	httpcfg->origin.u.http.node_map[valid_nth++] = nmd;
    }
    httpcfg->origin.u.http.node_map_entries = valid_nth;

exit:

    return retval;
}

/********************************************************************** 
 *	Function : get_part_from_cache_prefix ()
 *
 *	Purpose : The cache prefix has a multitude of information
 *	in it. This function lets you get the relevant part from 
 *	the prefix based on the given enum. 
 *
 *	Returns; An allocated string with the value that has to be
 *		freed by the caller
 ********************************************************************* */
char*
get_part_from_cache_prefix(const char* cp_cache_prefix,
			en_cp_part_type part_type, uint32_t cache_prefix_len)
{
	char	*cp_retval = NULL;
	char	*cp_buffer;
	uint32_t	len = 0;
	const char	*cp_start = NULL;
	const char	*cp_end = NULL;
	const char	*cp_cache_prefix_end = NULL;


	/* Sanity test */
	if (!cp_cache_prefix)
		return (NULL);

	/* -------------------------------------------------------
		The cache prefix is expected to be of the format:

		/<namespace>:<uuid>_<origin>:<port>

	 -------------------------------------------------------*/
	
	/* Check for the forward slash first */
	if ('/' != cp_cache_prefix [0])
		return (NULL);

	cp_cache_prefix_end = cp_cache_prefix + cache_prefix_len;
	cp_start = cp_cache_prefix;
	cp_start++; // skipping the "/"

	/* Get the first delimiter namely ':' */
	/* Avoiding use of strchr as we need to check only with the length */
	cp_end = cp_start;
	while ((*cp_end != ':') &&
			(cp_end < cp_cache_prefix_end))
		cp_end++;

	if (*cp_end != ':')
		return (NULL);

	/* If part_type is namespace then return it */
	if (part_type == CP_PART_NAMESPACE) {
		/* Get the length of the namespace part */
		len = cp_end - cp_start;

		cp_buffer = alloca(len + 1);

		/* Now copy the namespace into the buffer */
		strncpy (cp_buffer, cp_start, len);
		cp_buffer [len] = '\0';

		/* Now ready to allocate and return */
		cp_retval = nkn_strdup_type(cp_buffer, mod_mgmt_charbuf);

		return (cp_retval);
	}

	/* Advance the start pointer */
	cp_start = cp_end + 1;

	/* Get the first delimiter namely '_' */
	/* Avoiding use of strchr as we need to check only with the length */
	cp_end = cp_start;
	while ((*cp_end != '_') &&
			(cp_end < cp_cache_prefix_end))
		cp_end++;

	if (*cp_end != '_')
		return (NULL);

	/* Now get UUID */
	if (part_type == CP_PART_UUID) {
		/* Get the length of the uuid part */
		len = cp_end - cp_start;

		cp_buffer = alloca(len + 1);

		/* Now copy the UUID into the buffer */
		strncpy (cp_buffer, cp_start, len);
		cp_buffer [len] = '\0';

		/* Now ready to allocate and return */
		cp_retval = nkn_strdup_type(cp_buffer, mod_mgmt_charbuf);

		return (cp_retval);
	}

	/* Advance the start pointer */
	cp_start = cp_end + 1;

	/* Get the first delimiter namely ':' */
	/* Avoiding use of strchr as we need to check only with the length */
	cp_end = cp_start;
	while ((*cp_end != ':') &&
			(cp_end < cp_cache_prefix_end))
		cp_end++;

	if (*cp_end != ':')
		return (NULL);

	/* Now get server info */
	if (part_type == CP_PART_SERVER) {
		/* Get the length of the server part */
		len = cp_end - cp_start;

		cp_buffer = alloca(len + 1);

		/* Now copy the server into the buffer */
		strncpy (cp_buffer, cp_start, len);
		cp_buffer [len] = '\0';

		/* Now ready to allocate and return */
		cp_retval = nkn_strdup_type(cp_buffer, mod_mgmt_charbuf);

		return (cp_retval);
	}

	/* Advance the start pointer */
	cp_start = cp_end + 1;

	/* Since this is the last part just use the end of the string */
	cp_end = cp_cache_prefix_end;

	/* Now get the port */
	if (part_type == CP_PART_PORT) {
		/* Get the length of the port part */
		len = cp_end - cp_start;

		cp_buffer = alloca(len + 1);

		/* Now copy the port into the buffer */
		strncpy (cp_buffer, cp_start, len);
		cp_buffer [len] = '\0';

		/* Now ready to allocate and return */
		cp_retval = nkn_strdup_type(cp_buffer, mod_mgmt_charbuf);

		return (cp_retval);
	}

	return (NULL);

} /* end of get_part_from_cache_prefix () */

pub_point_config_t *new_pub_point_config_t(namespace_node_data_t *nd)
{
    int n, nth;

    pub_point_config_t *pp = (pub_point_config_t*) nkn_calloc_type(1, 
		                        sizeof(pub_point_config_t),
					mod_ns_pub_point_config_t);
    if(!pp) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	return 0;
    }

    pp->events = nkn_calloc_type(1, 
		    sizeof(pub_point_t *) * MAX_PUB_POINT,
		    mod_ns_pub_point_config_t);
    if (!pp->events) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	delete_pub_point_config_t(pp);
	return 0;
    }

    pp->entries = 0;
    for (n = 0; n < MAX_PUB_POINT; n++) {
	if (nd->rtsp_live_pub_point[n].name) {
	    nth = pp->entries;
	    pp->events[nth] = nkn_calloc_type(1, 
			    sizeof(pub_point_t), 
			    mod_ns_pub_point_config_t);

	    pp->events[nth]->name = 
		nkn_strdup_type(nd->rtsp_live_pub_point[n].name, 
				mod_ns_pub_point_t);

	    if (!pp->events[nth]->name) {
		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
		delete_pub_point_config_t(pp);
		return 0;
	    }

	    pp->events[nth]->enable = nd->rtsp_live_pub_point[n].enable; 

	    switch(nd->rtsp_live_pub_point[n].mode) {
	    case PUB_SERVER_MODE_NONE:
		pp->events[nth]->mode = PS_MODE_UNDEFINED;
		break;
	    case PUB_SERVER_MODE_ON_DEMAND:
		pp->events[nth]->mode = PS_MODE_PULL;
		break;
	    case PUB_SERVER_MODE_IMMEDIATE:
		pp->events[nth]->mode = PS_MODE_PUSH;
		if (nd->rtsp_live_pub_point[n].sdp_static) {
		    pp->events[nth]->sdp_static = 
			nkn_strdup_type(nd->rtsp_live_pub_point[n].sdp_static,
					mod_ns_pub_point_t);
		    if (!pp->events[nth]->sdp_static) {
			DBG_LOG(MSG, MOD_NAMESPACE, 
				"nkn_strdup_type() failed");
			delete_pub_point_config_t(pp);
			return 0;
		    }
		}
		break;
	    case PUB_SERVER_MODE_TIMED:
		pp->events[nth]->mode = PS_MODE_TIMED;
		if (nd->rtsp_live_pub_point[n].sdp_static) {
		    pp->events[nth]->sdp_static = 
			nkn_strdup_type(nd->rtsp_live_pub_point[n].sdp_static,
					mod_ns_pub_point_t);
		    if (!pp->events[nth]->sdp_static) {
			DBG_LOG(MSG, MOD_NAMESPACE, 
				"nkn_strdup_type() failed");
			delete_pub_point_config_t(pp);
			return 0;
		    }

		    if (nd->rtsp_live_pub_point[n].start_time) {
			pp->events[nth]->start_time = 
			    nkn_strdup_type(nd->rtsp_live_pub_point[n].start_time,
					    mod_ns_pub_point_t);

		        if (!pp->events[nth]->start_time) {
			    DBG_LOG(MSG, MOD_NAMESPACE, 
			  	    "nkn_strdup_type() failed");
			    delete_pub_point_config_t(pp);
			    return 0;
		        }
		    }

		    if (nd->rtsp_live_pub_point[n].end_time) {
			pp->events[nth]->end_time = 
			    nkn_strdup_type(nd->rtsp_live_pub_point[n].end_time,
					    mod_ns_pub_point_t);

		        if (!pp->events[nth]->end_time) {
			    DBG_LOG(MSG, MOD_NAMESPACE, 
			  	    "nkn_strdup_type() failed");
			    delete_pub_point_config_t(pp);
			    return 0;
		        }
		    }
		}
		break;
	    }

	    pp->events[nth]->cacheable = 
		nd->rtsp_live_pub_point[n].caching_enabled;

	    if (nd->rtsp_live_pub_point[n].cache_format_chunked_ts)
		pp->events[nth]->cache_format = PP_CACHE_FORMAT_CHUNKED_TS;
	    else if (nd->rtsp_live_pub_point[n].cache_format_moof)
		pp->events[nth]->cache_format = PP_CACHE_FORMAT_MOOF;
	    else if (nd->rtsp_live_pub_point[n].cache_format_frag_mp4)
		pp->events[nth]->cache_format = PP_CACHE_FORMAT_FRAG_MP4;
	    else if (nd->rtsp_live_pub_point[n].cache_format_chunk_flv)
		pp->events[nth]->cache_format = PP_CACHE_FORMAT_CHUNK_FLV;

	    pp->entries++;
	}
    }

    return pp;
} /* end of new_pub_point_config_t() */

void delete_pub_point_config_t(pub_point_config_t *pp)
{
    int nth;
    if (!pp) {
	return;
    }
    for (nth = 0; nth < MAX_PUB_POINT; nth++) {

	if (pp->events && pp->events[nth]) {
	    if (pp->events[nth]->name)
	        free(pp->events[nth]->name);
	    if (pp->events[nth]->sdp_static)
	        free(pp->events[nth]->sdp_static);
	    if (pp->events[nth]->start_time)
    	        free(pp->events[nth]->start_time);
	    if (pp->events[nth]->end_time)
	        free(pp->events[nth]->end_time);

	    if (pp->events[nth])
		free(pp->events[nth]);
	}
    }

    if (pp->events)
        free(pp->events);

    free(pp);
}

rtsp_config_t *new_rtsp_config_t(namespace_node_data_t *nd)
{
    rtsp_config_t *rp = (rtsp_config_t*) nkn_calloc_type(1, 
		    			    sizeof(rtsp_config_t),
					    mod_ns_pub_point_config_t);
    if (!rp) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	return 0;
    }

    if (nd->rtsp_origin_fetch.cache_directive_no_cache) {
	if (strcasecmp("follow",
	        nd->rtsp_origin_fetch.cache_directive_no_cache) == 0) {
	    rp->policies.cache_no_cache = NO_CACHE_FOLLOW;
	} else if (strcasecmp("override",
		nd->rtsp_origin_fetch.cache_directive_no_cache) == 0) {
	    rp->policies.cache_no_cache = NO_CACHE_OVERRIDE;
	}
    }
    else {
	rp->policies.cache_no_cache = NO_CACHE_FOLLOW;
    }

    rp->policies.cache_age_default = 
	nd->rtsp_origin_fetch.cache_age;
    rp->policies.store_cache_min_age = 
	nd->rtsp_origin_fetch.content_store_cache_age_threshold;

    rp->max_sessions = nd->rtsp_max_conns;

    rp->cache_index.ldn_name =
	nkn_strdup_type(nd->rtsp_cache_index.ldn_name, mod_ns_ldn_name_buf);
    rp->cache_index.exclude_domain =
	nd->rtsp_cache_index.exclude_domain;

    return rp;
}

void delete_rtsp_config_t(rtsp_config_t *rtsp)
{
    if (!rtsp) {
	return;
    }

    if (rtsp->cache_index.ldn_name) {
	free(rtsp->cache_index.ldn_name);
    }

    // BZ 5574: free rtsp to avoid leak
    free(rtsp);
    return;

}

static int hostport_to_data(const char *host_port, int host_port_strlen,
			    uint32_t *ip_host_byte, uint32_t *port_host_byte)
{
    int rv;
    int domain_len;
    uint16_t port;
    char *ip_str;
    struct in_addr inaddr;

    rv = parse_host_header(host_port, host_port_strlen, &domain_len, &port, 0);
    if (rv) {
    	DBG_LOG(MSG, MOD_NAMESPACE, 
		"parse_host_header() failed, rv=%d host_port=%s", 
		rv, host_port);
	return 1; // Error
    }

    ip_str = alloca(domain_len+1);
    memcpy(ip_str, host_port, domain_len);
    ip_str[domain_len] = '\0';

    rv = inet_aton((const char *)ip_str, &inaddr);
    if (rv) {
    	*ip_host_byte = ntohl(inaddr.s_addr);
	*port_host_byte = port;
	return 0;
    } else {
    	DBG_LOG(MSG, MOD_NAMESPACE, 
		"inet_aton() failed, rv=%d ip=%s", rv, ip_str);
    	return 2; // Error
    }

}

static void setup_cmm_monitor_config(node_map_descriptor_t *nmd, 
				     const cluster_monitor_params_t *clmp)
{

    nmd->cmm_heartbeat_interval = clmp->heartbeat_interval;
    nmd->cmm_connect_timeout = clmp->connect_timeout;
    nmd->cmm_allowed_connect_failures = clmp->allowed_fails;
    nmd->cmm_read_timeout = clmp->read_timeout;
}

// Cluster L7 suffix map trie interfaces

static void *trie_suffix_node_copy(nkn_trie_node_t nd)
{
    suffix_map_trie_node_t *smn;

    smn = nkn_calloc_type(1, sizeof(*smn), mod_ns_cl_suffix_node_t);
    if (smn) {
    	*smn = *((suffix_map_trie_node_t *)nd);
    }
    return smn;
}

static void trie_suffix_node_destruct(nkn_trie_node_t nd)
{
    suffix_map_trie_node_t *smn = (suffix_map_trie_node_t *)nd;
    if (smn) {
    	smn->magicno = 0;
    	free((void *) nd);
    }
}

static nkn_trie_t setup_cl_suffix_map(cluster_node_data_t *pnd)
{
    nkn_trie_t trie;
    suffix_map_trie_node_t smn;
    int entry;
    int ret;

    memset((char *)&smn, 0, sizeof(smn));
    smn.magicno = SFXMAP_TNODE_MAGIC;

    trie = nkn_trie_create(trie_suffix_node_copy, trie_suffix_node_destruct);
    if (!trie) {
    	DBG_LOG(MSG, MOD_CLUSTER, "nkn_trie_create() failed, trie=%p", trie);
	return trie;
    }

    for (entry = 0; entry < pnd->suffix_map_entries; entry++) {
    	if (!pnd->suffix_map[entry].valid) {
	    continue;
	}
	smn.type = (pnd->suffix_map[entry].action == 1) ? 
				INTERCEPT_PROXY : INTERCEPT_REDIRECT;
    	ret = nkn_trie_add(trie, pnd->suffix_map[entry].suffix_name, 
			   (nkn_trie_node_t) &smn);
	if (!ret) {
	    DBG_LOG(MSG, MOD_CLUSTER, "Add suffix, ix=%d key=%s action=%s",
		    entry, pnd->suffix_map[entry].suffix_name,
		    smn.type == INTERCEPT_PROXY ? "Proxy" : "Redirect");
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER, "nkn_trie_add(%s) failed, index=%d rv=%d",
		    pnd->suffix_map[entry].suffix_name, entry, ret);
	}
    }
    return trie;
}

cluster_config_t *new_cluster_config_t(namespace_node_data_t *nd,
				       namespace_config_t *nsc, 
				       const namespace_config_t *old_nsc)
{
    int rv;
    int n;
    const char *hostport;
    int hostport_strlen;
    cluster_descriptor_t *pcld;
    cluster_node_data_t *nd_cl;
    char *nd_cl_name;
    int load_metric;
    int online;

    cluster_config_t *clcfg = (cluster_config_t *)nkn_calloc_type(1, 
    						sizeof(cluster_config_t),
    						mod_ns_cluster_config_t);
    if (!clcfg) {
    	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }

    clcfg->cld = 
    	nkn_calloc_type(1, (sizeof(cluster_descriptor_t *) * 1024), 
			mod_ns_cluster_descriptor_t);
    if (!clcfg->cld) {
	goto err_exit;
    }


    for (n = 0; n < nd->num_clusters; n++) {
    	clcfg->cld[n] = (cluster_descriptor_t *)nkn_calloc_type(1,
    						sizeof(cluster_descriptor_t),
						mod_ns_cluster_descriptor_t);
    	if (!(pcld = clcfg->cld[n])) {
	    goto err_exit;
	}
    	clcfg->num_cld++;

	pcld->node_id_to_ns_index = nkn_calloc_type(1,
					(sizeof(int) * MAX_CLUSTER_MAP_NODES),
					mod_ns_cluster_desc_nodeid_map_t);
	if (!pcld->node_id_to_ns_index) {
	    goto err_exit;
	}
	nd_cl = nd->clusterMap[n];
	nd_cl_name = nd->cluster_map_name[n];

	/*
	 * Setup action remap trie
	 */
	pcld->suffix_map = setup_cl_suffix_map(nd_cl);

	switch (nd_cl->cl_type) {
	case CL_TYPE_REDIRECT:
	    switch (nd_cl->u.redirect_cluster.method) {
	    case CONSISTENT_HASH_REDIRECT:
	    	pcld->type = CLT_CH_REDIRECT;

		/* Setup intercept_attr */
		hostport = nd_cl->cnd_redir.rd_data.addr_port;
		hostport_strlen = hostport ? strlen(hostport) : 0;

		if (hostport && hostport_strlen) {
		    rv = hostport_to_data(hostport, hostport_strlen,
		    		&pcld->u.ch_redirect.intercept_attr.ip,
				&pcld->u.ch_redirect.intercept_attr.port);
		    if (rv) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) "
				"hostport_to_data() failed, rv=%d",
				nd->namespace, n, nd_cl_name, rv); 
	    		goto err_exit;
		    }
		}

		/* Setup ch_attr */
		pcld->u.ch_redirect.ch_attr.ch_uri_base = 
		    (nd_cl->hash_url == CL_BASE_URL);

		/* Setup redir_attr */
	    	pcld->u.ch_redirect.redir_attr.allow_redir_local = 
		    nd_cl->cnd_redir.rd_data.local_redirect;

		if (nd_cl->cnd_redir.rd_data.baseRedirectPath) {
		    pcld->u.ch_redirect.redir_attr.path_prefix = 
			nkn_strdup_type(
			    	nd_cl->cnd_redir.rd_data.baseRedirectPath,
				mod_ns_base_redir_path);
		    pcld->u.ch_redirect.redir_attr.path_prefix_strlen =
			strlen(pcld->u.ch_redirect.redir_attr.path_prefix);
		}

		/* Setup NodeMap */
	    	pcld->u.ch_redirect.node_map = 
		    new_nmd_noload_cluster_map(
				(const cluster_map_bin_t *)
				nd_cl->serverMap->binary_server_map,
				nd_cl->serverMap->binary_size,
				nd->namespace, nd->uid,
				&pcld->u.ch_redirect.ch_attr,
				nsc, old_nsc, &online);

	    	if (pcld->u.ch_redirect.node_map) {
		    if (online) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) online", 
				nd->namespace, n, nd_cl_name); 
		    } else {
		    	AO_fetch_and_add1(&nsc->init_in_progress);
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) initializing", 
				nd->namespace, n, nd_cl_name); 
		    }

		    setup_cmm_monitor_config(pcld->u.ch_redirect.node_map,
					     &nd_cl->serverMap->monitor_values);
	    	} else {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "new_nmd_noload_cluster_map() failed, "
			    "namespace[%s]:%d(%s)", 
			    nd->namespace, n, nd_cl_name);
		    goto err_exit;
	    	}
	    	break;

	    case CHASH_REDIRECT_REPLICATE:
	    	pcld->type = CLT_CH_REDIRECT_LB;

		/* Setup intercept_attr */
		hostport = nd_cl->cnd_redir_repl.rd_data.addr_port;
		hostport_strlen = hostport ? strlen(hostport) : 0;

		if (hostport && hostport_strlen) {
		    rv = hostport_to_data(hostport, hostport_strlen,
		    		&pcld->u.ch_redirect_lb.intercept_attr.ip,
				&pcld->u.ch_redirect_lb.intercept_attr.port);
		    if (rv) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) "
				"hostport_to_data() failed, rv=%d",
				nd->namespace, n, nd_cl_name, rv); 
	    		goto err_exit;
		    }
		}

		/* Setup ch_attr */
		pcld->u.ch_redirect_lb.ch_attr.ch_uri_base =
		    (nd_cl->hash_url == CL_BASE_URL);

		/* Setup lb_attr */
		if (nd_cl->cnd_redir_repl.lb_data.replicate == 
		    REPLICATE_LEAST_LOADED) {
		    pcld->u.ch_redirect_lb.lb_attr.action = 
		    	CL_REDIR_LB_REP_LEAST_LOADED;
		} else {
		    pcld->u.ch_redirect_lb.lb_attr.action = 
		    	CL_REDIR_LB_REP_RANDOM;
		}

		load_metric = nd_cl->cnd_redir_repl.lb_data.threshold;
		if (load_metric < 0) {
		    pcld->u.ch_redirect_lb.lb_attr.load_metric_threshold = 0;
		} else if (load_metric > 100) {
		    pcld->u.ch_redirect_lb.lb_attr.load_metric_threshold = 100;
		} else {
		    pcld->u.ch_redirect_lb.lb_attr.load_metric_threshold = 
		    	load_metric;
		}

		/* Setup redir_attr */
	    	pcld->u.ch_redirect_lb.redir_attr.allow_redir_local = 
		    nd_cl->cnd_redir_repl.rd_data.local_redirect;

		if (nd_cl->cnd_redir_repl.rd_data.baseRedirectPath) {
		    pcld->u.ch_redirect_lb.redir_attr.path_prefix = 
			nkn_strdup_type(
			    nd_cl->cnd_redir_repl.rd_data.baseRedirectPath,
			    mod_ns_base_redir_path);
		    pcld->u.ch_redirect_lb.redir_attr.path_prefix_strlen =
		      	strlen(pcld->u.ch_redirect_lb.redir_attr.path_prefix);
		}

		/* Setup NodeMap */
	    	pcld->u.ch_redirect_lb.node_map = 
		    new_nmd_load_cluster_map(
				(const cluster_map_bin_t *)
				nd_cl->serverMap->binary_server_map,
				nd_cl->serverMap->binary_size,
				nd->namespace, nd->uid, "RD",
				&pcld->u.ch_redirect_lb.ch_attr,
				&pcld->u.ch_redirect_lb.lb_attr,
				nsc, old_nsc, &online);

	    	if (pcld->u.ch_redirect_lb.node_map) {
		    if (online) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) online", 
				nd->namespace, n, nd_cl_name); 
		    } else {
		    	AO_fetch_and_add1(&nsc->init_in_progress);
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) initializing", 
				nd->namespace, n, nd_cl_name); 
		    }
		    setup_cmm_monitor_config(pcld->u.ch_redirect_lb.node_map,
					     &nd_cl->serverMap->monitor_values);
	    	} else {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "new_nmd_load_cluster_map() failed, "
			    "namespace[%s]:%d(%s)", 
			    nd->namespace, n, nd_cl_name);
		    goto err_exit;
	    	}
	    	break;

	    default:
	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"Unsupported redirect_cluster method=%d "
			"namespace[%s]:%d(%s)", 
			nd_cl->u.redirect_cluster.method,
			nd->namespace, n, nd_cl_name);
	    	goto err_exit;
	    }
	    break;

	case CL_TYPE_PROXY:
	    switch (nd_cl->u.proxy_cluster.method) {
	    case CONSISTENT_HASH_PROXY:
	    	pcld->type = CLT_CH_PROXY;

		/* Setup intercept_attr */
		hostport = nd_cl->cnd_proxy.pd_data.addr_port;
		hostport_strlen = hostport ? strlen(hostport) : 0;

		if (hostport && hostport_strlen) {
		    rv = hostport_to_data(hostport, hostport_strlen,
		    		&pcld->u.ch_proxy.intercept_attr.ip,
				&pcld->u.ch_proxy.intercept_attr.port);
		    if (rv) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) "
				"hostport_to_data() failed, rv=%d",
				nd->namespace, n, nd_cl_name, rv); 
	    		goto err_exit;
		    }
		}

		/* Setup ch_attr */
		pcld->u.ch_proxy.ch_attr.ch_uri_base = 
		    (nd_cl->hash_url == CL_BASE_URL);

		/* Setup redir_attr */
	    	pcld->u.ch_proxy.redir_attr.allow_redir_local = 
		    nd_cl->cnd_proxy.pd_data.local_redirect;

		if (nd_cl->cnd_proxy.pd_data.baseRedirectPath) {
		    pcld->u.ch_proxy.redir_attr.path_prefix = 
			nkn_strdup_type(
			    	nd_cl->cnd_proxy.pd_data.baseRedirectPath,
				mod_ns_base_redir_path);
		    pcld->u.ch_proxy.redir_attr.path_prefix_strlen =
			strlen(pcld->u.ch_proxy.redir_attr.path_prefix);
		}

		/* Setup NodeMap */
	    	pcld->u.ch_proxy.node_map = 
		    new_nmd_noload_cluster_map(
				(const cluster_map_bin_t *)
				nd_cl->serverMap->binary_server_map,
				nd_cl->serverMap->binary_size,
				nd->namespace, nd->uid,
				&pcld->u.ch_proxy.ch_attr,
				nsc, old_nsc, &online);

	    	if (pcld->u.ch_proxy.node_map) {
		    if (online) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) online", 
				nd->namespace, n, nd_cl_name); 
		    } else {
		    	AO_fetch_and_add1(&nsc->init_in_progress);
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) initializing", 
				nd->namespace, n, nd_cl_name); 
		    }

		    setup_cmm_monitor_config(pcld->u.ch_proxy.node_map,
					     &nd_cl->serverMap->monitor_values);
	    	} else {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "new_nmd_noload_cluster_map() failed, "
			    "namespace[%s]:%d(%s)", 
			    nd->namespace, n, nd_cl_name);
		    goto err_exit;
	    	}
	    	break;

	    case CHASH_PROXY_REPLICATE:
	    	pcld->type = CLT_CH_PROXY_LB;

		/* Setup intercept_attr */
		hostport = nd_cl->cnd_proxy_repl.pd_data.addr_port;
		hostport_strlen = hostport ? strlen(hostport) : 0;

		if (hostport && hostport_strlen) {
		    rv = hostport_to_data(hostport, hostport_strlen,
		    		&pcld->u.ch_proxy_lb.intercept_attr.ip,
				&pcld->u.ch_proxy_lb.intercept_attr.port);
		    if (rv) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) "
				"hostport_to_data() failed, rv=%d",
				nd->namespace, n, nd_cl_name, rv); 
	    		goto err_exit;
		    }
		}

		/* Setup ch_attr */
		pcld->u.ch_proxy_lb.ch_attr.ch_uri_base =
		    (nd_cl->hash_url == CL_BASE_URL);

		/* Setup lb_attr */
		if (nd_cl->cnd_proxy_repl.lb_data.replicate == 
		    REPLICATE_LEAST_LOADED) {
		    pcld->u.ch_proxy_lb.lb_attr.action = 
		    	CL_REDIR_LB_REP_LEAST_LOADED;
		} else {
		    pcld->u.ch_proxy_lb.lb_attr.action = 
		    	CL_REDIR_LB_REP_RANDOM;
		}

		load_metric = nd_cl->cnd_proxy_repl.lb_data.threshold;
		if (load_metric < 0) {
		    pcld->u.ch_proxy_lb.lb_attr.load_metric_threshold = 0;
		} else if (load_metric > 100) {
		    pcld->u.ch_proxy_lb.lb_attr.load_metric_threshold = 100;
		} else {
		    pcld->u.ch_proxy_lb.lb_attr.load_metric_threshold = 
		    	load_metric;
		}

		/* Setup redir_attr */
	    	pcld->u.ch_proxy_lb.redir_attr.allow_redir_local = 
		    nd_cl->cnd_proxy_repl.pd_data.local_redirect;

		if (nd_cl->cnd_proxy_repl.pd_data.baseRedirectPath) {
		    pcld->u.ch_proxy_lb.redir_attr.path_prefix = 
			nkn_strdup_type(
			    nd_cl->cnd_proxy_repl.pd_data.baseRedirectPath,
			    mod_ns_base_redir_path);
		    pcld->u.ch_proxy_lb.redir_attr.path_prefix_strlen =
		      	strlen(pcld->u.ch_proxy_lb.redir_attr.path_prefix);
		}

		/* Setup NodeMap */
	    	pcld->u.ch_proxy_lb.node_map = 
		    new_nmd_load_cluster_map(
				(const cluster_map_bin_t *)
				nd_cl->serverMap->binary_server_map,
				nd_cl->serverMap->binary_size,
				nd->namespace, nd->uid, "RDP",
				&pcld->u.ch_proxy_lb.ch_attr,
				&pcld->u.ch_proxy_lb.lb_attr,
				nsc, old_nsc, &online);

	    	if (pcld->u.ch_proxy_lb.node_map) {
		    if (online) {
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) online", 
				nd->namespace, n, nd_cl_name); 
		    } else {
		    	AO_fetch_and_add1(&nsc->init_in_progress);
		    	DBG_LOG(MSG, MOD_CLUSTER, 
			    	"namespace[%s]:%d(%s) initializing", 
				nd->namespace, n, nd_cl_name); 
		    }
		    setup_cmm_monitor_config(pcld->u.ch_proxy_lb.node_map,
					     &nd_cl->serverMap->monitor_values);
	    	} else {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "new_nmd_load_cluster_map() failed, "
			    "namespace[%s]:%d(%s)", 
			    nd->namespace, n, nd_cl_name);
		    goto err_exit;
	    	}
		break;

	    default:
	    	DBG_LOG(MSG, MOD_NAMESPACE, 
			"Unsupported redirect_cluster method=%d "
			"namespace[%s]:%d(%s)", 
			nd_cl->u.redirect_cluster.method,
			nd->namespace, n, nd_cl_name);
	    	goto err_exit;
	    	break;
	    }
	    break;

	case CL_TYPE_CACHE_CLUSTER:
	default:
	    DBG_LOG(MSG, MOD_NAMESPACE, "Unsupported type=%d "
		    "namespace[%s]:%d(%s)",
		    nd_cl->cl_type, nd->namespace, n, nd_cl_name);
	    goto err_exit;
	}
    }

    return clcfg;

err_exit:

    if (clcfg) {
    	delete_cluster_config_t(clcfg);
    }
    return NULL;
}

void delete_cluster_config_t(cluster_config_t *clcfg)
{
    int n;
    cluster_descriptor_t *cldesc;
    struct node_map_descriptor *nmap;

    if (!clcfg) {
    	return;
    }

    if (clcfg->cld) {
    	for (n = 0; n < clcfg->num_cld; n++) {
	    if ((cldesc = clcfg->cld[n])) {
	    	if (cldesc->node_id_to_ns_index) {
		    free(cldesc->node_id_to_ns_index);
		    cldesc->node_id_to_ns_index = NULL;
		}
		if (cldesc->suffix_map) {
		    nkn_trie_destroy(cldesc->suffix_map);
		    cldesc->suffix_map = NULL;
		}
	    	CLD2NODE_MAP(cldesc, nmap);
	    	if (nmap) {
		    delete_node_map_descriptor_t(nmap);
	    	}
		switch(cldesc->type) {
		case CLT_CH_REDIRECT:
		    if (cldesc->u.ch_redirect.redir_attr.path_prefix) {
		        free((char *)
			     cldesc->u.ch_redirect.redir_attr.path_prefix);
		        cldesc->u.ch_redirect.redir_attr.path_prefix = NULL;
		    }
		    break;
		case CLT_CH_REDIRECT_LB:
		    if (cldesc->u.ch_redirect_lb.redir_attr.path_prefix) {
		        free((char *)
			     cldesc->u.ch_redirect_lb.redir_attr.path_prefix);
		        cldesc->u.ch_redirect_lb.redir_attr.path_prefix = NULL;
		    }
		    break;
		default:
		    break;
		}
	    	free(cldesc);
		clcfg->cld[n] = NULL;
	    }
    	}
    	free(clcfg->cld);
    	clcfg->cld = NULL;
    }
    free(clcfg);
}



cache_pin_config_t *new_cache_pin_config_t(namespace_node_data_t *nd)
{
    cache_pin_config_t *cpcfg;

    cpcfg =
	nkn_calloc_type(1, sizeof(cache_pin_config_t),
			mod_ns_cache_pin_config_t);
    if (!cpcfg) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	goto err_exit;
    }

    if (nd->origin_fetch.object.cache_pin.cache_pin_ena) {
	cpcfg->enable = NKN_TRUE;
    }

    cpcfg->cache_auto_pin = nd->origin_fetch.object.cache_pin.cache_auto_pin;

    cpcfg->cache_resv_bytes =
        nd->origin_fetch.object.cache_pin.cache_resv_bytes;
    cpcfg->max_obj_bytes =
        nd->origin_fetch.object.cache_pin.max_obj_bytes;

    if (nd->origin_fetch.object.cache_pin.pin_header) {
        cpcfg->pin_header =
            nkn_strdup_type(nd->origin_fetch.object.cache_pin.pin_header,
    		            mod_ns_cache_pin_header);
        if (!cpcfg->pin_header) {
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
    	    goto err_exit;
        }
        cpcfg->pin_header_len = strlen(cpcfg->pin_header);
    }

    if (nd->origin_fetch.object.cache_pin.end_header) {
        cpcfg->end_header =
    	    nkn_strdup_type(nd->origin_fetch.object.cache_pin.end_header,
			    mod_ns_cache_pin_end_header);
	if (!cpcfg->end_header) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	    goto err_exit;
	}
	cpcfg->end_header_len = strlen(cpcfg->end_header);
    }

    if (nd->origin_fetch.object.start_header) {
	cpcfg->start_header =
	    nkn_strdup_type(nd->origin_fetch.object.start_header,
			    mod_ns_cache_pin_start_header);
    	if (!cpcfg->start_header) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	    goto err_exit;
    	}
	    cpcfg->start_header_len = strlen(cpcfg->start_header);
    }

    return cpcfg;

err_exit:
    if (cpcfg) {
	delete_cache_pin_config_t(cpcfg);
    }
    return NULL;
}

void delete_cache_pin_config_t(cache_pin_config_t *cpcfg)
{
    if (!cpcfg) {
	return;
    }

    if (cpcfg->pin_header) {
	free(cpcfg->pin_header);
    }
    if (cpcfg->end_header) {
	free(cpcfg->end_header);
    }
    if (cpcfg->start_header) {
	free(cpcfg->start_header);
    }

    free(cpcfg);
}

struct ns_accesslog_config *new_accesslog_config_t(namespace_node_data_t *nd)
{
    struct ns_accesslog_config *alcfg;
    int n;

    alcfg = nkn_calloc_type(1, sizeof(struct ns_accesslog_config),
	    mod_ns_accesslog_config);
    if (!alcfg) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }

    if (nd->access_log.name) {
	alcfg->name = nkn_strdup_type(nd->access_log.name, mod_ns_access_log);
	if (!alcfg->name) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	    goto err_exit;
	}

	n = strlen(alcfg->name) + strlen("uds-acclog-") + 4;

	alcfg->key = nkn_calloc_type(1, n, mod_ns_access_log);


	if (!alcfg->key) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	    goto err_exit;
	}

	snprintf(alcfg->key, n, "uds-acclog-%s", alcfg->name);
	alcfg->keyhash = n_str_hash(alcfg->key);
	alcfg->al_record_cache_history =
		nd->access_log.al_record_cache_history;
	alcfg->al_resp_header_configured =
		nd->access_log.al_resp_header_configured;
	alcfg->al_resp_hdr_md5_chksum_configured =
		nd->access_log.al_resp_hdr_md5_chksum_configured;
    }

    return alcfg;

err_exit:
    if (alcfg) {
	delete_accesslog_config_t(alcfg);
    }
    return NULL;

}

void delete_accesslog_config_t(struct ns_accesslog_config *acclog_cfg)
{
    if (!acclog_cfg)
	return;
    if (acclog_cfg->name) {
	free(acclog_cfg->name);
    }
    if (acclog_cfg->key) {
	free(acclog_cfg->key);
    }
    free(acclog_cfg);
    return;
}

compress_config_t *new_compress_config_t(namespace_node_data_t *nd)
{
    compress_config_t *cpcfg;
    int i = 0;

    cpcfg =
	nkn_calloc_type(1, sizeof(compress_config_t),
			mod_ns_compress_config_t);
    if (!cpcfg) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	goto err_exit;
    }

	cpcfg->enable = nd->origin_fetch.compression_status ;
	cpcfg->max_obj_size = nd->origin_fetch.compress_obj_max_size;
	cpcfg->min_obj_size = nd->origin_fetch.compress_obj_min_size;
	cpcfg->algorithm = nd->origin_fetch.compression_algorithm;
	for(i=0; i<MAX_NS_COMPRESS_FILETYPES; i++) {
		cpcfg->file_types[i].type = nd->origin_fetch.compress_file_types[i].type ? nkn_strdup_type(nd->origin_fetch.compress_file_types[i].type, mod_ns_compress_config_t): NULL; 
		cpcfg->file_types[i].type_len = nd->origin_fetch.compress_file_types[i].type_len; 
		cpcfg->file_types[i].allowed = nd->origin_fetch.compress_file_types[i].allowed;
	}

    return cpcfg;

err_exit:
    if (cpcfg) {
	delete_compress_config_t(cpcfg);
    }
    return NULL;
}

void delete_compress_config_t(compress_config_t *cpcfg)
{
    int i =0;
    if (!cpcfg) {
	return;
    }


	for(i=0; i<MAX_NS_COMPRESS_FILETYPES; i++) {
		if(cpcfg->file_types[i].type) {
			free(cpcfg->file_types[i].type);
		}
	}
    free(cpcfg);
}

unsuccess_response_cache_cfg_t *new_non2xx_cache_config_t(namespace_node_data_t *nd)
{
    unsuccess_response_cache_cfg_t *cpcfg;
    int i = 0;

    cpcfg =
	nkn_calloc_type(MAX_UNSUCCESSFUL_RESPONSE_CODES, 
			sizeof(unsuccess_response_cache_cfg_t),
			mod_ns_unsuccess_cache_cfg);
    if (!cpcfg) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_strdup_type() failed");
	goto err_exit;
    }

	for(i=0; i<MAX_UNSUCCESSFUL_RESPONSE_CODES; i++) {
		cpcfg[i].status = nd->origin_fetch.unsuccess_response_cache[i].status; 
		cpcfg[i].code = nd->origin_fetch.unsuccess_response_cache[i].code; 
		cpcfg[i].age = nd->origin_fetch.unsuccess_response_cache[i].age; 
	}

    return cpcfg;

err_exit:
    if (cpcfg) {
	delete_non2xx_cache_config_t(cpcfg);
    }
    return NULL;
}

void delete_non2xx_cache_config_t(unsuccess_response_cache_cfg_t *cpcfg)
{
    if (!cpcfg) {
	return;
    }

    free(cpcfg);
}

static void *uf_trie_copy(nkn_trie_node_t nd)
{
    return nd;
}

static void uf_trie_destruct(nkn_trie_node_t nd)
{
}

void *new_url_filter_trie(const char *fname, const char *namespace, int *err)
{
    int ret = 0;
    size_t size = 0;
    const char *addr = 0;
    int fd = -1;
    int rv;
    struct stat sb;
    nkn_trie_t *ptrie = 0;
    long nkn_trie_node_data = 1 + 1; // Allow for header
    bin_uf_fmt_url_hdr_t *uf_hdr;
    bin_uf_fmt_url_rec_t *uf_rec;

    url_filter_trie_t *uf_trie_data = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
	DBG_LOG(MSG, MOD_NAMESPACE, "open(%s) failed, errno=%d", fname, errno);
	ret = 1;
	break;
    }
    rv = fstat(fd, &sb);
    if (rv != 0) {
	DBG_LOG(MSG, MOD_NAMESPACE, "fstat(fd=%d) failed, errno=%d", 
		fd, errno);
	ret = 2;
	break;
    }
    size = sb.st_size;

    addr = (const char *)mmap64(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
	DBG_LOG(MSG, MOD_NAMESPACE, "mmap64(fd=%d) failed, errno=%d",
		fd, errno);
	ret = 3;
	break;
    }

    ptrie = nkn_trie_create(uf_trie_copy, uf_trie_destruct);
    if (!ptrie) {
    	DBG_LOG(MSG, MOD_NAMESPACE, 
		"Namespace=%s URL Filter nkn_trie_create() failed",
		namespace);
	ret = 4;
	break;
    }

    uf_hdr = (bin_uf_fmt_url_hdr_t *) addr;

    if (uf_hdr->magicno != BIN_UF_FMT_URL_MAGIC) {
    	DBG_LOG(MSG, MOD_NAMESPACE,
		"Namespace=%s URL Filter bin file bad magicno, "
		"expected=%ld given=%ld",
		namespace, BIN_UF_FMT_URL_MAGIC, uf_hdr->magicno);
	ret = 5;
	break;
    }

    if (uf_hdr->version_maj != BIN_UF_FMT_URL_VERS_MAJ) {
    	DBG_LOG(MSG, MOD_NAMESPACE,
		"Namespace=%s URL Filter bin file bad version major, "
		"expected=%d given=%d",
		namespace, BIN_UF_FMT_URL_VERS_MAJ, uf_hdr->version_maj);
	ret = 6;
	break;
    }

    if (uf_hdr->version_min != BIN_UF_FMT_URL_VERS_MIN) {
    	DBG_LOG(MSG, MOD_NAMESPACE, 
		"Namespace=%s URL Filter bin file bad version minor, "
		"expected=%d given=%d",
		namespace, BIN_UF_FMT_URL_VERS_MIN, uf_hdr->version_min);
	ret = 7;
	break;
    }

    uf_rec = UF_URL_FIRST_RECORD(uf_hdr);

    while (1) {
    	if (!uf_rec->sizeof_data) {
	    break;
	}
	// Note: nkn_trie_node_data == Entry line number in uf.txt
    	rv = nkn_trie_add(ptrie, uf_rec->data, 
			  (nkn_trie_node_t)(nkn_trie_node_data++));
	if (rv) {
	    DBG_LOG(MSG, MOD_NAMESPACE,
		    "Namespace=%s URL Filter trie add failed, "
		    "rv=%d key=%s", namespace, rv, uf_rec->data);
	    ret = 8;
	    break;
	}
	uf_rec = UF_URL_NXT_RECORD(uf_rec);
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    }
    ////////////////////////////////////////////////////////////////////////////

    if (fd >= 0) {
    	if (addr && (addr != MAP_FAILED) && size) {
	    munmap((void *)addr, size);
	}
	close(fd);
    }

    if (!ret) {
    	uf_trie_data = nkn_calloc_type(1, sizeof(url_filter_trie_t), 
				       mod_uf_trie);
	if (uf_trie_data) {
	    uf_trie_data->magicno = UF_TRIE_DATA_MAGIC;
	    uf_trie_data->trie = ptrie;
	    AO_fetch_and_add1(&uf_trie_data->refcnt);
	} else {
	    ret = 9;
	    nkn_trie_destroy(ptrie);
	    ptrie = 0;
	}
    } else {
    	nkn_trie_destroy(ptrie);
    	ptrie = 0;
    }

    *err = ret;
    return (void *)uf_trie_data;
}

void *dup_url_filter_trie(void *arg)
{
    url_filter_trie_t *uf_trie = (url_filter_trie_t *) arg;
    if (uf_trie && (uf_trie->magicno == UF_TRIE_DATA_MAGIC)) {
	AO_fetch_and_add1(&uf_trie->refcnt);
	return arg;
    } else {
    	return 0;
    }
}

void delete_url_filter_trie(void *arg)
{
    int64_t old_refcnt;
    url_filter_trie_t *uf_trie = (url_filter_trie_t *) arg;

    if (uf_trie && (uf_trie->magicno == UF_TRIE_DATA_MAGIC)) {
    	old_refcnt = AO_fetch_and_sub1(&uf_trie->refcnt);
	if (old_refcnt == 1) {
	    nkn_trie_destroy(uf_trie->trie);
	    uf_trie->trie = 0;
	    uf_trie->magicno = ~UF_TRIE_DATA_MAGIC;

	    free(uf_trie);
	}
    }
}

static
int setup_uf_config(url_filter_config_t *cfg, namespace_node_data_t *nd)
{
    int ret = 0;
    int rv;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    //Setup reject structure for uri size filter
    cfg->uf_max_uri_size = nd->url_filter.uf_uri_size;
    if (cfg->uf_max_uri_size != 0) {
	switch (nd->url_filter.uf_uri_size_reject_action) {
	case UF_REJECT_RESET:
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_RESET;
	    break;
    
	case UF_REJECT_CLOSE:
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_CLOSE;
	    break;
    
	case UF_REJECT_HTTP_403:
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_HTTP_403;
	    break;
    
	case UF_REJECT_HTTP_404:
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_HTTP_404;
	    break;
    
	case UF_REJECT_HTTP_301:
	   if (nd->url_filter.uri_size_rej.http_301.redir_host) {
		cfg->uf_uri_size_reject.http_301.redir_host = 
		   nkn_strdup_type(nd->url_filter.uri_size_rej.http_301.redir_host, 
			    mod_uf_host_str);
		cfg->uf_uri_size_reject.http_301.redir_host_strlen = 
		   strlen(cfg->uf_uri_size_reject.http_301.redir_host);
	    }
    
	   if (nd->url_filter.uri_size_rej.http_301.redir_url) {
		cfg->uf_uri_size_reject.http_301.redir_url = 
		   nkn_strdup_type(nd->url_filter.uri_size_rej.http_301.redir_url, 
			    mod_uf_url_str);
		cfg->uf_uri_size_reject.http_301.redir_url_strlen = 
		   strlen(cfg->uf_uri_size_reject.http_301.redir_url);
	    }
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_HTTP_301;
	    
	    break;
    
	case UF_REJECT_HTTP_302:
	   if (nd->url_filter.uri_size_rej.http_302.redir_host) {
		cfg->uf_uri_size_reject.http_302.redir_host = 
		   nkn_strdup_type(nd->url_filter.uri_size_rej.http_302.redir_host, 
			    mod_uf_host_str2);
		cfg->uf_uri_size_reject.http_302.redir_host_strlen = 
		   strlen(cfg->uf_uri_size_reject.http_302.redir_host);
	    }
    
	   if (nd->url_filter.uri_size_rej.http_302.redir_url) {
		cfg->uf_uri_size_reject.http_302.redir_url = 
		   nkn_strdup_type(nd->url_filter.uri_size_rej.http_302.redir_url, 
			    mod_uf_url_str2);
		cfg->uf_uri_size_reject.http_302.redir_url_strlen = 
		   strlen(cfg->uf_uri_size_reject.http_302.redir_url);
	    }
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_HTTP_302;
	    break;
    
	case UF_REJECT_HTTP_200:
	{
	   int body_size;
	    char *pbuf;
    
	   if (nd->url_filter.uri_size_rej.http_200.response_body) {
		body_size = strlen(pre_html_body) + 
		   strlen(nd->url_filter.uri_size_rej.http_200.response_body) +
		    strlen(post_html_body) + 1;
    
		cfg->uf_uri_size_reject.http_200.response_body = 
		   nkn_malloc_type(body_size, mod_uf_body);
    
		if (cfg->uf_uri_size_reject.http_200.response_body) {
		    pbuf = (char *)cfg->uf_uri_size_reject.http_200.response_body;
		    strcpy(pbuf, pre_html_body);
		    strcat(pbuf, nd->url_filter.uri_size_rej.http_200.response_body);
		    strcat(pbuf, post_html_body);
    
		    cfg->uf_uri_size_reject.http_200.response_body_strlen = body_size - 1;
		} else {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "Namespace=%s URL Filter response_body malloc failed",
			    nd->namespace);
		   ret = 3;
		    break;
		}
	    }
	   cfg->uf_uri_size_reject_action = NS_UF_URI_SIZE_REJECT_HTTP_200;
	    break;
	}
	case UF_REJECT_HTTP_CUSTOM:
	    // Not yet supported.
	    // Fall through to default
	default:
	    DBG_LOG(MSG, MOD_NAMESPACE, "Invalid URL reject_action=%d", 
		    nd->url_filter.uf_uri_size_reject_action);
	   ret = 4;
	    break;
	}
    }

    if (nd->url_filter.uf_local_file == NULL) {
	DBG_LOG(MSG, MOD_NAMESPACE, "filename is NULL,"
		" disabling url-filtering ")
	ret = 1;
	break;
    }

    if (nd->url_filter.uf_trie_data) {
    	cfg->uf_trie = dup_url_filter_trie(nd->url_filter.uf_trie_data);
    } else {
	glob_url_filter_read++;
    	cfg->uf_trie = (url_filter_trie_t *)
	    new_url_filter_trie(nd->url_filter.uf_local_file,
			        nd->namespace, &ret);
	if (!cfg->uf_trie) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "new_url_filter_trie() failed, ret=%d", 
		    ret);
	    ret = 100 + ret;
	    break;
	}
    }

    //Setup reject structure for white-list/black-list filter
    switch (nd->url_filter.uf_type) {
    case UF_URL:
    	cfg->uf_map_type = NS_UF_MAP_TYPE_URL;
	break;
    case UF_IWF:
    	cfg->uf_map_type = NS_UF_MAP_TYPE_IWF;
	break;
    case UF_CALEA:
    	cfg->uf_map_type = NS_UF_MAP_TYPE_CALEA;
	break;
    default:
	DBG_LOG(MSG, MOD_NAMESPACE,
		"Namespace=%s Invalid URL Filter uf_type=%d",
		nd->namespace, nd->url_filter.uf_type);
    	ret = 2;
	break;
    }

    switch (nd->url_filter.uf_reject_action) {
    case UF_REJECT_RESET:
    	cfg->uf_reject_action = NS_UF_REJECT_RESET;
	break;

    case UF_REJECT_CLOSE:
    	cfg->uf_reject_action = NS_UF_REJECT_CLOSE;
	break;

    case UF_REJECT_HTTP_403:
    	cfg->uf_reject_action = NS_UF_REJECT_HTTP_403;
	break;

    case UF_REJECT_HTTP_404:
    	cfg->uf_reject_action = NS_UF_REJECT_HTTP_404;
	break;

    case UF_REJECT_HTTP_301:
    	if (nd->url_filter.u_rej.http_301.redir_host) {
	    cfg->uf_reject.http_301.redir_host = 
	    	nkn_strdup_type(nd->url_filter.u_rej.http_301.redir_host, 
			   mod_uf_host_str);
	    cfg->uf_reject.http_301.redir_host_strlen = 
	    	strlen(cfg->uf_reject.http_301.redir_host);
	}

    	if (nd->url_filter.u_rej.http_301.redir_url) {
	    cfg->uf_reject.http_301.redir_url = 
	    	nkn_strdup_type(nd->url_filter.u_rej.http_301.redir_url, 
			   mod_uf_url_str);
	    cfg->uf_reject.http_301.redir_url_strlen = 
	    	strlen(cfg->uf_reject.http_301.redir_url);
	}
    	cfg->uf_reject_action = NS_UF_REJECT_HTTP_301;
	
	break;

    case UF_REJECT_HTTP_302:
    	if (nd->url_filter.u_rej.http_302.redir_host) {
	    cfg->uf_reject.http_302.redir_host = 
	    	nkn_strdup_type(nd->url_filter.u_rej.http_302.redir_host, 
			   mod_uf_host_str2);
	    cfg->uf_reject.http_302.redir_host_strlen = 
	    	strlen(cfg->uf_reject.http_302.redir_host);
	}

    	if (nd->url_filter.u_rej.http_302.redir_url) {
	    cfg->uf_reject.http_302.redir_url = 
	    	nkn_strdup_type(nd->url_filter.u_rej.http_302.redir_url, 
			   mod_uf_url_str2);
	    cfg->uf_reject.http_302.redir_url_strlen = 
	    	strlen(cfg->uf_reject.http_302.redir_url);
	}
    	cfg->uf_reject_action = NS_UF_REJECT_HTTP_302;
	break;

    case UF_REJECT_HTTP_200:
    {
    	int body_size;
	char *pbuf;

    	if (nd->url_filter.u_rej.http_200.response_body) {
	    body_size = strlen(pre_html_body) + 
	    	strlen(nd->url_filter.u_rej.http_200.response_body) +
		strlen(post_html_body) + 1;

	    cfg->uf_reject.http_200.response_body = 
	    	nkn_malloc_type(body_size, mod_uf_body);

	    if (cfg->uf_reject.http_200.response_body) {
		pbuf = (char *)cfg->uf_reject.http_200.response_body;
		strcpy(pbuf, pre_html_body);
		strcat(pbuf, nd->url_filter.u_rej.http_200.response_body);
		strcat(pbuf, post_html_body);

		cfg->uf_reject.http_200.response_body_strlen = body_size - 1;
	    } else {
		DBG_LOG(MSG, MOD_NAMESPACE, 
			"Namespace=%s URL Filter response_body malloc failed",
			nd->namespace);
	    	ret = 3;
		break;
	    }
	}
    	cfg->uf_reject_action = NS_UF_REJECT_HTTP_200;
	break;
    }
    case UF_REJECT_HTTP_CUSTOM:
	// Not yet supported.
	// Fall through to default
    default:
	DBG_LOG(MSG, MOD_NAMESPACE, "Invalid URL reject_action=%d", 
		nd->url_filter.uf_reject_action);
    	ret = 4;
	break;
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End of while
    ////////////////////////////////////////////////////////////////////////////
    
    if (!ret) {
    	cfg->uf_is_black_list = 
	    (nd->url_filter.uf_list_type == UF_LT_BLACK_LIST) ? 1 : 0;
    } else {
    	delete_url_filter_trie(cfg->uf_trie);
    	cfg->uf_trie = 0;
    }

    return ret;
}

url_filter_config_t *new_url_filter_config_t(namespace_node_data_t *nd)
{
    int rv;
    url_filter_config_t *cfg = 0;

    cfg = nkn_calloc_type(1, sizeof(url_filter_config_t), mod_uf_config_t);
    if (!cfg) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto error_exit;
    }

    if (strcmp(nd->namespace, "mfc_probe")) {
	rv = setup_uf_config(cfg, nd);
	if (rv) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "setup_uf_config() failed, rv=%d", 
		    rv);
	}
    }

    return cfg;

error_exit:

    if (cfg) {
	delete_url_filter_config_t(cfg);
    }
    return NULL;
}

void delete_url_filter_config_t(url_filter_config_t *ufc)
{
    if (!ufc) {
    	return;
    }

    if (ufc->uf_trie) {
    	delete_url_filter_trie(ufc->uf_trie);
    	ufc->uf_trie = 0;
    }

    switch (ufc->uf_reject_action) {
    case NS_UF_REJECT_HTTP_301:
    	if (ufc->uf_reject.http_301.redir_host) {
	    free((void *)ufc->uf_reject.http_301.redir_host);
	    ufc->uf_reject.http_301.redir_host = NULL;
	}
	if (ufc->uf_reject.http_301.redir_url) {
	    free((void *)ufc->uf_reject.http_301.redir_url);
	    ufc->uf_reject.http_301.redir_url = NULL;
	}
    	break;

    case NS_UF_REJECT_HTTP_302:
    	if (ufc->uf_reject.http_302.redir_host) {
	    free((void *)ufc->uf_reject.http_302.redir_host);
	    ufc->uf_reject.http_302.redir_host = NULL;
	}
	if (ufc->uf_reject.http_302.redir_url) {
	    free((void *)ufc->uf_reject.http_302.redir_url);
	    ufc->uf_reject.http_302.redir_url = NULL;
	}
    	break;

    case NS_UF_REJECT_HTTP_200:
    	if (ufc->uf_reject.http_200.response_body) {
	    free((void *)ufc->uf_reject.http_200.response_body);
	    ufc->uf_reject.http_200.response_body = NULL;
	}
    	break;

    default:
        break;
    }
    //Remove reject structure for uri size filter
    switch (ufc->uf_uri_size_reject_action) {
    case NS_UF_URI_SIZE_REJECT_HTTP_301:
    	if (ufc->uf_uri_size_reject.http_301.redir_host) {
	    free((void *)ufc->uf_uri_size_reject.http_301.redir_host);
	    ufc->uf_uri_size_reject.http_301.redir_host = NULL;
	}
	if (ufc->uf_uri_size_reject.http_301.redir_url) {
	    free((void *)ufc->uf_uri_size_reject.http_301.redir_url);
	    ufc->uf_uri_size_reject.http_301.redir_url = NULL;
	}
    	break;

    case NS_UF_URI_SIZE_REJECT_HTTP_302:
    	if (ufc->uf_uri_size_reject.http_302.redir_host) {
	    free((void *)ufc->uf_uri_size_reject.http_302.redir_host);
	    ufc->uf_uri_size_reject.http_302.redir_host = NULL;
	}
	if (ufc->uf_uri_size_reject.http_302.redir_url) {
	    free((void *)ufc->uf_uri_size_reject.http_302.redir_url);
	    ufc->uf_uri_size_reject.http_302.redir_url = NULL;
	}
    	break;

    case NS_UF_URI_SIZE_REJECT_HTTP_200:
    	if (ufc->uf_uri_size_reject.http_200.response_body) {
	    free((void *)ufc->uf_uri_size_reject.http_200.response_body);
	    ufc->uf_uri_size_reject.http_200.response_body = NULL;
	}
    	break;

    default:
        break;
    }
    free((void *)ufc);
}

/*
 * End of nkn_namespace_utils.c
 */

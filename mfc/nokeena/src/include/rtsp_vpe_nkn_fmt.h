#ifndef _RTSP_VPE_NKN_FMT__HH
#define _RTSP_VPE_NKN_FMT__HH

#define ANKEENA_FMT_EXT	".nknc"

/*
 * Define generic video cache format.
 *
 * 1) Ankeena Format header structure.
 * 2) Seek Box
 */

#define MAX_TRACK	6

typedef struct sdp_track {
        char    trackID[16]; //track id as specified in the control field in the SDP
        char    m_string[32]; //m=audio 0 RTP/AP 32
        char    b_string[16]; //b=AS:500
        uint32_t sample_rate; //sampling frequency of the given track id
        uint8_t track_type; //Set to Audio or Video track
        uint8_t channel_id; //Set to the channel id
        uint8_t flag_abs_url;
} sdp_track_t;

typedef struct rl_sdp_info {
	int32_t		num_trackID;
        sdp_track_t     track[MAX_TRACK];
        char            npt_start[16]; // It could be a string. e.g. range:npt=now-
        char            npt_stop[16];
} rl_sdp_info_t;



/*
 * 1) Ankeena Format header structure.
 */
#define MAGIC_STRING		"ankeena_video"
#define RVNFH_VERSION		0x03

typedef struct rtsp_vpe_nkn_fmt_header {
	char magic[32];
	char version;
	char reserved[7];

	/* SDP parsing result */
	rl_sdp_info_t sdp_info;

	/* Original SDP string */
	int32_t sdp_size;
	char sdp_string[0];
} rvnf_header_t;
#define rvnf_header_s sizeof(rvnf_header_t)






/*
 * 2) Seek Box, 
 *    distributed inside cache file randomly for seek purpose.
 *    We need at least one which immediately follows rvnf_header_t structure.
 */
typedef struct rtsp_vpe_nkn_fmt_seek_box {
        uint32_t end_time;	// in Seconds related to beginning of the video
        uint32_t this_box_data_size; // This seek box data size, excluding seek box header size.
	// Followed by RTP packet.
} rvnf_seek_box_t;
#define rvnf_seek_box_s sizeof(rvnf_seek_box_t)


#endif // _RTSP_VPE_NKN_FMT__HH

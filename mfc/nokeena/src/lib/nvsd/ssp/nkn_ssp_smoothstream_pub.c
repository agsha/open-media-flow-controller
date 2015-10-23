/*
 * Silverlight Smooth Streaming Server Side Player
 * May 26, 2010
 * 
 */

#include <stdlib.h>
#include <alloca.h>
#include <string.h>
#include <strings.h>

#include "ssp.h"
#include "nkn_ssp_cb.h"
#include "nkn_ssp_players.h"
#include "ssp_authentication.h"
#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_stat.h"

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_ism_read_api.h"
#include "nkn_vpe_ismc_read_api.h"
#include "nkn_vpe_types.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )
/* extern char *strcasestr(const char *haystack, const char *needle); */
const char *smoothstream_def_av_qparam_name[] = { "video", "audio" };
/////////////////////////////////////
// Silverlight Smooth Streaming    //
// Server Side Player (SSP Type 6) //
/////////////////////////////////////

// Prototypes
static int smoothstream_uri_get_ql(const char *uri_data, const char *quality_str,
				   uint32_t *bitrate);
static int parse_smoothstream_uri(const char *uri_data, const char *quality_str, const char *fragment_str, \
			   const char *video_tracktype, const char *audio_tracktype, \
			   uint8_t *track_type, uint32_t *bit_rate, uint64_t *time_offset);

static int find_base_uri_endpos(const char *uri_data, const char *quality_str);

static uint32_t
get_mp4_boxsize(uint8_t *src, size_t size);


// Smooth Streaming On Demand Publisher
int
smoothstreamingSSP(con_t *con, const char *uriData, int uriLen, \
		   const ssp_config_t *ssp_cfg, \
		   off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *uri;
    char *p_qpos, *p_fpos, *p_cmpos;
    char *manifest_uri, *media_uri;
    const char *video_qparam_name, *audio_qparam_name;
    uint8_t track_type, mfro[16];
    uint16_t pos=0, track_id;
    uint32_t manifest_uri_size=0;
    uint32_t bit_rate;
    uint64_t time_offset=0;
    uint32_t box_size;

    size_t mfra_off=0, moof_size=0, moof_offset=0, moof_time=0;
    xml_read_ctx_t *ism;
    ism_bitrate_map_t *map;
    ismv_parser_ctx_t *ctx;

    int remap_len=0, rv, err;

    const char *tmpData;
    int tmpLen=0, tmpCnt=0;
    unsigned int tmpAttrs=0;

    http_cb_t *phttp = &con->http;

    // Obtain the relative URL length stripped of all query params
    if ( (remap_len = findStr(uriData, "?", uriLen)) == -1 ) {
	remap_len = uriLen;
    }

    if ( ssp_cfg->smoothstream_pub.quality_tag == NULL || \
	 ssp_cfg->smoothstream_pub.fragment_tag == NULL ) {
	DBG_LOG(ERROR, MOD_SSP, "Required Quality/Frag tags unspecified in CLI, exiting...");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }

    // If request is for client manifest (.ismc is requested as .ism/Manifest)
    // Rebase URI and return
    p_cmpos = strcasestr(uriData, ".ism/Manifest");
    if ( p_cmpos != NULL ) {
	manifest_uri_size = p_cmpos - uriData + strlen(".ismc") + SSP_MAX_EXTENSION_SZ;
	manifest_uri = (char *)alloca(manifest_uri_size);
	memset(manifest_uri, 0, manifest_uri_size);
	manifest_uri[0]='\0';
	strncat(manifest_uri, uriData, (p_cmpos-uriData) );
	strncat(manifest_uri, ".ismc", strlen(".ismc"));

	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, manifest_uri, strlen(manifest_uri));
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, manifest_uri, strlen(manifest_uri));

	phttp->p_ssp_cb->ssp_streamtype = NKN_SSP_SMOOTHSTREAM_STREAMTYPE_MANIFEST; // text/xml
	DBG_LOG(MSG, MOD_SSP, "SmoothStream Pub: Client manifest file request %s", manifest_uri);

	rv = SSP_SEND_DATA_OTW;
	goto exit;
    }

    /*************************************************************
     * State Flow for Smooth Stream Publishing
     * State 0:
     *		Remap URI to fetch .ISM Server Manifest, SEND_DATA_BACK_TO_SSP
     * State 10:
     *		Parse the XML .ism buffer, extract track id, quality, source name, etc
     *		Remap URI to source name, SEND_DATA_BACK_TO_SSP
     * State 20:
     * 		Read the content length, Trigger a byte range fetch from EOF-16
     *		This is to get the MFRO box (Assuming that it is the last box)
     * 		SEND_DATA_BACK_TO_SSP
     * State 30:
     * 		Parse the MFRO box, find the MFRA offset.
     *		Trigger a byte range fetch to MFRA
     *		SEND_DATA_BACK_TO_SSP
     * State 40:
     *		Allocate buffer for MFRA box, loop and copy the data in chunked fashion
     *		Return code will be SSP_WAIT_FOR_DATA till all chunked data is read.
     *		Fall through to State 50
     *		Parse the MFRA box using the time offset provided
     *		Determine the moof offset and size.
     * State 50:
     *		To determine the size of the fragment first do a internal 4 byte fetch
     *		to find the box size for the moof header, then go to next state
     * State 60:
     *		Again perform a 4-byte fetch to find the size of the mdat box. The sum
     * 		of both the sizes, gives the moof length. Now set the actual byte range
     * 		fetch and send over the wire SEND_DATA_OTW
     */

    switch(con->ssp.seek_state) {

	case 0:

            // Reset all previous stateful variables to 0
	    // (Helpful when GET requests are pipelined in the same keep alive conn)
            con->ssp.ss_seek_offset = 0;
	    con->ssp.ss_track_type = 0;
	    con->ssp.ss_bit_rate = 0;
	    con->ssp.ssp_br_offset = 0;
	    con->ssp.ssp_br_length = 0;

            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);
            con->ssp.header_size = 0;
            if ( con->ssp.header ) {
                free(con->ssp.header);
                con->ssp.header = NULL;
            }
            con->ssp.ssp_partial_content_len = 0;
	    con->ssp.ssp_streamtype = 0;

	    // Remap .ism file uri and request to send to ssp
	    p_qpos = strstr(uriData, ssp_cfg->smoothstream_pub.quality_tag);
	    p_fpos = strstr(uriData, ssp_cfg->smoothstream_pub.fragment_tag);

	    if (p_qpos && p_fpos) { // Process only if uri is a valid ss uri
		// Extract/Create the server manifest uri (*.ism)
		manifest_uri_size = p_qpos - uriData;
		manifest_uri = (char *)alloca(manifest_uri_size + SSP_MAX_EXTENSION_SZ);
		memset(manifest_uri, 0, manifest_uri_size + SSP_MAX_EXTENSION_SZ);
		manifest_uri[0] = '\0';
		strncat(manifest_uri, uriData, manifest_uri_size-1);

#if 0
		parse_smoothstream_uri(uriData, ssp_cfg->smoothstream_pub.quality_tag, \
		    	       ssp_cfg->smoothstream_pub.fragment_tag,	\
		    	       "video", "audio",			\
		    	       &track_type, &bit_rate, &time_offset);
		con->ssp.ss_track_type = track_type;
		con->ssp.ss_bit_rate = bit_rate;
		con->ssp.ss_seek_offset = time_offset;

		snprintf(con->ssp.ss_orig_uri, uriLen, "%s", uriData);

		if(time_offset > 0) //BZ 6541
		    SET_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);

		DBG_LOG(MSG, MOD_SSP, "SmoothStream Pub (State: 0):"
			"Parsed Quality: %d, Time: %ld", con->ssp.ss_bit_rate, con->ssp.ss_seek_offset);
#endif
		DBG_LOG(MSG, MOD_SSP, "SmoothStream Pub (State: 0): Fetch .ism file %s", manifest_uri);

		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, manifest_uri, strlen(manifest_uri));
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, manifest_uri, strlen(manifest_uri));

		con->ssp.seek_state = 10;
		rv = SSP_SEND_DATA_BACK;
		goto exit;
	    } else { // just allow to pass through
		DBG_LOG(MSG, MOD_SSP, "SmoothStream Pub (State: 0): Original URI not SS compliant: %s", uriData);

		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }

	    break;

	case 10:
	    // Obtain .ism file, parse the XML context and extract track/src data
	    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) { // File not found
		DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 10): Server Manifest not found. Cannot proceed");

		rv = -NKN_SSP_SS_SVR_MANIFEST_ISM_NOT_FOUND;
		goto exit;
	    }

	    // Manifest found, initiate xml parser (chunked fetch for .ISM files not supported)
	    if (dataBuf != NULL && contentLen <= bufLen) {
		//const char *orig_uri = uriData;//con->ssp.ss_orig_uri; [BZ 10387]

		/* delete the known MIME headers set in state 0 */
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);

		// Obtain the original restful URL [BZ 10387]
		if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, &tmpData, &tmpLen, &tmpAttrs, &tmpCnt)) {
		    if (get_known_header(&phttp->hdr , MIME_HDR_X_NKN_URI, &tmpData, &tmpLen, &tmpAttrs, &tmpCnt)) {
			DBG_LOG(WARNING, MOD_SSP,
				"SmoothStream Pub (State: 10): Failed to extract url from NKN_URI");
			return -NKN_SSP_BAD_URL;
		    }
		}

		/* read the server manifest ism file into an XML context */
		ism = init_xml_read_ctx( (uint8_t *)dataBuf, bufLen);

		/* read the [trackid, bitrate] map from the ism file context */
		map = ism_read_bitrate_map(ism);

		/* read the bitrate from the uri, this is necessary to
		 * query the "trackName" attribute from the ism bitrate
		 * map
		*/
		rv = smoothstream_uri_get_ql(tmpData,
					ssp_cfg->smoothstream_pub.quality_tag,
					&bit_rate);
		if (rv!=0){
		    DBG_LOG(WARNING, MOD_SSP,
                            "SmoothStream Pub (State: 10): Tags positions not found. Exiting");
                    rv = SSP_SEND_DATA_OTW;
                    goto exit;
		}

		/* retrieve the query param name for the A/V track
		 * for the given bitrate. if the "trackName" attribute
		 * is not present the we fallback to defaults
		 */
		err = ism_get_video_qparam_name(map, bit_rate,
						&video_qparam_name);
		if (err == ISM_USE_DEF_QPARAM) {
		    video_qparam_name =			\
			smoothstream_def_av_qparam_name[0];
		}
		err = ism_get_audio_qparam_name(map, bit_rate,
						&audio_qparam_name);
		if (err == ISM_USE_DEF_QPARAM) {
		    audio_qparam_name =			\
			smoothstream_def_av_qparam_name[1];
		}

		/* now parse with the audio and video query param tag
		 * parsed from the ism bitrate map
		 */
		rv = parse_smoothstream_uri(tmpData, ssp_cfg->smoothstream_pub.quality_tag, \
				       ssp_cfg->smoothstream_pub.fragment_tag, \
				       video_qparam_name, audio_qparam_name, \
				       &track_type, &bit_rate,
				       &time_offset);
		if (rv!=0){
                    DBG_LOG(WARNING, MOD_SSP,
                            "SmoothStream Pub (State: 10): Tags positions not found. Exiting");
                    rv = SSP_SEND_DATA_OTW;
                    goto exit;
                }

		con->ssp.ss_track_type = track_type;
		con->ssp.ss_bit_rate = bit_rate;
		con->ssp.ss_seek_offset = time_offset;
		if(time_offset > 0) //BZ 6541
		    SET_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);

		/* find the trackid for a given track type and bitrate */
		track_id = ism_get_track_id(map, con->ssp.ss_bit_rate, con->ssp.ss_track_type);
		if (track_id == 0) {
		    DBG_LOG(WARNING, MOD_SSP,
			    "SmoothStream Pub (State: 10):"
			    "Unable to find a track corresponding to bit rate %d kbps. Close conn ",
			    con->ssp.ss_bit_rate);
		    rv = -NKN_SSP_SS_TRACK_NOT_FOUND;
		    goto exit;
		}

		/* Use track id to find the source file name */
		uri = ism_get_video_name(map, con->ssp.ss_track_type,
					 track_id,
					 con->ssp.ss_bit_rate, NULL,
					 NULL, 0);

		if (con->ssp.ss_track_type == 0) {
		    phttp->p_ssp_cb->ssp_streamtype = NKN_SSP_SMOOTHSTREAM_STREAMTYPE_VIDEO; //video/mp4
		} else if (con->ssp.ss_track_type == 1) {
		    phttp->p_ssp_cb->ssp_streamtype = NKN_SSP_SMOOTHSTREAM_STREAMTYPE_AUDIO; //audio/mp4
		}

		con->ssp.ss_track_id = track_id;
		//con->ssp.ss_seek_offset = time_offset;

		// Create the source file URI for the .ISMV or .ISMA
		// file
		p_qpos = strstr(tmpData, ssp_cfg->smoothstream_pub.quality_tag);
		p_fpos = strstr(tmpData, ssp_cfg->smoothstream_pub.fragment_tag);

		if (p_qpos == NULL || p_fpos == NULL) { // [BZ 10387]
		    DBG_LOG(WARNING, MOD_SSP,
			    "SmoothStream Pub (State: 10): Tags positions not found. Exiting");
		    rv = SSP_SEND_DATA_OTW;
		    goto exit;
		}

		// Extract/Create the server manifest uri (*.ism)
		manifest_uri_size = p_qpos - tmpData;
		if (manifest_uri_size > strlen(tmpData)) { // [BZ 10387]
		    DBG_LOG(WARNING, MOD_SSP,
                            "SmoothStream Pub (State: 10): uri size gone out of bounds. Exiting");
                    rv = SSP_SEND_DATA_OTW;
                    goto exit;
		}

		manifest_uri = (char *)alloca(manifest_uri_size + SSP_MAX_EXTENSION_SZ);
		memset(manifest_uri, 0,	manifest_uri_size + SSP_MAX_EXTENSION_SZ);
		manifest_uri[0] = '\0';
		strncat(manifest_uri, tmpData, manifest_uri_size-1);

		pos = find_base_uri_endpos(manifest_uri, ssp_cfg->smoothstream_pub.quality_tag);
		media_uri = (char *)alloca(pos + strlen(uri) + SSP_MAX_EXTENSION_SZ);
		memset(media_uri, 0, pos + strlen(uri) + SSP_MAX_EXTENSION_SZ);
		media_uri[0] = '\0';
		strncat(media_uri, manifest_uri, pos);
		strncat(media_uri, uri, strlen(uri));

		// Uri for the media asset (Could be an ISMV or ISMA or a packaged ISMV file)
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, media_uri, strlen(media_uri));
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, media_uri, strlen(media_uri));
		SET_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS); // BZ 6541
		DBG_LOG(MSG, MOD_SSP, "SmoothStream Pub (State 10):"
			"Got .ism file, fetching .ismv file %s [QL: %d, Time: %ld]",
			media_uri, con->ssp.ss_bit_rate, con->ssp.ss_seek_offset);

		ism_cleanup_map(map);
		xml_cleanup_ctx(ism);

		con->ssp.seek_state = 20;
		rv = SSP_SEND_DATA_BACK;
		goto exit;
	    } else {
		DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 10): NULL dataBuf returned for Server Manifest");
		rv = -NKN_SSP_SS_NULL_BUF_INT_FETCH;
		goto exit;
	    }

	    break;

	case 20:
	    // Find the content length of the media file, set a byte range request for mfro box

	    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) { // File not found
                DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 20): .ISMV Media file not found. Cannot proceed");

                rv = -NKN_SSP_SS_ISMV_ISMA_NOT_FOUND;
                goto exit;
            }

	    if (dataBuf != NULL) {
		con->ssp.ssp_content_length = contentLen;

		// Fixed offset from EOF
		phttp->brstart = contentLen - 16;
		phttp->brstop = 0;
		SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		DBG_LOG(MSG, MOD_SSP,
			"SmoothStream Pub (State 20): Requesting mfro box (Off/Size:  %ld/%ld) [Time: %ld]",
			phttp->brstart, con->ssp.ssp_content_length, con->ssp.ss_seek_offset);

		con->ssp.seek_state = 30;
		rv = SSP_SEND_DATA_BACK;
		goto exit;
	    } else {
		DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 20): NULL dataBuf returned for ISMV media file");

                rv = -NKN_SSP_SS_NULL_BUF_INT_FETCH;
                goto exit;
	    }

	    break;

	case 30:
	    /* For fragmented ISMV files, the mfro box is usually the last box
	     * in the file and is of fixed size.
	     * The mfro box gives the offset in the file for the mfra box.
	     * The mfra box in turn has information about the timestamp & offset of each moof fragment
	     */

            if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) { // File not found
                DBG_LOG(WARNING, MOD_SSP, "SmoothStream Pub (State: 30): Byte range fetch for MFRO failed. Cannot proceed");

                rv = -NKN_SSP_SS_ISMV_ISMA_NOT_FOUND;
                goto exit;
            }

	    if (dataBuf != NULL) {
		memset(mfro, 0, 16);
		memcpy(mfro, dataBuf, 16);

		/* read the mfra offset from the mfro box */
		mfra_off = con->ssp.ssp_content_length - mp4_get_mfra_offset(mfro, 16);
		if(mfra_off == con->ssp.ssp_content_length) {
		    DBG_LOG(WARNING, MOD_SSP, "SmoothStream Pub (State: 30): MFRO box offset read failed");
		    rv = -NKN_SSP_SS_MFRO_OFF_FAIL;
		    goto exit;
		}

		phttp->brstart = mfra_off;
		phttp->brstop = 0;
		SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		con->ssp.header_size = con->ssp.ssp_content_length - mfra_off; // Size of MFRA box till the end
                DBG_LOG(MSG, MOD_SSP,
			"SmoothStream Pub (State: 30): Requesting the MFRA box (Off/Size:  %ld/%ld) [Time: %ld]",
			phttp->brstart, con->ssp.ssp_content_length, con->ssp.ss_seek_offset);

                con->ssp.seek_state = 40;
                rv = SSP_SEND_DATA_BACK;
		goto exit;
	    } else {
		DBG_LOG(WARNING, MOD_SSP, "SmoothStream Pub (State: 30): NULL dataBuf returned for MFRO offset");

                rv = -NKN_SSP_SS_NULL_BUF_INT_FETCH;
                goto exit;
	    }

	    break;

	case 40:
	    // Buffered loopback read to copy the entire MFRA box
	    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) { // File not found
                DBG_LOG(WARNING, MOD_SSP, "SmoothStream Pub (State: 40): Byte range fetch for MFRA failed");
                rv = -NKN_SSP_SS_ISMV_ISMA_NOT_FOUND;
                goto exit;
            }

	    if ( !con->ssp.header ) { // Buffer for MFRA box
		con->ssp.header = (uint8_t *)nkn_calloc_type(con->ssp.header_size,
							     sizeof(char), mod_ssp_smoothstream_pub_t);
		if (con->ssp.header == NULL) {
		    DBG_LOG(WARNING, MOD_SSP,
			    "SmoothStream Pub (State: 40): Failed to allocate space for copy MFRA box");
		    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		    rv = -NKN_SSP_SS_MEM_ALLOC_HDR_FAIL; // error case
		    goto exit;
		}
		con->ssp.ssp_partial_content_len = 0;
	    }

	    if (dataBuf != NULL) {
		memcpy(con->ssp.header + con->ssp.ssp_partial_content_len, dataBuf, bufLen);
		con->ssp.ssp_partial_content_len += bufLen;

		if (con->ssp.ssp_partial_content_len < con->ssp.header_size) {
		    DBG_LOG(MSG, MOD_SSP,
			    "SmoothStream Pub (State: 40): Chunked read for MFRA box, read %d of %ld [Time: %ld]",
			    con->ssp.ssp_partial_content_len, con->ssp.header_size, con->ssp.ss_seek_offset);
		    rv = SSP_WAIT_FOR_DATA;
		    goto exit;
		}
	    }

	    // Parse the populated MFRA box to glean the offset and length for the MOOF fragment
	    /* initialize the ismv context */
	    ctx = mp4_init_ismv_parser_ctx(con->ssp.header, con->ssp.header_size, NULL);

	    /* seek to a timestamp in a track id, returns the correct moof
	     * offset and its length; track id determined by call to 'ism_get_track_id' */
	    mp4_frag_seek(ctx, con->ssp.ss_track_id, 0, con->ssp.ss_seek_offset,
			  &moof_offset, &moof_size, &moof_time);

	    if (moof_offset == 0) {
		DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 40): Invalid fragment offset(Off:  %ld)",
			moof_time);
		con->ssp.seek_state = 0;
		rv = -NKN_SSP_SS_ISMV_ISMA_NOT_FOUND;
                goto exit;

	    }

	    con->ssp.ssp_br_offset = moof_offset;
	    phttp->brstart = moof_offset;
	    phttp->brstop = moof_offset + 4 - 1;

	    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    DBG_LOG(MSG, MOD_SSP,
		    "SmoothStream Pub (State: 40): Refetch to parse the moof size value (Off:  %ld)",
		    phttp->brstart);

	    mp4_cleanup_ismv_ctx(ctx);
	    con->ssp.seek_state = 50;
	    rv = SSP_SEND_DATA_BACK;
	    goto exit;

	    break;

	case 50:
	    if (dataBuf != NULL) {
		box_size = get_mp4_boxsize((uint8_t *)dataBuf, 4);
		phttp->brstart += box_size;
		phttp->brstop = phttp->brstart + 4 - 1;
		SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		DBG_LOG(MSG, MOD_SSP,
			"SmoothStream Pub (State: 60): Refetch to parse the moof size value (Off:  %ld)",
			phttp->brstart);
		con->ssp.seek_state = 60;
		rv = SSP_SEND_DATA_BACK;
		goto exit;
	    }
	    else {
		DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 50): NULL dataBuf returned for ISMV media file");
		rv = -NKN_SSP_SS_NULL_BUF_INT_FETCH;
		goto exit;
	    }
	    break;

	case 60:
	    if (dataBuf != NULL) {
		box_size = get_mp4_boxsize((uint8_t *)dataBuf, 4);
		phttp->brstart = con->ssp.ssp_br_offset;
		phttp->brstop += box_size - 4 + 1;
		SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		SET_HTTP_FLAG(phttp, HRF_BYTE_SEEK); // To override the 206 response to 200
		CLEAR_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS); // BZ 6541

		DBG_LOG(MSG, MOD_SSP,
			"SmoothStream Pub (State: 60): Found MOOF offset,size = %ld, %ld [Time: %ld]",
			phttp->brstart, phttp->brstop - phttp->brstart, con->ssp.ss_seek_offset);

		con->ssp.seek_state = 0;
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }
	    else {
		DBG_LOG(WARNING, MOD_SSP,
			"SmoothStream Pub (State: 20): NULL dataBuf returned for ISMV media file");

		con->ssp.seek_state = 0;
		rv = -NKN_SSP_SS_NULL_BUF_INT_FETCH;
		goto exit;
	    }

	    break;

	default:
	    DBG_LOG(WARNING, MOD_SSP,
		    "SmoothStream Pub (State: Default): Not supported state = %d", con->ssp.seek_state);
	    rv = SSP_SEND_DATA_OTW;
            goto exit;
	    break;
    } //switch(con->ssp.seek_state)

 exit:
    return rv;
}


static int
smoothstream_uri_get_ql(const char *uri_data, const char *quality_str,
			uint32_t *bit_rate)
{
    char *p_start_address, *p_end_address;
    char buf[100];
    uint8_t len;

    p_start_address = strstr(uri_data, quality_str);
    if (p_start_address == NULL) {
	DBG_LOG(WARNING, MOD_SSP,
		"SmoothStream Pub (uri_get_ql): Tag not found");
	return -1;
    }

    p_start_address += strlen(quality_str) + 1;

    p_end_address = strstr(p_start_address, ")");
    len = p_end_address - p_start_address;
    memset(buf, 0, 100);
    buf[0] = '\0';
    strncat(buf, p_start_address, len);

    *bit_rate = strtol(buf, NULL, 10);

    return 0;
}

static int
parse_smoothstream_uri(const char *uri_data, const char *quality_str, const char *fragment_str, \
		       const char *video_tracktype, const char * audio_tracktype, \
		       uint8_t *track_type, uint32_t *bit_rate, uint64_t *time_offset)
{
    char *p_start_address;
    char *p_end_address;
    int8_t len;

    char buf[100];

    // Extract the Bit rate information
    p_start_address = strstr(uri_data, quality_str);
    if (p_start_address == NULL) {
        DBG_LOG(WARNING, MOD_SSP,
                "SmoothStream Pub (parse_ss_uri): Tag not found");
        return -1;
    }

    p_start_address += strlen(quality_str) + 1;

    p_end_address = strstr(p_start_address, ")");
    len = p_end_address - p_start_address;
    memset(buf, 0, 100);
    buf[0] = '\0';
    strncat(buf, p_start_address, len);

    *bit_rate = strtol(buf, NULL, 10);

    // Determine track type
    memset(buf, 0, 100);
    p_start_address = strstr(uri_data, fragment_str);
    if (p_start_address == NULL) {
        DBG_LOG(WARNING, MOD_SSP,
                "SmoothStream Pub (uri_get_ql): Tag not found");
        return -1;
    }
    p_start_address += strlen(fragment_str) + 1;

    p_end_address = strstr(p_start_address, ")");
    len = p_end_address - p_start_address;
    buf[0] = '\0';
    strncat(buf, p_start_address, len);

    if ( strstr(buf, video_tracktype) ) {
	*track_type = 0;
    } else if (strstr(buf, audio_tracktype)) {
	*track_type = 1;
    }

    // Extract the Time Offset
    memset(buf, 0, 100);
    p_start_address = strstr(uri_data, fragment_str);
    if (p_start_address == NULL) {
        DBG_LOG(WARNING, MOD_SSP,
                "SmoothStream Pub (uri_get_ql): Tag not found");
        return -1;
    }
    p_start_address += strlen(fragment_str) + 1 + ( track_type==0?strlen(video_tracktype):strlen(audio_tracktype) ) + 1;

    p_end_address = strstr(p_start_address, ")");
    len = p_end_address - p_start_address;
    buf[0] = '\0';
    strncat(buf, p_start_address, len);

    *time_offset = strtol(buf, NULL, 10);

    return 0;
}


static int
find_base_uri_endpos(const char *uri_data, const char *quality_str)
{
    UNUSED_ARGUMENT(quality_str);

    char *p_start_address;
    //    char *p_end_address;
    //int16_t len, pos;
    //char *buf;
    /*
    // Extract the Bit rate information
    p_end_address = strstr(uri_data, quality_str) - 1;

    len = p_end_address - uri_data;
    buf = (char *)alloca(len+1);
    memset(buf, 0, len+1);
    strncpy(buf, uri_data, len);

    // One additional directory back step
    p_start_address = strrchr(buf, '/');
    pos = p_start_address - buf + 1; // Includes the trailing /
    */
    int16_t pos;
    p_start_address = strrchr(uri_data, '/');
    pos = p_start_address - uri_data + 1; // Includes the trailing /
    return pos;
}


static uint32_t
get_mp4_boxsize(uint8_t *src, size_t size)
{
    bitstream_state_t *bs;
    uint32_t off;

    bs = ioh.ioh_open((char*)src, "rb", size);
    off = 0;
    ioh.ioh_read((void*)(&off), 1, sizeof(uint32_t), (void*)(bs));
    off = nkn_vpe_swap_uint32(off);
    ioh.ioh_close((void*)bs);
    return off;

}

/*
 * Flash HTTP Adapative Streaming Server Side Origin Module (Player)
 * Aug 20, 2010
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

#include "nkn_vpe_f4v_frag_access_api.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )
/* extern char *strcasestr(const char *haystack, const char *needle); */

/////////////////////////////////////
// Flash HTTP Adaptive Streaming   //
// Server Side Player (SSP Type 7) //
/////////////////////////////////////

// Prototypes
static int parse_zeri_uri(const char *uri_data, const char *seg_str, \
                          const char *frag_str, const char *delim_str, \
                          int64_t *seg_index,  int64_t *frag_index, int16_t *seguri_len);

// Flash Streaming On Demand Publisher (Origin Zeri Module)
int
ssp_zeri_streamingSSP(con_t *con, const char *uriData, int uriLen, \
		      const ssp_config_t *ssp_cfg, \
		      off_t contentLen, const char *dataBuf, int bufLen)
{
    char *uribuf, *remap_uribuf;
    int64_t seg_index, frag_index;
    int16_t segurl_pos;
    int32_t num_segs;
    int remap_len=0, rv;

    http_cb_t *phttp = &con->http;

    f4v_frag_access_ctx_t *f4v_ctx;
    vpe_olt_t olt;

    // Obtain the relative URL length stripped of all query params
    if ( (remap_len = findStr(uriData, "?", uriLen)) == -1 ) {
        remap_len = uriLen;
    }
    uribuf = (char *)alloca(remap_len + SSP_MAX_EXTENSION_SZ);
    memset(uribuf, 0, remap_len + SSP_MAX_EXTENSION_SZ);
    uribuf[0] = '\0';
    strncat(uribuf, uriData, remap_len);

    /********************************************************************
     * State Flow for Zeri Publishing
     * State 0:
     *          Remap URI to fetch .F4X file, SEND_DATA_BACK_TO_SSP
     * State 10:
     *          Buffered read to fetch the entire F4X file, Parse and
     *		obtain the  offsets (SSP_WAIT_FOR_DATA)
     * State 20:
     *          Remap URI to F4F file with correct offset, SEND_DATA_OTW
     ********************************************************************/
    switch(con->ssp.seek_state) {

	case 0:
            // Reset all stateful variables to 0 (Helpful when GET requests are pipelined
	    // in the same keep alive conn)
            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
            con->ssp.header_size = 0;
            if ( con->ssp.header ) {
                free(con->ssp.header);
                con->ssp.header = NULL;
            }
            con->ssp.ssp_partial_content_len = 0;
	    phttp->p_ssp_cb->ssp_streamtype = 0;
	    con->ssp.fs_seg_index = 0;
	    con->ssp.fs_frag_index = 0;

	    // If request is for manifest (.f4m) or segment (.f4f) or index (.f4x) send directly to client
	    if ( strcasestr(uribuf, ".f4m") != NULL || strcasestr(uribuf, ".f4f") != NULL \
		 || strcasestr(uribuf, ".f4x") != NULL ) {
		DBG_LOG(MSG, MOD_SSP, "FlashStream Pub: Direct request for F4M/F4F/F4X - %s", uribuf);
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }

	    // Parse the Zeri request
	    rv = parse_zeri_uri(uribuf, ssp_cfg->flashstream_pub.seg_tag,
				ssp_cfg->flashstream_pub.frag_tag,
				ssp_cfg->flashstream_pub.seg_frag_delimiter,
				&seg_index, &frag_index, &segurl_pos);
	    if (rv < 0) {
		// Could not match URI to Adobe format, pass the request through
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }

	    if (seg_index <= 0 || frag_index <= 0 || segurl_pos <= 0 ) { // Error in URI format
		rv = -NKN_SSP_FS_URI_FORMAT_ERR;
		DBG_LOG(ERROR, MOD_SSP,
			"negative indices - Seg Idx: %ld, Frag Idx: %ld, SegEnd Pos: %d, (Error: %d)",
			seg_index, frag_index, segurl_pos, rv);
		goto exit;
	    }
	    con->ssp.fs_seg_index = seg_index;
	    con->ssp.fs_frag_index = frag_index;
	    con->ssp.fs_segurl_pos = segurl_pos;

	    // Remap URI to fetch .F4X file, SEND_DATA_BACK_TO_SSP
	    remap_uribuf = (char *)alloca(segurl_pos + SSP_MAX_EXTENSION_SZ);
	    memset(remap_uribuf, 0, segurl_pos + SSP_MAX_EXTENSION_SZ);
	    remap_uribuf[0] = '\0';
	    strncat(remap_uribuf, uriData, segurl_pos);
	    strncat(remap_uribuf, ".f4x", strlen(".f4x"));
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, remap_uribuf, strlen(remap_uribuf));
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, remap_uribuf, strlen(remap_uribuf));
	    DBG_LOG(MSG, MOD_SSP, "FlashStream Pub (State: 0): Fetch .F4x file %s", remap_uribuf);

	    con->ssp.seek_state = 10;
	    rv = SSP_SEND_DATA_BACK;
	    goto exit;

	    break;

	case 10:
	    // Do a buffered read till all the F4X data is read, then parse and find offset
	    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) { // File not found
                DBG_LOG(WARNING, MOD_SSP, "FlashStream Pub (State: 10): Fetch for F4X file failed");
                rv = -NKN_SSP_FS_F4X_NOT_FOUND;
                goto exit;
            }

	    if ( !con->ssp.header ) { // Buffer for F4X box
		con->ssp.header_size = contentLen;
                con->ssp.header = (uint8_t *)nkn_calloc_type(con->ssp.header_size, \
							     sizeof(char), mod_ssp_flashstream_pub_t);
                if (con->ssp.header == NULL) {
                    DBG_LOG(WARNING, MOD_SSP,
			    "FlashStream Pub (State: 10): Failed to allocate space for F4X box");
                    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                    rv = -NKN_SSP_FS_MEM_ALLOC_HDR_FAIL; // error case
                    goto exit;
                }
                con->ssp.ssp_partial_content_len = 0;
	    }// !con->ssp.header

	    if (dataBuf != NULL) {
		memcpy(con->ssp.header + con->ssp.ssp_partial_content_len,
		       dataBuf, bufLen);
		con->ssp.ssp_partial_content_len += bufLen;

		if (con->ssp.ssp_partial_content_len < con->ssp.header_size) {
		    DBG_LOG(MSG, MOD_SSP,
			    "FlashStream Pub (State: 10): Chunked read for F4X %d of %ld",
			    con->ssp.ssp_partial_content_len, con->ssp.header_size);
		    rv = SSP_WAIT_FOR_DATA;
		    goto exit;
		} // con->ssp.ssp_partial
	    } // dataBuf != NULL

	    // initialize the f4v context
	    f4v_ctx = init_f4v_frag_access_ctx();
	    if (!f4v_ctx) {
		DBG_LOG(ERROR, MOD_SSP, "FlashStream Pub (State: 10): Unable to create F4X context");
		rv = -NKN_SSP_FS_F4X_CONTEXT_FAILURE;
		goto exit;
	    }

	    // read the index for the current profile
	    f4v_create_profile_index(f4v_ctx, con->ssp.header, con->ssp.header_size, con->ssp.fs_seg_index);

	    // read the number of segments in the current index
	    num_segs = f4v_get_num_segs(f4v_ctx);

	    // read the offset&length for a [segment, frag] pair
            rv = f4v_get_afra_olt(f4v_ctx, con->ssp.fs_seg_index, con->ssp.fs_frag_index, &olt);
	    if (rv < 0) {
		DBG_LOG(ERROR, MOD_SSP, \
			"FlashStream Pub (State: 10): Frag offset parse failure (Seg %ld, Frag %ld) [Error: %d]", \
			con->ssp.fs_seg_index, con->ssp.fs_frag_index, rv);
                rv = -NKN_SSP_FS_F4F_OFFSET_PARSE_ERROR;
                goto exit;
	    }

	    // Remap URI to fetch .F4F file, SEND_DATA_BACK_TO_SSP
            remap_uribuf = (char *)alloca(con->ssp.fs_segurl_pos + SSP_MAX_EXTENSION_SZ);
	    memset(remap_uribuf, 0, con->ssp.fs_segurl_pos + SSP_MAX_EXTENSION_SZ);
	    remap_uribuf[0] = '\0';
            strncat(remap_uribuf, uriData, con->ssp.fs_segurl_pos);
            strncat(remap_uribuf, ".f4f", strlen(".f4f"));
            add_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, remap_uribuf, strlen(remap_uribuf));
            add_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, remap_uribuf, strlen(remap_uribuf));
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
            DBG_LOG(MSG, MOD_SSP, "FlashStream Pub (State: 10): Fetch .F4F file %s", remap_uribuf);

            // Set the frag byte range fetch
            phttp->brstart = olt.offset;
	    if (olt.length != 0xffffffffffffffff)
		phttp->brstop  = olt.offset + olt.length - 1;
            SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
            SET_HTTP_FLAG(phttp, HRF_BYTE_SEEK); // To override the 206 response to 200
            DBG_LOG(MSG, MOD_SSP, \
                    "FlashStream Pub (State: 10): Found F4F offset for " \
		    "(Seg: %ld, Frag %ld, Off: %ld, Len: %ld)",\
                    con->ssp.fs_seg_index, con->ssp.fs_frag_index, olt.offset, olt.length);

	    phttp->p_ssp_cb->ssp_streamtype = NKN_SSP_FLASHSTREAM_STREAMTYPE_FRAGMENT; //video/f4f

	    con->ssp.seek_state = 0; // Set state to clean up all stateful variables
	    rv = SSP_SEND_DATA_OTW;

	    cleanup_f4v_profile_index(f4v_ctx);
	    cleanup_f4v_frag_access_ctx(f4v_ctx);

	    break;

        default:
            DBG_LOG(WARNING, MOD_SSP,
		    "FlashStream Pub (State: Default): Not supported state = %d",
		    con->ssp.seek_state);
            rv = SSP_SEND_DATA_OTW;
            goto exit;
            break;

    }

 exit:
    return rv;
}


static int
parse_zeri_uri(const char *uri_data, const char *seg_str, \
	       const char *frag_str, const char *delim_str, \
	       int64_t *seg_index,  int64_t *frag_index, int16_t *seguri_len)
{
    char *p_uritag, *p_seg_pos, *p_frag_pos, *p_delim_pos, *p_tmp_pos;
    char *buf;
    int len, buflen, rv=0;

    *seg_index = -1;
    *frag_index = -1;

    // Find the last '/' delimiter
    p_uritag = strrchr(uri_data, '/');
    if ( p_uritag == NULL ) {
	rv = -NKN_SSP_FS_URI_FORMAT_ERR; // Pass the request through...
	DBG_LOG(WARNING, MOD_SSP, "FlashStream Pub: Uri does not start with a / :%s, (Error: %d)",
		uri_data, rv);
	goto exit;
    }

    // Check for Seg, Frag & Delimiter tag presence
    p_seg_pos = strstr(p_uritag, seg_str);
    if (p_seg_pos == NULL) {
	rv = -NKN_SSP_FS_SEGFRAG_TAG_NOT_FOUND; // Pass the request
        DBG_LOG(ERROR, MOD_SSP, "FlashStream Pub:"
                "Mandatory seg tag missing %s, pass the request (Error: %d)",
                p_uritag, rv);
        goto exit;
    }

    while (p_seg_pos != NULL) { //Recursive check for the last Seg tag
	p_tmp_pos = strstr(p_seg_pos + strlen(seg_str), seg_str);
	if (p_tmp_pos == NULL) {
	    break;
	} else {
	    p_seg_pos = p_tmp_pos;
	    p_tmp_pos = NULL;
	}
    }

    p_delim_pos = strstr(p_seg_pos, delim_str);
    if (p_delim_pos == NULL) {
        rv = -NKN_SSP_FS_SEGFRAG_TAG_NOT_FOUND; // Pass the request
        DBG_LOG(ERROR, MOD_SSP, "FlashStream Pub:"
                "Mandatory delimiter tag missing %s, pass the request (Error: %d)",
                p_uritag, rv);
        goto exit;
    }

    p_frag_pos 	= strstr(p_seg_pos, frag_str);
    if (p_frag_pos == NULL) {
        rv = -NKN_SSP_FS_SEGFRAG_TAG_NOT_FOUND; // Pass the request
        DBG_LOG(ERROR, MOD_SSP, "FlashStream Pub:"
                "Mandatory frag tag missing %s, pass the request (Error: %d)",
                p_uritag, rv);
        goto exit;
    }

    /*    while (p_delim_pos != NULL) { // Recursive check for delimiters...
	p_tmp_pos = strstr(p_delim_pos + strlen(delim_str), delim_str);
	if (p_tmp_pos == NULL) {
	    break;
	} else {
	    p_delim_pos = p_tmp_pos;
	    p_tmp_pos = NULL;
	}
	}

    if ( p_seg_pos == NULL || p_frag_pos == NULL || p_delim_pos == NULL) {
	rv = -NKN_SSP_FS_SEGFRAG_TAG_NOT_FOUND; // Pass the request
	DBG_LOG(ERROR, MOD_SSP, "FlashStream Pub:"
		"Mandatory seg/frag/delim tag missing %s, pass the request (Error: %d)",
		p_uritag, rv);
	goto exit;
	}*/

    buflen = strlen(p_uritag);
    buf = (char *)alloca(buflen*2);
    memset(buf, 0, buflen*2);

    p_seg_pos += strlen(seg_str);
    len =  p_delim_pos - p_seg_pos;
    buf[0] = '\0';
    strncat(buf, p_seg_pos, len);

    // Obtain the Seg Index
    *seg_index = strtol(buf, NULL, 10);

    *seguri_len = p_delim_pos - uri_data;

    // Obtain the Frag Index
    p_frag_pos += strlen(frag_str);
    len = strlen(p_frag_pos);

    memset(buf, 0, buflen*2);
    buf[0] = '\0';
    strncat(buf, p_frag_pos, len);

    *frag_index = strtol(buf, NULL, 10);

 exit:
    return rv;
}

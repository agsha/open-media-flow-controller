#ifndef _PARSER_ERR_
#define _PARSER_ERR_

/* file conversion error codes 1xx */
/** \def CONTINUE FILE CONVERSION */
#define E_VPE_FILE_CONVERT_CONTINUE 105
/** \def STOP FILE CONVERSION */
#define E_VPE_FILE_CONVERT_STOP 106
/** \def SKIPP FILE CONVERSION */
#define E_VPE_FILE_CONVERT_SKIP 107
#define E_VPE_FILE_CONVERT_CLEANUP_PENDING 108
#define E_VPE_INVALID_FILE_FROMAT 109
#define E_VPE_VID_ONLY_NOT_SUPPORTED 110
#define E_VPE_AUD_ONLY_NOT_SUPPORTED 111
#define E_VPE_READ_INCOMPLETE 112
#define E_VPE_FORMATTER_ERROR 113
/* generic parser error codes 2xx*/
/** \def Out of memory */
#define E_VPE_PARSER_NO_MEM 201
/** \def unable to open file descriptor */
#define E_VPE_PARSER_INVALID_FILE 202
/** \def invalid output path (unable to write file) */
#define E_VPE_PARSER_INVALID_OUTPUT_PATH 203
/** \def Fatal error, invalid state change or some detectable data
    corruption */
#define E_VPE_PARSER_FATAL_ERR 204
/** \def Invalid context/ state */
#define E_VPE_PARSER_INVALID_CTX 205
/** \def MAX processing limit defined by number of profiles */
#define E_VPE_MAX_PROFILE_ERR 206
/** \def all segments do not have the same segment duration
 * this is specific to ABR publishing and strictly speaking may not be
 * an error if the underlying ABR technology can handle the same
 */
#define E_VPE_INCONSISENT_SEG_DURATION 207
/** \def I/O Error - Write Failed */
#define E_VPE_PARSER_WRITE_FAILE 208
/*payload related errors like timestamp gap*/
#define E_VPE_PARSER_PAYLOAD_TIMESTAMP_GAP 209
#define E_VPE_PARSER_PAYLOAD_CHUNKING_ERR 210
#define E_VPE_AV_INTERLEAVE_ERR 211

/* MP4 parsing error codes 3xx */
/** \def No MOOV in the MOOV search search window */
#define E_VPE_MP4_MOOV_ERR 301
#define E_VPE_MP4_NO_STSS_TAB 302
#define E_VPE_NO_AVCC_BOX 303
#define E_VPE_MP4_MISALIGMENT 304
#define E_VPE_MP4_ESDS_ERR 305
#define E_VPE_MP4_AV_SYNC_ERROR 306
#define E_VPE_PROFILE_LEVEL_ERROR 307
#define E_VPE_VIDEO_CODEC_ERR 308
#define E_VPE_DIFF_MOOFS_ACROSS_PROFILE 309
#define E_MP4_PARSE_ERR 310
#define E_VPE_MP4_NO_ESDS_BOX 311
#define E_VPE_MP4_ESDS_PARSE_ERR 312
#define E_VPE_MP4_SEEK_UNDEFINED_PROVIDER 313
#define E_VPE_MP4_SEEK_UNKNOWN_PROVIDER 314
#define E_VPE_MP4_OOB 315

/* ISMx parsing related error codes 400-450 */
/** \def invalid context */
#define E_ISMV_INVALID_CTX 401
/** \def out of memory */
#define E_ISMV_NOMEM 402
/** \def invalid trak timescale */
#define E_ISMV_INVALID_TIMESCALE 403
/** \def no more fragments to parse */
#define E_ISMV_NO_MORE_FRAG 404
/** \def required tag missing in ISMC file */
#define E_ISMC_PARSE_ERR 405
#define E_ISM_PARSE_ERR 406
/** \def required tag missing in ISMV file */
#define E_ISMV_PARSE_ERR 407
/** \def MFRA box missing */
#define E_ISMV_MFRA_MISSING 408
#define E_VPE_MISMATCH_CUST_ATTR 409

/* HTTP Sync Reader Error Codes 450 - 500 */
/** \def invalid HTTP version number */
#define E_VPE_HTTP_SYNC_VER_ERR 450
#define E_VPE_HTTP_SYNC_PARSE_ERR 451

#define E_VPE_F4V_BASE 500
#define E_VPE_F4V_INVALID_F4M (E_VPE_F4V_BASE+1)
/** \def invalid frag request */
#define E_VPE_F4V_INVALID_FRAG (E_VPE_F4V_INVALID_F4M+1)

/* FLV parser error codes 600*/
/** \def unhandled data span across buffers */
#define E_VPE_FLV_PARSER_SPAN_UNHANDLED 601
#define E_VPE_FLV_PARSER_MEM_ALLOC_ERROR 602

/** error codes for TS creation **/
#define E_VPE_TS_CREATION_FAILED  800
#define E_VPE_NO_VIDEO_PKT 801

/** error codes for TS segmentation ***/
#define E_VPE_DIFF_TS_CHUNKS_ACR_PROFILE 901
#define E_VPE_DIFF_TS_CHUNK_DURATION 902
#define E_VPE_NO_PMT_FOUND 903
#define E_VPE_ERR_PARSE_PIDS 904
#define E_VPE_MFU_CREATION_FAILED 905
#define E_VPE_DIFF_NUM_TS_CHUNKS 906
#define E_VPE_NO_SYNC_BYTE 907
#define E_VPE_ERR_PARSE_PES 908
#define E_VPE_SL_FMT_INIT_ERR 909
#define E_VPE_MFU2SL_ERR 910
#endif //_PARSER_ERR_

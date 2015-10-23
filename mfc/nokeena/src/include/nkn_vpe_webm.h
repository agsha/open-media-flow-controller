/*
 * Filename: nkn_vpe_webm.h
 * Data:
 * Function: basic definition of webm container.
 *
 *
 */


#ifndef _NKN_VPE_WEBM_DEFS_
#define _NKN_VPE_WEBM_DEFS_
/*
 * The webm container is a subset of MKV
 * For MKV spec, please check:
 * http://www.matroska.org/technical/diagram/index.html
 * http://www.matroska.org/technical/specs/index.html
 * For webm, please check:
 * http://www.webmproject.org/code/specs/container/
 *
 *
 *
 *
 */


/* ebml types */
typedef enum {
    EBML_INT,
    EBML_UINT,
    EBML_FLOAT,
    EBML_STR,
    EBML_UTF8,
    EBML_DATA,
    EBML_MASTER_ELEMENT,
    EBML_BIN,
}nkn_vpe_ebml_type_t;

typedef enum {
    WEBM_VIDEO_TRAK    = 0x01,
    WEBM_AUDIO_TRAK    = 0x02,
    WEBM_COMPLEX_TRAK  = 0x03,
    WEBM_LOGO_TRAK     = 0x10,
    WEBM_SUBTITLE_TRAK = 0x11,
    WEBM_BUTTONS_TRAK  = 0x12,
    WEBM_CONTROL_TRAK  = 0x20,
}nkn_vpe_webm_trak_type_t;

/* ebml header */
#define EBML                       0x1A45DFA3
#define EBML_VERSION               0x4286
#define EBML_READ_VERSION          0x42F7
#define EBML_MAX_ID_LENGTH         0x42F2
#define EBML_MAX_SIZE_LENGTH       0x42F3
#define EBML_DOC_TYPE              0x4282
#define EBML_DOC_TYPE_VERSION      0x4287
#define EBML_DOC_TYPE_READ_VERSION 0x4285

/* Global elements (used everywhere in the format) */
#define EBML_VOID   0xec
#define EBML_CRC_32 0xbf

/* Segment */
#define WEBM_SEGMENT 0x18538067

/* Meta seek information */
#define WEBM_SEEKHEAD     0x114D9B74
#define WEBM_SEEK         0x4DBB
#define WEBM_SEEKID       0x53AB
#define WEBM_SEEKPOSITION 0x53AC

/* Segment Informaton */
#define WEBM_SEGMENT_INFO                  0x1549A966
#define WEBM_SEGMENT_UID                   0x73A4
#define WEBM_SEGMENT_FILENAME              0x7384
#define WEBM_PREV_UID                      0x3CB923
#define WEBM_PREV_FILENAME                 0x3C83AB
#define WEBM_NEXT_UID                      0x3EB923
#define WEBM_NEXT_FILENAME                 0x3E83BB
#define WEBM_SEGMENT_FAMILY                0x4444
#define WEBM_CHAPTER_TRANSLATE             0x6924
#define WEBM_CHAPTER_TRANSLATE_EDITION_UID 0x69FC
#define WEBM_CHAPTER_TRANSLATE_CODEC       0x69BF
#define WEBM_CHAPTER_TRANSLATE_ID          0x69A5
#define WEBM_TIMECODE_SCALE                0x2AD7B1
#define WEBM_DURATION                      0x4489
#define WEBM_DATEUTC                       0x4461
#define WEBM_TITLE                         0x7BA9
#define WEBM_MUXING_APP                    0x4D80
#define WEBM_WRITIN_GAPP                   0x5741

/* Cluster */
#define WEBM_CLUSTER             0x1F43B675
#define WEBM_TIMECODE            0xE7
#define WEBM_SILENT_TRACKS       0x5854
#define WEBM_SILENT_TRACK_NUMBER 0x58D7
#define WEBM_POSITION            0xA7
#define WEBM_PREV_SIZE           0xAB
#define WEBM_BLOCK_GROUP         0xA0
#define WEBM_BLOCK               0xA1
#define WEBM_BLOCK_ADDITIONS     0x75A1
#define WEBM_BLOCK_MORE          0xA6
#define WEBM_BLOCK_ADD_ID        0xEE
#define WEBM_BLOCK_ADDITIONAL    0xA5
#define WEBM_BLOCK_DURATION      0x9B
#define WEBM_REFERENCE_PRIORITY  0xFA
#define WEBM_REFERENCE_BLOCK     0xFB
#define WEBM_CODEC_STATE         0xA4
#define WEBM_SLICES              0x8E
#define WEBM_TIME_SLICE          0xE8
#define WEBM_LACE_NUMBER         0xCC
#define WEBM_SIMPLE_BLOCK        0xA3

/* Track */
#define WEBM_TRACKS                      0x1654AE6B
#define WEBM_TRACK_ENTRY                 0xAE
#define WEBM_TRACK_NUMBER                0xD7
#define WEBM_TRACK_UID                   0x73C5
#define WEBM_TRACK_TYPE                  0x83
#define WEBM_FLAG_ENABLED                0xB9
#define WEBM_FLAG_DEFAULT                0x88
#define WEBM_FLAG_FORCED                 0x55AA
#define WEBM_FLAG_LACING                 0x9C
#define WEBM_MIN_CACHE                   0x6DE7
#define WEBM_MAX_CACHE                   0x6DF8
#define WEBM_DEFAULT_DURATION            0x23E383
#define WEBM_TRACK_TIMECODE_SCALE        0x23314F
#define WEBM_MAX_BLOCK_ADDITION_ID       0x55EE
#define WEBM_NAME                        0x536E
#define WEBM_LANGUAGE                    0x22B59C
#define WEBM_CODEC_ID                    0x86
#define WEBM_CODEC_PRIVATE               0x63A2
#define WEBM_CODEC_NAME                  0x258688
#define WEBM_ATTACHMENT_LINK             0x7446
#define WEBM_CODEC_DECODE_ALL            0xAA
#define WEBM_TRACK_OVERLAY               0x6FAB
#define WEBM_TRACK_TRANSLATE             0x6624
#define WEBM_TRACK_TRANSLATE_EDITION_UID 0x66FC
#define WEBM_TRACK_TRANSLATE_CODEC       0x66BF
#define WEBM_TRACK_TRANSLATE_TRACK_ID    0x66A5
/* video settings of video track */
#define WEBM_VIDEO                       0xE0
#define WEBM_FLAG_INTERLACED             0x9A
#define WEBM_STEREO_MODE                 0x53B8
#define WEBM_PIXEL_WIDTH                 0xB0
#define WEBM_PIXEL_HEIGHT                0xBA
#define WEBM_PIXEL_CROP_BOTTOM           0x54AA
#define WEBM_PIXEL_CROP_TOP              0x54BB
#define WEBM_PIXEL_CROP_LEFT             0x54CC
#define WEBM_PIXEL_CROP_RIGHT            0x54DD
#define WEBM_DISPLAY_WIDTH               0x54B0
#define WEBM_DISPLAY_HEIGHT              0x54BA
#define WEBM_DISPLAY_UNIT                0x54B2
#define WEBM_ASPECT_RATIO_TYPE           0x54B3
#define WEBM_COLOR_SPACE                 0x2EB524
#define WEBM_FRAME_RATE                  0x2383E3
/* Audio settings of audio track*/
#define WEBM_AUDIO                       0xE1
#define WEBM_SAMPLING_FREQUENCY          0xB5
#define WEBM_OUTPUT_SAMPLING_FREQUENCY   0x78B5
#define WEBM_CHANNELS                    0x9F
#define WEBM_BIT_DEPTH                   0x6264
/* Content Encoding of track */
#define WEBM_CONTENT_ENCODINGS           0x6D80
#define WEBM_CONTENT_ENCODING            0x6240
#define WEBM_CONTENT_ENCODING_ORDER      0x5031
#define WEBM_CONTENT_ENCODING_SCOPE      0x5032
#define WEBM_CONTENT_ENCODING_TYPE       0x5033
#define WEBM_CONTENT_COMPRESSION         0x5034
#define WEBM_CONTENT_COMP_ALGO           0x4254
#define WEBM_CONTENT_COMP_SETTINGS       0x4255
#define WEBM_CONTENT_ENCRYPTION          0x5035
#define WEBM_CONTENT_ENC_ALGO            0x47E1
#define WEBM_CONTENT_ENC_KEY_ID          0x47E2
#define WEBM_CONTENT_SIGNATURE           0x47E3
#define WEBM_CONTENT_SIG_KEY_ID          0x47E4
#define WEBM_CONTENT_SIG_ALOG            0x47E5
#define WEBM_CONTENT_SIG_HASH_ALGO       0x47E6

/* Cueing data */
#define WEBM_CUES                 0x1C53BB6B
#define WEBM_CUE_POINT            0xBB
#define WEBM_CUE_TIME             0xB3
#define WEBM_CUE_TRACK_POSITIONS  0xB7
#define WEBM_CUE_TRACK            0xF7
#define WEBM_CUE_CLUSTER_POSITION 0xF1
#define WEBM_CUE_BLOCK_NUMBER     0x5378

/* Attachments */
#define WEBM_ATTACHMENTS      0x1941A469
#define WEBM_ATTACHED_FILE    0x61A7
#define WEBM_FILE_DESCRIPTION 0x467E
#define WEBM_FILE_NAME        0x466E
#define WEBM_FILE_MIME_TYPE   0x4660
#define WEBM_FILE_DATA        0x465C
#define WEBM_FILE_UID         0x46AE

/*
 * Chapters
 * All elements in Chapets are undecided now according
 * to webm specs
 */
#define WEBM_CHAPTERS                    0x1043A770
#define WEBM_EDITION_ENTRY               0x45B9
#define WEBM_EDITION_UID                 0x45BC
#define WEBM_EDITION_FLAG_HIDDEN         0x45BD
#define WEBM_EDITION_FLAG_DEFAULT        0x45DB
#define WEBM_EDITION_FLAG_ORDERED        0x45DD
#define WEBM_CHAPTER_ATOM                0xB6
#define WEBM_CHAPTER_UID                 0x73C4
#define WEBM_CHAPTER_TIME_START          0x91
#define WEBM_CHAPTER_TIME_END            0x91
#define WEBM_CHAPTER_FLAG_HIDDEN         0x98
#define WEBM_CHAPTER_FLAG_ENABLED        0x4598
#define WEBM_CHAPTER_SEGMENT_UID         0x6E67
#define WEBM_CHAPTER_SEGMENT_EDITION_UID 0x6EBC
#define WEBM_CHAPTER_PHYSICAL_EQUIV      0x63C3
#define WEBM_CHAPTER_TRACK               0x8F
#define WEBM_CHAPTER_TRACK_NUMBER        0x89
#define WEBM_CHAPTER_DISPLAY             0x80
#define WEBM_CHAP_STRING                 0x85
#define WEBM_CHAP_LANGUAGE               0x437C
#define WEBM_CHAP_COUNTRY                0x437E
#define WEBM_CHAP_PROCESS                0x6944
#define WEBM_CHAP_PROCESS_CODEC_ID       0x6955
#define WEBM_CHAP_PROCESS_PRIVATE        0x450D
#define WEBM_CHAP_PROCESS_COMMAND        0x6911
#define WEBM_CHAP_PROCESS_TIME           0x6922
#define WEBM_CHAP_PROCESS_DATA           0x6933

/*
 * Tagging
 * All elements in Tagging are undecided now according
 * according to webm spec
 */
#define WEBM_TAGS                0x1254C367
#define WEBM_TAG                 0x7373
#define WEBM_TARGETS             0x63C0
#define WEBM_TARGET_TYPE_VALUE   0x68CA
#define WEBM_TARGET_TYPE         0x63CA
#define WEBM_TAGS_TRACK_UID      0x63C5
#define WEBM_TAGS_EDITION_UID    0x63C9
#define WEBM_TAGS_CHAPTER_UID    0x63C4
#define WEBM_TAGS_ATTACHMENT_UID 0x63C6
#define WEBM_SIMPLE_TAG          0x67C8
#define WEBM_TAG_NAME            0x45A3
#define WEBM_TAG_LANGUAGE        0x447A
#define WEBM_TAG_DEFAULT         0x4484
#define WEBM_TAG_STRING          0x4487
#define WEBM_TAG_BINARY          0x4485



#endif // _NKN_VPE_WEBM_DEFS_

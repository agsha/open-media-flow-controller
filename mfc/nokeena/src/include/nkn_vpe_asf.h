/*
 * Filename: nkn_vpe_asf.h
 * Data:
 * Function: basic definition of microsoft asf container.
 *
 *
 */


#ifndef _NKN_VPE_ASF_DEFS_
#define _NKN_VPE_ASF_DEFS_
/*
 * The Advanced Systems Format (ASF) is the file format used by
 * Windows Media. Audio and/or Video content compressed with a
 * wide variety of codecs can be stored in an ASF file
 *
 * The spec of ASF can be found at
 * http://www.microsoft.com/download/en/details.aspx?displaylang=en&id=14995
 */

/*
 * Only ASF Header Object and File Properties OBject are
 * parsed, other Object will be bypass now.
 */

#include "nkn_vpe_types.h"

#define ASF_OBJ_GUID_LEN     16
#define ASF_OBJ_SIZE_LEN      8
/* Header Object*/
#define ASF_HD_OBJ_ID_LEN    16
#define ASF_HD_OBJ_SIZE_LEN   8
#define ASF_HD_NUM_OBJ_LEN    4
#define ASF_HD_RESERVED1_LEN  1
#define ASF_HD_RESERVED2_LEN  1

/* File Properties Object */
#define ASF_FILE_PROP_HD_OBJ_ID_LEN            16
#define ASF_FILE_PROP_HD_OBJ_SIZE_LEN           8
#define ASF_FILE_PROP_HD_FILE_ID_LEN           16
#define ASF_FILE_PROP_HD_FILE_SIZE_LEN          8
#define ASF_FILE_PROP_HD_CREATE_DATA_LEN        8
#define ASF_FILE_PROP_HD_DATA_PAK_CNT_LEN       8
#define ASF_FILE_PROP_HD_PLAY_DUR_LEN           8
#define ASF_FILE_PROP_HD_SEND_DUR_LEN           8
#define ASF_FILE_PROP_HD_PREROLLLEN             8
#define ASF_FILE_PROP_HD_FLAG_LEN               4
#define ASF_FILE_PROP_HD_MIN_DATA_PAK_SIZE_LEN  4
#define ASF_FILE_PROP_HD_MAX_DATA_PAK_SIZE_LEN  4
#define ASF_FILE_PROP_HD_MAX_BITRATE_LEN        4

/* ASF Header Object */
const uint8_t guid_asf_header[ASF_HD_OBJ_ID_LEN] = {
    0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C
};

/* ASF File Properties Object */
const uint8_t guid_asf_file_prop_header[ASF_FILE_PROP_HD_OBJ_ID_LEN] = {
    0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};


#endif // _NKN_VPE_ASF_DEFS_

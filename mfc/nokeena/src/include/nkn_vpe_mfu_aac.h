#ifndef _NKN_VPE_MFU_AAC__
#define _NKN_VPE_MFU_AAC__

#include "nkn_vpe_ismv_mfu.h"
#include "rtp_mpeg4_es.h"

#define MP2_AAC 13
#define MP4_AAC 14
#define FR_LEN_NO_PROTECTION 7
#define FR_LEN_PROTECTION 9


typedef struct adtsaac{
    uint8_t is_mpeg2;
    uint8_t object_type;
    uint8_t sr_index;
    uint16_t sample_len;
    uint8_t num_channels;
}adts_t;


/*=================Function declarations==================*/

int32_t mp4_get_esds_box(uint8_t*, uint8_t **);
int32_t mp4_handle_esds(uint8_t*, adts_t*);
int32_t mfu_get_aac_headers(stbl_t* , adts_t *);
//uint8_t mp4_read_object_type_indication(uint8_t);
int32_t mfu_form_aac_header(adts_t*,uint8_t**,int32_t*);
int32_t  mfu_dump_aac_unit(uint8_t*, int32_t,audio_pt*, moof2mfu_desc_t*);//uint8_t*, int32_t, FILE*);




/*standard related information: For reference*/

/*
========  MPEG-4 Audio Object Types: ==================
  0: Null
  1: AAC Main
  2: AAC LC (Low Complexity)
  3: AAC SSR (Scalable Sample Rate)
  4: AAC LTP (Long Term Prediction)
  5: SBR (Spectral Band Replication)
...........
========================================================


Section 2.6.6.2 for 14496 1/3
=============ValueObjectTypeIndication Description======

    0x00 Forbidden
    0x01Systems ISO/IEC 14496-1   a
    0x02Systems ISO/IEC 14496-1   b
    0x03Interaction Stream
    0x04Systems ISO/IEC 14496-1 Extended BIFS Configuration   c
    0x05Systems ISO/IEC 14496-1 AFX  d
    0x06reserved for ISO use
    0x07reserved for ISO use
    0x08-0x1Freserved for ISO use
    0x20Visual ISO/IEC 14496-2   e
    0x21-0x3Freserved for ISO use
    0x40Audio ISO/IEC 14496-3   f
    0x41-0x5Freserved for ISO use
    0x60Visual ISO/IEC 13818-2 Simple Profile
    0x61Visual ISO/IEC 13818-2 Main Profile
    0x62Visual ISO/IEC 13818-2 SNR Profile
    0x63Visual ISO/IEC 13818-2 Spatial Profile
    0x64Visual ISO/IEC 13818-2 High Profile
    0x65Visual ISO/IEC 13818-2 422 Profile
    0x66Audio ISO/IEC 13818-7 Main Profile
    0x67Audio ISO/IEC 13818-7 LowComplexity Profile
    0x68Audio ISO/IEC 13818-7 Scaleable Sampling Rate Profile
    0x69Audio ISO/IEC 13818-3
    0x6AVisual ISO/IEC 11172-2
    0x6BAudio ISO/IEC 11172-3
    0x6CVisual ISO/IEC 10918-1
    0x6D - 0xBFreserved for ISO use
    0xC0 - 0xFEuser private
    0xFFno object type specified g
===============================================================

=======================ADTS header =================================
    Header consists of 7 or 9 bytes (with or without CRC).
    . 12 bits of syncword 0xFFF, all bits must be 1
    . 1 bit of field ID. 0 for MPEG-4, 1 for MPEG-2
    . 2 bits of MPEG layer. If you send AAC in MPEG-TS, set to 0
    . 1 bit of protection absense. Warning, set to 1 if there is no CRC and 0 if there is CRC
    . 2 bits of profile code. The MPEG-4 Audio Object Type minus 1
    . 4 bits of sample rate code. MPEG-4 Sampling Frequency Index (15 is not allowed)
    . 1 bit of private stream. Set to 0
    . 3 bits of channels code. MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
    . 1 bit of originality. Set to 0
    . 1 bit of home. Set to 0
    . 1 bit of copyrighted stream. Ignore it on read and set to 0
    . 1 bit of copyright start. Set to 0
    . 13 bits of frame length. This value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame)
    . 11 bits of some data. unknown yet
    . 2 bits of frames count in one packet. Set to 0
    . aac frame going.
===================================================================
*/


#endif

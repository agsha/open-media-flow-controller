#ifndef __PRE_PROC_TYPES_H__
#define __PRE_PROC_TYPES_H__

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#pragma pack(push, 1)

    typedef struct tag_uint24 {
	uint8_t b0, b1, b2;
    }uint24_t;

    typedef struct tag_io_hanlders {
	void* (*ioh_open)(char *, const char*, size_t);
	size_t (*ioh_tell)(void*);
	size_t (*ioh_seek)(void *, size_t, size_t);
	size_t (*ioh_read)(void *, size_t, size_t, void*);
	void (*ioh_close)(void *);
	size_t (*ioh_write)(void *, size_t, size_t, void*);
    }io_handlers_t;

    /**
     * \struct - a offset, length pair 
     */
    typedef struct _tag_vpe_ol {
	size_t offset;
	size_t length;
    } vpe_ol_t;

    /**
     * \struct - a offset, length pair 
     */
    typedef struct _tag_vpe_olt {
	size_t offset;
	size_t length;
	uint64_t time;
    } vpe_olt_t;

#pragma pack(pop)

#define VPE_MPROPS_VCODEC_AVC 1
#define VPE_MPROPS_VCODEC_VP6 2
#define VPE_MPROPS_VCODEC_MPEG4  3
#define VPE_MPROPS_VCODEC_H263   4
#define VPE_MPROPS_VCODEC_VP8    5
#define VPE_MPROPS_VCODEC_UNKNOWN 6

#define VPE_MPROPS_ACODEC_AAC 1
#define VPE_MPROPS_ACODEC_MP3 2
#define VPE_MPROPS_ACODEC_VORBIS  3
#define VPE_MPROPS_ACODEC_UNKNOWN 4

#  define nkn_vpe_swap_uint16(x) ((uint16_t)((((x) & 0x00FFU) << 8) |	\
					     (((x) & 0xFF00U) >> 8)))

#  define nkn_vpe_swap_int16(x) ((int16_t)((((x) & 0x00FF) << 8) |	\
					   (((x) & 0xFF00) >> 8)))

#  define nkn_vpe_swap_uint32(x) ((uint32_t)((((x) & 0x000000FFU) << 24) | \
					     (((x) & 0x0000FF00U) << 8)  | \
					     (((x) & 0x00FF0000U) >> 8)  | \
					     (((x) & 0xFF000000U) >> 24)))

#  define nkn_vpe_swap_int32(x) ((int32_t)((((x) & 0x000000FFU) << 23) | \
					   (((x) & 0x0000FF00U) << 8)  | \
					   (((x) & 0x00FF0000U) >> 8)  | \
					   (((x) & 0x7F000000U) >> 23)))

#  define nkn_vpe_swap_uint64(x) ((uint64_t)((((x) & 0x00000000000000FFUL) << 56)  | \
					     (((x) & 0x000000000000FF00UL) << 40)  | \
					     (((x) & 0x0000000000FF0000UL) << 24)  | \
					     (((x) & 0x00000000FF000000UL) << 8)   | \
					     (((x) & 0x000000FF00000000UL) >> 8)   | \
					     (((x) & 0x0000FF0000000000UL) >> 24)  | \
					     (((x) & 0x00FF000000000000UL) >> 40)  | \
					     (((x) & 0xFF00000000000000UL) >> 56))) 

#  define nkn_vpe_convert_uint24_to_uint32(x) ((uint32_t)(((x).b0 << 16) | \
							  ((x).b1 << 8) | (x).b2))

    uint24_t nkn_vpe_convert_uint32_to_uint24(uint32_t l);

#ifdef __cplusplus
}
#endif 

#endif 

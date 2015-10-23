#ifndef __RTP_AVC_READ__
#define __RTP_AVC_READ__

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define SINGLE_NAL 1
#define FU_A 2
#define STAP_A 3

#define FU_HEADER 1
#define FU_IN 1
#define STAP_HEADER_SIZE 1

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct params1{
	uint8_t length;
	uint8_t* param_set;
    }param_sets_t;

    typedef struct avcconf{
	uint8_t version;
	uint8_t AVCProfileIndication;
	uint8_t profile_compatibility;
	uint8_t AVCLevelIndication;
	uint8_t NALUnitLength;
	uint8_t numOfSequenceParameterSets;
	uint8_t numOfPictureParameterSets;
	param_sets_t* sps;
	param_sets_t* pps;
    }AVC_conf_Record_t;

    typedef struct somename{
	uint8_t F;
	uint8_t NRI;
	uint8_t type;
    }AVC_NAL_octet;

    typedef struct fua_hdr{
	uint8_t S;
	uint8_t E;
	uint8_t R;
	uint8_t type;
    }AVC_FUA;

    typedef struct fua{
	AVC_NAL_octet fu_indicator;
	AVC_FUA fu_header;
    }AVC_FU_A;

    int32_t get_nal_size( uint8_t*,uint8_t);
    int32_t get_num_NALs_AU(uint8_t* ,uint8_t ,int32_t);



#ifdef __cplusplus
}
#endif
#pragma pack(pop)

#endif

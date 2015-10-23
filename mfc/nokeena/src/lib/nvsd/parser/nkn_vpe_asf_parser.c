#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "nkn_vpe_asf.h"
#include "nkn_vpe_asf_parser.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_bitio.h"
#include "nkn_memalloc.h"


/* initial header object parser */
nkn_vpe_asf_header_obj_t*
nkn_vpe_asf_initial_header_obj(uint8_t* p_data,
			       uint64_t off,
			       uint64_t datasize)
{
    nkn_vpe_asf_header_obj_t* p_asf_header_obj;
    p_asf_header_obj = NULL;
    p_asf_header_obj = (nkn_vpe_asf_header_obj_t*) \
	nkn_calloc_type(1,
			sizeof(nkn_vpe_asf_header_obj_t),
			mod_vpe_asf_header_obj_t);
    if (!p_asf_header_obj) {
	return NULL;
    }

    p_asf_header_obj->pos = off;
    p_asf_header_obj->data = p_data;
    p_asf_header_obj->datasize = datasize;
    return p_asf_header_obj;
}

/* initial file propertiy object parser*/
nkn_vpe_asf_file_prop_obj_t*
nkn_vpe_asf_initial_file_prop_obj(uint8_t* p_data,
				  uint64_t off,
				  uint64_t datasize)
{

    nkn_vpe_asf_file_prop_obj_t* p_asf_file_prop_obj;
    p_asf_file_prop_obj = NULL;
    p_asf_file_prop_obj = (nkn_vpe_asf_file_prop_obj_t*) \
	nkn_calloc_type(1,
			sizeof(nkn_vpe_asf_file_prop_obj_t),
			mod_vpe_asf_file_prop_obj_t);
    if (!p_asf_file_prop_obj) {
	return NULL;
    }
    p_asf_file_prop_obj->pos = off;
    p_asf_file_prop_obj->data = p_data;
    p_asf_file_prop_obj->datasize = datasize;
    return p_asf_file_prop_obj;
}

/* ASF free parser*/
int
nkn_vpe_asf_clean_header_obj(nkn_vpe_asf_header_obj_t* p_asf_header_obj)
{
    if (p_asf_header_obj) {
	if (p_asf_header_obj->p_asf_file_prop_obj) {
	    nkn_vpe_asf_clean_file_prop_obj(p_asf_header_obj->p_asf_file_prop_obj);
	}
	free(p_asf_header_obj);
    }
    return 0;
}

int
nkn_vpe_asf_clean_file_prop_obj(nkn_vpe_asf_file_prop_obj_t* p_asf_file_prop_obj)
{
    if (p_asf_file_prop_obj) {
	free(p_asf_file_prop_obj);
    }
    return 0;
}

/*
 * check the ASF guid for ASF file type detection
 * return 0:ASF, 1: not ASF
 */
int
nkn_vpe_asf_header_guid_check(uint8_t* p_data)
{
    if (!memcmp(p_data, guid_asf_header, ASF_HD_OBJ_ID_LEN)) {
	return 0;
    }
    return 1;
}


/*
 * find the size and offset of Header Object
 * the offset will be zero. when we call this function, the error handle
 * will be different from others
 * the size should not be zeros.
 */
int
nkn_vpe_asf_header_obj_pos(uint8_t* p_data,
		       uint64_t datasize,
		       vpe_ol_t* ol)
{
    int rv;
    uint64_t bytes_passed;
    uint64_t asf_header_obj_size;
    ol->offset = 0;
    ol->length = 0;

    /* check asf guid for header object */
    rv = nkn_vpe_asf_header_guid_check(p_data);
    if (rv != 0) { /* not ASF */
	return rv;
    }
    bytes_passed = ASF_HD_OBJ_ID_LEN;

    p_data += bytes_passed;

    memcpy(&asf_header_obj_size, p_data, ASF_HD_OBJ_SIZE_LEN);
    //asf_header_obj_size = nkn_vpe_swap_uint64(asf_header_obj_size);
    //asf_header_obj_size += 24;
    ol->length = asf_header_obj_size;
    return 0;
}



/* parse the content ASF Header Object */
int
nkn_vpe_asf_header_obj_parser(nkn_vpe_asf_header_obj_t* p_asf_header_obj)
{
    uint8_t* p_data;
    uint64_t bytes_passed;
    int rv;
    uint8_t tmp_guid[16]; /* guid is 128 bits*/
    uint64_t asf_obj_size;
    p_data = p_asf_header_obj->data + p_asf_header_obj->pos;
    bytes_passed = 0;

    /* check asf guid for header object */
    rv = nkn_vpe_asf_header_guid_check(p_data);
    if (rv != 0) { /* not ASF */
        return rv;
    }
    bytes_passed = ASF_HD_OBJ_ID_LEN;
    bytes_passed += (ASF_HD_OBJ_SIZE_LEN + ASF_HD_NUM_OBJ_LEN
		     + ASF_HD_RESERVED1_LEN + ASF_HD_RESERVED2_LEN);

    p_data += bytes_passed;

    do {
	//	printf("bytes_passed %d\n", (int)bytes_passed);
	memcpy(&tmp_guid, p_data, ASF_OBJ_GUID_LEN);
	//	for(int i=0; i< 16; i++) {
	//	    printf("%x ", tmp_guid[i]);
	//	}
	//	printf("\n");

	memcpy(&asf_obj_size, p_data + ASF_OBJ_GUID_LEN, ASF_OBJ_SIZE_LEN);
	//asf_obj_size = nkn_vpe_swap_uint64(asf_obj_size);

	if (!memcmp(tmp_guid, guid_asf_file_prop_header, ASF_OBJ_GUID_LEN)) {
	    /* initial asf_file_prop_obj */
	    p_asf_header_obj->p_asf_file_prop_obj = \
		nkn_vpe_asf_initial_file_prop_obj(p_data, 0, asf_obj_size);
	    if (!p_asf_header_obj->p_asf_file_prop_obj) {
		rv = 2;
		return rv;
	    }
	    /* parse asf_file_prop_obj */
	    nkn_vpe_asf_file_prop_obj_parser(p_asf_header_obj->p_asf_file_prop_obj);

	    bytes_passed += asf_obj_size;
            p_data += asf_obj_size;
	} else {
	    /* bypass other obj*/
	    bytes_passed += asf_obj_size;
	    p_data += asf_obj_size;
	}

    } while (bytes_passed < p_asf_header_obj->datasize);

    return 0;
}

/* parse the content ASF File Properties Object  */
int
nkn_vpe_asf_file_prop_obj_parser(nkn_vpe_asf_file_prop_obj_t* p_asf_file_prop_obj)
{
    uint8_t* p_data;
    uint64_t bytes_passed;
    int rv;
    uint64_t tmp_duration;
    uint32_t tmp_maxbitrate;
    uint8_t lsb_flag;
    p_data = p_asf_file_prop_obj->data + p_asf_file_prop_obj->pos;
    bytes_passed = ASF_FILE_PROP_HD_OBJ_ID_LEN + ASF_FILE_PROP_HD_OBJ_SIZE_LEN
	+ ASF_FILE_PROP_HD_FILE_ID_LEN + ASF_FILE_PROP_HD_FILE_SIZE_LEN
	+ ASF_FILE_PROP_HD_CREATE_DATA_LEN + ASF_FILE_PROP_HD_DATA_PAK_CNT_LEN;



    memcpy(&tmp_duration, p_data + bytes_passed, ASF_FILE_PROP_HD_PLAY_DUR_LEN);
    p_asf_file_prop_obj->play_duration = tmp_duration; //nkn_vpe_swap_uint64(tmp_duration);

    bytes_passed += ASF_FILE_PROP_HD_PLAY_DUR_LEN + ASF_FILE_PROP_HD_SEND_DUR_LEN;

    memcpy(&tmp_duration, p_data + bytes_passed, ASF_FILE_PROP_HD_PREROLLLEN);
    p_asf_file_prop_obj->preroll = tmp_duration; //nkn_vpe_swap_uint64(tmp_duration);
    bytes_passed += ASF_FILE_PROP_HD_PREROLLLEN;

    lsb_flag = p_data[bytes_passed];
    bytes_passed += (ASF_FILE_PROP_HD_FLAG_LEN + ASF_FILE_PROP_HD_MIN_DATA_PAK_SIZE_LEN
		     + ASF_FILE_PROP_HD_MAX_DATA_PAK_SIZE_LEN);
    memcpy(&tmp_maxbitrate, p_data + bytes_passed, ASF_FILE_PROP_HD_MAX_BITRATE_LEN);
    p_asf_file_prop_obj->max_bitrate = tmp_maxbitrate;

    if (lsb_flag & 0x01 ) { /* broadcasting flat */
	p_asf_file_prop_obj->play_duration = 0;
	p_asf_file_prop_obj->preroll = 0;
	p_asf_file_prop_obj->max_bitrate = 0;
    }

    return 0;
}

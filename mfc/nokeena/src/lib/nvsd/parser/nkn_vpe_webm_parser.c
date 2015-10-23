#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "nkn_vpe_webm.h"
#include "nkn_vpe_webm_parser.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_bitio.h"
#include "nkn_memalloc.h"


static int
ebml_data_float_parser(uint8_t* p_data, uint64_t datasize, double* val);
static int
ebml_data_string_parser(uint8_t* p_data, uint64_t datasize, char* str);
static int
copy_ebml_buf_to_val(uint8_t *p_data, uint64_t *p_elem_val, uint64_t elem_len);
static int
conv_ebml_buf_to_val(uint8_t* p_data, uint64_t* p_elem_val, uint64_t elem_len);
static int
calc_ebml_elem_ID_length(uint8_t* p_data, uint64_t* p_elem_ID_len);
static int
calc_ebml_elem_datasize_length(uint8_t* p_data, uint64_t* p_elem_datasize_len);
static int
nkn_vpe_ebml_elem_ID_parser(uint8_t* p_data, uint64_t* p_elem_ID_val,
			    uint64_t* p_elem_ID_len);

/* webm initial parser */
nkn_vpe_webm_segment_t*
nkn_vpe_webm_init_segment(uint8_t* p_data, uint64_t off,
			  uint64_t headsize, uint64_t datasize)
{
    nkn_vpe_webm_segment_t* p_webm_segment;
    p_webm_segment = NULL;
    p_webm_segment = (nkn_vpe_webm_segment_t*) \
			nkn_calloc_type(1,
					sizeof(nkn_vpe_webm_segment_t),
					mod_vpe_webm_segment_t);
    if (!p_webm_segment) {
	return NULL;
    }
    p_webm_segment->pos = off;
    p_webm_segment->data = p_data;
    p_webm_segment->headsize = headsize;
    p_webm_segment->datasize = datasize;
    return p_webm_segment;
}


nkn_vpe_webm_segment_info_t*
nkn_vpe_webm_init_segment_info(uint8_t* p_data, uint64_t off,
			       uint64_t headsize, uint64_t datasize)
{
    nkn_vpe_webm_segment_info_t* p_webm_segment_info;
    p_webm_segment_info = NULL;
    p_webm_segment_info = (nkn_vpe_webm_segment_info_t*)
				nkn_calloc_type(1,
						sizeof(nkn_vpe_webm_segment_info_t),
						mod_vpe_webm_segment_info_t);

    if (!p_webm_segment_info) {
	return NULL;
    }
    p_webm_segment_info->pos = off;
    p_webm_segment_info->data = p_data;
    p_webm_segment_info->headsize = headsize;
    p_webm_segment_info->datasize = datasize;
    return p_webm_segment_info;
}

/*
 * We will init with 2 traks first. If we detect more traks,
 * we will alloc more.
 */
nkn_vpe_webm_traks_t*
nkn_vpe_webm_init_traks(uint8_t* p_data, uint64_t off,
			uint64_t headsize, uint64_t datasize)
{
    nkn_vpe_webm_traks_t* p_webm_traks;
    p_webm_traks = NULL;
    p_webm_traks = (nkn_vpe_webm_traks_t*)
			nkn_calloc_type(1,
					sizeof(nkn_vpe_webm_traks_t),
					mod_vpe_webm_traks_t);
    if (!p_webm_traks) {
	return NULL;
    }
    p_webm_traks->pos = off;
    p_webm_traks->data = p_data;
    p_webm_traks->headsize = headsize;
    p_webm_traks->datasize = datasize;
    return p_webm_traks;
}

nkn_vpe_webm_trak_entry_t*
nkn_vpe_webm_init_trak_entry(uint8_t* p_data, uint64_t off,
			     uint64_t headsize, uint64_t datasize)
{
    nkn_vpe_webm_trak_entry_t* p_webm_trak_entry;
    p_webm_trak_entry = NULL;
    p_webm_trak_entry = (nkn_vpe_webm_trak_entry_t*)
			nkn_calloc_type(1,
					sizeof(nkn_vpe_webm_trak_entry_t),
					mod_vpe_webm_trak_entry);
    if (!p_webm_trak_entry) {
	return NULL;;
    }
    p_webm_trak_entry->pos = off;
    p_webm_trak_entry->data = p_data;
    p_webm_trak_entry->headsize = headsize;
    p_webm_trak_entry->datasize = datasize;
    return p_webm_trak_entry;
}


int
nkn_vpe_webm_clean_segment(nkn_vpe_webm_segment_t* p_webm_segment)
{

    if(p_webm_segment) {
	/* free webm_segment_info */
	if (p_webm_segment->p_webm_segment_info)
	    free(p_webm_segment->p_webm_segment_info);
	/* free p_webm_traks*/
	if (p_webm_segment->p_webm_traks)
	    nkn_vpe_webm_clean_traks(p_webm_segment->p_webm_traks);
	/* free p_webm_segment itself*/
	free(p_webm_segment);
    }
    return 0;
}

int
nkn_vpe_webm_clean_trak_entry(nkn_vpe_webm_trak_entry_t* p_webm_trak_entry)
{
    if (p_webm_trak_entry) {
	if(p_webm_trak_entry->p_webm_codec_ID) {
	    free(p_webm_trak_entry->p_webm_codec_ID);
	}
	free(p_webm_trak_entry);
    }
    return 0;
}

int
nkn_vpe_webm_clean_traks(nkn_vpe_webm_traks_t* p_webm_traks)
{
    int i;
    if (p_webm_traks) {
	for(i = 0; i < p_webm_traks->webm_num_traks; i++) {
	    nkn_vpe_webm_clean_trak_entry(
				p_webm_traks->p_webm_trak_entry[i]);
	}
	free(p_webm_traks);
    }
    return 0;
}


/*
 * webm header parser
 * It will check the ebml-ID for EBML  and "webm" string
 */
int
nkn_vpe_webm_head_parser(uint8_t *p_data,
			 uint64_t *p_webm_head_size)
{
    char webm_ebml_doc_type[] = {'w','e','b','m'};
    int rv;
    uint64_t ebml_hd_ID_val;
    uint64_t ebml_hd_datasize_val;
    uint64_t ebml_hd_size;
    int memcmp_len;
    rv = nkn_vpe_ebml_parser(p_data, &ebml_hd_ID_val,
			     &ebml_hd_datasize_val,
			     &ebml_hd_size);
    if (rv != 0) {
	return rv;
    }
    *p_webm_head_size = ebml_hd_size + ebml_hd_datasize_val;
    if (rv == EBML_INVALID_LENGTH) {
        return rv;
    }

    if (ebml_hd_ID_val != EBML) {
	/* not ebml header, just return*/
	rv = 1;
	return rv;
    }

    memcmp_len = sizeof(webm_ebml_doc_type);
    for (uint64_t i = ebml_hd_size; \
	 i < *p_webm_head_size - memcmp_len; i++) {
	if (!memcmp(p_data+i, webm_ebml_doc_type, memcmp_len)) {
	    /* doc_type is webm */
	    rv = 0;
	    return rv;
	}
    }

    rv = 2;/* It is ebml header, but it is not webm */
    return rv;
}

#if 0
/*
 * webm parse level 0 element
 * level 0: webm header and segment
 */
int
nkn_vpe_webm_level_0_elem_parser(uint8_t* p_data, uint64_t datasize,
			nkn_vpe_webm_segment_t* p_webm_segment)
{
    uint64_t webm_head_size;
    uint64_t ebml_head_size;
    uint64_t elem_ID_val;
    uint64_t elem_datasize_val;
    uint64_t avail_datasize;
    int rv;
    /* parse webm header*/
    rv = nkn_vpe_webm_head_parser(p_data,
				&webm_head_size);
    if (rv != 0) {
	return rv;
    }
    p_data = p_data + webm_head_size;
    nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
			&ebml_head_size);
    if (elem_ID_val == WEBM_SEGMENT) {
	/* parse Segment*/
	/* 1. allocate Segment*/
	avail_datasize =
		(datasize - webm_head_size - ebml_head_size) < elem_datasize_val ?
		(datasize - webm_head_size - ebml_head_size) : elem_datasize_val;
	nkn_vpe_webm_init_segment(p_data, webm_head_size,
				  ebml_head_size, avail_datasize,
				  p_webm_segment);

	/* 2. parse Segment*/
	nkn_vpe_webm_segment_parser(p_webm_segment);
    } else {
	/* cannot find Segment after the webm header */
	/* This is not a valid webm */
	rv = 3;
	return rv;
    }
    return rv;
}
#endif

/*
 * WebM doesnot have the moov box like MP4
 * The metadata of WebM is at the start part of Segment.
 * For details, please go to:
 * http://www.matroska.org/technical/order/index.html#cues_front
 * There are three possible arrangement of WebM metadata
 * But we only need to Find the start pos of Segment and the end pos of
 * Tracks, which is in Segent
 * We will return pos of Segment, and the length till the end of
 * Tracks
 *
 * This function is used in the WAIT_FOR_MORE_DATA loop
 * to decide the header_size
 *
 */
int
nkn_vpe_webm_segment_pos(uint8_t* p_data, uint64_t datasize,
			vpe_ol_t *ol)
{
    uint64_t webm_head_size;
    //    uint64_t cur_pos;
    uint64_t bytes_passed;
    uint64_t elem_ID_val, elem_datasize_val, ebml_head_size;
    int rv;

    bytes_passed = 0;

    ol->offset = 0;
    ol->length = 0;
    rv = nkn_vpe_webm_head_parser(p_data,
				  &webm_head_size);
    if (rv != 0) {
        return rv;
    }

    bytes_passed += webm_head_size;
    p_data += bytes_passed;
    rv = nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
			     &ebml_head_size);
    if (rv != 0) {
	return rv;
    }
    if (elem_ID_val == WEBM_SEGMENT) {
	/* continue to find pos and offset of segment info and track*/
	//	avail_datasize = datasize - webm_head_size - ebml_head_size;
	ol->offset = webm_head_size;
	p_data += ebml_head_size;
	bytes_passed += ebml_head_size;
	do {
	    rv = nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
				     &ebml_head_size);
	    if (rv != 0) {
		return rv;
	    }
	    switch (elem_ID_val) {
		case WEBM_SEGMENT_INFO:
 		    /* parse Segment Information pos and length */
		    //printf("Found Segment Info   ");
		    //printf("%lu\n",bytes_passed);
		    p_data += ebml_head_size + elem_datasize_val;
		    bytes_passed += ebml_head_size + elem_datasize_val;
                    break;

		case WEBM_TRACKS:
		    /* parse Track pos and length*/
		    //printf("Found Tracks   ");
		    //printf("%lu\n",bytes_passed);
		    p_data += ebml_head_size + elem_datasize_val;
		    bytes_passed += ebml_head_size + elem_datasize_val;
		    ol->length = bytes_passed - ol->offset;
		    break;
		default:
		    /*
		     * Other elements in Segment will be skip
		     * We will not parse other elements
		     */
		    p_data += ebml_head_size + elem_datasize_val;
		    bytes_passed += ebml_head_size + elem_datasize_val;
		    break;
	    } /* end of switch (elem_ID_val) */
	} while (bytes_passed < datasize);
    } else {
        /* cannot find Segment after the webm header */
        /* This is not a valid webm */
        rv = 3;
        return rv;
    }

    return rv;
}

/* webm parse Segment */
int
nkn_vpe_webm_segment_parser(nkn_vpe_webm_segment_t* p_webm_segment)
{

    /*
     * Meta Seek Info will contain the position of other elements.
     * But from the muxer Guildlines for webm, only pos of cues is mandatory
     * for Meta Seek Info.
     * For the AFR, only the duration and codectype is needed. As a result,
     * we will only parse Segment Info and Track (both of audio and video)
     * The pos of Segment Info or Track is not mandatory in Meta Seek Info.
     *
     * As a result, we will not parse Meta Seek Info, but we will go through
     * elements one by one (Segment Info and Track  to parse, others just skip)
     */
    uint8_t* p_data;
    uint64_t bytes_passed;
    int rv;
    p_data = p_webm_segment->data + p_webm_segment->pos;
    /* Skip the Segment header */
    p_data = p_data + p_webm_segment->headsize;
    bytes_passed = p_webm_segment->headsize;

    do {
    /* Parse the ebml head, and find out elem-ID and datasize */
	uint64_t elem_ID_val;
        uint64_t elem_datasize_val;
        uint64_t ebml_head_size;

	rv = nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
				 &ebml_head_size);
	if (rv != 0) {
	    return rv; /* fail to parse */
	}
	switch (elem_ID_val) {
	    case WEBM_SEGMENT_INFO:
		/* parse Segment Information */
		/* 1. alloc the webm_segment_info*/
		//printf("Found Segment Info   ");
		//printf("%lu\n",bytes_passed);

		p_webm_segment->p_webm_segment_info = \
			nkn_vpe_webm_init_segment_info(p_data, 0,
						       ebml_head_size,
						       elem_datasize_val);
		if (!p_webm_segment->p_webm_segment_info) {
		    /* fail to alloc */
		    rv = 1;
		    return rv;
		}
		/* 2. start to parse Segment Info and obtain the duraton*/
		rv = nkn_vpe_webm_segment_info_parser( \
			p_webm_segment->p_webm_segment_info);
		bytes_passed += ebml_head_size + elem_datasize_val;
		p_data += ebml_head_size + elem_datasize_val;
		break;
	    case WEBM_TRACKS:
		/* parse Track */
		/* 1. alloc the webm_traks */
		//printf("Found Tracks       ");
		//printf("%lu\n",bytes_passed);

		p_webm_segment->p_webm_traks = \
			nkn_vpe_webm_init_traks(p_data, 0,
						ebml_head_size,
						elem_datasize_val);
		if (p_webm_segment->p_webm_traks == NULL) {
		    /* fail to alloc */
		    rv =1;
		    return rv;
		}
		/* 2. start to parse traks and traks entry's for codec info*/
		rv = nkn_vpe_webm_traks_parser(p_webm_segment->p_webm_traks);
		if (rv != 0) {
		    return rv; /* fail to parse traks */
		}
                bytes_passed += ebml_head_size + elem_datasize_val;
                p_data += ebml_head_size + elem_datasize_val;
		break;
	    default:
		/*
		 * Other elements in Segment will be skip
	         * We will not parse other elements
		 */
                bytes_passed += ebml_head_size + elem_datasize_val;
                p_data += ebml_head_size + elem_datasize_val;
	    break;
	} /* end of switch (elem_ID_val) */
    } while (bytes_passed < (p_webm_segment->datasize + p_webm_segment->headsize));

    return 0;
}

/* webm parse Segment information*/
int
nkn_vpe_webm_segment_info_parser(nkn_vpe_webm_segment_info_t*
						p_webm_segment_info)
{
    uint8_t* p_data;
    //    uint64_t cur_pos;
    uint64_t bytes_passed;
    int found, rv;
    found = 0;
    /* go to Segment_info */
    p_data = p_webm_segment_info->data + p_webm_segment_info->pos;
    /* skip the Segment_info header*/
    p_data = p_data + p_webm_segment_info->headsize;
    bytes_passed =  p_webm_segment_info->headsize;
    do{
	/* parse elements in Segment information*/
	uint64_t elem_ID_val;
	uint64_t elem_datasize_val;
	uint64_t ebml_head_size;
	rv = nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
				 &ebml_head_size);
	if (rv != 0) {
	    return rv;
	}
	if (elem_ID_val == WEBM_DURATION) {
	    /* parse duration */
	    //printf("Found Duration   ");
	    //printf("%lu\n",bytes_passed);

	    p_data += ebml_head_size;
	    ebml_data_float_parser(p_data, elem_datasize_val,
				&(p_webm_segment_info->webm_duration));
	    p_data += elem_datasize_val;
	    bytes_passed += ebml_head_size + elem_datasize_val;
	    found ++;
	    if (found == 2) {
		return 0; /* already found, jump out the parse*/
	    }
	} else if (elem_ID_val == WEBM_TIMECODE_SCALE) {
	    /* parse timecode scale*/
	    //printf("Found timecode scale");
	    //printf ("%lu\n", bytes_passed);
	    p_data += ebml_head_size;
	    copy_ebml_buf_to_val(p_data,
				 &(p_webm_segment_info->webm_timescale),
				 elem_datasize_val);
            p_data += elem_datasize_val;
            bytes_passed += ebml_head_size + elem_datasize_val;
            found ++;
            if (found == 2) {
                return 0; /* already found, jump out the parse*/
            }
	} else {
	    /* other elements, just skip */
            bytes_passed += ebml_head_size + elem_datasize_val;
            p_data += ebml_head_size + elem_datasize_val;
	}
    } while (bytes_passed < \
	     (p_webm_segment_info->datasize + p_webm_segment_info->headsize));

    /* if we found duration, return 0, else return 1*/
    return found ? 0 : 1;
}

/* webm parse Track */
int
nkn_vpe_webm_traks_parser(nkn_vpe_webm_traks_t* p_webm_traks)
{
    uint8_t* p_data;
    uint64_t bytes_passed;
    int rv;
    //    uint64_t cur_pos;
    p_webm_traks->webm_num_traks = 0; /* initial to 0 */
    /* go to Tracks */
    p_data = p_webm_traks->data + p_webm_traks->pos;
    /*skip the Track header*/
    p_data = p_data + p_webm_traks->headsize;
    bytes_passed = p_webm_traks->headsize;
    do{
	/* parse track entry in Track*/
        uint64_t elem_ID_val;
        uint64_t elem_datasize_val;
        uint64_t ebml_head_size;
        rv = nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
				 &ebml_head_size);
	if (rv != 0) {
	    return rv;
	}
	if (elem_ID_val == WEBM_TRACK_ENTRY) {
	    /*
	     * found one track entry
	     * 1. alloc mem for this track entry
	     * 2. parse this track entry
	     * 3. update  p_webm_traks->webm_num_traks
	     */
	    //printf("Found Track entry   ");
	    //printf("%lu\n",bytes_passed);

	    p_webm_traks->p_webm_trak_entry[p_webm_traks->webm_num_traks] =  \
	    nkn_vpe_webm_init_trak_entry(p_data, 0,
					 ebml_head_size, elem_datasize_val);
	    if (p_webm_traks->p_webm_trak_entry[p_webm_traks->webm_num_traks] == NULL) {
		/* fail to alloc */
		return 1;
	    }
	    rv = nkn_vpe_webm_trak_entry_parser( \
		p_webm_traks->p_webm_trak_entry[p_webm_traks->webm_num_traks]);
	    if (rv != 0) {
		/* fail to parse trak_entry */
		return rv;
	    }
	    p_webm_traks->webm_num_traks ++;
	    bytes_passed += ebml_head_size + elem_datasize_val;
            p_data += ebml_head_size + elem_datasize_val;
	} else {
	    /* other elements, just skip */
            bytes_passed += ebml_head_size + elem_datasize_val;
            p_data += ebml_head_size + elem_datasize_val;
	}
    } while (bytes_passed < (p_webm_traks->datasize + p_webm_traks->headsize));
    return 0;
}

/* webm parse Track entry*/
int
nkn_vpe_webm_trak_entry_parser(nkn_vpe_webm_trak_entry_t* p_webm_trak_entry)
{
    uint8_t* p_data;
    //    uint64_t cur_pos;
    uint64_t bytes_passed;
    int found;
    int rv;
    found = 0;
    p_data = p_webm_trak_entry->data + p_webm_trak_entry->pos;
    /* skip the track entry header*/
    p_data = p_data + p_webm_trak_entry->headsize;
    bytes_passed = p_webm_trak_entry->headsize;

    do {
	/* parse elements in track entry */
        uint64_t elem_ID_val;
        uint64_t elem_datasize_val;
        uint64_t ebml_head_size;
        rv = nkn_vpe_ebml_parser(p_data, &elem_ID_val, &elem_datasize_val,
				 &ebml_head_size);
	if (rv != 0) {
	    return rv;
	}
        if (elem_ID_val == WEBM_CODEC_ID) {
            /* parse codec_ID */
            p_data += ebml_head_size;
	    p_webm_trak_entry->p_webm_codec_ID = /* alloc mem for codec_ID */
				(char*) nkn_calloc_type(1,
						elem_datasize_val,
						mod_vpe_webm_mem);
            ebml_data_string_parser(p_data, elem_datasize_val,
				    p_webm_trak_entry->p_webm_codec_ID);
            p_data += elem_datasize_val;
            bytes_passed += ebml_head_size + elem_datasize_val;
	    found ++;
	    if (found == 2) {
		return 0;
	    }
        } else if (elem_ID_val == WEBM_TRACK_TYPE) {
            /* parse codec_type */
            p_data += ebml_head_size;
	    rv = conv_ebml_buf_to_val(p_data, &(p_webm_trak_entry->webm_trak_type),
				      elem_datasize_val);
	    if (rv != 0) {
		/* fail to convert */
		return rv;
	    }
            p_data += elem_datasize_val;
            bytes_passed += ebml_head_size + elem_datasize_val;
	    found ++;
	    if (found == 2) {
		return 0;
	    }
	} else {
            /* other elements, just skip */
            bytes_passed += ebml_head_size + elem_datasize_val;
            p_data += ebml_head_size + elem_datasize_val;
        }
    } while (bytes_passed < \
	     (p_webm_trak_entry->datasize + p_webm_trak_entry->headsize));

    return found == 2 ? 0 : 1;
}

/* read the float data*/
static int
ebml_data_float_parser(uint8_t *p_data, uint64_t datasize, double* val)
{
    if (datasize == 0) {
	*val = 0.0;
    } else if (datasize == 4) {
	/* read 4 bytes from p_data */
	ebml_float tmp;
	memcpy(&tmp.ui32, p_data, datasize);
	tmp.ui32 = nkn_vpe_swap_uint32(tmp.ui32);
	*val = tmp.f;
    } else if (datasize == 8) {
	/* read 8 bytes from p_data */
	ebml_double tmp;
	memcpy(&tmp.ui64,p_data, datasize);
	tmp.ui64 = nkn_vpe_swap_uint64(tmp.ui64);
	*val = tmp.d;
    }
    return 0;
}

static int
ebml_data_string_parser(uint8_t *p_data, uint64_t datasize, char* str)
{
    if (datasize !=0) {
	memcpy(str, p_data,datasize);
    }
    return 0;
}

static int
copy_ebml_buf_to_val(uint8_t *p_data, uint64_t *p_elem_val, uint64_t elem_len)
{
    int rv = 0;
    *p_elem_val = 0;
    for (uint64_t i = 0; i < elem_len; i++ )
	*p_elem_val = (*p_elem_val << 8) + p_data[i];
    return rv;
}

/*
 *
 * The elem_len should not be EBML_INVALID_LENGTH
 * Need a check before call conv_buf_to_val;
 */
static int
conv_ebml_buf_to_val(uint8_t *p_data, uint64_t *p_elem_val, uint64_t elem_len)
{
    uint64_t mask;
    int rv = 0;

    //    uint64_t mask_table[] = {};
    *p_elem_val = 0;
    //we should  modify this
    for (uint64_t i = 0; i < elem_len; i++)
	*p_elem_val = (*p_elem_val << 8) + p_data[i];

    switch(elem_len) {
	case 1:
	    mask = 0x7F;
	    break;
	case 2:
	    mask = 0x3FFF;
	    break;
	case 3:
	    mask = 0x1FFFFF;
	    break;
	case 4:
	    mask = 0xFFFFFFF;
	    break;
	case 5:
	    mask = 0x7FFFFFFFF;
	    break;
	case 6:
	    mask = 0x3FFFFFFFFFF;
	    break;
	case 7:
	    mask = 0x1FFFFFFFFFFFF;
	    break;
	case 8:
	    mask = 0xFFFFFFFFFFFFFF;
	    break;
	default:
	    mask = 0;
	    rv = EBML_INVALID_LENGTH;
	    break;
    }

    *p_elem_val = *p_elem_val & mask;

    return rv;
}
/*
 * Calculate ebml elem_ID length
 * The leading bits of the EBML IDs are used to identify
 * the length of the ID.
 *
*/
static int
calc_ebml_elem_ID_length(uint8_t *p_data, uint64_t* p_elem_ID_len)
{
    uint8_t *p_one_byte;
    int rv = 0;
    p_one_byte = p_data;

    if (p_one_byte[0] & 0x80) {
	*p_elem_ID_len = 1;
    } else if (p_one_byte[0] & 0x40) {
	*p_elem_ID_len = 2;
    } else if (p_one_byte[0] & 0x20) {
	*p_elem_ID_len = 3;
    } else if (p_one_byte[0] & 0x10) {
	*p_elem_ID_len = 4;
    } else {
	*p_elem_ID_len = EBML_INVALID_LENGTH;
	rv = EBML_INVALID_LENGTH;
    }

    return rv;
};

/*
 * Calculate ebml elem_datasize length
 * The leading bits of the EBML IDs are used to identify
 * the length of the ID.
 *
 */
static int
calc_ebml_elem_datasize_length(uint8_t *p_data, uint64_t* p_elem_datasize_len)
{
    uint8_t *p_one_byte;
    int rv = 0;
    p_one_byte = p_data;

    if (p_one_byte[0] & 0x80) {
	*p_elem_datasize_len = 1;
    } else if (p_one_byte[0] & 0x40) {
	*p_elem_datasize_len = 2;
    } else if (p_one_byte[0] & 0x20) {
	*p_elem_datasize_len = 3;
    } else if (p_one_byte[0] & 0x10) {
	*p_elem_datasize_len = 4;
    } else if (p_one_byte[0] & 0x08) {
	*p_elem_datasize_len = 5;
    } else if (p_one_byte[0] & 0x04) {
	*p_elem_datasize_len = 6;
    } else if (p_one_byte[0] & 0x02) {
	*p_elem_datasize_len = 7;
    } else if (p_one_byte[0] & 0x01) {
	*p_elem_datasize_len = 8;
    }else {
	*p_elem_datasize_len = EBML_INVALID_LENGTH;
	rv = EBML_INVALID_LENGTH;
    }

    return rv;
};

/* ebml elem_ID parser*/
static int
nkn_vpe_ebml_elem_ID_parser(uint8_t* p_data, uint64_t* p_elem_ID_val,
			    uint64_t* p_elem_ID_len)
{
    int rv = 0;
    rv  = calc_ebml_elem_ID_length(p_data, p_elem_ID_len);
    if (rv == EBML_INVALID_LENGTH) {
	return rv;
    }
    rv = copy_ebml_buf_to_val(p_data, p_elem_ID_val, *p_elem_ID_len);
    return rv;
}


/* ebml datasize parser*/
static int
nkn_vpe_ebml_datasize_parser(uint8_t* p_data, uint64_t* p_elem_datasize_val,
			     uint64_t* p_elem_datasize_len)
{
    int rv = 0;
    rv  = calc_ebml_elem_datasize_length(p_data, p_elem_datasize_len);
    if (rv == EBML_INVALID_LENGTH) {
	return rv;
    }

    rv = conv_ebml_buf_to_val(p_data, p_elem_datasize_val,
				*p_elem_datasize_len);
    return rv;
}

/* ebml parser */
int
nkn_vpe_ebml_parser(uint8_t* p_data, uint64_t* p_elem_ID_val,
		    uint64_t* p_elem_datasize_val,
		    uint64_t* p_ebml_head_size)
{
    int rv = 0;
    uint64_t elem_ID_len, elem_datasize_len;
    /*ebml elem_ID parser*/
    rv = nkn_vpe_ebml_elem_ID_parser(p_data, p_elem_ID_val,
				&elem_ID_len);
    if (rv == EBML_INVALID_LENGTH) {
	return rv;
    }

    p_data = p_data + elem_ID_len;

    /*ebml datasize parser*/
    rv = nkn_vpe_ebml_datasize_parser(p_data, p_elem_datasize_val,
				&elem_datasize_len);
    if (rv == EBML_INVALID_LENGTH) {
	return rv;
    }

    *p_ebml_head_size = elem_ID_len + elem_datasize_len;

    return rv;
}

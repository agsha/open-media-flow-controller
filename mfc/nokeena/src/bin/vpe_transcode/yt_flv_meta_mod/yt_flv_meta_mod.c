#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

#include "nkn_vpe_bitio.h"
#include "nkn_vpe_flv_parser.h"
#include "nkn_memalloc.h"
#include "nkn_mem_counter_def.h"

#define FLV_BLK_SIZE (32*1024)

static int
check_arg_parm(char* in_file_name, char* out_file_name);

/*
 * BZ 8382
 * youtube FLV onmetadata injector
 * we use the yamdi to inject several onmetadata properties into onmetadata.
 * but the youtube FLV onmetadata is different from the yamdi injected one
 *
 * yamdi has onmetadata
 *       char   *metaDataCreator;
 *       uint8_t hasKeyFrames;
 *       uint8_t hasVideo;
 *       uint8_t hasAudio;
 *       uint8_t hasMetaData;
 *       uint8_t canSeekToEnd;
 *       double duration;
 *       double dataSize;
 *       double videoSize;
 *       double frameRate;
 *       double videoDataRate;
 *       double videoCodecId;
 *       double width;
 *       double height;
 *       double audioSize;
 *       double audioDataRate;
 *       double audioCodecId;
 *       double audioSampleRate;
 *       double audioSampleSize;
 *       uint8_t stereo;
 *       double fileSize;
 *       double lastTimestamp;
 *       double lastKeyFrameTimestamp;
 *       double lastKeyFrameLocation;
 *       double *filePositions; // these two are for the keyframes
 *       double *times;  // these two are for the keyframes
 *
 * youtube has onmetadata
 *        double duration;// for yt seek
 *        double startTime;// for yt seek
 *        double totalDuration;
 *        double width;
 *        double height;
 *        double videoDataRate;
 *        double audioDataRate;
 *        double totalDataRate;
 *        double frameRate;
 *        double byteLength;// for yt seek
 *        double canSeekOnTime;
 *        char   *sourceData;
 *        char   *purl;
 *        char   *pmsg;
 *        char   *httpHostHeader;
 *
 *
 *
 *  Compared with the above two, we can find that youtube onmetadata only use
 *  certain properities that yamdi injected.
 *  we need to read the below value from yamdi
 *  const char* yamdi_onmetadata[]= { "duration",
 *                                "width",
 *                                "height",
 *                                "videodatarate",
 *                                "audiodatarate",
 *                                "framerate",
 *                                "filesize" }; // is the bytelength in yt_onmetadata
 *  and they are used to generated the following values.
 *  const char* yt_onmetadata[]={ "onMetaData",
 *                            "duration",          // yamdi duration
 *                            "totalduration",     // yamdi duration
 *                            "width",             // yamdi width
 *                            "height",            // yamdi height
 *                            "videodatarate",     // yamdi videodatarate
 *                            "audiodatarate",     // yamdi audiodatarate
 *                            "totaldatarate",     // (videosize+audiosize+sizeof(ytonmetadata))/duration
 *                            "framerate",         // yamdi framerate
 *                            "bytelength",        // size of this new flv file
 *                            "canseekontime",    // true
 *                            "sourcedata",
 *                            "purl",
 *                            "pmsg",
 *                            "httphostheader" };
 *
 */



int32_t
main(int32_t argc, char *argv[])
{
    char *in_file_name, *out_file_name;
    size_t  bytes_remaining, file_size, pos_onmetadata_start, pos_onmetadata_end;
    FILE *fin = NULL;
    int rv;
    uint8_t TagType;
    uint24_t uint_temp_24;
    uint32_t TagDataSize;
    uint32_t timestamp_onmetadata, temp_previous_tag_size, previous_tag_size;
    int8_t found_onmetadata = 0;
    uint8_t *onmetadata_data_ori, *onmetadata_data_modi;
    uint8_t *buffer_before_onmetadata, *buffer_after_onmetadata;

    /* yt_mode = -1; */
    if (argc != 3) {
	printf("\nthere should be 2 parameters for program\n");
	return -1;
    }
    in_file_name = argv[1];
    out_file_name = argv[2];

    /* check these input value */
    rv = check_arg_parm(in_file_name, out_file_name);
    if (rv != 1) {
	printf("\nerror of the input parm\n");
	return -1;
    }

    /* open the FLV file */
    fin = fopen(in_file_name, "rb");
    if(!fin) {
	printf("\nerror opening file\n");
	return -1;
    }

    /* find the size of the FLV file */
    fseek(fin, 0, SEEK_END);
    file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    fseek(fin,FLV_HEADER_SIZE,SEEK_SET);
    bytes_remaining = file_size - FLV_HEADER_SIZE;

    while (bytes_remaining > 0) {

	if (found_onmetadata == 1) {
			break;
	}

	fread(&TagType, 1, 1, fin);
	fread(&uint_temp_24, 1, sizeof(uint24_t), fin);
	TagDataSize = nkn_vpe_convert_uint24_to_uint32(uint_temp_24);

	switch(TagType & 0x1f) {
	    case 8: // audio tag, just pass
		fseek(fin, FLV_TAG_HEADER_SIZE- 4 + TagDataSize + FLV_PREVIOUS_TAG_SIZE,SEEK_CUR);
		break;
	    case 9: // video tag, just pass
		fseek(fin, FLV_TAG_HEADER_SIZE- 4 + TagDataSize + FLV_PREVIOUS_TAG_SIZE,SEEK_CUR);
		break;
	    case 18: //script tag, need to parse if it is onmetadata
		pos_onmetadata_start = ftell(fin)- 4; // 4  is TagType + TagDataSize
		pos_onmetadata_end   = pos_onmetadata_start + FLV_TAG_HEADER_SIZE
		                     + TagDataSize + FLV_PREVIOUS_TAG_SIZE -1;

		found_onmetadata = 1;
		fread(&timestamp_onmetadata,1,sizeof(uint32_t), fin);
		fseek(fin, 3, SEEK_CUR);//bypass the 3 bytes StreamID

		onmetadata_data_ori = (uint8_t*)malloc(TagDataSize);
		if(!onmetadata_data_ori) {
		    printf("\nfail to allocate mem for onmetadata_data_ori\n");
		    return -1;
		}
		fread(onmetadata_data_ori, 1, TagDataSize, fin);

		onmetadata_data_modi = (uint8_t*)malloc(YT_ONMETADATA_SIZE);
		if (!onmetadata_data_modi) {
		    printf("\nfail to allocate mem for onmetadata_data_modi\n");
		    return -1;
		}
		yt_flv_onmetadata_convert(onmetadata_data_ori, TagDataSize,
					  onmetadata_data_modi, YT_ONMETADATA_SIZE);
		break;
	    default: // handle error
		printf("\nthis is not a valid FLV file \n");
		return -1;
		break;
	}

    }

    buffer_before_onmetadata = (uint8_t*)malloc(pos_onmetadata_start);
    if (buffer_before_onmetadata == NULL) {
	printf("\nfail to allocate mem for buffer_before_onmetadata\n");
	return -1;
    }

    buffer_after_onmetadata = (uint8_t*)malloc(file_size - pos_onmetadata_end -1);
    if (buffer_after_onmetadata == NULL) {
	printf("\nfail to allocate mem for buffer_after_onmetadata\n");
	return -1;
    }

    fseek(fin, 0, SEEK_SET);
    fread(buffer_before_onmetadata, 1, pos_onmetadata_start, fin);
    fseek(fin,pos_onmetadata_end + 1, SEEK_SET);
    fread(buffer_after_onmetadata, 1, file_size - pos_onmetadata_end - 1, fin);
    fclose(fin);

    TagType = 18;
    TagDataSize = YT_ONMETADATA_SIZE;
    uint_temp_24 = nkn_vpe_convert_uint32_to_uint24(TagDataSize);

    previous_tag_size  = TagDataSize + FLV_TAG_HEADER_SIZE;
    temp_previous_tag_size = nkn_vpe_swap_uint32(previous_tag_size);

    /* write output to file */
    FILE *fout = fopen(out_file_name, "wb");
    fwrite(buffer_before_onmetadata, 1, pos_onmetadata_start, fout);

    fwrite(&TagType, 1, 1, fout);  //TagType
    fwrite(&uint_temp_24, 1, 3, fout); //TagDataSize
    fwrite(&timestamp_onmetadata, 1, 4, fout); //timestamp
    uint_temp_24 = nkn_vpe_convert_uint32_to_uint24(0);
    fwrite(&uint_temp_24, 1, 3, fout); //streamID 3 bytes 0
    fwrite(onmetadata_data_modi, 1, TagDataSize, fout);
    fwrite(&temp_previous_tag_size,1,FLV_PREVIOUS_TAG_SIZE,fout);

    fwrite(buffer_after_onmetadata, 1, file_size - pos_onmetadata_end -1,fout);
    fclose(fout);

#ifdef TEST
    FILE *fmeta = fopen("metadata","wb");
    fwrite(onmetadata_data_modi, 1, TagDataSize, fmeta);
    fclose(fmeta);
#endif

    /* free the allocated mem */
    if (buffer_before_onmetadata != NULL) {
	free(buffer_before_onmetadata);
    }

    if (buffer_after_onmetadata != NULL) {
	free(buffer_after_onmetadata);
    }

    if (onmetadata_data_ori != NULL) {
	free(onmetadata_data_ori);
    }

    if (onmetadata_data_modi != NULL) {
	free(onmetadata_data_modi);
    }

    return 0;
}

static int
check_arg_parm(char* in_file_name, char* out_file_name)
{

    if (in_file_name == NULL) {
	printf("\ninput file name cannot be empty\n");
	return -1;
    }

    if (out_file_name == NULL) {
	printf("\ninput file name cannot be empty\n");
	return -1;
    }
	/*
	if (seek_time < 0) {
	    printf("\nseek_time cannot be < 0\n");
	    return -1;
	}

	if (yt_mode < 0 || yt_mode >1) {
	    printf("\nyoutube_mode can only be on/off (1/0)\n");
	    return -1;
	}
	*/
    return 1;
}

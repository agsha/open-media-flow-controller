
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_ism_read_api.h"

#include "nkn_vpe_mfp_ts2mfu.h"
#include "nkn_vpe_m3u8.h"
#include  "nkn_vpe_ismv_mfu.h"
#include <string.h>
#define MOOV_SEARCH_SIZE (32*1024)



typedef struct m3u8in{
    uint32_t smooth_flow_duration;
    int32_t n_profiles;
    char * output_fname;
    char * scratch_path;
    char * uri_prefix;
    char* ism_file_name;
    uint32_t n_chunks;
}im3ut_t;

int32_t  mfu_process_ism(xml_read_ctx_t*,ism_bitrate_map_t**,char*);


int32_t main(int argc, char *argv[]){
    char out[250],m3u8_fname[250],*fname;//*ismv_name;
    //char *audio_name;
    xml_read_ctx_t *ism;
    ism_bitrate_map_t **mapp,*map;
    im3ut_t im3u8;
    uint32_t ts,i;
    int32_t num_chunks;
    //uint32_t Is_audio = 1;
    m3u8_data  **p_md;
    ismv2ts_converter_t *ismv2ts_converter_desc;

    if(argc<2){
	printf("<Input file name><time of chunk in secs>");
	return 0;
    }

    fname = argv[1];
    ts = atoi(argv[2]);
    mapp = (ism_bitrate_map_t**)malloc(sizeof(ism_bitrate_map_t*));
    mfu_process_ism(ism,mapp,fname);
    map = *mapp;
    memset(m3u8_fname,'\0',250);
    strncpy(m3u8_fname,fname,strlen(fname)-4);
    im3u8.n_profiles = map->attr[0]->n_profiles; //Take the video profile only. Valid assumption.
    im3u8.smooth_flow_duration = ts;
    im3u8.output_fname = m3u8_fname;
    im3u8.scratch_path = "./";
    im3u8.uri_prefix ="http://192.168.10.17/iphone";//"http://172.19.172.106/iphonefromsl";

    p_md = (m3u8_data**)calloc(im3u8.n_profiles,  sizeof(m3u8_data*));
    ismv2ts_converter_desc = (ismv2ts_converter_t*) malloc(sizeof(ismv2ts_converter_t));
    //ismv2ts_converter_desc->profile_info = (ismv2ts_profile_t*)malloc(sizeof(ismv2ts_profile_t *));

    for(i=0;i<im3u8.n_profiles;i++){
	memset(out,'\0',250);
	strncpy(out,m3u8_fname,strlen(m3u8_fname));
	ismv2ts_converter_desc->m3u8_name = m3u8_fname;
	ismv2ts_converter_desc->n_profiles = im3u8.n_profiles;
	
	ismv2ts_converter_desc->profile_info[i].ismv_name =
	    map->attr[0]->prop[i].video_name;
	
	ismv2ts_converter_desc->profile_info[i].ts = ts;
	ismv2ts_converter_desc->profile_info[i].video_rate =
	    map->attr[0]->prop[i].bitrate/1024;
	ismv2ts_converter_desc->profile_info[i].audio_rate =
	    map->attr[1]->prop[0].bitrate/1024;
	//ismv2ts_convert(ismv2ts_converter_desc);

	p_md[i] = (m3u8_data*)calloc(1, sizeof(m3u8_data));
	p_md[i]->smooth_flow_duration =ts;
	p_md[i]->n_chunks = num_chunks;
	p_md[i]->bitrate = map->attr[0]->prop[i].bitrate;
    }
    ismv2ts_convert(ismv2ts_converter_desc);
    write_m3u8(p_md,im3u8.n_profiles,im3u8.output_fname,im3u8.scratch_path,im3u8.uri_prefix);

    for(i=0;i<im3u8.n_profiles;i++)
	free(p_md[i]);
    free(p_md);
    ism_cleanup_map(map);
    //if(map != NULL)
    //free(map);
    if(mapp != NULL)
	free(mapp);
    return 0;
    
}


int32_t mfu_process_ism(xml_read_ctx_t*ism,ism_bitrate_map_t** mapp,char* ism_file){
    FILE *fp_ism;   
    uint8_t *dataBuf;
    int bufLen;
    ism_bitrate_map_t* map;
    fp_ism = fopen(ism_file, "rb");
    fseek (fp_ism, 0, SEEK_END);
    bufLen=ftell (fp_ism);
    fseek (fp_ism, 0, SEEK_SET);
    dataBuf = (uint8_t*)malloc(bufLen);
    fread(dataBuf,bufLen,sizeof(uint8_t),fp_ism);
    /* read the server manifest ism file into an XML context */
    ism = init_xml_read_ctx( (uint8_t *)dataBuf, bufLen);
    /* read the [trackid, bitrate] map from the ism file context */
    map = ism_read_bitrate_map(ism);
    *mapp = map;
    free(dataBuf);
    dataBuf = NULL;
    free(ism);
    fclose(fp_ism);
    return 0;

}

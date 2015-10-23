#include <stdio.h>
#include <stdlib.h>
#include "nkn_vpe_types.h"
#include "nkn_vpe_m3u8.h"


int32_t write_m3u8(m3u8_data **p_md, int32_t n_profiles, char *video_name, char *out_path, char *uri_prefix)
{
    char m3u8_name[MAX_PATH];
    char parent_m3u8_name[MAX_PATH];
    char uri[4096];
    int32_t i,j;
    FILE *f_parent_m3u8_file, *f_m3u8_file;

    f_parent_m3u8_file = NULL;
    f_m3u8_file = NULL;

    snprintf(parent_m3u8_name, strlen(out_path) + strlen(video_name) + 7, "%s/%s.m3u8", out_path, video_name);
    f_parent_m3u8_file = fopen(parent_m3u8_name, "wb");
    fprintf(f_parent_m3u8_file, "#EXTM3U\n");

    for(i = 1; i <= n_profiles; i++) {
        snprintf(m3u8_name, strlen(out_path) + strlen(video_name) + 11, "%s/%s_p%02d.m3u8", out_path, video_name, i);
        f_m3u8_file = fopen(m3u8_name, "wb");
        fprintf(f_m3u8_file, "#EXTM3U\n#EXT-X-TARGETDURATION:%d\n",p_md[0]->smooth_flow_duration);
        fprintf(f_parent_m3u8_file, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%d\n", p_md[i-1]->bitrate*8);
        fprintf(f_parent_m3u8_file, "%s/%s\n", uri_prefix, (m3u8_name+strlen(out_path)+1) );
        for(j = 1; j <= (int32_t)(p_md[i-1]->n_chunks); j++) {
            snprintf(uri, strlen(uri_prefix) + strlen(video_name) + 14, "%s/%s_p%02d_%04d.ts", uri_prefix, video_name, i, j);
            fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", (p_md[0]->smooth_flow_duration), uri);
        }
        fprintf(f_m3u8_file, "#EXT-X-ENDLIST");
    }


    fclose(f_parent_m3u8_file);
    fclose(f_m3u8_file);

    return 0;

}

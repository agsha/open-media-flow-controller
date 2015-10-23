#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nkn_vpe_metadata.h"

meta_data_reader* 
init_meta_reader(void *meta_data)
{
    meta_data_reader *mdr;

    mdr = (meta_data_reader*)nkn_malloc_type(sizeof(meta_data_reader), mod_vpe_mp4_meta_parser);
    mdr->p_data = (uint8_t*)meta_data;
    mdr->pos = 0;
    mdr->p_start = meta_data;

    mdr->hdr = (nkn_meta_hdr*)meta_data;
    mdr->feature_tbl = (nkn_feature_table*)(mdr->p_data+sizeof(nkn_meta_hdr));

    return mdr;
}

void*
get_feature(meta_data_reader* mdr, uint32_t feature_id)
{
    uint32_t i;
    i=0;

    for(i = 0; i < mdr->hdr->n_features; i++){
	if(mdr->feature_tbl[i].feature_id == feature_id){
	    return (&(mdr->p_start[mdr->feature_tbl[i].feature_tag_offset]))+sizeof(nkn_feature_tag);
	}
    }

    return NULL;
}

int 
destory_meta_data_reader(meta_data_reader *mdr)
{
    if(mdr){
	free(mdr);
    }

    return 0;
}

//#define _TEST_META_DATA_READER

#ifdef _TEST_META_DATA_READER 
int
main(int argc, char *argv[])
{
    FILE *fp;
    meta_data_reader *mdr;
    sf_meta_data *sf;
    uint8_t *meta_data;

    int size = 0;
    mdr = NULL;
    meta_data = NULL;

    fp = fopen("/home/sunil/data/mad.dat", "rb");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    meta_data = (uint8_t*)nkn_malloc_type(size, mod_test);
    fread(meta_data, 1, size, fp);

    mdr = init_meta_reader(meta_data);
    sf = (sf_meta_data*)get_feature(mdr, FEATURE_SMOOTHFLOW);

    free(meta_data);

    fclose(fp);
    return 0;

}
#endif

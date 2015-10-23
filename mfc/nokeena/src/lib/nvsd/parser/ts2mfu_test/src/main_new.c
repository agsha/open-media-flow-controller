#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include "nkn_vpe_mfp_ts2mfu.h"
#include "mfu2iphone.h"
#include "mfp_ref_count_mem.h"


//declaration
#define MFU_IN_FILE_NAME "out.mfu"

//for verifying mfu
int32_t main(int argc, char *argv[])
{
    FILE* mfu_file_fp = NULL;
    mfu_data_t* mfu_data;
    uint8_t* temp = NULL;
    ref_count_mem_t *ref_count_mem;

    //validating mfu box
    mfu_data = (mfu_data_t*)malloc(sizeof(mfu_data_t));
    mfu_file_fp = fopen(argv[1],"rb");
    fseek(mfu_file_fp,0,SEEK_END);
    mfu_data->data_size = ftell(mfu_file_fp);
    fseek(mfu_file_fp,0,SEEK_SET);
    mfu_data->data = (uint8_t*)malloc(mfu_data->data_size);
    temp = (uint8_t *)malloc(100);
    fread(mfu_data->data ,mfu_data->data_size,1,mfu_file_fp);

    ref_count_mem = createRefCountedContainer(mfu_data, NULL);

    mfubox_ts(ref_count_mem);

    free(mfu_data->data);
    free(ref_count_mem);

    return 0;
}


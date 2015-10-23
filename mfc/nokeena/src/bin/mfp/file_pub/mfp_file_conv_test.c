#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "nkn_vpe_ismc_read_api.h"
#include "nkn_vpe_ism_read_api.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "mfp_file_sess_mgr_api.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_file_convert_api.h"
#include "mfp_live_sess_id.h"
#include "mfp_pmf_tag_defs.h"
#include "mfp_mgmt_sess_id_map.h"
#include "thread_pool/mfp_thread_pool.h"
#include "mfp_err.h"
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_types.h"
#include "nkn_debug.h"
#ifdef UNIT_TEST
/* config paths, read from conf file ? */
#define UT_TEST_VEC_PATH "/home/sunil/data"

const char *invalid_src_path_pmf = \
    UT_TEST_VEC_PATH"/invalid_src_path.xml";

int32_t num_invalid_video_asset_paths = 7;
const char *invalid_video_asset_path[] = {
    UT_TEST_VEC_PATH"/invalid_ism_path_mbr.xml",
    UT_TEST_VEC_PATH"/invalid_ismc_path_mbr.xml",
    UT_TEST_VEC_PATH"/invalid_ismv_path_mbr.xml",
    UT_TEST_VEC_PATH"/invalid_ismv_path_sbr.xml",
    UT_TEST_VEC_PATH"/ismv_path_is_dir.xml",
    UT_TEST_VEC_PATH"/ismc_path_is_dir.xml",
    UT_TEST_VEC_PATH"/ism_path_is_dir.xml"
};

const char *invalid_dest_path_pmf = \
    UT_TEST_VEC_PATH"/invalid_dest_path.xml"; 
const char *invalid_pmf_path = "/klkkjgj/pppor.xml";
const char *ut_result_dump = \
    UT_TEST_VEC_PATH"/ut_results.csv";
const char *invalid_conversion_target_01 = \
    UT_TEST_VEC_PATH"/invalid_conversion_target_01.xml";
const char *invalid_conversion_target_02 = \
    UT_TEST_VEC_PATH"/invalid_conversion_target_02.xml";
const char *ismc_test_vec_01 = 
    UT_TEST_VEC_PATH"/ismc_test_01.xml";

/* number of thrads in the read thread pool */
extern uint32_t glob_mfp_file_num_read_thr;
/* number of threads in the write thread pool */
extern uint32_t glob_mfp_file_num_write_thr;
/* thread pools for read and write tasks */
/* total number of tasks supported per thread pool */
extern uint32_t glob_mfp_file_max_tasks;
extern mfp_thread_pool_t* read_thread_pool;
extern mfp_thread_pool_t* write_thread_pool;
extern uint32_t glob_mfp_fconv_sync_mode;
extern file_id_t *file_state_cont;
extern uint32_t glob_mfp_max_sess_supported;

typedef struct tag_mgmt_bind_pair {
    int32_t pub_sess;
    const char *mgmt_sess;
}mgmt_bind_pair_t;

const mgmt_bind_pair_t bp[] = {
    {0, "f01"},
    {1, "f02"},
    {2, "f03"}
};


int32_t compare_files(char *src1, char *src2);
int32_t exec_moof_fidelity_test(char *ismv_path, char *ism_path, 
				char *ismc_path, char *vec1, 
				char *vec2); 
int32_t exec_invalid_path_test(const char *pmf_path);
int32_t exec_trigger_invalid_sess(int32_t sess);
void * exec_mgmt_find_sess(const char *find_sess);
void build_dummy_sess_tbl(void);
int32_t exec_parse_ismc(const char *path);
static size_t nkn_vpe_fread(void *buf, size_t n, size_t size, 
			      void *desc); 
static void * nkn_vpe_fopen(char *p_data, const char *mode, 
			      size_t size); 
static size_t nkn_vpe_ftell(void *desc);
static size_t nkn_vpe_fseek(void *desc, size_t seekto, 
			      size_t whence);
static void nkn_vpe_fclose(void *desc);
static size_t nkn_vpe_fwrite(void *buf, size_t n, size_t size, 
			       void *desc);

int32_t
compare_files(char *src1, char *src2)
{
    FILE *f1, *f2;
    int32_t err;
    size_t size1, size2;
    uint8_t *p1, *p2;
    
    err = 0;
    f1 = f2 = NULL;
    p1 = p2 = NULL;

    f1 = fopen(src1, "rb");
    f2 = fopen(src2, "rb");
    
    if (!f1 || !f2) {
	printf("error opening file\n");
		return 1;
    }
    
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
    
    size1 = ftell(f1);
    size2 = ftell(f2);
    
    if (size1 != size2) {
	printf("file compare: file sizes are diffrent\n");
	err = 1;
	goto error;
    }

    rewind(f1);
    rewind(f2);

    p1 = (uint8_t*)malloc(size1);
    p2 = (uint8_t*)malloc(size2);

    fread(p1, 1, size1, f1);
    fread(p2, 1, size2, f2);

    err = memcmp(p1, p2, size1);

    printf("\nComparing\n%s with size %ld\n%s with size %ld\nresult is"	   
	   " %d", src1, size1, src2, size2, err);
   error:
    if(f1) fclose(f1);
    if(f2) fclose(f2);
    if(p1) free(p1);
    if(p2) free(p2);
    
    return err;
}

int32_t 
exec_invalid_path_test(const char *pmf_path)
{
    int32_t err;

    err = mfp_file_start_listener_session((char*)pmf_path);
    
    return err;
}

int32_t
exec_moof_fidelity_test(char *ismv_path, char *ism_path, 
		   char *ismc_path, char *vec1, char *vec2)
{
    ismc_publish_stream_info_t *psi;
    ism_bitrate_map_t *map;
    ismv_parser_ctx_t *ismv_ctx;
    int32_t err, i, j, trak_id, profile, rv;
    size_t moof_offset, moof_size;
    uint64_t moof_time;
    const char *test_desc = "Fidelity Test";
    const char *src_video_name;
    const char *trak_type_str[2] = {
	"video", "audio"};
    char out_file_name1[256], out_file_name2[256], *tmp;    

    err = 0;
    moof_size = moof_offset = 0;

    err = ismc_read_profile_map_from_file(ismc_path, &psi);
    if (err) {
	printf("%s: error opening ISMC file\n", test_desc);
	goto error;
    }

    err = ism_read_map_from_file(ism_path, &map);
    if (err) {
	printf("%s: error opening ISM file\n", test_desc);
	goto error;
    }

    err = mp4_create_ismv_ctx_from_file(ismv_path, &ismv_ctx);
    if (err) {
	printf("%s: error opening ISMV file\n", test_desc);
	goto error;
    }
   

    for(i = 0; i < MAX_TRAK_TYPES; i++) {
	if(psi->attr[i]) {
	    for(j = 0; j < (int32_t)psi->attr[i]->n_profiles; j++) {
		profile = psi->attr[i]->bitrates[j];
		trak_id = ism_get_track_id(map,
					   profile,
					   i);
		tmp = strrchr(ism_path, '/');
		if (tmp) {
		    src_video_name = strdup(tmp);
		} else {
		    src_video_name = strdup(ism_path);
		}

		mp4_frag_seek(ismv_ctx, trak_id, 0, 0,\
			      &moof_offset, &moof_size, &moof_time); 
		while(mp4_get_next_frag(ismv_ctx, &moof_offset, &moof_size,
					&moof_time) !=\
		                                E_ISMV_NO_MORE_FRAG) {  
		    if (moof_time == 140000000) {
			int uu = 0;
			uu++;
		    }
		    sprintf(out_file_name1,"%s/%s/QualityLevels(%d)/Fragments(%s=%ld)",
			    vec1, src_video_name, profile,
			    trak_type_str[i], moof_time); 
		    sprintf(out_file_name2,"%s/%s/QualityLevels(%d)/Fragments(%s=%ld)",
			    vec2, src_video_name, profile,
			    trak_type_str[i], moof_time); 
		    rv = compare_files(out_file_name1,
				       out_file_name2);
		    if (rv) {
			printf("%s: segment %s and segment %s are not"
			       "the same\n", test_desc,
			       out_file_name1, out_file_name2);
			return -1;
		    }
		}
	    }
	}
    }

 error:
    return 0;   
}

int32_t
exec_trigger_invalid_sess(int32_t sess)
{
    int32_t err;

    err = mfp_convert_pmf(sess);
    return err;
}
    
void 
build_dummy_sess_tbl(void)
{
    int32_t i, n;

    i = 0;
    n = sizeof(bp)/sizeof(bp[0]);
    
    for (i = 0; i < n; i++) {
	mfp_mgmt_sess_tbl_add(bp[i].pub_sess, (char*) bp[i].mgmt_sess);
    }
    
}

void *
exec_mgmt_find_sess(const char *find_sess)
{
    mgmt_sess_state_t *pub_sess;

    pub_sess = mfp_mgmt_sess_tbl_find((char*)find_sess);
    
    return pub_sess;
    	
}

int32_t
exec_parse_ismc(const char *path)
{
    ismc_publish_stream_info_t *psi;
    int32_t rv;

    rv = ismc_read_profile_map_from_file((char*)path, &psi);

    return 0;
}

int32_t main(int argc, char *argv[])
{
    char *ismv_path, *ism_path, *ismc_path,
	*src1, *src2;
    int32_t i, err, run_fidelity_test;
    FILE *fp;
    int8_t *xsd_path;
    
    log_thread_start();
    glob_mfp_fconv_sync_mode = 1;

    fp = fopen(ut_result_dump, "wb");
    i = 0;
    run_fidelity_test = atoi(argv[++i]);
    if (run_fidelity_test) {
	ismv_path = argv[++i];
	ism_path = argv[++i];
	ismc_path = argv[++i];

	src1 = argv[++i];
	src2 = argv[++i];
    }

    xsd_path = (int8_t*)argv[++i];
    
    file_state_cont = newLiveIdState(glob_mfp_max_sess_supported);
    readFileIntoMemory((int8_t const*)xsd_path,
		       &xml_schema_data, &xsd_len);
    if (xml_schema_data == NULL) {
	printf("XML Schema load failed.\n");
	exit(-1);
    }

    //init the mgmt <-> mfp sess id map
    mfp_mgmt_sess_tbl_init();
    
    // init the tag names for the PMF parser
    pmfLoadTagNames();

    // force to run only one job at a time */
    glob_mfp_fconv_sync_mode = 1;

    // create the thread pools
    glob_mfp_file_num_read_thr = 1;//force to 1 thread
    glob_mfp_file_num_write_thr = 1;//force to 1 thread
    read_thread_pool = newMfpThreadPool(glob_mfp_file_num_read_thr,
					glob_mfp_file_max_tasks);
    write_thread_pool = newMfpThreadPool(glob_mfp_file_num_write_thr,
					 glob_mfp_file_max_tasks);

    fprintf(fp, "Test, Expected Error Code, Error Code\n");
    if (run_fidelity_test) {

	err = exec_moof_fidelity_test(ismv_path, ism_path, ismc_path,
				      src1, src2);
	if (err) {
	    fprintf(fp, "%s,%d,%d\n", "moof Fidelity Test", 0, err);
	}
    }

    err = exec_invalid_path_test(invalid_src_path_pmf);
    if (err) {
	fprintf(fp, "%s,%d,%d\n", "Invalid Source Path Test",
		-E_VPE_PARSER_INVALID_FILE, err); 
    }
    
    err = exec_invalid_path_test(invalid_dest_path_pmf);
    if (err) {
	fprintf(fp, "%s,%d,%d\n", "Invalid Dest Path Test",
		-E_VPE_PARSER_INVALID_OUTPUT_PATH, err);
    }
    
    err = exec_invalid_path_test(invalid_pmf_path);
    if (err) {
	fprintf(fp, "%s,%d,%d\n", "Invalid PMF Path Test",
		-E_MFP_PMF_INVALID_FILE, err); 
    }

    /* check for invalid video asset paths*/
    for (i = 0; i < num_invalid_video_asset_paths; i++) {
	err = exec_invalid_path_test(invalid_video_asset_path[i]);
	if (err) {
	    fprintf(fp, "%s,%d,%d\n", "Invalid Source Path Test",
		    -E_VPE_PARSER_INVALID_FILE, err); 
	}
    }

    /* mfp_file_convert_api.c tests */
    err = exec_trigger_invalid_sess(100);
    if (err) {
	fprintf(fp, "%s,%d,%d\n", "Invalid Session ID",
		-E_MFP_INVALID_SESS, err); 
    }

    err = exec_invalid_path_test(invalid_conversion_target_01);
    if (err) {
	fprintf(fp, "%s,%d,%d\n", "Invalid Conversion Target",
		-E_MFP_INVALID_CONVERSION_REQ, err); 
    }

    /* mfp_mgmt_sess_id_map tests */
    int32_t *sess, rv;
    build_dummy_sess_tbl();
    sess = exec_mgmt_find_sess("f04");
    if (!sess) {
	printf("Invalid MGMT Bind Session Test:test passed\n");
    } else {
	printf("Invalid MGMT Bind Session Test:test failed\n");
    } 
    
    sess = exec_mgmt_find_sess("f02");
    if (sess) {
	printf("Invalid MGMT Bind Session Test:test passed\n");
    } else {
	printf("Invalid MGMT Bind Session Test:test failed\n");
    }

    rv = mfp_mgmt_sess_tbl_add(bp[0].pub_sess, (char*)bp[0].mgmt_sess);
    printf("%d\n", rv);
    rv = mfp_mgmt_sess_tbl_add(bp[1].pub_sess, (char*)bp[0].mgmt_sess);
    printf("%d\n", rv);
    rv = mfp_mgmt_sess_tbl_add(bp[0].pub_sess, (char*)bp[1].mgmt_sess);
    printf("%d\n", rv);

    rv = exec_parse_ismc(ismc_test_vec_01);

    const char *http_path =
	"http://172.27.159.11/vec1/src/bb_full_512.ts";
    const char *file_path = 
	"/home/sunil/data/iphone/vec1/src/bb_full_512.ts";
    vpe_http_reader_t *hr;
    uint8_t buf[1024], *buf1, *buf2;
    size_t size1, size2;
    void *desc1, *desc2;
    int32_t iter, n_iter, cmp;
    io_handlers_t ioh1 = {
	.ioh_open = nkn_vpe_fopen,
	.ioh_read = nkn_vpe_fread,
	.ioh_tell = nkn_vpe_ftell,
	.ioh_seek = nkn_vpe_fseek,
	.ioh_close = nkn_vpe_fclose,
	.ioh_write = nkn_vpe_fwrite
    };
    io_handlers_t ioh2 = {
	.ioh_open = vpe_http_sync_open,
	.ioh_read = vpe_http_sync_read,
	.ioh_tell = vpe_http_sync_tell,
	.ioh_seek = vpe_http_sync_seek,
	.ioh_close = vpe_http_sync_close,
	.ioh_write = NULL
    };  
    
    desc1 = ioh1.ioh_open((char *)file_path, "rb", 0);
    ioh1.ioh_seek(desc1, 0, SEEK_END);
    size1 = ioh1.ioh_tell(desc1);
    ioh1.ioh_seek(desc1, 0, SEEK_SET);

    desc2 = ioh2.ioh_open((char *)http_path, "rb", 0);
    ioh2.ioh_seek(desc2, 0, SEEK_END);
    size2 = ioh2.ioh_tell(desc2);
    ioh2.ioh_seek(desc2, 0, SEEK_SET);

    if (size1 != size2) {
	printf("error in size calculation \n");
	return -1;
    }

    n_iter = (size1/3145616);
    if (size1 % (3145616)) {
	n_iter++;
    }
	
    buf1 = (uint8_t*)malloc(3145616);
    buf2 = (uint8_t*)malloc(3145616);

    for (iter = 0; iter < n_iter; iter++) {
	ioh1.ioh_read(buf1, 1, 3145616, desc1);
	{ 
	    FILE *f1;
	    f1 = fopen("/home/sunil/data/b1", "wb");
	    fwrite(buf1, 1, 3145616, f1);
	    fclose(f1);
	}
		
	ioh2.ioh_read(buf2, 1, 3145616, desc2);
	{ 
	    FILE *f1;
	    f1 = fopen("/home/sunil/data/b2", "wb");
	    fwrite(buf2, 1, 3145616, f1);
	    fclose(f1);
	}
	if ((cmp = memcmp(buf1, buf2, 3145616)) != 0) {
	    printf ("fidelity error \n");
	    continue;
	}
    }
	
    fclose(fp);
}

void *
nkn_vpe_fopen(char *p_data, const char *mode, size_t size)
{
    size = size;
    return (void *)fopen(p_data, mode);
}

static size_t
nkn_vpe_fseek(void *desc, size_t seek, size_t whence)
{
    FILE *f = (FILE*)desc;
    return fseek(f, seek, whence);
}

static size_t 
nkn_vpe_ftell(void *desc)
{
    FILE *f = (FILE*)desc; 
    return ftell(f);
}

static void
nkn_vpe_fclose(void *desc)
{
    FILE *f = (FILE*)desc;
    fclose(f);
    
    return;
}

static size_t
nkn_vpe_fwrite(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;
    return fwrite(buf, n, size, f);
}


static size_t
nkn_vpe_fread(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;
	
    return fread(buf, n, size, f);
}


#endif

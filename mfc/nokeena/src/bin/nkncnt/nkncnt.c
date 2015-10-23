#include <sys/types.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <libgen.h>
#include <limits.h>
#include <syslog.h>

#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_common_config.h"
#include "proc_utils.h"
#include "tstring.h"

////////////////////////////////////////////////////////////
// Save Data into a file
////////////////////////////////////////////////////////////
/*
    File Format:
	1. struct parameters: sizeof(struct parameters) 
	2. for(i=0;i<pshmdef->tot_cnts;i++) {
		2 bytes: scid
		nbytes: typestring, '\0', ending with '\0'
		2 bytes: sizeof variable
	   }
	3. the next formats are always 
		2 bytes id,
		1/2/4/8 bytes data.
	4. If time mark is changed, it always put time mark first.
	   time id is 0xFFFF. time resolution is second.
	5. If data has not been changed, there is no save.

    Notes:
	a) the first few bytes are signature of the file format.
	b) Max file size if 100MBytes. 
	c) If max file size is reached, it will be compressed by gzip.
	d) filename is specified by user
*/
	

struct parameters {
        char sig[64];
        time_t tv;
        int32_t freq;  // in seconds
        int32_t duration;  // in seconds
        int32_t tot_cnts;
        int64_t headoffset;
	int32_t reserved;
} gpar;

#define TIME_ID 0xFFFFFFFF
#define TIME_ID_2 0xFFFF   //Used for nkncnt 1.1
#define MAX_CNT_FILESIZE 100*MBYTES
#define MAX_NUM_OF_CNT_FILE    100
#define FROM_SHMEM 0
#define FROM_FILE  1
#define MAX_SEARCH_STRING	20
#define DISP_FORMAT_LINE	0
#define DISP_FORMAT_COLUMN	1
// SHIFT_MASK = sizeof(long int) * ( NUMBER OF CHAR BITS - 1)
#define SHIFT_MASK 56

#define SIZE_OF_ZIP_FILENAME 256
#define SIZE_OF_UPLOAD_CMD   1024
#define SIZE_OF_PATH_FILENAME 1024
#define SIZE_OF_HOSTNAME      256
#define SIZE_OF_UPLOAD_FILENAME 512
#define SIZE_OF_TIMEBUFFER 40

static glob_item_t * g_allcnts;
static int g_cnt_mark [MAX_CNTS_NVSD];
static nkn_shmdef_t * pshmdef;
static char * varname;
static int sleep_time = 2;
static int duration_time = INT_MAX; 
static char producer[256]="/opt/nkn/sbin/nvsd";
static glob_item_t last_bak[MAX_CNTS_NVSD];
static int list_countername=0;
static uint64_t sh_size;
static int max_cnt_space;
static int cntfileid=0;
const char * countfile=NULL; //"/var/log/nkncnt/nkn_counters"; 
const char * cntfileid_filename="/var/log/nkncnt/.nkncnt.fileid";
static char * save_to_file_as_txt = NULL; 
static FILE * scfd=NULL;
static FILE * save_as_txt_fd=NULL;
static unsigned long cntfile_totsize=0;
static char cntbuffer[1124];
static int curlen=0;
static int display_format = DISP_FORMAT_LINE;
static int display_zero = 1;
static int display_short = 0;
static int id_sz = 2;

static char * URL = NULL;
static char time_string_buffer[SIZE_OF_TIMEBUFFER] = { 0 };
static struct tm *loctime;
static const char *szKeyCountersHeading = "\t\t<Total Bytes Sent> <Current Connections> <Total Size - Origin> <Total Size - Cache>" ;
static void savetobuffer(void * p, int size);
inline
static long int ulabs(long int val);

static FILE * cnt_open_file(void);
static int savetofile(void);
static FILE * file_read_head(void);
static void file_display_counters(void);
static void shm_display_counters(void);
static void usage(void);
void catcher(int sig);
static void version(void);
static void print_out_time_to_screen_and_file( time_t *tv);
static void print_value_in_column_to_screen_and_file( glob_item_t *pitem, void *value);
static void file_do_display_counters_to_screen_and_file( glob_item_t *pitem, void *value);
static void shm_display_and_save_counters_astxt( void);
static void flushbuffertofile(void);
static void upload_file(void);
static int ReadUrl(const char *fname);

////////////////////////////////////////////////////////////
// Common functions
////////////////////////////////////////////////////////////
static void nkn_read_fileid(void);
static void nkn_read_fileid(void)
{
	FILE * fp;
	fp = fopen(cntfileid_filename, "r");
	if(!fp) return;
	fscanf(fp, "cntfileid=%d\n", &cntfileid);
	fclose(fp);
}

static void nkn_save_fileid(void);
static void nkn_save_fileid(void)
{
	FILE * fp;
	fp = fopen(cntfileid_filename, "w");
	if(!fp) return;
	fprintf(fp, "cntfileid=%d\n", cntfileid);
	fclose(fp);
}


/*
 * backup the data value for calculating rate purpose.
 */
static void cnt_backup_data(int tot_cnts, glob_item_t * pglobal, glob_item_t * pbak)
{
	int id;

	for(id=0; id<tot_cnts; id++) {
		pbak[id].name = pglobal[id].name;
		pbak[id].size = pglobal[id].size;
	}
}

static void mark_display_field(int tot_cnts, glob_item_t * pitem, char * var[])
{
	int i, j;

	/*
	 * Mark all fields are not displayed.
	*/
	for(i=0; i<tot_cnts; i++) {
		g_cnt_mark[i] = 0;
	}

	/*
	 * based on search string, mark display fields.
	 */
	for(j=0;j<MAX_SEARCH_STRING;j++) {
		if(var[j] == NULL) continue;
		for(i=0;i<tot_cnts; i++) {
			if ( (pitem[i].size == 0) && 
			     (list_countername == 0) ) {
				continue;
			}
			if(strstr(varname+pitem[i].name, var[j]) != NULL) {
			   if(display_short==1) {
			      if (strlen(varname+pitem[i].name)<=40) {
				g_cnt_mark[i] = 1;
			      }
			   }
			   else {
				g_cnt_mark[i] = 1;
			   }
			}
		}
	}

	return;
}

////////////////////////////////////////////////////////////
// Functions for data column display
////////////////////////////////////////////////////////////

static void print_counter_name(int tot_cnts, glob_item_t * pitem)
{
	int i;

	printf("\t");
	for(i=0;i<tot_cnts; i++) {
		if(g_cnt_mark[i] == 1) {
			printf("%s ", varname+pitem[i].name);
		}
	}
	printf("\n");
}

inline
static long int ulabs(long int val)
{
    long int mask = val >> SHIFT_MASK; 
    return (val ^ mask) - mask;
}

static void print_value_in_line(glob_item_t * pitem, void * value)
{
	char * pc, * pc1;
	short * ps, * ps1;
	int * pi, * pi1;
	unsigned long * pl, *pl1;
	double rate;

	switch(pitem->size) {
	case 1:
		pc=(char *) value;
		pc1=(char *) &pitem->value;
		rate=(sleep_time ? (double)(*pc - *pc1) / (1.0*sleep_time) : 0);
		if(*pc || rate || display_zero) {
			printf("%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *pc, rate);
		}
		break;
	case 2:
		ps=(short *) value;
		ps1=(short *) &pitem->value;
		rate=(sleep_time ? (double)(*ps - *ps1) / (1.0*sleep_time) : 0);
		if(*ps || rate || display_zero) {
			printf("%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *ps, rate);
		}
		break;
	case 4:
		pi=(int *) value;
		pi1=(int *) &pitem->value;
		rate=(sleep_time ? (double)(*pi - *pi1) / (1.0*sleep_time) : 0);
		if(*pi || rate || display_zero) {
			printf("%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *pi, rate);
		}
		break;
	case 8:
		/***************************************************************
		* Although this is for long, for really huge numbers we go negative
		* So changing everything inside here to unisgned. If there are
		* counters which require long negative numbers to be displayed
		* then 1) We have bigger problems, 2) add another case here
		* Plus no point in having negative rate. Made it absolute.
		**************************************************************/
		pl=(unsigned long *) value;
		pl1=(unsigned long *) &pitem->value;
		rate=(sleep_time ? (double) (ulabs(*pl - *pl1) / (1.0*sleep_time)) : 0);
		if(*pl || rate || display_zero) {
			printf("%-40s = %-13lu\t%.2f /sec\n", varname+pitem->name, *pl, rate);
		}
		break;
	}

	pitem->value=*(long *)value;
}

static void print_out_time(time_t * tv)
{
	char buf[64];

	// We don't want to save time mark either if there is no change
	strcpy(buf, asctime(gmtime(tv)));
	buf[strlen(buf)-1]=0;
	printf("%s ", buf);
}

static void
print_out_time_to_screen_and_file(
				time_t *tv)
{
	char buf[64];
	strcpy(buf, asctime(gmtime(tv)));
	buf[strlen(buf)-1]=0;
	printf("%s ", buf);
	fprintf(save_as_txt_fd,"%s ",buf);	
}

static void print_value_in_column(glob_item_t * pitem, void * value)
{
	char * pc;
	short * ps;
	int * pi;
	long * pl;

	switch(pitem->size) {
	case 1:
		pc=(char *) value;
		printf(" %1d", *pc);
		break;
	case 2:
		ps=(short *) value;
		printf(" %6d", *ps);
		break;
	case 4:
		pi=(int *) value;
		printf(" %10d", *pi);
		break;
	case 8:
		pl=(long *) value;
		printf(" %13ld", *pl);
		break;
	}

	pitem->value=*(long *)value;
}

static void 
print_value_in_column_to_screen_and_file(
		glob_item_t *pitem,
		 void *value)
{
	char * pc;
	short * ps;
	int * pi;
	long * pl;

	switch(pitem->size) {
	case 1:
		pc=(char *) value;
		printf(" %1d", *pc);
		fprintf(save_as_txt_fd, " %1d", *pc);
		break;
	case 2:
		ps=(short *) value;
		printf(" %6d", *ps);
		fprintf(save_as_txt_fd, " %6d", *ps);
		break;
	case 4:
		pi=(int *) value;
		printf(" %10d", *pi);
		fprintf(save_as_txt_fd, " %10d", *pi);
		break;
	case 8:
		pl=(long *) value;
		printf(" %13ld", *pl);
		fprintf(save_as_txt_fd, " %13ld", *pl);
		break;
	}

	pitem->value=*(long *)value;
}

static void 
print_value_in_line_to_screen_and_file (
				glob_item_t *pitem, 
				void *value)
{
	char * pc, * pc1;
	short * ps, * ps1;
	int * pi, * pi1;
	long * pl, *pl1;
	double rate;

	switch(pitem->size) {
	case 1:
		pc=(char *) value;
		pc1=(char *) &pitem->value;
		rate=(sleep_time ? (double)(*pc - *pc1) / (1.0*sleep_time) : 0);
		if(*pc || rate || display_zero) {
			printf("%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *pc, rate);
			fprintf(save_as_txt_fd,"%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *pc, rate);
		}
		break;
	case 2:
		ps=(short *) value;
		ps1=(short *) &pitem->value;
		rate=(sleep_time ? (double)(*ps - *ps1) / (1.0*sleep_time) : 0);
		if(*ps || rate || display_zero) {
			printf("%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *ps, rate);
			fprintf(save_as_txt_fd,"%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *ps, rate);
		}
		break;
	case 4:
		pi=(int *) value;
		pi1=(int *) &pitem->value;
		rate=(sleep_time ? (double)(*pi - *pi1) / (1.0*sleep_time) : 0);
		if(*pi || rate || display_zero) {
			printf("%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *pi, rate);
			fprintf(save_as_txt_fd,"%-40s = %-13d\t%.2f /sec\n", varname+pitem->name, *pi, rate);
		}
		break;
	case 8:
		pl=(long *) value;
		pl1=(long *) &pitem->value;
		rate=(sleep_time ? (double)(*pl - *pl1) / (1.0*sleep_time) : 0);
		if(*pl || rate || display_zero) {
			printf("%-40s = %-13ld\t%.2f /sec\n", varname+pitem->name, *pl, rate);
			fprintf(save_as_txt_fd,"%-40s = %-13ld\t%.2f /sec\n", varname+pitem->name, *pl, rate);
		}
		break;
	}

	pitem->value=*(long *)value;
}




static void print_list_counter_names(int tot_cnts, glob_item_t * p_allcnts)
{
	int i;

	printf("%-40s\ttype\n", "variable name");
	for(i=0;i<tot_cnts; i++) {

		if(g_cnt_mark[i] == 0) {
			continue;
		}

		printf("%-40s\t", varname+p_allcnts[i].name);
		switch(p_allcnts[i].size) {
		case 0: printf("__deleted__\n"); break;
		case 1: printf("char\n"); break;
		case 2: printf("short\n"); break;
		case 4: printf("int\n"); break;
		case 8: printf("long\n"); break;
		default: printf("\n"); break;
		}
	}
}

////////////////////////////////////////////////////////////
// Save data into file
////////////////////////////////////////////////////////////

static FILE * cnt_open_file(void)
{
	int id;
	char * name;
	struct stat statbuf;
	char cntfilename_cmd[256];
	unsigned int timeid=TIME_ID;
	time_t tv;
	time_t cur_time;

	// Delete the file if exists
	nkn_read_fileid();
	cntfileid++;
	if(cntfileid >= MAX_NUM_OF_CNT_FILE) {
		cntfileid = 1;
	}
	nkn_save_fileid();

	snprintf(cntfilename_cmd, 256, "rm -rf %s.%d", countfile, cntfileid);
	system(cntfilename_cmd);
	snprintf(cntfilename_cmd, 256, "rm -rf %s.%d.gz", countfile, cntfileid);
	system(cntfilename_cmd);

	/*
	 * Signature string: "nkn_ver1.0" '\0'
	 * n bytes: sizeof(struct parameters)
	 * for(i=0;i<pshmdef->tot_cnts;i++) {
	 *      2 bytes: scid
	 *     typestring, '\0'
	 * }
	 */
	snprintf(cntfilename_cmd, 256, "%s.%d", countfile, cntfileid);
	scfd = fopen(cntfilename_cmd, "wb");
	if( scfd == NULL )
	{
		printf("failed to open file %s\n", cntfilename_cmd);
		exit(1);
	}
	//The date and time function to get the values and append to the file while uploading.
        cur_time = time(NULL);
        loctime = localtime(&cur_time);
        /* format current time to yearmonthday-hourminutesecond format */
        strftime (time_string_buffer,SIZE_OF_TIMEBUFFER, "%G%m%d-%H%M%S",loctime);

	cntfile_totsize = 0;
	
	// Initialize the parameter
	strcpy(gpar.sig, pshmdef->version);

	if(!strcmp(gpar.sig, "nkn_cnt_1.2"))
	    id_sz = 4;

	time(&gpar.tv);
	gpar.freq=sleep_time; // in seconds
	gpar.duration=duration_time; // in seconds
	gpar.tot_cnts=pshmdef->tot_cnts;
	gpar.headoffset = sizeof(struct parameters);
	for(id=0; id<gpar.tot_cnts; id++) {
		name=varname+g_allcnts[id].name;
		gpar.headoffset += id_sz + (strlen(name)+1) + 2;
	}

	savetobuffer(&gpar, sizeof(struct parameters));

	for(id=0; id<gpar.tot_cnts; id++)
	{
		savetobuffer(&id, id_sz);
		name=varname+g_allcnts[id].name;
		savetobuffer(name, strlen(name)+1);
		savetobuffer(&g_allcnts[id].size, 2);
	}

	// Save the init value
	time(&tv);
	savetobuffer(&timeid, id_sz); // sizeof(unsigned short)
	savetobuffer(&tv, sizeof(time_t)); // 8=sizeof(time_t)

	for(id=0; id<pshmdef->tot_cnts; id++)
	{
		savetobuffer(&id, id_sz);
		savetobuffer(&g_allcnts[id].value, g_allcnts[id].size);
	}
	return scfd;
}

static void gzip_cnt_file(void);
static void gzip_cnt_file() {
	char cntfilename_cmd[SIZE_OF_ZIP_FILENAME];
	int len =0;
        len = snprintf(cntfilename_cmd,sizeof(cntfilename_cmd), "gzip %s.%d", countfile, cntfileid);
	if(len){
		system(cntfilename_cmd);
		upload_file();
	}
}

static void upload_file(){

	char upload_cmd[SIZE_OF_UPLOAD_CMD];
	char path_file_name[SIZE_OF_PATH_FILENAME];
	char *filename = NULL;
	char *directoryname = NULL;
	char hostname[ SIZE_OF_HOSTNAME];
	char upload_filename[SIZE_OF_UPLOAD_FILENAME];
	char upload_dir_file[SIZE_OF_PATH_FILENAME];
	int len =0,err= 0,status =0;
	tstring *ret_output = NULL;


	if((countfile != NULL)&&(cntfileid != 0 )){
		len = snprintf(path_file_name,sizeof(path_file_name), "%s.%d.gz", countfile, cntfileid);

		if(len){
			filename = basename(path_file_name);
			directoryname = dirname(path_file_name);	
			gethostname ( hostname, sizeof(hostname));
			
		snprintf(upload_filename,sizeof(upload_filename),"%s-nkn-counters-%s.gz",hostname,
				time_string_buffer ); 
		
		}
		if((URL != NULL ) &&( filename != NULL)){

			len = snprintf(upload_cmd,sizeof(upload_cmd),
				      "%s%s",URL,upload_filename);
		
			 len = snprintf(upload_dir_file,sizeof(upload_cmd) ,
                                      "%s/%s",directoryname,filename);

			if(len)
				err = lc_launch_quick_status(&status, &ret_output, true, 10, "/opt/tms/bin/mdreq",
							    "-q","action","/file/transfer/upload",
						            "remote_url","uri",upload_cmd,
							    "local_path","string",upload_dir_file);
		                bail_error(err);
		}
	}   

bail:
    ts_free(&ret_output);

}

/*
 * in order to avoid too much disk operation, all data is stored in the memory first.
 */
static void savetobuffer(void * p, int size)
{
	// We buffer first to manager the max length of the counter file
	if(curlen>KiBYTES) {
		fwrite(cntbuffer, 1, curlen, scfd);
		cntfile_totsize+=curlen;
		curlen=0;
		if(cntfile_totsize>MAX_CNT_FILESIZE) {
			flushbuffertofile();
			fclose(scfd);
			gzip_cnt_file();
			cnt_open_file();
		}
	}

	// Buffer the data
	memcpy(&cntbuffer[curlen], p, size);
	curlen+=size;
}

static void flushbuffertofile(void)
{
	fwrite(cntbuffer, 1, curlen, scfd);
	cntfile_totsize+=curlen;
	curlen=0;
}

static int savetofile(void)
{
	int id;
	unsigned int timeid=TIME_ID;
	time_t tv;
	int timesaved=0;

	// We don't want to save time mark either if there is no change
	time(&tv);

	for(id=0; id<pshmdef->tot_cnts; id++)
	{
		if( g_allcnts[id].value == last_bak[id].value ) continue;
		last_bak[id].value = g_allcnts[id].value;

		if(timesaved == 0) {
			savetobuffer(&timeid, id_sz); // sizeof(unsigned short)
			savetobuffer(&tv, sizeof(time_t)); // 8=sizeof(time_t)
			timesaved=1;
		}

		savetobuffer(&id, id_sz);
		savetobuffer(&g_allcnts[id].value, g_allcnts[id].size);
	}
	return 1;
}

////////////////////////////////////////////////////////////
// Display from file
////////////////////////////////////////////////////////////

static FILE * file_read_head(void)
{
	FILE * fd;
	int id, ret;
	uint64_t len;
	char * p;

	// Read the file first
	fd = fopen(countfile, "rb");
	if( fd == NULL )
	{
		printf("failed to open file %s\n", countfile);
		exit(1);
	}
	fread((char *)&gpar, 1, sizeof(struct parameters), fd);

	if(!strcmp(gpar.sig, "nkn_cnt_1.2"))
	    id_sz = 4;

	/*
	 * hardcoded 100K buffer size which should be enough for now.
	 */
	len = gpar.headoffset - sizeof(struct parameters);
	varname=(char *)malloc(sizeof(char)*(len+10));
	if( !varname ) exit(2);

	p = varname;
	while(len) {
		ret = fread(p, 1, len, fd);
		p += ret;
		len -= ret;
	}

	/*
	 * fix header pointers.
	 */
	p=varname;
	for(id=0; id<gpar.tot_cnts; id++) {
		p+=id_sz;  // skip id
		last_bak[id].name = p-varname;
		p += strlen(p)+1;
		last_bak[id].size = *p;
		p += 2;
	}

	fseek(fd, gpar.headoffset, SEEK_SET);

	sleep_time=gpar.freq;
	duration_time=gpar.freq;
	return fd;
}

static void file_do_display_counters(glob_item_t * pitem, void * value)
{
	(display_format == DISP_FORMAT_COLUMN) 
		? print_value_in_column(pitem, value)
		: print_value_in_line(pitem, value);
}

static void 
file_do_display_counters_to_screen_and_file(
					glob_item_t *pitem, 
					void *value)
{
	(display_format == DISP_FORMAT_COLUMN) 
		? print_value_in_column_to_screen_and_file(pitem, value)
		: print_value_in_line_to_screen_and_file(pitem, value);
}



static void file_display_counters(void)
{
	int id = 0, lastid = 0;
	unsigned long value;
	time_t tv;

	lastid=gpar.tot_cnts;
	// Display the counters
	while(!feof(scfd)) {
		// Time id
		if(fread(&id, 1, id_sz, scfd)==0) break;

		if( (id_sz == 4 && (unsigned int)id==TIME_ID) ||
			(id_sz ==2 && (unsigned int)id==TIME_ID_2) ) {
			for(; lastid<gpar.tot_cnts; lastid++) {
				if(g_cnt_mark[lastid] == 1) {
				file_do_display_counters(&last_bak[lastid], &last_bak[lastid].value);
				}
			}
			printf("\n");
			fread(&tv, 1, sizeof(time_t), scfd); // 8=sizeof(time_t)
			print_out_time(&tv);
			if(display_format != DISP_FORMAT_COLUMN) {
				printf("\n");
			}
			lastid=0;
			continue;
		}

		assert(id>=0 && id<=gpar.tot_cnts);
		value=0;
		fread(&value, 1, last_bak[id].size, scfd);

		for(; lastid<id; lastid++) {
			if(g_cnt_mark[lastid] == 1) {
			file_do_display_counters(&last_bak[lastid], &last_bak[lastid].value);
			}
		}

		if(g_cnt_mark[id] == 1) {
		file_do_display_counters(&last_bak[id], &value);
		}
		lastid = id+1;
	}

	for(; lastid<gpar.tot_cnts; lastid++) {
		if(g_cnt_mark[lastid] == 1) {
		file_do_display_counters(&last_bak[lastid], &last_bak[lastid].value);
		}
	}
	printf("\n");
}

////////////////////////////////////////////////////////////
// Display from shared Memory
////////////////////////////////////////////////////////////

static void shm_display_counters(void)
{
	int i;
	time_t tv;

	// We don't want to save time mark either if there is no change
	time(&tv);
	print_out_time(&tv);
	if(display_format != DISP_FORMAT_COLUMN) {
		printf("\n");
	}

	/* Display the counters */
	for(i=0;i<pshmdef->tot_cnts; i++) {

		if(g_cnt_mark[i] == 1) {
			file_do_display_counters(&last_bak[i], &g_allcnts[i].value);
		}
	}
	printf("\n");
}

////////////////////////////////////////////////////////////
// Write data to txt file 
////////////////////////////////////////////////////////////

static void 
shm_display_and_save_counters_astxt(
				void)
{

	int i;
	time_t tv;

	time(&tv);
	/* display time to screen as well as to the file */
	print_out_time_to_screen_and_file(&tv);

	if(display_format != DISP_FORMAT_COLUMN) {
		printf("\n");
		fprintf(save_as_txt_fd,"\n");
	}

	/* Display the counters */
	for(i=0;i<pshmdef->tot_cnts; i++) {

		if(g_cnt_mark[i] == 1) {
			file_do_display_counters_to_screen_and_file(&last_bak[i],&g_allcnts[i].value);
		}
	}
	printf("\n");
	fprintf(save_as_txt_fd,"\n");
}



////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

static void usage(void)
{
        printf("\n");
        printf("nkncnt - Nokeena Counter Management/Display Tool\n");
        printf("Usage:\n");
        printf("-p <name> : use this option if your module is running from /opt/nkn/sbin\n");
        printf("-P <path> : provide the full path of the executable \n");
        printf("-f <name> : save statistic counter into this file\n");
        printf("-F <name> : load statistic counter from this file\n");
        printf("-t <time> : display interval in seconds. default: 2. when 0: it runs only once\n");
        printf("-s <str>  : search a counter which includes this substring\n");
        printf("-L        : List all counters name only\n");
        printf("-c        : Display counter in column format\n");
        printf("-k        : show a few key counters continuously\n\n");
        printf("-d        : Run as Daemon\n");
        printf("-l <time> : duration to loop in seconds\n");
        printf("-S <name> : display as well as save counters in a text file\n");
        printf("-z        : don't print this counter if it is 0 and rate is 0\n");
	printf("-C <file> : Config file that contains the URL to upload to - [scp|ftp|sftp]://[user]:[password]@host/path\n");
        printf("-v        : show version\n");
        printf("-h        : show this help\n");
        printf("Examples:\n");
        printf("1) To save statistic counters into a file:\n");
        printf("   	    nkncnt -f output\n");
        printf("2) To display counter in real time:\n");
        printf("   	    nkncnt -s err\n");
        printf("3) To display counters from file:\n");
        printf("   	    nkncnt -F output.1 -s err\n");
        printf("4) To display counters only once:\n");
        printf("   	    nkncnt -s err -t 0\n");
        printf("5) To display key counters:\n");
        printf("   	    nkncnt -k\n");
        printf("\n");
        exit(1);
}

static void version(void)
{
        printf("\n");
        printf("nkncnt - Nokeena Count Management Tool\n");
        printf("Build Date: "__DATE__ " " __TIME__ "\n");
        printf("\n");
        exit(1);

}

static void daemonize(void)
{

        if (0 != fork()) exit(0);

        if (-1 == setsid()) exit(0);

        signal(SIGHUP, SIG_IGN);

        if (0 != fork()) exit(0);

        /* Do not chdir when this processis managed by the PM
         * if (chdir("/") != 0) exit(0);
         */
}

void catcher(int sig)
{
	if(scfd) {
		flushbuffertofile();
		fclose(scfd);
		gzip_cnt_file();
	}
	if(sig == SIGHUP) {
		return;
	}
	exit(1);
}


////////////////////////////////////////////////////////////
// Main function
////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
	key_t shmkey;
	int shmid;
	char *shm;
	char * search[MAX_SEARCH_STRING];
	int nextsearch=0;
	int runas_daemon=0;
	int nCount = 0 ;
	int ret;
	int from=FROM_SHMEM;
	int revision;
	int cummulative_time = 0;
	int bShowKeyCounters = NKN_FALSE;

	// Initialize
	for(ret = 0; ret<MAX_SEARCH_STRING; ret++) {
		search[ret] = NULL;
	}

	while ((ret = getopt(argc, argv, "bp:P:f:F:t:Ls:l:S:C:chvdkz")) != -1) {
        switch (ret) {
	case 'p':
		snprintf(producer, sizeof(producer), "/opt/nkn/sbin/%s", optarg);
		break;
	case 'P':
		strcpy(producer, optarg);
		break;
        case 'f':
		countfile=optarg;
		from=FROM_SHMEM;
		break;
        case 'F':
		countfile=optarg;
		from=FROM_FILE;
		break;
        case 't':
		sleep_time = atoi(optarg);
		break;
        case 's':
		if(nextsearch<MAX_SEARCH_STRING) {
			search[nextsearch] = optarg;
			nextsearch++;
		}
		break;
        case 'L':
		list_countername=1;
		break;
        case 'd':
		runas_daemon=1;
		break;
        case 'b':
		display_short=1;
		break;
        case 'c':
		display_format=DISP_FORMAT_COLUMN;
		break;
        case 'k':
		bShowKeyCounters = NKN_TRUE;
		search[0] = (char *)"glob_tot_bytes_sent";
		search[1] = (char *)"glob_socket_cur_open";
		search[2] = (char *)"glob_tot_size_from_origin";
		search[3] = (char *)"glob_tot_size_from_cache";
		search[4] = NULL;
		break;
        case 'h':
		usage();
		break;
	case 'l':
		duration_time = atoi(optarg);
		break;
	case 'S':
		save_to_file_as_txt = optarg;
		break; 
	case 'z':
		display_zero = 0;
		break; 
        case 'v':
		version();
		break;
	case 'C':
		ReadUrl(optarg);
		break;
	}
}
	memset((char *)&last_bak[0], 0, MAX_CNTS_NVSD*sizeof(glob_item_t));
	memset((char *)&g_cnt_mark[0], 0, MAX_CNTS_NVSD*sizeof(int));

	/*
	 * load data from file only.
	 */
	if( from==FROM_FILE ) {
		assert(countfile!=NULL);

		scfd = file_read_head();
		mark_display_field(gpar.tot_cnts, &last_bak[0], search);
		if(display_format==DISP_FORMAT_COLUMN) {
			print_counter_name(gpar.tot_cnts, &last_bak[0]);
		}

		(list_countername) 
		? print_list_counter_names(gpar.tot_cnts, &last_bak[0])
		: file_display_counters();

		fclose(scfd);
		exit(1);
	}

	/*
	 * necessary unix application signaling handling.
	 */
	if(runas_daemon) daemonize();

	signal(SIGHUP,catcher);
	signal(SIGKILL,catcher);
	signal(SIGTERM,catcher);
	signal(SIGINT,catcher);


	/*
	 * Locate the segment.
	 */
	if (strcmp(producer,"/opt/nkn/sbin/nvsd")==0) {
		shmkey=NKN_SHMKEY;
		max_cnt_space = MAX_CNTS_NVSD;
	}
	else if (strcmp(producer,"/opt/nkn/sbin/ssld")==0) {
		shmkey=NKN_SSL_SHMKEY;
		max_cnt_space = MAX_CNTS_SSLD;

	}
	else if( (strstr(producer,"live_mfpd")) ||
		  (strstr(producer, "file_mfpd"))) {
	        if ((shmkey=ftok(producer,NKN_SHMKEY))<0)
		{
			char errbuf[256];
			snprintf(errbuf, sizeof(errbuf), "ftok failed, %s may not have a shared counter in this machine", producer);
                	perror(errbuf);
                	exit(1);
		}
		max_cnt_space = MAX_CNTS_MFP;
	}
	else if (strcmp(producer,"/opt/nkn/sbin/cb")==0) {
		shmkey=NKN_CB_SHMKEY;
		max_cnt_space = MAX_CNTS_CB;
	} else {
        	if((shmkey=ftok(producer,NKN_SHMKEY))<0)
        	{
			char errbuf[256];
			snprintf(errbuf, sizeof(errbuf), "ftok failed, %s may not have a shared counter in this machine", producer);
                	perror(errbuf);
                	exit(1);
        	}
		max_cnt_space = MAX_CNTS_OTHERS;
	}
	sh_size = (sizeof(nkn_shmdef_t)+max_cnt_space*(sizeof(glob_item_t)+30));
	

	if ((shmid = shmget(shmkey, sh_size, 0666)) < 0) {
		char errbuf[256];
		snprintf(errbuf, sizeof(errbuf), "shmget error, %s may not have a shared counter in this machine", producer);
		perror(errbuf);
		exit(1);
	}

	/*
	 * Now we attach the segment to our data space.
	 */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
		char errbuf[256];
		snprintf(errbuf, sizeof(errbuf), "shmat error, %s may not be running on this machine", producer);
		perror(errbuf);
		exit(1);
	}
again:
	pshmdef = (nkn_shmdef_t *) shm;
	g_allcnts = (glob_item_t *)(shm+sizeof(nkn_shmdef_t));
	varname = (char *)(shm+sizeof(nkn_shmdef_t)+sizeof(glob_item_t)*max_cnt_space);

	if( (strcmp(pshmdef->version, "nkn_cnt_1.2")!=0) && (strcmp(pshmdef->version, "nkn_cnt_1.1")!=0)) {
		printf("Version does not match. It requires %s\n", pshmdef->version);
		printf("This binary %s version is %s\n", argv[0], NKN_VERSION);
		exit(1);
	}

	/*
	 * open file for saving data
	 */
	if(countfile) {
		cnt_open_file();
	}

	if (save_to_file_as_txt)
	{
		save_as_txt_fd= fopen(save_to_file_as_txt , "w");
		if( save_as_txt_fd == NULL )
		{
			printf("failed to create file %s\n", save_to_file_as_txt);
			exit(14);
		}
	}


	/*
	 * search counter names.
	 */
	mark_display_field(pshmdef->tot_cnts, &g_allcnts[0], search);
	revision = pshmdef->revision;

	/*
	 * list counter name only.
	 */
	if(list_countername) {
		print_list_counter_names(pshmdef->tot_cnts, &g_allcnts[0]);
		exit(1);
	}

	/*
	 * infinite loop to display/save counter values.
	 */
	while(1)
	{

		/*
		 * Display counter name line.
		 * For every 10 lines print the header 
		 */
		if(nCount==0 && display_format==DISP_FORMAT_COLUMN) {
			if (bShowKeyCounters) {
				printf("\t%s\n", szKeyCountersHeading);
			} else {
				print_counter_name(pshmdef->tot_cnts, &g_allcnts[0]);
			}
		}

		/*
		 * print out counters to console or save to file.
		 */
		cnt_backup_data(pshmdef->tot_cnts, &g_allcnts[0], &last_bak[0]);
		if(countfile) {
			savetofile();
		}
		else if (save_to_file_as_txt){
			shm_display_and_save_counters_astxt();
		} else {
			shm_display_counters();
		}

		nCount++;
		nCount = nCount % 20;
		/*
		 * if sleep_time is zero, we only display once.
		 */
		if (0 == sleep_time) 
			break ; /* come out after one time */

		cummulative_time += sleep_time;
		/* overflow == break or condition for duration is met */
		if ((cummulative_time < 0) || (cummulative_time >= duration_time))
			break;

		sleep(sleep_time);

		// Check if counters have changed
		if ((bShowKeyCounters != NKN_TRUE) && (revision != pshmdef->revision)) {
			if(countfile) {
				flushbuffertofile();
				fclose(scfd);
				gzip_cnt_file();
				goto again;
			}
                        mark_display_field(pshmdef->tot_cnts, &g_allcnts[0], search);
      			revision = pshmdef->revision;
		}
	}

	if(scfd) {
		flushbuffertofile();
		fclose(scfd);
		gzip_cnt_file();
	}
	if(save_as_txt_fd) 
	{
		fflush(save_as_txt_fd);
		fclose(save_as_txt_fd);
        	syslog(LOG_NOTICE,("Generated counters dump file %s\n"), basename(save_to_file_as_txt));
	}
	
	exit(0);
}


static int ReadUrl(const char *fname)
{
    FILE *f = NULL;
    char url[1024];
    int n = 0;

    if (fname == NULL)
	return 0;

    f = fopen(fname, "r");
    if (f == NULL) {
	return 0;
    }

    n = fscanf(f, "%s", url);
    if ( n <= 0) {
	return 0;
    }

    if (URL) {
	free(URL);
	URL = NULL;
    }

    if (n)
	URL = strdup(url);

    fclose(f);
    return 0;

}

/*
 *
 * Filename:  unit_test.c
 * Date:      2009/02/06
 * Module:    Unit Tests for FLV Pre-Processing Module
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */



#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <getopt.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

#include "nkn_vpe_file_io.h"
#include "nkn_vpe_flv.h"
#include "nkn_vpe_bitstream.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_pre_proc.h"
#include "nkn_defs.h"

#define MAX_ARGS 30

typedef struct _tag_cmd_line_params{
    char desc[10*1024];
    char *argvt[255];
    int argct;
    int error_no;
}cmd_line_params;


/********************************************************************
 *                         FUNCTION PROTOTYPES
 *******************************************************************/

void UT_PRINT_RESULT(char);
int run_unit_tests(FILE *);

void
UT_PRINT_RESULT(char rv)
{
    rv==1?printf("TEST RESULT:SUCCESS\n"):printf("TEST RESULT:FAIL\n");
}


#ifdef UNIT_TEST


int run_unit_tests(FILE *fp)
{

    int i;
    char *ptr;
    int test_num;
    cmd_line_params cmd;
    int bytes;

    bytes = 0;
    test_num = 1;
    i = 0;
    ptr = NULL;

    memset(&cmd, 0, sizeof(cmd_line_params));


    ptr=cmd.argvt[0]=cmd.desc;
    while(1){

	if(bytes == 18){
	    printf("hi\n");
	}

	//ptr=cmd.desc;
	*ptr=fgetc(fp);
	bytes++;
	
	if(feof(fp))
	    break;

	if((*ptr)== 0x0d){
	    ptr++;
	    if((*ptr) == 0x0d){
		// cmd.argvt[i+1]=ptr+1;
		 *ptr='\0';
		 i++;
	    }
	}else if((*ptr)== 0x0a){
	    cmd.argvt[i+1]=ptr+1;
	    *(ptr)='\0';
	    ptr++;
	    i++;
	}else if((*ptr)=='#'){
	    cmd.argct=i-2;
	    i=0;
	    printf("\n\nTEST NO %d -  %s\n", test_num++, cmd.argvt[0]);
	    UT_PRINT_RESULT((ut_main(cmd.argct, &cmd.argvt[1]))== -1);
	    memset(&cmd,0,sizeof(cmd));
	    cmd.argvt[0]=cmd.desc;
	    ptr=cmd.desc;
	    fseek(fp, 1, SEEK_CUR);
	    optind = 0;
	    //optreset = 1;// reset getopt, very important to run unit tests!
	}else{
	    ptr++;

	}

    }

    return 1;
}

#endif

#ifdef UNIT_TEST

int main(int argc,char *argv[])
{

    FILE *fp;;
    
    UNUSED_ARGUMENT(argc);
    UNUSED_ARGUMENT(argv);

    fp = NULL;
    fp=fopen("../unittest.txt","rb");
 
    if(fp==NULL){
	   printf("\nInput test argument file is missing\n");
	   return 0;
    }


    run_unit_tests(fp);



    /* if(res==0){
       printf("\nError in test function\n");
       return 0;}*/

 
    /*! SANITY TESTS VIA MAIN FUNCTION */
    /* TEST 1 - illegal file path test */
    /* TEST 2 - faulty arg test */
    /* TEST 3 - Wrong number of Arguments */

    /*FLV FILE FORMAT HANDLING TESTS*/ 
    /* TEST 4 - 0 byte file test*/ 

    fclose(fp);
    return 1;
}
#endif




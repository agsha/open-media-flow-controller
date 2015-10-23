#include <stdio.h>

#include "includes.h"

int main(int argc,
	 char *argv[])
{
    bptr = NULL;
    unsigned long exp_num_exts=0;
    unsigned long num_blks=0;
    unsigned short type=0;
    char dir_name[20];
    unsigned int i=0;
 
    long int lret;

    if (argc != 2) {
	fprintf(stdout, "Usage : %s <disk cache directory>\n", argv[0]);
	exit(-1);
    }

    strcpy(dir_name, argv[1]);
    getfile(dir_name);

    num_blks = 0;
    type = 1;
    for (i = 0; i < g_tot_containers; i++) {
	exp_num_exts = (cnptr+i)->num_exts;
        (cnptr+i)->fid=fopen((cnptr+i)->name,"r");
        if((cnptr+i)->fid==NULL) {
            fprintf(stdout,"Unable to open container file %s for reading\n",(cnptr+i)->name);
            exit(-1);
        }
	lret = readContainerFile((cnptr+i)->fid, exp_num_exts, num_blks,
				 (cnptr+i)->uri_prefix, type); 
	if ((lret < 0) && (lret > -9999)) {
	    fprintf(stderr, "Error returned from readContainerFile : %ld\n",
		    lret);
	    exit(-1);
	} else {
	    num_blks = (unsigned long)lret;
	    fprintf(stdout, "Number of blocks = %lu\n", num_blks);
	}
        if((cnptr+i)->fid) {
            fclose((cnptr+i)->fid);
        }
    }
    verifyContainerOverlap(num_blks);

    type = 1;
    //  printContainers(num_blks,type);
    return (0);
}

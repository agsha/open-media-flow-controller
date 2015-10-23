#include "includes.h"

long int readContainerFile(FILE *cfid,
			   unsigned long exp_num_exts,
			   unsigned long num_blks,
			   char *uri_prefix,
			   unsigned short type __attribute((unused)))
{

  unsigned long long foffset=0;
  unsigned long i=0,j=0;
  int ret=0,blkpresent=0;

  DM2_disk_extent_t tmpext;
  unsigned long long tblk=0x0;
  ExtlistPtr textlistptr,newextlistptr;
  unsigned short tstatus=0;

  if(bptr==NULL) {
     bptr=(BlkPtr)malloc(100*sizeof(Blk));
     if(bptr==NULL) {
        fprintf(stderr,"Unable to allocate memory for bptr\n");
        return(-5);
     }
     g_num_block_alloc+=100;
  }

  foffset=sizeof(dm2_container_t);
  for(i=0;i<exp_num_exts;i++) {
    fseek(cfid,(long)(foffset+(i*DEV_BSIZE)),SEEK_SET);
//fprintf(stdout,"iteration %lu exp_num_exts %lu seeking %llu\n",i,exp_num_exts,(long)(foffset+(i*DEV_BSIZE)));
    ret=fread(&tmpext,1,sizeof(DM2_disk_extent_t),cfid);
    if(ret!=sizeof(DM2_disk_extent_t)) {
      fprintf(stdout,"Read of DM2_disk_extent_t failed at offset %llu\nRead bytes : %d\n",foffset+(i*DEV_BSIZE),ret);
      return(-2);
    }
    if(tmpext.dext_header.dext_magic==0x44455854) {
        tstatus=1;
    } else if (tmpext.dext_header.dext_magic==0x444F4E45) {
        tstatus=2;
    } else {
        fprintf(stderr,"BAD MAGIC : extent index = %lu expected 0x44455854 or 0x444F4E45 : got %X\n",i,tmpext.dext_header.dext_magic);
        return(-3);
    }
    tblk=tmpext.dext_physid;
    blkpresent=0;
    for(j=0;j<num_blks;j++) {
      if((bptr+j)->blockid==tblk) {
        blkpresent=1;
        textlistptr=(bptr+j)->extlist;
        while(textlistptr->next!=NULL) {
           textlistptr=textlistptr->next;
        }
        newextlistptr=(ExtlistPtr)malloc(sizeof(Extlist));
        if(newextlistptr==NULL) {
          fprintf(stderr,"Unable to allocate memory for textlistptr\n");
          return(-5);
        }
        newextlistptr->start_sector=tmpext.dext_start_sector;
        newextlistptr->contentoffset=tmpext.dext_offset;
        newextlistptr->contentlength=tmpext.dext_length;
        newextlistptr->status=tstatus;
        newextlistptr->uri=(char *)malloc(256);
        if(newextlistptr->uri==NULL) {
          fprintf(stderr,"Unable to allocate memory for textlistptr->uri\n");
          return(-5);
        }
        strcpy(newextlistptr->uri,uri_prefix);
        strcat(newextlistptr->uri,tmpext.dext_basename);
        newextlistptr->next=NULL;
        textlistptr->next=newextlistptr;
      }
    }
    if(!blkpresent) {
      if(num_blks>=g_num_block_alloc) {
         bptr=realloc(bptr,(g_num_block_alloc+100)*sizeof(Blk));
         if(bptr==NULL) {
            fprintf(stderr,"Unable to allocate memory for bptr\n");
            return(-5);
         }
         g_num_block_alloc+=100;
      }
      (bptr+num_blks)->blockid=tmpext.dext_physid;
      textlistptr=(ExtlistPtr)malloc(sizeof(Extlist));
      if(textlistptr==NULL) {
        fprintf(stderr,"Unable to allocate memory for textlistptr\n");
        return(-5);
      }
      textlistptr->start_sector=tmpext.dext_start_sector;
      textlistptr->contentoffset=tmpext.dext_offset;
      textlistptr->contentlength=tmpext.dext_length;
      textlistptr->status=tstatus;
      textlistptr->uri=(char *)malloc(256);
      if(textlistptr->uri==NULL) {
        fprintf(stderr,"Unable to allocate memory for textlistptr->uri\n");
        return(-5);
      }
      strcpy(textlistptr->uri,uri_prefix);
      strcat(textlistptr->uri,tmpext.dext_basename);
      textlistptr->next=NULL;
      (bptr+num_blks)->extlist=textlistptr;
      num_blks++;
    }
  }
  return (long int)num_blks;
}

int printContainers(unsigned long num_blks,
		    unsigned short type __attribute((unused)))
{
  unsigned long i=0,slno=0;
  ExtlistPtr textlistptr;
  fprintf(stdout,"SlNo,BlockID,ContentOffset,ContentLength,StartSector,URI\n");
  for(i=0;i<num_blks;i++) {
    textlistptr=(bptr+i)->extlist; 
    while(textlistptr!=NULL) {
      fprintf(stdout,"%ld,%llu, %llu, %llu, %llu, %s\n",
          slno,(bptr+i)->blockid,textlistptr->contentoffset,textlistptr->contentlength,
           textlistptr->start_sector,textlistptr->uri);
      textlistptr=textlistptr->next;
      slno++;
    }
  }
  return(0);
}

char cur_path[512];
char cur_cont[512];
static int process_inode(const char *fpath,
                         const struct stat *sb,
                         int typeflag)

{
    char *slash,*tfpath;
    char *str;
    int ret=sizeof(sb);
    struct stat fbuf;
    unsigned long long fsize=0;
    if((tfpath=malloc(512))==NULL) {
        fprintf(stderr,"Unable to allocate memory for tfpath\n");
        exit(-1);
    }

    if((str=(strstr(fpath,"/attributes")))!=NULL) {
       free(tfpath);
       return 0;
    }
    if((str=(strstr(fpath,"/freeblks")))!=NULL) {
       free(tfpath);
       return 0;
    }
    if((str=(strstr(fpath,"/core")))!=NULL) {
       free(tfpath);
       return 0;
    }
    if ((typeflag & FTW_NS) == FTW_NS)  // Stat failed: shouldn't happen
        assert(0);
    if ((typeflag & FTW_DNR) == FTW_DNR)// Can not read: shouldn't happen
        assert(0);
    if ((typeflag & FTW_D) == FTW_D)    // Directory
    {
      if(strcmp(cur_path,fpath)) {
        strcpy(cur_path,fpath);
        ftw(fpath,process_inode,10);
      }
        free(tfpath);
        return 0;
    }
    if(strcmp(cur_cont,fpath)) {
      strcpy(cur_cont,fpath);
      slash = strrchr(fpath, '/');
      if (slash == NULL)          // Can not find /: shouldn't happen
        assert(0);
      if (*(slash+1) == '\0')     // mount point
        return 0;
//      fprintf(stdout,"FNAME : %s\n",fpath);
              if(cnptr==NULL) {
                cnptr=(ContNamePtr)malloc(1*sizeof(ContName));
              } else {
                cnptr=(ContNamePtr)realloc(cnptr,(g_num_contname_alloc+1)*sizeof(ContName));
              }
              if(cnptr==NULL) {
                 fprintf(stderr,"Unable to allocate memory for cnptr\n");
                 exit(-1);
              }
              g_num_contname_alloc++;
              if(((cnptr+g_tot_containers)->name=malloc(strlen(fpath)+1))==NULL) {
                fprintf(stderr,"Unable to allocate memory for cnptr->name\n");
                exit(-1);
              }
              if(((cnptr+g_tot_containers)->uri_prefix=malloc(strlen(fpath)+1))==NULL) {
                fprintf(stderr,"Unable to allocate memory for cnptr->uri_prefix\n");
                exit(-1);
              }
              strcpy((cnptr+g_tot_containers)->name,fpath);
              tfpath=strstr((cnptr+g_tot_containers)->name,"dc_");
              if(tfpath==NULL) {
                 fprintf(stderr,"The file %s does not seem to be from a valid MFD cache\nString dc_ missing in filename\n",(cnptr+g_tot_containers)->name);
                 exit(-1);
              }
              strcpy((cnptr+g_tot_containers)->uri_prefix,tfpath);
              tfpath=strstr((cnptr+g_tot_containers)->uri_prefix,"/");
              strcpy((cnptr+g_tot_containers)->uri_prefix,tfpath);
              tfpath=strstr((cnptr+g_tot_containers)->uri_prefix,"/container");
              (cnptr+g_tot_containers)->uri_prefix[strlen((cnptr+g_tot_containers)->uri_prefix)-strlen(tfpath)]='\0';
//              (cnptr+g_tot_containers)->fid=fopen((cnptr+g_tot_containers)->name,"r");
//              if((cnptr+g_tot_containers)->fid==NULL) {
//                 fprintf(stdout,"Unable to open container file %s for reading\n",(cnptr+g_tot_containers)->name);
//                 exit(-1);
//              }
              ret=stat((const char *)(cnptr+g_tot_containers)->name,&fbuf);
              if(ret<0) {
                  fprintf(stdout,"stat returned %d for file %s\n",ret,(cnptr+g_tot_containers)->name);
                  return 0;
              }
              fsize=fbuf.st_size;
              if(fsize<sizeof(dm2_container_t)) {
                  fprintf(stdout,"File %s size is too small to be a container file\n",(cnptr+g_tot_containers)->name);
                  return 0;
              } else {
                  (cnptr+g_tot_containers)->num_exts=(fsize-sizeof(dm2_container_t))/DEV_BSIZE;
              }
              if((((cnptr+g_tot_containers)->num_exts*DEV_BSIZE)+sizeof(dm2_container_t))!=fsize) {
                  fprintf(stdout,"File %s has extra bytes at the end\n",(cnptr+g_tot_containers)->name);
                  return 0;
              }

/*
*/
              g_tot_containers++;

    }
  return 0;
}


void getfile(char *dir) {
  strcpy(cur_path,dir);
 ftw(dir,process_inode,10);

}




int updateExtentBitmap(unsigned long long contentlength,unsigned long long start_sector,unsigned long long blockid, char *uri) {
  unsigned int k;
  unsigned int st_page=0;
  unsigned int num_pages=0;
  unsigned int local_err=0;
  num_pages=ceil((double)(contentlength)/32768.0);
  if(((start_sector)%4096)%64 != 0) {
    fprintf(stdout,"ERROR : dext_start_sector %llu in block %llx is not a mod 64 number\n",start_sector,blockid);
  }
  st_page=((start_sector)%4096)/64;
  if(st_page+num_pages > 64) {
     fprintf(stdout,"ERROR : number of pages in block %llx exceeds 64\n",blockid);
     return(-1);
  }
  for(k=st_page;k<num_pages;k++) {
    ExtentBitmap[k]++;
    if(ExtentBitmap[k]>1) {
       fprintf(stdout,"ERROR : Pages %u in block %llx has duplicate reference - file name = %s\n",k,blockid,uri);
       fprintf(stdout,"Bitmap value = %u\n",ExtentBitmap[k]);
       local_err++;
    }
  }
  return (0);
}

int clearExtentBitmap(void) {
  memset(ExtentBitmap,0,64*sizeof(char));
  return(0);
}

int verifyContainerOverlap(unsigned long num_blks) {
  unsigned long i=0;
  ExtlistPtr textlistptr;
  for(i=0;i<num_blks;i++) {
    textlistptr=(bptr+i)->extlist;
    while(textlistptr!=NULL) {
      if(textlistptr->status == 1) {
       updateExtentBitmap(textlistptr->contentlength,textlistptr->start_sector,(bptr+i)->blockid, textlistptr->uri);
      }
       textlistptr=textlistptr->next;
    }
    clearExtentBitmap();
  }
  return(0);
}


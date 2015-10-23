#include <stdio.h>
#include <sys/param.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/errno.h>

#include "nkn_defs.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_attr.h"


typedef struct extlist_t {
  unsigned long long start_sector;
  unsigned long long contentoffset;
  unsigned long long contentlength;
  unsigned short     status; /* 0-unassigned, 1-active, 2-deleted */
  char		     *uri;
  struct extlist_t   *next;
} Extlist, *ExtlistPtr;

typedef struct block_t {
  unsigned long long blockid;
  ExtlistPtr	     extlist;
} Blk, *BlkPtr;

typedef struct attribute_t {
  char *uri;
  unsigned long long attrfileoffset;
  unsigned long long contentlength;
  unsigned short     status; /* 0-unassigned, 1-active, 2-deleted */
  time_t	     cache_expiry;
  uint64_t	     hotval;
} Attr, *AttrPtr;

typedef struct uritracker_t {
  char *uri;
  ExtlistPtr list;
}Uritrack, *UritrackPtr;

typedef struct container_name_t {
  char		*name;
  char		*uri_prefix;
  FILE		*fid;
  unsigned long num_exts;
} ContName, *ContNamePtr;

unsigned long g_num_block_alloc;
unsigned long g_num_contname_alloc;
unsigned int g_tot_containers;
BlkPtr bptr;
ContNamePtr cnptr;
char ExtentBitmap[64];

long int readContainerFile(FILE *cfid, unsigned long exp_num_exts,
			   unsigned long num_exts, char *uri_prefix,
			   unsigned short  type);

/* type=1->print all exts, type=2->print active exts, type=3->print deleted exts */
int printContainers(unsigned long num_blks, unsigned short type);
void getfile(char *dir);
int updateExtentBitmap(unsigned long long contentlength,
		       unsigned long long start_sector,
		       unsigned long long blockid, char *uri);
int clearExtentBitmap(void);
int verifyContainerOverlap(unsigned long num_blks);

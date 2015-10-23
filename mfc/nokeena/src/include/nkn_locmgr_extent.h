#ifndef NKN_LOCMGR_EXTENT_H
#define NKN_LOCMGR_EXTENT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "nkn_defs.h"


/* TBD: Revisit this number. Can we afford to have a fixed number of extents */
#define NKN_SYSTEM_MAX_EXTENTS 1000000

/*
	This structure contains information about a URI.
	The URI could be contained across virtual blocks (physid).
	Hence, many of these structures could constitute a single URI.
*/
typedef struct LM_extent {
	nkn_uol_t uol;
	char	  physid[MAX_URI_SIZE];
	char	  filename[MAX_URI_SIZE];
	uint64_t  extent_id;
	uint64_t  file_offset;
	uint64_t  tot_content_len;		// not used
	uint32_t  inUse;
} LM_extent_t;

typedef uint64_t nkn_lm_extent_id_t;

void   nkn_lm_extent_dump_counters(void);
int    nkn_lm_extent_init(void);
struct LM_extent *nkn_lm_extent_get(nkn_lm_extent_id_t in_extent_id);
nkn_lm_extent_id_t nkn_lm_extent_create_new(char *physid,
					    uint32_t uri_offset,
					    uint32_t uri_len);
void nkn_lm_extent_free(nkn_lm_extent_id_t id);

//void nkn_lm_get_extents_by_uri(char *in_uriname, GList **out_extent_list);
GList *nkn_lm_get_extents_by_uri(char *in_uriname);
void nkn_lm_get_extents_by_physid(char *in_physid, GList **out_extent_list);

void nkn_lm_set_extent_offset(char *in_uri, struct LM_extent *in_extent,
			      uint64_t in_uri_offset);
int nkn_lm_extent_insert_by_uri(char *in_string, struct LM_extent *in_extent);
int nkn_lm_extent_insert_by_physid(char *in_string, struct LM_extent *in_extent);

int nkn_lm_extent_delete_all_by_uri(char *in_string);
int nkn_lm_extent_delete_all_by_physid(char *in_string);

#endif	/* NKN_LOCMGR_EXTENT_H */


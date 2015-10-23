#ifndef __NKN_STAT__H
#define __NKN_STAT__H

#include <stdint.h>
/*
 * Interface for a library to publish monitoring data via shared
 * memory.  This can be used by a daemon process (e.g. nvsd) to publish
 * monitoring data that is statically or dynamically defined.  
 * Multiple processes can then subscribe to read the data via the library.
 *
 * Each logical collection of monitors is a region identified by a key (which is
 * the underlying shared memory key).  Each process has a default region that
 * can accessed via a NULL parameter.
 *
 * The basic unit of publishing data is variable that is identified with a
 * string name.   The ID for a statically defined variable is the variable
 * name itself.  Dynamically defined variables are given IDs via a
 * registration function.
 *
 * Multiple instances of an ID can be published using a string to identify
 * each instance.
 *
 * A subscriber can find iterate thru instances of an ID.
 *
 * Publish/Access to an object is provided by copying (rather than direct access
 * to the shared memory).  This allows the library to provide consistency
 * checking.  Also, the library becomes non-critical to the base execution of
 * the daemon.
 */

/*
 * Add a named variable to the region.
 * A variable can be a base type or a structure.  If a structure is used,
 * it must be defined in an external header so that a client can access its
 * members.
 *
 * The "instance" provides support for publishing variables
 * that can have many instances - e.g. one per resource like a disk.  It should
 * be set to NULL for single instance variables.
 *
 * The delete interface MUST be called before the variable is deallocated.
 * Otherwise, we can crash the process when the variable is copied or else
 * publish bad data.
 */

#include <inttypes.h>
#ifdef LIB_NET
#include "network_int.h"
#endif

int nkn_mon_add(const char *name, const char *instance, void *obj, uint32_t size);
void nkn_mon_delete(const char *name, const char *instance);
void nkn_mon_get(const char *name, const char *instance, void *obj, uint32_t *size);
void nkn_mon_reset(const char *name, const char *instance);

#define MAX_CTR_NAME_LEN 512

/* Macros for statically defined variables */

#define NKNCNT_DEF(name, type, unit, comment) \
	type glob_##name = 0; \
	const char * gunit_##name = unit; \
	const char * gcomment_##name = comment;

#define NKNCNT_EXTERN(name, type) \
	extern type glob_##name;

#define NKNCNT_INIT(name) \
	glob_##name = 0;

#define NKNCNT_PLUS(name, val) \
	glob_##name += val;

#define NKNCNT_MINUS(name, val) \
	glob_##name -= val;

#define NKNCNT_INC(name) \
	glob_##name ++;

#define NKNCNT_DEC(name) \
	glob_##name --;

typedef struct glob_item {
	int32_t name;	// offset to the varname
	void * addr;
	char * comment;
	uint32_t reserved;
	int32_t next_deleted_counter;
	int32_t size;
	uint64_t value;
} glob_item_t;

#define MAX_CNTS_NVSD	500000
#define MAX_CNTS_OTHERS	10000
#define MAX_CNTS_SSLD	10000
#define MAX_CNTS_MFP    30000

//
// The following structure is defined for shared memory
//
#define NKN_VERSION "nkn_cnt_1.2"
typedef struct nkn_shmdef {
	char version[16];
	int tot_cnts;		// total number of entries
	int static_cnts;	// number of static entries
	int revision;		// incremented on entry add/delete
} nkn_shmdef_t;

// Assuming that average sizeo of each variable name is 20.
//#define NKN_SHMSZ       (sizeof(nkn_shmdef_t)+MAX_CNTS*(sizeof(glob_item_t)+30))
#define NKN_SHMKEY      5680
#define NKN_OOM_SHMKEY	5681
#define NKN_SSL_SHMKEY  5682
#endif // __NKN_STAT__H

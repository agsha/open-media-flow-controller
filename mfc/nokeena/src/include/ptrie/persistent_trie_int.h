/*
 * persistent_trie_int.h -- Persistent Trie internal definitions
 */

#ifndef _PERSISTENT_TRIE_INT_H
#define _PERSISTENT_TRIE_INT_H

#include <sys/types.h>
#include <sys/param.h>
#include "atomic_ops.h"
#include "cprops/collection.h"
#include "cprops/trie.h"

/*
 * Patricia tree checkpoint file definitions
 *
 * File layout:
 *  file_header_t #1 offset=0
 *  file_header_t #2 offset=META_HDRSIZE - DEV_BSIZE
 *
 *  file_node_data_t
 *  ...
 */
typedef struct fh_user_data {
    long data[16]; // strict alignment
} fh_user_data_t;

typedef struct file_header {
    union {
    	struct hdrdata { // 512 bytes
	    int version;
	    int pad1;
	    long magicno;
	    long seqno;
	    struct timespec ts;
	    off_t filesize;
	    fh_user_data_t ud;
	    char pad2[336];
	} hdr;
	char raw_hdr[512];
    } u;
} file_header_t;

#define FH_VERSION 	0x10000000 // 32 bits
#define FH_MAGICNO 	0x1301201212340001

#define META_HDRSIZE (1024 * 1024)
#define METAHDR_FOFF (0)
#define FHDR_1_FOFF (METAHDR_FOFF)
#define FHDR_2_FOFF (METAHDR_FOFF + META_HDRSIZE - DEV_BSIZE)

#define LOFF2FOFF(_off) ((_off) + META_HDRSIZE)
#define FOFF2LOFF(_off) ((_off) - META_HDRSIZE)

#define KEYMAXSIZE ((2*1024)-1)

/* 
 * Patricia tree memory node definitions
 */
typedef struct data_header {
    long 	magicno;
    off_t 	loff; // Checkpoint file loff
    int 	key_strlen;
    int		pad;
    long 	incarnation;
    long 	flags;
} data_header_t;

#define DH_MAGICNO 	(long)0x1301201212340002
#define DH_MAGICNO_FREE (long)0xdeadbeefdeadbeef

typedef struct app_data {
    long d[16]; // strict alignment
} app_data_t;

typedef struct node_data { // 176 bytes
    data_header_t dh;
    app_data_t ad;
    void *ctx;
} node_data_t;

typedef struct file_node_data { // Disk copy
    node_data_t nd; // always first
    char key[KEYMAXSIZE];
    char key_null;
} file_node_data_t;

/*
 * Checkpoint file memory definitions
 */
typedef struct ckpt_file_bmap {
    int entries;
    int mapsize;
    char *map;
} ckpt_file_bmap_t;

typedef struct ckpt_file_data {
    int file_fd;
    off_t next_entry_loff;
    ckpt_file_bmap_t freemap;
    file_header_t fhd1;
    file_header_t fhd2;
} ckpt_file_data_t;

typedef struct trie_data {
    AO_t refcnt;
    cp_trie *trie;
} trie_data_t;

/*
 * Transaction memory log definitions
 */
typedef enum ptrie_xaction_type {
    PT_XC_ADD=1,
    PT_XC_UPDATE,
    PT_XC_DELETE,
    PT_XC_RESET
} ptrie_xaction_type_t;

typedef struct trie_xaction_entry {
    ptrie_xaction_type_t type;
    file_node_data_t fn;
} trie_xaction_entry_t;

#define MAX_XACTION_ENTRIES 100000
typedef struct trie_xaction_log {
    int entries;
    trie_xaction_entry_t en[MAX_XACTION_ENTRIES];
} trie_xaction_log_t;

#define MAX_TRIE_ENTRIES (8 * 1024 * 1024)
#define MAX_CKPT_FILESIZE (META_HDRSIZE + \
				(MAX_TRIE_ENTRIES * sizeof(file_node_data_t)))

/* 
 * Persistent Trie memory context definitions
 */
typedef struct ptrie_context {
    ckpt_file_data_t *cur_ckpt_file;
    ckpt_file_data_t *shdw_ckpt_file;
    ckpt_file_data_t f[2];

    off_t bi_next_entry_loff; // Before image ckpt file next_entry_loff
    ckpt_file_bmap_t bi_ckpt_file_bmap; // Before image ckpt file freemap

    AO_t active_td; // (trie_data_t *)
    trie_data_t *cur_td; 
    trie_data_t *shdw_td; 
    trie_data_t td[2];

    long fnode_incarnation;

    AO_t in_xaction;
    trie_xaction_log_t *log;

    void (*copy_app_data)(const app_data_t *src, app_data_t *dest);
    void (*destruct_app_data)(app_data_t *d);
} ptrie_context_t;

typedef enum ptrie_log_level {
    PT_LOGL_ALARM = 0,
    PT_LOGL_SEVERE = 1,
    PT_LOGL_ERROR = 2,
    PT_LOGL_WARNING = 3,
    PT_LOGL_MSG = 4,
    PT_LOGL_MAX, // Always last
} ptrie_log_level_t;

#endif /* _PERSISTENT_TRIE_INT_H */

/*
 * End of persistent_trie_int.h
 */

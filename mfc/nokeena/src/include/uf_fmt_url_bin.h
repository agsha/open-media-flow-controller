/*
 * uf_fmt_url_bin.h -- URL filter format URL
 */
#ifndef _UF_FMT_URL_BIN_H
#define _UF_FMT_URL_BIN_H

#include <stdint.h>

#define BIN_UF_FMT_URL_MAGIC 0x20141234ffffeeee
#define BIN_UF_FMT_URL_VERS_MAJ 1 
#define BIN_UF_FMT_URL_VERS_MIN 0

#define BIN_UF_FMT_URL_OPT_LE (1 << 0) // Little Endian
#define BIN_UF_FMT_URL_OPT_BE (1 << 0) // Big Endian

#define UF_URL_FIRST_RECORD(_addr_hdr) (bin_uf_fmt_url_rec_t *) \
    (((char *)(_addr_hdr)) + sizeof(bin_uf_fmt_url_hdr_t))

#define UF_URL_NXT_RECORD(_addr_rec) (bin_uf_fmt_url_rec_t *) \
    ( ((char *)(_addr_rec)) + sizeof(bin_uf_fmt_url_rec_t) +  \
      ((_addr_rec)->sizeof_data - 1) )

/* Note: EOF denoted with bin_uf_fmt_url_rec_t.sizeof_data == 0 */

typedef struct bin_uf_fmt_url_hdr {
    uint64_t magicno;
    uint32_t version_maj;
    uint32_t version_min;
    uint64_t options;
} bin_uf_fmt_url_hdr_t;

typedef struct bin_uf_fmt_url_rec {
    uint32_t flags;
    uint32_t sizeof_data;
    char data[1]; // variable length includes NULL
} bin_uf_fmt_url_rec_t;

#endif /* _UF_FMT_URL_BIN_H */

/*
 * End of uf_fmt_url_bin.h
 */

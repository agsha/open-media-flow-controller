/*
 *******************************************************************************
 * host_origin_map_bin.h -- Binary representation of HostOriginMap XML
 *******************************************************************************
 */

#ifndef _HOST_ORIGIN_MAP_BIN_H
#define _HOST_ORIGIN_MAP_BIN_H

#include <stdint.h>
#include "xml_map_bin.h"

#define HOM_VERSION 1
#define HOM_MAGICNO 0xabcd1234

typedef struct host2origin {
    istr_ix_t host;
    istr_ix_t origin_host;
    uint16_t origin_port; // native byte order
    uint16_t pad;
    uint32_t options;
} host2origin_t;

typedef struct host_origin_map_bin {
    /* Note: All numeric quantities stored in native byte order */
    uint32_t version;
    uint32_t magicno;
    uint32_t string_table_offset; // offset from start of struct
    uint32_t string_table_length;
    uint32_t num_entries;
    host2origin_t entry[1];
    /* Variable length string table resides here */
} host_origin_map_bin_t;

#endif /* _HOST_ORIGIN_MAP_BIN_H */

/*
 * End of host_origin_map_bin.h
 */

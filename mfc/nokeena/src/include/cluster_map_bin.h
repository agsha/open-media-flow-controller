/*
 *******************************************************************************
 * cluster_map_bin.h -- Binary representation of ClusterMap XML
 *******************************************************************************
 */

#ifndef _CLUSTER_MAP_BIN_H
#define _CLUSTER_MAP_BIN_H

#include <stdint.h>
#include "xml_map_bin.h"

#define CLM_VERSION 1
#define CLM_MAGICNO 0xabcd5678

/*
 * Options attribute name/value fields
 */
#define CLM_OPTIONS_HEARTBEATHPATH "heartbeatpath"

typedef struct clustermap {
    istr_ix_t nodename;
    istr_ix_t ipaddr;
    istr_ix_t options;
    uint16_t port; // native byte order
    uint16_t pad;
} clustermap_t;

typedef struct cluster_map_bin {
    /* Note: All numeric quantities stored in native byte order */
    uint32_t version;
    uint32_t magicno;
    uint32_t string_table_offset; // offset from start of struct
    uint32_t string_table_length;
    uint32_t num_entries;
    clustermap_t entry[1];
    /* Variable length string table resides here */
} cluster_map_bin_t;

#endif /* _CLUSTER_MAP_BIN_H */

/*
 * End of cluster_map_bin.h
 */

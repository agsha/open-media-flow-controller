/*
 *******************************************************************************
 * origin_round_robin_map_bin.h -- Binary representation of 
 *				   OriginRoundRobinMap XML
 *******************************************************************************
 */

#ifndef _ORIGIN_ROUND_ROBIN_MAP_BIN_H
#define _ORIGIN_ROUND_ROBIN_MAP_BIN_H

#include <stdint.h>
#include "xml_map_bin.h"

#define ORRRM_VERSION 1
#define ORRRM_MAGICNO 0xabcddefa

/*
 * Options attribute name/value fields
 */
#define ORRRM_OPTIONS_HEARTBEATHPATH "heartbeatpath"
#define ORRRM_OPTIONS_RRWEIGHT "round_robin_weight"

typedef struct origin_round_robinmap {
    istr_ix_t origin;
    istr_ix_t options;
    uint16_t port; // native byte order
    uint16_t pad;
} origin_round_robinmap_t;

typedef struct origin_round_robin_map_bin {
    /* Note: All numeric quantities stored in native byte order */
    uint32_t version;
    uint32_t magicno;
    uint32_t string_table_offset; // offset from start of struct
    uint32_t string_table_length;
    uint32_t num_entries;
    origin_round_robinmap_t entry[1];
    /* Variable length string table resides here */
} origin_round_robin_map_bin_t;

#endif /* _ORIGIN_ROUND_ROBIN_MAP_BIN_H */

/*
 * End of origin_round_robin_map_bin.h
 */

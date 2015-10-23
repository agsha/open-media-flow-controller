/*
 *******************************************************************************
 * origin_escalation_map_bin.h -- Binary representation of 
 *				  OriginEscalationMap XML
 *******************************************************************************
 */

#ifndef _ORIGIN_ESCALATION_MAP_BIN_H
#define _ORIGIN_ESCALATION_MAP_BIN_H

#include <stdint.h>
#include "xml_map_bin.h"

#define OREM_VERSION 1
#define OREM_MAGICNO 0xabcd9abc

/*
 * Options attribute name/value fields
 */
#define OREM_OPTIONS_HEARTBEATHPATH "heartbeatpath"
#define OREM_OPTIONS_WEIGHT "weight"
#define OREM_OPTIONS_HTTP_RESPONSE_FAILURE_CODES "http_response_failure_codes"

typedef struct origin_escalationmap {
    istr_ix_t origin;
    istr_ix_t options;
    uint16_t port; // native byte order
    uint16_t pad;
} origin_escalationmap_t;

typedef struct origin_escalation_map_bin {
    /* Note: All numeric quantities stored in native byte order */
    uint32_t version;
    uint32_t magicno;
    uint32_t string_table_offset; // offset from start of struct
    uint32_t string_table_length;
    uint32_t num_entries;
    origin_escalationmap_t entry[1];
    /* Variable length string table resides here */
} origin_escalation_map_bin_t;

#endif /* _ORIGIN_ESCALATION_MAP_BIN_H */

/*
 * End of origin_escalation_map_bin.h
 */

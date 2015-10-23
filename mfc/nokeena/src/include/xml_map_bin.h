/*
 *******************************************************************************
 * xml_map_bin.h -- XML binary representation fundamental definitions
 *******************************************************************************
 */
#ifndef _XML_MAP_BIN_H
#define _XML_MAP_BIN_H
#include <stdint.h>

typedef uint32_t istr_ix_t;

#define ISTR(_struc, _stoff, _ix) \
        (char *)((char *)((_struc)) + (_struc)->_stoff +(_ix))

#endif /* _XML_MAP_BIN_H */

/*
 * End of xml_map_bin.h
 */

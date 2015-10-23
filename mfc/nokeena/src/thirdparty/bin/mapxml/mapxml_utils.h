/*
 * mapxml_utils.h -- Utility functions
 * Copyright (c) 2014 by Juniper Networks, Inc
 */
#ifndef _MAPXML_UTILS_H
#define _MAPXML_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xml_map_bin.h"

/*
 * add_str_table -- Add given string to passed string table
 * 	Input:
 *		str_table - String table
 *		str_table_bytes_used - size of string table
 *		data - NULL terminated string to add to string table
 *		
 *	Output:
 *		index - Index in string table for given string
 *
 *	Returns: 0 => Success
 */
int
add_str_table(char **str_table, int *str_table_size, int *str_table_bytes_used, 
	      istr_ix_t *index, const char *data);

/*
 * write_binary_file -- Write XML binary file consisting of meta data followed
 *    			by string table data.
 * 	Input:
 *		filename - output filename
 *		meta_data - Written first
 *		meta_datalen - Length in bytes
 *		str_table - Written second
 *		str_tablelen - Length in bytes
 *
 *	Returns: 0 => Success
 */
int 
write_binary_file(const char *filename, 
		  const char *meta_data, int meta_datalen,
		  const char *str_table, int str_tablelen);

#ifdef __cplusplus
}
#endif

#endif /* _MAPXML_UTILS_H */

/*
 * End of mapxml_utils.h
 */

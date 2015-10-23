/*
 * mapxml_utils.cpp - Support utilities
 * Copyright (c) 2014 by Juniper Networks, Inc
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "mapxml_utils.h"

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
add_str_table(char **str_table, 
	      int *str_table_size, int *str_table_bytes_used, 
	      istr_ix_t *index, const char *data)
{
    int datalen;

    if (!data) {
        return 1;
    }
    if (!(*str_table)) {
        int size = 32 * 1024;
      	*str_table = (char*)calloc(1, size);
	if (!(*str_table)) {
	    printf("calloc failed, size=%d", size);
	    return 2;
	}
      	*str_table_size = size;
      	*str_table_bytes_used = 0;
    }
    datalen = strlen(data);

    if ((datalen + 1 + *str_table_bytes_used) > *str_table_size) {
    	// Grow string table
    	char *p;
	int new_size = (*str_table_size) * 2;
	p = (char*)realloc(*str_table, new_size);
	if (!p) {
	    printf("realloc failed, size=%d newsize=%d", 
	    	   *str_table_size, new_size);
	    return 3;
	}
	*str_table = p;
	*str_table_size = new_size;
    }
    strcpy(&((*str_table)[*str_table_bytes_used]), data);
    *index = *str_table_bytes_used;
    *str_table_bytes_used += (datalen + 1);

    return 0;
}

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
		  const char *str_table, int str_tablelen)
{
    int ret = 0;
    int fd = -1;
    int bytes_sent;

    fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fd < 0) {
    	printf("open('%s') failed, errno=%d\n", filename, errno);
	ret = 1;
	goto exit;
    }

    bytes_sent = write(fd, meta_data, meta_datalen);
    if (bytes_sent != meta_datalen) {
    	printf("short header write('%s') bytes_sent=%d actual_bytes_sent=%d\n",
	       filename, meta_datalen, bytes_sent);
	ret = 2;
	goto exit;
    }

    bytes_sent = write(fd, str_table, str_tablelen);
    if (bytes_sent != str_tablelen) {
    	printf("short string table write('%s') "
	       "bytes_sent=%d actual_bytes_sent=%d\n",
	       filename, str_tablelen, bytes_sent);
	ret = 3;
	goto exit;
    }

exit:

    if (fd >= 0) {
    	close(fd);
    }
    return ret;
}

/*
 * End of mapxml_utils.cpp
 */

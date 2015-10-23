/*
 *
 * Filename:  nkn_vpe_backend_io_plugin.h
 * Date:      2009/02/06
 * Module:    Plugin architecture for IO
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef _NKN_BACKEND_IO_PLUGIN_
#define _NKN_BACKEND_IO_PLUGIN_

#include "stdio.h"
#include "inttypes.h"

#ifdef __cplusplus
extern "C"{
#endif

    /**I/O hanlders
     */
    typedef void* (*fnp_create_descriptor_from_file)(const char *, const char *mode);
    typedef void* (*fnp_create_raw_descriptor)(unsigned int init_size);
    typedef void* (*fnp_init_descriptor)(void *buf, size_t size);
    typedef size_t (*fnp_read_data)(void *desc, size_t n_elements, size_t element_size, void *src);
    typedef size_t (*fnp_write_data)(void *desc, size_t n_elements, size_t element_size, void *dst);
    typedef size_t (*fnp_seek)(void *desc, size_t skip_bytes, size_t start_from);
    typedef size_t (*fnp_tell)(void *desc);
    typedef size_t (*fnp_get_size)(void *desc);
    typedef size_t (*fnp_close_descriptor)(void *desc);
    typedef size_t (*fnp_write_raw_descriptor_to_file)(void *desc, char *);
    typedef size_t (*fnp_formatted_write)(void*, const char *, ...);

    /**Plugin structure
     */
    typedef struct _tag_backendIO{
	fnp_create_descriptor_from_file nkn_create_descriptor_from_file;
	fnp_create_raw_descriptor       nkn_create_raw_descriptor;
	fnp_init_descriptor             nkn_init_descriptor;
	fnp_read_data		        nkn_read;
	fnp_write_data		        nkn_write;
	fnp_seek		        nkn_seek;
	fnp_tell			nkn_tell;
	fnp_get_size		        nkn_get_size;
	fnp_close_descriptor	        nkn_close;
	fnp_write_raw_descriptor_to_file nkn_write_stream_to_file;
	fnp_formatted_write              nkn_formatted_write;
    }io_funcs;

    /**API to register the backend IO handlers
     */
    int register_backend_io_funcs(fnp_create_descriptor_from_file open_file,
				  fnp_create_raw_descriptor create_raw_bitstream, 
				  fnp_init_descriptor init_descriptor,
				  fnp_read_data read_data, 
				  fnp_write_data write_data,  
				  fnp_seek seek_data, 
				  fnp_tell tell,
				  fnp_get_size get_size,
				  fnp_close_descriptor close,
				  fnp_write_raw_descriptor_to_file nkn_write_stream_to_file,
				  fnp_formatted_write nkn_formatted_write,
				  io_funcs *iof);

#ifdef __cplusplus
}
#endif

#endif

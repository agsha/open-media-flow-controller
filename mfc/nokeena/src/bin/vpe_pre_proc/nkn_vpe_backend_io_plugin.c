/*
 *
 * Filename:  nkn_vpe_backend_io_plugin.c
 * Date:      2009/02/06
 * Module:    FLV pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "nkn_vpe_backend_io_plugin.h"

/** Registers the IO handlers
 *@return Returns the number of handlers registered
 */
int register_backend_io_funcs(fnp_create_descriptor_from_file open_file,
			      fnp_create_raw_descriptor open_stream, 
			      fnp_init_descriptor init_stream,
			      fnp_read_data read_data, 
			      fnp_write_data write_data,  
			      fnp_seek seek_data, 
			      fnp_tell tell,
			      fnp_get_size get_size,
			      fnp_close_descriptor close,
			      fnp_write_raw_descriptor_to_file write_stream_to_file,
			      fnp_formatted_write formatted_write,
			      io_funcs *iof) 
{
  unsigned int i;

  i = 0;
  iof->nkn_read = read_data;i++;
  iof->nkn_write = write_data;i++;
  iof->nkn_seek = seek_data;i++;
  iof->nkn_create_descriptor_from_file = open_file;i++;
  iof->nkn_create_raw_descriptor = open_stream;i++;
  iof->nkn_tell = tell;i++;
  iof->nkn_get_size = get_size;i++;
  iof->nkn_write_stream_to_file = write_stream_to_file;i++;
  iof->nkn_close = close;i++;
  iof->nkn_formatted_write = formatted_write;i++;
  iof->nkn_init_descriptor = init_stream;i++;

  return i;

	
}

/*
 *
 * Filename:  nkn_vpe_file_io.c
 * Date:      2009/02/06
 * Module:    File based IO
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include "nkn_vpe_file_io.h"

size_t nkn_read_file(void *src, size_t n_elements, size_t element_size, void *dst){

  return fread(src, n_elements, element_size, (FILE*)dst);

}

size_t nkn_write(void *src, size_t n_elements, size_t element_size, void *dst){

  return fwrite(src, n_elements, element_size, (FILE*)dst);

}

size_t nkn_seek(void *desc, size_t bytes_to_skip, size_t curr_pos){

  return fseek(desc, bytes_to_skip, curr_pos);
}

void* nkn_open_file(const char *in_file, const char *mode){
  return fopen(in_file, mode);
}

size_t nkn_tell(void *desc){
  return ftell(desc);
}

size_t nkn_get_size(void *desc){
  size_t curr_pos, size;

  curr_pos = ftell(desc);
  fseek(desc, 0, SEEK_END);
  size = ftell(desc);
  fseek(desc, curr_pos, SEEK_SET);

  return size;
	
}

size_t nkn_close(void *desc){
  return fclose(desc);
}

size_t nkn_fprintf(void *desc, const char * format, ...)
{
  va_list ap;

  va_start(ap, format);

  return fprintf(desc, format, ap);
}

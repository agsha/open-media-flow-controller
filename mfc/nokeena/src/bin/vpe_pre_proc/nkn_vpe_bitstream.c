/*
 *
 * Filename:  nkn_vpe_bitstream.c
 * Date:      2009/02/06
 * Module:    Memory Based IO
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <stdarg.h>

#include "nkn_vpe_bitstream.h"
#define DATA_BLOCK_SIZE 10*1024*1024


nkn_bitstream* open_bitstream(char *in, char *mode)
{
  // atm mode is always read binary!

  unsigned int file_size = 0, rv = 0;
  nkn_bitstream *bs;
  FILE *infile;
  infile = fopen64(in, mode);
  
  if(infile==NULL)
   return NULL;

  bs = (nkn_bitstream*)malloc(sizeof(nkn_bitstream));
  memset(bs, 0, sizeof(nkn_bitstream));
  fseek(infile, 0, SEEK_END);
  file_size = ftell(infile);
  rewind(infile);

  bs->_data = bs->_start = (unsigned char*)malloc(file_size);
  bs->_pos = 0;
  bs->_size = file_size;
  rv = fread(bs->_data, 1, file_size, infile);

  fclose(infile);

  printf("file size = %d \n", file_size);

  return bs;
}

int read_bitstream(\
		   void *buf,\
		   int element_size,\
		   int n_elements,\
		   nkn_bitstream *bs){
  if(bs->_data > bs->_start + bs->_size)
    {
      printf("reading past allocated memory\n");
      return -1; 
    }
  memcpy(buf, bs->_data, n_elements * element_size);
  bs->_pos += n_elements * element_size;
  bs->_data = bs->_start +  bs->_pos;
  
  return n_elements;//element_size;//nn_elements;
}

int seek_bitstream(\
		   nkn_bitstream *bs,\
		   int seek_bytes,\
		   unsigned int seek_from){

  switch(seek_from){
  case SEEK_SET:
    bs->_data = bs->_start + seek_bytes;
    bs->_pos = bs->_pos + seek_bytes;
    break;
  case SEEK_CUR:
    bs->_data = bs->_data + seek_bytes;
    bs->_pos = bs->_pos + seek_bytes;
    break;
  case SEEK_END:
    bs->_data = bs->_start + bs->_size + seek_bytes;
    bs->_pos = bs->_size  + seek_bytes;
    break;

  }

  return bs->_pos;
  
}

nkn_bitstream* create_bitstream(unsigned int size){

  nkn_bitstream *bs;

  bs = NULL;
  bs = (nkn_bitstream*) malloc (sizeof(nkn_bitstream));
  memset(bs, 0, sizeof(nkn_bitstream));

  bs->_data = bs->_start = (unsigned char*) malloc(size);
  bs->_pos = 0;
  bs->_size = size;
  
  return bs;
}

int write_bitstream(\
		    void *buf,\
		    int element_size,\
		    int n_elements,\
		    nkn_bitstream *bs)
{
  //if((bs->_pos + (n_elements * element_size)) > bs->_size){
  //	resize_bitstream(bs, DATA_BLOCK_SIZE);
  //}
  memcpy(bs->_data, buf, (n_elements * element_size));
  bs->_pos += (n_elements * element_size);
  bs->_data = bs->_start + bs->_pos;
	

  return n_elements;
}

int write_bitstream_to_file(\
			    nkn_bitstream *bs,\
			    char *out)
{
  int rv;
   FILE *fout;

  rv = 0;

  fout = fopen(out, "wb");
  
  if(fout==NULL)
   return -1;  

  rv = fwrite(bs->_start, 1, bs->_pos, fout);
  fclose(fout);

  return rv;
}

/*int resize_bitstream(nkn_bitstream *bs, uint32 new_size){

//bs->_data = (unsigned char*)realloc(bs->_data, bs->_size
return 0;

}*/

unsigned int get_bitstream_offset(nkn_bitstream *bs){

  return bs->_pos;
}

int get_bitstream_size(nkn_bitstream *bs){
  return bs->_size;

}

int close_bitstream(nkn_bitstream *bs){
  free(bs->_start);
  free(bs);

  return 0;
}

/*
int
printf_bitstream(nkn_bitstream *bs, const char *format, ...)
{
  int len;
  char pp[256];
//va_list ap;

  
  //va_start(ap, format);

  len = 0;
  //sprintf(pp, format);
  //sprintf((void*)(bs->_data), format);
  len = strlen((char*)(bs->_data));
  bs->_data += len;
  bs->_pos += len;

  return 1;
}
*/

/*
 *
 * Filename:  nkn_vpe_bitstream.h
 * Date:      2009/02/06
 * Module:    Memory based IO handlers
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef __NKN_BITSTREAM_H__
#define __NKN_BITSTREAM_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

    
  typedef struct tag_nkn_bitstream{
    unsigned char* _data;
    unsigned int _size;
    unsigned int _pos;
    unsigned char* _start;
  }nkn_bitstream;

    /** Opens a bitstream from a file. Allocates file size bytes of memory. To Do Buffered IO
     *@param infile [in] - file name to open (null terminated)
     *@param mode [in] - open mode (only read supported). Dont care at the moment
     *@return returns a valid bitstream handle, null if any error occurs
     */
  nkn_bitstream* open_bitstream(char *infile, char *mode);

    /** Reads data from an open bitstream
     *@param buf [in,out] - buffer to read data into
     *@param element_size [in] - size of each element
     *@param n_elements [in] - number of elements to read
     *@param bs[in] - bistream handle
     *@return returns the total data read n_elements*element_size, '0' if error
     */
  int read_bitstream(\
		     void *buf,\
		     int element_size,\
		     int n_elements,\
		     nkn_bitstream *bs);

    /** Seek to a point in the bitstream
     *@param bs [in] - handle to the bitstream
     *@param seek_bytes [in] - position to seek to
     *@param seek_fram [in] = position to seek from
     *@return returns current position in bitstream after seeking, <0 on error
     */
  int seek_bitstream(\
		     nkn_bitstream *bs,\
		     int seek_bytes,\
		     unsigned int seek_from);

    /** API has limitations (SHOULD NOTE BE USED)
     */
  nkn_bitstream* create_bitstream(unsigned int size);

    /** API has limitations (SHOULD NOTE BE USED)
     */
  int write_bitstream(\
		      void *buf,\
		      int element_size,\
		      int n_elements,\
		      nkn_bitstream *bs);

    /**API has limitations (SHOULD NOTE BE USED)
     */
  int write_bitstream_to_file(\
			      nkn_bitstream *bs,\
			      char *fout);

    /** Tells the current position in the bitstream
     *@param bs [in] = handle to the bitstream
     *@return returns the current position, <0 if error
     */
  unsigned int get_bitstream_offset(nkn_bitstream *bs);

    /**Returns the size of the Bistream
     *@param bs [in] - handle to the bitstream
     *@return returns the size of the bistream
     */
  int get_bitstream_size(nkn_bitstream *bs);

    /**Close the bitstream handle and cleans up allocated memory
     *@param bs - hanlde to the bistream
     */
  int close_bitstream(nkn_bitstream *bs);

    /**formatted write to bistream -- UNSUPPORTED API
     */
  int printf_bitstream(nkn_bitstream *bs, const char *format, ...);
#ifdef __cplusplus
}
#endif

#endif

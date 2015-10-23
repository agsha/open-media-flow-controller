#ifndef _NKN_MODULE_FILE_IO_
#define _NKN_MODULE_FILE_IO_

#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

  void* nkn_open_file(const char *,const char *mode);
  size_t nkn_read_file(void *src, size_t n_elements, size_t element_size, void *dst);
  size_t nkn_write(void *src, size_t n_elements, size_t element_size, void *dst);
  size_t nkn_seek(void *desc, size_t bytes_to_skip, size_t curr_pos);
  size_t nkn_tell(void *desc);
  size_t nkn_get_size(void *desc);
  size_t nkn_close(void *desc);
  size_t nkn_fprintf(void *desc, const char* format,...);

#ifdef __cplusplus
}
#endif

#endif


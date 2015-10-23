#ifndef NKN_VPE_UTILS_H
#define NKN_VPE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "nkn_memalloc.h"

int32_t recursive_mkdir(const char *const path, int32_t mode);


typedef void (*traverse_hdlr_fptr)(void* traverse_ctx,
		char const* dir_path, char const* file_name);

int32_t traverse_tree_dir(char const* dir_path, int32_t level,
		traverse_hdlr_fptr traverse_hdlr, void* traverse_ctx); 

/* FILE I/O callbacks */
size_t nkn_vpe_fread(void *buf, size_t n, size_t size,
		     void *desc);
void * nkn_vpe_fopen(char *p_data, const char *mode,
		     size_t size);
size_t nkn_vpe_ftell(void *desc);
size_t nkn_vpe_fseek(void *desc, size_t seekto,
		     size_t whence);
void nkn_vpe_fclose(void *desc);
size_t nkn_vpe_fwrite(void *buf, size_t n, size_t size,
		      void *desc);

/* Primitive I/O Callbacks */
size_t pio_read(void *buf, size_t n, size_t size, void *desc);

void * pio_open(char *p_data, const char *mode, size_t size);

size_t pio_tell(void *desc);

size_t pio_seek(void *desc, size_t seekto, size_t whence);

void pio_close(void *desc);

size_t pio_write(void *buf, size_t n, size_t size, void *desc);
uint64_t nkn_memstat_type(nkn_obj_type_t type);


#endif

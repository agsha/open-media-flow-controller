#ifndef _BUFIO_
#define _BUFIO_

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "rtsp_func.h"
#include "nkn_vfs.h"

#define NKN_VFS_BLKSIZE 1048575

#ifdef __cplusplus
extern "C" {
#endif

uint32_t _read32(uint8_t*,int32_t);
uint16_t _read16(uint8_t*,int32_t);
size_t nkn_vpe_buffered_read(void *buf, size_t size, size_t count, FILE *desc);
size_t nkn_vpe_buffered_read_opt(void *buf, size_t size, size_t count, FILE *desc,
				 void **temp_buf, int *free_buf);

#ifdef __cplusplus
}
#endif

#endif //header protection

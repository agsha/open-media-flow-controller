#include "nkn_vpe_bufio.h"
#include "nkn_vpe_types.h"
#include <byteswap.h>

inline uint32_t _read32(uint8_t *buf, int32_t pos)
{
	return bswap_32(*((int32_t *)(buf + pos)));
}

uint16_t _read16(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<8)|(buf[pos+1]);
}

/* block based read function to handle buffered I/O from DM2
 */
size_t
nkn_vpe_buffered_read(void *buf, size_t size, size_t count, FILE *desc)
{
    size_t size_to_read, ret, rem, tot;
    size_t read_size;
    uint8_t *p_buf;

    p_buf = (uint8_t*)buf;
    size_to_read = count*size;
    ret = 0;
    rem = 0;
    tot = 0;
    read_size = 0;

    if(size_to_read <= NKN_VFS_BLKSIZE) {
	ret = nkn_vfs_fread(buf, size, count, (FILE*)desc);
	return ret;
    } else {
	rem = size_to_read;
	while(rem) {
	    if(rem >= NKN_VFS_BLKSIZE) {
		read_size = NKN_VFS_BLKSIZE;
	    } else {
		read_size = rem;
	    }

	    if(tot > 35000000) {
		int uu = 0;
	    }
	    ret = nkn_vfs_fread(p_buf, 1, read_size, (FILE*)desc);
	    p_buf += ret;
	    if(ret == 0) {//if there is a read error we dont want it to loop infinitely
		return ret;
	    }
	    rem -= ret;
	    tot += ret;
	    
	}
	
	ret = tot;
    }
    
    return tot;
}


size_t
nkn_vpe_buffered_read_opt(void *buf, size_t size, size_t count, FILE *desc,
			  void **temp_buf, int *free_buf)
{
    size_t size_to_read, ret, rem, tot;
    size_t read_size;
    uint8_t *p_buf;

    p_buf = (uint8_t*)buf;
    size_to_read = count*size;
    ret = 0;
    rem = 0;
    tot = 0;
    read_size = 0;

    if(size_to_read <= NKN_VFS_BLKSIZE) {
	ret = nkn_vfs_fread_opt(buf, size, count, (FILE*)desc,
			    temp_buf, free_buf);
	return ret;
    } else {
	rem = size_to_read;
	while(rem) {
	    if(rem >= NKN_VFS_BLKSIZE) {
		read_size = NKN_VFS_BLKSIZE;
	    } else {
		read_size = rem;
	    }

	    if(tot > 35000000) {
		int uu = 0;
	    }
	    ret = nkn_vfs_fread_opt(p_buf, 1, read_size, (FILE*)desc,
				temp_buf, free_buf);
	    p_buf += ret;
	    if(ret == 0) {//if there is a read error we dont want it to loop infinitely
		return ret;
	    }
	    rem -= ret;
	    tot += ret;
	    
	}
	
	ret = tot;
    }
    
    return tot;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "nkn_debug.h"
#include "nkn_util.h"
#include "nkn_defs.h"

#define CKSUM_BASE_SIZE 4096

const char *provider_name[NKN_MM_MAX_CACHE_PROVIDERS] = {
	"Unknown",		// 0
	"SSD",			// 1
	"Flash Storage",	// 2
	"RAM Cache",		// 3
	"Unknown_4",		// 4
	"SAS Disk",		// 5
	"SATA Disk",		// 6
	"Unknown_7",		// 7
	"Unknown_8",		// 8
	"Unknown_9",		// 9
	"Peer Cache",		// 10
	"Temporary Cache",	// 11
	"Unknown_12",		// 12
	"Unknown_13",		// 13
	"Unknown_14",		// 14
	"Unknown_15",		// 15
	"Unknown_16",		// 16
	"Unknown_17",		// 17
	"Unknown_18",		// 18
	"NFS Origin",		// 19
	"HTTP Origin",		// 20
};

const char *
nkn_provider_name(nkn_provider_type_t id)
{
	if (id < NKN_MM_MAX_CACHE_PROVIDERS)
		return provider_name[id];
	return provider_name[0];
}

/*
 * Very fast case sensitive comparison for two string.
 * return:
 *    0: p1 and p2 are equal for n bytes.
 *    1: p1 and p2 are not equal.
 */
int32_t nkn_strcmp_case(const char * p1, const char * p2, int n)
{
        // compare two string without case
        if(!p1 || !p2 || n==0) return 1;
        while(n) {
                if(*p1==0 || *p2==0) return 1;
                if((*p1) != (*p2)) return 1;
                p1++;
                p2++;
                n--;
        }
        return 0;
}

/*
 * Very fast case insensitive comparison for two string.
 * return:
 *    0: p1 and p2 are equal for n bytes.
 *    1: p1 and p2 are not equal.
 */
int32_t nkn_strcmp_incase(const char * p1, const char * p2, int n)
{
	// compare two string without case
	if(!p1 || !p2 || n==0) return 1;
	while(n) {
		if(*p1==0 || *p2==0) return 1;
		if((*p1|0x20) != (*p2|0x20)) return 1;
		p1++;
		p2++;
		n--;
	}
	return 0;
}


/*
 * Convert Hex format IPv4 address into unsigned int IPv4 address
 * the return ipv4 address is in network order.
 */
uint32_t convert_ip_hex_2_network(uint8_t * addr)
{
	//00AC13AC
	uint32_t ip=0;
	int i;

	if( (addr == NULL) || (strlen((char *)addr) != 8) ) return 0;

	for(i=0;i<8;i++) {
		ip <<= 4;
		if (*addr<='9' && *addr >='0') ip += *addr - '0';
		else if (*addr<='F' && *addr >='A') ip += (*addr - 'A') + 10;
		else if (*addr<='f' && *addr >='a') ip += (*addr - 'a') + 10;
		else return 0;

		addr++;
	}

	return ip;
}

/*
 * convert MAC address format 
 * from 00:15:17:57:C1:CC to 0x00, 0x15, 0x17, 0x57, 0xC1, 0xCC
 */
int32_t convert_mac_str_to_bin(char * mac_str, uint8_t * mac_bin)
{
	char val;
	int i;

	for(i=0;i<6;i++) {
		val=0;

		if (*mac_str<='9' && *mac_str >='0') val = *mac_str - '0';
		else if (*mac_str<='F' && *mac_str >='A') val = (*mac_str - 'A') + 10;
		else if (*mac_str<='f' && *mac_str >='a') val = (*mac_str - 'a') + 10;
		else return 0;
		mac_str++;

		val <<= 4;

		if (*mac_str<='9' && *mac_str >='0') val += *mac_str - '0';
		else if (*mac_str<='F' && *mac_str >='A') val += (*mac_str - 'A')+10;
		else if (*mac_str<='f' && *mac_str >='a') val += (*mac_str - 'a')+10;
		else return 0;
		mac_str++;

		*mac_bin=val;
		mac_bin++;

		mac_str++; //skip :
	}
	return 1;
}

/*
 * convert a hex string to an integrate.
 */
int32_t convert_hexstr_2_int32(char * s)
{
	//0x12345678
        int32_t val=0;
        int i;

        if(s == NULL) return 0;

        for(i=0;i<8;i++) {
		if(*s==0) break;

		val <<= 4;
                if (*s<='9' && *s >='0') val += *s - '0';
                else if (*s<='F' && *s >='A') val += (*s - 'A') + 10;
                else if (*s<='f' && *s >='a') val += (*s - 'a') + 10;
                else break;

		s++;
	}
	return val;
}

/*
 * convert a hex string to an long.
 */
int64_t convert_hexstr_2_int64(char * s)
{
	//0x1234567890123456
        int64_t val=0;
        int i;

        if(s == NULL) return 0;

        for(i=0;i<16;i++) {
		if(*s==0) break;

		val <<= 4;
                if (*s<='9' && *s >='0') val += *s - '0';
                else if (*s<='F' && *s >='A') val += (*s - 'A') + 10;
                else if (*s<='f' && *s >='a') val += (*s - 'a') + 10;
                else break;

		s++;
	}
	return val;
}


/*
 * Create the full directory using a pessimistic algorithm.
 *
 * Possible optimizations:
 * - Stat the full pathname; and if it is present, then return.
 */
int
nkn_create_directory(char *dir_path,
		     int  isdir)
{
    char *end_slash, *cptr;
    int ret;

    if (isdir) {
	end_slash = &dir_path[strlen(dir_path)];
    } else {
	end_slash = strrchr(dir_path, '/');
    }
    cptr = dir_path;
    while (cptr < end_slash) {
	cptr = strchr(cptr+1, '/');
	if (cptr) {
	    *cptr = '\0';
	} else if (isdir) {
	    cptr = end_slash;
	} else {
	    break;
	}
	ret = mkdir(dir_path, 0755);
	if (ret != 0 && errno != EEXIST) {
	    /* This should never happen except when running on ext3 and we
	     * have more than 32,000 directories already */
	    DBG_LOG(ERROR, MOD_ANY_MOD,
		    "mkdir error (%s): %d", dir_path, errno);
	    return -errno;
	}
	if (!isdir || cptr != end_slash)
	    *cptr = '/';
    }
    /* Restore slash, so the container name is complete again */
    if (!isdir)
	*end_slash = '/';
    return 0;
}	/* nkn_create_directory */


/*
 * Do a 64-bit checksum on an arbitrary memory area..
 *
 * This isn't a great routine, but it's not _horrible_ either. The
 * inner loop could be unrolled a bit further, and there are better
 * ways to do the carry, but this is reasonable.
 */
unsigned long
do_csum64_iterate_aligned(const unsigned char *buff,
			  int		      len,
			  unsigned long	      old_checksum)
{
    unsigned long result = old_checksum;

    assert(len % 8 == 0);
    assert(((uint64_t)buff & 0x7) == 0);

    for (; len > 0; len -= 8, buff += 8) {
	result ^= *(uint64_t *)buff;
    }
    return result;
}	/* do_csum64_iterate_aligned */

unsigned int
do_csum32_iterate_aligned(const unsigned char *buff,
			  int		      len,
			  unsigned int	      old_checksum)
{
    unsigned int result = old_checksum;

    assert(len % 8 == 0);
    assert(((uint64_t)buff & 0x7) == 0);

    for (; len > 0; len -= 8, buff += 8) {
	result ^= *(uint32_t *)buff;
    }
    return result;
}	/* do_csum32_iterate_aligned */


unsigned long
do_csum64_aligned(const unsigned char *buff,
		  int		      len)
{
    return do_csum64_iterate_aligned(buff, len, 0);
}	/* do_csum64_aligned */

unsigned int
do_csum32_iterate_aligned_v3(const unsigned char *buff,
                          int                 len,
                          unsigned int        old_checksum)
{
    unsigned int    result = old_checksum;
    int             bindex;
    int             halfbasesize = CKSUM_BASE_SIZE/2;

    assert(len % 8 == 0);
    assert(((uint64_t)buff & 0x7) == 0);

    if (len < CKSUM_BASE_SIZE ) {
        for (; len > 0; len -= 8, buff += 8) {
            result ^= *(uint32_t *)buff;
        }
    } else {
        /* Calculate checksum for first half of CKSUM_BASE_SIZE bytes */
        for (bindex = 0; bindex < halfbasesize; bindex += 8, buff += 8) {
            result ^= *(uint32_t *)buff;
        }
        /* Calculate checksum for last half of CKSUM_BASE_SIZE bytes */
        for (bindex = len-halfbasesize; bindex < len; bindex += 8, buff += 8) {
            result ^= *(uint32_t *)buff;
        }
    }
    return result;
}       /* do_csum32_iterate_aligned_v3 */

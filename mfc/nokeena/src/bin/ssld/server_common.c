/*
 * server_common.c -- Shared interfaces
 */

#ifdef HTTP2_SUPPORT
/*
 *******************************************************************************
 * Experimental SPDY/HTTP2 implementation, not for general use.
 *******************************************************************************
 */

 #include <string.h>
 #include "server_common.h"

int streq(const unsigned char *str1, unsigned int str1_len,
	  const unsigned char *str2, unsigned int str2_len)
{
    if (str1_len != str2_len) {
    	return 0; // No match
    }

    return (memcmp(str1, str2, str1_len) == 0);
}

#endif /* HTTP2_SUPPORT */

/*
 * End of server_common.c
 */

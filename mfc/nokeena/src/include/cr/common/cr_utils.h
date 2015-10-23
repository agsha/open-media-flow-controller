#ifndef _CR_UTILS_
#define _CR_UTILS_

#include <stdio.h>
#include <sys/types.h>

#include "nkn_memalloc.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif


typedef struct tag_conjugate_graticule {
    double latitude;		/**< latitude in degreses North is
				 * positive and south is negative
				 */
    double longitude;		/**< longitude in degrees east is
				 * positive and west is negative
				 */
} conjugate_graticule_t;

typedef struct tag_ip_addr_range {
    uint32_t start_ip;
    uint32_t end_ip;
} ip_addr_range_t;

/**
 * enumerates the network interfaces into a NULL terminated sockaddr
 * array 
 * @param max_if[in] - a guesstimate of the maximum interfaces in the
 * system
 * returns a NULL terminated array of sockaddr_in elements containing
 * the ip  * addresses of all the ifterfaces. On error it returns NULL
 */
struct sockaddr_in** getIPInterfaces(int32_t max_if);

/** 
 * does a blocking read on the network with a configurable timeout
 * 
 * @param sd - socket descriptor
 * @param buf - buffer to receive data
 * @param len - length of the buffer
 * @param usec - timeout in microseconds
 * 
 * @return 0 on success and -errno on error
 */
int32_t timed_nw_read(int32_t sd, char* buf, uint32_t len, 
		      uint64_t usec); 

/** 
 * computes the time elapsed between two timeval instances
 * 
 * @param end [in] - end time
 * @param start [in] - start time
 * 
 * @return 0 if the end time is less than start time else returns the
 * time elapsed in milliseconds
 */
uint32_t diffTimevalToMs(struct timeval const * end,
			 struct timeval const * start);


/** 
 * computes the 'as the crow flies' distance between two points on an
 * ellipsoid (roughly the earth's shape). the two points are defined
 * by the conjugate_graticule_t type. We use the Haversine formula to
 * compute the distance given by 'd'
 * d = R.c, where R = radius of the earth (mean) 6371Km
 * and,
 * c = 2atan2(a^0.5, (1 - a)^0.5, where
 * a = sin^2(DLat/2) + (cos(Lat1/2).cos(Lat2/2)*sin^2(DLong/2), where
 * DLat is the distance between the Lat1 and Lat2 and  Lat1 and Lat2
 * are the latitude co-ordinates of the two points
 * 
 * @param cg1 
 * @param cg2 
 * 
 * @return 
 */
double compute_geo_distance(const conjugate_graticule_t *const cg1,
		     const conjugate_graticule_t *const cg2);

/** 
 * returns the reverse of a string; the new string is the same length
 * as the input and is alloated by the function and needs to be freed
 * by the caller
 * 
 * @param str - input string to be reversed
 *
 * @return returns NULL on error and the reversed string on success
 */
char* strrev(char const*str);

char* domain_label_rev (char *str);


#define STORE_SOCK_FILE "/var/cr_store.sock"

int32_t makeUnixConnect(char const* path);

#ifdef __cpluplus
}
#endif

#endif //_CR_UTILS_

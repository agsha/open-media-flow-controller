#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

//net utils
#include <netinet/in.h>
#include <net/if.h> //if_nameindex()
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/un.h>


#include "cr_utils.h"

static inline double degree2radians(double deg);

/**
 * enumerates the network interfaces into a NULL terminated sockaddr
 * array 
 * @param max_if[in] - a guesstimate of the maximum interfaces in the
 * system
 * returns a NULL terminated array of sockaddr_in elements containing
 * the ip  * addresses of all the ifterfaces. On error it returns NULL
 */
struct sockaddr_in** getIPInterfaces(int32_t max_if) {

    struct ifreq net_if_data;
    int32_t sock = 0, i = 0;
    struct if_nameindex *net_if_list = NULL;
    struct sockaddr_in **res_addr = calloc(max_if + 1,
				   sizeof(struct sockaddr_in*));
    if (res_addr == NULL) {
	printf("memory alloc failed\n");
	return NULL;
    }

    if( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	perror("socket");
	goto error_return;
	return NULL;
    }

    net_if_list = if_nameindex();
    struct if_nameindex* net_if_list_ft = net_if_list;

    int32_t ip_if_count = 0;
    for(; net_if_list->if_index != 0; net_if_list++) {

	strncpy(net_if_data.ifr_name, net_if_list->if_name, IF_NAMESIZE);

	if(ioctl(sock, SIOCGIFADDR, &net_if_data) != 0){
	    if (errno != EADDRNOTAVAIL) {
		perror("ioctl");
	    }
	    continue;
	}
	res_addr[ip_if_count] = calloc(1, sizeof(struct sockaddr_in));
	if (res_addr[ip_if_count] == NULL)
	    goto error_return;
	memcpy(res_addr[ip_if_count], &(net_if_data.ifr_addr),
	       sizeof(struct sockaddr_in));
	// debug purpose
	char ip_addr[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, &(res_addr[ip_if_count]->sin_addr.s_addr),
		      &ip_addr[0], INET_ADDRSTRLEN) == NULL) {
	    perror("inet_ntop");
	} else
	    printf("INIT : IF IP ADDR: %s\n", &(ip_addr[0]));
	// debug end
	ip_if_count++;
	if (ip_if_count == max_if)
	    break;
    }
    res_addr[ip_if_count] = NULL;
    if_freenameindex(net_if_list_ft);
    close(sock);
    return res_addr;

 error_return:
    if (res_addr != NULL) {
	while(res_addr[i] != NULL)
	    free(res_addr[i++]);
	free(res_addr);
    }
    return NULL;
}

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
int32_t
timed_nw_read(int32_t sd, char* buf, uint32_t len, uint64_t usec)
{
    fd_set read_set;
    int32_t rc = 0;
    
    struct timeval timeout;
    uint64_t sec = usec/1000000;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec%1000000;
    FD_ZERO(&read_set);
    FD_SET(sd, &read_set);

    rc = select(sd+1, &read_set, NULL, NULL, &timeout);
    if (rc == 0) {
	return -ETIMEDOUT;
    }
    if (rc < 0) {
	return -errno;
    }
    if (FD_ISSET(sd, &read_set)) {
	rc = recv(sd, buf, len, 0);
	if (rc < 0) {
	    return -errno;
	}
    }

    return rc;
}

/** 
 * computes the time elapsed between two timeval instances
 * 
 * @param end [in] - end time
 * @param start [in] - start time
 * 
 * @return 0 if the end time is less than start time else returns the
 * time elapsed in milliseconds
 */
uint32_t
diffTimevalToMs(struct timeval const* from,
		struct timeval const * val) 
{

    //if -ve, return 0
    double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
    double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
    double diff = d_from - d_val;
    if (diff < 0)
	return 0;
    return (uint32_t)(diff * 1000);
}

inline double
degree2radians(double deg)
{
    return ((double)(deg * M_PI/180));
}

double
compute_geo_distance(const conjugate_graticule_t *const cg1,
		     const conjugate_graticule_t *const cg2)
{
    
    double x11, y11, x22, y22, x11r, x22r,
	p1, t1,
	tmp1, tmp2, a, c, d = 0.0;
    const uint32_t R = 6371;

    x11 = cg1->latitude; y11 = cg1->longitude;
    x22 = cg2->latitude; y22 = cg2->longitude;
    
    p1 = degree2radians((x22 - x11));
    t1 = degree2radians((y22 - y11));
    x11r = degree2radians(x11);
    x22r = degree2radians(x22);

    tmp1 = (sin(p1/2) * sin(p1/2));
    tmp2 = (sin(t1/2) * sin(t1/2));
    a = tmp1 + (cos(x11r) * cos(x22r) * tmp2);
    
    tmp1 = pow(a, 0.5);
    tmp2 = pow((1 - a), 0.5);
    c = 2*atan2(tmp1, tmp2);
    
    d = (double) (R * c);
    
    return d;

}

char* 
strrev(char const* str) 
{

    char* str1;
    int32_t len,j,i=0;
    len = strlen(str) + 1;
    str1 = (char*)nkn_calloc_type(1, sizeof(char)*len, mod_cr_ds);
    for(j=len-2;j>=0;j--)
	str1[j] = str[i++];
    return str1;

}

char* 
domain_label_rev (char *str) 
{
    uint32_t len = 0, pos = 0, s_end = 0, s_st = 0, d_st = 0;
    char *out = NULL;

    s_end = len = strlen(str);

    out = (char *)
	nkn_calloc_type(1, sizeof(char) * (len + 1), mod_cr_ds);

    d_st = len;
    while(str[pos] != '\0') {
	if (str[pos] == '.') {
	    d_st -= (pos - s_st + 1);
	    memcpy(out + d_st, str + s_st , pos - s_st + 1);
	    s_st = pos + 1;
	}
	pos++;
    }
    d_st -= (pos - s_st);
    memcpy(out + d_st, str + s_st - 1, pos - s_st + 1);
    
    return out;
}

int32_t makeUnixConnect(char const* path) {

    int32_t store_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (store_fd < 0) {
	int32_t err = -errno;
	//printf("unable to create store sock err=%d\n", err);
	return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, 256, "%s", path);

    if (connect(store_fd, (struct sockaddr *)&addr,
		sizeof(struct sockaddr_un)) != 0) {
	int32_t err = -errno;
	//printf("unable to connect to store err=%d\n", err);
	close(store_fd);
	return -2;
    }
    return store_fd;
}


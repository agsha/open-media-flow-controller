#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>

#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_ioctl.h"
#include "nkn_stat.h"

/*
 * We are using MMAP to map the buffer from user space to driver
 * to avoid socket data copy.
 *
 * Before launch, you need to run
 * mknod -m 666 /dev/nkn_mmap c 240 0
 * for setting up the mmap.
 */

void * p_drv_mmap=NULL, * p_tcp_mmap=NULL, * p_data_mmap=NULL, * p_copy_mmap=NULL;
size_t l_drv_mmap=0, l_tcp_mmap=0, l_data_mmap=0, l_copy_mmap=0;
nkn_drv_mmap_header_t * p_mmap_header=NULL;
size_t l_mmap_header=0;
static int fd=0;
//extern int useIOVEC;

int nkn_init_mmap(void);
void nkn_cleanup_mmap(void);

static void nkn_set_mmap_header(void) 
{
	memset((char *)p_mmap_header, 0, l_mmap_header);

	p_mmap_header->rx_init_offset = nkn_drv_mmap_header_s;
	p_mmap_header->rx_tot_len = RX_TOT_LEN;

	p_mmap_header->tx_init_offset = nkn_drv_mmap_header_s + RX_TOT_LEN;
	p_mmap_header->tx_tot_len = TX_TOT_LEN;

	/*
	 * Setup counters.
	 */
	nkn_mon_add("glob_drv_tot_cp_pkt",  NULL,
		(void *)&p_mmap_header->glob_drv_tot_cp_pkt, 
		sizeof(unsigned long));
	nkn_mon_add("glob_drv_tot_snd_pkt",  NULL,
		(void *)&p_mmap_header->glob_drv_tot_snd_pkt, 
		sizeof(unsigned long));
	nkn_mon_add("glob_drv_tot_rcv_pkt",  NULL,
		(void *)&p_mmap_header->glob_drv_tot_rcv_pkt, 
		sizeof(unsigned long));
	nkn_mon_add("glob_drv_tot_enq_pkt",  NULL,
		(void *)&p_mmap_header->glob_drv_tot_enq_pkt, 
		sizeof(unsigned long));
	nkn_mon_add("glob_drv_tot_sg_pkts",  NULL,
		(void *)&p_mmap_header->glob_drv_tot_sg_pkts, 
		sizeof(unsigned long));
}

int nkn_init_mmap(void) 
{
	/* 
	 * We split the whole memory into three pieces. 
	 * 	1. sizeof(nkn_drv_mmap_header_t)
	 *	2. RX_TOT_LEN  for RX packets
	 *	3. TX_TOT_LEN  for TX packets
	 *	4. Stack buffer (future enhancement)
	 */
	if(MMAP_TOT_BUFSIZE < cm_max_cache_size + RX_TOT_LEN + TX_TOT_LEN + MMAP_STACK_BUFSIZE) {
		DBG_LOG(SEVERE, MOD_TCP, 
			"MMAP size (%d) is too small. Total needs %ld bytes\n",
			MMAP_TOT_BUFSIZE, 
			cm_max_cache_size + RX_TOT_LEN + TX_TOT_LEN + MMAP_STACK_BUFSIZE);
		DBG_ERR(SEVERE,  
			"MMAP size (%d) is too small. Total needs %ld bytes\n",
			MMAP_TOT_BUFSIZE, 
			cm_max_cache_size + RX_TOT_LEN + TX_TOT_LEN + MMAP_STACK_BUFSIZE);
		assert(0);
		assert(0);
	}

	fd = open("/dev/nkn_mmap", O_RDWR | O_SYNC);
	if( fd == -1) {
		const char msg[] = 	"open /dev/nkn_mmap error, pls do the following:\n"
				"\n"
				"- find the major number assigned to the driver\n"
				"%% grep nkn_mmap /proc/devices\n"
				"- and create the special file (should be major number 240)\n"
				"%% mknod -m 666 /dev/nkn_mmap c 240 0\n"
				"\n";
		printf(msg);
		assert(0);
	}

	// test mmap
	l_drv_mmap = MMAP_TOT_BUFSIZE;
	p_drv_mmap = mmap(0, l_drv_mmap, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
	if(p_drv_mmap == MAP_FAILED) {
		DBG_LOG(SEVERE, MOD_TCP, "mmap() failed errno=%d", errno);
		DBG_ERR(SEVERE, "mmap() failed errno=%d", errno);
		assert(0);
	}
	DBG_LOG(MSG, MOD_TCP, "nkn_init_mmap: p_drv_mmap=%p l_drv_mmap=%ld", p_drv_mmap, l_drv_mmap);

	/* The first cm_max_cache_size bytes memory will be used by buffer manager */
	mlock((void *)p_drv_mmap, l_drv_mmap);

	/* Set up nkn_drv_mmap_header_t */
	p_mmap_header = (nkn_drv_mmap_header_t *)p_drv_mmap;
	l_mmap_header = nkn_drv_mmap_header_s;
	nkn_set_mmap_header();

	/*
	 * p_data_mmap=NULL; l_data_mmap=0;
	 */
	p_copy_mmap=(char*)p_mmap_header + l_mmap_header;
	l_copy_mmap=RX_TOT_LEN + TX_TOT_LEN;
	DBG_LOG(MSG, MOD_TCP, "nkn_init_mmap: p_copy_mmap=%p l_copy_mmap=%ld", p_copy_mmap, l_copy_mmap);

	p_tcp_mmap=(char *)p_copy_mmap + l_copy_mmap;
	l_tcp_mmap=MMAP_TOT_BUFSIZE - l_mmap_header - l_copy_mmap;
	DBG_LOG(MSG, MOD_TCP, "nkn_init_mmap: p_tcp_mmap=%p l_tcp_mmap=%ld", p_tcp_mmap, l_tcp_mmap);

	return 1;
}

void nkn_cleanup_mmap(void) 
{
	munlock((void *)p_drv_mmap, l_drv_mmap);
	munmap(p_drv_mmap, l_drv_mmap);
	close(fd);
}


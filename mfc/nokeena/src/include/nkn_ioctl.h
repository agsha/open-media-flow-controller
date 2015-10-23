#ifndef NKN_IOCTL__H
#define NKN_IOCTL__H

#define NKN_USR_READ_IOCTL 		0x89FF
#define NKN_USR_WRITE_IOCTL 		0x89FE
#define NKN_USR_WRITE_IOCTL_SG 		0x89FD
#define NKN_USR_READ_MULTIPLE_IOCTL 	0x89FC
#define NKN_IOCTL_TEST  		0x89F8

#define SIZEOF_ETHERHEAD	14

#define MAX_DRV_THREAD 4

/*
 * Used by MMAP buffer
 * Inside driver, it was allocated 1.5Gbits memory.
 */
#define MMAP_TOT_BUFSIZE	1556925000      //1.44Gbits
#define RX_TOT_LEN		(4096*200)
#define TX_TOT_LEN		(4096*200)
#define MMAP_STACK_BUFSIZE      (100*1024*1024) // used by TCP Stack size


/* some common header used between linux driver and user process */
/*
 * the shared MMAP buffer includes
 * 1. 4KBytes mmap_header
 * 2. read_offset bytes for NIC input packets
 * 3. write_offset bytes for TCP stack output packets.
 * 4. data buffer space
 */
typedef struct nkn_drv_mmap_header {
	/* shared thread mutex */
	unsigned long rx_init_offset;	// Must be aligned to 4K
	unsigned long rx_tot_len;	// 
	unsigned long tx_init_offset;	// 
	unsigned long tx_tot_len;	// 

	unsigned long bm_init_offset;	// buffer manager offset
	unsigned long bm_tot_len; 	//

	/* counters */
	unsigned long glob_drv_tot_cp_pkt;
	unsigned long glob_drv_tot_snd_pkt;
	unsigned long glob_drv_tot_rcv_pkt;
	unsigned long glob_drv_tot_enq_pkt;
	unsigned long glob_drv_tot_sg_pkts;
} nkn_drv_mmap_header_t;
#define nkn_drv_mmap_header_s 4096 /* 4K size reserved for header */
extern nkn_drv_mmap_header_t * p_mmap_header;

#pragma pack(1)

#if 0
typedef struct nkn_tx_packet {
	int len;
	char dstMac[6]; // destinatin Mac
	char srcMac[6]; // source Mac
	char type[2];	// ethernet type
	char data[1500];// ip and tcp here
} nkn_tx_packet_t;
#define nkn_tx_packet_s sizeof(struct nkn_tx_packet)

typedef struct nkn_iovec_sg {
	/* header part */
        char * header_ptr;	// offset in MMAP
        unsigned int header_len;
	/* TCP payload part */
        unsigned long payload_ptr;	// offset in MMAP
        unsigned int payload_len;
} nkn_iovec_sg_t;
#define nkn_iovec_sg_s sizeof(struct nkn_iovec_sg)

typedef struct nkn_tx_packet_sg {
        int totpkt;
	nkn_iovec_sg_t ppkt[1];
} nkn_tx_packet_sg_t;
#define nkn_tx_packet_sg_s 2048
#endif // 0

typedef struct nkn_packet {
        int len;
} nkn_packet_t;
#define nkn_packet_s sizeof(struct nkn_packet)

typedef struct nkn_ioctl_fmt {
        int numOfPacket;
        unsigned long begin_offset;
        unsigned long end_offset;
} nkn_ioctl_fmt_t;
#define nkn_ioctl_fmt_s sizeof(struct nkn_ioctl_fmt)

#pragma pack()

#endif // NKN_IOCTL__H


#ifndef IOV_BUF_STATE_H
#define IOV_BUF_STATE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/uio.h>

#define TCP_IOV_FREE_ALL 0x01  // Free data under every iov and free(iov)
#define TCP_IOV_FREE_BASE 0x02 // Free data only at the first iov and free(iov)
// > 0x02 // No free


typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned char uint8_t;


typedef struct iov_buf_state {
        struct iovec* iov_array_ptr;
        uint32_t iov_array_count;
        uint32_t iov_array_total_bytes;
        uint32_t iov_array_curr_pos;
	uint8_t iov_array_free_flag;
} iov_buf_state_t;

#ifdef __cplusplus
extern "C" {
#endif

iov_buf_state_t* createIovBufState();


void initIovBufState(iov_buf_state_t* iov_state, struct iovec* iov_array_ptr, 
		uint32_t iov_array_count, uint32_t offset, uint8_t iov_array_free_flag);


uint32_t countDataInIovBuf(iov_buf_state_t* iov_state);


void freeIovBufState(iov_buf_state_t* iov_state);


void deleteIovBufState(iov_buf_state_t* iov_state);

// other utils related to iov buf handling
int32_t sendIovFromOffset(int32_t sock_fd, struct iovec* iov, 
			uint32_t iovlen, uint32_t offset, uint32_t iov_send_flags);

void freeIovBuf(struct iovec* iov, uint32_t iovlen, uint8_t free_flag);

int32_t tcpIovSend(iov_buf_state_t* prev_iov_state, int32_t sock_fd,
                struct msghdr *mh, int32_t sret, uint8_t free_flag);

#ifdef __cplusplus
}
#endif

#endif

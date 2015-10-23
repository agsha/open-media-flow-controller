#include  "iov_buf_state.h"


static int32_t rebase_iovec_from_off(struct iovec const* fromVec, struct iovec* toVec,
                uint32_t count, uint32_t offset);


static uint32_t calculateIovByteLen(struct iovec const* iov, uint32_t iovlen);


iov_buf_state_t* createIovBufState() {

	iov_buf_state_t* iov_state = 
		(iov_buf_state_t*) malloc(sizeof(iov_buf_state_t));
	if (iov_state != NULL)
		initIovBufState(iov_state, NULL, 0, 0, 0x00);
	return iov_state;
}


void initIovBufState(iov_buf_state_t* iov_state, struct iovec* iov_array_ptr, 
			uint32_t iov_array_count, uint32_t offset, uint8_t iov_array_free_flag) {

        iov_state->iov_array_ptr = iov_array_ptr;
	iov_state->iov_array_count = iov_array_count;
        iov_state->iov_array_curr_pos = offset;
        iov_state->iov_array_free_flag = iov_array_free_flag;
        iov_state->iov_array_total_bytes = calculateIovByteLen(iov_array_ptr, iov_array_count);
}


uint32_t countDataInIovBuf(iov_buf_state_t* iov_state) {

	uint32_t data_avl = 0;
	if ((iov_state != NULL) && (iov_state->iov_array_ptr != NULL))
		data_avl = iov_state->iov_array_total_bytes - iov_state->iov_array_curr_pos;
	return data_avl;
}


void freeIovBufState(iov_buf_state_t* iov_state) {

	freeIovBuf(iov_state->iov_array_ptr, iov_state->iov_array_count, 
		iov_state->iov_array_free_flag);	
	iov_state->iov_array_ptr = NULL;
	iov_state->iov_array_count = 0;
}


void deleteIovBufState(iov_buf_state_t* iov_state) {
	if (iov_state != NULL) {
		freeIovBufState(iov_state);
		free(iov_state);
	}
	iov_state = NULL;
}


int32_t sendIovFromOffset(int32_t sock_fd, struct iovec* iov,
                        uint32_t iov_count, uint32_t byte_offset, uint32_t iov_send_flags) {

	struct iovec* iovTmp = (struct iovec *)	alloca(iov_count * sizeof(struct iovec));	
        uint32_t cnt = rebase_iovec_from_off(iov, iovTmp, iov_count, byte_offset);
	struct msghdr msg;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iovTmp; 
	msg.msg_iovlen = cnt;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
	msg.msg_flags = 0;

	return sendmsg(sock_fd, &msg, iov_send_flags); 
}


void freeIovBuf(struct iovec* iov, uint32_t iovlen, uint8_t free_flag) {

	if (free_flag == TCP_IOV_FREE_ALL) {
		uint32_t i = 0;
		while (i < iovlen)
			free(iov[i++].iov_base);
		free(iov);
	} else if (free_flag == TCP_IOV_FREE_BASE) {
		if (iovlen > 0)
			free(iov[0].iov_base);
		free(iov);
	}
}


int32_t tcpIovSend(iov_buf_state_t* prev_iov_state, int32_t sock_fd,
                struct msghdr *mh, int32_t sret, uint8_t free_flag) {

        int32_t ret = 0;
        if (countDataInIovBuf(prev_iov_state)) {
                ret = sendIovFromOffset(sock_fd, prev_iov_state->iov_array_ptr,
                        prev_iov_state->iov_array_count,
                         prev_iov_state->iov_array_curr_pos, MSG_DONTWAIT);
                if (ret >= 0) {
                        prev_iov_state->iov_array_curr_pos += ret;
                	if (countDataInIovBuf(prev_iov_state) == 0) {
			
				//printf("remaining sent: %d:%d\n", ret, prev_iov_state->iov_array_curr_pos);
                        	freeIovBufState(prev_iov_state);
                        	goto curr_send;
                	} else {// prev pkt unsent: drop curr pkt
                       		freeIovBuf(mh->msg_iov, mh->msg_iovlen, free_flag);
				//printf("remaining still unsent: %d:%d\n", ret, prev_iov_state->iov_array_curr_pos);
			}
		}
        } else {
curr_send:
                ret = sendmsg(sock_fd, mh, MSG_DONTWAIT);
                if (ret >= 0) {
                        if (ret < sret) {
				//printf("half sent: %d:%d\n", ret, sret);
                                initIovBufState(prev_iov_state, mh->msg_iov, mh->msg_iovlen, ret, free_flag);
                        } else //curr pkt completely sent: free resources
                                freeIovBuf(mh->msg_iov, mh->msg_iovlen, free_flag);
                }
        }
        return ret;
}


static int32_t rebase_iovec_from_off(struct iovec const* fromVec, struct iovec* toVec,
                uint32_t count, uint32_t offset) {
        // In Condition: toVec is pre-allocated and has same count as fromVec
        // Out : Returns the count of toVec array
        uint32_t i = 0, traversed = 0;
        while (i < count) {
                traversed += fromVec[i].iov_len;
                if (traversed > offset)
                        break;
                i++;
        }
        if (i < count) {
                uint32_t j = 0;
                while (j < (count - i)) {
                        toVec[j].iov_base = fromVec[i+j].iov_base;
                        toVec[j].iov_len = fromVec[i+j].iov_len;
                        j++;
                }
                uint32_t shift = toVec[0].iov_len - (traversed - offset);
                toVec[0].iov_base = (uint8_t*)toVec[0].iov_base + shift;
                toVec[0].iov_len -= shift;
                return j;
        } else
                return 0;
}

static uint32_t calculateIovByteLen(struct iovec const* iov, uint32_t iovlen) {

	uint32_t len = 0, i = 0;
	while (i < iovlen)
		len += iov[i++].iov_len;
	return len;
}


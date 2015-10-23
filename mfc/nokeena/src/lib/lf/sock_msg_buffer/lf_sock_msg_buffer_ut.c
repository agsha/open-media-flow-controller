#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lf_sock_msg_buffer.h"

int32_t completeHdlr(uint8_t* buf, uint32_t len, void const* ctx) {

	uint32_t i = 0;
	for (; i < len; i++)
		printf("%c", buf[i]);
	printf("\n");
	printf("\n");
	free(buf);
	return 0;
}


int32_t main(int32_t argc, char** argv) {

	msg_buffer_t* sock_buf = createMsgBuffer(10000, 10000, completeHdlr,
			NULL, NULL);
	char*msg1 = "GET /test/index.html HTTP/1.1\r\ncontent-Length: 15"; 
	char* msg2 = "\r\n\r\nHelloHelloHelloPOST /test/index2.html HTTP/1.1";
	char* msg3 = "\r\nContent-length: 20\r\n\r\nHelloHelloHelloHello";
	/*printf("%u %u %u\n", (uint32_t)strlen(msg1), (uint32_t)strlen(msg2),
		  (uint32_t)strlen(msg3));*/
	sock_buf->add_read_msg_hdlr(sock_buf, (uint8_t*)msg1, strlen(msg1));
	sock_buf->add_read_msg_hdlr(sock_buf, (uint8_t*)msg2, strlen(msg2));
	sock_buf->add_read_msg_hdlr(sock_buf, (uint8_t*)msg3, strlen(msg3));
	sock_buf->delete_hdlr(sock_buf);
	return 0;
}


#include <stdio.h>

#include "lf_http_builder.h"

int32_t main(int32_t argc, char** argv) {

	http_msg_builder_t* http_bldr = createHTTP_Request("1.2", "POST",
			"/index.html", 500);
	http_bldr->add_hdr_hdlr(http_bldr, "Content-Language", "en");
	http_bldr->add_hdr_hdlr(http_bldr, "Content-Length", "10");
	http_bldr->add_content_hdlr(http_bldr, (uint8_t*)"HelloHello", 10);
	uint8_t const* ptr;
	uint32_t len;
	http_bldr->get_msg_hdlr(http_bldr, &ptr, &len);
	int32_t i = 0;
	for (; i < len; i++)
		printf("%c", ptr[i]);
	printf("\n");
	http_bldr->delete_hdlr(http_bldr);

	return 0;
}


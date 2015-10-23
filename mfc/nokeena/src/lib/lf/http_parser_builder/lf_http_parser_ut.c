#include "lf_http_parser.h"

int32_t main(int32_t argc, char** argv) {

	char* message = "HTTP/1.0 200 OK\r\nAccept-Language: en\r\nContent-Length: 10\r\n\r\nHelloHello";
	parsed_http_msg_t* parse_out = parseHTTP_Message((uint8_t*)message);
	printParsedHttpMessage(parse_out);
	parse_out->delete_hdlr(parse_out);
	return 0;
	
}

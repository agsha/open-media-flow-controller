/*
 * proto_http_test.c -- Unit test cases
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>
#include <assert.h>

#include "proto_http/proto_http.h"

#define UNUSED_ARGUMENT(x) (void)x

static const char *test_HTTP_request =
"GET /abc/def/ghi/xyz.html?querystr=querydata HTTP/1.1\r\n"
"Host: somehost.com\r\n\r\n";

static const char *test_HTTP_response =
"HTTP/1.1 200 OK\r\n"
"Date: Sat, 17 Jan 2015 02:14:18 GMT\r\n"
"Server: Apache/2.2.15 (CentOS)\r\n"
"Content-Length: 100\r\n\r\n";


int main(int argc, char **argv)
{
    UNUSED_ARGUMENT(argc);
    UNUSED_ARGUMENT(argv);
    int rv;
    char buf[2048];
    token_data_t token;
    proto_data_t pd;
    proto_data_t pd_response;
    HTTPProtoStatus status;
    ProtoHTTPHeaderId hid;
    const char *name;
    int namelen;
    const char *val;
    int vallen;
    int header_cnt;
    proto_http_config_t cfg;
    int respcode;
    const char *tval;
    int tvallen;

    char *pbuf;
    int pbuf_len;
    int nv_count;
    int n;

    printf("Start proto_http_test\n");

    memset(&cfg, 0, sizeof(cfg));
    cfg.interface_version = PROTO_HTTP_INTF_VERSION;
    cfg.options = OPT_NONE;
    rv = proto_http_init(&cfg);
    assert(rv == 0);

    strcpy(buf, test_HTTP_request);

    pd.u.HTTP.destIP = "10.157.42.207";
    pd.u.HTTP.destPort = 80;
    pd.u.HTTP.clientIP = "10.157.42.52";
    pd.u.HTTP.clientPort = 2000;
    pd.u.HTTP.data = buf;
    pd.u.HTTP.datalen = strlen(test_HTTP_request) + 1;

    token = HTTPProtoData2TokenData(0, &pd, &status);
    assert(token);
    assert(status == HP_SUCCESS);

    rv = HTTPHeaderBytes(token);
    assert(rv > 0);

    hid = HTTPHdrStrToHdrToken("Content-Length", strlen("Content-Length"));
    assert(hid == H_CONTENT_LENGTH);

    rv = HTTPAddKnownHeader(token, 0, H_COOKIE, "cookie-data", 
			    strlen("cookie-data"));
    assert(rv == 0);

    rv = HTTPAddKnownHeader(token, 0, H_COOKIE, "cookie-data2", 
			    strlen("cookie-data2"));
    assert(rv == 0);

    rv = HTTPAddKnownHeader(token, 1, H_COOKIE, "cookie-data", 
			    strlen("cookie-data"));
    assert(rv == 0);

    rv = HTTPAddKnownHeader(token, 1, H_COOKIE, "cookie-data2", 
			    strlen("cookie-data2"));
    assert(rv == 0);

    rv = HTTPGetKnownHeader(token, 0, H_COOKIE, &val, &vallen, &header_cnt);
    assert(rv == 0);
    assert(bcmp(val, "cookie-data", vallen)== 0);
    assert(header_cnt == 2);

    rv = HTTPGetKnownHeader(token, 1, H_COOKIE, &val, &vallen, &header_cnt);
    assert(rv == 0);
    assert(bcmp(val, "cookie-data", vallen)== 0);
    assert(header_cnt == 2);

    rv = HTTPGetNthKnownHeader(token, 0, H_COOKIE, 1, &val, &vallen);
    assert(rv == 0);
    assert(bcmp(val, "cookie-data2", vallen)== 0);

    rv = HTTPGetNthKnownHeader(token, 1, H_COOKIE, 1, &val, &vallen);
    assert(rv == 0);
    assert(bcmp(val, "cookie-data2", vallen)== 0);

    rv = HTTPGetQueryArg(token, 0, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(bcmp(name, "querystr", namelen) == 0);
    assert(bcmp(val, "querydata", vallen) == 0);

    rv = HTTPGetQueryArgbyName(token, "querystr", strlen("querystr"), 
    			       &val, &vallen);
    assert(rv == 0);
    assert(bcmp(val, "querydata", vallen) == 0);

    rv = HTTPDeleteKnownHeader(token, 0, H_COOKIE);
    assert(rv == 0);

    rv = HTTPDeleteKnownHeader(token, 1, H_COOKIE);
    assert(rv == 0);

    rv = HTTPAddUnknownHeader(token, 0, "myheader", strlen("myheader"),
    			      "myheader-value", strlen("myheader-value"));
    assert(rv == 0);

    rv = HTTPAddUnknownHeader(token, 0, "myheader2", strlen("myheader2"),
    			      "myheader-value2", strlen("myheader-value2"));
    assert(rv == 0);

    rv = HTTPAddUnknownHeader(token, 1, "myheader", strlen("myheader"),
    			      "myheader-value", strlen("myheader-value"));
    assert(rv == 0);

    rv = HTTPAddUnknownHeader(token, 1, "myheader2", strlen("myheader2"),
    			      "myheader-value2", strlen("myheader-value2"));
    assert(rv == 0);

    rv = HTTPGetNthUnknownHeader(token, 0, 1, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(bcmp("myheader2", name, namelen)== 0);
    assert(bcmp("myheader-value2", val, vallen)== 0);

    rv = HTTPGetNthUnknownHeader(token, 1, 1, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(bcmp("myheader2", name, namelen)== 0);
    assert(bcmp("myheader-value2", val, vallen)== 0);

    rv = HTTPDeleteUnknownHeader(token, 0, "myheader", strlen("myheader"));
    assert(rv == 0);

    rv = HTTPDeleteUnknownHeader(token, 0, "myheader2", strlen("myheader2"));
    assert(rv == 0);

    rv = HTTPDeleteUnknownHeader(token, 1, "myheader", strlen("myheader"));
    assert(rv == 0);

    rv = HTTPDeleteUnknownHeader(token, 1, "myheader2", strlen("myheader2"));
    assert(rv == 0);

    rv = HTTPAddKnownHeader(token, 1, H_COOKIE, "cookie-data", 
			    strlen("cookie-data"));
    assert(rv == 0);

    rv = HTTPAddKnownHeader(token, 1, H_CONTENT_LENGTH, "123456", 
			    strlen("123456"));
    assert(rv == 0);

    val = 0;
    vallen = 0;
    rv = HTTPResponse(token, 200, &val, &vallen, 0);
    assert(rv == 0);
    assert(val);
    assert(vallen);

    rv = HTTPResponse(token, -1, &tval, &tvallen, &respcode);
    assert(rv == 0);
    assert(tval == val);
    assert(tvallen == vallen);
    assert(respcode == 200);

    deleteHTTPtoken_data(token);

    // NewTokenData()
    token = NewTokenData();
    assert(token != 0);

    // Verify HTTPProtoSetReqOptions()/HTTPProtoReqOptions()
    HTTPProtoSetReqOptions(token, PH_ROPT_METHOD_GET|PH_ROPT_METHOD_TRACE);
    assert(HTTPProtoReqOptions(token) == 
    		PH_ROPT_METHOD_GET|PH_ROPT_METHOD_TRACE);
    deleteHTTPtoken_data(token);

    // Verify HTTPProtoRespData2TokenData()
    strcpy(buf, test_HTTP_response);
    memset(&pd_response, sizeof(pd_response), 0);
    pd_response.u.HTTP.data = buf;
    pd_response.u.HTTP.datalen = strlen(test_HTTP_response);

    token = HTTPProtoRespData2TokenData(0, &pd_response, &status, &respcode);
    assert(token != 0);
    assert(status == HP_SUCCESS);
    assert(respcode == 200);

    // Verify HTTPHdr2NV()
    pbuf = 0;
    pbuf_len = 0;
    nv_count = 0;

    rv = HTTPHdr2NV(token, 0, FL_2NV_SPDY_PROTO, &pbuf, &pbuf_len, &nv_count);
    assert(rv == 0);
    assert(pbuf != 0);
    assert(pbuf_len != 0);
    assert(nv_count != 0);

    for (n = 0; (n * 2) < nv_count * 2; n++) {
    	if (!strcmp(*(((char **)pbuf)+(n * 2)), "Date")) {
	    assert(!strcmp(*(((char **)pbuf)+((n * 2) + 1)), 
	    		   "Sat, 17 Jan 2015 02:14:18 GMT"));
	    break;
	}
    }
    assert(n < nv_count);

    printf("End proto_http_test [Success]\n");
}

/*
 * End of proto_http_test.c
 */

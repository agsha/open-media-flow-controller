/*
 * libnet unit test.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>
#include <assert.h>
#include "libnet/libnet.h"
#include "proto_http/proto_http.h"

int recv_data_event(libnet_conn_handle_t *handle,
		    char *data, int datalen, const libnet_conn_attr_t *attr)
{
    int rv;
    proto_data_t pd;
    token_data_t token;
    HTTPProtoStatus status;

    const char *uri;
    int urilen;
    int hdrcnt;

    char *location;

    char *respbuf;
    int respbuflen;

    pd.u.HTTP.destIP = attr->DestIP;
    pd.u.HTTP.destPort = *attr->DestPort;
    pd.u.HTTP.clientIP = attr->ClientIP;
    pd.u.HTTP.clientPort = *attr->ClientPort;
    pd.u.HTTP.data = data;
    pd.u.HTTP.datalen = datalen;

    token = HTTPProtoData2TokenData(0, &pd, &status);
    if (status != HP_SUCCESS) {
    	if (status == HP_MORE_DATA) {
	    return 1; // Callback when more data available
	}
    }

    while (1) {

    rv = HTTPGetKnownHeader(token, 0, H_X_NKN_URI, &uri, &urilen, &hdrcnt);
    if (rv) {
    	printf("HTTPGetKnownHeader(H_X_NKN_URI) failed, rv=%d", rv);
    	break;
    }

    location = alloca(urilen + 1024);
    strcpy(location, "http://MyRedirectHost:4040");
    strncat(location, uri, urilen);

    rv = HTTPAddKnownHeader(token, 1, H_LOCATION, location, strlen(location));
    if (rv) {
    	printf("HTTPAddKnownHeader(H_LOCATION) failed, rv=%d", rv);
	break;
    }

    rv = HTTPResponse(token, 302, (const char **)&respbuf, &respbuflen, 0);
    if (rv) {
    	printf("HTTPResponse() failed, rv=%d", rv);
	break;
    }

    rv = libnet_send_data(handle, respbuf, respbuflen);
    assert(rv >= 0);
    break;

    } // End while

    rv = libnet_close_conn(handle, 0);
    deleteHTTPtoken_data(token);
    return 0;
}


int recv_data_event_err(libnet_conn_handle_t *handle,
		 	char *data, int datalen,
			const libnet_conn_attr_t *attr, int reason)
{
    int rv;

    rv = libnet_close_conn(handle, 0);
    return 0;
}

int send_data_event(libnet_conn_handle_t *handle)
{
    int rv;

    rv = libnet_close_conn(handle, 0);
    return 0;
}

int timeout_event(libnet_conn_handle_t *handle)
{
    int rv;

    rv = libnet_close_conn(handle, 0);
    return 0;
}

main()
{
    libnet_config_t cfg;
    proto_http_config_t proto_http_cfg;
    int rv, err, suberr;

    memset(&cfg, 0, sizeof(cfg));

    cfg.interface_version = LIBNET_INTF_VERSION;
    cfg.libnet_recv_data_event = recv_data_event;
    cfg.libnet_recv_data_event_err = recv_data_event_err;
    cfg.libnet_send_data_event = send_data_event;
    cfg.libnet_timeout_event = timeout_event;

    cfg.libnet_listen_ports[0] = 9090;

    rv = libnet_init(&cfg, &err, &suberr, 0);
    if (rv) {
    	_exit(1);
    }

    memset(&proto_http_cfg, 0, sizeof(proto_http_cfg));
    proto_http_cfg.interface_version = PROTO_HTTP_INTF_VERSION;
    proto_http_cfg.options = OPT_NONE;
    rv = proto_http_init(&proto_http_cfg);
    if (rv) {
    	_exit(2);
    }

    while (1) {
        sleep(10);
    }
}

/*
 * End of libnet_test.c
 */

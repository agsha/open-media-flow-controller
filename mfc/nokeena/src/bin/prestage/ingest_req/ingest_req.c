#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#define UNIX_PATH_MAX 256

struct message {
    int         flags;
    char        type;
    char        _pad;
    short       len;

    int         nsname_len;
    int         host_len;
    int         pathlen;
    int		uri_len;
    int		size_len;
    char        data[1];
};

#define NKN_INGEST_SOCKET "/vtmp/ingester/uds-ingester-server"

int main(int argc, char **argv)
{
    struct sockaddr_un address;
    int  socket_fd, nbytes, n, addr_len;
    char buffer[256], c  ;
    struct message *data = NULL;
    const char *uri = NULL, *host = NULL, *ns_name = NULL
	, *size = NULL, *file = NULL;
    int uri_len = 0, path_len = 0, host_len = 0, size_len = 0, 
	nsname_len = 0, total_len = 0;

    while ((c = getopt (argc, argv, "f:u:h:n:l:")) != -1) {
	switch (c) {
	    case 'u':
		uri = optarg;
		break;
	    case 'h':
		host = optarg;
		break;
	    case 'n':
		ns_name = optarg;
		break;
	    case 'l':
		size = optarg;
		break;
	    case 'f':
		file = optarg;
		break;
	    default:
		break;
	}
    }

    if (uri == NULL || host == NULL || ns_name == NULL || size == NULL || file == NULL) {
	printf("values are NULL");
	return 1;
    }

    uri_len = strlen(uri);
    host_len = strlen(host);
    nsname_len = strlen(ns_name);
    size_len = strlen(size);
    path_len = strlen(file);

    total_len = sizeof(struct message) + uri_len + host_len + nsname_len + size_len + path_len - 1;
    data = malloc(total_len);
    if (data == NULL) {
	printf("malloc() failed");
	return 1;
    }

    data->len = total_len - 8; // DN len should only have the payload size

    data->nsname_len = nsname_len;
    memcpy(data->data, ns_name, nsname_len );
    data->host_len = host_len;
    memcpy(data->data+ nsname_len , host, host_len );
    data->uri_len= uri_len;
    memcpy(data->data+nsname_len+ host_len , uri, uri_len);
    data->size_len = size_len;
    memcpy(data->data+nsname_len+ host_len+ uri_len , size, size_len);
    data->pathlen = path_len;
    memcpy(data->data+nsname_len+ host_len+ uri_len+ size_len, file, path_len);

    printf("sending message - len- %d, %d, %d, %d, %d, %d\n", data->len, data->nsname_len, data->host_len,
	    data->uri_len, data->size_len, data->pathlen);

    printf("sending message - msg- %s, %s, %s, %s, %s\n", ns_name, host, uri, size, file);

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd < 0)
    {
	printf("socket() failed\n");
	return 1;
    }

    /* start with a clean address structure */
    //memset(&address, 0, sizeof(struct sockaddr_un));

    address.sun_family = AF_UNIX;
    n = snprintf(address.sun_path, UNIX_PATH_MAX, NKN_INGEST_SOCKET);
    addr_len = sizeof(address.sun_family) + n;

    if(connect(socket_fd, 
		(struct sockaddr *) &address, 
		addr_len) != 0)
    {
	printf("connect() %d failed\n", errno);
	return 1;
    }

    write(socket_fd, data, total_len);

    nbytes = read(socket_fd, buffer, sizeof(buffer));
    buffer[nbytes] = 0;

    printf("MESSAGE FROM SERVER: %s\n", buffer);

    close(socket_fd);

    return 0;
}

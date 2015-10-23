/**
 * @file   dns_server.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Thu Mar  1 04:18:07 2012
 * 
 * @brief  implements the DNS UDP and TCP listeners
 * 
 * 
 */
#include "dns_server.h"

#include "uint_ctx_cont.h"

/* config defaults */
extern cr_dns_cfg_t g_cfg;

/** 
 * starts the udp server on a dispatcher described by event_disp_t
 * on a given set of network interfaces
 * 
 * @param gl_ip_if_addr [in] - contains a NULL terminated array of
 * interfaces on which the server needs to listen
 * @param disp [in] - the event dispatcher on which the network
 * events needs to be sent
 * 
 * @return returns 0 on success and a non zero negative value on
 * error
 */
int32_t dns_start_udp_server(struct sockaddr_in **gl_ip_if_addr,
	event_disp_t *disp) 
{

    int32_t net_idx = 0, err = 0;
    int fd, val;

    while (gl_ip_if_addr[net_idx] != NULL) {

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
	    err = -errno;
	    DBG_LOG(SEVERE, MOD_NETWORK, "error opening socket "
		    "[err = %d]; exiting now", err);
	    goto error;
	}

	err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		sizeof(val));
	if (err == -1) {
	    err = -errno;
	    DBG_LOG(SEVERE, MOD_NETWORK, "error setting socket "
		    "option, err code = %d", err);
	    goto error;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(g_cfg.dns_listen_port);
	addr.sin_addr.s_addr = 
	    gl_ip_if_addr[net_idx++]->sin_addr.s_addr;
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
	    perror("bind: ");
	    goto error;
	}

	entity_data_t* entity_ctx = newEntityData(fd, NULL, NULL, 
		cr_dns_udp_epollin_handler, NULL, 
		cr_dns_udp_epollerr_handler, 
		cr_dns_udp_epollhup_handler, NULL);
	if (entity_ctx == NULL) {
	    err = -ENOMEM;
	    DBG_LOG(SEVERE, MOD_NETWORK, "error adding socket to "
		    "event dispatcher [fd = %d], [err = %d]",
		    fd, err);
	    goto error;
	}
	entity_ctx->event_flags |= EPOLLIN;
	disp->reg_entity_hdlr(disp, entity_ctx);
    }

error:
    return err;
}

int32_t dns_start_tcp_server(struct sockaddr_in **gl_ip_if_addr,
	event_disp_t *disp,
	obj_list_ctx_t *con_list)
{
    int32_t net_idx = 0, err = 0;
    int fd, val;
    nw_ctx_t *nw = NULL;
    dns_con_ctx_t *con = NULL;

    while (gl_ip_if_addr[net_idx] != NULL) {

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
	    err = -errno;
	    DBG_LOG(SEVERE, MOD_NETWORK, "error opening socket "
		    "[err = %d]; exiting now", err);
	    goto error;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(g_cfg.dns_listen_port);
	addr.sin_addr.s_addr = 
	    gl_ip_if_addr[net_idx++]->sin_addr.s_addr;

	err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		sizeof(val));
	if (err == -1) {
	    err = -errno;
	    DBG_LOG(SEVERE, MOD_NETWORK, "error setting socket "
		    "option, err code = %d", err);
	    goto error;
	}

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
	    perror("bind: ");
	    goto error;
	}

	err = dns_set_socket_non_blocking(fd);
	if (err) {
	    DBG_LOG(SEVERE, MOD_NETWORK, "error setting socket "
		    "to non-blocking");
	    goto error;
	}

	listen(fd, 10000);

	err = con_list->pop_free(con_list, (void **)&con);
	if (err) {
	    DBG_LOG(ERROR, MOD_NETWORK, "error finding a free connection "
		    "context [fd = %d], [err = %d]", fd, err);
	    goto error;
	}
	nw = dns_con_get_nw_ctx(con);
	nw->fd = fd;

	entity_data_t* entity_ctx = newEntityData(fd, con, freeConnCtxHdlr, 
		cr_dns_tcp_listen_handler, NULL, 
		cr_dns_tcp_epollerr_handler, 
		cr_dns_tcp_epollhup_handler, NULL);
	if (entity_ctx == NULL) {
	    err = -ENOMEM;
	    DBG_LOG(SEVERE, MOD_NETWORK, "error adding socket to "
		    "event dispatcher [fd = %d], [err = %d]",
		    fd, err);
	    goto error;
	}
	entity_ctx->event_flags |= EPOLLIN;
	disp->reg_entity_hdlr(disp, entity_ctx);
    }

error:
    return err;

}

int32_t
dns_set_socket_non_blocking(int fd)
{
    int opts = O_RDWR; //fcntl(fd, F_GETFL);
    int on;

    if(opts < 0) {
	return -1;
    }
    opts = (opts | O_NONBLOCK);
    if(fcntl(fd, F_SETFL, opts) < 0) {
	return -1;
    }

    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

    return 0;
}

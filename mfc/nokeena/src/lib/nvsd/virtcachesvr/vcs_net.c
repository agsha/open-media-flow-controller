/*============================================================================
 *
 *
 * Purpose: This file implements vcs_net functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>

#include "vcs_net.h"
#include "vcs_dbg.h"
#include "nkn_defs.h"
#include "vcs_private.h"

int vcs_net_listen()
{
    int s = -1, opt, rc;
    struct sockaddr_in addr;

    signal(SIGPIPE, SIG_IGN);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	DBG_VCE("socket() failed: %s\n", strerror(errno));
	goto cleanup;
    }

    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (rc < 0) {
	DBG_VCE("setsockopt(%d, SO_REUSEADDR): %s\n", s, strerror(errno));
	goto cleanup;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(VCCMP_SERVER_ADDR);
    addr.sin_port = htons(VCCMP_SERVER_PORT);

    rc = bind(s, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
	DBG_VCE("bind(%d) failed: %s\n", s, strerror(errno));
	goto cleanup;
    }

    rc = listen(s, VCS_N_CHANNELS);
    if (rc < 0) {
	DBG_VCE("listen(%d) failed: %s\n", s, strerror(errno));
	goto cleanup;
    }

    vc->acceptor.socket = s;
    vc->acceptor.port = VCCMP_SERVER_PORT;

    DBG_VCM3("socket(%d), port(%d)\n", s, VCCMP_SERVER_PORT);
    return 0;

cleanup:
    if (s >= 0) {
	close(s);
    }
    return -1;
}

void vcs_net_shutdown(void)
{
    DBG_VCM3("socket(%d)\n", vc->acceptor.socket);
    close(vc->acceptor.socket);
}

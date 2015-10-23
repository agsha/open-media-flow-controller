/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2012 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include "tms_dhcp_utils.h"


int
tms_dhcp_set_cloexec(int fd)
{
    int err = 0;

    if (fd < 0) {
        err = -1;
        goto bail;
    }

    err = fcntl(fd, F_SETFD, FD_CLOEXEC);
    /* Pass through the error return code */

 bail:
    return(err);
}

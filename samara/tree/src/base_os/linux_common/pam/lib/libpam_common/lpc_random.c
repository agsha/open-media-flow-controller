/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2013 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include "pam_common.h"

static char lpc_random_state[128];
static char *lpc_random_old_state = NULL;


/* ------------------------------------------------------------------------- */
/* Derived from lc_random_get_seed().
 */
static int
lpc_random_get_seed(int *ret_seed, int non_blocking)
{
    int fd = -1;
    int count = 0;
    int read_count = 0;
    int err = 0;
    char buf[4];

    if (ret_seed == NULL) {
        err = 1;
        goto bail;
    }

    *ret_seed = -1;

    fd = open("/dev/random", O_RDONLY);
    if (fd < 1) {
        err = 1;
        goto bail;
    }

    if (non_blocking) {
        err = lpc_set_nonblocking(fd, 1);
        if (err) {
            goto bail;
        }

    }

    do {
        read_count = read(fd, &buf[count], 4 - count);
        if (read_count == -1) {
            if (errno == EINTR) { continue; }
            if (errno == EAGAIN) { break; }
            err = 1;
            goto bail;
        }
        count += read_count;
    } while (count < 4);

    if (non_blocking && count < 4) {
        close(fd);

        fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0) {
            err = 1;
            goto bail;
        }

        err = lpc_set_nonblocking(fd, 1);
        if (err) {
            goto bail;
        }

        do {
            read_count = read(fd, &buf[count], 4 - count);
            if (read_count == -1) {
                if (errno == EINTR) { continue; }
                err = 1;
                goto bail;
            }
            count += read_count;
        } while (count < 4);
    }

    if (count != 4) {
        err = 1;
        goto bail;
    }

    *ret_seed = (((unsigned int)buf[0]) << 24 |
                 ((unsigned int)buf[1]) << 16 |
                 ((unsigned int)buf[2]) << 8 |
                 (unsigned int)buf[3]);

 bail:
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Derived from lc_random_seed_nonblocking().
 */
static int
lpc_random_seed_nonblocking(void)
{
    int err = 0;
    int seed = 0;

    err = lpc_random_get_seed(&seed, 1);
    if (err) {
        goto bail;
    }

    lpc_random_old_state = initstate(seed, lpc_random_state,
                                     sizeof(lpc_random_state));
    if (lpc_random_old_state == NULL) {
        err = 1;
        goto bail;
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
lpc_random_restore_old_state(void)
{
    int err = 0;
    char *old_state = NULL;

    if (lpc_random_old_state == NULL) {
        goto bail;
    }

    old_state = setstate(lpc_random_old_state);
    if (old_state == NULL) {
        err = 1;
        goto bail;
    }
    else if (old_state != lpc_random_state) {
        /*
         * Unexpected; what else would it have been, if we had put
         * something into lpc_random_old_state?
         */
        syslog(LOG_INFO, "Unexpected return from setstate(): %p", old_state);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Derived from lc_random_get_uint64().
 */
uint64_t
lpc_random_get_uint64(void)
{
    int err = 0;
    long int rnum = 0;
    uint64_t result = 0;

    err = lpc_random_seed_nonblocking();
    if (err) {
        syslog(LOG_WARNING, "Unable to seed random number generator");
        goto bail;
    }

    rnum = random();
    result = (uint64_t)(((uint64_t) rnum) & 0x0000ffff) << 48;

    rnum = random();
    result |= (uint64_t)(((uint64_t) rnum) & 0x0000ffff) << 24;

    rnum = random();
    result |= (uint64_t)(((uint64_t) rnum) & 0x0000ffff) << 16;

    rnum = random();
    result |= (uint64_t)(((uint64_t) rnum) & 0x0000ffff);

    err = lpc_random_restore_old_state();
    if (err) {
        syslog(LOG_WARNING, "Unable to restore random number generator state");
        goto bail;
    }

 bail:
    return(result);
}

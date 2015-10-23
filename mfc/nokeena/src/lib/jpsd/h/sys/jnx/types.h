/*
 * @file types.h
 * @brief
 * type.h - place holder to find equivalent data type in linux
 * and compilation flags
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _TYPES_H
#define _TYPES_H

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#define __unused __attribute__((unused))

#define TRUE 1
#define FALSE 0

#define index32_t uint32_t
#define atomic_u32_t uint32_t

#define strlcpy strncpy
#define strlcat strncat

#endif /* !_TYPES_H */



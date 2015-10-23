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

#if defined(PROD_TARGET_OS_FREEBSD)
    #include "apr.h.freebsd"
#elif defined(PROD_TARGET_ARCH_I386)
    #include "apr.h.i386"
#elif defined(PROD_TARGET_ARCH_X86_64)
    #include "apr.h.x86_64"
#elif defined(PROD_TARGET_ARCH_PPC)
    #include "apr.h.ppc"
#endif

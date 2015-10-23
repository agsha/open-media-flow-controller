/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2006 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#include <stdlib.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
int
gpl_lt_timezone_handle_changed(void)
{
    time_t lt = 0;

    /* This assumes the TZ environment variable was not already set */
    putenv("TZ=error");
    tzset();
    unsetenv("TZ");

    localtime(&lt);

    return(0);
}

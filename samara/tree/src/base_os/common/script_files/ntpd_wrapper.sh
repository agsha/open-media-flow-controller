#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2012 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

if [ -r /etc/build_version.sh ]; then
    . /etc/build_version.sh
fi

#

# NOTE: this conversion is OBSOLETE, and we do NOT currently use this
# wrapper script.  This wrapper was used when there were differences in
# the arguments that ntpd took between two at-the-time concurrently
# supported base os versions.
#
# We leave the script in the tree as an example, in case ntpd or another
# daemon needs any argument conversion in the future.
#

# We convert everything that's not of a certain version (xxx to be filled in)
if [ "x${BUILD_TARGET_PLATFORM_FULL}" = "xLINUX_xxx" ]; then
    do_convert=0
else
    do_convert=1
fi

if [ "${do_convert}" -eq 1 ]; then
    new_args=`echo "$*" | sed 's/-U/-u/'`
else
    new_args=`echo "$*"`
fi

exec /usr/sbin/ntpd ${new_args}

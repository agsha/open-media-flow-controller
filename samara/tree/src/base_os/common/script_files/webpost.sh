#!/bin/sh
#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2006 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#
# This script uses HTTP to POST a file to a URL.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


CURL=/usr/bin/curl

usage()
{
    echo "usage: $0 url file"
    echo ""
    exit 1
}

if [ $# != 2 ]; then
    usage
fi

URL=$1
FILE=$2

if ! $CURL --data-binary @$FILE $URL > /dev/null; then
    echo "Failed to upload data"
    exit 1
fi

exit 0

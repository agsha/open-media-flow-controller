#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2011 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

#
# This script is to be run on the staging area into which the RPM
# is extracted.
#

umask 022

PARCH=$1
if [ -z "${PARCH}" ]; then
    PARCH=I386
fi

if [ "${PARCH}" = "X86_64" ]; then
    PARCH_APPEND=".x86_64"
else
    PARCH_APPEND=
fi

#
# Create a symlink to the 'jdb' binary
#
(
    mkdir -p usr/bin ;
    cd usr/bin ;
    ln -s ../lib/jvm/java-1.6.0-openjdk-1.6.0.0${PARCH_APPEND}/bin/jdb .
)

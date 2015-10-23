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

#
# This script is to be run on the staging area into which the RPM
# is extracted.
#

umask 022

#
# Notes:
#
#   * We use relative paths on purpose, since we are run from within
#     the RPM extract staging area.  Absolute paths would try to operate
#     on the tcpdump utility on the build machine!
#
#   * The chmod must come after the chgrp.  The chgrp command will clear
#     the setuid bit!
#
#   * 3000 == mgt_diag (from md_client.h)
#
chgrp 3000 usr/sbin/tcpdump
chmod 04750 usr/sbin/tcpdump

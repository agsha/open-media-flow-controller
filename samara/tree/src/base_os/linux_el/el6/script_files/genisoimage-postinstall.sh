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

# This is needed because the '/usr/bin/mkisofs' symlink is a '%ghost'
# file in the RPM, so it does not really exist.  But we want the
# symlink, and aren't going to do any fancy 'alternatives' stuff to get
# it, so we just make it ourselves.

(cd ./usr/bin ; ln -s genisoimage mkisofs)

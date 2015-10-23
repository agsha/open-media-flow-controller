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

#
# This script is called by smartd when a disk event comes up.  Our goal is
# to read the environment variables it hsa set for us, and use mdreq to send
# an event to mgmtd describing what has happened.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


# We already know what host we're on
SMARTD_SUBJECT=`echo ${SMARTD_SUBJECT} | sed 's, on host:.*$,,'`

# We background this mdreq so as not to block smartd if mgmtd is slow
/opt/tms/bin/mdreq event /smart/events/warning /events/short_description string "${SMARTD_SUBJECT}" /events/full_description string "${SMARTD_MESSAGE}" /smart/notification/warning/device string "${SMARTD_DEVICE}" &


# SMARTD_MAILER=/sbin/smartev.sh
# SMARTD_SUBJECT=SMART error (emailtest) detected on host: tb1.tallmaple.com
# SMARTD_TFIRSTEPOCH=1067282601
# SMARTD_FAILTYPE=emailtest
# SMARTD_TFIRST=Mon Oct 27 19:23:21 2003 GMT
# SMARTD_DEVICETYPE=ata
# SMARTD_DEVICE=/dev/hda
# SMARTD_DEVICESTRING=/dev/hda
# SMARTD_MESSAGE=TEST EMAIL from smartd for device: /dev/hda

#!/bin/bash

#       File: raid_events.sh
#
#       Description: The script that parses the md events. This
#	is run every time an event happens and is started by raid_monitor.sh
#
#       Created On : 12 June, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# Set the Variables
# event is in the format listed in mdadm_events.conf file
# device is in the format /dev/mdXX
event=$1
device=$2
component=$3

# MDADM log file
MDADM_LOG=/var/log/nkn/mdadm_events.log

# MD Events configuration file
MDEVENTS_CONF=/config/nkn/mdadm_events.conf

# This conf file contains the list of MD events that needs to be monitored.
MONITORED_MDEVENTS_CONF=/nkn/tmp/monitored_mdevents.conf

# Generate the monitored md events file.
grep -v "#" $MDEVENTS_CONF > $MONITORED_MDEVENTS_CONF

# logic to check only the events that needs to be monitored based
# on the configuration file.
grep -q "${event}" $MONITORED_MDEVENTS_CONF
if [ $? -eq 0 ]; then
  case "${event}" in
  Fail)
    date >> $MDADM_LOG
    echo "Mirrorbroken event is detected on device" $device "and component" $component >> $MDADM_LOG
    logger -s "Mirrorbroken event is detected on device $device"
    /opt/tms/bin/mdreq event /nkn/nvsd/events/root_disk_mirror_broken /nkn/monitor/mirror-broken-error string "Mirrorbroken event is detected on device $device"
    ;;
  RebuildFinished)
    date >> $MDADM_LOG
    echo "Mirrorcomplete event is detected on device" $device >> $MDADM_LOG
    logger -s "Mirrorcomplete event is detected on device $device"
    /opt/tms/bin/mdreq event /nkn/nvsd/events/root_disk_mirror_complete /nkn/monitor/mirror-complete  string "Mirrorcomplete event is detected on device $device"
    ;;
  *)
    date >> $MDADM_LOG
    echo "A Mirroring event " $event " is detected on device" $device >> $MDADM_LOG
    logger -s "A Mirroring event $event is detected on device $device"
    ;;
  esac
fi

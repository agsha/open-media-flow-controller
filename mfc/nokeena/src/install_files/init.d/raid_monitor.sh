#!/bin/bash

# 	File: raid_monitor.sh
#
#       Description: The script that starts monitoring the raid
#		arrays configured thru mdadm and executes the PROGRAM
#		when an event is triggered.
#
#       Created On : 12 June, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# Set the Variables
MDADM=/sbin/mdadm

# delay indicates how frequently md polls the devices
# We are going to use 300 seconds delay.
DELAY=300

# PROGRAM is executed whenever an event is triggered and reads the
# MONITORED_MDEVENTS_CONF  file and acts accordingly for the events.
PROGRAM=/opt/nkn/bin/raid_events.sh

# Start the mdadm in monitor mode
# Options used are:
# --monitor	- to start in monitor mode
# --scan	- to scan for md devices
# --delay	- Indicates how frequently md polls the raid devices
# --daemonize	- to start in background
# --program	- the program that needs to be run when an event occurs
$MDADM --monitor --scan --delay=$DELAY --daemonize --program $PROGRAM

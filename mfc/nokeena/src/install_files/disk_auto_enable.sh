#! /bin/bash
#
#	File : disk_auto_enable.sh
#
#	Description : This script enables the disks that are automatically
#                     disabled by DM2
#
#	Written By : Jeya ganesh babu (jjeya@juniper.net)
#
#	Created On : 11 February, 2011 
#
#	Copyright (c) Juniper Networks, 2011
#
#! /bin/bash
# TMPCMD is the temp file used to give cli command to enable the disk
# TMPOUT is the temp file used to give cli command to show the disk list
# LOG is the log file of the enable activity
# The scripts runs the show media-cache disk list. It finds all the disks
#    that are disabled. If the disabled disk shows disk active field as yes
#    and cache field as yes, it means that the disk has been automatically
#    disabled. The script enables it again.

TMPCMD=/nkn/tmp/disk_auto_enable.cmd
TMPOUT=/nkn/tmp/disk_auto_enable.out
LOG=/nkn/tmp/disk_auto_enable.log
DISK_DISABLE_SLEEP_TIME=1800

while [ true ]; do
/opt/tms/bin/cli << EOF > $TMPOUT
en
configure t
show media-cache disk list
EOF
  cat $TMPOUT | while read line; do
    if echo $line | grep -q "disk cacheable, but cache not enabled" ; then
      enabled=`echo $line | awk '{print $5}' | sed s@/.*@@`
      active=`echo $line | awk '{print $4}' | sed s@/.*@@`
      disk=`echo $line| awk ' {print $1}' | sed s@/.*@@`
      if [ $active == "yes" ] && [ $enabled == "yes" ]
      then
        curtime=`date`
        echo $curtime: Enabling disk $disk >> $LOG
/opt/tms/bin/cli << EOF > $TMPCMD
en
configure t
media-cache disk $disk ca enable
EOF
      fi
    fi
  done
  sleep $DISK_DISABLE_SLEEP_TIME
done


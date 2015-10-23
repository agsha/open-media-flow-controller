#! /bin/bash
#
#       File : delete_one_drive_from_TMdb.sh
#
#       Description : This script deletes the disk info from the tall maple db
#
#       Created By : Raja Gopal Andra (randra@juniper.net)
#
#       Created On : 16 April, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# Set the Variables
CLI=/opt/tms/bin/cli
TMPOUT=/nkn/tmp/getonediskdata.out

# set the exit codes
# 1 = internal error
# 2 = not supported model


#usage info
usage() {
  echo "delete_one_drive_from_TMdb.sh <drive_id>"
  echo "  -h: help"
  exit 1
}

if [ $# -lt 1 ]; then
  usage;
fi


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
#--------------------------------------------------------------

# We are going to use cli interface to do this. If nvsd is not present,
# then we might have to call mdreq to delete the disk info directly.
# Syntax for same is:
# /opt/tms/bin/mdreq -s query  iterate - /nkn/nvsd/diskcache/config/disk_id
# /opt/tms/bin/mdreq set delete - /nkn/nvsd/diskcache/config/disk_id/<Disk_id>

$CLI << EOF > $TMPOUT
en
con t
internal query get - /nkn/nvsd/diskcache/config/disk_id/$1
EOF
cat $TMPOUT | while read line; do
  echo $line | grep -q "No bindings returned." 
  if [ $? == 0 ]; then
    echo "Disk " $1 " not present in tall maple db"
  else
$CLI << EOF > $TMPOUT
en
con t
internal set delete - /nkn/nvsd/diskcache/config/disk_id/$1
EOF
  fi
done

#
# End of delete_one_drive_from_TMdb.sh
#

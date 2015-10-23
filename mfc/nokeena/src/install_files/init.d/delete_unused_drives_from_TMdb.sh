#! /bin/bash
#
#       File : delete_unused_drives_from_TMdb.sh
#
#       Description : This script deletes the disk info from the tall maple db
#
#       Created On : 13 September, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# Set the Variables
CLI=/opt/tms/bin/cli
TMPDISKID=/nkn/tmp/getdiskid.out
MDREQ=/opt/tms/bin/mdreq
MDDBREQ=/opt/tms/bin/mddbreq
DB_DELETE_CMD=
DB_ITERATE_CMD=
ACTIVE_DB=
LOG_FILE=/log/init/delete_unused_drives_from_TMdb.log

# set the exit codes
# 1 = internal error
# 2 = not supported model

if [ ! -e /opt/nkn/bin/disks_common.sh ]; then
  logger -s "delete_unused_drives_from_TMdb.sh: Missing library file"
  exit 1;
fi
source /opt/nkn/bin/disks_common.sh

#usage info
usage() {
  echo "delete_unused_drives_from_TMdb.sh"
  exit 1
}

function setup_db_cmd()
{
  ps -e | grep -v grep | grep -q mgmtd
  if [ ${?} -eq 0 ]; then
    if [ ! -x ${MDREQ} ]; then
      logger -s "${ME}: ERROR: Command to update DB is missing: ${MDREQ}"
      return 1
    fi
    DB_DELETE_CMD="${MDREQ} set delete -"
    DB_ITERATE_CMD="${MDREQ} -v query iterate -"
  else
    if [ ! -x ${MDDBREQ} ]; then
      logger -s "${ME}: ERROR: Command to update DB is missing: ${MDDBREQ}"
      return 2
    fi
    ACTIVE_DB="/config/db/`cat /config/db/active`"
    DB_DELETE_CMD="${MDDBREQ} ${ACTIVE_DB} set delete -"
    DB_ITERATE_CMD="${MDDBREQ} -v ${ACTIVE_DB} query iterate -"
  fi
  # Verify that the command will work.
  echo "${DB_ITERATE_CMD} /nkn/nvsd/diskcache/config/disk_id" > ${LOG_FILE} 2>&1
  ${DB_ITERATE_CMD} /nkn/nvsd/diskcache/config/disk_id >> ${LOG_FILE} 2>&1
  if [ ${?} -ne 0 ]; then
    echo "ERROR: Failed: ${DB_ITERATE_CMD} ${TM_DISKID}" >> ${LOG_FILE}
    logger -s "${ME}: ERROR: Failed: ${DB_ITERATE_CMD} ${TM_DISKID}"
    return 3
  fi
  return 0
}

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
#--------------------------------------------------------------

# Check if system is VXA 2100 or not
if ! is_vxa_2100 ; then
  Print "delete_unused_drives_from_TMdb.sh is not supported on this platform"
  Print "delete_unused_drives_from_TMdb.sh is only supported on VXA 2100"
  exit 2
fi

setup_db_cmd
if [ ${?} -ne 0 ]; then
  logger -s "${ME}: ERROR: cannot write to configuration DB"
  exit 1
fi

echo "Iterate command is :" $DB_ITERATE_CMD >> ${LOG_FILE} 2>&1
echo "Delete command is :" $DB_DELETE_CMD >> ${LOG_FILE} 2>&1

# Get all the disk_ids from Tall maple DB
# We are going to use mddbreq directly as this script needs to be run before
# initdisks and can not use nvsd CLI interface during boot time. The DB*CMD's
# are set appropriately.
$DB_ITERATE_CMD /nkn/nvsd/diskcache/config/disk_id > $TMPDISKID

# Now check if there are any serial# which are not in findslot output
for i in `cat $TMPDISKID`; do
  grep -q $i $LASTFINDSLOTFILE
  if [ $? -ne 0 ]; then
    # The disk info exists only in tall maple db
    echo "The disk info $i exists only in tall maple db" >> ${LOG_FILE} 2>&1
    echo "$DB_DELETE_CMD /nkn/nvsd/diskcache/config/disk_id/$i" >> ${LOG_FILE} 2>&1
    $DB_DELETE_CMD /nkn/nvsd/diskcache/config/disk_id/$i
    continue;
  fi
done

#
# End of delete_unused_drives_from_TMdb.sh
#

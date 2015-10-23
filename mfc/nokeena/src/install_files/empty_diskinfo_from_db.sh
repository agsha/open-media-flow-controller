#! /bin/bash
#
#	File : empty_diskinfo_from_db.sh
#
#	Description : This script deletes all disk information from
#		the TallMaple DB.  It does the opposite of
#		write_diskinfo_to_db.sh
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 15 February, 2009
#
#	Copyright (c) Nokeena Inc., 2009
#

PATH=$PATH:/sbin
CACHE_CONFIG=/config/nkn/diskcache-startup.info
if [ -x /opt/nkn/bin/smartctl ]; then
  SMARTCTL=/opt/nkn/bin/smartctl
else
  SMARTCTL=/usr/sbin/smartctl
fi
TM_DB_IS_EMPTY=0
MDREQ=/opt/tms/bin/mdreq

# TallMaple Nodes
TM_DISKID=/nkn/nvsd/diskcache/config/disk_id
TM_ACTIVATED=activated
TM_CACHE_ENABLED=cache_enabled
TM_COMMENTED_OUT=commented_out
TM_FS_PART=fs_partition
TM_MISSING_AFTER_BOOT=missing_after_boot
TM_RAW_PART=raw_partition
TM_SET_SEC_CNT=set_sector_count
TM_SUBPROV=sub_provider

NKN_MDREQ_LOG=/nkn/tmp/empty_diskinfo_from_db.log

# Local functions

function list_disk_caches()
{
  ${MDREQ} -v query iterate - ${TM_DISKID}
}	# list_disk_caches


function empty_diskcache_info_from_db()
{
  local disks=`list_disk_caches`

  for d in ${disks}; do
    ${MDREQ} set delete - ${TM_DISKID}/${d}
  done
}

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Create the cache partitions and filesystems
echo
rm -f $NKN_MDREQ_LOG

logger -s "Deleting disk cache information from DB."
empty_diskcache_info_from_db

# Make DB change persistent
${MDREQ} action /mgmtd/db/save > /nkn/tmp/empty_diskinfo_from_db.save_db 2>&1
RET=$?
if [ $RET -ne 0 ]; then
  logger -s "ERROR: Failed to persist DB: $RET"
fi

echo

exit 0

#
# End of initdisks.sh
#

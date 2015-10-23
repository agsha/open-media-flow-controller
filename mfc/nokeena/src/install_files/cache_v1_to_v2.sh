#! /bin/bash

ME=cache_v1_to_v2.sh
CACHE_VERS=/opt/nkn/bin/cache_version
ATTR_CONV=/opt/nkn/bin/attr_v1_to_v2

usage() {
  echo "Usage: ${ME} <mount pt>"
  exit 1
}

#
# Upgrade the namespace folder directory from 1.0.4 to 1.0.5 version.
# This replacement_namespace should be only applied for this upgrade.
# -- Max He
function find_namespace()
{
/opt/tms/bin/cli << EOF
   en
   config t
   sh namespace $1
EOF
}


#
# It's possible that we will not rename the directory because we might not
# find the namespace.  The namespace won't be present if:
#
# 1. The namespace was deleted by the user and the URIs have not been evicted
#	before we attempt an upgrade.
#
# 2. The admin performs a re-MFG install with the "keep-cache" option.  In
#	this case, the TM db is deleted which means that all namespaces are
#	gone. It's possible to make this case work by forcing the below code
#	to exit with an error if the namespace is not found. Then the
#	namespace could be added and the disk re-activated.  The problem is
#	that we can't easily tell this case from case #1.  Is it enough to
#	also determine that *no* namespace exist and that we are booting from
#	a re-manufacturing.
#
function replacement_namespace()
{
  curpwd=`pwd`
  cd ${MNT}
  for oldname in *
  do
    # Assume that : can not appear in namespace or origin names.  This makes
    # the call idempotent
    name=`echo $oldname | /usr/bin/cut -d':' -f3`
    if [ "${name}" == "" ]; then
      namespace=`echo $oldname | /usr/bin/cut -d':' -f1`
      orgsvr=`find_namespace ${namespace} | /bin/grep Origin-Server | /usr/bin/cut -d'/' -f3`
      # If origin server not found, then the rename doesn't happen
      if [ "${orgsvr}" != "" ]; then
        newname=${oldname}"_"${orgsvr}
#       echo "/bin/mv $oldname $newname"
        /bin/mv $oldname $newname
      fi
    fi
  done
  cd ${curpwd}
}

#############################################################################
#
# Main
#
#############################################################################
if [ $# -lt 1 ]; then
  usage
fi
MNT=$1
BASENAME=`basename ${MNT}`
ERRFILE=/nkn/tmp/cache_version.${BASENAME}.err
LOGFILE=/nkn/tmp/cache_version.${BASENAME}.log

# Don't re-direct stdout
version=`${CACHE_VERS} ${MNT} 2> ${ERRFILE}`
ret=$?
if [ $ret -ne 0 ]; then
  logger -s "[${ME}] cache_version get failed: $MNT (ret=$ret)"
  exit 1
fi

# Must have version 1 to continue
if [ "$version" != "1" ]; then
  logger -s "[${ME}] run on incorrect version cache: $version"
  exit 1
fi

# Change top level directory names
replacement_namespace

# Look for V1 attribute file name 'attributes'
# Since there could be spaces in the directory name, set the IFS to not take
# space as delimiter.
IFS=$'\t\n'
EMPTY=`find $1 -name attributes | wc -l`
if [ "$EMPTY" != "0" ]; then
  find $1 -name attributes | xargs -n1 ${ATTR_CONV} -f ${LOGFILE} >> ${ERRFILE} 2>&1
  ret=$?
  # For xargs to exit with 0, all invocations must succeed.
  if [ $ret -ne 0 ]; then
    logger -s "[${ME}] find failed: $MNT (ret=$ret)"
    exit 1
  fi
fi
IFS=$' \t\n'
# If the cache is empty, we can just change the version.

# Write version 2 to bitmap.
${CACHE_VERS} -w 2 ${MNT} >> ${ERRFILE} 2>&1
ret=$?
if [ $ret -ne 0 ]; then
  logger -s "[${ME}] failed to update version to 2 (ret=$ret)"
  exit 1
fi


#! /bin/bash

ME=cache_convert.sh
GET_VERS=/opt/nkn/bin/cache_version
CACHE_CONV=/opt/nkn/bin/cache_v1_to_v2.sh

usage() {
  echo "Usage: ${ME} <mount pt>"
  exit 1
}

if [ $# -lt 1 ]; then
  usage
fi
MNT=$1
BASENAME=`basename ${MNT}`
ERRFILE=/nkn/tmp/cache_convert.${BASENAME}.err
LOGFILE=/nkn/tmp/cache_convert.${BASENAME}.log

# Don't re-direct stdout
version=`${GET_VERS} ${MNT} 2> ${ERRFILE}`
ret=$?
if [ $ret -ne 0 ]; then
  logger -s "[${ME}] cache_version failed: $MNT (ret=$ret)"
  exit 1
fi

# If we are already at version 2, don't do anything
if [ "$version" = "1" ]; then
  ${CACHE_CONV} ${MNT} >> ${ERRFILE} 2>&1
  ret=$?
fi
exit $ret

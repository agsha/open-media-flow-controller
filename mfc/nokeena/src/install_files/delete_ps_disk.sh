#!/bin/bash

INIT_CACHE_PS_DISKS_FILE=/config/nkn/ps-disks-list.txt
MP_FILE=/tmp/ps-disks-list.tmp

rm -f ${TMP_FILE}
if [ -f ${INIT_CACHE_PS_DISKS_FILE} ] ; then
  mv ${INIT_CACHE_PS_DISKS_FILE} ${TMP_FILE}
  grep -v "^${1}\$" ${TMP_FILE} > ${INIT_CACHE_PS_DISKS_FILE}
fi
rm -f ${TMP_FILE}

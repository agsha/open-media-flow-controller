#! /bin/bash
DISK=$1
BUCKET=$2

usage() {
  echo "dump_uri_list.sh <disk> <bucket>"
  echo "where, disk is dc_x"
  echo "	bucket is evict bucket number, -1 for all"
  exit 1
}

if [ $# -lt 2 ]; then
  usage;
fi


ARGLIST="-N dump_disk:${DISK} -N evict_bkt:$BUCKET}"

ret=1
while [ $ret -ne 0 ]; do
  # re-use the remove function rather than inventing a new one
  /opt/nkn/bin/nknfqueue $ARGLIST -s foo -r /tmp/ignore -q /tmp/FMGR.queue
  ret=$?
  if [ $ret -ne 0 ]; then
    sleep 1;
  fi
done

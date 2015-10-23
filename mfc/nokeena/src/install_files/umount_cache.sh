#! /bin/bash
# $1 == mount path

mnt_path=$1
disk_name=`echo $mnt_path|cut -d "/" -f 4`
evict_process_id=`ps -eo pid,args|grep -w ${disk_name}|egrep -v "grep|umount"| sed 's/^ *//g' | cut -d " " -f 1`
if [ "$evict_process_id" != "" ]; then
    kill -9 $evict_process_id
    sync
fi
out=`/bin/umount $mnt_path`
ret=$?
if [ $ret -ne 0 ]; then
    logger -s "ERROR: umount $mnt_path failed: $ret"
    logger -s "$out"
    exit 1
fi
exit 0


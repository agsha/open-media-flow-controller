#!/bin/sh
if [ $# -lt 1 ]; then
  echo "Usage: wd_dbg.sh <alarm-name>"
  exit 0
fi

alarm=$1
shift

pid=`pidof nvsd`

if [ "$pid" == "" ]; then
	exit 1
fi



outputfilename=/vtmp/watchdog_bt.${pid}
sysdumpdir=/var/log/nkn/
cmdfilename=/vtmp/${alarm}

echo "Process Name: NVSD" > $outputfilename
echo "PID: $pid" >> $outputfilename
echo "===============================================================" >> $outputfilename

/usr/bin/gdb --batch --quiet -pid $pid -command $cmdfilename >> $outputfilename

nohup /bin/mv $outputfilename $sysdumpdir  2> /dev/null &

#!/bin/sh

if test $# -eq 0 
then
	echo "--------------------------------------"
	echo "This is a run-time debug script."
	echo "output filename is 'output.txt'"
	echo ""
	echo "   > debug.sh "gdb_command""
	echo "e.g."
	echo "   > debug.sh \"info threads\""
	echo "   > debug.sh \"info threads\" \"thread apply all bt\" \"p glob_adns_cache_timeout\""
	echo "--------------------------------------"
	echo ""
	exit
fi

pid=`ps -elf | grep "nvsd" | grep -v "pipe_w" | awk '{ print $4}'`
cmdfilename=/tmp/gdb.bat
outputfilename=/tmp/output.bat

echo "" > $cmdfilename
while test $# -gt 0
do
	echo "$1" >> $cmdfilename
	shift
done
echo "detach" >> $cmdfilename
echo "quit" >> $cmdfilename
#cat $cmdfilename

/usr/bin/gdb -pid $pid -command $cmdfilename > $outputfilename
#/bin/rm -rf $cmdfilename

echo ""
echo "gdb output filename is '$outputfilename'"
echo ""


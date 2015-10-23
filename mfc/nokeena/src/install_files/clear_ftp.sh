#!/bin/bash

# $1 is $HOME
DIR=$HOME
DELETE_INFO_TMP=$DIR/tmp.info
find $DIR  -type d -mmin +480 -name "*"  | sort -r > $DELETE_INFO_TMP
echo "deleting directories $DELETE_INFO_TMP"
if [ -f $DELETE_INFO_TMP ] ; then
    while read line
    do
	fno=`find $line -name "*" | grep -v ".info$" | wc -l`
	echo $line $fno;
	if [ $fno -lt 2 ] ; then
	    echo "deleting $line";
	    rm -fr $line
	fi
    done < $DELETE_INFO_TMP
    rm -f $DELETE_INFO_TMP
else
    echo "no $DELETE_INFO_TMP found";
fi

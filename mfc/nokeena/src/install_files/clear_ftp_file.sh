#!/bin/bash

# $1 is $HOME
DIR=$1
AGE=$2
DELETE_INFO_TMP=$DIR/ftmp.info
CLEAN_INFO=$DIR/clean.info
find "$DIR"  -type f -mmin +${AGE} -name "*"  | sort -r | grep -v ".info$" > "$DELETE_INFO_TMP"
echo "deleting directories $DELETE_INFO_TMP"
if [ -f $DELETE_INFO_TMP ] ; then
    while read line
    do
	fname=`basename "$line"`
	up_info=`dirname "$line"`/upload.info
	/bin/rm -f "${line}";
	if [ -f "$up_info" ]
	then
	    cat "$up_info" | grep -vw "${fname}" > "$CLEAN_INFO";
	    /bin/mv "$CLEAN_INFO" "$up_info";
	fi
	echo "deleting $line, $fname, $up_info" ;
    done < $DELETE_INFO_TMP
    /bin/rm -f "$DELETE_INFO_TMP"
else
    echo "no $DELETE_INFO_TMP found";
fi

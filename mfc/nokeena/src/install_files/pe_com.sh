#! /bin/bash
#
#       File : pe_com.sh
#
#       Modified by : Thilak Raj S
#       Created Date : 21st June, 2010
#
#       Copyright notice ( © 2010 Juniper Networks Inc., All rights reserved)
#

# Local Environment Variables
PE_LIB=/opt/nkn/bin/policy/pe_files.sh

function usage()
{
	echo
		echo "Usage: pe_com.sh  <filename>"
		echo
		exit 1
}

# Get the parameters into readable variable names
function parse_params()
{
	FILE_NAME=$1;
	shift;
}

#Check to see if we have one argument
if [ $# -lt 1 ]; then
	echo usage;
fi

# Now get the parameters
parse_params $*;

# Include the pe files
if [ -f $PE_LIB ]; then
	. $PE_LIB;
else
	echo "Failed to include $PE_LIB, unable to continue";
	exit 2;
fi

#bkup the commit file
if [ -f ${PE_DIR}/${FILE_NAME}.tcl.com ]; then

	mv ${PE_DIR}/${FILE_NAME}.tcl.com ${PE_DIR}/${FILE_NAME}.tcl.bk
else
	#log a warning
	echo "Dont have a back up"
fi

if [ -f ${PE_DIR}/${FILE_NAME}.tcl.scratch ]; then
	cp ${PE_DIR}/${FILE_NAME}.tcl.scratch ${PE_DIR}/${FILE_NAME}.tcl.com
	echo "Policy Committed"
else
	#log a error
	echo "Something wrong"
	exit 1
	#something has gone wrong
fi

#
#
# Endof pe_com.sh
#


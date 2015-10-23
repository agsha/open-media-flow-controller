#! /bin/bash
#
#       File : pe_new.sh
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
		echo "Usage: pe_new.sh  <filename>"
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

# Check to see if a policy already exists
if [ -f ${PE_DIR}/${FILE_NAME}.tcl.scratch ]; then
	echo "Policy Already Existing.Please try a different name"
	exit 1;
fi

#cp the sample to scratch file
if [ -f ${PE_DIR}/${PE_SAMPLE} ]; then

	cp ${PE_DIR}/${PE_SAMPLE} ${PE_DIR}/${FILE_NAME}.tcl.scratch

	#Change the permission accoringly
	chmod 664 ${PE_DIR}/${FILE_NAME}.tcl.scratch

	#vi into the scratch file
	chroot ${PE_DIR} ${VI} -n ${FILE_NAME}.tcl.scratch 
else
	echo "Policy template file missing"
	exit 1
fi

#
#
# Endof pe_new.sh
#


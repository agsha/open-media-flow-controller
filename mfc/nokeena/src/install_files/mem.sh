#! /bin/sh
#
#	File : mem.sh
#
#	Description : This script  gets vmem info from ps output
#
#	Created By : Dhruva Narasimhan (dhruva@ankeena.com)
#
#	Created On : 02 January, 2009
#
#	Copyright (c) Ankeena Inc., 2008-2009
#


# Typicaly display the usage 
function usage()
{
  #echo
  #echo "Usage: $0 <pid> <vmem field> <proc name>"
  #echo 

  # Return 0 as the value for now since the avlue in stdout is used in mgmtd 
  echo 0;
  exit 1
}


# Get the parameters into readable variable names
function parse_params()
{
	PROC_PID=$1;
	shift;

	VMEM_FIELD=$1;
	shift;

	PROC_NAME=$1;
}

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Check if min number of parametes are passed
if [ $# -lt 3 ]; then
	usage;
fi

# Get the params
parse_params $*;

# if pid is 0 then we need to get pid from proc name
if [ "${PROC_PID}" = "0" ]; then
	# Get the PID from the proc name 
	if [ "${PROC_NAME}" = "" ]; then
		# We cannot get the pid hence return 0
		echo "0";
		exit 1;
	fi

	# Now get the PID if possible
	PROC_PID=`ps ax | grep ${PROC_NAME} | grep -v grep | cut -c1-5 | sed -e s/' '//g | head -n1`
fi

#cat /proc/31310/status | grep VmPeak | awk '{print $2}'

# Ensure the PID's status file exists
if [ -f /proc/${PROC_PID}/status ]; then
	ret=`cat /proc/${PROC_PID}/status | grep ${VMEM_FIELD} | awk '{print $2}'`
	# Also check to ensure ret has a value
	if [ "${ret}" = "" ]; then
		ret=0;
	fi
else
	# If PID not found then return 0
	ret=0;
fi

# Return the value in stdout 
echo $ret

#
# mem.sh
#

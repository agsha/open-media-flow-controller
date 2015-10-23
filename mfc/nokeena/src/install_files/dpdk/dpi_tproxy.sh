#!/bin/bash
#
#	File : dpitproxy 
#
#	Author : Ramanand Narayanan (ramanandn@juniper.net)
#	
#	(C) Copyright 2014 Juniper Networks, Inc.
#	All Rights Reserved
#


# Local Environment Variables
RETVAL=0;
ROOT_DIR=/opt/dpi-analyzer
SCRIPT_DIR=$ROOT_DIR/scripts
PID_FILE=/var/run/http_analyzer.pid
INIT_SCRIPT=/opt/dpdk/switch_intf_dpdk.sh
RUN_SCRIPT=/opt/nkn/sbin/http_analyzer
	
# Function to restart the server 
restart() {
	# Perform program exit housekeeping
	echo "restart dpitproxy";
	logger "restarting dpitproxy";
	kill -HUP `cat ${PID_FILE}`;
}

# Function to clean up the server sending SIGTERM to the PID 
clean_up() {
	# Perform program exit housekeeping
	echo "Cleaning-up ";
	logger "cleaning-up dpitproxy server";
	kill `cat ${PID_FILE}`;
        sleep 5 
	umount /mnt/huge
	exit $RETVAL;
}

# Handle the standard signals
trap 'clean_up' INT
trap 'clean_up' TERM
# trap 'restart' HUP  # Restart not supported for now
trap 'clean_up' HUP 
trap 'clean_up' QUIT 

# Making this a daemon process
sleep_forever() {
	while /bin/true ; do
		sleep 3
		# Check if http_analyzer is running
		ps -p `cat ${PID_FILE}` > /dev/null
		if [ $? == 1 ]; then
			clean_up;
			echo "warning: http analyzer exited, hence exiting http_analyzer";
			exit 1;
		fi
	done
}

# Start the server process
runserver() {
	if [ -f $PID_FILE ]; then
		rm $PID_FILE
	fi
	${INIT_SCRIPT} $1
	${RUN_SCRIPT} -c 0x4 -n 3 -m 4096 -- -p 1 &
	echo $! > $PID_FILE;
	sleep_forever;
}

usage(){
	exit 1
}


#
# MAIN LOGIC BEGINS HERE
#

if [ $# -eq 0 ]; then
	runserver 

elif [ $# -eq 1 ]; then
	runserver $1
else
	usage
fi

exit $RETVAL


# End of dpitproxy 

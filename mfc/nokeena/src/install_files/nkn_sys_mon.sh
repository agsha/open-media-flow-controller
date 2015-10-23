#!/bin/sh
#
#	File : nkn_sys_mon.sh
#
#	Description : This script monitors relevant processes and system
#		resources
#
#	Created By : Ramanand Narayanan (ramanand@ankeena.com)
#
#	Created On : 12 December, 2009
#
#	Copyright (c) Ankeena Inc., 2009
#


# Local Environment Variables
PATH=$PATH:/sbin
OOM_ADJ_VALUE=12
LOG_FILE_DIR=/var/log/nkn/sys
SYS_LOG_FILE_PREFIX=sysinfo

#
#  Function: adjust_mgmtd_oom_score()
#  Description: Get the nvsd oom_score and set the mgmtd
#  oom_score to 10+ nvsd's score
#
function adjust_mgmtd_oom_score()
{
	# Get the mgmtd process id
	mgmtd_pid=`pidof mgmtd`;
	ret_val=$?
  	if [ ${ret_val} -ne 0 ]; then
		# Nothing to do 
		return;
	fi
	# Now get the nvsd pid
	nvsd_pid=`pidof nvsd`;
	ret_val=$?
  	if [ ${ret_val} -ne 0 ]; then
		# Nothing to do 
		return;
	fi

	# Get nvsd's oom_score and 
	nvsd_oom_score=`cat /proc/$nvsd_pid/oom_score`;

	# set oom_adj for mgmtd to $OOM_ADJ_VALUE so that it goes highest
	echo $OOM_ADJ_VALUE > /proc/$mgmtd_pid/oom_adj;

	# Get mgmtd oom_score
	mgmtd_oom_score=`cat /proc/$mgmtd_pid/oom_score`;

	return ;

} # end of adjust_mgmtd_oom_score()


#
#  Function: log_sys_stats()
#  Description: Log the current memory stats
#	Rotating log with a log file directory for every hour
#	the rotation happens every day that way keeping
#	the mem info for 24hours
#
function log_sys_stats()
{
	log_file=$1;

	# Log the top output
	echo "Current top output -> " >> $log_file;
	top -b -n1 >> $log_file;

	# Now get a dump of nkncnt
	echo >> $log_file;
	echo "Current counters -> " >> $log_file;
	echo "----------------" >> $log_file;
	/opt/nkn/bin/nkncnt -t 0 -s '' | grep -v "= 0" >> $log_file;

} # end of log_sys_stats


#
#	MAIN LOGIC BEGINS HERE 
#

# First adjust the mgmtd oom_score
adjust_mgmtd_oom_score;

# Get the current hour and current minute
current_hour=`date | cut -d' ' -f4 | cut -d':' -f1`;
current_min=`date | cut -d' ' -f4 | cut -d':' -f2`;

# Get the log file name based on the hour and minute
SYS_LOG_DIR=${LOG_FILE_DIR}/hour-${current_hour}
SYS_LOG_FILE=${SYS_LOG_DIR}/${SYS_LOG_FILE_PREFIX}-minute-${current_min}.log

# Ensure the log directory exists
mkdir -p ${SYS_LOG_DIR};

# Remove the old one and start logging the new info
rm -f ${SYS_LOG_FILE}.gz;

echo "System Infomation Recorded On -> " > $SYS_LOG_FILE;
date >> $SYS_LOG_FILE;
echo "----------------------------------------------" >> $SYS_LOG_FILE;

# Get the system info logged
log_sys_stats $SYS_LOG_FILE;

echo "------------------------------------------------------" >> $SYS_LOG_FILE;
echo "End of file" >> $SYS_LOG_FILE;

# Compress the log file
gzip $SYS_LOG_FILE;

#
# End of nkn_mon.sh
#

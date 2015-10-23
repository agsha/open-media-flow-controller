#! /bin/bash
#
#	File : sftp.sh
#
#	Description : A wrapper script for sftp client to be used
#			from CLI for uploading accesslog
#
#	Created By : Ramanand Narayanan (ramanandn@juniper.net)
#
#	Created On : 04 September, 2010
#
#	Copyright (c) Juniper Networks Inc., 2010
#


# Local Environment Variables
PATH=$PATH:/sbin
LOG_PATH=/var/log/nkn/accesslog/
BATCHFILE=/tmp/sftp.batch

function usage()
{
	echo "usage: $0 <filename> <usrname@hostname:path> <profile>"
	exit 1;
}

# Parse the URL to get username, hostname and destination path
function parse_url()
{
	local URL=$1;

	# First trim the sftp:// prefix
	URL=`echo $URL | cut -c8-`;

	# Now get the username
	USERNAME=`echo $URL | cut -f1 -d'@'`

	# Next get the sftp server name 
	SERVER=`echo $URL | cut -f2 -d'@' | cut -f1 -d':'`;

	# Finally get the path in the server
	DEST_PATH=`echo $URL | cut -f2 -d'@' | cut -f2 -d':'`;
}

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Check if min number of parametes are passed
if [ $# -lt 2 ]; then
	usage $*;
fi

# Get the parameters
UPLOAD_FILE=$1;
URL=$2;
PROFILE=$3;

parse_url $URL;

# Change to the log path
cd $LOG_PATH/$PROFILE

# Now create a batch file 
echo cd $DEST_PATH > ${BATCHFILE}

# Run thru the matching files and add entries
for log_file in `ls $UPLOAD_FILE`
do
	# Prefix the FQDN to the logfile name when uploading
	remote_log_file=${HOSTNAME}-${log_file}

	# Upload the file with a .tmp extension
	echo put $log_file ${remote_log_file}.tmp >> ${BATCHFILE}

	# Remove if the file already exists on the server
	echo -rm $remote_log_file >> ${BATCHFILE}

	# Once the upload is complete rename
	echo rename ${remote_log_file}.tmp $remote_log_file >> ${BATCHFILE}
done

echo exit >> ${BATCHFILE}

# Upload the file using sftp
/usr/bin/sftp -b ${BATCHFILE} $USERNAME@$SERVER

#
# End of sftp.sh
#

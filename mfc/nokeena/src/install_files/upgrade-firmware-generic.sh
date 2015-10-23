#! /bin/bash
#
# Upgrade Pacifica SATA DIMMs with latest firmware
#
#

UPDATE_DIRECTORY=/config/firmware_upgrade_dir/
LOG_FILE=/tmp/firmware_update_prep.log
OPTION=$1
UPDATE_FILE=`basename $2`
FILE_URI=$2
usage() {
    logger -s "Usage: media-cache disk upgrade-firmware download <http url of the firmware file> [safe]"  2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
}

is_pacifica() {
    if [ -f /etc/pacifica_hw ]; then
	echo 1
    else
	echo 0
    fi
}

check_update_and_return() {
    if [ -s $UPDATE_FILE ]; then
	logger -s "Firmware update for the SSD is setup. Update will start during the next reboot"  2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
	exit 0
    else
	logger -s "Unable to fetch file $FILE_URI"  2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
    fi
    usage
    exit 1
}

if [ $( is_pacifica ) == "1" ]; then
    if [ $OPTION = "download-reboot" ]; then
	# Fetch for file using curl if the file is located in a remote path
	if [ ! -d $UPDATE_DIRECTORY ]; then
	    mkdir $UPDATE_DIRECTORY 2> $LOG_FILE
	fi
	cd $UPDATE_DIRECTORY 2>> $LOG_FILE
	curl -f -o $UPDATE_FILE $FILE_URI  2>> $LOG_FILE
	check_update_and_return
	exit 0
    elif [ $OPTION = "download" ]; then
	logger -s "Instant update of firmware not supported. Only Safe mode is supported. Use Safe mode" 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
    fi
    usage
    exit 1
else
    logger -s "Not a Pacifica system. Cannot do the upgrade"  2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
fi


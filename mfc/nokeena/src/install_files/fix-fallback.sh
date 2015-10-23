#!/bin/sh
#
#	File : fix-fallback.sh
#
#	Description : This script  fixes the name of the backed up
#			config file in the case of 1.x to 2.x upgrade
#
#	Created By : Ramanand Narayanan (ramanandn@juniper.net)
#
#	Created On : 30 August, 2010
#
#	Copyright (c) Juniper Networks Inc., 2010
#


# Local Environment Variables
CONFIG_PATH=/config/db
OLD_PROD_NAME=nokeenamfd
NEW_PROD_NAME=junipermfc


# Change to the db directory
cd $CONFIG_PATH

# Now for every config backup file that has the new prod name as a
# a substring, make a copy with the new name replaced with old name
for cfgfile in *${NEW_PROD_NAME}*
do
	oldcfgfile=`echo $cfgfile | sed -e s/$NEW_PROD_NAME/$OLD_PROD_NAME/g`

	if [ ! -f $oldcfgfile ]; then 
		logger "Making a copy of $cfgfile to $oldcfgfile"
		cp $cfgfile $oldcfgfile
	fi
done

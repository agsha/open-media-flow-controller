#! /bin/bash
#
#       File : disk_metadata.sh
#
#       Modified by : Thilak Raj S
#       Created Date : 21st June, 2010
#
#


# Remove the older datafile
rm -f /var/log/nkn/dm2_meta_data.*

# Get the new data file
/opt/nkn/bin/cache_ls.sh /nkn/mnt/dc_* > $1

# Echo the location if its not empty or pass the error message
OUTPUT=`/bin/cat ${1} | head -n 1`

echo $OUTPUT
#
# Endof disk_metadata.sh
#


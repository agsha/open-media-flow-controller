#! /bin/bash
#
#	File : collect_nkncnt_data.sh
#
#	Modified by : Varsha R 
#	Date : June 04th, 2010
#

NKNCNT_BINARY=/opt/nkn/bin/nkncnt


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------


# Now run the script that invokes nkncnt in background 
nice ${NKNCNT_BINARY} $* &

#sleep 2;

#
# End of collect_nkncnt_data.sh
#

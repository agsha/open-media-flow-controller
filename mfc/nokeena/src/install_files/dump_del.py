#!/usr/bin/python

import sys, os

#***************************************************************************#
# File Name : dump_del.py
# Author : Ramalingam Chandran
# Date : 17-07-2010
# delete all the dumps from the MFC
#***************************************************************************#

# Deleting the sysdump files in /var/opt/tms/sysdumps
err = os.system ('rm -f /var/opt/tms/sysdumps/sys*.*')

# Check whether the files are deleted by checking the return code
if err < 0 :
   print "Error in deleting the sysdump files. Check the permissions of the files."
else:
   print "Successfully deleted the sysdump files."


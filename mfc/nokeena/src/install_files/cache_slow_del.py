#!/usr/bin/python
#
#       File : cache_slow_del.py
#
#       Modified by : Ramalingam Chandran
#       Created Date : 22-07-2010
#
#

import sys
import os
import time
import syslog
import re
import mgmt_lib

MAX_PER_ITER=500
PAUSE_TIME=2
NAMESPACE = ""
# Get the parameters

if (len(sys.argv) < 4):
    print "Usage: cache_slow_del <domain> <UID> <list file>"
    sys.exit()
else:
    NS_DOMAIN       = sys.argv[1]
    NS_UID          = sys.argv[2]
    LIST_FILE        = sys.argv[3]
    NAMESPACE	     = sys.argv[4]
    PID              = sys.argv[5]
# Get the number of entries
_f_list_file = open(LIST_FILE, "r")
_num_entries = len(_f_list_file.readlines())

LOG_DIR = "/var/log/nkn/cache"

LOCK_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".slow.del.lck"


if (NS_DOMAIN == "all"):
    NS_DOMAIN = "any"


# Sanity check 
if (not os.path.isfile(LIST_FILE)):
    syslog.syslog(syslog.LOG_NOTICE, "file doesnot exists :  " + LIST_FILE)
    syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Error: List file missing for Domain : " + NS_DOMAIN)
    sys.exit()

if mgmt_lib.is_lock_valid(LOCK_FILE):
    print "Object deletion is already in progress. Please retry after few minutes"
    sys.exit()
else:
    mgmt_lib.acquire_lock(LOCK_FILE)


if not NAMESPACE == "" :
    syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Starting for Namespace: " + NAMESPACE)

syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Starting for Domain : " + NS_DOMAIN)

#Queue the delete request
#arglist = "-s "+NS_DOMAIN+" -N del_uri_file:"+LIST_FILE+" -r /tmp/ignore -q /tmp/FMGR.queue"
#fq_arglist = "/opt/nkn/bin/nknfqueue "+_arglist
COUNT = 0

_f_list_file.seek(0)
#_f_list_file.readline()
#_f_list_file.readline()
#_f_list_file.readline()
#_f_list_file.readline()

#line in _f_list_file.readlines():
del_count=0
while 1:
    line = _f_list_file.readline()
    flag_end=0
    #If list generation is still in progress,dont exit 
    if line == "":
        flag_end=1
    if re.search("End of cache objects for namespace.*",line):
        flag_end=1
    if mgmt_lib.is_pid_exists(PID) and flag_end==1:
	continue
    elif flag_end==1:
	break
    _ret = 1
    while not _ret == 0 :
    	#BUG 9565: Uri with space  are not deleted. Passing the entire object line within quotes
    	#to ensure that the  whole object is queued.
    	final_object=re.search("[ \t]*[A-Za-z_0-9]+[ \t]*[A-Z]+[ \t]*[0-9]+[ \t]*[A-Z-]+[ \t]*([A-Za-z]+[ \t]*[A-Za-z]+[ \t]*[0-9]+[ \t]*[0-9]+:[0-9]+:[0-9]+[ \t]*[0-9]+)[ \t]*(.*)", line)

    	if final_object:
            OBJ_PATTERN = final_object.group(2)
            OBJ_PATTERN = OBJ_PATTERN.replace("\\", "\\\\")
            query = "/opt/nkn/bin/nknfqueue -f -r "+NS_UID+"_\""+OBJ_PATTERN+"\" -q /tmp/FMGR.queue"
	    #syslog.syslog(syslog.LOG_NOTICE, "query is " +query)
	    _ret = os.system(query)
            if not _ret == 0:
       	        time.sleep(1)
	    else:
		del_count =del_count + 1
        else:
            _ret=0
    COUNT = COUNT + 1
	
    if COUNT > MAX_PER_ITER :
	syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Entries queued for deletion : " + str(MAX_PER_ITER))
	COUNT = 0
	time.sleep(PAUSE_TIME)

syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Number of objects deleted : " + str(del_count))
		
# Remove the delete list file
_f_list_file.close()
os.remove(LIST_FILE)
os.remove(LOCK_FILE)


syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Completed for Domain: " + NS_DOMAIN)

if not NAMESPACE == "" :
    syslog.syslog(syslog.LOG_NOTICE, "[Cache Deletion] Completed for Namespace: " + NAMESPACE)

sys.exit()

#!/usr/bin/python
#
#       File : mgmt_build_ns_object_list.py
#
#       Modified by  : Ramalingam Chandran
#       Created Date : 27-07-2010
#
#

import os
import subprocess
import time
import sys
import syslog
import commands
import re
import glob
sys.path.append("/opt/nkn/bin")
import cache_disk_lib
import logging
import logging.handlers
import mgmt_lib

# Local Environment Variables
os.environ['PATH']=os.environ['PATH']+":/sbin"
LOG_DIR = "/nkn/ns_objects/"
TM_NAMESPACE = "/nkn/nvsd/namespace"
MDREQ = "/opt/tms/bin/mdreq"

TM_RAMCACHE_ACTION = "/nkn/nvsd/namespace/actions/get_ramcache_list"
TM_UPLOAD_ACTION = "/file/transfer/upload"

#initiate the syslog
syslog.openlog(sys.argv[0])

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Check if min number of parametes are passed
if (len(sys.argv) < 2):
    print "Usage: mgmt_build_ns_object_list.py  <namespace> <ns-uid>"
    sys.exit()

# Now get the parameters
else:
    NAMESPACE       = sys.argv[1]
# namespace uid is passed by caller. there could be case that we by the time
# we query for UID using mdreq, namespace might have been deleted 
    NS_UID 	    = sys.argv[2]

#TODO, get the pattern
    PATTERN	    = sys.argv[3]
    FILE 	    = sys.argv[4]
    if (len(sys.argv) > 5):
        EXPORT_URL   = sys.argv[5]
    if (len(sys.argv) > 6):
	EXPORT_PASSWORD	    = sys.argv[6]

# Create the name of the LOG file and the Lock file
OBJECT_LIST_FILE=LOG_DIR+"/ns_objects_"+ NAMESPACE + "." + FILE + ".lst"
RAM_OBJECT_LIST_FILE=LOG_DIR+"/ns_ram_objects_"+NAMESPACE+".lst"

LOCK_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".lst.lck"
#EXPORT_URL=LOG_DIR+"/remote_url_"+NAMESPACE
#EXPORT_PASSWORD=LOG_DIR+"/remote_password_"+NAMESPACE

logger = logging.getLogger('mgmt_cache_list')
hdlr = logging.handlers.RotatingFileHandler(LOG_DIR + '/mgmt-list-log-' + NAMESPACE + '.txt', maxBytes = 40960, backupCount=3)
formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
hdlr.setFormatter(formatter)
logger.addHandler(hdlr)
logger.setLevel(logging.INFO)
# Check if namespace is active
#TODO,Send the pattern as a binding
query = MDREQ+" -v query get - "+TM_NAMESPACE+"/"+NAMESPACE+"/status/active"
active = commands.getoutput(query)
if (not active):
    print NAMESPACE+" : namespace inactive"

# Get the active disk list
A_DISKS = cache_disk_lib.get_active_disks()


logger.info('------------------------------------------------------------')
logger.info('Starting background listing...')
logger.info("Starting mgmt build list with (%s, %s, %s)", NAMESPACE, NS_UID, PATTERN)
#NICE_VAL_0=os.nice(-10)
#logger.info("nice val is %d", NICE_VAL_0)

# If lock file exists then it means the task is already in progress
# Hence, nothing to do
if mgmt_lib.is_lock_valid(LOCK_FILE):
    print "Object list generation is already in progress. Please retry after few minutes"
    logger.info('Bailing on retry since task is running.')
    sys.exit()
else:
    # Check if the list file exists
    if os.path.exists(OBJECT_LIST_FILE):
	# If the number of objects in the old list is above 10K 
	# we would like to create a 2 min pause between subsequent
	# requests
	old_list_count=len(OBJECT_LIST_FILE)
        if old_list_count > 10000 :
	    # Now find the time since that last update 
	    # to the object list file
            file_change_time=os.path.getmtime(OBJECT_LIST_FILE)
	    curr_time=time.time()
	    time_diff = curr_time - file_change_time
            # Check if time difference is less than 120 sec 
	    # then sleep for the remaining seconds
	    logger.info('Last time till update : %d s', time_diff)
	    if time_diff < 120 :
		# Create the lock file
		mgmt_lib.acquire_lock(LOCK_FILE)
		sleep_time = 120 - time_diff
		syslog.syslog(syslog.LOG_NOTICE, "Object List Creation for Namespace "+NAMESPACE+" : Pausing "+sleep_time+" seconds")
		time.sleep(sleep_time)
		logger.info('Sleeping for %d...', sleep_time)
	
    # Create the lock file
    mgmt_lib.acquire_lock(LOCK_FILE)

    # Remove the old list file
    if os.path.exists(OBJECT_LIST_FILE):
        os.remove(OBJECT_LIST_FILE)
    if os.path.exists(RAM_OBJECT_LIST_FILE):
	os.remove(RAM_OBJECT_LIST_FILE)

numobjects = '0'
# Now get the ramcache object list first
query = MDREQ+" -q action "+TM_RAMCACHE_ACTION+" namespace string "+NAMESPACE+" uid string "+NS_UID+" filename string "+RAM_OBJECT_LIST_FILE
try:
    p = subprocess.Popen(query, shell=True, stdout=subprocess.PIPE)
except OSError, e:
    print >>sys.stderr, "Execution Failed:", e
else:
    bindings = p.communicate()
    for binding in bindings[0].split('\n'):
	if "=" in binding:
	    numobjects = binding.split(' = ')[1]

logger.info('Issue ram-cache read and got %s objects from engine.', numobjects)

syslog.syslog(syslog.LOG_NOTICE, "Object List Creation for Namespace "+NAMESPACE+" : STARTED")


fobjlist = open(OBJECT_LIST_FILE, 'w')
f_ram_obj_lst = open(RAM_OBJECT_LIST_FILE, "r")

f_ram_obj_lst.seek(0)

#fobjlist.write("*  Loc  Pinned  Size (KB)          Expiry          		URL\n")
#fobjlist.write("*--------------------------------------------------------------------------------------\n")

#uniqueobjects=set(f_ram_obj_lst.readlines())
PY_PATTERN=":[0-9]+" + PATTERN

#Shoule we always write line by line?
count=0
for line in f_ram_obj_lst.readlines():
    if count < 6:
  	logger.info('writing line,%s',line)
	fobjlist.write(line)
	count = count + 1
    elif re.search(PY_PATTERN, line):
        fobjlist.write(line)

fobjlist.close()
f_ram_obj_lst.close()
GREP_PATTERN=":[0-9]\{1,\}" + PATTERN

# Now if the file got created then append to it 
if  os.path.exists(OBJECT_LIST_FILE):
    fobjlist = open(OBJECT_LIST_FILE, 'a')

    # Now get the directory list 
    # Find the directories that match the prefixed UID
    #fobjlist.write("*  Loc  Pinned  Size (KB)          Expiry          		URL\n")
    #fobjlist.write("*--------------------------------------------------------------------------------------\n")
    #fobjlist.close()

    for disk in A_DISKS:

        DIR_LIST = []

        DIR_LIST = glob.glob("/nkn/mnt/"+disk+"/"+NS_UID+"_*")

        for i in DIR_LIST:
	    if not os.path.exists(i):
	        continue

	    logger.info('Reading container from %s', i)
	    #print i
	    pin_flag = cache_disk_lib.is_lowest_tier(disk)
	    #Grep for the pattern here
            if(pin_flag == -1):
                syslog.syslog(syslog.LOG_CRIT, "Object List Creation failed for Namespace "+NAMESPACE+": FAILED")
                os.remove(LOCK_FILE)
                sys.exit()
	    elif(pin_flag):
		query = "/opt/nkn/bin/container_read -m -R -l " + i + " |grep \"" + GREP_PATTERN + "\" >> " + OBJECT_LIST_FILE
	    else:
		query = "/opt/nkn/bin/container_read -m -R " + i + " |grep \"" + GREP_PATTERN + "\" >> " + OBJECT_LIST_FILE 
            ret = os.system(query)
    	    logger.info('container query is %s and ret value is %d', query, ret)
 
    fobjlist = open(OBJECT_LIST_FILE, 'a')    
    fobjlist.write("\n---------------------------------------------------------------------------------------\n")
    fobjlist.write("End of cache objects for namespace "+ NAMESPACE+"\n")
    fobjlist.write("---------------------------------------------------------------------------------------\n")
    fobjlist.close()
    filehandler = open(OBJECT_LIST_FILE, "r+")
    filehandler.seek(0)
    Numofobj = 0
    for line in filehandler.readlines():
	if re.match ("(.*)RAM(.*)", line) or re.match ("(.*)dc_(.*)", line):
		Numofobj = Numofobj + 1
    filehandler.write("-------------------------------------------------------------\n")
    filehandler.write("Total num of objects:"+str(Numofobj)+"\n")
    filehandler.write("-------------------------------------------------------------\n")
    filehandler.close()
    logger.info('Total objects: %d', Numofobj)

syslog.syslog(syslog.LOG_NOTICE, "Object List Creation for Namespace "+NAMESPACE+": COMPLETED")

if (len(sys.argv) > 5):
	if (len(sys.argv) > 6):
		EXPORT_URL  = re.escape(EXPORT_URL)
		EXPORT_PASSWORD  = re.escape(EXPORT_PASSWORD)
		upload_query = "/opt/tms/bin/mdreq -q action "+TM_UPLOAD_ACTION+" remote_url string "+EXPORT_URL+" password string "+EXPORT_PASSWORD+" local_dir string "+"/nkn/ns_objects/"+" local_filename string "+"ns_objects_"+NAMESPACE+"."+FILE+".lst"
		subprocess.Popen(upload_query, shell=True)
	else:
		upload_query = "/opt/tms/bin/mdreq -q action "+TM_UPLOAD_ACTION+" remote_url string "+EXPORT_URL+" local_dir string "+"/nkn/ns_objects/"+" local_filename string "+"ns_objects_"+NAMESPACE+"."+FILE+".lst"
		subprocess.Popen(upload_query, shell=True)

# Now that we are done remove the lock file 
os.remove(LOCK_FILE)

logger.info('Done...')
logger.info('------------------------------------------------------------')
sys.exit()

#
# mgmt_build_ns_object_list.py
#

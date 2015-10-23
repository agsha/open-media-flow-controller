#!/usr/bin/python
#
#       File : cache_mgmt_del.py
#
#       Modified by : Ramalingam Chandran
#       Created Date : 19-07-2010
#
#	Last Modified : Jan 05 2011

import sys
import os
import os.path 
import commands
import time
import re
import glob
import mgmt_lib
import subprocess
import signal
import syslog

MAX_RETRY=6


TM_RAMVERIFY_ACTION = "/nkn/nvsd/namespace/actions/is_obj_cached"
MDREQ = "/opt/tms/bin/mdreq"


LOG_DIR="/nkn/ns_objects/"
BUILD_OBJECT_LIST_SCRIPT="/opt/nkn/bin/mgmt_build_ns_object_list.py"
PINNED = ""
pid=0
SEARCH_PORT="80"

#Signal Handler
def handleSIGCHLD(signal, frame):
    os.waitpid(-1, os.WNOHANG)

def check_if_pre_pread_done():
    sata_preread = commands.getstatusoutput("/opt/nkn/bin/nkncnt -s SATA.dm2_preread_done -t 0 | awk 'NR==2{print $3;}'")

    sas_preread = commands.getstatusoutput("/opt/nkn/bin/nkncnt -s SAS.dm2_preread_done -t 0 | awk 'NR==2{print $3;}'")

    ssd_preread = commands.getstatusoutput("/opt/nkn/bin/nkncnt -s SSD.dm2_preread_done -t 0 | awk 'NR==2{print $3;}'")

    if(sata_preread[0] == 0 and sata_preread[1] == "0") :
	return False;

    if(sas_preread[0] == 0 and sas_preread[1] == "0") :
	return False;

    if(ssd_preread[0] == 0 and ssd_preread[1] == "0") :
	return False;

    return True;


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
#--------------------------------------------------------------


# Sanity test
#create the log dir if it doesnot exists
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

# Get the parameters 
if (len(sys.argv) < 5):
    print "Usage: cache_mgmt_del <namespace> <uid> <domain> <all|<pattern>>"+\
          " <all|<search domain>>"
    sys.exit()
else:
    NAMESPACE       = sys.argv[1]
    NS_UID          = sys.argv[2]
    NS_DOMAIN       = sys.argv[3]
    OBJ_PATTERN     = sys.argv[4]
    if (len(sys.argv) > 5):
        SEARCH_DOMAIN   = sys.argv[5]
    if (len(sys.argv) > 6):
	SEARCH_PORT	    = sys.argv[6]

#debug
#print NAMESPACE
#print NS_UID
#print NS_DOMAIN
#print OBJ_PATTERN
#print SEARCH_DOMAIN
#print SEARCH_PORT


# Object list file name
#Define file type as pattern grep has been made line
#By giving unique name to files,we can get rid of these locks all together
#Having the locks for now,as we need more time to test
try:

    FILE_TYPE="del"
    OBJECT_LIST_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+"."+FILE_TYPE+".lst"
    OBJECT_PIN_LIST_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".lst.pin"
    DELETE_OBJECT_LIST_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+"_delete.lst"
    DELETE_OBJECT_LIST_FILE_TMP=LOG_DIR+"/ns_objects_"+NAMESPACE+"_delete.lst.tmp"

    #Deletions on same namespace from multiple cli not allowed
    #so create a lock file
    LOCK_FILE = LOG_DIR+"/ns_objects_"+NAMESPACE+".del.lck"
    #Look at the obj_build_file_lock
    OBJ_BUILD_LOCK_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".lst.lck"
    SLOW_DEL_LOCK_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".slow.del.lck"
    #Error file is more like nohup.out
    ERROR_FILE = LOG_DIR +"/ns_" + NAMESPACE +"_del.err"

    #Re-direct all exception to this file
    if os.path.exists(ERROR_FILE):
	os.remove(ERROR_FILE)

    err_file = open(ERROR_FILE, "w")


    #This will remove only for an domain/port
    if (os.path.isfile(DELETE_OBJECT_LIST_FILE)):
	os.remove(DELETE_OBJECT_LIST_FILE)
    # Single URI Delete
    # Go through the disks based on UID passed and get the originserver:port
    # Append uri with originserver:port and call nknfqueue

    if (SEARCH_DOMAIN != "all"):
	if (SEARCH_PORT != "*"):
	    MOUNT_POINT="/nkn/mnt/dc_*/" + NS_UID + "_" + SEARCH_DOMAIN + ":" + SEARCH_PORT
	else:
	    MOUNT_POINT="/nkn/mnt/dc_*/" + NS_UID + "_" + SEARCH_DOMAIN + ":*"
    else:
	MOUNT_POINT="/nkn/mnt/dc_*/" +  NS_UID + "_*"


    #Get the object path alone
    OBJ_DIR_PATH = os.path.dirname(OBJ_PATTERN)
    #Canonicalize the path
    OBJ_DIR = os.path.abspath(OBJ_DIR_PATH)
    FOUND = "false"
    wild_card=0
    if OBJ_PATTERN != "all" :
	wild_card = re.search("\*|\?",OBJ_PATTERN,re.M)
	if wild_card :
	    URI_PATTERN = OBJ_PATTERN
	else:
	    URI_PATTERN = ""

	if URI_PATTERN == "" :
	    #Single object delete
	    #No files used,so no lock
	    DIR_LIST = []

	    DIR_LIST = glob.glob(MOUNT_POINT)
	    #check if pre-read is in progress
	    if not check_if_pre_pread_done():
		print "Pre-read is in progress,Please try after sometime."
		sys.exit()
	    OBJ_PATTERN = OBJ_PATTERN.replace("\\", "\\\\")
	    for i in DIR_LIST:

		obj_id  = i.rsplit("/", 1)
		if not os.path.exists(i+OBJ_DIR):
		    continue
		CONTAINER = "/" + obj_id[1] + OBJ_PATTERN
		query = "/opt/nkn/bin/nknfqueue -f -r " + CONTAINER + " -q /tmp/FMGR.queue "
		ret = os.system(query)
		FOUND = "true"
	    
	    #Check if object is in RAM cache by triggering an action
	    #Get the response giving the actual cache name
	    if not FOUND == "true":
		query = MDREQ+" action "+TM_RAMVERIFY_ACTION+" namespace string \
		"+NAMESPACE+" uid string "+NS_UID+" uri string "+ OBJ_PATTERN
		CACHE_URI = commands.getstatusoutput(query)
		if not CACHE_URI[1] == "":
		    ACT_RET = CACHE_URI[1].split('\n')
		    RET_CODE_LN = ACT_RET[0].split(':')
		    RET_CODE = RET_CODE_LN[1].strip()
		    RET_MSG_LN = ACT_RET[1].split(':',1)
		    RET_MSG = RET_MSG_LN[1].strip()
		    ret_list = re.search('/(\S+?):(\S+?):(\d+)')
		    if(SEARCH_DOMAIN != "all" and ret_list[1] == SEARCH_DOMAIN ):
		 	domain_match = 1
		    if(SEARCH_PORT != "*" and ret_list[2] == SEARCH_PORT):	
			port_match = 1
		    if RET_CODE == "0" and domain_match and port_match:
			query = "/opt/nkn/bin/nknfqueue -f -r " + RET_MSG + " -q /tmp/FMGR.queue "
			ret = os.system(query)
			FOUND = "true"

	    if FOUND == "true" :
		    print "Object queued for deletion"
		    print "Please run the object list command for the URI"
	    else:
		    print "Object not found"
		    
	    sys.exit()

    # Convert the PATTERN that is grep friendly
    # first made . as literal and then made the normal grep friendly pattern 
    #[!!fix for bug 1737]


    #Setup the signal handler here,
    # at paths which take object listing
    signal.signal(signal.SIGCHLD, handleSIGCHLD)

    if mgmt_lib.is_lock_valid(SLOW_DEL_LOCK_FILE):
	print NAMESPACE+" : Object deletion is already in progress.Please try after sometime."
	sys.exit()


    if mgmt_lib.is_lock_valid(OBJ_BUILD_LOCK_FILE):
	print NAMESPACE+" : Object list generation is already in progress.Please try after sometime."
	sys.exit()


    # Check if a delete is already in progress
    #Moved this check here as files are created only for a pattern or delete all
    if mgmt_lib.is_lock_valid(LOCK_FILE):
	print NAMESPACE+" : Deletion of objects for this namespace already"+ \
	" in progress"
	print "Issue this command only after the earlier one has completed"
	print "Check log for status of deletion"
	sys.exit()
    else:
	# Psuedo lock so that multiple deletes are not kicked in
	# Simply touch the object file
	mgmt_lib.acquire_lock(LOCK_FILE)

    #form the obj pattern here,
    if wild_card:
	OBJ_PATTERN=mgmt_lib.get_python_pattern(OBJ_PATTERN)
    else:
	OBJ_PATTERN=".*"

    # Now run the script that builds the whole object list in the background
    #TODO,Pass the pattern to the list
    #Get the PID of the lidt process

    #PR 797343
    #If a previous delete was aborted,
    #old list file might still exist
    #and the new del might use the old
    #partial file leading to loop 
    #in slow del
    if os.path.exists(OBJECT_LIST_FILE):
	os.remove(OBJECT_LIST_FILE)
    pid=subprocess.Popen(["nohup","python",  BUILD_OBJECT_LIST_SCRIPT, NAMESPACE, NS_UID, OBJ_PATTERN, FILE_TYPE],stdout=err_file, stderr=err_file).pid
    #pid = os.spawnl(os.P_NOWAIT, '/usr/bin/python', 'python', BUILD_OBJECT_LIST_SCRIPT, NAMESPACE, NS_UID, OBJ_PATTERN,FILE_TYPE)

    # Just give a short pause and check for the list file
    #TODO,Wait for the lock file to come,Should re think
    #time.sleep(8)

    # Now check if the list file exists if not just print "no objects"
    #if (mgmt_lib.is_process_exists(pid)):
    #    print NAMESPACE+": No objects to delete for this namespace"
    #    sys.exit()
    #fname=DELETE_OBJECT_LIST_FILE
    #if os.path.exists(fname):
    #    os.utime(fname, None)
    #else:
    #    open(fname, 'w').close()

    #skip the unwanted lines and get only the URI part from the object list
    #Wait till the object list file is in place
    retry_count=0
    while not os.path.exists(OBJECT_LIST_FILE):
	if retry_count < MAX_RETRY:
	    time.sleep(2)
	    syslog.syslog(syslog.LOG_NOTICE,"[OBJ Deletion] : Waiting for object list file : " + NAMESPACE)
	else:
	    print "Failed to get the object list file,Please try again."
	    if mgmt_lib.is_pid_exists(pid):
		os.kill(pid, 9)
	    sys.exit(0)
	retry_count = retry_count + 1

    f_object_list = open(OBJECT_LIST_FILE, "r")
    #No need for the temp file,as list file has wat we need and
    #slow del will cut the correct pattern to be enqueued
    #f_delete_list_tmp = open (DELETE_OBJECT_LIST_FILE_TMP, "w")

    #Wait till either of the below is true,
    #1)We have > 50 entries to be displayed
    #2)Object listing process is complete
    while 1:
	line_cnt = len(f_object_list.readlines())
	if(line_cnt > 50):
	    syslog.syslog(syslog.LOG_INFO,"[OBJ Deletion]Reached display count")
	    break
	if not mgmt_lib.is_pid_exists(pid):
	    syslog.syslog(syslog.LOG_INFO,"[OBJ Deletion]Listing process exited")
	    break
	f_object_list.seek(0)
	time.sleep(3)


    if PINNED == "pin" :
	f_delete_list = open (DELETE_OBJECT_LIST_FILE, "w")
	for line in f_object_list.readlines():
	    if re.search("  dc_[0-9]* *Y", line):
		final_object = line.rsplit(" ", 1)
		f_delete_list.write(final_object[1])

	f_delete_list.write(line)
	f_delete_list.close()
	os.remove(OBJECT_LIST_FILE)
    #The else part is not needed,see previous comment
    #else:
	#BUG 9565: Uri with space are not deleted when object delete all is given.
	#The previous code line.rsplit(" ", 1) was taking the space in the uri name into account.
	#the scan for space was from right to left and the space in uri was encountered first.
	#So the delete list was having only the last portion of the uri.
	#Fix: Scannin from the left ,the number of spaces to be ignored before we arrive at
	#the uri portion  for RAM is 26, for dc_* it is 25. This ensures that the complete
	#uri is  added to the delete list.
	#Eg: Suppose the object list had the following object 
	#RAM    N          2048  Sun Oct  2 15:52:23 2011  10.157.42.69:80/space/2mb/file name with spaces5
	#initially only space5 was added to the delete list
	#with the following fix 
	#10.157.42.69:80/space/2mb/file name with spaces5 is added to the delete list.
	# ??what happens if we have dc_10(will this space calculation hold good)
	#Bug was reopened since the space calcualtion method is wrong. Using the regex pattern to split 
	#each line into 2 parts
	#print b.group(0)
	#RAM    N             0  Fri Feb 18 02:03:08 2011 20.2.1.101:80/Radhakrish/buffer_manager/object/abnokeena1297965720.610
	#print b.group(1)
	#Fri Feb 18 02:03:08 2011
	# print b.group(2)
	#20.2.1.101:80/Radhakrish/buffer_manager/object/abnokeena1297965720.610
	#We will use the group(2) value as this contains the entire uri.
	# regex patterns is <space><RAM/dc ><space><size><day><date><space><uri>
	# <uri>  forms part 2.
	#<day><date> forms part 1.
	#BUG FIX:9613 - The ram cache size pattern check was considering only single digit. Added [0-9]+
	#TODO,what is the need of the temp file?
	#for line in f_object_list.readlines():
	    #final_object = re.search("[ \t]*[A-Za-z_0-9]+[ \t]*[A-Z]+[ \t]*[0-9]+[ \t]*([A-Za-z]+[ \t]*[A-Za-z]+[ \t]*[0-9]+[ \t]*[0-9]+:[0-9]+:[0-9]+[ \t]*[0-9]+)[ \t]*(.*)", line)
	    #if (final_object != None):
		#f_delete_list_tmp.write(final_object.group(2) + "\n")

    #f_delete_list_tmp.close()

    #Remove the grep pattern search here, as the pattern find is to be done in mgmt_list

    # First check if request is for all objects and all domains as this is easy
    #If its a delete for non-pinned object,the original list file is the delete file
    if not PINNED == "pin":
	DELETE_OBJECT_LIST_FILE=OBJECT_LIST_FILE
	#f_delete_list_tmp = open(DELETE_OBJECT_LIST_FILE_TMP, "r")
	#if OBJ_PATTERN == "all$" :
	    #if (SEARCH_DOMAIN == "all"):
		# Grep for only the object entries and create the list
		#uniqueobjects=set(f_delete_list_tmp.readlines())		
		#for line in uniqueobjects:
		    #f_delete_list.write(line)
	    #else:
		#uniqueobjects=set(f_delete_list_tmp.readlines())
		#for line in uniqueobjects:
		    #if  re.search (SEARCH_DOMAIN, line):
			    #f_delete_list.write(line)
	#else:
		#uniqueobjects=set(f_delete_list_tmp.readlines())
		#for line in uniqueobjects:
		    #if  re.search (OBJ_PATTERN, line):
			    #f_delete_list.write(line)

    #f_delete_list_tmp.close()
    # Remove the temporary file
    #os.remove(DELETE_OBJECT_LIST_FILE_TMP)

    # Now for each entry in the file call the cache_slow_del.py
    #For safety make sure the file pointer is in the top of the file
    #This NUM_ENTRIES is misleading
    NUM_ENTRIES=mgmt_lib.count_objects(DELETE_OBJECT_LIST_FILE)

    if (NUM_ENTRIES == 0):
	print NAMESPACE+" : No objects found, hence nothing to delete"
	#os.remove(DELETE_OBJECT_LIST_FILE)
    else:
	print NAMESPACE+" : "+str(NUM_ENTRIES)+"(approx) object(s) being deleted "
	print " "
	print "Check log for status of deletion"
	print "Note : Same object in multiple tiers are treated as one object"+ \
	      " for deletion"
	print "Warning : Deletion happens by blocks hence neighbouring objects"+ \
	      " could also get deleted"

	#call the slow del script to delete the objectes
	#Note:PID is not used,just sending it now
	subprocess.Popen(["nohup", "python", "/opt/nkn/bin/cache_slow_del.py", SEARCH_DOMAIN, NS_UID, \
		    DELETE_OBJECT_LIST_FILE, NAMESPACE, str(pid)],stderr=err_file)
	#sh_cmd="nohup /opt/nkn/bin/cache_slow_del.py "+SEARCH_DOMAIN+" "+ NS_UID+" "+ \
	      #DELETE_OBJECT_LIST_FILE+ " "+ NAMESPACE + " "+str(pid) +" 2> /dev/null &"
	#os.system(sh_cmd)

    os.remove(LOCK_FILE)
    sys.exit()
except KeyboardInterrupt:
    print "Interrupted, Exiting ..."
    mgmt_lib.kill_mgmt_obj_ls(pid)
except re.error:
    print "Unsupported Expression"
    mgmt_lib.kill_mgmt_obj_ls(pid)
except SystemExit:
    pass
except Exception, e:
    mgmt_lib.kill_mgmt_obj_ls(pid)
    mgmt_lib.handle_error(LOG_DIR + "mgmt_obj_del")
#
# End of cache_mgmt_del.py
#

#!/usr/bin/python
#
#	File : cache_mgmt_ls.py
#
#	Modified by : Ramalingam Chandran 
#	Date : April 31st, 2010
#

import sys
import os
import time
import re
import glob
import fnmatch
import shutil
import atexit
import subprocess
import signal

sys.path.append("/opt/nkn/bin")
import cache_disk_lib

import mgmt_lib

MAX_DISPLAY = 50
MAX_RETRYCOUNT = 10
HEADER = 6
LOG_DIR = "/nkn/ns_objects/"
BUILD_OBJECT_LIST_SCRIPT = "/opt/nkn/bin/mgmt_build_ns_object_list.py"
pid=0

#Signal Handler

def handleSIGCHLD(signal, frame):
    os.waitpid(-1, os.WNOHANG)

#print header
def print_list_header(filename):

    filehandler = open(filename, "r")
    filehandler.seek(0)

    Numofobj = 0
    for line in filehandler.readlines():
	if re.match ("(.*)RAM(.*)", line) or re.match ("(.*)dc_(.*)", line):
		Numofobj = Numofobj + 1

    logger.info('File %s has %d objects.. either this is true or the file didnt get flushed to disk.',\
	    filename, Numofobj)

    if Numofobj > (MAX_DISPLAY):
	print "Possible number of objects: "+ str(Numofobj) +" (approx) "
	print "Note:"
        print "  Displaying the first "+str(MAX_DISPLAY)+" lines"
        print ""
        print "  Background task is in progress for collecting the full object list"
        print "  Please check system log for the task completion"
	print ""
    else:
        print "Possible number of objects: "+ str(Numofobj) + " (approx)";

    if Numofobj == 0:
	return;

    if os.path.exists(EXPORT_URL):
       	export_url = open(EXPORT_URL, 'r+')
       	REMOTE_URL = export_url.readline()
       	print "  Generated object list file will be available at:"
       	c= re.search(':.*?:(.*)@',REMOTE_URL)
       	if c:
       	       	password =c.group(1)
       	       	replace_str = '*' * len(password)
       	       	password  = re.escape(password)
       	       	REMOTE_URL_NOPASS = re.sub(password,replace_str,REMOTE_URL)
       	       	print REMOTE_URL_NOPASS
       	else:
       	       	print REMOTE_URL
        
    else:
        print "  Generated object list file can be uploaded using /'export/' option of this command"
        print ""

    filehandler.seek(0)	
    count = 1
    for line in filehandler.readlines():
	if count > (MAX_DISPLAY + HEADER):
                break
	if count > HEADER:
		if re.match ("(.*)RAM(.*)", line) or re.match ("(.*)dc_(.*)", line):
			print line,
			count = count + 1
	else:
		print line,
		count = count + 1
	
#    filehandler.write("-------------------------------------------------------------\n")
#    filehandler.write("Total num of objects:"+str(Numofobj)+"\n")
#    filehandler.write("-------------------------------------------------------------\n")

#count object lines
def count_object_lines(filename):
    filehandler = open(filename, "r+")
    filehandler.seek(0)

    Numofobj = 0
    for line in filehandler.readlines():
	if re.match ("(.*)RAM(.*)", line) or re.match ("(.*)dc_(.*)", line):
		Numofobj = Numofobj + 1
    
    #filehandler.seek(0,2)	
    #filehandler.write("-------------------------------------------------------------\n")
    #filehandler.write("Total num of objects:"+str(Numofobj)+"\n")
    #filehandler.write("-------------------------------------------------------------\n")
    
#clean up function
def cleanup():
    if os.path.exists(SINGLE_LOCK_FILE):
        os.remove(SINGLE_LOCK_FILE)
    if os.path.exists(PATTERN_LOCK_FILE):
	os.remove(PATTERN_LOCK_FILE)

#atexit.register(cleanup)    
#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Sanity check on the parameters
# Check if min number of parametes are passed

try:
    if (len(sys.argv) < 4):
	print "Usage: cache_mgmt_ls <namespace> <all|<pattern>> <all|domain>"
	sys.exit()
    else:
	NAMESPACE       = sys.argv[1]
	OBJ_PATTERN     = sys.argv[2]
	SEARCH_DOMAIN   = sys.argv[3]
	NS_UID          = sys.argv[4]
	SEARCH_PORT	    = sys.argv[5]

    import logging
    import logging.handlers


    logger = logging.getLogger('cache_list')
    hdlr = logging.handlers.RotatingFileHandler(LOG_DIR + '/list-log-' + NAMESPACE +'.txt', maxBytes=40960, backupCount=3)
    formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
    hdlr.setFormatter(formatter)
    logger.addHandler(hdlr)
    logger.setLevel(logging.INFO)


    logger.info('------------------------------------------------------------')
    logger.info("Starting list with (%s, %s, %s, %s, %s)", NAMESPACE, OBJ_PATTERN, SEARCH_DOMAIN, NS_UID, SEARCH_PORT)


    PATTERN_LOCK_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".grep.lck"
    #Search for single URI
    SINGLE_OBJECT_LIST_FILE = LOG_DIR+"/ns_single_objects_"+NAMESPACE+".lst"
    GREPED_SINGLE_OBJECT_LIST_FILE = LOG_DIR+"/ns_single_objects_"+NAMESPACE+"_grep.lst"
    GREPED_SINGLE_OBJECT_LIST_FILE_TMP = LOG_DIR+"/ns_single_objects_"+NAMESPACE+"_grep.lst.tmp" 
    TM_RAMCACHE_ACTION = "/nkn/nvsd/namespace/actions/get_ramcache_list"
    SINGLE_LOCK_FILE=LOG_DIR+"/ns_single_objects_"+NAMESPACE+".lst.lck"
    ERROR_FILE = LOG_DIR +"/ns_" + NAMESPACE +"_sh.err"
    EXPORT_URL=LOG_DIR+"/remote_url_"+NAMESPACE
    EXPORT_PASSWORD=LOG_DIR+"/remote_password_"+NAMESPACE

    #time.sleep(30)

    #Re-direct all exception to this file
    if os.path.exists(ERROR_FILE):
	os.remove(ERROR_FILE)

    err_file = open(ERROR_FILE, "w")

    if [ SEARCH_DOMAIN != "all" ] :
	MOUNT_POINT = SEARCH_DOMAIN + ":" + SEARCH_PORT
    else :
	MOUNT_POINT = "*"


    #Start of single object listing
    if OBJ_PATTERN != "all" :
	logger.info('Starting a pattern search....')
	wild_card = re.search("\*|\?",OBJ_PATTERN,re.M)
	if wild_card:
	    URI_PATTERN = OBJ_PATTERN
	else:
	    #Add a dollar symbol at the  end of the pattern for exact match
	    URI_PATTERN = ""
	    OBJ_PATTERN = OBJ_PATTERN + "$"

	logger.info('URI_PATTERN,OBJ_PATTERN: %s,%s', URI_PATTERN,OBJ_PATTERN)

	if URI_PATTERN != OBJ_PATTERN :
	    OBJ_PATTERN = OBJ_PATTERN.replace("(", "\(")
	    OBJ_PATTERN = OBJ_PATTERN.replace(")", "\)")
	    OBJ_PATTERN = OBJ_PATTERN.replace("[", "\[")
	    OBJ_PATTERN = OBJ_PATTERN.replace("]", "\]")
	    OBJ_PATTERN = OBJ_PATTERN.replace("^", "\^")
	    OBJ_PATTERN = OBJ_PATTERN.replace("?", "\?")
	    OBJ_PATTERN = OBJ_PATTERN.replace(".", "\.")

	    # Get the active disk list
	    A_DISKS = cache_disk_lib.get_active_disks()
			    
	    #Get the object path alone
	    OBJ_DIR_PATH = os.path.dirname(OBJ_PATTERN)
	    #Canonicalize the path
	    OBJ_DIR = os.path.abspath(OBJ_DIR_PATH) 
    #lock start
    #Lock for single object display. This set of commands call the ramcache action directly, display
    #the output and delete the temp files.Don't allow two commands to call this set of commands immediately.
    #If lock is present, display the error message for the command trying to acquire the lock.
	    if mgmt_lib.is_lock_valid(SINGLE_LOCK_FILE):
		print "The task is already in progress. Please retry after few minutes"
		sys.exit()
	    else:
		mgmt_lib.acquire_lock(SINGLE_LOCK_FILE)
		#BUG FIX:10727.
		#If file  for single object exists remove this and then open a new one.
		#This is to handle the case where in the previous listing  had script error due to some invalid exp
		#The cleanup is handled here to ensure only the object  is listed only once.
		#Issue: The single object file was not getting cleanedup and the new query was appending the new list to old list
		#resulting in multiple entry display.
		if os.path.exists(GREPED_SINGLE_OBJECT_LIST_FILE):
		    os.remove(GREPED_SINGLE_OBJECT_LIST_FILE)
		
		if os.path.exists(SINGLE_OBJECT_LIST_FILE):
		    os.remove(SINGLE_OBJECT_LIST_FILE)

		logger.info('Querying ramcache for object list')
		#Get ramcache list and search
		query = "/opt/tms/bin/mdreq -s -q action "+TM_RAMCACHE_ACTION+" namespace string "+NAMESPACE+" uid string "+NS_UID+" filename string "+SINGLE_OBJECT_LIST_FILE
		os.system(query)
		time.sleep(2)

		single_obj_file = open(SINGLE_OBJECT_LIST_FILE, "r+")
		g_single_obj_file = open (GREPED_SINGLE_OBJECT_LIST_FILE, "w+")
		#If there is no RAM data then the field names are not printed
		#This in turn affects the header count we have assumed as 6
		#To display the field names and to match the correct header lines
		#check the length of the single obj file after reading RAM data.
		#If it is empty then add the field names to the file.
		leng = len(single_obj_file.readlines())
		logger.info('Ramcache list returned %d lines.', leng)
		if (leng == 4):
		    #single_obj_file.seek(0,2)
		    single_obj_file.write("*  Loc  Pinned  Size (KB)          Expiry          		URL\n")
		    single_obj_file.write("*--------------------------------------------------------------------------------------\n")
		    single_obj_file.flush()
		

		for disk in A_DISKS:

		    DIR_LIST = []

		    DIR_LIST = glob.glob("/nkn/mnt/"+disk+"/"+NS_UID+"_*")

		    for i in DIR_LIST:
			    if not os.path.exists(i + OBJ_DIR):
				    continue
			    logger.info('listing objects in directory: %s', i)
			    pin_flag = cache_disk_lib.is_lowest_tier(disk)
			    if(pin_flag):
				query = "/opt/nkn/bin/container_read -m -R -l " + i + OBJ_DIR + " >> " + SINGLE_OBJECT_LIST_FILE
			    else:
				query = "/opt/nkn/bin/container_read -m -R " + i + OBJ_DIR + " >> " + SINGLE_OBJECT_LIST_FILE
			    ret = os.system(query)

		#Now grep our wanted items
	    
		single_obj_file.seek(0)	
		#Doing it here as we dont want [] escaped
		OBJ_PATTERN= ":[0-9]+" + OBJ_PATTERN
		logger.info('Searching for objects with pattern: %s', OBJ_PATTERN)
		line_cnt = 1
		for line in single_obj_file.readlines():
		    if line_cnt > HEADER:
			if re.search(OBJ_PATTERN, line):
			    g_single_obj_file.write(line)
		    else:
			g_single_obj_file.write(line)
			line_cnt = line_cnt + 1

		#echo the greped output
		g_single_obj_file.seek(0)
		leng = len(g_single_obj_file.readlines())
		logger.info('Found %d lines of objects after applying pattern', leng)
		if  leng > HEADER :
		    print_list_header(GREPED_SINGLE_OBJECT_LIST_FILE)
		else:
		    print "No match found."

		single_obj_file.close()
		g_single_obj_file.close()
		os.remove(GREPED_SINGLE_OBJECT_LIST_FILE)
		os.remove(SINGLE_OBJECT_LIST_FILE)
		    
		os.remove(SINGLE_LOCK_FILE)
    #unlock
		logger.info('Done grepp-ing, exiting with success.')
		sys.exit()

    #End of single object listing

    #Start of object listing ofr pattern and all

    # Object list file name 
    FILE_TYPE="sh"
    OBJECT_LIST_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE + "." + FILE_TYPE + ".lst"
    GREPED_OBJECT_LIST_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".grep"+".lst"
    OBJ_BUILD_LOCK_FILE=LOG_DIR+"/ns_objects_"+NAMESPACE+".lst.lck"

    #Setup the signal handler for those which spawns objects listing
    signal.signal(signal.SIGCHLD, handleSIGCHLD) 

    if os.path.exists(OBJECT_LIST_FILE):
	os.remove(OBJECT_LIST_FILE)


    if OBJ_PATTERN =="all" :
	OBJ_PATTERN=".*"
    else:
	#Normalize the Wildcard search
	OBJ_PATTERN=mgmt_lib.get_python_pattern(OBJ_PATTERN)
	logger.info('Normalized OBJ_PATTERN=%s',OBJ_PATTERN)

    if mgmt_lib.is_lock_valid(PATTERN_LOCK_FILE):
	print NAMESPACE+" : Pattern generation is in progress. Try after some time"
	logger.info('exiting because pattern generation in progress.')
	sys.exit();
    else:

	# Create the lock file
	mgmt_lib.acquire_lock(PATTERN_LOCK_FILE)

	# Now run the script that builds the whole object list in the background
	logger.info('Starting a full listing...')
	#do we need to hold this lock? Yes,Obj listing still can be started by delete
	#mgmt_build_list should take in a file as an argument
	if not mgmt_lib.is_lock_valid(OBJ_BUILD_LOCK_FILE):
	    if  os.path.exists(EXPORT_URL):
		export_url = open(EXPORT_URL, 'r+')
		REMOTE_URL = export_url.readline()
		if  os.path.exists(EXPORT_PASSWORD):
		    export_pass = open(EXPORT_PASSWORD, 'r+')
		    REMOTE_PASSWORD = export_pass.readline()
		    pid = subprocess.Popen(["nohup","python",  BUILD_OBJECT_LIST_SCRIPT, NAMESPACE, NS_UID, OBJ_PATTERN, FILE_TYPE, REMOTE_URL, REMOTE_PASSWORD],stdout=err_file, stderr=err_file).pid
		else:
		    pid = subprocess.Popen(["nohup","python",  BUILD_OBJECT_LIST_SCRIPT, NAMESPACE, NS_UID, OBJ_PATTERN, FILE_TYPE, REMOTE_URL],stdout=err_file, stderr=err_file).pid
	    else:
		pid = subprocess.Popen(["nohup","python",  BUILD_OBJECT_LIST_SCRIPT, NAMESPACE, NS_UID, OBJ_PATTERN, FILE_TYPE],stdout=err_file, stderr=err_file).pid
	else:
	    print NAMESPACE+" : Object list generation is already in progress.Please try after sometime."
	    #exiting here as we dont want multiple obj_ls writing to same file
	    sys.exit()

	# Just give a short pause and check for the log file
	logger.info('Sleep 8... and check for %s', OBJECT_LIST_FILE)
	#time.sleep(8)

	# Now check if the list file exists if not just print "no objects"
	# Ensure that only one pattern search is happening at a time.If the 
	#pattern lock exists. show an display message to try later and quit.
       
	#After we spawn the object_list script,
	#we expect the file to be present there within a time span
	#that depends on the load on the system and number of objecst at that instant
	#so loop for max count with a sleep
	retry_count = 0
	while not os.path.exists(OBJECT_LIST_FILE):
	    if retry_count > MAX_RETRYCOUNT:
		logger.warning('No object list file found. Returning with 0 objects')
		print NAMESPACE+" : Object list not generated for this namespace,exiting..."
		if mgmt_lib.is_pid_exists(pid):
		    os.kill(pid, 9)
		sys.exit();
	    else:
		logger.warning("%s : Object list not yet found for this namespace,Re-trying...", NAMESPACE)
		time.sleep(2)
	    retry_count = retry_count+1

     

	#Is it needed?
	
	# First check if request is for all objects and all domains as this is easy
	g_obj_list_file = open (GREPED_OBJECT_LIST_FILE, "w+")
	obj_list_file = open (OBJECT_LIST_FILE, "r")



	while 1:
	    line_cnt = len(obj_list_file.readlines())
	    if(line_cnt > MAX_DISPLAY):
		break
	    if not mgmt_lib.is_pid_exists(pid):
		break
	    obj_list_file.seek(0)
	    time.sleep(3)

	if OBJ_PATTERN ==".*" : 
	    if SEARCH_DOMAIN == "all":

		    # We have to echo the head of the object list
		    print_list_header(OBJECT_LIST_FILE)

		    # Done 
		    #sys.exit()
		    # Why are we exiting here? I have commented out here
	    else:
		    # Sleep an additional 4 sec if is a pattern search
		    logger.info('Sleepin 4.. and then check for list file')
		    time.sleep(4)

		    # Get the head part from the full list as is
		    obj_list_file.seek(0)		
		    line_cnt = 1
		    for line in obj_list_file.readlines():
			    if line_cnt > HEADER :
				    if re.search(MOUNT_POINT, line):
					    g_obj_list_file.write(line)
			    else:
				    g_obj_list_file.write(line)
				    line_cnt = line_cnt + 1

		    logger.info('Wrote a total of %d lines in %s', \
			    line_cnt, GREPED_OBJECT_LIST_FILE)

		    # We have to echo the head of the object list

		    g_obj_list_file.seek(0)
		    if  len(g_obj_list_file.readlines()) > HEADER :
			    print_list_header(GREPED_OBJECT_LIST_FILE)
			    count_object_lines(GREPED_OBJECT_LIST_FILE)
		    else:
			    print "No match found"
			    print "Note : If there are a lot of objects in the cache then"
			    print "       the background task to get the object list might"
			    print "       still be running, hence check again in about 15 seconds"


	else: 
	    # Sleep an additional 4 sec if is a pattern search
	    #no need for sleep here,as the grep is inline
	    #logger.info('(2) Sleep 4..')
	    time.sleep(4)


	    # Get the head part from the full list as is
	    #line_cnt = 1
	    obj_list_file.seek(0)
	    #for line in obj_list_file.readlines():
		    #if line_cnt > HEADER :
			    #if re.search(OBJ_PATTERN, line):
				    #g_obj_list_file.write(line)
		    #else:
			    #g_obj_list_file.write(line)
			    #line_cnt = line_cnt + 1

	    # We have to echo the head of the object list

	    #g_obj_list_file.seek(0)

	    if  len(obj_list_file.readlines()) > HEADER :
		    print_list_header(OBJECT_LIST_FILE)
		    #count_object_lines(GREPED_OBJECT_LIST_FILE)
	    else:
		    print "No match found"
		    print "Note : If there are a lot of objects in the cache then"
		    print "       the background task to get the object list might"
		    print "       still be running, hence check again in about 10 seconds"

	g_obj_list_file.close()
	obj_list_file.close()
	#Bug 8730- when the greped output is display, the same output list
	#should be avialable on upload.
    #    shutil.copy(GREPED_OBJECT_LIST_FILE, OBJECT_LIST_FILE) 
    #    os.remove(GREPED_OBJECT_LIST_FILE)
	os.remove(PATTERN_LOCK_FILE)  

    if  os.path.exists(EXPORT_URL):
	os.remove(EXPORT_URL)

    if  os.path.exists(EXPORT_PASSWORD):
	os.remove(EXPORT_PASSWORD)
	
	logger.info('Done....')
	logger.info('-------------------------------------------------------')
except KeyboardInterrupt:
    print 'Listing Interrupted, Exiting.'
    mgmt_lib.kill_mgmt_obj_ls(pid)
except re.error:
    print "Unsupported Expression"
    mgmt_lib.kill_mgmt_obj_ls(pid)
except SystemExit:
    pass
except Exception, e:
    mgmt_lib.kill_mgmt_obj_ls(pid)
    mgmt_lib.handle_error(LOG_DIR + "mgmt_obj_ls")



#
# End of cache_mgmt_ls.py
#

#!/usr/bin/python
#
# Python script to freeup metadata space by deleting URIs
# Author: Srihari MS / Jeba P (sriharim@juniper.net / jebap@juniper.net)
#

from datetime import datetime, date
import time
import sys, os, commands, re, fnmatch

# Check the metadata partition usage. If it is less than metadata_hwm, disk still has metadata space. Exit.
def get_metadata_use_percent(src_dir):
    usagevar=commands.getoutput('df '+src_dir)
    m=re.search("([0-9]+)%",usagevar)
    if(m==None):
        fh_error.write("Error getting metadata usage info from the filesystem")
        sys.exit(-1)
    metadata_use_percent = m.group(1)
    return(metadata_use_percent)

def update_del_list(container_file,active_uris):
    # Get the Namespace UID for the container. This is needed by the cache_slow_del.py script
    namespace = commands.getoutput('echo \"'+container_file+'\" | cut -f5 -d"/" | cut -f1 -d":"')
    uuid = commands.getoutput('echo \"'+container_file+'\" | cut -f5 -d"/" | cut -f2 -d":" | cut -c-8')
    tuid = namespace + ":" + uuid
    uid='/'+tuid
    # Get the domain for the container. This is needed by the cache_slow_del.py script
    domain=commands.getoutput('echo \"'+container_file+'\" | cut -f5 -d"/" | cut -f2- -d":" | cut -c10-')
    curkey=tuid+"_"+domain
    if(not(evictfiledict.has_key(curkey))):
        # The delete uri list for this domain is not yet created. Create one
        date_str = datetime.now().strftime("%Y%m%d-%H%M%S")
        evict_fname=dst_dir+date_str+"_Evict_"+curkey+".txt"
        fh_evict_fname=open(evict_fname,"a")
        evictfiledict[curkey]=[uid,domain,evict_fname,fh_evict_fname]
    else:
        # The delete uri list is already created. Just get the handle and append the file
        fh_evict_fname=evictfiledict[curkey][3]
    for lines in active_uris:
        # Store the URI in the delete uri list file. Example format : "www.ssd.com:80/SSD/myf206"
        uri='/'+commands.getoutput('echo \"'+lines+'\" | cut -f6- -d"/"')
        fh_evict_fname.write(domain+uri+"\n")
	fh_evict_fname.flush()
    return(0)

def get_container_info(path,container_file):
    # This list includes all the URIs including DELETED ones
    all_uris=commands.getoutput(cachels+" -1 -p \""+path+"\"").split('\n')
    # This list includes only non deleted URIs
    active_uris=commands.getoutput(cachels+" -1 -p -s \""+path+"\"").split('\n')
    len_all_uris=len(all_uris)
    len_active_uris=len(active_uris)
    if(len_active_uris==0):
        # If all the URIs have been deleted, then the code should have been deleted the container. Report it and log in syslog also.
        fh_error.write("ERROR : Empty container "+container_file+" is not removed by disk manager\n")
        #commands.getoutput("logger [ERROR] : Empty container "+container_file+" is not removed by disk manager")
#    else:
#        update_del_list(container_file,active_uris)
#
    elif(long(len_active_uris)<=long(numuris_thr)):
        # This container has less URIs than the number threshold provided by the user. Call update_del_list() to add all the active uris to the delete list.
        update_del_list(container_file,active_uris)
    else:
        use_percent=(len_active_uris/len_all_uris)*100
        if(int(use_percent)<=int(percent_thr)):
            # This container has percentage wise less number of active URIs. Call update_del_list() to add all the active uris to the delete list.
            update_del_list(container_file,active_uris)
    return(0)

#walk the '/nkn/mnt/*' and find all the container/attributes/freeblks file
def walk_over_dirs (top_dir):
    numcont_rd = 0
    for path, dirs, files in os.walk(top_dir, topdown=False):
	if numcont_rd > numcont_thr:
	    break
        #code segment to detect empty directories
        if((len(files)==0) and (len(dirs)==0)):
            if(not(fnmatch.fnmatch(path,'lost+found'))):
                tbuf=commands.getoutput("rm -f "+path)
	for filename in files:
	    if fnmatch.fnmatch(filename, contfname):
                size=os.path.getsize(path+'/'+filename)
                if (size <= 8192):
                    # Container not fully created. Delete it and proceed to next container
                    tbuf=commands.getoutput("rm -f "+path+'/'+filename)
                elif (not (os.path.isfile(path+'/'+attrfname))):
                    # Container does not have an attribute file pair. Delete it and proceed to next container
                    tbuf=commands.getoutput("rm -f "+path+'/'+filename)
                else:
                    # This is regular container. Call function get_container_info() check for the usage info
		    #print "Processing container :" + path + " " + str(numcont_rd)
                    get_container_info(path,path+'/'+filename)
		    numcont_rd = numcont_rd + 1
    return(0)

def do_meta_evict ():
    #evictfiledict=dict()
    walk_over_dirs (src_dir)
    # For each domain which has files to be deleted, call cache_slow_del.py and then remove the URI list file
    for curkey in evictfiledict.keys():
	ns_uid = str(evictfiledict[curkey][0])
	ns_parts = ns_uid.split(":")
	ns_name = ns_parts[0]
	tbuf2=commands.getoutput("logger [INFO] : Evicting uris from namespace :"+str(evictfiledict[curkey][0])+" domain :"+str(evictfiledict[curkey][1]))
	tbuf3=commands.getoutput(slowdel+" "+evictfiledict[curkey][1]+" "+evictfiledict[curkey][0]+" "+evictfiledict[curkey][2]+" "+ns_name)
	
	#evictfiledict[curkey][3].close()
        #time.sleep(15)
	tbuf1=commands.getoutput('rm -f '+evictfiledict[curkey][2])
        del evictfiledict[curkey]
    return(0)

#
#  main
#

# Check if the arguments are sufficient
if(len(sys.argv) < 4):
    print "Usage : "+sys.argv[0]+" <disk name> <percent_thr> <numuris_thr>\n"
    sys.exit()

# Initialize the variables that are required
src_dir = '/nkn/mnt/'+sys.argv[1]
tries = 0
metadata_hwm = 95
metadata_lwm = 90
percent_thr = int(sys.argv[2])
numuris_thr = int(sys.argv[3])
numcont_thr = 500
dst_dir = '/nkn/tmp/metadata_evict/'
cachels = '/opt/nkn/bin/cache_ls.sh'
slowdel = '/opt/nkn/bin/cache_slow_del.py'
contfname = 'container'
attrfname = 'attributes-2'

# If the dst_dir does not exist, then create it
if(not(os.path.isdir(dst_dir))):
    tbuf=commands.getoutput('mkdir -p '+dst_dir)

# File handle to store error messages if any
error = dst_dir+"meta_evict_error.txt"
fh_error = open(error,"a")

metadata_use_percent=int(get_metadata_use_percent(src_dir))
if(metadata_use_percent < int(metadata_hwm)):
    sys.exit()

tbuf=commands.getoutput("logger [INFO] : Reached metadata_hwm. Current use percentage is "+str(metadata_use_percent))

# This dictionary will hold file handle for each domain. A separate file will be created for each domain and the URIs from multiple containers under the domain will be added to the file
# The key:value is as follows:
# Key -> NamespaceUID+domain:port
# Value -> list containing [Namespace UID, Domain:port, Name of file where URIs to be deleted under this domain is stored, Handle to the file]
evictfiledict=dict()

while 1 :
    # We dont want to be looping without deleting anything
    if (tries >= 5):
	break
    metadata_use_percent=int(get_metadata_use_percent(src_dir))
    if (tries != 0):
	percent_thr = 75
	numuris_thr = 100
    if(metadata_use_percent > int(metadata_lwm)):
	tries = tries + 1
	do_meta_evict()
    else:
	break

# Close the file handles
fh_error.close()

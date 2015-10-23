import sys
import os
import re
import traceback
import datetime

def is_pid_exists(pid):
    if os.path.exists("/proc/" + str(pid) +"/status"):
	return 1
    else:
	return 0

#lock
def is_lock_valid(name):
    
    if not os.path.exists(name):
	return 0
    else:
	lockfd = open(name, "r")
	pid = lockfd.readline()
	if os.path.exists("/proc/" + pid +"/status"):
	    lockfd.close()
	    return 1
	else:
	    #process owning lock is not present,
	    #remove and create a new lock
	    lockfd.close()
	    # Create the lock file
            #newlockfd = os.open(name, "w")
	    #newlockfd.writeline(str(os.getpid()))
            #newlockfd.close()
	    return 0

#acquire Lock
def acquire_lock(name):
    lckhdl = os.open(name, os.O_CREAT | os.O_WRONLY)
    os.ftruncate(lckhdl, 0)
    os.write(lckhdl, str(os.getpid()))
    os.close(lckhdl)
    return

#function to get an equivalent python regex given bash style pattern
def get_python_pattern(bash_pattern):
    #Normalize the Wildcard search
    #Escape a . with \.
    bash_pattern=bash_pattern.replace(".","\.")
    bash_pattern = bash_pattern.replace("[", "\[")
    bash_pattern = bash_pattern.replace("]", "\]")
    bash_pattern = bash_pattern.replace("^", "\^")


    #Replace * with a greedy/non-greedy match
    bash_pattern=bash_pattern.replace("*","[^/]*")

    #Replace . with a greedy/non-greedy match
    bash_pattern=bash_pattern.replace("?",".")

    bash_pattern = bash_pattern + "$"

    return bash_pattern


#count object lines
def count_objects(filename):
    filehandler = open(filename, "r+")
    filehandler.seek(0)

    Numofobj = 0
    for line in filehandler.readlines():
        if re.match ("(.*)RAM(.*)", line) or re.match ("(.*)dc_(.*)", line):
                Numofobj = Numofobj + 1

    return Numofobj

#function to be called by except
#logs the backtrace in a file
#prints Unexpected error
def handle_error(log_name):
    print "Error occured"
    now = datetime.datetime.now()	
    f = open(log_name, 'a')
    f.write('='*60 + '\n')
    f.write(str(now) + '\n')
    f.write('-'*60 + '\n')
    traceback.print_exc(file=f)
    f.close()

def kill_mgmt_obj_ls(pid):
    if is_pid_exists(pid):
        os.kill(pid, 9)
    sys.exit(0)


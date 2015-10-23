import os
import subprocess
import time
import sys
import syslog
import commands
import re
import glob

#global variables
ACTIVE_DISKS = []

# Local Environment Variables
os.environ['PATH']=os.environ['PATH']+":/sbin"
MDREQ = "/opt/tms/bin/mdreq"
TM_FS_PART = "fs_partition"

GET_DISK_ID = "/nkn/nvsd/diskcache/config/disk_id"
TM_MISSING_AFTER_BOOT = "missing_after_boot"
TM_ACTIVATED = "activated"
TM_CACHE_ENABLED = "cache_enabled"
TM_COMMENTED_OUT = "commented_out"

#Function to get the disk id list
def list_disk_caches():

    fdisklist = []
    disk = ""
    query = MDREQ+" -v query iterate - "+GET_DISK_ID

    tdisklist = commands.getoutput(query)

    for dk in tdisklist:
        if dk != "\n":
            disk = disk + dk
        else:
            fdisklist.append(disk)
            disk = ""

    fdisklist.append(disk)
    return fdisklist

#Function to get the disk
def disk_get(disk,node):

    fdisklist = ""
    query = MDREQ+" -v query get - "+GET_DISK_ID+"/"+disk+"/"+node
    op = subprocess.Popen(query,stdout=subprocess.PIPE, shell=True)
    disklist = op.communicate()

    for disk in disklist:
        if (disk != None) and (disk != ''):
            fdisklist = disk.strip("\n")

    return fdisklist

#Function to get the diskname
def get_diskname(fs_part):

    diskname="";
    procfile=open("/proc/mounts","r")
    files=procfile.readlines()
    for line in files:
        list=line.split(" ")
        if list[0] == fs_part :
                diskname = list[1].strip("/nkn/mnt")

    return diskname

#Function to check whether the disk is missing after system reboot
def disk_missing_after_boot(serial_num):

    val = disk_get(serial_num,TM_MISSING_AFTER_BOOT)
    if val == "true" :
        return 0
    else:
        return 1

#Function to check whether the disk is in the activated or deactivated state
def disk_activated(serial_num):

    val = disk_get(serial_num,TM_ACTIVATED)
    if val == "true" :
        return 0
    else:
        return 1

#Function to check whether the disk is in the commented state
def disk_commented_out(serial_num):

    val = disk_get(serial_num, TM_COMMENTED_OUT)
    if val == "true" :
        return 0
    else:
        return 1

#Function to check whether the disk is enabled or disabled
def disk_enabled(serial_num):

    val = disk_get(serial_num,TM_CACHE_ENABLED)
    if val == "true" :
        return 0
    else:
        return 1


#Function to get the disk running info
def disk_running(diskname):

    diskrunning = ""
    query = "/opt/nkn/bin/nkncnt -t 0 -s "+diskname+".dm2_state | grep dm2_stat | awk '{print $3}'"
    op = subprocess.Popen(query,stdout=subprocess.PIPE, shell=True)
    disklist = op.communicate()

    for disk in disklist:
        if (disk != None) and (disk != ''):
            diskrunning = disk.strip("\n")

    if (diskrunning == "10"):
        return 0
    else:
        return 1


# Get the list of active disks
def get_active_disks():

    #Get the available disk list from the system#
    disks = list_disk_caches()

    for disk in disks:
        serial_num=disk
        fs_part=disk_get(disk,TM_FS_PART)
        diskname=get_diskname(fs_part)

        # Need to check that the disk exists in the system because it can be
        # pulled while in a 'cache enabled' 'status active' state.
        if not  disk_missing_after_boot(serial_num):
            continue
        if not disk_commented_out(serial_num):
            continue
        if disk_activated(serial_num):
            continue
        if disk_enabled(serial_num):
            continue
        if disk_running(diskname):
            continue

        # It is a disk of interest
        ACTIVE_DISKS.append(diskname)

    return ACTIVE_DISKS

# get the tier given the diskname
def get_disk_tier(diskname):

    query = "/opt/nkn/bin/nkncnt -t 0 -s "+diskname+".dm2_provider_type | grep dm2_provider |awk '{print $3}'"
    op = subprocess.Popen(query,stdout=subprocess.PIPE, shell=True)
    tokens = op.communicate()
    tier = -1
    for token in tokens:
        if (token != None) and (token != ''):
            tier = token.strip("\n")

    return tier

# get lowest tier
def is_lowest_tier(diskname):

    query = "/opt/nkn/bin/nkncnt -t 0 -s glob_dm2_lowest_tier | grep glob_dm2_lowest |awk '{print $3}'"
    op = subprocess.Popen(query,stdout=subprocess.PIPE, shell=True)
    tokens = op.communicate()

    for token in tokens:
        if (token != None) and (token != ''):
            lowest_tier = token.strip("\n")

    tier = get_disk_tier(diskname)

    if(tier == lowest_tier):
	return 1
    elif(tier == -1):
        return -1
    else:
	return 0


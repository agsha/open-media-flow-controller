#!/usr/bin/python
#
#Import the library files

import commands
import sys
import os
import string
import re

config_file_ls = [
[["config/nkn/key"], []],
[["config/nkn/csr"],[]],
[["config/nkn/cert"],[]],
[["config/nkn/ca"],[]],
[["nkn/smap"],[]],
[["nkn/policy_engine/"],["*tcl*"]],
]


def compress_config_files(f_name):
    cmd = "tar -zcvf " + f_name
    for i in xrange(0, len(config_file_ls)):
	if os.path.exists("/" + config_file_ls[i][0][0]):
	    if len(config_file_ls[i][1]):
	        cmd = cmd + " /" + config_file_ls[i][0][0]+config_file_ls[i][1][0]
	    else:
	        cmd = cmd + " /" + config_file_ls[i][0][0]
    ret = commands.getstatusoutput(cmd)
    return ret[0]
    #run command

def verify_compressed_files(f_name):
    cmd = "tar -tvf " + f_name
    output = commands.getoutput(cmd)
    lines = string.split(output, "\n")
    for line in lines:
	found = 0
	for i in xrange(0, len(config_file_ls)):
	    path = "/" + line.split() [5]
	    if re.search("^/" + config_file_ls[i][0][0] + "*", path):
		found = 1
		break
	if not found == 1:
	    return 0	
    return 1

def extract_config_files(f_name):
    cmd = "tar -xzvf " + f_name +" -C /"
    for i in xrange(0, len(config_file_ls)):
	if os.path.exists("/"+config_file_ls[i][0][0]):
	    if len(config_file_ls[i][1]):
	        cmd = cmd + " " + config_file_ls[i][0][0]+config_file_ls[i][1][0]
	    else:
	        cmd = cmd + " " + config_file_ls[i][0][0]
    status = commands.getstatusoutput(cmd)
    return status[0]

######################################################################
#Main
######################################################################

cmd = sys.argv[1]
f_name = sys.argv[2]
if cmd == "1":
    ret = compress_config_files(f_name)
    if not ret:
	print "Files compressed."
    else:
	print "Error compressing files"
elif cmd == "2":
    ret = verify_compressed_files(f_name)
    if not ret:
	print "Unauthorized files."
	sys.exit()
    ret = extract_config_files(f_name)
    if not ret:
	print "Files Extracted."
    else:
	print "Error extracting files."



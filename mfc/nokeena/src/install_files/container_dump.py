#!/usr/bin/python
#
# Python script to create a dump of the cache meta data.
# This script would be invoked from the 'tech-support' command.
# Author: Srihari MS (srihari@ankeena.com)
#

from datetime import datetime, date, time
import os, fnmatch, socket, shutil, tarfile

# Initialize the variables that are required
host = socket.gethostname()
date_str_file = datetime.now().strftime("%Y%m%d-%H%M%S")
output_prefix = 'cachedump-'+host+'-'+date_str_file
src_dir = '/nkn/mnt/'
final_dir = '/nkn/tmp'
stage_dir = os.path.join(final_dir, output_prefix)
dump_filename = os.path.join(final_dir, output_prefix) +'.tgz'

#create the cache-dump tgz, delete the cache-dump directory
def create_tar():
	cd_tar = tarfile.open(dump_filename, mode = 'w:gz')
	cd_tar.add(stage_dir, os.path.basename(stage_dir))
	cd_tar.close()
	shutil.rmtree(stage_dir);
	print "CONTAINER="+dump_filename

#copy the file to the cache-dump directory
def copy_file (dirpath, name):
    src_file = os.path.join(dirpath, name);
    dst_dir = stage_dir+dirpath;
    dst_file = os.path.join(dst_dir, name);
    if not os.path.exists(dst_dir):
	os.makedirs(dst_dir);
    shutil.copy2(src_file, dst_file);
    return

#walk the '/nkn/mnt/*' and find all the container/attributes/freeblks file
def dump_contents ():
    for dirpath, dirs, files in os.walk(src_dir, topdown=False):
	for name in files:
	    if fnmatch.fnmatch(name, 'freeblks'):
		copy_file(dirpath, name)
	    if fnmatch.fnmatch(name, 'container'):
		copy_file(dirpath, name)
	    if fnmatch.fnmatch(name, 'attributes*'):
		copy_file(dirpath, name)

#
#  main
#

dump_contents ()
create_tar()

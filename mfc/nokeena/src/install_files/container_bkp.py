#!/usr/bin/python
#
# Python script to back up bad containers for offline use.
# Author: Srihari MS (srihari@ankeena.com)
#

from datetime import datetime, date, time
import sys, os, shutil

#validate the input args
argc =  len(sys.argv)-1
if argc == 0:
   print "Usage: %s <Destination Dir>" % sys.argv[0]
   sys.exit(0)

#initialize the variables
cont_dir = sys.argv[1]
date_str = datetime.now().strftime("%Y%m%d-%H%M%S")
dest_dir = '/log/dump'
cont_name = os.path.join(cont_dir, 'container')
attr_name = os.path.join(cont_dir, 'attributes-2')

#prepare the output filename - 'filename-yyyymmdd-hhmmss'
#example: 'attributes-2-20100608-062608" "container-20100608-062608"

dest_cont = os.path.join(dest_dir,'container-') + date_str
dest_attr = os.path.join(dest_dir,'attributes-2-') + date_str

#do the copy
if not os.path.exists(dest_dir):
    os.makedirs(dest_dir)
if os.path.exists(cont_name):
    shutil.copy2(cont_name, dest_cont)
if os.path.exists(attr_name):
    shutil.copy2(attr_name, dest_attr)


#!/usr/bin/python
#
#       File : prefetch.py
#
#       Modified by : Ramalingam Chandran
#       Created Date : 13st September, 2010
#
#       Copyright notice ( 2010 Juniper Networks Inc., All rights reserved)
#

#Import the library files

import os
import sys
import syslog
import re
from urllib2 import Request, urlopen, URLError, HTTPError
from urlparse import urlparse

#Get the the command line arguments
options = sys.argv[1]
jobname = sys.argv[2]

#initiate the syslog
syslog.openlog(sys.argv[0])


#delete the scheduled job
if (options == "delete"):
    #filename = sys.argv[2]
    filepath  = "/nkn/prefetch/jobs/"+jobname
    if (os.path.exists(filepath)):
    #check if the scheduled job is already started. Check for the lock file
        lockfile = "/nkn/prefetch/lockfile/"+jobname+".lck"
        if (os.path.exists(lockfile)):
            print "The scheduled job is in progress. Cannot delete the job."
	    syslog.syslog(syslog.LOG_NOTICE, "The scheduled job is in progress. cannot delete the job")
        else:
            os.remove(filepath)
            print "The Downloaded URL file is successfully deleted."
	    syslog.syslog(syslog.LOG_NOTICE, "The Downloaded URL file is successfully deleted")

#open the file to read the URL's
if (options == "download"):

    failure = 0
    success = 0

    fileurl = sys.argv[2]
    filepath = "/nkn/prefetch/jobs/"+jobname

    #Check whether the scheduled job is already removed
    if (not os.path.exists(filepath)):
        syslog.syslog(syslog.LOG_NOTICE,"The scheduled job file - %s is already removed" % jobname)
        sys.exit()

    syslog.syslog(syslog.LOG_NOTICE,"Prefetch Started for the url file - %s " % jobname)

    #Create a lock file to  avoid deletion during the download
    lockfile = "/nkn/prefetch/lockfile/"+jobname+".lck"
    lockfilehdlr = os.open(lockfile, os.O_CREAT)
    os.close(lockfilehdlr)

    #Open the URL file and start the download
    filehandler = open(filepath, 'r')

    for url in filehandler:
	#/*Parse the URL and get the values*/
	url_details = urlparse(url)	
	if url_details[0] == None or url_details[1] == None :
		#/*url_details[0] specifies protocol*/
		#/*url_details[1] specifies domain*/
		#/*url_details[2] specifies path of the object*/
                syslog.syslog(syslog.LOG_NOTICE,"Malformed Url" % url)
		failure = failure + 1
        else :
		if not url_details[0] == "http" :
			failure = failure + 1
			continue
		domain_name = url_details[1]
         	uri = url_details[2]

		urireq = "http://127.0.0.1" + uri
	        req = Request(urireq)
		req.add_header('Host', domain_name)
		req.add_header('X-NKN-Internal', 'client=pre-fetch')
		try:
			response = urlopen(req)
		except HTTPError, e:
			failure = failure + 1
		except URLError, e:
			failure = failure + 1
		else:
			content_len = response.headers["Content-Length"]
                        two_mb = 2 * 1024 * 1024
                        while 1:
                                if int(content_len) > two_mb:
                                        response.read(two_mb)
                                        content_len = int(content_len) - two_mb
                                else:
                                        response.read()
                                        break
			success = success + 1

    syslog.syslog(syslog.LOG_NOTICE,"Prefetch completed for the url file - %s   Success - %d   Failure - %d" % (jobname, success, failure))

    #change the status of the job
    cmd = "/opt/tms/bin/mdreq set modify - /nkn/prefetch/config/"+jobname+"/status string completed"
    os.system(cmd);

    #Delete only the lock file once the download is completed
    #URL file can be used for re-try and will be deleted when job is deleted
    filehandler.close()
    os.remove(lockfile)
#Close syslog
syslog.closelog()


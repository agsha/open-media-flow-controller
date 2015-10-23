#! /usr/bin/python


import os
import errno
import time
import sys
import logging
import logging.handlers
from mdreq import MdReq
import fcntl

"""  Constants  """

# Log formats for our internal [debug] logger
DEBUG_LOG_FORMAT = '[%(asctime)s] [%(process)d] [%(levelname)s] : %(message)s'

# Log format that is visible to the user in syslog
SYSLOG_FORMAT = '%(name)s[%(process)d]: [%(name)s.%(levelname)s]: %(message)s'

# Config nodes that we will query for
NODE_LOG_EXPORT_CRITERIA = '/nkn/logexport/config/purge/criteria/'
NODE_LOG_EXPORT_INTER_THRESH = NODE_LOG_EXPORT_CRITERIA + 'interval'
NODE_LOG_EXPORT_SIZE_THRESH = NODE_LOG_EXPORT_CRITERIA + 'threshold_size_thou_pct'

#file which tells the size reserved for log space
LOG_EXPORT_SIZE = '/config/nkn/log-export-size'

# Base path where all log files are stored
BASE_DIR_PATH = '/log/logexport/chroot/home/LogExport/'

# a log file that we use for all messages that are generated from this script
LOG_FILE_NAME = '/var/log/nkn/log-export-task.log'


# Cause/Reason codes - only for logging into Syslog
CODE_EXPIRED =      1001
CODE_SIZE_THRESH =  1002


def getFolderSize(folder):
    total = os.path.getsize(folder)
    for i in os.listdir(folder):
        path = os.path.join(folder, i)
        if os.path.isfile(path):
            total += os.path.getsize(path)
        elif os.path.isdir(path):
            total += getFolderSize(path)
    return total

def getDiskUsage(path):
    """Return disk usage statistics about the given path.

    Returned values is a tuple with attributes 'total', 'used' and
    'free', which are the amount of total, used and free space, in bytes.
    """
    global strerr
    strerr = ""
    total = 0
    used = 0

    # read /log/log-export-size (symlink) to get the size of the
    # log space
    try:
	f = open(LOG_EXPORT_SIZE, 'r')
        total = int(f.read())
	#since the size in is 1K bytes in LOG_EXPORT_SIZE and we need in bytes
	total = total*1024
        f.close()
    except OSError(errno, strerr):
        logger.info('Exception (readfile): ' + str(errno) + ': ' + strerr)
    # Good, so far. Now get the used space
    #
    try:
        used = getFolderSize(BASE_DIR_PATH)
    except OSError(errno,strerr):
        logger.info('OS Exception: ' + str(errno) + ': ' + strerr)
    except:
        logger.info('Unhandled exception:')

    return ( total, used )


# Get oldest files that are candidates for delete
def oldFilestoDelete(dirpath):
    return sorted(
            (os.path.join(dirname, filename)
                for dirname, dirnames, filenames in os.walk(dirpath)
                for filename in filenames),
            key = lambda fn: os.stat(fn).st_mtime,
            reverse = True)


def freeSpaceUpto(Threshold, used_bytes, dirpath):
    global strerr
    strerr = ""
    try:
        file_list = oldFilestoDelete(dirpath)
        while (used_bytes > Threshold) and file_list :
            del_file = file_list.pop()
            os.remove(del_file)
            syslog.info('(' + \
                    str(CODE_SIZE_THRESH) + ')' + \
                    'log file deleted: \'' + \
                    os.path.basename(del_file) + '\'' + \
                    '(space used: ' + str(used_bytes) + \
                    ', threshold: ' + str(Threshold) + ')')
            logger.info('Deleting [size based criteria]: ' + del_file)
            used_bytes = getFolderSize(dirpath)

    except OSError (errno, strerr):
        logger.info('Unhandled exception:')



def onIntervalThresh(interThresh = 0):

    """ NOTE:
    interThresh must always be in seconds when this method
    is called.
    """
    # Do nothing if threshold is set to 0
    if  interThresh == 0:
        return

    global strerr
    strerr = ""
    try:
        for file in os.listdir(BASE_DIR_PATH):
            # Grab the create time
            """ WARN: Linux has no notion of create time. Last
            modified time or access time is considered as create
            time.
            """
            # Only work with Epoch's and UTC!
            u_ctime = os.stat(BASE_DIR_PATH + file).st_ctime
            u_now = time.time()

            if (u_ctime > (u_now - (2 * interThresh))) and \
                    (u_ctime < (u_now - interThresh)) :
                os.remove(BASE_DIR_PATH + file)
                logger.info('Deleting[frequency based criteria]: ' + \
                        BASE_DIR_PATH + file)

                # format: (1001) log file deleted: <filename>
                # Never show directory in log
                syslog.info('(' +                   \
                        str(CODE_EXPIRED) +         \
                        ') log file deleted: \'' +  \
                        file + '\'')

    except OSError (errno, strerr):
        raise



def onSizeThresh(sizeThresh = 0):


    if sizeThresh == 0:
        return

    global strerr
    strerr = ""
    try:
        (total, used) = getDiskUsage(BASE_DIR_PATH)

	Threshold = sizeThresh * total/1000/100

        if Threshold < used:
            logger.info('Log directory size:' + str(used) + \
                    'bytes exceeds threshold size:' + str(Threshold))
            freeSpaceUpto(Threshold, used, BASE_DIR_PATH)

    except OSError (errno, strerr):
        logger.info('OS Exception: ' + str(errno) + ':' + strerr)
    except:
        logger.info('Unhandled exception:')



####### MAIN ENTRY POINT #######
if __name__ == '__main__':

    # Set up log handlers
    logger = logging.getLogger('log-purge')
    hdlr = logging.handlers.RotatingFileHandler(LOG_FILE_NAME, \
            maxBytes = 20480, backupCount = 3)
    formatter = logging.Formatter(DEBUG_LOG_FORMAT)
    hdlr.setFormatter(formatter)
    logger.addHandler(hdlr)
    logger.setLevel(logging.INFO)

    # Setup a log handler for Syslogs
    syslog = logging.getLogger('log-export')
    hdlr = logging.handlers.SysLogHandler(address = '/dev/log')
    formatter = logging.Formatter(SYSLOG_FORMAT)
    hdlr.setFormatter(formatter)
    syslog.addHandler(hdlr)
    syslog.setLevel(logging.INFO)


    ###############################################################
    """ Need to check if there is another instance of this script
    running. This can be done with either a flock or adding a
    sentinel at this place... not before and not after.
    """
    pid_file = 'logCleanup.lock'
    fp = open(pid_file, 'w')
    try:
        fcntl.lockf(fp, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except IOError:
    # kill if another instance is running
           logger.warn('attempt to re-enter when already running' \
                       'an instance')
	   sys.exit(0)


    # Read in configured parameters
    mdreq = MdReq()
    try:

		sizeThresh = int(mdreq.query(NODE_LOG_EXPORT_SIZE_THRESH))
		#convert the interval from hours to seconds
		interThresh = int(mdreq.query(NODE_LOG_EXPORT_INTER_THRESH))*60*60
    except Exception, e:
         logger.error('Bad karma!!! - fix my exception handling')
	
    onSizeThresh(sizeThresh)
    onIntervalThresh(interThresh)


    sys.exit(0)

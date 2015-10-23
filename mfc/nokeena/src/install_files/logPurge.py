#! /usr/bin/python


import os
import errno
import logging
import logging.handlers
import fcntl
import sys

"""  Constants  """

# Log formats for our internal [debug] logger
DEBUG_LOG_FORMAT = '[%(asctime)s] [%(process)d] [%(levelname)s] : %(message)s'

# Log format that is visible to the user in syslog
SYSLOG_FORMAT = '%(name)s[%(process)d]: [%(name)s.%(levelname)s]: %(message)s'

# Base path where all log files are stored
BASE_DIR_PATH = '/log/logexport/chroot/home/LogExport/'

LOG_FILE_NAME = '/var/log/nkn/log-export-task.log'
# Cause/Reason codes - only for logging into Syslog
CODE_FORCEPURGE = 1003

def onforcePurge():

    global strerr
    strerr = ""
    try:
        for file in os.listdir(BASE_DIR_PATH):
                # format: (1003) log file deleted: <filename>

            if os.path.isfile(BASE_DIR_PATH + file):
                os.remove(BASE_DIR_PATH + file)

	            # log the filenames that will be deleted
                logger.info('Deleting due to forcepurge: ' + \
                        BASE_DIR_PATH + file)
                # log the filenames in syslog - PR 773314
                syslog.warning('(' +                \
                        str(CODE_FORCEPURGE) +      \
                        ') log file deleted: \'' +  \
                        file + '\'')
    except OSError (errno, strerr):
        raise


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
    pid_file = 'logPurge.lock'
    fp = open(pid_file, 'w')
    try:
        fcntl.lockf(fp, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except IOError:
    # kill if another instance is running
        logger.warn('attempt to re-enter when already running' \
               'an instancei of logPurge')
        sys.exit(0)

    onforcePurge()

    sys.exit(0)

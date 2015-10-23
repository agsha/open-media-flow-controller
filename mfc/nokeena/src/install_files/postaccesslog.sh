#! /bin/bash

# Customer may edit this file for accesslog process.

/usr/bin/gzip $1

# uncomment out the next lines to run log analysis tool to analyze this log file.
# if [[ $2 =~ "AccessLog" ]]; then
# /opt/nkn/bin/log_analyzer -f /config/nkn/log_analyzer.conf -m 0 -p $1.gz
# fi


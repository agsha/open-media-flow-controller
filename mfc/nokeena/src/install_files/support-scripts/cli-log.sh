#! /bin/bash







if [ -f /var/log/messages ] ; then 
    /bin/grep -e "[cli.NOTICE]" /var/log/messages > /var/log/cli-log
fi

exit 0

#!/bin/sh

if [ ! -f /tmp/nvsd-fail ]; then
        # everything is fine, send a 200 OK
        echo -e "Content-type: text/html\n"
else
	# no content type is intentional, will be a 500 error after 10 seconds
	# allows RPM to detect the failure cleanly
        sleep 10
fi

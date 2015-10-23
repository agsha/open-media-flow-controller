#! /bin/bash

usage() {
  echo "Usage: cache_del [-D <domain>] URI_ARG"
  echo "	where, URI_ARG is 'A' || 'B' with"
  echo "	A: '-F <URI list file>'"
  echo "	B: <full URI name>"
  echo "Notes:"
  echo "  If -D option not given, <domain> defaults to 'any'"
  exit 1
}

if [ $# -lt 1 ]; then
  usage
fi

while getopts ":D:F" options; do
  case $options in
    D ) DOMAIN=$OPTARG
	;;
    F ) DEL_LIST=1
	;;
    h ) usage
	;;
    \? ) usage
	;;
    * ) usage
	;;
  esac
done

if [ "${DOMAIN}" != "" ]; then
  shift;shift;
fi
if [ "${DEL_LIST}" != "" ]; then
    shift;
fi
if [ "$DOMAIN" == "" ]; then
  DOMAIN=any
fi

# if DEL_LIST is 1, URI will point a file that lists the objects to be deleted.
# the URI list file is passed to nknfqueue as an attribute using the 
# 'del_uri_file' token. the arg to 'r' option is a special object "/tmp/ignore"
# which nvsd will ignore.
# if DEL_LIST is 0, URI will be the actual object to be deleted.

URI=$1

if [ "$DEL_LIST" != "1" ]; then
    ARGLIST="-s ${DOMAIN} -r $URI -q /tmp/FMGR.queue"
else
    ARGLIST="-s ${DOMAIN} -N del_uri_file:$URI -r /tmp/ignore -q /tmp/FMGR.queue"
fi

ret=1
while [ $ret -ne 0 ]; do
  /opt/nkn/bin/nknfqueue ${ARGLIST}
  ret=$?
  if [ $ret -ne 0 ]; then
    sleep 1
  fi
done

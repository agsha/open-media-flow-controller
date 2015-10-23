#! /bin/bash

usage() {
  echo "cmm_op.sh [-c] [-f] [-h] [-i] [-r] [-u] <NodeHandle: {NSToken}:{Instance}:{Host:Port}> <cancel|monitor>"
  echo "  -c connect timeout msecs (default: 100 msecs)"
  echo "  -f allowed failures (default: 3)"
  echo "  -h help"
  echo "  -i heartbeat interval msecs (default: 100 msecs)"
  echo "  -r read timeout msecs (default: 100)"
  echo "  -u heartbeat URL"
  exit 1
}

if [ $# -lt 2 ]; then
  usage;
fi

while getopts "c:f:hi:r:u:" options; do
  case $options in
    c ) CONNECT_TIMEOUT=$OPTARG
	;;
    f ) ALLOWED_FAILURES=$OPTARG
	;;
    h ) usage
	;;
    i ) HEARTBEAT_INTERVAL=$OPTARG
	;;
    r ) READ_TIMEOUT=$OPTARG
	;;
    u ) HEARTBEAT_URL=$OPTARG
	;;
    \? ) usage
	;;
    * ) usage
	;;
  esac
done

if [ "${CONNECT_TIMEOUT}" != "" ]; then
    shift;shift;
fi
if [ "${ALLOWED_FAILURES}" != "" ]; then
    shift;shift;
fi
if [ "${HEARTBEAT_INTERVAL}" != "" ]; then
    shift;shift;
fi
if [ "${READ_TIMEOUT}" != "" ]; then
    shift;shift;
fi
if [ "${HEARTBEAT_URL}" != "" ]; then
    shift;shift;
fi

NODE_HANDLE=$1
if [ "${NODE_HANDLE}" == "" ]; then
    echo "NodeHandle not specified"
    usage
fi

ACTION=$2
if [ "${ACTION}" != "cancel" -a "${ACTION}" != "monitor" ]; then
    echo "Incorrect action"
    usage
fi 

if [ "${HEARTBEAT_URL}" == "" -a "${ACTION}" == "monitor" ]; then
    echo "Heartbeat URL not specified"
    usage
fi

ARGLIST="-n -u http://cmm -p /tmp"

if [ "${ACTION}" == "cancel" ]; then
    ARGLIST="${ARGLIST} -N Cancel:1 -N \"Data:${NODE_HANDLE};\""
fi

if [ "${ACTION}" == "monitor" ]; then
    ARGLIST="${ARGLIST} -N Monitor:1"

    # Handle default options

    if [ "${HEARTBEAT_INTERVAL}" == "" ]; then
        HEARTBEAT_INTERVAL=100
    fi

    if [ "${CONNECT_TIMEOUT}" == "" ]; then
        CONNECT_TIMEOUT=100
    fi
    
    if [ "${ALLOWED_FAILURES}" == "" ]; then
        ALLOWED_FAILURES=3
    fi
    
    if [ "${READ_TIMEOUT}" == "" ]; then
        READ_TIMEOUT=100
    fi

    ARGLIST="${ARGLIST} -N \"Data:${NODE_HANDLE} ${HEARTBEAT_URL} ${HEARTBEAT_INTERVAL} ${CONNECT_TIMEOUT} ${ALLOWED_FAILURES} ${READ_TIMEOUT};\""
fi

/bin/bash -c "/opt/nkn/bin/nknfqueue -i /tmp/fqelement.$$ ${ARGLIST}"
/opt/nkn/bin/nknfqueue -e /tmp/fqelement.$$ -q /tmp/CMM.queue
rm -rf /tmp/fqelement.$$
exit 0

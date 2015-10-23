#! /bin/bash

usage() {
  echo "Usage: cache_ls [-a] [-h] [-1] [-5 <no_of_bytes>] [-L] [-p] [-C] [-s] [-t] <directory name>"
  echo "Options: For now, extra options must be given in above format"
  echo "     - a: Print only attributes"
  echo "     - h: print headers also"
  echo "     - 1: Try to print one row per URI"
  echo "     - 5 <no_of_bytes>: Print md5 checksum for <no_of_byes> of first"
  echo "         extent"
  echo "     - L: Print content length (read extra file)"
  echo "     - p: Print entire pathname of URI"
  echo "     - C: Print checksum"
  echo "     - s: Skip deleted entries"
  echo "     - t: Print temperature & update time (read extra file)"
  echo "     - v: Print attribute version with -a option"
  echo "  Note: "
  echo "     - pass '/nkn/mnt' to see all URIs"
  echo "     - pass '/nkn/mnt/dc_1' to see URIs in the dc_1 disk"
  echo 
  exit 1
}

if [ $# -lt 1 ]; then
    usage;
fi

SHIFTCNT=0
OPTS="-cd"
while getopts ":ah1LpCstv5:" opt; do
  case $opt in
    [ah1LpstCv] ) OPTS="${OPTS}${opt}"
	;;
    5 ) NO_OF_BYTES=$OPTARG
        (( SHIFTCNT++ ))
	MD5ARGS="-5 ${NO_OF_BYTES}"
        ;;
    \? ) usage
	;;
    * ) usage
	;;
  esac
  (( SHIFTCNT++ ))
done

if [ "${NO_OF_BYTES}" != "" ]; then
  if [[ "${NO_OF_BYTES}" =~ ^[0-9]+$ ]]; then
    if [ $[$NO_OF_BYTES % (512) ] != 0 ]; then
      echo "number of bytes to calculate md5 must be multiple of"
      echo "512 and less than or equal to 2MB"
      usage
    fi
    if [ "$NO_OF_BYTES" -lt 0 ]; then
      echo "number of bytes to calculate md5 must be between 0 and 2MB"
      usage
    fi
    if [ "$NO_OF_BYTES" -gt 2097152 ]; then
      echo "number of bytes to calculate md5 must be between 0 and 2MB"
      usage
    fi
  else
    echo "Integer expected for number of bytes to calculate md5"
    usage
  fi
fi

while [ $SHIFTCNT -gt 0 ]; do
  shift;
  (( SHIFTCNT-- ))
done

# Since there could be spaces in the directory name set the IFS to not take
# space as delimiter
IFS=$'\t\n'
find $* -name container | head -1 | grep -q container
if [ $? -ne 0 ]; then
  echo "Cache is empty"
  exit 0
fi

/opt/nkn/bin/container_read $OPTS $MD5ARGS -R $*
IFS=$' \t\n'

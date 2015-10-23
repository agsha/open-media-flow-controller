:
# Get_Device_From_Http.sh -- Find out which network device gives
# access to the specfied http or https URL.

function Usage()
{
  echo Print out what eth device is used to access an http or https URL.
  echo Syntax:
  echo '    [-v] <URL>'
  echo "The -v prints to stderr everything that is being done."
  echo "The URL must be http or https."
  echo "You do not have to specify the leading 'http://'"
}

VERBOSE=quiet
URL=
for i in ${*}
do
  case "${i}" in
  -v|--v*) VERBOSE=verbose ;;
  -h|--h*)  Usage; exit 1;;
  http://*)  URL="${i}" ;;
  https://*) URL="${i}" ;;
  //*)       URL=http:"${i}" ;;
  *)         URL=http://"${i}" ;;
  esac
done

if [ -z "${URL}" ] ; then
  echo Must specify a URL 1>&2
  Usage
  exit 1
fi

function Vprint()
{
  [ "${VERBOSE}" = "verbose" ] && echo "${@}" 1>&2
}
Vprint Testing URL=${URL}

ALL_ETH_LIST=
UP_ETH_LIST=
DOWN_ETH_LIST=
IF_RAW_LIST=`/sbin/ifconfig -a | grep 'Ethernet.*HWaddr' | sed 's/^\([^ ]*\) .*HWaddr /\1-/' `
for i in ${IF_RAW_LIST}
do
  ETH_NAME=`echo ${i} | cut -f1 -d-`
  ALL_ETH_LIST="${ALL_ETH_LIST} ${ETH_NAME}"
  /sbin/ifconfig ${ETH_NAME} | grep -q ' UP '
  if [ ${?} -eq 0 ] ; then
    UP_ETH_LIST="${UP_ETH_LIST} ${ETH_NAME}"
  else
    DOWN_ETH_LIST="${DOWN_ETH_LIST} ${ETH_NAME}"
  fi
done

Vprint "All eth devs=${ALL_ETH_LIST}"
Vprint "Up   devs=${UP_ETH_LIST}"
Vprint "Down devs=${DOWN_ETH_LIST}"

function Restore_Up_State()
{
  for ETH_NAME in ${UP_ETH_LIST}
  do
    Vprint /sbin/ifconfig ${ETH_NAME} up
    /sbin/ifconfig ${ETH_NAME} up > /dev/null 2> /dev/null
  done
}

function trap_signal()
{
  echo Caught signal 1>&2
  Restore_Up_State
  exit 1
}

trap trap_signal HUP INT QUIT PIPE TERM 

# Set all the up interfaces to down
for ETH_NAME in ${UP_ETH_LIST}
do
  Vprint /sbin/ifconfig ${ETH_NAME} down
  /sbin/ifconfig ${ETH_NAME} down 1>&2
  if [ ${?} -ne 0 ] ; then
    echo "No permission to change ${ETH_NAME}" 1>&2
    echo "Cannot determine which device provides access to ${URL}" 1>&2
    Restore_Up_State
    exit 1
  fi
done
sleep 2

# Now enable the devices one at a time to see if the URL can be accessed.
# Do the devices that were already up first.
FOUND=
for DEV in ${UP_ETH_LIST} ${DOWN_ETH_LIST}
do
  Vprint /sbin/ifconfig ${DEV} up
  /sbin/ifconfig ${DEV} up 1>&2
  sleep 2
  wget --spider ${URL} 2> /dev/null
  RV=${?}
  Vprint /sbin/ifconfig ${DEV} down
  /sbin/ifconfig ${DEV} down 1>&2
  if [ ${RV} -eq 0 ] ; then
    FOUND=${DEV}
    Vprint FOUND ${DEV}
    break
  fi
done
Restore_Up_State

[ -z "${FOUND}" ] && exit 1
# Found
for i in ${IF_RAW_LIST}
do
  echo ${i} | grep ${FOUND}-
done
exit 0

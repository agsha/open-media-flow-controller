#! /bin/bash
# Flash the ethernet port LEDs in order, over and over.
# Use the order that ifconfig gives us.

STOP_FILE=/tmp/stop.$$.tmp
trap_int_handler()
{
  touch ${STOP_FILE}
}
rm -f ${STOP_FILE}
trap "trap_int_handler" INT

LOOPS=10
case _${1} in
_[0-9]|_[0-9][0-9])
  LOOPS=${1}
  shift
  ;;
_-h|_--help)
  echo 'Syntax: [<LoopCount>] [ + | <input-file> [ <ref-file> ] ]'
  exit 1
  ;;
esac

LOOP_COUNT=1
F_SEC=3
P_SEC=10
echo Flashing eth LEDs
echo Flashing each for ${F_SEC} seconds, then a ${P_SEC} second pause, for ${LOOPS} times
while true
do
  echo Loop ${LOOP_COUNT}
  LOOP_COUNT=$(( ${LOOP_COUNT} + 1 ))
  for ETH_NAME in `/sbin/ifconfig -a | grep ^eth | cut -f1 -d' '`
  do
    [ -f ${STOP_FILE} ] && break
    echo Flashing ${ETH_NAME}
    /usr/sbin/ethtool -p ${ETH_NAME} ${F_SEC}
  done
  [ -f ${STOP_FILE} ] && break
  [ ${LOOP_COUNT} -gt ${LOOPS} ] && break
  echo Pause
  sleep ${P_SEC}
done
trap - INT
if [ -f ${STOP_FILE} ] ; then
  echo
  echo "(Interupt)"
  echo Stopped flashing LEDs
  echo
fi
rm -f ${STOP_FILE}

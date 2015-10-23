:
if [ -z "${LD_LIBRARY_PATH}" ] ; then
  LD_LIBRARY_PATH=/usr/StorMan
else
  LD_LIBRARY_PATH="${LD_LIBRARY_PATH}":/usr/StorMan
fi
export LD_LIBRARY_PATH


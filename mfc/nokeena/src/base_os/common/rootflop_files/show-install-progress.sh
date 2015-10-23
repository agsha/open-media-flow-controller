:
# This script is run when installation is being run on another
# terminal, or in the background, to show the installation progress.

trap_int_handler()
{
  echo
  echo "(Interrupt)"
  echo
}
trap trap_int_handler INT

if [ -f /tmp/autoinstall.log ] ; then
  echo Automatic install in progress
  echo 
  tail -f /tmp/autoinstall.log
fi

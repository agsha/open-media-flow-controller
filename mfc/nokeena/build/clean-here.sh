#
OPT="${1}"
svn info &>/dev/null
if [ ${?} -ne 0 ] ; then
  echo Not in an SVN tree
  exit 1
fi
Clean_This()
{
  A=`svn status | grep '^?' | grep ${1} | wc -l`
  if [ ${A} -eq 0 ] ; then
    echo Already clean
    return
  fi
  if [ "_${OPT}" != "_-" ] ; then
    echo "Use the parameter '-' to actually remove the files:"
    svn status | grep '^?' | grep ${1} | sed "s/^.     //"
  else
    svn status | grep '^?' | grep ${1} | sed "s/^.     /rm/"
    svn status | grep '^?' | grep ${1} | cut -c9- | xargs rm
  fi
}
if [ -d nokeena/src ] ; then
  Clean_This nokeena
else
  Clean_This .
fi

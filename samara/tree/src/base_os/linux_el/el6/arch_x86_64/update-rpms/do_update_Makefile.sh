:
MY_DIR=`dirname ${0}`
[ ! -d tmpdir ] && mkdir tmpdir
FROM=Makefile.orig
[ ! -f ${FROM} ] && cp -a Makefile ${FROM}

cat Makefile.orig \
  | grep DIST_ \
  | grep -v DIST_OTHER \
  | grep -v DIST_UNKNOWN \
  | grep .rpm \
  | sed "/^.*\//s//unknown\//" \
  | cut -f1 -d' ' \
  | sort | uniq \
  > tmpdir/rpms.full.list

# Now generate the files
#   ok.list
#   special.list
#   change.list
#   change-notfound.list
sh ${MY_DIR}/FIND_RPM_UPDATES.sh tmpdir `cat tmpdir/rpms.full.list`
wc -l tmpdir/change.list tmpdir/change-notfound.list tmpdir/ok.list tmpdir/special.list

# Now create an updated verison of the Makefile
sh ${MY_DIR}/UPDATE_Makefile.sh tmpdir ${FROM} tmpdir/change.list tmpdir/change-notfound.list

ls -l tmpdir/Makefile.new

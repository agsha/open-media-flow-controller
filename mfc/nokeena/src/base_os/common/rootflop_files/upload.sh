:
# This script uploads an update script and runs it.
# The update script may upload other files as well.
ARG=build-latest
[ "_${1}" != "_" ] && ARG=${1}
URL=http://172.19.172.52/mfg/${ARG}/update.sh
echo wget ${URL}
wget ${URL}
if [ -f update.sh ] ; then
    sh ./update.sh ${ARG} ${URL}
else
    echo Not found ${URL}
fi

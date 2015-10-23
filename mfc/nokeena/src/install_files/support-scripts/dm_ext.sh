#! /bin/bash


RMDIR="rm -rf"
MKDIR=mkdir
MKDIR_ARGS="-p"
TAR=tar
TAR_CREATE_ARGS="czf"


tmp_dir=${1}

function dump_contents()
{
	prefix=$1
	dm_mount=$2

	find ${dm_mount} -type d | grep -v "lost+found" > ${prefix}/tmp_file

	cat ${prefix}/tmp_file | \
	while read line
	do
		mkdir -p ${prefix}/${line}
	done
	rm -f ${prefix}/tmp_file
	

	find ${dm_mount} -type f | grep -e "freeblks" -e "container" -e "attribute" > ${prefix}/tmp_file

	cat ${prefix}/tmp_file | \
	while read line
	do
		set ${line}
		dd if=${line} of=${prefix}/${line} iflag=direct status=noxfer > /dev/null 2>&1
	done
	rm -f ${prefix}/tmp_file
}


if [ ! -d ${tmp_dir} ]
then
    ${MKDIR} ${MKDIR_ARGS} ${tmp_dir}
fi


dirs=`ls -d /nkn/mnt/*`
for dir in $dirs
do
	dump_contents ${tmp_dir} ${dir}
done


# this file name is always fixed. If you change it here
# then change it in tech-support.cfg as well, 
# and vice-versa
${TAR} ${TAR_CREATE_ARGS} /var/log/dc_container.tgz ${tmp_dir}
${RMDIR} ${tmp_dir}


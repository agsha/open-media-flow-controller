#!/bin/sh

#####################################################################
#####################################################################
##
## mkchroot.sh - set up a chroot jail.
##
## This script is written to work for Red Hat 8/9 systems, but may work on
## other systems.  Or, it may not...  In fact, it may not work at all.  Use at
## your own risk.  :)
##
fail() {

	echo "`basename $0`: fatal error" >&2
	echo "$1" >&2
	exit $2
}

#####################################################################
#
# Initialize - handle command-line args, and set up variables and such.
# 
# $1 is the directory to make the root of the chroot jail (required)
# $2, if given, is the user who should own the jail (optional)
# $3, if given,  is the permissions on the directory (optional) 
#

if [ -z "$1" ]; then
	echo "`basename $0`: error parsing command line" >&2
	echo "  You must specify a directory to use as the chroot jail." >&2
	exit 1
fi

jail_dir="$1"

if [ -n "$2" ]; then
	owner="$2"
fi

if [ -n "$3" ]; then
	perms="$3"
fi


#####################################################################
#
# build the jail
#
# now make the directory

if [ ! -d "$jail_dir" ]; then
	echo "Creating root jail directory."
	mkdir -p "$jail_dir"
	if [ $? -ne 0 ]; then
		echo "  `basename $0`: error creating jail directory." >&2
		echo "Check permissions on parent directory." >&2
		exit 2
	fi
fi

#TODO:fix this issue
#irrespective of the  jail_dir availability the below part of script is executed  which is redundant
if [ -n "$owner" -a `whoami` = "admin" ]; then
	echo "Setting owner of jail."
	chown "$owner" "$jail_dir"
	if [ $? -ne 0 ]; then
		echo "  `basename $0`: error changing owner of jail directory." >&2
		exit 3
	 fi
else
	echo -e "NOT changing owner of root jail. \c"
	if [ `whoami` != "admin" ]; then
		echo "You are not root."
	else
		echo
	fi
fi

if [ -n "$owner" -a `whoami` = "admin" ]; then
	echo "Setting permissions of jail."
	chmod "$perms" "$jail_dir"
	if [ $? -ne 0 ]; then
		echo "  `basename $0`: error changing perms of jail directory." >&2
		exit 3
	 fi
else
	echo -e "NOT changing perms of root jail. \c"
	if [ `whoami` != "admin" ]; then
		echo "You are not root."
	else
		echo
	fi
fi

# copy SSH files

scp_path="/usr/bin/scp"
sftp_server_path="/usr/libexec/openssh/sftp-server"
rssh_path="/usr/bin/rssh"
chroot_helper_path="/usr/libexec/rssh_chroot_helper"

for jail_path in `dirname "$jail_dir$scp_path"` `dirname "$jail_dir$sftp_server_path"` `dirname "$jail_dir$chroot_helper_path"`; do

	echo "setting up $jail_path"

	if [ ! -d "$jail_path" ]; then
		mkdir -p "$jail_path" || \
			fail "Error creating $jail_path. Exiting." 4
	fi

done

cp "$scp_path" "$jail_dir$scp_path" || \
	fail "Error copying $scp_path. Exiting." 5
cp "$sftp_server_path" "$jail_dir$sftp_server_path" || \
	fail "Error copying $sftp_server_path. Exiting." 5
cp "$rssh_path" "$jail_dir$rssh_path" || \
	fail "Error copying $rssh_path. Exiting." 5
cp "$chroot_helper_path" "$jail_dir$chroot_helper_path" || \
	fail "Error copying $chroot_helper_path. Exiting." 5


#####################################################################
#
# identify and copy libraries needed in the jail
#

for prog in $scp_path $sftp_server_path $rssh_path $chroot_helper_path; do
	echo "Copying libraries for $prog."
	libs=`ldd $prog | grep -v vdso | grep -v ld-linux | tr -s ' ' | cut -d' ' -f3`
	for lib in $libs; do
		mkdir -p "$jail_dir$(dirname $lib)"
		echo -e "\t$lib"
		cp "$lib" "$jail_dir$lib"
	done
done

echo "copying name service resolution libraries..."
tar -cf - /lib/libnss_files* | tar -C "$jail_dir" -xvf - |sed 's/^/\t/'

echo "copying ld-linux..."
cp /lib64/ld-linux-x86-64.so.2 $jail_dir/lib64/.

#####################################################################
#
# copy config files for the dynamic linker, nsswitch.conf, and the passwd file
#

echo "Setting up /etc in the chroot jail"
mkdir -p "$jail_dir/etc"
#cp /etc/nsswitch.conf "$jail_dir/etc/"
ln -s /etc/nssswitch.conf $jail_dir/etc/
#cp /etc/passwd "$jail_dir/etc/"
ln -s /etc/passwd $jail_dir/etc/
cp -R /etc/ld.* "$jail_dir/etc/"

echo "Setting up common home..."
mkdir -p $jail_dir/home/LogExport
echo "linking storage to common home..."
chown -R LogTransferUser $jail_dir/home/LogExport
if [ $? -ne 0 ]; then
                echo "   error changing owner of $jail_dir/home/LogExport." >&2

fi

echo -e "Chroot jail configuration completed."
echo -e "\nNOTE: if you are not using the passwd file for authentication,"
echo -e "you may need to copy some of the /lib/libnss_* files into the jail.\n"


#####################################################################
#
# set up /dev/log
#

mkdir -p "$jail_dir/dev"

#after all changes move back to the readonly mode

echo -e "NOTE: you must MANUALLY edit your syslog rc script to start syslogd"
echo -e "with appropriate options to log to $jail_dir/dev/log.  In most cases,"
echo -e "you will need to start syslog as:\n"
echo -e "   /sbin/syslogd -a $jail_dir/dev/log\n"

echo -e "NOTE: we make no guarantee that ANY of this will work for you... \c"
echo -e "if it\ndoesn't, you're on your own.  Sorry!\n"



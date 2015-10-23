# Notes
#

# There can only be includes or excludes for a package, not both

@includes_bzip2=(
                '/usr/bin/bunzip2',
                '/usr/bin/bzcat',
                '/usr/bin/bzip2'
                );

@includes_coreutils=('/usr/bin/sha1sum', '/usr/bin/sha256sum');

@includes_device_mapper=('/lib*/lib*.so*');

@includes_e2fsprogs=(
                     '/sbin/mke2fs',
                     '/sbin/e2label',
                     '/etc/mke2fs.conf'
                     );

@strips_e2fsprogs_libs=('/lib*/lib*.so.*.*');


# Note that we are _not_ using /lib*/ below since cpio's extract lets
# that '*' match subdirectories, which we do not want.
@includes_glibc=(
                 '/lib64/ld-[0-9]*.[0-9]*.so',
                 '/lib64/ld-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/ld-linux*.so.[0-9]*',
                 '/lib64/libc-[0-9]*.[0-9]*.so',
                 '/lib64/libc-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libc.so.[0-9]*',
                 '/lib64/libcrypt-[0-9]*.[0-9]*.so',
                 '/lib64/libcrypt-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libcrypt.so.[0-9]*',
                 '/lib64/libdl-[0-9]*.[0-9]*.so',
                 '/lib64/libdl-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libdl.so.[0-9]*',
                 '/lib64/libnss_dns-[0-9]*.[0-9]*.so',
                 '/lib64/libnss_dns-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libnss_dns.so.[0-9]*',
                 '/lib64/libnss_files-[0-9]*.[0-9]*.so',
                 '/lib64/libnss_files-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libnss_files.so.[0-9]*',
                 '/lib64/libresolv-[0-9]*.[0-9]*.so',
                 '/lib64/libresolv-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libresolv.so.[0-9]*',
                 '/lib64/librt-[0-9]*.[0-9]*.so',
                 '/lib64/librt-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/librt.so.[0-9]*',
                 '/lib64/libnsl-[0-9]*.[0-9]*.so',
                 '/lib64/libnsl-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libnsl.so.[0-9]*',
                 '/lib64/libpthread-[0-9]*.[0-9]*.so',
                 '/lib64/libpthread-[0-9]*.[0-9]*.[0-9]*.so',
                 '/lib64/libpthread.so.[0-9]*'
                 );

@strips_glibc=('/lib*/lib*.so');

if ("$BUILD_PROD_FEATURE_IMAGE_SECURITY" eq "1") {
    @includes_gnupg2=('/usr/bin/gpg', '/usr/bin/gpg2');
}

@includes_grub=(
                '/sbin/grub',
                '/sbin/grub-install',
                '/usr/share/grub/*-redhat/*',
               );

@includes_tar=(
               '/bin/tar'
               );

@includes_unzip=(
               '/usr/bin/unzip'
               );


@includes_util_linux_ng=(
                      '/sbin/sfdisk',
                      '/sbin/mkswap'
                      );


#!/bin/sh
#set -x

# create a Junos package on Linux

# This script is only called by "mfc-mk.x86.sh" and "ubuntu-mk.sh"
# Two parameters
# 1: Input directory, normally the tftpboot dir
#     (elilo.conf, initrd.img, vmlinuz, version.txt, pkgfilename.txt, root filesystem file ...)
#     The version.txt file specifies the package version.
#     The pkgfilename.txt specifies the package filename.
#     The root filesystem file is mfcrootfs.img or filesystem.rootfs.tgz
# 2: Output directory where the package tgz file is written.
#     If this directory does not exist it will be created.
# 3: Optional, signing parameter
#     Defaults to "default".
# 4: Optional, specify type of package being signed.  (MFC,UbuntuKVM)
#     Defaults to "MFC".

# Steps:
# 1: Copy the input files to a temporary directory.
# 2: Create manifest and symlinks files. 
# 3: Create an ISO filesystem file.
# 4: Create the pkg_create input files (+DESC, +INSTALL, ...)
# 5: Use pkg_create the create the basic package.
# 6: Use pkg_create the create the 'signed' package of the basic package.
# 7: Copy the package files to the output directory.

# Note that the basic package has signed files.

# Contents of the basic package <pkgname>.tgz:
#    +COMMENT
#    +CONTENTS
#    +DEINSTALL
#    +DESC
#    +INSTALL
#    +REQUIRE
#    mfc-12.3.8-3_30400_402.symlinks
#    packages/<pkgname>
#    packages/<pkgname>.certs
#    packages/<pkgname>.sha1
#    packages/<pkgname>.sig
#
# Second package type (<pkgname>-signed.tgz) contents:
#    +COMMENT
#    +CONTENTS
#    +DESC
#    +INSTALL
#    certs.pem
#    <pkgname>.tgz      # This is the basic package type tgz file.
#    <pkgname>.tgz.sha1
#    <pkgname>.tgz.sig
# 
# This '-signed' package type is what we ship.
# The '-signed' package validates whether the package being installed is
# valid for the system and the existing software, and checks the validity
# of the basic package itself.
# The basic package (without the '-signed' suffix) does not impose these
# constraints while installing.
#
# See Understanding_JunOS_Packages.html for more info.


# Set DUMMY_SIGNING=yes to use fake signatures and certficate for
# standalone opensource MFC build
##################
DUMMY_SIGNING=no
##################

DUMMY_SIGNATURE=`pwd`/dummy-signature
DUMMY_CERTIFICATE=`pwd`/dummy-certficate.crt

MYNAME=`basename ${0}`

usage () {
   echo ${MYNAME} input-dir output-dir
}

if [ ${#} -lt 2 -o ${#} -gt 4 ]; then
    usage
    exit 2
fi

PKGINDIR=${1}
RELEASEDIR=${2}
SIGN_OPT=${3}
PKGTYPE=${4}
[ -z "${SIGN_OPT}" ] && SIGN_OPT=default
[ -z "${PKGTYPE}"  ] && PKGTYPE=MFC

# Validate the input dir
if [ ! -d ${PKGINDIR} ]; then
   echo ${PKGINDIR} is not a directory
   echo ${0} dir
   exit 2
fi

# -------  CERT and Signing stuff -------- #
SIGNING_APP=/volume/buildtools/bin/sign.py
SIGNING_HASH=sha1

# ---------------------------------------------------------
# MFC signing (dev) ---------------------------------------
# Port 20006
# CN=PackageDevelopmentMFC_2014
# Description: MFC Development 2014
# All build servers permitted
# Responsible parties:
#     Director: Kumar Narayanan <kumarnar@juniper.net>
#     Buildmasters: David Peet<dpeet@juniper.net>,
# ---------------------------------------------------------
# MFC signing (production) --------------------------------
# Port 20007
# CN=PackageProductionMFC_2014
# Description: "AS-MLC OS Image" (aka "MFC") Production 2014
# Build server: cmbu-esx18-build01.englab.juniper.net
# Responsible parties:
#     Director: Kumar Narayanan <kumarnar@juniper.net>
#     Buildmasters: David Peet<dpeet@juniper.net>,
#                   Vinod Nair<vinodnair@juniper.net>
# ---------------------------------------------------------

# ---------------------------------------------------------
# AS-MLC Ubuntu KVM signing (dev) -------------------------
# Port 20010
# CN=PackageDevelopmentUbuntuKVM_2015
# Description: AS-MLC Ubuntu KVM OS Image - Development 2015
# All build servers permitted
# Responsible parties:
#     Director: Kumar Narayanan <kumarnar@juniper.net>
#     Buildmasters: David Peet<dpeet@juniper.net>
# ---------------------------------------------------------
# AS-MLC Ubuntu KVM signing (production) ------------------
# Port 20011
# CN=PackageProductionUbuntuKVM_2015
# Description: AS-MLC Ubuntu KVM OS Image - Production 2015
# Build server: cmbu-esx18-build01.englab.juniper.net
# Responsible parties:
#      Director: Kumar Narayanan <kumarnar@juniper.net>
#      Buildmasters: David Peet<dpeet@juniper.net>,
#                    Vinod Nair<vinodnair@juniper.net>
# ---------------------------------------------------------

# Production signing can only be done on cmbu-esx18-build01.englab.juniper.net
H=`hostname -f`
if [ "${H}" = "cmbu-esx18-build01.englab.juniper.net" ] ; then
  PROD_OK=yes
else
  PROD_OK=no
fi

Set_Signing_Hostport()
{
  if  [ ${SIGN_OPT} = "default" ] ; then
    if [ ${PROD_OK} = "no" ] ; then
      SIGN_OPT=dev
    else
      SIGN_OPT=prod
    fi
  fi
  if  [ "${PKGTYPE}" = "MFC" ] ; then
    if  [ "${SIGN_OPT}" = "prod" ] ; then
      SIGNING_HOSTPORT=svl-junos-signer.juniper.net:20007
    else
      SIGNING_HOSTPORT=svl-junos-signer.juniper.net:20006
    fi
  elif  [ "${PKGTYPE}" = "UbuntuKVM" ] ; then
    if  [ "${SIGN_OPT}" = "prod" ] ; then
      SIGNING_HOSTPORT=svl-junos-signer.juniper.net:20011
    else
      SIGNING_HOSTPORT=svl-junos-signer.juniper.net:20010
    fi
  else
    echo "Unknown package type:${PKGTYPE}"
    exit 1
  fi
}

case "${SIGN_OPT}" in
*:*) SIGNING_HOSTPORT=${SIGN_OPT} ;;
*)   Set_Signing_Hostport ;;
esac

case `echo ${SIGNING_HOSTPORT} | cut -f2 -d:` in
20007|20011)
   if [ "${PROD_OK}" = "no" ] ; then
     echo Error, cannot do production signing on this machine
     exit 1
   fi
   PRODUCTION_SIGNING=yes ;;
*) PRODUCTION_SIGNING=no ;;
esac
echo PRODUCTION_SIGNING=${PRODUCTION_SIGNING}


# Note: pkgvars requires these to be set
#    TMP_DIR
#    JUNOS_PACKAGING_TOP_DIR
if [ -z "${TMP_DIR}" ] ; then
  echo Error, TMP_DIR must be set
  exit 1
fi
if [ ! -d "${TMP_DIR}" ] ; then
  echo Error, TMP_DIR=${TMP_DIR} is not a directory
  exit 1
fi
if [ -z "${JUNOS_PACKAGING_TOP_DIR}" ] ; then
  echo Error, JUNOS_PACKAGING_TOP_DIR must be set
  exit 1
fi
if [ -z "${JUNOS_PACKAGING_TOP_DIR}" ] ; then
  echo Error, JUNOS_PACKAGING_TOP_DIR=${JUNOS_PACKAGING_TOP_DIR} is not a directory
  exit 1
fi
export TMP_DIR
export JUNOS_PACKAGING_TOP_DIR
# Note: These need to be set in the environment:
# JUNOS_RELMAJOR and JUNOS_RELMINOR specify the minimum JunOS version that we can install on.
if [ -z "${JUNOS_RELMAJOR}" ] ; then
  echo Error, JUNOS_RELMAJOR must be set
  exit 1
fi
if [ -z "${JUNOS_RELMINOR}" ] ; then
  echo Error, JUNOS_RELMINOR must be set
  exit 1
fi
export JUNOS_RELMAJOR
export JUNOS_RELMINOR
PKG_REL_FOR_ISO="${JUNOS_RELMAJOR}.${JUNOS_RELMINOR}"
echo JUNOS_RELMAJOR=${JUNOS_RELMAJOR}
echo JUNOS_RELMINOR=${JUNOS_RELMINOR}
echo PKG_REL_FOR_ISO=${PKG_REL_FOR_ISO}


PKGVARTOPDIR=${JUNOS_PACKAGING_TOP_DIR}/host-utils/scripts
PKGNAME=`cat ${PKGINDIR}/version.txt`
PKGFILE=`cat ${PKGINDIR}/pkgfilename.txt`
if [ "${PKGNAME}" = "${PKGFILE}" ] ; then
  echo Error, version.txt is the same as pkgfilename.txt: ${PKGNAME}
  echo Expect to have two dashes before the package version numbers.
  exit 1
fi
case ${PKGNAME} in
*_DEV)
  if [ "${PRODUCTION_SIGNING}" = "yes" ] ; then
    echo Error, will not sign _DEV package name with production certs.
    exit 1
  fi
  ;;
*)
  if [ "${PRODUCTION_SIGNING}" = "no" ] ; then
    # When not using production signing, change the package name to end with "_DEV".
    echo Append _DEV to package name
    cat ${PKGINDIR}/elilo.conf | sed -e "s/${PKGNAME}/${PKGNAME}_DEV/g" > ${PKGINDIR}/tmpfile
    diff ${PKGINDIR}/elilo.conf ${PKGINDIR}/tmpfile
    cat ${PKGINDIR}/tmpfile > ${PKGINDIR}/elilo.conf
    rm -f ${PKGINDIR}/tmpfile
    echo ${PKGNAME}_DEV > ${PKGINDIR}/version.txt
    echo ${PKGFILE}_DEV > ${PKGINDIR}/pkgfilename.txt
    PKGNAME=`cat ${PKGINDIR}/version.txt`
    PKGFILE=`cat ${PKGINDIR}/pkgfilename.txt`
  fi
  ;;
esac
echo PKGNAME=${PKGNAME}
echo PKGFILE=${PKGFILE}
echo PKG_REL_FOR_ISO=${PKG_REL_FOR_ISO}
MY_PKG_VERSION=`echo ${PKGNAME} | cut -f2 -d-`
PRODUCT_NAME=`echo ${PKGNAME} | cut -f1 -d-`
PRODUCT_TAG=`echo ${PRODUCT_NAME} | awk '{print toupper($0)}'`


SYMLINKDIR=var/tftpboot/${PKGNAME}
export PKGNAME PKG SYMLINKDIR

echo +++ . ${PKGVARTOPDIR}/pkgvars +++
# NOTE: pkgvars sets:
#   SIGKEYDIR=/etc/jcerts or /tmp/jcerts
#   PKGBUILDDIR=${TMP_DIR}/junos
#   PKGSRCTOPDIR=${JUNOS_PACKAGING_TOP_DIR}
#   PKGBLDOBJDIR=${PKGBUILDDIR}/package-scripts
#   PKGBLDSRCDIR=${PKGBUILDDIR}/release/rootdir
#   HOST_OBJTOP_JUNOS=${JUNOS_PACKAGING_TOP_DIR}/buildroot
#   md5=
#   sha1=
echo SIGKEYDIR=${SIGKEYDIR}
echo PKGBUILDDIR=${PKGBUILDDIR}
echo PKGSRCTOPDIR=${PKGSRCTOPDIR}
echo PKGBLDOBJDIR=${PKGBLDOBJDIR}
echo PKGBLDSRCDIR=${PKGBLDSRCDIR}
echo HOST_OBJTOP_JUNOS=${HOST_OBJTOP_JUNOS}
. ${PKGVARTOPDIR}/pkgvars
echo ++++++

PKGOBJ_UNSIGNED=${PKGBLDOBJDIR}/${PKGNAME}
PKGOBJ_SIGNED=${PKGBLDOBJDIR}/${PKGNAME}-signed
export PKGOBJ_UNSIGNED PKGOBJ_SIGNED

# gensig uses SIGKEY, /etc/jcerts/*.pem
export PKGFILE PKGVARTOPDIR MY_PKG_VERSION

echo +++ . ${PKGVARTOPDIR}/setup.sh +++
. ${PKGVARTOPDIR}/setup.sh
echo PKG_CREATE=${PKG_CREATE}
echo ++++++

pkgcert=ProductionMFC
[ -z "${OFFICIAL}" ] && pkgcert=DevelopmentMFC
#SIGKEY="${SIGKEYDIR}/Package${pkgcert}_${JUNOS_RELMAJOR}_${JUNOS_RELMINOR}_0_key.pem"

srcdir=boot
pkgtmpdir=${PKGBUILDDIR}/tmp/${PKGNAME}-$$
pkgscripts=${PKGSRCTOPDIR}/package-scripts
hostutils=${PKGSRCTOPDIR}/host-utils

hostscripts=${hostutils}/scripts
certsdir=${SIGKEYDIR}
CERTS_CHAIN_FILE=${PKGBLDSRCDIR}/certs.pem

gensig=${hostscripts}/gensig
makeiso=${hostscripts}/make-iso.sh
mkmanifestsymlinks=${hostscripts}/mk-mfc-manifest-symlinks.sh

# create dirs as needed
[ -d ${PKGOBJ_UNSIGNED} ] || mkdir -p ${PKGOBJ_UNSIGNED}
[ -d ${PKGOBJ_SIGNED} ] || mkdir -p ${PKGOBJ_SIGNED}
[ -d ${RELEASEDIR} ] || mkdir -p ${RELEASEDIR}
[ -d ${pkgtmpdir} ] || mkdir -p ${pkgtmpdir}/{pkg,${srcdir}}
[ -d ${PKGBLDSRCDIR}/packages ] || mkdir -p ${PKGBLDSRCDIR}/packages


# find the type f files
files=`find ${PKGINDIR} -type f`
echo "${MYNAME}: Step 1: copying input files to tmpdir"
for file in ${files}; do
#    echo `basename ${file}`
    name=`basename ${file}`
    cp -p ${PKGINDIR}/${name} ${pkgtmpdir}/${srcdir}/${name}
done


echo Configuring for Signing Server use:  ${SIGNING_HOSTPORT}
# Note: sign.py requires python2.7 
SIGNING_CMD="python2.7 ${SIGNING_APP} -u ${SIGNING_HOSTPORT} -h ${SIGNING_HASH}"
# Get the public certificate chain for the key connected with
# ${SIGNING_HOSTPORT} all the way up to the root CA certificate.
# Put the cert chain into the the file ${CERTS_CHAIN_FILE}.
rm -f ${CERTS_CHAIN_FILE}
if [ ${DUMMY_SIGNING} = "no" ] ; then
  python2.7 ${SIGNING_APP} -u ${SIGNING_HOSTPORT} -C ${CERTS_CHAIN_FILE}
else
  cp ${DUMMY_CERTIFICATE} ${CERTS_CHAIN_FILE}
fi
WC=`cat ${CERTS_CHAIN_FILE} | wc -l`
if [ ${WC} -eq 0 ] ; then
  echo ERROR, unable to get public cert chain from server.
  exit 1
fi
ls -l ${CERTS_CHAIN_FILE}

Sign_This()
{
  THIS=${1}
  rm -f ${THIS}.sig
  echo "${MYNAME} running ${SIGNING_CMD} -o ${THIS}.sig ${THIS} ............................................"
  if [ ${DUMMY_SIGNING} = "no" ] ; then
    ${SIGNING_CMD} -o ${THIS}.sig ${THIS}
    RV=${?}
    echo RV=${RV}
  else 
    cp ${DUMMY_SIGNATURE} ${THIS}.sig
    echo RV=0
  fi
  WC=`cat ${THIS}.sig | wc -l`
  if [ ${WC} -eq 0 ] ; then
    echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    echo Failed to create ${THIS}.sig ${RV}
    echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    exit 1
  fi
}

# create the manifest, manifest.{certs|sha1|sig} and ${PKGNAME}.symlinks
echo "${MYNAME}: Step 2: building pkg/manifest, .certs, .sha1, .sig and ${PKGNAME}.symlinks"
cd ${pkgtmpdir}/pkg
echo In `pwd` +++++++++++++++++++++
echo PKGVARTOPDIR=${PKGVARTOPDIR}
echo ${mkmanifestsymlinks} ${pkgtmpdir} ${srcdir} manifest ${PKGOBJ_UNSIGNED}/${PKGNAME}.symlinks
${mkmanifestsymlinks} ${pkgtmpdir} ${srcdir} manifest ${PKGOBJ_UNSIGNED}/${PKGNAME}.symlinks
echo back from ${mkmanifestsymlinks}

echo ${CERTS_CHAIN_FILE} copied to manifest.certs
cp ${CERTS_CHAIN_FILE} manifest.certs

${sha1} manifest > manifest.sha1
Sign_This manifest

# create the ${pkgtmpdir} ${PKGNAME}, ${PKGNAME}.{certs|sha1|sig}
echo "${MYNAME}: Step 3: building ISO FS ${PKGFILE}, .certs, .sha1 and .sig"
cd ${pkgtmpdir}
echo ${makeiso} ${PKGBLDSRCDIR}/packages ${PKGNAME} ${PKG_REL_FOR_ISO} ${PKGFILE} ${pkgtmpdir}
${makeiso} ${PKGBLDSRCDIR}/packages ${PKGNAME} ${PKG_REL_FOR_ISO} ${PKGFILE} ${pkgtmpdir}
cd ${PKGBLDSRCDIR}/packages
echo ${CERTS_CHAIN_FILE} copied to ${PKGFILE}.certs
cp ${CERTS_CHAIN_FILE} ${PKGFILE}.certs

${sha1} ${PKGFILE} > ${PKGFILE}.sha1
Sign_This ${PKGFILE}

Cat_Sed()
{
  /bin/cat ${*} | sed \
    -e "s;%BUILDSYS_RELEASE%;7.1-RELEASE-JNPR-4;g" \
    -e "s;%CERTS_OBJDIR%;${PKGBLDSRCDIR};g" \
    -e "s;%COPYRIGHT%;1996-2014;g" \
    -e "s;%MACHINE%;i386;g" \
    -e "s;%RELEASE%;${MY_PKG_VERSION};g" \
    -e "s;%RELEASE_MAJOR%;${JUNOS_RELMAJOR};g" \
    -e "s;%RELEASE_MINOR%;${JUNOS_RELMINOR};g" \
    -e "s;%RELEASE_CATEGORY%;${CATEGORY};g" \
    -e "s;%SDK_SUBPKGS%;;g" \
    -e "s;%JCRYPTO_PKG%;;g" \
    -e "s;%JKERNEL_PKG%;;g" \
    -e "s;%JBUNDLE_PKG%;;g" \
    -e "s;%JKERNEL_OBJDIR%;;g" \
    -e "s;%MACHINE_COMPAT%;i386;g" \
    -e "s;%JUNOS_SDK_VERSION_MAJOR%;5;g" \
    -e "s;%JUNOS_SDK_VERSION_MINOR%;3;g" \
    -e "s;%JUNOS_SDK_COMPAT%;11.1;g" \
    -e "s;%SD_VERSION%;2;g" \
    -e "s;%CHASSISD_PFE_VERSION%;98.1.2;g" \
    -e "s;%CONFIG_DBM_VERSION%;27.1;g" \
    -e "s;%CONFIG_DDL_VERSION%;1;g" \
    -e "s;%EJN_COPYRIGHT%;2000-2011;g" \
    -e "s;%MC_VERSION%;73;g" \
    -e "s;%PKG%;${PKG};g" \
    -e "s;%PKGFILE%;${PKGFILE};g" \
    -e "s;%PKGNAME%;${PKGNAME};g" \
    -e "s;%RTSOCK_IF_VERSION%;98.9;g" \
    -e "s;%PKG_SUFFIX%;-ex;g" \
    -e "s;%OBJDIR%;${OBJDIR};g" \
    -e "s;%PACKAGE%;${PKGFILE};g"  \
    -e "s;%SRCDIR%;${PKGBLDSRCDIR};g"  \
    -e "s;%SYMLINKDIR%;${SYMLINKDIR};g" \
    -e "s;%PRODUCTNAME%;${PRODUCT_NAME};g" \
    -e "s;%PRODUCTTAG%;${PRODUCT_TAG};g" \

}

Generate_pkg_create_input_files()
{

  signed_pkg_scripts_dir=${PKGSRCTOPDIR}/package-scripts/signed
  generic_pkg_scripts_dir=${PKGSRCTOPDIR}/package-scripts/generic
  basic_pkg_scripts_dir=${PKGSRCTOPDIR}/package-scripts/basic

  # Use sed to replace variables with values when creating the '+' files.
  # Create for signed packaging: +COMMENT, +DESC, +INSTALL, +PLIST
  # Create for basic packaging: +COMMENT, +DESC, +REQUIRE, +DEINSTALL, +INSTALl, +PLIST

  ############################################
  # First do the files for the signed package.
  export OBJDIR=${PKGOBJ_SIGNED}
  Cat_Sed ${signed_pkg_scripts_dir}/COMMENT        > ${PKGOBJ_SIGNED}/+COMMENT
  Cat_Sed ${signed_pkg_scripts_dir}/DESC           > ${PKGOBJ_SIGNED}/+DESC
  Cat_Sed ${generic_pkg_scripts_dir}/script-header \
          ${generic_pkg_scripts_dir}/script-funcs  \
          ${signed_pkg_scripts_dir}/INSTALL        > ${PKGOBJ_SIGNED}/+INSTALL
  Cat_Sed ${signed_pkg_scripts_dir}/PLIST          > ${PKGOBJ_SIGNED}/+PLIST

  ############################################
  # Now do the files for the basic package.
  export OBJDIR=${PKGOBJ_UNSIGNED}
  Cat_Sed ${basic_pkg_scripts_dir}/COMMENT      > ${PKGOBJ_UNSIGNED}/+COMMENT
  Cat_Sed ${basic_pkg_scripts_dir}/DESC         > ${PKGOBJ_UNSIGNED}/+DESC
  Cat_Sed ${generic_pkg_scripts_dir}/script-header \
          ${generic_pkg_scripts_dir}/script-funcs  \
          ${basic_pkg_scripts_dir}/my-script-header \
          ${basic_pkg_scripts_dir}/REQUIRE      > ${PKGOBJ_UNSIGNED}/+REQUIRE
  Cat_Sed ${generic_pkg_scripts_dir}/script-header \
          ${generic_pkg_scripts_dir}/script-funcs  \
          ${basic_pkg_scripts_dir}/my-script-header \
          ${basic_pkg_scripts_dir}/DEINSTALL    > ${PKGOBJ_UNSIGNED}/+DEINSTALL
  Cat_Sed ${generic_pkg_scripts_dir}/script-header \
          ${generic_pkg_scripts_dir}/script-funcs  \
          ${basic_pkg_scripts_dir}/my-script-header \
          ${basic_pkg_scripts_dir}/INSTALL      > ${PKGOBJ_UNSIGNED}/+INSTALL
  Cat_Sed ${basic_pkg_scripts_dir}/PLIST        > ${PKGOBJ_UNSIGNED}/+PLIST
}
# Generate the pkg_create input files
echo "${MYNAME}:Step 4: Creating the pkg_create input files"
Generate_pkg_create_input_files

# create ${PKGFILE}.tgz, ${PKGFILE}.tgz.{certs|sha1|sig}
echo "${MYNAME}: Step 5: building package ${PKGFILE}.tgz"
${PKG_CREATE} -f ${PKGOBJ_UNSIGNED}/+PLIST   -c ${PKGOBJ_UNSIGNED}/+COMMENT -d ${PKGOBJ_UNSIGNED}/+DESC \
              -i ${PKGOBJ_UNSIGNED}/+INSTALL -k ${PKGOBJ_UNSIGNED}/+DEINSTALL \
              -r ${PKGOBJ_UNSIGNED}/+REQUIRE -o JUNOS_PKG ${PKGBLDSRCDIR}/${PKGFILE}.tgz
echo Back from ${PKG_CREATE}
cd ${PKGBLDSRCDIR}
echo "${MYNAME} building .sha1 and tgz.sig"
${md5}  ${PKGFILE}.tgz > ${PKGFILE}.tgz.md5
${sha1} ${PKGFILE}.tgz > ${PKGFILE}.tgz.sha1
Sign_This ${PKGFILE}.tgz

# Create ${PKGFILE}-signed.tgz
echo "${MYNAME}: Step 6: building package ${PKGFILE}-signed.tgz"
${PKG_CREATE} -f ${PKGOBJ_SIGNED}/+PLIST -c ${PKGOBJ_SIGNED}/+COMMENT -d ${PKGOBJ_SIGNED}/+DESC \
              -i ${PKGOBJ_SIGNED}/+INSTALL ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz
echo Back from ${PKG_CREATE}
#${md5}  ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz > ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz.md5
${sha1} ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz > ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz.sha1

# Move package to ship
echo "${MYNAME}: Step 7:  moving ${PKGFILE}-signed.tgz to ${RELEASEDIR}"
/bin/mv ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz ${RELEASEDIR}/${PKGFILE}-signed.tgz
#/bin/mv ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz.md5 ${RELEASEDIR}/${PKGFILE}-signed.tgz.md5
/bin/mv ${PKGBLDSRCDIR}/${PKGFILE}-signed.tgz.sha1 ${RELEASEDIR}/${PKGFILE}-signed.tgz.sha1

/bin/rm -rf ${pkgtmpdir}
ls ${RELEASEDIR}/${PKGFILE}*

exit 0

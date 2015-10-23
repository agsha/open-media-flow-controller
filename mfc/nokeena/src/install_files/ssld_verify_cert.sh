#! /bin/bash
#
#       File : pe_com.sh
#
#       Modified by : Thilak Raj S
#       Created Date : 21st June, 2010
#
#       Copyright notice ( © 2010 Juniper Networks Inc., All rights reserved)
#

# Local Environment Variables
NKN_CERT=/config/nkn/cert/
NKN_KEY=/config/nkn/key/
NKN_CSR=/config/nkn/csr/
NKN_CA=/config/nkn/ca/
function usage()
{
	echo
		echo "Usage: ssld_verify_cert.sh  <filename> <type>  [passphrase]"
		echo
		exit 1
}

#Get the params
function parse_params()
{
    FILE_NAME=$1;
    shift;
    TYPE=$1;
    shift;
    PASS_PHRASE=$1;
    shift;

#logger "FILE_NAME $FILE_NAME"
#logger "PASSPHRASE $PASS_PHRASE"
#logger "TYPE $TYPE"
}

function verify_cert_key()
{
	TMP_FILE_NAME=${FILE_NAME}".tmp"
	MD5HASH_CERT=`openssl x509 -noout -modulus -in "$NKN_CERT""$TMP_FILE_NAME" | openssl md5`
	MD5HASH_KEY=`openssl rsa -passin pass:"$PASS_PHRASE" -noout -modulus -in "$NKN_CERT""$TMP_FILE_NAME" | openssl md5`
#logger "CERT $MD5HASH_CERT"
#logger "KEY $MD5HASH_KEY"

		if [ "$MD5HASH_CERT" != "$MD5HASH_KEY" ]; then
			echo "In-Valid"
#logger "invalid"
			rm -f $NKN_CERT$TMP_FILE_NAME
		else
			echo "Valid"
#logger "valid"
			#move the tmp file to actual file name if valid
			mv $NKN_CERT$TMP_FILE_NAME $NKN_CERT$FILE_NAME
		fi
}

#Main Logic starts from here
if [ $# -lt 1 ]; then
	echo usage;
fi

# Now get the parameters
parse_params $*;
if [ "$TYPE" == "1" ]; then
#	verify_cert_key;
TMP_FILE_NAME=${FILE_NAME}".tmp"
mv $NKN_CERT$TMP_FILE_NAME $NKN_CERT$FILE_NAME
	echo "Valid"
elif [ "$TYPE" == "2" ]; then
TMP_FILE_NAME=${FILE_NAME}".tmp"
#logger "valid for type2"
mv $NKN_KEY$TMP_FILE_NAME $NKN_KEY$FILE_NAME
	echo "Valid"
elif [ "$TYPE" == "3" ]; then
TMP_FILE_NAME=${FILE_NAME}".tmp"
#logger "valid for type2"
mv $NKN_CSR$TMP_FILE_NAME $NKN_CSR$FILE_NAME
	echo "Valid"
elif [ "$TYPE" == "4" ]; then
TMP_FILE_NAME=${FILE_NAME}".tmp"
#logger "valid for type4"
mv $NKN_CA$TMP_FILE_NAME $NKN_CA$FILE_NAME
ret=`/usr/local/openssl/bin/c_rehash $NKN_CA > /dev/null 2>&1`	

echo "Valid"
fi

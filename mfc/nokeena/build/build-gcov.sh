export BUILD_EXTRA_LDADD="-lgcov"
echo "BUILD_EXTRA_LDADD set to '$BUILD_EXTRA_LDADD'"
BUILD_GCOV=1 build-mine.sh fresh 

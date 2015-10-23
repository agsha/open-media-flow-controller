/**
 * @file   dns_errno.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Sat Mar  3 18:50:38 2012
 * 
 * @brief
 * debug  error codes start from 1000 onwards
 * DNS specific error codes are from XXXX onwards
 * 
 * 
 */
#ifndef _DNS_ERRNO_
#define _DNS_ERRNO_

#include <errno.h>

#define E_DNS_QUERY_OOB 1000
#define E_DNS_TPOOL_TASK_CREATE 1001
#define E_DNS_UNSUPPORTED_RR_TYPE 1002
#define E_DNS_DATA_TFORM_LOOKUP_ERR 1003
#define E_DNS_QUERY_FAIL 1004
#define E_DNS_UNSUPPORTED_OPCODE 1005
#define E_PROTO_DATA_BUF_OOB 1100

#endif //_DNS_ERRNO_

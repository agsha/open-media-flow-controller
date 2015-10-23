/**
 * @file   cr_errno.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed Apr 18 02:50:36 2012
 * 
 * @brief  error codes for the CR product
 * GUIDELINES:
 * 1. use error codes in <errno.h> as much as possible usually in the
 * range 1-300
 * 2. module specific error codes should be greater than 1000
 * MODULE SPECIFIC ERROR CODE RESERVATION:
 * 1. DNS Daemon error codes 1000 - 2000 defined in dns_errno.h
 * 2. Store Daemon error codes 2001 -3000 defined in stored_errno.h
 * 
 */
#ifndef _CR_ERRNO_
#define _CR_ERRNO_

#include "dns_errno.h"
#include "stored/store_errno.h"

#endif //_CR_ERRNO_

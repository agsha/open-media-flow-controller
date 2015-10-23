/**
 * @file   store_errno.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed Apr 18 02:56:33 2012
 * 
 * @brief  error codes for store daemon
 * error codes reserved between 2001-3000
 * 
 * 
 */
#ifndef _STORE_ERRNO_
#define _STORE_ERRNO_

#define E_LF_DATA 2001

#define E_POP_ABSENT 2002
#define E_CE_ABSENT 2003
#define E_CG_ABSENT 2004
#define E_CE_DUP 2005
#define E_CE_EXITS 2006
#define E_DOMAIN_ABSENT 2007
#define E_POP_DUP 2008
#define E_CE_UNSUPP_LF 2009
#define E_CE_NO_IPV4_CNAME_ADDR_TYPE 2010
#define E_DOMAIN_INVALID_ROUTING_POLICY 2011
#define E_DOMAIN_DUP 2012
#define E_CG_DUP 2013
#define E_CE_POP_BINDING_ABSENT 2014
#define E_CE_CG_BINDING_ABSENT 2015
#define E_CG_DOMAIN_BINDING_ABSENT 2016
#define E_MAX_DOMAIN_LIMIT_HIT 2017
#define E_MAX_CE_LIMIT_HIT 2018
#define E_CE_POP_EXISTS 2019
#define E_DOMAIN_HAS_CG 2020

#endif //_STORE_ERRNO_

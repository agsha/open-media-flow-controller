#  Makefile
#
#  Filename:  $Source: /windy/home/scm/CVS_TMS/customer/Makefile,v $
#  Revision:  $Revision: 1.5 $
#  Date:      $Date: 2006/03/29 23:36:09 $
#  Author:    $Author: gregs $
# 
#  (C) Copyright 2002-2006 Tall Maple Systems, Inc.  
#  All rights reserved.
#
#  $TallMaple: customer/Makefile,v 1.5 2006/03/29 23:36:09 gregs Exp $
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=${PROD_PRODUCT_LC}

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

release:
	$(MAKE) -C ${PROD_PRODUCT_LC}/src release

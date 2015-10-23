#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2006 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

#
# This file contains definitions and graft functions that are specific
# to certain optional Samara features.  Some of these features may be
# separately licensable; while others may be included in the base Samara
# product but their code kept separate for better abstraction, etc.
#
# As with customer.sh and customer_rootflop.sh, this content is split
# across two files to minimize the size added to the root floppy.
#
# The graft functions here do not have comments at the top telling 
# what they are each for.  This is mainly because it would be a pain to
# keep updating them here as well as in the customer scripts they 
# appear in.  Either look at the context they are called from, or read
# the comment out of customer_rootflop.sh.
#
# In this file we do not test any of the PROD_FEATURE variables to see
# which features are enabled.  The assumption is that the scripts 
# containing definitions for a given feature will only be installed 
# if that feature is installed.  So if we find the script, we assume
# the feature is installed, so we should call it.
#
# XXX/EMT: we should have a more automated way of checking for graft 
# functions for all of the optional features, so that we don't have to 
# modify this file whenever we add a new graft function.  (We could 
# do the same thing for looking for scripts, but that would make us 
# scan for a lot more files, and so could be costly if run frequently,
# and it would only save us a modification every time a new feature
# is added.)
#

# Definitions for the CMC features
if [ -f /sbin/cmc_rootflop.sh ]; then
    . /sbin/cmc_rootflop.sh
fi

# Definitions for the Cluster features
if [ -f /sbin/cluster_rootflop.sh ]; then
    . /sbin/cluster_rootflop.sh
fi

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_1=y
opt_manufacture_graft_1()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_1" = "y" ]; then
        cmc_manufacture_graft_1
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_1" = "y" ]; then
        cluster_manufacture_graft_1
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_2=y
opt_manufacture_graft_2()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_2" = "y" ]; then
        cmc_manufacture_graft_2
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_2" = "y" ]; then
        cluster_manufacture_graft_2
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_3=y
opt_manufacture_graft_3()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_3" = "y" ]; then
        cmc_manufacture_graft_3
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_3" = "y" ]; then
        cluster_manufacture_graft_3
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_3b=y
opt_manufacture_graft_3b()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_3b" = "y" ]; then
        cmc_manufacture_graft_3b
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_3b" = "y" ]; then
        cluster_manufacture_graft_3b
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_4=y
opt_manufacture_graft_4()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_4" = "y" ]; then
        cmc_manufacture_graft_4
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_4" = "y" ]; then
        cluster_manufacture_graft_4
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_4b=y
opt_manufacture_graft_4b()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_4b" = "y" ]; then
        cmc_manufacture_graft_4b
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_4b" = "y" ]; then
        cluster_manufacture_graft_4b
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_5=y
opt_manufacture_graft_5()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_5" = "y" ]; then
        cmc_manufacture_graft_5
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_5" = "y" ]; then
        cluster_manufacture_graft_5
    fi
}

# -----------------------------------------------------------------------------
HAVE_OPT_MANUFACTURE_GRAFT_6=y
opt_manufacture_graft_6()
{
    if [ "$HAVE_CMC_MANUFACTURE_GRAFT_6" = "y" ]; then
        cmc_manufacture_graft_6
    fi
    if [ "$HAVE_CLUSTER_MANUFACTURE_GRAFT_6" = "y" ]; then
        cluster_manufacture_graft_6
    fi
}

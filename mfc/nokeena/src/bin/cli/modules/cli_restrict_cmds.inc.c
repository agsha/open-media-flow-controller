/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

/* 
 * This file is included by src/bin/cli/modules/cli_restrict_cmds.c.
 */

/*
 * Graft point 1: modify list of restricted commands 
 */
#if CLI_RESTRICT_CMDS_INC_GRAFT_POINT == 1

    err = tstr_array_append_str(restricted_cmds, "media-cache disk * cache-type *");
    bail_error(err);

#endif /* GRAFT_POINT 1 */

/*
 *******************************************************************************
 * nkn_nodemap_originroundrobinmap.h - Internal interface
 *******************************************************************************
 */

#ifndef _NKN_NODEMAP_ORIGINROUNDROBINMAP_H
#define _NKN_NODEMAP_ORIGINROUNDROBINMAP_H

/*
 * MAX_ORIGINRR_MAP_NODES is bounded by the following:
 *
 * 1) FQUEUE_ELEMENTSIZE, which is currently 8KB, since the FQueue is
 *    used to pass the initial cluster configuration to nkn_cmm.
 *    Typical overhead is ~100 bytes per node.
 *    This bounds MAX_ORIGINESCAL_MAP_NODES to 
 *    (8KB-1KB(overhead))/100 => ~71 nodes
 *
 * 2) MAX_CLUSTER_MAP_NODES, which is currently 256, sizes the generic
 *    cluster membership data structures.
 */
#define MAX_ORIGINRR_MAP_NODES    64
#define MAX_ORIGINRR_MAP_RRWEIGHT 64

#endif /* _NKN_NODEMAP_ORIGINROUNDROBINMAP_H */

/*
 * End of nkn_nodemap_originroundrobinmap.h
 */

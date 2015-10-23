/*
 *******************************************************************************
 * nkn_nodemap_clustermap.h - Internal interface
 *******************************************************************************
 */

#ifndef _NKN_NODEMAP_CLUSTERMAP_H
#define _NKN_NODEMAP_CLUSTERMAP_H

/* 
 * MAX_CLUSTERMAP_NODES is bounded by the following:
 *
 * 1) libketama requires (max nodes * 40 * 28 * 4) bytes of stack space.
 *    This effects the net and node status thread stacks which currently
 *    are sized at 512MB.
 *    Using 64 nodes results in 286,720 bytes for libketama, the 
 *    (512MB - 286,720) remaining bytes are used for basic nvsd processing 
 *    (128KB max) + cushion.
 *
 * 2) libketama MC_SHMSIZE, which currently is 512MB, is the maximum size 
 *    of the shared memory segment which holds the continuum data.   
 *    This bounds MAX_CLUSTERMAP_NODES to 512MB/4480 => ~117 nodes
 *
 * 3) FQUEUE_ELEMENTSIZE, which is currently 8KB, since the FQueue is
 *    used to pass the initial cluster configuration to nkn_cmm.
 *    Typical overhead is ~100 bytes per node.
 *    This bounds MAX_CLUSTERMAP_NODES to (8KB-1KB(overhead))/100 => ~71 nodes
 *
 * 4) MAX_CLUSTER_MAP_NODES, which is currently 256, sizes the generic
 *    cluster membership data structures.
 */
#define MAX_CLUSTERMAP_NODES	64 

/*
 *******************************************************************************
 * nodemap_clustermap_init - NodeMap ClusterMap system initialization
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
int nodemap_clustermap_init(void);

#endif /* _NKN_NODEMAP_CLUSTERMAP_H */

/*
 * End of nkn_nodemap_clustermap.h
 */

/*
 *
 * src/bin/snmp/tms/sn_cache.h
 *
 *
 *
 */

#ifndef __SN_CACHE_H_
#define __SN_CACHE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ............................................................................
 * NOTE: this file is part of the old implementation of SNMP tables.
 *       This has been replaced with a new implementation, and is 
 *       currently inert.
 * ............................................................................
 */


int sn_cache_init(void);

int sn_cache_deinit(void);

int sn_cache_get_next_binding_child(const char *parent, const char *this_child,
                                    bn_type btype, lt_dur_ms cache_lifetime_ms,
                                    const bn_binding **ret_binding);

int sn_cache_get_nth_binding_child(const char *parent, uint32 n,
                                   bn_type btype, lt_dur_ms cache_lifetime_ms,
                                   const bn_binding **ret_binding);

int sn_cache_get_binding(const char *node_name, const char *cache_subtree_root,
                         lt_dur_ms cache_lifetime_ms,
                         const bn_binding **ret_binding);


#ifdef __cplusplus
}
#endif

#endif /* __SN_CACHE_H_ */

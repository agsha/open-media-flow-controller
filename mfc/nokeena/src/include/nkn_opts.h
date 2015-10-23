#ifndef NKN_OPTS_H
#define NKN_OPTS_H
/*
 * Definitions to provide optimizations
 */

/* Likely and unlikely give hints to the compiler for branch prediction */
#define likely(x)	__builtin_expect((x),1)
#define unlikely(x)	__builtin_expect((x),0)

#endif /* NKN_OPTS_H */

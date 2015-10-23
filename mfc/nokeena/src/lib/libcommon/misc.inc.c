/* Graft file for $PROD_TREE_ROOT/src/lib/libcommon/misc.c. */
#ifndef __MISC_INC_C_
#define __MISC_INC_C_
#endif /* __MISC_INC_C_ */

/* 
 * Different blocks are chosen by the caller #define'ing MISC_INC_GRAFT_POINT
 * to the appropriate number.
 */

/* Graft point 1: model name mappings */
#if MISC_INC_GRAFT_POINT == 1

    {"cache",    "Cache on root drive"},
    {"normal",   "No cache on root drive"},
    {"vxa1",     "Cache on root drive, eUSB boot"},
    {"vxa2",     "No cache on root drive, eUSB boot"},
    {"mirror",   "No cache on root drive, Root drive mirror"},
    {"demo8g2",  "No cache on root drive, Minimum 8G root HDD for demo"},
    {"demo32g1", "Cache on root drive, Minimum 32G root HDD for demo"},

#endif /* GRAFT_POINT 1 */

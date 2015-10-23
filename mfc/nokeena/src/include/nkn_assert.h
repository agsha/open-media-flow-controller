#ifndef __NKN_ASSERT__H
#define __NKN_ASSERT__H

/*
 * Assert that can be enabled.
 */
#include <assert.h>
#include "nkn_stat.h"

/* Depend on gcc mapping all global defs of nkn_assert_enable to same addr */
extern int nkn_assert_enable;
extern void print_trace(void);

#define NKN_ASSERT_X(x, func, line) \
{ \
	if (nkn_assert_enable) { \
		assert(x); \
	} \
	else { \
		static int32_t assert_hits = 0; \
		if ((x) == 0) { \
		    if (assert_hits == 0) { \
			char assert_name[100]; \
			snprintf(assert_name, 100, "assert_%s_%d", func, line); \
			nkn_mon_add(assert_name, NULL, &assert_hits, sizeof(int32_t)); \
		    } \
		    assert_hits++; \
		    DBG_LOG(SEVERE, MOD_SYSTEM, "hit assert %d times", assert_hits); \
		    print_trace(); \
		} \
	} \
}

#define NKN_ASSERT(x) NKN_ASSERT_X(x, __FUNCTION__, __LINE__)

#endif // __NKN_ASSERT__H

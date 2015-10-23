/*
 *
 * str_array.h
 *
 *
 *
 */

#ifndef __STR_ARRAY_H_
#define __STR_ARRAY_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file str_array.h Array of strings (char *)
 * \ingroup lc_ds
 */

#include <stdlib.h>
#include "typed_array_tmpl.h"

TYPED_ARRAY_HEADER_NEW_NONE(str, char *);

/**
 * Create a new string array.  Note that the options this specifies
 * cause strings to be copied on add, so the array does not take
 * ownership of strings passed in.
 *
 * \retval ret_arr A newly allocated string array
 */
int str_array_new(str_array **ret_arr);

#ifdef __cplusplus
}
#endif

#endif /* __STR_ARRAY_H_ */

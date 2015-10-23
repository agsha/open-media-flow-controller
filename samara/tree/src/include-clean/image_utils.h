/*
 *
 * image_utils.h
 *
 *
 *
 */

#ifndef __IMAGE_UTILS_H_
#define __IMAGE_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

/* ------------------------------------------------------------------------- */
/** \file image_utils.h Utilities for system image.
 * \ingroup lc
 */

/* ------------------------------------------------------------------------- */
/** Built-in features-related functions
 * \param feature Name of feature to lookup
 * \retval ret_match Indication if feature is built-in to the system
 */
int lc_have_prod_feature(const char *feature, tbool *ret_match);

#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_UTILS_H_ */

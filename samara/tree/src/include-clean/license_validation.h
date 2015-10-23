/*
 *
 * license_validation.h
 *
 *
 *
 */

#ifndef __LICENSE_VALIDATION_H_
#define __LICENSE_VALIDATION_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "license.h"

/* ------------------------------------------------------------------------- */
/** \file license_validation.h Routines for accessing license validators.
 * \ingroup llic
 *
 * This header file is the API for accessing license validators.
 * It is used only by those binaries who need to be able to check
 * licenses for validity.
 *
 * Note that this is different from license_validator.h.  See top of
 * license.h for explanation of the differences between these three
 * header files.
 */

/**
 * Return the secret to use for type 1 licenses.  Note that this just
 * extracts the first valid secret out of an array of validators, 
 * presumably returned from lk_get_validators().
 */
int lk_get_lk1_secret(const lk_validator_calc_array *validators,
                      char **ret_secret);

/**
 * Return the validators used for the keyed hash in licenses.
 *
 * Note that this takes some calculation to produce, so generally
 * callers should only call us once, and save the result, which will
 * not change while the process is running.  The caller is responsible
 * for freeing the result.
 */
int lk_get_validators(lk_validator_calc_array **ret_validators);

#ifdef __cplusplus
}
#endif

#endif /* __LICENSE_VALIDATION_H_ */

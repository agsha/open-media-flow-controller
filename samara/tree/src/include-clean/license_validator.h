/*
 *
 * license_validator.h
 *
 *
 *
 */

#ifndef __LICENSE_VALIDATOR_H_
#define __LICENSE_VALIDATOR_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file license_validator.h Licensing validator structure
 * \ingroup llic
 *
 * This file defines a structure which can be used to represent
 * validators for licenses.  A validator is some data used to verify
 * the validity of a license.  Currently the only kind of validator
 * supported is the shared secret, but other kinds may be supported
 * in the future.
 *
 * These definitions would naturally go into license.h; except that
 * these need to be \#included from customer.h, which itself is already
 * \#included from license.h.  Having this as a separate file avoids a
 * circular dependency.
 *
 * Note that this is different from license_validation.h.  See top of
 * license.h for explanation of the differences between these three
 * header files.
 */

/* As we do not include common.h, check if this include is prohibited */
#if defined(NON_REDIST_HEADERS_PROHIBITED)
#error Non-redistributable header file included when prohibited
#endif

#include "ttypes.h"

#define LV_MAX_DATA_INT 64

/**
 * What kind of validator is this?
 */
typedef enum {
    lvt_none = 0,

    /** Shared secret, encoded in plaintext in lv_data_str */
    lvt_secret_plaintext,

    /** Shared secret, obfuscated with ROT-13 in lv_data_str */
    lvt_secret_obfuscated,

    /**
     * Shared secret, stored in lv_data_int.  The first number in the
     * array is an offset, which functions kind of like a salt with
     * hashes.  It must be in [0..127].  Subsequent numbers, until
     * terminated with a -1 delimiter, represent the characters in the
     * secret.  Each value is the sum of the previous value (0 for the
     * first), plus the ASCII value of the character being encoded, plus
     * the offset defined in the first slot, all mod 256.
     *
     * e.g. for a 3-char string with ASCII values {a0, a1, a2},
     * using 17 as the offset, we store four numbers:
     *   - offset = 17
     *   - n0 = (0  + a0 + offset) % 256
     *   - n1 = (n0 + a1 + offset) % 256
     *   - n2 = (n1 + a2 + offset) % 256
     *
     * Use utility in tools/license_secret to encode secrets.
     */
    lvt_secret_int_additive,

} lk_validator_type;

/**
 * What is the status of this validator?  e.g. is it still valid?
 */
typedef enum {
    lvs_none = 0,

    /** Licenses encoded with this secret are valid */
    lvs_valid,

    /** Licenses encoded with this secret are invalid (due to revocation) */
    lvs_revoked,

} lk_validator_status;

/**
 * How is lv_relevance_data used to limit the licenses to which this
 * validator is relevant?
 */
typedef enum {
    /**
     * The validator is relevant to all licenses.  lv_relevance_data is
     * ignored.
     */
    lvrc_none = 0,

    /**
     * The validator is only relevant to licenses for specific feature
     * names.  lv_relevance_data is a comma-delimited list of feature
     * names.  The validator is relevant only for these features.
     */
    lvrc_only_features,

    /**
     * The validator is not relevant to licenses for specific feature
     * names.  lv_relevance_data is a comma-delimited list of feature
     * names.  The validator is relevant for all features EXCEPT those
     * listed here.
     */
    lvrc_only_not_features,

} lk_validator_relevance_type;

typedef struct lk_validator {
    /** Type of validator */
    lk_validator_type lv_type;

    /** Validator status */
    lk_validator_status lv_status;

    /**
     * Data for this validator, in string form.  In the case of a shared
     * secret held as a string, this is the secret itself, either in
     * plaintext or obfuscated.
     */
    const char *lv_data_str;

    /**
     * Data for this validator, in integer form.  This is essentially an
     * alternative to lv_data_str, which offers another way of obfuscating
     * the data, so it doesn't show up so obviously in the output of
     * "strings".  Its exact interpretation depends on lv_type.
     *
     * There is no separate field to designate the length of the data in
     * this field; by convention, the contents of this field are
     * terminated by a -1.
     */
    int lv_data_int[LV_MAX_DATA_INT];

    /**
     * Magic number for this validator.  This field is optional, and
     * has no built-in semantics; its meaning is only what is added by
     * the customer.  In mgmtd, this number is passed in an mdl_license
     * structure to an mdm_license_activation_check_func function, to
     * indicate which validator was used to validate the license.
     * If you are going to use it for that purpose, it should be unique
     * among all of your validator definitions, at least inasmuch as you
     * want to be able to distinguish them -- of course, it can be the
     * same for validators that you want to treat the same.
     *
     * By convention, pass zero here if you do not intend to use the field.
     */
    uint32 lv_magic_number;

    /**
     * Validator relevance type: tells how to interpret lv_relevance_data.
     */
    lk_validator_relevance_type lv_relevance_type;

    /**
     * Validator relevance data: some data which can be used to declare
     * under what conditions this validator record applies.  The specific
     * interpretation of this data is dependent on lv_relevance_type;
     */
    const char *lv_relevance_data;

    /**
     * Minimum permitted length for a hash for a license using this
     * validator.  This constraint is in addition to the global
     * lk_hash_min_length.  Set to 0 to have no additional constraints.
     */
    uint32 lv_hash_min_length;

    /**
     * Comma-separated list of hash algorithms permitted to be used with
     * this validator.  This constraint is in addition to the global
     * lk_hash_permitted_algorithms.  Set to NULL to have no additional
     * constraints.
     */
    const char *lv_hash_permitted_algorithms;

} lk_validator;

#ifdef __cplusplus
}
#endif

#endif /* __LICENSE_VALIDATOR_H_ */

/*
 *
 * license.h
 *
 *
 *
 */

#ifndef __LICENSE_H_
#define __LICENSE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup llic LibLicense: licensing utilities */

/* ------------------------------------------------------------------------- */
/** \file license.h License key utilities
 * \ingroup llic
 *
 * The API defined in this file is mainly used by the genlicense
 * command-line utility for generating licenses, and by the mgmtd
 * infrastructure for validating licenses.  Customer code will
 * generally not need to call into this API.  To register a
 * licenseable feature from your mgmtd module, or to register an
 * activation option callback, please see the licensing section of
 * md_mod_reg.h.
 *
 * (However, if you are adding a new activation or informational
 * option tag type, you'll need to use the graft point in this file as
 * part of the process of registering it.)
 *
 * There are three header files related to licenses:
 *
 *   1. license_validator.h: declares the lk_validator data structure,
 *      which is used in customer.h to define license validators
 *      (shared secrets).  Because this is included by customer.h, it
 *      is ultimately included by most code in the Samara infrastructure.
 *      It does not contain any APIs.
 *
 *   2. license_validation.h: declares routines for accessing the 
 *      license validators.  These APIs are implemented in 
 *      liblicense_validation.  Because we want to keep the validator
 *      data as restricted as possible, only those who need to 
 *      check licenses for validation -- mgmtd and dumplicense --
 *      link with this library.  Customer.h should use an
 *      '\#ifdef LK_NEED_VALIDATORS' to only define its validators in
 *      those contexts, as this library is the only one to define
 *      that constant.
 *
 *   3. license.h (this file): general routines for working with
 *      licenses.  These APIs are implemented by liblicense.
 *      Only code that needs to work with licenses includes this
 *      and links with the library, but it does not need to be so
 *      restricted as liblicense_validation, since it does not include
 *      any validators; it expects to be passed them as parameters.
 *      Notably, genlicense links with liblicense but not 
 *      liblicense_validation.  Thus it does not know anything about
 *      the validators, and can be more broadly distributed (e.g. to
 *      field personnel who are only being told a subset of the
 *      validators, which may come with more restrictions).
 */

#include "common.h"
#include "tstring.h"
#include "ttime.h"
#include "license_validator.h"
#include "customer.h"
#include "bnode.h"

/* ------------------------------------------------------------------------- */
/** @name Common to multiple license versions
 */

/*@{*/

#define LK_PREFIX "LK"
static const char lk_prefix[] = LK_PREFIX;

/**
 * Elsewhere these are just referered to as "type 1" and "type 2"
 * licenses.  The use of an enum here is a legacy.
 */
typedef enum {
    lkt_NONE = 0,
    lkt_tied_db_node_v1 = 1,
    lkt_tagged_v2 = 2,
    lkt_LAST
} lk_license_type;

int lk_validate_feature_name(const char *feature_name, lk_license_type ltype,
                             tbool *ret_valid);

int lk_get_license_type(const char *license, tbool *ret_header_ok, 
                        lk_license_type *ret_ltype);

static const char lk_pad_char = '_';
static const char lk_sep_char = '-';

/**
 * The license version header and the feature name are delimited by
 * lk_sep_char.  Subsequently, we insert them every
 * lk_hash_sep_interval characters for readability, but they have no
 * other significance.
 */
static const uint32 lk_hash_sep_interval = 4;

/**
 * Used for type 1 and type 2 licenses.  Type 1 uses these as a
 * comprehensive list of all the things you can tie a license to.
 * Type 2 uses these to implement its activation options that mirror
 * the type 1 "tied" options.  Note that new activation options are
 * not added here, but in lk_option_tag_id (and other places which
 * reference this enum).
 */
typedef enum {
    lktii_NONE =      0,
    lktii_first_mac = 1,
    lktii_hostid =    2,
    lktii_untied =    3,
    lktii_LAST
} lk_tied_info_id;

static const lc_enum_string_map lk_tied_info_id_map[] = {
    {lktii_NONE, "NONE"},
    {lktii_first_mac, "first_mac"},
    {lktii_hostid, "hostid"},
    {lktii_untied, "untied"},
    {0, NULL},
};

/**
 * These are the names of the nodes in the system whose value the tied
 * value from the license must match in order for the license to be
 * considered valid.
 */
static const lc_enum_string_map lk_tied_info_names[] = {
    {lktii_first_mac, "/net/interface/state/" md_if_license_name 
     "/type/ethernet/mac"},
    {lktii_hostid, "/system/hostid"},
    {0, NULL},
};

/*
 * NOTE: these numbers are encoded in type 2 licenses, and so must
 * remain stable.
 */
typedef enum {
    lht_none = 0,
    lht_hmac_md5_full = 1,
    lht_hmac_md5_96_bit = 2,
    lht_hmac_md5_48_bit = 3,
    lht_hmac_sha256_full = 5,
    lht_hmac_sha256_128_bit = 6,
    lht_hmac_sha256_96_bit = 7,
    lht_hmac_sha256_48_bit = 8,
} lk_hash_type;

static const lc_enum_string_map lk_hash_type_short_str_map[] = {
    {lht_none,                "none"},
    {lht_hmac_md5_full,       "hmac_md5_full"},
    {lht_hmac_md5_96_bit,     "hmac_md5_96"},
    {lht_hmac_md5_48_bit,     "hmac_md5_48"},
    {lht_hmac_sha256_full,    "hmac_sha256_full"},
    {lht_hmac_sha256_128_bit, "hmac_sha256_128"},
    {lht_hmac_sha256_96_bit,  "hmac_sha256_96"},
    {lht_hmac_sha256_48_bit,  "hmac_sha256_48"},
    {0, NULL}
};

static const lc_enum_string_map lk_hash_type_long_str_map[] = {
    {lht_none,                "none"},
    {lht_hmac_md5_full,       "HMAC MD5 with full 128-bit length"},
    {lht_hmac_md5_96_bit,     "HMAC MD5 truncated to 96 bits"},
    {lht_hmac_md5_48_bit,     "HMAC MD5 truncated to 48 bits"},
    {lht_hmac_sha256_full,    "HMAC SHA256 with full 256-bit length"},
    {lht_hmac_sha256_128_bit, "HMAC SHA256 truncated to 128 bits"},
    {lht_hmac_sha256_96_bit,  "HMAC SHA256 truncated to 96 bits"},
    {lht_hmac_sha256_48_bit,  "HMAC SHA256 truncated to 48 bits"},
    {0, NULL}
};

/*
 * Hash length and algorithm requirements.  If these were not defined
 * already in customer.h (included above), define them here.
 */

#ifndef lk_hash_min_length
#define lk_hash_min_length             48
#endif

#ifndef lk_hash_permitted_algorithms
#define lk_hash_permitted_algorithms   "hmac_md5,hmac_sha256"
#endif

#ifndef lk_hash_type_default
#define lk_hash_type_default                lht_hmac_sha256_48_bit
#endif

/**
 * Tell if the specified hash type is legal, according to the
 * global limitations defined above.
 *
 * NOTE: this does NOT reflect hash type restrictions defined on a
 * per-validator basis in lk_validators.
 */
int lk_validate_hash_type(lk_hash_type hash_type,
                          const char **ret_hash_alg, uint32 *ret_hash_bits,
                          tbool *ret_length_ok, tbool *ret_alg_ok,
                          tbool *ret_overall_ok);

typedef struct lk_validator_calc {
    /**
     * Validator type.  Currently can only be lvt_secret_plaintext,
     * since obfuscated secrets are de-obfuscated before being put
     * into here.
     */
    lk_validator_type lvc_type;

    /**
     * Validator status, copied straight out of lv_status field of
     * lk_validator struct.
     */
    lk_validator_status lvc_status;

    /**
     * Validator data, translated into a normalized type.  For a
     * shared secret, always a plaintext secret (de-obfuscated if the
     * original type was an obfuscated shared secret).
     */
    tstring *lvc_data;

    /**
     * Validator magic number, copied straight out of lv_magic_number
     * field of lk_validator struct.
     */
    uint32 lvc_magic_number;
    
    /**
     * Relevance type, copied straight out of lv_relevance_type field
     * of lk_validator struct.
     */
    lk_validator_relevance_type lvc_relevance_type;

    /**
     * Relevance data, copied straight out of lv_relevance_data field
     * of lk_validator struct.
     */
    const char *lvc_relevance_data;

    /**
     * If lvc_relevance_type is either lvrc_only_features or
     * lvrc_only_not_features, then lvc_relevance_data is a
     * comma-delimited list of feature names.  In this case, we are a
     * tokenization of that list.  Otherwise, we are NULL.
     */
    tstr_array *lvc_relevance_feature_list;

    /**
     * Minimum hash length, copied straight from lv_hash_min_length
     * field of lk_validator struct.
     */
    uint32 lvc_hash_min_length;

    /**
     * List of permitted hash algorithms, tokenized from 
     * lv_hash_permitted_algorithms field of lk_validator struct.
     */
    tstr_array *lvc_hash_permitted_algorithms;

    /**
     * Validator identifier.  Result of hashing the validator using
     * lk_calc_validator_ident().
     */
    uint16 lvc_identifier;

} lk_validator_calc;

TYPED_ARRAY_HEADER_NEW_NONE(lk_validator_calc, lk_validator_calc *);

int lk_calc_validator_ident(const char *secret, uint16 *ret_vi);

/**
 * Decode a license secret which was encoded with the
 * lvt_secret_int_additive method.  This is used by the license validation
 * library to decode the valid secrets declared in customer.h.  If code
 * needs to be able to generate licenses autonomously without being told
 * the secret by something else (not recommended, obviously, but sometimes
 * might be temporarily unavoidable), it can encode the secret using the
 * same method and only decode it at runtime, to make it harder for someone
 * to fish the secret out of it.
 */
int lk_decode_int_additive(const int vdata_int[LV_MAX_DATA_INT],
                           char **ret_secret);

/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Only applying to type 1 licenses
 */

/*@{*/

static const uint32 lk_hash_trunc_size = 6;
static const lk_hash_type lk_type_1_hash_type = lht_hmac_md5_48_bit;

int
lk_key_generate_type_1(tstring **ret_license_string,
                       const char *feature_name, 
                       lt_time_sec active_start,
                       lt_time_sec active_end,
                       lk_tied_info_id tied_info_id,
                       const char *tied_info_value_str,
                       const char *secret);

int
lk_key_components_type_1(const char *license_string,
                         tbool *ret_is_well_formed,
                         lk_license_type *ret_license_type,
                         tstring **ret_feature_name, 
                         lt_time_sec *ret_active_start,
                         lt_time_sec *ret_active_end,
                         lk_tied_info_id *ret_tied_info_id,
                         tstring **ret_hash);

int
lk_key_validate_type_1(const char *license_string,
                       lt_time_sec constraint_curr_time,
                       const char *tied_info_value_str,   
                       const char *secret,
                       tbool *ret_is_well_formed,
                       tbool *ret_is_valid,
                       tbool *ret_is_active,
                       tstring **ret_activated_feature);

/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Only applying to type 2 licenses
 */

/*@{*/

/**
 * Type of a tag.  Note that these must remain stable, as their values
 * are encoded into the licenses, so changing them would cause the
 * interpretation of previously-generated licenses to change.
 */
typedef enum {
    ltt_none = 0,
    ltt_option = 1,
    ltt_key = 2,
    ltt_LAST
} lk_tag_type;

/**
 * Type of an option tag.  This information is NOT encoded in the
 * license; it is implicit in the lk_option_tag_id, which identifies a
 * particular option which is registered with its option tag type.
 */
typedef enum {
    lott_none = 0,
    lott_activation,
    lott_info,
    lott_LAST,
} lk_option_tag_type;

static const lc_enum_string_map lk_option_tag_type_map[] = {
    {lott_none, "none"},
    {lott_activation, "activation"},
    {lott_info, "info"},
    {0, NULL}
};

/**
 * Option tag ids.  Must be in [0..127] as they are encoded in 7 bits.
 * These may be either activation options or informational options.
 * An array of structures declared separately specifies which kind
 * each one is, along with some other information about each.
 *
 * NOTE: ids [0..47] are reserved for Samara internal use.
 * Customers adding ids through the graft point should use [48..127].
 *
 * NOTE: these numbers are encoded in licenses, and so must remain
 * stable.
 */
typedef enum {
    loti_none = 0,

    /**
     * Activation option: system date must be greater than or equal to
     * the attribute.
     */
    loti_start_date = 1,

    /**
     * Activation option: system date must be less than or equal to
     * the attribute.
     */
    loti_end_date = 2,

    /**
     * Activation option: MAC address of the primary Ethernet
     * interface must match the attribute (same as lktii_first_mac).
     */
    loti_tied_primary_mac = 3,

    /**
     * Activation option: Host ID of the system must match the
     * attribute (same as lktii_hostid).
     */
    loti_tied_host_id = 4,

    /**
     * Activation option: Host ID of the system must match the
     * attribute.  Same as loti_tied_host_id, except that this option
     * requires the host ID string to be only lowercase hexadecimal
     * characters, which is the case for the host IDs created by
     * Samara's manufacturing by default.  This restriction allows us
     * to encode the host ID more compactly, leading to a shorter
     * license.
     */
    loti_tied_host_id_hex = 5,

    /**
     * Activation option: serial number of the system (per dmidecode)
     * must match the attribute.
     */
    loti_tied_serialno = 6,

    /**
     * Activation option: UUID of the system (per dmidecode) must
     * match the attribute.
     */
    loti_tied_uuid = 7,

    /**
     * Informational option: What limitations should we place on which
     * users can run the bash shell from the CLI using the "_shell"
     * or "_exec" commands?
     *
     *   (a) If this option is not present, the license imposes no
     *       restrictions on users.  The commands are still subject
     *       to whatever restrictions imposed by either (i) registration,
     *       or (ii) their execution handlers.
     *
     *   (b) If this option is present, its contents are treated as a
     *       comma-delimited list of usernames.  Any username in this 
     *       list may use the command.  These are original usernames,
     *       NOT mapped local usernames.
     *
     *   (c) The string ":g:admin" has a special meaning, and matches
     *       any user with admin privileges.
     */
    loti_shell_access_users = 19,

    /**
     * Informational option: How many clients will the CMC server
     * manage?  This only applies to the separately-licenseable
     * CMC product.
     *
     * NOT YET IMPLEMENTED (and not defined in lk_option_tags).
     */
    loti_cmc_max_clients = 20,

    /**
     * Informational option: Is Virtualization feature enabled?
     *
     * Note that this only applies to the licensed feature named by
     * VIRT_LICENSE_FEATURE_NAME, if it's defined.  The VIRT
     * feature does not need this option; it is assumed to be
     * implicitly true.
     *
     * Any other virtualization informational options (listed below)
     * are valid for either VIRT or the extra feature.
     */
    loti_virt = 30,

    /**
     * Informational option: if Virtualization is licensed at all,
     * what is the maximum number of VMs that may be powered on at
     * once?
     *
     * The summary function for this option is "max".
     *
     * Note: this acts just like loti_virt_max_vms_add, except for its
     * summary function: the other one is cumulative.  If licenses
     * with both informational options are present, the maximum of
     * both summary numbers is the one used.
     */
    loti_virt_max_vms = 31,

    /**
     * Informational option: if Virtualization is licensed at all,
     * what is the maximum number of VMs that may be powered on at
     * once?
     *
     * The summary function for this option is "sum".
     *
     * Note: this acts just like loti_virt_max_vms, except for its
     * summary function: this one is cumulative.  If licenses with
     * both informational options are present, the maximum of both
     * summary numbers is the one used.
     */
    loti_virt_max_vms_add = 32,

    /**
     * Hidden informational option: unique key identifier.  This holds
     * a number, randomly generated at license creation time, whose
     * purpose is to uniquify the license.  It also might be used as an
     * identifier/key to look up a license in a custom license management
     * solution.
     */
    loti_uniquifier = 46,

    /**
     * Hidden informational option: validator identifier.  This holds
     * a hash of the secret used to encode the license's hash, to
     * facilitate validation of the license.
     */
    loti_validator_ident = 47,

/* ========================================================================= */
/* Customer-specific graft point 1: additional option tag IDs.  NOTE:
 * IDs [0..47] are reserved for internal use; customer IDs must be in
 * [48..127] so as not to conflict.
 * =========================================================================
 */
#ifdef INC_LICENSE_INC_GRAFT_POINT
#undef LICENSE_INC_GRAFT_POINT
#define LICENSE_INC_GRAFT_POINT 1
#include "../include/license.inc.h"
#endif /* INC_LICENSE_INC_GRAFT_POINT */

} lk_option_tag_id;

static const uint32 lk_option_tag_id_max = (1 << 7) - 1;

/**
 * In conjunction with a value of ::lk_option_empty_behavior, this
 * enum describes the method to be used to calculate the the "summary
 * value" of an informational option for a given feature.  Usually
 * this applies only if there are multiple active licenses for the
 * feature, since if there was only one, the summary would generally
 * just reflect what was found in that one.
 *
 * Sometimes (as with losf_min and losf_max) this is used to select a
 * single one of the values, which would generally be the one most
 * favorable to the user.  In other cases (as with losf_sum), the
 * result may be an aggregation of all the available values.
 *
 * Note that determining a summary option value only applies to
 * informational options.  Specify losf_none for activation options.
 */
typedef enum {

    /** Do not calculate a summary value */
    losf_none = 0,

    /** Summarize using the highest value */
    losf_max,

    /** Summarize using the lowest value */
    losf_min,

    /** Summarize using the sum of all values */
    losf_sum,

    /* For backward compatibility only */
    lopt_none = losf_none,
    lopt_max = losf_max,
    lopt_min = losf_min,

} lk_option_summary_func;

/**
 * Specifies how to handle it when there are multiple valid licenses for
 * the same feature, and one or more of them do not specify a value for
 * a given option.
 */
typedef enum {
    loeb_none = 0,

    /**
     * If any valid licenses for the feature have no value for a given
     * option tag, the summary value is to be no value at all.  That is,
     * the "/license/feature/ * /option/ * /value" node would not be 
     * returned for that feature/option combination.  This can be used
     * when an option represents a restriction on some aspect of the 
     * feature, and not specifying the option means no restriction.
     */
    loeb_empty_wins,

    /**
     * If any valid licenses for the feature have no value for a given
     * option tag, they are ignored for purposes of computing the 
     * summary value.  If there are valid licenses for the feature,
     * but none of them specify a value for the option, the value
     * from ::lot_default_value is used.
     */
    loeb_empty_loses,

    /**
     * If any valid licenses for the feature have no value for a given
     * option tag, they are treated as if they have the value
     * specified in the ::lot_default_value field of the
     * ::lk_option_tag structure.  The normal summary rule
     * specified in the ::lot_summary field is then applied.
     * If lot_default_value is NULL, this acts the same as
     * ::loeb_empty_wins.
     */ 
    loeb_use_default,

} lk_option_empty_behavior;


/**
 * Flags to modify an lk_option_tag.
 */
typedef enum {
    lotf_none = 0,

    /**
     * Permit multiple instances of this option tag in a license.
     * By default, only one instance is permitted.
     *
     * NOT YET IMPLEMENTED -- currently multiple instances of all
     * option tags are permitted.
     *
     * (Implementation note: this value must remain 1, because this
     * enum supplanted a boolean field in the lk_option_tag structure,
     * and we want 'true' (defined as 1 in ttypes.h) to preserve its
     * previous meaning of permitting multiple licenses.)
     */
    lotf_permit_mult = 1 << 0,

    /**
     * Specify that this option tag pertains to a PROD_FEATURE which
     * is currently disabled.  We cannot make the entire definition
     * conditional on the feature (see bug 13713), but we do want to 
     * mostly disable its usage.  While the option will be recognized
     * when decoding a license and still displayed by dumplicense,
     * this flag has three effects:
     *   (a) Genlicense will not offer the option in the usage message,
     *       nor accept it if it is specified.
     *   (b) The contents of the option will not be exposed in the UI.
     *   (c) The monitoring nodes will not expose this option.
     *       There will be no option summary under the per-feature nodes,
     *       nor will the option show under the per-license nodes.
     *
     * XXX/EMT: (b) and (c) are slightly unfortunate.  (c), which 
     * forces (b), is done lest poorly-implemented license enforcement
     * code enforce the option.  See bug 13730 for proposed solution.
     *
     * Note that ::lotf_internal and ::lotf_invisible both cause a
     * subset of these effects, and so this effectively implies both
     * of them (though in those cases, the effects are used for
     * different reasons; i.e. a disabled feature isn't necessarily
     * "internal").
     */
    lotf_disabled = 1 << 1,

    /**
     * Specify that this option tag is for internal use by the
     * licensing infrastructure.  This has a subset of the effects of
     * ::lotf_disabled: it does (a) and (b), but not (c).
     */
    lotf_internal = 1 << 2,

    /**
     * Specify that this option tag is to be hidden from end users.
     * This is a subset of the behavior from ::lotf_disabled: just
     * (b), but neither (a) nor (c).
     */
    lotf_invisible = 1 << 3,

} lk_option_tag_flags;

typedef uint32 lk_option_tag_flags_bf;


/**
 * Option tag registration structure.
 */
typedef struct lk_option_tag {

    /**
     * Option tag ID
     */
    lk_option_tag_id lot_id;

    /**
     * String representing enum name with "loti_" removed.  Genlicense
     * will allow the user to specify an option tag ID with this
     * string or with an integer; and this also acts as an
     * lc_enum_string_map and can be used for logging, etc.
     */
    const char *lot_id_str;

    /**
     * Brief human-readable description of the option tag.  The CLI
     * and Web UI use this when displaying a list of options on a
     * license.
     */
    const char *lot_descr_brief;

    /**
     * More verbose human-readable description of the option tag.
     * Genlicense uses this in its usage help message.
     */ 
    const char *lot_descr;

    /**
     * Option tag type
     */
    lk_option_tag_type lot_option_tag_type;

    /**
     * Option flags.
     *
     * (NOTE: this used to be 'tbool lot_permit_mult'.  Since all enums
     * are type-compatible, this change was made without disruption.
     * We purposefully chose 1 for lotf_permit_mult so 'true' would 
     * preserve its meaning.  But code should be migrated to use 
     * these new flags; or in the most common case of not wanting any
     * flags, should specify 0 instead of false.)
     */
    lk_option_tag_flags_bf lot_flags;

    /**
     * Data type of the data to be specified in the license
     */
    bn_type lot_value_data_type;

    /**
     * How to determine the summary value for this option.  
     * See ::lk_option_summary_func for details.
     *
     * Applies only to informational options, as the rules for applying
     * activation options are already fixed (a license must meet all
     * criteria in it to be considered active).
     *
     * The result of applying this calculation to all eligible option
     * values is exposed through the monitoring node
     * "/license/feature/ * /option/ * /value" for any given feature
     * and option.
     */
    lk_option_summary_func lot_summary;

    /**
     * See ::lk_option_empty_behavior type for explanation.
     * Applies only to informational options.  Note that if lot_summary
     * is ::losf_none, this field has no effect, and so might as well
     * be ::loeb_none.
     */
    lk_option_empty_behavior lot_empty_behavior;

    /**
     * If the ::lot_option_empty field is ::loeb_use_default or
     * ::loeb_empty_loses, this field specifies the string
     * representation of the default value to use.  Otherwise, this
     * field is ignored and a warning may be logged if it is not NULL.
     * Applies only to informational options.
     */
    const char *lot_default_value;

    /**
     * If specified, limits the set of features to which this option tag
     * is relevant.  It is a comma-separated list of feature names.
     * The effects of setting this are:
     *   - If ::lot_default_value is specified, a feature may be assigned
     *     an option value that was not specified in any license for the
     *     feature.  But if the option is limited to a specific set of 
     *     features, the default will not be used to supply a value for
     *     that option in any other features.
     *   - Genlicense will refuse to generate licenses with this option on 
     *     other features.
     *   - If we encounter a license with an irrelevant option
     *     (e.g. it was generated before genlicense started enforcing
     *     this, or before the set of relevant features was set to 
     *     exclude this one), it is ignored.
     *   - If we encounter a license with an irrelevant option, we log
     *     a warning message (NOT YET IMPLEMENTED).
     *
     * If NULL, the option tag is considered relevant to all features.
     * If the empty string, the option tag is not considered relevant
     * to any features; this is a way of disabling it without removing
     * the registration.
     */
    const char *lot_relevant_features;

} lk_option_tag;

/**
 * Option tag registrations.  This array declares all of the option
 * tags, but in the case of activation options, the callback function
 * to enforce each is omitted.  The registration of an activation
 * option must be completed by a mgmtd module calling
 * mdm_license_add_activation_tag() to pass the callback function
 * pointer.
 *
 * XXX/EMT: I18N.  For now, just use N_(), but nothing will currently
 * catalog this file, nor do we try to look them up when using them.
 */
static const lk_option_tag lk_option_tags[] = {
    {loti_start_date, "start_date", N_("Start date"),
     N_("Start date: license not active before this date"),
     lott_activation, 0, bt_date,
     losf_none, loeb_none, NULL, NULL},
    
    {loti_end_date, "end_date", N_("End date"),
     N_("End date: license not active after this date"),
     lott_activation, 0, bt_date,
     losf_none, loeb_none, NULL, NULL},

    {loti_tied_primary_mac, "tied_primary_mac", N_("Tied to MAC addr"),
     N_("Tie to this MAC address on primary interface"),
     lott_activation, lotf_permit_mult, bt_macaddr802,
     losf_none, loeb_none, NULL, NULL},

    {loti_tied_host_id, "tied_host_id", N_("Tied to host ID"),
     N_("Tie to this host ID"),
     lott_activation, lotf_permit_mult, bt_string,
     losf_none, loeb_none, NULL, NULL},

    {loti_tied_host_id_hex, "tied_host_id_hex", N_("Tied to hex host ID"),
     N_("Tie to this host ID (lowercase hexadecimal)"),
     lott_activation, lotf_permit_mult, bt_string,
     losf_none, loeb_none, NULL, NULL},

    {loti_tied_serialno, "tied_serialno", N_("Tied to serial number"),
     N_("Tie to this serial number"),
     lott_activation, 0
#if !defined(PROD_TARGET_ARCH_FAMILY_X86)
     | lotf_disabled
#endif
     , bt_string, losf_none, loeb_none, NULL, NULL},

    /*
     * Note that although this declares bt_string as the data type,
     * that is only its intermediate representation.  It is actually
     * encoded in the license as binary, since the string is 
     * assumed to contain a hexadecimal rendering of 128 bits,
     * per RFC 4122.
     */
    {loti_tied_uuid, "tied_uuid", N_("Tied to UUID"),
     N_("Tie to this UUID (must be RFC 4122 compliant)"),
     lott_activation, 0
#if !defined(PROD_TARGET_ARCH_FAMILY_X86)
     | lotf_disabled
#endif
     , bt_string, losf_none, loeb_none, NULL, NULL},

    {loti_shell_access_users, "shell_access_users", 
     N_("Allowed users"),
     N_("List of users with shell access (comma-delimited)"),
     lott_info, 0
#if !defined(PROD_FEATURE_SHELL_ACCESS)
     | lotf_disabled
#endif
     , bt_string, losf_none, loeb_none, "", 
     "SHELL_ACCESS"},

    /*
     * This feature is not yet implemented...
     */
#if 0
    {loti_cmc_max_clients, "cmc_max_clients", N_("Max CMC clients"),
     N_("Maximum number of clients manageable by CMC server"),
     lott_info, 0
#if !defined(PROD_FEATURE_CMC_SERVER_LICENSED)
     | lotf_disabled
#endif
     , bt_uint32, losf_max, loeb_empty_wins, NULL, "CMC_SERVER"},
#endif /* 0 */

    /*
     * We need this in a constant whether or not it was defined, so
     * we can limit these other options to this particular feature.
     * But we don't want to generally define VIRT_LICENSE_FEATURE_NAME,
     * since its presence or absence means something in other
     * contexts.
     */
#ifdef VIRT_LICENSE_FEATURE_NAME
    #define VIRT_LICENSE_FEATURE_NAME_USED VIRT_LICENSE_FEATURE_NAME
#else /* VIRT_LICENSE_FEATURE_NAME */
    #define VIRT_LICENSE_FEATURE_NAME_USED "VIRT"
#endif /* VIRT_LICENSE_FEATURE_NAME */

    {loti_virt, "virt", N_("Virtualization"),
     N_("Virtualization feature enabled"),
     lott_info, 0
#if !defined(PROD_FEATURE_VIRT_LICENSED) || !defined(VIRT_LICENSE_FEATURE_NAME)
     | lotf_disabled
#endif
     , bt_bool, losf_max, loeb_empty_loses, NULL, 
     VIRT_LICENSE_FEATURE_NAME_USED},

    {loti_virt_max_vms, "virt_max_vms", N_("Max # of VMs running"),
     N_("Maximum number of virtual machines running"),
     lott_info, 0
#if !defined(PROD_FEATURE_VIRT_LICENSED)
     | lotf_disabled
#endif
     , bt_uint32, losf_max, loeb_empty_loses, NULL, 
     VIRT_LICENSE_FEATURE_NAME_USED},

    {loti_virt_max_vms_add, "virt_max_vms_add", 
     N_("Max # of VMs running (cumulative)"),
     N_("Maximum number of virtual machines running (cumulative)"),
     lott_info, 0
#if !defined(PROD_FEATURE_VIRT_LICENSED)
     | lotf_disabled
#endif
     , bt_uint32, losf_sum, loeb_empty_loses, NULL, 
     VIRT_LICENSE_FEATURE_NAME_USED},

#undef VIRT_LICENSE_FEATURE_NAME_USED

    {loti_uniquifier, "uniquifier", N_("Uniquifier"),
     N_("Uniqufier"),
     lott_info, lotf_internal, bt_uint64, 
     losf_none, loeb_none, NULL, NULL},

    {loti_validator_ident, "validator_ident", N_("Validator identifier"),
     N_("Validator identifier"),
     lott_info, lotf_internal, bt_uint16, 
     losf_none, loeb_none, NULL, NULL},

/* ========================================================================= */
/* Customer-specific graft point 2: additional option tags.
 * =========================================================================
 */
#ifdef INC_LICENSE_INC_GRAFT_POINT
#undef LICENSE_INC_GRAFT_POINT
#define LICENSE_INC_GRAFT_POINT 2
#include "../include/license.inc.h"
#endif /* INC_LICENSE_INC_GRAFT_POINT */

};

int lk_option_tag_lookup_by_str(const char *option_tag_id_str, 
                                const lk_option_tag **ret_option_tag);

int lk_option_tag_lookup_by_id(lk_option_tag_id id,
                               const lk_option_tag **ret_option_tag);

typedef struct lk_option {
    lk_option_tag_id lko_id;
    bn_attrib *lko_value;
} lk_option;

int lk_option_new(lk_option_tag_id id, bn_attrib **inout_attrib,
                  lk_option **ret_opt);

int lk_option_free(lk_option **inout_opt);

TYPED_ARRAY_HEADER_NEW_NONE(lk_option, lk_option *);

int lk_option_array_new(lk_option_array **ret_arr);

int
lk_key_generate_type_2(tstring **ret_license_string,
                       const char *feature_name, 
                       const char *secret, lk_hash_type hash_type, 
                       const lk_option_array *opts);

/**
 * This is like a combination of the old lk_key_components() and
 * lk_key_validate().  We always get the components (as
 * lk_key_validate() did), but then only validate if the validators
 * and ret_is_valid are provided.  We do NOT check if the license is
 * active, since that may require queries to mgmtd nodes, which is
 * done in a caller-specific way.
 *
 * 'ret_is_revoked' tells if the license used to be valid, but was
 * revoked.  This will only be populated if 'validators' and
 * 'ret_is_valid' were both also non-NULL.  A revoked license is
 * always invalid, so this will only ever return 'true' if
 * ret_is_valid is getting back 'false'.
 */
int
lk_key_interpret_type_2(const char *license_string,
                        const lk_validator_calc_array *validators,
                        tbool *ret_is_well_formed,
                        tstring **ret_feature_name, 
                        lk_option_array **ret_opts,
                        lk_hash_type *ret_hash_type,
                        tbool *ret_is_valid, tbool *ret_is_revoked,
                        uint32 *ret_validator_magic_number);

/**
 * Given the name of a feature and a particular kind of option tag,
 * tell whether the option tag is relevant.  This is based on the
 * contents of the lot_relevant_features field in the option tag.
 */
int lk_key_relevant_option_type_2(const char *feature_name,
                                  const lk_option_tag *option_tag,
                                  tbool *ret_is_relevant);

#ifdef __cplusplus
}
#endif

#endif /* __LICENSE_H_ */

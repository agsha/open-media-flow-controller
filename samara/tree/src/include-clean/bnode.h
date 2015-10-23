/*
 *
 */

#ifndef __BNODE_H_
#define __BNODE_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ========================================================================= */
/** \file src/include/bnode.h Binding node: general API
 * \ingroup gcl
 *
 * This file contains data type and function definitions for working
 * with binding nodes.
 */

#include "common.h"
#include "tstring.h"
#include "tbuffer.h"
#include "ttime.h"
#include "typed_arrays.h"

static const char bn_name_separator_char = '/';
static const char bn_name_escape_char = '\\';
static const char bn_name_quoting_char = '\0';
static const char bn_name_wildcard_char = '*';

/*
 * All the information about the body protocol for BNODE.
 */

/* Attributes */

/*
 * Indicates a type of hash algorithm, and any other related options
 * such as truncation.  This is currently used to indicate how we
 * computed the ba_crypt_key_hash attribute from the encryption key.
 *
 * NOTE: these numbers must be stable, as they are written into
 * persistent storage, and expected to mean the same thing when they
 * are read out again.
 */
typedef enum {
    bht_none = 0,

    /*
     * SHA256 truncated to 48 bits.  It doens't have to be very long,
     * as the main purpose of this hash is to detect key changes, not
     * for security.
     */
    bht_sha256_trunc_48 = 1,

} bn_hash_type;

/**
 * Attribute ID.  Every binding contains an array of attributes;
 * either zero or one of each attribute ID.  The most commonly-used
 * attribute ID is ba_value; the others are fairly rare.
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnAttributeId.java, which
 * must be kept in sync.
 *
 * NOTE: bn_attribute_id_map (in bnode.c) must be kept in sync.
 */
typedef enum {

    /* WARNING: do not change these numbers, only add new ones */

    /** Invalid/unspecified */
    ba_NONE           = 0,

    /**
     * Value: by far the most commonly-used attribute ID.
     * The term "value" is overloaded in our type system;
     * Do not confuse this with the value of an attribute.
     * Each attribute has an ID, name, type, and value.
     * ba_value represents the "value" attribute ID, but all
     * attributes regardless of ID have a value.
     */
    ba_value          = 1,

    /**
     * Encrypted form of a value.  This attribute should be of type
     * bt_binary, and holds the encrypted form of the serialized
     * representation of the ba_value attribute.  A binding that has
     * this attribute should not also have a ba_value attribute.
     *
     * NOTE that this attribute is automatically maintained by the
     * infrastructure, if PROD_FEATURE_SECURE_NODES is set.  Mgmtd
     * clients should NOT set this value on their own.  It will
     * generally be seen in certain nodes of a persisted db (viewable
     * e.g. with mddbreq), or in the results of a mgmtd query where
     * the mdqf_as_saved or bqnf_as_saved flag has been specified.
     */
    ba_encrypted_value = 2,

    ba_none           = 4,  /* not yet set */

    /**
     * If present, this attribute indicates that one or more other
     * attributes in this binding were transformed from an unrecognized
     * data type to a bt_binary type containing the GCL's full serialized 
     * (copyout) representation of the attribute.
     *
     * This attribute will be a string which is a comma-delimited list
     * of attribute ID names, e.g. "value,info".  This information
     * serves to alert the reader that one or more attributes were
     * transformed in this way; and also allows the original binding
     * to be reconstructed, if necessary, by a later version of Samara
     * (i.e. one that understands the data types which caused this
     * earlier version to transform the attribute(s) in this way).
     */
    ba_unknown_attribs = 5,

    /* ============================== */
    /* Registration attributes: "not dynamically changeable".
     *
     * EMT: since ba_FIRST_REGISTRATION_ATTRIB does not appear
     * anywhere in the source tree, there is apparently no real
     * difference in treatment between this group of attribute ids and
     * the ones above.  The intention was that they are ones that are
     * derived from registration and automatically managed by the
     * infrastructure.  This does not quite hold true for
     * ba_obfuscation, whose value is affected by registration but
     * also by the requests the lead to the creation of the bindings
     * it is found in.  But this attribute doesn't really fit anywhere
     * else either, since we do not really want it to appear
     * user-changeable.  (Being in this block does not prevent it from
     * being changed, but it at least puts it in company that suggests
     * that changing it is going behind the curtain, and not to be
     * done lightly.)
     */
     /* ============================== */
    ba_product_name   = 65, /* for upgrade: only on root node of tree */
    ba_FIRST_REGISTRATION_ATTRIB = ba_product_name,
    ba_module_name    = 66, /* for upgrade: only on root node of reg trees */
    ba_module_version = 67, /* for upgrade: semantic version of subtree */
    ba_info           = 68, /* meta-information, returned w/ query if set */
    ba_cap_mask       = 69, /* for external monitoring: mask of capabilities */

    /*
     * Track if value is obfuscated.  Currently, this should be a
     * boolean, 'true' for obfuscated, 'false' or not present if not.
     * Generally the attribute will only be present (as 'true') if 
     * the value is obfuscated.
     */
    ba_obfuscation    = 70, 

    /*
     * Contains a hash of the key which was used to encrypt this
     * node.
     *
     * (Since we use the same key to encrypt all of the secure nodes
     * in the database, it would be nicer if we could just represent
     * the hash once at the root of the subtree.  But it's simpler
     * this way, both due to the abstraction boundary between libmdb
     * and mgmtd, and to keep the scope of the encryption function
     * limited to the node it's working on.  i.e. when writing the db,
     * by the time the encryption function is called, it's too late to
     * add a ba_crypt_key_hash attribute to the root node, since it
     * has already been written.  Also, in the case where the key is
     * changed and we're not doing key management yet, you could end
     * up with a db where some nodes have the old key because they
     * have not been reset yet, while the nodes that have been set now
     * have the new key.)
     */
    ba_crypt_key_hash = 71,

    /*
     * Contains a uint32 that is an instance of bn_hash_type, which
     * tells what kind of hash was used on the encryption key to
     * compute ba_crypt_key_hash.  Any node that has a
     * ba_crypt_key_hash attribute should also have this attribute,
     * and vice-versa.
     */
    ba_crypt_key_hash_type = 72,

    /* ============================== */
    /* Automatic dynamic attributes: not changeable, must be requested */
    /* ============================== */

    ba_revision_id = 129,  /* NYI: mgmt db revision number (in response hdr) */
    ba_FIRST_AUTO_ATTRIB = ba_revision_id,
    ba_node_count = 130,   /* Number of nodes (wc iterate with count only) */

    ba_LAST,
} bn_attribute_id;

extern const lc_enum_string_map bn_attribute_id_map[];

#define bn_attribute_id_to_str(aid) lc_enum_to_string(bn_attribute_id_map, aid)

/**
 * Data types
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnType.java, which
 * must be kept in sync.
 *
 * NOTE: bn_type_map (in bnode.c) must be kept in sync.
 */
typedef enum {

    /* WARNING: do not change these numbers, only add new ones */

    /** Invalid/undefined type; distinct from bt_none */
    bt_NONE = 0,

    /* ********** Fixed size types */

    /* Integral types */
    
    /** Unsigned 8-bit integer */
    bt_uint8 =        1,

    /** Signed 8-bit integer */
    bt_sint8 =        2,

    /** Signed 8-bit integer */
    bt_int8 =         bt_sint8,

    /** Unsigned 16-bit integer */
    bt_uint16 =       3,

    /** Signed 16-bit integer */
    bt_sint16 =       4,

    /** Signed 16-bit integer */
    bt_int16 =        bt_sint16,
    
    /** Unsigned 32-bit integer */
    bt_uint32 =       5,

    /** Signed 32-bit integer */
    bt_sint32 =       6,

    /** Signed 32-bit integer */
    bt_int32 =        bt_sint32,

    /** Unsigned 64-bit integer */
    bt_uint64 =       7,

    /** Signed 64-bit integer */
    bt_sint64 =       8,
    
    /** Signed 64-bit integer */
    bt_int64 =        bt_sint64,

    /** Boolean (true/false) */
    bt_bool =         9, 

    /** Boolean (true/false) */
    bt_tbool =        bt_bool,

    /** 32-bit floating point */
    bt_float32 =      20, 

    /** 64-bit floating point */
    bt_float64 =      21,

    /* bt_float128, */
    /* bt_rational32_32, */

    /** Single-byte ASCII-7 character */
    bt_char =         40,

    /** IPv4 address; 
     * Natively represented as 4 bytes;
     * Sample string representation: "192.168.1.150"
     */
    bt_ipv4addr =     60,

    /** IPv4 prefix: an IPv4 address with mask;
     * Natively represented as 8 bytes: higher 4 for the IP address,
     * and the lower 4 for the netmask;
     * String representation example: "196.168.0.0/16"
     * (using mask length instead of netmask).
     */
    bt_ipv4prefix =   61,

    /** 48-bit IEEE 802 MAC address;
     * Natively represented as 6 bytes;
     * Sample string representation: "11:22:33:aa:bb:cc"
     */
    bt_macaddr802 =   62,

    /** IPv6 address;
     * Natively represented as 16 bytes (struct in6_addr);
     * Sample string representation: "fe80::21c:23ff:fec1:4fb8"
     */
    bt_ipv6addr =     63,

    /** IPv6 address with zone id;
     * Natively represented as 20 bytes (struct in6_addr followed by
     * uint32 zone_id);
     * Sample string representation: "fe80::21c:23ff:fec1:4fb0%7"
     */

    bt_ipv6addrz =    64,

    /** IPv6 prefix: an IPv6 address with masklength;
     * Natively represented as 17 bytes (struct in6_addr followed by
     * uint8 masklen);
     * Sample string representation: "2001:abcd::/32"
     */
    bt_ipv6prefix =   65,

    /** Inet address: either an IPv4 or IPv6 address (not both at once).
     * Natively represented as 18 bytes (uint16 family followed by
     * (union of struct in6_addr ; struct in_addr));
     * Sample string representation: "fe80::21c:23ff:fec1:4fb8"
     */
    bt_inetaddr =     66,

    /** Inet address: either an IPv4 or IPv6 address (not both at once),
     * with zone id;
     * Natively represented as 22 bytes (uint16 family followed by
     * (union of struct in6_addr ; struct in_addr) followed by uint32
     * zone_id);
     * Sample string representation: "fe80::21c:23ff:fec1:4fb8%99"
     */
    bt_inetaddrz =    67,

    /** Inet address prefix: either an IPv4 or IPv6 address (not both at
     *  once) with a masklength;
     * Natively represented as 19 bytes (uint16 family followed by
     * (union of struct in6_addr ; struct in_addr) followed by uint8
     * masklen);
     * Sample string representation: "fe80::21c:23ff:fec1:4fb8/64"
     */
    bt_inetprefix =   68,

    /* Date time types */

    /** Date: see ::lt_date */
    bt_date =         70,

    /** Time of day in seconds: see ::lt_time_sec */
    bt_time_sec =     71,

    /** Time of day in seconds: see ::lt_time_sec */
    bt_time =         bt_time_sec,

    /** Time of day in milliseconds: see ::lt_time_ms */
    bt_time_ms =      72,

    /** Time of day in microseconds: see ::lt_time_us */
    bt_time_us =      73,

    /** Date and time in seconds: see ::lt_time_sec */
    bt_datetime_sec = 74,

    /** Date and time in seconds: see ::lt_time_sec */
    bt_datetime =     bt_datetime_sec,

    /** Date and time in milliseconds: see ::lt_time_ms */
    bt_datetime_ms =  75,

    /** Date and time in microseconds: see ::lt_time_us */
    bt_datetime_us =  76,

    /** Elapsed time in seconds: see ::lt_dur_sec */
    bt_duration_sec = 77,

    /** Elapsed time in seconds: see ::lt_dur_sec */
    bt_duration = bt_duration_sec,

    /** Elapsed time in milliseconds: see ::lt_dur_ms */
    bt_duration_ms =  78,

    /** Elapsed time in microseconds: see ::lt_dur_us */
    bt_duration_us =  79,

    /* XXX? month names, day of month, ... */

    /* Special types, fixed size */

    /** Empty type: contains no data */
    bt_none =         90,

    /** Wildcard type: can represent any type; this is reserved for
     * internal use and will not likely come up in normal usage.
     */
    bt_any =          91,

    /** Binding type: a member of the ::bn_type enum */
    bt_btype =        92,

    /** Attribute type: a member of the ::bn_attribute_id enum */
    bt_attribute =    93,

    /* ========== Variable sized types */
    
    /* String and binary types */

    /** ASCII-7 string */
    bt_string =       101,

    bt_FIRST_VARIABLE_SIZED_TYPE = bt_string,

    /** UTF-8 string */
    bt_utf8_string =  102,

    /* XXX bt_utf8char : chose carefully fixed or variable */

    /** Binary data, may contain zeroes */
    bt_binary =       103,

    /* SNMP types */

    /** SNMP OID (Object Identifier) */
    bt_oid =          105,

    /* Special types, variable sized */

    /** Link to another binding node: essentially a binding name with
     * special semantics
     */
    bt_link =         120,

    /** Binding name */
    bt_name =         121,

    /* bt_name_and_attribute, */

    /* Other constrained string types */ 
    /* XXX what about type derivation stuff? */

    /** Hostname: each character must be alphanumeric, a period,
     * or a hyphen, but first character cannot be a hyphen.
     */
    bt_hostname =     130,

    /** URI: Uniform Resource Identifier */
    bt_uri =          131,

    /** List of ASCII-7 characters */
    bt_charlist =     132,
   
    /** Regular expression */
    bt_regex =        133,

    /** Glob pattern */
    bt_globpattern =  134,

    bt_LAST
} bn_type;


extern const lc_enum_string_map bn_type_map[];

#ifndef lc_disallow_enum_to_string
#define bn_type_to_str(t) lc_enum_to_string(bn_type_map, t)
#endif

#define bn_type_to_str_quiet_null(t) \
    lc_enum_to_string_quiet_null(bn_type_map, t)

#define bn_type_to_str_quiet_err(t, retstr) \
    lc_enum_to_string_quiet_err(bn_type_map, t, retstr)


/* Note: a flagged type is a type ORd with some bn_type_flags */


/**
 * Flags which can be used to modify a type (seldom used, and not 
 * all implemented; use caution).
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnTypeFlags.java, which
 * must be kept in sync.
 */
typedef enum {
    /**
     * On conversion between native and string format, use "pretty" 
     * representation.  Currently only meaningful for duration types
     * bt_duration_sec, bt_duration_ms, and bt_duration_us.
     */
    btf_pretty =       1 << 16,

    /** To tell snmp a type can wrap; seldom used */
    btf_counter =      1 << 17,

    /**
     * Specifies that the attribute does not contain valid data.
     * Used when a client wants to send a request which it knows is
     * invalid, so that the provider can make up the return message
     * to be consistent with other messages the provider might 
     * return in other contexts.
     */
    btf_invalid =      1 << 18,

    /**
     * Used in certain special contexts to indicate that an attribute
     * has been deleted.  e.g. in compact representations of a list of
     * changed nodes, the old attributes of a deleted node may have 
     * this type flag on them.
     */
    btf_deleted =      1 << 19,

    /**
     * Used when you don't want to specify a value, such as when an
     * attribute is in a GET request (probably wrapped in a binding).
     */
    btf_no_value =     1 << 20,

    btf_class_config = 1 << 21,
    btf_class_state =  1 << 22,
    btf_class_reg =    1 << 23,

    /**
     * Similar to btf_deleted, may be used in certain contexts to
     * indicate that an attribute has just been added.  In a change
     * notification, this may distinguish between newly-added nodes
     * and changed nodes.
     */
    btf_added =        1 << 24,

    /**
     * Indicates that this attribute contains a binary representation
     * of an attribute which was unable to be read (de-serialized) by
     * the current version of Samara.  Usually this would happen if
     * the database was written by a later version of Samara with a
     * bn_type which was not known to the current version.  The
     * converted attribute, expected to be of type bt_binary, is the
     * GCL's serialized (copyout) representation of the attribute.
     *
     * Note that if one or more of the attributes in a binding have
     * the btf_unknown_converted type flag set, it is expected that
     * the binding will also have a ba_unknown_attribs attribute
     * present, containing a boolean 'true'.
     */
    btf_unknown_converted =  1 << 25,

    /** Reserved for future use -- DO NOT USE */
    btf_RESERVED2 =    1 << 26,

    /** Reserved for future use -- DO NOT USE */
    btf_RESERVED3 =    1 << 27,

    /** Reserved for future use -- DO NOT USE */
    btf_RESERVED4 =    1 << 28,

/* ========================================================================= */
/* Customer-specific graft point 1: additional type flags.
 *
 * NOTE: bit positions 25-28 are reserved for future use by the Samara
 * base platform.  Your graft point may use bit positions 29 and 30.
 * Do not use 31, as this changes the representation of the bn_type_flags
 * enum, and will break compilation of some code.
 * =========================================================================
 */
#ifdef INC_BNODE_HEADER_INC_GRAFT_POINT
#undef BNODE_HEADER_INC_GRAFT_POINT
#define BNODE_HEADER_INC_GRAFT_POINT 1
#include "../include/bnode.inc.h"
#endif /* INC_BNODE_HEADER_INC_GRAFT_POINT */

    btf_mask =         0xffff0000U,
    /* XXX enums? */
} bn_type_flags;

/* Bit field of ::bn_type_flags ORed together */
typedef uint32 bn_type_flags_bf;

static const lc_enum_string_map bn_type_flags_map[] = {
    { btf_pretty, "pretty" },
    { btf_counter, "counter" },
    { btf_invalid, "invalid" },
    { btf_deleted, "deleted" },
    { btf_no_value, "no_value" },
    { btf_class_config, "class_config" },
    { btf_class_state, "class_state" },
    { btf_class_reg, "class_reg" },
    { btf_added, "added" },
    { btf_unknown_converted, "unknown_converted" },
    
/* ========================================================================= */
/* Customer-specific graft point 2: mapping of additional type flags to
 * strings.
 * =========================================================================
 */
#ifdef INC_BNODE_HEADER_INC_GRAFT_POINT
#undef BNODE_HEADER_INC_GRAFT_POINT
#define BNODE_HEADER_INC_GRAFT_POINT 2
#include "../include/bnode.inc.h"
#endif /* INC_BNODE_HEADER_INC_GRAFT_POINT */

    { 0, NULL }
}; /* bn_type_flags_map */

/**
 * Given a flagged type (returned from many query functions such as
 * bn_binding_get_str()), return the data type as a ::bn_type.
 */
#define bn_type_from_flagged_type(x)        ((x) & ~btf_mask)

/**
 * Given a flagged type (returned from many query functions such as
 * bn_binding_get_str()), return the type flags as a uint32,
 * which contains a bit field of ::bn_type_flags ORed together.
 */
#define bn_type_flags_from_flagged_type(x)  ((x) &  btf_mask)

/* XXX: are there other valueless types? */
#define bn_type_requires_value(x)           ((x) != bt_none)

#define bn_type_requires_substance(x)                                         \
    ((x) != bt_none && (x) < bt_FIRST_VARIABLE_SIZED_TYPE)

/* Values used in attributes */
typedef union bn_value bn_value;

typedef struct bn_attrib bn_attrib;

typedef struct bn_binding bn_binding;

/**
 * \struct bn_attrib_array
 * An instantiation of the array data type
 * where the elements are of type bn_attrib&nbsp;*.
 * Unlike most arrays which are just meant to hold any number of
 * like elements, this one is generally used to hold all of the 
 * attributes for a single binding, and a binding may not have 
 * more than one of each kind of attribute.  So the bn_attribute_id
 * is used as the index into the array.  e.g. ba_value has the value 1,
 * so the "value" attribute would always be found at position 1 in 
 * the array.
 */

/**
 * Define the bn_attrib_array data type.
 */
TYPED_ARRAY_HEADER_NEW_NONE(bn_attrib, struct bn_attrib *);

/**
 * \struct bn_binding_array
 * An instantiation of the array data type
 * where the elements are of type bn_binding&nbsp;*.
 * This is an opaque data type, but the APIs for working with
 * them are defined in bnode_types.h.
 */

/**
 * Define the bn_binding_array data type.
 */
TYPED_ARRAY_HEADER_NEW_NONE(bn_binding, struct bn_binding *);


/**
 * Convenient structure to hold stringified version of a binding value.
 */
typedef struct bn_str_value {
    const char *bv_name;
    bn_type bv_type;
    const char *bv_value;
} bn_str_value;


#include "bnode_types.h"


/* ========== Common ========== */

/**
 * @name Type classification 
 */

/*@{*/

/**
 * Tells whether or not this type dynamically allocates memory to
 * hold its value.  Essentially this is the same as asking whether
 * the type is of variable length, as the binding node data structure
 * has a union that can hold any of the fixed-length types.
 */ 
tbool bn_type_allocs(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is a numeric type on which arithmetic can be
 * performed.  This rules out certain numeric types such as IP addresses.
 */
tbool bn_type_numeric(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is an integral numeric type.  This means
 * the same as bn_type_numeric() except that any types that support
 * non-integer values (currently only bt_float32 and bt_float64)
 * are excluded.
 */
tbool bn_type_numeric_int(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is an unsigned integral numeric type.
 */
tbool bn_type_numeric_unsigned_int(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is an unsigned integral numeric type.
 */
tbool bn_type_numeric_signed_int(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is a floating point numeric type.
 */
tbool bn_type_numeric_float(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is one having to do with time: a time, date,
 * datetime, or duration.
 */
tbool bn_type_time_related(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is an ordered numeric type; that is, can
 * you compare two attributes of this type?
 */
tbool bn_type_ordered_numeric(bn_type btype, uint32 type_flags);

/**
 * Tells whether this type is an ordered numeric type for which
 * cross-type comparisons are supported; that is, can you compare two
 * attributes which both pass this test?  This is a subset of the types
 * which answer 'true' for bn_type_ordered_numeric(), since there are
 * some types for which comparison is only supported against attributes
 * of the same type.
 */
tbool bn_type_ordered_numeric_crosstype(bn_type btype, uint32 type_flags);

/** 
 * Tells whether this type is stored natively in a string representation.
 */
tbool bn_type_string(bn_type btype, uint32 type_flags);

/** 
 * Tells whether this type is stored natively in a tbuffer.
 */
tbool bn_type_tbuf(bn_type btype, uint32 type_flags);

/**
 * Flags used in bn_attrib serialization to describe the serialized byte
 * order.
 */
typedef enum {
    bee_none = 0,
    bee_fixedlen_netorder_encoding = 1,
    bee_fixedlen_native_encoding = 2,
    bee_bytestring_encoding = 4,
} bn_external_encoding;

/**
 * Returns the size of a fixed-size type,
 *
 * \param btype The data type whose size to check.
 * \param type_flags Modification flags to the type, from ::bn_type_flags.
 * \param ret_fixed_size Returns the size of the type in bytes.
 * \param ret_byte_swap Returns true if the in memory form of the type needs to
 * be put into network byte order before being serialized
 * \param ret_encoding Returns the encoding marker (netorder vs. host
 * order) used in serialization.
 */
int bn_type_fixed_size(bn_type btype, uint32 type_flags, 
                       uint32 *ret_fixed_size,
                       tbool *ret_byte_swap,
                       bn_external_encoding *ret_encoding);

/*@}*/

#define CLIST_LOWER_LETTERS   "abcdefghijklmnopqrstuvwxyz"
#define CLIST_UPPER_LETTERS   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define CLIST_LETTERS         CLIST_LOWER_LETTERS CLIST_UPPER_LETTERS
#define CLIST_NUMBERS         "0123456789"
#define CLIST_LOWER_ALPHANUM  CLIST_LOWER_LETTERS CLIST_NUMBERS
#define CLIST_UPPER_ALPHANUM  CLIST_UPPER_LETTERS CLIST_NUMBERS
#define CLIST_ALPHANUM        CLIST_LETTERS CLIST_NUMBERS

/*
 * Characters other than alphanumeric that are permitted in
 * a filename in certain contexts.
 *
 * This does not include the underscore character because they are
 * used to limit the values for strings which may be embedded in
 * larger filenames which may use underscores for separation.
 * For example, this is used for the names for CMC identities,
 * which are embedded in filenames such as id_&lt;name&gt;_rsa2.
 * We don't want an underscore in &lt;name&gt; as this complicates
 * parsing of the filename.
 */

#define CLIST_FILENAME_SPEC  "@.-"

#define CLIST_FILENAME       CLIST_ALPHANUM CLIST_FILENAME_SPEC

#define CLIST_USERNAME_SPEC  "._-"

#define CLIST_USERNAME       CLIST_ALPHANUM CLIST_USERNAME_SPEC

/*
 * Note that additionally, we cannot permit '-' as the first character.
 */
#define CLIST_HOSTNAME_NOIPV6 CLIST_ALPHANUM ".-"

/* This plus CLIST_ALPHANUM and ' ' is all the printable ACSII */
#define CLIST_PUNCT "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"


/**
 * @name Type arithmetic
 */

/*@{*/

/**
 * Computes the difference between two numeric attributes (a1 - a2)
 * and returns the result.
 *
 * a1 and a2 must be the same type.  The result will be of the next
 * highest integer size, if there is one, and will always be signed,
 * in order to be able to represent all of the possible deltas.
 *
 * If a1 and a2 are int64 or uint64 (including the various time-related
 * types that are typedef'd to int64), there is no single type large
 * enough to represent all of the possible deltas.  An int64 is used,
 * and if the delta is too large to fit, it is clamped at either
 * INT64_MIN or INT64_MAX.
 *
 * XXX/EMT: currently this is only implemented for the known-size integer
 * types, [u]int{8,16,32,64}.
 *
 * \param a1 First number.  The other number is subtracted from this
 * to compute the difference.
 *
 * \param a2 Second number.  This is subtracted from the other number
 * to compute the difference.
 *
 * \param ret_delta Place to store the result in attribute form.  
 * The result will be a dynamically-allocated attribute that the 
 * caller must free.
 *
 * \param ret_clamped The result was too big to fit into an attribute, and
 * was clamped.  This happens only when the attributes were 64-bit
 * integers, and the arithmetic overflowed an int64.
 */
int bn_type_numeric_delta(const bn_attrib *a1, const bn_attrib *a2,
                          bn_attrib **ret_delta, tbool *ret_clamped);

/**
 * Figure out the data type needed to hold the result of computing the
 * difference between two attributes of a given type, if it were
 * computed using bn_type_numeric_delta().
 *
 * \param type Type of attributes whose difference to compute.
 * \param ret_delta_type Returns the type of the attribute that would
 * hold the difference between two attributes of the specified type.
 */
int bn_type_numeric_delta_type(bn_type type, bn_type *ret_delta_type);

/**
 * For data types which pass bn_type_numeric(), return a compatible
 * type which can be used to do arithmetic with the least likelihood
 * of overflow/underflow.  If the type is floating point, or already
 * 64-bit, we just return the same type.  For integer data types of
 * less than 64 bits, we return the 64-bit integer type of the same
 * sign.
 *
 * (XXX/EMT: overflow may be possible with float32, and in some cases
 * might be prevented by upconverting to float64...)
 *
 * Note that if called with a data type that does not pass
 * bn_type_numeric(), this function will silently return
 * lc_err_bad_type.
 */
int bn_type_numeric_biggest_int_type(bn_type type, bn_type_flags_bf type_flags,
                                     bn_type *ret_biggest_type);

/**
 * Convert an attribute from the "biggest int type" returned from
 * bn_type_numeric_biggest_int_type() back to the original data type.
 * The attribute provided MUST be of the data type returned by
 * bn_type_numeric_biggest_int_type() for the specified orig_type.
 * The result of the conversion is returned in ret_a2.
 *
 * Note that if orig_type does not pass bn_type_numeric(), this
 * function will silently return lc_err_bad_type without taking any
 * other action.
 */
int
bn_type_numeric_downconvert_from_biggest(const bn_attrib *a1,
                                         bn_type orig_type,
                                         bn_type_flags_bf orig_type_flags,
                                         bn_attrib **ret_a2);

/**
 * Convert a numeric attribute into one of a signed type, either
 * representing the same value, or the negation of the original value.
 * The original attribute may be signed or unsigned.  An error will
 * result if the attribute was not of the correct type, or if the
 * original value cannot be represented in any signed type (only if
 * it was a uint64 with a value greater than 2^63-1).
 */
int bn_type_numeric_convert_to_signed(const bn_attrib *attr, tbool negate,
                                      bn_attrib **ret_new_attr);

/**
 * An arithmetic operation to be performed on a numeric data type.
 */
typedef enum {
    btno_none = 0,
    btno_add,
    btno_subtract,
    btno_multiply,
    btno_divide
} bn_type_numeric_op;

/**
 * Computes (a1 OPER a2) and stores result in a1, where OPER can be
 * either addition, subtraction, multiplication, or division.
 *
 * \param a1 First operand.  Will hold the result of the operation when
 * the function returns.
 * \param a2 Second operand.
 * \param oper Arithmetic operation to perform on a1 and a2.
 */
int bn_type_numeric_arithmetic(bn_attrib *a1, const bn_attrib *a2,
                               bn_type_numeric_op oper);



/**
 * Comparison function for comparing two numeric attributes.
 * Most of these are boolean functions, returning nonzero if the
 * comparison is true, and zero if it is false.  The only non-boolean
 * one is btncf_cmp, which returns <0 if attrib1<attrib2, 0 if 
 * attrib1==attrib2, and >0 if attrib1>attrib2.
 *
 * Note: when one or both arguments are NaNs, all of the boolean tests
 * always return false/0.  For btncf_cmp:
 *   - If both are NaN, they are not considered equal, and we arbitrarily
 *     return -1.
 *   - If only one is a NaN, NaNs are considered "high", i.e. greater than
 *     any non-NaN, even including infinity.
 */
typedef enum {
    btncf_none = 0,

    /** Greater than */
    btncf_gt = 1,

    /** Greater than or equal to */
    btncf_gteq = 2,

    /** Less than */
    btncf_lt = 3,

    /** Less than or equal to */
    btncf_lteq = 4,

    /** Equal to */
    btncf_eq = 5,

    /** Compare (non-boolean) */
    btncf_cmp = 6 
} bn_type_numeric_compare_func;

/*
 * NaN check: for all the tests except for 'cmp', we just inherit the
 * base C behavior.  For 'cmp', we are forced to choose among {-1, 0, 1}.
 * We arbitrarily consider them "high", so greater than any non-NaN,
 * including infinity.  If both are NaN, we arbitrarily return -1.
 */

/** \cond */
#define BT_COMP(comp, a, b, dest)                                             \
    do {                                                                      \
        __typeof__ (a) _a = (a);                                              \
        __typeof__ (b) _b = (b);                                              \
        switch (comp) {                                                       \
        case btncf_gt:                                                        \
            dest = _a > _b;                                                   \
            break;                                                            \
        case btncf_gteq:                                                      \
            dest = _a >= _b;                                                  \
            break;                                                            \
        case btncf_lt:                                                        \
            dest = _a < _b;                                                   \
            break;                                                            \
        case btncf_lteq:                                                      \
            dest = _a <= _b;                                                  \
            break;                                                            \
        case btncf_eq:                                                        \
            dest = _a == _b;                                                  \
            break;                                                            \
        case btncf_cmp:                                                       \
            dest = ((_a == _b) ? 0 :                                          \
                   ((_a < _b) ? -1 :                                          \
                   ((_a > _b) ? 1 :                                           \
                   ((_a != _a && _b != _b) ? -1 :                             \
                   ((_a != _a) ? 1 : -1)))));                                 \
            break;                                                            \
        default:                                                              \
            bail_force(lc_err_unexpected_case);                               \
        }                                                                     \
    } while (0)
/** \endcond */


/**
 * Compare two "ordered numeric" attributes.  As to which data types of
 * attributes are supported, there are three cases:
 *
 *   1. If the data types of both attributes pass
 *      bn_type_ordered_numeric_crosstype(), then they can be compared
 *      even if their types do not match.
 *
 *   2. Otherwise, if the data types match, and the data type shared
 *      by both attributes passes the less-restrictive
 *      bn_type_ordered_numeric(), then they can be compared.
 *
 *   3. Otherwise, the attributes cannot be compared.
 *
 * NOTE: in case 3, if the attributes cannot be compared, an error is
 * logged, but the caller cannot tell by the return value, since the
 * return value is simply the result of the comparison.  In this case,
 * 0 is always returned.
 *
 * \param attrib1 First attribute to compare.
 * \param attrib2 Second attribute to compare.
 * \param comp Comparison function to apply to these attributes.
 *
 * \retval The results of the comparison.  See ::bn_type_numeric_compare_func
 * for description of possible results.
 */
int bn_type_numeric_compare(const bn_attrib *attrib1, 
                            const bn_attrib *attrib2,
                            bn_type_numeric_compare_func comp);

/**
 * Extract the value of a numeric attribute as a float64.
 */
int bn_type_numeric_convert_to_float64(const bn_attrib *attr,
                                       float64 *ret_float);

/**
 * Create a numeric attribute from a float64.
 */
int bn_type_numeric_convert_from_float64(bn_attrib **ret_attr,
                                         bn_attribute_id attrib_id,
                                         bn_type type,
                                         uint32 type_flags,
                                         float64 value);

/**
 * Convert a numeric binding (one for which bn_type_numeric() returns
 * true) into a 64-bit number.  All signed integers will be returned
 * as int64; all unsigned integers as uint64; and all floating point
 * numbers as float64.  The two data types not used will be set to zero.
 *
 * \param attr Numeric attribute to be converted.  If it is not numeric,
 * lc_err_bad_type will be returned.
 *
 * \retval ret_int64 Int64 representation of the attribute, if it is a
 * signed integer data type; or zero otherwise.
 *
 * \retval ret_uint64 Uint64 representation of the attribute, if it is an
 * unsigned integer data type; or zero otherwise.
 *
 * \retval ret_float64 Float64 representation of the attribute, if it is a
 * floating point data type; or zero otherwise.
 *
 * \retval ret_type Which type of data was returned.  Will be either 
 * bt_int64, bt_uint64, bt_float64, or bt_NONE; with the last case only
 * coming up in an error case.
 */
int bn_type_numeric_convert_to_64bit(const bn_attrib *attr, int64 *ret_int64,
                                     uint64 *ret_uint64, float64 *ret_float64,
                                     bn_type *ret_type);

/**
 * Try to create a numeric attribute from a string, using heuristics
 * to choose the correct type for it.  We do not choose the smallest
 * type that the number will fit into, in case the caller later wants
 * to use it in arithmetic which could overflow that type.  Instead,
 * we choose the largest type appropriate for it.  Similarly, we
 * always choose a signed type if possible.  So there are four
 * possible outcomes:
 *   - If the value is an integer that fits in an int64, return an int64.
 *   - Else if the value is an unsigned integer that fits in a uint64, 
 *     return a uint64.
 *   - Else if the value is any number, return a float64.
 *   - Else silently return an error (probably lc_err_bad_type).
 */
int bn_type_numeric_convert_from_string(bn_attrib **ret_attr,
                                        bn_attribute_id attrib_id,
                                        uint32 type_flags,
                                        const char *str);

/*@}*/

/**
 * @name Address-related utilities
 */

/*@{*/

/**
 * For bn_attrib types which contain IP addresses (ipv4addr,
 * ipv4prefix, ipv6addr, ipv6addrz, ipv6prefix, inetaddr, inetaddrz,
 * inetprefix), return a string representation of the address portion
 * of the attribute value.  For all other types, return
 * lc_err_bad_type.
 */
int bn_attrib_addr_get_address_str(const bn_attrib *attrib, char **ret_str);


/**
 * For bn_attrib types which contain a mask length (ipv4prefix,
 * ipv6prefix, inetprefix), return just the mask length.  For all
 * other types, return lc_err_bad_type.
 */
int bn_attrib_addr_get_masklen(const bn_attrib *attrib, uint8 *ret_masklen);


/**
 * For bn_attrib types which contain a zone ID (ipv6addrz, inetaddrz),
 * return just the zone ID.  For all other types, return
 * lc_err_bad_type.
 */
int bn_attrib_addr_get_zone_id(const bn_attrib *attrib, uint32 *ret_zone_id);


/*@}*/

/**
 * Determine whether a string occurs in a list of valid choices.
 * \param string The string to test.
 * \param case_matters Pass \c true to do case-sensitive comparisons;
 * \c false for case-insensitive comparisons.
 * \param choices An array of all possible valid strings.
 * \retval \c true if the string was found in the list of choices,
 * or \c false otherwise.
 */
tbool bn_str_limit_choices(const char *string, 
                           tbool case_matters,
                           const tstr_array *choices);

tbool bn_str_limit_char_range(const char *str,
                              tbool case_matters,
                              const tstr_array *char_range,
                              const tstr_array *no_char_range);


int bn_attrib_array_new(bn_attrib_array **ret_arr);
int bn_attrib_array_new_size(bn_attrib_array **ret_arr, uint32 initial_size);

/*
 * Compare two attrib arrays for equality, disregarding any "auto"
 * attributes.
 */
int bn_attrib_array_equal_noauto(const bn_attrib_array *arr1,
                                 const bn_attrib_array *arr2,
                                 tbool *ret_equal);

typedef enum {
    baacf_none = 0,

    /** Do not compare 'auto' attributes */
    baacf_noauto = 1 << 0,

    /**
     * Override default semantic for NaNs (in floating point), and
     * consider a NaN equal to itself.
     */
    baacf_nan_equal_to_self = 1 << 1,

} bn_attrib_array_compare_flag;

/** Bit field of ::bn_attrib_array_compare_flag ORed together */
typedef uint32 bn_attrib_array_compare_flag_bf;

/*
 * Compare two attrib arrays, with option flags.
 *
 * (Couldn't just call this "bn_attrib_array_equal" because the typed
 * array template already took that name)
 */
int bn_attrib_array_equal_flags(const bn_attrib_array *arr1,
                                const bn_attrib_array *arr2,
                                bn_attrib_array_compare_flag_bf flags,
                                tbool *ret_equal);

int bn_attrib_array_get_attrib_tstr(const bn_attrib_array *arr,
                                    bn_attribute_id attrib_id,
                                    uint32 *ret_flagged_type,
                                    tstring **ret_value);

typedef enum {
    baac_none = 0,
    baac_min,
    baac_max,
    baac_mean,
} bn_attrib_array_calculation;

/**
 * Scan all of the attributes in the provided array, and return an
 * attribute that is an aggregation of these values.  The new value
 * may be a copy of one of the ones in the array (e.g. in the case 
 * of min and max); but it also could be a new value (e.g. in the case
 * of mean).
 *
 * NOTE: if calc is baac_mean, it is required that all of the 
 * attributes in the array have the same type and type flags.
 *
 * If the array is empty, lc_err_not_found will be returned and
 * ret_result will be set to NULL.
 */
int bn_attrib_array_calc(const bn_attrib_array *arr, 
                         bn_attrib_array_calculation calc,
                         bn_attrib **ret_result);

/**
 * @name Attribute manipulation
 */

/*@{*/

int bn_attrib_new(bn_attrib **ret_attrib);
int bn_attrib_free_value(bn_attrib *ret_attrib);
int bn_attrib_free(bn_attrib **ret_attrib);
void bn_attrib_free_for_array(void *elem);
int bn_attrib_dup(const bn_attrib *attrib, bn_attrib **ret_attrib);
int bn_attrib_dup_for_array(const void *src, void **ret_dest);
int bn_attrib_get_type(const bn_attrib *attrib, bn_type *ret_type,
                       uint32 *ret_type_flags);
int bn_attrib_get_attribute_id(const bn_attrib *attrib, 
                               bn_attribute_id *ret_attrib_id);
int bn_attrib_set_attribute_id(bn_attrib *attrib, bn_attribute_id attrib_id);

/**
 * A combination of bn_attrib_get_attribute_id() and
 * bn_attrib_get_type(), except that it doesn't tell you the type.
 * This is geared for the specific situation where you already know
 * the type, and are about to call a type-specific variant of
 * bn_attrib_set_...() to change an attribute, but don't want to change
 * its attribute ID or type flags.
 */
int bn_attrib_get_aid_tflags(const bn_attrib *attrib, 
                             bn_attribute_id *ret_attrib_id,
                             uint32 *ret_type_flags);

/**
 * Compare the two attributes and return a number less than, equal to, or
 * greater than zero if b1 is less than, equal to, or greater than b2,
 * respectively.
 * 
 * If one of the attributes is NULL, that one is considered to be greater.
 *
 * If the attributes are not of the same type, they will never be
 * considered equal (even if they represent the same number, such as a
 * uint16 and a uint32 both containing the number 500); they are
 * ordered according to their type number as determined by the bn_type
 * enum.
 *
 * Note: when one or both arguments are NaNs (as with btncf_cmp):
 *   - If both are NaN, they are not considered equal, and we arbitrarily
 *     return -1.
 *   - If only one is a NaN, NaNs are considered "high", i.e. greater than
 *     any non-NaN, even including infinity.
 */
int bn_attrib_compare(const bn_attrib *b1, const bn_attrib *b2,
                      int *ret_cmp_value);

/**
 * Tell if an attribute contains a NaN ("not a number").  For this to be
 * true, it must be (a) non-NULL, (b) a floating point type (bt_float32
 * or bt_float64), and (c) contain the special value "nan".
 */
tbool bn_attrib_is_nan(const bn_attrib *attr);


/*@}*/

int bn_binding_compare_names_for_array(const void *elem1, const void *elem2);

/**
 * @name Binding arrays
 */

/*@{*/

int bn_binding_array_new(bn_binding_array **ret_arr);
int bn_binding_array_new_size(bn_binding_array **ret_arr,
                              uint32 initial_size);

/**
 * Tell if two binding arrays are equal, in that (a) all of the
 * binding names match, and (b) all of the specified attribute match.
 * Of course, they both must also have the same number of bindings.
 *
 * Note that bn_binding_array_equal(), whose implementation we get
 * from the typed array template, only checks to see if the arrays
 * both have the same set of binding names; it does not check the
 * attributes of the bindings.
 */
int bn_binding_array_equal_one_attrib(const bn_binding_array *arr1, 
                                      const bn_binding_array *arr2,
                                      bn_attribute_id attrib_id,
                                      tbool *ret_equal);

int bn_binding_array_dump(const char *prefix, const bn_binding_array *arr,
                          int log_level);

int bn_binding_array_get_index_by_name(const bn_binding_array *arr,
                                       const char *name, int32 *ret_idx);

/**
 * Get a pointer to a binding from a binding array, given its name
 * \param arr Binding array to search.
 * \param name Name of binding to find.
 * \retval ret_binding A pointer to a binding in the array with the
 * specified name, if any.  Note that if there is more than one
 * binding in the area with this name, there is no guarantee as to
 * which one is returned (it might not be the first one).
 */
int bn_binding_array_get_binding_by_name(const bn_binding_array *arr,
                                         const char *name, 
                                         const bn_binding **ret_binding);

/**
 * Like bn_binding_array_get_binding_by_name() except the name of the
 * binding to get is formed with a printf-style format string and
 * variable argument list.
 */
int bn_binding_array_get_binding_by_name_fmt(const bn_binding_array *arr,
                                             const bn_binding **ret_binding,
                                             const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

/**
 * Like bn_binding_array_get_binding_by_name() except the name of the 
 * binding to get is specified in two parts, a parent and a relative
 * name, which are concatenated together with a '/' in between.
 */
int bn_binding_array_get_binding_by_name_rel(const bn_binding_array *arr,
                                             const char *parent_name, 
                                             const char *relative_name,
                                             const bn_binding **ret_binding);

/**
 * Get the value of the first binding in a binding array whose name
 * matches a provided binding name pattern.
 *
 * (XXX/EMT: add pointer to central description of binding name pattern 
 * format.)

 * \param arr Binding array to search.
 * \param pattern Binding name pattern to search for, e.g. "/a/b/ * /c",
 * or "d/e/ **".
 * \param start_idx The index in the array to start from.  The search
 * will be done linearly forward from this point.
 * \retval ret_idx The index at which the item was found.  If it was
 * not found, 0 is returned (this is also a valid value, so it's better
 * to check ret_name or ret_value).
 * \retval ret_name The concrete name of the binding found, if any, 
 * or NULL otherwise.
 * \retval ret_value The value of the \c ba_value attribute of the binding
 * found, if any, or NULL otherwise.
 */
int bn_binding_array_get_first_matching_value_tstr(const bn_binding_array *arr,
                                                   const char *pattern,
                                                   uint32 start_idx,
                                                   uint32 *ret_idx,
                                                   tstring **ret_name,
                                                   tstring **ret_value);

/**
 * Get a reference to the first binding in a binding array whose name
 * matches a provided binding name pattern.
 */
int bn_binding_array_get_first_matching_binding(const bn_binding_array *arr,
                                                const char *pattern,
                                                uint32 start_idx,
                                                uint32 *ret_idx,
                                               const bn_binding **ret_binding);

int bn_binding_array_detach_binding_by_name(bn_binding_array *arr,
                                            const char *name, 
                                            bn_binding **ret_binding);

/**
 * Find a binding in an binding array, and retrieve its ba_value
 * attribute as a boolean.  Three possible results:
 *
 *   - Binding not found: no error returned, found set to false, value
 *     not changed.
 *
 *   - Binding found, but not of type bt_bool: lc_err_bad_type returned,
 *     found set to true, value not changed.
 *
 *   - Binding found, of type bt_bool: no error returned, found set to
 *     true, value set to value of boolean.
 */
int bn_binding_array_get_value_tbool_by_name(const bn_binding_array *arr,
                                             const char *name, 
                                             tbool *ret_found,
                                             tbool *ret_value);

int bn_binding_array_get_value_tbool_by_name_fmt(const bn_binding_array *arr,
                                                 tbool *ret_found,
                                                 tbool *ret_value,
                                                 const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));

int bn_binding_array_get_value_tbool_by_name_rel(const bn_binding_array *arr,
                                                 const char *parent_name, 
                                                 const char *relative_name,
                                                 tbool *ret_found,
                                                 tbool *ret_value);

/**
 * Get the value of the \c ba_value attribute of a binding in the
 * specified array with the specified name.
 * \param arr The binding array to search.
 * \param name The name of the binding to search for.
 * \retval ret_type The type of the binding retrieved, if any.
 * \retval ret_value The value of the \c ba_value attribute of
 * the binding found, if any.  Note that if there is more than one
 * binding in the area with this name, there is no guarantee as to
 * which one is returned (it might not be the first one).
 */
int bn_binding_array_get_value_tstr_by_name(const bn_binding_array *arr,
                                            const char *name, 
                                            bn_type *ret_type,
                                            tstring **ret_value);

int bn_binding_array_get_value_tstr_by_name_fmt(const bn_binding_array *arr,
                                                bn_type *ret_type,
                                                tstring **ret_value, 
                                                const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));

int bn_binding_array_get_value_tstr_by_name_rel(const bn_binding_array *arr,
                                                const char *parent_name, 
                                                const char *relative_name,
                                                bn_type *ret_type,
                                                tstring **ret_value);

/**
 * For every binding in the array which matches the specified pattern,
 * extract the name_part_idx'th name part (starting from 0) from the
 * name of that binding.  Then return a tstr_array containing all of
 * the extracted strings.
 */
int bn_binding_array_get_name_part_tstr_array(const bn_binding_array *arr,
                                              const char *pattern, 
                                              uint32 name_part_idx,
                                              tstr_array **ret_tstr_arr);

/**
 * For every instance of a particular wildcard, save the string
 * representation of a specified attribute (usually ba_value) on a
 * given child of that wildcard (or of the wildcard itself), and
 * return these values in a tstr_array.
 *
 * \param bindings The array to search.  The answer will be solely
 * confined to what is found in this array; no query is sent to mgmtd.
 *
 * \param parent_name The name of the parent of the wildcard node.
 * For example, if you want to get the list of NTP peers (or the
 * values of one of the literals on each peer), you would pass
 * "/ntp/peer/address".  (Do not include a '*' for the wildcard
 * itself; we only want the wildcard's parent.)
 *
 * \param child_name The name of the child node underneath each
 * wildcard whose value to fetch, or NULL to return the values of the
 * wildcard itself.  So for the addresses of all NTP peers (continuing
 * the example from above), you'd pass NULL; or for their versions,
 * pass "version".
 *
 * \param attrib_id Which attribute to fetch.  Most commonly ba_value
 * will be used here.
 *
 * \retval ret_values The values of the bindings found in the binding
 * array matching the criteria specified in the other parameters.
 */
int bn_binding_array_get_child_values(const bn_binding_array *bindings,
                                      const char *parent_name, 
                                      const char *child_name,
                                      bn_attribute_id attrib_id,
                                      tstr_array **ret_values);

/*@}*/

/**
 * Get an array containing the last part of the names in each of the
 * bindings in the array.  e.g. if the array had three bindings named
 * "/net/interface/eth0", "/net/interface/eth1", and "/net/interface/eth2",
 * the resulting tstr_array would contain "eth0", "eth1", and "eth2".
 *
 * \param bindings The binding array whose binding names to extract.
 * \retval ret_last_name_parts The array of binding name last parts.
 */
int bn_binding_array_get_last_name_parts
    (const bn_binding_array *bindings, tstr_array **ret_last_name_parts);

/**
 * @name Binding manipulation
 */

/*@{*/

int bn_binding_new(bn_binding **ret_binding);
int bn_binding_new_named(bn_binding **ret_binding, const char *binding_name);
int bn_binding_new_attrib(bn_binding **ret_binding, const char *binding_name,
                          const bn_attrib *attrib);
int bn_binding_new_attrib_takeover(bn_binding **ret_binding,
                                   const char *binding_name,
                                   bn_attrib **inout_attrib);
int bn_binding_free(bn_binding **ret_binding);
void bn_binding_free_for_array(void *elem);

/**
 * Create a new binding, specifying the name both as a binding name
 * and in components.  We trust that the name and parts match, and
 * do not sanity-check this.  This is provided as an aid to performance;
 * if you have already broken the name down into parts, this saves
 * someone else reading the binding of doing the same later on.
 *
 * \retval ret_binding The newly-created binding.
 * \param binding_name The name for the binding as a single string.
 * \param binding_name_parts The binding name broken down into parts.
 * \param binding_name_parts_is_absolute Reflects whether the binding
 * name represented by the parts is absolute, since the leading '/' is
 * lost in the conversion to parts.
 * \param attribs Attributes to put in the new binding.
 */
int bn_binding_new_parts_components(bn_binding **ret_binding,
                                    const char *binding_name,
                                    const tstr_array *binding_name_parts,
                                    tbool binding_name_parts_is_absolute,
                                    const bn_attrib_array *attribs);

/**
 * Like bn_binding_new_parts_components() except the provided component memory is
 * taken over to produce the returned binding.
 */
int bn_binding_new_parts_components_takeover(bn_binding **ret_binding,
                                         tstring **inout_binding_name,
                                         tstr_array **inout_binding_name_parts,
                                         tbool binding_name_parts_is_absolute,
                                         bn_attrib_array **inout_attribs);


int bn_binding_new_components(bn_binding **ret_binding,
                              const char *binding_name,
                              const bn_attrib_array *attribs);
int bn_binding_dup(const bn_binding *binding, bn_binding **ret_binding);
int bn_binding_dup_for_array(const void *src, void **ret_dest);
int bn_binding_set_name(bn_binding *binding, const char *name);
int bn_binding_set_name_tstr_takeover(bn_binding *binding,
                                      tstring **inout_name);

/**
 * Set the name parts array of a binding, if it doesn't already have
 * an array.  We assume that these name parts are consistent with the
 * name, and do not compare them.  If the binding already has a name
 * parts field filled in, we do nothing.
 */
int bn_binding_set_name_parts(bn_binding *binding, 
                              const tstr_array *name_parts,
                              tbool binding_name_parts_abs);

/*
 * Makes a copy of the provided attribute
 */
int bn_binding_set_attrib(bn_binding *binding, 
                          bn_attribute_id attrib_id, 
                          const bn_attrib *ret_attrib);
int bn_binding_set_attrib_takeover(bn_binding *binding, 
                                   bn_attribute_id attrib_id, 
                                   bn_attrib **inout_attrib);

int bn_binding_get(const bn_binding *binding, 
                   const tstring **ret_name,
                   const bn_attrib_array **ret_attribs);
int bn_binding_get_name(const bn_binding *binding, 
                        const tstring **ret_name);

/**
 * Like bn_binding_get(), except we destroy the binding in the
 * process.  The name and attribute array returned are not copies,
 * but are ripped directly out of the binding provided.
 */
int bn_binding_get_takeover(bn_binding **inout_binding,
                            tstring **ret_name,
                            tstr_array **ret_name_parts,
                            tbool *ret_name_parts_abs,
                            bn_attrib_array **ret_attribs);

/**
 * If a binding already has the name parts in it, return them.
 * If it does not, return NULL.
 */
int bn_binding_get_name_parts_const(const bn_binding *binding, 
                                    const tstr_array **ret_name_parts,
                                    tbool *ret_name_is_absolute);

/**
 * If a binding already has the name parts in it, return them.
 * If it does not, compute them, update the binding, and return them.
 * Either way, the array returned is owned by the binding, and should
 * not be freed directly by the caller.
 */
int bn_binding_get_name_parts_const_force(bn_binding *binding, 
                                          const tstr_array **ret_name_parts,
                                          tbool *ret_name_is_absolute);

const tstr_array *bn_binding_get_name_parts_const_quick(const bn_binding *
                                                        binding);
int bn_binding_get_type(const bn_binding *binding, bn_attribute_id attrib_id,
                        bn_type *ret_type, uint32 *ret_type_flags);
const tstring *bn_binding_get_name_quick(const bn_binding *binding);
int bn_binding_get_name_parts(const bn_binding *binding,
                              tstr_array **ret_name_parts);
int bn_binding_get_name_last_part(const bn_binding *binding,
                                  tstring **ret_last_part);
int bn_binding_name_get_last_part(const char *name,
                                  tstring **ret_last_part);

/**
 * Sort a tstr_array, treating each entry as a binding name, and doing
 * a numeric sort on binding name components which are purely numeric.
 *
 * \param arr Array to be sorted.
 * \param flags Reserved for future use; caller must pass 0 for now.
 */
int tstr_array_sort_bname_numeric(tstr_array *arr, uint32 flags);

/**
 * Obfuscate one attribute of a binding by replacing its value with a
 * fixed value determined by the attribute type.
 *
 * \param binding The binding whose attribute is to be obfuscated.
 * \param attrib_id The id of the attribute to obfuscate.
 */
int bn_binding_obfuscate_attrib(bn_binding *binding,
                                bn_attribute_id attrib_id);

/**
 * Compare a single attribute of two bindings, without regard to the 
 * name or other attributes
 */
int bn_binding_compare_attribs(const bn_binding *b1, const bn_binding *b2,
                               bn_attribute_id attrib_id, tbool *ret_match);

/**
 * Compare the name of two bindings.  Also true if both the bindings are
 * not set, or both the bindings' names are not set.
 */
int bn_binding_compare_names(const bn_binding *b1, const bn_binding *b2,
                             tbool *ret_match);

/* ------------------------------------------------------------------------- */
/** Retrieve one or more parts of a binding name directly into tstrings.
 *
 * \param binding The binding whose name to retrieve
 *
 * \param retrieve_partial Specify behavior if any of the binding name
 * part indices requested exceeds the highest part index number
 * available (i.e. the number of parts in the binding name minus one).
 * If \c retrieve_partial is \c true, NULL will be returned for the
 * out-of-range parts, but any in-range parts will still be returned,
 * and the function's return value will indicate success.  If it is \c
 * false, NULL will be returned for all parts, and the function's
 * return value will indicate failure.
 *
 * \param num_parts_to_get The number of parts of the binding name to
 * retrieve.  This number should be equal to the number of parameters
 * following this one divided by two.  It must be at least 1.
 *
 * \param part_index_1 The index number of the first part of the name
 * to retrieve, starting counting from 0.  The behavior if the binding
 * name does not have this many parts is determined by the \c
 * retrieve_partial parameter.
 *
 * \param ret_part_1 Returns the nth part of the binding name, where n
 * was specified by the previous parameter.  This is a
 * dynamically-allocated string for which the caller assumes
 * responsibility.
 *
 * \param ... The variable-length parameter list must have an even
 * number of parameters, and must consist of pairs of (uint32) and
 * (tstring **) parameters.  Each of these retrieves an additional
 * part of the binding name.  Note that it is good defensive
 * programming to put a NULL at the end of your argument list to
 * protect against reading off the end of the list should you shorten
 * the list and forget to update num_parts_to_get.
 *
 * All of the part indices requested must be in ascending order by
 * index number.  This is to allow the function to detect and return
 * failure if it sees a zero argument in the variable-length portion,
 * as part of the defensive programming technique discussed above.
 */
int bn_binding_get_name_parts_va(const bn_binding *binding,
                                 tbool retrieve_partial,
                                 uint32 num_parts_to_get,
                                 uint32 part_index_1, tstring **ret_part_1,
                                 ...);

int bn_binding_get_name_str(const bn_binding *binding, const char **ret_name);
int bn_binding_get_num_attribs(const bn_binding *binding, 
                               uint32 *ret_num_attribs);

int bn_binding_get_attrib(const bn_binding *binding, 
                          bn_attribute_id attrib_id, 
                          const bn_attrib **ret_attrib);

int bn_binding_get_attrib_copy(const bn_binding *binding, 
                               bn_attribute_id attrib_id, 
                               bn_attrib **ret_attrib);

int bn_binding_get_attrib_detach(bn_binding *binding, 
                                 bn_attribute_id attrib_id, 
                                 bn_attrib **ret_attrib);

int bn_binding_get_attribs(const bn_binding *binding, 
                           const bn_attrib_array **ret_attribs);

int bn_binding_get_attribs_detach(bn_binding *binding, 
                                  bn_attrib_array **ret_attribs);

int bn_binding_dump(int log_level, const char *prefix,
                    const bn_binding *binding);

int bn_attrib_dump(int log_level, const char *prefix,
                   const bn_attrib *attrib);

/*@}*/

/* Invalid functions */
int bn_attrib_invalid_new(bn_attrib **ret_attrib, 
                          bn_attribute_id attrib_id,
                          uint32 type_flags);

int bn_binding_invalid_new(bn_binding **ret_binding, 
                           const char *binding_name,
                           bn_attribute_id attrib_id,
                           uint32 type_flags);

/**
 * @name Binding names
 */

/*@{*/

int bn_binding_name_to_parts(const char *name, tstr_array **ret_name_parts,
                             tbool *ret_is_absolute);

int bn_binding_name_parts_to_name(const tstr_array *name_parts,
                                  tbool is_absolute,
                                  char **ret_name);

int bn_binding_name_parts_to_name_tstr(const tstr_array *name_parts,
                                       tbool is_absolute,
                                       tstring **ret_tstr);
int bn_binding_name_escape_str(const char *name, char **ret_escaped_name);

int bn_binding_name_parts_to_name_va(tbool is_absolute, char **ret_name,
                                uint32 num_parts, const char *name_part_1,
                                ...);

int bn_binding_name_parts_partial_to_name(const tstr_array *name_parts, 
                                     uint32 start_index,
                                     int32 end_index,
                                     tbool is_absolute,
                                     char **ret_name);

int bn_binding_name_append_part(const char *name, const char *new_part,
                                char **ret_new_name);

int bn_binding_name_append_part_tstr(tstring *name, const char *new_part);

int bn_binding_name_append_parts_va(char **ret_new_name,
                                    const char *name, 
                                    uint32 num_parts, 
                                    const char *name_part_1, ...);

/**
 * The same as bn_binding_get_name_parts_va() except that it operates
 * on a binding name that has already been extracted from a binding.
 */
int bn_binding_name_to_parts_va(const char *name,
                                tbool retrieve_partial,
                                uint32 num_parts_to_get,
                                uint32 part_index_1, tstring **ret_part_1,
                                ...);

int bn_binding_name_parts_to_parts_va(tstr_array *name_parts,
                               tbool retrieve_partial,
                               uint32 num_parts_to_get,
                               uint32 part_index_1, tstring **ret_part_1, ...);

/* ------------------------------------------------------------------------- */
/** Check to see if the specified binding name matches the specified
 * binding name pattern.  The pattern may contain "*" or "**" as
 * binding name parts, as described in the comment for
 * tstr_array_wildcard_match().
 */
tbool bn_binding_name_pattern_match(const char *name, const char *pattern);

/**
 * Same as bn_binding_name_pattern_match() except that the binding name
 * is already broken down into parts for a quicker comparison.
 *
 * \param name_parts A binding name broken down into parts.
 * \param name_parts_abs Pass \c true if the name represented by
 * the parts was an absolute name (i.e. it started with a '/'); 
 * pass \c false otherwise.
 * \param pattern The binding name pattern to compare to.
 * \retval \c true if the name matches the pattern;
 * \c false if not.
 */
tbool bn_binding_name_parts_pattern_match(const tstr_array *name_parts,
                                          tbool name_parts_abs,
                                          const char *pattern);

/**
 * Like bn_binding_name_parts_pattern_match(), except that the pattern is
 * taken as a series of part strings, rather than as a single joined
 * string.  This saves the trouble of tokenizing the pattern, which
 * can bog down performance.
 *
 * The caller can also specify a start index in the name_parts array,
 * in case part of the prefix has already been checked.  Pass 0 to
 * check all of name_parts.  Note that a corresponding number of items
 * from the pattern are NOT skipped: it is assumed that the pattern
 * provided is to be compared starting at the name_parts_start_idx'th
 * element of name_parts.
 *
 * It is assumed that the name_parts and pattern parts are either both
 * absolute, or both relative.  We could have the caller tell us this
 * and us return an error if we don't like it, but it seems a tad
 * condescending to be volunteering to compare two booleans for them.
 */
tbool bn_binding_name_parts_pattern_match_va(const tstr_array *name_parts,
                                             uint32 name_parts_start_idx,
                                             uint32 num_pattern_parts,
                                             const char *pattern_part_1, ...);

/*@}*/


/* ================================================== */

/**
 * Set suboperation.  The GCL bnode API has four main types of
 * operations: query, set, action, and event.  In the case of a 
 * set operation, the suboperation tells more specifically what
 * is to be done.
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnSetSubop.java, which
 * must be kept in sync.
 *
 * NOTE: bn_set_subop_map (in bnode.c) must be kept in sync.
 */
typedef enum {
    bsso_none =     0,

    /** Create a node, likely a wildcard instance */
    bsso_create =   1,

    /** Remove a leaf or subtree, likely a wildcard instance */
    bsso_delete =   2,

    /** Reset a node/subtree to initial/default values */
    bsso_reset =    3,

    /** Change value or other attributes; tries create first */
    bsso_modify =   4, 

    /** Move or copy a node/subtree */
    bsso_reparent = 5,
} bn_set_subop;

extern const lc_enum_string_map bn_set_subop_map[];

#ifdef __cplusplus
}
#endif

#endif /* __BNODE_H_ */

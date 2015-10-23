/*
 *
 */

#ifndef __BNODE_TYPES_H_
#define __BNODE_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <netinet/in.h>
#include "common.h"
#include "tstring.h"
#include "bnode.h"
#include "addr_utils.h"

/**
 * \file src/include/bnode_types.h Binding node: API for converting 
 * between binding nodes and other data types.
 * \ingroup gcl
 */

/** \cond */

typedef struct bn_type_converter bn_type_converter;

int bn_init_type_converter(bn_type_converter **ret_converter);
int bn_get_type_for_str(const bn_type_converter *converter,
                        const char *type, bn_type *ret_type);
int bn_deinit_type_converter(bn_type_converter **inout_converter);

/** \endcond */

/* ========================================================================= */
/** @name Creating/Modifying Bindings and Attributes in String Form
 */

/*@{*/

/**
 * Create a new attribute from a string representation of the value.
 *
 * NOTE: if datetimes are created using this API, the string is
 * interpreted as localtime, not UTC.
 *
 * \param attrib_id ID of attribute to create.
 * \param type Data type of attribute to create.
 * \param type_flags Bit field of ::bn_type_flags ORed together.
 * \param str_value String representation of value to use for attribute.
 * \retval ret_attrib Newly-created attribute, in dynamically-allocated
 * memory which it is the caller's responsibility to free.
 */
int bn_attrib_new_str(bn_attrib **ret_attrib, 
                      bn_attribute_id attrib_id,
                      bn_type type,
                      uint32 type_flags,
                      const char *str_value);

/**
 * Set just the attribute ID, type, and type flags of an existing
 * attribute, without changing anything else.
 */
int bn_attrib_set_generic_nofree(bn_attrib *attrib, bn_attribute_id attrib_id,
                                 bn_type type, uint32 type_flags);


/**
 * Just set the type flags of an existing attribute, without changing
 * anything else.
 */
int bn_attrib_set_type_flags(bn_attrib *attrib, uint32 type_flags);

/**
 * Set the value of a previously-created attribute.
 *
 * NOTE: if datetimes are set using this API, the string is
 * interpreted as localtime, not UTC.
 *
 * \param attrib The attribute whose value to set.
 * \param attrib_id The ID to change this attribute to.
 * \param type The type to change this attribute to.
 * \param type_flags Bit field of ::bn_type_flags ORed together,
 * to change this attribute to.
 * \param str_value String representation of new value to use for this
 * attribute.  The old value is automatically freed by this function.
 */
int bn_attrib_set_str(bn_attrib *attrib, 
                      bn_attribute_id attrib_id,
                      bn_type type,
                      uint32 type_flags,
                      const char *str_value);

/**
 * Create a new binding and set one attribute in the binding.  Note that
 * a binding may have any number of attributes set, but this is a 
 * convenience function that only sets one, because in most cases only
 * one will need to be set.
 *
 * NOTE: if datetimes are created using this API, the string is
 * interpreted as localtime, not UTC.
 *
 * \param binding_name Name of binding to create.
 * \param attrib_id ID of the one attribute we will set in the binding
 * (usually ::ba_value).
 * \param type Data type of the one attribute we will set in the binding.
 * \param type_flags Bit field of ::bn_type_flags ORed together,
 * to modify the type of the one attribute we will set in the binding.
 * \param str_value String representation of the value for the one 
 * attribute we will set in the binding.
 * \retval ret_binding Newly-created binding, in dynamically-allocated
 * memory which it is the caller's responsibility to free.
 */
int bn_binding_new_str(bn_binding **ret_binding,
                       const char *binding_name,
                       bn_attribute_id attrib_id,
                       bn_type type,
                       uint32 type_flags,
                       const char *str_value);

/**
 * Create a new binding.  Same as bn_binding_new_str(), except that
 * we also provide the binding name broken down into parts.
 * This is simply an optimization, to save others the trouble of
 * breaking the name down into parts in case you already have it in
 * that form.  The bn_binding data structure has an optional field 
 * for holding the broken-down representation of the binding name, 
 * which can be requested by anyone looking at the binding.
 * \param binding_name Name of binding to create.
 * \param binding_name_parts Name of binding to create, broken
 * down into parts by the '/' separator.  e.g. if the binding name
 * was "/one/two/three", we'd have an array {"one", "two", "three"}.
 * \param binding_name_parts_is_absolute Tells if the binding name
 * represented in binding_name_parts is an absolute one (i.e. would
 * begin with a '/' in stringified form).
 * \param attrib_id ID of the one attribute we will set in the binding
 * (usually ::ba_value).
 * \param type Data type of the one attribute we will set in the binding.
 * \param type_flags Bit field of ::bn_type_flags ORed together,
 * to modify the type of the one attribute we will set in the binding.
 * \param str_value String representation of the value for the one 
 * attribute we will set in the binding.
 * \retval ret_binding Newly-created binding, in dynamically-allocated
 * memory which it is the caller's responsibility to free.
 */
int bn_binding_new_parts_str(bn_binding **ret_binding,
                       const char *binding_name,
                       const tstr_array *binding_name_parts,
                       tbool binding_name_parts_is_absolute,
                       bn_attribute_id attrib_id,
                       bn_type type,
                       uint32 type_flags,
                       const char *str_value);


/**
 * Create a new binding containing an obfuscated attribute of the 
 * specified type.  This also automatically sets ba_obfuscation to
 * contain a boolean 'true'.
 */
int bn_binding_new_obfuscated(bn_binding **ret_binding,
                              const char *binding_name,
                              bn_attribute_id attrib_id, bn_type type,
                              uint32 type_flags);

/**
 * Call bn_binding_new_str(); but if we get back an lc_err_bad_type error,
 * call it back again to create an "invalid" binding (using the
 * ::btf_invalid type flag) containing the string provided.  This is
 * to shift all of the burden of making up error messages to mgmtd so
 * the message can be the same whether the value was of the wrong type,
 * or of the correct type but not within range.
 */
int bn_binding_new_str_autoinval(bn_binding **ret_binding,
                                 const char *binding_name,
                                 bn_attribute_id attrib_id,
                                 bn_type type,
                                 uint32 type_flags,
                                 const char *str_value);

/**
 * Same as bn_binding_new_str_autoinval() except that we call
 * bn_binding_new_parts_str().
 */
int bn_binding_new_parts_str_autoinval(bn_binding **ret_binding,
                                       const char *binding_name,
                                       const tstr_array *binding_name_parts,
                                       tbool binding_name_parts_is_absolute,
                                       bn_attribute_id attrib_id,
                                       bn_type type,
                                       uint32 type_flags,
                                       const char *str_value);

/**
 * Set the value of a single attribute within a binding.  If that
 * attribute was already set, replace it; if not, create it.
 * No other attributes besides the one specified here are affected.
 *
 * NOTE: if datetimes are set using this API, the string is
 * interpreted as localtime, not UTC.
 */
int bn_binding_set_str(bn_binding *binding,
                       bn_attribute_id attrib_id,
                       bn_type type,
                       uint32 type_flags,
                       const char *str_value);



/**
 * Same as bn_attrib_new_str(), except takes a tstring, and only works
 * on attributes which will be stored internally as a tstring.  You can
 * pass a type to bn_type_string() to see if a type is stored as a
 * tstring.
 */
int bn_attrib_new_tstring(bn_attrib **ret_attrib, 
                          bn_attribute_id attrib_id,
                          bn_type type, uint32 type_flags, 
                          const tstring *value);

/**
 * Similar to bn_attrib_new_tstring(), except that the provided tstring
 * memory is taken over to produce the returned attribute.
 */
int bn_attrib_new_tstring_takeover(bn_attrib **ret_attrib, 
                                   bn_attribute_id attrib_id,
                                   bn_type type, uint32 type_flags, 
                                   tstring **inout_value);

/**
 * Same as bn_attrib_set_str(), except takes a tstring, and only works
 * on attributes which will be stored internally as a tstring.  You can
 * pass a type to bn_type_string() to see if a type is stored as a
 * tstring.
 */
int bn_attrib_set_tstring(bn_attrib *attrib, 
                          bn_attribute_id attrib_id,
                          bn_type type, uint32 type_flags, 
                          const tstring *value);


/**
 * Similar to bn_attrib_set_tstring(), except that the provided tstring
 * memory is taken over to produce the returned attribute.
 */
int bn_attrib_set_tstring_takeover(bn_attrib *attrib, 
                                   bn_attribute_id attrib_id,
                                   bn_type type, uint32 type_flags, 
                                   tstring **inout_value);



/*@}*/

/* ========================================================================= */
/** @name Reading Bindings and Attributes in String Form
 */

/*@{*/

/**
 * Get the value of an attribute in string form.
 * \param attrib Attribute whose value to get.
 * \param expected_type Type we expect the attribute to be (optional).
 * If the attribute is not of this type, return an error.  If you 
 * don't want to make any assertions about the type of the attribute,
 * pass ::bt_NONE (or 0).
 * \retval ret_attrib_id The ID of this attribute.
 * \retval ret_flagged_type The data type of the attribute, ORed 
 * together with any type flags.  Use bn_type_from_flagged_type()
 * to retrieve the type from this return value, or 
 * bn_type_flags_from_flagged_type() to retrieve the type flags.
 * \retval ret_str_value String representation of the value of this
 * attribute.
 */
int bn_attrib_get_str(const bn_attrib *attrib, 
                      bn_attribute_id *ret_attrib_id,
                      bn_type expected_type,
                      uint32 *ret_flagged_type,
                      char **ret_str_value);

/**
 * Same as bn_attrib_get_str() except the string value is returned 
 * wrapped in a ::tstring.
 */
int bn_attrib_get_tstr(const bn_attrib *attrib, 
                       bn_attribute_id *ret_attrib_id,
                       bn_type expected_type,
                       uint32 *ret_flagged_type,
                       tstring **ret_tstr_value);

/**
 * Same as bn_attrib_get_str() except the string value is returned 
 * in a non-zero-terminated buffer.
 * \param attrib Attribute whose value to retrieve.
 * \param expected_type Data type the attribute is expected to have,
 * or 0 if you don't want to assert anything.
 * \param buf_len Length of buffer you are providing to receive the
 * answer.
 * \param ret_buf_value Pre-allocated buffer in which the answer should
 * be placed.  Must have at least as much space as buf_len specifies.
 * \retval ret_attrib_id The ID of the attribute provided.
 * \retval ret_flagged_type Flagged type, same as with bn_attrib_get_str().
 * \retval ret_buf_value_len The number of bytes in the answer placed
 * in ret_buf_value.
 */
int bn_attrib_get_buf(const bn_attrib *attrib, 
                      bn_attribute_id *ret_attrib_id,
                      bn_type expected_type,
                      uint32 *ret_flagged_type,
                      uint32 buf_len, char *ret_buf_value, 
                      uint32 *ret_buf_value_len);

/**
 * Similar to bn_attrib_get_str() except it it can only be used on
 * attributes already stored internally as a tstring.  It then retrieves a
 * const pointer to the tstring.  As a result, this is more efficient for
 * large strings, as no memory duplication is done.  You can pass a type to
 * bn_type_string() to see if a type is stored as a tstring.
 *
 */
int bn_attrib_get_tstring_const(const bn_attrib *attrib, 
                                bn_attribute_id *ret_attrib_id,
                                bn_type expected_type,
                                uint32 *ret_flagged_type,
                                const tstring **ret_tstr_value);

/**
 * Similar to bn_attrib_get_tstring_const() except the tstring is removed
 * from the attribute, and the attribute is free'd.
 */
int bn_attrib_get_tstring_takeover(bn_attrib **inout_attrib,
                                  bn_attribute_id attrib_id,
                                  bn_type expected_type,
                                  uint32 *ret_flagged_type,
                                  tstring **ret_tstr_value);


/**
 * Same as bn_attrib_new_str(), except takes a tbuf, and only works
 * on attributes which will be stored internally as a tbuf.  You can
 * pass a type to bn_type_tbuf() to see if a type is stored as a
 * tbuf.
 */
int bn_attrib_new_tbuf(bn_attrib **ret_attrib, 
                       bn_attribute_id attrib_id,
                       bn_type type, uint32 type_flags, 
                       const tbuf *value);

/**
 * Similar to bn_attrib_new_tbuf(), except that the provided tbuf
 * memory is taken over to produce the returned attribute.
 */
int bn_attrib_new_tbuf_takeover(bn_attrib **ret_attrib, 
                                bn_attribute_id attrib_id,
                                bn_type type, uint32 type_flags, 
                                tbuf **inout_value);

/**
 * Same as bn_attrib_set_str(), except takes a tbuf, and only works
 * on attributes which will be stored internally as a tbuf.  You can
 * pass a type to bn_type_tbuf() to see if a type is stored as a
 * tbuf.
 */
int bn_attrib_set_tbuf(bn_attrib *attrib, 
                       bn_attribute_id attrib_id,
                       bn_type type, uint32 type_flags, 
                       const tbuf *value);

/**
 * Similar to bn_attrib_set_tbuf(), except that the provided tbuf
 * memory is taken over to produce the returned attribute.
 */
int bn_attrib_set_tbuf_takeover(bn_attrib *attrib,
                                bn_attribute_id attrib_id,
                                bn_type type, uint32 type_flags,
                                tbuf **inout_value);

/**
 * Similar to bn_attrib_get_str() except it it can only be used on
 * attributes already stored internally as a tbuf.  It then retrieves a
 * const pointer to the tbuf.  As a result, this is more efficient for
 * large strings, as no memory duplication is done.  You can pass a type to
 * bn_type_tbuf() to see if a type is stored as a tbuf.
 *
 */
int bn_attrib_get_tbuf_const(const bn_attrib *attrib, 
                                bn_attribute_id *ret_attrib_id,
                                bn_type expected_type,
                                uint32 *ret_flagged_type,
                                const tbuf **ret_tbuf_value);

/**
 * Similar to bn_attrib_get_tbuf_const() except the tbuf is removed
 * from the attribute, and the attribute is free'd.
 */
int bn_attrib_get_tbuf_takeover(bn_attrib **inout_attrib,
                                  bn_attribute_id attrib_id,
                                  bn_type expected_type,
                                  uint32 *ret_flagged_type,
                                  tbuf **ret_tbuf_value);


/**
 * Get the value of one attribute from a binding in string form.
 * \param binding The binding from which to extract an attribute's value.
 * \param attrib_id The ID of the attribute to read.
 * \param expected_type Type we expect the attribute to be (optional).
 * If the attribute is not of this type, return an error.  If you 
 * don't want to make any assertions about the type of the attribute,
 * pass ::bt_NONE (or 0).
 * \retval ret_flagged_type The data type of the attribute, ORed 
 * together with any type flags.  Use bn_type_from_flagged_type()
 * to retrieve the type from this return value, or 
 * bn_type_flags_from_flagged_type() to retrieve the type flags.
 * \retval ret_str_value String representation of the value of the
 * attribute.
 */
int bn_binding_get_str(const bn_binding *binding,
                       bn_attribute_id attrib_id,
                       bn_type expected_type,
                       uint32 *ret_flagged_type,
                       char **ret_str_value);

/**
 * Same as bn_binding_get_str() except the string value is returned 
 * wrapped in a ::tstring.
 */
int bn_binding_get_tstr(const bn_binding *binding,
                        bn_attribute_id attrib_id,
                        bn_type expected_type,
                        uint32 *ret_flagged_type,
                        tstring **ret_tstr_value);

/**
 * Same as bn_binding_get_str() except the string value is returned 
 * in a non-zero-terminated buffer.
 * \param binding The binding from which to extract an attribute's value.
 * \param attrib_id The ID of the attribute to read.
 * \param expected_type Data type the attribute is expected to have,
 * or 0 if you don't want to assert anything.
 * \param buf_len Length of buffer you are providing to receive the
 * answer.
 * \param ret_buf_value Pre-allocated buffer in which the answer should
 * be placed.  Must have at least as much space as buf_len specifies.
 * \retval ret_flagged_type Flagged type, same as with bn_attrib_get_str().
 * \retval ret_buf_value_len The number of bytes in the answer placed
 * in ret_buf_value.
 */
int bn_binding_get_buf(const bn_binding *binding,
                       bn_attribute_id attrib_id,
                       bn_type expected_type,
                       uint32 *ret_flagged_type,
                       uint32 buf_len, char *ret_buf_value,
                       uint32 *ret_buf_value_len);

/**
 * Similar to bn_binding_get_tstr() except it can only be used on bindings
 * where the attribute to be retrieved is already stored internally as a
 * tstring.  It then retrieves a const pointer to the tstring.  As a result,
 * this is more efficient for large strings, as no memory duplication is
 * done.  You can pass a type to bn_type_string() to see if a type is stored
 * as a tstring.
 *
 */

int bn_binding_get_tstring_const(const bn_binding *binding,
                                 bn_attribute_id attrib_id,
                                 bn_type expected_type,
                                 uint32 *ret_flagged_type,
                                 const tstring **ret_tstr_value);  

/**
 * Same as bn_binding_get_tstring_const() except the tstring is removed from
 * the specified attribute, and the attribute is removed and free'd from the
 * binding.
 */
int bn_binding_get_tstring_detach(bn_binding *inout_binding,
                                  bn_attribute_id attrib_id,
                                  bn_type expected_type,
                                  uint32 *ret_flagged_type,
                                  tstring **ret_tstr_value);

/**
 * Similar to bn_binding_get_tstring_const() except it can only be used on
 * bindings where the attribute to be retrieved is already stored internally
 * as a tbuf.  It then retrieves a const pointer to the tbuf.  As a result,
 * this is more efficient for large strings, as no memory duplication is
 * done. You can pass a type to bn_type_tbuf() to see if a type is stored as
 * a tbuf.  Currently, only bt_binary is stored in a tbuf.
 *
 */
int bn_binding_get_tbuf_const(const bn_binding *binding,
                              bn_attribute_id attrib_id,
                              bn_type expected_type,
                              uint32 *ret_flagged_type,
                              const tbuf **ret_tbuf_value);

/**
 * Same as bn_binding_get_tbuf() except the tbuf is removed from the
 * specified attribute, and the attribute is removed and free'd from the
 * binding.
 */
int bn_binding_get_tbuf_detach(bn_binding *inout_binding,
                               bn_attribute_id attrib_id,
                               bn_type expected_type,
                               uint32 *ret_flagged_type,
                               tbuf **ret_tbuf_value);

/*@}*/


/* ========================================================================= */
/* Type-specific function prototype macros
 */

#define BN_ATTRIB_NEW_PROTO(TYPE, VALDECL)               \
int bn_attrib_new_ ## TYPE (bn_attrib **ret_attrib,      \
                            bn_attribute_id attrib_id,   \
                            uint32 type_flags,           \
                            VALDECL)

#define BN_ATTRIB_NEW_SUFFIX_PROTO(TYPE, SUFFIX, VALDECL) \
int bn_attrib_new_ ## TYPE ## _ ## SUFFIX (bn_attrib **ret_attrib, \
                            bn_attribute_id attrib_id,   \
                            uint32 type_flags,           \
                            VALDECL)

#define BN_ATTRIB_SET_PROTO(TYPE, VALDECL)               \
int bn_attrib_set_ ## TYPE (bn_attrib *attrib,           \
                            bn_attribute_id attrib_id,   \
                            uint32 type_flags,           \
                            VALDECL)

#define BN_ATTRIB_SET_SUFFIX_PROTO(TYPE, SUFFIX, VALDECL) \
int bn_attrib_set_ ## TYPE ## _ ## SUFFIX (bn_attrib *attrib, \
                            bn_attribute_id attrib_id,   \
                            uint32 type_flags,           \
                            VALDECL)

#define BN_BINDING_NEW_PROTO(TYPE, VALDECL)              \
int bn_binding_new_ ## TYPE (bn_binding **ret_binding,   \
                             const char *binding_name,   \
                             bn_attribute_id attrib_id,  \
                             uint32 type_flags,          \
                             VALDECL)

#define BN_BINDING_NEW_SUFFIX_PROTO(TYPE, SUFFIX, VALDECL)       \
int bn_binding_new_ ## TYPE ## _ ## SUFFIX (bn_binding **ret_binding,   \
                             const char *binding_name,   \
                             bn_attribute_id attrib_id,  \
                             uint32 type_flags,          \
                             VALDECL)

#define BN_BINDING_NEW_PARTS_PROTO(TYPE, VALDECL)        \
int bn_binding_new_parts_ ## TYPE (bn_binding **ret_binding, \
                   const char *binding_name,             \
                   const tstr_array *binding_name_parts, \
                   tbool binding_name_parts_is_absolute, \
                   bn_attribute_id attrib_id,            \
                   uint32 type_flags,                    \
                   VALDECL)

#define BN_BINDING_NEW_PARTS_SUFFIX_PROTO(TYPE, SUFFIX, VALDECL) \
int bn_binding_new_parts_ ## TYPE ## _ ## SUFFIX (bn_binding **ret_binding, \
                   const char *binding_name,             \
                   const tstr_array *binding_name_parts, \
                   tbool binding_name_parts_is_absolute, \
                   bn_attribute_id attrib_id,            \
                   uint32 type_flags,                    \
                   VALDECL)

#define BN_BINDING_SET_PROTO(TYPE, VALDECL)              \
int bn_binding_set_ ## TYPE (bn_binding *binding,        \
                            bn_attribute_id attrib_id,   \
                            uint32 type_flags,           \
                            VALDECL)

#define BN_BINDING_SET_SUFFIX_PROTO(TYPE, SUFFIX, VALDECL)       \
int bn_binding_set_ ## TYPE ## _ ## SUFFIX (bn_binding *binding,        \
                            bn_attribute_id attrib_id,   \
                            uint32 type_flags,           \
                            VALDECL)

#define BN_ATTRIB_GET_PROTO(TYPE, RETVALDECL)                \
int bn_attrib_get_ ## TYPE (const bn_attrib *attrib,         \
                            bn_attribute_id *ret_attrib_id,  \
                            uint32 *ret_flagged_type,        \
                            RETVALDECL)

#define BN_ATTRIB_GET_SUFFIX_PROTO(TYPE, SUFFIX, RETVALDECL)  \
int bn_attrib_get_ ## TYPE ## _ ## SUFFIX (const bn_attrib *attrib,         \
                            bn_attribute_id *ret_attrib_id,  \
                            uint32 *ret_flagged_type,        \
                            RETVALDECL)

#define BN_BINDING_GET_PROTO(TYPE, RETVALDECL)               \
int bn_binding_get_ ## TYPE (const bn_binding *binding,      \
                             bn_attribute_id attrib_id,      \
                             uint32 *ret_flagged_type,       \
                             RETVALDECL)

#define BN_BINDING_GET_SUFFIX_PROTO(TYPE, SUFFIX, RETVALDECL) \
int bn_binding_get_ ## TYPE ## _ ## SUFFIX (const bn_binding *binding,      \
                             bn_attribute_id attrib_id,      \
                             uint32 *ret_flagged_type,       \
                             RETVALDECL)

#define BN_ATTRIB_NEW_PROTO_SIMPLE(TYPE) \
        BN_ATTRIB_NEW_PROTO(TYPE, TYPE value)

#define BN_ATTRIB_SET_PROTO_SIMPLE(TYPE) \
        BN_ATTRIB_SET_PROTO(TYPE, TYPE value)

#define BN_BINDING_NEW_PROTO_SIMPLE(TYPE) \
        BN_BINDING_NEW_PROTO(TYPE, TYPE value)

#define BN_BINDING_NEW_PARTS_PROTO_SIMPLE(TYPE) \
        BN_BINDING_NEW_PARTS_PROTO(TYPE, TYPE value)

#define BN_BINDING_SET_PROTO_SIMPLE(TYPE) \
        BN_BINDING_SET_PROTO(TYPE, TYPE value)

#define BN_ATTRIB_GET_PROTO_SIMPLE(TYPE) \
        BN_ATTRIB_GET_PROTO(TYPE, TYPE *ret_value)

#define BN_BINDING_GET_PROTO_SIMPLE(TYPE) \
        BN_BINDING_GET_PROTO(TYPE, TYPE *ret_value)


/** @name Operating on bindings as native types */

/*@{*/

/* XXX/EMT: expand ONE of the type-specific macros, for reference,
 * and have a comment that the others are just the same except for
 * the type.
 */

/* ========================================================================= */
/* UINT8 functions
 * Functions for working with uint8s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(uint8);
BN_ATTRIB_SET_PROTO_SIMPLE(uint8);
BN_BINDING_NEW_PROTO_SIMPLE(uint8);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(uint8);
BN_BINDING_SET_PROTO_SIMPLE(uint8);
BN_ATTRIB_GET_PROTO_SIMPLE(uint8);
BN_BINDING_GET_PROTO_SIMPLE(uint8);


/* ========================================================================= */
/* INT8 functions
 * Functions for working with int8s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(int8);
BN_ATTRIB_SET_PROTO_SIMPLE(int8);
BN_BINDING_NEW_PROTO_SIMPLE(int8);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(int8);
BN_BINDING_SET_PROTO_SIMPLE(int8);
BN_ATTRIB_GET_PROTO_SIMPLE(int8);
BN_BINDING_GET_PROTO_SIMPLE(int8);


/* ========================================================================= */
/* UINT16 functions
 * Functions for working with uint16s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(uint16);
BN_ATTRIB_SET_PROTO_SIMPLE(uint16);
BN_BINDING_NEW_PROTO_SIMPLE(uint16);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(uint16);
BN_BINDING_SET_PROTO_SIMPLE(uint16);
BN_ATTRIB_GET_PROTO_SIMPLE(uint16);
BN_BINDING_GET_PROTO_SIMPLE(uint16);


/* ========================================================================= */
/* INT16 functions
 * Functions for working with int16s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(int16);
BN_ATTRIB_SET_PROTO_SIMPLE(int16);
BN_BINDING_NEW_PROTO_SIMPLE(int16);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(int16);
BN_BINDING_SET_PROTO_SIMPLE(int16);
BN_ATTRIB_GET_PROTO_SIMPLE(int16);
BN_BINDING_GET_PROTO_SIMPLE(int16);


/* ========================================================================= */
/* UINT32 functions
 * Functions for working with uint32s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(uint32);
BN_ATTRIB_SET_PROTO_SIMPLE(uint32);
BN_BINDING_NEW_PROTO_SIMPLE(uint32);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(uint32);
BN_BINDING_SET_PROTO_SIMPLE(uint32);
BN_ATTRIB_GET_PROTO_SIMPLE(uint32);
BN_BINDING_GET_PROTO_SIMPLE(uint32);


/* ========================================================================= */
/* INT32 functions
 * Functions for working with int32s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(int32);
BN_ATTRIB_SET_PROTO_SIMPLE(int32);
BN_BINDING_NEW_PROTO_SIMPLE(int32);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(int32);
BN_BINDING_SET_PROTO_SIMPLE(int32);
BN_ATTRIB_GET_PROTO_SIMPLE(int32);
BN_BINDING_GET_PROTO_SIMPLE(int32);


/* ========================================================================= */
/* UINT64 functions
 * Functions for working with uint64s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(uint64);
BN_ATTRIB_SET_PROTO_SIMPLE(uint64);
BN_BINDING_NEW_PROTO_SIMPLE(uint64);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(uint64);
BN_BINDING_SET_PROTO_SIMPLE(uint64);
BN_ATTRIB_GET_PROTO_SIMPLE(uint64);
BN_BINDING_GET_PROTO_SIMPLE(uint64);


/* ========================================================================= */
/* INT64 functions
 * Functions for working with int64s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(int64);
BN_ATTRIB_SET_PROTO_SIMPLE(int64);
BN_BINDING_NEW_PROTO_SIMPLE(int64);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(int64);
BN_BINDING_SET_PROTO_SIMPLE(int64);
BN_ATTRIB_GET_PROTO_SIMPLE(int64);
BN_BINDING_GET_PROTO_SIMPLE(int64);


/* ========================================================================= */
/* BOOL functions
 * Functions for working with bools natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(tbool);
BN_ATTRIB_SET_PROTO_SIMPLE(tbool);
BN_BINDING_NEW_PROTO_SIMPLE(tbool);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(tbool);
BN_BINDING_SET_PROTO_SIMPLE(tbool);
BN_ATTRIB_GET_PROTO_SIMPLE(tbool);
BN_BINDING_GET_PROTO_SIMPLE(tbool);


/* ========================================================================= */
/* FLOAT32 functions
 * Functions for working with float32s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(float32);
BN_ATTRIB_SET_PROTO_SIMPLE(float32);
BN_BINDING_NEW_PROTO_SIMPLE(float32);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(float32);
BN_BINDING_SET_PROTO_SIMPLE(float32);
BN_ATTRIB_GET_PROTO_SIMPLE(float32);
BN_BINDING_GET_PROTO_SIMPLE(float32);


/* ========================================================================= */
/* FLOAT64 functions
 * Functions for working with float64s natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(float64);
BN_ATTRIB_SET_PROTO_SIMPLE(float64);
BN_BINDING_NEW_PROTO_SIMPLE(float64);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(float64);
BN_BINDING_SET_PROTO_SIMPLE(float64);
BN_ATTRIB_GET_PROTO_SIMPLE(float64);
BN_BINDING_GET_PROTO_SIMPLE(float64);


/* ========================================================================= */
/* CHAR functions
 * Functions for working with ASCII characters natively
 */

BN_ATTRIB_NEW_PROTO_SIMPLE(char);
BN_ATTRIB_SET_PROTO_SIMPLE(char);
BN_BINDING_NEW_PROTO_SIMPLE(char);
BN_BINDING_NEW_PARTS_PROTO_SIMPLE(char);
BN_BINDING_SET_PROTO_SIMPLE(char);
BN_ATTRIB_GET_PROTO_SIMPLE(char);
BN_BINDING_GET_PROTO_SIMPLE(char);

#ifndef TMSLIBS_DISABLE_IPV4_UINT32

/* ========================================================================= */
/* IPV4ADDR functions
 * Functions for working with IPv4 addresses natively, as 'uint32'.
 */

BN_ATTRIB_NEW_PROTO(ipv4addr, uint32 value);
BN_ATTRIB_SET_PROTO(ipv4addr, uint32 value);
BN_BINDING_NEW_PROTO(ipv4addr, uint32 value);
BN_BINDING_NEW_PARTS_PROTO(ipv4addr, uint32 value);
BN_BINDING_SET_PROTO(ipv4addr, uint32 value);
BN_ATTRIB_GET_PROTO(ipv4addr, uint32 *ret_value);
BN_BINDING_GET_PROTO(ipv4addr, uint32 *ret_value);

#endif /* TMSLIBS_DISABLE_IPV4_UINT32 */

/* ========================================================================= */
/* IPV4ADDR functions
 * Functions for working with IPv4 addresses natively, as 'struct in_addr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr value);
BN_BINDING_SET_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr *ret_value);

/* ========================================================================= */
/* IPV4ADDR functions
 * Functions for working with IPv4 addresses natively, as 'bn_ipv4addr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr value);
BN_BINDING_SET_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr *ret_value);

/* ========================================================================= */
/* IPV4ADDR functions
 * Functions for working with IPv4 addresses natively, as 'bn_inetaddr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr value);
BN_BINDING_SET_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr *ret_value);

#ifndef TMSLIBS_DISABLE_IPV4_UINT32

/* ========================================================================= */
/* IPV4PREFIX functions
 * Functions for working with IPv4 prefixes natively
 */

BN_ATTRIB_NEW_PROTO(ipv4prefix, uint64 value);
BN_ATTRIB_SET_PROTO(ipv4prefix, uint64 value);
BN_BINDING_NEW_PROTO(ipv4prefix, uint64 value);
BN_BINDING_NEW_PARTS_PROTO(ipv4prefix, uint64 value);
BN_BINDING_SET_PROTO(ipv4prefix, uint64 value);
BN_ATTRIB_GET_PROTO(ipv4prefix, uint64 *ret_value);
BN_BINDING_GET_PROTO(ipv4prefix, uint64 *ret_value);

#endif /* TMSLIBS_DISABLE_IPV4_UINT32 */

/* ========================================================================= */
/* IPV4PREFIX functions
 * Functions for working with IPv4 prefixes natively, as 'bn_ipv4prefix'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix value);
BN_BINDING_SET_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix *ret_value);

/* ========================================================================= */
/* IPV6ADDR functions
 * Functions for working with IPv6 addresses natively, as 'bn_ipv6addr'.
 */

BN_ATTRIB_NEW_PROTO(ipv6addr, bn_ipv6addr value);
BN_ATTRIB_SET_PROTO(ipv6addr, bn_ipv6addr value);
BN_BINDING_NEW_PROTO(ipv6addr, bn_ipv6addr value);
BN_BINDING_NEW_PARTS_PROTO(ipv6addr, bn_ipv6addr value);
BN_BINDING_SET_PROTO(ipv6addr, bn_ipv6addr value);
BN_ATTRIB_GET_PROTO(ipv6addr, bn_ipv6addr *ret_value);
BN_BINDING_GET_PROTO(ipv6addr, bn_ipv6addr *ret_value);

/* ========================================================================= */
/* IPV6ADDR functions (alias for previous block)
 * Functions for working with IPv6 addresses natively, as 'bn_ipv6addr'.
 * This is an alias, for consistency with IPv4.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr value);
BN_BINDING_SET_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr *ret_value);

/* ========================================================================= */
/* IPV6ADDR functions using 'struct in6_addr'
 * Functions for working with IPv6 addresses natively, as 'struct in6_addr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr value);
BN_BINDING_SET_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr *ret_value);

/* ========================================================================= */
/* IPV6ADDR functions using 'bn_inetaddr'
 * Functions for working with IPv6 addresses natively, as 'bn_inetaddr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr value);
BN_ATTRIB_SET_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr value);
BN_BINDING_NEW_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr value);
BN_BINDING_SET_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr value);
BN_ATTRIB_GET_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr *ret_value);

/* ========================================================================= */
/* IPV6ADDRZ functions
 * Functions for working with IPv6 zone id'd addresses natively, as
 * 'bn_ipv6addrz'.
 */

BN_ATTRIB_NEW_PROTO(ipv6addrz, bn_ipv6addrz value);
BN_ATTRIB_SET_PROTO(ipv6addrz, bn_ipv6addrz value);
BN_BINDING_NEW_PROTO(ipv6addrz, bn_ipv6addrz value);
BN_BINDING_NEW_PARTS_PROTO(ipv6addrz, bn_ipv6addrz value);
BN_BINDING_SET_PROTO(ipv6addrz, bn_ipv6addrz value);
BN_ATTRIB_GET_PROTO(ipv6addrz, bn_ipv6addrz *ret_value);
BN_BINDING_GET_PROTO(ipv6addrz, bn_ipv6addrz *ret_value);


/* ========================================================================= */
/* IPV6PREFIX functions
 * Functions for working with IPv6 prefixes natively, as 'bn_ipv6prefix'.
 */

BN_ATTRIB_NEW_PROTO(ipv6prefix, bn_ipv6prefix value);
BN_ATTRIB_SET_PROTO(ipv6prefix, bn_ipv6prefix value);
BN_BINDING_NEW_PROTO(ipv6prefix, bn_ipv6prefix value);
BN_BINDING_NEW_PARTS_PROTO(ipv6prefix, bn_ipv6prefix value);
BN_BINDING_SET_PROTO(ipv6prefix, bn_ipv6prefix value);
BN_ATTRIB_GET_PROTO(ipv6prefix, bn_ipv6prefix *ret_value);
BN_BINDING_GET_PROTO(ipv6prefix, bn_ipv6prefix *ret_value);


/* ========================================================================= */
/* INETADDR functions
 * Functions for working with Inet addresses natively, as 'bn_inetaddr'.
 */

BN_ATTRIB_NEW_PROTO(inetaddr, bn_inetaddr value);
BN_ATTRIB_SET_PROTO(inetaddr, bn_inetaddr value);
BN_BINDING_NEW_PROTO(inetaddr, bn_inetaddr value);
BN_BINDING_NEW_PARTS_PROTO(inetaddr, bn_inetaddr value);
BN_BINDING_SET_PROTO(inetaddr, bn_inetaddr value);
BN_ATTRIB_GET_PROTO(inetaddr, bn_inetaddr *ret_value);
BN_BINDING_GET_PROTO(inetaddr, bn_inetaddr *ret_value);

/* ========================================================================= */
/* INETADDR functions
 * Functions for working with Inet addresses natively, as 'bn_ipv4addr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr value);
BN_ATTRIB_SET_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr value);
BN_BINDING_NEW_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr value);
BN_BINDING_SET_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr value);
BN_ATTRIB_GET_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr *ret_value);

/* ========================================================================= */
/* INETADDR functions
 * Functions for working with Inet addresses natively, as 'bn_ipv6addr'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr value);
BN_ATTRIB_SET_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr value);
BN_BINDING_NEW_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr value);
BN_BINDING_SET_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr value);
BN_ATTRIB_GET_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr *ret_value);

/* ========================================================================= */
/* INETADDRZ functions
 * Functions for working with Inet zone id'd addresses natively, as
 * 'bn_inetaddrz'.
 */

BN_ATTRIB_NEW_PROTO(inetaddrz, bn_inetaddrz value);
BN_ATTRIB_SET_PROTO(inetaddrz, bn_inetaddrz value);
BN_BINDING_NEW_PROTO(inetaddrz, bn_inetaddrz value);
BN_BINDING_NEW_PARTS_PROTO(inetaddrz, bn_inetaddrz value);
BN_BINDING_SET_PROTO(inetaddrz, bn_inetaddrz value);
BN_ATTRIB_GET_PROTO(inetaddrz, bn_inetaddrz *ret_value);
BN_BINDING_GET_PROTO(inetaddrz, bn_inetaddrz *ret_value);

/* ========================================================================= */
/* INETADDRZ functions
 * Functions for working with Inet zone id'd addresses natively, as
 * 'bn_ipv6addrz'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz value);
BN_ATTRIB_SET_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz value);
BN_BINDING_NEW_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz value);
BN_BINDING_SET_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz value);
BN_ATTRIB_GET_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz *ret_value);

/* ========================================================================= */
/* INETPREFIX functions
 * Functions for working with Inet prefixes natively, as 'bn_inetprefix'.
 */

BN_ATTRIB_NEW_PROTO(inetprefix, bn_inetprefix value);
BN_ATTRIB_SET_PROTO(inetprefix, bn_inetprefix value);
BN_BINDING_NEW_PROTO(inetprefix, bn_inetprefix value);
BN_BINDING_NEW_PARTS_PROTO(inetprefix, bn_inetprefix value);
BN_BINDING_SET_PROTO(inetprefix, bn_inetprefix value);
BN_ATTRIB_GET_PROTO(inetprefix, bn_inetprefix *ret_value);
BN_BINDING_GET_PROTO(inetprefix, bn_inetprefix *ret_value);

/* ========================================================================= */
/* INETPREFIX functions
 * Functions for working with Inet prefixes natively, as 'bn_ipv4prefix'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix value);
BN_ATTRIB_SET_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix value);
BN_BINDING_NEW_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix value);
BN_BINDING_SET_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix value);
BN_ATTRIB_GET_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix *ret_value);

/* ========================================================================= */
/* INETPREFIX functions
 * Functions for working with Inet prefixes natively, as 'bn_ipv6prefix'.
 */

BN_ATTRIB_NEW_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix value);
BN_ATTRIB_SET_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix value);
BN_BINDING_NEW_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix value);
BN_BINDING_NEW_PARTS_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix value);
BN_BINDING_SET_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix value);
BN_ATTRIB_GET_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix *ret_value);
BN_BINDING_GET_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix *ret_value);

/* ========================================================================= */
/* MACADDR802 functions
 * Functions for working with IEEE802 addresses natively
 */

BN_ATTRIB_NEW_PROTO(macaddr802, const uint8 value[6]);
BN_ATTRIB_SET_PROTO(macaddr802, const uint8 value[6]);
BN_BINDING_NEW_PROTO(macaddr802, const uint8 value[6]);
BN_BINDING_NEW_PARTS_PROTO(macaddr802, const uint8 value[6]);
BN_BINDING_SET_PROTO(macaddr802, const uint8 value[6]);
BN_ATTRIB_GET_PROTO(macaddr802, uint8 ret_value[6]);
BN_BINDING_GET_PROTO(macaddr802, uint8 ret_value[6]);


/* ========================================================================= */
/* DATE functions
 * Functions for working with dates natively
 */

BN_ATTRIB_NEW_PROTO(date, lt_time_sec value);
BN_ATTRIB_SET_PROTO(date, lt_time_sec value);
BN_BINDING_NEW_PROTO(date, lt_time_sec value);
BN_BINDING_NEW_PARTS_PROTO(date, lt_time_sec value);
BN_BINDING_SET_PROTO(date, lt_time_sec value);
BN_ATTRIB_GET_PROTO(date, lt_time_sec *ret_value);
BN_BINDING_GET_PROTO(date, lt_time_sec *ret_value);


/* ========================================================================= */
/* TIME_SEC functions
 * Functions for working with time_secs natively
 */

BN_ATTRIB_NEW_PROTO(time_sec, lt_time_sec value);
BN_ATTRIB_SET_PROTO(time_sec, lt_time_sec value);
BN_BINDING_NEW_PROTO(time_sec, lt_time_sec value);
BN_BINDING_NEW_PARTS_PROTO(time_sec, lt_time_sec value);
BN_BINDING_SET_PROTO(time_sec, lt_time_sec value);
BN_ATTRIB_GET_PROTO(time_sec, lt_time_sec *ret_value);
BN_BINDING_GET_PROTO(time_sec, lt_time_sec *ret_value);


/* ========================================================================= */
/* TIME_MS functions
 * Functions for working with time_mss natively
 */

BN_ATTRIB_NEW_PROTO(time_ms, lt_time_ms value);
BN_ATTRIB_SET_PROTO(time_ms, lt_time_ms value);
BN_BINDING_NEW_PROTO(time_ms, lt_time_ms value);
BN_BINDING_NEW_PARTS_PROTO(time_ms, lt_time_ms value);
BN_BINDING_SET_PROTO(time_ms, lt_time_ms value);
BN_ATTRIB_GET_PROTO(time_ms, lt_time_ms *ret_value);
BN_BINDING_GET_PROTO(time_ms, lt_time_ms *ret_value);


/* ========================================================================= */
/* TIME_US functions
 * Functions for working with time_uss natively
 */

BN_ATTRIB_NEW_PROTO(time_us, lt_time_us value);
BN_ATTRIB_SET_PROTO(time_us, lt_time_us value);
BN_BINDING_NEW_PROTO(time_us, lt_time_us value);
BN_BINDING_NEW_PARTS_PROTO(time_us, lt_time_us value);
BN_BINDING_SET_PROTO(time_us, lt_time_us value);
BN_ATTRIB_GET_PROTO(time_us, lt_time_us *ret_value);
BN_BINDING_GET_PROTO(time_us, lt_time_us *ret_value);


/* ========================================================================= */
/* DATETIME_SEC functions
 * Functions for working with datetime_secs natively
 */

BN_ATTRIB_NEW_PROTO(datetime_sec, lt_time_sec value);
BN_ATTRIB_SET_PROTO(datetime_sec, lt_time_sec value);
BN_BINDING_NEW_PROTO(datetime_sec, lt_time_sec value);
BN_BINDING_NEW_PARTS_PROTO(datetime_sec, lt_time_sec value);
BN_BINDING_SET_PROTO(datetime_sec, lt_time_sec value);
BN_ATTRIB_GET_PROTO(datetime_sec, lt_time_sec *ret_value);
BN_BINDING_GET_PROTO(datetime_sec, lt_time_sec *ret_value);


/* ========================================================================= */
/* DATETIME_MS functions
 * Functions for working with datetime_mss natively
 */

BN_ATTRIB_NEW_PROTO(datetime_ms, lt_time_ms value);
BN_ATTRIB_SET_PROTO(datetime_ms, lt_time_ms value);
BN_BINDING_NEW_PROTO(datetime_ms, lt_time_ms value);
BN_BINDING_NEW_PARTS_PROTO(datetime_ms, lt_time_ms value);
BN_BINDING_SET_PROTO(datetime_ms, lt_time_ms value);
BN_ATTRIB_GET_PROTO(datetime_ms, lt_time_ms *ret_value);
BN_BINDING_GET_PROTO(datetime_ms, lt_time_ms *ret_value);


/* ========================================================================= */
/* DATETIME_US functions
 * Functions for working with datetime_uss natively
 */

BN_ATTRIB_NEW_PROTO(datetime_us, lt_time_us value);
BN_ATTRIB_SET_PROTO(datetime_us, lt_time_us value);
BN_BINDING_NEW_PROTO(datetime_us, lt_time_us value);
BN_BINDING_NEW_PARTS_PROTO(datetime_us, lt_time_us value);
BN_BINDING_SET_PROTO(datetime_us, lt_time_us value);
BN_ATTRIB_GET_PROTO(datetime_us, lt_time_us *ret_value);
BN_BINDING_GET_PROTO(datetime_us, lt_time_us *ret_value);


/* ========================================================================= */
/* DURATION_SEC functions
 * Functions for working with duration_secs natively
 */

BN_ATTRIB_NEW_PROTO(duration_sec, lt_dur_sec value);
BN_ATTRIB_SET_PROTO(duration_sec, lt_dur_sec value);
BN_BINDING_NEW_PROTO(duration_sec, lt_dur_sec value);
BN_BINDING_NEW_PARTS_PROTO(duration_sec, lt_dur_sec value);
BN_BINDING_SET_PROTO(duration_sec, lt_dur_sec value);
BN_ATTRIB_GET_PROTO(duration_sec, lt_dur_sec *ret_value);
BN_BINDING_GET_PROTO(duration_sec, lt_dur_sec *ret_value);


/* ========================================================================= */
/* DURATION_MS functions
 * Functions for working with duration_mss natively
 */

BN_ATTRIB_NEW_PROTO(duration_ms, lt_dur_ms value);
BN_ATTRIB_SET_PROTO(duration_ms, lt_dur_ms value);
BN_BINDING_NEW_PROTO(duration_ms, lt_dur_ms value);
BN_BINDING_NEW_PARTS_PROTO(duration_ms, lt_dur_ms value);
BN_BINDING_SET_PROTO(duration_ms, lt_dur_ms value);
BN_ATTRIB_GET_PROTO(duration_ms, lt_dur_ms *ret_value);
BN_BINDING_GET_PROTO(duration_ms, lt_dur_ms *ret_value);


/* ========================================================================= */
/* DURATION_US functions
 * Functions for working with duration_uss natively
 */

BN_ATTRIB_NEW_PROTO(duration_us, lt_dur_us value);
BN_ATTRIB_SET_PROTO(duration_us, lt_dur_us value);
BN_BINDING_NEW_PROTO(duration_us, lt_dur_us value);
BN_BINDING_NEW_PARTS_PROTO(duration_us, lt_dur_us value);
BN_BINDING_SET_PROTO(duration_us, lt_dur_us value);
BN_ATTRIB_GET_PROTO(duration_us, lt_dur_us *ret_value);
BN_BINDING_GET_PROTO(duration_us, lt_dur_us *ret_value);


/* ========================================================================= */
/* TYPE functions
 * Functions for working with types natively
 */

BN_ATTRIB_NEW_PROTO(btype, bn_type value);
BN_ATTRIB_SET_PROTO(btype, bn_type value);
BN_BINDING_NEW_PROTO(btype, bn_type value);
BN_BINDING_NEW_PARTS_PROTO(btype, bn_type value);
BN_BINDING_SET_PROTO(btype, bn_type value);
BN_ATTRIB_GET_PROTO(btype, bn_type *ret_value);
BN_BINDING_GET_PROTO(btype, bn_type *ret_value);


/* ========================================================================= */
/* ATTRIBUTE functions
 * Functions for working with attributes natively
 */

BN_ATTRIB_NEW_PROTO(attribute, bn_attribute_id value);
BN_ATTRIB_SET_PROTO(attribute, bn_attribute_id value);
BN_BINDING_NEW_PROTO(attribute, bn_attribute_id value);
BN_BINDING_NEW_PARTS_PROTO(attribute, bn_attribute_id value);
BN_BINDING_SET_PROTO(attribute, bn_attribute_id value);
BN_ATTRIB_GET_PROTO(attribute, bn_attribute_id *ret_value);
BN_BINDING_GET_PROTO(attribute, bn_attribute_id *ret_value);


/* ========================================================================= */
/* OID functions
 * Functions for working with OIDs natively
 */

/* Same as MAX_OID_LEN in NET-SNMP's headers */
#define BN_MAX_OID_LEN 128

/**
 * Get an OID in native form out of an attribute.
 *
 * \param attrib
 * \retval ret_attrib_id
 * \retval ret_flagged_type
 * \param max_oid_len The size of the ret_oid buffer.  This is expressed
 * in subids, NOT in bytes.
 * \retval ret_oid Native representation of the OID.
 * \retval ret_oid_len The number of subids in the OID returned.
 * \retval ret_oid_abs Reflects whether or not the OID was absolute.
 * An OID beginning with '.' is considered to be absolute; otherwise
 * it is relative.
 */
int bn_attrib_get_oid(const bn_attrib *attrib, bn_attribute_id *ret_attrib_id,
                      uint32 *ret_flagged_type, uint32 max_oid_len, 
                      uint32 ret_oid[], uint32 *ret_oid_len, 
                      tbool *ret_oid_abs);

/**
 * Like bn_attrib_get_oid(), but extracts from a binding.
 */
int bn_binding_get_oid(const bn_binding *binding, bn_attribute_id attrib_id,
                       uint32 *ret_flagged_type, uint32 max_oid_len, 
                       uint32 ret_oid[], uint32 *ret_oid_len,
                       tbool *ret_oid_abs);

/**
 * Create a new attribute with an OID in native form.
 *
 * \retval ret_attrib Newly created attribute.
 * \param attrib_id Attribute ID.
 * \param type_flags Type flags, see ::bn_type_flags.
 * \param oid OID to set, with one subid per uint32 array element.
 * \param oid_len The length of the OID to set, in number of subids.
 * The 'oid' parameter is expected to have this number of elements;
 * any after this number are ignored.
 * \param oid_abs Specify 'true' for an absolute OID, and 'false' for a 
 * relative one.  In terms of string representation, 'true' will make 
 * it prefixed with a '.' character.
 */
int bn_attrib_new_oid(bn_attrib **ret_attrib, bn_attribute_id attrib_id,
                      uint32 type_flags, uint32 oid[], uint32 oid_len,
                      tbool oid_abs);

int bn_attrib_set_oid(bn_attrib *attrib, bn_attribute_id attrib_id,
                      uint32 type_flags, uint32 oid[], uint32 oid_len,
                      tbool oid_abs);

int bn_binding_new_oid(bn_binding **ret_binding, const char *bname,
                       bn_attribute_id attrib_id, uint32 type_flags, 
                       uint32 oid[], uint32 oid_len, tbool oid_abs);

int bn_binding_set_oid(bn_binding *binding, bn_attribute_id attrib_id,
                       uint32 type_flags, uint32 oid[], uint32 oid_len,
                       tbool oid_abs);

int bn_binding_new_parts_oid(bn_binding **ret_binding,
                             const char *binding_name,
                             const tstr_array *binding_name_parts,
                             tbool binding_name_parts_is_absolute,
                             bn_attribute_id attrib_id,
                             uint32 type_flags,
                             uint32 oid[], uint32 oid_len, tbool oid_abs);

/* ========================================================================= */
/* BINARY functions
 * Functions for working with binary tbuf values natively
 */

BN_ATTRIB_NEW_PROTO(binary, const tbuf *value);
BN_ATTRIB_SET_PROTO(binary, const tbuf *value);
BN_BINDING_NEW_PROTO(binary, const tbuf *value);
BN_BINDING_NEW_PARTS_PROTO(binary, const tbuf *value);
BN_BINDING_SET_PROTO(binary, const tbuf *value);
BN_ATTRIB_GET_PROTO(binary, tbuf **ret_value);
BN_BINDING_GET_PROTO(binary, tbuf **ret_value);

/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* __BNODE_TYPES_H_ */

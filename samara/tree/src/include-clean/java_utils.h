/*
 *
 * java_utils.h
 *
 *
 *
 */

#ifndef __JAVA_UTILS_H_
#define __JAVA_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup java LibTjava: Java JNI support utilities */

/* ------------------------------------------------------------------------- */
/** \file src/include/java_utils.h Java native code helpers.
 * \ingroup java
 */

#include <jni.h>
#include "common.h"
#include "bnode.h"
#include "bnode_proto.h"

#define java_constructor_name   "<init>"
#define java_typecode_long      "J"
#define java_typecode_void      "V"

/**
 * Function to be called once for every element in an ArrayList passed to
 * java_ArrayList_foreach().
 */
typedef int (*java_ArrayList_foreach_func)(JNIEnv *env, jobject jarrayList,
                                           jobject jarrayElem, int idx,
                                           int num_elems, void *callback_data);

/**
 * @name Error handling
 */

/*@{*/

#if 0
/**
 * Throw an Java exception.
 *
 * NOT YET IMPLEMENTED.
 *
 * \param env JNI environment.
 * \param exception_name Name of the subclass of Exception to instantiate.
 * \param exception_str String to be passed to the exception constructor.
 */
void java_throw_exception(JNIEnv *env, const char *exception_name,
                          const char *exception_str);
#endif

/**
 * Throw a Java BadTypeException.
 *
 * \param env JNI environment.
 * \param btype The data type that was expected.
 * \param fmt A printf-style format string that will be used in
 * conjunction with the following parameter to construct a string
 * representation of the bad value given.  Since there is only one bad
 * value, this should have only one format code.
 * \param ... This should be only one argument: the bad value given.
 */
void java_throw_bad_type_exception(JNIEnv *env, bn_type btype, 
                                   const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));


/**
 * If err is nonzero, create an Exception object and throw it.
 * Intended to be always called from the bail clause of a native method
 * implementation.
 */
void java_maybe_throw_exception(JNIEnv *env, int throw_err);

/**
 * If an exception is pending, catch it, log it, and turn it into an
 * error code to return.  If there was both an error and an exception,
 * the exception takes precedence.  This is meant to be used from the
 * bail clause of some code that may have called into Java, but will
 * not immediately be returning to Java, such as the GCL callbacks in
 * the GclEventThread.
 */
void java_maybe_catch_exception(JNIEnv *env, int *inout_err);

/**
 * Throw an exception that tells us the method is not yet implemented.
 */
void java_throw_nyi_exception(JNIEnv *env);

/*@}*/

/**
 * @name Miscellaneous Java helpers
 */

/*@{*/

/**
 * Instantiate a Java object.
 */
int java_new_object(JNIEnv *env, const char *class_name, 
                    const char *constr_sig, jobject *ret_obj, ...);

/**
 * Convert an integer into a Java enum of a specified class.
 * This relies on the class having a FromInt() method we can call,
 * since the Java language expressly forbids converting from int to
 * enum, and really tries to make it difficult.
 */
int java_int_to_Enum(JNIEnv *env, const char *enum_class_name, int enum_value, 
                     jobject *ret_jenum);

/**
 * Get the integer equivalent of a Java-based enum type.
 * (This does not use the java.lang.Enum API, but a convention
 * that we follow with our enums, which have a getValue() method.)
 */
int java_Enum_to_int(JNIEnv *env, jobject jenum, int *ret_enum_value);

/**
 * Get the integer contents of a bit field class.  This is different from
 * java_Enum_to_int because enums have getValue(), while our bit fields
 * have only a public field; and that field is a long instead of an int.
 */
int java_BF_to_long(JNIEnv *env, jobject jbf, long *ret_bf_value);

/*@}*/

/**
 * @name Java method and field manipulation
 */

/*@{*/

/**
 * Get the ID of a field in the specified object.
 * This is a thin wrapper around GetObjectClass() and GetFieldID(),
 * to make code slightly more compact.
 */
int java_get_field_id(JNIEnv *env, jobject jobj, const char *field_name,
                      const char *field_type_sig, jfieldID *ret_field_id);

/**
 * Get the ID of a method of the specified object.
 * This is a thin wrapper around GetObjectClass() and GetMethodID(),
 * to make code slightly more compact.
 */
int java_get_method_id(JNIEnv *env, jobject jobj, const char *method_name,
                       const char *method_sig, jmethodID *ret_method_id);

/**
 * Get a field of an object, assuming that field is a Java object.
 */
int java_get_object_field(JNIEnv *env, jobject jobj, const char *field_name,
                          const char *field_type_sig, jobject *ret_obj_field);

/**
 * Fetch a C pointer from a specified field in the provided object.
 * The field is assumed to be of the Java type "long".  The pointer is
 * returned as a 'void *', and the caller can cast it however desired.
 */
int java_get_field_ptr(JNIEnv *env, jobject jobj, const char *field_name,
                       void **ret_ptr);

/**
 * Set a C pointer into a specified field in the provided object,
 * which is assumed to be of the Java type "long".
 */
int java_set_field_ptr(JNIEnv *env, jobject jobj, const char *field_name,
                       void *ptr);

/*@}*/

/**
 * @name String conversion
 */

/*@{*/

/**
 * Get a constant C string with the contents of a Java String object.
 * This is a thin wrapper around GetStringUTFChars() to make it slightly
 * more convenient.  Must call java_free_string() in the bail clause
 * to relinquish this pointer (even though it is const).
 */
int java_get_string(JNIEnv *env, jstring jstr, const char **ret_str);

/**
 * Free any resources associated with a C representation of a Java
 * String.  Call this in your bail clause of any function that has
 * called java_get_string().  If the string provided is NULL, nothing
 * happens.  (Note: do NOT use this on String objects from
 * java_create_string(); those are assumed to be garbage-collected,
 * and so have no free function.)
 *
 * Your C string will be set to NULL, to make multiple calls to this
 * function with the same parameters harmless (in case Java does not
 * gracefully deal with double-frees).
 */
void java_free_string(JNIEnv *env, jstring jstr, const char **inout_cstr);

/**
 * Creates a Java String object out of a C string.  The C string is
 * not disturbed.
 */
int java_create_string(JNIEnv *env, const char *cstr, jstring *ret_jstr);

/**
 * Calls a Java Object's toString() method, and returns the result as a
 * C string.  Unlike with java_get_string(), the caller assumes ownership
 * of the string returned.
 */
int java_Object_toString(JNIEnv *env, jobject jobj, tstring **ret_str);

/**
 * Calls toString() on every object in an object array, and returns
 * a concatenation of all of the results.
 *
 * \param env JNI environment.
 * \param jarr The object array to convert to a string.
 * \param separator A string to insert between the string representations
 * of every object in the array.
 * \retval ret_str The resulting string.
 */
int java_ObjectArray_toString(JNIEnv *env, jobjectArray jarr, 
                              const char *separator, tstring **ret_str);

/*@}*/

/**
 * @name Array manipulation
 */

/*@{*/

/**
 * Call a callback function for every element in a Java ArrayList.
 */
int java_ArrayList_foreach(JNIEnv *env, jobject jarrayList, 
                           java_ArrayList_foreach_func callback,
                           void *callback_data, int *ret_num_elems);

/**
 * Append a Java Object to a Java ArrayList.
 */
int java_ArrayList_append_Object(JNIEnv *env, jobject jsarr, jobject jobj);

/**
 * Append a C string to a Java ArrayList<String>.
 */
int java_ArrayListString_append_str(JNIEnv *env, jobject jsarr,
                                    const char *str);

/**
 * Convert a C tstr_array into a Java ArrayList<String>.
 */
int java_tstr_array_to_ArrayListString(JNIEnv *env, const tstr_array *tsarr,
                                       jobject *ret_jtsarr);

/**
 * Convert a Java ArrayList<String> into a C tstr_array.
 */
int java_ArrayListString_to_tstr_array(JNIEnv *env, jobject jtsarr,
                                       tstr_array **ret_tsarr);

/*@}*/

/**
 * @name Bnode helpers
 */

/*@{*/

/**
 * Special case of java_get_field_ptr() to get the bn_request inside
 * a BnRequest.
 */
int java_bn_request_get_ptr(JNIEnv *env, jobject jobj, bn_request **ret_req);

/**
 * Special case of java_get_field_ptr() to get the bn_response inside
 * a BnResponse.
 */
int java_bn_response_get_ptr(JNIEnv *env, jobject jobj, 
                             bn_response **ret_resp);

/**
 * Special case of java_get_field_ptr() to get the bn_attrib inside
 * a BnAttrib.
 */
int java_bn_attrib_get_ptr(JNIEnv *env, jobject jobj, bn_attrib **ret_attr);

/**
 * Create a new, empty instance of ArrayList<BnBinding>.
 */
int java_ArrayListBnBinding_new(JNIEnv *env, jobject *ret_jbarr);

/**
 * Convert a C binding name into a Java Bname.  Either name, or name
 * parts, or both may be provided, and we will make the most efficient
 * use of them.
 */
int java_bname_to_Bname(JNIEnv *env, const char *bname,
                        const tstr_array *bname_parts, tbool bname_parts_abs,
                        jobject *ret_jbname);

/**
 * Convert a C bn_attrib into a Java BnAttrib, taking over ownership
 * of the bn_attrib in the process.
 */
int java_bn_attrib_to_BnAttrib_takeover(JNIEnv *env, bn_attrib **inout_attrib,
                                        jobject *ret_jattrib);

/**
 * Convert a C bn_binding into a Java BnBinding, taking over ownership
 * of the bn_binding in the process.
 */
int java_bn_binding_to_BnBinding_takeover(JNIEnv *env,
                                          bn_binding **inout_binding,
                                          jobject *ret_jbind);

/**
 * Convert a C bn_binding_array into a Java ArrayList<BnBinding>,
 * taking over ownership of all bn_bindings in the process.
 */
int java_bn_binding_array_to_ArrayListBnBinding_takeover
    (JNIEnv *env, bn_binding_array **inout_barr, jobject *ret_jbarr);

/**
 * Convert a Java BnBinding into a C bn_binding.
 *
 * XXX/EMT: currently makes a copy of the underlying binding, so as to 
 * leave the Java binding undisturbed. Could be more efficient to steal
 * the Java binding's attribs, and leave it bereft.
 */
int java_BnBinding_to_bn_binding(JNIEnv *env, jobject jbind,
                                 bn_binding **ret_binding);

/**
 * Append a C bn_binding onto the end of a Java ArrayList<BnBinding>,
 * taking over ownership of the bn_binding in the process.
 */
int java_ArrayListBnBinding_append_bn_binding_takeover(JNIEnv *env,
                                                       jobject jbarr,
                                                       bn_binding **
                                                       inout_binding);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __JAVA_UTILS_H_ */

/*
 * $Id: dia_avp_dict_api.h 674749 2014-10-03 18:47:53Z vitaly $
 *
 * dia_avp_dict_api.h - DIAMETER Dictionary API header file. 
 *
 * Subash T Comerica, July 2011
 *
 * Copyright (c) 2011-2013, Juniper Networks, Inc.
 * All rights reserved.
 */
#ifndef __DIA_AVP_DICT_API_H__
#define __DIA_AVP_DICT_API_H__

/* max number of types of attributes present across dictionaries */
#define DIAMETER_MAX_ORDINALS       400

typedef struct diameter_avp_dictionary_s* diameter_avp_dict_handle;

typedef void*   diameter_avp_itable_handle;

/* DIAMETER Formats */
typedef enum diameter_avp_format_s {
    /* RFC 3588 Basic Formats start here */
    DIAMETER_FORMAT_NULL,
    DIAMETER_FORMAT_OCTETSTRING,
    DIAMETER_FORMAT_UNSIGNED16,
    DIAMETER_FORMAT_INTEGER32,
    DIAMETER_FORMAT_UNSIGNED32,
    DIAMETER_FORMAT_INTEGER64,
    DIAMETER_FORMAT_UNSIGNED64,
    /* Float types are not supported
    DIAMETER_FORMAT_FLOAT32,
    DIAMETER_FORMAT_FLOAT64,
    */
    DIAMETER_FORMAT_GROUPED,
    /* RFC 3588 Derived Formats start here */
    DIAMETER_FORMAT_ENUM,
    DIAMETER_FORMAT_TIME,
    DIAMETER_FORMAT_ADDRESS,
    DIAMETER_FORMAT_UTF8STRING,
    DIAMETER_FORMAT_DIAMETER_IDENTITY,
    DIAMETER_FORMAT_DIAMETER_URI,
    DIAMETER_FORMAT_IPFILTER_RULE,
    DIAMETER_FORMAT_QOSFILTER_RULE,
    DIAMETER_FORMAT_DELETED,
    DIAMETER_FORMAT_V4_ADDRESS,
    DIAMETER_FORMAT_V6_ADDRESS,
    DIAMETER_FORMAT_V6_PREFIX,
    DIAMETER_FORMAT_UNKNOWN
} diameter_avp_format_t;
typedef enum diameter_avp_flag_e {
    DIAMETER_AVP_TAG_1 = 1,
    DIAMETER_AVP_TAG_2 = 2
} diameter_avp_flag_t;

#define ATTR_NAME_MAX 255

typedef struct diameter_avp_attribute_info_s {
    uint32_t              ord_no;
    char                  name[ATTR_NAME_MAX];
    uint32_t              code;
    diameter_avp_format_t format;
    uint32_t              use;
    uint32_t              flag_must_rules;
    uint32_t              flag_must_not_rules;
    uint32_t              vendor_id;
    uint32_t              vendor_flags;
    uint32_t              code_be;
    uint32_t              vendor_id_be;
} diameter_avp_attribute_info_t;

/* data handle used by diameter lib for data reference */
typedef void *diameter_avp_data_handle;

/* result codes for diameter avp operations */

typedef enum diameter_avp_rc_s {
    DIAMETER_AVP_SUCCESS = 0,
    DIAMETER_AVP_FAIL,
    DIAMETER_AVP_INVALID_HANDLE,
    DIAMETER_AVP_INVALID_INDEX,
    DIAMETER_AVP_INVALID_VAL,
    DIAMETER_AVP_INVALID_PARAM,
    DIAMETER_AVP_UNKNOWN_AVP,
    DIAMETER_AVP_MEM_FAILURE,
    DIAMETER_AVP_IS_MISSING,
    DIAMETER_AVP_INVALID_PAYLOAD,
    DIAMETER_AVP_INVALID_FLAGS,
    DIAMETER_AVP_INVALID_AVP_LENGTH,
    DIAMETER_AVP_INVALID_MESSAGE_LENGTH,
    DIAMETER_AVP_INVALID_DICTIONARY,
    DIAMETER_AVP_MAX_ERRORS
} diameter_avp_rc_t;


static inline const char *
diameter_avp_rc_str (diameter_avp_rc_t rc)
{
    switch (rc) {
    case DIAMETER_AVP_SUCCESS:
        return "success";
    case DIAMETER_AVP_FAIL:
        return "failure";
    case DIAMETER_AVP_INVALID_HANDLE:
        return "invalid handle";
    case DIAMETER_AVP_INVALID_INDEX:
        return "invalid index";
    case DIAMETER_AVP_INVALID_VAL:
        return "invalid value";
    case DIAMETER_AVP_INVALID_PARAM:
        return "invalid parameter";
    case DIAMETER_AVP_UNKNOWN_AVP:
        return "unknown AVP";
    case DIAMETER_AVP_MEM_FAILURE:
        return "memory failure";
    case DIAMETER_AVP_IS_MISSING:
        return "is missing";
    case DIAMETER_AVP_INVALID_PAYLOAD:
        return "invalid payload";
    case DIAMETER_AVP_INVALID_FLAGS:
        return "invalid flags";
    case DIAMETER_AVP_INVALID_AVP_LENGTH:
        return "invalid AVP length";
    case DIAMETER_AVP_INVALID_MESSAGE_LENGTH:
        return "invalid message length";
    case DIAMETER_AVP_INVALID_DICTIONARY:
        return "invalid dictionary";
    default:
        return "unknown";
    }
}

/* callbacks */

/*
 *  Returns size of data
 */
typedef void diameter_avp_cb_data_get_size(diameter_avp_data_handle buffer,
                                           uint32_t *pnReturnSize);
/*
 *  Returns pointer to data
 */
typedef void diameter_avp_cb_data_get_ptr(diameter_avp_data_handle buffer,
                                          u_int32_t offset, uint8_t **ppData);
/*
 *  Deallocated memory allocated for data (for example, jbuf)
 */
typedef uint32_t diameter_avp_cb_data_release(diameter_avp_data_handle buffer);

typedef uint32_t diameter_avp_cb_copy(diameter_avp_data_handle buffer,
                                      uint32_t offset, const void *data,
                                      uint32_t len);

typedef struct diameter_avp_data_cb_table_s {
    diameter_avp_cb_data_get_size*  cb_get_size;
    diameter_avp_cb_data_get_ptr*   cb_get_ptr;
    diameter_avp_cb_data_release*   cb_release;
    diameter_avp_cb_copy*           cb_copy;
} diameter_avp_data_cb_table_t;

/* ------------------------- Memory management ---------------------------- */
typedef enum diameter_avp_object_type_e {
    DIAMETER_AVP_TYPE_MESSAGE = 0,
    DIAMETER_AVP_TYPE_PAYLOAD, 
    DIAMETER_AVP_TYPE_VENDOR,
    DIAMETER_AVP_TYPE_DICTIONARY,
    /* must be the last */
    DIAMETER_AVP_TYPE_TOTAL
 } diameter_avp_object_type_t;

uint32_t diameter_avp_object_size (diameter_avp_object_type_t type);

/*
 *  Creates reference-counted object of the given size
 */
typedef void* diameter_avp_cb_object_allocate(diameter_avp_object_type_t type,
                                              uint32_t nSize);
/*
 *  Frees memory allocated for the object
 */
typedef uint32_t diameter_avp_cb_object_free(diameter_avp_object_type_t type,
                                             void* object);

/*
 *  Allocates memory of the given size
 */
typedef void* diameter_avp_cb_memory_allocate(uint32_t nSize);
/*
 *  Allocates critical memory of the given size
 */
typedef void* diameter_avp_cb_memory_allocate_critical(uint32_t nSize);
/*
 *  frees memory
 */
typedef void diameter_avp_cb_memory_free(void*, uint32_t);
/*
 *  frees critical memory
 */
typedef void diameter_avp_cb_memory_free_critical(void*, uint32_t);

typedef struct diameter_avp_memory_cb_table_s {
    diameter_avp_cb_memory_allocate*            cb_allocate;
    diameter_avp_cb_memory_allocate_critical*   cb_allocate_critical;
    diameter_avp_cb_memory_free*                cb_free;
    diameter_avp_cb_memory_free_critical*       cb_free_critical;
    diameter_avp_cb_object_allocate*            cb_object_allocate;
    diameter_avp_cb_object_free*                cb_object_free;
} diameter_avp_memory_cb_table_t;



/* ------------------------- Data Lookups --------------------------------- */
/*
 * Creates itable table (for example dsi).
 */

typedef struct diameter_avp_itable_parameters_s {
    uint32_t max_index;

} diameter_avp_itable_parameters_t;

typedef void (*diameter_avp_cb_itable_create_t)(
        const char* name,
        diameter_avp_itable_parameters_t* parameters,
        diameter_avp_itable_handle* hhandle);

/*
 * Destroys itable table represented by dia_dict_avp_table_handle.
 * The handle is no longer valid after this call.
 */
typedef void (*diameter_avp_cb_itable_destroy_t)(diameter_avp_itable_handle handle);
/*
 * Inserts entry into table represented by dia_dict_avp_table_handle
 */
typedef void (*diameter_avp_cb_itable_insert_t)(diameter_avp_itable_handle handle,
        void* ptr, const index32_t *target_index);
/*
 * Removes entry from the table represented by dia_dict_avp_table_handle
 */
typedef void (*diameter_avp_cb_itable_remove_t)(diameter_avp_itable_handle handle,
        const index32_t *target_index);
/*
 * Looks up entry in the table represented by dia_dict_avp_table_handle
 */
typedef void* (*diameter_avp_cb_itable_find_t)(diameter_avp_itable_handle handle,
        const index32_t *target_index);

typedef struct diameter_avp_itable_cb_table_s {
    diameter_avp_cb_itable_create_t    cb_create;
    diameter_avp_cb_itable_destroy_t   cb_destroy;
    diameter_avp_cb_itable_insert_t    cb_insert;
    diameter_avp_cb_itable_remove_t    cb_remove;
    diameter_avp_cb_itable_find_t      cb_find;
} diameter_avp_itable_cb_table_t;


/* ------------------------- System --------------------------------------- */

typedef struct diameter_avp_cb_table_s {
    diameter_avp_memory_cb_table_t  memory_cbs;
    diameter_avp_data_cb_table_t    data_cbs;
    diameter_avp_itable_cb_table_t    itable_cbs;
} diameter_avp_cb_table_t;


/*
 * @brief
 * Create Diameter dictionary.
 * @param [in]  dict_name
 *              Dictionary name.
 * @param [out] dict_hdl
 *              Dictionary handle of the new dictionary.
 *
 * @return dia_dict_status_t
 *         status code
 */
int diameter_avp_create_diameter_dictionary(const char *dict_name,
                                            diameter_avp_dict_handle *dict_hdl);

/*
 * @brief
 * Create RADIUS dictionary.
 * @param [in]  dict_name
 *              Dictionary name.
 * @param [out] dict_hdl
 *              Dictionary handle of the new dictionary.
 *
 * @return dia_dict_status_t
 *         status code
 */
int diameter_avp_create_radius_dictionary(const char *dict_name,
                                          diameter_avp_dict_handle *dict_hdl);
/*
 * @brief
 * Add an attribute to this dictionary. This adds the attribute's metadata.
 * It is assumed that the ordinal_no is a sequential number starting from 1
 * and it does not exceed DIA_DICT_MAX_ATTRS per dictionary.
 * @param [in] dict_hdl
 *             dictionary handle to which the attribute needs to be added.
 * @param [in] attr_info
 *             Attribute metadata structure defining the code/flags/format etc
 * @param [in] ordinal_no
 *             The ordinal number of this attribute which uniquely identifies this
 *             attribute.
 *
 * @return dia_dict_status_t
 *         status code
 */
int
diameter_avp_dictionary_add_attribute(diameter_avp_dict_handle       dict_hdl,
                                      diameter_avp_attribute_info_t *attr_info,
                                      uint32_t                       ordinal_no);

/*
 * @brief
 * Get the attribute metadata present in this dictionary for the given ordinal_no
 *
 * @param [in]  dict_hdl
 *              dictionary handle in which we lookup the attribute metadata
 * @param [in]  ordinal_no
 *              The ordinal number of this attribute which uniquely identifies this
 *              attribute.
 * @param [out] attr_info
 *              Attribute metadata structure defining the code/flags/format etc
 *
 * @return dia_dict_status_t
 *         status code
 */
int diameter_avp_dictionary_get_attribute_info(
                               diameter_avp_dict_handle dict_hdl,
                               uint32_t ordinal_no,
                               diameter_avp_attribute_info_t **attr_info); 

/*
 * @brief
 * Get the attribute name present in this dictionary for the given ordinal_no
 *
 * @param [in]  dict_hdl
 *              dictionary handle in which we lookup the attribute metadata
 * @param [in]  ordinal_no
 *              The ordinal number of this attribute which uniquely identifies this
 *              attribute.
 * @param [out] attr_name
 *              Pointer to the attribute name if found in this dictionary
 * @param [out] buffer_size
 *              size of the provided buffer. 
 *
 * @return dia_dict_status_t
 *         status code
 */
int diameter_avp_dictionary_get_attribute_name(
                           diameter_avp_dict_handle dict_hdl,
                           uint32_t ordinal_no,
                           char* attr_name, uint32_t buffer_size); 

/*
 * @brief
 * Get the attribute metadata present in this dictionary for the given avp_code
 *
 * @param [in]  dict_hdl
 *              dictionary handle in which we lookup the attribute metadata
 * @param [in]  avp_code
 *              AVP code of the DIAMETER AVP which we are looking up. 
 * @param [out] ordinal_no
 *              Pointer to the ordinal number of this attribute
 * @param [out] attr_info
 *              Attribute metadata structure defining the code/flags/format etc
 *
 * @return dia_dict_status_t
 *         status code
 */
int diameter_avp_dictionary_get_attribute_by_code(
                           diameter_avp_dict_handle dict_hdl,
                           uint32_t vendor_id, uint32_t avp_code,
                           uint32_t* ordinal_no,
                           diameter_avp_attribute_info_t **attr_info); 
/*
 * @brief
 * Get the attribute metadata present in this dictionary for the given avp_code
 * in network byte order
 *
 * @param [in]  dict_hdl
 *              dictionary handle in which we lookup the attribute metadata
 * @param [in]  avp_code
 *              AVP code of the DIAMETER AVP which we are looking up
 *              in network byte order.
 * @param [out] ordinal_no
 *              Pointer to the ordinal number of this attribute
 * @param [out] attr_info
 *              Attribute metadata structure defining the code/flags/format etc
 *
 * @return dia_dict_status_t
 *         status code
 */
int diameter_avp_dictionary_get_attribute_by_code_be(
                           diameter_avp_dict_handle dict_hdl,
                           uint32_t* vendor_id_be, uint32_t* avp_code_be,
                           uint32_t* ordinal_no,
                           diameter_avp_attribute_info_t **attr_info);

/*
 * @brief
 * Deletes Diameter or RADIUS dictionary.
 * @param [out] dict_hdl
 *              Dictionary handle of the new dictionary.
 *
 * @return dia_dict_status_t
 *         status code
 */
int
diameter_avp_delete_dictionary (diameter_avp_dict_handle dict_hdl);

/*
 * @brief
 * Removes an attribute to this dictionary.
 * Thread-safety is responsibility of caller
 * @param [in] dict_hdl
 *             dictionary handle to which the attribute needs to be added.
 * @param [in] ordinal_no
 *             The ordinal number of this attribute which uniquely identifies this
 *             attribute.
 *
 * @return dia_dict_status_t
 *         status code
 */
int
diameter_avp_dictionary_remove_attribute (diameter_avp_dict_handle  dict_hdl,
                                          uint32_t                  ordinal);

/* Commented Out for Now - Group Attribute Dictionary Handling
 *
 * int diameter_avp_dict_create_attribute_group (diameter_avp_dict_handle dict_hdl,
 *                       diameter_avp_dict_attribute_group_handle* attr_grp_hdl);
 *
 * int diameter_avp_dict_attribute_group_add_attribute (
 *                       diameter_avp_dict_attribute_group_handle attr_grp_hdl,
 *                       uint32_t ordinal_no);
 *
 * dia_dict_status_t diameter_avp_dictionary_attribute_group_get_size (
 *                             diameter_avp_dict_attribute_group_handle attr_grp_hdl,
 *                             size_t* size);
 * dia_dict_status_t diameter_avp_dict_attribute_group_get_attribute (
 *                             diameter_avp_dict_attribute_group_handle attr_grp_hdl,
 *                             size_t index, uint32_t* ordinal_no);
 * dia_dict_status_t diameter_avp_dictionary_add_grouped_attribute (
 *                             diameter_avp_dict_handle dict_hdl,
 *                             diameter_avp_attribute_info* pInfo,
 *                             diameter_avp_dict_attribute_group_handle attr_grp_hdl,
 *                             uint32_t* ordinal_no); 
 * dia_dict_status_t diameter_avp_dictionary_get_attribute_group (
 *                         diameter_avp_dict_handle dict_hdl, uint32_t ordinal_no,
 *                         diameter_avp_dict_attribute_group_handle attr_grp_hdl);
 */


/*
 * Sample add attribute creation callflow can look like as below.
 * diameter_avp_dict_handle dict_hdl;
 * diameter_avp_attribute_info dia_attr_info;
 * dia_dict_status_t status;

 * status = diameter_avp_create_dictionary("MyFirstDict", &dict_hdl);
 * if (status!=DIA_DICT_SUCCESS) return;

 * dia_attr_info.code = 85;
 * strncpy(dia_attr_info.name,"Acct-Interim-Interval", ATTR_NAME_MAX);
 * dia_attr_info.format = DIAMETER_FORMAT_UNSIGNED32;
 * dia_attr_info.enc = my_encode_fn;
 * dia_attr_info.dec = my_decode_fn;
 * dia_attr_info.vendor_id = 0;
 * dia_attr_info.vendor_flags = 0;
 * dia_attr_info.flags = 0x60
 * dia_attr_info.use = 0;
 * status = diameter_avp_dictionary_add_attribute (dict_hdl, attr_info, 1);

 * if (status!=DIA_DICT_STATUS) return;
 */


#endif /* __DIA_AVP_DICT_API_H__ */

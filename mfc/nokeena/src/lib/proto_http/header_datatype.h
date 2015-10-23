/* File : header_data_type.h Author : 
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

#ifndef _HEADER_DATATYPE_H_
/** 
 * @file header_datatype.h
 * @brief Header Related data type definitions
 * @author 
 * @version 1.00
 * @date 2009-09-03
 */

#define _HEADER_DATATYPE_H_
#include "stdint.h"

/** 
 * @brief Known Header type definitions
 */
typedef enum {
    DT_INT,
    DT_STRING,
    DT_DATE_1123,
    DT_ETAG,
    DT_CACHECONTROL_ENUM,
    DT_CONTENTRANGE,
    DT_RANGE,
    DT_DATE_1123_OR_ETAG,
    DT_SIZE
} mime_hdr_datatype_t;


/** 
 * @brief Cache Control Definitions
 */
typedef enum {
    CT_PUBLIC = 0,
    CT_PRIVATE,
    CT_NO_CACHE,
    CT_NO_STORE,
    CT_NO_TRANSFORM,
    CT_MUST_REVALIDATE,
    CT_PROXY_REVALIDATE,
    CT_MAX_AGE,
    CT_MAX_STALE,
    CT_MIN_FRESH,
    CT_ONLY_IF_CACHED,
    CT_S_MAXAGE,
    CT_MAX_DEFS
} http_cache_control_type;

#define CT_MASK_PUBLIC (1 << CT_PUBLIC)
#define CT_MASK_PRIVATE (1 << CT_PRIVATE)
#define CT_MASK_NO_CACHE (1 << CT_NO_CACHE)
#define CT_MASK_NO_STORE (1 << CT_NO_STORE)
#define CT_MASK_NO_TRANSFORM (1 << CT_NO_TRANSFORM)
#define CT_MASK_MUST_REVALIDATE (1 << CT_MUST_REVALIDATE) 
#define CT_MASK_PROXY_REVALIDATE (1 << CT_PROXY_REVALIDATE)
#define CT_MASK_MAX_AGE (1 << CT_MAX_AGE)
#define CT_MASK_MAX_STALE (1 << CT_MAX_STALE)
#define CT_MASK_MIN_FRESH (1 << CT_MIN_FRESH)
#define CT_MASK_ONLY_IF_CACHED (1 << CT_ONLY_IF_CACHED)
#define CT_MASK_S_MAXAGE (1 << CT_S_MAXAGE)

/** 
 * @brief Structure that defines the known data types
 * TODO - May be this structure should include the 'data type' type
 * to mean what the union is refering to.
 */
typedef struct cooked_data_types {
    union {
	/* DT_INT */
	struct dt_int_type {
	    int64_t l;
	} dt_int;

	/* DT_STRING */
	struct dt_string_type {
	    char str[1];	/* [str,len] len is returned */
	} dt_string;

	/* DT_DATE_1123 */
	struct dt_date_1123_type {
	    time_t t;
	} dt_date_1123;

	/* DT_ETAG */
	struct dt_etag_type {
	    char etag[1];	/* [str,len] len is returned */
	} dt_etag;

	/* DT_CACHECONTROL_ENUM */
	struct dt_cachecontrol_enum_type {
	    int mask;		/* Bit defs per
				 * http_cache_control_type def */
	    int max_age;
	    int max_stale;
	    int min_fresh;
	    int s_maxage;
	} dt_cachecontrol_enum;

	/* DT_CONTENTRANGE */
	struct dt_contentrange_type {
	    int64_t offset;
	    int64_t length;
	    int64_t entity_length;
	} dt_contentrange;

	/* DT_RANGE */
	struct dt_range_type {
	    int64_t offset;
	    int64_t length;
	} dt_range;

	/* DT_DATE_1123_OR_ETAG */
	struct dt_date_1123_or_etag_type {
	    char date_or_etag[1];	/* [str,len] len is
					 * returned */
	} dt_date_1123_or_etag;

	/* DT_SIZE */
	struct dt_size_type {
	    uint64_t ll;
	} dt_size;
    } u;
} cooked_data_types_t;


typedef u_int32_t dataddr_t;

#define MAX_HEAP_ELEMENTSZ	(16 * 1024)
#define MAX_EXT_HEAPSZ		(64 * 1024)

/* Type of element */
#define F_VALUE_DATA		0x1
#define F_NAME_VALUE_DATA	0x2
#define F_DATA			0x4

#define F_ALLOCATED		(F_VALUE_DATA|F_NAME_VALUE_DATA|F_DATA)
#define F_USER_ATTR_MASK	0xff000000

/* Data reference info */
#define F_NAME_HEAPDATA_REF	0x8
#define F_VALUE_HEAPDATA_REF	0x10

#define USERATTR2ATTR(_a) (((_a) << 24) & F_USER_ATTR_MASK)
#define ATTR2USERATTR(_a) (u_int8_t)(((_a) >> 24) & 0xff)

/** 
 * @brief HTTP header heap data
 * Support data structures for heap allocation within mime context
 *
 */
typedef struct heap_data {	/* 32 byte element; must be pwr of 2 */
    u_int64_t flags;
    union {
	struct value_data {
	    dataddr_t value;
	    int32_t value_len;
	    dataddr_t value_next;
	    dataddr_t cooked_data;
	    int32_t hdrcnt;
	    char pad[4];
	} v;

	struct name_value_data {
	    dataddr_t next;
	    dataddr_t name;
	    int32_t name_len;
	    dataddr_t value;
	    int32_t value_len;
	    char pad[4];
	} nv;

	struct raw_data {
	    int32_t size;
	    char data[20];
	} d;

	struct pad {
	    char pad[24];
	} p;
    } u;
} heap_data_t;


/* Hash Table definitions */

/** 
 * @brief Hash table Entry
 */
typedef struct hash_entry
{
    struct hash_entry *next;
    const char *name;
    int name_len;
    long data;
} hash_entry_t;

/** 
 * @brief Hash table definition
 */
typedef struct hash_table_def 
{
    hash_entry_t *ht;
    int size; // power of 2
    int max_strlen;
    int (*hash_func)(const char *str, int len);
    int (*add_func)(struct hash_table_def *ht, const char *str, int len, 
			int data);
    int (*cmp_func)(const char *str1, const char *str2, int len);
    int (*lookup_func)(const struct hash_table_def *ht, const char *str, 
			int len, int *data);
    int (*dealloc_func)(const struct hash_table_def *ht);
    int (*del_func) (struct hash_table_def *ht, const char *str, int len);
    pthread_mutex_t *ht_lock;
} hash_table_def_t;

extern hash_table_def_t ht_base_nocase_hash;
extern hash_table_def_t ht_base_case_hash;

#endif

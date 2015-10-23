/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 12088 $ of $ 
 *
 * $Id:$
 */
/* standard Net-SNMP includes */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/* include our parent header */
#include "jmfcNamespaceTable.h"


/** @defgroup data_get data_get: Routines to get data
 *
 * TODO:230:M: Implement jmfcNamespaceTable get routines.
 * TODO:240:M: Implement jmfcNamespaceTable mapping routines (if any).
 *
 * These routine are used to get the value for individual objects. The
 * row context is passed, along with a pointer to the memory where the
 * value should be copied.
 *
 * @{
 */
/**********************************************************************
 **********************************************************************
 ***
 *** Table jmfcNamespaceTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * JUNIPER-MFC-MIB::jmfcNamespaceTable is subid 1 of jmfcNamespace.
 * Its status is Current.
 * OID: .1.3.6.1.4.1.2636.1.2.1.4.1, length: 12
*/

/* ---------------------------------------------------------------------
 * TODO:200:r: Implement jmfcNamespaceTable data context functions.
 */


/**
 * set mib index(es)
 *
 * @param tbl_idx mib index structure
 * @param jmfcNamespaceName_ptr
 * @param jmfcNamespaceName_ptr_len
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 *
 * @remark
 *  This convenience function is useful for setting all the MIB index
 *  components with a single function call. It is assume that the C values
 *  have already been mapped from their native/rawformat to the MIB format.
 */
int
jmfcNamespaceTable_indexes_set_tbl_idx(jmfcNamespaceTable_mib_index *tbl_idx, char *jmfcNamespaceName_val_ptr,  size_t jmfcNamespaceName_val_ptr_len)
{
    DEBUGMSGTL(("verbose:jmfcNamespaceTable:jmfcNamespaceTable_indexes_set_tbl_idx","called\n"));

    /* jmfcNamespaceName(2)/OCTETSTR/ASN_OCTET_STR/char(char)//L/A/w/e/R/d/h */
    tbl_idx->jmfcNamespaceName_len = sizeof(tbl_idx->jmfcNamespaceName)/sizeof(tbl_idx->jmfcNamespaceName[0]); /* max length */
    /*
     * make sure there is enough space for jmfcNamespaceName data
     */
    if ((NULL == tbl_idx->jmfcNamespaceName) ||
        (tbl_idx->jmfcNamespaceName_len <
         (jmfcNamespaceName_val_ptr_len))) {
        snmp_log(LOG_ERR,"not enough space for value\n");
        return MFD_ERROR;
    }
    tbl_idx->jmfcNamespaceName_len = jmfcNamespaceName_val_ptr_len;
    memcpy( tbl_idx->jmfcNamespaceName, jmfcNamespaceName_val_ptr, jmfcNamespaceName_val_ptr_len* sizeof(jmfcNamespaceName_val_ptr[0]) );
    

    return MFD_SUCCESS;
} /* jmfcNamespaceTable_indexes_set_tbl_idx */

/**
 * @internal
 * set row context indexes
 *
 * @param reqreq_ctx the row context that needs updated indexes
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 *
 * @remark
 *  This function sets the mib indexs, then updates the oid indexs
 *  from the mib index.
 */
int
jmfcNamespaceTable_indexes_set(jmfcNamespaceTable_rowreq_ctx *rowreq_ctx, char *jmfcNamespaceName_val_ptr,  size_t jmfcNamespaceName_val_ptr_len)
{
    DEBUGMSGTL(("verbose:jmfcNamespaceTable:jmfcNamespaceTable_indexes_set","called\n"));

    if(MFD_SUCCESS != jmfcNamespaceTable_indexes_set_tbl_idx(&rowreq_ctx->tbl_idx
                                   , jmfcNamespaceName_val_ptr, jmfcNamespaceName_val_ptr_len
           ))
        return MFD_ERROR;

    /*
     * convert mib index to oid index
     */
    rowreq_ctx->oid_idx.len = sizeof(rowreq_ctx->oid_tmp) / sizeof(oid);
    if(0 != jmfcNamespaceTable_index_to_oid(&rowreq_ctx->oid_idx,
                                    &rowreq_ctx->tbl_idx)) {
        return MFD_ERROR;
    }

    return MFD_SUCCESS;
} /* jmfcNamespaceTable_indexes_set */


/*---------------------------------------------------------------------
 * JUNIPER-MFC-MIB::jmfcNamespaceEntry.jmfcNamespaceUID
 * jmfcNamespaceUID is subid 3 of jmfcNamespaceEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.4.1.2636.1.2.1.4.1.1.3
 * Description:
A unique, internally generated UID to track the
            namespace object.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 *
 * Its syntax is OCTETSTR (based on perltype OCTETSTR)
 * The net-snmp type is ASN_OCTET_STR. The C type decl is char (char)
 * This data type requires a length.
 */
/**
 * Extract the current value of the jmfcNamespaceUID data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param jmfcNamespaceUID_val_ptr_ptr
 *        Pointer to storage for a char variable
 * @param jmfcNamespaceUID_val_ptr_len_ptr
 *        Pointer to a size_t. On entry, it will contain the size (in bytes)
 *        pointed to by jmfcNamespaceUID.
 *        On exit, this value should contain the data size (in bytes).
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
*
 * @note If you need more than (*jmfcNamespaceUID_val_ptr_len_ptr) bytes of memory,
 *       allocate it using malloc() and update jmfcNamespaceUID_val_ptr_ptr.
 *       <b>DO NOT</b> free the previous pointer.
 *       The MFD helper will release the memory you allocate.
 *
 * @remark If you call this function yourself, you are responsible
 *         for checking if the pointer changed, and freeing any
 *         previously allocated memory. (Not necessary if you pass
 *         in a pointer to static memory, obviously.)
 */
int
jmfcNamespaceUID_get( jmfcNamespaceTable_rowreq_ctx *rowreq_ctx, char **jmfcNamespaceUID_val_ptr_ptr, size_t *jmfcNamespaceUID_val_ptr_len_ptr )
{
   /** we should have a non-NULL pointer and enough storage */
   netsnmp_assert( (NULL != jmfcNamespaceUID_val_ptr_ptr) && (NULL != *jmfcNamespaceUID_val_ptr_ptr));
   netsnmp_assert( NULL != jmfcNamespaceUID_val_ptr_len_ptr );


    DEBUGMSGTL(("verbose:jmfcNamespaceTable:jmfcNamespaceUID_get","called\n"));

    netsnmp_assert(NULL != rowreq_ctx);

/*
 * TODO:231:o: |-> Extract the current value of the jmfcNamespaceUID data.
 * copy (* jmfcNamespaceUID_val_ptr_ptr ) data and (* jmfcNamespaceUID_val_ptr_len_ptr ) from rowreq_ctx->data
 */
    /*
     * make sure there is enough space for jmfcNamespaceUID data
     */
    if ((NULL == (* jmfcNamespaceUID_val_ptr_ptr )) ||
        ((* jmfcNamespaceUID_val_ptr_len_ptr ) <
         (rowreq_ctx->data.jmfcNamespaceUID_len* sizeof(rowreq_ctx->data.jmfcNamespaceUID[0])))) {
        /*
         * allocate space for jmfcNamespaceUID data
         */
        (* jmfcNamespaceUID_val_ptr_ptr ) = malloc(rowreq_ctx->data.jmfcNamespaceUID_len* sizeof(rowreq_ctx->data.jmfcNamespaceUID[0]));
        if(NULL == (* jmfcNamespaceUID_val_ptr_ptr )) {
            snmp_log(LOG_ERR,"could not allocate memory\n");
            return MFD_ERROR;
        }
    }
    (* jmfcNamespaceUID_val_ptr_len_ptr ) = rowreq_ctx->data.jmfcNamespaceUID_len* sizeof(rowreq_ctx->data.jmfcNamespaceUID[0]);
    memcpy( (* jmfcNamespaceUID_val_ptr_ptr ), rowreq_ctx->data.jmfcNamespaceUID, rowreq_ctx->data.jmfcNamespaceUID_len* sizeof(rowreq_ctx->data.jmfcNamespaceUID[0]) );

    return MFD_SUCCESS;
} /* jmfcNamespaceUID_get */

/*---------------------------------------------------------------------
 * JUNIPER-MFC-MIB::jmfcNamespaceEntry.jmfcNamespaceStatus
 * jmfcNamespaceStatus is subid 4 of jmfcNamespaceEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.4.1.2636.1.2.1.4.1.1.4
 * Description:
The current status of the namespace - Active or Inactive.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 *
 * Its syntax is INTEGER32 (based on perltype INTEGER32)
 * The net-snmp type is ASN_INTEGER. The C type decl is long (long)
 */
/**
 * Extract the current value of the jmfcNamespaceStatus data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param jmfcNamespaceStatus_val_ptr
 *        Pointer to storage for a long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int
jmfcNamespaceStatus_get( jmfcNamespaceTable_rowreq_ctx *rowreq_ctx, long * jmfcNamespaceStatus_val_ptr )
{
   /** we should have a non-NULL pointer */
   netsnmp_assert( NULL != jmfcNamespaceStatus_val_ptr );


    DEBUGMSGTL(("verbose:jmfcNamespaceTable:jmfcNamespaceStatus_get","called\n"));

    netsnmp_assert(NULL != rowreq_ctx);

/*
 * TODO:231:o: |-> Extract the current value of the jmfcNamespaceStatus data.
 * copy (* jmfcNamespaceStatus_val_ptr ) from rowreq_ctx->data
 */
    (* jmfcNamespaceStatus_val_ptr ) = rowreq_ctx->data.jmfcNamespaceStatus;

    return MFD_SUCCESS;
} /* jmfcNamespaceStatus_get */

/*---------------------------------------------------------------------
 * JUNIPER-MFC-MIB::jmfcNamespaceEntry.jmfcNamespaceResourcePool
 * jmfcNamespaceResourcePool is subid 5 of jmfcNamespaceEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.4.1.2636.1.2.1.4.1.1.5
 * Description:
The resource pool associated with  namespace object.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 *
 * Its syntax is OCTETSTR (based on perltype OCTETSTR)
 * The net-snmp type is ASN_OCTET_STR. The C type decl is char (char)
 * This data type requires a length.
 */
/**
 * Extract the current value of the jmfcNamespaceResourcePool data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param jmfcNamespaceResourcePool_val_ptr_ptr
 *        Pointer to storage for a char variable
 * @param jmfcNamespaceResourcePool_val_ptr_len_ptr
 *        Pointer to a size_t. On entry, it will contain the size (in bytes)
 *        pointed to by jmfcNamespaceResourcePool.
 *        On exit, this value should contain the data size (in bytes).
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
*
 * @note If you need more than (*jmfcNamespaceResourcePool_val_ptr_len_ptr) bytes of memory,
 *       allocate it using malloc() and update jmfcNamespaceResourcePool_val_ptr_ptr.
 *       <b>DO NOT</b> free the previous pointer.
 *       The MFD helper will release the memory you allocate.
 *
 * @remark If you call this function yourself, you are responsible
 *         for checking if the pointer changed, and freeing any
 *         previously allocated memory. (Not necessary if you pass
 *         in a pointer to static memory, obviously.)
 */
int
jmfcNamespaceResourcePool_get( jmfcNamespaceTable_rowreq_ctx *rowreq_ctx, char **jmfcNamespaceResourcePool_val_ptr_ptr, size_t *jmfcNamespaceResourcePool_val_ptr_len_ptr )
{
   /** we should have a non-NULL pointer and enough storage */
   netsnmp_assert( (NULL != jmfcNamespaceResourcePool_val_ptr_ptr) && (NULL != *jmfcNamespaceResourcePool_val_ptr_ptr));
   netsnmp_assert( NULL != jmfcNamespaceResourcePool_val_ptr_len_ptr );


    DEBUGMSGTL(("verbose:jmfcNamespaceTable:jmfcNamespaceResourcePool_get","called\n"));

    netsnmp_assert(NULL != rowreq_ctx);

/*
 * TODO:231:o: |-> Extract the current value of the jmfcNamespaceResourcePool data.
 * copy (* jmfcNamespaceResourcePool_val_ptr_ptr ) data and (* jmfcNamespaceResourcePool_val_ptr_len_ptr ) from rowreq_ctx->data
 */
    /*
     * make sure there is enough space for jmfcNamespaceResourcePool data
     */
    if ((NULL == (* jmfcNamespaceResourcePool_val_ptr_ptr )) ||
        ((* jmfcNamespaceResourcePool_val_ptr_len_ptr ) <
         (rowreq_ctx->data.jmfcNamespaceResourcePool_len* sizeof(rowreq_ctx->data.jmfcNamespaceResourcePool[0])))) {
        /*
         * allocate space for jmfcNamespaceResourcePool data
         */
        (* jmfcNamespaceResourcePool_val_ptr_ptr ) = malloc(rowreq_ctx->data.jmfcNamespaceResourcePool_len* sizeof(rowreq_ctx->data.jmfcNamespaceResourcePool[0]));
        if(NULL == (* jmfcNamespaceResourcePool_val_ptr_ptr )) {
            snmp_log(LOG_ERR,"could not allocate memory\n");
            return MFD_ERROR;
        }
    }
    (* jmfcNamespaceResourcePool_val_ptr_len_ptr ) = rowreq_ctx->data.jmfcNamespaceResourcePool_len* sizeof(rowreq_ctx->data.jmfcNamespaceResourcePool[0]);
    memcpy( (* jmfcNamespaceResourcePool_val_ptr_ptr ), rowreq_ctx->data.jmfcNamespaceResourcePool, rowreq_ctx->data.jmfcNamespaceResourcePool_len* sizeof(rowreq_ctx->data.jmfcNamespaceResourcePool[0]) );

    return MFD_SUCCESS;
} /* jmfcNamespaceResourcePool_get */



/** @} */
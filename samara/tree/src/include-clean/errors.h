/*
 *
 * errors.h
 *
 *
 *
 */

#ifndef __ERRORS_H_
#define __ERRORS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file errors.h Error return codes.
 * \ingroup lc
 */

#include "common.h"

/* ------------------------------------------------------------------------- */
/** Error return codes.
 *
 * All codes (with the exception of lc_err_no_error) should be in the
 * range 14000-14999 (an arbitrarily chosen range of large numbers) to
 * decrease the likelihood of overlap with return codes used by
 * functions not conforming to the TMS coding guidelines.  This will
 * allow our code to assume more strongly that a particular code
 * actually means what it says in this file.
 *
 * lc_err_no_error cannot be changed from zero, as a lot of code
 * relies on zero meaning no error.
 *
 * NOTE: lc_error_code_map (in errors.c) must be kept in sync.
 */
typedef enum {
    /** Success */
    lc_err_no_error          =      0,

    /** @name Generic error codes */
    /*@{*/

    /** Generic error */
    lc_err_generic_error     =  14000,

    /** The code asserted that a value would not be NULL, but it was */
    lc_err_unexpected_null   =  14001,

    /** The code asserted that a boolean expression would be true, but 
     * it was not.
     */
    lc_err_assertion_failed  =  14002,

    /** Function called is Not Yet Implemented */
    lc_err_nyi               =  14003,

    /** The requested item was not found. */
    lc_err_not_found         =  14004,

    /** The item unexpectedly already exists; usually returned when an
     * option is set to not allow duplicates in a certain context.
     */
    lc_err_exists            =  14005,

    /** Multiple items were found where a single one was expected, e.g.
     * returned by a function requested to retrieve a single item but
     * more than one was found matching the criteria.
     */
    lc_err_multiple          =  14006,

    /** Unexpected End Of File during file I/O */
    lc_err_unexpected_eof    =  14007,

    /** Unknown command-line argument used */
    lc_err_unexpected_arg    =  14008,

    /** Path name specified is not valid */
    lc_err_bad_path          =  14009,

    /** Specified file does not have the expected format or is not
     * appropriate.
     */
    lc_err_bad_file          =  14010,

    /** Ran out of buffer space for string operation */
    lc_err_no_buffer_space   =  14011,

    /** Parser error: generally, a string being parsed did not have
     * the expected format.
     */
    lc_err_parse_error       =  14012,

    /** File or directory not found */
    lc_err_file_not_found    =  14013,

    /** Generic Input/Output error */
    lc_err_io_error          =  14014,

    /** I/O source/sink would block */
    lc_err_io_blocking_error =  14015,

    /** I/O source sink is closed */
    lc_err_io_closed         =  14016,

    /** I/O operation timed out */
    lc_err_io_timeout        =  14017,

    /** I/O operation found bad or unexpected file existence */
    lc_err_io_bad_existence  =  14018,

    /** I/O verify failed */
    lc_err_io_verify_failed  =  14019,

    /** I/O operation used unsupported address family */
    lc_err_io_bad_family     =  14020,

    /** Unexpected case in switch statement */
    lc_err_unexpected_case   =  14021,

    /** Type conversion failed, or inappropriate type used */
    lc_err_bad_type          =  14022,

    /** Bad UTF-8 character byte sequence */
    lc_err_bad_utf8_seq      =  14023,

    /** Attempt to divide by zero */
    lc_err_divide_by_zero    =  14024,

    /** Out of space in filesystem (ENOSPC) */
    lc_err_no_fs_space       =  14025,

    /** Decryption failure (probably a bad key, or corrupted data) */
    lc_err_decryption_failure = 14026,

    /** Cannot write to read-only item */
    lc_err_read_only         = 14027,

    /** A Java exception occurred */
    lc_err_java_exception    = 14028,

    /** Some limit was exceeded */
    lc_err_limit_exceeded    = 14029,

    /** Too many files or connections open */
    lc_err_too_many_fds      = 14030,

    /** Necessary initialization has not been done */
    lc_err_not_initialized   = 14031,

    /** Lock already locked (and not recursive) */
    lc_err_lock_busy         = 14032,

    /** Lock should have been locked, but was not */
    lc_err_lock_not_locked   = 14033,

    /** Lock operation with incorrect ownership */
    lc_err_lock_bad_owner    = 14034,

    /** Value out of range */
    lc_err_range             = 14035,

    /** An invalid parameter was passed */
    lc_err_invalid_param     = 14036,

    /** The current value should be skipped (used by SNMP) */
    lc_err_skip_value        = 14037,

    /*@}*/
    /** @name Errors for the ::tree data structure */
    /*@{*/

    /** Tried to delete a tree node with children */
    lc_err_has_children      =  14090,

    /** Tried to get a path relative to a node which is not this node's
     * ancestor.
     */
    lc_err_not_an_ancestor   =  14091,

    /**
     * Tried to insert a node by path, but the required ancestors did not 
     * exist.
     */
    lc_err_no_path           =  14092,

    /*@}*/
    /** @name Codes returned by foreach functions
     * Some of these are not really errors, but tell the iterator
     * how to proceed with the iteration.  If ::lc_err_no_error is 
     * returned, proceed with the iteration normally.
     */
    /*@{*/

    /** Current element should be deleted */
    lc_err_foreach_delete    =  14100,

    /** Current element should be removed */
    lc_err_foreach_remove    =  14101,

    /** Halt the iteration here with an error */
    lc_err_foreach_halt_err  =  14102,

    /** Halt the iteration here with no error */
    lc_err_foreach_halt_ok   =  14103,

    /*@}*/
    /** @name CLI-specific codes */
    /*@{*/
    /** Reset the associated timer for a shell event callback */
    lc_err_reset_timeout     =  14110,

    /** Delete this shell event registration */
    lc_err_event_delete      =  14111,
   
    /** A CLI shell-only function was called by a module running in a
     * non-shell client; returned by libclish.
     */
    lc_err_not_cli_shell     = 14115,

    /** Returned by gl_get_line(): the command line was too long */
    lc_err_cli_cmd_too_long  = 14116,

    /*@}*/
    /** @name Miscellaneous */
    /*@{*/

    /**
     * Special code indicating that the current operation has been cancelled
     * under normal circumstances (e.g. by the user).  The code is special
     * because the bail macros do not treat it as an error, and hence do not
     * log anything when it is received.  If the user of a bail macro wishes
     * to log an internal error if lc_err_cancelled is received, it would
     * have to test for it explicitly with
     * bail_require(err != lc_err_cancelled) before calling bail_error(err).
     */
    lc_err_cancelled         =  14120,

    /**
     * Indicates that the operation has been cancelled in an error
     * situation which does warrant normal error logging.  Just like 
     * lc_err_cancelled, except without the special silent treatment.
     */
    lc_err_cancelled_error   = 14121,

    /**
     * Used by the stats daemon when reading in files from disk:
     * number of instances in the file differs from configured 
     * number of instances.
     */
    lc_err_wrong_num_instances = 14125,

    /**
     * Code indicating a transient failure when processing a request.
     * The client may usefully decide to resend the original request.
     * In general, providers do not use or return this code.  The
     * clustering service is currently the only exception.
     */
    lc_err_transient_failure = 14201,

    /**
     * Code used by libmdb to indicate a particular kind of failure
     * reading a db file: at least one attribute was of an
     * unrecognized type.
     */
    lc_err_db_unrecognized_type = 14301,
    
} lc_error_code;

extern const lc_enum_string_map lc_error_code_map[];

#define lc_error_to_string(errcode) \
    lc_enum_to_string_quiet(lc_error_code_map, errcode)

#ifdef __cplusplus
}
#endif

#endif /* __ERRORS_H_ */

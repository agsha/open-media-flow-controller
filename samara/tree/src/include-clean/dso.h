/*
 *
 * dso.h
 *
 *
 *
 */

#ifndef __DSO_H_
#define __DSO_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file dso.h Dynamic Shared Object library.
 * \ingroup lc
 *
 * This library is mainly used by various Samara infrastructures to
 * deal with plug-in modules.
 *
 * This library allows the client to implement a plug-in module
 * architecture.  It allows DSOs to be loaded and unloaded, as well as
 * initialized or deinitialized through the use of well-known function
 * names.  It allows pointers to arbitrary other symbols to be
 * retrieved from any dynamically-loaded object.  It also allows the
 * current state of each module to be queried by the client.
 */

#include "common.h"

/* ============================================================================
 * Typedefs
 * ============================================================================
 */

/* ------------------------------------------------------------------------- */
/** Initialization options for the DSO library.  This structure is
 * currently empty, but is provided to allow for future expansion of
 * the DSO library's capabilities without changing the API.  One
 * possible future expansion would be to allow the caller to specify
 * how to form the names of the initialization and deinitialization
 * functions.
 */
typedef struct lc_dso_options {
} lc_dso_options;

/* ------------------------------------------------------------------------- */
/** DSO library context pointer.  Holds all of the state about a set of
 * modules.  All of the library's persistent state is kept in this
 * context structure.  This allows multiple parts of the same binary
 * to load and manage their own set of modules using the library
 * without interfering with each other.
 */
typedef struct lc_dso_context lc_dso_context;

/* ------------------------------------------------------------------------- */
/** Current state of a particular module.  Events that affect state:
 *
 *   - Loading a module.  If the load attempt succeeds, a module record
 *     is created with initial state lds_loaded.  If the load attempt
 *     fails, no module record is created.
 *
 *   - Loading a different module of the same name.  If the
 *     llp_replace_old load preference (see below) is used when,  
 *     loading modules a module could be automatically kicked out
 *     if a new one takes precedence.  In this case, (a) if the
 *     module was in the lds_initialized state it is deinitialized;
 *     then (b) the module is unloaded and its record deleted.
 *
 *   - Initializing a module.  Transition from lds_loaded or
 *     lds_deinitialized to lds_initialized, even if initialization
 *     fails.  Modules already in the lds_initialized state
 *     cannot be initialized.
 *
 *   - Deinitializing a module.  Transition from lds_initialized
 *     to lds_deinitialized, even if deinitialization fails.
 *     Modules in the lds_loaded or lds_deinitialized states
 *     cannot be deinitialized.
 *
 *   - Unloading a module.  Can be done from any state, although
 *     if the module was in the lds_initialized state, a warning
 *     is logged, as resources may have been leaked.  This does
 *     not transition to a different state; it deletes the module
 *     record.
 *
 * NOTE: lc_dso_module_state_map (in dso.c) must be kept in sync.
 */
typedef enum {
    /** State is undefined/uninitialized */
    lds_none = 0,

    /**
     * The module has been loaded and linked, but its initialization
     * function has not yet been called.
     */
    lds_loaded,

    /**
     * The module has been loaded and linked, and its initialization
     * function has been called.
     */
    lds_initialized,

    /**
     * The module was initialized, but has now had its deinitialization
     * function called as well.
     */
    lds_deinitialized,
    
    lds_last
} lc_dso_module_state;

extern const lc_enum_string_map lc_dso_module_state_map[];

/* ------------------------------------------------------------------------- */
/** Publicly-exposed information about a module.  This information is
 * available to the library client when querying about a loaded module
 * or iterating over the modules, as well as to the module itself when
 * the library calls it for initialization or deinitialization.
 *
 * The name of a module (ldi_name) is the filename with the extension
 * (".so" on UNIX platforms) removed.
 */
typedef struct lc_dso_info {
    lc_dso_context *ldi_context;
    lc_dso_module_state ldi_state;

    /** Filename module was loaded from, without extension */
    char *ldi_name;

    /** Directory module was loaded from                   */
    char *ldi_dir;

    /**
     * Return code from last call to this module's init function.
     * Only valid if the module is in the lds_initialized state;
     * otherwise its value is undefined.
     */
    int ldi_init_return_code;

    /**
     * Return code from last call to this module's deinit function.
     * Only valid if the module is in the lds_deinitialized state;
     * otherwise its value is undefined.
     */
    int ldi_deinit_return_code;
} lc_dso_info;

/* ------------------------------------------------------------------------- */
/** @name Function types: init, deinit, iterators.
 *
 * Can be called by the library at the request of the library client.
 *
 * The 'info' structure is provided by the library.  The ldi_state
 * field in the info structure is not updated to reflect the module's
 * new state until after the init/deinit function returns.  The 'data'
 * argument is passed through from the client.
 *
 * Prototypes for an actual functions of these types should be
 * declared using the macros below.  This makes it clear that the
 * parameter lists cannot be changed.  Otherwise the module writer
 * could change a parameter list, and no compile error would result,
 * but unpredictable runtime behavior would occur because the DSO
 * library would then unknowingly cast the function pointer to the
 * wrong function type.
 *
 * The functions are expected to return zero for success, nonzero for
 * failure.
 */
/*@{*/
#define LC_DSO_INIT_FUNC(fn) int (fn)(const lc_dso_info *info, void *data)
typedef LC_DSO_INIT_FUNC(lc_dso_init_func);
#define lc_dso_init_func_suffix "_init"
#define lc_dso_init_order_suffix lc_dso_init_func_suffix "_order"

#define LC_DSO_DEINIT_FUNC(fn) int (fn)(const lc_dso_info *info, void *data)
typedef LC_DSO_DEINIT_FUNC(lc_dso_deinit_func);
#define lc_dso_deinit_func_suffix "_deinit"

#define LC_DSO_FOREACH_FUNC(fn) int (fn)(const lc_dso_info *info, void *data)
typedef LC_DSO_FOREACH_FUNC(lc_dso_foreach_func);
/*@}*/

/* ------------------------------------------------------------------------- */
/** Specify behavior when an attempt is made to load a module which has
 * the same name as a module previously loaded.  If llp_keep_old is
 * specified, the new module is not loaded.  If llp_replace_old is
 * specified, the old module is automatically deinitialized (if it was
 * in the lds_initialized state) and then unloaded first, then the new
 * one is loaded.
 */
typedef enum {llp_none = 0, llp_keep_old, llp_replace_old,
              llp_last} lc_dso_load_precedence;

/* ------------------------------------------------------------------------- */
/** The filename extension DSO files are expected to have.
 */
#define DSO_STANDARD_EXTENSION ".so"
extern const char dso_standard_extension[];


/* ------------------------------------------------------------------------- */
/** This is to be used by callers that also support static linking of
 * modules, and want to allow modules to retrieve symbol pointers from
 * each other by name.
 */
typedef struct lc_dso_mod_symbol {
    const char *dms_module_name;
    const char *dms_symbol_name;
    void *dms_symbol;
} lc_dso_mod_symbol;


/* ========================================================================= */
/** @name DSO library contexts
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Retrieve the default options for the DSO library.  If you want to
 * override any of the defaults, call this first and then overwrite
 * the values you want to.
 */
int lc_dso_context_get_defaults(lc_dso_options *ret_options);

/* ------------------------------------------------------------------------- */
/** Create a new context.  Do this before using any other part of this
 * library.  If NULL is passed for 'options', the default options are
 * used.  Otherwise all options must be filled out, which can be
 * ensured by calling lc_dso_context_get_defaults() first and then
 * overwriting the values you want to change.
 */
int lc_dso_context_new(lc_dso_context **ret_context,
                       const lc_dso_options *options);

/* ------------------------------------------------------------------------- */
/** Free a context.  Note that this does not automatically deinitialize
 * or unload any modules that may still be active.  A warning is
 * logged for every module record still in the context, as there 
 * should not be any left by the time this is called.
 */
int lc_dso_context_free(lc_dso_context **inout_context);

/*@}*/

/* ========================================================================= */
/** @name Loading and unloading modules
 * Link and unlink with the DSOs.
 * On UNIX platforms, this corresponds to dlopen() and dlclose().
 *
 * Modules may be loaded from multiple directories, but multiple
 * modules with the same filename will not be maintained
 * simultaneously, as the naming convention for the init and deinit
 * functions would cause their namespaces to clash.  If there are
 * multiple modules sharing the same filename, the parameters to the
 * load function will determine whether the old or new one takes
 * precedence.
 *
 * This restriction allows loaded modules to be referred to by clients
 * only by filename, without a path.  This allows overlays placed in a
 * separate directory to be used in a manner transparent to much of
 * the code.
 *
 * When loading modules, all symbols are resolved immediately (rather
 * than lazily when they are called from the code).  This corresponds
 * to using RTLD_NOW, rather than RTLD_LAZY, in the UNIX dlopen()
 * interface.
 *
 * Note that unloading modules may make function pointers and other
 * symbols previously returned from lc_dso_module_get_symbol()
 * invalid.  Loading modules may do this also, if llp_replace_old is
 * passed and an old module needs to be unloaded to make room for a
 * new one.
 */

/* ============================================================================
 *
 * XXX/EMT: Some steps could be taken to deal with the case of a client
 * wanting to dynamically adjust to modules appearing, disappearing,
 * or being changed in the directories from which they were originally
 * loaded, without disturbing the modules that have changed.
 *
 * Currently the best plan would be to deinitialize all of the
 * modules, and rerun the same routine used to load them originally.
 * This would have the potentially undesirable effect of unloading and
 * reloading modules that have not changed.  If this is a problem:
 *
 *   - To allow new modules to be added, modify the load routines to
 *     check the list of currently loaded modules for one whose path
 *     and filename matches the one requested to be loaded.  If there
 *     is a match, assume it is the same module, and do not load the
 *     new one.
 *
 *   - To allow changed modules to be reloaded, keep a hash of the
 *     contents of each module file, and when about to load one
 *     with the same directory and filename, compare the hashes.
 *     If they do not match, reload the file anyway (in spite of
 *     the previous rule).  Note: the alternate approach of having
 *     a version number in a global variable in the module
 *     (a) is more burden on module writers, and (b) requires the
 *     module to be loaded to check the version number, by which
 *     time it is too late.
 *
 *   - To allow removed modules to be unloaded, provide a routine
 *     that rescans the directories from which modules were loaded,
 *     and checks to see if the files still exist.  If they do
 *     not, deinitialize and unload them.
 *
 * This is not planned to be implemented soon, as unloading and
 * loading all modules is thought to be a sufficient solution.
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Load a single module.
 *
 * If the load fails, an error is returned but a record is not created
 * for that module.
 *
 * \param context DSO library context pointer.
 * \param path Absolute path to the module to be loaded.  The filename
 * extension may either be provided or omitted.  If it is omitted, the
 * default extension for the current platform will be assumed.  On UNIX,
 * the default is ".so".
 * \param precedence What to do if caller tries to load a module with the
 * same name as one already loaded.
 * \param global_symbols Determines whether the symbols in this
 * module should be available for automatic linking by modules loaded
 * subsequently.  If is is 'true', modules loaded after this one can
 * refer to symbols from this one directly.  This corresponds to using
 * RTLD_GLOBAL in the UNIX dlopen() interface.  If it is 'false',
 * other modules may still use symbols from this one, but must first
 * get the pointers using lc_dso_module_get_symbol().
 */
int lc_dso_module_load_one_path(lc_dso_context *context, const char *path,
                                lc_dso_load_precedence precedence,
                                tbool global_symbols);

/**
 * Like lc_dso_module_load_one_path(), except that the path is split into
 * a directory and a filename.
 *
 * \param context DSO library context pointer.
 * \param dir Directory in which to search for the module.  Callers may 
 * not pass NULL for this.  For portability reasons, the UNIX dlopen() 
 * behavior of searching various directories in this case is not exposed.
 * \param filename Name of module to load, with or without the
 * extension.
 * \param precedence What to do if caller tries to load a module with the
 * same name as one already loaded.
 * \param global_symbols See lc_dso_module_load_one_path().
 */
int lc_dso_module_load_one_dirfile(lc_dso_context *context, const char *dir,
                                   const char *filename,
                                   lc_dso_load_precedence precedence,
                                   tbool global_symbols);

/* ------------------------------------------------------------------------- */
/** Load multiple modules.  Loads all modules in the specified
 * directory whose filenames have the specified prefix.  If NULL or
 * the empty string is provided for the prefix, all DSOs in the
 * directory are loaded.  If the loading of a module fails, this does
 * not preempt the loading of other modules.
 */
int lc_dso_module_load_mult(lc_dso_context *context, const char *dir,
                            const char *filename_prefix,
                            lc_dso_load_precedence precedence,
                            tbool global_symbols);

/* ------------------------------------------------------------------------- */
/** Unload one module.  Note that a
 * single module is specified according to its name (filename without
 * the extension) without a directory, as the name is unique within
 * the context.
 *
 * If a module is in the lds_initialized state immediately prior to
 * being unloaded, it is unloaded anyway but a warning is logged.
 *
 * If unloading fails (very rare), an appropriate error message is
 * logged, the module record is deleted anyway, and an error code is
 * returned.  This does not preempt the unloading of other modules.
 */
int lc_dso_module_unload_one(lc_dso_context *context,
                             const char *module_name);

/**
 * Unload all modules in this context.
 */
int lc_dso_module_unload_all(lc_dso_context *context);

/**
 * Unload all modules in this context, but only free the state,
 * do not call dlclose().  This may be useful if you are using
 * a memory leak tool like valgrind, and want to free all your memory
 * but still have all the module symbols present.
 */
int lc_dso_module_unload_all_free_only(lc_dso_context *context);

/*@}*/

/* ========================================================================= */
/** @name Initialization and deinitialization of modules
 *
 * This has the
 * effect of calling a function with a well-known name in the
 * module(s) specified, as well as keeping track of the module's
 * current state.
 *
 * The names of the init and deinit functions are formed by starting
 * with the filename of the module, chopping off the extension, and
 * appending lc_dso_init_func_suffix and lc_dso_deinit_func_suffix.
 * e.g. with the current values for those constants, if the DSO is
 * named cli_interface_cmds.so, the init and deinit functions must be
 * named cli_interface_cmds_init() and cli_interface_cmds_deinit().
 *
 * Modules may define a symbol that controls what order they are
 * initialized in relative to other modules.  To do so, define an
 * int32 variable whose name is that of the init function, plus
 * "_order" at the end.  Any modules who do not define this symbol
 * will be treated as if they had defined it with a value of 0.
 * Modules are initialized in ascending order of this value.  So for
 * example, to cause the interface module in the example above to be
 * initialized after all of the other modules (who did not declare an
 * ordering constant), the module could do this:
 *
 *   const int32 cli_interface_cmds_init_order = 100;
 *
 * The order that modules are initialized relative to others with the
 * same ordering constant is undefined.
 *
 * Modules will be deinitialized in the reverse order that they were
 * initialized.  (If this is not desirable, we could honor a separate
 * deinit order symbol.)
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Attempt to initialize one module.  If it was not previously in the
 * lds_loaded or lds_deinitialized state, an error is returned, and
 * the module is unaffected.  If initialization fails, an error is
 * returned, but the module is still moved to the initialized state.
 * The error code returned is not necessarily the same one the module
 * returned.
 *
 * The module name is its filename without the extension.
 *
 * If the module does not have an init function, the outcome depends
 * on 'require_init'.  If it is set, the module's state does not
 * change, and an error is returned.  If it is not set, the module is
 * moved to the initialized state, and success is returned.
 */
int lc_dso_module_init_one(lc_dso_context *context, const char *module_name,
                           tbool require_init, tbool log_info, void *data);

/* ------------------------------------------------------------------------- */
/** Attempt to initialize all modules which have not already been
 * initialized.
 *
 * Note that this will be done in order according to the ordering
 * constants defined by the modules, if any; see comment at beginning
 * of this section for details.
 *
 * If 'continue_on_failure' is set, all functions will be called even
 * if an earlier one fails (or is not present, in the case where
 * 'require_init' is set); otherwise, control is returned after the
 * first failure.  In any case, if any initializations fail, failure
 * is returned but all of the modules whose initialization functions
 * were called are moved into the initialized state.
 */
int lc_dso_module_init_all(lc_dso_context *context, tbool require_init,
                           tbool continue_on_failure, tbool log_info, 
                           void *data);

/* ------------------------------------------------------------------------- */
/** Attempt to deinitialize one module.  If it was not previously in the
 * lds_initialized state, an error is returned.  In other respects, this
 * behaves similarly to lc_dso_module_init_one().
 *
 * Note that even modules which failed initialization are
 * deinitialized.  This is because initialization failure may have
 * left some resources allocated which still need to be freed.
 */
int lc_dso_module_deinit_one(lc_dso_context *context, const char *module_name,
                             tbool require_deinit, tbool log_info, void *data);

/* ------------------------------------------------------------------------- */
/** Attempt to deinitialize all modules which are currently in the
 * initialized state.  In other respects, this behaves similarly to
 * lc_dso_module_init_all().
 */
int lc_dso_module_deinit_all(lc_dso_context *context, tbool require_deinit,
                             tbool continue_on_failure, tbool log_info, 
                             void *data);

/*@}*/

/* ========================================================================= */
/** @name Miscellaneous functions
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Get a pointer to a symbol in the specified module.  Note that only
 * the filename needs to be specified, not the full path, since only
 * one module with a given filename may be loaded within a single
 * context.
 *
 * The module name is its filename without the extension.
 *
 * The caller is responsible for casting the pointer returned to the
 * appropriate type, whether it is a pointer to a function or a
 * variable.
 */
int lc_dso_module_get_symbol(lc_dso_context *context, const char *module_name,
                             const char *symbol_name,
                             void **ret_symbol_ptr);

/* ------------------------------------------------------------------------- */
/** Get information about a specified module.
 *
 * The module name is its filename without the extension.
 */
int lc_dso_module_get_info(lc_dso_context *context, const char *module_name,
                           const lc_dso_info **ret_info);

/* ------------------------------------------------------------------------- */
/** Call a provided function for each module in the specified context.
 * The 'data' argument is passed to the function each time, along with
 * an lc_dso_info structure.  If a foreach function returns failure,
 * the iteration will be aborted immediately.
 *
 * Note: DO NOT unload modules from your foreach function.  This will
 * mess up the library's iteration as it will affect the position of
 * modules in the array that is being iterated over.
 */
int lc_dso_module_foreach(lc_dso_context *context,
                          lc_dso_foreach_func foreach_func, void *data);

/* ----------------------------------------------------------------------------
 * XXX/EMT: Could add ability to unload modules from foreach functions if
 * the need arose.  This would involve honoring the lc_err_foreach...
 * return codes.  lc_err_foreach_delete would be a request to unload
 * the module the function was just called with.
 */

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __DSO_H_ */

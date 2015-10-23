#include "agentd_modules.h"
#include "dso.h"
#include "tpaths.h"

/* ============================================================================
 * Statically linked
 *
 * When modules are added or removed, both the list of prototypes and
 * the list of calls to initialization functions must be updated.  If
 * the module has a deinitialization function, that must be added too.
 */
#ifdef STATIC_MODULES

/* ------------------------------------------------------------------------- */
int
agentd_load_modules(void)
{
    return(0);
}

/* ------------------------------------------------------------------------- */
int
agentd_init_modules(void)
{
    int err = 0;

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
agentd_deinit_modules(void)
{
    int err = 0;

 bail:    
    return(err);
}

/* ------------------------------------------------------------------------- */
int
agentd_unload_modules(void)
{
    return(0);
}


/* ------------------------------------------------------------------------- */
int
agentd_get_symbol(const char *module_name, const char *symbol_name,
               void **ret_symbol)
{
    int err = 0;

 bail:
    return(err);
}


/* ============================================================================
 * Dynamically linked
 */
#else

static const char agentd_module_filename_prefix[] = "agentd_";
static lc_dso_context *agentd_module_dso_context = NULL;
#define AGENTD_MODULE_PATH PROD_STATIC_ROOT "/lib/agentd/modules"

/* ------------------------------------------------------------------------- */
int
agentd_load_modules(void)
{
    int err = 0;

    err = lc_dso_context_new(&agentd_module_dso_context, NULL);
    bail_error_null(err, agentd_module_dso_context);

    err = lc_dso_module_load_mult(agentd_module_dso_context,
                                  AGENTD_MODULE_PATH,
                                  agentd_module_filename_prefix,
                                  llp_replace_old, false);
    bail_error(err);
    
 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
agentd_init_modules(void)
{
    int err = 0;

    err = lc_dso_module_init_all(agentd_module_dso_context, true, true, false,
                                 NULL);
    complain_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
agentd_deinit_modules(void)
{
    int err = 0;

    if (agentd_module_dso_context) {
        err = lc_dso_module_deinit_all(agentd_module_dso_context, false, true,
                                       false, NULL);
        bail_error(err);
    }

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
agentd_unload_modules(void)
{
    int err = 0;

    if (agentd_module_dso_context) {
        err = lc_dso_module_unload_all_free_only(agentd_module_dso_context);
        bail_error(err);
    }

    err = lc_dso_context_free(&agentd_module_dso_context);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
agentd_get_symbol(const char *module_name, const char *symbol_name,
               void **ret_symbol)
{
    int err = 0;

    err = lc_dso_module_get_symbol(agentd_module_dso_context, module_name,
                                   symbol_name, ret_symbol);
    bail_error(err);

 bail:
    return(err);
}

#endif

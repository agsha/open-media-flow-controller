/*
 *
 * webtmpl.h
 *
 *
 *
 */

#ifndef __WEBTMPL_H__
#define __WEBTMPL_H__


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <stdio.h>
#include "common.h"
#include "tstring.h"

/*=============================================================================
 * LIBWEBTMPL
 */

/* ========================================================================= */
/** \file webtmpl.h Template compiler API
 * \ingroup web
 *
 * This library contains functions for parsing TMS template files (*.tem) and
 * generate TCL files (*.tcl).
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char lwt_template_ext[] = ".tem";
static const char lwt_tcl_ext[] = ".tcl";

/*-----------------------------------------------------------------------------
 * API
 */

/**
 * Get the full path to the template file for the given template.
 * On error, ret_path will contain NULL.
 * \param tmpl_name the name of the template.
 * \param ret_path the path as a tstring.
 * \return 0 on success, error code on failure.
 */
int lwt_get_tmpl_path(const tstring *tmpl_name, tstring **ret_path);

/**
 * Get the full path to the tcl file for the given template.
 * On error, ret_path will contain NULL.
 * \param tcl_name the name of the template.
 * \param ret_path the path as a tstring.
 * \return 0 on success, error code on failure.  lc_err_bad_path is used
 * if tcl_name is invalid; different errors are used otherwise.
 */
int lwt_get_tcl_path(const tstring *tcl_name, tstring **ret_path);

/**
 * The given template will be converted from template form to tcl
 * if necessary. The definition of 'if necessary' is if the tcl
 * version doesn't exist OR the tcl version has a modified date
 * earlier than the template. The template name should NOT contain
 * any suffix, just the major part of the template name.
 * \param tmpl_name the name of the template to compile.
 * \return 0 on success, error code on failure.
 */
int lwt_compile_template(const tstring *tmpl_name);

/**
 * The given template will be converted from template form to tcl.
 * Any existing file will be overwritten.
 * If an error occurs, the contents of the destination path (tcl_path)
 * is undefined. It may be untouched, overwritten, or contain only
 * a portion of the template.
 * \param tmpl_path the path to the template to compile.
 * \param tcl_path the path to the resulting compiled template.
 * \return 0 on success, error code on failure.
 */
int lwt_compile_template_to(const tstring *tmpl_path, const tstring *tcl_path);

#ifdef __cplusplus
}
#endif

#endif /* __WEBTMPL_H_ */

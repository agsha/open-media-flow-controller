/*
 *
 * texpr.h
 *
 *
 *
 */

#ifndef __TEXPR_H_
#define __TEXPR_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "bnode_types.h"
#include "tstring.h"


/* ------------------------------------------------------------------------- */
/** \file texpr.h Tall Maple API for processing expressions.
 * \ingroup libtexpr
 *
 * This library processes expressions with a Scheme-like syntax.  They
 * are currently used only for advanced queries, but the mechanism
 * provided by this library is more general.  It evaluates expressions,
 * using libgcl's bn_attrib to hold typed values.
 *
 * See doc/dev/adv-queries.txt for documentation on how advanced
 * queries work.  Libtexpr is one of the components involved.  It knows
 * how to parse and evaluate expressions, following the grammar
 * documented in adv-queries.txt.  It implements several built-in
 * functions, namely all of the infrastructure functions that do not 
 * need an mdb_db to operate on.
 */


/**
 * Structure to hold all context data for the APIs in this library.
 * You must create one of these and pass it to every subsequent
 * libtexpr call.  In turn it will be passed back to all functions
 * called back while evaluating an expression.
 *
 * Note that the custom function registations are part of the context.
 * If for some reason you need multiple contexts, this is permitted,
 * but you must register any custom functions you need separately with
 * each context.
 */
typedef struct ltx_context {
    /**
     * Node name, potentially to be used in evaluating this
     * expression.  The caller should set this field before trying to
     * evaluate an expression that will use any functions that refer
     * to it.  In the case of advanced queries, this field contains
     * the name of the candidate wildcard, which the expression is
     * deciding whether or not will be returned in the query results.
     *
     * XXX/EMT: this and lxc_node_name_parts are a slight break in the
     * libtexpr abstraction.  Generally, it is for evaluating any type
     * of expression, while these two fields are specific to
     * libtexpr's use in handling advanced queries.  A caller who
     * wants to use this library for something else may leave these
     * fields blank, provided that the expressions they evaluate do
     * not call any functions that refer to them.  Such function names
     * begin with "WILD-" (returning some attribute of the named node
     * itself), or "CHILD-" (returning some attribute of a child of
     * the named node).
     *
     * This field is constant as far as ltx_context is concerned.
     * It is the caller's responsibility to maintain it, and free
     * it if necessary.
     */
    const char *lxc_node_name;

    /**
     * lxc_node_name, broken down into component parts.  This is
     * redundant information, provided only for efficiency.  The
     * libtexpr client should set this field before trying to evaluate
     * an expression that will use any functions that refer to it.
     *
     * This field is constant as far as ltx_context is concerned.
     * It is the caller's responsibility to maintain it, and free
     * it if necessary.
     */
    const tstr_array *lxc_node_name_parts;

    /**
     * Available for use by libtexpr client.  This field will never be 
     * modified by libtexpr.  Generally, the caller will set this field
     * for use by custom callback functions registered by the caller.
     */
    void *lxc_user_data;

    /**
     * For internal use by libtexpr; do not modify.
     */
    void *lxc_internal;

} ltx_context;


/**
 * A parsed expression, ready to be evaluated.
 */
typedef struct ltx_expr ltx_expr;
TYPED_ARRAY_HEADER_NEW_NONE(ltx_expr, ltx_expr *);


/**
 * Callback for a function to be invoked as part of an expression.
 *
 * \param func_name Name of the function being called.
 *
 * \param func_data Contents of the lfr_data field in the function
 * registration.
 *
 * \param args The arguments to the function, in expression form.
 * Callers can fetch the attributes out of these expressions using
 * ltx_get_arg_attrib_quick().
 *
 * \param context Context information, if any.  If the function's
 * registration has lfr_needs_context set to true, this is guaranteed
 * to be non-NULL.  Otherwise, it may be NULL, as the assumption when
 * lfr_needs_context is false is that you can operate solely on your
 * arguments.
 *
 * \retval ret_result The attribute that the function wants to return.
 */
typedef int (*ltx_func)(const char *func_name, void *func_data, 
                        ltx_expr_array *args,
                        const ltx_context *context,
                        bn_attrib **ret_result);


/*
 * Registration information for a function that can be used in
 * expressions.
 */
typedef struct ltx_func_reg {
    /**
     * Function name.  This is the string that will be used in
     * expressions to invoke the function.
     */
    const char *lxfr_name;

    /**
     * Type of attribute this function is going to return.  If the
     * function may return different types under different conditions,
     * set this to by_any.
     */
    bn_type lxfr_return_type;

    /**
     * Specifies whether this function requires an ltx_context to
     * operate.  Functions that can operate solely using their
     * arguments should pass false here.  This increases efficiency,
     * by allowing us to call the function only once, rather than
     * once per expression evaluation, provided that none of its
     * parameters needed a context to be evaluated.
     */
    tbool lxfr_needs_context;

    /**
     * C function pointer to be called when function is invoked.
     */
    ltx_func lxfr_func;

    /**
     * Pointer to be passed back to lxfr_func whenever it is called.
     */
    void *lxfr_data;

    /*
     * XXX/EMT: could allow registration of what parameters (how many,
     * what types) the function expects.  This would allow the
     * typechecking to be centralized in the infrastructure, and mean
     * less work for function writers.  But it would have to be
     * optional, and functions must be able to specify bt_any for the
     * types of their attributes.
     */

} ltx_func_reg;


/**
 * Create a new libtexpr context.
 */
int ltx_context_new(ltx_context **ret_context);


/**
 * Register a function for use in the provided libtexpr context.
 */
int ltx_reg_func(ltx_context *context, const ltx_func_reg *reg);


/**
 * Register an array of functions for use in the provided libtexpr
 * context.  The array of functions MUST be terminated by a record
 * whose lxfr_func field is NULL.
 */
int ltx_reg_funcs(ltx_context *context, const ltx_func_reg reg[]);


/**
 * Free a libtexpr context.  This does not do anything to free the
 * lxc_user_data field, so you should free your own data there before
 * calling this.  Nor does it free the lxc_node_name or
 * lxc_node_name_parts fields, which are also the caller's
 * responsibility.
 */
int ltx_context_free(ltx_context **inout_context);


/**
 * Parse a single expression from string form, and create an ltx_expr
 * data structure (possibly recursively nested, to contain other
 * expressions found within).  This also pre-evaluates any
 * subexpressions that can be evaluated (i.e. resolved to an
 * attribute) because they are functions marked as not requiring a
 * context.
 *
 * \param context Expression library context.
 * \param expr_str String containing the expression to parse.
 * \retval ret_expr_parsed An structure containing the parsed expression.
 * \retval ret_next_offset The offset into the provided string of the
 * next non-whitespace character in the string following the end of
 * the expression.  If there was nothing else in the string after the
 * expression, this will point to the terminating NUL character.
 */
int ltx_parse(ltx_context *context, const char *expr_str,
              ltx_expr **ret_expr_parsed, int32 *ret_next_offset);


/**
 * Free an expression, and recursively all other expressions within it.
 */
int ltx_expr_free(ltx_expr **inout_expr);


/**
 * Parse a string containing multiple expressions stacked end-to-end
 * into an expression array.
 */
int ltx_parse_mult(ltx_context *context, const char *exprs_str,
                   ltx_expr_array **ret_exprs_parsed);


/**
 * Evaluate an expression, given the provided context, and return a copy
 * of the attribute which the outermost expression evaluated to.
 *
 * Note that an expression can be reevaluated any number of times.
 * Each time, the results of any previous evaluations that depended on
 * the context are thrown out, and recomputed anew.  But the results
 * of function calls that did not depend on context (and had only
 * parameters that did not depend on the context) are retained.
 */
int ltx_eval(ltx_context *context, ltx_expr *expr, 
             bn_attrib **ret_result);


/**
 * Clear out all variable definitions held in the provided context.
 * This is the only way variables ever become undefined; otherwise,
 * variable assignments will carry forward from one evaluation to the
 * next.
 */
int ltx_vars_clear(ltx_context *context);


/**
 * Clear the context of all variable definitions, evaluate an array of
 * multiple expressions, and return a copy of the value of the last
 * expression.
 */
int ltx_eval_all(ltx_context *context, ltx_expr_array *exprs, 
                 bn_attrib **ret_result);



/**
 * For implementations of callback functions.  Resolve the specified
 * argument to an attribute, and return a const pointer to that
 * attribute.  Note that this will fail if the argument cannot be
 * resolved, e.g. if it is an unknown variable reference.
 */
const bn_attrib *ltx_get_arg_attrib_quick(const ltx_context *context, 
                                          ltx_expr_array *args, uint32 idx);


#ifdef __cplusplus
}
#endif

#endif /* __TEXPR_H_ */

/*
 *
 * tcompiler.h
 *
 *
 *
 */

#ifndef __TCOMPILER_H_
#define __TCOMPILER_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file src/include/tcompiler.h Compiler-specific macros for type testing.
 * \ingroup lc
 */

/* As we do not include common.h, check if this include is prohibited */
#if defined(NON_REDIST_HEADERS_PROHIBITED)
#error Non-redistributable header file included when prohibited
#endif

/**
 * Given a (constant at built time) expression, fail the compilation if the
 * expression is true.  This is done by trying to look at sizeof(char[1]) or
 * sizeof(char[-1]), where the latter is not a valid expression to compile.
 */
#define COMPILE_FAIL(expr) ((void)sizeof(char[1 - 2 * !!(expr)]))

/**
 * Like COMPILE_FAIL, except that it evaluates to a number, so it can be
 * used as a term in arthimetic expressions.  Returns a (size_t) 0 on
 * success, and otherwise fails the compilation.
 */
#define COMPILE_FAIL_NUM(expr) (sizeof(char[1]) - sizeof(char[1 - 2 * !!(expr)]))

#ifndef __cplusplus

/* If var is of type 'char *' then 1, else 0 */
#define VAR_TYPE_IS_CHAR_STAR(var) (__builtin_types_compatible_p(__typeof__(var), char *))
/* If var is of type 'char []' then 1, else 0 */
#define VAR_TYPE_IS_CHAR_ARRAY(var) (__builtin_types_compatible_p(__typeof__(var), char []))
/* If var is of given type then 1, else 0 */
#define VAR_TYPE_IS_TYPE(var,type) (__builtin_types_compatible_p(__typeof__(var), type))
/* If var is some type of array then 1, else 0 */
#define VAR_TYPE_IS_ARRAY(var) (!__builtin_types_compatible_p(__typeof__(var), __typeof__(&var[0])))

#define COMPILE_FAIL_NOT_CHAR_STAR(var) COMPILE_FAIL(!VAR_TYPE_IS_CHAR_STAR(var))
#define COMPILE_FAIL_NOT_CHAR_ARRAY(var) COMPILE_FAIL(!VAR_TYPE_IS_CHAR_ARRAY(var))
#define COMPILE_FAIL_IS_TYPE(var,type) COMPILE_FAIL(VAR_TYPE_IS_TYPE(var,type))
#define COMPILE_FAIL_NOT_ARRAY(var) COMPILE_FAIL(!VAR_TYPE_IS_ARRAY(var))
#define COMPILE_FAIL_NOT_ARRAY_NUM(var) COMPILE_FAIL_NUM(!VAR_TYPE_IS_ARRAY(var))

#else

#define VAR_TYPE_IS_CHAR_STAR(var) (0)
#define VAR_TYPE_IS_CHAR_ARRAY(var) (0)
#define VAR_TYPE_IS_TYPE(var,type) (0)
#define VAR_TYPE_IS_ARRAY(var) (0)
#define COMPILE_FAIL_NOT_CHAR_STAR(var) ((void) 0)
#define COMPILE_FAIL_NOT_CHAR_ARRAY(var) ((void) 0)
#define COMPILE_FAIL_IS_TYPE(var,type) ((void) 0)
#define COMPILE_FAIL_NOT_ARRAY(var) ((void) 0)
#define COMPILE_FAIL_NOT_ARRAY_NUM(var) ((void) 0)

#endif



#ifdef __cplusplus
}
#endif

#endif /* __TCOMPILER_H_ */

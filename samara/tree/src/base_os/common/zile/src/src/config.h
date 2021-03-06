/* src/config.h.  Generated by configure.  */
/* src/config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
#define HAVE_UTIME_NULL 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define this to support the xterm resizing. */
#define SUPPORT_XTERM_RESIZE 1

/* Define this to remove the debugging assertions.  */
#define NDEBUG 1

/* Define this to include debugging code.  */
/* #undef DEBUG */

/* Define if you have the ncurses library (-lncurses).  */
#define HAVE_LIBNCURSES 1

/* Define this to use the ncurses screen handling library.  */
#define USE_NCURSES 1

/* The date of compilation.  */
#define CONFIGURE_DATE "Fri Sep 19 2003"

/* The host of compilation.  */
#define CONFIGURE_HOST "jellobot.tallmaple.com"

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the vasprintf function.  */
#define HAVE_VASPRINTF 1

/* Define if you have the xmalloc function.  */
/* #undef HAVE_XMALLOC */

/* Define if you have the xrealloc function.  */
/* #undef HAVE_XREALLOC */

/* Define if you have the xstrdup function.  */
/* #undef HAVE_XSTRDUP */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <ncurses.h> header file.  */
#define HAVE_NCURSES_H 1

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

#ifdef USE_NCURSES
#ifndef HAVE_LIBNCURSES
#error ncurses is required for compilation.
#endif
#endif

#include <stddef.h>

#ifndef HAVE_XMALLOC
extern void *xmalloc(size_t);
#endif

#ifndef HAVE_XREALLOC
extern void *xrealloc(void *, size_t);
#endif

#ifndef HAVE_XSTRDUP
extern char *xstrdup(const char *);
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

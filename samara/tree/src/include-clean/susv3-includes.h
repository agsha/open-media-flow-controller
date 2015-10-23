/*
 *
 * susv3-includes.h
 *
 *
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/*
 *  This list contains every header file listed in Single UNIX Specification,
 *   Version 3 (SUSv3) .
 *
 *  The intented use is to include this in common.h, and then do a full build.
 *  any conflicting symbols will then be detected.  For instance, the symbol
 *  'index' commonly creeps into the source base.
 *
 *  See also test/general/include_test.sh , and src/include/stdcpp-includes.h .
 */

#include <arpa/inet.h>
#include <assert.h>
#ifndef __cplusplus
#include <complex.h>
#endif
#include <cpio.h>
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <float.h>
#include <fmtmsg.h>
#include <fnmatch.h>
#include <ftw.h>
#include <glob.h>
#include <grp.h>
#include <iconv.h>
#include <inttypes.h>
#include <iso646.h>
#include <langinfo.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <monetary.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nl_types.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <regex.h>
#include <sched.h>
#include <search.h>
#include <semaphore.h>
#include <setjmp.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syslog.h>
#include <tar.h>
#include <termios.h>
#ifndef __cplusplus
#include <tgmath.h>
#endif
#include <time.h>
#include <ucontext.h>
#include <ulimit.h>
#include <unistd.h>
#include <utime.h>
#include <utmpx.h>
#include <wchar.h>
#include <wctype.h>
#include <wordexp.h>
#include <aio.h>


/* XXX These all seem to cause trouble if included */

#if 0
#include <trace.h>
#include <mqueue.h>
#include <ndbm.h>
#include <stropts.h>
#endif



#ifdef __cplusplus
}
#endif

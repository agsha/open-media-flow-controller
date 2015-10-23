/*
 *
 * src/bin/web/request_handler/rh_debug.h
 *
 *
 *
 */

#ifndef __RH_DEBUG_H_
#define __RH_DEBUG_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <unistd.h>

/*
 * Defining CGI_DEBUG causes debug code to be generated into the program.
 * This debug code enables certain command line flags to read CGI env/input
 * from a given file. Also, debug mode causes the executable to generate
 * env/input file in /tmp that can later be used to run the program from
 * the command line using the aformentioned command line flags.
 *
 * program [-f env/input-file-name]
 */
#ifdef CGI_DEBUG

int rh_debug_setup(int argc, char *argv[]);
void rh_debug_cleanup(void);

#define RH_POST_FD                      rh_post_fd
extern int rh_post_fd;

#else

#define RH_POST_FD                      STDIN_FILENO

#endif /* CGI_DEBUG */


#ifdef __cplusplus
}
#endif

#endif /* __RH_DEBUG_H_ */

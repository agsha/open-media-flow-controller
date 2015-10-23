#ifndef SNMP_PARSE_ARGS_H
#define SNMP_PARSE_ARGS_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parsing of command line arguments succeeded and application is expected
 * to continue with normal operation.
 */
#define NETSNMP_PARSE_ARGS_SUCCESS       0
/**
  * Parsing of command line arguments succeeded, but the application is expected
  * to exit with zero exit code. For example, '-V' parameter has been found.
  */
#define NETSNMP_PARSE_ARGS_SUCCESS_EXIT  -2
/**
 * Parsing of command line arguments failed and application is expected to show
 * usage (i.e. list of parameters) and exit with nozero exit code..
 */
#define NETSNMP_PARSE_ARGS_ERROR_USAGE   -1
/**
 * Parsing of command line arguments failed and application is expected to exit
 * with nozero exit code.  netsnmp_parse_args() has already printed what went
 * wrong.
 */
#define NETSNMP_PARSE_ARGS_ERROR         -3

/**
 *  @return 0 on success, -1 on failure (usage should be printed), -2 on success
 *  when the application termination is expected (like using -V parameter) and
 *  -3 on failure, when netsnmp_parse_args already printed enough information
 *  and no usage should be printed.
 */
int
snmp_parse_args(int argc, char **argv, netsnmp_session *session,
		const char *localOpts, void (*proc)(int, char *const *, int));

void
snmp_parse_args_descriptions(FILE *);

void
snmp_parse_args_usage(FILE *);

#ifdef __cplusplus
}
#endif
#endif

/*
 * Filename :   nkn_nknexecd.h
 * Date:        23 February 2012
 * Author:      Rajagopal Andra (randra@juniper.net)
 *
 * (C) Copyright 2012, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _NKNEXECD_API_H
#define _NKNEXECD_API_H

#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"

#define NKNEXECD_MAX_EVENTS   65535
#define NKNEXECD_MAGIC        0x0ECD0ECD
#define NKNEXECD_MAGIC_DEAD   0xdead0ECD

#define NKNEXECD_SERVER_SOCKET	"/config/nkn/.nknexecd_d"
#define NKNEXECD_CLIENT_SOCKET	"/config/nkn/.nknexecd_c"

#define NKNEXECD_MAXCMD_LEN	    160
#define NKNEXECD_MAXBASENAME_LEN    40
#define NKNEXECD_MAXFILENAME_LEN    64

enum NKNEXECD_commands {
    NKNEXECD_NONE = 0,
    NKNEXECD_RUN,
};

typedef struct nkn_payload_s
{
    /* Command to execute */
    char command[NKNEXECD_MAXCMD_LEN]; 
    /* temporary file basename to be used for stdout/stderr */
    char basename[NKNEXECD_MAXBASENAME_LEN]; 
} nkn_payload_t;

typedef struct nknexecd_req_s
{
    uint32_t req_magic;
    uint32_t req_version;
    uint32_t req_length;  // length of the cmd
    uint32_t req_op_code; // operation to perform, comes from nknexecd commands
    nkn_payload_t req_payload;
} nknexecd_req_t;

typedef struct nknexecd_reply_s
{
    uint32_t reply_magic;
    uint32_t reply_version;
    uint32_t reply_length;
    uint32_t reply_reply_code; // reply code for request
    uint32_t reply_system_retcode; // return code for system() system call
    char reply_stdoutfile[NKNEXECD_MAXFILENAME_LEN]; //  std output file
    char reply_stderrfile[NKNEXECD_MAXFILENAME_LEN]; //  std error file
} nknexecd_reply_t;

typedef struct nknexecd_retval_s
{
    uint32_t retval_reply_code; // reply code for request
    char retval_stdoutfile[NKNEXECD_MAXFILENAME_LEN]; //  std output file
    char retval_stderrfile[NKNEXECD_MAXFILENAME_LEN]; //  std error file
} nknexecd_retval_t;


int nknexecd_process_request(nknexecd_req_t *in_msg, nknexecd_reply_t *out_msg);
extern int nknexecd_run(char *command, char *absename,
			nknexecd_retval_t *retval);
int nkn_print_file_to_syslog(char *filename, int no_of_lines);
void nknexecd_print_req(nknexecd_req_t *in_msg);
void nknexecd_print_reply(nknexecd_reply_t *out_msg);
void nknexecd_print_retval(nknexecd_retval_t *retval);

#endif /* _NKNEXECD_API_H */

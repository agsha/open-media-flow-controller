/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * header.c  Create pre-filled header for TACACS+ request.
 */

#include "tacplus.h"
#include "libtac.h"
#include "xalloc.h"
#include "magic.h"
#include <netinet/in.h>

/* Miscellaneous variables that are global, because we need
 * store their values between different functions and connections.
 */
/* Session identifier. */
u_int session_id;
static int call_magic_init = TRUE;

/* Returns pre-filled TACACS+ packet header of given type.
 * 1. you MUST fill th->datalength and th->version
 * 2. you MAY fill th->encryption
 * 3. you are responsible for freeing allocated header 
 * By default packet encryption is enabled. The version
 * field depends on the TACACS+ request type and thus it
 * cannot be predefined.
 */
HDR *
_tac_req_header(u_char type, u_char seq_no)
{
    HDR *th;

    if (call_magic_init) {
	magic_init();
	call_magic_init = FALSE;
    }

    th = (HDR *) xcalloc(1, TAC_PLUS_HDR_SIZE);

    /* preset some packet options in header */
    th->type = type;
    th->seq_no = seq_no;
    th->encryption = TAC_PLUS_ENCRYPTED;
 
    /* make session_id from pseudo-random number */
    if (seq_no == TAC_PLUS_INIT_SEQNO) {
	session_id = magic();
    }
    th->session_id = htonl(session_id);

    return(th);
}

/*
 * Figure out the TACACS minor version number based on message
 * type, action, etc.
 */
u_char
protocol_version(int msg_type, int var, int type)
{
    u_char minor;

    switch (msg_type) {
        case TAC_PLUS_AUTHEN:
	    /* 'var' represents the 'action' */
	    switch (var) {
	        case TAC_PLUS_AUTHEN_LOGIN:
		    switch (type) {

		        case TAC_PLUS_AUTHEN_TYPE_PAP:
			case TAC_PLUS_AUTHEN_TYPE_CHAP:
			case TAC_PLUS_AUTHEN_TYPE_ARAP:
			    minor = 1;
			break;

			default:
			    minor = 0;
			break;
		     }
		break;

		case TAC_PLUS_AUTHEN_SENDAUTH:
		    minor = 1;
		break;

		default:
		    minor = 0;
		break;
	    };
	break;

	case TAC_PLUS_AUTHOR:
	    /* 'var' represents the 'method' */
	    switch (var) {
	        /*
		 * When new authentication methods are added, include 'method'
		 * in determining the value of 'minor'.  At this point, all
                 * methods defined in this implementation (see "Authorization 
                 * authentication methods" in taclib.h) are minor version 0
		 * Not all types, however, indicate minor version 0.
		 */
	        case AUTHEN_METH_NOTSET:
                case AUTHEN_METH_NONE:
                case AUTHEN_METH_KRB5:
                case AUTHEN_METH_LINE:
                case AUTHEN_METH_ENABLE:
                case AUTHEN_METH_LOCAL:
                case AUTHEN_METH_TACACSPLUS:
                case AUTHEN_METH_RCMD:
		    switch (type) {
		        case TAC_PLUS_AUTHEN_TYPE_PAP:
			case TAC_PLUS_AUTHEN_TYPE_CHAP:
			case TAC_PLUS_AUTHEN_TYPE_ARAP:
			    minor = 0;
			break;

			default:
			    minor = 0;
			break;
		     }
	        break;
	        default:
		    minor = 0;
		break;
	    }
        break;

	default:
	    minor = 0;
        break;
    }

    return(TAC_PLUS_MAJOR_VER | minor);
}

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/cdefs.h>

#include "nkn_defs.h"
#include "nkn_elf.h"
#include "nkn_debug.h"

/*
 * Display Symbol
 */
#define MSG_TYPE_STRING	1
#define MSG_TYPE_HEX	2

typedef struct msg_display_symbol {
	int type;
	uint64_t address;
	char * name;
} msg_display_symbol_t;
#define msg_display_symbol_s sizeof(struct msg_display_symbol)

/*
 * Write Symbol
 */
typedef struct msg_write_symbol {
	uint64_t value;
	char * name;
} msg_write_symbol_t;
#define msg_write_symbol_s sizeof(struct msg_write_symbol)

/*
 * Call function
 */
typedef struct msg_call_func {
	int argc;
	char * name;
	char * arg1;
	char * arg2;
	char * arg3;
	char * arg4;
	char * arg5;
} msg_call_func_t;
#define msg_call_func_s sizeof(struct msg_call_func)


static int display_symbol(int argc, char ** argv);
static int write_symbol(int argc, char ** argv);
static int call_func(int argc, char ** argv);
static int display_symbol_in_hex(char * pbuf, int lbuf, void * p, int size);
static int display_symbol_default(char * pbuf, int lbuf, void * p, int size);
static int write_symbol_default(void * p, int size, uint64_t value);
int nkn_printf(const char * fmt, ...);
int nkn_puts(const char *s);

/*
 * ----------------------------------------------------------
 * The function set used to display a symbol value in the nvsd
 */
static int display_symbol_in_string(char * pbuf, int lbuf, void * p, int size)
{
	int i, len;
	char * pc;

	UNUSED_ARGUMENT(size);

	pc = (char *)*(uint64_t *)p;

	len = snprintf(pbuf, lbuf, "0x%p \"", pc);
	pbuf += len;

	for(i=0; i<20; i++) {
		if( !isascii(*pc) || *pc==0 ) break;
		*pbuf = *pc;
		pbuf ++;
		pc ++;
	}
	*pbuf='\"'; pbuf++;
	*pbuf = 0;

	return len+i+2;
}

static int display_symbol_in_hex(char * pbuf, int lbuf, void * p, int size)
{
	int len;

	switch(size) {
	case 1: 
		len=snprintf(pbuf, lbuf, "0x%x", *(uint8_t *)p); 
		break;
	case 2: 
		len=snprintf(pbuf, lbuf, "0x%x", *(uint16_t *)p); 
		break;
	case 4: 
		len=snprintf(pbuf, lbuf, "0x%x", *(uint32_t *)p); 
		break;
	case 8: 
		len=snprintf(pbuf, lbuf, "0x%lx", *(uint64_t *)p); 
		break;
	default: 
		len=snprintf(pbuf, lbuf, "does not exist");
		break;
	}

	return len;
}

static int display_symbol_default(char * pbuf, int lbuf, void * p, int size)
{
	int len;

	switch(size) {
	case 1: 
		len=snprintf(pbuf, lbuf, "%c", *(uint8_t *)p); 
		break;
	case 2: 
		len=snprintf(pbuf, lbuf, "%d", *(uint16_t *)p); 
		break;
	case 4: 
		len=snprintf(pbuf, lbuf, "%d", *(uint32_t *)p); 
		break;
	case 8: 
		len=snprintf(pbuf, lbuf, "%ld", *(uint64_t *)p); 
		break;
	default: 
		len=snprintf(pbuf, lbuf, "does not exist");
		break;
	}

	return len;
}


static int display_symbol(int argc, char ** argv)
{
	msg_display_symbol_t pdisplay;
	char buf[1024];
	int len, ret, size;
	void * p;

	/*
	 * Format: p [/x] [/s] symbole
	 */
	if(argc < 2) return 0;

	if(argv[1][0] == '/') {
		// we have option
		if(argc < 3) return 0;
		if(argv[1][1] == 'x') pdisplay.type = MSG_TYPE_HEX;
		else if(argv[1][1] == 's') pdisplay.type = MSG_TYPE_STRING;
		else pdisplay.type = 0;
		pdisplay.name = argv[2];
	}
	else {
		pdisplay.type = 0;
		pdisplay.name = argv[1];
	}
	pdisplay.address = 0;

	ret =  nkn_get_symbol_addr(pdisplay.name, &p, &size);
	if(ret == 0) {
		DBG_LOG(SEVERE, MOD_SYSTEM, "%s does not exist.", pdisplay.name);
		return 0;
	}

	len=snprintf(buf, sizeof(buf), "%s: ", pdisplay.name); 
	if(pdisplay.type == MSG_TYPE_STRING) {
		len += display_symbol_in_string(&buf[len], sizeof(buf)-len, p, size);
	}
	else if(pdisplay.type == MSG_TYPE_HEX) {
		len += display_symbol_in_hex(&buf[len], sizeof(buf)-len, p, size);
	}
	else {
		len += display_symbol_default(&buf[len], sizeof(buf)-len, p, size);
	}

	DBG_LOG(SEVERE, MOD_SYSTEM, "%s", buf);
	return 1;
}


/*
 * ----------------------------------------------------------
 * The function set used to write a value into a variable
 */

static int write_symbol_default(void * p, int size, uint64_t value)
{
	switch(size) {
	case 1: 
		*(uint8_t *)p = (uint8_t)value;
		break;
	case 2: 
		*(uint16_t *)p = (uint16_t)value;
		break;
	case 4: 
		*(uint32_t *)p = (uint32_t)value;
		break;
	case 8: 
		*(uint64_t *)p = (uint64_t)value;
		break;
	default: 
		return 0;
	}

	return 1;
}

static int write_symbol(int argc, char ** argv)
{
	msg_write_symbol_t pwritesym;
	int ret, size;
	char * p;
	void * psym;

	/*
	 * Format: w symbole value
	 */
	if(argc < 3) return 0;


	pwritesym.name = argv[1];
	if( (argv[2][0] == '0') && (argv[2][1] == 'x') ) {
		// COnvert Hex to uint64_t
		p=&argv[2][2];
		pwritesym.value = 0;
		while(*p != 0) {
			pwritesym.value <<= 4;
			if(*p>='0' && *p<='9') pwritesym.value += *p-'0';
			else if(*p>='a' && *p<='f') pwritesym.value += *p-'a'+10;
			else if(*p>='A' && *p<='F') pwritesym.value += *p-'A'+10;
			else { return 0; } // Error
			p++;
		}
	}
	else {
		pwritesym.value = atol(argv[2]);
	}

	ret =  nkn_get_symbol_addr(pwritesym.name, &psym, &size);
	if(ret == 0) {
		DBG_LOG(SEVERE, MOD_SYSTEM, "%s does not exist.", pwritesym.name);
		return 0;
	}

	write_symbol_default(psym, size, pwritesym.value);
	DBG_LOG(SEVERE, MOD_SYSTEM, "%s: done.", pwritesym.name); 

	return 1;
}

/*
 * ----------------------------------------------------------
 * The function set used to call a function
 */
#define MAX_BUF_SIZE    4096
static char conmsg_buf[MAX_BUF_SIZE];
static int conmsg_buf_filled=0;

typedef void (* FUNC_DUMP)(void);
typedef int (* FUNC_printf)(const char * fmt, ...);
typedef int (* FUNC_puts)(const char *s);
static FUNC_printf psys_printf = NULL;
static FUNC_puts psys_puts = NULL;

int nkn_printf(const char * fmt, ...)
{
        int ret;
        va_list ap;
        char *cp;

        if(MAX_BUF_SIZE-conmsg_buf_filled < 100) return 0;

        va_start(ap, fmt);
        cp = conmsg_buf;
        cp += conmsg_buf_filled;
        ret = vsnprintf(cp, (size_t)(MAX_BUF_SIZE-conmsg_buf_filled), fmt, ap);
        va_end(ap);

        conmsg_buf_filled += ret;
        return ret;
}

int nkn_puts(const char *s)
{
        int len = 0;
        const char * ps;

        ps = s;
        while(*ps!=0) { ps++; len++; }
        if(len >= MAX_BUF_SIZE - conmsg_buf_filled)
                return 0;
        memcpy(&conmsg_buf[conmsg_buf_filled], s, len);
        conmsg_buf_filled += len;
        return len;
}

static int call_func(int argc, char ** argv)
{
	msg_call_func_t pcall;
	int ret, size;
	void * p;
	FUNC_DUMP pf;

	/*
	 * Format: call func (arg1, arg2 ... )
	 */
	if(argc < 2) return 0;

	pcall.argc = argc - 2;
	pcall.name = argv[1];

	ret =  nkn_get_symbol_addr(pcall.name, &p, &size);
	if(ret == 0) {
		DBG_LOG(SEVERE, MOD_SYSTEM, "%s does not exist", pcall.name);
		return 0;
	}

        pf = (FUNC_DUMP)p;
        psys_printf = (FUNC_printf) nkn_replace_func("printf", (void *)nkn_printf);
        psys_puts = (FUNC_puts) nkn_replace_func("puts", (void *)nkn_puts);
        (*pf) ();
        nkn_replace_func("printf", (void *)psys_printf);
        nkn_replace_func("puts", (void *)psys_puts);

	conmsg_buf[conmsg_buf_filled] = 0;
	DBG_LOG(SEVERE, MOD_SYSTEM, "%s", conmsg_buf);

	return 1;
}


#define	IFS		"\t \n(),"

#define	TOK_KEEP	1
#define	TOK_EAT		2

#define	WINCR		40	// NTGO: limit the max expression to 80, value was 20.
#define	AINCR		10

#define	tok_malloc(a)		nkn_malloc_type(a, mod_rtsp_ses_hp_malloc)
#define	tok_free(a)		free(a)
#define	tok_realloc(a, b)	nkn_realloc_type(a, b, mod_rtsp_ses_hp_malloc)

typedef enum {
	T_none, T_single, T_double, T_one, T_doubleone
} quote_t;


typedef struct tokenizer {
	char	*ifs;		/* In field separator			 */
	int	 argc, amax;	/* Current and maximum number of args	 */
	char   **argv;		/* Argument list			 */
	char	*wptr, *wmax;	/* Space and limit on the word buffer	 */
	char	*wstart;	/* Beginning of next word		 */
	char	*wspace;	/* Space of word buffer			 */
	quote_t	 quote;		/* Quoting state			 */
	int	 flags;		/* flags;				 */
} Tokenizer;


void tok_finish(Tokenizer *tok);
Tokenizer * tok_init(const char *ifs);
void tok_reset(Tokenizer *tok);
void tok_end(Tokenizer *tok);
int tok_line(Tokenizer *tok, const char *line, int *argc, char ***argv);
int debug_run_command(const char *line);

/* tok_finish():
 *	Finish a word in the tokenizer.
 */
void
tok_finish(Tokenizer *tok)
{

	*tok->wptr = '\0';
	if ((tok->flags & TOK_KEEP) || tok->wptr != tok->wstart) {
		tok->argv[tok->argc++] = tok->wstart;
		tok->argv[tok->argc] = NULL;
		tok->wstart = ++tok->wptr;
	}
	tok->flags &= ~TOK_KEEP;
}


/* tok_init():
 *	Initialize the tokenizer
 */
Tokenizer *
tok_init(const char *ifs)
{
	Tokenizer *tok = (Tokenizer *) tok_malloc(sizeof(Tokenizer));

	UNUSED_ARGUMENT(ifs);

	tok->ifs = nkn_strdup_type(ifs ? ifs : IFS, mod_rtsp_ses_hp_malloc);
	tok->argc = 0;
	tok->amax = AINCR;
	tok->argv = (char **) tok_malloc(sizeof(char *) * tok->amax);
	if (tok->argv == NULL)
		return (NULL);
	tok->argv[0] = NULL;
	tok->wspace = (char *) tok_malloc(WINCR);
	if (tok->wspace == NULL)
		return (NULL);
	tok->wmax = tok->wspace + WINCR;
	tok->wstart = tok->wspace;
	tok->wptr = tok->wspace;
	tok->flags = 0;
	tok->quote = T_none;

	return (tok);
}


/* tok_reset():
 *	Reset the tokenizer
 */
void
tok_reset(Tokenizer *tok)
{

	tok->argc = 0;
	tok->wstart = tok->wspace;
	tok->wptr = tok->wspace;
	tok->flags = 0;
	tok->quote = T_none;
}


/* tok_end():
 *	Clean up
 */
void
tok_end(Tokenizer *tok)
{

	tok_free((char *) tok->ifs);
	tok_free((char *) tok->wspace);
	tok_free((char *) tok->argv);
	tok_free((char *) tok);
}



/* tok_line():
 *	Bourne shell like tokenizing
 *	Return:
 *		-1: Internal error
 *		 3: Quoted return
 *		 2: Unmatched double quote
 *		 1: Unmatched single quote
 *		 0: Ok
 */
int
tok_line(Tokenizer *tok, const char *line, int *argc, char ***argv)
{
	const char *ptr;

	for (;;) {
		switch (*(ptr = line++)) {
		case '\'':
			tok->flags |= TOK_KEEP;
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case T_none:
				tok->quote = T_single;	/* Enter single quote
							 * mode */
				break;

			case T_single:	/* Exit single quote mode */
				tok->quote = T_none;
				break;

			case T_one:	/* Quote this ' */
				tok->quote = T_none;
				*tok->wptr++ = *ptr;
				break;

			case T_double:	/* Stay in double quote mode */
				*tok->wptr++ = *ptr;
				break;

			case T_doubleone:	/* Quote this ' */
				tok->quote = T_double;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		case '"':
			tok->flags &= ~TOK_EAT;
			tok->flags |= TOK_KEEP;
			switch (tok->quote) {
			case T_none:	/* Enter double quote mode */
				tok->quote = T_double;
				break;

			case T_double:	/* Exit double quote mode */
				tok->quote = T_none;
				break;

			case T_one:	/* Quote this " */
				tok->quote = T_none;
				*tok->wptr++ = *ptr;
				break;

			case T_single:	/* Stay in single quote mode */
				*tok->wptr++ = *ptr;
				break;

			case T_doubleone:	/* Quote this " */
				tok->quote = T_double;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		case '\\':
			tok->flags |= TOK_KEEP;
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case T_none:	/* Quote next character */
				tok->quote = T_one;
				break;

			case T_double:	/* Quote next character */
				tok->quote = T_doubleone;
				break;

			case T_one:	/* Quote this, restore state */
				*tok->wptr++ = *ptr;
				tok->quote = T_none;
				break;

			case T_single:	/* Stay in single quote mode */
				*tok->wptr++ = *ptr;
				break;

			case T_doubleone:	/* Quote this \ */
				tok->quote = T_double;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		case '\n':
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case T_none:
				tok_finish(tok);
				*argv = tok->argv;
				*argc = tok->argc;
				return (0);

			case T_single:
			case T_double:
				*tok->wptr++ = *ptr;	/* Add the return */
				break;

			case T_doubleone:   /* Back to double, eat the '\n' */
				tok->flags |= TOK_EAT;
				tok->quote = T_double;
				break;

			case T_one:	/* No quote, more eat the '\n' */
				tok->flags |= TOK_EAT;
				tok->quote = T_none;
				break;

			default:
				return (0);
			}
			break;

		case '\0':
			switch (tok->quote) {
			case T_none:
				/* Finish word and return */
				if (tok->flags & TOK_EAT) {
					tok->flags &= ~TOK_EAT;
					return (3);
				}
				tok_finish(tok);
				*argv = tok->argv;
				*argc = tok->argc;
				return (0);

			case T_single:
				return (1);

			case T_double:
				return (2);

			case T_doubleone:
				tok->quote = T_double;
				*tok->wptr++ = *ptr;
				break;

			case T_one:
				tok->quote = T_none;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		default:
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case T_none:
				if (strchr(tok->ifs, *ptr) != NULL)
					tok_finish(tok);
				else
					*tok->wptr++ = *ptr;
				break;

			case T_single:
			case T_double:
				*tok->wptr++ = *ptr;
				break;


			case T_doubleone:
				*tok->wptr++ = '\\';
				tok->quote = T_double;
				*tok->wptr++ = *ptr;
				break;

			case T_one:
				tok->quote = T_none;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);

			}
			break;
		}

		if (tok->wptr >= tok->wmax - 4) {
			size_t size = tok->wmax - tok->wspace + WINCR;
			char *s = (char *) tok_realloc(tok->wspace, size);
			/* SUPPRESS 22 */
			int offs = s - tok->wspace;
			if (s == NULL)
				return (-1);

			if (offs != 0) {
				int i;
				for (i = 0; i < tok->argc; i++)
					tok->argv[i] = tok->argv[i] + offs;
				tok->wptr = tok->wptr + offs;
				tok->wstart = tok->wstart + offs;
				tok->wmax = s + size;
				tok->wspace = s;
			}
		}
		if (tok->argc >= tok->amax - 4) {
			char **p;
			tok->amax += AINCR;
			p = (char **) tok_realloc(tok->argv,
			    tok->amax * sizeof(char *));
			if (p == NULL)
				return (-1);
			tok->argv = p;
		}
	}
}

/* parse_line():
 *	Parse a line and dispatch it
 */
int debug_run_command(const char *line)
{
	char ** argv;
	int argc;
	Tokenizer *tok;
	int ret=0;

	tok = tok_init(NULL);
	tok_line(tok, line, &argc, &argv);

	if( argc==0 ) goto done;

	if( (strcmp(argv[0], "print")==0) || (strcmp(argv[0], "p")==0) )
	{
		ret = display_symbol(argc, argv);
	}
	else if( (strcmp(argv[0], "set")==0) || (strcmp(argv[0], "s")==0) )
	{
		ret = write_symbol(argc, argv);
	}
	else if( (strcmp(argv[0], "call")==0) || (strcmp(argv[0], "c")==0) )
	{
		ret = call_func(argc, argv);
	}
	else {
		DBG_LOG(SEVERE, MOD_SYSTEM, "Parsing Error. Maybe not supported command.");
	}

done:
	tok_end(tok);

	return (ret);
}

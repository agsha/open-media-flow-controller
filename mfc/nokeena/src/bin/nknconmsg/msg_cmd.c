#include <sys/types.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/cdefs.h>
#include <string.h>
#include <stdlib.h>

extern int display_symbol(int argc, char ** argv);
extern int write_symbol(int argc, char ** argv);
extern int call_func(int argc, char ** argv);

#define	IFS		"\t \n(),"

#define	TOK_KEEP	1
#define	TOK_EAT		2

#define	WINCR		40	// NTGO: limit the max expression to 80, value was 20.
#define	AINCR		10

#define	tok_malloc(a)		malloc(a)
#define	tok_free(a)		free(a)
#define	tok_realloc(a, b)	realloc(a, b)

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
int msg_parse_line(const char *line);

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

	tok->ifs = strdup(ifs ? ifs : IFS);
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
int
msg_parse_line(const char *line)
{
	char ** argv;
	int argc;
	Tokenizer *tok;
	int i;
	int ret=0;

	tok = tok_init(NULL);
	tok_line(tok, line, &argc, &argv);

	if( argc==0 ) goto done;

	if( strcmp(argv[0], "p")==0 )
	{
		ret = display_symbol(argc, argv);
	}
	else if( strcmp(argv[0], "w")==0 )
	{
		ret = write_symbol(argc, argv);
	}
	else if( strcmp(argv[0], "call")==0 )
	{
		ret = call_func(argc, argv);
	}

	if (ret == 0) {
		printf("\tParsing Error. Maybe not supported command.\n");
	}

done:
	tok_end(tok);

	return (ret);
}

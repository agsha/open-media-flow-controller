%{
#include <stdio.h>
#include "y.tab.h"

extern void yyerror(const char *str);
extern int yylex(void);
extern int read_in_string(char *buffer, int *num_bytes, int max_bytes);
extern int verbose;

%}

%token OBRACE NAME COMMA VALUE EBRACE

%%

nv_pair_set:
		| nv_pair_set nv_pair
		{
			(verbose) ? fprintf(stderr, "Parse nv_pair_set name_value\n") : 0;
		}
		| nv_pair_set nv_pair COMMA
		{
			(verbose) ? fprintf(stderr, "Parse nv_pair_set name_value with comma\n") : 0;
		}
		;

nv_pair:
		name_value
		{
			(verbose) ? fprintf(stderr, "Parse name-value\n") : 0;
		}
       	;

name_value:
		OBRACE NAME COMMA VALUE EBRACE
		{
			(verbose) ? fprintf(stderr,"N-V Pair\n") : 0;
		}
		;

%%

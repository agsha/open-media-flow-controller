%{
#include <stdio.h>
#include <string.h>
#include "y.tab.h"


#undef YY_INPUT
#define YY_INPUT(b,r,s) read_in_string(b,&r,s)
//extern int yylval;
extern int verbose;
void yyerror(const char* s);
extern int read_in_string( char *buffer, int *num_bytes, int max_bytes);
%}

%%

[a-zA-Z0-9._]+   { 
						if (!strncmp(yytext, "http.req.custom.flvseek_offset", 30) || 
							!strncmp(yytext, "http.req.custom.range_start", 27) || 
							!strncmp(yytext, "http.req.custom.range_end", 25)) {
							return NAME;
						} else {
							yyerror("Not a pre-defined name. Please use one of the following names,\n"
									"http.req.custom.flvseek_offset\n"
									"http.req.custom.range_start\n"
									"http.req.custom.range_end");
							return -1;
						}
					}

[\$0-9]+ 	{
				yylval=atoi(yytext+1); 
				(verbose) ? fprintf(stderr, "yylval=%d, str=%s\n", yylval, (yytext+1)) : 0;
				if ((yylval>=1) && (yylval<=9)) { 
					return VALUE;
				} else {
					yyerror("$ value should range from [1-9] only");
					return -1;
				}
			}

[ \t]+		{
				yyerror("Whitespace character(s) not allowed");
				return -1;
			}

[;'"`]+		{
				yyerror("Invalid character(s)");
				return -1;
			}	
,           return COMMA;
\(          return OBRACE;
\)          return EBRACE;
\n          /*ignore*/

%%

void yyerror(const char *str)
{
    fprintf(stderr,"Error: %s\n",str);
}

int yywrap()
{
    return -1;
}


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "nkn_common_config.h"
#include "nkn_mgmt_agent.h"
#include "nkn_mgmt_smi.h"
#include "nkn_defs.h"
#include "nkn_util.h"

char **split_on_spaces(const char *str);


static char * mark_end_of_line(char * p)
{
  while(1) {
    if(*p=='\r' || *p=='\n' || *p=='\0') {
      *p=0;
      return p;
    }
    p++;
  }
  return NULL;
}

static char * skip_to_value(char * p)
{
  while(*p==' '||*p=='\t') p++;
  if(*p!='=') return NULL;
  p++;
  while(*p==' '||*p=='\t') p++;
  return p;
}

char **split_on_spaces(const char *str)
{
  int nwords = 0, k = 0;
  char *p = nkn_strdup_type(str, mod_mgmta_charbuf);
  char *q = p;
  for( ; *p != 0; )
  {
    while ( ( *p == ' ' ) || ( *p == '\r' ) || ( *p == '\t' ) || ( *p == '\n' ) ) p++; 
    nwords++;
    while ( ( *p != ' ' ) && ( *p != '\r' ) && ( *p != '\t' ) && ( *p != '\n' ) 
            && ( *p != '\0' ) ) p++;
  }
  if ( !nwords ) return NULL; 
  char **m = (char **)nkn_malloc_type(nwords*sizeof(char *), mod_mgmta_charbuf);
  for( ; *q != 0; )
  {
    while ( ( *q == ' ' ) || ( *q == '\r' ) || ( *q == '\t' ) || ( *q == '\n' ) ) q++; 
    if ( *q != 0 ) { m[k] = q; k++; }
    while ( ( *q != ' ' ) && ( *q != '\r' ) && ( *q != '\t' ) && ( *q != '\n' ) 
            && ( *q != '\0' ) ) q++;
    if ( *q != 0 ) { *q = 0; q++; }
  }
  return m;
}

typedef void (*CfgEntry)(char *);
 
int read_nkn_cfg(char * configfile)
{
  FILE * fp;
  char * p;
  char buf[1024];
  int  len, i;

  if (configfile==NULL) {
    configfile = (char*)NKNSRV_CONF_FILE;
  }

  fp = fopen(configfile,"r");
  if ( fp == NULL ) {
    printf("Unable to open configuration file %s\n", configfile);
    exit(0);
  }

  while( !feof(fp) ) {
    if(fgets(buf, 1024, fp) == NULL) break;

    p=buf;
    if(*p=='#' || *p == '\r' || *p == '\n') continue;
    mark_end_of_line(p);

    int assigned = 0;
    for(i=0; !assigned; i++) {
      NknCfgParamValue *cfgParams = &nknCfgParams[i];
      const char *name = cfgParams->defn.name;
      if ( name == NULL ) break;
      len = strlen(name);
      if ( strncmp( buf, name, len ) == 0 ) {
        p = skip_to_value( &buf[len] );

        switch( cfgParams->defn.type ) 
	{
        case NKN_INT_TYPE:
	  if( (strlen(p)>=3) && (p[0]=='0') && (p[1]=='x') ) {
          	*((int *)cfgParams->pAddr) = convert_hexstr_2_int32(p+2); 
	  }
	  else {
          	*((int *)cfgParams->pAddr) = atoi(p);
	  }
          assigned = 1;
          break;
        case NKN_LONG_TYPE:
	  if( (strlen(p)>=3) && (p[0]=='0') && (p[1]=='x') ) {
          	*((int *)cfgParams->pAddr) = convert_hexstr_2_int64(p+2); 
	  }
	  else {
          	*((long *)cfgParams->pAddr) = atol(p);
	  }
          assigned = 1;
          break;
        case NKN_FLOAT_TYPE:
          *((float *)cfgParams->pAddr) = (float)atof(p);
          assigned = 1;
          break;
        case NKN_DOUBLE_TYPE:
          *((double *)cfgParams->pAddr) = atof(p);
          assigned = 1;
          break;
        case NKN_STR_PTR_TYPE:
          *((char **)cfgParams->pAddr) = nkn_strdup_type(p, mod_mgmta_charbuf);
          assigned = 1;        
          break;
        case NKN_STR_LIST_TYPE:
	  *((char ***)cfgParams->pAddr) = split_on_spaces(p); 
	  assigned = 1;
	  break;
        case NKN_FUNC_TYPE:
	  (*((CfgEntry)cfgParams->pAddr))(p);
	  assigned = 1;
          break;
        case NKN_CB_FUNC_TYPE:
	  (*((CfgEntry)cfgParams->pAddr))(p);
	  assigned = 1;
          break;
        default:
          break;
        }
      }
    }
  }
  fclose(fp);
  return NKN_SUCCESS;
}

#include <string.h>
#include <stdlib.h>

#include "lf_common_defs.h"
#include "lf_config_file_read_api.h"
#include "lf_err.h"

static char * mark_end_of_line(char * p);
static char * skip_to_value(char * p);
static int32_t convert_hexstr_2_int32(char * s);
static uint64_t convert_hexstr_2_uint64(char * s);

int32_t
lf_read_cfg_file(const char *cfg_file,
		 lf_cfg_param_val_t *cfg_param)
{
    FILE *fp = NULL;
    char *p;
    char buf[1024];
    int32_t len, i = 0, err = 0;

    if (cfg_file == NULL) {
	err = -E_LF_INVAL;
	return err;
    }

    fp = fopen(cfg_file, "r");
    if (!fp) {
	err = -E_LF_OPEN_CFG_FILE;
	goto error;
    }
    
    while (!feof(fp)) {
	if (fgets(buf, 1024, fp) == NULL) break;
	
	p = buf;
	if (*p == '#' || *p == '\r' || *p == '\n') continue;
	
	mark_end_of_line(p);
	
	int32_t assigned = 0;
	for (i =0; !assigned; i++) {
	    lf_cfg_param_val_t *cfg = &cfg_param[i];
	    const char *name = cfg->defn.name;
	    if (name == NULL) break;
	    len = strlen(name);
	    if (!strncmp(buf, name, len)) {
		p = skip_to_value(&buf[len]);
		
		switch (cfg->defn.type) {
		    case LF_INT_TYPE:
			if( (strlen(p)>=3) && (p[0]=='0') &&
			    (p[1]=='x') ) {
			    *((int *)cfg->p_val) =
				convert_hexstr_2_int32(p+2);
			}
			else {
			    *((int *)cfg->p_val) = atoi(p);
			}
			assigned = 1;
			break;
		     case LF_LONG_TYPE:
		     	if( (strlen(p)>=3) && (p[0]=='0') &&
				(p[1]=='x') ) {
				*((uint64_t *)cfg->p_val) =
					convert_hexstr_2_uint64(p+2);
			}
			else {
				*((uint64_t *)cfg->p_val) = atoll(p);
			}
			assigned = 1;
			break;
			 case LF_STR_VAL_TYPE:
			*(char**)(cfg->p_val) = (void*)strdup(p);
			assigned = 1;
			break;
		}
	    }
	}
    }
    fclose(fp);
    return err;

 error:
    if (fp) fclose(fp);
    return err;
}

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

static int32_t convert_hexstr_2_int32(char * s)
{
    //0x12345678
    int32_t val=0;
    int i;
    
    if(s == NULL) return 0;
    
    for(i=0;i<8;i++) {
	if(*s==0) break;
	
	val <<= 4;
	if (*s<='9' && *s >='0') val += *s - '0';
	else if (*s<='F' && *s >='A') val += (*s - 'A') + 10;
	else if (*s<='f' && *s >='a') val += (*s - 'a') + 10;
	else break;
	    
	s++;
    }
    return val;
}

static uint64_t convert_hexstr_2_uint64(char * s)
{
    //0x12345678
    uint64_t val=0;
    int i;
    
    if(s == NULL) return 0;
    
    for(i=0;i<16;i++) {
	if(*s==0) break;
	
	val <<= 4;
	if (*s<='9' && *s >='0') val += *s - '0';
	else if (*s<='F' && *s >='A') val += (*s - 'A') + 10;
	else if (*s<='f' && *s >='a') val += (*s - 'a') + 10;
	else break;
	    
	s++;
    }
    return val;
}
 

 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>

#include "nkn_om.h"
#include "om_defs.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"


static pthread_mutex_t cfg_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////
// check if configuration is right
////////////////////////////////////////////////////////////////////////

#if 0
// This function will check if the configuration is right
// Or convert the unit

void http_cfg_content_type_callback(char * s);
void http_no_cfg_content_type_callback(char * s);
const char * get_http_content_type(const char * uri, int len);

void http_cfg_suppress_response_hdr_callback(char *s);
void http_no_cfg_suppress_response_hdr_callback(char *s);
int http_suppress_response_hdr(const char *name, int namelen);

void http_cfg_add_response_hdr_callback(char *s);
void http_no_cfg_add_response_hdr_callback(char *s);
int http_get_nth_add_response_hdr(int nth_element, 
			const char **name, int *namelen, 
			const char **value, int *valuelen);

void http_cfg_allowed_content_types_callback(char *s);
void http_no_cfg_allowed_content_types_callback(char *s);
int allow_content_type(const char *uri, int urilen);

#if 0
void om_suppress_cache_suffix_callback(char *s);
void om_no_suppress_cache_suffix_callback(char *s);
int om_suppress_uri_from_cache(const char *uri, int len);

void ssp_cfg_customer_type_callback(char * s);
void ssp_no_cfg_customer_type_callback(char * s);
const char* ssp_lookup_cust_type(const char *cust);
#endif //0

void nkn_http_portlist_callback(char * http_port_list_str);
void nkn_rtsp_vod_portlist_callback(char * rtsp_vod_port_list_str);
void nkn_rtsp_live_portlist_callback(char * rtsp_live_port_list_str);
#endif // 0

////////////////////////////////////////////////////////////////////////
// Tool Functions
////////////////////////////////////////////////////////////////////////

// This function will modify the original string to mark the end of token.
// Eventually this function should move to common folder.
// return token and ship pointer of p
static char * get_next_token (char * pin, int * shift)
{
        char * token;
        char * p = pin;

        if(p==NULL) return NULL;

        // skip the leading space
        while(*p==' '||*p=='\t') {
                if(*p==0) return NULL; // No more token
                p++;
        }
        if(*p==0) return NULL; // No more token

        // starting pointer of token
        token = p;

        // mark the end of token
        while(*p!=' ') {
                if(*p==0) { goto done; }
                p++;
        }
        *p=0; p++;

done:
        *shift = p-pin;
        return token;
}


////////////////////////////////////////////////////////////////////////
// Configuration of om.entry-list
////////////////////////////////////////////////////////////////////////

#if 0
NKNCNT_DEF(om_tot_cfg_policy, int, "", "Total configured OM policy list")
static om_cfg_t * omcfg_list = NULL;

void om_cfg_entry_list_callback(char * cfg)
{
	char * domain, * ip, * port, * prefix;
	char * p;
	int len;
	om_cfg_t * pcl;

        if(cfg==NULL) {
		// This is mostly called at initialization time.
		return;
        }

	pcl = (om_cfg_t *)nkn_malloc_type( sizeof(om_cfg_t), mod_mgmta_charbuf );
	if( !pcl ) return;

	// Format of the string will be "server port prefix"
	p = cfg;

	// get domain
	domain = get_next_token (p, &len);
	if(!domain) return; //error
	p+=len;

        // get ip
	ip = get_next_token (p, &len);
	if(!ip) return; //error
	p+=len;

        // get port
	port = get_next_token (p, &len);
	if(!port) return; //error
	p+=len;

	// get proxy
	prefix = get_next_token (p, &len);
	if(!prefix) return; //error
	p+=len;

	pcl->domain = nkn_strdup_type(domain, mod_mgmta_charbuf);
	pcl->ip = nkn_strdup_type(ip, mod_mgmta_charbuf);
	pcl->port = htons(atoi(port));
	pcl->prefix = nkn_strdup_type(prefix, mod_mgmta_charbuf);
	pcl->prefix_len = strlen(prefix);
	pcl->next = NULL;

	/*
	printf("domain=%s port=%d prefix=%s\n",
		omcfg_list[ glob_om_tot_cfg_policy ].domain,
		ntohs(omcfg_list[ glob_om_tot_cfg_policy ].port),
		omcfg_list[ glob_om_tot_cfg_policy ].prefix);
	*/
	if( omcfg_list == NULL ) {
		omcfg_list=pcl;
	}
	else {
		om_cfg_t * ptmp;
		ptmp=omcfg_list;
		while(ptmp->next) { ptmp=ptmp->next; }
		ptmp->next=pcl;
	}
	glob_om_tot_cfg_policy ++;
}

om_cfg_t * om_lookup_cfg(char * uri)
{
        om_cfg_t * pcfg;

        //printf("om_lookup_cfg: called\n");

        //printf("om_lookup_cfg: uri=%s \n", uri);
	pcfg = omcfg_list;
        while( pcfg ) {
            //printf("prefix=%s <%d>\n", pcfg->prefix, pcfg->prefix_len);
            if(strncmp(uri, pcfg->prefix, pcfg->prefix_len)==0)
            {
		return pcfg;
            }
	    pcfg = pcfg->next;
        }
        printf("uri %s does not match any prefix\n", uri);
        return NULL; // Didn't find any prefix match
}


////////////////////////////////////////////////////////////////////////
// Configuration of om.suppress_cache_suffix
////////////////////////////////////////////////////////////////////////

typedef struct om_suppress_suffix_type {
	struct om_suppress_suffix_type *next;
	char *suffix;
	int suffix_len;
} om_suppress_suffix_type_t;

static om_suppress_suffix_type_t *om_suppress_cache_suffix_list = NULL;

void om_suppress_cache_suffix_callback(char *s)
{
	om_suppress_suffix_type_t *pcfg;
	char *suffix;
	int len;

	suffix = get_next_token(s, &len);
	if( !suffix ) return;
	s += len;
	
	pcfg = (om_suppress_suffix_type_t *)
		nkn_malloc_type(sizeof(om_suppress_suffix_type_t), mod_mgmta_charbuf);
	if (!pcfg) return;

	pcfg->suffix = nkn_strdup_type(suffix, mod_mgmta_charbuf);
	pcfg->suffix_len = strlen(suffix);
	pcfg->next = NULL;

	pthread_mutex_lock(&cfg_mutex);

        if( om_suppress_cache_suffix_list == NULL ) {
                om_suppress_cache_suffix_list=pcfg;
        } else {
                om_suppress_suffix_type_t * ptmp;
                ptmp = om_suppress_cache_suffix_list;
                while(ptmp->next) { ptmp=ptmp->next; }
                ptmp->next=pcfg;
        }

	pthread_mutex_unlock(&cfg_mutex);
	return;
}

void om_no_suppress_cache_suffix_callback(char *s)
{
	om_suppress_suffix_type_t * pcfg, * pcfgnext;

	if( !s || !om_suppress_cache_suffix_list ) return;

	pthread_mutex_lock(&cfg_mutex);

	pcfg = om_suppress_cache_suffix_list;
	if (strncmp(s, pcfg->suffix, pcfg->suffix_len)==0) {
		om_suppress_cache_suffix_list = pcfg->next;
		free(pcfg->suffix);
		free(pcfg);
	}
	else {
		while(pcfg->next) {
			pcfgnext = pcfg->next;
			if (strncmp(s, pcfgnext->suffix, pcfgnext->suffix_len)==0) {
				pcfg->next = pcfgnext->next;
				free(pcfgnext->suffix);
				free(pcfgnext);
				break;
			}
			pcfg = pcfg->next;
		}
	}
	pthread_mutex_unlock(&cfg_mutex);
	return;
}

int om_suppress_uri_from_cache(const char *uri, int len)
{
	om_suppress_suffix_type_t *pcfg;
	char *p;

	if(!uri) return 0;
	p=memchr(uri, '.', len);
	if(!p) return 0;

	pthread_mutex_lock(&cfg_mutex);

        pcfg = om_suppress_cache_suffix_list;
        while( pcfg ) {
            if(strncmp(p, pcfg->suffix, pcfg->suffix_len)==0)
            {
		pthread_mutex_unlock(&cfg_mutex);
                return 1; // No cache
            }
            pcfg = pcfg->next;
        }

	pthread_mutex_unlock(&cfg_mutex);
        return 0; // No match, allow caching
}

#endif // 0

////////////////////////////////////////////////////////////////////////
// Configuration of http.content-type
////////////////////////////////////////////////////////////////////////

typedef struct http_content_type {
	struct http_content_type * next;
	char * suffix;
	int suffix_len;
	char * content_type;
} http_content_type_t;

static http_content_type_t * hct_list = NULL;


void http_cfg_content_type_callback(char * s)
{
	http_content_type_t * pcfg;
	char * suffix;
	char * content_type;
	int len;

	suffix = get_next_token(s, &len);
	if( !suffix ) return ;
	s+=len;
	
	while(isspace(*s)) s++;
	len = strlen(s);
	content_type = s;
        if( !content_type ) return ;

	pcfg = (http_content_type_t *)nkn_malloc_type(sizeof(http_content_type_t), mod_mgmta_charbuf);
	if( !pcfg ) return;

	pcfg->suffix = nkn_strdup_type(suffix, mod_mgmta_charbuf);
	pcfg->suffix_len = strlen(suffix);
	pcfg->content_type = nkn_strdup_type(content_type, mod_mgmta_charbuf);
	pcfg->next = NULL;

	pthread_mutex_lock(&cfg_mutex);

        if( hct_list == NULL ) {
                hct_list=pcfg;
        }
        else {
                http_content_type_t * ptmp;
                ptmp=hct_list;
                while(ptmp->next) { ptmp=ptmp->next; }
                ptmp->next=pcfg;
        }

	pthread_mutex_unlock(&cfg_mutex);
	return;
}

void http_no_cfg_content_type_callback(char * s)
{
	http_content_type_t * pcfg, * pcfgnext;

	if( !s || !hct_list ) return ;
	
	pthread_mutex_lock(&cfg_mutex);

	pcfg = hct_list;
        if (strncmp(s, pcfg->suffix, pcfg->suffix_len)==0) {
                hct_list=pcfg->next;
		free(pcfg->suffix);
		free(pcfg->content_type);
		free(pcfg);
        }
        else {
		while(pcfg->next) {
			pcfgnext = pcfg->next;
			if (strncmp(s, pcfgnext->suffix, pcfgnext->suffix_len)==0) {
				pcfg->next = pcfgnext->next;
				free(pcfgnext->suffix);
				free(pcfgnext->content_type);
				free(pcfgnext);
				break;
			}
			pcfg = pcfg->next;
		}
        }

	pthread_mutex_unlock(&cfg_mutex);
	return;
}

const char * get_http_content_type(const char * uri, int len)
{
	const char * def_type = "text/html";
	static char type_backup[64];
        http_content_type_t * pcfg;
	const char * p;

	if(!uri) return def_type;
	p=memchr(uri, '.', len);
	if(!p) return def_type;
	pthread_mutex_lock(&cfg_mutex);

        pcfg = hct_list;
        while( pcfg ) {
	    if (len > pcfg->suffix_len) {
		p = uri + (len - pcfg->suffix_len);
		if (*p == '.' 
			&& (strncmp(p, pcfg->suffix, pcfg->suffix_len)==0)) {
		    strcpy(type_backup, pcfg->content_type);
		    pthread_mutex_unlock(&cfg_mutex);
		    return type_backup; // We cannot use pcfg->content_type here
		}
	    }
            pcfg = pcfg->next;
        }

	pthread_mutex_unlock(&cfg_mutex);
        return def_type; // Didn't find any prefix match
}


////////////////////////////////////////////////////////////////////////
// Configuration of http.suppress-response-header
////////////////////////////////////////////////////////////////////////

typedef struct name_descriptor {
     struct name_descriptor *next;
     char *name;
     int namelen;
} name_descriptor_t;

static name_descriptor_t *http_suppress_response_hdr_head = 0;

void http_cfg_suppress_response_hdr_callback(char *s)
{
    char *hdrname;
    int len;
    name_descriptor_t *nd;

    while ((hdrname = get_next_token(s, &len))) {
	if(!hdrname) { 
	    return;
	}
	nd = (name_descriptor_t *)nkn_malloc_type(sizeof(name_descriptor_t), mod_mgmta_charbuf);
	if (nd) {
	    nd->name = nkn_strdup_type(hdrname, mod_mgmta_charbuf);
	    nd->namelen = strlen(hdrname);

	    pthread_mutex_lock(&cfg_mutex);
	    if (http_suppress_response_hdr_head) {
		nd->next = http_suppress_response_hdr_head;
		http_suppress_response_hdr_head = nd;
	    } else {
		nd->next = 0;
		http_suppress_response_hdr_head = nd;
	    }
	    pthread_mutex_unlock(&cfg_mutex);
	}
	s += len;
    }
    return;
}

void http_no_cfg_suppress_response_hdr_callback(char *s)
{
	name_descriptor_t *nd, *ndnext;

	if(!s || !http_suppress_response_hdr_head) {
		return;
	}

	pthread_mutex_lock(&cfg_mutex);

	nd = http_suppress_response_hdr_head;
	if (strncasecmp(s, nd->name, nd->namelen) == 0) {
		http_suppress_response_hdr_head = nd->next;
		free(nd->name);
		free(nd);
	}
	else {
		while(nd->next) {
			ndnext = nd->next;
			if (strncasecmp(s, ndnext->name, ndnext->namelen) == 0) {
				nd->next = ndnext->next;
				free(ndnext->name);
				free(ndnext);
				break;
			}
			nd = nd->next;
		}
	}

	pthread_mutex_unlock(&cfg_mutex);
	return;
}

int http_suppress_response_hdr(const char *name, int namelen)
{
    name_descriptor_t *nd;

    pthread_mutex_lock(&cfg_mutex);
    nd = http_suppress_response_hdr_head;
    while (nd) {
        if ((nd->namelen == namelen) && 
		(strncasecmp(nd->name, name, namelen) == 0)) {
    	    pthread_mutex_unlock(&cfg_mutex);
	    return 1; // Match
	}
	nd = nd->next;
    }
    pthread_mutex_unlock(&cfg_mutex);
    return 0; // No Match
}

////////////////////////////////////////////////////////////////////////
// Configuration of http.add-response-header
////////////////////////////////////////////////////////////////////////

typedef struct config_name_value_descriptor {
     struct config_name_value_descriptor *next;
     char *name;
     int namelen;
     char *value;
     int valuelen;
} config_name_value_descriptor_t;

static config_name_value_descriptor_t *http_add_response_hdr_head = 0;

void http_cfg_add_response_hdr_callback(char *s)
{
    char *hdrname;
    char *value;
    int len;
    config_name_value_descriptor_t *nd;

    while ((hdrname = get_next_token(s, &len))) {
	if (!hdrname) { 
	    return;
	}
	s += len;

    	value = get_next_token(s, &len);
	if (!value) { 
	    return;
	}
	s += len;

	nd = (config_name_value_descriptor_t *)
		nkn_malloc_type(sizeof(config_name_value_descriptor_t), mod_mgmta_charbuf);
	if (nd) {
	    nd->name = nkn_strdup_type(hdrname, mod_mgmta_charbuf);
	    nd->namelen = strlen(hdrname);
	    nd->value = nkn_strdup_type(value, mod_mgmta_charbuf);
	    nd->valuelen = strlen(value);

    	    pthread_mutex_lock(&cfg_mutex);
	    if (http_add_response_hdr_head) {
		nd->next = http_add_response_hdr_head;
		http_add_response_hdr_head = nd;
	    } else {
		nd->next = 0;
		http_add_response_hdr_head = nd;
	    }
    	    pthread_mutex_unlock(&cfg_mutex);
	}
    }
    return;
}

void http_no_cfg_add_response_hdr_callback(char *s)
{
	config_name_value_descriptor_t *nd, * ndnext;

	if (!s || !http_add_response_hdr_head) {
		return;
	}

	pthread_mutex_lock(&cfg_mutex);

	nd = http_add_response_hdr_head;
	if (strncmp(s, nd->name, nd->namelen) == 0) {
		http_add_response_hdr_head = nd->next;
		free(nd->name);
		free(nd->value);
		free(nd);
	}
	else {
		while(nd->next) {
			ndnext = nd->next;
			if (strncmp(s, ndnext->name, ndnext->namelen) == 0) {
				nd->next = ndnext->next;
				free(ndnext->name);
				free(ndnext->value);
				free(ndnext);
				break;
			}
			nd = nd->next;
		}
	}
	
	pthread_mutex_unlock(&cfg_mutex);
	return;
}

int http_get_nth_add_response_hdr(int nth_element, 
			const char **name, int *namelen, 
			const char **value, int *valuelen)
{
    config_name_value_descriptor_t *nd;
    int n = 0;

    pthread_mutex_lock(&cfg_mutex);
    nd = http_add_response_hdr_head;
    while (nd) {
        if (n == nth_element) {
	    *name = nd->name;
	    *namelen = nd->namelen;
	    *value = nd->value;
	    *valuelen = nd->valuelen;

    	    pthread_mutex_unlock(&cfg_mutex);
	    return 0;
	}
	nd = nd->next;
	n++;
    }
    pthread_mutex_unlock(&cfg_mutex);
    return 1; // Selected element not found
}

////////////////////////////////////////////////////////////////////////
// Configuration of http.allowed-content-types
////////////////////////////////////////////////////////////////////////

static name_descriptor_t *http_allowed_content_types_head = 0;

void http_cfg_allowed_content_types_callback(char *s)
{
    char *type;
    int len;
    name_descriptor_t *nd;

    while ((type = get_next_token(s, &len))) {
	if(!type) { 
	    return;
	}
	nd = (name_descriptor_t *)nkn_malloc_type(sizeof(name_descriptor_t), mod_mgmta_charbuf);
	if (nd) {
	    nd->name = nkn_strdup_type(type, mod_mgmta_charbuf);
	    nd->namelen = strlen(type);

	    pthread_mutex_lock(&cfg_mutex);
	    if (http_allowed_content_types_head) {
		nd->next = http_allowed_content_types_head;
		http_allowed_content_types_head = nd;
	    } else {
		nd->next = 0;
		http_allowed_content_types_head = nd;
	    }
	    pthread_mutex_unlock(&cfg_mutex);
	}
	s += len;
    }
    return;
}

void http_no_cfg_allowed_content_types_callback(char *s)
{
	name_descriptor_t *nd, *ndnext;

	if (!s || !http_allowed_content_types_head) {
		return;
	}

	pthread_mutex_lock(&cfg_mutex);

	nd = http_allowed_content_types_head;
	if (strncasecmp(s, nd->name, nd->namelen) == 0) {
		http_allowed_content_types_head = nd;
		free(nd->name);
		free(nd);
        }
	else {
		while(nd->next) {
			ndnext = nd->next;
			if (strncasecmp(s, ndnext->name, ndnext->namelen) == 0) {
				nd->next = ndnext->next;
				free(ndnext->name);
				free(ndnext);
				break;
			}
			nd = nd->next;
		}
	}

	pthread_mutex_unlock(&cfg_mutex);
	return;
}

int allow_content_type(const char *uri, int urilen)
{
    const char *ctype;
    int ctype_len;

    if (!http_allowed_content_types_head) {
        return 1; // Allowed 
    }

    ctype = get_http_content_type(uri, urilen);
    if (!ctype) {
    	return 0; // Not allowed
    }
    ctype_len = strlen(ctype);

    name_descriptor_t *nd;

    pthread_mutex_lock(&cfg_mutex);
    nd = http_allowed_content_types_head;
    while (nd) {
        if ((nd->namelen == ctype_len) && 
		(strncasecmp(nd->name, ctype, ctype_len) == 0)) {
    	    pthread_mutex_unlock(&cfg_mutex);
	    return 1; // Match, allow
	}
	nd = nd->next;
    }
    pthread_mutex_unlock(&cfg_mutex);
    return 0; // No Match
}

////////////////////////////////////////////////////////////////////////
// Configuration of customer type ssp.cust-type
////////////////////////////////////////////////////////////////////////

#if 0

typedef struct ssp_customer_type {
  struct ssp_customer_type * next;
  char *cust_type;
} ssp_customer_type_t;

static ssp_customer_type_t *sct_list = NULL;

void ssp_cfg_customer_type_callback(char * s)
{
  ssp_customer_type_t *pcfg;
  char *cust_type;
  int len, i=0;

  while ((cust_type = get_next_token(s, &len))) {
    if (!cust_type)
      return;

    i=0;
    pcfg = (ssp_customer_type_t *)nkn_malloc_type(sizeof(ssp_customer_type_t), mod_mgmta_charbuf);
    if( !pcfg ) return;

    pcfg->cust_type = nkn_strdup_type(cust_type, mod_mgmta_charbuf);

    while(pcfg->cust_type[i]){ // Convert to lower case
      pcfg->cust_type[i] = tolower(pcfg->cust_type[i]);
      i++;
    }

    pcfg->next = NULL;

    pthread_mutex_lock(&cfg_mutex);
    if( sct_list == NULL ) {
      sct_list=pcfg;
    }
    else {
      ssp_customer_type_t *ptmp;
      ptmp=sct_list;
      while(ptmp->next) { ptmp=ptmp->next; }
      ptmp->next=pcfg;
    }
    pthread_mutex_unlock(&cfg_mutex);

    s += len;
  }

  return;
}

void ssp_no_cfg_customer_type_callback(char * s)
{
	ssp_customer_type_t *pcfg, *pcfgnext;


	if (!s || !sct_list) {
		return;
	}

	pthread_mutex_lock(&cfg_mutex);

	
	pthread_mutex_unlock(&cfg_mutex);
  while ((cust_type = get_next_token(s, &len))) {
    if (!cust_type)
      return;

    i=0;
    pcfg = (ssp_customer_type_t *)nkn_malloc_type(sizeof(ssp_customer_type_t), mod_mgmta_charbuf);
    if( !pcfg ) return;

    pcfg->cust_type = nkn_strdup_type(cust_type, mod_mgmta_charbuf);

    while(pcfg->cust_type[i]){ // Convert to lower case
      pcfg->cust_type[i] = tolower(pcfg->cust_type[i]);
      i++;
    }

    pcfg->next = NULL;

    if( sct_list == NULL ) {
      sct_list=pcfg;
    }
    else {
      ssp_customer_type_t *ptmp;
      ptmp=sct_list;
      while(ptmp->next) { ptmp=ptmp->next; }
      ptmp->next=pcfg;
    }

    s += len;
  }

  return;
}


const char* ssp_lookup_cust_type(const char *cust)
{
  ssp_customer_type_t *pcfg;

  pcfg = sct_list;

  while( pcfg ) {
    if(strncmp(cust, pcfg->cust_type, strlen(cust))==0) {
      return pcfg->cust_type;
    }
    pcfg = pcfg->next;
  }

  return NULL; // Didn't find any prefix match
}

#endif // 0


void nkn_http_portlist_callback(char * http_port_list_str)
{
	char * port_str;
	int len, port, i;
	static int called=0;

	if(called == 1) return; // only allow to be called once.

	for(i=0;i<MAX_PORTLIST; i++) {
		nkn_http_serverport[i] = 0;
	}

	for(i=0;i<MAX_PORTLIST; i++) {
		port_str = get_next_token(http_port_list_str, &len);
		if( !port_str ) return ;
		http_port_list_str += len;

		port = atoi(port_str);
		if(port > 0) {
			nkn_http_serverport[i] = port;
		}
	}
}


void nkn_http_interfacelist_callback(char * http_interface_list_str)
{
	char * interface_str;
	int len, i;
	static int interface_called=0;
    int realloc_len = 16;
    int realloc_factor = MAX_NKN_INTERFACE;
    int realloc_count = 1;

	if(interface_called == 1) return; // only allow to be called once.

	for(i=0; i<MAX_NKN_INTERFACE; i++) {
		interface[i].enabled = 0;
	}

    http_listen_intfname = (char **)nkn_malloc_type(sizeof (char *) * realloc_factor * realloc_count, mod_ns_config_t);

	while(1) {
		interface_str = get_next_token(http_interface_list_str, &len);
		if( !interface_str ) return ;
		http_interface_list_str += len;

        http_listen_intfname[http_listen_intfcnt] = (char *)nkn_malloc_type(sizeof(char) * realloc_len, mod_ns_config_t);
		strncpy(http_listen_intfname[http_listen_intfcnt], interface_str, len);
		http_listen_intfcnt++;
        if (http_listen_intfcnt == (realloc_factor * realloc_count)) {
            realloc_count += 1;
            http_listen_intfname = (char **)nkn_realloc_type(http_listen_intfname, (sizeof (char *) * realloc_factor *
                        realloc_count), mod_ns_config_t); 
        }
	}
}

void nkn_rtsp_interfacelist_callback(char * rtsp_interface_list_str)
{
	char * interface_str;
	int len, i;
	static int interface_called=0;

	if(interface_called == 1) return; // only allow to be called once.

	for(i=0; i<MAX_NKN_INTERFACE; i++) {
		interface[i].enabled = 0;
	}

	while(1) {
		interface_str = get_next_token(rtsp_interface_list_str, &len);
		if( !interface_str ) return ;
		rtsp_interface_list_str += len;

		strncpy(rtsp_listen_intfname[rtsp_listen_intfcnt], interface_str, len);
		rtsp_listen_intfcnt++;
	}
}

void nkn_rtsp_live_portlist_callback(char * rtsp_port_list_str)
{
	char * port_str;
	int len, port, i;
	static int called=0;

	if(called == 1) return; // only allow to be called once.

	for(i=0;i<MAX_PORTLIST; i++) {
		nkn_rtsp_live_serverport[i] = 0;
	}

	for(i=0;i<MAX_PORTLIST; i++) {
		port_str = get_next_token(rtsp_port_list_str, &len);
		if( !port_str ) return ;
		rtsp_port_list_str += len;

		port = atoi(port_str);
		if(port > 0) {
			nkn_rtsp_live_serverport[i] = port;
		}
	}
}

void nkn_rtsp_vod_portlist_callback(char * rtsp_port_list_str)
{
	char * port_str;
	int len, port, i;
	static int called=0;

	if(called == 1) return; // only allow to be called once.

	for(i=0;i<MAX_PORTLIST; i++) {
		nkn_rtsp_vod_serverport[i] = 0;
	}

	for(i=0;i<MAX_PORTLIST; i++) {
		port_str = get_next_token(rtsp_port_list_str, &len);
		if( !port_str ) return ;
		rtsp_port_list_str += len;

		port = atoi(port_str);
		if(port > 0) {
			nkn_rtsp_vod_serverport[i] = port;
		}
	}
}

void nkn_http_allow_req_method_callback(char * method_name)
{
	int i = 0;

	if(!method_name)
		return;
	NKN_MUTEX_LOCK(&http_allowed_methods.cfg_mutex);

	for (i = 0; i < MAX_HTTP_METHODS_ALLOWED; i++) {
		if (http_allowed_methods.method[i].name == NULL &&
				http_allowed_methods.method[i].allowed == 0) {
			http_allowed_methods.method[i].allowed = 1;
			http_allowed_methods.method[i].method_len = strlen(method_name);
			http_allowed_methods.method[i].name = 
				(char *) nkn_strdup_type(method_name, 
						mod_mgmta_charbuf);
			if (http_allowed_methods.method[i].name == NULL) {
				// Need to add a debug message here
			}
			http_allowed_methods.method_cnt++;
			break;
		}
	}

	NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
	return;
}

void nkn_http_delete_req_method_callback(char * method_name)
{
	
	int i = 0;

	if(!method_name)
		return;
	NKN_MUTEX_LOCK(&http_allowed_methods.cfg_mutex);

		for ( i = 0; i < MAX_HTTP_METHODS_ALLOWED; i++ ) {
			if ( http_allowed_methods.method[i].name && ( (strcmp ( method_name ,
					http_allowed_methods.method[i].name )) == 0)) {
				free(http_allowed_methods.method[i].name);
				http_allowed_methods.method[i].name = NULL;
				http_allowed_methods.method[i].method_len = 0;
				http_allowed_methods.method[i].allowed = 0;
				http_allowed_methods.method_cnt--;
				break;
			}

		}

	NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);

}

void nkn_http_set_all_req_method_callback( int allow_all_method)
{
	NKN_MUTEX_LOCK(&http_allowed_methods.cfg_mutex);

	if (allow_all_method){
		http_allowed_methods.all_methods = 1;
	} else {
		http_allowed_methods.all_methods = 0;
	}
	NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
}

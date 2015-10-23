#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>

#include <string.h>
#include <strings.h>
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_ism_read_api.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_utils.h"

static int32_t ism_parse_custom_attribute(xmlNode *ql, ism_av_props_t *av);

int32_t
ism_read_map_from_file(char* ism_file,
		       ism_bitrate_map_t **out, 
		       io_handlers_t *iocb)
{
    FILE *fp_ism;   
    uint8_t *dataBuf;
    int bufLen, err;
    ism_bitrate_map_t* map;
    xml_read_ctx_t*ism;
    struct stat st;
    void *io_desc = NULL;

    /* init */
    err = 0;
    fp_ism = NULL;
    dataBuf = NULL;
    map = NULL;
    ism = NULL;
    *out = NULL;

    if(!stat(ism_file, &st)) {
	if (S_ISDIR(st.st_mode)) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    goto error;
	}
    } 

    /* open ISM file */
    io_desc = iocb->ioh_open((char*)\
			     ism_file,
			     "rb", 0);
    if (!io_desc) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    
    /* find size of ISM file */
    iocb->ioh_seek(io_desc, 0, SEEK_END);
    bufLen = iocb->ioh_tell(io_desc);
    if (!bufLen) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    iocb->ioh_seek(io_desc, 0, SEEK_SET);

    /* allocate memory for ISM file */
    dataBuf = (uint8_t*)nkn_malloc_type(bufLen, 
					mod_vpe_ism_buf);
    if (!dataBuf) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;	
    }

    /* read the ISM file */
    iocb->ioh_read(dataBuf, bufLen, sizeof(uint8_t), io_desc);
    
    /* read the server manifest ism file into an XML context */
    ism = init_xml_read_ctx( (uint8_t *)dataBuf, bufLen);
    if (!ism) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }

    /* read the [trackid, bitrate] map from the ism file context */
    map = ism_read_bitrate_map(ism);
    if (!map) {
	err = -E_ISM_PARSE_ERR;
	*out = NULL;
	goto error;
    }

    /* store to output */
    *out = map;

 error:
    /* cleanup */
    if (dataBuf)
	free(dataBuf);
    xml_cleanup_ctx(ism);
    if (io_desc)
	iocb->ioh_close(io_desc);

    return err;

}

ism_bitrate_map_t*
ism_read_bitrate_map(xml_read_ctx_t *ctx)
{
    uint32_t i, k;
    ism_bitrate_map_t *map;
    xmlNode *node;
    uint32_t profile_count[MAX_TRAK_TYPES];
    xmlAttr *ismc;
    
    map = (ism_bitrate_map_t*)nkn_calloc_type(1, 
					      sizeof(ism_bitrate_map_t),
					      mod_vpe_ism_map_t);
   
    for(i = 0; i < MAX_TRAK_TYPES; i++) {
	map->attr[i] = (ism_av_attr_t*)nkn_calloc_type(1, 
						       sizeof(ism_av_attr_t),
						       mod_vpe_ism_av_attr_t);
	profile_count[i]= 0;
    }

    node = ctx->root;

    node = xml_read_till_node(node, "meta");
    if (!node) {
	goto err;
    }
    ismc = xml_read_till_attr(node, "content");
    if (!ismc) {
	goto err;
    }
    map->ismc_name = strdup((char*)ismc->children->content);
   
    node = ctx->root;
    node = xml_read_till_node(node, "switch");
    if (!node) {
	goto err;
    }

    node = node->children;
    while(node) {
	if(!strcmp((char*)node->name, "video")) {
	    xmlAttr *a;
	    xmlNode *n1;
	    xmlNode *n2;
	    a = xml_read_till_attr(node, "src");
	    if (a == NULL) {
	      goto err;
	    }
	    map->attr[0]->prop[profile_count[0]].video_name =
		strdup((char*)a->children->content);
	    n1 = xml_read_till_node(node, "param");
	    if (n1 == NULL) {
	      goto err;
	    }
	    a = xml_read_till_attr(node, "systemBitrate");
	    if (a == NULL) {
	      goto err;
	    }
	    map->attr[0]->prop[profile_count[0]].bitrate =
		atoi((char*)a->children->content);
	    a = xml_read_till_attr(n1, "value");
	    if (a == NULL) {
	      goto err;
	    }
	    map->attr[0]->prop[profile_count[0]].track_id =
		atoi((char*)a->children->content);
	    TAILQ_INIT(&map->attr[0]->prop[profile_count[0]].cust_attr_head);
	    n2 = xml_read_till_node(n1->next, "param");
	    if (n2 != NULL) {
		ism_parse_custom_attribute(n1,
					   &map->attr[0]->prop[profile_count[0]]);
	    }

	    map->attr[0]->n_profiles++;
	    profile_count[0]++;
	} else {
	    if(!strcmp((char*)node->name, "audio")) {
		xmlAttr *a;
		xmlNode *n1;
		xmlNode *n2;
		a = xml_read_till_attr(node, "src");
		if (a == NULL) {
		  goto err;
		}
		map->attr[1]->prop[profile_count[1]].video_name =
		    strdup((char*)a->children->content);
		n1 = xml_read_till_node(node, "param");
		if (n1 == NULL) {
		  goto err;
		}
		a = xml_read_till_attr(node, "systemBitrate");
		if (a == NULL) {
		  goto err;
		}
		map->attr[1]->prop[profile_count[1]].bitrate =
		    atoi((char*)a->children->content);
		a = xml_read_till_attr(n1, "value");
		if (a == NULL) {
		  goto err;
		}
		map->attr[1]->prop[profile_count[1]].track_id =
		    atoi((char*)a->children->content);
		TAILQ_INIT(&map->attr[1]->prop[profile_count[1]].cust_attr_head);
		n2 = xml_read_till_node(n1->next, "param");
		if (n2 != NULL) {
		    ism_parse_custom_attribute(n1,
					       &map->attr[1]->prop[profile_count[1]]);
		}
		map->attr[1]->n_profiles++;
		profile_count[1]++;
	    }
	}
	node = node->next;
    }

    return map;

 err:
    ism_cleanup_map(map);
    return NULL;
}

uint32_t ca_alloc = 0;
uint32_t ca_free =0;
static int32_t
ism_parse_custom_attribute(xmlNode *n1, ism_av_props_t *av)
{
    xmlNode *n2;
    xmlAttr *a1;
    ism_cust_attr_t *ca;
    uint32_t rv =0;
    uint32_t n_cust_attrs = 0;
    n2 = xml_read_till_node(n1->next, "param");
    while(n2) {
	a1 = xml_read_till_attr(n2, "name");
        if (!a1) {
            return -E_ISM_PARSE_ERR;
        }
        ca = (ism_cust_attr_t*)\
            nkn_malloc_type(sizeof(ism_cust_attr_t),
                            mod_vpe_ism_cust_attr_t);
	ca_alloc++;
        if (!ca) {
            return -E_VPE_PARSER_NO_MEM;
        }

        ca->name = strdup((char*)a1->children->content);

        a1 = xml_read_till_attr(n2, "Value");
        if(a1) {
            ca->val = strdup((char*)a1->children->content);
        }
	//printf("Insert %s %s at %lx\n", ca->name, ca->val, (uint64_t)ca);
        TAILQ_INSERT_TAIL(&(av->cust_attr_head),	\
                          ca, entries);
	n_cust_attrs++;
        n2 = n2->next;
    }
    av->n_cust_attrs = n_cust_attrs;
    return rv;
}


uint32_t 
ism_get_track_id(ism_bitrate_map_t *map, uint32_t bitrate, uint32_t trak_type)
{
    ism_av_attr_t *attr;
    uint32_t i, rv;

    rv = 0;
    attr = map->attr[trak_type];

    for(i = 0;i < attr->n_profiles;i++) {
	if(attr->prop[i].bitrate == bitrate) {
	    rv = attr->prop[i].track_id;
	    break;
	}
    }

    return rv;
}

const char *
ism_get_video_name(ism_bitrate_map_t *map, uint32_t trak_type,
		   uint32_t trak_id, uint32_t bitrate, char
		   *ismc_attr_name[], char * ismc_attr_val[], uint32_t
		   ca_ismc_attr_count)
{
    ism_av_attr_t *attr;
    uint32_t i, k, l;
    ism_cust_attr_t *ca_ism;
    uint32_t ca_ism_attr_count = 0;
    uint32_t is_complete_match = 1;

    attr = map->attr[trak_type];

    for(i = 0;i < attr->n_profiles;i++) {
	char *ca_ism_name[MAX_ATTR] = {NULL};
	char *ca_ism_val[MAX_ATTR] = {NULL};;
	ca_ism_attr_count = 0;
	//getting all the custom attributes for specific profile in
	//ism
	TAILQ_FOREACH(ca_ism, &attr->prop[i].cust_attr_head,
		      entries) {
	    //printf("ca_ism->name: %s\n", ca_ism->name);
	    ca_ism_name[ca_ism_attr_count] = strdup(ca_ism->name);
	    ca_ism_val[ca_ism_attr_count] = strdup(ca_ism->val);
	    ca_ism_attr_count++;
	}

	if(attr->prop[i].track_id == trak_id) {
	    if(bitrate) {
		/* if the search needs to be done on trak_id and
		 * bitrate 
		 */
		if(attr->prop[i].bitrate == bitrate) {
		    /* bitrate and trak id matches */
		   
		    if(ca_ismc_attr_count == 0)
			return attr->prop[i].video_name;
		    else {
			//ca_ismc_attr_count != 0
			if(ca_ism_attr_count < ca_ismc_attr_count) {
			    continue;
			} else {
			    for(k=0;k <ca_ismc_attr_count; k++) {
				for(l=0; l<ca_ism_attr_count; l++) {
				    if(!(strcmp(ismc_attr_name[k],ca_ism_name[l]))) {
					if(!(strcmp(ismc_attr_val[k],
						    ca_ism_val[l]))) {
					    break;
					} else {
					    is_complete_match = 0;
					    break;
					}
					
				}
				}//l loop
				
			    }//k loop
			    if(is_complete_match == 1)
				return attr->prop[i].video_name;
			    else
				continue;
			}
		    }//ca_ismc_attr_count != 0
		}
	    } else {
		return attr->prop[i].video_name;
	    }//if(bitrate)
    	}//if(attr->prop[i].track_id == trak_id)
    }

    return NULL;
}

void
ism_cleanup_map(ism_bitrate_map_t *map)
{
    uint32_t i, j, k;
    ism_cust_attr_t *ca;
    ism_av_attr_t *attr;

    if(map) {
	for(i = 0; i < MAX_TRAK_TYPES; i++) {
	    if(map->attr[i]) {
		for(j = 0; j < map->attr[i]->n_profiles; j++){
		    if(map->attr[i]->prop[j].video_name) {
			free(map->attr[i]->prop[j].video_name);
		    }	
		    TAILQ_FOREACH(ca,\
				  &map->attr[i]->prop[j].cust_attr_head, \
				  entries)
		    //printf("Free %s %s at %lx\n", ca->name, ca->val, (uint64_t)ca);
		    if(ca) {
			if(ca->val) free(ca->val);
			if(ca->name) free(ca->name);   
			free(ca);
			ca_free++;
		    }
		}
		free(map->attr[i]);
	    }
	}
	if(map->ismc_name)
	    free(map->ismc_name);
	free(map);
    }
}

int32_t
ism_get_video_qparam_name(ism_bitrate_map_t *map,
			  uint32_t bitrate,
			  const char **video_qparam_name)
{

    ism_av_attr_t *attr;
    ism_cust_attr_t *ca;
    uint32_t i;

    attr = map->attr[0];

    for (i = 0;i < attr->n_profiles; i++) {
	if (attr->prop[i].bitrate != bitrate) {
	    continue;
	}
	TAILQ_FOREACH(ca,
		      &attr->prop[i].cust_attr_head, entries)
	if (!strcmp(ca->name, "trackName")) { 
	    *video_qparam_name = ca->val;
	    return 0;
	}
    }

    return ISM_USE_DEF_QPARAM;
}

int32_t
ism_get_audio_qparam_name(ism_bitrate_map_t *map,
			  uint32_t bitrate,
			  const char **audio_qparam_name)
{

    ism_av_attr_t *attr;
    ism_cust_attr_t *ca;
    uint32_t i;

    attr = map->attr[1];

    for (i = 0;i < attr->n_profiles; i++) {
	if (attr->prop[i].bitrate != bitrate) {
	    continue;
	}
	TAILQ_FOREACH(ca,
		      &attr->prop[i].cust_attr_head, entries)
	if (!strcmp(ca->name, "trackName")) { 
	    *audio_qparam_name = ca->val;
	    return 0;
	}
    }

    return ISM_USE_DEF_QPARAM;
}

xml_read_ctx_t*
init_xml_read_ctx(uint8_t *p_xml, size_t size)
{
    xml_read_ctx_t *ctx;
    uint32_t enc_identifier;

    ctx = (xml_read_ctx_t*)nkn_calloc_type(1, sizeof(xml_read_ctx_t),
					   mod_vpe_xml_read_ctx_t);
    if(!ctx) {
	return NULL;
    }

    //    ctx->doc = xmlReadFile(fname, "utf-16", XML_PARSE_NOBLANKS );
    ctx->doc = xmlReadMemory((char*)p_xml, size, "noname.xml",
			     NULL, XML_PARSE_NOBLANKS);
    if(!ctx->doc) {
	//	xmlCleanupParser();
	free(ctx);
	return NULL;
    }
    
    ctx->root = xmlDocGetRootElement(ctx->doc);
    ctx->xpath = xmlXPathNewContext(ctx->doc);

    return ctx;
}

void
xml_cleanup_ctx(xml_read_ctx_t *ctx)
{
    if(ctx) {
	if(ctx->doc) {
	    xmlFreeDoc(ctx->doc);
	}
	
	xmlXPathFreeContext(ctx->xpath);
	//	xmlCleanupParser();
	free(ctx);
    }
}

void
xml_print_elements(xmlNode *node) 
{
    for(; node; node = node->next) {
	if(node->type == XML_ELEMENT_NODE ||
	   node->type == XML_TEXT_NODE) {
	    printf("type %s\n", node->name);
	}
	xml_print_elements(node->children);
    }

}

xmlNode *
xml_read_till_node(xmlNode *node, const char *name)
{
    xmlNode *n, *n1;
    
    n = node;
    for(; n; n = n->next) {
	if(!strcmp((char*)n->name, name)) {
	    return n;
	}
	n1 = xml_read_till_node(n->children, name);
	if(n1) {
	    return n1;
	} 	    
    }
    
    return NULL;
}

xmlAttr*
xml_read_till_attr(xmlNode *node, const char *name)
{
    xmlAttr *n;
    
    n = node->properties;
    for(; n; n = n->next) {
	if(!strcasecmp((char*)n->name, name)) {
	    return n;
	}
    }
    
    return NULL;
}

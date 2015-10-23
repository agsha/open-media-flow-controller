#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <sys/stat.h>

#include "nkn_vpe_ism_read_api.h"
#include "nkn_vpe_ismc_read_api.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_utils.h"

#define ISMC_VIDEO_ID 0x65646976
#define ISMC_AUDIO_ID 0x69647561

static int32_t ismc_parse_cust_attr(xmlNode *ql, ismc_av_attr_t *av);

int32_t
ismc_read_profile_map_from_file(char *ismc_name,
				ismc_publish_stream_info_t **out,
				io_handlers_t *iocb)
{
    FILE *fismc;
    size_t ismc_size;
    uint8_t *ismc_data;
    ismc_publish_stream_info_t *psi;
    xml_read_ctx_t *ismc;
    int32_t err;
    struct stat st;
    void *io_desc = NULL;

    err = 0;
    ismc_data = NULL;
    fismc = NULL;
    ismc = NULL;
    *out = NULL;

    if(!stat(ismc_name, &st)) {
	if (S_ISDIR(st.st_mode)) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    goto error;
	}
    } 

    io_desc = iocb->ioh_open((char*)\
			     ismc_name,
			     "rb", 0);
    if (!io_desc) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    iocb->ioh_seek(io_desc, 0, SEEK_END);

    ismc_size = iocb->ioh_tell(io_desc);
    if (!ismc_size) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    iocb->ioh_seek(io_desc, 0, SEEK_SET);
    //rewind(fismc);
    ismc_data = (uint8_t*)nkn_malloc_type(ismc_size,
					  mod_vpe_media_buffer); 

    iocb->ioh_read(ismc_data, 1, ismc_size, io_desc);

    ismc = init_xml_read_ctx((uint8_t*)ismc_data, ismc_size);
    if (!ismc) {
	err = -E_ISMC_PARSE_ERR;
	goto error;
    }
    *out = ismc_read_profile_map(ismc);
    
    if (!(*out)) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }

    if (io_desc) iocb->ioh_close(io_desc);
    if (ismc) xml_cleanup_ctx(ismc);
    if (ismc_data) free(ismc_data);
    return err;

 error:
    if (io_desc) iocb->ioh_close(io_desc);
    if ((*out)) free(*out);
    if (ismc) xml_cleanup_ctx(ismc);
    if (ismc_data) free(ismc_data);
    return err;
}

ismc_publish_stream_info_t*
ismc_read_profile_map(xml_read_ctx_t *ctx)
{
    
    ismc_publish_stream_info_t *psi;
    xmlNode *stream_index, *quality_level;
    xmlAttr *a1, *url_attr;
    int32_t *type, i, j, k;
    char *url_fmt, *p;
    uint8_t parse_custom_attr;

    parse_custom_attr = 0;
    url_fmt = NULL;
    psi = (ismc_publish_stream_info_t*)nkn_calloc_type(1,
						       sizeof(ismc_publish_stream_info_t),
						       mod_vpe_ismc_psi_t);
    for(i = 0; i < MAX_TRAK_TYPES; i++) {
        psi->attr[i] = (ismc_av_attr_t*)nkn_calloc_type(1, 
							sizeof(ismc_av_attr_t),
							mod_vpe_ismc_av_attr_t);
	for (j = 0; j < MAX_TRACKS; j++) {
	    TAILQ_INIT(&psi->attr[i]->cust_attr_head[j]);
	}
    }
	   
    stream_index = xml_read_till_node(ctx->root, "StreamIndex");
    while(stream_index) {
	url_attr = xml_read_till_attr(stream_index, "Url");
	if(url_attr) {
	    url_fmt = (char*)url_attr->children->content;
	    p = strstr(url_fmt, "{CustomAttributes}");
	    if (p) {
		parse_custom_attr = 1;
	    }
	}
	a1 = xml_read_till_attr(stream_index, "Type");
	quality_level = xml_read_till_node(stream_index,
					   "QualityLevel");
	if (!a1 || !quality_level) {
	    stream_index = stream_index->next;
	    continue;
	}
	type = (int32_t*)(a1->children->content);
	switch(*type) {
	    case ISMC_VIDEO_ID:
		while(quality_level) {
		    if(!strcmp((char*)quality_level->name, "QualityLevel")) {
			a1 = xml_read_till_attr(quality_level, "Bitrate");
			psi->attr[0]->bitrates[psi->attr[0]->n_profiles] = 
			    atoi((char*)a1->children->content);
			if (parse_custom_attr) {
			    ismc_parse_cust_attr(quality_level,
						 psi->attr[0]);
			}
			psi->attr[0]->n_profiles++;
		    } 
		    quality_level = quality_level->next;
		}
		break;
	    case ISMC_AUDIO_ID:
		while(quality_level) {
		    if(!strcmp((char*)quality_level->name, "QualityLevel")) {
			a1 = xml_read_till_attr(quality_level, "Bitrate");
			psi->attr[1]->bitrates[psi->attr[1]->n_profiles] = 
			    atoi((char*)a1->children->content);
			if (parse_custom_attr) {
			    ismc_parse_cust_attr(quality_level,
						 psi->attr[0]);
			}
			psi->attr[1]->n_profiles++;
		    }
		    quality_level = quality_level->next;
		}
		break;
	}
	stream_index = stream_index->next;
    }

    return psi;
}

static int32_t
ismc_parse_cust_attr(xmlNode *ql, ismc_av_attr_t *av)
{
    xmlNode *n1;
    xmlAttr *a1;
    ismc_cust_attr_t *ca;
    uint32_t n_cust_attrs = 0;

    n1 = xml_read_till_node(ql, "CustomAttributes");
    if (!n1) {
	return -E_ISMC_PARSE_ERR;
    }

    n1 = xml_read_till_node(n1, "Attribute");
    	
    while(n1) {
	a1 = xml_read_till_attr(n1, "Name");
	if (!a1) {
	    return -E_ISMC_PARSE_ERR;
	}
	ca = (ismc_cust_attr_t*)\
	    nkn_malloc_type(sizeof(ismc_cust_attr_t),
			    mod_vpe_ismc_cust_attr_t);
	if (!ca) {
	    return -E_VPE_PARSER_NO_MEM;
	}
    
	ca->name = strdup((char*)a1->children->content);
       
	a1 = xml_read_till_attr(n1, "Value");
	if(a1) {
	    ca->val = strdup((char*)a1->children->content);
	}
	TAILQ_INSERT_TAIL(&av->cust_attr_head[av->n_profiles], \
			  ca, entries);
	n_cust_attrs++;
	n1 = n1->next;
    }
    av->n_cust_attrs[av->n_profiles] = n_cust_attrs;
    return 0;
}

void
ismc_cleanup_profile_map(ismc_publish_stream_info_t *psi)
{
    uint32_t i, j, k;
    ismc_cust_attr_t *ca;

    if(psi) {
	for(i = 0; i < MAX_TRAK_TYPES;i++) {
	    if(psi->attr[i]) {
		for (j = 0; j < MAX_TRACKS; j++) {
		    TAILQ_FOREACH(ca,\
				  &psi->attr[i]->cust_attr_head[j],\
				  entries);
		    if(ca) free(ca);
		}
		free(psi->attr[i]);
	    }
	}
	free(psi);
    }
					   
}

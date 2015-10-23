#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//mfp includes
#include "mfp_publ_context.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_pmf_tag_defs.h"

//video parser includes (for XML parsing)
#include "nkn_vpe_ism_read_api.h"

#define GET_ATTR_DATA(_attr) ((_attr)->children->content)

/*************************************************************
 *           HELPER - (STATIC) FUNCTION PROTOTYPES
 ************************************************************/
static int32_t parseEncapsulationTag(xml_read_ctx_t *xml,
				     int32_t src_type,
				     mfp_publ_t *pub);
static int32_t parseMediaTag(xmlNode *child,
			     int32_t src_type,
			     stream_param_t *st);
static int32_t parsePubSchemesTag(xml_read_ctx_t *xml,
				  mfp_publ_t *pub);
static int32_t parseRootTag(xml_read_ctx_t *xml,
			    const mfp_publ_t * pub);
static int32_t populateMFPFromNode(xmlNode *node,
				   int32_t i,
				   mfp_publ_t *mp);
static int32_t getNumberEncapsulations(xml_read_ctx_t *xml);
static int32_t getSrcTypeFromPMF(xml_read_ctx_t *xml);
static int32_t convertStrToPubScheme(const char *str);
static int32_t convertStrToOptype(const char *const str);
static int32_t parsePubPathTag(xml_read_ctx_t *xml,
			       mfp_publ_t *pub);
int32_t compareBitRate(const void *p, const void *q);
int32_t compareMediaType(const void *a, const void *b);
static int32_t validateXmlWithSchema(int8_t *xsd_data, uint32_t xsd_len,
				     xmlDocPtr const xml_doc);
static int32_t parseControlPMF(xml_read_ctx_t *xml,
			       mfp_publ_t *pub);
static int32_t initSIO(mfp_publ_t *pub);

extern pmf_tag_props_t srcTagProp, encapsTagProp, SSPMediaTagProp,
    MP4MediaTagProp, MP2TS_SPTS_MediaTagProp, msPubSchemeTagProp,
    rootTagProp, controlTagProp, globalOPCfgProp;

extern xml_tag_parser_t *pmfMP2TS_SPTS_MediaTagParser, *pmfMP4_MediaTagParser,
    *pmfSSPMediaTagParser, *pmfEncapsTagParser, *pmfSrcTagParser,
    *pmfMSPubSchemeTagParser, *pmfRootTagParser, *pmfControlTagParser,
    *pmfGlobalOPCfgParser;

/*********************************************************************
 *           PMF PARSER API implementation
 * 1. readPublishContextFromFile - returns a fully populated publisher
 *    context from a PMF file
 ********************************************************************/

/**
 * parses a PMF file and populates a publisher context
 * @param pmf_file - path to the pmf file
 * @param mp [out] - fully populated publisher context returned to the
 * caller
 * @return returns 0 on success, negative number on error
 */
int32_t readPublishContextFromFile(char *pmf_file, mfp_publ_t **mp)
{
    FILE *fp;
    size_t pmf_size;
    uint8_t *p_pmf_data;
    xml_read_ctx_t *xml_ctx;
    xmlNode *node;
    int32_t err, n_encapsulation;
    const char *xml_read_nodes[] = {
    };
    enum source_type stype;
    mfp_publ_t *pub;
    int32_t rv;

    rv = 0;
    err = 0;

    /* open the pmf file */
    fp = fopen(pmf_file, "rb");
    if (!fp) {
	perror("File open failed: ");
	return -E_MFP_PMF_INVALID_FILE;
    }

    /* find the size of the pmf file */
    fseek(fp, 0, SEEK_END);
    pmf_size = ftell(fp);
    rewind(fp);

    /* allocate memory for mfp data */
    p_pmf_data =  (uint8_t *)mfp_live_calloc(1, pmf_size);

    if (!p_pmf_data) {
	fclose(fp);
	return -E_MFP_PMF_NO_MEM;
    }

    /* read the pmf file */
    fread(p_pmf_data, 1, pmf_size, fp);

    /* create an xml parser context for the mfp data */
    xml_ctx = init_xml_read_ctx(p_pmf_data, pmf_size);
    if (!xml_ctx) {
	fclose(fp);
	free(p_pmf_data);
	return -E_MFP_PMF_NO_MEM;
    }

    if (xsd_len > 0) {
	/* Checking if the xml data conforms to the standard(xsd) */
	int32_t rc =
	    validateXmlWithSchema(xml_schema_data, xsd_len, xml_ctx->doc);
	if (rc != 0) {
	    err = -E_MFP_PMF_XSD_MISMATCH;
	    goto done;
	}
    }

    /* To proceed with initializing the pub context and allocating
     * resources for the same, we need to parse two parameteres
     * from the PMF file, namely (a) Source type (b) Number of
     * encapsulations
     */

    /* read the source type */
    stype = getSrcTypeFromPMF(xml_ctx);
    if (stype != LIVE_SRC && stype != FILE_SRC &&
	stype != CONTROL_SRC) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    /* create a new publisher context */
    *mp = pub = newPublishContext(1, stype);
    if (!pub) {
	err = -E_MFP_PMF_NO_MEM;
	goto done;
    }

    /* parse the root tag */
    rv = parseRootTag(xml_ctx, pub);
    if (rv) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    if (stype == LIVE_SRC) {
	if(newPublishContextStats(pub) != 1){
	    err = -E_MFP_PMF_INVALID_TAG;
	    goto done;
	}
    }

    /* if control PMF parse and exit */
    if (stype == CONTROL_SRC) {
	parseControlPMF(xml_ctx, pub);
	goto done;
    }

    /* populate the encapsulation segment in the publisher context
     */
    rv = parseEncapsulationTag(xml_ctx, stype, pub);
    if (rv) {
	err = rv;
	goto done;
    }

    rv = parsePubSchemesTag(xml_ctx, pub);
    if (rv) {
	err = rv;
	goto done;
    }

 done:
    fclose(fp);
    free(p_pmf_data);
    xml_cleanup_ctx(xml_ctx);

    return err;
}

/**
 * parse the encapsulation tags in the PMF and populatet the publisher
 * context
 * @param xml - pointer to the head of xml node to search from
 * @param src_type - the type of source (LIVE or FILE) since some of
 * the attributes are source type specific
 * @param pub[in/out] - pointer to the publisher context that needs to
 * be populated
 * @return - returns 0 on sucess and negative number on error
 */
static int32_t parseEncapsulationTag(xml_read_ctx_t *xml,
				     int32_t src_type,
				     mfp_publ_t *pub)
{
    xmlNode *parent, *child;
    xmlAttr *attr;
    int32_t i, rv, j=0, acount, vcount, node_found;
    stream_param_t *e, *m;
    xmlXPathObject *xpath_obj;

    xpath_obj = NULL;
    i = 0;
    rv = 0;
    acount = vcount = node_found = 0;

    for (i = 0;i < encapsTagProp.n_entries; i++) {
	xpath_obj =				\
	    xmlXPathEvalExpression(
				   (xmlChar *)\
				   encapsTagProp.tag_parser[i].tag_names,
				   xml->xpath);
	if (xpath_obj->nodesetval->nodeNr ) {
	    node_found++;
	    /* populate the encaps tag and the media tag */
	    rv = encapsTagProp.tag_parser[i].parse_tag(xml,
						       &i,
						       pub);
	    if (rv) {
		xmlXPathFreeObject(xpath_obj);
		xpath_obj = NULL;
		goto err;
	    }
	}
	xmlXPathFreeObject(xpath_obj);
	xpath_obj = NULL;
    }

    if (!node_found) {
	rv = -E_MFP_PMF_INVALID_TAG;
	goto err;
    }

    /* find the number of audio and video streams */
    m = pub->encaps[0];
    for (i = 0; i < (int32_t)pub->streams_per_encaps[0]; i++) {
	switch (m->med_type) {
	    case AUDIO:
		acount++;
		break;
	    case VIDEO:
		vcount++;
		break;
	    case MUX:
	    case SVR_MANIFEST:
	    case CLIENT_MANIFEST:
	    case ZERI_MANIFEST:
	    case ZERI_INDEX:
		break;
	}
	m++;
    }

    e = pub->encaps[0];
    if (vcount && acount) {
	/* come here only if the are audio and video
	 * streams and not if they are mux or manifest
	 * type
	 */

	/* partition a/v streams */
	qsort(e, pub->streams_per_encaps[0], sizeof(stream_param_t),
	      compareMediaType);

	/* sort video and audio streams by bitrate */
	qsort(e, vcount, sizeof(stream_param_t),
	      compareBitRate);
	qsort(&e[vcount], j - vcount,
	      sizeof(stream_param_t),
	      compareBitRate);
    }

 err:
    return rv;
}

/**
 * parse the Pub Schemes Tag and populate the respective target
 * publishing schemes
 * @param xml - the xml context
 * @param pub - the publisher context which needs to be populated with
 * the correct Publishing Schemes
 * @return - returns 0 on success, negative integer on error
 */
static int32_t parsePubSchemesTag(xml_read_ctx_t *xml,
				  mfp_publ_t *pub)
{
    int32_t err, rv, i;
    xmlXPathObject *xpath_obj;

    err = 0;
    xpath_obj = NULL;

    xpath_obj =\
	xmlXPathEvalExpression((xmlChar *)globalOPCfgProp.tag_parser[0].tag_names,
			       xml->xpath);
    if (xpath_obj->nodesetval->nodeNr) {
	pub->op_cfg.disk_usage = atoi((char *)\
				      xpath_obj->nodesetval->nodeTab[0]->children->content);
	if (pub->op_cfg.disk_usage > 0) {
	    err = mfp_safe_io_init(NULL, 0.0, pub->op_cfg.disk_usage, &pub->op_cfg.sioc);
	    if (err) {
		xmlXPathFreeObject(xpath_obj);
		goto done;
	    }
	} 	    
    }
    xmlXPathFreeObject(xpath_obj);
			   
    /* evaluation XPath for all PubSchemes tags */
    xpath_obj = \
	xmlXPathEvalExpression(
			       (xmlChar*)("//PubSchemes/*[@status=\"on\"]"),
			       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr ) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    /* loop through the PubSchemes tags and process each type */
    for (i = 0; i < xpath_obj->nodesetval->nodeNr; i++) {
	switch (convertStrToPubScheme((char*)\
				      xpath_obj->nodesetval->nodeTab[i]->name)) {
	    case SMOOTH_STREAMING:
		err = populateSmoothStreamingProps(xml,
						   (mfp_publ_t*)pub);
		if (err) {
		    goto done;
		}
		pub->stats.num_sl_fmtr += 1;

		break;
	    case FLASH_STREAMING:
		err = populateFlashStreamingProps(xml,
						  (mfp_publ_t*)pub);
		if (err) {
		    goto done;
		}
		pub->stats.num_zeri_fmtr += 1;

		break;
	    case MOBILE_STREAMING:
		//MOBILE STREAMING
		err = populateMobileStreamingProps(xml,
						   (mfp_publ_t*)pub);
		if (err) {
		    goto done;
		}
		pub->stats.num_apple_fmtr += 1;
		break;
		
	}
    }

 done:
    /* cleanup */
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return err;
}


static int32_t
initSIO(mfp_publ_t *pub)
{
    int32_t i = 0, err = 0;
    mfp_safe_io_ctx_t *sioc = NULL;

    /* if global config is available then create a safe io context
     * based on the global disk usage paramter and copy it to all the
     * pub schemes
     */
    if (pub->op_cfg.disk_usage) {
	err = mfp_safe_io_init(NULL, 0.0, pub->op_cfg.disk_usage,
			       &sioc);
	if (err) {
	    goto error;
	}
	pub->op_cfg.sioc = sioc;
	pub->ms_parm.sioc = sioc;
	pub->ss_parm.sioc = sioc;
	
    } else {
	err = mfp_safe_io_init(NULL, 0.0, pub->ms_parm.disk_usage,
			       &pub->ms_parm.sioc);
	if (err) {
	    goto error;
	}
	err = mfp_safe_io_init(NULL, 0.0, pub->ss_parm.disk_usage,
			       &pub->ss_parm.sioc);
	if (err) {
	    goto error;
	}
    }

    return err;

 error:
    return err;
}

/**
 * parse the ROOT tag and its attributes
 * @param xml - the context to read xml files
 * @param pub [out] -  the publish context whose appropriate fields
 * need to be populated
 * @return returns 0 on success, negative number on error
 */
static int32_t
parseRootTag(xml_read_ctx_t *xml, const mfp_publ_t * pub)
{
    int32_t err, rv, i;
    xmlXPathObject *xpath_obj;

    err = 0;
    xpath_obj = NULL;

    /* evaluation XPath for all PubSchemes tags */
    xpath_obj = \
	xmlXPathEvalExpression(
			       (xmlChar*)(ROOT_TAG),
			       xml->xpath);
    if (!xpath_obj->nodesetval->nodeNr ) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    err = rootTagProp.tag_parser[0].parse_tag(xml, \
					      rootTagProp.tag_parser[0].child, \
					      pub);
 done:
    /* cleanup */
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return err;
}

static int32_t
parseControlPMF(xml_read_ctx_t *xml, mfp_publ_t *pub)
{
    int32_t err, rv, i, j, len;
    xmlXPathObject *xpath_obj;
    char *p;

    err = 0;
    xpath_obj = NULL;

    for (i = 0; i < controlTagProp.n_entries; i++) {

	/* evaluation XPath for all PubSchemes tags */
	xpath_obj =	\
	    xmlXPathEvalExpression(\
				   (xmlChar*)(controlTagProp.tag_parser[i].tag_names),
				   xml->xpath);
	if (!xpath_obj) {
	    continue;
	}
	if (!xpath_obj->nodesetval) {
	    xmlXPathFreeObject(xpath_obj);
	    continue;
	}
	switch (i) {
	    case 0:
		if (!xpath_obj->nodesetval->nodeNr ) {
		    xmlXPathFreeObject(xpath_obj);
		    err = -E_MFP_PMF_INVALID_TAG;
		    continue;
		}
		p = (char *)xpath_obj->nodesetval->nodeTab[0]->children->content;
		err = convertStrToOptype(p);
		if (err < 0) {
		    xmlXPathFreeObject(xpath_obj);
		    return err;
		}
		pub->op = err;
		break;
	    case 1:
		if (pub->op !=  PUB_SESS_STATE_STATUS_LIST ||
		    (!xpath_obj->nodesetval->nodeNr)) {
		    break;
		}
		err = allocSessNameList(pub, xpath_obj->nodesetval->nodeNr);
		if (err) {
		    err = -E_MFP_NO_MEM;
		    return err;
		}
		for (j = 0; j < (int32_t)pub->n_sess_names; j++) {
			if(xpath_obj->nodesetval->nodeTab[j] != NULL 
				&& xpath_obj->nodesetval->nodeTab[j]->children != NULL){
		    if ( (len = strlen((char*)(xpath_obj->nodesetval->nodeTab[0]->children->content))) >
			 MAX_PUB_NAME_LEN) {
			err = -E_MFP_PMF_INVALID_TAG;
			break;
		    }
		    strncpy(pub->sess_name_list[j],
			    (char*)xpath_obj->nodesetval->nodeTab[j]->children->content,
			    strlen ((char*)(xpath_obj->nodesetval->nodeTab[j]->children->content)));
		}else{
			err = -E_MFP_PMF_INVALID_TAG;
			break;
		}
		}
		break;
	    case 2:
		if (xpath_obj) {
		    controlTagProp.tag_parser[i].parse_tag(xml, NULL,
							   pub); 
		}
		break;
	}
	if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    }

    return err;
}

/**
 * convert a string to PUB_SESS_OP_STATE
 * @param str - string to be converted
 * @return returns the correct OP TYPE on success and negative number
 * on error
 */
static int32_t convertStrToOptype(const char *const str)
{

    if (!strcasecmp(str, "STOP")) {
	return PUB_SESS_STATE_STOP;
    } else if (!strcasecmp(str, "PAUSE")) {
	return PUB_SESS_STATE_PAUSE;
    } else if (!strcasecmp(str, "STATUS")) {
	return PUB_SESS_STATE_STATUS;
    } else if (!strcasecmp(str, "STATUS_LIST")) {
	return PUB_SESS_STATE_STATUS_LIST;
    } else if (!strcasecmp(str, "CONFIG")) {
	return PUB_SESS_STATE_CONFIG;
    }  else if (!strcasecmp(str, "REMOVE")){
	return PUB_SESS_STATE_REMOVE;
    }else {
	return -E_MFP_PMF_INVALID_TAG;
    }

}

/**
 * convert a string to PubScheme type
 * @param str - string to convert
 * @return returns the correct publish scheme enumeration on success
 * and a negative number on error
 */
static int32_t convertStrToPubScheme(const char *str)
{

    if (!strcasecmp(str, "SmoothStreaming")) {
	return SMOOTH_STREAMING;
    } else if (!strcasecmp(str, "FlashStreaming")) {
	return FLASH_STREAMING;
    } else if (!strcasecmp(str,"MobileStreaming")) {
	return MOBILE_STREAMING;
    } else  {
	return -E_MFP_PMF_INVALID_TAG;
    }

}
/**
 * validates the sock addr structure
 * @param sin - input sockaddr_in to be validated
 * @return returns 0 on success, negative integer on error
 */
static int32_t validateSockAddr(struct sockaddr_in *saddr)
{
    if(saddr->sin_family != AF_INET) {
	return -E_MFP_PMF_INCOMPLETE_TAG;
    }

    if(!saddr->sin_port ||
       !saddr->sin_addr.s_addr) {
	return -E_MFP_PMF_INCOMPLETE_TAG;
    }

    return 0;
}

/**
 * validates the stream param structure for FILE based source types
 * @param sp - input stream_param_t structure to be validated
 * @return returns 0 on success, negative integer on error
 */
static int32_t validateFileStreamParam(stream_param_t *sp)
{
    return 0;
}

/**
 * validates the stream param structure for LIVE sources
 * @param sp - input stream_param_t structure to be validated
 * @return returns 0 on success, negative integer on error
 */
static int32_t validateLiveStreamParam(stream_param_t *sp)
{
    if (!sp->bit_rate) {
	return -E_MFP_PMF_INCOMPLETE_TAG;
    }

    /* This may not be a _necessary_ attribute since we can guess
       the transport type based on the first byte received,
       0x80 for RTP, 0x47 for MPEG2-TS over UDP/IP etc.
    */
    /*	if (sp->tport_type != UDP_IP) {
      return -E_MFP_PMF_INCOMPLETE_TAG;
      }*/

    if (validateSockAddr(&sp->media_src.live_src.to)) {
	return -E_MFP_PMF_INCOMPLETE_TAG;
    }

    return 0;
}

/**
 * validates the stream param structure
 * @param sp - input stream_param_t structure to be validated
 * @param src_type - source type LIVE or SOURCE
 * @return returns 0 on success, negative integer on error
 */
static int32_t validateStreamParam(stream_param_t *sp,
				   int32_t src_type)
{
    int32_t rv;

    rv = 0;

    switch (src_type) {
	case LIVE_SRC:
	    rv = validateLiveStreamParam(sp);
	    break;
	case FILE_SRC:
	    rv = validateFileStreamParam(sp);
	    break;
    }

    return rv;

}

/**
 * converts a string to Source Type enumeration
 * @param xml - pointer to the head of xml node to search from
 * @return - returns 0 on sucess and negative number on error
 */
static int32_t getSrcTypeFromPMF(xml_read_ctx_t *xml)
{
    xmlNode *node;
    char *str;
    int32_t err, node_found, i;
    xmlAttr *attr;
    xmlXPathObject *xpath_obj;

    err = 0;
    xpath_obj = NULL;
    node_found = 0;

    /* evaluation XPath for all Src Typetags */
    for (i = 0; i < srcTagProp.n_entries;i++) {
	xpath_obj =				\
	    xmlXPathEvalExpression(
				   (xmlChar *)\
				   srcTagProp.tag_parser[i].tag_names,
				   xml->xpath);
	if (xpath_obj->nodesetval->nodeNr ) {
	    node_found = 1;
	    xmlXPathFreeObject(xpath_obj);
	    break;
	}
	xmlXPathFreeObject(xpath_obj);
    }

    if (!node_found) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    switch (i) {
	case 0:
	    err = LIVE_SRC;
	    break;
	case 1:
	    err = FILE_SRC;
	    break;
	case 2:
	    err = CONTROL_SRC;
	    break;
    }
 done:
    return err;

 error:
    /* cleanup */
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    return err;
}

/**
 * finds the number of encapsulation tags present; this number is
 * required upfront for creating the publisher context
 * @param xml - pointer to the head of xml node to search from
 * @return - returns 0 on sucess and negative number on error
 */
static int32_t getNumberEncapsulations(xml_read_ctx_t *xml)
{
    xmlNode *node;
    int32_t i;

    node = xml_read_till_node(xml->root, "Encapsulation");

    if (!node) {
	return 0;
    }

    i = 1;
    node = node->next;

    while (node) {
	node = xml_read_till_node(node, "Encapsulation");
	if (!node) {
	    break;
	}
	node = node->next;
	i++;
    }

    return i;
}

static int32_t validateXmlWithSchema(int8_t *xsd_data, uint32_t len,
				     xmlDocPtr const xml_doc) {

    xmlSchemaParserCtxtPtr parser_ctx =
	xmlSchemaNewMemParserCtxt((char const*)xsd_data, len);
    if (parser_ctx == NULL) {

	DBG_MFPLOG("XSD-VAL", ERROR, MOD_MFPLIVE,
		   "XSD parser ctxt create failed");
	printf("XML Schema Parser Ctxt init error:\n");
	return -1;
    }

    xmlSchemaValidCtxtPtr validator_ctxt = NULL;
    xmlSchemaPtr xml_schema = NULL;

    xmlSchemaSetParserErrors(parser_ctx,
			     (xmlSchemaValidityErrorFunc) fprintf,
			     (xmlSchemaValidityWarningFunc) fprintf, stderr);
    xml_schema = xmlSchemaParse(parser_ctx);
    if (!xml_schema) {
	xmlSchemaFreeParserCtxt(parser_ctx);
	DBG_MFPLOG("XSD-VAL", ERROR, MOD_MFPLIVE,
		   "XSD parse failed");
	printf("XML schema parse error.\n");
	return -1;
    }

    validator_ctxt = xmlSchemaNewValidCtxt(xml_schema);
    if (!validator_ctxt) {
	DBG_MFPLOG("XSD-VAL", ERROR, MOD_MFPLIVE,
		   "XSD new validator ctxt create failed");
	printf("XML schema invalid.\n");
	xmlSchemaFree(xml_schema);
	xmlSchemaFreeParserCtxt(parser_ctx);
	return -1;
    }
    xmlSchemaSetValidErrors(validator_ctxt,
			    (xmlSchemaValidityErrorFunc) fprintf,
			    (xmlSchemaValidityWarningFunc) fprintf, stderr);

    int32_t rc = xmlSchemaValidateDoc(validator_ctxt, xml_doc);
    if (rc == 0) {
	DBG_MFPLOG("XSD-VAL", MSG, MOD_MFPLIVE, "XML validation success");
	printf("xml file valid\n");
    } else if (rc > 0) {
	DBG_MFPLOG("XSD-VAL", ERROR, MOD_MFPLIVE,
		   "PMF XML Invalid. XSD validation failed");
	printf("xml file invalid(to the schema)\n");
	rc = -E_MFP_PMF_XSD_MISMATCH;
    } else {
	DBG_MFPLOG("XSD-VAL", ERROR, MOD_MFPLIVE,
		   "XSD validation generated an internal error");
	printf("validation generated an internal error\n");
	rc = -E_MFP_PMF_XSD_INT_ERROR;
    }
    xmlSchemaFreeParserCtxt(parser_ctx);
    xmlSchemaFreeValidCtxt(validator_ctxt);
    xmlSchemaFree(xml_schema);
    return rc;
}


void readFileIntoMemory(int8_t const* file_path, int8_t** file_data,
			uint32_t* file_size) {

    *file_data = NULL;
    *file_size = 0;
    FILE* fp = fopen((char const*)file_path, "rb");
    if (fp == NULL) {
	perror("schema file open: ");
	return;
    }

    fseek(fp, 0, SEEK_END);
    *file_size = ftell(fp);
    rewind(fp);

    *file_data =  (int8_t *)mfp_live_calloc(1, *file_size);
    if (*file_data == NULL) {
	fclose(fp);
	return;
    }

    if (fread(*file_data, 1, *file_size, fp) != *file_size)
	fclose(fp);

    return;
}


int32_t compareMediaType(const void *p, const void *q)
{
    stream_param_t *a, *b;

    a = (stream_param_t*)p;
    b = (stream_param_t*)q;
    if (a->med_type == b->med_type) {
	return 0;
    } else if (a->med_type < b->med_type) {
	return -1;
    } else {
	return 1;
    }

}

int32_t compareBitRate(const void *p, const void *q)
{
    stream_param_t *a, *b;

    a = (stream_param_t*)p;
    b = (stream_param_t*)q;
    if (a->bit_rate == b->bit_rate) {
	return 0;
    } else if (a->bit_rate < b->bit_rate) {
	return -1;
    } else {
	return 1;
    }
}

#if 0
/**
 * parse a Media tag a populate the corresponding stream param
 * structure
 * @param child - xml node the denotes the root of the media tag
 * @param src_type - the source type LIVE/FILE; this is required since
 * some of the parsing is specific to the source type
 */
static int32_t parseMediaTag(xmlNode *child,
			     int32_t src_type,
			     stream_param_t *st)
{
    xmlAttr *attr;
    char *str;
    int32_t rv;

    rv = 0;

    child = xml_read_till_node(child, "Media");
    if (!child) {
	goto err;
    }

    attr = xml_read_till_attr(child, "Type");
    if (attr) {
	st->med_type =
	    convertStrToMediaType(
				  (const char*)(GET_ATTR_DATA(attr)));
	printf("Media Type: %s\n",
	       attr->children->content);
    }
    attr = xml_read_till_attr(child, "src");
    if (attr) {
	if (src_type == LIVE_SRC) {
	    convertStrToLiveSrc(
				(const char*)(GET_ATTR_DATA(attr)),
				&st->media_src.live_src.from);
	} else if (src_type == FILE_SRC) {
	    strcpy((char*)st->media_src.file_src.file_url,
		   (char*)(GET_ATTR_DATA(attr)));
	} else {
	    rv = -E_MFP_PMF_UNSUPPORTED_TYPE;
	    goto err;
	}
	printf("Media Src: %s\n",
	       attr->children->content);
    }
    attr = xml_read_till_attr(child, "dest");
    if (attr) {
	if (src_type == LIVE_SRC) {
	    st->media_src.live_src.is_multicast = 0;
	    convertStrToLiveSrc(
				(const char*)(GET_ATTR_DATA(attr)),
				&st->media_src.live_src.to);
	    st->media_src.live_src.is_multicast =
		isMulticast((const char*)(GET_ATTR_DATA(attr)));
	    if (st->media_src.live_src.is_multicast != 1)
		printf("NOT A MULTICAST ADDRESS\n");
	} else if (src_type == FILE_SRC) {
	    strcpy((char*)st->media_src.file_src.file_url,
		   (char*)(GET_ATTR_DATA(attr)));
	} else {
	    rv = -E_MFP_PMF_UNSUPPORTED_TYPE;
	    goto err;
	}
	printf("Media Src: %s\n",
	       attr->children->content);
    }
    attr = xml_read_till_attr(child, "transport");
    if (attr) {
	st->tport_type = convertStrToTransportType(\
						   (const char *)(GET_ATTR_DATA(attr)));

	printf("Media Transport: %s\n",
	       attr->children->content);
    }
    attr = xml_read_till_attr(child, "Bit-rate");
    if (attr) {
	st->bit_rate =
	    atoi((char *)(GET_ATTR_DATA(attr)));
	printf("Media Bitrate: %d\n", st->bit_rate);
    }

 err:
    return rv;
}

/**
 * parse a Media tag a populate the corresponding stream param
 * structure
 * @param child - xml node the denotes the root of the media tag
 * @param src_type - the source type LIVE/FILE; this is required since
 * some of the parsing is specific to the source type
 */
//static int32_t parseMediaTag(xmlXPathContext *xpath,
//int32_t src_type,
//		     stream_param_t *st)
//{
//
//   return 0;
//}
static int32_t parsePubPathTag(xml_read_ctx_t *xml,
			       mfp_publ_t *pub)
{
    int32_t err;
    xmlXPathContext *xpath_ctx;
    xmlXPathObject *xpath_obj;
    char *tmp_path;

    err = 0;

    /* open a new XML context for XPath parsing */
    xpath_ctx = xmlXPathNewContext(xml->doc);
    if (!xpath_ctx) {
	err = -E_MFP_PMF_INVALID_FILE;
	goto done;
    }

    /* evaluation XPath for all PubSchemes tags */
    xpath_obj =\
	xmlXPathEvalExpression(
			       (xmlChar*)("//StorageURL"),
			       xpath_ctx);
    if (!xpath_obj->nodesetval->nodeNr ) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    xmlNode *node;
    node = xpath_obj->nodesetval->nodeTab[0];
    tmp_path = \
	(char*)(strdup((char*)node->children->content));
    pub->ss_store_url = pub->ms_store_url = pub->fs_store_url =\
	(int8_t*)tmp_path;

    xpath_obj =\
	xmlXPathEvalExpression(
			       (xmlChar*)("//DeliveryURL"),
			       xpath_ctx);
    if (!xpath_obj->nodesetval->nodeNr ) {
	err = -E_MFP_PMF_INVALID_TAG;
	goto done;
    }

    node = xpath_obj->nodesetval->nodeTab[0];
    tmp_path = \
	(char*)(strdup((char*)node->children->content));
    pub->ss_delivery_url = pub->ms_delivery_url = \
	pub->fs_delivery_url =	(int8_t*)tmp_path;

 done:
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    if (xpath_ctx) xmlXPathFreeContext(xpath_ctx);
    return err;
}

#endif

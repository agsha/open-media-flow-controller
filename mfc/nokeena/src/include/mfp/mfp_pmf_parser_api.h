#ifndef _MFP_PMF_PARSER_API_
#define _MFP_PMF_PARSER_API_

#include <libxml/xmlschemastypes.h>

#include "mfp_publ_context.h"
#include "nkn_debug.h"

#ifdef __cplusplus 
extern "C" {
#endif

	extern uint32_t xsd_len;
	extern int8_t* xml_schema_data;

    /*********************************************************************  
     *                  API FUNCTIONS
     ********************************************************************/
    /**
     * parses a PMF file and populates a publisher context
     * @param pmf_file - path to the pmf file
     * @param mp [out] - fully populated publisher context returned to 
     * the caller
     * @return returns 0 on success, negative number on error 
     */
    int32_t readPublishContextFromFile(char *pmf_file, 
				       mfp_publ_t **pub); 

    /**
     * A generic untility function to load file into memory
     * Input: file_path
     * Output: file_data and file_size
     * If (*file_data) is not NULL => Success else Error
     */
    void readFileIntoMemory(int8_t const* file_path, 
			    int8_t** file_data,
			    uint32_t* file_size);

#ifdef __cpluplus
}
#endif

#endif //_MFP_PMF_PARSER_API_

#ifndef _LF_METRICS2XML_API_
#define _LF_METRICS2XML_API_

#include <stdio.h>

// libxml
#include <libxml/xmlwriter.h>

// lf includes
#include "lf_msg_handler.h" 
#include "lf_common_defs.h"
#include "lf_metrics_monitor.h"
#include "lf_err.h"
#include "lf_dbg.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

typedef struct tag_xml_out_cfg {
    uint32_t section_mask;
} xml_out_cfg_t;

int32_t writeSystemMetrics(xmlTextWriterPtr writer, 
			   lf_metrics_ctx_t *lfm,
			   void **metrics);
int32_t writeNAMetrics(xmlTextWriterPtr writer,
		       void **metrics);
int32_t writeVMMetrics(xmlTextWriterPtr writer,
		       void **metrics);
int32_t writeXMLFromMetrics(xml_out_cfg_t *cfg,
			    lf_metrics_ctx_t *lfm,
			    void **metrics, void **out);
int32_t lfWriteFilterdCounterSet(xmlTextWriterPtr writer,
				 void **metrics);

#ifdef __cplusplus
}
#endif

#endif //_LF_METRICS2XML_API_

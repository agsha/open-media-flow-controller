#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "lf_metrics2xml_api.h"

/* globals */
const char *lf_na_http_disk_tier_name[/*lf_na_http_disk_tier_t*/] = {
    "SSD",     /* LF_NA_HTTP_DISK_TIER_SSD */
    "SAS",     /* LF_NA_HTTP_DISK_TIER_SAS */
    "SATA"     /* LF_NA_HTTP_DISK_TIER_SATA */
};
const char *lf_xml_ns_str = "http://www.juniper.net/mfc/schema/lf";
extern char * g_lfd_xml_ns_str;
extern uint32_t g_na_http_show_vip_stats;

static int32_t lf_strip_counter_name(
			const char *const full_name,
			char *entity_name, 
			uint32_t entity_name_len,
			char *counter_name,
			uint32_t counter_name_len);
uint64_t glob_lf_xml_val_oor = 0;

#define XML_CLAMP_VAL(_x, _clamp, _tol) \
    if (_x > _tol) { \
        glob_lf_xml_val_oor++;\
    }\
    if (_x > _clamp) { \
        _x = _clamp; \
    } \

int32_t writeSystemMetrics(xmlTextWriterPtr writer, 
			   lf_metrics_ctx_t *lfm,
			   void **metrics)
{
    int32_t err = 0;
    uint32_t i, vmm_max_cpu = 0;
    lf_na_http_metrics_out_t *hm = NULL;

    assert(metrics);
    assert(lfm);
    assert(metrics[LF_SYSTEM_SECTION]);

    if (lfm->section_map[LF_NA_SECTION] == LF_SECTION_AVAIL) {
	hm = metrics[LF_NA_SECTION];
    }
    lf_sys_glob_metrics_out_t *sgm = metrics[LF_SYSTEM_SECTION];
    lf_vma_metrics_out_t *vmm = metrics[LF_VM_SECTION];

    /* Write the SYSTEM section
     * includes CPU, TCP connection rate and Disk I/O metrics
     */
    err = xmlTextWriterStartElement(writer, BAD_CAST "system");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " buffer");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterStartElement(writer, BAD_CAST "cpuInfo");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " buffer");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    for (i = 0; i < sgm->cpu_max; i++) {
	err = xmlTextWriterStartElement(writer, BAD_CAST "cpu");
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   " buffer");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "id",
						"%d", i);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " s->cpu[%d]->id", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	XML_CLAMP_VAL(sgm->cpu_use[i], 100, 101);
	err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "util",
						"%4.2f", sgm->cpu_use[i]);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " s->cpu[%d]->usage", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterEndElement(writer);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " s->cpu section");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

    }
    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->cpuInfo section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    vmm_max_cpu = 0;
    if (vmm) {
	vmm_max_cpu = vmm->cpu_max;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxCPUUsage",
					  "%d", 100); //*(sgm->cpu_max
					  //- vmm_max_cpu));
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->cpu_max");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "ifBandwidth",
					  "%lu", sgm->if_bw);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->ifBandwidth");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxIfBandwidth",
					  "%lu", sgm->if_bw_max);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxIfBandwidth");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

#if 0
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "diskIOPS",
					  "%lu", sgm->blk_io_rate);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->diskIOPS");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
#endif

    if (sgm->num_disks) {
	err = xmlTextWriterStartElement(writer, BAD_CAST "diskInfo");
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   " buffer");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
	for(i = 0; i < sgm->num_disks; i++) {
	    err = xmlTextWriterStartElement(writer, BAD_CAST "disk");
	    if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   " buffer");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	    }
	    
	    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "id",
					    "%s", sgm->disk_stats[i].name);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " s->cpu[%d]->id", i);
		err = -E_LF_XML_GENERIC;
		goto error;
	    }
	    
	    XML_CLAMP_VAL(sgm->disk_stats[i].util, 100, 101);
	    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "util",
				    "%4.2f", sgm->disk_stats[i].util);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " s->cpu[%d]->usage", i);
		err = -E_LF_XML_GENERIC;
		goto error;
	    }

#if 0
	    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "m",
					    "%d", sgm->blk_io_rate_max);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " s->cpu[%d]->usage", i);
		err = -E_LF_XML_GENERIC;
		goto error;
	    }
#endif	    
	    err = xmlTextWriterEndElement(writer);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " s->cpu section");
		err = -E_LF_XML_GENERIC;
		goto error;
	    }
	}
	err = xmlTextWriterEndElement(writer);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " system section");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

    }	

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxDiskUsage",
					  "%u", 100);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxDiskIOPS");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    uint64_t tot_http_conn = 0;
    if (hm) {
#if 0
	for (i = 0; i < hm->num_rp; i++) {
	    tot_http_conn += hm->rpm[i].active_conn;
	}
#endif
	tot_http_conn = hm->gm.tps;
    }
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "httpTPS",
					  "%lu", tot_http_conn);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxDiskIOPS");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST
					  "maxHttpTPS",
					  "%u", sgm->http_tps_max);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxDiskIOPS");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
    
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "tcpConnRate",
					  "%d", sgm->tcp_conn_rate);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->tcpConnRate");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxTcpConnRate",
					  "%d", sgm->tcp_conn_rate_max);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxTcpConnRate");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "tcpActiveConn",
					  "%d", sgm->num_tcp_conn);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxTcpConnRate");
	err = -E_LF_XML_GENERIC;
	goto error;
    }


    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST
					  "maxTcpActiveConn",
					  "%d", sgm->tcp_active_conn_max);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->maxTcpConnRate");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
    
    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " system section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

 error:
    return err;
}

int32_t writeVMMetrics(xmlTextWriterPtr writer,
		       void **metrics)
{

    int32_t err = 0;
    lf_vma_metrics_out_t *vmm = metrics[LF_VM_SECTION];

    if (!vmm) {
	return 0;
    }

    /* write the VMA section */
    err = xmlTextWriterStartElement(writer, BAD_CAST "vma");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " buffer");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "vmaName",
					  "%s", vmm->name);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " vm->vmaName");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

	
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "cpu",
					  "%4.2f", vmm->cpu_use);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " vm->cpu");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxCPU",
					  "%d", vmm->cpu_max * 100);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " vm->cpu_max");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
	
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "ifBandwidth",
					  "%lu", vmm->if_bw_use);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " vm->ifBandwidth");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxIfBandwidth",
					  "%lu", vmm->if_bw_max);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " vm->maxIfBandwidth");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       "vm section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
	
 error:
    return err;
}

int32_t writeNAMetrics(xmlTextWriterPtr writer,
		       void **metrics)
{
    int32_t err = 0;
    uint32_t i;
    lf_na_http_metrics_out_t *hm = metrics[LF_NA_SECTION];
    char vip_ip_str[INET_ADDRSTRLEN + 1] = {0};

    if (!hm) {
	return 0;
    }
    err = xmlTextWriterStartElement(writer, BAD_CAST "na");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " na section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterStartElement(writer, BAD_CAST "http");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " na->http");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    XML_CLAMP_VAL(hm->gm.lf, 100, 101);
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "cpuUsage",
					  "%4.2f", hm->gm.lf);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " hm->gm.lf");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    XML_CLAMP_VAL(hm->gm.ram_lf, 100, 101);
    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "ramUsage",
					  "%4.2f", hm->gm.ram_lf);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " hm->gm.ram_lf");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    for (i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++ ) {
	err = xmlTextWriterStartElement(writer, BAD_CAST "disk");
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   "na->http->disk");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST
		"tier", "%s", lf_na_http_disk_tier_name[i]);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " s->cpu[%d]->id", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatElement(writer,
	      BAD_CAST "prereadState", "%d", hm->gm.preread_state[i]);

	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->preread[%d]->name", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	XML_CLAMP_VAL(hm->gm.disk_lf[i], 100, 101);
	err = xmlTextWriterWriteFormatElement(writer,
	      BAD_CAST "diskUsage", "%2.2f", hm->gm.disk_lf[i]);

	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->preread[%d]->name", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterEndElement(writer);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   "resource pool close");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
    }

    if (g_na_http_show_vip_stats) {
	for (i = 0; i < hm->gm.num_vip; i++) {
	    err = xmlTextWriterStartElement(writer, BAD_CAST "vip");
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		       "na->http->vip");
		err = -E_LF_XML_GENERIC;
		goto error;
	    }

	    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "name",
						  "%s", hm->vipm[i].name);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " na->http->vip[%d]->name", i);
		err = -E_LF_XML_GENERIC;
		goto error;
	    }

	    memset(vip_ip_str, 0, INET_ADDRSTRLEN + 1);
	    inet_ntop(AF_INET, &hm->vipm[i].ip, 
		      vip_ip_str, INET_ADDRSTRLEN + 1);
	    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "ip",
						  "%s", vip_ip_str);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " na->http->vip[%d]->name", i);
		err = -E_LF_XML_GENERIC;
		goto error;
	    }

	    err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "totSessions",
						  "%ld", hm->vipm[i].tot_sessions);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " na->http->vip[%d]->name", i);
		err = -E_LF_XML_GENERIC;
		goto error;
	    }

	    err = xmlTextWriterEndElement(writer);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       "vip close");
		err = -E_LF_XML_GENERIC;
		goto error;
	    }
	}
    }

    for (i = 0; i < hm->gm.num_rp; i++) {
	err = xmlTextWriterStartElement(writer, BAD_CAST "resourcePool");
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   "na->http->rp");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "name",
					      "%s", hm->rpm[i].name);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->rp[%d]->name", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "availability",
					      "%u", hm->rpm[i].avail);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->rp[%d]->avail", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "bandwidth",
					      "%lu", hm->rpm[i].bw_use);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->rp[%d]->bw_use", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
	    
	err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "maxBandwidth",
					      "%lu", hm->rpm[i].bw_max);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->rp[%d]->bw_max", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "activeConn",
					      "%u", hm->rpm[i].active_conn);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->rp[%d]->active_conn", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
	    
	err = xmlTextWriterWriteFormatElement(writer, 
	      BAD_CAST "maxActiveConn", "%u", hm->rpm[i].active_conn_max);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " na->http->rp[%d]->active_conn_max", i);
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}

	err = xmlTextWriterEndElement(writer);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   "resource pool close");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
    }

    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       "http pool close");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

 error:
    return err;
    
}

int32_t writeXMLFromMetrics(xml_out_cfg_t *cfg,
			    lf_metrics_ctx_t *lfm,
			    void **metrics, void **out)
{
    xmlTextWriterPtr writer = NULL;
    xmlBufferPtr buf = NULL;
    int32_t err = 0;
    char *tbuf = NULL, *xml_ns_str = (char*)lf_xml_ns_str;
	
    buf = xmlBufferCreate();
    if (!buf) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml buffer");
	err = -E_LF_NO_MEM;
	goto error;
    }

    writer = xmlNewTextWriterMemory(buf, 0);
    if (!writer) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml buffer");
	err = -E_LF_NO_MEM;
	goto error;
    }

    err = xmlTextWriterSetIndent(writer, 1);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml buffer");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterStartDocument(writer, "1.0", "utf-8",
				     NULL);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " buffer");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
	
    err = xmlTextWriterStartElement(writer, BAD_CAST "loadSnapshot");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " root tag");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    if (g_lfd_xml_ns_str) {
	xml_ns_str = g_lfd_xml_ns_str;
    }
    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "xmlns",
					    "%s", xml_ns_str);

    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " writing snapshot timestamp");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    tbuf = lf_get_snapshot_time_safe(lfm);
    if (tbuf) {
	err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "at",
						"%s", tbuf);

	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   " writing snapshot timestamp");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
    }
    lf_release_lock(lfm);

    err = writeSystemMetrics(writer, lfm, metrics);
    if (err < 0) 
	goto error;

    if (metrics[LF_VM_SECTION]) {
	err = writeVMMetrics(writer, metrics);
	if (err < 0) 
	    goto error;
    }

    if (lfm->section_map[LF_NA_SECTION] == LF_SECTION_AVAIL) {
	err = writeNAMetrics(writer, metrics);
	if (err < 0)
	    goto error;
    }
    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       "closing loadMetrics element");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterEndDocument(writer);	
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " document end");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    xmlFreeTextWriter(writer);
    *out = buf;
    return err;

 error:
    xmlFreeTextWriter(writer);
    free(buf->content);
    return err;

}

static int32_t
lf_strip_counter_name(const char *const full_name,
		      char *entity_name, 
		      uint32_t entity_name_len,
		      char *counter_name,
		      uint32_t counter_name_len)
{
    int32_t err = 0;
    const uint32_t max_lvl = 10;
    uint32_t len = 0, i = 0, pos[max_lvl], copy_len = 0;
    char *p = NULL;

    p = (char *const)full_name;
    while(len != LF_NA_HTTP_MAX_RP && *p != '\0') {
	if (*p == '.') {
	    if (i < max_lvl) {
		pos[i] = len;
		i++;
	    } else {
		err = -1;
		goto error;
	    }
	}
	p++;
	len++;
    }
    if (len == LF_NA_HTTP_MAX_RP) {
	return -1;
    }

    p = (char *const)full_name;
    copy_len = pos[1] - pos[0] - 1;
    if (copy_len > entity_name_len) {
	return -1;
    }
    memcpy(entity_name, p + pos[0] + 1, copy_len);
    entity_name[copy_len] = '\0';

    copy_len = len - pos[1] - 1;
    if (copy_len > counter_name_len) {
	return -1;
    }
    memcpy(counter_name, p + pos[1] + 1, copy_len);
    counter_name[copy_len] = '\0';

 error:
    return 0;
}

int32_t
lfWriteFilterdCounterSet(xmlTextWriterPtr writer,
			 void **metrics)
{
    struct cp_vec_list_t *list = NULL;
    cp_vec_list_elm_t *elm = NULL;
    cp_vector *vec = NULL;
    lf_na_filter_out_t *fo = NULL;
    int32_t err =0, n = 0, i, id;
    char en[LF_NA_ENTITY_NAME_LEN] = {0}, 
	cn[LF_NA_COUNTER_NAME_LEN] ={0};
    const char *const section_str[] = {"resourcePoolList",
				       "namespaceList"};
    const char *const sub_section_str[] = {"resourcePool",
					   "namespace"};

    assert(writer);
    assert(metrics);

    err = xmlTextWriterStartElement(writer, BAD_CAST "na");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " na section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    err = xmlTextWriterStartElement(writer, BAD_CAST "http");
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
	       " na->http");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
    
    fo = (lf_na_filter_out_t *)(metrics[LF_NA_SECTION]);
    for (id = 0; id < LF_NA_FILTER_ID_MAX; id++) {
	list = fo->filter_out[id]; 
	if (!list) {
	    continue;
	}
	err = xmlTextWriterStartElement(writer, BAD_CAST section_str[id]);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
		   "na->http->rp");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
	TAILQ_FOREACH(elm, list, entries) {
	    vec = elm->v;
	    n = cp_vector_size(vec);
	    for (i = 0; i < n; i++) {
		glob_item_t *item = NULL;
		item = (glob_item_t*)cp_vector_element_at(vec, i);
		if (item) {
		    err =
			lf_strip_counter_name(
				fo->counter_name_offset + item->name,
				en, LF_NA_ENTITY_NAME_LEN, 
				cn, LF_NA_COUNTER_NAME_LEN);
		    if (i == 0) {
			err = xmlTextWriterStartElement(writer, BAD_CAST sub_section_str[id]);
			if (err < 0) {
			    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
				   "na->http->rp");
			    err = -E_LF_XML_GENERIC;
			    goto error;
			}
			
			err = xmlTextWriterWriteFormatElement(writer, BAD_CAST "name",
							      "%s", en);
			if (err < 0) {
			    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
				   " na->http->rp[%d]->name", i);
			    err = -E_LF_XML_GENERIC;
			    goto error;
			}
		    }
		    err = xmlTextWriterStartElement(writer, BAD_CAST "c");
		    if (err < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error creating xml"
			       " buffer");
			err = -E_LF_XML_GENERIC;
			goto error;
		    }

		    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "n",
							    "%s", cn);
		    if (err < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
			       " s->cpu[%d]->id", i);
			err = -E_LF_XML_GENERIC;
			goto error;
		    }

		    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "t",
							    "%d", item->size);
		    if (err < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
			       " s->cpu[%d]->usage", i);
			err = -E_LF_XML_GENERIC;
			goto error;
		    }

		    err = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "v",
							    "%lu", item->value);
		    if (err < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
			       " s->cpu[%d]->usage", i);
			err = -E_LF_XML_GENERIC;
			goto error;
		    }

		    err = xmlTextWriterEndElement(writer);
		    if (err < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
			       " s->cpu section");
			err = -E_LF_XML_GENERIC;
			goto error;
		    }
		    printf("item->name: %s, item->size: %d "
			   "item->value: %ld\n",
			   fo->counter_name_offset + item->name, 
			   item->size, item->value);
		}
	    }
	    err = xmlTextWriterEndElement(writer);
	    if (err < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		       " s->cpu section");
		err = -E_LF_XML_GENERIC;
		goto error;
	    }
	}
	err = xmlTextWriterEndElement(writer);
	if (err < 0) {
	    LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
		   " s->cpu section");
	    err = -E_LF_XML_GENERIC;
	    goto error;
	}
    }

    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->cpu section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
    
    err = xmlTextWriterEndElement(writer);
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " s->cpu section");
	err = -E_LF_XML_GENERIC;
	goto error;
    }
    
    err = xmlTextWriterEndDocument(writer);	
    if (err < 0) {
	LF_LOG(LOG_ERR, LFD_MSG_HDLR, "error writing element"
	       " document end");
	err = -E_LF_XML_GENERIC;
	goto error;
    }

    xmlFreeTextWriter(writer);    

 error:
    return err;
}

/*
 * @file diameter_avp.c
 * @brief
 * diameter_avp.c - definations for diameter avp functions.
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "nkn_memalloc.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

#include "jnx/getput.h"
#include "jnx/jsf/jsf_ip.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/dia_avp_dict_api.h"
#include "jnx/diameter_defs.h"
#include "jnx/diameter_avp_api.h"

#include "diameter_avp.h"

NKNCNT_DEF(dia_avp_msg, AO_t, "", "Avp Type - Message")
NKNCNT_DEF(dia_avp_payload, AO_t, "", "Avp Type - Payload")
NKNCNT_DEF(dia_avp_vendor, AO_t, "", "Avp Type - Vendor")
NKNCNT_DEF(dia_avp_dictionary, AO_t, "", "Avp Type - Dictionary")
NKNCNT_DEF(dia_avp_unknown, AO_t, "", "Avp Type - Unknown")

diameter_avp_dict_handle dictionary;

typedef struct aaa_diam_dictionary_s {
	aaa_diam_avp_list_t avp_ordinal;
	char avp_name[AAA_DIAM_MAX_AVP_NAME_LEN];
	uint32_t avp_code;
	diameter_avp_format_t avp_type;
	uint32_t app_type;
	uint32_t avp_must_flag;
	uint32_t avp_must_not_flag;
	uint32_t avp_vendor_id;
	uint32_t avp_vendor_flag;
} aaa_diam_dictionary_t;

static diameter_avp_attribute_info_t aaa_diam_dic[] = {
        { AAA_DIAM_AVP_SUPPORTED_FEATURES,
          "Supported-Features",
          628,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PACKET_FILTER_INFORMATION,
          "Packet-Filter-Information",
          1061,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_QOS_INFORMATION,
          "QoS-Information",
          1016,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ALLOCATION_RETENTION_PRIORITY,
          "Allocation-Retention-Priority",
          1034,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_DEFAULT_EPS_BEARER_QOS,
          "Default-EPS-Bearer-QoS",
          1049,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TFT_PACKET_FILTER_INFORMATION,
          "TFT-Packet-Filter-Information",
          1013,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CHARGING_RULE_REPORT,
          "Charging-Rule-Report",
          1018,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_EVENT_REPORT_INDICATION,
          "Event-Report-Indication",
          1033,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCESS_NETWORK_CHARGING_IDENTIFIER_GX,
          "Access-Network-Charging-Identifier-Gx",
          1022,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CHARGING_RULE_REMOVE,
          "Charging-Rule-Remove",
          1002,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CHARGING_RULE_INSTALL,
          "Charging-Rule-Install",
          1001,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CHARGING_RULE_DEFINITION,
          "Charging-Rule-Definition",
          1003,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FLOW_INFORMATION,
          "Flow-Information",
          1058,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FLOWS,
          "Flows",
          510,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CHARGING_INFORMATION,
          "Charging-Information",
          618,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FINAL_UNIT_INDICATION,
          "Final-Unit-Indication",
          430,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REDIRECT_SERVER,
          "Redirect-Server",
          434,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_USER_CSG_INFORMATION,
          "User-CSG-Information",
          2319,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRACE_DATA,
          "Trace-Data",
          1458,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_USAGE_MONITORING_INFO,
          "Usage-Monitoring-Information",
          1067,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_COA_INFORMATION,
          "CoA-Information",
          1039,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        /* Gy specific Grouped AVPs */
        { AAA_DIAM_AVP_SUBSCRIPTION_ID,
          "Subscription-Id",
          443,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_GRANTED_SERVICE_UNIT,
          "Granted-Service-Unit",
          431,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REQ_SERVICE_UNIT,
          "Requested-Service-Unit",
          437,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_USED_SERVICE_UNIT,
          "Used-Service-Unit",
          446,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_GSU_POOL_REF,
          "G-S-U-Pool-Reference",
          457,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_MONEY,
          "CC-Money",
          413,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ENVELOPE,
          "Envelope",
          1266,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_UNIT_VALUE,
          "Unit-Value",
          445,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TRIGGER,
          "Trigger",
          1264,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_AF_CORRELATION_INFO,
          "AF-Correlation-Information",
          1276,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_SPECIFIC_INFO,
          "Service-Specific-Info",
          1249,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_USER_EQUIPMENT_INFO,
          "User-Equipment-Info",
          458,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PROXY_INFO,
          "Proxy-Info",
          284,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_P,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_INFO,
          "Service-Information",
          873,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AOC_INFO,
          "AoC-Information",
          2054,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_AOC_COST_INFO,
          "AoC-Cost-Information",
          2053,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCUMULATED_COST,
          "Accumulated-Cost",
          2052,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_INCREMENTAL_COST,
          "Incremental-Cost",
          2062,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TARIFF_INFO,
          "Tariff-Information",
          2060,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CURRENT_TARIFF,
          "Current-Tariff",
          2056,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SCALE_FACTOR,
          "Scale-Factor",
          2059,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_RATE_ELEMENT,
          "Rate-Element",
          2058,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_UNIT_COST,
          "Unit-Cost",
          2061,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_NEXT_TARIFF,
          "Next-Tariff",
          2057,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_AOC_SUBSCRIPTION_INFO,
          "AoC-Subscription-Information",
          2314,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AOC_SERVICE,
          "AoC-Service",
          2311,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PS_INFORMATION,
          "PS-Information",
          874,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PS_FURNISH_CHARGE_INFO,
          "PS-Furnish-Charging-Information",
          865,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_OFFLINE_CHARGING,
          "Offline-Charging",
          1278,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TIME_QUOTA_MECHANISM,
          "Time-Quota-Mechanism",
          1270,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRAFFIC_DATA_VOL,
          "Traffic-Data-Volumes",
          2046,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_DATA_CONTAINER,
          "Service-Data-Container",
          2040,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FAILED_AVP,
          "Failed-AVP",
          279,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        /* List of other Gx AVPs */
        { AAA_DIAM_AVP_VENDOR_ID,
          "Vendor-Id",
          266,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_FEATURE_LIST_ID,
          "Feature-List-ID",
          629,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FEATURE_LIST,
          "Feature-List",
          630,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_NETWORK_REQUEST_SUPPORT,
          "Network-Request-Support",
          1024,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PCC_RULE_STATUS,
          "PCC-Rule-Status",
          1019,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_RULE_FAILURE_CODE,
          "Rule-Failure-Code",
          1031,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_RULE_ACTIVATION_TIME,
          "Rule-Activation-Time",
          1043,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_RULE_DEACTIVATION_TIME,
          "Rule-Deactivation-Time",
          1044,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_RESOURCE_ALLOCATION_NOTIFICATION,
          "Resource-Allocation-Notification",
          1063,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CHARGING_CORRELATION_INDICATOR,
          "Charging-Correlation-Indicator",
          1073,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PACKET_FILTER_CONTENT,
          "Packet-Filter-Content",
          1059,
          DIAMETER_FORMAT_IPFILTER_RULE,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PACKET_FILTER_IDENTIFIER,
          "Packet-Filter-Identifier",
          1060,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PRECEDENCE,
          "Precedence",
          1010,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TOS_TRAFFIC_CLASS,
          "ToS-Traffic-Class",
          1014,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SECURITY_PARAMETER_INDEX,
          "Security-Parameter-Index",
          1056,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_FLOW_LABEL,
          "Flow-Label",
          1057,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_FLOW_DIRECTION,
          "Flow-Direction",
          1080,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PACKET_FILTER_OPERATION,
          "Packet-Filter-Operation",
          1062,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_RAT_TYPE,
          "RAT-Type",
          1032,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_SGSN_ADDRESS,
          "3GPP-SGSN-Address",
          6,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_USER_LOCATION_INFO,
          "3GPP-User-Location-Info",
          22,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_MS_TIMEZONE,
          "3GPP-MS-TimeZone",
          23,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_RAI,
          "RAI",
          909,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_EVENT_TRIGGER,
          "Event-Trigger",
          1006,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
#if 0
        /* TODO: Code specified as 5535/9010 */
        { AAA_DIAM_AVP_3GPP2_BSID,
          "3GPP2-BSID",
          5535,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX |
                  AAA_DIAM_DICT_APP_GY),
                  0,
                  0,
                  0,
                  {
                   AAA_DIAM_AVP_EVENT_REPORT_INDICATION,
                   AAA_DIAM_AVP_PS_INFORMATION,
                   AAA_DIAM_AVP_SERVICE_DATA_CONTAINER,
                   AAA_DIAM_AVP_UNDEFINED,
                   AAA_DIAM_AVP_UNDEFINED,
                   AAA_DIAM_AVP_UNDEFINED,
                   AAA_DIAM_AVP_UNDEFINED }
        },
#endif 
        { AAA_DIAM_AVP_MAX_REQUESTED_BANDWIDTH_UL,
          "Maximum-Bandwidth-UL",
          516,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_MAX_REQUESTED_BANDWIDTH_DL,
          "Maximum-Bandwidth-DL",
          515,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_GUARANTEED_BITRATE_UL,
          "Guaranteed-Bitrate-UL",
          1026,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_GUARANTEED_BITRATE_DL,
          "Guaranteed-Bitrate-DL",
          1025,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRACE_REFERENCE,
          "Trace-Reference",
          1459,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRACE_DEPTH,
          "Trace-Depth",
          1462,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRACE_NE_TYPE_LIST,
          "Trace-NE-Type-List",
          1463,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRACE_INTERFACE_LIST,
          "Trace-Interface-List",
          1464,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TRACE_EVENT_LIST,
          "Trace-Event-List",
          1465,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_OMC_ID,
          "OMC-Id",
          1466,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TRACE_COLLECTION_ENTITY,
          "Trace-Collection-Entity",
          1452,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_BEARER_IDENTIFIER,
          "Bearer-Identifier",
          1020,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_BEARER_OPERATION,
          "Bearer-Operation",
          1021,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FRAMED_IP_ADDRESS,
          "Framed-IP-Address",
          8,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FRAMED_IPV6_PREFIX,
          "Framed-IPv6-Prefix",
          97,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_IP_CAN_TYPE,
          "IP-CAN-Type",
          1027,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_RAT_TYPE,
          "3GPP-RAT-Type",
          21,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_QOS_CLASS_IDENTIFIER,
          "QoS-Class-Identifier",
          1028,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_APN_AGGREGATE_MAX_BITRATE_UL,
          "APN-Aggregated-Max-Bitrate-UL",
          1041,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_APN_AGGREGATE_MAX_BITRATE_DL,
          "APN-Aggregated-Max-Bitrate-DL",
          1040,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_QOS_NEGOTIATION,
          "QoS-Negotiation",
          1029,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_QOS_UPGRADE,
          "QoS-Upgrade",
          1030,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AN_GW_ADDRESS,
          "AN-GW-Address",
          1050,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_SGSN_MCC_MNC,
          "3GPP-SGSN-MCC-MNC",
          18,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_SGSN_IPV6_ADDRESS,
          "3GPP-SGSN-IPv6-Address",
          15,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PDN_CONNECTION_ID,
          "PDN-Connection-ID",
          2050,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_BEARER_USAGE,
          "Bearer-Usage",
          1000,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ONLINE,
          "Online",
          1009,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_OFFLINE,
          "Offline",
          1008,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TFT_FILTER,
          "TFT-Filter",
          1012,
          DIAMETER_FORMAT_IPFILTER_RULE,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CHARGING_RULE_NAME,
          "Charging-Rule-Name",
          1005,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CHARGING_RULE_BASE_NAME,
          "Charging-Rule-Base-Name",
          1004,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCESS_NETWORK_CHARGING_ADDRESS,
          "Access-Network-Charging-Address",
          501,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_BEARER_CONTROL_MODE,
          "Bearer-Control-Mode",
          1023,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_IDENTIFIER,
          "Service-Identifier",
          439,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_RATING_GROUP,
          "Rating-Group",
          432,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FLOW_DESCRIPTION,
          "Flow-Description",
          507,
          DIAMETER_FORMAT_IPFILTER_RULE,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PACKET_FILTER_USAGE,
          "Packet-Filter-Usage",
          1072,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FLOW_STATUS,
          "Flow-Status",
          511,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REPORTING_LEVEL,
          "Reporting-Level",
          1011,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_METERING_METHOD,
          "Metering-Method",
          1007,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AF_CHARGING_IDENTIFIER,
          "AF-Charging-Identifier",
          505,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_MEDIA_COMPONENT_NUMBER,
          "Media-Component-Number",
          518,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FLOW_NUMBER,
          "Flow-Number",
          509,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FINAL_UNIT_ACTION,
          "Final-Unit-Action",
          449,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AF_SIGNALLING_PROTOCOL,
          "AF-Signalling-Protocol",
          529,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PRIMARY_EVENT_CHARGING_FUNCTION_NAME,
          "Primary-Event-Charging-Function-Name",
          619,
          DIAMETER_FORMAT_DIAMETER_URI,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SECONDARY_EVENT_CHARGING_FUNCTION_NAME,
          "Secondary-Event-Charging-Function-Name",
          620,
          DIAMETER_FORMAT_DIAMETER_URI,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PRIMARY_CHARGING_COLLECTION_FUNCTION_NAME,
          "Primary-Charging-Collection-Function-Name",
          621,
          DIAMETER_FORMAT_DIAMETER_URI,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SECONDARY_CHARGING_COLLECTION_FUNCTION_NAME,
          "Secondary-Charging-Collection-Function-Name",
          622,
          DIAMETER_FORMAT_DIAMETER_URI,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REVALIDATION_TIME,
          "Revalidation-Time",
          1042,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PRIORITY_LEVEL,
          "Priority-Level",
          1046,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY ),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PREEMPTION_CAPABILITY,
          "Pre-emption-Capability",
          1047,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY ),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PREEMPTION_VULNERABILITY,
          "Pre-emption-Vulnerability",
          1048,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY ),
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ACCESS_NETWORK_CHARGING_IDENTIFIER_VALUE,
          "Access-Network-Charging-Identifier-Value",
          503,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_MONITORING_KEY,
          "Monitoring-Key",
          1066,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_RESTRICTION_FILTER_RULE,
          "Restriction-Filter-Rule",
          438,
          DIAMETER_FORMAT_IPFILTER_RULE,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FILTER_ID,
          "Filter-Id",
          11,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REDIRECT_ADDRESS_TYPE,
          "Redirect-Address-Type",
          433,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REDIRECT_SERVER_ADDRESS,
          "Redirect-Server-Address",
          435,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CSG_ID,
          "CSG-Id",
          1437,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CSG_ACCESS_MODE,
          "CSG-Access-Mode",
          2317,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CSG_MEMBERSHIP_INDICATION,
          "CSG-Membership-Indication",
          2318,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SESSION_ID,
          "Session-Id",
          263,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ORIGIN_HOST,
          "Origin-Host",
          264,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ORIGIN_REALM,
          "Origin-Realm",
          296,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_DEST_REALM,
          "Destination-Realm",
          283,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_AUTH_APP_ID,
          "Auth-Application-Id",
          258,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ORIGIN_STATE_ID,
          "Origin-State-Id",
          278,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_CTX_ID,
          "Service-Context-Id",
          461,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CC_REQ_TYPE,
          "CC-Request-Type",
          416,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CC_REQ_NUM,
          "CC-Request-Number",
          415,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_DEST_HOST,
          "Destination-Host",
          293,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_USER_NAME,
          "User-Name",
          1,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_EVENT_TIMESTAMP,
          "Event-Timestamp",
          55,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SUBSCRIPTION_ID_TYPE,
          "Subscription-Id-Type",
          450,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SUBSCRIPTION_ID_DATA,
          "Subscription-Id-Data",
          444,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TERMINATION_CAUSE,
          "Termination-Cause",
          295,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_MULT_SERVICES_ID,
          "Multiple-Services-Indicator",
          455,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_MULT_SERVICES_CREDIT_CTRL,
          "Multiple-Services-Credit-Control",
          456,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TARIFF_CHANGE_USAGE,
          "Tariff-Change-Usage",
          452,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_VALIDITY_TIME,
          "Validity-Time",
          448,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_RESULT_CODE,
          "Result-Code",
          268,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TARIFF_TIME_CHANGE,
          "Tariff-Time-Change",
          451,
          DIAMETER_FORMAT_TIME,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_TIME,
          "CC-Time",
          420,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_TOTAL_OCTETS,
          "CC-Total-Octets",
          421,
          DIAMETER_FORMAT_UNSIGNED64,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_INPUT_OCTETS,
          "CC-Input-Octets",
          412,
          DIAMETER_FORMAT_UNSIGNED64,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_OUTPUT_OCTETS,
          "CC-Output-Octets",
          414,
          DIAMETER_FORMAT_UNSIGNED64,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_SERVICE_SPECIFIC_UNITS,
          "CC-Service-Specific-Units",
          417,
          DIAMETER_FORMAT_UNSIGNED64,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_GSU_POOL_ID,
          "G-S-U-Pool-Identifier",
          453,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CC_UNIT_TYPE,
          "CC-Unit-Type",
          454,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CURRENCY_CODE,
          "Currency-Code",
          425,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_VALUE_DIGITS,
          "Value-Digits",
          447,
          DIAMETER_FORMAT_INTEGER64,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_EXPONENT,
          "Exponent",
          429,
          DIAMETER_FORMAT_INTEGER32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REPORTING_REASON,
          "Reporting-Reason",
          872,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TRIGGER_TYPE,
          "Trigger-Type",
          870,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ENVELOPE_START_TIME,
          "Envelope-Start-Time",
          1269,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ENVELOPE_END_TIME,
          "Envelope-End-Time",
          1267,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SERVICE_SPECIFIC_DATA,
          "Service-Specific-Data",
          863,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_SPECIFIC_TYPE,
          "Service-Specific-Type",
          1257,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_USER_EQUIPMENT_INFO_TYPE,
          "User-Equipment-Info-Type",
          459,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_M,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_USER_EQUIPMENT_INFO_VALUE,
          "User-Equipment-Info-Value",
          460,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PROXY_HOST,
          "Proxy-Host",
          280,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_P,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PROXY_STATE,
          "Proxy-State",
          33,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_P,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ROUTE_RECORD,
          "Route-Record",
          282,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_P,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REASON_CODE,
          "Reason-Code",
          2316,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_UNIT_QUOTA_THRESHOLD,
          "Unit-Quota-Threshold",
          1226,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AOC_SERVICE_OBLIGATORY_TYPE,
          "AoC-Service-Obligatory-Type",
          2312,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AOC_SERVICE_TYPE,
          "AoC-Service-Type",
          2313,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AOC_FORMAT,
          "AoC-Format",
          2310,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_PREFERRED_AOC_CURRENCY,
          "Preferred-AoC-Currency",
          2315,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_CHARGING_ID,
          "3GPP-Charging-Id",
          2,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_NODE_ID,
          "Node-Id",
          2064,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_PDP_TYPE,
          "3GPP-PDP-Type",
          3,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PDP_ADDRESS,
          "PDP-Address",
          1227,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_DYNAMIC_ADDRESS_FLAG,
          "Dynamic-Address-Flag",
          2051,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SGSN_ADDRESS,
          "SGSN-Address",
          1228,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_GGSN_ADDRESS,
          "GGSN-Address",
          847,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SGW_ADDRESS,
          "SGW-Address",
          2067,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CG_ADDRESS,
          "CG-Address",
          846,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SERVING_NODE_TYPE,
          "Serving-Node-Type",
          2047,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SGW_CHANGE,
          "SGW-Change",
          2065,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_IMSI_MCC_MNC,
          "3GPP-IMSI-MCC-MNC",
          8,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_IMSI_UNAUTH_FLAG,
          "IMSI-Unauthenticated-Flag",
          2308,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_GGSN_MCC_MNC,
          "3GPP-GGSN-MCC-MNC",
          9,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_NSAPI,
          "3GPP-NSAPI",
          10,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CALLED_STATION_ID,
          "Called-Station-Id",
          30,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_SESSION_STOP_IND,
          "3GPP-Session-Stop-Indicator",
          11,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V  | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_SELECTION_MODE,
          "3GPP-Selection-Mode",
          12,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_CHARGING_CHAR,
          "3GPP-Charging-Characteristics",
          13,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CHARGING_CHAR_SEL_MODE,
          "Charging-Characteristics-Selection-Mode",
          2066,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PS_FREE_FORMAT_DATA,
          "PS-Free-Format-Data",
          866,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PS_APPEND_FREE_FORMAT_DATA,
          "PS-Append-Free-Format-Data",
          867,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PDP_CTXT_TYPE,
          "PDP-Context-Type",
          1247,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_QUOTA_CONSUMPTION_TIME,
          "Quota-Consumption-Time",
          881,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TIME_QUOTA_TYPE,
          "Time-Quota-Type",
          1271,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_BASE_TIME_INTERVAL,
          "Base-Time-Interval",
          1265,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ENVELOPE_REPORTING,
          "Envelope-Reporting",
          1268,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCOUNTING_INPUT_OCTETS,
          "Accounting-Input-Octets",
          363,
          DIAMETER_FORMAT_UNSIGNED64,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ACCOUNTING_INPUT_PKTS,
          "Accounting-Input-Packets",
          365,
          DIAMETER_FORMAT_UNSIGNED64,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ACCOUNTING_OUTPUT_OCTETS,
          "Accounting-Output-Octets",
          364,
          DIAMETER_FORMAT_UNSIGNED64,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ACCOUNTING_OUTPUT_PKTS,
          "Accounting-Output-Packets",
          366,
          DIAMETER_FORMAT_UNSIGNED64,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CHANGE_TIME,
          "Change-Time",
          2038,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_LOCAL_SEQ_NUM,
          "Local-Sequence-Number",
          2063,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TIME_FIRST_USAGE,
          "Time-First-Usage",
          2043,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TIME_LAST_USAGE,
          "Time-Last-Usage",
          2044,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TIME_USAGE,
          "Time-Usage",
          2045,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_START_TIME,
          "Start-Time",
          2041,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_STOP_TIME,
          "Stop-Time",
          2042,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_CHANGE_CONDITION,
          "Change-Condition",
          2037,
          DIAMETER_FORMAT_INTEGER32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_DIAGNOSTICS,
          "Diagnostics",
          2039,
          DIAMETER_FORMAT_INTEGER32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CC_SESSION_FAILOVER,
          "CC-Session-Failover",
          418,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_TIME_QUOTA_THRESHOLD,
          "Time-Quota-Threshold",
          868,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_VOL_QUOTA_THRESHOLD,
          "Volume-Quota-Threshold",
          869,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_QUOTA_HOLDING_TIME,
          "Quota-Holding-Time",
          871,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CREDIT_CTRL_FAIL_HANDLING,
          "Credit-Control-Failure-Handling",
          427,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REDIRECT_HOST,
          "Redirect-Host",
          292,
          DIAMETER_FORMAT_DIAMETER_URI,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REDIRECT_HOST_USAGE,
          "Redirect-Host-Usage",
          261,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REDIRECT_MAX_CACHE_TIME,
          "Redirect-Max-Cache-Time",
          262,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_REAUTH_REQ_TYPE,
          "Re-Auth-Request-Type",
          285,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_ERROR_MESSAGE,
          "Error-Message",
          281,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TERMINAL_INFO,
          "Terminal-Information",
          1401,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_IMEI,
          "IMEI",
          1402,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_SOFTWARE_VERSION,
          "Software-Version",
          1403,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_INTERIM_INTERVAL,
          "Acct-Interim-Interval",
          85,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_REALTIME_REQUIRED,
          "Accounting-Realtime-Required",
          483,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_MULTI_SESSION_ID,
          "Acct-Multi-Session-Id",
          50,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_RECORD_NUM,
          "Accounting-Record-Number",
          485,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_RECORD_TYPE,
          "Accounting-Record-Type",
          480,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_SESSION_ID,
          "Accounting-Session-Id",
          44,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_SUB_SESSION_ID,
          "Accounting-Sub-Session-Id",
          287,
          DIAMETER_FORMAT_UNSIGNED64,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ACCT_APPLICATION_ID,
          "Acct-Application-Id",
          259,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AUTH_REQUEST_TYPE,
          "Auth-Request-Type",
          274,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AUTH_LIFETIME,
          "Authorization-Lifetime",
          291,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_AUTH_GRACE_PERIOD,
          "Auth-Grace-Period",
          276,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CLASS,
          "Class",
          25,
          DIAMETER_FORMAT_OCTETSTRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_DISCONNECT_CAUSE,
          "Disconnect-Cause",
          273,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        /* TODO: RFC3588 dosent give the AVPs contained in this Grouped AVP */
        { AAA_DIAM_AVP_E2E_SEQUENCE,
          "E2E-Sequence",
          300,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERROR_REPORTING_HOST,
          "Error-Reporting-Host",
          294,
          DIAMETER_FORMAT_DIAMETER_IDENTITY,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_EXPERIMENTAL_RESULT,
          "Experimental-Result",
          297,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_EXPERIMENTAL_RESULT_CODE,
          "Experimental-Result-Code",
          298,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FIRMWARE_REVISION,
          "Firmware-Revision",
          267,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M | DIAMETER_AVP_FLAG_P,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_HOST_IP_ADDR,
          "Host-IP-Address",
          257,
          DIAMETER_FORMAT_ADDRESS,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_INBAND_SECURITY_ID,
          "Inband-Security-Id",
          299,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_MULTI_ROUND_TIME_OUT,
          "Multi-Round-Time-Out",
          272,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_PRODUCT_NAME,
          "Product-Name",
          269,
          DIAMETER_FORMAT_UTF8STRING,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          0, DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M | DIAMETER_AVP_FLAG_P,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SESSION_TIMEOUT,
          "Session-Timeout",
          27,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SESSION_BINDING,
          "Session-Binding",
          270,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SESSION_SERVER_FAILOVER,
          "Session-Server-Failover",
          271,
          DIAMETER_FORMAT_ENUM,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SUPPORTED_VENDOR_ID,
          "Supported-Vendor-Id",
          265,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_VENDOR_SPECIFIC_APP_ID,
          "Vendor-Specific-Application-Id",
          260,
          DIAMETER_FORMAT_GROUPED,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_USAGE_MONITORING_LEVEL,
          "Usage-Monitoring-Level",
          1068,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_USAGE_MONITORING_REPORT,
          "Usage-Monitoring-Report",
          1069,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_USAGE_MONITORING_SUPPORT,
          "Usage-Monitoring-Support",
          1070,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TUNNEL_INFO,
          "Tunnel-Information",
          1038,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_COA_IP_ADDR,
          "CoA-IP-Address",
          1035,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TUNNEL_HEADER_LEN,
          "Tunnel-Header-Length",
          1037,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TUNNEL_HEADER_FILTER,
          "Tunnel-Header-Filter",
          1036,
          DIAMETER_FORMAT_IPFILTER_RULE,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SESSION_REL_CAUSE,
          "Session-Release-Cause",
          1045,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_MANDATORY_CAPABILITY,
          "Mandatory-Capability",
          604,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_OPTIONAL_CAPABILITY,
          "Optional-Capability",
          605,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REASON_INFO,
          "Reason-Info",
          617,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_DIRECT_DEBITING_FAILURE_HANDLING,
          "Direct-Debiting-Failure-Handling",
          428,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_LOW_BALANCE_INDICATION,
          "Low-Balance-Indication",
          2020,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_VF_USER_AGENT,
          "User-Agent",
          279,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M,  DIAMETER_AVP_FLAG_P,
          VENDOR_ID_VODAFONE,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_VF_USER_URL,
          "User-URL",
          280,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M,  DIAMETER_AVP_FLAG_P,
          VENDOR_ID_VODAFONE,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_VF_TRAFFIC_REDIRECTED,
          "Traffic-Redirected",
          278,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M,  DIAMETER_AVP_FLAG_P,
          VENDOR_ID_VODAFONE,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_NAS_PORT,
          "NAS-Port",
          5,
          DIAMETER_FORMAT_UNSIGNED32,
          (AAA_DIAM_DICT_APP_GX | AAA_DIAM_DICT_APP_GY),
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_CSG_INFORMATION_REPORTING,
          "CSG-Information-Reporting",
          1071,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },

        { AAA_DIAM_AVP_3GPP_GPRS_NEGOTIATED_QOS_PROFILE,
          "3GPP-GPRS-Negotiated-QoS-Profile",
          5,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_3GPP_GGSN_ADDRESS,
          "3GPP-GGSN-Address",
          7,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_RULE_SPACE_SUGGESTION,
          "Rule-Space-Suggestion",
          290,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_GX_CAPABILITY_LIST,
          "Gx-Capability-List",
          1060,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_CREDIT_INSTANCE_ID,
          "Credit-Instance-ID",
          1143,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_SERVICE_START_TIMESTAMP,
          "Service-Start-Timestamp",
          1144,
          DIAMETER_FORMAT_TIME,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_CUMULATIVE_USED_SERVICE_UNIT,
          "Cumulative-Used-Service-Unit",
          1145,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_RULE_SPACE_DECISION,
          "Rule-Space-Decision",
          291,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ERICSSON_RULE_SPACE_DECISION,
          "Rule-Space-Decision",
          291,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_ERICSSON,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_MIP_HA_TO_MN_MSA,
          "MIP-HA-to-MN-MSA",
          332,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GY,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          0,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_APPLICATION_DETECTION_INFO,
          "Application-Detection-Info",
          1098,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TDF_APPLICATION_IDENTIFIER,
          "TDF-Application-Identifier",
          1088,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TDF_APPLICATION_IDENTIFIER_BASE,
          "TDF-Application-Identifier-Base",
          1100,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_TDF_APPLICATION_INSTANCE_IDENTIFIER,
          "TDF-Application-Instance-Identifier",
          2802,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ADC_RULE_INSTALL,
          "ADC-Rule-Install",
          1092,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0, 
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ADC_RULE_REMOVE,
          "ADC-Rule-Remove",
          1093,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0, 
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ADC_RULE_DEFINITION,
          "ADC-Rule-Definition",
          1094,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ADC_RULE_BASE_NAME,
          "ADC-Rule-Base-Name",
          1095,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ADC_RULE_NAME,
          "ADC-Rule-Name",
          1096,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_ADC_RULE_REPORT,
          "ADC-Rule-Report",
          1097,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V | DIAMETER_AVP_FLAG_M, 0,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_MUTE_NOTIFICATION,
          "Mute-Notification",
          2809,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, DIAMETER_AVP_FLAG_M,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_SERVICE_CHAIN_IDENTIFIER,
          "Service-Chain-Identifier",
          1101,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_LRF_PROFILE_NAME,
          "LRF-Profile-Name",
          1102,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_HCM_PROFILE_NAME,
          "HCM-Profile-Name",
          1103,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_FORWARDING_CLASS_NAME,
          "Forwarding-Class-Name",
          1104,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REDIRECT_INFO,
          "Redirect-Information",
          1085,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REDIRECT_SUPPORT,
          "Redirect-Support",
          1086,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_M, DIAMETER_AVP_FLAG_V,
          VENDOR_ID_3GPP,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REDIRECT_VRF,
          "Redirect-VRF",
          1105,
          DIAMETER_FORMAT_UTF8STRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REQ_BURST_SIZE_UL,
          "Requested-Burst-Size-Uplink",
          1106,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_REQ_BURST_SIZE_DL,
          "Requested-Burst-Size-Downlink",
          1107,
          DIAMETER_FORMAT_UNSIGNED32,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_STEERING_INFO,
          "Steering-Information",
          1108,
          DIAMETER_FORMAT_GROUPED,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_STEERING_VRF_UL,
          "Steering-VRF-Uplink",
          1109,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_STEERING_VRF_DL,
          "Steering-VRF-Downlink",
          1110,
          DIAMETER_FORMAT_OCTETSTRING,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_STEERING_IP_ADDRESS,
          "Steering-IP-Address",
          1111,
          DIAMETER_FORMAT_ADDRESS,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        { AAA_DIAM_AVP_KEEP_EXISTING_STEERING,
          "Keep-Existing-Steering",
          1112,
          DIAMETER_FORMAT_ENUM,
          AAA_DIAM_DICT_APP_GX,
          DIAMETER_AVP_FLAG_V, 0,
          VENDOR_ID_JUNIPER,
          0x0000, 0, 0
        },
        /* must be the last entry */
        {
            0,
            "",
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        }
};

diameter_avp_rc_t diameter_add_octet_string(aaa_diam_avp_list_t avp_ordinal,
			diameter_avp_payload_handle payload, uint8_t *str,
			uint32_t size, diameter_avp_message_handle msg_hdl)
{
	diameter_avp_variant_t var;
	(void)msg_hdl;

	memset(&var, 0x00, sizeof(diameter_avp_variant_t));
	var.vt = DIAMETER_FORMAT_OCTETSTRING;
	var.avp_payload_len = size;
	var.data.a_i8.p = str;
	assert(var.data.a_i8.p != NULL);

	return diameter_avp_payload_add_avp(payload, avp_ordinal, &var);
}

diameter_avp_rc_t diameter_add_uint32(aaa_diam_avp_list_t avp_ordinal,
			diameter_avp_payload_handle payload_hdl,
			uint32_t val, diameter_avp_message_handle msg_hdl)
{
	diameter_avp_variant_t var;
	(void)msg_hdl;

	memset(&var, 0x00, sizeof(diameter_avp_variant_t));
	var.vt = DIAMETER_FORMAT_UNSIGNED32;
	var.data.v_i32 = val;

	return diameter_avp_payload_add_avp(payload_hdl, avp_ordinal, &var);
}

diameter_avp_rc_t diameter_get_diam_identity(diameter_avp_payload_handle payload_hdl,
			aaa_diam_avp_list_t avp_ordinal, uint16_t avp_index,
			diameter_avp_variant_t **avp, uint8_t **str, uint32_t *size)
{
	diameter_avp_rc_t diam_avp_ret = DIAMETER_AVP_SUCCESS;
	diameter_avp_variant_t *var;

	diam_avp_ret = diameter_avp_payload_get_avp_by_ordinal(payload_hdl,
				avp_ordinal, avp_index, &var);

	*str = var->data.a_i8.p;
	*size = var->avp_payload_len;

	if (avp)
		*avp = var;

	return diam_avp_ret;
}

int diameter_get_enum(diameter_avp_payload_handle payload_hdl,
			aaa_diam_avp_list_t avp_ordinal,
			diameter_avp_variant_t **avp,
			uint16_t avp_index, int32_t *val)
{
	int status = DIAMETER_AVP_SUCCESS;
	diameter_avp_variant_t *var;

	status = diameter_avp_payload_get_avp_by_ordinal(payload_hdl,
			avp_ordinal, avp_index, &var);
	if (status != DIAMETER_AVP_SUCCESS) {
		*val = 0;
		return status;
	}

	assert(var->vt == DIAMETER_FORMAT_UNSIGNED32 || var->vt == DIAMETER_FORMAT_ENUM);
	*val = var->data.v_i32;

	if (avp)
		*avp = var;

	return status;
}

int diameter_get_group(diameter_avp_payload_handle payload_hdl,
			aaa_diam_avp_list_t avp_ordinal, uint16_t avp_index,
			diameter_avp_variant_t **avp,
			diameter_avp_payload_handle *grp_hdl)
{
	int status = DIAMETER_AVP_SUCCESS;
	diameter_avp_variant_t *var;

	status = diameter_avp_payload_get_avp_by_ordinal(payload_hdl,
			avp_ordinal, avp_index, &var);
	if (status != DIAMETER_AVP_SUCCESS) {
		if (grp_hdl) {
			*grp_hdl = NULL;
		}
		return status;
	}

	if (grp_hdl) {
		*grp_hdl = (var->data.grouped_avp);
	}

	if (avp)
		*avp = var;

	return status;
}

static void *diameter_dict_alloc(uint32_t size)
{
	void *p;

	p = nkn_malloc_type(size, mod_diameter_t);
	return p;
}

static void diameter_dict_free(void *p, uint32_t unused __attribute__((unused)))
{
	free(p);
	return;
}

static void *diameter_dict_object_alloc(diameter_avp_object_type_t type, uint32_t size)
{
	void *p;
	unsigned int *refcount;

	p = nkn_malloc_type((sizeof(unsigned int)+size), mod_diameter_t);
	if (p == NULL)
		return NULL;

	refcount = (unsigned int *)p;
	AO_int_store(refcount, 0);

	switch (type) {
	case DIAMETER_AVP_TYPE_MESSAGE:
		AO_fetch_and_add1(&glob_dia_avp_msg);
		break;
	case DIAMETER_AVP_TYPE_PAYLOAD:
		AO_fetch_and_add1(&glob_dia_avp_payload);
		break;
	case DIAMETER_AVP_TYPE_VENDOR:
		AO_fetch_and_add1(&glob_dia_avp_vendor);
		break;
	case DIAMETER_AVP_TYPE_DICTIONARY:
		AO_fetch_and_add1(&glob_dia_avp_dictionary);
		break;
	default:
		AO_fetch_and_add1(&glob_dia_avp_unknown);
		break;
	}

	return (uint8_t *)p + sizeof(unsigned int);
}

static uint32_t diameter_dict_object_free(diameter_avp_object_type_t type, void *p)
{
	unsigned int *refcount;

	if (p == NULL)
		return DIAMETER_CB_ERROR;

	refcount = (unsigned int *)p - 1;
	free(refcount);

	switch (type) {
	case DIAMETER_AVP_TYPE_MESSAGE:
		AO_fetch_and_sub1(&glob_dia_avp_msg);
		break;
	case DIAMETER_AVP_TYPE_PAYLOAD:
		AO_fetch_and_sub1(&glob_dia_avp_payload);
		break;
	case DIAMETER_AVP_TYPE_VENDOR:
		AO_fetch_and_sub1(&glob_dia_avp_vendor);
		break;
	case DIAMETER_AVP_TYPE_DICTIONARY:
		AO_fetch_and_sub1(&glob_dia_avp_dictionary);
		break;
	default:
		AO_fetch_and_sub1(&glob_dia_avp_unknown);
		break;
	}

	return DIAMETER_CB_OK;
}

static void diameter_dict_itable_create(const char *name,
			diameter_avp_itable_parameters_t *parameters,
			diameter_avp_itable_handle *hndl)
{
	(void) name;
	(void) parameters;

	*hndl = nkn_calloc_type(1, (sizeof(void *)*20000), mod_diameter_t);

	return;
}

static void diameter_dict_itable_destroy(diameter_avp_itable_handle hndl)
{
	free(hndl);
	return;
}

static void diameter_dict_itable_insert(diameter_avp_itable_handle hndl,
			void *obj, const uint32_t *target_index)
{
	void **itable = hndl;
	uint32_t i = *target_index;

	if (i > 20000) {
		put_long(&i, *target_index);
		if (i > 20000) {
			assert(0);
		}
	}

	itable[i] = obj;

	return;
}

static void diameter_dict_itable_remove(diameter_avp_itable_handle hndl,
			const uint32_t *target_index)
{
	void **itable = hndl;
	uint32_t i = *target_index;

	if (i > 20000) {
		put_long(&i, *target_index);
		if (i > 20000) {
			assert(0);
			return;
		}
	}

	itable[i] = NULL;

	return;
}

static void *diameter_dict_itable_find(diameter_avp_itable_handle hndl,
			const uint32_t *target_index)
{
	void **itable = hndl;
	uint32_t i = *target_index;

	if (i > 20000) {
		put_long(&i, *target_index);
		if (i > 20000) {
			assert(0);
			return NULL;
		}
	}

	return itable[i];
}

void *diameter_dict_data_alloc(uint32_t size)
{
	struct iovec *iov;

	iov = nkn_malloc_type(sizeof(struct iovec), mod_diameter_t);
	if (iov == NULL)
		return NULL;

	iov->iov_base = nkn_malloc_type(size, mod_diameter_t);
	if (iov->iov_base == NULL) {
		free(iov);
	}
	iov->iov_len = 0;

	return iov;
}

uint32_t diameter_dict_data_release(diameter_avp_data_handle buffer)
{
	struct iovec *iov;
	iov = (struct iovec *)buffer;
	free(iov->iov_base);
	free(iov);

	return 0;
}

void diameter_dict_data_get_size(diameter_avp_data_handle buffer, uint32_t *size)
{
	struct iovec *iov;
	iov = (struct iovec *)buffer;
	*size = iov->iov_len;

	return;
}

void diameter_dict_data_get_ptr(diameter_avp_data_handle buffer,
			uint32_t offset, uint8_t **pdata)
{

	struct iovec *iov;
	iov = (struct iovec *)buffer;
	*pdata = (uint8_t *)iov->iov_base + offset;

	return;
}

uint32_t diameter_dict_data_copy(diameter_avp_data_handle buffer,
			uint32_t offset, const void *data, uint32_t len)
{
	struct iovec *iov;

	iov = (struct iovec *)buffer;
	if ((offset + len) > iov->iov_len) {
		iov->iov_len = offset + len;
	}
	memcpy((uint8_t *)iov->iov_base + offset, data, len);

	return iov->iov_len;
}

void diameter_dict_get_handle(char *name, diameter_avp_dict_handle *dict_hdl)
{
        int status;
        uint32_t avp_index;
        uint32_t counter = 0;
        diameter_avp_dict_handle diam_base_dic_handle;

	/* create data structures for dictionary */
        status = diameter_avp_create_diameter_dictionary(name,
                                                        &diam_base_dic_handle);
        assert(status == DIAMETER_AVP_SUCCESS);

	/* load avp information */
        for (avp_index = 0; aaa_diam_dic[avp_index].code; ++avp_index) {
                status = diameter_avp_dictionary_add_attribute(diam_base_dic_handle,
                                &aaa_diam_dic[avp_index], aaa_diam_dic[avp_index].ord_no);
                if (status != DIAMETER_AVP_SUCCESS) {
                        DBG_LOG(MSG, MOD_JPSD,
				"Error in attrib add for ordinal %u error code = %u",
				aaa_diam_dic[avp_index].ord_no, status);
                } else {
                        counter++;
                }
        }

	DBG_LOG(MSG, MOD_JPSD, "Total Avps %d", counter);

        *dict_hdl = diam_base_dic_handle;

        return;
}

void diameter_dict_init(uint32_t aaa_diam_avp_max)
{
        int status;
        diameter_avp_cb_table_t avp_func_table;

	/* register dictionary memory callback functions */
        avp_func_table.memory_cbs.cb_allocate = diameter_dict_alloc;
        avp_func_table.memory_cbs.cb_allocate_critical = diameter_dict_alloc;
        avp_func_table.memory_cbs.cb_free = diameter_dict_free;
        avp_func_table.memory_cbs.cb_free_critical = diameter_dict_free;
        avp_func_table.memory_cbs.cb_object_allocate = diameter_dict_object_alloc;
        avp_func_table.memory_cbs.cb_object_free = diameter_dict_object_free;

	/* register dictionary hash callback functions */
        avp_func_table.itable_cbs.cb_create = diameter_dict_itable_create;
        avp_func_table.itable_cbs.cb_destroy = diameter_dict_itable_destroy;
        avp_func_table.itable_cbs.cb_insert = diameter_dict_itable_insert;
        avp_func_table.itable_cbs.cb_remove = diameter_dict_itable_remove;
        avp_func_table.itable_cbs.cb_find = diameter_dict_itable_find;

	/* register dictionary data callback functions */
        avp_func_table.data_cbs.cb_get_size = diameter_dict_data_get_size;
        avp_func_table.data_cbs.cb_get_ptr = diameter_dict_data_get_ptr;
        avp_func_table.data_cbs.cb_release = diameter_dict_data_release;
        avp_func_table.data_cbs.cb_copy = diameter_dict_data_copy;

        status = diameter_avp_lib_init(&avp_func_table, aaa_diam_avp_max);
        assert(status == DIAMETER_AVP_SUCCESS);

        return;
}

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "lf_common_defs.h"
#include "lf_err.h"

int32_t
lf_app_metrics_intf_register(lf_app_metrics_intf_t **app_intf,
			     uint32_t section_id,
			     uint32_t app_id,
			     lf_app_metrics_intf_t *intf)
{
    int32_t err = 0, idx = 0;

    if (section_id >= LF_MAX_SECTIONS ||
	app_id >= LF_MAX_APPS) {
	err = -E_LF_APP_METRICS_INTF_REG;
	goto error;
    }

    idx = (section_id * LF_MAX_SECTIONS) + app_id;
    app_intf[idx] = intf;

 error:
    return err;
}

lf_app_metrics_intf_t *
lf_app_metrics_intf_query(lf_app_metrics_intf_t **app_intf,
			  uint32_t section_id, uint32_t app_id)
{
    int32_t idx = 0;
    idx = (section_id * LF_MAX_SECTIONS) + app_id;
    return app_intf[idx];
}

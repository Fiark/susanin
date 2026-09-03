#ifndef SUSANIN_TELEMETRY_H
#define SUSANIN_TELEMETRY_H

#include "config.h"
#include "routeros_api.h"

int telemetry_collect_once(
    ros_client_t *ros,
    const app_config_t *cfg
);

#endif

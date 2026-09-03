#ifndef SUSANIN_ROUTERLOG_H
#define SUSANIN_ROUTERLOG_H

#include "config.h"
#include "routeros_api.h"

int routerlog_collect_errors(
    ros_client_t *ros,
    const app_config_t *cfg
);

#endif

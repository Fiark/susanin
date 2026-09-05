#ifndef SUSANIN_TARGET_H
#define SUSANIN_TARGET_H

#include "config.h"
#include "routeros_api.h"

void target_show(
    const app_config_t *cfg
);

int target_list(
    ros_client_t *ros
);

int target_set_interface(
    ros_client_t *ros,
    app_config_t *cfg,
    const char *name
);

int target_set_routing_table(
    ros_client_t *ros,
    app_config_t *cfg,
    const char *name
);

#endif

#ifndef SUSANIN_DIRECT_H
#define SUSANIN_DIRECT_H

#include "config.h"
#include "routeros_api.h"

int direct_list_local(void);

int direct_sync(
    ros_client_t *ros,
    const app_config_t *cfg
);

int direct_add(
    ros_client_t *ros,
    const app_config_t *cfg,
    const char *kind,
    const char *value
);

int direct_remove(
    ros_client_t *ros,
    const app_config_t *cfg,
    const char *kind,
    const char *value
);

#endif

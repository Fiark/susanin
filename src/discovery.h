#ifndef AUTOAWG_DISCOVERY_H
#define AUTOAWG_DISCOVERY_H

#include "config.h"
#include "routeros_api.h"

int discovery_run(ros_client_t *ros, const app_config_t *cfg, int plan_mode);

#endif

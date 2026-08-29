#ifndef AUTOAWG_STATUS_H
#define AUTOAWG_STATUS_H

#include "config.h"
#include "routeros_api.h"

int status_run(ros_client_t *ros, const app_config_t *cfg);

#endif

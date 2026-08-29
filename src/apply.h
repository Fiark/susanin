#ifndef SUSANIN_APPLY_H
#define SUSANIN_APPLY_H

#include "config.h"
#include "routeros_api.h"

int apply_dry_run(ros_client_t *ros, const app_config_t *cfg);

#endif

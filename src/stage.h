#ifndef SUSANIN_STAGE_H
#define SUSANIN_STAGE_H

#include "config.h"
#include "routeros_api.h"

int stage_run(ros_client_t *ros, const app_config_t *cfg);
int stage_clean_run(ros_client_t *ros);

#endif

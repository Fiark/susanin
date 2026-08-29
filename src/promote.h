#ifndef SUSANIN_PROMOTE_H
#define SUSANIN_PROMOTE_H

#include "config.h"
#include "routeros_api.h"

int promote_dry_run(ros_client_t *ros, const app_config_t *cfg);
int promote_run(ros_client_t *ros, const app_config_t *cfg);
int rollback_run(ros_client_t *ros);

#endif

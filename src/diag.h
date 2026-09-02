#ifndef SUSANIN_DIAG_H
#define SUSANIN_DIAG_H

#include "config.h"

int diag_start(app_config_t *cfg);
int diag_stop(app_config_t *cfg);
int diag_status(const app_config_t *cfg);

int diag_event(
    const app_config_t *cfg,
    const char *type,
    const char *message
);

#endif

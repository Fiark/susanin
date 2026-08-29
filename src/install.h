#ifndef SUSANIN_INSTALL_H
#define SUSANIN_INSTALL_H

#include "config.h"
#include "routeros_api.h"

/*
 * Fresh-install transaction. Existing complete legacy AUTO-AWG installations
 * are treated as already installed and are left untouched. Partial managed
 * installations are blocked rather than guessed at.
 */
int install_run(ros_client_t *ros, const app_config_t *cfg, int dry_run);

#endif

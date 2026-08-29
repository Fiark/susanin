#ifndef SUSANIN_RENDERER_H
#define SUSANIN_RENDERER_H

#include "config.h"
#include "routeros_api.h"

#include <stddef.h>

#define SUSANIN_SCRIPT_COUNT 4

typedef struct {
    char name[32];
    char *source;
    size_t bytes;
    char fp[17];
} susanin_rendered_script_t;

typedef struct {
    susanin_rendered_script_t scripts[SUSANIN_SCRIPT_COUNT];
    unsigned lan_networks;
    char egress_address[64];
} susanin_render_bundle_t;

int renderer_build(ros_client_t *ros, const app_config_t *cfg, susanin_render_bundle_t *out);
void renderer_free(susanin_render_bundle_t *bundle);
int renderer_run(ros_client_t *ros, const app_config_t *cfg);

#endif

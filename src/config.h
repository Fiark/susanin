#ifndef SUSANIN_CONFIG_H
#define SUSANIN_CONFIG_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    SUSANIN_TARGET_INTERFACE = 0,
    SUSANIN_TARGET_ROUTING_TABLE = 1
} susanin_target_mode_t;

typedef enum {
    SUSANIN_ACCURACY_FAST = 0,
    SUSANIN_ACCURACY_MIDDLE = 1,
    SUSANIN_ACCURACY_SLOW = 2
} susanin_accuracy_profile_t;

typedef struct {
    const char *host;
    uint16_t port;
    const char *user;
    const char *password;

    const char *lan_list;

    /*
     * Resolved compatibility pair used by the stable v0.11.5 data plane.
     * v0.12 target abstraction is layered above these fields.
     */
    const char *egress_interface;
    const char *routing_table;

    susanin_target_mode_t target_mode;
    const char *target_value;

    susanin_accuracy_profile_t accuracy_profile;

    const char *log_level;

    char host_buf[256];
    char password_buf[1024];
    char lan_buf[128];
    char egress_buf[128];
    char table_buf[128];
    char target_value_buf[128];
    char log_level_buf[16];

    unsigned diagnostic_max_size_mb;
    unsigned diagnostic_max_files;

    int diagnostics_enabled;
    int host_configured;
    int port_configured;
    int selection_configured;
} app_config_t;

int config_load_local(app_config_t *cfg);
int config_load(app_config_t *cfg);

int config_save_selection(
    app_config_t *cfg,
    const char *egress,
    const char *table
);

int config_save_target(
    app_config_t *cfg,
    susanin_target_mode_t mode,
    const char *target_value,
    const char *resolved_egress,
    const char *resolved_table
);

int config_set_option(
    app_config_t *cfg,
    const char *key,
    const char *value
);

const char *config_target_mode_name(
    susanin_target_mode_t mode
);

int config_parse_target_mode(
    const char *value,
    susanin_target_mode_t *out
);

const char *config_accuracy_profile_name(
    susanin_accuracy_profile_t profile
);

int config_parse_accuracy_profile(
    const char *value,
    susanin_accuracy_profile_t *out
);

void config_print_safe(const app_config_t *cfg);
void config_print_settings(const app_config_t *cfg);

#endif

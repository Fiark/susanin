#ifndef SUSANIN_CONFIG_H
#define SUSANIN_CONFIG_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *host;
    uint16_t port;
    const char *user;
    const char *password;
    const char *lan_list;
    const char *egress_interface;
    const char *routing_table;
    const char *log_level;

    char host_buf[256];
    char password_buf[1024];
    char lan_buf[128];
    char egress_buf[128];
    char table_buf[128];
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
int config_save_selection(app_config_t *cfg, const char *egress, const char *table);
int config_set_option(app_config_t *cfg, const char *key, const char *value);
void config_print_safe(const app_config_t *cfg);
void config_print_settings(const app_config_t *cfg);

#endif

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
    char host_buf[256];
    char password_buf[1024];
    char lan_buf[128];
    char egress_buf[128];
    char table_buf[128];
    int selection_configured;
} app_config_t;

int config_load(app_config_t *cfg);
int config_save_selection(app_config_t *cfg, const char *egress, const char *table);
void config_print_safe(const app_config_t *cfg);

#endif

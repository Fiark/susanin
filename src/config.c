#include "config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUSANIN_SECRET_PASSWORD "/run/secrets/routeros_password"
#define SUSANIN_CONFIG_FILE "/data/susanin.conf"
#define SUSANIN_AGENT_USER "susanin-agent"

static int read_text_file(const char *path, char *buf, size_t buflen) {
    if (!path || !buf || buflen < 2) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, buflen - 1, f);
    int e = ferror(f);
    fclose(f);
    if (e || n == 0) return -1;
    buf[n] = '\0';
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ' || buf[n - 1] == '\t')) buf[--n] = '\0';
    return n ? 0 : -1;
}

static int infer_default_gateway(char out[256]) {
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) return -1;
    char line[512];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    while (fgets(line, sizeof(line), f)) {
        char iface[64];
        unsigned long dest = 0, gw = 0, flags = 0;
        if (sscanf(line, "%63s %lx %lx %lx", iface, &dest, &gw, &flags) != 4) continue;
        if (dest != 0 || !(flags & 0x2)) continue;
        struct in_addr a;
        a.s_addr = (uint32_t)gw;
        if (!inet_ntop(AF_INET, &a, out, 256)) { fclose(f); return -1; }
        fclose(f);
        return 0;
    }
    fclose(f);
    return -1;
}

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (*p == ' ' || *p == '\t') ++p;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
}

static void load_nonsecret_config(app_config_t *cfg) {
    snprintf(cfg->lan_buf, sizeof(cfg->lan_buf), "LAN");
    FILE *f = fopen(SUSANIN_CONFIG_FILE, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (!*line || line[0] == '#') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq++ = '\0';
        trim(line); trim(eq);
        if (strcmp(line, "lan_list") == 0 && *eq) snprintf(cfg->lan_buf, sizeof(cfg->lan_buf), "%s", eq);
        else if (strcmp(line, "egress_interface") == 0 && *eq) snprintf(cfg->egress_buf, sizeof(cfg->egress_buf), "%s", eq);
        else if (strcmp(line, "routing_table") == 0 && *eq) snprintf(cfg->table_buf, sizeof(cfg->table_buf), "%s", eq);
        else if (strcmp(line, "router_host") == 0 && *eq) snprintf(cfg->host_buf, sizeof(cfg->host_buf), "%s", eq);
        else if (strcmp(line, "router_port") == 0 && *eq) {
            errno = 0; char *end = NULL; long p = strtol(eq, &end, 10);
            if (!errno && end && !*end && p >= 1 && p <= 65535) cfg->port = (uint16_t)p;
        }
    }
    fclose(f);
}

int config_load(app_config_t *cfg) {
    if (!cfg) return -1;
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = 8728;
    load_nonsecret_config(cfg);

    if (!cfg->host_buf[0] && infer_default_gateway(cfg->host_buf) < 0) {
        snprintf(cfg->host_buf, sizeof(cfg->host_buf), "172.31.254.1");
    }
    if (read_text_file(SUSANIN_SECRET_PASSWORD, cfg->password_buf, sizeof(cfg->password_buf)) < 0) {
        fprintf(stderr, "Susanin bootstrap secret is missing: %s\n", SUSANIN_SECRET_PASSWORD);
        fprintf(stderr, "Install Susanin with the RouterOS bootstrap script; no user-entered credentials are required.\n");
        return -1;
    }

    cfg->host = cfg->host_buf;
    cfg->user = SUSANIN_AGENT_USER;
    cfg->password = cfg->password_buf;
    cfg->lan_list = cfg->lan_buf[0] ? cfg->lan_buf : "LAN";
    cfg->egress_interface = cfg->egress_buf[0] ? cfg->egress_buf : NULL;
    cfg->routing_table = cfg->table_buf[0] ? cfg->table_buf : NULL;
    cfg->selection_configured = cfg->egress_interface && cfg->routing_table;
    return 0;
}

int config_save_selection(app_config_t *cfg, const char *egress, const char *table) {
    if (!cfg || !egress || !*egress || !table || !*table) return -1;
    FILE *f = fopen(SUSANIN_CONFIG_FILE ".tmp", "w");
    if (!f) { perror("open Susanin config"); return -1; }
    fprintf(f, "# Susanin non-secret runtime selection\n");
    fprintf(f, "lan_list=%s\n", cfg->lan_list ? cfg->lan_list : "LAN");
    fprintf(f, "egress_interface=%s\n", egress);
    fprintf(f, "routing_table=%s\n", table);
    if (fclose(f) != 0) return -1;
    if (rename(SUSANIN_CONFIG_FILE ".tmp", SUSANIN_CONFIG_FILE) != 0) { perror("rename Susanin config"); return -1; }
    snprintf(cfg->egress_buf, sizeof(cfg->egress_buf), "%s", egress);
    snprintf(cfg->table_buf, sizeof(cfg->table_buf), "%s", table);
    cfg->egress_interface = cfg->egress_buf;
    cfg->routing_table = cfg->table_buf;
    cfg->selection_configured = 1;
    return 0;
}

void config_print_safe(const app_config_t *cfg) {
    printf("Project: Susanin\n");
    printf("Router: %s:%u\n", cfg->host, (unsigned)cfg->port);
    printf("Auth: auto-provisioned local agent\n");
    printf("LAN interface-list: %s\n", cfg->lan_list);
    printf("Egress interface: %s\n", cfg->egress_interface ? cfg->egress_interface : "<not selected>");
    printf("Routing table: %s\n", cfg->routing_table ? cfg->routing_table : "<auto-detect after selection>");
}

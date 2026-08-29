#include "renderer.h"
#include "fingerprint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LAN_IFS 64
#define MAX_LAN_NETS 128

typedef struct {
    char iface[128];
} lan_if_t;

typedef struct {
    char network[64];
    unsigned prefix;
    char mask[32];
} lan_net_t;

typedef struct {
    const app_config_t *cfg;
    lan_if_t ifs[MAX_LAN_IFS];
    size_t if_count;
    lan_net_t nets[MAX_LAN_NETS];
    size_t net_count;
    char egress_address[64];
} render_ctx_t;

static int has_if(const render_ctx_t *ctx, const char *name) {
    for (size_t i = 0; i < ctx->if_count; ++i) {
        if (strcmp(ctx->ifs[i].iface, name) == 0) return 1;
    }
    return 0;
}

static int member_cb(const ros_sentence_t *s, void *opaque) {
    render_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *list = ros_get_attr(s, "list");
    const char *iface = ros_get_attr(s, "interface");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!list || strcmp(list, ctx->cfg->lan_list) != 0) return 0;
    if (disabled && (strcmp(disabled, "true") == 0 || strcmp(disabled, "yes") == 0)) return 0;
    if (!iface || !*iface || ctx->if_count >= MAX_LAN_IFS || has_if(ctx, iface)) return 0;
    snprintf(ctx->ifs[ctx->if_count++].iface, sizeof(ctx->ifs[0].iface), "%s", iface);
    return 0;
}

static void ipv4_mask(unsigned prefix, char out[32]) {
    unsigned m = prefix == 0 ? 0u : 0xffffffffu << (32u - prefix);
    snprintf(out, 32, "%u.%u.%u.%u",
             (m >> 24) & 255u, (m >> 16) & 255u, (m >> 8) & 255u, m & 255u);
}

static int addr_cb(const ros_sentence_t *s, void *opaque) {
    render_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *iface = ros_get_attr(s, "interface");
    const char *address = ros_get_attr(s, "address");
    const char *network = ros_get_attr(s, "network");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!iface || !address || !network) return 0;
    if (disabled && (strcmp(disabled, "true") == 0 || strcmp(disabled, "yes") == 0)) return 0;

    if (ctx->cfg->egress_interface && strcmp(iface, ctx->cfg->egress_interface) == 0 && !ctx->egress_address[0]) {
        snprintf(ctx->egress_address, sizeof(ctx->egress_address), "%s", address);
    }

    if (!has_if(ctx, iface) || ctx->net_count >= MAX_LAN_NETS) return 0;
    const char *slash = strchr(address, '/');
    if (!slash) return 0;
    char *end = NULL;
    unsigned long p = strtoul(slash + 1, &end, 10);
    if (!end || *end || p > 32) return 0;

    lan_net_t *n = &ctx->nets[ctx->net_count++];
    snprintf(n->network, sizeof(n->network), "%s", network);
    n->prefix = (unsigned)p;
    ipv4_mask(n->prefix, n->mask);
    return 0;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *buf = calloc((size_t)n + 1, 1);
    if (!buf) { fclose(f); return NULL; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    return buf;
}

static char *replace_all(const char *src, const char *needle, const char *value) {
    if (!src || !needle || !*needle || !value) return NULL;
    size_t count = 0, nl = strlen(needle), vl = strlen(value);
    const char *p = src;
    while ((p = strstr(p, needle)) != NULL) { count++; p += nl; }
    size_t outlen = strlen(src) + count * (vl - nl);
    char *out = malloc(outlen + 1);
    if (!out) return NULL;
    char *w = out;
    p = src;
    const char *q;
    while ((q = strstr(p, needle)) != NULL) {
        size_t chunk = (size_t)(q - p);
        memcpy(w, p, chunk); w += chunk;
        memcpy(w, value, vl); w += vl;
        p = q + nl;
    }
    strcpy(w, p);
    return out;
}

static char *lan_match_snippet(const render_ctx_t *ctx) {
    size_t cap = 256 + ctx->net_count * 128;
    char *buf = calloc(cap, 1);
    if (!buf) return NULL;
    size_t used = 0;
    used += (size_t)snprintf(buf + used, cap - used, ":local fromLAN false\n");
    if (ctx->net_count == 0) return buf;
    used += (size_t)snprintf(buf + used, cap - used, ":if (");
    for (size_t i = 0; i < ctx->net_count; ++i) {
        if (i) used += (size_t)snprintf(buf + used, cap - used, " || ");
        used += (size_t)snprintf(buf + used, cap - used,
                                 "(($src & %s) = %s)", ctx->nets[i].mask, ctx->nets[i].network);
    }
    (void)snprintf(buf + used, cap - used, ") do={ :set fromLAN true }\n");
    return buf;
}

static char *load_template(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "/usr/share/susanin/templates/%s", name);
    char *s = read_file(path);
    if (s) return s;
    snprintf(path, sizeof(path), "templates/%s", name);
    return read_file(path);
}

static int render_one(const char *tmpl_name, const char *script_name,
                      const char *lan_match, const app_config_t *cfg,
                      susanin_rendered_script_t *out) {
    char *tmpl = load_template(tmpl_name);
    if (!tmpl) {
        fprintf(stderr, "Cannot load Susanin template: %s\n", tmpl_name);
        return -1;
    }
    char *a = replace_all(tmpl, "{{LAN_MATCH}}", lan_match);
    free(tmpl);
    if (!a) return -1;
    char *b = replace_all(a, "{{EGRESS_INTERFACE}}", cfg->egress_interface ? cfg->egress_interface : "");
    free(a);
    if (!b) return -1;
    char *c = replace_all(b, "{{LAN_LIST}}", cfg->lan_list ? cfg->lan_list : "LAN");
    free(b);
    if (!c) return -1;
    char *d = replace_all(c, "{{ROUTING_TABLE}}", cfg->routing_table ? cfg->routing_table : "");
    free(c);
    if (!d) return -1;

    snprintf(out->name, sizeof(out->name), "%s", script_name);
    out->source = d;
    out->bytes = strlen(d);
    susanin_fingerprint_hex(d, out->fp);
    return 0;
}

int renderer_build(ros_client_t *ros, const app_config_t *cfg, susanin_render_bundle_t *out) {
    if (!ros || !cfg || !out || !cfg->egress_interface || !cfg->routing_table) return -1;
    memset(out, 0, sizeof(*out));
    render_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg = cfg;

    /* Do not rely on a RouterOS API query filter for interface-list members.
       On RouterOS 7.23.3 a filtered request with a narrow .proplist can yield
       !re records that are sufficient for a count-only check but omit fields
       the renderer needs. Read list+interface explicitly and filter locally. */
    const char *members[] = {"/interface/list/member/print", "=.proplist=list,interface,disabled"};
    if (ros_command(ros, members, 2, member_cb, &ctx) < 0) return -1;
    const char *addrs[] = {"/ip/address/print", "=.proplist=address,network,interface,disabled"};
    if (ros_command(ros, addrs, 2, addr_cb, &ctx) < 0) return -1;

    if (ctx.if_count == 0) {
        fprintf(stderr, "Susanin render: no usable interfaces found in interface-list '%s'\n", cfg->lan_list);
        return -1;
    }
    if (ctx.net_count == 0) {
        fprintf(stderr, "Susanin render: %zu LAN interface(s) found in '%s', but none has an enabled IPv4 address\n",
                ctx.if_count, cfg->lan_list);
        return -1;
    }
    if (!ctx.egress_address[0]) {
        fprintf(stderr, "Susanin render: egress '%s' has no IPv4 address; the health template requires one\n",
                cfg->egress_interface);
        return -1;
    }

    char *match = lan_match_snippet(&ctx);
    if (!match) return -1;

    static const char *tn[SUSANIN_SCRIPT_COUNT] = {"health.rsc.tmpl", "fast.rsc.tmpl", "soft.rsc.tmpl", "judge.rsc.tmpl"};
    static const char *sn[SUSANIN_SCRIPT_COUNT] = {"auto-awg-health", "auto-awg-fast", "auto-awg-detect", "auto-awg-judge"};
    for (size_t i = 0; i < SUSANIN_SCRIPT_COUNT; ++i) {
        if (render_one(tn[i], sn[i], match, cfg, &out->scripts[i]) < 0) {
            free(match);
            renderer_free(out);
            return -1;
        }
    }
    free(match);
    out->lan_networks = (unsigned)ctx.net_count;
    snprintf(out->egress_address, sizeof(out->egress_address), "%s", ctx.egress_address);
    return 0;
}

void renderer_free(susanin_render_bundle_t *bundle) {
    if (!bundle) return;
    for (size_t i = 0; i < SUSANIN_SCRIPT_COUNT; ++i) {
        free(bundle->scripts[i].source);
        bundle->scripts[i].source = NULL;
    }
}

int renderer_run(ros_client_t *ros, const app_config_t *cfg) {
    susanin_render_bundle_t b;
    if (renderer_build(ros, cfg, &b) < 0) return -1;
    printf("=== SUSANIN RENDER v0.11.3 ===\n");
    printf("Mode: generated desired RouterOS data-plane; no changes\n");
    printf("LAN interface-list: %s\n", cfg->lan_list);
    printf("LAN IPv4 networks discovered: %u\n", b.lan_networks);
    printf("Egress: %s address=%s\n", cfg->egress_interface, b.egress_address);
    printf("Routing table: %s\n\n", cfg->routing_table);
    printf("Desired script fingerprints:\n");
    for (size_t i = 0; i < SUSANIN_SCRIPT_COUNT; ++i) {
        printf("  %-18s bytes=%-6zu fnv1a64=%s\n", b.scripts[i].name, b.scripts[i].bytes, b.scripts[i].fp);
    }
    printf("\nGenerator state: READY (read-only)\n");
    renderer_free(&b);
    return 0;
}

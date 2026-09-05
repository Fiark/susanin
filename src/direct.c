#include "direct.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIRECT_FILE "/data/vpn_direct.conf"
#define DIRECT_LIST "vpn_direct"
#define DIRECT_MAX 256
#define DIRECT_IDS_MAX 512

typedef enum {
    DIRECT_IP = 1,
    DIRECT_DOMAIN = 2
} direct_kind_t;

typedef struct {
    direct_kind_t kind;
    char value[256];
} direct_entry_t;

typedef struct {
    direct_entry_t entries[DIRECT_MAX];
    size_t count;
} direct_db_t;

static void trim(
    char *s
) {
    if (!s) return;

    char *p = s;

    while (
        *p == ' ' ||
        *p == '\t'
    ) {
        ++p;
    }

    if (p != s) {
        memmove(
            s,
            p,
            strlen(p) + 1
        );
    }

    size_t n = strlen(s);

    while (
        n &&
        (
            s[n - 1] == ' ' ||
            s[n - 1] == '\t' ||
            s[n - 1] == '\r' ||
            s[n - 1] == '\n'
        )
    ) {
        s[--n] = '\0';
    }
}

static const char *kind_name(
    direct_kind_t kind
) {
    return
        kind == DIRECT_DOMAIN
            ? "domain"
            : "ip";
}

static int parse_kind(
    const char *s,
    direct_kind_t *out
) {
    if (!s || !out) return -1;

    if (
        strcmp(s, "ip") == 0 ||
        strcmp(s, "ipv4") == 0
    ) {
        *out = DIRECT_IP;
        return 0;
    }

    if (
        strcmp(s, "domain") == 0 ||
        strcmp(s, "dns") == 0
    ) {
        *out = DIRECT_DOMAIN;
        return 0;
    }

    return -1;
}

static int normalize_ip(
    const char *input,
    char out[256]
) {
    if (
        !input ||
        !*input ||
        !out
    ) {
        return -1;
    }

    char buf[256];

    snprintf(
        buf,
        sizeof(buf),
        "%s",
        input
    );

    char *slash = strchr(
        buf,
        '/'
    );

    unsigned prefix = 32;

    if (slash) {
        *slash++ = '\0';

        if (!*slash) return -1;

        char *end = NULL;
        unsigned long p = strtoul(
            slash,
            &end,
            10
        );

        if (
            !end ||
            *end ||
            p > 32
        ) {
            return -1;
        }

        prefix = (unsigned)p;
    }

    struct in_addr a;

    if (
        inet_pton(
            AF_INET,
            buf,
            &a
        ) != 1
    ) {
        return -1;
    }

    char canon[INET_ADDRSTRLEN];

    if (
        !inet_ntop(
            AF_INET,
            &a,
            canon,
            sizeof(canon)
        )
    ) {
        return -1;
    }

    snprintf(
        out,
        256,
        "%s/%u",
        canon,
        prefix
    );

    return 0;
}

static int valid_domain_label(
    const char *p,
    size_t n
) {
    if (
        !p ||
        n == 0 ||
        n > 63
    ) {
        return 0;
    }

    if (
        p[0] == '-' ||
        p[n - 1] == '-'
    ) {
        return 0;
    }

    for (
        size_t i = 0;
        i < n;
        ++i
    ) {
        unsigned char c =
            (unsigned char)p[i];

        if (
            !isalnum(c) &&
            c != '-'
        ) {
            return 0;
        }
    }

    return 1;
}

static int normalize_domain(
    const char *input,
    char out[256]
) {
    if (
        !input ||
        !*input ||
        !out
    ) {
        return -1;
    }

    size_t n = strlen(input);

    if (
        n == 0 ||
        n >= 254
    ) {
        return -1;
    }

    char buf[256];

    snprintf(
        buf,
        sizeof(buf),
        "%s",
        input
    );

    n = strlen(buf);

    while (
        n &&
        buf[n - 1] == '.'
    ) {
        buf[--n] = '\0';
    }

    if (
        n == 0 ||
        !strchr(buf, '.')
    ) {
        return -1;
    }

    for (
        size_t i = 0;
        i < n;
        ++i
    ) {
        buf[i] = (char)tolower(
            (unsigned char)buf[i]
        );
    }

    const char *p = buf;

    while (*p) {
        const char *dot = strchr(
            p,
            '.'
        );

        size_t len =
            dot
                ? (size_t)(dot - p)
                : strlen(p);

        if (
            !valid_domain_label(
                p,
                len
            )
        ) {
            return -1;
        }

        if (!dot) break;

        p = dot + 1;
    }

    snprintf(
        out,
        256,
        "%s",
        buf
    );

    return 0;
}

static int normalize_value(
    direct_kind_t kind,
    const char *input,
    char out[256]
) {
    if (kind == DIRECT_IP) {
        return normalize_ip(
            input,
            out
        );
    }

    if (kind == DIRECT_DOMAIN) {
        return normalize_domain(
            input,
            out
        );
    }

    return -1;
}

static int db_load(
    direct_db_t *db
) {
    if (!db) return -1;

    memset(
        db,
        0,
        sizeof(*db)
    );

    FILE *f = fopen(
        DIRECT_FILE,
        "r"
    );

    if (!f) {
        return 0;
    }

    char line[512];

    while (
        fgets(
            line,
            sizeof(line),
            f
        )
    ) {
        trim(line);

        if (
            !*line ||
            line[0] == '#'
        ) {
            continue;
        }

        char *sep = strchr(
            line,
            '\t'
        );

        if (!sep) {
            sep = strchr(
                line,
                ' '
            );
        }

        if (!sep) continue;

        *sep++ = '\0';

        trim(line);
        trim(sep);

        direct_kind_t kind;

        if (
            parse_kind(
                line,
                &kind
            ) < 0
        ) {
            continue;
        }

        if (
            db->count >= DIRECT_MAX
        ) {
            fclose(f);
            return -1;
        }

        char normalized[256];

        if (
            normalize_value(
                kind,
                sep,
                normalized
            ) < 0
        ) {
            continue;
        }

        direct_entry_t *e =
            &db->entries[db->count++];

        e->kind = kind;

        snprintf(
            e->value,
            sizeof(e->value),
            "%s",
            normalized
        );
    }

    fclose(f);
    return 0;
}

static int db_save(
    const direct_db_t *db
) {
    if (!db) return -1;

    FILE *f = fopen(
        DIRECT_FILE ".tmp",
        "w"
    );

    if (!f) {
        perror("open VPN Direct database");
        return -1;
    }

    fprintf(
        f,
        "# Susanin VPN Direct persistent policy\n"
        "# Format: ip|domain<TAB>value\n"
    );

    for (
        size_t i = 0;
        i < db->count;
        ++i
    ) {
        fprintf(
            f,
            "%s\t%s\n",
            kind_name(
                db->entries[i].kind
            ),
            db->entries[i].value
        );
    }

    if (fclose(f) != 0) {
        perror("close VPN Direct database");

        remove(
            DIRECT_FILE ".tmp"
        );

        return -1;
    }

    if (
        rename(
            DIRECT_FILE ".tmp",
            DIRECT_FILE
        ) != 0
    ) {
        perror("rename VPN Direct database");

        remove(
            DIRECT_FILE ".tmp"
        );

        return -1;
    }

    return 0;
}

static int db_find(
    const direct_db_t *db,
    direct_kind_t kind,
    const char *value
) {
    if (
        !db ||
        !value
    ) {
        return -1;
    }

    for (
        size_t i = 0;
        i < db->count;
        ++i
    ) {
        if (
            db->entries[i].kind == kind &&
            strcmp(
                db->entries[i].value,
                value
            ) == 0
        ) {
            return (int)i;
        }
    }

    return -1;
}

typedef enum {
    COLLECT_LIST,
    COLLECT_COMMENT_PREFIX,
    COLLECT_COMMENT_EXACT
} collect_mode_t;

typedef struct {
    collect_mode_t mode;
    const char *match;
    char ids[DIRECT_IDS_MAX][64];
    size_t count;
} id_collect_t;

static int id_collect_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    id_collect_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *id =
        ros_get_attr(
            s,
            ".id"
        );

    if (
        !id ||
        !*id
    ) {
        return 0;
    }

    int matched = 0;

    if (
        ctx->mode ==
            COLLECT_LIST
    ) {
        const char *list =
            ros_get_attr(
                s,
                "list"
            );

        matched =
            list &&
            strcmp(
                list,
                ctx->match
            ) == 0;

    } else {
        const char *comment =
            ros_get_attr(
                s,
                "comment"
            );

        if (comment) {
            if (
                ctx->mode ==
                    COLLECT_COMMENT_EXACT
            ) {
                matched =
                    strcmp(
                        comment,
                        ctx->match
                    ) == 0;
            } else {
                matched =
                    strncmp(
                        comment,
                        ctx->match,
                        strlen(ctx->match)
                    ) == 0;
            }
        }
    }

    if (
        matched &&
        ctx->count < DIRECT_IDS_MAX
    ) {
        snprintf(
            ctx->ids[ctx->count++],
            sizeof(ctx->ids[0]),
            "%s",
            id
        );
    }

    return 0;
}

static int remove_matching(
    ros_client_t *ros,
    const char *print_cmd,
    const char *remove_cmd,
    const char *proplist,
    collect_mode_t mode,
    const char *match
) {
    id_collect_t ctx;

    memset(
        &ctx,
        0,
        sizeof(ctx)
    );

    ctx.mode = mode;
    ctx.match = match;

    char props[256];

    snprintf(
        props,
        sizeof(props),
        "=.proplist=%s",
        proplist
    );

    const char *printv[] = {
        print_cmd,
        props
    };

    if (
        ros_command(
            ros,
            printv,
            2,
            id_collect_cb,
            &ctx
        ) < 0
    ) {
        return -1;
    }

    for (
        size_t i = 0;
        i < ctx.count;
        ++i
    ) {
        char wid[96];

        snprintf(
            wid,
            sizeof(wid),
            "=.id=%s",
            ctx.ids[i]
        );

        const char *removev[] = {
            remove_cmd,
            wid
        };

        /*
         * Dynamic DNS-generated address-list entries may disappear
         * between print/remove. Treat that race as harmless.
         */
        (void)ros_command(
            ros,
            removev,
            2,
            NULL,
            NULL
        );
    }

    return 0;
}

typedef struct {
    char id[64];
    int found;
} first_auto_t;

static int first_auto_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    first_auto_t *ctx = opaque;

    if (
        !ctx ||
        ctx->found ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *comment =
        ros_get_attr(
            s,
            "comment"
        );

    const char *id =
        ros_get_attr(
            s,
            ".id"
        );

    if (
        comment &&
        id &&
        strncmp(
            comment,
            "AUTO-AWG:",
            9
        ) == 0
    ) {
        ctx->found = 1;

        snprintf(
            ctx->id,
            sizeof(ctx->id),
            "%s",
            id
        );
    }

    return 0;
}

static int add_bypass_rule(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    first_auto_t first;

    memset(
        &first,
        0,
        sizeof(first)
    );

    const char *scan[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=.id,comment"
    };

    if (
        ros_command(
            ros,
            scan,
            2,
            first_auto_cb,
            &first
        ) < 0
    ) {
        return -1;
    }

    char wlan[192];

    snprintf(
        wlan,
        sizeof(wlan),
        "=in-interface-list=%s",
        cfg->lan_list
            ? cfg->lan_list
            : "LAN"
    );

    char place[96];

    const char *cmd[12];
    size_t n = 0;

    cmd[n++] =
        "/ip/firewall/mangle/add";

    cmd[n++] =
        "=chain=prerouting";

    cmd[n++] =
        "=action=accept";

    cmd[n++] = wlan;

    cmd[n++] =
        "=dst-address-list="
        DIRECT_LIST;

    cmd[n++] =
        "=comment=SUSANIN: VPN Direct bypass";

    cmd[n++] =
        "=disabled=no";

    if (first.found) {
        snprintf(
            place,
            sizeof(place),
            "=place-before=%s",
            first.id
        );

        cmd[n++] = place;
    }

    return ros_command(
        ros,
        cmd,
        n,
        NULL,
        NULL
    );
}

static int add_ip_entry(
    ros_client_t *ros,
    const char *value
) {
    char waddr[320];
    char wcomment[384];

    snprintf(
        waddr,
        sizeof(waddr),
        "=address=%s",
        value
    );

    snprintf(
        wcomment,
        sizeof(wcomment),
        "=comment=SUSANIN: VPN Direct IP %s",
        value
    );

    const char *cmd[] = {
        "/ip/firewall/address-list/add",
        "=list=" DIRECT_LIST,
        waddr,
        wcomment
    };

    return ros_command(
        ros,
        cmd,
        4,
        NULL,
        NULL
    );
}

static int add_domain_entry(
    ros_client_t *ros,
    const char *value
) {
    char wname[320];
    char wcomment[384];

    snprintf(
        wname,
        sizeof(wname),
        "=name=%s",
        value
    );

    snprintf(
        wcomment,
        sizeof(wcomment),
        "=comment=SUSANIN: VPN Direct domain %s",
        value
    );

    const char *cmd[] = {
        "/ip/dns/static/add",
        wname,
        "=type=FWD",
        "=match-subdomain=yes",
        "=address-list=" DIRECT_LIST,
        "=disabled=no",
        wcomment
    };

    return ros_command(
        ros,
        cmd,
        7,
        NULL,
        NULL
    );
}

int direct_sync(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    if (
        !ros ||
        !cfg
    ) {
        return -1;
    }

    direct_db_t db;

    if (
        db_load(
            &db
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: cannot load persistent database.\n"
        );

        return -1;
    }

    if (
        remove_matching(
            ros,
            "/ip/firewall/address-list/print",
            "/ip/firewall/address-list/remove",
            ".id,list",
            COLLECT_LIST,
            DIRECT_LIST
        ) < 0
    ) {
        return -1;
    }

    if (
        remove_matching(
            ros,
            "/ip/dns/static/print",
            "/ip/dns/static/remove",
            ".id,comment",
            COLLECT_COMMENT_PREFIX,
            "SUSANIN: VPN Direct domain "
        ) < 0
    ) {
        return -1;
    }

    if (
        remove_matching(
            ros,
            "/ip/firewall/mangle/print",
            "/ip/firewall/mangle/remove",
            ".id,comment",
            COLLECT_COMMENT_EXACT,
            "SUSANIN: VPN Direct bypass"
        ) < 0
    ) {
        return -1;
    }

    if (db.count == 0) {
        printf(
            "VPN Direct sync: empty policy; "
            "RouterOS bypass objects removed.\n"
        );

        return 0;
    }

    if (
        add_bypass_rule(
            ros,
            cfg
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: could not install bypass rule.\n"
        );

        return -1;
    }

    unsigned ip_count = 0;
    unsigned domain_count = 0;

    for (
        size_t i = 0;
        i < db.count;
        ++i
    ) {
        if (
            db.entries[i].kind ==
                DIRECT_IP
        ) {
            if (
                add_ip_entry(
                    ros,
                    db.entries[i].value
                ) < 0
            ) {
                return -1;
            }

            ip_count++;
        } else {
            if (
                add_domain_entry(
                    ros,
                    db.entries[i].value
                ) < 0
            ) {
                return -1;
            }

            domain_count++;
        }
    }

    if (domain_count > 0) {
        const char *flush[] = {
            "/ip/dns/cache/flush"
        };

        (void)ros_command(
            ros,
            flush,
            1,
            NULL,
            NULL
        );
    }

    printf(
        "VPN Direct sync: "
        "IP/CIDR=%u domains=%u total=%zu\n",
        ip_count,
        domain_count,
        db.count
    );

    return 0;
}

int direct_list_local(void) {
    direct_db_t db;

    if (
        db_load(
            &db
        ) < 0
    ) {
        return -1;
    }

    printf(
        "=== SUSANIN VPN DIRECT ===\n\n"
    );

    if (db.count == 0) {
        printf(
            "Policy is empty.\n"
        );

        return 0;
    }

    for (
        size_t i = 0;
        i < db.count;
        ++i
    ) {
        printf(
            "  %-7s %s\n",
            kind_name(
                db.entries[i].kind
            ),
            db.entries[i].value
        );
    }

    printf(
        "\nEntries: %zu\n",
        db.count
    );

    return 0;
}

int direct_add(
    ros_client_t *ros,
    const app_config_t *cfg,
    const char *kind_s,
    const char *value_s
) {
    direct_kind_t kind;

    if (
        parse_kind(
            kind_s,
            &kind
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: type must be ip or domain.\n"
        );

        return -1;
    }

    char value[256];

    if (
        normalize_value(
            kind,
            value_s,
            value
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: invalid %s value: %s\n",
            kind_name(kind),
            value_s
        );

        return -1;
    }

    direct_db_t db;

    if (
        db_load(
            &db
        ) < 0
    ) {
        return -1;
    }

    if (
        db_find(
            &db,
            kind,
            value
        ) >= 0
    ) {
        printf(
            "VPN Direct: already present: %s %s\n",
            kind_name(kind),
            value
        );

        return direct_sync(
            ros,
            cfg
        );
    }

    if (
        db.count >= DIRECT_MAX
    ) {
        fprintf(
            stderr,
            "VPN Direct: maximum %d entries reached.\n",
            DIRECT_MAX
        );

        return -1;
    }

    direct_entry_t *e =
        &db.entries[db.count++];

    e->kind = kind;

    snprintf(
        e->value,
        sizeof(e->value),
        "%s",
        value
    );

    if (
        db_save(
            &db
        ) < 0
    ) {
        return -1;
    }

    if (
        direct_sync(
            ros,
            cfg
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: persistent policy saved, "
            "but RouterOS sync failed.\n"
        );

        return -1;
    }

    printf(
        "VPN Direct added: %s %s\n",
        kind_name(kind),
        value
    );

    return 0;
}

int direct_remove(
    ros_client_t *ros,
    const app_config_t *cfg,
    const char *kind_s,
    const char *value_s
) {
    direct_kind_t kind;

    if (
        parse_kind(
            kind_s,
            &kind
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: type must be ip or domain.\n"
        );

        return -1;
    }

    char value[256];

    if (
        normalize_value(
            kind,
            value_s,
            value
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: invalid %s value: %s\n",
            kind_name(kind),
            value_s
        );

        return -1;
    }

    direct_db_t db;

    if (
        db_load(
            &db
        ) < 0
    ) {
        return -1;
    }

    int idx = db_find(
        &db,
        kind,
        value
    );

    if (idx < 0) {
        fprintf(
            stderr,
            "VPN Direct: entry not found: %s %s\n",
            kind_name(kind),
            value
        );

        return -1;
    }

    for (
        size_t i = (size_t)idx;
        i + 1 < db.count;
        ++i
    ) {
        db.entries[i] =
            db.entries[i + 1];
    }

    db.count--;

    if (
        db_save(
            &db
        ) < 0
    ) {
        return -1;
    }

    if (
        direct_sync(
            ros,
            cfg
        ) < 0
    ) {
        fprintf(
            stderr,
            "VPN Direct: persistent removal saved, "
            "but RouterOS sync failed.\n"
        );

        return -1;
    }

    printf(
        "VPN Direct removed: %s %s\n",
        kind_name(kind),
        value
    );

    return 0;
}

#include "apply.h"
#include "config.h"
#include "diag.h"
#include "discovery.h"
#include "routeros_api.h"
#include "status.h"
#include "snapshot.h"
#include "renderer.h"
#include "validate.h"
#include "stage.h"
#include "promote.h"
#include "setup.h"
#include "install.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SUSANIN_VERSION "0.11.4-dev"

static void usage(const char *argv0) {
    fprintf(stderr,
        "Susanin %s - adaptive RouterOS tunnel routing controller\n"
        "Usage:\n"
        "  %s discover\n"
        "  %s plan\n"
        "  %s status\n"
        "  %s apply --dry-run\n"
        "  %s snapshot\n"
        "  %s render\n"
        "  %s validate\n"
        "  %s stage\n"
        "  %s stage-clean\n"
        "  %s promote --dry-run\n"
        "  %s promote\n"
        "  %s rollback\n"
        "  %s setup\n"
        "  %s install --dry-run\n"
        "  %s install\n"
        "  %s daemon\n"
        "  %s version\n\n"
        "Runtime configuration:\n"
        "  - RouterOS host is auto-detected from the container default gateway.\n"
        "  - RouterOS agent user is internal: susanin-agent.\n"
        "  - Password is read only from /run/secrets/routeros_password.\n"
        "  - VPN selection is stored in /data/susanin.conf.\n"
        "  - No environment file is used.\n",
        SUSANIN_VERSION, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
    fprintf(
        stderr,
        "\nLocal settings and diagnostics:\n"
        "  %s config show\n"
        "  %s config set <key> <value>\n"
        "  %s diag status\n"
        "  %s diag start\n"
        "  %s diag stop\n",
        argv0,
        argv0,
        argv0,
        argv0,
        argv0
    );
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "version") == 0) {
        printf("Susanin %s\n", SUSANIN_VERSION);
        return 0;
    }

    int plan = 0;
    int status = 0;
    int apply_dry = 0;
    int snapshot = 0;
    int render = 0;
    int validate = 0;
    int stage = 0;
    int stage_clean = 0;
    int promote_dry = 0;
    int promote = 0;
    int rollback = 0;
    int setup = 0;
    int install_dry = 0;
    int install = 0;
    int daemon = 0;
    int config_show = 0;
    int config_set = 0;
    int diag_status_cmd = 0;
    int diag_start_cmd = 0;
    int diag_stop_cmd = 0;

    if (argc == 2 && strcmp(argv[1], "discover") == 0) {
        /* read-only */
    } else if (argc == 2 && strcmp(argv[1], "plan") == 0) {
        plan = 1;
    } else if (argc == 2 && strcmp(argv[1], "status") == 0) {
        status = 1;
    } else if (argc == 3 && strcmp(argv[1], "apply") == 0 && strcmp(argv[2], "--dry-run") == 0) {
        apply_dry = 1;
    } else if (argc == 2 && strcmp(argv[1], "snapshot") == 0) {
        snapshot = 1;
    } else if (argc == 2 && strcmp(argv[1], "render") == 0) {
        render = 1;
    } else if (argc == 2 && strcmp(argv[1], "validate") == 0) {
        validate = 1;
    } else if (argc == 2 && strcmp(argv[1], "stage") == 0) {
        stage = 1;
    } else if (argc == 2 && strcmp(argv[1], "stage-clean") == 0) {
        stage_clean = 1;
    } else if (argc == 3 && strcmp(argv[1], "promote") == 0 && strcmp(argv[2], "--dry-run") == 0) {
        promote_dry = 1;
    } else if (argc == 2 && strcmp(argv[1], "promote") == 0) {
        promote = 1;
    } else if (argc == 2 && strcmp(argv[1], "rollback") == 0) {
        rollback = 1;
    } else if (argc == 2 && strcmp(argv[1], "setup") == 0) {
        setup = 1;
    } else if (argc == 3 && strcmp(argv[1], "install") == 0 && strcmp(argv[2], "--dry-run") == 0) {
        install_dry = 1;
    } else if (argc == 2 && strcmp(argv[1], "install") == 0) {
        install = 1;
    } else if (
        argc == 3 &&
        strcmp(argv[1], "config") == 0 &&
        strcmp(argv[2], "show") == 0
    ) {
        config_show = 1;
    } else if (
        argc == 5 &&
        strcmp(argv[1], "config") == 0 &&
        strcmp(argv[2], "set") == 0
    ) {
        config_set = 1;
    } else if (
        argc == 3 &&
        strcmp(argv[1], "diag") == 0 &&
        strcmp(argv[2], "status") == 0
    ) {
        diag_status_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(argv[1], "diag") == 0 &&
        strcmp(argv[2], "start") == 0
    ) {
        diag_start_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(argv[1], "diag") == 0 &&
        strcmp(argv[2], "stop") == 0
    ) {
        diag_stop_cmd = 1;
    } else if (argc == 2 && strcmp(argv[1], "daemon") == 0) {
        daemon = 1;
    } else if (argc == 2 && strcmp(argv[1], "apply") == 0) {
        fprintf(stderr, "Susanin v%s write-mode apply is intentionally disabled. Use: apply --dry-run\n", SUSANIN_VERSION);
        return 2;
    } else {
        usage(argv[0]);
        return 2;
    }

    if (daemon) {
        printf("Susanin controller is running. Use RouterOS /container/shell with `susanin setup` for first-run setup.\n");
        fflush(stdout);
        for (;;) sleep(3600);
    }

    app_config_t cfg;

    if (
        config_show ||
        config_set ||
        diag_status_cmd ||
        diag_start_cmd ||
        diag_stop_cmd
    ) {
        if (config_load_local(&cfg) < 0) {
            return 2;
        }

        if (config_show) {
            config_print_settings(&cfg);
            return 0;
        }

        if (config_set) {
            if (
                config_set_option(
                    &cfg,
                    argv[3],
                    argv[4]
                ) < 0
            ) {
                return 2;
            }

            printf("Susanin setting saved.\n\n");
            config_print_settings(&cfg);
            return 0;
        }

        if (diag_status_cmd) {
            return diag_status(&cfg) == 0 ? 0 : 1;
        }

        if (diag_start_cmd) {
            return diag_start(&cfg) == 0 ? 0 : 1;
        }

        if (diag_stop_cmd) {
            return diag_stop(&cfg) == 0 ? 0 : 1;
        }
    }

    if (config_load(&cfg) < 0) return 2;

    config_print_safe(&cfg);
    printf("\nConnecting...\n");

    ros_client_t ros;
    if (ros_connect(&ros, cfg.host, cfg.port) < 0) {
        fprintf(stderr, "Cannot connect to RouterOS API at %s:%u\n", cfg.host, (unsigned)cfg.port);
        return 1;
    }
    if (ros_login(&ros, cfg.user, cfg.password) < 0) {
        fprintf(stderr, "RouterOS API login failed\n");
        ros_close(&ros);
        return 1;
    }

    printf("Authenticated.\n\n");
    int rc;
    if (setup) rc = setup_run(&ros, &cfg);
    else if (install_dry) rc = install_run(&ros, &cfg, 1);
    else if (install) rc = install_run(&ros, &cfg, 0);
    else if (apply_dry) rc = apply_dry_run(&ros, &cfg);
    else if (snapshot) rc = snapshot_run(&ros, &cfg);
    else if (render) rc = renderer_run(&ros, &cfg);
    else if (validate) rc = validate_run(&ros, &cfg);
    else if (stage) rc = stage_run(&ros, &cfg);
    else if (stage_clean) rc = stage_clean_run(&ros);
    else if (promote_dry) rc = promote_dry_run(&ros, &cfg);
    else if (promote) rc = promote_run(&ros, &cfg);
    else if (rollback) rc = rollback_run(&ros);
    else if (status) rc = status_run(&ros, &cfg);
    else rc = discovery_run(&ros, &cfg, plan);

    ros_close(&ros);
    return rc == 0 ? 0 : 1;
}

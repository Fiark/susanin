#include "apply.h"
#include "config.h"
#include "diag.h"
#include "direct.h"
#include "telemetry.h"
#include "routerlog.h"
#include "discovery.h"
#include "routeros_api.h"
#include "status.h"
#include "snapshot.h"
#include "renderer.h"
#include "validate.h"
#include "stage.h"
#include "promote.h"
#include "setup.h"
#include "target.h"
#include "install.h"
#include "version.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void usage(
    const char *argv0
) {
    (void)argv0;

    fprintf(
        stderr,
        "Susanin %s - adaptive RouterOS tunnel routing controller\n"
        "\n"
        "Core:\n"
        "  susanin discover\n"
        "  susanin plan\n"
        "  susanin status\n"
        "  susanin apply --dry-run\n"
        "  susanin snapshot\n"
        "  susanin render\n"
        "  susanin validate\n"
        "  susanin stage\n"
        "  susanin stage-clean\n"
        "  susanin promote --dry-run\n"
        "  susanin promote\n"
        "  susanin rollback\n"
        "  susanin setup\n"
        "  susanin install --dry-run\n"
        "  susanin install\n"
        "\n"
        "Routing target:\n"
        "  susanin target show\n"
        "  susanin target list\n"
        "  susanin target set interface <name>\n"
        "  susanin target set routing-table <name>\n"
        "\n"
        "VPN Direct:\n"
        "  susanin direct list\n"
        "  susanin direct add ip <IPv4[/prefix]>\n"
        "  susanin direct add domain <domain>\n"
        "  susanin direct remove ip <IPv4[/prefix]>\n"
        "  susanin direct remove domain <domain>\n"
        "  susanin direct sync\n"
        "\n"
        "Settings:\n"
        "  susanin config show\n"
        "  susanin config set accuracy-profile fast|middle|slow\n"
        "  susanin config set log-level quiet|error|info|debug|trace\n"
        "  susanin config set diagnostics on|off\n"
        "\n"
        "Diagnostics:\n"
        "  susanin diag status\n"
        "  susanin diag start\n"
        "  susanin diag stop\n"
        "  susanin diag sample\n"
        "  susanin diag errors\n"
        "\n"
        "Runtime:\n"
        "  susanin daemon\n"
        "  susanin version\n",
        SUSANIN_VERSION
    );
}

int main(
    int argc,
    char **argv
) {
    if (
        argc == 2 &&
        strcmp(
            argv[1],
            "version"
        ) == 0
    ) {
        printf(
            "Susanin %s\n",
            SUSANIN_VERSION
        );

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
    int diag_sample_cmd = 0;
    int diag_errors_cmd = 0;

    int target_show_cmd = 0;
    int target_list_cmd = 0;
    int target_set_interface_cmd = 0;
    int target_set_table_cmd = 0;

    int direct_list_cmd = 0;
    int direct_add_cmd = 0;
    int direct_remove_cmd = 0;
    int direct_sync_cmd = 0;

    const char *target_value = NULL;
    const char *direct_kind = NULL;
    const char *direct_value = NULL;

    if (
        argc == 2 &&
        strcmp(
            argv[1],
            "discover"
        ) == 0
    ) {
        /* read-only discovery */
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "plan"
        ) == 0
    ) {
        plan = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "status"
        ) == 0
    ) {
        status = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "apply"
        ) == 0 &&
        strcmp(
            argv[2],
            "--dry-run"
        ) == 0
    ) {
        apply_dry = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "snapshot"
        ) == 0
    ) {
        snapshot = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "render"
        ) == 0
    ) {
        render = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "validate"
        ) == 0
    ) {
        validate = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "stage"
        ) == 0
    ) {
        stage = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "stage-clean"
        ) == 0
    ) {
        stage_clean = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "promote"
        ) == 0 &&
        strcmp(
            argv[2],
            "--dry-run"
        ) == 0
    ) {
        promote_dry = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "promote"
        ) == 0
    ) {
        promote = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "rollback"
        ) == 0
    ) {
        rollback = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "setup"
        ) == 0
    ) {
        setup = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "install"
        ) == 0 &&
        strcmp(
            argv[2],
            "--dry-run"
        ) == 0
    ) {
        install_dry = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "install"
        ) == 0
    ) {
        install = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "target"
        ) == 0 &&
        strcmp(
            argv[2],
            "show"
        ) == 0
    ) {
        target_show_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "target"
        ) == 0 &&
        strcmp(
            argv[2],
            "list"
        ) == 0
    ) {
        target_list_cmd = 1;
    } else if (
        argc == 5 &&
        strcmp(
            argv[1],
            "target"
        ) == 0 &&
        strcmp(
            argv[2],
            "set"
        ) == 0 &&
        strcmp(
            argv[3],
            "interface"
        ) == 0
    ) {
        target_set_interface_cmd = 1;
        target_value = argv[4];
    } else if (
        argc == 5 &&
        strcmp(
            argv[1],
            "target"
        ) == 0 &&
        strcmp(
            argv[2],
            "set"
        ) == 0 &&
        (
            strcmp(
                argv[3],
                "routing-table"
            ) == 0 ||
            strcmp(
                argv[3],
                "table"
            ) == 0
        )
    ) {
        target_set_table_cmd = 1;
        target_value = argv[4];
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "direct"
        ) == 0 &&
        strcmp(
            argv[2],
            "list"
        ) == 0
    ) {
        direct_list_cmd = 1;
    } else if (
        argc == 5 &&
        strcmp(
            argv[1],
            "direct"
        ) == 0 &&
        strcmp(
            argv[2],
            "add"
        ) == 0
    ) {
        direct_add_cmd = 1;
        direct_kind = argv[3];
        direct_value = argv[4];
    } else if (
        argc == 5 &&
        strcmp(
            argv[1],
            "direct"
        ) == 0 &&
        strcmp(
            argv[2],
            "remove"
        ) == 0
    ) {
        direct_remove_cmd = 1;
        direct_kind = argv[3];
        direct_value = argv[4];
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "direct"
        ) == 0 &&
        strcmp(
            argv[2],
            "sync"
        ) == 0
    ) {
        direct_sync_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "config"
        ) == 0 &&
        strcmp(
            argv[2],
            "show"
        ) == 0
    ) {
        config_show = 1;
    } else if (
        argc == 5 &&
        strcmp(
            argv[1],
            "config"
        ) == 0 &&
        strcmp(
            argv[2],
            "set"
        ) == 0
    ) {
        config_set = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "diag"
        ) == 0 &&
        strcmp(
            argv[2],
            "status"
        ) == 0
    ) {
        diag_status_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "diag"
        ) == 0 &&
        strcmp(
            argv[2],
            "start"
        ) == 0
    ) {
        diag_start_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "diag"
        ) == 0 &&
        strcmp(
            argv[2],
            "stop"
        ) == 0
    ) {
        diag_stop_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "diag"
        ) == 0 &&
        strcmp(
            argv[2],
            "sample"
        ) == 0
    ) {
        diag_sample_cmd = 1;
    } else if (
        argc == 3 &&
        strcmp(
            argv[1],
            "diag"
        ) == 0 &&
        strcmp(
            argv[2],
            "errors"
        ) == 0
    ) {
        diag_errors_cmd = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "daemon"
        ) == 0
    ) {
        daemon = 1;
    } else if (
        argc == 2 &&
        strcmp(
            argv[1],
            "apply"
        ) == 0
    ) {
        fprintf(
            stderr,
            "Susanin v%s write-mode apply is intentionally disabled. "
            "Use: apply --dry-run\n",
            SUSANIN_VERSION
        );

        return 2;
    } else {
        usage(argv[0]);
        return 2;
    }

    if (daemon) {
        app_config_t daemon_cfg;

        if (
            config_load_local(
                &daemon_cfg
            ) == 0
        ) {
            (void)diag_event(
                &daemon_cfg,
                "controller_start",
                "Susanin controller daemon started"
            );
        }

        printf(
            "Susanin controller is running. "
            "Use RouterOS /container/shell with "
            "`susanin setup` for first-run setup.\n"
        );

        fflush(stdout);

        for (;;) {
            sleep(3600);
        }
    }

    app_config_t cfg;

    if (
        config_show ||
        config_set ||
        target_show_cmd ||
        direct_list_cmd ||
        diag_status_cmd ||
        diag_start_cmd ||
        diag_stop_cmd
    ) {
        if (
            config_load_local(
                &cfg
            ) < 0
        ) {
            return 2;
        }

        if (config_show) {
            config_print_settings(
                &cfg
            );

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

            printf(
                "Susanin setting saved.\n\n"
            );

            config_print_settings(
                &cfg
            );

            return 0;
        }

        if (target_show_cmd) {
            target_show(
                &cfg
            );

            return 0;
        }

        if (direct_list_cmd) {
            return
                direct_list_local() == 0
                    ? 0
                    : 1;
        }

        if (diag_status_cmd) {
            return
                diag_status(
                    &cfg
                ) == 0
                    ? 0
                    : 1;
        }

        if (diag_start_cmd) {
            return
                diag_start(
                    &cfg
                ) == 0
                    ? 0
                    : 1;
        }

        if (diag_stop_cmd) {
            return
                diag_stop(
                    &cfg
                ) == 0
                    ? 0
                    : 1;
        }
    }

    if (
        config_load(
            &cfg
        ) < 0
    ) {
        return 2;
    }

    (void)diag_event(
        &cfg,
        "command_start",
        argv[1]
    );

    config_print_safe(
        &cfg
    );

    printf(
        "\nConnecting...\n"
    );

    ros_client_t ros;

    if (
        ros_connect(
            &ros,
            cfg.host,
            cfg.port
        ) < 0
    ) {
        (void)diag_event(
            &cfg,
            "routeros_error",
            "RouterOS API connection failed"
        );

        fprintf(
            stderr,
            "Cannot connect to RouterOS API at %s:%u\n",
            cfg.host,
            (unsigned)cfg.port
        );

        return 1;
    }

    (void)diag_event(
        &cfg,
        "routeros_connected",
        "RouterOS API connection established"
    );

    if (
        ros_login(
            &ros,
            cfg.user,
            cfg.password
        ) < 0
    ) {
        (void)diag_event(
            &cfg,
            "routeros_error",
            "RouterOS API authentication failed"
        );

        fprintf(
            stderr,
            "RouterOS API login failed\n"
        );

        ros_close(
            &ros
        );

        return 1;
    }

    (void)diag_event(
        &cfg,
        "routeros_authenticated",
        "RouterOS API authentication succeeded"
    );

    printf(
        "Authenticated.\n\n"
    );

    int rc;

    if (diag_errors_cmd) {
        rc =
            routerlog_collect_errors(
                &ros,
                &cfg
            );
    } else if (diag_sample_cmd) {
        rc =
            telemetry_collect_once(
                &ros,
                &cfg
            );
    } else if (target_list_cmd) {
        rc =
            target_list(
                &ros
            );
    } else if (target_set_interface_cmd) {
        rc =
            target_set_interface(
                &ros,
                &cfg,
                target_value
            );
    } else if (target_set_table_cmd) {
        rc =
            target_set_routing_table(
                &ros,
                &cfg,
                target_value
            );
    } else if (direct_add_cmd) {
        rc =
            direct_add(
                &ros,
                &cfg,
                direct_kind,
                direct_value
            );
    } else if (direct_remove_cmd) {
        rc =
            direct_remove(
                &ros,
                &cfg,
                direct_kind,
                direct_value
            );
    } else if (direct_sync_cmd) {
        rc =
            direct_sync(
                &ros,
                &cfg
            );
    } else if (setup) {
        rc =
            setup_run(
                &ros,
                &cfg
            );

        if (rc == 0) {
            rc =
                direct_sync(
                    &ros,
                    &cfg
                );
        }
    } else if (install_dry) {
        rc =
            install_run(
                &ros,
                &cfg,
                1
            );
    } else if (install) {
        rc =
            install_run(
                &ros,
                &cfg,
                0
            );

        if (rc == 0) {
            rc =
                direct_sync(
                    &ros,
                    &cfg
                );
        }
    } else if (apply_dry) {
        rc =
            apply_dry_run(
                &ros,
                &cfg
            );
    } else if (snapshot) {
        rc =
            snapshot_run(
                &ros,
                &cfg
            );
    } else if (render) {
        rc =
            renderer_run(
                &ros,
                &cfg
            );
    } else if (validate) {
        rc =
            validate_run(
                &ros,
                &cfg
            );
    } else if (stage) {
        rc =
            stage_run(
                &ros,
                &cfg
            );
    } else if (stage_clean) {
        rc =
            stage_clean_run(
                &ros
            );
    } else if (promote_dry) {
        rc =
            promote_dry_run(
                &ros,
                &cfg
            );
    } else if (promote) {
        rc =
            promote_run(
                &ros,
                &cfg
            );
    } else if (rollback) {
        rc =
            rollback_run(
                &ros
            );
    } else if (status) {
        rc =
            status_run(
                &ros,
                &cfg
            );
    } else {
        rc =
            discovery_run(
                &ros,
                &cfg,
                plan
            );
    }

    ros_close(
        &ros
    );

    {
        char event_message[256];

        snprintf(
            event_message,
            sizeof(event_message),
            "%s rc=%d",
            argv[1],
            rc
        );

        (void)diag_event(
            &cfg,
            "command_finish",
            event_message
        );
    }

    return
        rc == 0
            ? 0
            : 1;
}

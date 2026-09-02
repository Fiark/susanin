#include "diag.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define DIAG_DIR "/data/diagnostics"
#define DIAG_FILE DIAG_DIR "/susanin-debug.ndjson"

static int ensure_directory(const char *path) {
    if (mkdir(path, 0750) == 0) return 0;
    if (errno == EEXIST) return 0;

    perror(path);
    return -1;
}

static int ensure_diag_directory(void) {
    if (ensure_directory("/data") < 0) return -1;
    if (ensure_directory(DIAG_DIR) < 0) return -1;
    return 0;
}

static void rotated_path(
    unsigned index,
    char *out,
    size_t out_size
) {
    if (index == 0) {
        snprintf(out, out_size, "%s", DIAG_FILE);
    } else {
        snprintf(
            out,
            out_size,
            "%s.%u",
            DIAG_FILE,
            index
        );
    }
}

static int rotate_if_needed(
    const app_config_t *cfg
) {
    struct stat st;

    if (stat(DIAG_FILE, &st) != 0) {
        if (errno == ENOENT) return 0;

        perror("stat diagnostic log");
        return -1;
    }

    unsigned long long limit =
        (unsigned long long)cfg->diagnostic_max_size_mb *
        1024ULL *
        1024ULL;

    if ((unsigned long long)st.st_size < limit) {
        return 0;
    }

    unsigned max_files = cfg->diagnostic_max_files;

    if (max_files < 1) max_files = 1;

    if (max_files == 1) {
        if (remove(DIAG_FILE) != 0 && errno != ENOENT) {
            perror("remove diagnostic log");
            return -1;
        }

        return 0;
    }

    for (
        unsigned i = max_files - 1;
        i > 0;
        --i
    ) {
        char src[512];
        char dst[512];

        rotated_path(i - 1, src, sizeof(src));
        rotated_path(i, dst, sizeof(dst));

        if (i == max_files - 1) {
            if (remove(dst) != 0 && errno != ENOENT) {
                perror("remove old diagnostic log");
                return -1;
            }
        }

        if (rename(src, dst) != 0) {
            if (errno != ENOENT) {
                perror("rotate diagnostic log");
                return -1;
            }
        }
    }

    return 0;
}

static void json_string(
    FILE *f,
    const char *s
) {
    fputc('"', f);

    if (s) {
        const unsigned char *p =
            (const unsigned char *)s;

        while (*p) {
            unsigned char c = *p++;

            switch (c) {
                case '"':
                    fputs("\\\"", f);
                    break;

                case '\\':
                    fputs("\\\\", f);
                    break;

                case '\n':
                    fputs("\\n", f);
                    break;

                case '\r':
                    fputs("\\r", f);
                    break;

                case '\t':
                    fputs("\\t", f);
                    break;

                default:
                    if (c < 0x20) {
                        fprintf(
                            f,
                            "\\u%04x",
                            (unsigned)c
                        );
                    } else {
                        fputc((int)c, f);
                    }
                    break;
            }
        }
    }

    fputc('"', f);
}

static void timestamp_utc(
    char out[32]
) {
    time_t now = time(NULL);
    struct tm *tm_now = gmtime(&now);

    if (!tm_now) {
        snprintf(
            out,
            32,
            "1970-01-01T00:00:00Z"
        );
        return;
    }

    if (
        strftime(
            out,
            32,
            "%Y-%m-%dT%H:%M:%SZ",
            tm_now
        ) == 0
    ) {
        snprintf(
            out,
            32,
            "1970-01-01T00:00:00Z"
        );
    }
}

int diag_event(
    const app_config_t *cfg,
    const char *type,
    const char *message
) {
    if (!cfg || !cfg->diagnostics_enabled) {
        return 0;
    }

    if (ensure_diag_directory() < 0) {
        return -1;
    }

    if (rotate_if_needed(cfg) < 0) {
        return -1;
    }

    FILE *f = fopen(DIAG_FILE, "a");

    if (!f) {
        perror("open diagnostic log");
        return -1;
    }

    char ts[32];
    timestamp_utc(ts);

    fputs("{\"ts\":", f);
    json_string(f, ts);

    fputs(",\"type\":", f);
    json_string(f, type ? type : "event");

    fputs(",\"message\":", f);
    json_string(f, message ? message : "");

    fputs("}\n", f);

    if (fclose(f) != 0) {
        perror("close diagnostic log");
        return -1;
    }

    return 0;
}

int diag_start(app_config_t *cfg) {
    if (!cfg) return -1;

    if (ensure_diag_directory() < 0) {
        return -1;
    }

    if (cfg->diagnostics_enabled) {
        printf(
            "Diagnostic recorder is already enabled.\n"
        );
        return 0;
    }

    if (
        config_set_option(
            cfg,
            "diagnostics",
            "on"
        ) < 0
    ) {
        return -1;
    }

    if (
        diag_event(
            cfg,
            "session_start",
            "Diagnostic recorder enabled"
        ) < 0
    ) {
        (void)config_set_option(
            cfg,
            "diagnostics",
            "off"
        );

        return -1;
    }

    printf(
        "Diagnostic recorder enabled.\n"
        "Output: %s\n",
        DIAG_FILE
    );

    return 0;
}

int diag_stop(app_config_t *cfg) {
    if (!cfg) return -1;

    if (!cfg->diagnostics_enabled) {
        printf(
            "Diagnostic recorder is already disabled.\n"
        );
        return 0;
    }

    int event_rc = diag_event(
        cfg,
        "session_stop",
        "Diagnostic recorder disabled"
    );

    if (
        config_set_option(
            cfg,
            "diagnostics",
            "off"
        ) < 0
    ) {
        return -1;
    }

    if (event_rc < 0) {
        fprintf(
            stderr,
            "Warning: diagnostic recorder was disabled, "
            "but the final event could not be written.\n"
        );
    }

    printf("Diagnostic recorder disabled.\n");

    return event_rc < 0 ? -1 : 0;
}

int diag_status(const app_config_t *cfg) {
    if (!cfg) return -1;

    printf("=== SUSANIN DIAGNOSTICS ===\n\n");

    printf(
        "Recorder : %s\n",
        cfg->diagnostics_enabled
            ? "on"
            : "off"
    );

    printf(
        "File     : %s\n",
        DIAG_FILE
    );

    printf(
        "Max size : %u MB\n",
        cfg->diagnostic_max_size_mb
    );

    printf(
        "Max files: %u\n",
        cfg->diagnostic_max_files
    );

    unsigned files = 0;
    unsigned long long total = 0;

    for (
        unsigned i = 0;
        i < cfg->diagnostic_max_files;
        ++i
    ) {
        char path[512];
        struct stat st;

        rotated_path(i, path, sizeof(path));

        if (stat(path, &st) == 0) {
            ++files;
            total +=
                (unsigned long long)st.st_size;
        }
    }

    printf("Stored   : %u file(s)\n", files);
    printf("Bytes    : %llu\n", total);

    return 0;
}

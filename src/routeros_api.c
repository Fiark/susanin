#define _POSIX_C_SOURCE 200112L
#include "routeros_api.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int write_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int read_all(int fd, void *buf, size_t len) {
    unsigned char *p = (unsigned char *)buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static size_t encode_len(uint32_t len, unsigned char out[5]) {
    if (len < 0x80U) {
        out[0] = (unsigned char)len;
        return 1;
    }
    if (len < 0x4000U) {
        uint32_t v = len | 0x8000U;
        out[0] = (unsigned char)(v >> 8);
        out[1] = (unsigned char)v;
        return 2;
    }
    if (len < 0x200000U) {
        uint32_t v = len | 0xC00000U;
        out[0] = (unsigned char)(v >> 16);
        out[1] = (unsigned char)(v >> 8);
        out[2] = (unsigned char)v;
        return 3;
    }
    if (len < 0x10000000U) {
        uint32_t v = len | 0xE0000000U;
        out[0] = (unsigned char)(v >> 24);
        out[1] = (unsigned char)(v >> 16);
        out[2] = (unsigned char)(v >> 8);
        out[3] = (unsigned char)v;
        return 4;
    }
    out[0] = 0xF0;
    out[1] = (unsigned char)(len >> 24);
    out[2] = (unsigned char)(len >> 16);
    out[3] = (unsigned char)(len >> 8);
    out[4] = (unsigned char)len;
    return 5;
}

static int send_word(int fd, const char *word) {
    size_t slen = word ? strlen(word) : 0;
    if (slen > UINT32_MAX) return -1;
    unsigned char prefix[5];
    size_t plen = encode_len((uint32_t)slen, prefix);
    if (write_all(fd, prefix, plen) < 0) return -1;
    if (slen && write_all(fd, word, slen) < 0) return -1;
    return 0;
}

static int recv_len(int fd, uint32_t *out) {
    unsigned char b[5];
    if (read_all(fd, b, 1) < 0) return -1;

    if ((b[0] & 0x80U) == 0) {
        *out = b[0];
        return 0;
    }
    if ((b[0] & 0xC0U) == 0x80U) {
        if (read_all(fd, b + 1, 1) < 0) return -1;
        *out = ((uint32_t)(b[0] & 0x3FU) << 8) | b[1];
        return 0;
    }
    if ((b[0] & 0xE0U) == 0xC0U) {
        if (read_all(fd, b + 1, 2) < 0) return -1;
        *out = ((uint32_t)(b[0] & 0x1FU) << 16) |
               ((uint32_t)b[1] << 8) | b[2];
        return 0;
    }
    if ((b[0] & 0xF0U) == 0xE0U) {
        if (read_all(fd, b + 1, 3) < 0) return -1;
        *out = ((uint32_t)(b[0] & 0x0FU) << 24) |
               ((uint32_t)b[1] << 16) |
               ((uint32_t)b[2] << 8) | b[3];
        return 0;
    }
    if (b[0] == 0xF0U) {
        if (read_all(fd, b + 1, 4) < 0) return -1;
        *out = ((uint32_t)b[1] << 24) |
               ((uint32_t)b[2] << 16) |
               ((uint32_t)b[3] << 8) | b[4];
        return 0;
    }
    return -1;
}

static void free_sentence(ros_sentence_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->count; ++i) free(s->words[i]);
    free(s->words);
    s->words = NULL;
    s->count = 0;
}

static int recv_sentence(int fd, ros_sentence_t *out) {
    memset(out, 0, sizeof(*out));
    size_t cap = 8;
    out->words = calloc(cap, sizeof(char *));
    if (!out->words) return -1;

    for (;;) {
        uint32_t len = 0;
        if (recv_len(fd, &len) < 0) {
            free_sentence(out);
            return -1;
        }
        if (len == 0) return 0;
        if (out->count >= ROS_MAX_WORDS) {
            free_sentence(out);
            return -1;
        }
        if (out->count == cap) {
            cap *= 2;
            char **tmp = realloc(out->words, cap * sizeof(char *));
            if (!tmp) {
                free_sentence(out);
                return -1;
            }
            out->words = tmp;
        }
        char *w = malloc((size_t)len + 1);
        if (!w) {
            free_sentence(out);
            return -1;
        }
        if (read_all(fd, w, len) < 0) {
            free(w);
            free_sentence(out);
            return -1;
        }
        w[len] = '\0';
        out->words[out->count++] = w;
    }
}

int ros_connect(ros_client_t *client, const char *host, uint16_t port) {
    if (!client || !host) return -1;
    memset(client, 0, sizeof(*client));
    client->fd = -1;
    snprintf(client->host, sizeof(client->host), "%s", host);
    client->port = port;

    struct addrinfo hints, *res = NULL, *it = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%u", (unsigned)port);
    int rc = getaddrinfo(host, portbuf, &hints, &res);
    if (rc != 0) return -1;

    for (it = res; it; it = it->ai_next) {
        int fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            client->fd = fd;
            break;
        }
        close(fd);
    }
    freeaddrinfo(res);
    return client->fd >= 0 ? 0 : -1;
}

void ros_close(ros_client_t *client) {
    if (client && client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
}

int ros_is_reply(const ros_sentence_t *sentence, const char *reply) {
    return sentence && sentence->count > 0 && reply &&
           strcmp(sentence->words[0], reply) == 0;
}

const char *ros_get_attr(const ros_sentence_t *sentence, const char *key) {
    if (!sentence || !key) return NULL;
    size_t klen = strlen(key);
    for (size_t i = 1; i < sentence->count; ++i) {
        const char *w = sentence->words[i];
        if (!w || w[0] != '=') continue;
        const char *p = w + 1;
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') return p + klen + 1;
    }
    return NULL;
}

int ros_command(ros_client_t *client, const char *const *words, size_t nwords,
                ros_sentence_cb cb, void *ctx) {
    if (!client || client->fd < 0 || !words || nwords == 0) return -1;

    for (size_t i = 0; i < nwords; ++i) {
        if (send_word(client->fd, words[i]) < 0) return -1;
    }
    if (send_word(client->fd, "") < 0) return -1;

    for (;;) {
        ros_sentence_t s;
        if (recv_sentence(client->fd, &s) < 0) return -1;

        int cb_rc = 0;
        if (cb) cb_rc = cb(&s, ctx);

        /* RouterOS 7.18+ may emit !empty for commands with no data, but the
           protocol still terminates every command with a final !done sentence.
           Treating !empty as terminal leaves that !done unread on the socket;
           the next command then consumes the stale !done and appears to return
           an empty result. This is especially visible during fresh install,
           where many managed-object lookups are expected to be empty. */
        int done = ros_is_reply(&s, "!done");
        int trap = ros_is_reply(&s, "!trap") || ros_is_reply(&s, "!fatal");

        if (trap) {
            const char *msg = ros_get_attr(&s, "message");
            fprintf(stderr, "RouterOS API error: %s\n", msg ? msg : "unknown error");
        }

        free_sentence(&s);
        if (cb_rc != 0) return cb_rc;
        if (trap) return -1;
        if (done) return 0;
    }
}

static int login_cb(const ros_sentence_t *s, void *ctx) {
    (void)ctx;
    if (ros_is_reply(s, "!trap") || ros_is_reply(s, "!fatal")) return -1;
    return 0;
}

int ros_login(ros_client_t *client, const char *user, const char *password) {
    if (!user || !password) return -1;
    char name[320];
    char pass[1024];
    snprintf(name, sizeof(name), "=name=%s", user);
    snprintf(pass, sizeof(pass), "=password=%s", password);
    const char *cmd[] = {"/login", name, pass};
    return ros_command(client, cmd, 3, login_cb, NULL);
}

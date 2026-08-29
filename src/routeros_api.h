#ifndef AUTOAWG_ROUTEROS_API_H
#define AUTOAWG_ROUTEROS_API_H

#include <stddef.h>
#include <stdint.h>

#define ROS_MAX_WORDS 128

typedef struct {
    int fd;
    char host[256];
    uint16_t port;
} ros_client_t;

typedef struct {
    char **words;
    size_t count;
} ros_sentence_t;

typedef int (*ros_sentence_cb)(const ros_sentence_t *sentence, void *ctx);

int ros_connect(ros_client_t *client, const char *host, uint16_t port);
void ros_close(ros_client_t *client);
int ros_login(ros_client_t *client, const char *user, const char *password);
int ros_command(ros_client_t *client, const char *const *words, size_t nwords,
                ros_sentence_cb cb, void *ctx);
const char *ros_get_attr(const ros_sentence_t *sentence, const char *key);
int ros_is_reply(const ros_sentence_t *sentence, const char *reply);

#endif

CC ?= cc
CFLAGS ?= -O2 -pipe -std=c11 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?= -Isrc
LDFLAGS ?=

SRC := src/main.c src/routeros_api.c src/config.c src/diag.c src/telemetry.c src/discovery.c src/status.c src/apply.c src/fingerprint.c src/snapshot.c src/renderer.c src/validate.c src/stage.c src/promote.c src/setup.c src/install.c
OBJ := $(SRC:.c=.o)
BIN := susanin

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

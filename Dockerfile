FROM alpine:3.22 AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY . .
RUN make clean \
 && make CFLAGS='-Os -pipe -std=c11 -Wall -Wextra -Wpedantic -Werror' \
 && strip susanin

FROM alpine:3.22
RUN addgroup -S susanin && adduser -S -G susanin susanin
COPY --from=build /src/susanin /usr/local/bin/susanin
COPY templates /usr/share/susanin/templates
USER susanin
ENTRYPOINT ["/usr/local/bin/susanin"]
CMD ["daemon"]

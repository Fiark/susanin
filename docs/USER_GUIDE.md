
# Susanin v0.11.5 — руководство пользователя

**v0.11.5** — первый стабильный релиз Susanin для проверенного
ARM64 / RouterOS 7.23.3 reference profile.

Susanin анализирует поведение RouterOS connection tracking,
проверяет проблемные направления через уже существующий route-based
VPN/tunnel и временно запоминает, какой маршрут работает.

Susanin не является VPN-клиентом.

Перед установкой и обновлением сделайте RouterOS backup.

---

## История возможностей

### v0.11.3 — фундамент Susanin

В v0.11.3 появилась основная архитектура проекта:

- credentialless bootstrap;
- внутренний RouterOS account `susanin-agent`;
- случайный machine secret;
- secret хранится в mounted file, а не environment/argv;
- автоматический поиск LAN;
- выбор VPN/tunnel;
- поиск подходящей routing table;
- возможность создать отдельную FIB table;
- динамический renderer RouterOS scripts;
- validation generated source самим RouterOS;
- transactional fresh install;
- FAST;
- SOFT / DETECT;
- JUDGE;
- HEALTH;
- независимое TCP/UDP обучение;
- динамические WATCH / TEST / OK / COOLDOWN состояния;
- fail-open DIRECT при падении tunnel;
- automatic recovery после восстановления tunnel;
- controller/data-plane separation;
- stage / promote / rollback;
- исправление RouterOS API framing `!empty` + `!done`.

### v0.11.4 — эксплуатация, логирование и диагностика

v0.11.4 добавил эксплуатационные возможности:

- persistent configuration `/data/susanin.conf`;
- configurable RouterOS decision logging;
- уровни `quiet`, `error`, `info`, `debug`, `trace`;
- internal diagnostic recorder;
- NDJSON diagnostic log;
- bounded log rotation;
- `diag status`;
- `diag start`;
- `diag stop`;
- `diag sample`;
- `diag errors`;
- RouterOS CPU/RAM telemetry;
- RouterOS uptime/version/board telemetry;
- conntrack total/max telemetry;
- script-job inventory;
- managed worker inventory;
- RouterOS script-error collection;
- отдельный подсчёт `no such item`;
- более безопасные RouterOS script exits.

### v0.11.5 — стабилизация data plane

v0.11.5 основан на проверенном `v0.11.5-dev4`.

Важные изменения:

- FAST использует один filtered conntrack snapshot;
- SOFT/DETECT использует один consolidated snapshot;
- JUDGE использует один TEST+OK snapshot;
- одна transient connection entry не должна обрывать весь worker;
- dynamic address-list mutations защищены от transient races;
- HEALTH cleanup/recovery защищён;
- сокращено количество обращений к dynamic conntrack;
- FAST/SOFT/JUDGE thresholds сохранены;
- scheduler cadence сохранён;
- WATCH/TEST/OK/COOLDOWN state machine сохранена.

---

# Установка

Из GitHub Release нужны:

~~~text
susanin.tar
install.rsc
~~~

Дополнительно:

~~~text
SHA256SUMS
uninstall.rsc
uninstall-controller.rsc
~~~

Загрузите файлы в MikroTik.

Parser dry-run:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Установка:

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Дождитесь RUNNING:

~~~routeros
/container print where name="susanin-controller"
~~~

Затем:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin setup" \
  no-sh \
  timeout=300
~~~

---

# Проверка после установки

Status:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin status" \
  no-sh \
  timeout=60
~~~

Нормальный результат:

~~~text
Summary: scripts=4/4 schedulers=4/4 mangle=8
Installation state: detected
~~~

Structural reconciliation:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh \
  timeout=60
~~~

Нормальное состояние:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

---

# Data plane

## FAST

FAST ищет быстро распознаваемые признаки:

- TCP SYN без ответа;
- TCP CLOSE с плохим обменом;
- QUIC UDP/443 без reply.

FAST не подтверждает destination окончательно.

Он переводит destination в TEST.

## SOFT / DETECT

SOFT/DETECT ищет менее очевидные проблемы:

- TCP STALL;
- TCP LATE STALL;
- UDP no-reply;
- QUIC late stall.

Для late-stall используется короткий WATCH/debounce.

## JUDGE

JUDGE проверяет соединение, которое уже было направлено через tunnel.

Успех:

~~~text
TEST -> OK
~~~

Неудача через tunnel:

~~~text
TEST -> COOLDOWN -> DIRECT
~~~

TCP и UDP оцениваются отдельно.

Один IP может быть OK для TCP и DIRECT для UDP, или наоборот.

## HEALTH

HEALTH следит за выбранным tunnel.

После двух health miss:

~~~text
tunnel DOWN
adaptive mangle disabled
traffic fail-open -> DIRECT
~~~

После восстановления:

~~~text
tunnel UP
adaptive routing enabled
~~~

---

# Runtime cache

TCP:

~~~text
auto_awg_watch_tcp
auto_awg_test_tcp
auto_awg_ok_tcp
auto_awg_cooldown_tcp
~~~

UDP:

~~~text
auto_awg_watch_udp
auto_awg_test_udp
auto_awg_ok_udp
auto_awg_cooldown_udp
~~~

WATCH и TEST короткоживущие.

OK — временный подтверждённый cache.

Это не постоянный IP blocklist.

---

# Основные команды

Все команды запускаются внутри Susanin controller.

## version

~~~text
susanin version
~~~

## setup

Первичная настройка:

~~~text
susanin setup
~~~

## status

Текущее состояние:

~~~text
susanin status
~~~

Показывает:

- egress;
- scripts;
- schedulers;
- mangle;
- TCP cache counters;
- UDP cache counters.

## discover

Read-only discovery:

~~~text
susanin discover
~~~

## plan

Read-only анализ:

~~~text
susanin plan
~~~

## apply --dry-run

Ничего не меняет.

~~~text
susanin apply --dry-run
~~~

Показывает structural drift.

---

# Renderer и validation

## render

~~~text
susanin render
~~~

Генерирует desired source без изменения production.

Показывает:

- script name;
- bytes;
- FNV-1a fingerprint.

## validate

~~~text
susanin validate
~~~

Создаёт временные validator objects.

Production source не изменяется.

Ожидается:

~~~text
PASS=4 FAIL=0
Production scripts changed: NO
~~~

## snapshot

~~~text
susanin snapshot
~~~

Показывает фактические fingerprints production scripts.

---

# Installation commands

## install --dry-run

~~~text
susanin install --dry-run
~~~

Проверяет возможность установки без commit.

## install

~~~text
susanin install
~~~

Создаёт/reconciles managed data plane.

---

# Safe source upgrade

## stage

~~~text
susanin stage
~~~

Создаёт inert:

~~~text
susanin-stage-health
susanin-stage-fast
susanin-stage-detect
susanin-stage-judge
~~~

Production не меняется.

## promote --dry-run

~~~text
susanin promote --dry-run
~~~

Проверяет safety gates.

## promote

~~~text
susanin promote
~~~

Последовательность:

1. snapshot production;
2. rollback backup;
3. pause schedulers;
4. wait jobs idle;
5. replace source;
6. verify;
7. restore schedulers.

## rollback

~~~text
susanin rollback
~~~

Возвращает `susanin-backup-*`.

## stage-clean

~~~text
susanin stage-clean
~~~

Удаляет inert stage objects.

---

# Runtime configuration

## config show

~~~text
susanin config show
~~~

Показывает:

- Logging level;
- Diagnostic recorder;
- Diagnostic max size;
- Diagnostic max files.

## log-level

~~~text
susanin config set log-level quiet
susanin config set log-level error
susanin config set log-level info
susanin config set log-level debug
susanin config set log-level trace
~~~

Default:

~~~text
info
~~~

Важно: log-level встраивается renderer в RouterOS script source.

Для уже установленного data plane после изменения log-level
`apply --dry-run` может показать UPDATE.

Тогда применяйте изменение через:

~~~text
stage
promote --dry-run
promote
~~~

## diagnostics

~~~text
susanin config set diagnostics on
susanin config set diagnostics off
~~~

Обычно удобнее:

~~~text
susanin diag start
susanin diag stop
~~~

## Diagnostic rotation

Допустимый размер:

~~~text
diagnostic-max-size-mb = 1..100
~~~

Количество файлов:

~~~text
diagnostic-max-files = 1..10
~~~

Пример:

~~~text
susanin config set diagnostic-max-size-mb 20
susanin config set diagnostic-max-files 5
~~~

---

# Diagnostics

## diag status

~~~text
susanin diag status
~~~

Показывает:

- recorder on/off;
- diagnostic file;
- max size;
- max files;
- количество файлов;
- общий размер.

## diag start

~~~text
susanin diag start
~~~

Включает internal recorder.

Container path:

~~~text
/data/diagnostics/susanin-debug.ndjson
~~~

## diag stop

~~~text
susanin diag stop
~~~

Записывает session_stop и отключает recorder.

## diag sample

~~~text
susanin diag sample
~~~

Собирает:

- CPU;
- free memory;
- total memory;
- uptime;
- RouterOS version;
- board;
- architecture;
- conntrack total;
- conntrack max;
- script jobs;
- Susanin managed workers.

## diag errors

~~~text
susanin diag errors
~~~

Показывает:

- RouterOS log records scanned;
- script errors;
- `no such item`;
- latest `no such item`;
- до 8 последних script errors.

---

# Обязательная процедура перед Bug Issue

Перед созданием технического Issue:

~~~text
1. susanin diag start
2. воспроизвести проблему
3. susanin diag sample
4. susanin diag errors
5. susanin status
6. susanin apply --dry-run
7. susanin diag stop
~~~

Полная инструкция:

[LOGGING.md](LOGGING.md)

Issue без диагностических данных намного сложнее воспроизвести.

---

# Что нельзя публиковать

Никогда не прикладывайте:

- RouterOS `.backup`;
- `/export show-sensitive`;
- `susanin-secrets/routeros_password`;
- WireGuard private key;
- AmneziaWG private key;
- VPN password;
- API password;
- другие credentials.

---

# Controller и data plane

Susanin container — control plane.

RouterOS scripts — data plane.

Поэтому остановка или обновление container сама по себе
не останавливает уже работающие schedulers.

После любого controller upgrade проверяйте:

~~~text
susanin status
susanin apply --dry-run
~~~

Для exact source drift используйте:

~~~text
susanin snapshot
~~~

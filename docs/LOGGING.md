
# Логирование и диагностика Susanin v0.11.5

Перед созданием Bug Issue рекомендуется сначала собрать
диагностический пакет внутренними механизмами Susanin.

В Susanin существуют два разных вида логирования:

1. RouterOS decision log `AUTO-AWG:`;
2. internal Susanin diagnostic recorder.

Они дополняют друг друга.

---

# RouterOS decision log

Текущий уровень:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config show" \
  no-sh timeout=30
~~~

Допустимые уровни:

| Level | Назначение |
|---|---|
| `quiet` | почти без decision logging |
| `error` | важные health/fail-open события |
| `info` | error + recovery + CONFIRMED |
| `debug` | info + FAST/SOFT detection |
| `trace` | принимается config; в v0.11.5 verbosity практически соответствует debug |

Default:

~~~text
info
~~~

Изменить:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config set log-level debug" \
  no-sh timeout=30
~~~

## Важно: log-level меняет generated source

Renderer встраивает logging switches непосредственно в RouterOS scripts.

Поэтому после изменения уровня:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh timeout=60
~~~

может показать UPDATE.

Для применения:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin stage" \
  no-sh timeout=300

/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin promote --dry-run" \
  no-sh timeout=120

/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin promote" \
  no-sh timeout=300
~~~

После диагностики рекомендуется вернуть:

~~~text
log-level=info
~~~

и снова выполнить безопасный stage/promote.

---

# Просмотр AUTO-AWG

Live:

~~~routeros
/log print follow-only where message~"AUTO-AWG:"
~~~

Последние события:

~~~routeros
/log print where message~"AUTO-AWG:"
~~~

Типовые сообщения:

~~~text
FAST TCP-SYN
FAST TCP-CLOSE
FAST QUIC

SOFT TCP-STALL
SOFT TCP-LATE-STALL
SOFT UDP
SOFT QUIC-LATE-STALL

CONFIRMED ... via tcp
CONFIRMED ... via udp

tunnel DOWN ... DIRECT
tunnel UP ...
~~~

RouterOS log может содержать destination IP и port.

Просмотрите данные перед публикацией.

---

# Internal diagnostic recorder

Проверка:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag status" \
  no-sh timeout=30
~~~

Включить:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag start" \
  no-sh timeout=30
~~~

Container path:

~~~text
/data/diagnostics/susanin-debug.ndjson
~~~

RouterOS data mount:

~~~text
susanin-data/diagnostics/
~~~

Отключить:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag stop" \
  no-sh timeout=30
~~~

---

# Что пишет recorder

NDJSON: один JSON object на строку.

Типовые event types:

~~~text
session_start
session_stop
controller_start
config_change
command_start
command_finish
routeros_connected
routeros_authenticated
routeros_error
router_metrics
conntrack_metrics
script_jobs
routeros_error_summary
telemetry_error
~~~

Данные никуда автоматически не отправляются.

---

# Что recorder НЕ делает

`diag start` НЕ является непрерывной копией RouterOS `/log`.

Recorder пишет события Susanin controller.

Чтобы добавить состояние RouterOS после воспроизведения проблемы,
отдельно выполняются:

~~~text
diag sample
diag errors
~~~

---

# diag sample

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag sample" \
  no-sh timeout=60
~~~

Снимает:

## Resource

- CPU load;
- free memory;
- total memory;
- uptime;
- RouterOS version;
- board name;
- architecture.

## Conntrack

- total entries;
- maximum entries.

## Script jobs

- total jobs;
- managed Susanin workers;
- susanin-agent jobs;
- job names;
- start information.

Если recorder включён, данные также попадают в NDJSON.

---

# diag errors

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag errors" \
  no-sh timeout=60
~~~

Команда читает bounded RouterOS log и локально ищет script errors.

Показывает:

~~~text
Log records scanned
Script errors
no such item
Latest no such item
Recent script errors
~~~

Susanin фильтрует данные локально, поскольку RouterOS 7.23.3
показывал непоследовательное поведение server-side filter
по `topics~"script,error"`.

При включённом recorder summary также записывается как:

~~~text
routeros_error_summary
~~~

---

# Rotation

Defaults:

~~~text
diagnostic_max_size_mb=10
diagnostic_max_files=3
~~~

Настройка:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config set diagnostic-max-size-mb 20" \
  no-sh timeout=30

/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config set diagnostic-max-files 5" \
  no-sh timeout=30
~~~

Допустимо:

~~~text
1..100 MB
1..10 files
~~~

Файлы:

~~~text
susanin-debug.ndjson
susanin-debug.ndjson.1
susanin-debug.ndjson.2
...
~~~

---

# ОБЯЗАТЕЛЬНАЯ последовательность перед Bug Issue

## 1. Включить recorder

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag start" \
  no-sh timeout=30
~~~

## 2. При необходимости включить debug

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config set log-level debug" \
  no-sh timeout=30
~~~

Если `apply --dry-run` показывает UPDATE,
примените logging source через stage/promote.

## 3. Воспроизвести проблему

Запомните приблизительное время.

## 4. Снять telemetry

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag sample" \
  no-sh timeout=60
~~~

## 5. Снять RouterOS errors

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag errors" \
  no-sh timeout=60
~~~

## 6. Снять status

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin status" \
  no-sh timeout=60
~~~

## 7. Проверить drift

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh timeout=60
~~~

## 8. Сохранить relevant RouterOS log

~~~routeros
/log print where message~"AUTO-AWG:"
~~~

Script errors:

~~~routeros
/log print where message~"script error"
~~~

## 9. Остановить recorder

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag stop" \
  no-sh timeout=30
~~~

---


# Как выгрузить diagnostic files

На RouterOS persistent diagnostic files находятся в mounted directory:

~~~text
susanin-data/diagnostics/
~~~

Проверить:

~~~routeros
/file print where name~"susanin-data/diagnostics"
~~~

Обычно нужны:

~~~text
susanin-data/diagnostics/susanin-debug.ndjson
susanin-data/diagnostics/susanin-debug.ndjson.1
susanin-data/diagnostics/susanin-debug.ndjson.2
~~~

Скачать их можно через WinBox → Files, SCP или другой обычный способ доступа к RouterOS Files.

Не выгружайте целиком `susanin-data` и тем более `susanin-secrets`.

Перед публикацией просмотрите NDJSON и RouterOS logs и при необходимости anonymize IP/interface/topology metadata.

---

# Что приложить к Issue

Минимально:

- Susanin version;
- RouterOS version;
- architecture/device family;
- tunnel type;
- topology description;
- `susanin status`;
- `susanin apply --dry-run`;
- `susanin diag sample`;
- `susanin diag errors`;
- sanitized relevant `AUTO-AWG:` log;
- sanitized diagnostic NDJSON.

---

# Не публикуйте секреты

Запрещено публиковать:

~~~text
RouterOS .backup
/export show-sensitive
susanin-secrets/routeros_password
WireGuard private key
AmneziaWG private key
VPN credentials
API credentials
passwords
~~~

Diagnostic recorder специально не пишет machine password.

Но RouterOS logs могут содержать:

- IP;
- port;
- interface names;
- topology information.

При необходимости anonymize эти данные.

---

# Рекомендуемый нормальный режим

После диагностики:

~~~text
log-level=info
diagnostics=off
~~~

Проверить:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config show" \
  no-sh timeout=30
~~~

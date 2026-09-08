> [!WARNING]
> **Susanin изменяет RouterOS firewall/routing objects.**
> Перед установкой или обновлением обязательно сделайте backup RouterOS.
>
> Stable v0.12.0 проверен на **ARM64 / RouterOS 7.23.3**.

# Сусанин — адаптивная маршрутизация через VPN для MikroTik

[![C11](https://img.shields.io/badge/C-11-blue)](https://en.cppreference.com/w/c/11)
[![RouterOS](https://img.shields.io/badge/RouterOS-tested%207.23.3-293239)](https://mikrotik.com/)
[![Architecture](https://img.shields.io/badge/arch-ARM64-6a5acd)](#требования)
[![Status](https://img.shields.io/badge/status-stable-brightgreen)](https://github.com/Fiark/susanin/releases/tag/v0.12.0)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

[![Release](https://img.shields.io/github/v/release/Fiark/susanin?label=stable%20release)](https://github.com/Fiark/susanin/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Fiark/susanin/total?label=downloads)](https://github.com/Fiark/susanin/releases)

**Susanin** наблюдает за поведением соединений в MikroTik RouterOS,
обнаруживает направления, которые плохо работают через обычный DIRECT path,
проверяет их через уже существующий VPN/tunnel и временно запоминает
рабочий маршрут.

В v0.12.0 adaptive identity учитывает:

~~~text
protocol + destination IPv4 + destination port
~~~

Поэтому, например, TCP/443 и UDP/443 к одному IP могут маршрутизироваться
по-разному.

Susanin **не является VPN-клиентом**. WireGuard, AmneziaWG или другой
route-based VPN должен быть настроен заранее.

## Stable v0.12.0

> [!IMPORTANT]
> ### Скачать
>
> Release:
> **[Susanin v0.12.0](https://github.com/Fiark/susanin/releases/tag/v0.12.0)**
>
> Для установки нужны:
>
> - [susanin.tar](https://github.com/Fiark/susanin/releases/download/v0.12.0/susanin.tar)
> - [install.rsc](https://github.com/Fiark/susanin/releases/download/v0.12.0/install.rsc)
>
> Дополнительно:
>
> - [SHA256SUMS](https://github.com/Fiark/susanin/releases/download/v0.12.0/SHA256SUMS)
> - [uninstall.rsc](https://github.com/Fiark/susanin/releases/download/v0.12.0/uninstall.rsc)
> - [uninstall-controller.rsc](https://github.com/Fiark/susanin/releases/download/v0.12.0/uninstall-controller.rsc)
>
> Полное руководство: [docs/USER_GUIDE.md](docs/USER_GUIDE.md)
>
> Release notes: [docs/RELEASE_NOTES_v0.12.0.md](docs/RELEASE_NOTES_v0.12.0.md)

## Что нового в v0.12.0

Основные изменения относительно v0.11.5:

- routing target **Interface** или **Routing table**;
- table-native routing;
- **VPN Direct** для явного принудительного VPN-маршрута;
- port-aware adaptive state;
- отдельное обучение TCP и UDP;
- lazy per-port mangle rules;
- профили `fast`, `middle`, `slow`;
- migration hardening старого IP-only state;
- bounded runtime GC;
- strict IPv4-only mode;
- graceful shutdown controller;
- сохранение RouterOS data plane при остановке controller.

Подробно: [Release Notes v0.12.0](docs/RELEASE_NOTES_v0.12.0.md).

## Как это устроено

Susanin разделён на два слоя.

### RouterOS data plane

Непрерывно работает непосредственно в RouterOS:

~~~text
auto-awg-health
auto-awg-fast
auto-awg-detect
auto-awg-judge
~~~

Именно RouterOS:

- наблюдает connection tracking;
- ведёт временное adaptive state;
- создаёт lazy per-port routing state;
- маркирует нужные соединения;
- отправляет подтверждённый трафик в выбранный routing target;
- делает fail-open в DIRECT при проблемах с VPN.

### Susanin controller

Контейнер выполняет control-plane задачи:

- discovery;
- first-run setup;
- выбор routing target;
- генерацию RouterOS scripts;
- validation;
- install;
- status;
- structural reconciliation;
- VPN Direct policy;
- configuration;
- diagnostics;
- stage/promote/rollback;
- runtime GC.

Пользовательский трафик **не проходит через контейнер Susanin**.

Поэтому уже установленный RouterOS data plane продолжает работать даже при
остановке controller.

## Routing target

v0.12.0 поддерживает два режима.

### 1. Interface

Вы выбираете route-based интерфейс, например:

~~~text
wg-vpn
~~~

Susanin использует его как target.

Если подходящей отдельной FIB routing table нет, Susanin может создать
собственную таблицу и маршрут через выбранный интерфейс.

### 2. Routing table

Вы выбираете уже существующую RouterOS routing table, например:

~~~text
r_to_awg
~~~

Это предпочтительно, если маршрутизацией VPN уже управляет ваша конфигурация.

Susanin передаёт выбранный трафик в эту таблицу и не пытается заменить её
внутреннюю схему одним gateway/interface.

Так можно сохранить:

- recursive routing;
- несколько маршрутов;
- ECMP;
- multi-egress;
- собственный failover;
- собственный NAT.

Для `routing-table` target Susanin **не создаёт автоматически tunnel NAT**.

## VPN Direct

VPN Direct — явное правило пользователя:

> этот IP, CIDR или domain всегда отправлять через выбранный VPN target,
> не ожидая adaptive learning.

Это **не bypass в DIRECT**.

Примеры внутри controller:

~~~text
susanin direct add ip 1.1.1.1/32
susanin direct add domain example.com
susanin direct list
susanin direct sync
~~~

Удаление:

~~~text
susanin direct remove ip 1.1.1.1/32
susanin direct remove domain example.com
~~~

VPN Direct имеет приоритет над обычными adaptive rules.

### Domain policy

Для domain Susanin использует RouterOS DNS:

- static FWD;
- `match-subdomain=yes`;
- `address-list=vpn_direct`.

Поэтому RouterOS должен **видеть DNS-запрос клиента**.

Если клиент использует внешний DoH/DoT/private DNS или hardcoded IP,
RouterOS может не узнать адреса домена.

TLS SNI inspection в v0.12.0 отсутствует.

## Accuracy profiles

Доступны:

~~~text
fast
middle
slow
~~~

Просмотр текущей конфигурации:

~~~text
susanin config show
~~~

Изменение профиля:

~~~text
susanin config set accuracy-profile fast
susanin config set accuracy-profile middle
susanin config set accuracy-profile slow
~~~

`fast` — reference profile финального field acceptance v0.12.0.

## Требования

Проверенная конфигурация stable v0.12.0:

- MikroTik с поддержкой Containers;
- ARM64;
- RouterOS 7.23.3 stable;
- IPv4;
- interface-list `LAN`;
- существующий route-based VPN/tunnel;
- доступный RouterOS container storage;
- загруженные `susanin.tar` и `install.rsc`.

На reference router использовались:

~~~text
LAN:
bridge-LAN
192.168.1.1/24

VPN:
wg-awg-proxy

Routing table:
r_to_awg
~~~

Другие RouterOS версии и архитектуры могут работать, но пока не входят в
официально проверенный stable profile.

### IPv6

v0.12.0 — строго IPv4-only.

IPv6 adaptive routing в этот релиз не входит.

## Быстрый старт

### 1. Сделайте backup RouterOS

Перед установкой сохраните рабочую конфигурацию и убедитесь, что знаете,
как восстановить роутер.

### 2. Скачайте release

Скачайте:

~~~text
susanin.tar
install.rsc
~~~

Рекомендуется также скачать:

~~~text
SHA256SUMS
uninstall.rsc
uninstall-controller.rsc
~~~

Проверьте SHA256:

~~~bash
sha256sum -c SHA256SUMS
~~~

### 3. Загрузите файлы в MikroTik

Загрузите `susanin.tar` и `install.rsc` через WinBox/WebFig Files.

Имена должны остаться именно:

~~~text
susanin.tar
install.rsc
~~~

### 4. Опционально проверьте parser

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

### 5. Запустите bootstrap

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Bootstrap:

- создаст изолированный controller network;
- создаст restricted `susanin-agent`;
- сгенерирует machine secret;
- проверит secret после записи;
- подключит mounts;
- распакует `susanin.tar`;
- запустит `susanin-controller`;
- удалит временные bootstrap helpers.

Пользовательский RouterOS API пароль вводить не требуется.

### 6. Дождитесь RUNNING

~~~routeros
/container print where name="susanin-controller"
~~~

Нужен флаг:

~~~text
R
~~~

### 7. Запустите setup

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin setup" \
    no-sh \
    timeout=300
~~~

Setup предложит выбрать routing target:

~~~text
1) Interface
2) Routing table
~~~

Если у вас уже есть отдельная таблица маршрутизации для VPN, обычно выбирайте
**Routing table**.

Если используется просто отдельный route-based интерфейс без готовой policy
routing схемы — можно выбрать **Interface**.

После выбора Susanin:

- валидирует generated RouterOS source;
- создаёт data plane;
- создаёт schedulers;
- очищает несовместимый legacy runtime state;
- запускает adaptive routing.

Reference fresh install:

~~~text
Validation summary: PASS=4 FAIL=0

Fresh install result: SUCCESS
scripts=4 schedulers=4 mangle=8 safety=3
~~~

## Проверка после установки

### Version

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin version" \
    no-sh \
    timeout=30
~~~

Нормально:

~~~text
Susanin 0.12.0
~~~

### Status

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin status" \
    no-sh \
    timeout=60
~~~

Reference state:

~~~text
scripts=4/4
schedulers=4/4
fixed-mangle=8/8
fixed-duplicates=0

Installation state: detected
Adaptive migration state: clean
~~~

Количество dynamic lazy per-port rules может меняться во время работы.

### Structural reconciliation

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin apply --dry-run" \
    no-sh \
    timeout=60
~~~

Нормальный результат:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## Основные команды

Все команды ниже выполняются внутри `susanin-controller`.

### Discovery

~~~text
susanin discover
~~~

### Status

~~~text
susanin status
~~~

### Routing target

~~~text
susanin target show
susanin target list

susanin target set interface <name>
susanin target set routing-table <name>
~~~

После изменения routing target проверьте desired/production state и следуйте
процедуре из полного руководства.

### VPN Direct

~~~text
susanin direct list

susanin direct add ip <IPv4[/prefix]>
susanin direct add domain <domain>

susanin direct remove ip <IPv4[/prefix]>
susanin direct remove domain <domain>

susanin direct sync
~~~

### Configuration

~~~text
susanin config show
susanin config set accuracy-profile fast|middle|slow
susanin config set log-level quiet|error|info|debug|trace
susanin config set diagnostics on|off
~~~

### Validation

~~~text
susanin plan
susanin render
susanin validate
susanin snapshot
susanin apply --dry-run
~~~

### Safe data-plane update

~~~text
susanin stage
susanin promote --dry-run
susanin promote
susanin rollback
susanin stage-clean
~~~

Не запускайте `promote` вслепую. См. [docs/UPGRADE.md](docs/UPGRADE.md).

### Diagnostics

~~~text
susanin diag status
susanin diag start
susanin diag sample
susanin diag errors
susanin diag stop
~~~

Подробнее: [docs/LOGGING.md](docs/LOGGING.md).

## Fail-open

Если выбранный VPN/tunnel становится недоступен, Susanin должен сохранить
обычный доступ пользователей к сети.

HEALTH переводит managed adaptive routing в DIRECT fallback.

После восстановления VPN adaptive routing автоматически возвращается.

## Port-aware state

v0.12.0 различает:

~~~text
tcp + destination IP + destination port
udp + destination IP + destination port
~~~

Пример:

~~~text
tcp + 203.0.113.10 + 443
udp + 203.0.113.10 + 443
tcp + 203.0.113.10 + 8443
~~~

Это три разных adaptive состояния.

Это важно для CDN и серверов, где один IP обслуживает разные сервисы.

## Dynamic cache и GC

Adaptive state временный.

Susanin не строит постоянную глобальную базу заблокированных сайтов.

Во время работы появляются:

- temporary watch/test/ok/cooldown state;
- lazy per-port mangle rules;
- connection marks.

Runtime GC ограничивает накопление Susanin-owned динамического состояния.

## Controller можно остановить

Data plane живёт в RouterOS независимо от controller.

Штатная остановка:

~~~routeros
/container stop [find where name="susanin-controller"]
~~~

v0.12.0 корректно обрабатывает SIGTERM/SIGINT.

При штатной остановке ожидается сообщение:

~~~text
Susanin controller stopping gracefully.
~~~

Установленные RouterOS scripts/schedulers при этом остаются.

## Обновление

Не заменяйте data plane вручную.

Используйте:

[docs/UPGRADE.md](docs/UPGRADE.md)

Susanin поддерживает:

- render/validate;
- structural dry-run;
- inert stage;
- promotion dry-run;
- transactional promotion;
- rollback.

## Удаление

### Полностью удалить Susanin

~~~routeros
/import file-name=uninstall.rsc verbose=yes
~~~

Удаляются:

- controller;
- Susanin bridge/VETH;
- machine user/group;
- mounts;
- Susanin API rule;
- adaptive scripts;
- schedulers;
- Susanin-owned mangle/address-list state;
- VPN Direct state;
- Susanin config;
- machine secret.

Выбранный пользователем VPN/tunnel **не удаляется**.

Независимая routing table или NAT также не должны удаляться, если они не были
созданы Susanin.

### Удалить только controller

~~~routeros
/import file-name=uninstall-controller.rsc verbose=yes
~~~

Этот вариант сохраняет установленный RouterOS adaptive data plane.

## Безопасность bootstrap

Susanin не просит пользователя вводить RouterOS API credentials.

Bootstrap создаёт отдельную локальную machine identity:

~~~text
susanin-agent
~~~

Controller получает доступ к RouterOS API только через изолированную
controller network.

Machine secret:

- генерируется автоматически;
- имеет случайное значение;
- хранится в mounted file;
- не передаётся через container environment;
- не должен публиковаться в Issues или логах.

Не выполняйте команды, печатающие contents secret-файла.

Для безопасной проверки достаточно metadata, например размера файла.

Подробнее: [SECURITY.md](SECURITY.md).

## Что Susanin не делает

Susanin:

- не создаёт сам VPN;
- не является WireGuard/AmneziaWG implementation;
- не проксирует трафик через контейнер;
- не выполняет DPI;
- не выполняет TLS SNI inspection;
- не отправляет пользовательскую телеметрию во внешний сервис;
- не поддерживает IPv6 adaptive routing в v0.12.0.

## Проверенный stable scope

Финальная acceptance v0.12.0 включает:

~~~text
ARM64_RUNTIME=PASS
API_AUTH=PASS
FRESH_BOOTSTRAP=PASS
FRESH_SETUP=PASS
STRUCTURAL_SYNC=PASS

TABLE_NATIVE_TARGET=PASS

VPN_DIRECT_IPV4=PASS
VPN_DIRECT_DOMAIN=PASS
VPN_DIRECT_PRIORITY=PASS

GRACEFUL_SIGTERM=PASS
RESTART_AFTER_SIGTERM=PASS
DATA_PLANE_PRESERVED=PASS

FULL_UNINSTALL=PASS
API_STATE_RESTORED=PASS

INDEPENDENT_AWG_PRESERVED=PASS
INDEPENDENT_ROUTE_PRESERVED=PASS
INDEPENDENT_NAT_PRESERVED=PASS

FRESH_REINSTALL=PASS
GC_IDENTICAL=PASS
~~~

Подробная матрица:
[docs/TESTED.md](docs/TESTED.md).

## Release integrity

v0.12.0 использует frozen field-tested container artifact.

Runtime source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

`susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f
~~~

Stable tag не запускает автоматическую пересборку release image.

Подробнее:
[docs/RELEASE_INTEGRITY.md](docs/RELEASE_INTEGRITY.md).

## Документация

Основные документы:

- [Полное руководство](docs/USER_GUIDE.md)
- [Архитектура](docs/ARCHITECTURE.md)
- [Обновление](docs/UPGRADE.md)
- [Логирование и диагностика](docs/LOGGING.md)
- [Проверенные сценарии](docs/TESTED.md)
- [Release Notes v0.12.0](docs/RELEASE_NOTES_v0.12.0.md)
- [Release integrity](docs/RELEASE_INTEGRITY.md)
- [Security policy](SECURITY.md)
- [Changelog](CHANGELOG.md)

Development/acceptance документы в `docs/` сохранены как исторические
технические evidence.

## Сборка из исходников

Локальная сборка:

~~~bash
make clean
make
~~~

Container image:

~~~bash
docker build -t susanin:dev .
~~~

Эта сборка предназначена для разработки.

Она **не является** exact stable v0.12.0 release artifact.

Stable artifact identity указан в
[docs/RELEASE_INTEGRITY.md](docs/RELEASE_INTEGRITY.md).

## Проект

Susanin разрабатывался как практический инструмент для реальной RouterOS
инфраструктуры.

Большая часть поведения проверялась итеративно на настоящем MikroTik,
включая установку, migration, VPN Direct, остановку controller, uninstall и
чистую повторную установку.

Разработка велась с активным использованием ChatGPT.

Проект не связан и не аффилирован с MikroTik, Amnezia, WireGuard, OpenAI или
авторами упомянутых сторонних проектов.

## Лицензия

MIT — см. [LICENSE](LICENSE).

# Susanin v0.12.0 — руководство пользователя

Susanin — control plane для адаптивной маршрутизации в MikroTik RouterOS.

Он наблюдает за поведением соединений, проверяет проблемные направления через
уже существующий VPN/tunnel и временно запоминает рабочий путь.

Susanin не является VPN-клиентом и не создаёт WireGuard/AmneziaWG tunnel
за пользователя.

Перед установкой или обновлением обязательно сделайте backup RouterOS.

## Поддерживаемый stable profile

Финальный v0.12.0 проверен на:

- ARM64;
- RouterOS 7.23.3 stable;
- IPv4;
- interface-list `LAN`;
- существующем route-based VPN/tunnel;
- WireGuard/AmneziaWG reference path.

Другие версии RouterOS, архитектуры и VPN реализации могут работать, но не
входят в подтверждённую stable acceptance v0.12.0.

## Что изменилось в v0.12.0

Главные изменения относительно v0.11.5:

- routing target `Interface` или `Routing table`;
- table-native routing;
- VPN Direct;
- port-aware adaptive identity;
- lazy per-port mangle;
- profiles `fast`, `middle`, `slow`;
- migration hardening;
- bounded runtime GC;
- strict IPv4-only mode;
- graceful controller shutdown.

Adaptive identity:

~~~text
protocol + destination IPv4 + destination port
~~~

Например:

~~~text
tcp + 203.0.113.10 + 443
udp + 203.0.113.10 + 443
tcp + 203.0.113.10 + 8443
~~~

Это три независимых adaptive состояния.

# 1. Архитектура

Susanin разделён на control plane и data plane.

## 1.1. Controller

Контейнер `susanin-controller` выполняет:

- discovery;
- setup;
- target selection;
- rendering;
- validation;
- installation;
- structural reconciliation;
- configuration;
- VPN Direct policy management;
- diagnostics;
- stage/promote/rollback;
- runtime GC.

Пользовательский трафик через controller не проходит.

## 1.2. RouterOS data plane

После установки постоянно работают четыре RouterOS scripts:

~~~text
auto-awg-health
auto-awg-fast
auto-awg-detect
auto-awg-judge
~~~

Reference scheduler cadence:

~~~text
auto-awg-health    3s
auto-awg-fast      1s
auto-awg-detect    2s
auto-awg-judge     1s
~~~

Fixed mangle reference:

~~~text
8 adaptive rules
3 safety bypass rules
~~~

Dynamic per-port rules создаются лениво, поэтому их количество во время
работы может изменяться.

## 1.3. Почему data plane находится в RouterOS

Такое разделение позволяет:

- не гонять connection tracking через API каждую секунду;
- сохранить маршрутизацию при перезапуске controller;
- обновлять controller отдельно от data plane;
- выполнять adaptive routing непосредственно рядом с firewall/conntrack.

Остановка controller сама по себе не удаляет data plane.

# 2. Как принимается решение

Упрощённо:

~~~text
обычный DIRECT connection
        |
        v
наблюдение RouterOS conntrack
        |
        v
подозрительное поведение
        |
        v
TEST через выбранный VPN target
        |
        +--> через VPN работает --> OK
        |
        +--> через VPN тоже плохо --> COOLDOWN / DIRECT
~~~

TCP и UDP оцениваются независимо.

## 2.1. FAST

FAST ищет быстро распознаваемые признаки проблемы:

- TCP SYN без нормального ответа;
- ранний TCP failure;
- QUIC/UDP no-reply.

FAST не означает окончательное подтверждение VPN.

Он создаёт основание для проверки через выбранный target.

## 2.2. DETECT

DETECT ищет менее очевидные признаки:

- TCP stall;
- late stall;
- UDP no-reply;
- QUIC late stall.

Для части состояний используется WATCH/debounce.

## 2.3. JUDGE

JUDGE оценивает результат попытки через VPN target.

Успех:

~~~text
TEST -> OK
~~~

Неудача:

~~~text
TEST -> COOLDOWN -> DIRECT
~~~

## 2.4. HEALTH

HEALTH следит за работоспособностью выбранного target.

При проблеме adaptive routing переводится в fail-open:

~~~text
VPN problem
    |
    v
managed adaptive routing disabled
    |
    v
traffic uses normal DIRECT path
~~~

После восстановления target managed adaptive routing включается снова.


# 3. Routing target

Susanin v0.12.0 поддерживает два типа routing target:

~~~text
Interface
Routing table
~~~

Выбор выполняется во время `setup` или позже через CLI.

## 3.1. Interface

В этом режиме выбирается конкретный route-based интерфейс.

Пример:

~~~text
wg-vpn
~~~

Команда:

~~~text
susanin target set interface wg-vpn
~~~

Susanin использует выбранный интерфейс как VPN egress.

Если подходящей отдельной routing table нет, Susanin может создать
собственную FIB table и default route через выбранный интерфейс.

Этот режим удобен для простой конфигурации:

~~~text
LAN
 |
 v
Susanin policy routing
 |
 v
один VPN interface
~~~

## 3.2. Routing table

В этом режиме выбирается уже существующая RouterOS routing table.

Пример:

~~~text
r_to_awg
~~~

Команда:

~~~text
susanin target set routing-table r_to_awg
~~~

Также поддерживается сокращённая форма:

~~~text
susanin target set table r_to_awg
~~~

Susanin не пытается заменить внутреннюю логику таблицы одним
gateway/interface.

Это позволяет оставить в существующей RouterOS-конфигурации:

- recursive routing;
- ECMP;
- несколько egress;
- собственный failover;
- пользовательские route distances;
- сложные next-hop схемы.

Reference acceptance v0.12.0 использовала:

~~~text
routing table:
r_to_awg

resolved egress:
wg-awg-proxy
~~~

## 3.3. NAT в routing-table mode

Для target типа `routing-table` Susanin не создаёт автоматически NAT rule:

~~~text
SUSANIN: masquerade selected tunnel
~~~

В таком режиме NAT считается частью внешней routing/VPN инфраструктуры.

Например, в reference acceptance уже существовал:

~~~text
AWG selected traffic masquerade
~~~

и Susanin его не изменял.

## 3.4. Просмотр текущего target

~~~text
susanin target show
~~~

Список доступных targets:

~~~text
susanin target list
~~~

После ручного изменения target выполните:

~~~text
susanin discover
susanin validate
susanin apply --dry-run
susanin direct sync
~~~

Если `apply --dry-run` показывает UPDATE, применяйте изменение через
safe stage/promote procedure.

# 4. VPN Direct

VPN Direct — persistent policy для явной маршрутизации.

Его смысл:

> указанный IP, CIDR или domain должен быть принудительно направлен через
> выбранный VPN target независимо от adaptive learning.

VPN Direct не означает DIRECT bypass.

В финальной v0.12.0 это именно **force selected VPN target**.

## 4.1. Посмотреть policy

~~~text
susanin direct list
~~~

Если policy пустая:

~~~text
=== SUSANIN VPN DIRECT ===

Policy is empty.
~~~

## 4.2. Добавить IPv4

~~~text
susanin direct add ip 1.1.1.1
~~~

Можно использовать явный `/32`:

~~~text
susanin direct add ip 1.1.1.1/32
~~~

## 4.3. Добавить IPv4 CIDR

~~~text
susanin direct add ip 203.0.113.0/24
~~~

## 4.4. Удалить IP/CIDR

~~~text
susanin direct remove ip 1.1.1.1/32
~~~

## 4.5. Добавить domain

~~~text
susanin direct add domain example.com
~~~

## 4.6. Удалить domain

~~~text
susanin direct remove domain example.com
~~~

## 4.7. Синхронизировать policy

~~~text
susanin direct sync
~~~

Эта команда приводит RouterOS VPN Direct objects в соответствие с
persistent Susanin policy.

## 4.8. RouterOS objects VPN Direct

Для IP/CIDR используется RouterOS address-list:

~~~text
vpn_direct
~~~

Главное mangle правило имеет comment:

~~~text
SUSANIN: VPN Direct bypass
~~~

Но историческое слово `bypass` в comment не описывает финальную семантику.

Фактическое действие v0.12.0:

~~~text
action=mark-routing
new-routing-mark=<selected routing table>
passthrough=no
~~~

VPN Direct rule располагается раньше Susanin safety/adaptive rules.

Поэтому explicit policy пользователя имеет приоритет.

## 4.9. Domain policy

Для domain создаётся RouterOS DNS static FWD entry.

Используются:

~~~text
match-subdomain=yes
address-list=vpn_direct
~~~

Когда RouterOS видит DNS-запрос, полученные IPv4 могут автоматически
попадать в динамический `vpn_direct`.

## 4.10. Ограничение DNS visibility

Domain policy работает только тогда, когда RouterOS видит DNS lookup.

Проблемы возможны, если клиент использует:

- внешний DoH;
- внешний DoT;
- private DNS;
- собственный DNS через другой tunnel;
- hardcoded IP.

В таких случаях RouterOS может не узнать IP, соответствующий domain.

TLS SNI inspection в stable v0.12.0 отсутствует.

# 5. Accuracy profiles

Susanin поддерживает три accuracy profile:

~~~text
fast
middle
slow
~~~

Текущий профиль:

~~~text
susanin config show
~~~

Установить FAST:

~~~text
susanin config set accuracy-profile fast
~~~

Установить MIDDLE:

~~~text
susanin config set accuracy-profile middle
~~~

Установить SLOW:

~~~text
susanin config set accuracy-profile slow
~~~

Общая идея:

- `fast` — быстрее реагирует;
- `middle` — более сбалансированный режим;
- `slow` — более осторожный режим.

Final field acceptance v0.12.0 выполнялась с:

~~~text
accuracy-profile=fast
~~~

После смены профиля проверьте desired data plane:

~~~text
susanin validate
susanin apply --dry-run
~~~

Если появился UPDATE:

~~~text
susanin stage
susanin promote --dry-run
susanin promote
~~~


# 6. Установка

## 6.1. Сделайте backup RouterOS

Перед установкой Susanin сохраните рабочую конфигурацию MikroTik.

Установка меняет RouterOS:

- firewall mangle;
- schedulers;
- system scripts;
- API access;
- container infrastructure;
- address lists.

Поэтому backup и понятный rollback path обязательны.

## 6.2. Скачайте stable release

Для обычной установки нужны:

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

Stable v0.12.0 содержит ровно пять release assets:

~~~text
susanin.tar
install.rsc
uninstall.rsc
uninstall-controller.rsc
SHA256SUMS
~~~

## 6.3. Проверьте SHA256

На Linux:

~~~bash
sha256sum -c SHA256SUMS
~~~

Для финального v0.12.0 ожидается:

~~~text
susanin.tar: OK
install.rsc: OK
uninstall.rsc: OK
uninstall-controller.rsc: OK
~~~

Exact accepted `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f
~~~

Подробнее:

[RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

## 6.4. Загрузите файлы на MikroTik

Через WinBox или WebFig откройте `Files`.

Загрузите:

~~~text
susanin.tar
install.rsc
~~~

Не переименовывайте файлы.

Bootstrap ожидает image с именем:

~~~text
susanin.tar
~~~

## 6.5. Parser dry-run

Перед реальной установкой рекомендуется:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Dry-run должен завершиться без syntax errors.

Он не должен устанавливать production Susanin objects.

## 6.6. Запустите bootstrap

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Bootstrap не запрашивает у пользователя RouterOS API username/password.

Он автоматически:

1. создаёт `veth-susanin`;
2. создаёт `bridge-susanin`;
3. создаёт isolated controller network;
4. настраивает RouterOS API для controller;
5. создаёт restricted group `susanin-agent`;
6. запускает временный bootstrap worker;
7. генерирует random machine secret;
8. записывает и проверяет secret;
9. синхронизирует machine user `susanin-agent`;
10. создаёт secret/data mounts;
11. распаковывает `susanin.tar`;
12. запускает `susanin-controller`;
13. удаляет временные bootstrap helpers.

## 6.7. Дождитесь завершения extraction

Проверка:

~~~routeros
/container print detail where name="susanin-controller"
~~~

Во время распаковки возможен статус:

~~~text
E
~~~

Это означает:

~~~text
DOWNLOADING/EXTRACTING
~~~

В этот момент `tag`, `arch` и `image-id` могут быть ещё пустыми.

Не запускайте `setup`, пока container не станет RUNNING.

## 6.8. Нормальное состояние controller

После завершения bootstrap ожидается:

~~~text
name="susanin-controller"
tag="0.12.0"
arch="arm64"
root-dir=/susanin-controller-v0120
start-on-boot=yes
~~~

Финальный accepted image-id:

~~~text
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82
~~~

Временные bootstrap helpers должны исчезнуть.

Проверка:

~~~routeros
/system scheduler print count-only where name~"susanin-bootstrap-"
/system script print count-only where name~"susanin-bootstrap-"
~~~

После успешного bootstrap ожидается:

~~~text
0
0
~~~

## 6.9. Запустите first-run setup

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin setup" \
    no-sh \
    timeout=300
~~~

Setup сначала проверит RouterOS API authentication.

Нормально:

~~~text
Connecting...
Authenticated.
~~~

Затем предложит:

~~~text
Choose routing target type:
  1) Interface
  2) Routing table
~~~

## 6.10. Выбор Interface

Если хотите использовать конкретный route-based interface:

~~~text
Selection: 1
~~~

После этого выберите нужный интерфейс из списка.

## 6.11. Выбор Routing table

Если у вас уже есть отдельная RouterOS routing table для VPN:

~~~text
Selection: 2
~~~

Затем выберите таблицу.

Reference acceptance:

~~~text
Choose routing table:
  1) r_to_awg

Selection: 1
~~~

После выбора было сохранено:

~~~text
mode:
routing-table

table:
r_to_awg

egress:
wg-awg-proxy
~~~

Egress в table-native mode может быть informational.

Фактическую маршрутизацию определяет выбранная RouterOS routing table.

## 6.12. Generated source validation

До commit Susanin валидирует четыре generated RouterOS scripts.

Reference v0.12.0:

~~~text
PASS   auto-awg-health
PASS   auto-awg-fast
PASS   auto-awg-detect
PASS   auto-awg-judge

Validation summary: PASS=4 FAIL=0
~~~

Если validation не проходит, production data plane не должен считаться
успешно установленным.

## 6.13. Fresh install result

Reference результат:

~~~text
Fresh install result: SUCCESS
scripts=4 schedulers=4 mangle=8 safety=3
~~~

Для routing-table target:

~~~text
tunnel NAT=UNMANAGED
~~~

Это ожидаемо.

## 6.14. Persistent configuration

После setup persistent non-secret configuration хранится в:

~~~text
/data/susanin.conf
~~~

Machine secret хранится отдельно через mounted secret file.

Не публикуйте contents secret-файла.

# 7. Проверка после установки

## 7.1. Version

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin version" \
    no-sh \
    timeout=30
~~~

Ожидается:

~~~text
Susanin 0.12.0
~~~

## 7.2. Discovery

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin discover" \
    no-sh \
    timeout=60
~~~

Проверьте:

- `Authenticated.`;
- RouterOS version;
- architecture;
- LAN interface-list;
- LAN IPv4;
- target mode;
- routing table;
- ожидаемый egress.

Для routing-table reference:

~~~text
Target: routing-table r_to_awg
Egress interface: wg-awg-proxy
Routing table: r_to_awg
~~~

## 7.3. Status

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin status" \
    no-sh \
    timeout=60
~~~

Reference stable state:

~~~text
Scripts:
4/4

Schedulers:
4/4

Fixed mangle:
8/8

fixed duplicates:
0

Installation state:
detected

Adaptive migration state:
clean
~~~

Количество dynamic lazy per-port rules может быть больше нуля.

Это нормально и зависит от текущего трафика.

## 7.4. Structural reconciliation

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin apply --dry-run" \
    no-sh \
    timeout=60
~~~

Команда read-only.

Нормальный результат:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 7.5. VPN Direct после fresh install

Проверка:

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin direct list" \
    no-sh \
    timeout=30
~~~

Если пользователь ещё не добавлял policy:

~~~text
Policy is empty.
~~~

При пустой policy не должно оставаться активных VPN Direct objects от старой
установки.


# 8. Port-aware runtime state

v0.12.0 использует port-aware adaptive identity:

~~~text
protocol + destination IPv4 + destination port
~~~

То есть один destination IP больше не является одной общей adaptive записью.

Например:

~~~text
tcp + 203.0.113.10 + 443
udp + 203.0.113.10 + 443
tcp + 203.0.113.10 + 8443
~~~

считаются тремя разными состояниями.

Это важно для:

- CDN;
- shared hosting;
- серверов с несколькими сервисами;
- QUIC/HTTP3;
- адресов, где TCP и UDP ведут себя по-разному.

## 8.1. TCP и UDP разделены

Пример допустимого состояния:

~~~text
tcp/443 -> OK
udp/443 -> TEST
tcp/8443 -> COOLDOWN
~~~

Susanin не должен автоматически считать все протоколы и порты одного IP
одинаково работающими.

## 8.2. Runtime buckets

Во время работы status может показывать:

~~~text
TCP buckets: ok=N test=N watch=N cooldown=N
TCP entries: ok=N test=N watch=N cooldown=N

UDP buckets: ok=N test=N watch=N cooldown=N
UDP entries: ok=N test=N watch=N cooldown=N
~~~

Эти значения зависят от текущего трафика.

## 8.3. Legacy IP-only state

После migration старые несовместимые IP-only entries не должны оставаться
активной частью v0.12.0 data plane.

Нормальный status:

~~~text
legacy IP-only entries=0
Adaptive migration state: clean
~~~

# 9. Lazy per-port mangle

Susanin не создаёт правила для всех возможных TCP/UDP портов заранее.

Per-port AUTO-AWG rules появляются лениво только тогда, когда соответствующее
adaptive состояние реально возникло.

Поэтому status может показывать:

~~~text
fixed-mangle=8/8
lazy-mangle=4
~~~

или другое значение `lazy-mangle`.

Это нормально.

## 9.1. Fixed и dynamic rules

Fixed rules:

~~~text
8
~~~

участвуют в structural reconciliation.

Dynamic lazy rules:

~~~text
N
~~~

относятся к runtime state.

Поэтому:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
~~~

может одновременно существовать с ненулевым `lazy-mangle`.

Это не structural drift.

# 10. Runtime GC

Dynamic adaptive state не должен накапливаться бесконечно.

Susanin runtime GC обслуживает принадлежащие Susanin transient objects:

- port-aware lists;
- устаревшие runtime entries;
- stale lazy per-port rules;
- связанное временное adaptive state.

Ручной запуск:

~~~text
susanin gc
~~~

Controller daemon также выполняет GC периодически.

GC не предназначен для удаления независимых:

- VPN interfaces;
- пользовательских routing tables;
- пользовательских routes;
- пользовательского NAT.

Final v0.12.0 использует отдельно принятую и протестированную GC
реализацию без изменений во время финального release hardening.

# 11. Основные CLI-команды

Все команды ниже выполняются внутри `susanin-controller`.

Например, из RouterOS:

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin status" \
    no-sh \
    timeout=60
~~~

В обычном shell внутри container используется короткая форма:

~~~text
susanin status
~~~

## 11.1. version

~~~text
susanin version
~~~

Показывает версию runtime.

Для stable:

~~~text
Susanin 0.12.0
~~~

## 11.2. discover

~~~text
susanin discover
~~~

Read-only discovery RouterOS.

Показывает:

- RouterOS version;
- board;
- architecture;
- LAN;
- interfaces;
- routing tables;
- текущий target;
- resolved egress.

## 11.3. plan

~~~text
susanin plan
~~~

Read-only анализ состояния перед установкой/изменением.

## 11.4. status

~~~text
susanin status
~~~

Показывает:

- routing target;
- egress;
- scripts;
- schedulers;
- fixed mangle;
- port-aware state;
- lazy rules;
- migration state.

## 11.5. apply --dry-run

~~~text
susanin apply --dry-run
~~~

Read-only structural reconciliation.

Production RouterOS objects не изменяются.

Reference result:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 11.6. snapshot

~~~text
susanin snapshot
~~~

Показывает fingerprints фактических production scripts.

## 11.7. render

~~~text
susanin render
~~~

Генерирует desired RouterOS source без commit в production.

## 11.8. validate

~~~text
susanin validate
~~~

Проверяет generated source через временные RouterOS validator objects.

Reference result:

~~~text
PASS=4 FAIL=0
Production scripts changed: NO
~~~

# 12. Installation CLI

## 12.1. setup

~~~text
susanin setup
~~~

Используется при первой настройке.

Setup:

- обнаруживает RouterOS;
- предлагает routing target;
- сохраняет configuration;
- валидирует generated data plane;
- устанавливает или reconciles RouterOS objects;
- синхронизирует VPN Direct.

## 12.2. install --dry-run

~~~text
susanin install --dry-run
~~~

Проверяет возможность установки без production commit.

## 12.3. install

~~~text
susanin install
~~~

Устанавливает/reconciles managed RouterOS data plane.

Для обычного первого запуска предпочтителен:

~~~text
susanin setup
~~~

# 13. Routing target CLI

## 13.1. target show

~~~text
susanin target show
~~~

Показывает текущий target.

## 13.2. target list

~~~text
susanin target list
~~~

Показывает доступные targets.

## 13.3. target set interface

~~~text
susanin target set interface <name>
~~~

Пример:

~~~text
susanin target set interface wg-awg-proxy
~~~

## 13.4. target set routing-table

~~~text
susanin target set routing-table <name>
~~~

Пример:

~~~text
susanin target set routing-table r_to_awg
~~~

Также поддерживается:

~~~text
susanin target set table r_to_awg
~~~

# 14. VPN Direct CLI

Полный набор команд:

~~~text
susanin direct list

susanin direct add ip <IPv4[/prefix]>
susanin direct add domain <domain>

susanin direct remove ip <IPv4[/prefix]>
susanin direct remove domain <domain>

susanin direct sync
~~~

После ручного изменения routing target рекомендуется выполнить:

~~~text
susanin direct sync
~~~

чтобы RouterOS VPN Direct routing mark соответствовал текущему target.

# 15. Runtime configuration CLI

## 15.1. config show

~~~text
susanin config show
~~~

Показывает persistent non-secret configuration.

## 15.2. Accuracy profile

~~~text
susanin config set accuracy-profile fast
susanin config set accuracy-profile middle
susanin config set accuracy-profile slow
~~~

## 15.3. Log level

~~~text
susanin config set log-level quiet
susanin config set log-level error
susanin config set log-level info
susanin config set log-level debug
susanin config set log-level trace
~~~

## 15.4. Diagnostics flag

~~~text
susanin config set diagnostics on
susanin config set diagnostics off
~~~

После изменения configuration, влияющей на generated RouterOS source:

~~~text
susanin validate
susanin apply --dry-run
~~~

Если появился structural UPDATE, не заменяйте production scripts вручную.

Используйте safe upgrade procedure.


# 16. Safe data-plane upgrade

Обновление Susanin controller и обновление RouterOS data plane — это две
разные операции.

Замена container сама по себе не означает, что production RouterOS scripts
нужно немедленно заменять.

После обновления controller сначала выполните:

~~~text
susanin version
susanin discover
susanin validate
susanin apply --dry-run
~~~

Если результат:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

production data plane уже соответствует новому controller.

Если `apply --dry-run` показывает UPDATE, используйте staged promotion.

## 16.1. stage

~~~text
susanin stage
~~~

Создаются inert temporary scripts:

~~~text
susanin-stage-health
susanin-stage-fast
susanin-stage-detect
susanin-stage-judge
~~~

Они не заменяют production data plane.

Цель stage:

- создать новый desired source;
- проверить его отдельно;
- не вмешиваться в работающие production schedulers.

## 16.2. promote --dry-run

Перед promotion:

~~~text
susanin promote --dry-run
~~~

Команда проверяет safety gates без commit.

Не запускайте реальный `promote`, если dry-run показывает blocker.

## 16.3. promote

После успешного dry-run:

~~~text
susanin promote
~~~

Promotion выполняется транзакционно.

Общая последовательность:

1. snapshot текущего production;
2. создание rollback backup;
3. pause managed schedulers;
4. ожидание завершения active jobs;
5. замена production source;
6. проверка fingerprints;
7. восстановление scheduler state.

После promotion:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

Нормальный structural result:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 16.4. rollback

Если promotion оказался неудачным:

~~~text
susanin rollback
~~~

После rollback обязательно:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

## 16.5. stage-clean

Удаление inert stage objects:

~~~text
susanin stage-clean
~~~

Полная upgrade procedure:

[UPGRADE.md](UPGRADE.md)

# 17. Diagnostics

Susanin содержит встроенный diagnostic recorder.

Подробное описание:

[LOGGING.md](LOGGING.md)

## 17.1. diag status

~~~text
susanin diag status
~~~

Показывает состояние diagnostic recorder.

## 17.2. diag start

~~~text
susanin diag start
~~~

Включает diagnostic recording.

Диагностические данные хранятся внутри persistent `/data`.

## 17.3. diag sample

~~~text
susanin diag sample
~~~

Собирает диагностический snapshot.

В зависимости от runtime он может включать:

- RouterOS version;
- board;
- architecture;
- uptime;
- CPU state;
- memory state;
- connection-tracking counters;
- script jobs;
- managed Susanin workers;
- controller state.

## 17.4. diag errors

~~~text
susanin diag errors
~~~

Используется для сбора RouterOS script-error information.

В частности, помогает увидеть повторяющиеся ошибки RouterOS scripts и
связать их с текущим состоянием data plane.

## 17.5. diag stop

~~~text
susanin diag stop
~~~

Останавливает diagnostic recording.

# 18. Что собрать перед Bug Issue

Перед созданием технического Issue рекомендуется выполнить:

~~~text
susanin diag start
~~~

Затем воспроизвести проблему.

После этого:

~~~text
susanin diag sample
susanin diag errors
susanin status
susanin apply --dry-run
susanin target show
susanin config show
susanin direct list
susanin diag stop
~~~

В описании Issue полезно указать:

- Susanin version;
- RouterOS version;
- architecture;
- MikroTik board;
- target mode;
- target name;
- accuracy profile;
- используется ли VPN Direct;
- ожидаемое поведение;
- фактическое поведение;
- минимальные шаги воспроизведения.

Если проблема касается маршрутизации, также полезно указать:

- protocol;
- destination IP;
- destination port;
- работает ли тот же destination через DIRECT;
- работает ли он через выбранный VPN вручную.

Не публикуйте секретные данные ради диагностики.

# 19. Безопасность и секреты

Susanin bootstrap не требует ввода пользовательского RouterOS API password.

Для controller автоматически создаётся отдельная machine identity:

~~~text
susanin-agent
~~~

Controller network изолирована от обычного LAN.

## 19.1. Machine secret

Bootstrap генерирует random machine secret.

Он хранится в mounted file:

~~~text
susanin-secrets/routeros_password
~~~

Внутри container он доступен через secret mount.

Secret не должен передаваться:

- через container environment;
- через command-line arguments;
- через публичные diagnostics;
- через GitHub Issues.

## 19.2. Что нельзя публиковать

Никогда не прикладывайте публично:

- RouterOS `.backup`;
- `/export show-sensitive`;
- contents `susanin-secrets/routeros_password`;
- WireGuard private key;
- AmneziaWG private key;
- VPN password;
- RouterOS API password;
- другие credentials или private keys.

## 19.3. Не печатайте contents machine secret

Для проверки secret-файла не используйте команды, показывающие его
`contents`.

Не используйте широкие `/file print detail` запросы по Susanin-файлам:
RouterOS может вывести содержимое machine-secret файла вместе с metadata.

Безопаснее проверять только metadata.

Пример:

~~~routeros
:do {
    :local f [/file find where name="susanin-secrets/routeros_password"]

    :if ([:len $f] != 1) do={
        :error "machine secret missing"
    }

    :put ("secret-size=" . [/file get $f size])
}
~~~

Для stable bootstrap ожидается:

~~~text
secret-size=48
~~~

## 19.4. Если secret был опубликован

Если содержимое machine secret случайно попало:

- в терминальный transcript;
- в Issue;
- в screenshot;
- в публичный лог;

считайте этот secret скомпрометированным.

Не продолжайте использовать опубликованное значение.

Machine credential должен быть ротирован.

## 19.5. RouterOS API

Bootstrap создаёт firewall rule только для isolated controller source:

~~~text
SUSANIN: allow controller API
~~~

Не расширяйте RouterOS API на весь LAN или Internet только ради Susanin.

После полного uninstall Susanin пытается восстановить API state безопасно.

Если Susanin был единственным разрешённым API source, API может быть снова
отключён и allowed-address очищен.

Если API использовался также другими trusted sources, uninstall не должен
слепо уничтожать их configuration.

Подробнее:

[../SECURITY.md](../SECURITY.md)


# 20. Controller shutdown и restart

Controller можно штатно остановить:

~~~routeros
/container stop [find where name="susanin-controller"]
~~~

Stable v0.12.0 обрабатывает:

~~~text
SIGTERM
SIGINT
~~~

При штатной остановке ожидается сообщение:

~~~text
Susanin controller stopping gracefully.
~~~

RouterOS data plane при этом остаётся установленным.

То есть продолжают существовать:

- managed scripts;
- schedulers;
- fixed mangle;
- dynamic adaptive runtime state.

После повторного запуска controller рекомендуется проверить:

~~~text
susanin version
susanin discover
susanin status
susanin apply --dry-run
~~~

Reference после restart:

~~~text
Authenticated.

KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

# 21. Полное удаление Susanin

Полное удаление выполняется через release asset:

~~~text
uninstall.rsc
~~~

Перед реальным выполнением рекомендуется parser dry-run:

~~~routeros
/import file-name=uninstall.rsc verbose=yes dry-run
~~~

Ожидается:

~~~text
No syntax errors found in the import file
~~~

После этого:

~~~routeros
/import file-name=uninstall.rsc verbose=yes
~~~

## 21.1. Что удаляется

`uninstall.rsc` удаляет Susanin-owned:

- `susanin-controller`;
- `veth-susanin`;
- `bridge-susanin`;
- controller bridge port;
- controller RouterOS IP;
- `susanin-agent` user;
- `susanin-agent` group;
- secret mount;
- data mount;
- Susanin API firewall rule;
- managed RouterOS scripts;
- managed schedulers;
- fixed AUTO-AWG mangle;
- dynamic Susanin AUTO-AWG state;
- safety bypass rules;
- VPN Direct mangle;
- `vpn_direct` address-list state;
- Susanin VPN Direct DNS static entries;
- Susanin-owned NAT;
- Susanin-owned route;
- `/data/susanin.conf`;
- machine secret.

После полного uninstall Susanin-owned object counts должны быть нулевыми.

## 21.2. Что не должно удаляться

Выбранный пользователем VPN/tunnel сам по себе не удаляется.

Независимые routing objects также должны сохраняться, если они не принадлежат
Susanin.

Final acceptance отдельно проверяла сохранение:

~~~text
wg-awg-proxy
r_to_awg
0.0.0.0/0 via wg-awg-proxy
AWG selected traffic masquerade
~~~

После uninstall:

~~~text
wg-awg-proxy:
RUNNING

r_to_awg:
present

default route:
ACTIVE

independent NAT:
present
~~~

## 21.3. RouterOS API после uninstall

Если Susanin был единственным разрешённым API source, uninstall возвращает API
к безопасному исходному состоянию.

На reference router после uninstall было:

~~~text
api:
disabled

address:
empty
~~~

Если в API allowed-address были другие trusted sources, Susanin не должен
слепо удалять их configuration.

# 22. Удаление только controller

Для удаления только control plane:

~~~routeros
/import file-name=uninstall-controller.rsc verbose=yes
~~~

Этот вариант сохраняет уже установленный RouterOS adaptive data plane.

Используйте его, если требуется:

- заменить controller;
- убрать container;
- сохранить текущие scripts/schedulers;
- не останавливать adaptive routing.

После удаления controller RouterOS data plane продолжает работать независимо.

# 23. Troubleshooting

## 23.1. Container находится в состоянии E

Во время первого bootstrap:

~~~text
E
~~~

означает:

~~~text
DOWNLOADING/EXTRACTING
~~~

В это время могут быть пустыми:

~~~text
tag
arch
image-id
~~~

Это не обязательно ошибка.

Дождитесь завершения extraction.

После успешного bootstrap container должен стать RUNNING, а временные
`susanin-bootstrap-*` objects должны исчезнуть.

## 23.2. Controller не RUNNING

Проверьте:

~~~routeros
/container print detail where name="susanin-controller"
~~~

Также полезно посмотреть RouterOS log по container events.

Проверьте:

- container storage;
- наличие `susanin.tar`;
- architecture;
- mounts;
- VETH;
- bridge;
- RouterOS container support.

Для stable v0.12.0 публично проверен ARM64.

## 23.3. Cannot connect to RouterOS API

Проверьте API:

~~~routeros
/ip service print detail where name="api"
~~~

Проверьте Susanin firewall rule:

~~~routeros
/ip firewall filter print detail \
    where comment="SUSANIN: allow controller API"
~~~

Проверьте controller network:

~~~routeros
/ip address print detail \
    where comment="SUSANIN: controller gateway"
~~~

Не открывайте RouterOS API на весь Internet или LAN только ради диагностики.

## 23.4. Не найден LAN

Susanin ожидает RouterOS interface-list:

~~~text
LAN
~~~

Проверьте members:

~~~routeros
/interface list member print where list="LAN"
~~~

Также проверьте IPv4 addresses на LAN interfaces.

## 23.5. Не виден нужный Interface target

Запустите:

~~~text
susanin discover
susanin target list
~~~

Проверьте:

- interface существует;
- interface не disabled;
- это route-based target;
- RouterOS видит его в ожидаемом состоянии.

## 23.6. Не видна нужная Routing table

Проверьте RouterOS:

~~~routeros
/routing table print detail
~~~

И Susanin:

~~~text
susanin discover
susanin target list
~~~

Table должна существовать и быть пригодна для policy routing.

## 23.7. VPN Direct IP не работает

Проверьте:

~~~text
susanin direct list
susanin direct sync
susanin target show
~~~

На RouterOS проверьте наличие:

~~~text
vpn_direct
SUSANIN: VPN Direct bypass
~~~

VPN Direct mangle должен использовать выбранный routing mark.

## 23.8. VPN Direct domain не работает

Проверьте:

1. domain присутствует в `susanin direct list`;
2. выполнен `susanin direct sync`;
3. RouterOS DNS видит запрос клиента;
4. Susanin DNS static FWD entry присутствует;
5. `vpn_direct` получает dynamic IPv4 entries.

Если клиент использует DoH/DoT/private DNS, это может быть ожидаемым
ограничением.

## 23.9. apply --dry-run показывает CREATE

Это означает, что часть required production structure отсутствует.

Не игнорируйте CREATE.

Сначала проверьте:

~~~text
susanin status
susanin snapshot
susanin validate
~~~

Затем определите, почему object отсутствует.

## 23.10. apply --dry-run показывает UPDATE

Это означает, что существующий production object отличается от desired state.

Не вставляйте новый script source вручную.

Используйте:

~~~text
susanin stage
susanin promote --dry-run
~~~

Если safety gates проходят:

~~~text
susanin promote
susanin snapshot
susanin apply --dry-run
~~~

## 23.11. apply --dry-run показывает BLOCKERS

Не выполняйте promotion, пока blocker не понятен и не устранён.

BLOCKER означает, что Susanin не считает автоматическое изменение безопасным.

## 23.12. После падения VPN трафик идёт DIRECT

Это может быть штатным fail-open поведением.

Проверьте:

~~~text
susanin status
susanin target show
~~~

После восстановления target HEALTH должен вернуть managed adaptive routing
автоматически.

## 23.13. Lazy mangle больше нуля

Это нормально.

Например:

~~~text
lazy-mangle=4
~~~

означает, что runtime уже создал per-port adaptive state.

Structural reference при этом всё равно может быть:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
~~~

## 23.14. После restart controller data plane уже существует

Это нормально.

Controller и data plane независимы.

После restart не нужно автоматически переустанавливать data plane.

Сначала:

~~~text
susanin status
susanin apply --dry-run
~~~

Если structural state IN SYNC — production уже корректен.

# 24. Stable v0.12.0 acceptance reference

Финальный release прошёл полевую acceptance на реальном MikroTik.

Итог:

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

Полная acceptance matrix:

[TESTED.md](TESTED.md)

# 25. Stable runtime fingerprints

Production scripts:

~~~text
auto-awg-health   bytes=6148   fnv1a64=cafdf828c49d2946
auto-awg-fast     bytes=26098  fnv1a64=0c9672d93a6a4e85
auto-awg-detect   bytes=41519  fnv1a64=0ee9c8e6708bc6e8
auto-awg-judge    bytes=16840  fnv1a64=72733543f4561160
~~~

Structural reference:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

Fresh installed reference:

~~~text
scripts=4
schedulers=4
fixed mangle=8
safety bypass=3
~~~

# 26. Release integrity

Stable v0.12.0 использует frozen field-tested artifact.

Artifact source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

Exact `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f
~~~

RouterOS image-id:

~~~text
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82
~~~

Stable tag не запускает автоматическую пересборку `susanin.tar`.

Field-tested artifact не заменяется новой CI build.

Подробнее:

[RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

# 27. Ограничения stable v0.12.0

В stable scope не входят:

- IPv6 adaptive routing;
- TLS SNI inspection;
- SNI-based verification.

VPN Direct domain зависит от RouterOS-visible DNS.

Публично подтверждена:

~~~text
architecture:
ARM64

RouterOS:
7.23.3
~~~

Другие platform combinations могут работать, но не входят в текущую stable
claim.

# 28. Дополнительная документация

Основные документы:

- [README](../README.md)
- [Release Notes v0.12.0](RELEASE_NOTES_v0.12.0.md)
- [Architecture](ARCHITECTURE.md)
- [Upgrade](UPGRADE.md)
- [Logging and diagnostics](LOGGING.md)
- [Tested scenarios](TESTED.md)
- [Release integrity](RELEASE_INTEGRITY.md)
- [Security](../SECURITY.md)
- [Changelog](../CHANGELOG.md)

Development documents:

~~~text
ACCEPTANCE_v0.12.0-dev1.md
ACCEPTANCE_v0.12.0-dev3.md
ACCEPTANCE_v0.12.0-dev4.md
DEV2_PORT_AWARE.md
DEV3_PROFILES.md
DEV4_MIGRATION.md
~~~

сохранены как исторические technical evidence.

Они не заменяют текущий stable User Guide.

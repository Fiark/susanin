# Susanin v0.12.0 — stable

Susanin v0.12.0 — стабильный релиз адаптивной маршрутизации через
существующий VPN/tunnel для MikroTik RouterOS.

Публичный проверенный target:

- ARM64;
- RouterOS 7.23.3;
- IPv4;
- interface-list `LAN`;
- route-based VPN/tunnel;
- WireGuard / AmneziaWG — основной reference path.

Перед установкой или обновлением обязательно сделайте backup RouterOS.

## Главное в v0.12.0

v0.12.0 существенно расширяет архитектуру v0.11.5:

- два режима routing target: **Interface** и **Routing table**;
- table-native routing без упрощения существующей таблицы маршрутизации;
- **VPN Direct** для принудительного направления IP/CIDR/domain через выбранный VPN target;
- port-aware adaptive identity:
  `protocol + destination IPv4 + destination port`;
- lazy per-port mangle rules;
- accuracy profiles: **FAST / MIDDLE / SLOW**;
- migration hardening для старого IP-only adaptive state;
- bounded runtime garbage collection;
- строгий IPv4-only режим;
- graceful SIGTERM/SIGINT shutdown controller;
- сохранение RouterOS data plane при остановке или обновлении controller.

## Routing target

Во время `setup` можно выбрать один из двух вариантов.

### Interface

Susanin работает с выбранным route-based интерфейсом.

Если подходящей отдельной routing table нет, Susanin может создать
собственную FIB table и default route через выбранный интерфейс.

### Routing table

Susanin может использовать уже существующую RouterOS routing table напрямую.

Например:

~~~text
r_to_awg
~~~

В этом режиме Susanin не пытается заменить внутреннюю логику таблицы одним
interface gateway.

Это позволяет оставить управление маршрутами самой RouterOS-конфигурации,
включая более сложные схемы с recursive routing, несколькими маршрутами или
ECMP.

Для routing-table target NAT не создаётся автоматически Susanin, потому что
egress и NAT могут принадлежать внешней инфраструктуре.

## VPN Direct

VPN Direct — это явное пользовательское правило:

> этот IP, CIDR или domain должен идти через выбранный VPN target независимо
> от adaptive learning.

Это не DIRECT bypass.

VPN Direct имеет приоритет над обычной adaptive-классификацией Susanin.

Поддерживаются:

- IPv4;
- IPv4 CIDR;
- domain.

Примеры:

~~~text
susanin direct add ip 1.1.1.1/32
susanin direct add domain example.com
susanin direct list
susanin direct sync
~~~

Для IP/CIDR Susanin использует RouterOS address-list `vpn_direct`.

Для domain Susanin создаёт RouterOS DNS static FWD entry с
`match-subdomain=yes`, которая заполняет `vpn_direct` адресами,
увиденными RouterOS DNS.

### Ограничение VPN Direct domain

Domain policy работает только для DNS-запросов, которые видит RouterOS.

Если клиент использует:

- внешний DoH;
- внешний DoT;
- private/encrypted DNS;
- собственный DNS в обход RouterOS;
- hardcoded IP;

RouterOS может не узнать, какие IP относятся к domain, и соответствующие
динамические `vpn_direct` entries не появятся.

VPN Direct не выполняет TLS SNI inspection.

## Port-aware adaptive routing

В v0.11.x adaptive state в основном идентифицировал destination по IP.

В v0.12.0 рабочая identity стала:

~~~text
protocol + destination IPv4 + destination port
~~~

Например:

~~~text
tcp + 203.0.113.10 + 443
udp + 203.0.113.10 + 443
tcp + 203.0.113.10 + 8443
~~~

считаются разными adaptive состояниями.

Это особенно важно для адресов CDN и серверов, где один IP обслуживает
несколько протоколов и сервисов с разным сетевым поведением.

TCP и UDP по-прежнему обучаются независимо.

## Lazy per-port mangle

Susanin не создаёт заранее правила для каждого возможного порта.

Per-port AUTO-AWG rules создаются лениво только для реально наблюдаемого
adaptive state.

Fixed data plane остаётся ограниченным и предсказуемым, а динамические
per-port правила очищаются runtime GC.

## Accuracy profiles

v0.12.0 поддерживает три профиля точности:

- `fast`;
- `middle`;
- `slow`.

Профиль определяет, насколько быстро и насколько осторожно adaptive state
переходит между наблюдением, проверкой, подтверждением и cooldown.

Профиль сохраняется в persistent Susanin configuration.

## Data plane

Основной RouterOS data plane состоит из четырёх managed scripts:

~~~text
auto-awg-health
auto-awg-fast
auto-awg-detect
auto-awg-judge
~~~

Reference fingerprints финальной v0.12.0:

~~~text
auto-awg-health   bytes=6148   fnv1a64=cafdf828c49d2946
auto-awg-fast     bytes=26098  fnv1a64=0c9672d93a6a4e85
auto-awg-detect   bytes=41519  fnv1a64=0ee9c8e6708bc6e8
auto-awg-judge    bytes=16840  fnv1a64=72733543f4561160
~~~

Scheduler cadence reference profile:

~~~text
auto-awg-health   3s
auto-awg-fast     1s
auto-awg-detect   2s
auto-awg-judge    1s
~~~

Fixed adaptive mangle:

~~~text
8 rules
~~~

Safety bypass:

~~~text
3 rules
~~~

Dynamic lazy per-port rules не входят в fixed-rule reconciliation.

## Migration hardening

v0.12.0 содержит защиту перехода от старого v0.11.x IP-only adaptive state
к port-aware модели.

Перед запуском нового data plane Susanin очищает несовместимый runtime residue,
включая:

- legacy IP-only lists;
- старые port-state objects;
- lazy rules от предыдущего состояния;
- adaptive connection marks.

Development acceptance также включал upgrade/rollback сценарии от stable
v0.11.5.

## Fail-open

При проблеме с выбранным VPN/tunnel Susanin не должен превращать отказ VPN
в отказ пользовательского доступа.

HEALTH отключает managed adaptive routing и переводит трафик обратно в
обычный DIRECT path.

После восстановления tunnel adaptive routing включается обратно.

## Controller и data plane

Пользовательский трафик не проходит через контейнер Susanin.

Контейнер — control plane:

- discovery;
- setup;
- renderer;
- validation;
- install;
- status;
- configuration;
- diagnostics;
- stage/promote/rollback;
- VPN Direct policy management.

Непрерывный adaptive data plane работает внутри RouterOS.

Поэтому остановка controller не удаляет уже установленный RouterOS data plane.

## Graceful shutdown

Финальный v0.12.0 controller корректно обрабатывает SIGTERM и SIGINT.

При штатном `/container stop` controller завершает daemon без ожидания
принудительного SIGKILL и пишет:

~~~text
Susanin controller stopping gracefully.
~~~

После повторного запуска controller снова использует persistent configuration
и существующий RouterOS data plane.

## Garbage collection

Runtime GC ограничивает накопление Susanin-owned динамического состояния.

GC обслуживает только принадлежащие Susanin runtime objects и не должен
удалять независимую VPN/routing инфраструктуру пользователя.

Финальный runtime сохраняет принятый и отдельно протестированный GC source
без изменений.

## IPv4-only

v0.12.0 остаётся строго IPv4-only.

Во время setup Susanin проверяет RouterOS IPv6 state и работает в
поддерживаемой IPv4-only модели.

IPv6 adaptive routing в этот релиз не входит.

## Что Susanin не делает

Susanin:

- не является VPN-клиентом;
- не создаёт AmneziaWG/WireGuard tunnel за пользователя;
- не проксирует трафик через controller;
- не ведёт глобальную базу заблокированных сайтов;
- не выполняет DPI;
- не выполняет TLS SNI inspection;
- не отправляет пользовательскую телеметрию наружу.

## Field acceptance

Финальный frozen v0.12.0 был проверен на реальном MikroTik:

- RouterOS 7.23.3 stable;
- ARM64;
- existing WireGuard/AmneziaWG target;
- existing routing table `r_to_awg`.

Финальная acceptance включала:

- ARM64 image runtime;
- `Susanin 0.12.0` version check;
- credentialless bootstrap;
- RouterOS API authentication;
- fresh `setup`;
- routing-table target;
- `validate` PASS=4 FAIL=0;
- structural reconciliation:
  `KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0`;
- fresh data plane:
  4 scripts / 4 schedulers / 8 fixed mangle / 3 safety;
- VPN Direct IPv4 persistence and forced VPN egress;
- VPN Direct domain DNS population and forced VPN egress;
- VPN Direct priority over adaptive rules;
- graceful controller stop;
- controller restart;
- data-plane preservation while controller is stopped;
- full uninstall;
- restoration of RouterOS API state after uninstall;
- fresh reinstall from the same frozen release assets;
- preservation of independent VPN interface, routing table, default route
  and NAT during uninstall;
- clean VPN Direct state after fresh installation.

## Reference independent target used in field acceptance

The reference router already contained an independent VPN infrastructure:

~~~text
interface:      wg-awg-proxy
routing table:  r_to_awg
default route:  0.0.0.0/0 via wg-awg-proxy
NAT:            AWG selected traffic masquerade
~~~

Susanin used `r_to_awg` as its routing target.

Full uninstall removed Susanin-owned objects while preserving all of the
independent objects above.

## Frozen release identity

The runtime artifact was built from:

~~~text
ARTIFACT_SOURCE_SHA=d53517dfd6daccb7073517661138d79c573cac46
~~~

Field-tested `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f

RouterOS image-id:
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82

binary SHA256:
a9a4b963088682eb27c4ab50eae7bd7800949e0d1f98ec230a769830434b7d51
~~~

Bootstrap assets:

~~~text
192104fc8012958dc511f2f5e7db5ff58be9b7727a287fcfae5e2854407be1c0  install.rsc
6067daacdfb7a047aa4791d2d7d46796dc3a26102c6d9e1e1b1da2f5f40bec98  uninstall.rsc
c5b6751f7d0907cc5db2e904ee32832e92446adfff3e9f9b64d2343ba522d4d3  uninstall-controller.rsc
~~~

Release bundle содержит:

~~~text
susanin.tar
install.rsc
uninstall.rsc
uninstall-controller.rsc
SHA256SUMS
~~~

Важно: stable release использует именно frozen field-tested artifact.

GitHub release workflow не пересобирает `susanin.tar` при создании stable tag.

Документационные и release-only commits после `ARTIFACT_SOURCE_SHA` допустимы
только если frozen runtime/build inputs остаются идентичными исходному
artifact source.

## Known limitations

Для v0.12.0:

- публично проверенная архитектура — ARM64;
- reference RouterOS — 7.23.3;
- IPv6 не поддерживается;
- VPN/tunnel должен существовать до установки Susanin;
- VPN Direct domain зависит от DNS visibility в RouterOS;
- TLS SNI inspection отсутствует;
- SNI-based verification не входит в stable v0.12.0;
- другие RouterOS версии и архитектуры могут работать, но не входят в
  подтверждённый stable reference profile.

## Installation

Основная установка:

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

После запуска controller:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin setup" \
  no-sh \
  timeout=300
~~~

После setup:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin status" \
  no-sh \
  timeout=60
~~~

Structural check:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh \
  timeout=60
~~~

Нормальный structural result:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## Uninstall

Полное удаление Susanin:

~~~routeros
/import file-name=uninstall.rsc verbose=yes
~~~

`uninstall.rsc` удаляет Susanin controller и Susanin-managed RouterOS
data plane.

Выбранный пользователем VPN/tunnel сам по себе не удаляется.

Удаление только controller с сохранением adaptive RouterOS data plane:

~~~routeros
/import file-name=uninstall-controller.rsc verbose=yes
~~~

## Release assets

Stable release содержит пять файлов:

- `susanin.tar`;
- `install.rsc`;
- `uninstall.rsc`;
- `uninstall-controller.rsc`;
- `SHA256SUMS`.

Проверяйте SHA256 перед установкой.

## Documentation

- `README.md` — быстрый старт;
- `docs/USER_GUIDE.md` — полное руководство;
- `docs/ARCHITECTURE.md` — архитектура;
- `docs/UPGRADE.md` — обновление;
- `docs/LOGGING.md` — логирование и диагностика;
- `docs/TESTED.md` — проверенные сценарии;
- `docs/RELEASE_INTEGRITY.md` — происхождение и целостность release assets;
- `SECURITY.md` — модель безопасности.

# Обновление Susanin до v0.12.0

Susanin разделён на две независимые части:

- container — control plane;
- RouterOS `auto-awg-*` scripts/schedulers — data plane.

Замена container **не означает**, что уже установленный RouterOS data plane
автоматически обновился.

Для перехода на v0.12.0 используйте проверяемую последовательность:

~~~text
backup
  |
  v
upgrade controller
  |
  v
verify target/config
  |
  v
validate + apply --dry-run
  |
  v
stage
  |
  v
promote --dry-run
  |
  v
promote
  |
  v
post-upgrade verification
~~~

Не заменяйте production RouterOS scripts вручную.

## 1. Сделайте backup RouterOS

Перед обновлением сохраните рабочую конфигурацию.

Например:

~~~routeros
/system backup save name=before-susanin-v012
/export file=before-susanin-v012
~~~

Backup и export могут содержать чувствительные данные.

Не публикуйте их в GitHub Issues.

## 2. Скачайте stable v0.12.0

Нужны:

~~~text
susanin.tar
install.rsc
SHA256SUMS
~~~

Рекомендуется также сохранить:

~~~text
uninstall.rsc
uninstall-controller.rsc
~~~

Проверьте release assets:

~~~bash
sha256sum -c SHA256SUMS
~~~

Для stable v0.12.0 используйте только frozen release artifacts.

См.:

[RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

## 3. Загрузите новый release на MikroTik

Загрузите новый:

~~~text
susanin.tar
install.rsc
~~~

Имена должны остаться именно такими.

Убедитесь, что старый `susanin.tar` не остался под release-именем вместо
нового файла.

## 4. Проверьте bootstrap parser

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Не должно быть syntax errors.

## 5. Обновите controller

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Bootstrap заменяет старый Susanin controller, но работающий RouterOS data
plane остаётся отдельным объектом.

Дождитесь RUNNING:

~~~routeros
/container print detail where name="susanin-controller"
~~~

Для v0.12.0 ожидается:

~~~text
tag="0.12.0"
arch="arm64"
root-dir=/susanin-controller-v0120
~~~

## 6. Проверьте версию и API

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

Затем:

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin discover" \
    no-sh \
    timeout=60
~~~

Нужно увидеть:

~~~text
Authenticated.
~~~

## 7. Проверьте перенесённую configuration

Выполните:

~~~text
susanin config show
susanin target show
~~~

### Upgrade с v0.11.x

v0.12.0 умеет читать старую v0.11.x configuration.

Если новых `target_mode` / `target_value` ещё нет, controller использует
сохранённый старый `egress_interface` как:

~~~text
target mode:
interface
~~~

Если `accuracy_profile` отсутствует, используется:

~~~text
fast
~~~

То есть upgrade сам по себе **не переводит** старую installation в
`routing-table` mode.

## 8. Решите, какой target использовать

### Сохранить старый Interface mode

Если старый interface target вас устраивает, менять target не нужно.

Проверьте:

~~~text
susanin target show
susanin discover
~~~

### Перейти на Routing table

Если у вас уже есть отдельная RouterOS routing table:

~~~text
susanin target list
susanin target set routing-table <name>
~~~

Например:

~~~text
susanin target set routing-table r_to_awg
~~~

После изменения:

~~~text
susanin target show
susanin discover
susanin direct sync
~~~

Для routing-table target Susanin не должен автоматически владеть внешним NAT.


## 9. Проверьте generated RouterOS source

После обновления controller и проверки target:

~~~text
susanin validate
~~~

Reference v0.12.0:

~~~text
PASS=4 FAIL=0
Production scripts changed: NO
~~~

Если validation не проходит, не выполняйте promotion.

## 10. Сравните desired и production

~~~text
susanin apply --dry-run
~~~

Если получено:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

production data plane уже соответствует v0.12.0.

Дополнительный promotion не требуется.

## 11. Если есть UPDATE

При переходе с v0.11.x на v0.12.0 UPDATE ожидаем, потому что data plane
переходит на port-aware модель.

Не заменяйте production scripts вручную.

Создайте inert stage:

~~~text
susanin stage
~~~

Ожидается создание четырёх stage scripts:

~~~text
susanin-stage-health
susanin-stage-fast
susanin-stage-detect
susanin-stage-judge
~~~

Production data plane при этом продолжает работать.

## 12. Проверьте promotion safety gates

~~~text
susanin promote --dry-run
~~~

Продолжайте только если:

~~~text
Safety gates: PASS
~~~

Dry-run ничего не меняет в production.

## 13. Выполните transactional promotion

~~~text
susanin promote
~~~

Promotion выполняет:

1. snapshot текущего production;
2. создание `susanin-backup-*`;
3. pause managed schedulers;
4. ожидание завершения active jobs;
5. замену четырёх production sources;
6. проверку fingerprints;
7. очистку несовместимого adaptive runtime state;
8. восстановление scheduler states;
9. удаление stage objects.

Reference success:

~~~text
Promotion result: SUCCESS
Rollback backups retained: YES
Scheduler states restored: YES
~~~

## 14. Migration v0.11.x -> v0.12.0

v0.11.x использовал старую IP-oriented adaptive модель.

v0.12.0 использует:

~~~text
protocol + destination IPv4 + destination port
~~~

Поэтому старое runtime state нельзя просто оставить активным после замены
script source.

Во время успешного promotion Susanin очищает несовместимое состояние:

- legacy IP-only entries;
- старые port-state entries;
- stale lazy per-port rules;
- adaptive connection marks.

После upgrade нормальный status должен показывать:

~~~text
legacy IP-only entries=0
Adaptive migration state: clean
~~~

## 15. Проверьте production после promotion

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

Reference fingerprints v0.12.0:

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

## 16. Проверьте schedulers

Reference:

~~~text
auto-awg-health   interval=3s
auto-awg-fast     interval=1s
auto-awg-detect   interval=2s
auto-awg-judge    interval=1s
~~~

Проверьте через:

~~~routeros
/system scheduler print detail where name~"^auto-awg"
/system script job print
~~~

Все четыре managed schedulers должны быть enabled после успешного promotion,
если они были enabled до него.

## 17. Проверьте VPN Direct

После изменения target:

~~~text
susanin direct list
susanin direct sync
~~~

Если policy пустая:

~~~text
Policy is empty.
~~~

Если policy существует, убедитесь, что она использует новый выбранный target.

Для domain policy также проверьте DNS visibility через RouterOS.

## 18. Если promotion неудачен

Susanin пытается сохранить безопасное состояние и восстановить production
source.

Для ручного rollback:

~~~text
susanin rollback
~~~

Rollback:

- pauses schedulers;
- ждёт завершения jobs;
- очищает adaptive runtime state;
- восстанавливает `susanin-backup-*`;
- проверяет restored source;
- восстанавливает scheduler states.

После успешного rollback:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

## 19. Failed-safe состояние

Если runtime cleanup или source restore не удалось безопасно завершить,
Susanin может оставить managed schedulers PAUSED.

Это намеренное fail-safe поведение.

Не включайте их вручную до выяснения причины.

Сначала соберите:

~~~text
susanin status
susanin snapshot
susanin diag errors
susanin apply --dry-run
~~~

## 20. После обновления

Рекомендуется выполнить:

~~~text
susanin version
susanin discover
susanin target show
susanin config show
susanin direct list
susanin status
susanin apply --dry-run
~~~

Для stable v0.12.0 нормальный финал:

~~~text
Susanin 0.12.0

Authenticated.

KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 21. Что не нужно делать

Не нужно:

- вручную вставлять новый source в `auto-awg-*`;
- удалять старые runtime lists вручную перед promotion;
- отключать independent VPN infrastructure;
- пересоздавать существующую routing table без причины;
- запускать `setup` только ради обновления уже корректной configuration;
- запускать `promote`, если `promote --dry-run` не прошёл.

## 22. Если хотите перейти на routing-table mode после upgrade

После успешного controller upgrade можно отдельно изменить target:

~~~text
susanin target set routing-table <name>
~~~

Затем:

~~~text
susanin target show
susanin discover
susanin direct sync
susanin validate
susanin apply --dry-run
~~~

Если появился UPDATE:

~~~text
susanin stage
susanin promote --dry-run
susanin promote
~~~

## 23. Диагностика проблем upgrade

Если upgrade дал неожиданный результат:

~~~text
susanin diag start
susanin diag sample
susanin diag errors
susanin status
susanin snapshot
susanin apply --dry-run
susanin diag stop
~~~

См.:

[LOGGING.md](LOGGING.md)

## 24. Stable v0.12.0 reference

Финальная v0.12.0 acceptance включала:

~~~text
FRESH_BOOTSTRAP=PASS
FRESH_SETUP=PASS
STRUCTURAL_SYNC=PASS
TABLE_NATIVE_TARGET=PASS
GRACEFUL_SIGTERM=PASS
RESTART_AFTER_SIGTERM=PASS
FULL_UNINSTALL=PASS
FRESH_REINSTALL=PASS
GC_IDENTICAL=PASS
~~~

Подробнее:

[TESTED.md](TESTED.md)

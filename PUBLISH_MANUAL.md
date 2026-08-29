# Susanin — ручная публикация GitHub + Habr

Этот файл рассчитан на публикацию без подключения GitHub к ChatGPT. Все действия выполняются вручную из браузера и терминала.

## 0. Что публикуем

Проверенный функциональный baseline: `v0.11.3`.

Публичный статус проекта: **pilot / experimental**.

Проверенный target:

- MikroTik ARM64;
- RouterOS 7.23.3;
- route-based WireGuard / AmneziaWG egress;
- clean install `0/16 -> 16/16`;
- reboot;
- fail-open / recovery;
- credentialless bootstrap;
- controller upgrade;
- TCP/UDP behavioral learning.

В репозиторий НЕЛЬЗЯ добавлять реальные backup/export/config файлы роутера, VPN keys, endpoint IP, MAC/serial, старые passwords, `config.env`.

## 1. Перед публикацией — заменить placeholders

В этом пакете личный GitHub логин специально не зашит.

Задайте значения:

```bash
export GHUSER='ВАШ_GITHUB_LOGIN'
export AUTHOR='ВАШЕ_ИМЯ_ИЛИ_НИК'
```

Из корня репозитория:

```bash
grep -RIl 'Fiark' . | while read -r f; do
  sed -i "s/Fiark/${GHUSER}/g" "$f"
done

sed -i "s/YOUR_NAME_OR_NICKNAME/${AUTHOR}/g" LICENSE
```

Контроль:

```bash
grep -RIn 'Fiark\|YOUR_NAME_OR_NICKNAME' . || true
```

Вывод должен быть пустым.

## 2. Финальная проверка секретов

```bash
chmod +x tools/secret-scan.sh
./tools/secret-scan.sh .
```

Должно быть:

```text
PASS: no credential artifacts detected
```

Дополнительно:

```bash
grep -RInE 'PRIVATE KEY|BEGIN .*PRIVATE|password=|ROUTER_(USER|PASSWORD)|show-sensitive' . \
  --exclude-dir=.git || true
```

Любой подозрительный результат проверить до публикации.

## 3. Создать GitHub repository вручную

В GitHub:

1. `New repository`.
2. Repository name: `susanin`.
3. Visibility: `Public`.
4. Description, например:

   `Adaptive behavioral VPN routing for MikroTik RouterOS. Pilot ARM64 project in C11.`

5. Не ставить галочки `Add a README`, `Add .gitignore`, `Choose a license` — всё уже находится в пакете.
6. Нажать `Create repository`.

Рекомендуемые topics:

```text
mikrotik
routeros
vpn
wireguard
amneziawg
routing
connection-tracking
c11
arm64
networking
security
```

## 4. Первый push вручную

Распаковать пакет в отдельный каталог, затем:

```bash
cd susanin

git init
git branch -M main
git add .
git status
git commit -m 'Initial public pilot release v0.11.3'
git remote add origin "https://github.com/${GHUSER}/susanin.git"
git push -u origin main
```

Если используется SSH:

```bash
git remote set-url origin "git@github.com:${GHUSER}/susanin.git"
git push -u origin main
```

После push открыть GitHub и проверить, что README отображает картинки из `docs/images/`.

## 5. Что должно быть в корне

```text
.github/
bootstrap/
docs/
src/
templates/
tools/

README.md
README_en.md
CHANGELOG.md
CONTRIBUTING.md
SECURITY.md
LICENSE
Dockerfile
Makefile
.gitignore
.dockerignore
```

Структура намеренно похожа по уровню документации на `timbrs/amneziawg-mikrotik-c`: подробный README, английская версия, docs, changelog, troubleshooting и автоматическая сборка release artifacts. Это вдохновение по подходу и качеству оформления, а не копирование исходников.

## 6. Настройки GitHub repository

В `Settings -> General`:

- Issues: ON;
- Projects: по желанию;
- Wiki: OFF на старте, документация уже в `docs/`;
- Discussions: можно включить позже;
- Pull requests: ON.

В `Settings -> Actions -> General`:

- Allow all actions and reusable workflows;
- Workflow permissions: `Read and write permissions`, если release workflow не может создавать release assets.

В `Settings -> Security`:

- Dependabot не обязателен: проект практически без внешних runtime-зависимостей;
- secret scanning включить, если доступно для аккаунта/repository.

## 7. Проверить CI

После первого push открыть вкладку `Actions`.

Workflow `CI` должен:

- собрать C11 с `-Wall -Wextra -Wpedantic -Werror`;
- выполнить `secret-scan.sh`;
- собрать Docker image.

Если CI красный — release не создавать до исправления.

## 8. Локальная ARM64 сборка перед release

На build-машине:

```bash
docker buildx build \
  --builder way-builder \
  --platform linux/arm64 \
  --no-cache \
  -t susanin:0.11.3 \
  --output type=docker,dest=susanin.tar \
  .
```

Проверить:

```bash
sudo docker load -i susanin.tar
sudo docker image inspect susanin:0.11.3 --format '{{.Os}}/{{.Architecture}}'
sudo docker run --rm --platform linux/arm64 susanin:0.11.3 version
```

Ожидается:

```text
linux/arm64
Susanin 0.11.3
```

## 9. Создать Git tag и GitHub Release

После успешного CI:

```bash
git tag -a v0.11.3 -m 'Susanin v0.11.3 pilot'
git push origin v0.11.3
```

В репозитории уже есть `.github/workflows/release.yml`. Tag `v*` запускает ARM64 build и прикладывает к prerelease:

```text
susanin.tar
install.rsc
uninstall.rsc
uninstall-controller.rsc
SHA256SUMS
```

Проверить вкладку `Actions`, затем `Releases`.

Для `v0.11.3` оставить флаг **Pre-release**, потому что проект pilot.

Рекомендуемый текст release:

```text
Susanin v0.11.3 — first public pilot

Experimental adaptive behavioral routing control-plane for MikroTik RouterOS.

Tested baseline:
- ARM64
- RouterOS 7.23.3
- WireGuard / AmneziaWG route-based egress
- credentialless bootstrap
- clean install 0/16 -> 16/16
- reboot and fail-open/recovery

WARNING: pilot software. Make a RouterOS backup before installation.
```

## 10. Пользовательская инструкция, которую проверяем перед публикацией

Обычный пользователь не должен собирать проект.

### Требования

- ARM64 MikroTik;
- RouterOS 7.23.3 — проверенная версия;
- package `container`;
- device-mode с `container=yes` и `scheduler=yes`;
- interface-list `LAN` с IPv4 адресом;
- уже работающий route-based VPN/tunnel.

### Установка

1. Сделать RouterOS backup.
2. Скачать из GitHub Release:
   - `susanin.tar`;
   - `install.rsc`.
3. Загрузить оба файла в `Files` через WinBox/SCP.
4. Проверить import:

```routeros
/import file-name=install.rsc verbose=yes dry-run
```

5. Если `No syntax errors found`, выполнить:

```routeros
/import file-name=install.rsc verbose=yes
```

6. Дождаться:

```routeros
/container print where name="susanin-controller"
```

Флаг должен стать `R`.

7. Запустить setup:

```routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin setup" no-sh timeout=300
```

8. Выбрать VPN-интерфейс.
9. Проверить:

```routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin status" no-sh timeout=60
```

## 11. Как пользователю смотреть логи

Основной live log:

```routeros
/log print follow-only where message~"AUTO-AWG:"
```

Типичные события:

```text
AUTO-AWG: FAST TCP-SYN ...
AUTO-AWG: FAST QUIC ...
AUTO-AWG: SOFT TCP-STALL ...
AUTO-AWG: CONFIRMED ... via tcp:443
AUTO-AWG: CONFIRMED ... via udp:443
AUTO-AWG: tunnel DOWN after 2 health misses, fallback to DIRECT
AUTO-AWG: tunnel UP, recovery TCP=... UDP=...
```

Статус Susanin:

```routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin status" no-sh timeout=60
```

Structural reconciliation:

```routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin apply --dry-run" no-sh timeout=60
```

Ожидаем для полностью синхронной установки:

```text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
```

Scheduler counters:

```routeros
/system scheduler print where name~"auto-awg-"
```

Learned cache:

```routeros
/ip firewall address-list print where list~"auto_awg_"
```

Mangle:

```routeros
/ip firewall mangle print where comment~"^AUTO-AWG:|^SUSANIN:"
```

## 12. Удаление

Полное удаление — загрузить `uninstall.rsc` и выполнить по инструкции README.

Перед удалением также сделать backup.

`uninstall-controller.rsc` предназначен для удаления только control-plane, когда RouterOS data-plane нужно оставить работающим.

## 13. Подготовка статьи на Habr

Черновик находится в:

```text
docs/habr-article.md
```

Отдельный publishing checklist:

```text
docs/HABR_PUBLISHING.md
```

Статья сознательно строится в духе подробной инженерной статьи `Наконец-то: AmneziaWG в Mikrotik`, но текст и техническая задача Susanin самостоятельные.

Обязательные мысли статьи:

- почему надоело поддерживать статические списки доменов/IP;
- явная благодарность `timbrs/amneziawg-mikrotik-c` и ссылка на его Habr-статью;
- смысл названия «Сусанин»: пытается выбраться из леса сетевых путей и в итоге найти рабочую дорогу;
- автор — сетевой инженер и специалист по ИБ, не профессиональный C-разработчик;
- ChatGPT активно использовался и это не скрывается;
- проект pilot/experimental;
- FAST / SOFT / JUDGE / HEALTH;
- TCP/UDP обучаются отдельно;
- data plane в RouterOS, control plane в C container;
- credentialless bootstrap;
- реальные баги разработки, включая `!empty -> !done` RouterOS API framing;
- clean install, reboot, fail-open/recovery;
- полная пользовательская установка;
- команды просмотра логов и troubleshooting;
- ограничения и roadmap.

## 14. Картинки Habr

Готовые PNG лежат в:

```text
docs/images/
```

Порядок:

1. `hero.png`;
2. `before-after.png`;
3. `decision-flow.png`;
4. `architecture.png`;
5. `bootstrap-flow.png`;
6. `routeros-api-framing.png`;
7. `setup-terminal.png`;
8. `status-terminal.png`.

На Habr картинки загружаются вручную в редактор. После загрузки заменить локальные ссылки вида:

```markdown
![...](images/architecture.png)
```

на URL, который выдаст Habr storage.

## 15. Публикация Habr — порядок

1. Создать новую статью.
2. Вставить `docs/habr-article.md`.
3. Загрузить 8 PNG по местам.
4. Заменить `Fiark` в финальной ссылке.
5. Проверить preview.
6. Убедиться, что `<cut />` стоит после вступления.
7. Добавить хабы по смыслу: сетевые технологии, информационная безопасность, системное администрирование/сетевое оборудование — только те, что реально доступны в редакторе.
8. Теги: `mikrotik`, `routeros`, `vpn`, `wireguard`, `amneziawg`, `routing`, `connection tracking`, `c11`, `arm64`.
9. Перед публикацией открыть GitHub Release в отдельной вкладке и проверить, что файлы скачиваются.
10. Ещё раз проверить, что статья содержит слова `pilot/experimental` в начале и в разделе ограничений.

## 16. После публикации

В GitHub README добавить ссылку на опубликованную Habr-статью, затем:

```bash
git add README.md README_en.md
git commit -m 'docs: add Habr article link'
git push
```

Не перетегировать `v0.11.3` только ради ссылки на статью.

## 17. Главное правило первого публичного релиза

Не добавлять новые функции между проверенным clean-install v0.11.3 и публикацией.

Разрешены только:

- документация;
- картинки;
- CI/release packaging;
- исправление явной утечки секрета;
- критический packaging bug.

Любое изменение data-plane heuristics должно идти уже в следующую версию и снова проходить RouterOS clean-install/reboot test.

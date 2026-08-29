# Habr publishing checklist

## Рабочий заголовок

**Сусанин: как я перестал кормить MikroTik списками доменов и научил его сам искать рабочий маршрут**

Альтернативы:

- **Сусанин для MikroTik: adaptive routing без списков доменов и IP**
- **Как я научил MikroTik проверять проблемные направления через VPN вместо статических списков**

## Рекомендуемые хабы

До публикации проверьте актуальные названия на Хабре. По смыслу подходят:

- Сетевые технологии;
- Информационная безопасность;
- Сетевое оборудование;
- Системное администрирование;
- C / Open source — если доступны и уместны.

## Теги

```text
mikrotik
routeros
vpn
wireguard
amneziawg
routing
connection tracking
c11
arm64
network engineering
```

## Картинки для загрузки на Habr

Загружать лучше PNG из `docs/images/`:

1. `hero.png` — после TL;DR / в начало;
2. `before-after.png` — раздел про статические списки;
3. `decision-flow.png` — FAST/SOFT/JUDGE/HEALTH;
4. `architecture.png` — data plane/control plane;
5. `bootstrap-flow.png` — credentialless bootstrap;
6. `routeros-api-framing.png` — баг `!empty`/`!done`;
7. `setup-terminal.png` — пользовательский setup;
8. `status-terminal.png` — status/reconciliation.

После загрузки на Habr заменить локальные `images/...` ссылки в `docs/habr-article.md` на URL Habr storage.

## Обязательные акценты перед публикацией

- Проект явно помечен как **pilot/experimental**.
- В самом начале есть рекомендация сделать backup.
- Явно указать, что основной проверенный target — ARM64 + RouterOS 7.23.3.
- Не обещать широкую совместимость с любым VPN: WireGuard/AmneziaWG — основной реально проверенный путь.
- Явно поблагодарить `timbrs/amneziawg-mikrotik-c` и дать ссылку на исходную статью.
- Не скрывать использование ChatGPT в разработке.
- Пояснить, что автор — сетевой инженер/ИБ, а не профессиональный C-разработчик.
- Не публиковать реальные VPN keys, endpoint, serial/MAC, private backups или старые credentials.

## После создания GitHub repo

Проверить и заменить все ссылки на:

```text
https://github.com/Fiark/susanin
```

В Habr статье лучше ссылаться на конкретный pilot release, а не только на `main`.

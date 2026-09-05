# Обновление Susanin

Это официальная процедура обновления Susanin начиная с v0.11.5.

Главное различие:

- Susanin container = control plane;
- RouterOS `auto-awg-*` scripts = data plane.

Замена container **не означает**, что source уже существующих RouterOS scripts автоматически обновился.

`install` намеренно не перезаписывает полностью установленный data plane без отдельной проверки.

---

## 1. Сделайте backup RouterOS

~~~routeros
/system backup save name=before-susanin-upgrade dont-encrypt=yes
/export file=before-susanin-upgrade
~~~

Не публикуйте эти файлы в GitHub Issues.

---

## 2. Скачайте новый release

Нужны:

~~~text
susanin.tar
install.rsc
SHA256SUMS
~~~

Проверьте SHA256 до установки.

---

## 3. Проверка bootstrap parser

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Ожидается:

~~~text
No syntax errors found in the import file
~~~

---

## 4. Обновите controller

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Дождитесь:

~~~routeros
/container print where name="susanin-controller"
~~~

Нужен `R`.

---

## 5. Проверить версию controller

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin version" \
  no-sh timeout=30
~~~

---

## 6. Проверить generated RouterOS source

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin validate" \
  no-sh timeout=120
~~~

Нужно:

~~~text
PASS=4 FAIL=0
Production scripts changed: NO
~~~

---

## 7. Сравнить desired и production

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh timeout=60
~~~

Если:

~~~text
UPDATE=0
BLOCKERS=0
Result: IN SYNC structurally.
~~~

RouterOS data plane уже соответствует новой версии.

Обновление закончено.

---

# Если apply --dry-run показывает UPDATE

Это нормальная ситуация при переходе на версию,
в которой изменились FAST/SOFT/JUDGE/HEALTH.

Не используйте слепое перезаписывание scripts.

---

## 8. Создать inert stage

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin stage" \
  no-sh timeout=300
~~~

Нужно:

~~~text
STAGED=4
FAIL=0
Production scripts changed: NO
Schedulers attached to stage: NO
~~~

---

## 9. Проверить promotion

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin promote --dry-run" \
  no-sh timeout=120
~~~

Продолжайте только если:

~~~text
Safety gates: PASS
~~~

---

## 10. Transactional promotion

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin promote" \
  no-sh timeout=300
~~~

Нужно:

~~~text
Promotion result: SUCCESS
Rollback backups retained: YES
Scheduler states restored: YES
~~~

---

## 11. Проверить production после promotion

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin snapshot" \
  no-sh timeout=60

/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh timeout=60
~~~

Нормальный финал:

~~~text
KEEP=16
CREATE=0
UPDATE=0
BLOCKERS=0
Result: IN SYNC structurally.
~~~

---

## 12. Проверить schedulers

~~~routeros
/system/scheduler/print detail where name~"awg"
/system/script/job/print
~~~

---

## 13. Если promotion неудачен

Susanin автоматически пытается восстановить production source.

Дополнительно существует:

~~~text
susanin rollback
~~~

который использует `susanin-backup-*`.

---

# Обновление v0.11.3 / v0.11.4 -> v0.11.5

Для старых установок особенно важно выполнить:

~~~text
validate
apply --dry-run
stage
promote --dry-run
promote
apply --dry-run
~~~

Простой импорт нового `install.rsc` обновляет controller,
но не должен считаться доказательством обновления RouterOS data plane.

---

# После обновления

Рекомендуется:

~~~text
susanin status
susanin diag sample
susanin diag errors
~~~

Если возникла проблема, используйте процедуру из:

[LOGGING.md](LOGGING.md)

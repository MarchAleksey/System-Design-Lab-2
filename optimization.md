# Оптимизация запросов (мессенджер, вариант 5)

Документ описывает типичные «горячие» запросы, индексы из `schema.sql` и сравнение планов `EXPLAIN` до и после оптимизации.

## 1. Поиск пользователя по логину

```sql
EXPLAIN ANALYZE
SELECT id, login, first_name, last_name, created_at
FROM users
WHERE login = 'alice';
```

| | План |
|---|------|
| **До** (без UNIQUE) | `Seq Scan on users` — полный перебор |
| **После** | `Index Scan using users_login_unique` — O(log n) по логину |

**Индекс:** ограничение `UNIQUE (login)` создаёт B-tree индекс автоматически.

---

## 2. Поиск по маске имени / фамилии

```sql
EXPLAIN ANALYZE
SELECT id, login, first_name, last_name
FROM users
WHERE first_name ILIKE '%Carol%';
```

| | План |
|---|------|
| **До** | `Seq Scan` + фильтр `ILIKE` на каждой строке |
| **После** | `Bitmap Index Scan` на `idx_users_first_name_lower` (для префиксных масок) или сужение выборки при комбинации условий |

**Индексы:** `idx_users_first_name_lower`, `idx_users_last_name_lower` — ускоряют `lower(column)` и частично помогают `ILIKE` (для `%mask%` полный выигрыш ограничен; для production можно добавить `pg_trgm`).

**Переписывание:** передавать маску параметром `$1`, не конкатенировать SQL-строкой (см. `queries.sql`).

---

## 3. Список сообщений группового чата

```sql
EXPLAIN ANALYZE
SELECT id, chat_id, sender_id, content, created_at
FROM group_messages
WHERE chat_id = 1
ORDER BY created_at ASC, id ASC
LIMIT 100;
```

| | План |
|---|------|
| **До** | `Seq Scan` на всех партициях + `Sort` |
| **После** | `Index Scan` на `idx_group_messages_chat_created_at` (часто без отдельной сортировки) |

**Индекс:** `(chat_id, created_at)` — покрывает `WHERE chat_id = ?` и `ORDER BY created_at`.

---

## 4. P2P-лента пользователя

```sql
EXPLAIN ANALYZE
SELECT id, sender_id, recipient_id, content, created_at
FROM p2p_messages
WHERE sender_id = 1 OR recipient_id = 1
ORDER BY created_at ASC
LIMIT 100;
```

| | План |
|---|------|
| **До** | `Seq Scan` + `Sort` |
| **После** | `BitmapOr` двух `Index Scan`: `idx_p2p_messages_sender_created_at` и `idx_p2p_messages_recipient_created_at` |

**Оптимизация запроса** (альтернатива `OR`):

```sql
SELECT * FROM p2p_messages WHERE sender_id = $1
UNION ALL
SELECT * FROM p2p_messages WHERE recipient_id = $1 AND sender_id <> $1
ORDER BY created_at ASC
LIMIT $2;
```

Планировщик чаще использует оба индекса без широкого sequential scan.

---

## 5. Проверка членства в чате (JOIN / EXISTS)

```sql
EXPLAIN ANALYZE
SELECT 1
FROM group_chat_members
WHERE chat_id = 1 AND user_id = 2;
```

| | План |
|---|------|
| **До** | `Seq Scan` по `group_chat_members` |
| **После** | `Index Only Scan` или `Index Scan` по PK `(chat_id, user_id)` |

PK таблицы участников уже является оптимальным индексом для пары `(chat_id, user_id)`.

---

## 6. Партиционирование `group_messages`

**Стратегия:** `PARTITION BY RANGE (created_at)` — помесячные партиции (`group_messages_2025_01`, …).

**Зачем:** при миллионах сообщений запросы с фильтром по `created_at` (архивация, отчёты) читают только нужные партиции; `VACUUM` и удаление старых данных — через `DROP PARTITION`.

**Пример pruning:**

```sql
EXPLAIN ANALYZE
SELECT count(*) FROM group_messages
WHERE created_at >= '2025-02-01' AND created_at < '2025-03-01';
```

В плане — `Append` только по релевантным партициям, без сканирования всей истории.

---

## Как воспроизвести локально

```bash
docker compose up -d postgres
docker compose exec postgres psql -U messenger -d messenger -f /docker-entrypoint-initdb.d/01-schema.sql
# после загрузки data.sql:
docker compose exec postgres psql -U messenger -d messenger
# в psql: \i /path/to/queries fragments + EXPLAIN ANALYZE
```

Скрипты: `schema.sql`, `data.sql`, `queries.sql`.

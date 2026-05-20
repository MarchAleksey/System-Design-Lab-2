# System-Design-Lab-3: Messenger + PostgreSQL

Домашнее задание 03 — проектирование и оптимизация реляционной БД для мессенджера (вариант 5, [Slack](https://slack.com/)) с подключением REST API из лабораторной 02.

## Сущности

| Сущность | Таблица | Описание |
|----------|---------|----------|
| **User** | `users` | login, имя, фамилия, хэш пароля |
| **Group Chat** | `group_chats`, `group_chat_members` | чат и участники |
| **Group Message** | `group_messages` | сообщения в групповом чате (партиционирование по `created_at`) |
| **P2P Message** | `p2p_messages` | личная переписка |

Хранилище API: **PostgreSQL 16** (компонент userver `postgres-database`).

## Файлы задания

| Файл | Назначение |
|------|------------|
| [`schema.sql`](schema.sql) | `CREATE TABLE`, индексы, партиции |
| [`data.sql`](data.sql) | тестовые данные (≥10 строк в каждой таблице) |
| [`queries.sql`](queries.sql) | SQL для всех операций API |
| [`optimization.md`](optimization.md) | `EXPLAIN`, индексы, сравнение до/после |
| [`docker-compose.yaml`](docker-compose.yaml) | PostgreSQL + API |
| [`Dockerfile`](Dockerfile) | сборка сервиса userver |

Схема для автотестов: [`postgresql/schemas/messenger.sql`](postgresql/schemas/messenger.sql).

## ER-диаграмма (логическая схема)

```mermaid
erDiagram
    users ||--o{ group_chats : creates
    users ||--o{ group_chat_members : joins
    group_chats ||--o{ group_chat_members : has
    group_chats ||--o{ group_messages : contains
    users ||--o{ group_messages : sends
    users ||--o{ p2p_messages : sends
    users ||--o{ p2p_messages : receives
```

## Индексы (кратко)

- `users(login)` — UNIQUE, поиск по логину
- `idx_users_*_lower` — маска имени/фамилии (`ILIKE`)
- `idx_group_messages_chat_created_at` — лента чата
- `idx_p2p_messages_*_created_at` — входящие/исходящие P2P
- PK `(chat_id, user_id)` на `group_chat_members` — проверка членства

Подробнее: [`optimization.md`](optimization.md).

## API (`/api/v1`)

| Метод | URL | Auth | SQL / storage |
|-------|-----|------|----------------|
| GET | `/health` | — | — |
| POST | `/auth/register` | — | `INSERT users` |
| POST | `/auth/login` | — | `SELECT users` + сессия в памяти |
| POST | `/users` | — | `INSERT users` |
| GET | `/users/by-login/{login}` | — | `SELECT` по login |
| GET | `/users/search` | — | `ILIKE` по маскам |
| POST | `/group-chats` | Bearer | `INSERT group_chats` + creator в members |
| POST | `/group-chats/{id}/members` | Bearer | `INSERT group_chat_members` |
| POST | `/group-chats/{id}/messages` | Bearer | `INSERT group_messages` |
| GET | `/group-chats/{id}/messages` | Bearer | `SELECT` с LIMIT/OFFSET |
| POST | `/p2p/messages` | Bearer | `INSERT p2p_messages` |
| GET | `/p2p/messages` | Bearer | `SELECT` диалогов пользователя |

Спецификация: [`openapi.yaml`](openapi.yaml).

Идентификаторы в JSON: `user-1`, `chat-1`, `gmsg-1`, `pmsg-1` (внутри БД — `BIGSERIAL`).

## Запуск через Docker Compose

```bash
docker compose up --build
```

- PostgreSQL: `localhost:5432`, БД `messenger`, пользователь/пароль `messenger`
- API: http://localhost:8080

При первом старте Postgres выполняет `schema.sql` и `data.sql` из `docker-entrypoint-initdb.d`.

Пример:

```bash
curl http://127.0.0.1:8080/health
curl -X POST http://127.0.0.1:8080/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"login":"demo","first_name":"Demo","last_name":"User","password":"secret123"}'
```

## Локальная БД без Docker API

```bash
docker compose up -d postgres
# применить схему/данные вручную, если volume уже существовал:
docker compose exec postgres psql -U messenger -d messenger -f /docker-entrypoint-initdb.d/01-schema.sql
```

Сборка и запуск API (Linux / контейнер userver):

```bash
make docker-test-debug    # сборка + pytest + PostgreSQL testsuite
make docker-start-debug   # только сервис
```

Конфиг подключения: `pg-dsn` в [`configs/config_vars.yaml`](configs/config_vars.yaml).

## Только PostgreSQL + SQL

```bash
psql "postgresql://messenger:messenger@localhost:5432/messenger" -f schema.sql
psql "postgresql://messenger:messenger@localhost:5432/messenger" -f data.sql
```

Запросы из задания: [`queries.sql`](queries.sql).

## Структура проекта

```
schema.sql  data.sql  queries.sql  optimization.md
postgresql/schemas/messenger.sql
src/
  storage/          # PostgreSQL через userver::components::Postgres
  auth/             # Bearer-сессии (in-memory)
  handlers/         # REST handlers
configs/
  static_config.yaml
  config_vars.yaml
  config_vars.docker.yaml
  config_vars.testing.yaml
tests/
docker-compose.yaml
Dockerfile
```

## Партиционирование

Таблица `group_messages` разбита по `RANGE (created_at)` на месячные партиции (см. `schema.sql`). Стратегия описана в [`optimization.md`](optimization.md) §6.

## Тесты

```bash
make docker-test-debug
```

Используется `pytest_userver.plugins.postgresql` — временная БД и применение `postgresql/schemas/messenger.sql`.

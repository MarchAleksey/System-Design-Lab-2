# System-Design-Lab-2: Messenger REST API (Yandex userver, C++)

Домашнее задание 02 — REST API мессенджера (вариант 5) на **Yandex [userver](https://github.com/userver-framework/userver)** и **C++17**.

## Сущности

| Сущность | Описание |
|----------|----------|
| **User** | login, имя, фамилия, хэш пароля |
| **Group Chat** | групповой чат, участники, сообщения |
| **P2P Message** | личные сообщения между пользователями |

Хранилище: **in-memory**.

## Стек

- C++17, userver 2.8 (CPM)
- HTTP handlers (`HttpHandlerBase`)
- Bearer-токены (session-based JWT-подобная схема)
- OpenAPI: [`openapi.yaml`](openapi.yaml)
- Тесты: **pytest + userver testsuite**
- Docker: образ `ubuntu-24.04-userver`

## API (`/api/v1`)

| Метод | URL | Auth | Описание |
|-------|-----|------|----------|
| GET | `/health` | — | Health check |
| POST | `/auth/register` | — | Регистрация |
| POST | `/auth/login` | — | Логин → Bearer token |
| POST | `/users` | — | Создание пользователя |
| GET | `/users/by-login/{login}` | — | Поиск по логину |
| GET | `/users/search` | — | Поиск по маске имени/фамилии |
| POST | `/group-chats` | Bearer | Создание группового чата |
| POST | `/group-chats/{chat_id}/members` | Bearer | Добавить участника |
| POST | `/group-chats/{chat_id}/messages` | Bearer | Сообщение в чат |
| GET | `/group-chats/{chat_id}/messages` | Bearer | Список сообщений |
| POST | `/p2p/messages` | Bearer | P2P отправка |
| GET | `/p2p/messages` | Bearer | P2P список |

Аутентификация: заголовок `Authorization: Bearer <token>`. Проверка в `MessengerAuth::RequireUserId` (middleware на уровне handlers).

## Сборка и запуск (Linux / Docker)

Рекомендуется **Docker** (на Windows — WSL2 + Docker):

```bash
# Сборка и тесты в контейнере userver
make docker-test-debug

# Запуск сервиса локально в контейнере
make docker-start-debug
```

Без Docker (Ubuntu 22.04/24.04 с зависимостями userver):

```bash
make cmake-debug
make build-debug
make test-debug      
make start-debug     
```

Проверка:

```bash
curl http://127.0.0.1:8080/health
curl -X POST http://127.0.0.1:8080/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"login":"alice","first_name":"Alice","last_name":"Smith","password":"secret123"}'
```

## Docker Compose

```bash
docker compose up --build
```

API: http://localhost:8080

## Структура проекта

```
src/
  main.cpp                 # DaemonMain, компоненты userver
  storage/                 # In-memory хранилище (компонент)
  auth/                    # Регистрация, логин, Bearer middleware
  handlers/                # HTTP handlers REST API
configs/
  static_config.yaml       # Конфигурация handlers и компонентов
  config_vars.yaml
tests/
  test_messenger.py        # Функциональные тесты (testsuite)
openapi.yaml
CMakeLists.txt
Makefile
Dockerfile
docker-compose.yaml
```

## Конфигурация

| Переменная / config var | Описание |
|-------------------------|----------|
| `server-port` | Порт HTTP (по умолчанию 8080) |
| `jwt-secret` | Секрет для хэширования паролей и сессий |

## Тесты

```bash
make docker-test-debug
# или
make test-debug
```

Покрыты: пользователи, поиск, auth, групповые чаты, P2P, ошибки 400/401/404/409.

## Документация API

- Статическая спецификация: [`openapi.yaml`](openapi.yaml)
- Для интерактивного Swagger UI можно импортировать `openapi.yaml` в [Swagger Editor](https://editor.swagger.io/)


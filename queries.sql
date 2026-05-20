-- SQL for all messenger API operations (variant 5)
-- Placeholders: $1, $2, ... — as in PostgreSQL extended query protocol

-- =============================================================================
-- 1. Create user (POST /api/v1/users, POST /api/v1/auth/register)
-- =============================================================================
-- $1 login, $2 first_name, $3 last_name, $4 password_hash
INSERT INTO users (login, first_name, last_name, password_hash)
VALUES ($1, $2, $3, $4)
RETURNING id, login, first_name, last_name, created_at;

-- =============================================================================
-- 2. Find user by login (GET /api/v1/users/by-login/{login})
-- =============================================================================
-- $1 login
SELECT id, login, first_name, last_name, password_hash, created_at
FROM users
WHERE login = $1;

-- =============================================================================
-- 3. Search users by first/last name mask (GET /api/v1/users/search)
-- =============================================================================
-- $1 first_name mask (nullable), $2 last_name mask (nullable)
SELECT id, login, first_name, last_name, created_at
FROM users
WHERE ($1::text IS NULL OR first_name ILIKE '%' || $1 || '%')
  AND ($2::text IS NULL OR last_name ILIKE '%' || $2 || '%')
ORDER BY last_name, first_name;

-- =============================================================================
-- 4. Create group chat (POST /api/v1/group-chats)
-- =============================================================================
-- $1 name, $2 creator user id
WITH new_chat AS (
    INSERT INTO group_chats (name, created_by_id)
    VALUES ($1, $2)
    RETURNING id, name, created_by_id, created_at
),
add_creator AS (
    INSERT INTO group_chat_members (chat_id, user_id)
    SELECT id, created_by_id FROM new_chat
)
SELECT id, name, created_by_id, created_at FROM new_chat;

-- =============================================================================
-- 5. Add user to group chat (POST /api/v1/group-chats/{id}/members)
-- =============================================================================
-- $1 chat_id, $2 user_id
INSERT INTO group_chat_members (chat_id, user_id)
VALUES ($1, $2)
ON CONFLICT (chat_id, user_id) DO NOTHING
RETURNING chat_id, user_id;

-- =============================================================================
-- 6. Check membership (authorization helper)
-- =============================================================================
-- $1 chat_id, $2 user_id
SELECT 1
FROM group_chat_members
WHERE chat_id = $1 AND user_id = $2;

-- =============================================================================
-- 7. Add message to group chat (POST /api/v1/group-chats/{id}/messages)
-- =============================================================================
-- $1 chat_id, $2 sender_id, $3 content
INSERT INTO group_messages (chat_id, sender_id, content)
VALUES ($1, $2, $3)
RETURNING id, chat_id, sender_id, content, created_at;

-- =============================================================================
-- 8. List group chat messages (GET /api/v1/group-chats/{id}/messages)
-- =============================================================================
-- $1 chat_id, $2 limit, $3 offset
SELECT id, chat_id, sender_id, content, created_at
FROM group_messages
WHERE chat_id = $1
ORDER BY created_at ASC, id ASC
LIMIT $2 OFFSET $3;

-- =============================================================================
-- 9. Send P2P message (POST /api/v1/p2p/messages)
-- =============================================================================
-- $1 sender_id, $2 recipient_id, $3 content
INSERT INTO p2p_messages (sender_id, recipient_id, content)
VALUES ($1, $2, $3)
RETURNING id, sender_id, recipient_id, content, created_at;

-- =============================================================================
-- 10. List P2P messages for user (GET /api/v1/p2p/messages)
-- =============================================================================
-- $1 current user id, $2 optional peer id (NULL = all conversations), $3 limit, $4 offset
SELECT id, sender_id, recipient_id, content, created_at
FROM p2p_messages
WHERE (sender_id = $1 OR recipient_id = $1)
  AND ($2::bigint IS NULL
       OR (sender_id = $1 AND recipient_id = $2)
       OR (sender_id = $2 AND recipient_id = $1))
ORDER BY created_at ASC, id ASC
LIMIT $3 OFFSET $4;

-- =============================================================================
-- 11. Get user by id (auth validation)
-- =============================================================================
-- $1 user id
SELECT id, login, first_name, last_name, password_hash, created_at
FROM users
WHERE id = $1;

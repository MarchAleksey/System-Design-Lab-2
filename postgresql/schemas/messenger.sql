-- Messenger schema for userver testsuite (keep in sync with schema.sql)

CREATE TABLE users (
    id            BIGSERIAL PRIMARY KEY,
    login         VARCHAR(64)  NOT NULL,
    first_name    VARCHAR(128) NOT NULL,
    last_name     VARCHAR(128) NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT now(),
    CONSTRAINT users_login_unique UNIQUE (login),
    CONSTRAINT users_login_len CHECK (char_length(login) >= 1),
    CONSTRAINT users_names_not_empty CHECK (
        char_length(trim(first_name)) > 0 AND char_length(trim(last_name)) > 0
    )
);

CREATE TABLE group_chats (
    id            BIGSERIAL PRIMARY KEY,
    name          VARCHAR(256) NOT NULL,
    created_by_id BIGINT       NOT NULL REFERENCES users (id) ON DELETE RESTRICT,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT now(),
    CONSTRAINT group_chats_name_not_empty CHECK (char_length(trim(name)) > 0)
);

CREATE TABLE group_chat_members (
    chat_id   BIGINT      NOT NULL REFERENCES group_chats (id) ON DELETE CASCADE,
    user_id   BIGINT      NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    joined_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (chat_id, user_id)
);

CREATE TABLE group_messages (
    id         BIGSERIAL   NOT NULL,
    chat_id    BIGINT      NOT NULL,
    sender_id  BIGINT      NOT NULL,
    content    TEXT        NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT group_messages_content_not_empty CHECK (char_length(trim(content)) > 0),
    CONSTRAINT group_messages_chat_fk FOREIGN KEY (chat_id)
        REFERENCES group_chats (id) ON DELETE CASCADE,
    CONSTRAINT group_messages_sender_fk FOREIGN KEY (sender_id)
        REFERENCES users (id) ON DELETE RESTRICT,
    PRIMARY KEY (id, created_at)
) PARTITION BY RANGE (created_at);

CREATE TABLE group_messages_default PARTITION OF group_messages DEFAULT;
CREATE TABLE group_messages_2025_01 PARTITION OF group_messages
    FOR VALUES FROM ('2025-01-01') TO ('2025-02-01');
CREATE TABLE group_messages_2025_02 PARTITION OF group_messages
    FOR VALUES FROM ('2025-02-01') TO ('2025-03-01');
CREATE TABLE group_messages_2026_01 PARTITION OF group_messages
    FOR VALUES FROM ('2026-01-01') TO ('2026-02-01');
CREATE TABLE group_messages_2026_02 PARTITION OF group_messages
    FOR VALUES FROM ('2026-02-01') TO ('2026-03-01');

CREATE TABLE p2p_messages (
    id           BIGSERIAL PRIMARY KEY,
    sender_id    BIGINT      NOT NULL REFERENCES users (id) ON DELETE RESTRICT,
    recipient_id BIGINT      NOT NULL REFERENCES users (id) ON DELETE RESTRICT,
    content      TEXT        NOT NULL,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT p2p_messages_content_not_empty CHECK (char_length(trim(content)) > 0),
    CONSTRAINT p2p_messages_different_users CHECK (sender_id <> recipient_id)
);

CREATE INDEX idx_group_chats_created_by_id ON group_chats (created_by_id);
CREATE INDEX idx_group_chat_members_user_id ON group_chat_members (user_id);
CREATE INDEX idx_group_chat_members_chat_id ON group_chat_members (chat_id);
CREATE INDEX idx_group_messages_chat_created_at ON group_messages (chat_id, created_at);
CREATE INDEX idx_group_messages_sender_id ON group_messages (sender_id);
CREATE INDEX idx_p2p_messages_sender_created_at ON p2p_messages (sender_id, created_at);
CREATE INDEX idx_p2p_messages_recipient_created_at ON p2p_messages (recipient_id, created_at);
CREATE INDEX idx_users_first_name_lower ON users (lower(first_name));
CREATE INDEX idx_users_last_name_lower ON users (lower(last_name));

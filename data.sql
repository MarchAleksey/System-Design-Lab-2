-- Test data: at least 10 rows per table
-- Password hashes are placeholders (API uses SHA-256 of secret + password)

BEGIN;

INSERT INTO users (id, login, first_name, last_name, password_hash, created_at) VALUES
    (1,  'alice',     'Alice',     'Smith',    'seed_hash_01', '2025-01-10 10:00:00+00'),
    (2,  'bob',       'Bob',       'Johnson',  'seed_hash_02', '2025-01-11 11:00:00+00'),
    (3,  'carol',     'Caroline',  'Brown',    'seed_hash_03', '2025-01-12 12:00:00+00'),
    (4,  'dave',      'David',     'Miller',   'seed_hash_04', '2025-01-13 13:00:00+00'),
    (5,  'eve',       'Eve',       'Davis',    'seed_hash_05', '2025-01-14 14:00:00+00'),
    (6,  'frank',     'Frank',     'Wilson',   'seed_hash_06', '2025-01-15 15:00:00+00'),
    (7,  'grace',     'Grace',     'Taylor',   'seed_hash_07', '2025-01-16 16:00:00+00'),
    (8,  'henry',     'Henry',     'Anderson', 'seed_hash_08', '2025-01-17 17:00:00+00'),
    (9,  'iris',      'Iris',      'Thomas',   'seed_hash_09', '2025-01-18 18:00:00+00'),
    (10, 'jack',      'Jack',      'Jackson',  'seed_hash_10', '2025-01-19 19:00:00+00'),
    (11, 'kate',      'Katherine', 'White',    'seed_hash_11', '2025-01-20 20:00:00+00'),
    (12, 'liam',      'Liam',      'Harris',   'seed_hash_12', '2025-01-21 21:00:00+00');

SELECT setval(pg_get_serial_sequence('users', 'id'), 12);

INSERT INTO group_chats (id, name, created_by_id, created_at) VALUES
    (1,  'General',        1, '2025-02-01 09:00:00+00'),
    (2,  'Random',         2, '2025-02-02 09:00:00+00'),
    (3,  'Dev Team',       3, '2025-02-03 09:00:00+00'),
    (4,  'Design',         4, '2025-02-04 09:00:00+00'),
    (5,  'Ops',            5, '2025-02-05 09:00:00+00'),
    (6,  'Support',        6, '2025-02-06 09:00:00+00'),
    (7,  'Marketing',      7, '2025-02-07 09:00:00+00'),
    (8,  'HR',             8, '2025-02-08 09:00:00+00'),
    (9,  'Finance',        9, '2025-02-09 09:00:00+00'),
    (10, 'Legal',          10, '2025-02-10 09:00:00+00'),
    (11, 'Research',       11, '2025-02-11 09:00:00+00'),
    (12, 'Announcements',  12, '2025-02-12 09:00:00+00');

SELECT setval(pg_get_serial_sequence('group_chats', 'id'), 12);

INSERT INTO group_chat_members (chat_id, user_id, joined_at) VALUES
    (1, 1, '2025-02-01 09:05:00+00'),
    (1, 2, '2025-02-01 09:10:00+00'),
    (1, 3, '2025-02-01 09:15:00+00'),
    (2, 2, '2025-02-02 09:05:00+00'),
    (2, 4, '2025-02-02 09:10:00+00'),
    (3, 3, '2025-02-03 09:05:00+00'),
    (3, 5, '2025-02-03 09:10:00+00'),
    (4, 4, '2025-02-04 09:05:00+00'),
    (4, 6, '2025-02-04 09:10:00+00'),
    (5, 5, '2025-02-05 09:05:00+00'),
    (5, 7, '2025-02-05 09:10:00+00'),
    (6, 6, '2025-02-06 09:05:00+00'),
    (7, 7, '2025-02-07 09:05:00+00'),
    (8, 8, '2025-02-08 09:05:00+00'),
    (9, 9, '2025-02-09 09:05:00+00'),
    (10, 10, '2025-02-10 09:05:00+00'),
    (11, 11, '2025-02-11 09:05:00+00'),
    (12, 12, '2025-02-12 09:05:00+00');

INSERT INTO group_messages (chat_id, sender_id, content, created_at) VALUES
    (1, 1, 'Welcome to General',           '2025-02-01 10:00:00+00'),
    (1, 2, 'Thanks Alice',                 '2025-02-01 10:05:00+00'),
    (1, 3, 'Hi everyone',                  '2025-02-01 10:10:00+00'),
    (2, 2, 'Random chat kickoff',          '2025-02-02 10:00:00+00'),
    (2, 4, 'Sounds good',                  '2025-02-02 10:05:00+00'),
    (3, 3, 'Sprint planning',              '2025-02-03 10:00:00+00'),
    (3, 5, 'I will prepare the board',     '2025-02-03 10:05:00+00'),
    (4, 4, 'New mockups uploaded',         '2025-02-04 10:00:00+00'),
    (5, 5, 'Deploy at 18:00 UTC',          '2025-02-05 10:00:00+00'),
    (6, 6, 'Ticket #42 resolved',          '2025-02-06 10:00:00+00'),
    (7, 7, 'Campaign draft ready',         '2025-02-07 10:00:00+00'),
    (8, 8, 'Onboarding doc updated',       '2025-02-08 10:00:00+00');

INSERT INTO p2p_messages (sender_id, recipient_id, content, created_at) VALUES
    (1, 2, 'Hey Bob',                    '2025-03-01 08:00:00+00'),
    (2, 1, 'Hi Alice',                   '2025-03-01 08:05:00+00'),
    (1, 3, 'Carol, are you free?',       '2025-03-02 08:00:00+00'),
    (3, 1, 'Yes, in 10 minutes',         '2025-03-02 08:10:00+00'),
    (4, 5, 'Review the spec please',     '2025-03-03 08:00:00+00'),
    (5, 4, 'Done, left comments',        '2025-03-03 08:30:00+00'),
    (6, 7, 'Lunch today?',               '2025-03-04 12:00:00+00'),
    (7, 6, 'Sure, 13:00',                '2025-03-04 12:05:00+00'),
    (8, 9, 'Budget numbers attached',    '2025-03-05 09:00:00+00'),
    (9, 8, 'Received, thanks',           '2025-03-05 09:15:00+00'),
    (10, 11, 'Contract draft v2',        '2025-03-06 10:00:00+00'),
    (11, 12, 'Paper summary ready',      '2025-03-07 11:00:00+00');

COMMIT;

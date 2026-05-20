async def _register(service_client, login, first='Test', last='User'):
    return await service_client.post(
        '/api/v1/auth/register',
        json={
            'login': login,
            'first_name': first,
            'last_name': last,
            'password': 'password1',
        },
    )


async def _login(service_client, login):
    return await service_client.post(
        '/api/v1/auth/login',
        json={'login': login, 'password': 'password1'},
    )


async def test_health(service_client):
    response = await service_client.get('/health')
    assert response.status == 200
    assert response.json()['status'] == 'ok'


async def test_user_create_and_find(service_client):
    reg = await _register(service_client, 'alice', 'Alice', 'Smith')
    assert reg.status == 201

    found = await service_client.get('/api/v1/users/by-login/alice')
    assert found.status == 200
    assert found.json()['login'] == 'alice'


async def test_user_search_mask(service_client):
    await _register(service_client, 'carol', 'Caroline', 'Brown')
    response = await service_client.get(
        '/api/v1/users/search',
        params={'first_name_mask': 'Carol'},
    )
    assert response.status == 200
    assert len(response.json()) >= 1


async def test_search_requires_mask(service_client):
    response = await service_client.get('/api/v1/users/search')
    assert response.status == 400


async def test_duplicate_login(service_client):
    await _register(service_client, 'dupuser')
    again = await _register(service_client, 'dupuser')
    assert again.status == 409


async def test_login_invalid(service_client):
    await _register(service_client, 'dave')
    bad = await service_client.post(
        '/api/v1/auth/login',
        json={'login': 'dave', 'password': 'wrong'},
    )
    assert bad.status == 401


async def test_group_chat_flow(service_client):
    alice = await _register(service_client, 'group_alice')
    bob = await _register(service_client, 'group_bob')
    token = (await _login(service_client, 'group_alice')).json()['access_token']
    headers = {'Authorization': f'Bearer {token}'}

    chat = await service_client.post(
        '/api/v1/group-chats',
        json={'name': 'General'},
        headers=headers,
    )
    assert chat.status == 201
    chat_id = chat.json()['id']

    add = await service_client.post(
        f'/api/v1/group-chats/{chat_id}/members',
        json={'user_id': bob.json()['id']},
        headers=headers,
    )
    assert add.status == 204

    msg = await service_client.post(
        f'/api/v1/group-chats/{chat_id}/messages',
        json={'content': 'Hello team'},
        headers=headers,
    )
    assert msg.status == 201

    messages = await service_client.get(
        f'/api/v1/group-chats/{chat_id}/messages',
        headers=headers,
    )
    assert messages.status == 200
    assert len(messages.json()) == 1


async def test_group_chat_requires_auth(service_client):
    response = await service_client.post(
        '/api/v1/group-chats',
        json={'name': 'Secret'},
    )
    assert response.status == 401


async def test_p2p_messages(service_client):
    alice = await _register(service_client, 'p2p_alice')
    bob = await _register(service_client, 'p2p_bob')
    token = (await _login(service_client, 'p2p_alice')).json()['access_token']
    headers = {'Authorization': f'Bearer {token}'}

    sent = await service_client.post(
        '/api/v1/p2p/messages',
        json={'recipient_id': bob.json()['id'], 'content': 'Hi Bob'},
        headers=headers,
    )
    assert sent.status == 201

    inbox = await service_client.get('/api/v1/p2p/messages', headers=headers)
    assert inbox.status == 200
    assert len(inbox.json()) == 1

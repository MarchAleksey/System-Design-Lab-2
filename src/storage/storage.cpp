#include "storage/storage.hpp"

#include <stdexcept>

#include <userver/components/component.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <userver/utils/datetime.hpp>

#include "errors.hpp"

namespace messenger::storage {

namespace {

using userver::storages::postgres::ClusterHostType;

std::string ExternalId(const char* prefix, std::int64_t id) {
  return std::string(prefix) + std::to_string(id);
}

User RowToUser(const userver::storages::postgres::Row& row) {
  const auto id = row["id"].As<std::int64_t>();
  return User{
      .id = ExternalId("user-", id),
      .login = row["login"].As<std::string>(),
      .first_name = row["first_name"].As<std::string>(),
      .last_name = row["last_name"].As<std::string>(),
      .password_hash = row["password_hash"].As<std::string>(),
      .created_at = row["created_at"].As<std::chrono::system_clock::time_point>(),
  };
}

GroupChat RowToGroupChat(const userver::storages::postgres::Row& row) {
  const auto id = row["id"].As<std::int64_t>();
  const auto creator = row["created_by_id"].As<std::int64_t>();
  return GroupChat{
      .id = ExternalId("chat-", id),
      .name = row["name"].As<std::string>(),
      .created_by_id = ExternalId("user-", creator),
      .created_at = row["created_at"].As<std::chrono::system_clock::time_point>(),
  };
}

GroupMessage RowToGroupMessage(const userver::storages::postgres::Row& row) {
  const auto id = row["id"].As<std::int64_t>();
  const auto chat_id = row["chat_id"].As<std::int64_t>();
  const auto sender_id = row["sender_id"].As<std::int64_t>();
  return GroupMessage{
      .id = ExternalId("gmsg-", id),
      .chat_id = ExternalId("chat-", chat_id),
      .sender_id = ExternalId("user-", sender_id),
      .content = row["content"].As<std::string>(),
      .created_at = row["created_at"].As<std::chrono::system_clock::time_point>(),
  };
}

P2PMessage RowToP2PMessage(const userver::storages::postgres::Row& row) {
  const auto id = row["id"].As<std::int64_t>();
  const auto sender_id = row["sender_id"].As<std::int64_t>();
  const auto recipient_id = row["recipient_id"].As<std::int64_t>();
  return P2PMessage{
      .id = ExternalId("pmsg-", id),
      .sender_id = ExternalId("user-", sender_id),
      .recipient_id = ExternalId("user-", recipient_id),
      .content = row["content"].As<std::string>(),
      .created_at = row["created_at"].As<std::chrono::system_clock::time_point>(),
  };
}

}  // namespace

userver::formats::json::Value UserToJson(const User& user) {
  userver::formats::json::ValueBuilder b;
  b["id"] = user.id;
  b["login"] = user.login;
  b["first_name"] = user.first_name;
  b["last_name"] = user.last_name;
  b["created_at"] = userver::utils::datetime::Timestring(user.created_at);
  return b.ExtractValue();
}

userver::formats::json::Value GroupChatToJson(const GroupChat& chat) {
  userver::formats::json::ValueBuilder b;
  b["id"] = chat.id;
  b["name"] = chat.name;
  b["created_by_id"] = chat.created_by_id;
  b["created_at"] = userver::utils::datetime::Timestring(chat.created_at);
  return b.ExtractValue();
}

userver::formats::json::Value GroupMessageToJson(const GroupMessage& msg) {
  userver::formats::json::ValueBuilder b;
  b["id"] = msg.id;
  b["chat_id"] = msg.chat_id;
  b["sender_id"] = msg.sender_id;
  b["content"] = msg.content;
  b["created_at"] = userver::utils::datetime::Timestring(msg.created_at);
  return b.ExtractValue();
}

userver::formats::json::Value P2PMessageToJson(const P2PMessage& msg) {
  userver::formats::json::ValueBuilder b;
  b["id"] = msg.id;
  b["sender_id"] = msg.sender_id;
  b["recipient_id"] = msg.recipient_id;
  b["content"] = msg.content;
  b["created_at"] = userver::utils::datetime::Timestring(msg.created_at);
  return b.ExtractValue();
}

MessengerStorage::MessengerStorage(const userver::components::ComponentConfig& config,
                                   const userver::components::ComponentContext& context)
    : ComponentBase(config, context) {
  const auto component_name = config["postgres-component"].As<std::string>("postgres-database");
  pg_ = context.FindComponent<userver::components::Postgres>(component_name).GetCluster();
}

std::int64_t MessengerStorage::ParseEntityId(const std::string& external_id,
                                             const char* prefix) {
  const std::string expected{prefix};
  if (external_id.size() <= expected.size() ||
      external_id.compare(0, expected.size(), expected) != 0) {
    throw errors::ClientError(errors::Msg("Invalid identifier format"));
  }
  try {
    return std::stoll(external_id.substr(expected.size()));
  } catch (const std::exception&) {
    throw errors::ClientError(errors::Msg("Invalid identifier format"));
  }
}

User MessengerStorage::CreateUser(const std::string& login, const std::string& first_name,
                                  const std::string& last_name,
                                  const std::string& password_hash) {
  try {
    const auto result = pg_->Execute(
        ClusterHostType::kMaster,
        "INSERT INTO users (login, first_name, last_name, password_hash) "
        "VALUES ($1, $2, $3, $4) "
        "RETURNING id, login, first_name, last_name, password_hash, created_at",
        login, first_name, last_name, password_hash);
    return RowToUser(result[0]);
  } catch (const userver::storages::postgres::UniqueViolation&) {
    throw errors::ConflictError(errors::Msg("Login already taken"));
  }
}

std::optional<User> MessengerStorage::FindByLogin(const std::string& login) const {
  const auto result = pg_->Execute(
      ClusterHostType::kMaster,
      "SELECT id, login, first_name, last_name, password_hash, created_at "
      "FROM users WHERE login = $1",
      login);
  if (result.IsEmpty()) return std::nullopt;
  return RowToUser(result[0]);
}

std::vector<User> MessengerStorage::SearchByMask(
    const std::optional<std::string>& first_name_mask,
    const std::optional<std::string>& last_name_mask) const {
  const auto result = pg_->Execute(
      ClusterHostType::kMaster,
      "SELECT id, login, first_name, last_name, password_hash, created_at "
      "FROM users "
      "WHERE ($1::text IS NULL OR first_name ILIKE '%' || $1 || '%') "
      "  AND ($2::text IS NULL OR last_name ILIKE '%' || $2 || '%') "
      "ORDER BY last_name, first_name",
      first_name_mask, last_name_mask);

  std::vector<User> users;
  users.reserve(result.Size());
  for (const auto& row : result) {
    users.push_back(RowToUser(row));
  }
  return users;
}

GroupChat MessengerStorage::CreateGroupChat(const std::string& name,
                                            const std::string& creator_id) {
  const auto creator_pk = ParseEntityId(creator_id, "user-");
  const auto result = pg_->Execute(
      ClusterHostType::kMaster,
      "WITH new_chat AS ("
      "  INSERT INTO group_chats (name, created_by_id) VALUES ($1, $2) "
      "  RETURNING id, name, created_by_id, created_at"
      "), add_creator AS ("
      "  INSERT INTO group_chat_members (chat_id, user_id) "
      "  SELECT id, created_by_id FROM new_chat"
      ") "
      "SELECT id, name, created_by_id, created_at FROM new_chat",
      name, creator_pk);
  return RowToGroupChat(result[0]);
}

void MessengerStorage::AddMember(const std::string& chat_id, const std::string& user_id) {
  const auto chat_pk = ParseEntityId(chat_id, "chat-");
  const auto user_pk = ParseEntityId(user_id, "user-");

  if (!GetChat(chat_id)) {
    throw errors::ResourceNotFound(errors::Msg("Group chat not found"));
  }

  const auto user_row = pg_->Execute(ClusterHostType::kMaster,
                                     "SELECT 1 FROM users WHERE id = $1", user_pk);
  if (user_row.IsEmpty()) {
    throw errors::ResourceNotFound(errors::Msg("User to add not found"));
  }

  const auto member_row =
      pg_->Execute(ClusterHostType::kMaster,
                   "SELECT 1 FROM group_chat_members WHERE chat_id = $1 AND user_id = $2",
                   chat_pk, user_pk);
  if (!member_row.IsEmpty()) {
    throw errors::ConflictError(errors::Msg("User already in chat"));
  }

  pg_->Execute(ClusterHostType::kMaster,
               "INSERT INTO group_chat_members (chat_id, user_id) VALUES ($1, $2)", chat_pk,
               user_pk);
}

bool MessengerStorage::IsMember(const std::string& chat_id, const std::string& user_id) const {
  const auto chat_pk = ParseEntityId(chat_id, "chat-");
  const auto user_pk = ParseEntityId(user_id, "user-");
  const auto result = pg_->Execute(ClusterHostType::kMaster,
                                   "SELECT 1 FROM group_chat_members "
                                   "WHERE chat_id = $1 AND user_id = $2",
                                   chat_pk, user_pk);
  return !result.IsEmpty();
}

std::optional<GroupChat> MessengerStorage::GetChat(const std::string& chat_id) const {
  const auto chat_pk = ParseEntityId(chat_id, "chat-");
  const auto result = pg_->Execute(ClusterHostType::kMaster,
                                   "SELECT id, name, created_by_id, created_at "
                                   "FROM group_chats WHERE id = $1",
                                   chat_pk);
  if (result.IsEmpty()) return std::nullopt;
  return RowToGroupChat(result[0]);
}

GroupMessage MessengerStorage::AddGroupMessage(const std::string& chat_id,
                                               const std::string& sender_id,
                                               const std::string& content) {
  const auto chat_pk = ParseEntityId(chat_id, "chat-");
  const auto sender_pk = ParseEntityId(sender_id, "user-");

  if (!GetChat(chat_id)) {
    throw errors::ResourceNotFound(errors::Msg("Group chat not found"));
  }

  const auto result = pg_->Execute(
      ClusterHostType::kMaster,
      "INSERT INTO group_messages (chat_id, sender_id, content) "
      "VALUES ($1, $2, $3) "
      "RETURNING id, chat_id, sender_id, content, created_at",
      chat_pk, sender_pk, content);
  return RowToGroupMessage(result[0]);
}

std::vector<GroupMessage> MessengerStorage::ListGroupMessages(const std::string& chat_id,
                                                              int limit, int offset) const {
  const auto chat_pk = ParseEntityId(chat_id, "chat-");
  const auto result = pg_->Execute(
      ClusterHostType::kMaster,
      "SELECT id, chat_id, sender_id, content, created_at "
      "FROM group_messages WHERE chat_id = $1 "
      "ORDER BY created_at ASC, id ASC "
      "LIMIT $2 OFFSET $3",
      chat_pk, limit, offset);

  std::vector<GroupMessage> messages;
  messages.reserve(result.Size());
  for (const auto& row : result) {
    messages.push_back(RowToGroupMessage(row));
  }
  return messages;
}

P2PMessage MessengerStorage::AddP2PMessage(const std::string& sender_id,
                                           const std::string& recipient_id,
                                           const std::string& content) {
  const auto sender_pk = ParseEntityId(sender_id, "user-");
  const auto recipient_pk = ParseEntityId(recipient_id, "user-");

  try {
    const auto result = pg_->Execute(
        ClusterHostType::kMaster,
        "INSERT INTO p2p_messages (sender_id, recipient_id, content) "
        "VALUES ($1, $2, $3) "
        "RETURNING id, sender_id, recipient_id, content, created_at",
        sender_pk, recipient_pk, content);
    return RowToP2PMessage(result[0]);
  } catch (const userver::storages::postgres::ForeignKeyViolation&) {
    throw errors::ResourceNotFound(errors::Msg("Recipient not found"));
  } catch (const userver::storages::postgres::CheckViolation&) {
    throw errors::ClientError(errors::Msg("Cannot message yourself"));
  }
}

std::vector<P2PMessage> MessengerStorage::ListP2PMessages(
    const std::string& user_id, const std::optional<std::string>& peer_id, int limit,
    int offset) const {
  const auto user_pk = ParseEntityId(user_id, "user-");
  std::optional<std::int64_t> peer_pk;
  if (peer_id) {
    peer_pk = ParseEntityId(*peer_id, "user-");
  }

  const auto result = pg_->Execute(
      ClusterHostType::kMaster,
      "SELECT id, sender_id, recipient_id, content, created_at "
      "FROM p2p_messages "
      "WHERE (sender_id = $1 OR recipient_id = $1) "
      "  AND ($2::bigint IS NULL "
      "       OR (sender_id = $1 AND recipient_id = $2) "
      "       OR (sender_id = $2 AND recipient_id = $1)) "
      "ORDER BY created_at ASC, id ASC "
      "LIMIT $3 OFFSET $4",
      user_pk, peer_pk, limit, offset);

  std::vector<P2PMessage> messages;
  messages.reserve(result.Size());
  for (const auto& row : result) {
    messages.push_back(RowToP2PMessage(row));
  }
  return messages;
}

std::optional<User> MessengerStorage::GetUser(const std::string& user_id) const {
  const auto user_pk = ParseEntityId(user_id, "user-");
  const auto result = pg_->Execute(ClusterHostType::kMaster,
                                   "SELECT id, login, first_name, last_name, password_hash, "
                                   "created_at FROM users WHERE id = $1",
                                   user_pk);
  if (result.IsEmpty()) return std::nullopt;
  return RowToUser(result[0]);
}

}  // namespace messenger::storage

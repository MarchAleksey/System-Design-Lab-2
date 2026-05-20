#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <userver/components/component_base.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace messenger::storage {

struct User {
  std::string id;
  std::string login;
  std::string first_name;
  std::string last_name;
  std::string password_hash;
  std::chrono::system_clock::time_point created_at;
};

struct GroupChat {
  std::string id;
  std::string name;
  std::string created_by_id;
  std::chrono::system_clock::time_point created_at;
};

struct GroupMessage {
  std::string id;
  std::string chat_id;
  std::string sender_id;
  std::string content;
  std::chrono::system_clock::time_point created_at;
};

struct P2PMessage {
  std::string id;
  std::string sender_id;
  std::string recipient_id;
  std::string content;
  std::chrono::system_clock::time_point created_at;
};

userver::formats::json::Value UserToJson(const User& user);
userver::formats::json::Value GroupChatToJson(const GroupChat& chat);
userver::formats::json::Value GroupMessageToJson(const GroupMessage& msg);
userver::formats::json::Value P2PMessageToJson(const P2PMessage& msg);

class MessengerStorage final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "messenger-storage";

  MessengerStorage(const userver::components::ComponentConfig& config,
                   const userver::components::ComponentContext& context);

  User CreateUser(const std::string& login, const std::string& first_name,
                  const std::string& last_name, const std::string& password_hash);

  std::optional<User> FindByLogin(const std::string& login) const;
  std::vector<User> SearchByMask(const std::optional<std::string>& first_name_mask,
                                 const std::optional<std::string>& last_name_mask) const;

  GroupChat CreateGroupChat(const std::string& name, const std::string& creator_id);
  void AddMember(const std::string& chat_id, const std::string& user_id);
  bool IsMember(const std::string& chat_id, const std::string& user_id) const;
  std::optional<GroupChat> GetChat(const std::string& chat_id) const;

  GroupMessage AddGroupMessage(const std::string& chat_id, const std::string& sender_id,
                               const std::string& content);
  std::vector<GroupMessage> ListGroupMessages(const std::string& chat_id, int limit,
                                              int offset) const;

  P2PMessage AddP2PMessage(const std::string& sender_id, const std::string& recipient_id,
                           const std::string& content);
  std::vector<P2PMessage> ListP2PMessages(const std::string& user_id,
                                          const std::optional<std::string>& peer_id,
                                          int limit, int offset) const;

  std::optional<User> GetUser(const std::string& user_id) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;

  static std::int64_t ParseEntityId(const std::string& external_id, const char* prefix);
};

}  // namespace messenger::storage

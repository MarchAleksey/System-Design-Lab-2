#include "storage/storage.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <userver/components/component.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utils/datetime.hpp>

#include "errors.hpp"

namespace messenger::storage
{

  namespace
  {

    std::string ToLower(std::string s)
    {
      for (auto &c : s)
      {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      return s;
    }

    bool IContains(const std::string &haystack, const std::string &needle)
    {
      if (needle.empty())
        return true;
      return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
    }

  }

  userver::formats::json::Value UserToJson(const User &user)
  {
    userver::formats::json::ValueBuilder b;
    b["id"] = user.id;
    b["login"] = user.login;
    b["first_name"] = user.first_name;
    b["last_name"] = user.last_name;
    b["created_at"] = userver::utils::datetime::Timestring(user.created_at);
    return b.ExtractValue();
  }

  userver::formats::json::Value GroupChatToJson(const GroupChat &chat)
  {
    userver::formats::json::ValueBuilder b;
    b["id"] = chat.id;
    b["name"] = chat.name;
    b["created_by_id"] = chat.created_by_id;
    b["created_at"] = userver::utils::datetime::Timestring(chat.created_at);
    return b.ExtractValue();
  }

  userver::formats::json::Value GroupMessageToJson(const GroupMessage &msg)
  {
    userver::formats::json::ValueBuilder b;
    b["id"] = msg.id;
    b["chat_id"] = msg.chat_id;
    b["sender_id"] = msg.sender_id;
    b["content"] = msg.content;
    b["created_at"] = userver::utils::datetime::Timestring(msg.created_at);
    return b.ExtractValue();
  }

  userver::formats::json::Value P2PMessageToJson(const P2PMessage &msg)
  {
    userver::formats::json::ValueBuilder b;
    b["id"] = msg.id;
    b["sender_id"] = msg.sender_id;
    b["recipient_id"] = msg.recipient_id;
    b["content"] = msg.content;
    b["created_at"] = userver::utils::datetime::Timestring(msg.created_at);
    return b.ExtractValue();
  }

  MessengerStorage::MessengerStorage(const userver::components::ComponentConfig &config,
                                     const userver::components::ComponentContext &context)
      : ComponentBase(config, context) {}

  std::string MessengerStorage::NextId(const char *prefix)
  {
    std::ostringstream oss;
    oss << prefix << '-' << ++id_counter_;
    return oss.str();
  }

  User MessengerStorage::CreateUser(const std::string &login, const std::string &first_name,
                                    const std::string &last_name,
                                    const std::string &password_hash)
  {
    std::lock_guard lock(mutex_);
    if (login_index_.count(login))
    {
      throw errors::ConflictError(errors::Msg("Login already taken"));
    }
    User user{
        .id = NextId("user"),
        .login = login,
        .first_name = first_name,
        .last_name = last_name,
        .password_hash = password_hash,
        .created_at = userver::utils::datetime::Now(),
    };
    users_.emplace(user.id, user);
    login_index_[login] = user.id;
    return user;
  }

  std::optional<User> MessengerStorage::FindByLogin(const std::string &login) const
  {
    std::lock_guard lock(mutex_);
    auto it = login_index_.find(login);
    if (it == login_index_.end())
      return std::nullopt;
    return users_.at(it->second);
  }

  std::vector<User> MessengerStorage::SearchByMask(
      const std::optional<std::string> &first_name_mask,
      const std::optional<std::string> &last_name_mask) const
  {
    std::lock_guard lock(mutex_);
    std::vector<User> result;
    for (const auto &[_, user] : users_)
    {
      bool match = true;
      if (first_name_mask && !IContains(user.first_name, *first_name_mask))
        match = false;
      if (last_name_mask && !IContains(user.last_name, *last_name_mask))
        match = false;
      if (match)
        result.push_back(user);
    }
    return result;
  }

  GroupChat MessengerStorage::CreateGroupChat(const std::string &name,
                                              const std::string &creator_id)
  {
    std::lock_guard lock(mutex_);
    GroupChat chat{
        .id = NextId("chat"),
        .name = name,
        .created_by_id = creator_id,
        .created_at = userver::utils::datetime::Now(),
    };
    chats_.emplace(chat.id, chat);
    members_[chat.id][creator_id] = true;
    return chat;
  }

  void MessengerStorage::AddMember(const std::string &chat_id, const std::string &user_id)
  {
    std::lock_guard lock(mutex_);
    if (!chats_.count(chat_id))
    {
      throw errors::ResourceNotFound(errors::Msg("Group chat not found"));
    }
    if (!users_.count(user_id))
    {
      throw errors::ResourceNotFound(errors::Msg("User to add not found"));
    }
    auto &chat_members = members_[chat_id];
    if (chat_members.count(user_id))
    {
      throw errors::ConflictError(errors::Msg("User already in chat"));
    }
    chat_members[user_id] = true;
  }

  bool MessengerStorage::IsMember(const std::string &chat_id, const std::string &user_id) const
  {
    std::lock_guard lock(mutex_);
    auto chat_it = members_.find(chat_id);
    if (chat_it == members_.end())
      return false;
    return chat_it->second.count(user_id) > 0;
  }

  std::optional<GroupChat> MessengerStorage::GetChat(const std::string &chat_id) const
  {
    std::lock_guard lock(mutex_);
    auto it = chats_.find(chat_id);
    if (it == chats_.end())
      return std::nullopt;
    return it->second;
  }

  GroupMessage MessengerStorage::AddGroupMessage(const std::string &chat_id,
                                                 const std::string &sender_id,
                                                 const std::string &content)
  {
    std::lock_guard lock(mutex_);
    if (!chats_.count(chat_id))
    {
      throw errors::ResourceNotFound(errors::Msg("Group chat not found"));
    }
    GroupMessage msg{
        .id = NextId("gmsg"),
        .chat_id = chat_id,
        .sender_id = sender_id,
        .content = content,
        .created_at = userver::utils::datetime::Now(),
    };
    group_messages_.push_back(msg);
    return msg;
  }

  std::vector<GroupMessage> MessengerStorage::ListGroupMessages(const std::string &chat_id,
                                                                int limit, int offset) const
  {
    std::lock_guard lock(mutex_);
    std::vector<GroupMessage> filtered;
    for (const auto &m : group_messages_)
    {
      if (m.chat_id == chat_id)
        filtered.push_back(m);
    }
    if (offset >= static_cast<int>(filtered.size()))
      return {};
    auto end = std::min(offset + limit, static_cast<int>(filtered.size()));
    return {filtered.begin() + offset, filtered.begin() + end};
  }

  P2PMessage MessengerStorage::AddP2PMessage(const std::string &sender_id,
                                             const std::string &recipient_id,
                                             const std::string &content)
  {
    std::lock_guard lock(mutex_);
    if (!users_.count(recipient_id))
    {
      throw errors::ResourceNotFound(errors::Msg("Recipient not found"));
    }
    P2PMessage msg{
        .id = NextId("pmsg"),
        .sender_id = sender_id,
        .recipient_id = recipient_id,
        .content = content,
        .created_at = userver::utils::datetime::Now(),
    };
    p2p_messages_.push_back(msg);
    return msg;
  }

  std::vector<P2PMessage> MessengerStorage::ListP2PMessages(
      const std::string &user_id, const std::optional<std::string> &peer_id, int limit,
      int offset) const
  {
    std::lock_guard lock(mutex_);
    std::vector<P2PMessage> filtered;
    for (const auto &m : p2p_messages_)
    {
      const bool involves_user = m.sender_id == user_id || m.recipient_id == user_id;
      if (!involves_user)
        continue;
      if (peer_id)
      {
        const bool with_peer = (m.sender_id == user_id && m.recipient_id == *peer_id) ||
                               (m.sender_id == *peer_id && m.recipient_id == user_id);
        if (!with_peer)
          continue;
      }
      filtered.push_back(m);
    }
    if (offset >= static_cast<int>(filtered.size()))
      return {};
    auto end = std::min(offset + limit, static_cast<int>(filtered.size()));
    return {filtered.begin() + offset, filtered.begin() + end};
  }

  std::optional<User> MessengerStorage::GetUser(const std::string &user_id) const
  {
    std::lock_guard lock(mutex_);
    auto it = users_.find(user_id);
    if (it == users_.end())
      return std::nullopt;
    return it->second;
  }

}

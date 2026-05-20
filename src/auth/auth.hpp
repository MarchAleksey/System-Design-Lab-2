#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <userver/components/component_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/formats/json/value.hpp>

namespace messenger::storage {
class MessengerStorage;
struct User;
}  // namespace messenger::storage

namespace messenger::auth {

class MessengerAuth final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "messenger-auth";

  MessengerAuth(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context);

  storage::User Register(const std::string& login, const std::string& first_name,
                         const std::string& last_name, const std::string& password);

  userver::formats::json::Value Login(const std::string& login, const std::string& password);

  std::string RequireUserId(const userver::server::http::HttpRequest& request) const;

 private:
  std::string HashPassword(const std::string& password) const;
  std::string IssueToken(const std::string& user_id);
  std::optional<std::string> ResolveToken(const std::string& token) const;

  storage::MessengerStorage& storage_;
  std::string jwt_secret_;

  mutable userver::engine::Mutex mutex_;
  std::unordered_map<std::string, std::string> sessions_;
  std::uint64_t token_counter_{0};
};

}  // namespace messenger::auth

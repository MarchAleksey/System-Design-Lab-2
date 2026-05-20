#include "auth/auth.hpp"

#include <sstream>

#include <userver/components/component.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_request.hpp>

#include "errors.hpp"
#include "storage/storage.hpp"

namespace messenger::auth {

MessengerAuth::MessengerAuth(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      storage_(context.FindComponent<storage::MessengerStorage>()),
      jwt_secret_(config["jwt-secret"].As<std::string>("dev-secret-change-me")) {}

std::string MessengerAuth::HashPassword(const std::string& password) const {
  const auto data = jwt_secret_ + ":" + password;
  return userver::crypto::hash::Sha256(data, userver::crypto::hash::OutputEncoding::kHex);
}

std::string MessengerAuth::IssueToken(const std::string& user_id) {
  std::lock_guard lock(mutex_);
  std::ostringstream oss;
  oss << "tok-" << ++token_counter_;
  const auto token = oss.str();
  sessions_[token] = user_id;
  return token;
}

std::optional<std::string> MessengerAuth::ResolveToken(const std::string& token) const {
  std::lock_guard lock(mutex_);
  auto it = sessions_.find(token);
  if (it == sessions_.end()) return std::nullopt;
  return it->second;
}

storage::User MessengerAuth::Register(const std::string& login, const std::string& first_name,
                                      const std::string& last_name, const std::string& password) {
  return storage_.CreateUser(login, first_name, last_name, HashPassword(password));
}

userver::formats::json::Value MessengerAuth::Login(const std::string& login,
                                                   const std::string& password) {
  auto user = storage_.FindByLogin(login);
  if (!user || user->password_hash != HashPassword(password)) {
    throw errors::Unauthorized(errors::Msg("Invalid login or password"));
  }
  const auto token = IssueToken(user->id);
  userver::formats::json::ValueBuilder b;
  b["access_token"] = token;
  b["token_type"] = "bearer";
  return b.ExtractValue();
}

std::string MessengerAuth::RequireUserId(const userver::server::http::HttpRequest& request) const {
  const auto& header = request.GetHeader("Authorization");
  constexpr std::string_view prefix = "Bearer ";
  if (header.empty() || header.rfind(prefix.data(), 0) != 0) {
    throw errors::Unauthorized(errors::Msg("Authentication required"));
  }
  const auto token = header.substr(prefix.size());
  auto user_id = ResolveToken(token);
  if (!user_id) {
    throw errors::Unauthorized(errors::Msg("Invalid or expired token"));
  }
  if (!storage_.GetUser(*user_id)) {
    throw errors::Unauthorized(errors::Msg("User not found"));
  }
  return *user_id;
}

}  // namespace messenger::auth

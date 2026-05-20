#include "handlers/handlers.hpp"

#include <optional>
#include <string>

#include <userver/components/component.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/auth.hpp"
#include "errors.hpp"
#include "storage/storage.hpp"

namespace messenger::handlers {

namespace {

using userver::formats::json::Value;
using userver::server::handlers::HttpHandlerBase;
using errors::ClientError;
using errors::Forbidden;
using errors::Msg;
using errors::ResourceNotFound;
using userver::server::http::HttpRequest;
using userver::server::request::RequestContext;
using storage::GroupChatToJson;
using storage::GroupMessageToJson;
using storage::MessengerStorage;
using storage::P2PMessageToJson;
using storage::UserToJson;

std::string JsonResponse(const Value& value) {
  return userver::formats::json::ToString(value);
}

std::string RequireField(const Value& json, const char* name) {
  if (!json.HasMember(name)) {
    throw ClientError(Msg(std::string("Missing field: ") + name));
  }
  return json[name].As<std::string>();
}

int ParseLimit(const HttpRequest& request, int default_value, int max_value) {
  const auto arg = request.GetArg("limit");
  if (arg.empty()) return default_value;
  const int v = std::stoi(arg);
  if (v < 1 || v > max_value) {
    throw ClientError(Msg("Invalid limit parameter"));
  }
  return v;
}

int ParseOffset(const HttpRequest& request) {
  const auto arg = request.GetArg("offset");
  if (arg.empty()) return 0;
  const int v = std::stoi(arg);
  if (v < 0) throw ClientError(Msg("Invalid offset parameter"));
  return v;
}

class HealthHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-health";
  using HttpHandlerBase::HttpHandlerBase;

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    request.GetHttpResponse().SetContentType("application/json");
    return R"({"status":"ok"})";
  }
};

class AuthRegisterHandlerV2 final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-auth-register";
  AuthRegisterHandlerV2(const userver::components::ComponentConfig& config,
                          const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto body = userver::formats::json::FromString(request.RequestBody());
    auto user = auth_.Register(RequireField(body, "login"), RequireField(body, "first_name"),
                               RequireField(body, "last_name"), RequireField(body, "password"));
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(UserToJson(user));
  }

 private:
  auth::MessengerAuth& auth_;
};

class AuthLoginHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-auth-login";
  AuthLoginHandler(const userver::components::ComponentConfig& config,
                   const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto body = userver::formats::json::FromString(request.RequestBody());
    const auto result = auth_.Login(RequireField(body, "login"), RequireField(body, "password"));
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(result);
  }

 private:
  auth::MessengerAuth& auth_;
};

class UsersCreateHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-users-create";
  UsersCreateHandler(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto body = userver::formats::json::FromString(request.RequestBody());
    auto user = auth_.Register(RequireField(body, "login"), RequireField(body, "first_name"),
                               RequireField(body, "last_name"), RequireField(body, "password"));
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(UserToJson(user));
  }

 private:
  auth::MessengerAuth& auth_;
};

class UsersByLoginHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-users-by-login";
  UsersByLoginHandler(const userver::components::ComponentConfig& config,
                      const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto login = request.GetPathArg("login");
    auto user = storage_.FindByLogin(login);
    if (!user) throw ResourceNotFound(Msg("User not found"));
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(UserToJson(*user));
  }

 private:
  MessengerStorage& storage_;
};

class UsersSearchHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-users-search";
  UsersSearchHandler(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto first_mask = request.GetArg("first_name_mask");
    const auto last_mask = request.GetArg("last_name_mask");
    if (first_mask.empty() && last_mask.empty()) {
      throw ClientError(Msg("Provide at least one of first_name_mask or last_name_mask"));
    }
    std::optional<std::string> f = first_mask.empty() ? std::nullopt : std::optional(first_mask);
    std::optional<std::string> l = last_mask.empty() ? std::nullopt : std::optional(last_mask);
    const auto users = storage_.SearchByMask(f, l);
    userver::formats::json::ValueBuilder arr{userver::formats::json::Type::kArray};
    for (const auto& u : users) {
      arr.PushBack(UserToJson(u));
    }
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(arr.ExtractValue());
  }

 private:
  MessengerStorage& storage_;
};

class GroupChatsCreateHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-group-chats-create";
  GroupChatsCreateHandler(const userver::components::ComponentConfig& config,
                          const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto user_id = auth_.RequireUserId(request);
    const auto body = userver::formats::json::FromString(request.RequestBody());
    auto chat = storage_.CreateGroupChat(RequireField(body, "name"), user_id);
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(GroupChatToJson(chat));
  }

 private:
  MessengerStorage& storage_;
  auth::MessengerAuth& auth_;
};

class GroupChatsAddMemberHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-group-chats-add-member";
  GroupChatsAddMemberHandler(const userver::components::ComponentConfig& config,
                             const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto user_id = auth_.RequireUserId(request);
    const auto chat_id = request.GetPathArg("chat_id");
    if (!storage_.GetChat(chat_id)) throw ResourceNotFound(Msg("Group chat not found"));
    if (!storage_.IsMember(chat_id, user_id)) {
      throw Forbidden(Msg("Not a member of this chat"));
    }
    const auto body = userver::formats::json::FromString(request.RequestBody());
    storage_.AddMember(chat_id, RequireField(body, "user_id"));
    request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
    return {};
  }

 private:
  MessengerStorage& storage_;
  auth::MessengerAuth& auth_;
};

class GroupChatsPostMessageHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-group-chats-post-message";
  GroupChatsPostMessageHandler(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto user_id = auth_.RequireUserId(request);
    const auto chat_id = request.GetPathArg("chat_id");
    if (!storage_.GetChat(chat_id)) throw ResourceNotFound(Msg("Group chat not found"));
    if (!storage_.IsMember(chat_id, user_id)) {
      throw Forbidden(Msg("Not a member of this chat"));
    }
    const auto body = userver::formats::json::FromString(request.RequestBody());
    auto msg = storage_.AddGroupMessage(chat_id, user_id, RequireField(body, "content"));
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(GroupMessageToJson(msg));
  }

 private:
  MessengerStorage& storage_;
  auth::MessengerAuth& auth_;
};

class GroupChatsGetMessagesHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-group-chats-get-messages";
  GroupChatsGetMessagesHandler(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto user_id = auth_.RequireUserId(request);
    const auto chat_id = request.GetPathArg("chat_id");
    if (!storage_.GetChat(chat_id)) throw ResourceNotFound(Msg("Group chat not found"));
    if (!storage_.IsMember(chat_id, user_id)) {
      throw Forbidden(Msg("Not a member of this chat"));
    }
    const auto messages =
        storage_.ListGroupMessages(chat_id, ParseLimit(request, 50, 200), ParseOffset(request));
    userver::formats::json::ValueBuilder arr{userver::formats::json::Type::kArray};
    for (const auto& m : messages) {
      arr.PushBack(GroupMessageToJson(m));
    }
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(arr.ExtractValue());
  }

 private:
  MessengerStorage& storage_;
  auth::MessengerAuth& auth_;
};

class P2PSendHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-p2p-send";
  P2PSendHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto user_id = auth_.RequireUserId(request);
    const auto body = userver::formats::json::FromString(request.RequestBody());
    const auto recipient = RequireField(body, "recipient_id");
    if (recipient == user_id) throw ClientError(Msg("Cannot message yourself"));
    auto msg = storage_.AddP2PMessage(user_id, recipient, RequireField(body, "content"));
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(P2PMessageToJson(msg));
  }

 private:
  MessengerStorage& storage_;
  auth::MessengerAuth& auth_;
};

class P2PListHandler final : public HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-p2p-list";
  P2PListHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& context)
      : HttpHandlerBase(config, context),
        storage_(context.FindComponent<MessengerStorage>()),
        auth_(context.FindComponent<auth::MessengerAuth>()) {}

  std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
    const auto user_id = auth_.RequireUserId(request);
    const auto peer_arg = request.GetArg("peer_id");
    std::optional<std::string> peer =
        peer_arg.empty() ? std::nullopt : std::optional(peer_arg);
    if (peer && *peer == user_id) throw ClientError(Msg("Invalid peer_id"));
    if (peer && !storage_.GetUser(*peer)) throw ResourceNotFound(Msg("Peer not found"));
    const auto messages =
        storage_.ListP2PMessages(user_id, peer, ParseLimit(request, 100, 500), ParseOffset(request));
    userver::formats::json::ValueBuilder arr{userver::formats::json::Type::kArray};
    for (const auto& m : messages) {
      arr.PushBack(P2PMessageToJson(m));
    }
    request.GetHttpResponse().SetContentType("application/json");
    return JsonResponse(arr.ExtractValue());
  }

 private:
  MessengerStorage& storage_;
  auth::MessengerAuth& auth_;
};

}  // namespace

void AppendMessengerHandlers(userver::components::ComponentList& component_list) {
  component_list.Append<storage::MessengerStorage>()
      .Append<auth::MessengerAuth>()
      .Append<HealthHandler>()
      .Append<AuthRegisterHandlerV2>()
      .Append<AuthLoginHandler>()
      .Append<UsersCreateHandler>()
      .Append<UsersByLoginHandler>()
      .Append<UsersSearchHandler>()
      .Append<GroupChatsCreateHandler>()
      .Append<GroupChatsAddMemberHandler>()
      .Append<GroupChatsPostMessageHandler>()
      .Append<GroupChatsGetMessagesHandler>()
      .Append<P2PSendHandler>()
      .Append<P2PListHandler>();
}

}  // namespace messenger::handlers

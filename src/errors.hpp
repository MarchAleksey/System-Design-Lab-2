#pragma once

#include <string>
#include <string_view>

#include <userver/server/handlers/exceptions.hpp>

namespace messenger::errors
{

    using userver::server::handlers::ClientError;
    using userver::server::handlers::ConflictError;
    using userver::server::handlers::InternalMessage;
    using userver::server::handlers::ResourceNotFound;
    using userver::server::handlers::Unauthorized;

    using Forbidden = userver::server::handlers::ExceptionWithCode<
        userver::server::handlers::HandlerErrorCode::kForbidden>;

    inline InternalMessage Msg(std::string_view text) { return InternalMessage{std::string{text}}; }

}

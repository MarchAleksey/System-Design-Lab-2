#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/testsuite/testsuite_support.hpp>  // components::TestsuiteSupport
#include <userver/utils/daemon_run.hpp>

#include "handlers/handlers.hpp"

int main(int argc, char* argv[]) {
  auto component_list =
      userver::components::MinimalServerComponentList()
          .Append<userver::server::handlers::Ping>()
          .Append<userver::clients::dns::Component>()
          .Append<userver::components::HttpClient>()
          .Append<userver::components::TestsuiteSupport>();

  messenger::handlers::AppendMessengerHandlers(component_list);

  return userver::utils::DaemonMain(argc, argv, component_list);
}

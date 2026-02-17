#include "dbgx/mcp/http_server.hpp"
#include "dbgx/mcp/io_echo.hpp"
#include "dbgx/mcp/json_rpc.hpp"
#include "dbgx/windbg/dbgeng_command_executor.hpp"

#include <DbgEng.h>
#include <windows.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace {

constexpr std::uint16_t kDefaultPort = 5678;

struct ExtensionState {
  std::mutex mutex;
  std::unique_ptr<dbgx::windbg::DbgEngCommandExecutor> executor;
  std::unique_ptr<dbgx::mcp::JsonRpcRouter> router;
  std::unique_ptr<dbgx::mcp::HttpServer> server;
};

ExtensionState& State() {
  static ExtensionState state;
  return state;
}

void LogMessage(const std::string& message) {
  const std::string text = "[windbg-mcp] " + message;

  IDebugClient* debug_client = nullptr;
  if (SUCCEEDED(DebugCreate(__uuidof(IDebugClient), reinterpret_cast<void**>(&debug_client))) &&
      debug_client != nullptr) {
    IDebugControl* debug_control = nullptr;
    if (SUCCEEDED(debug_client->QueryInterface(__uuidof(IDebugControl), reinterpret_cast<void**>(&debug_control))) &&
        debug_control != nullptr) {
      std::string line = text + "\n";
      debug_control->Output(DEBUG_OUTPUT_NORMAL, "%s", line.c_str());
      debug_control->Release();
      debug_client->Release();
      return;
    }

    debug_client->Release();
  }

  std::string fallback_line = text + "\n";
  OutputDebugStringA(fallback_line.c_str());
}

void LogRequestEcho(const dbgx::mcp::HttpRequest& request) noexcept {
  try {
    LogMessage(dbgx::mcp::BuildRequestIoSummary(request));
  } catch (...) {
    LogMessage("mcp.request echo unavailable");
  }
}

void LogResponseEcho(const dbgx::mcp::HttpResponse& response) noexcept {
  try {
    LogMessage(dbgx::mcp::BuildResponseIoSummary(response));
  } catch (...) {
    LogMessage("mcp.response echo unavailable");
  }
}

dbgx::mcp::HttpResponse FinishMcpRequest(dbgx::mcp::HttpResponse response) {
  LogResponseEcho(response);
  return response;
}

dbgx::mcp::HttpResponse HandleRequest(const dbgx::mcp::HttpRequest& request) {
  dbgx::mcp::HttpResponse response;

  if (request.path != "/mcp") {
    response.status_code = 404;
    response.body = "{\"error\":\"Not Found\"}";
    return response;
  }

  LogRequestEcho(request);

  const auto origin_it = request.headers.find("origin");
  if (origin_it != request.headers.end() && !dbgx::mcp::IsOriginAllowed(origin_it->second)) {
    response.status_code = 403;
    response.body =
        "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32000,\"message\":\"Forbidden origin\"}}";
    return FinishMcpRequest(std::move(response));
  }

  const auto protocol_header_it = request.headers.find("mcp-protocol-version");
  if (protocol_header_it != request.headers.end()) {
    const std::string& protocol = protocol_header_it->second;
    if (protocol != "2025-11-25" && protocol != "2025-03-26") {
      response.status_code = 400;
      response.body =
          "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"Unsupported MCP protocol version\"}}";
      return FinishMcpRequest(std::move(response));
    }
  }

  if (request.method == "GET") {
    response.status_code = 405;
    response.body = "{\"error\":\"GET stream is not implemented in this MVP\"}";
    return FinishMcpRequest(std::move(response));
  }

  if (request.method != "POST") {
    response.status_code = 405;
    response.body = "{\"error\":\"Method Not Allowed\"}";
    return FinishMcpRequest(std::move(response));
  }

  ExtensionState& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.router == nullptr) {
    response.status_code = 500;
    response.body = "{\"error\":\"Router is not initialized\"}";
    return FinishMcpRequest(std::move(response));
  }

  const dbgx::mcp::JsonRpcHttpResult rpc_result = state.router->HandleJsonRpcPost(request.body);
  response.status_code = rpc_result.status_code;
  response.content_type = rpc_result.content_type;
  response.has_body = rpc_result.has_body;
  response.body = rpc_result.body;
  return FinishMcpRequest(std::move(response));
}

void Cleanup() {
  ExtensionState& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);

  if (state.server != nullptr) {
    state.server->Stop();
    state.server.reset();
  }

  state.router.reset();
  state.executor.reset();
}

}  // namespace

extern "C" HRESULT CALLBACK DebugExtensionInitialize(PULONG version, PULONG flags) {
  if (version != nullptr) {
    *version = DEBUG_EXTENSION_VERSION(1, 0);
  }
  if (flags != nullptr) {
    *flags = 0;
  }

  ExtensionState& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);

  if (state.server != nullptr && state.server->IsRunning()) {
    return S_OK;
  }

  state.executor = std::make_unique<dbgx::windbg::DbgEngCommandExecutor>();
  state.router = std::make_unique<dbgx::mcp::JsonRpcRouter>(state.executor.get());
  state.server = std::make_unique<dbgx::mcp::HttpServer>();

  std::string error_message;
  if (!state.server->Start("127.0.0.1", kDefaultPort, HandleRequest, &error_message)) {
    LogMessage("Failed to start HTTP server: " + error_message);
    state.server.reset();
    state.router.reset();
    state.executor.reset();
    return E_FAIL;
  }

  LogMessage("HTTP MCP server listening on http://127.0.0.1:" + std::to_string(state.server->BoundPort()) +
             "/mcp");
  return S_OK;
}

extern "C" HRESULT CALLBACK DebugExtensionCanUnload(void) {
  return S_OK;
}

extern "C" void CALLBACK DebugExtensionUninitialize(void) {
  Cleanup();
}

extern "C" void CALLBACK DebugExtensionUnload(void) {
  Cleanup();
}

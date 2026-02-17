#include "dbgx/mcp/json_rpc.hpp"
#include "dbgx/mcp/io_echo.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

class FakeExecutor final : public dbgx::windbg::IWinDbgCommandExecutor {
 public:
  dbgx::windbg::CommandExecutionResult Execute(const std::string& command) override {
    ++call_count;
    last_command = command;
    if (should_fail) {
      return {
          .success = false,
          .output = "",
          .error_message = failure_message,
      };
    }
    return {
        .success = true,
        .output = output,
        .error_message = "",
    };
  }

  bool should_fail = false;
  std::string failure_message = "failed";
  std::string output = "ok";
  std::string last_command;
  int call_count = 0;
};

bool Contains(const std::string& text, const std::string& expected_substring) {
  return text.find(expected_substring) != std::string::npos;
}

void Expect(bool condition, const std::string& message, int* failures) {
  if (!condition) {
    std::cerr << "[FAIL] " << message << '\n';
    ++(*failures);
  }
}

void TestInitialize(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result = router.HandleJsonRpcPost(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25"}})");

  Expect(result.status_code == 200, "initialize should return HTTP 200", failures);
  Expect(Contains(result.body, "\"protocolVersion\":\"2025-11-25\""), "initialize should return protocol version", failures);
  Expect(Contains(result.body, "\"windbg.eval\""), "initialize should mention windbg.eval capability", failures);
}

void TestToolsList(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result =
      router.HandleJsonRpcPost(R"({"jsonrpc":"2.0","id":"abc","method":"tools/list","params":{}})");

  Expect(result.status_code == 200, "tools/list should return HTTP 200", failures);
  Expect(Contains(result.body, "\"name\":\"windbg.eval\""), "tools/list should include windbg.eval", failures);
}

void TestToolsCallSuccess(int* failures) {
  FakeExecutor executor;
  executor.output = "eax=0x42";

  dbgx::mcp::JsonRpcRouter router(&executor);
  const dbgx::mcp::JsonRpcHttpResult result = router.HandleJsonRpcPost(
      R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"windbg.eval","arguments":{"command":"r eax"}}})");

  Expect(result.status_code == 200, "tools/call should return HTTP 200", failures);
  Expect(executor.call_count == 1, "tools/call should execute one command", failures);
  Expect(executor.last_command == "r eax", "tools/call should forward command text", failures);
  Expect(Contains(result.body, "eax=0x42"), "tools/call should return executor output", failures);
}

void TestToolsCallMissingCommand(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result = router.HandleJsonRpcPost(
      R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"windbg.eval","arguments":{}}})");

  Expect(result.status_code == 200, "missing command should still return JSON-RPC response", failures);
  Expect(Contains(result.body, "\"code\":-32602"), "missing command should return invalid params", failures);
  Expect(executor.call_count == 0, "missing command must not execute command", failures);
}

void TestUnknownMethod(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result =
      router.HandleJsonRpcPost(R"({"jsonrpc":"2.0","id":4,"method":"unknown/method","params":{}})");

  Expect(result.status_code == 200, "unknown method should return HTTP 200 with JSON-RPC error", failures);
  Expect(Contains(result.body, "\"code\":-32601"), "unknown method should return method not found", failures);
}

void TestInitializedNotification(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result =
      router.HandleJsonRpcPost(R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})");

  Expect(result.status_code == 202, "initialized notification should return HTTP 202", failures);
  Expect(!result.has_body, "initialized notification should return empty body", failures);
  Expect(result.body.empty(), "initialized notification payload should be empty", failures);
}

void TestParseError(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result = router.HandleJsonRpcPost("this-is-not-json");

  Expect(result.status_code == 400, "invalid JSON should return HTTP 400", failures);
  Expect(Contains(result.body, "\"code\":-32700"), "invalid JSON should return parse error", failures);
}

void TestIoEchoRequestSummaryMasksSensitiveHeader(int* failures) {
  dbgx::mcp::HttpRequest request;
  request.method = "POST";
  request.path = "/mcp";
  request.headers.insert_or_assign("authorization", "Bearer super-secret-token");
  request.body =
      R"({"jsonrpc":"2.0","id":99,"method":"tools/call","params":{"name":"windbg.eval","arguments":{"command":"r eax"}}})";

  const std::string summary = dbgx::mcp::BuildRequestIoSummary(request);

  Expect(Contains(summary, "method=POST"), "request summary should include HTTP method", failures);
  Expect(Contains(summary, "path=/mcp"), "request summary should include request path", failures);
  Expect(Contains(summary, "rpc_method=tools/call"), "request summary should include JSON-RPC method", failures);
  Expect(Contains(summary, "rpc_id=99"), "request summary should include JSON-RPC id", failures);
  Expect(Contains(summary, "authorization=<masked>"), "request summary should mask authorization header", failures);
  Expect(!Contains(summary, "super-secret-token"), "request summary must not expose authorization token", failures);
}

void TestIoEchoSummaryTruncatesLongPayload(int* failures) {
  const std::string long_text(512, 'x');

  dbgx::mcp::HttpResponse response;
  response.status_code = 200;
  response.body =
      "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + long_text +
      "\"}]}}";

  const std::string summary = dbgx::mcp::BuildResponseIoSummary(response);
  Expect(Contains(summary, "...(truncated)"), "long response summary should be truncated", failures);
}

void TestIoEchoRequestSummaryIncludesTraceContext(int* failures) {
  dbgx::mcp::HttpRequest request;
  request.method = "POST";
  request.path = "/mcp";
  request.body =
      R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"windbg.eval","arguments":{"command":"g"}}})";

  dbgx::mcp::IoTraceContext trace_context;
  trace_context.trace_id = "rpc:7";
  trace_context.stage = "tool_execute_start";
  trace_context.rpc_method = "tools/call";
  trace_context.rpc_id = "7";
  trace_context.tool_name = "windbg.eval";
  trace_context.duration_ms = 12;

  const std::string summary = dbgx::mcp::BuildRequestIoSummary(request, trace_context);
  Expect(Contains(summary, "trace_id=rpc:7"), "request summary should include trace id", failures);
  Expect(Contains(summary, "stage=tool_execute_start"), "request summary should include stage", failures);
  Expect(Contains(summary, "duration_ms=12"), "request summary should include duration", failures);
  Expect(Contains(summary, "tool=windbg.eval"), "request summary should include tool name", failures);
}

void TestIoEchoParseRequestMetaMissingId(int* failures) {
  dbgx::mcp::HttpRequest request;
  request.method = "POST";
  request.path = "/mcp";
  request.body =
      R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"windbg.eval","arguments":{"command":"g"}}})";

  const dbgx::mcp::RequestIoMeta meta = dbgx::mcp::ParseRequestIoMeta(request);
  Expect(meta.parseable, "request meta should parse valid JSON", failures);
  Expect(meta.has_rpc_method && meta.rpc_method == "tools/call", "request meta should include tools/call method", failures);
  Expect(!meta.has_rpc_id, "request meta should detect missing id", failures);
  Expect(meta.has_tool_name && meta.tool_name == "windbg.eval", "request meta should include tool name", failures);
}

void TestIoEchoLocalTraceIdConsistencyAcrossStages(int* failures) {
  dbgx::mcp::HttpRequest request;
  request.method = "POST";
  request.path = "/mcp";
  request.body =
      R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"windbg.eval","arguments":{"command":"g"}}})";

  dbgx::mcp::IoTraceContext request_context;
  request_context.trace_id = "local-42";
  request_context.stage = "request_received";
  request_context.rpc_method = "tools/call";
  request_context.tool_name = "windbg.eval";
  request_context.duration_ms = 0;

  const std::string request_summary = dbgx::mcp::BuildRequestIoSummary(request, request_context);
  Expect(Contains(request_summary, "trace_id=local-42"), "request summary should include local trace id", failures);

  dbgx::mcp::HttpResponse response;
  response.status_code = 200;
  response.body =
      R"({"jsonrpc":"2.0","result":{"content":[{"type":"text","text":"ok"}],"isError":false}})";

  dbgx::mcp::IoTraceContext response_context = request_context;
  response_context.stage = "response_sent";
  response_context.duration_ms = 25;

  const std::string response_summary = dbgx::mcp::BuildResponseIoSummary(response, response_context);
  Expect(Contains(response_summary, "trace_id=local-42"), "response summary should include same local trace id", failures);
  Expect(Contains(response_summary, "stage=response_sent"), "response summary should include response stage", failures);
}

void TestIoEchoResponseSummaryCoversSuccessAndError(int* failures) {
  dbgx::mcp::HttpResponse success_response;
  success_response.status_code = 200;
  success_response.body =
      "{\"jsonrpc\":\"2.0\",\"id\":10,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"ok\"}]}}";

  const std::string success_summary = dbgx::mcp::BuildResponseIoSummary(success_response);
  Expect(Contains(success_summary, "rpc_outcome=success"), "success response should be marked as success", failures);

  dbgx::mcp::HttpResponse error_response;
  error_response.status_code = 200;
  error_response.body =
      "{\"jsonrpc\":\"2.0\",\"id\":10,\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"}}";

  const std::string error_summary = dbgx::mcp::BuildResponseIoSummary(error_response);
  Expect(Contains(error_summary, "rpc_outcome=error"), "error response should be marked as error", failures);
  Expect(Contains(error_summary, "-32600"), "error response summary should include JSON-RPC error code", failures);
}

void TestIoEchoResponseSummaryTreatsToolIsErrorAsError(int* failures) {
  dbgx::mcp::HttpResponse response;
  response.status_code = 200;
  response.body =
      R"({"jsonrpc":"2.0","id":5,"result":{"content":[{"type":"text","text":"failed"}],"isError":true}})";

  const std::string summary = dbgx::mcp::BuildResponseIoSummary(response);
  Expect(Contains(summary, "rpc_outcome=error"), "result.isError=true should be treated as error outcome", failures);
}

void TestIoEchoBlockingLocatabilityStageOrder(int* failures) {
  dbgx::mcp::IoTraceContext execute_start_context;
  execute_start_context.trace_id = "rpc:5";
  execute_start_context.stage = "tool_execute_start";
  execute_start_context.rpc_method = "tools/call";
  execute_start_context.rpc_id = "5";
  execute_start_context.tool_name = "windbg.eval";
  execute_start_context.outcome = "in_progress";
  execute_start_context.duration_ms = 0;

  dbgx::mcp::IoTraceContext response_context = execute_start_context;
  response_context.stage = "response_sent";
  response_context.outcome.clear();
  response_context.duration_ms = 40;

  dbgx::mcp::HttpResponse response;
  response.status_code = 200;
  response.body =
      R"({"jsonrpc":"2.0","id":5,"result":{"content":[{"type":"text","text":"ok"}],"isError":false}})";

  std::vector<std::string> logs;
  logs.push_back(dbgx::mcp::BuildLifecycleIoSummary(execute_start_context, "entering tool executor"));
  logs.push_back(dbgx::mcp::BuildResponseIoSummary(response, response_context));

  Expect(Contains(logs[0], "stage=tool_execute_start"), "first log should indicate tool execution start", failures);
  Expect(Contains(logs[1], "stage=response_sent"), "second log should indicate response stage", failures);
  Expect(Contains(logs[0], "trace_id=rpc:5"), "first log should include request trace id", failures);
  Expect(Contains(logs[1], "trace_id=rpc:5"), "second log should include same request trace id", failures);
}

}  // namespace

int main() {
  int failures = 0;

  TestInitialize(&failures);
  TestToolsList(&failures);
  TestToolsCallSuccess(&failures);
  TestToolsCallMissingCommand(&failures);
  TestUnknownMethod(&failures);
  TestInitializedNotification(&failures);
  TestParseError(&failures);
  TestIoEchoRequestSummaryMasksSensitiveHeader(&failures);
  TestIoEchoSummaryTruncatesLongPayload(&failures);
  TestIoEchoRequestSummaryIncludesTraceContext(&failures);
  TestIoEchoParseRequestMetaMissingId(&failures);
  TestIoEchoLocalTraceIdConsistencyAcrossStages(&failures);
  TestIoEchoResponseSummaryCoversSuccessAndError(&failures);
  TestIoEchoResponseSummaryTreatsToolIsErrorAsError(&failures);
  TestIoEchoBlockingLocatabilityStageOrder(&failures);

  if (failures == 0) {
    std::cout << "All unit tests passed.\n";
    return 0;
  }

  std::cerr << failures << " test(s) failed.\n";
  return 1;
}

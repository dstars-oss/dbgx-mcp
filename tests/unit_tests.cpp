#include "dbgx/mcp/json_rpc.hpp"

#include <iostream>
#include <string>

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

void TestParseError(int* failures) {
  FakeExecutor executor;
  dbgx::mcp::JsonRpcRouter router(&executor);

  const dbgx::mcp::JsonRpcHttpResult result = router.HandleJsonRpcPost("this-is-not-json");

  Expect(result.status_code == 400, "invalid JSON should return HTTP 400", failures);
  Expect(Contains(result.body, "\"code\":-32700"), "invalid JSON should return parse error", failures);
}

}  // namespace

int main() {
  int failures = 0;

  TestInitialize(&failures);
  TestToolsList(&failures);
  TestToolsCallSuccess(&failures);
  TestToolsCallMissingCommand(&failures);
  TestUnknownMethod(&failures);
  TestParseError(&failures);

  if (failures == 0) {
    std::cout << "All unit tests passed.\n";
    return 0;
  }

  std::cerr << failures << " test(s) failed.\n";
  return 1;
}

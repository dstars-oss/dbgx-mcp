#pragma once

#include <string>

namespace dbgx::windbg {

struct CommandExecutionResult {
  bool success = false;
  std::string output;
  std::string error_message;
};

class IWinDbgCommandExecutor {
 public:
  virtual ~IWinDbgCommandExecutor() = default;
  virtual CommandExecutionResult Execute(const std::string& command) = 0;
};

}  // namespace dbgx::windbg

#pragma once

#include <string>

#include "dbgx/mcp/http_server.hpp"

namespace dbgx::mcp {

std::string BuildRequestIoSummary(const HttpRequest& request);
std::string BuildResponseIoSummary(const HttpResponse& response);

}  // namespace dbgx::mcp

#include "dbgx/mcp/io_echo.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

#include "dbgx/mcp/json.hpp"

namespace dbgx::mcp {

namespace {

constexpr std::size_t kSummaryValueLimit = 160;
constexpr std::string_view kTruncatedSuffix = "...(truncated)";

std::string ToLowerAscii(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (const unsigned char ch : value) {
    lowered.push_back(static_cast<char>(std::tolower(ch)));
  }
  return lowered;
}

bool IsSensitiveHeaderName(std::string_view header_name) {
  const std::string lowered = ToLowerAscii(header_name);
  return lowered == "authorization" || lowered == "proxy-authorization" || lowered == "x-api-key";
}

std::string SanitizeSingleLine(std::string_view value) {
  std::string sanitized;
  sanitized.reserve(value.size());

  for (const char ch : value) {
    if (ch == '\r' || ch == '\n' || ch == '\t') {
      sanitized.push_back(' ');
      continue;
    }
    sanitized.push_back(ch);
  }

  return sanitized;
}

std::string TruncateForSummary(std::string_view value) {
  const std::string sanitized = SanitizeSingleLine(value);
  if (sanitized.size() <= kSummaryValueLimit) {
    return sanitized;
  }

  std::string truncated = sanitized.substr(0, kSummaryValueLimit);
  truncated += kTruncatedSuffix;
  return truncated;
}

std::string JoinValues(const std::vector<std::string>& values) {
  if (values.empty()) {
    return {};
  }

  std::string joined = values.front();
  for (std::size_t i = 1; i < values.size(); ++i) {
    joined += ",";
    joined += values[i];
  }
  return joined;
}

std::string BuildSensitiveHeadersSummary(const HttpRequest& request) {
  std::vector<std::string> masked_headers;
  masked_headers.reserve(request.headers.size());

  for (const auto& [header_name, header_value] : request.headers) {
    if (!IsSensitiveHeaderName(header_name)) {
      continue;
    }

    const std::string lower_name = ToLowerAscii(header_name);
    if (header_value.empty()) {
      masked_headers.push_back(lower_name + "=<masked>");
      continue;
    }

    masked_headers.push_back(lower_name + "=<masked>");
  }

  std::sort(masked_headers.begin(), masked_headers.end());

  if (masked_headers.empty()) {
    return "sensitive_headers=none";
  }

  return "sensitive_headers=" + JoinValues(masked_headers);
}

void AppendRpcRequestMeta(const json::FieldMap& fields, std::string* summary) {
  std::string method;
  if (json::TryGetStringField(fields, "method", &method)) {
    *summary += " rpc_method=" + TruncateForSummary(method);
  } else {
    *summary += " rpc_method=(missing)";
  }

  std::string id_raw;
  if (json::TryGetRawField(fields, "id", &id_raw)) {
    *summary += " rpc_id=" + TruncateForSummary(id_raw);
  } else {
    *summary += " rpc_id=(missing)";
  }
}

void AppendRpcResponseMeta(const json::FieldMap& fields, std::string* summary) {
  std::string id_raw;
  if (json::TryGetRawField(fields, "id", &id_raw)) {
    *summary += " rpc_id=" + TruncateForSummary(id_raw);
  }

  std::string error_raw;
  if (json::TryGetRawField(fields, "error", &error_raw)) {
    *summary += " rpc_outcome=error";
    *summary += " error=" + TruncateForSummary(error_raw);
    return;
  }

  std::string result_raw;
  if (json::TryGetRawField(fields, "result", &result_raw)) {
    *summary += " rpc_outcome=success";
    *summary += " result=" + TruncateForSummary(result_raw);
    return;
  }

  *summary += " rpc_outcome=unknown";
}

}  // namespace

std::string BuildRequestIoSummary(const HttpRequest& request) {
  std::string summary = "mcp.request method=" + TruncateForSummary(request.method);
  summary += " path=" + TruncateForSummary(request.path);

  json::FieldMap root_fields;
  std::string parse_error;
  if (json::ParseObjectFields(request.body, &root_fields, &parse_error)) {
    AppendRpcRequestMeta(root_fields, &summary);
  } else {
    summary += " rpc_meta=unparseable";
  }

  summary += " body_bytes=" + std::to_string(request.body.size());
  summary += " " + BuildSensitiveHeadersSummary(request);
  return summary;
}

std::string BuildResponseIoSummary(const HttpResponse& response) {
  std::string summary = "mcp.response status=" + std::to_string(response.status_code);
  summary += response.has_body ? " has_body=true" : " has_body=false";

  if (!response.has_body) {
    return summary;
  }

  json::FieldMap root_fields;
  std::string parse_error;
  if (json::ParseObjectFields(response.body, &root_fields, &parse_error)) {
    AppendRpcResponseMeta(root_fields, &summary);
    return summary;
  }

  summary += " rpc_meta=unparseable";
  summary += " body=" + TruncateForSummary(response.body.empty() ? "(empty)" : response.body);
  return summary;
}

}  // namespace dbgx::mcp

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dbgx::mcp {

struct HttpRequest {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

struct HttpResponse {
  int status_code = 200;
  std::string content_type = "application/json; charset=utf-8";
  std::string body;
  bool has_body = true;
};

using HttpRequestHandler = std::function<HttpResponse(const HttpRequest& request)>;

bool IsOriginAllowed(std::string_view origin_header);

class HttpServer {
 public:
  HttpServer();
  ~HttpServer();

  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  bool Start(
      const std::string& host,
      std::uint16_t port,
      HttpRequestHandler handler,
      std::string* error_message);
  void Stop();

  bool IsRunning() const;
  std::uint16_t BoundPort() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace dbgx::mcp

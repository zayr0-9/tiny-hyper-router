#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tiny_hyper_router {

struct HttpRequest {
  std::string method = "GET";
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};

struct HttpResponse {
  long status_code = 0;
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;
};

class HttpError : public std::runtime_error {
 public:
  HttpError(std::string message, long status_code = 0)
      : std::runtime_error(std::move(message)), status_code_(status_code) {}

  [[nodiscard]] long status_code() const noexcept {
    return status_code_;
  }

 private:
  long status_code_;
};

class HttpClient {
 public:
  virtual ~HttpClient() = default;

  virtual HttpResponse send(const HttpRequest& request) = 0;
};

class NullHttpClient final : public HttpClient {
 public:
  HttpResponse send(const HttpRequest& request) override;
};

}  // namespace tiny_hyper_router

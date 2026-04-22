#pragma once

#include "tiny_hyper_router/http/client.hpp"

namespace tiny_hyper_router {

class CurlHttpClient final : public HttpClient {
 public:
  HttpResponse send(const HttpRequest& request) override;
};

}  // namespace tiny_hyper_router

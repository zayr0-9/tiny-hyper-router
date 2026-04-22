#include "tiny_hyper_router/http/client.hpp"

namespace tiny_hyper_router {

HttpResponse NullHttpClient::send(const HttpRequest& request) {
  (void)request;
  throw HttpError("HTTP client is not implemented yet. Provide a concrete HttpClient.");
}

}  // namespace tiny_hyper_router

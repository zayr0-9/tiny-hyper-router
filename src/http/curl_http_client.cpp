#include "tiny_hyper_router/http/curl_http_client.hpp"

#include <curl/curl.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tiny_hyper_router {

namespace {

struct CurlGlobalInit {
  CurlGlobalInit() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }

  ~CurlGlobalInit() {
    curl_global_cleanup();
  }
};

CurlGlobalInit& curl_global_init_once() {
  static CurlGlobalInit instance;
  return instance;
}

struct CurlHandleDeleter {
  void operator()(CURL* handle) const noexcept {
    if (handle != nullptr) {
      curl_easy_cleanup(handle);
    }
  }
};

struct CurlSlistDeleter {
  void operator()(curl_slist* list) const noexcept {
    if (list != nullptr) {
      curl_slist_free_all(list);
    }
  }
};

using CurlHandle = std::unique_ptr<CURL, CurlHandleDeleter>;
using CurlHeaderList = std::unique_ptr<curl_slist, CurlSlistDeleter>;

std::size_t write_body_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  const auto total_size = size * nmemb;
  auto* body = static_cast<std::string*>(userdata);
  body->append(ptr, total_size);
  return total_size;
}

std::size_t write_header_callback(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
  const auto total_size = size * nitems;
  auto* headers = static_cast<std::vector<std::pair<std::string, std::string>>*>(userdata);

  std::string line(buffer, total_size);
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.pop_back();
  }

  if (line.empty()) {
    return total_size;
  }

  const auto delimiter = line.find(':');
  if (delimiter == std::string::npos) {
    return total_size;
  }

  std::string key = line.substr(0, delimiter);
  std::string value = line.substr(delimiter + 1);

  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
    value.erase(value.begin());
  }

  headers->emplace_back(std::move(key), std::move(value));
  return total_size;
}

void set_common_options(CURL* curl,
                        const HttpRequest& request,
                        std::string& response_body,
                        std::vector<std::pair<std::string, std::string>>& response_headers) {
  curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_body_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &write_header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
}

void set_method_and_body(CURL* curl, const HttpRequest& request) {
  if (request.method == "GET") {
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    return;
  }

  if (request.method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
  }

  if (!request.body.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
  }
}

CurlHeaderList build_headers(const HttpRequest& request) {
  curl_slist* raw_headers = nullptr;

  for (const auto& [key, value] : request.headers) {
    const auto header_line = key + ": " + value;
    raw_headers = curl_slist_append(raw_headers, header_line.c_str());
    if (raw_headers == nullptr) {
      throw HttpError("Failed to allocate curl header list.");
    }
  }

  return CurlHeaderList(raw_headers);
}

}  // namespace

HttpResponse CurlHttpClient::send(const HttpRequest& request) {
  (void)curl_global_init_once();

  CurlHandle curl(curl_easy_init());
  if (!curl) {
    throw HttpError("Failed to initialize libcurl.");
  }

  std::string response_body;
  std::vector<std::pair<std::string, std::string>> response_headers;

  set_common_options(curl.get(), request, response_body, response_headers);
  set_method_and_body(curl.get(), request);

  auto header_list = build_headers(request);
  if (header_list) {
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());
  }

  const auto result = curl_easy_perform(curl.get());
  if (result != CURLE_OK) {
    throw HttpError(std::string{"libcurl request failed: "} + curl_easy_strerror(result));
  }

  long status_code = 0;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code);

  return HttpResponse{
      .status_code = status_code,
      .body = std::move(response_body),
      .headers = std::move(response_headers),
  };
}

}  // namespace tiny_hyper_router

#include "tiny_hyper_router/embed/c_api.h"

#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/embed/client.hpp"

namespace {

using tiny_hyper_router::embed::Client;
using tiny_hyper_router::embed::ClientConfig;
using tiny_hyper_router::embed::SendMessageRequest;

const char* allocate_c_string(const std::string& value) {
  auto* buffer = new char[value.size() + 1];
  std::memcpy(buffer, value.c_str(), value.size() + 1);
  return buffer;
}

std::string error_json(const std::string& message) {
  return nlohmann::json{
      {"ok", false},
      {"error", message},
  }.dump();
}

std::string stringify_exception() {
  try {
    throw;
  } catch (const std::exception& exception) {
    return error_json(exception.what());
  } catch (...) {
    return error_json("Unknown native exception.");
  }
}

}  // namespace

extern "C" {

thr_client_handle thr_create_client_from_json(const char* config_json) {
  try {
    if (config_json == nullptr) {
      return nullptr;
    }

    const auto parsed = nlohmann::json::parse(config_json);
    auto client = std::make_unique<Client>(ClientConfig::from_json(parsed));
    return reinterpret_cast<thr_client_handle>(client.release());
  } catch (...) {
    return nullptr;
  }
}

void thr_destroy_client(thr_client_handle handle) {
  auto* client = reinterpret_cast<Client*>(handle);
  delete client;
}

const char* thr_send_message_json(thr_client_handle handle, const char* request_json) {
  try {
    if (handle == nullptr) {
      return allocate_c_string(error_json("Client handle is null."));
    }
    if (request_json == nullptr) {
      return allocate_c_string(error_json("Request JSON is null."));
    }

    auto* client = reinterpret_cast<Client*>(handle);
    const auto parsed = nlohmann::json::parse(request_json);
    const auto response = client->send_message_json(SendMessageRequest::from_json(parsed));

    return allocate_c_string(response.dump());
  } catch (...) {
    return allocate_c_string(stringify_exception());
  }
}

const char* thr_get_session_json(thr_client_handle handle, const char* session_id) {
  try {
    if (handle == nullptr) {
      return allocate_c_string(error_json("Client handle is null."));
    }
    if (session_id == nullptr) {
      return allocate_c_string(error_json("Session id is null."));
    }

    auto* client = reinterpret_cast<Client*>(handle);
    const auto response = client->get_session_json(session_id);
    return allocate_c_string(response.dump());
  } catch (...) {
    return allocate_c_string(stringify_exception());
  }
}

void thr_free_string(const char* value) {
  delete[] value;
}

}  // extern "C"

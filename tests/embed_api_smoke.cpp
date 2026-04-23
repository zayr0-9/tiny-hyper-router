#include <cstdlib>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/embed/index.hpp"

int main() {
  using namespace tiny_hyper_router::embed;

  Client client(ClientConfig{
      .agent_name = "embed-test-agent",
      .instructions = "You are helpful.",
      .model = "stub-model",
      .default_max_steps = 4,
      .use_stub_provider = true,
      .use_in_memory_storage = true,
  });

  const auto response = client.send_message_json(SendMessageRequest{
      .session_id = "embed-session",
      .input = "hello embed",
  });

  if (response.value("status", std::string{}) != "completed") {
    std::cerr << "Expected completed status.\n";
    return EXIT_FAILURE;
  }

  if (response.value("assistant_text", std::string{}).find("hello embed") == std::string::npos) {
    std::cerr << "Expected assistant_text to contain echoed input.\n";
    return EXIT_FAILURE;
  }

  const auto session = client.get_session_json("embed-session");
  if (!session.contains("messages") || !session.at("messages").is_array() || session.at("messages").size() < 2) {
    std::cerr << "Expected persisted session messages.\n";
    return EXIT_FAILURE;
  }

  const auto config_json = nlohmann::json{
      {"agent_name", "embed-c-api-agent"},
      {"instructions", "You are helpful."},
      {"model", "stub-model"},
      {"use_stub_provider", true},
      {"use_in_memory_storage", true},
  };

  const auto request_json = nlohmann::json{
      {"session_id", "c-api-session"},
      {"input", "ping from c api"},
  };

  auto handle = thr_create_client_from_json(config_json.dump().c_str());
  if (handle == nullptr) {
    std::cerr << "Expected non-null client handle.\n";
    return EXIT_FAILURE;
  }

  const char* response_text = thr_send_message_json(handle, request_json.dump().c_str());
  if (response_text == nullptr) {
    std::cerr << "Expected C API response string.\n";
    thr_destroy_client(handle);
    return EXIT_FAILURE;
  }

  const auto parsed_response = nlohmann::json::parse(response_text, nullptr, false);
  thr_free_string(response_text);

  if (parsed_response.is_discarded() || parsed_response.value("status", std::string{}) != "completed") {
    std::cerr << "Expected valid completed C API response JSON.\n";
    thr_destroy_client(handle);
    return EXIT_FAILURE;
  }

  const char* session_text = thr_get_session_json(handle, "c-api-session");
  if (session_text == nullptr) {
    std::cerr << "Expected C API session string.\n";
    thr_destroy_client(handle);
    return EXIT_FAILURE;
  }

  const auto parsed_session = nlohmann::json::parse(session_text, nullptr, false);
  thr_free_string(session_text);
  thr_destroy_client(handle);

  if (parsed_session.is_discarded() || !parsed_session.contains("messages") || parsed_session.at("messages").size() < 2) {
    std::cerr << "Expected persisted C API session transcript.\n";
    return EXIT_FAILURE;
  }

  std::cout << "embed_api_smoke passed\n";
  return EXIT_SUCCESS;
}

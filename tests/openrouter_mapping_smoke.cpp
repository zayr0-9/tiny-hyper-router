#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/providers/openrouter/index.hpp"
#include "tiny_hyper_router/storage/in_memory_storage.hpp"
#include "tiny_hyper_router/tiny_hyper_router.hpp"

namespace {

class FakeHttpClient final : public tiny_hyper_router::HttpClient {
 public:
  explicit FakeHttpClient(long status_code, std::string body)
      : status_code_(status_code), body_(std::move(body)) {}

  tiny_hyper_router::HttpResponse send(
      const tiny_hyper_router::HttpRequest& request) override {
    last_request_ = request;
    return tiny_hyper_router::HttpResponse{
        .status_code = status_code_,
        .body = body_,
    };
  }

  tiny_hyper_router::HttpRequest last_request_{};

 private:
  long status_code_;
  std::string body_;
};

bool contains_header(const tiny_hyper_router::HttpRequest& request,
                     const std::string& key,
                     const std::string& value) {
  for (const auto& [header_key, header_value] : request.headers) {
    if (header_key == key && header_value == value) {
      return true;
    }
  }

  return false;
}

}  // namespace

int main() {
  using namespace tiny_hyper_router;

  const std::vector<Message> messages = {
      Message{
          .role = Role::System,
          .content = "You are helpful.",
          .timestamp = "2026-01-01T00:00:00Z",
      },
      Message{
          .role = Role::User,
          .content = "Hello",
          .timestamp = "2026-01-01T00:00:01Z",
      },
  };

  const auto input_items = to_openrouter_input_items(messages);
  if (input_items.size() != 2) {
    std::cerr << "Expected 2 OpenRouter input items.\n";
    return EXIT_FAILURE;
  }

  ToolDefinition echo_tool{
      .name = "echo",
      .description = "Echo the input.",
      .input_schema = nlohmann::json{
          {"type", "object"},
          {"properties", {{"text", {{"type", "string"}}}}},
          {"required", nlohmann::json::array({"text"})},
      },
  };

  const auto success_body = nlohmann::json{
      {"output_text", "Hello back"},
      {"output", nlohmann::json::array({
          {
              {"type", "reasoning"},
              {"id", "rs_1"},
              {"encrypted_content", "encrypted-reasoning"},
              {"format", "openai-responses-v1"},
              {"status", "completed"},
              {"summary", nlohmann::json::array({
                  {
                      {"text", "First think"},
                      {"type", "summary_text"},
                  },
                  {
                      {"text", "Then answer"},
                      {"type", "summary_text"},
                  },
              })},
          },
          {
              {"type", "message"},
              {"role", "assistant"},
              {"content", nlohmann::json::array({
                  {
                      {"type", "output_text"},
                      {"text", "Hello back"},
                  },
              })},
          },
          {
              {"type", "function_call"},
              {"call_id", "call-1"},
              {"name", "echo"},
              {"arguments", "{\"text\":\"Hello\"}"},
          },
      })},
      {"usage", {
          {"output_tokens_details", {
              {"reasoning_tokens", 45},
          }},
      }},
  };

  auto fake_http_client = std::make_shared<FakeHttpClient>(200, success_body.dump());

  OpenRouterProvider provider(OpenRouterProviderOptions{
      .api_key = std::string{"secret-token"},
      .parallel_tool_calls = true,
      .store = false,
      .max_output_tokens = 256,
      .temperature = 0.7,
      .top_p = 0.9,
      .user = std::string{"demo-user"},
      .metadata = nlohmann::json{{"sdk", "tiny-hyper-router"}},
      .reasoning = OpenRouterReasoningOptions{
          .effort = OpenRouterReasoningEffort::High,
          .max_tokens = 128,
      },
      .http_client = fake_http_client,
  });

  const auto request_body = provider.build_request_body(GenerateRequest{
      .session_id = std::optional<std::string>{"demo-session"},
      .model = "openai/gpt-4o",
      .messages = messages,
      .tools = {echo_tool},
  });

  if (request_body.value("model", std::string{}) != "openai/gpt-4o") {
    std::cerr << "Expected model in request body.\n";
    return EXIT_FAILURE;
  }

  if (request_body.value("instructions", std::string{}) != "You are helpful.") {
    std::cerr << "Expected instructions in request body.\n";
    return EXIT_FAILURE;
  }

  if (!request_body.contains("tools") || !request_body.at("tools").is_array() || request_body.at("tools").empty()) {
    std::cerr << "Expected tool definitions in request body.\n";
    return EXIT_FAILURE;
  }

  if (request_body.value("tool_choice", std::string{}) != "auto") {
    std::cerr << "Expected tool_choice=auto.\n";
    return EXIT_FAILURE;
  }

  if (!request_body.value("parallel_tool_calls", false)) {
    std::cerr << "Expected parallel_tool_calls=true.\n";
    return EXIT_FAILURE;
  }

  if (request_body.value("session_id", std::string{}) != "demo-session") {
    std::cerr << "Expected session_id in request body.\n";
    return EXIT_FAILURE;
  }

  if (!request_body.contains("reasoning") || request_body.at("reasoning").value("effort", std::string{}) != "high") {
    std::cerr << "Expected reasoning config in request body.\n";
    return EXIT_FAILURE;
  }

  if (!request_body.contains("include") || !request_body.at("include").is_array() ||
      request_body.at("include").empty() || request_body.at("include").front() != "reasoning.encrypted_content") {
    std::cerr << "Expected include=[reasoning.encrypted_content] in request body.\n";
    return EXIT_FAILURE;
  }

  const auto parsed = provider.parse_response_body(success_body);

  const auto generated = provider.generate(GenerateRequest{
      .session_id = std::optional<std::string>{"demo-session"},
      .model = "openai/gpt-4o",
      .messages = messages,
      .tools = {echo_tool},
  });

  if (!parsed.message.has_value()) {
    std::cerr << "Expected parsed assistant message.\n";
    return EXIT_FAILURE;
  }

  if (parsed.message->content != "Hello back") {
    std::cerr << "Unexpected parsed text: " << parsed.message->content << "\n";
    return EXIT_FAILURE;
  }

  if (parsed.tool_calls.size() != 1 || parsed.tool_calls.front().tool_name != "echo") {
    std::cerr << "Expected parsed tool call.\n";
    return EXIT_FAILURE;
  }

  if (parsed.reasoning.size() != 1 || !parsed.reasoning_tokens.has_value() || *parsed.reasoning_tokens != 45) {
    std::cerr << "Expected parsed reasoning details and tokens.\n";
    return EXIT_FAILURE;
  }

  const auto& reasoning = parsed.reasoning.front();
  if (!reasoning.format.has_value() || *reasoning.format != "openai-responses-v1" ||
      !reasoning.status.has_value() || *reasoning.status != "completed" ||
      reasoning.summary.size() != 2 || !reasoning.summary.front().type.has_value()) {
    std::cerr << "Expected richer reasoning persistence fields.\n";
    return EXIT_FAILURE;
  }

  if (!generated.message.has_value() || generated.tool_calls.size() != 1) {
    std::cerr << "Expected generate() to use HTTP client and parse response.\n";
    return EXIT_FAILURE;
  }

  if (generated.message->reasoning.size() != 1 || !generated.message->reasoning.front().encrypted_content.has_value()) {
    std::cerr << "Expected assistant message to carry reasoning details for persistence.\n";
    return EXIT_FAILURE;
  }

  auto storage = std::make_shared<InMemoryStorage>();
  storage->save_messages("reasoning-session", {*generated.message});
  const auto persisted_messages = storage->load_messages("reasoning-session");
  if (persisted_messages.size() != 1 || persisted_messages.front().reasoning.size() != 1) {
    std::cerr << "Expected persisted message reasoning details.\n";
    return EXIT_FAILURE;
  }

  const auto& persisted_reasoning = persisted_messages.front().reasoning.front();
  if (!persisted_reasoning.id.has_value() || *persisted_reasoning.id != "rs_1" ||
      !persisted_reasoning.encrypted_content.has_value() ||
      !persisted_reasoning.format.has_value() ||
      !persisted_reasoning.status.has_value() ||
      !persisted_reasoning.raw_item.has_value() ||
      persisted_reasoning.summary.size() != 2) {
    std::cerr << "Expected durable reasoning persistence fields.\n";
    return EXIT_FAILURE;
  }

  const auto replay_input_items = to_openrouter_input_items(persisted_messages);
  bool replay_contains_reasoning = false;
  for (const auto& item : replay_input_items) {
    if (!item.is_object() || item.value("type", std::string{}) != "reasoning") {
      continue;
    }

    replay_contains_reasoning = item.value("id", std::string{}) == "rs_1" &&
        item.value("encrypted_content", std::string{}) == "encrypted-reasoning";
    if (replay_contains_reasoning) {
      break;
    }
  }

  if (!replay_contains_reasoning) {
    std::cerr << "Expected replay input to include persisted reasoning item.\n";
    return EXIT_FAILURE;
  }

  const auto sent_body = nlohmann::json::parse(fake_http_client->last_request_.body, nullptr, false);
  if (sent_body.is_discarded() || sent_body.value("model", std::string{}) != "openai/gpt-4o") {
    std::cerr << "Expected HTTP client to receive serialized request body.\n";
    return EXIT_FAILURE;
  }

  if (fake_http_client->last_request_.method != "POST") {
    std::cerr << "Expected HTTP method POST.\n";
    return EXIT_FAILURE;
  }

  if (!contains_header(fake_http_client->last_request_, "Authorization", "Bearer secret-token")) {
    std::cerr << "Expected Authorization header.\n";
    return EXIT_FAILURE;
  }

  try {
    auto error_http_client = std::make_shared<FakeHttpClient>(429, nlohmann::json{
        {"error", {
            {"code", "rate_limit_exceeded"},
            {"message", "Rate limit exceeded. Please try again later."},
        }},
    }.dump());

    OpenRouterProvider error_provider(OpenRouterProviderOptions{
        .http_client = error_http_client,
    });

    (void)error_provider.generate(GenerateRequest{
        .model = "openai/gpt-4o",
        .messages = messages,
    });

    std::cerr << "Expected API error to throw.\n";
    return EXIT_FAILURE;
  } catch (const OpenRouterError& error) {
    if (error.status_code() != 429 || !error.api_code().has_value() || *error.api_code() != "rate_limit_exceeded") {
      std::cerr << "Unexpected OpenRouterError contents.\n";
      return EXIT_FAILURE;
    }
  }

  try {
    auto invalid_json_http_client = std::make_shared<FakeHttpClient>(200, "not-json");

    OpenRouterProvider invalid_json_provider(OpenRouterProviderOptions{
        .http_client = invalid_json_http_client,
    });

    (void)invalid_json_provider.generate(GenerateRequest{
        .model = "openai/gpt-4o",
        .messages = messages,
    });

    std::cerr << "Expected invalid JSON to throw.\n";
    return EXIT_FAILURE;
  } catch (const OpenRouterError&) {
  }

  std::cout << "openrouter_mapping_smoke passed\n";
  return EXIT_SUCCESS;
}

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/http/client.hpp"
#include "tiny_hyper_router/http/curl_http_client.hpp"
#include "tiny_hyper_router/providers/openrouter/index.hpp"
#include "tiny_hyper_router/storage/in_memory_storage.hpp"
#include "tiny_hyper_router/tiny_hyper_router.hpp"

namespace {

void write_text_file(const std::filesystem::path& path, const std::string& content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("Failed to open file for writing: " + path.string());
  }

  stream << content;
  if (!stream) {
    throw std::runtime_error("Failed to write file: " + path.string());
  }
}

void write_json_file(const std::filesystem::path& path, const nlohmann::json& value) {
  write_text_file(path, value.dump(2));
}

nlohmann::json headers_to_json(const std::vector<std::pair<std::string, std::string>>& headers) {
  nlohmann::json result = nlohmann::json::array();
  for (const auto& [key, value] : headers) {
    result.push_back({
        {"key", key},
        {"value", key == "Authorization" ? "Bearer ***redacted***" : value},
    });
  }
  return result;
}

int read_env_int(const char* name, int default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string(value).empty()) {
    return default_value;
  }

  try {
    const int parsed = std::stoi(value);
    return parsed > 0 ? parsed : default_value;
  } catch (const std::exception&) {
    return default_value;
  }
}

std::string build_large_tool_text(const std::string& tool_name,
                                  const std::string& seed,
                                  int repeat_count) {
  const std::string block =
      tool_name + " seed=" + seed +
      " :: This is an intentionally large deterministic payload for transcript, replay, and prompt-cache testing. "
      "It is repeated many times so the follow-up request crosses the prompt caching threshold and exercises large tool-output transcription.\n";

  std::string result;
  result.reserve(block.size() * static_cast<std::size_t>(repeat_count + 8));

  for (int index = 0; index < repeat_count; ++index) {
    result += "chunk-" + std::to_string(index) + ": " + block;
  }

  return result;
}

nlohmann::json model_response_to_json(const tiny_hyper_router::ModelResponse& response) {
  nlohmann::json json = {
      {"tool_calls", nlohmann::json::array()},
      {"generated_images", nlohmann::json::array()},
      {"reasoning", nlohmann::json::array()},
  };

  if (response.message.has_value()) {
    json["message"] = {
        {"role", tiny_hyper_router::to_string(response.message->role)},
        {"content", response.message->content},
        {"timestamp", response.message->timestamp},
        {"reasoning_tokens", response.message->reasoning_tokens.has_value() ? nlohmann::json(*response.message->reasoning_tokens) : nlohmann::json(nullptr)},
        {"reasoning", nlohmann::json::array()},
    };

    for (const auto& reasoning : response.message->reasoning) {
      nlohmann::json summary = nlohmann::json::array();
      for (const auto& summary_item : reasoning.summary) {
        summary.push_back({
            {"text", summary_item.text},
            {"type", summary_item.type.has_value() ? nlohmann::json(*summary_item.type) : nlohmann::json(nullptr)},
        });
      }

      json["message"]["reasoning"].push_back({
          {"id", reasoning.id.has_value() ? nlohmann::json(*reasoning.id) : nlohmann::json(nullptr)},
          {"encrypted_content", reasoning.encrypted_content.has_value() ? nlohmann::json(*reasoning.encrypted_content) : nlohmann::json(nullptr)},
          {"format", reasoning.format.has_value() ? nlohmann::json(*reasoning.format) : nlohmann::json(nullptr)},
          {"status", reasoning.status.has_value() ? nlohmann::json(*reasoning.status) : nlohmann::json(nullptr)},
          {"summary", summary},
      });
    }
  }

  if (response.stop_reason.has_value()) {
    json["stop_reason"] = tiny_hyper_router::to_string(*response.stop_reason);
  }

  if (response.reasoning_tokens.has_value()) {
    json["reasoning_tokens"] = *response.reasoning_tokens;
  }

  for (const auto& tool_call : response.tool_calls) {
    json["tool_calls"].push_back({
        {"id", tool_call.id.has_value() ? nlohmann::json(*tool_call.id) : nlohmann::json(nullptr)},
        {"tool_name", tool_call.tool_name},
        {"args", tool_call.args},
    });
  }

  for (const auto& reasoning : response.reasoning) {
    nlohmann::json summary = nlohmann::json::array();
    for (const auto& summary_item : reasoning.summary) {
      summary.push_back({
          {"text", summary_item.text},
          {"type", summary_item.type.has_value() ? nlohmann::json(*summary_item.type) : nlohmann::json(nullptr)},
      });
    }

    json["reasoning"].push_back({
        {"id", reasoning.id.has_value() ? nlohmann::json(*reasoning.id) : nlohmann::json(nullptr)},
        {"encrypted_content", reasoning.encrypted_content.has_value() ? nlohmann::json(*reasoning.encrypted_content) : nlohmann::json(nullptr)},
        {"format", reasoning.format.has_value() ? nlohmann::json(*reasoning.format) : nlohmann::json(nullptr)},
        {"status", reasoning.status.has_value() ? nlohmann::json(*reasoning.status) : nlohmann::json(nullptr)},
        {"summary", summary},
    });
  }

  return json;
}

class LoggingHttpClient final : public tiny_hyper_router::HttpClient {
 public:
  LoggingHttpClient(std::shared_ptr<tiny_hyper_router::HttpClient> inner,
                    std::filesystem::path output_dir)
      : inner_(std::move(inner)), output_dir_(std::move(output_dir)) {}

  tiny_hyper_router::HttpResponse send(const tiny_hyper_router::HttpRequest& request) override {
    ++request_index_;

    const auto request_json = nlohmann::json::parse(request.body, nullptr, false);
    nlohmann::json request_log = {
        {"index", request_index_},
        {"method", request.method},
        {"url", request.url},
        {"headers", headers_to_json(request.headers)},
        {"body", request_json.is_discarded() ? nlohmann::json(request.body) : request_json},
    };
    write_json_file(output_dir_ / ("http_request_" + std::to_string(request_index_) + ".json"), request_log);

    const auto response = inner_->send(request);
    const auto response_json = nlohmann::json::parse(response.body, nullptr, false);
    nlohmann::json response_log = {
        {"index", request_index_},
        {"status_code", response.status_code},
        {"headers", headers_to_json(response.headers)},
        {"body", response_json.is_discarded() ? nlohmann::json(response.body) : response_json},
    };
    write_json_file(output_dir_ / ("http_response_" + std::to_string(request_index_) + ".json"), response_log);

    return response;
  }

 private:
  std::shared_ptr<tiny_hyper_router::HttpClient> inner_;
  std::filesystem::path output_dir_;
  std::size_t request_index_ = 0;
};

void save_provider_request_shape(const std::filesystem::path& output_dir,
                                 const std::string& name,
                                 const tiny_hyper_router::OpenRouterProvider& provider,
                                 const tiny_hyper_router::GenerateRequest& request) {
  write_json_file(output_dir / (name + "_provider_request_shape.json"), provider.build_request_body(request));
}

void save_sdk_response(const std::filesystem::path& output_dir,
                       const std::string& name,
                       const tiny_hyper_router::ModelResponse& response) {
  write_json_file(output_dir / (name + "_sdk_response.json"), model_response_to_json(response));
}

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Failed to open file for reading: " + path.string());
  }

  return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

nlohmann::json analyze_reasoning_replay_in_request(const nlohmann::json& request_body) {
  nlohmann::json result = {
      {"has_input_array", request_body.contains("input") && request_body.at("input").is_array()},
      {"reasoning_item_count", 0},
      {"reasoning_items", nlohmann::json::array()},
      {"contains_encrypted_reasoning", false},
  };

  if (!result.at("has_input_array").get<bool>()) {
    return result;
  }

  for (const auto& item : request_body.at("input")) {
    if (!item.is_object() || item.value("type", std::string{}) != "reasoning") {
      continue;
    }

    result["reasoning_item_count"] = result.at("reasoning_item_count").get<int>() + 1;
    const auto encrypted_content = item.value("encrypted_content", std::string{});
    if (!encrypted_content.empty()) {
      result["contains_encrypted_reasoning"] = true;
    }

    result["reasoning_items"].push_back({
        {"id", item.contains("id") ? item.at("id") : nlohmann::json(nullptr)},
        {"encrypted_content", encrypted_content.empty() ? nlohmann::json(nullptr) : nlohmann::json(encrypted_content)},
        {"status", item.contains("status") ? item.at("status") : nlohmann::json(nullptr)},
        {"format", item.contains("format") ? item.at("format") : nlohmann::json(nullptr)},
    });
  }

  return result;
}

int run_chain_messages_test(const std::filesystem::path& output_dir,
                            const std::string& api_key,
                            const std::string& model) {
  using namespace tiny_hyper_router;

  auto curl_http_client = std::make_shared<CurlHttpClient>();
  auto logging_http_client = std::make_shared<LoggingHttpClient>(curl_http_client, output_dir / "chain_messages");

  OpenRouterProvider provider(OpenRouterProviderOptions{
      .api_key = api_key,
      .max_output_tokens = 512,
      .reasoning = OpenRouterReasoningOptions{
          .effort = OpenRouterReasoningEffort::Medium,
      },
      .http_client = logging_http_client,
  });

  GenerateRequest request{
      .session_id = std::optional<std::string>{"openrouter-live-chain-messages"},
      .model = model,
      .messages = {
          Message{.role = Role::User, .content = "Say hello in one short sentence.", .timestamp = current_timestamp_utc()},
          Message{.role = Role::Assistant, .content = "Hello there.", .timestamp = current_timestamp_utc()},
          Message{.role = Role::User, .content = "Now answer: what did you just say?", .timestamp = current_timestamp_utc()},
      },
  };

  save_provider_request_shape(output_dir, "chain_messages", provider, request);
  const auto response = provider.generate(request);
  save_sdk_response(output_dir, "chain_messages", response);

  std::cout << "chain_messages: " << (response.message.has_value() ? response.message->content : "<empty>") << "\n";
  return 0;
}

int run_single_tool_test(const std::filesystem::path& output_dir,
                         const std::string& api_key,
                         const std::string& model) {
  using namespace tiny_hyper_router;

  const auto single_tool_http_dir = output_dir / "single_tool";
  const int large_tool_text_repeat_count = read_env_int("OPENROUTER_LIVE_TOOL_TEXT_REPEAT", 160);
  write_json_file(output_dir / "single_tool_large_payload_config.json", {
      {"repeat_count", large_tool_text_repeat_count},
  });

  auto provider = std::make_shared<OpenRouterProvider>(OpenRouterProviderOptions{
      .api_key = api_key,
      .max_output_tokens = 512,
      .reasoning = OpenRouterReasoningOptions{
          .effort = OpenRouterReasoningEffort::Medium,
      },
      .http_client = std::make_shared<LoggingHttpClient>(std::make_shared<CurlHttpClient>(), single_tool_http_dir),
  });

  ToolDefinition echo_tool{
      .name = "echo_tool",
      .description = "Echoes back fake text for testing.",
      .input_schema = {
          {"type", "object"},
          {"properties", {{"text", {{"type", "string"}}}}},
          {"required", nlohmann::json::array({"text"})},
      },
      .execute = [large_tool_text_repeat_count](const nlohmann::json& args, const AgentContext&) {
        const auto text = args.value("text", std::string{});
        const auto large_text = build_large_tool_text("echo_tool", text, large_tool_text_repeat_count);
        return ToolResult{
            .ok = true,
            .output = nlohmann::json{
                {"text", "fake tool output: " + text},
                {"large_text", large_text},
                {"large_text_char_count", large_text.size()},
                {"repeat_count", large_tool_text_repeat_count},
            },
        };
      },
  };

  auto storage = std::make_shared<InMemoryStorage>();
  AgentRuntime runtime(RuntimeConfig{
      .agent = AgentDefinition{
          .name = "single-tool-agent",
          .instructions = "Use the echo_tool exactly once when helpful, then answer briefly.",
          .model = model,
          .tools = {echo_tool},
      },
      .provider = provider,
      .storage = storage,
  });

  save_provider_request_shape(output_dir,
                              "single_tool_initial",
                              *provider,
                              GenerateRequest{
                                  .session_id = std::optional<std::string>{"openrouter-live-single-tool"},
                                  .model = model,
                                  .messages = {
                                      Message{.role = Role::System, .content = "Use the echo_tool exactly once when helpful, then answer briefly.", .timestamp = current_timestamp_utc()},
                                      Message{.role = Role::User, .content = "Call the echo tool with text=hello-world and then summarize result.", .timestamp = current_timestamp_utc()},
                                  },
                                  .tools = {echo_tool},
                              });

  const auto result = runtime.run(AgentRunInput{
      .session_id = "openrouter-live-single-tool",
      .input = "Call the echo tool with text=hello-world and then summarize result.",
      .max_steps = 4,
  });

  nlohmann::json transcript = nlohmann::json::array();
  for (const auto& message : result.messages) {
    nlohmann::json reasoning = nlohmann::json::array();
    for (const auto& item : message.reasoning) {
      nlohmann::json summary = nlohmann::json::array();
      for (const auto& summary_item : item.summary) {
        summary.push_back({
            {"text", summary_item.text},
            {"type", summary_item.type.has_value() ? nlohmann::json(*summary_item.type) : nlohmann::json(nullptr)},
        });
      }

      reasoning.push_back({
          {"id", item.id.has_value() ? nlohmann::json(*item.id) : nlohmann::json(nullptr)},
          {"encrypted_content", item.encrypted_content.has_value() ? nlohmann::json(*item.encrypted_content) : nlohmann::json(nullptr)},
          {"format", item.format.has_value() ? nlohmann::json(*item.format) : nlohmann::json(nullptr)},
          {"status", item.status.has_value() ? nlohmann::json(*item.status) : nlohmann::json(nullptr)},
          {"summary", summary},
      });
    }

    transcript.push_back({
        {"role", to_string(message.role)},
        {"content", message.content},
        {"timestamp", message.timestamp},
        {"tool_call_id", message.tool_call_id.has_value() ? nlohmann::json(*message.tool_call_id) : nlohmann::json(nullptr)},
        {"reasoning_tokens", message.reasoning_tokens.has_value() ? nlohmann::json(*message.reasoning_tokens) : nlohmann::json(nullptr)},
        {"reasoning", reasoning},
    });
  }

  write_json_file(output_dir / "single_tool_runtime_transcript.json", {
      {"status", to_string(result.status)},
      {"messages", transcript},
  });

  const auto second_request_log_path = single_tool_http_dir / "http_request_2.json";
  const auto second_request_log = std::filesystem::exists(second_request_log_path)
      ? nlohmann::json::parse(read_text_file(second_request_log_path), nullptr, false)
      : nlohmann::json();

  if (!second_request_log.is_discarded() && second_request_log.contains("body") && second_request_log.at("body").is_object()) {
    write_json_file(output_dir / "single_tool_second_request_reasoning_replay_check.json",
                    analyze_reasoning_replay_in_request(second_request_log.at("body")));
  }

  std::cout << "single_tool: status=" << to_string(result.status)
            << " messages=" << result.messages.size()
            << " large_tool_text_repeat_count=" << large_tool_text_repeat_count << "\n";
  return 0;
}

int run_tool_chain_test(const std::filesystem::path& output_dir,
                        const std::string& api_key,
                        const std::string& model) {
  using namespace tiny_hyper_router;

  const auto tool_chain_http_dir = output_dir / "tool_chain";
  const int large_tool_text_repeat_count = read_env_int("OPENROUTER_LIVE_TOOL_TEXT_REPEAT", 160);
  write_json_file(output_dir / "tool_chain_large_payload_config.json", {
      {"repeat_count", large_tool_text_repeat_count},
  });

  auto provider = std::make_shared<OpenRouterProvider>(OpenRouterProviderOptions{
      .api_key = api_key,
      .parallel_tool_calls = false,
      .max_output_tokens = 768,
      .reasoning = OpenRouterReasoningOptions{
          .effort = OpenRouterReasoningEffort::High,
      },
      .http_client = std::make_shared<LoggingHttpClient>(std::make_shared<CurlHttpClient>(), tool_chain_http_dir),
  });

  ToolDefinition lookup_tool{
      .name = "lookup_tool",
      .description = "Returns fake lookup data.",
      .input_schema = {
          {"type", "object"},
          {"properties", {{"query", {{"type", "string"}}}}},
          {"required", nlohmann::json::array({"query"})},
      },
      .execute = [large_tool_text_repeat_count](const nlohmann::json& args, const AgentContext&) {
        const auto query = args.value("query", std::string{});
        const auto value = "lookup(" + query + ")";
        const auto large_text = build_large_tool_text("lookup_tool", query, large_tool_text_repeat_count);
        return ToolResult{
            .ok = true,
            .output = nlohmann::json{
                {"value", value},
                {"large_text", large_text},
                {"large_text_char_count", large_text.size()},
                {"repeat_count", large_tool_text_repeat_count},
            },
        };
      },
  };

  ToolDefinition finalize_tool{
      .name = "finalize_tool",
      .description = "Returns fake finalization data.",
      .input_schema = {
          {"type", "object"},
          {"properties", {{"value", {{"type", "string"}}}}},
          {"required", nlohmann::json::array({"value"})},
      },
      .execute = [large_tool_text_repeat_count](const nlohmann::json& args, const AgentContext&) {
        const auto value = args.value("value", std::string{});
        const auto final_value = "finalized(" + value + ")";
        const auto large_text = build_large_tool_text("finalize_tool", value, large_tool_text_repeat_count);
        return ToolResult{
            .ok = true,
            .output = nlohmann::json{
                {"final", final_value},
                {"large_text", large_text},
                {"large_text_char_count", large_text.size()},
                {"repeat_count", large_tool_text_repeat_count},
            },
        };
      },
  };

  auto storage = std::make_shared<InMemoryStorage>();
  AgentRuntime runtime(RuntimeConfig{
      .agent = AgentDefinition{
          .name = "tool-chain-agent",
          .instructions = "Use lookup_tool first, then finalize_tool, then answer briefly.",
          .model = model,
          .tools = {lookup_tool, finalize_tool},
      },
      .provider = provider,
      .storage = storage,
  });

  const auto result = runtime.run(AgentRunInput{
      .session_id = "openrouter-live-tool-chain",
      .input = "Use the tools in sequence to process query mars-size.",
      .max_steps = 6,
  });

  nlohmann::json transcript = nlohmann::json::array();
  for (const auto& message : result.messages) {
    nlohmann::json reasoning = nlohmann::json::array();
    for (const auto& item : message.reasoning) {
      nlohmann::json summary = nlohmann::json::array();
      for (const auto& summary_item : item.summary) {
        summary.push_back({
            {"text", summary_item.text},
            {"type", summary_item.type.has_value() ? nlohmann::json(*summary_item.type) : nlohmann::json(nullptr)},
        });
      }

      reasoning.push_back({
          {"id", item.id.has_value() ? nlohmann::json(*item.id) : nlohmann::json(nullptr)},
          {"encrypted_content", item.encrypted_content.has_value() ? nlohmann::json(*item.encrypted_content) : nlohmann::json(nullptr)},
          {"format", item.format.has_value() ? nlohmann::json(*item.format) : nlohmann::json(nullptr)},
          {"status", item.status.has_value() ? nlohmann::json(*item.status) : nlohmann::json(nullptr)},
          {"summary", summary},
      });
    }

    transcript.push_back({
        {"role", to_string(message.role)},
        {"content", message.content},
        {"timestamp", message.timestamp},
        {"tool_call_id", message.tool_call_id.has_value() ? nlohmann::json(*message.tool_call_id) : nlohmann::json(nullptr)},
        {"reasoning_tokens", message.reasoning_tokens.has_value() ? nlohmann::json(*message.reasoning_tokens) : nlohmann::json(nullptr)},
        {"reasoning", reasoning},
    });
  }

  write_json_file(output_dir / "tool_chain_runtime_transcript.json", {
      {"status", to_string(result.status)},
      {"messages", transcript},
  });

  const auto second_request_log_path = tool_chain_http_dir / "http_request_2.json";
  const auto second_request_log = std::filesystem::exists(second_request_log_path)
      ? nlohmann::json::parse(read_text_file(second_request_log_path), nullptr, false)
      : nlohmann::json();

  if (!second_request_log.is_discarded() && second_request_log.contains("body") && second_request_log.at("body").is_object()) {
    write_json_file(output_dir / "tool_chain_second_request_reasoning_replay_check.json",
                    analyze_reasoning_replay_in_request(second_request_log.at("body")));
  }

  std::cout << "tool_chain: status=" << to_string(result.status)
            << " messages=" << result.messages.size()
            << " large_tool_text_repeat_count=" << large_tool_text_repeat_count << "\n";
  return 0;
}

}  // namespace

int main() {
  try {
    const char* api_key_env = std::getenv("OPENROUTER_API_KEY");
    if (api_key_env == nullptr || std::string(api_key_env).empty()) {
      std::cerr << "OPENROUTER_API_KEY is not set.\n";
      return 1;
    }

    const char* model_env = std::getenv("OPENROUTER_MODEL");
    const std::string model =
        (model_env != nullptr && std::string(model_env).size() > 0)
            ? std::string(model_env)
            : std::string{"openai/o4-mini"};

    const auto output_dir = std::filesystem::path{"out"} / "openrouter-live-manual";
    std::filesystem::create_directories(output_dir);

    int exit_code = 0;
    exit_code |= run_chain_messages_test(output_dir, api_key_env, model);
    exit_code |= run_single_tool_test(output_dir, api_key_env, model);
    exit_code |= run_tool_chain_test(output_dir, api_key_env, model);

    std::cout << "Logs written under: " << output_dir.string() << "\n";
    return exit_code;
  } catch (const std::exception& error) {
    std::cerr << "Live manual test failed: " << error.what() << "\n";
    return 1;
  }
}

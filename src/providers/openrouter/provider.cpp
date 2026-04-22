#include "tiny_hyper_router/providers/openrouter/provider.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tiny_hyper_router {

namespace {

constexpr std::size_t k_error_body_snippet_limit = 512;

std::string truncate_for_error(std::string text) {
  if (text.size() <= k_error_body_snippet_limit) {
    return text;
  }

  return text.substr(0, k_error_body_snippet_limit) + "...";
}

nlohmann::json parse_tool_arguments(const nlohmann::json& value) {
  if (value.is_object() || value.is_array()) {
    return value;
  }

  if (value.is_string()) {
    const auto& text = value.get_ref<const std::string&>();
    if (text.empty()) {
      return nlohmann::json::object();
    }

    const auto parsed = nlohmann::json::parse(text, nullptr, false);
    return parsed.is_discarded() ? nlohmann::json::object() : parsed;
  }

  return nlohmann::json::object();
}

std::string extract_output_text(const OpenRouterResponseBody& response_body) {
  if (response_body.contains("output_text") && response_body.at("output_text").is_string()) {
    return response_body.at("output_text").get<std::string>();
  }

  std::string text;

  if (!response_body.contains("output") || !response_body.at("output").is_array()) {
    return text;
  }

  for (const auto& item : response_body.at("output")) {
    if (!item.is_object() || item.value("type", std::string{}) != "message") {
      continue;
    }

    if (!item.contains("content") || !item.at("content").is_array()) {
      continue;
    }

    for (const auto& part : item.at("content")) {
      if (!part.is_object() || part.value("type", std::string{}) != "output_text") {
        continue;
      }

      const auto chunk = part.value("text", std::string{});
      if (chunk.empty()) {
        continue;
      }

      if (!text.empty()) {
        text += "\n";
      }

      text += chunk;
    }
  }

  return text;
}

std::vector<ReasoningDetails> extract_reasoning_details(const OpenRouterResponseBody& response_body) {
  std::vector<ReasoningDetails> reasoning;

  if (!response_body.contains("output") || !response_body.at("output").is_array()) {
    return reasoning;
  }

  for (const auto& item : response_body.at("output")) {
    if (!item.is_object() || item.value("type", std::string{}) != "reasoning") {
      continue;
    }

    ReasoningDetails details;

    const auto id = item.value("id", std::string{});
    if (!id.empty()) {
      details.id = id;
    }

    const auto encrypted_content = item.value("encrypted_content", std::string{});
    if (!encrypted_content.empty()) {
      details.encrypted_content = encrypted_content;
    }

    const auto format = item.value("format", std::string{});
    if (!format.empty()) {
      details.format = format;
    }

    const auto status = item.value("status", std::string{});
    if (!status.empty()) {
      details.status = status;
    }

    details.raw_item = item;

    if (item.contains("summary") && item.at("summary").is_array()) {
      for (const auto& summary_item : item.at("summary")) {
        if (summary_item.is_string()) {
          details.summary.push_back(ReasoningSummaryItem{
              .text = summary_item.get<std::string>(),
              .type = std::nullopt,
          });
          continue;
        }

        if (summary_item.is_object()) {
          const auto text = summary_item.value("text", std::string{});
          if (!text.empty()) {
            const auto type = summary_item.value("type", std::string{});
            details.summary.push_back(ReasoningSummaryItem{
                .text = text,
                .type = type.empty() ? std::nullopt : std::optional<std::string>{type},
            });
          }
        }
      }
    }

    reasoning.push_back(std::move(details));
  }

  return reasoning;
}

std::optional<int> extract_reasoning_tokens(const OpenRouterResponseBody& response_body) {
  if (!response_body.contains("usage") || !response_body.at("usage").is_object()) {
    return std::nullopt;
  }

  const auto& usage = response_body.at("usage");
  if (!usage.contains("output_tokens_details") || !usage.at("output_tokens_details").is_object()) {
    return std::nullopt;
  }

  const auto& output_tokens_details = usage.at("output_tokens_details");
  if (!output_tokens_details.contains("reasoning_tokens") || !output_tokens_details.at("reasoning_tokens").is_number_integer()) {
    return std::nullopt;
  }

  return output_tokens_details.at("reasoning_tokens").get<int>();
}

std::optional<std::string> try_extract_api_error_code(const OpenRouterResponseBody& response_body) {
  if (!response_body.is_object() || !response_body.contains("error") || !response_body.at("error").is_object()) {
    return std::nullopt;
  }

  const auto& error = response_body.at("error");
  const auto code = error.value("code", std::string{});
  return code.empty() ? std::nullopt : std::optional<std::string>{code};
}

std::optional<std::string> try_extract_api_error_message(const OpenRouterResponseBody& response_body) {
  if (!response_body.is_object() || !response_body.contains("error") || !response_body.at("error").is_object()) {
    return std::nullopt;
  }

  const auto& error = response_body.at("error");
  const auto message = error.value("message", std::string{});
  return message.empty() ? std::nullopt : std::optional<std::string>{message};
}

void throw_if_api_error_present(const OpenRouterResponseBody& response_body, long status_code = 0) {
  const auto api_message = try_extract_api_error_message(response_body);
  if (!api_message.has_value()) {
    return;
  }

  throw OpenRouterError(
      "OpenRouter API error: " + *api_message,
      status_code,
      try_extract_api_error_code(response_body));
}

nlohmann::json build_reasoning_json(const OpenRouterReasoningOptions& reasoning) {
  nlohmann::json json = nlohmann::json::object();

  if (reasoning.effort.has_value()) {
    json["effort"] = to_string(*reasoning.effort);
  }

  if (reasoning.max_tokens.has_value()) {
    json["max_tokens"] = *reasoning.max_tokens;
  }

  if (reasoning.exclude.has_value()) {
    json["exclude"] = *reasoning.exclude;
  }

  if (reasoning.enabled.has_value()) {
    json["enabled"] = *reasoning.enabled;
  }

  return json;
}

}  // namespace

OpenRouterProvider::OpenRouterProvider(OpenRouterProviderOptions options)
    : options_(std::move(options)),
      http_client_(options_.http_client ? options_.http_client : std::make_shared<NullHttpClient>()) {}

ModelResponse OpenRouterProvider::generate(const GenerateRequest& request) {
  const auto request_body = build_request_body(request);

  std::vector<std::pair<std::string, std::string>> headers = {
      {"Content-Type", "application/json"},
  };

  if (options_.api_key.has_value() && !options_.api_key->empty()) {
    headers.emplace_back("Authorization", "Bearer " + *options_.api_key);
  }

  const auto http_response = http_client_->send(HttpRequest{
      .method = "POST",
      .url = options_.base_url + options_.responses_path,
      .headers = std::move(headers),
      .body = request_body.dump(),
  });

  const auto response_body = nlohmann::json::parse(http_response.body, nullptr, false);

  if (http_response.status_code < 200 || http_response.status_code >= 300) {
    if (!response_body.is_discarded()) {
      throw_if_api_error_present(response_body, http_response.status_code);
    }

    throw OpenRouterError(
        "OpenRouter request failed with HTTP status " + std::to_string(http_response.status_code) +
            ": " + truncate_for_error(http_response.body),
        http_response.status_code);
  }

  if (response_body.is_discarded()) {
    throw OpenRouterError(
        "OpenRouter returned invalid JSON: " + truncate_for_error(http_response.body),
        http_response.status_code);
  }

  throw_if_api_error_present(response_body, http_response.status_code);
  return parse_response_body(response_body);
}

OpenRouterRequestBody OpenRouterProvider::build_request_body(const GenerateRequest& request) const {
  OpenRouterRequestBody body = {
      {"model", request.model},
      {"input", to_openrouter_input_items(request.messages)},
      {"stream", false},
  };

  std::optional<std::string> instructions;
  for (const auto& message : request.messages) {
    if (message.role == Role::System && !message.content.empty()) {
      if (instructions.has_value()) {
        *instructions += "\n\n";
        *instructions += message.content;
      } else {
        instructions = message.content;
      }
    }
  }

  if (instructions.has_value()) {
    body["instructions"] = *instructions;
  }

  const auto tool_definitions = to_openrouter_tool_definitions(request.tools);
  if (!tool_definitions.empty()) {
    body["tools"] = tool_definitions;
    body["tool_choice"] = "auto";

    if (options_.parallel_tool_calls.has_value()) {
      body["parallel_tool_calls"] = *options_.parallel_tool_calls;
    }
  }

  body["store"] = false;

  if (options_.include_reasoning_encrypted_content) {
    body["include"] = nlohmann::json::array({"reasoning.encrypted_content"});
  }

  if (options_.max_output_tokens.has_value()) {
    body["max_output_tokens"] = *options_.max_output_tokens;
  }

  if (options_.temperature.has_value()) {
    body["temperature"] = *options_.temperature;
  }

  if (options_.top_p.has_value()) {
    body["top_p"] = *options_.top_p;
  }

  if (request.session_id.has_value() && !request.session_id->empty()) {
    body["session_id"] = *request.session_id;
  }

  if (options_.user.has_value() && !options_.user->empty()) {
    body["user"] = *options_.user;
  }

  if (options_.metadata.is_object() && !options_.metadata.empty()) {
    body["metadata"] = options_.metadata;
  }

  if (options_.reasoning.has_value()) {
    const auto reasoning_json = build_reasoning_json(*options_.reasoning);
    if (!reasoning_json.empty()) {
      body["reasoning"] = reasoning_json;
    }
  }

  return body;
}

ModelResponse OpenRouterProvider::parse_response_body(const OpenRouterResponseBody& response_body) const {
  throw_if_api_error_present(response_body);

  const auto tool_calls = normalize_tool_calls(response_body);
  const auto generated_images = extract_generated_images(response_body);
  const auto text = extract_output_text(response_body);
  const auto reasoning = extract_reasoning_details(response_body);
  const auto reasoning_tokens = extract_reasoning_tokens(response_body);

  ModelResponse response{
      .message = std::nullopt,
      .tool_calls = tool_calls,
      .stop_reason = tool_calls.empty() ? StopReason::Completed : StopReason::ToolCalls,
      .generated_images = generated_images,
      .reasoning = reasoning,
      .reasoning_tokens = reasoning_tokens,
  };

  if (!text.empty() || !tool_calls.empty() || !reasoning.empty()) {
    response.message = Message{
        .role = Role::Assistant,
        .content = text,
        .timestamp = current_timestamp_utc(),
        .tool_calls = tool_calls,
        .reasoning = reasoning,
        .reasoning_tokens = reasoning_tokens,
    };
  }

  return response;
}

const OpenRouterProviderOptions& OpenRouterProvider::options() const noexcept {
  return options_;
}

std::vector<ToolCall> OpenRouterProvider::normalize_tool_calls(const OpenRouterResponseBody& response_body) const {
  std::vector<ToolCall> tool_calls;

  if (!response_body.contains("output") || !response_body.at("output").is_array()) {
    return tool_calls;
  }

  for (const auto& item : response_body.at("output")) {
    if (!item.is_object() || item.value("type", std::string{}) != "function_call") {
      continue;
    }

    ToolCall tool_call;

    const auto call_id = item.value("call_id", std::string{});
    if (!call_id.empty()) {
      tool_call.id = call_id;
    }

    tool_call.tool_name = item.value("name", std::string{"unknown_tool"});
    tool_call.args = parse_tool_arguments(item.value("arguments", nlohmann::json::object()));
    tool_calls.push_back(std::move(tool_call));
  }

  return tool_calls;
}

std::vector<GeneratedImage> OpenRouterProvider::extract_generated_images(const OpenRouterResponseBody& response_body) const {
  std::vector<GeneratedImage> images;

  if (!response_body.contains("output") || !response_body.at("output").is_array()) {
    return images;
  }

  for (const auto& item : response_body.at("output")) {
    if (!item.is_object() || item.value("type", std::string{}) != "image_generation_call") {
      continue;
    }

    const auto result = item.value("result", std::string{});
    if (result.empty()) {
      continue;
    }

    if (result.starts_with("data:")) {
      GeneratedImage image;
      image.data_url = result;

      const auto mime_end = result.find_first_of(";,", 5);
      if (mime_end != std::string::npos && mime_end > 5) {
        image.mime_type = result.substr(5, mime_end - 5);
      }

      images.push_back(std::move(image));
      continue;
    }

    images.push_back(GeneratedImage{
        .url = result,
    });
  }

  return images;
}

}  // namespace tiny_hyper_router

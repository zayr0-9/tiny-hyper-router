#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/http/client.hpp"

namespace tiny_hyper_router {

enum class OpenRouterContinuationStrategy {
  Transcript,
  State,
  Hybrid,
  Ephemeral,
};

enum class OpenRouterReasoningEffort {
  Minimal,
  Low,
  Medium,
  High,
};

inline constexpr std::string_view to_string(OpenRouterContinuationStrategy value) noexcept {
  switch (value) {
    case OpenRouterContinuationStrategy::Transcript:
      return "transcript";
    case OpenRouterContinuationStrategy::State:
      return "state";
    case OpenRouterContinuationStrategy::Hybrid:
      return "hybrid";
    case OpenRouterContinuationStrategy::Ephemeral:
      return "ephemeral";
  }

  return "unknown";
}

inline constexpr std::string_view to_string(OpenRouterReasoningEffort value) noexcept {
  switch (value) {
    case OpenRouterReasoningEffort::Minimal:
      return "minimal";
    case OpenRouterReasoningEffort::Low:
      return "low";
    case OpenRouterReasoningEffort::Medium:
      return "medium";
    case OpenRouterReasoningEffort::High:
      return "high";
  }

  return "unknown";
}

class OpenRouterError : public std::runtime_error {
 public:
  OpenRouterError(std::string message,
                  long status_code = 0,
                  std::optional<std::string> api_code = std::nullopt)
      : std::runtime_error(std::move(message)),
        status_code_(status_code),
        api_code_(std::move(api_code)) {}

  [[nodiscard]] long status_code() const noexcept {
    return status_code_;
  }

  [[nodiscard]] const std::optional<std::string>& api_code() const noexcept {
    return api_code_;
  }

 private:
  long status_code_;
  std::optional<std::string> api_code_;
};

struct OpenRouterReasoningOptions {
  std::optional<OpenRouterReasoningEffort> effort;
  std::optional<int> max_tokens;
  std::optional<bool> exclude;
  std::optional<bool> enabled;
};

struct OpenRouterProviderOptions {
  std::optional<std::string> api_key;
  std::string base_url = "https://openrouter.ai/api/v1";
  std::string responses_path = "/responses";
  OpenRouterContinuationStrategy continuation_strategy = OpenRouterContinuationStrategy::Transcript;
  std::optional<bool> parallel_tool_calls = true;
  std::optional<bool> store = false;
  bool include_reasoning_encrypted_content = true;
  std::optional<int> max_output_tokens;
  std::optional<double> temperature;
  std::optional<double> top_p;
  std::optional<std::string> user;
  nlohmann::json metadata = nlohmann::json::object();
  std::optional<OpenRouterReasoningOptions> reasoning;
  std::shared_ptr<HttpClient> http_client;
};

struct OpenRouterToolCallLike {
  std::optional<std::string> id;
  std::optional<std::string> name;
  nlohmann::json arguments = nlohmann::json::object();
};

using OpenRouterInputItem = nlohmann::json;
using OpenRouterToolDefinition = nlohmann::json;
using OpenRouterRequestBody = nlohmann::json;
using OpenRouterResponseBody = nlohmann::json;

}  // namespace tiny_hyper_router

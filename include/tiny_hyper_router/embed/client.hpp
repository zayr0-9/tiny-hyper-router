#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/core/runtime.hpp"
#include "tiny_hyper_router/http/client.hpp"
#include "tiny_hyper_router/providers/openrouter/types.hpp"

namespace tiny_hyper_router::embed {

struct ClientConfig {
  std::string agent_name = "embed-agent";
  std::string instructions;
  std::string model;
  std::optional<std::string> api_key;
  std::filesystem::path storage_directory;
  std::size_t default_max_steps = 5;
  bool default_ephemeral = false;
  bool use_stub_provider = false;
  bool use_in_memory_storage = false;
  std::vector<ToolDefinition> tools;
  BuildMessagesFunction build_messages;
  std::shared_ptr<ModelProvider> provider;
  std::shared_ptr<StorageAdapter> storage;
  std::shared_ptr<HttpClient> http_client;
  std::string openrouter_base_url = "https://openrouter.ai/api/v1";
  std::string openrouter_responses_path = "/responses";
  OpenRouterContinuationStrategy continuation_strategy = OpenRouterContinuationStrategy::Transcript;
  std::optional<bool> parallel_tool_calls = true;
  bool include_reasoning_encrypted_content = true;
  std::optional<int> max_output_tokens;
  std::optional<double> temperature;
  std::optional<double> top_p;
  std::optional<std::string> user;
  nlohmann::json metadata = nlohmann::json::object();
  std::optional<OpenRouterReasoningOptions> reasoning;

  static ClientConfig from_json(const nlohmann::json& json);
};

struct SendMessageRequest {
  std::string session_id;
  std::string input;
  std::optional<std::size_t> max_steps;
  std::optional<bool> ephemeral;

  static SendMessageRequest from_json(const nlohmann::json& json);
};

struct SendMessageResponse {
  std::string session_id;
  RunStatus status = RunStatus::Completed;
  std::vector<Message> messages;
  std::optional<Message> last_assistant_message;
};

struct SessionSnapshot {
  std::string session_id;
  std::vector<Message> messages;
  std::optional<SessionMetadata> metadata;
};

class Client final {
 public:
  explicit Client(ClientConfig config);

  SendMessageResponse send_message(const SendMessageRequest& request);
  nlohmann::json send_message_json(const SendMessageRequest& request);

  SessionSnapshot get_session(const std::string& session_id);
  nlohmann::json get_session_json(const std::string& session_id);

 private:
  AgentDefinition agent_definition_;
  std::shared_ptr<ModelProvider> provider_;
  std::shared_ptr<StorageAdapter> storage_;
  std::size_t default_max_steps_ = 5;
  bool default_ephemeral_ = false;
  std::unique_ptr<AgentRuntime> runtime_;
  std::mutex mutex_;
};

}  // namespace tiny_hyper_router::embed

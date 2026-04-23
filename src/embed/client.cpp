#include "tiny_hyper_router/embed/client.hpp"

#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/providers/openrouter/provider.hpp"
#include "tiny_hyper_router/providers/stub/provider.hpp"
#include "tiny_hyper_router/storage/in_memory_storage.hpp"
#include "tiny_hyper_router/storage/json_file_storage.hpp"
#include "tiny_hyper_router/storage/json_serialization.hpp"

namespace tiny_hyper_router::embed {

namespace {

std::string as_required_string(const nlohmann::json& json, const char* key) {
  if (!json.contains(key) || !json.at(key).is_string()) {
    throw std::invalid_argument(std::string("Missing required string field: ") + key);
  }

  return json.at(key).get<std::string>();
}

std::optional<std::string> as_optional_string(const nlohmann::json& json, const char* key) {
  if (!json.contains(key) || json.at(key).is_null()) {
    return std::nullopt;
  }

  if (!json.at(key).is_string()) {
    throw std::invalid_argument(std::string("Expected string field: ") + key);
  }

  return json.at(key).get<std::string>();
}

nlohmann::json message_to_json(const Message& message) {
  return nlohmann::json(message);
}

std::optional<Message> find_last_assistant_message(const std::vector<Message>& messages) {
  for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
    if (it->role == Role::Assistant) {
      return *it;
    }
  }

  return std::nullopt;
}

nlohmann::json send_message_response_to_json(const SendMessageResponse& response) {
  nlohmann::json json = {
      {"session_id", response.session_id},
      {"status", std::string(to_string(response.status))},
      {"messages", response.messages},
  };

  if (response.last_assistant_message.has_value()) {
    json["last_assistant_message"] = message_to_json(*response.last_assistant_message);
    json["assistant_text"] = response.last_assistant_message->content;
  } else {
    json["last_assistant_message"] = nullptr;
    json["assistant_text"] = nullptr;
  }

  return json;
}

nlohmann::json session_snapshot_to_json(const SessionSnapshot& snapshot) {
  nlohmann::json json = {
      {"session_id", snapshot.session_id},
      {"messages", snapshot.messages},
  };

  if (snapshot.metadata.has_value()) {
    json["metadata"] = *snapshot.metadata;
  } else {
    json["metadata"] = nullptr;
  }

  return json;
}

}  // namespace

ClientConfig ClientConfig::from_json(const nlohmann::json& json) {
  ClientConfig config;

  if (json.contains("agent_name") && json.at("agent_name").is_string()) {
    config.agent_name = json.at("agent_name").get<std::string>();
  }

  config.instructions = as_required_string(json, "instructions");
  config.model = as_required_string(json, "model");

  if (const auto api_key = as_optional_string(json, "api_key"); api_key.has_value()) {
    config.api_key = *api_key;
  }

  if (json.contains("storage_directory") && json.at("storage_directory").is_string()) {
    config.storage_directory = json.at("storage_directory").get<std::string>();
  } else if (json.contains("storage_dir") && json.at("storage_dir").is_string()) {
    config.storage_directory = json.at("storage_dir").get<std::string>();
  }

  if (json.contains("default_max_steps") && json.at("default_max_steps").is_number_unsigned()) {
    config.default_max_steps = json.at("default_max_steps").get<std::size_t>();
  }

  if (json.contains("default_ephemeral") && json.at("default_ephemeral").is_boolean()) {
    config.default_ephemeral = json.at("default_ephemeral").get<bool>();
  }

  if (json.contains("use_stub_provider") && json.at("use_stub_provider").is_boolean()) {
    config.use_stub_provider = json.at("use_stub_provider").get<bool>();
  }

  if (json.contains("use_in_memory_storage") && json.at("use_in_memory_storage").is_boolean()) {
    config.use_in_memory_storage = json.at("use_in_memory_storage").get<bool>();
  }

  if (json.contains("openrouter_base_url") && json.at("openrouter_base_url").is_string()) {
    config.openrouter_base_url = json.at("openrouter_base_url").get<std::string>();
  }

  if (json.contains("openrouter_responses_path") && json.at("openrouter_responses_path").is_string()) {
    config.openrouter_responses_path = json.at("openrouter_responses_path").get<std::string>();
  }

  if (json.contains("parallel_tool_calls") && !json.at("parallel_tool_calls").is_null()) {
    config.parallel_tool_calls = json.at("parallel_tool_calls").get<bool>();
  }

  if (json.contains("include_reasoning_encrypted_content") && json.at("include_reasoning_encrypted_content").is_boolean()) {
    config.include_reasoning_encrypted_content = json.at("include_reasoning_encrypted_content").get<bool>();
  }

  if (json.contains("max_output_tokens") && !json.at("max_output_tokens").is_null()) {
    config.max_output_tokens = json.at("max_output_tokens").get<int>();
  }

  if (json.contains("temperature") && !json.at("temperature").is_null()) {
    config.temperature = json.at("temperature").get<double>();
  }

  if (json.contains("top_p") && !json.at("top_p").is_null()) {
    config.top_p = json.at("top_p").get<double>();
  }

  if (const auto user = as_optional_string(json, "user"); user.has_value()) {
    config.user = *user;
  }

  if (json.contains("metadata") && json.at("metadata").is_object()) {
    config.metadata = json.at("metadata");
  }

  return config;
}

SendMessageRequest SendMessageRequest::from_json(const nlohmann::json& json) {
  SendMessageRequest request;
  request.session_id = as_required_string(json, "session_id");
  request.input = as_required_string(json, "input");

  if (json.contains("max_steps") && json.at("max_steps").is_number_unsigned()) {
    request.max_steps = json.at("max_steps").get<std::size_t>();
  }

  if (json.contains("ephemeral") && json.at("ephemeral").is_boolean()) {
    request.ephemeral = json.at("ephemeral").get<bool>();
  }

  return request;
}

Client::Client(ClientConfig config)
    : agent_definition_{
          .name = std::move(config.agent_name),
          .instructions = std::move(config.instructions),
          .model = std::move(config.model),
          .tools = std::move(config.tools),
          .build_messages = std::move(config.build_messages),
      },
      default_max_steps_(config.default_max_steps == 0 ? std::size_t{5} : config.default_max_steps),
      default_ephemeral_(config.default_ephemeral) {
  if (config.provider) {
    provider_ = std::move(config.provider);
  } else if (config.use_stub_provider) {
    provider_ = std::make_shared<StubProvider>();
  } else {
    auto openrouter_options = OpenRouterProviderOptions{
        .api_key = std::move(config.api_key),
        .base_url = std::move(config.openrouter_base_url),
        .responses_path = std::move(config.openrouter_responses_path),
        .continuation_strategy = config.continuation_strategy,
        .parallel_tool_calls = config.parallel_tool_calls,
        .store = false,
        .include_reasoning_encrypted_content = config.include_reasoning_encrypted_content,
        .max_output_tokens = config.max_output_tokens,
        .temperature = config.temperature,
        .top_p = config.top_p,
        .user = std::move(config.user),
        .metadata = std::move(config.metadata),
        .reasoning = std::move(config.reasoning),
        .http_client = std::move(config.http_client),
    };
    provider_ = std::make_shared<OpenRouterProvider>(std::move(openrouter_options));
  }

  if (config.storage) {
    storage_ = std::move(config.storage);
  } else if (config.use_in_memory_storage) {
    storage_ = std::make_shared<InMemoryStorage>();
  } else {
    if (config.storage_directory.empty()) {
      throw std::invalid_argument("ClientConfig.storage_directory is required when not using in-memory storage.");
    }

    storage_ = std::make_shared<JsonFileStorage>(std::move(config.storage_directory));
  }

  runtime_ = std::make_unique<AgentRuntime>(RuntimeConfig{
      .agent = agent_definition_,
      .provider = provider_,
      .storage = storage_,
  });
}

SendMessageResponse Client::send_message(const SendMessageRequest& request) {
  std::lock_guard<std::mutex> lock(mutex_);

  const auto result = runtime_->run(AgentRunInput{
      .session_id = request.session_id,
      .input = request.input,
      .max_steps = request.max_steps.value_or(default_max_steps_),
      .ephemeral = request.ephemeral.value_or(default_ephemeral_),
  });

  return SendMessageResponse{
      .session_id = request.session_id,
      .status = result.status,
      .messages = result.messages,
      .last_assistant_message = find_last_assistant_message(result.messages),
  };
}

nlohmann::json Client::send_message_json(const SendMessageRequest& request) {
  return send_message_response_to_json(send_message(request));
}

SessionSnapshot Client::get_session(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return SessionSnapshot{
      .session_id = session_id,
      .messages = storage_->load_messages(session_id),
      .metadata = storage_->get_session_metadata(session_id),
  };
}

nlohmann::json Client::get_session_json(const std::string& session_id) {
  return session_snapshot_to_json(get_session(session_id));
}

}  // namespace tiny_hyper_router::embed

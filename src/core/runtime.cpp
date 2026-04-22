#include "tiny_hyper_router/core/runtime.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

namespace tiny_hyper_router {

namespace {

std::string build_default_toolset_hash_input(const std::vector<ToolDefinition>& tools) {
  nlohmann::json items = nlohmann::json::array();

  for (const auto& tool : tools) {
    items.push_back({
        {"name", tool.name},
        {"description", tool.description},
        {"inputSchema", tool.input_schema},
    });
  }

  return items.dump();
}

}  // namespace

AgentRuntime::AgentRuntime(RuntimeConfig config)
    : config_(std::move(config)) {
  if (!config_.provider) {
    throw std::invalid_argument("AgentRuntime requires a provider.");
  }

  if (!config_.storage) {
    throw std::invalid_argument("AgentRuntime requires a storage adapter.");
  }
}

RuntimeResult AgentRuntime::run(const AgentRunInput& input) {
  const auto max_steps = input.max_steps == 0 ? std::size_t{5} : input.max_steps;
  const bool is_ephemeral = input.ephemeral;

  std::vector<Message> previous_messages;
  std::optional<SessionMetadata> previous_session_metadata;

  if (!is_ephemeral) {
    previous_messages = config_.storage->load_messages(input.session_id);
    previous_messages.erase(
        std::remove_if(previous_messages.begin(), previous_messages.end(), [](const Message& message) {
          return message.role == Role::System;
        }),
        previous_messages.end());
    previous_session_metadata = config_.storage->get_session_metadata(input.session_id);
  }

  std::vector<Message> base_messages;
  if (config_.agent.build_messages) {
    base_messages = config_.agent.build_messages(input.input);
  } else {
    base_messages.push_back(Message{
        .role = Role::User,
        .content = input.input,
        .timestamp = current_timestamp_utc(),
    });
  }

  std::vector<Message> messages;
  messages.reserve(1 + previous_messages.size() + base_messages.size());
  messages.push_back(Message{
      .role = Role::System,
      .content = config_.agent.instructions,
      .timestamp = current_timestamp_utc(),
  });
  messages.insert(messages.end(), previous_messages.begin(), previous_messages.end());
  messages.insert(messages.end(), base_messages.begin(), base_messages.end());

  RunStatus status = RunStatus::MaxStepsReached;

  for (std::size_t step = 1; step <= max_steps; ++step) {
    ModelResponse response = config_.provider->generate(GenerateRequest{
        .session_id = std::optional<std::string>{input.session_id},
        .model = config_.agent.model,
        .messages = messages,
        .tools = config_.agent.tools,
        .previous_session_metadata = previous_session_metadata,
        .ephemeral = is_ephemeral,
    });

    if (response.message.has_value()) {
      messages.push_back(*response.message);
    }

    const auto& tool_calls = response.tool_calls;
    if (tool_calls.empty()) {
      status = RunStatus::Completed;
      break;
    }

    auto tool_messages = execute_tool_calls(input.session_id, step, tool_calls);
    messages.insert(messages.end(), tool_messages.begin(), tool_messages.end());
    status = RunStatus::NeedsTool;

    if (step == max_steps) {
      status = RunStatus::MaxStepsReached;
      break;
    }
  }

  if (!is_ephemeral) {
    std::vector<Message> transcript_messages;
    transcript_messages.reserve(messages.size());

    for (const auto& message : messages) {
      if (message.role != Role::System) {
        transcript_messages.push_back(message);
      }
    }

    config_.storage->save_messages(input.session_id, transcript_messages);
    config_.storage->save_run(RunRecord{
        .session_id = input.session_id,
        .status = status,
    });
    update_session_metadata(input.session_id);
  }

  return RuntimeResult{
      .status = status,
      .messages = std::move(messages),
  };
}

void AgentRuntime::update_session_metadata(const std::string& session_id) {
  auto metadata = config_.storage->get_session_metadata(session_id).value_or(SessionMetadata{});

  metadata.agent_name = config_.agent.name;
  metadata.model = config_.agent.model;
  metadata.prompt_hash = hash_value(config_.agent.instructions);
  metadata.prompt_snapshot = config_.agent.instructions;
  metadata.toolset_hash = hash_value(build_default_toolset_hash_input(config_.agent.tools));
  metadata.updated_at = current_timestamp_utc();

  config_.storage->set_session_metadata(session_id, metadata);
}

std::string AgentRuntime::hash_value(const std::string& value) const {
  return std::to_string(std::hash<std::string>{}(value));
}

std::vector<Message> AgentRuntime::execute_tool_calls(
    const std::string& session_id,
    std::size_t step,
    const std::vector<ToolCall>& tool_calls) const {
  std::vector<Message> messages;
  messages.reserve(tool_calls.size());

  for (const auto& tool_call : tool_calls) {
    const auto tool_it = std::find_if(
        config_.agent.tools.begin(),
        config_.agent.tools.end(),
        [&](const ToolDefinition& tool) {
          return tool.name == tool_call.tool_name;
        });

    ToolResult result;
    if (tool_it == config_.agent.tools.end()) {
      result.ok = false;
      result.error = std::string("Unknown tool: ") + tool_call.tool_name;
    } else if (!tool_it->can_execute()) {
      result.ok = false;
      result.error = std::string("Tool has no execute function: ") + tool_call.tool_name;
    } else {
      result = tool_it->execute(tool_call.args, AgentContext{
          .session_id = session_id,
          .step = step,
      });
    }

    nlohmann::json result_json = {
        {"ok", result.ok},
    };

    if (result.output.has_value()) {
      result_json["output"] = *result.output;
    }

    if (result.error.has_value()) {
      result_json["error"] = *result.error;
    }

    messages.push_back(Message{
        .role = Role::Tool,
        .content = result_json.dump(),
        .name = tool_call.tool_name,
        .timestamp = current_timestamp_utc(),
        .tool_call_id = tool_call.id,
    });
  }

  return messages;
}

}  // namespace tiny_hyper_router

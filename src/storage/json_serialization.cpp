#include "tiny_hyper_router/storage/json_serialization.hpp"

#include <stdexcept>

namespace tiny_hyper_router {

namespace {

std::string role_to_string(Role role) {
  return std::string(to_string(role));
}

Role role_from_string(const std::string& value) {
  if (value == "system") {
    return Role::System;
  }
  if (value == "user") {
    return Role::User;
  }
  if (value == "assistant") {
    return Role::Assistant;
  }
  if (value == "tool") {
    return Role::Tool;
  }

  throw std::invalid_argument("Unknown role: " + value);
}

std::string run_status_to_string(RunStatus status) {
  return std::string(to_string(status));
}

RunStatus run_status_from_string(const std::string& value) {
  if (value == "completed") {
    return RunStatus::Completed;
  }
  if (value == "needs_tool") {
    return RunStatus::NeedsTool;
  }
  if (value == "waiting_for_user") {
    return RunStatus::WaitingForUser;
  }
  if (value == "max_steps_reached") {
    return RunStatus::MaxStepsReached;
  }
  if (value == "failed") {
    return RunStatus::Failed;
  }
  if (value == "waiting_for_permission") {
    return RunStatus::WaitingForPermission;
  }

  throw std::invalid_argument("Unknown run status: " + value);
}

}  // namespace

void to_json(nlohmann::json& json, const ToolCall& value) {
  json = {
      {"tool_name", value.tool_name},
      {"args", value.args},
  };

  if (value.id.has_value()) {
    json["id"] = *value.id;
  }
}

void from_json(const nlohmann::json& json, ToolCall& value) {
  value = ToolCall{};
  value.tool_name = json.value("tool_name", std::string{});
  value.args = json.contains("args") ? json.at("args") : nlohmann::json::object();

  if (json.contains("id") && !json.at("id").is_null()) {
    value.id = json.at("id").get<std::string>();
  }
}

void to_json(nlohmann::json& json, const ReasoningSummaryItem& value) {
  json = {
      {"text", value.text},
  };

  if (value.type.has_value()) {
    json["type"] = *value.type;
  }
}

void from_json(const nlohmann::json& json, ReasoningSummaryItem& value) {
  value = ReasoningSummaryItem{};
  value.text = json.value("text", std::string{});

  if (json.contains("type") && !json.at("type").is_null()) {
    value.type = json.at("type").get<std::string>();
  }
}

void to_json(nlohmann::json& json, const ReasoningDetails& value) {
  json = {
      {"summary", value.summary},
  };

  if (value.id.has_value()) {
    json["id"] = *value.id;
  }
  if (value.encrypted_content.has_value()) {
    json["encrypted_content"] = *value.encrypted_content;
  }
  if (value.format.has_value()) {
    json["format"] = *value.format;
  }
  if (value.status.has_value()) {
    json["status"] = *value.status;
  }
  if (value.raw_item.has_value()) {
    json["raw_item"] = *value.raw_item;
  }
}

void from_json(const nlohmann::json& json, ReasoningDetails& value) {
  value = ReasoningDetails{};

  if (json.contains("id") && !json.at("id").is_null()) {
    value.id = json.at("id").get<std::string>();
  }
  if (json.contains("encrypted_content") && !json.at("encrypted_content").is_null()) {
    value.encrypted_content = json.at("encrypted_content").get<std::string>();
  }
  if (json.contains("format") && !json.at("format").is_null()) {
    value.format = json.at("format").get<std::string>();
  }
  if (json.contains("status") && !json.at("status").is_null()) {
    value.status = json.at("status").get<std::string>();
  }
  if (json.contains("summary") && json.at("summary").is_array()) {
    value.summary = json.at("summary").get<std::vector<ReasoningSummaryItem>>();
  }
  if (json.contains("raw_item") && !json.at("raw_item").is_null()) {
    value.raw_item = json.at("raw_item");
  }
}

void to_json(nlohmann::json& json, const Message& value) {
  json = {
      {"role", role_to_string(value.role)},
      {"content", value.content},
      {"timestamp", value.timestamp},
      {"tool_calls", value.tool_calls},
      {"reasoning", value.reasoning},
  };

  if (value.name.has_value()) {
    json["name"] = *value.name;
  }
  if (value.tool_call_id.has_value()) {
    json["tool_call_id"] = *value.tool_call_id;
  }
  if (value.reasoning_tokens.has_value()) {
    json["reasoning_tokens"] = *value.reasoning_tokens;
  }
}

void from_json(const nlohmann::json& json, Message& value) {
  value = Message{};
  value.role = role_from_string(json.value("role", std::string{"user"}));
  value.content = json.value("content", std::string{});
  value.timestamp = json.value("timestamp", std::string{});

  if (json.contains("name") && !json.at("name").is_null()) {
    value.name = json.at("name").get<std::string>();
  }
  if (json.contains("tool_call_id") && !json.at("tool_call_id").is_null()) {
    value.tool_call_id = json.at("tool_call_id").get<std::string>();
  }
  if (json.contains("tool_calls") && json.at("tool_calls").is_array()) {
    value.tool_calls = json.at("tool_calls").get<std::vector<ToolCall>>();
  }
  if (json.contains("reasoning") && json.at("reasoning").is_array()) {
    value.reasoning = json.at("reasoning").get<std::vector<ReasoningDetails>>();
  }
  if (json.contains("reasoning_tokens") && !json.at("reasoning_tokens").is_null()) {
    value.reasoning_tokens = json.at("reasoning_tokens").get<int>();
  }
}

void to_json(nlohmann::json& json, const SessionMetadata& value) {
  json = {
      {"custom", value.custom},
  };

  if (value.agent_name.has_value()) {
    json["agent_name"] = *value.agent_name;
  }
  if (value.model.has_value()) {
    json["model"] = *value.model;
  }
  if (value.prompt_hash.has_value()) {
    json["prompt_hash"] = *value.prompt_hash;
  }
  if (value.prompt_snapshot.has_value()) {
    json["prompt_snapshot"] = *value.prompt_snapshot;
  }
  if (value.toolset_hash.has_value()) {
    json["toolset_hash"] = *value.toolset_hash;
  }
  if (value.updated_at.has_value()) {
    json["updated_at"] = *value.updated_at;
  }
}

void from_json(const nlohmann::json& json, SessionMetadata& value) {
  value = SessionMetadata{};
  value.custom = json.contains("custom") ? json.at("custom") : nlohmann::json::object();

  if (json.contains("agent_name") && !json.at("agent_name").is_null()) {
    value.agent_name = json.at("agent_name").get<std::string>();
  }
  if (json.contains("model") && !json.at("model").is_null()) {
    value.model = json.at("model").get<std::string>();
  }
  if (json.contains("prompt_hash") && !json.at("prompt_hash").is_null()) {
    value.prompt_hash = json.at("prompt_hash").get<std::string>();
  }
  if (json.contains("prompt_snapshot") && !json.at("prompt_snapshot").is_null()) {
    value.prompt_snapshot = json.at("prompt_snapshot").get<std::string>();
  }
  if (json.contains("toolset_hash") && !json.at("toolset_hash").is_null()) {
    value.toolset_hash = json.at("toolset_hash").get<std::string>();
  }
  if (json.contains("updated_at") && !json.at("updated_at").is_null()) {
    value.updated_at = json.at("updated_at").get<std::string>();
  }
}

void to_json(nlohmann::json& json, const RunRecord& value) {
  json = {
      {"session_id", value.session_id},
      {"status", run_status_to_string(value.status)},
  };
}

void from_json(const nlohmann::json& json, RunRecord& value) {
  value = RunRecord{};
  value.session_id = json.value("session_id", std::string{});
  value.status = run_status_from_string(json.value("status", std::string{"completed"}));
}

}  // namespace tiny_hyper_router

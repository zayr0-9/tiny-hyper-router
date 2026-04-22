#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace tiny_hyper_router {

enum class Role {
  System,
  User,
  Assistant,
  Tool,
};

enum class RunStatus {
  Completed,
  NeedsTool,
  WaitingForUser,
  MaxStepsReached,
  Failed,
  WaitingForPermission,
};

enum class PauseReason {
  WaitingForUser,
  WaitingForPermission,
  WaitingForTool,
};

enum class StopReason {
  Completed,
  MaxStepsReached,
  ProviderError,
  ToolFailed,
  PermissionDenied,
  ToolCalls,
};

enum class ToolCallStatus {
  Pending,
  Running,
  Completed,
  Failed,
  Cancelled,
};

inline constexpr std::string_view to_string(Role value) noexcept {
  switch (value) {
    case Role::System:
      return "system";
    case Role::User:
      return "user";
    case Role::Assistant:
      return "assistant";
    case Role::Tool:
      return "tool";
  }

  return "unknown";
}

inline constexpr std::string_view to_string(RunStatus value) noexcept {
  switch (value) {
    case RunStatus::Completed:
      return "completed";
    case RunStatus::NeedsTool:
      return "needs_tool";
    case RunStatus::WaitingForUser:
      return "waiting_for_user";
    case RunStatus::MaxStepsReached:
      return "max_steps_reached";
    case RunStatus::Failed:
      return "failed";
    case RunStatus::WaitingForPermission:
      return "waiting_for_permission";
  }

  return "unknown";
}

inline constexpr std::string_view to_string(PauseReason value) noexcept {
  switch (value) {
    case PauseReason::WaitingForUser:
      return "waiting_for_user";
    case PauseReason::WaitingForPermission:
      return "waiting_for_permission";
    case PauseReason::WaitingForTool:
      return "waiting_for_tool";
  }

  return "unknown";
}

inline constexpr std::string_view to_string(StopReason value) noexcept {
  switch (value) {
    case StopReason::Completed:
      return "completed";
    case StopReason::MaxStepsReached:
      return "max_steps_reached";
    case StopReason::ProviderError:
      return "provider_error";
    case StopReason::ToolFailed:
      return "tool_failed";
    case StopReason::PermissionDenied:
      return "permission_denied";
    case StopReason::ToolCalls:
      return "tool_calls";
  }

  return "unknown";
}

inline constexpr std::string_view to_string(ToolCallStatus value) noexcept {
  switch (value) {
    case ToolCallStatus::Pending:
      return "pending";
    case ToolCallStatus::Running:
      return "running";
    case ToolCallStatus::Completed:
      return "completed";
    case ToolCallStatus::Failed:
      return "failed";
    case ToolCallStatus::Cancelled:
      return "cancelled";
  }

  return "unknown";
}

struct ToolCall {
  std::optional<std::string> id;
  std::string tool_name;
  nlohmann::json args = nlohmann::json::object();
};

struct GeneratedImage {
  std::optional<std::string> data_url;
  std::optional<std::string> url;
  std::optional<std::string> mime_type;
};

struct ReasoningSummaryItem {
  std::string text;
  std::optional<std::string> type;
};

struct ReasoningDetails {
  std::optional<std::string> id;
  std::optional<std::string> encrypted_content;
  std::optional<std::string> format;
  std::optional<std::string> status;
  std::vector<ReasoningSummaryItem> summary;
  std::optional<nlohmann::json> raw_item;
};

struct Message {
  Role role = Role::User;
  std::string content;
  std::optional<std::string> name;
  std::string timestamp;
  std::optional<std::string> tool_call_id;
  std::vector<ToolCall> tool_calls;
  std::vector<ReasoningDetails> reasoning;
  std::optional<int> reasoning_tokens;
};

struct ToolResult {
  bool ok = false;
  std::optional<nlohmann::json> output;
  std::optional<std::string> error;
};

struct AgentRunInput {
  std::string session_id;
  std::string input;
  std::size_t max_steps = 5;
  bool ephemeral = false;
};

struct ModelResponse {
  std::optional<Message> message;
  std::vector<ToolCall> tool_calls;
  std::optional<StopReason> stop_reason;
  std::vector<GeneratedImage> generated_images;
  std::vector<ReasoningDetails> reasoning;
  std::optional<int> reasoning_tokens;
};

struct AgentContext {
  std::string session_id;
  std::size_t step = 0;
};

std::string current_timestamp_utc();

}  // namespace tiny_hyper_router

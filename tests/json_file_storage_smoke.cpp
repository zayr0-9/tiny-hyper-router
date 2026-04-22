#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/providers/openrouter/items.hpp"
#include "tiny_hyper_router/providers/stub/index.hpp"
#include "tiny_hyper_router/storage/json_file_storage.hpp"
#include "tiny_hyper_router/tiny_hyper_router.hpp"

namespace {

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Failed to open file for reading: " + path.string());
  }

  return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  using namespace tiny_hyper_router;

  const auto storage_dir = std::filesystem::path{"out/json-file-storage-smoke"};
  std::error_code error;
  std::filesystem::remove_all(storage_dir, error);

  auto storage = std::make_shared<JsonFileStorage>(storage_dir);

  const Message user_message{
      .role = Role::User,
      .content = "hello",
      .timestamp = "2026-01-01T00:00:00Z",
  };

  Message assistant_message{
      .role = Role::Assistant,
      .content = "hi back",
      .timestamp = "2026-01-01T00:00:01Z",
      .tool_calls = {
          ToolCall{
              .id = std::string{"call-1"},
              .tool_name = "echo",
              .args = nlohmann::json{{"text", "hello"}},
          },
      },
      .reasoning = {
          ReasoningDetails{
              .id = std::string{"rs_1"},
              .encrypted_content = std::string{"encrypted-reasoning"},
              .format = std::string{"openai-responses-v1"},
              .status = std::string{"completed"},
              .summary = {
                  ReasoningSummaryItem{
                      .text = "Think first",
                      .type = std::string{"summary_text"},
                  },
              },
              .raw_item = nlohmann::json{{"type", "reasoning"}, {"id", "rs_1"}, {"encrypted_content", "encrypted-reasoning"}},
          },
      },
      .reasoning_tokens = 42,
  };

  const Message tool_message{
      .role = Role::Tool,
      .content = R"({"ok":true,"output":{"value":"done"}})",
      .name = std::string{"echo"},
      .timestamp = "2026-01-01T00:00:02Z",
      .tool_call_id = std::string{"call-1"},
  };

  storage->save_messages("session/1", {
      Message{
          .role = Role::System,
          .content = "should not persist",
          .timestamp = "2026-01-01T00:00:00Z",
      },
      user_message,
      assistant_message,
      tool_message,
  });

  SessionMetadata metadata;
  metadata.agent_name = "agent-a";
  metadata.model = "openai/o4-mini";
  metadata.prompt_hash = "prompt-hash";
  metadata.prompt_snapshot = "You are helpful.";
  metadata.toolset_hash = "toolset-hash";
  metadata.updated_at = "2026-01-01T00:00:03Z";
  metadata.custom = nlohmann::json{{"provider", "openrouter"}};
  storage->set_session_metadata("session/1", metadata);

  storage->save_run(RunRecord{
      .session_id = "session/1",
      .status = RunStatus::Completed,
  });

  const auto messages = storage->load_messages("session/1");
  if (messages.size() != 3) {
    std::cerr << "Expected 3 persisted non-system messages.\n";
    return EXIT_FAILURE;
  }

  if (messages[1].reasoning.size() != 1 || !messages[1].reasoning.front().raw_item.has_value()) {
    std::cerr << "Expected persisted reasoning raw_item.\n";
    return EXIT_FAILURE;
  }

  const auto replay_input = to_openrouter_input_items(messages);
  bool has_reasoning_replay = false;
  for (const auto& item : replay_input) {
    if (item.is_object() && item.value("type", std::string{}) == "reasoning" &&
        item.value("encrypted_content", std::string{}) == "encrypted-reasoning") {
      has_reasoning_replay = true;
      break;
    }
  }

  if (!has_reasoning_replay) {
    std::cerr << "Expected reasoning replay item after JSON round-trip.\n";
    return EXIT_FAILURE;
  }

  const auto loaded_metadata = storage->get_session_metadata("session/1");
  if (!loaded_metadata.has_value() || !loaded_metadata->agent_name.has_value() || *loaded_metadata->agent_name != "agent-a") {
    std::cerr << "Expected persisted session metadata.\n";
    return EXIT_FAILURE;
  }

  const auto session_file = storage_dir / "session_1.json";
  if (!std::filesystem::exists(session_file)) {
    std::cerr << "Expected JSON session file to exist.\n";
    return EXIT_FAILURE;
  }

  const auto json = nlohmann::json::parse(read_text_file(session_file), nullptr, false);

  if (json.is_discarded() || json.value("schema_version", 0) != 1) {
    std::cerr << "Expected valid schema_version in session file.\n";
    return EXIT_FAILURE;
  }

  if (!json.contains("last_run") || json.at("last_run").value("status", std::string{}) != "completed") {
    std::cerr << "Expected last_run in session file.\n";
    return EXIT_FAILURE;
  }

  AgentDefinition agent{
      .name = "test-agent",
      .instructions = "You are helpful.",
      .model = "stub-model",
  };

  auto provider = std::make_shared<StubProvider>();
  auto runtime_storage = std::make_shared<JsonFileStorage>(storage_dir / "runtime");
  AgentRuntime runtime(RuntimeConfig{
      .agent = agent,
      .provider = provider,
      .storage = runtime_storage,
  });

  const auto result = runtime.run(AgentRunInput{
      .session_id = "runtime-session",
      .input = "ping",
  });

  if (result.status != RunStatus::Completed) {
    std::cerr << "Expected runtime completed status.\n";
    return EXIT_FAILURE;
  }

  const auto runtime_messages = runtime_storage->load_messages("runtime-session");
  if (runtime_messages.size() < 2) {
    std::cerr << "Expected runtime transcript to persist in JSON storage.\n";
    return EXIT_FAILURE;
  }

  std::cout << "json_file_storage_smoke passed\n";
  return EXIT_SUCCESS;
}

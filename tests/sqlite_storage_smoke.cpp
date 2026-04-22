#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include <sqlite3.h>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/providers/openrouter/items.hpp"
#include "tiny_hyper_router/providers/stub/index.hpp"
#include "tiny_hyper_router/storage/sqlite_storage.hpp"
#include "tiny_hyper_router/tiny_hyper_router.hpp"

int main() {
  using namespace tiny_hyper_router;

  const auto database_path = std::filesystem::path{"out/sqlite-storage-smoke/storage.db"};
  std::error_code error;
  std::filesystem::remove_all(database_path.parent_path(), error);

  auto storage = std::make_shared<SqliteStorage>(database_path);

  const Message user_message{
      .role = Role::User,
      .content = "hello",
      .timestamp = "2026-01-01T00:00:00Z",
  };

  const Message assistant_message{
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
    std::cerr << "Expected reasoning replay item after SQLite round-trip.\n";
    return EXIT_FAILURE;
  }

  const auto loaded_metadata = storage->get_session_metadata("session/1");
  if (!loaded_metadata.has_value() || !loaded_metadata->agent_name.has_value() || *loaded_metadata->agent_name != "agent-a") {
    std::cerr << "Expected persisted session metadata.\n";
    return EXIT_FAILURE;
  }

  sqlite3* database = nullptr;
  if (sqlite3_open(database_path.string().c_str(), &database) != SQLITE_OK) {
    std::cerr << "Expected SQLite database file to open.\n";
    if (database != nullptr) {
      sqlite3_close(database);
    }
    return EXIT_FAILURE;
  }

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(database, "SELECT schema_version, last_run_json FROM sessions WHERE session_id = ?1;", -1, &statement, nullptr) != SQLITE_OK) {
    std::cerr << "Expected SQLite query to prepare.\n";
    sqlite3_close(database);
    return EXIT_FAILURE;
  }

  sqlite3_bind_text(statement, 1, "session/1", -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    std::cerr << "Expected persisted SQLite row.\n";
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return EXIT_FAILURE;
  }

  if (sqlite3_column_int(statement, 0) != 1) {
    std::cerr << "Expected schema_version=1 in SQLite row.\n";
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return EXIT_FAILURE;
  }

  const auto last_run_json = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
  const auto parsed_last_run = nlohmann::json::parse(last_run_json != nullptr ? last_run_json : "", nullptr, false);
  if (parsed_last_run.is_discarded() || parsed_last_run.value("status", std::string{}) != "completed") {
    std::cerr << "Expected completed last_run in SQLite row.\n";
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return EXIT_FAILURE;
  }

  sqlite3_finalize(statement);
  sqlite3_close(database);

  AgentDefinition agent{
      .name = "test-agent",
      .instructions = "You are helpful.",
      .model = "stub-model",
  };

  auto provider = std::make_shared<StubProvider>();
  auto runtime_storage = std::make_shared<SqliteStorage>(database_path.parent_path() / "runtime.db");
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
    std::cerr << "Expected runtime transcript to persist in SQLite storage.\n";
    return EXIT_FAILURE;
  }

  std::cout << "sqlite_storage_smoke passed\n";
  return EXIT_SUCCESS;
}

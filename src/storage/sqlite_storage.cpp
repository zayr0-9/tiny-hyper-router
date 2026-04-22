#include "tiny_hyper_router/storage/sqlite_storage.hpp"

#include <filesystem>
#include <stdexcept>
#include <utility>

#include <sqlite3.h>

#include "tiny_hyper_router/storage/json_serialization.hpp"

namespace tiny_hyper_router {

namespace {

class Statement final {
 public:
  Statement(sqlite3* database, const char* sql)
      : database_(database) {
    if (sqlite3_prepare_v2(database_, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database_));
    }
  }

  ~Statement() {
    if (statement_ != nullptr) {
      sqlite3_finalize(statement_);
    }
  }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const noexcept {
    return statement_;
  }

 private:
  sqlite3* database_ = nullptr;
  sqlite3_stmt* statement_ = nullptr;
};

void exec_or_throw(sqlite3* database, const char* sql) {
  char* error_message = nullptr;
  if (sqlite3_exec(database, sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
    const std::string message = error_message != nullptr ? error_message : sqlite3_errmsg(database);
    sqlite3_free(error_message);
    throw std::runtime_error(message);
  }
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const unsigned char* text = sqlite3_column_text(statement, index);
  if (text == nullptr) {
    return {};
  }

  return std::string(reinterpret_cast<const char*>(text));
}

std::optional<std::string> column_optional_text(sqlite3_stmt* statement, int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
    return std::nullopt;
  }

  return column_text(statement, index);
}

template <typename T>
std::string json_dump(const T& value) {
  return nlohmann::json(value).dump();
}

}  // namespace

SqliteStorage::SqliteStorage(std::filesystem::path database_path)
    : database_path_(std::move(database_path)) {
  if (database_path_.empty()) {
    throw std::invalid_argument("SqliteStorage database path must not be empty.");
  }

  const auto parent_directory = database_path_.parent_path();
  if (!parent_directory.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent_directory, error);
    if (error) {
      throw std::runtime_error("Failed to create SQLite directory: " + parent_directory.string());
    }
  }

  open_database();
  ensure_schema();
}

SqliteStorage::~SqliteStorage() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (database_ != nullptr) {
    sqlite3_close_v2(database_);
    database_ = nullptr;
  }
}

std::vector<Message> SqliteStorage::load_messages(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return load_session_row_locked(session_id).messages;
}

void SqliteStorage::save_messages(const std::string& session_id, const std::vector<Message>& messages) {
  std::lock_guard<std::mutex> lock(mutex_);
  upsert_session_row_locked(session_id, filter_persisted_messages(messages), std::nullopt, std::nullopt);
}

void SqliteStorage::save_run(const RunRecord& record) {
  std::lock_guard<std::mutex> lock(mutex_);
  upsert_session_row_locked(record.session_id, std::nullopt, std::nullopt, record);
}

std::optional<SessionMetadata> SqliteStorage::get_session_metadata(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return load_session_row_locked(session_id).metadata;
}

void SqliteStorage::set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) {
  std::lock_guard<std::mutex> lock(mutex_);
  upsert_session_row_locked(session_id, std::nullopt, metadata, std::nullopt);
}

void SqliteStorage::open_database() {
  if (sqlite3_open(database_path_.string().c_str(), &database_) != SQLITE_OK) {
    const std::string message = database_ != nullptr ? sqlite3_errmsg(database_) : "Unknown SQLite open error";
    if (database_ != nullptr) {
      sqlite3_close(database_);
      database_ = nullptr;
    }
    throw std::runtime_error("Failed to open SQLite database: " + message);
  }

  exec_or_throw(database_, "PRAGMA journal_mode=WAL;");
  exec_or_throw(database_, "PRAGMA foreign_keys=ON;");
}

void SqliteStorage::ensure_schema() {
  exec_or_throw(
      database_,
      "CREATE TABLE IF NOT EXISTS sessions ("
      "session_id TEXT PRIMARY KEY,"
      "schema_version INTEGER NOT NULL DEFAULT 1,"
      "messages_json TEXT NOT NULL DEFAULT '[]',"
      "metadata_json TEXT NULL,"
      "last_run_json TEXT NULL,"
      "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
      ");");
}

SqliteStorage::SessionRow SqliteStorage::load_session_row_locked(const std::string& session_id) const {
  Statement statement(
      database_,
      "SELECT messages_json, metadata_json, last_run_json FROM sessions WHERE session_id = ?1;");

  if (sqlite3_bind_text(statement.get(), 1, session_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }

  const int step_result = sqlite3_step(statement.get());
  if (step_result == SQLITE_DONE) {
    return SessionRow{};
  }
  if (step_result != SQLITE_ROW) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }

  SessionRow row;

  const auto messages_json = nlohmann::json::parse(column_text(statement.get(), 0), nullptr, false);
  if (messages_json.is_discarded() || !messages_json.is_array()) {
    throw std::runtime_error("Invalid messages_json in SQLite session row.");
  }
  row.messages = messages_json.get<std::vector<Message>>();

  if (const auto metadata_text = column_optional_text(statement.get(), 1); metadata_text.has_value()) {
    const auto metadata_json = nlohmann::json::parse(*metadata_text, nullptr, false);
    if (metadata_json.is_discarded() || !metadata_json.is_object()) {
      throw std::runtime_error("Invalid metadata_json in SQLite session row.");
    }
    row.metadata = metadata_json.get<SessionMetadata>();
  }

  if (const auto last_run_text = column_optional_text(statement.get(), 2); last_run_text.has_value()) {
    const auto last_run_json = nlohmann::json::parse(*last_run_text, nullptr, false);
    if (last_run_json.is_discarded() || !last_run_json.is_object()) {
      throw std::runtime_error("Invalid last_run_json in SQLite session row.");
    }
    row.last_run = last_run_json.get<RunRecord>();
  }

  return row;
}

void SqliteStorage::upsert_session_row_locked(const std::string& session_id,
                                              const std::optional<std::vector<Message>>& messages,
                                              const std::optional<SessionMetadata>& metadata,
                                              const std::optional<RunRecord>& last_run) const {
  SessionRow existing;
  if (!messages.has_value() || !metadata.has_value() || !last_run.has_value()) {
    existing = load_session_row_locked(session_id);
  }

  const auto final_messages = messages.has_value() ? *messages : existing.messages;
  const auto final_metadata = metadata.has_value() ? metadata : existing.metadata;
  const auto final_last_run = last_run.has_value() ? last_run : existing.last_run;

  Statement statement(
      database_,
      "INSERT INTO sessions (session_id, schema_version, messages_json, metadata_json, last_run_json, updated_at) "
      "VALUES (?1, 1, ?2, ?3, ?4, CURRENT_TIMESTAMP) "
      "ON CONFLICT(session_id) DO UPDATE SET "
      "messages_json = excluded.messages_json, "
      "metadata_json = excluded.metadata_json, "
      "last_run_json = excluded.last_run_json, "
      "updated_at = CURRENT_TIMESTAMP;");

  const auto messages_payload = json_dump(final_messages);
  const auto metadata_payload = final_metadata.has_value() ? std::optional<std::string>{json_dump(*final_metadata)} : std::nullopt;
  const auto last_run_payload = final_last_run.has_value() ? std::optional<std::string>{json_dump(*final_last_run)} : std::nullopt;

  if (sqlite3_bind_text(statement.get(), 1, session_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_bind_text(statement.get(), 2, messages_payload.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }

  if (metadata_payload.has_value()) {
    if (sqlite3_bind_text(statement.get(), 3, metadata_payload->c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database_));
    }
  } else if (sqlite3_bind_null(statement.get(), 3) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }

  if (last_run_payload.has_value()) {
    if (sqlite3_bind_text(statement.get(), 4, last_run_payload->c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database_));
    }
  } else if (sqlite3_bind_null(statement.get(), 4) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }
}

std::vector<Message> SqliteStorage::filter_persisted_messages(const std::vector<Message>& messages) {
  std::vector<Message> filtered;
  filtered.reserve(messages.size());

  for (const auto& message : messages) {
    if (message.role != Role::System) {
      filtered.push_back(message);
    }
  }

  return filtered;
}

}  // namespace tiny_hyper_router

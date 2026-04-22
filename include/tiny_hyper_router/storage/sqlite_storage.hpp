#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

#include "tiny_hyper_router/storage/types.hpp"

namespace tiny_hyper_router {

class SqliteStorage final : public StorageAdapter {
 public:
  explicit SqliteStorage(std::filesystem::path database_path);
  ~SqliteStorage() override;

  SqliteStorage(const SqliteStorage&) = delete;
  SqliteStorage& operator=(const SqliteStorage&) = delete;

  SqliteStorage(SqliteStorage&&) = delete;
  SqliteStorage& operator=(SqliteStorage&&) = delete;

  std::vector<Message> load_messages(const std::string& session_id) override;
  void save_messages(const std::string& session_id, const std::vector<Message>& messages) override;
  void save_run(const RunRecord& record) override;
  std::optional<SessionMetadata> get_session_metadata(const std::string& session_id) override;
  void set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) override;

 private:
  struct SessionRow {
    std::vector<Message> messages;
    std::optional<SessionMetadata> metadata;
    std::optional<RunRecord> last_run;
  };

  void open_database();
  void ensure_schema();
  SessionRow load_session_row_locked(const std::string& session_id) const;
  void upsert_session_row_locked(const std::string& session_id,
                                 const std::optional<std::vector<Message>>& messages,
                                 const std::optional<SessionMetadata>& metadata,
                                 const std::optional<RunRecord>& last_run) const;
  static std::vector<Message> filter_persisted_messages(const std::vector<Message>& messages);

  std::filesystem::path database_path_;
  sqlite3* database_ = nullptr;
  mutable std::mutex mutex_;
};

}  // namespace tiny_hyper_router

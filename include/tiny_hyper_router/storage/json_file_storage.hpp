#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "tiny_hyper_router/storage/types.hpp"

namespace tiny_hyper_router {

class JsonFileStorage final : public StorageAdapter {
 public:
  explicit JsonFileStorage(std::filesystem::path root_directory);

  std::vector<Message> load_messages(const std::string& session_id) override;
  void save_messages(const std::string& session_id, const std::vector<Message>& messages) override;
  void save_run(const RunRecord& record) override;
  std::optional<SessionMetadata> get_session_metadata(const std::string& session_id) override;
  void set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) override;

 private:
  struct SessionDocument {
    std::string session_id;
    std::vector<Message> messages;
    std::optional<SessionMetadata> metadata;
    std::optional<RunRecord> last_run;
  };

  SessionDocument load_session_document_locked(const std::string& session_id) const;
  void save_session_document_locked(const SessionDocument& document) const;
  std::filesystem::path session_file_path(const std::string& session_id) const;
  static std::string sanitize_session_id(const std::string& session_id);

  std::filesystem::path root_directory_;
  mutable std::mutex mutex_;
};

}  // namespace tiny_hyper_router

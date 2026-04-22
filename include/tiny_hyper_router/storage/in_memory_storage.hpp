#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "tiny_hyper_router/storage/types.hpp"

namespace tiny_hyper_router {

class InMemoryStorage final : public StorageAdapter {
 public:
  std::vector<Message> load_messages(const std::string& session_id) override;
  void save_messages(const std::string& session_id, const std::vector<Message>& messages) override;
  void save_run(const RunRecord& record) override;
  std::optional<SessionMetadata> get_session_metadata(const std::string& session_id) override;
  void set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) override;

 private:
  std::unordered_map<std::string, std::vector<Message>> messages_;
  std::unordered_map<std::string, RunRecord> runs_;
  std::unordered_map<std::string, SessionMetadata> session_metadata_;
};

}  // namespace tiny_hyper_router

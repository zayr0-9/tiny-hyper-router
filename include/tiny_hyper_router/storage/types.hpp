#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/core/types.hpp"

namespace tiny_hyper_router {

struct StandardSessionMetadata {
  std::optional<std::string> agent_name;
  std::optional<std::string> model;
  std::optional<std::string> prompt_hash;
  std::optional<std::string> prompt_snapshot;
  std::optional<std::string> toolset_hash;
  std::optional<std::string> updated_at;
};

struct SessionMetadata : StandardSessionMetadata {
  nlohmann::json custom = nlohmann::json::object();
};

struct RunRecord {
  std::string session_id;
  RunStatus status = RunStatus::Completed;
};

class StorageAdapter {
 public:
  virtual ~StorageAdapter() = default;

  virtual std::vector<Message> load_messages(const std::string& session_id) = 0;
  virtual void save_messages(const std::string& session_id, const std::vector<Message>& messages) = 0;
  virtual void save_run(const RunRecord& record) = 0;
  virtual std::optional<SessionMetadata> get_session_metadata(const std::string& session_id) = 0;
  virtual void set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) = 0;
};

}  // namespace tiny_hyper_router

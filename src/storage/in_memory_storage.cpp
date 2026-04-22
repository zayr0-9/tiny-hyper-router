#include "tiny_hyper_router/storage/in_memory_storage.hpp"

#include <algorithm>

namespace tiny_hyper_router {

std::vector<Message> InMemoryStorage::load_messages(const std::string& session_id) {
  const auto it = messages_.find(session_id);
  if (it == messages_.end()) {
    return {};
  }

  return it->second;
}

void InMemoryStorage::save_messages(const std::string& session_id, const std::vector<Message>& messages) {
  std::vector<Message> filtered;
  filtered.reserve(messages.size());

  for (const auto& message : messages) {
    if (message.role != Role::System) {
      filtered.push_back(message);
    }
  }

  messages_[session_id] = std::move(filtered);
}

void InMemoryStorage::save_run(const RunRecord& record) {
  runs_[record.session_id] = record;
}

std::optional<SessionMetadata> InMemoryStorage::get_session_metadata(const std::string& session_id) {
  const auto it = session_metadata_.find(session_id);
  if (it == session_metadata_.end()) {
    return std::nullopt;
  }

  return it->second;
}

void InMemoryStorage::set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) {
  session_metadata_[session_id] = metadata;
}

}  // namespace tiny_hyper_router

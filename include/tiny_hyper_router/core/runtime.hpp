#pragma once

#include <memory>
#include <string>
#include <vector>

#include "tiny_hyper_router/core/agent.hpp"
#include "tiny_hyper_router/core/providers.hpp"
#include "tiny_hyper_router/core/storage.hpp"

namespace tiny_hyper_router {

struct RuntimeConfig {
  AgentDefinition agent;
  std::shared_ptr<ModelProvider> provider;
  std::shared_ptr<StorageAdapter> storage;
};

struct RuntimeResult {
  RunStatus status = RunStatus::Completed;
  std::vector<Message> messages;
};

class AgentRuntime {
 public:
  explicit AgentRuntime(RuntimeConfig config);

  RuntimeResult run(const AgentRunInput& input);

 private:
  void update_session_metadata(const std::string& session_id);
  std::string hash_value(const std::string& value) const;
  std::vector<Message> execute_tool_calls(
      const std::string& session_id,
      std::size_t step,
      const std::vector<ToolCall>& tool_calls) const;

  RuntimeConfig config_;
};

}  // namespace tiny_hyper_router

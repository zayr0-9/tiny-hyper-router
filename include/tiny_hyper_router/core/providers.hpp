#pragma once

#include <optional>
#include <string>
#include <vector>

#include "tiny_hyper_router/core/tool.hpp"
#include "tiny_hyper_router/core/types.hpp"
#include "tiny_hyper_router/storage/types.hpp"

namespace tiny_hyper_router {

struct GenerateRequest {
  std::optional<std::string> session_id;
  std::string model;
  std::vector<Message> messages;
  std::vector<ToolDefinition> tools;
  std::optional<SessionMetadata> previous_session_metadata;
  bool ephemeral = false;
};

class ModelProvider {
 public:
  virtual ~ModelProvider() = default;

  virtual ModelResponse generate(const GenerateRequest& request) = 0;
};

}  // namespace tiny_hyper_router

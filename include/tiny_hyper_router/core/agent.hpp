#pragma once

#include <functional>
#include <string>
#include <vector>

#include "tiny_hyper_router/core/tool.hpp"

namespace tiny_hyper_router {

using BuildMessagesFunction = std::function<std::vector<Message>(const std::string& input)>;

struct AgentDefinition {
  std::string name;
  std::string instructions;
  std::string model;
  std::vector<ToolDefinition> tools;
  BuildMessagesFunction build_messages;
};

}  // namespace tiny_hyper_router

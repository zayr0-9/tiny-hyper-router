#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/core/types.hpp"

namespace tiny_hyper_router {

using ToolExecuteFunction = std::function<ToolResult(const nlohmann::json&, const AgentContext&)>;

struct ToolDefinition {
  std::string name;
  std::string description;
  nlohmann::json input_schema = nlohmann::json::object();
  ToolExecuteFunction execute;

  [[nodiscard]] bool can_execute() const noexcept {
    return static_cast<bool>(execute);
  }
};

}  // namespace tiny_hyper_router

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "tiny_hyper_router/core/tool.hpp"
#include "tiny_hyper_router/core/types.hpp"
#include "tiny_hyper_router/providers/openrouter/types.hpp"

namespace tiny_hyper_router {

std::vector<OpenRouterInputItem> to_openrouter_input_items(const std::vector<Message>& messages);
OpenRouterInputItem to_openrouter_reasoning_item(const ReasoningDetails& reasoning);
std::vector<OpenRouterToolDefinition> to_openrouter_tool_definitions(const std::vector<ToolDefinition>& tools);
std::string to_openrouter_message_item_id(const Message& message, std::size_t index);

}  // namespace tiny_hyper_router

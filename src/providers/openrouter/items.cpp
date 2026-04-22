#include "tiny_hyper_router/providers/openrouter/items.hpp"

namespace tiny_hyper_router {

OpenRouterInputItem to_openrouter_reasoning_item(const ReasoningDetails& reasoning) {
  if (reasoning.raw_item.has_value() && reasoning.raw_item->is_object()) {
    return *reasoning.raw_item;
  }

  OpenRouterInputItem item = {
      {"type", "reasoning"},
  };

  if (reasoning.id.has_value() && !reasoning.id->empty()) {
    item["id"] = *reasoning.id;
  }

  if (reasoning.encrypted_content.has_value() && !reasoning.encrypted_content->empty()) {
    item["encrypted_content"] = *reasoning.encrypted_content;
  }

  if (reasoning.format.has_value() && !reasoning.format->empty()) {
    item["format"] = *reasoning.format;
  }

  if (reasoning.status.has_value() && !reasoning.status->empty()) {
    item["status"] = *reasoning.status;
  }

  if (!reasoning.summary.empty()) {
    item["summary"] = nlohmann::json::array();
    for (const auto& summary_item : reasoning.summary) {
      nlohmann::json json_item = {
          {"text", summary_item.text},
      };

      if (summary_item.type.has_value() && !summary_item.type->empty()) {
        json_item["type"] = *summary_item.type;
      }

      item["summary"].push_back(std::move(json_item));
    }
  }

  return item;
}

std::vector<OpenRouterInputItem> to_openrouter_input_items(const std::vector<Message>& messages) {
  std::vector<OpenRouterInputItem> items;
  items.reserve(messages.size());

  for (std::size_t index = 0; index < messages.size(); ++index) {
    const auto& message = messages[index];

    if (message.role == Role::Tool) {
      items.push_back({
          {"type", "function_call_output"},
          {"call_id", message.tool_call_id.value_or(message.name.value_or("tool") + std::string{"-result"})},
          {"output", message.content},
      });
      continue;
    }

    if (message.role == Role::Assistant) {
      for (const auto& reasoning : message.reasoning) {
        items.push_back(to_openrouter_reasoning_item(reasoning));
      }

      if (!message.content.empty()) {
        items.push_back({
            {"id", to_openrouter_message_item_id(message, index)},
            {"type", "message"},
            {"role", "assistant"},
            {"status", "completed"},
            {"content", nlohmann::json::array({
                {
                    {"type", "output_text"},
                    {"text", message.content},
                    {"annotations", nlohmann::json::array()},
                },
            })},
        });
      }

      for (const auto& tool_call : message.tool_calls) {
        items.push_back({
            {"type", "function_call"},
            {"call_id", tool_call.id.value_or(tool_call.tool_name + std::string{"-call"})},
            {"name", tool_call.tool_name},
            {"arguments", tool_call.args.dump()},
        });
      }

      continue;
    }

    if (message.role == Role::System) {
      items.push_back({
          {"id", to_openrouter_message_item_id(message, index)},
          {"type", "message"},
          {"role", "system"},
          {"content", nlohmann::json::array({
              {
                  {"type", "input_text"},
                  {"text", message.content},
              },
          })},
      });
      continue;
    }

    items.push_back({
        {"type", "message"},
        {"role", "user"},
        {"content", nlohmann::json::array({
            {
                {"type", "input_text"},
                {"text", message.content},
            },
        })},
    });
  }

  return items;
}

std::vector<OpenRouterToolDefinition> to_openrouter_tool_definitions(const std::vector<ToolDefinition>& tools) {
  std::vector<OpenRouterToolDefinition> definitions;
  definitions.reserve(tools.size());

  for (const auto& tool : tools) {
    definitions.push_back({
        {"type", "function"},
        {"name", tool.name},
        {"description", tool.description},
        {"parameters", tool.input_schema.is_object() ? tool.input_schema : nlohmann::json::object()},
    });
  }

  return definitions;
}

std::string to_openrouter_message_item_id(const Message& message, std::size_t index) {
  return std::string(to_string(message.role)) + "-" + message.timestamp + "-" + std::to_string(index);
}

}  // namespace tiny_hyper_router

#include "tiny_hyper_router/providers/stub/provider.hpp"

#include <algorithm>

namespace tiny_hyper_router {

ModelResponse StubProvider::generate(const GenerateRequest& request) {
  auto last_user_message = std::find_if(
      request.messages.rbegin(),
      request.messages.rend(),
      [](const Message& message) {
        return message.role == Role::User;
      });

  std::string content = "Stub response from " + request.model + ": ";
  if (last_user_message != request.messages.rend()) {
    content += last_user_message->content;
  }

  return ModelResponse{
      .message = Message{
          .role = Role::Assistant,
          .content = std::move(content),
          .timestamp = current_timestamp_utc(),
          .tool_calls = {},
          .reasoning = {},
          .reasoning_tokens = std::nullopt,
      },
      .tool_calls = {},
      .stop_reason = StopReason::Completed,
      .generated_images = {},
      .reasoning = {},
      .reasoning_tokens = std::nullopt,
  };
}

}  // namespace tiny_hyper_router

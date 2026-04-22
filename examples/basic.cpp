#include <iostream>
#include <memory>

#include "tiny_hyper_router/providers/stub/index.hpp"
#include "tiny_hyper_router/storage/in_memory_storage.hpp"
#include "tiny_hyper_router/tiny_hyper_router.hpp"

int main() {
  using namespace tiny_hyper_router;

  AgentDefinition agent{
      .name = "example-agent",
      .instructions = "You are helpful.",
      .model = "stub-model",
  };

  auto provider = std::make_shared<StubProvider>();
  auto storage = std::make_shared<InMemoryStorage>();

  AgentRuntime runtime(RuntimeConfig{
      .agent = agent,
      .provider = provider,
      .storage = storage,
  });

  const auto result = runtime.run(AgentRunInput{
      .session_id = "demo-session",
      .input = "Hello from tiny-hyper-router!",
  });

  std::cout << "Status: " << to_string(result.status) << "\n";
  if (!result.messages.empty()) {
    const auto& last = result.messages.back();
    std::cout << "Last message role: " << to_string(last.role) << "\n";
    std::cout << "Last message content: " << last.content << "\n";
  }

  return 0;
}

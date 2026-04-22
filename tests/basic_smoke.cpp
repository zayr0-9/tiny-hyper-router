#include <cstdlib>
#include <iostream>
#include <memory>

#include "tiny_hyper_router/providers/stub/index.hpp"
#include "tiny_hyper_router/storage/in_memory_storage.hpp"
#include "tiny_hyper_router/tiny_hyper_router.hpp"

int main() {
  using namespace tiny_hyper_router;

  AgentDefinition agent{
      .name = "test-agent",
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
      .session_id = "test-session",
      .input = "ping",
  });

  if (result.status != RunStatus::Completed) {
    std::cerr << "Expected completed status.\n";
    return EXIT_FAILURE;
  }

  if (result.messages.empty()) {
    std::cerr << "Expected messages in runtime result.\n";
    return EXIT_FAILURE;
  }

  const auto& last = result.messages.back();
  if (last.role != Role::Assistant) {
    std::cerr << "Expected last message role to be assistant.\n";
    return EXIT_FAILURE;
  }

  if (last.content.find("Stub response from stub-model: ping") == std::string::npos) {
    std::cerr << "Unexpected assistant content: " << last.content << "\n";
    return EXIT_FAILURE;
  }

  const auto saved_messages = storage->load_messages("test-session");
  if (saved_messages.size() < 2) {
    std::cerr << "Expected saved transcript messages.\n";
    return EXIT_FAILURE;
  }

  std::cout << "basic_smoke passed\n";
  return EXIT_SUCCESS;
}

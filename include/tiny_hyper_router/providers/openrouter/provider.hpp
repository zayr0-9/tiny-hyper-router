#pragma once

#include <memory>
#include <vector>

#include "tiny_hyper_router/core/providers.hpp"
#include "tiny_hyper_router/http/client.hpp"
#include "tiny_hyper_router/providers/openrouter/items.hpp"
#include "tiny_hyper_router/providers/openrouter/types.hpp"

namespace tiny_hyper_router {

class OpenRouterProvider final : public ModelProvider {
 public:
  explicit OpenRouterProvider(OpenRouterProviderOptions options = {});

  ModelResponse generate(const GenerateRequest& request) override;

  [[nodiscard]] OpenRouterRequestBody build_request_body(const GenerateRequest& request) const;
  [[nodiscard]] ModelResponse parse_response_body(const OpenRouterResponseBody& response_body) const;
  [[nodiscard]] const OpenRouterProviderOptions& options() const noexcept;

 protected:
  [[nodiscard]] std::vector<ToolCall> normalize_tool_calls(const OpenRouterResponseBody& response_body) const;
  [[nodiscard]] std::vector<GeneratedImage> extract_generated_images(const OpenRouterResponseBody& response_body) const;

 private:
  OpenRouterProviderOptions options_;
  std::shared_ptr<HttpClient> http_client_;
};

}  // namespace tiny_hyper_router

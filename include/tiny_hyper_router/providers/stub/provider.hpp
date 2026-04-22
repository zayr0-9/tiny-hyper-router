#pragma once

#include "tiny_hyper_router/core/providers.hpp"

namespace tiny_hyper_router {

class StubProvider final : public ModelProvider {
 public:
  ModelResponse generate(const GenerateRequest& request) override;
};

}  // namespace tiny_hyper_router

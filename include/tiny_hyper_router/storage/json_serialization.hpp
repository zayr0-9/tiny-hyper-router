#pragma once

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/core/runtime.hpp"
#include "tiny_hyper_router/storage/types.hpp"

namespace tiny_hyper_router {

void to_json(nlohmann::json& json, const ToolCall& value);
void from_json(const nlohmann::json& json, ToolCall& value);

void to_json(nlohmann::json& json, const ReasoningSummaryItem& value);
void from_json(const nlohmann::json& json, ReasoningSummaryItem& value);

void to_json(nlohmann::json& json, const ReasoningDetails& value);
void from_json(const nlohmann::json& json, ReasoningDetails& value);

void to_json(nlohmann::json& json, const Message& value);
void from_json(const nlohmann::json& json, Message& value);

void to_json(nlohmann::json& json, const SessionMetadata& value);
void from_json(const nlohmann::json& json, SessionMetadata& value);

void to_json(nlohmann::json& json, const RunRecord& value);
void from_json(const nlohmann::json& json, RunRecord& value);

void to_json(nlohmann::json& json, const RuntimeResult& value);
void from_json(const nlohmann::json& json, RuntimeResult& value);

}  // namespace tiny_hyper_router

#include "tiny_hyper_router/storage/json_file_storage.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "tiny_hyper_router/storage/json_serialization.hpp"

namespace tiny_hyper_router {

namespace {

constexpr int kSchemaVersion = 1;

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Failed to open file for reading: " + path.string());
  }

  return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

void write_text_file_atomic(const std::filesystem::path& path, const std::string& content) {
  const auto temp_path = path.string() + ".tmp";
  {
    std::ofstream stream(temp_path, std::ios::binary | std::ios::trunc);
    if (!stream) {
      throw std::runtime_error("Failed to open file for writing: " + temp_path);
    }

    stream << content;
    if (!stream.good()) {
      throw std::runtime_error("Failed to write file: " + temp_path);
    }
  }

  std::error_code error;
  std::filesystem::rename(temp_path, path, error);
  if (!error) {
    return;
  }

  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temp_path, path, error);
  if (error) {
    throw std::runtime_error("Failed to replace file: " + path.string());
  }
}

std::vector<Message> filter_persisted_messages(const std::vector<Message>& messages) {
  std::vector<Message> filtered;
  filtered.reserve(messages.size());

  for (const auto& message : messages) {
    if (message.role != Role::System) {
      filtered.push_back(message);
    }
  }

  return filtered;
}

}  // namespace

JsonFileStorage::JsonFileStorage(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {
  if (root_directory_.empty()) {
    throw std::invalid_argument("JsonFileStorage root directory must not be empty.");
  }

  std::error_code error;
  std::filesystem::create_directories(root_directory_, error);
  if (error) {
    throw std::runtime_error("Failed to create storage directory: " + root_directory_.string());
  }
}

std::vector<Message> JsonFileStorage::load_messages(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return load_session_document_locked(session_id).messages;
}

void JsonFileStorage::save_messages(const std::string& session_id, const std::vector<Message>& messages) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto document = load_session_document_locked(session_id);
  document.messages = filter_persisted_messages(messages);
  save_session_document_locked(document);
}

void JsonFileStorage::save_run(const RunRecord& record) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto document = load_session_document_locked(record.session_id);
  document.last_run = record;
  save_session_document_locked(document);
}

std::optional<SessionMetadata> JsonFileStorage::get_session_metadata(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return load_session_document_locked(session_id).metadata;
}

void JsonFileStorage::set_session_metadata(const std::string& session_id, const SessionMetadata& metadata) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto document = load_session_document_locked(session_id);
  document.metadata = metadata;
  save_session_document_locked(document);
}

JsonFileStorage::SessionDocument JsonFileStorage::load_session_document_locked(const std::string& session_id) const {
  SessionDocument document{
      .session_id = session_id,
      .messages = {},
      .metadata = std::nullopt,
      .last_run = std::nullopt,
  };

  const auto path = session_file_path(session_id);
  if (!std::filesystem::exists(path)) {
    return document;
  }

  const auto parsed = nlohmann::json::parse(read_text_file(path), nullptr, false);
  if (parsed.is_discarded() || !parsed.is_object()) {
    throw std::runtime_error("Invalid JSON session document: " + path.string());
  }

  if (parsed.contains("session_id") && parsed.at("session_id").is_string()) {
    document.session_id = parsed.at("session_id").get<std::string>();
  }

  if (parsed.contains("messages") && parsed.at("messages").is_array()) {
    document.messages = parsed.at("messages").get<std::vector<Message>>();
  }

  if (parsed.contains("metadata") && parsed.at("metadata").is_object()) {
    document.metadata = parsed.at("metadata").get<SessionMetadata>();
  }

  if (parsed.contains("last_run") && parsed.at("last_run").is_object()) {
    document.last_run = parsed.at("last_run").get<RunRecord>();
  }

  return document;
}

void JsonFileStorage::save_session_document_locked(const SessionDocument& document) const {
  std::error_code error;
  std::filesystem::create_directories(root_directory_, error);
  if (error) {
    throw std::runtime_error("Failed to create storage directory: " + root_directory_.string());
  }

  nlohmann::json json = {
      {"schema_version", kSchemaVersion},
      {"session_id", document.session_id},
      {"messages", document.messages},
      {"metadata", document.metadata.has_value() ? nlohmann::json(*document.metadata) : nlohmann::json(nullptr)},
      {"last_run", document.last_run.has_value() ? nlohmann::json(*document.last_run) : nlohmann::json(nullptr)},
  };

  write_text_file_atomic(session_file_path(document.session_id), json.dump(2));
}

std::filesystem::path JsonFileStorage::session_file_path(const std::string& session_id) const {
  return root_directory_ / (sanitize_session_id(session_id) + ".json");
}

std::string JsonFileStorage::sanitize_session_id(const std::string& session_id) {
  if (session_id.empty()) {
    return "session";
  }

  std::string sanitized;
  sanitized.reserve(session_id.size());

  for (const unsigned char ch : session_id) {
    if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.') {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      sanitized.push_back('_');
    }
  }

  return sanitized;
}

}  // namespace tiny_hyper_router

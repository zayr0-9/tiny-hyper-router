#include "tiny_hyper_router/core/types.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tiny_hyper_router {

std::string current_timestamp_utc() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const std::time_t time = clock::to_time_t(now);

  std::tm utc_time{};
#if defined(_WIN32)
  gmtime_s(&utc_time, &time);
#else
  gmtime_r(&time, &utc_time);
#endif

  std::ostringstream stream;
  stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

}  // namespace tiny_hyper_router

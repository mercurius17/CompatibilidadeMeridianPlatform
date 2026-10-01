#pragma once
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace MeridianBrowserPolicy {
inline constexpr std::size_t kMaxOperations = 256;
inline constexpr std::size_t kMaxBytes = 1024 * 1024;
inline constexpr std::string_view kPage = "mod://aetherius-client/index.html";

// The client has one entry point. Do not let legacy file URLs grant native
// bindings to arbitrary files, hosts, encodings, paths or remote documents.
inline std::optional<std::string> NormalizeUrl(std::string_view value)
{
  if (value.size() > 4096)
    return std::nullopt;
  const auto suffixAt = value.find_first_of("?#");
  const auto path = value.substr(0, suffixAt);
  if (path != kPage && path != "file:///Data/Platform/UI/index.html")
    return std::nullopt;
  const auto suffix = suffixAt == std::string_view::npos
    ? std::string_view{} : value.substr(suffixAt);
  for (const unsigned char c : suffix) {
    if (c < 0x20 || c == 0x7f || c == '\\' || c == '\"' || c == '\'')
      return std::nullopt;
  }
  return std::string(kPage) + std::string(suffix);
}

// Access only on the serialized SKSE task thread. No SDK calls under a mutex.
class ScriptQueue
{
public:
  void Reset(std::uint64_t generation)
  {
    currentGeneration = generation;
    bytes = 0;
    scripts.clear();
  }
  bool Push(std::uint64_t generation, std::string script)
  {
    if (generation != currentGeneration || scripts.size() >= kMaxOperations ||
        script.size() > kMaxBytes - bytes)
      return false;
    bytes += script.size();
    scripts.push_back(std::move(script));
    return true;
  }
  std::optional<std::string> Pop(std::uint64_t generation)
  {
    if (generation != currentGeneration || scripts.empty())
      return std::nullopt;
    auto script = std::move(scripts.front());
    scripts.pop_front();
    bytes -= script.size();
    return script;
  }
private:
  std::uint64_t currentGeneration = 1;
  std::size_t bytes = 0;
  std::deque<std::string> scripts;
};
}

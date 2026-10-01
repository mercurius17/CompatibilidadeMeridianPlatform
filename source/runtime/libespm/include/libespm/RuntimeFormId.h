#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace espm {

using SourceId = uint32_t;
using FileFormId = uint32_t;
using LocalFormId = uint32_t;
using RuntimeFormId = uint32_t;

enum class PluginClass : uint8_t
{
  Regular,
  Light
};

enum class RuntimeFormIdClass : uint8_t
{
  Null,
  Regular,
  Light,
  Dynamic
};

struct RuntimePlacement
{
  PluginClass kind = PluginClass::Regular;
  uint16_t slot = 0;
};

struct ResolvedFormId
{
  RuntimeFormId runtimeId = 0;
  RuntimeFormIdClass idClass = RuntimeFormIdClass::Null;
  std::optional<SourceId> source;
  std::optional<LocalFormId> localId;
};

struct MasterBinding
{
  SourceId source = 0;
  uint32_t fileIndex = 0;
  PluginClass kind = PluginClass::Regular;
};

struct SourceMetadata
{
  SourceId id = 0;
  std::string fileName;
  size_t loadOrderOrdinal = 0;
  PluginClass kind = PluginClass::Regular;
  bool isMaster = false;
  bool hasEslFlag = false;
  bool hasEslExtension = false;
  uint32_t tes4Flags = 0;
  RuntimePlacement placement;
  std::vector<MasterBinding> masters;
};

enum class ResolveErrorCode : uint8_t
{
  NotFinalized,
  UnknownSource,
  DuplicateSourceName,
  DuplicateRuntimeSlot,
  UnknownRegularSlot,
  UnknownLightSlot,
  MissingMaster,
  InvalidNamespace,
  LocalIdOutOfRange,
  MalformedFileFormId,
  DynamicIdHasNoSource
};

class ResolveError : public std::runtime_error
{
public:
  ResolveError(ResolveErrorCode code, const std::string& message)
    : std::runtime_error(message)
    , code_(code)
  {
  }

  ResolveErrorCode code() const noexcept { return code_; }

private:
  ResolveErrorCode code_;
};

} // namespace espm

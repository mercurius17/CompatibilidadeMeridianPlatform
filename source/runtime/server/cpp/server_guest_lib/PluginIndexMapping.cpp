#include "PluginIndexMapping.h"
#include "libespm/Loader.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fmt/format.h>
#include <stdexcept>

namespace {

constexpr uint32_t kTes4FlagMaster = 0x00000001;
constexpr uint32_t kTes4FlagLight = 0x00000200;
constexpr uint32_t kRegularSlotCount = 0xFE;
constexpr uint32_t kLightSlotCount = 0x1000;

bool HasExtension(const std::string& fileName, const char* extension)
{
  const size_t length = std::strlen(extension);
  if (fileName.size() < length) {
    return false;
  }
  const size_t offset = fileName.size() - length;
  for (size_t i = 0; i < length; ++i) {
    if (static_cast<char>(std::tolower(
          static_cast<unsigned char>(fileName[offset + i]))) != extension[i]) {
      return false;
    }
  }
  return true;
}

std::string StripDirectory(const std::string& fileName)
{
  const auto slash = fileName.find_last_of("\\/");
  return slash == std::string::npos ? fileName : fileName.substr(slash + 1);
}

std::string ToLower(const std::string& s)
{
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

std::string NormalizeFilename(const std::string& fileName)
{
  return ToLower(StripDirectory(fileName));
}

} // namespace

PluginKind PluginIndexMapping::DetectPluginKind(const std::string& fileName,
                                                uint32_t tes4Flags)
{
  if (HasExtension(fileName, ".esl")) {
    return PluginKind::LightEsl;
  }
  if (tes4Flags & kTes4FlagLight) {
    return PluginKind::LightEspfe;
  }
  if (HasExtension(fileName, ".esm") || (tes4Flags & kTes4FlagMaster)) {
    return PluginKind::Master;
  }
  return PluginKind::RegularEsp;
}

void PluginIndexMapping::Build(const std::vector<std::string>& files,
                               espm::Loader* loader)
{
  plugins.clear();
  pluginIndexByLowerName.clear();
  pluginIndexByRegularSlot.assign(kRegularSlotCount, std::nullopt);
  pluginIndexByLightSlot.assign(kLightSlotCount, std::nullopt);

  std::vector<uint32_t> headerFlags;
  if (loader) {
    headerFlags = loader->GetFileHeaderFlags();
  }

  uint16_t nextRegularSlot = 0;
  uint16_t nextLightSlot = 0;

  plugins.reserve(files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    const std::string baseName = StripDirectory(files[i]);
    const std::string lowerName = ToLower(baseName);

    if (pluginIndexByLowerName.count(lowerName)) {
      throw std::runtime_error(
        fmt::format("Duplicate plugin filename '{}'", baseName));
    }

    const uint32_t tes4Flags =
      (loader && i < headerFlags.size()) ? headerFlags[i] : 0;

    PluginIndexMappingEntry entry;
    entry.fileName = baseName;
    entry.kind = DetectPluginKind(baseName, tes4Flags);
    entry.isLight =
      entry.kind == PluginKind::LightEsl || entry.kind == PluginKind::LightEspfe;
    entry.espmFileIndex = i;

    if (entry.isLight) {
      if (nextLightSlot >= kLightSlotCount) {
        throw std::runtime_error(
          "PluginIndexMapping: too many light plugins (max 4096)");
      }
      entry.lightSlot = nextLightSlot;
      pluginIndexByLightSlot[nextLightSlot] = plugins.size();
      ++nextLightSlot;
    } else {
      if (nextRegularSlot >= kRegularSlotCount) {
        throw std::runtime_error(
          "PluginIndexMapping: too many regular plugins (max 254)");
      }
      entry.regularSlot = nextRegularSlot;
      pluginIndexByRegularSlot[nextRegularSlot] = plugins.size();
      ++nextRegularSlot;
    }

    pluginIndexByLowerName[lowerName] = plugins.size();
    plugins.push_back(std::move(entry));
  }
}

uint32_t PluginIndexMapping::GetEspmFileIndexByFilename(
  const std::string& fileName) const noexcept
{
  auto it = pluginIndexByLowerName.find(NormalizeFilename(fileName));
  if (it == pluginIndexByLowerName.end()) {
    return kInvalidFileIndex;
  }
  return static_cast<uint32_t>(plugins[it->second].espmFileIndex);
}

uint32_t PluginIndexMapping::GetEspmFileIndex(uint32_t runtimeFormId) const
  noexcept
{
  if (runtimeFormId == 0) {
    return kInvalidFileIndex;
  }

  const uint32_t prefix = runtimeFormId >> 24;
  if (prefix == 0xFF) {
    return kInvalidFileIndex;
  }
  if (prefix == 0xFE) {
    const uint32_t lightSlot = (runtimeFormId >> 12) & 0xFFF;
    if (lightSlot >= pluginIndexByLightSlot.size() ||
        !pluginIndexByLightSlot[lightSlot]) {
      return kInvalidFileIndex;
    }
    return static_cast<uint32_t>(
      plugins[*pluginIndexByLightSlot[lightSlot]].espmFileIndex);
  }
  if (prefix >= pluginIndexByRegularSlot.size() ||
      !pluginIndexByRegularSlot[prefix]) {
    return kInvalidFileIndex;
  }
  return static_cast<uint32_t>(
    plugins[*pluginIndexByRegularSlot[prefix]].espmFileIndex);
}

std::optional<uint32_t> PluginIndexMapping::TryRuntimeFormIdToEspmId(
  uint32_t runtimeFormId) const noexcept
{
  const uint32_t prefix = runtimeFormId >> 24;
  if (prefix == 0xFF) {
    return runtimeFormId;
  }

  size_t pluginIdx;
  uint32_t localPortion;
  if (prefix == 0xFE) {
    const uint32_t lightSlot = (runtimeFormId >> 12) & 0xFFF;
    if (lightSlot >= pluginIndexByLightSlot.size() ||
        !pluginIndexByLightSlot[lightSlot]) {
      return std::nullopt;
    }
    pluginIdx = *pluginIndexByLightSlot[lightSlot];
    localPortion = runtimeFormId & 0xFFF;
  } else {
    if (prefix >= pluginIndexByRegularSlot.size() ||
        !pluginIndexByRegularSlot[prefix]) {
      return std::nullopt;
    }
    pluginIdx = *pluginIndexByRegularSlot[prefix];
    localPortion = runtimeFormId & 0xFFFFFF;
  }

  const size_t espmFileIndex = plugins[pluginIdx].espmFileIndex;
  if (espmFileIndex > 0xFF) {
    return std::nullopt;
  }
  return (static_cast<uint32_t>(espmFileIndex) << 24) | localPortion;
}

uint32_t PluginIndexMapping::RuntimeFormIdToEspmId(
  uint32_t runtimeFormId) const
{
  auto result = TryRuntimeFormIdToEspmId(runtimeFormId);
  if (!result) {
    throw std::runtime_error(fmt::format(
      "PluginIndexMapping: unable to translate runtime FormID {:#x} to an "
      "espm ID",
      runtimeFormId));
  }
  return *result;
}

std::optional<uint32_t> PluginIndexMapping::TryEspmIdToRuntimeFormId(
  uint32_t espmId) const noexcept
{
  const uint32_t espmFileIndex = espmId >> 24;
  if (espmFileIndex == 0xFF) {
    return espmId;
  }
  if (espmFileIndex >= plugins.size()) {
    return std::nullopt;
  }

  const auto& entry = plugins[espmFileIndex];
  const uint32_t localPortion = espmId & 0xFFFFFF;
  if (entry.isLight) {
    if (localPortion > 0xFFF) {
      return std::nullopt;
    }
    return 0xFE000000u | (static_cast<uint32_t>(*entry.lightSlot) << 12) |
      localPortion;
  }
  return (static_cast<uint32_t>(*entry.regularSlot) << 24) | localPortion;
}

uint32_t PluginIndexMapping::EspmIdToRuntimeFormId(uint32_t espmId) const
{
  auto result = TryEspmIdToRuntimeFormId(espmId);
  if (!result) {
    throw std::runtime_error(
      fmt::format("PluginIndexMapping: unable to translate espm ID {:#x} to "
                 "a runtime FormID",
                 espmId));
  }
  return *result;
}

FormDesc PluginIndexMapping::RuntimeFormIdToFormDesc(
  uint32_t runtimeFormId) const
{
  const uint32_t prefix = runtimeFormId >> 24;
  if (prefix == 0xFF) {
    return FormDesc(runtimeFormId - 0xFF000000u, "");
  }
  if (prefix == 0xFE) {
    const uint32_t lightSlot = (runtimeFormId >> 12) & 0xFFF;
    if (lightSlot >= pluginIndexByLightSlot.size() ||
        !pluginIndexByLightSlot[lightSlot]) {
      throw std::runtime_error(fmt::format(
        "PluginIndexMapping: unknown light runtime slot {:#x}", lightSlot));
    }
    const auto& entry = plugins[*pluginIndexByLightSlot[lightSlot]];
    return FormDesc(runtimeFormId & 0xFFF, entry.fileName);
  }
  if (prefix >= pluginIndexByRegularSlot.size() ||
      !pluginIndexByRegularSlot[prefix]) {
    throw std::runtime_error(fmt::format(
      "PluginIndexMapping: unknown regular runtime slot {:#x}", prefix));
  }
  const auto& entry = plugins[*pluginIndexByRegularSlot[prefix]];
  return FormDesc(runtimeFormId & 0xFFFFFF, entry.fileName);
}

uint32_t PluginIndexMapping::FormDescToRuntimeFormId(
  const FormDesc& desc) const
{
  if (desc.file.empty()) {
    return 0xFF000000u + desc.shortFormId;
  }

  auto it = pluginIndexByLowerName.find(NormalizeFilename(desc.file));
  if (it == pluginIndexByLowerName.end()) {
    throw std::runtime_error(desc.file + " not found in loaded files");
  }

  const auto& entry = plugins[it->second];
  if (entry.isLight) {
    if (desc.shortFormId > 0xFFF) {
      throw std::runtime_error(fmt::format(
        "PluginIndexMapping: local FormID {:#x} exceeds the light plugin "
        "range for '{}'",
        desc.shortFormId, entry.fileName));
    }
    return 0xFE000000u | (static_cast<uint32_t>(*entry.lightSlot) << 12) |
      desc.shortFormId;
  }
  return (static_cast<uint32_t>(*entry.regularSlot) << 24) | desc.shortFormId;
}

FormDesc PluginIndexMapping::EspmIdToFormDesc(uint32_t espmId) const
{
  return RuntimeFormIdToFormDesc(EspmIdToRuntimeFormId(espmId));
}

uint32_t PluginIndexMapping::FormDescToEspmId(const FormDesc& desc) const
{
  return RuntimeFormIdToEspmId(FormDescToRuntimeFormId(desc));
}

#pragma once
#include "FormDesc.h"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace espm {
class Loader;
}

enum class PluginKind
{
  Master,
  RegularEsp,
  LightEsl,
  LightEspfe
};

struct PluginIndexMappingEntry
{
  std::string fileName;
  PluginKind kind = PluginKind::RegularEsp;
  bool isLight = false;
  std::optional<uint16_t> regularSlot;
  std::optional<uint16_t> lightSlot;
  size_t espmFileIndex = 0;
};

// Translates between the game's runtime FormIDs (regular plugins get a
// single-byte slot 0x00-0xFD, light/ESL plugins share slot 0xFE with a
// 12-bit sub-index - see espm::Combiner) and the portable (localId, file
// name) pairs FormDesc persists. Unlike the raw "index of the file in
// espmFiles" arithmetic this replaces, it excludes light/ESL-flagged
// plugins from the regular slot count, so a regular plugin's slot number
// does not shift just because ESL plugins were added earlier in the load
// order.
class PluginIndexMapping
{
public:
  static constexpr uint32_t kInvalidFileIndex = 0xFFFFFFFFu;

  // Classifies a plugin the same way the game does: file extension and the
  // TES4 record's own flags (ESM = 0x1, Light/ESL = 0x200) both matter,
  // because a "light" plugin can be shipped with either a .esl or a .esp
  // extension (the latter is commonly called "espfe").
  static PluginKind DetectPluginKind(const std::string& fileName,
                                     uint32_t tes4Flags);

  // Builds the mapping from the load order's file list, in the same order
  // as espm::Loader::GetFileNames(). When `loader` is provided, per-file
  // TES4 flags (espm::Loader::GetFileHeaderFlags(), same order as
  // GetFileNames()) are used so an ESL-flagged .esp is detected correctly;
  // without it, only the .esl extension can be used as a light-plugin
  // signal.
  void Build(const std::vector<std::string>& files,
             espm::Loader* loader = nullptr);

  const std::vector<PluginIndexMappingEntry>& GetPlugins() const noexcept
  {
    return plugins;
  }

  // Espm file index (position in the vector passed to Build) for a plugin
  // by filename, directory prefix and case ignored. kInvalidFileIndex if
  // unknown.
  uint32_t GetEspmFileIndexByFilename(const std::string& fileName) const
    noexcept;

  // Espm file index for a runtime FormID. kInvalidFileIndex for a dynamic
  // FormID or an unmapped slot.
  uint32_t GetEspmFileIndex(uint32_t runtimeFormId) const noexcept;

  // Runtime FormID <-> the legacy "top byte = espm file index" scheme
  // (what espm::Loader/espmFiles-index arithmetic expects). Throws /
  // returns nullopt when the file index does not fit a single byte, or the
  // slot/index is unknown.
  uint32_t RuntimeFormIdToEspmId(uint32_t runtimeFormId) const;
  std::optional<uint32_t> TryRuntimeFormIdToEspmId(
    uint32_t runtimeFormId) const noexcept;
  uint32_t EspmIdToRuntimeFormId(uint32_t espmId) const;
  std::optional<uint32_t> TryEspmIdToRuntimeFormId(uint32_t espmId) const
    noexcept;

  // Runtime FormID <-> FormDesc (the portable, persistence-friendly form).
  FormDesc RuntimeFormIdToFormDesc(uint32_t runtimeFormId) const;
  uint32_t FormDescToRuntimeFormId(const FormDesc& desc) const;

  // EspmId <-> FormDesc, composed from the two conversions above.
  FormDesc EspmIdToFormDesc(uint32_t espmId) const;
  uint32_t FormDescToEspmId(const FormDesc& desc) const;

private:
  std::vector<PluginIndexMappingEntry> plugins;
  std::unordered_map<std::string, size_t> pluginIndexByLowerName;
  std::vector<std::optional<size_t>> pluginIndexByRegularSlot;
  std::vector<std::optional<size_t>> pluginIndexByLightSlot;
};

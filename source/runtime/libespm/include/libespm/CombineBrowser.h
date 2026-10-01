#pragma once
#include "CellOrGridPos.h"
#include "GroupStack.h"
#include "IdMapping.h"
#include "LookupResult.h"
#include "RuntimeFormId.h"
#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace espm {

class RecordHeader;
class Browser;

class CombineBrowser
{
  friend class Combiner;

public:
  // Returns default constructed LookupResult on failure
  // Gets record from the last file in the load order
  LookupResult LookupById(uint32_t formId) const noexcept;

  // Returns a record for each file adding/editing record with such id
  std::vector<LookupResult> LookupByIdAll(uint32_t formId) const noexcept;

  RuntimeFormId ToRuntimeFormId(SourceId source, LocalFormId localId) const;
  ResolvedFormId ResolveRuntimeFormId(RuntimeFormId formId) const;
  RuntimeFormId ResolveReferencedFormId(SourceId referringSource,
                                        FileFormId referencedId) const;
  RuntimeFormId ResolveRecordFormId(SourceId source, FileFormId recordId) const;
  FileFormId ToFileFormId(SourceId viewingSource,
                          RuntimeFormId runtimeId) const;
  std::optional<RuntimeFormId> TryResolveReferencedFormId(
    SourceId referringSource, FileFormId referencedId) const noexcept;
  const SourceMetadata* GetSourceMetadata(SourceId source) const noexcept;
  bool MatchesSourceSelector(RuntimeFormId formId,
                             uint32_t selector) const noexcept;
  std::optional<SourceId> FindSourceByFilename(
    const char* fileName) const noexcept;
  size_t GetSourceCount() const noexcept;

  std::pair<const RecordHeader**, size_t> FindNavMeshes(
    uint32_t worldSpaceId, CellOrGridPos cellOrGridPos) const noexcept;

  [[deprecated("use GetRecordsByTypeResolved")]]
  std::vector<const std::vector<const RecordHeader*>*> GetRecordsByType(
    const char* type) const;
  std::vector<LookupResult> GetRecordsByTypeResolved(const char* type) const;

  std::vector<LookupResult> GetDistinctRecordsByType(const char* type) const;

  std::vector<const std::vector<const RecordHeader*>*> GetRecordsAtPos(
    uint32_t cellOrWorld, int16_t cellX, int16_t cellY) const;

  // Returns nullptr on failure
  [[deprecated("use source-aware runtime FormID APIs")]]
  const IdMapping* GetCombMapping(size_t fileIndex) const noexcept;
  [[deprecated("use source-aware runtime FormID APIs")]]
  const IdMapping* GetRawMapping(size_t fileIndex) const noexcept;

  // CompressedFieldsCache is not logically related to Combiner, this method is
  // added for usability
  espm::CompressedFieldsCache& GetCache() const noexcept;

  const GroupStack& GetParentGroupsEnsured(const RecordHeader* rec) const;
  const std::vector<const void*>& GetSubsEnsured(
    const GroupHeader* group) const;

public:
  struct Source
  {
    Browser* br = nullptr;
    std::string fileName;
    SourceMetadata metadata;
    std::vector<SourceId> targets;
    std::vector<SourceId> regularTargets;
    std::vector<SourceId> lightTargets;
    bool hasExplicitPlacement = false;
    std::unique_ptr<espm::IdMapping> toComb, toRaw;
  };

  struct Impl
  {
    CompressedFieldsCache cache;

    std::vector<Source> sources;
    std::unordered_map<std::string, SourceId> sourceByName;
    std::array<std::optional<SourceId>, 0xFE> regularSourceBySlot{};
    std::array<std::optional<SourceId>, 0x1000> lightSourceBySlot{};
    std::vector<std::vector<SourceId>> candidateSourcesByOwner;
    bool finalized = false;
    int32_t GetFileIndex(const char* fileName) const noexcept;
  };

private:
  std::shared_ptr<Impl> pImpl;

  CombineBrowser() = default;
  CombineBrowser(const CombineBrowser&) = delete;
  void operator=(const CombineBrowser&) = delete;
};

}

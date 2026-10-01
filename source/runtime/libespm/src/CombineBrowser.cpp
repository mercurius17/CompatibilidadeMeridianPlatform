#include "libespm/CombineBrowser.h"

#include "libespm/Browser.h"
#include "libespm/RecordHeader.h"
#include "libespm/Utils.h"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <memory>
#include <unordered_set>

namespace espm {
namespace {

const CombineBrowser::Source& GetSource(const CombineBrowser::Impl& impl,
                                        SourceId source)
{
  if (source >= impl.sources.size()) {
    throw ResolveError(ResolveErrorCode::UnknownSource,
                       "unknown source " + std::to_string(source));
  }
  return impl.sources[source];
}

const std::vector<SourceId>& GetFileNamespace(
  const CombineBrowser::Source& source, PluginClass kind) noexcept
{
  return kind == PluginClass::Light ? source.lightTargets
                                    : source.regularTargets;
}

size_t FindFileNamespaceIndex(const CombineBrowser::Source& source,
                              PluginClass kind, SourceId target)
{
  const auto& targets = GetFileNamespace(source, kind);
  const auto it = std::find(targets.begin(), targets.end(), target);
  if (it == targets.end()) {
    throw ResolveError(ResolveErrorCode::MissingMaster,
                       "runtime source is not visible from the referring "
                       "plugin");
  }
  return static_cast<size_t>(std::distance(targets.begin(), it));
}

} // namespace

int32_t CombineBrowser::Impl::GetFileIndex(const char* fileName) const noexcept
{
  if (!fileName || fileName[0] == '\0') {
    return -1;
  }
  std::string normalized = fileName;
  const auto slash = normalized.find_last_of("\\/");
  if (slash != std::string::npos) {
    normalized.erase(0, slash + 1);
  }
  std::transform(
    normalized.begin(), normalized.end(), normalized.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  auto it = sourceByName.find(normalized);
  if (it != sourceByName.end()) {
    return static_cast<int32_t>(it->second);
  }
  return -1;
}

RuntimeFormId CombineBrowser::ToRuntimeFormId(SourceId source,
                                              LocalFormId localId) const
{
  if (!pImpl || !pImpl->finalized) {
    throw ResolveError(ResolveErrorCode::NotFinalized,
                       "Combiner state is not finalized");
  }
  const auto& entry = GetSource(*pImpl, source);
  if (entry.metadata.kind == PluginClass::Light) {
    if (localId > 0xFFF) {
      throw ResolveError(ResolveErrorCode::LocalIdOutOfRange,
                         "light plugin local FormID exceeds 0xFFF");
    }
    return 0xFE000000u |
      (static_cast<uint32_t>(entry.metadata.placement.slot) << 12) | localId;
  }
  if (localId > 0xFFFFFF) {
    throw ResolveError(ResolveErrorCode::LocalIdOutOfRange,
                       "regular plugin local FormID exceeds 0xFFFFFF");
  }
  if (entry.metadata.placement.slot >= 0xFE) {
    throw ResolveError(ResolveErrorCode::UnknownRegularSlot,
                       "invalid regular runtime slot");
  }
  const RuntimeFormId result =
    (static_cast<uint32_t>(entry.metadata.placement.slot) << 24) | localId;
  if (result == 0) {
    throw ResolveError(ResolveErrorCode::LocalIdOutOfRange,
                       "regular FormID zero is reserved for null");
  }
  return result;
}

ResolvedFormId CombineBrowser::ResolveRuntimeFormId(RuntimeFormId formId) const
{
  if (!pImpl || !pImpl->finalized) {
    throw ResolveError(ResolveErrorCode::NotFinalized,
                       "Combiner state is not finalized");
  }
  if (formId == 0) {
    return { formId, RuntimeFormIdClass::Null, std::nullopt, std::nullopt };
  }
  const uint32_t prefix = formId >> 24;
  if (prefix == 0xFF) {
    return { formId, RuntimeFormIdClass::Dynamic, std::nullopt, std::nullopt };
  }
  if (prefix == 0xFE) {
    const uint16_t slot = static_cast<uint16_t>((formId >> 12) & 0xFFF);
    const auto source = pImpl->lightSourceBySlot[slot];
    if (!source.has_value()) {
      throw ResolveError(ResolveErrorCode::UnknownLightSlot,
                         "unknown light runtime slot " + std::to_string(slot));
    }
    return { formId, RuntimeFormIdClass::Light, source, formId & 0xFFF };
  }
  if (prefix >= pImpl->regularSourceBySlot.size() ||
      !pImpl->regularSourceBySlot[prefix].has_value()) {
    throw ResolveError(ResolveErrorCode::UnknownRegularSlot,
                       "unknown regular runtime slot " +
                         std::to_string(prefix));
  }
  return { formId, RuntimeFormIdClass::Regular,
           pImpl->regularSourceBySlot[prefix], formId & 0xFFFFFF };
}

RuntimeFormId CombineBrowser::ResolveReferencedFormId(
  SourceId referringSource, FileFormId referencedId) const
{
  if (!pImpl || !pImpl->finalized) {
    throw ResolveError(ResolveErrorCode::NotFinalized,
                       "Combiner state is not finalized");
  }
  if (referencedId == 0 || referencedId >= 0xFF000000) {
    return referencedId;
  }

  const auto& source = GetSource(*pImpl, referringSource);
  const uint32_t prefix = referencedId >> 24;
  if (prefix == 0xFE) {
    const uint32_t namespaceIndex = (referencedId >> 12) & 0xFFF;
    const uint32_t localId = referencedId & 0xFFF;
    if (namespaceIndex >= source.lightTargets.size()) {
      throw ResolveError(ResolveErrorCode::InvalidNamespace,
                         "light file FormID index is outside the source "
                         "namespace");
    }
    return ToRuntimeFormId(source.lightTargets[namespaceIndex], localId);
  }

  const uint32_t namespaceIndex = prefix;
  const uint32_t localId = referencedId & 0xFFFFFF;

  if (namespaceIndex < source.regularTargets.size() && localId > 0xFFF) {
    return ToRuntimeFormId(source.regularTargets[namespaceIndex], localId);
  }

  if (namespaceIndex < source.targets.size()) {
    const SourceId target = source.targets[namespaceIndex];
    const auto& targetEntry = GetSource(*pImpl, target);
    if (targetEntry.metadata.kind == PluginClass::Light) {
      return ToRuntimeFormId(target, referencedId & 0xFFF);
    }
    return ToRuntimeFormId(target, localId);
  }

  if (namespaceIndex < source.regularTargets.size()) {
    return ToRuntimeFormId(source.regularTargets[namespaceIndex], localId);
  }

  throw ResolveError(ResolveErrorCode::InvalidNamespace,
                     "regular file FormID index is outside the source "
                     "namespace");
}

RuntimeFormId CombineBrowser::ResolveRecordFormId(SourceId source,
                                                  FileFormId recordId) const
{
  return ResolveReferencedFormId(source, recordId);
}

FileFormId CombineBrowser::ToFileFormId(SourceId viewingSource,
                                        RuntimeFormId runtimeId) const
{
  if (!pImpl || !pImpl->finalized) {
    throw ResolveError(ResolveErrorCode::NotFinalized,
                       "Combiner state is not finalized");
  }
  if (runtimeId == 0 || runtimeId >= 0xFF000000) {
    return runtimeId;
  }

  const auto resolved = ResolveRuntimeFormId(runtimeId);
  const auto& source = GetSource(*pImpl, viewingSource);
  const auto target = *resolved.source;
  if (resolved.idClass == RuntimeFormIdClass::Light) {
    const size_t namespaceIndex =
      FindFileNamespaceIndex(source, PluginClass::Light, target);
    return 0xFE000000u | (static_cast<uint32_t>(namespaceIndex) << 12) |
      (runtimeId & 0xFFF);
  }
  const size_t namespaceIndex =
    FindFileNamespaceIndex(source, PluginClass::Regular, target);
  return (static_cast<uint32_t>(namespaceIndex) << 24) |
    (runtimeId & 0xFFFFFF);
}

std::optional<RuntimeFormId> CombineBrowser::TryResolveReferencedFormId(
  SourceId referringSource, FileFormId referencedId) const noexcept
{
  try {
    return ResolveReferencedFormId(referringSource, referencedId);
  } catch (...) {
    return std::nullopt;
  }
}

const SourceMetadata* CombineBrowser::GetSourceMetadata(
  SourceId source) const noexcept
{
  if (!pImpl || source >= pImpl->sources.size()) {
    return nullptr;
  }
  return &pImpl->sources[source].metadata;
}

bool CombineBrowser::MatchesSourceSelector(RuntimeFormId formId,
                                           uint32_t selector) const noexcept
{
  try {
    const auto resolved = ResolveRuntimeFormId(formId);
    if (!resolved.source.has_value()) {
      return false;
    }
    const SourceId source = *resolved.source;
    const auto& metadata = pImpl->sources[source].metadata;
    if (selector == 0xFE) {
      return metadata.kind == PluginClass::Light;
    }
    if ((selector >> 24) == 0xFE) {
      return metadata.kind == PluginClass::Light &&
        metadata.placement.slot == ((selector >> 12) & 0xFFF);
    }
    if (selector < pImpl->sources.size()) {
      return source == selector;
    }
    return metadata.kind == PluginClass::Regular &&
      metadata.placement.slot == selector;
  } catch (...) {
    return false;
  }
}

std::optional<SourceId> CombineBrowser::FindSourceByFilename(
  const char* fileName) const noexcept
{
  if (!pImpl || !fileName) {
    return std::nullopt;
  }
  const int32_t index = pImpl->GetFileIndex(fileName);
  if (index < 0) {
    return std::nullopt;
  }
  return static_cast<SourceId>(index);
}

size_t CombineBrowser::GetSourceCount() const noexcept
{
  return pImpl ? pImpl->sources.size() : 0;
}

LookupResult CombineBrowser::LookupById(uint32_t combFormId) const noexcept
{
  if (combFormId == 0 || !pImpl || !pImpl->finalized ||
      combFormId >= 0xFF000000) {
    return LookupResult();
  }

  const RecordHeader* result = nullptr;
  SourceId resultSource = 0;
  ResolvedFormId resolved;
  try {
    resolved = ResolveRuntimeFormId(combFormId);
  } catch (const ResolveError&) {
    return LookupResult();
  }
  if (!resolved.source.has_value()) {
    return LookupResult();
  }
  const auto& candidates = pImpl->candidateSourcesByOwner[*resolved.source];
  for (SourceId i : candidates) {
    try {
      const uint32_t rawFormId = ToFileFormId(i, combFormId);
      auto record = pImpl->sources[i].br->LookupById(rawFormId);
      if (!record && pImpl->sources[i].metadata.kind == PluginClass::Light &&
          *resolved.source == i) {
        const uint32_t localId = combFormId & 0xFFF;
        record = pImpl->sources[i].br->LookupById(localId);
        if (!record) {
          const uint32_t ownPrefix = static_cast<uint32_t>(
            pImpl->sources[i].metadata.masters.size());
          record = pImpl->sources[i].br->LookupById(
            (ownPrefix << 24) | localId);
        }
      }
      if (record) {
        result = record;
        resultSource = i;
      }
    } catch (const ResolveError&) {
      // This source cannot see the runtime owner; it is not a candidate.
    }
  }
  return result ? LookupResult(this, result, resultSource) : LookupResult();
}

std::vector<LookupResult> CombineBrowser::LookupByIdAll(
  uint32_t combFormId) const noexcept
{
  std::vector<LookupResult> result;
  if (combFormId == 0 || !pImpl || !pImpl->finalized ||
      combFormId >= 0xFF000000) {
    return result;
  }
  ResolvedFormId resolved;
  try {
    resolved = ResolveRuntimeFormId(combFormId);
  } catch (const ResolveError&) {
    return result;
  }
  if (!resolved.source.has_value()) {
    return result;
  }
  const auto& candidates = pImpl->candidateSourcesByOwner[*resolved.source];
  for (SourceId i : candidates) {
    try {
      const uint32_t rawFormId = ToFileFormId(i, combFormId);
      const RecordHeader* record = pImpl->sources[i].br->LookupById(rawFormId);
      if (!record && pImpl->sources[i].metadata.kind == PluginClass::Light &&
          *resolved.source == i) {
        const uint32_t localId = combFormId & 0xFFF;
        record = pImpl->sources[i].br->LookupById(localId);
        if (!record) {
          const uint32_t ownPrefix = static_cast<uint32_t>(
            pImpl->sources[i].metadata.masters.size());
          record = pImpl->sources[i].br->LookupById(
            (ownPrefix << 24) | localId);
        }
      }
      if (record) {
        result.emplace_back(this, record, i);
      }
    } catch (const ResolveError&) {
    }
  }
  return result;
}

std::pair<const RecordHeader**, size_t> CombineBrowser::FindNavMeshes(
  uint32_t worldSpaceId, CellOrGridPos cellOrGridPos) const noexcept
{
  for (SourceId i = 0; i < pImpl->sources.size(); ++i) {
    try {
      const uint32_t rawFormId = ToFileFormId(i, worldSpaceId);
      auto p = pImpl->sources[i].br->FindNavMeshes(rawFormId, cellOrGridPos);
      if ((!p.first || !*p.first) &&
          pImpl->sources[i].metadata.kind == PluginClass::Light) {
        const auto resolved = ResolveRuntimeFormId(worldSpaceId);
        if (resolved.source.has_value() && *resolved.source == i) {
          p = pImpl->sources[i].br->FindNavMeshes(worldSpaceId & 0xFFF,
                                                  cellOrGridPos);
          if (!p.first || !*p.first) {
            const uint32_t ownPrefix = static_cast<uint32_t>(
              pImpl->sources[i].metadata.masters.size());
            p = pImpl->sources[i].br->FindNavMeshes(
              (ownPrefix << 24) | (worldSpaceId & 0xFFF), cellOrGridPos);
          }
        }
      }
      if (p.first && *p.first) {
        return p;
      }
    } catch (const ResolveError&) {
    }
  }
  return { nullptr, 0 };
}

std::vector<const std::vector<const RecordHeader*>*>
CombineBrowser::GetRecordsByType(const char* type) const
{
  std::vector<const std::vector<const RecordHeader*>*> result;
  for (const auto& source : pImpl->sources) {
    result.push_back(&source.br->GetRecordsByType(type));
  }
  return result;
}

std::vector<LookupResult> CombineBrowser::GetRecordsByTypeResolved(
  const char* type) const
{
  std::vector<LookupResult> result;
  for (SourceId sourceId = 0; sourceId < pImpl->sources.size(); ++sourceId) {
    const auto& records = pImpl->sources[sourceId].br->GetRecordsByType(type);
    for (const auto* record : records) {
      result.emplace_back(this, record, sourceId);
    }
  }
  return result;
}

std::vector<LookupResult> CombineBrowser::GetDistinctRecordsByType(
  const char* type) const
{
  std::unordered_set<formId> formSet;
  std::vector<LookupResult> result;
  for (size_t i = pImpl->sources.size(); i > 0; --i) {
    const SourceId sourceId = static_cast<SourceId>(i - 1);
    const auto& records = pImpl->sources[sourceId].br->GetRecordsByType(type);
    for (auto record : records) {
      try {
        const auto mappedId = ResolveRecordFormId(sourceId, record->GetId());
        if (formSet.insert(mappedId).second) {
          result.emplace_back(this, record, sourceId);
        }
      } catch (const ResolveError&) {
        // Malformed records are not allowed to poison the combined index.
      }
    }
  }
  return result;
}

std::vector<const std::vector<const RecordHeader*>*>
CombineBrowser::GetRecordsAtPos(uint32_t cellOrWorld, int16_t cellX,
                                int16_t cellY) const
{
  static const std::vector<const RecordHeader*> kEmpty;
  std::vector<const std::vector<const RecordHeader*>*> result;
  for (SourceId i = 0; i < pImpl->sources.size(); ++i) {
    try {
      const uint32_t raw = ToFileFormId(i, cellOrWorld);
      const auto* records =
        &pImpl->sources[i].br->GetRecordsAtPos(raw, cellX, cellY);
      if (records->empty() &&
          pImpl->sources[i].metadata.kind == PluginClass::Light) {
        const auto resolved = ResolveRuntimeFormId(cellOrWorld);
        if (resolved.source.has_value() && *resolved.source == i) {
          records = &pImpl->sources[i].br->GetRecordsAtPos(
            cellOrWorld & 0xFFF, cellX, cellY);
          if (records->empty()) {
            const uint32_t ownPrefix = static_cast<uint32_t>(
              pImpl->sources[i].metadata.masters.size());
            records = &pImpl->sources[i].br->GetRecordsAtPos(
              (ownPrefix << 24) | (cellOrWorld & 0xFFF), cellX, cellY);
          }
        }
      }
      result.push_back(records);
    } catch (const ResolveError&) {
      result.push_back(&kEmpty);
    }
  }
  return result;
}

const IdMapping* CombineBrowser::GetCombMapping(
  size_t fileIndex) const noexcept
{
  if (!pImpl || fileIndex >= pImpl->sources.size()) {
    return nullptr;
  }
  return pImpl->sources[fileIndex].toComb.get();
}

const IdMapping* CombineBrowser::GetRawMapping(size_t fileIndex) const noexcept
{
  if (!pImpl || fileIndex >= pImpl->sources.size()) {
    return nullptr;
  }
  return pImpl->sources[fileIndex].toRaw.get();
}

CompressedFieldsCache& CombineBrowser::GetCache() const noexcept
{
  return pImpl->cache;
}

const GroupStack& CombineBrowser::GetParentGroupsEnsured(
  const RecordHeader* rec) const
{
  for (const auto& source : pImpl->sources) {
    const auto result = source.br->GetParentGroupsOptional(rec);
    if (result) {
      return *result;
    }
  }
  throw std::runtime_error(fmt::format(
    "espm::CombineBrowser: no browsers know record id={:#x}", rec->GetId()));
}

const std::vector<const void*>& CombineBrowser::GetSubsEnsured(
  const GroupHeader* group) const
{
  for (const auto& source : pImpl->sources) {
    const auto result = source.br->GetSubsOptional(group);
    if (result) {
      return *result;
    }
  }
  throw std::runtime_error(
    "espm::CombineBrowser: no browsers know requested group");
}

} // namespace espm

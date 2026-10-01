#include "libespm/Combiner.h"

#include "libespm/Browser.h"
#include "libespm/Convert.h"
#include "libespm/Records.h"
#include "libespm/espm.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace espm {
namespace {

constexpr uint32_t kRecordFlagEsm = 0x00000001;
constexpr uint32_t kRecordFlagLight = 0x00000200;

std::string NormalizeFilename(const char* fileName)
{
  std::string result = fileName ? fileName : "";
  const auto slash = result.find_last_of("\\/");
  if (slash != std::string::npos) {
    result.erase(0, slash + 1);
  }
  std::transform(
    result.begin(), result.end(), result.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

bool HasExtension(const std::string& fileName, const char* extension)
{
  const size_t length = std::strlen(extension);
  if (fileName.size() < length) {
    return false;
  }
  const size_t offset = fileName.size() - length;
  for (size_t i = 0; i < length; ++i) {
    if (static_cast<char>(std::tolower(fileName[offset + i])) !=
        static_cast<char>(std::tolower(extension[i]))) {
      return false;
    }
  }
  return true;
}

} // namespace

Combiner::Combiner()
  : pImpl(std::make_shared<CombineBrowser::Impl>())
{
  pImpl->regularSourceBySlot.fill(std::nullopt);
  pImpl->lightSourceBySlot.fill(std::nullopt);
}

SourceId Combiner::AddSource(Browser* src, const char* fileName)
{
  if (pImpl->finalized) {
    throw CombineError("cannot add a source after Combine()");
  }
  if (!src) {
    throw CombineError("cannot add a null source");
  }

  const std::string normalized = NormalizeFilename(fileName);
  if (normalized.empty()) {
    throw CombineError("cannot add a source without a filename");
  }
  if (pImpl->sourceByName.find(normalized) != pImpl->sourceByName.end()) {
    throw CombineError("duplicate source filename " + normalized);
  }

  const auto tes4 = Convert<TES4>(src->LookupById(0));
  if (!tes4) {
    throw CombineError(std::string(fileName) + " doesn't have TES4 record");
  }

  SourceId id = static_cast<SourceId>(pImpl->sources.size());
  CombineBrowser::Source source;
  source.br = src;
  source.fileName = fileName ? fileName : "";
  source.metadata.id = id;
  source.metadata.fileName = source.fileName;
  source.metadata.loadOrderOrdinal = id;
  source.metadata.tes4Flags = tes4->GetFlags();
  source.metadata.hasEslFlag = (tes4->GetFlags() & kRecordFlagLight) != 0;
  source.metadata.hasEslExtension = HasExtension(source.fileName, ".esl");
  source.metadata.kind =
    source.metadata.hasEslFlag || source.metadata.hasEslExtension
    ? PluginClass::Light
    : PluginClass::Regular;
  source.metadata.isMaster = HasExtension(source.fileName, ".esm") ||
    (tes4->GetFlags() & kRecordFlagEsm) != 0;
  source.metadata.placement.kind = source.metadata.kind;

  pImpl->sources.push_back(std::move(source));
  pImpl->sourceByName.emplace(normalized, id);
  return id;
}

void Combiner::SetRuntimePlacement(SourceId source, RuntimePlacement placement)
{
  if (pImpl->finalized) {
    throw CombineError("cannot change runtime placement after Combine()");
  }
  if (source >= pImpl->sources.size()) {
    throw CombineError("unknown source " + std::to_string(source));
  }
  auto& entry = pImpl->sources[source];
  if (entry.metadata.kind != placement.kind) {
    throw CombineError("runtime placement kind does not match source " +
                       entry.fileName);
  }
  entry.metadata.placement = placement;
  entry.hasExplicitPlacement = true;
}

RuntimeFormId Combiner::ToRuntimeFormId(SourceId source,
                                        LocalFormId localId) const
{
  CombineBrowser view;
  view.pImpl = pImpl;
  return view.ToRuntimeFormId(source, localId);
}

ResolvedFormId Combiner::ResolveRuntimeFormId(RuntimeFormId formId) const
{
  CombineBrowser view;
  view.pImpl = pImpl;
  return view.ResolveRuntimeFormId(formId);
}

RuntimeFormId Combiner::ResolveReferencedFormId(SourceId referringSource,
                                                FileFormId referencedId) const
{
  CombineBrowser view;
  view.pImpl = pImpl;
  return view.ResolveReferencedFormId(referringSource, referencedId);
}

const SourceMetadata* Combiner::GetSourceMetadata(
  SourceId source) const noexcept
{
  if (!pImpl || source >= pImpl->sources.size()) {
    return nullptr;
  }
  return &pImpl->sources[source].metadata;
}

std::unique_ptr<CombineBrowser> Combiner::Combine()
{
  if (pImpl->finalized) {
    throw CombineError("Combine() called twice");
  }

  pImpl->regularSourceBySlot.fill(std::nullopt);
  pImpl->lightSourceBySlot.fill(std::nullopt);
  size_t nextRegularSlot = 0;
  size_t nextLightSlot = 0;
  for (size_t i = 0; i < pImpl->sources.size(); ++i) {
    auto& source = pImpl->sources[i];
    if (!source.hasExplicitPlacement) {
      continue;
    }
    const auto placement = source.metadata.placement;
    if (placement.kind == PluginClass::Regular) {
      if (placement.slot >= pImpl->regularSourceBySlot.size() ||
          pImpl->regularSourceBySlot[placement.slot].has_value()) {
        throw CombineError("duplicate or invalid regular runtime slot for " +
                           source.fileName);
      }
      pImpl->regularSourceBySlot[placement.slot] = static_cast<SourceId>(i);
      nextRegularSlot = std::max(nextRegularSlot, size_t(placement.slot) + 1);
    } else {
      if (placement.slot >= pImpl->lightSourceBySlot.size() ||
          pImpl->lightSourceBySlot[placement.slot].has_value()) {
        throw CombineError("duplicate or invalid light runtime slot for " +
                           source.fileName);
      }
      pImpl->lightSourceBySlot[placement.slot] = static_cast<SourceId>(i);
      nextLightSlot = std::max(nextLightSlot, size_t(placement.slot) + 1);
    }
  }

  for (auto& source : pImpl->sources) {
    if (source.hasExplicitPlacement) {
      continue;
    }

    if (source.metadata.kind == PluginClass::Regular) {
      while (nextRegularSlot < pImpl->regularSourceBySlot.size() &&
             pImpl->regularSourceBySlot[nextRegularSlot].has_value()) {
        ++nextRegularSlot;
      }
      if (nextRegularSlot >= pImpl->regularSourceBySlot.size()) {
        throw CombineError("maximum number of regular runtime plugins "
                           "(254) exceeded");
      }
      source.metadata.placement.slot =
        static_cast<uint16_t>(nextRegularSlot++);
      pImpl->regularSourceBySlot[source.metadata.placement.slot] =
        source.metadata.id;
    } else {
      while (nextLightSlot < pImpl->lightSourceBySlot.size() &&
             pImpl->lightSourceBySlot[nextLightSlot].has_value()) {
        ++nextLightSlot;
      }
      if (nextLightSlot >= pImpl->lightSourceBySlot.size()) {
        throw CombineError("maximum number of light runtime plugins "
                           "(4096) exceeded");
      }
      source.metadata.placement.slot = static_cast<uint16_t>(nextLightSlot++);
      pImpl->lightSourceBySlot[source.metadata.placement.slot] =
        source.metadata.id;
    }
  }

  for (size_t i = 0; i < pImpl->sources.size(); ++i) {
    auto& source = pImpl->sources[i];
    espm::CompressedFieldsCache dummyCache;
    const auto tes4 = Convert<TES4>(source.br->LookupById(0));
    const auto masters = tes4->GetData(dummyCache).masters;

    source.targets.clear();
    source.regularTargets.clear();
    source.lightTargets.clear();
    source.metadata.masters.clear();

    uint32_t masterFileIndex = 0;
    for (const char* masterName : masters) {
      const auto normalized = NormalizeFilename(masterName);
      auto it = pImpl->sourceByName.find(normalized);
      if (it == pImpl->sourceByName.end()) {
        throw CombineError(source.fileName + " has unresolved dependency (" +
                           std::string(masterName) + ")");
      }
      SourceId target = it->second;
      if (std::any_of(source.metadata.masters.begin(),
                      source.metadata.masters.end(),
                      [&](const MasterBinding& binding) {
                        return binding.source == target;
                      })) {
        throw CombineError(source.fileName + " lists duplicate master " +
                           std::string(masterName));
      }
      auto targetKind = pImpl->sources[target].metadata.kind;
      auto& classTargets = targetKind == PluginClass::Regular
        ? source.regularTargets
        : source.lightTargets;
      uint32_t fileIndex = masterFileIndex++;
      source.targets.push_back(target);
      classTargets.push_back(target);
      source.metadata.masters.push_back(
        MasterBinding{ target, fileIndex, targetKind });
    }

    // The plugin itself is placed at index masters.size() in the unified file namespace.
    source.targets.push_back(static_cast<SourceId>(i));

    if (source.metadata.kind == PluginClass::Regular) {
      source.regularTargets.push_back(static_cast<SourceId>(i));
    } else {
      source.lightTargets.push_back(static_cast<SourceId>(i));
    }

    if (source.regularTargets.size() > 0xFE) {
      throw CombineError(source.fileName +
                         " has too many entries in its regular FormID "
                         "namespace (maximum 254)");
    }
    if (source.lightTargets.size() > 0x1000) {
      throw CombineError(source.fileName +
                         " has too many entries in its light FormID "
                         "namespace (maximum 4096)");
    }

    // Compatibility mappings are retained for old callers. New code must use
    // the source-aware APIs; a target outside the legacy byte namespace is
    // deliberately left unmapped instead of being truncated.
    auto toComb = std::make_unique<IdMapping>();
    auto toRaw = std::make_unique<IdMapping>();
    toComb->fill(0xff);
    toRaw->fill(0xff);
    for (size_t namespaceIndex = 0;
         namespaceIndex < source.targets.size(); ++namespaceIndex) {
      const SourceId target = source.targets[namespaceIndex];
      if (target <= 0xfe) {
        (*toComb)[namespaceIndex] = static_cast<uint8_t>(target);
        if (target < 0x100) {
          (*toRaw)[target] = static_cast<uint8_t>(namespaceIndex);
        }
      }
    }
    source.toComb = std::move(toComb);
    source.toRaw = std::move(toRaw);
  }

  pImpl->candidateSourcesByOwner.clear();
  pImpl->candidateSourcesByOwner.resize(pImpl->sources.size());
  for (SourceId source = 0; source < pImpl->sources.size(); ++source) {
    for (const auto& master : pImpl->sources[source].metadata.masters) {
      if (master.source < pImpl->candidateSourcesByOwner.size()) {
        pImpl->candidateSourcesByOwner[master.source].push_back(source);
      }
    }
    // A plugin's own records are also candidates for overrides and lookups.
    pImpl->candidateSourcesByOwner[source].push_back(source);
  }
  for (auto& candidates : pImpl->candidateSourcesByOwner) {
    std::sort(
      candidates.begin(), candidates.end(),
      [&](SourceId left, SourceId right) {
        const auto leftOrder = pImpl->sources[left].metadata.loadOrderOrdinal;
        const auto rightOrder =
          pImpl->sources[right].metadata.loadOrderOrdinal;
        return leftOrder != rightOrder ? leftOrder < rightOrder : left < right;
      });
    candidates.erase(std::unique(candidates.begin(), candidates.end()),
                     candidates.end());
  }

  pImpl->finalized = true;
  auto result = std::unique_ptr<CombineBrowser>(new CombineBrowser);
  result->pImpl = pImpl;
  return result;
}

Combiner::~Combiner() = default;

} // namespace espm

#pragma once
#include <cstdint>
#include "RuntimeFormId.h"

namespace espm {

class CombineBrowser;

struct BrowserInfo
{
  BrowserInfo() = default;
  BrowserInfo(const CombineBrowser* parent_, SourceId sourceId_);

  // Returns 0 for empty (default constructed) LookupResult
  uint32_t ToGlobalId(uint32_t rawId) const;

  SourceId GetSourceId() const noexcept { return sourceId; }

  const CombineBrowser* const parent = nullptr;
  // Legacy name retained for source compatibility. This is a SourceId, not a
  // runtime regular-plugin slot.
  const SourceId fileIdx = 0;
  const SourceId sourceId = 0;
};

}

#pragma once
#include "BrowserInfo.h"
#include "RecordHeader.h"

namespace espm {

struct LookupResult : public BrowserInfo
{
  LookupResult() = default;
  LookupResult(const CombineBrowser* parent_, const RecordHeader* rec_,
               SourceId sourceId_);

  // Uses the owning record's namespace when rawId is the record itself. Other
  // values are interpreted as references declared by this source.
  uint32_t ToGlobalId(uint32_t rawId) const;

  const RecordHeader* const rec = nullptr;
};

}

#include "libespm/LookupResult.h"
#include "libespm/CombineBrowser.h"

namespace espm {

LookupResult::LookupResult(const CombineBrowser* parent_,
                           const RecordHeader* rec_, SourceId sourceId_)
  : BrowserInfo(parent_, sourceId_)
  , rec(rec_)
{
}

uint32_t LookupResult::ToGlobalId(uint32_t rawId) const
{
  if (!parent) {
    return 0;
  }
  if (rec && rawId == rec->GetId()) {
    return parent->ResolveRecordFormId(sourceId, rawId);
  }
  return parent->ResolveReferencedFormId(sourceId, rawId);
}

}

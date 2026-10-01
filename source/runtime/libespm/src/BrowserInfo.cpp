#include "libespm/BrowserInfo.h"
#include "libespm/CombineBrowser.h"

namespace espm {

BrowserInfo::BrowserInfo(const CombineBrowser* parent_, SourceId sourceId_)
  : parent(parent_)
  , fileIdx(sourceId_)
  , sourceId(sourceId_) {}

uint32_t BrowserInfo::ToGlobalId(uint32_t rawId) const
{
  if (!parent) {
    return 0;
  }
  return parent->ResolveReferencedFormId(sourceId, rawId);
}

}

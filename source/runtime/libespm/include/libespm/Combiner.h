#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "CombineBrowser.h"
#include "GroupStack.h"
#include "GroupUtils.h"
#include "IdMapping.h"
#include "RuntimeFormId.h"
#include "espm.h"

namespace espm {

class Browser;
class RecordHeader;
class Combiner;

class Combiner
{
public:
  Combiner();
  ~Combiner();

  class CombineError : public std::logic_error
  {
  public:
    CombineError(const std::string& str)
      : logic_error(str) {};
  };

  SourceId AddSource(Browser* src, const char* fileName);

  void SetRuntimePlacement(SourceId source, RuntimePlacement placement);

  // Resolver facade. The finalized state is shared with CombineBrowser, so
  // these methods and LookupResult use the same implementation.
  RuntimeFormId ToRuntimeFormId(SourceId source, LocalFormId localId) const;
  ResolvedFormId ResolveRuntimeFormId(RuntimeFormId formId) const;
  RuntimeFormId ResolveReferencedFormId(SourceId referringSource,
                                        FileFormId referencedId) const;
  const SourceMetadata* GetSourceMetadata(SourceId source) const noexcept;

  // Throws CombineError
  std::unique_ptr<CombineBrowser> Combine();

private:
  std::shared_ptr<CombineBrowser::Impl> pImpl;

  Combiner(const Combiner&) = delete;
  void operator=(const Combiner&) = delete;
};

}

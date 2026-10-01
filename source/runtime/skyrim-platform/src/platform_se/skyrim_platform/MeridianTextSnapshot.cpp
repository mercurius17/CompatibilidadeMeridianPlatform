#include "MeridianTextSnapshot.h"
#include "TextApi.h"
#include "BrowserApi.h"
#include <mutex>
#include <vector>

namespace MeridianTextSnapshot {
namespace {
std::mutex snapshotMutex;
std::shared_ptr<const std::vector<TextToDraw>> snapshot =
  std::make_shared<const std::vector<TextToDraw>>();
}
void Publish()
{
  auto next = std::make_shared<std::vector<TextToDraw>>();
  const auto visibility = TextApi::GetTextsVisibility();
  if (visibility == TextApi::TextsVisibility::kOn ||
      (visibility == TextApi::TextsVisibility::kInheritBrowser && BrowserApi::IsVisible())) {
    for (const auto& [id, text] : TextsCollection::GetSingleton().GetCreatedTexts()) {
      if (!text.refrDirty)
        next->push_back(text);
    }
  }
  std::scoped_lock lock(snapshotMutex);
  snapshot = std::move(next);
}
void Read(TextToDrawCallback callback)
{
  std::shared_ptr<const std::vector<TextToDraw>> current;
  {
    std::scoped_lock lock(snapshotMutex);
    current = snapshot;
  }
  for (const auto& text : *current)
    callback(text); // No own mutex held while DirectX draws.
}
}

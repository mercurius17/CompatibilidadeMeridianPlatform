#pragma once
#include "MeridianUIAPI/ViewAPI.h"
#include "MeridianBrowserPolicy.h"

// Public API boundary, shared by the real adapter and deterministic fake-
// provider tests. These tests do not claim to emulate Chromium or Skyrim.
namespace MeridianViewOperations {
using namespace Meridian::UI::View;
inline ViewHandle Create(IViewAPI* api, const std::string& url, bool visible,
                         DOMReadyCallback ready, ListenerCallback message)
{
  const auto normalized = MeridianBrowserPolicy::NormalizeUrl(url);
  if (!api || !normalized || *normalized != url)
    return INVALID_VIEW_HANDLE;
  ViewCreateInfo info{};
  info.ownerName = "aetherius-client";
  info.viewName = "SkyrimPlatform_Default";
  info.startUrl = url.c_str();
  info.initiallyVisible = visible;
  info.onDOMReady = ready;
  const auto handle = api->CreateView(&info);
  if (handle && !api->RegisterListener(handle, "aetheriusNativeMessage", message)) {
    api->DestroyView(handle);
    return INVALID_VIEW_HANDLE;
  }
  return handle;
}
inline bool IsCurrentReady(IViewAPI* api, ViewHandle current, ViewHandle callback)
{
  return api && current && current == callback && api->IsReady(current);
}
inline FocusResult Focus(IViewAPI* api, ViewHandle view)
{
  return api && view ? api->TryFocus(view, FocusMode::Unpaused) : FocusResult::InvalidView;
}
inline void Destroy(IViewAPI* api, ViewHandle view)
{
  if (api && view && api->IsValid(view))
    api->DestroyView(view);
}
enum class FlushResult { Deferred, Delivered, Rejected };
inline FlushResult Flush(IViewAPI* api, ViewHandle view, bool bridgeReady,
                         std::uint64_t generation, MeridianBrowserPolicy::ScriptQueue& scripts)
{
  if (!api || !view || !bridgeReady)
    return FlushResult::Deferred;
  while (auto script = scripts.Pop(generation)) {
    if (!api->ExecuteJavaScript(view, script->c_str())) {
      scripts.Reset(generation);
      return FlushResult::Rejected;
    }
  }
  return FlushResult::Delivered;
}
}

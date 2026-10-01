#include "MeridianViewOperations.h"
#include <iostream>
#include <vector>

using namespace Meridian::UI::View;
using namespace MeridianViewOperations;
void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}
void __cdecl Ready(ViewHandle) {}
void __cdecl Message(const char*) {}
class FakeProvider : public IViewAPI
{
public:
  bool creationFails = false, registrationFails = false, executionFails = false;
  bool valid = false, ready = false, focused = false, visible = false;
  FocusResult focusResult = FocusResult::Granted;
  int creates = 0, destroys = 0;
  std::vector<std::string> executed;
  ViewHandle __cdecl CreateView(const ViewCreateInfo* info) override
  {
    ++creates;
    Require(info->structSize == sizeof(ViewCreateInfo), "descriptor size");
    Require(std::string(info->ownerName) == "aetherius-client", "pinned owner");
    Require(std::string(info->viewName) == "SkyrimPlatform_Default", "owned view");
    Require(std::string(info->startUrl) == MeridianBrowserPolicy::kPage, "pinned URL");
    Require(info->onDOMReady == Ready, "readiness callback");
    visible = info->initiallyVisible;
    valid = !creationFails;
    return valid ? 7 : 0;
  }
  void __cdecl DestroyView(ViewHandle view) override
  {
    Require(view == 7, "destroyed a foreign view");
    ++destroys; valid = false; focused = false; visible = false;
  }
  bool __cdecl IsValid(ViewHandle handle) const override { return handle == 7 && valid; }
  bool __cdecl IsReady(ViewHandle handle) const override { return IsValid(handle) && ready; }
  bool __cdecl RegisterListener(ViewHandle handle, const char* name, ListenerCallback callback) override
  {
    Require(handle == 7 && std::string(name) == "aetheriusNativeMessage" && callback == Message, "listener contract");
    return !registrationFails;
  }
  bool __cdecl ExecuteJavaScript(ViewHandle handle, const char* script) override
  {
    if (!IsValid(handle) || executionFails) return false;
    executed.emplace_back(script); return true;
  }
  bool __cdecl Show(ViewHandle handle) override { visible = IsValid(handle); return visible; }
  bool __cdecl Hide(ViewHandle handle) override { visible = false; focused = false; return IsValid(handle); }
  FocusResult __cdecl TryFocus(ViewHandle handle, FocusMode mode) override
  {
    Require(mode == FocusMode::Unpaused, "multiplayer must not pause");
    if (!IsValid(handle)) return FocusResult::InvalidView;
    if (!ready) return FocusResult::NotReady;
    focused = focusResult == FocusResult::Granted || focusResult == FocusResult::AlreadyFocused;
    return focusResult;
  }
  void __cdecl Unfocus(ViewHandle) override { focused = false; }
  bool __cdecl HasFocus(ViewHandle handle) const override { return IsValid(handle) && focused; }
  bool __cdecl HasAnyFocus() const override { return focused; }
};
int main()
{
  const std::string url(MeridianBrowserPolicy::kPage);
  Require(Create(nullptr, url, false, Ready, Message) == 0, "absent API");
  FakeProvider provider;
  Require(Create(&provider, "mod://other/index.html", false, Ready, Message) == 0 && provider.creates == 0, "foreign origin");
  provider.creationFails = true;
  Require(Create(&provider, url, false, Ready, Message) == 0 && provider.destroys == 0, "creation failure");
  provider.creationFails = false; provider.registrationFails = true;
  Require(Create(&provider, url, false, Ready, Message) == 0 && provider.destroys == 1, "listener failure cleanup");
  provider.registrationFails = false;
  Require(Create(&provider, url, true, Ready, Message) == 7 && provider.visible, "view creation");
  Require(Focus(&provider, 7) == FocusResult::NotReady && !provider.HasFocus(7), "pre-DOM focus");
  Require(!IsCurrentReady(&provider, 7, 6), "stale callback");
  provider.ready = true;
  Require(IsCurrentReady(&provider, 7, 7), "DOM ready");
  provider.focusResult = FocusResult::Busy;
  Require(Focus(&provider, 7) == FocusResult::Busy && !provider.HasFocus(7), "non-preemptive focus");
  provider.focusResult = FocusResult::Granted;
  Require(Focus(&provider, 7) == FocusResult::Granted && provider.HasFocus(7), "granted focus");
  MeridianBrowserPolicy::ScriptQueue scripts;
  scripts.Push(1, "a"); scripts.Push(1, "b");
  Require(Flush(&provider, 7, false, 1, scripts) == FlushResult::Deferred && provider.executed.empty(), "pre-bridge deferral");
  Require(Flush(&provider, 7, true, 1, scripts) == FlushResult::Delivered &&
    provider.executed == std::vector<std::string>({"a", "b"}), "ordered delivery");
  scripts.Push(1, "old"); scripts.Reset(2);
  Require(Flush(&provider, 7, true, 2, scripts) == FlushResult::Delivered && provider.executed.size() == 2, "cancelled generation");
  scripts.Push(2, "bad"); scripts.Push(2, "must-not-run"); provider.executionFails = true;
  Require(Flush(&provider, 7, true, 2, scripts) == FlushResult::Rejected && !scripts.Pop(2), "failed execution cancellation");
  Destroy(&provider, 7); const auto destroys = provider.destroys;
  Destroy(&provider, 7); Destroy(&provider, 6); Destroy(nullptr, 7);
  Require(provider.destroys == destroys && !provider.HasFocus(7), "idempotent shutdown/foreign handle guard");
  Require(Focus(&provider, 7) == FocusResult::InvalidView, "operation after shutdown");
  std::cout << "PASS: absent API, owner, creation/listener failure, readiness, Busy/Granted, FIFO, cancellation, shutdown\n";
}

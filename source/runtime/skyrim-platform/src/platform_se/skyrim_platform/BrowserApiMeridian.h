#pragma once
#include "MeridianUIAPI/ViewAPI.h"
#include "MeridianBrowserPolicy.h"
#include "NapiHelper.h"
#include <atomic>
#include <functional>

class BrowserApiMeridian
{
public:
  static BrowserApiMeridian& GetInstance();
  void HandleSkseMessage(SKSE::MessagingInterface::Message* message);
  void Register(Napi::Env env, Napi::Object& browser);
  bool IsVisible() const;
  bool IsFocused() const;
  void Shutdown();

private:
  using ViewHandle = Meridian::UI::View::ViewHandle;
  BrowserApiMeridian() = default;
  bool Queue(std::function<void()> operation, std::size_t bytes = 0);
  void CreateView(std::uint64_t expectedGeneration);
  void ApplyVisibility();
  void ApplyFocus();
  void FlushScripts();
  static void __cdecl OnDOMReady(ViewHandle handle);
  static void __cdecl OnMessage(const char* payload);

  std::atomic<Meridian::UI::View::IViewAPI*> api{ nullptr };
  std::atomic<ViewHandle> view{ 0 };
  std::atomic<std::uint64_t> generation{ 1 };
  std::atomic<bool> closing{ false };
  std::atomic<bool> visible{ false };
  std::atomic<bool> bridgeReady{ false };
  // Queue budgets include task payloads, not just the pre-DOM script buffer.
  std::atomic<std::size_t> pendingOperations{ 0 };
  std::atomic<std::size_t> pendingBytes{ 0 };
  std::atomic<std::size_t> pendingMessages{ 0 };
  std::atomic<std::size_t> pendingMessageBytes{ 0 };
  std::atomic<bool> focusRequested{ false };
  std::atomic<unsigned> focusResult{ static_cast<unsigned>(Meridian::UI::View::FocusResult::NotReady) };
  // The fields below belong exclusively to the SKSE task thread.
  bool canCreate = false;
  bool wantedVisible = false;
  bool wantedFocused = false;
  std::string wantedUrl{ MeridianBrowserPolicy::kPage };
  MeridianBrowserPolicy::ScriptQueue scripts;
};

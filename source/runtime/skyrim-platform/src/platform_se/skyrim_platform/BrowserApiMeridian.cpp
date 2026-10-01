#include "BrowserApiMeridian.h"
#include "MeridianUIAPI/ViewDllLoader.h"
#include "MeridianViewOperations.h"
#include "EventsApi.h"
#include "SkyrimPlatform.h"
#include <cstring>

using namespace Meridian::UI::View;

BrowserApiMeridian& BrowserApiMeridian::GetInstance()
{
  // Process lifetime: CEF callbacks can outlive WinMain. No DLL calls in a
  // static destructor or DllMain, and shutdown invalidates pending work first.
  static auto* instance = new BrowserApiMeridian();
  return *instance;
}

bool BrowserApiMeridian::Queue(std::function<void()> operation, std::size_t bytes)
{
  auto* tasks = SKSE::GetTaskInterface();
  if (closing || !tasks || bytes > MeridianBrowserPolicy::kMaxBytes)
    return false;
  const auto count = pendingOperations.fetch_add(1);
  const auto total = pendingBytes.fetch_add(bytes);
  if (count >= MeridianBrowserPolicy::kMaxOperations ||
      total > MeridianBrowserPolicy::kMaxBytes - bytes) {
    pendingOperations.fetch_sub(1);
    pendingBytes.fetch_sub(bytes);
    logger::error("Meridian: task queue limit reached; operation rejected");
    return false;
  }
  tasks->AddTask([this, operation = std::move(operation), bytes] {
    // Release budget before executing; never requeue in this drain.
    pendingOperations.fetch_sub(1);
    pendingBytes.fetch_sub(bytes);
    if (closing)
      return;
    try {
      operation();
    } catch (const std::exception& e) {
      logger::error("Meridian: UI operation failed: {}", e.what());
    }
  });
  return true;
}

void BrowserApiMeridian::HandleSkseMessage(SKSE::MessagingInterface::Message* message)
{
  if (!message || closing)
    return;
  if (message->type == SKSE::MessagingInterface::kInputLoaded) {
    Meridian::UI::Settings settings{};
    // Exact extension/version; no remote pages, debugging port or new CEF copy.
    api = Query(&settings, "aetherius-client");
    if (!api)
      logger::error("Meridian.View/1 unavailable at InputLoaded; UI disabled, core runtime remains active");
    else
      logger::info("Meridian.View/1 acquired at InputLoaded; legacy CEF excluded");
  } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
    Queue([this] {
      canCreate = true;
      CreateView(generation.load());
    });
  }
}

void BrowserApiMeridian::CreateView(std::uint64_t expectedGeneration)
{
  if (closing || generation != expectedGeneration || !canCreate)
    return;
  auto* views = api.load();
  if (!views) {
    scripts.Reset(expectedGeneration);
    logger::error("Meridian: view not created (required provider unavailable)");
    return;
  }
  const auto oldView = view.exchange(0);
  visible = false;
  bridgeReady = false;
  MeridianViewOperations::Destroy(views, oldView);
  const auto created = MeridianViewOperations::Create(views, wantedUrl,
    wantedVisible, OnDOMReady, OnMessage);
  view = created;
  if (created == INVALID_VIEW_HANDLE) {
    view = 0;
    scripts.Reset(expectedGeneration);
    logger::error("Meridian: view creation/listener registration failed");
    return;
  }
  visible = wantedVisible;
  logger::info("Meridian: created view {}, generation {}", created, expectedGeneration);
}

void __cdecl BrowserApiMeridian::OnDOMReady(ViewHandle handle)
{
  auto& self = GetInstance();
  const auto expected = self.generation.load();
  self.Queue([&self, handle, expected] {
    auto* views = self.api.load();
    if (self.generation != expected ||
        !MeridianViewOperations::IsCurrentReady(views, self.view, handle))
      return;
    // The bootstrap is packaged before application code. Listener bindings
    // are already replayed by Meridian; no injected polling loop is needed.
    const auto attach = "window.__aetheriusAttachMeridian?.(" +
      std::to_string(expected) + ");";
    if (!views->ExecuteJavaScript(handle, attach.c_str()))
      logger::error("Meridian: DOM ready but bootstrap could not be executed");
    self.ApplyVisibility();
    self.ApplyFocus();
    logger::info("Meridian: DOM ready for view {}", handle);
  });
}

void __cdecl BrowserApiMeridian::OnMessage(const char* payload)
{
  auto& self = GetInstance();
  if (!payload || self.closing)
    return;
  const auto size = strnlen_s(payload, MeridianBrowserPolicy::kMaxBytes + 1);
  if (size > MeridianBrowserPolicy::kMaxBytes)
    return;
  // Copy immediately, before the CEF callback's pointer ceases to be valid.
  std::string copy(payload, size);
  const auto expected = self.generation.load();
  self.Queue([&self, expected, copy = std::move(copy)] {
    if (self.generation != expected || !self.view)
      return;
    bool tooDeep = false;
    const auto envelope = nlohmann::json::parse(copy,
      [&tooDeep](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
        if (depth > 64) tooDeep = true;
        return depth <= 64;
      }, false);
    if (tooDeep || envelope.is_discarded() || !envelope.is_object() ||
        !envelope.contains("generation") || !envelope["generation"].is_number_unsigned() ||
        envelope["generation"].get<std::uint64_t>() != expected ||
        !envelope.contains("arguments") || !envelope["arguments"].is_array() ||
        envelope["arguments"].size() > 64) {
      logger::warn("Meridian: malformed/stale bridge payload rejected");
      return;
    }
    const auto& arguments = envelope["arguments"];
    if (arguments.size() == 1 && arguments[0] == "__aetherius::bridge-ready") {
      self.bridgeReady = true;
      self.FlushScripts();
      return;
    }
    if (!self.bridgeReady)
      return;
    if (self.pendingMessages.fetch_add(1) >= MeridianBrowserPolicy::kMaxOperations) {
      self.pendingMessages.fetch_sub(1);
      logger::error("Meridian: pending runtime message limit reached");
      return;
    }
    auto json = arguments.dump();
    const auto bytes = json.size();
    if (bytes > MeridianBrowserPolicy::kMaxBytes) {
      self.pendingMessages.fetch_sub(1);
      return;
    }
    if (self.pendingMessageBytes.fetch_add(bytes) > MeridianBrowserPolicy::kMaxBytes - bytes) {
      self.pendingMessageBytes.fetch_sub(bytes);
      self.pendingMessages.fetch_sub(1);
      logger::error("Meridian: pending runtime message byte limit reached");
      return;
    }
    // Queue onto Skyrim Platform's existing JS worker, never run Napi or game
    // calls on CEF's callback thread. Both hops reject obsolete generations.
    SkyrimPlatform::GetSingleton()->AddTickTask(
      [&self, expected, bytes, json = std::move(json)](Napi::Env env) {
        self.pendingMessages.fetch_sub(1);
        self.pendingMessageBytes.fetch_sub(bytes);
        if (self.closing || self.generation != expected)
          return;
        auto event = Napi::Object::New(env);
        event.Set("arguments", NapiHelper::ParseJson(env, json));
        EventsApi::SendEvent("browserMessage", { event });
      });
  }, size);
}

bool BrowserApiMeridian::IsVisible() const
{
  auto* views = api.load();
  const auto handle = view.load();
  return !closing && visible && views && handle && views->IsValid(handle);
}

bool BrowserApiMeridian::IsFocused() const
{
  auto* views = api.load();
  const auto handle = view.load();
  return !closing && views && handle && views->HasFocus(handle);
}

void BrowserApiMeridian::ApplyVisibility()
{
  auto* views = api.load();
  const auto handle = view.load();
  if (!views || !handle)
    return;
  visible = wantedVisible ? views->Show(handle) : false;
  if (!wantedVisible) {
    wantedFocused = false;
    focusRequested = false;
    views->Hide(handle);
  }
}

void BrowserApiMeridian::ApplyFocus()
{
  auto* views = api.load();
  const auto handle = view.load();
  if (!views || !handle)
    return;
  if (!wantedFocused || !wantedVisible) {
    views->Unfocus(handle);
    return;
  }
  const auto result = MeridianViewOperations::Focus(views, handle);
  focusResult = static_cast<unsigned>(result);
  if (result != FocusResult::Granted && result != FocusResult::AlreadyFocused)
    logger::warn("Meridian: focus not granted (result {})", static_cast<unsigned>(result));
}

void BrowserApiMeridian::FlushScripts()
{
  auto* views = api.load();
  const auto handle = view.load();
  const auto expected = generation.load();
  if (MeridianViewOperations::Flush(views, handle, bridgeReady, expected, scripts) ==
      MeridianViewOperations::FlushResult::Rejected) {
      bridgeReady = false;
      logger::error("Meridian: script execution rejected; queued work cancelled");
  }
}

void BrowserApiMeridian::Register(Napi::Env env, Napi::Object& browser)
{
  browser.Set("getBackend", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
    auto result = Napi::Object::New(info.Env());
    result.Set("name", "meridian");
    result.Set("ready", bridgeReady.load());
    result.Set("available", api.load() != nullptr);
    result.Set("generation", static_cast<double>(generation.load()));
    result.Set("focusRequested", focusRequested.load());
    result.Set("focusResult", focusResult.load());
    return result;
  }));
  browser.Set("isVisible", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), IsVisible());
  }));
  browser.Set("isFocused", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), IsFocused());
  }));
  browser.Set("setVisible", Napi::Function::New(env, NapiHelper::WrapCppExceptions(
    [this](const Napi::CallbackInfo& info) {
      const auto requested = NapiHelper::ExtractBoolean(info[0], "isVisible");
      if (!Queue([this, requested] { wantedVisible = requested; ApplyVisibility(); }))
        throw std::runtime_error("Meridian UI visibility request rejected");
      return info.Env().Undefined();
    })));
  browser.Set("setFocused", Napi::Function::New(env, NapiHelper::WrapCppExceptions(
    [this](const Napi::CallbackInfo& info) {
      const auto requested = NapiHelper::ExtractBoolean(info[0], "isFocused");
      if (!Queue([this, requested] {
        wantedFocused = requested;
        focusRequested = requested;
        ApplyFocus();
      }))
        throw std::runtime_error("Meridian UI focus request rejected");
      return info.Env().Undefined();
    })));
  browser.Set("loadUrl", Napi::Function::New(env, NapiHelper::WrapCppExceptions(
    [this](const Napi::CallbackInfo& info) {
      const auto url = MeridianBrowserPolicy::NormalizeUrl(NapiHelper::ExtractString(info[0], "url"));
      if (!url) {
        logger::warn("Meridian: refusing URL outside the trusted client entry point");
        return Napi::Boolean::New(info.Env(), false);
      }
      const auto expected = generation.fetch_add(1) + 1;
      const bool previouslyReady = bridgeReady.exchange(false);
      const auto accepted = Queue([this, expected, url = *url] {
        if (generation != expected)
          return;
        wantedUrl = url;
        scripts.Reset(expected);
        CreateView(expected);
      });
      if (!accepted) {
        auto requestedGeneration = expected;
        if (generation.compare_exchange_strong(requestedGeneration, expected - 1))
          bridgeReady = previouslyReady;
      }
      return Napi::Boolean::New(info.Env(), accepted);
    })));
  browser.Set("executeJavaScript", Napi::Function::New(env, NapiHelper::WrapCppExceptions(
    [this](const Napi::CallbackInfo& info) {
      auto source = NapiHelper::ExtractString(info[0], "src");
      const auto bytes = source.size();
      if (source.find('\0') != std::string::npos)
        throw std::runtime_error("Meridian script contains NUL");
      const auto expected = generation.load();
      if (!Queue([this, expected, source = std::move(source)] {
        if (generation != expected)
          return;
        if (!scripts.Push(expected, source)) {
          logger::error("Meridian: pre-ready script queue full; script rejected");
          return;
        }
        FlushScripts();
      }, bytes))
        throw std::runtime_error("Meridian script request exceeds queue budget");
      return info.Env().Undefined();
    })));
}

void BrowserApiMeridian::Shutdown()
{
  if (closing.exchange(true))
    return;
  generation.fetch_add(1);
  visible = false;
  bridgeReady = false;
  const auto handle = view.exchange(0);
  auto* views = api.exchange(nullptr);
  // Provider shutdown invalidates handles. Never shut down its global CEF.
  MeridianViewOperations::Destroy(views, handle);
}

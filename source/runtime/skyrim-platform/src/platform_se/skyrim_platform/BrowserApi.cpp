#include "BrowserApi.h"
#ifdef SKYRIM_PLATFORM_MERIDIAN
#include "BrowserApiMeridian.h"
#else
#include "BrowserApiNirnLab.h"
#include "BrowserApiTilted.h"
#endif

namespace BrowserApi {
Backend GetBackend()
{
  static std::optional<Backend> g_backend;
  if (g_backend) {
    return *g_backend;
  }

  auto settings = Settings::GetPlatformSettings();

  std::string backendName = settings->GetString("Browser", "BackendName",
#ifdef SKYRIM_PLATFORM_MERIDIAN
                                               "meridian");
#else
                                               "auto");
#endif
  if (backendName == "off") {
    logger::info("browser backend: config value is off, browser disabled");
    g_backend = Backend::kOff;
  }
#ifdef SKYRIM_PLATFORM_MERIDIAN
  else if (backendName == "meridian" || backendName == "auto") {
    logger::info("browser backend: Meridian.View/1 (legacy CEF excluded)");
    g_backend = Backend::kMeridian;
  } else {
    logger::error("This distribution cannot use browser backend '{}'; UI disabled (no legacy fallback)", backendName);
    g_backend = Backend::kOff;
  }
#else
  else if (backendName == "auto") {
    logger::info("browser backend: config value is auto. Auto mode not "
                 "implemented yet, falling back to Tilted UI (legacy)");
    g_backend = Backend::kTilted;
  } else if (backendName == "tilted") {
    logger::info(
      "browser backend: config value is tilted, using Tilted UI (legacy)");
    g_backend = Backend::kTilted;
  } else if (backendName == "nirnlab") {
    logger::info(
      "browser backend: config value is nirnlab, using NirnLab UI Platform");
    g_backend = Backend::kNirnLab;
  } else {
    throw std::runtime_error("invalid BackendName in SkyrimPlatform.ini: must "
                             "be auto/tilted/nirnlab");
  }
#endif
  return *g_backend;
}

bool IsVisible()
{
  switch (GetBackend()) {
    case Backend::kOff:
      return false;
#ifdef SKYRIM_PLATFORM_MERIDIAN
    case Backend::kMeridian:
      return BrowserApiMeridian::GetInstance().IsVisible();
#else
    case Backend::kTilted:
      return BrowserApiTilted::IsVisible();
    case Backend::kNirnLab:
      return BrowserApiNirnLab::GetInstance().IsVisible();
#endif
    default:
      return false;
  }
  // can't reach this place (exception would've thrown), yet the compiler
  // complains
  return false;
}

void Register(Napi::Env env, Napi::Object& exports)
{
  logger::info("registering browser api");

  auto browser = Napi::Object::New(env);
  switch (GetBackend()) {
    case Backend::kOff:
      browser.Set(
        "getBackend",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            auto result = Napi::Object::New(info.Env());
            result.Set("name", Napi::String::New(info.Env(), "off"));
            return result;
          })));
      browser.Set("setVisible",
                  Napi::Function::New(env, [](const Napi::CallbackInfo&) {}));
      browser.Set("isVisible",
                  Napi::Function::New(env, [](const Napi::CallbackInfo& info) {
                    return Napi::Boolean::New(info.Env(), false);
                  }));
      browser.Set("setFocused",
                  Napi::Function::New(env, [](const Napi::CallbackInfo&) {}));
      browser.Set("isFocused",
                  Napi::Function::New(env, [](const Napi::CallbackInfo& info) {
                    return Napi::Boolean::New(info.Env(), false);
                  }));
      browser.Set("loadUrl",
                  Napi::Function::New(env, [](const Napi::CallbackInfo& info) {
                    return Napi::Boolean::New(info.Env(), false);
                  }));
      browser.Set("executeJavaScript",
                  Napi::Function::New(env, [](const Napi::CallbackInfo&) {}));
      break;
#ifdef SKYRIM_PLATFORM_MERIDIAN
    case Backend::kMeridian:
      BrowserApiMeridian::GetInstance().Register(env, browser);
      break;
#else
    case Backend::kTilted:
      browser.Set(
        "getBackend",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            auto result = Napi::Object::New(info.Env());
            result.Set("name", Napi::String::New(info.Env(), "tilted"));
            return result;
          })));
      browser.Set(
        "setVisible",
        Napi::Function::New(
          env, NapiHelper::WrapCppExceptions(BrowserApiTilted::SetVisible)));
      browser.Set(
        "isVisible",
        Napi::Function::New(
          env, NapiHelper::WrapCppExceptions(BrowserApiTilted::IsVisibleJS)));
      browser.Set(
        "setFocused",
        Napi::Function::New(
          env, NapiHelper::WrapCppExceptions(BrowserApiTilted::SetFocused)));
      browser.Set(
        "isFocused",
        Napi::Function::New(
          env, NapiHelper::WrapCppExceptions(BrowserApiTilted::IsFocused)));
      browser.Set(
        "loadUrl",
        Napi::Function::New(
          env, NapiHelper::WrapCppExceptions(BrowserApiTilted::LoadUrl)));
      browser.Set("executeJavaScript",
                  Napi::Function::New(env,
                                      NapiHelper::WrapCppExceptions(
                                        BrowserApiTilted::ExecuteJavaScript)));
      break;
    case Backend::kNirnLab:
      browser.Set(
        "getBackend",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            auto result = Napi::Object::New(info.Env());
            result.Set("name", Napi::String::New(info.Env(), "nirnlab"));
            return result;
          })));
      browser.Set(
        "setVisible",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            return BrowserApiNirnLab::GetInstance().SetVisible(info);
          })));
      browser.Set(
        "isVisible",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            return BrowserApiNirnLab::GetInstance().IsVisible(info);
          })));
      browser.Set(
        "setFocused",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            return BrowserApiNirnLab::GetInstance().SetFocused(info);
          })));
      browser.Set(
        "isFocused",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            return BrowserApiNirnLab::GetInstance().IsFocused(info);
          })));
      browser.Set(
        "loadUrl",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            return BrowserApiNirnLab::GetInstance().LoadUrl(info);
          })));
      browser.Set(
        "executeJavaScript",
        Napi::Function::New(
          env,
          NapiHelper::WrapCppExceptions([](const Napi::CallbackInfo& info) {
            return BrowserApiNirnLab::GetInstance().ExecuteJavaScript(info);
          })));
      break;
  }
#endif
#ifdef SKYRIM_PLATFORM_MERIDIAN
    default:
      throw std::runtime_error("Legacy backend is excluded from this build");
  }
#endif
  exports.Set("browser", browser);
}
} // namespace BrowserApi

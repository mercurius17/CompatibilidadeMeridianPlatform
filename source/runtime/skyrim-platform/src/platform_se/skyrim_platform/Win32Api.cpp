#include "Win32Api.h"

Napi::Value Win32Api::LoadUrl(const Napi::CallbackInfo& info)
{
  auto str = NapiHelper::ExtractString(info[0], "url");
  if (str.substr(0, 8) != "https://") {
    throw std::runtime_error(
      "Permission denied, only 'https://' prefix is allowed");
  } else {
    // CoInitializeEx is needed to call this function, but it seems we already
    // have this call somewhere in code, related link:
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutea
    ShellExecute(0, 0, str.c_str(), 0, 0, SW_SHOW);
  }
  return info.Env().Undefined();
}

Napi::Value Win32Api::ExitProcess(const Napi::CallbackInfo& info)
{
  // JS tick runs outside the game thread. Request normal engine shutdown
  // through SKSE's task queue; never terminate the process or CEF directly.
  auto* tasks = SKSE::GetTaskInterface();
  if (!tasks) {
    throw std::runtime_error("SKSE task interface unavailable for normal shutdown");
  }
  tasks->AddTask([] {
    if (auto* main = RE::Main::GetSingleton()) {
      main->quitGame = true;
      logger::info("Normal engine shutdown requested on the game thread");
    } else {
      logger::error("Normal shutdown failed: RE::Main unavailable");
    }
  });
  return info.Env().Undefined();
}

#include "MeridianTextRenderer.h"
#include <DirectXTK/SimpleMath.h>
#include <filesystem>

MeridianTextRenderer::MeridianTextRenderer(ObtainTextsToDrawFunction obtain)
  : obtainTexts(std::move(obtain))
{
  auto& hook = CEFUtils::D3D11Hook::Get();
  presentConnection = hook.OnPresent.Connect([this](IDXGISwapChain* chain) { Render(chain); });
  lostConnection = hook.OnLost.Connect([this](IDXGISwapChain*) { Reset(); });
}

MeridianTextRenderer::~MeridianTextRenderer()
{
  auto& hook = CEFUtils::D3D11Hook::Get();
  hook.OnPresent.Disconnect(presentConnection);
  hook.OnLost.Disconnect(lostConnection);
}

void MeridianTextRenderer::Reset()
{
  fonts.clear();
  batch.reset();
  states.reset();
  deferred.Reset();
  immediate.Reset();
  device.Reset();
  failed = false;
}

bool MeridianTextRenderer::Create(IDXGISwapChain* chain)
{
  if (!chain || FAILED(chain->GetDevice(IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))))
    return false;
  device->GetImmediateContext(immediate.ReleaseAndGetAddressOf());
  if (!immediate || FAILED(device->CreateDeferredContext(0, deferred.ReleaseAndGetAddressOf())))
    return false;
  batch = std::make_unique<DirectX::SpriteBatch>(deferred.Get());
  states = std::make_unique<DirectX::CommonStates>(device.Get());
  const std::filesystem::path directory("Data/Platform/Fonts");
  if (!std::filesystem::is_directory(directory)) {
    logger::error("Meridian HUD: Platform/Fonts directory missing");
    return false;
  }
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() != ".spritefont")
      continue;
    fonts.emplace(entry.path().stem().string(),
      std::make_unique<DirectX::SpriteFont>(device.Get(), entry.path().c_str()));
  }
  logger::info("Meridian HUD: {} independent SpriteFonts loaded", fonts.size());
  return true;
}

void MeridianTextRenderer::Render(IDXGISwapChain* chain)
{
  if (failed)
    return;
  try {
    if (!batch && !Create(chain)) {
      failed = true;
      return;
    }
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
    immediate->OMGetRenderTargets(1, target.GetAddressOf(), depth.GetAddressOf());
    if (!target)
      return;
    D3D11_VIEWPORT viewport{};
    UINT count = 1;
    immediate->RSGetViewports(&count, &viewport);
    if (!count || !viewport.Width || !viewport.Height)
      return;
    auto* targetRaw = target.Get();
    deferred->OMSetRenderTargets(1, &targetRaw, depth.Get());
    deferred->RSSetViewports(1, &viewport);
    batch->SetViewport(viewport);
    batch->Begin(DirectX::SpriteSortMode_Deferred, states->NonPremultiplied());
    obtainTexts([this](const TextToDraw& text) {
      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
      const auto it = fonts.find(converter.to_bytes(text.fontName));
      if (it == fonts.end())
        return;
      const auto& font = it->second;
      const auto origin = DirectX::SimpleMath::Vector2(font->MeasureString(text.string.c_str())) / 2;
      const DirectX::XMVECTORF32 color = {
        static_cast<float>(text.color[0]), static_cast<float>(text.color[1]),
        static_cast<float>(text.color[2]), static_cast<float>(text.color[3])
      };
      font->DrawString(batch.get(), text.string.c_str(),
        DirectX::XMFLOAT2(static_cast<float>(text.x), static_cast<float>(text.y)),
        color, text.rotation, origin, text.size, text.effects,
        static_cast<float>(text.layerDepth));
    });
    batch->End();
    Microsoft::WRL::ComPtr<ID3D11CommandList> commands;
    if (SUCCEEDED(deferred->FinishCommandList(FALSE, commands.GetAddressOf())))
      immediate->ExecuteCommandList(commands.Get(), TRUE); // Restore game's pipeline state.
  } catch (const std::exception& e) {
    failed = true;
    logger::error("Meridian HUD renderer disabled after failure: {}", e.what());
  }
}

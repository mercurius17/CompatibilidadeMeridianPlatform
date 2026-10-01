#pragma once
#include <ui/TextToDraw.h>
#include <DirectXTK/CommonStates.h>
#include <DirectXTK/SpriteFont.h>
#include <d3d11.h>
#include <wrl.h>
#include <map>

// DirectXTK font drawing only. No CEF objects, browser textures or input hooks.
class MeridianTextRenderer
{
public:
  explicit MeridianTextRenderer(ObtainTextsToDrawFunction obtain);
  ~MeridianTextRenderer();
private:
  void Render(IDXGISwapChain* swapchain);
  bool Create(IDXGISwapChain* swapchain);
  void Reset();
  ObtainTextsToDrawFunction obtainTexts;
  std::size_t presentConnection = 0;
  std::size_t lostConnection = 0;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred;
  std::unique_ptr<DirectX::SpriteBatch> batch;
  std::unique_ptr<DirectX::CommonStates> states;
  std::map<std::string, std::unique_ptr<DirectX::SpriteFont>> fonts;
  bool failed = false;
};

#pragma once
#include<d3d12.h>
#include<wrl.h>

class ResourceObject
{
public:
  ResourceObject(Microsoft::WRL::ComPtr <ID3D12Resource>& resource)
    :resource_(resource)
  {}
  // デストラクタはオブジェクトの寿命が尽きた時に呼ばれる
  ~ResourceObject() {
    // ここで、Resourceを呼べばい良い
    if (resource_) {
      resource_->Release();
    }
  }

  Microsoft::WRL::ComPtr <ID3D12Resource> Get() { return resource_; }

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};

struct D3DResourceLeakChecker {
  // デストラクタはオブジェクトの寿命が尽きた時に呼ばれる
  ~D3DResourceLeakChecker() {

    //リソースリークチェック
    //Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    //if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
    //  debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    //  debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
    //  debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
   }
  };
//};
#include<Windows.h>
#include<cstdint>
#include<string>
#include<format>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#include<dxgidebug.h>
#include<dxcapi.h>
#include "Vector2.h"
#include "Vector3.h"
#include"Vector4.h"
#include "Matrix.h"
#include "Transform.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include"externals/DirectXTex/DirectXTex.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxcompiler.lib")


struct VertexData
{
  Vector4 position;
  Vector2 texcoord;
  Vector3 normal;
};
struct Material {
  Vector4 color;
  int32_t enbleLighting;
};
struct TransformationMatrix {
  Matrix4x4 WVP;
  Matrix4x4 World;
};

struct DirectionalLight
{
  Vector4 color;
  Vector3 direction;
  float intensity;

};

const uint32_t kSubdivision = 16;							//分割数
uint32_t vertexSphere = kSubdivision * kSubdivision * 6;

//ウィンドウプロージャー
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  //メッセージに応じてゲーム固有の処理を行う

  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true;
  }

  switch (msg) {
    //ウィンドウが破棄された
  case WM_DESTROY:
    //OSに対して、アプリの終了を伝える
    PostQuitMessage(0);
    return 0;
  }
  //標準のメッセージ処理を行う
  return DefWindowProc(hwnd, msg, wparam, lparam);
}
void Log(const std::string& message) {
  OutputDebugStringA(message.c_str());;
}

std::wstring ConvertString(const std::string& str) {
  if (str.empty()) {
    return std::wstring();
  }

  auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
  if (sizeNeeded == 0) {
    return std::wstring();
  }
  std::wstring result(sizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
  return result;
}

std::string ConvertString(const std::wstring& str) {
  if (str.empty()) {
    return std::string();
  }

  auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
  if (sizeNeeded == 0) {
    return std::string();
  }
  std::string result(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
  return result;
}



//=============================
//----CompileShader関数========
//=============================

IDxcBlob* CompileShader(
  //CompileするShaderファイルのパス
  const std::wstring& filePath,
  //Compilerに使用するProfile
  const wchar_t* profile,
  //初期化で生成したものを3つ
  IDxcUtils* dxcUtils,
  IDxcCompiler3* dxcCompiler,
  IDxcIncludeHandler* includeHandler) {


  //1.hlslファイルを読む
  //これからシェーダーをコンパイルする旨をログに出す
  Log(ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));
  IDxcBlobEncoding* shaderSource = nullptr;
  HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
  //読めなかったら止める





  assert(SUCCEEDED(hr));
  //読み込んだファイルの内容を設定する
  DxcBuffer shaderSourceBuffer;


  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();


  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8のコードであることを通知

  //2.Compileする
  LPCWSTR arguments[] =
  {

       filePath.c_str(),
       L"-E",L"main",
       L"-T",profile,
       L"-Zi",L"-Qembed_debug",
       L"-Od",
       L"-Zpr",
  };
  //実際にshaderをコンパイルする
  IDxcResult* shaderResult = nullptr;


  hr = dxcCompiler->Compile(
    &shaderSourceBuffer,
    arguments,
    _countof(arguments),
    includeHandler,
    IID_PPV_ARGS(&shaderResult)
  );
  assert(SUCCEEDED(hr));

  IDxcBlobUtf8* shaderError = nullptr;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

  if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
    Log(shaderError->GetStringPointer());
    assert(false);
  }

  //コンパイル結果から実行用のバイナリ部分を取得
  IDxcBlob* shaderBlob = nullptr;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
  assert(SUCCEEDED(hr));
  //成功したログを出す
  Log(ConvertString(std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile)));
  //もう使わないリソースを解放
  shaderSource->Release();
  shaderResult->Release();
  //実行用のバイナリを返却
  return shaderBlob;


}


ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes)

{
  //IDXGIのファクトリーの生成
  IDXGIFactory7* dxgiFactory = nullptr;

  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

  //========================================
  //======= VertexResourceを生成する =======
  //========================================

  // 頂点リソース用のヒープの設定
  D3D12_HEAP_PROPERTIES uploadHeapProperties{};
  uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;  // uploadHeapを使う

  // 頂点リソース用のヒープの設定
  D3D12_RESOURCE_DESC vertexResourceDesc{};

  // バッファリソース。テクスチャーの場合はまた別の設定をする
  vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vertexResourceDesc.Width = sizeInBytes; //リソースのサイズ。

  // バッファの場合はこれらは1にする決まり
  vertexResourceDesc.Height = 1;
  vertexResourceDesc.DepthOrArraySize = 1;
  vertexResourceDesc.MipLevels = 1;
  vertexResourceDesc.SampleDesc.Count = 1;

  // バッファの場合はこれにする
  vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


  //実際に頂点リソースを作る
  ID3D12Resource* Resource = nullptr;
  hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Resource));
  assert(SUCCEEDED(hr));

  return Resource;
}

ID3D12DescriptorHeap* CreateDescriptorHeap(
  ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
  //ディスクリプタヒープの生成
  ID3D12DescriptorHeap* desciptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
  descriptorHeapDesc.Type = heapType;//レンダーターゲットビュー用
  descriptorHeapDesc.NumDescriptors = numDescriptors;//ダブルバッファ用二2つ。多くても構わない
  descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&desciptorHeap));
  //ディスクリプタヒープが作れなかったので起動できない
  assert(SUCCEEDED(hr));
  return desciptorHeap;
}



//=====================================//
//=== Textureデータを読み込むための関数 ===//
//=====================================//

DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
  // テクスチャファイルを読んでプログラムで扱えるようにする
  DirectX::ScratchImage image{};
  std::wstring filePathW = ConvertString(filePath);
  HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
  assert(SUCCEEDED(hr));

  // ミニマップの作成
  DirectX::ScratchImage mipImages{};
  hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
  assert(SUCCEEDED(hr));


  //ミニマップ付きのデータを返す
  return mipImages;
}

//=================================//
//==== TextureResourceを作る関数 ====//
//=================================//

ID3D12Resource* CreateTextureResourse(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{

  // 1. metadataを基にResourceの設定
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = UINT(metadata.width);// Textureの幅
  resourceDesc.Height = UINT(metadata.height);// Textureの高さ
  resourceDesc.MipLevels = UINT16(metadata.mipLevels);// mipmapの数
  resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);// 奥行きor配列Textureの配列数
  resourceDesc.Format = metadata.format;// TextureのFormat
  resourceDesc.SampleDesc.Count = 1;// サンプリングカウント。1固定
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);// Textureの次元数。普段は2次元

  // 2. 利用するHeapの設定。非常に特殊な運用。02_04exの資料で一般的なケース番がある

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;// 細かい設定を行う
  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;// WriteBackポリシーでCPUアクセス可能
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;// プロセッサの近くに配置


  // 3. Resourceを生成する

  ID3D12Resource* resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
    &heapProperties,// Heapの設定
    D3D12_HEAP_FLAG_NONE,// Heapの特殊な設定
    &resourceDesc,// Resourceの設定
    D3D12_RESOURCE_STATE_GENERIC_READ,// 初回のResourceState。Textureは基本読むだけ
    nullptr,// Clear最適値。使わないからnullptr
    IID_PPV_ARGS(&resource));// 作成するResourceポインタへのポインタ
  assert(SUCCEEDED(hr));

  return resource;
}


//==========================//
//==== データを転送する関数 ====//
//==========================//


void UploadTextureDate(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{

  // Meta情報を取得する
  const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

  // 全MipMapについて
  for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {

    // MipMapLevelを指定して各Imageを取得
    const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);

    // Textureに転送
    HRESULT hr = texture->WriteToSubresource(
      UINT(mipLevel),
      nullptr,              // 全領域へコピー
      img->pixels,          // 元データアドレス
      UINT(img->rowPitch),  // 1ラインサイズ
      UINT(img->slicePitch) // 1枚サイズ
    );
    assert(SUCCEEDED(hr));
  }
}


//========================================//
//======= DepthStencilTextureを作る =======//
//========================================//

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {

  // 生成するResourceの設定
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = width;// Textureの幅
  resourceDesc.Height = height;// Textureの高さ
  resourceDesc.MipLevels = 1;// mipmapの高さ
  resourceDesc.DepthOrArraySize = 1;// 奥行き or 配列Textureの配列数
  resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;// DepthStencilとして利用可能なフォーマット
  resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 2次元
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;// DepthStencilとして通知

  // 利用するHeapの設定
  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// VRAM上に作る

  // 深度値のクリア設定
  D3D12_CLEAR_VALUE depthClearValue{};
  depthClearValue.DepthStencil.Depth = 1.0f;// 1.0f(最大値)でクリア
  depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;// フォーマット。Resourceと合わせる


  // Resourceの生成
  ID3D12Resource* resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
    &heapProperties, //　Heapの設定
    D3D12_HEAP_FLAG_NONE,// Heapの特殊な設定
    &resourceDesc, // Resourceの設定
    D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
    &depthClearValue, // Clear最適値
    IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
  assert(SUCCEEDED(hr));

  return resource;
}

//==================================
// DescriptorHandleを関数化
//==================================

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
  D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  handleCPU.ptr += (descriptorSize * index);
  return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
  D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
  handleGPU.ptr += (descriptorSize * index);
  return handleGPU;
}


//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  OutputDebugStringA("Hello,Directx!\n");

  CoInitializeEx(0, COINIT_MULTITHREADED);

  WNDCLASS wc{};
  //ウィンドウプロシージャ
  wc.lpfnWndProc = WindowProc;
  //ウィンドウクラス名
  wc.lpszClassName = L"CG2WindowClass";
  //インスタンスハンドル
  wc.hInstance = GetModuleHandle(nullptr);
  //カーソル
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  //ウィンドウクラスを登録する
  RegisterClass(&wc);


  //クライアント領域のサイズ
  const int32_t kClientWidth = 1280;
  const int32_t kClientHeight = 720;

  //ウィンドウサイズを表す構造体にクライアント領域を入れる
  RECT wrc = { 0,0,kClientWidth,kClientHeight };

  //クライアント領域を元に実際のサイズにwrcを変更してもらう
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

  //ウィンドウの生成
  HWND hwnd = CreateWindow(
    wc.lpszClassName,        //利用するクラス名
    L"CG2",                  //タイトルバーの文字
    WS_OVERLAPPEDWINDOW,     //よく見るウィンドウスタイル
    CW_USEDEFAULT,           //表示X座標(Windowsに任せる)
    CW_USEDEFAULT,           //表示Y座標(WindowsOSに任せる)
    wrc.right - wrc.left,    //ウィンドウ横幅
    wrc.bottom - wrc.top,    //ウィンドウ縦幅
    nullptr,                 //親ウィンドウハンドル
    nullptr,                 //メニューハンドル
    wc.hInstance,            //インスタンスハンドル
    nullptr);                //オプション

  //ウィンドウを表示する
  ShowWindow(hwnd, SW_SHOW);






  //デバックレイヤー
#ifdef _DEBUG
  ID3D12Debug1* debugController = nullptr;

  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    //デバッグレイヤーを有効化
    debugController->EnableDebugLayer();
    //さらにGPU側でもチェックを行えるようにする
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif // _DEBUG




  /*D3D12Deviceの生成*/
  ID3D12Device* device = nullptr;



  //IDXGIのファクトリーの生成
  IDXGIFactory7* dxgiFactory = nullptr;

  //HRESULTはWindows系のエラーコードであり、
//関数が成功したかどうかをSUCCEEDEDマクロで判定できる
  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
  //初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が
//多いのでassertにしておく
  assert(SUCCEEDED(hr));

  //仕様するアダプター用の変数。最初にnullptrを入れておく
  IDXGIAdapter4* useAdapter = nullptr;
  //良い順にアダプターを頼む
  for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
    DXGI_ERROR_NOT_FOUND; ++i) {
    //アダプターの情報を取得する
    DXGI_ADAPTER_DESC3 adapterDesc{};
    hr = useAdapter->GetDesc3(&adapterDesc);
    assert(SUCCEEDED(hr));//取得できないのは一大事
    //ソフトウェアアダプターでなければ採用
    if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
      //採用したアダプタの情報をログに出力。wstringの方なので注意
      Log(ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
      break;
    }
    useAdapter = nullptr;//ソフトウェアアダプタの場合は見なかったことにする
  }
  //適切なアダプタが見つからないので起動できない
  assert(useAdapter != nullptr);
  //機能レベルとログ出力用の文字列
  D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
  };
  const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
  //高い順に生成できるか試す
  for (size_t i = 0; i < _countof(featureLevels); ++i) {
    //採用したアダプターでデバイスを生成
    hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
    //指定した機能レベルでデバイスが生成できたか確認
    if (SUCCEEDED(hr)) {
      //生成できたのでログ出力を行ってループを抜ける
      Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
      break;
    }
  }
  //デバイスの生成がうまくいかなかったので起動できない
  assert(device != nullptr);
  Log("Complete create D3D12Device!!!\n");//初期化完了のログを出す

#ifdef _DEBUG
  ID3D12InfoQueue* infoQueue = nullptr;
  if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
    //ヤバイエラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    //エラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    //警告時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
    //抑制するメッセージのID
    D3D12_MESSAGE_ID denyIds[] = {
      //Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
      //https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
      D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
    };
    //抑制するレベル
    D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
    D3D12_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    //指定したメッセージの表示を抑制
    infoQueue->PushStorageFilter(&filter);

    //解放
    infoQueue->Release();
  }
#endif // _DEBUG




  //コマンドキューを生成する
  ID3D12CommandQueue* commandQueue = nullptr;
  D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
  hr = device->CreateCommandQueue(&commandQueueDesc,
    IID_PPV_ARGS(&commandQueue));
  //コマンドキューの生成がうまくいかなかったので起動できない
  assert(SUCCEEDED(hr));

  //コマンドアロケーターを生成する
  ID3D12CommandAllocator* commandAllocator = nullptr;
  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
  //コマンドアロケータの生成がうまくいかなかったので起動できない
  assert(SUCCEEDED(hr));

  //コマンドリストを生成する
  ID3D12GraphicsCommandList* commandList = nullptr;
  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr,
    IID_PPV_ARGS(&commandList));
  //コマンドリストの生成がうまくいかなかったので起動できない
  assert(SUCCEEDED(hr));

  //SwapChain(スワップチェーン)を生成する
  IDXGISwapChain4* swapChain = nullptr;
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.Width = kClientWidth;//画面の幅。ウィンドウのクライアント領域を同じものにしておく
  swapChainDesc.Height = kClientHeight;//画面の高さ。ウィンドウのクライアント領域を同じものにしておく
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の形式
  swapChainDesc.SampleDesc.Count = 1;//マルチサンプルしない
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画のターゲットとして利用する
  swapChainDesc.BufferCount = 2;//ダブルバッファ
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニタに移したら、中身居を破棄
  //コマンドキュー、ウィンドウハンドル、設定を渡して生成する
  hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
  assert(SUCCEEDED(hr));

  //==========================================//
  //============ DescriptorRange =============//
  //==========================================//

  D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
  descriptorRange[0].BaseShaderRegister = 0;// 0から
  descriptorRange[0].NumDescriptors = 1;// 数は1つ
  descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う
  descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;// offsetを自動計算

  //=========================================
  //========= RootSignatureを生成する =========
  //=========================================

  D3D12_ROOT_SIGNATURE_DESC decriptionRootSignature{};
  decriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // RootParameterを作成。PixelShaderのMaterialとVertexShaderのTransform
  D3D12_ROOT_PARAMETER rootParameters[4] = {};
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
  rootParameters[0].Descriptor.ShaderRegister = 0;  //レジスタ番号0を使う

  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; //CBVを使う
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; //VertexShaderで使う
  rootParameters[1].Descriptor.ShaderRegister = 0; //レジスタ番号を使う
  // DescriptorTable
  rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;// DescriptorTableを使う
  rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;// PixelShaderで使う
  rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;// Tableの中身の配列を指定
  rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);// Tableで利用する数

  rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  rootParameters[3].Descriptor.ShaderRegister = 1;


  decriptionRootSignature.pParameters = rootParameters;  //ルートパラメータ配列へのポインタ
  decriptionRootSignature.NumParameters = _countof(rootParameters);  //配列の長さ




  //======================================//
  //============　Samplerの設定 =============//
  //======================================//


  D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
  staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; //バイリニアフィルター
  staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0～1の範囲外をリピート
  staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;// 比較しない
  staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipMapを使う
  staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
  staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
  decriptionRootSignature.pStaticSamplers = staticSamplers;
  decriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);


  // Textureを読んで転送する
  DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
  const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
  ID3D12Resource* textureResource = CreateTextureResourse(device, metadata);
  UploadTextureDate(textureResource, mipImages);

  // 2枚目のTextureを読んで転送する
  DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
  const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
  ID3D12Resource* textureResource2 = CreateTextureResourse(device, metadata2);
  UploadTextureDate(textureResource2, mipImages2);

  //シリアライズしてバイナリにする
  ID3DBlob* signatureBlob = nullptr;
  ID3DBlob* errorBlob = nullptr;
  hr = D3D12SerializeRootSignature(&decriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
  if (FAILED(hr)) {
    Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
    assert(false);
  }
  //バイナリを元に作成
  ID3D12RootSignature* rootSignature = nullptr;
  hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
  assert(SUCCEEDED(hr));


  //=========================================
  //======== InputLayoutの設定を行う ==========
  //=========================================

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
  inputElementDescs[0].SemanticName = "POSITION";
  inputElementDescs[0].SemanticIndex = 0;
  inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

  inputElementDescs[1].SemanticName = "TEXCOORD";
  inputElementDescs[1].SemanticIndex = 0;
  inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

  inputElementDescs[2].SemanticName = "NORMAL";
  inputElementDescs[2].SemanticIndex = 0;
  inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = inputElementDescs;
  inputLayoutDesc.NumElements = _countof(inputElementDescs);



  //========================================
  //======= BlendStateの設定を行う =========
  //========================================

  D3D12_BLEND_DESC blendDesc{};
  //全ての色要素を書き込む
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;



  //========================================
  //===== RasterizerStateの設定を行う ======
  //========================================

  D3D12_RASTERIZER_DESC rasterizerDesc{};
  //裏面(時計回り)を表示しない
  rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
  //三角形の中を塗りつぶす

  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;



  // DescriptorSizeを取得しておく
  const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


  //RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
  ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
  //SRV用のヒープでディスクリプタの数は128。SRVはShaderないで触れるものなので、ShaderVisibleはtrue
  ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
   // DSV用のヒープでディスクリプタの数は1。DSVはShaderないで触るものではないので、ShaderVisibleはfalse
  ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);




  // DepthStencilTextureをウィンドウサイズで作成
  ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);


  //==========================================//
 //========= DepthStencilView(DSC) ==========//
 //==========================================//

  // DSVの設定
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; //2DTexture

  // DSCHeapの先頭にDSVを作る
  device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

  // 描画先のRTVとDSVを設定する
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();






  //=============================//
  //======ShaderResourceView=====//
  //=============================//

 
  // 1枚目のSRV
  // meataDataを基にSRVの設定
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = metadata.format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; //2Dテクスチャ
  srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

  // SRVを作成するDescriptorHeapの場所を決める
  D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
  // 先頭はImGuiが使ってるのでその次を使う
  textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // SRVの生成
  device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

  // 2枚目のSRV
  // meataDataを基にSRVの設定
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
  srvDesc2.Format = metadata2.format;
  srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

  // SRVを生成するDescriptorHeapの場所を決める
  D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

  // SRVの生成
  device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

  //SwapChainからResourceを引っ張ってくる
  ID3D12Resource* swapChainResources[2] = { nullptr };
  hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
  //上手く取得できなければ起動できない
  assert(SUCCEEDED(hr));
  hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
  assert(SUCCEEDED(hr));

  //RTVの設定
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;//出力結果をSRGB2変換して書き込む
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;//2Dテクスチャとして読み込む
  //ディスクリプタの先頭を取得する
  D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  //RTVを2つ作るのでディスクリプタを2つ用意
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
  //まず1つ目を作る。1つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある
  rtvHandles[0] = rtvStartHandle;
  device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
  //2つ目のディスクリプタハンドルを得る
  rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  //2つ目を作る
  device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

  typedef struct D3D12_CPU_DESCRIPTOR_HANDLE {
    SIZE_T ptr;
  } D3D12_CPU_DESCRIPTOR_HANDLE;
  rtvHandles[0] = rtvStartHandle;
  rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  //初期値0でFenceを作る
  ID3D12Fence* fence = nullptr;
  uint64_t fenceValue = 0;
  hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  assert(SUCCEEDED(hr));

  //FenceのSignalを待つためのイベントを作成する
  HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent != nullptr);

  // dxCompilerを初期化
  IDxcUtils* dxcUtils = nullptr;
  IDxcCompiler3* dxcCompiler = nullptr;
  hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
  assert(SUCCEEDED(hr));
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
  assert(SUCCEEDED(hr));


  //現時点でincludeはしないが、includeに対応するための設定を行っていく
  IDxcIncludeHandler* includeHandler = nullptr;
  hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
  assert(SUCCEEDED(hr));



  //========================================
  //======== ShaderをCompileする ===========
  //========================================

  IDxcBlob* verterShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
  assert(verterShaderBlob != nullptr);

  IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
  assert(pixelShaderBlob != nullptr);



  //=====================================================//
  //=========== DepthStencilStateの設定を行う =============//
  //=====================================================//

  // DepthStencilStateの設定
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

  // Depthの機能を有効化する
  depthStencilDesc.DepthEnable = true;

  // 書き込みします
  depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

  // 比較関数はLessEqual。つまり、近ければ描画される
  depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;





  //======================================
  //========== PSOを生成する =============
  //======================================

  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = rootSignature;   // RootSignature
  graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;    // InputLayout
  graphicsPipelineStateDesc.VS = { verterShaderBlob->GetBufferPointer(),verterShaderBlob->GetBufferSize() };  // VertexShader
  graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };    // PixelShader
  graphicsPipelineStateDesc.BlendState = blendDesc;// BlendState
  graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; //RasterizerState

  // DepthStencilの設定
  graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

  // 書き込むRTVの情報
  graphicsPipelineStateDesc.NumRenderTargets = 1;
  graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  //利用するトポロジ(形状)のタイプ。三角形
  graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


  // どのように画面に色を打ち込むかの設定(気にしなくて良い)
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

  // 実際に生成
  ID3D12PipelineState* graphicsPipelineState = nullptr;
  hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
  assert(SUCCEEDED(hr));


  //========================================
  //======= VertexResourceを生成する =======
  //========================================

  //マテリアル用のリソース
  ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
  //マテリアル用にデータを書き込む
  Material* materialData = nullptr;
  //書き込むためのアドレスを取得
  materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
  materialData->color={ 1.0f,1.0f,1.0f,1.0f };
  materialData->enbleLighting = true;

  //
  ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
  Material* materialDataSprite = nullptr;
  materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
  materialDataSprite->color = {1.0f,1.0f,1.0f,1.0f};
  materialDataSprite->enbleLighting = false;

  // 
  ID3D12Resource* lightResource = CreateBufferResource(device, sizeof(DirectionalLight));
  DirectionalLight* directionalLightData = nullptr;
  lightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
  directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
  directionalLightData->direction = { 0.0f,-1.0f,0.0f };
  directionalLightData->intensity = 1.0f;



  //==============================================
  //========== VertexBufferViewを作成 ============
  //==============================================
  ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * vertexSphere);

  // Sprite用の頂点リソースを作る
  ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

  //頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
  // リソースの先頭のアドレスから使う
  vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
  // 使用するリソースのサイズは6つ分のサイズ
  vertexBufferView.SizeInBytes = sizeof(VertexData) * vertexSphere;
  // 1頂点当たりのサイズ
  vertexBufferView.StrideInBytes = sizeof(VertexData);

  // Spriteの頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
  vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
  vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
  vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);


  //==============================================
  //========= Resourceにデータを書き込む =========
  //==============================================



  //頂点リソースにデータを書き込む
  VertexData* vertexData = nullptr;

  //書き込むためのアドレスを取得
  vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));



  const float kLonEvery = (float)M_PI * 2.0f / float(kSubdivision);

  const float kLatEvery = (float)M_PI / float(kSubdivision);

  for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {

    float lat = (float)-M_PI / 2.0f + kLatEvery * latIndex;
    // 経度の方向に分割しながら線を描く
    for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {

      uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;

      float lon = lonIndex * kLonEvery;
      float nextLon = lon + kLonEvery;

      float u = float(lonIndex) / float(kSubdivision);
      float v = 1.0f - float(latIndex) / float(kSubdivision);
     // float lat = -0.5f * (float)M_PI + latIndex * kLatEvery;
      float nextLat = lat + kLatEvery;

      // 頂点にデータを入力する
      vertexData[start].position.x = cos(lat) * cos(lon);
      vertexData[start].position.y = sin(lat);
      vertexData[start].position.z = cos(lat) * sin(lon);
      vertexData[start].position.w = 1.0f;
      vertexData[start].texcoord = { u,v };
      vertexData[start].normal.x = vertexData[start].position.x;
      vertexData[start].normal.y = vertexData[start].position.y;
      vertexData[start].normal.z = vertexData[start].position.z;

      vertexData[start + 1].position.x = cos(nextLat) * cos(lon);
      vertexData[start + 1].position.y = sin(nextLat);
      vertexData[start + 1].position.z = cos(nextLat) * sin(lon);
      vertexData[start + 1].position.w = 1.0f;
      vertexData[start + 1].texcoord = { float(lonIndex) / float(kSubdivision),1.0f - float(latIndex + 1) / float(kSubdivision) };
      vertexData[start+1].normal.x = vertexData[start+1].position.x;
      vertexData[start+1].normal.y = vertexData[start+1].position.y;
      vertexData[start+1].normal.z = vertexData[start+1].position.z;

      vertexData[start + 2].position.x = cos(lat) * cos(nextLon);
      vertexData[start + 2].position.y = sin(lat);
      vertexData[start + 2].position.z = cos(lat) * sin(nextLon);
      vertexData[start + 2].position.w = 1.0f;
      vertexData[start + 2].texcoord = { float(lonIndex + 1) / float(kSubdivision),1.0f - float(latIndex) / float(kSubdivision) };
      vertexData[start+2].normal.x = vertexData[start+2].position.x;
      vertexData[start+2].normal.y = vertexData[start+2].position.y;
      vertexData[start+2].normal.z = vertexData[start+2].position.z;

      vertexData[start + 3].position.x = cos(nextLat) * cos(nextLon);
      vertexData[start + 3].position.y = sin(nextLat);
      vertexData[start + 3].position.z = cos(nextLat) * sin(nextLon);
      vertexData[start + 3].position.w = 1.0f;
      vertexData[start + 3].texcoord = { float(lonIndex+1) / float(kSubdivision),1.0f - float(latIndex+1) / float(kSubdivision) };
      vertexData[start+3].normal.x = vertexData[start+3].position.x;
      vertexData[start+3].normal.y = vertexData[start+3].position.y;
      vertexData[start+3].normal.z = vertexData[start+3].position.z;

      vertexData[start + 4].position.x = cos(lat) * cos(nextLon);
      vertexData[start + 4].position.y = sin(lat);
      vertexData[start + 4].position.z = cos(lat) * sin(nextLon);
      vertexData[start + 4].position.w = 1.0f;
      vertexData[start + 4].texcoord = { float(lonIndex + 1) / float(kSubdivision),1.0f - float(latIndex) / float(kSubdivision) };
      vertexData[start+4].normal.x = vertexData[start+4].position.x;
      vertexData[start+4].normal.y = vertexData[start+4].position.y;
      vertexData[start+4].normal.z = vertexData[start+4].position.z;

      vertexData[start + 5].position.x = cos(nextLat) * cos(lon);
      vertexData[start + 5].position.y = sin(nextLat);
      vertexData[start + 5].position.z = cos(nextLat) * sin(lon);
      vertexData[start + 5].position.w = 1.0f;
      vertexData[start + 5].texcoord = { float(lonIndex) / float(kSubdivision),1.0f - float(latIndex + 1) / float(kSubdivision) };
      vertexData[start+5].normal.x = vertexData[start+5].position.x;
      vertexData[start+5].normal.y = vertexData[start+5].position.y;
      vertexData[start+5].normal.z = vertexData[start+5].position.z;

    }
  }

  //===========================================================//
  //=========== Spriteの頂点データを設定、三角形2枚とする ===========//
  //===========================================================//

  VertexData* vertexDataSprite = nullptr;
  vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

  // 1枚目の三角形
  vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };// 左下
  vertexDataSprite[0].texcoord = { 0.0f,1.0f };
  vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };

  vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };// 左上
  vertexDataSprite[1].texcoord = { 0.0f,0.0f };
  vertexDataSprite[1].normal = { 0.0f,0.0f,-1.0f };

  vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };// 右上
  vertexDataSprite[2].texcoord = { 1.0f,1.0f };
  vertexDataSprite[2].normal = { 0.0f,0.0f,-1.0f };

  // 2枚目の三角形
  vertexDataSprite[3].position = { 0.0f,0.0f,0.0f,1.0f };// 左上
  vertexDataSprite[3].texcoord = { 0.0f,0.0f };
  vertexDataSprite[3].normal = { 0.0f,0.0f,-1.0f };

  vertexDataSprite[4].position = { 640.0f,0.0f,0.0f,1.0f };// 右上
  vertexDataSprite[4].texcoord = { 1.0f,0.0f };
  vertexDataSprite[4].normal = { 0.0f,0.0f,-1.0f };

  vertexDataSprite[5].position = { 640.0f,360.0f,0.0f,1.0f };// 右下
  vertexDataSprite[5].texcoord = { 1.0f,1.0f };
  vertexDataSprite[5].normal = { 0.0f,0.0f,-1.0f };

  //==============================================================//
 //============ TransformationMatrix用のリソースを作成 =============//
 //==============================================================//

 // WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
  ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));

  // Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
  ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));

  //データを書き込む
  TransformationMatrix* wvpData = nullptr;

  TransformationMatrix* transformationMatrixDataSprite = nullptr;


  //書き込むためのアドレスを取得
  wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

  transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

  //単位行列を書き込んでおく
  wvpData->WVP = MakeIdentity();
  wvpData->World = MakeIdentity();

  transformationMatrixDataSprite->WVP = MakeIdentity();
  transformationMatrixDataSprite->World = MakeIdentity();



  //======================================
  //======== ViewportとScissor ===========
  //======================================

   // ビューポート
  D3D12_VIEWPORT viewport{};


  //クライアント領域のサイズと一緒にして画面全体に表示
  viewport.Width = kClientWidth;
  viewport.Height = kClientHeight;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;


  //シザー短形
  D3D12_RECT scissorRect{};

  //基本的にビューポートと同じ矩形が構成されるようにする
  scissorRect.left = 0;
  scissorRect.right = kClientWidth;
  scissorRect.top = 0;
  scissorRect.bottom = kClientHeight;


  //Transform変数を作る
  Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

  Transform cameraTransform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };

  // CPUで動かす用のTransformを作る
  Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };


  //ImGuiの初期化。
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(device,
    swapChainDesc.BufferCount,
    rtvDesc.Format,
    srvDescriptorHeap,
    srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
    srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

  bool useMonsterBall = true;

  /*メインループ*/
  MSG msg{};
  //ウィンドウの×ボタンが押されるまでループ
  while (msg.message != WM_QUIT) {
    //Windowにメッセージが来ていたら最優先で処理させる

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else {
      //ゲームの処理

      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();






      //開発用UIの処理
      //ImGui::ShowDemoWindow();
      ImGui::Begin("color");
      ImGui::ColorEdit4("Color", reinterpret_cast<float*>(materialData));
      ImGui::DragFloat3("Move", &transform.translate.x, 0.01f);
      ImGui::DragFloat3("lightDirection", &directionalLightData->direction.x, 0.01f);
      ImGui::DragFloat3("intensity", &directionalLightData->intensity, 0.01f);
      ImGui::Checkbox("useMonsterball", &useMonsterBall);

      ImGui::End();


      //ImGuiの内部コマンドを生成する
      ImGui::Render();

      transform.rotate.y += 0.01f;

      Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
      Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
      Matrix4x4 viewMatrix = Inverse(cameraMatrix);
      Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
      Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

      wvpData->World = worldViewProjectionMatrix;
      wvpData->WVP = worldViewProjectionMatrix;

      Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
      Matrix4x4 viewMatrixSprite = MakeIdentity();
      Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
      Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
      transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
      transformationMatrixDataSprite->World = worldViewProjectionMatrixSprite;


      //ここから書き込むバックバッファのインデックスを取得
      UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
      //TransitionBarrierの設定
      D3D12_RESOURCE_BARRIER barrier{};
      //今回のバリアはTransition
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      //Noneにしておく
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      //バリアを張る対象のリソース。現在のバックバッファに対して行う
      barrier.Transition.pResource = swapChainResources[backBufferIndex];
      //遷移前のResourceState
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      //遷移後のResourceState
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      //TransitionBarrierを張る
      commandList->ResourceBarrier(1, &barrier);


      //描画先のRTVを指定する
      commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
      //指定した色で画面全体をクリアする
      float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };//青っぽい色。RGBAの順
      commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
      // 指定した深度で画面全体をクリアする
      commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

      //描画用のDescriptorHeapの設定
      ID3D12DescriptorHeap* descriptorHeap[] = { srvDescriptorHeap };
      commandList->SetDescriptorHeaps(1, descriptorHeap);

      //========================
      //=== コマンドを積む =====
      //========================
      commandList->RSSetViewports(1, &viewport); //Viewportを設定
      commandList->RSSetScissorRects(1, &scissorRect);  //Scirssorを設定
      //RootSignatureを設定。PSOに設定しているけど別途設定が必要
      commandList->SetGraphicsRootSignature(rootSignature);
      commandList->SetPipelineState(graphicsPipelineState);// PSOを設定
      commandList->IASetVertexBuffers(0, 1, &vertexBufferView);//VBVを設定
      //形状を設定。PSOに設定しているものとはまた別。同じものを設定する
      commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      //マテリアルのCBufferの場所を設定
      commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
      //wvp用のCBufferの場所を設定
      //これはRootParameter[1]に対してCBVの設定を行っている
      commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
      commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());

      // SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
      commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);


      
      // 描画！
      commandList->DrawInstanced(1526, 1, 0, 0);



      // Spriteを常にuvCheckerにする
      commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

      // Spriteの描画。変更が必要なものだけ変更する
      commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);

      // TransformationMatrixCBufferの場所を指定
      commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

      //commandList->DrawInstanced(6, 1, 0, 0);

      //実際のcommandListのImGuiのコマンドを積む
      ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);


      //画面に描く処理はすべて終わり、画面に移すので、状態を遷移
      //今回はRenderTargetからPresentにする
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

      //TransitionBarrierを張る
      commandList->ResourceBarrier(1, &barrier);
      //コマンドリストの内容を確定させる。全てのコマンドを積んでからCloseすること
      hr = commandList->Close();
      assert(SUCCEEDED(hr));

      //GPUにコマンドリストのリストの実行を行わせる
      ID3D12CommandList* commandLists[] = { commandList };
      commandQueue->ExecuteCommandLists(1, commandLists);
      //GPUとOSに画面の交換を行うように通知する
      swapChain->Present(1, 0);
      //Fenceの値の更新
      fenceValue++;
      //GPUがここまでたどり着いたときに、Fenceの値に代入するようにSignalを送る
      commandQueue->Signal(fence, fenceValue);
      //Fenceの値が指定したSignal値にたどり着いているか確認する
      //GetCompletedValueの初期値はFence作成時に渡した初期値
      if (fence->GetCompletedValue() < fenceValue)
      {
        //指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        //イベントを待つ
        WaitForSingleObject(fenceEvent, INFINITE);
      }

      //次のフレーム用のコマンドリストを準備
      hr = commandAllocator->Reset();
      assert(SUCCEEDED(hr));
      hr = commandList->Reset(commandAllocator, nullptr);
      assert(SUCCEEDED(hr));
    }

  }

  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CoUninitialize();


  CloseHandle(fenceEvent);
  fence->Release();

  rtvDescriptorHeap->Release();
  srvDescriptorHeap->Release();
  swapChainResources[0]->Release();
  swapChainResources[1]->Release();
  swapChain->Release();
  commandList->Release();
  commandAllocator->Release();
  commandQueue->Release();
  device->Release();
  useAdapter->Release();
  dxgiFactory->Release();
  materialResource->Release();
  wvpResource->Release();
  vertexResourceSprite->Release();
  transformationMatrixResourceSprite->Release();
  materialResourceSprite->Release();
  lightResource->Release();
#ifdef _DEBUG
  debugController->Release();

#endif

  vertexResource->Release();
  graphicsPipelineState->Release();
  signatureBlob->Release();
  if (errorBlob) {
    errorBlob->Release();
  }
  rootSignature->Release();
  pixelShaderBlob->Release();
  verterShaderBlob->Release();
  textureResource->Release();
  textureResource2->Release();
  depthStencilResource->Release();
  dsvDescriptorHeap->Release();
  

  //リソースチェック
  IDXGIDebug1* debug;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
    debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
    debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    debug->Release();
  }

  return 0;
}


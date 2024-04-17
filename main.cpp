#include<Windows.h>
#include<cstdint>
#include<string>
#include<format>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")







//ウィンドウプロージャー
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  //メッセージ二応じてゲーム固有の処理を行う

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






//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  OutputDebugStringA("Hello,Directx!\n");

  WNDCLASS wc{};
  //ウィンドウプロシージャ
  wc.lpfnWndProc = WindowProc;
  //ウィンドウクラス名
  wc.lpszClassName=L"CG2WindowClass";
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

  MSG msg{};
  //ウィンドウの×ボタンが押されるまでループ
  while (msg.message != WM_QUIT) {
    //Windowにメッセージが来ていたら最優先で処理させる
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else {
      //ゲーム処理
    }
  }

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


  /*D3D12Deviceの生成*/
  ID3D12Device* device = nullptr;
  //機能レベルとログ出力用の文字列
  D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
  };
  return 0;
}

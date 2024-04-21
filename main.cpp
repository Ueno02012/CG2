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

#ifdef _DEBUG
  ID3D12Debug1* debugController = nullptr;

  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    //デバッグレイヤーを有効化
    debugController->EnableDebugLayer();
    //さらにGPU側でもチェックを行えるようにする
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif // _DEBUG



  //ウィンドウを表示する
  ShowWindow(hwnd, SW_SHOW);
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



  //ディスクリプタヒープの生成
  ID3D12DescriptorHeap* rtvDesciptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
  rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビュー用
  rtvDescriptorHeapDesc.NumDescriptors = 2;//ダブルバッファ用二2つ。多くても構わない
  hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDesciptorHeap));
  //ディスクリプタヒープが作れなかったので起動できない
  assert(SUCCEEDED(hr));


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
  D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDesciptorHeap->GetCPUDescriptorHandleForHeapStart();
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

  // FenceのSignalを待つためのイベントを作成する
  HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent != nullptr);




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
      
      
      
      //ここから書き込むバックバッファのインデックスを取得
      UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();


      // TransitionBarrierの設定
      D3D12_RESOURCE_BARRIER barrier{};
      // 今回のバリアはTransion
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      // Noneにしておく
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      // バリアを張る対象のリソース。現在のバックバッファに対して行う
      barrier.Transition.pResource = swapChainResources[backBufferIndex];
      // 遷移前(現在)のResourceState
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      // 遷移後のResourceState
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      // TransitionBarrierを張る
      commandList->ResourceBarrier(1, &barrier);




      //描画先のRTVを指定する
      commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

      //指定した色で画面全体をクリアする
      float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };//青っぽい色。RGBAの順
      commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

      // 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
      // 今回はRenderTargetからPresentにする
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter |= D3D12_RESOURCE_STATE_PRESENT;
      // TransitionBarrierを張る
      commandList->ResourceBarrier(1, &barrier);

      //コマンドリストの内容を確定させる。全てのコマンドを積んでからCloseすること
      hr = commandList->Close();
      assert(SUCCEEDED(hr));

      //GPUにコマンドリストのリストの実行を行わせる
      ID3D12CommandList* commandLists[] = { commandList };
      commandQueue->ExecuteCommandLists(1, commandLists);
      //GPUとOSに画面の交換を行うように通知する
      swapChain->Present(1, 0);

      // Fenceの値の更新
      fenceValue++;
      // GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
      commandQueue->Signal(fence, fenceValue);

      // Fenceの値が指定したSignal値にたどり着いているか確認する
      // GetCompletedValueの初期値葉Fence作成時に渡した初期値
      if (fence->GetCompletedValue() < fenceValue)
      {
        // 指定したSignaにたどりついていないので、たどり着くまで待つようにイベントを設定する
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        // イベントを待つ
        WaitForSingleObject(fenceEvent, INFINITE);
      }

      //次のフレーム用のコマンドリストを準備
      hr = commandAllocator->Reset();
      assert(SUCCEEDED(hr));
      hr = commandList->Reset(commandAllocator, nullptr);
      assert(SUCCEEDED(hr));
    }
  }



  return 0;
}

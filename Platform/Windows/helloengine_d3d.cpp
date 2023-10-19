#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <stdint.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

const uint32_t SCREEN_WIDTH  =  960;
const uint32_t SCREEN_HEIGHT =  480;

// 全局声明
IDXGISwapChain          *g_pSwapchain = nullptr;              // 指向交换链接口的指针
ID3D11Device            *g_pDev       = nullptr;              // 指向 Direct3D 设备接口的指针
ID3D11DeviceContext     *g_pDevcon    = nullptr;              // 指向 Direct3D 设备上下文的指针

ID3D11RenderTargetView  *g_pRTView    = nullptr;

ID3D11InputLayout       *g_pLayout    = nullptr;              // 指向输入布局的指针
ID3D11VertexShader      *g_pVS        = nullptr;              // 指向顶点着色器的指针
ID3D11PixelShader       *g_pPS        = nullptr;              // 指向像素着色器的指针

ID3D11Buffer            *g_pVBuffer   = nullptr;              // 顶点缓冲区

// 顶点缓冲区结构
struct VERTEX {
        XMFLOAT3    Position;
        XMFLOAT4    Color;
};

template<class T>
inline void SafeRelease(T **ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease != nullptr)
    {
        (*ppInterfaceToRelease)->Release();

        (*ppInterfaceToRelease) = nullptr;
    }
}

void CreateRenderTarget() {
    HRESULT hr;
    ID3D11Texture2D *pBackBuffer;

    // 获取指向后台缓冲区的指针
    g_pSwapchain->GetBuffer( 0, __uuidof( ID3D11Texture2D ),
                                 ( LPVOID* )&pBackBuffer );

    // 创建渲染目标视图
    g_pDev->CreateRenderTargetView( pBackBuffer, NULL,
                                          &g_pRTView );
    pBackBuffer->Release();

    // 绑定视图
    g_pDevcon->OMSetRenderTargets( 1, &g_pRTView, NULL );
}

void SetViewPort() {
    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = SCREEN_WIDTH;
    viewport.Height = SCREEN_HEIGHT;

    g_pDevcon->RSSetViewports(1, &viewport);
}

// 加载和准备着色器
void InitPipeline() {
    // 加载并编译两个着色器
    ID3DBlob *VS, *PS;

    D3DReadFileToBlob(L"copy.vso", &VS);
    D3DReadFileToBlob(L"copy.pso", &PS);

    // 将两个着色器封装到着色器对象中
    g_pDev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &g_pVS);
    g_pDev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &g_pPS);

    // 设置着色器对象
    g_pDevcon->VSSetShader(g_pVS, 0, 0);
    g_pDevcon->PSSetShader(g_pPS, 0, 0);

    // 创建输入布局对象
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    g_pDev->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &g_pLayout);
    g_pDevcon->IASetInputLayout(g_pLayout);

    VS->Release();
    PS->Release();
}

// 创建要渲染的形状
void InitGraphics() {
    // 使用 VERTEX 结构创建一个三角形
    VERTEX OurVertices[] =
    {
        {XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)},
        {XMFLOAT3(0.45f, -0.5, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},
        {XMFLOAT3(-0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}
    };


    // 创建顶点缓冲区
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    bd.Usage = D3D11_USAGE_DYNAMIC;                // CPU 和 GPU 的写访问权限
    bd.ByteWidth = sizeof(VERTEX) * 3;             // size
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // 用作顶点缓冲区
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // 允许CPU写入缓冲区

    g_pDev->CreateBuffer(&bd, NULL, &g_pVBuffer);       // create the buffer

    // 将顶点复制到缓冲区中
    D3D11_MAPPED_SUBRESOURCE ms;
    g_pDevcon->Map(g_pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);    // 映射缓冲区
    memcpy(ms.pData, OurVertices, sizeof(VERTEX) * 3);                       // 复制数据
    g_pDevcon->Unmap(g_pVBuffer, NULL);                                      // 取消映射缓冲区
}

// 准备使用的图形资源
HRESULT CreateGraphicsResources(HWND hWnd)
{
    HRESULT hr = S_OK;
    if (g_pSwapchain == nullptr)
    {
        // 创建一个结构体来保存有关交换链的信息
        DXGI_SWAP_CHAIN_DESC scd;

        // 清除结构以供使用
        ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

        scd.BufferCount = 1;                                    // 一个后台缓冲区
        scd.BufferDesc.Width = SCREEN_WIDTH;
        scd.BufferDesc.Height = SCREEN_HEIGHT;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // 使用 32 位颜色
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // 如何使用交换链
        scd.OutputWindow = hWnd;                                // 要使用的窗口
        scd.SampleDesc.Count = 4;                               // 有多少多重样本
        scd.Windowed = TRUE;                                    // 窗口/全屏模式(windowed/full-screen mode)
        scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;     // 允许全屏切换

        const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1,
                                                    D3D_FEATURE_LEVEL_11_0,
                                                    D3D_FEATURE_LEVEL_10_1,
                                                    D3D_FEATURE_LEVEL_10_0,
                                                    D3D_FEATURE_LEVEL_9_3,
                                                    D3D_FEATURE_LEVEL_9_2,
                                                    D3D_FEATURE_LEVEL_9_1};
        D3D_FEATURE_LEVEL FeatureLevelSupported;

        HRESULT hr = S_OK;

        // 使用 scd 结构中的信息创建设备、设备上下文和交换链
        hr = D3D11CreateDeviceAndSwapChain(NULL,
                                      D3D_DRIVER_TYPE_HARDWARE,
                                      NULL,
                                      0,
                                      FeatureLevels,
                                      _countof(FeatureLevels),
                                      D3D11_SDK_VERSION,
                                      &scd,
                                      &g_pSwapchain,
                                      &g_pDev,
                                      &FeatureLevelSupported,
                                      &g_pDevcon);

        if (hr == E_INVALIDARG) {
            hr = D3D11CreateDeviceAndSwapChain(NULL,
                                      D3D_DRIVER_TYPE_HARDWARE,
                                      NULL,
                                      0,
                                      &FeatureLevelSupported,
                                      1,
                                      D3D11_SDK_VERSION,
                                      &scd,
                                      &g_pSwapchain,
                                      &g_pDev,
                                      NULL,
                                      &g_pDevcon);
        }

        if (hr == S_OK) {
            CreateRenderTarget();
            SetViewPort();
            InitPipeline();
            InitGraphics();
        }
    }
    return hr;
}

void DiscardGraphicsResources()
{
    SafeRelease(&g_pLayout);
    SafeRelease(&g_pVS);
    SafeRelease(&g_pPS);
    SafeRelease(&g_pVBuffer);
    SafeRelease(&g_pSwapchain);
    SafeRelease(&g_pRTView);
    SafeRelease(&g_pDev);
    SafeRelease(&g_pDevcon);
}

void RenderFrame()
{
    // 将后台缓冲区清除为深蓝色
    const FLOAT clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    g_pDevcon->ClearRenderTargetView(g_pRTView, clearColor);

    //在此处的后台缓冲区上进行 3D 渲染
    {
        // 选择要显示的顶点缓冲区
        UINT stride = sizeof(VERTEX);
        UINT offset = 0;
        g_pDevcon->IASetVertexBuffers(0, 1, &g_pVBuffer, &stride, &offset);

        // 选择我们正在使用的原始类型
        g_pDevcon->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 将顶点缓冲区绘制到后台缓冲区
        g_pDevcon->Draw(3, 0);
    }

    // 交换后台缓冲区和前台缓冲区
    g_pSwapchain->Present(0, 0);
}

LRESULT CALLBACK WindowProc(HWND hWnd,
                         UINT message,
                         WPARAM wParam,
                         LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR lpCmdLine,
                   int nCmdShow)
{
    HWND hWnd;
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = _T("WindowClass1");

    RegisterClassEx(&wc);

    hWnd = CreateWindowEx(0,
                          _T("WindowClass1"),                   // name of the window class
                          _T("Hello, Engine![Direct 3D]"),      // title of the window
                          WS_OVERLAPPEDWINDOW,                  // window style
                          100,                                  // x-position of the window
                          100,                                  // y-position of the window
                          SCREEN_WIDTH,                         // width of the window
                          SCREEN_HEIGHT,                        // height of the window
                          NULL,                                 // we have no parent window, NULL
                          NULL,                                 // we aren't using menus, NULL
                          hInstance,                            // application handle
                          NULL);                                // used with multiple windows, NULL

    ShowWindow(hWnd, nCmdShow);

    MSG msg;

    while(GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    bool wasHandled = false;

    switch(message)
    {
	case WM_CREATE:
		wasHandled = true;
        break;	

	case WM_PAINT:
		result = CreateGraphicsResources(hWnd);
        	RenderFrame();
		wasHandled = true;
        break;

	case WM_SIZE:
		if (g_pSwapchain != nullptr)
		{
		    DiscardGraphicsResources();
			g_pSwapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
		}
		wasHandled = true;
        break;

	case WM_DESTROY:
		DiscardGraphicsResources();
		PostQuitMessage(0);
		wasHandled = true;
        break;

    case WM_DISPLAYCHANGE:
        InvalidateRect(hWnd, nullptr, false);
        wasHandled = true;
        break;
    }

    if (!wasHandled) { result = DefWindowProc (hWnd, message, wParam, lParam); }
    return result;
}


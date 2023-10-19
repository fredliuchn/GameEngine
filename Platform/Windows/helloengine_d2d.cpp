#include <windows.h>
#include <windowsx.h>
#include <tchar.h>

#include <d2d1.h>

ID2D1Factory			*pFactory = nullptr;
/*画布*/
ID2D1HwndRenderTarget	*pRenderTarget = nullptr;
ID2D1SolidColorBrush	*pLightSlateGrayBrush = nullptr;
ID2D1SolidColorBrush	*pCornflowerBlueBrush = nullptr;

template<class T>
inline void SafeRelease(T **ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease != nullptr)
    {
        (*ppInterfaceToRelease)->Release();

        (*ppInterfaceToRelease) = nullptr;
    }
}

HRESULT CreateGraphicsResources(HWND hWnd)
{
    HRESULT hr = S_OK;
    if (pRenderTarget == nullptr)
    {
        RECT rc;
        GetClientRect(hWnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left,
                        rc.bottom - rc.top);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hWnd, size),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightSlateGray), &pLightSlateGrayBrush);

        }

        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::CornflowerBlue), &pCornflowerBlueBrush);

        }
    }
    return hr;
}

void DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pLightSlateGrayBrush);
    SafeRelease(&pCornflowerBlueBrush);
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

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return -1;

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
                          _T("WindowClass1"),    // name of the window class
                          _T("Hello, Engine![Direct 2D]"),   // title of the window
                          WS_OVERLAPPEDWINDOW,    // window style
                          100,    // x-position of the window
                          100,    // y-position of the window
                          960,    // width of the window
                          540,    // height of the window
                          NULL,    // we have no parent window, NULL
                          NULL,    // we aren't using menus, NULL
                          hInstance,    // application handle
                          NULL);    // used with multiple windows, NULL

    ShowWindow(hWnd, nCmdShow);

    MSG msg;

    while(GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);

        DispatchMessage(&msg);
    }

    CoUninitialize();

    return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    bool wasHandled = false;

    switch(message)
    {
	case WM_CREATE:
		if (FAILED(D2D1CreateFactory(
					D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			result = -1;
		}
		wasHandled = true;
        result = 1;
        break;	

	case WM_PAINT:
	    {
			HRESULT hr = CreateGraphicsResources(hWnd);
			if (SUCCEEDED(hr))
			{
				PAINTSTRUCT ps;
				BeginPaint(hWnd, &ps);

				// 开始构建GPU绘制命令
				pRenderTarget->BeginDraw();

				// 用白色清除背景
				pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

                // 检索绘图区域的大小
                D2D1_SIZE_F rtSize = pRenderTarget->GetSize();

                // 绘制网格背景
                int width = static_cast<int>(rtSize.width);
                int height = static_cast<int>(rtSize.height);

                for (int x = 0; x < width; x += 10)
                {
                    pRenderTarget->DrawLine(
                        D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
                        D2D1::Point2F(static_cast<FLOAT>(x), rtSize.height),
                        pLightSlateGrayBrush,
                        0.5f
                        );
                }

                for (int y = 0; y < height; y += 10)
                {
                    pRenderTarget->DrawLine(
                        D2D1::Point2F(0.0f, static_cast<FLOAT>(y)),
                        D2D1::Point2F(rtSize.width, static_cast<FLOAT>(y)),
                        pLightSlateGrayBrush,
                        0.5f
                        );
                }

                // 画两个矩形
                D2D1_RECT_F rectangle1 = D2D1::RectF(
                     rtSize.width/2 - 50.0f,
                     rtSize.height/2 - 50.0f,
                     rtSize.width/2 + 50.0f,
                     rtSize.height/2 + 50.0f
                     );

                 D2D1_RECT_F rectangle2 = D2D1::RectF(
                     rtSize.width/2 - 100.0f,
                     rtSize.height/2 - 100.0f,
                     rtSize.width/2 + 100.0f,
                     rtSize.height/2 + 100.0f
                     );

                // 绘制一个填充的矩形
                pRenderTarget->FillRectangle(&rectangle1, pLightSlateGrayBrush);

                // 只画一个矩形的轮廓
                pRenderTarget->DrawRectangle(&rectangle2, pCornflowerBlueBrush);

				// 结束 GPU 绘制命令构建
				hr = pRenderTarget->EndDraw();
				if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
				{
					DiscardGraphicsResources();
				}

				EndPaint(hWnd, &ps);
			}
	    }
		wasHandled = true;
        break;

	case WM_SIZE:
		if (pRenderTarget != nullptr)
		{
			RECT rc;
			GetClientRect(hWnd, &rc);

			D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

			pRenderTarget->Resize(size);
		}
		wasHandled = true;
        break;

	case WM_DESTROY:
		DiscardGraphicsResources();
		if (pFactory) {pFactory->Release(); pFactory=nullptr; }
		PostQuitMessage(0);
        result = 1;
		wasHandled = true;
        break;

    case WM_DISPLAYCHANGE:
        InvalidateRect(hWnd, nullptr, false);
        wasHandled = true;
        break;
    }

    // 处理 switch 语句没有处理的任何消息
    if (!wasHandled) { result = DefWindowProc (hWnd, message, wParam, lParam); }
    return result;
}


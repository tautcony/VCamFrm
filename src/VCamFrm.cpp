/* VCamFrm
Copyright (C) 2018  TautCony

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma comment(lib,"gdiplus.lib")
#include <Windows.h>
#include <Tchar.h>
#include <string>
#include <Gdiplus.h>
#include "VCamFrm.h"
#include "Utils.h"

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
vcam_param p1;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_VCAMFRM, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    const auto hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VCAMFRM));

    uvc_vcam_t uvc1;
    uvc1.pid = 0xcc10;
    uvc1.vid = 0xbb10;
    uvc1.manu_fact = "TC Tech Inc.";
    uvc1.product = "Virtual TC Camera";
    uvc1.frame_callback = frame_callback;
    uvc1.param = &p1;
    const auto vcam1 = vcam_create(&uvc1);

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    vcam_destroy(vcam1);
    p1.bitmap.reset();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}


//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VCAMFRM));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_VCAMFRM);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   SetWindowPos(hWnd, nullptr, 0, 0, 640, 400, 0);

   return TRUE;
}

std::wstring OpenFileDialog(HWND hWnd)
{
    OPENFILENAME ofn;
    TCHAR file_name[260] = { 0 };

    ZeroMemory(&ofn, sizeof ofn);
    ofn.lStructSize     = sizeof ofn;
    ofn.hwndOwner       = hWnd;
    ofn.lpstrFilter     = _T("图片(*.BMP, *.GIF, *.JPEG, *.PNG, *.TIFF)\0*.BMP;*.GIF;*.JPEG;*.JPG;*.PNG;*.TIFF\0全部文件(*.*)\0*.*\0\0");
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = file_name;
    ofn.nMaxFile        = sizeof file_name / sizeof TCHAR;
    ofn.lpstrFileTitle  = nullptr;
    ofn.nMaxFileTitle   = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        return std::wstring(file_name);
    }
    return _T("");
}


//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_LOAD:
            {
                auto image_path = OpenFileDialog(hWnd);
                change_image(p1, image_path);
                RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
                break;
            }
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            EnterCriticalSection(&p1.cs);
            PAINTSTRUCT ps;
            const auto hdc = BeginPaint(hWnd, &ps);
            if (p1.bitmap != nullptr)
            {
                Gdiplus::Graphics g(hdc);
                RECT client_area;
                GetClientRect(hWnd, &client_area);
                const auto resized_img = resize(p1.bitmap, client_area.right, client_area.bottom);
                g.DrawImage(resized_img.get(), 0, 0, client_area.right, client_area.bottom);
            }
            EndPaint(hWnd, &ps);
            LeaveCriticalSection(&p1.cs);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

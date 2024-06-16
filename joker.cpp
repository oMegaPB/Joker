#define UNICODE
#define _UNICODE
#define JOKER_GIF 101
#define JOKER_SOUND 102

#include <iostream>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <mmsystem.h> // Multimedia API
#include <vector>
#include <thread>
#include <sstream>    // For std::stringstream

#pragma comment (lib, "Gdiplus.lib")
#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;

ULONG_PTR gdiplusToken;
Image* gifImage = nullptr;
std::vector<UINT> frameDelays;
UINT frameCount = 0;
UINT currentFrame = 0;
UINT_PTR timerId = 1;
HWND actual_hwnd;
HINSTANCE actual_hinstance;
int left = 0, top = 0, x = 13, y = 13;

void InitGDIPlus() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

void ShutdownGDIPlus() {
    GdiplusShutdown(gdiplusToken);
}

void MoveWindow() {
    left += x;
    top += y;
    MoveWindow(actual_hwnd, left, top, 270, 270, TRUE);
    if (top + 270 >= GetSystemMetrics(SM_CYSCREEN) || top < 0) {
        y *= -1;
    }
    if (left + 270 >= GetSystemMetrics(SM_CXSCREEN) || left <= 0)
    {
        x *= -1;
    }
}

bool LoadGIF() {
    HRSRC hResInfo = FindResource(actual_hinstance, MAKEINTRESOURCE(JOKER_GIF), RT_RCDATA);
    HGLOBAL hResData = LoadResource(actual_hinstance, hResInfo);
    LPVOID pvRes = LockResource(hResData);
    DWORD dwSize = SizeofResource(actual_hinstance, hResInfo);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dwSize);
    LPVOID pvMem = GlobalLock(hMem);

    memcpy(pvMem, pvRes, dwSize);
    IStream* pStream = NULL;
    CreateStreamOnHGlobal(hMem, FALSE, &pStream);

    gifImage = new Image(pStream);

    GUID pageGuid = FrameDimensionTime;
    frameCount = gifImage->GetFrameCount(&pageGuid);

    PropertyItem* propertyItem;
    UINT propertySize = gifImage->GetPropertyItemSize(PropertyTagFrameDelay);
    propertyItem = (PropertyItem*)malloc(propertySize);
    gifImage->GetPropertyItem(PropertyTagFrameDelay, propertySize, propertyItem);

    frameDelays.resize(frameCount);
    for (UINT i = 0; i < frameCount; ++i) {
        frameDelays[i] = ((UINT*)propertyItem->value)[i] * 10; // Convert to milliseconds
    }
    free(propertyItem);

    return true;
}

void UpdateFrame() {
    if (gifImage) {
        currentFrame = (currentFrame + 1) % frameCount;
        GUID pageGuid = FrameDimensionTime;
        gifImage->SelectActiveFrame(&pageGuid, currentFrame);
        InvalidateRect(actual_hwnd, NULL, FALSE);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        LoadGIF();
        SetTimer(hwnd, timerId, frameDelays[currentFrame], NULL);
        break;

    case WM_TIMER:
        if (wParam == timerId) {
            UpdateFrame();
            MoveWindow();
            SetTimer(hwnd, timerId, frameDelays[currentFrame], NULL);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Graphics graphics(hdc);
        if (gifImage) {
            graphics.DrawImage(gifImage, 0, 0);
        }
        EndPaint(hwnd, &ps);
    }
    break;

    case WM_DESTROY:
        if (gifImage) {
            delete gifImage;
        }
        KillTimer(hwnd, timerId);
        PostQuitMessage(0);
        break;
    
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    
    case WM_CLOSE:
        return DefWindowProc(hwnd, msg, wParam, lParam);

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void PlaySoundEndless(HINSTANCE hInstance) {
    DWORD dwSize;
    HRSRC hResInfo = FindResource(hInstance, MAKEINTRESOURCE(JOKER_SOUND), RT_RCDATA);
    HGLOBAL hResData = LoadResource(hInstance, hResInfo);
    dwSize = SizeofResource(hInstance, hResInfo);
    BYTE* pBuffer = (BYTE*)LockResource(hResData);
    while (1) {
        sndPlaySound((LPCWSTR)pBuffer, SND_MEMORY | SND_ASYNC);
        Sleep(10000);
    }
    FreeResource(hResData);
}

// Main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    actual_hinstance = hInstance;
    FreeConsole();
    InitGDIPlus();

    MSG msg;
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"JokerApp";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,          // Optional window styles.
        wc.lpszClassName,       // Window class
        NULL,                   // Window text
        WS_POPUP | WS_VISIBLE,  // Window style
        0, 0, 270, 270,         // Size and position
        NULL,                   // Parent window
        NULL,                   // Menu
        hInstance,              // Instance handle
        NULL                    // Additional application data
    );
    actual_hwnd = hwnd;
    ShowWindow(hwnd, nCmdShow);

    std::thread t1(PlaySoundEndless, hInstance);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ShutdownGDIPlus();

    return (int)msg.wParam;
}
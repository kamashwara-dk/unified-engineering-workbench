// main.cpp -- Win32 + WebView2 host for the Unified Engineering Workbench.
// No business logic: creates a window, starts the server, hosts WebView2.
// Compiled on Windows only (WIN32 guard in CMakeLists.txt).
//
// Startup: CoInitializeEx -> start Server -> check WebView2 runtime ->
//   [present] create window + WebView2; [absent] MessageBox + open browser.
// On NavigationCompleted: record launch_to_ready_ms.
// WM_DESTROY: stop server, PostQuitMessage.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

// WRL headers: client.h for ComPtr, implements.h for Callback<>.
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/event.h>    // Microsoft::WRL::Callback<>

#include "WebView2.h"

#include "workbench/server.hpp"
#include "workbench/token.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;

// Measure from process start for launch_to_ready_ms.
static const auto kProcessStart = std::chrono::steady_clock::now();

static workbench::Server* gServer  = nullptr;
static HWND               gHwnd    = nullptr;
static bool               gHasWV2  = false;

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    const auto slash = p.rfind(L'\\');
    return (slash != std::wstring::npos) ? p.substr(0, slash) : L".";
}

static void FatalBox(const wchar_t* msg) {
    MessageBoxW(nullptr, msg, L"Workbench Error", MB_OK | MB_ICONERROR);
}

// Forward declaration.
static void CreateWebView(
    HWND hwnd, const std::wstring& initialUrl, workbench::Server* srv);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        auto* ctrl = reinterpret_cast<ICoreWebView2Controller*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (ctrl) {
            RECT rc{}; GetClientRect(hwnd, &rc);
            ctrl->put_Bounds(rc);
        }
        return 0;
    }
    case WM_DESTROY:
        if (gServer) { gServer->stop(); }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static void CreateWebView(
    HWND hwnd, const std::wstring& initialUrl, workbench::Server* srv)
{
    // WebView2 user-data folder under %LOCALAPPDATA%\Workbench (not next to exe).
    wchar_t appData[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData);
    const std::wstring udFolder =
        std::wstring(appData) + L"\\Workbench\\WebView2UserData";

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, udFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, initialUrl, srv]
            (HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(hr) || !env) {
                    FatalBox(L"WebView2 environment creation failed.");
                    return hr ? hr : E_FAIL;
                }
                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, initialUrl, srv]
                        (HRESULT hr2, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(hr2) || !ctrl) {
                                FatalBox(L"WebView2 controller creation failed.");
                                return hr2 ? hr2 : E_FAIL;
                            }
                            ctrl->AddRef();
                            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(ctrl));
                            RECT rc{}; GetClientRect(hwnd, &rc);
                            ctrl->put_Bounds(rc);

                            ComPtr<ICoreWebView2> wv;
                            ctrl->get_CoreWebView2(&wv);
                            if (!wv) {
                                FatalBox(L"Failed to get CoreWebView2.");
                                return E_FAIL;
                            }

                            // On first NavigationCompleted: record launch-to-ready time.
                            EventRegistrationToken tok{};
                            wv->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [srv]
                                    (ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)
                                    -> HRESULT {
                                        using namespace std::chrono;
                                        const int64_t ms = duration_cast<milliseconds>(
                                            steady_clock::now() - kProcessStart).count();
                                        srv->set_launch_to_ready_ms(ms);
                                        return S_OK;
                                    }).Get(), &tok);

                            wv->Navigate(initialUrl.c_str());
                            ShowWindow(hwnd, SW_SHOWNORMAL);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // Single instance enforcement: second launch restores existing window and exits cleanly.
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\PASS_UnifiedWorkbench_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingHwnd = FindWindowW(L"WorkbenchHost", nullptr);
        if (existingHwnd) {
            ShowWindow(existingHwnd, SW_RESTORE);
            SetForegroundWindow(existingHwnd);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        FatalBox(L"CoInitializeEx failed.");
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 1;
    }

    // Resolve ui/ dir relative to exe.
    const std::wstring exeDir = ExeDir();
    const std::wstring uiDirW = exeDir + L"\\app\\ui";
    // Convert wide path to UTF-8 for ServerConfig.
    int needed = WideCharToMultiByte(CP_UTF8, 0, uiDirW.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string  uiDir(static_cast<std::size_t>(needed > 0 ? needed - 1 : 0), '\0');
    if (needed > 0) WideCharToMultiByte(CP_UTF8, 0, uiDirW.c_str(), -1, uiDir.data(), needed, nullptr, nullptr);

    // Start the HTTP server.
    workbench::ServerConfig cfg;
    cfg.ui_dir = uiDir;
    cfg.token  = workbench::generate_token();
    auto server = std::make_unique<workbench::Server>(std::move(cfg));
    gServer = server.get();
    uint16_t port = 0;
    try { port = server->start(); }
    catch (const std::exception& ex) {
        const std::string what(ex.what());
        std::wstring msg = L"Failed to start server:\n";
        msg.append(what.begin(), what.end());
        msg += L"\n\nCheck that no other instance is running.";
        FatalBox(msg.c_str());
        CoUninitialize();
        return 1;
    }

    const std::string urlA = "http://127.0.0.1:" + std::to_string(port)
        + "/?t=" + server->token();
    int wneeded = MultiByteToWideChar(CP_UTF8, 0, urlA.c_str(), -1, nullptr, 0);
    std::wstring urlW(static_cast<std::size_t>(wneeded > 0 ? wneeded - 1 : 0), L'\0');
    if (wneeded > 0) MultiByteToWideChar(CP_UTF8, 0, urlA.c_str(), -1, urlW.data(), wneeded);

    // Check for WebView2 runtime.
    wchar_t* verStr = nullptr;
    gHasWV2 = SUCCEEDED(
        GetAvailableCoreWebView2BrowserVersionString(nullptr, &verStr));
    CoTaskMemFree(verStr);

    if (!gHasWV2) {
        MessageBoxW(nullptr,
            L"The Microsoft WebView2 Runtime is not installed.\n\n"
            L"Workbench has opened in your default browser instead.\n\n"
            L"Install the runtime: https://developer.microsoft.com/microsoft-edge/webview2\n\n"
            L"Close this window to quit Workbench.",
            L"Workbench -- WebView2 Runtime Required", MB_OK | MB_ICONINFORMATION);
        ShellExecuteW(nullptr, L"open", urlW.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    // Register window class.
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.style        = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc  = WndProc;
    wc.hInstance    = hInst;
    wc.lpszClassName = L"WorkbenchHost";
    wc.hCursor      = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    const wchar_t* title = gHasWV2
        ? L"Unified Engineering Workbench"
        : L"Workbench (browser mode) -- close to quit";
    gHwnd = CreateWindowExW(0, L"WorkbenchHost", title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, hInst, nullptr);
    if (!gHwnd) { FatalBox(L"CreateWindow failed."); CoUninitialize(); return 1; }

    if (gHasWV2) {
        // Window is shown inside CreateWebView callback (after WebView2 is ready).
        CreateWebView(gHwnd, urlW, server.get());
    } else {
        ShowWindow(gHwnd, SW_SHOWNORMAL);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

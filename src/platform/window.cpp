#include "platform/window.h"

#include <stdexcept>

namespace m0 {

namespace {
constexpr wchar_t kClassName[] = L"M0Window";
}

Window::Window(const wchar_t* title, uint32_t clientWidth, uint32_t clientHeight) {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Window::wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);  // fails harmlessly if already registered

    // The requested size is the client area; grow the outer rect to fit borders and title bar.
    RECT rect = {0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExW(0, kClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                            instance, this);
    if (!hwnd_) throw std::runtime_error("CreateWindowExW failed");

    RECT client;
    GetClientRect(hwnd_, &client);
    width_ = static_cast<uint32_t>(client.right - client.left);
    height_ = static_cast<uint32_t>(client.bottom - client.top);
    resized_ = false;

    ShowWindow(hwnd_, SW_SHOW);
}

Window::~Window() {
    if (hwnd_) DestroyWindow(hwnd_);
}

bool Window::pumpMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !closed_;
}

bool Window::consumeResized() {
    bool r = resized_;
    resized_ = false;
    return r;
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        // Messages arrive before CreateWindowExW returns, so store the HWND here, not after it.
        auto* self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Window::handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        width_ = LOWORD(lp);
        height_ = HIWORD(lp);
        resized_ = true;
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        closed_ = true;
        hwnd_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace m0

#pragma once

#include <windows.h>

#include <cstdint>

namespace m0 {

// A Win32 window. The main loop polls it for resizes and for being closed.
class Window {
public:
    Window(const wchar_t* title, uint32_t clientWidth, uint32_t clientHeight);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Handles all pending messages. Returns false once the window has been closed.
    bool pumpMessages();

    // True once after the client area changed size.
    bool consumeResized();

    HWND hwnd() const { return hwnd_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    bool minimized() const { return width_ == 0 || height_ == 0; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);

    HWND hwnd_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    bool resized_ = false;
    bool closed_ = false;
};

}  // namespace m0

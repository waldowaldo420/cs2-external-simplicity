#pragma once

#include <Windows.h>
#include <dwmapi.h>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

static HANDLE g_hOut{};
static HANDLE g_hIn{};

namespace console
{
    static void init()
    {
        g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        g_hIn = GetStdHandle(STD_INPUT_HANDLE);

        DWORD outMode{};
        GetConsoleMode(g_hOut, &outMode);

        SetConsoleMode(
            g_hOut,
            outMode |
            ENABLE_VIRTUAL_TERMINAL_PROCESSING |
            ENABLE_PROCESSED_OUTPUT
        );

        DWORD inMode{};
        GetConsoleMode(g_hIn, &inMode);

        SetConsoleMode(
            g_hIn,
            inMode |
            ENABLE_EXTENDED_FLAGS |
            ENABLE_QUICK_EDIT_MODE
        );

        CONSOLE_FONT_INFOEX cfi{};
        cfi.cbSize = sizeof(cfi);

        GetCurrentConsoleFontEx(g_hOut, FALSE, &cfi);

        wcscpy_s(cfi.FaceName, L"Cascadia Mono");
        cfi.dwFontSize.Y = 18;
        cfi.FontWeight = FW_NORMAL;

        SetCurrentConsoleFontEx(g_hOut, FALSE, &cfi);

        CONSOLE_SCREEN_BUFFER_INFOEX csbi{};
        csbi.cbSize = sizeof(csbi);

        GetConsoleScreenBufferInfoEx(g_hOut, &csbi);

        csbi.ColorTable[0] = RGB(8, 10, 18);
        csbi.ColorTable[1] = RGB(55, 65, 185);
        csbi.ColorTable[2] = RGB(120, 140, 255);
        csbi.ColorTable[7] = RGB(220, 225, 240);

        SetConsoleScreenBufferInfoEx(g_hOut, &csbi);

        HWND hwnd = GetConsoleWindow();

        if (hwnd)
        {
            LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);

            SetWindowLongW(
                hwnd,
                GWL_EXSTYLE,
                ex | WS_EX_DLGMODALFRAME | WS_EX_LAYERED
            );

            LONG style = GetWindowLongW(hwnd, GWL_STYLE);

            style &= ~(
                WS_MAXIMIZEBOX |
                WS_SIZEBOX |
                WS_THICKFRAME
                );

            SetWindowLongW(hwnd, GWL_STYLE, style);

            SetWindowPos(
                hwnd,
                nullptr,
                0, 0, 0, 0,
                SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOZORDER |
                SWP_FRAMECHANGED
            );

            SetLayeredWindowAttributes(
                hwnd,
                0,
                245,
                LWA_ALPHA
            );

            DWM_WINDOW_CORNER_PREFERENCE corner =
                DWMWCP_ROUND;

            DwmSetWindowAttribute(
                hwnd,
                DWMWA_WINDOW_CORNER_PREFERENCE,
                &corner,
                sizeof(corner)
            );

            COLORREF caption = RGB(12, 14, 24);

            DwmSetWindowAttribute(
                hwnd,
                DWMWA_CAPTION_COLOR,
                &caption,
                sizeof(caption)
            );

            COLORREF border = RGB(70, 90, 255);

            DwmSetWindowAttribute(
                hwnd,
                DWMWA_BORDER_COLOR,
                &border,
                sizeof(border)
            );

            COLORREF text = RGB(180, 190, 255);

            DwmSetWindowAttribute(
                hwnd,
                DWMWA_TEXT_COLOR,
                &text,
                sizeof(text)
            );

            SMALL_RECT rect
            {
                0,
                0,
                100,
                32
            };

            SetConsoleWindowInfo(
                g_hOut,
                TRUE,
                &rect
            );

            COORD size
            {
                101,
                33
            };

            SetConsoleScreenBufferSize(
                g_hOut,
                size
            );

            RECT rc{};
            GetWindowRect(hwnd, &rc);

            const int width =
                rc.right - rc.left;

            const int height =
                rc.bottom - rc.top;

            const int screenW =
                GetSystemMetrics(SM_CXSCREEN);

            const int screenH =
                GetSystemMetrics(SM_CYSCREEN);

            SetWindowPos(
                hwnd,
                HWND_TOP,
                (screenW - width) / 2,
                (screenH - height) / 2,
                0,
                0,
                SWP_NOSIZE
            );
        }

        CONSOLE_CURSOR_INFO ci{};
        ci.dwSize = 1;
        ci.bVisible = FALSE;

        SetConsoleCursorInfo(g_hOut, &ci);

        printf("\x1b[2J");
        printf("\x1b[H");

        system("color 0F");
    }
}
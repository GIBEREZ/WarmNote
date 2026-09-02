#include "../include/Screenshot.h"

#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Core.h"

namespace Screenshot
{
    int GetDisplayData(DisplayContext& display_context)
    {
        display_context = {};

        const HWND window = GetForegroundWindow();
        if (window == nullptr || IsWindow(window) == FALSE)
        {
            WARM_LOGE("获取显示数据失败：当前前台窗口无效。");
            return -1;
        }

        const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        if (monitor == nullptr)
        {
            WARM_LOGE("获取显示数据失败：无法获取游戏窗口所在显示器，错误码：%lu。", GetLastError());
            return -1;
        }

        MONITORINFO monitor_info = {};
        monitor_info.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoW(monitor, &monitor_info) == FALSE)
        {
            WARM_LOGE("获取显示数据失败：无法获取显示器信息，错误码：%lu。", GetLastError());
            return -1;
        }

        RECT client_rect = {};
        if (GetClientRect(window, &client_rect) == FALSE)
        {
            WARM_LOGE("获取显示数据失败：无法获取游戏客户区，错误码：%lu。", GetLastError());
            return -1;
        }

        const int monitor_width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
        const int monitor_height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
        const int nikki_width = client_rect.right - client_rect.left;
        const int nikki_height = client_rect.bottom - client_rect.top;
        if (monitor_width <= 0 || monitor_height <= 0 || nikki_width <= 0 || nikki_height <= 0)
        {
            WARM_LOGE("获取显示数据失败：显示器或游戏客户区尺寸无效。");
            return -1;
        }

        const UINT dpi = GetDpiForWindow(window);
        if (dpi == 0)
        {
            WARM_LOGE("获取显示数据失败：无法获取游戏窗口 DPI，错误码：%lu。", GetLastError());
            return -1;
        }

        display_context.Monitor_Width = monitor_width;
        display_context.Monitor_Height = monitor_height;
        display_context.NIKKI_Width = nikki_width;
        display_context.NIKKI_Height = nikki_height;
        display_context.dpi = dpi;
        display_context.scale = static_cast<double>(dpi) / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
        return 0;
    }

    int CaptureClientArea(HWND window, std::vector<std::uint8_t>& buffer, int& width, int& height)
    {
        buffer.clear();
        width = 0;
        height = 0;

        if (window == nullptr || !IsWindow(window))
        {
            WARM_LOGE("目标窗口无效。");
            return -1;
        }

        RECT client_rect{};
        if (!GetClientRect(window, &client_rect))
        {
            WARM_LOGE("GetClientRect 调用失败，Win32 错误码：%lu", GetLastError());
            return -1;
        }

        const int captured_width = client_rect.right - client_rect.left;
        const int captured_height = client_rect.bottom - client_rect.top;
        if (captured_width <= 0 || captured_height <= 0)
        {
            WARM_LOGE("目标窗口的客户区尺寸为空。");
            return -1;
        }

        HDC window_dc = GetDC(window);
        if (window_dc == nullptr)
        {
            WARM_LOGE("GetDC 调用失败，Win32 错误码：%lu", GetLastError());
            return -1;
        }

        HDC memory_dc = CreateCompatibleDC(window_dc);
        if (memory_dc == nullptr)
        {
            WARM_LOGE("CreateCompatibleDC 调用失败，Win32 错误码：%lu", GetLastError());
            ReleaseDC(window, window_dc);
            return -1;
        }

        HBITMAP bitmap = CreateCompatibleBitmap(window_dc, captured_width, captured_height);
        if (bitmap == nullptr)
        {
            WARM_LOGE("CreateCompatibleBitmap 调用失败，Win32 错误码：%lu", GetLastError());
            DeleteDC(memory_dc);
            ReleaseDC(window, window_dc);
            return -1;
        }

        const HGDIOBJ previous_object = SelectObject(memory_dc, bitmap);
        if (previous_object == nullptr || previous_object == HGDI_ERROR)
        {
            WARM_LOGE("SelectObject 调用失败，Win32 错误码：%lu", GetLastError());
            DeleteObject(bitmap);
            DeleteDC(memory_dc);
            ReleaseDC(window, window_dc);
            return -1;
        }

        constexpr UINT print_window_flags = PW_CLIENTONLY | 0x00000002U;
        BOOL captured = PrintWindow(window, memory_dc, print_window_flags);
        const DWORD print_window_error = captured ? ERROR_SUCCESS : GetLastError();

        if (!captured)
        {
            if (IsIconic(window))
            {
                WARM_LOGE("PrintWindow 调用失败，且目标窗口已最小化。Win32 错误码：%lu", print_window_error);
            }
            else
            {
                POINT client_origin{0, 0};
                if (!ClientToScreen(window, &client_origin))
                {
                    WARM_LOGE("ClientToScreen 调用失败，Win32 错误码：%lu", GetLastError());
                }
                else
                {
                    HDC screen_dc = GetDC(nullptr);
                    if (screen_dc == nullptr)
                    {
                        WARM_LOGE("获取屏幕 DC 失败，Win32 错误码：%lu", GetLastError());
                    }
                    else
                    {
                        captured = BitBlt(
                            memory_dc,
                            0,
                            0,
                            captured_width,
                            captured_height,
                            screen_dc,
                            client_origin.x,
                            client_origin.y,
                            SRCCOPY | CAPTUREBLT);
                        if (!captured)
                        {
                            WARM_LOGE("PrintWindow 与 BitBlt 均调用失败，Win32 错误码：%lu", GetLastError());
                        }
                        ReleaseDC(nullptr, screen_dc);
                    }
                }
            }
        }

        SelectObject(memory_dc, previous_object);

        if (!captured)
        {
            DeleteObject(bitmap);
            DeleteDC(memory_dc);
            ReleaseDC(window, window_dc);
            return -1;
        }

        BITMAPINFO bitmap_info{};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = captured_width;
        bitmap_info.bmiHeader.biHeight = -captured_height;
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;

        const std::size_t pixel_data_size = static_cast<std::size_t>(captured_width) * static_cast<std::size_t>(
            captured_height) * 4U;
        std::vector<std::uint8_t> captured_buffer(pixel_data_size);

        const int copied_scan_lines = GetDIBits(
            window_dc,
            bitmap,
            0,
            static_cast<UINT>(captured_height),
            captured_buffer.data(),
            &bitmap_info,
            DIB_RGB_COLORS);
        const DWORD get_dibits_error = copied_scan_lines == captured_height ? ERROR_SUCCESS : GetLastError();

        DeleteObject(bitmap);
        DeleteDC(memory_dc);
        ReleaseDC(window, window_dc);

        if (copied_scan_lines != captured_height)
        {
            WARM_LOGE("GetDIBits 调用失败，Win32 错误码：%lu", get_dibits_error);
            return -1;
        }

        buffer = std::move(captured_buffer);
        width = captured_width;
        height = captured_height;
        return 0;
    }

    int CaptureClientArea(HWND window, std::vector<std::uint8_t>& buffer, int& width, int& height, const std::filesystem::path& output_path)
    {
        if (CaptureClientArea(window, buffer, width, height) != 0)
        {
            return -1;
        }

        std::error_code filesystem_error;
        std::filesystem::path output_directory = output_path;
        if (output_directory.empty())
        {
            output_directory = std::filesystem::current_path(filesystem_error);
            if (filesystem_error)
            {
                WARM_LOGE("获取当前工作目录失败，错误码：%d", filesystem_error.value());
                return -1;
            }
        }

        if (!std::filesystem::exists(output_directory, filesystem_error))
        {
            if (!std::filesystem::create_directories(output_directory, filesystem_error))
            {
                WARM_LOGE("创建截图保存目录失败：%ls，错误码：%d", output_directory.c_str(), filesystem_error.value());
                return -1;
            }
        }
        else if (filesystem_error || !std::filesystem::is_directory(output_directory, filesystem_error))
        {
            WARM_LOGE("截图保存路径不是有效目录：%ls", output_directory.c_str());
            return -1;
        }

        SYSTEMTIME local_time{};
        GetLocalTime(&local_time);

        std::wostringstream filename;
        filename << L"WarmNote_"
            << std::setfill(L'0')
            << std::setw(4) << local_time.wYear
            << std::setw(2) << local_time.wMonth
            << std::setw(2) << local_time.wDay
            << L'_'
            << std::setw(2) << local_time.wHour
            << std::setw(2) << local_time.wMinute
            << std::setw(2) << local_time.wSecond
            << L'_'
            << std::setw(3) << local_time.wMilliseconds
            << L".bmp";

        const std::filesystem::path image_path = output_directory / filename.str();

        BITMAPINFOHEADER bitmap_header{};
        bitmap_header.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_header.biWidth = width;
        bitmap_header.biHeight = -height;
        bitmap_header.biPlanes = 1;
        bitmap_header.biBitCount = 32;
        bitmap_header.biCompression = BI_RGB;
        bitmap_header.biSizeImage = static_cast<DWORD>(buffer.size());

        BITMAPFILEHEADER file_header{};
        file_header.bfType = 0x4D42;
        file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file_header.bfSize = file_header.bfOffBits + static_cast<DWORD>(buffer.size());

        std::ofstream output(image_path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            WARM_LOGE("打开截图文件失败：%ls", image_path.c_str());
            return -1;
        }

        output.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
        output.write(reinterpret_cast<const char*>(&bitmap_header), sizeof(bitmap_header));
        output.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

        if (!output)
        {
            WARM_LOGE("写入截图文件失败：%ls", image_path.c_str());
            return -1;
        }

        return 0;
    }

    int BufferImageManager::RunScreenshot(HWND window)
    {
        return 0;
    }
}

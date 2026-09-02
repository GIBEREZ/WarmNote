#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <Windows.h>
#include <opencv2/core/mat.hpp>

struct DisplayContext;

namespace Screenshot
{
    int GetDisplayData(DisplayContext& display_context);
    int CaptureClientArea(HWND window, std::vector<std::uint8_t>& buffer, int& width, int& height);
    int CaptureClientArea(HWND window, std::vector<std::uint8_t>& buffer, int& width, int& height, const std::filesystem::path& output_path);

    class BufferImageManager
    {
    public:
        BufferImageManager(cv::Mat* mat);

        int RunScreenshot(HWND window);

        [[nodiscard]] const std::uint8_t* Data() const noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;
        [[nodiscard]] int Width() const noexcept;
        [[nodiscard]] int Height() const noexcept;
        [[nodiscard]] int Stride() const noexcept;
        [[nodiscard]] bool Empty() const noexcept;

    private:
        std::vector<std::uint8_t> buffer;
        cv::Mat* mat;
        int width = 0;
        int height = 0;
    };
}

//
// Created by 30388 on 2026/9/2.
//

#include "TremoloLayer.h"

#include <string_view>

#include <Windows.h>

namespace
{
    constexpr std::string_view FluteKeyLayout = "QWERTYUASDFGHJ";

    bool ParseTremoloPayload(const std::string& payload, int& keycode)
    {
        std::vector<int> keycodes;
        if (!Control::ParseKeyCodePayload(payload, keycodes) || keycodes.size() != 1)
        {
            return false;
        }

        keycode = keycodes.front();
        return FluteKeyLayout.find(static_cast<char>(keycode)) != std::string_view::npos;
    }
}

TremoloLayer::TremoloLayer() = default;

TremoloLayer::~TremoloLayer() = default;

int TremoloLayer::Validate(const std::string& payload, int duration_ms) const
{
    int keycode = 0;
    return duration_ms >= 0 && ParseTremoloPayload(payload, keycode) ? 0 : -1;
}

int TremoloLayer::Execute(const std::string& payload, int duration_ms)
{
    int keycode = 0;
    if (duration_ms < 0 || !ParseTremoloPayload(payload, keycode))
    {
        return -1;
    }

    return Control::KeyCodeTap(std::vector<int>{VK_SHIFT, keycode}, duration_ms);
}

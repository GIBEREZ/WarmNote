//
// Created by 30388 on 2026/9/2.
//

#include "KeyCodeInputLayer.h"

KeyCodeInputLayer::KeyCodeInputLayer() = default;

KeyCodeInputLayer::~KeyCodeInputLayer() = default;

int KeyCodeInputLayer::Validate(const std::string& payload, int duration_ms) const
{
    std::vector<int> keycodes;
    return duration_ms >= 0 && Control::ParseKeyCodePayload(payload, keycodes) ? 0 : -1;
}

int KeyCodeInputLayer::Execute(const std::string& payload, int duration_ms)
{
    std::vector<int> keycodes;
    if (duration_ms < 0 || !Control::ParseKeyCodePayload(payload, keycodes))
    {
        return -1;
    }

    return Control::KeyCodeTap(keycodes, duration_ms);
}

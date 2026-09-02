#include "Control.h"
#include "action_layer_declaration.h"
#include "Core.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ranges>
#include <thread>

#include <Windows.h>

namespace Control
{
    struct BuiltinActionLayerEntry
    {
        const char* ActionType;
        ActionLayerCreator Creator;
    };

    // 表项由warmnote_add_action_layer在CMake配置阶段自动生成。
    constexpr BuiltinActionLayerEntry BuiltinActionLayers[] =
    {
#include "action_layer_registry.h"
    };

    ActionLayer::ActionLayer() = default;
    ActionLayer::~ActionLayer() = default;

    void ActionLayer::Reset() noexcept
    {
    }

    int ActionLayerRegistry::Register(const std::string& action_type, ActionLayerCreator creator)
    {
        if (action_type.empty() || creator == nullptr)
        {
            WARM_LOGE("动作层注册失败：动作类型或创建函数无效。");
            return -1;
        }

        if (!Creators.emplace(action_type, creator).second)
        {
            WARM_LOGE("动作层注册失败：动作类型 %s 已经注册。", action_type.c_str());
            return -1;
        }

        return 0;
    }

    std::unique_ptr<ActionLayer> ActionLayerRegistry::Create(const std::string& action_type) const
    {
        const auto iterator = Creators.find(action_type);
        if (iterator == Creators.end())
        {
            return nullptr;
        }

        return iterator->second();
    }

    bool ActionLayerRegistry::Contains(const std::string& action_type) const
    {
        return Creators.contains(action_type);
    }

    ActionDispatcher::ActionDispatcher()
    {
        for (const BuiltinActionLayerEntry& entry : BuiltinActionLayers)
        {
            if (Registry.Register(entry.ActionType, entry.Creator) != 0)
            {
                WARM_LOGE("动作分发器初始化失败：无法注册动作类型 %s。", entry.ActionType);
            }
        }
    }

    ActionDispatcher::~ActionDispatcher()
    {
        Reset();
    }

    int ActionDispatcher::RegisterLayer(const std::string& action_type, ActionLayerCreator creator)
    {
        if (Layers.contains(action_type))
        {
            WARM_LOGE("动作层注册失败：动作类型 %s 已经创建实例。", action_type.c_str());
            return -1;
        }

        return Registry.Register(action_type, creator);
    }

    int ActionDispatcher::Validate(const std::string& action_type, const std::string& payload, int duration_ms)
    {
        ActionLayer* layer = ResolveLayer(action_type);
        return layer == nullptr ? -1 : layer->Validate(payload, duration_ms);
    }

    int ActionDispatcher::Execute(const std::string& action_type, const std::string& payload, int duration_ms)
    {
        ActionLayer* layer = ResolveLayer(action_type);
        return layer == nullptr ? -1 : layer->Execute(payload, duration_ms);
    }

    void ActionDispatcher::Reset() noexcept
    {
        for (const auto& val : Layers | std::views::values)
        {
            val->Reset();
        }
    }

    ActionLayer* ActionDispatcher::ResolveLayer(const std::string& action_type)
    {
        const auto iterator = Layers.find(action_type);
        if (iterator != Layers.end())
        {
            return iterator->second.get();
        }

        std::unique_ptr<ActionLayer> layer = Registry.Create(action_type);
        if (layer == nullptr)
        {
            WARM_LOGE("动作分发失败：动作类型 %s 尚未注册。", action_type.c_str());
            return nullptr;
        }

        ActionLayer* result = layer.get();
        Layers.emplace(action_type, std::move(layer));
        return result;
    }

    bool ParseKeyCodePayload(const std::string& payload, std::vector<int>& keycodes)
    {
        keycodes.clear();
        std::size_t begin = 0;
        while (begin <= payload.size())
        {
            const std::size_t end = payload.find('+', begin);
            const std::string token = payload.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
            if (token.size() != 1)
            {
                return false;
            }

            const unsigned char raw_key = static_cast<unsigned char>(token.front());
            const int virtual_key = std::toupper(raw_key);
            if (!std::isalnum(static_cast<unsigned char>(virtual_key)) || std::find(keycodes.begin(), keycodes.end(), virtual_key) != keycodes.end())
            {
                return false;
            }

            keycodes.push_back(virtual_key);
            if (end == std::string::npos)
            {
                break;
            }
            begin = end + 1;
        }

        return !keycodes.empty();
    }

    int MouseClick(int x, int y)
    {
        const HWND window = GetForegroundWindow();
        if (window == nullptr)
        {
            WARM_LOGE("鼠标点击失败：无法获取前台窗口。");
            return -1;
        }

        RECT client_rect = {};
        if (GetClientRect(window, &client_rect) == FALSE)
        {
            WARM_LOGE("鼠标点击失败：无法获取游戏客户区，错误码：%lu。", GetLastError());
            return -1;
        }

        POINT click_position = {x, y};
        if (PtInRect(&client_rect, click_position) == FALSE)
        {
            WARM_LOGE("鼠标点击失败：坐标 (%d, %d) 超出游戏客户区。", x, y);
            return -1;
        }

        if (ClientToScreen(window, &click_position) == FALSE)
        {
            WARM_LOGE("鼠标点击失败：客户区坐标转换失败，错误码：%lu。", GetLastError());
            return -1;
        }

        if (SetCursorPos(click_position.x, click_position.y) == FALSE)
        {
            WARM_LOGE("鼠标点击失败：鼠标移动失败，错误码：%lu。", GetLastError());
            return -1;
        }

        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

        const UINT inserted_count = SendInput(2, inputs, sizeof(INPUT));
        if (inserted_count != 2)
        {
            const DWORD error_code = GetLastError();
            INPUT release_input = inputs[1];
            SendInput(1, &release_input, sizeof(INPUT));
            WARM_LOGE("鼠标点击失败：仅发送 %u/2 个输入事件，错误码：%lu。", inserted_count, error_code);
            return -1;
        }

        return 0;
    }

    int KeyCodeTap(int Keycode, int ms)
    {
        if (Keycode <= 0 || Keycode > 0xFE)
        {
            WARM_LOGE("键盘长按失败：无效的虚拟键码 %d。", Keycode);
            return -1;
        }
        if (ms < 0)
        {
            WARM_LOGE("键盘长按失败：持续时间不能小于 0 毫秒。");
            return -1;
        }

        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(Keycode);

        if (SendInput(1, &input, sizeof(INPUT)) != 1)
        {
            WARM_LOGE("键盘长按失败：无法发送按下事件，错误码：%lu。", GetLastError());
            return -1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(ms));

        input.ki.dwFlags = KEYEVENTF_KEYUP;
        if (SendInput(1, &input, sizeof(INPUT)) != 1)
        {
            const DWORD error_code = GetLastError();
            SendInput(1, &input, sizeof(INPUT));
            WARM_LOGE("键盘长按失败：无法发送释放事件，错误码：%lu。", error_code);
            return -1;
        }

        return 0;
    }

    int KeyCodeTap(const std::vector<int>& Keycodes, int ms)
    {
        if (Keycodes.empty())
        {
            WARM_LOGE("组合键长按失败：键码列表为空。");
            return -1;
        }
        if (ms < 0)
        {
            WARM_LOGE("组合键长按失败：持续时间不能小于 0 毫秒。");
            return -1;
        }

        for (std::size_t index = 0; index < Keycodes.size(); ++index)
        {
            if (Keycodes[index] <= 0 || Keycodes[index] > 0xFE)
            {
                WARM_LOGE("组合键长按失败：无效的虚拟键码 %d。", Keycodes[index]);
                return -1;
            }
            for (std::size_t previous_index = 0; previous_index < index; ++previous_index)
            {
                if (Keycodes[previous_index] == Keycodes[index])
                {
                    WARM_LOGE("组合键长按失败：虚拟键码 %d 重复。", Keycodes[index]);
                    return -1;
                }
            }
        }

        std::vector<INPUT> press_inputs(Keycodes.size());
        std::vector<INPUT> release_inputs(Keycodes.size());
        for (std::size_t index = 0; index < Keycodes.size(); ++index)
        {
            press_inputs[index].type = INPUT_KEYBOARD;
            press_inputs[index].ki.wVk = static_cast<WORD>(Keycodes[index]);

            const std::size_t release_index = Keycodes.size() - index - 1;
            release_inputs[index].type = INPUT_KEYBOARD;
            release_inputs[index].ki.wVk = static_cast<WORD>(Keycodes[release_index]);
            release_inputs[index].ki.dwFlags = KEYEVENTF_KEYUP;
        }

        const UINT input_count = static_cast<UINT>(press_inputs.size());
        const UINT pressed_count = SendInput(input_count, press_inputs.data(), sizeof(INPUT));
        if (pressed_count != input_count)
        {
            const DWORD error_code = GetLastError();
            SendInput(input_count, release_inputs.data(), sizeof(INPUT));
            WARM_LOGE("组合键长按失败：仅发送 %u/%u 个按下事件，错误码：%lu。", pressed_count, input_count, error_code);
            return -1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(ms));

        const UINT released_count = SendInput(input_count, release_inputs.data(), sizeof(INPUT));
        if (released_count != input_count)
        {
            const DWORD error_code = GetLastError();
            SendInput(input_count, release_inputs.data(), sizeof(INPUT));
            WARM_LOGE("组合键长按失败：仅发送 %u/%u 个释放事件，错误码：%lu。", released_count, input_count, error_code);
            return -1;
        }

        return 0;
    }

    int KeycodeInput(int Keycode)
    {
        if (Keycode <= 0 || Keycode > 0xFE)
        {
            WARM_LOGE("键盘输入失败：无效的虚拟键码 %d。", Keycode);
            return -1;
        }

        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = static_cast<WORD>(Keycode);
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = static_cast<WORD>(Keycode);
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        const UINT inserted_count = SendInput(2, inputs, sizeof(INPUT));
        if (inserted_count != 2)
        {
            const DWORD error_code = GetLastError();
            INPUT release_input = inputs[1];
            SendInput(1, &release_input, sizeof(INPUT));
            WARM_LOGE("键盘输入失败：仅发送 %u/2 个输入事件，错误码：%lu。", inserted_count, error_code);
            return -1;
        }

        return 0;
    }
}

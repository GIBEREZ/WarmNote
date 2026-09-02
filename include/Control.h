//
// Created by 30388 on 2026/8/30.
//

#ifndef WARMNOTE_INPUT_H
#define WARMNOTE_INPUT_H
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Control
{
    // 所有谱面动作执行层的公共基类。
    // ActionLayer只约定动作校验、执行和状态清理接口，不依赖Score或具体业务模块。
    class ActionLayer
    {
    public:
        ActionLayer();
        virtual ~ActionLayer();

        ActionLayer(const ActionLayer&) = delete;
        ActionLayer& operator=(const ActionLayer&) = delete;
        ActionLayer(ActionLayer&&) = delete;
        ActionLayer& operator=(ActionLayer&&) = delete;

        // 在正式演奏前检查动作载荷和持续时间是否合法。
        [[nodiscard]] virtual int Validate(const std::string& payload, int duration_ms) const = 0;
        // 执行一次已经通过Validate校验的动作。
        [[nodiscard]] virtual int Execute(const std::string& payload, int duration_ms) = 0;
        // 释放执行层持有的按键或其他状态；无状态执行层可以沿用默认空实现。
        virtual void Reset() noexcept;
    };

    // 动作层创建函数。每次调用都返回一个独立的动作层实例。
    using ActionLayerCreator = std::unique_ptr<ActionLayer> (*)();

    // 根据动作层类名生成统一格式的创建函数，由CMake生成的声明文件调用。
#define DEFINE_ACTION_LAYER_CREATOR(name)                                      \
    static std::unique_ptr<Control::ActionLayer> name##_action_layer_creator() \
    {                                                                          \
        return std::make_unique<name>();                                        \
    }

    // 保存“动作类型名称 -> 动作层创建函数”的映射，不负责执行动作。
    class ActionLayerRegistry
    {
    public:
        // 注册一种动作类型；类型名为空、创建函数为空或重复注册时返回-1。
        [[nodiscard]] int Register(const std::string& action_type, ActionLayerCreator creator);
        // 根据类型名创建动作层；未注册时返回空指针。
        [[nodiscard]] std::unique_ptr<ActionLayer> Create(const std::string& action_type) const;
        [[nodiscard]] bool Contains(const std::string& action_type) const;
    private:
        std::unordered_map<std::string, ActionLayerCreator> Creators;
    };

    // 根据动作类型选择对应动作层，并缓存已经创建的实例。
    class ActionDispatcher
    {
    public:
        ActionDispatcher();
        ~ActionDispatcher();

        ActionDispatcher(const ActionDispatcher&) = delete;
        ActionDispatcher& operator=(const ActionDispatcher&) = delete;
        ActionDispatcher(ActionDispatcher&&) = delete;
        ActionDispatcher& operator=(ActionDispatcher&&) = delete;

        // 注册后续新增的动作层，MusicExtractor不需要为新类型增加分支。
        [[nodiscard]] int RegisterLayer(const std::string& action_type, ActionLayerCreator creator);
        [[nodiscard]] int Validate(const std::string& action_type, const std::string& payload, int duration_ms);
        [[nodiscard]] int Execute(const std::string& action_type, const std::string& payload, int duration_ms);
        // 在演奏开始、结束或失败时释放所有动作层保存的状态。
        void Reset() noexcept;
    private:
        [[nodiscard]] ActionLayer* ResolveLayer(const std::string& action_type);

        ActionLayerRegistry Registry;
        std::unordered_map<std::string, std::unique_ptr<ActionLayer>> Layers;
    };

    // 将“A+D+E”形式的动作载荷解析为Windows虚拟键码。
    [[nodiscard]] bool ParseKeyCodePayload(const std::string& payload, std::vector<int>& keycodes);

    int MouseClick(int x, int y);
    int KeyCodeTap(int Keycode, int ms);
    int KeyCodeTap(const std::vector<int>& Keycodes, int ms);
    int KeycodeInput(int Keycode);
}

#endif //WARMNOTE_INPUT_H

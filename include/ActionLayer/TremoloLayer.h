//
// Created by 30388 on 2026/9/2.
//

#ifndef WARMNOTE_TREMOLOLAYER_H
#define WARMNOTE_TREMOLOLAYER_H

#include "../Control.h"

// 执行横笛的Shift颤音操作。
// 谱面中的动作类型为Tremolo，载荷为Q-U或A-J中的一个目标音符。
class TremoloLayer final : public Control::ActionLayer
{
public:
    TremoloLayer();
    ~TremoloLayer() override;

    [[nodiscard]] int Validate(const std::string& payload, int duration_ms) const override;
    [[nodiscard]] int Execute(const std::string& payload, int duration_ms) override;
};

#endif //WARMNOTE_TREMOLOLAYER_H

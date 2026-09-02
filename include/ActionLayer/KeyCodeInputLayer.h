//
// Created by 30388 on 2026/9/2.
//

#ifndef WARMNOTE_KEYCODEINPUTLAYER_H
#define WARMNOTE_KEYCODEINPUTLAYER_H

#include "../Control.h"

class KeyCodeInputLayer final : public Control::ActionLayer
{
public:
    KeyCodeInputLayer();
    ~KeyCodeInputLayer() override;

    [[nodiscard]] int Validate(const std::string& payload, int duration_ms) const override;
    [[nodiscard]] int Execute(const std::string& payload, int duration_ms) override;
};


#endif //WARMNOTE_KEYCODEINPUTLAYER_H

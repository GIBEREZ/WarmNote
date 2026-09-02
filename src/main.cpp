#include "Core.h"
#include "Score.h"

#include <cstdio>
#include <filesystem>

#include <Windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    // 加载项目score目录中的《Take Me Hand》音谱。
    const std::filesystem::path score_path = std::filesystem::path(__FILE__).parent_path().parent_path() / L"score" / L"青花瓷.param";
    Score score;
    if (score.SetTicksPerBeat(480) != 0 || score.Load(score_path) != 0)
    {
        WARM_LOGE("自动弹琴启动失败：《Take Me Hand》音谱加载失败。");
        return 1;
    }

    // 仿照ncnn Extractor的使用方式，将part1_001设为运行起点。
    Core::MusicExtractor extractor;
    if (extractor.input("part2_001", score) != 0)
    {
        WARM_LOGE("自动弹琴启动失败：无法设置音谱运行起点。");
        return 2;
    }

    // 前台检测成功后，才允许MusicExtractor向游戏发送演奏按键。
    if (Core::NIKKI_FrontDeskCheck() != 0)
    {
        WARM_LOGE("自动弹琴启动失败：无法将无限暖暖切换到前台。");
        return 3;
    }

    std::printf("开始演奏：《%s》\n", score.GetName().c_str());
    if (extractor.extract() != 0)
    {
        WARM_LOGE("自动弹琴执行失败：《Take Me Hand》未能完整演奏。");
        return 4;
    }

    std::printf("演奏完成：《%s》\n", score.GetName().c_str());
    return 0;
}

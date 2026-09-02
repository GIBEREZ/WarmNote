#ifndef WARMNOTE_CORE_H
#define WARMNOTE_CORE_H

#include <filesystem>
#include <string>
#include <thread>

#include <Windows.h>

#include "Score.h"
#include "Control.h"

#define NIKKI_WINDOW "X6Game-Win64-Shipping.exe"

#ifndef WARMNOTE_ENABLE_LOGGING
#define WARMNOTE_ENABLE_LOGGING 1
#endif

#if WARMNOTE_ENABLE_LOGGING
#define WARM_LOGE(...)                         \
    do                                         \
    {                                          \
        std::fprintf(stderr, __VA_ARGS__);      \
        std::fprintf(stderr, "\n");            \
    } while (0)
#else
#define WARM_LOGE(...) ((void)0)
#endif

struct DisplayContext
{
    int Monitor_Width;
    int Monitor_Height;
    int NIKKI_Width;
    int NIKKI_Height;
    double scale;
    unsigned int dpi;
};

namespace Core
{
    // 判断指定窗口是否属于给定进程名，可传入NIKKI_WINDOW或其他进程名宏。
    bool IsProcessWindow(HWND window, const char* process_name);
    // 根据给定进程名查找对应的可见顶层窗口。
    HWND FindProcessWindow(const char* process_name);
    // 将任意有效顶层窗口切换到前台。
    bool SwitchToWindow(HWND window);

    int NIKKI_FrontDeskCheck();

    class MusicExtractor
    {
    public:
        MusicExtractor();
        ~MusicExtractor();

        // 绑定琴谱，并将指定记录名作为运行起点。
        int input(const std::string& record_name, const Score& score);
        // 从input指定的记录开始运行；传入结束记录名时，执行完该记录后停止。
        int extract(const std::string& end_record_name = {});
    private:
        Score InputScore;                       // 保存独立的谱面快照，避免外部卸载影响当前运行。
        std::string InputRecordName;            // 当前作为起点的记录名。
        std::size_t StartBeatIndex;             // 起始记录第一拍在Beats中的下标。
        bool InputReady;                        // input是否已经成功绑定。
        Control::ActionDispatcher Dispatcher;   // 根据动作类型选择并复用对应的动作层。
    };
}

#endif // WARMNOTE_CORE_H

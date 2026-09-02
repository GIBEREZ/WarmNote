#include "Core.h"
#include "Control.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace Core
{
    bool IsProcessWindow(HWND window, const char* process_name)
    {
        if (window == nullptr || process_name == nullptr || process_name[0] == '\0')
        {
            return false;
        }

        DWORD process_id = 0;
        GetWindowThreadProcessId(window, &process_id);
        if (process_id == 0)
        {
            return false;
        }

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
        if (process == nullptr)
        {
            return false;
        }

        char process_path[32768] = {};
        DWORD process_path_size = sizeof(process_path);
        const BOOL query_result = QueryFullProcessImageNameA(process, 0, process_path, &process_path_size);
        CloseHandle(process);
        if (query_result == FALSE)
        {
            return false;
        }

        const char* file_name = std::strrchr(process_path, '\\');
        file_name = file_name == nullptr ? process_path : file_name + 1;
        return _stricmp(file_name, process_name) == 0;
    }

    HWND FindProcessWindow(const char* process_name)
    {
        if (process_name == nullptr || process_name[0] == '\0')
        {
            return nullptr;
        }

        struct WindowSearchContext
        {
            const char* ProcessName;
            HWND Window;
        };

        WindowSearchContext context = {process_name, nullptr};
        EnumWindows([](HWND window, LPARAM parameter) -> BOOL
        {
            auto* search_context = reinterpret_cast<WindowSearchContext*>(parameter);
            if (IsWindowVisible(window) != FALSE && IsProcessWindow(window, search_context->ProcessName))
            {
                search_context->Window = window;
                return FALSE;
            }

            return TRUE;
        }, reinterpret_cast<LPARAM>(&context));
        return context.Window;
    }

    bool SwitchToWindow(HWND window)
    {
        if (window == nullptr)
        {
            return false;
        }

        if (IsIconic(window) != FALSE)
        {
            ShowWindow(window, SW_RESTORE);
        }

        const HWND foreground_window = GetForegroundWindow();
        const DWORD current_thread_id = GetCurrentThreadId();
        const DWORD foreground_thread_id = foreground_window == nullptr ? 0 : GetWindowThreadProcessId(foreground_window, nullptr);
        const DWORD nikki_thread_id = GetWindowThreadProcessId(window, nullptr);
        const bool attached_foreground = foreground_thread_id != 0 && foreground_thread_id != current_thread_id && AttachThreadInput(current_thread_id, foreground_thread_id, TRUE) != FALSE;
        const bool attached_nikki = nikki_thread_id != 0 && nikki_thread_id != current_thread_id && nikki_thread_id != foreground_thread_id && AttachThreadInput(current_thread_id, nikki_thread_id, TRUE) != FALSE;

        BringWindowToTop(window);
        const bool switch_result = SetForegroundWindow(window) != FALSE;
        SetFocus(window);

        if (attached_nikki)
        {
            AttachThreadInput(current_thread_id, nikki_thread_id, FALSE);
        }
        if (attached_foreground)
        {
            AttachThreadInput(current_thread_id, foreground_thread_id, FALSE);
        }

        return switch_result;
    }

    int NIKKI_FrontDeskCheck()
    {
        constexpr int max_check_count = 3;
        constexpr auto retry_interval = std::chrono::milliseconds(300);

        for (int check_count = 1; check_count <= max_check_count; ++check_count)
        {
            if (IsProcessWindow(GetForegroundWindow(), NIKKI_WINDOW))
            {
                return 0;
            }

            if (check_count == max_check_count)
            {
                break;
            }

            HWND nikki_window = FindProcessWindow(NIKKI_WINDOW);
            if (nikki_window == nullptr)
            {
                WARM_LOGE("无限暖暖前台检测失败：未找到游戏窗口，第 %d/%d 次。", check_count, max_check_count);
            }
            else if (!SwitchToWindow(nikki_window))
            {
                WARM_LOGE("无限暖暖前台切换失败，第 %d/%d 次。", check_count, max_check_count);
            }

            std::this_thread::sleep_for(retry_interval);
        }

        WARM_LOGE("无限暖暖前台检测失败：共检测 %d 次。", max_check_count);
        return -1;
    }

    MusicExtractor::MusicExtractor() : StartBeatIndex(0), InputReady(false)
    {
    }

    MusicExtractor::~MusicExtractor() = default;

    int MusicExtractor::input(const std::string& record_name, const Score& score)
    {
        Dispatcher.Reset();
        InputReady = false;
        InputRecordName.clear();
        StartBeatIndex = 0;

        // MusicExtractor保存谱面快照，之后的extract不依赖外部Score对象的生命周期。
        InputScore = score;
        if (!InputScore.GetLoadStatus())
        {
            WARM_LOGE("音乐提取器输入失败：琴谱尚未成功加载。");
            return -1;
        }
        if (record_name.empty())
        {
            WARM_LOGE("音乐提取器输入失败：记录名为空。");
            return -1;
        }
        if (InputScore.GetSmallBeat() <= 0 || InputScore.GetTicksPerBeat() <= 0 || InputScore.GetBpm() <= 0)
        {
            WARM_LOGE("音乐提取器输入失败：琴谱时间参数无效。");
            return -1;
        }

        const std::vector<Score::Beat>& beats = InputScore.GetBeats();
        const std::vector<std::string>& record_names = InputScore.GetRecordNames();
        const std::size_t beats_per_record = static_cast<std::size_t>(InputScore.GetSmallBeat());
        if (record_names.empty() || beats.size() != record_names.size() * beats_per_record)
        {
            WARM_LOGE("音乐提取器输入失败：琴谱记录索引无效。");
            return -1;
        }

        const auto record_position = std::find(record_names.begin(), record_names.end(), record_name);
        if (record_position != record_names.end())
        {
            const std::size_t record_index = static_cast<std::size_t>(std::distance(record_names.begin(), record_position));
            StartBeatIndex = record_index * beats_per_record;
            InputRecordName = record_name;
            InputReady = true;
            return 0;
        }

        WARM_LOGE("音乐提取器输入失败：找不到记录 %s。", record_name.c_str());
        return -1;
    }

    int MusicExtractor::extract(const std::string& end_record_name)
    {
        if (!InputReady)
        {
            WARM_LOGE("音乐提取器运行失败：请先成功调用input。");
            return -1;
        }

        const std::vector<Score::Beat>& beats = InputScore.GetBeats();
        const std::vector<std::string>& record_names = InputScore.GetRecordNames();
        const int ticks_per_beat = InputScore.GetTicksPerBeat();
        const int bpm = InputScore.GetBpm();
        const int action_duration_ms = InputScore.GetBeatDurationMs();
        const std::size_t beats_per_record = static_cast<std::size_t>(InputScore.GetSmallBeat());
        if (StartBeatIndex >= beats.size() || beats_per_record == 0 || record_names.empty() || beats.size() != record_names.size() * beats_per_record || ticks_per_beat <= 0 || bpm <= 0 || action_duration_ms < 0)
        {
            WARM_LOGE("音乐提取器运行失败：运行状态或时间参数无效。");
            return -1;
        }

        std::size_t end_beat_index = beats.size();
        if (!end_record_name.empty())
        {
            const auto record_position = std::find(record_names.begin(), record_names.end(), end_record_name);
            if (record_position == record_names.end())
            {
                WARM_LOGE("音乐提取器运行失败：找不到结束记录 %s。", end_record_name.c_str());
                return -1;
            }

            const std::size_t record_index = static_cast<std::size_t>(std::distance(record_names.begin(), record_position));
            end_beat_index = (record_index + 1) * beats_per_record;
            if (end_beat_index <= StartBeatIndex || end_beat_index > beats.size())
            {
                WARM_LOGE("音乐提取器运行失败：结束记录 %s 位于起始记录 %s 之前。", end_record_name.c_str(), InputRecordName.c_str());
                return -1;
            }
        }

        // 播放前只预检查本次起止范围内的动作，Core不解释具体载荷格式。
        Dispatcher.Reset();
        for (std::size_t beat_index = StartBeatIndex; beat_index < end_beat_index; ++beat_index)
        {
            for (const Score::BeatAction& action : beats[beat_index])
            {
                if (action.OffsetTick < 0 || action.OffsetTick >= ticks_per_beat || Dispatcher.Validate(action.ActionType, action.Payload, action_duration_ms) != 0)
                {
                    WARM_LOGE("音乐提取器运行失败：记录 %s 的动作参数无效。", action.RecordName.c_str());
                    Dispatcher.Reset();
                    return -1;
                }
            }
        }

        // 所有动作都以同一个起始时间计算绝对截止点，避免逐拍sleep导致误差累积。
        const long double nanoseconds_per_tick = 60000000000.0L / (static_cast<long double>(bpm) * ticks_per_beat);
        const auto playback_begin = std::chrono::steady_clock::now();
        for (std::size_t beat_index = StartBeatIndex; beat_index < end_beat_index; ++beat_index)
        {
            const Score::Beat& beat = beats[beat_index];
            for (const Score::BeatAction& action : beat)
            {
                const std::size_t relative_beat_index = beat_index - StartBeatIndex;
                const std::int64_t relative_tick = static_cast<std::int64_t>(relative_beat_index) * ticks_per_beat + action.OffsetTick;
                const auto target_offset = std::chrono::nanoseconds(std::llround(relative_tick * nanoseconds_per_tick));
                std::this_thread::sleep_until(playback_begin + target_offset);

                if (Dispatcher.Execute(action.ActionType, action.Payload, action_duration_ms) != 0)
                {
                    WARM_LOGE("音乐提取器运行失败：记录 %s 执行失败。", action.RecordName.c_str());
                    Dispatcher.Reset();
                    return -1;
                }
            }
        }

        // 最后一个动作后仍保留结束记录剩余的休止拍。
        const std::int64_t total_tick = static_cast<std::int64_t>(end_beat_index - StartBeatIndex) * ticks_per_beat;
        const auto playback_end_offset = std::chrono::nanoseconds(std::llround(total_tick * nanoseconds_per_tick));
        std::this_thread::sleep_until(playback_begin + playback_end_offset);
        Dispatcher.Reset();
        return 0;
    }
}
